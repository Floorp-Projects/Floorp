# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

function(jxl_discover_tests TESTNAME)
  if (CMAKE_VERSION VERSION_LESS "3.10.3")
    gtest_discover_tests(${TESTNAME} TIMEOUT 240)
  else ()
    gtest_discover_tests(${TESTNAME} DISCOVERY_TIMEOUT 240)
  endif ()
endfunction()

if (CMAKE_VERSION VERSION_LESS "3.12.4")
  set(JXL_HWY_INCLUDE_DIRS "$<BUILD_INTERFACE:$<TARGET_PROPERTY:hwy,INTERFACE_INCLUDE_DIRECTORIES>>")
else()
  set(JXL_HWY_INCLUDE_DIRS "$<BUILD_INTERFACE:$<TARGET_PROPERTY:$<IF:$<TARGET_EXISTS:hwy::hwy>,hwy::hwy,hwy>,INTERFACE_INCLUDE_DIRECTORIES>>")
endif()
