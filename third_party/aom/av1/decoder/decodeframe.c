/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <stdlib.h>  // qsort()

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_scale_rtcd.h"
#include "./av1_rtcd.h"

#include "aom/aom_codec.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_reader.h"
#include "aom_dsp/bitreader.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/mem_ops.h"
#include "aom_scale/aom_scale.h"
#include "aom_util/aom_thread.h"

#if CONFIG_BITSTREAM_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

#include "av1/common/alloccommon.h"
#if CONFIG_CDEF
#include "av1/common/cdef.h"
#endif
#if CONFIG_INSPECTION
#include "av1/decoder/inspection.h"
#endif
#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/idct.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#if CONFIG_FRAME_SUPERRES
#include "av1/common/resize.h"
#endif  // CONFIG_FRAME_SUPERRES
#include "av1/common/seg_common.h"
#include "av1/common/thread_common.h"
#include "av1/common/tile_common.h"

#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decodemv.h"
#include "av1/decoder/decoder.h"
#if CONFIG_LV_MAP
#include "av1/decoder/decodetxb.h"
#endif
#include "av1/decoder/detokenize.h"
#include "av1/decoder/dsubexp.h"
#include "av1/decoder/symbolrate.h"

#if CONFIG_WARPED_MOTION || CONFIG_GLOBAL_MOTION
#include "av1/common/warped_motion.h"
#endif  // CONFIG_WARPED_MOTION || CONFIG_GLOBAL_MOTION

#define MAX_AV1_HEADER_SIZE 80
#define ACCT_STR __func__

#if CONFIG_PVQ
#include "av1/common/partition.h"
#include "av1/common/pvq.h"
#include "av1/common/scan.h"
#include "av1/decoder/decint.h"
#include "av1/decoder/pvq_decoder.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#endif

#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif

#if CONFIG_STRIPED_LOOP_RESTORATION && !CONFIG_LOOP_RESTORATION
#error "striped_loop_restoration requires loop_restoration"
#endif

#if CONFIG_LOOP_RESTORATION
static void loop_restoration_read_sb_coeffs(const AV1_COMMON *const cm,
                                            MACROBLOCKD *xd,
                                            aom_reader *const r, int plane,
                                            int rtile_idx);
#endif

static struct aom_read_bit_buffer *init_read_bit_buffer(
    AV1Decoder *pbi, struct aom_read_bit_buffer *rb, const uint8_t *data,
    const uint8_t *data_end, uint8_t clear_data[MAX_AV1_HEADER_SIZE]);
static int read_compressed_header(AV1Decoder *pbi, const uint8_t *data,
                                  size_t partition_size);
static size_t read_uncompressed_header(AV1Decoder *pbi,
                                       struct aom_read_bit_buffer *rb);

static int is_compound_reference_allowed(const AV1_COMMON *cm) {
#if CONFIG_ONE_SIDED_COMPOUND  // Normative in decoder
  return !frame_is_intra_only(cm);
#else
  int i;
  if (frame_is_intra_only(cm)) return 0;
  for (i = 1; i < INTER_REFS_PER_FRAME; ++i)
    if (cm->ref_frame_sign_bias[i + 1] != cm->ref_frame_sign_bias[1]) return 1;

  return 0;
#endif  // CONFIG_ONE_SIDED_COMPOUND
}

static void setup_compound_reference_mode(AV1_COMMON *cm) {
#if CONFIG_EXT_REFS
  cm->comp_fwd_ref[0] = LAST_FRAME;
  cm->comp_fwd_ref[1] = LAST2_FRAME;
  cm->comp_fwd_ref[2] = LAST3_FRAME;
  cm->comp_fwd_ref[3] = GOLDEN_FRAME;

  cm->comp_bwd_ref[0] = BWDREF_FRAME;
  cm->comp_bwd_ref[1] = ALTREF2_FRAME;
  cm->comp_bwd_ref[2] = ALTREF_FRAME;
#else   // !CONFIG_EXT_REFS
  if (cm->ref_frame_sign_bias[LAST_FRAME] ==
      cm->ref_frame_sign_bias[GOLDEN_FRAME]) {
    cm->comp_fixed_ref = ALTREF_FRAME;
    cm->comp_var_ref[0] = LAST_FRAME;
    cm->comp_var_ref[1] = GOLDEN_FRAME;
  } else if (cm->ref_frame_sign_bias[LAST_FRAME] ==
             cm->ref_frame_sign_bias[ALTREF_FRAME]) {
    cm->comp_fixed_ref = GOLDEN_FRAME;
    cm->comp_var_ref[0] = LAST_FRAME;
    cm->comp_var_ref[1] = ALTREF_FRAME;
  } else {
    cm->comp_fixed_ref = LAST_FRAME;
    cm->comp_var_ref[0] = GOLDEN_FRAME;
    cm->comp_var_ref[1] = ALTREF_FRAME;
  }
#endif  // CONFIG_EXT_REFS
}

static int read_is_valid(const uint8_t *start, size_t len, const uint8_t *end) {
  return len != 0 && len <= (size_t)(end - start);
}

static int decode_unsigned_max(struct aom_read_bit_buffer *rb, int max) {
  const int data = aom_rb_read_literal(rb, get_unsigned_bits(max));
  return data > max ? max : data;
}

static TX_MODE read_tx_mode(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
#if CONFIG_TX64X64
  TX_MODE tx_mode;
#endif
  if (cm->all_lossless) return ONLY_4X4;
#if CONFIG_VAR_TX_NO_TX_MODE
  (void)rb;
  return TX_MODE_SELECT;
#else
#if CONFIG_TX64X64
  tx_mode = aom_rb_read_bit(rb) ? TX_MODE_SELECT : aom_rb_read_literal(rb, 2);
  if (tx_mode == ALLOW_32X32) tx_mode += aom_rb_read_bit(rb);
  return tx_mode;
#else
  return aom_rb_read_bit(rb) ? TX_MODE_SELECT : aom_rb_read_literal(rb, 2);
#endif  // CONFIG_TX64X64
#endif  // CONFIG_VAR_TX_NO_TX_MODE
}

#if !CONFIG_RESTRICT_COMPRESSED_HDR
static void read_inter_mode_probs(FRAME_CONTEXT *fc, aom_reader *r) {
  int i;
  for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i)
    av1_diff_update_prob(r, &fc->newmv_prob[i], ACCT_STR);
  for (i = 0; i < ZEROMV_MODE_CONTEXTS; ++i)
    av1_diff_update_prob(r, &fc->zeromv_prob[i], ACCT_STR);
  for (i = 0; i < REFMV_MODE_CONTEXTS; ++i)
    av1_diff_update_prob(r, &fc->refmv_prob[i], ACCT_STR);
  for (i = 0; i < DRL_MODE_CONTEXTS; ++i)
    av1_diff_update_prob(r, &fc->drl_prob[i], ACCT_STR);
}
#endif

static REFERENCE_MODE read_frame_reference_mode(
    const AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  if (is_compound_reference_allowed(cm)) {
#if CONFIG_REF_ADAPT
    return aom_rb_read_bit(rb) ? REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
#else
    return aom_rb_read_bit(rb)
               ? REFERENCE_MODE_SELECT
               : (aom_rb_read_bit(rb) ? COMPOUND_REFERENCE : SINGLE_REFERENCE);
#endif  // CONFIG_REF_ADAPT
  } else {
    return SINGLE_REFERENCE;
  }
}

#if !CONFIG_RESTRICT_COMPRESSED_HDR
static void read_frame_reference_mode_probs(AV1_COMMON *cm, aom_reader *r) {
  FRAME_CONTEXT *const fc = cm->fc;
  int i;

  if (cm->reference_mode == REFERENCE_MODE_SELECT)
    for (i = 0; i < COMP_INTER_CONTEXTS; ++i)
      av1_diff_update_prob(r, &fc->comp_inter_prob[i], ACCT_STR);

  if (cm->reference_mode != COMPOUND_REFERENCE) {
    for (i = 0; i < REF_CONTEXTS; ++i) {
      int j;
      for (j = 0; j < (SINGLE_REFS - 1); ++j) {
        av1_diff_update_prob(r, &fc->single_ref_prob[i][j], ACCT_STR);
      }
    }
  }

  if (cm->reference_mode != SINGLE_REFERENCE) {
#if CONFIG_EXT_COMP_REFS
    for (i = 0; i < COMP_REF_TYPE_CONTEXTS; ++i)
      av1_diff_update_prob(r, &fc->comp_ref_type_prob[i], ACCT_STR);

    for (i = 0; i < UNI_COMP_REF_CONTEXTS; ++i) {
      int j;
      for (j = 0; j < (UNIDIR_COMP_REFS - 1); ++j)
        av1_diff_update_prob(r, &fc->uni_comp_ref_prob[i][j], ACCT_STR);
    }
#endif  // CONFIG_EXT_COMP_REFS

    for (i = 0; i < REF_CONTEXTS; ++i) {
      int j;
#if CONFIG_EXT_REFS
      for (j = 0; j < (FWD_REFS - 1); ++j)
        av1_diff_update_prob(r, &fc->comp_ref_prob[i][j], ACCT_STR);
      for (j = 0; j < (BWD_REFS - 1); ++j)
        av1_diff_update_prob(r, &fc->comp_bwdref_prob[i][j], ACCT_STR);
#else
      for (j = 0; j < (COMP_REFS - 1); ++j)
        av1_diff_update_prob(r, &fc->comp_ref_prob[i][j], ACCT_STR);
#endif  // CONFIG_EXT_REFS
    }
  }
}

static void update_mv_probs(aom_prob *p, int n, aom_reader *r) {
  int i;
  for (i = 0; i < n; ++i) av1_diff_update_prob(r, &p[i], ACCT_STR);
}

static void read_mv_probs(nmv_context *ctx, int allow_hp, aom_reader *r) {
  int i;
  if (allow_hp) {
    for (i = 0; i < 2; ++i) {
      nmv_component *const comp_ctx = &ctx->comps[i];
      update_mv_probs(&comp_ctx->class0_hp, 1, r);
      update_mv_probs(&comp_ctx->hp, 1, r);
    }
  }
}
#endif

static void inverse_transform_block(MACROBLOCKD *xd, int plane,
#if CONFIG_LGT_FROM_PRED
                                    PREDICTION_MODE mode,
#endif
                                    const TX_TYPE tx_type,
                                    const TX_SIZE tx_size, uint8_t *dst,
                                    int stride, int16_t scan_line, int eob) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *const dqcoeff = pd->dqcoeff;
  av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_LGT_FROM_PRED
                              mode,
#endif
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              xd->mrc_mask,
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
                              tx_type, tx_size, dst, stride, eob);
  memset(dqcoeff, 0, (scan_line + 1) * sizeof(dqcoeff[0]));
}

static int get_block_idx(const MACROBLOCKD *xd, int plane, int row, int col) {
  const int bsize = xd->mi[0]->mbmi.sb_type;
  const struct macroblockd_plane *pd = &xd->plane[plane];
#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize =
      AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#elif CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(AOMMAX(BLOCK_8X8, bsize), pd);
#endif
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
  const uint8_t txh_unit = tx_size_high_unit[tx_size];
  return row * max_blocks_wide + col * txh_unit;
}

#if CONFIG_PVQ
static int av1_pvq_decode_helper(MACROBLOCKD *xd, tran_low_t *ref_coeff,
                                 tran_low_t *dqcoeff, int16_t *quant, int pli,
                                 int bs, TX_TYPE tx_type, int xdec,
                                 PVQ_SKIP_TYPE ac_dc_coded) {
  unsigned int flags;  // used for daala's stream analyzer.
  int off;
  const int is_keyframe = 0;
  const int has_dc_skip = 1;
  int coeff_shift = 3 - av1_get_tx_scale(bs);
  int hbd_downshift = 0;
  int rounding_mask;
  // DC quantizer for PVQ
  int pvq_dc_quant;
  int lossless = (quant[0] == 0);
  const int blk_size = tx_size_wide[bs];
  int eob = 0;
  int i;
  od_dec_ctx *dec = &xd->daala_dec;
  int use_activity_masking = dec->use_activity_masking;
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff_pvq[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);
  DECLARE_ALIGNED(16, tran_low_t, ref_coeff_pvq[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);

  od_coeff ref_int32[OD_TXSIZE_MAX * OD_TXSIZE_MAX];
  od_coeff out_int32[OD_TXSIZE_MAX * OD_TXSIZE_MAX];

  hbd_downshift = xd->bd - 8;

  od_raster_to_coding_order(ref_coeff_pvq, blk_size, tx_type, ref_coeff,
                            blk_size);

  assert(OD_COEFF_SHIFT >= 4);
  if (lossless)
    pvq_dc_quant = 1;
  else {
    if (use_activity_masking)
      pvq_dc_quant =
          OD_MAXI(1,
                  (quant[0] << (OD_COEFF_SHIFT - 3) >> hbd_downshift) *
                          dec->state.pvq_qm_q4[pli][od_qm_get_index(bs, 0)] >>
                      4);
    else
      pvq_dc_quant =
          OD_MAXI(1, quant[0] << (OD_COEFF_SHIFT - 3) >> hbd_downshift);
  }

  off = od_qm_offset(bs, xdec);

  // copy int16 inputs to int32
  for (i = 0; i < blk_size * blk_size; i++) {
    ref_int32[i] =
        AOM_SIGNED_SHL(ref_coeff_pvq[i], OD_COEFF_SHIFT - coeff_shift) >>
        hbd_downshift;
  }

  od_pvq_decode(dec, ref_int32, out_int32,
                OD_MAXI(1, quant[1] << (OD_COEFF_SHIFT - 3) >> hbd_downshift),
                pli, bs, OD_PVQ_BETA[use_activity_masking][pli][bs],
                is_keyframe, &flags, ac_dc_coded, dec->state.qm + off,
                dec->state.qm_inv + off);

  if (!has_dc_skip || out_int32[0]) {
    out_int32[0] =
        has_dc_skip + generic_decode(dec->r, &dec->state.adapt->model_dc[pli],
                                     &dec->state.adapt->ex_dc[pli][bs][0], 2,
                                     "dc:mag");
    if (out_int32[0]) out_int32[0] *= aom_read_bit(dec->r, "dc:sign") ? -1 : 1;
  }
  out_int32[0] = out_int32[0] * pvq_dc_quant + ref_int32[0];

  // copy int32 result back to int16
  assert(OD_COEFF_SHIFT > coeff_shift);
  rounding_mask = (1 << (OD_COEFF_SHIFT - coeff_shift - 1)) - 1;
  for (i = 0; i < blk_size * blk_size; i++) {
    out_int32[i] = AOM_SIGNED_SHL(out_int32[i], hbd_downshift);
    dqcoeff_pvq[i] = (out_int32[i] + (out_int32[i] < 0) + rounding_mask) >>
                     (OD_COEFF_SHIFT - coeff_shift);
  }

  od_coding_order_to_raster(dqcoeff, blk_size, tx_type, dqcoeff_pvq, blk_size);

  eob = blk_size * blk_size;

  return eob;
}

static PVQ_SKIP_TYPE read_pvq_skip(AV1_COMMON *cm, MACROBLOCKD *const xd,
                                   int plane, TX_SIZE tx_size) {
  // decode ac/dc coded flag. bit0: DC coded, bit1 : AC coded
  // NOTE : we don't use 5 symbols for luma here in aom codebase,
  // since block partition is taken care of by aom.
  // So, only AC/DC skip info is coded
  const int ac_dc_coded = aom_read_symbol(
      xd->daala_dec.r,
      xd->daala_dec.state.adapt->skip_cdf[2 * tx_size + (plane != 0)], 4,
      "skip");
  if (ac_dc_coded < 0 || ac_dc_coded > 3) {
    aom_internal_error(&cm->error, AOM_CODEC_INVALID_PARAM,
                       "Invalid PVQ Skip Type");
  }
  return ac_dc_coded;
}

static int av1_pvq_decode_helper2(AV1_COMMON *cm, MACROBLOCKD *const xd,
                                  MB_MODE_INFO *const mbmi, int plane, int row,
                                  int col, TX_SIZE tx_size, TX_TYPE tx_type) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  // transform block size in pixels
  int tx_blk_size = tx_size_wide[tx_size];
  int i, j;
  tran_low_t *pvq_ref_coeff = pd->pvq_ref_coeff;
  const int diff_stride = tx_blk_size;
  int16_t *pred = pd->pred;
  tran_low_t *const dqcoeff = pd->dqcoeff;
  uint8_t *dst;
  int eob;
  const PVQ_SKIP_TYPE ac_dc_coded = read_pvq_skip(cm, xd, plane, tx_size);

  eob = 0;
  dst = &pd->dst.buf[4 * row * pd->dst.stride + 4 * col];

  if (ac_dc_coded) {
    int xdec = pd->subsampling_x;
    int seg_id = mbmi->segment_id;
    int16_t *quant;
    TxfmParam txfm_param;
    // ToDo(yaowu): correct this with optimal number from decoding process.
    const int max_scan_line = tx_size_2d[tx_size];
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      for (j = 0; j < tx_blk_size; j++)
        for (i = 0; i < tx_blk_size; i++)
          pred[diff_stride * j + i] =
              CONVERT_TO_SHORTPTR(dst)[pd->dst.stride * j + i];
    } else {
#endif
      for (j = 0; j < tx_blk_size; j++)
        for (i = 0; i < tx_blk_size; i++)
          pred[diff_stride * j + i] = dst[pd->dst.stride * j + i];
#if CONFIG_HIGHBITDEPTH
    }
#endif

    txfm_param.tx_type = tx_type;
    txfm_param.tx_size = tx_size;
    txfm_param.lossless = xd->lossless[seg_id];

#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      txfm_param.bd = xd->bd;
      av1_highbd_fwd_txfm(pred, pvq_ref_coeff, diff_stride, &txfm_param);
    } else {
#endif  // CONFIG_HIGHBITDEPTH
      av1_fwd_txfm(pred, pvq_ref_coeff, diff_stride, &txfm_param);
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH

    quant = &pd->seg_dequant[seg_id][0];  // aom's quantizer

    eob = av1_pvq_decode_helper(xd, pvq_ref_coeff, dqcoeff, quant, plane,
                                tx_size, tx_type, xdec, ac_dc_coded);

    inverse_transform_block(xd, plane, tx_type, tx_size, dst, pd->dst.stride,
                            max_scan_line, eob);
  }

  return eob;
}
#endif

static void predict_and_reconstruct_intra_block(
    AV1_COMMON *cm, MACROBLOCKD *const xd, aom_reader *const r,
    MB_MODE_INFO *const mbmi, int plane, int row, int col, TX_SIZE tx_size) {
  PLANE_TYPE plane_type = get_plane_type(plane);
  const int block_idx = get_block_idx(xd, plane, row, col);
#if CONFIG_PVQ
  (void)r;
#endif
  av1_predict_intra_block_facade(cm, xd, plane, block_idx, col, row, tx_size);

  if (!mbmi->skip) {
#if !CONFIG_PVQ
    struct macroblockd_plane *const pd = &xd->plane[plane];
#if CONFIG_LV_MAP
    int16_t max_scan_line = 0;
    int eob;
    av1_read_coeffs_txb_facade(cm, xd, r, row, col, block_idx, plane,
                               pd->dqcoeff, tx_size, &max_scan_line, &eob);
    // tx_type will be read out in av1_read_coeffs_txb_facade
    const TX_TYPE tx_type =
        av1_get_tx_type(plane_type, xd, row, col, block_idx, tx_size);
#else   // CONFIG_LV_MAP
    const TX_TYPE tx_type =
        av1_get_tx_type(plane_type, xd, row, col, block_idx, tx_size);
    const SCAN_ORDER *scan_order = get_scan(cm, tx_size, tx_type, mbmi);
    int16_t max_scan_line = 0;
    const int eob =
        av1_decode_block_tokens(cm, xd, plane, scan_order, col, row, tx_size,
                                tx_type, &max_scan_line, r, mbmi->segment_id);
#endif  // CONFIG_LV_MAP
    if (eob) {
      uint8_t *dst =
          &pd->dst.buf[(row * pd->dst.stride + col) << tx_size_wide_log2[0]];
      inverse_transform_block(xd, plane,
#if CONFIG_LGT_FROM_PRED
                              mbmi->mode,
#endif
                              tx_type, tx_size, dst, pd->dst.stride,
                              max_scan_line, eob);
    }
#else   // !CONFIG_PVQ
    const TX_TYPE tx_type =
        av1_get_tx_type(plane_type, xd, row, col, block_idx, tx_size);
    av1_pvq_decode_helper2(cm, xd, mbmi, plane, row, col, tx_size, tx_type);
#endif  // !CONFIG_PVQ
  }
#if CONFIG_CFL
  if (plane == AOM_PLANE_Y && xd->cfl->store_y) {
    cfl_store_tx(xd, row, col, tx_size, mbmi->sb_type);
  }
#endif  // CONFIG_CFL
}

#if CONFIG_VAR_TX && !CONFIG_COEF_INTERLEAVE
static void decode_reconstruct_tx(AV1_COMMON *cm, MACROBLOCKD *const xd,
                                  aom_reader *r, MB_MODE_INFO *const mbmi,
                                  int plane, BLOCK_SIZE plane_bsize,
                                  int blk_row, int blk_col, int block,
                                  TX_SIZE tx_size, int *eob_total) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  const TX_SIZE plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];
  // Scale to match transform block unit.
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size) {
    PLANE_TYPE plane_type = get_plane_type(plane);
#if CONFIG_LV_MAP
    int16_t max_scan_line = 0;
    int eob;
    av1_read_coeffs_txb_facade(cm, xd, r, blk_row, blk_col, block, plane,
                               pd->dqcoeff, tx_size, &max_scan_line, &eob);
    // tx_type will be read out in av1_read_coeffs_txb_facade
    const TX_TYPE tx_type =
        av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, plane_tx_size);
#else   // CONFIG_LV_MAP
    const TX_TYPE tx_type =
        av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, plane_tx_size);
    const SCAN_ORDER *sc = get_scan(cm, plane_tx_size, tx_type, mbmi);
    int16_t max_scan_line = 0;
    const int eob = av1_decode_block_tokens(
        cm, xd, plane, sc, blk_col, blk_row, plane_tx_size, tx_type,
        &max_scan_line, r, mbmi->segment_id);
#endif  // CONFIG_LV_MAP
    inverse_transform_block(xd, plane,
#if CONFIG_LGT_FROM_PRED
                            mbmi->mode,
#endif
                            tx_type, plane_tx_size,
                            &pd->dst.buf[(blk_row * pd->dst.stride + blk_col)
                                         << tx_size_wide_log2[0]],
                            pd->dst.stride, max_scan_line, eob);
    *eob_total += eob;
  } else {
#if CONFIG_RECT_TX_EXT
    int is_qttx = plane_tx_size == quarter_txsize_lookup[plane_bsize];
    const TX_SIZE sub_txs = is_qttx ? plane_tx_size : sub_tx_size_map[tx_size];
    if (is_qttx) assert(blk_row == 0 && blk_col == 0 && block == 0);
#else
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    assert(IMPLIES(tx_size <= TX_4X4, sub_txs == tx_size));
    assert(IMPLIES(tx_size > TX_4X4, sub_txs < tx_size));
#endif
    const int bsl = tx_size_wide_unit[sub_txs];
    int sub_step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];
    int i;

    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
#if CONFIG_RECT_TX_EXT
      int is_wide_tx = tx_size_wide_unit[sub_txs] > tx_size_high_unit[sub_txs];
      const int offsetr =
          is_qttx ? (is_wide_tx ? i * tx_size_high_unit[sub_txs] : 0)
                  : blk_row + ((i >> 1) * bsl);
      const int offsetc =
          is_qttx ? (is_wide_tx ? 0 : i * tx_size_wide_unit[sub_txs])
                  : blk_col + (i & 0x01) * bsl;
#else
      const int offsetr = blk_row + (i >> 1) * bsl;
      const int offsetc = blk_col + (i & 0x01) * bsl;
#endif

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      decode_reconstruct_tx(cm, xd, r, mbmi, plane, plane_bsize, offsetr,
                            offsetc, block, sub_txs, eob_total);
      block += sub_step;
    }
  }
}
#endif  // CONFIG_VAR_TX

#if !CONFIG_VAR_TX || CONFIG_SUPERTX || CONFIG_COEF_INTERLEAVE || \
    (!CONFIG_VAR_TX && CONFIG_EXT_TX && CONFIG_RECT_TX)
static int reconstruct_inter_block(AV1_COMMON *cm, MACROBLOCKD *const xd,
                                   aom_reader *const r, int segment_id,
                                   int plane, int row, int col,
                                   TX_SIZE tx_size) {
  PLANE_TYPE plane_type = get_plane_type(plane);
  int block_idx = get_block_idx(xd, plane, row, col);
#if CONFIG_PVQ
  int eob;
  (void)r;
  (void)segment_id;
#else
  struct macroblockd_plane *const pd = &xd->plane[plane];
#endif

#if !CONFIG_PVQ
#if CONFIG_LV_MAP
  (void)segment_id;
  int16_t max_scan_line = 0;
  int eob;
  av1_read_coeffs_txb_facade(cm, xd, r, row, col, block_idx, plane, pd->dqcoeff,
                             tx_size, &max_scan_line, &eob);
  // tx_type will be read out in av1_read_coeffs_txb_facade
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, row, col, block_idx, tx_size);
#else   // CONFIG_LV_MAP
  int16_t max_scan_line = 0;
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, row, col, block_idx, tx_size);
  const SCAN_ORDER *scan_order =
      get_scan(cm, tx_size, tx_type, &xd->mi[0]->mbmi);
  const int eob =
      av1_decode_block_tokens(cm, xd, plane, scan_order, col, row, tx_size,
                              tx_type, &max_scan_line, r, segment_id);
#endif  // CONFIG_LV_MAP
  uint8_t *dst =
      &pd->dst.buf[(row * pd->dst.stride + col) << tx_size_wide_log2[0]];
  if (eob)
    inverse_transform_block(xd, plane,
#if CONFIG_LGT_FROM_PRED
                            xd->mi[0]->mbmi.mode,
#endif
                            tx_type, tx_size, dst, pd->dst.stride,
                            max_scan_line, eob);
#else
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, row, col, block_idx, tx_size);
  eob = av1_pvq_decode_helper2(cm, xd, &xd->mi[0]->mbmi, plane, row, col,
                               tx_size, tx_type);
#endif
  return eob;
}
#endif  // !CONFIG_VAR_TX || CONFIG_SUPER_TX

static void set_offsets(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                        BLOCK_SIZE bsize, int mi_row, int mi_col, int bw,
                        int bh, int x_mis, int y_mis) {
  const int offset = mi_row * cm->mi_stride + mi_col;
  int x, y;
  const TileInfo *const tile = &xd->tile;

  xd->mi = cm->mi_grid_visible + offset;
  xd->mi[0] = &cm->mi[offset];
  // TODO(slavarnway): Generate sb_type based on bwl and bhl, instead of
  // passing bsize from decode_partition().
  xd->mi[0]->mbmi.sb_type = bsize;
#if CONFIG_RD_DEBUG
  xd->mi[0]->mbmi.mi_row = mi_row;
  xd->mi[0]->mbmi.mi_col = mi_col;
#endif
#if CONFIG_CFL
  xd->cfl->mi_row = mi_row;
  xd->cfl->mi_col = mi_col;
#endif
  for (y = 0; y < y_mis; ++y)
    for (x = !y; x < x_mis; ++x) xd->mi[y * cm->mi_stride + x] = xd->mi[0];

  set_plane_n4(xd, bw, bh);
  set_skip_context(xd, mi_row, mi_col);

#if CONFIG_VAR_TX
  xd->max_tx_size = max_txsize_lookup[bsize];
#endif

  // Distance of Mb to the various image edges. These are specified to 8th pel
  // as they are always compared to values that are in 1/8th pel units
  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);

  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col);
}

#if CONFIG_SUPERTX
static MB_MODE_INFO *set_offsets_extend(AV1_COMMON *const cm,
                                        MACROBLOCKD *const xd,
                                        const TileInfo *const tile,
                                        BLOCK_SIZE bsize_pred, int mi_row_pred,
                                        int mi_col_pred, int mi_row_ori,
                                        int mi_col_ori) {
  // Used in supertx
  // (mi_row_ori, mi_col_ori): location for mv
  // (mi_row_pred, mi_col_pred, bsize_pred): region to predict
  const int bw = mi_size_wide[bsize_pred];
  const int bh = mi_size_high[bsize_pred];
  const int offset = mi_row_ori * cm->mi_stride + mi_col_ori;
  xd->mi = cm->mi_grid_visible + offset;
  xd->mi[0] = cm->mi + offset;
  set_mi_row_col(xd, tile, mi_row_pred, bh, mi_col_pred, bw,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);

  xd->up_available = (mi_row_ori > tile->mi_row_start);
  xd->left_available = (mi_col_ori > tile->mi_col_start);

  set_plane_n4(xd, bw, bh);

  return &xd->mi[0]->mbmi;
}

#if CONFIG_SUPERTX
static MB_MODE_INFO *set_mb_offsets(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                    BLOCK_SIZE bsize, int mi_row, int mi_col,
                                    int bw, int bh, int x_mis, int y_mis) {
  const int offset = mi_row * cm->mi_stride + mi_col;
  const TileInfo *const tile = &xd->tile;
  int x, y;

  xd->mi = cm->mi_grid_visible + offset;
  xd->mi[0] = cm->mi + offset;
  xd->mi[0]->mbmi.sb_type = bsize;
  for (y = 0; y < y_mis; ++y)
    for (x = !y; x < x_mis; ++x) xd->mi[y * cm->mi_stride + x] = xd->mi[0];

  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);
  return &xd->mi[0]->mbmi;
}
#endif

