# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

set(JPEGLI_INTERNAL_SOURCES
  jpegli/color_transform.h
  jpegli/color_transform.cc
  jpegli/common.h
  jpegli/common.cc
  jpegli/dct.h
  jpegli/dct.cc
  jpegli/decode.h
  jpegli/decode.cc
  jpegli/decode_internal.h
  jpegli/decode_marker.h
  jpegli/decode_marker.cc
  jpegli/decode_scan.h
  jpegli/decode_scan.cc
  jpegli/destination_manager.cc
  jpegli/encode.h
  jpegli/encode.cc
  jpegli/encode_internal.h
  jpegli/entropy_coding.h
  jpegli/entropy_coding.cc
  jpegli/error.h
  jpegli/error.cc
  jpegli/huffman.h
  jpegli/huffman.cc
  jpegli/idct.h
  jpegli/idct.cc
  jpegli/memory_manager.h
  jpegli/quant.h
  jpegli/quant.cc
  jpegli/render.h
  jpegli/render.cc
  jpegli/source_manager.h
  jpegli/source_manager.cc
  jpegli/upsample.h
  jpegli/upsample.cc
)

set(JPEGLI_INTERNAL_LIBS
  hwy
  jxl-static
  Threads::Threads
  ${ATOMICS_LIBRARIES}
)

add_library(jpegli-static STATIC EXCLUDE_FROM_ALL "${JPEGLI_INTERNAL_SOURCES}")
target_compile_options(jpegli-static PRIVATE "${JPEGXL_INTERNAL_FLAGS}")
target_compile_options(jpegli-static PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jpegli-static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jpegli-static PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:hwy,INTERFACE_INCLUDE_DIRECTORIES>>"
)
target_include_directories(jpegli-static PUBLIC "${JPEG_INCLUDE_DIRS}")
target_link_libraries(jpegli-static PUBLIC ${JPEGLI_INTERNAL_LIBS})

#
# Tests for jpegli-static
#

if(BUILD_TESTING)
set(TEST_FILES
  jpegli/decode_api_test.cc
  jpegli/encode_api_test.cc
)

# Individual test binaries:
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests)
foreach (TESTFILE IN LISTS TEST_FILES)
  # The TESTNAME is the name without the extension or directory.
  get_filename_component(TESTNAME ${TESTFILE} NAME_WE)
  add_executable(${TESTNAME} ${TESTFILE} jpegli/test_utils.h)
  target_compile_options(${TESTNAME} PRIVATE
    ${JPEGXL_INTERNAL_FLAGS}
    # Add coverage flags to the test binary so code in the private headers of
    # the library is also instrumented when running tests that execute it.
    ${JPEGXL_COVERAGE_FLAGS}
  )
  target_compile_definitions(${TESTNAME} PRIVATE
    -DTEST_DATA_PATH="${JPEGXL_TEST_DATA_PATH}")
  target_include_directories(${TESTNAME} PRIVATE "${PROJECT_SOURCE_DIR}")
  target_link_libraries(${TESTNAME}
    hwy
    jpegli-static
    gmock
    GTest::GTest
    GTest::Main
  )
  # Output test targets in the test directory.
  set_target_properties(${TESTNAME} PROPERTIES PREFIX "tests/")
  if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(${TESTNAME} PROPERTIES COMPILE_FLAGS "-Wno-error")
  endif ()
  if(CMAKE_VERSION VERSION_LESS "3.10.3")
    gtest_discover_tests(${TESTNAME} TIMEOUT 240)
  else ()
    gtest_discover_tests(${TESTNAME} DISCOVERY_TIMEOUT 240)
  endif ()
endforeach ()
endif()

#
# Build libjpeg.so that links to libjpeg-static
#

if (JPEGXL_ENABLE_JPEGLI_LIBJPEG AND NOT APPLE AND NOT WIN32 AND NOT JPEGXL_EMSCRIPTEN)
set(JPEGLI_LIBJPEG_MAJOR_VERSION 62)
set(JPEGLI_LIBJPEG_MINOR_VERSION 3)
set(JPEGLI_LIBJPEG_PATCH_VERSION 0)
set(JPEGLI_LIBJPEG_LIBRARY_VERSION
    "${JPEGLI_LIBJPEG_MAJOR_VERSION}.${JPEGLI_LIBJPEG_MINOR_VERSION}.${JPEGLI_LIBJPEG_PATCH_VERSION}"
)

set(JPEGLI_LIBJPEG_LIBRARY_SOVERSION "${JPEGLI_LIBJPEG_MAJOR_VERSION}")

set(JPEGLI_LIBJPEG_OBJ_COMPILE_DEFINITIONS
  JPEGLI_LIBJPEG_MAJOR_VERSION=${JPEGLI_LIBJPEG_MAJOR_VERSION}
  JPEGLI_LIBJPEG_MINOR_VERSION=${JPEGLI_LIBJPEG_MINOR_VERSION}
  JPEGLI_LIBJPEG_PATCH_VERSION=${JPEGLI_LIBJPEG_PATCH_VERSION}
)

add_library(jpegli-libjpeg-obj OBJECT jpegli/libjpeg_wrapper.cc)
target_compile_options(jpegli-libjpeg-obj PRIVATE ${JPEGXL_INTERNAL_FLAGS})
target_compile_options(jpegli-libjpeg-obj PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jpegli-libjpeg-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jpegli-libjpeg-obj PUBLIC "${PROJECT_SOURCE_DIR}")
target_compile_definitions(jpegli-libjpeg-obj PUBLIC
  ${JPEGLI_LIBJPEG_OBJ_COMPILE_DEFINITIONS}
)
set(JPEGLI_LIBJPEG_INTERNAL_OBJECTS $<TARGET_OBJECTS:jpegli-libjpeg-obj>)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/jpegli)
add_library(jpeg SHARED ${JPEGLI_LIBJPEG_INTERNAL_OBJECTS})
target_link_libraries(jpeg PUBLIC ${JPEGXL_COVERAGE_FLAGS})
target_link_libraries(jpeg PRIVATE jpegli-static)
set_target_properties(jpeg PROPERTIES
  VERSION ${JPEGLI_LIBJPEG_LIBRARY_VERSION}
  SOVERSION ${JPEGLI_LIBJPEG_LIBRARY_SOVERSION}
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/jpegli"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/jpegli")

# Add a jpeg.version file as a version script to tag symbols with the
# appropriate version number.
set_target_properties(jpeg PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/jpegli/jpeg.version)
set_property(TARGET jpeg APPEND_STRING PROPERTY
  LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/jpegli/jpeg.version")

# This hides the default visibility symbols from static libraries bundled into
# the shared library. In particular this prevents exposing symbols from hwy
# in the shared library.
if(LINKER_SUPPORT_EXCLUDE_LIBS)
  set_property(TARGET jpeg APPEND_STRING PROPERTY
    LINK_FLAGS " ${LINKER_EXCLUDE_LIBS_FLAG}")
endif()
endif()
