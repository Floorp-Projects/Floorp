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
if (NOT AOM_AV1_AV1_CMAKE_)
set(AOM_AV1_AV1_CMAKE_ 1)

set(AOM_AV1_COMMON_SOURCES
    "${AOM_ROOT}/av1/av1_iface_common.h"
    "${AOM_ROOT}/av1/common/alloccommon.c"
    "${AOM_ROOT}/av1/common/alloccommon.h"
    # TODO(tomfinegan): Foward transform belongs in encoder.
    "${AOM_ROOT}/av1/common/av1_fwd_txfm1d.c"
    "${AOM_ROOT}/av1/common/av1_fwd_txfm1d.h"
    "${AOM_ROOT}/av1/common/av1_fwd_txfm2d.c"
    "${AOM_ROOT}/av1/common/av1_fwd_txfm1d_cfg.h"
    "${AOM_ROOT}/av1/common/av1_inv_txfm1d.c"
    "${AOM_ROOT}/av1/common/av1_inv_txfm1d.h"
    "${AOM_ROOT}/av1/common/av1_inv_txfm2d.c"
    "${AOM_ROOT}/av1/common/av1_inv_txfm1d_cfg.h"
    "${AOM_ROOT}/av1/common/av1_loopfilter.c"
    "${AOM_ROOT}/av1/common/av1_loopfilter.h"
    "${AOM_ROOT}/av1/common/av1_txfm.h"
    "${AOM_ROOT}/av1/common/blockd.c"
    "${AOM_ROOT}/av1/common/blockd.h"
    "${AOM_ROOT}/av1/common/common.h"
    "${AOM_ROOT}/av1/common/common_data.h"
    "${AOM_ROOT}/av1/common/convolve.c"
    "${AOM_ROOT}/av1/common/convolve.h"
    "${AOM_ROOT}/av1/common/daala_tx.c"
    "${AOM_ROOT}/av1/common/daala_tx.h"
    "${AOM_ROOT}/av1/common/debugmodes.c"
    "${AOM_ROOT}/av1/common/entropy.c"
    "${AOM_ROOT}/av1/common/entropy.h"
    "${AOM_ROOT}/av1/common/entropymode.c"
    "${AOM_ROOT}/av1/common/entropymode.h"
    "${AOM_ROOT}/av1/common/entropymv.c"
    "${AOM_ROOT}/av1/common/entropymv.h"
    "${AOM_ROOT}/av1/common/enums.h"
    "${AOM_ROOT}/av1/common/filter.c"
    "${AOM_ROOT}/av1/common/filter.h"
    "${AOM_ROOT}/av1/common/frame_buffers.c"
    "${AOM_ROOT}/av1/common/frame_buffers.h"
    "${AOM_ROOT}/av1/common/idct.c"
    "${AOM_ROOT}/av1/common/idct.h"
    "${AOM_ROOT}/av1/common/mv.h"
    "${AOM_ROOT}/av1/common/mvref_common.c"
    "${AOM_ROOT}/av1/common/mvref_common.h"
    "${AOM_ROOT}/av1/common/odintrin.c"
    "${AOM_ROOT}/av1/common/odintrin.h"
    "${AOM_ROOT}/av1/common/onyxc_int.h"
    "${AOM_ROOT}/av1/common/pred_common.c"
    "${AOM_ROOT}/av1/common/pred_common.h"
    "${AOM_ROOT}/av1/common/quant_common.c"
    "${AOM_ROOT}/av1/common/quant_common.h"
    "${AOM_ROOT}/av1/common/reconinter.c"
    "${AOM_ROOT}/av1/common/reconinter.h"
    "${AOM_ROOT}/av1/common/reconintra.c"
    "${AOM_ROOT}/av1/common/reconintra.h"
    "${AOM_ROOT}/av1/common/resize.c"
    "${AOM_ROOT}/av1/common/resize.h"
    "${AOM_ROOT}/av1/common/scale.c"
    "${AOM_ROOT}/av1/common/scale.h"
    "${AOM_ROOT}/av1/common/scan.c"
    "${AOM_ROOT}/av1/common/scan.h"
    "${AOM_ROOT}/av1/common/seg_common.c"
    "${AOM_ROOT}/av1/common/seg_common.h"
    "${AOM_ROOT}/av1/common/thread_common.c"
    "${AOM_ROOT}/av1/common/thread_common.h"
    "${AOM_ROOT}/av1/common/tile_common.c"
    "${AOM_ROOT}/av1/common/tile_common.h")

