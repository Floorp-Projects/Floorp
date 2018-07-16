/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_ENUMS_H_
#define AV1_COMMON_ENUMS_H_

#include "config/aom_config.h"

#include "aom/aom_codec.h"
#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef MAX_SB_SIZE

// Max superblock size
#define MAX_SB_SIZE_LOG2 7
#define MAX_SB_SIZE (1 << MAX_SB_SIZE_LOG2)
#define MAX_SB_SQUARE (MAX_SB_SIZE * MAX_SB_SIZE)

// Min superblock size
#define MIN_SB_SIZE_LOG2 6

// Pixels per Mode Info (MI) unit
#define MI_SIZE_LOG2 2
#define MI_SIZE (1 << MI_SIZE_LOG2)

// MI-units per max superblock (MI Block - MIB)
#define MAX_MIB_SIZE_LOG2 (MAX_SB_SIZE_LOG2 - MI_SIZE_LOG2)
#define MAX_MIB_SIZE (1 << MAX_MIB_SIZE_LOG2)

// MI-units per min superblock
#define MIN_MIB_SIZE_LOG2 (MIN_SB_SIZE_LOG2 - MI_SIZE_LOG2)

// Mask to extract MI offset within max MIB
#define MAX_MIB_MASK (MAX_MIB_SIZE - 1)

// Maximum number of tile rows and tile columns
#define MAX_TILE_ROWS 64
#define MAX_TILE_COLS 64

#define MAX_VARTX_DEPTH 2

#define MI_SIZE_64X64 (64 >> MI_SIZE_LOG2)
#define MI_SIZE_128X128 (128 >> MI_SIZE_LOG2)

#define MAX_PALETTE_SQUARE (64 * 64)
// Maximum number of colors in a palette.
#define PALETTE_MAX_SIZE 8
// Minimum number of colors in a palette.
#define PALETTE_MIN_SIZE 2

#define FRAME_OFFSET_BITS 5
#define MAX_FRAME_DISTANCE ((1 << FRAME_OFFSET_BITS) - 1)

#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)

// 4 scratch frames for the new frames to support a maximum of 4 cores decoding
// in parallel, 3 for scaled references on the encoder.
// TODO(hkuang): Add ondemand frame buffers instead of hardcoding the number
// of framebuffers.
// TODO(jkoleszar): These 3 extra references could probably come from the
// normal reference pool.
#define FRAME_BUFFERS (REF_FRAMES + 7)

// 4 frame filter levels: y plane vertical, y plane horizontal,
// u plane, and v plane
#define FRAME_LF_COUNT 4
#define DEFAULT_DELTA_LF_MULTI 0
#define MAX_MODE_LF_DELTAS 2

#define DIST_PRECISION_BITS 4
#define DIST_PRECISION (1 << DIST_PRECISION_BITS)  // 16

// TODO(chengchen): Temporal flag serve as experimental flag for WIP
// bitmask construction.
// Shall be removed when bitmask code is completely checkedin
#define LOOP_FILTER_BITMASK 0

#define PROFILE_BITS 3
// The following three profiles are currently defined.
// Profile 0.  8-bit and 10-bit 4:2:0 and 4:0:0 only.
// Profile 1.  8-bit and 10-bit 4:4:4
// Profile 2.  8-bit and 10-bit 4:2:2
//            12-bit  4:0:0, 4:2:2 and 4:4:4
// Since we have three bits for the profiles, it can be extended later.
typedef enum BITSTREAM_PROFILE {
  PROFILE_0,
  PROFILE_1,
  PROFILE_2,
  MAX_PROFILES,
} BITSTREAM_PROFILE;

#define LEVEL_MAJOR_BITS 3
#define LEVEL_MINOR_BITS 2
#define LEVEL_BITS (LEVEL_MAJOR_BITS + LEVEL_MINOR_BITS)

#define LEVEL_MAJOR_MIN 2
#define LEVEL_MAJOR_MAX ((1 << LEVEL_MAJOR_BITS) - 1 + LEVEL_MAJOR_MIN)
#define LEVEL_MINOR_MIN 0
#define LEVEL_MINOR_MAX ((1 << LEVEL_MINOR_BITS) - 1)

#define OP_POINTS_CNT_MINUS_1_BITS 5
#define OP_POINTS_IDC_BITS 12

// Note: Some enums use the attribute 'packed' to use smallest possible integer
// type, so that we can save memory when they are used in structs/arrays.

