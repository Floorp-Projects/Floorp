/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

#include "absl/strings/string_view.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

// Evaluates x and returns false if false.
#define RETURN_IF_FALSE(x) \
  if (!(x)) {              \
    return false;          \
  }

// Evaluates x, which is intended to return an optional. If result is nullopt,
// returns false. Else, calls fun() with the dereferenced optional as parameter.
#define READ_OR_RETURN(x, fun)     \
  do {                             \
    if (auto optional_val = (x)) { \
      fun(*optional_val);          \
    } else {                       \
      return false;                \
    }                              \
  } while (false)

namespace {
const size_t kVp9NumRefsPerFrame = 3;
const size_t kVp9MaxRefLFDeltas = 4;
const size_t kVp9MaxModeLFDeltas = 2;
const size_t kVp9MinTileWidthB64 = 4;
const size_t kVp9MaxTileWidthB64 = 64;

class BitstreamReader {
 public:
  explicit BitstreamReader(rtc::BitBuffer* buffer) : buffer_(buffer) {}

  // Reads on bit from the input stream and:
  // * returns false if bit cannot be read
  // * calls f_true() if bit is true, returns return value of that function
  // * calls f_else() if bit is false, returns return value of that function
  bool IfNextBoolean(
      std::function<bool()> f_true,
      std::function<bool()> f_false = [] { return true; }) {
    uint32_t val;
    if (!buffer_->ReadBits(1, val)) {
      return false;
    }
    if (val != 0) {
      return f_true();
    }
    return f_false();
  }

  absl::optional<bool> ReadBoolean() {
    uint32_t val;
    if (!buffer_->ReadBits(1, val)) {
      return {};
    }
    return {val != 0};
  }

  // Reads a bit from the input stream and returns:
  // * false if bit cannot be read
  // * true if bit matches expected_val
  // * false if bit does not match expected_val - in which case `error_msg` is
  //   logged as warning, if provided.
  bool VerifyNextBooleanIs(bool expected_val, absl::string_view error_msg) {
    uint32_t val;
    if (!buffer_->ReadBits(1, val)) {
      return false;
    }
    if ((val != 0) != expected_val) {
      if (!error_msg.empty()) {
        RTC_LOG(LS_WARNING) << error_msg;
      }
      return false;
    }
    return true;
  }

  // Reads `bits` bits from the bitstream and interprets them as an unsigned
  // integer that gets cast to the type T before returning.
  // Returns nullopt if all bits cannot be read.
  // If number of bits matches size of data type, the bits parameter may be
  // omitted. Ex:
  //  ReadUnsigned<uint8_t>(2);  // Returns uint8_t with 2 LSB populated.
  //  ReadUnsigned<uint8_t>();   // Returns uint8_t with all 8 bits populated.
  template <typename T>
  absl::optional<T> ReadUnsigned(int bits = sizeof(T) * 8) {
    RTC_DCHECK_LE(bits, 32);
    RTC_DCHECK_LE(bits, sizeof(T) * 8);
    uint32_t val;
    if (!buffer_->ReadBits(bits, val)) {
      return {};
    }
    return (static_cast<T>(val));
  }

  // Helper method that reads `num_bits` from the bitstream, returns:
  // * false if bits cannot be read.
  // * true if `expected_val` matches the read bits
  // * false if `expected_val` does not match the read bits, and logs
  //   `error_msg` as a warning (if provided).
  bool VerifyNextUnsignedIs(int num_bits,
                            uint32_t expected_val,
                            absl::string_view error_msg) {
    uint32_t val;
    if (!buffer_->ReadBits(num_bits, val)) {
      return false;
    }
    if (val != expected_val) {
      if (!error_msg.empty()) {
        RTC_LOG(LS_WARNING) << error_msg;
      }
      return false;
    }
    return true;
  }

  // Basically the same as ReadUnsigned() - but for signed integers.
  // Here `bits` indicates the size of the value - number of bits read from the
  // bit buffer is one higher (the sign bit). This is made to matche the spec in
  // which eg s(4) = f(1) sign-bit, plus an f(4).
  template <typename T>
  absl::optional<T> ReadSigned(int bits = sizeof(T) * 8) {
    uint32_t sign;
    if (!buffer_->ReadBits(1, sign)) {
      return {};
    }
    uint32_t val;
    if (!buffer_->ReadBits(bits, val)) {
      return {};
    }
    int64_t sign_val = val;
    if (sign != 0) {
      sign_val = -sign_val;
    }
    return {static_cast<T>(sign_val)};
  }

