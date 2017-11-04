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
if (NOT AOM_AOM_DSP_AOM_DSP_CMAKE_)
set(AOM_AOM_DSP_AOM_DSP_CMAKE_ 1)

set(AOM_DSP_COMMON_SOURCES
    "${AOM_ROOT}/aom_dsp/aom_convolve.c"
    "${AOM_ROOT}/aom_dsp/aom_convolve.h"
    "${AOM_ROOT}/aom_dsp/aom_dsp_common.h"
    "${AOM_ROOT}/aom_dsp/aom_filter.h"
    "${AOM_ROOT}/aom_dsp/aom_simd.h"
    "${AOM_ROOT}/aom_dsp/aom_simd_inline.h"
    "${AOM_ROOT}/aom_dsp/blend.h"
    "${AOM_ROOT}/aom_dsp/blend_a64_hmask.c"
    "${AOM_ROOT}/aom_dsp/blend_a64_mask.c"
    "${AOM_ROOT}/aom_dsp/blend_a64_vmask.c"
    "${AOM_ROOT}/aom_dsp/intrapred.c"
    "${AOM_ROOT}/aom_dsp/intrapred_common.h"
    "${AOM_ROOT}/aom_dsp/loopfilter.c"
    "${AOM_ROOT}/aom_dsp/prob.c"
    "${AOM_ROOT}/aom_dsp/prob.h"
    "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/subtract.c"
    "${AOM_ROOT}/aom_dsp/txfm_common.h"
    "${AOM_ROOT}/aom_dsp/x86/txfm_common_intrin.h")

set(AOM_DSP_COMMON_ASM_SSE2
    "${AOM_ROOT}/aom_dsp/x86/aom_convolve_copy_sse2.asm"
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_sse2.asm"
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_bilinear_sse2.asm"
    "${AOM_ROOT}/aom_dsp/x86/intrapred_sse2.asm")

set(AOM_DSP_COMMON_INTRIN_SSE2
    "${AOM_ROOT}/aom_dsp/x86/aom_asm_stubs.c"
    "${AOM_ROOT}/aom_dsp/x86/convolve.h"
    "${AOM_ROOT}/aom_dsp/x86/intrapred_sse2.c"
    "${AOM_ROOT}/aom_dsp/x86/txfm_common_sse2.h"
    "${AOM_ROOT}/aom_dsp/x86/lpf_common_sse2.h"
    "${AOM_ROOT}/aom_dsp/x86/loopfilter_sse2.c")

set(AOM_DSP_COMMON_ASM_SSSE3
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_ssse3.asm"
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_bilinear_ssse3.asm"
    "${AOM_ROOT}/aom_dsp/x86/intrapred_ssse3.asm")

set(AOM_DSP_COMMON_INTRIN_SSSE3
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_intrin_ssse3.c"
    "${AOM_ROOT}/aom_dsp/x86/intrapred_ssse3.c"
    "${AOM_ROOT}/aom_dsp/x86/inv_txfm_ssse3.c")

set(AOM_DSP_COMMON_INTRIN_SSE4_1
    "${AOM_ROOT}/aom_dsp/x86/blend_a64_hmask_sse4.c"
    "${AOM_ROOT}/aom_dsp/x86/blend_a64_mask_sse4.c"
    "${AOM_ROOT}/aom_dsp/x86/blend_a64_vmask_sse4.c")

set(AOM_DSP_COMMON_INTRIN_AVX2
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_intrin_avx2.c"
    "${AOM_ROOT}/aom_dsp/x86/intrapred_avx2.c"
    "${AOM_ROOT}/aom_dsp/x86/inv_txfm_avx2.c"
    "${AOM_ROOT}/aom_dsp/x86/common_avx2.h"
    "${AOM_ROOT}/aom_dsp/x86/inv_txfm_common_avx2.h"
    "${AOM_ROOT}/aom_dsp/x86/txfm_common_avx2.h")

