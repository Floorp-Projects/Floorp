/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>
#include <limits.h>

#include "vpx/vpx_encoder.h"
#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_entropymv.h"
#include "vp9/common/vp9_tile_common.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/common/vp9_pragmas.h"

#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_bitstream.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_subexp.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vp9/encoder/vp9_write_bit_buffer.h"

#ifdef ENTROPY_STATS
vp9_coeff_stats tree_update_hist[TX_SIZES][PLANE_TYPES];
extern unsigned int active_section;
#endif

static struct vp9_token intra_mode_encodings[INTRA_MODES];
static struct vp9_token switchable_interp_encodings[SWITCHABLE_FILTERS];
static struct vp9_token partition_encodings[PARTITION_TYPES];
static struct vp9_token inter_mode_encodings[INTER_MODES];

void vp9_entropy_mode_init() {
  vp9_tokens_from_tree(intra_mode_encodings, vp9_intra_mode_tree);
  vp9_tokens_from_tree(switchable_interp_encodings, vp9_switchable_interp_tree);
  vp9_tokens_from_tree(partition_encodings, vp9_partition_tree);
  vp9_tokens_from_tree(inter_mode_encodings, vp9_inter_mode_tree);
}

static void write_intra_mode(vp9_writer *w, MB_PREDICTION_MODE mode,
                             const vp9_prob *probs) {
  vp9_write_token(w, vp9_intra_mode_tree, probs, &intra_mode_encodings[mode]);
}

static void write_inter_mode(vp9_writer *w, MB_PREDICTION_MODE mode,
                             const vp9_prob *probs) {
  assert(is_inter_mode(mode));
  vp9_write_token(w, vp9_inter_mode_tree, probs,
                  &inter_mode_encodings[INTER_OFFSET(mode)]);
}

static INLINE void write_be32(uint8_t *p, int value) {
  p[0] = value >> 24;
  p[1] = value >> 16;
  p[2] = value >> 8;
  p[3] = value;
}

void vp9_encode_unsigned_max(struct vp9_write_bit_buffer *wb,
                             int data, int max) {
  vp9_wb_write_literal(wb, data, get_unsigned_bits(max));
}

static void prob_diff_update(const vp9_tree_index *tree,
                             vp9_prob probs[/*n - 1*/],
                             const unsigned int counts[/*n - 1*/],
                             int n, vp9_writer *w) {
  int i;
  unsigned int branch_ct[32][2];

  // Assuming max number of probabilities <= 32
  assert(n <= 32);

  vp9_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i)
    vp9_cond_prob_diff_update(w, &probs[i], branch_ct[i]);
}

static void write_selected_tx_size(const VP9_COMP *cpi, MODE_INFO *m,
                                   TX_SIZE tx_size, BLOCK_SIZE bsize,
                                   vp9_writer *w) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  const MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  const vp9_prob *const tx_probs = get_tx_probs2(max_tx_size, xd,
                                                 &cpi->common.fc.tx_probs);
  vp9_write(w, tx_size != TX_4X4, tx_probs[0]);
  if (tx_size != TX_4X4 && max_tx_size >= TX_16X16) {
    vp9_write(w, tx_size != TX_8X8, tx_probs[1]);
    if (tx_size != TX_8X8 && max_tx_size >= TX_32X32)
      vp9_write(w, tx_size != TX_16X16, tx_probs[2]);
  }
}

