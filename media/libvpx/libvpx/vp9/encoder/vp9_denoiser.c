/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_scale/yv12config.h"
#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/encoder/vp9_context_tree.h"
#include "vp9/encoder/vp9_denoiser.h"
#include "vp9/encoder/vp9_encoder.h"

#ifdef OUTPUT_YUV_DENOISED
static void make_grayscale(YV12_BUFFER_CONFIG *yuv);
#endif

static int absdiff_thresh(BLOCK_SIZE bs, int increase_denoising) {
  (void)bs;
  return 3 + (increase_denoising ? 1 : 0);
}

static int delta_thresh(BLOCK_SIZE bs, int increase_denoising) {
  (void)bs;
  (void)increase_denoising;
  return 4;
}

static int noise_motion_thresh(BLOCK_SIZE bs, int increase_denoising) {
  (void)bs;
  (void)increase_denoising;
  return 625;
}

static unsigned int sse_thresh(BLOCK_SIZE bs, int increase_denoising) {
  return (1 << num_pels_log2_lookup[bs]) * (increase_denoising ? 80 : 40);
}

static int sse_diff_thresh(BLOCK_SIZE bs, int increase_denoising,
                           int motion_magnitude) {
  if (motion_magnitude >
      noise_motion_thresh(bs, increase_denoising)) {
    if (increase_denoising)
      return (1 << num_pels_log2_lookup[bs]) << 2;
    else
      return 0;
  } else {
    return (1 << num_pels_log2_lookup[bs]) << 4;
  }
}

static int total_adj_weak_thresh(BLOCK_SIZE bs, int increase_denoising) {
  return (1 << num_pels_log2_lookup[bs]) * (increase_denoising ? 3 : 2);
}

// TODO(jackychen): If increase_denoising is enabled in the future,
// we might need to update the code for calculating 'total_adj' in
// case the C code is not bit-exact with corresponding sse2 code.
int vp9_denoiser_filter_c(const uint8_t *sig, int sig_stride,
                          const uint8_t *mc_avg,
                          int mc_avg_stride,
                          uint8_t *avg, int avg_stride,
                          int increase_denoising,
                          BLOCK_SIZE bs,
                          int motion_magnitude) {
  int r, c;
  const uint8_t *sig_start = sig;
  const uint8_t *mc_avg_start = mc_avg;
  uint8_t *avg_start = avg;
  int diff, adj, absdiff, delta;
  int adj_val[] = {3, 4, 6};
  int total_adj = 0;
  int shift_inc = 1;

  // If motion_magnitude is small, making the denoiser more aggressive by
  // increasing the adjustment for each level. Add another increment for
  // blocks that are labeled for increase denoising.
  if (motion_magnitude <= MOTION_MAGNITUDE_THRESHOLD) {
    if (increase_denoising) {
      shift_inc = 2;
    }
    adj_val[0] += shift_inc;
    adj_val[1] += shift_inc;
    adj_val[2] += shift_inc;
  }

  // First attempt to apply a strong temporal denoising filter.
  for (r = 0; r < (4 << b_height_log2_lookup[bs]); ++r) {
    for (c = 0; c < (4 << b_width_log2_lookup[bs]); ++c) {
      diff = mc_avg[c] - sig[c];
      absdiff = abs(diff);

      if (absdiff <= absdiff_thresh(bs, increase_denoising)) {
        avg[c] = mc_avg[c];
        total_adj += diff;
      } else {
        switch (absdiff) {
          case 4: case 5: case 6: case 7:
            adj = adj_val[0];
            break;
          case 8: case 9: case 10: case 11:
          case 12: case 13: case 14: case 15:
            adj = adj_val[1];
            break;
          default:
            adj = adj_val[2];
        }
        if (diff > 0) {
          avg[c] = VPXMIN(UINT8_MAX, sig[c] + adj);
          total_adj += adj;
        } else {
          avg[c] = VPXMAX(0, sig[c] - adj);
          total_adj -= adj;
        }
      }
    }
    sig += sig_stride;
    avg += avg_stride;
    mc_avg += mc_avg_stride;
  }

  // If the strong filter did not modify the signal too much, we're all set.
  if (abs(total_adj) <= total_adj_strong_thresh(bs, increase_denoising)) {
    return FILTER_BLOCK;
  }

  // Otherwise, we try to dampen the filter if the delta is not too high.
  delta = ((abs(total_adj) - total_adj_strong_thresh(bs, increase_denoising))
           >> num_pels_log2_lookup[bs]) + 1;

  if (delta >= delta_thresh(bs, increase_denoising)) {
    return COPY_BLOCK;
  }

  mc_avg =  mc_avg_start;
  avg = avg_start;
  sig = sig_start;
  for (r = 0; r < (4 << b_height_log2_lookup[bs]); ++r) {
    for (c = 0; c < (4 << b_width_log2_lookup[bs]); ++c) {
      diff = mc_avg[c] - sig[c];
      adj = abs(diff);
      if (adj > delta) {
        adj = delta;
      }
      if (diff > 0) {
        // Diff positive means we made positive adjustment above
        // (in first try/attempt), so now make negative adjustment to bring
        // denoised signal down.
        avg[c] = VPXMAX(0, avg[c] - adj);
        total_adj -= adj;
      } else {
        // Diff negative means we made negative adjustment above
        // (in first try/attempt), so now make positive adjustment to bring
        // denoised signal up.
        avg[c] = VPXMIN(UINT8_MAX, avg[c] + adj);
        total_adj += adj;
      }
    }
    sig += sig_stride;
    avg += avg_stride;
    mc_avg += mc_avg_stride;
  }

  // We can use the filter if it has been sufficiently dampened
  if (abs(total_adj) <= total_adj_weak_thresh(bs, increase_denoising)) {
    return FILTER_BLOCK;
  }
  return COPY_BLOCK;
}