set(AOM_AV1_DECODER_SOURCES
    "${AOM_ROOT}/av1/av1_dx_iface.c"
    "${AOM_ROOT}/av1/decoder/decodeframe.c"
    "${AOM_ROOT}/av1/decoder/decodeframe.h"
    "${AOM_ROOT}/av1/decoder/decodemv.c"
    "${AOM_ROOT}/av1/decoder/decodemv.h"
    "${AOM_ROOT}/av1/decoder/decoder.c"
    "${AOM_ROOT}/av1/decoder/decoder.h"
    "${AOM_ROOT}/av1/decoder/detokenize.c"
    "${AOM_ROOT}/av1/decoder/detokenize.h"
    "${AOM_ROOT}/av1/decoder/dsubexp.c"
    "${AOM_ROOT}/av1/decoder/dsubexp.h"
    "${AOM_ROOT}/av1/decoder/dthread.c"
    "${AOM_ROOT}/av1/decoder/dthread.h"
    "${AOM_ROOT}/av1/decoder/symbolrate.h")

set(AOM_AV1_ENCODER_SOURCES
    "${AOM_ROOT}/av1/av1_cx_iface.c"
    "${AOM_ROOT}/av1/encoder/aq_complexity.c"
    "${AOM_ROOT}/av1/encoder/aq_complexity.h"
    "${AOM_ROOT}/av1/encoder/aq_cyclicrefresh.c"
    "${AOM_ROOT}/av1/encoder/aq_cyclicrefresh.h"
    "${AOM_ROOT}/av1/encoder/aq_variance.c"
    "${AOM_ROOT}/av1/encoder/aq_variance.h"
    "${AOM_ROOT}/av1/encoder/av1_quantize.c"
    "${AOM_ROOT}/av1/encoder/av1_quantize.h"
    "${AOM_ROOT}/av1/encoder/bitstream.c"
    "${AOM_ROOT}/av1/encoder/bitstream.h"
    "${AOM_ROOT}/av1/encoder/block.h"
    "${AOM_ROOT}/av1/encoder/context_tree.c"
    "${AOM_ROOT}/av1/encoder/context_tree.h"
    "${AOM_ROOT}/av1/encoder/cost.c"
    "${AOM_ROOT}/av1/encoder/cost.h"
    "${AOM_ROOT}/av1/encoder/dct.c"
    "${AOM_ROOT}/av1/encoder/encodeframe.c"
    "${AOM_ROOT}/av1/encoder/encodeframe.h"
    "${AOM_ROOT}/av1/encoder/encodemb.c"
    "${AOM_ROOT}/av1/encoder/encodemb.h"
    "${AOM_ROOT}/av1/encoder/encodemv.c"
    "${AOM_ROOT}/av1/encoder/encodemv.h"
    "${AOM_ROOT}/av1/encoder/encoder.c"
    "${AOM_ROOT}/av1/encoder/encoder.h"
    "${AOM_ROOT}/av1/encoder/ethread.c"
    "${AOM_ROOT}/av1/encoder/ethread.h"
    "${AOM_ROOT}/av1/encoder/extend.c"
    "${AOM_ROOT}/av1/encoder/extend.h"
    "${AOM_ROOT}/av1/encoder/firstpass.c"
    "${AOM_ROOT}/av1/encoder/firstpass.h"
    "${AOM_ROOT}/av1/encoder/hash.c"
    "${AOM_ROOT}/av1/encoder/hash.h"
    "${AOM_ROOT}/av1/encoder/hybrid_fwd_txfm.c"
    "${AOM_ROOT}/av1/encoder/hybrid_fwd_txfm.h"
    "${AOM_ROOT}/av1/encoder/lookahead.c"
    "${AOM_ROOT}/av1/encoder/lookahead.h"
    "${AOM_ROOT}/av1/encoder/mbgraph.c"
    "${AOM_ROOT}/av1/encoder/mbgraph.h"
    "${AOM_ROOT}/av1/encoder/mcomp.c"
    "${AOM_ROOT}/av1/encoder/mcomp.h"
    "${AOM_ROOT}/av1/encoder/palette.c"
    "${AOM_ROOT}/av1/encoder/palette.h"
    "${AOM_ROOT}/av1/encoder/picklpf.c"
    "${AOM_ROOT}/av1/encoder/picklpf.h"
    "${AOM_ROOT}/av1/encoder/ratectrl.c"
    "${AOM_ROOT}/av1/encoder/ratectrl.h"
    "${AOM_ROOT}/av1/encoder/rd.c"
    "${AOM_ROOT}/av1/encoder/rd.h"
    "${AOM_ROOT}/av1/encoder/rdopt.c"
    "${AOM_ROOT}/av1/encoder/rdopt.h"
    "${AOM_ROOT}/av1/encoder/segmentation.c"
    "${AOM_ROOT}/av1/encoder/segmentation.h"
    "${AOM_ROOT}/av1/encoder/speed_features.c"
    "${AOM_ROOT}/av1/encoder/speed_features.h"
    "${AOM_ROOT}/av1/encoder/subexp.c"
    "${AOM_ROOT}/av1/encoder/subexp.h"
    "${AOM_ROOT}/av1/encoder/temporal_filter.c"
    "${AOM_ROOT}/av1/encoder/temporal_filter.h"
    "${AOM_ROOT}/av1/encoder/tokenize.c"
    "${AOM_ROOT}/av1/encoder/tokenize.h"
    "${AOM_ROOT}/av1/encoder/treewriter.c"
    "${AOM_ROOT}/av1/encoder/treewriter.h")

