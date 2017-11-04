##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
if (NOT AOM_TEST_TEST_CMAKE_)
set(AOM_TEST_TEST_CMAKE_ 1)

include(FindPythonInterp)
include(ProcessorCount)

include("${AOM_ROOT}/test/test_data_util.cmake")

set(AOM_UNIT_TEST_DATA_LIST_FILE "${AOM_ROOT}/test/test-data.sha1")

set(AOM_UNIT_TEST_WRAPPER_SOURCES
    "${AOM_CONFIG_DIR}/usage_exit.c"
    "${AOM_ROOT}/test/test_libaom.cc")

set(AOM_UNIT_TEST_COMMON_SOURCES
    "${AOM_ROOT}/test/acm_random.h"
    "${AOM_ROOT}/test/clear_system_state.h"
    "${AOM_ROOT}/test/codec_factory.h"
    "${AOM_ROOT}/test/decode_test_driver.cc"
    "${AOM_ROOT}/test/decode_test_driver.h"
    "${AOM_ROOT}/test/function_equivalence_test.h"
    "${AOM_ROOT}/test/md5_helper.h"
    "${AOM_ROOT}/test/register_state_check.h"
    "${AOM_ROOT}/test/transform_test_base.h"
    "${AOM_ROOT}/test/util.h"
    "${AOM_ROOT}/test/video_source.h")

if (NOT BUILD_SHARED_LIBS)
  set(AOM_UNIT_TEST_COMMON_SOURCES
      ${AOM_UNIT_TEST_COMMON_SOURCES}
      "${AOM_ROOT}/test/convolve_test.cc"
      "${AOM_ROOT}/test/simd_impl.h")

  if (HAVE_NEON)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/simd_neon_test.cc")
  endif ()
  if (HAVE_SSE2)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/simd_sse2_test.cc")
  endif ()
  if (HAVE_SSSE3)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/simd_ssse3_test.cc")
  endif ()
  if (HAVE_SSE4)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/simd_sse4_test.cc")
  endif ()
  if (HAVE_AVX2)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/simd_avx2_test.cc")
  endif ()

  if (CONFIG_ACCOUNTING)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/accounting_test.cc")
  endif ()

  if (CONFIG_ADAPT_SCAN)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/scan_test.cc")
  endif ()

  if (CONFIG_GLOBAL_MOTION OR CONFIG_WARPED_MOTION)
    if (HAVE_SSE2)
      set(AOM_UNIT_TEST_COMMON_SOURCES
          ${AOM_UNIT_TEST_COMMON_SOURCES}
          "${AOM_ROOT}/test/warp_filter_test.cc"
          "${AOM_ROOT}/test/warp_filter_test_util.cc"
          "${AOM_ROOT}/test/warp_filter_test_util.h")
    endif ()
  endif ()
endif ()

set(AOM_UNIT_TEST_DECODER_SOURCES
    "${AOM_ROOT}/test/decode_api_test.cc"
    "${AOM_ROOT}/test/ivf_video_source.h")

set(AOM_UNIT_TEST_ENCODER_SOURCES
    "${AOM_ROOT}/test/altref_test.cc"
    "${AOM_ROOT}/test/aq_segment_test.cc"
    "${AOM_ROOT}/test/datarate_test.cc"
    "${AOM_ROOT}/test/encode_api_test.cc"
    "${AOM_ROOT}/test/encode_test_driver.cc"
    "${AOM_ROOT}/test/encode_test_driver.h"
    "${AOM_ROOT}/test/error_resilience_test.cc"
    "${AOM_ROOT}/test/i420_video_source.h"
    "${AOM_ROOT}/test/resize_test.cc"
    "${AOM_ROOT}/test/y4m_test.cc"
    "${AOM_ROOT}/test/y4m_video_source.h"
    "${AOM_ROOT}/test/yuv_video_source.h")

if (NOT BUILD_SHARED_LIBS)
  set(AOM_UNIT_TEST_ENCODER_SOURCES
      ${AOM_UNIT_TEST_ENCODER_SOURCES}
      "${AOM_ROOT}/test/dct16x16_test.cc"
      "${AOM_ROOT}/test/dct32x32_test.cc"
      "${AOM_ROOT}/test/sad_test.cc")