static uint8_t *block_start(uint8_t *framebuf, int stride,
                            int mi_row, int mi_col) {
  return framebuf + (stride * mi_row  << 3) + (mi_col << 3);
}

static VP9_DENOISER_DECISION perform_motion_compensation(VP9_DENOISER *denoiser,
                                                         MACROBLOCK *mb,
                                                         BLOCK_SIZE bs,
                                                         int increase_denoising,
                                                         int mi_row,
                                                         int mi_col,
                                                         PICK_MODE_CONTEXT *ctx,
                                                         int motion_magnitude,
                                                         int is_skin,
                                                         int *zeromv_filter,
                                                         int consec_zeromv) {
  int sse_diff = ctx->zeromv_sse - ctx->newmv_sse;
  MV_REFERENCE_FRAME frame;
  MACROBLOCKD *filter_mbd = &mb->e_mbd;
  MODE_INFO *mi = filter_mbd->mi[0];
  MODE_INFO saved_mi;
  int i;
  struct buf_2d saved_dst[MAX_MB_PLANE];
  struct buf_2d saved_pre[MAX_MB_PLANE];

  frame = ctx->best_reference_frame;
  saved_mi = *mi;

  if (is_skin && (motion_magnitude > 0 || consec_zeromv < 4))
    return COPY_BLOCK;

  // Avoid denoising for small block (unless motion is small).
  // Small blocks are selected in variance partition (before encoding) and
  // will typically lie on moving areas.
  if (denoiser->denoising_level < kDenHigh &&
      motion_magnitude > 16 && bs <= BLOCK_8X8)
    return COPY_BLOCK;

  // If the best reference frame uses inter-prediction and there is enough of a
  // difference in sum-squared-error, use it.
  if (frame != INTRA_FRAME &&
      ctx->newmv_sse != UINT_MAX &&
      sse_diff > sse_diff_thresh(bs, increase_denoising, motion_magnitude)) {
    mi->ref_frame[0] = ctx->best_reference_frame;
    mi->mode = ctx->best_sse_inter_mode;
    mi->mv[0] = ctx->best_sse_mv;
  } else {
    // Otherwise, use the zero reference frame.
    frame = ctx->best_zeromv_reference_frame;
    ctx->newmv_sse = ctx->zeromv_sse;
    // Bias to last reference.
    if (frame != LAST_FRAME &&
        ((ctx->zeromv_lastref_sse < (5 * ctx->zeromv_sse) >> 2) ||
         denoiser->denoising_level >= kDenHigh)) {
      frame = LAST_FRAME;
      ctx->newmv_sse = ctx->zeromv_lastref_sse;
    }
    mi->ref_frame[0] = frame;
    mi->mode = ZEROMV;
    mi->mv[0].as_int = 0;
    ctx->best_sse_inter_mode = ZEROMV;
    ctx->best_sse_mv.as_int = 0;
    *zeromv_filter = 1;
    if (denoiser->denoising_level > kDenMedium) {
      motion_magnitude = 0;
    }
  }

  if (ctx->newmv_sse > sse_thresh(bs, increase_denoising)) {
    // Restore everything to its original state
    *mi = saved_mi;
    return COPY_BLOCK;
  }
  if (motion_magnitude >
     (noise_motion_thresh(bs, increase_denoising) << 3)) {
    // Restore everything to its original state
    *mi = saved_mi;
    return COPY_BLOCK;
  }

  // We will restore these after motion compensation.
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    saved_pre[i] = filter_mbd->plane[i].pre[0];
    saved_dst[i] = filter_mbd->plane[i].dst;
  }

  // Set the pointers in the MACROBLOCKD to point to the buffers in the denoiser
  // struct.
  filter_mbd->plane[0].pre[0].buf =
      block_start(denoiser->running_avg_y[frame].y_buffer,
                  denoiser->running_avg_y[frame].y_stride,
                  mi_row, mi_col);
  filter_mbd->plane[0].pre[0].stride =
      denoiser->running_avg_y[frame].y_stride;
  filter_mbd->plane[1].pre[0].buf =
       block_start(denoiser->running_avg_y[frame].u_buffer,
                  denoiser->running_avg_y[frame].uv_stride,
                  mi_row, mi_col);
  filter_mbd->plane[1].pre[0].stride =
      denoiser->running_avg_y[frame].uv_stride;
  filter_mbd->plane[2].pre[0].buf =
      block_start(denoiser->running_avg_y[frame].v_buffer,
                  denoiser->running_avg_y[frame].uv_stride,
                  mi_row, mi_col);
  filter_mbd->plane[2].pre[0].stride =
      denoiser->running_avg_y[frame].uv_stride;

  filter_mbd->plane[0].dst.buf =
      block_start(denoiser->mc_running_avg_y.y_buffer,
                  denoiser->mc_running_avg_y.y_stride,
                  mi_row, mi_col);
  filter_mbd->plane[0].dst.stride = denoiser->mc_running_avg_y.y_stride;
  filter_mbd->plane[1].dst.buf =
      block_start(denoiser->mc_running_avg_y.u_buffer,
                  denoiser->mc_running_avg_y.uv_stride,
                  mi_row, mi_col);
  filter_mbd->plane[1].dst.stride = denoiser->mc_running_avg_y.uv_stride;
  filter_mbd->plane[2].dst.buf =
      block_start(denoiser->mc_running_avg_y.v_buffer,
                  denoiser->mc_running_avg_y.uv_stride,
                  mi_row, mi_col);
  filter_mbd->plane[2].dst.stride = denoiser->mc_running_avg_y.uv_stride;

  vp9_build_inter_predictors_sby(filter_mbd, mi_row, mi_col, bs);

  // Restore everything to its original state
  *mi = saved_mi;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    filter_mbd->plane[i].pre[0] = saved_pre[i];
    filter_mbd->plane[i].dst = saved_dst[i];
  }

  return FILTER_BLOCK;
}

