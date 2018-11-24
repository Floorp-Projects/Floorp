/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DAV1D_SRC_LEVELS_H__
#define __DAV1D_SRC_LEVELS_H__

#include "dav1d/picture.h"

enum ObuType {
    OBU_SEQ_HDR   = 1,
    OBU_TD        = 2,
    OBU_FRAME_HDR = 3,
    OBU_TILE_GRP  = 4,
    OBU_METADATA  = 5,
    OBU_FRAME     = 6,
    OBU_REDUNDANT_FRAME_HDR = 7,
    OBU_PADDING   = 15,
};

enum TxfmSize {
    TX_4X4,
    TX_8X8,
    TX_16X16,
    TX_32X32,
    TX_64X64,
    N_TX_SIZES,
};

enum BlockLevel {
    BL_128X128,
    BL_64X64,
    BL_32X32,
    BL_16X16,
    BL_8X8,
    N_BL_LEVELS,
};

enum TxfmMode {
    TX_4X4_ONLY,
    TX_LARGEST,
    TX_SWITCHABLE,
    N_TX_MODES,
};

enum RectTxfmSize {
    RTX_4X8 = N_TX_SIZES,
    RTX_8X4,
    RTX_8X16,
    RTX_16X8,
    RTX_16X32,
    RTX_32X16,
    RTX_32X64,
    RTX_64X32,
    RTX_4X16,
    RTX_16X4,
    RTX_8X32,
    RTX_32X8,
    RTX_16X64,
    RTX_64X16,
    N_RECT_TX_SIZES
};

enum TxfmType {
    DCT_DCT,    // DCT  in both horizontal and vertical
    ADST_DCT,   // ADST in vertical, DCT in horizontal
    DCT_ADST,   // DCT  in vertical, ADST in horizontal
    ADST_ADST,  // ADST in both directions
    FLIPADST_DCT,
    DCT_FLIPADST,
    FLIPADST_FLIPADST,
    ADST_FLIPADST,
    FLIPADST_ADST,
    IDTX,
    V_DCT,
    H_DCT,
    V_ADST,
    H_ADST,
    V_FLIPADST,
    H_FLIPADST,
    N_TX_TYPES,
    WHT_WHT = N_TX_TYPES,
    N_TX_TYPES_PLUS_LL,
};

enum TxfmTypeSet {
    TXTP_SET_DCT,
    TXTP_SET_DCT_ID,
    TXTP_SET_DT4_ID,
    TXTP_SET_DT4_ID_1D,
    TXTP_SET_DT9_ID_1D,
    TXTP_SET_ALL,
    TXTP_SET_LOSSLESS,
    N_TXTP_SETS
};

enum TxClass {
    TX_CLASS_2D,
    TX_CLASS_H,
    TX_CLASS_V,
};

enum IntraPredMode {
    DC_PRED,
    VERT_PRED,
    HOR_PRED,
    DIAG_DOWN_LEFT_PRED,
    DIAG_DOWN_RIGHT_PRED,
    VERT_RIGHT_PRED,
    HOR_DOWN_PRED,
    HOR_UP_PRED,
    VERT_LEFT_PRED,
    SMOOTH_PRED,
    SMOOTH_V_PRED,
    SMOOTH_H_PRED,
    PAETH_PRED,
    N_INTRA_PRED_MODES,
    CFL_PRED = N_INTRA_PRED_MODES,
    N_UV_INTRA_PRED_MODES,
    N_IMPL_INTRA_PRED_MODES = N_UV_INTRA_PRED_MODES,
    LEFT_DC_PRED = DIAG_DOWN_LEFT_PRED,
    TOP_DC_PRED,
    DC_128_PRED,
    Z1_PRED,
    Z2_PRED,
    Z3_PRED,
    FILTER_PRED = N_INTRA_PRED_MODES,
};

enum InterIntraPredMode {
    II_DC_PRED,
    II_VERT_PRED,
    II_HOR_PRED,
    II_SMOOTH_PRED,
    N_INTER_INTRA_PRED_MODES,
};

enum BlockPartition {
    PARTITION_NONE,     // [ ] <-.
    PARTITION_H,        // [-]   |
    PARTITION_V,        // [|]   |
    PARTITION_SPLIT,    // [+] --'
    PARTITION_T_TOP_SPLIT,    // [⊥] i.e. split top, H bottom
    PARTITION_T_BOTTOM_SPLIT, // [т] i.e. H top, split bottom
    PARTITION_T_LEFT_SPLIT,   // [-|] i.e. split left, V right
    PARTITION_T_RIGHT_SPLIT,  // [|-] i.e. V left, split right
    PARTITION_H4,       // [Ⲷ]
    PARTITION_V4,       // [Ⲽ]
    N_PARTITIONS,
    N_SUB8X8_PARTITIONS = PARTITION_T_TOP_SPLIT,
};

