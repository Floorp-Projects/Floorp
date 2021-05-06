// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_MODULAR_TRANSFORM_SUBTRACTGREEN_H_
#define LIB_JXL_MODULAR_TRANSFORM_SUBTRACTGREEN_H_

#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/modular_image.h"

namespace jxl {

template <int transform_type>
void InvSubtractGreenRow(const pixel_type* in0, const pixel_type* in1,
                         const pixel_type* in2, pixel_type* out0,
                         pixel_type* out1, pixel_type* out2, size_t w) {
  static_assert(transform_type >= 0 && transform_type < 7,
                "Invalid transform type");
  int second = transform_type >> 1;
  int third = transform_type & 1;
  for (size_t x = 0; x < w; x++) {
    if (transform_type == 6) {
      pixel_type_w Y = in0[x];
      pixel_type_w Co = in1[x];
      pixel_type_w Cg = in2[x];
      pixel_type_w tmp = Y - (Cg >> 1);
      pixel_type_w G = Cg + tmp;
      pixel_type_w B = tmp - (Co >> 1);
      pixel_type_w R = B + Co;
      out0[x] = ClampToRange<pixel_type>(R);
      out1[x] = ClampToRange<pixel_type>(G);
      out2[x] = ClampToRange<pixel_type>(B);
    } else {
      pixel_type_w First = in0[x];
      pixel_type_w Second = in1[x];
      pixel_type_w Third = in2[x];
      if (third) Third = Third + First;
      if (second == 1) {
        Second = Second + First;
      } else if (second == 2) {
        Second = Second + ((First + Third) >> 1);
      }
      out0[x] = ClampToRange<pixel_type>(First);
      out1[x] = ClampToRange<pixel_type>(Second);
      out2[x] = ClampToRange<pixel_type>(Third);
    }
  }
}

Status InvSubtractGreen(Image& input, size_t begin_c, size_t rct_type) {
  size_t m = begin_c;
  if (input.nb_channels + input.nb_meta_channels < begin_c + 3) {
    return JXL_FAILURE(
        "Invalid number of channels to apply inverse subtract_green.");
  }
  Channel& c0 = input.channel[m + 0];
  Channel& c1 = input.channel[m + 1];
  Channel& c2 = input.channel[m + 2];
  size_t w = c0.w;
  size_t h = c0.h;
  if (c0.plane.xsize() < w || c0.plane.ysize() < h || c1.plane.xsize() < w ||
      c1.plane.ysize() < h || c2.plane.xsize() < w || c2.plane.ysize() < h ||
      c1.w != w || c1.h != h || c2.w != w || c2.h != h) {
    return JXL_FAILURE(
        "Invalid channel dimensions to apply inverse subtract_green (maybe "
        "chroma is subsampled?).");
  }
  if (rct_type == 0) {  // noop
    return true;
  }
  // Permutation: 0=RGB, 1=GBR, 2=BRG, 3=RBG, 4=GRB, 5=BGR
  int permutation = rct_type / 7;
  JXL_CHECK(permutation < 6);
  // 0-5 values have the low bit corresponding to Third and the high bits
  // corresponding to Second. 6 corresponds to YCoCg.
  //
  // Second: 0=nop, 1=SubtractFirst, 2=SubtractAvgFirstThird
  //
  // Third: 0=nop, 1=SubtractFirst
  int custom = rct_type % 7;
  // Special case: permute-only. Swap channels around.
  if (custom == 0) {
    Channel ch0 = std::move(input.channel[m]);
    Channel ch1 = std::move(input.channel[m + 1]);
    Channel ch2 = std::move(input.channel[m + 2]);
    input.channel[m + (permutation % 3)] = std::move(ch0);
    input.channel[m + ((permutation + 1 + permutation / 3) % 3)] =
        std::move(ch1);
    input.channel[m + ((permutation + 2 - permutation / 3) % 3)] =
        std::move(ch2);
    return true;
  }
  constexpr decltype(&InvSubtractGreenRow<0>) inv_subtract_green_row[] = {
      InvSubtractGreenRow<0>, InvSubtractGreenRow<1>, InvSubtractGreenRow<2>,
      InvSubtractGreenRow<3>, InvSubtractGreenRow<4>, InvSubtractGreenRow<5>,
      InvSubtractGreenRow<6>};
  for (size_t y = 0; y < h; y++) {
    const pixel_type* in0 = input.channel[m].Row(y);
    const pixel_type* in1 = input.channel[m + 1].Row(y);
    const pixel_type* in2 = input.channel[m + 2].Row(y);
    pixel_type* out0 = input.channel[m + (permutation % 3)].Row(y);
    pixel_type* out1 =
        input.channel[m + ((permutation + 1 + permutation / 3) % 3)].Row(y);
    pixel_type* out2 =
        input.channel[m + ((permutation + 2 - permutation / 3) % 3)].Row(y);
    inv_subtract_green_row[custom](in0, in1, in2, out0, out1, out2, w);
  }
  return true;
}

Status FwdSubtractGreen(Image& input, size_t begin_c, size_t rct_type) {
  if (input.nb_channels + input.nb_meta_channels < begin_c + 3) {
    return false;
  }
  if (rct_type == 0) {  // noop
    return false;
  }
  // Permutation: 0=RGB, 1=GBR, 2=BRG, 3=RBG, 4=GRB, 5=BGR
  int permutation = rct_type / 7;
  // 0-5 values have the low bit corresponding to Third and the high bits
  // corresponding to Second. 6 corresponds to YCoCg.
  //
  // Second: 0=nop, 1=SubtractFirst, 2=SubtractAvgFirstThird
  //
  // Third: 0=nop, 1=SubtractFirst
  int custom = rct_type % 7;
  size_t m = begin_c;
  size_t w = input.channel[m + 0].w;
  size_t h = input.channel[m + 0].h;
  if (input.channel[m + 1].w < w || input.channel[m + 1].h < h ||
      input.channel[m + 2].w < w || input.channel[m + 2].h < h) {
    return JXL_FAILURE("Invalid channel dimensions to apply subtract_green.");
  }
  int second = (custom % 7) >> 1;
  int third = (custom % 7) & 1;
  for (size_t y = 0; y < h; y++) {
    const pixel_type* in0 = input.channel[m + (permutation % 3)].Row(y);
    const pixel_type* in1 =
        input.channel[m + ((permutation + 1 + permutation / 3) % 3)].Row(y);
    const pixel_type* in2 =
        input.channel[m + ((permutation + 2 - permutation / 3) % 3)].Row(y);
    pixel_type* out0 = input.channel[m].Row(y);
    pixel_type* out1 = input.channel[m + 1].Row(y);
    pixel_type* out2 = input.channel[m + 2].Row(y);
    for (size_t x = 0; x < w; x++) {
      if (custom == 6) {
        pixel_type R = in0[x];
        pixel_type G = in1[x];
        pixel_type B = in2[x];
        out1[x] = R - B;
        pixel_type tmp = B + (out1[x] >> 1);
        out2[x] = G - tmp;
        out0[x] = tmp + (out2[x] >> 1);
      } else {
        pixel_type First = in0[x];
        pixel_type Second = in1[x];
        pixel_type Third = in2[x];
        if (second == 1) {
          Second = Second - First;
        } else if (second == 2) {
          Second = Second - ((First + Third) >> 1);
        }
        if (third) Third = Third - First;
        out0[x] = First;
        out1[x] = Second;
        out2[x] = Third;
      }
    }
  }
  return true;
}

}  // namespace jxl

#endif  // LIB_JXL_MODULAR_TRANSFORM_SUBTRACTGREEN_H_