void vp9_denoiser_denoise(VP9_COMP *cpi, MACROBLOCK *mb,
                          int mi_row, int mi_col, BLOCK_SIZE bs,
                          PICK_MODE_CONTEXT *ctx,
                          VP9_DENOISER_DECISION *denoiser_decision) {
  int mv_col, mv_row;
  int motion_magnitude = 0;
  int zeromv_filter = 0;
  VP9_DENOISER *denoiser = &cpi->denoiser;
  VP9_DENOISER_DECISION decision = COPY_BLOCK;
  YV12_BUFFER_CONFIG avg = denoiser->running_avg_y[INTRA_FRAME];
  YV12_BUFFER_CONFIG mc_avg = denoiser->mc_running_avg_y;
  uint8_t *avg_start = block_start(avg.y_buffer, avg.y_stride, mi_row, mi_col);
  uint8_t *mc_avg_start = block_start(mc_avg.y_buffer, mc_avg.y_stride,
                                          mi_row, mi_col);
  struct buf_2d src = mb->plane[0].src;
  int is_skin = 0;
  int consec_zeromv = 0;
  mv_col = ctx->best_sse_mv.as_mv.col;
  mv_row = ctx->best_sse_mv.as_mv.row;
  motion_magnitude = mv_row * mv_row + mv_col * mv_col;

  if (cpi->use_skin_detection &&
      bs <= BLOCK_32X32 &&
      denoiser->denoising_level < kDenHigh) {
    int motion_level = (motion_magnitude < 16) ? 0 : 1;
    // If motion for current block is small/zero, compute consec_zeromv for
    // skin detection (early exit in skin detection is done for large
    // consec_zeromv when current block has small/zero motion).
    consec_zeromv = 0;
    if (motion_level == 0) {
      VP9_COMMON * const cm = &cpi->common;
      int j, i;
      // Loop through the 8x8 sub-blocks.
      const int bw = num_8x8_blocks_wide_lookup[BLOCK_64X64];
      const int bh = num_8x8_blocks_high_lookup[BLOCK_64X64];
      const int xmis = VPXMIN(cm->mi_cols - mi_col, bw);
      const int ymis = VPXMIN(cm->mi_rows - mi_row, bh);
      const int block_index = mi_row * cm->mi_cols + mi_col;
      consec_zeromv = 100;
      for (i = 0; i < ymis; i++) {
        for (j = 0; j < xmis; j++) {
          int bl_index = block_index + i * cm->mi_cols + j;
          consec_zeromv = VPXMIN(cpi->consec_zero_mv[bl_index], consec_zeromv);
          // No need to keep checking 8x8 blocks if any of the sub-blocks
          // has small consec_zeromv (since threshold for no_skin based on
          // zero/small motion in skin detection is high, i.e, > 4).
          if (consec_zeromv < 4) {
            i = ymis;
            j = xmis;
          }
        }
      }
    }
    // TODO(marpan): Compute skin detection over sub-blocks.
    is_skin = vp9_compute_skin_block(mb->plane[0].src.buf,
                                     mb->plane[1].src.buf,
                                     mb->plane[2].src.buf,
                                     mb->plane[0].src.stride,
                                     mb->plane[1].src.stride,
                                     bs,
                                     consec_zeromv,
                                     motion_level);
  }
  if (!is_skin &&
      denoiser->denoising_level == kDenHigh) {
    denoiser->increase_denoising = 1;
  } else {
    denoiser->increase_denoising = 0;
  }

  if (denoiser->denoising_level >= kDenLow)
    decision = perform_motion_compensation(denoiser, mb, bs,
                                           denoiser->increase_denoising,
                                           mi_row, mi_col, ctx,
                                           motion_magnitude,
                                           is_skin,
                                           &zeromv_filter,
                                           consec_zeromv);

  if (decision == FILTER_BLOCK) {
    decision = vp9_denoiser_filter(src.buf, src.stride,
                                 mc_avg_start, mc_avg.y_stride,
                                 avg_start, avg.y_stride,
                                 denoiser->increase_denoising,
                                 bs, motion_magnitude);
  }

  if (decision == FILTER_BLOCK) {
    vpx_convolve_copy(avg_start, avg.y_stride, src.buf, src.stride,
                      NULL, 0, NULL, 0,
                      num_4x4_blocks_wide_lookup[bs] << 2,
                      num_4x4_blocks_high_lookup[bs] << 2);
  } else {  // COPY_BLOCK
    vpx_convolve_copy(src.buf, src.stride, avg_start, avg.y_stride,
                      NULL, 0, NULL, 0,
                      num_4x4_blocks_wide_lookup[bs] << 2,
                      num_4x4_blocks_high_lookup[bs] << 2);
  }
  *denoiser_decision = decision;
  if (decision == FILTER_BLOCK && zeromv_filter == 1)
    *denoiser_decision = FILTER_ZEROMV_BLOCK;
}

