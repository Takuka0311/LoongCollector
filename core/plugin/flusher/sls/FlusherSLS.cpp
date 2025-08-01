// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/FlusherSLS.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/batch/FlushStrategy.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "collection_pipeline/queue/SLSSenderQueueItem.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/EndpointUtil.h"
#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "common/TimeUtil.h"
#include "common/compression/CompressorFactory.h"
#include "common/http/Constant.h"
#include "common/http/HttpRequest.h"
#include "plugin/flusher/sls/DiskBufferWriter.h"
#include "plugin/flusher/sls/PackIdManager.h"
#include "plugin/flusher/sls/SLSClientManager.h"
#include "plugin/flusher/sls/SLSConstant.h"
#include "plugin/flusher/sls/SLSResponse.h"
#include "plugin/flusher/sls/SLSUtil.h"
#include "plugin/flusher/sls/SendResult.h"
#include "provider/Provider.h"
#include "runner/FlusherRunner.h"
#include "sls_logs.pb.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

using namespace std;

DEFINE_FLAG_INT32(batch_send_interval, "batch sender interval (second)(default 3)", 3);
DEFINE_FLAG_INT32(merge_log_count_limit, "log count in one logGroup at most", 4000);
DEFINE_FLAG_INT32(batch_send_metric_size, "batch send metric size limit(bytes)(default 512KB)", 512 * 1024);
DEFINE_FLAG_INT32(send_check_real_ip_interval, "seconds", 2);

DEFINE_FLAG_INT32(unauthorized_send_retrytimes,
                  "how many times should retry if PostLogStoreLogs operation return UnAuthorized",
                  5);
DEFINE_FLAG_INT32(unauthorized_allowed_delay_after_reset, "allowed delay to retry for unauthorized error, 30s", 30);
DEFINE_FLAG_INT32(discard_send_fail_interval, "discard data when send fail after 6 * 3600 seconds", 6 * 3600);
DEFINE_FLAG_INT32(profile_data_send_retrytimes, "how many times should retry if profile data send fail", 5);
DEFINE_FLAG_INT32(unknow_error_try_max, "discard data when try times > this value", 5);
DEFINE_FLAG_BOOL(enable_metricstore_channel, "only works for metrics data for enhance metrics query performance", true);
DEFINE_FLAG_INT32(max_send_log_group_size, "bytes", 10 * 1024 * 1024);
DEFINE_FLAG_DOUBLE(sls_serialize_size_expansion_ratio, "", 1.2);
DEFINE_FLAG_INT32(sls_request_dscp, "set dscp for sls request, from 0 to 63", -1);

DECLARE_FLAG_BOOL(send_prefer_real_ip);

