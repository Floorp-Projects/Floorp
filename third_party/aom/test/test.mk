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

LIBAOM_TEST_SRCS-yes += acm_random.h
LIBAOM_TEST_SRCS-yes += clear_system_state.h
LIBAOM_TEST_SRCS-yes += codec_factory.h
LIBAOM_TEST_SRCS-yes += md5_helper.h
LIBAOM_TEST_SRCS-yes += register_state_check.h
LIBAOM_TEST_SRCS-yes += test.mk
LIBAOM_TEST_SRCS-yes += test_libaom.cc
LIBAOM_TEST_SRCS-yes += util.h
LIBAOM_TEST_SRCS-yes += video_source.h
LIBAOM_TEST_SRCS-yes += transform_test_base.h
LIBAOM_TEST_SRCS-yes += function_equivalence_test.h

##
## BLACK BOX TESTS
##
## Black box tests only use the public API.
##
LIBAOM_TEST_SRCS-yes                   += ../md5_utils.h ../md5_utils.c
LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER)    += ivf_video_source.h
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += ../y4minput.h ../y4minput.c
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += altref_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += aq_segment_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += datarate_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += encode_api_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += error_resilience_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += i420_video_source.h
#LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += realtime_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += resize_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += y4m_video_source.h
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += yuv_video_source.h

#LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += level_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += active_map_refresh_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += active_map_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += borders_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += cpu_speed_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += frame_size_tests.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += lossless_test.cc

LIBAOM_TEST_SRCS-yes                   += decode_test_driver.cc
LIBAOM_TEST_SRCS-yes                   += decode_test_driver.h
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += encode_test_driver.cc
LIBAOM_TEST_SRCS-yes                   += encode_test_driver.h

## IVF writing.
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += ../ivfenc.c ../ivfenc.h

## Y4m parsing.
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER)    += y4m_test.cc ../y4menc.c ../y4menc.h

## WebM Parsing
ifeq ($(CONFIG_WEBM_IO), yes)
LIBWEBM_PARSER_SRCS += ../third_party/libwebm/mkvparser/mkvparser.cc
LIBWEBM_PARSER_SRCS += ../third_party/libwebm/mkvparser/mkvreader.cc
LIBWEBM_PARSER_SRCS += ../third_party/libwebm/mkvparser/mkvparser.h
LIBWEBM_PARSER_SRCS += ../third_party/libwebm/mkvparser/mkvreader.h
LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER) += $(LIBWEBM_PARSER_SRCS)
LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER) += ../tools_common.h
LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER) += ../webmdec.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER) += ../webmdec.h
LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER) += webm_video_source.h
endif

LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER) += decode_api_test.cc

# Currently we only support decoder perf tests for av1. Also they read from WebM
# files, so WebM IO is required.
ifeq ($(CONFIG_DECODE_PERF_TESTS)$(CONFIG_AV1_DECODER)$(CONFIG_WEBM_IO), \
      yesyesyes)
LIBAOM_TEST_SRCS-yes                   += decode_perf_test.cc
endif

ifeq ($(CONFIG_ENCODE_PERF_TESTS)$(CONFIG_AV1_ENCODER), yesyes)
LIBAOM_TEST_SRCS-yes += encode_perf_test.cc
endif

## Multi-codec / unconditional black box tests.
ifeq ($(findstring yes,$(CONFIG_AV1_ENCODER)),yes)
LIBAOM_TEST_SRCS-yes += active_map_refresh_test.cc
LIBAOM_TEST_SRCS-yes += active_map_test.cc
LIBAOM_TEST_SRCS-yes += end_to_end_test.cc
endif

##
## WHITE BOX TESTS
##
## Whitebox tests invoke functions not exposed via the public API. Certain
## shared library builds don't make these functions accessible.
##
ifeq ($(CONFIG_SHARED),)

## AV1
ifeq ($(CONFIG_AV1),yes)

