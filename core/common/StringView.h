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

#pragma once
#include <string_view>

#include "boost/utility/string_view.hpp"

namespace logtail {

// like string, in string_view, tailing \0 is not included in size
using StringView = boost::string_view;

inline constexpr StringView kEmptyStringView("");

struct StringViewHash {
    size_t operator()(const StringView& k) const {
        return std::hash<std::string_view>()(std::string_view(k.data(), k.size()));
    }
};

struct StringViewEqual {
    bool operator()(const StringView& lhs, const StringView& rhs) const { return lhs == rhs; }
};

} // namespace logtail
