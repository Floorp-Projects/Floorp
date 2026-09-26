# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)
project(FloorpAppShim LANGUAGES CXX OBJCXX)

if(NOT APPLE)
  message(FATAL_ERROR "Floorp App Shim requires macOS")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

function(sign_development_target target identifier)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND /usr/bin/codesign --force --sign - --identifier ${identifier} "$<TARGET_FILE:${target}>"
    VERBATIM)
endfunction()

add_library(FloorpAppShimCore STATIC
  Protocol.mm MachTransport.mm PeerIdentity.mm Session.mm ShimView.mm ShimApplication.mm ColdLaunch.mm)
target_include_directories(FloorpAppShimCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(FloorpAppShimCore PUBLIC -fobjc-arc -fno-exceptions
  PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
target_link_libraries(FloorpAppShimCore PUBLIC
  "-framework Cocoa" "-framework QuartzCore" "-framework IOSurface"
  "-framework Security" "-framework Carbon" bsm)

add_executable(floorp-app-shim main.mm)
target_link_libraries(floorp-app-shim PRIVATE FloorpAppShimCore)
sign_development_target(floorp-app-shim org.floorp.appshim.development)

include(CTest)
if(BUILD_TESTING)
  add_executable(floorp-app-shim-tests Tests.mm)
  target_compile_definitions(floorp-app-shim-tests PRIVATE FLOORP_SHIM_NATIVE_TESTS=1)
  target_link_libraries(floorp-app-shim-tests PRIVATE FloorpAppShimCore)
  sign_development_target(floorp-app-shim-tests org.floorp.appshim.native-tests)
  add_test(NAME native-app-shim COMMAND floorp-app-shim-tests)
  set_tests_properties(native-app-shim PROPERTIES TIMEOUT 30)
  add_executable(floorp-app-shim-bundle-tests
    runtime/BundleTransactionTests.mm runtime/BundleTransaction.mm)
  target_compile_options(floorp-app-shim-bundle-tests PRIVATE -fobjc-arc -fno-exceptions -UNDEBUG)
  target_link_libraries(floorp-app-shim-bundle-tests PRIVATE "-framework Foundation")
  sign_development_target(floorp-app-shim-bundle-tests org.floorp.appshim.bundle-tests)
  add_test(NAME native-bundle-transactions COMMAND floorp-app-shim-bundle-tests)
  set_tests_properties(native-bundle-transactions PROPERTIES TIMEOUT 30)
endif()
