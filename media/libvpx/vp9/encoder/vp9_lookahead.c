/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <assert.h>
#include <stdlib.h>

#include "./vpx_config.h"
#include "vp9/common/vp9_common.h"
#include "vp9/encoder/vp9_lookahead.h"
#include "vp9/common/vp9_extend.h"

struct lookahead_ctx {
  unsigned int max_sz;         /* Absolute size of the queue */
  unsigned int sz;             /* Number of buffers currently in the queue */
  unsigned int read_idx;       /* Read index */
  unsigned int write_idx;      /* Write index */
  struct lookahead_entry *buf; /* Buffer list */
};


/* Return the buffer at the given absolute index and increment the index */
static struct lookahead_entry * pop(struct lookahead_ctx *ctx,
                                    unsigned int *idx) {
  unsigned int index = *idx;
  struct lookahead_entry *buf = ctx->buf + index;

  assert(index < ctx->max_sz);
  if (++index >= ctx->max_sz)
    index -= ctx->max_sz;
  *idx = index;
  return buf;
}


void vp9_lookahead_destroy(struct lookahead_ctx *ctx) {
  if (ctx) {
    if (ctx->buf) {
      unsigned int i;

      for (i = 0; i < ctx->max_sz; i++)
        vp9_free_frame_buffer(&ctx->buf[i].img);
      free(ctx->buf);
    }
    free(ctx);
  }
}


struct lookahead_ctx * vp9_lookahead_init(unsigned int width,
                                          unsigned int height,
                                          unsigned int subsampling_x,
                                          unsigned int subsampling_y,
                                          unsigned int depth) {
  struct lookahead_ctx *ctx = NULL;

  // Clamp the lookahead queue depth
  depth = clamp(depth, 1, MAX_LAG_BUFFERS);

  // Allocate the lookahead structures
  ctx = calloc(1, sizeof(*ctx));
  if (ctx) {
    unsigned int i;
    ctx->max_sz = depth;
    ctx->buf = calloc(depth, sizeof(*ctx->buf));
    if (!ctx->buf)
      goto bail;
    for (i = 0; i < depth; i++)
      if (vp9_alloc_frame_buffer(&ctx->buf[i].img,
                                 width, height, subsampling_x, subsampling_y,
                                 VP9BORDERINPIXELS))
        goto bail;
  }
  return ctx;
 bail:
  vp9_lookahead_destroy(ctx);
  return NULL;
}

#define USE_PARTIAL_COPY 0

int vp9_lookahead_push(struct lookahead_ctx *ctx, YV12_BUFFER_CONFIG   *src,
                       int64_t ts_start, int64_t ts_end, unsigned int flags,
                       unsigned char *active_map) {
  struct lookahead_entry *buf;
#if USE_PARTIAL_COPY
  int row, col, active_end;
  int mb_rows = (src->y_height + 15) >> 4;
  int mb_cols = (src->y_width + 15) >> 4;
#endif

  if (ctx->sz + 1 > ctx->max_sz)
    return 1;
  ctx->sz++;
  buf = pop(ctx, &ctx->write_idx);

#if USE_PARTIAL_COPY
  // TODO(jkoleszar): This is disabled for now, as
  // vp9_copy_and_extend_frame_with_rect is not subsampling/alpha aware.

  // Only do this partial copy if the following conditions are all met:
  // 1. Lookahead queue has has size of 1.
  // 2. Active map is provided.
  // 3. This is not a key frame, golden nor altref frame.
  if (ctx->max_sz == 1 && active_map && !flags) {
    for (row = 0; row < mb_rows; ++row) {
      col = 0;

      while (1) {
        // Find the first active macroblock in this row.
        for (; col < mb_cols; ++col) {
          if (active_map[col])
            break;
        }

        // No more active macroblock in this row.
        if (col == mb_cols)
          break;

        // Find the end of active region in this row.
        active_end = col;

        for (; active_end < mb_cols; ++active_end) {
          if (!active_map[active_end])
            break;
        }

        // Only copy this active region.
        vp9_copy_and_extend_frame_with_rect(src, &buf->img,
                                            row << 4,
                                            col << 4, 16,
                                            (active_end - col) << 4);

        // Start again from the end of this active region.
        col = active_end;
      }

      active_map += mb_cols;
    }
  } else {
    vp9_copy_and_extend_frame(src, &buf->img);
  }
#else
  // Partial copy not implemented yet
  vp9_copy_and_extend_frame(src, &buf->img);
#endif

  buf->ts_start = ts_start;
  buf->ts_end = ts_end;
  buf->flags = flags;
  return 0;
}


struct lookahead_entry * vp9_lookahead_pop(struct lookahead_ctx *ctx,
                                           int drain) {
  struct lookahead_entry *buf = NULL;

  if (ctx->sz && (drain || ctx->sz == ctx->max_sz)) {
    buf = pop(ctx, &ctx->read_idx);
    ctx->sz--;
  }
  return buf;
}


struct lookahead_entry * vp9_lookahead_peek(struct lookahead_ctx *ctx,
                                            int index) {
  struct lookahead_entry *buf = NULL;

  assert(index < (int)ctx->max_sz);
  if (index < (int)ctx->sz) {
    index += ctx->read_idx;
    if (index >= (int)ctx->max_sz)
      index -= ctx->max_sz;
    buf = ctx->buf + index;
  }
  return buf;
}

unsigned int vp9_lookahead_depth(struct lookahead_ctx *ctx) {
  return ctx->sz;
}
