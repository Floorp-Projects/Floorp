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

set(JPEGXL_PROFILER_SOURCES
  profiler/profiler.cc
  profiler/profiler.h
  profiler/tsc_timer.h
)

### Static library.
add_library(jxl_profiler STATIC ${JPEGXL_PROFILER_SOURCES})
target_link_libraries(jxl_profiler hwy)

target_compile_options(jxl_profiler PRIVATE ${JPEGXL_INTERNAL_FLAGS})
target_compile_options(jxl_profiler PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jxl_profiler PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(jxl_profiler
  PRIVATE "${PROJECT_SOURCE_DIR}")

set_target_properties(jxl_profiler PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
)

# Make every library linking against the jxl_profiler define this macro to
# enable the profiler.
target_compile_definitions(jxl_profiler
  PUBLIC -DPROFILER_ENABLED)
