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
#include "vpx_scale/yv12config.h"
#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/encoder/vp9_context_tree.h"
#include "vp9/encoder/vp9_denoiser.h"

/* The VP9 denoiser is a work-in-progress. It currently is only designed to work
 * with speed 6, though it (inexplicably) seems to also work with speed 5 (one
 * would need to modify the source code in vp9_pickmode.c and vp9_encoder.c to
 * make the calls to the vp9_denoiser_* functions when in speed 5).
 *
 * The implementation is very similar to that of the VP8 denoiser. While
 * choosing the motion vectors / reference frames, the denoiser is run, and if
 * it did not modify the signal to much, the denoised block is copied to the
 * signal.
 */

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
  return (1 << num_pels_log2_lookup[bs]) * (increase_denoising ? 60 : 40);
}

static int sse_diff_thresh(BLOCK_SIZE bs, int increase_denoising,
                           int motion_magnitude) {
  if (motion_magnitude >
      noise_motion_thresh(bs, increase_denoising)) {
    return 0;
  } else {
    return (1 << num_pels_log2_lookup[bs]) * 20;
  }
}

int total_adj_strong_thresh(BLOCK_SIZE bs, int increase_denoising) {
  return (1 << num_pels_log2_lookup[bs]) * (increase_denoising ? 3 : 2);
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
          avg[c] = MIN(UINT8_MAX, sig[c] + adj);
          total_adj += adj;
        } else {
          avg[c] = MAX(0, sig[c] - adj);
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
        avg[c] = MAX(0, avg[c] - adj);
        total_adj -= adj;
      } else {
        // Diff negative means we made negative adjustment above
        // (in first try/attempt), so now make positive adjustment to bring
        // denoised signal up.
        avg[c] = MIN(UINT8_MAX, avg[c] + adj);
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
  return framebuf + (stride * mi_row * 8) + (mi_col * 8);
}

static VP9_DENOISER_DECISION perform_motion_compensation(VP9_DENOISER *denoiser,
                                                         MACROBLOCK *mb,
                                                         BLOCK_SIZE bs,
                                                         int increase_denoising,
                                                         int mi_row,
                                                         int mi_col,
                                                         PICK_MODE_CONTEXT *ctx,
                                                         int *motion_magnitude
                                                         ) {
  int mv_col, mv_row;
  int sse_diff = ctx->zeromv_sse - ctx->newmv_sse;
  MV_REFERENCE_FRAME frame;
  MACROBLOCKD *filter_mbd = &mb->e_mbd;
  MB_MODE_INFO *mbmi = &filter_mbd->mi[0]->mbmi;
  MB_MODE_INFO saved_mbmi;
  int i, j;
  struct buf_2d saved_dst[MAX_MB_PLANE];
  struct buf_2d saved_pre[MAX_MB_PLANE][2];  // 2 pre buffers

  mv_col = ctx->best_sse_mv.as_mv.col;
  mv_row = ctx->best_sse_mv.as_mv.row;
  *motion_magnitude = mv_row * mv_row + mv_col * mv_col;
  frame = ctx->best_reference_frame;

  saved_mbmi = *mbmi;

  // If the best reference frame uses inter-prediction and there is enough of a
  // difference in sum-squared-error, use it.
  if (frame != INTRA_FRAME &&
      sse_diff > sse_diff_thresh(bs, increase_denoising, *motion_magnitude)) {
    mbmi->ref_frame[0] = ctx->best_reference_frame;
    mbmi->mode = ctx->best_sse_inter_mode;
    mbmi->mv[0] = ctx->best_sse_mv;
  } else {
    // Otherwise, use the zero reference frame.
    frame = ctx->best_zeromv_reference_frame;

    mbmi->ref_frame[0] = ctx->best_zeromv_reference_frame;
    mbmi->mode = ZEROMV;
    mbmi->mv[0].as_int = 0;

    ctx->best_sse_inter_mode = ZEROMV;
    ctx->best_sse_mv.as_int = 0;
    ctx->newmv_sse = ctx->zeromv_sse;
  }

  if (ctx->newmv_sse > sse_thresh(bs, increase_denoising)) {
    // Restore everything to its original state
    *mbmi = saved_mbmi;
    return COPY_BLOCK;
  }
  if (*motion_magnitude >
     (noise_motion_thresh(bs, increase_denoising) << 3)) {
    // Restore everything to its original state
    *mbmi = saved_mbmi;
    return COPY_BLOCK;
  }

  // We will restore these after motion compensation.
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    for (j = 0; j < 2; ++j) {
      saved_pre[i][j] = filter_mbd->plane[i].pre[j];
    }
    saved_dst[i] = filter_mbd->plane[i].dst;
  }

  // Set the pointers in the MACROBLOCKD to point to the buffers in the denoiser
  // struct.
  for (j = 0; j < 2; ++j) {
    filter_mbd->plane[0].pre[j].buf =
        block_start(denoiser->running_avg_y[frame].y_buffer,
                    denoiser->running_avg_y[frame].y_stride,
                    mi_row, mi_col);
    filter_mbd->plane[0].pre[j].stride =
        denoiser->running_avg_y[frame].y_stride;
    filter_mbd->plane[1].pre[j].buf =
        block_start(denoiser->running_avg_y[frame].u_buffer,
                    denoiser->running_avg_y[frame].uv_stride,
                    mi_row, mi_col);
    filter_mbd->plane[1].pre[j].stride =
        denoiser->running_avg_y[frame].uv_stride;
    filter_mbd->plane[2].pre[j].buf =
        block_start(denoiser->running_avg_y[frame].v_buffer,
                    denoiser->running_avg_y[frame].uv_stride,
                    mi_row, mi_col);
    filter_mbd->plane[2].pre[j].stride =
        denoiser->running_avg_y[frame].uv_stride;
  }
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

  vp9_build_inter_predictors_sby(filter_mbd, mv_row, mv_col, bs);

  // Restore everything to its original state
  *mbmi = saved_mbmi;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    for (j = 0; j < 2; ++j) {
      filter_mbd->plane[i].pre[j] = saved_pre[i][j];
    }
    filter_mbd->plane[i].dst = saved_dst[i];
  }

  mv_row = ctx->best_sse_mv.as_mv.row;
  mv_col = ctx->best_sse_mv.as_mv.col;

  return FILTER_BLOCK;
}

