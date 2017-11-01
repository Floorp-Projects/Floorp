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


DSP_SRCS-yes += aom_dsp.mk
DSP_SRCS-yes += aom_dsp_common.h

DSP_SRCS-$(HAVE_MSA)    += mips/macros_msa.h

DSP_SRCS-$(ARCH_X86)$(ARCH_X86_64)   += x86/synonyms.h

# bit reader
DSP_SRCS-yes += prob.h
DSP_SRCS-yes += prob.c
DSP_SRCS-$(CONFIG_ANS) += ans.h

ifeq ($(CONFIG_AV1_ENCODER),yes)
ifeq ($(CONFIG_ANS),yes)
DSP_SRCS-yes += answriter.h
DSP_SRCS-yes += buf_ans.h
DSP_SRCS-yes += buf_ans.c
else
DSP_SRCS-yes += entenc.c
DSP_SRCS-yes += entenc.h
DSP_SRCS-yes += daalaboolwriter.c
DSP_SRCS-yes += daalaboolwriter.h
endif
DSP_SRCS-yes += bitwriter.h
DSP_SRCS-yes += bitwriter_buffer.c
DSP_SRCS-yes += bitwriter_buffer.h
DSP_SRCS-yes += binary_codes_writer.c
DSP_SRCS-yes += binary_codes_writer.h
DSP_SRCS-yes += psnr.c
DSP_SRCS-yes += psnr.h
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += ssim.c
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += ssim.h
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += psnrhvs.c
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += fastssim.c
endif

ifeq ($(CONFIG_AV1_DECODER),yes)
ifeq ($(CONFIG_ANS),yes)
DSP_SRCS-yes += ansreader.h
else
DSP_SRCS-yes += entdec.c
DSP_SRCS-yes += entdec.h
DSP_SRCS-yes += daalaboolreader.c
DSP_SRCS-yes += daalaboolreader.h
endif
DSP_SRCS-yes += bitreader.h
DSP_SRCS-yes += bitreader_buffer.c
DSP_SRCS-yes += bitreader_buffer.h
DSP_SRCS-yes += binary_codes_reader.c
DSP_SRCS-yes += binary_codes_reader.h
endif

# intra predictions
DSP_SRCS-yes += intrapred.c
DSP_SRCS-yes += intrapred_common.h

ifneq ($(CONFIG_ANS),yes)
DSP_SRCS-yes += entcode.c
DSP_SRCS-yes += entcode.h
endif

DSP_SRCS-$(HAVE_SSE) += x86/intrapred_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/intrapred_sse2.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/intrapred_ssse3.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_8t_ssse3.asm

DSP_SRCS-$(HAVE_SSE2) += x86/intrapred_sse2.c
DSP_SRCS-$(HAVE_SSSE3) += x86/intrapred_ssse3.c
DSP_SRCS-$(HAVE_AVX2) += x86/intrapred_avx2.c

ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE)  += x86/highbd_intrapred_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_intrapred_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_intrapred_sse2.c
DSP_SRCS-$(HAVE_SSSE3) += x86/highbd_intrapred_ssse3.c
DSP_SRCS-$(HAVE_SSSE3) += x86/highbd_intrapred_avx2.c
endif  # CONFIG_HIGHBITDEPTH

DSP_SRCS-$(HAVE_NEON_ASM) += arm/intrapred_neon_asm$(ASM)
DSP_SRCS-$(HAVE_NEON) += arm/intrapred_neon.c
DSP_SRCS-$(HAVE_MSA) += mips/intrapred_msa.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/intrapred4_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/intrapred8_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/intrapred16_dspr2.c

DSP_SRCS-$(HAVE_DSPR2)  += mips/common_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/common_dspr2.c