typedef enum ATTRIBUTE_PACKED {
  BLOCK_4X4,
  BLOCK_4X8,
  BLOCK_8X4,
  BLOCK_8X8,
  BLOCK_8X16,
  BLOCK_16X8,
  BLOCK_16X16,
  BLOCK_16X32,
  BLOCK_32X16,
  BLOCK_32X32,
  BLOCK_32X64,
  BLOCK_64X32,
  BLOCK_64X64,
  BLOCK_64X128,
  BLOCK_128X64,
  BLOCK_128X128,
  BLOCK_4X16,
  BLOCK_16X4,
  BLOCK_8X32,
  BLOCK_32X8,
  BLOCK_16X64,
  BLOCK_64X16,
  BLOCK_SIZES_ALL,
  BLOCK_SIZES = BLOCK_4X16,
  BLOCK_INVALID = 255,
  BLOCK_LARGEST = (BLOCK_SIZES - 1)
} BLOCK_SIZE;

// 4X4, 8X8, 16X16, 32X32, 64X64, 128X128
#define SQR_BLOCK_SIZES 6

typedef enum ATTRIBUTE_PACKED {
  PARTITION_NONE,
  PARTITION_HORZ,
  PARTITION_VERT,
  PARTITION_SPLIT,
  PARTITION_HORZ_A,  // HORZ split and the top partition is split again
  PARTITION_HORZ_B,  // HORZ split and the bottom partition is split again
  PARTITION_VERT_A,  // VERT split and the left partition is split again
  PARTITION_VERT_B,  // VERT split and the right partition is split again
  PARTITION_HORZ_4,  // 4:1 horizontal partition
  PARTITION_VERT_4,  // 4:1 vertical partition
  EXT_PARTITION_TYPES,
  PARTITION_TYPES = PARTITION_SPLIT + 1,
  PARTITION_INVALID = 255
} PARTITION_TYPE;

typedef char PARTITION_CONTEXT;
#define PARTITION_PLOFFSET 4  // number of probability models per block size
#define PARTITION_BLOCK_SIZES 5
#define PARTITION_CONTEXTS (PARTITION_BLOCK_SIZES * PARTITION_PLOFFSET)

// block transform size
#if defined(_MSC_VER)
typedef uint8_t TX_SIZE;
enum ATTRIBUTE_PACKED {
#else
typedef enum ATTRIBUTE_PACKED {
#endif
  TX_4X4,             // 4x4 transform
  TX_8X8,             // 8x8 transform
  TX_16X16,           // 16x16 transform
  TX_32X32,           // 32x32 transform
  TX_64X64,           // 64x64 transform
  TX_4X8,             // 4x8 transform
  TX_8X4,             // 8x4 transform
  TX_8X16,            // 8x16 transform
  TX_16X8,            // 16x8 transform
  TX_16X32,           // 16x32 transform
  TX_32X16,           // 32x16 transform
  TX_32X64,           // 32x64 transform
  TX_64X32,           // 64x32 transform
  TX_4X16,            // 4x16 transform
  TX_16X4,            // 16x4 transform
  TX_8X32,            // 8x32 transform
  TX_32X8,            // 32x8 transform
  TX_16X64,           // 16x64 transform
  TX_64X16,           // 64x16 transform
  TX_SIZES_ALL,       // Includes rectangular transforms
  TX_SIZES = TX_4X8,  // Does NOT include rectangular transforms
  TX_SIZES_LARGEST = TX_64X64,
  TX_INVALID = 255  // Invalid transform size
#if defined(_MSC_VER)
};
#else
} TX_SIZE;
#endif

#define TX_SIZE_LUMA_MIN (TX_4X4)
/* We don't need to code a transform size unless the allowed size is at least
   one more than the minimum. */
#define TX_SIZE_CTX_MIN (TX_SIZE_LUMA_MIN + 1)

// Maximum tx_size categories
#define MAX_TX_CATS (TX_SIZES - TX_SIZE_CTX_MIN)
#define MAX_TX_DEPTH 2

#define MAX_TX_SIZE_LOG2 (6)
#define MAX_TX_SIZE (1 << MAX_TX_SIZE_LOG2)
#define MIN_TX_SIZE_LOG2 2
#define MIN_TX_SIZE (1 << MIN_TX_SIZE_LOG2)
#define MAX_TX_SQUARE (MAX_TX_SIZE * MAX_TX_SIZE)

