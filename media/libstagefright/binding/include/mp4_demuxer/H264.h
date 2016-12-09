/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_H264_H_
#define MP4_DEMUXER_H264_H_

#include "mp4_demuxer/DecoderData.h"

namespace mp4_demuxer {

// Spec 7.4.2.1
#define MAX_SPS_COUNT 32
#define MAX_PPS_COUNT 256

// NAL unit types
enum NAL_TYPES {
    H264_NAL_SLICE           = 1,
    H264_NAL_DPA             = 2,
    H264_NAL_DPB             = 3,
    H264_NAL_DPC             = 4,
    H264_NAL_IDR_SLICE       = 5,
    H264_NAL_SEI             = 6,
    H264_NAL_SPS             = 7,
    H264_NAL_PPS             = 8,
    H264_NAL_AUD             = 9,
    H264_NAL_END_SEQUENCE    = 10,
    H264_NAL_END_STREAM      = 11,
    H264_NAL_FILLER_DATA     = 12,
    H264_NAL_SPS_EXT         = 13,
    H264_NAL_PREFIX          = 14,
    H264_NAL_AUXILIARY_SLICE = 19,
    H264_NAL_SLICE_EXT       = 20,
    H264_NAL_SLICE_EXT_DVC   = 21,
};

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
   Displayed size.
   display_width and display_height are adjusted according to the display
   sample aspect ratio.
   */
  uint32_t display_width;
  uint32_t display_height;

  float sample_ratio;

  uint32_t crop_left;
  uint32_t crop_right;
  uint32_t crop_top;
  uint32_t crop_bottom;

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
    chroma_format_idc specifies the chroma sampling relative to the luma
    sampling as specified in clause 6.2. The value of chroma_format_idc shall be
    in the range of 0 to 3, inclusive. When chroma_format_idc is not present,
    it shall be inferred to be equal to 1 (4:2:0 chroma format).
    When profile_idc is equal to 183, chroma_format_idc shall be equal to 0
    (4:0:0 chroma format).
   */
  uint8_t chroma_format_idc;

  /*
    bit_depth_luma_minus8 specifies the bit depth of the samples of the luma
    array and the value of the luma quantisation parameter range offset
    QpBdOffset Y , as specified by
      BitDepth Y = 8 + bit_depth_luma_minus8 (7-3)
      QpBdOffset Y = 6 * bit_depth_luma_minus8 (7-4)
    When bit_depth_luma_minus8 is not present, it shall be inferred to be equal
    to 0. bit_depth_luma_minus8 shall be in the range of 0 to 6, inclusive.
  */
  uint8_t bit_depth_luma_minus8;

  /*
    bit_depth_chroma_minus8 specifies the bit depth of the samples of the chroma
    arrays and the value of the chroma quantisation parameter range offset
    QpBdOffset C , as specified by
      BitDepth C = 8 + bit_depth_chroma_minus8 (7-5)
      QpBdOffset C = 6 * bit_depth_chroma_minus8 (7-6)
    When bit_depth_chroma_minus8 is not present, it shall be inferred to be
    equal to 0. bit_depth_chroma_minus8 shall be in the range of 0 to 6, inclusive.
  */
  uint8_t bit_depth_chroma_minus8;

  /*
    separate_colour_plane_flag equal to 1 specifies that the three colour
    components of the 4:4:4 chroma format are coded separately.
    separate_colour_plane_flag equal to 0 specifies that the colour components
    are not coded separately. When separate_colour_plane_flag is not present,
    it shall be inferred to be equal to 0. When separate_colour_plane_flag is
    equal to 1, the primary coded picture consists of three separate components,
    each of which consists of coded samples of one colour plane (Y, Cb or Cr)
    that each use the monochrome coding syntax. In this case, each colour plane
    is associated with a specific colour_plane_id value.
   */
  bool separate_colour_plane_flag;

/*
   seq_scaling_matrix_present_flag equal to 1 specifies that the flags
   seq_scaling_list_present_flag[ i ] for i = 0..7 or
   i = 0..11 are present. seq_scaling_matrix_present_flag equal to 0 specifies
   that these flags are not present and the sequence-level scaling list
   specified by Flat_4x4_16 shall be inferred for i = 0..5 and the
   sequence-level scaling list specified by Flat_8x8_16 shall be inferred for
   i = 6..11. When seq_scaling_matrix_present_flag is not present, it shall be
   inferred to be equal to 0.
   */
  bool seq_scaling_matrix_present_flag;

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
    direct_8x8_inference_flag specifies the method used in the derivation
    process for luma motion vectors for B_Skip, B_Direct_16x16 and B_Direct_8x8
    as specified in clause 8.4.1.2. When frame_mbs_only_flag is equal to 0,
    direct_8x8_inference_flag shall be equal to 1.
  */
  bool direct_8x8_inference_flag;

