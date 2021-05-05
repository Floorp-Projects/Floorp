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

# Ubuntu bionic ships with cmake 3.10.
cmake_minimum_required(VERSION 3.10)

# Honor VISIBILITY_INLINES_HIDDEN on all types of targets.
if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()
# Pass CMAKE_EXE_LINKER_FLAGS to CC and CXX compilers when testing if they work.
if(POLICY CMP0065)
  cmake_policy(SET CMP0065 NEW)
endif()

# Set PIE flags for POSITION_INDEPENDENT_CODE targets, added in 3.14.
if(POLICY CMP0083)
  cmake_policy(SET CMP0083 NEW)
endif()

project(JPEGXL LANGUAGES C CXX)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
   "int main() {
      #if !defined(__EMSCRIPTEN__)
      static_assert(false, \"__EMSCRIPTEN__ is not defined\");
      #endif
      return 0;
    }"
  JPEGXL_EMSCRIPTEN
)

message(STATUS "CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}")
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsanitize=fuzzer-no-link" CXX_FUZZERS_SUPPORTED)
check_cxx_compiler_flag("-Xclang -mconstructor-aliases" CXX_CONSTRUCTOR_ALIASES_SUPPORTED)

# Enabled PIE binaries by default if supported.
include(CheckPIESupported OPTIONAL RESULT_VARIABLE CHECK_PIE_SUPPORTED)
if(CHECK_PIE_SUPPORTED)
  check_pie_supported(LANGUAGES CXX)
  if(CMAKE_CXX_LINK_PIE_SUPPORTED)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  endif()
endif()

### Project build options:
if(${CXX_FUZZERS_SUPPORTED})
  # Enabled by default except on arm64, Windows and Apple builds.
  set(ENABLE_FUZZERS_DEFAULT true)
endif()
find_package(PkgConfig)
if(NOT APPLE AND NOT WIN32 AND NOT HAIKU AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
  pkg_check_modules(TCMallocMinimalVersionCheck QUIET IMPORTED_TARGET
      libtcmalloc_minimal)
  if(TCMallocMinimalVersionCheck_FOUND AND
     NOT TCMallocMinimalVersionCheck_VERSION VERSION_EQUAL 2.8.0)
    # Enabled by default except on Windows and Apple builds for
    # tcmalloc != 2.8.0. tcmalloc 2.8.1 already has a fix for this issue.
    set(ENABLE_TCMALLOC_DEFAULT true)
  else()
    message(STATUS
        "tcmalloc version ${TCMallocMinimalVersionCheck_VERSION} -- "
        "tcmalloc 2.8.0 disabled due to "
        "https://github.com/gperftools/gperftools/issues/1204")
  endif()
endif()

set(WARNINGS_AS_ERRORS_DEFAULT false)

set(JPEGXL_ENABLE_FUZZERS ${ENABLE_FUZZERS_DEFAULT} CACHE BOOL
    "Build JPEGXL fuzzer targets.")
set(JPEGXL_ENABLE_DEVTOOLS false CACHE BOOL
    "Build JPEGXL developer tools.")
set(JPEGXL_ENABLE_MANPAGES true CACHE BOOL
    "Build and install man pages for the command-line tools.")
set(JPEGXL_ENABLE_BENCHMARK true CACHE BOOL
    "Build JPEGXL benchmark tools.")
set(JPEGXL_ENABLE_EXAMPLES true CACHE BOOL
    "Build JPEGXL library usage examples.")
set(JPEGXL_ENABLE_SJPEG true CACHE BOOL
    "Build JPEGXL with support for encoding with sjpeg.")
set(JPEGXL_ENABLE_OPENEXR true CACHE BOOL
    "Build JPEGXL with support for OpenEXR if available.")
set(JPEGXL_ENABLE_SKCMS true CACHE BOOL
    "Build with skcms instead of lcms2.")
set(JPEGXL_ENABLE_VIEWERS false CACHE BOOL
    "Build JPEGXL viewer tools for evaluation.")
set(JPEGXL_ENABLE_TCMALLOC ${ENABLE_TCMALLOC_DEFAULT} CACHE BOOL
    "Build JPEGXL using gperftools (tcmalloc) allocator.")
set(JPEGXL_ENABLE_PLUGINS false CACHE BOOL
    "Build third-party plugings to support JPEG XL in other applications.")
set(JPEGXL_ENABLE_COVERAGE false CACHE BOOL
    "Enable code coverage tracking for libjxl. This also enables debug and disables optimizations.")
set(JPEGXL_STATIC false CACHE BOOL
    "Build tools as static binaries.")
set(JPEGXL_WARNINGS_AS_ERRORS ${WARNINGS_AS_ERRORS_DEFAULT} CACHE BOOL
    "Treat warnings as errors during compilation.")
set(JPEGXL_DEP_LICENSE_DIR "" CACHE STRING
    "Directory where to search for system dependencies \"copyright\" files.")
set(JPEGXL_FORCE_NEON false CACHE BOOL
    "Set flags to enable NEON in arm if not enabled by your toolchain.")


# Force system dependencies.
set(JPEGXL_FORCE_SYSTEM_GTEST false CACHE BOOL
    "Force using system installed googletest (gtest/gmock) instead of third_party/googletest source.")
set(JPEGXL_FORCE_SYSTEM_BROTLI false CACHE BOOL
    "Force using system installed brotli instead of third_party/brotli source.")
set(JPEGXL_FORCE_SYSTEM_HWY false CACHE BOOL
    "Force using system installed highway (libhwy-dev) instead of third_party/highway source.")

# Check minimum compiler versions. Older compilers are not supported and fail
# with hard to understand errors.
if (NOT ${CMAKE_C_COMPILER_ID} STREQUAL ${CMAKE_CXX_COMPILER_ID})
  message(FATAL_ERROR "Different C/C++ compilers set: "
          "${CMAKE_C_COMPILER_ID} vs ${CMAKE_CXX_COMPILER_ID}")
endif()
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  # Android NDK's toolchain.cmake fakes the clang version in
  # CMAKE_CXX_COMPILER_VERSION with an incorrect number, so ignore this.
  if (NOT ${CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION} MATCHES "clang"
      AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 6)
    message(FATAL_ERROR
      "Minimum Clang version required is Clang 6, please update.")
  endif()
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
  if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 7)
    message(FATAL_ERROR
      "Minimum GCC version required is 7, please update.")
  endif()
