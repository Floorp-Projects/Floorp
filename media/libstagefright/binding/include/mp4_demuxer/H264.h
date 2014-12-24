/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_H264_H_
#define MP4_DEMUXER_H264_H_

#include "mp4_demuxer/DecoderData.h"

namespace mp4_demuxer
{

class BitReader;

struct SPSData
{
  /* Decoded Members */
  /*
    pic_width is the decoded width according to:
    pic_width = ((pic_width_in_mbs_minus1 + 1) * 16)
                - (frame_crop_left_offset + frame_crop_right_offset) * 2
   */
  uint32_t pic_width;
  /*
    pic_height is the decoded height according to:
    pic_height = (2 - frame_mbs_only_flag) * ((pic_height_in_map_units_minus1 + 1) * 16)
                 - (frame_crop_top_offset + frame_crop_bottom_offset) * 2
   */
  uint32_t pic_height;

  bool interlaced;

  /*
    H264 decoding parameters according to ITU-T H.264 (T-REC-H.264-201402-I/en)
   http://www.itu.int/rec/T-REC-H.264-201402-I/en
   */

  bool constraint_set0_flag;
  bool constraint_set1_flag;
  bool constraint_set2_flag;
  bool constraint_set3_flag;
  bool constraint_set4_flag;
  bool constraint_set5_flag;

  /*
    profile_idc and level_idc indicate the profile and level to which the coded
    video sequence conforms when the SVC sequence parameter set is the active
    SVC sequence parameter set.
   */
  uint8_t profile_idc;
  uint8_t level_idc;

  /*
    seq_parameter_set_id identifies the sequence parameter set that is referred
    to by the picture parameter set. The value of seq_parameter_set_id shall be
    in the range of 0 to 31, inclusive.
   */
  uint8_t seq_parameter_set_id;

  /*
    When the value of chroma_format_idc is equal to 1, the nominal vertical
    and horizontal relative locations of luma and chroma samples in frames are
    shown in Figure 6-1. Alternative chroma sample relative locations may be
    indicated in video usability information (see Annex E).
   */
  uint8_t chroma_format_idc;

  /*
    If separate_colour_plane_flag is equal to 0, each of the two chroma arrays
    has the same height and width as the luma array. Otherwise
    (separate_colour_plane_flag is equal to 1), the three colour planes are
    separately processed as monochrome sampled pictures.
   */
  bool separate_colour_plane_flag;

  /*
    log2_max_frame_num_minus4 specifies the value of the variable
    MaxFrameNum that is used in frame_num related derivations as
    follows:

     MaxFrameNum = 2( log2_max_frame_num_minus4 + 4 ). The value of
    log2_max_frame_num_minus4 shall be in the range of 0 to 12, inclusive.
   */
  uint8_t log2_max_frame_num;

  /*
    pic_order_cnt_type specifies the method to decode picture order
    count (as specified in subclause 8.2.1). The value of
    pic_order_cnt_type shall be in the range of 0 to 2, inclusive.
   */
  uint8_t pic_order_cnt_type;

  /*
    log2_max_pic_order_cnt_lsb_minus4 specifies the value of the
    variable MaxPicOrderCntLsb that is used in the decoding
    process for picture order count as specified in subclause
    8.2.1 as follows:

    MaxPicOrderCntLsb = 2( log2_max_pic_order_cnt_lsb_minus4 + 4 )

    The value of log2_max_pic_order_cnt_lsb_minus4 shall be in
    the range of 0 to 12, inclusive.
   */
  uint8_t log2_max_pic_order_cnt_lsb;

  /*
    delta_pic_order_always_zero_flag equal to 1 specifies that
    delta_pic_order_cnt[ 0 ] and delta_pic_order_cnt[ 1 ] are
    not present in the slice headers of the sequence and shall
    be inferred to be equal to 0.
   */
  bool delta_pic_order_always_zero_flag;

  /*
    offset_for_non_ref_pic is used to calculate the picture
    order count of a non-reference picture as specified in
    8.2.1. The value of offset_for_non_ref_pic shall be in the
    range of -231 to 231 - 1, inclusive.
   */
  int8_t offset_for_non_ref_pic;

  /*
    offset_for_top_to_bottom_field is used to calculate the
    picture order count of a bottom field as specified in
    subclause 8.2.1. The value of offset_for_top_to_bottom_field
    shall be in the range of -231 to 231 - 1, inclusive.
   */
  int8_t offset_for_top_to_bottom_field;

