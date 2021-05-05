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

#include "lib/jxl/enc_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/compressed_dc.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/dct_util.h"
#include "lib/jxl/dec_frame.h"
#include "lib/jxl/enc_frame.h"
#include "lib/jxl/enc_group.h"
#include "lib/jxl/enc_modular.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/passes_state.h"
#include "lib/jxl/quantizer.h"

namespace jxl {

void InitializePassesEncoder(const Image3F& opsin, ThreadPool* pool,
                             PassesEncoderState* enc_state,
                             ModularFrameEncoder* modular_frame_encoder,
                             AuxOut* aux_out) {
  PROFILER_FUNC;

  PassesSharedState& JXL_RESTRICT shared = enc_state->shared;

  enc_state->histogram_idx.resize(shared.frame_dim.num_groups);

  enc_state->x_qm_multiplier =
      std::pow(1.25f, shared.frame_header.x_qm_scale - 2.0f);
  enc_state->b_qm_multiplier =
      std::pow(1.25f, shared.frame_header.b_qm_scale - 2.0f);

  if (enc_state->coeffs.size() < shared.frame_header.passes.num_passes) {
    enc_state->coeffs.reserve(shared.frame_header.passes.num_passes);
    for (size_t i = enc_state->coeffs.size();
         i < shared.frame_header.passes.num_passes; i++) {
      // Allocate enough coefficients for each group on every row.
      enc_state->coeffs.emplace_back(make_unique<ACImageT<int32_t>>(
          kGroupDim * kGroupDim, shared.frame_dim.num_groups));
    }
  }
  while (enc_state->coeffs.size() > shared.frame_header.passes.num_passes) {
    enc_state->coeffs.pop_back();
  }

  Image3F dc(shared.frame_dim.xsize_blocks, shared.frame_dim.ysize_blocks);
  RunOnPool(
      pool, 0, shared.frame_dim.num_groups, ThreadPool::SkipInit(),
      [&](size_t group_idx, size_t _) {
        ComputeCoefficients(group_idx, enc_state, opsin, &dc);
      },
      "Compute coeffs");

  if (shared.frame_header.flags & FrameHeader::kUseDcFrame) {
    CompressParams cparams = enc_state->cparams;
    // Guess a distance that produces good initial results.
    cparams.butteraugli_distance =
        std::max(kMinButteraugliDistance,
                 enc_state->cparams.butteraugli_distance * 0.1f);
    cparams.dots = Override::kOff;
    cparams.noise = Override::kOff;
    cparams.patches = Override::kOff;
    cparams.gaborish = Override::kOff;
    cparams.epf = 0;
    cparams.max_error_mode = true;
    for (size_t c = 0; c < 3; c++) {
      cparams.max_error[c] = shared.quantizer.MulDC()[c];
    }
    JXL_ASSERT(cparams.progressive_dc > 0);
    cparams.progressive_dc--;
    // The DC frame will have alpha=0. Don't erase its contents.
    cparams.keep_invisible = true;
    // No EPF or Gaborish in DC frames.
    cparams.epf = 0;
    cparams.gaborish = Override::kOff;
    // Use kVarDCT in max_error_mode for intermediate progressive DC,
    // and kModular for the smallest DC (first in the bitstream)
    if (cparams.progressive_dc == 0) {
      cparams.modular_mode = true;
      cparams.quality_pair.first = cparams.quality_pair.second =
          99.f - enc_state->cparams.butteraugli_distance * 0.2f;
    }
    ImageBundle ib(&shared.metadata->m);
    // This is a lie - dc is in XYB
    // (but EncodeFrame will skip RGB->XYB conversion anyway)
    ib.SetFromImage(
        std::move(dc),
        ColorEncoding::LinearSRGB(shared.metadata->m.color_encoding.IsGray()));
    if (!ib.metadata()->extra_channel_info.empty()) {
      // Add dummy extra channels to the patch image: dc_level frames do not yet
      // support extra channels, but the codec expects that the amount of extra
      // channels in frames matches that in the metadata of the codestream.
      std::vector<ImageF> extra_channels;
      extra_channels.reserve(ib.metadata()->extra_channel_info.size());
      for (size_t i = 0; i < ib.metadata()->extra_channel_info.size(); i++) {
        const auto& eci = ib.metadata()->extra_channel_info[i];
        extra_channels.emplace_back(eci.Size(ib.xsize()), eci.Size(ib.ysize()));
        // Must initialize the image with data to not affect blending with
        // uninitialized memory.
        // TODO(lode): dc_level must copy and use the real extra channels
        // instead.
        ZeroFillImage(&extra_channels.back());
      }
      ib.SetExtraChannels(std::move(extra_channels));
    }
    std::unique_ptr<PassesEncoderState> state =
        jxl::make_unique<PassesEncoderState>();

    auto special_frame = std::unique_ptr<BitWriter>(new BitWriter());
    FrameInfo dc_frame_info;
    dc_frame_info.frame_type = FrameType::kDCFrame;
    dc_frame_info.dc_level = shared.frame_header.dc_level + 1;
    dc_frame_info.ib_needs_color_transform = false;
    dc_frame_info.save_before_color_transform = true;  // Implicitly true
    // TODO(lode): the EncodeFrame / DecodeFrame pair here is likely broken in
    // case of dc_level >= 3, since EncodeFrame may output multiple frames
    // to the bitwriter, while DecodeFrame reads only one.
    JXL_CHECK(EncodeFrame(cparams, dc_frame_info, shared.metadata, ib,
                          state.get(), pool, special_frame.get(), nullptr));
    const Span<const uint8_t> encoded = special_frame->GetSpan();
    enc_state->special_frames.emplace_back(std::move(special_frame));

    BitReader br(encoded);
    ImageBundle decoded(&shared.metadata->m);
    std::unique_ptr<PassesDecoderState> dec_state =
        jxl::make_unique<PassesDecoderState>();
    JXL_CHECK(dec_state->output_encoding_info.Set(shared.metadata->m));
    JXL_CHECK(DecodeFrame({}, dec_state.get(), pool, &br, &decoded,
                          *shared.metadata, /*constraints=*/nullptr));
    // TODO(lode): shared.frame_header.dc_level should be equal to
    // dec_state.shared->frame_header.dc_level - 1 here, since above we set
    // dc_frame_info.dc_level = shared.frame_header.dc_level + 1, and
    // dc_frame_info.dc_level is used by EncodeFrame. However, if EncodeFrame
    // outputs multiple frames, this assumption could be wrong.
    shared.dc_storage =
        CopyImage(dec_state->shared->dc_frames[shared.frame_header.dc_level]);
    ZeroFillImage(&shared.quant_dc);
    shared.dc = &shared.dc_storage;
    JXL_CHECK(br.Close());
  } else {
    auto compute_dc_coeffs = [&](int group_index, int /* thread */) {
      modular_frame_encoder->AddVarDCTDC(
          dc, group_index,
          enc_state->cparams.butteraugli_distance >= 2.0f &&
              enc_state->cparams.speed_tier != SpeedTier::kFalcon,
          enc_state);
    };
    RunOnPool(pool, 0, shared.frame_dim.num_dc_groups, ThreadPool::SkipInit(),
              compute_dc_coeffs, "Compute DC coeffs");
    // TODO(veluca): this is only useful in tests and if inspection is enabled.
    if (!(shared.frame_header.flags & FrameHeader::kSkipAdaptiveDCSmoothing)) {
      AdaptiveDCSmoothing(shared.quantizer.MulDC(), &shared.dc_storage, pool);
    }
  }
  auto compute_ac_meta = [&](int group_index, int /* thread */) {
    modular_frame_encoder->AddACMetadata(group_index, /*jpeg_transcode=*/false,
                                         enc_state);
  };
  RunOnPool(pool, 0, shared.frame_dim.num_dc_groups, ThreadPool::SkipInit(),
            compute_ac_meta, "Compute AC Metadata");

  if (aux_out != nullptr) {
    aux_out->InspectImage3F("compressed_image:InitializeFrameEncCache:dc_dec",
                            shared.dc_storage);
  }
}

void EncCache::InitOnce() {
  PROFILER_FUNC;

  if (num_nzeroes.xsize() == 0) {
    num_nzeroes = Image3I(kGroupDimInBlocks, kGroupDimInBlocks);
  }
}

}  // namespace jxl