// Pad 4 extra columns to remove horizontal availability check.
#define TX_PAD_HOR_LOG2 2
#define TX_PAD_HOR 4
// Pad 6 extra rows (2 on top and 4 on bottom) to remove vertical availability
// check.
#define TX_PAD_TOP 2
#define TX_PAD_BOTTOM 4
#define TX_PAD_VER (TX_PAD_TOP + TX_PAD_BOTTOM)
// Pad 16 extra bytes to avoid reading overflow in SIMD optimization.
#define TX_PAD_END 16
#define TX_PAD_2D ((32 + TX_PAD_HOR) * (32 + TX_PAD_VER) + TX_PAD_END)

// Number of maxium size transform blocks in the maximum size superblock
#define MAX_TX_BLOCKS_IN_MAX_SB_LOG2 ((MAX_SB_SIZE_LOG2 - MAX_TX_SIZE_LOG2) * 2)
#define MAX_TX_BLOCKS_IN_MAX_SB (1 << MAX_TX_BLOCKS_IN_MAX_SB_LOG2)

// frame transform mode
typedef enum ATTRIBUTE_PACKED {
  ONLY_4X4,         // use only 4x4 transform
  TX_MODE_LARGEST,  // transform size is the largest possible for pu size
  TX_MODE_SELECT,   // transform specified for each block
  TX_MODES,
} TX_MODE;

// 1D tx types
typedef enum ATTRIBUTE_PACKED {
  DCT_1D,
  ADST_1D,
  FLIPADST_1D,
  IDTX_1D,
  TX_TYPES_1D,
} TX_TYPE_1D;

typedef enum ATTRIBUTE_PACKED {
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
  TX_TYPES,
} TX_TYPE;

typedef enum {
  REG_REG,
  REG_SMOOTH,
  REG_SHARP,
  SMOOTH_REG,
  SMOOTH_SMOOTH,
  SMOOTH_SHARP,
  SHARP_REG,
  SHARP_SMOOTH,
  SHARP_SHARP,
} DUAL_FILTER_TYPE;

typedef enum ATTRIBUTE_PACKED {
  // DCT only
  EXT_TX_SET_DCTONLY,
  // DCT + Identity only
  EXT_TX_SET_DCT_IDTX,
  // Discrete Trig transforms w/o flip (4) + Identity (1)
  EXT_TX_SET_DTT4_IDTX,
  // Discrete Trig transforms w/o flip (4) + Identity (1) + 1D Hor/vert DCT (2)
  EXT_TX_SET_DTT4_IDTX_1DDCT,
  // Discrete Trig transforms w/ flip (9) + Identity (1) + 1D Hor/Ver DCT (2)
  EXT_TX_SET_DTT9_IDTX_1DDCT,
  // Discrete Trig transforms w/ flip (9) + Identity (1) + 1D Hor/Ver (6)
  EXT_TX_SET_ALL16,
  EXT_TX_SET_TYPES
} TxSetType;

#define IS_2D_TRANSFORM(tx_type) (tx_type < IDTX)

#define EXT_TX_SIZES 4       // number of sizes that use extended transforms
#define EXT_TX_SETS_INTER 4  // Sets of transform selections for INTER
#define EXT_TX_SETS_INTRA 3  // Sets of transform selections for INTRA

typedef enum ATTRIBUTE_PACKED {
  AOM_LAST_FLAG = 1 << 0,
  AOM_LAST2_FLAG = 1 << 1,
  AOM_LAST3_FLAG = 1 << 2,
  AOM_GOLD_FLAG = 1 << 3,
  AOM_BWD_FLAG = 1 << 4,
  AOM_ALT2_FLAG = 1 << 5,
  AOM_ALT_FLAG = 1 << 6,
  AOM_REFFRAME_ALL = (1 << 7) - 1
} AOM_REFFRAME;

typedef enum ATTRIBUTE_PACKED {
  UNIDIR_COMP_REFERENCE,
  BIDIR_COMP_REFERENCE,
  COMP_REFERENCE_TYPES,
} COMP_REFERENCE_TYPE;

typedef enum ATTRIBUTE_PACKED {
  PLANE_TYPE_Y,
  PLANE_TYPE_UV,
  PLANE_TYPES
} PLANE_TYPE;

