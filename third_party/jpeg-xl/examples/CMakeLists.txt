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