set(AOM_AV1_COMMON_INTRIN_SSE2
    "${AOM_ROOT}/av1/common/x86/idct_intrin_sse2.c")

set(AOM_AV1_COMMON_INTRIN_SSSE3
    "${AOM_ROOT}/av1/common/x86/av1_convolve_ssse3.c")

set(AOM_AV1_COMMON_INTRIN_SSE4_1
    "${AOM_ROOT}/av1/common/x86/av1_fwd_txfm1d_sse4.c"
    "${AOM_ROOT}/av1/common/x86/av1_fwd_txfm2d_sse4.c"
    "${AOM_ROOT}/av1/common/x86/highbd_inv_txfm_sse4.c")

set(AOM_AV1_COMMON_INTRIN_AVX2
    "${AOM_ROOT}/av1/common/x86/highbd_inv_txfm_avx2.c"
    "${AOM_ROOT}/av1/common/x86/hybrid_inv_txfm_avx2.c")

set(AOM_AV1_COMMON_INTRIN_MSA
    "${AOM_ROOT}/av1/common/mips/msa/av1_idct16x16_msa.c"
    "${AOM_ROOT}/av1/common/mips/msa/av1_idct4x4_msa.c"
    "${AOM_ROOT}/av1/common/mips/msa/av1_idct8x8_msa.c")

set(AOM_AV1_ENCODER_ASM_SSE2
    "${AOM_ROOT}/av1/encoder/x86/dct_sse2.asm"
    "${AOM_ROOT}/av1/encoder/x86/error_sse2.asm"
    "${AOM_ROOT}/av1/encoder/x86/temporal_filter_apply_sse2.asm")

set(AOM_AV1_ENCODER_INTRIN_SSE2
    "${AOM_ROOT}/av1/encoder/x86/dct_intrin_sse2.c"
    "${AOM_ROOT}/av1/encoder/x86/highbd_block_error_intrin_sse2.c"
    "${AOM_ROOT}/av1/encoder/x86/av1_quantize_sse2.c")

set(AOM_AV1_ENCODER_ASM_SSSE3_X86_64
    "${AOM_ROOT}/av1/encoder/x86/av1_quantize_ssse3_x86_64.asm")