  // Reads `bits` from the bitstream, disregarding their value.
  // Returns true if full number of bits were read, false otherwise.
  bool ConsumeBits(int bits) { return buffer_->ConsumeBits(bits); }

  void GetPosition(size_t* out_byte_offset, size_t* out_bit_offset) const {
    buffer_->GetCurrentOffset(out_byte_offset, out_bit_offset);
  }

 private:
  rtc::BitBuffer* buffer_;
};

bool Vp9ReadColorConfig(BitstreamReader* br,
                        Vp9UncompressedHeader* frame_info) {
  if (frame_info->profile == 2 || frame_info->profile == 3) {
    READ_OR_RETURN(br->ReadBoolean(), [frame_info](bool ten_or_twelve_bits) {
      frame_info->bit_detph =
          ten_or_twelve_bits ? Vp9BitDept::k12Bit : Vp9BitDept::k10Bit;
    });
  } else {
    frame_info->bit_detph = Vp9BitDept::k8Bit;
  }

  READ_OR_RETURN(
      br->ReadUnsigned<uint8_t>(3), [frame_info](uint8_t color_space) {
        frame_info->color_space = static_cast<Vp9ColorSpace>(color_space);
      });

  if (frame_info->color_space != Vp9ColorSpace::CS_RGB) {
    READ_OR_RETURN(br->ReadBoolean(), [frame_info](bool color_range) {
      frame_info->color_range =
          color_range ? Vp9ColorRange::kFull : Vp9ColorRange::kStudio;
    });

    if (frame_info->profile == 1 || frame_info->profile == 3) {
      READ_OR_RETURN(br->ReadUnsigned<uint8_t>(2),
                     [frame_info](uint8_t subsampling) {
                       switch (subsampling) {
                         case 0b00:
                           frame_info->sub_sampling = Vp9YuvSubsampling::k444;
                           break;
                         case 0b01:
                           frame_info->sub_sampling = Vp9YuvSubsampling::k440;
                           break;
                         case 0b10:
                           frame_info->sub_sampling = Vp9YuvSubsampling::k422;
                           break;
                         case 0b11:
                           frame_info->sub_sampling = Vp9YuvSubsampling::k420;
                           break;
                       }
                     });

      RETURN_IF_FALSE(br->VerifyNextBooleanIs(
          0, "Failed to parse header. Reserved bit set."));
    } else {
      // Profile 0 or 2.
      frame_info->sub_sampling = Vp9YuvSubsampling::k420;
    }
  } else {
    // SRGB
    frame_info->color_range = Vp9ColorRange::kFull;
    if (frame_info->profile == 1 || frame_info->profile == 3) {
      frame_info->sub_sampling = Vp9YuvSubsampling::k444;
      RETURN_IF_FALSE(br->VerifyNextBooleanIs(
          0, "Failed to parse header. Reserved bit set."));
    } else {
      RTC_LOG(LS_WARNING) << "Failed to parse header. 4:4:4 color not supported"
                             " in profile 0 or 2.";
      return false;
    }
  }

  return true;
}

bool ReadRefreshFrameFlags(BitstreamReader* br,
                           Vp9UncompressedHeader* frame_info) {
  // Refresh frame flags.
  READ_OR_RETURN(br->ReadUnsigned<uint8_t>(), [frame_info](uint8_t flags) {
    for (int i = 0; i < 8; ++i) {
      frame_info->updated_buffers.set(i, (flags & (0x01 << (7 - i))) != 0);
    }
  });
  return true;
}

bool Vp9ReadFrameSize(BitstreamReader* br, Vp9UncompressedHeader* frame_info) {
  // 16 bits: frame (width|height) - 1.
  READ_OR_RETURN(br->ReadUnsigned<uint16_t>(), [frame_info](uint16_t width) {
    frame_info->frame_width = width + 1;
  });
  READ_OR_RETURN(br->ReadUnsigned<uint16_t>(), [frame_info](uint16_t height) {
    frame_info->frame_height = height + 1;
  });
  return true;
}

bool Vp9ReadRenderSize(BitstreamReader* br, Vp9UncompressedHeader* frame_info) {
  // render_and_frame_size_different
  return br->IfNextBoolean(
      [&] {
        auto& pos = frame_info->render_size_position.emplace();
        br->GetPosition(&pos.byte_offset, &pos.bit_offset);
        // 16 bits: render (width|height) - 1.
        READ_OR_RETURN(br->ReadUnsigned<uint16_t>(),
                       [frame_info](uint16_t width) {
                         frame_info->render_width = width + 1;
                       });
        READ_OR_RETURN(br->ReadUnsigned<uint16_t>(),
                       [frame_info](uint16_t height) {
                         frame_info->render_height = height + 1;
                       });
        return true;
      },
      /*else*/
      [&] {
        frame_info->render_height = frame_info->frame_height;
        frame_info->render_width = frame_info->frame_width;
        return true;
      });
}

bool Vp9ReadFrameSizeFromRefs(BitstreamReader* br,
                              Vp9UncompressedHeader* frame_info) {
  bool found_ref = false;
  for (size_t i = 0; !found_ref && i < kVp9NumRefsPerFrame; i++) {
    // Size in refs.
    br->IfNextBoolean([&] {
      frame_info->infer_size_from_reference = frame_info->reference_buffers[i];
      found_ref = true;
      return true;
    });
  }

  if (!found_ref) {
    if (!Vp9ReadFrameSize(br, frame_info)) {
      return false;
    }
  }
  return Vp9ReadRenderSize(br, frame_info);
}

bool Vp9ReadLoopfilter(BitstreamReader* br) {
  // 6 bits: filter level.
  // 3 bits: sharpness level.
  RETURN_IF_FALSE(br->ConsumeBits(9));

  return br->IfNextBoolean([&] {    // if mode_ref_delta_enabled
    return br->IfNextBoolean([&] {  // if mode_ref_delta_update
      for (size_t i = 0; i < kVp9MaxRefLFDeltas; i++) {
        RETURN_IF_FALSE(br->IfNextBoolean([&] { return br->ConsumeBits(7); }));
      }
      for (size_t i = 0; i < kVp9MaxModeLFDeltas; i++) {
        RETURN_IF_FALSE(br->IfNextBoolean([&] { return br->ConsumeBits(7); }));
      }
      return true;
    });
  });
}

bool Vp9ReadQp(BitstreamReader* br, Vp9UncompressedHeader* frame_info) {
  READ_OR_RETURN(br->ReadUnsigned<uint8_t>(),
                 [frame_info](uint8_t qp) { frame_info->base_qp = qp; });

  // yuv offsets
  frame_info->is_lossless = frame_info->base_qp == 0;
  for (int i = 0; i < 3; ++i) {
    RETURN_IF_FALSE(br->IfNextBoolean([&] {  // if delta_coded
      READ_OR_RETURN(br->ReadUnsigned<int>(4), [&](int delta) {
        if (delta != 0) {
          frame_info->is_lossless = false;
        }
      });
      return true;
    }));
  }
  return true;
}

bool Vp9ReadSegmentationParams(BitstreamReader* br,
                               Vp9UncompressedHeader* frame_info) {
  constexpr int kSegmentationFeatureBits[kVp9SegLvlMax] = {8, 6, 2, 0};
  constexpr bool kSegmentationFeatureSigned[kVp9SegLvlMax] = {1, 1, 0, 0};

  return br->IfNextBoolean([&] {  // segmentation_enabled
    frame_info->segmentation_enabled = true;
    RETURN_IF_FALSE(br->IfNextBoolean([&] {  // update_map
      frame_info->segmentation_tree_probs.emplace();
      for (int i = 0; i < 7; ++i) {
        RETURN_IF_FALSE(br->IfNextBoolean(
            [&] {
              READ_OR_RETURN(br->ReadUnsigned<uint8_t>(), [&](uint8_t prob) {
                (*frame_info->segmentation_tree_probs)[i] = prob;
              });
              return true;
            },
            [&] {
              (*frame_info->segmentation_tree_probs)[i] = 255;
              return true;
            }));
      }

      // temporal_update
      frame_info->segmentation_pred_prob.emplace();
      return br->IfNextBoolean(
          [&] {
            for (int i = 0; i < 3; ++i) {
              RETURN_IF_FALSE(br->IfNextBoolean(
                  [&] {
                    READ_OR_RETURN(
                        br->ReadUnsigned<uint8_t>(), [&](uint8_t prob) {
                          (*frame_info->segmentation_pred_prob)[i] = prob;
                        });
                    return true;
                  },
                  [&] {
                    (*frame_info->segmentation_pred_prob)[i] = 255;
                    return true;
                  }));
            }
            return true;
          },
          [&] {
            frame_info->segmentation_pred_prob->fill(255);
            return true;
          });
    }));

    return br->IfNextBoolean([&] {  // segmentation_update_data
      RETURN_IF_FALSE(br->IfNextBoolean([&] {
        frame_info->segmentation_is_delta = true;
        return true;
      }));

      for (size_t i = 0; i < kVp9MaxSegments; ++i) {
        for (size_t j = 0; j < kVp9SegLvlMax; ++j) {
          RETURN_IF_FALSE(br->IfNextBoolean([&] {  // feature_enabled
            READ_OR_RETURN(
                br->ReadUnsigned<uint8_t>(kSegmentationFeatureBits[j]),
                [&](uint8_t feature_value) {
                  frame_info->segmentation_features[i][j] = feature_value;
                });
            if (kSegmentationFeatureSigned[j]) {
              RETURN_IF_FALSE(br->IfNextBoolean([&] {
                (*frame_info->segmentation_features[i][j]) *= -1;
                return true;
              }));
            }
            return true;
          }));
        }
      }
      return true;
    });
  });
}

bool Vp9ReadTileInfo(BitstreamReader* br, Vp9UncompressedHeader* frame_info) {
  size_t mi_cols = (frame_info->frame_width + 7) >> 3;
  size_t sb64_cols = (mi_cols + 7) >> 3;

  size_t min_log2 = 0;
  while ((kVp9MaxTileWidthB64 << min_log2) < sb64_cols) {
    ++min_log2;
  }

  size_t max_log2 = 1;
  while ((sb64_cols >> max_log2) >= kVp9MinTileWidthB64) {
    ++max_log2;
  }
  --max_log2;

  frame_info->tile_cols_log2 = min_log2;
  bool done = false;
  while (!done && frame_info->tile_cols_log2 < max_log2) {
    RETURN_IF_FALSE(br->IfNextBoolean(
        [&] {
          ++frame_info->tile_cols_log2;
          return true;
        },
        [&] {
          done = true;
          return true;
        }));
  }
  frame_info->tile_rows_log2 = 0;
  RETURN_IF_FALSE(br->IfNextBoolean([&] {
    ++frame_info->tile_rows_log2;
    return br->IfNextBoolean([&] {
      ++frame_info->tile_rows_log2;
      return true;
    });
  }));
  return true;
}

const Vp9InterpolationFilter kLiteralToType[4] = {
    Vp9InterpolationFilter::kEightTapSmooth, Vp9InterpolationFilter::kEightTap,
    Vp9InterpolationFilter::kEightTapSharp, Vp9InterpolationFilter::kBilinear};
}  // namespace

