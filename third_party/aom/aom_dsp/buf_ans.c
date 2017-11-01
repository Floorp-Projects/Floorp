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

#include <string.h>

#include "aom_dsp/buf_ans.h"
#include "aom_mem/aom_mem.h"
#include "aom/internal/aom_codec_internal.h"

void aom_buf_ans_alloc(struct BufAnsCoder *c,
                       struct aom_internal_error_info *error) {
  c->error = error;
  assert(c->size > 1);
  AOM_CHECK_MEM_ERROR(error, c->buf, aom_malloc(c->size * sizeof(*c->buf)));
  // Initialize to overfull to trigger the assert in write.
  c->offset = c->size + 1;
}

void aom_buf_ans_free(struct BufAnsCoder *c) {
  aom_free(c->buf);
  c->buf = NULL;
  c->size = 0;
}

#if !ANS_MAX_SYMBOLS
void aom_buf_ans_grow(struct BufAnsCoder *c) {
  struct buffered_ans_symbol *new_buf = NULL;
  int new_size = c->size * 2;
  AOM_CHECK_MEM_ERROR(c->error, new_buf,
                      aom_malloc(new_size * sizeof(*new_buf)));
  memcpy(new_buf, c->buf, c->size * sizeof(*c->buf));
  aom_free(c->buf);
  c->buf = new_buf;
  c->size = new_size;
}
#endif

void aom_buf_ans_flush(struct BufAnsCoder *const c) {
  int offset;
#if ANS_MAX_SYMBOLS
  if (c->offset == 0) return;
#endif
  assert(c->offset > 0);
  offset = c->offset - 1;
  // Code the first symbol such that it brings the state to the smallest normal
  // state from an initial state that would have been a subnormal/refill state.
  if (c->buf[offset].method == ANS_METHOD_RANS) {
    c->ans.state += c->buf[offset].val_start;
  } else {
    c->ans.state += c->buf[offset].val_start ? c->buf[offset].prob : 0;
  }
  for (offset = offset - 1; offset >= 0; --offset) {
    if (c->buf[offset].method == ANS_METHOD_RANS) {
      rans_write(&c->ans, c->buf[offset].val_start, c->buf[offset].prob);
    } else {
      rabs_write(&c->ans, (uint8_t)c->buf[offset].val_start,
                 (AnsP8)c->buf[offset].prob);
    }
  }
  c->offset = 0;
  c->output_bytes += ans_write_end(&c->ans);
}