void vp9_denoiser_denoise(VP9_DENOISER *denoiser, MACROBLOCK *mb,
                          int mi_row, int mi_col, BLOCK_SIZE bs,
                          PICK_MODE_CONTEXT *ctx) {
  int motion_magnitude = 0;
  VP9_DENOISER_DECISION decision = FILTER_BLOCK;
  YV12_BUFFER_CONFIG avg = denoiser->running_avg_y[INTRA_FRAME];
  YV12_BUFFER_CONFIG mc_avg = denoiser->mc_running_avg_y;
  uint8_t *avg_start = block_start(avg.y_buffer, avg.y_stride, mi_row, mi_col);
  uint8_t *mc_avg_start = block_start(mc_avg.y_buffer, mc_avg.y_stride,
                                          mi_row, mi_col);
  struct buf_2d src = mb->plane[0].src;

  decision = perform_motion_compensation(denoiser, mb, bs,
                                         denoiser->increase_denoising,
                                         mi_row, mi_col, ctx,
                                         &motion_magnitude);

  if (decision == FILTER_BLOCK) {
    decision = vp9_denoiser_filter(src.buf, src.stride,
                                 mc_avg_start, mc_avg.y_stride,
                                 avg_start, avg.y_stride,
                                 0, bs, motion_magnitude);
  }

  if (decision == FILTER_BLOCK) {
    vp9_convolve_copy(avg_start, avg.y_stride, src.buf, src.stride,
                      NULL, 0, NULL, 0,
                      num_4x4_blocks_wide_lookup[bs] << 2,
                      num_4x4_blocks_high_lookup[bs] << 2);
  } else {  // COPY_BLOCK
    vp9_convolve_copy(src.buf, src.stride, avg_start, avg.y_stride,
                      NULL, 0, NULL, 0,
                      num_4x4_blocks_wide_lookup[bs] << 2,
                      num_4x4_blocks_high_lookup[bs] << 2);
  }
}

static void copy_frame(YV12_BUFFER_CONFIG dest, const YV12_BUFFER_CONFIG src) {
  int r;
  const uint8_t *srcbuf = src.y_buffer;
  uint8_t *destbuf = dest.y_buffer;

  assert(dest.y_width == src.y_width);
  assert(dest.y_height == src.y_height);

  for (r = 0; r < dest.y_height; ++r) {
    memcpy(destbuf, srcbuf, dest.y_width);
    destbuf += dest.y_stride;
    srcbuf += src.y_stride;
  }
}

static void swap_frame_buffer(YV12_BUFFER_CONFIG *dest,
                              YV12_BUFFER_CONFIG *src) {
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
                                    int refresh_last_frame) {
  if (frame_type == KEY_FRAME) {
    int i;
    // Start at 1 so as not to overwrite the INTRA_FRAME
    for (i = 1; i < MAX_REF_FRAMES; ++i)
      copy_frame(denoiser->running_avg_y[i], src);
    return;
  }

  /* For non key frames */
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

void vp9_denoiser_reset_frame_stats(PICK_MODE_CONTEXT *ctx) {
  ctx->zeromv_sse = UINT_MAX;
  ctx->newmv_sse = UINT_MAX;
}

void vp9_denoiser_update_frame_stats(MB_MODE_INFO *mbmi, unsigned int sse,
                                     PREDICTION_MODE mode,
                                     PICK_MODE_CONTEXT *ctx) {
  // TODO(tkopp): Use both MVs if possible
  if (mbmi->mv[0].as_int == 0 && sse < ctx->zeromv_sse) {
    ctx->zeromv_sse = sse;
    ctx->best_zeromv_reference_frame = mbmi->ref_frame[0];
  }

  if (mbmi->mv[0].as_int != 0 && sse < ctx->newmv_sse) {
    ctx->newmv_sse = sse;
    ctx->best_sse_inter_mode = mode;
    ctx->best_sse_mv = mbmi->mv[0];
    ctx->best_reference_frame = mbmi->ref_frame[0];
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
    fail = vp9_alloc_frame_buffer(&denoiser->running_avg_y[i], width, height,
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

  fail = vp9_alloc_frame_buffer(&denoiser->mc_running_avg_y, width, height,
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

  return 0;
}

void vp9_denoiser_free(VP9_DENOISER *denoiser) {
  int i;
  denoiser->frame_buffer_initialized = 0;
  if (denoiser == NULL) {
    return;
  }
  for (i = 0; i < MAX_REF_FRAMES; ++i) {
    vp9_free_frame_buffer(&denoiser->running_avg_y[i]);
  }
  vp9_free_frame_buffer(&denoiser->mc_running_avg_y);
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