static void copy_frame(YV12_BUFFER_CONFIG * const dest,
                       const YV12_BUFFER_CONFIG * const src) {
  int r;
  const uint8_t *srcbuf = src->y_buffer;
  uint8_t *destbuf = dest->y_buffer;

  assert(dest->y_width == src->y_width);
  assert(dest->y_height == src->y_height);

  for (r = 0; r < dest->y_height; ++r) {
    memcpy(destbuf, srcbuf, dest->y_width);
    destbuf += dest->y_stride;
    srcbuf += src->y_stride;
  }
}

static void swap_frame_buffer(YV12_BUFFER_CONFIG * const dest,
                              YV12_BUFFER_CONFIG * const src) {
  uint8_t *tmp_buf = dest->y_buffer;
  assert(dest->y_width == src->y_width);
  assert(dest->y_height == src->y_height);
  dest->y_buffer = src->y_buffer;
  src->y_buffer = tmp_buf;
}

void vp9_denoiser_update_frame_info(VP9_DENOISER *denoiser,
                                    YV12_BUFFER_CONFIG src,
                                    FRAME_TYPE frame_type,
                                    int refresh_alt_ref_frame,
                                    int refresh_golden_frame,
                                    int refresh_last_frame,
                                    int resized) {
  // Copy source into denoised reference buffers on KEY_FRAME or
  // if the just encoded frame was resized.
  if (frame_type == KEY_FRAME || resized != 0 || denoiser->reset) {
    int i;
    // Start at 1 so as not to overwrite the INTRA_FRAME
    for (i = 1; i < MAX_REF_FRAMES; ++i)
      copy_frame(&denoiser->running_avg_y[i], &src);
    denoiser->reset = 0;
    return;
  }

  // If more than one refresh occurs, must copy frame buffer.
  if ((refresh_alt_ref_frame + refresh_golden_frame + refresh_last_frame)
      > 1) {
    if (refresh_alt_ref_frame) {
      copy_frame(&denoiser->running_avg_y[ALTREF_FRAME],
                 &denoiser->running_avg_y[INTRA_FRAME]);
    }
    if (refresh_golden_frame) {
      copy_frame(&denoiser->running_avg_y[GOLDEN_FRAME],
                 &denoiser->running_avg_y[INTRA_FRAME]);
    }
    if (refresh_last_frame) {
      copy_frame(&denoiser->running_avg_y[LAST_FRAME],
                 &denoiser->running_avg_y[INTRA_FRAME]);
    }
  } else {
    if (refresh_alt_ref_frame) {
      swap_frame_buffer(&denoiser->running_avg_y[ALTREF_FRAME],
                        &denoiser->running_avg_y[INTRA_FRAME]);
    }
    if (refresh_golden_frame) {
      swap_frame_buffer(&denoiser->running_avg_y[GOLDEN_FRAME],
                        &denoiser->running_avg_y[INTRA_FRAME]);
    }
    if (refresh_last_frame) {
      swap_frame_buffer(&denoiser->running_avg_y[LAST_FRAME],
                        &denoiser->running_avg_y[INTRA_FRAME]);
    }
  }
}

