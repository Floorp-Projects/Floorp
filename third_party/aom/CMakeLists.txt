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
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
  endif ()
endif ()

option(ENABLE_CCACHE "Enable ccache support." OFF)
option(ENABLE_DISTCC "Enable distcc support." OFF)
option(ENABLE_DOCS "Enable documentation generation (doxygen required)." ON)
option(ENABLE_NASM "Use nasm instead of yasm for x86 assembly." OFF)

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
include("${AOM_ROOT}/build/cmake/aom_optimization.cmake")
include("${AOM_ROOT}/aom_dsp/aom_dsp.cmake")
include("${AOM_ROOT}/aom_mem/aom_mem.cmake")
include("${AOM_ROOT}/aom_ports/aom_ports.cmake")
include("${AOM_ROOT}/aom_scale/aom_scale.cmake")
include("${AOM_ROOT}/aom_util/aom_util.cmake")
include("${AOM_ROOT}/av1/av1.cmake")
include("${AOM_ROOT}/test/test.cmake")

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

set(AOM_WEBM_DECODER_SOURCES
    "${AOM_ROOT}/webmdec.cc"
    "${AOM_ROOT}/webmdec.h")

set(AOM_WEBM_ENCODER_SOURCES
    "${AOM_ROOT}/webmenc.cc"
    "${AOM_ROOT}/webmenc.h")

include_directories(${AOM_ROOT} ${AOM_CONFIG_DIR})

# Targets
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
add_library(aom_encoder_stats OBJECT ${AOM_ENCODER_STATS_SOURCES})
add_library(aom ${AOM_SOURCES} $<TARGET_OBJECTS:aom_rtcd>)

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

#
# Application and application support targets.
#
add_library(aom_common_app_util OBJECT ${AOM_COMMON_APP_UTIL_SOURCES})