if (NOT CONFIG_PARALLEL_DEBLOCKING)
  set(AOM_DSP_COMMON_INTRIN_AVX2
      ${AOM_DSP_COMMON_INTRIN_AVX2}
      "${AOM_ROOT}/aom_dsp/x86/loopfilter_avx2.c")
endif ()

if (NOT CONFIG_EXT_PARTITION)
  set(AOM_DSP_COMMON_ASM_NEON
      "${AOM_ROOT}/aom_dsp/arm/aom_convolve8_avg_neon_asm.asm"
      "${AOM_ROOT}/aom_dsp/arm/aom_convolve8_neon_asm.asm"
      "${AOM_ROOT}/aom_dsp/arm/aom_convolve_avg_neon_asm.asm"
      "${AOM_ROOT}/aom_dsp/arm/aom_convolve_copy_neon_asm.asm")
endif ()

set(AOM_DSP_COMMON_ASM_NEON
    ${AOM_DSP_COMMON_ASM_NEON}
    "${AOM_ROOT}/aom_dsp/arm/idct16x16_1_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/idct16x16_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/idct32x32_1_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/idct32x32_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/idct4x4_1_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/idct4x4_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/idct8x8_1_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/idct8x8_add_neon.asm"
    "${AOM_ROOT}/aom_dsp/arm/intrapred_neon_asm.asm"
    "${AOM_ROOT}/aom_dsp/arm/save_reg_neon.asm")

if (NOT CONFIG_PARALLEL_DEBLOCKING)
  set(AOM_DSP_COMMON_ASM_NEON
      ${AOM_DSP_COMMON_ASM_NEON}
      "${AOM_ROOT}/aom_dsp/arm/loopfilter_16_neon.asm"
      "${AOM_ROOT}/aom_dsp/arm/loopfilter_4_neon.asm"
      "${AOM_ROOT}/aom_dsp/arm/loopfilter_8_neon.asm"
      "${AOM_ROOT}/aom_dsp/arm/loopfilter_mb_neon.asm")
endif ()

if (NOT CONFIG_EXT_PARTITION)
  set(AOM_DSP_COMMON_INTRIN_NEON
      "${AOM_ROOT}/aom_dsp/arm/aom_convolve_neon.c")
endif ()

set(AOM_DSP_COMMON_INTRIN_NEON
    ${AOM_DSP_COMMON_INTRIN_NEON}
    "${AOM_ROOT}/aom_dsp/arm/avg_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/fwd_txfm_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/hadamard_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/idct16x16_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/intrapred_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/sad4d_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/sad_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/subpel_variance_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/subtract_neon.c"
    "${AOM_ROOT}/aom_dsp/arm/variance_neon.c")

if (NOT CONFIG_PARALLEL_DEBLOCKING)
  set(AOM_DSP_COMMON_INTRIN_NEON
      ${AOM_DSP_COMMON_INTRIN_NEON}
      "${AOM_ROOT}/aom_dsp/arm/loopfilter_neon.c")
endif ()

if ("${AOM_TARGET_CPU}" STREQUAL "arm64")
  if (NOT CONFIG_EXT_PARTITION)
    set(AOM_DSP_COMMON_INTRIN_NEON
        ${AOM_DSP_COMMON_INTRIN_NEON}
        "${AOM_ROOT}/aom_dsp/arm/aom_convolve8_avg_neon.c"
        "${AOM_ROOT}/aom_dsp/arm/aom_convolve8_neon.c"
        "${AOM_ROOT}/aom_dsp/arm/aom_convolve_avg_neon.c"
        "${AOM_ROOT}/aom_dsp/arm/aom_convolve_copy_neon.c")
  endif ()

  set(AOM_DSP_COMMON_INTRIN_NEON
      ${AOM_DSP_COMMON_INTRIN_NEON}
      "${AOM_ROOT}/aom_dsp/arm/idct16x16_1_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/idct16x16_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/idct32x32_1_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/idct32x32_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/idct4x4_1_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/idct4x4_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/idct8x8_1_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/idct8x8_add_neon.c"
      "${AOM_ROOT}/aom_dsp/arm/intrapred_neon.c")

  if (NOT CONFIG_PARALLEL_DEBLOCKING)
    set(AOM_DSP_COMMON_INTRIN_NEON
        ${AOM_DSP_COMMON_INTRIN_NEON}
        "${AOM_ROOT}/aom_dsp/arm/loopfilter_16_neon.c"
        "${AOM_ROOT}/aom_dsp/arm/loopfilter_4_neon.c"
        "${AOM_ROOT}/aom_dsp/arm/loopfilter_8_neon.c")
  endif ()
