// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
#include "lib/jxl/base/printf_macros.h"
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
#include "lib/jxl/epf.h"
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
#include "lib/jxl/sanitizers.h"
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
      dparams.allow_partial_files && dparams.allow_more_progressive_steps,
      true));

  // Handling of progressive decoding.
  const FrameHeader& frame_header = frame_decoder.GetFrameHeader();
  {
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
  frame_decoder.SetRenderSpotcolors(dparams.render_spotcolors);
  frame_decoder.SetCoalescing(dparams.coalescing);

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
      if (pos + (dparams.allow_more_progressive_steps &&
                         (i == 0 ||
                          frame_header.encoding == FrameEncoding::kModular)
                     ? b
                     : e) <=
              reader->TotalBytes() ||
          (i == 0 && dparams.allow_more_progressive_steps)) {
        auto br = make_unique<BitReader>(Span<const uint8_t>(
            reader->FirstByte() + b + pos,
            (pos + b > reader->TotalBytes()
                 ? 0
                 : std::min(reader->TotalBytes() - pos - b, e - b))));
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
      return JXL_FAILURE("Invalid section %" PRIuS " status: %d",
                         section_info[i].id, s);
    }
  }

  JXL_RETURN_IF_ERROR(close_ok);

  JXL_RETURN_IF_ERROR(frame_decoder.FinalizeFrame());
  decoded->SetDecodedBytes(processed_bytes);
  return true;
}

Status FrameDecoder::InitFrame(BitReader* JXL_RESTRICT br, ImageBundle* decoded,
                               bool is_preview, bool allow_partial_frames,
                               bool allow_partial_dc_global,
                               bool output_needed) {
  PROFILER_FUNC;
  decoded_ = decoded;
  JXL_ASSERT(is_finalized_);

  allow_partial_frames_ = allow_partial_frames;
  allow_partial_dc_global_ = allow_partial_dc_global;

  // Reset the dequantization matrices to their default values.
  dec_state_->shared_storage.matrices = DequantMatrices();

  frame_header_.nonserialized_is_preview = is_preview;
  size_t pos = br->TotalBitsConsumed() / kBitsPerByte;
  Status have_frameheader =
      br->TotalBytes() > pos && DecodeFrameHeader(br, &frame_header_);
  JXL_RETURN_IF_ERROR(have_frameheader || allow_partial_frames);
  if (!have_frameheader) {
    if (dec_state_->shared_storage.dc_frames[0].xsize() > 0) {
      // If we have a (partial) DC frame available, but we don't have the next
      // frame header (so allow_partial_frames is true), then we'll assume the
      // next frame uses that DC frame (which may not be true, e.g. there might
      // first be a ReferenceOnly patch frame, but it's reasonable to assume
      // that the DC frame is a good progressive preview)
      frame_header_.flags |= FrameHeader::kUseDcFrame;
      frame_header_.encoding = FrameEncoding::kVarDCT;
      frame_header_.dc_level = 0;
    } else
      return JXL_FAILURE("Couldn't read frame header");
  }
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
                                       &section_sizes_, &groups_total_size) ||
                      allow_partial_frames);

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
    return JXL_FAILURE(
        "Non-444 chroma subsampling is not allowed when adaptive DC "
        "smoothing is enabled");
  }

  if (!output_needed) return true;
  JXL_RETURN_IF_ERROR(
      InitializePassesSharedState(frame_header_, &dec_state_->shared_storage));
  JXL_RETURN_IF_ERROR(dec_state_->Init());
  modular_frame_decoder_.Init(frame_dim_);

  if (decoded->IsJPEG()) {
    if (frame_header_.encoding == FrameEncoding::kModular) {
      return JXL_FAILURE("Cannot output JPEG from Modular");
    }
    jpeg::JPEGData* jpeg_data = decoded->jpeg_data.get();
    size_t num_components = jpeg_data->components.size();
    if (num_components != 1 && num_components != 3) {
      return JXL_FAILURE("Invalid number of components");
    }
    if (frame_header_.nonserialized_metadata->m.xyb_encoded) {
      return JXL_FAILURE("Cannot decode to JPEG an XYB image");
    }
    auto jpeg_c_map = JpegOrder(ColorTransform::kYCbCr, num_components == 1);
    decoded->jpeg_data->width = frame_dim_.xsize;
    decoded->jpeg_data->height = frame_dim_.ysize;
    for (size_t c = 0; c < num_components; c++) {
      auto& component = jpeg_data->components[jpeg_c_map[c]];
      component.width_in_blocks =
          frame_dim_.xsize_blocks >> frame_header_.chroma_subsampling.HShift(c);
      component.height_in_blocks =
          frame_dim_.ysize_blocks >> frame_header_.chroma_subsampling.VShift(c);
      component.h_samp_factor =
          1 << frame_header_.chroma_subsampling.RawHShift(c);
      component.v_samp_factor =
          1 << frame_header_.chroma_subsampling.RawVShift(c);
      component.coeffs.resize(component.width_in_blocks *
                              component.height_in_blocks * jxl::kDCTBlockSize);
    }
  }

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
  allocated_ = false;
  return true;
}

