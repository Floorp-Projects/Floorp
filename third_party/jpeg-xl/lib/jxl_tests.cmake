# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

include(jxl_lists.cmake)

if(BUILD_TESTING OR JPEGXL_ENABLE_TOOLS)
# Library with test-only code shared between all tests / fuzzers.
add_library(jxl_testlib-internal STATIC ${JPEGXL_INTERNAL_TESTLIB_FILES})
target_compile_options(jxl_testlib-internal PRIVATE
  ${JPEGXL_INTERNAL_FLAGS}
  ${JPEGXL_COVERAGE_FLAGS}
)
target_compile_definitions(jxl_testlib-internal PUBLIC
  -DTEST_DATA_PATH="${JPEGXL_TEST_DATA_PATH}")
target_include_directories(jxl_testlib-internal PUBLIC
  "${PROJECT_SOURCE_DIR}"
)
target_link_libraries(jxl_testlib-internal
  hwy
  jxl_extras_nocodec-internal
  jxl-internal
  jxl_threads
)
endif()

if(NOT BUILD_TESTING)
  return()
endif()

list(APPEND JPEGXL_INTERNAL_TESTS
  # TODO(deymo): Move this to tools/
  ../tools/djxl_fuzzer_test.cc
)

find_package(GTest)

# Individual test binaries:
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests)
foreach (TESTFILE IN LISTS JPEGXL_INTERNAL_TESTS)
  # The TESTNAME is the name without the extension or directory.
  get_filename_component(TESTNAME ${TESTFILE} NAME_WE)
  if(TESTFILE STREQUAL ../tools/djxl_fuzzer_test.cc)
    add_executable(${TESTNAME} ${TESTFILE} ../tools/djxl_fuzzer.cc)
  else()
    add_executable(${TESTNAME} ${TESTFILE})
  endif()
  if(EMSCRIPTEN)
    # The emscripten linking step takes too much memory and crashes during the
    # wasm-opt step when using -O2 optimization level
    set_target_properties(${TESTNAME} PROPERTIES LINK_FLAGS "\
      -O1 \
      -s USE_LIBPNG=1 \
      -s ALLOW_MEMORY_GROWTH=1 \
      -s SINGLE_FILE=1 \
      -s PROXY_TO_PTHREAD \
      -s EXIT_RUNTIME=1 \
      -s USE_PTHREADS=1 \
      -s NODERAWFS=1 \
    ")
  else()
    set_target_properties(${TESTNAME} PROPERTIES LINK_FLAGS "${JPEGXL_COVERAGE_LINK_FLAGS}")
  endif()
  target_compile_options(${TESTNAME} PRIVATE
    ${JPEGXL_INTERNAL_FLAGS}
    # Add coverage flags to the test binary so code in the private headers of
    # the library is also instrumented when running tests that execute it.
    ${JPEGXL_COVERAGE_FLAGS}
  )
  target_link_libraries(${TESTNAME}
    gmock
    GTest::GTest
    GTest::Main
    jxl_extras-internal
    jxl_testlib-internal
  )
  # Output test targets in the test directory.
  set_target_properties(${TESTNAME} PROPERTIES PREFIX "tests/")
  if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(${TESTNAME} PROPERTIES COMPILE_FLAGS "-Wno-error")
  endif ()
  # 240 seconds because some build types (e.g. coverage) can be quite slow.
  gtest_discover_tests(${TESTNAME} DISCOVERY_TIMEOUT 240)
endforeach ()
