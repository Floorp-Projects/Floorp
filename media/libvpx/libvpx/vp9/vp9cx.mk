##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

VP9_CX_EXPORTS += exports_enc

VP9_CX_SRCS-yes += $(VP9_COMMON_SRCS-yes)
VP9_CX_SRCS-no  += $(VP9_COMMON_SRCS-no)
VP9_CX_SRCS_REMOVE-yes += $(VP9_COMMON_SRCS_REMOVE-yes)
VP9_CX_SRCS_REMOVE-no  += $(VP9_COMMON_SRCS_REMOVE-no)

VP9_CX_SRCS-yes += vp9_cx_iface.c

VP9_CX_SRCS-yes += encoder/vp9_avg.c
VP9_CX_SRCS-yes += encoder/vp9_bitstream.c
VP9_CX_SRCS-yes += encoder/vp9_context_tree.c
VP9_CX_SRCS-yes += encoder/vp9_context_tree.h
VP9_CX_SRCS-yes += encoder/vp9_cost.h
VP9_CX_SRCS-yes += encoder/vp9_cost.c
VP9_CX_SRCS-yes += encoder/vp9_dct.c
VP9_CX_SRCS-yes += encoder/vp9_dct.h
VP9_CX_SRCS-$(CONFIG_VP9_TEMPORAL_DENOISING) += encoder/vp9_denoiser.c
VP9_CX_SRCS-$(CONFIG_VP9_TEMPORAL_DENOISING) += encoder/vp9_denoiser.h
VP9_CX_SRCS-yes += encoder/vp9_encodeframe.c
VP9_CX_SRCS-yes += encoder/vp9_encodeframe.h
VP9_CX_SRCS-yes += encoder/vp9_encodemb.c
VP9_CX_SRCS-yes += encoder/vp9_encodemv.c
VP9_CX_SRCS-yes += encoder/vp9_ethread.h
VP9_CX_SRCS-yes += encoder/vp9_ethread.c
VP9_CX_SRCS-yes += encoder/vp9_extend.c
VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/vp9_fastssim.c
VP9_CX_SRCS-yes += encoder/vp9_firstpass.c
VP9_CX_SRCS-yes += encoder/vp9_block.h
VP9_CX_SRCS-yes += encoder/vp9_writer.h
VP9_CX_SRCS-yes += encoder/vp9_writer.c
VP9_CX_SRCS-yes += encoder/vp9_write_bit_buffer.c
VP9_CX_SRCS-yes += encoder/vp9_write_bit_buffer.h
VP9_CX_SRCS-yes += encoder/vp9_bitstream.h
VP9_CX_SRCS-yes += encoder/vp9_encodemb.h
VP9_CX_SRCS-yes += encoder/vp9_encodemv.h
VP9_CX_SRCS-yes += encoder/vp9_extend.h
VP9_CX_SRCS-yes += encoder/vp9_firstpass.h
VP9_CX_SRCS-yes += encoder/vp9_lookahead.c
VP9_CX_SRCS-yes += encoder/vp9_lookahead.h
VP9_CX_SRCS-yes += encoder/vp9_mcomp.h
VP9_CX_SRCS-yes += encoder/vp9_encoder.h
VP9_CX_SRCS-yes += encoder/vp9_quantize.h
VP9_CX_SRCS-yes += encoder/vp9_ratectrl.h
VP9_CX_SRCS-yes += encoder/vp9_rd.h
VP9_CX_SRCS-yes += encoder/vp9_rdopt.h
VP9_CX_SRCS-yes += encoder/vp9_pickmode.h
VP9_CX_SRCS-yes += encoder/vp9_svc_layercontext.h
VP9_CX_SRCS-yes += encoder/vp9_tokenize.h
VP9_CX_SRCS-yes += encoder/vp9_treewriter.h
VP9_CX_SRCS-yes += encoder/vp9_variance.h
VP9_CX_SRCS-yes += encoder/vp9_mcomp.c
VP9_CX_SRCS-yes += encoder/vp9_encoder.c
VP9_CX_SRCS-yes += encoder/vp9_picklpf.c
VP9_CX_SRCS-yes += encoder/vp9_picklpf.h
VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/vp9_psnrhvs.c
VP9_CX_SRCS-yes += encoder/vp9_quantize.c
VP9_CX_SRCS-yes += encoder/vp9_ratectrl.c
VP9_CX_SRCS-yes += encoder/vp9_rd.c
VP9_CX_SRCS-yes += encoder/vp9_rdopt.c
VP9_CX_SRCS-yes += encoder/vp9_pickmode.c
VP9_CX_SRCS-yes += encoder/vp9_segmentation.c
VP9_CX_SRCS-yes += encoder/vp9_segmentation.h
VP9_CX_SRCS-yes += encoder/vp9_speed_features.c
VP9_CX_SRCS-yes += encoder/vp9_speed_features.h
VP9_CX_SRCS-yes += encoder/vp9_subexp.c
VP9_CX_SRCS-yes += encoder/vp9_subexp.h
VP9_CX_SRCS-yes += encoder/vp9_svc_layercontext.c
VP9_CX_SRCS-yes += encoder/vp9_resize.c
VP9_CX_SRCS-yes += encoder/vp9_resize.h
VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/vp9_ssim.c
VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/vp9_ssim.h
VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/vp9_blockiness.c