# These tests require both the encoder and decoder to be built.
ifeq ($(CONFIG_AV1_ENCODER)$(CONFIG_AV1_DECODER),yesyes)
# IDCT test currently depends on FDCT function
LIBAOM_TEST_SRCS-yes                   += coding_path_sync.cc
LIBAOM_TEST_SRCS-yes                   += idct8x8_test.cc
LIBAOM_TEST_SRCS-yes                   += partial_idct_test.cc
LIBAOM_TEST_SRCS-yes                   += superframe_test.cc
LIBAOM_TEST_SRCS-yes                   += tile_independence_test.cc
LIBAOM_TEST_SRCS-yes                   += ethread_test.cc
LIBAOM_TEST_SRCS-yes                   += motion_vector_test.cc
ifneq ($(CONFIG_ANS),yes)
LIBAOM_TEST_SRCS-yes                   += binary_codes_test.cc
endif
ifeq ($(CONFIG_EXT_TILE),yes)
LIBAOM_TEST_SRCS-yes                   += av1_ext_tile_test.cc
endif
ifeq ($(CONFIG_ANS),yes)
LIBAOM_TEST_SRCS-yes                   += ans_test.cc
LIBAOM_TEST_SRCS-yes                   += ans_codec_test.cc
else
LIBAOM_TEST_SRCS-yes                   += boolcoder_test.cc
ifeq ($(CONFIG_ACCOUNTING),yes)
LIBAOM_TEST_SRCS-yes                   += accounting_test.cc
endif
endif
LIBAOM_TEST_SRCS-yes                   += divu_small_test.cc
#LIBAOM_TEST_SRCS-yes                   += encoder_parms_get_to_decoder.cc
endif

LIBAOM_TEST_SRCS-$(CONFIG_ADAPT_SCAN)  += scan_test.cc
LIBAOM_TEST_SRCS-yes                   += convolve_test.cc
LIBAOM_TEST_SRCS-yes                   += lpf_8_test.cc
ifeq ($(CONFIG_CDEF_SINGLEPASS),yes)
LIBAOM_TEST_SRCS-$(CONFIG_CDEF)        += cdef_test.cc
else
LIBAOM_TEST_SRCS-$(CONFIG_CDEF)        += dering_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_CDEF)        += clpf_test.cc
endif
LIBAOM_TEST_SRCS-yes                   += simd_cmp_impl.h
LIBAOM_TEST_SRCS-$(HAVE_SSE2)          += simd_cmp_sse2.cc
LIBAOM_TEST_SRCS-$(HAVE_SSSE3)         += simd_cmp_ssse3.cc
LIBAOM_TEST_SRCS-$(HAVE_SSE4_1)        += simd_cmp_sse4.cc
LIBAOM_TEST_SRCS-$(HAVE_AVX2)          += simd_cmp_avx2.cc
LIBAOM_TEST_SRCS-$(HAVE_NEON)          += simd_cmp_neon.cc
LIBAOM_TEST_SRCS-yes                   += simd_impl.h
LIBAOM_TEST_SRCS-$(HAVE_SSE2)          += simd_sse2_test.cc
LIBAOM_TEST_SRCS-$(HAVE_SSSE3)         += simd_ssse3_test.cc
LIBAOM_TEST_SRCS-$(HAVE_SSE4_1)        += simd_sse4_test.cc
LIBAOM_TEST_SRCS-$(HAVE_AVX2)          += simd_avx2_test.cc
LIBAOM_TEST_SRCS-$(HAVE_NEON)          += simd_neon_test.cc
LIBAOM_TEST_SRCS-yes                   += intrapred_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_INTRABC)     += intrabc_test.cc
#LIBAOM_TEST_SRCS-$(CONFIG_AV1_DECODER) += av1_thread_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += dct16x16_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += dct32x32_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += fdct4x4_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += fdct8x8_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += hadamard_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += minmax_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += variance_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += error_block_test.cc
#LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_quantize_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += subtract_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += arf_freq_test.cc
ifneq ($(CONFIG_NEW_QUANT), yes)
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += quantize_func_test.cc
endif
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += block_error_test.cc

LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_inv_txfm_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_dct_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht4x4_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht8x8_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht16x16_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht32x32_test.cc
ifeq ($(CONFIG_TX64X64),yes)
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht64x64_test.cc
endif
ifeq ($(CONFIG_EXT_TX),yes)
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht4x8_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht8x4_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht8x16_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht16x8_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht16x32_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fht32x16_test.cc
endif

LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += sum_squares_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += subtract_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += blend_a64_mask_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += blend_a64_mask_1d_test.cc

LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += masked_variance_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += masked_sad_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_wedge_utils_test.cc

## Skip the unit test written for 4-tap filter intra predictor, because we
## revert to 3-tap filter.
## ifeq ($(CONFIG_FILTER_INTRA),yes)
## LIBAOM_TEST_SRCS-$(HAVE_SSE4_1) += filterintra_predictors_test.cc
## endif

ifeq ($(CONFIG_MOTION_VAR),yes)
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += obmc_sad_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += obmc_variance_test.cc
endif

ifeq ($(CONFIG_HIGHBITDEPTH),yes)
ifeq ($(CONFIG_AV1_ENCODER),yes)
LIBAOM_TEST_SRCS-$(HAVE_SSE4_1) += av1_quantize_test.cc
LIBAOM_TEST_SRCS-$(HAVE_SSE4_1) += av1_highbd_iht_test.cc
endif
endif # CONFIG_HIGHBITDEPTH
endif # AV1

## Multi-codec / unconditional whitebox tests.

ifeq ($(CONFIG_AV1_ENCODER),yes)
LIBAOM_TEST_SRCS-yes += avg_test.cc
endif
ifeq ($(CONFIG_INTERNAL_STATS),yes)
LIBAOM_TEST_SRCS-$(CONFIG_HIGHBITDEPTH) += hbd_metrics_test.cc
endif
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += sad_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1) += av1_txfm_test.h
LIBAOM_TEST_SRCS-$(CONFIG_AV1) += av1_txfm_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fwd_txfm1d_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_inv_txfm1d_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_fwd_txfm2d_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_inv_txfm2d_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1) += av1_convolve_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1) += av1_convolve_optimz_test.cc
ifneq ($(findstring yes,$(CONFIG_GLOBAL_MOTION)$(CONFIG_WARPED_MOTION)),)
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += warp_filter_test_util.h
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += warp_filter_test.cc warp_filter_test_util.cc
endif
ifeq ($(CONFIG_LOOP_RESTORATION),yes)
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += hiprec_convolve_test_util.h
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += hiprec_convolve_test.cc
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += hiprec_convolve_test_util.cc
LIBAOM_TEST_SRCS-$(HAVE_SSE4_1) += selfguided_filter_test.cc
endif
ifeq ($(CONFIG_CONVOLVE_ROUND),yes)
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += av1_convolve_2d_test_util.h
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += av1_convolve_2d_test.cc
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += av1_convolve_2d_test_util.cc
LIBAOM_TEST_SRCS-yes          += convolve_round_test.cc
endif

ifeq (yesx,$(CONFIG_CONVOLVE_ROUND)x$(CONFIG_COMPOUND_ROUND))
LIBAOM_TEST_SRCS-$(HAVE_SSE4_1) += av1_convolve_scale_test.cc
endif

ifeq ($(CONFIG_GLOBAL_MOTION)$(CONFIG_AV1_ENCODER),yesyes)
LIBAOM_TEST_SRCS-$(HAVE_SSE4_1) += corner_match_test.cc
endif

TEST_INTRA_PRED_SPEED_SRCS-yes := test_intra_pred_speed.cc
TEST_INTRA_PRED_SPEED_SRCS-yes += ../md5_utils.h ../md5_utils.c

endif # CONFIG_SHARED

include $(SRC_PATH_BARE)/test/test-data.mk
