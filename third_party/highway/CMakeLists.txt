# Copyright 2019 Google LLC
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

cmake_minimum_required(VERSION 3.10)

# Set PIE flags for POSITION_INDEPENDENT_CODE targets, added in 3.14.
if(POLICY CMP0083)
  cmake_policy(SET CMP0083 NEW)
endif()

project(hwy VERSION 0.16.0)  # Keep in sync with highway.h version

# Directly define the ABI version from the cmake project() version values:
set(LIBRARY_VERSION "${hwy_VERSION}")
set(LIBRARY_SOVERSION ${hwy_VERSION_MAJOR})

set(CMAKE_CXX_EXTENSIONS OFF)

# Enabled PIE binaries by default if supported.
include(CheckPIESupported OPTIONAL RESULT_VARIABLE CHECK_PIE_SUPPORTED)
if(CHECK_PIE_SUPPORTED)
  check_pie_supported(LANGUAGES CXX)
  if(CMAKE_CXX_LINK_PIE_SUPPORTED)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  endif()
endif()

include(GNUInstallDirs)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(HWY_CMAKE_ARM7 OFF CACHE BOOL "Set copts for ARMv7 with NEON (requires vfpv4)?")

# Unconditionally adding -Werror risks breaking the build when new warnings
# arise due to compiler/platform changes. Enable this in CI/tests.
set(HWY_WARNINGS_ARE_ERRORS OFF CACHE BOOL "Add -Werror flag?")

set(HWY_ENABLE_EXAMPLES ON CACHE BOOL "Build examples")
set(HWY_ENABLE_INSTALL ON CACHE BOOL "Install library")

include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
   "int main() {
      #if !defined(__EMSCRIPTEN__)
      static_assert(false, \"__EMSCRIPTEN__ is not defined\");
      #endif
      return 0;
    }"
  HWY_EMSCRIPTEN
)

set(HWY_CONTRIB_SOURCES
    hwy/contrib/dot/dot-inl.h
    hwy/contrib/image/image.cc
    hwy/contrib/image/image.h
    hwy/contrib/math/math-inl.h
    hwy/contrib/sort/disabled_targets.h
    hwy/contrib/sort/shared-inl.h
    hwy/contrib/sort/sorting_networks-inl.h
    hwy/contrib/sort/traits-inl.h
    hwy/contrib/sort/traits128-inl.h
    hwy/contrib/sort/vqsort-inl.h
    hwy/contrib/sort/vqsort.cc
    hwy/contrib/sort/vqsort.h
    hwy/contrib/sort/vqsort_128a.cc
    hwy/contrib/sort/vqsort_128d.cc
    hwy/contrib/sort/vqsort_f32a.cc
    hwy/contrib/sort/vqsort_f32d.cc
    hwy/contrib/sort/vqsort_f64a.cc
    hwy/contrib/sort/vqsort_f64d.cc
    hwy/contrib/sort/vqsort_i16a.cc
    hwy/contrib/sort/vqsort_i16d.cc
    hwy/contrib/sort/vqsort_i32a.cc
    hwy/contrib/sort/vqsort_i32d.cc
    hwy/contrib/sort/vqsort_i64a.cc
    hwy/contrib/sort/vqsort_i64d.cc
    hwy/contrib/sort/vqsort_u16a.cc
    hwy/contrib/sort/vqsort_u16d.cc
    hwy/contrib/sort/vqsort_u32a.cc
    hwy/contrib/sort/vqsort_u32d.cc
    hwy/contrib/sort/vqsort_u64a.cc
    hwy/contrib/sort/vqsort_u64d.cc
)

set(HWY_SOURCES
    hwy/aligned_allocator.cc
    hwy/aligned_allocator.h
    hwy/base.h
    hwy/cache_control.h
    hwy/detect_compiler_arch.h  # private
    hwy/detect_targets.h  # private
    hwy/foreach_target.h
    hwy/highway.h
    hwy/highway_export.h
    hwy/nanobenchmark.cc
    hwy/nanobenchmark.h
    hwy/ops/arm_neon-inl.h
    hwy/ops/arm_sve-inl.h
    hwy/ops/generic_ops-inl.h
    hwy/ops/scalar-inl.h
    hwy/ops/set_macros-inl.h
    hwy/ops/shared-inl.h
    hwy/ops/wasm_128-inl.h
    hwy/ops/x86_128-inl.h
    hwy/ops/x86_256-inl.h
    hwy/ops/x86_512-inl.h
    hwy/targets.cc
    hwy/targets.h
)

set(HWY_TEST_SOURCES
    hwy/tests/hwy_gtest.h
    hwy/tests/test_util-inl.h
    hwy/tests/test_util.cc
    hwy/tests/test_util.h
)

if (MSVC)
  # TODO(janwas): add flags