static void set_offsets_topblock(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 const TileInfo *const tile, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int offset = mi_row * cm->mi_stride + mi_col;

  xd->mi = cm->mi_grid_visible + offset;
  xd->mi[0] = cm->mi + offset;

  set_plane_n4(xd, bw, bh);

  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw,
#if CONFIG_DEPENDENT_HORZTILES
                 cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                 cm->mi_rows, cm->mi_cols);

  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col);
}

static void set_param_topblock(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                               BLOCK_SIZE bsize, int mi_row, int mi_col,
                               int txfm, int skip) {
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);
  const int offset = mi_row * cm->mi_stride + mi_col;
  int x, y;

  xd->mi = cm->mi_grid_visible + offset;
  xd->mi[0] = cm->mi + offset;

  for (y = 0; y < y_mis; ++y)
    for (x = 0; x < x_mis; ++x) {
      xd->mi[y * cm->mi_stride + x]->mbmi.skip = skip;
      xd->mi[y * cm->mi_stride + x]->mbmi.tx_type = txfm;
    }
#if CONFIG_VAR_TX
  xd->above_txfm_context = cm->above_txfm_context + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  set_txfm_ctxs(xd->mi[0]->mbmi.tx_size, bw, bh, skip, xd);
#endif
}

static void set_ref(AV1_COMMON *const cm, MACROBLOCKD *const xd, int idx,
                    int mi_row, int mi_col) {
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
#if CONFIG_COMPOUND_SINGLEREF
  RefBuffer *ref_buffer =
      has_second_ref(mbmi) ? &cm->frame_refs[mbmi->ref_frame[idx] - LAST_FRAME]
                           : &cm->frame_refs[mbmi->ref_frame[0] - LAST_FRAME];
#else
  RefBuffer *ref_buffer = &cm->frame_refs[mbmi->ref_frame[idx] - LAST_FRAME];
#endif  // CONFIG_COMPOUND_SINGLEREF
  xd->block_refs[idx] = ref_buffer;
  if (!av1_is_valid_scale(&ref_buffer->sf))
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Invalid scale factors");
  av1_setup_pre_planes(xd, idx, ref_buffer->buf, mi_row, mi_col,
                       &ref_buffer->sf);
  aom_merge_corrupted_flag(&xd->corrupted, ref_buffer->buf->corrupted);
}

static void dec_predict_b_extend(
    AV1Decoder *const pbi, MACROBLOCKD *const xd, const TileInfo *const tile,
    int block, int mi_row_ori, int mi_col_ori, int mi_row_pred, int mi_col_pred,
    int mi_row_top, int mi_col_top, int plane, uint8_t *dst_buf, int dst_stride,
    BLOCK_SIZE bsize_top, BLOCK_SIZE bsize_pred, int b_sub8x8, int bextend) {
  // Used in supertx
  // (mi_row_ori, mi_col_ori): location for mv
  // (mi_row_pred, mi_col_pred, bsize_pred): region to predict
  // (mi_row_top, mi_col_top, bsize_top): region of the top partition size
  // block: sub location of sub8x8 blocks
  // b_sub8x8: 1: ori is sub8x8; 0: ori is not sub8x8
  // bextend: 1: region to predict is an extension of ori; 0: not
  int r = (mi_row_pred - mi_row_top) * MI_SIZE;
  int c = (mi_col_pred - mi_col_top) * MI_SIZE;
  const int mi_width_top = mi_size_wide[bsize_top];
  const int mi_height_top = mi_size_high[bsize_top];
  MB_MODE_INFO *mbmi;
  AV1_COMMON *const cm = &pbi->common;

  if (mi_row_pred < mi_row_top || mi_col_pred < mi_col_top ||
      mi_row_pred >= mi_row_top + mi_height_top ||
      mi_col_pred >= mi_col_top + mi_width_top || mi_row_pred >= cm->mi_rows ||
      mi_col_pred >= cm->mi_cols)
    return;

  mbmi = set_offsets_extend(cm, xd, tile, bsize_pred, mi_row_pred, mi_col_pred,
                            mi_row_ori, mi_col_ori);
  set_ref(cm, xd, 0, mi_row_pred, mi_col_pred);
  if (has_second_ref(&xd->mi[0]->mbmi)
#if CONFIG_COMPOUND_SINGLEREF
      || is_inter_singleref_comp_mode(xd->mi[0]->mbmi.mode)
#endif  // CONFIG_COMPOUND_SINGLEREF
          )
    set_ref(cm, xd, 1, mi_row_pred, mi_col_pred);
  if (!bextend) mbmi->tx_size = max_txsize_lookup[bsize_top];

  xd->plane[plane].dst.stride = dst_stride;
  xd->plane[plane].dst.buf =
      dst_buf + (r >> xd->plane[plane].subsampling_y) * dst_stride +
      (c >> xd->plane[plane].subsampling_x);

  if (!b_sub8x8)
    av1_build_inter_predictor_sb_extend(&pbi->common, xd, mi_row_ori,
                                        mi_col_ori, mi_row_pred, mi_col_pred,
                                        plane, bsize_pred);
  else
    av1_build_inter_predictor_sb_sub8x8_extend(
        &pbi->common, xd, mi_row_ori, mi_col_ori, mi_row_pred, mi_col_pred,
        plane, bsize_pred, block);
}

static void dec_extend_dir(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                           const TileInfo *const tile, int block,
                           BLOCK_SIZE bsize, BLOCK_SIZE top_bsize,
                           int mi_row_ori, int mi_col_ori, int mi_row,
                           int mi_col, int mi_row_top, int mi_col_top,
                           int plane, uint8_t *dst_buf, int dst_stride,
                           int dir) {
  // dir: 0-lower, 1-upper, 2-left, 3-right
  //      4-lowerleft, 5-upperleft, 6-lowerright, 7-upperright
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  int xss = xd->plane[1].subsampling_x;
  int yss = xd->plane[1].subsampling_y;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
  int b_sub8x8 = (bsize < BLOCK_8X8) && !unify_bsize ? 1 : 0;
  BLOCK_SIZE extend_bsize;
  int mi_row_pred, mi_col_pred;

  int wide_unit, high_unit;
  int i, j;
  int ext_offset = 0;

  if (dir == 0 || dir == 1) {
    extend_bsize =
        (mi_width == mi_size_wide[BLOCK_8X8] || bsize < BLOCK_8X8 || xss < yss)
            ? BLOCK_8X8
            : BLOCK_16X8;
#if CONFIG_CB4X4
    if (bsize < BLOCK_8X8) {
      extend_bsize = BLOCK_4X4;
      ext_offset = mi_size_wide[BLOCK_8X8];
    }
#endif

    wide_unit = mi_size_wide[extend_bsize];
    high_unit = mi_size_high[extend_bsize];

    mi_row_pred = mi_row + ((dir == 0) ? mi_height : -(mi_height + ext_offset));
    mi_col_pred = mi_col;

    for (j = 0; j < mi_height + ext_offset; j += high_unit)
      for (i = 0; i < mi_width + ext_offset; i += wide_unit)
        dec_predict_b_extend(pbi, xd, tile, block, mi_row_ori, mi_col_ori,
                             mi_row_pred + j, mi_col_pred + i, mi_row_top,
                             mi_col_top, plane, dst_buf, dst_stride, top_bsize,
                             extend_bsize, b_sub8x8, 1);
  } else if (dir == 2 || dir == 3) {
    extend_bsize =
        (mi_height == mi_size_high[BLOCK_8X8] || bsize < BLOCK_8X8 || yss < xss)
            ? BLOCK_8X8
            : BLOCK_8X16;
#if CONFIG_CB4X4
    if (bsize < BLOCK_8X8) {
      extend_bsize = BLOCK_4X4;
      ext_offset = mi_size_wide[BLOCK_8X8];
    }
#endif

    wide_unit = mi_size_wide[extend_bsize];
    high_unit = mi_size_high[extend_bsize];

    mi_row_pred = mi_row;
    mi_col_pred = mi_col + ((dir == 3) ? mi_width : -(mi_width + ext_offset));

    for (j = 0; j < mi_height + ext_offset; j += high_unit)
      for (i = 0; i < mi_width + ext_offset; i += wide_unit)
        dec_predict_b_extend(pbi, xd, tile, block, mi_row_ori, mi_col_ori,
                             mi_row_pred + j, mi_col_pred + i, mi_row_top,
                             mi_col_top, plane, dst_buf, dst_stride, top_bsize,
                             extend_bsize, b_sub8x8, 1);
  } else {
    extend_bsize = BLOCK_8X8;
#if CONFIG_CB4X4
    if (bsize < BLOCK_8X8) {
      extend_bsize = BLOCK_4X4;
      ext_offset = mi_size_wide[BLOCK_8X8];
    }
#endif
    wide_unit = mi_size_wide[extend_bsize];
    high_unit = mi_size_high[extend_bsize];

    mi_row_pred = mi_row + ((dir == 4 || dir == 6) ? mi_height
                                                   : -(mi_height + ext_offset));
    mi_col_pred =
        mi_col + ((dir == 6 || dir == 7) ? mi_width : -(mi_width + ext_offset));

    for (j = 0; j < mi_height + ext_offset; j += high_unit)
      for (i = 0; i < mi_width + ext_offset; i += wide_unit)
        dec_predict_b_extend(pbi, xd, tile, block, mi_row_ori, mi_col_ori,
                             mi_row_pred + j, mi_col_pred + i, mi_row_top,
                             mi_col_top, plane, dst_buf, dst_stride, top_bsize,
                             extend_bsize, b_sub8x8, 1);
  }
}

static void dec_extend_all(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                           const TileInfo *const tile, int block,
                           BLOCK_SIZE bsize, BLOCK_SIZE top_bsize,
                           int mi_row_ori, int mi_col_ori, int mi_row,
                           int mi_col, int mi_row_top, int mi_col_top,
                           int plane, uint8_t *dst_buf, int dst_stride) {
  for (int i = 0; i < 8; ++i) {
    dec_extend_dir(pbi, xd, tile, block, bsize, top_bsize, mi_row_ori,
                   mi_col_ori, mi_row, mi_col, mi_row_top, mi_col_top, plane,
                   dst_buf, dst_stride, i);
  }
}

static void dec_predict_sb_complex(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                                   const TileInfo *const tile, int mi_row,
                                   int mi_col, int mi_row_top, int mi_col_top,
                                   BLOCK_SIZE bsize, BLOCK_SIZE top_bsize,
                                   uint8_t *dst_buf[3], int dst_stride[3]) {
  const AV1_COMMON *const cm = &pbi->common;
  const int hbs = mi_size_wide[bsize] / 2;
  const PARTITION_TYPE partition = get_partition(cm, mi_row, mi_col, bsize);
  const BLOCK_SIZE subsize = get_subsize(bsize, partition);
#if CONFIG_EXT_PARTITION_TYPES
  const BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif
  int i;
  const int mi_offset = mi_row * cm->mi_stride + mi_col;
  uint8_t *dst_buf1[3], *dst_buf2[3], *dst_buf3[3];
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif

  DECLARE_ALIGNED(16, uint8_t, tmp_buf1[MAX_MB_PLANE * MAX_TX_SQUARE * 2]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf2[MAX_MB_PLANE * MAX_TX_SQUARE * 2]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf3[MAX_MB_PLANE * MAX_TX_SQUARE * 2]);
  int dst_stride1[3] = { MAX_TX_SIZE, MAX_TX_SIZE, MAX_TX_SIZE };
  int dst_stride2[3] = { MAX_TX_SIZE, MAX_TX_SIZE, MAX_TX_SIZE };
  int dst_stride3[3] = { MAX_TX_SIZE, MAX_TX_SIZE, MAX_TX_SIZE };

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    dst_buf1[0] = CONVERT_TO_BYTEPTR(tmp_buf1);
    dst_buf1[1] = CONVERT_TO_BYTEPTR(tmp_buf1 + MAX_TX_SQUARE * len);
    dst_buf1[2] = CONVERT_TO_BYTEPTR(tmp_buf1 + 2 * MAX_TX_SQUARE * len);
    dst_buf2[0] = CONVERT_TO_BYTEPTR(tmp_buf2);
    dst_buf2[1] = CONVERT_TO_BYTEPTR(tmp_buf2 + MAX_TX_SQUARE * len);
    dst_buf2[2] = CONVERT_TO_BYTEPTR(tmp_buf2 + 2 * MAX_TX_SQUARE * len);
    dst_buf3[0] = CONVERT_TO_BYTEPTR(tmp_buf3);
    dst_buf3[1] = CONVERT_TO_BYTEPTR(tmp_buf3 + MAX_TX_SQUARE * len);
    dst_buf3[2] = CONVERT_TO_BYTEPTR(tmp_buf3 + 2 * MAX_TX_SQUARE * len);
  } else {
#endif
    dst_buf1[0] = tmp_buf1;
    dst_buf1[1] = tmp_buf1 + MAX_TX_SQUARE;
    dst_buf1[2] = tmp_buf1 + 2 * MAX_TX_SQUARE;
    dst_buf2[0] = tmp_buf2;
    dst_buf2[1] = tmp_buf2 + MAX_TX_SQUARE;
    dst_buf2[2] = tmp_buf2 + 2 * MAX_TX_SQUARE;
    dst_buf3[0] = tmp_buf3;
    dst_buf3[1] = tmp_buf3 + MAX_TX_SQUARE;
    dst_buf3[2] = tmp_buf3 + 2 * MAX_TX_SQUARE;
#if CONFIG_HIGHBITDEPTH
  }
#endif

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  xd->mi = cm->mi_grid_visible + mi_offset;
  xd->mi[0] = cm->mi + mi_offset;

  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].dst.buf = dst_buf[i];
    xd->plane[i].dst.stride = dst_stride[i];
  }

  switch (partition) {
    case PARTITION_NONE:
      assert(bsize < top_bsize);
      for (i = 0; i < MAX_MB_PLANE; i++) {
        dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                             mi_row_top, mi_col_top, i, dst_buf[i],
                             dst_stride[i], top_bsize, bsize, 0, 0);
        dec_extend_all(pbi, xd, tile, 0, bsize, top_bsize, mi_row, mi_col,
                       mi_row, mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                       dst_stride[i]);
      }
      break;
    case PARTITION_HORZ:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        for (i = 0; i < MAX_MB_PLANE; i++) {
          // For sub8x8, predict in 8x8 unit
          // First half
          dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf[i],
                               dst_stride[i], top_bsize, BLOCK_8X8, 1, 0);
          if (bsize < top_bsize)
            dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf[i], dst_stride[i]);

          // Second half
          dec_predict_b_extend(pbi, xd, tile, 2, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf1[i],
                               dst_stride1[i], top_bsize, BLOCK_8X8, 1, 1);
          if (bsize < top_bsize)
            dec_extend_all(pbi, xd, tile, 2, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf1[i], dst_stride1[i]);
        }

        // weighted average to smooth the boundary
        xd->plane[0].dst.buf = dst_buf[0];
        xd->plane[0].dst.stride = dst_stride[0];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[0], dst_stride[0], dst_buf1[0], dst_stride1[0], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            0);
      } else {
        for (i = 0; i < MAX_MB_PLANE; i++) {
#if CONFIG_CB4X4
          const struct macroblockd_plane *pd = &xd->plane[i];
          int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
              subsize, pd->subsampling_x, pd->subsampling_y);

          if (handle_chroma_sub8x8) {
            int mode_offset_row = CONFIG_CHROMA_SUB8X8 ? hbs : 0;

            dec_predict_b_extend(pbi, xd, tile, 0, mi_row + mode_offset_row,
                                 mi_col, mi_row, mi_col, mi_row_top, mi_col_top,
                                 i, dst_buf[i], dst_stride[i], top_bsize, bsize,
                                 0, 0);
            if (bsize < top_bsize)
              dec_extend_all(pbi, xd, tile, 0, bsize, top_bsize,
                             mi_row + mode_offset_row, mi_col, mi_row, mi_col,
                             mi_row_top, mi_col_top, i, dst_buf[i],
                             dst_stride[i]);
          } else {
#endif
            // First half
            dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row,
                                 mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                                 dst_stride[i], top_bsize, subsize, 0, 0);
            if (bsize < top_bsize)
              dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                             mi_col, mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i]);
            else
              dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                             mi_col, mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i], 0);

            if (mi_row + hbs < cm->mi_rows) {
              // Second half
              dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col,
                                   mi_row + hbs, mi_col, mi_row_top, mi_col_top,
                                   i, dst_buf1[i], dst_stride1[i], top_bsize,
                                   subsize, 0, 0);
              if (bsize < top_bsize)
                dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize,
                               mi_row + hbs, mi_col, mi_row + hbs, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf1[i],
                               dst_stride1[i]);
              else
                dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize,
                               mi_row + hbs, mi_col, mi_row + hbs, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf1[i],
                               dst_stride1[i], 1);

              // weighted average to smooth the boundary
              xd->plane[i].dst.buf = dst_buf[i];
              xd->plane[i].dst.stride = dst_stride[i];
              av1_build_masked_inter_predictor_complex(
                  xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i],
                  mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                  PARTITION_HORZ, i);
            }
#if CONFIG_CB4X4
          }
#endif
        }
      }
      break;
    case PARTITION_VERT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        for (i = 0; i < MAX_MB_PLANE; i++) {
          // First half
          dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf[i],
                               dst_stride[i], top_bsize, BLOCK_8X8, 1, 0);
          if (bsize < top_bsize)
            dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf[i], dst_stride[i]);

          // Second half
          dec_predict_b_extend(pbi, xd, tile, 1, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf1[i],
                               dst_stride1[i], top_bsize, BLOCK_8X8, 1, 1);
          if (bsize < top_bsize)
            dec_extend_all(pbi, xd, tile, 1, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf1[i], dst_stride1[i]);
        }

        // Smooth
        xd->plane[0].dst.buf = dst_buf[0];
        xd->plane[0].dst.stride = dst_stride[0];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[0], dst_stride[0], dst_buf1[0], dst_stride1[0], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            0);
      } else {
        for (i = 0; i < MAX_MB_PLANE; i++) {
#if CONFIG_CB4X4
          const struct macroblockd_plane *pd = &xd->plane[i];
          int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
              subsize, pd->subsampling_x, pd->subsampling_y);

          if (handle_chroma_sub8x8) {
            int mode_offset_col = CONFIG_CHROMA_SUB8X8 ? hbs : 0;
            assert(i > 0 && bsize == BLOCK_8X8);

            dec_predict_b_extend(pbi, xd, tile, 0, mi_row,
                                 mi_col + mode_offset_col, mi_row, mi_col,
                                 mi_row_top, mi_col_top, i, dst_buf[i],
                                 dst_stride[i], top_bsize, bsize, 0, 0);
            if (bsize < top_bsize)
              dec_extend_all(pbi, xd, tile, 0, bsize, top_bsize, mi_row,
                             mi_col + mode_offset_col, mi_row, mi_col,
                             mi_row_top, mi_col_top, i, dst_buf[i],
                             dst_stride[i]);
          } else {
#endif
            // First half
            dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row,
                                 mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                                 dst_stride[i], top_bsize, subsize, 0, 0);
            if (bsize < top_bsize)
              dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                             mi_col, mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i]);
            else
              dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                             mi_col, mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i], 3);

            // Second half
            if (mi_col + hbs < cm->mi_cols) {
              dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col + hbs,
                                   mi_row, mi_col + hbs, mi_row_top, mi_col_top,
                                   i, dst_buf1[i], dst_stride1[i], top_bsize,
                                   subsize, 0, 0);
              if (bsize < top_bsize)
                dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                               mi_col + hbs, mi_row, mi_col + hbs, mi_row_top,
                               mi_col_top, i, dst_buf1[i], dst_stride1[i]);
              else
                dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                               mi_col + hbs, mi_row, mi_col + hbs, mi_row_top,
                               mi_col_top, i, dst_buf1[i], dst_stride1[i], 2);

              // Smooth
              xd->plane[i].dst.buf = dst_buf[i];
              xd->plane[i].dst.stride = dst_stride[i];
              av1_build_masked_inter_predictor_complex(
                  xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i],
                  mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                  PARTITION_VERT, i);
            }
#if CONFIG_CB4X4
          }
#endif
        }
      }
      break;
    case PARTITION_SPLIT:
      if (bsize == BLOCK_8X8 && !unify_bsize) {
        for (i = 0; i < MAX_MB_PLANE; i++) {
          dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf[i],
                               dst_stride[i], top_bsize, BLOCK_8X8, 1, 0);
          dec_predict_b_extend(pbi, xd, tile, 1, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf1[i],
                               dst_stride1[i], top_bsize, BLOCK_8X8, 1, 1);
          dec_predict_b_extend(pbi, xd, tile, 2, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf2[i],
                               dst_stride2[i], top_bsize, BLOCK_8X8, 1, 1);
          dec_predict_b_extend(pbi, xd, tile, 3, mi_row, mi_col, mi_row, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf3[i],
                               dst_stride3[i], top_bsize, BLOCK_8X8, 1, 1);
          if (bsize < top_bsize) {
            dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf[i], dst_stride[i]);
            dec_extend_all(pbi, xd, tile, 1, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf1[i], dst_stride1[i]);
            dec_extend_all(pbi, xd, tile, 2, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf2[i], dst_stride2[i]);
            dec_extend_all(pbi, xd, tile, 3, subsize, top_bsize, mi_row, mi_col,
                           mi_row, mi_col, mi_row_top, mi_col_top, i,
                           dst_buf3[i], dst_stride3[i]);
          }
        }
#if CONFIG_CB4X4
      } else if (bsize == BLOCK_8X8) {
        for (i = 0; i < MAX_MB_PLANE; i++) {
          const struct macroblockd_plane *pd = &xd->plane[i];
          int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
              subsize, pd->subsampling_x, pd->subsampling_y);

          if (handle_chroma_sub8x8) {
            int mode_offset_row =
                CONFIG_CHROMA_SUB8X8 && mi_row + hbs < cm->mi_rows ? hbs : 0;
            int mode_offset_col =
                CONFIG_CHROMA_SUB8X8 && mi_col + hbs < cm->mi_cols ? hbs : 0;

            dec_predict_b_extend(pbi, xd, tile, 0, mi_row + mode_offset_row,
                                 mi_col + mode_offset_col, mi_row, mi_col,
                                 mi_row_top, mi_col_top, i, dst_buf[i],
                                 dst_stride[i], top_bsize, BLOCK_8X8, 0, 0);
            if (bsize < top_bsize)
              dec_extend_all(pbi, xd, tile, 0, BLOCK_8X8, top_bsize,
                             mi_row + mode_offset_row, mi_col + mode_offset_col,
                             mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i]);
          } else {
            dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row,
                                 mi_col, mi_row_top, mi_col_top, i, dst_buf[i],
                                 dst_stride[i], top_bsize, subsize, 0, 0);
            if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols)
              dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col + hbs,
                                   mi_row, mi_col + hbs, mi_row_top, mi_col_top,
                                   i, dst_buf1[i], dst_stride1[i], top_bsize,
                                   subsize, 0, 0);
            if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols)
              dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col,
                                   mi_row + hbs, mi_col, mi_row_top, mi_col_top,
                                   i, dst_buf2[i], dst_stride2[i], top_bsize,
                                   subsize, 0, 0);
            if (mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols)
              dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col + hbs,
                                   mi_row + hbs, mi_col + hbs, mi_row_top,
                                   mi_col_top, i, dst_buf3[i], dst_stride3[i],
                                   top_bsize, subsize, 0, 0);

            if (bsize < top_bsize) {
              dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                             mi_col, mi_row, mi_col, mi_row_top, mi_col_top, i,
                             dst_buf[i], dst_stride[i]);
              if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols)
                dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                               mi_col + hbs, mi_row, mi_col + hbs, mi_row_top,
                               mi_col_top, i, dst_buf1[i], dst_stride1[i]);
              if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols)
                dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize,
                               mi_row + hbs, mi_col, mi_row + hbs, mi_col,
                               mi_row_top, mi_col_top, i, dst_buf2[i],
                               dst_stride2[i]);
              if (mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols)
                dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize,
                               mi_row + hbs, mi_col + hbs, mi_row + hbs,
                               mi_col + hbs, mi_row_top, mi_col_top, i,
                               dst_buf3[i], dst_stride3[i]);
            }
          }
        }
#endif
      } else {
        dec_predict_sb_complex(pbi, xd, tile, mi_row, mi_col, mi_row_top,
                               mi_col_top, subsize, top_bsize, dst_buf,
                               dst_stride);
        if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols)
          dec_predict_sb_complex(pbi, xd, tile, mi_row, mi_col + hbs,
                                 mi_row_top, mi_col_top, subsize, top_bsize,
                                 dst_buf1, dst_stride1);
        if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols)
          dec_predict_sb_complex(pbi, xd, tile, mi_row + hbs, mi_col,
                                 mi_row_top, mi_col_top, subsize, top_bsize,
                                 dst_buf2, dst_stride2);
        if (mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols)
          dec_predict_sb_complex(pbi, xd, tile, mi_row + hbs, mi_col + hbs,
                                 mi_row_top, mi_col_top, subsize, top_bsize,
                                 dst_buf3, dst_stride3);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
#if CONFIG_CB4X4
        const struct macroblockd_plane *pd = &xd->plane[i];
        int handle_chroma_sub8x8 = need_handle_chroma_sub8x8(
            subsize, pd->subsampling_x, pd->subsampling_y);
        if (handle_chroma_sub8x8) continue;  // Skip <4x4 chroma smoothing
#else
        if (bsize == BLOCK_8X8 && i != 0)
          continue;  // Skip <4x4 chroma smoothing
#endif
        if (mi_row < cm->mi_rows && mi_col + hbs < cm->mi_cols) {
          av1_build_masked_inter_predictor_complex(
              xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i],
              mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
              PARTITION_VERT, i);
          if (mi_row + hbs < cm->mi_rows) {
            av1_build_masked_inter_predictor_complex(
                xd, dst_buf2[i], dst_stride2[i], dst_buf3[i], dst_stride3[i],
                mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                PARTITION_VERT, i);
            av1_build_masked_inter_predictor_complex(
                xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i],
                mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
                PARTITION_HORZ, i);
          }
        } else if (mi_row + hbs < cm->mi_rows && mi_col < cm->mi_cols) {
          av1_build_masked_inter_predictor_complex(
              xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i],
              mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
              PARTITION_HORZ, i);
        }
      }
      break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error HORZ/VERT_A/B partitions not yet updated in superres code
#endif
    case PARTITION_HORZ_A:
      dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, dst_buf, dst_stride,
                           top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row, mi_col,
                     mi_row_top, mi_col_top, dst_buf, dst_stride);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col + hbs, mi_row,
                           mi_col + hbs, mi_row_top, mi_col_top, dst_buf1,
                           dst_stride1, top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row, mi_col + hbs,
                     mi_row_top, mi_col_top, dst_buf1, dst_stride1);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col, mi_row + hbs,
                           mi_col, mi_row_top, mi_col_top, dst_buf2,
                           dst_stride2, top_bsize, subsize, 0, 0);
      if (bsize < top_bsize)
        dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row + hbs,
                       mi_col, mi_row_top, mi_col_top, dst_buf2, dst_stride2);
      else
        dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize, mi_row + hbs,
                       mi_col, mi_row_top, mi_col_top, dst_buf2, dst_stride2,
                       1);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            i);
      }
      break;
    case PARTITION_VERT_A:

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, dst_buf, dst_stride,
                           top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row, mi_col,
                     mi_row_top, mi_col_top, dst_buf, dst_stride);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col, mi_row + hbs,
                           mi_col, mi_row_top, mi_col_top, dst_buf1,
                           dst_stride1, top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row + hbs, mi_col,
                     mi_row_top, mi_col_top, dst_buf1, dst_stride1);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col + hbs, mi_row,
                           mi_col + hbs, mi_row_top, mi_col_top, dst_buf2,
                           dst_stride2, top_bsize, subsize, 0, 0);
      if (bsize < top_bsize)
        dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                       mi_col + hbs, mi_row_top, mi_col_top, dst_buf2,
                       dst_stride2);
      else
        dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize, mi_row,
                       mi_col + hbs, mi_row_top, mi_col_top, dst_buf2,
                       dst_stride2, 2);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf2[i], dst_stride2[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            i);
      }
      break;
    case PARTITION_HORZ_B:
      dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, dst_buf, dst_stride,
                           top_bsize, subsize, 0, 0);
      if (bsize < top_bsize)
        dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride);
      else
        dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride, 0);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col, mi_row + hbs,
                           mi_col, mi_row_top, mi_col_top, dst_buf1,
                           dst_stride1, top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row + hbs, mi_col,
                     mi_row_top, mi_col_top, dst_buf1, dst_stride1);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col + hbs,
                           mi_row + hbs, mi_col + hbs, mi_row_top, mi_col_top,
                           dst_buf2, dst_stride2, top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row + hbs,
                     mi_col + hbs, mi_row_top, mi_col_top, dst_buf2,
                     dst_stride2);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf1[i];
        xd->plane[i].dst.stride = dst_stride1[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf1[i], dst_stride1[i], dst_buf2[i], dst_stride2[i],
            mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
            PARTITION_VERT, i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_HORZ,
            i);
      }
      break;
    case PARTITION_VERT_B:
      dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col, mi_row, mi_col,
                           mi_row_top, mi_col_top, dst_buf, dst_stride,
                           top_bsize, subsize, 0, 0);
      if (bsize < top_bsize)
        dec_extend_all(pbi, xd, tile, 0, subsize, top_bsize, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride);
      else
        dec_extend_dir(pbi, xd, tile, 0, subsize, top_bsize, mi_row, mi_col,
                       mi_row_top, mi_col_top, dst_buf, dst_stride, 3);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row, mi_col + hbs, mi_row,
                           mi_col + hbs, mi_row_top, mi_col_top, dst_buf1,
                           dst_stride1, top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row, mi_col + hbs,
                     mi_row_top, mi_col_top, dst_buf1, dst_stride1);

      dec_predict_b_extend(pbi, xd, tile, 0, mi_row + hbs, mi_col + hbs,
                           mi_row + hbs, mi_col + hbs, mi_row_top, mi_col_top,
                           dst_buf2, dst_stride2, top_bsize, bsize2, 0, 0);
      dec_extend_all(pbi, xd, tile, 0, bsize2, top_bsize, mi_row + hbs,
                     mi_col + hbs, mi_row_top, mi_col_top, dst_buf2,
                     dst_stride2);

      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf1[i];
        xd->plane[i].dst.stride = dst_stride1[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf1[i], dst_stride1[i], dst_buf2[i], dst_stride2[i],
            mi_row, mi_col, mi_row_top, mi_col_top, bsize, top_bsize,
            PARTITION_HORZ, i);
      }
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = dst_buf[i];
        xd->plane[i].dst.stride = dst_stride[i];
        av1_build_masked_inter_predictor_complex(
            xd, dst_buf[i], dst_stride[i], dst_buf1[i], dst_stride1[i], mi_row,
            mi_col, mi_row_top, mi_col_top, bsize, top_bsize, PARTITION_VERT,
            i);
      }
      break;