VP9_CX_SRCS-yes += encoder/vp9_tokenize.c
VP9_CX_SRCS-yes += encoder/vp9_treewriter.c
VP9_CX_SRCS-yes += encoder/vp9_variance.c
VP9_CX_SRCS-yes += encoder/vp9_aq_variance.c
VP9_CX_SRCS-yes += encoder/vp9_aq_variance.h
VP9_CX_SRCS-yes += encoder/vp9_aq_cyclicrefresh.c
VP9_CX_SRCS-yes += encoder/vp9_aq_cyclicrefresh.h
VP9_CX_SRCS-yes += encoder/vp9_aq_complexity.c
VP9_CX_SRCS-yes += encoder/vp9_aq_complexity.h
VP9_CX_SRCS-yes += encoder/vp9_skin_detection.c
VP9_CX_SRCS-yes += encoder/vp9_skin_detection.h
ifeq ($(CONFIG_VP9_POSTPROC),yes)
VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS) += common/vp9_postproc.h
VP9_CX_SRCS-$(CONFIG_INTERNAL_STATS) += common/vp9_postproc.c
endif
VP9_CX_SRCS-yes += encoder/vp9_temporal_filter.c
VP9_CX_SRCS-yes += encoder/vp9_temporal_filter.h
VP9_CX_SRCS-yes += encoder/vp9_mbgraph.c
VP9_CX_SRCS-yes += encoder/vp9_mbgraph.h

VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_avg_intrin_sse2.c
VP9_CX_SRCS-$(HAVE_AVX2) += encoder/x86/vp9_subpel_variance_impl_intrin_avx2.c
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_temporal_filter_apply_sse2.asm
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_quantize_sse2.c
ifeq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_highbd_quantize_intrin_sse2.c
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_highbd_block_error_intrin_sse2.c
endif

ifeq ($(CONFIG_USE_X86INC),yes)
VP9_CX_SRCS-$(HAVE_MMX) += encoder/x86/vp9_dct_mmx.asm
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_error_sse2.asm
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_subtract_sse2.asm
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_variance_sse2.c
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_subpel_variance.asm
ifeq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_highbd_variance_sse2.c
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_highbd_subpel_variance.asm
endif
endif

ifeq ($(ARCH_X86_64),yes)
VP9_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/vp9_quantize_ssse3_x86_64.asm
VP9_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/vp9_dct_ssse3_x86_64.asm
endif
VP9_CX_SRCS-$(ARCH_X86_64) += encoder/x86/vp9_ssim_opt_x86_64.asm

VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_dct_sse2.c
VP9_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/vp9_dct_ssse3.c
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_dct_sse2.h
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_dct32x32_sse2_impl.h
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_dct_sse2_impl.h

ifeq ($(CONFIG_VP9_TEMPORAL_DENOISING),yes)
VP9_CX_SRCS-$(HAVE_SSE2) += encoder/x86/vp9_denoiser_sse2.c
endif

VP9_CX_SRCS-$(HAVE_AVX2) += encoder/x86/vp9_dct32x32_avx2_impl.h
VP9_CX_SRCS-$(HAVE_AVX2) += encoder/x86/vp9_dct_avx2.c
VP9_CX_SRCS-$(HAVE_AVX2) += encoder/x86/vp9_error_intrin_avx2.c
VP9_CX_SRCS-$(HAVE_AVX2) += encoder/x86/vp9_variance_avx2.c

ifneq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP9_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/vp9_dct_neon.c
endif
VP9_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/vp9enc_avg_neon.c
VP9_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/vp9_quantize_neon.c
VP9_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/vp9_subtract_neon.c
VP9_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/vp9_variance_neon.c

VP9_CX_SRCS-yes := $(filter-out $(VP9_CX_SRCS_REMOVE-yes),$(VP9_CX_SRCS-yes))
