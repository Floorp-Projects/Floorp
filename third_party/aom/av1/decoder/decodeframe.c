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
#include <stddef.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_codec.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_reader.h"
#include "aom_dsp/bitreader.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/mem_ops.h"
#include "aom_scale/aom_scale.h"
#include "aom_util/aom_thread.h"

#if CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG

#include "av1/common/alloccommon.h"
#include "av1/common/cdef.h"
#include "av1/common/cfl.h"
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
#include "av1/common/resize.h"
#include "av1/common/seg_common.h"
#include "av1/common/thread_common.h"
#include "av1/common/tile_common.h"
#include "av1/common/warped_motion.h"
#include "av1/common/obmc.h"
#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decodemv.h"
#include "av1/decoder/decoder.h"
#include "av1/decoder/decodetxb.h"
#include "av1/decoder/detokenize.h"

#define ACCT_STR __func__

// This is needed by ext_tile related unit tests.
#define EXT_TILE_DEBUG 1
#define MC_TEMP_BUF_PELS                       \
  (((MAX_SB_SIZE)*2 + (AOM_INTERP_EXTEND)*2) * \
   ((MAX_SB_SIZE)*2 + (AOM_INTERP_EXTEND)*2))

// Checks that the remaining bits start with a 1 and ends with 0s.
// It consumes an additional byte, if already byte aligned before the check.
int av1_check_trailing_bits(AV1Decoder *pbi, struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  // bit_offset is set to 0 (mod 8) when the reader is already byte aligned
  int bits_before_alignment = 8 - rb->bit_offset % 8;
  int trailing = aom_rb_read_literal(rb, bits_before_alignment);
  if (trailing != (1 << (bits_before_alignment - 1))) {
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return -1;
  }
  return 0;
}

// Use only_chroma = 1 to only set the chroma planes
static void set_planes_to_neutral_grey(AV1_COMMON *const cm,
                                       const YV12_BUFFER_CONFIG *const buf,
                                       int only_chroma) {
  const int val = 1 << (cm->bit_depth - 1);

  for (int plane = only_chroma; plane < MAX_MB_PLANE; plane++) {
    const int is_uv = plane > 0;
    for (int row_idx = 0; row_idx < buf->crop_heights[is_uv]; row_idx++) {
      if (cm->use_highbitdepth) {
        // TODO(yaowu): replace this with aom_memset16() for speed
        for (int col_idx = 0; col_idx < buf->crop_widths[is_uv]; col_idx++) {
          uint16_t *base = CONVERT_TO_SHORTPTR(buf->buffers[plane]);
          base[row_idx * buf->strides[is_uv] + col_idx] = val;
        }
      } else {
        memset(&buf->buffers[plane][row_idx * buf->uv_stride], 1 << 7,
               buf->crop_widths[is_uv]);
      }
    }
  }
}

static void loop_restoration_read_sb_coeffs(const AV1_COMMON *const cm,
                                            MACROBLOCKD *xd,
                                            aom_reader *const r, int plane,
                                            int runit_idx);

static void setup_compound_reference_mode(AV1_COMMON *cm) {
  cm->comp_fwd_ref[0] = LAST_FRAME;
  cm->comp_fwd_ref[1] = LAST2_FRAME;
  cm->comp_fwd_ref[2] = LAST3_FRAME;
  cm->comp_fwd_ref[3] = GOLDEN_FRAME;

  cm->comp_bwd_ref[0] = BWDREF_FRAME;
  cm->comp_bwd_ref[1] = ALTREF2_FRAME;
  cm->comp_bwd_ref[2] = ALTREF_FRAME;
}

static int read_is_valid(const uint8_t *start, size_t len, const uint8_t *end) {
  return len != 0 && len <= (size_t)(end - start);
}

static TX_MODE read_tx_mode(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  if (cm->coded_lossless) return ONLY_4X4;
  return aom_rb_read_bit(rb) ? TX_MODE_SELECT : TX_MODE_LARGEST;
}

static REFERENCE_MODE read_frame_reference_mode(
    const AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  if (frame_is_intra_only(cm)) {
    return SINGLE_REFERENCE;
  } else {
    return aom_rb_read_bit(rb) ? REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
  }
}

static void inverse_transform_block(MACROBLOCKD *xd, int plane,
                                    const TX_TYPE tx_type,
                                    const TX_SIZE tx_size, uint8_t *dst,
                                    int stride, int reduced_tx_set) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *const dqcoeff = pd->dqcoeff;
  eob_info *eob_data = pd->eob_data + xd->txb_offset[plane];
  uint16_t scan_line = eob_data->max_scan_line;
  uint16_t eob = eob_data->eob;

  memcpy(dqcoeff, pd->dqcoeff_block + xd->cb_offset[plane],
         (scan_line + 1) * sizeof(dqcoeff[0]));
  av1_inverse_transform_block(xd, dqcoeff, plane, tx_type, tx_size, dst, stride,
                              eob, reduced_tx_set);
  memset(dqcoeff, 0, (scan_line + 1) * sizeof(dqcoeff[0]));
}

static void read_coeffs_tx_intra_block(AV1_COMMON *cm, MACROBLOCKD *const xd,
                                       aom_reader *const r, int plane, int row,
                                       int col, TX_SIZE tx_size) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  if (!mbmi->skip) {
#if TXCOEFF_TIMER
    struct aom_usec_timer timer;
    aom_usec_timer_start(&timer);
#endif
    av1_read_coeffs_txb_facade(cm, xd, r, row, col, plane, tx_size);
#if TXCOEFF_TIMER
    aom_usec_timer_mark(&timer);
    const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);
    cm->txcoeff_timer += elapsed_time;
    ++cm->txb_count;
#endif
  }
}

static void predict_and_reconstruct_intra_block(AV1_COMMON *cm,
                                                MACROBLOCKD *const xd,
                                                aom_reader *const r, int plane,
                                                int row, int col,
                                                TX_SIZE tx_size) {
  (void)r;
  MB_MODE_INFO *mbmi = xd->mi[0];
  PLANE_TYPE plane_type = get_plane_type(plane);

  av1_predict_intra_block_facade(cm, xd, plane, col, row, tx_size);

  if (!mbmi->skip) {
    struct macroblockd_plane *const pd = &xd->plane[plane];

    // tx_type will be read out in av1_read_coeffs_txb_facade
    const TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, row, col, tx_size,
                                            cm->reduced_tx_set_used);
    eob_info *eob_data = pd->eob_data + xd->txb_offset[plane];
    if (eob_data->eob) {
      uint8_t *dst =
          &pd->dst.buf[(row * pd->dst.stride + col) << tx_size_wide_log2[0]];
      inverse_transform_block(xd, plane, tx_type, tx_size, dst, pd->dst.stride,
                              cm->reduced_tx_set_used);
    }
  }
  if (plane == AOM_PLANE_Y && store_cfl_required(cm, xd)) {
    cfl_store_tx(xd, row, col, tx_size, mbmi->sb_type);
  }
}

static void inverse_transform_inter_block(const AV1_COMMON *const cm,
                                          MACROBLOCKD *const xd,
                                          aom_reader *const r,
                                          const int blk_row, const int blk_col,
                                          const int plane,
                                          const TX_SIZE tx_size) {
  (void)r;
  PLANE_TYPE plane_type = get_plane_type(plane);
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = xd->mi[0];

  // tx_type will be read out in av1_read_coeffs_txb_facade
  const TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, blk_row, blk_col,
                                          tx_size, cm->reduced_tx_set_used);

  if (plane == 0)
    update_txk_array(mbmi->txk_type, mbmi->sb_type, blk_row, blk_col, tx_size,
                     tx_type);

  uint8_t *dst =
      &pd->dst
           .buf[(blk_row * pd->dst.stride + blk_col) << tx_size_wide_log2[0]];
  inverse_transform_block(xd, plane, tx_type, tx_size, dst, pd->dst.stride,
                          cm->reduced_tx_set_used);
}

static void set_cb_buffer_offsets(MACROBLOCKD *const xd, TX_SIZE tx_size,
                                  int plane) {
  xd->cb_offset[plane] += tx_size_wide[tx_size] * tx_size_high[tx_size];
  xd->txb_offset[plane] =
      xd->cb_offset[plane] / (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
}

static void decode_reconstruct_tx(AV1_COMMON *cm, MACROBLOCKD *const xd,
                                  aom_reader *r, MB_MODE_INFO *const mbmi,
                                  int plane, BLOCK_SIZE plane_bsize,
                                  int blk_row, int blk_col, int block,
                                  TX_SIZE tx_size, int *eob_total) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const TX_SIZE plane_tx_size =
      plane ? av1_get_max_uv_txsize(mbmi->sb_type, pd->subsampling_x,
                                    pd->subsampling_y)
            : mbmi->inter_tx_size[av1_get_txb_size_index(plane_bsize, blk_row,
                                                         blk_col)];
  // Scale to match transform block unit.
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size || plane) {
#if TXCOEFF_TIMER
    struct aom_usec_timer timer;
    aom_usec_timer_start(&timer);
#endif
    av1_read_coeffs_txb_facade(cm, xd, r, blk_row, blk_col, plane, tx_size);
#if TXCOEFF_TIMER
    aom_usec_timer_mark(&timer);
    const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);
    cm->txcoeff_timer += elapsed_time;
    ++cm->txb_count;
#endif
    inverse_transform_inter_block(cm, xd, r, blk_row, blk_col, plane, tx_size);

#if CONFIG_MISMATCH_DEBUG
    int pixel_c, pixel_r;
    BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
    int blk_w = block_size_wide[bsize];
    int blk_h = block_size_high[bsize];
    mi_to_pixel_loc(&pixel_c, &pixel_r, mi_col, mi_row, blk_col, blk_row,
                    pd->subsampling_x, pd->subsampling_y);
    mismatch_check_block_tx(dst, pd->dst.stride, cm->frame_offset, plane,
                            pixel_c, pixel_r, blk_w, blk_h,
                            xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
#endif
    eob_info *eob_data = pd->eob_data + xd->txb_offset[plane];
    *eob_total += eob_data->eob;
    set_cb_buffer_offsets(xd, tx_size, plane);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    assert(IMPLIES(tx_size <= TX_4X4, sub_txs == tx_size));
    assert(IMPLIES(tx_size > TX_4X4, sub_txs < tx_size));
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    const int sub_step = bsw * bsh;

    assert(bsw > 0 && bsh > 0);

    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        const int offsetr = blk_row + row;
        const int offsetc = blk_col + col;

        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

        decode_reconstruct_tx(cm, xd, r, mbmi, plane, plane_bsize, offsetr,
                              offsetc, block, sub_txs, eob_total);
        block += sub_step;
      }
    }
  }
}

static void set_offsets(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                        BLOCK_SIZE bsize, int mi_row, int mi_col, int bw,
                        int bh, int x_mis, int y_mis) {
  const int num_planes = av1_num_planes(cm);

  const int offset = mi_row * cm->mi_stride + mi_col;
  const TileInfo *const tile = &xd->tile;

  xd->mi = cm->mi_grid_visible + offset;
  xd->mi[0] = &cm->mi[offset];
  // TODO(slavarnway): Generate sb_type based on bwl and bhl, instead of
  // passing bsize from decode_partition().
  xd->mi[0]->sb_type = bsize;
#if CONFIG_RD_DEBUG
  xd->mi[0]->mi_row = mi_row;
  xd->mi[0]->mi_col = mi_col;
#endif
  xd->cfl.mi_row = mi_row;
  xd->cfl.mi_col = mi_col;

  assert(x_mis && y_mis);
  for (int x = 1; x < x_mis; ++x) xd->mi[x] = xd->mi[0];
  int idx = cm->mi_stride;
  for (int y = 1; y < y_mis; ++y) {
    memcpy(&xd->mi[idx], &xd->mi[0], x_mis * sizeof(xd->mi[0]));
    idx += cm->mi_stride;
  }

  set_plane_n4(xd, bw, bh, num_planes);
  set_skip_context(xd, mi_row, mi_col, num_planes);

  // Distance of Mb to the various image edges. These are specified to 8th pel
  // as they are always compared to values that are in 1/8th pel units
  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw, cm->mi_rows, cm->mi_cols);

  av1_setup_dst_planes(xd->plane, bsize, get_frame_new_buffer(cm), mi_row,
                       mi_col, 0, num_planes);
}

static void decode_mbmi_block(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                              int mi_row, int mi_col, aom_reader *r,
                              PARTITION_TYPE partition, BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);

#if CONFIG_ACCOUNTING
  aom_accounting_set_context(&pbi->accounting, mi_col, mi_row);
#endif
  set_offsets(cm, xd, bsize, mi_row, mi_col, bw, bh, x_mis, y_mis);
  xd->mi[0]->partition = partition;
  av1_read_mode_info(pbi, xd, mi_row, mi_col, r, x_mis, y_mis);
  if (bsize >= BLOCK_8X8 && (cm->subsampling_x || cm->subsampling_y)) {
    const BLOCK_SIZE uv_subsize =
        ss_size_lookup[bsize][cm->subsampling_x][cm->subsampling_y];
    if (uv_subsize == BLOCK_INVALID)
      aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid block size.");
  }

  int reader_corrupted_flag = aom_reader_has_error(r);
  aom_merge_corrupted_flag(&xd->corrupted, reader_corrupted_flag);
}

typedef struct PadBlock {
  int x0;
  int x1;
  int y0;
  int y1;
} PadBlock;

static void highbd_build_mc_border(const uint8_t *src8, int src_stride,
                                   uint8_t *dst8, int dst_stride, int x, int y,
                                   int b_w, int b_h, int w, int h) {
  // Get a pointer to the start of the real data for this row.
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const uint16_t *ref_row = src - x - y * src_stride;

  if (y >= h)
    ref_row += (h - 1) * src_stride;
  else if (y > 0)
    ref_row += y * src_stride;

  do {
    int right = 0, copy;
    int left = x < 0 ? -x : 0;

    if (left > b_w) left = b_w;

    if (x + b_w > w) right = x + b_w - w;

    if (right > b_w) right = b_w;

    copy = b_w - left - right;

    if (left) aom_memset16(dst, ref_row[0], left);

    if (copy) memcpy(dst + left, ref_row + x + left, copy * sizeof(uint16_t));

    if (right) aom_memset16(dst + left + copy, ref_row[w - 1], right);

    dst += dst_stride;
    ++y;

    if (y > 0 && y < h) ref_row += src_stride;
  } while (--b_h);
}

static void build_mc_border(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int x, int y, int b_w, int b_h,
                            int w, int h) {
  // Get a pointer to the start of the real data for this row.
  const uint8_t *ref_row = src - x - y * src_stride;

  if (y >= h)
    ref_row += (h - 1) * src_stride;
  else if (y > 0)
    ref_row += y * src_stride;

  do {
    int right = 0, copy;
    int left = x < 0 ? -x : 0;

    if (left > b_w) left = b_w;

    if (x + b_w > w) right = x + b_w - w;

    if (right > b_w) right = b_w;

    copy = b_w - left - right;

    if (left) memset(dst, ref_row[0], left);

    if (copy) memcpy(dst + left, ref_row + x + left, copy);

    if (right) memset(dst + left + copy, ref_row[w - 1], right);

    dst += dst_stride;
    ++y;

    if (y > 0 && y < h) ref_row += src_stride;
  } while (--b_h);
}

static INLINE int update_extend_mc_border_params(
    const struct scale_factors *const sf, struct buf_2d *const pre_buf,
    MV32 scaled_mv, PadBlock *block, int subpel_x_mv, int subpel_y_mv,
    int do_warp, int is_intrabc, int *x_pad, int *y_pad) {
  const int is_scaled = av1_is_scaled(sf);
  // Get reference width and height.
  int frame_width = pre_buf->width;
  int frame_height = pre_buf->height;

  // Do border extension if there is motion or
  // width/height is not a multiple of 8 pixels.
  if ((!is_intrabc) && (!do_warp) &&
      (is_scaled || scaled_mv.col || scaled_mv.row || (frame_width & 0x7) ||
       (frame_height & 0x7))) {
    if (subpel_x_mv || (sf->x_step_q4 != SUBPEL_SHIFTS)) {
      block->x0 -= AOM_INTERP_EXTEND - 1;
      block->x1 += AOM_INTERP_EXTEND;
      *x_pad = 1;
    }

    if (subpel_y_mv || (sf->y_step_q4 != SUBPEL_SHIFTS)) {
      block->y0 -= AOM_INTERP_EXTEND - 1;
      block->y1 += AOM_INTERP_EXTEND;
      *y_pad = 1;
    }

    // Skip border extension if block is inside the frame.
    if (block->x0 < 0 || block->x1 > frame_width - 1 || block->y0 < 0 ||
        block->y1 > frame_height - 1) {
      return 1;
    }
  }
  return 0;
}

static INLINE void extend_mc_border(const struct scale_factors *const sf,
                                    struct buf_2d *const pre_buf,
                                    MV32 scaled_mv, PadBlock block,
                                    int subpel_x_mv, int subpel_y_mv,
                                    int do_warp, int is_intrabc, int highbd,
                                    uint8_t *mc_buf, uint8_t **pre,
                                    int *src_stride) {
  int x_pad = 0, y_pad = 0;
  if (update_extend_mc_border_params(sf, pre_buf, scaled_mv, &block,
                                     subpel_x_mv, subpel_y_mv, do_warp,
                                     is_intrabc, &x_pad, &y_pad)) {
    // Get reference block pointer.
    const uint8_t *const buf_ptr =
        pre_buf->buf0 + block.y0 * pre_buf->stride + block.x0;
    int buf_stride = pre_buf->stride;
    const int b_w = block.x1 - block.x0;
    const int b_h = block.y1 - block.y0;

    // Extend the border.
    if (highbd) {
      highbd_build_mc_border(buf_ptr, buf_stride, mc_buf, b_w, block.x0,
                             block.y0, b_w, b_h, pre_buf->width,
                             pre_buf->height);
    } else {
      build_mc_border(buf_ptr, buf_stride, mc_buf, b_w, block.x0, block.y0, b_w,
                      b_h, pre_buf->width, pre_buf->height);
    }
    *src_stride = b_w;
    *pre = mc_buf + y_pad * (AOM_INTERP_EXTEND - 1) * b_w +
           x_pad * (AOM_INTERP_EXTEND - 1);
  }
}

static INLINE void dec_calc_subpel_params(
    MACROBLOCKD *xd, const struct scale_factors *const sf, const MV mv,
    int plane, const int pre_x, const int pre_y, int x, int y,
    struct buf_2d *const pre_buf, SubpelParams *subpel_params, int bw, int bh,
    PadBlock *block, int mi_x, int mi_y, MV32 *scaled_mv, int *subpel_x_mv,
    int *subpel_y_mv) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int is_scaled = av1_is_scaled(sf);
  if (is_scaled) {
    int ssx = pd->subsampling_x;
    int ssy = pd->subsampling_y;
    int orig_pos_y = (pre_y + y) << SUBPEL_BITS;
    orig_pos_y += mv.row * (1 << (1 - ssy));
    int orig_pos_x = (pre_x + x) << SUBPEL_BITS;
    orig_pos_x += mv.col * (1 << (1 - ssx));
    int pos_y = sf->scale_value_y(orig_pos_y, sf);
    int pos_x = sf->scale_value_x(orig_pos_x, sf);
    pos_x += SCALE_EXTRA_OFF;
    pos_y += SCALE_EXTRA_OFF;

    const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
    const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
    const int bottom = (pre_buf->height + AOM_INTERP_EXTEND)
                       << SCALE_SUBPEL_BITS;
    const int right = (pre_buf->width + AOM_INTERP_EXTEND) << SCALE_SUBPEL_BITS;
    pos_y = clamp(pos_y, top, bottom);
    pos_x = clamp(pos_x, left, right);

    subpel_params->subpel_x = pos_x & SCALE_SUBPEL_MASK;
    subpel_params->subpel_y = pos_y & SCALE_SUBPEL_MASK;
    subpel_params->xs = sf->x_step_q4;
    subpel_params->ys = sf->y_step_q4;

    // Get reference block top left coordinate.
    block->x0 = pos_x >> SCALE_SUBPEL_BITS;
    block->y0 = pos_y >> SCALE_SUBPEL_BITS;

    // Get reference block bottom right coordinate.
    block->x1 =
        ((pos_x + (bw - 1) * subpel_params->xs) >> SCALE_SUBPEL_BITS) + 1;
    block->y1 =
        ((pos_y + (bh - 1) * subpel_params->ys) >> SCALE_SUBPEL_BITS) + 1;

    MV temp_mv;
    temp_mv = clamp_mv_to_umv_border_sb(xd, &mv, bw, bh, pd->subsampling_x,
                                        pd->subsampling_y);
    *scaled_mv = av1_scale_mv(&temp_mv, (mi_x + x), (mi_y + y), sf);
    scaled_mv->row += SCALE_EXTRA_OFF;
    scaled_mv->col += SCALE_EXTRA_OFF;

    *subpel_x_mv = scaled_mv->col & SCALE_SUBPEL_MASK;
    *subpel_y_mv = scaled_mv->row & SCALE_SUBPEL_MASK;
  } else {
    // Get block position in current frame.
    int pos_x = (pre_x + x) << SUBPEL_BITS;
    int pos_y = (pre_y + y) << SUBPEL_BITS;

    const MV mv_q4 = clamp_mv_to_umv_border_sb(
        xd, &mv, bw, bh, pd->subsampling_x, pd->subsampling_y);
    subpel_params->xs = subpel_params->ys = SCALE_SUBPEL_SHIFTS;
    subpel_params->subpel_x = (mv_q4.col & SUBPEL_MASK) << SCALE_EXTRA_BITS;
    subpel_params->subpel_y = (mv_q4.row & SUBPEL_MASK) << SCALE_EXTRA_BITS;

    // Get reference block top left coordinate.
    pos_x += mv_q4.col;
    pos_y += mv_q4.row;
    block->x0 = pos_x >> SUBPEL_BITS;
    block->y0 = pos_y >> SUBPEL_BITS;

    // Get reference block bottom right coordinate.
    block->x1 = (pos_x >> SUBPEL_BITS) + (bw - 1) + 1;
    block->y1 = (pos_y >> SUBPEL_BITS) + (bh - 1) + 1;

    scaled_mv->row = mv_q4.row;
    scaled_mv->col = mv_q4.col;
    *subpel_x_mv = scaled_mv->col & SUBPEL_MASK;
    *subpel_y_mv = scaled_mv->row & SUBPEL_MASK;
  }
}