#endif  // CONFIG_EXT_PARTITION_TYPES
    default: assert(0);
  }
}

static void set_segment_id_supertx(const AV1_COMMON *const cm, int mi_row,
                                   int mi_col, BLOCK_SIZE bsize) {
  const struct segmentation *seg = &cm->seg;
  const int miw = AOMMIN(mi_size_wide[bsize], cm->mi_cols - mi_col);
  const int mih = AOMMIN(mi_size_high[bsize], cm->mi_rows - mi_row);
  const int mi_offset = mi_row * cm->mi_stride + mi_col;
  MODE_INFO **const mip = cm->mi_grid_visible + mi_offset;
  int r, c;
  int seg_id_supertx = MAX_SEGMENTS;

  if (!seg->enabled) {
    seg_id_supertx = 0;
  } else {
    // Find the minimum segment_id
    for (r = 0; r < mih; r++)
      for (c = 0; c < miw; c++)
        seg_id_supertx =
            AOMMIN(mip[r * cm->mi_stride + c]->mbmi.segment_id, seg_id_supertx);
    assert(0 <= seg_id_supertx && seg_id_supertx < MAX_SEGMENTS);
  }

  // Assign the the segment_id back to segment_id_supertx
  for (r = 0; r < mih; r++)
    for (c = 0; c < miw; c++)
      mip[r * cm->mi_stride + c]->mbmi.segment_id_supertx = seg_id_supertx;
}
#endif  // CONFIG_SUPERTX

static void decode_mbmi_block(AV1Decoder *const pbi, MACROBLOCKD *const xd,
#if CONFIG_SUPERTX
                              int supertx_enabled,
#endif  // CONFIG_SUPERTX
                              int mi_row, int mi_col, aom_reader *r,
#if CONFIG_EXT_PARTITION_TYPES
                              PARTITION_TYPE partition,
#endif  // CONFIG_EXT_PARTITION_TYPES
                              BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);

#if CONFIG_ACCOUNTING
  aom_accounting_set_context(&pbi->accounting, mi_col, mi_row);
#endif
#if CONFIG_SUPERTX
  if (supertx_enabled) {
    set_mb_offsets(cm, xd, bsize, mi_row, mi_col, bw, bh, x_mis, y_mis);
  } else {
    set_offsets(cm, xd, bsize, mi_row, mi_col, bw, bh, x_mis, y_mis);
  }
#if CONFIG_EXT_PARTITION_TYPES
  xd->mi[0]->mbmi.partition = partition;
#endif
  av1_read_mode_info(pbi, xd, supertx_enabled, mi_row, mi_col, r, x_mis, y_mis);
#else
  set_offsets(cm, xd, bsize, mi_row, mi_col, bw, bh, x_mis, y_mis);
#if CONFIG_EXT_PARTITION_TYPES
  xd->mi[0]->mbmi.partition = partition;
#endif
  av1_read_mode_info(pbi, xd, mi_row, mi_col, r, x_mis, y_mis);
#endif  // CONFIG_SUPERTX
  if (bsize >= BLOCK_8X8 && (cm->subsampling_x || cm->subsampling_y)) {
    const BLOCK_SIZE uv_subsize =
        ss_size_lookup[bsize][cm->subsampling_x][cm->subsampling_y];
    if (uv_subsize == BLOCK_INVALID)
      aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid block size.");
  }

#if CONFIG_SUPERTX
  xd->mi[0]->mbmi.segment_id_supertx = MAX_SEGMENTS;
#endif  // CONFIG_SUPERTX

  int reader_corrupted_flag = aom_reader_has_error(r);
  aom_merge_corrupted_flag(&xd->corrupted, reader_corrupted_flag);
}

#if CONFIG_NCOBMC_ADAPT_WEIGHT
static void set_mode_info_offsets(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                  int mi_row, int mi_col) {
  const int offset = mi_row * cm->mi_stride + mi_col;
  xd->mi = cm->mi_grid_visible + offset;
  xd->mi[0] = &cm->mi[offset];
}

static void get_ncobmc_recon(AV1_COMMON *const cm, MACROBLOCKD *xd, int mi_row,
                             int mi_col, int bsize, int mode) {
  uint8_t *pred_buf[4][MAX_MB_PLANE];
  int pred_stride[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  // target block in pxl
  int pxl_row = mi_row << MI_SIZE_LOG2;
  int pxl_col = mi_col << MI_SIZE_LOG2;

  int plane;
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    ASSIGN_ALIGNED_PTRS_HBD(pred_buf[0], cm->ncobmcaw_buf[0], MAX_SB_SQUARE,
                            len);
    ASSIGN_ALIGNED_PTRS_HBD(pred_buf[1], cm->ncobmcaw_buf[1], MAX_SB_SQUARE,
                            len);
    ASSIGN_ALIGNED_PTRS_HBD(pred_buf[2], cm->ncobmcaw_buf[2], MAX_SB_SQUARE,
                            len);
    ASSIGN_ALIGNED_PTRS_HBD(pred_buf[3], cm->ncobmcaw_buf[3], MAX_SB_SQUARE,
                            len);
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    ASSIGN_ALIGNED_PTRS(pred_buf[0], cm->ncobmcaw_buf[0], MAX_SB_SQUARE);
    ASSIGN_ALIGNED_PTRS(pred_buf[1], cm->ncobmcaw_buf[1], MAX_SB_SQUARE);
    ASSIGN_ALIGNED_PTRS(pred_buf[2], cm->ncobmcaw_buf[2], MAX_SB_SQUARE);
    ASSIGN_ALIGNED_PTRS(pred_buf[3], cm->ncobmcaw_buf[3], MAX_SB_SQUARE);
#if CONFIG_HIGHBITDEPTH
  }
#endif
  av1_get_ext_blk_preds(cm, xd, bsize, mi_row, mi_col, pred_buf, pred_stride);
  av1_get_ori_blk_pred(cm, xd, bsize, mi_row, mi_col, pred_buf[3], pred_stride);
  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    build_ncobmc_intrpl_pred(cm, xd, plane, pxl_row, pxl_col, bsize, pred_buf,
                             pred_stride, mode);
  }
}

static void av1_get_ncobmc_recon(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 int bsize, const int mi_row, const int mi_col,
                                 const NCOBMC_MODE modes) {
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  assert(bsize >= BLOCK_8X8);

  reset_xd_boundary(xd, mi_row, mi_height, mi_col, mi_width, cm->mi_rows,
                    cm->mi_cols);
  get_ncobmc_recon(cm, xd, mi_row, mi_col, bsize, modes);
}

static void recon_ncobmc_intrpl_pred(AV1_COMMON *const cm,
                                     MACROBLOCKD *const xd, int mi_row,
                                     int mi_col, BLOCK_SIZE bsize) {
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int hbs = AOMMAX(mi_size_wide[bsize] / 2, mi_size_high[bsize] / 2);
  const BLOCK_SIZE sqr_blk = bsize_2_sqr_bsize[bsize];
  if (mi_width > mi_height) {
    // horizontal partition
    av1_get_ncobmc_recon(cm, xd, sqr_blk, mi_row, mi_col, mbmi->ncobmc_mode[0]);
    xd->mi += hbs;
    av1_get_ncobmc_recon(cm, xd, sqr_blk, mi_row, mi_col + hbs,
                         mbmi->ncobmc_mode[1]);
  } else if (mi_height > mi_width) {
    // vertical partition
    av1_get_ncobmc_recon(cm, xd, sqr_blk, mi_row, mi_col, mbmi->ncobmc_mode[0]);
    xd->mi += hbs * xd->mi_stride;
    av1_get_ncobmc_recon(cm, xd, sqr_blk, mi_row + hbs, mi_col,
                         mbmi->ncobmc_mode[1]);
  } else {
    av1_get_ncobmc_recon(cm, xd, sqr_blk, mi_row, mi_col, mbmi->ncobmc_mode[0]);
  }
  set_mode_info_offsets(cm, xd, mi_row, mi_col);
  // restore dst buffer and mode info
  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col);
}
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

static void decode_token_and_recon_block(AV1Decoder *const pbi,
                                         MACROBLOCKD *const xd, int mi_row,
                                         int mi_col, aom_reader *r,
                                         BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);

  set_offsets(cm, xd, bsize, mi_row, mi_col, bw, bh, x_mis, y_mis);
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
#if CONFIG_CFL && CONFIG_CHROMA_SUB8X8
  CFL_CTX *const cfl = xd->cfl;
  cfl->is_chroma_reference = is_chroma_reference(
      mi_row, mi_col, bsize, cfl->subsampling_x, cfl->subsampling_y);
#endif  // CONFIG_CFL && CONFIG_CHROMA_SUB8X8

  if (cm->delta_q_present_flag) {
    int i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
#if CONFIG_EXT_DELTA_Q
      const int current_qindex =
          av1_get_qindex(&cm->seg, i, xd->current_qindex);
#else
      const int current_qindex = xd->current_qindex;
#endif  // CONFIG_EXT_DELTA_Q
      int j;
      for (j = 0; j < MAX_MB_PLANE; ++j) {
        const int dc_delta_q = j == 0 ? cm->y_dc_delta_q : cm->uv_dc_delta_q;
        const int ac_delta_q = j == 0 ? 0 : cm->uv_ac_delta_q;

        xd->plane[j].seg_dequant[i][0] =
            av1_dc_quant(current_qindex, dc_delta_q, cm->bit_depth);
        xd->plane[j].seg_dequant[i][1] =
            av1_ac_quant(current_qindex, ac_delta_q, cm->bit_depth);
      }
    }
  }

#if CONFIG_CB4X4
  if (mbmi->skip) av1_reset_skip_context(xd, mi_row, mi_col, bsize);
#else
  if (mbmi->skip) {
    av1_reset_skip_context(xd, mi_row, mi_col, AOMMAX(BLOCK_8X8, bsize));
  }
#endif

#if CONFIG_COEF_INTERLEAVE
  {
    const struct macroblockd_plane *const pd_y = &xd->plane[0];
    const struct macroblockd_plane *const pd_c = &xd->plane[1];
    const TX_SIZE tx_log2_y = mbmi->tx_size;
    const TX_SIZE tx_log2_c = av1_get_uv_tx_size(mbmi, pd_c);
    const int tx_sz_y = (1 << tx_log2_y);
    const int tx_sz_c = (1 << tx_log2_c);
    const int num_4x4_w_y = pd_y->n4_w;
    const int num_4x4_h_y = pd_y->n4_h;
    const int num_4x4_w_c = pd_c->n4_w;
    const int num_4x4_h_c = pd_c->n4_h;
    const int max_4x4_w_y = get_max_4x4_size(num_4x4_w_y, xd->mb_to_right_edge,
                                             pd_y->subsampling_x);
    const int max_4x4_h_y = get_max_4x4_size(num_4x4_h_y, xd->mb_to_bottom_edge,
                                             pd_y->subsampling_y);
    const int max_4x4_w_c = get_max_4x4_size(num_4x4_w_c, xd->mb_to_right_edge,
                                             pd_c->subsampling_x);
    const int max_4x4_h_c = get_max_4x4_size(num_4x4_h_c, xd->mb_to_bottom_edge,
                                             pd_c->subsampling_y);

    // The max_4x4_w/h may be smaller than tx_sz under some corner cases,
    // i.e. when the SB is splitted by tile boundaries.
    const int tu_num_w_y = (max_4x4_w_y + tx_sz_y - 1) / tx_sz_y;
    const int tu_num_h_y = (max_4x4_h_y + tx_sz_y - 1) / tx_sz_y;
    const int tu_num_w_c = (max_4x4_w_c + tx_sz_c - 1) / tx_sz_c;
    const int tu_num_h_c = (max_4x4_h_c + tx_sz_c - 1) / tx_sz_c;
    const int tu_num_c = tu_num_w_c * tu_num_h_c;

    if (!is_inter_block(mbmi)) {
      int tu_idx_c = 0;
      int row_y, col_y, row_c, col_c;
      int plane;

// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ
      for (plane = 0; plane <= 1; ++plane) {
        if (mbmi->palette_mode_info.palette_size[plane])
          av1_decode_palette_tokens(xd, plane, r);
      }
#endif  // !CONFIG_PVQ

      for (row_y = 0; row_y < tu_num_h_y; row_y++) {
        for (col_y = 0; col_y < tu_num_w_y; col_y++) {
          // luma
          predict_and_reconstruct_intra_block(
              cm, xd, r, mbmi, 0, row_y * tx_sz_y, col_y * tx_sz_y, tx_log2_y);
          // chroma
          if (tu_idx_c < tu_num_c) {
            row_c = (tu_idx_c / tu_num_w_c) * tx_sz_c;
            col_c = (tu_idx_c % tu_num_w_c) * tx_sz_c;
            predict_and_reconstruct_intra_block(cm, xd, r, mbmi, 1, row_c,
                                                col_c, tx_log2_c);
            predict_and_reconstruct_intra_block(cm, xd, r, mbmi, 2, row_c,
                                                col_c, tx_log2_c);
            tu_idx_c++;
          }
        }
      }

      // In 422 case, it's possilbe that Chroma has more TUs than Luma
      while (tu_idx_c < tu_num_c) {
        row_c = (tu_idx_c / tu_num_w_c) * tx_sz_c;
        col_c = (tu_idx_c % tu_num_w_c) * tx_sz_c;
        predict_and_reconstruct_intra_block(cm, xd, r, mbmi, 1, row_c, col_c,
                                            tx_log2_c);
        predict_and_reconstruct_intra_block(cm, xd, r, mbmi, 2, row_c, col_c,
                                            tx_log2_c);
        tu_idx_c++;
      }
    } else {
      // Prediction
      av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL,
                                    AOMMAX(bsize, BLOCK_8X8));

      // Reconstruction
      if (!mbmi->skip) {
        int eobtotal = 0;
        int tu_idx_c = 0;
        int row_y, col_y, row_c, col_c;

        for (row_y = 0; row_y < tu_num_h_y; row_y++) {
          for (col_y = 0; col_y < tu_num_w_y; col_y++) {
            // luma
            eobtotal += reconstruct_inter_block(cm, xd, r, mbmi->segment_id, 0,
                                                row_y * tx_sz_y,
                                                col_y * tx_sz_y, tx_log2_y);
            // chroma
            if (tu_idx_c < tu_num_c) {
              row_c = (tu_idx_c / tu_num_w_c) * tx_sz_c;
              col_c = (tu_idx_c % tu_num_w_c) * tx_sz_c;
              eobtotal += reconstruct_inter_block(cm, xd, r, mbmi->segment_id,
                                                  1, row_c, col_c, tx_log2_c);
              eobtotal += reconstruct_inter_block(cm, xd, r, mbmi->segment_id,
                                                  2, row_c, col_c, tx_log2_c);
              tu_idx_c++;
            }
          }
        }

        // In 422 case, it's possilbe that Chroma has more TUs than Luma
        while (tu_idx_c < tu_num_c) {
          row_c = (tu_idx_c / tu_num_w_c) * tx_sz_c;
          col_c = (tu_idx_c % tu_num_w_c) * tx_sz_c;
          eobtotal += reconstruct_inter_block(cm, xd, r, mbmi->segment_id, 1,
                                              row_c, col_c, tx_log2_c);
          eobtotal += reconstruct_inter_block(cm, xd, r, mbmi->segment_id, 2,
                                              row_c, col_c, tx_log2_c);
          tu_idx_c++;
        }

        // TODO(CONFIG_COEF_INTERLEAVE owners): bring eob == 0 corner case
        // into line with the defaut configuration
        if (bsize >= BLOCK_8X8 && eobtotal == 0) mbmi->skip = 1;
      }
    }
  }
#else  // CONFIG_COEF_INTERLEAVE
  if (!is_inter_block(mbmi)) {
    int plane;

// TODO(anybody) : remove this flag when PVQ supports pallete coding tool
#if !CONFIG_PVQ
    for (plane = 0; plane <= 1; ++plane) {
      if (mbmi->palette_mode_info.palette_size[plane])
        av1_decode_palette_tokens(xd, plane, r);
    }
#endif  // #if !CONFIG_PVQ

    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
      const int stepr = tx_size_high_unit[tx_size];
      const int stepc = tx_size_wide_unit[tx_size];
#if CONFIG_CHROMA_SUB8X8
      const BLOCK_SIZE plane_bsize =
          AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#elif CONFIG_CB4X4
      const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
      const BLOCK_SIZE plane_bsize =
          get_plane_block_size(AOMMAX(BLOCK_8X8, bsize), pd);
#endif
      int row, col;
      const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
      const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
#if CONFIG_CB4X4
      if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                               pd->subsampling_y))
        continue;
#endif
      int blk_row, blk_col;
      const BLOCK_SIZE max_unit_bsize = get_plane_block_size(BLOCK_64X64, pd);
      int mu_blocks_wide =
          block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
      int mu_blocks_high =
          block_size_high[max_unit_bsize] >> tx_size_high_log2[0];
      mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
      mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

      for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
        const int unit_height = AOMMIN(mu_blocks_high + row, max_blocks_high);
        for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
          const int unit_width = AOMMIN(mu_blocks_wide + col, max_blocks_wide);

          for (blk_row = row; blk_row < unit_height; blk_row += stepr)
            for (blk_col = col; blk_col < unit_width; blk_col += stepc)
              predict_and_reconstruct_intra_block(cm, xd, r, mbmi, plane,
                                                  blk_row, blk_col, tx_size);
        }
      }
    }
  } else {
    int ref;

#if CONFIG_COMPOUND_SINGLEREF
    for (ref = 0; ref < 1 + is_inter_anyref_comp_mode(mbmi->mode); ++ref)
#else
    for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref)
#endif  // CONFIG_COMPOUND_SINGLEREF
    {
      const MV_REFERENCE_FRAME frame =
#if CONFIG_COMPOUND_SINGLEREF
          has_second_ref(mbmi) ? mbmi->ref_frame[ref] : mbmi->ref_frame[0];
#else
          mbmi->ref_frame[ref];
#endif  // CONFIG_COMPOUND_SINGLEREF
      if (frame < LAST_FRAME) {
#if CONFIG_INTRABC
        assert(is_intrabc_block(mbmi));
        assert(frame == INTRA_FRAME);
        assert(ref == 0);
#else
        assert(0);
#endif  // CONFIG_INTRABC
      } else {
        RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];

        xd->block_refs[ref] = ref_buf;
        if ((!av1_is_valid_scale(&ref_buf->sf)))
          aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                             "Reference frame has invalid dimensions");
        av1_setup_pre_planes(xd, ref, ref_buf->buf, mi_row, mi_col,
                             &ref_buf->sf);
      }
    }

#if CONFIG_CB4X4
    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
#else
    av1_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL,
                                  AOMMAX(bsize, BLOCK_8X8));
#endif

#if CONFIG_MOTION_VAR
    if (mbmi->motion_mode == OBMC_CAUSAL) {
#if CONFIG_NCOBMC
      av1_build_ncobmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
#else
      av1_build_obmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
#endif
    }
#endif  // CONFIG_MOTION_VAR
#if CONFIG_NCOBMC_ADAPT_WEIGHT
    if (mbmi->motion_mode == NCOBMC_ADAPT_WEIGHT) {
      int plane;
      recon_ncobmc_intrpl_pred(cm, xd, mi_row, mi_col, bsize);
      for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
        get_pred_from_intrpl_buf(xd, mi_row, mi_col, bsize, plane);
      }
    }
#endif
    // Reconstruction
    if (!mbmi->skip) {
      int eobtotal = 0;
      int plane;

      for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
        const struct macroblockd_plane *const pd = &xd->plane[plane];
#if CONFIG_CHROMA_SUB8X8
        const BLOCK_SIZE plane_bsize =
            AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#elif CONFIG_CB4X4
        const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
        const BLOCK_SIZE plane_bsize =
            get_plane_block_size(AOMMAX(BLOCK_8X8, bsize), pd);
#endif
        const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
        const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
        int row, col;

#if CONFIG_CB4X4
        if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                                 pd->subsampling_y))
          continue;
#endif

#if CONFIG_VAR_TX
        const BLOCK_SIZE max_unit_bsize = get_plane_block_size(BLOCK_64X64, pd);
        int mu_blocks_wide =
            block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
        int mu_blocks_high =
            block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

        mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
        mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

        const TX_SIZE max_tx_size = get_vartx_max_txsize(
            mbmi, plane_bsize, pd->subsampling_x || pd->subsampling_y);
        const int bh_var_tx = tx_size_high_unit[max_tx_size];
        const int bw_var_tx = tx_size_wide_unit[max_tx_size];
        int block = 0;
        int step =
            tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];

        for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
          for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
            int blk_row, blk_col;
            const int unit_height =
                AOMMIN(mu_blocks_high + row, max_blocks_high);
            const int unit_width =
                AOMMIN(mu_blocks_wide + col, max_blocks_wide);
            for (blk_row = row; blk_row < unit_height; blk_row += bh_var_tx) {
              for (blk_col = col; blk_col < unit_width; blk_col += bw_var_tx) {
                decode_reconstruct_tx(cm, xd, r, mbmi, plane, plane_bsize,
                                      blk_row, blk_col, block, max_tx_size,
                                      &eobtotal);
                block += step;
              }
            }
          }
        }
#else
        const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
        const int stepr = tx_size_high_unit[tx_size];
        const int stepc = tx_size_wide_unit[tx_size];
        for (row = 0; row < max_blocks_high; row += stepr)
          for (col = 0; col < max_blocks_wide; col += stepc)
            eobtotal += reconstruct_inter_block(cm, xd, r, mbmi->segment_id,
                                                plane, row, col, tx_size);
#endif
      }
    }
  }
#if CONFIG_CFL && CONFIG_CHROMA_SUB8X8
  if (mbmi->uv_mode != UV_CFL_PRED) {
#if CONFIG_DEBUG
    if (cfl->is_chroma_reference) {
      cfl_clear_sub8x8_val(cfl);
    }
#endif
    if (!cfl->is_chroma_reference && is_inter_block(mbmi)) {
      cfl_store_block(xd, mbmi->sb_type, mbmi->tx_size);
    }
  }
#endif  // CONFIG_CFL && CONFIG_CHROMA_SUB8X8
#endif  // CONFIG_COEF_INTERLEAVE

  int reader_corrupted_flag = aom_reader_has_error(r);
  aom_merge_corrupted_flag(&xd->corrupted, reader_corrupted_flag);
}

#if NC_MODE_INFO && CONFIG_MOTION_VAR
static void detoken_and_recon_sb(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                                 int mi_row, int mi_col, aom_reader *r,
                                 BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const int hbs = mi_size_wide[bsize] >> 1;
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
#if CONFIG_EXT_PARTITION_TYPES
  BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  partition = get_partition(cm, mi_row, mi_col, bsize);
  subsize = subsize_lookup[partition][bsize];

  if (!hbs && !unify_bsize) {
    xd->bmode_blocks_wl = 1 >> !!(partition & PARTITION_VERT);
    xd->bmode_blocks_hl = 1 >> !!(partition & PARTITION_HORZ);
    decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, subsize);
  } else {
    switch (partition) {
      case PARTITION_NONE:
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, bsize);
        break;
      case PARTITION_HORZ:
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, subsize);
        if (has_rows)
          decode_token_and_recon_block(pbi, xd, mi_row + hbs, mi_col, r,
                                       subsize);
        break;
      case PARTITION_VERT:
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, subsize);
        if (has_cols)
          decode_token_and_recon_block(pbi, xd, mi_row, mi_col + hbs, r,
                                       subsize);
        break;
      case PARTITION_SPLIT:
        detoken_and_recon_sb(pbi, xd, mi_row, mi_col, r, subsize);
        detoken_and_recon_sb(pbi, xd, mi_row, mi_col + hbs, r, subsize);
        detoken_and_recon_sb(pbi, xd, mi_row + hbs, mi_col, r, subsize);
        detoken_and_recon_sb(pbi, xd, mi_row + hbs, mi_col + hbs, r, subsize);
        break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
#error NC_MODE_INFO+MOTION_VAR not yet supported for new HORZ/VERT_AB partitions
#endif
      case PARTITION_HORZ_A:
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, bsize2);
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col + hbs, r, bsize2);
        decode_token_and_recon_block(pbi, xd, mi_row + hbs, mi_col, r, subsize);
        break;
      case PARTITION_HORZ_B:
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, subsize);
        decode_token_and_recon_block(pbi, xd, mi_row + hbs, mi_col, r, bsize2);
        decode_token_and_recon_block(pbi, xd, mi_row + hbs, mi_col + hbs, r,
                                     bsize2);
        break;
      case PARTITION_VERT_A:
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, bsize2);
        decode_token_and_recon_block(pbi, xd, mi_row + hbs, mi_col, r, bsize2);
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col + hbs, r, subsize);
        break;
      case PARTITION_VERT_B:
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, subsize);
        decode_token_and_recon_block(pbi, xd, mi_row, mi_col + hbs, r, bsize2);
        decode_token_and_recon_block(pbi, xd, mi_row + hbs, mi_col + hbs, r,
                                     bsize2);
        break;
#endif
      default: assert(0 && "Invalid partition type");
    }
  }
}
#endif

static void decode_block(AV1Decoder *const pbi, MACROBLOCKD *const xd,
#if CONFIG_SUPERTX
                         int supertx_enabled,
#endif  // CONFIG_SUPERTX
                         int mi_row, int mi_col, aom_reader *r,
#if CONFIG_EXT_PARTITION_TYPES
                         PARTITION_TYPE partition,
#endif  // CONFIG_EXT_PARTITION_TYPES
                         BLOCK_SIZE bsize) {
  decode_mbmi_block(pbi, xd,
#if CONFIG_SUPERTX
                    supertx_enabled,
#endif
                    mi_row, mi_col, r,
#if CONFIG_EXT_PARTITION_TYPES
                    partition,
#endif
                    bsize);

#if !(CONFIG_MOTION_VAR && NC_MODE_INFO)
#if CONFIG_SUPERTX
  if (!supertx_enabled)
#endif  // CONFIG_SUPERTX
    decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, bsize);
#endif
}

static PARTITION_TYPE read_partition(AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col, aom_reader *r,
                                     int has_rows, int has_cols,
                                     BLOCK_SIZE bsize) {
#if CONFIG_UNPOISON_PARTITION_CTX
  const int ctx =
      partition_plane_context(xd, mi_row, mi_col, has_rows, has_cols, bsize);
#else
  const int ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
#endif
  PARTITION_TYPE p;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)cm;

  aom_cdf_prob *partition_cdf = (ctx >= 0) ? ec_ctx->partition_cdf[ctx] : NULL;

  if (has_rows && has_cols) {
#if CONFIG_EXT_PARTITION_TYPES
    const int num_partition_types =
        (mi_width_log2_lookup[bsize] > mi_width_log2_lookup[BLOCK_8X8])
            ? EXT_PARTITION_TYPES
            : PARTITION_TYPES;
#else
    const int num_partition_types = PARTITION_TYPES;
#endif  // CONFIG_EXT_PARTITION_TYPES
    p = (PARTITION_TYPE)aom_read_symbol(r, partition_cdf, num_partition_types,
                                        ACCT_STR);
  } else if (!has_rows && has_cols) {
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_vert_alike(cdf, partition_cdf);
    assert(cdf[1] == AOM_ICDF(CDF_PROB_TOP));
    p = aom_read_cdf(r, cdf, 2, ACCT_STR) ? PARTITION_SPLIT : PARTITION_HORZ;
    // gather cols
  } else if (has_rows && !has_cols) {
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_horz_alike(cdf, partition_cdf);
    assert(cdf[1] == AOM_ICDF(CDF_PROB_TOP));
    p = aom_read_cdf(r, cdf, 2, ACCT_STR) ? PARTITION_SPLIT : PARTITION_VERT;
  } else {
    p = PARTITION_SPLIT;
  }

  return p;
}

#if CONFIG_SUPERTX
static int read_skip(AV1_COMMON *cm, const MACROBLOCKD *xd, int segment_id,
                     aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int ctx = av1_get_skip_context(xd);
#if CONFIG_NEW_MULTISYMBOL
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    const int skip = aom_read_symbol(r, ec_ctx->skip_cdfs[ctx], 2, ACCT_STR);
#else
    const int skip = aom_read(r, cm->fc->skip_probs[ctx], ACCT_STR);
#endif
    FRAME_COUNTS *counts = xd->counts;
    if (counts) ++counts->skip[ctx][skip];
    return skip;
  }
}
#endif  // CONFIG_SUPERTX