endif ()

set(AOM_DSP_COMMON_INTRIN_DSPR2
    "${AOM_ROOT}/aom_dsp/mips/common_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/common_dspr2.h"
    "${AOM_ROOT}/aom_dsp/mips/convolve2_avg_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve2_avg_horiz_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve2_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve2_horiz_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve2_vert_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve8_avg_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve8_avg_horiz_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve8_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve8_horiz_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve8_vert_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/convolve_common_dspr2.h"
    "${AOM_ROOT}/aom_dsp/mips/intrapred16_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/intrapred4_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/intrapred8_dspr2.c"
    "${AOM_ROOT}/aom_dsp/mips/inv_txfm_dspr2.h")

if (NOT CONFIG_PARALLEL_DEBLOCKING)
  set(AOM_DSP_COMMON_INTRIN_DSPR2
      ${AOM_DSP_COMMON_INTRIN_DSPR2}
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_filters_dspr2.c"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_filters_dspr2.h"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_macros_dspr2.h"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_masks_dspr2.h"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_mb_dspr2.c"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_mb_horiz_dspr2.c"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_mb_vert_dspr2.c")
endif ()

set(AOM_DSP_COMMON_INTRIN_MSA
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve8_avg_horiz_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve8_avg_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve8_avg_vert_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve8_horiz_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve8_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve8_vert_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve_avg_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve_copy_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/aom_convolve_msa.h"
    "${AOM_ROOT}/aom_dsp/mips/fwd_dct32x32_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/fwd_txfm_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/fwd_txfm_msa.h"
    "${AOM_ROOT}/aom_dsp/mips/idct16x16_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/idct32x32_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/idct4x4_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/idct8x8_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/intrapred_msa.c"
    "${AOM_ROOT}/aom_dsp/mips/inv_txfm_msa.h"
    "${AOM_ROOT}/aom_dsp/mips/macros_msa.h"
    "${AOM_ROOT}/aom_dsp/mips/txfm_macros_msa.h")

if (NOT CONFIG_PARALLEL_DEBLOCKING)
  set(AOM_DSP_COMMON_INTRIN_MSA
      ${AOM_DSP_COMMON_INTRIN_MSA}
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_16_msa.c"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_4_msa.c"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_8_msa.c"
      "${AOM_ROOT}/aom_dsp/mips/loopfilter_msa.h")
endif ()

if (CONFIG_HIGHBITDEPTH)
  set(AOM_DSP_COMMON_ASM_SSE2
      ${AOM_DSP_COMMON_ASM_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/aom_high_subpixel_8t_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/aom_high_subpixel_bilinear_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/highbd_intrapred_sse2.asm")

  set(AOM_DSP_COMMON_INTRIN_SSE2
      ${AOM_DSP_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/highbd_intrapred_sse2.c"
      "${AOM_ROOT}/aom_dsp/x86/highbd_loopfilter_sse2.c")

  set(AOM_DSP_COMMON_INTRIN_SSSE3
      ${AOM_DSP_COMMON_INTRIN_SSSE3}
      "${AOM_ROOT}/aom_dsp/x86/highbd_intrapred_ssse3.c")

  set(AOM_DSP_COMMON_INTRIN_AVX2
      ${AOM_DSP_COMMON_INTRIN_AVX2}
      "${AOM_ROOT}/aom_dsp/x86/highbd_convolve_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/highbd_intrapred_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/highbd_loopfilter_avx2.c")
else ()
  set(AOM_DSP_COMMON_INTRIN_DSPR2
      ${AOM_DSP_COMMON_INTRIN_DSPR2}
      "${AOM_ROOT}/aom_dsp/mips/itrans16_dspr2.c"
      "${AOM_ROOT}/aom_dsp/mips/itrans32_cols_dspr2.c"
      "${AOM_ROOT}/aom_dsp/mips/itrans32_dspr2.c"
      "${AOM_ROOT}/aom_dsp/mips/itrans4_dspr2.c"
      "${AOM_ROOT}/aom_dsp/mips/itrans8_dspr2.c")
endif ()

if (CONFIG_ANS)
  set(AOM_DSP_COMMON_SOURCES
      ${AOM_DSP_COMMON_SOURCES}
      "${AOM_ROOT}/aom_dsp/ans.h")
else ()
  set(AOM_DSP_COMMON_SOURCES
      ${AOM_DSP_COMMON_SOURCES}
      "${AOM_ROOT}/aom_dsp/entcode.c"
      "${AOM_ROOT}/aom_dsp/entcode.h")
endif ()

if (CONFIG_AV1)
  set(AOM_DSP_COMMON_SOURCES
      ${AOM_DSP_COMMON_SOURCES}
      "${AOM_ROOT}/aom_dsp/inv_txfm.c"
      "${AOM_ROOT}/aom_dsp/inv_txfm.h")

  set(AOM_DSP_COMMON_ASM_SSE2
      ${AOM_DSP_COMMON_ASM_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/inv_wht_sse2.asm")

  set(AOM_DSP_COMMON_INTRIN_SSE2
      ${AOM_DSP_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/inv_txfm_sse2.c"
      "${AOM_ROOT}/aom_dsp/x86/inv_txfm_sse2.h")
endif ()

if (CONFIG_AV1_DECODER)
  set(AOM_DSP_DECODER_SOURCES
      "${AOM_ROOT}/aom_dsp/binary_codes_reader.c"
      "${AOM_ROOT}/aom_dsp/binary_codes_reader.h"
      "${AOM_ROOT}/aom_dsp/bitreader.h"
      "${AOM_ROOT}/aom_dsp/bitreader_buffer.c"
      "${AOM_ROOT}/aom_dsp/bitreader_buffer.h")

  if (CONFIG_ANS)
    set(AOM_DSP_DECODER_SOURCES
        ${AOM_DSP_DECODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/ansreader.h")
  else ()
    set(AOM_DSP_DECODER_SOURCES
        ${AOM_DSP_DECODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/daalaboolreader.c"
        "${AOM_ROOT}/aom_dsp/daalaboolreader.h"
        "${AOM_ROOT}/aom_dsp/entdec.c"
        "${AOM_ROOT}/aom_dsp/entdec.h")
  endif ()
endif ()

if (CONFIG_AV1_ENCODER)
  set(AOM_DSP_ENCODER_SOURCES
      "${AOM_ROOT}/aom_dsp/binary_codes_writer.c"
      "${AOM_ROOT}/aom_dsp/binary_codes_writer.h"
      "${AOM_ROOT}/aom_dsp/bitwriter.h"
      "${AOM_ROOT}/aom_dsp/bitwriter_buffer.c"
      "${AOM_ROOT}/aom_dsp/bitwriter_buffer.h"
      "${AOM_ROOT}/aom_dsp/psnr.c"
      "${AOM_ROOT}/aom_dsp/psnr.h"
      "${AOM_ROOT}/aom_dsp/sad.c"
      "${AOM_ROOT}/aom_dsp/variance.c"
      "${AOM_ROOT}/aom_dsp/variance.h")

  set(AOM_DSP_ENCODER_ASM_SSE2
      ${AOM_DSP_ENCODER_ASM_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/halfpix_variance_impl_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/sad4d_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/sad_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/subtract_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/subpel_variance_sse2.asm")

  set(AOM_DSP_ENCODER_INTRIN_SSE2
      "${AOM_ROOT}/aom_dsp/x86/quantize_sse2.c")

  set(AOM_DSP_ENCODER_ASM_SSSE3
      "${AOM_ROOT}/aom_dsp/x86/sad_ssse3.asm")

  set(AOM_DSP_ENCODER_ASM_SSSE3_X86_64
      "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_ssse3_x86_64.asm"
      "${AOM_ROOT}/aom_dsp/x86/ssim_opt_x86_64.asm")

  set(AOM_DSP_ENCODER_INTRIN_SSE3 "${AOM_ROOT}/aom_dsp/x86/sad_sse3.asm")
  set(AOM_DSP_ENCODER_ASM_SSE4_1 "${AOM_ROOT}/aom_dsp/x86/sad_sse4.asm")

  set(AOM_DSP_ENCODER_INTRIN_AVX2
      "${AOM_ROOT}/aom_dsp/x86/fwd_dct32x32_impl_avx2.h"
      "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_avx2.h"
      "${AOM_ROOT}/aom_dsp/x86/highbd_quantize_intrin_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/sad4d_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/sad_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/sad_impl_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/variance_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/variance_impl_avx2.c")

  if (CONFIG_AV1_ENCODER)
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/avg.c"
        "${AOM_ROOT}/aom_dsp/fwd_txfm.c"
        "${AOM_ROOT}/aom_dsp/fwd_txfm.h"
        "${AOM_ROOT}/aom_dsp/quantize.c"
        "${AOM_ROOT}/aom_dsp/quantize.h"
        "${AOM_ROOT}/aom_dsp/sum_squares.c")

    set(AOM_DSP_ENCODER_INTRIN_SSE2
        ${AOM_DSP_ENCODER_INTRIN_SSE2}
        "${AOM_ROOT}/aom_dsp/x86/avg_intrin_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/fwd_dct32_8cols_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/fwd_dct32x32_impl_sse2.h"
        "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_impl_sse2.h"
        "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_sse2.h"
        "${AOM_ROOT}/aom_dsp/x86/halfpix_variance_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/highbd_quantize_intrin_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/variance_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/sum_squares_sse2.c")

    set(AOM_DSP_ENCODER_ASM_SSSE3_X86_64
        ${AOM_DSP_ENCODER_ASM_SSSE3_X86_64}
        "${AOM_ROOT}/aom_dsp/x86/avg_ssse3_x86_64.asm"
        "${AOM_ROOT}/aom_dsp/x86/quantize_ssse3_x86_64.asm")

    set(AOM_DSP_ENCODER_AVX_ASM_X86_64
        ${AOM_DSP_ENCODER_AVX_ASM_X86_64}
        "${AOM_ROOT}/aom_dsp/x86/quantize_avx_x86_64.asm")

    set(AOM_DSP_ENCODER_INTRIN_MSA
        "${AOM_ROOT}/aom_dsp/mips/sad_msa.c"
        "${AOM_ROOT}/aom_dsp/mips/subtract_msa.c"
        "${AOM_ROOT}/aom_dsp/mips/variance_msa.c"
        "${AOM_ROOT}/aom_dsp/mips/sub_pixel_variance_msa.c")

      set(AOM_DSP_ENCODER_INTRIN_SSSE3
          ${AOM_DSP_ENCODER_INTRIN_SSSE3}
          "${AOM_ROOT}/aom_dsp/x86/masked_sad_intrin_ssse3.c"
          "${AOM_ROOT}/aom_dsp/x86/masked_variance_intrin_ssse3.c")

    if (CONFIG_HIGHBITDEPTH)
      set(AOM_DSP_ENCODER_INTRIN_SSE2
          ${AOM_DSP_ENCODER_INTRIN_SSE2}
          "${AOM_ROOT}/aom_dsp/x86/highbd_subtract_sse2.c")
    endif ()
  endif ()

  if (CONFIG_HIGHBITDEPTH)
    set(AOM_DSP_ENCODER_ASM_SSE2
        ${AOM_DSP_ENCODER_ASM_SSE2}
        "${AOM_ROOT}/aom_dsp/x86/highbd_sad4d_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/highbd_sad_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/highbd_subpel_variance_impl_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/highbd_variance_impl_sse2.asm")

    set(AOM_DSP_ENCODER_INTRIN_SSE2
        ${AOM_DSP_ENCODER_INTRIN_SSE2}
        "${AOM_ROOT}/aom_dsp/x86/highbd_variance_sse2.c")

    set(AOM_DSP_ENCODER_INTRIN_SSE4_1
        ${AOM_DSP_ENCODER_INTRIN_SSE4_1}
        "${AOM_ROOT}/aom_dsp/x86/highbd_variance_sse4.c")

    set(AOM_DSP_ENCODER_INTRIN_AVX2
        ${AOM_DSP_ENCODER_INTRIN_AVX2}
        "${AOM_ROOT}/aom_dsp/x86/sad_highbd_avx2.c")
  endif ()

  if (CONFIG_ANS)
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/answriter.h"
        "${AOM_ROOT}/aom_dsp/buf_ans.c"
        "${AOM_ROOT}/aom_dsp/buf_ans.h")
  else ()
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/daalaboolwriter.c"
        "${AOM_ROOT}/aom_dsp/daalaboolwriter.h"
        "${AOM_ROOT}/aom_dsp/entenc.c"
        "${AOM_ROOT}/aom_dsp/entenc.h")
  endif ()

  if (CONFIG_INTERNAL_STATS)
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/fastssim.c"
        "${AOM_ROOT}/aom_dsp/psnrhvs.c"
        "${AOM_ROOT}/aom_dsp/ssim.c"
        "${AOM_ROOT}/aom_dsp/ssim.h")
  endif ()
endif ()

if (CONFIG_LOOP_RESTORATION)
  set(AOM_DSP_COMMON_INTRIN_SSE2
      ${AOM_DSP_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/aom_convolve_hip_sse2.c")

  if (CONFIG_HIGHBITDEPTH)
    set(AOM_DSP_COMMON_INTRIN_SSSE3
      ${AOM_DSP_COMMON_INTRIN_SSSE3}
        "${AOM_ROOT}/aom_dsp/x86/aom_highbd_convolve_hip_ssse3.c")
  endif ()
endif ()

if (CONFIG_MOTION_VAR)
  set(AOM_DSP_ENCODER_INTRIN_SSE4_1
      ${AOM_DSP_ENCODER_INTRIN_SSE4_1}
      "${AOM_ROOT}/aom_dsp/x86/obmc_sad_sse4.c"
      "${AOM_ROOT}/aom_dsp/x86/obmc_variance_sse4.c")
endif ()

# Creates aom_dsp build targets. Must not be called until after libaom target
# has been created.
function (setup_aom_dsp_targets)
  add_library(aom_dsp_common OBJECT ${AOM_DSP_COMMON_SOURCES})
  list(APPEND AOM_LIB_TARGETS aom_dsp_common)
  create_dummy_source_file("aom_av1" "c" "dummy_source_file")
  add_library(aom_dsp OBJECT "${dummy_source_file}")
  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp_common>)
  list(APPEND AOM_LIB_TARGETS aom_dsp)

  # Not all generators support libraries consisting only of object files. Add a
  # dummy source file to the aom_dsp target.
  add_dummy_source_file_to_target("aom_dsp" "c")

  if (CONFIG_AV1_DECODER)
    add_library(aom_dsp_decoder OBJECT ${AOM_DSP_DECODER_SOURCES})
    set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_dsp_decoder)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp_decoder>)
  endif ()

  if (CONFIG_AV1_ENCODER)
    add_library(aom_dsp_encoder OBJECT ${AOM_DSP_ENCODER_SOURCES})
    set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_dsp_encoder)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp_encoder>)
  endif ()

  if (HAVE_SSE2)
    add_asm_library("aom_dsp_common_sse2" "AOM_DSP_COMMON_ASM_SSE2" "aom")
    add_intrinsics_object_library("-msse2" "sse2" "aom_dsp_common"
                                   "AOM_DSP_COMMON_INTRIN_SSE2" "aom")

    if (CONFIG_AV1_ENCODER)
      add_asm_library("aom_dsp_encoder_sse2" "AOM_DSP_ENCODER_ASM_SSE2"
                      "aom")
      add_intrinsics_object_library("-msse2" "sse2" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_SSE2" "aom")
    endif()
  endif ()

  if (HAVE_SSE3 AND CONFIG_AV1_ENCODER)
    add_asm_library("aom_dsp_encoder_sse3" "AOM_DSP_ENCODER_INTRIN_SSE3" "aom")
  endif ()

  if (HAVE_SSSE3)
    add_asm_library("aom_dsp_common_ssse3" "AOM_DSP_COMMON_ASM_SSSE3" "aom")
    add_intrinsics_object_library("-mssse3" "ssse3" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSSE3" "aom")

    if (CONFIG_AV1_ENCODER)
      if ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
        list(APPEND AOM_DSP_ENCODER_ASM_SSSE3
             ${AOM_DSP_ENCODER_ASM_SSSE3_X86_64})
      endif ()
      add_asm_library("aom_dsp_encoder_ssse3" "AOM_DSP_ENCODER_ASM_SSSE3" "aom")
      if (AOM_DSP_ENCODER_INTRIN_SSSE3)
        add_intrinsics_object_library("-mssse3" "ssse3" "aom_dsp_encoder"
                                      "AOM_DSP_ENCODER_INTRIN_SSSE3" "aom")
      endif ()
    endif ()
  endif ()

  if (HAVE_SSE4_1)
    add_intrinsics_object_library("-msse4.1" "sse4_1" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSE4_1" "aom")
    if (CONFIG_AV1_ENCODER)
      if (AOM_DSP_ENCODER_INTRIN_SSE4_1)
        add_intrinsics_object_library("-msse4.1" "sse4_1" "aom_dsp_encoder"
                                      "AOM_DSP_ENCODER_INTRIN_SSE4_1" "aom")
      endif ()
      add_asm_library("aom_dsp_encoder_sse4_1" "AOM_DSP_ENCODER_ASM_SSE4_1"
                      "aom")
    endif ()
  endif ()

  if (HAVE_AVX AND "${AOM_TARGET_CPU}" STREQUAL "x86_64")
    if (CONFIG_AV1_ENCODER)
      add_asm_library("aom_dsp_encoder_avx" "AOM_DSP_ENCODER_AVX_ASM_X86_64"
                      "aom")
    endif ()
  endif ()

  if (HAVE_AVX2)
    add_intrinsics_object_library("-mavx2" "avx2" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_AVX2" "aom")
    if (CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("-mavx2" "avx2" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_AVX2" "aom")
    endif ()
  endif ()

  if (HAVE_NEON_ASM)
    if (AOM_ADS2GAS_REQUIRED)
      add_gas_asm_library("aom_dsp_common_neon" "AOM_DSP_COMMON_ASM_NEON" "aom")
    else ()
      add_asm_library("aom_dsp_common_neon" "AOM_DSP_COMMON_ASM_NEON" "aom")
    endif ()
  endif ()

  if (HAVE_NEON)
    add_intrinsics_object_library("${AOM_NEON_INTRIN_FLAG}" "neon"
                                  "aom_dsp_common" "AOM_DSP_COMMON_INTRIN_NEON"
                                  "aom")
  endif ()

  if (HAVE_DSPR2)
    add_intrinsics_object_library("" "dspr2" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_DSPR2" "aom")
  endif ()

  if (HAVE_MSA)
    add_intrinsics_object_library("" "msa" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_MSA" "aom")
    if (CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("" "msa" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_MSA" "aom")
    endif ()
  endif ()

  # Pass the new lib targets up to the parent scope instance of
  # $AOM_LIB_TARGETS.
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction ()

endif ()  # AOM_AOM_DSP_AOM_DSP_CMAKE_