void vp9_denoiser_reset_frame_stats(PICK_MODE_CONTEXT *ctx) {
  ctx->zeromv_sse = UINT_MAX;
  ctx->newmv_sse = UINT_MAX;
  ctx->zeromv_lastref_sse = UINT_MAX;
  ctx->best_sse_mv.as_int = 0;
}

void vp9_denoiser_update_frame_stats(MODE_INFO *mi, unsigned int sse,
                                     PREDICTION_MODE mode,
                                     PICK_MODE_CONTEXT *ctx) {
  if (mi->mv[0].as_int == 0 && sse < ctx->zeromv_sse) {
    ctx->zeromv_sse = sse;
    ctx->best_zeromv_reference_frame = mi->ref_frame[0];
    if (mi->ref_frame[0] == LAST_FRAME)
      ctx->zeromv_lastref_sse = sse;
  }

  if (mi->mv[0].as_int != 0 && sse < ctx->newmv_sse) {
    ctx->newmv_sse = sse;
    ctx->best_sse_inter_mode = mode;
    ctx->best_sse_mv = mi->mv[0];
    ctx->best_reference_frame = mi->ref_frame[0];
  }
}

int vp9_denoiser_alloc(VP9_DENOISER *denoiser, int width, int height,
                       int ssx, int ssy,
#if CONFIG_VP9_HIGHBITDEPTH
                       int use_highbitdepth,
#endif
                       int border) {
  int i, fail;
  const int legacy_byte_alignment = 0;
  assert(denoiser != NULL);

  for (i = 0; i < MAX_REF_FRAMES; ++i) {
    fail = vpx_alloc_frame_buffer(&denoiser->running_avg_y[i], width, height,
                                  ssx, ssy,
#if CONFIG_VP9_HIGHBITDEPTH
                                  use_highbitdepth,
#endif
                                  border, legacy_byte_alignment);
    if (fail) {
      vp9_denoiser_free(denoiser);
      return 1;
    }
#ifdef OUTPUT_YUV_DENOISED
    make_grayscale(&denoiser->running_avg_y[i]);
#endif
  }

  fail = vpx_alloc_frame_buffer(&denoiser->mc_running_avg_y, width, height,
                                ssx, ssy,
#if CONFIG_VP9_HIGHBITDEPTH
                                use_highbitdepth,
#endif
                                border, legacy_byte_alignment);
  if (fail) {
    vp9_denoiser_free(denoiser);
    return 1;
  }

  fail = vpx_alloc_frame_buffer(&denoiser->last_source, width, height,
                                ssx, ssy,
#if CONFIG_VP9_HIGHBITDEPTH
                                use_highbitdepth,
#endif
                                border, legacy_byte_alignment);
  if (fail) {
    vp9_denoiser_free(denoiser);
    return 1;
  }
#ifdef OUTPUT_YUV_DENOISED
  make_grayscale(&denoiser->running_avg_y[i]);
#endif
  denoiser->increase_denoising = 0;
  denoiser->frame_buffer_initialized = 1;
  denoiser->denoising_level = kDenLow;
  denoiser->prev_denoising_level = kDenLow;
  denoiser->reset = 0;
  return 0;
}

