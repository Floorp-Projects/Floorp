/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include "./macros_msa.h"

void vpx_plane_add_noise_msa(uint8_t *start_ptr, char *noise,
                             char blackclamp[16], char whiteclamp[16],
                             char bothclamp[16], uint32_t width,
                             uint32_t height, int32_t pitch) {
  uint32_t i, j;

  for (i = 0; i < height / 2; ++i) {
    uint8_t *pos0_ptr = start_ptr + (2 * i) * pitch;
    int8_t *ref0_ptr = (int8_t *)(noise + (rand() & 0xff));
    uint8_t *pos1_ptr = start_ptr + (2 * i + 1) * pitch;
    int8_t *ref1_ptr = (int8_t *)(noise + (rand() & 0xff));
    for (j = width / 16; j--;) {
      v16i8 temp00_s, temp01_s;
      v16u8 temp00, temp01, black_clamp, white_clamp;
      v16u8 pos0, ref0, pos1, ref1;
      v16i8 const127 = __msa_ldi_b(127);

      pos0 = LD_UB(pos0_ptr);
      ref0 = LD_UB(ref0_ptr);
      pos1 = LD_UB(pos1_ptr);
      ref1 = LD_UB(ref1_ptr);
      black_clamp = (v16u8)__msa_fill_b(blackclamp[0]);
      white_clamp = (v16u8)__msa_fill_b(whiteclamp[0]);
      temp00 = (pos0 < black_clamp);
      pos0 = __msa_bmnz_v(pos0, black_clamp, temp00);
      temp01 = (pos1 < black_clamp);
      pos1 = __msa_bmnz_v(pos1, black_clamp, temp01);
      XORI_B2_128_UB(pos0, pos1);
      temp00_s = __msa_adds_s_b((v16i8)white_clamp, const127);
      temp00 = (v16u8)(temp00_s < pos0);
      pos0 = (v16u8)__msa_bmnz_v((v16u8)pos0, (v16u8)temp00_s, temp00);
      temp01_s = __msa_adds_s_b((v16i8)white_clamp, const127);
      temp01 = (temp01_s < pos1);
      pos1 = (v16u8)__msa_bmnz_v((v16u8)pos1, (v16u8)temp01_s, temp01);
      XORI_B2_128_UB(pos0, pos1);
      pos0 += ref0;
      ST_UB(pos0, pos0_ptr);
      pos1 += ref1;
      ST_UB(pos1, pos1_ptr);
      pos0_ptr += 16;
      pos1_ptr += 16;
      ref0_ptr += 16;
      ref1_ptr += 16;
    }
  }
}
