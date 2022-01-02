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
#include "aom_dsp/daalaboolwriter.h"

void aom_daala_start_encode(daala_writer *br, uint8_t *source) {
  br->buffer = source;
  br->pos = 0;
  od_ec_enc_init(&br->ec, 62025);
}

int aom_daala_stop_encode(daala_writer *br) {
  int nb_bits;
  uint32_t daala_bytes;
  unsigned char *daala_data;
  daala_data = od_ec_enc_done(&br->ec, &daala_bytes);
  nb_bits = od_ec_enc_tell(&br->ec);
  memcpy(br->buffer, daala_data, daala_bytes);
  br->pos = daala_bytes;
  od_ec_enc_clear(&br->ec);
  return nb_bits;
}