set(AOM_AV1_ENCODER_INTRIN_SSE4_1
    ${AOM_AV1_ENCODER_INTRIN_SSE4_1}
    "${AOM_ROOT}/av1/encoder/x86/av1_highbd_quantize_sse4.c"
    "${AOM_ROOT}/av1/encoder/x86/highbd_fwd_txfm_sse4.c")

set(AOM_AV1_ENCODER_INTRIN_AVX2
    "${AOM_ROOT}/av1/encoder/x86/av1_quantize_avx2.c"
    "${AOM_ROOT}/av1/encoder/x86/av1_highbd_quantize_avx2.c"
    "${AOM_ROOT}/av1/encoder/x86/error_intrin_avx2.c"
    "${AOM_ROOT}/av1/encoder/x86/hybrid_fwd_txfm_avx2.c")

set(AOM_AV1_ENCODER_INTRIN_NEON
    "${AOM_ROOT}/av1/encoder/arm/neon/quantize_neon.c")

set(AOM_AV1_ENCODER_INTRIN_MSA
    "${AOM_ROOT}/av1/encoder/mips/msa/error_msa.c"
    "${AOM_ROOT}/av1/encoder/mips/msa/fdct16x16_msa.c"
    "${AOM_ROOT}/av1/encoder/mips/msa/fdct4x4_msa.c"
    "${AOM_ROOT}/av1/encoder/mips/msa/fdct8x8_msa.c"
    "${AOM_ROOT}/av1/encoder/mips/msa/fdct_msa.h"
    "${AOM_ROOT}/av1/encoder/mips/msa/temporal_filter_msa.c")

if (CONFIG_HIGHBITDEPTH)
  set(AOM_AV1_COMMON_INTRIN_SSE4_1
      ${AOM_AV1_COMMON_INTRIN_SSE4_1}
      "${AOM_ROOT}/av1/common/x86/av1_highbd_convolve_sse4.c")
else ()
  set(AOM_AV1_COMMON_INTRIN_NEON
      ${AOM_AV1_COMMON_INTRIN_NEON}
      "${AOM_ROOT}/av1/common/arm/neon/iht4x4_add_neon.c"
      "${AOM_ROOT}/av1/common/arm/neon/iht8x8_add_neon.c")

  set(AOM_AV1_ENCODER_INTRIN_NEON
      ${AOM_AV1_ENCODER_INTRIN_NEON}
      "${AOM_ROOT}/av1/encoder/arm/neon/error_neon.c")
endif ()

