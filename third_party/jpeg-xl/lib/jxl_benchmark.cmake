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

# All files ending in "_gbench.cc" are considered Google benchmark files and
# should be listed here.
set(JPEGXL_INTERNAL_SOURCES_GBENCH
  extras/tone_mapping_gbench.cc
  jxl/dec_external_image_gbench.cc
  jxl/enc_external_image_gbench.cc
  jxl/splines_gbench.cc
  jxl/tf_gbench.cc
)

# benchmark.h doesn't work in our MINGW set up since it ends up including the
# wrong stdlib header. We don't run gbench on MINGW targets anyway.
if(NOT MINGW)

# This is the Google benchmark project (https://github.com/google/benchmark).
find_package(benchmark QUIET)

if(benchmark_FOUND)
  # Compiles all the benchmark files into a single binary. Individual benchmarks
  # can be run with --benchmark_filter.
  add_executable(jxl_gbench "${JPEGXL_INTERNAL_SOURCES_GBENCH}")

  target_compile_definitions(jxl_gbench PRIVATE
    -DTEST_DATA_PATH="${PROJECT_SOURCE_DIR}/third_party/testdata")
  target_link_libraries(jxl_gbench
    jxl_extras-static
    jxl-static
    benchmark::benchmark
    benchmark::benchmark_main
  )
endif() # benchmark_FOUND

endif() # MINGW
