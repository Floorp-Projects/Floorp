/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/ByteWriter.h"
#include "mp4_demuxer/H264.h"
#include <media/stagefright/foundation/ABitReader.h>

using namespace mozilla;

namespace mp4_demuxer
{

class BitReader
{
public:
  explicit BitReader(const ByteBuffer& aBuffer)
  : mBitReader(aBuffer.Elements(), aBuffer.Length())
  {
  }

  uint32_t ReadBits(size_t aNum)
  {
    MOZ_ASSERT(mBitReader.numBitsLeft());
    MOZ_ASSERT(aNum <= 32);
    if (mBitReader.numBitsLeft() < aNum) {
      return 0;
    }
    return mBitReader.getBits(aNum);
  }

  uint32_t ReadBit()
  {
    return ReadBits(1);
  }

  // Read unsigned integer Exp-Golomb-coded.
  uint32_t ReadUE()
  {
    uint32_t i = 0;

    while (ReadBit() == 0 && i < 32) {
      i++;
    }
    if (i == 32) {
      MOZ_ASSERT(false);
      return 0;
    }
    uint32_t r = ReadBits(i);
    r += (1 << i) - 1;
    return r;
  }

  // Read signed integer Exp-Golomb-coded.
  int32_t ReadSE()
  {
    int32_t r = ReadUE();
    if (r & 1) {
      return (r+1) / 2;
    } else {
      return -r / 2;
    }
  }

private:
  stagefright::ABitReader mBitReader;
};

SPSData::SPSData()
{
  PodZero(this);
  chroma_format_idc = 1;
}

/* static */ already_AddRefed<ByteBuffer>
H264::DecodeNALUnit(const ByteBuffer* aNAL)
{
  MOZ_ASSERT(aNAL);

  if (aNAL->Length() < 4) {
    return nullptr;
  }

  nsRefPtr<ByteBuffer> rbsp = new ByteBuffer;
  ByteReader reader(*aNAL);
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
  uint32_t zeros = 0;
  while (reader.Remaining()) {
    uint8_t byte = reader.ReadU8();
    if (zeros < 2 || byte == 0x03) {
      rbsp->AppendElement(byte);
    }
    if (byte == 0) {
      zeros++;
    } else {
      zeros = 0;
    }
  }
  return rbsp.forget();
}

/* static */ bool
H264::DecodeSPS(const ByteBuffer* aSPS, SPSData& aDest)
{
  MOZ_ASSERT(aSPS);
  BitReader br(*aSPS);

  int32_t lastScale;
  int32_t nextScale;
  int32_t deltaScale;

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
    br.ReadUE();  // bit_depth_luma_minus8
    br.ReadUE();  // bit_depth_chroma_minus8
    br.ReadBit(); // qpprime_y_zero_transform_bypass_flag
    if (br.ReadBit()) { // seq_scaling_matrix_present_flag
      for (int idx = 0; idx < ((aDest.chroma_format_idc != 3) ? 8 : 12); ++idx) {
        if (br.ReadBit()) { // Scaling list present
          lastScale = nextScale = 8;
          int sl_n = (idx < 6) ? 16 : 64;
          for (int sl_i = 0; sl_i < sl_n; sl_i++) {
            if (nextScale) {
              deltaScale = br.ReadSE();
              nextScale = (lastScale + deltaScale + 256) % 256;
            }
            lastScale = (nextScale == 0) ? lastScale : nextScale;
          }
        }
      }
    }
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
  br.ReadBit(); // direct_8x8_inference_flag
  aDest.frame_cropping_flag = br.ReadBit();
  if (aDest.frame_cropping_flag) {
    aDest.frame_crop_left_offset = br.ReadUE();
    aDest.frame_crop_right_offset = br.ReadUE();
    aDest.frame_crop_top_offset = br.ReadUE();
    aDest.frame_crop_bottom_offset = br.ReadUE();
  }

  // Calculate common values.

  // FFmpeg and VLC ignore the left and top cropping. Do the same here.

  uint8_t ChromaArrayType =
    aDest.separate_colour_plane_flag ? 0 : aDest.chroma_format_idc;
  // Calculate width.
  uint32_t CropUnitX = 1;
  uint32_t SubWidthC = aDest.chroma_format_idc == 3 ? 1 : 2;
  if (ChromaArrayType != 0) {
    CropUnitX = SubWidthC;
  }
  uint32_t cropX = CropUnitX * aDest.frame_crop_right_offset;
  aDest.pic_width = aDest.pic_width_in_mbs * 16 - cropX;

  // Calculate Height
  uint32_t CropUnitY = 2 - aDest.frame_mbs_only_flag;
  uint32_t SubHeightC = aDest.chroma_format_idc <= 1 ? 2 : 1;
  if (ChromaArrayType != 0)
    CropUnitY *= SubHeightC;
  uint32_t cropY = CropUnitY * aDest.frame_crop_bottom_offset;
  aDest.pic_height = aDest.pic_height_in_map_units * 16 - cropY;

  aDest.interlaced = !aDest.frame_mbs_only_flag;
  return true;
}

/* static */ void
H264::vui_parameters(BitReader& aBr, SPSData& aDest)
{
  aDest.aspect_ratio_info_present_flag = aBr.ReadBit();
  if (aDest.aspect_ratio_info_present_flag)
  {
    aDest.aspect_ratio_idc = aBr.ReadBits(8);

    if (aDest.aspect_ratio_idc == 255 /* EXTENDED_SAR */) {
      aDest.sar_width  = aBr.ReadBits(16);
      aDest.sar_height = aBr.ReadBits(16);
    }
  }
  else {
    aDest.sar_width = aDest.sar_height = 0;
  }

  if (aBr.ReadBit()) { //overscan_info_present_flag
    aDest.overscan_appropriate_flag = aBr.ReadBit();
  }
  if (aBr.ReadBit()) { //video_signal_type_present_flag
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

  if (aBr.ReadBit()) { //timing_info_present_flag
    aDest.num_units_in_tick = aBr.ReadBits(32);
    aDest.time_scale = aBr.ReadBits(32);
    aDest.fixed_frame_rate_flag = aBr.ReadBit();
  }

  bool hrd_present = false;
  if (aBr.ReadBit()) { // nal_hrd_parameters_present_flag
    hrd_parameters(aBr);
    hrd_present = true;
  }
  if (aBr.ReadBit()) { // vcl_hrd_parameters_present_flag
    hrd_parameters(aBr);
    hrd_present = true;
  }
  if (hrd_present) {
    aBr.ReadBit(); // low_delay_hrd_flag
  }
  aDest.pic_struct_present_flag = aBr.ReadBit();
  aDest.bitstream_restriction_flag = aBr.ReadBit();
  if (aDest.bitstream_restriction_flag) {
    aDest.motion_vectors_over_pic_boundaries_flag = aBr.ReadBit();
    aDest.max_bytes_per_pic_denom = aBr.ReadUE();
    aDest.max_bits_per_mb_denom = aBr.ReadUE();
    aDest.log2_max_mv_length_horizontal = aBr.ReadUE();
    aDest.log2_max_mv_length_vertical = aBr.ReadUE();
    aDest.max_num_reorder_frames = aBr.ReadUE();
    aDest.max_dec_frame_buffering = aBr.ReadUE();
  }
}

/* static */ void
H264::hrd_parameters(BitReader& aBr)
{
  uint32_t cpb_cnt_minus1 = aBr.ReadUE();
  aBr.ReadBits(4); // bit_rate_scale
  aBr.ReadBits(4); // cpb_size_scale
  for (uint32_t SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++) {
    aBr.ReadUE(); // bit_rate_value_minus1[ SchedSelIdx ]
    aBr.ReadUE(); // cpb_size_value_minus1[ SchedSelIdx ]
    aBr.ReadBit(); // cbr_flag[ SchedSelIdx ]
  }
  aBr.ReadBits(5); // initial_cpb_removal_delay_length_minus1
  aBr.ReadBits(5); // cpb_removal_delay_length_minus1
  aBr.ReadBits(5); // dpb_output_delay_length_minus1
  aBr.ReadBits(5); // time_offset_length
}

/* static */ bool
H264::DecodeSPSFromExtraData(const ByteBuffer* aExtraData, SPSData& aDest)
{
  if (!AnnexB::HasSPS(aExtraData)) {
    return false;
  }
  ByteReader reader(*aExtraData);

  if (!reader.Read(5)) {
    return false;
  }

  if (!(reader.ReadU8() & 0x1f)) {
    // No SPS.
    reader.DiscardRemaining();
    return false;
  }
  uint16_t length = reader.ReadU16();

  if ((reader.PeekU8() & 0x1f) != 7) {
    // Not a SPS NAL type.
    reader.DiscardRemaining();
    return false;
  }

  const uint8_t* ptr = reader.Read(length);
  if (!ptr) {
    return false;
  }

  nsRefPtr<ByteBuffer> rawNAL = new ByteBuffer;
  rawNAL->AppendElements(ptr, length);

  nsRefPtr<ByteBuffer> sps = DecodeNALUnit(rawNAL);

  reader.DiscardRemaining();

  return DecodeSPS(sps, aDest);
}

} // namespace mp4_demuxer
