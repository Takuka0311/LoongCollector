# Copyright 2024 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
 
cmake_minimum_required(VERSION 3.22)
project(serializer_unittest)

add_executable(serializer_unittest SerializerUnittest.cpp)
target_link_libraries(serializer_unittest ${UT_BASE_TARGET})

add_executable(sls_serializer_unittest SLSSerializerUnittest.cpp)
target_link_libraries(sls_serializer_unittest ${UT_BASE_TARGET})

add_executable(json_serializer_unittest JsonSerializerUnittest.cpp)
target_link_libraries(json_serializer_unittest ${UT_BASE_TARGET})

include(GoogleTest)
gtest_discover_tests(serializer_unittest)
gtest_discover_tests(sls_serializer_unittest)
gtest_discover_tests(json_serializer_unittest)