  /*
    frame_cropping_flag equal to 1 specifies that the frame cropping
    offset parameters follow next in the sequence parameter
    set. frame_cropping_flag equal to 0 specifies that the frame
    cropping offset parameters are not present.
   */
  bool frame_cropping_flag;
  uint32_t frame_crop_left_offset;
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

  /*
    video_signal_type_present_flag equal to 1 specifies that video_format,
    video_full_range_flag and colour_description_present_flag are present.
    video_signal_type_present_flag equal to 0, specify that video_format,
    video_full_range_flag and colour_description_present_flag are not present.
   */
  bool video_signal_type_present_flag;

  /*
    overscan_info_present_flag equal to1 specifies that the
    overscan_appropriate_flag is present. When overscan_info_present_flag is
    equal to 0 or is not present, the preferred display method for the video
    signal is unspecified (Unspecified).
   */
  bool overscan_info_present_flag;
  /*
    overscan_appropriate_flag equal to 1 indicates that the cropped decoded
    pictures output are suitable for display using overscan.
    overscan_appropriate_flag equal to 0 indicates that the cropped decoded
    pictures output contain visually important information in the entire region
    out to the edges of the cropping rectangle of the picture
   */
  bool overscan_appropriate_flag;

  /*
    video_format indicates the representation of the pictures as specified in
    Table E-2, before being coded in accordance with this
    Recommendation | International Standard. When the video_format syntax element
    is not present, video_format value shall be inferred to be equal to 5.
    (Unspecified video format)
   */
  uint8_t video_format;

  /*
    video_full_range_flag indicates the black level and range of the luma and
    chroma signals as derived from E′Y, E′PB, and E′PR or E′R, E′G, and E′B
    real-valued component signals.
    When the video_full_range_flag syntax element is not present, the value of
    video_full_range_flag shall be inferred to be equal to 0.
   */
  bool video_full_range_flag;

  /*
    colour_description_present_flag equal to1 specifies that colour_primaries,
    transfer_characteristics and matrix_coefficients are present.
    colour_description_present_flag equal to 0 specifies that colour_primaries,
    transfer_characteristics and matrix_coefficients are not present.
   */
  bool colour_description_present_flag;

  /*
    colour_primaries indicates the chromaticity coordinates of the source
    primaries as specified in Table E-3 in terms of the CIE 1931 definition of
    x and y as specified by ISO 11664-1.
    When the colour_primaries syntax element is not present, the value of
    colour_primaries shall be inferred to be equal to 2 (the chromaticity is
    unspecified or is determined by the application).
   */
  uint8_t colour_primaries;

  /*
    transfer_characteristics indicates the opto-electronic transfer
    characteristic of the source picture as specified in Table E-4 as a function
    of a linear optical intensity input Lc with a nominal real-valued range of 0
    to 1.
    When the transfer_characteristics syntax element is not present, the value
    of transfer_characteristics shall be inferred to be equal to 2
    (the transfer characteristics are unspecified or are determined by the
    application).
   */
  uint8_t transfer_characteristics;

  uint8_t matrix_coefficients;
  bool chroma_loc_info_present_flag;
  uint32_t chroma_sample_loc_type_top_field;
  uint32_t chroma_sample_loc_type_bottom_field;
  bool timing_info_present_flag;
  uint32_t num_units_in_tick;
  uint32_t time_scale;
  bool fixed_frame_rate_flag;

  bool scaling_matrix_present;
  uint8_t scaling_matrix4x4[6][16];
  uint8_t scaling_matrix8x8[6][64];

  SPSData();
};

