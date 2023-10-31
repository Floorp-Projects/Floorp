# Copyright 2022 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
cmake_minimum_required(VERSION 3.22)

project(chrome_enterprise_connector_local_analysis)

# Ensure a C++14 compiler is used.
set(CMAKE_CXX_STANDARD 14)

# Determine the operating system being targeted.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(WIN TRUE)
  set(MAC FALSE)
  set(LINUX FALSE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(WIN FALSE)
  set(MAC TRUE)
  set(LINUX FALSE)
else()
  set(WIN FALSE)
  set(MAC FALSE)
  set(LINUX TRUE)
endif()

# Set the path to the protoc protobuf compiler.
if(WIN)
  set(PROTOC ${PROJECT_BINARY_DIR}/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe)
elseif(MAC)
  set(PROTOC ${PROJECT_BINARY_DIR}/vcpkg/installed/x64-osx/tools/protobuf/protoc)
elseif(LINUX)
  set(PROTOC ${PROJECT_BINARY_DIR}/vcpkg/installed/x64-linux/tools/protobuf/protoc)
endif()

# Calls the protoc compiler using the arguments specific to this project.
# protobuf_generate_cpp is not flexible enough for our needs.
add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc
  COMMAND
    ${PROTOC}
    --cpp_out=${PROJECT_BINARY_DIR}/gen
    --proto_path=${PROJECT_SOURCE_DIR}/proto
    ${PROJECT_SOURCE_DIR}/proto/content_analysis/sdk/analysis.proto
  DEPENDS ./proto/content_analysis/sdk/analysis.proto
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)
# Define proto target. Compile this target exclusively by calling:
# `cmake --build <build_dir> --target proto`
add_custom_target(proto
  ALL
  DEPENDS
  ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc
)

# The include directory contains the header files needed by the demo code.
# The gen directory contains generated protobuf headers describing the request
# and response objects used to communicate with Google Chrome.
set(AGENT_INCLUDES
  ./agent/include
  .
  ${PROJECT_BINARY_DIR}/gen
)
set(BROWSER_INCLUDES
  ./browser/include
  .
  ${PROJECT_BINARY_DIR}/gen
)

# The SDK contains platform specific code for each of the supported platforms.
# ${PLATFORM_AGENT_CODE} holds the list of source files needed for the current
# platform being built.
if(WIN)
  set(PLATFORM_AGENT_CODE
    ./agent/src/agent_utils_win.cc
    ./agent/src/agent_utils_win.h
    ./agent/src/agent_win.cc
    ./agent/src/agent_win.h
    ./agent/src/event_win.cc
    ./agent/src/event_win.h
    ./agent/src/scoped_print_handle_win.cc
    ./agent/src/scoped_print_handle_win.h
    ./common/utils_win.cc
    ./common/utils_win.h
  )
  set(PLATFORM_TEST_CODE
    ./agent/src/agent_win_unittest.cc
    ./agent/src/event_win_unittest.cc
  )
elseif(MAC)
  set(PLATFORM_AGENT_CODE
    ./agent/src/agent_mac.cc
    ./agent/src/agent_mac.h
    ./agent/src/event_mac.cc
    ./agent/src/event_mac.h
    ./agent/src/scoped_print_handle_mac.cc
    ./agent/src/scoped_print_handle_mac.h
  )
  set(PLATFORM_TEST_CODE
    ./agent/src/event_mac_unittest.cc
  )
elseif(LINUX)
  set(PLATFORM_AGENT_CODE
    ./agent/src/agent_posix.cc
    ./agent/src/agent_posix.h
    ./agent/src/event_posix.cc
    ./agent/src/event_posix.h
    ./agent/src/scoped_print_handle_posix.cc
    ./agent/src/scoped_print_handle_posix.h
  )
  set(PLATFORM_TEST_CODE
    ./agent/src/event_posix_unittest.cc
  )
endif()

# The SDK contains platform specific code for each of the supported platforms.
# ${PLATFORM_BROWSER_CODE} holds the list of source files needed for the current
# platform being built.
if(WIN)
  set(PLATFORM_BROWSER_CODE
    ./browser/src/client_win.cc
    ./browser/src/client_win.h
    ./common/utils_win.cc
    ./common/utils_win.h
  )
elseif(MAC)
  set(PLATFORM_BROWSER_CODE
    ./browser/src/client_mac.cc
    ./browser/src/client_mac.h
  )
elseif(LINUX)
  set(PLATFORM_BROWSER_CODE
    ./browser/src/client_posix.cc
    ./browser/src/client_posix.h
  )
endif()

# Makes available the package definitions in vcpkg.
include("${PROJECT_BINARY_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
find_package(Protobuf CONFIG REQUIRED)
# Unit tests
enable_testing()
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(unit_tests
  ${PLATFORM_TEST_CODE}
)
set_property(TARGET unit_tests PROPERTY CXX_STANDARD 20)
target_include_directories(unit_tests
  PRIVATE
  ${AGENT_INCLUDES}
  ${BROWSER_INCLUDES}
)
target_link_libraries(unit_tests
  PUBLIC 
  cac_agent
  cac_browser
  GTest::gtest GTest::gtest_main
)

gtest_discover_tests(unit_tests) 

# Builds the content analysis connector agent linker library.  This library
# is linked into the agent in order to listen for and process content analysis
# requests from Google Chrome.
add_library(cac_agent
  ./agent/include/content_analysis/sdk/analysis_agent.h
  ./agent/include/content_analysis/sdk/result_codes.h
  ./agent/src/agent_base.cc
  ./agent/src/agent_base.h
  ./agent/src/event_base.cc
  ./agent/src/event_base.h
  ./agent/src/scoped_print_handle_base.cc
  ./agent/src/scoped_print_handle_base.h
  ${PLATFORM_AGENT_CODE}
  ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc
)
target_link_libraries(cac_agent
  PUBLIC
  protobuf::libprotoc
  protobuf::libprotobuf
  protobuf::libprotobuf-lite)
target_include_directories(cac_agent PRIVATE ${AGENT_INCLUDES})
# Builds the content analysis connector browser linker library.  This library
# is linked into the client in order to send content analysis requests to the
# agent.
add_library(cac_browser
  ./browser/include/content_analysis/sdk/analysis_client.h
  ./browser/src/client_base.cc
  ./browser/src/client_base.h
  ${PLATFORM_BROWSER_CODE}
  ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc
)
target_include_directories(cac_browser PRIVATE ${BROWSER_INCLUDES})
target_link_libraries(cac_browser
  PUBLIC
  protobuf::libprotoc
  protobuf::libprotobuf
  protobuf::libprotobuf-lite)

# The demo agent executable.
add_executable(agent
  ./demo/agent.cc
  ./demo/handler.h
)
target_compile_features(agent PRIVATE cxx_std_17)
target_include_directories(agent PRIVATE ${AGENT_INCLUDES})
target_link_libraries(agent PRIVATE cac_agent)

# The demo client executable.
add_executable(browser ./demo/client.cc)
target_include_directories(browser PRIVATE ${BROWSER_INCLUDES})
target_link_libraries(browser PRIVATE cac_browser)