# inter predictions
DSP_SRCS-yes            += blend.h
DSP_SRCS-yes            += blend_a64_mask.c
DSP_SRCS-yes            += blend_a64_hmask.c
DSP_SRCS-yes            += blend_a64_vmask.c
DSP_SRCS-$(HAVE_SSE4_1) += x86/blend_sse4.h
DSP_SRCS-$(HAVE_SSE4_1) += x86/blend_a64_mask_sse4.c
DSP_SRCS-$(HAVE_SSE4_1) += x86/blend_a64_hmask_sse4.c
DSP_SRCS-$(HAVE_SSE4_1) += x86/blend_a64_vmask_sse4.c

# interpolation filters
DSP_SRCS-yes += aom_convolve.c
DSP_SRCS-yes += aom_convolve.h
DSP_SRCS-yes += aom_filter.h

DSP_SRCS-$(ARCH_X86)$(ARCH_X86_64) += x86/convolve.h
DSP_SRCS-$(ARCH_X86)$(ARCH_X86_64) += x86/aom_asm_stubs.c
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_subpixel_8t_sse2.asm
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_subpixel_bilinear_sse2.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_8t_ssse3.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_bilinear_ssse3.asm
DSP_SRCS-$(HAVE_AVX2)  += x86/aom_subpixel_8t_intrin_avx2.c
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_8t_intrin_ssse3.c
ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_high_subpixel_8t_sse2.asm
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_high_subpixel_bilinear_sse2.asm
DSP_SRCS-$(HAVE_AVX2)  += x86/highbd_convolve_avx2.c
endif
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_convolve_copy_sse2.asm

ifneq ($(CONFIG_EXT_PARTITION),yes)
ifeq ($(HAVE_NEON_ASM),yes)
DSP_SRCS-yes += arm/aom_convolve_copy_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve8_avg_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve8_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve_avg_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve_neon.c
else
ifeq ($(HAVE_NEON),yes)
DSP_SRCS-yes += arm/aom_convolve_copy_neon.c
DSP_SRCS-yes += arm/aom_convolve8_avg_neon.c
DSP_SRCS-yes += arm/aom_convolve8_neon.c
DSP_SRCS-yes += arm/aom_convolve_avg_neon.c
DSP_SRCS-yes += arm/aom_convolve_neon.c
endif  # HAVE_NEON
endif  # HAVE_NEON_ASM
endif  # CONFIG_EXT_PARTITION

# common (msa)
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_avg_horiz_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_avg_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_avg_vert_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_horiz_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_vert_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve_avg_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve_copy_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve_msa.h

# common (dspr2)
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve_common_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_avg_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_avg_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_vert_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_avg_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_avg_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_vert_dspr2.c

# loop filters
DSP_SRCS-yes += loopfilter.c

DSP_SRCS-$(ARCH_X86)$(ARCH_X86_64)   += x86/loopfilter_sse2.c
DSP_SRCS-$(HAVE_SSE2)                += x86/lpf_common_sse2.h

ifneq ($(CONFIG_PARALLEL_DEBLOCKING),yes)
DSP_SRCS-$(HAVE_AVX2)   += x86/loopfilter_avx2.c

DSP_SRCS-$(HAVE_NEON)   += arm/loopfilter_neon.c
ifeq ($(HAVE_NEON_ASM),yes)
DSP_SRCS-yes  += arm/loopfilter_mb_neon$(ASM)
DSP_SRCS-yes  += arm/loopfilter_16_neon$(ASM)
DSP_SRCS-yes  += arm/loopfilter_8_neon$(ASM)
DSP_SRCS-yes  += arm/loopfilter_4_neon$(ASM)
else
ifeq ($(HAVE_NEON),yes)
DSP_SRCS-yes   += arm/loopfilter_16_neon.c
DSP_SRCS-yes   += arm/loopfilter_8_neon.c
DSP_SRCS-yes   += arm/loopfilter_4_neon.c
endif  # HAVE_NEON
endif  # HAVE_NEON_ASM

DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_msa.h
DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_16_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_8_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_4_msa.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_filters_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_filters_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_macros_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_masks_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_mb_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_mb_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_mb_vert_dspr2.c
endif  # !CONFIG_PARALLEL_DEBLOCKING

ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_loopfilter_sse2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/highbd_loopfilter_avx2.c
endif  # CONFIG_HIGHBITDEPTH

DSP_SRCS-yes            += txfm_common.h
DSP_SRCS-yes            += x86/txfm_common_intrin.h
DSP_SRCS-$(HAVE_AVX2)   += x86/common_avx2.h
DSP_SRCS-$(HAVE_SSE2)   += x86/txfm_common_sse2.h
DSP_SRCS-$(HAVE_SSSE3)  += x86/obmc_intrinsic_ssse3.h
DSP_SRCS-$(HAVE_MSA)    += mips/txfm_macros_msa.h

# forward transform
ifneq ($(findstring yes,$(CONFIG_AV1)$(CONFIG_PVQ)),)
DSP_SRCS-$(HAVE_AVX2)   += x86/txfm_common_avx2.h
ifeq ($(CONFIG_AV1_ENCODER),yes)
DSP_SRCS-yes            += fwd_txfm.c
DSP_SRCS-yes            += fwd_txfm.h
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_txfm_sse2.h
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_txfm_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_dct32_8cols_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_txfm_impl_sse2.h
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_dct32x32_impl_sse2.h
ifeq ($(ARCH_X86_64),yes)
DSP_SRCS-$(HAVE_SSSE3)  += x86/fwd_txfm_ssse3_x86_64.asm
endif
DSP_SRCS-$(HAVE_AVX2)   += x86/fwd_txfm_avx2.h
DSP_SRCS-$(HAVE_AVX2)   += x86/fwd_txfm_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/fwd_dct32x32_impl_avx2.h
DSP_SRCS-$(HAVE_NEON)   += arm/fwd_txfm_neon.c
DSP_SRCS-$(HAVE_MSA)    += mips/fwd_txfm_msa.h
DSP_SRCS-$(HAVE_MSA)    += mips/fwd_txfm_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/fwd_dct32x32_msa.c
endif  # CONFIG_AV1_ENCODER
endif  # CONFIG_AV1

# inverse transform
ifeq ($(CONFIG_AV1), yes)
DSP_SRCS-yes            += inv_txfm.h
DSP_SRCS-yes            += inv_txfm.c
DSP_SRCS-$(HAVE_SSE2)   += x86/inv_txfm_sse2.h
DSP_SRCS-$(HAVE_SSE2)   += x86/inv_txfm_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/inv_wht_sse2.asm
DSP_SRCS-$(HAVE_SSSE3)  += x86/inv_txfm_ssse3.c
DSP_SRCS-$(HAVE_AVX2)   += x86/inv_txfm_common_avx2.h
DSP_SRCS-$(HAVE_AVX2)   += x86/inv_txfm_avx2.c

ifeq ($(HAVE_NEON_ASM),yes)
DSP_SRCS-yes  += arm/save_reg_neon$(ASM)
DSP_SRCS-yes  += arm/idct4x4_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct4x4_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct8x8_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct8x8_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct16x16_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct16x16_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct32x32_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct32x32_add_neon$(ASM)
else
ifeq ($(HAVE_NEON),yes)
DSP_SRCS-yes  += arm/idct4x4_1_add_neon.c
DSP_SRCS-yes  += arm/idct4x4_add_neon.c
DSP_SRCS-yes  += arm/idct8x8_1_add_neon.c
DSP_SRCS-yes  += arm/idct8x8_add_neon.c
DSP_SRCS-yes  += arm/idct16x16_1_add_neon.c
DSP_SRCS-yes  += arm/idct16x16_add_neon.c
DSP_SRCS-yes  += arm/idct32x32_1_add_neon.c
DSP_SRCS-yes  += arm/idct32x32_add_neon.c
endif  # HAVE_NEON
endif  # HAVE_NEON_ASM
DSP_SRCS-$(HAVE_NEON)  += arm/idct16x16_neon.c