static INLINE void dec_build_inter_predictors(const AV1_COMMON *cm,
                                              MACROBLOCKD *xd, int plane,
                                              const MB_MODE_INFO *mi,
                                              int build_for_obmc, int bw,
                                              int bh, int mi_x, int mi_y) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int is_compound = has_second_ref(mi);
  int ref;
  const int is_intrabc = is_intrabc_block(mi);
  assert(IMPLIES(is_intrabc, !is_compound));
  int is_global[2] = { 0, 0 };
  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
    is_global[ref] = is_global_mv_block(mi, wm->wmtype);
  }

  const BLOCK_SIZE bsize = mi->sb_type;
  const int ss_x = pd->subsampling_x;
  const int ss_y = pd->subsampling_y;
  int sub8x8_inter = (block_size_wide[bsize] < 8 && ss_x) ||
                     (block_size_high[bsize] < 8 && ss_y);

  if (is_intrabc) sub8x8_inter = 0;

  // For sub8x8 chroma blocks, we may be covering more than one luma block's
  // worth of pixels. Thus (mi_x, mi_y) may not be the correct coordinates for
  // the top-left corner of the prediction source - the correct top-left corner
  // is at (pre_x, pre_y).
  const int row_start =
      (block_size_high[bsize] == 4) && ss_y && !build_for_obmc ? -1 : 0;
  const int col_start =
      (block_size_wide[bsize] == 4) && ss_x && !build_for_obmc ? -1 : 0;
  const int pre_x = (mi_x + MI_SIZE * col_start) >> ss_x;
  const int pre_y = (mi_y + MI_SIZE * row_start) >> ss_y;

  sub8x8_inter = sub8x8_inter && !build_for_obmc;
  if (sub8x8_inter) {
    for (int row = row_start; row <= 0 && sub8x8_inter; ++row) {
      for (int col = col_start; col <= 0; ++col) {
        const MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
        if (!is_inter_block(this_mbmi)) sub8x8_inter = 0;
        if (is_intrabc_block(this_mbmi)) sub8x8_inter = 0;
      }
    }
  }

  if (sub8x8_inter) {
    // block size
    const int b4_w = block_size_wide[bsize] >> ss_x;
    const int b4_h = block_size_high[bsize] >> ss_y;
    const BLOCK_SIZE plane_bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
    const int b8_w = block_size_wide[plane_bsize] >> ss_x;
    const int b8_h = block_size_high[plane_bsize] >> ss_y;
    assert(!is_compound);

    const struct buf_2d orig_pred_buf[2] = { pd->pre[0], pd->pre[1] };

    int row = row_start;
    int src_stride;
    for (int y = 0; y < b8_h; y += b4_h) {
      int col = col_start;
      for (int x = 0; x < b8_w; x += b4_w) {
        MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
        is_compound = has_second_ref(this_mbmi);
        DECLARE_ALIGNED(32, CONV_BUF_TYPE, tmp_dst[8 * 8]);
        int tmp_dst_stride = 8;
        assert(bw < 8 || bh < 8);
        ConvolveParams conv_params = get_conv_params_no_round(
            0, 0, plane, tmp_dst, tmp_dst_stride, is_compound, xd->bd);
        conv_params.use_jnt_comp_avg = 0;
        struct buf_2d *const dst_buf = &pd->dst;
        uint8_t *dst = dst_buf->buf + dst_buf->stride * y + x;

        ref = 0;
        const RefBuffer *ref_buf =
            &cm->frame_refs[this_mbmi->ref_frame[ref] - LAST_FRAME];

        pd->pre[ref].buf0 =
            (plane == 1) ? ref_buf->buf->u_buffer : ref_buf->buf->v_buffer;
        pd->pre[ref].buf =
            pd->pre[ref].buf0 + scaled_buffer_offset(pre_x, pre_y,
                                                     ref_buf->buf->uv_stride,
                                                     &ref_buf->sf);
        pd->pre[ref].width = ref_buf->buf->uv_crop_width;
        pd->pre[ref].height = ref_buf->buf->uv_crop_height;
        pd->pre[ref].stride = ref_buf->buf->uv_stride;

        const struct scale_factors *const sf =
            is_intrabc ? &cm->sf_identity : &ref_buf->sf;
        struct buf_2d *const pre_buf = is_intrabc ? dst_buf : &pd->pre[ref];

        const MV mv = this_mbmi->mv[ref].as_mv;

        uint8_t *pre;
        SubpelParams subpel_params;
        PadBlock block;
        MV32 scaled_mv;
        int subpel_x_mv, subpel_y_mv;
        int highbd;
        WarpTypesAllowed warp_types;
        warp_types.global_warp_allowed = is_global[ref];
        warp_types.local_warp_allowed = this_mbmi->motion_mode == WARPED_CAUSAL;

        dec_calc_subpel_params(xd, sf, mv, plane, pre_x, pre_y, x, y, pre_buf,
                               &subpel_params, bw, bh, &block, mi_x, mi_y,
                               &scaled_mv, &subpel_x_mv, &subpel_y_mv);
        pre = pre_buf->buf0 + block.y0 * pre_buf->stride + block.x0;
        src_stride = pre_buf->stride;
        highbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
        extend_mc_border(sf, pre_buf, scaled_mv, block, subpel_x_mv,
                         subpel_y_mv, 0, is_intrabc, highbd, xd->mc_buf[ref],
                         &pre, &src_stride);
        conv_params.ref = ref;
        conv_params.do_average = ref;
        if (is_masked_compound_type(mi->interinter_comp.type)) {
          // masked compound type has its own average mechanism
          conv_params.do_average = 0;
        }

        av1_make_inter_predictor(
            pre, src_stride, dst, dst_buf->stride, &subpel_params, sf, b4_w,
            b4_h, &conv_params, this_mbmi->interp_filters, &warp_types,
            (mi_x >> pd->subsampling_x) + x, (mi_y >> pd->subsampling_y) + y,
            plane, ref, mi, build_for_obmc, xd, cm->allow_warped_motion);

        ++col;
      }
      ++row;
    }

    for (ref = 0; ref < 2; ++ref) pd->pre[ref] = orig_pred_buf[ref];
    return;
  }

  {
    struct buf_2d *const dst_buf = &pd->dst;
    uint8_t *const dst = dst_buf->buf;
    uint8_t *pre[2];
    SubpelParams subpel_params[2];
    DECLARE_ALIGNED(32, uint16_t, tmp_dst[MAX_SB_SIZE * MAX_SB_SIZE]);
    int src_stride[2];
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      const struct scale_factors *const sf =
          is_intrabc ? &cm->sf_identity : &xd->block_refs[ref]->sf;
      struct buf_2d *const pre_buf = is_intrabc ? dst_buf : &pd->pre[ref];
      const MV mv = mi->mv[ref].as_mv;
      PadBlock block;
      MV32 scaled_mv;
      int subpel_x_mv, subpel_y_mv;
      int highbd;

      dec_calc_subpel_params(xd, sf, mv, plane, pre_x, pre_y, 0, 0, pre_buf,
                             &subpel_params[ref], bw, bh, &block, mi_x, mi_y,
                             &scaled_mv, &subpel_x_mv, &subpel_y_mv);
      pre[ref] = pre_buf->buf0 + block.y0 * pre_buf->stride + block.x0;
      src_stride[ref] = pre_buf->stride;
      highbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;

      WarpTypesAllowed warp_types;
      warp_types.global_warp_allowed = is_global[ref];
      warp_types.local_warp_allowed = mi->motion_mode == WARPED_CAUSAL;
      int do_warp = (bw >= 8 && bh >= 8 &&
                     av1_allow_warp(mi, &warp_types,
                                    &xd->global_motion[mi->ref_frame[ref]],
                                    build_for_obmc, subpel_params[ref].xs,
                                    subpel_params[ref].ys, NULL));
      do_warp = (do_warp && xd->cur_frame_force_integer_mv == 0);

      extend_mc_border(sf, pre_buf, scaled_mv, block, subpel_x_mv, subpel_y_mv,
                       do_warp, is_intrabc, highbd, xd->mc_buf[ref], &pre[ref],
                       &src_stride[ref]);
    }

    ConvolveParams conv_params = get_conv_params_no_round(
        0, 0, plane, tmp_dst, MAX_SB_SIZE, is_compound, xd->bd);
    av1_jnt_comp_weight_assign(cm, mi, 0, &conv_params.fwd_offset,
                               &conv_params.bck_offset,
                               &conv_params.use_jnt_comp_avg, is_compound);

    for (ref = 0; ref < 1 + is_compound; ++ref) {
      const struct scale_factors *const sf =
          is_intrabc ? &cm->sf_identity : &xd->block_refs[ref]->sf;
      WarpTypesAllowed warp_types;
      warp_types.global_warp_allowed = is_global[ref];
      warp_types.local_warp_allowed = mi->motion_mode == WARPED_CAUSAL;
      conv_params.ref = ref;
      conv_params.do_average = ref;
      if (is_masked_compound_type(mi->interinter_comp.type)) {
        // masked compound type has its own average mechanism
        conv_params.do_average = 0;
      }

      if (ref && is_masked_compound_type(mi->interinter_comp.type))
        av1_make_masked_inter_predictor(
            pre[ref], src_stride[ref], dst, dst_buf->stride,
            &subpel_params[ref], sf, bw, bh, &conv_params, mi->interp_filters,
            plane, &warp_types, mi_x >> pd->subsampling_x,
            mi_y >> pd->subsampling_y, ref, xd, cm->allow_warped_motion);
      else
        av1_make_inter_predictor(
            pre[ref], src_stride[ref], dst, dst_buf->stride,
            &subpel_params[ref], sf, bw, bh, &conv_params, mi->interp_filters,
            &warp_types, mi_x >> pd->subsampling_x, mi_y >> pd->subsampling_y,
            plane, ref, mi, build_for_obmc, xd, cm->allow_warped_motion);
    }
  }
}

static void dec_build_inter_predictors_for_planes(const AV1_COMMON *cm,
                                                  MACROBLOCKD *xd,
                                                  BLOCK_SIZE bsize, int mi_row,
                                                  int mi_col, int plane_from,
                                                  int plane_to) {
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = pd->width;
    const int bh = pd->height;

    if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                             pd->subsampling_y))
      continue;

    dec_build_inter_predictors(cm, xd, plane, xd->mi[0], 0, bw, bh, mi_x, mi_y);
  }
}

static void dec_build_inter_predictors_sby(const AV1_COMMON *cm,
                                           MACROBLOCKD *xd, int mi_row,
                                           int mi_col, BUFFER_SET *ctx,
                                           BLOCK_SIZE bsize) {
  dec_build_inter_predictors_for_planes(cm, xd, bsize, mi_row, mi_col, 0, 0);

  if (is_interintra_pred(xd->mi[0])) {
    BUFFER_SET default_ctx = { { xd->plane[0].dst.buf, NULL, NULL },
                               { xd->plane[0].dst.stride, 0, 0 } };
    if (!ctx) ctx = &default_ctx;
    av1_build_interintra_predictors_sby(cm, xd, xd->plane[0].dst.buf,
                                        xd->plane[0].dst.stride, ctx, bsize);
  }
}

static void dec_build_inter_predictors_sbuv(const AV1_COMMON *cm,
                                            MACROBLOCKD *xd, int mi_row,
                                            int mi_col, BUFFER_SET *ctx,
                                            BLOCK_SIZE bsize) {
  dec_build_inter_predictors_for_planes(cm, xd, bsize, mi_row, mi_col, 1,
                                        MAX_MB_PLANE - 1);

  if (is_interintra_pred(xd->mi[0])) {
    BUFFER_SET default_ctx = {
      { NULL, xd->plane[1].dst.buf, xd->plane[2].dst.buf },
      { 0, xd->plane[1].dst.stride, xd->plane[2].dst.stride }
    };
    if (!ctx) ctx = &default_ctx;
    av1_build_interintra_predictors_sbuv(
        cm, xd, xd->plane[1].dst.buf, xd->plane[2].dst.buf,
        xd->plane[1].dst.stride, xd->plane[2].dst.stride, ctx, bsize);
  }
}

static void dec_build_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                          int mi_row, int mi_col,
                                          BUFFER_SET *ctx, BLOCK_SIZE bsize) {
  const int num_planes = av1_num_planes(cm);
  dec_build_inter_predictors_sby(cm, xd, mi_row, mi_col, ctx, bsize);
  if (num_planes > 1)
    dec_build_inter_predictors_sbuv(cm, xd, mi_row, mi_col, ctx, bsize);
}

static INLINE void dec_build_prediction_by_above_pred(
    MACROBLOCKD *xd, int rel_mi_col, uint8_t above_mi_width,
    MB_MODE_INFO *above_mbmi, void *fun_ctxt, const int num_planes) {
  struct build_prediction_ctxt *ctxt = (struct build_prediction_ctxt *)fun_ctxt;
  const int above_mi_col = ctxt->mi_col + rel_mi_col;
  int mi_x, mi_y;
  MB_MODE_INFO backup_mbmi = *above_mbmi;

  av1_setup_build_prediction_by_above_pred(xd, rel_mi_col, above_mi_width,
                                           &backup_mbmi, ctxt, num_planes);
  mi_x = above_mi_col << MI_SIZE_LOG2;
  mi_y = ctxt->mi_row << MI_SIZE_LOG2;

  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;

  for (int j = 0; j < num_planes; ++j) {
    const struct macroblockd_plane *pd = &xd->plane[j];
    int bw = (above_mi_width * MI_SIZE) >> pd->subsampling_x;
    int bh = clamp(block_size_high[bsize] >> (pd->subsampling_y + 1), 4,
                   block_size_high[BLOCK_64X64] >> (pd->subsampling_y + 1));

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 0)) continue;
    dec_build_inter_predictors(ctxt->cm, xd, j, &backup_mbmi, 1, bw, bh, mi_x,
                               mi_y);
  }
}

static void dec_build_prediction_by_above_preds(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int mi_row, int mi_col,
    uint8_t *tmp_buf[MAX_MB_PLANE], int tmp_width[MAX_MB_PLANE],
    int tmp_height[MAX_MB_PLANE], int tmp_stride[MAX_MB_PLANE]) {
  if (!xd->up_available) return;

  // Adjust mb_to_bottom_edge to have the correct value for the OBMC
  // prediction block. This is half the height of the original block,
  // except for 128-wide blocks, where we only use a height of 32.
  int this_height = xd->n8_h * MI_SIZE;
  int pred_height = AOMMIN(this_height / 2, 32);
  xd->mb_to_bottom_edge += (this_height - pred_height) * 8;

  struct build_prediction_ctxt ctxt = { cm,         mi_row,
                                        mi_col,     tmp_buf,
                                        tmp_width,  tmp_height,
                                        tmp_stride, xd->mb_to_right_edge };
  BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  foreach_overlappable_nb_above(cm, xd, mi_col,
                                max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                dec_build_prediction_by_above_pred, &ctxt);

  xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
  xd->mb_to_right_edge = ctxt.mb_to_far_edge;
  xd->mb_to_bottom_edge -= (this_height - pred_height) * 8;
}

static INLINE void dec_build_prediction_by_left_pred(
    MACROBLOCKD *xd, int rel_mi_row, uint8_t left_mi_height,
    MB_MODE_INFO *left_mbmi, void *fun_ctxt, const int num_planes) {
  struct build_prediction_ctxt *ctxt = (struct build_prediction_ctxt *)fun_ctxt;
  const int left_mi_row = ctxt->mi_row + rel_mi_row;
  int mi_x, mi_y;
  MB_MODE_INFO backup_mbmi = *left_mbmi;

  av1_setup_build_prediction_by_left_pred(xd, rel_mi_row, left_mi_height,
                                          &backup_mbmi, ctxt, num_planes);
  mi_x = ctxt->mi_col << MI_SIZE_LOG2;
  mi_y = left_mi_row << MI_SIZE_LOG2;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;

  for (int j = 0; j < num_planes; ++j) {
    const struct macroblockd_plane *pd = &xd->plane[j];
    int bw = clamp(block_size_wide[bsize] >> (pd->subsampling_x + 1), 4,
                   block_size_wide[BLOCK_64X64] >> (pd->subsampling_x + 1));
    int bh = (left_mi_height << MI_SIZE_LOG2) >> pd->subsampling_y;

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 1)) continue;
    dec_build_inter_predictors(ctxt->cm, xd, j, &backup_mbmi, 1, bw, bh, mi_x,
                               mi_y);
  }
}

static void dec_build_prediction_by_left_preds(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int mi_row, int mi_col,
    uint8_t *tmp_buf[MAX_MB_PLANE], int tmp_width[MAX_MB_PLANE],
    int tmp_height[MAX_MB_PLANE], int tmp_stride[MAX_MB_PLANE]) {
  if (!xd->left_available) return;

  // Adjust mb_to_right_edge to have the correct value for the OBMC
  // prediction block. This is half the width of the original block,
  // except for 128-wide blocks, where we only use a width of 32.
  int this_width = xd->n8_w * MI_SIZE;
  int pred_width = AOMMIN(this_width / 2, 32);
  xd->mb_to_right_edge += (this_width - pred_width) * 8;

  struct build_prediction_ctxt ctxt = { cm,         mi_row,
                                        mi_col,     tmp_buf,
                                        tmp_width,  tmp_height,
                                        tmp_stride, xd->mb_to_bottom_edge };
  BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  foreach_overlappable_nb_left(cm, xd, mi_row,
                               max_neighbor_obmc[mi_size_high_log2[bsize]],
                               dec_build_prediction_by_left_pred, &ctxt);

  xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
  xd->mb_to_right_edge -= (this_width - pred_width) * 8;
  xd->mb_to_bottom_edge = ctxt.mb_to_far_edge;
}

