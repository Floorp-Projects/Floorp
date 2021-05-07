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

#ifndef LIB_JXL_DEC_FRAME_H_
#define LIB_JXL_DEC_FRAME_H_

#include <stdint.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/dec_modular.h"
#include "lib/jxl/dec_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// TODO(veluca): remove DecodeFrameHeader once the API migrates to FrameDecoder.

// `frame_header` must have nonserialized_metadata and
// nonserialized_is_preview set.
Status DecodeFrameHeader(BitReader* JXL_RESTRICT reader,
                         FrameHeader* JXL_RESTRICT frame_header);

// Decodes a frame. Groups may be processed in parallel by `pool`.
// See DecodeFile for explanation of c_decoded.
// `io` is only used for reading maximum image size. Also updates
// `dec_state` with the new frame header.
// `metadata` is the metadata that applies to all frames of the codestream
// `decoded->metadata` must already be set and must match metadata.m.
Status DecodeFrame(const DecompressParams& dparams,
                   PassesDecoderState* dec_state, ThreadPool* JXL_RESTRICT pool,
                   BitReader* JXL_RESTRICT reader, ImageBundle* decoded,
                   const CodecMetadata& metadata,
                   const SizeConstraints* constraints, bool is_preview = false);

// Leaves reader in the same state as DecodeFrame would. Used to skip preview.
// Also updates `dec_state` with the new frame header.
Status SkipFrame(const CodecMetadata& metadata, BitReader* JXL_RESTRICT reader,
                 bool is_preview = false);

// TODO(veluca): implement "forced drawing".
class FrameDecoder {
 public:
  // All parameters must outlive the FrameDecoder.
  FrameDecoder(PassesDecoderState* dec_state, const CodecMetadata& metadata,
               ThreadPool* pool)
      : dec_state_(dec_state), pool_(pool), frame_header_(&metadata) {}

  // `constraints` must outlive the FrameDecoder if not null, or stay alive
  // until the next call to SetFrameSizeLimits.
  void SetFrameSizeLimits(const SizeConstraints* constraints) {
    constraints_ = constraints;
  }

  // Read FrameHeader and table of contents from the given BitReader.
  // Also checks frame dimensions for their limits, and sets the output
  // image buffer.
  // TODO(veluca): remove the `allow_partial_frames` flag - this should be moved
  // on callers.
  Status InitFrame(BitReader* JXL_RESTRICT br, ImageBundle* decoded,
                   bool is_preview, bool allow_partial_frames,
                   bool allow_partial_dc_global);

  struct SectionInfo {
    BitReader* JXL_RESTRICT br;
    size_t id;
  };

  enum SectionStatus {
    // Processed correctly.
    kDone = 0,
    // Skipped because other required sections were not yet processed.
    kSkipped = 1,
    // Skipped because the section was already processed.
    kDuplicate = 2,
    // Only partially decoded: the section will need to be processed again.
    kPartial = 3,
  };

  // Processes `num` sections; each SectionInfo contains the index
  // of the section and a BitReader that only contains the data of the section.
  // `section_status` should point to `num` elements, and will be filled with
  // information about whether each section was processed or not.
  // A section is a part of the encoded file that is indexed by the TOC.
  Status ProcessSections(const SectionInfo* sections, size_t num,
                         SectionStatus* section_status);

  // Flushes all the data decoded so far to pixels.
  Status Flush();

  // Runs final operations once a frame data is decoded.
  // Must be called exactly once per frame, after all calls to ProcessSections.
  Status FinalizeFrame();

  // Returns offset of this section after the end of the TOC. The end of the TOC
  // is the byte position of the bit reader after InitFrame was called.
  const std::vector<uint64_t>& SectionOffsets() const {
    return section_offsets_;
  }
  const std::vector<uint32_t>& SectionSizes() const { return section_sizes_; }
  size_t NumSections() const { return section_sizes_.size(); }

  // TODO(veluca): remove once we remove --downsampling flag.
  void SetMaxPasses(size_t max_passes) { max_passes_ = max_passes; }
  const FrameHeader& GetFrameHeader() const { return frame_header_; }

  // Returns whether a DC image has been decoded, accessible at low resolution
  // at passes.shared_storage.dc_storage
  bool HasDecodedDC() const {
    return frame_header_.encoding == FrameEncoding::kVarDCT && finalized_dc_;
  }