static int write_skip(const VP9_COMP *cpi, int segment_id, MODE_INFO *m,
                      vp9_writer *w) {
  const MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  if (vp9_segfeature_active(&cpi->common.seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int skip = m->mbmi.skip;
    vp9_write(w, skip, vp9_get_skip_prob(&cpi->common, xd));
    return skip;
  }
}

void vp9_update_skip_probs(VP9_COMMON *cm, vp9_writer *w) {
  int k;

  for (k = 0; k < SKIP_CONTEXTS; ++k)
    vp9_cond_prob_diff_update(w, &cm->fc.skip_probs[k], cm->counts.skip[k]);
}

static void update_switchable_interp_probs(VP9_COMP *cpi, vp9_writer *w) {
  VP9_COMMON *const cm = &cpi->common;
  int j;
  for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
    prob_diff_update(vp9_switchable_interp_tree,
                     cm->fc.switchable_interp_prob[j],
                     cm->counts.switchable_interp[j], SWITCHABLE_FILTERS, w);
}

static void pack_mb_tokens(vp9_writer* const w,
                           TOKENEXTRA **tp,
                           const TOKENEXTRA *const stop) {
  TOKENEXTRA *p = *tp;

  while (p < stop && p->token != EOSB_TOKEN) {
    const int t = p->token;
    const struct vp9_token *const a = &vp9_coef_encodings[t];
    const vp9_extra_bit *const b = &vp9_extra_bits[t];
    int i = 0;
    int v = a->value;
    int n = a->len;

    /* skip one or two nodes */
    if (p->skip_eob_node) {
      n -= p->skip_eob_node;
      i = 2 * p->skip_eob_node;
    }

    // TODO(jbb): expanding this can lead to big gains.  It allows
    // much better branch prediction and would enable us to avoid numerous
    // lookups and compares.

    // If we have a token that's in the constrained set, the coefficient tree
    // is split into two treed writes.  The first treed write takes care of the
    // unconstrained nodes.  The second treed write takes care of the
    // constrained nodes.
    if (t >= TWO_TOKEN && t < EOB_TOKEN) {
      int len = UNCONSTRAINED_NODES - p->skip_eob_node;
      int bits = v >> (n - len);
      vp9_write_tree(w, vp9_coef_tree, p->context_tree, bits, len, i);
      vp9_write_tree(w, vp9_coef_con_tree,
                     vp9_pareto8_full[p->context_tree[PIVOT_NODE] - 1],
                     v, n - len, 0);
    } else {
      vp9_write_tree(w, vp9_coef_tree, p->context_tree, v, n, i);
    }

    if (b->base_val) {
      const int e = p->extra, l = b->len;

      if (l) {
        const unsigned char *pb = b->prob;
        int v = e >> 1;
        int n = l;              /* number of bits in v, assumed nonzero */
        int i = 0;

        do {
          const int bb = (v >> --n) & 1;
          vp9_write(w, bb, pb[i >> 1]);
          i = b->tree[i + bb];
        } while (n);
      }

      vp9_write_bit(w, e & 1);
    }
    ++p;
  }

  *tp = p + (p->token == EOSB_TOKEN);
}

static void write_segment_id(vp9_writer *w, const struct segmentation *seg,
                             int segment_id) {
  if (seg->enabled && seg->update_map)
    vp9_write_tree(w, vp9_segment_tree, seg->tree_probs, segment_id, 3, 0);
}

// This function encodes the reference frame
static void encode_ref_frame(VP9_COMP *cpi, vp9_writer *bc) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mi = &xd->mi_8x8[0]->mbmi;
  const int segment_id = mi->segment_id;
  int seg_ref_active = vp9_segfeature_active(&cm->seg, segment_id,
                                             SEG_LVL_REF_FRAME);
  // If segment level coding of this signal is disabled...
  // or the segment allows multiple reference frame options
  if (!seg_ref_active) {
    // does the feature use compound prediction or not
    // (if not specified at the frame/segment level)
    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      vp9_write(bc, mi->ref_frame[1] > INTRA_FRAME,
                vp9_get_reference_mode_prob(cm, xd));
    } else {
      assert((mi->ref_frame[1] <= INTRA_FRAME) ==
             (cm->reference_mode == SINGLE_REFERENCE));
    }

    if (mi->ref_frame[1] > INTRA_FRAME) {
      vp9_write(bc, mi->ref_frame[0] == GOLDEN_FRAME,
                vp9_get_pred_prob_comp_ref_p(cm, xd));
    } else {
      vp9_write(bc, mi->ref_frame[0] != LAST_FRAME,
                vp9_get_pred_prob_single_ref_p1(cm, xd));
      if (mi->ref_frame[0] != LAST_FRAME)
        vp9_write(bc, mi->ref_frame[0] != GOLDEN_FRAME,
                  vp9_get_pred_prob_single_ref_p2(cm, xd));
    }
  } else {
    assert(mi->ref_frame[1] <= INTRA_FRAME);
    assert(vp9_get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME) ==
           mi->ref_frame[0]);
  }

  // If using the prediction model we have nothing further to do because
  // the reference frame is fully coded by the segment.
}

static void pack_inter_mode_mvs(VP9_COMP *cpi, MODE_INFO *m, vp9_writer *bc) {
  VP9_COMMON *const cm = &cpi->common;
  const nmv_context *nmvc = &cm->fc.nmvc;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct segmentation *seg = &cm->seg;
  MB_MODE_INFO *const mi = &m->mbmi;
  const MV_REFERENCE_FRAME rf = mi->ref_frame[0];
  const MV_REFERENCE_FRAME sec_rf = mi->ref_frame[1];
  const MB_PREDICTION_MODE mode = mi->mode;
  const int segment_id = mi->segment_id;
  int skip;
  const BLOCK_SIZE bsize = mi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;

#ifdef ENTROPY_STATS
  active_section = 9;
#endif

  if (seg->update_map) {
    if (seg->temporal_update) {
      const int pred_flag = mi->seg_id_predicted;
      vp9_prob pred_prob = vp9_get_pred_prob_seg_id(seg, xd);
      vp9_write(bc, pred_flag, pred_prob);
      if (!pred_flag)
        write_segment_id(bc, seg, segment_id);
    } else {
      write_segment_id(bc, seg, segment_id);
    }
  }

  skip = write_skip(cpi, segment_id, m, bc);

  if (!vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
    vp9_write(bc, rf != INTRA_FRAME, vp9_get_intra_inter_prob(cm, xd));

  if (bsize >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT &&
      !(rf != INTRA_FRAME &&
        (skip || vp9_segfeature_active(seg, segment_id, SEG_LVL_SKIP)))) {
    write_selected_tx_size(cpi, m, mi->tx_size, bsize, bc);
  }

  if (rf == INTRA_FRAME) {
#ifdef ENTROPY_STATS
    active_section = 6;
#endif

    if (bsize >= BLOCK_8X8) {
      write_intra_mode(bc, mode, cm->fc.y_mode_prob[size_group_lookup[bsize]]);
    } else {
      int idx, idy;
      const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
      for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
        for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
          const MB_PREDICTION_MODE bm = m->bmi[idy * 2 + idx].as_mode;
          write_intra_mode(bc, bm, cm->fc.y_mode_prob[0]);
        }
      }
    }
    write_intra_mode(bc, mi->uv_mode, cm->fc.uv_mode_prob[mode]);
  } else {
    vp9_prob *mv_ref_p;
    encode_ref_frame(cpi, bc);
    mv_ref_p = cpi->common.fc.inter_mode_probs[mi->mode_context[rf]];

#ifdef ENTROPY_STATS
    active_section = 3;
#endif

    // If segment skip is not enabled code the mode.
    if (!vp9_segfeature_active(seg, segment_id, SEG_LVL_SKIP)) {
      if (bsize >= BLOCK_8X8) {
        write_inter_mode(bc, mode, mv_ref_p);
        ++cm->counts.inter_mode[mi->mode_context[rf]][INTER_OFFSET(mode)];
      }
    }

    if (cm->interp_filter == SWITCHABLE) {
      const int ctx = vp9_get_pred_context_switchable_interp(xd);
      vp9_write_token(bc, vp9_switchable_interp_tree,
                      cm->fc.switchable_interp_prob[ctx],
                      &switchable_interp_encodings[mi->interp_filter]);
    } else {
      assert(mi->interp_filter == cm->interp_filter);
    }

    if (bsize < BLOCK_8X8) {
      const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
      int idx, idy;
      for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
        for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
          const int j = idy * 2 + idx;
          const MB_PREDICTION_MODE blockmode = m->bmi[j].as_mode;
          write_inter_mode(bc, blockmode, mv_ref_p);
          ++cm->counts.inter_mode[mi->mode_context[rf]]
                                 [INTER_OFFSET(blockmode)];

          if (blockmode == NEWMV) {
#ifdef ENTROPY_STATS
            active_section = 11;
#endif
            vp9_encode_mv(cpi, bc, &m->bmi[j].as_mv[0].as_mv,
                          &mi->ref_mvs[rf][0].as_mv, nmvc, allow_hp);

            if (has_second_ref(mi))
              vp9_encode_mv(cpi, bc, &m->bmi[j].as_mv[1].as_mv,
                            &mi->ref_mvs[sec_rf][0].as_mv, nmvc, allow_hp);
          }
        }
      }
    } else if (mode == NEWMV) {
#ifdef ENTROPY_STATS
      active_section = 5;
#endif
      vp9_encode_mv(cpi, bc, &mi->mv[0].as_mv,
                    &mi->ref_mvs[rf][0].as_mv, nmvc, allow_hp);

      if (has_second_ref(mi))
        vp9_encode_mv(cpi, bc, &mi->mv[1].as_mv,
                      &mi->ref_mvs[sec_rf][0].as_mv, nmvc, allow_hp);
    }
  }
}