std::string Vp9UncompressedHeader::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder oss(buf);

  oss << "Vp9UncompressedHeader { "
      << "profile = " << profile;

  if (show_existing_frame) {
    oss << ", show_existing_frame = " << *show_existing_frame << " }";
    return oss.str();
  }

  oss << ", frame type = " << (is_keyframe ? "key" : "delta")
      << ", show_frame = " << (show_frame ? "true" : "false")
      << ", error_resilient = " << (error_resilient ? "true" : "false");

  oss << ", bit_depth = ";
  switch (bit_detph) {
    case Vp9BitDept::k8Bit:
      oss << "8bit";
      break;
    case Vp9BitDept::k10Bit:
      oss << "10bit";
      break;
    case Vp9BitDept::k12Bit:
      oss << "12bit";
      break;
  }

  if (color_space) {
    oss << ", color_space = ";
    switch (*color_space) {
      case Vp9ColorSpace::CS_UNKNOWN:
        oss << "unknown";
        break;
      case Vp9ColorSpace::CS_BT_601:
        oss << "CS_BT_601 Rec. ITU-R BT.601-7";
        break;
      case Vp9ColorSpace::CS_BT_709:
        oss << "Rec. ITU-R BT.709-6";
        break;
      case Vp9ColorSpace::CS_SMPTE_170:
        oss << "SMPTE-170";
        break;
      case Vp9ColorSpace::CS_SMPTE_240:
        oss << "SMPTE-240";
        break;
      case Vp9ColorSpace::CS_BT_2020:
        oss << "Rec. ITU-R BT.2020-2";
        break;
      case Vp9ColorSpace::CS_RESERVED:
        oss << "Reserved";
        break;
      case Vp9ColorSpace::CS_RGB:
        oss << "sRGB (IEC 61966-2-1)";
        break;
    }
  }

  if (color_range) {
    oss << ", color_range = ";
    switch (*color_range) {
      case Vp9ColorRange::kFull:
        oss << "full";
        break;
      case Vp9ColorRange::kStudio:
        oss << "studio";
        break;
    }
  }

  if (sub_sampling) {
    oss << ", sub_sampling = ";
    switch (*sub_sampling) {
      case Vp9YuvSubsampling::k444:
        oss << "444";
        break;
      case Vp9YuvSubsampling::k440:
        oss << "440";
        break;
      case Vp9YuvSubsampling::k422:
        oss << "422";
        break;
      case Vp9YuvSubsampling::k420:
        oss << "420";
        break;
    }
  }

  if (infer_size_from_reference) {
    oss << ", infer_frame_resolution_from = " << *infer_size_from_reference;
  } else {
    oss << ", frame_width = " << frame_width
        << ", frame_height = " << frame_height;
  }
  if (render_width != 0 && render_height != 0) {
    oss << ", render_width = " << render_width
        << ", render_height = " << render_height;
  }

  oss << ", base qp = " << base_qp;
  if (reference_buffers[0] != -1) {
    oss << ", last_buffer = " << reference_buffers[0];
  }
  if (reference_buffers[1] != -1) {
    oss << ", golden_buffer = " << reference_buffers[1];
  }
  if (reference_buffers[2] != -1) {
    oss << ", altref_buffer = " << reference_buffers[2];
  }

  oss << ", updated buffers = { ";
  bool first = true;
  for (int i = 0; i < 8; ++i) {
    if (updated_buffers.test(i)) {
      if (first) {
        first = false;
      } else {
        oss << ", ";
      }
      oss << i;
    }
  }
  oss << " }";

  oss << ", compressed_header_size_bytes = " << compressed_header_size;

  oss << " }";
  return oss.str();
}

