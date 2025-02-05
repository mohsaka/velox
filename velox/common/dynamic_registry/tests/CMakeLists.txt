# Copyright (c) Facebook, Inc. and its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# To test functions being added by dynamically linked libraries, we compile
# `MyDynamicFunction.cpp` as a small .so library, and use the
# VELOX_TEST_DYNAMIC_LIBRARY_PATH macro to locate the .so binary.

add_library(velox_function_my_dynamic SHARED MyDynamicFunction.cpp)
add_library(velox_overwrite_int_function_my_dynamic SHARED
            MyDynamicIntFunctionOverwrite.cpp)
add_library(velox_overwrite_varchar_function_my_dynamic SHARED
            MyDynamicVarcharFunctionOverwrite.cpp)
add_library(velox_function_err_my_dynamic SHARED MyDynamicErrFunction.cpp)
add_library(velox_overload_int_function_my_dynamic SHARED
            MyDynamicIntFunctionOverload.cpp)
add_library(velox_overload_varchar_function_my_dynamic SHARED
            MyDynamicVarcharFunctionOverload.cpp)

set(CMAKE_DYLIB_TEST_LINK_LIBRARIES fmt::fmt glog::glog xsimd)

target_link_libraries(
  velox_function_my_dynamic
  PRIVATE ${CMAKE_DYLIB_TEST_LINK_LIBRARIES})

target_link_libraries(
  velox_overwrite_int_function_my_dynamic
  PRIVATE ${CMAKE_DYLIB_TEST_LINK_LIBRARIES})

target_link_libraries(
  velox_overwrite_varchar_function_my_dynamic
  PRIVATE ${CMAKE_DYLIB_TEST_LINK_LIBRARIES})

target_link_libraries(
  velox_function_err_my_dynamic
  PRIVATE ${CMAKE_DYLIB_TEST_LINK_LIBRARIES})

target_link_libraries(
  velox_overload_int_function_my_dynamic
  PRIVATE ${CMAKE_DYLIB_TEST_LINK_LIBRARIES})

target_link_libraries(
  velox_overload_varchar_function_my_dynamic
  PRIVATE ${CMAKE_DYLIB_TEST_LINK_LIBRARIES})

target_link_options(velox_function_my_dynamic PRIVATE
                    "-Wl,-undefined,dynamic_lookup")
target_link_options(velox_overwrite_int_function_my_dynamic PRIVATE
                    "-Wl,-undefined,dynamic_lookup")
target_link_options(velox_overwrite_varchar_function_my_dynamic PRIVATE
                    "-Wl,-undefined,dynamic_lookup")
target_link_options(velox_function_err_my_dynamic PRIVATE
                    "-Wl,-undefined,dynamic_lookup")
target_link_options(velox_overload_int_function_my_dynamic PRIVATE
                    "-Wl,-undefined,dynamic_lookup")
target_link_options(velox_overload_varchar_function_my_dynamic PRIVATE
                    "-Wl,-undefined,dynamic_lookup")

# Here's the actual test which will dynamically load the library defined above.
add_executable(velox_function_dynamic_link_test DynamicLinkTest.cpp)

target_link_libraries(
  velox_function_dynamic_link_test
  velox_functions_test_lib
  velox_dynamic_library_loader
  velox_function_registry
  xsimd
  GTest::gmock
  GTest::gtest
  GTest::gtest_main)

target_compile_definitions(
  velox_function_dynamic_link_test
  PRIVATE VELOX_TEST_DYNAMIC_LIBRARY_PATH="${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(
  velox_function_dynamic_link_test
  PRIVATE
    VELOX_TEST_DYNAMIC_LIBRARY_PATH_SUFFIX="${CMAKE_SHARED_LIBRARY_SUFFIX}")

add_test(NAME velox_function_dynamic_link_test
         COMMAND velox_function_dynamic_link_test)