namespace logtail {

enum class OperationOnFail { RETRY_IMMEDIATELY, RETRY_LATER, DISCARD };

static const int ON_FAIL_LOG_WARNING_INTERVAL_SECOND = 10;

static constexpr int64_t kInvalidHashKeySeqID = 0;

static const char* GetOperationString(OperationOnFail op) {
    switch (op) {
        case OperationOnFail::RETRY_IMMEDIATELY:
            return "retry now";
        case OperationOnFail::RETRY_LATER:
            return "retry later";
        case OperationOnFail::DISCARD:
        default:
            return "discard data";
    }
}

static OperationOnFail DefaultOperation(uint32_t retryTimes) {
    if (retryTimes > static_cast<uint32_t>(INT32_FLAG(unknow_error_try_max))) {
        return OperationOnFail::DISCARD;
    } else {
        return OperationOnFail::RETRY_LATER;
    }
}

void FlusherSLS::InitResource() {
#ifndef APSARA_UNIT_TEST_MAIN
    if (!sIsResourceInited) {
        SLSClientManager::GetInstance()->Init();
        DiskBufferWriter::GetInstance()->Init();
        sIsResourceInited = true;
    }
#endif
}

void FlusherSLS::RecycleResourceIfNotUsed() {
#ifndef APSARA_UNIT_TEST_MAIN
    if (sIsResourceInited) {
        SLSClientManager::GetInstance()->Stop();
        DiskBufferWriter::GetInstance()->Stop();
    }
#endif
}

mutex FlusherSLS::sMux;
unordered_map<string, weak_ptr<ConcurrencyLimiter>> FlusherSLS::sProjectConcurrencyLimiterMap;
unordered_map<string, weak_ptr<ConcurrencyLimiter>> FlusherSLS::sRegionConcurrencyLimiterMap;
unordered_map<string, weak_ptr<ConcurrencyLimiter>> FlusherSLS::sLogstoreConcurrencyLimiterMap;

shared_ptr<ConcurrencyLimiter> GetConcurrencyLimiter(const std::string& description) {
    return make_shared<ConcurrencyLimiter>(description, AppConfig::GetInstance()->GetSendRequestConcurrency());
}

shared_ptr<ConcurrencyLimiter> FlusherSLS::GetLogstoreConcurrencyLimiter(const std::string& project,
                                                                         const std::string& logstore) {
    lock_guard<mutex> lock(sMux);
    std::string key = project + "-" + logstore;

    auto iter = sLogstoreConcurrencyLimiterMap.find(key);
    if (iter == sLogstoreConcurrencyLimiterMap.end()) {
        auto limiter = make_shared<ConcurrencyLimiter>(sName + "#quota#logstore#" + key,
                                                       AppConfig::GetInstance()->GetSendRequestConcurrency());
        sLogstoreConcurrencyLimiterMap.try_emplace(key, limiter);
        return limiter;
    }
    auto limiter = iter->second.lock();
    if (!limiter) {
        limiter = make_shared<ConcurrencyLimiter>(sName + "#quota#logstore#" + key,
                                                  AppConfig::GetInstance()->GetSendRequestConcurrency());
        iter->second = limiter;
    }
    return limiter;
}

shared_ptr<ConcurrencyLimiter> FlusherSLS::GetProjectConcurrencyLimiter(const string& project) {
    lock_guard<mutex> lock(sMux);
    auto iter = sProjectConcurrencyLimiterMap.find(project);
    if (iter == sProjectConcurrencyLimiterMap.end()) {
        auto limiter = make_shared<ConcurrencyLimiter>(sName + "#quota#project#" + project,
                                                       AppConfig::GetInstance()->GetSendRequestConcurrency());
        sProjectConcurrencyLimiterMap.try_emplace(project, limiter);
        return limiter;
    }
    auto limiter = iter->second.lock();
    if (!limiter) {
        limiter = make_shared<ConcurrencyLimiter>(sName + "#quota#project#" + project,
                                                  AppConfig::GetInstance()->GetSendRequestConcurrency());
        iter->second = limiter;
    }
    return limiter;
}

shared_ptr<ConcurrencyLimiter> FlusherSLS::GetRegionConcurrencyLimiter(const string& region) {
    lock_guard<mutex> lock(sMux);
    auto iter = sRegionConcurrencyLimiterMap.find(region);
    if (iter == sRegionConcurrencyLimiterMap.end()) {
        auto limiter = make_shared<ConcurrencyLimiter>(
            sName + "#network#region#" + region,
            AppConfig::GetInstance()->GetSendRequestConcurrency(),
            AppConfig::GetInstance()->GetSendRequestConcurrency()
                * AppConfig::GetInstance()->GetGlobalConcurrencyFreePercentageForOneRegion());
        sRegionConcurrencyLimiterMap.try_emplace(region, limiter);
        return limiter;
    }
    auto limiter = iter->second.lock();
    if (!limiter) {
        limiter = make_shared<ConcurrencyLimiter>(
            sName + "#network#region#" + region,
            AppConfig::GetInstance()->GetSendRequestConcurrency(),
            AppConfig::GetInstance()->GetSendRequestConcurrency()
                * AppConfig::GetInstance()->GetGlobalConcurrencyFreePercentageForOneRegion());
        iter->second = limiter;
    }
    return limiter;
}

void FlusherSLS::ClearInvalidConcurrencyLimiters() {
    lock_guard<mutex> lock(sMux);
    for (auto iter = sProjectConcurrencyLimiterMap.begin(); iter != sProjectConcurrencyLimiterMap.end();) {
        if (iter->second.expired()) {
            iter = sProjectConcurrencyLimiterMap.erase(iter);
        } else {
            ++iter;
        }
    }
    for (auto iter = sRegionConcurrencyLimiterMap.begin(); iter != sRegionConcurrencyLimiterMap.end();) {
        if (iter->second.expired()) {
            iter = sRegionConcurrencyLimiterMap.erase(iter);
        } else {
            ++iter;
        }
    }
    for (auto iter = sLogstoreConcurrencyLimiterMap.begin(); iter != sLogstoreConcurrencyLimiterMap.end();) {
        if (iter->second.expired()) {
            iter = sLogstoreConcurrencyLimiterMap.erase(iter);
        } else {
            ++iter;
        }
    }
}

mutex FlusherSLS::sDefaultRegionLock;
string FlusherSLS::sDefaultRegion;

string FlusherSLS::GetDefaultRegion() {
    lock_guard<mutex> lock(sDefaultRegionLock);
    if (sDefaultRegion.empty()) {
        sDefaultRegion = STRING_FLAG(default_region_name);
    }
    return sDefaultRegion;
}

void FlusherSLS::SetDefaultRegion(const string& region) {
    lock_guard<mutex> lock(sDefaultRegionLock);
    sDefaultRegion = region;
}

mutex FlusherSLS::sProjectRegionMapLock;
unordered_map<string, int32_t> FlusherSLS::sProjectRefCntMap;
unordered_map<string, string> FlusherSLS::sProjectRegionMap;

string FlusherSLS::GetAllProjects() {
    string result;
    lock_guard<mutex> lock(sProjectRegionMapLock);
    for (auto iter = sProjectRefCntMap.cbegin(); iter != sProjectRefCntMap.cend(); ++iter) {
        result.append(iter->first).append(" ");
    }
    return result;
}

std::string FlusherSLS::GetProjectRegion(const std::string& project) {
    lock_guard<mutex> lock(sProjectRegionMapLock);
    auto iter = sProjectRegionMap.find(project);
    if (iter == sProjectRegionMap.end()) {
        return "";
    }
    return iter->second;
}

void FlusherSLS::IncreaseProjectRegionReferenceCnt(const string& project, const string& region) {
    lock_guard<mutex> lock(sProjectRegionMapLock);
    ++sProjectRefCntMap[project];
    sProjectRegionMap[project] = region;
}

void FlusherSLS::DecreaseProjectRegionReferenceCnt(const string& project, const string& region) {
    lock_guard<mutex> lock(sProjectRegionMapLock);
    auto projectRefCnt = sProjectRefCntMap.find(project);
    if (projectRefCnt != sProjectRefCntMap.end()) {
        if (--projectRefCnt->second == 0) {
            sProjectRefCntMap.erase(projectRefCnt);
            sProjectRegionMap.erase(project);
        }
    }
}

bool FlusherSLS::sIsResourceInited = false;

const string FlusherSLS::sName = "flusher_sls";

const unordered_set<string> FlusherSLS::sNativeParam = {"Project",
                                                        "Logstore",
                                                        "Region",
                                                        "Endpoint",
                                                        "EndpointMode",
                                                        "Aliuid",
                                                        "CompressType",
                                                        "TelemetryType",
                                                        "MaxSendRate",
                                                        "ShardHashKeys",
                                                        "Batch"};

FlusherSLS::FlusherSLS() : mRegion(GetDefaultRegion()) {
}

bool FlusherSLS::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

