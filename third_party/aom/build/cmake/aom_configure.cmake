#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_AOM_CONFIGURE_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_AOM_CONFIGURE_CMAKE_
set(AOM_BUILD_CMAKE_AOM_CONFIGURE_CMAKE_ 1)

include(FindGit)
include(FindPerl)
include(FindThreads)

set(AOM_SUPPORTED_CPU_TARGETS
    "arm64 armv7 armv7s generic mips32 mips64 ppc x86 x86_64")

# Generate the user config settings. This must occur before include of
# aom_config_defaults.cmake (because it turns every config variable into a cache
# variable with its own help string).
get_cmake_property(cmake_cache_vars CACHE_VARIABLES)
foreach(cache_var ${cmake_cache_vars})
  get_property(cache_var_helpstring CACHE ${cache_var} PROPERTY HELPSTRING)
  set(cmdline_helpstring "No help, variable specified on the command line.")
  if("${cache_var_helpstring}" STREQUAL "${cmdline_helpstring}")
    set(AOM_CMAKE_CONFIG "${AOM_CMAKE_CONFIG} -D${cache_var}=${${cache_var}}")
  endif()
endforeach()
string(STRIP "${AOM_CMAKE_CONFIG}" AOM_CMAKE_CONFIG)

include("${AOM_ROOT}/build/cmake/aom_config_defaults.cmake")
include("${AOM_ROOT}/build/cmake/aom_experiment_deps.cmake")
include("${AOM_ROOT}/build/cmake/aom_optimization.cmake")
include("${AOM_ROOT}/build/cmake/compiler_flags.cmake")
include("${AOM_ROOT}/build/cmake/compiler_tests.cmake")
include("${AOM_ROOT}/build/cmake/util.cmake")

# Detect target CPU.
if(NOT AOM_TARGET_CPU)
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64" OR
     "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
      set(AOM_TARGET_CPU "x86")
    elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
      set(AOM_TARGET_CPU "x86_64")
    else()
      message(FATAL_ERROR
                "--- Unexpected pointer size (${CMAKE_SIZEOF_VOID_P}) for\n"
                "      CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}\n"
                "      CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}\n"
                "      CMAKE_GENERATOR=${CMAKE_GENERATOR}\n")
    endif()
  elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i386" OR
         "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86")
    set(AOM_TARGET_CPU "x86")
  elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^arm" OR
         "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^mips")
    set(AOM_TARGET_CPU "${CMAKE_SYSTEM_PROCESSOR}")
  elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
    set(AOM_TARGET_CPU "arm64")
  elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^ppc")
    set(AOM_TARGET_CPU "ppc")
  else()
    message(WARNING "The architecture ${CMAKE_SYSTEM_PROCESSOR} is not "
                    "supported, falling back to the generic target")
    set(AOM_TARGET_CPU "generic")
  endif()
endif()

if(CMAKE_TOOLCHAIN_FILE) # Add toolchain file to config string.
  set(toolchain_string "-DCMAKE_TOOLCHAIN_FILE=\\\"${CMAKE_TOOLCHAIN_FILE}\\\"")
  set(AOM_CMAKE_CONFIG "${toolchain_string} ${AOM_CMAKE_CONFIG}")
else()

  # Add detected CPU to the config string.
  set(AOM_CMAKE_CONFIG "-DAOM_TARGET_CPU=${AOM_TARGET_CPU} ${AOM_CMAKE_CONFIG}")
endif()
set(AOM_CMAKE_CONFIG "-G \\\"${CMAKE_GENERATOR}\\\" ${AOM_CMAKE_CONFIG}")
string(STRIP "${AOM_CMAKE_CONFIG}" AOM_CMAKE_CONFIG)

message("--- aom_configure: Detected CPU: ${AOM_TARGET_CPU}")
set(AOM_TARGET_SYSTEM ${CMAKE_SYSTEM_NAME})

if("${CMAKE_BUILD_TYPE}" MATCHES "Deb")
  set(CONFIG_DEBUG 1)
endif()