else()
  set(HWY_FLAGS
    # Avoid changing binaries based on the current time and date.
    -Wno-builtin-macro-redefined
    -D__DATE__="redacted"
    -D__TIMESTAMP__="redacted"
    -D__TIME__="redacted"

    # Optimizations
    -fmerge-all-constants

    # Warnings
    -Wall
    -Wextra
    # These are not included in Wall nor Wextra:
    -Wconversion
    -Wsign-conversion
    -Wvla
    -Wnon-virtual-dtor
  )

  if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    list(APPEND HWY_FLAGS
      -Wfloat-overflow-conversion
      -Wfloat-zero-conversion
      -Wfor-loop-analysis
      -Wgnu-redeclared-enum
      -Winfinite-recursion
      -Wself-assign
      -Wstring-conversion
      -Wtautological-overlap-compare
      -Wthread-safety-analysis
      -Wundefined-func-template

      -fno-cxx-exceptions
      -fno-slp-vectorize
      -fno-vectorize

      # Use color in messages
      -fdiagnostics-show-option -fcolor-diagnostics
    )
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
      list(APPEND HWY_FLAGS -Wc++2a-extensions)
    endif()
  endif()

  if (WIN32)
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
      list(APPEND HWY_FLAGS
        -Wno-global-constructors
        -Wno-language-extension-token
        -Wno-used-but-marked-unused
        -Wno-shadow-field-in-constructor
        -Wno-unused-member-function
        -Wno-unused-template
        -Wno-c++98-compat-pedantic
        -Wno-used-but-marked-unused
        -Wno-zero-as-null-pointer-constant
      )
    endif()

    list(APPEND HWY_FLAGS
      -Wno-cast-align
      -Wno-double-promotion
      -Wno-float-equal
      -Wno-format-nonliteral
      -Wno-shadow
      -Wno-sign-conversion
    )
  else()
    list(APPEND HWY_FLAGS
      -fmath-errno
      -fno-exceptions
    )
  endif()  # WIN32

  if (HWY_CMAKE_ARM7)
    list(APPEND HWY_FLAGS
      -march=armv7-a
      -mfpu=neon-vfpv4
      -mfloat-abi=hard  # must match the toolchain specified as CXX=
      -mfp16-format=ieee  # required for vcvt_f32_f16
    )
  endif()  # HWY_CMAKE_ARM7

  if (HWY_WARNINGS_ARE_ERRORS)
    list(APPEND HWY_FLAGS -Werror)
  endif()

endif()  # !MSVC

# By default prefer STATIC build (legacy behavior)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(HWY_FORCE_STATIC_LIBS "Ignore BUILD_SHARED_LIBS" OFF)
# only expose shared/static options to advanced users:
mark_as_advanced(BUILD_SHARED_LIBS)
mark_as_advanced(HWY_FORCE_STATIC_LIBS)
# Define visibility settings globally:
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Copy-cat "add_library" logic + add override.
set(HWY_LIBRARY_TYPE "SHARED")
if (NOT BUILD_SHARED_LIBS OR HWY_FORCE_STATIC_LIBS)
  set(HWY_LIBRARY_TYPE "STATIC")
endif()

# This preprocessor define will drive the build, also used in the *.pc files:
if("${HWY_LIBRARY_TYPE}" STREQUAL "SHARED")
  set(DLLEXPORT_TO_DEFINE "HWY_SHARED_DEFINE")
else()
  set(DLLEXPORT_TO_DEFINE "HWY_STATIC_DEFINE")
endif()

