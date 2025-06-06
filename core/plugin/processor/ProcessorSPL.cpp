/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plugin/processor/ProcessorSPL.h"

#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "monitor/metric_constants/MetricConstants.h"

DEFINE_FLAG_INT32(logtail_spl_pipeline_quota, "", 16);
DEFINE_FLAG_INT32(logtail_spl_query_max_size, "", 65536);

namespace logtail {

const std::string ProcessorSPL::sName = "processor_spl";

const uint32_t uint32Max = std::numeric_limits<uint32_t>::max();

bool ProcessorSPL::Init(const Json::Value& config) {
    std::string errorMsg;
    if (!GetMandatoryStringParam(config, "Script", mSpl, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (!GetOptionalUIntParam(config, "TimeoutMilliSeconds", mTimeoutMills, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mTimeoutMills,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    if (!GetOptionalUIntParam(config, "MaxMemoryBytes", mMaxMemoryBytes, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mMaxMemoryBytes,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    mSPLPipelinePtr = std::make_shared<LoongCollectorSplPipeline>();
    errorMsg.clear();
    ResultCode success = mSPLPipelinePtr->InitLoongCollectorSPL(mSpl,
                                                                INT32_FLAG(logtail_spl_pipeline_quota),
                                                                INT32_FLAG(logtail_spl_query_max_size),
                                                                errorMsg,
                                                                mTimeoutMills,
                                                                mMaxMemoryBytes);
    if (success != ResultCode::OK) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "failed to parse spl: " + mSpl + " error: " + errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    mSplExcuteErrorCount = GetMetricsRecordRef().CreateCounter("proc_spl_excute_error_count");
    mSplExcuteTimeoutErrorCount = GetMetricsRecordRef().CreateCounter("proc_spl_excute_timeout_error_count");
    mSplExcuteMemoryExceedErrorCount = GetMetricsRecordRef().CreateCounter("proc_spl_excute_memory_exceed_error_count");

    // spl raw statistic
    mProcessMicros = GetMetricsRecordRef().CreateCounter("proc_spl_process_micros");
    mInputMicros = GetMetricsRecordRef().CreateCounter("proc_spl_input_micros");
    mOutputMicros = GetMetricsRecordRef().CreateCounter("proc_spl_output_micros");
    mMemPeakBytes = GetMetricsRecordRef().CreateIntGauge("proc_spl_mem_peak_bytes");
    mTotalTaskCount = GetMetricsRecordRef().CreateCounter("proc_spl_total_task_count");
    mSuccTaskCount = GetMetricsRecordRef().CreateCounter("proc_spl_succ_task_count");
    mFailTaskCount = GetMetricsRecordRef().CreateCounter("proc_spl_fail_task_count");

    return true;
}


void ProcessorSPL::Process(PipelineEventGroup& logGroup) {
    LOG_ERROR(sLogger,
              ("ProcessorSPL error", "unexpected enter in ProcessorSPL::Process(PipelineEventGroup& logGroup)")(
                  "project", mContext->GetProjectName())("logstore", mContext->GetLogstoreName())(
                  "region", mContext->GetRegion())("configName", mContext->GetConfigName()));
}


void ProcessorSPL::Process(std::vector<PipelineEventGroup>& logGroupList) {
    std::string errorMsg;
    if (logGroupList.empty()) {
        return;
    }
    PipelineEventGroup logGroup = std::move(logGroupList[0]);
    std::vector<PipelineEventGroup>().swap(logGroupList);

    PipelineStats pipelineStats;
    ResultCode result = mSPLPipelinePtr->Execute(std::move(logGroup), logGroupList, pipelineStats, mContext);

    const time_t currentTime = GetCurrentLogtailTime().tv_sec;

    for (auto& g : logGroupList) {
        EventsContainer& events = g.MutableEvents();
        for (auto& e : events) {
            // parse failed time in spl will be set to uint32Max
            if (e->GetTimestamp() == 0 || e->GetTimestamp() == uint32Max) {
                e->SetTimestamp(currentTime);
            }
        }
    }

    if (result != ResultCode::OK) {
        ADD_COUNTER(mSplExcuteErrorCount, 1);
        if (result == ResultCode::TIMEOUT_ERROR) {
            ADD_COUNTER(mSplExcuteTimeoutErrorCount, 1);
        } else if (result == ResultCode::MEM_EXCEEDED) {
            ADD_COUNTER(mSplExcuteMemoryExceedErrorCount, 1);
        }
    } else {
        ADD_COUNTER(mProcessMicros, pipelineStats.processMicros_);
        ADD_COUNTER(mInputMicros, pipelineStats.inputMicros_);
        ADD_COUNTER(mOutputMicros, pipelineStats.outputMicros_);
        SET_GAUGE(mMemPeakBytes, pipelineStats.memPeakBytes_);
        ADD_COUNTER(mTotalTaskCount, pipelineStats.totalTaskCount_);
        ADD_COUNTER(mSuccTaskCount, pipelineStats.succTaskCount_);
        ADD_COUNTER(mFailTaskCount, pipelineStats.failTaskCount_);
    }

    return;
}


bool ProcessorSPL::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}
} // namespace logtail