// TODO(slavarnway): eliminate bsize and subsize in future commits
static void decode_partition(AV1Decoder *const pbi, MACROBLOCKD *const xd,
#if CONFIG_SUPERTX
                             int supertx_enabled,
#endif
                             int mi_row, int mi_col, aom_reader *r,
                             BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const int num_8x8_wh = mi_size_wide[bsize];
  const int hbs = num_8x8_wh >> 1;
#if CONFIG_EXT_PARTITION_TYPES && CONFIG_EXT_PARTITION_TYPES_AB
  const int qbs = num_8x8_wh >> 2;
#endif
#if CONFIG_CB4X4
  const int unify_bsize = 1;
#else
  const int unify_bsize = 0;
#endif
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
#if CONFIG_EXT_PARTITION_TYPES
  const int quarter_step = num_8x8_wh / 4;
  int i;
#if !CONFIG_EXT_PARTITION_TYPES_AB
  BLOCK_SIZE bsize2 = get_subsize(bsize, PARTITION_SPLIT);
#endif
#endif
  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;
#if CONFIG_SUPERTX
  const int read_token = !supertx_enabled;
  int skip = 0;
  TX_SIZE supertx_size = max_txsize_lookup[bsize];
  const TileInfo *const tile = &xd->tile;
  int txfm = DCT_DCT;
#endif  // CONFIG_SUPERTX

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  partition = (bsize < BLOCK_8X8) ? PARTITION_NONE
                                  : read_partition(cm, xd, mi_row, mi_col, r,
                                                   has_rows, has_cols, bsize);
  subsize = subsize_lookup[partition][bsize];  // get_subsize(bsize, partition);

  // Check the bitstream is conformant: if there is subsampling on the
  // chroma planes, subsize must subsample to a valid block size.
  const struct macroblockd_plane *const pd_u = &xd->plane[1];
  if (get_plane_block_size(subsize, pd_u) == BLOCK_INVALID) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Block size %dx%d invalid with this subsampling mode",
                       block_size_wide[subsize], block_size_high[subsize]);
  }

#if CONFIG_PVQ
  assert(partition < PARTITION_TYPES);
  assert(subsize < BLOCK_SIZES_ALL);
#endif
#if CONFIG_SUPERTX
  if (!frame_is_intra_only(cm) && partition != PARTITION_NONE &&
      bsize <= MAX_SUPERTX_BLOCK_SIZE && !supertx_enabled && !xd->lossless[0]) {
    const int supertx_context = partition_supertx_context_lookup[partition];
    supertx_enabled = aom_read(
        r, cm->fc->supertx_prob[supertx_context][supertx_size], ACCT_STR);
    if (xd->counts)
      xd->counts->supertx[supertx_context][supertx_size][supertx_enabled]++;
#if CONFIG_VAR_TX
    if (supertx_enabled) xd->supertx_size = supertx_size;
#endif
  }
#endif  // CONFIG_SUPERTX

#if CONFIG_SUPERTX
#define DEC_BLOCK_STX_ARG supertx_enabled,
#else
#define DEC_BLOCK_STX_ARG
#endif
#if CONFIG_EXT_PARTITION_TYPES
#define DEC_BLOCK_EPT_ARG partition,
#else
#define DEC_BLOCK_EPT_ARG
#endif
#define DEC_BLOCK(db_r, db_c, db_subsize)                   \
  decode_block(pbi, xd, DEC_BLOCK_STX_ARG(db_r), (db_c), r, \
               DEC_BLOCK_EPT_ARG(db_subsize))
#define DEC_PARTITION(db_r, db_c, db_subsize) \
  decode_partition(pbi, xd, DEC_BLOCK_STX_ARG(db_r), (db_c), r, (db_subsize))

  if (!hbs && !unify_bsize) {
    // calculate bmode block dimensions (log 2)
    xd->bmode_blocks_wl = 1 >> !!(partition & PARTITION_VERT);
    xd->bmode_blocks_hl = 1 >> !!(partition & PARTITION_HORZ);
    DEC_BLOCK(mi_row, mi_col, subsize);
  } else {
    switch (partition) {
      case PARTITION_NONE: DEC_BLOCK(mi_row, mi_col, subsize); break;
      case PARTITION_HORZ:
        DEC_BLOCK(mi_row, mi_col, subsize);
        if (has_rows) DEC_BLOCK(mi_row + hbs, mi_col, subsize);
        break;
      case PARTITION_VERT:
        DEC_BLOCK(mi_row, mi_col, subsize);
        if (has_cols) DEC_BLOCK(mi_row, mi_col + hbs, subsize);
        break;
      case PARTITION_SPLIT:
        DEC_PARTITION(mi_row, mi_col, subsize);
        DEC_PARTITION(mi_row, mi_col + hbs, subsize);
        DEC_PARTITION(mi_row + hbs, mi_col, subsize);
        DEC_PARTITION(mi_row + hbs, mi_col + hbs, subsize);
        break;
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION_TYPES_AB
      case PARTITION_HORZ_A:
        DEC_BLOCK(mi_row, mi_col, get_subsize(bsize, PARTITION_HORZ_4));
        DEC_BLOCK(mi_row + qbs, mi_col, get_subsize(bsize, PARTITION_HORZ_4));
        DEC_BLOCK(mi_row + hbs, mi_col, subsize);
        break;
      case PARTITION_HORZ_B:
        DEC_BLOCK(mi_row, mi_col, subsize);
        DEC_BLOCK(mi_row + hbs, mi_col, get_subsize(bsize, PARTITION_HORZ_4));
        if (mi_row + 3 * qbs < cm->mi_rows)
          DEC_BLOCK(mi_row + 3 * qbs, mi_col,
                    get_subsize(bsize, PARTITION_HORZ_4));
        break;
      case PARTITION_VERT_A:
        DEC_BLOCK(mi_row, mi_col, get_subsize(bsize, PARTITION_VERT_4));
        DEC_BLOCK(mi_row, mi_col + qbs, get_subsize(bsize, PARTITION_VERT_4));
        DEC_BLOCK(mi_row, mi_col + hbs, subsize);
        break;
      case PARTITION_VERT_B:
        DEC_BLOCK(mi_row, mi_col, subsize);
        DEC_BLOCK(mi_row, mi_col + hbs, get_subsize(bsize, PARTITION_VERT_4));
        if (mi_col + 3 * qbs < cm->mi_cols)
          DEC_BLOCK(mi_row, mi_col + 3 * qbs,
                    get_subsize(bsize, PARTITION_VERT_4));
        break;
#else
      case PARTITION_HORZ_A:
        DEC_BLOCK(mi_row, mi_col, bsize2);
        DEC_BLOCK(mi_row, mi_col + hbs, bsize2);
        DEC_BLOCK(mi_row + hbs, mi_col, subsize);
        break;
      case PARTITION_HORZ_B:
        DEC_BLOCK(mi_row, mi_col, subsize);
        DEC_BLOCK(mi_row + hbs, mi_col, bsize2);
        DEC_BLOCK(mi_row + hbs, mi_col + hbs, bsize2);
        break;
      case PARTITION_VERT_A:
        DEC_BLOCK(mi_row, mi_col, bsize2);
        DEC_BLOCK(mi_row + hbs, mi_col, bsize2);
        DEC_BLOCK(mi_row, mi_col + hbs, subsize);
        break;
      case PARTITION_VERT_B:
        DEC_BLOCK(mi_row, mi_col, subsize);
        DEC_BLOCK(mi_row, mi_col + hbs, bsize2);
        DEC_BLOCK(mi_row + hbs, mi_col + hbs, bsize2);
        break;
#endif
      case PARTITION_HORZ_4:
        for (i = 0; i < 4; ++i) {
          int this_mi_row = mi_row + i * quarter_step;
          if (i > 0 && this_mi_row >= cm->mi_rows) break;
          DEC_BLOCK(this_mi_row, mi_col, subsize);
        }
        break;
      case PARTITION_VERT_4:
        for (i = 0; i < 4; ++i) {
          int this_mi_col = mi_col + i * quarter_step;
          if (i > 0 && this_mi_col >= cm->mi_cols) break;
          DEC_BLOCK(mi_row, this_mi_col, subsize);
        }
        break;
#endif  // CONFIG_EXT_PARTITION_TYPES
      default: assert(0 && "Invalid partition type");
    }
  }

#undef DEC_PARTITION
#undef DEC_BLOCK
#undef DEC_BLOCK_EPT_ARG
#undef DEC_BLOCK_STX_ARG

#if CONFIG_SUPERTX
  if (supertx_enabled && read_token) {
    uint8_t *dst_buf[3];
    int dst_stride[3], i;
    int offset = mi_row * cm->mi_stride + mi_col;

    set_segment_id_supertx(cm, mi_row, mi_col, bsize);

    if (cm->delta_q_present_flag) {
      for (i = 0; i < MAX_SEGMENTS; i++) {
        int j;
        for (j = 0; j < MAX_MB_PLANE; ++j) {
          const int dc_delta_q = j == 0 ? cm->y_dc_delta_q : cm->uv_dc_delta_q;
          const int ac_delta_q = j == 0 ? 0 : cm->uv_ac_delta_q;

          xd->plane[j].seg_dequant[i][0] =
              av1_dc_quant(xd->current_qindex, dc_delta_q, cm->bit_depth);
          xd->plane[j].seg_dequant[i][1] =
              av1_ac_quant(xd->current_qindex, ac_delta_q, cm->bit_depth);
        }
      }
    }

    xd->mi = cm->mi_grid_visible + offset;
    xd->mi[0] = cm->mi + offset;
    set_mi_row_col(xd, tile, mi_row, mi_size_high[bsize], mi_col,
                   mi_size_wide[bsize],
#if CONFIG_DEPENDENT_HORZTILES
                   cm->dependent_horz_tiles,
#endif  // CONFIG_DEPENDENT_HORZTILES
                   cm->mi_rows, cm->mi_cols);
    set_skip_context(xd, mi_row, mi_col);
    skip = read_skip(cm, xd, xd->mi[0]->mbmi.segment_id_supertx, r);
    if (skip) {
      av1_reset_skip_context(xd, mi_row, mi_col, bsize);
    } else {
      FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
#if CONFIG_EXT_TX
      if (get_ext_tx_types(supertx_size, bsize, 1, cm->reduced_tx_set_used) >
          1) {
        const int eset =
            get_ext_tx_set(supertx_size, bsize, 1, cm->reduced_tx_set_used);
        if (eset > 0) {
          const TxSetType tx_set_type = get_ext_tx_set_type(
              supertx_size, bsize, 1, cm->reduced_tx_set_used);
          const int packed_sym =
              aom_read_symbol(r, ec_ctx->inter_ext_tx_cdf[eset][supertx_size],
                              av1_num_ext_tx_set[tx_set_type], ACCT_STR);
          txfm = av1_ext_tx_inv[tx_set_type][packed_sym];
#if CONFIG_ENTROPY_STATS
          if (xd->counts) ++xd->counts->inter_ext_tx[eset][supertx_size][txfm];
#endif  // CONFIG_ENTROPY_STATS
        }
      }
#else
      if (supertx_size < TX_32X32) {
        txfm = aom_read_symbol(r, ec_ctx->inter_ext_tx_cdf[supertx_size],
                               TX_TYPES, ACCT_STR);
#if CONFIG_ENTROPY_STATS
        if (xd->counts) ++xd->counts->inter_ext_tx[supertx_size][txfm];
#endif  // CONFIG_ENTROPY_STATS
      }
#endif  // CONFIG_EXT_TX
    }

    av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                         mi_col);
    for (i = 0; i < MAX_MB_PLANE; i++) {
      dst_buf[i] = xd->plane[i].dst.buf;
      dst_stride[i] = xd->plane[i].dst.stride;
    }
    dec_predict_sb_complex(pbi, xd, tile, mi_row, mi_col, mi_row, mi_col, bsize,
                           bsize, dst_buf, dst_stride);

    if (!skip) {
      int eobtotal = 0;
      MB_MODE_INFO *mbmi;
      set_offsets_topblock(cm, xd, tile, bsize, mi_row, mi_col);
      mbmi = &xd->mi[0]->mbmi;
      mbmi->tx_type = txfm;
      assert(mbmi->segment_id_supertx != MAX_SEGMENTS);
      for (i = 0; i < MAX_MB_PLANE; ++i) {
        const struct macroblockd_plane *const pd = &xd->plane[i];
        int row, col;
        const TX_SIZE tx_size = av1_get_tx_size(i, xd);
        const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
        const int stepr = tx_size_high_unit[tx_size];
        const int stepc = tx_size_wide_unit[tx_size];
        const int max_blocks_wide = max_block_wide(xd, plane_bsize, i);
        const int max_blocks_high = max_block_high(xd, plane_bsize, i);

        for (row = 0; row < max_blocks_high; row += stepr)
          for (col = 0; col < max_blocks_wide; col += stepc)
            eobtotal += reconstruct_inter_block(
                cm, xd, r, mbmi->segment_id_supertx, i, row, col, tx_size);
      }
      if ((unify_bsize || !(subsize < BLOCK_8X8)) && eobtotal == 0) skip = 1;
    }
    set_param_topblock(cm, xd, bsize, mi_row, mi_col, txfm, skip);
  }
#endif  // CONFIG_SUPERTX

#if CONFIG_EXT_PARTITION_TYPES
  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
#else
  // update partition context
  if (bsize >= BLOCK_8X8 &&
      (bsize == BLOCK_8X8 || partition != PARTITION_SPLIT))
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_LPF_SB
  if (bsize == cm->sb_size) {
    int filt_lvl;
    if (mi_row == 0 && mi_col == 0) {
      filt_lvl = aom_read_literal(r, 6, ACCT_STR);
      cm->mi_grid_visible[0]->mbmi.reuse_sb_lvl = 0;
      cm->mi_grid_visible[0]->mbmi.delta = 0;
      cm->mi_grid_visible[0]->mbmi.sign = 0;
    } else {
      int prev_mi_row, prev_mi_col;
      if (mi_col - MAX_MIB_SIZE < 0) {
        prev_mi_row = mi_row - MAX_MIB_SIZE;
        prev_mi_col = mi_col;
      } else {
        prev_mi_row = mi_row;
        prev_mi_col = mi_col - MAX_MIB_SIZE;
      }

      MB_MODE_INFO *curr_mbmi =
          &cm->mi_grid_visible[mi_row * cm->mi_stride + mi_col]->mbmi;
      MB_MODE_INFO *prev_mbmi =
          &cm->mi_grid_visible[prev_mi_row * cm->mi_stride + prev_mi_col]->mbmi;
      const uint8_t prev_lvl = prev_mbmi->filt_lvl;

      const int reuse_ctx = prev_mbmi->reuse_sb_lvl;
      const int reuse_prev_lvl = aom_read_symbol(
          r, xd->tile_ctx->lpf_reuse_cdf[reuse_ctx], 2, ACCT_STR);
      curr_mbmi->reuse_sb_lvl = reuse_prev_lvl;

      if (reuse_prev_lvl) {
        filt_lvl = prev_lvl;
        curr_mbmi->delta = 0;
        curr_mbmi->sign = 0;
      } else {
        const int delta_ctx = prev_mbmi->delta;
        unsigned int delta = aom_read_symbol(
            r, xd->tile_ctx->lpf_delta_cdf[delta_ctx], DELTA_RANGE, ACCT_STR);
        curr_mbmi->delta = delta;
        delta *= LPF_STEP;

        if (delta) {
          const int sign_ctx = prev_mbmi->sign;
          const int sign = aom_read_symbol(
              r, xd->tile_ctx->lpf_sign_cdf[reuse_ctx][sign_ctx], 2, ACCT_STR);
          curr_mbmi->sign = sign;
          filt_lvl = sign ? prev_lvl + delta : prev_lvl - delta;
        } else {
          filt_lvl = prev_lvl;
          curr_mbmi->sign = 0;
        }
      }
    }

    av1_loop_filter_sb_level_init(cm, mi_row, mi_col, filt_lvl);
  }
#endif

#if CONFIG_CDEF
  if (bsize == cm->sb_size) {
    int width_step = mi_size_wide[BLOCK_64X64];
    int height_step = mi_size_wide[BLOCK_64X64];
    int w, h;
    for (h = 0; (h < mi_size_high[cm->sb_size]) && (mi_row + h < cm->mi_rows);
         h += height_step) {
      for (w = 0; (w < mi_size_wide[cm->sb_size]) && (mi_col + w < cm->mi_cols);
           w += width_step) {
        if (!cm->all_lossless && !sb_all_skip(cm, mi_row + h, mi_col + w))
          cm->mi_grid_visible[(mi_row + h) * cm->mi_stride + (mi_col + w)]
              ->mbmi.cdef_strength =
              aom_read_literal(r, cm->cdef_bits, ACCT_STR);
        else
          cm->mi_grid_visible[(mi_row + h) * cm->mi_stride + (mi_col + w)]
              ->mbmi.cdef_strength = -1;
      }
    }
  }
#endif  // CONFIG_CDEF
#if CONFIG_LOOP_RESTORATION
  for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
    int rcol0, rcol1, rrow0, rrow1, nhtiles;
    if (av1_loop_restoration_corners_in_sb(cm, plane, mi_row, mi_col, bsize,
                                           &rcol0, &rcol1, &rrow0, &rrow1,
                                           &nhtiles)) {
      for (int rrow = rrow0; rrow < rrow1; ++rrow) {
        for (int rcol = rcol0; rcol < rcol1; ++rcol) {
          int rtile_idx = rcol + rrow * nhtiles;
          loop_restoration_read_sb_coeffs(cm, xd, r, plane, rtile_idx);
        }
      }
    }
  }
#endif
}

static void setup_bool_decoder(const uint8_t *data, const uint8_t *data_end,
                               const size_t read_size,
                               struct aom_internal_error_info *error_info,
                               aom_reader *r,
#if CONFIG_ANS && ANS_MAX_SYMBOLS
                               int window_size,
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
                               aom_decrypt_cb decrypt_cb, void *decrypt_state) {
  // Validate the calculated partition length. If the buffer
  // described by the partition can't be fully read, then restrict
  // it to the portion that can be (for EC mode) or throw an error.
  if (!read_is_valid(data, read_size, data_end))
    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile length");

#if CONFIG_ANS && ANS_MAX_SYMBOLS
  r->window_size = window_size;
#endif
  if (aom_reader_init(r, data, read_size, decrypt_cb, decrypt_state))
    aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate bool decoder %d", 1);
}

static void setup_segmentation(AV1_COMMON *const cm,
                               struct aom_read_bit_buffer *rb) {
  struct segmentation *const seg = &cm->seg;
  int i, j;

  seg->update_map = 0;
  seg->update_data = 0;
  seg->temporal_update = 0;

  seg->enabled = aom_rb_read_bit(rb);
  if (!seg->enabled) return;

  // Segmentation map update
  if (frame_is_intra_only(cm) || cm->error_resilient_mode) {
    seg->update_map = 1;
  } else {
    seg->update_map = aom_rb_read_bit(rb);
  }
  if (seg->update_map) {
    if (frame_is_intra_only(cm) || cm->error_resilient_mode) {
      seg->temporal_update = 0;
    } else {
      seg->temporal_update = aom_rb_read_bit(rb);
    }
  }

  // Segmentation data update
  seg->update_data = aom_rb_read_bit(rb);
  if (seg->update_data) {
    seg->abs_delta = aom_rb_read_bit(rb);

    av1_clearall_segfeatures(seg);

    for (i = 0; i < MAX_SEGMENTS; i++) {
      for (j = 0; j < SEG_LVL_MAX; j++) {
        int data = 0;
        const int feature_enabled = aom_rb_read_bit(rb);
        if (feature_enabled) {
          av1_enable_segfeature(seg, i, j);
          data = decode_unsigned_max(rb, av1_seg_feature_data_max(j));
          if (av1_is_segfeature_signed(j))
            data = aom_rb_read_bit(rb) ? -data : data;
        }
        av1_set_segdata(seg, i, j, data);
      }
    }
  }
}

#if CONFIG_LOOP_RESTORATION
static void decode_restoration_mode(AV1_COMMON *cm,
                                    struct aom_read_bit_buffer *rb) {
  int p;
  RestorationInfo *rsi = &cm->rst_info[0];
  if (aom_rb_read_bit(rb)) {
    rsi->frame_restoration_type =
        aom_rb_read_bit(rb) ? RESTORE_SGRPROJ : RESTORE_WIENER;
  } else {
    rsi->frame_restoration_type =
        aom_rb_read_bit(rb) ? RESTORE_SWITCHABLE : RESTORE_NONE;
  }
  for (p = 1; p < MAX_MB_PLANE; ++p) {
    rsi = &cm->rst_info[p];
    if (aom_rb_read_bit(rb)) {
      rsi->frame_restoration_type =
          aom_rb_read_bit(rb) ? RESTORE_SGRPROJ : RESTORE_WIENER;
    } else {
      rsi->frame_restoration_type = RESTORE_NONE;
    }
  }

  cm->rst_info[0].restoration_tilesize = RESTORATION_TILESIZE_MAX;
  cm->rst_info[1].restoration_tilesize = RESTORATION_TILESIZE_MAX;
  cm->rst_info[2].restoration_tilesize = RESTORATION_TILESIZE_MAX;
  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    rsi = &cm->rst_info[0];
    rsi->restoration_tilesize >>= aom_rb_read_bit(rb);
    if (rsi->restoration_tilesize != RESTORATION_TILESIZE_MAX) {
      rsi->restoration_tilesize >>= aom_rb_read_bit(rb);
    }
  }
  int s = AOMMIN(cm->subsampling_x, cm->subsampling_y);
  if (s && (cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
            cm->rst_info[2].frame_restoration_type != RESTORE_NONE)) {
    cm->rst_info[1].restoration_tilesize =
        cm->rst_info[0].restoration_tilesize >> (aom_rb_read_bit(rb) * s);
  } else {
    cm->rst_info[1].restoration_tilesize = cm->rst_info[0].restoration_tilesize;
  }
  cm->rst_info[2].restoration_tilesize = cm->rst_info[1].restoration_tilesize;

  cm->rst_info[0].procunit_width = cm->rst_info[0].procunit_height =
      RESTORATION_PROC_UNIT_SIZE;
  cm->rst_info[1].procunit_width = cm->rst_info[2].procunit_width =
      RESTORATION_PROC_UNIT_SIZE >> cm->subsampling_x;
  cm->rst_info[1].procunit_height = cm->rst_info[2].procunit_height =
      RESTORATION_PROC_UNIT_SIZE >> cm->subsampling_y;
}

static void read_wiener_filter(int wiener_win, WienerInfo *wiener_info,
                               WienerInfo *ref_wiener_info, aom_reader *rb) {
  if (wiener_win == WIENER_WIN)
    wiener_info->vfilter[0] = wiener_info->vfilter[WIENER_WIN - 1] =
        aom_read_primitive_refsubexpfin(
            rb, WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
            WIENER_FILT_TAP0_SUBEXP_K,
            ref_wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV, ACCT_STR) +
        WIENER_FILT_TAP0_MINV;
  else
    wiener_info->vfilter[0] = wiener_info->vfilter[WIENER_WIN - 1] = 0;
  wiener_info->vfilter[1] = wiener_info->vfilter[WIENER_WIN - 2] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
          WIENER_FILT_TAP1_SUBEXP_K,
          ref_wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV, ACCT_STR) +
      WIENER_FILT_TAP1_MINV;
  wiener_info->vfilter[2] = wiener_info->vfilter[WIENER_WIN - 3] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
          WIENER_FILT_TAP2_SUBEXP_K,
          ref_wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV, ACCT_STR) +
      WIENER_FILT_TAP2_MINV;
  // The central element has an implicit +WIENER_FILT_STEP
  wiener_info->vfilter[WIENER_HALFWIN] =
      -2 * (wiener_info->vfilter[0] + wiener_info->vfilter[1] +
            wiener_info->vfilter[2]);

  if (wiener_win == WIENER_WIN)
    wiener_info->hfilter[0] = wiener_info->hfilter[WIENER_WIN - 1] =
        aom_read_primitive_refsubexpfin(
            rb, WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
            WIENER_FILT_TAP0_SUBEXP_K,
            ref_wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV, ACCT_STR) +
        WIENER_FILT_TAP0_MINV;
  else
    wiener_info->hfilter[0] = wiener_info->hfilter[WIENER_WIN - 1] = 0;
  wiener_info->hfilter[1] = wiener_info->hfilter[WIENER_WIN - 2] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
          WIENER_FILT_TAP1_SUBEXP_K,
          ref_wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV, ACCT_STR) +
      WIENER_FILT_TAP1_MINV;
  wiener_info->hfilter[2] = wiener_info->hfilter[WIENER_WIN - 3] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
          WIENER_FILT_TAP2_SUBEXP_K,
          ref_wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV, ACCT_STR) +
      WIENER_FILT_TAP2_MINV;
  // The central element has an implicit +WIENER_FILT_STEP
  wiener_info->hfilter[WIENER_HALFWIN] =
      -2 * (wiener_info->hfilter[0] + wiener_info->hfilter[1] +
            wiener_info->hfilter[2]);
  memcpy(ref_wiener_info, wiener_info, sizeof(*wiener_info));
}

static void read_sgrproj_filter(SgrprojInfo *sgrproj_info,
                                SgrprojInfo *ref_sgrproj_info, aom_reader *rb) {
  sgrproj_info->ep = aom_read_literal(rb, SGRPROJ_PARAMS_BITS, ACCT_STR);
  sgrproj_info->xqd[0] =
      aom_read_primitive_refsubexpfin(
          rb, SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
          ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0, ACCT_STR) +
      SGRPROJ_PRJ_MIN0;
  sgrproj_info->xqd[1] =
      aom_read_primitive_refsubexpfin(
          rb, SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
          ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1, ACCT_STR) +
      SGRPROJ_PRJ_MIN1;
  memcpy(ref_sgrproj_info, sgrproj_info, sizeof(*sgrproj_info));
}

static void loop_restoration_read_sb_coeffs(const AV1_COMMON *const cm,
                                            MACROBLOCKD *xd,
                                            aom_reader *const r, int plane,
                                            int rtile_idx) {
  const RestorationInfo *rsi = cm->rst_info + plane;
  if (rsi->frame_restoration_type == RESTORE_NONE) return;

  const int wiener_win = (plane > 0) ? WIENER_WIN_CHROMA : WIENER_WIN;
  WienerInfo *wiener_info = xd->wiener_info + plane;
  SgrprojInfo *sgrproj_info = xd->sgrproj_info + plane;

  if (rsi->frame_restoration_type == RESTORE_SWITCHABLE) {
    assert(plane == 0);
    rsi->restoration_type[rtile_idx] =
        aom_read_tree(r, av1_switchable_restore_tree,
                      cm->fc->switchable_restore_prob, ACCT_STR);

    if (rsi->restoration_type[rtile_idx] == RESTORE_WIENER) {
      read_wiener_filter(wiener_win, &rsi->wiener_info[rtile_idx], wiener_info,
                         r);
    } else if (rsi->restoration_type[rtile_idx] == RESTORE_SGRPROJ) {
      read_sgrproj_filter(&rsi->sgrproj_info[rtile_idx], sgrproj_info, r);
    }
  } else if (rsi->frame_restoration_type == RESTORE_WIENER) {
    if (aom_read(r, RESTORE_NONE_WIENER_PROB, ACCT_STR)) {
      rsi->restoration_type[rtile_idx] = RESTORE_WIENER;
      read_wiener_filter(wiener_win, &rsi->wiener_info[rtile_idx], wiener_info,
                         r);
    } else {
      rsi->restoration_type[rtile_idx] = RESTORE_NONE;
    }
  } else if (rsi->frame_restoration_type == RESTORE_SGRPROJ) {
    if (aom_read(r, RESTORE_NONE_SGRPROJ_PROB, ACCT_STR)) {
      rsi->restoration_type[rtile_idx] = RESTORE_SGRPROJ;
      read_sgrproj_filter(&rsi->sgrproj_info[rtile_idx], sgrproj_info, r);
    } else {
      rsi->restoration_type[rtile_idx] = RESTORE_NONE;
    }
  }
}
#endif  // CONFIG_LOOP_RESTORATION

static void setup_loopfilter(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  struct loopfilter *lf = &cm->lf;
#if !CONFIG_LPF_SB
#if CONFIG_LOOPFILTER_LEVEL
  lf->filter_level[0] = aom_rb_read_literal(rb, 6);
  lf->filter_level[1] = aom_rb_read_literal(rb, 6);
  if (lf->filter_level[0] || lf->filter_level[1]) {
    lf->filter_level_u = aom_rb_read_literal(rb, 6);
    lf->filter_level_v = aom_rb_read_literal(rb, 6);
  }
#else
  lf->filter_level = aom_rb_read_literal(rb, 6);
#endif
#endif  // CONFIG_LPF_SB
  lf->sharpness_level = aom_rb_read_literal(rb, 3);

  // Read in loop filter deltas applied at the MB level based on mode or ref
  // frame.
  lf->mode_ref_delta_update = 0;

  lf->mode_ref_delta_enabled = aom_rb_read_bit(rb);
  if (lf->mode_ref_delta_enabled) {
    lf->mode_ref_delta_update = aom_rb_read_bit(rb);
    if (lf->mode_ref_delta_update) {
      int i;

      for (i = 0; i < TOTAL_REFS_PER_FRAME; i++)
        if (aom_rb_read_bit(rb))
          lf->ref_deltas[i] = aom_rb_read_inv_signed_literal(rb, 6);

      for (i = 0; i < MAX_MODE_LF_DELTAS; i++)
        if (aom_rb_read_bit(rb))
          lf->mode_deltas[i] = aom_rb_read_inv_signed_literal(rb, 6);
    }
  }
}

#if CONFIG_CDEF
static void setup_cdef(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  int i;
#if CONFIG_CDEF_SINGLEPASS
  cm->cdef_pri_damping = cm->cdef_sec_damping = aom_rb_read_literal(rb, 2) + 3;
#else
  cm->cdef_pri_damping = aom_rb_read_literal(rb, 1) + 5;
  cm->cdef_sec_damping = aom_rb_read_literal(rb, 2) + 3;
#endif
  cm->cdef_bits = aom_rb_read_literal(rb, 2);
  cm->nb_cdef_strengths = 1 << cm->cdef_bits;
  for (i = 0; i < cm->nb_cdef_strengths; i++) {
    cm->cdef_strengths[i] = aom_rb_read_literal(rb, CDEF_STRENGTH_BITS);
    cm->cdef_uv_strengths[i] = cm->subsampling_x == cm->subsampling_y
                                   ? aom_rb_read_literal(rb, CDEF_STRENGTH_BITS)
                                   : 0;
  }
}
#endif  // CONFIG_CDEF

