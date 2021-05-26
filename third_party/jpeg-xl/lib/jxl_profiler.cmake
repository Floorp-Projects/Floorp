# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

set(JPEGXL_PROFILER_SOURCES
  profiler/profiler.cc
  profiler/profiler.h
  profiler/tsc_timer.h
)

### Static library.
add_library(jxl_profiler STATIC ${JPEGXL_PROFILER_SOURCES})
target_link_libraries(jxl_profiler PUBLIC hwy)

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
  PUBLIC -DPROFILER_ENABLED=1)
