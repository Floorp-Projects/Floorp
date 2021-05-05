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

set(JPEGXL_MAJOR_VERSION 0)
set(JPEGXL_MINOR_VERSION 3)
set(JPEGXL_PATCH_VERSION 7)
set(JPEGXL_LIBRARY_VERSION
    "${JPEGXL_MAJOR_VERSION}.${JPEGXL_MINOR_VERSION}.${JPEGXL_PATCH_VERSION}")

# This the library API compatibility version.
set(JPEGXL_LIBRARY_SOVERSION "${JPEGXL_MAJOR_VERSION}")


# List of warning and feature flags for our library and tests.
if (MSVC)
set(JPEGXL_INTERNAL_FLAGS
  # TODO(janwas): add flags
)
else ()
set(JPEGXL_INTERNAL_FLAGS
  # F_FLAGS
  -fmerge-all-constants
  -fno-builtin-fwrite
  -fno-builtin-fread

  # WARN_FLAGS
  -Wall
  -Wextra
  -Wc++11-compat
  -Warray-bounds
  -Wformat-security
  -Wimplicit-fallthrough
  -Wno-register  # Needed by public headers in lcms
  -Wno-unused-function
  -Wno-unused-parameter
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wvla
)

# Warning flags supported by clang.
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  list(APPEND JPEGXL_INTERNAL_FLAGS
    -Wc++2a-extensions
    -Wdeprecated-increment-bool
    # TODO(deymo): Add -Wextra-semi once we update third_party/highway.
    # -Wextra-semi
    -Wfloat-overflow-conversion
    -Wfloat-zero-conversion
    -Wfor-loop-analysis
    -Wgnu-redeclared-enum
    -Winfinite-recursion
    -Wliteral-conversion
    -Wno-c++98-compat
    -Wno-unused-command-line-argument
    -Wprivate-header
    -Wself-assign
    -Wstring-conversion
    -Wtautological-overlap-compare
    -Wthread-safety-analysis
    -Wundefined-func-template
    -Wunreachable-code
    -Wunused-comparison
  )
endif()  # Clang

if (WIN32)
  list(APPEND JPEGXL_INTERNAL_FLAGS
    -Wno-c++98-compat-pedantic
    -Wno-cast-align
    -Wno-double-promotion
    -Wno-float-equal
    -Wno-format-nonliteral
    -Wno-global-constructors
    -Wno-language-extension-token
    -Wno-missing-prototypes
    -Wno-shadow
    -Wno-shadow-field-in-constructor
    -Wno-sign-conversion
    -Wno-unused-member-function
    -Wno-unused-template
    -Wno-used-but-marked-unused
    -Wno-zero-as-null-pointer-constant
  )
else()  # WIN32
  list(APPEND JPEGXL_INTERNAL_FLAGS
    -fsized-deallocation
    -fno-exceptions

    # Language flags
    -fmath-errno
  )

  if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    list(APPEND JPEGXL_INTERNAL_FLAGS
      -fnew-alignment=8
      -fno-cxx-exceptions
      -fno-slp-vectorize
      -fno-vectorize

      -disable-free
      -disable-llvm-verifier
    )
  endif()  # Clang
endif()  # WIN32

# Internal flags for coverage builds:
if(JPEGXL_ENABLE_COVERAGE)
set(JPEGXL_COVERAGE_FLAGS
    -g -O0 -fprofile-arcs -ftest-coverage -DJXL_DISABLE_SLOW_TESTS
    -DJXL_ENABLE_ASSERT=0 -DJXL_ENABLE_CHECK=0
)
endif()  # JPEGXL_ENABLE_COVERAGE
endif()  #!MSVC

# The jxl library definition.
include(jxl.cmake)

# Other libraries outside the core jxl library.
include(jxl_extras.cmake)
include(jxl_threads.cmake)

# Install all the library headers from the source and the generated ones. There
# is no distinction on which libraries use which header since it is expected
# that all developer libraries are available together at build time.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/jxl
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/jxl
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(BUILD_TESTING)
# Unittests
cmake_policy(SET CMP0057 NEW)  # https://gitlab.kitware.com/cmake/cmake/issues/18198
include(GoogleTest)

# Tests for the jxl library.
include(jxl_tests.cmake)

# Google benchmark for the jxl library
include(jxl_benchmark.cmake)

# Profiler for libjxl
include(jxl_profiler.cmake)

endif()  # BUILD_TESTING