static void dec_build_obmc_inter_predictors_sb(const AV1_COMMON *cm,
                                               MACROBLOCKD *xd, int mi_row,
                                               int mi_col) {
  const int num_planes = av1_num_planes(cm);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf1[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf2[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
  uint8_t *dst_buf1[MAX_MB_PLANE], *dst_buf2[MAX_MB_PLANE];
  int dst_stride1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_stride2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    dst_buf1[0] = CONVERT_TO_BYTEPTR(tmp_buf1);
    dst_buf1[1] = CONVERT_TO_BYTEPTR(tmp_buf1 + MAX_SB_SQUARE * len);
    dst_buf1[2] = CONVERT_TO_BYTEPTR(tmp_buf1 + MAX_SB_SQUARE * 2 * len);
    dst_buf2[0] = CONVERT_TO_BYTEPTR(tmp_buf2);
    dst_buf2[1] = CONVERT_TO_BYTEPTR(tmp_buf2 + MAX_SB_SQUARE * len);
    dst_buf2[2] = CONVERT_TO_BYTEPTR(tmp_buf2 + MAX_SB_SQUARE * 2 * len);
  } else {
    dst_buf1[0] = tmp_buf1;
    dst_buf1[1] = tmp_buf1 + MAX_SB_SQUARE;
    dst_buf1[2] = tmp_buf1 + MAX_SB_SQUARE * 2;
    dst_buf2[0] = tmp_buf2;
    dst_buf2[1] = tmp_buf2 + MAX_SB_SQUARE;
    dst_buf2[2] = tmp_buf2 + MAX_SB_SQUARE * 2;
  }
  dec_build_prediction_by_above_preds(cm, xd, mi_row, mi_col, dst_buf1,
                                      dst_width1, dst_height1, dst_stride1);
  dec_build_prediction_by_left_preds(cm, xd, mi_row, mi_col, dst_buf2,
                                     dst_width2, dst_height2, dst_stride2);
  av1_setup_dst_planes(xd->plane, xd->mi[0]->sb_type, get_frame_new_buffer(cm),
                       mi_row, mi_col, 0, num_planes);
  av1_build_obmc_inter_prediction(cm, xd, mi_row, mi_col, dst_buf1, dst_stride1,
                                  dst_buf2, dst_stride2);
}

static void cfl_store_inter_block(AV1_COMMON *const cm, MACROBLOCKD *const xd) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  if (store_cfl_required(cm, xd)) {
    cfl_store_block(xd, mbmi->sb_type, mbmi->tx_size);
  }
}

static void predict_inter_block(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                int mi_row, int mi_col, BLOCK_SIZE bsize) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int num_planes = av1_num_planes(cm);
  for (int ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
    if (frame < LAST_FRAME) {
      assert(is_intrabc_block(mbmi));
      assert(frame == INTRA_FRAME);
      assert(ref == 0);
    } else {
      RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];

      xd->block_refs[ref] = ref_buf;
      av1_setup_pre_planes(xd, ref, ref_buf->buf, mi_row, mi_col, &ref_buf->sf,
                           num_planes);
    }
  }

  dec_build_inter_predictors_sb(cm, xd, mi_row, mi_col, NULL, bsize);
  if (mbmi->motion_mode == OBMC_CAUSAL)
    dec_build_obmc_inter_predictors_sb(cm, xd, mi_row, mi_col);
}

static void set_color_index_map_offset(MACROBLOCKD *const xd, int plane,
                                       aom_reader *r) {
  (void)r;
  Av1ColorMapParam params;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  av1_get_block_dimensions(mbmi->sb_type, plane, xd, &params.plane_width,
                           &params.plane_height, NULL, NULL);
  xd->color_index_map_offset[plane] += params.plane_width * params.plane_height;
}

static void decode_token_and_recon_block(AV1Decoder *const pbi,
                                         MACROBLOCKD *const xd, int mi_row,
                                         int mi_col, aom_reader *r,
                                         BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);

  set_offsets(cm, xd, bsize, mi_row, mi_col, bw, bh, x_mis, y_mis);
  MB_MODE_INFO *mbmi = xd->mi[0];
  CFL_CTX *const cfl = &xd->cfl;
  cfl->is_chroma_reference = is_chroma_reference(
      mi_row, mi_col, bsize, cfl->subsampling_x, cfl->subsampling_y);

  if (cm->delta_q_present_flag) {
    for (int i = 0; i < MAX_SEGMENTS; i++) {
      const int current_qindex =
          av1_get_qindex(&cm->seg, i, xd->current_qindex);
      for (int j = 0; j < num_planes; ++j) {
        const int dc_delta_q =
            j == 0 ? cm->y_dc_delta_q
                   : (j == 1 ? cm->u_dc_delta_q : cm->v_dc_delta_q);
        const int ac_delta_q =
            j == 0 ? 0 : (j == 1 ? cm->u_ac_delta_q : cm->v_ac_delta_q);
        xd->plane[j].seg_dequant_QTX[i][0] =
            av1_dc_quant_QTX(current_qindex, dc_delta_q, cm->bit_depth);
        xd->plane[j].seg_dequant_QTX[i][1] =
            av1_ac_quant_QTX(current_qindex, ac_delta_q, cm->bit_depth);
      }
    }
  }
  if (mbmi->skip) av1_reset_skip_context(xd, mi_row, mi_col, bsize, num_planes);

  if (!is_inter_block(mbmi)) {
    int row, col;
    assert(bsize == get_plane_block_size(bsize, xd->plane[0].subsampling_x,
                                         xd->plane[0].subsampling_y));
    const int max_blocks_wide = max_block_wide(xd, bsize, 0);
    const int max_blocks_high = max_block_high(xd, bsize, 0);
    const BLOCK_SIZE max_unit_bsize = BLOCK_64X64;
    int mu_blocks_wide =
        block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
    int mu_blocks_high =
        block_size_high[max_unit_bsize] >> tx_size_high_log2[0];
    mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
    mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

    for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
      for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
        for (int plane = 0; plane < num_planes; ++plane) {
          const struct macroblockd_plane *const pd = &xd->plane[plane];
          if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                                   pd->subsampling_y))
            continue;

          const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
          const int stepr = tx_size_high_unit[tx_size];
          const int stepc = tx_size_wide_unit[tx_size];

          const int unit_height = ROUND_POWER_OF_TWO(
              AOMMIN(mu_blocks_high + row, max_blocks_high), pd->subsampling_y);
          const int unit_width = ROUND_POWER_OF_TWO(
              AOMMIN(mu_blocks_wide + col, max_blocks_wide), pd->subsampling_x);

          for (int blk_row = row >> pd->subsampling_y; blk_row < unit_height;
               blk_row += stepr) {
            for (int blk_col = col >> pd->subsampling_x; blk_col < unit_width;
                 blk_col += stepc) {
              read_coeffs_tx_intra_block(cm, xd, r, plane, blk_row, blk_col,
                                         tx_size);
              predict_and_reconstruct_intra_block(cm, xd, r, plane, blk_row,
                                                  blk_col, tx_size);
              set_cb_buffer_offsets(xd, tx_size, plane);
            }
          }
        }
      }
    }
  } else {
    predict_inter_block(cm, xd, mi_row, mi_col, bsize);
#if CONFIG_MISMATCH_DEBUG
    for (int plane = 0; plane < num_planes; ++plane) {
      const struct macroblockd_plane *pd = &xd->plane[plane];
      int pixel_c, pixel_r;
      mi_to_pixel_loc(&pixel_c, &pixel_r, mi_col, mi_row, 0, 0,
                      pd->subsampling_x, pd->subsampling_y);
      if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                               pd->subsampling_y))
        continue;
      mismatch_check_block_pre(pd->dst.buf, pd->dst.stride, cm->frame_offset,
                               plane, pixel_c, pixel_r, pd->width, pd->height,
                               xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
    }
#endif

    // Reconstruction
    if (!mbmi->skip) {
      int eobtotal = 0;

      const int max_blocks_wide = max_block_wide(xd, bsize, 0);
      const int max_blocks_high = max_block_high(xd, bsize, 0);
      int row, col;

      const BLOCK_SIZE max_unit_bsize = BLOCK_64X64;
      assert(max_unit_bsize ==
             get_plane_block_size(BLOCK_64X64, xd->plane[0].subsampling_x,
                                  xd->plane[0].subsampling_y));
      int mu_blocks_wide =
          block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
      int mu_blocks_high =
          block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

      mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
      mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

      for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
        for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
          for (int plane = 0; plane < num_planes; ++plane) {
            const struct macroblockd_plane *const pd = &xd->plane[plane];
            if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                                     pd->subsampling_y))
              continue;
            const BLOCK_SIZE bsizec =
                scale_chroma_bsize(bsize, pd->subsampling_x, pd->subsampling_y);
            const BLOCK_SIZE plane_bsize = get_plane_block_size(
                bsizec, pd->subsampling_x, pd->subsampling_y);

            const TX_SIZE max_tx_size =
                get_vartx_max_txsize(xd, plane_bsize, plane);
            const int bh_var_tx = tx_size_high_unit[max_tx_size];
            const int bw_var_tx = tx_size_wide_unit[max_tx_size];
            int block = 0;
            int step =
                tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
            int blk_row, blk_col;
            const int unit_height = ROUND_POWER_OF_TWO(
                AOMMIN(mu_blocks_high + row, max_blocks_high),
                pd->subsampling_y);
            const int unit_width = ROUND_POWER_OF_TWO(
                AOMMIN(mu_blocks_wide + col, max_blocks_wide),
                pd->subsampling_x);

            for (blk_row = row >> pd->subsampling_y; blk_row < unit_height;
                 blk_row += bh_var_tx) {
              for (blk_col = col >> pd->subsampling_x; blk_col < unit_width;
                   blk_col += bw_var_tx) {
                decode_reconstruct_tx(cm, xd, r, mbmi, plane, plane_bsize,
                                      blk_row, blk_col, block, max_tx_size,
                                      &eobtotal);
                block += step;
              }
            }
          }
        }
      }
    }
    cfl_store_inter_block(cm, xd);
  }

  av1_visit_palette(pbi, xd, mi_row, mi_col, r, bsize,
                    set_color_index_map_offset);

  int reader_corrupted_flag = aom_reader_has_error(r);
  aom_merge_corrupted_flag(&xd->corrupted, reader_corrupted_flag);
}

static void read_tx_size_vartx(MACROBLOCKD *xd, MB_MODE_INFO *mbmi,
                               TX_SIZE tx_size, int depth, int blk_row,
                               int blk_col, aom_reader *r) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  int is_split = 0;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);

  if (depth == MAX_VARTX_DEPTH) {
    for (int idy = 0; idy < tx_size_high_unit[tx_size]; ++idy) {
      for (int idx = 0; idx < tx_size_wide_unit[tx_size]; ++idx) {
        const int index =
            av1_get_txb_size_index(bsize, blk_row + idy, blk_col + idx);
        mbmi->inter_tx_size[index] = tx_size;
      }
    }
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

  const int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                         xd->left_txfm_context + blk_row,
                                         mbmi->sb_type, tx_size);
  is_split = aom_read_symbol(r, ec_ctx->txfm_partition_cdf[ctx], 2, ACCT_STR);

  if (is_split) {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];

    if (sub_txs == TX_4X4) {
      for (int idy = 0; idy < tx_size_high_unit[tx_size]; ++idy) {
        for (int idx = 0; idx < tx_size_wide_unit[tx_size]; ++idx) {
          const int index =
              av1_get_txb_size_index(bsize, blk_row + idy, blk_col + idx);
          mbmi->inter_tx_size[index] = sub_txs;
        }
      }
      mbmi->tx_size = sub_txs;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, sub_txs, tx_size);
      return;
    }

    assert(bsw > 0 && bsh > 0);
    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        int offsetr = blk_row + row;
        int offsetc = blk_col + col;
        read_tx_size_vartx(xd, mbmi, sub_txs, depth + 1, offsetr, offsetc, r);
      }
    }
  } else {
    for (int idy = 0; idy < tx_size_high_unit[tx_size]; ++idy) {
      for (int idx = 0; idx < tx_size_wide_unit[tx_size]; ++idx) {
        const int index =
            av1_get_txb_size_index(bsize, blk_row + idy, blk_col + idx);
        mbmi->inter_tx_size[index] = tx_size;
      }
    }
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  }
}

static TX_SIZE read_selected_tx_size(MACROBLOCKD *xd, aom_reader *r) {
  // TODO(debargha): Clean up the logic here. This function should only
  // be called for intra.
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize);
  const int max_depths = bsize_to_max_depth(bsize);
  const int ctx = get_tx_size_context(xd);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const int depth = aom_read_symbol(r, ec_ctx->tx_size_cdf[tx_size_cat][ctx],
                                    max_depths + 1, ACCT_STR);
  assert(depth >= 0 && depth <= max_depths);
  const TX_SIZE tx_size = depth_to_tx_size(depth, bsize);
  return tx_size;
}

static TX_SIZE read_tx_size(AV1_COMMON *cm, MACROBLOCKD *xd, int is_inter,
                            int allow_select_inter, aom_reader *r) {
  const TX_MODE tx_mode = cm->tx_mode;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  if (xd->lossless[xd->mi[0]->segment_id]) return TX_4X4;

  if (block_signals_txsize(bsize)) {
    if ((!is_inter || allow_select_inter) && tx_mode == TX_MODE_SELECT) {
      const TX_SIZE coded_tx_size = read_selected_tx_size(xd, r);
      return coded_tx_size;
    } else {
      return tx_size_from_tx_mode(bsize, tx_mode);
    }
  } else {
    assert(IMPLIES(tx_mode == ONLY_4X4, bsize == BLOCK_4X4));
    return max_txsize_rect_lookup[bsize];
  }
}

static void decode_block(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                         int mi_row, int mi_col, aom_reader *r,
                         PARTITION_TYPE partition, BLOCK_SIZE bsize) {
  decode_mbmi_block(pbi, xd, mi_row, mi_col, r, partition, bsize);

  av1_visit_palette(pbi, xd, mi_row, mi_col, r, bsize,
                    av1_decode_palette_tokens);

  AV1_COMMON *cm = &pbi->common;
  MB_MODE_INFO *mbmi = xd->mi[0];
  int inter_block_tx = is_inter_block(mbmi) || is_intrabc_block(mbmi);
  if (cm->tx_mode == TX_MODE_SELECT && block_signals_txsize(bsize) &&
      !mbmi->skip && inter_block_tx && !xd->lossless[mbmi->segment_id]) {
    const TX_SIZE max_tx_size = max_txsize_rect_lookup[bsize];
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    const int width = block_size_wide[bsize] >> tx_size_wide_log2[0];
    const int height = block_size_high[bsize] >> tx_size_high_log2[0];

    for (int idy = 0; idy < height; idy += bh)
      for (int idx = 0; idx < width; idx += bw)
        read_tx_size_vartx(xd, mbmi, max_tx_size, 0, idy, idx, r);
  } else {
    mbmi->tx_size = read_tx_size(cm, xd, inter_block_tx, !mbmi->skip, r);
    if (inter_block_tx)
      memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
    set_txfm_ctxs(mbmi->tx_size, xd->n8_w, xd->n8_h,
                  mbmi->skip && is_inter_block(mbmi), xd);
  }

  decode_token_and_recon_block(pbi, xd, mi_row, mi_col, r, bsize);
}

static PARTITION_TYPE read_partition(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     aom_reader *r, int has_rows, int has_cols,
                                     BLOCK_SIZE bsize) {
  const int ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (!has_rows && !has_cols) return PARTITION_SPLIT;

  assert(ctx >= 0);
  aom_cdf_prob *partition_cdf = ec_ctx->partition_cdf[ctx];
  if (has_rows && has_cols) {
    return (PARTITION_TYPE)aom_read_symbol(
        r, partition_cdf, partition_cdf_length(bsize), ACCT_STR);
  } else if (!has_rows && has_cols) {
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_vert_alike(cdf, partition_cdf, bsize);
    assert(cdf[1] == AOM_ICDF(CDF_PROB_TOP));
    return aom_read_cdf(r, cdf, 2, ACCT_STR) ? PARTITION_SPLIT : PARTITION_HORZ;
  } else {
    assert(has_rows && !has_cols);
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_horz_alike(cdf, partition_cdf, bsize);
    assert(cdf[1] == AOM_ICDF(CDF_PROB_TOP));
    return aom_read_cdf(r, cdf, 2, ACCT_STR) ? PARTITION_SPLIT : PARTITION_VERT;
  }
}

// TODO(slavarnway): eliminate bsize and subsize in future commits
static void decode_partition(AV1Decoder *const pbi, MACROBLOCKD *const xd,
                             int mi_row, int mi_col, aom_reader *r,
                             BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const int bw = mi_size_wide[bsize];
  const int hbs = bw >> 1;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  const int quarter_step = bw / 4;
  BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);
  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  const int num_planes = av1_num_planes(cm);
  for (int plane = 0; plane < num_planes; ++plane) {
    int rcol0, rcol1, rrow0, rrow1, tile_tl_idx;
    if (av1_loop_restoration_corners_in_sb(cm, plane, mi_row, mi_col, bsize,
                                           &rcol0, &rcol1, &rrow0, &rrow1,
                                           &tile_tl_idx)) {
      const int rstride = cm->rst_info[plane].horz_units_per_tile;
      for (int rrow = rrow0; rrow < rrow1; ++rrow) {
        for (int rcol = rcol0; rcol < rcol1; ++rcol) {
          const int runit_idx = tile_tl_idx + rcol + rrow * rstride;
          loop_restoration_read_sb_coeffs(cm, xd, r, plane, runit_idx);
        }
      }
    }
  }

  partition = (bsize < BLOCK_8X8) ? PARTITION_NONE
                                  : read_partition(xd, mi_row, mi_col, r,
                                                   has_rows, has_cols, bsize);
  subsize = get_partition_subsize(bsize, partition);

  // Check the bitstream is conformant: if there is subsampling on the
  // chroma planes, subsize must subsample to a valid block size.
  const struct macroblockd_plane *const pd_u = &xd->plane[1];
  if (get_plane_block_size(subsize, pd_u->subsampling_x, pd_u->subsampling_y) ==
      BLOCK_INVALID) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Block size %dx%d invalid with this subsampling mode",
                       block_size_wide[subsize], block_size_high[subsize]);
  }

#define DEC_BLOCK_STX_ARG
#define DEC_BLOCK_EPT_ARG partition,
#define DEC_BLOCK(db_r, db_c, db_subsize)                   \
  decode_block(pbi, xd, DEC_BLOCK_STX_ARG(db_r), (db_c), r, \
               DEC_BLOCK_EPT_ARG(db_subsize))
#define DEC_PARTITION(db_r, db_c, db_subsize) \
  decode_partition(pbi, xd, DEC_BLOCK_STX_ARG(db_r), (db_c), r, (db_subsize))

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
    case PARTITION_HORZ_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;
        DEC_BLOCK(this_mi_row, mi_col, subsize);
      }
      break;
    case PARTITION_VERT_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;
        DEC_BLOCK(mi_row, this_mi_col, subsize);
      }
      break;
    default: assert(0 && "Invalid partition type");
  }

#undef DEC_PARTITION
#undef DEC_BLOCK
#undef DEC_BLOCK_EPT_ARG
#undef DEC_BLOCK_STX_ARG

  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
}

static void setup_bool_decoder(const uint8_t *data, const uint8_t *data_end,
                               const size_t read_size,
                               struct aom_internal_error_info *error_info,
                               aom_reader *r, uint8_t allow_update_cdf) {
  // Validate the calculated partition length. If the buffer
  // described by the partition can't be fully read, then restrict
  // it to the portion that can be (for EC mode) or throw an error.
  if (!read_is_valid(data, read_size, data_end))
    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile length");

  if (aom_reader_init(r, data, read_size))
    aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate bool decoder %d", 1);

  r->allow_update_cdf = allow_update_cdf;
}

static void setup_segmentation(AV1_COMMON *const cm,
                               struct aom_read_bit_buffer *rb) {
  struct segmentation *const seg = &cm->seg;

  seg->update_map = 0;
  seg->update_data = 0;
  seg->temporal_update = 0;

  seg->enabled = aom_rb_read_bit(rb);
  if (!seg->enabled) {
    if (cm->cur_frame->seg_map)
      memset(cm->cur_frame->seg_map, 0, (cm->mi_rows * cm->mi_cols));

    memset(seg, 0, sizeof(*seg));
    segfeatures_copy(&cm->cur_frame->seg, seg);
    return;
  }
  if (cm->seg.enabled && cm->prev_frame &&
      (cm->mi_rows == cm->prev_frame->mi_rows) &&
      (cm->mi_cols == cm->prev_frame->mi_cols)) {
    cm->last_frame_seg_map = cm->prev_frame->seg_map;
  } else {
    cm->last_frame_seg_map = NULL;
  }
  // Read update flags
  if (cm->primary_ref_frame == PRIMARY_REF_NONE) {
    // These frames can't use previous frames, so must signal map + features
    seg->update_map = 1;
    seg->temporal_update = 0;
    seg->update_data = 1;
  } else {
    seg->update_map = aom_rb_read_bit(rb);
    if (seg->update_map) {
      seg->temporal_update = aom_rb_read_bit(rb);
    } else {
      seg->temporal_update = 0;
    }
    seg->update_data = aom_rb_read_bit(rb);
  }

  // Segmentation data update
  if (seg->update_data) {
    av1_clearall_segfeatures(seg);

    for (int i = 0; i < MAX_SEGMENTS; i++) {
      for (int j = 0; j < SEG_LVL_MAX; j++) {
        int data = 0;
        const int feature_enabled = aom_rb_read_bit(rb);
        if (feature_enabled) {
          av1_enable_segfeature(seg, i, j);

          const int data_max = av1_seg_feature_data_max(j);
          const int data_min = -data_max;
          const int ubits = get_unsigned_bits(data_max);

          if (av1_is_segfeature_signed(j)) {
            data = aom_rb_read_inv_signed_literal(rb, ubits);
          } else {
            data = aom_rb_read_literal(rb, ubits);
          }

          data = clamp(data, data_min, data_max);
        }
        av1_set_segdata(seg, i, j, data);
      }
    }
    calculate_segdata(seg);
  } else if (cm->prev_frame) {
    segfeatures_copy(seg, &cm->prev_frame->seg);
  }
  segfeatures_copy(&cm->cur_frame->seg, seg);
}

