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

#include "lib/jxl/aux_out.h"

#include <stdint.h>

#include <numeric>  // accumulate

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/enc_bit_writer.h"

namespace jxl {

void AuxOut::Print(size_t num_inputs) const {
  if (num_inputs == 0) return;

  LayerTotals all_layers;
  for (size_t i = 0; i < layers.size(); ++i) {
    all_layers.Assimilate(layers[i]);
  }

  printf("Average butteraugli iters: %10.2f\n",
         num_butteraugli_iters * 1.0 / num_inputs);

  for (size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].total_bits != 0) {
      printf("Total layer bits %-10s\t", LayerName(i));
      printf("%10f%%", 100.0 * layers[i].total_bits / all_layers.total_bits);
      layers[i].Print(num_inputs);
    }
  }
  printf("Total image size           ");
  all_layers.Print(num_inputs);

  const uint32_t dc_pred_total =
      std::accumulate(dc_pred_usage.begin(), dc_pred_usage.end(), 0u);
  const uint32_t dc_pred_total_xb =
      std::accumulate(dc_pred_usage_xb.begin(), dc_pred_usage_xb.end(), 0u);
  if (dc_pred_total + dc_pred_total_xb != 0) {
    printf("\nDC pred     Y                XB:\n");
    for (size_t i = 0; i < dc_pred_usage.size(); ++i) {
      printf("  %6u (%5.2f%%)    %6u (%5.2f%%)\n", dc_pred_usage[i],
             100.0 * dc_pred_usage[i] / dc_pred_total, dc_pred_usage_xb[i],
             100.0 * dc_pred_usage_xb[i] / dc_pred_total_xb);
    }
  }

  size_t total_blocks = 0;
  size_t total_positions = 0;
  if (total_blocks != 0 && total_positions != 0) {
    printf("\n\t\t  Blocks\t\tPositions\t\t\tBlocks/Position\n");
    printf(" Total:\t\t    %7zu\t\t     %7zu \t\t\t%10f%%\n\n", total_blocks,
           total_positions, 100.0 * total_blocks / total_positions);
  }
}

void AuxOut::DumpCoeffImage(const char* label,
                            const Image3S& coeff_image) const {
  JXL_ASSERT(coeff_image.xsize() % 64 == 0);
  Image3S reshuffled(coeff_image.xsize() / 8, coeff_image.ysize() * 8);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < coeff_image.ysize(); y++) {
      for (size_t x = 0; x < coeff_image.xsize(); x += 64) {
        for (size_t i = 0; i < 64; i++) {
          reshuffled.PlaneRow(c, 8 * y + i / 8)[x / 8 + i % 8] =
              coeff_image.PlaneRow(c, y)[x + i];
        }
      }
    }
  }
  DumpImage(label, reshuffled);
}

void ReclaimAndCharge(BitWriter* JXL_RESTRICT writer,
                      BitWriter::Allotment* JXL_RESTRICT allotment,
                      size_t layer, AuxOut* JXL_RESTRICT aux_out) {
  size_t used_bits, unused_bits;
  allotment->PrivateReclaim(writer, &used_bits, &unused_bits);

#if 0
  printf("Layer %s bits: max %zu used %zu unused %zu\n", LayerName(layer),
         allotment->MaxBits(), used_bits, unused_bits);
#endif

  // This may be a nested call with aux_out == null. Whenever we know that
  // aux_out is null, we can call ReclaimUnused directly.
  if (aux_out != nullptr) {
    aux_out->layers[layer].total_bits += used_bits;
    aux_out->layers[layer].histogram_bits += allotment->HistogramBits();
  }
}

}  // namespace jxl