    // Project
    if (!GetMandatoryStringParam(config, "Project", mProject, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // Workspace
    if (!GetOptionalStringParam(config, "Workspace", mWorkspace, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // TelemetryType
    string telemetryType;
    if (!GetOptionalStringParam(config, "TelemetryType", telemetryType, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              "logs",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (telemetryType == "metrics") {
        // TelemetryType set to metrics
        mTelemetryType = BOOL_FLAG(enable_metricstore_channel) ? sls_logs::SLS_TELEMETRY_TYPE_METRICS
                                                               : sls_logs::SLS_TELEMETRY_TYPE_LOGS;
    } else if (telemetryType == "metrics_multivalue") {
        mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_METRICS_MULTIVALUE;
    } else if (telemetryType == "metrics_host") {
        mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_METRICS_HOST;
    } else if (telemetryType == "arms_agentinfo") {
        mSubpath = APM_AGENTINFOS_URL;
        mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_APM_AGENTINFOS;
    } else if (telemetryType == "arms_metrics") {
        mSubpath = APM_METRICS_URL;
        mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_APM_METRICS;
    } else if (telemetryType == "arms_traces") {
        mSubpath = APM_TRACES_URL;
        mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_APM_TRACES;
    } else if (!telemetryType.empty() && telemetryType != "logs") {
        // TelemetryType invalid
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              "string param TelemetryType is not valid",
                              "logs",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // Logstore
    if (IsRawSLSTelemetryType()) {
        // log and metric
        if (!GetMandatoryStringParam(config, "Logstore", mLogstore, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
    }

    // Region
    if (
#ifdef __ENTERPRISE__
        !EnterpriseConfigProvider::GetInstance()->IsDataServerPrivateCloud() &&
#endif
        !GetOptionalStringParam(config, "Region", mRegion, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mRegion,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

#ifdef __ENTERPRISE__
    // Aliuid
    if (!GetOptionalStringParam(config, "Aliuid", mAliuid, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // EndpointMode
    string endpointMode = "default";
    if (!GetOptionalStringParam(config, "EndpointMode", endpointMode, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              endpointMode,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    if (endpointMode == "accelerate") {
        mEndpointMode = EndpointMode::ACCELERATE;
    } else if (endpointMode != "default") {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              "string param EndpointMode is not valid",
                              "default",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    if (mEndpointMode == EndpointMode::DEFAULT) {
        // for local pipeline whose flusher region is neither specified in local info nor included by config provider,
        // param Endpoint should be used, and the mode is set to default.
        // warning: if inconsistency exists among configs, only the first config would be considered in this situation.
        if (!GetOptionalStringParam(config, "Endpoint", mEndpoint, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        EnterpriseSLSClientManager::GetInstance()->UpdateRemoteRegionEndpoints(
            mRegion, {mEndpoint}, EnterpriseSLSClientManager::RemoteEndpointUpdateAction::APPEND);
    }
    mCandidateHostsInfo
        = EnterpriseSLSClientManager::GetInstance()->GetCandidateHostsInfo(mRegion, mProject, mEndpointMode);
    LOG_INFO(mContext->GetLogger(),
             ("get candidate hosts info, region", mRegion)("project", mProject)("logstore", mLogstore)(
                 "endpoint mode", EndpointModeToString(mCandidateHostsInfo->GetMode())));
#else
    // Endpoint
    if (!GetMandatoryStringParam(config, "Endpoint", mEndpoint, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    mEndpoint = TrimString(mEndpoint);
    if (mEndpoint.empty()) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "param Endpoint is empty",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
#endif


    // Batch
    const char* key = "Batch";
    const Json::Value* itr = config.find(key, key + strlen(key));
    if (itr) {
        if (!itr->isObject()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 "param Batch is not of type object",
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }

        // Deprecated, use ShardHashKeys instead
        if (!GetOptionalListParam<string>(*itr, "Batch.ShardHashKeys", mShardHashKeys, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
    }

    // ShardHashKeys
    if (mTelemetryType == sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS && !mContext->IsExactlyOnceEnabled()) {
        if (!GetOptionalListParam<string>(config, "ShardHashKeys", mShardHashKeys, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
    }

    DefaultFlushStrategyOptions strategy{
        static_cast<uint32_t>(INT32_FLAG(max_send_log_group_size) / DOUBLE_FLAG(sls_serialize_size_expansion_ratio)),
        static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
        static_cast<uint32_t>(INT32_FLAG(merge_log_count_limit)),
        static_cast<uint32_t>(INT32_FLAG(batch_send_interval))};
    if (!mBatcher.Init(itr ? *itr : Json::Value(),
                       this,
                       strategy,
                       !mContext->IsExactlyOnceEnabled() && mShardHashKeys.empty() && IsMetricsTelemetryType())) {
        // when either exactly once is enabled or ShardHashKeys is not empty or telemetry type is metrics, we don't
        // enable group batch
        return false;
    }

    // CompressType
    if (BOOL_FLAG(sls_client_send_compress)) {
        mCompressor = CompressorFactory::GetInstance()->Create(config, *mContext, sName, mPluginID, CompressType::LZ4);
    }

    mGroupSerializer = make_unique<SLSEventGroupSerializer>(this);
    mGroupListSerializer = make_unique<SLSEventGroupListSerializer>(this);

    // MaxSendRate
    // For legacy reason, MaxSendRate should be int, where negative number means unlimited. However, this can be
    // compatable with the following logic.
    if (!GetOptionalUIntParam(config, "MaxSendRate", mMaxSendRate, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mMaxSendRate,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    if (!mContext->IsExactlyOnceEnabled()) {
        GenerateQueueKey(mProject + "#" + mLogstore);
        SenderQueueManager::GetInstance()->CreateQueue(
            mQueueKey,
            mPluginID,
            *mContext,
            {{"region", GetRegionConcurrencyLimiter(mRegion)},
             {"project", GetProjectConcurrencyLimiter(mProject)},
             {"logstore", GetLogstoreConcurrencyLimiter(mProject, mLogstore)}},
            mMaxSendRate);
    }

    GenerateGoPlugin(config, optionalGoPipeline);

    mSendCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL);
    mSendDoneCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SEND_DONE_TOTAL);
    mSuccessCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SUCCESS_TOTAL);
    mDiscardCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_DISCARD_TOTAL);
    mNetworkErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_NETWORK_ERROR_TOTAL);
    mServerErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SERVER_ERROR_TOTAL);
    mShardWriteQuotaErrorCnt
        = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SLS_SHARD_WRITE_QUOTA_ERROR_TOTAL);
    mProjectQuotaErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SLS_PROJECT_QUOTA_ERROR_TOTAL);
    mUnauthErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_UNAUTH_ERROR_TOTAL);
    mParamsErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_PARAMS_ERROR_TOTAL);
    mSequenceIDErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SLS_SEQUENCE_ID_ERROR_TOTAL);
    mRequestExpiredErrorCnt
        = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SLS_REQUEST_EXPRIRED_ERROR_TOTAL);
    mOtherErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_OTHER_ERROR_TOTAL);

    return true;
}

bool FlusherSLS::Start() {
    Flusher::Start();

    IncreaseProjectRegionReferenceCnt(mProject, mRegion);
    return true;
}

bool FlusherSLS::Stop(bool isPipelineRemoving) {
    Flusher::Stop(isPipelineRemoving);

    DecreaseProjectRegionReferenceCnt(mProject, mRegion);
    return true;
}


bool FlusherSLS::Send(PipelineEventGroup&& g) {
    if (g.IsReplay()) {
        return SerializeAndPush(std::move(g));
    } else {
        vector<BatchedEventsList> res;
        mBatcher.Add(std::move(g), res);
        return SerializeAndPush(std::move(res));
    }
}

bool FlusherSLS::Flush(size_t key) {
    BatchedEventsList res;
    mBatcher.FlushQueue(key, res);
    return SerializeAndPush(std::move(res));
}

bool FlusherSLS::FlushAll() {
    vector<BatchedEventsList> res;
    mBatcher.FlushAll(res);
    return SerializeAndPush(std::move(res));
}

bool FlusherSLS::BuildRequest(SenderQueueItem* item, unique_ptr<HttpSinkRequest>& req, bool* keepItem, string* errMsg) {
    ADD_COUNTER(mSendCnt, 1);

    SLSClientManager::AuthType type;
    string accessKeyId, accessKeySecret;
    if (!SLSClientManager::GetInstance()->GetAccessKey(mAliuid, type, accessKeyId, accessKeySecret)) {
#ifdef __ENTERPRISE__
        if (!EnterpriseSLSClientManager::GetInstance()->GetAccessKeyIfProjectSupportsAnonymousWrite(
                mProject, type, accessKeyId, accessKeySecret)) {
            *keepItem = true;
            *errMsg = "failed to get access key";
            return false;
        }
#endif
    }

    auto data = static_cast<SLSSenderQueueItem*>(item);
#ifdef __ENTERPRISE__
    if (BOOL_FLAG(send_prefer_real_ip)) {
        data->mCurrentHost = EnterpriseSLSClientManager::GetInstance()->GetRealIp(mRegion);
        if (data->mCurrentHost.empty()) {
            auto info
                = EnterpriseSLSClientManager::GetInstance()->GetCandidateHostsInfo(mRegion, mProject, mEndpointMode);
            if (mCandidateHostsInfo.get() != info.get()) {
                LOG_INFO(sLogger,
                         ("update candidate hosts info, region", mRegion)("project", mProject)("logstore", mLogstore)(
                             "from", EndpointModeToString(mCandidateHostsInfo->GetMode()))(
                             "to", EndpointModeToString(info->GetMode())));
                mCandidateHostsInfo = info;
            }
            data->mCurrentHost = mCandidateHostsInfo->GetCurrentHost();
            data->mRealIpFlag = false;
        } else {
            data->mRealIpFlag = true;
        }
    } else {
        // in case local region endpoint mode is changed, we should always check before sending
        auto info = EnterpriseSLSClientManager::GetInstance()->GetCandidateHostsInfo(mRegion, mProject, mEndpointMode);
        if (mCandidateHostsInfo == nullptr) {
            // TODO: temporarily used here, for send logtail alarm only, should be removed after alarm is refactored
            mCandidateHostsInfo = info;
        }
        if (mCandidateHostsInfo.get() != info.get()) {
            LOG_INFO(sLogger,
                     ("update candidate hosts info, region", mRegion)("project", mProject)("logstore", mLogstore)(
                         "from", EndpointModeToString(mCandidateHostsInfo->GetMode()))(
                         "to", EndpointModeToString(info->GetMode())));
            mCandidateHostsInfo = info;
        }
        data->mCurrentHost = mCandidateHostsInfo->GetCurrentHost();
    }
    if (data->mCurrentHost.empty()) {
        if (mCandidateHostsInfo->IsInitialized()) {
            GetRegionConcurrencyLimiter(mRegion)->OnFail(chrono::system_clock::now());
        }
        *errMsg = "failed to get available host";
        *keepItem = true;
        return false;
    }
#else
    static string host = mProject + "." + mEndpoint;
    data->mCurrentHost = host;
#endif

    switch (mTelemetryType) {
        case sls_logs::SLS_TELEMETRY_TYPE_LOGS:
        case sls_logs::SLS_TELEMETRY_TYPE_METRICS_MULTIVALUE:
            req = CreatePostLogStoreLogsRequest(accessKeyId, accessKeySecret, type, data);
            break;
        case sls_logs::SLS_TELEMETRY_TYPE_METRICS_HOST:
            req = CreatePostHostMetricsRequest(accessKeyId, accessKeySecret, type, data);
            break;
        case sls_logs::SLS_TELEMETRY_TYPE_METRICS:
            req = CreatePostMetricStoreLogsRequest(accessKeyId, accessKeySecret, type, data);
            break;
        case sls_logs::SLS_TELEMETRY_TYPE_APM_AGENTINFOS:
        case sls_logs::SLS_TELEMETRY_TYPE_APM_METRICS:
        case sls_logs::SLS_TELEMETRY_TYPE_APM_TRACES:
            req = CreatePostAPMBackendRequest(accessKeyId, accessKeySecret, type, data);
            break;
        default:
            break;
    }
    return true;
}

void FlusherSLS::OnSendDone(const HttpResponse& response, SenderQueueItem* item) {
    ADD_COUNTER(mSendDoneCnt, 1);
    SLSResponse slsResponse = ParseHttpResponse(response);

    auto data = static_cast<SLSSenderQueueItem*>(item);
    string configName = HasContext() ? GetContext().GetConfigName() : "";
    string hostname = data->mCurrentHost;
    bool isProfileData = GetProfileSender()->IsProfileData(mRegion, mProject, data->mLogstore);
    int32_t curTime = time(NULL);
    auto curSystemTime = chrono::system_clock::now();
    SendResult sendResult = SEND_OK;
    if (slsResponse.mStatusCode == 200) {
        auto& cpt = data->mExactlyOnceCheckpoint;
        if (cpt) {
            cpt->Commit();
            cpt->IncreaseSequenceID();
        }
        LOG_DEBUG(
            sLogger,
            ("send data to sls succeeded, item address", item)("request id", slsResponse.mRequestId)(
                "config", configName)("region", mRegion)("project", mProject)("logstore", data->mLogstore)(
                "response time",
                ToString(chrono::duration_cast<chrono::milliseconds>(curSystemTime - item->mLastSendTime).count())
                    + "ms")(
                "total send time",
                ToString(chrono::duration_cast<chrono::milliseconds>(curSystemTime - item->mFirstEnqueTime).count())
                    + "ms")("try cnt", data->mTryCnt)("endpoint", data->mCurrentHost)("is profile data",
                                                                                      isProfileData));
        GetRegionConcurrencyLimiter(mRegion)->OnSuccess(curSystemTime);
        GetProjectConcurrencyLimiter(mProject)->OnSuccess(curSystemTime);
        GetLogstoreConcurrencyLimiter(mProject, mLogstore)->OnSuccess(curSystemTime);
        SenderQueueManager::GetInstance()->DecreaseConcurrencyLimiterInSendingCnt(item->mQueueKey);
        ADD_COUNTER(mSuccessCnt, 1);
        DealSenderQueueItemAfterSend(item, false);
    } else {
        OperationOnFail operation;
        sendResult = ConvertErrorCode(slsResponse.mErrorCode);
        ostringstream failDetail, suggestion;
        if (sendResult == SEND_NETWORK_ERROR || sendResult == SEND_SERVER_ERROR) {
            if (sendResult == SEND_NETWORK_ERROR) {
                failDetail << "network error";
                ADD_COUNTER(mNetworkErrorCnt, 1);
            } else {
                failDetail << "server error";
                ADD_COUNTER(mServerErrorCnt, 1);
            }
            suggestion << "check network connection to endpoint";
#ifdef __ENTERPRISE__
            if (data->mRealIpFlag) {
                // connect refused, use vip directly
                failDetail << ", real ip may be stale, force update";
                EnterpriseSLSClientManager::GetInstance()->UpdateOutdatedRealIpRegions(mRegion);
            }
#endif
            operation = data->mBufferOrNot ? OperationOnFail::RETRY_LATER : OperationOnFail::DISCARD;
            GetRegionConcurrencyLimiter(mRegion)->OnFail(curSystemTime);
            GetProjectConcurrencyLimiter(mProject)->OnSuccess(curSystemTime);
            GetLogstoreConcurrencyLimiter(mProject, mLogstore)->OnSuccess(curSystemTime);
        } else if (sendResult == SEND_QUOTA_EXCEED) {
            if (slsResponse.mErrorCode == LOGE_SHARD_WRITE_QUOTA_EXCEED) {
                failDetail << "shard write quota exceed";
                suggestion << "Split logstore shards. https://help.aliyun.com/zh/sls/user-guide/expansion-of-resources";
                GetLogstoreConcurrencyLimiter(mProject, mLogstore)->OnFail(curSystemTime);
                GetRegionConcurrencyLimiter(mRegion)->OnSuccess(curSystemTime);
                GetProjectConcurrencyLimiter(mProject)->OnSuccess(curSystemTime);
                ADD_COUNTER(mShardWriteQuotaErrorCnt, 1);
            } else {
                failDetail << "project write quota exceed";
                suggestion << "Submit quota modification request. "
                              "https://help.aliyun.com/zh/sls/user-guide/expansion-of-resources";
                GetProjectConcurrencyLimiter(mProject)->OnFail(curSystemTime);
                GetRegionConcurrencyLimiter(mRegion)->OnSuccess(curSystemTime);
                GetLogstoreConcurrencyLimiter(mProject, mLogstore)->OnSuccess(curSystemTime);
                ADD_COUNTER(mProjectQuotaErrorCnt, 1);
            }
            AlarmManager::GetInstance()->SendAlarm(SEND_QUOTA_EXCEED_ALARM,
                                                   "error_code: " + slsResponse.mErrorCode
                                                       + ", error_message: " + slsResponse.mErrorMsg
                                                       + ", request_id:" + slsResponse.mRequestId,
                                                   mRegion,
                                                   mProject,
                                                   mContext ? mContext->GetConfigName() : "",
                                                   data->mLogstore);
            operation = OperationOnFail::RETRY_LATER;
        } else if (sendResult == SEND_UNAUTHORIZED) {
            failDetail << "write unauthorized";
            suggestion << "check access keys provided";
            operation = OperationOnFail::RETRY_LATER;
            ADD_COUNTER(mUnauthErrorCnt, 1);
        } else if (sendResult == SEND_PARAMETER_INVALID) {
            failDetail << "invalid parameters";
            suggestion << "check input parameters";
            operation = DefaultOperation(item->mTryCnt);
            ADD_COUNTER(mParamsErrorCnt, 1);
        } else if (sendResult == SEND_INVALID_SEQUENCE_ID) {
            failDetail << "invalid exactly-once sequence id";
            ADD_COUNTER(mSequenceIDErrorCnt, 1);
            do {
                auto& cpt = data->mExactlyOnceCheckpoint;
                if (!cpt) {
                    failDetail << ", unexpected result when exactly once checkpoint is not found";
                    suggestion << "report bug";
                    AlarmManager::GetInstance()->SendAlarm(
                        EXACTLY_ONCE_ALARM,
                        "drop exactly once log group because of invalid sequence ID, request id:"
                            + slsResponse.mRequestId,
                        mRegion,
                        mProject,
                        mContext ? mContext->GetConfigName() : "",
                        data->mLogstore);
                    operation = OperationOnFail::DISCARD;
                    break;
                }

                // Because hash key is generated by UUID library, we consider that
                //  the possibility of hash key conflict is very low, so data is
                //  dropped here.
                cpt->Commit();
                failDetail << ", drop exactly once log group and commit checkpoint"
                           << " checkpointKey:" << cpt->key << " checkpoint:" << cpt->data.DebugString();
                suggestion << "no suggestion";
                AlarmManager::GetInstance()->SendAlarm(
                    EXACTLY_ONCE_ALARM,
                    "drop exactly once log group because of invalid sequence ID, cpt:" + cpt->key
                        + ", data:" + cpt->data.DebugString() + "request id:" + slsResponse.mRequestId,
                    mRegion,
                    mProject,
                    mContext ? mContext->GetConfigName() : "",
                    data->mLogstore);
                operation = OperationOnFail::DISCARD;
                cpt->IncreaseSequenceID();
            } while (0);
        } else if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()
                   && LOGE_REQUEST_TIME_EXPIRED == slsResponse.mErrorCode) {
            failDetail << "write request expired, will retry";
            suggestion << "check local system time";
            operation = OperationOnFail::RETRY_IMMEDIATELY;
            ADD_COUNTER(mRequestExpiredErrorCnt, 1);
        } else {
            failDetail << "other error";
            suggestion << "no suggestion";
            // when unknown error such as SignatureNotMatch happens, we should retry several times
            // first time, we will retry immediately
            // then we record error and retry latter
            // when retry times > unknow_error_try_max, we will drop this data
            operation = DefaultOperation(item->mTryCnt);
            ADD_COUNTER(mOtherErrorCnt, 1);
        }
        if (chrono::duration_cast<chrono::seconds>(curSystemTime - item->mFirstEnqueTime).count()
            > INT32_FLAG(discard_send_fail_interval)) {
            operation = OperationOnFail::DISCARD;
        }
        if (isProfileData && data->mTryCnt >= static_cast<uint32_t>(INT32_FLAG(profile_data_send_retrytimes))) {
            operation = OperationOnFail::DISCARD;
        }

#define LOG_PATTERN \
    ("failed to send request", failDetail.str())("operation", GetOperationString(operation))("suggestion", \
                                                                                             suggestion.str())( \
        "item address", item)("request id", slsResponse.mRequestId)("status code", slsResponse.mStatusCode)( \
        "error code", slsResponse.mErrorCode)("errMsg", slsResponse.mErrorMsg)("config", configName)( \
        "region", mRegion)("project", mProject)("logstore", data->mLogstore)("try cnt", data->mTryCnt)( \
        "response time", \
        ToString(chrono::duration_cast<chrono::seconds>(curSystemTime - data->mLastSendTime).count()) \
            + "ms")("total send time", \
                    ToString(chrono::duration_cast<chrono::seconds>(curSystemTime - data->mFirstEnqueTime).count()) \
                        + "ms")("endpoint", data->mCurrentHost)("is profile data", isProfileData)

        switch (operation) {
            case OperationOnFail::RETRY_IMMEDIATELY:
                ++item->mTryCnt;
                FlusherRunner::GetInstance()->PushToHttpSink(item, false);
                break;
            case OperationOnFail::RETRY_LATER:
                if (slsResponse.mErrorCode == LOGE_REQUEST_TIMEOUT
                    || curTime - data->mLastLogWarningTime > ON_FAIL_LOG_WARNING_INTERVAL_SECOND) {
                    LOG_WARNING(sLogger, LOG_PATTERN);
                    data->mLastLogWarningTime = curTime;
                }
                SenderQueueManager::GetInstance()->DecreaseConcurrencyLimiterInSendingCnt(item->mQueueKey);
                DealSenderQueueItemAfterSend(item, true);
                break;
            case OperationOnFail::DISCARD:
                ADD_COUNTER(mDiscardCnt, 1);
            default:
                LOG_WARNING(sLogger, LOG_PATTERN);
                if (!isProfileData) {
                    AlarmManager::GetInstance()->SendAlarm(
                        SEND_DATA_FAIL_ALARM,
                        "failed to send request: " + failDetail.str() + "\toperation: " + GetOperationString(operation)
                            + "\trequestId: " + slsResponse.mRequestId
                            + "\tstatusCode: " + ToString(slsResponse.mStatusCode)
                            + "\terrorCode: " + slsResponse.mErrorCode + "\terrorMessage: " + slsResponse.mErrorMsg
                            + "\tconfig: " + configName + "\tendpoint: " + data->mCurrentHost,
                        mRegion,
                        mProject,
                        mContext ? mContext->GetConfigName() : "",
                        data->mLogstore);
                }
                SenderQueueManager::GetInstance()->DecreaseConcurrencyLimiterInSendingCnt(item->mQueueKey);
                DealSenderQueueItemAfterSend(item, false);
                break;
        }
    }
#ifdef __ENTERPRISE__
    bool hasNetworkError = sendResult == SEND_NETWORK_ERROR;
    EnterpriseSLSClientManager::GetInstance()->UpdateHostStatus(
        mProject, mCandidateHostsInfo->GetMode(), hostname, !hasNetworkError);
    mCandidateHostsInfo->SelectBestHost();

    if (!hasNetworkError) {
        bool hasAuthError = sendResult == SEND_UNAUTHORIZED;
        EnterpriseSLSClientManager::GetInstance()->UpdateAccessKeyStatus(mAliuid, !hasAuthError);
        EnterpriseSLSClientManager::GetInstance()->UpdateProjectAnonymousWriteStatus(mProject, !hasAuthError);
    }
#endif
}

bool FlusherSLS::Send(string&& data, const string& shardHashKey, const string& logstore) {
    string compressedData;
    if (mCompressor) {
        string errorMsg;
        if (!mCompressor->DoCompress(data, compressedData, errorMsg)) {
            LOG_WARNING(mContext->GetLogger(),
                        ("failed to compress data",
                         errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
            mContext->GetAlarm().SendAlarm(COMPRESS_FAIL_ALARM,
                                           "failed to compress data: " + errorMsg + "\taction: discard data\tplugin: "
                                               + sName + "\tconfig: " + mContext->GetConfigName(),
                                           mContext->GetRegion(),
                                           mContext->GetProjectName(),
                                           mContext->GetConfigName(),
                                           mContext->GetLogstoreName());
            return false;
        }
    } else {
        compressedData = data;
    }

    QueueKey key = mQueueKey;
    if (!HasContext()) {
        key = QueueKeyManager::GetInstance()->GetKey(mProject + "-" + mLogstore);
        if (SenderQueueManager::GetInstance()->GetQueue(key) == nullptr) {
            CollectionPipelineContext ctx;
            SenderQueueManager::GetInstance()->CreateQueue(
                key, "", ctx, std::unordered_map<std::string, std::shared_ptr<ConcurrencyLimiter>>());
        }
    }
    return Flusher::PushToQueue(make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                                data.size(),
                                                                this,
                                                                key,
                                                                logstore.empty() ? mLogstore : logstore,
                                                                RawDataType::EVENT_GROUP,
                                                                shardHashKey));
}

void FlusherSLS::GenerateGoPlugin(const Json::Value& config, Json::Value& res) const {
    Json::Value detail(Json::objectValue);
    for (auto itr = config.begin(); itr != config.end(); ++itr) {
        if (sNativeParam.find(itr.name()) == sNativeParam.end() && itr.name() != "Type") {
            detail[itr.name()] = *itr;
        }
    }
    if (mContext->IsFlushingThroughGoPipeline()) {
        Json::Value plugin(Json::objectValue);
        plugin["type"]
            = CollectionPipeline::GenPluginTypeWithID("flusher_sls", mContext->GetPipeline().GetNowPluginID());
        plugin["detail"] = detail;
        res["flushers"].append(plugin);
    }
}

bool FlusherSLS::SerializeAndPush(PipelineEventGroup&& group) {
    string serializedData, compressedData;
    BatchedEvents g(std::move(group.MutableEvents()),
                    std::move(group.GetSizedTags()),
                    std::move(group.GetSourceBuffer()),
                    group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                    std::move(group.GetExactlyOnceCheckpoint()));
    AddPackId(g);
    string errorMsg;
    if (!mGroupSerializer->DoSerialize(std::move(g), serializedData, errorMsg)) {
        LOG_WARNING(mContext->GetLogger(),
                    ("failed to serialize event group",
                     errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
        mContext->GetAlarm().SendAlarm(SERIALIZE_FAIL_ALARM,
                                       "failed to serialize event group: " + errorMsg
                                           + "\taction: discard data\tplugin: " + sName
                                           + "\tconfig: " + mContext->GetConfigName(),
                                       mContext->GetRegion(),
                                       mContext->GetProjectName(),
                                       mContext->GetConfigName(),
                                       mContext->GetLogstoreName());
        return false;
    }
    if (mCompressor) {
        if (!mCompressor->DoCompress(serializedData, compressedData, errorMsg)) {
            LOG_WARNING(mContext->GetLogger(),
                        ("failed to compress event group",
                         errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
            mContext->GetAlarm().SendAlarm(COMPRESS_FAIL_ALARM,
                                           "failed to compress event group: " + errorMsg
                                               + "\taction: discard data\tplugin: " + sName
                                               + "\tconfig: " + mContext->GetConfigName(),
                                           mContext->GetRegion(),
                                           mContext->GetProjectName(),
                                           mContext->GetConfigName(),
                                           mContext->GetLogstoreName());
            return false;
        }
    } else {
        compressedData = serializedData;
    }
    // must create a tmp, because eoo checkpoint is moved in second param
    auto fbKey = g.mExactlyOnceCheckpoint->fbKey;
    return PushToQueue(fbKey,
                       make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                       serializedData.size(),
                                                       this,
                                                       fbKey,
                                                       mLogstore,
                                                       RawDataType::EVENT_GROUP,
                                                       g.mExactlyOnceCheckpoint->data.hash_key(),
                                                       std::move(g.mExactlyOnceCheckpoint),
                                                       false));
}

bool FlusherSLS::SerializeAndPush(BatchedEventsList&& groupList) {
    if (groupList.empty()) {
        return true;
    }
    vector<CompressedLogGroup> compressedLogGroups;
    string shardHashKey, serializedData, compressedData;
    size_t packageSize = 0;
    bool enablePackageList = groupList.size() > 1;

    bool allSucceeded = true;
    for (auto& group : groupList) {
        if (!mShardHashKeys.empty()) {
            shardHashKey = GetShardHashKey(group);
        }
        AddPackId(group);
        string errorMsg;
        if (!mGroupSerializer->DoSerialize(std::move(group), serializedData, errorMsg)) {
            LOG_WARNING(mContext->GetLogger(),
                        ("failed to serialize event group",
                         errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
            mContext->GetAlarm().SendAlarm(SERIALIZE_FAIL_ALARM,
                                           "failed to serialize event group: " + errorMsg
                                               + "\taction: discard data\tplugin: " + sName
                                               + "\tconfig: " + mContext->GetConfigName(),
                                           mContext->GetRegion(),
                                           mContext->GetProjectName(),
                                           mContext->GetConfigName(),
                                           mContext->GetLogstoreName());
            allSucceeded = false;
            continue;
        }
        if (mCompressor) {
            if (!mCompressor->DoCompress(serializedData, compressedData, errorMsg)) {
                LOG_WARNING(mContext->GetLogger(),
                            ("failed to compress event group",
                             errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
                mContext->GetAlarm().SendAlarm(COMPRESS_FAIL_ALARM,
                                               "failed to compress event group: " + errorMsg
                                                   + "\taction: discard data\tplugin: " + sName
                                                   + "\tconfig: " + mContext->GetConfigName(),
                                               mContext->GetRegion(),
                                               mContext->GetProjectName(),
                                               mContext->GetConfigName(),
                                               mContext->GetLogstoreName());
                allSucceeded = false;
                continue;
            }
        } else {
            compressedData = serializedData;
        }
        if (enablePackageList) {
            packageSize += serializedData.size();
            compressedLogGroups.emplace_back(std::move(compressedData), serializedData.size());
        } else {
            if (group.mExactlyOnceCheckpoint) {
                // must create a tmp, because eoo checkpoint is moved in second param
                auto fbKey = group.mExactlyOnceCheckpoint->fbKey;
                allSucceeded
                    = PushToQueue(fbKey,
                                  make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                                  serializedData.size(),
                                                                  this,
                                                                  fbKey,
                                                                  mLogstore,
                                                                  RawDataType::EVENT_GROUP,
                                                                  group.mExactlyOnceCheckpoint->data.hash_key(),
                                                                  std::move(group.mExactlyOnceCheckpoint),
                                                                  false))
                    && allSucceeded;
            } else {
                allSucceeded = Flusher::PushToQueue(make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                                                    serializedData.size(),
                                                                                    this,
                                                                                    mQueueKey,
                                                                                    mLogstore,
                                                                                    RawDataType::EVENT_GROUP,
                                                                                    shardHashKey))
                    && allSucceeded;
            }
        }
    }
    if (enablePackageList) {
        string errorMsg;
        mGroupListSerializer->DoSerialize(std::move(compressedLogGroups), serializedData, errorMsg);
        allSucceeded
            = Flusher::PushToQueue(make_unique<SLSSenderQueueItem>(
                  std::move(serializedData), packageSize, this, mQueueKey, mLogstore, RawDataType::EVENT_GROUP_LIST))
            && allSucceeded;
    }
    return allSucceeded;
}

bool FlusherSLS::SerializeAndPush(vector<BatchedEventsList>&& groupLists) {
    bool allSucceeded = true;
    for (auto& groupList : groupLists) {
        allSucceeded = SerializeAndPush(std::move(groupList)) && allSucceeded;
    }
    return allSucceeded;
}

bool FlusherSLS::PushToQueue(QueueKey key, unique_ptr<SenderQueueItem>&& item, uint32_t retryTimes) {
    const string& str = QueueKeyManager::GetInstance()->GetName(key);
    for (size_t i = 0; i < retryTimes; ++i) {
        int rst = SenderQueueManager::GetInstance()->PushQueue(key, std::move(item));
        if (rst == 0) {
            return true;
        }
        if (rst == 2) {
            // should not happen
            LOG_ERROR(sLogger,
                      ("failed to push data to sender queue",
                       "queue not found")("action", "discard data")("config-flusher-dst", str));
            AlarmManager::GetInstance()->SendAlarm(
                DISCARD_DATA_ALARM,
                "failed to push data to sender queue: queue not found\taction: discard data\tconfig-flusher-dst" + str);
            return false;
        }
        if (i % 100 == 0) {
            LOG_WARNING(sLogger,
                        ("push attempts to sender queue continuously failed for the past second",
                         "retry again")("config-flusher-dst", str));
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    LOG_WARNING(
        sLogger,
        ("failed to push data to sender queue", "queue full")("action", "discard data")("config-flusher-dst", str));
    AlarmManager::GetInstance()->SendAlarm(
        DISCARD_DATA_ALARM,
        "failed to push data to sender queue: queue full\taction: discard data\tconfig-flusher-dst" + str);
    return false;
}

string FlusherSLS::GetShardHashKey(const BatchedEvents& g) const {
    // TODO: improve performance
    string key;
    for (size_t i = 0; i < mShardHashKeys.size(); ++i) {
        for (auto& item : g.mTags.mInner) {
            if (item.first == mShardHashKeys[i]) {
                key += item.second.to_string();
                break;
            }
        }
        if (i != mShardHashKeys.size() - 1) {
            key += "_";
        }
    }
    return CalcMD5(key);
}

void FlusherSLS::AddPackId(BatchedEvents& g) const {
    string packIdPrefixStr = g.mPackIdPrefix.to_string();
    int64_t packidPrefix = HashString(packIdPrefixStr);
    int64_t packSeq = PackIdManager::GetInstance()->GetAndIncPackSeq(
        HashString(packIdPrefixStr + "_" + mProject + "_" + mLogstore));
    auto packId = g.mSourceBuffers[0]->CopyString(ToHexString(packidPrefix) + "-" + ToHexString(packSeq));
    g.mTags.Insert(LOG_RESERVED_KEY_PACKAGE_ID, StringView(packId.data, packId.size));
}

unique_ptr<HttpSinkRequest> FlusherSLS::CreatePostLogStoreLogsRequest(const string& accessKeyId,
                                                                      const string& accessKeySecret,
                                                                      SLSClientManager::AuthType type,
                                                                      SLSSenderQueueItem* item) const {
    optional<uint64_t> seqId;
    if (item->mExactlyOnceCheckpoint) {
        seqId = item->mExactlyOnceCheckpoint->data.sequence_id();
    }
    string path, query;
    map<string, string> header;
    PreparePostLogStoreLogsRequest(accessKeyId,
                                   accessKeySecret,
                                   type,
                                   item->mCurrentHost,
                                   item->mRealIpFlag,
                                   mProject,
                                   item->mLogstore,
                                   CompressTypeToString(mCompressor->GetCompressType()),
                                   item->mType,
                                   item->mData,
                                   item->mRawSize,
                                   item->mShardHashKey,
                                   seqId,
                                   path,
                                   query,
                                   header);
    bool httpsFlag = SLSClientManager::GetInstance()->UsingHttps(mRegion);
    return make_unique<HttpSinkRequest>(HTTP_POST,
                                        httpsFlag,
                                        item->mCurrentHost,
                                        httpsFlag ? 443 : 80,
                                        path,
                                        query,
                                        header,
                                        item->mData,
                                        item,
                                        INT32_FLAG(default_http_request_timeout_sec),
                                        1,
                                        CurlSocket(INT32_FLAG(sls_request_dscp)));
}

unique_ptr<HttpSinkRequest> FlusherSLS::CreatePostHostMetricsRequest(const string& accessKeyId,
                                                                     const string& accessKeySecret,
                                                                     SLSClientManager::AuthType type,
                                                                     SLSSenderQueueItem* item) const {
    string path, query;
    map<string, string> header;
    PreparePostHostMetricsRequest(accessKeyId,
                                  accessKeySecret,
                                  type,
                                  CompressTypeToString(mCompressor->GetCompressType()),
                                  item->mType,
                                  item->mData,
                                  item->mRawSize,
                                  path,
                                  header);
    bool httpsFlag = SLSClientManager::GetInstance()->UsingHttps(mRegion);
    return make_unique<HttpSinkRequest>(HTTP_POST,
                                        httpsFlag,
                                        item->mCurrentHost,
                                        httpsFlag ? 443 : 80,
                                        path,
                                        query,
                                        header,
                                        item->mData,
                                        item,
                                        INT32_FLAG(default_http_request_timeout_sec),
                                        1,
                                        CurlSocket(INT32_FLAG(sls_request_dscp)));
}

unique_ptr<HttpSinkRequest> FlusherSLS::CreatePostMetricStoreLogsRequest(const string& accessKeyId,
                                                                         const string& accessKeySecret,
                                                                         SLSClientManager::AuthType type,
                                                                         SLSSenderQueueItem* item) const {
    string path;
    map<string, string> header;
    PreparePostMetricStoreLogsRequest(accessKeyId,
                                      accessKeySecret,
                                      type,
                                      item->mCurrentHost,
                                      item->mRealIpFlag,
                                      mProject,
                                      item->mLogstore,
                                      CompressTypeToString(mCompressor->GetCompressType()),
                                      item->mData,
                                      item->mRawSize,
                                      path,
                                      header);
    bool httpsFlag = SLSClientManager::GetInstance()->UsingHttps(mRegion);
    return make_unique<HttpSinkRequest>(HTTP_POST,
                                        httpsFlag,
                                        item->mCurrentHost,
                                        httpsFlag ? 443 : 80,
                                        path,
                                        "",
                                        header,
                                        item->mData,
                                        item,
                                        INT32_FLAG(default_http_request_timeout_sec),
                                        1,
                                        CurlSocket(INT32_FLAG(sls_request_dscp)));
}

unique_ptr<HttpSinkRequest> FlusherSLS::CreatePostAPMBackendRequest(const string& accessKeyId,
                                                                    const string& accessKeySecret,
                                                                    SLSClientManager::AuthType type,
                                                                    SLSSenderQueueItem* item) const {
    map<string, string> header;
    header.insert({CMS_HEADER_WORKSPACE, mWorkspace});
    header.insert({APM_HEADER_PROJECT, mProject});
    PreparePostAPMBackendRequest(accessKeyId,
                                 accessKeySecret,
                                 type,
                                 item->mCurrentHost,
                                 item->mRealIpFlag,
                                 mProject,
                                 CompressTypeToString(mCompressor->GetCompressType()),
                                 item->mType,
                                 item->mData,
                                 item->mRawSize,
                                 mSubpath,
                                 header);
    bool httpsFlag = SLSClientManager::GetInstance()->UsingHttps(mRegion);
    return make_unique<HttpSinkRequest>(HTTP_POST,
                                        httpsFlag,
                                        item->mCurrentHost,
                                        httpsFlag ? 443 : 80,
                                        mSubpath,
                                        "",
                                        header,
                                        item->mData,
                                        item,
                                        INT32_FLAG(default_http_request_timeout_sec),
                                        1,
                                        CurlSocket(INT32_FLAG(sls_request_dscp)));
}

bool FlusherSLS::IsRawSLSTelemetryType() const {
    return mTelemetryType == sls_logs::SLS_TELEMETRY_TYPE_LOGS || mTelemetryType == sls_logs::SLS_TELEMETRY_TYPE_METRICS
        || mTelemetryType == sls_logs::SLS_TELEMETRY_TYPE_METRICS_MULTIVALUE;
}

bool FlusherSLS::IsMetricsTelemetryType() const {
    return mTelemetryType != sls_logs::SLS_TELEMETRY_TYPE_METRICS
        && mTelemetryType != sls_logs::SLS_TELEMETRY_TYPE_METRICS_MULTIVALUE
        && mTelemetryType != sls_logs::SLS_TELEMETRY_TYPE_METRICS_HOST;
}

sls_logs::SlsCompressType ConvertCompressType(CompressType type) {
    sls_logs::SlsCompressType compressType = sls_logs::SLS_CMP_NONE;
    switch (type) {
        case CompressType::LZ4:
            compressType = sls_logs::SLS_CMP_LZ4;
            break;
        case CompressType::ZSTD:
            compressType = sls_logs::SLS_CMP_ZSTD;
            break;
        default:
            break;
    }
    return compressType;
}

} // namespace logtail