if(NOT MSVC)
  if(BUILD_SHARED_LIBS)
    set(CONFIG_PIC 1)
    set(CONFIG_SHARED 1)
    set(CONFIG_STATIC 0)
  endif()

  if(CONFIG_PIC)

    # TODO(tomfinegan): clang needs -pie in CMAKE_EXE_LINKER_FLAGS for this to
    # work.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Linux" AND "${AOM_TARGET_CPU}" MATCHES
       "^armv7")
      set(AOM_AS_FLAGS ${AOM_AS_FLAGS} --defsym PIC=1)
    else()
      set(AOM_AS_FLAGS ${AOM_AS_FLAGS} -DPIC)
    endif()
  endif()
else()
  set(CONFIG_MSVS 1)
endif()

if(NOT "${AOM_SUPPORTED_CPU_TARGETS}" MATCHES "${AOM_TARGET_CPU}")
  message(FATAL_ERROR
            "No RTCD support for ${AOM_TARGET_CPU}. Create it, or "
            "add -DAOM_TARGET_CPU=generic to your cmake command line for a "
            "generic build of libaom and tools.")
endif()

if("${AOM_TARGET_CPU}" STREQUAL "x86" OR "${AOM_TARGET_CPU}" STREQUAL "x86_64")
  find_program(AS_EXECUTABLE yasm $ENV{YASM_PATH})
  if(NOT AS_EXECUTABLE OR ENABLE_NASM)
    unset(AS_EXECUTABLE CACHE)
    find_program(AS_EXECUTABLE nasm $ENV{NASM_PATH})
    if(AS_EXECUTABLE)
      test_nasm()
    endif()
  endif()

  if(NOT AS_EXECUTABLE)
    message(FATAL_ERROR
              "Unable to find assembler. Install 'yasm' or 'nasm.' "
              "To build without optimizations, add -DAOM_TARGET_CPU=generic to "
              "your cmake command line.")
  endif()
  get_asm_obj_format("objformat")
  set(AOM_AS_FLAGS -f ${objformat} ${AOM_AS_FLAGS})
  string(STRIP "${AOM_AS_FLAGS}" AOM_AS_FLAGS)