#define CFL_ALPHABET_SIZE_LOG2 4
#define CFL_ALPHABET_SIZE (1 << CFL_ALPHABET_SIZE_LOG2)
#define CFL_MAGS_SIZE ((2 << CFL_ALPHABET_SIZE_LOG2) + 1)
#define CFL_IDX_U(idx) (idx >> CFL_ALPHABET_SIZE_LOG2)
#define CFL_IDX_V(idx) (idx & (CFL_ALPHABET_SIZE - 1))

typedef enum ATTRIBUTE_PACKED {
  CFL_PRED_U,
  CFL_PRED_V,
  CFL_PRED_PLANES
} CFL_PRED_TYPE;

typedef enum ATTRIBUTE_PACKED {
  CFL_SIGN_ZERO,
  CFL_SIGN_NEG,
  CFL_SIGN_POS,
  CFL_SIGNS
} CFL_SIGN_TYPE;

typedef enum ATTRIBUTE_PACKED {
  CFL_DISALLOWED,
  CFL_ALLOWED,
  CFL_ALLOWED_TYPES
} CFL_ALLOWED_TYPE;

// CFL_SIGN_ZERO,CFL_SIGN_ZERO is invalid
#define CFL_JOINT_SIGNS (CFL_SIGNS * CFL_SIGNS - 1)
// CFL_SIGN_U is equivalent to (js + 1) / 3 for js in 0 to 8
#define CFL_SIGN_U(js) (((js + 1) * 11) >> 5)
// CFL_SIGN_V is equivalent to (js + 1) % 3 for js in 0 to 8
#define CFL_SIGN_V(js) ((js + 1) - CFL_SIGNS * CFL_SIGN_U(js))

// There is no context when the alpha for a given plane is zero.
// So there are 2 fewer contexts than joint signs.
#define CFL_ALPHA_CONTEXTS (CFL_JOINT_SIGNS + 1 - CFL_SIGNS)
#define CFL_CONTEXT_U(js) (js + 1 - CFL_SIGNS)
// Also, the contexts are symmetric under swapping the planes.
#define CFL_CONTEXT_V(js) \
  (CFL_SIGN_V(js) * CFL_SIGNS + CFL_SIGN_U(js) - CFL_SIGNS)

typedef enum ATTRIBUTE_PACKED {
  PALETTE_MAP,
  COLOR_MAP_TYPES,
} COLOR_MAP_TYPE;

typedef enum ATTRIBUTE_PACKED {
  TWO_COLORS,
  THREE_COLORS,
  FOUR_COLORS,
  FIVE_COLORS,
  SIX_COLORS,
  SEVEN_COLORS,
  EIGHT_COLORS,
  PALETTE_SIZES
} PALETTE_SIZE;

typedef enum ATTRIBUTE_PACKED {
  PALETTE_COLOR_ONE,
  PALETTE_COLOR_TWO,
  PALETTE_COLOR_THREE,
  PALETTE_COLOR_FOUR,
  PALETTE_COLOR_FIVE,
  PALETTE_COLOR_SIX,
  PALETTE_COLOR_SEVEN,
  PALETTE_COLOR_EIGHT,
  PALETTE_COLORS
} PALETTE_COLOR;

// Note: All directional predictors must be between V_PRED and D67_PRED (both
// inclusive).
typedef enum ATTRIBUTE_PACKED {
  DC_PRED,        // Average of above and left pixels
  V_PRED,         // Vertical
  H_PRED,         // Horizontal
  D45_PRED,       // Directional 45  degree
  D135_PRED,      // Directional 135 degree
  D113_PRED,      // Directional 113 degree
  D157_PRED,      // Directional 157 degree
  D203_PRED,      // Directional 203 degree
  D67_PRED,       // Directional 67  degree
  SMOOTH_PRED,    // Combination of horizontal and vertical interpolation
  SMOOTH_V_PRED,  // Vertical interpolation
  SMOOTH_H_PRED,  // Horizontal interpolation
  PAETH_PRED,     // Predict from the direction of smallest gradient
  NEARESTMV,
  NEARMV,
  GLOBALMV,
  NEWMV,
  // Compound ref compound modes
  NEAREST_NEARESTMV,
  NEAR_NEARMV,
  NEAREST_NEWMV,
  NEW_NEARESTMV,
  NEAR_NEWMV,
  NEW_NEARMV,
  GLOBAL_GLOBALMV,
  NEW_NEWMV,
  MB_MODE_COUNT,
  INTRA_MODE_START = DC_PRED,
  INTRA_MODE_END = NEARESTMV,
  INTRA_MODE_NUM = INTRA_MODE_END - INTRA_MODE_START,
  SINGLE_INTER_MODE_START = NEARESTMV,
  SINGLE_INTER_MODE_END = NEAREST_NEARESTMV,
  SINGLE_INTER_MODE_NUM = SINGLE_INTER_MODE_END - SINGLE_INTER_MODE_START,
  COMP_INTER_MODE_START = NEAREST_NEARESTMV,
  COMP_INTER_MODE_END = MB_MODE_COUNT,
  COMP_INTER_MODE_NUM = COMP_INTER_MODE_END - COMP_INTER_MODE_START,
  INTRA_MODES = PAETH_PRED + 1,  // PAETH_PRED has to be the last intra mode.
  INTRA_INVALID = MB_MODE_COUNT  // For uv_mode in inter blocks
} PREDICTION_MODE;