static void decode_restoration_mode(AV1_COMMON *cm,
                                    struct aom_read_bit_buffer *rb) {
  assert(!cm->all_lossless);
  const int num_planes = av1_num_planes(cm);
  if (cm->allow_intrabc) return;
  int all_none = 1, chroma_none = 1;
  for (int p = 0; p < num_planes; ++p) {
    RestorationInfo *rsi = &cm->rst_info[p];
    if (aom_rb_read_bit(rb)) {
      rsi->frame_restoration_type =
          aom_rb_read_bit(rb) ? RESTORE_SGRPROJ : RESTORE_WIENER;
    } else {
      rsi->frame_restoration_type =
          aom_rb_read_bit(rb) ? RESTORE_SWITCHABLE : RESTORE_NONE;
    }
    if (rsi->frame_restoration_type != RESTORE_NONE) {
      all_none = 0;
      chroma_none &= p == 0;
    }
  }
  if (!all_none) {
    assert(cm->seq_params.sb_size == BLOCK_64X64 ||
           cm->seq_params.sb_size == BLOCK_128X128);
    const int sb_size = cm->seq_params.sb_size == BLOCK_128X128 ? 128 : 64;

    for (int p = 0; p < num_planes; ++p)
      cm->rst_info[p].restoration_unit_size = sb_size;

    RestorationInfo *rsi = &cm->rst_info[0];

    if (sb_size == 64) {
      rsi->restoration_unit_size <<= aom_rb_read_bit(rb);
    }
    if (rsi->restoration_unit_size > 64) {
      rsi->restoration_unit_size <<= aom_rb_read_bit(rb);
    }
  } else {
    const int size = RESTORATION_UNITSIZE_MAX;
    for (int p = 0; p < num_planes; ++p)
      cm->rst_info[p].restoration_unit_size = size;
  }

  if (num_planes > 1) {
    int s = AOMMIN(cm->subsampling_x, cm->subsampling_y);
    if (s && !chroma_none) {
      cm->rst_info[1].restoration_unit_size =
          cm->rst_info[0].restoration_unit_size >> (aom_rb_read_bit(rb) * s);
    } else {
      cm->rst_info[1].restoration_unit_size =
          cm->rst_info[0].restoration_unit_size;
    }
    cm->rst_info[2].restoration_unit_size =
        cm->rst_info[1].restoration_unit_size;
  }
}

static void read_wiener_filter(int wiener_win, WienerInfo *wiener_info,
                               WienerInfo *ref_wiener_info, aom_reader *rb) {
  memset(wiener_info->vfilter, 0, sizeof(wiener_info->vfilter));
  memset(wiener_info->hfilter, 0, sizeof(wiener_info->hfilter));

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
  const sgr_params_type *params = &sgr_params[sgrproj_info->ep];

  if (params->r[0] == 0) {
    sgrproj_info->xqd[0] = 0;
    sgrproj_info->xqd[1] =
        aom_read_primitive_refsubexpfin(
            rb, SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
            ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1, ACCT_STR) +
        SGRPROJ_PRJ_MIN1;
  } else if (params->r[1] == 0) {
    sgrproj_info->xqd[0] =
        aom_read_primitive_refsubexpfin(
            rb, SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
            ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0, ACCT_STR) +
        SGRPROJ_PRJ_MIN0;
    sgrproj_info->xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - sgrproj_info->xqd[0],
                                 SGRPROJ_PRJ_MIN1, SGRPROJ_PRJ_MAX1);
  } else {
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
  }

  memcpy(ref_sgrproj_info, sgrproj_info, sizeof(*sgrproj_info));
}

static void loop_restoration_read_sb_coeffs(const AV1_COMMON *const cm,
                                            MACROBLOCKD *xd,
                                            aom_reader *const r, int plane,
                                            int runit_idx) {
  const RestorationInfo *rsi = &cm->rst_info[plane];
  RestorationUnitInfo *rui = &rsi->unit_info[runit_idx];
  if (rsi->frame_restoration_type == RESTORE_NONE) return;

  assert(!cm->all_lossless);

  const int wiener_win = (plane > 0) ? WIENER_WIN_CHROMA : WIENER_WIN;
  WienerInfo *wiener_info = xd->wiener_info + plane;
  SgrprojInfo *sgrproj_info = xd->sgrproj_info + plane;

  if (rsi->frame_restoration_type == RESTORE_SWITCHABLE) {
    rui->restoration_type =
        aom_read_symbol(r, xd->tile_ctx->switchable_restore_cdf,
                        RESTORE_SWITCHABLE_TYPES, ACCT_STR);
    switch (rui->restoration_type) {
      case RESTORE_WIENER:
        read_wiener_filter(wiener_win, &rui->wiener_info, wiener_info, r);
        break;
      case RESTORE_SGRPROJ:
        read_sgrproj_filter(&rui->sgrproj_info, sgrproj_info, r);
        break;
      default: assert(rui->restoration_type == RESTORE_NONE); break;
    }
  } else if (rsi->frame_restoration_type == RESTORE_WIENER) {
    if (aom_read_symbol(r, xd->tile_ctx->wiener_restore_cdf, 2, ACCT_STR)) {
      rui->restoration_type = RESTORE_WIENER;
      read_wiener_filter(wiener_win, &rui->wiener_info, wiener_info, r);
    } else {
      rui->restoration_type = RESTORE_NONE;
    }
  } else if (rsi->frame_restoration_type == RESTORE_SGRPROJ) {
    if (aom_read_symbol(r, xd->tile_ctx->sgrproj_restore_cdf, 2, ACCT_STR)) {
      rui->restoration_type = RESTORE_SGRPROJ;
      read_sgrproj_filter(&rui->sgrproj_info, sgrproj_info, r);
    } else {
      rui->restoration_type = RESTORE_NONE;
    }
  }
}

static void setup_loopfilter(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  const int num_planes = av1_num_planes(cm);
  struct loopfilter *lf = &cm->lf;
  if (cm->allow_intrabc || cm->coded_lossless) {
    // write default deltas to frame buffer
    av1_set_default_ref_deltas(cm->cur_frame->ref_deltas);
    av1_set_default_mode_deltas(cm->cur_frame->mode_deltas);
    return;
  }
  assert(!cm->coded_lossless);
  if (cm->prev_frame) {
    // write deltas to frame buffer
    memcpy(lf->ref_deltas, cm->prev_frame->ref_deltas, REF_FRAMES);
    memcpy(lf->mode_deltas, cm->prev_frame->mode_deltas, MAX_MODE_LF_DELTAS);
  } else {
    av1_set_default_ref_deltas(lf->ref_deltas);
    av1_set_default_mode_deltas(lf->mode_deltas);
  }
  lf->filter_level[0] = aom_rb_read_literal(rb, 6);
  lf->filter_level[1] = aom_rb_read_literal(rb, 6);
  if (num_planes > 1) {
    if (lf->filter_level[0] || lf->filter_level[1]) {
      lf->filter_level_u = aom_rb_read_literal(rb, 6);
      lf->filter_level_v = aom_rb_read_literal(rb, 6);
    }
  }
  lf->sharpness_level = aom_rb_read_literal(rb, 3);

  // Read in loop filter deltas applied at the MB level based on mode or ref
  // frame.
  lf->mode_ref_delta_update = 0;

  lf->mode_ref_delta_enabled = aom_rb_read_bit(rb);
  if (lf->mode_ref_delta_enabled) {
    lf->mode_ref_delta_update = aom_rb_read_bit(rb);
    if (lf->mode_ref_delta_update) {
      for (int i = 0; i < REF_FRAMES; i++)
        if (aom_rb_read_bit(rb))
          lf->ref_deltas[i] = aom_rb_read_inv_signed_literal(rb, 6);

      for (int i = 0; i < MAX_MODE_LF_DELTAS; i++)
        if (aom_rb_read_bit(rb))
          lf->mode_deltas[i] = aom_rb_read_inv_signed_literal(rb, 6);
    }
  }

  // write deltas to frame buffer
  memcpy(cm->cur_frame->ref_deltas, lf->ref_deltas, REF_FRAMES);
  memcpy(cm->cur_frame->mode_deltas, lf->mode_deltas, MAX_MODE_LF_DELTAS);
}

static void setup_cdef(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  const int num_planes = av1_num_planes(cm);
  if (cm->allow_intrabc) return;
  cm->cdef_pri_damping = cm->cdef_sec_damping = aom_rb_read_literal(rb, 2) + 3;
  cm->cdef_bits = aom_rb_read_literal(rb, 2);
  cm->nb_cdef_strengths = 1 << cm->cdef_bits;
  for (int i = 0; i < cm->nb_cdef_strengths; i++) {
    cm->cdef_strengths[i] = aom_rb_read_literal(rb, CDEF_STRENGTH_BITS);
    cm->cdef_uv_strengths[i] =
        num_planes > 1 ? aom_rb_read_literal(rb, CDEF_STRENGTH_BITS) : 0;
  }
}

static INLINE int read_delta_q(struct aom_read_bit_buffer *rb) {
  return aom_rb_read_bit(rb) ? aom_rb_read_inv_signed_literal(rb, 6) : 0;
}

static void setup_quantization(AV1_COMMON *const cm,
                               struct aom_read_bit_buffer *rb) {
  const int num_planes = av1_num_planes(cm);
  cm->base_qindex = aom_rb_read_literal(rb, QINDEX_BITS);
  cm->y_dc_delta_q = read_delta_q(rb);
  if (num_planes > 1) {
    int diff_uv_delta = 0;
    if (cm->separate_uv_delta_q) diff_uv_delta = aom_rb_read_bit(rb);
    cm->u_dc_delta_q = read_delta_q(rb);
    cm->u_ac_delta_q = read_delta_q(rb);
    if (diff_uv_delta) {
      cm->v_dc_delta_q = read_delta_q(rb);
      cm->v_ac_delta_q = read_delta_q(rb);
    } else {
      cm->v_dc_delta_q = cm->u_dc_delta_q;
      cm->v_ac_delta_q = cm->u_ac_delta_q;
    }
  }
  cm->dequant_bit_depth = cm->bit_depth;
  cm->using_qmatrix = aom_rb_read_bit(rb);
  if (cm->using_qmatrix) {
    cm->qm_y = aom_rb_read_literal(rb, QM_LEVEL_BITS);
    cm->qm_u = aom_rb_read_literal(rb, QM_LEVEL_BITS);
    if (!cm->separate_uv_delta_q)
      cm->qm_v = cm->qm_u;
    else
      cm->qm_v = aom_rb_read_literal(rb, QM_LEVEL_BITS);
  } else {
    cm->qm_y = 0;
    cm->qm_u = 0;
    cm->qm_v = 0;
  }
}

// Build y/uv dequant values based on segmentation.
static void setup_segmentation_dequant(AV1_COMMON *const cm) {
  const int using_qm = cm->using_qmatrix;
  // When segmentation is disabled, only the first value is used.  The
  // remaining are don't cares.
  const int max_segments = cm->seg.enabled ? MAX_SEGMENTS : 1;
  for (int i = 0; i < max_segments; ++i) {
    const int qindex = av1_get_qindex(&cm->seg, i, cm->base_qindex);
    cm->y_dequant_QTX[i][0] =
        av1_dc_quant_QTX(qindex, cm->y_dc_delta_q, cm->bit_depth);
    cm->y_dequant_QTX[i][1] = av1_ac_quant_QTX(qindex, 0, cm->bit_depth);
    cm->u_dequant_QTX[i][0] =
        av1_dc_quant_QTX(qindex, cm->u_dc_delta_q, cm->bit_depth);
    cm->u_dequant_QTX[i][1] =
        av1_ac_quant_QTX(qindex, cm->u_ac_delta_q, cm->bit_depth);
    cm->v_dequant_QTX[i][0] =
        av1_dc_quant_QTX(qindex, cm->v_dc_delta_q, cm->bit_depth);
    cm->v_dequant_QTX[i][1] =
        av1_ac_quant_QTX(qindex, cm->v_ac_delta_q, cm->bit_depth);
    const int lossless = qindex == 0 && cm->y_dc_delta_q == 0 &&
                         cm->u_dc_delta_q == 0 && cm->u_ac_delta_q == 0 &&
                         cm->v_dc_delta_q == 0 && cm->v_ac_delta_q == 0;
    // NB: depends on base index so there is only 1 set per frame
    // No quant weighting when lossless or signalled not using QM
    int qmlevel = (lossless || using_qm == 0) ? NUM_QM_LEVELS - 1 : cm->qm_y;
    for (int j = 0; j < TX_SIZES_ALL; ++j) {
      cm->y_iqmatrix[i][j] = av1_iqmatrix(cm, qmlevel, AOM_PLANE_Y, j);
    }
    qmlevel = (lossless || using_qm == 0) ? NUM_QM_LEVELS - 1 : cm->qm_u;
    for (int j = 0; j < TX_SIZES_ALL; ++j) {
      cm->u_iqmatrix[i][j] = av1_iqmatrix(cm, qmlevel, AOM_PLANE_U, j);
    }
    qmlevel = (lossless || using_qm == 0) ? NUM_QM_LEVELS - 1 : cm->qm_v;
    for (int j = 0; j < TX_SIZES_ALL; ++j) {
      cm->v_iqmatrix[i][j] = av1_iqmatrix(cm, qmlevel, AOM_PLANE_V, j);
    }
  }
}

static InterpFilter read_frame_interp_filter(struct aom_read_bit_buffer *rb) {
  return aom_rb_read_bit(rb) ? SWITCHABLE
                             : aom_rb_read_literal(rb, LOG_SWITCHABLE_FILTERS);
}

static void setup_render_size(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  cm->render_width = cm->superres_upscaled_width;
  cm->render_height = cm->superres_upscaled_height;
  if (aom_rb_read_bit(rb))
    av1_read_frame_size(rb, 16, 16, &cm->render_width, &cm->render_height);
}

