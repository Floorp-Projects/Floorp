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

#ifndef LIB_JXL_MODULAR_TRANSFORM_NEAR_LOSSLESS_H_
#define LIB_JXL_MODULAR_TRANSFORM_NEAR_LOSSLESS_H_

// Very simple lossy preprocessing step.
// Quantizes the prediction residual (so the entropy coder has an easier job)
// Obviously there's room for encoder improvement here
// The decoder doesn't need to know about this step

#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/modular_image.h"

namespace jxl {

pixel_type DeltaQuantize(int max_error, pixel_type in, pixel_type prediction) {
  int resolution = 2 << max_error;
  pixel_type d = in - prediction;
  int32_t val = std::abs(d);
  if (val < std::max(1, resolution / 4 * 3 / 4)) return 0;
  if (val < resolution / 2 * 3 / 4) {
    return d > 0 ? resolution / 4 : -resolution / 4;
  }
  if (val < resolution * 3 / 4) return d > 0 ? resolution / 2 : -resolution / 2;
  val = (val + resolution / 2) / resolution * resolution;
  return d > 0 ? val : -val;
}

static Status FwdNearLossless(Image& input, size_t begin_c, size_t end_c,
                              int max_delta_error, Predictor predictor) {
  if (begin_c < input.nb_meta_channels || begin_c > input.channel.size() ||
      end_c < input.nb_meta_channels || end_c >= input.channel.size() ||
      end_c < begin_c) {
    return JXL_FAILURE("Invalid channel range %zu-%zu", begin_c, end_c);
  }

  JXL_DEBUG_V(8, "Applying loss on channels %zu-%zu with max delta=%i.",
              begin_c, end_c, max_delta_error);
  uint64_t total_error = 0;
  for (size_t c = begin_c; c <= end_c; c++) {
    size_t w = input.channel[c].w;
    size_t h = input.channel[c].h;

    Channel out(w, h);
    weighted::Header header;
    weighted::State wp_state(header, w, h);
    for (size_t y = 0; y < h; y++) {
      pixel_type* JXL_RESTRICT p_in = input.channel[c].Row(y);
      pixel_type* JXL_RESTRICT p_out = out.Row(y);
      for (size_t x = 0; x < w; x++) {
        PredictionResult pred = PredictNoTreeWP(
            w, p_out + x, out.plane.PixelsPerRow(), x, y, predictor, &wp_state);
        pixel_type delta = DeltaQuantize(max_delta_error, p_in[x], pred.guess);
        pixel_type reconstructed = pred.guess + delta;
        int e = p_in[x] - reconstructed;
        total_error += abs(e);
        p_out[x] = reconstructed;
        wp_state.UpdateErrors(p_out[x], x, y, w);
      }
    }
    input.channel[c] = std::move(out);
    // fprintf(stderr, "Avg error: %f\n", total_error * 1.0 / (w * h));
    JXL_DEBUG_V(9, "  Avg error: %f", total_error * 1.0 / (w * h));
  }
  return false;  // don't signal this 'transform' in the bitstream, there is no
                 // inverse transform to be done
}

}  // namespace jxl

#endif  // LIB_JXL_MODULAR_TRANSFORM_NEAR_LOSSLESS_H_
