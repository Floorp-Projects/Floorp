# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

add_executable(decode_oneshot decode_oneshot.cc)
target_link_libraries(decode_oneshot jxl_dec jxl_threads)
add_executable(encode_oneshot encode_oneshot.cc)
target_link_libraries(encode_oneshot jxl jxl_threads)

add_executable(jxlinfo jxlinfo.c)
target_link_libraries(jxlinfo jxl)

if(NOT ${SANITIZER} STREQUAL "none")
  # Linking a C test binary with the C++ JPEG XL implementation when using
  # address sanitizer is not well supported by clang 9, so force using clang++
  # for linking this test if a sanitizer is used.
  set_target_properties(jxlinfo PROPERTIES LINKER_LANGUAGE CXX)
endif()  # SANITIZER != "none"