if (CONFIG_CDEF)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/cdef.c"
      "${AOM_ROOT}/av1/common/cdef.h"
      "${AOM_ROOT}/av1/common/cdef_block.c"
      "${AOM_ROOT}/av1/common/cdef_block.h")

  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/pickcdef.c")

  set(AOM_AV1_COMMON_INTRIN_SSE2
      ${AOM_AV1_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/av1/common/cdef_block_sse2.c")

  set(AOM_AV1_COMMON_INTRIN_SSSE3
      ${AOM_AV1_COMMON_INTRIN_SSSE3}
      "${AOM_ROOT}/av1/common/cdef_block_ssse3.c")

  set(AOM_AV1_COMMON_INTRIN_SSE4_1
      ${AOM_AV1_COMMON_INTRIN_SSE4_1}
      "${AOM_ROOT}/av1/common/cdef_block_sse4.c")

  set(AOM_AV1_COMMON_INTRIN_AVX2
      ${AOM_AV1_COMMON_INTRIN_AVX2}
      "${AOM_ROOT}/av1/common/cdef_block_avx2.c")

  set(AOM_AV1_COMMON_INTRIN_NEON
      ${AOM_AV1_COMMON_INTRIN_NEON}
      "${AOM_ROOT}/av1/common/cdef_block_neon.c")

  if (NOT CONFIG_CDEF_SINGLEPASS)
    set(AOM_AV1_COMMON_SOURCES
        ${AOM_AV1_COMMON_SOURCES}
        "${AOM_ROOT}/av1/common/clpf.c"
        "${AOM_ROOT}/av1/common/clpf_simd.h"
        "${AOM_ROOT}/av1/common/cdef_block_simd.h")

    set(AOM_AV1_COMMON_INTRIN_SSE2
        ${AOM_AV1_COMMON_INTRIN_SSE2}
        "${AOM_ROOT}/av1/common/clpf_sse2.c")

    set(AOM_AV1_COMMON_INTRIN_SSSE3
        ${AOM_AV1_COMMON_INTRIN_SSSE3}
        "${AOM_ROOT}/av1/common/clpf_ssse3.c")

    set(AOM_AV1_COMMON_INTRIN_SSE4_1
        ${AOM_AV1_COMMON_INTRIN_SSE4_1}
        "${AOM_ROOT}/av1/common/clpf_sse4.c")

    set(AOM_AV1_COMMON_INTRIN_NEON
        ${AOM_AV1_COMMON_INTRIN_NEON}
        "${AOM_ROOT}/av1/common/clpf_neon.c")
  endif ()
endif ()

if (CONFIG_CONVOLVE_ROUND)
  set(AOM_AV1_COMMON_INTRIN_SSE2
      ${AOM_AV1_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/av1/common/x86/convolve_2d_sse2.c")
  if (CONFIG_HIGHBITDEPTH)
    set(AOM_AV1_COMMON_INTRIN_SSSE3
        ${AOM_AV1_COMMON_INTRIN_SSSE3}
        "${AOM_ROOT}/av1/common/x86/highbd_convolve_2d_ssse3.c")
  endif ()

  if(NOT CONFIG_COMPOUND_ROUND)
    set(AOM_AV1_COMMON_INTRIN_SSE4_1
        ${AOM_AV1_COMMON_INTRIN_SSE4_1}
        "${AOM_ROOT}/av1/common/x86/av1_convolve_scale_sse4.c")
  endif()

  set(AOM_AV1_COMMON_INTRIN_AVX2
      ${AOM_AV1_COMMON_INTRIN_AVX2}
      "${AOM_ROOT}/av1/common/x86/convolve_avx2.c")
endif ()

  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/wedge_utils.c")

  set(AOM_AV1_ENCODER_INTRIN_SSE2
      ${AOM_AV1_ENCODER_INTRIN_SSE2}
      "${AOM_ROOT}/av1/encoder/x86/wedge_utils_sse2.c")

if (CONFIG_FILTER_INTRA)
  set(AOM_AV1_COMMON_INTRIN_SSE4_1
      ${AOM_AV1_COMMON_INTRIN_SSE4_1}
      "${AOM_ROOT}/av1/common/x86/filterintra_sse4.c")
endif ()

if (CONFIG_ACCOUNTING)
  set(AOM_AV1_DECODER_SOURCES
      ${AOM_AV1_DECODER_SOURCES}
      "${AOM_ROOT}/av1/decoder/accounting.c"
      "${AOM_ROOT}/av1/decoder/accounting.h")
endif ()

if (CONFIG_BGSPRITE)
  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/bgsprite.c"
      "${AOM_ROOT}/av1/encoder/bgsprite.h")
endif ()

if (CONFIG_GLOBAL_MOTION)
  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/corner_detect.c"
      "${AOM_ROOT}/av1/encoder/corner_detect.h"
      "${AOM_ROOT}/av1/encoder/corner_match.c"
      "${AOM_ROOT}/av1/encoder/corner_match.h"
      "${AOM_ROOT}/av1/encoder/global_motion.c"
      "${AOM_ROOT}/av1/encoder/global_motion.h"
      "${AOM_ROOT}/av1/encoder/ransac.c"
      "${AOM_ROOT}/av1/encoder/ransac.h"
      "${AOM_ROOT}/third_party/fastfeat/fast_9.c"
      "${AOM_ROOT}/third_party/fastfeat/fast.c"
      "${AOM_ROOT}/third_party/fastfeat/fast.h"
      "${AOM_ROOT}/third_party/fastfeat/nonmax.c")

  set(AOM_AV1_ENCODER_INTRIN_SSE4_1
      ${AOM_AV1_ENCODER_INTRIN_SSE4_1}
      "${AOM_ROOT}/av1/encoder/x86/corner_match_sse4.c")
endif ()

if (CONFIG_INSPECTION)
  set(AOM_AV1_DECODER_SOURCES
      ${AOM_AV1_DECODER_SOURCES}
      "${AOM_ROOT}/av1/decoder/inspection.c"
      "${AOM_ROOT}/av1/decoder/inspection.h")
endif ()

if (CONFIG_INTERNAL_STATS)
  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/blockiness.c")
endif ()

if (CONFIG_LV_MAP)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/txb_common.c"
      "${AOM_ROOT}/av1/common/txb_common.h")

  set(AOM_AV1_DECODER_SOURCES
      ${AOM_AV1_DECODER_SOURCES}
      "${AOM_ROOT}/av1/decoder/decodetxb.c"
      "${AOM_ROOT}/av1/decoder/decodetxb.h")

  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/encodetxb.c"
      "${AOM_ROOT}/av1/encoder/encodetxb.h")
