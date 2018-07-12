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

AV1_CX_EXPORTS += exports_enc

AV1_CX_SRCS-yes += $(AV1_COMMON_SRCS-yes)
AV1_CX_SRCS-no  += $(AV1_COMMON_SRCS-no)
AV1_CX_SRCS_REMOVE-yes += $(AV1_COMMON_SRCS_REMOVE-yes)
AV1_CX_SRCS_REMOVE-no  += $(AV1_COMMON_SRCS_REMOVE-no)

AV1_CX_SRCS-yes += av1_cx_iface.c

AV1_CX_SRCS-yes += encoder/av1_quantize.c
AV1_CX_SRCS-yes += encoder/av1_quantize.h
AV1_CX_SRCS-yes += encoder/bitstream.c
AV1_CX_SRCS-$(CONFIG_BGSPRITE) += encoder/bgsprite.c
AV1_CX_SRCS-$(CONFIG_BGSPRITE) += encoder/bgsprite.h
AV1_CX_SRCS-yes += encoder/context_tree.c
AV1_CX_SRCS-yes += encoder/context_tree.h
AV1_CX_SRCS-yes += encoder/cost.h
AV1_CX_SRCS-yes += encoder/cost.c
AV1_CX_SRCS-yes += encoder/dct.c
AV1_CX_SRCS-yes += encoder/hybrid_fwd_txfm.c
AV1_CX_SRCS-yes += encoder/hybrid_fwd_txfm.h
AV1_CX_SRCS-yes += encoder/encodeframe.c
AV1_CX_SRCS-yes += encoder/encodeframe.h
AV1_CX_SRCS-yes += encoder/encodemb.c
AV1_CX_SRCS-yes += encoder/encodemv.c
AV1_CX_SRCS-yes += encoder/ethread.h
AV1_CX_SRCS-yes += encoder/ethread.c
AV1_CX_SRCS-yes += encoder/extend.c
AV1_CX_SRCS-yes += encoder/firstpass.c
AV1_CX_SRCS-yes += encoder/mathutils.h
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += ../third_party/fastfeat/fast.h
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += ../third_party/fastfeat/nonmax.c
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += ../third_party/fastfeat/fast_9.c
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += ../third_party/fastfeat/fast.c
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/corner_match.c
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/corner_match.h
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/corner_detect.c
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/corner_detect.h
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/global_motion.c
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/global_motion.h
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/ransac.c
AV1_CX_SRCS-$(CONFIG_GLOBAL_MOTION) += encoder/ransac.h
AV1_CX_SRCS-yes += encoder/block.h
AV1_CX_SRCS-yes += encoder/bitstream.h
AV1_CX_SRCS-yes += encoder/encodemb.h
AV1_CX_SRCS-yes += encoder/encodemv.h
AV1_CX_SRCS-$(CONFIG_LV_MAP) += encoder/encodetxb.c
AV1_CX_SRCS-$(CONFIG_LV_MAP) += encoder/encodetxb.h
AV1_CX_SRCS-yes += encoder/extend.h
AV1_CX_SRCS-yes += encoder/firstpass.h
AV1_CX_SRCS-yes += encoder/lookahead.c
AV1_CX_SRCS-yes += encoder/lookahead.h
AV1_CX_SRCS-yes += encoder/mcomp.h
AV1_CX_SRCS-yes += encoder/encoder.h
AV1_CX_SRCS-yes += encoder/random.h
AV1_CX_SRCS-yes += encoder/ratectrl.h
ifeq ($(CONFIG_XIPHRC),yes)
AV1_CX_SRCS-yes += encoder/ratectrl_xiph.h
endif
AV1_CX_SRCS-yes += encoder/rd.h
AV1_CX_SRCS-yes += encoder/rdopt.h
AV1_CX_SRCS-yes += encoder/tokenize.h
AV1_CX_SRCS-yes += encoder/treewriter.h
AV1_CX_SRCS-yes += encoder/mcomp.c
AV1_CX_SRCS-yes += encoder/encoder.c
AV1_CX_SRCS-yes += encoder/k_means_template.h
AV1_CX_SRCS-yes += encoder/palette.h
AV1_CX_SRCS-yes += encoder/palette.c
AV1_CX_SRCS-yes += encoder/picklpf.c
AV1_CX_SRCS-yes += encoder/picklpf.h
AV1_CX_SRCS-$(CONFIG_LOOP_RESTORATION) += encoder/pickrst.c
AV1_CX_SRCS-$(CONFIG_LOOP_RESTORATION) += encoder/pickrst.h
AV1_CX_SRCS-yes += encoder/ratectrl.c
ifeq ($(CONFIG_XIPHRC),yes)
AV1_CX_SRCS-yes += encoder/ratectrl_xiph.c
endif
AV1_CX_SRCS-yes += encoder/rd.c
AV1_CX_SRCS-yes += encoder/rdopt.c
AV1_CX_SRCS-yes += encoder/segmentation.c
AV1_CX_SRCS-yes += encoder/segmentation.h
AV1_CX_SRCS-yes += encoder/speed_features.c
AV1_CX_SRCS-yes += encoder/speed_features.h
AV1_CX_SRCS-yes += encoder/subexp.c
AV1_CX_SRCS-yes += encoder/subexp.h
AV1_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/blockiness.c

