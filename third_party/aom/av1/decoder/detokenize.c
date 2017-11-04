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

#include "./aom_config.h"
#if !CONFIG_PVQ
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#endif  // !CONFIG_PVQ

#include "av1/common/blockd.h"
#include "av1/decoder/detokenize.h"

#define ACCT_STR __func__

#if !CONFIG_PVQ || CONFIG_VAR_TX
#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/idct.h"
#endif

#include "av1/decoder/symbolrate.h"

#if !CONFIG_PVQ || CONFIG_VAR_TX
#define EOB_CONTEXT_NODE 0
#define ZERO_CONTEXT_NODE 1
#define ONE_CONTEXT_NODE 2
#define LOW_VAL_CONTEXT_NODE 0
#define TWO_CONTEXT_NODE 1
#define THREE_CONTEXT_NODE 2
#define HIGH_LOW_CONTEXT_NODE 3
#define CAT_ONE_CONTEXT_NODE 4
#define CAT_THREEFOUR_CONTEXT_NODE 5
#define CAT_THREE_CONTEXT_NODE 6
#define CAT_FIVE_CONTEXT_NODE 7

#define INCREMENT_COUNT(token)                   \
  do {                                           \
    if (counts) ++coef_counts[band][ctx][token]; \
  } while (0)

#if CONFIG_NEW_MULTISYMBOL
#define READ_COEFF(counts, prob_name, cdf_name, num, r) \
  read_coeff(counts, cdf_name, num, r);
static INLINE int read_coeff(FRAME_COUNTS *counts,
                             const aom_cdf_prob *const *cdf, int n,
                             aom_reader *r) {
#if !CONFIG_SYMBOLRATE
  (void)counts;
#endif
  int val = 0;
  int i = 0;
  int count = 0;
  while (count < n) {
    const int size = AOMMIN(n - count, 4);
    val |= av1_read_record_cdf(counts, r, cdf[i++], 1 << size, ACCT_STR)
           << count;
    count += size;
  }
  return val;
}
#else
#define READ_COEFF(counts, prob_name, cdf_name, num, r) \
  read_coeff(counts, prob_name, num, r);
static INLINE int read_coeff(FRAME_COUNTS *counts, const aom_prob *probs, int n,
                             aom_reader *r) {
#if !CONFIG_SYMBOLRATE
  (void)counts;
#endif
  int i, val = 0;
  for (i = 0; i < n; ++i)
    val = (val << 1) | av1_read_record(counts, r, probs[i], ACCT_STR);
  return val;
}

#endif

static int token_to_value(FRAME_COUNTS *counts, aom_reader *const r, int token,
                          TX_SIZE tx_size, int bit_depth) {
#if !CONFIG_HIGHBITDEPTH
  assert(bit_depth == 8);
#endif  // !CONFIG_HIGHBITDEPTH

  switch (token) {
    case ZERO_TOKEN:
    case ONE_TOKEN:
    case TWO_TOKEN:
    case THREE_TOKEN:
    case FOUR_TOKEN: return token;
    case CATEGORY1_TOKEN:
      return CAT1_MIN_VAL +
             READ_COEFF(counts, av1_cat1_prob, av1_cat1_cdf, 1, r);
    case CATEGORY2_TOKEN:
      return CAT2_MIN_VAL +
             READ_COEFF(counts, av1_cat2_prob, av1_cat2_cdf, 2, r);
    case CATEGORY3_TOKEN:
      return CAT3_MIN_VAL +
             READ_COEFF(counts, av1_cat3_prob, av1_cat3_cdf, 3, r);
    case CATEGORY4_TOKEN:
      return CAT4_MIN_VAL +
             READ_COEFF(counts, av1_cat4_prob, av1_cat4_cdf, 4, r);
    case CATEGORY5_TOKEN:
      return CAT5_MIN_VAL +
             READ_COEFF(counts, av1_cat5_prob, av1_cat5_cdf, 5, r);
    case CATEGORY6_TOKEN: {
      const int skip_bits = (int)sizeof(av1_cat6_prob) -
                            av1_get_cat6_extrabits_size(tx_size, bit_depth);
      return CAT6_MIN_VAL + READ_COEFF(counts, av1_cat6_prob + skip_bits,
                                       av1_cat6_cdf, 18 - skip_bits, r);
    }
    default:
      assert(0);  // Invalid token.
      return -1;
  }
}

