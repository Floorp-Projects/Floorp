/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/BitReader.h"
#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/ByteWriter.h"
#include "mp4_demuxer/H264.h"
#include <media/stagefright/foundation/ABitReader.h>
#include <limits>
#include <cmath>

using namespace mozilla;

namespace mp4_demuxer {

// Default scaling lists (per spec).
// ITU H264:
// Table 7-2 – Assignment of mnemonic names to scaling list indices and
// specification of fall-back rule
static const uint8_t Default_4x4_Intra[16] = {
    6, 13, 13, 20,
   20, 20, 28, 28,
   28, 28, 32, 32,
   32, 37, 37, 42
};

static const uint8_t Default_4x4_Inter[16] = {
   10, 14, 14, 20,
   20, 20, 24, 24,
   24, 24, 27, 27,
   27, 30, 30, 34
};

static const uint8_t Default_8x8_Intra[64] = {
    6, 10, 10, 13, 11, 13, 16, 16,
   16, 16, 18, 18, 18, 18, 18, 23,
   23, 23, 23, 23, 23, 25, 25, 25,
   25, 25, 25, 25, 27, 27, 27, 27,
   27, 27, 27, 27, 29, 29, 29, 29,
   29, 29, 29, 31, 31, 31, 31, 31,
   31, 33, 33, 33, 33, 33, 36, 36,
   36, 36, 38, 38, 38, 40, 40, 42
};

static const uint8_t Default_8x8_Inter[64] = {
    9, 13, 13, 15, 13, 15, 17, 17,
   17, 17, 19, 19, 19, 19, 19, 21,
   21, 21, 21, 21, 21, 22, 22, 22,
   22, 22, 22, 22, 24, 24, 24, 24,
   24, 24, 24, 24, 25, 25, 25, 25,
   25, 25, 25, 27, 27, 27, 27, 27,
   27, 28, 28, 28, 28, 28, 30, 30,
   30, 30, 32, 32, 32, 33, 33, 35
};

namespace detail {
static void
scaling_list(BitReader& aBr, uint8_t* aScalingList, int aSizeOfScalingList,
             const uint8_t* aDefaultList, const uint8_t* aFallbackList)
{
  int32_t lastScale = 8;
  int32_t nextScale = 8;
  int32_t deltaScale;

  // (pic|seq)_scaling_list_present_flag[i]
  if (!aBr.ReadBit()) {
    if (aFallbackList) {
      memcpy(aScalingList, aFallbackList, aSizeOfScalingList);
    }
    return;
  }

  for (int i = 0; i < aSizeOfScalingList; i++) {
    if (nextScale != 0) {
      deltaScale = aBr.ReadSE();
      nextScale = (lastScale + deltaScale + 256) % 256;
      if (!i && !nextScale) {
        memcpy(aScalingList, aDefaultList, aSizeOfScalingList);
        return;
      }
    }
    aScalingList[i] = (nextScale == 0) ? lastScale : nextScale;
    lastScale = aScalingList[i];
  }
}
} // namespace detail.

template <size_t N>
static void
scaling_list(BitReader& aBr, uint8_t (&aScalingList)[N],
             const uint8_t (&aDefaultList)[N], const uint8_t (&aFallbackList)[N])
{
  detail::scaling_list(aBr, aScalingList, N, aDefaultList, aFallbackList);
}

template <size_t N>
static void
scaling_list(BitReader& aBr, uint8_t (&aScalingList)[N], const uint8_t (&aDefaultList)[N])
{
  detail::scaling_list(aBr, aScalingList, N, aDefaultList, nullptr);
}

SPSData::SPSData()
{
  PodZero(this);
  // Default values when they aren't defined as per ITU-T H.264 (2014/02).
  chroma_format_idc = 1;
  video_format = 5;
  colour_primaries = 2;
  transfer_characteristics = 2;
  sample_ratio = 1.0;
  memset(scaling_matrix4x4, 16, sizeof(scaling_matrix4x4));
  memset(scaling_matrix8x8, 16, sizeof(scaling_matrix8x8));
}

PPSData::PPSData()
{
  PodZero(this);
  memset(scaling_matrix4x4, 16, sizeof(scaling_matrix4x4));
  memset(scaling_matrix8x8, 16, sizeof(scaling_matrix8x8));
}

const uint8_t H264::ZZ_SCAN[16] = { 0,  1,  4,  8,
                                    5,  2,  3,  6,
                                    9, 12, 13, 10,
                                    7, 11, 14, 15 };

const uint8_t H264::ZZ_SCAN8[64] = {  0,  1,  8, 16,  9,  2,  3, 10,
                                     17, 24, 32, 25, 18, 11,  4,  5,
                                     12, 19, 26, 33, 40, 48, 41, 34,
                                     27, 20, 13,  6,  7, 14, 21, 28,
                                     35, 42, 49, 56, 57, 50, 43, 36,
                                     29, 22, 15, 23, 30, 37, 44, 51,
                                     58, 59, 52, 45, 38, 31, 39, 46,
                                     53, 60, 61, 54, 47, 55, 62, 63 };

/* static */ already_AddRefed<mozilla::MediaByteBuffer>
H264::DecodeNALUnit(const mozilla::MediaByteBuffer* aNAL)
{
  MOZ_ASSERT(aNAL);

  if (aNAL->Length() < 4) {
    return nullptr;
  }

  RefPtr<mozilla::MediaByteBuffer> rbsp = new mozilla::MediaByteBuffer;
  ByteReader reader(aNAL);
  uint8_t nal_unit_type = reader.ReadU8() & 0x1f;
  uint32_t nalUnitHeaderBytes = 1;
  if (nal_unit_type == 14 || nal_unit_type == 20 || nal_unit_type == 21) {
    bool svc_extension_flag = false;
    bool avc_3d_extension_flag = false;
    if (nal_unit_type != 21) {
      svc_extension_flag = reader.PeekU8() & 0x80;
    } else {
      avc_3d_extension_flag = reader.PeekU8() & 0x80;
    }
    if (svc_extension_flag) {
      nalUnitHeaderBytes += 3;
    } else if (avc_3d_extension_flag) {
      nalUnitHeaderBytes += 2;
    } else {
      nalUnitHeaderBytes += 3;
    }
  }
  if (!reader.Read(nalUnitHeaderBytes - 1)) {
    return nullptr;
  }
  uint32_t lastbytes = 0xffff;
  while (reader.Remaining()) {
    uint8_t byte = reader.ReadU8();
    if ((lastbytes & 0xffff) == 0 && byte == 0x03) {
      // reset last two bytes, to detect the 0x000003 sequence again.
      lastbytes = 0xffff;
    } else {
      rbsp->AppendElement(byte);
    }
    lastbytes = (lastbytes << 8) | byte;
  }
  return rbsp.forget();
}

static int32_t
ConditionDimension(float aValue)
{
  // This will exclude NaNs and too-big values.
  if (aValue > 1.0 && aValue <= INT32_MAX)
    return int32_t(aValue);
  return 0;
}

/* static */ bool
H264::DecodeSPS(const mozilla::MediaByteBuffer* aSPS, SPSData& aDest)
{
  if (!aSPS) {
    return false;
  }
  BitReader br(aSPS);

  aDest.profile_idc = br.ReadBits(8);
  aDest.constraint_set0_flag = br.ReadBit();
  aDest.constraint_set1_flag = br.ReadBit();
  aDest.constraint_set2_flag = br.ReadBit();
  aDest.constraint_set3_flag = br.ReadBit();
  aDest.constraint_set4_flag = br.ReadBit();
  aDest.constraint_set5_flag = br.ReadBit();
  br.ReadBits(2); // reserved_zero_2bits
  aDest.level_idc = br.ReadBits(8);
  aDest.seq_parameter_set_id = br.ReadUE();
  if (aDest.profile_idc == 100 || aDest.profile_idc == 110 ||
      aDest.profile_idc == 122 || aDest.profile_idc == 244 ||
      aDest.profile_idc == 44 || aDest.profile_idc == 83 ||
      aDest.profile_idc == 86 || aDest.profile_idc == 118 ||
      aDest.profile_idc == 128 || aDest.profile_idc == 138 ||
      aDest.profile_idc == 139 || aDest.profile_idc == 134) {
    if ((aDest.chroma_format_idc = br.ReadUE()) == 3) {
      aDest.separate_colour_plane_flag = br.ReadBit();
    }
    aDest.bit_depth_luma_minus8 = br.ReadUE();
    aDest.bit_depth_chroma_minus8 = br.ReadUE();
    br.ReadBit();       // qpprime_y_zero_transform_bypass_flag
    aDest.seq_scaling_matrix_present_flag = br.ReadBit();
    if (aDest.seq_scaling_matrix_present_flag) {
      scaling_list(br, aDest.scaling_matrix4x4[0], Default_4x4_Intra,
                   Default_4x4_Intra);
      scaling_list(br, aDest.scaling_matrix4x4[1], Default_4x4_Intra,
                   aDest.scaling_matrix4x4[0]);
      scaling_list(br, aDest.scaling_matrix4x4[2], Default_4x4_Intra,
                   aDest.scaling_matrix4x4[1]);
      scaling_list(br, aDest.scaling_matrix4x4[3], Default_4x4_Inter,
                   Default_4x4_Inter);
      scaling_list(br, aDest.scaling_matrix4x4[4], Default_4x4_Inter,
                   aDest.scaling_matrix4x4[3]);
      scaling_list(br, aDest.scaling_matrix4x4[5], Default_4x4_Inter,
                   aDest.scaling_matrix4x4[4]);

      scaling_list(br, aDest.scaling_matrix8x8[0], Default_8x8_Intra,
                   Default_8x8_Intra);
      scaling_list(br, aDest.scaling_matrix8x8[1], Default_8x8_Inter,
                   Default_8x8_Inter);
      if (aDest.chroma_format_idc == 3) {
        scaling_list(br, aDest.scaling_matrix8x8[2], Default_8x8_Intra,
                     aDest.scaling_matrix8x8[0]);
        scaling_list(br, aDest.scaling_matrix8x8[3], Default_8x8_Inter,
                     aDest.scaling_matrix8x8[1]);
        scaling_list(br, aDest.scaling_matrix8x8[4], Default_8x8_Intra,
                     aDest.scaling_matrix8x8[2]);
        scaling_list(br, aDest.scaling_matrix8x8[5], Default_8x8_Inter,
                     aDest.scaling_matrix8x8[3]);
      }
    }
  } else if (aDest.profile_idc == 183) {
    aDest.chroma_format_idc = 0;
  } else {
    // default value if chroma_format_idc isn't set.
    aDest.chroma_format_idc = 1;
  }
  aDest.log2_max_frame_num = br.ReadUE() + 4;
  aDest.pic_order_cnt_type = br.ReadUE();
  if (aDest.pic_order_cnt_type == 0) {
    aDest.log2_max_pic_order_cnt_lsb = br.ReadUE() + 4;
  } else if (aDest.pic_order_cnt_type == 1) {
    aDest.delta_pic_order_always_zero_flag = br.ReadBit();
    aDest.offset_for_non_ref_pic = br.ReadSE();
    aDest.offset_for_top_to_bottom_field = br.ReadSE();
    uint32_t num_ref_frames_in_pic_order_cnt_cycle = br.ReadUE();
    for (uint32_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
      br.ReadSE(); // offset_for_ref_frame[i]
    }
  }
  aDest.max_num_ref_frames = br.ReadUE();
  aDest.gaps_in_frame_num_allowed_flag = br.ReadBit();
  aDest.pic_width_in_mbs = br.ReadUE() + 1;
  aDest.pic_height_in_map_units = br.ReadUE() + 1;
  aDest.frame_mbs_only_flag = br.ReadBit();
  if (!aDest.frame_mbs_only_flag) {
    aDest.pic_height_in_map_units *= 2;
    aDest.mb_adaptive_frame_field_flag = br.ReadBit();
  }
  aDest.direct_8x8_inference_flag = br.ReadBit();
  aDest.frame_cropping_flag = br.ReadBit();
  if (aDest.frame_cropping_flag) {
    aDest.frame_crop_left_offset = br.ReadUE();
    aDest.frame_crop_right_offset = br.ReadUE();
    aDest.frame_crop_top_offset = br.ReadUE();
    aDest.frame_crop_bottom_offset = br.ReadUE();
  }

  aDest.sample_ratio = 1.0f;
  aDest.vui_parameters_present_flag = br.ReadBit();
  if (aDest.vui_parameters_present_flag) {
    vui_parameters(br, aDest);
  }

  // Calculate common values.

  uint8_t ChromaArrayType =
    aDest.separate_colour_plane_flag ? 0 : aDest.chroma_format_idc;
  // Calculate width.
  uint32_t CropUnitX = 1;
  uint32_t SubWidthC = aDest.chroma_format_idc == 3 ? 1 : 2;
  if (ChromaArrayType != 0) {
    CropUnitX = SubWidthC;
  }

  // Calculate Height
  uint32_t CropUnitY = 2 - aDest.frame_mbs_only_flag;
  uint32_t SubHeightC = aDest.chroma_format_idc <= 1 ? 2 : 1;
  if (ChromaArrayType != 0) {
    CropUnitY *= SubHeightC;
  }

  uint32_t width = aDest.pic_width_in_mbs * 16;
  uint32_t height = aDest.pic_height_in_map_units * 16;
  if (aDest.frame_crop_left_offset <= std::numeric_limits<int32_t>::max() / 4 / CropUnitX &&
      aDest.frame_crop_right_offset <= std::numeric_limits<int32_t>::max() / 4 / CropUnitX &&
      aDest.frame_crop_top_offset <= std::numeric_limits<int32_t>::max() / 4 / CropUnitY &&
      aDest.frame_crop_bottom_offset <= std::numeric_limits<int32_t>::max() / 4 / CropUnitY &&
      (aDest.frame_crop_left_offset + aDest.frame_crop_right_offset) * CropUnitX < width &&
      (aDest.frame_crop_top_offset + aDest.frame_crop_bottom_offset) * CropUnitY < height) {
    aDest.crop_left = aDest.frame_crop_left_offset * CropUnitX;
    aDest.crop_right = aDest.frame_crop_right_offset * CropUnitX;
    aDest.crop_top = aDest.frame_crop_top_offset * CropUnitY;
    aDest.crop_bottom = aDest.frame_crop_bottom_offset * CropUnitY;
  } else {
    // Nonsensical value, ignore them.
    aDest.crop_left = aDest.crop_right = aDest.crop_top = aDest.crop_bottom = 0;
  }

  aDest.pic_width = width - aDest.crop_left - aDest.crop_right;
  aDest.pic_height = height - aDest.crop_top - aDest.crop_bottom;

  aDest.interlaced = !aDest.frame_mbs_only_flag;

  // Determine display size.
  if (aDest.sample_ratio > 1.0) {
    // Increase the intrinsic width
    aDest.display_width =
      ConditionDimension(aDest.pic_width * aDest.sample_ratio);
    aDest.display_height = aDest.pic_height;
  } else {
    // Increase the intrinsic height
    aDest.display_width = aDest.pic_width;
    aDest.display_height =
      ConditionDimension(aDest.pic_height / aDest.sample_ratio);
  }

  return true;
}

/* static */ void
H264::vui_parameters(BitReader& aBr, SPSData& aDest)
{
  aDest.aspect_ratio_info_present_flag = aBr.ReadBit();
  if (aDest.aspect_ratio_info_present_flag) {
    aDest.aspect_ratio_idc = aBr.ReadBits(8);
    aDest.sar_width = aDest.sar_height = 0;

    // From E.2.1 VUI parameters semantics (ITU-T H.264 02/2014)
    switch (aDest.aspect_ratio_idc) {
      case 0:
        // Unspecified
        break;
      case 1:
        /*
          1:1
         7680x4320 16:9 frame without horizontal overscan
         3840x2160 16:9 frame without horizontal overscan
         1280x720 16:9 frame without horizontal overscan
         1920x1080 16:9 frame without horizontal overscan (cropped from 1920x1088)
         640x480 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 1.0f;
        break;
      case 2:
        /*
          12:11
         720x576 4:3 frame with horizontal overscan
         352x288 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 12.0 / 11.0;
        break;
      case 3:
        /*
          10:11
         720x480 4:3 frame with horizontal overscan
         352x240 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 10.0 / 11.0;
        break;
      case 4:
        /*
          16:11
         720x576 16:9 frame with horizontal overscan
         528x576 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 16.0 / 11.0;
        break;
      case 5:
        /*
          40:33
         720x480 16:9 frame with horizontal overscan
         528x480 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 40.0 / 33.0;
        break;
      case 6:
        /*
          24:11
         352x576 4:3 frame without horizontal overscan
         480x576 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 24.0 / 11.0;
        break;
      case 7:
        /*
          20:11
         352x480 4:3 frame without horizontal overscan
         480x480 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 20.0 / 11.0;
        break;
      case 8:
        /*
          32:11
         352x576 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 32.0 / 11.0;
        break;
      case 9:
        /*
          80:33
         352x480 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 80.0 / 33.0;
        break;
      case 10:
        /*
          18:11
         480x576 4:3 frame with horizontal overscan
         */
        aDest.sample_ratio = 18.0 / 11.0;
        break;
      case 11:
        /*
          15:11
         480x480 4:3 frame with horizontal overscan
         */
        aDest.sample_ratio = 15.0 / 11.0;
        break;
      case 12:
        /*
          64:33
         528x576 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 64.0 / 33.0;
        break;
      case 13:
        /*
          160:99
         528x480 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 160.0 / 99.0;
        break;
      case 14:
        /*
          4:3
         1440x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 4.0 / 3.0;
        break;
      case 15:
        /*
          3:2
         1280x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 3.2 / 2.0;
        break;
      case 16:
        /*
          2:1
         960x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 2.0 / 1.0;
        break;
      case 255:
        /* Extended_SAR */
        aDest.sar_width = aBr.ReadBits(16);
        aDest.sar_height = aBr.ReadBits(16);
        if (aDest.sar_width && aDest.sar_height) {
          aDest.sample_ratio = float(aDest.sar_width) / float(aDest.sar_height);
        }
        break;
      default:
        break;
    }
  }

  if (aBr.ReadBit()) { //overscan_info_present_flag
    aDest.overscan_appropriate_flag = aBr.ReadBit();
  }

  if (aBr.ReadBit()) { // video_signal_type_present_flag
    aDest.video_format = aBr.ReadBits(3);
    aDest.video_full_range_flag = aBr.ReadBit();
    aDest.colour_description_present_flag = aBr.ReadBit();
    if (aDest.colour_description_present_flag) {
      aDest.colour_primaries = aBr.ReadBits(8);
      aDest.transfer_characteristics = aBr.ReadBits(8);
      aDest.matrix_coefficients = aBr.ReadBits(8);
    }
  }

  aDest.chroma_loc_info_present_flag = aBr.ReadBit();
  if (aDest.chroma_loc_info_present_flag) {
    aDest.chroma_sample_loc_type_top_field = aBr.ReadUE();
    aDest.chroma_sample_loc_type_bottom_field = aBr.ReadUE();
  }

  aDest.timing_info_present_flag = aBr.ReadBit();
  if (aDest.timing_info_present_flag) {
    aDest.num_units_in_tick = aBr.ReadBits(32);
    aDest.time_scale = aBr.ReadBits(32);
    aDest.fixed_frame_rate_flag = aBr.ReadBit();
  }
}

/* static */ bool
H264::DecodeSPSFromExtraData(const mozilla::MediaByteBuffer* aExtraData,
                             SPSData& aDest)
{
  if (!AnnexB::HasSPS(aExtraData)) {
    return false;
  }
  ByteReader reader(aExtraData);

  if (!reader.Read(5)) {
    return false;
  }

  uint8_t numSps = reader.ReadU8() & 0x1f;
  if (!numSps) {
    // No SPS.
    return false;
  }
  NS_ASSERTION(numSps == 1, "More than one SPS in extradata");

  uint16_t length = reader.ReadU16();

  if ((reader.PeekU8() & 0x1f) != 7) {
    // Not a SPS NAL type.
    return false;
  }
  const uint8_t* ptr = reader.Read(length);
  if (!ptr) {
    return false;
  }

  RefPtr<mozilla::MediaByteBuffer> rawNAL = new mozilla::MediaByteBuffer;
  rawNAL->AppendElements(ptr, length);

  RefPtr<mozilla::MediaByteBuffer> sps = DecodeNALUnit(rawNAL);

  if (!sps) {
    return false;
  }

  return DecodeSPS(sps, aDest);
}

/* static */ bool
H264::EnsureSPSIsSane(SPSData& aSPS)
{
  bool valid = true;
  static const float default_aspect = 4.0f / 3.0f;
  if (aSPS.sample_ratio <= 0.0f || aSPS.sample_ratio > 6.0f) {
    if (aSPS.pic_width && aSPS.pic_height) {
      aSPS.sample_ratio = (float)aSPS.pic_width / (float)aSPS.pic_height;
    } else {
      aSPS.sample_ratio = default_aspect;
    }
    aSPS.display_width = aSPS.pic_width;
    aSPS.display_height = aSPS.pic_height;
    valid = false;
  }
  if (aSPS.max_num_ref_frames > 16) {
    aSPS.max_num_ref_frames = 16;
    valid = false;
  }
  return valid;
}

/* static */ bool
H264::DecodePPSFromExtraData(const mozilla::MediaByteBuffer* aExtraData,
                             const SPSData& aSPS, PPSData& aDest)
{
  if (!AnnexB::HasPPS(aExtraData)) {
    return false;
  }
  ByteReader reader(aExtraData);

  if (!reader.Read(5)) {
    return false;
  }

  uint8_t numSps = reader.ReadU8() & 0x1f;
  if (!numSps) {
    // No SPS.
    return false;
  }
  NS_ASSERTION(numSps == 1, "More than one SPS in extradata");
  for (uint8_t i = 0; i < numSps; i++) {
    uint16_t length = reader.ReadU16();

    if ((reader.PeekU8() & 0x1f) != 7) {
      // Not a SPS NAL type.
      return false;
    }
    const uint8_t* ptr = reader.Read(length);
    if (!ptr) {
      return false;
    }
  }
  uint8_t numPps = reader.ReadU8();
  if (!numPps) {
    // No PPs.
    return false;
  }
  NS_ASSERTION(numPps == 1, "More than one PPS in extradata");
  uint16_t length = reader.ReadU16();

  if ((reader.PeekU8() & 0x1f) != 8) {
    // Not a PPS NAL type.
    return false;
  }
  const uint8_t* ptr = reader.Read(length);
  if (!ptr) {
    return false;
  }

  RefPtr<mozilla::MediaByteBuffer> rawNAL = new mozilla::MediaByteBuffer;
  rawNAL->AppendElements(ptr, length);

  RefPtr<mozilla::MediaByteBuffer> pps = DecodeNALUnit(rawNAL);

  if (!pps) {
    return false;
  }

  return DecodePPS(pps, aSPS, aDest);
}

/* static */ bool
H264::DecodePPS(const mozilla::MediaByteBuffer* aPPS,const SPSData& aSPS,
                PPSData& aDest)
{
  if (!aPPS) {
    return false;
  }

  memcpy(aDest.scaling_matrix4x4, aSPS.scaling_matrix4x4,
         sizeof(aDest.scaling_matrix4x4));
  memcpy(aDest.scaling_matrix8x8, aSPS.scaling_matrix8x8,
         sizeof(aDest.scaling_matrix8x8));

  BitReader br(aPPS);

  aDest.pic_parameter_set_id = br.ReadUE();
  aDest.seq_parameter_set_id = br.ReadUE();
  aDest.entropy_coding_mode_flag = br.ReadBit();
  aDest.bottom_field_pic_order_in_frame_present_flag = br.ReadBit();
  aDest.num_slice_groups_minus1 = br.ReadUE();
  if (aDest.num_slice_groups_minus1 > 0) {
    aDest.slice_group_map_type = br.ReadUE();
    switch (aDest.slice_group_map_type) {
      case 0:
        for (uint8_t iGroup = 0; iGroup <= aDest.num_slice_groups_minus1;
             iGroup++) {
          aDest.run_length_minus1[iGroup] = br.ReadUE();
        }
        break;
      case 2:
        for (uint8_t iGroup = 0; iGroup < aDest.num_slice_groups_minus1;
             iGroup++) {
          aDest.top_left[iGroup] = br.ReadUE();
          aDest.bottom_right[iGroup] = br.ReadUE();
        }
        break;
      case 3:
      case 4:
      case 5:
        aDest.slice_group_change_direction_flag = br.ReadBit();
        aDest.slice_group_change_rate_minus1 = br.ReadUE();
        break;
      case 6:
        aDest.pic_size_in_map_units_minus1 = br.ReadUE();
        for (uint32_t i = 0; i <= aDest.pic_size_in_map_units_minus1; i++) {
          /* slice_group_id[ i ] identifies a slice group of the i-th slice group map
            unit in raster scan order. The length of the slice_group_id[i] syntax
            element is Ceil(Log2(num_slice_groups_minus1+1)) bits. The value of
            slice_group_id[i] shall be in the range of 0 to num_slice_groups_minus1,
            inclusive. */
          br.ReadBits(std::ceil(std::log2(aDest.num_slice_groups_minus1 + 1)));
        }
        break;
      default:
        return false;
    }
  }
  aDest.num_ref_idx_l0_default_active_minus1 = br.ReadUE();
  aDest.num_ref_idx_l1_default_active_minus1 = br.ReadUE();
  if (aDest.num_ref_idx_l0_default_active_minus1 > 32 ||
      aDest.num_ref_idx_l1_default_active_minus1 > 32) {
    // reference overflow.
    return false;
  }
  aDest.weighted_pred_flag = br.ReadBit();
  aDest.weighted_bipred_idc = br.ReadBits(2);
  aDest.pic_init_qp_minus26 = br.ReadSE();
  aDest.pic_init_qs_minus26 = br.ReadSE();
  aDest.chroma_qp_index_offset = br.ReadSE();
  aDest.deblocking_filter_control_present_flag = br.ReadBit();
  aDest.constrained_intra_pred_flag = br.ReadBit();
  aDest.redundant_pic_cnt_present_flag = br.ReadBit();
  if (br.BitsLeft()) {
    aDest.transform_8x8_mode_flag = br.ReadBit();
    if (br.ReadBit()) { // pic_scaling_matrix_present_flag
      if (aSPS.seq_scaling_matrix_present_flag) {
        scaling_list(br, aDest.scaling_matrix4x4[0], Default_4x4_Intra);
        scaling_list(br, aDest.scaling_matrix4x4[1], Default_4x4_Intra,
                     aDest.scaling_matrix4x4[0]);
        scaling_list(br, aDest.scaling_matrix4x4[2], Default_4x4_Intra,
                     aDest.scaling_matrix4x4[1]);
        scaling_list(br, aDest.scaling_matrix4x4[3], Default_4x4_Inter);
      } else {
        scaling_list(br, aDest.scaling_matrix4x4[0], Default_4x4_Intra,
                     Default_4x4_Intra);
        scaling_list(br, aDest.scaling_matrix4x4[1], Default_4x4_Intra,
                     aDest.scaling_matrix4x4[0]);
        scaling_list(br, aDest.scaling_matrix4x4[2], Default_4x4_Intra,
                     aDest.scaling_matrix4x4[1]);
        scaling_list(br, aDest.scaling_matrix4x4[3], Default_4x4_Inter,
                     Default_4x4_Inter);
      }
      scaling_list(br, aDest.scaling_matrix4x4[4], Default_4x4_Inter,
                   aDest.scaling_matrix4x4[3]);
      scaling_list(br, aDest.scaling_matrix4x4[5], Default_4x4_Inter,
                   aDest.scaling_matrix4x4[4]);
      if (aDest.transform_8x8_mode_flag) {
        if (aSPS.seq_scaling_matrix_present_flag) {
          scaling_list(br, aDest.scaling_matrix8x8[0], Default_8x8_Intra);
          scaling_list(br, aDest.scaling_matrix8x8[1], Default_8x8_Inter);
        } else {
          scaling_list(br, aDest.scaling_matrix8x8[0], Default_8x8_Intra,
                       Default_8x8_Intra);
          scaling_list(br, aDest.scaling_matrix8x8[1], Default_8x8_Inter,
                       Default_8x8_Inter);
        }
        if (aSPS.chroma_format_idc == 3) {
          scaling_list(br, aDest.scaling_matrix8x8[2], Default_8x8_Intra,
                       aDest.scaling_matrix8x8[0]);
          scaling_list(br, aDest.scaling_matrix8x8[3], Default_8x8_Inter,
                       aDest.scaling_matrix8x8[1]);
          scaling_list(br, aDest.scaling_matrix8x8[4], Default_8x8_Intra,
                       aDest.scaling_matrix8x8[2]);
          scaling_list(br, aDest.scaling_matrix8x8[5], Default_8x8_Inter,
                       aDest.scaling_matrix8x8[3]);
        }
      }
    }
    aDest.second_chroma_qp_index_offset = br.ReadSE();
  }
  return true;
}

/* static */ uint32_t
H264::ComputeMaxRefFrames(const mozilla::MediaByteBuffer* aExtraData)
{
  uint32_t maxRefFrames = 4;
  // Retrieve video dimensions from H264 SPS NAL.
  SPSData spsdata;
  if (DecodeSPSFromExtraData(aExtraData, spsdata)) {
    // max_num_ref_frames determines the size of the sliding window
    // we need to queue that many frames in order to guarantee proper
    // pts frames ordering. Use a minimum of 4 to ensure proper playback of
    // non compliant videos.
    maxRefFrames =
      std::min(std::max(maxRefFrames, spsdata.max_num_ref_frames + 1), 16u);
  }
  return maxRefFrames;
}

/* static */ H264::FrameType
H264::GetFrameType(const mozilla::MediaRawData* aSample)
{
  if (!AnnexB::IsAVCC(aSample)) {
    // We must have a valid AVCC frame with extradata.
    return FrameType::INVALID;
  }
  MOZ_ASSERT(aSample->Data());

  int nalLenSize = ((*aSample->mExtraData)[4] & 3) + 1;

  ByteReader reader(aSample->Data(), aSample->Size());

  while (reader.Remaining() >= nalLenSize) {
    uint32_t nalLen;
    switch (nalLenSize) {
      case 1: nalLen = reader.ReadU8();  break;
      case 2: nalLen = reader.ReadU16(); break;
      case 3: nalLen = reader.ReadU24(); break;
      case 4: nalLen = reader.ReadU32(); break;
    }
    if (!nalLen) {
      continue;
    }
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      return FrameType::INVALID;
    }
    if ((p[0] & 0x1f) == 5) {
      // IDR NAL.
      return FrameType::I_FRAME;
    }
  }

  return FrameType::OTHER;
}

} // namespace mp4_demuxer