endif ()

if (CONFIG_CFL)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
    "${AOM_ROOT}/av1/common/cfl.c"
    "${AOM_ROOT}/av1/common/cfl.h")
endif ()

if (CONFIG_LOOP_RESTORATION)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/restoration.c"
      "${AOM_ROOT}/av1/common/restoration.h")

  set(AOM_AV1_COMMON_INTRIN_SSE4_1
      ${AOM_AV1_COMMON_INTRIN_SSE4_1}
      "${AOM_ROOT}/av1/common/x86/selfguided_sse4.c")

  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/pickrst.c"
      "${AOM_ROOT}/av1/encoder/pickrst.h")
endif ()

if (CONFIG_INTRA_EDGE)
  set(AOM_AV1_COMMON_INTRIN_SSE4_1
      ${AOM_AV1_COMMON_INTRIN_SSE4_1}
      "${AOM_ROOT}/av1/common/x86/intra_edge_sse4.c")
endif ()

if (CONFIG_NCOBMC_ADAPT_WEIGHT)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/ncobmc_kernels.c"
      "${AOM_ROOT}/av1/common/ncobmc_kernels.h")
endif ()

if (CONFIG_PVQ)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/laplace_tables.c"
      "${AOM_ROOT}/av1/common/pvq.c"
      "${AOM_ROOT}/av1/common/pvq.h"
      "${AOM_ROOT}/av1/common/pvq_state.c"
      "${AOM_ROOT}/av1/common/pvq_state.h"
      "${AOM_ROOT}/av1/common/partition.c"
      "${AOM_ROOT}/av1/common/partition.h"
      "${AOM_ROOT}/av1/common/generic_code.c"
      "${AOM_ROOT}/av1/common/generic_code.h"
      "${AOM_ROOT}/av1/common/zigzag4.c"
      "${AOM_ROOT}/av1/common/zigzag8.c"
      "${AOM_ROOT}/av1/common/zigzag16.c"
      "${AOM_ROOT}/av1/common/zigzag32.c")

    set(AOM_AV1_DECODER_SOURCES
        ${AOM_AV1_DECODER_SOURCES}
        "${AOM_ROOT}/av1/decoder/decint.h"
        "${AOM_ROOT}/av1/decoder/pvq_decoder.c"
        "${AOM_ROOT}/av1/decoder/pvq_decoder.h"
        "${AOM_ROOT}/av1/decoder/generic_decoder.c"
        "${AOM_ROOT}/av1/decoder/laplace_decoder.c")

    set(AOM_AV1_ENCODER_SOURCES
        ${AOM_AV1_ENCODER_SOURCES}
        "${AOM_ROOT}/av1/encoder/daala_compat_enc.c"
        "${AOM_ROOT}/av1/encoder/encint.h"
        "${AOM_ROOT}/av1/encoder/pvq_encoder.c"
        "${AOM_ROOT}/av1/encoder/pvq_encoder.h"
        "${AOM_ROOT}/av1/encoder/generic_encoder.c"
        "${AOM_ROOT}/av1/encoder/laplace_encoder.c")

    set(AOM_AV1_COMMON_INTRIN_SSE4_1
        ${AOM_AV1_COMMON_INTRIN_SSE4_1}
        "${AOM_ROOT}/av1/common/x86/pvq_sse4.c"
        "${AOM_ROOT}/av1/common/x86/pvq_sse4.h")

    if (NOT CONFIG_AV1_ENCODER)
      # TODO(tomfinegan): These should probably be in av1/common, and in a
      # common source list. For now this mirrors the original build system.
      set(AOM_AV1_DECODER_SOURCES
          ${AOM_AV1_DECODER_SOURCES}
          "${AOM_ROOT}/av1/encoder/dct.c"
          "${AOM_ROOT}/av1/encoder/hybrid_fwd_txfm.c"
          "${AOM_ROOT}/av1/encoder/hybrid_fwd_txfm.h")

      set(AOM_AV1_DECODER_ASM_SSE2
          ${AOM_AV1_DECODER_ASM_SSE2}
          "${AOM_ROOT}/av1/encoder/x86/dct_sse2.asm")

      set(AOM_AV1_DECODER_INTRIN_SSE2
          ${AOM_AV1_DECODER_INTRIN_SSE2}
          "${AOM_ROOT}/av1/encoder/x86/dct_intrin_sse2.c")

    endif ()