static int decode_coefs(MACROBLOCKD *xd, PLANE_TYPE type, tran_low_t *dqcoeff,
                        TX_SIZE tx_size, TX_TYPE tx_type, const int16_t *dq,
#if CONFIG_NEW_QUANT
                        dequant_val_type_nuq *dq_val,
#else
#if CONFIG_AOM_QM
                        qm_val_t *iqm[2][TX_SIZES_ALL],
#endif  // CONFIG_AOM_QM
#endif  // CONFIG_NEW_QUANT
                        int ctx, const int16_t *scan, const int16_t *nb,
                        int16_t *max_scan_line, aom_reader *r) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const int max_eob = tx_size_2d[tx_size];
  const int ref = is_inter_block(&xd->mi[0]->mbmi);
#if CONFIG_AOM_QM && !CONFIG_NEW_QUANT
  const qm_val_t *iqmatrix = iqm[!ref][tx_size];
#endif  // CONFIG_AOM_QM
  (void)tx_type;
  int band, c = 0;
  const TX_SIZE tx_size_ctx = txsize_sqr_map[tx_size];
  aom_cdf_prob(*coef_head_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_head_cdfs[tx_size_ctx][type][ref];
  aom_cdf_prob(*coef_tail_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_tail_cdfs[tx_size_ctx][type][ref];
  int val = 0;

  uint8_t token_cache[MAX_TX_SQUARE];
  const uint8_t *band_translate = get_band_translate(tx_size);
  int dq_shift;
  int v, token;
  int32_t dqv = dq[0];
#if CONFIG_NEW_QUANT
  const tran_low_t *dqv_val = &dq_val[0][0];
#endif  // CONFIG_NEW_QUANT

  dq_shift = av1_get_tx_scale(tx_size);

  band = *band_translate++;

  int more_data = 1;
  while (more_data) {
    int comb_token;
    int last_pos = (c + 1 == max_eob);
    int first_pos = (c == 0);

#if CONFIG_NEW_QUANT
    dqv_val = &dq_val[band][0];
#endif  // CONFIG_NEW_QUANT

    comb_token = last_pos ? 2 * av1_read_record_bit(xd->counts, r, ACCT_STR) + 2
                          : av1_read_record_symbol(
                                xd->counts, r, coef_head_cdfs[band][ctx],
                                HEAD_TOKENS + first_pos, ACCT_STR) +
                                !first_pos;
    if (first_pos) {
      if (comb_token == 0) return 0;
    }
    token = comb_token >> 1;

    while (!token) {
      *max_scan_line = AOMMAX(*max_scan_line, scan[c]);
      token_cache[scan[c]] = 0;
#if CONFIG_SYMBOLRATE
      av1_record_coeff(xd->counts, 0);
#endif
      ++c;
      dqv = dq[1];
      ctx = get_coef_context(nb, token_cache, c);
      band = *band_translate++;

      last_pos = (c + 1 == max_eob);

      comb_token =
          last_pos
              ? 2 * av1_read_record_bit(xd->counts, r, ACCT_STR) + 2
              : av1_read_record_symbol(xd->counts, r, coef_head_cdfs[band][ctx],
                                       HEAD_TOKENS, ACCT_STR) +
                    1;
      token = comb_token >> 1;
    }

    more_data = comb_token & 1;

    if (token > ONE_TOKEN)
      token += av1_read_record_symbol(xd->counts, r, coef_tail_cdfs[band][ctx],
                                      TAIL_TOKENS, ACCT_STR);
#if CONFIG_NEW_QUANT
    dqv_val = &dq_val[band][0];
#endif  // CONFIG_NEW_QUANT

    *max_scan_line = AOMMAX(*max_scan_line, scan[c]);
    token_cache[scan[c]] = av1_pt_energy_class[token];

    val = token_to_value(xd->counts, r, token, tx_size, xd->bd);
#if CONFIG_SYMBOLRATE
    av1_record_coeff(xd->counts, val);
#endif

#if CONFIG_NEW_QUANT
    v = av1_dequant_abscoeff_nuq(val, dqv, dqv_val);
    v = dq_shift ? ROUND_POWER_OF_TWO(v, dq_shift) : v;
#else
#if CONFIG_AOM_QM
    // Apply quant matrix only for 2D transforms
    if (IS_2D_TRANSFORM(tx_type) && iqmatrix != NULL)
      dqv = ((iqmatrix[scan[c]] * (int)dqv) + (1 << (AOM_QM_BITS - 1))) >>
            AOM_QM_BITS;
#endif
    v = (val * dqv) >> dq_shift;
#endif

    v = (int)check_range(av1_read_record_bit(xd->counts, r, ACCT_STR) ? -v : v,
                         xd->bd);

    dqcoeff[scan[c]] = v;

    ++c;
    more_data &= (c < max_eob);
    if (!more_data) break;
    dqv = dq[1];
    ctx = get_coef_context(nb, token_cache, c);
    band = *band_translate++;
  }

  return c;
}
#endif  // !CONFIG_PVQ

static void decode_color_map_tokens(Av1ColorMapParam *param, aom_reader *r) {
  uint8_t color_order[PALETTE_MAX_SIZE];
  const int n = param->n_colors;
  uint8_t *const color_map = param->color_map;
  MapCdf color_map_cdf = param->map_cdf;
  int plane_block_width = param->plane_width;
  int plane_block_height = param->plane_height;
  int rows = param->rows;
  int cols = param->cols;

  // The first color index.
  color_map[0] = av1_read_uniform(r, n);
  assert(color_map[0] < n);

#if CONFIG_PALETTE_THROUGHPUT
  // Run wavefront on the palette map index decoding.
  for (int i = 1; i < rows + cols - 1; ++i) {
    for (int j = AOMMIN(i, cols - 1); j >= AOMMAX(0, i - rows + 1); --j) {
      const int color_ctx = av1_get_palette_color_index_context(
          color_map, plane_block_width, (i - j), j, n, color_order, NULL);
      const int color_idx = aom_read_symbol(
          r, color_map_cdf[n - PALETTE_MIN_SIZE][color_ctx], n, ACCT_STR);
      assert(color_idx >= 0 && color_idx < n);
      color_map[(i - j) * plane_block_width + j] = color_order[color_idx];
    }
  }
  // Copy last column to extra columns.
  if (cols < plane_block_width) {
    for (int i = 0; i < rows; ++i) {
      memset(color_map + i * plane_block_width + cols,
             color_map[i * plane_block_width + cols - 1],
             (plane_block_width - cols));
    }
  }
#else
  for (int i = 0; i < rows; ++i) {
    for (int j = (i == 0 ? 1 : 0); j < cols; ++j) {
      const int color_ctx = av1_get_palette_color_index_context(
          color_map, plane_block_width, i, j, n, color_order, NULL);
      const int color_idx = aom_read_symbol(
          r, color_map_cdf[n - PALETTE_MIN_SIZE][color_ctx], n, ACCT_STR);
      assert(color_idx >= 0 && color_idx < n);
      color_map[i * plane_block_width + j] = color_order[color_idx];
    }
    memset(color_map + i * plane_block_width + cols,
           color_map[i * plane_block_width + cols - 1],
           (plane_block_width - cols));  // Copy last column to extra columns.
  }
#endif  // CONFIG_PALETTE_THROUGHPUT
  // Copy last row to extra rows.
  for (int i = rows; i < plane_block_height; ++i) {
    memcpy(color_map + i * plane_block_width,
           color_map + (rows - 1) * plane_block_width, plane_block_width);
  }
}

static void get_palette_params(const MACROBLOCKD *const xd, int plane,
                               BLOCK_SIZE bsize, Av1ColorMapParam *params) {
  assert(plane == 0 || plane == 1);
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  params->color_map = xd->plane[plane].color_index_map;
  params->map_cdf = plane ? xd->tile_ctx->palette_uv_color_index_cdf
                          : xd->tile_ctx->palette_y_color_index_cdf;
  params->n_colors = pmi->palette_size[plane];
  av1_get_block_dimensions(bsize, plane, xd, &params->plane_width,
                           &params->plane_height, &params->rows, &params->cols);
}

#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
static void get_mrc_params(const MACROBLOCKD *const xd, TX_SIZE tx_size,
                           Av1ColorMapParam *params) {
  memset(params, 0, sizeof(*params));
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
  params->color_map = xd->mrc_mask;
  params->map_cdf = is_inter ? xd->tile_ctx->mrc_mask_inter_cdf
                             : xd->tile_ctx->mrc_mask_intra_cdf;
  params->n_colors = 2;
  params->plane_width = tx_size_wide[tx_size];
  params->rows = tx_size_high[tx_size];
  params->cols = tx_size_wide[tx_size];
}
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK

void av1_decode_palette_tokens(MACROBLOCKD *const xd, int plane,
                               aom_reader *r) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  assert(plane == 0 || plane == 1);
  assert(mbmi->sb_type >= BLOCK_8X8);
  Av1ColorMapParam color_map_params;
  memset(&color_map_params, 0, sizeof(color_map_params));
  get_palette_params(xd, plane, mbmi->sb_type, &color_map_params);
  decode_color_map_tokens(&color_map_params, r);
}

