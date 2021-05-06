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

#ifndef LIB_JXL_MODULAR_TRANSFORM_SQUEEZE_H_
#define LIB_JXL_MODULAR_TRANSFORM_SQUEEZE_H_

// Haar-like transform: halves the resolution in one direction
// A B   -> (A+B)>>1              in one channel (average)  -> same range as
// original channel
//          A-B - tendency        in a new channel ('residual' needed to make
//          the transform reversible)
//                                        -> theoretically range could be 2.5
//                                        times larger (2 times without the
//                                        'tendency'), but there should be lots
//                                        of zeroes
// Repeated application (alternating horizontal and vertical squeezes) results
// in downscaling
//
// The default coefficient ordering is low-frequency to high-frequency, as in
// M. Antonini, M. Barlaud, P. Mathieu and I. Daubechies, "Image coding using
// wavelet transform", IEEE Transactions on Image Processing, vol. 1, no. 2, pp.
// 205-220, April 1992, doi: 10.1109/83.136597.

#include <stdlib.h>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/transform.h"

#define JXL_MAX_FIRST_PREVIEW_SIZE 8

namespace jxl {

/*
        int avg=(A+B)>>1;
        int diff=(A-B);
        int rA=(diff+(avg<<1)+(diff&1))>>1;
        int rB=rA-diff;

*/
//         |A B|C D|E F|
//           p   a   n             p=avg(A,B), a=avg(C,D), n=avg(E,F)
//
// Goal: estimate C-D (avoiding ringing artifacts)
// (ensuring that in smooth areas, a zero residual corresponds to a smooth
// gradient)

// best estimate for C: (B + 2*a)/3
// best estimate for D: (n + 3*a)/4
// best estimate for C-D:  4*B - 3*n - a /12

// avoid ringing by 1) only doing this if B <= a <= n  or  B >= a >= n
// (otherwise, this is not a smooth area and we cannot really estimate C-D)
//                  2) making sure that B <= C <= D <= n  or B >= C >= D >= n

inline pixel_type_w SmoothTendency(pixel_type_w B, pixel_type_w a,
                                   pixel_type_w n) {
  pixel_type_w diff = 0;
  if (B >= a && a >= n) {
    diff = (4 * B - 3 * n - a + 6) / 12;
    //      2C = a<<1 + diff - diff&1 <= 2B  so diff - diff&1 <= 2B - 2a
    //      2D = a<<1 - diff - diff&1 >= 2n  so diff + diff&1 <= 2a - 2n
    if (diff - (diff & 1) > 2 * (B - a)) diff = 2 * (B - a) + 1;
    if (diff + (diff & 1) > 2 * (a - n)) diff = 2 * (a - n);
  } else if (B <= a && a <= n) {
    diff = (4 * B - 3 * n - a - 6) / 12;
    //      2C = a<<1 + diff + diff&1 >= 2B  so diff + diff&1 >= 2B - 2a
    //      2D = a<<1 - diff + diff&1 <= 2n  so diff - diff&1 >= 2a - 2n
    if (diff + (diff & 1) < 2 * (B - a)) diff = 2 * (B - a) - 1;
    if (diff - (diff & 1) < 2 * (a - n)) diff = 2 * (a - n);
  }
  return diff;
}

void InvHSqueeze(Image &input, int c, int rc, ThreadPool *pool) {
  const Channel &chin = input.channel[c];
  const Channel &chin_residual = input.channel[rc];
  // These must be valid since we ran MetaApply already.
  JXL_ASSERT(chin.w == DivCeil(chin.w + chin_residual.w, 2));
  JXL_ASSERT(chin.h == chin_residual.h);

  if (chin_residual.w == 0 || chin_residual.h == 0) {
    input.channel[c].resize(chin.w + chin_residual.w, chin.h);
    input.channel[c].hshift--;
    input.channel[c].hcshift--;
    return;
  }

  Channel chout(chin.w + chin_residual.w, chin.h, chin.hshift - 1, chin.vshift,
                chin.hcshift - 1, chin.vcshift);
  JXL_DEBUG_V(4,
              "Undoing horizontal squeeze of channel %i using residuals in "
              "channel %i (going from width %zu to %zu)",
              c, rc, chin.w, chout.w);
  RunOnPool(
      pool, 0, chin.h, ThreadPool::SkipInit(),
      [&](const int task, const int thread) {
        const size_t y = task;
        const pixel_type *JXL_RESTRICT p_residual = chin_residual.Row(y);
        const pixel_type *JXL_RESTRICT p_avg = chin.Row(y);
        pixel_type *JXL_RESTRICT p_out = chout.Row(y);

        // special case for x=0 so we don't have to check x>0
        pixel_type_w avg = p_avg[0];
        pixel_type_w next_avg = (1 < chin.w ? p_avg[1] : avg);
        pixel_type_w tendency = SmoothTendency(avg, avg, next_avg);
        pixel_type_w diff = p_residual[0] + tendency;
        pixel_type_w A =
            ((avg * 2) + diff + (diff > 0 ? -(diff & 1) : (diff & 1))) >> 1;
        pixel_type_w B = A - diff;
        p_out[0] = ClampToRange<pixel_type>(A);
        p_out[1] = ClampToRange<pixel_type>(B);

        for (size_t x = 1; x < chin_residual.w; x++) {
          pixel_type_w diff_minus_tendency = p_residual[x];
          pixel_type_w avg = p_avg[x];
          pixel_type_w next_avg = (x + 1 < chin.w ? p_avg[x + 1] : avg);
          pixel_type_w left = p_out[(x << 1) - 1];
          pixel_type_w tendency = SmoothTendency(left, avg, next_avg);
          pixel_type_w diff = diff_minus_tendency + tendency;
          pixel_type_w A =
              ((avg * 2) + diff + (diff > 0 ? -(diff & 1) : (diff & 1))) >> 1;
          p_out[x << 1] = ClampToRange<pixel_type>(A);
          pixel_type_w B = A - diff;
          p_out[(x << 1) + 1] = ClampToRange<pixel_type>(B);
        }
        if (chout.w & 1) p_out[chout.w - 1] = p_avg[chin.w - 1];
      },
      "InvHorizontalSqueeze");
  input.channel[c] = std::move(chout);
}

void FwdHSqueeze(Image &input, int c, int rc) {
  const Channel &chin = input.channel[c];

  JXL_DEBUG_V(4, "Doing horizontal squeeze of channel %i to new channel %i", c,
              rc);

  Channel chout((chin.w + 1) / 2, chin.h, chin.hshift + 1, chin.vshift,
                chin.hcshift + 1, chin.vcshift);
  Channel chout_residual(chin.w - chout.w, chout.h, chin.hshift + 1,
                         chin.vshift, chin.hcshift, chin.vcshift);

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

void InvVSqueeze(Image &input, int c, int rc, ThreadPool *pool) {
  const Channel &chin = input.channel[c];
  const Channel &chin_residual = input.channel[rc];
  // These must be valid since we ran MetaApply already.
  JXL_ASSERT(chin.h == DivCeil(chin.h + chin_residual.h, 2));
  JXL_ASSERT(chin.w == chin_residual.w);

  if (chin_residual.w == 0 || chin_residual.h == 0) {
    input.channel[c].resize(chin.w, chin.h + chin_residual.h);
    input.channel[c].vshift--;
    input.channel[c].vcshift--;
    return;
  }

  // Note: chin.h >= chin_residual.h and at most 1 different.
  Channel chout(chin.w, chin.h + chin_residual.h, chin.hshift, chin.vshift - 1,
                chin.hcshift, chin.vcshift - 1);
  JXL_DEBUG_V(
      4,
      "Undoing vertical squeeze of channel %i using residuals in channel "
      "%i (going from height %zu to %zu)",
      c, rc, chin.h, chout.h);

  intptr_t onerow_in = chin.plane.PixelsPerRow();
  intptr_t onerow_out = chout.plane.PixelsPerRow();
  constexpr int kColsPerThread = 64;
  RunOnPool(
      pool, 0, DivCeil(chin.w, kColsPerThread), ThreadPool::SkipInit(),
      [&](const int task, const int thread) {
        const size_t x0 = task * kColsPerThread;
        const size_t x1 = std::min((size_t)(task + 1) * kColsPerThread, chin.w);
        // We only iterate up to std::min(chin_residual.h, chin.h) which is
        // always chin_residual.h.
        for (size_t y = 0; y < chin_residual.h; y++) {
          const pixel_type *JXL_RESTRICT p_residual = chin_residual.Row(y);
          const pixel_type *JXL_RESTRICT p_avg = chin.Row(y);
          pixel_type *JXL_RESTRICT p_out = chout.Row(y << 1);
          for (size_t x = x0; x < x1; x++) {
            pixel_type_w diff_minus_tendency = p_residual[x];
            pixel_type_w avg = p_avg[x];

            pixel_type_w next_avg = avg;
            if (y + 1 < chin.h) next_avg = p_avg[x + onerow_in];
            pixel_type_w top =
                (y > 0 ? p_out[static_cast<ssize_t>(x) - onerow_out] : avg);
            pixel_type_w tendency = SmoothTendency(top, avg, next_avg);
            pixel_type_w diff = diff_minus_tendency + tendency;
            pixel_type_w out =
                ((avg * 2) + diff + (diff > 0 ? -(diff & 1) : (diff & 1))) >> 1;

            p_out[x] = ClampToRange<pixel_type>(out);
            // If the chin_residual.h == chin.h, the output has an even number
            // of rows so the next line is fine. Otherwise, this loop won't
            // write to the last output row which is handled separately.
            p_out[x + onerow_out] = ClampToRange<pixel_type>(p_out[x] - diff);
          }
        }
      },
      "InvVertSqueeze");

  if (chout.h & 1) {
    size_t y = chin.h - 1;
    const pixel_type *p_avg = chin.Row(y);
    pixel_type *p_out = chout.Row(y << 1);
    for (size_t x = 0; x < chin.w; x++) {
      p_out[x] = p_avg[x];
    }
  }
  input.channel[c] = std::move(chout);
}

void FwdVSqueeze(Image &input, int c, int rc) {
  const Channel &chin = input.channel[c];

  JXL_DEBUG_V(4, "Doing vertical squeeze of channel %i to new channel %i", c,
              rc);

  Channel chout(chin.w, (chin.h + 1) / 2, chin.hshift, chin.vshift + 1,
                chin.hcshift, chin.vcshift + 1);
  Channel chout_residual(chin.w, chin.h - chout.h, chin.hshift, chin.vshift + 1,
                         chin.hcshift, chin.vcshift);
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

void DefaultSqueezeParameters(std::vector<SqueezeParams> *parameters,
                              const Image &image) {
  int nb_channels = image.nb_channels;
  // maybe other transforms have been applied before, but let's assume the first
  // nb_channels channels still contain the 'main' data

  parameters->clear();
  size_t w = image.channel[image.nb_meta_channels].w;
  size_t h = image.channel[image.nb_meta_channels].h;
  JXL_DEBUG_V(7, "Default squeeze parameters for %zux%zu image: ", w, h);

  bool wide =
      (w >
       h);  // do horizontal first on wide images; vertical first on tall images

  if (nb_channels > 2 && image.channel[image.nb_meta_channels + 1].w == w &&
      image.channel[image.nb_meta_channels + 1].h == h) {
    // assume channels 1 and 2 are chroma, and can be squeezed first for 4:2:0
    // previews
    JXL_DEBUG_V(7, "(4:2:0 chroma), %zux%zu image", w, h);
    //        if (!wide) {
    //        parameters.push_back(0+2); // vertical chroma squeeze
    //        parameters.push_back(image.nb_meta_channels+1);
    //        parameters.push_back(image.nb_meta_channels+2);
    //        }
    SqueezeParams params;
    // horizontal chroma squeeze
    params.horizontal = true;
    params.in_place = false;
    params.begin_c = image.nb_meta_channels + 1;
    params.num_c = 2;
    parameters->push_back(params);
    params.horizontal = false;
    // vertical chroma squeeze
    parameters->push_back(params);
  }
  SqueezeParams params;
  params.begin_c = image.nb_meta_channels;
  params.num_c = nb_channels;
  params.in_place = true;

  if (!wide) {
    if (h > JXL_MAX_FIRST_PREVIEW_SIZE) {
      params.horizontal = false;
      parameters->push_back(params);
      h = (h + 1) / 2;
      JXL_DEBUG_V(7, "Vertical (%zux%zu), ", w, h);
    }
  }
  while (w > JXL_MAX_FIRST_PREVIEW_SIZE || h > JXL_MAX_FIRST_PREVIEW_SIZE) {
    if (w > JXL_MAX_FIRST_PREVIEW_SIZE) {
      params.horizontal = true;
      parameters->push_back(params);
      w = (w + 1) / 2;
      JXL_DEBUG_V(7, "Horizontal (%zux%zu), ", w, h);
    }
    if (h > JXL_MAX_FIRST_PREVIEW_SIZE) {
      params.horizontal = false;
      parameters->push_back(params);
      h = (h + 1) / 2;
      JXL_DEBUG_V(7, "Vertical (%zux%zu), ", w, h);
    }
  }
  JXL_DEBUG_V(7, "that's it");
}

Status CheckMetaSqueezeParams(const std::vector<SqueezeParams> &parameters,
                              int num_channels) {
  for (size_t i = 0; i < parameters.size(); i++) {
    int c1 = parameters[i].begin_c;
    int c2 = parameters[i].begin_c + parameters[i].num_c - 1;
    if (c1 < 0 || c1 > num_channels || c2 < 0 || c2 >= num_channels ||
        c2 < c1) {
      return JXL_FAILURE("Invalid channel range");
    }
  }
  return true;
}

Status MetaSqueeze(Image &image, std::vector<SqueezeParams> *parameters) {
  if (parameters->empty()) {
    DefaultSqueezeParameters(parameters, image);
  }
  JXL_RETURN_IF_ERROR(
      CheckMetaSqueezeParams(*parameters, image.channel.size()));

  for (size_t i = 0; i < parameters->size(); i++) {
    bool horizontal = (*parameters)[i].horizontal;
    bool in_place = (*parameters)[i].in_place;
    uint32_t beginc = (*parameters)[i].begin_c;
    uint32_t endc = (*parameters)[i].begin_c + (*parameters)[i].num_c - 1;

    uint32_t offset;
    if (in_place) {
      offset = endc + 1;
    } else {
      offset = image.channel.size();
    }
    for (uint32_t c = beginc; c <= endc; c++) {
      Channel dummy;
      dummy.hcshift = image.channel[c].hcshift;
      dummy.vcshift = image.channel[c].vcshift;
      if (image.channel[c].hshift > 30 || image.channel[c].vshift > 30) {
        return JXL_FAILURE("Too many squeezes: shift > 30");
      }
      if (horizontal) {
        size_t w = image.channel[c].w;
        image.channel[c].w = (w + 1) / 2;
        image.channel[c].hshift++;
        image.channel[c].hcshift++;
        dummy.w = w - (w + 1) / 2;
        dummy.h = image.channel[c].h;
      } else {
        size_t h = image.channel[c].h;
        image.channel[c].h = (h + 1) / 2;
        image.channel[c].vshift++;
        image.channel[c].vcshift++;
        dummy.h = h - (h + 1) / 2;
        dummy.w = image.channel[c].w;
      }
      dummy.hshift = image.channel[c].hshift;
      dummy.vshift = image.channel[c].vshift;

      image.channel.insert(image.channel.begin() + offset + (c - beginc),
                           std::move(dummy));
    }
  }
  return true;
}

Status InvSqueeze(Image &input, std::vector<SqueezeParams> parameters,
                  ThreadPool *pool) {
  if (parameters.empty()) {
    DefaultSqueezeParameters(&parameters, input);
  }
  JXL_RETURN_IF_ERROR(CheckMetaSqueezeParams(parameters, input.channel.size()));

  for (int i = parameters.size() - 1; i >= 0; i--) {
    bool horizontal = parameters[i].horizontal;
    bool in_place = parameters[i].in_place;
    uint32_t beginc = parameters[i].begin_c;
    uint32_t endc = parameters[i].begin_c + parameters[i].num_c - 1;
    uint32_t offset;
    if (in_place) {
      offset = endc + 1;
    } else {
      offset = input.channel.size() + beginc - endc - 1;
    }
    for (uint32_t c = beginc; c <= endc; c++) {
      uint32_t rc = offset + c - beginc;
      if ((input.channel[c].w < input.channel[rc].w) ||
          (input.channel[c].h < input.channel[rc].h)) {
        return JXL_FAILURE("Corrupted squeeze transform");
      }
      if (input.channel[rc].is_empty()) {
        input.channel[rc].resize();  // assume all zeroes
      }
      if (horizontal) {
        InvHSqueeze(input, c, rc, pool);
      } else {
        InvVSqueeze(input, c, rc, pool);
      }
    }
    input.channel.erase(input.channel.begin() + offset,
                        input.channel.begin() + offset + (endc - beginc + 1));
  }
  return true;
}

Status FwdSqueeze(Image &input, std::vector<SqueezeParams> parameters,
                  ThreadPool *pool) {
  if (parameters.empty()) {
    DefaultSqueezeParameters(&parameters, input);
  }
  JXL_RETURN_IF_ERROR(CheckMetaSqueezeParams(parameters, input.channel.size()));

  for (size_t i = 0; i < parameters.size(); i++) {
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

#endif  // LIB_JXL_MODULAR_TRANSFORM_SQUEEZE_H_
