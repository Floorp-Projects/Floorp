// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/modular/transform/enc_squeeze.h"

#include <stdlib.h>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/squeeze.h"
#include "lib/jxl/modular/transform/transform.h"

namespace jxl {

void FwdHSqueeze(Image &input, int c, int rc) {
  const Channel &chin = input.channel[c];

  JXL_DEBUG_V(4, "Doing horizontal squeeze of channel %i to new channel %i", c,
              rc);

  Channel chout((chin.w + 1) / 2, chin.h, chin.hshift + 1, chin.vshift);
  Channel chout_residual(chin.w - chout.w, chout.h, chin.hshift + 1,
                         chin.vshift);

  for (size_t y = 0; y < chout.h; y++) {
    const pixel_type *JXL_RESTRICT p_in = chin.Row(y);
    pixel_type *JXL_RESTRICT p_out = chout.Row(y);
    pixel_type *JXL_RESTRICT p_res = chout_residual.Row(y);
    for (size_t x = 0; x < chout_residual.w; x++) {
      pixel_type A = p_in[x * 2];
      pixel_type B = p_in[x * 2 + 1];
      pixel_type avg = (A + B + (A > B)) >> 1;
      p_out[x] = avg;

      pixel_type diff = A - B;

      pixel_type next_avg = avg;
      if (x + 1 < chout_residual.w) {
        next_avg = (p_in[x * 2 + 2] + p_in[x * 2 + 3] +
                    (p_in[x * 2 + 2] > p_in[x * 2 + 3])) >>
                   1;  // which will be chout.value(y,x+1)
      } else if (chin.w & 1)
        next_avg = p_in[x * 2 + 2];
      pixel_type left = (x > 0 ? p_in[x * 2 - 1] : avg);
      pixel_type tendency = SmoothTendency(left, avg, next_avg);

      p_res[x] = diff - tendency;
    }
    if (chin.w & 1) {
      int x = chout.w - 1;
      p_out[x] = p_in[x * 2];
    }
  }
  input.channel[c] = std::move(chout);
  input.channel.insert(input.channel.begin() + rc, std::move(chout_residual));
}

void FwdVSqueeze(Image &input, int c, int rc) {
  const Channel &chin = input.channel[c];

  JXL_DEBUG_V(4, "Doing vertical squeeze of channel %i to new channel %i", c,
              rc);

  Channel chout(chin.w, (chin.h + 1) / 2, chin.hshift, chin.vshift + 1);
  Channel chout_residual(chin.w, chin.h - chout.h, chin.hshift,
                         chin.vshift + 1);
  intptr_t onerow_in = chin.plane.PixelsPerRow();
  for (size_t y = 0; y < chout_residual.h; y++) {
    const pixel_type *JXL_RESTRICT p_in = chin.Row(y * 2);
    pixel_type *JXL_RESTRICT p_out = chout.Row(y);
    pixel_type *JXL_RESTRICT p_res = chout_residual.Row(y);
    for (size_t x = 0; x < chout.w; x++) {
      pixel_type A = p_in[x];
      pixel_type B = p_in[x + onerow_in];
      pixel_type avg = (A + B + (A > B)) >> 1;
      p_out[x] = avg;

      pixel_type diff = A - B;

      pixel_type next_avg = avg;
      if (y + 1 < chout_residual.h) {
        next_avg = (p_in[x + 2 * onerow_in] + p_in[x + 3 * onerow_in] +
                    (p_in[x + 2 * onerow_in] > p_in[x + 3 * onerow_in])) >>
                   1;  // which will be chout.value(y+1,x)
      } else if (chin.h & 1) {
        next_avg = p_in[x + 2 * onerow_in];
      }
      pixel_type top =
          (y > 0 ? p_in[static_cast<ssize_t>(x) - onerow_in] : avg);
      pixel_type tendency = SmoothTendency(top, avg, next_avg);

      p_res[x] = diff - tendency;
    }
  }
  if (chin.h & 1) {
    size_t y = chout.h - 1;
    const pixel_type *p_in = chin.Row(y * 2);
    pixel_type *p_out = chout.Row(y);
    for (size_t x = 0; x < chout.w; x++) {
      p_out[x] = p_in[x];
    }
  }
  input.channel[c] = std::move(chout);
  input.channel.insert(input.channel.begin() + rc, std::move(chout_residual));
}

Status FwdSqueeze(Image &input, std::vector<SqueezeParams> parameters,
                  ThreadPool *pool) {
  if (parameters.empty()) {
    DefaultSqueezeParameters(&parameters, input);
  }
  // if nothing to do, don't do squeeze
  if (parameters.empty()) return false;
  for (size_t i = 0; i < parameters.size(); i++) {
    JXL_RETURN_IF_ERROR(
        CheckMetaSqueezeParams(parameters[i], input.channel.size()));
    bool horizontal = parameters[i].horizontal;
    bool in_place = parameters[i].in_place;
    uint32_t beginc = parameters[i].begin_c;
    uint32_t endc = parameters[i].begin_c + parameters[i].num_c - 1;
    uint32_t offset;
    if (in_place) {
      offset = endc + 1;
    } else {
      offset = input.channel.size();
    }
    for (uint32_t c = beginc; c <= endc; c++) {
      if (horizontal) {
        FwdHSqueeze(input, c, offset + c - beginc);
      } else {
        FwdVSqueeze(input, c, offset + c - beginc);
      }
    }
  }
  return true;
}

}  // namespace jxl
