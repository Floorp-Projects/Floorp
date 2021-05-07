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

#include "lib/jxl/dec_frame.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <hwy/aligned_allocator.h>
#include <numeric>
#include <utility>
#include <vector>

#include "lib/jxl/ac_context.h"
#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/coeff_order.h"
#include "lib/jxl/coeff_order_fwd.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/compressed_dc.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/dec_group.h"
#include "lib/jxl/dec_modular.h"
#include "lib/jxl/dec_params.h"
#include "lib/jxl/dec_patch_dictionary.h"
#include "lib/jxl/dec_reconstruct.h"
#include "lib/jxl/dec_upsample.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/filters.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/jpeg/jpeg_data.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/luminance.h"
#include "lib/jxl/passes_state.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer.h"
#include "lib/jxl/splines.h"
#include "lib/jxl/toc.h"

namespace jxl {

namespace {
Status DecodeGlobalDCInfo(BitReader* reader, bool is_jpeg,
                          PassesDecoderState* state, ThreadPool* pool) {
  PROFILER_FUNC;
  JXL_RETURN_IF_ERROR(state->shared_storage.quantizer.Decode(reader));

  JXL_RETURN_IF_ERROR(
      DecodeBlockCtxMap(reader, &state->shared_storage.block_ctx_map));

  JXL_RETURN_IF_ERROR(state->shared_storage.cmap.DecodeDC(reader));

  // Pre-compute info for decoding a group.
  if (is_jpeg) {
    state->shared_storage.quantizer.ClearDCMul();  // Don't dequant DC
  }

  state->shared_storage.ac_strategy.FillInvalid();
  return true;
}
}  // namespace

Status DecodeFrameHeader(BitReader* JXL_RESTRICT reader,
                         FrameHeader* JXL_RESTRICT frame_header) {
  JXL_ASSERT(frame_header->nonserialized_metadata != nullptr);
  JXL_RETURN_IF_ERROR(ReadFrameHeader(reader, frame_header));

  if (frame_header->encoding == FrameEncoding::kModular) {
    if (frame_header->chroma_subsampling.MaxHShift() != 0 ||
        frame_header->chroma_subsampling.MaxVShift() != 0) {
      return JXL_FAILURE("Chroma subsampling in modular mode is not supported");
    }
  }

  return true;
}

Status SkipFrame(const CodecMetadata& metadata, BitReader* JXL_RESTRICT reader,
                 bool is_preview) {
  FrameHeader header(&metadata);
  header.nonserialized_is_preview = is_preview;
  JXL_RETURN_IF_ERROR(DecodeFrameHeader(reader, &header));

  // Read TOC.
  std::vector<uint64_t> group_offsets;
  std::vector<uint32_t> group_sizes;
  uint64_t groups_total_size;
  const bool has_ac_global = true;
  const FrameDimensions frame_dim = header.ToFrameDimensions();
  const size_t toc_entries =
      NumTocEntries(frame_dim.num_groups, frame_dim.num_dc_groups,
                    header.passes.num_passes, has_ac_global);
  JXL_RETURN_IF_ERROR(ReadGroupOffsets(toc_entries, reader, &group_offsets,
                                       &group_sizes, &groups_total_size));

  // Pretend all groups are read.
  reader->SkipBits(groups_total_size * kBitsPerByte);
  if (reader->TotalBitsConsumed() > reader->TotalBytes() * kBitsPerByte) {
    return JXL_FAILURE("Group code extends after stream end");
  }

  return true;
}

static BitReader* GetReaderForSection(
    size_t num_groups, size_t num_passes, size_t group_codes_begin,
    const std::vector<uint64_t>& group_offsets,
    const std::vector<uint32_t>& group_sizes, BitReader* JXL_RESTRICT reader,
    BitReader* JXL_RESTRICT store, size_t index) {
  if (num_groups == 1 && num_passes == 1) return reader;
  const size_t group_offset = group_codes_begin + group_offsets[index];
  const size_t next_group_offset =
      group_codes_begin + group_offsets[index] + group_sizes[index];
  // The order of these variables must be:
  // group_codes_begin <= group_offset <= next_group_offset <= file.size()
  JXL_DASSERT(group_codes_begin <= group_offset);
  JXL_DASSERT(group_offset <= next_group_offset);
  JXL_DASSERT(next_group_offset <= reader->TotalBytes());
  const size_t group_size = next_group_offset - group_offset;
  const size_t remaining_size = reader->TotalBytes() - group_offset;
  const size_t size = std::min(group_size + 8, remaining_size);
  *store =
      BitReader(Span<const uint8_t>(reader->FirstByte() + group_offset, size));
  return store;
}

Status DecodeFrame(const DecompressParams& dparams,
                   PassesDecoderState* dec_state, ThreadPool* JXL_RESTRICT pool,
                   BitReader* JXL_RESTRICT reader, ImageBundle* decoded,
                   const CodecMetadata& metadata,
                   const SizeConstraints* constraints, bool is_preview) {
  PROFILER_ZONE("DecodeFrame uninstrumented");

  FrameDecoder frame_decoder(dec_state, metadata, pool);

  frame_decoder.SetFrameSizeLimits(constraints);

  JXL_RETURN_IF_ERROR(frame_decoder.InitFrame(
      reader, decoded, is_preview, dparams.allow_partial_files,
      dparams.allow_partial_files && dparams.allow_more_progressive_steps));

  // Handling of progressive decoding.
  {
    const FrameHeader& frame_header = frame_decoder.GetFrameHeader();
    size_t max_passes = dparams.max_passes;
    size_t max_downsampling = std::max(
        dparams.max_downsampling >> (frame_header.dc_level * 3), size_t(1));
    // TODO(veluca): deal with downsamplings >= 8.
    if (max_downsampling >= 8) {
      max_passes = 0;
    } else {
      for (uint32_t i = 0; i < frame_header.passes.num_downsample; ++i) {
        if (max_downsampling >= frame_header.passes.downsample[i] &&
            max_passes > frame_header.passes.last_pass[i]) {
          max_passes = frame_header.passes.last_pass[i] + 1;
        }
      }
    }
    // Do not use downsampling for kReferenceOnly frames.
    if (frame_header.frame_type == FrameType::kReferenceOnly) {
      max_passes = frame_header.passes.num_passes;
    }
    max_passes = std::min<size_t>(max_passes, frame_header.passes.num_passes);
    frame_decoder.SetMaxPasses(max_passes);
  }

  size_t processed_bytes = reader->TotalBitsConsumed() / kBitsPerByte;

  Status close_ok = true;
  std::vector<std::unique_ptr<BitReader>> section_readers;
  {
    std::vector<std::unique_ptr<BitReaderScopedCloser>> section_closers;
    std::vector<FrameDecoder::SectionInfo> section_info;
    std::vector<FrameDecoder::SectionStatus> section_status;
    size_t bytes_to_skip = 0;
    for (size_t i = 0; i < frame_decoder.NumSections(); i++) {
      size_t b = frame_decoder.SectionOffsets()[i];
      size_t e = b + frame_decoder.SectionSizes()[i];
      bytes_to_skip += e - b;
      size_t pos = reader->TotalBitsConsumed() / kBitsPerByte;
      if (pos + e <= reader->TotalBytes()) {
        auto br = make_unique<BitReader>(
            Span<const uint8_t>(reader->FirstByte() + b + pos, e - b));
        section_info.emplace_back(FrameDecoder::SectionInfo{br.get(), i});
        section_closers.emplace_back(
            make_unique<BitReaderScopedCloser>(br.get(), &close_ok));
        section_readers.emplace_back(std::move(br));
      } else if (!dparams.allow_partial_files) {
        return JXL_FAILURE("Premature end of stream.");
      }
    }
    // Skip over the to-be-decoded sections.
    reader->SkipBits(kBitsPerByte * bytes_to_skip);
    section_status.resize(section_info.size());

    JXL_RETURN_IF_ERROR(frame_decoder.ProcessSections(
        section_info.data(), section_info.size(), section_status.data()));

    for (size_t i = 0; i < section_status.size(); i++) {
      auto s = section_status[i];
      if (s == FrameDecoder::kDone) {
        processed_bytes += frame_decoder.SectionSizes()[i];
        continue;
      }
      if (dparams.allow_more_progressive_steps && s == FrameDecoder::kPartial) {
        continue;
      }
      if (dparams.max_downsampling > 1 && s == FrameDecoder::kSkipped) {
        continue;
      }
      return JXL_FAILURE("Invalid section %zu status: %d", section_info[i].id,
                         s);
    }
  }

  JXL_RETURN_IF_ERROR(close_ok);

  JXL_RETURN_IF_ERROR(frame_decoder.FinalizeFrame());
  decoded->SetDecodedBytes(processed_bytes);
  return true;
}

Status FrameDecoder::InitFrame(BitReader* JXL_RESTRICT br, ImageBundle* decoded,
                               bool is_preview, bool allow_partial_frames,
                               bool allow_partial_dc_global) {
  PROFILER_FUNC;
  decoded_ = decoded;
  JXL_ASSERT(is_finalized_);

  allow_partial_frames_ = allow_partial_frames;
  allow_partial_dc_global_ = allow_partial_dc_global;

  // Reset the dequantization matrices to their default values.
  dec_state_->shared_storage.matrices = DequantMatrices();

  frame_header_.nonserialized_is_preview = is_preview;
  JXL_RETURN_IF_ERROR(DecodeFrameHeader(br, &frame_header_));
  frame_dim_ = frame_header_.ToFrameDimensions();

  const size_t num_passes = frame_header_.passes.num_passes;
  const size_t xsize = frame_dim_.xsize;
  const size_t ysize = frame_dim_.ysize;
  const size_t num_groups = frame_dim_.num_groups;

  // Check validity of frame dimensions.
  JXL_RETURN_IF_ERROR(VerifyDimensions(constraints_, xsize, ysize));

  // If the previous frame was not a kRegularFrame, `decoded` may have different
  // dimensions; must reset to avoid errors.
  decoded->RemoveColor();
  decoded->ClearExtraChannels();

  // Read TOC.
  uint64_t groups_total_size;
  const bool has_ac_global = true;
  const size_t toc_entries = NumTocEntries(num_groups, frame_dim_.num_dc_groups,
                                           num_passes, has_ac_global);
  JXL_RETURN_IF_ERROR(ReadGroupOffsets(toc_entries, br, &section_offsets_,
                                       &section_sizes_, &groups_total_size));

  JXL_DASSERT((br->TotalBitsConsumed() % kBitsPerByte) == 0);
  const size_t group_codes_begin = br->TotalBitsConsumed() / kBitsPerByte;
  JXL_DASSERT(!section_offsets_.empty());

  // Overflow check.
  if (group_codes_begin + groups_total_size < group_codes_begin) {
    return JXL_FAILURE("Invalid group codes");
  }

  if (!frame_header_.chroma_subsampling.Is444() &&
      !(frame_header_.flags & FrameHeader::kSkipAdaptiveDCSmoothing) &&
      frame_header_.encoding == FrameEncoding::kVarDCT) {
    // TODO(veluca): actually implement this.
    return JXL_FAILURE(
        "Non-444 chroma subsampling is not supported when adaptive DC "
        "smoothing is enabled");
  }
  JXL_RETURN_IF_ERROR(
      InitializePassesSharedState(frame_header_, &dec_state_->shared_storage));
  dec_state_->Init();
  modular_frame_decoder_.Init(frame_dim_);

  if (decoded->IsJPEG()) {
    if (frame_header_.encoding == FrameEncoding::kModular) {
      return JXL_FAILURE("Cannot output JPEG from Modular");
    }
    jpeg::JPEGData* jpeg_data = decoded->jpeg_data.get();
    if (jpeg_data->components.size() != 1 &&
        jpeg_data->components.size() != 3) {
      return JXL_FAILURE("Invalid number of components");
    }
    decoded->jpeg_data->width = frame_dim_.xsize;
    decoded->jpeg_data->height = frame_dim_.ysize;
    if (jpeg_data->components.size() == 1) {
      jpeg_data->components[0].width_in_blocks = frame_dim_.xsize_blocks;
      jpeg_data->components[0].height_in_blocks = frame_dim_.ysize_blocks;
    } else {
      for (size_t c = 0; c < 3; c++) {
        jpeg_data->components[c < 2 ? c ^ 1 : c].width_in_blocks =
            frame_dim_.xsize_blocks >>
            frame_header_.chroma_subsampling.HShift(c);
        jpeg_data->components[c < 2 ? c ^ 1 : c].height_in_blocks =
            frame_dim_.ysize_blocks >>
            frame_header_.chroma_subsampling.VShift(c);
      }
    }
    for (size_t c = 0; c < jpeg_data->components.size(); c++) {
      jpeg_data->components[c].h_samp_factor =
          1 << frame_header_.chroma_subsampling.RawHShift(c < 2 ? c ^ 1 : c);
      jpeg_data->components[c].v_samp_factor =
          1 << frame_header_.chroma_subsampling.RawVShift(c < 2 ? c ^ 1 : c);
    }
    for (auto& v : jpeg_data->components) {
      v.coeffs.resize(v.width_in_blocks * v.height_in_blocks *
                      jxl::kDCTBlockSize);
    }
  }

  dec_state_->upsampler.Init(
      frame_header_.upsampling,
      frame_header_.nonserialized_metadata->m.transform_data);

  // Clear the state.
  decoded_dc_global_ = false;
  decoded_ac_global_ = false;
  is_finalized_ = false;
  finalized_dc_ = false;
  decoded_dc_groups_.clear();
  decoded_dc_groups_.resize(frame_dim_.num_dc_groups);
  decoded_passes_per_ac_group_.clear();
  decoded_passes_per_ac_group_.resize(frame_dim_.num_groups, 0);
  processed_section_.clear();
  processed_section_.resize(section_offsets_.size());
  max_passes_ = frame_header_.passes.num_passes;
  num_renders_ = 0;

  return true;
}

Status FrameDecoder::ProcessDCGlobal(BitReader* br) {
  PROFILER_FUNC;
  PassesSharedState& shared = dec_state_->shared_storage;
  if (shared.frame_header.flags & FrameHeader::kPatches) {
    JXL_RETURN_IF_ERROR(shared.image_features.patches.Decode(
        br, frame_dim_.xsize_padded, frame_dim_.ysize_padded));
  }
  if (shared.frame_header.flags & FrameHeader::kSplines) {
    JXL_RETURN_IF_ERROR(shared.image_features.splines.Decode(
        br, frame_dim_.xsize * frame_dim_.ysize));
  }
  if (shared.frame_header.flags & FrameHeader::kNoise) {
    JXL_RETURN_IF_ERROR(DecodeNoise(br, &shared.image_features.noise_params));
  }

  JXL_RETURN_IF_ERROR(dec_state_->shared_storage.matrices.DecodeDC(br));
  if (frame_header_.encoding == FrameEncoding::kVarDCT) {
    JXL_RETURN_IF_ERROR(
        jxl::DecodeGlobalDCInfo(br, decoded_->IsJPEG(), dec_state_, pool_));
  }
  Status dec_status = modular_frame_decoder_.DecodeGlobalInfo(
      br, frame_header_, allow_partial_dc_global_);
  if (dec_status.IsFatalError()) return dec_status;
  if (dec_status) {
    decoded_dc_global_ = true;
  }
  return dec_status;
}

Status FrameDecoder::ProcessDCGroup(size_t dc_group_id, BitReader* br) {
  PROFILER_FUNC;
  const size_t gx = dc_group_id % frame_dim_.xsize_dc_groups;
  const size_t gy = dc_group_id / frame_dim_.xsize_dc_groups;
  if (frame_header_.encoding == FrameEncoding::kVarDCT &&
      !(frame_header_.flags & FrameHeader::kUseDcFrame)) {
    JXL_RETURN_IF_ERROR(
        modular_frame_decoder_.DecodeVarDCTDC(dc_group_id, br, dec_state_));
  }
  const Rect mrect(gx * kDcGroupDim, gy * kDcGroupDim, kDcGroupDim,
                   kDcGroupDim);
  JXL_RETURN_IF_ERROR(modular_frame_decoder_.DecodeGroup(
      mrect, br, 3, 1000, ModularStreamId::ModularDC(dc_group_id),
      /*zerofill=*/false));
  if (frame_header_.encoding == FrameEncoding::kVarDCT) {
    JXL_RETURN_IF_ERROR(
        modular_frame_decoder_.DecodeAcMetadata(dc_group_id, br, dec_state_));
  }
  decoded_dc_groups_[dc_group_id] = true;
  return true;
}

void FrameDecoder::FinalizeDC() {
  // Do Adaptive DC smoothing if enabled. This *must* happen between all the
  // ProcessDCGroup and ProcessACGroup.
  if (frame_header_.encoding == FrameEncoding::kVarDCT &&
      !(frame_header_.flags & FrameHeader::kSkipAdaptiveDCSmoothing) &&
      !(frame_header_.flags & FrameHeader::kUseDcFrame)) {
    AdaptiveDCSmoothing(dec_state_->shared->quantizer.MulDC(),
                        &dec_state_->shared_storage.dc_storage, pool_);
  }

  finalized_dc_ = true;
}

void FrameDecoder::AllocateOutput() {
  const CodecMetadata& metadata = *frame_header_.nonserialized_metadata;
  if (dec_state_->rgb_output == nullptr && !dec_state_->pixel_callback) {
    decoded_->SetFromImage(Image3F(frame_dim_.xsize_upsampled_padded,
                                   frame_dim_.ysize_upsampled_padded),
                           dec_state_->output_encoding_info.color_encoding);
  }
  dec_state_->extra_channels.clear();
  if (metadata.m.num_extra_channels > 0) {
    for (size_t i = 0; i < metadata.m.num_extra_channels; i++) {
      const auto eci = metadata.m.extra_channel_info[i];
      dec_state_->extra_channels.emplace_back(
          eci.Size(frame_dim_.xsize_upsampled_padded),
          eci.Size(frame_dim_.ysize_upsampled_padded));
#if MEMORY_SANITIZER
      // Avoid errors due to loading vectors on the outermost padding.
      for (size_t y = 0; y < eci.Size(frame_dim_.ysize_upsampled_padded); y++) {
        for (size_t x = eci.Size(frame_dim_.xsize_upsampled);
             x < eci.Size(frame_dim_.xsize_upsampled_padded); x++) {
          dec_state_->extra_channels.back().Row(y)[x] = 0;
        }
      }
#endif
    }
  }
  decoded_->origin = dec_state_->shared->frame_header.frame_origin;
}

Status FrameDecoder::ProcessACGlobal(BitReader* br) {
  JXL_CHECK(finalized_dc_);
  JXL_CHECK(decoded_->HasColor() || dec_state_->rgb_output != nullptr ||
            !!dec_state_->pixel_callback);

  // Decode AC group.
  if (frame_header_.encoding == FrameEncoding::kVarDCT) {
    JXL_RETURN_IF_ERROR(dec_state_->shared_storage.matrices.Decode(
        br, &modular_frame_decoder_));

    size_t num_histo_bits =
        CeilLog2Nonzero(dec_state_->shared->frame_dim.num_groups);
    dec_state_->shared_storage.num_histograms =
        1 + br->ReadBits(num_histo_bits);

    dec_state_->code.resize(kMaxNumPasses);
    dec_state_->context_map.resize(kMaxNumPasses);
    // Read coefficient orders and histograms.
    size_t max_num_bits_ac = 0;
    for (size_t i = 0;
         i < dec_state_->shared_storage.frame_header.passes.num_passes; i++) {
      uint16_t used_orders = U32Coder::Read(kOrderEnc, br);
      JXL_RETURN_IF_ERROR(DecodeCoeffOrders(
          used_orders, dec_state_->used_acs,
          &dec_state_->shared_storage
               .coeff_orders[i * dec_state_->shared_storage.coeff_order_size],
          br));
      size_t num_contexts =
          dec_state_->shared->num_histograms *
          dec_state_->shared_storage.block_ctx_map.NumACContexts();
      JXL_RETURN_IF_ERROR(DecodeHistograms(
          br, num_contexts, &dec_state_->code[i], &dec_state_->context_map[i]));
      // Add extra values to enable the cheat in hot loop of DecodeACVarBlock.
      dec_state_->context_map[i].resize(
          num_contexts + kZeroDensityContextLimit - kZeroDensityContextCount);
      max_num_bits_ac =
          std::max(max_num_bits_ac, dec_state_->code[i].max_num_bits);
    }
    max_num_bits_ac += CeilLog2Nonzero(
        dec_state_->shared_storage.frame_header.passes.num_passes);
    // 16-bit buffer for decoding to JPEG are not implemented.
    // TODO(veluca): figure out the exact limit - 16 should still work with
    // 16-bit buffers, but we are excluding it for safety.
    bool use_16_bit = max_num_bits_ac < 16 && !decoded_->IsJPEG();
    bool store = frame_header_.passes.num_passes > 1;
    size_t xs = store ? kGroupDim * kGroupDim : 0;
    size_t ys = store ? frame_dim_.num_groups : 0;
    if (use_16_bit) {
      dec_state_->coefficients = make_unique<ACImageT<int16_t>>(xs, ys);
    } else {
      dec_state_->coefficients = make_unique<ACImageT<int32_t>>(xs, ys);
    }
    if (store) {
      dec_state_->coefficients->ZeroFill();
    }
  }

  // Set JPEG decoding data.
  if (decoded_->IsJPEG()) {
    decoded_->color_transform = frame_header_.color_transform;
    decoded_->chroma_subsampling = frame_header_.chroma_subsampling;
    const std::vector<QuantEncoding>& qe =
        dec_state_->shared_storage.matrices.encodings();
    if (qe.empty() || qe[0].mode != QuantEncoding::Mode::kQuantModeRAW ||
        std::abs(qe[0].qraw.qtable_den - 1.f / (8 * 255)) > 1e-8f) {
      return JXL_FAILURE(
          "Quantization table is not a JPEG quantization table.");
    }
    auto jpeg_c_map = JpegOrder(frame_header_.color_transform,
                                decoded_->jpeg_data->components.size() == 1);
    for (size_t c = 0; c < 3; c++) {
      if (c != 1 && decoded_->jpeg_data->components.size() == 1) {
        continue;
      }
      size_t jpeg_channel = jpeg_c_map[c];
      size_t qpos = decoded_->jpeg_data->components[jpeg_channel].quant_idx;
      JXL_CHECK(qpos != decoded_->jpeg_data->quant.size());
      for (size_t x = 0; x < 8; x++) {
        for (size_t y = 0; y < 8; y++) {
          decoded_->jpeg_data->quant[qpos].values[x * 8 + y] =
              (*qe[0].qraw.qtable)[c * 64 + y * 8 + x];
        }
      }
    }
  }
  // Set memory buffer for pre-color-transform frame, if needed.
  if (frame_header_.needs_color_transform() &&
      frame_header_.save_before_color_transform) {
    dec_state_->pre_color_transform_frame = Image3F(
        frame_dim_.xsize_upsampled_padded, frame_dim_.ysize_upsampled_padded);
  } else {
    // clear pre_color_transform_frame to ensure that previously moved-from
    // images are not used.
    dec_state_->pre_color_transform_frame = Image3F();
  }
  decoded_ac_global_ = true;
  return true;
}

Status FrameDecoder::ProcessACGroup(size_t ac_group_id,
                                    BitReader* JXL_RESTRICT* br,
                                    size_t num_passes, size_t thread,
                                    bool force_draw, bool dc_only) {
  PROFILER_ZONE("process_group");
  const size_t gx = ac_group_id % frame_dim_.xsize_groups;
  const size_t gy = ac_group_id / frame_dim_.xsize_groups;
  const size_t x = gx * frame_dim_.group_dim;
  const size_t y = gy * frame_dim_.group_dim;

  if (frame_header_.encoding == FrameEncoding::kVarDCT) {
    group_dec_caches_[thread].InitOnce(frame_header_.passes.num_passes,
                                       dec_state_->used_acs);
    JXL_RETURN_IF_ERROR(DecodeGroup(
        br, num_passes, ac_group_id, dec_state_, &group_dec_caches_[thread],
        thread, decoded_, decoded_passes_per_ac_group_[ac_group_id], force_draw,
        dc_only));
  }

  // don't limit to image dimensions here (is done in DecodeGroup)
  const Rect mrect(x, y, frame_dim_.group_dim, frame_dim_.group_dim);
  for (size_t i = 0; i < frame_header_.passes.num_passes; i++) {
    int minShift, maxShift;
    frame_header_.passes.GetDownsamplingBracket(i, minShift, maxShift);
    if (i >= decoded_passes_per_ac_group_[ac_group_id] &&
        i < decoded_passes_per_ac_group_[ac_group_id] + num_passes) {
      JXL_RETURN_IF_ERROR(modular_frame_decoder_.DecodeGroup(
          mrect, br[i - decoded_passes_per_ac_group_[ac_group_id]], minShift,
          maxShift, ModularStreamId::ModularAC(ac_group_id, i),
          /*zerofill=*/false));
    } else if (i >= decoded_passes_per_ac_group_[ac_group_id] + num_passes &&
               force_draw) {
      JXL_RETURN_IF_ERROR(modular_frame_decoder_.DecodeGroup(
          mrect, nullptr, minShift, maxShift,
          ModularStreamId::ModularAC(ac_group_id, i), /*zerofill=*/true));
    }
  }
  decoded_passes_per_ac_group_[ac_group_id] += num_passes;
  return true;
}

Status FrameDecoder::ProcessSections(const SectionInfo* sections, size_t num,
                                     SectionStatus* section_status) {
  if (num == 0) return true;  // Nothing to process
  std::fill(section_status, section_status + num, SectionStatus::kSkipped);
  size_t dc_global_sec = num;
  size_t ac_global_sec = num;
  std::vector<size_t> dc_group_sec(frame_dim_.num_dc_groups, num);
  std::vector<std::vector<size_t>> ac_group_sec(
      frame_dim_.num_groups,
      std::vector<size_t>(frame_header_.passes.num_passes, num));
  std::vector<size_t> num_ac_passes(frame_dim_.num_groups);
  if (frame_dim_.num_groups == 1 && frame_header_.passes.num_passes == 1) {
    JXL_ASSERT(num == 1);
    JXL_ASSERT(sections[0].id == 0);
    if (processed_section_[0] == false) {
      processed_section_[0] = true;
      ac_group_sec[0].resize(1);
      dc_global_sec = ac_global_sec = dc_group_sec[0] = ac_group_sec[0][0] = 0;
      num_ac_passes[0] = 1;
    } else {
      section_status[0] = SectionStatus::kDuplicate;
    }
  } else {
    size_t ac_global_index = frame_dim_.num_dc_groups + 1;
    for (size_t i = 0; i < num; i++) {
      JXL_ASSERT(sections[i].id < processed_section_.size());
      if (processed_section_[sections[i].id]) {
        section_status[i] = SectionStatus::kDuplicate;
        continue;
      }
      if (sections[i].id == 0) {
        dc_global_sec = i;
      } else if (sections[i].id < ac_global_index) {
        dc_group_sec[sections[i].id - 1] = i;
      } else if (sections[i].id == ac_global_index) {
        ac_global_sec = i;
      } else {
        size_t ac_idx = sections[i].id - ac_global_index - 1;
        size_t acg = ac_idx % frame_dim_.num_groups;
        size_t acp = ac_idx / frame_dim_.num_groups;
        if (acp >= frame_header_.passes.num_passes) {
          return JXL_FAILURE("Invalid section ID");
        }
        if (acp >= max_passes_) {
          continue;
        }
        ac_group_sec[acg][acp] = i;
      }
      processed_section_[sections[i].id] = true;
    }
    // Count number of new passes per group.
    for (size_t g = 0; g < ac_group_sec.size(); g++) {
      size_t j = 0;
      for (; j + decoded_passes_per_ac_group_[g] < max_passes_; j++) {
        if (ac_group_sec[g][j + decoded_passes_per_ac_group_[g]] == num) {
          break;
        }
      }
      num_ac_passes[g] = j;
    }
  }
  if (dc_global_sec != num) {
    Status dc_global_status = ProcessDCGlobal(sections[dc_global_sec].br);
    if (dc_global_status.IsFatalError()) return dc_global_status;
    if (dc_global_status) {
      section_status[dc_global_sec] = SectionStatus::kDone;
    } else {
      section_status[dc_global_sec] = SectionStatus::kPartial;
    }
  }

  std::atomic<bool> has_error{false};
  if (decoded_dc_global_) {
    RunOnPool(
        pool_, 0, dc_group_sec.size(), ThreadPool::SkipInit(),
        [this, &dc_group_sec, &num, &sections, &section_status, &has_error](
            size_t i, size_t thread) {
          if (dc_group_sec[i] != num) {
            if (!ProcessDCGroup(i, sections[dc_group_sec[i]].br)) {
              has_error = true;
            } else {
              section_status[dc_group_sec[i]] = SectionStatus::kDone;
            }
          }
        },
        "DecodeDCGroup");
  }
  if (has_error) return JXL_FAILURE("Error in DC group");

  if (*std::min_element(decoded_dc_groups_.begin(), decoded_dc_groups_.end()) ==
          true &&
      !finalized_dc_) {
    FinalizeDC();
    AllocateOutput();
  }

  if (finalized_dc_) dec_state_->EnsureBordersStorage();
  if (finalized_dc_ && ac_global_sec != num && !decoded_ac_global_) {
    dec_state_->InitForAC(pool_);
    JXL_RETURN_IF_ERROR(ProcessACGlobal(sections[ac_global_sec].br));
    section_status[ac_global_sec] = SectionStatus::kDone;
  }

  if (decoded_ac_global_) {
    // The decoded image requires padding for filtering. ProcessACGlobal added
    // the padding, however when Flush is used, the image is shrunk to the
    // output size. Add the padding back here. This is a cheap opeartion
    // since the image has the original allocated size. The memory and original
    // size are already there, but for safety we require the indicated xsize and
    // ysize dimensions match the working area, see PlaneRowBoundsCheck.
    decoded_->ShrinkTo(frame_dim_.xsize_upsampled_padded,
                       frame_dim_.ysize_upsampled_padded);

    // Mark all the AC groups that we received as not complete yet.
    for (size_t i = 0; i < ac_group_sec.size(); i++) {
      if (num_ac_passes[i] == 0) continue;
      dec_state_->group_border_assigner.ClearDone(i);
    }

    RunOnPool(
        pool_, 0, ac_group_sec.size(),
        [this](size_t num_threads) {
          PrepareStorage(num_threads, decoded_passes_per_ac_group_.size());
          return true;
        },
        [this, &ac_group_sec, &num_ac_passes, &num, &sections, &section_status,
         &has_error](size_t g, size_t thread) {
          if (num_ac_passes[g] == 0) {  // no new AC pass, nothing to do.
            return;
          }
          (void)num;
          size_t first_pass = decoded_passes_per_ac_group_[g];
          BitReader* JXL_RESTRICT readers[kMaxNumPasses];
          for (size_t i = 0; i < num_ac_passes[g]; i++) {
            JXL_ASSERT(ac_group_sec[g][first_pass + i] != num);
            readers[i] = sections[ac_group_sec[g][first_pass + i]].br;
          }
          if (!ProcessACGroup(g, readers, num_ac_passes[g],
                              GetStorageLocation(thread, g),
                              /*force_draw=*/false, /*dc_only=*/false)) {
            has_error = true;
          } else {
            for (size_t i = 0; i < num_ac_passes[g]; i++) {
              section_status[ac_group_sec[g][first_pass + i]] =
                  SectionStatus::kDone;
            }
          }
        },
        "DecodeGroup");
  }
  if (has_error) return JXL_FAILURE("Error in AC group");

  for (size_t i = 0; i < num; i++) {
    if (section_status[i] == SectionStatus::kSkipped ||
        section_status[i] == SectionStatus::kPartial) {
      processed_section_[sections[i].id] = false;
    }
  }
  return true;
}

Status FrameDecoder::Flush() {
  bool has_blending = frame_header_.blending_info.mode != BlendMode::kReplace ||
                      frame_header_.custom_size_or_origin;
  for (const auto& blending_info_ec :
       frame_header_.extra_channel_blending_info) {
    if (blending_info_ec.mode != BlendMode::kReplace) has_blending = true;
  }
  // No early Flush() if blending is enabled.
  if (has_blending && !is_finalized_) {
    return false;
  }
  // No early Flush() - nothing to do - if the frame is a kSkipProgressive
  // frame.
  if (frame_header_.frame_type == FrameType::kSkipProgressive &&
      !is_finalized_) {
    return true;
  }
  if (decoded_->IsJPEG()) {
    // Nothing to do.
    return true;
  }
  uint32_t completely_decoded_ac_pass = *std::min_element(
      decoded_passes_per_ac_group_.begin(), decoded_passes_per_ac_group_.end());
  if (completely_decoded_ac_pass < frame_header_.passes.num_passes) {
    // We don't have all AC yet: force a draw of all the missing areas.
    dec_state_->dc_upsampler.Init(
        /*upsampling=*/8, frame_header_.nonserialized_metadata->transform_data);
    // Mark all sections as not complete.
    for (size_t i = 0; i < decoded_passes_per_ac_group_.size(); i++) {
      if (decoded_passes_per_ac_group_[i] == frame_header_.passes.num_passes)
        continue;
      dec_state_->group_border_assigner.ClearDone(i);
    }
    std::atomic<bool> has_error{false};
    RunOnPool(
        pool_, 0, decoded_passes_per_ac_group_.size(),
        [this](size_t num_threads) {
          PrepareStorage(num_threads, decoded_passes_per_ac_group_.size());
          return true;
        },
        [this, &has_error](size_t g, size_t thread) {
          if (decoded_passes_per_ac_group_[g] ==
              frame_header_.passes.num_passes) {
            // This group was drawn already, nothing to do.
            return;
          }
          BitReader* JXL_RESTRICT readers[kMaxNumPasses] = {};
          bool ok = ProcessACGroup(
              g, readers, /*num_passes=*/0, GetStorageLocation(thread, g),
              /*force_draw=*/true, /*dc_only=*/!decoded_ac_global_);
          if (!ok) has_error = true;
        },
        "ForceDrawGroup");
    if (has_error) {
      return JXL_FAILURE("Drawing groups failed");
    }
  }
  // TODO(veluca): the rest of this function should be removed once we have full
  // support for per-group decoding.

  // undo global modular transforms and copy int pixel buffers to float ones
  JXL_RETURN_IF_ERROR(
      modular_frame_decoder_.FinalizeDecoding(dec_state_, pool_, decoded_));

  JXL_RETURN_IF_ERROR(FinalizeFrameDecoding(decoded_, dec_state_, pool_,
                                            /*force_fir=*/false,
                                            /*skip_blending=*/false));

  num_renders_++;
  return true;
}

Status FrameDecoder::FinalizeFrame() {
  if (is_finalized_) {
    return JXL_FAILURE("FinalizeFrame called multiple times");
  }
  is_finalized_ = true;
  if (decoded_->IsJPEG()) {
    // Nothing to do.
    return true;
  }
  if (!finalized_dc_) {
    // We don't have all of DC: EPF might not behave correctly (and is not
    // particularly useful anyway on upsampling results), so we disable it.
    dec_state_->shared_storage.frame_header.loop_filter.epf_iters = 0;
  }
  if ((!decoded_dc_global_ || !decoded_ac_global_ ||
       *std::min_element(decoded_dc_groups_.begin(),
                         decoded_dc_groups_.end()) != 1 ||
       *std::min_element(decoded_passes_per_ac_group_.begin(),
                         decoded_passes_per_ac_group_.end()) < max_passes_) &&
      !allow_partial_frames_) {
    return JXL_FAILURE(
        "FinalizeFrame called before the frame was fully decoded");
  }

  JXL_RETURN_IF_ERROR(Flush());

  if (dec_state_->shared->frame_header.CanBeReferenced()) {
    size_t id = dec_state_->shared->frame_header.save_as_reference;
    if (dec_state_->pre_color_transform_frame.xsize() == 0) {
      dec_state_->shared_storage.reference_frames[id].storage =
          decoded_->Copy();
    } else {
      dec_state_->shared_storage.reference_frames[id].storage =
          ImageBundle(decoded_->metadata());
      dec_state_->shared_storage.reference_frames[id].storage.SetFromImage(
          std::move(dec_state_->pre_color_transform_frame),
          decoded_->c_current());
      if (decoded_->HasExtraChannels()) {
        const std::vector<ImageF>* ecs = &dec_state_->pre_color_transform_ec;
        if (ecs->empty()) ecs = &decoded_->extra_channels();
        std::vector<ImageF> extra_channels;
        for (const auto& ec : *ecs) {
          extra_channels.push_back(CopyImage(ec));
        }
        dec_state_->shared_storage.reference_frames[id]
            .storage.SetExtraChannels(std::move(extra_channels));
      }
    }
    dec_state_->shared_storage.reference_frames[id].frame =
        &dec_state_->shared_storage.reference_frames[id].storage;
    dec_state_->shared_storage.reference_frames[id].ib_is_in_xyb =
        dec_state_->shared->frame_header.save_before_color_transform;
  }
  if (dec_state_->shared->frame_header.dc_level != 0) {
    dec_state_->shared_storage
        .dc_frames[dec_state_->shared->frame_header.dc_level - 1] =
        std::move(*decoded_->color());
    decoded_->RemoveColor();
  }
  if (frame_header_.nonserialized_is_preview) {
    // Fix possible larger image size (multiple of kBlockDim)
    // TODO(lode): verify if and when that happens.
    decoded_->ShrinkTo(frame_dim_.xsize, frame_dim_.ysize);
  } else if (!decoded_->IsJPEG()) {
    // A kRegularFrame is blended with the other frames, and thus results in a
    // coalesced frame of size equal to image dimensions. Other frames are not
    // blended, thus their final size is the size that was defined in the
    // frame_header.
    if (frame_header_.frame_type == kRegularFrame ||
        frame_header_.frame_type == kSkipProgressive) {
      decoded_->ShrinkTo(
          dec_state_->shared->frame_header.nonserialized_metadata->xsize(),
          dec_state_->shared->frame_header.nonserialized_metadata->ysize());
    } else {
      // xsize_upsampled is the actual frame size, after any upsampling has been
      // applied.
      decoded_->ShrinkTo(frame_dim_.xsize_upsampled,
                         frame_dim_.ysize_upsampled);
    }
  }

  return true;
}

}  // namespace jxl