endif ()

if (CONFIG_WARPED_MOTION OR CONFIG_GLOBAL_MOTION)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/warped_motion.c"
      "${AOM_ROOT}/av1/common/warped_motion.h")

  set(AOM_AV1_COMMON_INTRIN_SSE2
      ${AOM_AV1_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/av1/common/x86/warp_plane_sse2.c")

  set(AOM_AV1_COMMON_INTRIN_SSSE3
      ${AOM_AV1_COMMON_INTRIN_SSSE3}
      "${AOM_ROOT}/av1/common/x86/warp_plane_ssse3.c")

  if (CONFIG_HIGHBITDEPTH)
    set(AOM_AV1_COMMON_INTRIN_SSSE3
        ${AOM_AV1_COMMON_INTRIN_SSSE3}
        "${AOM_ROOT}/av1/common/x86/highbd_warp_plane_ssse3.c")
  endif ()
endif ()

if (CONFIG_HASH_ME)
  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/hash_motion.h"
      "${AOM_ROOT}/av1/encoder/hash_motion.c"
      "${AOM_ROOT}/third_party/vector/vector.h"
      "${AOM_ROOT}/third_party/vector/vector.c")
endif ()

if (CONFIG_Q_ADAPT_PROBS)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/token_cdfs.h")
endif ()

if (CONFIG_XIPHRC)
  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/ratectrl_xiph.c"
      "${AOM_ROOT}/av1/encoder/ratectrl_xiph.h")
endif ()

