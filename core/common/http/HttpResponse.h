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

#include <cstdint>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

class curl_slist;

namespace logtail {

struct CurlTLS;
struct CurlSocket;

enum NetworkCode {
    Ok = 0,
    ConnectionFailed,
    RemoteAccessDenied,
    SSLConnectError,
    SSLCertError,
    SSLOtherProblem,
    SendDataFailed,
    RecvDataFailed,
    Timeout,
    Other
};

struct NetworkStatus {
    NetworkCode mCode = NetworkCode::Ok;
    std::string mMessage;
};

bool caseInsensitiveComp(const char lhs, const char rhs);

bool compareHeader(const std::string& lhs, const std::string& rhs);

size_t DefaultWriteCallback(char* buffer, size_t size, size_t nmemb, void* data);

class HttpResponse {
    friend void* CreateCurlHandler(const std::string& method,
                                   bool httpsFlag,
                                   const std::string& host,
                                   int32_t port,
                                   const std::string& url,
                                   const std::string& queryString,
                                   const std::map<std::string, std::string>& header,
                                   const std::string& body,
                                   HttpResponse& response,
                                   curl_slist*& headers,
                                   uint32_t timeout,
                                   bool replaceHostWithIp,
                                   const std::string& intf,
                                   bool followRedirects,
                                   const std::optional<CurlTLS>& tls,
                                   const std::optional<CurlSocket>& socket);

public:
    HttpResponse()
        : mHeader(compareHeader),
          mBody(new std::string(), [](void* p) { delete static_cast<std::string*>(p); }),
          mWriteCallback(DefaultWriteCallback) {}
    HttpResponse(void* body,
                 const std::function<void(void*)>& bodyDeleter,
                 size_t (*callback)(char*, size_t, size_t, void*))
        : mHeader(compareHeader), mBody(body, bodyDeleter), mWriteCallback(callback) {}

    int32_t GetStatusCode() const { return mStatusCode; }
    void SetStatusCode(int32_t code) { mStatusCode = code; }

    const std::map<std::string, std::string, decltype(compareHeader)*>& GetHeader() const { return mHeader; }

    template <class T>
    const T* GetBody() const {
        return static_cast<const T*>(mBody.get());
    }

    template <class T>
    T* GetBody() {
        return static_cast<T*>(mBody.get());
    }

    void SetResponseTime(const std::chrono::milliseconds& time) { mResponseTime = time; }
    std::chrono::milliseconds GetResponseTime() const { return mResponseTime; }

    const NetworkStatus& GetNetworkStatus() { return mNetworkStatus; }
    void SetNetworkStatus(NetworkCode code, const std::string& msg) {
        mNetworkStatus.mCode = code;
        mNetworkStatus.mMessage = msg;
    }

#ifdef APSARA_UNIT_TEST_MAIN
    template <class T>
    void SetBody(const T& body) {
        *mBody = body;
    }

    void AddHeader(const std::string& key, const std::string& value) { mHeader[key] = value; }
#endif

private:
    int32_t mStatusCode = 0; // 0 means no response from server
    NetworkStatus mNetworkStatus;
    std::map<std::string, std::string, decltype(compareHeader)*> mHeader;
    std::unique_ptr<void, std::function<void(void*)>> mBody;
    size_t (*mWriteCallback)(char*, size_t, size_t, void*) = nullptr;
    std::chrono::milliseconds mResponseTime = std::chrono::milliseconds::max();

#ifdef APSARA_UNIT_TEST_MAIN
    friend class HttpSinkMock;
#endif
};

} // namespace logtail
