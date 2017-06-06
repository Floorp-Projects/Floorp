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
#include <stdio.h>
#include <string.h>
#include "aom_util/debug_util.h"

#define QUEUE_MAX_SIZE 2000000
static int result_queue[QUEUE_MAX_SIZE];
static int nsymbs_queue[QUEUE_MAX_SIZE];
static aom_cdf_prob cdf_queue[QUEUE_MAX_SIZE][16];

static int queue_r = 0;
static int queue_w = 0;
static int queue_prev_w = -1;
static int skip_r = 0;
static int skip_w = 0;

static int frame_idx_w = 0;

static int frame_idx_r = 0;

void bitstream_queue_set_frame_write(int frame_idx) { frame_idx_w = frame_idx; }

int bitstream_queue_get_frame_write(void) { return frame_idx_w; }

void bitstream_queue_set_frame_read(int frame_idx) { frame_idx_r = frame_idx; }

int bitstream_queue_get_frame_read(void) { return frame_idx_r; }

void bitstream_queue_set_skip_write(int skip) { skip_w = skip; }

void bitstream_queue_set_skip_read(int skip) { skip_r = skip; }

void bitstream_queue_record_write(void) { queue_prev_w = queue_w; }

void bitstream_queue_reset_write(void) { queue_w = queue_prev_w; }

int bitstream_queue_get_write(void) { return queue_w; }

int bitstream_queue_get_read(void) { return queue_r; }

void bitstream_queue_pop(int *result, aom_cdf_prob *cdf, int *nsymbs) {
  if (!skip_r) {
    if (queue_w == queue_r) {
      printf("buffer underflow queue_w %d queue_r %d\n", queue_w, queue_r);
      assert(0);
    }
    *result = result_queue[queue_r];
    *nsymbs = nsymbs_queue[queue_r];
    memcpy(cdf, cdf_queue[queue_r], *nsymbs * sizeof(*cdf));
    queue_r = (queue_r + 1) % QUEUE_MAX_SIZE;
  }
}

void bitstream_queue_push(int result, const aom_cdf_prob *cdf, int nsymbs) {
  if (!skip_w) {
    result_queue[queue_w] = result;
    nsymbs_queue[queue_w] = nsymbs;
    memcpy(cdf_queue[queue_w], cdf, nsymbs * sizeof(*cdf));
    queue_w = (queue_w + 1) % QUEUE_MAX_SIZE;
    if (queue_w == queue_r) {
      printf("buffer overflow queue_w %d queue_r %d\n", queue_w, queue_r);
      assert(0);
    }
  }
}