// TODO(afergs): make "struct aom_read_bit_buffer *const rb"?
static void setup_superres(AV1_COMMON *const cm, struct aom_read_bit_buffer *rb,
                           int *width, int *height) {
  cm->superres_upscaled_width = *width;
  cm->superres_upscaled_height = *height;

  const SequenceHeader *const seq_params = &cm->seq_params;
  if (!seq_params->enable_superres) return;

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

static void setup_frame_size(AV1_COMMON *cm, int frame_size_override_flag,
                             struct aom_read_bit_buffer *rb) {
  int width, height;
  BufferPool *const pool = cm->buffer_pool;

  if (frame_size_override_flag) {
    int num_bits_width = cm->seq_params.num_bits_width;
    int num_bits_height = cm->seq_params.num_bits_height;
    av1_read_frame_size(rb, num_bits_width, num_bits_height, &width, &height);
    if (width > cm->seq_params.max_frame_width ||
        height > cm->seq_params.max_frame_height) {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Frame dimensions are larger than the maximum values");
    }
  } else {
    width = cm->seq_params.max_frame_width;
    height = cm->seq_params.max_frame_height;
  }

  setup_superres(cm, rb, &width, &height);
  resize_context_buffers(cm, width, height);
  setup_render_size(cm, rb);

  lock_buffer_pool(pool);
  if (aom_realloc_frame_buffer(
          get_frame_new_buffer(cm), cm->width, cm->height, cm->subsampling_x,
          cm->subsampling_y, cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
          cm->byte_alignment,
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
  pool->frame_bufs[cm->new_fb_idx].buf.color_primaries = cm->color_primaries;
  pool->frame_bufs[cm->new_fb_idx].buf.transfer_characteristics =
      cm->transfer_characteristics;
  pool->frame_bufs[cm->new_fb_idx].buf.matrix_coefficients =
      cm->matrix_coefficients;
  pool->frame_bufs[cm->new_fb_idx].buf.monochrome = cm->seq_params.monochrome;
  pool->frame_bufs[cm->new_fb_idx].buf.chroma_sample_position =
      cm->chroma_sample_position;
  pool->frame_bufs[cm->new_fb_idx].buf.color_range = cm->color_range;
  pool->frame_bufs[cm->new_fb_idx].buf.render_width = cm->render_width;
  pool->frame_bufs[cm->new_fb_idx].buf.render_height = cm->render_height;
}

static void setup_sb_size(SequenceHeader *seq_params,
                          struct aom_read_bit_buffer *rb) {
  set_sb_size(seq_params, aom_rb_read_bit(rb) ? BLOCK_128X128 : BLOCK_64X64);
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
  int found = 0;
  int has_valid_ref_frame = 0;
  BufferPool *const pool = cm->buffer_pool;
  for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    if (aom_rb_read_bit(rb)) {
      YV12_BUFFER_CONFIG *const buf = cm->frame_refs[i].buf;
      width = buf->y_crop_width;
      height = buf->y_crop_height;
      cm->render_width = buf->render_width;
      cm->render_height = buf->render_height;
      setup_superres(cm, rb, &width, &height);
      resize_context_buffers(cm, width, height);
      found = 1;
      break;
    }
  }

  if (!found) {
    int num_bits_width = cm->seq_params.num_bits_width;
    int num_bits_height = cm->seq_params.num_bits_height;

    av1_read_frame_size(rb, num_bits_width, num_bits_height, &width, &height);
    setup_superres(cm, rb, &width, &height);
    resize_context_buffers(cm, width, height);
    setup_render_size(cm, rb);
  }

  if (width <= 0 || height <= 0)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Invalid frame size");

  // Check to make sure at least one of frames that this frame references
  // has valid dimensions.
  for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    RefBuffer *const ref_frame = &cm->frame_refs[i];
    has_valid_ref_frame |=
        valid_ref_frame_size(ref_frame->buf->y_crop_width,
                             ref_frame->buf->y_crop_height, width, height);
  }
  if (!has_valid_ref_frame)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Referenced frame has invalid size");
  for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    RefBuffer *const ref_frame = &cm->frame_refs[i];
    if (!valid_ref_frame_img_fmt(ref_frame->buf->bit_depth,
                                 ref_frame->buf->subsampling_x,
                                 ref_frame->buf->subsampling_y, cm->bit_depth,
                                 cm->subsampling_x, cm->subsampling_y))
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Referenced frame has incompatible color format");
  }

  lock_buffer_pool(pool);
  if (aom_realloc_frame_buffer(
          get_frame_new_buffer(cm), cm->width, cm->height, cm->subsampling_x,
          cm->subsampling_y, cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
          cm->byte_alignment,
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
  pool->frame_bufs[cm->new_fb_idx].buf.color_primaries = cm->color_primaries;
  pool->frame_bufs[cm->new_fb_idx].buf.transfer_characteristics =
      cm->transfer_characteristics;
  pool->frame_bufs[cm->new_fb_idx].buf.matrix_coefficients =
      cm->matrix_coefficients;
  pool->frame_bufs[cm->new_fb_idx].buf.monochrome = cm->seq_params.monochrome;
  pool->frame_bufs[cm->new_fb_idx].buf.chroma_sample_position =
      cm->chroma_sample_position;
  pool->frame_bufs[cm->new_fb_idx].buf.color_range = cm->color_range;
  pool->frame_bufs[cm->new_fb_idx].buf.render_width = cm->render_width;
  pool->frame_bufs[cm->new_fb_idx].buf.render_height = cm->render_height;
}

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
  int width_mi = ALIGN_POWER_OF_TWO(cm->mi_cols, cm->seq_params.mib_size_log2);
  int height_mi = ALIGN_POWER_OF_TWO(cm->mi_rows, cm->seq_params.mib_size_log2);
  int width_sb = width_mi >> cm->seq_params.mib_size_log2;
  int height_sb = height_mi >> cm->seq_params.mib_size_log2;

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
    int i;
    int start_sb;
    for (i = 0, start_sb = 0; width_sb > 0 && i < MAX_TILE_COLS; i++) {
      const int size_sb =
          1 + rb_read_uniform(rb, AOMMIN(width_sb, cm->max_tile_width_sb));
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
    int i;
    int start_sb;
    for (i = 0, start_sb = 0; height_sb > 0 && i < MAX_TILE_ROWS; i++) {
      const int size_sb =
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

void av1_set_single_tile_decoding_mode(AV1_COMMON *const cm) {
  cm->single_tile_decoding = 0;
  if (cm->large_scale_tile) {
    struct loopfilter *lf = &cm->lf;

    // Figure out single_tile_decoding by loopfilter_level.
    const int no_loopfilter = !(lf->filter_level[0] || lf->filter_level[1]);
    const int no_cdef = cm->cdef_bits == 0 && cm->cdef_strengths[0] == 0 &&
                        cm->cdef_uv_strengths[0] == 0;
    const int no_restoration =
        cm->rst_info[0].frame_restoration_type == RESTORE_NONE &&
        cm->rst_info[1].frame_restoration_type == RESTORE_NONE &&
        cm->rst_info[2].frame_restoration_type == RESTORE_NONE;
    assert(IMPLIES(cm->coded_lossless, no_loopfilter && no_cdef));
    assert(IMPLIES(cm->all_lossless, no_restoration));
    cm->single_tile_decoding = no_loopfilter && no_cdef && no_restoration;
  }
}

static void read_tile_info(AV1Decoder *const pbi,
                           struct aom_read_bit_buffer *const rb) {
  AV1_COMMON *const cm = &pbi->common;

  read_tile_info_max_tile(cm, rb);

  cm->context_update_tile_id = 0;
  if (cm->tile_rows * cm->tile_cols > 1) {
    // tile to use for cdf update
    cm->context_update_tile_id =
        aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);
    // tile size magnitude
    pbi->tile_size_bytes = aom_rb_read_literal(rb, 2) + 1;
  }
}

#if EXT_TILE_DEBUG
static void read_ext_tile_info(AV1Decoder *const pbi,
                               struct aom_read_bit_buffer *const rb) {
  AV1_COMMON *const cm = &pbi->common;

  // This information is stored as a separate byte.
  int mod = rb->bit_offset % CHAR_BIT;
  if (mod > 0) aom_rb_read_literal(rb, CHAR_BIT - mod);
  assert(rb->bit_offset % CHAR_BIT == 0);

  if (cm->tile_cols * cm->tile_rows > 1) {
    // Read the number of bytes used to store tile size
    pbi->tile_col_size_bytes = aom_rb_read_literal(rb, 2) + 1;
    pbi->tile_size_bytes = aom_rb_read_literal(rb, 2) + 1;
  }
}
#endif  // EXT_TILE_DEBUG

static size_t mem_get_varsize(const uint8_t *src, int sz) {
  switch (sz) {
    case 1: return src[0];
    case 2: return mem_get_le16(src);
    case 3: return mem_get_le24(src);
    case 4: return mem_get_le32(src);
    default: assert(0 && "Invalid size"); return -1;
  }
}

#if EXT_TILE_DEBUG
// Reads the next tile returning its size and adjusting '*data' accordingly
// based on 'is_last'. On return, '*data' is updated to point to the end of the
// raw tile buffer in the bit stream.
static void get_ls_tile_buffer(
    const uint8_t *const data_end, struct aom_internal_error_info *error_info,
    const uint8_t **data, TileBufferDec (*const tile_buffers)[MAX_TILE_COLS],
    int tile_size_bytes, int col, int row, int tile_copy_mode) {
  size_t size;

  size_t copy_size = 0;
  const uint8_t *copy_data = NULL;

  if (!read_is_valid(*data, tile_size_bytes, data_end))
    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile length");
  size = mem_get_varsize(*data, tile_size_bytes);

  // If tile_copy_mode = 1, then the top bit of the tile header indicates copy
  // mode.
  if (tile_copy_mode && (size >> (tile_size_bytes * 8 - 1)) == 1) {
    // The remaining bits in the top byte signal the row offset
    int offset = (size >> (tile_size_bytes - 1) * 8) & 0x7f;

    // Currently, only use tiles in same column as reference tiles.
    copy_data = tile_buffers[row - offset][col].data;
    copy_size = tile_buffers[row - offset][col].size;
    size = 0;
  } else {
    size += AV1_MIN_TILE_SIZE_BYTES;
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
}

// Returns the end of the last tile buffer
// (tile_buffers[cm->tile_rows - 1][cm->tile_cols - 1]).
static const uint8_t *get_ls_tile_buffers(
    AV1Decoder *pbi, const uint8_t *data, const uint8_t *data_end,
    TileBufferDec (*const tile_buffers)[MAX_TILE_COLS]) {
  AV1_COMMON *const cm = &pbi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  const int have_tiles = tile_cols * tile_rows > 1;
  const uint8_t *raw_data_end;  // The end of the last tile buffer

  if (!have_tiles) {
    const size_t tile_size = data_end - data;
    tile_buffers[0][0].data = data;
    tile_buffers[0][0].size = tile_size;
    raw_data_end = NULL;
  } else {
    // We locate only the tile buffers that are required, which are the ones
    // specified by pbi->dec_tile_col and pbi->dec_tile_row. Also, we always
    // need the last (bottom right) tile buffer, as we need to know where the
    // end of the compressed frame buffer is for proper superframe decoding.

    const uint8_t *tile_col_data_end[MAX_TILE_COLS] = { NULL };
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
    // Read tile column sizes for all columns (we need the last tile buffer)
    for (int c = 0; c < tile_cols; ++c) {
      const int is_last = c == tile_cols - 1;
      size_t tile_col_size;

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
    for (int c = tile_cols_start; c < tile_cols_end; ++c) {
      const int is_last = c == tile_cols - 1;

      if (c > 0) data = tile_col_data_end[c - 1];

      if (!is_last) data += tile_col_size_bytes;

      // Get the whole of the last column, otherwise stop at the required tile.
      for (int r = 0; r < (is_last ? tile_rows : tile_rows_end); ++r) {
        get_ls_tile_buffer(tile_col_data_end[c], &pbi->common.error, &data,
                           tile_buffers, tile_size_bytes, c, r, tile_copy_mode);
      }
    }

    // If we have not read the last column, then read it to get the last tile.
    if (tile_cols_end != tile_cols) {
      const int c = tile_cols - 1;

      data = tile_col_data_end[c - 1];

      for (int r = 0; r < tile_rows; ++r) {
        get_ls_tile_buffer(tile_col_data_end[c], &pbi->common.error, &data,
                           tile_buffers, tile_size_bytes, c, r, tile_copy_mode);
      }
    }
    raw_data_end = data;
  }
  return raw_data_end;
}
#endif  // EXT_TILE_DEBUG

static const uint8_t *get_ls_single_tile_buffer(
    AV1Decoder *pbi, const uint8_t *data,
    TileBufferDec (*const tile_buffers)[MAX_TILE_COLS]) {
  assert(pbi->dec_tile_row >= 0 && pbi->dec_tile_col >= 0);
  tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].data = data;
  tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].size =
      (size_t)pbi->coded_tile_data_size;
  return data + pbi->coded_tile_data_size;
}

// Reads the next tile returning its size and adjusting '*data' accordingly
// based on 'is_last'.
static void get_tile_buffer(const uint8_t *const data_end,
                            const int tile_size_bytes, int is_last,
                            struct aom_internal_error_info *error_info,
                            const uint8_t **data, TileBufferDec *const buf) {
  size_t size;

  if (!is_last) {
    if (!read_is_valid(*data, tile_size_bytes, data_end))
      aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Truncated packet or corrupt tile length");

    size = mem_get_varsize(*data, tile_size_bytes) + AV1_MIN_TILE_SIZE_BYTES;
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
                             int start_tile, int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int tc = 0;
  int first_tile_in_tg = 0;

  for (int r = 0; r < tile_rows; ++r) {
    for (int c = 0; c < tile_cols; ++c, ++tc) {
      TileBufferDec *const buf = &tile_buffers[r][c];

      const int is_last = (tc == end_tile);
      const size_t hdr_offset = 0;

      if (tc < start_tile || tc > end_tile) continue;

      if (data + hdr_offset >= data_end)
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Data ended before all tiles were read.");
      first_tile_in_tg += tc == first_tile_in_tg ? pbi->tg_size : 0;
      data += hdr_offset;
      get_tile_buffer(data_end, pbi->tile_size_bytes, is_last,
                      &pbi->common.error, &data, buf);
    }
  }
}

static void set_cb_buffer(MACROBLOCKD *const xd, CB_BUFFER *cb_buffer,
                          const int num_planes) {
  for (int plane = 0; plane < num_planes; ++plane) {
    xd->plane[plane].dqcoeff_block = cb_buffer->dqcoeff[plane];
    xd->plane[plane].eob_data = cb_buffer->eob_data[plane];
    xd->cb_offset[plane] = 0;
    xd->txb_offset[plane] = 0;
  }
  xd->plane[0].color_index_map = cb_buffer->color_index_map[0];
  xd->plane[1].color_index_map = cb_buffer->color_index_map[1];
  xd->color_index_map_offset[0] = 0;
  xd->color_index_map_offset[1] = 0;
}

static void decode_tile_sb_row(AV1Decoder *pbi, ThreadData *const td,
                               TileInfo tile_info, const int mi_row) {
  AV1_COMMON *const cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);
  av1_zero_left_context(&td->xd);

  for (int mi_col = tile_info.mi_col_start; mi_col < tile_info.mi_col_end;
       mi_col += cm->seq_params.mib_size) {
    set_cb_buffer(&td->xd, &td->cb_buffer_base, num_planes);

    decode_partition(pbi, &td->xd, mi_row, mi_col, td->bit_reader,
                     cm->seq_params.sb_size);
  }
}

static int check_trailing_bits_after_symbol_coder(aom_reader *r) {
  uint32_t nb_bits = aom_reader_tell(r);
  uint32_t nb_bytes = (nb_bits + 7) >> 3;

  const uint8_t *p_begin = aom_reader_find_begin(r);
  const uint8_t *p_end = aom_reader_find_end(r);

  // It is legal to have no padding bytes (nb_bytes == p_end - p_begin).
  if ((ptrdiff_t)nb_bytes > p_end - p_begin) return -1;
  const uint8_t *p = p_begin + nb_bytes;

  // aom_reader_tell() returns 1 for a newly initialized decoder, and the
  // return value only increases as values are decoded. So nb_bits > 0, and
  // thus p > p_begin. Therefore accessing p[-1] is safe.
  uint8_t last_byte = p[-1];
  uint8_t pattern = 128 >> ((nb_bits - 1) & 7);
  if ((last_byte & (2 * pattern - 1)) != pattern) return -1;

  // Make sure that all padding bytes are zero as required by the spec.
  while (p < p_end) {
    if (*p != 0) return -1;
    p++;
  }
  return 0;
}

static void decode_tile(AV1Decoder *pbi, ThreadData *const td, int tile_row,
                        int tile_col) {
  TileInfo tile_info;

  AV1_COMMON *const cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);

  av1_tile_set_row(&tile_info, cm, tile_row);
  av1_tile_set_col(&tile_info, cm, tile_col);
  av1_zero_above_context(cm, tile_info.mi_col_start, tile_info.mi_col_end,
                         tile_row);
  av1_reset_loop_restoration(&td->xd, num_planes);

  for (int mi_row = tile_info.mi_row_start; mi_row < tile_info.mi_row_end;
       mi_row += cm->seq_params.mib_size) {
    decode_tile_sb_row(pbi, td, tile_info, mi_row);
  }

  int corrupted =
      (check_trailing_bits_after_symbol_coder(td->bit_reader)) ? 1 : 0;
  aom_merge_corrupted_flag(&td->xd.corrupted, corrupted);
}

static const uint8_t *decode_tiles(AV1Decoder *pbi, const uint8_t *data,
                                   const uint8_t *data_end, int start_tile,
                                   int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  const int n_tiles = tile_cols * tile_rows;
  TileBufferDec(*const tile_buffers)[MAX_TILE_COLS] = pbi->tile_buffers;
  const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
  const int single_row = pbi->dec_tile_row >= 0;
  const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
  const int single_col = pbi->dec_tile_col >= 0;
  int tile_rows_start;
  int tile_rows_end;
  int tile_cols_start;
  int tile_cols_end;
  int inv_col_order;
  int inv_row_order;
  int tile_row, tile_col;
  uint8_t allow_update_cdf;
  const uint8_t *raw_data_end = NULL;

  if (cm->large_scale_tile) {
    tile_rows_start = single_row ? dec_tile_row : 0;
    tile_rows_end = single_row ? dec_tile_row + 1 : tile_rows;
    tile_cols_start = single_col ? dec_tile_col : 0;
    tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;
    inv_col_order = pbi->inv_tile_order && !single_col;
    inv_row_order = pbi->inv_tile_order && !single_row;
    allow_update_cdf = 0;
  } else {
    tile_rows_start = 0;
    tile_rows_end = tile_rows;
    tile_cols_start = 0;
    tile_cols_end = tile_cols;
    inv_col_order = pbi->inv_tile_order;
    inv_row_order = pbi->inv_tile_order;
    allow_update_cdf = 1;
  }

  // No tiles to decode.
  if (tile_rows_end <= tile_rows_start || tile_cols_end <= tile_cols_start ||
      // First tile is larger than end_tile.
      tile_rows_start * cm->tile_cols + tile_cols_start > end_tile ||
      // Last tile is smaller than start_tile.
      (tile_rows_end - 1) * cm->tile_cols + tile_cols_end - 1 < start_tile)
    return data;

  allow_update_cdf = allow_update_cdf && !cm->disable_cdf_update;

  assert(tile_rows <= MAX_TILE_ROWS);
  assert(tile_cols <= MAX_TILE_COLS);

#if EXT_TILE_DEBUG
  if (cm->large_scale_tile && !pbi->ext_tile_debug)
    raw_data_end = get_ls_single_tile_buffer(pbi, data, tile_buffers);
  else if (cm->large_scale_tile && pbi->ext_tile_debug)
    raw_data_end = get_ls_tile_buffers(pbi, data, data_end, tile_buffers);
  else
#endif  // EXT_TILE_DEBUG
    get_tile_buffers(pbi, data, data_end, tile_buffers, start_tile, end_tile);

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
  // Load all tile information into thread_data.
  for (tile_row = tile_rows_start; tile_row < tile_rows_end; ++tile_row) {
    const int row = inv_row_order ? tile_rows - 1 - tile_row : tile_row;

    for (tile_col = tile_cols_start; tile_col < tile_cols_end; ++tile_col) {
      const int col = inv_col_order ? tile_cols - 1 - tile_col : tile_col;
      ThreadData *const td = &pbi->td;
      TileDataDec *const tile_data = pbi->tile_data + row * cm->tile_cols + col;
      const TileBufferDec *const tile_bs_buf = &tile_buffers[row][col];

      if (row * cm->tile_cols + col < start_tile ||
          row * cm->tile_cols + col > end_tile)
        continue;

      td->xd = pbi->mb;
      td->xd.corrupted = 0;
      td->xd.mc_buf[0] = pbi->td.mc_buf[0];
      td->xd.mc_buf[1] = pbi->td.mc_buf[1];
      td->bit_reader = &tile_data->bit_reader;
      av1_zero(td->dqcoeff);
      av1_tile_init(&td->xd.tile, cm, row, col);
      setup_bool_decoder(tile_bs_buf->data, data_end, tile_bs_buf->size,
                         &cm->error, td->bit_reader, allow_update_cdf);
#if CONFIG_ACCOUNTING
      if (pbi->acct_enabled) {
        td->bit_reader->accounting = &pbi->accounting;
        td->bit_reader->accounting->last_tell_frac =
            aom_reader_tell_frac(td->bit_reader);
      } else {
        td->bit_reader->accounting = NULL;
      }
#endif
      av1_init_macroblockd(cm, &td->xd, td->dqcoeff);
      av1_init_above_context(cm, &td->xd, row);

      // Initialise the tile context from the frame context
      tile_data->tctx = *cm->fc;
      td->xd.tile_ctx = &tile_data->tctx;

      // decode tile
      decode_tile(pbi, &pbi->td, row, col);
      aom_merge_corrupted_flag(&pbi->mb.corrupted, td->xd.corrupted);
      if (pbi->mb.corrupted)
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Failed to decode tile data");
    }
  }

  if (cm->large_scale_tile) {
    if (n_tiles == 1) {
      // Find the end of the single tile buffer
      return aom_reader_find_end(&pbi->tile_data->bit_reader);
    }
    // Return the end of the last tile buffer
    return raw_data_end;
  }
  TileDataDec *const tile_data = pbi->tile_data + end_tile;

  return aom_reader_find_end(&tile_data->bit_reader);
}

static TileJobsDec *get_dec_job_info(AV1DecTileMT *tile_mt_info) {
  TileJobsDec *cur_job_info = NULL;
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(tile_mt_info->job_mutex);

  if (tile_mt_info->jobs_dequeued < tile_mt_info->jobs_enqueued) {
    cur_job_info = tile_mt_info->job_queue + tile_mt_info->jobs_dequeued;
    tile_mt_info->jobs_dequeued++;
  }

  pthread_mutex_unlock(tile_mt_info->job_mutex);
#else
  (void)tile_mt_info;
#endif
  return cur_job_info;
}

static int tile_worker_hook(void *arg1, void *arg2) {
  DecWorkerData *const thread_data = (DecWorkerData *)arg1;
  AV1Decoder *const pbi = (AV1Decoder *)arg2;
  AV1_COMMON *cm = &pbi->common;
  ThreadData *const td = thread_data->td;
  uint8_t allow_update_cdf;

  if (setjmp(thread_data->error_info.jmp)) {
    thread_data->error_info.setjmp = 0;
    thread_data->td->xd.corrupted = 1;
    return 0;
  }
  allow_update_cdf = cm->large_scale_tile ? 0 : 1;
  allow_update_cdf = allow_update_cdf && !cm->disable_cdf_update;

  assert(cm->tile_cols > 0);
  while (1) {
    TileJobsDec *cur_job_info = get_dec_job_info(&pbi->tile_mt_info);

    if (cur_job_info != NULL && !td->xd.corrupted) {
      const TileBufferDec *const tile_buffer = cur_job_info->tile_buffer;
      TileDataDec *const tile_data = cur_job_info->tile_data;
      volatile int tile_row = tile_data->tile_info.tile_row;
      volatile int tile_col = tile_data->tile_info.tile_col;

      td->xd = pbi->mb;
      td->xd.corrupted = 0;
      td->xd.mc_buf[0] = td->mc_buf[0];
      td->xd.mc_buf[1] = td->mc_buf[1];
      td->bit_reader = &tile_data->bit_reader;
      av1_zero(td->dqcoeff);
      av1_tile_init(&td->xd.tile, cm, tile_row, tile_col);
      setup_bool_decoder(tile_buffer->data, thread_data->data_end,
                         tile_buffer->size, &cm->error, td->bit_reader,
                         allow_update_cdf);
#if CONFIG_ACCOUNTING
      if (pbi->acct_enabled) {
        td->bit_reader->accounting = &pbi->accounting;
        td->bit_reader->accounting->last_tell_frac =
            aom_reader_tell_frac(td->bit_reader);
      } else {
        td->bit_reader->accounting = NULL;
      }
#endif
      av1_init_macroblockd(cm, &td->xd, td->dqcoeff);
      av1_init_above_context(cm, &td->xd, tile_row);

      // Initialise the tile context from the frame context
      tile_data->tctx = *cm->fc;
      td->xd.tile_ctx = &tile_data->tctx;
#if CONFIG_ACCOUNTING
      if (pbi->acct_enabled) {
        tile_data->bit_reader.accounting->last_tell_frac =
            aom_reader_tell_frac(&tile_data->bit_reader);
      }
#endif
      // decode tile
      decode_tile(pbi, td, tile_row, tile_col);
    } else {
      break;
    }
  }
  return !td->xd.corrupted;
}

// sorts in descending order
static int compare_tile_buffers(const void *a, const void *b) {
  const TileJobsDec *const buf1 = (const TileJobsDec *)a;
  const TileJobsDec *const buf2 = (const TileJobsDec *)b;
  return (((int)buf2->tile_buffer->size) - ((int)buf1->tile_buffer->size));
}

static void enqueue_tile_jobs(AV1Decoder *pbi, AV1_COMMON *cm,
                              int tile_rows_start, int tile_rows_end,
                              int tile_cols_start, int tile_cols_end,
                              int startTile, int endTile) {
  AV1DecTileMT *tile_mt_info = &pbi->tile_mt_info;
  TileJobsDec *tile_job_queue = tile_mt_info->job_queue;
  tile_mt_info->jobs_enqueued = 0;
  tile_mt_info->jobs_dequeued = 0;

  for (int row = tile_rows_start; row < tile_rows_end; row++) {
    for (int col = tile_cols_start; col < tile_cols_end; col++) {
      if (row * cm->tile_cols + col < startTile ||
          row * cm->tile_cols + col > endTile)
        continue;
      tile_job_queue->tile_buffer = &pbi->tile_buffers[row][col];
      tile_job_queue->tile_data = pbi->tile_data + row * cm->tile_cols + col;
      tile_job_queue++;
      tile_mt_info->jobs_enqueued++;
    }
  }
}

static void alloc_dec_jobs(AV1DecTileMT *tile_mt_info, AV1_COMMON *cm,
                           int tile_rows, int tile_cols) {
  tile_mt_info->alloc_tile_rows = tile_rows;
  tile_mt_info->alloc_tile_cols = tile_cols;
  int num_tiles = tile_rows * tile_cols;
#if CONFIG_MULTITHREAD
  {
    CHECK_MEM_ERROR(cm, tile_mt_info->job_mutex,
                    aom_malloc(sizeof(*tile_mt_info->job_mutex) * num_tiles));

    for (int i = 0; i < num_tiles; i++) {
      pthread_mutex_init(&tile_mt_info->job_mutex[i], NULL);
    }
  }
#endif
  CHECK_MEM_ERROR(cm, tile_mt_info->job_queue,
                  aom_malloc(sizeof(*tile_mt_info->job_queue) * num_tiles));
}

void av1_free_mc_tmp_buf(void *td, int use_highbd) {
  ThreadData *thread_data = (ThreadData *)td;
  int ref;
  for (ref = 0; ref < 2; ref++) {
    if (use_highbd)
      aom_free(CONVERT_TO_SHORTPTR(thread_data->mc_buf[ref]));
    else
      aom_free(thread_data->mc_buf[ref]);
    thread_data->mc_buf[ref] = NULL;
  }
  thread_data->mc_buf_size = 0;
}

static void allocate_mc_tmp_buf(AV1_COMMON *const cm, void *td, int buf_size,
                                int use_highbd) {
  ThreadData *thread_data = (ThreadData *)td;

  for (int ref = 0; ref < 2; ref++) {
    if (use_highbd) {
      uint16_t *hbd_mc_buf;
      CHECK_MEM_ERROR(cm, hbd_mc_buf, (uint16_t *)aom_memalign(16, buf_size));
      thread_data->mc_buf[ref] = CONVERT_TO_BYTEPTR(hbd_mc_buf);
    } else {
      CHECK_MEM_ERROR(cm, thread_data->mc_buf[ref],
                      (uint8_t *)aom_memalign(16, buf_size));
    }
  }
  thread_data->mc_buf_size = buf_size;
}

static const uint8_t *decode_tiles_mt(AV1Decoder *pbi, const uint8_t *data,
                                      const uint8_t *data_end, int start_tile,
                                      int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  const int n_tiles = tile_cols * tile_rows;
  TileBufferDec(*const tile_buffers)[MAX_TILE_COLS] = pbi->tile_buffers;
  const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
  const int single_row = pbi->dec_tile_row >= 0;
  const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
  const int single_col = pbi->dec_tile_col >= 0;
  int tile_rows_start;
  int tile_rows_end;
  int tile_cols_start;
  int tile_cols_end;
  int tile_count_tg;
  int num_workers;
  int worker_idx;
  const uint8_t *raw_data_end = NULL;

  if (cm->large_scale_tile) {
    tile_rows_start = single_row ? dec_tile_row : 0;
    tile_rows_end = single_row ? dec_tile_row + 1 : tile_rows;
    tile_cols_start = single_col ? dec_tile_col : 0;
    tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;
  } else {
    tile_rows_start = 0;
    tile_rows_end = tile_rows;
    tile_cols_start = 0;
    tile_cols_end = tile_cols;
  }
  tile_count_tg = end_tile - start_tile + 1;
  num_workers = AOMMIN(pbi->max_threads, tile_count_tg);

  // No tiles to decode.
  if (tile_rows_end <= tile_rows_start || tile_cols_end <= tile_cols_start ||
      // First tile is larger than end_tile.
      tile_rows_start * tile_cols + tile_cols_start > end_tile ||
      // Last tile is smaller than start_tile.
      (tile_rows_end - 1) * tile_cols + tile_cols_end - 1 < start_tile)
    return data;

  assert(tile_rows <= MAX_TILE_ROWS);
  assert(tile_cols <= MAX_TILE_COLS);
  assert(tile_count_tg > 0);
  assert(num_workers > 0);
  assert(start_tile <= end_tile);
  assert(start_tile >= 0 && end_tile < n_tiles);

  // Create workers and thread_data
  if (pbi->num_workers == 0) {
    const int num_threads = pbi->max_threads;
    CHECK_MEM_ERROR(cm, pbi->tile_workers,
                    aom_malloc(num_threads * sizeof(*pbi->tile_workers)));
    CHECK_MEM_ERROR(cm, pbi->thread_data,
                    aom_malloc(num_threads * sizeof(*pbi->thread_data)));

    for (worker_idx = 0; worker_idx < num_threads; ++worker_idx) {
      AVxWorker *const worker = &pbi->tile_workers[worker_idx];
      DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
      ++pbi->num_workers;

      winterface->init(worker);
      if (worker_idx < num_threads - 1 && !winterface->reset(worker)) {
        aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                           "Tile decoder thread creation failed");
      }

      if (worker_idx < num_threads - 1) {
        // Allocate thread data.
        CHECK_MEM_ERROR(cm, thread_data->td,
                        aom_memalign(32, sizeof(*thread_data->td)));
        av1_zero(*thread_data->td);
      } else {
        // Main thread acts as a worker and uses the thread data in pbi
        thread_data->td = &pbi->td;
      }
    }
  }
  const int use_highbd = cm->use_highbitdepth ? 1 : 0;
  const int buf_size = MC_TEMP_BUF_PELS << use_highbd;
  for (worker_idx = 0; worker_idx < pbi->max_threads - 1; ++worker_idx) {
    DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
    if (thread_data->td->mc_buf_size != buf_size) {
      av1_free_mc_tmp_buf(thread_data->td, use_highbd);
      allocate_mc_tmp_buf(cm, thread_data->td, buf_size, use_highbd);
    }
  }

    // get tile size in tile group
#if EXT_TILE_DEBUG
  if (cm->large_scale_tile)
    raw_data_end = get_ls_tile_buffers(pbi, data, data_end, tile_buffers);
  else
#endif  // EXT_TILE_DEBUG
    get_tile_buffers(pbi, data, data_end, tile_buffers, start_tile, end_tile);

  if (pbi->tile_data == NULL || n_tiles != pbi->allocated_tiles) {
    aom_free(pbi->tile_data);
    CHECK_MEM_ERROR(cm, pbi->tile_data,
                    aom_memalign(32, n_tiles * sizeof(*pbi->tile_data)));
    pbi->allocated_tiles = n_tiles;
  }

  // Reset tile decoding hook
  for (worker_idx = 0; worker_idx < num_workers; ++worker_idx) {
    AVxWorker *const worker = &pbi->tile_workers[worker_idx];
    DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
    winterface->sync(worker);

    worker->hook = tile_worker_hook;
    worker->data1 = thread_data;
    worker->data2 = pbi;
  }
#if CONFIG_ACCOUNTING
  if (pbi->acct_enabled) {
    aom_accounting_reset(&pbi->accounting);
  }
#endif
  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      TileDataDec *tile_data = pbi->tile_data + row * cm->tile_cols + col;
      av1_tile_init(&tile_data->tile_info, cm, row, col);
    }
  }

  if (pbi->tile_mt_info.alloc_tile_cols != tile_cols ||
      pbi->tile_mt_info.alloc_tile_rows != tile_rows) {
    av1_dealloc_dec_jobs(&pbi->tile_mt_info);
    alloc_dec_jobs(&pbi->tile_mt_info, cm, tile_rows, tile_cols);
  }
  enqueue_tile_jobs(pbi, cm, tile_rows_start, tile_rows_end, tile_cols_start,
                    tile_cols_end, start_tile, end_tile);
  qsort(pbi->tile_mt_info.job_queue, pbi->tile_mt_info.jobs_enqueued,
        sizeof(pbi->tile_mt_info.job_queue[0]), compare_tile_buffers);

  {
    const int base = tile_count_tg / num_workers;
    const int remain = tile_count_tg % num_workers;
    int tile_start = start_tile;
    int corrupted = 0;

    for (worker_idx = 0; worker_idx < num_workers; ++worker_idx) {
      // compute number of tiles assign to each worker
      const int count = base + (remain + worker_idx) / num_workers;
      AVxWorker *const worker = &pbi->tile_workers[worker_idx];
      DecWorkerData *const thread_data = (DecWorkerData *)worker->data1;

      thread_data->data_end = data_end;
      tile_start += count;

      worker->had_error = 0;
      if (worker_idx == num_workers - 1) {
        winterface->execute(worker);
      } else {
        winterface->launch(worker);
      }
    }

    for (; worker_idx > 0; --worker_idx) {
      AVxWorker *const worker = &pbi->tile_workers[worker_idx - 1];
      aom_merge_corrupted_flag(&corrupted, !winterface->sync(worker));
    }

    pbi->mb.corrupted = corrupted;
  }

  if (pbi->mb.corrupted)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Failed to decode tile data");

  if (cm->large_scale_tile) {
    if (n_tiles == 1) {
      // Find the end of the single tile buffer
      return aom_reader_find_end(&pbi->tile_data->bit_reader);
    }
    // Return the end of the last tile buffer
    return raw_data_end;
  }
  TileDataDec *const tile_data = pbi->tile_data + end_tile;

  return aom_reader_find_end(&tile_data->bit_reader);
}

