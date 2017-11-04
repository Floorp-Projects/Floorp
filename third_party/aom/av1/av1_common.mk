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

AV1_COMMON_SRCS-yes += av1_common.mk
AV1_COMMON_SRCS-yes += av1_iface_common.h
AV1_COMMON_SRCS-yes += common/alloccommon.c
AV1_COMMON_SRCS-yes += common/av1_loopfilter.c
AV1_COMMON_SRCS-yes += common/av1_loopfilter.h
AV1_COMMON_SRCS-yes += common/blockd.c
AV1_COMMON_SRCS-yes += common/debugmodes.c
AV1_COMMON_SRCS-yes += common/entropy.c
AV1_COMMON_SRCS-yes += common/entropymode.c
AV1_COMMON_SRCS-yes += common/entropymv.c
AV1_COMMON_SRCS-yes += common/frame_buffers.c
AV1_COMMON_SRCS-yes += common/frame_buffers.h
AV1_COMMON_SRCS-yes += common/alloccommon.h
AV1_COMMON_SRCS-yes += common/blockd.h
AV1_COMMON_SRCS-yes += common/common.h
AV1_COMMON_SRCS-yes += common/daala_tx.c
AV1_COMMON_SRCS-yes += common/daala_tx.h
AV1_COMMON_SRCS-yes += common/entropy.h
AV1_COMMON_SRCS-yes += common/entropymode.h
AV1_COMMON_SRCS-yes += common/entropymv.h
AV1_COMMON_SRCS-yes += common/enums.h
AV1_COMMON_SRCS-yes += common/filter.h
AV1_COMMON_SRCS-yes += common/filter.c
AV1_COMMON_SRCS-yes += common/idct.h
AV1_COMMON_SRCS-yes += common/idct.c
AV1_COMMON_SRCS-yes += common/thread_common.h
AV1_COMMON_SRCS-$(CONFIG_LV_MAP) += common/txb_common.h
AV1_COMMON_SRCS-$(CONFIG_LV_MAP) += common/txb_common.c
AV1_COMMON_SRCS-yes += common/mv.h
AV1_COMMON_SRCS-yes += common/onyxc_int.h
AV1_COMMON_SRCS-yes += common/pred_common.h
AV1_COMMON_SRCS-yes += common/pred_common.c
AV1_COMMON_SRCS-yes += common/quant_common.h
AV1_COMMON_SRCS-yes += common/reconinter.h
AV1_COMMON_SRCS-yes += common/reconintra.h
AV1_COMMON_SRCS-yes += common/av1_rtcd.c
AV1_COMMON_SRCS-yes += common/av1_rtcd_defs.pl
AV1_COMMON_SRCS-yes += common/scale.h
AV1_COMMON_SRCS-yes += common/scale.c
AV1_COMMON_SRCS-yes += common/seg_common.h
AV1_COMMON_SRCS-yes += common/seg_common.c
AV1_COMMON_SRCS-yes += common/tile_common.h
AV1_COMMON_SRCS-yes += common/tile_common.c
AV1_COMMON_SRCS-yes += common/thread_common.c
AV1_COMMON_SRCS-yes += common/mvref_common.c
AV1_COMMON_SRCS-yes += common/mvref_common.h
AV1_COMMON_SRCS-yes += common/quant_common.c
AV1_COMMON_SRCS-yes += common/reconinter.c
AV1_COMMON_SRCS-yes += common/reconintra.c
AV1_COMMON_SRCS-yes += common/resize.c
AV1_COMMON_SRCS-yes += common/resize.h
AV1_COMMON_SRCS-yes += common/common_data.h
AV1_COMMON_SRCS-yes += common/scan.c
AV1_COMMON_SRCS-yes += common/scan.h
# TODO(angiebird) the forward transform belongs under encoder/
AV1_COMMON_SRCS-yes += common/av1_txfm.h
AV1_COMMON_SRCS-yes += common/av1_fwd_txfm1d.h
AV1_COMMON_SRCS-yes += common/av1_fwd_txfm1d.c
AV1_COMMON_SRCS-yes += common/av1_inv_txfm1d.h
AV1_COMMON_SRCS-yes += common/av1_inv_txfm1d.c
AV1_COMMON_SRCS-yes += common/av1_fwd_txfm2d.c
AV1_COMMON_SRCS-yes += common/av1_fwd_txfm1d_cfg.h
AV1_COMMON_SRCS-yes += common/av1_inv_txfm2d.c
AV1_COMMON_SRCS-yes += common/av1_inv_txfm1d_cfg.h
AV1_COMMON_SRCS-$(HAVE_AVX2) += common/x86/convolve_avx2.c
AV1_COMMON_SRCS-$(HAVE_SSSE3) += common/x86/av1_convolve_ssse3.c
ifeq ($(CONFIG_CONVOLVE_ROUND)x$(CONFIG_COMPOUND_ROUND),yesx)
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/av1_convolve_scale_sse4.c
endif
ifeq ($(CONFIG_HIGHBITDEPTH),yes)
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/av1_highbd_convolve_sse4.c
endif
AV1_COMMON_SRCS-yes += common/convolve.c
AV1_COMMON_SRCS-yes += common/convolve.h
ifeq ($(CONFIG_LOOP_RESTORATION),yes)
AV1_COMMON_SRCS-yes += common/restoration.h
AV1_COMMON_SRCS-yes += common/restoration.c
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/selfguided_sse4.c
endif
ifeq ($(CONFIG_INTRA_EDGE),yes)
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/intra_edge_sse4.c
endif
ifeq (yes,$(filter $(CONFIG_GLOBAL_MOTION) $(CONFIG_WARPED_MOTION),yes))
AV1_COMMON_SRCS-yes += common/warped_motion.h
AV1_COMMON_SRCS-yes += common/warped_motion.c
endif
ifeq ($(CONFIG_CDEF),yes)
ifeq ($(CONFIG_CDEF_SINGLEPASS),yes)
AV1_COMMON_SRCS-$(HAVE_AVX2) += common/cdef_block_avx2.c
else
AV1_COMMON_SRCS-yes += common/clpf.c
AV1_COMMON_SRCS-yes += common/clpf_simd.h
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/clpf_sse2.c
AV1_COMMON_SRCS-$(HAVE_SSSE3) += common/clpf_ssse3.c
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/clpf_sse4.c
AV1_COMMON_SRCS-$(HAVE_NEON) += common/clpf_neon.c
endif
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/cdef_block_sse2.c
AV1_COMMON_SRCS-$(HAVE_SSSE3) += common/cdef_block_ssse3.c
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/cdef_block_sse4.c
AV1_COMMON_SRCS-$(HAVE_NEON) += common/cdef_block_neon.c
AV1_COMMON_SRCS-yes += common/cdef_block.c
AV1_COMMON_SRCS-yes += common/cdef_block.h
AV1_COMMON_SRCS-yes += common/cdef_block_simd.h
AV1_COMMON_SRCS-yes += common/cdef.c
AV1_COMMON_SRCS-yes += common/cdef.h
endif
AV1_COMMON_SRCS-yes += common/odintrin.c
AV1_COMMON_SRCS-yes += common/odintrin.h