if (CONFIG_AV1_DECODER)
  add_library(aom_decoder_app_util OBJECT ${AOM_DECODER_APP_UTIL_SOURCES})
  add_executable(aomdec
               "${AOM_ROOT}/aomdec.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)

  if (CONFIG_ANALYZER)
    add_executable(analyzer
                   "${AOM_ROOT}/examples/analyzer.cc"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_decoder_app_util>)
    target_link_libraries(analyzer ${AOM_LIB_LINK_TYPE} ${wxWidgets_LIBRARIES})
    set(AOM_APP_TARGETS ${AOM_APP_TARGETS} analyzer)
  endif ()

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

  if (CONFIG_INSPECTION)
    add_executable(inspect
                   "${AOM_ROOT}/examples/inspect.c"
                   $<TARGET_OBJECTS:aom_common_app_util>
                   $<TARGET_OBJECTS:aom_decoder_app_util>)
    set(AOM_DECODER_EXAMPLE_TARGETS ${AOM_DECODER_EXAMPLE_TARGETS} inspect)

    if (EMSCRIPTEN)
      add_preproc_definition(_POSIX_SOURCE)
      add_compiler_flag_if_supported("-s TOTAL_MEMORY=134217728")
      add_compiler_flag_if_supported("-s MODULARIZE=1")
      add_compiler_flag_if_supported("-s EXPORT_NAME=\"\'DecoderModule\'\"")
      add_compiler_flag_if_supported("--memory-init-file 0")
      if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
        # Default to -O3 when no build type specified.
        add_compiler_flag_if_supported("-O3")
      endif ()
      em_link_post_js(inspect "${AOM_ROOT}/tools/inspect-post.js")
    endif ()
  endif ()

  # Maintain lists of example and app targets.
  set(AOM_DECODER_EXAMPLE_TARGETS ${AOM_DECODER_EXAMPLE_TARGETS}
      decode_to_md5 decode_with_drops simple_decoder)
  set(AOM_APP_TARGETS ${AOM_APP_TARGETS} aomdec ${AOM_DECODER_EXAMPLE_TARGETS})
endif ()


if (CONFIG_AV1_ENCODER)
  add_library(aom_encoder_app_util OBJECT ${AOM_ENCODER_APP_UTIL_SOURCES})
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

  # Add encoder apps and examples to target lists.
  set(AOM_ENCODER_EXAMPLE_TARGETS
      lossless_encoder set_maps simple_encoder twopass_encoder)
  set(AOM_APP_TARGETS ${AOM_APP_TARGETS} aomenc ${AOM_ENCODER_EXAMPLE_TARGETS})
endif ()

# Maintain a separate variable listing only the examples to facilitate
# installation of example programs into an examples sub directory of
# $AOM_DIST_DIR/bin when building the dist target.
set(AOM_EXAMPLE_TARGETS
    ${AOM_DECODER_EXAMPLE_TARGETS} ${AOM_ENCODER_EXAMPLE_TARGETS})

if (CONFIG_AV1_DECODER AND CONFIG_AV1_ENCODER)
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

if (CONFIG_LIBYUV)
  add_library(yuv OBJECT ${AOM_LIBYUV_SOURCES})
  if (NOT MSVC)
    target_compile_options(yuv PRIVATE -Wno-unused-parameter)
  endif ()
  include_directories("${AOM_ROOT}/third_party/libyuv/include")

  # Add to existing targets.
  foreach (aom_app ${AOM_APP_TARGETS})
    target_sources(${aom_app} PUBLIC $<TARGET_OBJECTS:yuv>)
    set_property(TARGET ${aom_app} PROPERTY LINKER_LANGUAGE CXX)
  endforeach ()
endif ()

if (CONFIG_WEBM_IO)
  add_library(webm OBJECT ${AOM_LIBWEBM_SOURCES})
  include_directories("${AOM_ROOT}/third_party/libwebm")

  if (NOT MSVC)
    target_compile_options(webm PRIVATE -Wno-shadow)
  endif ()

  # Add to existing targets.
  if (CONFIG_AV1_DECODER)
    target_sources(aom_decoder_app_util PUBLIC ${AOM_WEBM_DECODER_SOURCES})
  endif ()

  if (CONFIG_AV1_ENCODER)
    target_sources(aom_encoder_app_util PUBLIC ${AOM_WEBM_ENCODER_SOURCES})
  endif ()

  foreach (aom_app ${AOM_APP_TARGETS})
    target_sources(${aom_app} PUBLIC $<TARGET_OBJECTS:webm>)
    set_property(TARGET ${aom_app} PROPERTY LINKER_LANGUAGE CXX)
   endforeach ()
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
    set(XCODE_DUMMY_CXX_FILE "${AOM_CONFIG_DIR}/dummy.cc")
    file(WRITE "${XCODE_DUMMY_CXX_FILE}"
         "// Xcode needs a C++ file to link, ignore this file.")
    foreach (aom_app ${AOM_APP_TARGETS})
      target_sources(${aom_app} PUBLIC "${XCODE_DUMMY_CXX_FILE}")
    endforeach ()
  endif ()
endif ()

if ("${CMAKE_GENERATOR}" MATCHES "Makefiles$" )
  # Users of the configure build expect the example targets to be built in the
  # examples sub directory of the configured build directory after running make.
  file(MAKE_DIRECTORY "${AOM_CONFIG_DIR}/examples")
  set_target_properties(${AOM_EXAMPLE_TARGETS} PROPERTIES
                        RUNTIME_OUTPUT_DIRECTORY "${AOM_CONFIG_DIR}/examples")
endif ()

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
  set(AOM_INSTALL_BINS ${AOM_INSTALL_BINS} aomdec)
  set(AOM_INSTALL_INCS
      ${AOM_INSTALL_INCS}
      "${AOM_ROOT}/aom/aom_decoder.h"
      "${AOM_ROOT}/aom/aomdx.h")
endif ()

if (CONFIG_AV1_ENCODER)
  set(AOM_INSTALL_INCS
      ${AOM_INSTALL_INCS}
      "${AOM_ROOT}/aom/aomcx.h"
      "${AOM_ROOT}/aom/aom_encoder.h")
  set(AOM_INSTALL_BINS ${AOM_INSTALL_BINS} aomenc)
endif ()

set(AOM_INSTALL_LIBS aom)

install(FILES ${AOM_INSTALL_INCS}
        DESTINATION "${CMAKE_INSTALL_PREFIX}/include/aom")
install(FILES "${AOM_CONFIG_DIR}/aom.pc"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
install(TARGETS ${AOM_INSTALL_LIBS} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
install(TARGETS ${AOM_INSTALL_BINS} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

# Aomedia dist rule.
if (CONFIG_AV1_DECODER)
  set(AOM_DIST_APPS ${AOM_DIST_APPS} $<TARGET_FILE:aomdec>)
endif ()
if (CONFIG_AV1_ENCODER)
  set(AOM_DIST_APPS ${AOM_DIST_APPS} $<TARGET_FILE:aomenc>)
endif ()

foreach (example ${AOM_EXAMPLE_TARGETS})
  list(APPEND AOM_DIST_EXAMPLES $<TARGET_FILE:${example}>)
endforeach ()

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
                  -DAOM_DIST_INCLUDES="${AOM_INSTALL_INCS}"
                  -DAOM_DIST_LIBS=$<TARGET_FILE:aom>
                  -DENABLE_DOCS=${ENABLE_DOCS}
                  -P "${AOM_ROOT}/build/cmake/dist.cmake"
                  DEPENDS ${AOM_INSTALL_BINS} ${AOM_INSTALL_LIBS}
                  ${AOM_INSTALL_INCS} ${AOM_EXAMPLE_TARGETS})

if (ENABLE_DOCS)
  add_dependencies(dist docs)
endif ()