static void write_mb_modes_kf(const VP9_COMP *cpi, MODE_INFO **mi_8x8,
                              vp9_writer *bc) {
  const VP9_COMMON *const cm = &cpi->common;
  const MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  const struct segmentation *const seg = &cm->seg;
  MODE_INFO *m = mi_8x8[0];
  const int ym = m->mbmi.mode;
  const int segment_id = m->mbmi.segment_id;
  MODE_INFO *above_mi = mi_8x8[-xd->mode_info_stride];
  MODE_INFO *left_mi = xd->left_available ? mi_8x8[-1] : NULL;

  if (seg->update_map)
    write_segment_id(bc, seg, m->mbmi.segment_id);

  write_skip(cpi, segment_id, m, bc);

  if (m->mbmi.sb_type >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT)
    write_selected_tx_size(cpi, m, m->mbmi.tx_size, m->mbmi.sb_type, bc);

  if (m->mbmi.sb_type >= BLOCK_8X8) {
    const MB_PREDICTION_MODE A = vp9_above_block_mode(m, above_mi, 0);
    const MB_PREDICTION_MODE L = vp9_left_block_mode(m, left_mi, 0);
    write_intra_mode(bc, ym, vp9_kf_y_mode_prob[A][L]);
  } else {
    int idx, idy;
    const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[m->mbmi.sb_type];
    const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[m->mbmi.sb_type];
    for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
      for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
        int i = idy * 2 + idx;
        const MB_PREDICTION_MODE A = vp9_above_block_mode(m, above_mi, i);
        const MB_PREDICTION_MODE L = vp9_left_block_mode(m, left_mi, i);
        const int bm = m->bmi[i].as_mode;
        write_intra_mode(bc, bm, vp9_kf_y_mode_prob[A][L]);
      }
    }
  }

  write_intra_mode(bc, m->mbmi.uv_mode, vp9_kf_uv_mode_prob[ym]);
}

static void write_modes_b(VP9_COMP *cpi, const TileInfo *const tile,
                          vp9_writer *w, TOKENEXTRA **tok, TOKENEXTRA *tok_end,
                          int mi_row, int mi_col) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  MODE_INFO *m;

  xd->mi_8x8 = cm->mi_grid_visible + (mi_row * cm->mode_info_stride + mi_col);
  m = xd->mi_8x8[0];

  set_mi_row_col(xd, tile,
                 mi_row, num_8x8_blocks_high_lookup[m->mbmi.sb_type],
                 mi_col, num_8x8_blocks_wide_lookup[m->mbmi.sb_type],
                 cm->mi_rows, cm->mi_cols);
  if (frame_is_intra_only(cm)) {
    write_mb_modes_kf(cpi, xd->mi_8x8, w);
#ifdef ENTROPY_STATS
    active_section = 8;
#endif
  } else {
    pack_inter_mode_mvs(cpi, m, w);
#ifdef ENTROPY_STATS
    active_section = 1;
#endif
  }

  assert(*tok < tok_end);
  pack_mb_tokens(w, tok, tok_end);
}

static void write_partition(VP9_COMP *cpi, int hbs, int mi_row, int mi_col,
                            PARTITION_TYPE p, BLOCK_SIZE bsize, vp9_writer *w) {
  VP9_COMMON *const cm = &cpi->common;
  const int ctx = partition_plane_context(cpi->above_seg_context,
                                          cpi->left_seg_context,
                                          mi_row, mi_col, bsize);
  const vp9_prob *const probs = get_partition_probs(cm, ctx);
  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;

  if (has_rows && has_cols) {
    vp9_write_token(w, vp9_partition_tree, probs, &partition_encodings[p]);
  } else if (!has_rows && has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_HORZ);
    vp9_write(w, p == PARTITION_SPLIT, probs[1]);
  } else if (has_rows && !has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_VERT);
    vp9_write(w, p == PARTITION_SPLIT, probs[2]);
  } else {
    assert(p == PARTITION_SPLIT);
  }
}