endif()

message(STATUS
    "Compiled IDs C:${CMAKE_C_COMPILER_ID}, C++:${CMAKE_CXX_COMPILER_ID}")

# CMAKE_EXPORT_COMPILE_COMMANDS is used to generate the compilation database
# used by clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(JPEGXL_STATIC)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  set(BUILD_SHARED_LIBS 0)
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
  if (MINGW)
    # In MINGW libstdc++ uses pthreads directly. When building statically a
    # program (regardless of whether the source code uses pthread or not) the
    # toolchain will add stdc++ and pthread to the linking step but stdc++ will
    # be linked statically while pthread will be linked dynamically.
    # To avoid this and have pthread statically linked with need to pass it in
    # the command line with "-Wl,-Bstatic -lpthread -Wl,-Bdynamic" but the
    # linker will discard it if not used by anything else up to that point in
    # the linker command line. If the program or any dependency don't use
    # pthread directly -lpthread is discarded and libstdc++ (added by the
    # toolchain later) will then use the dynamic version. For this we also need
    # to pass -lstdc++ explicitly before -lpthread. For pure C programs -lstdc++
    # will be discarded anyway.
    # This adds these flags as dependencies for *all* targets. Adding this to
    # CMAKE_EXE_LINKER_FLAGS instead would cause them to be included before any
    # object files and therefore discarded.
    link_libraries(-Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic)
  endif()  # MINGW
endif()  # JPEGXL_STATIC

if (MSVC)
# TODO(janwas): add flags
else ()

# Global compiler flags for all targets here and in subdirectories.
add_definitions(
  # Avoid changing the binary based on the current time and date.
  -D__DATE__="redacted"
  -D__TIMESTAMP__="redacted"
  -D__TIME__="redacted"
)

if("${JPEGXL_ENABLE_FUZZERS}" OR "${JPEGXL_ENABLE_COVERAGE}")
  add_definitions(
    -DJXL_ENABLE_FUZZERS
  )
endif()  # JPEGXL_ENABLE_FUZZERS