static INLINE int read_delta_q(struct aom_read_bit_buffer *rb) {
  return aom_rb_read_bit(rb) ? aom_rb_read_inv_signed_literal(rb, 6) : 0;
}

static void setup_quantization(AV1_COMMON *const cm,
                               struct aom_read_bit_buffer *rb) {
  cm->base_qindex = aom_rb_read_literal(rb, QINDEX_BITS);
  cm->y_dc_delta_q = read_delta_q(rb);
  cm->uv_dc_delta_q = read_delta_q(rb);
  cm->uv_ac_delta_q = read_delta_q(rb);
  cm->dequant_bit_depth = cm->bit_depth;
#if CONFIG_AOM_QM
  cm->using_qmatrix = aom_rb_read_bit(rb);
  if (cm->using_qmatrix) {
    cm->min_qmlevel = aom_rb_read_literal(rb, QM_LEVEL_BITS);
    cm->max_qmlevel = aom_rb_read_literal(rb, QM_LEVEL_BITS);
  } else {
    cm->min_qmlevel = 0;
    cm->max_qmlevel = 0;
  }
#endif
}

// Build y/uv dequant values based on segmentation.
static void setup_segmentation_dequant(AV1_COMMON *const cm) {
#if CONFIG_AOM_QM
  const int using_qm = cm->using_qmatrix;
  const int minqm = cm->min_qmlevel;
  const int maxqm = cm->max_qmlevel;
#endif
  // When segmentation is disabled, only the first value is used.  The
  // remaining are don't cares.
  const int max_segments = cm->seg.enabled ? MAX_SEGMENTS : 1;
  for (int i = 0; i < max_segments; ++i) {
    const int qindex = av1_get_qindex(&cm->seg, i, cm->base_qindex);
    cm->y_dequant[i][0] = av1_dc_quant(qindex, cm->y_dc_delta_q, cm->bit_depth);
    cm->y_dequant[i][1] = av1_ac_quant(qindex, 0, cm->bit_depth);
    cm->uv_dequant[i][0] =
        av1_dc_quant(qindex, cm->uv_dc_delta_q, cm->bit_depth);
    cm->uv_dequant[i][1] =
        av1_ac_quant(qindex, cm->uv_ac_delta_q, cm->bit_depth);
#if CONFIG_AOM_QM
    const int lossless = qindex == 0 && cm->y_dc_delta_q == 0 &&
                         cm->uv_dc_delta_q == 0 && cm->uv_ac_delta_q == 0;
    // NB: depends on base index so there is only 1 set per frame
    // No quant weighting when lossless or signalled not using QM
    const int qmlevel = (lossless || using_qm == 0)
                            ? NUM_QM_LEVELS - 1
                            : aom_get_qmlevel(cm->base_qindex, minqm, maxqm);
    for (int j = 0; j < TX_SIZES_ALL; ++j) {
      cm->y_iqmatrix[i][1][j] = aom_iqmatrix(cm, qmlevel, 0, j, 1);
      cm->y_iqmatrix[i][0][j] = aom_iqmatrix(cm, qmlevel, 0, j, 0);
      cm->uv_iqmatrix[i][1][j] = aom_iqmatrix(cm, qmlevel, 1, j, 1);
      cm->uv_iqmatrix[i][0][j] = aom_iqmatrix(cm, qmlevel, 1, j, 0);
    }
#endif  // CONFIG_AOM_QM
#if CONFIG_NEW_QUANT
    for (int dq = 0; dq < QUANT_PROFILES; dq++) {
      for (int b = 0; b < COEF_BANDS; ++b) {
        av1_get_dequant_val_nuq(cm->y_dequant[i][b != 0], b,
                                cm->y_dequant_nuq[i][dq][b], NULL, dq);
        av1_get_dequant_val_nuq(cm->uv_dequant[i][b != 0], b,
                                cm->uv_dequant_nuq[i][dq][b], NULL, dq);
      }
    }
#endif  //  CONFIG_NEW_QUANT
  }
}

static InterpFilter read_frame_interp_filter(struct aom_read_bit_buffer *rb) {
  return aom_rb_read_bit(rb) ? SWITCHABLE
                             : aom_rb_read_literal(rb, LOG_SWITCHABLE_FILTERS);
}

static void setup_render_size(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
#if CONFIG_FRAME_SUPERRES
  cm->render_width = cm->superres_upscaled_width;
  cm->render_height = cm->superres_upscaled_height;
#else
  cm->render_width = cm->width;
  cm->render_height = cm->height;
#endif  // CONFIG_FRAME_SUPERRES
  if (aom_rb_read_bit(rb))
    av1_read_frame_size(rb, &cm->render_width, &cm->render_height);
}

#if CONFIG_FRAME_SUPERRES
// TODO(afergs): make "struct aom_read_bit_buffer *const rb"?
static void setup_superres(AV1_COMMON *const cm, struct aom_read_bit_buffer *rb,
                           int *width, int *height) {
  cm->superres_upscaled_width = *width;
  cm->superres_upscaled_height = *height;
  if (aom_rb_read_bit(rb)) {
    cm->superres_scale_denominator =
        (uint8_t)aom_rb_read_literal(rb, SUPERRES_SCALE_BITS);
    cm->superres_scale_denominator += SUPERRES_SCALE_DENOMINATOR_MIN;
    // Don't edit cm->width or cm->height directly, or the buffers won't get
    // resized correctly
    av1_calculate_scaled_superres_size(width, height,
                                       cm->superres_scale_denominator);
  } else {
    // 1:1 scaling - ie. no scaling, scale not provided
    cm->superres_scale_denominator = SCALE_NUMERATOR;
  }
}
#endif  // CONFIG_FRAME_SUPERRES

static void resize_context_buffers(AV1_COMMON *cm, int width, int height) {
#if CONFIG_SIZE_LIMIT
  if (width > DECODE_WIDTH_LIMIT || height > DECODE_HEIGHT_LIMIT)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Dimensions of %dx%d beyond allowed size of %dx%d.",
                       width, height, DECODE_WIDTH_LIMIT, DECODE_HEIGHT_LIMIT);
#endif
  if (cm->width != width || cm->height != height) {
    const int new_mi_rows =
        ALIGN_POWER_OF_TWO(height, MI_SIZE_LOG2) >> MI_SIZE_LOG2;
    const int new_mi_cols =
        ALIGN_POWER_OF_TWO(width, MI_SIZE_LOG2) >> MI_SIZE_LOG2;

    // Allocations in av1_alloc_context_buffers() depend on individual
    // dimensions as well as the overall size.
    if (new_mi_cols > cm->mi_cols || new_mi_rows > cm->mi_rows) {
      if (av1_alloc_context_buffers(cm, width, height))
        aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to allocate context buffers");
    } else {
      av1_set_mb_mi(cm, width, height);
    }
    av1_init_context_buffers(cm);
    cm->width = width;
    cm->height = height;
  }

  ensure_mv_buffer(cm->cur_frame, cm);
  cm->cur_frame->width = cm->width;
  cm->cur_frame->height = cm->height;
}

static void setup_frame_size(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  int width, height;
  BufferPool *const pool = cm->buffer_pool;
  av1_read_frame_size(rb, &width, &height);
#if CONFIG_FRAME_SUPERRES
  setup_superres(cm, rb, &width, &height);
#endif  // CONFIG_FRAME_SUPERRES
  setup_render_size(cm, rb);
  resize_context_buffers(cm, width, height);

  lock_buffer_pool(pool);
  if (aom_realloc_frame_buffer(
          get_frame_new_buffer(cm), cm->width, cm->height, cm->subsampling_x,
          cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
          cm->use_highbitdepth,
#endif
          AOM_BORDER_IN_PIXELS, cm->byte_alignment,
          &pool->frame_bufs[cm->new_fb_idx].raw_frame_buffer, pool->get_fb_cb,
          pool->cb_priv)) {
    unlock_buffer_pool(pool);
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");
  }
  unlock_buffer_pool(pool);

  pool->frame_bufs[cm->new_fb_idx].buf.subsampling_x = cm->subsampling_x;
  pool->frame_bufs[cm->new_fb_idx].buf.subsampling_y = cm->subsampling_y;
  pool->frame_bufs[cm->new_fb_idx].buf.bit_depth = (unsigned int)cm->bit_depth;
  pool->frame_bufs[cm->new_fb_idx].buf.color_space = cm->color_space;
#if CONFIG_COLORSPACE_HEADERS
  pool->frame_bufs[cm->new_fb_idx].buf.transfer_function =
      cm->transfer_function;
  pool->frame_bufs[cm->new_fb_idx].buf.chroma_sample_position =
      cm->chroma_sample_position;
#endif
  pool->frame_bufs[cm->new_fb_idx].buf.color_range = cm->color_range;
  pool->frame_bufs[cm->new_fb_idx].buf.render_width = cm->render_width;
  pool->frame_bufs[cm->new_fb_idx].buf.render_height = cm->render_height;
}

static void setup_sb_size(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  (void)rb;
#if CONFIG_EXT_PARTITION
  set_sb_size(cm, aom_rb_read_bit(rb) ? BLOCK_128X128 : BLOCK_64X64);
#else
  set_sb_size(cm, BLOCK_64X64);
#endif  // CONFIG_EXT_PARTITION
}

static INLINE int valid_ref_frame_img_fmt(aom_bit_depth_t ref_bit_depth,
                                          int ref_xss, int ref_yss,
                                          aom_bit_depth_t this_bit_depth,
                                          int this_xss, int this_yss) {
  return ref_bit_depth == this_bit_depth && ref_xss == this_xss &&
         ref_yss == this_yss;
}

static void setup_frame_size_with_refs(AV1_COMMON *cm,
                                       struct aom_read_bit_buffer *rb) {
  int width, height;
  int found = 0, i;
  int has_valid_ref_frame = 0;
  BufferPool *const pool = cm->buffer_pool;
  for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    if (aom_rb_read_bit(rb)) {
      YV12_BUFFER_CONFIG *const buf = cm->frame_refs[i].buf;
      width = buf->y_crop_width;
      height = buf->y_crop_height;
      cm->render_width = buf->render_width;
      cm->render_height = buf->render_height;
#if CONFIG_FRAME_SUPERRES
      setup_superres(cm, rb, &width, &height);
#endif  // CONFIG_FRAME_SUPERRES
      found = 1;
      break;
    }
  }

  if (!found) {
    av1_read_frame_size(rb, &width, &height);
#if CONFIG_FRAME_SUPERRES
    setup_superres(cm, rb, &width, &height);
#endif  // CONFIG_FRAME_SUPERRES
    setup_render_size(cm, rb);
  }

  if (width <= 0 || height <= 0)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Invalid frame size");

  // Check to make sure at least one of frames that this frame references
  // has valid dimensions.
  for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    RefBuffer *const ref_frame = &cm->frame_refs[i];
    has_valid_ref_frame |=
        valid_ref_frame_size(ref_frame->buf->y_crop_width,
                             ref_frame->buf->y_crop_height, width, height);
  }
  if (!has_valid_ref_frame)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Referenced frame has invalid size");
  for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    RefBuffer *const ref_frame = &cm->frame_refs[i];
    if (!valid_ref_frame_img_fmt(ref_frame->buf->bit_depth,
                                 ref_frame->buf->subsampling_x,
                                 ref_frame->buf->subsampling_y, cm->bit_depth,
                                 cm->subsampling_x, cm->subsampling_y))
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Referenced frame has incompatible color format");
  }

  resize_context_buffers(cm, width, height);

  lock_buffer_pool(pool);
  if (aom_realloc_frame_buffer(
          get_frame_new_buffer(cm), cm->width, cm->height, cm->subsampling_x,
          cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
          cm->use_highbitdepth,
#endif
          AOM_BORDER_IN_PIXELS, cm->byte_alignment,
          &pool->frame_bufs[cm->new_fb_idx].raw_frame_buffer, pool->get_fb_cb,
          pool->cb_priv)) {
    unlock_buffer_pool(pool);
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");
  }
  unlock_buffer_pool(pool);

  pool->frame_bufs[cm->new_fb_idx].buf.subsampling_x = cm->subsampling_x;
  pool->frame_bufs[cm->new_fb_idx].buf.subsampling_y = cm->subsampling_y;
  pool->frame_bufs[cm->new_fb_idx].buf.bit_depth = (unsigned int)cm->bit_depth;
  pool->frame_bufs[cm->new_fb_idx].buf.color_space = cm->color_space;
#if CONFIG_COLORSPACE_HEADERS
  pool->frame_bufs[cm->new_fb_idx].buf.transfer_function =
      cm->transfer_function;
  pool->frame_bufs[cm->new_fb_idx].buf.chroma_sample_position =
      cm->chroma_sample_position;
#endif
  pool->frame_bufs[cm->new_fb_idx].buf.color_range = cm->color_range;
  pool->frame_bufs[cm->new_fb_idx].buf.render_width = cm->render_width;
  pool->frame_bufs[cm->new_fb_idx].buf.render_height = cm->render_height;
}

static void read_tile_group_range(AV1Decoder *pbi,
                                  struct aom_read_bit_buffer *const rb) {
  AV1_COMMON *const cm = &pbi->common;
  const int num_bits = cm->log2_tile_rows + cm->log2_tile_cols;
  const int num_tiles =
      cm->tile_rows * cm->tile_cols;  // Note: May be < (1<<num_bits)
  pbi->tg_start = aom_rb_read_literal(rb, num_bits);
  pbi->tg_size = 1 + aom_rb_read_literal(rb, num_bits);
  if (pbi->tg_start + pbi->tg_size > num_tiles)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Tile group extends past last tile in frame");
}

#if CONFIG_MAX_TILE

// Same function as av1_read_uniform but reading from uncompresses header wb
static int rb_read_uniform(struct aom_read_bit_buffer *const rb, int n) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  const int v = aom_rb_read_literal(rb, l - 1);
  assert(l != 0);
  if (v < m)
    return v;
  else
    return (v << 1) - m + aom_rb_read_literal(rb, 1);
}

static void read_tile_info_max_tile(AV1_COMMON *const cm,
                                    struct aom_read_bit_buffer *const rb) {
  int width_mi = ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);
  int height_mi = ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2);
  int width_sb = width_mi >> MAX_MIB_SIZE_LOG2;
  int height_sb = height_mi >> MAX_MIB_SIZE_LOG2;
  int start_sb, size_sb, i;

  av1_get_tile_limits(cm);
  cm->uniform_tile_spacing_flag = aom_rb_read_bit(rb);

  // Read tile columns
  if (cm->uniform_tile_spacing_flag) {
    cm->log2_tile_cols = cm->min_log2_tile_cols;
    while (cm->log2_tile_cols < cm->max_log2_tile_cols) {
      if (!aom_rb_read_bit(rb)) {
        break;
      }
      cm->log2_tile_cols++;
    }
  } else {
    for (i = 0, start_sb = 0; width_sb > 0 && i < MAX_TILE_COLS; i++) {
      size_sb = 1 + rb_read_uniform(rb, AOMMIN(width_sb, MAX_TILE_WIDTH_SB));
      cm->tile_col_start_sb[i] = start_sb;
      start_sb += size_sb;
      width_sb -= size_sb;
    }
    cm->tile_cols = i;
    cm->tile_col_start_sb[i] = start_sb + width_sb;
  }
  av1_calculate_tile_cols(cm);

  // Read tile rows
  if (cm->uniform_tile_spacing_flag) {
    cm->log2_tile_rows = cm->min_log2_tile_rows;
    while (cm->log2_tile_rows < cm->max_log2_tile_rows) {
      if (!aom_rb_read_bit(rb)) {
        break;
      }
      cm->log2_tile_rows++;
    }
  } else {
    for (i = 0, start_sb = 0; height_sb > 0 && i < MAX_TILE_ROWS; i++) {
      size_sb =
          1 + rb_read_uniform(rb, AOMMIN(height_sb, cm->max_tile_height_sb));
      cm->tile_row_start_sb[i] = start_sb;
      start_sb += size_sb;
      height_sb -= size_sb;
    }
    cm->tile_rows = i;
    cm->tile_row_start_sb[i] = start_sb + height_sb;
  }
  av1_calculate_tile_rows(cm);
}
#endif

static void read_tile_info(AV1Decoder *const pbi,
                           struct aom_read_bit_buffer *const rb) {
  AV1_COMMON *const cm = &pbi->common;
#if CONFIG_EXT_TILE
  cm->single_tile_decoding = 0;
  if (cm->large_scale_tile) {
    struct loopfilter *lf = &cm->lf;

    // Figure out single_tile_decoding by loopfilter_level.
    cm->single_tile_decoding = (!lf->filter_level) ? 1 : 0;
// Read the tile width/height
#if CONFIG_EXT_PARTITION
    if (cm->sb_size == BLOCK_128X128) {
      cm->tile_width = aom_rb_read_literal(rb, 5) + 1;
      cm->tile_height = aom_rb_read_literal(rb, 5) + 1;
    } else {
#endif  // CONFIG_EXT_PARTITION
      cm->tile_width = aom_rb_read_literal(rb, 6) + 1;
      cm->tile_height = aom_rb_read_literal(rb, 6) + 1;
#if CONFIG_EXT_PARTITION
    }
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_LOOPFILTERING_ACROSS_TILES
    cm->loop_filter_across_tiles_enabled = aom_rb_read_bit(rb);
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

    cm->tile_width <<= cm->mib_size_log2;
    cm->tile_height <<= cm->mib_size_log2;

    cm->tile_width = AOMMIN(cm->tile_width, cm->mi_cols);
    cm->tile_height = AOMMIN(cm->tile_height, cm->mi_rows);

    // Get the number of tiles
    cm->tile_cols = 1;
    while (cm->tile_cols * cm->tile_width < cm->mi_cols) ++cm->tile_cols;

    cm->tile_rows = 1;
    while (cm->tile_rows * cm->tile_height < cm->mi_rows) ++cm->tile_rows;

    if (cm->tile_cols * cm->tile_rows > 1) {
      // Read the number of bytes used to store tile size
      pbi->tile_col_size_bytes = aom_rb_read_literal(rb, 2) + 1;
      pbi->tile_size_bytes = aom_rb_read_literal(rb, 2) + 1;
    }

#if CONFIG_DEPENDENT_HORZTILES
    cm->dependent_horz_tiles = 0;
#endif
  } else {
#endif  // CONFIG_EXT_TILE

#if CONFIG_MAX_TILE
    read_tile_info_max_tile(cm, rb);
#else
  int min_log2_tile_cols, max_log2_tile_cols, max_ones;
  av1_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  // columns
  max_ones = max_log2_tile_cols - min_log2_tile_cols;
  cm->log2_tile_cols = min_log2_tile_cols;
  while (max_ones-- && aom_rb_read_bit(rb)) cm->log2_tile_cols++;

  if (cm->log2_tile_cols > 6)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Invalid number of tile columns");

  // rows
  cm->log2_tile_rows = aom_rb_read_bit(rb);
  if (cm->log2_tile_rows) cm->log2_tile_rows += aom_rb_read_bit(rb);

  cm->tile_width =
      get_tile_size(cm->mi_cols, cm->log2_tile_cols, &cm->tile_cols);
  cm->tile_height =
      get_tile_size(cm->mi_rows, cm->log2_tile_rows, &cm->tile_rows);

#endif  // CONFIG_MAX_TILE
#if CONFIG_DEPENDENT_HORZTILES
    if (cm->tile_rows > 1)
      cm->dependent_horz_tiles = aom_rb_read_bit(rb);
    else
      cm->dependent_horz_tiles = 0;
#endif
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    cm->loop_filter_across_tiles_enabled = aom_rb_read_bit(rb);
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

    // tile size magnitude
    pbi->tile_size_bytes = aom_rb_read_literal(rb, 2) + 1;
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE

// each tile group header is in its own tile group OBU
#if !CONFIG_OBU
  // Store an index to the location of the tile group information
  pbi->tg_size_bit_offset = rb->bit_offset;
  read_tile_group_range(pbi, rb);
#endif
}

static int mem_get_varsize(const uint8_t *src, int sz) {
  switch (sz) {
    case 1: return src[0];
    case 2: return mem_get_le16(src);
    case 3: return mem_get_le24(src);
    case 4: return mem_get_le32(src);
    default: assert(0 && "Invalid size"); return -1;
  }
}

#if CONFIG_EXT_TILE
// Reads the next tile returning its size and adjusting '*data' accordingly
// based on 'is_last'.
static void get_ls_tile_buffer(
    const uint8_t *const data_end, struct aom_internal_error_info *error_info,
    const uint8_t **data, aom_decrypt_cb decrypt_cb, void *decrypt_state,
    TileBufferDec (*const tile_buffers)[MAX_TILE_COLS], int tile_size_bytes,
    int col, int row, int tile_copy_mode) {
  size_t size;

  size_t copy_size = 0;
  const uint8_t *copy_data = NULL;

  if (!read_is_valid(*data, tile_size_bytes, data_end))
    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile length");
  if (decrypt_cb) {
    uint8_t be_data[4];
    decrypt_cb(decrypt_state, *data, be_data, tile_size_bytes);

    // Only read number of bytes in cm->tile_size_bytes.
    size = mem_get_varsize(be_data, tile_size_bytes);
  } else {
    size = mem_get_varsize(*data, tile_size_bytes);
  }

  // If tile_copy_mode = 1, then the top bit of the tile header indicates copy
  // mode.
  if (tile_copy_mode && (size >> (tile_size_bytes * 8 - 1)) == 1) {
    // The remaining bits in the top byte signal the row offset
    int offset = (size >> (tile_size_bytes - 1) * 8) & 0x7f;

    // Currently, only use tiles in same column as reference tiles.
    copy_data = tile_buffers[row - offset][col].data;
    copy_size = tile_buffers[row - offset][col].size;
    size = 0;
  }

  *data += tile_size_bytes;

  if (size > (size_t)(data_end - *data))
    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile size");

  if (size > 0) {
    tile_buffers[row][col].data = *data;
    tile_buffers[row][col].size = size;
  } else {
    tile_buffers[row][col].data = copy_data;
    tile_buffers[row][col].size = copy_size;
  }

  *data += size;

  tile_buffers[row][col].raw_data_end = *data;
}

static void get_ls_tile_buffers(
    AV1Decoder *pbi, const uint8_t *data, const uint8_t *data_end,
    TileBufferDec (*const tile_buffers)[MAX_TILE_COLS]) {
  AV1_COMMON *const cm = &pbi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  const int have_tiles = tile_cols * tile_rows > 1;

  if (!have_tiles) {
    const size_t tile_size = data_end - data;
    tile_buffers[0][0].data = data;
    tile_buffers[0][0].size = tile_size;
    tile_buffers[0][0].raw_data_end = NULL;
  } else {
    // We locate only the tile buffers that are required, which are the ones
    // specified by pbi->dec_tile_col and pbi->dec_tile_row. Also, we always
    // need the last (bottom right) tile buffer, as we need to know where the
    // end of the compressed frame buffer is for proper superframe decoding.

    const uint8_t *tile_col_data_end[MAX_TILE_COLS];
    const uint8_t *const data_start = data;

    const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
    const int single_row = pbi->dec_tile_row >= 0;
    const int tile_rows_start = single_row ? dec_tile_row : 0;
    const int tile_rows_end = single_row ? tile_rows_start + 1 : tile_rows;
    const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
    const int single_col = pbi->dec_tile_col >= 0;
    const int tile_cols_start = single_col ? dec_tile_col : 0;
    const int tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;

    const int tile_col_size_bytes = pbi->tile_col_size_bytes;
    const int tile_size_bytes = pbi->tile_size_bytes;
    const int tile_copy_mode =
        ((AOMMAX(cm->tile_width, cm->tile_height) << MI_SIZE_LOG2) <= 256) ? 1
                                                                           : 0;
    size_t tile_col_size;
    int r, c;

    // Read tile column sizes for all columns (we need the last tile buffer)
    for (c = 0; c < tile_cols; ++c) {
      const int is_last = c == tile_cols - 1;
      if (!is_last) {
        tile_col_size = mem_get_varsize(data, tile_col_size_bytes);
        data += tile_col_size_bytes;
        tile_col_data_end[c] = data + tile_col_size;
      } else {
        tile_col_size = data_end - data;
        tile_col_data_end[c] = data_end;
      }
      data += tile_col_size;
    }

    data = data_start;

    // Read the required tile sizes.
    for (c = tile_cols_start; c < tile_cols_end; ++c) {
      const int is_last = c == tile_cols - 1;

      if (c > 0) data = tile_col_data_end[c - 1];

      if (!is_last) data += tile_col_size_bytes;

      // Get the whole of the last column, otherwise stop at the required tile.
      for (r = 0; r < (is_last ? tile_rows : tile_rows_end); ++r) {
        tile_buffers[r][c].col = c;

        get_ls_tile_buffer(tile_col_data_end[c], &pbi->common.error, &data,
                           pbi->decrypt_cb, pbi->decrypt_state, tile_buffers,
                           tile_size_bytes, c, r, tile_copy_mode);
      }
    }

    // If we have not read the last column, then read it to get the last tile.
    if (tile_cols_end != tile_cols) {
      c = tile_cols - 1;

      data = tile_col_data_end[c - 1];

      for (r = 0; r < tile_rows; ++r) {
        tile_buffers[r][c].col = c;

        get_ls_tile_buffer(tile_col_data_end[c], &pbi->common.error, &data,
                           pbi->decrypt_cb, pbi->decrypt_state, tile_buffers,
                           tile_size_bytes, c, r, tile_copy_mode);
      }
    }
  }
}
#endif  // CONFIG_EXT_TILE

// Reads the next tile returning its size and adjusting '*data' accordingly
// based on 'is_last'.
static void get_tile_buffer(const uint8_t *const data_end,
                            const int tile_size_bytes, int is_last,
                            struct aom_internal_error_info *error_info,
                            const uint8_t **data, aom_decrypt_cb decrypt_cb,
                            void *decrypt_state, TileBufferDec *const buf) {
  size_t size;

  if (!is_last) {
    if (!read_is_valid(*data, tile_size_bytes, data_end))
      aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Truncated packet or corrupt tile length");

    if (decrypt_cb) {
      uint8_t be_data[4];
      decrypt_cb(decrypt_state, *data, be_data, tile_size_bytes);
      size = mem_get_varsize(be_data, tile_size_bytes);
    } else {
      size = mem_get_varsize(*data, tile_size_bytes);
    }
    *data += tile_size_bytes;

    if (size > (size_t)(data_end - *data))
      aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Truncated packet or corrupt tile size");
  } else {
    size = data_end - *data;
  }

  buf->data = *data;
  buf->size = size;

  *data += size;
}

static void get_tile_buffers(AV1Decoder *pbi, const uint8_t *data,
                             const uint8_t *data_end,
                             TileBufferDec (*const tile_buffers)[MAX_TILE_COLS],
                             int startTile, int endTile) {
  AV1_COMMON *const cm = &pbi->common;
  int r, c;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int tc = 0;
  int first_tile_in_tg = 0;
  struct aom_read_bit_buffer rb_tg_hdr;
  uint8_t clear_data[MAX_AV1_HEADER_SIZE];
#if !CONFIG_OBU
  const size_t hdr_size = pbi->uncomp_hdr_size + pbi->first_partition_size;
  const int tg_size_bit_offset = pbi->tg_size_bit_offset;
#else
  const int tg_size_bit_offset = 0;
#endif

#if CONFIG_DEPENDENT_HORZTILES
  int tile_group_start_col = 0;
  int tile_group_start_row = 0;
#endif

  for (r = 0; r < tile_rows; ++r) {
    for (c = 0; c < tile_cols; ++c, ++tc) {
      TileBufferDec *const buf = &tile_buffers[r][c];
#if CONFIG_OBU
      const int is_last = (tc == endTile);
      const size_t hdr_offset = 0;
#else
      const int is_last = (r == tile_rows - 1) && (c == tile_cols - 1);
      const size_t hdr_offset = (tc && tc == first_tile_in_tg) ? hdr_size : 0;
#endif

      if (tc < startTile || tc > endTile) continue;

      if (data + hdr_offset >= data_end)
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Data ended before all tiles were read.");
      buf->col = c;
      if (hdr_offset) {
        init_read_bit_buffer(pbi, &rb_tg_hdr, data, data_end, clear_data);
        rb_tg_hdr.bit_offset = tg_size_bit_offset;
        read_tile_group_range(pbi, &rb_tg_hdr);
#if CONFIG_DEPENDENT_HORZTILES
        tile_group_start_row = r;
        tile_group_start_col = c;
#endif
      }
      first_tile_in_tg += tc == first_tile_in_tg ? pbi->tg_size : 0;
      data += hdr_offset;
      get_tile_buffer(data_end, pbi->tile_size_bytes, is_last,
                      &pbi->common.error, &data, pbi->decrypt_cb,
                      pbi->decrypt_state, buf);
#if CONFIG_DEPENDENT_HORZTILES
      cm->tile_group_start_row[r][c] = tile_group_start_row;
      cm->tile_group_start_col[r][c] = tile_group_start_col;
#endif
    }
  }
}

#if CONFIG_PVQ
static void daala_dec_init(AV1_COMMON *const cm, daala_dec_ctx *daala_dec,
                           aom_reader *r) {
  daala_dec->r = r;

  // TODO(yushin) : activity masking info needs be signaled by a bitstream
  daala_dec->use_activity_masking = AV1_PVQ_ENABLE_ACTIVITY_MASKING;

  if (daala_dec->use_activity_masking)
    daala_dec->qm = OD_HVS_QM;
  else
    daala_dec->qm = OD_FLAT_QM;

  od_init_qm(daala_dec->state.qm, daala_dec->state.qm_inv,
             daala_dec->qm == OD_HVS_QM ? OD_QM8_Q4_HVS : OD_QM8_Q4_FLAT);

  if (daala_dec->use_activity_masking) {
    int pli;
    int use_masking = daala_dec->use_activity_masking;
    int segment_id = 0;
    int qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);

    for (pli = 0; pli < MAX_MB_PLANE; pli++) {
      int i;
      int q;

      q = qindex;
      if (q <= OD_DEFAULT_QMS[use_masking][0][pli].interp_q << OD_COEFF_SHIFT) {
        od_interp_qm(&daala_dec->state.pvq_qm_q4[pli][0], q,
                     &OD_DEFAULT_QMS[use_masking][0][pli], NULL);
      } else {
        i = 0;
        while (OD_DEFAULT_QMS[use_masking][i + 1][pli].qm_q4 != NULL &&
               q > OD_DEFAULT_QMS[use_masking][i + 1][pli].interp_q
                       << OD_COEFF_SHIFT) {
          i++;
        }
        od_interp_qm(&daala_dec->state.pvq_qm_q4[pli][0], q,
                     &OD_DEFAULT_QMS[use_masking][i][pli],
                     &OD_DEFAULT_QMS[use_masking][i + 1][pli]);
      }
    }
  }
}
#endif  // #if CONFIG_PVQ