struct PPSData
{
  /*
    H264 decoding parameters according to ITU-T H.264 (T-REC-H.264-201402-I/en)
   http://www.itu.int/rec/T-REC-H.264-201402-I/en
   7.3.2.2 Picture parameter set RBSP syntax
  */
  /* pic_parameter_set_id identifies the picture parameter set that is referred
      to in the slice header. The value of pic_parameter_set_id shall be in the
      range of 0 to 255, inclusive. */
  uint8_t pic_parameter_set_id;

  /* seq_parameter_set_id refers to the active sequence parameter set. The value
    of seq_parameter_set_id shall be in the range of 0 to 31, inclusive. */
  uint8_t seq_parameter_set_id;

  /* entropy_coding_mode_flag selects the entropy decoding method to be applied
    for the syntax elements for which two descriptors appear in the syntax tables
    as follows:
    – If entropy_coding_mode_flag is equal to 0, the method specified by the
      left descriptor in the syntax table is applied (Exp-Golomb coded, see
      clause 9.1 or CAVLC, see clause 9.2).
    – Otherwise (entropy_coding_mode_flag is equal to 1), the method specified
      by the right descriptor in the syntax table is applied (CABAC, see clause
      9.3) */
  bool entropy_coding_mode_flag;

  /* bottom_field_pic_order_in_frame_present_flag equal to 1 specifies that the
    syntax  elements delta_pic_order_cnt_bottom (when pic_order_cnt_type is
    equal to 0) or delta_pic_order_cnt[ 1 ] (when pic_order_cnt_type is equal to
    1), which are related to picture order counts for the bottom field of a
    coded frame, are present in the slice headers for coded frames as specified
    in clause 7.3.3. bottom_field_pic_order_in_frame_present_flag equal to 0
    specifies that the syntax elements delta_pic_order_cnt_bottom and
    delta_pic_order_cnt[ 1 ] are not present in the slice headers.
    Also known has pic_order_present_flag. */
  bool bottom_field_pic_order_in_frame_present_flag;

  /* num_slice_groups_minus1 plus 1 specifies the number of slice groups for a
    picture. When num_slice_groups_minus1 is equal to 0, all slices of the
    picture belong to the same slice group. The allowed range of
    num_slice_groups_minus1 is specified in Annex A. */
  uint8_t num_slice_groups_minus1;

  /* slice_group_map_type specifies how the mapping of slice group map units to
    slice groups is coded. The value of slice_group_map_type shall be in the
    range of 0 to 6, inclusive. */
  uint8_t slice_group_map_type;

  /* run_length_minus1[i] is used to specify the number of consecutive slice
    group map units to be assigned to the i-th slice group in raster scan order
    of slice group map units. The value of run_length_minus1[ i ] shall be in
    the range of 0 to PicSizeInMapUnits − 1, inclusive. */
  uint32_t run_length_minus1[8];

  /* top_left[i] and bottom_right[i] specify the top-left and bottom-right
    corners of a rectangle, respectively. top_left[i] and bottom_right[i] are
    slice group map unit positions in a raster scan of the picture for the slice
    group map units. For each rectangle i, all of the following constraints
    shall be obeyed by the values of the syntax elements top_left[i] and
    bottom_right[i]:
      – top_left[ i ] shall be less than or equal to bottom_right[i] and
        bottom_right[i] shall be less than PicSizeInMapUnits.
      – (top_left[i] % PicWidthInMbs) shall be less than or equal to the value
        of (bottom_right[i] % PicWidthInMbs). */
  uint32_t top_left[8];
  uint32_t bottom_right[8];

  /* slice_group_change_direction_flag is used with slice_group_map_type to
    specify the refined map type when slice_group_map_type is 3, 4, or 5. */
  bool slice_group_change_direction_flag;

  /* slice_group_change_rate_minus1 is used to specify the variable
    SliceGroupChangeRate. SliceGroupChangeRate specifies the multiple in number
    of slice group map units by which the size of a slice group can change from
    one picture to the next. The value of slice_group_change_rate_minus1 shall
    be in the range of 0 to PicSizeInMapUnits − 1, inclusive.
    The SliceGroupChangeRate variable is specified as follows:
      SliceGroupChangeRate = slice_group_change_rate_minus1 + 1 */
  uint32_t slice_group_change_rate_minus1;

