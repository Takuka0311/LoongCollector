// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <filesystem>
#include <memory>
#include <string>

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/JsonUtil.h"
#include "file_server/FileServer.h"
#include "plugin/input/InputFile.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(default_plugin_log_queue_size);
DECLARE_FLAG_STRING(default_container_host_path);

using namespace std;

namespace logtail {

class InputFileUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnEnableContainerDiscovery();
    void TestCreateInnerProcessors();
    void OnPipelineUpdate();
    void TestSetContainerBaseDir();

protected:
    static void SetUpTestCase() {
        AppConfig::GetInstance()->mPurageContainerMode = true;
        PluginRegistry::GetInstance()->LoadPlugins();
    }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        p.mPluginID.store(0);
        ctx.SetPipeline(p);
    }

private:
    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputFileUnittest::OnSuccessfulInit() {
    unique_ptr<InputFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(input->mEnableContainerDiscovery);
    APSARA_TEST_EQUAL(0U, input->mMaxCheckpointDirSearchDepth);
    APSARA_TEST_EQUAL(0U, input->mExactlyOnceConcurrency);
    APSARA_TEST_FALSE(ctx.IsExactlyOnceEnabled());

    // valid optional param
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableContainerDiscovery": true,
            "MaxCheckpointDirSearchDepth": 1,
            "EnableExactlyOnce": 1
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->mEnableContainerDiscovery);
    APSARA_TEST_EQUAL(1U, input->mMaxCheckpointDirSearchDepth);
    APSARA_TEST_EQUAL(1U, input->mExactlyOnceConcurrency);
    APSARA_TEST_TRUE(ctx.IsExactlyOnceEnabled());

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableContainerDiscovery": "true",
            "MaxCheckpointDirSearchDepth": true,
            "EnableExactlyOnce": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(input->mEnableContainerDiscovery);
    APSARA_TEST_EQUAL(0U, input->mMaxCheckpointDirSearchDepth);
    APSARA_TEST_EQUAL(0U, input->mExactlyOnceConcurrency);
    APSARA_TEST_FALSE(ctx.IsExactlyOnceEnabled());

    // TailingAllMatchedFiles
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "TailingAllMatchedFiles": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->mFileReader.mTailingAllMatchedFiles);
    APSARA_TEST_TRUE(input->mFileDiscovery.IsTailingAllMatchedFiles());

    // ExactlyOnceConcurrency
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableExactlyConcurrency": 600
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(0U, input->mExactlyOnceConcurrency);
    APSARA_TEST_FALSE(ctx.IsExactlyOnceEnabled());
}

void InputFileUnittest::OnFailedInit() {
    unique_ptr<InputFile> input;
    Json::Value configJson, optionalGoPipeline;

    input.reset(new InputFile());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
}

void InputFileUnittest::OnEnableContainerDiscovery() {
    unique_ptr<InputFile> input;
    Json::Value configJson, optionalGoPipelineJson, optionalGoPipeline;
    string configStr, optionalGoPipelineStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");

    configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "EnableContainerDiscovery": true,
                "ContainerFilters": {
                    "K8sNamespaceRegex": "default"
                },
                "CollectingContainersMeta": true
            }
        )";
    optionalGoPipelineStr = R"(
            {
                "global": {},
                "inputs": [
                    {                
                        "type": "metric_container_info/2",
                        "detail": {
                            "CollectingContainersMeta": true,
                            "FilePattern": "*.log",
                            "K8sNamespaceRegex": "default",
                            "MaxDepth": 0,
                            "LogPath": ""
                        }
                    }
                ]
            }
        )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(optionalGoPipelineStr, optionalGoPipelineJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    optionalGoPipelineJson["global"]["DefaultLogQueueSize"] = Json::Value(INT32_FLAG(default_plugin_log_queue_size));
    optionalGoPipelineJson["inputs"][0]["detail"]["LogPath"] = Json::Value(filePath.parent_path().string());
    PluginInstance::PluginMeta meta = ctx.GetPipeline().GenNextPluginMeta(false);
    input.reset(new InputFile());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, meta.mPluginID);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->mEnableContainerDiscovery);
    APSARA_TEST_TRUE(input->mFileDiscovery.IsContainerDiscoveryEnabled());
    APSARA_TEST_EQUAL(optionalGoPipelineJson.toStyledString(), optionalGoPipeline.toStyledString());

    // not in container but with flag set
    AppConfig::GetInstance()->mPurageContainerMode = false;
    configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "EnableContainerDiscovery": true
            }
        )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    meta = ctx.GetPipeline().GenNextPluginMeta(false);
    input.reset(new InputFile());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputFile::sName, meta.mPluginID);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(input->mEnableContainerDiscovery);
    APSARA_TEST_FALSE(input->mFileDiscovery.IsContainerDiscoveryEnabled());
    AppConfig::GetInstance()->mPurageContainerMode = true;
}