endif ()

set(AOM_DECODE_PERF_TEST_SOURCES "${AOM_ROOT}/test/decode_perf_test.cc")
set(AOM_ENCODE_PERF_TEST_SOURCES "${AOM_ROOT}/test/encode_perf_test.cc")
set(AOM_UNIT_TEST_WEBM_SOURCES "${AOM_ROOT}/test/webm_video_source.h")

set(AOM_TEST_INTRA_PRED_SPEED_SOURCES
    "${AOM_CONFIG_DIR}/usage_exit.c"
    "${AOM_ROOT}/test/test_intra_pred_speed.cc")

if (NOT BUILD_SHARED_LIBS)
  if (CONFIG_AV1_DECODER OR CONFIG_AV1_ENCODER)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/av1_convolve_optimz_test.cc"
        "${AOM_ROOT}/test/av1_convolve_test.cc"
        "${AOM_ROOT}/test/av1_txfm_test.cc"
        "${AOM_ROOT}/test/av1_txfm_test.h"
        "${AOM_ROOT}/test/intrapred_test.cc"
        "${AOM_ROOT}/test/lpf_8_test.cc"
        "${AOM_ROOT}/test/simd_cmp_impl.h")

    set(AOM_UNIT_TEST_ENCODER_SOURCES
        ${AOM_UNIT_TEST_ENCODER_SOURCES}
        "${AOM_ROOT}/test/motion_vector_test.cc")

    if (CONFIG_CDEF)
      if (CONFIG_CDEF_SINGLEPASS)
        set(AOM_UNIT_TEST_COMMON_SOURCES
            ${AOM_UNIT_TEST_COMMON_SOURCES}
            "${AOM_ROOT}/test/cdef_test.cc")
      else ()
        set(AOM_UNIT_TEST_COMMON_SOURCES
            ${AOM_UNIT_TEST_COMMON_SOURCES}
            "${AOM_ROOT}/test/clpf_test.cc"
            "${AOM_ROOT}/test/dering_test.cc")
      endif ()
    endif ()

    # Omit 4-tap filter intra predictor test-- currently a 3-tap filter is in
    # use.
    #if (CONFIG_FILTER_INTRA)
    #  if (HAVE_SSE4_1)
    #    set(AOM_UNIT_TEST_COMMON_SOURCES
    #        ${AOM_UNIT_TEST_COMMON_SOURCES}
    #        "${AOM_ROOT}/test/filterintra_predictors_test.cc")
    #  endif ()
    #endif ()

    if (CONFIG_INTRABC)
        set(AOM_UNIT_TEST_COMMON_SOURCES
            ${AOM_UNIT_TEST_COMMON_SOURCES}
            "${AOM_ROOT}/test/intrabc_test.cc")
    endif ()

    if (CONFIG_LOOP_RESTORATION)
      set(AOM_UNIT_TEST_COMMON_SOURCES
          ${AOM_UNIT_TEST_COMMON_SOURCES}
          "${AOM_ROOT}/test/selfguided_filter_test.cc")

      if (HAVE_SSE2)
        set(AOM_UNIT_TEST_COMMON_SOURCES
            ${AOM_UNIT_TEST_COMMON_SOURCES}
            "${AOM_ROOT}/test/hiprec_convolve_test.cc"
            "${AOM_ROOT}/test/hiprec_convolve_test_util.cc"
            "${AOM_ROOT}/test/hiprec_convolve_test_util.h")
      endif ()
    endif ()

    set(AOM_UNIT_TEST_COMMON_INTRIN_NEON
        ${AOM_UNIT_TEST_COMMON_INTRIN_NEON}
        "${AOM_ROOT}/test/simd_cmp_neon.cc")
    set(AOM_UNIT_TEST_COMMON_INTRIN_SSE2
        ${AOM_UNIT_TEST_COMMON_INTRIN_SSE2}
        "${AOM_ROOT}/test/simd_cmp_sse2.cc")
    set(AOM_UNIT_TEST_COMMON_INTRIN_SSSE3
        ${AOM_UNIT_TEST_COMMON_INTRIN_SSSE3}
        "${AOM_ROOT}/test/simd_cmp_ssse3.cc")
    set(AOM_UNIT_TEST_COMMON_INTRIN_SSE4_1
        ${AOM_UNIT_TEST_COMMON_INTRIN_SSE4_1}
        "${AOM_ROOT}/test/simd_cmp_sse4.cc")
    set(AOM_UNIT_TEST_COMMON_INTRIN_AVX2
        ${AOM_UNIT_TEST_COMMON_INTRIN_AVX2}
        "${AOM_ROOT}/test/simd_cmp_avx2.cc")
  endif ()
