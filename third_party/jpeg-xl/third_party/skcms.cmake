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

function(target_link_skcms TARGET_NAME)
  target_sources(${TARGET_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/third_party/skcms/skcms.cc")
  target_include_directories(${TARGET_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/third_party/skcms/")

  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-Wno-psabi" CXX_WPSABI_SUPPORTED)
  if(CXX_WPSABI_SUPPORTED)
    set_source_files_properties("${PROJECT_SOURCE_DIR}/third_party/skcms/skcms.cc"
      PROPERTIES COMPILE_OPTIONS "-Wno-psabi"
      TARGET_DIRECTORY ${TARGET_NAME})
  endif()
endfunction()
