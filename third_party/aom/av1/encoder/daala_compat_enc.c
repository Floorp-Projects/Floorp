/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "encint.h"

void od_encode_checkpoint(const daala_enc_ctx *enc, od_rollback_buffer *rbuf) {
#if !CONFIG_ANS
  od_ec_enc_checkpoint(&rbuf->ec, &enc->w.ec);
#else
#error "CONFIG_PVQ currently requires !CONFIG_ANS."
#endif
  OD_COPY(&rbuf->adapt, enc->state.adapt, 1);
}

void od_encode_rollback(daala_enc_ctx *enc, const od_rollback_buffer *rbuf) {
#if !CONFIG_ANS
  od_ec_enc_rollback(&enc->w.ec, &rbuf->ec);
#else
#error "CONFIG_PVQ currently requires !CONFIG_ANS."
#endif
  OD_COPY(enc->state.adapt, &rbuf->adapt, 1);
}