void vp9_denoiser_free(VP9_DENOISER *denoiser) {
  int i;
  denoiser->frame_buffer_initialized = 0;
  if (denoiser == NULL) {
    return;
  }
  for (i = 0; i < MAX_REF_FRAMES; ++i) {
    vpx_free_frame_buffer(&denoiser->running_avg_y[i]);
  }
  vpx_free_frame_buffer(&denoiser->mc_running_avg_y);
  vpx_free_frame_buffer(&denoiser->last_source);
}

void vp9_denoiser_set_noise_level(VP9_DENOISER *denoiser,
                                  int noise_level) {
  denoiser->denoising_level = noise_level;
  if (denoiser->denoising_level > kDenLowLow &&
      denoiser->prev_denoising_level == kDenLowLow)
    denoiser->reset = 1;
  else
    denoiser->reset = 0;
  denoiser->prev_denoising_level = denoiser->denoising_level;
}

#ifdef OUTPUT_YUV_DENOISED
static void make_grayscale(YV12_BUFFER_CONFIG *yuv) {
  int r, c;
  uint8_t *u = yuv->u_buffer;
  uint8_t *v = yuv->v_buffer;

  for (r = 0; r < yuv->uv_height; ++r) {
    for (c = 0; c < yuv->uv_width; ++c) {
      u[c] = UINT8_MAX / 2;
      v[c] = UINT8_MAX / 2;
    }
    u += yuv->uv_stride;
    v += yuv->uv_stride;
  }
}
#endif