static void write_modes_sb(VP9_COMP *cpi, const TileInfo *const tile,
                           vp9_writer *w, TOKENEXTRA **tok, TOKENEXTRA *tok_end,
                           int mi_row, int mi_col, BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  const int bsl = b_width_log2(bsize);
  const int bs = (1 << bsl) / 4;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  MODE_INFO *m = cm->mi_grid_visible[mi_row * cm->mode_info_stride + mi_col];

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  partition = partition_lookup[bsl][m->mbmi.sb_type];
  write_partition(cpi, bs, mi_row, mi_col, partition, bsize, w);
  subsize = get_subsize(bsize, partition);
  if (subsize < BLOCK_8X8) {
    write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
  } else {
    switch (partition) {
      case PARTITION_NONE:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        break;
      case PARTITION_HORZ:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_row + bs < cm->mi_rows)
          write_modes_b(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col);
        break;
      case PARTITION_VERT:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_col + bs < cm->mi_cols)
          write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + bs);
        break;
      case PARTITION_SPLIT:
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col + bs,
                       subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col,
                       subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col + bs,
                       subsize);
        break;
      default:
        assert(0);
    }
  }

  // update partition context
  if (bsize >= BLOCK_8X8 &&
      (bsize == BLOCK_8X8 || partition != PARTITION_SPLIT))
    update_partition_context(cpi->above_seg_context, cpi->left_seg_context,
                             mi_row, mi_col, subsize, bsize);
}

static void write_modes(VP9_COMP *cpi, const TileInfo *const tile,
                        vp9_writer *w, TOKENEXTRA **tok, TOKENEXTRA *tok_end) {
  int mi_row, mi_col;

  for (mi_row = tile->mi_row_start; mi_row < tile->mi_row_end;
       mi_row += MI_BLOCK_SIZE) {
      vp9_zero(cpi->left_seg_context);
    for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
         mi_col += MI_BLOCK_SIZE)
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, BLOCK_64X64);
  }
}

static void build_tree_distribution(VP9_COMP *cpi, TX_SIZE tx_size) {
  vp9_coeff_probs_model *coef_probs = cpi->frame_coef_probs[tx_size];
  vp9_coeff_count *coef_counts = cpi->coef_counts[tx_size];
  unsigned int (*eob_branch_ct)[REF_TYPES][COEF_BANDS][COEFF_CONTEXTS] =
      cpi->common.counts.eob_branch[tx_size];
  vp9_coeff_stats *coef_branch_ct = cpi->frame_branch_ct[tx_size];
  int i, j, k, l, m;

  for (i = 0; i < PLANE_TYPES; ++i) {
    for (j = 0; j < REF_TYPES; ++j) {
      for (k = 0; k < COEF_BANDS; ++k) {
        for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
          vp9_tree_probs_from_distribution(vp9_coef_tree,
                                           coef_branch_ct[i][j][k][l],
                                           coef_counts[i][j][k][l]);
          coef_branch_ct[i][j][k][l][0][1] = eob_branch_ct[i][j][k][l] -
                                             coef_branch_ct[i][j][k][l][0][0];
          for (m = 0; m < UNCONSTRAINED_NODES; ++m)
            coef_probs[i][j][k][l][m] = get_binary_prob(
                                            coef_branch_ct[i][j][k][l][m][0],
                                            coef_branch_ct[i][j][k][l][m][1]);
#ifdef ENTROPY_STATS
          if (!cpi->dummy_packing) {
            int t;
            for (t = 0; t < ENTROPY_TOKENS; ++t)
              context_counters[tx_size][i][j][k][l][t] +=
                  coef_counts[i][j][k][l][t];
            context_counters[tx_size][i][j][k][l][ENTROPY_TOKENS] +=
                eob_branch_ct[i][j][k][l];
          }
#endif
        }
      }
    }
  }
}

static void update_coef_probs_common(vp9_writer* const bc, VP9_COMP *cpi,
                                     TX_SIZE tx_size) {
  vp9_coeff_probs_model *new_frame_coef_probs = cpi->frame_coef_probs[tx_size];
  vp9_coeff_probs_model *old_frame_coef_probs =
      cpi->common.fc.coef_probs[tx_size];
  vp9_coeff_stats *frame_branch_ct = cpi->frame_branch_ct[tx_size];
  const vp9_prob upd = DIFF_UPDATE_PROB;
  const int entropy_nodes_update = UNCONSTRAINED_NODES;
  int i, j, k, l, t;
  switch (cpi->sf.use_fast_coef_updates) {
    case 0: {
      /* dry run to see if there is any udpate at all needed */
      int savings = 0;
      int update[2] = {0, 0};
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              for (t = 0; t < entropy_nodes_update; ++t) {
                vp9_prob newp = new_frame_coef_probs[i][j][k][l][t];
                const vp9_prob oldp = old_frame_coef_probs[i][j][k][l][t];
                int s;
                int u = 0;
                if (t == PIVOT_NODE)
                  s = vp9_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_frame_coef_probs[i][j][k][l], &newp, upd, i, j);
                else
                  s = vp9_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], oldp, &newp, upd);
                if (s > 0 && newp != oldp)
                  u = 1;
                if (u)
                  savings += s - (int)(vp9_cost_zero(upd));
                else
                  savings -= (int)(vp9_cost_zero(upd));
                update[u]++;
              }
            }
          }
        }
      }

      // printf("Update %d %d, savings %d\n", update[0], update[1], savings);
      /* Is coef updated at all */
      if (update[1] == 0 || savings < 0) {
        vp9_write_bit(bc, 0);
        return;
      }
      vp9_write_bit(bc, 1);
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              // calc probs and branch cts for this frame only
              for (t = 0; t < entropy_nodes_update; ++t) {
                vp9_prob newp = new_frame_coef_probs[i][j][k][l][t];
                vp9_prob *oldp = old_frame_coef_probs[i][j][k][l] + t;
                const vp9_prob upd = DIFF_UPDATE_PROB;
                int s;
                int u = 0;
                if (t == PIVOT_NODE)
                  s = vp9_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_frame_coef_probs[i][j][k][l], &newp, upd, i, j);
                else
                  s = vp9_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t],
                      *oldp, &newp, upd);
                if (s > 0 && newp != *oldp)
                  u = 1;
                vp9_write(bc, u, upd);