endif ()

if (CONFIG_AV1_ENCODER)
  set(AOM_UNIT_TEST_ENCODER_SOURCES
      ${AOM_UNIT_TEST_ENCODER_SOURCES}
      "${AOM_ROOT}/test/active_map_test.cc"
      "${AOM_ROOT}/test/borders_test.cc"
      "${AOM_ROOT}/test/cpu_speed_test.cc"
      "${AOM_ROOT}/test/end_to_end_test.cc"
      "${AOM_ROOT}/test/frame_size_tests.cc"
      "${AOM_ROOT}/test/lossless_test.cc")

  if (NOT BUILD_SHARED_LIBS)
    set(AOM_UNIT_TEST_ENCODER_SOURCES
        ${AOM_UNIT_TEST_ENCODER_SOURCES}
        "${AOM_ROOT}/test/arf_freq_test.cc"
        "${AOM_ROOT}/test/av1_dct_test.cc"
        "${AOM_ROOT}/test/av1_fht16x16_test.cc"
        "${AOM_ROOT}/test/av1_fht32x32_test.cc"
        "${AOM_ROOT}/test/av1_fht8x8_test.cc"
        "${AOM_ROOT}/test/av1_fwd_txfm1d_test.cc"
        "${AOM_ROOT}/test/av1_fwd_txfm2d_test.cc"
        "${AOM_ROOT}/test/av1_inv_txfm1d_test.cc"
        "${AOM_ROOT}/test/av1_inv_txfm2d_test.cc"
        "${AOM_ROOT}/test/av1_inv_txfm_test.cc"
        "${AOM_ROOT}/test/av1_wedge_utils_test.cc"
        "${AOM_ROOT}/test/avg_test.cc"
        "${AOM_ROOT}/test/blend_a64_mask_1d_test.cc"
        "${AOM_ROOT}/test/blend_a64_mask_test.cc"
        "${AOM_ROOT}/test/error_block_test.cc"
        "${AOM_ROOT}/test/fdct4x4_test.cc"
        "${AOM_ROOT}/test/fdct8x8_test.cc"
        "${AOM_ROOT}/test/hadamard_test.cc"
        "${AOM_ROOT}/test/masked_sad_test.cc"
        "${AOM_ROOT}/test/masked_variance_test.cc"
        "${AOM_ROOT}/test/minmax_test.cc"
        "${AOM_ROOT}/test/subtract_test.cc"
        "${AOM_ROOT}/test/sum_squares_test.cc"
        "${AOM_ROOT}/test/variance_test.cc")

    if (NOT CONFIG_AOM_QM AND NOT CONFIG_NEW_QUANT)
      set(AOM_UNIT_TEST_ENCODER_SOURCES
          ${AOM_UNIT_TEST_ENCODER_SOURCES}
          "${AOM_ROOT}/test/quantize_func_test.cc")
    endif ()

    if (CONFIG_CONVOLVE_ROUND)
      set(AOM_UNIT_TEST_ENCODER_SOURCES
          ${AOM_UNIT_TEST_ENCODER_SOURCES}
          "${AOM_ROOT}/test/convolve_round_test.cc")
      if (HAVE_SSE2)
        set(AOM_UNIT_TEST_ENCODER_SOURCES
            ${AOM_UNIT_TEST_ENCODER_SOURCES}
            "${AOM_ROOT}/test/av1_convolve_2d_test.cc"
            "${AOM_ROOT}/test/av1_convolve_2d_test_util.cc"
            "${AOM_ROOT}/test/av1_convolve_2d_test_util.h")
      endif ()
      if (NOT CONFIG_COMPOUND_ROUND)
        if (HAVE_SSE4_1)
          set(AOM_UNIT_TEST_ENCODER_SOURCES
              ${AOM_UNIT_TEST_ENCODER_SOURCES}
              "${AOM_ROOT}/test/av1_convolve_scale_test.cc")
        endif ()
      endif ()
    endif ()

    if (CONFIG_EXT_TX)
      set(AOM_UNIT_TEST_ENCODER_SOURCES
          ${AOM_UNIT_TEST_ENCODER_SOURCES}
          "${AOM_ROOT}/test/av1_fht16x32_test.cc"
          "${AOM_ROOT}/test/av1_fht16x8_test.cc"
          "${AOM_ROOT}/test/av1_fht32x16_test.cc"
          "${AOM_ROOT}/test/av1_fht4x4_test.cc"
          "${AOM_ROOT}/test/av1_fht4x8_test.cc"
          "${AOM_ROOT}/test/av1_fht8x16_test.cc"
          "${AOM_ROOT}/test/av1_fht8x4_test.cc")
    endif ()

    if (CONFIG_GLOBAL_MOTION)
      set(AOM_UNIT_TEST_ENCODER_INTRIN_SSE4_1
          ${AOM_UNIT_TEST_ENCODER_INTRIN_SSE4_1}
          "${AOM_ROOT}/test/corner_match_test.cc")
    endif ()

    if (CONFIG_MOTION_VAR)
      set(AOM_UNIT_TEST_ENCODER_SOURCES
          ${AOM_UNIT_TEST_ENCODER_SOURCES}
          "${AOM_ROOT}/test/obmc_sad_test.cc"
          "${AOM_ROOT}/test/obmc_variance_test.cc")
    endif ()

    if (CONFIG_TX64X64)
      set(AOM_UNIT_TEST_ENCODER_SOURCES
          ${AOM_UNIT_TEST_ENCODER_SOURCES}
          "${AOM_ROOT}/test/av1_fht64x64_test.cc")
    endif ()
  endif ()