# In CMake before 3.12 it is problematic to pass repeated flags like -Xclang.
# For this reason we place them in CMAKE_CXX_FLAGS instead.
# See https://gitlab.kitware.com/cmake/cmake/issues/15826

# Machine flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funwind-tables")
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -mrelax-all")
endif()
if ("${CXX_CONSTRUCTOR_ALIASES_SUPPORTED}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -mconstructor-aliases")
endif()

if(WIN32)
# Not supported by clang-cl, but frame pointers are default on Windows
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
endif()

# CPU flags - remove once we have NEON dynamic dispatch

# TODO(janwas): this also matches M1, but only ARMv7 is intended/needed.
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
if(JPEGXL_FORCE_NEON)
# GCC requires these flags, otherwise __ARM_NEON is undefined.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
   -mfpu=neon-vfpv4 -mfloat-abi=hard")
endif()
endif()

# Force build with optimizations in release mode.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

add_compile_options(
  # Ignore this to allow redefining __DATE__ and others.
  -Wno-builtin-macro-redefined

  # Global warning settings.
  -Wall
)

if (JPEGXL_WARNINGS_AS_ERRORS)
add_compile_options(-Werror)
endif ()
endif ()  # !MSVC

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_subdirectory(third_party)

set(THREADS_PREFER_PTHREAD_FLAG YES)
find_package(Threads REQUIRED)

# Copy the JXL license file to the output build directory.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
               ${PROJECT_BINARY_DIR}/LICENSE.jpeg-xl COPYONLY)

# Enable tests regardless of where are they defined.
enable_testing()
include(CTest)

# Libraries.
add_subdirectory(lib)

if(BUILD_TESTING)
# Script to run tests over the source code in bash.
find_program (BASH_PROGRAM bash)
if(BASH_PROGRAM)
  add_test(
    NAME bash_test
    COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/bash_test.sh)
endif()
endif() # BUILD_TESTING

# Documentation generated by Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
set(DOXYGEN_GENERATE_HTML "YES")
set(DOXYGEN_GENERATE_XML "NO")
set(DOXYGEN_STRIP_FROM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "README.md")
set(DOXYGEN_WARN_AS_ERROR "YES")
doxygen_add_docs(doc
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/include/jxl"
  "${CMAKE_CURRENT_SOURCE_DIR}/doc/api.txt"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Generating C API documentation")
else()
# Create a "doc" target for compatibility since "doc" is not otherwise added to
# the build when doxygen is not installed.
add_custom_target(doc false
  COMMENT "Error: Can't generate doc since Doxygen not installed.")
endif() # DOXYGEN_FOUND

if(JPEGXL_ENABLE_MANPAGES)
find_package(Python COMPONENTS Interpreter)
if(Python_Interpreter_FOUND)
  find_program(ASCIIDOC a2x)
endif()
if(NOT Python_Interpreter_FOUND OR "${ASCIIDOC}" STREQUAL "ASCIIDOC-NOTFOUND")
  message(WARNING "asciidoc was not found, the man pages will not be installed.")
else()
  set(MANPAGE_FILES "")
  set(MANPAGES "")
  foreach(PAGE IN ITEMS cjxl djxl)
    # Invoking the Python interpreter ourselves instead of running the a2x binary
    # directly is necessary on MSYS2, otherwise it is run through cmd.exe which
    # does not recognize it.
    add_custom_command(
      OUTPUT "${PAGE}.1"
      COMMAND Python::Interpreter
      ARGS "${ASCIIDOC}"
        --format manpage --destination-dir="${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/${PAGE}.txt"
      MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/doc/man/${PAGE}.txt")
    list(APPEND MANPAGE_FILES "${CMAKE_CURRENT_BINARY_DIR}/${PAGE}.1")
    list(APPEND MANPAGES "${PAGE}.1")
  endforeach()
  add_custom_target(manpages ALL DEPENDS ${MANPAGES})
  install(FILES ${MANPAGE_FILES} DESTINATION share/man/man1)
endif()
endif()

# Example usage code.
if (${JPEGXL_ENABLE_EXAMPLES})
add_subdirectory(examples)
endif ()

# Plugins for third-party software
if (${JPEGXL_ENABLE_PLUGINS})
add_subdirectory(plugins)
endif ()

# Binary tools
add_subdirectory(tools)