#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
static void decode_mrc_tokens(MACROBLOCKD *const xd, TX_TYPE tx_size,
                              aom_reader *r) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
  if ((is_inter && !SIGNAL_MRC_MASK_INTER) ||
      (!is_inter && !SIGNAL_MRC_MASK_INTRA))
    return;
  Av1ColorMapParam color_map_params;
  get_mrc_params(xd, tx_size, &color_map_params);
  decode_color_map_tokens(&color_map_params, r);
}
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK

#if !CONFIG_PVQ || CONFIG_VAR_TX
int av1_decode_block_tokens(AV1_COMMON *cm, MACROBLOCKD *const xd, int plane,
                            const SCAN_ORDER *sc, int x, int y, TX_SIZE tx_size,
                            TX_TYPE tx_type, int16_t *max_scan_line,
                            aom_reader *r, int seg_id) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int16_t *const dequant = pd->seg_dequant[seg_id];
  const int ctx =
      get_entropy_context(tx_size, pd->above_context + x, pd->left_context + y);
#if CONFIG_NEW_QUANT
  const int ref = is_inter_block(&xd->mi[0]->mbmi);
  int dq =
      get_dq_profile_from_ctx(xd->qindex[seg_id], ctx, ref, pd->plane_type);
#endif  //  CONFIG_NEW_QUANT

#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  if (tx_type == MRC_DCT) decode_mrc_tokens(xd, tx_size, r);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK

  const int eob =
      decode_coefs(xd, pd->plane_type, pd->dqcoeff, tx_size, tx_type, dequant,
#if CONFIG_NEW_QUANT
                   pd->seg_dequant_nuq[seg_id][dq],
#else
#if CONFIG_AOM_QM
                   pd->seg_iqmatrix[seg_id],
#endif  // CONFIG_AOM_QM
#endif  // CONFIG_NEW_QUANT
                   ctx, sc->scan, sc->neighbors, max_scan_line, r);
  av1_set_contexts(xd, pd, plane, tx_size, eob > 0, x, y);
#if CONFIG_ADAPT_SCAN
  if (xd->counts)
    av1_update_scan_count_facade(cm, xd->counts, tx_size, tx_type, pd->dqcoeff,
                                 eob);
#else
  (void)cm;
#endif
  return eob;
}
#endif  // !CONFIG_PVQ