# Setup AV1 common/decoder/encoder targets. The libaom target must exist before
# this function is called.
function (setup_av1_targets)
  add_library(aom_av1_common OBJECT ${AOM_AV1_COMMON_SOURCES})
  list(APPEND AOM_LIB_TARGETS aom_av1_common)

  create_dummy_source_file("aom_av1" "c" "dummy_source_file")
  add_library(aom_av1 OBJECT "${dummy_source_file}")
  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_av1_common>)
  list(APPEND AOM_LIB_TARGETS aom_av1)

  # Not all generators support libraries consisting only of object files. Add a
  # dummy source file to the aom_av1 target.
  add_dummy_source_file_to_target("aom_av1" "c")

  if (CONFIG_AV1_DECODER)
    add_library(aom_av1_decoder OBJECT ${AOM_AV1_DECODER_SOURCES})
    set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_av1_decoder)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_av1_decoder>)
  endif ()

  if (CONFIG_AV1_ENCODER)
    add_library(aom_av1_encoder OBJECT ${AOM_AV1_ENCODER_SOURCES})
    set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_av1_encoder)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_av1_encoder>)
  endif ()

  if (HAVE_SSE2)
    require_compiler_flag_nomsvc("-msse2" NO)
    add_intrinsics_object_library("-msse2" "sse2" "aom_av1_common"
                                  "AOM_AV1_COMMON_INTRIN_SSE2" "aom")
    if (CONFIG_AV1_DECODER)
      if (AOM_AV1_DECODER_ASM_SSE2)
        add_asm_library("aom_av1_decoder_sse2" "AOM_AV1_DECODER_ASM_SSE2" "aom")
      endif ()

      if (AOM_AV1_DECODER_INTRIN_SSE2)
        add_intrinsics_object_library("-msse2" "sse2" "aom_av1_decoder"
                                      "AOM_AV1_DECODER_INTRIN_SSE2" "aom")
      endif ()
    endif ()

    if (CONFIG_AV1_ENCODER)
      add_asm_library("aom_av1_encoder_sse2" "AOM_AV1_ENCODER_ASM_SSE2" "aom")
      add_intrinsics_object_library("-msse2" "sse2" "aom_av1_encoder"
                                    "AOM_AV1_ENCODER_INTRIN_SSE2" "aom")
    endif ()
  endif ()

  if (HAVE_SSSE3)
    require_compiler_flag_nomsvc("-mssse3" NO)
    add_intrinsics_object_library("-mssse3" "ssse3" "aom_av1_common"
                                  "AOM_AV1_COMMON_INTRIN_SSSE3" "aom")

    if (CONFIG_AV1_DECODER)
      if (AOM_AV1_DECODER_INTRIN_SSSE3)
        add_intrinsics_object_library("-mssse3" "ssse3" "aom_av1_decoder"
                                      "AOM_AV1_DECODER_INTRIN_SSSE3" "aom")
      endif ()
    endif ()
  endif ()

  if (HAVE_SSE4_1)
    require_compiler_flag_nomsvc("-msse4.1" NO)
    add_intrinsics_object_library("-msse4.1" "sse4" "aom_av1_common"
                                  "AOM_AV1_COMMON_INTRIN_SSE4_1" "aom")

    if (CONFIG_AV1_ENCODER)
      if ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
        add_asm_library("aom_av1_encoder_ssse3"
                        "AOM_AV1_ENCODER_ASM_SSSE3_X86_64" "aom")
      endif ()

      if (AOM_AV1_ENCODER_INTRIN_SSE4_1)
        add_intrinsics_object_library("-msse4.1" "sse4" "aom_av1_encoder"
                                      "AOM_AV1_ENCODER_INTRIN_SSE4_1" "aom")
      endif ()
    endif ()
  endif ()

  if (HAVE_AVX2)
    require_compiler_flag_nomsvc("-mavx2" NO)
    add_intrinsics_object_library("-mavx2" "avx2" "aom_av1_common"
                                  "AOM_AV1_COMMON_INTRIN_AVX2" "aom")

    if (CONFIG_AV1_ENCODER)
      add_intrinsics_object_library("-mavx2" "avx2" "aom_av1_encoder"
                                    "AOM_AV1_ENCODER_INTRIN_AVX2" "aom")
    endif ()
  endif ()

  if (HAVE_NEON)
    if (AOM_AV1_COMMON_INTRIN_NEON)
      add_intrinsics_object_library("${AOM_INTRIN_NEON_FLAG}"
                                    "neon"
                                    "aom_av1_common"
                                    "AOM_AV1_COMMON_INTRIN_NEON" "aom")
    endif ()

    if (AOM_AV1_ENCODER_INTRIN_NEON)
      add_intrinsics_object_library("${AOM_INTRIN_NEON_FLAG}"
                                    "neon"
                                    "aom_av1_encoder"
                                    "AOM_AV1_ENCODER_INTRIN_NEON" "aom")
    endif ()
  endif ()

  if (HAVE_MSA)
    add_intrinsics_object_library("" "msa" "aom_av1_common"
                                  "AOM_AV1_COMMON_INTRIN_MSA" "aom")
    add_intrinsics_object_library("" "msa" "aom_av1_encoder"
                                  "AOM_AV1_ENCODER_INTRIN_MSA" "aom")
  endif ()

  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_dsp>)
  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_scale>)

  # Pass the new lib targets up to the parent scope instance of
  # $AOM_LIB_TARGETS.
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction ()

function (setup_av1_test_targets)
endfunction ()

endif ()  # AOM_AV1_AV1_CMAKE_
