// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/modular/transform/squeeze.h"

#include <stdlib.h>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/transform.h"

namespace jxl {

void InvHSqueeze(Image &input, uint32_t c, uint32_t rc, ThreadPool *pool) {
  JXL_ASSERT(c < input.channel.size());
  JXL_ASSERT(rc < input.channel.size());
  const Channel &chin = input.channel[c];
  const Channel &chin_residual = input.channel[rc];
  // These must be valid since we ran MetaApply already.
  JXL_ASSERT(chin.w == DivCeil(chin.w + chin_residual.w, 2));
  JXL_ASSERT(chin.h == chin_residual.h);

  if (chin_residual.w == 0) {
    // Short-circuit: output channel has same dimensions as input.
    input.channel[c].hshift--;
    return;
  }

  // Note: chin.w >= chin_residual.w and at most 1 different.
  Channel chout(chin.w + chin_residual.w, chin.h, chin.hshift - 1, chin.vshift);
  JXL_DEBUG_V(4,
              "Undoing horizontal squeeze of channel %i using residuals in "
              "channel %i (going from width %zu to %zu)",
              c, rc, chin.w, chout.w);

  if (chin_residual.h == 0) {
    // Short-circuit: channel with no pixels.
    input.channel[c] = std::move(chout);
    return;
  }

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
        p_out[0] = A;
        p_out[1] = B;

        for (size_t x = 1; x < chin_residual.w; x++) {
          pixel_type_w diff_minus_tendency = p_residual[x];
          pixel_type_w avg = p_avg[x];
          pixel_type_w next_avg = (x + 1 < chin.w ? p_avg[x + 1] : avg);
          pixel_type_w left = p_out[(x << 1) - 1];
          pixel_type_w tendency = SmoothTendency(left, avg, next_avg);
          pixel_type_w diff = diff_minus_tendency + tendency;
          pixel_type_w A =
              ((avg * 2) + diff + (diff > 0 ? -(diff & 1) : (diff & 1))) >> 1;
          p_out[x << 1] = A;
          pixel_type_w B = A - diff;
          p_out[(x << 1) + 1] = B;
        }
        if (chout.w & 1) p_out[chout.w - 1] = p_avg[chin.w - 1];
      },
      "InvHorizontalSqueeze");
  input.channel[c] = std::move(chout);
}

