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

add_library(skcms STATIC skcms/skcms.cc)
target_include_directories(skcms PUBLIC "${CMAKE_CURRENT_LIST_DIR}/skcms/")

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-Wno-psabi" CXX_WPSABI_SUPPORTED)
if(CXX_WPSABI_SUPPORTED)
  target_compile_options(skcms PRIVATE -Wno-psabi)
endif()

set_property(TARGET skcms PROPERTY POSITION_INDEPENDENT_CODE ON)