DSP_SRCS-$(HAVE_MSA)   += mips/inv_txfm_msa.h
DSP_SRCS-$(HAVE_MSA)   += mips/idct4x4_msa.c
DSP_SRCS-$(HAVE_MSA)   += mips/idct8x8_msa.c
DSP_SRCS-$(HAVE_MSA)   += mips/idct16x16_msa.c
DSP_SRCS-$(HAVE_MSA)   += mips/idct32x32_msa.c

ifneq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_DSPR2) += mips/inv_txfm_dspr2.h
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans4_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans8_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans16_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans32_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans32_cols_dspr2.c
endif  # CONFIG_HIGHBITDEPTH

ifeq ($(CONFIG_LOOP_RESTORATION),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/aom_convolve_hip_sse2.c
ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSSE3)  += x86/aom_highbd_convolve_hip_ssse3.c
endif
endif  # CONFIG_LOOP_RESTORATION
endif  # CONFIG_AV1

# quantization
ifneq ($(filter yes,$(CONFIG_AV1_ENCODER)),)
DSP_SRCS-yes            += quantize.c
DSP_SRCS-yes            += quantize.h

DSP_SRCS-$(HAVE_SSE2)   += x86/quantize_sse2.c

DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_quantize_intrin_sse2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/highbd_quantize_intrin_avx2.c

ifeq ($(ARCH_X86_64),yes)
DSP_SRCS-$(HAVE_SSSE3)  += x86/quantize_ssse3_x86_64.asm
DSP_SRCS-$(HAVE_AVX)    += x86/quantize_avx_x86_64.asm
endif

# avg
DSP_SRCS-yes           += avg.c
DSP_SRCS-$(HAVE_SSE2)  += x86/avg_intrin_sse2.c
DSP_SRCS-$(HAVE_NEON)  += arm/avg_neon.c
DSP_SRCS-$(HAVE_NEON)  += arm/hadamard_neon.c
ifeq ($(ARCH_X86_64),yes)
DSP_SRCS-$(HAVE_SSSE3) += x86/avg_ssse3_x86_64.asm
endif

# high bit depth subtract
ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)  += x86/highbd_subtract_sse2.c
endif

endif  # CONFIG_AV1_ENCODER

ifeq ($(CONFIG_AV1_ENCODER),yes)
DSP_SRCS-yes            += sum_squares.c

DSP_SRCS-$(HAVE_SSE2)   += x86/sum_squares_sse2.c
endif # CONFIG_AV1_ENCODER

ifeq ($(CONFIG_AV1_ENCODER),yes)
DSP_SRCS-yes            += sad.c
DSP_SRCS-yes            += subtract.c

DSP_SRCS-$(HAVE_NEON)   += arm/sad4d_neon.c
DSP_SRCS-$(HAVE_NEON)   += arm/sad_neon.c
DSP_SRCS-$(HAVE_NEON)   += arm/subtract_neon.c

DSP_SRCS-$(HAVE_MSA)    += mips/sad_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/subtract_msa.c

DSP_SRCS-$(HAVE_SSE3)   += x86/sad_sse3.asm
DSP_SRCS-$(HAVE_SSSE3)  += x86/sad_ssse3.asm
DSP_SRCS-$(HAVE_SSE4_1) += x86/sad_sse4.asm
DSP_SRCS-$(HAVE_AVX2)   += x86/sad4d_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/sad_avx2.c

ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_AVX2)   += x86/sad_highbd_avx2.c
endif