// TODO(ltrudeau) Do we really want to pack this?
// TODO(ltrudeau) Do we match with PREDICTION_MODE?
typedef enum ATTRIBUTE_PACKED {
  UV_DC_PRED,        // Average of above and left pixels
  UV_V_PRED,         // Vertical
  UV_H_PRED,         // Horizontal
  UV_D45_PRED,       // Directional 45  degree
  UV_D135_PRED,      // Directional 135 degree
  UV_D113_PRED,      // Directional 113 degree
  UV_D157_PRED,      // Directional 157 degree
  UV_D203_PRED,      // Directional 203 degree
  UV_D67_PRED,       // Directional 67  degree
  UV_SMOOTH_PRED,    // Combination of horizontal and vertical interpolation
  UV_SMOOTH_V_PRED,  // Vertical interpolation
  UV_SMOOTH_H_PRED,  // Horizontal interpolation
  UV_PAETH_PRED,     // Predict from the direction of smallest gradient
  UV_CFL_PRED,       // Chroma-from-Luma
  UV_INTRA_MODES,
  UV_MODE_INVALID,  // For uv_mode in inter blocks
} UV_PREDICTION_MODE;

typedef enum ATTRIBUTE_PACKED {
  SIMPLE_TRANSLATION,
  OBMC_CAUSAL,    // 2-sided OBMC
  WARPED_CAUSAL,  // 2-sided WARPED
  MOTION_MODES
} MOTION_MODE;

typedef enum ATTRIBUTE_PACKED {
  II_DC_PRED,
  II_V_PRED,
  II_H_PRED,
  II_SMOOTH_PRED,
  INTERINTRA_MODES
} INTERINTRA_MODE;

typedef enum {
  COMPOUND_AVERAGE,
  COMPOUND_WEDGE,
  COMPOUND_DIFFWTD,
  COMPOUND_TYPES,
} COMPOUND_TYPE;

typedef enum ATTRIBUTE_PACKED {
  FILTER_DC_PRED,
  FILTER_V_PRED,
  FILTER_H_PRED,
  FILTER_D157_PRED,
  FILTER_PAETH_PRED,
  FILTER_INTRA_MODES,
} FILTER_INTRA_MODE;

#define DIRECTIONAL_MODES 8
#define MAX_ANGLE_DELTA 3
#define ANGLE_STEP 3

#define INTER_MODES (1 + NEWMV - NEARESTMV)

#define INTER_COMPOUND_MODES (1 + NEW_NEWMV - NEAREST_NEARESTMV)

#define SKIP_CONTEXTS 3
#define SKIP_MODE_CONTEXTS 3

#define COMP_INDEX_CONTEXTS 6
#define COMP_GROUP_IDX_CONTEXTS 6

#define NMV_CONTEXTS 3

#define NEWMV_MODE_CONTEXTS 6
#define GLOBALMV_MODE_CONTEXTS 2
#define REFMV_MODE_CONTEXTS 6
#define DRL_MODE_CONTEXTS 3

#define GLOBALMV_OFFSET 3
#define REFMV_OFFSET 4

#define NEWMV_CTX_MASK ((1 << GLOBALMV_OFFSET) - 1)
#define GLOBALMV_CTX_MASK ((1 << (REFMV_OFFSET - GLOBALMV_OFFSET)) - 1)
#define REFMV_CTX_MASK ((1 << (8 - REFMV_OFFSET)) - 1)