elseif("${AOM_TARGET_CPU}" MATCHES "arm")
  if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
    set(AS_EXECUTABLE as)
    set(AOM_AS_FLAGS -arch ${AOM_TARGET_CPU} -isysroot ${CMAKE_OSX_SYSROOT})
  elseif("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
    if(NOT AS_EXECUTABLE)
      set(AS_EXECUTABLE as)
    endif()
  elseif("${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
    if(NOT AS_EXECUTABLE)
      set(AS_EXECUTABLE ${CMAKE_C_COMPILER} -c -mimplicit-it=always)
    endif()
  endif()
  if(NOT AS_EXECUTABLE)
    message(FATAL_ERROR
              "Unknown assembler for: ${AOM_TARGET_CPU}-${AOM_TARGET_SYSTEM}")
  endif()

  string(STRIP "${AOM_AS_FLAGS}" AOM_AS_FLAGS)
endif()

if(CONFIG_ANALYZER)
  include(FindwxWidgets)
  find_package(wxWidgets REQUIRED adv base core)
  include(${wxWidgets_USE_FILE})
endif()

if(NOT MSVC AND CMAKE_C_COMPILER_ID MATCHES "GNU\|Clang")
  set(CONFIG_GCC 1)
endif()

if(CONFIG_GCOV)
  message("--- Testing for CONFIG_GCOV support.")
  require_linker_flag("-fprofile-arcs -ftest-coverage")
  require_compiler_flag("-fprofile-arcs -ftest-coverage" YES)
endif()

if(CONFIG_GPROF)
  message("--- Testing for CONFIG_GPROF support.")
  require_compiler_flag("-pg" YES)
endif()

if("${AOM_TARGET_SYSTEM}" MATCHES "Darwin\|Linux\|Windows")
  set(CONFIG_OS_SUPPORT 1)
endif()

#
# Fix CONFIG_* dependencies. This must be done before including cpu.cmake to
# ensure RTCD_CONFIG_* are properly set.
fix_experiment_configs()

# Test compiler support.
aom_get_inline("INLINE")

# Don't just check for pthread.h, but use the result of the full pthreads
# including a linking check in FindThreads above.
set(HAVE_PTHREAD_H ${CMAKE_USE_PTHREADS_INIT})
aom_check_source_compiles("unistd_check" "#include <unistd.h>" HAVE_UNISTD_H)

if(NOT MSVC)
  aom_push_var(CMAKE_REQUIRED_LIBRARIES "m")
  aom_check_c_compiles(
    "fenv_check"
    "#define _GNU_SOURCE
                        #include <fenv.h>
                        void unused(void) {
                          (void)unused;
                          (void)feenableexcept(FE_DIVBYZERO | FE_INVALID);
                        }"
    HAVE_FEXCEPT)
  aom_pop_var(CMAKE_REQUIRED_LIBRARIES)
endif()

include("${AOM_ROOT}/build/cmake/cpu.cmake")

if(ENABLE_CCACHE)
  set_compiler_launcher(ENABLE_CCACHE ccache)
endif()

if(ENABLE_DISTCC)
  set_compiler_launcher(ENABLE_DISTCC distcc)
endif()

if(ENABLE_GOMA)
  set_compiler_launcher(ENABLE_GOMA gomacc)
endif()

if(NOT CONFIG_AV1_DECODER AND NOT CONFIG_AV1_ENCODER)
  message(FATAL_ERROR "Decoder and encoder disabled, nothing to build.")
endif()

if(DECODE_HEIGHT_LIMIT OR DECODE_WIDTH_LIMIT)
  change_config_and_warn(CONFIG_SIZE_LIMIT 1
                         "DECODE_HEIGHT_LIMIT and DECODE_WIDTH_LIMIT")
endif()

if(CONFIG_SIZE_LIMIT)
  if(NOT DECODE_HEIGHT_LIMIT OR NOT DECODE_WIDTH_LIMIT)
    message(FATAL_ERROR "When setting CONFIG_SIZE_LIMIT, DECODE_HEIGHT_LIMIT "
                        "and DECODE_WIDTH_LIMIT must be set.")
  endif()
endif()

# Test compiler flags.
if(MSVC)
  add_compiler_flag_if_supported("/W3")

  # Disable MSVC warnings that suggest making code non-portable.
  add_compiler_flag_if_supported("/wd4996")
  if(ENABLE_WERROR)
    add_compiler_flag_if_supported("/WX")
  endif()
else()
  require_c_flag("-std=c99" YES)
  add_compiler_flag_if_supported("-Wall")
  add_compiler_flag_if_supported("-Wdisabled-optimization")
  add_compiler_flag_if_supported("-Wextra")
  add_compiler_flag_if_supported("-Wfloat-conversion")
  add_compiler_flag_if_supported("-Wimplicit-function-declaration")
  add_compiler_flag_if_supported("-Wlogical-op")
  add_compiler_flag_if_supported("-Wpointer-arith")
  add_compiler_flag_if_supported("-Wsign-compare")
  add_compiler_flag_if_supported("-Wstack-usage=360000")
  add_compiler_flag_if_supported("-Wstring-conversion")
  add_compiler_flag_if_supported("-Wtype-limits")
  add_compiler_flag_if_supported("-Wuninitialized")
  add_compiler_flag_if_supported("-Wunused")
  add_compiler_flag_if_supported("-Wvla")

  # TODO(jzern): this could be added as a cxx flags for test/*.cc only, avoiding
  # third_party.
  add_c_flag_if_supported("-Wshorten-64-to-32")

  # Add -Wshadow only for C files to avoid massive gtest warning spam.
  add_c_flag_if_supported("-Wshadow")

  # Add -Wundef only for C files to avoid massive gtest warning spam.
  add_c_flag_if_supported("-Wundef")

  if(ENABLE_WERROR)
    add_compiler_flag_if_supported("-Werror")
  endif()

  if("${CMAKE_BUILD_TYPE}" MATCHES "Rel")
    add_compiler_flag_if_supported("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")
  endif()
  add_compiler_flag_if_supported("-D_LARGEFILE_SOURCE")
  add_compiler_flag_if_supported("-D_FILE_OFFSET_BITS=64")
endif()

set(AOM_LIB_LINK_TYPE PUBLIC)
if(EMSCRIPTEN)

  # Avoid CMake generation time errors resulting from collisions with the form
  # of target_link_libraries() used by Emscripten.cmake.
  unset(AOM_LIB_LINK_TYPE)
endif()

# Generate aom_config templates.
set(aom_config_asm_template "${AOM_CONFIG_DIR}/config/aom_config.asm.cmake")
set(aom_config_h_template "${AOM_CONFIG_DIR}/config/aom_config.h.cmake")
execute_process(COMMAND
                  ${CMAKE_COMMAND} -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
                  -DAOM_ROOT=${AOM_ROOT} -P
                  "${AOM_ROOT}/build/cmake/generate_aom_config_templates.cmake")

# Generate aom_config.{asm,h}.
configure_file("${aom_config_asm_template}"
               "${AOM_CONFIG_DIR}/config/aom_config.asm")
configure_file("${aom_config_h_template}"
               "${AOM_CONFIG_DIR}/config/aom_config.h")

# Read the current git hash.
find_package(Git)
if(NOT GIT_FOUND)
  message("--- Git missing, version will be read from CHANGELOG.")
endif()

configure_file("${AOM_ROOT}/build/cmake/aom_config.c.template"
               "${AOM_CONFIG_DIR}/config/aom_config.c")

# Find Perl and generate the RTCD sources.
find_package(Perl)
if(NOT PERL_FOUND)
  message(FATAL_ERROR "Perl is required to build libaom.")
endif()

set(AOM_RTCD_CONFIG_FILE_LIST "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
    "${AOM_ROOT}/av1/common/av1_rtcd_defs.pl")
set(AOM_RTCD_HEADER_FILE_LIST "${AOM_CONFIG_DIR}/config/aom_dsp_rtcd.h"
    "${AOM_CONFIG_DIR}/config/aom_scale_rtcd.h"
    "${AOM_CONFIG_DIR}/config/av1_rtcd.h")
set(AOM_RTCD_SOURCE_FILE_LIST "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
    "${AOM_ROOT}/av1/common/av1_rtcd.c")
set(AOM_RTCD_SYMBOL_LIST aom_dsp_rtcd aom_scale_rtcd av1_rtcd)
list(LENGTH AOM_RTCD_SYMBOL_LIST AOM_RTCD_CUSTOM_COMMAND_COUNT)
math(EXPR AOM_RTCD_CUSTOM_COMMAND_COUNT "${AOM_RTCD_CUSTOM_COMMAND_COUNT} - 1")

foreach(NUM RANGE ${AOM_RTCD_CUSTOM_COMMAND_COUNT})
  list(GET AOM_RTCD_CONFIG_FILE_LIST ${NUM} AOM_RTCD_CONFIG_FILE)
  list(GET AOM_RTCD_HEADER_FILE_LIST ${NUM} AOM_RTCD_HEADER_FILE)
  list(GET AOM_RTCD_SOURCE_FILE_LIST ${NUM} AOM_RTCD_SOURCE_FILE)
  list(GET AOM_RTCD_SYMBOL_LIST ${NUM} AOM_RTCD_SYMBOL)
  execute_process(COMMAND ${PERL_EXECUTABLE} "${AOM_ROOT}/build/make/rtcd.pl"
                          --arch=${AOM_TARGET_CPU}
                          --sym=${AOM_RTCD_SYMBOL} ${AOM_RTCD_FLAGS}
                          --config=${AOM_CONFIG_DIR}/config/aom_config.h
                          ${AOM_RTCD_CONFIG_FILE}
                  OUTPUT_FILE ${AOM_RTCD_HEADER_FILE})
endforeach()

# Generate aom_version.h.
execute_process(COMMAND ${CMAKE_COMMAND} -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
                        -DAOM_ROOT=${AOM_ROOT}
                        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                        -DPERL_EXECUTABLE=${PERL_EXECUTABLE} -P
                        "${AOM_ROOT}/build/cmake/version.cmake")

if(NOT MSVC) # Generate aom.pc (pkg-config file).
  execute_process(COMMAND ${CMAKE_COMMAND} -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
                          -DAOM_ROOT=${AOM_ROOT}
                          -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                          -DCMAKE_PROJECT_NAME=${CMAKE_PROJECT_NAME}
                          -DCONFIG_MULTITHREAD=${CONFIG_MULTITHREAD}
                          -DHAVE_PTHREAD_H=${HAVE_PTHREAD_H} -P
                          "${AOM_ROOT}/build/cmake/pkg_config.cmake")
endif()