enum BlockSize {
    BS_128x128,
    BS_128x64,
    BS_64x128,
    BS_64x64,
    BS_64x32,
    BS_64x16,
    BS_32x64,
    BS_32x32,
    BS_32x16,
    BS_32x8,
    BS_16x64,
    BS_16x32,
    BS_16x16,
    BS_16x8,
    BS_16x4,
    BS_8x32,
    BS_8x16,
    BS_8x8,
    BS_8x4,
    BS_4x16,
    BS_4x8,
    BS_4x4,
    N_BS_SIZES,
};

enum FilterMode {
    FILTER_8TAP_REGULAR,
    FILTER_8TAP_SMOOTH,
    FILTER_8TAP_SHARP,
    N_SWITCHABLE_FILTERS,
    FILTER_BILINEAR = N_SWITCHABLE_FILTERS,
    N_FILTERS,
    FILTER_SWITCHABLE = N_FILTERS,
};

enum Filter2d { // order is horizontal, vertical
    FILTER_2D_8TAP_REGULAR,
    FILTER_2D_8TAP_REGULAR_SMOOTH,
    FILTER_2D_8TAP_REGULAR_SHARP,
    FILTER_2D_8TAP_SHARP_REGULAR,
    FILTER_2D_8TAP_SHARP_SMOOTH,
    FILTER_2D_8TAP_SHARP,
    FILTER_2D_8TAP_SMOOTH_REGULAR,
    FILTER_2D_8TAP_SMOOTH,
    FILTER_2D_8TAP_SMOOTH_SHARP,
    FILTER_2D_BILINEAR,
    N_2D_FILTERS,
};

enum MVJoint {
    MV_JOINT_ZERO,
    MV_JOINT_H,
    MV_JOINT_V,
    MV_JOINT_HV,
    N_MV_JOINTS,
};

enum InterPredMode {
    NEARESTMV,
    NEARMV,
    GLOBALMV,
    NEWMV,
    N_INTER_PRED_MODES,
};

enum CompInterPredMode {
    NEARESTMV_NEARESTMV,
    NEARMV_NEARMV,
    NEARESTMV_NEWMV,
    NEWMV_NEARESTMV,
    NEARMV_NEWMV,
    NEWMV_NEARMV,
    GLOBALMV_GLOBALMV,
    NEWMV_NEWMV,
    N_COMP_INTER_PRED_MODES,
};

enum CompInterType {
    COMP_INTER_NONE,
    COMP_INTER_WEIGHTED_AVG,
    COMP_INTER_AVG,
    COMP_INTER_SEG,
    COMP_INTER_WEDGE,
};

enum InterIntraType {
    INTER_INTRA_NONE,
    INTER_INTRA_BLEND,
    INTER_INTRA_WEDGE,
};

enum AdaptiveBoolean {
    OFF = 0,
    ON = 1,
    ADAPTIVE = 2,
};

enum RestorationType {
    RESTORATION_NONE,
    RESTORATION_SWITCHABLE,
    RESTORATION_WIENER,
    RESTORATION_SGRPROJ,
};

typedef struct mv {
    int16_t y, x;
} mv;

enum WarpedMotionType {
    WM_TYPE_IDENTITY,
    WM_TYPE_TRANSLATION,
    WM_TYPE_ROT_ZOOM,
    WM_TYPE_AFFINE,
};

typedef struct WarpedMotionParams {
    enum WarpedMotionType type;
    int32_t matrix[6];
    union {
        struct {
            int16_t alpha, beta, gamma, delta;
        };
        int16_t abcd[4];
    };
} WarpedMotionParams;

enum MotionMode {
    MM_TRANSLATION,
    MM_OBMC,
    MM_WARP,
};

typedef struct Av1SequenceHeader {
    int profile;
    int still_picture;
    int reduced_still_picture_header;
    int timing_info_present;
    int num_units_in_tick;
    int time_scale;
    int equal_picture_interval;
    int num_ticks_per_picture;
    int decoder_model_info_present;
    int encoder_decoder_buffer_delay_length;
    int num_units_in_decoding_tick;
    int buffer_removal_delay_length;
    int frame_presentation_delay_length;
    int display_model_info_present;
    int num_operating_points;
    struct Av1SequenceHeaderOperatingPoint {
        int idc;
        int major_level, minor_level;
        int tier;
        int decoder_model_param_present;
        int decoder_buffer_delay;
        int encoder_buffer_delay;
        int low_delay_mode;
        int display_model_param_present;
        int initial_display_delay;
    } operating_points[32];
    int max_width, max_height, width_n_bits, height_n_bits;
    int frame_id_numbers_present;
    int delta_frame_id_n_bits;
    int frame_id_n_bits;
    int sb128;
    int filter_intra;
    int intra_edge_filter;
    int inter_intra;
    int masked_compound;
    int warped_motion;
    int dual_filter;
    int order_hint;
    int jnt_comp;
    int ref_frame_mvs;
    enum AdaptiveBoolean screen_content_tools;
    enum AdaptiveBoolean force_integer_mv;
    int order_hint_n_bits;
    int super_res;
    int cdef;
    int restoration;
    int bpc;
    int hbd;
    int color_description_present;
    enum Dav1dPixelLayout layout;
    enum Dav1dColorPrimaries pri;
    enum Dav1dTransferCharacteristics trc;
    enum Dav1dMatrixCoefficients mtrx;
    enum Dav1dChromaSamplePosition chr;
    int color_range;
    int separate_uv_delta_q;
    int film_grain_present;
} Av1SequenceHeader;