endif ()

if (NOT BUILD_SHARED_LIBS)
  if (CONFIG_AV1_DECODER AND CONFIG_AV1_ENCODER)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/divu_small_test.cc"
        "${AOM_ROOT}/test/ethread_test.cc"
        "${AOM_ROOT}/test/coding_path_sync.cc"
        "${AOM_ROOT}/test/idct8x8_test.cc"
        "${AOM_ROOT}/test/partial_idct_test.cc"
        "${AOM_ROOT}/test/superframe_test.cc"
        "${AOM_ROOT}/test/tile_independence_test.cc")

    if (CONFIG_ANS)
      set(AOM_UNIT_TEST_COMMON_SOURCES
          ${AOM_UNIT_TEST_COMMON_SOURCES}
          "${AOM_ROOT}/test/ans_codec_test.cc"
          "${AOM_ROOT}/test/ans_test.cc")
    else ()
      set(AOM_UNIT_TEST_COMMON_SOURCES
          ${AOM_UNIT_TEST_COMMON_SOURCES}
          "${AOM_ROOT}/test/binary_codes_test.cc"
          "${AOM_ROOT}/test/boolcoder_test.cc")
    endif ()

    if (CONFIG_EXT_TILE)
      set(AOM_UNIT_TEST_COMMON_SOURCES
          ${AOM_UNIT_TEST_COMMON_SOURCES}
          "${AOM_ROOT}/test/av1_ext_tile_test.cc")
    endif ()
  endif ()
endif ()

if (CONFIG_HIGHBITDEPTH)
  if (CONFIG_AV1_ENCODER AND NOT BUILD_SHARED_LIBS)
    set(AOM_UNIT_TEST_COMMON_INTRIN_SSE4_1
        ${AOM_UNIT_TEST_COMMON_INTRIN_SSE4_1}
        "${AOM_ROOT}/test/av1_highbd_iht_test.cc"
        "${AOM_ROOT}/test/av1_quantize_test.cc")
  endif ()

  if (CONFIG_INTERNAL_STATS)
    set(AOM_UNIT_TEST_COMMON_SOURCES
        ${AOM_UNIT_TEST_COMMON_SOURCES}
        "${AOM_ROOT}/test/hbd_metrics_test.cc")
  endif ()