  /* pic_size_in_map_units_minus1 is used to specify the number of slice group
    map units in the picture. pic_size_in_map_units_minus1 shall be equal to
    PicSizeInMapUnits − 1. */
  uint32_t pic_size_in_map_units_minus1;

  /* num_ref_idx_l0_default_active_minus1 specifies how
    num_ref_idx_l0_active_minus1 is inferred for P, SP, and B slices
    with num_ref_idx_active_override_flag equal to 0. The value of
    num_ref_idx_l0_default_active_minus1 shall be in the
    range of 0 to 31, inclusive. */
  uint8_t num_ref_idx_l0_default_active_minus1;

  /* num_ref_idx_l1_default_active_minus1 specifies how
    num_ref_idx_l1_active_minus1 is inferred for B slices with
    num_ref_idx_active_override_flag equal to 0. The value of
    num_ref_idx_l1_default_active_minus1 shall be in the range
    of 0 to 31, inclusive. */
  uint8_t num_ref_idx_l1_default_active_minus1;

  /* weighted_pred_flag equal to 0 specifies that the default weighted
    prediction shall be applied to P and SP slices.
    weighted_pred_flag equal to 1 specifies that explicit weighted prediction
    shall be applied to P and SP slices.weighted_pred_flag 1 */
  bool weighted_pred_flag;

  /* weighted_bipred_idc equal to 0 specifies that the default weighted
     prediction shall be applied to B slices.
     weighted_bipred_idc equal to 1 specifies that explicit weighted prediction
     shall be applied to B slices. weighted_bipred_idc equal to 2 specifies that
     implicit weighted prediction shall be applied to B slices. The value of
     weighted_bipred_idc shall be in the range of 0 to 2, inclusive. */
  uint8_t weighted_bipred_idc;

  /* pic_init_qp_minus26 specifies the initial value minus 26 of SliceQP Y for
     each slice. The initial value is modified at the slice layer when a
     non-zero value of slice_qp_delta is decoded, and is modified further when a
     non-zero value of mb_qp_delta is decoded at the macroblock layer.
     The value of pic_init_qp_minus26 shall be in the range of
     −(26 + QpBdOffset Y ) to +25, inclusive. */
  int8_t pic_init_qp_minus26;

  /* pic_init_qs_minus26 specifies the initial value minus 26 of SliceQS Y for
    all macroblocks in SP or SI slices. The initial value is modified at the
    slice layer when a non-zero value of slice_qs_delta is decoded.
    The value of pic_init_qs_minus26 shall be in the range of −26 to +25,
    inclusive. */
  int8_t pic_init_qs_minus26;

  /* chroma_qp_index_offset specifies the offset that shall be added to QP Y and
    QS Y for addressing the table of QP C values for the Cb chroma component.
    The value of chroma_qp_index_offset shall be in the range of −12 to +12,
    inclusive. */
  int8_t chroma_qp_index_offset;

  /* deblocking_filter_control_present_flag equal to 1 specifies that a set of
    syntax elements controlling the characteristics of the deblocking filter is
    present in the slice header. deblocking_filter_control_present_flag equal to
    0 specifies that the set of syntax elements controlling the characteristics
    of the deblocking filter is not present in the slice headers and their
    inferred values are in effect. */
  bool deblocking_filter_control_present_flag;

  /* constrained_intra_pred_flag equal to 0 specifies that intra prediction
    allows usage of residual data and decoded samples of neighbouring
    macroblocks coded using Inter macroblock prediction modes for the prediction
    of macroblocks coded using Intra macroblock prediction modes.
    constrained_intra_pred_flag equal to 1 specifies constrained intra
    prediction, in which case prediction of macroblocks coded using Intra
    macroblock prediction modes only uses residual data and decoded samples from
    I or SI macroblock types. */
  bool constrained_intra_pred_flag;

  /* redundant_pic_cnt_present_flag equal to 0 specifies that the
    redundant_pic_cnt syntax element is not present in slice headers, coded
    slice data partition B NAL units, and coded slice data partition C NAL units
    that refer (either directly or by association with a corresponding coded
    slice data partition A NAL unit) to the picture parameter set.
    redundant_pic_cnt_present_flag equal to 1 specifies that the
    redundant_pic_cnt syntax element is present in all slice headers, coded
    slice data partition B NAL units, and coded slice data partition C NAL units
    that refer (either directly or by association with a corresponding coded
    slice data partition A NAL unit) to the picture parameter set. */
  bool redundant_pic_cnt_present_flag;