ifeq ($(CONFIG_AV1_ENCODER),yes)
DSP_SRCS-$(HAVE_SSSE3)  += x86/masked_sad_intrin_ssse3.c
DSP_SRCS-$(HAVE_SSSE3)  += x86/masked_variance_intrin_ssse3.c
ifeq ($(CONFIG_MOTION_VAR),yes)
DSP_SRCS-$(HAVE_SSE4_1) += x86/obmc_sad_sse4.c
DSP_SRCS-$(HAVE_SSE4_1) += x86/obmc_variance_sse4.c
endif  #CONFIG_MOTION_VAR
ifeq ($(CONFIG_EXT_PARTITION),yes)
DSP_SRCS-$(HAVE_AVX2) += x86/sad_impl_avx2.c
endif
endif  #CONFIG_AV1_ENCODER

DSP_SRCS-$(HAVE_SSE)    += x86/sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE)    += x86/sad_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/sad_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/subtract_sse2.asm

ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_sad_sse2.asm
endif  # CONFIG_HIGHBITDEPTH

endif  # CONFIG_AV1_ENCODER

ifneq ($(filter yes,$(CONFIG_AV1_ENCODER)),)
DSP_SRCS-yes            += variance.c
DSP_SRCS-yes            += variance.h

DSP_SRCS-$(HAVE_NEON)   += arm/subpel_variance_neon.c
DSP_SRCS-$(HAVE_NEON)   += arm/variance_neon.c

DSP_SRCS-$(HAVE_MSA)    += mips/variance_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/sub_pixel_variance_msa.c

DSP_SRCS-$(HAVE_SSE)    += x86/variance_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/variance_sse2.c  # Contains SSE2 and SSSE3
DSP_SRCS-$(HAVE_SSE2)   += x86/halfpix_variance_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/halfpix_variance_impl_sse2.asm
DSP_SRCS-$(HAVE_AVX2)   += x86/variance_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/variance_impl_avx2.c

ifeq ($(ARCH_X86_64),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/ssim_opt_x86_64.asm
endif  # ARCH_X86_64

DSP_SRCS-$(HAVE_SSE)    += x86/subpel_variance_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/subpel_variance_sse2.asm  # Contains SSE2 and SSSE3

ifeq ($(CONFIG_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_variance_sse2.c
DSP_SRCS-$(HAVE_SSE4_1) += x86/highbd_variance_sse4.c
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_variance_impl_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_subpel_variance_impl_sse2.asm
endif  # CONFIG_HIGHBITDEPTH
endif  # CONFIG_AV1_ENCODER

DSP_SRCS-no += $(DSP_SRCS_REMOVE-yes)

DSP_SRCS-yes += aom_dsp_rtcd.c
DSP_SRCS-yes += aom_dsp_rtcd_defs.pl

DSP_SRCS-yes += aom_simd.h
DSP_SRCS-yes += aom_simd_inline.h
DSP_SRCS-yes += simd/v64_intrinsics.h
DSP_SRCS-yes += simd/v64_intrinsics_c.h
DSP_SRCS-yes += simd/v128_intrinsics.h
DSP_SRCS-yes += simd/v128_intrinsics_c.h
DSP_SRCS-yes += simd/v256_intrinsics.h
DSP_SRCS-yes += simd/v256_intrinsics_c.h
DSP_SRCS-yes += simd/v256_intrinsics_v128.h
DSP_SRCS-$(HAVE_SSE2) += simd/v64_intrinsics_x86.h
DSP_SRCS-$(HAVE_SSE2) += simd/v128_intrinsics_x86.h
DSP_SRCS-$(HAVE_SSE2) += simd/v256_intrinsics_x86.h
DSP_SRCS-$(HAVE_NEON) += simd/v64_intrinsics_arm.h
DSP_SRCS-$(HAVE_NEON) += simd/v128_intrinsics_arm.h
DSP_SRCS-$(HAVE_NEON) += simd/v256_intrinsics_arm.h

$(eval $(call rtcd_h_template,aom_dsp_rtcd,aom_dsp/aom_dsp_rtcd_defs.pl))