AV1_CX_SRCS-yes += encoder/tokenize.c
AV1_CX_SRCS-yes += encoder/treewriter.c
AV1_CX_SRCS-yes += encoder/aq_variance.c
AV1_CX_SRCS-yes += encoder/aq_variance.h
AV1_CX_SRCS-yes += encoder/aq_cyclicrefresh.c
AV1_CX_SRCS-yes += encoder/aq_cyclicrefresh.h
AV1_CX_SRCS-yes += encoder/aq_complexity.c
AV1_CX_SRCS-yes += encoder/aq_complexity.h
AV1_CX_SRCS-yes += encoder/temporal_filter.c
AV1_CX_SRCS-yes += encoder/temporal_filter.h
AV1_CX_SRCS-yes += encoder/mbgraph.c
AV1_CX_SRCS-yes += encoder/mbgraph.h
AV1_CX_SRCS-yes += encoder/hash.c
AV1_CX_SRCS-yes += encoder/hash.h
ifeq ($(CONFIG_HASH_ME),yes)
AV1_CX_SRCS-yes += ../third_party/vector/vector.h
AV1_CX_SRCS-yes += ../third_party/vector/vector.c
AV1_CX_SRCS-yes += encoder/hash_motion.c
AV1_CX_SRCS-yes += encoder/hash_motion.h
endif
ifeq ($(CONFIG_CDEF),yes)
AV1_CX_SRCS-yes += encoder/pickcdef.c
endif
ifeq ($(CONFIG_PVQ),yes)
# PVQ from daala
AV1_CX_SRCS-yes += encoder/daala_compat_enc.c
AV1_CX_SRCS-yes += encoder/pvq_encoder.c
AV1_CX_SRCS-yes += encoder/pvq_encoder.h
AV1_CX_SRCS-yes += encoder/generic_encoder.c
AV1_CX_SRCS-yes += encoder/laplace_encoder.c
endif
ifneq ($(findstring yes,$(CONFIG_XIPHRC)$(CONFIG_PVQ)),)
AV1_CX_SRCS-yes += encoder/encint.h
endif

AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/av1_quantize_sse2.c
AV1_CX_SRCS-$(HAVE_AVX2) += encoder/x86/av1_quantize_avx2.c
AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/temporal_filter_apply_sse2.asm

AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/highbd_block_error_intrin_sse2.c
AV1_CX_SRCS-$(HAVE_AVX2) += encoder/x86/av1_highbd_quantize_avx2.c


AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/dct_sse2.asm
AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/error_sse2.asm

ifeq ($(ARCH_X86_64),yes)
AV1_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/av1_quantize_ssse3_x86_64.asm
endif

AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/dct_intrin_sse2.c
AV1_CX_SRCS-$(HAVE_AVX2) += encoder/x86/hybrid_fwd_txfm_avx2.c

AV1_CX_SRCS-$(HAVE_SSE4_1) += encoder/x86/av1_highbd_quantize_sse4.c

AV1_CX_SRCS-$(HAVE_SSE4_1) += encoder/x86/highbd_fwd_txfm_sse4.c

AV1_CX_SRCS-yes += encoder/wedge_utils.c
AV1_CX_SRCS-$(HAVE_SSE2) += encoder/x86/wedge_utils_sse2.c

AV1_CX_SRCS-$(HAVE_AVX2) += encoder/x86/error_intrin_avx2.c

ifneq ($(CONFIG_HIGHBITDEPTH),yes)
AV1_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/error_neon.c
endif
AV1_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/quantize_neon.c

AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/error_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct4x4_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct8x8_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct16x16_msa.c
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct_msa.h
AV1_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/temporal_filter_msa.c

ifeq ($(CONFIG_GLOBAL_MOTION),yes)
AV1_CX_SRCS-$(HAVE_SSE4_1) += encoder/x86/corner_match_sse4.c
endif

AV1_CX_SRCS-yes := $(filter-out $(AV1_CX_SRCS_REMOVE-yes),$(AV1_CX_SRCS-yes))