#ifdef ENTROPY_STATS
                if (!cpi->dummy_packing)
                  ++tree_update_hist[tx_size][i][j][k][l][t][u];
#endif
                if (u) {
                  /* send/use new probability */
                  vp9_write_prob_diff_update(bc, newp, *oldp);
                  *oldp = newp;
                }
              }
            }
          }
        }
      }
      return;
    }

    case 1:
    case 2: {
      const int prev_coef_contexts_to_update =
          cpi->sf.use_fast_coef_updates == 2 ? COEFF_CONTEXTS >> 1
                                             : COEFF_CONTEXTS;
      const int coef_band_to_update =
          cpi->sf.use_fast_coef_updates == 2 ? COEF_BANDS >> 1
                                             : COEF_BANDS;
      int updates = 0;
      int noupdates_before_first = 0;
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              // calc probs and branch cts for this frame only
              for (t = 0; t < entropy_nodes_update; ++t) {
                vp9_prob newp = new_frame_coef_probs[i][j][k][l][t];
                vp9_prob *oldp = old_frame_coef_probs[i][j][k][l] + t;
                int s;
                int u = 0;
                if (l >= prev_coef_contexts_to_update ||
                    k >= coef_band_to_update) {
                  u = 0;
                } else {
                  if (t == PIVOT_NODE)
                    s = vp9_prob_diff_update_savings_search_model(
                        frame_branch_ct[i][j][k][l][0],
                        old_frame_coef_probs[i][j][k][l], &newp, upd, i, j);
                  else
                    s = vp9_prob_diff_update_savings_search(
                        frame_branch_ct[i][j][k][l][t],
                        *oldp, &newp, upd);
                  if (s > 0 && newp != *oldp)
                    u = 1;
                }
                updates += u;
                if (u == 0 && updates == 0) {
                  noupdates_before_first++;
#ifdef ENTROPY_STATS
                  if (!cpi->dummy_packing)
                    ++tree_update_hist[tx_size][i][j][k][l][t][u];
#endif
                  continue;
                }
                if (u == 1 && updates == 1) {
                  int v;
                  // first update
                  vp9_write_bit(bc, 1);
                  for (v = 0; v < noupdates_before_first; ++v)
                    vp9_write(bc, 0, upd);
                }
                vp9_write(bc, u, upd);
#ifdef ENTROPY_STATS
                if (!cpi->dummy_packing)
                  ++tree_update_hist[tx_size][i][j][k][l][t][u];
#endif
                if (u) {
                  /* send/use new probability */
                  vp9_write_prob_diff_update(bc, newp, *oldp);
                  *oldp = newp;
                }
              }
            }
          }
        }
      }
      if (updates == 0) {
        vp9_write_bit(bc, 0);  // no updates
      }
      return;
    }

    default:
      assert(0);
  }
}

static void update_coef_probs(VP9_COMP* cpi, vp9_writer* w) {
  const TX_MODE tx_mode = cpi->common.tx_mode;
  const TX_SIZE max_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  TX_SIZE tx_size;
  vp9_clear_system_state();

  for (tx_size = TX_4X4; tx_size <= TX_32X32; ++tx_size)
    build_tree_distribution(cpi, tx_size);

  for (tx_size = TX_4X4; tx_size <= max_tx_size; ++tx_size)
    update_coef_probs_common(w, cpi, tx_size);
}

static void encode_loopfilter(struct loopfilter *lf,
                              struct vp9_write_bit_buffer *wb) {
  int i;

  // Encode the loop filter level and type
  vp9_wb_write_literal(wb, lf->filter_level, 6);
  vp9_wb_write_literal(wb, lf->sharpness_level, 3);

  // Write out loop filter deltas applied at the MB level based on mode or
  // ref frame (if they are enabled).
  vp9_wb_write_bit(wb, lf->mode_ref_delta_enabled);

  if (lf->mode_ref_delta_enabled) {
    vp9_wb_write_bit(wb, lf->mode_ref_delta_update);
    if (lf->mode_ref_delta_update) {
      for (i = 0; i < MAX_REF_LF_DELTAS; i++) {
        const int delta = lf->ref_deltas[i];
        const int changed = delta != lf->last_ref_deltas[i];
        vp9_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_ref_deltas[i] = delta;
          vp9_wb_write_literal(wb, abs(delta) & 0x3F, 6);
          vp9_wb_write_bit(wb, delta < 0);
        }
      }

      for (i = 0; i < MAX_MODE_LF_DELTAS; i++) {
        const int delta = lf->mode_deltas[i];
        const int changed = delta != lf->last_mode_deltas[i];
        vp9_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_mode_deltas[i] = delta;
          vp9_wb_write_literal(wb, abs(delta) & 0x3F, 6);
          vp9_wb_write_bit(wb, delta < 0);
        }
      }
    }
  }
}

static void write_delta_q(struct vp9_write_bit_buffer *wb, int delta_q) {
  if (delta_q != 0) {
    vp9_wb_write_bit(wb, 1);
    vp9_wb_write_literal(wb, abs(delta_q), 4);
    vp9_wb_write_bit(wb, delta_q < 0);
  } else {
    vp9_wb_write_bit(wb, 0);
  }
}

static void encode_quantization(VP9_COMMON *cm,
                                struct vp9_write_bit_buffer *wb) {
  vp9_wb_write_literal(wb, cm->base_qindex, QINDEX_BITS);
  write_delta_q(wb, cm->y_dc_delta_q);
  write_delta_q(wb, cm->uv_dc_delta_q);
  write_delta_q(wb, cm->uv_ac_delta_q);
}