Status FrameDecoder::ProcessDCGlobal(BitReader* br) {
  PROFILER_FUNC;
  PassesSharedState& shared = dec_state_->shared_storage;
  if (shared.frame_header.flags & FrameHeader::kPatches) {
    bool uses_extra_channels = false;
    JXL_RETURN_IF_ERROR(shared.image_features.patches.Decode(
        br, frame_dim_.xsize_padded, frame_dim_.ysize_padded,
        &uses_extra_channels));
    if (uses_extra_channels && frame_header_.upsampling != 1) {
      for (size_t ecups : frame_header_.extra_channel_upsampling) {
        if (ecups != frame_header_.upsampling) {
          return JXL_FAILURE(
              "Cannot use extra channels in patches if color channels are "
              "subsampled differently from extra channels");
        }
      }
    }
  } else {
    shared.image_features.patches.Clear();
  }
  shared.image_features.splines.Clear();
  if (shared.frame_header.flags & FrameHeader::kSplines) {
    JXL_RETURN_IF_ERROR(shared.image_features.splines.Decode(
        br, frame_dim_.xsize * frame_dim_.ysize));
  }
  if (shared.frame_header.flags & FrameHeader::kNoise) {
    JXL_RETURN_IF_ERROR(DecodeNoise(br, &shared.image_features.noise_params));
  }
  if (!allow_partial_dc_global_ ||
      br->TotalBitsConsumed() < br->TotalBytes() * kBitsPerByte) {
    JXL_RETURN_IF_ERROR(dec_state_->shared_storage.matrices.DecodeDC(br));

    if (frame_header_.encoding == FrameEncoding::kVarDCT) {
      JXL_RETURN_IF_ERROR(
          jxl::DecodeGlobalDCInfo(br, decoded_->IsJPEG(), dec_state_, pool_));
    }
  }
  // Splines' draw cache uses the color correlation map.
  if (shared.frame_header.flags & FrameHeader::kSplines) {
    JXL_RETURN_IF_ERROR(shared.image_features.splines.InitializeDrawCache(
        frame_dim_.xsize_upsampled_padded, frame_dim_.ysize_upsampled_padded,
        dec_state_->shared->cmap));
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
  const LoopFilter& lf = dec_state_->shared->frame_header.loop_filter;
  if (frame_header_.encoding == FrameEncoding::kVarDCT &&
      !(frame_header_.flags & FrameHeader::kUseDcFrame)) {
    JXL_RETURN_IF_ERROR(
        modular_frame_decoder_.DecodeVarDCTDC(dc_group_id, br, dec_state_));
  }
  const Rect mrect(gx * frame_dim_.dc_group_dim, gy * frame_dim_.dc_group_dim,
                   frame_dim_.dc_group_dim, frame_dim_.dc_group_dim);
  JXL_RETURN_IF_ERROR(modular_frame_decoder_.DecodeGroup(
      mrect, br, 3, 1000, ModularStreamId::ModularDC(dc_group_id),
      /*zerofill=*/false, nullptr, nullptr, allow_partial_frames_));
  if (frame_header_.encoding == FrameEncoding::kVarDCT) {
    JXL_RETURN_IF_ERROR(
        modular_frame_decoder_.DecodeAcMetadata(dc_group_id, br, dec_state_));
  } else if (lf.epf_iters > 0) {
    FillImage(kInvSigmaNum / lf.epf_sigma_for_modular,
              &dec_state_->filter_weights.sigma);
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
  if (allocated_) return;
  const CodecMetadata& metadata = *frame_header_.nonserialized_metadata;
  if (dec_state_->rgb_output == nullptr && !dec_state_->pixel_callback) {
    modular_frame_decoder_.MaybeDropFullImage();
    decoded_->SetFromImage(Image3F(frame_dim_.xsize_upsampled_padded,
                                   frame_dim_.ysize_upsampled_padded),
                           dec_state_->output_encoding_info.color_encoding);
  }
  dec_state_->extra_channels.clear();
  if (metadata.m.num_extra_channels > 0) {
    for (size_t i = 0; i < metadata.m.num_extra_channels; i++) {
      uint32_t ecups = frame_header_.extra_channel_upsampling[i];
      dec_state_->extra_channels.emplace_back(
          DivCeil(frame_dim_.xsize_upsampled_padded, ecups),
          DivCeil(frame_dim_.ysize_upsampled_padded, ecups));
#if JXL_MEMORY_SANITIZER
      // Avoid errors due to loading vectors on the outermost padding.
      // Upsample of extra channels requires this padding to be initialized.
      // TODO(deymo): Remove this and use rects up to {x,y}size_upsampled
      // instead of the padded one.
      for (size_t y = 0; y < DivCeil(frame_dim_.ysize_upsampled_padded, ecups);
           y++) {
        for (size_t x = (y < DivCeil(frame_dim_.ysize_upsampled, ecups)
                             ? DivCeil(frame_dim_.xsize_upsampled, ecups)
                             : 0);
             x < DivCeil(frame_dim_.xsize_upsampled_padded, ecups); x++) {
          dec_state_->extra_channels.back().Row(y)[x] =
              msan::kSanitizerSentinel;
        }
      }
#endif
    }
  }
  decoded_->origin = dec_state_->shared->frame_header.frame_origin;
  dec_state_->InitForAC(nullptr);
  allocated_ = true;
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
    jpeg::JPEGData* jpeg_data = decoded_->jpeg_data.get();
    size_t num_components = jpeg_data->components.size();
    bool is_gray = (num_components == 1);
    auto jpeg_c_map = JpegOrder(frame_header_.color_transform, is_gray);
    size_t qt_set = 0;
    for (size_t c = 0; c < num_components; c++) {
      // TODO(eustas): why 1-st quant table for gray?
      size_t quant_c = is_gray ? 1 : c;
      size_t qpos = jpeg_data->components[jpeg_c_map[c]].quant_idx;
      JXL_CHECK(qpos != jpeg_data->quant.size());
      qt_set |= 1 << qpos;
      for (size_t x = 0; x < 8; x++) {
        for (size_t y = 0; y < 8; y++) {
          jpeg_data->quant[qpos].values[x * 8 + y] =
              (*qe[0].qraw.qtable)[quant_c * 64 + y * 8 + x];
        }
      }
    }
    for (size_t i = 0; i < jpeg_data->quant.size(); i++) {
      if (qt_set & (1 << i)) continue;
      if (i == 0) return JXL_FAILURE("First quant table unused.");
      // Unused quant table is set to copy of previous quant table
      for (size_t j = 0; j < 64; j++) {
        jpeg_data->quant[i].values[j] = jpeg_data->quant[i - 1].values[j];
      }
    }
  }
  // Set memory buffer for pre-color-transform frame, if needed.
  if (frame_header_.needs_color_transform() &&
      frame_header_.save_before_color_transform) {
    dec_state_->pre_color_transform_frame =
        Image3F(frame_dim_.xsize_upsampled, frame_dim_.ysize_upsampled);
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
          /*zerofill=*/false, dec_state_, decoded_, allow_partial_frames_));
    } else if (i >= decoded_passes_per_ac_group_[ac_group_id] + num_passes &&
               force_draw) {
      JXL_RETURN_IF_ERROR(modular_frame_decoder_.DecodeGroup(
          mrect, nullptr, minShift, maxShift,
          ModularStreamId::ModularAC(ac_group_id, i), /*zerofill=*/true,
          dec_state_, decoded_, allow_partial_frames_));
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
    JXL_RETURN_IF_ERROR(ProcessACGlobal(sections[ac_global_sec].br));
    section_status[ac_global_sec] = SectionStatus::kDone;
  }

  if (decoded_ac_global_) {
    // The decoded image requires padding for filtering. ProcessACGlobal added
    // the padding, however when Flush is used, the image is shrunk to the
    // output size. Add the padding back here. This is a cheap operation
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
  AllocateOutput();

  uint32_t completely_decoded_ac_pass = *std::min_element(
      decoded_passes_per_ac_group_.begin(), decoded_passes_per_ac_group_.end());
  if (completely_decoded_ac_pass < frame_header_.passes.num_passes) {
    // We don't have all AC yet: force a draw of all the missing areas.
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
  JXL_RETURN_IF_ERROR(modular_frame_decoder_.FinalizeDecoding(
      dec_state_, pool_, decoded_, is_finalized_));
  JXL_RETURN_IF_ERROR(FinalizeFrameDecoding(decoded_, dec_state_, pool_,
                                            /*force_fir=*/false,
                                            /*skip_blending=*/!coalescing_,
                                            /*move_ec=*/is_finalized_));

  num_renders_++;
  return true;
}

int FrameDecoder::SavedAs(const FrameHeader& header) {
  if (header.frame_type == FrameType::kDCFrame) {
    // bits 16, 32, 64, 128 for DC level
    return 16 << (header.dc_level - 1);
  } else if (header.CanBeReferenced()) {
    // bits 1, 2, 4 and 8 for the references
    return 1 << header.save_as_reference;
  }

  return 0;
}

int FrameDecoder::References() const {
  if (is_finalized_) {
    return 0;
  }
  if ((!decoded_dc_global_ || !decoded_ac_global_ ||
       *std::min_element(decoded_dc_groups_.begin(),
                         decoded_dc_groups_.end()) != 1 ||
       *std::min_element(decoded_passes_per_ac_group_.begin(),
                         decoded_passes_per_ac_group_.end()) < max_passes_)) {
    return 0;
  }

  int result = 0;

  // Blending
  if (frame_header_.frame_type == FrameType::kRegularFrame ||
      frame_header_.frame_type == FrameType::kSkipProgressive) {
    bool cropped = frame_header_.custom_size_or_origin;
    if (cropped || frame_header_.blending_info.mode != BlendMode::kReplace) {
      result |= (1 << frame_header_.blending_info.source);
    }
    const auto& extra = frame_header_.extra_channel_blending_info;
    for (size_t i = 0; i < extra.size(); ++i) {
      if (cropped || extra[i].mode != BlendMode::kReplace) {
        result |= (1 << extra[i].source);
      }
    }
  }

  // Patches
  if (frame_header_.flags & FrameHeader::kPatches) {
    result |= dec_state_->shared->image_features.patches.GetReferences();
  }

  // DC Level
  if (frame_header_.flags & FrameHeader::kUseDcFrame) {
    // Reads from the next dc level
    int dc_level = frame_header_.dc_level + 1;
    // bits 16, 32, 64, 128 for DC level
    result |= (16 << (dc_level - 1));
  }

  return result;
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

  if (!finalized_dc_) {
    JXL_ASSERT(allow_partial_frames_);
    AllocateOutput();
  }

  JXL_RETURN_IF_ERROR(Flush());

  if (dec_state_->shared->frame_header.CanBeReferenced() &&
      (frame_header_.frame_type != kRegularFrame || coalescing_)) {
    size_t id = dec_state_->shared->frame_header.save_as_reference;
    auto& reference_frame = dec_state_->shared_storage.reference_frames[id];
    if (dec_state_->pre_color_transform_frame.xsize() == 0) {
      reference_frame.storage = decoded_->Copy();
    } else {
      reference_frame.storage = ImageBundle(decoded_->metadata());
      reference_frame.storage.SetFromImage(
          CopyImage(dec_state_->pre_color_transform_frame),
          decoded_->c_current());
      if (decoded_->HasExtraChannels()) {
        const std::vector<ImageF>* ecs = &dec_state_->pre_color_transform_ec;
        if (ecs->empty()) ecs = &decoded_->extra_channels();
        std::vector<ImageF> extra_channels;
        for (const auto& ec : *ecs) {
          extra_channels.push_back(CopyImage(ec));
        }
        reference_frame.storage.SetExtraChannels(std::move(extra_channels));
      }
    }
    reference_frame.frame = &reference_frame.storage;
    reference_frame.ib_is_in_xyb =
        dec_state_->shared->frame_header.save_before_color_transform;
    if (!dec_state_->shared->frame_header.save_before_color_transform) {
      const CodecMetadata* metadata =
          dec_state_->shared->frame_header.nonserialized_metadata;
      if (reference_frame.frame->xsize() < metadata->xsize() ||
          reference_frame.frame->ysize() < metadata->ysize()) {
        return JXL_FAILURE(
            "trying to save a reference frame that is too small: %" PRIuS
            "x%" PRIuS
            " "
            "instead of %" PRIuS "x%" PRIuS,
            reference_frame.frame->xsize(), reference_frame.frame->ysize(),
            metadata->xsize(), metadata->ysize());
      }
      reference_frame.storage.ShrinkTo(metadata->xsize(), metadata->ysize());
    }
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
    if (coalescing_ && (frame_header_.frame_type == kRegularFrame ||
                        frame_header_.frame_type == kSkipProgressive)) {
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

  if (render_spotcolors_) {
    for (size_t i = 0; i < decoded_->extra_channels().size(); i++) {
      // Don't use Find() because there may be multiple spot color channels.
      const ExtraChannelInfo& eci = decoded_->metadata()->extra_channel_info[i];
      if (eci.type == ExtraChannel::kOptional) {
        continue;
      }
      if (eci.type == ExtraChannel::kUnknown ||
          (int(ExtraChannel::kReserved0) <= int(eci.type) &&
           int(eci.type) <= int(ExtraChannel::kReserved7))) {
        return JXL_FAILURE(
            "Unknown extra channel (bits %u, shift %u, name '%s')\n",
            eci.bit_depth.bits_per_sample, eci.dim_shift, eci.name.c_str());
      }
      if (eci.type == ExtraChannel::kSpotColor) {
        float scale = eci.spot_color[3];
        for (size_t c = 0; c < 3; c++) {
          for (size_t y = 0; y < decoded_->ysize(); y++) {
            float* JXL_RESTRICT p = decoded_->color()->Plane(c).Row(y);
            const float* JXL_RESTRICT s =
                decoded_->extra_channels()[i].ConstRow(y);
            for (size_t x = 0; x < decoded_->xsize(); x++) {
              float mix = scale * s[x];
              p[x] = mix * eci.spot_color[c] + (1.0 - mix) * p[x];
            }
          }
        }
      }
    }
  }
  if (dec_state_->shared->frame_header.dc_level != 0) {
    dec_state_->shared_storage
        .dc_frames[dec_state_->shared->frame_header.dc_level - 1] =
        std::move(*decoded_->color());
    decoded_->RemoveColor();
  }
  return true;
}

}  // namespace jxl