static void error_handler(void *data) {
  AV1_COMMON *const cm = (AV1_COMMON *)data;
  aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME, "Truncated packet");
}

// Reads the high_bitdepth and twelve_bit fields in color_config() and sets
// cm->bit_depth based on the values of those fields and cm->profile. Reports
// errors by calling rb->error_handler() or aom_internal_error().
static void av1_read_bitdepth(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  const int high_bitdepth = aom_rb_read_bit(rb);
  if (cm->profile == PROFILE_2 && high_bitdepth) {
    const int twelve_bit = aom_rb_read_bit(rb);
    cm->bit_depth = twelve_bit ? AOM_BITS_12 : AOM_BITS_10;
  } else if (cm->profile <= PROFILE_2) {
    cm->bit_depth = high_bitdepth ? AOM_BITS_10 : AOM_BITS_8;
  } else {
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Unsupported profile/bit-depth combination");
  }
}

void av1_read_film_grain_params(AV1_COMMON *cm,
                                struct aom_read_bit_buffer *rb) {
  aom_film_grain_t *pars = &cm->film_grain_params;

  pars->apply_grain = aom_rb_read_bit(rb);
  if (!pars->apply_grain) {
    memset(pars, 0, sizeof(*pars));
    return;
  }

  pars->random_seed = aom_rb_read_literal(rb, 16);
  if (cm->frame_type == INTER_FRAME)
    pars->update_parameters = aom_rb_read_bit(rb);
  else
    pars->update_parameters = 1;

  if (!pars->update_parameters) {
    // inherit parameters from a previous reference frame
    RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
    int film_grain_params_ref_idx = aom_rb_read_literal(rb, 3);
    int buf_idx = cm->ref_frame_map[film_grain_params_ref_idx];
    if (buf_idx == INVALID_IDX) {
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Invalid Film grain reference idx");
    }
    if (!frame_bufs[buf_idx].film_grain_params_present) {
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Film grain reference parameters not available");
    }
    uint16_t random_seed = pars->random_seed;
    *pars = frame_bufs[buf_idx].film_grain_params;  // inherit paramaters
    pars->random_seed = random_seed;                // with new random seed
    return;
  }

  // Scaling functions parameters
  pars->num_y_points = aom_rb_read_literal(rb, 4);  // max 14
  if (pars->num_y_points > 14)
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Number of points for film grain luma scaling function "
                       "exceeds the maximum value.");
  for (int i = 0; i < pars->num_y_points; i++) {
    pars->scaling_points_y[i][0] = aom_rb_read_literal(rb, 8);
    if (i && pars->scaling_points_y[i - 1][0] >= pars->scaling_points_y[i][0])
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "First coordinate of the scaling function points "
                         "shall be increasing.");
    pars->scaling_points_y[i][1] = aom_rb_read_literal(rb, 8);
  }

  if (!cm->seq_params.monochrome)
    pars->chroma_scaling_from_luma = aom_rb_read_bit(rb);

  if (cm->seq_params.monochrome || pars->chroma_scaling_from_luma ||
      ((cm->subsampling_x == 1) && (cm->subsampling_y == 1) &&
       (pars->num_y_points == 0))) {
    pars->num_cb_points = 0;
    pars->num_cr_points = 0;
  } else {
    pars->num_cb_points = aom_rb_read_literal(rb, 4);  // max 10
    if (pars->num_cb_points > 10)
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Number of points for film grain cb scaling function "
                         "exceeds the maximum value.");
    for (int i = 0; i < pars->num_cb_points; i++) {
      pars->scaling_points_cb[i][0] = aom_rb_read_literal(rb, 8);
      if (i &&
          pars->scaling_points_cb[i - 1][0] >= pars->scaling_points_cb[i][0])
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "First coordinate of the scaling function points "
                           "shall be increasing.");
      pars->scaling_points_cb[i][1] = aom_rb_read_literal(rb, 8);
    }

    pars->num_cr_points = aom_rb_read_literal(rb, 4);  // max 10
    if (pars->num_cr_points > 10)
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Number of points for film grain cr scaling function "
                         "exceeds the maximum value.");
    for (int i = 0; i < pars->num_cr_points; i++) {
      pars->scaling_points_cr[i][0] = aom_rb_read_literal(rb, 8);
      if (i &&
          pars->scaling_points_cr[i - 1][0] >= pars->scaling_points_cr[i][0])
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "First coordinate of the scaling function points "
                           "shall be increasing.");
      pars->scaling_points_cr[i][1] = aom_rb_read_literal(rb, 8);
    }

    if ((cm->subsampling_x == 1) && (cm->subsampling_y == 1) &&
        (((pars->num_cb_points == 0) && (pars->num_cr_points != 0)) ||
         ((pars->num_cb_points != 0) && (pars->num_cr_points == 0))))
      aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "In YCbCr 4:2:0, film grain shall be applied "
                         "to both chroma components or neither.");
  }

  pars->scaling_shift = aom_rb_read_literal(rb, 2) + 8;  // 8 + value

  // AR coefficients
  // Only sent if the corresponsing scaling function has
  // more than 0 points

  pars->ar_coeff_lag = aom_rb_read_literal(rb, 2);

  int num_pos_luma = 2 * pars->ar_coeff_lag * (pars->ar_coeff_lag + 1);
  int num_pos_chroma = num_pos_luma;
  if (pars->num_y_points > 0) ++num_pos_chroma;

  if (pars->num_y_points)
    for (int i = 0; i < num_pos_luma; i++)
      pars->ar_coeffs_y[i] = aom_rb_read_literal(rb, 8) - 128;

  if (pars->num_cb_points || pars->chroma_scaling_from_luma)
    for (int i = 0; i < num_pos_chroma; i++)
      pars->ar_coeffs_cb[i] = aom_rb_read_literal(rb, 8) - 128;

  if (pars->num_cr_points || pars->chroma_scaling_from_luma)
    for (int i = 0; i < num_pos_chroma; i++)
      pars->ar_coeffs_cr[i] = aom_rb_read_literal(rb, 8) - 128;

  pars->ar_coeff_shift = aom_rb_read_literal(rb, 2) + 6;  // 6 + value

  pars->grain_scale_shift = aom_rb_read_literal(rb, 2);

  if (pars->num_cb_points) {
    pars->cb_mult = aom_rb_read_literal(rb, 8);
    pars->cb_luma_mult = aom_rb_read_literal(rb, 8);
    pars->cb_offset = aom_rb_read_literal(rb, 9);
  }

  if (pars->num_cr_points) {
    pars->cr_mult = aom_rb_read_literal(rb, 8);
    pars->cr_luma_mult = aom_rb_read_literal(rb, 8);
    pars->cr_offset = aom_rb_read_literal(rb, 9);
  }

  pars->overlap_flag = aom_rb_read_bit(rb);

  pars->clip_to_restricted_range = aom_rb_read_bit(rb);
}

static void read_film_grain(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  if (cm->film_grain_params_present && (cm->show_frame || cm->showable_frame)) {
    av1_read_film_grain_params(cm, rb);
  } else {
    memset(&cm->film_grain_params, 0, sizeof(cm->film_grain_params));
  }
  cm->film_grain_params.bit_depth = cm->bit_depth;
  memcpy(&cm->cur_frame->film_grain_params, &cm->film_grain_params,
         sizeof(aom_film_grain_t));
}

void av1_read_color_config(AV1_COMMON *cm, struct aom_read_bit_buffer *rb,
                           int allow_lowbitdepth) {
  av1_read_bitdepth(cm, rb);

  cm->use_highbitdepth = cm->bit_depth > AOM_BITS_8 || !allow_lowbitdepth;
  // monochrome bit (not needed for PROFILE_1)
  const int is_monochrome = cm->profile != PROFILE_1 ? aom_rb_read_bit(rb) : 0;
  cm->seq_params.monochrome = is_monochrome;
  int color_description_present_flag = aom_rb_read_bit(rb);
  if (color_description_present_flag) {
    cm->color_primaries = aom_rb_read_literal(rb, 8);
    cm->transfer_characteristics = aom_rb_read_literal(rb, 8);
    cm->matrix_coefficients = aom_rb_read_literal(rb, 8);
  } else {
    cm->color_primaries = AOM_CICP_CP_UNSPECIFIED;
    cm->transfer_characteristics = AOM_CICP_TC_UNSPECIFIED;
    cm->matrix_coefficients = AOM_CICP_MC_UNSPECIFIED;
  }
  if (is_monochrome) {
    // [16,235] (including xvycc) vs [0,255] range
    cm->color_range = aom_rb_read_bit(rb);
    cm->subsampling_y = cm->subsampling_x = 1;
    cm->chroma_sample_position = AOM_CSP_UNKNOWN;
    cm->separate_uv_delta_q = 0;
    return;
  }
  if (cm->color_primaries == AOM_CICP_CP_BT_709 &&
      cm->transfer_characteristics == AOM_CICP_TC_SRGB &&
      cm->matrix_coefficients == AOM_CICP_MC_IDENTITY) {  // it would be better
                                                          // to remove this
                                                          // dependency too
    cm->subsampling_y = cm->subsampling_x = 0;
    cm->color_range = 1;  // assume full color-range
    if (!(cm->profile == PROFILE_1 ||
          (cm->profile == PROFILE_2 && cm->bit_depth == AOM_BITS_12))) {
      aom_internal_error(
          &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
          "sRGB colorspace not compatible with specified profile");
    }
  } else {
    // [16,235] (including xvycc) vs [0,255] range
    cm->color_range = aom_rb_read_bit(rb);
    if (cm->profile == PROFILE_0) {
      // 420 only
      cm->subsampling_x = cm->subsampling_y = 1;
    } else if (cm->profile == PROFILE_1) {
      // 444 only
      cm->subsampling_x = cm->subsampling_y = 0;
    } else {
      assert(cm->profile == PROFILE_2);
      if (cm->bit_depth == AOM_BITS_12) {
        cm->subsampling_x = aom_rb_read_bit(rb);
        if (cm->subsampling_x)
          cm->subsampling_y = aom_rb_read_bit(rb);  // 422 or 420
        else
          cm->subsampling_y = 0;  // 444
      } else {
        // 422
        cm->subsampling_x = 1;
        cm->subsampling_y = 0;
      }
    }
    if (cm->matrix_coefficients == AOM_CICP_MC_IDENTITY &&
        (cm->subsampling_x || cm->subsampling_y)) {
      aom_internal_error(
          &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
          "Identity CICP Matrix incompatible with non 4:4:4 color sampling");
    }
    if (cm->subsampling_x && cm->subsampling_y) {
      cm->chroma_sample_position = aom_rb_read_literal(rb, 2);
    }
  }
  cm->separate_uv_delta_q = aom_rb_read_bit(rb);
}