  /* transform_8x8_mode_flag equal to 1 specifies that the 8x8 transform
    decoding process may be in use (see clause 8.5).
    transform_8x8_mode_flag equal to 0 specifies that the 8x8 transform decoding
    process is not in use. When transform_8x8_mode_flag is not present, it shall
    be inferred to be 0. */
  bool transform_8x8_mode_flag;

  /* second_chroma_qp_index_offset specifies the offset that shall be added to
    QP Y and QS Y for addressing the table of QP C values for the Cr chroma
    component.
    The value of second_chroma_qp_index_offset shall be in the range of
    −12 to +12, inclusive.
    When second_chroma_qp_index_offset is not present, it shall be inferred to
    be equal to chroma_qp_index_offset. */
  int8_t second_chroma_qp_index_offset;

  uint8_t scaling_matrix4x4[6][16];
  uint8_t scaling_matrix8x8[6][64];

  PPSData();
};

typedef AutoTArray<SPSData, MAX_SPS_COUNT> SPSDataSet;
typedef AutoTArray<PPSData, MAX_PPS_COUNT> PPSDataSet;

struct H264ParametersSet
{
  SPSDataSet SPSes;
  PPSDataSet PPSes;
};

class H264
{
public:
  /* Extract RAW BYTE SEQUENCE PAYLOAD from NAL content.
     Returns nullptr if invalid content.
     This is compliant to ITU H.264 7.3.1 Syntax in tabular form NAL unit syntax
   */
  static already_AddRefed<mozilla::MediaByteBuffer> DecodeNALUnit(
    const mozilla::MediaByteBuffer* aNAL);

  // Ensure that SPS data makes sense, Return true if SPS data was, and false
  // otherwise. If false, then content will be adjusted accordingly.
  static bool EnsureSPSIsSane(SPSData& aSPS);

  static bool DecodeSPSFromExtraData(const mozilla::MediaByteBuffer* aExtraData,
                                     SPSData& aDest);

  static bool DecodeParametersSet(const mozilla::MediaByteBuffer* aExtraData,
                                  H264ParametersSet& aDest);

  // If the given aExtraData is valid, return the aExtraData.max_num_ref_frames
  // clamped to be in the range of [4, 16]; otherwise return 4.
  static uint32_t ComputeMaxRefFrames(
    const mozilla::MediaByteBuffer* aExtraData);

  enum class FrameType
  {
    I_FRAME,
    OTHER,
    INVALID,
  };

  // Returns the frame type. Returns I_FRAME if the sample is an IDR
  // (Instantaneous Decoding Refresh) Picture.
  static FrameType GetFrameType(const mozilla::MediaRawData* aSample);

  // ZigZag index taBles.
  // Some hardware requires the tables to be in zigzag order.
  // Use ZZ_SCAN table for the scaling_matrix4x4.
  // Use ZZ_SCAN8 table for the scaling_matrix8x8.
  // e.g. dest_scaling_matrix4x4[i,j] = scaling_matrix4x4[ZZ_SCAN(i,j)]
  static const uint8_t ZZ_SCAN[16];
  static const uint8_t ZZ_SCAN8[64];

private:
  static bool DecodeSPSDataSetFromExtraData(const mozilla::MediaByteBuffer* aExtraData,
                                            SPSDataSet& aDest);

  static bool DecodePPSDataSetFromExtraData(const mozilla::MediaByteBuffer* aExtraData,
                                            const SPSDataSet& aPS,
                                            PPSDataSet& aDest);

  /* Decode SPS NAL RBSP and fill SPSData structure */
  static bool DecodeSPS(const mozilla::MediaByteBuffer* aSPS, SPSData& aDest);
  /* Decode PPS NAL RBSP and fill PPSData structure */
  static bool DecodePPS(const mozilla::MediaByteBuffer* aPPS,
                        const SPSDataSet& aSPSs, PPSData& aDest);
  static void vui_parameters(BitReader& aBr, SPSData& aDest);
  // Read HRD parameters, all data is ignored.
  static void hrd_parameters(BitReader& aBr);
};

} // namespace mp4_demuxer

#endif // MP4_DEMUXER_H264_H_