  /*
    max_num_ref_frames specifies the maximum number of short-term and
    long-term reference frames, complementary reference field pairs,
    and non-paired reference fields that may be used by the decoding
    process for inter prediction of any picture in the
    sequence. max_num_ref_frames also determines the size of the sliding
    window operation as specified in subclause 8.2.5.3. The value of
    max_num_ref_frames shall be in the range of 0 to MaxDpbSize (as
    specified in subclause A.3.1 or A.3.2), inclusive.
   */
  uint32_t max_num_ref_frames;

  /*
    gaps_in_frame_num_value_allowed_flag specifies the allowed
    values of frame_num as specified in subclause 7.4.3 and the
    decoding process in case of an inferred gap between values of
    frame_num as specified in subclause 8.2.5.2.
   */
  bool gaps_in_frame_num_allowed_flag;

  /*
    pic_width_in_mbs_minus1 plus 1 specifies the width of each
    decoded picture in units of macroblocks.  16 macroblocks in a row
   */
  uint32_t pic_width_in_mbs;

  /*
    pic_height_in_map_units_minus1 plus 1 specifies the height in
    slice group map units of a decoded frame or field.  16
    macroblocks in each column.
   */
  uint32_t pic_height_in_map_units;

  /*
    frame_mbs_only_flag equal to 0 specifies that coded pictures of
    the coded video sequence may either be coded fields or coded
    frames. frame_mbs_only_flag equal to 1 specifies that every
    coded picture of the coded video sequence is a coded frame
    containing only frame macroblocks.
   */
  bool frame_mbs_only_flag;

  /*
    mb_adaptive_frame_field_flag equal to 0 specifies no
    switching between frame and field macroblocks within a
    picture. mb_adaptive_frame_field_flag equal to 1 specifies
    the possible use of switching between frame and field
    macroblocks within frames. When mb_adaptive_frame_field_flag
    is not present, it shall be inferred to be equal to 0.
   */
  bool mb_adaptive_frame_field_flag;

  /*
    frame_cropping_flag equal to 1 specifies that the frame cropping
    offset parameters follow next in the sequence parameter
    set. frame_cropping_flag equal to 0 specifies that the frame
    cropping offset parameters are not present.
   */
  bool frame_cropping_flag;
  uint32_t frame_crop_left_offset;;
  uint32_t frame_crop_right_offset;
  uint32_t frame_crop_top_offset;
  uint32_t frame_crop_bottom_offset;

  // VUI Parameters

  /*
    vui_parameters_present_flag equal to 1 specifies that the
    vui_parameters( ) syntax structure as specified in Annex E is
    present. vui_parameters_present_flag equal to 0 specifies that
    the vui_parameters( ) syntax structure as specified in Annex E
    is not present.
   */
  bool vui_parameters_present_flag;

  /*
   aspect_ratio_info_present_flag equal to 1 specifies that
   aspect_ratio_idc is present. aspect_ratio_info_present_flag
   equal to 0 specifies that aspect_ratio_idc is not present.
   */
  bool aspect_ratio_info_present_flag;

  /*
    aspect_ratio_idc specifies the value of the sample aspect
    ratio of the luma samples. Table E-1 shows the meaning of
    the code. When aspect_ratio_idc indicates Extended_SAR, the
    sample aspect ratio is represented by sar_width and
    sar_height. When the aspect_ratio_idc syntax element is not
    present, aspect_ratio_idc value shall be inferred to be
    equal to 0.
   */
  uint8_t aspect_ratio_idc;
  uint32_t sar_width;
  uint32_t sar_height;

  bool overscan_info_present_flag;
  bool overscan_appropriate_flag;

  uint8_t video_format;
  bool video_full_range_flag;
  bool colour_description_present_flag;
  uint8_t colour_primaries;
  uint8_t transfer_characteristics;
  uint8_t matrix_coefficients;
  bool chroma_loc_info_present_flag;
  uint32_t chroma_sample_loc_type_top_field;
  uint32_t chroma_sample_loc_type_bottom_field;
  uint32_t num_units_in_tick;
  uint32_t time_scale;
  bool fixed_frame_rate_flag;

  // Bitstream restriction parameters

  /*
    pic_struct_present_flag equal to 1 specifies that picture timing SEI
    messages (clause D.2.2) are present that include the pic_struct syntax
    element. pic_struct_present_flag equal to 0 specifies that the pic_struct
    syntax element is not present in picture timing SEI messages.
    When pic_struct_present_flag is not present, its value shall be inferred to
    be equal to 0.
   */
  bool pic_struct_present_flag;

  /*
    bitstream_restriction_flag equal to 1, specifies that the following coded
    video sequence bitstream restriction parameters are present.
    bitstream_restriction_flag equal to 0, specifies that the following coded
    video sequence bitstream restriction parameters are not present.
   */
  bool bitstream_restriction_flag;