void av1_read_timing_info_header(AV1_COMMON *cm,
                                 struct aom_read_bit_buffer *rb) {
  cm->timing_info.num_units_in_display_tick = aom_rb_read_unsigned_literal(
      rb, 32);  // Number of units in a display tick
  cm->timing_info.time_scale =
      aom_rb_read_unsigned_literal(rb, 32);  // Time scale
  if (cm->timing_info.num_units_in_display_tick == 0 ||
      cm->timing_info.time_scale == 0) {
    aom_internal_error(
        &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
        "num_units_in_display_tick and time_scale must be greater than 0.");
  }
  cm->timing_info.equal_picture_interval =
      aom_rb_read_bit(rb);  // Equal picture interval bit
  if (cm->timing_info.equal_picture_interval) {
    cm->timing_info.num_ticks_per_picture =
        aom_rb_read_uvlc(rb) + 1;  // ticks per picture
    if (cm->timing_info.num_ticks_per_picture == 0) {
      aom_internal_error(
          &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
          "num_ticks_per_picture_minus_1 cannot be (1 << 32) − 1.");
    }
  }
}

void av1_read_decoder_model_info(AV1_COMMON *cm,
                                 struct aom_read_bit_buffer *rb) {
  cm->buffer_model.encoder_decoder_buffer_delay_length =
      aom_rb_read_literal(rb, 5) + 1;
  cm->buffer_model.num_units_in_decoding_tick = aom_rb_read_unsigned_literal(
      rb, 32);  // Number of units in a decoding tick
  cm->buffer_model.buffer_removal_delay_length = aom_rb_read_literal(rb, 5) + 1;
  cm->buffer_model.frame_presentation_delay_length =
      aom_rb_read_literal(rb, 5) + 1;
}

void av1_read_op_parameters_info(AV1_COMMON *const cm,
                                 struct aom_read_bit_buffer *rb, int op_num) {
  // The cm->op_params array has MAX_NUM_OPERATING_POINTS + 1 elements.
  if (op_num > MAX_NUM_OPERATING_POINTS) {
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "AV1 does not support %d decoder model operating points",
                       op_num + 1);
  }

  cm->op_params[op_num].decoder_buffer_delay = aom_rb_read_literal(
      rb, cm->buffer_model.encoder_decoder_buffer_delay_length);

  cm->op_params[op_num].encoder_buffer_delay = aom_rb_read_literal(
      rb, cm->buffer_model.encoder_decoder_buffer_delay_length);

  cm->op_params[op_num].low_delay_mode_flag = aom_rb_read_bit(rb);
}

static void av1_read_tu_pts_info(AV1_COMMON *const cm,
                                 struct aom_read_bit_buffer *rb) {
  cm->tu_presentation_delay =
      aom_rb_read_literal(rb, cm->buffer_model.frame_presentation_delay_length);
}

void read_sequence_header(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  // rb->error_handler may be triggered during aom_rb_read_bit(), raising
  // internal errors and immediate decoding termination. We use a local variable
  // to store the info. as we decode. At the end, if no errors have occurred,
  // cm->seq_params is updated.
  SequenceHeader sh = cm->seq_params;
  SequenceHeader *const seq_params = &sh;
  int num_bits_width = aom_rb_read_literal(rb, 4) + 1;
  int num_bits_height = aom_rb_read_literal(rb, 4) + 1;
  int max_frame_width = aom_rb_read_literal(rb, num_bits_width) + 1;
  int max_frame_height = aom_rb_read_literal(rb, num_bits_height) + 1;

  seq_params->num_bits_width = num_bits_width;
  seq_params->num_bits_height = num_bits_height;
  seq_params->max_frame_width = max_frame_width;
  seq_params->max_frame_height = max_frame_height;

  if (seq_params->reduced_still_picture_hdr) {
    seq_params->frame_id_numbers_present_flag = 0;
  } else {
    seq_params->frame_id_numbers_present_flag = aom_rb_read_bit(rb);
  }
  if (seq_params->frame_id_numbers_present_flag) {
    // We must always have delta_frame_id_length < frame_id_length,
    // in order for a frame to be referenced with a unique delta.
    // Avoid wasting bits by using a coding that enforces this restriction.
    seq_params->delta_frame_id_length = aom_rb_read_literal(rb, 4) + 2;
    seq_params->frame_id_length =
        aom_rb_read_literal(rb, 3) + seq_params->delta_frame_id_length + 1;
    if (seq_params->frame_id_length > 16)
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid frame_id_length");
  }

  setup_sb_size(seq_params, rb);

  seq_params->enable_filter_intra = aom_rb_read_bit(rb);
  seq_params->enable_intra_edge_filter = aom_rb_read_bit(rb);

  if (seq_params->reduced_still_picture_hdr) {
    seq_params->enable_interintra_compound = 0;
    seq_params->enable_masked_compound = 0;
    seq_params->enable_warped_motion = 0;
    seq_params->enable_dual_filter = 0;
    seq_params->enable_order_hint = 0;
    seq_params->enable_jnt_comp = 0;
    seq_params->enable_ref_frame_mvs = 0;
    seq_params->force_screen_content_tools = 2;  // SELECT_SCREEN_CONTENT_TOOLS
    seq_params->force_integer_mv = 2;            // SELECT_INTEGER_MV
    seq_params->order_hint_bits_minus_1 = -1;
  } else {
    seq_params->enable_interintra_compound = aom_rb_read_bit(rb);
    seq_params->enable_masked_compound = aom_rb_read_bit(rb);
    seq_params->enable_warped_motion = aom_rb_read_bit(rb);
    seq_params->enable_dual_filter = aom_rb_read_bit(rb);

    seq_params->enable_order_hint = aom_rb_read_bit(rb);
    seq_params->enable_jnt_comp =
        seq_params->enable_order_hint ? aom_rb_read_bit(rb) : 0;
    seq_params->enable_ref_frame_mvs =
        seq_params->enable_order_hint ? aom_rb_read_bit(rb) : 0;

    if (aom_rb_read_bit(rb)) {
      seq_params->force_screen_content_tools =
          2;  // SELECT_SCREEN_CONTENT_TOOLS
    } else {
      seq_params->force_screen_content_tools = aom_rb_read_bit(rb);
    }

    if (seq_params->force_screen_content_tools > 0) {
      if (aom_rb_read_bit(rb)) {
        seq_params->force_integer_mv = 2;  // SELECT_INTEGER_MV
      } else {
        seq_params->force_integer_mv = aom_rb_read_bit(rb);
      }
    } else {
      seq_params->force_integer_mv = 2;  // SELECT_INTEGER_MV
    }
    seq_params->order_hint_bits_minus_1 =
        seq_params->enable_order_hint ? aom_rb_read_literal(rb, 3) : -1;
  }

  seq_params->enable_superres = aom_rb_read_bit(rb);
  seq_params->enable_cdef = aom_rb_read_bit(rb);
  seq_params->enable_restoration = aom_rb_read_bit(rb);
  cm->seq_params = *seq_params;
}

static int read_global_motion_params(WarpedMotionParams *params,
                                     const WarpedMotionParams *ref_params,
                                     struct aom_read_bit_buffer *rb,
                                     int allow_hp) {
  TransformationType type = aom_rb_read_bit(rb);
  if (type != IDENTITY) {
    if (aom_rb_read_bit(rb))
      type = ROTZOOM;
    else
      type = aom_rb_read_bit(rb) ? TRANSLATION : AFFINE;
  }

  *params = default_warp_params;
  params->wmtype = type;

  if (type >= ROTZOOM) {
    params->wmmat[2] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
                               (1 << GM_ALPHA_PREC_BITS)) *
                           GM_ALPHA_DECODE_FACTOR +
                       (1 << WARPEDMODEL_PREC_BITS);
    params->wmmat[3] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF)) *
                       GM_ALPHA_DECODE_FACTOR;
  }

  if (type >= AFFINE) {
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

  if (type >= TRANSLATION) {
    const int trans_bits = (type == TRANSLATION)
                               ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                               : GM_ABS_TRANS_BITS;
    const int trans_dec_factor =
        (type == TRANSLATION) ? GM_TRANS_ONLY_DECODE_FACTOR * (1 << !allow_hp)
                              : GM_TRANS_DECODE_FACTOR;
    const int trans_prec_diff = (type == TRANSLATION)
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
  }

  if (params->wmtype <= AFFINE) {
    int good_shear_params = get_shear_params(params);
    if (!good_shear_params) return 0;
  }

  return 1;
}

static void read_global_motion(AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  for (int frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
    const WarpedMotionParams *ref_params =
        cm->prev_frame ? &cm->prev_frame->global_motion[frame]
                       : &default_warp_params;
    int good_params = read_global_motion_params(
        &cm->global_motion[frame], ref_params, rb, cm->allow_high_precision_mv);
    if (!good_params) {
#if WARPED_MOTION_DEBUG
      printf("Warning: unexpected global motion shear params from aomenc\n");
#endif
      cm->global_motion[frame].invalid = 1;
    }

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
         REF_FRAMES * sizeof(WarpedMotionParams));
}

static void show_existing_frame_reset(AV1Decoder *const pbi,
                                      int existing_frame_idx) {
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;
  RefCntBuffer *const frame_bufs = pool->frame_bufs;

  assert(cm->show_existing_frame);

  cm->frame_type = KEY_FRAME;

  pbi->refresh_frame_flags = (1 << REF_FRAMES) - 1;

  for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    cm->frame_refs[i].idx = INVALID_IDX;
    cm->frame_refs[i].buf = NULL;
  }

  if (pbi->need_resync) {
    memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
    pbi->need_resync = 0;
  }

  cm->cur_frame->intra_only = 1;

  if (cm->seq_params.frame_id_numbers_present_flag) {
    /* If bitmask is set, update reference frame id values and
       mark frames as valid for reference.
       Note that the displayed frame be valid for referencing
       in order to have been selected.
    */
    int refresh_frame_flags = pbi->refresh_frame_flags;
    int display_frame_id = cm->ref_frame_id[existing_frame_idx];
    for (int i = 0; i < REF_FRAMES; i++) {
      if ((refresh_frame_flags >> i) & 1) {
        cm->ref_frame_id[i] = display_frame_id;
        cm->valid_for_referencing[i] = 1;
      }
    }
  }

  cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;

  // Generate next_ref_frame_map.
  lock_buffer_pool(pool);
  int ref_index = 0;
  for (int mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
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

  // Reload the adapted CDFs from when we originally coded this keyframe
  *cm->fc = cm->frame_contexts[existing_frame_idx];
}