bool Parse(rtc::ArrayView<const uint8_t> buf,
           Vp9UncompressedHeader* frame_info,
           bool qp_only) {
  rtc::BitBuffer bit_buffer(buf.data(), buf.size());
  BitstreamReader br(&bit_buffer);

  // Frame marker.
  RETURN_IF_FALSE(br.VerifyNextUnsignedIs(
      2, 0x2, "Failed to parse header. Frame marker should be 2."));

  // Profile has low bit first.
  READ_OR_RETURN(br.ReadBoolean(),
                 [frame_info](bool low) { frame_info->profile = int{low}; });
  READ_OR_RETURN(br.ReadBoolean(), [frame_info](bool high) {
    frame_info->profile |= int{high} << 1;
  });
  if (frame_info->profile > 2) {
    RETURN_IF_FALSE(br.VerifyNextBooleanIs(
        false, "Failed to get QP. Unsupported bitstream profile."));
  }

  // Show existing frame.
  RETURN_IF_FALSE(br.IfNextBoolean([&] {
    READ_OR_RETURN(br.ReadUnsigned<uint8_t>(3),
                   [frame_info](uint8_t frame_idx) {
                     frame_info->show_existing_frame = frame_idx;
                   });
    return true;
  }));
  if (frame_info->show_existing_frame.has_value()) {
    return true;
  }

  READ_OR_RETURN(br.ReadBoolean(), [frame_info](bool frame_type) {
    // Frame type: KEY_FRAME(0), INTER_FRAME(1).
    frame_info->is_keyframe = frame_type == 0;
  });
  READ_OR_RETURN(br.ReadBoolean(), [frame_info](bool show_frame) {
    frame_info->show_frame = show_frame;
  });
  READ_OR_RETURN(br.ReadBoolean(), [frame_info](bool error_resilient) {
    frame_info->error_resilient = error_resilient;
  });

  if (frame_info->is_keyframe) {
    RETURN_IF_FALSE(br.VerifyNextUnsignedIs(
        24, 0x498342, "Failed to get QP. Invalid sync code."));

    if (!Vp9ReadColorConfig(&br, frame_info))
      return false;
    if (!Vp9ReadFrameSize(&br, frame_info))
      return false;
    if (!Vp9ReadRenderSize(&br, frame_info))
      return false;

    // Key-frames implicitly update all buffers.
    frame_info->updated_buffers.set();
  } else {
    // Non-keyframe.
    bool is_intra_only = false;
    if (!frame_info->show_frame) {
      READ_OR_RETURN(br.ReadBoolean(),
                     [&](bool intra_only) { is_intra_only = intra_only; });
    }
    if (!frame_info->error_resilient) {
      RETURN_IF_FALSE(br.ConsumeBits(2));  // Reset frame context.
    }

    if (is_intra_only) {
      RETURN_IF_FALSE(br.VerifyNextUnsignedIs(
          24, 0x498342, "Failed to get QP. Invalid sync code."));

      if (frame_info->profile > 0) {
        if (!Vp9ReadColorConfig(&br, frame_info))
          return false;
      } else {
        frame_info->color_space = Vp9ColorSpace::CS_BT_601;
        frame_info->sub_sampling = Vp9YuvSubsampling::k420;
        frame_info->bit_detph = Vp9BitDept::k8Bit;
      }
      frame_info->reference_buffers.fill(-1);
      RETURN_IF_FALSE(ReadRefreshFrameFlags(&br, frame_info));
      RETURN_IF_FALSE(Vp9ReadFrameSize(&br, frame_info));
      RETURN_IF_FALSE(Vp9ReadRenderSize(&br, frame_info));
    } else {
      RETURN_IF_FALSE(ReadRefreshFrameFlags(&br, frame_info));

      frame_info->reference_buffers_sign_bias[0] = false;
      for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
        READ_OR_RETURN(br.ReadUnsigned<uint8_t>(3), [&](uint8_t idx) {
          frame_info->reference_buffers[i] = idx;
        });
        READ_OR_RETURN(br.ReadBoolean(), [&](bool sign_bias) {
          frame_info
              ->reference_buffers_sign_bias[Vp9ReferenceFrame::kLast + i] =
              sign_bias;
        });
      }

      if (!Vp9ReadFrameSizeFromRefs(&br, frame_info))
        return false;

      READ_OR_RETURN(br.ReadBoolean(), [&](bool allow_high_precision_mv) {
        frame_info->allow_high_precision_mv = allow_high_precision_mv;
      });

      // Interpolation filter.
      RETURN_IF_FALSE(br.IfNextBoolean(
          [frame_info] {
            frame_info->interpolation_filter =
                Vp9InterpolationFilter::kSwitchable;
            return true;
          },
          [&] {
            READ_OR_RETURN(
                br.ReadUnsigned<uint8_t>(2), [frame_info](uint8_t filter) {
                  frame_info->interpolation_filter = kLiteralToType[filter];
                });
            return true;
          }));
    }
  }

  if (!frame_info->error_resilient) {
    // 1 bit: Refresh frame context.
    // 1 bit: Frame parallel decoding mode.
    RETURN_IF_FALSE(br.ConsumeBits(2));
  }

  // Frame context index.
  READ_OR_RETURN(br.ReadUnsigned<uint8_t>(2),
                 [&](uint8_t idx) { frame_info->frame_context_idx = idx; });

  if (!Vp9ReadLoopfilter(&br))
    return false;

  // Read base QP.
  RETURN_IF_FALSE(Vp9ReadQp(&br, frame_info));

  if (qp_only) {
    // Not interested in the rest of the header, return early.
    return true;
  }

  RETURN_IF_FALSE(Vp9ReadSegmentationParams(&br, frame_info));
  RETURN_IF_FALSE(Vp9ReadTileInfo(&br, frame_info));
  READ_OR_RETURN(br.ReadUnsigned<uint16_t>(), [frame_info](uint16_t size) {
    frame_info->compressed_header_size = size;
  });

  // Trailing bits.
  RETURN_IF_FALSE(br.ConsumeBits(bit_buffer.RemainingBitCount() % 8));
  frame_info->uncompressed_header_size =
      buf.size() - (bit_buffer.RemainingBitCount() / 8);

  return true;
}

absl::optional<Vp9UncompressedHeader> ParseUncompressedVp9Header(
    rtc::ArrayView<const uint8_t> buf) {
  Vp9UncompressedHeader frame_info;
  if (Parse(buf, &frame_info, /*qp_only=*/false) &&
      frame_info.frame_width > 0) {
    return frame_info;
  }
  return absl::nullopt;
}

namespace vp9 {

bool GetQp(const uint8_t* buf, size_t length, int* qp) {
  Vp9UncompressedHeader frame_info;
  if (!Parse(rtc::MakeArrayView(buf, length), &frame_info, /*qp_only=*/true)) {
    return false;
  }
  *qp = frame_info.base_qp;
  return true;
}

}  // namespace vp9
}  // namespace webrtc
