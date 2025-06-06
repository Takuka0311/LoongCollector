/*
 * Copyright 2022 iLogtail Authors
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

#pragma once

#if defined(_MSC_VER)
#include <stddef.h>
#endif

#include <cstdint>

#include <numeric>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "json/json.h"

#include "plugin/flusher/sls/FlusherSLS.h"
#include "protobuf/sls/sls_logs.pb.h"

extern "C" {
// The definition of Golang type is copied from PluginAdaptor.h that
// generated by `go build -buildmode=c-shared ...`.
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN64) // 64-bit system
typedef long long GoInt64;
typedef GoInt64 GoInt;
typedef struct {
    const char* p;
    GoInt64 n;
} GoString;
typedef struct {
    void* data;
    GoInt len;
    GoInt cap;
} GoSlice;
#elif defined(_WIN32) // x86 Go + x86 MinGW + Win32 VS.
typedef long long GoInt64;
typedef int GoInt32;
typedef GoInt32 GoInt;
typedef struct {
    const char* p;
    ptrdiff_t n;
} _GoString_;
typedef _GoString_ GoString;
typedef struct {
    void* data;
    GoInt len;
    GoInt cap;
} GoSlice;
#endif

struct innerContainerMeta {
    char* podName;
    char* k8sNamespace;
    char* containerName;
    char* image;
    int k8sLabelsSize;
    int containerLabelsSize;
    int envSize;
    char** k8sLabelsKey;
    char** k8sLabelsVal;
    char** containerLabelsKey;
    char** containerLabelsVal;
    char** envsKey;
    char** envsVal;
};

typedef struct {
    char* key;
    char* value;
} InnerKeyValue;

typedef struct {
    InnerKeyValue** keyValues;
    int count;
} InnerPluginMetric;

typedef struct {
    InnerPluginMetric** metrics;
    int count;
} InnerPluginMetrics;

struct K8sContainerMeta {
    std::string PodName;
    std::string K8sNamespace;
    std::string ContainerName;
    std::string Image;
    std::unordered_map<std::string, std::string> containerLabels;
    std::unordered_map<std::string, std::string> k8sLabels;
    std::unordered_map<std::string, std::string> envs;
    std::string ToString() {
        std::stringstream ss;
        ss << "PodName: " << PodName << " K8sNamespace: " << K8sNamespace << " ContainerName: " << ContainerName
           << " Image: " << Image;
        ss << " containerLabels: "
           << std::accumulate(containerLabels.begin(),
                              containerLabels.end(),
                              std::string(),
                              [](const std::string& s, const std::pair<const std::string, std::string>& p) {
                                  return s + p.first + "=" + p.second + ",";
                              });
        ss << " k8sLabels: "
           << std::accumulate(k8sLabels.begin(),
                              k8sLabels.end(),
                              std::string(),
                              [](const std::string& s, const std::pair<const std::string, std::string>& p) {
                                  return s + p.first + "=" + p.second + ",";
                              });
        ss << " envs: "
           << std::accumulate(envs.begin(),
                              envs.end(),
                              std::string(),
                              [](const std::string& s, const std::pair<const std::string, std::string>& p) {
                                  return s + p.first + "=" + p.second + ",";
                              });
        return ss.str();
    }
};

// Methods export by plugin.
typedef GoInt (*LoadGlobalConfigFun)(GoString);
typedef GoInt (*LoadPipelineFun)(GoString p, GoString l, GoString c, GoInt64 k, GoString p2);
typedef GoInt (*UnloadPipelineFun)(GoString c);
typedef void (*StopAllPipelinesFun)(GoInt);
typedef void (*StopFun)(GoString, GoInt);
typedef void (*StopBuiltInModulesFun)();
typedef void (*StartFun)(GoString);
typedef GoInt (*InitPluginBaseFun)();
typedef GoInt (*InitPluginBaseV2Fun)(GoString cfg);
typedef GoInt (*ProcessLogsFun)(GoString c, GoSlice l, GoString p, GoString t, GoSlice tags);
typedef GoInt (*ProcessLogGroupFun)(GoString c, GoSlice l, GoString p);
typedef struct innerContainerMeta* (*GetContainerMetaFun)(GoString containerID);
typedef InnerPluginMetrics* (*GetGoMetricsFun)(GoString metricType);

// Methods export by adapter.
typedef int (*IsValidToSendFun)(long long logstoreKey);

typedef int (*SendPbFun)(const char* configName,
                         int configNameSize,
                         const char* logstore,
                         int logstoreSize,
                         char* pbBuffer,
                         int pbSize,
                         int lines);
typedef int (*SendPbV2Fun)(const char* configName,
                           int configNameSize,
                           const char* logstore,
                           int logstoreSize,
                           char* pbBuffer,
                           int pbSize,
                           int lines,
                           const char* shardHash,
                           int shardHashSize);

typedef int (*PluginCtlCmdFun)(
    const char* configName, int configNameSize, int optId, const char* params, int paramsLen);

typedef void (*RegisterLogtailCallBack)(IsValidToSendFun checkFun, SendPbFun sendFun, PluginCtlCmdFun cmdFun);
typedef void (*RegisterLogtailCallBackV2)(IsValidToSendFun checkFun,
                                          SendPbFun sendFun,
                                          SendPbV2Fun sendV2Fun,
                                          PluginCtlCmdFun cmdFun);

typedef int (*PluginAdapterVersion)();
}

// Create by david zhang. 2017/09/02 22:22:12
class LogtailPlugin {
public:
    LogtailPlugin();
    ~LogtailPlugin();

    enum PluginCmdType {
        PLUGIN_CMD_MIN = 0,
        PLUGIN_DOCKER_UPDATE_FILE = 1,
        PLUGIN_DOCKER_REMOVE_FILE = 2,
        PLUGIN_DOCKER_UPDATE_FILE_ALL = 3,
        PLUGIN_DOCKER_STOP_FILE = 4,
        PLUGIN_CMD_MAX = 5
    };

    static LogtailPlugin* GetInstance() {
        if (s_instance == NULL) {
            s_instance = new LogtailPlugin;
        }
        return s_instance;
    }

    static void FinalizeInstance() {
        if (s_instance != NULL) {
            delete s_instance;
            s_instance = NULL;
        }
    }

    bool LoadPluginBase();
    bool LoadPipeline(const std::string& pipelineName,
                      const std::string& pipeline,
                      const std::string& project = "",
                      const std::string& logstore = "",
                      const std::string& region = "",
                      logtail::QueueKey logstoreKey = 0);
    bool UnloadPipeline(const std::string& pipelineName);
    void StopAllPipelines(bool withInputFlag);
    void Stop(const std::string& configName, bool removingFlag);
    void StopBuiltInModules();
    void Start(const std::string& configName);

    bool IsPluginOpened() { return mPluginValid; }

    void ProcessLog(const std::string& configName,
                    sls_logs::Log& log,
                    const std::string& packId,
                    const std::string& topic,
                    const std::string& tags);

    void ProcessLogGroup(const std::string& configName, const std::string& logGroup, const std::string& packId);

    static int IsValidToSend(long long logstoreKey);

    static int SendPb(const char* configName,
                      int32_t configNameSize,
                      const char* logstore,
                      int logstoreSize,
                      char* pbBuffer,
                      int32_t pbSize,
                      int32_t lines);

    // because go pipeline supports route, but only use one flusher, so the actual logstore is given by go, not by the
    // logstore param in FlusherSLS
    static int SendPbV2(const char* configName,
                        int32_t configNameSize,
                        const char* logstore,
                        int logstoreSize,
                        char* pbBuffer,
                        int32_t pbSize,
                        int32_t lines,
                        const char* shardHash,
                        int shardHashSize);

    static int ExecPluginCmd(const char* configName, int configNameSize, int cmdId, const char* params, int paramsLen);

    K8sContainerMeta GetContainerMeta(logtail::StringView containerID);
    K8sContainerMeta GetContainerMeta(const std::string& containerID);

    void GetGoMetrics(std::vector<std::map<std::string, std::string>>& metircsList, const std::string& metricType);

private:
    void* mPluginBasePtr;
    void* mPluginAdapterPtr;

    LoadGlobalConfigFun mLoadGlobalConfigFun;
    LoadPipelineFun mLoadPipelineFun;
    UnloadPipelineFun mUnloadPipelineFun;
    StopAllPipelinesFun mStopAllPipelinesFun;
    StopFun mStopFun;
    StopBuiltInModulesFun mStopBuiltInModulesFun;
    StartFun mStartFun;
    volatile bool mPluginValid;
    logtail::FlusherSLS mPluginAlarmConfig;
    logtail::FlusherSLS mPluginContainerConfig;
    ProcessLogsFun mProcessLogsFun;
    ProcessLogGroupFun mProcessLogGroupFun;
    GetContainerMetaFun mGetContainerMetaFun;
    GetGoMetricsFun mGetGoMetricsFun;

    // Configuration for plugin system in JSON format.
    Json::Value mPluginCfg;

private:
    static LogtailPlugin* s_instance;
};