#define NUM_SEGMENTS 8

typedef struct Av1SegmentationData {
    int delta_q;
    int delta_lf_y_v, delta_lf_y_h, delta_lf_u, delta_lf_v;
    int ref;
    int skip;
    int globalmv;
} Av1SegmentationData;

typedef struct Av1SegmentationDataSet {
    Av1SegmentationData d[NUM_SEGMENTS];
    int preskip;
    int last_active_segid;
} Av1SegmentationDataSet;

typedef struct Av1LoopfilterModeRefDeltas {
    int mode_delta[2];
    int ref_delta[8];
} Av1LoopfilterModeRefDeltas;

typedef struct Av1FilmGrainData {
    int num_y_points;
    uint8_t y_points[14][2 /* value, scaling */];
    int chroma_scaling_from_luma;
    int num_uv_points[2];
    uint8_t uv_points[2][10][2 /* value, scaling */];
    int scaling_shift;
    int ar_coeff_lag;
    int8_t ar_coeffs_y[24];
    int8_t ar_coeffs_uv[2][25];
    int ar_coeff_shift;
    int grain_scale_shift;
    int uv_mult[2];
    int uv_luma_mult[2];
    int uv_offset[2];
    int overlap_flag;
    int clip_to_restricted_range;
} Av1FilmGrainData;

typedef struct Av1FrameHeader {
    int show_existing_frame;
    int existing_frame_idx;
    int frame_id;
    int frame_presentation_delay;
    enum Dav1dFrameType frame_type;
    int show_frame;
    int showable_frame;
    int error_resilient_mode;
    int disable_cdf_update;
    int allow_screen_content_tools;
    int force_integer_mv;
    int frame_size_override;
#define PRIMARY_REF_NONE 7
    int primary_ref_frame;
    int buffer_removal_time_present;
    struct Av1FrameHeaderOperatingPoint {
        int buffer_removal_time;
    } operating_points[32];
    int frame_offset;
    int refresh_frame_flags;
    int width, height;
    int render_width, render_height;
    int super_res;
    int have_render_size;
    int allow_intrabc;
    int frame_ref_short_signaling;
    int refidx[7];
    int hp;
    enum FilterMode subpel_filter_mode;
    int switchable_motion_mode;
    int use_ref_frame_mvs;
    int refresh_context;
    struct {
        int uniform;
        unsigned n_bytes;
        int min_log2_cols, max_log2_cols, log2_cols, cols;
        int col_start_sb[1025];
        int min_log2_rows, max_log2_rows, log2_rows, rows;
        int row_start_sb[1025];
        int update;
    } tiling;
    struct {
        int yac;
        int ydc_delta;
        int udc_delta, uac_delta, vdc_delta, vac_delta;
        int qm, qm_y, qm_u, qm_v;
    } quant;
    struct {
        int enabled, update_map, temporal, update_data;
        Av1SegmentationDataSet seg_data;
        int lossless[NUM_SEGMENTS], qidx[NUM_SEGMENTS];
    } segmentation;
    struct {
        struct {
            int present;
            int res_log2;
        } q;
        struct {
            int present;
            int res_log2;
            int multi;
        } lf;
    } delta;
    int all_lossless;
    struct {
        int level_y[2];
        int level_u, level_v;
        int mode_ref_delta_enabled;
        int mode_ref_delta_update;
        Av1LoopfilterModeRefDeltas mode_ref_deltas;
        int sharpness;
    } loopfilter;
    struct {
        int damping;
        int n_bits;
        int y_strength[8];
        int uv_strength[8];
    } cdef;
    struct {
        enum RestorationType type[3];
        int unit_size[2];
    } restoration;
    enum TxfmMode txfm_mode;
    int switchable_comp_refs;
    int skip_mode_allowed, skip_mode_enabled, skip_mode_refs[2];
    int warp_motion;
    int reduced_txtp_set;
    WarpedMotionParams gmv[7];
    struct {
        int present, update, seed;
        Av1FilmGrainData data;
    } film_grain;
} Av1FrameHeader;

#define QINDEX_RANGE 256

typedef struct Av1Block {
    uint8_t bl, bs, bp;
    uint8_t intra, seg_id, skip_mode, skip, uvtx;
    union {
        struct {
            uint8_t y_mode, uv_mode, tx, pal_sz[2];
            int8_t y_angle, uv_angle, cfl_alpha[2];
        }; // intra
        struct {
            int8_t ref[2];
            uint8_t comp_type, wedge_idx, mask_sign, inter_mode, drl_idx;
            uint8_t interintra_type, interintra_mode, motion_mode;
            uint8_t max_ytx, filter2d;
            uint16_t tx_split[2];
            mv mv[2];
        }; // inter
    };
} Av1Block;

#endif /* __DAV1D_SRC_LEVELS_H__ */