static void encode_segmentation(VP9_COMP *cpi,
                                struct vp9_write_bit_buffer *wb) {
  int i, j;

  struct segmentation *seg = &cpi->common.seg;

  vp9_wb_write_bit(wb, seg->enabled);
  if (!seg->enabled)
    return;

  // Segmentation map
  vp9_wb_write_bit(wb, seg->update_map);
  if (seg->update_map) {
    // Select the coding strategy (temporal or spatial)
    vp9_choose_segmap_coding_method(cpi);
    // Write out probabilities used to decode unpredicted  macro-block segments
    for (i = 0; i < SEG_TREE_PROBS; i++) {
      const int prob = seg->tree_probs[i];
      const int update = prob != MAX_PROB;
      vp9_wb_write_bit(wb, update);
      if (update)
        vp9_wb_write_literal(wb, prob, 8);
    }

    // Write out the chosen coding method.
    vp9_wb_write_bit(wb, seg->temporal_update);
    if (seg->temporal_update) {
      for (i = 0; i < PREDICTION_PROBS; i++) {
        const int prob = seg->pred_probs[i];
        const int update = prob != MAX_PROB;
        vp9_wb_write_bit(wb, update);
        if (update)
          vp9_wb_write_literal(wb, prob, 8);
      }
    }
  }

  // Segmentation data
  vp9_wb_write_bit(wb, seg->update_data);
  if (seg->update_data) {
    vp9_wb_write_bit(wb, seg->abs_delta);

    for (i = 0; i < MAX_SEGMENTS; i++) {
      for (j = 0; j < SEG_LVL_MAX; j++) {
        const int active = vp9_segfeature_active(seg, i, j);
        vp9_wb_write_bit(wb, active);
        if (active) {
          const int data = vp9_get_segdata(seg, i, j);
          const int data_max = vp9_seg_feature_data_max(j);

          if (vp9_is_segfeature_signed(j)) {
            vp9_encode_unsigned_max(wb, abs(data), data_max);
            vp9_wb_write_bit(wb, data < 0);
          } else {
            vp9_encode_unsigned_max(wb, data, data_max);
          }
        }
      }
    }
  }
}


static void encode_txfm_probs(VP9_COMP *cpi, vp9_writer *w) {
  VP9_COMMON *const cm = &cpi->common;

  // Mode
  vp9_write_literal(w, MIN(cm->tx_mode, ALLOW_32X32), 2);
  if (cm->tx_mode >= ALLOW_32X32)
    vp9_write_bit(w, cm->tx_mode == TX_MODE_SELECT);

  // Probabilities
  if (cm->tx_mode == TX_MODE_SELECT) {
    int i, j;
    unsigned int ct_8x8p[TX_SIZES - 3][2];
    unsigned int ct_16x16p[TX_SIZES - 2][2];
    unsigned int ct_32x32p[TX_SIZES - 1][2];


    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      tx_counts_to_branch_counts_8x8(cm->counts.tx.p8x8[i], ct_8x8p);
      for (j = 0; j < TX_SIZES - 3; j++)
        vp9_cond_prob_diff_update(w, &cm->fc.tx_probs.p8x8[i][j], ct_8x8p[j]);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      tx_counts_to_branch_counts_16x16(cm->counts.tx.p16x16[i], ct_16x16p);
      for (j = 0; j < TX_SIZES - 2; j++)
        vp9_cond_prob_diff_update(w, &cm->fc.tx_probs.p16x16[i][j],
                                  ct_16x16p[j]);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      tx_counts_to_branch_counts_32x32(cm->counts.tx.p32x32[i], ct_32x32p);
      for (j = 0; j < TX_SIZES - 1; j++)
        vp9_cond_prob_diff_update(w, &cm->fc.tx_probs.p32x32[i][j],
                                  ct_32x32p[j]);
    }
  }
}

static void write_interp_filter(INTERP_FILTER filter,
                                struct vp9_write_bit_buffer *wb) {
  const int filter_to_literal[] = { 1, 0, 2, 3 };

  vp9_wb_write_bit(wb, filter == SWITCHABLE);
  if (filter != SWITCHABLE)
    vp9_wb_write_literal(wb, filter_to_literal[filter], 2);
}

static void fix_interp_filter(VP9_COMMON *cm) {
  if (cm->interp_filter == SWITCHABLE) {
    // Check to see if only one of the filters is actually used
    int count[SWITCHABLE_FILTERS];
    int i, j, c = 0;
    for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
      count[i] = 0;
      for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
        count[i] += cm->counts.switchable_interp[j][i];
      c += (count[i] > 0);
    }
    if (c == 1) {
      // Only one filter is used. So set the filter at frame level
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        if (count[i]) {
          cm->interp_filter = i;
          break;
        }
      }
    }
  }
}

static void write_tile_info(VP9_COMMON *cm, struct vp9_write_bit_buffer *wb) {
  int min_log2_tile_cols, max_log2_tile_cols, ones;
  vp9_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  // columns
  ones = cm->log2_tile_cols - min_log2_tile_cols;
  while (ones--)
    vp9_wb_write_bit(wb, 1);

  if (cm->log2_tile_cols < max_log2_tile_cols)
    vp9_wb_write_bit(wb, 0);

  // rows
  vp9_wb_write_bit(wb, cm->log2_tile_rows != 0);
  if (cm->log2_tile_rows != 0)
    vp9_wb_write_bit(wb, cm->log2_tile_rows != 1);
}