void InputFileUnittest::TestCreateInnerProcessors() {
    unique_ptr<InputFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");
    {
        // no multiline
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL('\n', plugin->mSplitChar);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
    }
    {
        // custom multiline
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "Multiline": {
                    "StartPattern": "\\d+",
                    "EndPattern": "end",
                    "IgnoringUnmatchWarning": true,
                    "UnmatchedContentTreatment": "discard"
                },
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitMultilineLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitMultilineLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, plugin->mMultiline.mMode);
        APSARA_TEST_STREQ("\\d+", plugin->mMultiline.mStartPattern.c_str());
        APSARA_TEST_STREQ("", plugin->mMultiline.mContinuePattern.c_str());
        APSARA_TEST_STREQ("end", plugin->mMultiline.mEndPattern.c_str());
        APSARA_TEST_TRUE(plugin->mMultiline.mIgnoringUnmatchWarning);
        APSARA_TEST_EQUAL(MultilineOptions::UnmatchedContentTreatment::DISCARD,
                          plugin->mMultiline.mUnmatchedContentTreatment);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
    }
    {
        // json multiline, first processor is json parser
        ctx.SetIsFirstProcessorJsonFlag(true);
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL('\0', plugin->mSplitChar);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetIsFirstProcessorJsonFlag(false);
    }
    {
        // json multiline, json mode
        ctx.SetIsFirstProcessorJsonFlag(true);
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "Multiline": {
                    "Mode": "JSON"
                },
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL('\0', plugin->mSplitChar);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetIsFirstProcessorJsonFlag(false);
    }
    {
        // disable raw content: has native processor
        ctx.SetHasNativeProcessorsFlag(true);
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetHasNativeProcessorsFlag(false);
    }
    {
        // disable raw content: exactly once
        ctx.SetExactlyOnceFlag(true);
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetExactlyOnceFlag(false);
    }
    {
        // disable raw content: flushing through go pipeline
        ctx.SetIsFlushingThroughGoPipelineFlag(true);
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetIsFlushingThroughGoPipelineFlag(false);
    }
    {
        // enable raw content
        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_TRUE(plugin->mEnableRawContent);
    }
}

void InputFileUnittest::OnPipelineUpdate() {
    Json::Value configJson, optionalGoPipeline;
    InputFile input;
    input.SetContext(ctx);
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");

    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableExactlyOnce": 2
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();

    APSARA_TEST_TRUE(input.Start());
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetFileDiscoveryConfig("test_config").first);
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetFileReaderConfig("test_config").first);
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetMultilineConfig("test_config").first);
    APSARA_TEST_EQUAL(2U, FileServer::GetInstance()->GetExactlyOnceConcurrency("test_config"));

    APSARA_TEST_TRUE(input.Stop(true));
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetFileDiscoveryConfig("test_config").first);
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetFileReaderConfig("test_config").first);
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetMultilineConfig("test_config").first);
    APSARA_TEST_EQUAL(0U, FileServer::GetInstance()->GetExactlyOnceConcurrency("test_config"));
}

void InputFileUnittest::TestSetContainerBaseDir() {
    InputFile inputFile;
    ContainerInfo containerInfo;
    containerInfo.mID = "testContainer";
    containerInfo.mUpperDir = "/UpperDir";
    containerInfo.mMounts.push_back(Mount("/source1", "/data1"));
    containerInfo.mMounts.push_back(Mount("/source2", "/data1/data2"));
    containerInfo.mMounts.push_back(Mount("/source3", "/data1/data2/data3"));
    containerInfo.mMounts.push_back(Mount("/source4", "/data1/data2/data3/data4"));

    containerInfo.mRealBaseDir = "";
    ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data2/log"));
    APSARA_TEST_EQUAL(STRING_FLAG(default_container_host_path) + "/UpperDir/data2/log", containerInfo.mRealBaseDir);

    containerInfo.mRealBaseDir = "";
    ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/log"));
    APSARA_TEST_EQUAL(STRING_FLAG(default_container_host_path) + "/source1/log", containerInfo.mRealBaseDir);

    containerInfo.mRealBaseDir = "";
    ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/log"));
    APSARA_TEST_EQUAL(STRING_FLAG(default_container_host_path) + "/source2/log", containerInfo.mRealBaseDir);

    containerInfo.mRealBaseDir = "";
    ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/data3/log"));
    APSARA_TEST_EQUAL(STRING_FLAG(default_container_host_path) + "/source3/log", containerInfo.mRealBaseDir);

    containerInfo.mRealBaseDir = "";
    ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/data3/data4/log"));
    APSARA_TEST_EQUAL(STRING_FLAG(default_container_host_path) + "/source4/log", containerInfo.mRealBaseDir);

    containerInfo.mRealBaseDir = "";
    ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/data3/data4/data5/log"));
    APSARA_TEST_EQUAL(STRING_FLAG(default_container_host_path) + "/source4/data5/log", containerInfo.mRealBaseDir);
}

UNIT_TEST_CASE(InputFileUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputFileUnittest, OnFailedInit)
UNIT_TEST_CASE(InputFileUnittest, OnEnableContainerDiscovery)
UNIT_TEST_CASE(InputFileUnittest, TestCreateInnerProcessors)
UNIT_TEST_CASE(InputFileUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(InputFileUnittest, TestSetContainerBaseDir)

} // namespace logtail

UNIT_TEST_MAIN
