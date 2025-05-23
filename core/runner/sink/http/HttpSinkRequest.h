/*
 * Copyright 2024 iLogtail Authors
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

#include <optional>

#include "collection_pipeline/queue/SenderQueueItem.h"
#include "common/http/HttpRequest.h"

namespace logtail {

struct HttpSinkRequest : public AsynHttpRequest {
    SenderQueueItem* mItem = nullptr;

    HttpSinkRequest(const std::string& method,
                    bool httpsFlag,
                    const std::string& host,
                    int32_t port,
                    const std::string& url,
                    const std::string& query,
                    const std::map<std::string, std::string>& header,
                    const std::string& body,
                    SenderQueueItem* item,
                    uint32_t timeout = static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)),
                    uint32_t maxTryCnt = static_cast<uint32_t>(INT32_FLAG(default_http_request_max_try_cnt)),
                    std::optional<CurlSocket> socket = std::nullopt)
        : AsynHttpRequest(method,
                          httpsFlag,
                          host,
                          port,
                          url,
                          query,
                          header,
                          body,
                          HttpResponse(),
                          timeout,
                          maxTryCnt,
                          false,
                          std::nullopt,
                          std::move(socket)),
          mItem(item) {}

    bool IsContextValid() const override { return true; }
    void OnSendDone(HttpResponse& response) override {}
};

} // namespace logtail