#if CONFIG_LOOPFILTERING_ACROSS_TILES
static void dec_setup_across_tile_boundary_info(
    const AV1_COMMON *const cm, const TileInfo *const tile_info) {
  if (tile_info->mi_row_start >= tile_info->mi_row_end ||
      tile_info->mi_col_start >= tile_info->mi_col_end)
    return;

  if (!cm->loop_filter_across_tiles_enabled) {
    av1_setup_across_tile_boundary_info(cm, tile_info);
  }
}
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

static const uint8_t *decode_tiles(AV1Decoder *pbi, const uint8_t *data,
                                   const uint8_t *data_end, int startTile,
                                   int endTile) {
  AV1_COMMON *const cm = &pbi->common;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  const int n_tiles = tile_cols * tile_rows;
  TileBufferDec(*const tile_buffers)[MAX_TILE_COLS] = pbi->tile_buffers;
#if CONFIG_EXT_TILE
  const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
  const int single_row = pbi->dec_tile_row >= 0;
  const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
  const int single_col = pbi->dec_tile_col >= 0;
#endif  // CONFIG_EXT_TILE
  int tile_rows_start;
  int tile_rows_end;
  int tile_cols_start;
  int tile_cols_end;
  int inv_col_order;
  int inv_row_order;
  int tile_row, tile_col;

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    tile_rows_start = single_row ? dec_tile_row : 0;
    tile_rows_end = single_row ? dec_tile_row + 1 : tile_rows;
    tile_cols_start = single_col ? dec_tile_col : 0;
    tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;
    inv_col_order = pbi->inv_tile_order && !single_col;
    inv_row_order = pbi->inv_tile_order && !single_row;
  } else {
#endif  // CONFIG_EXT_TILE
    tile_rows_start = 0;
    tile_rows_end = tile_rows;
    tile_cols_start = 0;
    tile_cols_end = tile_cols;
    inv_col_order = pbi->inv_tile_order;
    inv_row_order = pbi->inv_tile_order;
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE

  if (cm->lf.filter_level && !cm->skip_loop_filter &&
      pbi->lf_worker.data1 == NULL) {
    CHECK_MEM_ERROR(cm, pbi->lf_worker.data1,
                    aom_memalign(32, sizeof(LFWorkerData)));
    pbi->lf_worker.hook = (AVxWorkerHook)av1_loop_filter_worker;
    if (pbi->max_threads > 1 && !winterface->reset(&pbi->lf_worker)) {
      aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                         "Loop filter thread creation failed");
    }
  }

  if (cm->lf.filter_level && !cm->skip_loop_filter) {
    LFWorkerData *const lf_data = (LFWorkerData *)pbi->lf_worker.data1;
    // Be sure to sync as we might be resuming after a failed frame decode.
    winterface->sync(&pbi->lf_worker);
    av1_loop_filter_data_reset(lf_data, get_frame_new_buffer(cm), cm,
                               pbi->mb.plane);
  }

  assert(tile_rows <= MAX_TILE_ROWS);
  assert(tile_cols <= MAX_TILE_COLS);

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile)
    get_ls_tile_buffers(pbi, data, data_end, tile_buffers);
  else
#endif  // CONFIG_EXT_TILE
    get_tile_buffers(pbi, data, data_end, tile_buffers, startTile, endTile);

  if (pbi->tile_data == NULL || n_tiles != pbi->allocated_tiles) {
    aom_free(pbi->tile_data);
    CHECK_MEM_ERROR(cm, pbi->tile_data,
                    aom_memalign(32, n_tiles * (sizeof(*pbi->tile_data))));
    pbi->allocated_tiles = n_tiles;
  }
#if CONFIG_ACCOUNTING
  if (pbi->acct_enabled) {
    aom_accounting_reset(&pbi->accounting);
  }
#endif
  // Load all tile information into tile_data.
  for (tile_row = tile_rows_start; tile_row < tile_rows_end; ++tile_row) {
    for (tile_col = tile_cols_start; tile_col < tile_cols_end; ++tile_col) {
      const TileBufferDec *const buf = &tile_buffers[tile_row][tile_col];
      TileData *const td = pbi->tile_data + tile_cols * tile_row + tile_col;

      if (tile_row * cm->tile_cols + tile_col < startTile ||
          tile_row * cm->tile_cols + tile_col > endTile)
        continue;

      td->cm = cm;
      td->xd = pbi->mb;
      td->xd.corrupted = 0;
      td->xd.counts =
          cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD
              ? &cm->counts
              : NULL;
      av1_zero(td->dqcoeff);
#if CONFIG_PVQ
      av1_zero(td->pvq_ref_coeff);
#endif
      av1_tile_init(&td->xd.tile, td->cm, tile_row, tile_col);
      setup_bool_decoder(buf->data, data_end, buf->size, &cm->error,
                         &td->bit_reader,
#if CONFIG_ANS && ANS_MAX_SYMBOLS
                         1 << cm->ans_window_size_log2,
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
                         pbi->decrypt_cb, pbi->decrypt_state);
#if CONFIG_ACCOUNTING
      if (pbi->acct_enabled) {
        td->bit_reader.accounting = &pbi->accounting;
      } else {
        td->bit_reader.accounting = NULL;
      }
#endif
      av1_init_macroblockd(cm, &td->xd,
#if CONFIG_PVQ
                           td->pvq_ref_coeff,
#endif
#if CONFIG_CFL
                           &td->cfl,
#endif
                           td->dqcoeff);

      // Initialise the tile context from the frame context
      td->tctx = *cm->fc;
      td->xd.tile_ctx = &td->tctx;

#if CONFIG_PVQ
      daala_dec_init(cm, &td->xd.daala_dec, &td->bit_reader);
      td->xd.daala_dec.state.adapt = &td->tctx.pvq_context;
#endif

      td->xd.plane[0].color_index_map = td->color_index_map[0];
      td->xd.plane[1].color_index_map = td->color_index_map[1];
#if CONFIG_MRC_TX
      td->xd.mrc_mask = td->mrc_mask;
#endif  // CONFIG_MRC_TX
    }
  }

  for (tile_row = tile_rows_start; tile_row < tile_rows_end; ++tile_row) {
    const int row = inv_row_order ? tile_rows - 1 - tile_row : tile_row;
    int mi_row = 0;
    TileInfo tile_info;

    av1_tile_set_row(&tile_info, cm, row);

    for (tile_col = tile_cols_start; tile_col < tile_cols_end; ++tile_col) {
      const int col = inv_col_order ? tile_cols - 1 - tile_col : tile_col;
      TileData *const td = pbi->tile_data + tile_cols * row + col;

      if (tile_row * cm->tile_cols + tile_col < startTile ||
          tile_row * cm->tile_cols + tile_col > endTile)
        continue;

#if CONFIG_ACCOUNTING
      if (pbi->acct_enabled) {
        td->bit_reader.accounting->last_tell_frac =
            aom_reader_tell_frac(&td->bit_reader);
      }
#endif

      av1_tile_set_col(&tile_info, cm, col);

#if CONFIG_DEPENDENT_HORZTILES
      av1_tile_set_tg_boundary(&tile_info, cm, tile_row, tile_col);
      if (!cm->dependent_horz_tiles || tile_row == 0 ||
          tile_info.tg_horz_boundary) {
        av1_zero_above_context(cm, tile_info.mi_col_start,
                               tile_info.mi_col_end);
      }
#else
      av1_zero_above_context(cm, tile_info.mi_col_start, tile_info.mi_col_end);
#endif
#if CONFIG_LOOP_RESTORATION
      for (int p = 0; p < MAX_MB_PLANE; ++p) {
        set_default_wiener(td->xd.wiener_info + p);
        set_default_sgrproj(td->xd.sgrproj_info + p);
      }
#endif  // CONFIG_LOOP_RESTORATION

#if CONFIG_LOOPFILTERING_ACROSS_TILES
      dec_setup_across_tile_boundary_info(cm, &tile_info);
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

      for (mi_row = tile_info.mi_row_start; mi_row < tile_info.mi_row_end;
           mi_row += cm->mib_size) {
        int mi_col;

        av1_zero_left_context(&td->xd);

        for (mi_col = tile_info.mi_col_start; mi_col < tile_info.mi_col_end;
             mi_col += cm->mib_size) {
#if CONFIG_NCOBMC_ADAPT_WEIGHT
          alloc_ncobmc_pred_buffer(&td->xd);
          set_sb_mi_boundaries(cm, &td->xd, mi_row, mi_col);
#endif
          decode_partition(pbi, &td->xd,
#if CONFIG_SUPERTX
                           0,
#endif  // CONFIG_SUPERTX
                           mi_row, mi_col, &td->bit_reader, cm->sb_size);
#if NC_MODE_INFO && CONFIG_MOTION_VAR
          detoken_and_recon_sb(pbi, &td->xd, mi_row, mi_col, &td->bit_reader,
                               cm->sb_size);
#endif
#if CONFIG_NCOBMC_ADAPT_WEIGHT
          free_ncobmc_pred_buffer(&td->xd);
#endif
        }
        aom_merge_corrupted_flag(&pbi->mb.corrupted, td->xd.corrupted);
        if (pbi->mb.corrupted)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Failed to decode tile data");
      }
    }

#if !CONFIG_OBU
    assert(mi_row > 0);
#endif

// when Parallel deblocking is enabled, deblocking should not
// be interleaved with decoding. Instead, deblocking should be done
// after the entire frame is decoded.
#if !CONFIG_VAR_TX && !CONFIG_PARALLEL_DEBLOCKING && !CONFIG_CB4X4
    // Loopfilter one tile row.
    // Note: If out-of-order tile decoding is used(for example, inv_row_order
    // = 1), the loopfiltering has be done after all tile rows are decoded.
    if (!inv_row_order && cm->lf.filter_level && !cm->skip_loop_filter) {
      LFWorkerData *const lf_data = (LFWorkerData *)pbi->lf_worker.data1;
      const int lf_start = AOMMAX(0, tile_info.mi_row_start - cm->mib_size);
      const int lf_end = tile_info.mi_row_end - cm->mib_size;

      // Delay the loopfilter if the first tile row is only
      // a single superblock high.
      if (lf_end <= 0) continue;

      // Decoding has completed. Finish up the loop filter in this thread.
      if (tile_info.mi_row_end >= cm->mi_rows) continue;

      winterface->sync(&pbi->lf_worker);
      lf_data->start = lf_start;
      lf_data->stop = lf_end;
      if (pbi->max_threads > 1) {
        winterface->launch(&pbi->lf_worker);
      } else {
        winterface->execute(&pbi->lf_worker);
      }
    }
#endif  // !CONFIG_VAR_TX && !CONFIG_PARALLEL_DEBLOCKING

    // After loopfiltering, the last 7 row pixels in each superblock row may
    // still be changed by the longest loopfilter of the next superblock row.
    if (cm->frame_parallel_decode)
      av1_frameworker_broadcast(pbi->cur_buf, mi_row << cm->mib_size_log2);
  }

#if CONFIG_VAR_TX || CONFIG_CB4X4
// Loopfilter the whole frame.
#if CONFIG_LPF_SB
  av1_loop_filter_frame(get_frame_new_buffer(cm), cm, &pbi->mb,
                        cm->lf.filter_level, 0, 0, 0, 0);
#else
#if CONFIG_LOOPFILTER_LEVEL
  if (cm->lf.filter_level[0] || cm->lf.filter_level[1]) {
    av1_loop_filter_frame(get_frame_new_buffer(cm), cm, &pbi->mb,
                          cm->lf.filter_level[0], cm->lf.filter_level[1], 0, 0);
    av1_loop_filter_frame(get_frame_new_buffer(cm), cm, &pbi->mb,
                          cm->lf.filter_level_u, cm->lf.filter_level_u, 1, 0);
    av1_loop_filter_frame(get_frame_new_buffer(cm), cm, &pbi->mb,
                          cm->lf.filter_level_v, cm->lf.filter_level_v, 2, 0);
  }
#else
#if CONFIG_OBU
  if (endTile == cm->tile_rows * cm->tile_cols - 1)
#endif
    av1_loop_filter_frame(get_frame_new_buffer(cm), cm, &pbi->mb,
                          cm->lf.filter_level, 0, 0);
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif  // CONFIG_LPF_SB
#else
#if CONFIG_PARALLEL_DEBLOCKING
  // Loopfilter all rows in the frame in the frame.
  if (cm->lf.filter_level && !cm->skip_loop_filter) {
    LFWorkerData *const lf_data = (LFWorkerData *)pbi->lf_worker.data1;
    winterface->sync(&pbi->lf_worker);
    lf_data->start = 0;
    lf_data->stop = cm->mi_rows;
    winterface->execute(&pbi->lf_worker);
  }
#else
  // Loopfilter remaining rows in the frame.
  if (cm->lf.filter_level && !cm->skip_loop_filter) {
    LFWorkerData *const lf_data = (LFWorkerData *)pbi->lf_worker.data1;
    winterface->sync(&pbi->lf_worker);
    lf_data->start = lf_data->stop;
    lf_data->stop = cm->mi_rows;
    winterface->execute(&pbi->lf_worker);
  }
#endif  // CONFIG_PARALLEL_DEBLOCKING
#endif  // CONFIG_VAR_TX
  if (cm->frame_parallel_decode)
    av1_frameworker_broadcast(pbi->cur_buf, INT_MAX);

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    if (n_tiles == 1) {
#if CONFIG_ANS
      return data_end;
#else
      // Find the end of the single tile buffer
      return aom_reader_find_end(&pbi->tile_data->bit_reader);
#endif  // CONFIG_ANS
    } else {
      // Return the end of the last tile buffer
      return tile_buffers[tile_rows - 1][tile_cols - 1].raw_data_end;
    }
  } else {
#endif  // CONFIG_EXT_TILE
#if CONFIG_ANS
    return data_end;
#else
#if !CONFIG_OBU
  {
    // Get last tile data.
    TileData *const td = pbi->tile_data + tile_cols * tile_rows - 1;
    return aom_reader_find_end(&td->bit_reader);
  }
#else
  TileData *const td = pbi->tile_data + endTile;
  return aom_reader_find_end(&td->bit_reader);
#endif
#endif  // CONFIG_ANS
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE
}

static int tile_worker_hook(TileWorkerData *const tile_data,
                            const TileInfo *const tile) {
  AV1Decoder *const pbi = tile_data->pbi;
  const AV1_COMMON *const cm = &pbi->common;
  int mi_row, mi_col;

  if (setjmp(tile_data->error_info.jmp)) {
    tile_data->error_info.setjmp = 0;
    aom_merge_corrupted_flag(&tile_data->xd.corrupted, 1);
    return 0;
  }

  tile_data->error_info.setjmp = 1;
  tile_data->xd.error_info = &tile_data->error_info;
#if CONFIG_DEPENDENT_HORZTILES
  if (!cm->dependent_horz_tiles || tile->tg_horz_boundary) {
    av1_zero_above_context(&pbi->common, tile->mi_col_start, tile->mi_col_end);
  }
#else
  av1_zero_above_context(&pbi->common, tile->mi_col_start, tile->mi_col_end);
#endif

  for (mi_row = tile->mi_row_start; mi_row < tile->mi_row_end;
       mi_row += cm->mib_size) {
    av1_zero_left_context(&tile_data->xd);

    for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
         mi_col += cm->mib_size) {
      decode_partition(pbi, &tile_data->xd,
#if CONFIG_SUPERTX
                       0,
#endif
                       mi_row, mi_col, &tile_data->bit_reader, cm->sb_size);
#if NC_MODE_INFO && CONFIG_MOTION_VAR
      detoken_and_recon_sb(pbi, &tile_data->xd, mi_row, mi_col,
                           &tile_data->bit_reader, cm->sb_size);
#endif
    }
  }
  return !tile_data->xd.corrupted;
}

// sorts in descending order
static int compare_tile_buffers(const void *a, const void *b) {
  const TileBufferDec *const buf1 = (const TileBufferDec *)a;
  const TileBufferDec *const buf2 = (const TileBufferDec *)b;
  return (int)(buf2->size - buf1->size);
}

static const uint8_t *decode_tiles_mt(AV1Decoder *pbi, const uint8_t *data,
                                      const uint8_t *data_end) {
  AV1_COMMON *const cm = &pbi->common;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  const int num_workers = AOMMIN(pbi->max_threads & ~1, tile_cols);
  TileBufferDec(*const tile_buffers)[MAX_TILE_COLS] = pbi->tile_buffers;
#if CONFIG_EXT_TILE
  const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
  const int single_row = pbi->dec_tile_row >= 0;
  const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
  const int single_col = pbi->dec_tile_col >= 0;
#endif  // CONFIG_EXT_TILE
  int tile_rows_start;
  int tile_rows_end;
  int tile_cols_start;
  int tile_cols_end;
  int tile_row, tile_col;
  int i;

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    tile_rows_start = single_row ? dec_tile_row : 0;
    tile_rows_end = single_row ? dec_tile_row + 1 : tile_rows;
    tile_cols_start = single_col ? dec_tile_col : 0;
    tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;
  } else {
#endif  // CONFIG_EXT_TILE
    tile_rows_start = 0;
    tile_rows_end = tile_rows;
    tile_cols_start = 0;
    tile_cols_end = tile_cols;
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE

#if !CONFIG_ANS
  int final_worker = -1;
#endif  // !CONFIG_ANS

  assert(tile_rows <= MAX_TILE_ROWS);
  assert(tile_cols <= MAX_TILE_COLS);

  assert(tile_cols * tile_rows > 1);

  // TODO(jzern): See if we can remove the restriction of passing in max
  // threads to the decoder.
  if (pbi->num_tile_workers == 0) {
    const int num_threads = pbi->max_threads & ~1;
    CHECK_MEM_ERROR(cm, pbi->tile_workers,
                    aom_malloc(num_threads * sizeof(*pbi->tile_workers)));
    // Ensure tile data offsets will be properly aligned. This may fail on
    // platforms without DECLARE_ALIGNED().
    assert((sizeof(*pbi->tile_worker_data) % 16) == 0);
    CHECK_MEM_ERROR(
        cm, pbi->tile_worker_data,
        aom_memalign(32, num_threads * sizeof(*pbi->tile_worker_data)));
    CHECK_MEM_ERROR(cm, pbi->tile_worker_info,
                    aom_malloc(num_threads * sizeof(*pbi->tile_worker_info)));
    for (i = 0; i < num_threads; ++i) {
      AVxWorker *const worker = &pbi->tile_workers[i];
      ++pbi->num_tile_workers;

      winterface->init(worker);
      if (i < num_threads - 1 && !winterface->reset(worker)) {
        aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                           "Tile decoder thread creation failed");
      }
    }
  }

  // Reset tile decoding hook
  for (i = 0; i < num_workers; ++i) {
    AVxWorker *const worker = &pbi->tile_workers[i];
    winterface->sync(worker);
    worker->hook = (AVxWorkerHook)tile_worker_hook;
    worker->data1 = &pbi->tile_worker_data[i];
    worker->data2 = &pbi->tile_worker_info[i];
  }

  // Initialize thread frame counts.
  if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
    for (i = 0; i < num_workers; ++i) {
      TileWorkerData *const twd = (TileWorkerData *)pbi->tile_workers[i].data1;
      av1_zero(twd->counts);
    }
  }

// Load tile data into tile_buffers
#if CONFIG_EXT_TILE
  if (cm->large_scale_tile)
    get_ls_tile_buffers(pbi, data, data_end, tile_buffers);
  else
#endif  // CONFIG_EXT_TILE
    get_tile_buffers(pbi, data, data_end, tile_buffers, 0,
                     cm->tile_rows * cm->tile_cols - 1);

  for (tile_row = tile_rows_start; tile_row < tile_rows_end; ++tile_row) {
    // Sort the buffers in this tile row based on size in descending order.
    qsort(&tile_buffers[tile_row][tile_cols_start],
          tile_cols_end - tile_cols_start, sizeof(tile_buffers[0][0]),
          compare_tile_buffers);

    // Rearrange the tile buffers in this tile row such that per-tile group
    // the largest, and presumably the most difficult tile will be decoded in
    // the main thread. This should help minimize the number of instances
    // where the main thread is waiting for a worker to complete.
    {
      int group_start;
      for (group_start = tile_cols_start; group_start < tile_cols_end;
           group_start += num_workers) {
        const int group_end = AOMMIN(group_start + num_workers, tile_cols);
        const TileBufferDec largest = tile_buffers[tile_row][group_start];
        memmove(&tile_buffers[tile_row][group_start],
                &tile_buffers[tile_row][group_start + 1],
                (group_end - group_start - 1) * sizeof(tile_buffers[0][0]));
        tile_buffers[tile_row][group_end - 1] = largest;
      }
    }

    for (tile_col = tile_cols_start; tile_col < tile_cols_end;) {
      // Launch workers for individual columns
      for (i = 0; i < num_workers && tile_col < tile_cols_end;
           ++i, ++tile_col) {
        TileBufferDec *const buf = &tile_buffers[tile_row][tile_col];
        AVxWorker *const worker = &pbi->tile_workers[i];
        TileWorkerData *const twd = (TileWorkerData *)worker->data1;
        TileInfo *const tile_info = (TileInfo *)worker->data2;

        twd->pbi = pbi;
        twd->xd = pbi->mb;
        twd->xd.corrupted = 0;
        twd->xd.counts =
            cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD
                ? &twd->counts
                : NULL;
        av1_zero(twd->dqcoeff);
        av1_tile_init(tile_info, cm, tile_row, buf->col);
        av1_tile_init(&twd->xd.tile, cm, tile_row, buf->col);

#if CONFIG_LOOPFILTERING_ACROSS_TILES
        dec_setup_across_tile_boundary_info(cm, tile_info);
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

        setup_bool_decoder(buf->data, data_end, buf->size, &cm->error,
                           &twd->bit_reader,
#if CONFIG_ANS && ANS_MAX_SYMBOLS
                           1 << cm->ans_window_size_log2,
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
                           pbi->decrypt_cb, pbi->decrypt_state);
        av1_init_macroblockd(cm, &twd->xd,
#if CONFIG_PVQ
                             twd->pvq_ref_coeff,
#endif
#if CONFIG_CFL
                             &twd->cfl,
#endif
                             twd->dqcoeff);
#if CONFIG_PVQ
        daala_dec_init(cm, &twd->xd.daala_dec, &twd->bit_reader);
        twd->xd.daala_dec.state.adapt = &twd->tctx.pvq_context;
#endif
        // Initialise the tile context from the frame context
        twd->tctx = *cm->fc;
        twd->xd.tile_ctx = &twd->tctx;
        twd->xd.plane[0].color_index_map = twd->color_index_map[0];
        twd->xd.plane[1].color_index_map = twd->color_index_map[1];

        worker->had_error = 0;
        if (i == num_workers - 1 || tile_col == tile_cols_end - 1) {
          winterface->execute(worker);
        } else {
          winterface->launch(worker);
        }

#if !CONFIG_ANS
        if (tile_row == tile_rows - 1 && buf->col == tile_cols - 1) {
          final_worker = i;
        }
#endif  // !CONFIG_ANS
      }

      // Sync all workers
      for (; i > 0; --i) {
        AVxWorker *const worker = &pbi->tile_workers[i - 1];
        // TODO(jzern): The tile may have specific error data associated with
        // its aom_internal_error_info which could be propagated to the main
        // info in cm. Additionally once the threads have been synced and an
        // error is detected, there's no point in continuing to decode tiles.
        pbi->mb.corrupted |= !winterface->sync(worker);
      }
    }
  }

  // Accumulate thread frame counts.
  if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
    for (i = 0; i < num_workers; ++i) {
      TileWorkerData *const twd = (TileWorkerData *)pbi->tile_workers[i].data1;
      av1_accumulate_frame_counts(&cm->counts, &twd->counts);
    }
  }

#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    // Return the end of the last tile buffer
    return tile_buffers[tile_rows - 1][tile_cols - 1].raw_data_end;
  } else {
#endif  // CONFIG_EXT_TILE
#if CONFIG_ANS
    return data_end;
#else
  assert(final_worker != -1);
  {
    TileWorkerData *const twd =
        (TileWorkerData *)pbi->tile_workers[final_worker].data1;
    return aom_reader_find_end(&twd->bit_reader);
  }
#endif  // CONFIG_ANS
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE
}

static void error_handler(void *data) {
  AV1_COMMON *const cm = (AV1_COMMON *)data;
  aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME, "Truncated packet");
}

static void read_bitdepth_colorspace_sampling(AV1_COMMON *cm,
                                              struct aom_read_bit_buffer *rb,
                                              int allow_lowbitdepth) {
  if (cm->profile >= PROFILE_2) {
    cm->bit_depth = aom_rb_read_bit(rb) ? AOM_BITS_12 : AOM_BITS_10;
  } else {
    cm->bit_depth = AOM_BITS_8;
  }

#if CONFIG_HIGHBITDEPTH
  cm->use_highbitdepth = cm->bit_depth > AOM_BITS_8 || !allow_lowbitdepth;
#else
  (void)allow_lowbitdepth;
#endif
#if CONFIG_COLORSPACE_HEADERS
  cm->color_space = aom_rb_read_literal(rb, 5);
  cm->transfer_function = aom_rb_read_literal(rb, 5);
#else
  cm->color_space = aom_rb_read_literal(rb, 3);
#endif
  if (cm->color_space != AOM_CS_SRGB) {
    // [16,235] (including xvycc) vs [0,255] range
    cm->color_range = aom_rb_read_bit(rb);
    if (cm->profile == PROFILE_1 || cm->profile == PROFILE_3) {
      cm->subsampling_x = aom_rb_read_bit(rb);
      cm->subsampling_y = aom_rb_read_bit(rb);
      if (cm->subsampling_x == 1 && cm->subsampling_y == 1)
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "4:2:0 color not supported in profile 1 or 3");
      if (aom_rb_read_bit(rb))
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Reserved bit set");
    } else {
      cm->subsampling_y = cm->subsampling_x = 1;
    }
#if CONFIG_COLORSPACE_HEADERS
    if (cm->subsampling_x == 1 && cm->subsampling_y == 1) {
      cm->chroma_sample_position = aom_rb_read_literal(rb, 2);
    }
#endif
  } else {
    if (cm->profile == PROFILE_1 || cm->profile == PROFILE_3) {
      // Note if colorspace is SRGB then 4:4:4 chroma sampling is assumed.
      // 4:2:2 or 4:4:0 chroma sampling is not allowed.
      cm->subsampling_y = cm->subsampling_x = 0;
      if (aom_rb_read_bit(rb))
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Reserved bit set");
    } else {
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "4:4:4 color not supported in profile 0 or 2");
    }
  }
}

#if CONFIG_REFERENCE_BUFFER
void read_sequence_header(SequenceHeader *seq_params,
                          struct aom_read_bit_buffer *rb) {
  /* Placeholder for actually reading from the bitstream */
  seq_params->frame_id_numbers_present_flag = aom_rb_read_bit(rb);
  if (seq_params->frame_id_numbers_present_flag) {
    seq_params->frame_id_length_minus7 = aom_rb_read_literal(rb, 4);
    seq_params->delta_frame_id_length_minus2 = aom_rb_read_literal(rb, 4);
  }
}
#endif  // CONFIG_REFERENCE_BUFFER

static void read_compound_tools(AV1_COMMON *cm,
                                struct aom_read_bit_buffer *rb) {
  (void)cm;
  (void)rb;
#if CONFIG_INTERINTRA
  if (!frame_is_intra_only(cm) && cm->reference_mode != COMPOUND_REFERENCE) {
    cm->allow_interintra_compound = aom_rb_read_bit(rb);
  } else {
    cm->allow_interintra_compound = 0;
  }
#endif  // CONFIG_INTERINTRA
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
#if CONFIG_COMPOUND_SINGLEREF
  if (!frame_is_intra_only(cm)) {
#else   // !CONFIG_COMPOUND_SINGLEREF
  if (!frame_is_intra_only(cm) && cm->reference_mode != SINGLE_REFERENCE) {
#endif  // CONFIG_COMPOUND_SINGLEREF
    cm->allow_masked_compound = aom_rb_read_bit(rb);
  } else {
    cm->allow_masked_compound = 0;
  }
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
}

#if CONFIG_VAR_REFS
static void check_valid_ref_frames(AV1_COMMON *cm) {
  MV_REFERENCE_FRAME ref_frame;
  // TODO(zoeliu): To handle ALTREF_FRAME the same way as do with other
  //               reference frames: Current encoder invalid ALTREF when ALTREF
  //               is the same as LAST, but invalid all the other references
  //               when they are the same as ALTREF.
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    RefBuffer *const ref_buf = &cm->frame_refs[ref_frame - LAST_FRAME];

    if (ref_buf->idx != INVALID_IDX) {
      ref_buf->is_valid = 1;

      MV_REFERENCE_FRAME ref;
      for (ref = LAST_FRAME; ref < ref_frame; ++ref) {
        RefBuffer *const buf = &cm->frame_refs[ref - LAST_FRAME];
        if (buf->is_valid && buf->idx == ref_buf->idx) {
          if (ref_frame != ALTREF_FRAME || ref == LAST_FRAME) {
            ref_buf->is_valid = 0;
            break;
          } else {
            buf->is_valid = 0;
          }
        }
      }
    } else {
      ref_buf->is_valid = 0;
    }
  }
}
#endif  // CONFIG_VAR_REFS

