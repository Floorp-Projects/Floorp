# Copyright (c) the JPEG XL Project
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

# Enable tests in third_party/ as well.
enable_testing()
include(CTest)

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

if(BUILD_TESTING)
# Add GTest from source and alias it to what the find_package(GTest) workflow
# defines. Omitting googletest/ directory would require it to be available in
# the base system instead, but it would work just fine. This makes packages
# using GTest and calling find_package(GTest) actually work.
if (EXISTS "${SOURCE_DIR}/googletest/CMakeLists.txt" AND
    NOT JPEGXL_FORCE_SYSTEM_GTEST)
  add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
  include(GoogleTest)

  set(GTEST_ROOT "${SOURCE_DIR}/googletest/googletest")
  set(GTEST_INCLUDE_DIR "$<TARGET_PROPERTY:INCLUDE_DIRECTORIES,gtest>"
      CACHE STRING "")
  set(GMOCK_INCLUDE_DIR "$<TARGET_PROPERTY:INCLUDE_DIRECTORIES,gmock>")
  set(GTEST_LIBRARY "$<TARGET_FILE:gtest>")
  set(GTEST_MAIN_LIBRARY "$<TARGET_FILE:gtest_main>")

  set_target_properties(gtest PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  set_target_properties(gmock PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  set_target_properties(gtest_main PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  set_target_properties(gmock_main PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

  # googletest doesn't compile clean with clang-cl (-Wundef)
  if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(gtest PROPERTIES COMPILE_FLAGS "-Wno-error")
    set_target_properties(gmock PROPERTIES COMPILE_FLAGS "-Wno-error")
    set_target_properties(gtest_main PROPERTIES COMPILE_FLAGS "-Wno-error")
    set_target_properties(gmock_main PROPERTIES COMPILE_FLAGS "-Wno-error")
  endif ()
  configure_file("${SOURCE_DIR}/googletest/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.googletest COPYONLY)
else()
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/googletest/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.googletest COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
  find_package(GTest REQUIRED)
endif()

endif()  # BUILD_TESTING