void InvVSqueeze(Image &input, uint32_t c, uint32_t rc, ThreadPool *pool) {
  JXL_ASSERT(c < input.channel.size());
  JXL_ASSERT(rc < input.channel.size());
  const Channel &chin = input.channel[c];
  const Channel &chin_residual = input.channel[rc];
  // These must be valid since we ran MetaApply already.
  JXL_ASSERT(chin.h == DivCeil(chin.h + chin_residual.h, 2));
  JXL_ASSERT(chin.w == chin_residual.w);

  if (chin_residual.h == 0) {
    // Short-circuit: output channel has same dimensions as input.
    input.channel[c].vshift--;
    return;
  }

  // Note: chin.h >= chin_residual.h and at most 1 different.
  Channel chout(chin.w, chin.h + chin_residual.h, chin.hshift, chin.vshift - 1);
  JXL_DEBUG_V(
      4,
      "Undoing vertical squeeze of channel %i using residuals in channel "
      "%i (going from height %zu to %zu)",
      c, rc, chin.h, chout.h);

  if (chin_residual.w == 0) {
    // Short-circuit: channel with no pixels.
    input.channel[c] = std::move(chout);
    return;
  }

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

            p_out[x] = out;
            // If the chin_residual.h == chin.h, the output has an even number
            // of rows so the next line is fine. Otherwise, this loop won't
            // write to the last output row which is handled separately.
            p_out[x + onerow_out] = p_out[x] - diff;
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

void DefaultSqueezeParameters(std::vector<SqueezeParams> *parameters,
                              const Image &image) {
  int nb_channels = image.channel.size() - image.nb_meta_channels;

  parameters->clear();
  size_t w = image.channel[image.nb_meta_channels].w;
  size_t h = image.channel[image.nb_meta_channels].h;
  JXL_DEBUG_V(7, "Default squeeze parameters for %zux%zu image: ", w, h);

  // do horizontal first on wide images; vertical first on tall images
  bool wide = (w > h);

  if (nb_channels > 2 && image.channel[image.nb_meta_channels + 1].w == w &&
      image.channel[image.nb_meta_channels + 1].h == h) {
    // assume channels 1 and 2 are chroma, and can be squeezed first for 4:2:0
    // previews
    JXL_DEBUG_V(7, "(4:2:0 chroma), %zux%zu image", w, h);
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

Status CheckMetaSqueezeParams(const SqueezeParams &parameter,
                              int num_channels) {
  int c1 = parameter.begin_c;
  int c2 = parameter.begin_c + parameter.num_c - 1;
  if (c1 < 0 || c1 >= num_channels || c2 < 0 || c2 >= num_channels || c2 < c1) {
    return JXL_FAILURE("Invalid channel range");
  }
  return true;
}

Status MetaSqueeze(Image &image, std::vector<SqueezeParams> *parameters) {
  if (parameters->empty()) {
    DefaultSqueezeParameters(parameters, image);
  }

  for (size_t i = 0; i < parameters->size(); i++) {
    JXL_RETURN_IF_ERROR(
        CheckMetaSqueezeParams((*parameters)[i], image.channel.size()));
    bool horizontal = (*parameters)[i].horizontal;
    bool in_place = (*parameters)[i].in_place;
    uint32_t beginc = (*parameters)[i].begin_c;
    uint32_t endc = (*parameters)[i].begin_c + (*parameters)[i].num_c - 1;

    uint32_t offset;
    if (beginc < image.nb_meta_channels) {
      if (endc >= image.nb_meta_channels) {
        return JXL_FAILURE("Invalid squeeze: mix of meta and nonmeta channels");
      }
      if (!in_place)
        return JXL_FAILURE(
            "Invalid squeeze: meta channels require in-place residuals");
      image.nb_meta_channels += (*parameters)[i].num_c;
    }
    if (in_place) {
      offset = endc + 1;
    } else {
      offset = image.channel.size();
    }
    for (uint32_t c = beginc; c <= endc; c++) {
      if (image.channel[c].hshift > 30 || image.channel[c].vshift > 30) {
        return JXL_FAILURE("Too many squeezes: shift > 30");
      }
      size_t w = image.channel[c].w;
      size_t h = image.channel[c].h;
      if (horizontal) {
        image.channel[c].w = (w + 1) / 2;
        image.channel[c].hshift++;
        w = w - (w + 1) / 2;
      } else {
        image.channel[c].h = (h + 1) / 2;
        image.channel[c].vshift++;
        h = h - (h + 1) / 2;
      }
      image.channel[c].shrink();
      Channel dummy(w, h);
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

  for (int i = parameters.size() - 1; i >= 0; i--) {
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
      offset = input.channel.size() + beginc - endc - 1;
    }
    if (beginc < input.nb_meta_channels) {
      // This is checked in MetaSqueeze.
      JXL_ASSERT(input.nb_meta_channels > parameters[i].num_c);
      input.nb_meta_channels -= parameters[i].num_c;
    }

    for (uint32_t c = beginc; c <= endc; c++) {
      uint32_t rc = offset + c - beginc;
      // MetaApply should imply that `rc` is within range, otherwise there's a
      // programming bug.
      JXL_ASSERT(rc < input.channel.size());
      if ((input.channel[c].w < input.channel[rc].w) ||
          (input.channel[c].h < input.channel[rc].h)) {
        return JXL_FAILURE("Corrupted squeeze transform");
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

}  // namespace jxl
