// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/aux_out.h"

#include <stdint.h>

#include <numeric>  // accumulate

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/printf_macros.h"
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
  if (min_quant_rescale != 1.0 || max_quant_rescale != 1.0) {
    printf("quant rescale range: %f .. %f\n", min_quant_rescale,
           max_quant_rescale);
    printf("bitrate error range: %.3f%% .. %.3f%%\n",
           100.0f * min_bitrate_error, 100.0f * max_bitrate_error);
  }

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
    printf(" Total:\t\t    %7" PRIuS "\t\t     %7" PRIuS " \t\t\t%10f%%\n\n",
           total_blocks, total_positions,
           100.0 * total_blocks / total_positions);
  }
}

void ReclaimAndCharge(BitWriter* JXL_RESTRICT writer,
                      BitWriter::Allotment* JXL_RESTRICT allotment,
                      size_t layer, AuxOut* JXL_RESTRICT aux_out) {
  size_t used_bits, unused_bits;
  allotment->PrivateReclaim(writer, &used_bits, &unused_bits);

#if 0
  printf("Layer %s bits: max %" PRIuS " used %" PRIuS " unused %" PRIuS "\n", LayerName(layer),
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