add_library(hwy ${HWY_LIBRARY_TYPE} ${HWY_SOURCES})
target_compile_definitions(hwy PUBLIC "${DLLEXPORT_TO_DEFINE}")
target_compile_options(hwy PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(hwy PUBLIC cxx_std_11)
set_target_properties(hwy PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version)
# not supported by MSVC/Clang, safe to skip (we use DLLEXPORT annotations)
if(UNIX AND NOT APPLE)
  set_property(TARGET hwy APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version")
endif()

add_library(hwy_contrib ${HWY_LIBRARY_TYPE} ${HWY_CONTRIB_SOURCES})
target_link_libraries(hwy_contrib hwy)
target_compile_options(hwy_contrib PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy_contrib PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy_contrib PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy_contrib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(hwy_contrib PUBLIC cxx_std_11)

add_library(hwy_test ${HWY_LIBRARY_TYPE} ${HWY_TEST_SOURCES})
target_link_libraries(hwy_test hwy)
target_compile_options(hwy_test PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy_test PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy_test PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy_test PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(hwy_test PUBLIC cxx_std_11)

# -------------------------------------------------------- hwy_list_targets
# Generate a tool to print the compiled-in targets as defined by the current
# flags. This tool will print to stderr at build time, after building hwy.
add_executable(hwy_list_targets hwy/tests/list_targets.cc)
target_compile_options(hwy_list_targets PRIVATE ${HWY_FLAGS})
target_link_libraries(hwy_list_targets hwy)
target_include_directories(hwy_list_targets PRIVATE
  $<TARGET_PROPERTY:hwy,INCLUDE_DIRECTORIES>)
# TARGET_FILE always returns the path to executable
# Naked target also not always could be run (due to the lack of '.\' prefix)
# Thus effective command to run should contain the full path
# and emulator prefix (if any).
if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
add_custom_command(TARGET hwy_list_targets POST_BUILD
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:hwy_list_targets> || (exit 0))
endif()

# --------------------------------------------------------
# Allow skipping the following sections for projects that do not need them:
# tests, examples, benchmarks and installation.

# -------------------------------------------------------- install library
if (HWY_ENABLE_INSTALL)

install(TARGETS hwy
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()

install(TARGETS hwy_contrib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_CONTRIB_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()

install(TARGETS hwy_test
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_TEST_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()

# Add a pkg-config file for libhwy and the contrib/test libraries.
set(HWY_LIBRARY_VERSION "${CMAKE_PROJECT_VERSION}")
foreach (pc libhwy.pc libhwy-contrib.pc libhwy-test.pc)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${pc}.in" "${pc}" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${pc}"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endforeach()

endif() # HWY_ENABLE_INSTALL
# -------------------------------------------------------- Examples
if (HWY_ENABLE_EXAMPLES)

# Avoids mismatch between GTest's static CRT and our dynamic.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Programming exercise with integrated benchmark
add_executable(hwy_benchmark hwy/examples/benchmark.cc)
target_sources(hwy_benchmark PRIVATE
    hwy/nanobenchmark.h)
# Try adding either -DHWY_COMPILE_ONLY_SCALAR or -DHWY_COMPILE_ONLY_STATIC to
# observe the difference in targets printed.
target_compile_options(hwy_benchmark PRIVATE ${HWY_FLAGS})
target_link_libraries(hwy_benchmark hwy)
set_target_properties(hwy_benchmark
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "examples/")

endif() # HWY_ENABLE_EXAMPLES
# -------------------------------------------------------- Tests

include(CTest)

if(BUILD_TESTING)
enable_testing()
include(GoogleTest)

set(HWY_SYSTEM_GTEST OFF CACHE BOOL "Use pre-installed googletest?")
if(HWY_SYSTEM_GTEST)
find_package(GTest REQUIRED)
else()
# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()
endif() # HWY_SYSTEM_GTEST

set(HWY_TEST_FILES
  hwy/contrib/dot/dot_test.cc
  hwy/contrib/image/image_test.cc
  # Disabled due to SIGILL in clang7 debug build during gtest discovery phase,
  # not reproducible locally. Still tested via bazel build.
  # hwy/contrib/math/math_test.cc
  hwy/contrib/sort/sort_test.cc
  hwy/aligned_allocator_test.cc
  hwy/base_test.cc
  hwy/highway_test.cc
  hwy/targets_test.cc
  hwy/examples/skeleton_test.cc
  hwy/tests/arithmetic_test.cc
  hwy/tests/blockwise_test.cc
  hwy/tests/combine_test.cc
  hwy/tests/compare_test.cc
  hwy/tests/convert_test.cc
  hwy/tests/crypto_test.cc
  hwy/tests/demote_test.cc
  hwy/tests/logical_test.cc
  hwy/tests/mask_test.cc
  hwy/tests/memory_test.cc
  hwy/tests/shift_test.cc
  hwy/tests/swizzle_test.cc
  hwy/tests/test_util_test.cc
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests)
foreach (TESTFILE IN LISTS HWY_TEST_FILES)
  # The TESTNAME is the name without the extension or directory.
  get_filename_component(TESTNAME ${TESTFILE} NAME_WE)
  add_executable(${TESTNAME} ${TESTFILE})
  target_compile_options(${TESTNAME} PRIVATE ${HWY_FLAGS})
  # Test all targets, not just the best/baseline. This changes the default
  # policy to all-attainable; note that setting -DHWY_COMPILE_* directly can
  # cause compile errors because only one may be set, and other CMakeLists.txt
  # that include us may set them.
  target_compile_options(${TESTNAME} PRIVATE -DHWY_IS_TEST=1)

  if(HWY_SYSTEM_GTEST)
    target_link_libraries(${TESTNAME} hwy hwy_contrib hwy_test GTest::GTest GTest::Main)
  else()
    target_link_libraries(${TESTNAME} hwy hwy_contrib hwy_test gtest gtest_main)
  endif()
  # Output test targets in the test directory.
  set_target_properties(${TESTNAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "tests")

  if (HWY_EMSCRIPTEN)
    set_target_properties(${TESTNAME} PROPERTIES LINK_FLAGS "-s SINGLE_FILE=1")
  endif()

  if(${CMAKE_VERSION} VERSION_LESS "3.10.3")
    gtest_discover_tests(${TESTNAME} TIMEOUT 60)
  else ()
    gtest_discover_tests(${TESTNAME} DISCOVERY_TIMEOUT 60)
  endif ()
endforeach ()

# The skeleton test uses the skeleton library code.
target_sources(skeleton_test PRIVATE hwy/examples/skeleton.cc)

endif() # BUILD_TESTING