#if CONFIG_GLOBAL_MOTION
static int read_global_motion_params(WarpedMotionParams *params,
                                     const WarpedMotionParams *ref_params,
                                     struct aom_read_bit_buffer *rb,
                                     int allow_hp) {
  TransformationType type = aom_rb_read_bit(rb);
  if (type != IDENTITY) {
#if GLOBAL_TRANS_TYPES > 4
    type += aom_rb_read_literal(rb, GLOBAL_TYPE_BITS);
#else
    if (aom_rb_read_bit(rb))
      type = ROTZOOM;
    else
      type = aom_rb_read_bit(rb) ? TRANSLATION : AFFINE;
#endif  // GLOBAL_TRANS_TYPES > 4
  }

  int trans_bits;
  int trans_dec_factor;
  int trans_prec_diff;
  *params = default_warp_params;
  params->wmtype = type;
  switch (type) {
    case HOMOGRAPHY:
    case HORTRAPEZOID:
    case VERTRAPEZOID:
      if (type != HORTRAPEZOID)
        params->wmmat[6] =
            aom_rb_read_signed_primitive_refsubexpfin(
                rb, GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
                (ref_params->wmmat[6] >> GM_ROW3HOMO_PREC_DIFF)) *
            GM_ROW3HOMO_DECODE_FACTOR;
      if (type != VERTRAPEZOID)
        params->wmmat[7] =
            aom_rb_read_signed_primitive_refsubexpfin(
                rb, GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
                (ref_params->wmmat[7] >> GM_ROW3HOMO_PREC_DIFF)) *
            GM_ROW3HOMO_DECODE_FACTOR;
    case AFFINE:
    case ROTZOOM:
      params->wmmat[2] = aom_rb_read_signed_primitive_refsubexpfin(
                             rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                             (ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
                                 (1 << GM_ALPHA_PREC_BITS)) *
                             GM_ALPHA_DECODE_FACTOR +
                         (1 << WARPEDMODEL_PREC_BITS);
      if (type != VERTRAPEZOID)
        params->wmmat[3] = aom_rb_read_signed_primitive_refsubexpfin(
                               rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                               (ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF)) *
                           GM_ALPHA_DECODE_FACTOR;
      if (type >= AFFINE) {
        if (type != HORTRAPEZOID)
          params->wmmat[4] = aom_rb_read_signed_primitive_refsubexpfin(
                                 rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                                 (ref_params->wmmat[4] >> GM_ALPHA_PREC_DIFF)) *
                             GM_ALPHA_DECODE_FACTOR;
        params->wmmat[5] = aom_rb_read_signed_primitive_refsubexpfin(
                               rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                               (ref_params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                                   (1 << GM_ALPHA_PREC_BITS)) *
                               GM_ALPHA_DECODE_FACTOR +
                           (1 << WARPEDMODEL_PREC_BITS);
      } else {
        params->wmmat[4] = -params->wmmat[3];
        params->wmmat[5] = params->wmmat[2];
      }
    // fallthrough intended
    case TRANSLATION:
      trans_bits = (type == TRANSLATION) ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                                         : GM_ABS_TRANS_BITS;
      trans_dec_factor = (type == TRANSLATION)
                             ? GM_TRANS_ONLY_DECODE_FACTOR * (1 << !allow_hp)
                             : GM_TRANS_DECODE_FACTOR;
      trans_prec_diff = (type == TRANSLATION)
                            ? GM_TRANS_ONLY_PREC_DIFF + !allow_hp
                            : GM_TRANS_PREC_DIFF;
      params->wmmat[0] = aom_rb_read_signed_primitive_refsubexpfin(
                             rb, (1 << trans_bits) + 1, SUBEXPFIN_K,
                             (ref_params->wmmat[0] >> trans_prec_diff)) *
                         trans_dec_factor;
      params->wmmat[1] = aom_rb_read_signed_primitive_refsubexpfin(
                             rb, (1 << trans_bits) + 1, SUBEXPFIN_K,
                             (ref_params->wmmat[1] >> trans_prec_diff)) *
                         trans_dec_factor;
    case IDENTITY: break;
    default: assert(0);
  }
  if (params->wmtype <= AFFINE) {
    int good_shear_params = get_shear_params(params);
    if (!good_shear_params) return 0;
  }

  return 1;
}

static void read_global_motion(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  int frame;
  for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
    const WarpedMotionParams *ref_params =
        cm->error_resilient_mode ? &default_warp_params
                                 : &cm->prev_frame->global_motion[frame];
    int good_params = read_global_motion_params(
        &cm->global_motion[frame], ref_params, rb, cm->allow_high_precision_mv);
    if (!good_params)
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid shear parameters for global motion.");

    // TODO(sarahparker, debargha): The logic in the commented out code below
    // does not work currently and causes mismatches when resize is on. Fix it
    // before turning the optimization back on.
    /*
    YV12_BUFFER_CONFIG *ref_buf = get_ref_frame(cm, frame);
    if (cm->width == ref_buf->y_crop_width &&
        cm->height == ref_buf->y_crop_height) {
      read_global_motion_params(&cm->global_motion[frame],
                                &cm->prev_frame->global_motion[frame], rb,
                                cm->allow_high_precision_mv);
    } else {
      cm->global_motion[frame] = default_warp_params;
    }
    */
    /*
    printf("Dec Ref %d [%d/%d]: %d %d %d %d\n",
           frame, cm->current_video_frame, cm->show_frame,
           cm->global_motion[frame].wmmat[0],
           cm->global_motion[frame].wmmat[1],
           cm->global_motion[frame].wmmat[2],
           cm->global_motion[frame].wmmat[3]);
           */
  }
  memcpy(cm->cur_frame->global_motion, cm->global_motion,
         TOTAL_REFS_PER_FRAME * sizeof(WarpedMotionParams));
}
#endif  // CONFIG_GLOBAL_MOTION

static size_t read_uncompressed_header(AV1Decoder *pbi,
                                       struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  MACROBLOCKD *const xd = &pbi->mb;
  BufferPool *const pool = cm->buffer_pool;
  RefCntBuffer *const frame_bufs = pool->frame_bufs;
  int i, mask, ref_index = 0;
  size_t sz;

  cm->last_frame_type = cm->frame_type;
  cm->last_intra_only = cm->intra_only;

#if CONFIG_EXT_REFS
  // NOTE: By default all coded frames to be used as a reference
  cm->is_reference_frame = 1;
#endif  // CONFIG_EXT_REFS

#if !CONFIG_OBU
  if (aom_rb_read_literal(rb, 2) != AOM_FRAME_MARKER)
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Invalid frame marker");

  cm->profile = av1_read_profile(rb);

  const BITSTREAM_PROFILE MAX_SUPPORTED_PROFILE =
      CONFIG_HIGHBITDEPTH ? MAX_PROFILES : PROFILE_2;

  if (cm->profile >= MAX_SUPPORTED_PROFILE)
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Unsupported bitstream profile");
#endif

#if CONFIG_EXT_TILE
  cm->large_scale_tile = aom_rb_read_literal(rb, 1);
#if CONFIG_REFERENCE_BUFFER
  if (cm->large_scale_tile) cm->seq_params.frame_id_numbers_present_flag = 0;
#endif  // CONFIG_REFERENCE_BUFFER
#endif  // CONFIG_EXT_TILE

  cm->show_existing_frame = aom_rb_read_bit(rb);

  if (cm->show_existing_frame) {
    // Show an existing frame directly.
    const int existing_frame_idx = aom_rb_read_literal(rb, 3);
    const int frame_to_show = cm->ref_frame_map[existing_frame_idx];
#if CONFIG_REFERENCE_BUFFER
    if (cm->seq_params.frame_id_numbers_present_flag) {
      int frame_id_length = cm->seq_params.frame_id_length_minus7 + 7;
      int display_frame_id = aom_rb_read_literal(rb, frame_id_length);
      /* Compare display_frame_id with ref_frame_id and check valid for
       * referencing */
      if (display_frame_id != cm->ref_frame_id[existing_frame_idx] ||
          cm->valid_for_referencing[existing_frame_idx] == 0)
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Reference buffer frame ID mismatch");
    }
#endif
    lock_buffer_pool(pool);
    if (frame_to_show < 0 || frame_bufs[frame_to_show].ref_count < 1) {
      unlock_buffer_pool(pool);
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Buffer %d does not contain a decoded frame",
                         frame_to_show);
    }
    ref_cnt_fb(frame_bufs, &cm->new_fb_idx, frame_to_show);
    unlock_buffer_pool(pool);

#if CONFIG_LOOPFILTER_LEVEL
    cm->lf.filter_level[0] = 0;
    cm->lf.filter_level[1] = 0;
#else
    cm->lf.filter_level = 0;
#endif
    cm->show_frame = 1;
    pbi->refresh_frame_flags = 0;

    if (cm->frame_parallel_decode) {
      for (i = 0; i < REF_FRAMES; ++i)
        cm->next_ref_frame_map[i] = cm->ref_frame_map[i];
    }

    return 0;
  }

#if !CONFIG_OBU
  cm->frame_type = (FRAME_TYPE)aom_rb_read_bit(rb);
  cm->show_frame = aom_rb_read_bit(rb);
  if (cm->frame_type != KEY_FRAME)
    cm->intra_only = cm->show_frame ? 0 : aom_rb_read_bit(rb);
#else
  cm->frame_type = (FRAME_TYPE)aom_rb_read_literal(rb, 2);  // 2 bits
  cm->show_frame = aom_rb_read_bit(rb);
  cm->intra_only = cm->frame_type == INTRA_ONLY_FRAME;
#endif
  cm->error_resilient_mode = aom_rb_read_bit(rb);
#if CONFIG_REFERENCE_BUFFER
#if !CONFIG_OBU
  if (frame_is_intra_only(cm)) read_sequence_header(&cm->seq_params, rb);
#endif  // !CONFIG_OBU
  if (cm->seq_params.frame_id_numbers_present_flag) {
    int frame_id_length = cm->seq_params.frame_id_length_minus7 + 7;
    int diff_len = cm->seq_params.delta_frame_id_length_minus2 + 2;
    int prev_frame_id = 0;
    if (cm->frame_type != KEY_FRAME) {
      prev_frame_id = cm->current_frame_id;
    }
    cm->current_frame_id = aom_rb_read_literal(rb, frame_id_length);

    if (cm->frame_type != KEY_FRAME) {
      int diff_frame_id;
      if (cm->current_frame_id > prev_frame_id) {
        diff_frame_id = cm->current_frame_id - prev_frame_id;
      } else {
        diff_frame_id =
            (1 << frame_id_length) + cm->current_frame_id - prev_frame_id;
      }
      /* Check current_frame_id for conformance */
      if (prev_frame_id == cm->current_frame_id ||
          diff_frame_id >= (1 << (frame_id_length - 1))) {
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Invalid value of current_frame_id");
      }
    }
    /* Check if some frames need to be marked as not valid for referencing */
    for (i = 0; i < REF_FRAMES; i++) {
      if (cm->frame_type == KEY_FRAME) {
        cm->valid_for_referencing[i] = 0;
      } else if (cm->current_frame_id - (1 << diff_len) > 0) {
        if (cm->ref_frame_id[i] > cm->current_frame_id ||
            cm->ref_frame_id[i] < cm->current_frame_id - (1 << diff_len))
          cm->valid_for_referencing[i] = 0;
      } else {
        if (cm->ref_frame_id[i] > cm->current_frame_id &&
            cm->ref_frame_id[i] <
                (1 << frame_id_length) + cm->current_frame_id - (1 << diff_len))
          cm->valid_for_referencing[i] = 0;
      }
    }
  }
#endif  // CONFIG_REFERENCE_BUFFER
  if (cm->frame_type == KEY_FRAME) {
#if !CONFIG_OBU
    read_bitdepth_colorspace_sampling(cm, rb, pbi->allow_lowbitdepth);
#endif
    pbi->refresh_frame_flags = (1 << REF_FRAMES) - 1;

    for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
      cm->frame_refs[i].idx = INVALID_IDX;
      cm->frame_refs[i].buf = NULL;
#if CONFIG_VAR_REFS
      cm->frame_refs[i].is_valid = 0;
#endif  // CONFIG_VAR_REFS
    }

    setup_frame_size(cm, rb);
    setup_sb_size(cm, rb);

    if (pbi->need_resync) {
      memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
      pbi->need_resync = 0;
    }
#if CONFIG_ANS && ANS_MAX_SYMBOLS
    cm->ans_window_size_log2 = aom_rb_read_literal(rb, 4) + 8;
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
    cm->allow_screen_content_tools = aom_rb_read_bit(rb);
#if CONFIG_AMVR
    if (cm->allow_screen_content_tools) {
      if (aom_rb_read_bit(rb)) {
        cm->seq_mv_precision_level = 2;
      } else {
        cm->seq_mv_precision_level = aom_rb_read_bit(rb) ? 0 : 1;
      }
    } else {
      cm->seq_mv_precision_level = 0;
    }
#endif
#if CONFIG_TEMPMV_SIGNALING
    cm->use_prev_frame_mvs = 0;
#endif
  } else {
    if (cm->intra_only) cm->allow_screen_content_tools = aom_rb_read_bit(rb);
#if CONFIG_TEMPMV_SIGNALING
    if (cm->intra_only || cm->error_resilient_mode) cm->use_prev_frame_mvs = 0;
#endif
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
// The only way to reset all frame contexts to their default values is with a
// keyframe.
#else
    if (cm->error_resilient_mode) {
      cm->reset_frame_context = RESET_FRAME_CONTEXT_ALL;
    } else {
      if (cm->intra_only) {
        cm->reset_frame_context = aom_rb_read_bit(rb)
                                      ? RESET_FRAME_CONTEXT_ALL
                                      : RESET_FRAME_CONTEXT_CURRENT;
      } else {
        cm->reset_frame_context = aom_rb_read_bit(rb)
                                      ? RESET_FRAME_CONTEXT_CURRENT
                                      : RESET_FRAME_CONTEXT_NONE;
        if (cm->reset_frame_context == RESET_FRAME_CONTEXT_CURRENT)
          cm->reset_frame_context = aom_rb_read_bit(rb)
                                        ? RESET_FRAME_CONTEXT_ALL
                                        : RESET_FRAME_CONTEXT_CURRENT;
      }
    }
#endif

    if (cm->intra_only) {
#if !CONFIG_OBU
      read_bitdepth_colorspace_sampling(cm, rb, pbi->allow_lowbitdepth);
#endif

      pbi->refresh_frame_flags = aom_rb_read_literal(rb, REF_FRAMES);
      setup_frame_size(cm, rb);
      setup_sb_size(cm, rb);
      if (pbi->need_resync) {
        memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
        pbi->need_resync = 0;
      }
#if CONFIG_ANS && ANS_MAX_SYMBOLS
      cm->ans_window_size_log2 = aom_rb_read_literal(rb, 4) + 8;
#endif
    } else if (pbi->need_resync != 1) { /* Skip if need resync */
#if CONFIG_OBU
      pbi->refresh_frame_flags = (cm->frame_type == S_FRAME)
                                     ? ~(1 << REF_FRAMES)
                                     : aom_rb_read_literal(rb, REF_FRAMES);
#else
      pbi->refresh_frame_flags = aom_rb_read_literal(rb, REF_FRAMES);
#endif

#if CONFIG_EXT_REFS
      if (!pbi->refresh_frame_flags) {
        // NOTE: "pbi->refresh_frame_flags == 0" indicates that the coded frame
        //       will not be used as a reference
        cm->is_reference_frame = 0;
      }
#endif  // CONFIG_EXT_REFS

      for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
        const int ref = aom_rb_read_literal(rb, REF_FRAMES_LOG2);
        const int idx = cm->ref_frame_map[ref];

        // Most of the time, streams start with a keyframe. In that case,
        // ref_frame_map will have been filled in at that point and will not
        // contain any -1's. However, streams are explicitly allowed to start
        // with an intra-only frame, so long as they don't then signal a
        // reference to a slot that hasn't been set yet. That's what we are
        // checking here.
        if (idx == -1)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Inter frame requests nonexistent reference");

        RefBuffer *const ref_frame = &cm->frame_refs[i];
        ref_frame->idx = idx;
        ref_frame->buf = &frame_bufs[idx].buf;
#if CONFIG_FRAME_SIGN_BIAS
#if CONFIG_OBU
        // NOTE: For the scenario of (cm->frame_type != S_FRAME),
        // ref_frame_sign_bias will be reset based on frame offsets.
        cm->ref_frame_sign_bias[LAST_FRAME + i] = 0;
#endif  // CONFIG_OBU
#else   // !CONFIG_FRAME_SIGN_BIAS
#if CONFIG_OBU
        cm->ref_frame_sign_bias[LAST_FRAME + i] =
            (cm->frame_type == S_FRAME) ? 0 : aom_rb_read_bit(rb);
#else   // !CONFIG_OBU
        cm->ref_frame_sign_bias[LAST_FRAME + i] = aom_rb_read_bit(rb);
#endif  // CONFIG_OBU
#endif  // CONFIG_FRAME_SIGN_BIAS
#if CONFIG_REFERENCE_BUFFER
        if (cm->seq_params.frame_id_numbers_present_flag) {
          int frame_id_length = cm->seq_params.frame_id_length_minus7 + 7;
          int diff_len = cm->seq_params.delta_frame_id_length_minus2 + 2;
          int delta_frame_id_minus1 = aom_rb_read_literal(rb, diff_len);
          int ref_frame_id =
              ((cm->current_frame_id - (delta_frame_id_minus1 + 1) +
                (1 << frame_id_length)) %
               (1 << frame_id_length));
          /* Compare values derived from delta_frame_id_minus1 and
           * refresh_frame_flags. Also, check valid for referencing */
          if (ref_frame_id != cm->ref_frame_id[ref] ||
              cm->valid_for_referencing[ref] == 0)
            aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                               "Reference buffer frame ID mismatch");
        }
#endif  // CONFIG_REFERENCE_BUFFER
      }

#if CONFIG_VAR_REFS
      check_valid_ref_frames(cm);
#endif  // CONFIG_VAR_REFS

#if CONFIG_FRAME_SIZE
      if (cm->error_resilient_mode == 0) {
        setup_frame_size_with_refs(cm, rb);
      } else {
        setup_frame_size(cm, rb);
      }
#else
      setup_frame_size_with_refs(cm, rb);
#endif

#if CONFIG_AMVR
      if (cm->seq_mv_precision_level == 2) {
        cm->cur_frame_mv_precision_level = aom_rb_read_bit(rb) ? 0 : 1;
      } else {
        cm->cur_frame_mv_precision_level = cm->seq_mv_precision_level;
      }
#endif
      cm->allow_high_precision_mv = aom_rb_read_bit(rb);
      cm->interp_filter = read_frame_interp_filter(rb);
#if CONFIG_TEMPMV_SIGNALING
      if (frame_might_use_prev_frame_mvs(cm))
        cm->use_prev_frame_mvs = aom_rb_read_bit(rb);
      else
        cm->use_prev_frame_mvs = 0;
#endif
      for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
        RefBuffer *const ref_buf = &cm->frame_refs[i];
#if CONFIG_HIGHBITDEPTH
        av1_setup_scale_factors_for_frame(
            &ref_buf->sf, ref_buf->buf->y_crop_width,
            ref_buf->buf->y_crop_height, cm->width, cm->height,
            cm->use_highbitdepth);
#else
        av1_setup_scale_factors_for_frame(
            &ref_buf->sf, ref_buf->buf->y_crop_width,
            ref_buf->buf->y_crop_height, cm->width, cm->height);
#endif
      }
    }
  }

#if CONFIG_FRAME_MARKER
  if (cm->show_frame == 0) {
    cm->frame_offset = cm->current_video_frame + aom_rb_read_literal(rb, 4);
  } else {
    cm->frame_offset = cm->current_video_frame;
  }
  av1_setup_frame_buf_refs(cm);

#if CONFIG_FRAME_SIGN_BIAS
#if CONFIG_OBU
  if (cm->frame_type != S_FRAME)
#endif  // CONFIG_OBU
    av1_setup_frame_sign_bias(cm);
#define FRAME_SIGN_BIAS_DEBUG 0
#if FRAME_SIGN_BIAS_DEBUG
  {
    printf("\n\nDECODER: Frame=%d, show_frame=%d:", cm->current_video_frame,
           cm->show_frame);
    MV_REFERENCE_FRAME ref_frame;
    for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
      printf(" sign_bias[%d]=%d", ref_frame,
             cm->ref_frame_sign_bias[ref_frame]);
    }
    printf("\n");
  }
#endif  // FRAME_SIGN_BIAS_DEBUG
#undef FRAME_SIGN_BIAS_DEBUG
#endif  // CONFIG_FRAME_SIGN_BIAS
#endif  // CONFIG_FRAME_MARKER

#if CONFIG_TEMPMV_SIGNALING
  cm->cur_frame->intra_only = cm->frame_type == KEY_FRAME || cm->intra_only;
#endif

#if CONFIG_REFERENCE_BUFFER
  if (cm->seq_params.frame_id_numbers_present_flag) {
    /* If bitmask is set, update reference frame id values and
       mark frames as valid for reference */
    int refresh_frame_flags =
        cm->frame_type == KEY_FRAME ? 0xFF : pbi->refresh_frame_flags;
    for (i = 0; i < REF_FRAMES; i++) {
      if ((refresh_frame_flags >> i) & 1) {
        cm->ref_frame_id[i] = cm->current_frame_id;
        cm->valid_for_referencing[i] = 1;
      }
    }
  }
#endif  // CONFIG_REFERENCE_BUFFER

  get_frame_new_buffer(cm)->bit_depth = cm->bit_depth;
  get_frame_new_buffer(cm)->color_space = cm->color_space;
#if CONFIG_COLORSPACE_HEADERS
  get_frame_new_buffer(cm)->transfer_function = cm->transfer_function;
  get_frame_new_buffer(cm)->chroma_sample_position = cm->chroma_sample_position;
#endif
  get_frame_new_buffer(cm)->color_range = cm->color_range;
  get_frame_new_buffer(cm)->render_width = cm->render_width;
  get_frame_new_buffer(cm)->render_height = cm->render_height;

  if (pbi->need_resync) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Keyframe / intra-only frame required to reset decoder"
                       " state");
  }

  if (!cm->error_resilient_mode) {
    cm->refresh_frame_context = aom_rb_read_bit(rb)
                                    ? REFRESH_FRAME_CONTEXT_FORWARD
                                    : REFRESH_FRAME_CONTEXT_BACKWARD;
  } else {
    cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_FORWARD;
  }
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
  // This flag will be overridden by the call to av1_setup_past_independence
  // below, forcing the use of context 0 for those frame types.
  cm->frame_context_idx = aom_rb_read_literal(rb, FRAME_CONTEXTS_LOG2);
#endif

  // Generate next_ref_frame_map.
  lock_buffer_pool(pool);
  for (mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
    if (mask & 1) {
      cm->next_ref_frame_map[ref_index] = cm->new_fb_idx;
      ++frame_bufs[cm->new_fb_idx].ref_count;
    } else {
      cm->next_ref_frame_map[ref_index] = cm->ref_frame_map[ref_index];
    }
    // Current thread holds the reference frame.
    if (cm->ref_frame_map[ref_index] >= 0)
      ++frame_bufs[cm->ref_frame_map[ref_index]].ref_count;
    ++ref_index;
  }

  for (; ref_index < REF_FRAMES; ++ref_index) {
    cm->next_ref_frame_map[ref_index] = cm->ref_frame_map[ref_index];

    // Current thread holds the reference frame.
    if (cm->ref_frame_map[ref_index] >= 0)
      ++frame_bufs[cm->ref_frame_map[ref_index]].ref_count;
  }
  unlock_buffer_pool(pool);
  pbi->hold_ref_buf = 1;

  if (frame_is_intra_only(cm) || cm->error_resilient_mode)
    av1_setup_past_independence(cm);

  setup_loopfilter(cm, rb);
  setup_quantization(cm, rb);
  xd->bd = (int)cm->bit_depth;

#if CONFIG_Q_ADAPT_PROBS
  av1_default_coef_probs(cm);
  if (cm->frame_type == KEY_FRAME || cm->error_resilient_mode ||
      cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL) {
    for (i = 0; i < FRAME_CONTEXTS; ++i) cm->frame_contexts[i] = *cm->fc;
  } else if (cm->reset_frame_context == RESET_FRAME_CONTEXT_CURRENT) {
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
    if (cm->frame_refs[0].idx <= 0) {
      cm->frame_contexts[cm->frame_refs[0].idx] = *cm->fc;
    }
#else
    cm->frame_contexts[cm->frame_context_idx] = *cm->fc;
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING
  }
#endif  // CONFIG_Q_ADAPT_PROBS

  setup_segmentation(cm, rb);

  {
    struct segmentation *const seg = &cm->seg;
    int segment_quantizer_active = 0;
    for (i = 0; i < MAX_SEGMENTS; i++) {
      if (segfeature_active(seg, i, SEG_LVL_ALT_Q)) {
        segment_quantizer_active = 1;
      }
    }

    cm->delta_q_res = 1;
#if CONFIG_EXT_DELTA_Q
    cm->delta_lf_res = 1;
    cm->delta_lf_present_flag = 0;
#if CONFIG_LOOPFILTER_LEVEL
    cm->delta_lf_multi = 0;
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif
    if (segment_quantizer_active == 0 && cm->base_qindex > 0) {
      cm->delta_q_present_flag = aom_rb_read_bit(rb);
    } else {
      cm->delta_q_present_flag = 0;
    }
    if (cm->delta_q_present_flag) {
      xd->prev_qindex = cm->base_qindex;
      cm->delta_q_res = 1 << aom_rb_read_literal(rb, 2);
#if CONFIG_EXT_DELTA_Q
      assert(!segment_quantizer_active);
      cm->delta_lf_present_flag = aom_rb_read_bit(rb);
      if (cm->delta_lf_present_flag) {
        xd->prev_delta_lf_from_base = 0;
        cm->delta_lf_res = 1 << aom_rb_read_literal(rb, 2);
#if CONFIG_LOOPFILTER_LEVEL
        cm->delta_lf_multi = aom_rb_read_bit(rb);
        for (int lf_id = 0; lf_id < FRAME_LF_COUNT; ++lf_id)
          xd->prev_delta_lf[lf_id] = 0;
#endif  // CONFIG_LOOPFILTER_LEVEL
      }
#endif  // CONFIG_EXT_DELTA_Q
    }
  }
#if CONFIG_AMVR
  xd->cur_frame_mv_precision_level = cm->cur_frame_mv_precision_level;
#endif

  for (i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = cm->seg.enabled
                           ? av1_get_qindex(&cm->seg, i, cm->base_qindex)
                           : cm->base_qindex;
    xd->lossless[i] = qindex == 0 && cm->y_dc_delta_q == 0 &&
                      cm->uv_dc_delta_q == 0 && cm->uv_ac_delta_q == 0;
    xd->qindex[i] = qindex;
  }
  cm->all_lossless = all_lossless(cm, xd);
  setup_segmentation_dequant(cm);
#if CONFIG_CDEF
  if (!cm->all_lossless) {
    setup_cdef(cm, rb);
  }
#endif
#if CONFIG_LOOP_RESTORATION
  decode_restoration_mode(cm, rb);
#endif  // CONFIG_LOOP_RESTORATION
  cm->tx_mode = read_tx_mode(cm, rb);
  cm->reference_mode = read_frame_reference_mode(cm, rb);
  if (cm->reference_mode != SINGLE_REFERENCE) setup_compound_reference_mode(cm);
  read_compound_tools(cm, rb);

#if CONFIG_EXT_TX
  cm->reduced_tx_set_used = aom_rb_read_bit(rb);
#endif  // CONFIG_EXT_TX

#if CONFIG_ADAPT_SCAN
  cm->use_adapt_scan = aom_rb_read_bit(rb);
  // TODO(angiebird): call av1_init_scan_order only when use_adapt_scan
  // switches from 1 to 0
  if (cm->use_adapt_scan == 0) av1_init_scan_order(cm);
#endif  // CONFIG_ADAPT_SCAN

#if CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING
  // NOTE(zoeliu): As cm->prev_frame can take neither a frame of
  //               show_exisiting_frame=1, nor can it take a frame not used as
  //               a reference, it is probable that by the time it is being
  //               referred to, the frame buffer it originally points to may
  //               already get expired and have been reassigned to the current
  //               newly coded frame. Hence, we need to check whether this is
  //               the case, and if yes, we have 2 choices:
  //               (1) Simply disable the use of previous frame mvs; or
  //               (2) Have cm->prev_frame point to one reference frame buffer,
  //                   e.g. LAST_FRAME.
  if (!dec_is_ref_frame_buf(pbi, cm->prev_frame)) {
    // Reassign the LAST_FRAME buffer to cm->prev_frame.
    cm->prev_frame =
        cm->frame_refs[LAST_FRAME - LAST_FRAME].idx != INVALID_IDX
            ? &cm->buffer_pool
                   ->frame_bufs[cm->frame_refs[LAST_FRAME - LAST_FRAME].idx]
            : NULL;
  }
#endif  // CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING

#if CONFIG_TEMPMV_SIGNALING
  if (cm->use_prev_frame_mvs && !frame_can_use_prev_frame_mvs(cm)) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Frame wrongly requests previous frame MVs");
  }
#else
  cm->use_prev_frame_mvs = !cm->error_resilient_mode && cm->prev_frame &&