static int read_uncompressed_header(AV1Decoder *pbi,
                                    struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  MACROBLOCKD *const xd = &pbi->mb;
  BufferPool *const pool = cm->buffer_pool;
  RefCntBuffer *const frame_bufs = pool->frame_bufs;

  if (!pbi->sequence_header_ready) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "No sequence header");
  }

  cm->last_frame_type = cm->frame_type;
  cm->last_intra_only = cm->intra_only;

  // NOTE: By default all coded frames to be used as a reference
  cm->is_reference_frame = 1;

  if (cm->seq_params.reduced_still_picture_hdr) {
    cm->show_existing_frame = 0;
    cm->show_frame = 1;
    cm->frame_type = KEY_FRAME;
    cm->error_resilient_mode = 1;
  } else {
    cm->show_existing_frame = aom_rb_read_bit(rb);
    cm->reset_decoder_state = 0;

    if (cm->show_existing_frame) {
      // Show an existing frame directly.
      const int existing_frame_idx = aom_rb_read_literal(rb, 3);
      const int frame_to_show = cm->ref_frame_map[existing_frame_idx];
      if (cm->seq_params.decoder_model_info_present_flag &&
          cm->timing_info.equal_picture_interval == 0) {
        av1_read_tu_pts_info(cm, rb);
      }
      if (cm->seq_params.frame_id_numbers_present_flag) {
        int frame_id_length = cm->seq_params.frame_id_length;
        int display_frame_id = aom_rb_read_literal(rb, frame_id_length);
        /* Compare display_frame_id with ref_frame_id and check valid for
         * referencing */
        if (display_frame_id != cm->ref_frame_id[existing_frame_idx] ||
            cm->valid_for_referencing[existing_frame_idx] == 0)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Reference buffer frame ID mismatch");
      }
      lock_buffer_pool(pool);
      if (frame_to_show < 0 || frame_bufs[frame_to_show].ref_count < 1) {
        unlock_buffer_pool(pool);
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Buffer %d does not contain a decoded frame",
                           frame_to_show);
      }
      ref_cnt_fb(frame_bufs, &cm->new_fb_idx, frame_to_show);
      cm->reset_decoder_state =
          frame_bufs[frame_to_show].frame_type == KEY_FRAME;
      unlock_buffer_pool(pool);

      cm->lf.filter_level[0] = 0;
      cm->lf.filter_level[1] = 0;
      cm->show_frame = 1;

      if (!frame_bufs[frame_to_show].showable_frame) {
        aom_merge_corrupted_flag(&xd->corrupted, 1);
      }
      if (cm->reset_decoder_state) frame_bufs[frame_to_show].showable_frame = 0;

      cm->film_grain_params = frame_bufs[frame_to_show].film_grain_params;

      if (cm->reset_decoder_state) {
        show_existing_frame_reset(pbi, existing_frame_idx);
      } else {
        pbi->refresh_frame_flags = 0;
      }

      return 0;
    }

    cm->frame_type = (FRAME_TYPE)aom_rb_read_literal(rb, 2);  // 2 bits
    cm->show_frame = aom_rb_read_bit(rb);
    if (cm->seq_params.still_picture &&
        (cm->frame_type != KEY_FRAME || !cm->show_frame)) {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Still pictures must be coded as shown keyframes");
    }
    cm->showable_frame = cm->frame_type != KEY_FRAME;
    if (cm->show_frame) {
      if (cm->seq_params.decoder_model_info_present_flag &&
          cm->timing_info.equal_picture_interval == 0)
        av1_read_tu_pts_info(cm, rb);
    } else {
      // See if this frame can be used as show_existing_frame in future
      cm->showable_frame = aom_rb_read_bit(rb);
    }
    cm->cur_frame->showable_frame = cm->showable_frame;
    cm->intra_only = cm->frame_type == INTRA_ONLY_FRAME;
    cm->error_resilient_mode =
        frame_is_sframe(cm) || (cm->frame_type == KEY_FRAME && cm->show_frame)
            ? 1
            : aom_rb_read_bit(rb);
  }

  cm->disable_cdf_update = aom_rb_read_bit(rb);
  if (cm->seq_params.force_screen_content_tools == 2) {
    cm->allow_screen_content_tools = aom_rb_read_bit(rb);
  } else {
    cm->allow_screen_content_tools = cm->seq_params.force_screen_content_tools;
  }

  if (cm->allow_screen_content_tools) {
    if (cm->seq_params.force_integer_mv == 2) {
      cm->cur_frame_force_integer_mv = aom_rb_read_bit(rb);
    } else {
      cm->cur_frame_force_integer_mv = cm->seq_params.force_integer_mv;
    }
  } else {
    cm->cur_frame_force_integer_mv = 0;
  }

  cm->frame_refs_short_signaling = 0;
  int frame_size_override_flag = 0;
  cm->allow_intrabc = 0;
  cm->primary_ref_frame = PRIMARY_REF_NONE;

  if (!cm->seq_params.reduced_still_picture_hdr) {
    if (cm->seq_params.frame_id_numbers_present_flag) {
      int frame_id_length = cm->seq_params.frame_id_length;
      int diff_len = cm->seq_params.delta_frame_id_length;
      int prev_frame_id = 0;
      int have_prev_frame_id = !pbi->decoding_first_frame &&
                               !(cm->frame_type == KEY_FRAME && cm->show_frame);
      if (have_prev_frame_id) {
        prev_frame_id = cm->current_frame_id;
      }
      cm->current_frame_id = aom_rb_read_literal(rb, frame_id_length);

      if (have_prev_frame_id) {
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
      for (int i = 0; i < REF_FRAMES; i++) {
        if (cm->frame_type == KEY_FRAME && cm->show_frame) {
          cm->valid_for_referencing[i] = 0;
        } else if (cm->current_frame_id - (1 << diff_len) > 0) {
          if (cm->ref_frame_id[i] > cm->current_frame_id ||
              cm->ref_frame_id[i] < cm->current_frame_id - (1 << diff_len))
            cm->valid_for_referencing[i] = 0;
        } else {
          if (cm->ref_frame_id[i] > cm->current_frame_id &&
              cm->ref_frame_id[i] < (1 << frame_id_length) +
                                        cm->current_frame_id - (1 << diff_len))
            cm->valid_for_referencing[i] = 0;
        }
      }
    }

    frame_size_override_flag =
        frame_is_sframe(cm) ? 1 : aom_rb_read_literal(rb, 1);

    cm->frame_offset =
        aom_rb_read_literal(rb, cm->seq_params.order_hint_bits_minus_1 + 1);
    cm->current_video_frame = cm->frame_offset;

    if (!cm->error_resilient_mode && !frame_is_intra_only(cm)) {
      cm->primary_ref_frame = aom_rb_read_literal(rb, PRIMARY_REF_BITS);
    }
  }

  if (cm->seq_params.decoder_model_info_present_flag) {
    cm->buffer_removal_delay_present = aom_rb_read_bit(rb);
    if (cm->buffer_removal_delay_present) {
      for (int op_num = 0;
           op_num < cm->seq_params.operating_points_cnt_minus_1 + 1; op_num++) {
        if (cm->op_params[op_num].decoder_model_param_present_flag) {
          if ((((cm->seq_params.operating_point_idc[op_num] >>
                 cm->temporal_layer_id) &
                0x1) &&
               ((cm->seq_params.operating_point_idc[op_num] >>
                 (cm->spatial_layer_id + 8)) &
                0x1)) ||
              cm->seq_params.operating_point_idc[op_num] == 0) {
            cm->op_frame_timing[op_num].buffer_removal_delay =
                aom_rb_read_literal(
                    rb, cm->buffer_model.buffer_removal_delay_length);
          } else {
            cm->op_frame_timing[op_num].buffer_removal_delay = 0;
          }
        } else {
          cm->op_frame_timing[op_num].buffer_removal_delay = 0;
        }
      }
    }
  }
  if (cm->frame_type == KEY_FRAME) {
    if (!cm->show_frame)  // unshown keyframe (forward keyframe)
      pbi->refresh_frame_flags = aom_rb_read_literal(rb, REF_FRAMES);
    else  // shown keyframe
      pbi->refresh_frame_flags = (1 << REF_FRAMES) - 1;

    for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
      cm->frame_refs[i].idx = INVALID_IDX;
      cm->frame_refs[i].buf = NULL;
    }
    if (pbi->need_resync) {
      memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
      pbi->need_resync = 0;
    }
  } else {
    if (cm->intra_only) {
      pbi->refresh_frame_flags = aom_rb_read_literal(rb, REF_FRAMES);
      if (pbi->refresh_frame_flags == 0xFF) {
        aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Intra only frames cannot have refresh flags 0xFF");
      }
      if (pbi->need_resync) {
        memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
        pbi->need_resync = 0;
      }
    } else if (pbi->need_resync != 1) { /* Skip if need resync */
      pbi->refresh_frame_flags =
          frame_is_sframe(cm) ? 0xFF : aom_rb_read_literal(rb, REF_FRAMES);
      if (!pbi->refresh_frame_flags) {
        // NOTE: "pbi->refresh_frame_flags == 0" indicates that the coded frame
        //       will not be used as a reference
        cm->is_reference_frame = 0;
      }
    }
  }

  if (!frame_is_intra_only(cm) || pbi->refresh_frame_flags != 0xFF) {
    // Read all ref frame order hints if error_resilient_mode == 1
    if (cm->error_resilient_mode && cm->seq_params.enable_order_hint) {
      for (int ref_idx = 0; ref_idx < REF_FRAMES; ref_idx++) {
        // Read order hint from bit stream
        unsigned int frame_offset =
            aom_rb_read_literal(rb, cm->seq_params.order_hint_bits_minus_1 + 1);
        // Get buffer index
        int buf_idx = cm->ref_frame_map[ref_idx];
        assert(buf_idx < FRAME_BUFFERS);
        if (buf_idx == -1 ||
            frame_offset != frame_bufs[buf_idx].cur_frame_offset) {
          if (buf_idx >= 0) {
            lock_buffer_pool(pool);
            decrease_ref_count(buf_idx, frame_bufs, pool);
            unlock_buffer_pool(pool);
          }
          // If no corresponding buffer exists, allocate a new buffer with all
          // pixels set to neutral grey.
          buf_idx = get_free_fb(cm);
          if (buf_idx == INVALID_IDX) {
            aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                               "Unable to find free frame buffer");
          }
          lock_buffer_pool(pool);
          if (aom_realloc_frame_buffer(
                  &frame_bufs[buf_idx].buf, cm->seq_params.max_frame_width,
                  cm->seq_params.max_frame_height, cm->subsampling_x,
                  cm->subsampling_y, cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                  cm->byte_alignment,
                  &pool->frame_bufs[buf_idx].raw_frame_buffer, pool->get_fb_cb,
                  pool->cb_priv)) {
            unlock_buffer_pool(pool);
            aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                               "Failed to allocate frame buffer");
          }
          unlock_buffer_pool(pool);
          set_planes_to_neutral_grey(cm, &frame_bufs[buf_idx].buf, 0);

          cm->ref_frame_map[ref_idx] = buf_idx;
          frame_bufs[buf_idx].cur_frame_offset = frame_offset;
        }
      }
    }
  }

  if (cm->frame_type == KEY_FRAME) {
    setup_frame_size(cm, frame_size_override_flag, rb);

    if (cm->allow_screen_content_tools && !av1_superres_scaled(cm))
      cm->allow_intrabc = aom_rb_read_bit(rb);
    cm->allow_ref_frame_mvs = 0;
    cm->prev_frame = NULL;
  } else {
    cm->allow_ref_frame_mvs = 0;

    if (cm->intra_only) {
      cm->cur_frame->film_grain_params_present = cm->film_grain_params_present;
      setup_frame_size(cm, frame_size_override_flag, rb);
      if (cm->allow_screen_content_tools && !av1_superres_scaled(cm))
        cm->allow_intrabc = aom_rb_read_bit(rb);

    } else if (pbi->need_resync != 1) { /* Skip if need resync */

      // Frame refs short signaling is off when error resilient mode is on.
      if (cm->seq_params.enable_order_hint)
        cm->frame_refs_short_signaling = aom_rb_read_bit(rb);

      if (cm->frame_refs_short_signaling) {
        // == LAST_FRAME ==
        const int lst_ref = aom_rb_read_literal(rb, REF_FRAMES_LOG2);
        const int lst_idx = cm->ref_frame_map[lst_ref];

        // == GOLDEN_FRAME ==
        const int gld_ref = aom_rb_read_literal(rb, REF_FRAMES_LOG2);
        const int gld_idx = cm->ref_frame_map[gld_ref];

        // Most of the time, streams start with a keyframe. In that case,
        // ref_frame_map will have been filled in at that point and will not
        // contain any -1's. However, streams are explicitly allowed to start
        // with an intra-only frame, so long as they don't then signal a
        // reference to a slot that hasn't been set yet. That's what we are
        // checking here.
        if (lst_idx == -1)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Inter frame requests nonexistent reference");
        if (gld_idx == -1)
          aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                             "Inter frame requests nonexistent reference");

        av1_set_frame_refs(cm, lst_ref, gld_ref);
      }

      for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
        int ref = 0;
        if (!cm->frame_refs_short_signaling) {
          ref = aom_rb_read_literal(rb, REF_FRAMES_LOG2);
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
          ref_frame->map_idx = ref;
        } else {
          ref = cm->frame_refs[i].map_idx;
        }

        cm->ref_frame_sign_bias[LAST_FRAME + i] = 0;

        if (cm->seq_params.frame_id_numbers_present_flag) {
          int frame_id_length = cm->seq_params.frame_id_length;
          int diff_len = cm->seq_params.delta_frame_id_length;
          int delta_frame_id_minus_1 = aom_rb_read_literal(rb, diff_len);
          int ref_frame_id =
              ((cm->current_frame_id - (delta_frame_id_minus_1 + 1) +
                (1 << frame_id_length)) %
               (1 << frame_id_length));
          // Compare values derived from delta_frame_id_minus_1 and
          // refresh_frame_flags. Also, check valid for referencing
          if (ref_frame_id != cm->ref_frame_id[ref] ||
              cm->valid_for_referencing[ref] == 0)
            aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                               "Reference buffer frame ID mismatch");
        }
      }

      if (!cm->error_resilient_mode && frame_size_override_flag) {
        setup_frame_size_with_refs(cm, rb);
      } else {
        setup_frame_size(cm, frame_size_override_flag, rb);
      }

      if (cm->cur_frame_force_integer_mv) {
        cm->allow_high_precision_mv = 0;
      } else {
        cm->allow_high_precision_mv = aom_rb_read_bit(rb);
      }
      cm->interp_filter = read_frame_interp_filter(rb);
      cm->switchable_motion_mode = aom_rb_read_bit(rb);
    }

    cm->prev_frame = get_prev_frame(cm);
    if (cm->primary_ref_frame != PRIMARY_REF_NONE &&
        cm->frame_refs[cm->primary_ref_frame].idx < 0) {
      aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Reference frame containing this frame's initial "
                         "frame context is unavailable.");
    }

    if (!cm->intra_only && pbi->need_resync != 1) {
      if (frame_might_allow_ref_frame_mvs(cm))
        cm->allow_ref_frame_mvs = aom_rb_read_bit(rb);
      else
        cm->allow_ref_frame_mvs = 0;

      for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
        RefBuffer *const ref_buf = &cm->frame_refs[i];
        av1_setup_scale_factors_for_frame(
            &ref_buf->sf, ref_buf->buf->y_crop_width,
            ref_buf->buf->y_crop_height, cm->width, cm->height);
        if ((!av1_is_valid_scale(&ref_buf->sf)))
          aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                             "Reference frame has invalid dimensions");
      }
    }
  }

  av1_setup_frame_buf_refs(cm);

  av1_setup_frame_sign_bias(cm);

  cm->cur_frame->intra_only = cm->frame_type == KEY_FRAME || cm->intra_only;
  cm->cur_frame->frame_type = cm->frame_type;

  if (cm->seq_params.frame_id_numbers_present_flag) {
    /* If bitmask is set, update reference frame id values and
       mark frames as valid for reference */
    int refresh_frame_flags = pbi->refresh_frame_flags;
    for (int i = 0; i < REF_FRAMES; i++) {
      if ((refresh_frame_flags >> i) & 1) {
        cm->ref_frame_id[i] = cm->current_frame_id;
        cm->valid_for_referencing[i] = 1;
      }
    }
  }

  const int might_bwd_adapt =
      !(cm->seq_params.reduced_still_picture_hdr) && !(cm->disable_cdf_update);
  if (might_bwd_adapt) {
    cm->refresh_frame_context = aom_rb_read_bit(rb)
                                    ? REFRESH_FRAME_CONTEXT_DISABLED
                                    : REFRESH_FRAME_CONTEXT_BACKWARD;
  } else {
    cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;
  }

  get_frame_new_buffer(cm)->bit_depth = cm->bit_depth;
  get_frame_new_buffer(cm)->color_primaries = cm->color_primaries;
  get_frame_new_buffer(cm)->transfer_characteristics =
      cm->transfer_characteristics;
  get_frame_new_buffer(cm)->matrix_coefficients = cm->matrix_coefficients;
  get_frame_new_buffer(cm)->monochrome = cm->seq_params.monochrome;
  get_frame_new_buffer(cm)->chroma_sample_position = cm->chroma_sample_position;
  get_frame_new_buffer(cm)->color_range = cm->color_range;
  get_frame_new_buffer(cm)->render_width = cm->render_width;
  get_frame_new_buffer(cm)->render_height = cm->render_height;

  if (pbi->need_resync) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Keyframe / intra-only frame required to reset decoder"
                       " state");
  }

  // Generate next_ref_frame_map.
  lock_buffer_pool(pool);
  int ref_index = 0;
  for (int mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
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

  if (cm->allow_intrabc) {
    // Set parameters corresponding to no filtering.
    struct loopfilter *lf = &cm->lf;
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
    cm->cdef_bits = 0;
    cm->cdef_strengths[0] = 0;
    cm->nb_cdef_strengths = 1;
    cm->cdef_uv_strengths[0] = 0;
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  }

  read_tile_info(pbi, rb);
  setup_quantization(cm, rb);
  xd->bd = (int)cm->bit_depth;

  if (cm->num_allocated_above_context_planes < av1_num_planes(cm) ||
      cm->num_allocated_above_context_mi_col < cm->mi_cols ||
      cm->num_allocated_above_contexts < cm->tile_rows) {
    av1_free_above_context_buffers(cm, cm->num_allocated_above_contexts);
    if (av1_alloc_above_context_buffers(cm, cm->tile_rows))
      aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate context buffers");
  }

  if (cm->primary_ref_frame == PRIMARY_REF_NONE) {
    av1_setup_past_independence(cm);
  }

  setup_segmentation(cm, rb);

  cm->delta_q_res = 1;
  cm->delta_lf_res = 1;
  cm->delta_lf_present_flag = 0;
  cm->delta_lf_multi = 0;
  cm->delta_q_present_flag = cm->base_qindex > 0 ? aom_rb_read_bit(rb) : 0;
  if (cm->delta_q_present_flag) {
    xd->current_qindex = cm->base_qindex;
    cm->delta_q_res = 1 << aom_rb_read_literal(rb, 2);
    if (!cm->allow_intrabc) cm->delta_lf_present_flag = aom_rb_read_bit(rb);
    if (cm->delta_lf_present_flag) {
      cm->delta_lf_res = 1 << aom_rb_read_literal(rb, 2);
      cm->delta_lf_multi = aom_rb_read_bit(rb);
      av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
    }
  }

  xd->cur_frame_force_integer_mv = cm->cur_frame_force_integer_mv;

  for (int i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = cm->seg.enabled
                           ? av1_get_qindex(&cm->seg, i, cm->base_qindex)
                           : cm->base_qindex;
    xd->lossless[i] = qindex == 0 && cm->y_dc_delta_q == 0 &&
                      cm->u_dc_delta_q == 0 && cm->u_ac_delta_q == 0 &&
                      cm->v_dc_delta_q == 0 && cm->v_ac_delta_q == 0;
    xd->qindex[i] = qindex;
  }
  cm->coded_lossless = is_coded_lossless(cm, xd);
  cm->all_lossless = cm->coded_lossless && !av1_superres_scaled(cm);
  setup_segmentation_dequant(cm);
  if (cm->coded_lossless) {
    cm->lf.filter_level[0] = 0;
    cm->lf.filter_level[1] = 0;
  }
  if (cm->coded_lossless || !cm->seq_params.enable_cdef) {
    cm->cdef_bits = 0;
    cm->cdef_strengths[0] = 0;
    cm->cdef_uv_strengths[0] = 0;
  }
  if (cm->all_lossless || !cm->seq_params.enable_restoration) {
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  }
  setup_loopfilter(cm, rb);

  if (!cm->coded_lossless && cm->seq_params.enable_cdef) {
    setup_cdef(cm, rb);
  }
  if (!cm->all_lossless && cm->seq_params.enable_restoration) {
    decode_restoration_mode(cm, rb);
  }

  cm->tx_mode = read_tx_mode(cm, rb);
  cm->reference_mode = read_frame_reference_mode(cm, rb);
  if (cm->reference_mode != SINGLE_REFERENCE) setup_compound_reference_mode(cm);

  av1_setup_skip_mode_allowed(cm);
  cm->skip_mode_flag = cm->is_skip_mode_allowed ? aom_rb_read_bit(rb) : 0;

  if (frame_might_allow_warped_motion(cm))
    cm->allow_warped_motion = aom_rb_read_bit(rb);
  else
    cm->allow_warped_motion = 0;

  cm->reduced_tx_set_used = aom_rb_read_bit(rb);

  if (cm->allow_ref_frame_mvs && !frame_might_allow_ref_frame_mvs(cm)) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Frame wrongly requests reference frame MVs");
  }

  if (!frame_is_intra_only(cm)) read_global_motion(cm, rb);

  cm->cur_frame->film_grain_params_present = cm->film_grain_params_present;
  read_film_grain(cm, rb);

#if EXT_TILE_DEBUG
  if (pbi->ext_tile_debug && cm->large_scale_tile) {
    read_ext_tile_info(pbi, rb);
    av1_set_single_tile_decoding_mode(cm);
  }
#endif  // EXT_TILE_DEBUG
  return 0;
}

struct aom_read_bit_buffer *av1_init_read_bit_buffer(
    AV1Decoder *pbi, struct aom_read_bit_buffer *rb, const uint8_t *data,
    const uint8_t *data_end) {
  rb->bit_offset = 0;
  rb->error_handler = error_handler;
  rb->error_handler_data = &pbi->common;
  rb->bit_buffer = data;
  rb->bit_buffer_end = data_end;
  return rb;
}

void av1_read_frame_size(struct aom_read_bit_buffer *rb, int num_bits_width,
                         int num_bits_height, int *width, int *height) {
  *width = aom_rb_read_literal(rb, num_bits_width) + 1;
  *height = aom_rb_read_literal(rb, num_bits_height) + 1;
}

BITSTREAM_PROFILE av1_read_profile(struct aom_read_bit_buffer *rb) {
  int profile = aom_rb_read_literal(rb, PROFILE_BITS);
  return (BITSTREAM_PROFILE)profile;
}

void superres_post_decode(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;

  if (!av1_superres_scaled(cm)) return;
  assert(!cm->all_lossless);

  lock_buffer_pool(pool);
  av1_superres_upscale(cm, pool);
  unlock_buffer_pool(pool);
}

int av1_decode_frame_headers_and_setup(AV1Decoder *pbi,
                                       struct aom_read_bit_buffer *rb,
                                       const uint8_t *data,
                                       const uint8_t **p_data_end,
                                       int trailing_bits_present) {
  AV1_COMMON *const cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &pbi->mb;

#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_set_frame_read(cm->current_video_frame * 2 + cm->show_frame);
#endif
#if CONFIG_MISMATCH_DEBUG
  mismatch_move_frame_idx_r();
#endif

  for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    cm->global_motion[i] = default_warp_params;
    cm->cur_frame->global_motion[i] = default_warp_params;
  }
  xd->global_motion = cm->global_motion;

  read_uncompressed_header(pbi, rb);

  if (trailing_bits_present) av1_check_trailing_bits(pbi, rb);

  // If cm->single_tile_decoding = 0, the independent decoding of a single tile
  // or a section of a frame is not allowed.
  if (!cm->single_tile_decoding &&
      (pbi->dec_tile_row >= 0 || pbi->dec_tile_col >= 0)) {
    pbi->dec_tile_row = -1;
    pbi->dec_tile_col = -1;
  }

  pbi->uncomp_hdr_size = aom_rb_bytes_read(rb);
  YV12_BUFFER_CONFIG *new_fb = get_frame_new_buffer(cm);
  xd->cur_buf = new_fb;
  if (av1_allow_intrabc(cm)) {
    av1_setup_scale_factors_for_frame(
        &cm->sf_identity, xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height,
        xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height);
  }

  if (cm->show_existing_frame) {
    // showing a frame directly
    *p_data_end = data + aom_rb_bytes_read(rb);
    if (cm->reset_decoder_state) {
      // Use the default frame context values.
      *cm->fc = cm->frame_contexts[FRAME_CONTEXT_DEFAULTS];
      if (!cm->fc->initialized)
        aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Uninitialized entropy context.");
    }
    return 0;
  }

  cm->setup_mi(cm);

  cm->current_frame_seg_map = cm->cur_frame->seg_map;

  av1_setup_motion_field(cm);

  av1_setup_block_planes(xd, cm->subsampling_x, cm->subsampling_y, num_planes);
  if (cm->primary_ref_frame == PRIMARY_REF_NONE) {
    // use the default frame context values
    *cm->fc = cm->frame_contexts[FRAME_CONTEXT_DEFAULTS];
  } else {
    *cm->fc = cm->frame_contexts[cm->frame_refs[cm->primary_ref_frame].idx];
  }
  if (!cm->fc->initialized)
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Uninitialized entropy context.");

  xd->corrupted = 0;
  return 0;
}

// Once-per-frame initialization
static void setup_frame_info(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;

  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    av1_alloc_restoration_buffers(cm);
  }
  const int use_highbd = cm->use_highbitdepth ? 1 : 0;
  const int buf_size = MC_TEMP_BUF_PELS << use_highbd;
  if (pbi->td.mc_buf_size != buf_size) {
    av1_free_mc_tmp_buf(&pbi->td, use_highbd);
    allocate_mc_tmp_buf(cm, &pbi->td, buf_size, use_highbd);
  }
}

void av1_decode_tg_tiles_and_wrapup(AV1Decoder *pbi, const uint8_t *data,
                                    const uint8_t *data_end,
                                    const uint8_t **p_data_end, int start_tile,
                                    int end_tile, int initialize_flag) {
  AV1_COMMON *const cm = &pbi->common;
  MACROBLOCKD *const xd = &pbi->mb;
  const int tile_count_tg = end_tile - start_tile + 1;

  if (initialize_flag) setup_frame_info(pbi);

  if (pbi->max_threads > 1 && tile_count_tg > 1 && !cm->large_scale_tile)
    *p_data_end = decode_tiles_mt(pbi, data, data_end, start_tile, end_tile);
  else
    *p_data_end = decode_tiles(pbi, data, data_end, start_tile, end_tile);

  const int num_planes = av1_num_planes(cm);
  // If the bit stream is monochrome, set the U and V buffers to a constant.
  if (num_planes < 3) set_planes_to_neutral_grey(cm, xd->cur_buf, 1);

  if (end_tile != cm->tile_rows * cm->tile_cols - 1) {
    return;
  }

  if (!cm->allow_intrabc && !cm->single_tile_decoding) {
    if (cm->lf.filter_level[0] || cm->lf.filter_level[1]) {
#if LOOP_FILTER_BITMASK
      av1_loop_filter_frame(get_frame_new_buffer(cm), cm, &pbi->mb, 0,
                            num_planes, 0);
#else
      if (pbi->num_workers > 1) {
        av1_loop_filter_frame_mt(get_frame_new_buffer(cm), cm, &pbi->mb, 0,
                                 num_planes, 0, pbi->tile_workers,
                                 pbi->num_workers, &pbi->lf_row_sync);
      } else {
        av1_loop_filter_frame(get_frame_new_buffer(cm), cm, &pbi->mb, 0,
                              num_planes, 0);
      }
#endif
    }

    const int do_loop_restoration =
        cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
        cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
        cm->rst_info[2].frame_restoration_type != RESTORE_NONE;
    const int do_cdef =
        !cm->skip_loop_filter && !cm->coded_lossless &&
        (cm->cdef_bits || cm->cdef_strengths[0] || cm->cdef_uv_strengths[0]);
    const int do_superres = av1_superres_scaled(cm);
    const int optimized_loop_restoration = !do_cdef && !do_superres;

    if (!optimized_loop_restoration) {
      if (do_loop_restoration)
        av1_loop_restoration_save_boundary_lines(&pbi->cur_buf->buf, cm, 0);

      if (do_cdef) av1_cdef_frame(&pbi->cur_buf->buf, cm, &pbi->mb);

      superres_post_decode(pbi);

      if (do_loop_restoration) {
        av1_loop_restoration_save_boundary_lines(&pbi->cur_buf->buf, cm, 1);
        if (pbi->num_workers > 1) {
          av1_loop_restoration_filter_frame_mt(
              (YV12_BUFFER_CONFIG *)xd->cur_buf, cm, optimized_loop_restoration,
              pbi->tile_workers, pbi->num_workers, &pbi->lr_row_sync,
              &pbi->lr_ctxt);
        } else {
          av1_loop_restoration_filter_frame((YV12_BUFFER_CONFIG *)xd->cur_buf,
                                            cm, optimized_loop_restoration,
                                            &pbi->lr_ctxt);
        }
      }
    } else {
      // In no cdef and no superres case. Provide an optimized version of
      // loop_restoration_filter.
      if (do_loop_restoration) {
        if (pbi->num_workers > 1) {
          av1_loop_restoration_filter_frame_mt(
              (YV12_BUFFER_CONFIG *)xd->cur_buf, cm, optimized_loop_restoration,
              pbi->tile_workers, pbi->num_workers, &pbi->lr_row_sync,
              &pbi->lr_ctxt);
        } else {
          av1_loop_restoration_filter_frame((YV12_BUFFER_CONFIG *)xd->cur_buf,
                                            cm, optimized_loop_restoration,
                                            &pbi->lr_ctxt);
        }
      }
    }
  }

  if (!xd->corrupted) {
    if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
      *cm->fc = pbi->tile_data[cm->context_update_tile_id].tctx;
      av1_reset_cdf_symbol_counters(cm->fc);
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
  if (!cm->large_scale_tile) {
    cm->frame_contexts[cm->new_fb_idx] = *cm->fc;
  }
}
