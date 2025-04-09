// Copyright 2024 iLogtail Authors
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

#include "plugin/flusher/file/FlusherFile.h"

#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"


#include "MetricTypes.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "protobuf/sls/LogGroupSerializer.h"

using namespace std;

namespace logtail {

const string FlusherFile::sName = "flusher_file";

bool FlusherFile::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    static uint32_t cnt = 0;
    GenerateQueueKey(to_string(++cnt));
    SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, *mContext);

    string errorMsg;
    // FilePath
    if (!GetMandatoryStringParam(config, "FilePath", mFilePath, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    // MaxFileSize
    GetMandatoryUIntParam(config, "MaxFileSize", mMaxFileSize, errorMsg);
    // MaxFiles
    GetMandatoryUIntParam(config, "MaxFiles", mMaxFileSize, errorMsg);

    // create file writer
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(mFilePath, mMaxFileSize, mMaxFiles, true);
    mFileWriter = std::make_shared<spdlog::async_logger>(
        sName, file_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    mFileWriter->set_pattern("%v");

    // mGroupSerializer = make_unique<JsonEventGroupSerializer>(this);
    mSendGroupCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL);
    return true;
}

bool FlusherFile::Send(PipelineEventGroup&& g) {
    ADD_COUNTER(mSendGroupCnt, 1);
    return SerializeAndPush(std::move(g));
}

bool FlusherFile::Flush(size_t key) {
    return true;
}

bool FlusherFile::FlushAll() {
    return true;
}

// Helper function to serialize common fields (tags and time)
template <typename WriterType>
void SerializeCommonFields(const unordered_map<const char*, const char*>& tags, uint64_t timestamp, WriterType& writer) {
    // Serialize tags
    for (const auto& tag : tags) {
        writer.Key(tag.first);
        writer.String(tag.second);
    }
    // Serialize time
    writer.Key("__time__");
    writer.Uint64(timestamp);
}

bool FlusherFile::SerializeAndPush(PipelineEventGroup&& group) {
    string serializedData;
    string errorMsg;
    BatchedEvents g(std::move(group.MutableEvents()),
                    std::move(group.GetSizedTags()),
                    std::move(group.GetSourceBuffer()),
                    group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                    std::move(group.GetExactlyOnceCheckpoint()));
    // mGroupSerializer->DoSerialize(std::move(g), serializedData, errorMsg);
    if (g.mEvents.empty()) {
        errorMsg = "empty event group";
        return false;
    }

    PipelineEvent::Type eventType = g.mEvents[0]->GetType();
    if (eventType == PipelineEvent::Type::NONE) {
        // should not happen
        errorMsg = "unsupported event type in event group";
        return false;
    }

    // Temporary buffer to store serialized tags
    unordered_map<const char*, const char*> tags;
    tags.reserve(g.mTags.mInner.size());
    for (const auto& tag : g.mTags.mInner) {
        tags[tag.first.to_string().c_str()] = tag.second.to_string().c_str();
    }

    // Create reusable StringBuffer and Writer
    rapidjson::StringBuffer jsonBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonBuffer);
    auto resetBuffer = [&jsonBuffer]() {
        jsonBuffer.Clear(); // Clear the buffer for reuse
    };

    // TODO: should support nano second
    switch (eventType) {
        case PipelineEvent::Type::LOG:
            for (const auto& item : g.mEvents) {
                const auto& e = item.Cast<LogEvent>();
                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(tags, e.GetTimestamp(), writer);
                // contents
                for (const auto& kv : e) {
                    writer.Key(kv.first.to_string().c_str());
                    writer.String(kv.second.to_string().c_str());
                }
                writer.EndObject();
                mFileWriter->info(jsonBuffer.GetString());
            }
            break;
        case PipelineEvent::Type::METRIC:
            // TODO: key should support custom key
            for (const auto& item : g.mEvents) {
                const auto& e = item.Cast<MetricEvent>();
                if (e.Is<std::monostate>()) {
                    continue;
                }
                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(tags, e.GetTimestamp(), writer);
                // __labels__
                writer.Key(METRIC_RESERVED_KEY_LABELS.c_str());
                writer.StartObject();
                for (auto tag = e.TagsBegin(); tag != e.TagsEnd(); tag++) {
                    writer.Key(tag->first.to_string().c_str());
                    writer.String(tag->second.to_string().c_str());
                }
                writer.EndObject();
                // __name__
                writer.Key(METRIC_RESERVED_KEY_NAME.c_str());
                writer.String(e.GetName().to_string().c_str());
                // __value__
                writer.Key(METRIC_RESERVED_KEY_VALUE.c_str());
                if (e.Is<UntypedSingleValue>()) {
                    writer.Double(e.GetValue<UntypedSingleValue>()->mValue);
                } else if (e.Is<UntypedMultiDoubleValues>()) {
                    writer.StartObject();
                    for (auto value = e.GetValue<UntypedMultiDoubleValues>()->ValuesBegin();
                         value != e.GetValue<UntypedMultiDoubleValues>()->ValuesEnd();
                         value++) {
                        writer.Key(value->first.to_string().c_str());
                        writer.Double(value->second.Value);
                    }
                    writer.EndObject();
                }
                writer.EndObject();
                mFileWriter->info(jsonBuffer.GetString());
            }
            break;
        case PipelineEvent::Type::SPAN:
            // TODO: implement span serializer
            LOG_ERROR(
                sLogger,
                ("invalid event type", "span type is not supported")("config", GetContext().GetConfigName()));
            break;
        case PipelineEvent::Type::RAW:
            for (const auto& item : g.mEvents) {
                const auto& e = item.Cast<RawEvent>();
                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(tags, e.GetTimestamp(), writer);
                // content
                writer.Key(DEFAULT_CONTENT_KEY.c_str());
                writer.String(e.GetContent().to_string().c_str());
                writer.EndObject();
                mFileWriter->info(jsonBuffer.GetString());
            }
            break;
        default:
            break;
    }

    mFileWriter->flush();
    return true;
}

} // namespace logtail