#define COMP_NEWMV_CTXS 5
#define INTER_MODE_CONTEXTS 8

#define DELTA_Q_SMALL 3
#define DELTA_Q_PROBS (DELTA_Q_SMALL)
#define DEFAULT_DELTA_Q_RES 4
#define DELTA_LF_SMALL 3
#define DELTA_LF_PROBS (DELTA_LF_SMALL)
#define DEFAULT_DELTA_LF_RES 2

/* Segment Feature Masks */
#define MAX_MV_REF_CANDIDATES 2

#define MAX_REF_MV_STACK_SIZE 8
#define REF_CAT_LEVEL 640

#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS 3

#define COMP_REF_TYPE_CONTEXTS 5
#define UNI_COMP_REF_CONTEXTS 3

#define TXFM_PARTITION_CONTEXTS ((TX_SIZES - TX_8X8) * 6 - 3)
typedef uint8_t TXFM_CONTEXT;

#define NONE_FRAME -1
#define INTRA_FRAME 0
#define LAST_FRAME 1
#define LAST2_FRAME 2
#define LAST3_FRAME 3
#define GOLDEN_FRAME 4
#define BWDREF_FRAME 5
#define ALTREF2_FRAME 6
#define ALTREF_FRAME 7
#define LAST_REF_FRAMES (LAST3_FRAME - LAST_FRAME + 1)

#define INTER_REFS_PER_FRAME (ALTREF_FRAME - LAST_FRAME + 1)

#define FWD_REFS (GOLDEN_FRAME - LAST_FRAME + 1)
#define FWD_RF_OFFSET(ref) (ref - LAST_FRAME)
#define BWD_REFS (ALTREF_FRAME - BWDREF_FRAME + 1)
#define BWD_RF_OFFSET(ref) (ref - BWDREF_FRAME)

#define SINGLE_REFS (FWD_REFS + BWD_REFS)

typedef enum ATTRIBUTE_PACKED {
  LAST_LAST2_FRAMES,      // { LAST_FRAME, LAST2_FRAME }
  LAST_LAST3_FRAMES,      // { LAST_FRAME, LAST3_FRAME }
  LAST_GOLDEN_FRAMES,     // { LAST_FRAME, GOLDEN_FRAME }
  BWDREF_ALTREF_FRAMES,   // { BWDREF_FRAME, ALTREF_FRAME }
  LAST2_LAST3_FRAMES,     // { LAST2_FRAME, LAST3_FRAME }
  LAST2_GOLDEN_FRAMES,    // { LAST2_FRAME, GOLDEN_FRAME }
  LAST3_GOLDEN_FRAMES,    // { LAST3_FRAME, GOLDEN_FRAME }
  BWDREF_ALTREF2_FRAMES,  // { BWDREF_FRAME, ALTREF2_FRAME }
  ALTREF2_ALTREF_FRAMES,  // { ALTREF2_FRAME, ALTREF_FRAME }
  TOTAL_UNIDIR_COMP_REFS,
  // NOTE: UNIDIR_COMP_REFS is the number of uni-directional reference pairs
  //       that are explicitly signaled.
  UNIDIR_COMP_REFS = BWDREF_ALTREF_FRAMES + 1,
} UNIDIR_COMP_REF;

#define TOTAL_COMP_REFS (FWD_REFS * BWD_REFS + TOTAL_UNIDIR_COMP_REFS)

#define COMP_REFS (FWD_REFS * BWD_REFS + UNIDIR_COMP_REFS)

// NOTE: A limited number of unidirectional reference pairs can be signalled for
//       compound prediction. The use of skip mode, on the other hand, makes it
//       possible to have a reference pair not listed for explicit signaling.
#define MODE_CTX_REF_FRAMES (REF_FRAMES + TOTAL_COMP_REFS)

typedef enum ATTRIBUTE_PACKED {
  RESTORE_NONE,
  RESTORE_WIENER,
  RESTORE_SGRPROJ,
  RESTORE_SWITCHABLE,
  RESTORE_SWITCHABLE_TYPES = RESTORE_SWITCHABLE,
  RESTORE_TYPES = 4,
} RestorationType;

#define SUPERRES_SCALE_BITS 3
#define SUPERRES_SCALE_DENOMINATOR_MIN (SCALE_NUMERATOR + 1)

// In large_scale_tile coding, external references are used.
#define MAX_EXTERNAL_REFERENCES 128

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENUMS_H_