  /*
    motion_vectors_over_pic_boundaries_flag equal to 0 indicates that no
    sample outside the picture boundaries and no sample at a fractional
    sample position for which the sample value is derived using one or more
    samples outside the picture boundaries is used for inter prediction of any
    sample. motion_vectors_over_pic_boundaries_flag equal to 1 indicates that
    one or more samples outside picture boundaries may be used in inter
    prediction. When the motion_vectors_over_pic_boundaries_flag syntax element
    is not present, motion_vectors_over_pic_boundaries_flag value shall be
    inferred to be equal to 1.
   */
  bool motion_vectors_over_pic_boundaries_flag;

  /*
    max_bytes_per_pic_denom indicates a number of bytes not exceeded by the
    sum of the sizes of the VCL NAL units associated with any coded picture in
    the coded video sequence.
   */
  uint32_t max_bytes_per_pic_denom;

  /*
    max_bits_per_mb_denom indicates an upper bound for the number of coded bits
    of macroblock_layer( ) data for any macroblock in any picture of the coded
    video sequence. The value of max_bits_per_mb_denom shall be in the range
    of 0 to 16, inclusive.
   */
  uint32_t max_bits_per_mb_denom;

  /*
    log2_max_mv_length_horizontal and log2_max_mv_length_vertical indicate the
    maximum absolute value of a decoded horizontal and vertical motion vector
    component, respectively, in 1⁄4 luma sample units, for all pictures in the
    coded video sequence. A value of n asserts that no value of a motion vector
    component shall exceed the range from −2n to 2n − 1, inclusive, in units
    of 1⁄4 luma sample displacement. The value of log2_max_mv_length_horizontal
    shall be in the range of 0 to 16, inclusive. The value of
    log2_max_mv_length_vertical shall be in the range of 0 to 16, inclusive.
    When log2_max_mv_length_horizontal is not present, the values of
    log2_max_mv_length_horizontal and log2_max_mv_length_vertical shall be
    inferred to be equal to 16.
   */
   uint32_t log2_max_mv_length_horizontal;
   uint32_t log2_max_mv_length_vertical;

  /*
    max_num_reorder_frames indicates an upper bound for the number of frames
    buffers, in the decoded picture buffer (DPB), that are required for storing
    frames, complementary field pairs, and non-paired fields before output.
    It is a requirement of bitstream conformance that the maximum number of
    frames, complementary field pairs, or non-paired fields that precede any
    frame, complementary field pair, or non-paired field in the coded video
    sequence in decoding order and follow it in output order shall be less than
    or equal to max_num_reorder_frames. The value of max_num_reorder_frames
    shall be in the range of 0 to max_dec_frame_buffering, inclusive.
    When the max_num_reorder_frames syntax element is not present, the value
    of max_num_reorder_frames value shall be inferred as follows:
     – If profile_idc is equal to 44, 86, 100, 110, 122, or 244 and
       constraint_set3_flag is equal to 1, the value of max_num_reorder_frames
       shall be inferred to be equal to 0.
     – Otherwise (profile_idc is not equal to 44, 86, 100, 110, 122, or 244 or
       constraint_set3_flag is equal to 0), the value of max_num_reorder_frames
       shall be inferred to be equal to MaxDpbFrames.
   */
   uint32_t max_num_reorder_frames;

  /*
    max_dec_frame_buffering specifies the required size of the HRD decoded
    picture buffer (DPB) in units of frame buffers. It is a requirement of
    bitstream conformance that the coded video sequence shall not require a
    decoded picture buffer with size of more than
    Max( 1, max_dec_frame_buffering ) frame buffers to enable the output of
    decoded pictures at the output times specified by dpb_output_delay of the
    picture timing SEI messages. The value of max_dec_frame_buffering shall be
    greater than or equal to max_num_ref_frames. An upper bound for the value
    of max_dec_frame_buffering is specified by the level limits in
    clauses A.3.1, A.3.2, G.10.2.1, and H.10.2.
   */
   uint32_t max_dec_frame_buffering;

  SPSData();
};

class H264
{
public:
  static bool DecodeSPSFromExtraData(const ByteBuffer* aExtraData, SPSData& aDest);
  /* Extract RAW BYTE SEQUENCE PAYLOAD from NAL content.
     Returns nullptr if invalid content. */
  static already_AddRefed<ByteBuffer> DecodeNALUnit(const ByteBuffer* aNAL);
  /* Decode SPS NAL RBSP and fill SPSData structure */
  static bool DecodeSPS(const ByteBuffer* aSPS, SPSData& aDest);

private:
  static void vui_parameters(BitReader& aBr, SPSData& aDest);
  // Read HRD parameters, all data is ignored.
  static void hrd_parameters(BitReader& aBr);
};

} // namespace mp4_demuxer

#endif // MP4_DEMUXER_H264_H_
