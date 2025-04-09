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

#include "collection_pipeline/serializer/JsonSerializer.h"
#include <unordered_map>

#include "constants/SpanConstants.h"
// TODO: the following dependencies should be removed
#include "protobuf/sls/LogGroupSerializer.h"

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace std;

namespace logtail {

const char* JSON_KEY_TIME = "__time__";

// Helper function to serialize common fields (tags and time)
template <typename WriterType>
void SerializeCommonFields(const unordered_map<const char*, const char*>& tags, uint64_t timestamp, WriterType& writer) {
    // Serialize tags
    for (const auto& tag : tags) {
        writer.Key(tag.first);
        writer.String(tag.second);
    }
    // Serialize time
    writer.Key(JSON_KEY_TIME);
    writer.Uint64(timestamp);
}

bool JsonEventGroupSerializer::Serialize(BatchedEvents&& group, string& res, string& errorMsg) {
    if (group.mEvents.empty()) {
        errorMsg = "empty event group";
        return false;
    }

    PipelineEvent::Type eventType = group.mEvents[0]->GetType();
    if (eventType == PipelineEvent::Type::NONE) {
        // should not happen
        errorMsg = "unsupported event type in event group";
        return false;
    }

    // Temporary buffer to store serialized tags
    unordered_map<const char*, const char*> tags;
    tags.reserve(group.mTags.mInner.size());
    for (const auto& tag : group.mTags.mInner) {
        tags[tag.first.to_string().c_str()] = tag.second.to_string().c_str();
    }

    // Use a single string buffer to avoid frequent memory allocations
    string buffer;
    buffer.reserve(group.mSizeBytes);

    // Create reusable StringBuffer and Writer
    rapidjson::StringBuffer jsonBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonBuffer);
    auto resetBuffer = [&jsonBuffer]() {
        jsonBuffer.Clear(); // Clear the buffer for reuse
    };

    // Temporary buffer to store serialized events
    vector<string> tempBuffer;
    tempBuffer.reserve(group.mEvents.size());

    // TODO: should support nano second
    switch (eventType) {
        case PipelineEvent::Type::LOG:
            for (const auto& item : group.mEvents) {
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
                tempBuffer.emplace_back(jsonBuffer.GetString(), jsonBuffer.GetSize());
            }
            break;
        case PipelineEvent::Type::METRIC:
            // TODO: key should support custom key
            for (const auto& item : group.mEvents) {
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
                tempBuffer.emplace_back(jsonBuffer.GetString(), jsonBuffer.GetSize());
            }
            break;
        case PipelineEvent::Type::SPAN:
            // TODO: implement span serializer
            LOG_ERROR(
                sLogger,
                ("invalid event type", "span type is not supported")("config", mFlusher->GetContext().GetConfigName()));
            break;
        case PipelineEvent::Type::RAW:
            for (const auto& item : group.mEvents) {
                const auto& e = item.Cast<RawEvent>();
                resetBuffer();

                writer.StartObject();
                SerializeCommonFields(tags, e.GetTimestamp(), writer);
                // content
                writer.Key(DEFAULT_CONTENT_KEY.c_str());
                writer.String(e.GetContent().to_string().c_str());
                writer.EndObject();
                tempBuffer.emplace_back(jsonBuffer.GetString(), jsonBuffer.GetSize());
            }
            break;
        default:
            break;
    }

    // Combine all events into a single string
    for (const auto& eventStr : tempBuffer) {
        buffer.append(eventStr);
        buffer.append("\n");
    }
    res = std::move(buffer);
    return true;
}

} // namespace logtail
