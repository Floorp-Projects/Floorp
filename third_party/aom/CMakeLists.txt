##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
cmake_minimum_required(VERSION 3.5)

if (NOT EMSCRIPTEN)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE
      "Build type: Debug, Release, RelWithDebInfo or MinSizeRel" STRING FORCE)
  endif ()
endif ()

option(ENABLE_ADOPTED_EXPERIMENTS "Enable adopted experiments." ON)
option(ENABLE_CCACHE "Enable ccache support." OFF)
option(ENABLE_DISTCC "Enable distcc support." OFF)
option(ENABLE_DOCS "Enable documentation generation (doxygen required)." ON)
option(ENABLE_EXAMPLES "Enables build of example code." ON)
option(ENABLE_GOMA "Enable goma support." OFF)
option(ENABLE_IDE_TEST_HOSTING
       "Enables running tests within IDEs like Visual Studio and Xcode." OFF)
option(ENABLE_NASM "Use nasm instead of yasm for x86 assembly." OFF)
option(ENABLE_TOOLS "Enable applications in tools sub directory." ON)
option(ENABLE_WERROR "Converts warnings to errors at compile time." OFF)

# $BUILD_SHARED_LIBS is a CMake built-in-- it's listed here for visibility.
option(BUILD_SHARED_LIBS "CMake should generate a shared library build." OFF)

project(AOM C CXX)