static int get_refresh_mask(VP9_COMP *cpi) {
    // Should the GF or ARF be updated using the transmitted frame or buffer
#if CONFIG_MULTIPLE_ARF
    if (!cpi->multi_arf_enabled && cpi->refresh_golden_frame &&
        !cpi->refresh_alt_ref_frame) {
#else
    if (cpi->refresh_golden_frame && !cpi->refresh_alt_ref_frame &&
        !cpi->use_svc) {
#endif
      // Preserve the previously existing golden frame and update the frame in
      // the alt ref slot instead. This is highly specific to the use of
      // alt-ref as a forward reference, and this needs to be generalized as
      // other uses are implemented (like RTC/temporal scaling)
      //
      // gld_fb_idx and alt_fb_idx need to be swapped for future frames, but
      // that happens in vp9_onyx_if.c:update_reference_frames() so that it can
      // be done outside of the recode loop.
      return (cpi->refresh_last_frame << cpi->lst_fb_idx) |
             (cpi->refresh_golden_frame << cpi->alt_fb_idx);
    } else {
      int arf_idx = cpi->alt_fb_idx;
#if CONFIG_MULTIPLE_ARF
      // Determine which ARF buffer to use to encode this ARF frame.
      if (cpi->multi_arf_enabled) {
        int sn = cpi->sequence_number;
        arf_idx = (cpi->frame_coding_order[sn] < 0) ?
            cpi->arf_buffer_idx[sn + 1] :
            cpi->arf_buffer_idx[sn];
      }
#endif
      return (cpi->refresh_last_frame << cpi->lst_fb_idx) |
             (cpi->refresh_golden_frame << cpi->gld_fb_idx) |
             (cpi->refresh_alt_ref_frame << arf_idx);
    }
}

static size_t encode_tiles(VP9_COMP *cpi, uint8_t *data_ptr) {
  VP9_COMMON *const cm = &cpi->common;
  vp9_writer residual_bc;

  int tile_row, tile_col;
  TOKENEXTRA *tok[4][1 << 6], *tok_end;
  size_t total_size = 0;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;

  vpx_memset(cpi->above_seg_context, 0, sizeof(*cpi->above_seg_context) *
             mi_cols_aligned_to_sb(cm->mi_cols));

  tok[0][0] = cpi->tok;
  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    if (tile_row)
      tok[tile_row][0] = tok[tile_row - 1][tile_cols - 1] +
                         cpi->tok_count[tile_row - 1][tile_cols - 1];

    for (tile_col = 1; tile_col < tile_cols; tile_col++)
      tok[tile_row][tile_col] = tok[tile_row][tile_col - 1] +
                                cpi->tok_count[tile_row][tile_col - 1];
  }

  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      TileInfo tile;

      vp9_tile_init(&tile, cm, tile_row, tile_col);
      tok_end = tok[tile_row][tile_col] + cpi->tok_count[tile_row][tile_col];

      if (tile_col < tile_cols - 1 || tile_row < tile_rows - 1)
        vp9_start_encode(&residual_bc, data_ptr + total_size + 4);
      else
        vp9_start_encode(&residual_bc, data_ptr + total_size);

      write_modes(cpi, &tile, &residual_bc, &tok[tile_row][tile_col], tok_end);
      assert(tok[tile_row][tile_col] == tok_end);
      vp9_stop_encode(&residual_bc);
      if (tile_col < tile_cols - 1 || tile_row < tile_rows - 1) {
        // size of this tile
        write_be32(data_ptr + total_size, residual_bc.pos);
        total_size += 4;
      }

      total_size += residual_bc.pos;
    }
  }

  return total_size;
}

static void write_display_size(const VP9_COMMON *cm,
                               struct vp9_write_bit_buffer *wb) {
  const int scaling_active = cm->width != cm->display_width ||
                             cm->height != cm->display_height;
  vp9_wb_write_bit(wb, scaling_active);
  if (scaling_active) {
    vp9_wb_write_literal(wb, cm->display_width - 1, 16);
    vp9_wb_write_literal(wb, cm->display_height - 1, 16);
  }
}

static void write_frame_size(const VP9_COMMON *cm,
                             struct vp9_write_bit_buffer *wb) {
  vp9_wb_write_literal(wb, cm->width - 1, 16);
  vp9_wb_write_literal(wb, cm->height - 1, 16);

  write_display_size(cm, wb);
}

static void write_frame_size_with_refs(VP9_COMP *cpi,
                                       struct vp9_write_bit_buffer *wb) {
  VP9_COMMON *const cm = &cpi->common;
  int found = 0;

  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, ref_frame);
    found = cm->width == cfg->y_crop_width &&
            cm->height == cfg->y_crop_height;

    // TODO(ivan): This prevents a bug while more than 3 buffers are used. Do it
    // in a better way.
    if (cpi->use_svc) {
      found = 0;
    }
    vp9_wb_write_bit(wb, found);
    if (found) {
      break;
    }
  }

  if (!found) {
    vp9_wb_write_literal(wb, cm->width - 1, 16);
    vp9_wb_write_literal(wb, cm->height - 1, 16);
  }

  write_display_size(cm, wb);
}

static void write_sync_code(struct vp9_write_bit_buffer *wb) {
  vp9_wb_write_literal(wb, VP9_SYNC_CODE_0, 8);
  vp9_wb_write_literal(wb, VP9_SYNC_CODE_1, 8);
  vp9_wb_write_literal(wb, VP9_SYNC_CODE_2, 8);
}