  // If the image has default exif orientation and no
  // blending, the current frame cannot be referenced by future frames, sets the
  // buffer to which uint8 sRGB pixels will be decoded to.
  // TODO(veluca): reduce this set of restrictions.
  // If an output callback is set, this function *must not* be called.
  void MaybeSetRGB8OutputBuffer(uint8_t* rgb_output, size_t stride,
                                bool is_rgba) const {
    if (decoded_->metadata()->GetOrientation() != Orientation::kIdentity) {
      return;
    }
    if (frame_header_.blending_info.mode != BlendMode::kReplace ||
        frame_header_.custom_size_or_origin) {
      return;
    }
    if (frame_header_.CanBeReferenced()) {
      return;
    }
    dec_state_->rgb_output = rgb_output;
    dec_state_->rgb_output_is_rgba = is_rgba;
    dec_state_->rgb_stride = stride;
    JXL_ASSERT(dec_state_->pixel_callback == nullptr);
#if !JXL_HIGH_PRECISION
    if (!is_rgba && decoded_->metadata()->xyb_encoded &&
        dec_state_->output_encoding_info.color_encoding.IsSRGB() &&
        dec_state_->output_encoding_info.all_default_opsin &&
        HasFastXYBTosRGB8() && frame_header_.needs_color_transform()) {
      dec_state_->fast_xyb_srgb8_conversion = true;
    }
#endif
  }

  // Same as MaybeSetRGB8OutputBuffer, but with a float callback.
  // If a RGB8 output buffer is set, this function *must not* be called.
  void MaybeSetFloatCallback(
      const std::function<void(const float* pixels, size_t x, size_t y,
                               size_t num_pixels)>& cb,
      bool is_rgba) const {
    if (decoded_->metadata()->GetOrientation() != Orientation::kIdentity) {
      return;
    }
    if (frame_header_.blending_info.mode != BlendMode::kReplace ||
        frame_header_.custom_size_or_origin) {
      return;
    }
    if (frame_header_.CanBeReferenced()) {
      return;
    }
    dec_state_->pixel_callback = cb;
    dec_state_->rgb_output_is_rgba = is_rgba;
    JXL_ASSERT(dec_state_->rgb_output == nullptr);
  }

  // Returns true if the rgb output buffer passed by MaybeSetRGB8OutputBuffer
  // has been/will be populated by Flush() / FinalizeFrame(), or if a pixel
  // callback has been used.
  bool HasRGBBuffer() const {
    return dec_state_->rgb_output != nullptr ||
           dec_state_->pixel_callback != nullptr;
  }

 private:
  Status ProcessDCGlobal(BitReader* br);
  Status ProcessDCGroup(size_t dc_group_id, BitReader* br);
  void FinalizeDC();
  void AllocateOutput();
  Status ProcessACGlobal(BitReader* br);
  Status ProcessACGroup(size_t ac_group_id, BitReader* JXL_RESTRICT* br,
                        size_t num_passes, size_t thread, bool force_draw,
                        bool dc_only);

  // Allocates storage for parallel decoding using up to `num_threads` threads
  // of up to `num_tasks` tasks. The value of `thread` passed to
  // `GetStorageLocation` must be smaller than the `num_threads` value passed
  // here. The value of `task` passed to `GetStorageLocation` must be smaller
  // than the value of `num_tasks` passed here.
  void PrepareStorage(size_t num_threads, size_t num_tasks) {
    size_t storage_size = std::min(num_threads, num_tasks);
    if (storage_size > group_dec_caches_.size()) {
      group_dec_caches_.resize(storage_size);
    }
    dec_state_->EnsureStorage(storage_size);
    use_task_id_ = num_threads > num_tasks;
  }

  size_t GetStorageLocation(size_t thread, size_t task) {
    if (use_task_id_) return task;
    return thread;
  }

  PassesDecoderState* dec_state_;
  ThreadPool* pool_;
  std::vector<uint64_t> section_offsets_;
  std::vector<uint32_t> section_sizes_;
  size_t max_passes_;
  // TODO(veluca): figure out the duplication between these and dec_state_.
  FrameHeader frame_header_;
  FrameDimensions frame_dim_;
  ImageBundle* decoded_;
  ModularFrameDecoder modular_frame_decoder_;
  bool allow_partial_frames_;
  bool allow_partial_dc_global_;

  std::vector<uint8_t> processed_section_;
  std::vector<uint8_t> decoded_passes_per_ac_group_;
  std::vector<uint8_t> decoded_dc_groups_;
  bool decoded_dc_global_;
  bool decoded_ac_global_;
  bool finalized_dc_ = true;
  bool is_finalized_ = true;
  size_t num_renders_ = 0;

  std::vector<GroupDecCache> group_dec_caches_;

  // Frame size limits.
  const SizeConstraints* constraints_ = nullptr;

  // Whether or not the task id should be used for storage indexing, instead of
  // the thread id.
  bool use_task_id_ = false;
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_FRAME_H_