set(AOM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(AOM_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

if ("${AOM_ROOT}" STREQUAL "${AOM_CONFIG_DIR}")
  message(FATAL_ERROR
          "Building from within the aom source tree is not supported.\n"
          "Hint: mkdir -p ../aom_build && cd ../aom_build\n"
          "Run cmake from there.")
endif ()

include("${AOM_ROOT}/build/cmake/aom_configure.cmake")
include("${AOM_ROOT}/aom_dsp/aom_dsp.cmake")
include("${AOM_ROOT}/aom_mem/aom_mem.cmake")
include("${AOM_ROOT}/aom_ports/aom_ports.cmake")
include("${AOM_ROOT}/aom_scale/aom_scale.cmake")
include("${AOM_ROOT}/aom_util/aom_util.cmake")
include("${AOM_ROOT}/av1/av1.cmake")
include("${AOM_ROOT}/test/test.cmake")
include("${AOM_ROOT}/build/cmake/sanitizers.cmake")
include("${AOM_ROOT}/build/cmake/util.cmake")

set(AOM_RTCD_SOURCES
    "${AOM_CONFIG_DIR}/aom_dsp_rtcd.h"
    "${AOM_CONFIG_DIR}/aom_scale_rtcd.h"
    "${AOM_CONFIG_DIR}/av1_rtcd.h"
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
    "${AOM_ROOT}/av1/common/av1_rtcd_defs.pl"
    "${AOM_ROOT}/av1/common/av1_rtcd.c"
    "${AOM_ROOT}/build/make/rtcd.pl")

# TODO(tomfinegan): Use libwebm's cmake support directly.
set(AOM_LIBWEBM_SOURCES
    "${AOM_ROOT}/third_party/libwebm/common/hdr_util.cc"
    "${AOM_ROOT}/third_party/libwebm/common/hdr_util.h"
    "${AOM_ROOT}/third_party/libwebm/common/webmids.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxer.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxer.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxertypes.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxerutil.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxerutil.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvwriter.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvwriter.h"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvparser.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvparser.h"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvreader.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvreader.h")

set(AOM_LIBYUV_SOURCES
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/basic_types.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/convert.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/convert_argb.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/convert_from.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/cpu_id.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/planar_functions.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/rotate.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/row.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/scale.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/scale_row.h"
    "${AOM_ROOT}/third_party/libyuv/source/cpu_id.cc"
    "${AOM_ROOT}/third_party/libyuv/source/planar_functions.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_any.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_common.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_gcc.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_mips.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_neon.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_neon64.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_win.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_any.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_common.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_gcc.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_mips.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_neon.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_neon64.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_win.cc")

set(AOM_SOURCES
    "${AOM_CONFIG_DIR}/aom_config.c"
    "${AOM_CONFIG_DIR}/aom_config.h"
    "${AOM_ROOT}/aom/aom.h"
    "${AOM_ROOT}/aom/aom_codec.h"
    "${AOM_ROOT}/aom/aom_decoder.h"
    "${AOM_ROOT}/aom/aom_encoder.h"
    "${AOM_ROOT}/aom/aom_frame_buffer.h"
    "${AOM_ROOT}/aom/aom_image.h"
    "${AOM_ROOT}/aom/aom_integer.h"
    "${AOM_ROOT}/aom/aomcx.h"
    "${AOM_ROOT}/aom/aomdx.h"
    "${AOM_ROOT}/aom/internal/aom_codec_internal.h"
    "${AOM_ROOT}/aom/src/aom_codec.c"
    "${AOM_ROOT}/aom/src/aom_decoder.c"
    "${AOM_ROOT}/aom/src/aom_encoder.c"
    "${AOM_ROOT}/aom/src/aom_image.c")

set(AOM_COMMON_APP_UTIL_SOURCES
    "${AOM_ROOT}/args.c"
    "${AOM_ROOT}/args.h"
    "${AOM_ROOT}/md5_utils.c"
    "${AOM_ROOT}/md5_utils.h"
    "${AOM_ROOT}/tools_common.c"
    "${AOM_ROOT}/tools_common.h"
    "${AOM_ROOT}/video_common.h"
    "${AOM_ROOT}/y4menc.c"
    "${AOM_ROOT}/y4menc.h")

set(AOM_DECODER_APP_UTIL_SOURCES
    "${AOM_ROOT}/ivfdec.c"
    "${AOM_ROOT}/ivfdec.h"
    "${AOM_ROOT}/video_reader.c"
    "${AOM_ROOT}/video_reader.h")

set(AOM_ENCODER_APP_UTIL_SOURCES
    "${AOM_ROOT}/ivfenc.c"
    "${AOM_ROOT}/ivfenc.h"
    "${AOM_ROOT}/video_writer.c"
    "${AOM_ROOT}/video_writer.h"
    "${AOM_ROOT}/warnings.c"
    "${AOM_ROOT}/warnings.h"
    "${AOM_ROOT}/y4minput.c"
    "${AOM_ROOT}/y4minput.h"
    "${AOM_ROOT}/examples/encoder_util.h"
    "${AOM_ROOT}/examples/encoder_util.c")

set(AOM_ENCODER_STATS_SOURCES
    "${AOM_ROOT}/aomstats.c"
    "${AOM_ROOT}/aomstats.h"
    "${AOM_ROOT}/rate_hist.c"
    "${AOM_ROOT}/rate_hist.h")

set(AOM_PKG_CONFIG_SOURCES "${AOM_CONFIG_DIR}/aom.pc")

set(AOM_VERSION_SOURCES "${AOM_CONFIG_DIR}/aom_version.h")

set(AOM_WEBM_DECODER_SOURCES
    "${AOM_ROOT}/webmdec.cc"
    "${AOM_ROOT}/webmdec.h")

set(AOM_WEBM_ENCODER_SOURCES
    "${AOM_ROOT}/webmenc.cc"
    "${AOM_ROOT}/webmenc.h")

include_directories(${AOM_ROOT} ${AOM_CONFIG_DIR})

# Targets
add_library(aom_version ${AOM_VERSION_SOURCES})
add_dummy_source_file_to_target(aom_version c)
add_custom_command(
  OUTPUT "${AOM_CONFIG_DIR}/aom_version.h"
  COMMAND ${CMAKE_COMMAND}
  ARGS -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
    -DAOM_ROOT=${AOM_ROOT}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DPERL_EXECUTABLE=${PERL_EXECUTABLE}
    -P "${AOM_ROOT}/build/cmake/version.cmake"
  COMMENT "Writing aom_version.h"
  VERBATIM)

add_custom_target(aom_version_check
  COMMAND ${CMAKE_COMMAND}
    -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
    -DAOM_ROOT=${AOM_ROOT}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DPERL_EXECUTABLE=${PERL_EXECUTABLE}
    -P "${AOM_ROOT}/build/cmake/version.cmake"
  COMMENT "Updating version info if necessary."
  VERBATIM)
add_dependencies(aom_version aom_version_check)

if (NOT MSVC)
  add_library(aom_pc ${AOM_PKG_CONFIG_SOURCES})
  add_dummy_source_file_to_target(aom_pc c)
  add_custom_command(
    OUTPUT "${AOM_CONFIG_DIR}/aom.pc"
    COMMAND ${CMAKE_COMMAND}
    ARGS -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
      -DAOM_ROOT=${AOM_ROOT}
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_PROJECT_NAME=${CMAKE_PROJECT_NAME}
      -DCONFIG_MULTITHREAD=${CONFIG_MULTITHREAD}
      -DHAVE_PTHREAD_H=${HAVE_PTHREAD_H}
      -P "${AOM_ROOT}/build/cmake/pkg_config.cmake"
      COMMENT "Writing aom.pc"
      VERBATIM)
  add_dependencies(aom_pc aom_version)
endif ()

# TODO(tomfinegan): Move rtcd target setup where it belongs for each rtcd
# source.
add_rtcd_build_step("${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
                    "${AOM_CONFIG_DIR}/aom_dsp_rtcd.h"
                    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
                    "aom_dsp_rtcd")
add_rtcd_build_step("${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
                    "${AOM_CONFIG_DIR}/aom_scale_rtcd.h"
                    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
                    "aom_scale_rtcd")
add_rtcd_build_step("${AOM_ROOT}/av1/common/av1_rtcd_defs.pl"
                    "${AOM_CONFIG_DIR}/av1_rtcd.h"
                    "${AOM_ROOT}/av1/common/av1_rtcd.c"
                    "av1_rtcd")

add_library(aom_rtcd OBJECT ${AOM_RTCD_SOURCES})
add_dependencies(aom_rtcd aom_version)

add_library(aom_encoder_stats OBJECT ${AOM_ENCODER_STATS_SOURCES})
add_library(aom ${AOM_SOURCES} $<TARGET_OBJECTS:aom_rtcd>)

if (NOT MSVC AND NOT APPLE)
  target_link_libraries(aom ${AOM_LIB_LINK_TYPE} m)
endif ()

# List of object and static library targets.
set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_rtcd aom_encoder_stats aom_mem
    aom_scale aom)

# Setup dependencies.
setup_aom_dsp_targets()
setup_aom_mem_targets()
setup_aom_ports_targets()
setup_aom_util_targets()
setup_aom_scale_targets()
setup_av1_targets()

# Make all library targets depend on aom_rtcd to make sure it builds first.
foreach (aom_lib ${AOM_LIB_TARGETS})
  if (NOT "${aom_lib}" STREQUAL "aom_rtcd")
    add_dependencies(${aom_lib} aom_rtcd)
  endif ()
endforeach ()

# Generate a stub file containing the C function usage_exit(). Users of the
# aom_common_app_util library must define this function. This is a convenience
# to allow omission of the function from applications that might want to use
# other pieces of the util support without defining the usage_exit().
file(WRITE "${AOM_CONFIG_DIR}/usage_exit.c" "void usage_exit(void) {}")

#
# Application and application support targets.
#
if (CONFIG_UNIT_TESTS OR ENABLE_EXAMPLES OR ENABLE_TOOLS)
  add_library(aom_common_app_util OBJECT ${AOM_COMMON_APP_UTIL_SOURCES})
  if (CONFIG_AV1_DECODER)
    add_library(aom_decoder_app_util OBJECT ${AOM_DECODER_APP_UTIL_SOURCES})
  endif ()
  if (CONFIG_AV1_ENCODER)
    add_library(aom_encoder_app_util OBJECT ${AOM_ENCODER_APP_UTIL_SOURCES})
  endif ()
endif ()

if (CONFIG_AV1_DECODER AND ENABLE_EXAMPLES)
  add_executable(aomdec
                 "${AOM_ROOT}/aomdec.c"
                 $<TARGET_OBJECTS:aom_common_app_util>
                 $<TARGET_OBJECTS:aom_decoder_app_util>)
  add_executable(decode_to_md5
                 "${AOM_ROOT}/examples/decode_to_md5.c"
                 $<TARGET_OBJECTS:aom_common_app_util>
                 $<TARGET_OBJECTS:aom_decoder_app_util>)
  add_executable(decode_with_drops
                 "${AOM_ROOT}/examples/decode_with_drops.c"
                 $<TARGET_OBJECTS:aom_common_app_util>
                 $<TARGET_OBJECTS:aom_decoder_app_util>)
  add_executable(simple_decoder
                 "${AOM_ROOT}/examples/simple_decoder.c"
                 $<TARGET_OBJECTS:aom_common_app_util>
                 $<TARGET_OBJECTS:aom_decoder_app_util>)

  if (CONFIG_ANALYZER)
    add_executable(analyzer
                   "${AOM_ROOT}/examples/analyzer.cc"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_decoder_app_util>)
    target_link_libraries(analyzer
                          ${AOM_LIB_LINK_TYPE} ${wxWidgets_LIBRARIES})
    set(AOM_APP_TARGETS ${AOM_APP_TARGETS} analyzer)
    set(AOM_DECODER_EXAMPLE_TARGETS ${AOM_DECODER_EXAMPLE_TARGETS} analyzer)
  endif ()

  if (CONFIG_INSPECTION)
    add_executable(inspect
                   "${AOM_ROOT}/examples/inspect.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_decoder_app_util>)
    set(AOM_DECODER_EXAMPLE_TARGETS ${AOM_DECODER_EXAMPLE_TARGETS} inspect)

    if (EMSCRIPTEN)
      add_preproc_definition(_POSIX_SOURCE)
      append_link_flag_to_target("inspect" "-s TOTAL_MEMORY=134217728")
      append_link_flag_to_target("inspect" "-s MODULARIZE=1")
      append_link_flag_to_target("inspect"
                                 "-s EXPORT_NAME=\"\'DecoderModule\'\"")
      append_link_flag_to_target("inspect" "--memory-init-file 0")

      if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
        # Default to -O3 when no build type specified.
        append_compiler_flag("-O3")
      endif ()

      em_link_post_js(inspect "${AOM_ROOT}/tools/inspect-post.js")
    endif ()
  endif ()

  # Maintain a list of decoder example targets.
  set(AOM_DECODER_EXAMPLE_TARGETS ${AOM_DECODER_EXAMPLE_TARGETS}
      aomdec decode_to_md5 decode_with_drops simple_decoder)

  # Add decoder examples to the app targets list.
  set(AOM_APP_TARGETS ${AOM_APP_TARGETS} ${AOM_DECODER_EXAMPLE_TARGETS})
endif ()

if (CONFIG_AV1_ENCODER)
  if (ENABLE_EXAMPLES)
    add_executable(aomenc
                   "${AOM_ROOT}/aomenc.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_encoder_app_util>
                   $<TARGET_OBJECTS:aom_encoder_stats>)
    add_executable(lossless_encoder
                   "${AOM_ROOT}/examples/lossless_encoder.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_encoder_app_util>)
    add_executable(set_maps
                   "${AOM_ROOT}/examples/set_maps.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_encoder_app_util>)
    add_executable(simple_encoder
                   "${AOM_ROOT}/examples/simple_encoder.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_encoder_app_util>)
    add_executable(twopass_encoder
                   "${AOM_ROOT}/examples/twopass_encoder.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_encoder_app_util>)

    # Maintain a list of encoder example targets.
    set(AOM_ENCODER_EXAMPLE_TARGETS
        aomenc lossless_encoder set_maps simple_encoder twopass_encoder)

    # Add encoder examples to app target list.
    set(AOM_APP_TARGETS ${AOM_APP_TARGETS} ${AOM_ENCODER_EXAMPLE_TARGETS})
  endif ()

  if (ENABLE_TOOLS AND CONFIG_ENTROPY_STATS)
    # TODO(tomfinegan): Sort out why a simple link command with
    # aom_entropy_optimizer.c won't work on macos, but dragging in all the
    # helper machinery allows the link to succeed.
    add_executable(aom_entropy_optimizer
                   "${AOM_CONFIG_DIR}/usage_exit.c"
                   "${AOM_ROOT}/tools/aom_entropy_optimizer.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_encoder_app_util>)

    # Maintain a list of encoder tool targets.
    set(AOM_ENCODER_TOOL_TARGETS
        ${AOM_ENCODER_TOOL_TARGETS} aom_entropy_optimizer)

      # Add encoder tools to app target list.
    set(AOM_APP_TARGETS ${AOM_APP_TARGETS} ${AOM_ENCODER_TOOL_TARGETS})
  endif ()
endif ()

if (ENABLE_EXAMPLES)
  # Maintain a separate variable listing only the examples to facilitate
  # installation of example programs into an examples sub directory of
  # $AOM_DIST_DIR/bin when building the dist target.
  set(AOM_EXAMPLE_TARGETS
      ${AOM_DECODER_EXAMPLE_TARGETS} ${AOM_ENCODER_EXAMPLE_TARGETS})
endif ()

if (ENABLE_TOOLS)
  # Maintain a separate variable listing only the examples to facilitate
  # installation of example programs into an tools sub directory of
  # $AOM_DIST_DIR/bin when building the dist target.
  set(AOM_TOOL_TARGETS ${AOM_DECODER_TOOL_TARGETS} ${AOM_ENCODER_TOOL_TARGETS})
endif ()

if (ENABLE_EXAMPLES AND CONFIG_AV1_DECODER AND CONFIG_AV1_ENCODER)
  add_executable(aom_cx_set_ref
                 "${AOM_ROOT}/examples/aom_cx_set_ref.c"
                 $<TARGET_OBJECTS:aom_common_app_util>
                 $<TARGET_OBJECTS:aom_encoder_app_util>)
  set(AOM_EXAMPLE_TARGETS ${AOM_EXAMPLE_TARGETS} aom_cx_set_ref)
  set(AOM_APP_TARGETS ${AOM_APP_TARGETS} aom_cx_set_ref)
endif ()

foreach (aom_app ${AOM_APP_TARGETS})
  target_link_libraries(${aom_app} ${AOM_LIB_LINK_TYPE} aom)
endforeach ()

if (CONFIG_UNIT_TESTS OR ENABLE_EXAMPLES OR ENABLE_TOOLS)
  if (CONFIG_LIBYUV)
    add_library(yuv OBJECT ${AOM_LIBYUV_SOURCES})
    if (NOT MSVC)
      target_compile_options(yuv PRIVATE -Wno-unused-parameter)
    endif ()
    include_directories("${AOM_ROOT}/third_party/libyuv/include")

    # Add to existing targets.
    foreach (aom_app ${AOM_APP_TARGETS})
      target_sources(${aom_app} PRIVATE $<TARGET_OBJECTS:yuv>)
      set_property(TARGET ${aom_app} PROPERTY LINKER_LANGUAGE CXX)
    endforeach ()
  endif ()

  if (CONFIG_WEBM_IO)
    add_library(webm OBJECT ${AOM_LIBWEBM_SOURCES})
    include_directories("${AOM_ROOT}/third_party/libwebm")
    target_compile_definitions(webm PRIVATE __STDC_CONSTANT_MACROS)
    target_compile_definitions(webm PRIVATE __STDC_LIMIT_MACROS)

    if (NOT MSVC)
      target_compile_options(webm PRIVATE -Wno-shadow)
    endif ()

    # Add to existing targets.
    if (CONFIG_AV1_DECODER)
      target_sources(aom_decoder_app_util PRIVATE ${AOM_WEBM_DECODER_SOURCES})
    endif ()

    if (CONFIG_AV1_ENCODER)
      target_sources(aom_encoder_app_util PRIVATE ${AOM_WEBM_ENCODER_SOURCES})
    endif ()

    foreach (aom_app ${AOM_APP_TARGETS})
      target_sources(${aom_app} PRIVATE $<TARGET_OBJECTS:webm>)
      set_property(TARGET ${aom_app} PROPERTY LINKER_LANGUAGE CXX)
     endforeach ()
  endif ()
endif ()

if (CONFIG_UNIT_TESTS)
  # Create test_libaom target and the targets it depends on.
  setup_aom_test_targets()
endif ()

if (HAVE_PTHREAD_H AND CONFIG_MULTITHREAD)
  find_package(Threads)
  foreach (app_target ${AOM_APP_TARGETS})
    target_link_libraries(${app_target} ${AOM_LIB_LINK_TYPE} Threads::Threads)
  endforeach ()
endif ()

if (XCODE)
  if (CONFIG_LIBYUV OR CONFIG_WEBM_IO)
    # The Xcode generator does not obey LINKER_LANGUAGE. Because of the issue
    # what looks like a C++ file needs to be in any target that Xcode will link
    # when the target contains a C++ dependency.
    # Without this Xcode will try to link with the C linker, which always ends
    # badly when a dependency actually includes C++.
    # Note: LINKER_LANGUAGE is explicitly set to C++ for all targets touched
    # here, it really is the Xcode generator's fault, or just a deficiency in
    # Xcode itself.
    foreach (aom_app ${AOM_APP_TARGETS})
      add_dummy_source_file_to_target("${aom_app}" "cc")
    endforeach ()
  endif ()
endif ()

if (ENABLE_EXAMPLES AND "${CMAKE_GENERATOR}" MATCHES "Makefiles$")
  # Users of the configure build expect the example targets to be built in the
  # examples sub directory of the configured build directory after running make.
  file(MAKE_DIRECTORY "${AOM_CONFIG_DIR}/examples")

  foreach (target ${AOM_EXAMPLE_TARGETS})
    if (NOT "${target}" MATCHES "aomdec\|aomenc")
      set_target_properties(${target} PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY
                            "${AOM_CONFIG_DIR}/examples")
    endif ()
  endforeach ()

  if (ENABLE_TOOLS AND AOM_TOOL_TARGETS)
    # The same expectation is true for tool targets.
    file(MAKE_DIRECTORY "${AOM_CONFIG_DIR}/tools")
    set_target_properties(${AOM_TOOL_TARGETS} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${AOM_CONFIG_DIR}/tools")
  endif ()
endif ()

if (BUILD_SHARED_LIBS)
  include("${AOM_ROOT}/build/cmake/exports.cmake")
  setup_exports_target()
  set_target_properties(aom PROPERTIES SOVERSION 0)
endif ()

# Handle user supplied compile and link flags last to ensure they're obeyed.
set_user_flags()

# Aomedia documentation rule.
if (ENABLE_DOCS)
  include(FindDoxygen)
  if (DOXYGEN_FOUND)
    include("${AOM_ROOT}/docs.cmake")
    setup_documentation_targets()
  else ()
    message("--- Cannot find doxygen, ENABLE_DOCS turned off.")
    set(ENABLE_DOCS OFF)
  endif ()
endif ()

# Aomedia install rule.
set(AOM_INSTALL_INCS
    "${AOM_ROOT}/aom/aom.h"
    "${AOM_ROOT}/aom/aom_codec.h"
    "${AOM_ROOT}/aom/aom_frame_buffer.h"
    "${AOM_ROOT}/aom/aom_image.h"
    "${AOM_ROOT}/aom/aom_integer.h"
    "${AOM_ROOT}/aom/aom.h")

if (CONFIG_AV1_DECODER)
  if (ENABLE_EXAMPLES)
    set(AOM_INSTALL_BINS ${AOM_INSTALL_BINS} aomdec)
  endif ()

  set(AOM_INSTALL_INCS
      ${AOM_INSTALL_INCS}
      "${AOM_ROOT}/aom/aom_decoder.h"
      "${AOM_ROOT}/aom/aomdx.h")
endif ()

if (CONFIG_AV1_ENCODER)
  if (ENABLE_EXAMPLES)
    set(AOM_INSTALL_BINS ${AOM_INSTALL_BINS} aomenc)
  endif ()

  set(AOM_INSTALL_INCS
      ${AOM_INSTALL_INCS}
      "${AOM_ROOT}/aom/aomcx.h"
      "${AOM_ROOT}/aom/aom_encoder.h")
endif ()

set(AOM_INSTALL_LIBS aom)

install(FILES ${AOM_INSTALL_INCS}
        DESTINATION "${CMAKE_INSTALL_PREFIX}/include/aom")
install(FILES "${AOM_CONFIG_DIR}/aom.pc"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
install(TARGETS ${AOM_INSTALL_LIBS} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")

if (ENABLE_EXAMPLES)
  install(TARGETS ${AOM_INSTALL_BINS} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif ()

# Aomedia dist rule.
if (CONFIG_AV1_DECODER AND ENABLE_EXAMPLES)
  set(AOM_DIST_APPS ${AOM_DIST_APPS} $<TARGET_FILE:aomdec>)
endif ()
if (CONFIG_AV1_ENCODER AND ENABLE_EXAMPLES)
  set(AOM_DIST_APPS ${AOM_DIST_APPS} $<TARGET_FILE:aomenc>)
endif ()

if (ENABLE_EXAMPLES)
  foreach (example ${AOM_EXAMPLE_TARGETS})
    list(APPEND AOM_DIST_EXAMPLES $<TARGET_FILE:${example}>)
  endforeach ()
endif ()

if (ENABLE_TOOLS)
  foreach (tool ${AOM_TOOL_TARGETS})
    list(APPEND AOM_DIST_TOOLS $<TARGET_FILE:${tool}>)
  endforeach ()
endif ()

if (NOT AOM_DIST_DIR)
  set(AOM_DIST_DIR "${AOM_CONFIG_DIR}/dist")
endif ()

add_custom_target(dist
                  COMMAND ${CMAKE_COMMAND}
                  -DAOM_ROOT=${AOM_ROOT}
                  -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
                  -DAOM_DIST_DIR=${AOM_DIST_DIR}
                  -DAOM_DIST_APPS="${AOM_DIST_APPS}"
                  -DAOM_DIST_EXAMPLES="${AOM_DIST_EXAMPLES}"
                  -DAOM_DIST_TOOLS="${AOM_DIST_TOOLS}"
                  -DAOM_DIST_INCLUDES="${AOM_INSTALL_INCS}"
                  -DAOM_DIST_LIBS=$<TARGET_FILE:aom>
                  -DENABLE_DOCS=${ENABLE_DOCS}
                  -P "${AOM_ROOT}/build/cmake/dist.cmake"
                  DEPENDS ${AOM_INSTALL_BINS} ${AOM_INSTALL_LIBS}
                  ${AOM_INSTALL_INCS} ${AOM_EXAMPLE_TARGETS}
                  ${AOM_TOOL_TARGETS})

if (ENABLE_DOCS)
  add_dependencies(dist docs)
endif ()