#if CONFIG_FRAME_SUPERRES
                           cm->width == cm->last_width &&
                           cm->height == cm->last_height &&
#else
                           cm->width == cm->prev_frame->buf.y_crop_width &&
                           cm->height == cm->prev_frame->buf.y_crop_height &&
#endif  // CONFIG_FRAME_SUPERRES
                           !cm->last_intra_only && cm->last_show_frame &&
                           (cm->last_frame_type != KEY_FRAME);
#endif  // CONFIG_TEMPMV_SIGNALING

#if CONFIG_GLOBAL_MOTION
  if (!frame_is_intra_only(cm)) read_global_motion(cm, rb);
#endif

  read_tile_info(pbi, rb);
  if (use_compressed_header(cm)) {
    sz = aom_rb_read_literal(rb, 16);
    if (sz == 0)
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid header size");
  } else {
    sz = 0;
  }
  return sz;
}

#if CONFIG_SUPERTX
static void read_supertx_probs(FRAME_CONTEXT *fc, aom_reader *r) {
  int i, j;
  if (aom_read(r, GROUP_DIFF_UPDATE_PROB, ACCT_STR)) {
    for (i = 0; i < PARTITION_SUPERTX_CONTEXTS; ++i) {
      for (j = TX_8X8; j < TX_SIZES; ++j) {
        av1_diff_update_prob(r, &fc->supertx_prob[i][j], ACCT_STR);
      }
    }
  }
}
#endif  // CONFIG_SUPERTX

static int read_compressed_header(AV1Decoder *pbi, const uint8_t *data,
                                  size_t partition_size) {
#if CONFIG_RESTRICT_COMPRESSED_HDR
  (void)pbi;
  (void)data;
  (void)partition_size;
  return 0;
#else
  AV1_COMMON *const cm = &pbi->common;
#if CONFIG_SUPERTX
  MACROBLOCKD *const xd = &pbi->mb;
#endif
  aom_reader r;
#if !CONFIG_NEW_MULTISYMBOL
  FRAME_CONTEXT *const fc = cm->fc;
  int i;
#endif

#if CONFIG_ANS && ANS_MAX_SYMBOLS
  r.window_size = 1 << cm->ans_window_size_log2;
#endif
  if (aom_reader_init(&r, data, partition_size, pbi->decrypt_cb,
                      pbi->decrypt_state))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate bool decoder 0");

#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
  if (cm->tx_mode == TX_MODE_SELECT)
    av1_diff_update_prob(&r, &fc->quarter_tx_size_prob, ACCT_STR);
#endif

#if CONFIG_LV_MAP && !LV_MAP_PROB
  av1_read_txb_probs(fc, cm->tx_mode, &r, &cm->counts);
#endif  // CONFIG_LV_MAP && !LV_MAP_PROB

#if !CONFIG_NEW_MULTISYMBOL
#if CONFIG_VAR_TX
  if (cm->tx_mode == TX_MODE_SELECT)
    for (i = 0; i < TXFM_PARTITION_CONTEXTS; ++i)
      av1_diff_update_prob(&r, &fc->txfm_partition_prob[i], ACCT_STR);
#endif  // CONFIG_VAR_TX
  for (i = 0; i < SKIP_CONTEXTS; ++i)
    av1_diff_update_prob(&r, &fc->skip_probs[i], ACCT_STR);
#endif

  if (!frame_is_intra_only(cm)) {
#if !CONFIG_NEW_MULTISYMBOL
    read_inter_mode_probs(fc, &r);
#endif

#if CONFIG_INTERINTRA
    if (cm->reference_mode != COMPOUND_REFERENCE &&
        cm->allow_interintra_compound) {
#if !CONFIG_NEW_MULTISYMBOL
      for (i = 0; i < BLOCK_SIZE_GROUPS; i++) {
        if (is_interintra_allowed_bsize_group(i)) {
          av1_diff_update_prob(&r, &fc->interintra_prob[i], ACCT_STR);
        }
      }
#endif
#if CONFIG_WEDGE && !CONFIG_NEW_MULTISYMBOL
#if CONFIG_EXT_PARTITION_TYPES
      int block_sizes_to_update = BLOCK_SIZES_ALL;
#else
      int block_sizes_to_update = BLOCK_SIZES;
#endif
      for (i = 0; i < block_sizes_to_update; i++) {
        if (is_interintra_allowed_bsize(i) && is_interintra_wedge_used(i)) {
          av1_diff_update_prob(&r, &fc->wedge_interintra_prob[i], ACCT_STR);
        }
      }
#endif  // CONFIG_WEDGE
    }
#endif  // CONFIG_INTERINTRA

#if !CONFIG_NEW_MULTISYMBOL
    for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
      av1_diff_update_prob(&r, &fc->intra_inter_prob[i], ACCT_STR);
#endif

#if !CONFIG_NEW_MULTISYMBOL
    read_frame_reference_mode_probs(cm, &r);
#endif

#if CONFIG_COMPOUND_SINGLEREF
    for (i = 0; i < COMP_INTER_MODE_CONTEXTS; i++)
      av1_diff_update_prob(&r, &fc->comp_inter_mode_prob[i], ACCT_STR);
#endif  // CONFIG_COMPOUND_SINGLEREF

#if !CONFIG_NEW_MULTISYMBOL
#if CONFIG_AMVR
    if (cm->cur_frame_mv_precision_level == 0) {
#endif
      for (i = 0; i < NMV_CONTEXTS; ++i)
        read_mv_probs(&fc->nmvc[i], cm->allow_high_precision_mv, &r);
#if CONFIG_AMVR
    }
#endif
#endif
#if CONFIG_SUPERTX
    if (!xd->lossless[0]) read_supertx_probs(fc, &r);
#endif
  }

  return aom_reader_has_error(&r);
#endif  // CONFIG_RESTRICT_COMPRESSED_HDR
}

#ifdef NDEBUG
#define debug_check_frame_counts(cm) (void)0
#else  // !NDEBUG
// Counts should only be incremented when frame_parallel_decoding_mode and
// error_resilient_mode are disabled.
static void debug_check_frame_counts(const AV1_COMMON *const cm) {
  FRAME_COUNTS zero_counts;
  av1_zero(zero_counts);
  assert(cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_BACKWARD ||
         cm->error_resilient_mode);
  assert(!memcmp(cm->counts.partition, zero_counts.partition,
                 sizeof(cm->counts.partition)));
  assert(!memcmp(cm->counts.switchable_interp, zero_counts.switchable_interp,
                 sizeof(cm->counts.switchable_interp)));
  assert(!memcmp(cm->counts.inter_compound_mode,
                 zero_counts.inter_compound_mode,
                 sizeof(cm->counts.inter_compound_mode)));
#if CONFIG_INTERINTRA
  assert(!memcmp(cm->counts.interintra, zero_counts.interintra,
                 sizeof(cm->counts.interintra)));
#if CONFIG_WEDGE
  assert(!memcmp(cm->counts.wedge_interintra, zero_counts.wedge_interintra,
                 sizeof(cm->counts.wedge_interintra)));
#endif  // CONFIG_WEDGE
#endif  // CONFIG_INTERINTRA
  assert(!memcmp(cm->counts.compound_interinter,
                 zero_counts.compound_interinter,
                 sizeof(cm->counts.compound_interinter)));
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  assert(!memcmp(cm->counts.motion_mode, zero_counts.motion_mode,
                 sizeof(cm->counts.motion_mode)));
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_NCOBMC_ADAPT_WEIGHT && CONFIG_MOTION_VAR
  assert(!memcmp(cm->counts.ncobmc_mode, zero_counts.ncobmc_mode,
                 sizeof(cm->counts.ncobmc_mode)));
#endif
  assert(!memcmp(cm->counts.intra_inter, zero_counts.intra_inter,
                 sizeof(cm->counts.intra_inter)));
#if CONFIG_COMPOUND_SINGLEREF
  assert(!memcmp(cm->counts.comp_inter_mode, zero_counts.comp_inter_mode,
                 sizeof(cm->counts.comp_inter_mode)));
#endif  // CONFIG_COMPOUND_SINGLEREF
  assert(!memcmp(cm->counts.comp_inter, zero_counts.comp_inter,
                 sizeof(cm->counts.comp_inter)));
#if CONFIG_EXT_COMP_REFS
  assert(!memcmp(cm->counts.comp_ref_type, zero_counts.comp_ref_type,
                 sizeof(cm->counts.comp_ref_type)));
  assert(!memcmp(cm->counts.uni_comp_ref, zero_counts.uni_comp_ref,
                 sizeof(cm->counts.uni_comp_ref)));
#endif  // CONFIG_EXT_COMP_REFS
  assert(!memcmp(cm->counts.single_ref, zero_counts.single_ref,
                 sizeof(cm->counts.single_ref)));
  assert(!memcmp(cm->counts.comp_ref, zero_counts.comp_ref,
                 sizeof(cm->counts.comp_ref)));
#if CONFIG_EXT_REFS
  assert(!memcmp(cm->counts.comp_bwdref, zero_counts.comp_bwdref,
                 sizeof(cm->counts.comp_bwdref)));
#endif  // CONFIG_EXT_REFS
  assert(!memcmp(&cm->counts.tx_size, &zero_counts.tx_size,
                 sizeof(cm->counts.tx_size)));
  assert(!memcmp(cm->counts.skip, zero_counts.skip, sizeof(cm->counts.skip)));
  assert(
      !memcmp(&cm->counts.mv[0], &zero_counts.mv[0], sizeof(cm->counts.mv[0])));
  assert(
      !memcmp(&cm->counts.mv[1], &zero_counts.mv[1], sizeof(cm->counts.mv[0])));
}
#endif  // NDEBUG

static struct aom_read_bit_buffer *init_read_bit_buffer(
    AV1Decoder *pbi, struct aom_read_bit_buffer *rb, const uint8_t *data,
    const uint8_t *data_end, uint8_t clear_data[MAX_AV1_HEADER_SIZE]) {
  rb->bit_offset = 0;
  rb->error_handler = error_handler;
  rb->error_handler_data = &pbi->common;
  if (pbi->decrypt_cb) {
    const int n = (int)AOMMIN(MAX_AV1_HEADER_SIZE, data_end - data);
    pbi->decrypt_cb(pbi->decrypt_state, data, clear_data, n);
    rb->bit_buffer = clear_data;
    rb->bit_buffer_end = clear_data + n;
  } else {
    rb->bit_buffer = data;
    rb->bit_buffer_end = data_end;
  }
  return rb;
}

//------------------------------------------------------------------------------

void av1_read_frame_size(struct aom_read_bit_buffer *rb, int *width,
                         int *height) {
  *width = aom_rb_read_literal(rb, 16) + 1;
  *height = aom_rb_read_literal(rb, 16) + 1;
}

BITSTREAM_PROFILE av1_read_profile(struct aom_read_bit_buffer *rb) {
  int profile = aom_rb_read_bit(rb);
  profile |= aom_rb_read_bit(rb) << 1;
  if (profile > 2) profile += aom_rb_read_bit(rb);
  return (BITSTREAM_PROFILE)profile;
}

static void make_update_tile_list_dec(AV1Decoder *pbi, int tile_rows,
                                      int tile_cols, FRAME_CONTEXT *ec_ctxs[]) {
  int i;
  for (i = 0; i < tile_rows * tile_cols; ++i)
    ec_ctxs[i] = &pbi->tile_data[i].tctx;
}

#if CONFIG_FRAME_SUPERRES
void superres_post_decode(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;

  if (av1_superres_unscaled(cm)) return;

  lock_buffer_pool(pool);
  av1_superres_upscale(cm, pool);
  unlock_buffer_pool(pool);
}
#endif  // CONFIG_FRAME_SUPERRES

static void dec_setup_frame_boundary_info(AV1_COMMON *const cm) {
// Note: When LOOPFILTERING_ACROSS_TILES is enabled, we need to clear the
// boundary information every frame, since the tile boundaries may
// change every frame (particularly when dependent-horztiles is also
// enabled); when it is disabled, the only information stored is the frame
// boundaries, which only depend on the frame size.
#if !CONFIG_LOOPFILTERING_ACROSS_TILES
  if (cm->width != cm->last_width || cm->height != cm->last_height)
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
  {
    int row, col;
    for (row = 0; row < cm->mi_rows; ++row) {
      MODE_INFO *mi = cm->mi + row * cm->mi_stride;
      for (col = 0; col < cm->mi_cols; ++col) {
        mi->mbmi.boundary_info = 0;
        mi++;
      }
    }
    av1_setup_frame_boundary_info(cm);
  }
}

size_t av1_decode_frame_headers_and_setup(AV1Decoder *pbi, const uint8_t *data,
                                          const uint8_t *data_end,
                                          const uint8_t **p_data_end) {
  AV1_COMMON *const cm = &pbi->common;
  MACROBLOCKD *const xd = &pbi->mb;
  struct aom_read_bit_buffer rb;
  uint8_t clear_data[MAX_AV1_HEADER_SIZE];
  size_t first_partition_size;
  YV12_BUFFER_CONFIG *new_fb;
#if CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING
  RefBuffer *last_fb_ref_buf = &cm->frame_refs[LAST_FRAME - LAST_FRAME];
#endif  // CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING

#if CONFIG_ADAPT_SCAN
  av1_deliver_eob_threshold(cm, xd);
#endif
#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_set_frame_read(cm->current_video_frame * 2 + cm->show_frame);
#endif

#if CONFIG_GLOBAL_MOTION
  int i;
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    cm->global_motion[i] = default_warp_params;
    cm->cur_frame->global_motion[i] = default_warp_params;
  }
  xd->global_motion = cm->global_motion;
#endif  // CONFIG_GLOBAL_MOTION

  first_partition_size = read_uncompressed_header(
      pbi, init_read_bit_buffer(pbi, &rb, data, data_end, clear_data));

#if CONFIG_EXT_TILE
  // If cm->single_tile_decoding = 0, the independent decoding of a single tile
  // or a section of a frame is not allowed.
  if (!cm->single_tile_decoding &&
      (pbi->dec_tile_row >= 0 || pbi->dec_tile_col >= 0)) {
    pbi->dec_tile_row = -1;
    pbi->dec_tile_col = -1;
  }
#endif  // CONFIG_EXT_TILE

  pbi->first_partition_size = first_partition_size;
  pbi->uncomp_hdr_size = aom_rb_bytes_read(&rb);
  new_fb = get_frame_new_buffer(cm);
  xd->cur_buf = new_fb;
#if CONFIG_INTRABC
#if CONFIG_HIGHBITDEPTH
  av1_setup_scale_factors_for_frame(
      &xd->sf_identity, xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height,
      xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height,
      cm->use_highbitdepth);
#else
  av1_setup_scale_factors_for_frame(
      &xd->sf_identity, xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height,
      xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height);
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_INTRABC

  if (cm->show_existing_frame) {
    // showing a frame directly
    *p_data_end = data + aom_rb_bytes_read(&rb);
    return 0;
  }

  data += aom_rb_bytes_read(&rb);
  if (first_partition_size)
    if (!read_is_valid(data, first_partition_size, data_end))
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Truncated packet or corrupt header length");

  cm->setup_mi(cm);

#if CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING
  // NOTE(zoeliu): As cm->prev_frame can take neither a frame of
  //               show_exisiting_frame=1, nor can it take a frame not used as
  //               a reference, it is probable that by the time it is being
  //               referred to, the frame buffer it originally points to may
  //               already get expired and have been reassigned to the current
  //               newly coded frame. Hence, we need to check whether this is
  //               the case, and if yes, we have 2 choices:
  //               (1) Simply disable the use of previous frame mvs; or
  //               (2) Have cm->prev_frame point to one reference frame buffer,
  //                   e.g. LAST_FRAME.
  if (!dec_is_ref_frame_buf(pbi, cm->prev_frame)) {
    // Reassign the LAST_FRAME buffer to cm->prev_frame.
    cm->prev_frame = last_fb_ref_buf->idx != INVALID_IDX
                         ? &cm->buffer_pool->frame_bufs[last_fb_ref_buf->idx]
                         : NULL;
  }
#endif  // CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING

#if CONFIG_TEMPMV_SIGNALING
  if (cm->use_prev_frame_mvs && !frame_can_use_prev_frame_mvs(cm)) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Frame wrongly requests previous frame MVs");
  }
#else
  cm->use_prev_frame_mvs = !cm->error_resilient_mode && cm->prev_frame &&
#if CONFIG_FRAME_SUPERRES
                           cm->width == cm->last_width &&
                           cm->height == cm->last_height &&
#else
                           cm->width == cm->prev_frame->buf.y_crop_width &&
                           cm->height == cm->prev_frame->buf.y_crop_height &&
#endif  // CONFIG_FRAME_SUPERRES
                           !cm->last_intra_only && cm->last_show_frame &&
                           (cm->last_frame_type != KEY_FRAME);
#endif  // CONFIG_TEMPMV_SIGNALING

#if CONFIG_MFMV
  av1_setup_motion_field(cm);
#endif  // CONFIG_MFMV

  av1_setup_block_planes(xd, cm->subsampling_x, cm->subsampling_y);
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
  if (cm->error_resilient_mode || frame_is_intra_only(cm)) {
    // use the default frame context values
    *cm->fc = cm->frame_contexts[FRAME_CONTEXT_DEFAULTS];
    cm->pre_fc = &cm->frame_contexts[FRAME_CONTEXT_DEFAULTS];
  } else {
    *cm->fc = cm->frame_contexts[cm->frame_refs[0].idx];
    cm->pre_fc = &cm->frame_contexts[cm->frame_refs[0].idx];
  }
#else
  *cm->fc = cm->frame_contexts[cm->frame_context_idx];
  cm->pre_fc = &cm->frame_contexts[cm->frame_context_idx];
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING
  if (!cm->fc->initialized)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Uninitialized entropy context.");

  av1_zero(cm->counts);

  xd->corrupted = 0;
  if (first_partition_size) {
    new_fb->corrupted = read_compressed_header(pbi, data, first_partition_size);
    if (new_fb->corrupted)
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Decode failed. Frame data header is corrupted.");
  }
  return first_partition_size;
}

void av1_decode_tg_tiles_and_wrapup(AV1Decoder *pbi, const uint8_t *data,
                                    const uint8_t *data_end,
                                    const uint8_t **p_data_end, int startTile,
                                    int endTile, int initialize_flag) {
  AV1_COMMON *const cm = &pbi->common;
  MACROBLOCKD *const xd = &pbi->mb;
  int context_updated = 0;

#if CONFIG_LOOP_RESTORATION
  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    av1_alloc_restoration_buffers(cm);
  }
#endif

#if !CONFIG_LOOPFILTER_LEVEL
  if (cm->lf.filter_level && !cm->skip_loop_filter) {
    av1_loop_filter_frame_init(cm, cm->lf.filter_level, cm->lf.filter_level);
  }
#endif

  // If encoded in frame parallel mode, frame context is ready after decoding
  // the frame header.
  if (cm->frame_parallel_decode && initialize_flag &&
      cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_BACKWARD) {
    AVxWorker *const worker = pbi->frame_worker_owner;
    FrameWorkerData *const frame_worker_data = worker->data1;
    if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_FORWARD) {
      context_updated = 1;
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
      cm->frame_contexts[cm->new_fb_idx] = *cm->fc;
#else
      cm->frame_contexts[cm->frame_context_idx] = *cm->fc;
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING
    }
    av1_frameworker_lock_stats(worker);
    pbi->cur_buf->row = -1;
    pbi->cur_buf->col = -1;
    frame_worker_data->frame_context_ready = 1;
    // Signal the main thread that context is ready.
    av1_frameworker_signal_stats(worker);
    av1_frameworker_unlock_stats(worker);
  }

  dec_setup_frame_boundary_info(cm);

  if (pbi->max_threads > 1 && !CONFIG_CB4X4 &&
#if CONFIG_EXT_TILE
      pbi->dec_tile_col < 0 &&  // Decoding all columns
#endif                          // CONFIG_EXT_TILE
      cm->tile_cols > 1) {
    // Multi-threaded tile decoder
    *p_data_end =
        decode_tiles_mt(pbi, data + pbi->first_partition_size, data_end);
    if (!xd->corrupted) {
      if (!cm->skip_loop_filter) {
// If multiple threads are used to decode tiles, then we use those
// threads to do parallel loopfiltering.
#if CONFIG_LOOPFILTER_LEVEL
        av1_loop_filter_frame_mt(
            (YV12_BUFFER_CONFIG *)xd->cur_buf, cm, pbi->mb.plane,
            cm->lf.filter_level[0], cm->lf.filter_level[1], 0, 0,
            pbi->tile_workers, pbi->num_tile_workers, &pbi->lf_row_sync);
#else
        av1_loop_filter_frame_mt((YV12_BUFFER_CONFIG *)xd->cur_buf, cm,
                                 pbi->mb.plane, cm->lf.filter_level, 0, 0,
                                 pbi->tile_workers, pbi->num_tile_workers,
                                 &pbi->lf_row_sync);
#endif  // CONFIG_LOOPFILTER_LEVEL
      }
    } else {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Decode failed. Frame data is corrupted.");
    }
  } else {
#if CONFIG_OBU
    *p_data_end = decode_tiles(pbi, data, data_end, startTile, endTile);
#else
    *p_data_end = decode_tiles(
        pbi, data + pbi->uncomp_hdr_size + pbi->first_partition_size, data_end,
        startTile, endTile);
#endif
  }

  if (endTile != cm->tile_rows * cm->tile_cols - 1) {
    return;
  }

#if CONFIG_STRIPED_LOOP_RESTORATION
  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    av1_loop_restoration_save_boundary_lines(&pbi->cur_buf->buf, cm);
  }
#endif

#if CONFIG_CDEF
  if (!cm->skip_loop_filter && !cm->all_lossless) {
    av1_cdef_frame(&pbi->cur_buf->buf, cm, &pbi->mb);
  }
#endif  // CONFIG_CDEF

#if CONFIG_FRAME_SUPERRES
  superres_post_decode(pbi);
#endif  // CONFIG_FRAME_SUPERRES

#if CONFIG_LOOP_RESTORATION
  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    aom_extend_frame_borders((YV12_BUFFER_CONFIG *)xd->cur_buf);
    av1_loop_restoration_frame((YV12_BUFFER_CONFIG *)xd->cur_buf, cm,
                               cm->rst_info, 7, 0, NULL);
  }
#endif  // CONFIG_LOOP_RESTORATION

  if (!xd->corrupted) {
    if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
      FRAME_CONTEXT **tile_ctxs = aom_malloc(cm->tile_rows * cm->tile_cols *
                                             sizeof(&pbi->tile_data[0].tctx));
      aom_cdf_prob **cdf_ptrs =
          aom_malloc(cm->tile_rows * cm->tile_cols *
                     sizeof(&pbi->tile_data[0].tctx.partition_cdf[0][0]));
      make_update_tile_list_dec(pbi, cm->tile_rows, cm->tile_cols, tile_ctxs);
#if CONFIG_LV_MAP
      av1_adapt_coef_probs(cm);
#endif  // CONFIG_LV_MAP
#if CONFIG_SYMBOLRATE
      av1_dump_symbol_rate(cm);
#endif
      av1_adapt_intra_frame_probs(cm);
      av1_average_tile_coef_cdfs(pbi->common.fc, tile_ctxs, cdf_ptrs,
                                 cm->tile_rows * cm->tile_cols);
      av1_average_tile_intra_cdfs(pbi->common.fc, tile_ctxs, cdf_ptrs,
                                  cm->tile_rows * cm->tile_cols);
#if CONFIG_PVQ
      av1_average_tile_pvq_cdfs(pbi->common.fc, tile_ctxs,
                                cm->tile_rows * cm->tile_cols);
#endif  // CONFIG_PVQ
#if CONFIG_ADAPT_SCAN
      av1_adapt_scan_order(cm);
#endif  // CONFIG_ADAPT_SCAN

      if (!frame_is_intra_only(cm)) {
        av1_adapt_inter_frame_probs(cm);
#if !CONFIG_NEW_MULTISYMBOL
        av1_adapt_mv_probs(cm, cm->allow_high_precision_mv);
#endif
        av1_average_tile_inter_cdfs(&pbi->common, pbi->common.fc, tile_ctxs,
                                    cdf_ptrs, cm->tile_rows * cm->tile_cols);
        av1_average_tile_mv_cdfs(pbi->common.fc, tile_ctxs, cdf_ptrs,
                                 cm->tile_rows * cm->tile_cols);
      }
      aom_free(tile_ctxs);
      aom_free(cdf_ptrs);
    } else {
      debug_check_frame_counts(cm);
    }
  } else {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Decode failed. Frame data is corrupted.");
  }

#if CONFIG_INSPECTION
  if (pbi->inspect_cb != NULL) {
    (*pbi->inspect_cb)(pbi, pbi->inspect_ctx);
  }
#endif

// Non frame parallel update frame context here.
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
  if (!context_updated) cm->frame_contexts[cm->new_fb_idx] = *cm->fc;
#else
  if (!cm->error_resilient_mode && !context_updated)
    cm->frame_contexts[cm->frame_context_idx] = *cm->fc;
#endif
}

#if CONFIG_OBU

static OBU_TYPE read_obu_header(struct aom_read_bit_buffer *rb,
                                uint32_t *header_size) {
  OBU_TYPE obu_type;
  int obu_extension_flag;

  *header_size = 1;

  obu_type = (OBU_TYPE)aom_rb_read_literal(rb, 5);
  aom_rb_read_literal(rb, 2);  // reserved
  obu_extension_flag = aom_rb_read_bit(rb);
  if (obu_extension_flag) {
    *header_size += 1;
    aom_rb_read_literal(rb, 3);  // temporal_id
    aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 1);  // reserved
  }

  return obu_type;
}

static uint32_t read_temporal_delimiter_obu() { return 0; }

static uint32_t read_sequence_header_obu(AV1Decoder *pbi,
                                         struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  SequenceHeader *const seq_params = &cm->seq_params;
  uint32_t saved_bit_offset = rb->bit_offset;

  cm->profile = av1_read_profile(rb);
  aom_rb_read_literal(rb, 4);  // level

  seq_params->frame_id_numbers_present_flag = aom_rb_read_bit(rb);
  if (seq_params->frame_id_numbers_present_flag) {
    seq_params->frame_id_length_minus7 = aom_rb_read_literal(rb, 4);
    seq_params->delta_frame_id_length_minus2 = aom_rb_read_literal(rb, 4);
  }

  read_bitdepth_colorspace_sampling(cm, rb, pbi->allow_lowbitdepth);

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_frame_header_obu(AV1Decoder *pbi, const uint8_t *data,
                                      const uint8_t *data_end,
                                      const uint8_t **p_data_end) {
  size_t header_size;

  header_size =
      av1_decode_frame_headers_and_setup(pbi, data, data_end, p_data_end);
  return (uint32_t)(pbi->uncomp_hdr_size + header_size);
}

static uint32_t read_tile_group_header(AV1Decoder *pbi,
                                       struct aom_read_bit_buffer *rb,
                                       int *startTile, int *endTile) {
  AV1_COMMON *const cm = &pbi->common;
  uint32_t saved_bit_offset = rb->bit_offset;

  *startTile = aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);
  *endTile = aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_one_tile_group_obu(AV1Decoder *pbi,
                                        struct aom_read_bit_buffer *rb,
                                        int is_first_tg, const uint8_t *data,
                                        const uint8_t *data_end,
                                        const uint8_t **p_data_end,
                                        int *is_last_tg) {
  AV1_COMMON *const cm = &pbi->common;
  int startTile, endTile;
  uint32_t header_size, tg_payload_size;

  header_size = read_tile_group_header(pbi, rb, &startTile, &endTile);
  data += header_size;
  av1_decode_tg_tiles_and_wrapup(pbi, data, data_end, p_data_end, startTile,
                                 endTile, is_first_tg);
  tg_payload_size = (uint32_t)(*p_data_end - data);

  // TODO(shan):  For now, assume all tile groups received in order
  *is_last_tg = endTile == cm->tile_rows * cm->tile_cols - 1;

  return header_size + tg_payload_size;
}

void av1_decode_frame_from_obus(struct AV1Decoder *pbi, const uint8_t *data,
                                const uint8_t *data_end,
                                const uint8_t **p_data_end) {
  AV1_COMMON *const cm = &pbi->common;
  int frame_decoding_finished = 0;
  int is_first_tg_obu_received = 1;
  int frame_header_received = 0;
  int frame_header_size = 0;

  // decode frame as a series of OBUs
  while (!frame_decoding_finished && !cm->error.error_code) {
    struct aom_read_bit_buffer rb;
    uint8_t clear_data[80];
    uint32_t obu_size, obu_header_size, obu_payload_size = 0;
    OBU_TYPE obu_type;

    init_read_bit_buffer(pbi, &rb, data + 4, data_end, clear_data);

    // every obu is preceded by 4-byte size of obu (obu header + payload size)
    // The obu size is only needed for tile group OBUs
    obu_size = mem_get_le32(data);
    obu_type = read_obu_header(&rb, &obu_header_size);
    data += (4 + obu_header_size);

    switch (obu_type) {
      case OBU_TD: obu_payload_size = read_temporal_delimiter_obu(); break;
      case OBU_SEQUENCE_HEADER:
        obu_payload_size = read_sequence_header_obu(pbi, &rb);
        break;
      case OBU_FRAME_HEADER:
        // Only decode first frame header received
        if (!frame_header_received) {
          frame_header_size = obu_payload_size =
              read_frame_header_obu(pbi, data, data_end, p_data_end);
          frame_header_received = 1;
        } else {
          obu_payload_size = frame_header_size;
        }
        if (cm->show_existing_frame) frame_decoding_finished = 1;
        break;
      case OBU_TILE_GROUP:
        obu_payload_size = read_one_tile_group_obu(
            pbi, &rb, is_first_tg_obu_received, data, data + obu_size - 1,
            p_data_end, &frame_decoding_finished);
        is_first_tg_obu_received = 0;
        break;
      default: break;
    }
    data += obu_payload_size;
  }
}
#endif