endif ()

if (CONFIG_UNIT_TESTS)
  find_package(PythonInterp)
  if (NOT PYTHONINTERP_FOUND)
    message(WARNING "--- Unit tests disabled: Python not found.")
    set(CONFIG_UNIT_TESTS 0)
  endif ()

  if (MSVC)
    # Force static run time to avoid collisions with googletest.
    include("${AOM_ROOT}/build/cmake/msvc_runtime.cmake")
  endif ()

  if (BUILD_SHARED_LIBS AND APPLE)
    # Silence an RPATH warning.
    set(CMAKE_MACOSX_RPATH 1)
  endif ()

  include_directories(
    "${AOM_ROOT}/third_party/googletest/src/googletest/src"
    "${AOM_ROOT}/third_party/googletest/src/googletest/include")

  if (AOM_DISABLE_GTEST_CMAKE)
    include_directories("${AOM_ROOT}/third_party/googletest/src/googletest")
    add_library(gtest STATIC
      "${AOM_ROOT}/third_party/googletest/src/googletest/src/gtest-all.cc")
  else ()
    add_subdirectory("${AOM_ROOT}/third_party/googletest/src/googletest"
                     EXCLUDE_FROM_ALL)
  endif ()

endif ()

# Setup the targets for CONFIG_UNIT_TESTS. The libaom and app util targets must
# exist before this function is called.
function (setup_aom_test_targets)
  add_library(test_aom_common OBJECT ${AOM_UNIT_TEST_COMMON_SOURCES})
  add_dependencies(test_aom_common aom)

  if (CONFIG_AV1_DECODER)
    add_library(test_aom_decoder OBJECT ${AOM_UNIT_TEST_DECODER_SOURCES})
    add_dependencies(test_aom_decoder aom)
  endif ()

  if (CONFIG_AV1_ENCODER)
    add_library(test_aom_encoder OBJECT ${AOM_UNIT_TEST_ENCODER_SOURCES})
    add_dependencies(test_aom_encoder aom)
  endif ()

  add_executable(test_libaom ${AOM_UNIT_TEST_WRAPPER_SOURCES}
                 $<TARGET_OBJECTS:aom_common_app_util>
                 $<TARGET_OBJECTS:test_aom_common>)
  set(AOM_APP_TARGETS ${AOM_APP_TARGETS} test_libaom)

  if (CONFIG_AV1_DECODER)
    target_sources(test_libaom PRIVATE
                   $<TARGET_OBJECTS:aom_decoder_app_util>
                   $<TARGET_OBJECTS:test_aom_decoder>)

    if (CONFIG_DECODE_PERF_TESTS AND CONFIG_WEBM_IO)
      target_sources(test_libaom PRIVATE ${AOM_DECODE_PERF_TEST_SOURCES})
    endif ()
  endif ()

  if (CONFIG_AV1_ENCODER)
    target_sources(test_libaom PRIVATE
                   $<TARGET_OBJECTS:test_aom_encoder>
                   $<TARGET_OBJECTS:aom_encoder_app_util>)

    if (CONFIG_ENCODE_PERF_TESTS)
      target_sources(test_libaom PRIVATE ${AOM_ENCODE_PERF_TEST_SOURCES})
    endif ()

    if (NOT BUILD_SHARED_LIBS)
      add_executable(test_intra_pred_speed
                     ${AOM_TEST_INTRA_PRED_SPEED_SOURCES}
                     $<TARGET_OBJECTS:aom_common_app_util>)
      target_link_libraries(test_intra_pred_speed ${AOM_LIB_LINK_TYPE}
                            aom gtest)
      set(AOM_APP_TARGETS ${AOM_APP_TARGETS} test_intra_pred_speed)
    endif ()
  endif ()

  target_link_libraries(test_libaom ${AOM_LIB_LINK_TYPE} aom gtest)

  if (CONFIG_LIBYUV)
    target_sources(test_libaom PRIVATE $<TARGET_OBJECTS:yuv>)
  endif ()
  if (CONFIG_WEBM_IO)
    target_sources(test_libaom PRIVATE $<TARGET_OBJECTS:webm>)
  endif ()
  if (HAVE_SSE2)
    add_intrinsics_source_to_target("-msse2" "test_libaom"
                                    "AOM_UNIT_TEST_COMMON_INTRIN_SSE2")
  endif ()
  if (HAVE_SSSE3)
    add_intrinsics_source_to_target("-mssse3" "test_libaom"
                                    "AOM_UNIT_TEST_COMMON_INTRIN_SSSE3")
  endif ()
  if (HAVE_SSE4_1)
    add_intrinsics_source_to_target("-msse4.1" "test_libaom"
                                    "AOM_UNIT_TEST_COMMON_INTRIN_SSE4_1")
    if (CONFIG_AV1_ENCODER)
      if (AOM_UNIT_TEST_ENCODER_INTRIN_SSE4_1)
        add_intrinsics_source_to_target("-msse4.1" "test_libaom"
                                        "AOM_UNIT_TEST_ENCODER_INTRIN_SSE4_1")
      endif ()
    endif ()
  endif ()
  if (HAVE_AVX2)
    add_intrinsics_source_to_target("-mavx2" "test_libaom"
                                    "AOM_UNIT_TEST_COMMON_INTRIN_AVX2")
  endif ()
  if (HAVE_NEON)
    add_intrinsics_source_to_target("${AOM_NEON_INTRIN_FLAG}" "test_libaom"
                                    "AOM_UNIT_TEST_COMMON_INTRIN_NEON")
  endif ()

  if (NOT ENABLE_IDE_TEST_HOSTING)
    if (MSVC OR XCODE)
      # Skip creation of test data download and test run targets when generating
      # for Visual Studio and Xcode unless the user explicitly requests IDE test
      # hosting. This is done to make build cycles in the IDE tolerable when the
      # IDE command for build project is used to build AOM. Default behavior in
      # IDEs is to build all targets, and the test run takes hours.
      return ()
    endif ()
  endif ()

  make_test_data_lists("${AOM_UNIT_TEST_DATA_LIST_FILE}"
                       test_files test_file_checksums)
  list(LENGTH test_files num_test_files)
  list(LENGTH test_file_checksums num_test_file_checksums)

  math(EXPR max_file_index "${num_test_files} - 1")
  foreach (test_index RANGE ${max_file_index})
    list(GET test_files ${test_index} test_file)
    list(GET test_file_checksums ${test_index} test_file_checksum)
    add_custom_target(testdata_${test_index}
                      COMMAND ${CMAKE_COMMAND}
                        -DAOM_CONFIG_DIR="${AOM_CONFIG_DIR}"
                        -DAOM_ROOT="${AOM_ROOT}"
                        -DAOM_TEST_FILE="${test_file}"
                        -DAOM_TEST_CHECKSUM=${test_file_checksum}
                        -P "${AOM_ROOT}/test/test_data_download_worker.cmake")
    set(testdata_targets ${testdata_targets} testdata_${test_index})
  endforeach ()

  # Create a custom build target for running each test data download target.
  add_custom_target(testdata)
  add_dependencies(testdata ${testdata_targets})

  # Pick a reasonable number of targets (this controls parallelization).
  ProcessorCount(num_test_targets)
  if (num_test_targets EQUAL 0)
    # Just default to 10 targets when there's no processor count available.
    set(num_test_targets 10)
  endif ()

  math(EXPR max_shard_index "${num_test_targets} - 1")
  foreach (shard_index RANGE ${max_shard_index})
    set(test_name "test_${shard_index}")
    add_custom_target(${test_name}
                      COMMAND ${CMAKE_COMMAND}
                      -DGTEST_SHARD_INDEX=${shard_index}
                      -DGTEST_TOTAL_SHARDS=${num_test_targets}
                      -DTEST_LIBAOM=$<TARGET_FILE:test_libaom>
                      -P "${AOM_ROOT}/test/test_runner.cmake"
                      DEPENDS testdata test_libaom)
    set(test_targets ${test_targets} ${test_name})
  endforeach ()
  add_custom_target(runtests)
  add_dependencies(runtests ${test_targets})

  set(AOM_APP_TARGETS ${AOM_APP_TARGETS} PARENT_SCOPE)
endfunction ()

endif ()  # AOM_TEST_TEST_CMAKE_