static void write_uncompressed_header(VP9_COMP *cpi,
                                      struct vp9_write_bit_buffer *wb) {
  VP9_COMMON *const cm = &cpi->common;

  vp9_wb_write_literal(wb, VP9_FRAME_MARKER, 2);

  // bitstream version.
  // 00 - profile 0. 4:2:0 only
  // 10 - profile 1. adds 4:4:4, 4:2:2, alpha
  vp9_wb_write_bit(wb, cm->version);
  vp9_wb_write_bit(wb, 0);

  vp9_wb_write_bit(wb, 0);
  vp9_wb_write_bit(wb, cm->frame_type);
  vp9_wb_write_bit(wb, cm->show_frame);
  vp9_wb_write_bit(wb, cm->error_resilient_mode);

  if (cm->frame_type == KEY_FRAME) {
    const COLOR_SPACE cs = UNKNOWN;
    write_sync_code(wb);
    vp9_wb_write_literal(wb, cs, 3);
    if (cs != SRGB) {
      vp9_wb_write_bit(wb, 0);  // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
      if (cm->version == 1) {
        vp9_wb_write_bit(wb, cm->subsampling_x);
        vp9_wb_write_bit(wb, cm->subsampling_y);
        vp9_wb_write_bit(wb, 0);  // has extra plane
      }
    } else {
      assert(cm->version == 1);
      vp9_wb_write_bit(wb, 0);  // has extra plane
    }

    write_frame_size(cm, wb);
  } else {
    if (!cm->show_frame)
      vp9_wb_write_bit(wb, cm->intra_only);

    if (!cm->error_resilient_mode)
      vp9_wb_write_literal(wb, cm->reset_frame_context, 2);

    if (cm->intra_only) {
      write_sync_code(wb);

      vp9_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
      write_frame_size(cm, wb);
    } else {
      MV_REFERENCE_FRAME ref_frame;
      vp9_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
      for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
        vp9_wb_write_literal(wb, get_ref_frame_idx(cpi, ref_frame),
                             REF_FRAMES_LOG2);
        vp9_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
      }

      write_frame_size_with_refs(cpi, wb);

      vp9_wb_write_bit(wb, cm->allow_high_precision_mv);

      fix_interp_filter(cm);
      write_interp_filter(cm->interp_filter, wb);
    }
  }

  if (!cm->error_resilient_mode) {
    vp9_wb_write_bit(wb, cm->refresh_frame_context);
    vp9_wb_write_bit(wb, cm->frame_parallel_decoding_mode);
  }

  vp9_wb_write_literal(wb, cm->frame_context_idx, FRAME_CONTEXTS_LOG2);

  encode_loopfilter(&cm->lf, wb);
  encode_quantization(cm, wb);
  encode_segmentation(cpi, wb);

  write_tile_info(cm, wb);
}

static size_t write_compressed_header(VP9_COMP *cpi, uint8_t *data) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  FRAME_CONTEXT *const fc = &cm->fc;
  vp9_writer header_bc;

  vp9_start_encode(&header_bc, data);

  if (xd->lossless)
    cm->tx_mode = ONLY_4X4;
  else
    encode_txfm_probs(cpi, &header_bc);

  update_coef_probs(cpi, &header_bc);

#ifdef ENTROPY_STATS
  active_section = 2;
#endif

  vp9_update_skip_probs(cm, &header_bc);

  if (!frame_is_intra_only(cm)) {
    int i;
#ifdef ENTROPY_STATS
    active_section = 1;
#endif

    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      prob_diff_update(vp9_inter_mode_tree, cm->fc.inter_mode_probs[i],
                       cm->counts.inter_mode[i], INTER_MODES, &header_bc);

    vp9_zero(cm->counts.inter_mode);

    if (cm->interp_filter == SWITCHABLE)
      update_switchable_interp_probs(cpi, &header_bc);

    for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
      vp9_cond_prob_diff_update(&header_bc, &fc->intra_inter_prob[i],
                                cm->counts.intra_inter[i]);

    if (cm->allow_comp_inter_inter) {
      const int use_compound_pred = cm->reference_mode != SINGLE_REFERENCE;
      const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;

      vp9_write_bit(&header_bc, use_compound_pred);
      if (use_compound_pred) {
        vp9_write_bit(&header_bc, use_hybrid_pred);
        if (use_hybrid_pred)
          for (i = 0; i < COMP_INTER_CONTEXTS; i++)
            vp9_cond_prob_diff_update(&header_bc, &fc->comp_inter_prob[i],
                                      cm->counts.comp_inter[i]);
      }
    }

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      for (i = 0; i < REF_CONTEXTS; i++) {
        vp9_cond_prob_diff_update(&header_bc, &fc->single_ref_prob[i][0],
                                  cm->counts.single_ref[i][0]);
        vp9_cond_prob_diff_update(&header_bc, &fc->single_ref_prob[i][1],
                                  cm->counts.single_ref[i][1]);
      }
    }

    if (cm->reference_mode != SINGLE_REFERENCE)
      for (i = 0; i < REF_CONTEXTS; i++)
        vp9_cond_prob_diff_update(&header_bc, &fc->comp_ref_prob[i],
                                  cm->counts.comp_ref[i]);

    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
      prob_diff_update(vp9_intra_mode_tree, cm->fc.y_mode_prob[i],
                       cm->counts.y_mode[i], INTRA_MODES, &header_bc);

    for (i = 0; i < PARTITION_CONTEXTS; ++i)
      prob_diff_update(vp9_partition_tree, fc->partition_prob[i],
                       cm->counts.partition[i], PARTITION_TYPES, &header_bc);

    vp9_write_nmv_probs(cm, cm->allow_high_precision_mv, &header_bc);
  }

  vp9_stop_encode(&header_bc);
  assert(header_bc.pos <= 0xffff);

  return header_bc.pos;
}

void vp9_pack_bitstream(VP9_COMP *cpi, uint8_t *dest, size_t *size) {
  uint8_t *data = dest;
  size_t first_part_size;
  struct vp9_write_bit_buffer wb = {data, 0};
  struct vp9_write_bit_buffer saved_wb;

  write_uncompressed_header(cpi, &wb);
  saved_wb = wb;
  vp9_wb_write_literal(&wb, 0, 16);  // don't know in advance first part. size

  data += vp9_rb_bytes_written(&wb);

  vp9_compute_update_table();

#ifdef ENTROPY_STATS
  if (cm->frame_type == INTER_FRAME)
    active_section = 0;
  else
    active_section = 7;
#endif

  vp9_clear_system_state();  // __asm emms;

  first_part_size = write_compressed_header(cpi, data);
  data += first_part_size;
  vp9_wb_write_literal(&saved_wb, first_part_size, 16);

  data += encode_tiles(cpi, data);

  *size = data - dest;
}