ifeq ($(CONFIG_CFL),yes)
AV1_COMMON_SRCS-yes += common/cfl.h
AV1_COMMON_SRCS-yes += common/cfl.c
endif

ifeq ($(CONFIG_MOTION_VAR),yes)
AV1_COMMON_SRCS-yes += common/obmc.h
endif

ifeq ($(CONFIG_PVQ),yes)
# PVQ from daala
AV1_COMMON_SRCS-yes += common/pvq.c
AV1_COMMON_SRCS-yes += common/partition.c
AV1_COMMON_SRCS-yes += common/partition.h
AV1_COMMON_SRCS-yes += common/zigzag4.c
AV1_COMMON_SRCS-yes += common/zigzag8.c
AV1_COMMON_SRCS-yes += common/zigzag16.c
AV1_COMMON_SRCS-yes += common/zigzag32.c
AV1_COMMON_SRCS-yes += common/zigzag.h
AV1_COMMON_SRCS-yes += common/generic_code.c
AV1_COMMON_SRCS-yes += common/pvq_state.c
AV1_COMMON_SRCS-yes += common/laplace_tables.c
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/pvq_sse4.c
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/pvq_sse4.h
endif
ifneq ($(findstring yes,$(CONFIG_PVQ)$(CONFIG_DAALA_DIST)$(CONFIG_XIPHRC)),)
AV1_COMMON_SRCS-yes += common/pvq.h
AV1_COMMON_SRCS-yes += common/pvq_state.h
AV1_COMMON_SRCS-yes += common/generic_code.h
endif

# common (msa)
AV1_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/av1_idct4x4_msa.c
AV1_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/av1_idct8x8_msa.c
AV1_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/av1_idct16x16_msa.c

AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/idct_intrin_sse2.c
AV1_COMMON_SRCS-$(HAVE_AVX2) += common/x86/hybrid_inv_txfm_avx2.c

ifeq ($(CONFIG_AV1_ENCODER),yes)
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/av1_txfm1d_sse4.h
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/av1_fwd_txfm1d_sse4.c
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/av1_fwd_txfm2d_sse4.c
endif

AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/highbd_txfm_utility_sse4.h
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/highbd_inv_txfm_sse4.c
AV1_COMMON_SRCS-$(HAVE_AVX2) += common/x86/highbd_inv_txfm_avx2.c

ifneq ($(CONFIG_HIGHBITDEPTH),yes)
AV1_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/iht4x4_add_neon.c
AV1_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/iht8x8_add_neon.c
endif

ifeq ($(CONFIG_FILTER_INTRA),yes)
AV1_COMMON_SRCS-$(HAVE_SSE4_1) += common/x86/filterintra_sse4.c
endif

ifneq ($(findstring yes,$(CONFIG_GLOBAL_MOTION) $(CONFIG_WARPED_MOTION)),)
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/warp_plane_sse2.c
AV1_COMMON_SRCS-$(HAVE_SSSE3) += common/x86/warp_plane_ssse3.c
ifeq ($(CONFIG_HIGHBITDEPTH),yes)
AV1_COMMON_SRCS-$(HAVE_SSSE3) += common/x86/highbd_warp_plane_ssse3.c
endif
endif

ifeq ($(CONFIG_CONVOLVE_ROUND),yes)
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/convolve_2d_sse2.c
ifeq ($(CONFIG_HIGHBITDEPTH),yes)
AV1_COMMON_SRCS-$(HAVE_SSSE3) += common/x86/highbd_convolve_2d_ssse3.c
endif
endif


ifeq ($(CONFIG_Q_ADAPT_PROBS),yes)
AV1_COMMON_SRCS-yes += common/token_cdfs.h
endif

ifeq ($(CONFIG_NCOBMC_ADAPT_WEIGHT),yes)
AV1_COMMON_SRCS-yes += common/ncobmc_kernels.h
AV1_COMMON_SRCS-yes += common/ncobmc_kernels.c
endif

$(eval $(call rtcd_h_template,av1_rtcd,av1/common/av1_rtcd_defs.pl))
