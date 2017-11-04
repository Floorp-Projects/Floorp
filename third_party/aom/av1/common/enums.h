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

#include "./aom_config.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef MAX_SB_SIZE

#if CONFIG_NCOBMC_ADAPT_WEIGHT
#define TWO_MODE
#endif

#if CONFIG_NCOBMC || CONFIG_NCOBMC_ADAPT_WEIGHT
#define NC_MODE_INFO 1
#else
#define NC_MODE_INFO 0
#endif

// Max superblock size
#if CONFIG_EXT_PARTITION
#define MAX_SB_SIZE_LOG2 7
#else
#define MAX_SB_SIZE_LOG2 6
#endif  // CONFIG_EXT_PARTITION
#define MAX_SB_SIZE (1 << MAX_SB_SIZE_LOG2)
#define MAX_SB_SQUARE (MAX_SB_SIZE * MAX_SB_SIZE)

// Min superblock size
#define MIN_SB_SIZE_LOG2 6

// Pixels per Mode Info (MI) unit
#if CONFIG_CB4X4
#define MI_SIZE_LOG2 2
#else
#define MI_SIZE_LOG2 3
#endif
#define MI_SIZE (1 << MI_SIZE_LOG2)

// MI-units per max superblock (MI Block - MIB)
#define MAX_MIB_SIZE_LOG2 (MAX_SB_SIZE_LOG2 - MI_SIZE_LOG2)
#define MAX_MIB_SIZE (1 << MAX_MIB_SIZE_LOG2)

// MI-units per min superblock
#define MIN_MIB_SIZE_LOG2 (MIN_SB_SIZE_LOG2 - MI_SIZE_LOG2)

// Mask to extract MI offset within max MIB
#define MAX_MIB_MASK (MAX_MIB_SIZE - 1)

// Maximum number of tile rows and tile columns
#if CONFIG_EXT_TILE
#define MAX_TILE_ROWS 1024
#define MAX_TILE_COLS 1024
#else
#if CONFIG_MAX_TILE
#define MAX_TILE_ROWS 64
#define MAX_TILE_COLS 64
#else
#define MAX_TILE_ROWS 4
#define MAX_TILE_COLS 64
#endif
#endif  // CONFIG_EXT_TILE

#if CONFIG_VAR_TX
#define MAX_VARTX_DEPTH 2
#define SQR_VARTX_DEPTH_INIT 0
#define RECT_VARTX_DEPTH_INIT 0
#endif

#define MI_SIZE_64X64 (64 >> MI_SIZE_LOG2)

#if CONFIG_LOOPFILTER_LEVEL
// 4 frame filter levels: y plane vertical, y plane horizontal,
// u plane, and v plane
#define FRAME_LF_COUNT 4
#define DEFAULT_DELTA_LF_MULTI 0
#endif  // CONFIG_LOOPFILTER_LEVEL

#if CONFIG_LPF_SB
#define LPF_DELTA_BITS 3
#define LPF_STEP 2
#define DELTA_RANGE (1 << LPF_DELTA_BITS)
#define MAX_LPF_OFFSET (LPF_STEP * ((1 << LPF_DELTA_BITS) - 1))

#define LPF_REUSE_CONTEXT 2
#define LPF_DELTA_CONTEXT DELTA_RANGE
#define LPF_SIGN_CONTEXT 2

// Half of maximum loop filter length (15-tap)
#define FILT_BOUNDARY_OFFSET 8
#define FILT_BOUNDARY_MI_OFFSET (FILT_BOUNDARY_OFFSET >> MI_SIZE_LOG2)
#endif  // CONFIG_LPF_SB

// Bitstream profiles indicated by 2-3 bits in the uncompressed header.
// 00: Profile 0.  8-bit 4:2:0 only.
// 10: Profile 1.  8-bit 4:4:4, 4:2:2, and 4:4:0.
// 01: Profile 2.  10-bit and 12-bit color only, with 4:2:0 sampling.
// 110: Profile 3. 10-bit and 12-bit color only, with 4:2:2/4:4:4/4:4:0
//                 sampling.
// 111: Undefined profile.
typedef enum BITSTREAM_PROFILE {
  PROFILE_0,
  PROFILE_1,
  PROFILE_2,
  PROFILE_3,
  MAX_PROFILES
} BITSTREAM_PROFILE;

// Note: Some enums use the attribute 'packed' to use smallest possible integer
// type, so that we can save memory when they are used in structs/arrays.

typedef enum ATTRIBUTE_PACKED {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  BLOCK_2X2,
  BLOCK_2X4,
  BLOCK_4X2,
#endif
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
#if CONFIG_EXT_PARTITION
  BLOCK_64X128,
  BLOCK_128X64,
  BLOCK_128X128,
#endif  // CONFIG_EXT_PARTITION
  BLOCK_4X16,
  BLOCK_16X4,
  BLOCK_8X32,
  BLOCK_32X8,
  BLOCK_16X64,
  BLOCK_64X16,
#if CONFIG_EXT_PARTITION
  BLOCK_32X128,
  BLOCK_128X32,
#endif  // CONFIG_EXT_PARTITION
  BLOCK_SIZES_ALL,
  BLOCK_SIZES = BLOCK_4X16,
  BLOCK_INVALID = 255,
  BLOCK_LARGEST = (BLOCK_SIZES - 1)
} BLOCK_SIZE;

typedef enum {
  PARTITION_NONE,
  PARTITION_HORZ,
  PARTITION_VERT,
  PARTITION_SPLIT,
#if CONFIG_EXT_PARTITION_TYPES
  PARTITION_HORZ_A,  // HORZ split and the top partition is split again
  PARTITION_HORZ_B,  // HORZ split and the bottom partition is split again
  PARTITION_VERT_A,  // VERT split and the left partition is split again
  PARTITION_VERT_B,  // VERT split and the right partition is split again
  PARTITION_HORZ_4,  // 4:1 horizontal partition
  PARTITION_VERT_4,  // 4:1 vertical partition
  EXT_PARTITION_TYPES,
#endif  // CONFIG_EXT_PARTITION_TYPES
  PARTITION_TYPES = PARTITION_SPLIT + 1,
  PARTITION_INVALID = 255
} PARTITION_TYPE;

typedef char PARTITION_CONTEXT;
#define PARTITION_PLOFFSET 4  // number of probability models per block size
#define PARTITION_BLOCK_SIZES (4 + CONFIG_EXT_PARTITION)
#define PARTITION_CONTEXTS_PRIMARY (PARTITION_BLOCK_SIZES * PARTITION_PLOFFSET)
#if CONFIG_UNPOISON_PARTITION_CTX
#define INVALID_PARTITION_CTX (-1)
#define PARTITION_CONTEXTS \
  (PARTITION_CONTEXTS_PRIMARY + 2 * PARTITION_BLOCK_SIZES)
#else
#define PARTITION_CONTEXTS PARTITION_CONTEXTS_PRIMARY
#endif

// block transform size
typedef enum ATTRIBUTE_PACKED {
#if CONFIG_CHROMA_2X2
  TX_2X2,  // 2x2 transform
#endif
  TX_4X4,    // 4x4 transform
  TX_8X8,    // 8x8 transform
  TX_16X16,  // 16x16 transform
  TX_32X32,  // 32x32 transform
#if CONFIG_TX64X64
  TX_64X64,  // 64x64 transform
#endif       // CONFIG_TX64X64
  TX_4X8,    // 4x8 transform
  TX_8X4,    // 8x4 transform
  TX_8X16,   // 8x16 transform
  TX_16X8,   // 16x8 transform
  TX_16X32,  // 16x32 transform
  TX_32X16,  // 32x16 transform
#if CONFIG_TX64X64
  TX_32X64,           // 32x64 transform
  TX_64X32,           // 64x32 transform
#endif                // CONFIG_TX64X64
  TX_4X16,            // 4x16 transform
  TX_16X4,            // 16x4 transform
  TX_8X32,            // 8x32 transform
  TX_32X8,            // 32x8 transform
  TX_SIZES_ALL,       // Includes rectangular transforms
  TX_SIZES = TX_4X8,  // Does NOT include rectangular transforms
  TX_INVALID = 255    // Invalid transform size
} TX_SIZE;

#define TX_SIZE_LUMA_MIN (TX_4X4)
/* We don't need to code a transform size unless the allowed size is at least
   one more than the minimum. */
#define TX_SIZE_CTX_MIN (TX_SIZE_LUMA_MIN + 1)

#define MAX_TX_DEPTH (TX_SIZES - TX_SIZE_CTX_MIN)

#if CONFIG_CTX1D
#define MAX_HVTX_SIZE (1 << 5)
#endif  // CONFIG_CTX1D

#define MAX_TX_SIZE_LOG2 (5 + CONFIG_TX64X64)
#define MAX_TX_SIZE (1 << MAX_TX_SIZE_LOG2)
#define MIN_TX_SIZE_LOG2 2
#define MIN_TX_SIZE (1 << MIN_TX_SIZE_LOG2)
#define MAX_TX_SQUARE (MAX_TX_SIZE * MAX_TX_SIZE)

// Number of maxium size transform blocks in the maximum size superblock
#define MAX_TX_BLOCKS_IN_MAX_SB_LOG2 ((MAX_SB_SIZE_LOG2 - MAX_TX_SIZE_LOG2) * 2)
#define MAX_TX_BLOCKS_IN_MAX_SB (1 << MAX_TX_BLOCKS_IN_MAX_SB_LOG2)

#if CONFIG_NCOBMC_ADAPT_WEIGHT
typedef enum ATTRIBUTE_PACKED {
  NCOBMC_MODE_0,
  NCOBMC_MODE_1,
  NCOBMC_MODE_2,
  NCOBMC_MODE_3,
  NCOBMC_MODE_4,
  NCOBMC_MODE_5,
  NCOBMC_MODE_6,
  NCOBMC_MODE_7,
  ALL_NCOBMC_MODES,
#ifdef TWO_MODE
  MAX_NCOBMC_MODES = NCOBMC_MODE_1 + 1,
#else
  MAX_NCOBMC_MODES = ALL_NCOBMC_MODES,
#endif
  NO_OVERLAP = MAX_NCOBMC_MODES + 1
} NCOBMC_MODE;

typedef enum {
  ADAPT_OVERLAP_BLOCK_8X8,
  ADAPT_OVERLAP_BLOCK_16X16,
  ADAPT_OVERLAP_BLOCK_32X32,
  ADAPT_OVERLAP_BLOCK_64X64,
  ADAPT_OVERLAP_BLOCKS,
  ADAPT_OVERLAP_BLOCK_INVALID = 255
} ADAPT_OVERLAP_BLOCK;
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

// frame transform mode
typedef enum {
  ONLY_4X4,     // only 4x4 transform used
  ALLOW_8X8,    // allow block transform size up to 8x8
  ALLOW_16X16,  // allow block transform size up to 16x16
  ALLOW_32X32,  // allow block transform size up to 32x32
#if CONFIG_TX64X64
  ALLOW_64X64,  // allow block transform size up to 64x64
#endif
  TX_MODE_SELECT,  // transform specified for each block
  TX_MODES,
} TX_MODE;

// 1D tx types
typedef enum {
  DCT_1D,
  ADST_1D,
  FLIPADST_1D,
  IDTX_1D,
  // TODO(sarahparker) need to eventually put something here for the
  // mrc experiment to make this work with the ext-tx pruning functions
  TX_TYPES_1D,
} TX_TYPE_1D;

typedef enum {
  DCT_DCT,    // DCT  in both horizontal and vertical
  ADST_DCT,   // ADST in vertical, DCT in horizontal
  DCT_ADST,   // DCT  in vertical, ADST in horizontal
  ADST_ADST,  // ADST in both directions
#if CONFIG_EXT_TX
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
#endif  // CONFIG_EXT_TX
#if CONFIG_MRC_TX
  MRC_DCT,  // DCT in both directions with mrc based bitmask
#endif      // CONFIG_MRC_TX
  TX_TYPES,
} TX_TYPE;

#if CONFIG_EXT_TX
typedef enum {
  // DCT only
  EXT_TX_SET_DCTONLY,
  // DCT + Identity only
  EXT_TX_SET_DCT_IDTX,
#if CONFIG_MRC_TX
  // DCT + MRC_DCT
  EXT_TX_SET_MRC_DCT,
  // DCT + MRC_DCT + IDTX
  EXT_TX_SET_MRC_DCT_IDTX,
#endif  // CONFIG_MRC_TX
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
#else
#define IS_2D_TRANSFORM(tx_type) 1
#endif

typedef enum {
  TILE_LEFT_BOUNDARY = 1,
  TILE_RIGHT_BOUNDARY = 2,
  TILE_ABOVE_BOUNDARY = 4,
  TILE_BOTTOM_BOUNDARY = 8,
  FRAME_LEFT_BOUNDARY = 16,
  FRAME_RIGHT_BOUNDARY = 32,
  FRAME_ABOVE_BOUNDARY = 64,
  FRAME_BOTTOM_BOUNDARY = 128,
} BOUNDARY_TYPE;

#if CONFIG_EXT_TX
#if CONFIG_CHROMA_2X2
#define EXT_TX_SIZES 5  // number of sizes that use extended transforms
#else
#define EXT_TX_SIZES 4  // number of sizes that use extended transforms
#endif                  // CONFIG_CHROMA_2X2
#if CONFIG_MRC_TX
#define EXT_TX_SETS_INTER 5  // Sets of transform selections for INTER
#define EXT_TX_SETS_INTRA 4  // Sets of transform selections for INTRA
#else                        // CONFIG_MRC_TX
#define EXT_TX_SETS_INTER 4  // Sets of transform selections for INTER
#define EXT_TX_SETS_INTRA 3  // Sets of transform selections for INTRA
#endif                       // CONFIG_MRC_TX
#else
#if CONFIG_CHROMA_2X2
#define EXT_TX_SIZES 4  // number of sizes that use extended transforms
#else
#define EXT_TX_SIZES 3  // number of sizes that use extended transforms
#endif
#endif  // CONFIG_EXT_TX

typedef enum {
  AOM_LAST_FLAG = 1 << 0,
#if CONFIG_EXT_REFS
  AOM_LAST2_FLAG = 1 << 1,
  AOM_LAST3_FLAG = 1 << 2,
  AOM_GOLD_FLAG = 1 << 3,
  AOM_BWD_FLAG = 1 << 4,
  AOM_ALT2_FLAG = 1 << 5,
  AOM_ALT_FLAG = 1 << 6,
  AOM_REFFRAME_ALL = (1 << 7) - 1
#else   // !CONFIG_EXT_REFS
  AOM_GOLD_FLAG = 1 << 1,
  AOM_ALT_FLAG = 1 << 2,
  AOM_REFFRAME_ALL = (1 << 3) - 1
#endif  // CONFIG_EXT_REFS
} AOM_REFFRAME;

#if CONFIG_EXT_COMP_REFS
#define USE_UNI_COMP_REFS 1

typedef enum {
  UNIDIR_COMP_REFERENCE,
  BIDIR_COMP_REFERENCE,
  COMP_REFERENCE_TYPES,
} COMP_REFERENCE_TYPE;
#else  // !CONFIG_EXT_COMP_REFS
#define USE_UNI_COMP_REFS 0
#endif  // CONFIG_EXT_COMP_REFS

typedef enum { PLANE_TYPE_Y, PLANE_TYPE_UV, PLANE_TYPES } PLANE_TYPE;

#if CONFIG_CFL
#define CFL_ALPHABET_SIZE_LOG2 4
#define CFL_ALPHABET_SIZE (1 << CFL_ALPHABET_SIZE_LOG2)
#define CFL_MAGS_SIZE ((2 << CFL_ALPHABET_SIZE_LOG2) + 1)
#define CFL_IDX_U(idx) (idx >> CFL_ALPHABET_SIZE_LOG2)
#define CFL_IDX_V(idx) (idx & (CFL_ALPHABET_SIZE - 1))

typedef enum { CFL_PRED_U, CFL_PRED_V, CFL_PRED_PLANES } CFL_PRED_TYPE;

typedef enum {
  CFL_SIGN_ZERO,
  CFL_SIGN_NEG,
  CFL_SIGN_POS,
  CFL_SIGNS
} CFL_SIGN_TYPE;

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
#endif

typedef enum {
  PALETTE_MAP,
#if CONFIG_MRC_TX
  MRC_MAP,
#endif  // CONFIG_MRC_TX
  COLOR_MAP_TYPES,
} COLOR_MAP_TYPE;

typedef enum {
  TWO_COLORS,
  THREE_COLORS,
  FOUR_COLORS,
  FIVE_COLORS,
  SIX_COLORS,
  SEVEN_COLORS,
  EIGHT_COLORS,
  PALETTE_SIZES
} PALETTE_SIZE;

typedef enum {
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

// Note: All directional predictors must be between V_PRED and D63_PRED (both
// inclusive).
typedef enum ATTRIBUTE_PACKED {
  DC_PRED,      // Average of above and left pixels
  V_PRED,       // Vertical
  H_PRED,       // Horizontal
  D45_PRED,     // Directional 45  deg = round(arctan(1/1) * 180/pi)
  D135_PRED,    // Directional 135 deg = 180 - 45
  D117_PRED,    // Directional 117 deg = 180 - 63
  D153_PRED,    // Directional 153 deg = 180 - 27
  D207_PRED,    // Directional 207 deg = 180 + 27
  D63_PRED,     // Directional 63  deg = round(arctan(2/1) * 180/pi)
  SMOOTH_PRED,  // Combination of horizontal and vertical interpolation
#if CONFIG_SMOOTH_HV
  SMOOTH_V_PRED,  // Vertical interpolation
  SMOOTH_H_PRED,  // Horizontal interpolation
#endif            // CONFIG_SMOOTH_HV
  TM_PRED,        // True-motion
  NEARESTMV,
  NEARMV,
  ZEROMV,
  NEWMV,
#if CONFIG_COMPOUND_SINGLEREF
  // Single ref compound modes
  SR_NEAREST_NEARMV,
  // SR_NEAREST_NEWMV,
  SR_NEAR_NEWMV,
  SR_ZERO_NEWMV,
  SR_NEW_NEWMV,
#endif  // CONFIG_COMPOUND_SINGLEREF
  // Compound ref compound modes
  NEAREST_NEARESTMV,
  NEAR_NEARMV,
  NEAREST_NEWMV,
  NEW_NEARESTMV,
  NEAR_NEWMV,
  NEW_NEARMV,
  ZERO_ZEROMV,
  NEW_NEWMV,
  MB_MODE_COUNT,
  INTRA_MODES = TM_PRED + 1,     // TM_PRED has to be the last intra mode.
  INTRA_INVALID = MB_MODE_COUNT  // For uv_mode in inter blocks
} PREDICTION_MODE;

#if CONFIG_CFL
// TODO(ltrudeau) Do we really want to pack this?
// TODO(ltrudeau) Do we match with PREDICTION_MODE?
typedef enum ATTRIBUTE_PACKED {
  UV_DC_PRED,      // Average of above and left pixels
  UV_V_PRED,       // Vertical
  UV_H_PRED,       // Horizontal
  UV_D45_PRED,     // Directional 45  deg = round(arctan(1/1) * 180/pi)
  UV_D135_PRED,    // Directional 135 deg = 180 - 45
  UV_D117_PRED,    // Directional 117 deg = 180 - 63
  UV_D153_PRED,    // Directional 153 deg = 180 - 27
  UV_D207_PRED,    // Directional 207 deg = 180 + 27
  UV_D63_PRED,     // Directional 63  deg = round(arctan(2/1) * 180/pi)
  UV_SMOOTH_PRED,  // Combination of horizontal and vertical interpolation
#if CONFIG_SMOOTH_HV
  UV_SMOOTH_V_PRED,  // Vertical interpolation
  UV_SMOOTH_H_PRED,  // Horizontal interpolation
#endif               // CONFIG_SMOOTH_HV
  UV_TM_PRED,        // True-motion
  UV_CFL_PRED,       // Chroma-from-Luma
  UV_INTRA_MODES,
  UV_MODE_INVALID,  // For uv_mode in inter blocks
} UV_PREDICTION_MODE;
#else
#define UV_INTRA_MODES (INTRA_MODES)
#define UV_PREDICTION_MODE PREDICTION_MODE
#define UV_DC_PRED (DC_PRED)
#define UV_MODE_INVALID (INTRA_INVALID)
#endif  // CONFIG_CFL

typedef enum {
  SIMPLE_TRANSLATION,
#if CONFIG_MOTION_VAR
  OBMC_CAUSAL,  // 2-sided OBMC
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  NCOBMC_ADAPT_WEIGHT,
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#endif  // CONFIG_MOTION_VAR
#if CONFIG_WARPED_MOTION
  WARPED_CAUSAL,  // 2-sided WARPED
#endif            // CONFIG_WARPED_MOTION
  MOTION_MODES
#if CONFIG_NCOBMC_ADAPT_WEIGHT && CONFIG_WARPED_MOTION
  ,
  OBMC_FAMILY_MODES = NCOBMC_ADAPT_WEIGHT + 1
#endif
} MOTION_MODE;

#if CONFIG_INTERINTRA
typedef enum {
  II_DC_PRED,
  II_V_PRED,
  II_H_PRED,
  II_SMOOTH_PRED,
  INTERINTRA_MODES
} INTERINTRA_MODE;
#endif

typedef enum {
  COMPOUND_AVERAGE,
#if CONFIG_WEDGE
  COMPOUND_WEDGE,
#endif  // CONFIG_WEDGE
#if CONFIG_COMPOUND_SEGMENT
  COMPOUND_SEG,
#endif  // CONFIG_COMPOUND_SEGMENT
  COMPOUND_TYPES,
} COMPOUND_TYPE;

// TODO(huisu): Consider adding FILTER_SMOOTH_PRED to "FILTER_INTRA_MODE".
#if CONFIG_FILTER_INTRA
typedef enum {
  FILTER_DC_PRED,
  FILTER_V_PRED,
  FILTER_H_PRED,
  FILTER_D45_PRED,
  FILTER_D135_PRED,
  FILTER_D117_PRED,
  FILTER_D153_PRED,
  FILTER_D207_PRED,
  FILTER_D63_PRED,
  FILTER_TM_PRED,
  FILTER_INTRA_MODES,
} FILTER_INTRA_MODE;
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
#define DIRECTIONAL_MODES 8
#endif  // CONFIG_EXT_INTRA

#define INTER_MODES (1 + NEWMV - NEARESTMV)

#if CONFIG_COMPOUND_SINGLEREF
#define INTER_SINGLEREF_COMP_MODES (1 + SR_NEW_NEWMV - SR_NEAREST_NEARMV)
#endif  // CONFIG_COMPOUND_SINGLEREF

#define INTER_COMPOUND_MODES (1 + NEW_NEWMV - NEAREST_NEARESTMV)

#define SKIP_CONTEXTS 3

#define NMV_CONTEXTS 3

#define NEWMV_MODE_CONTEXTS 7
#define ZEROMV_MODE_CONTEXTS 2
#define REFMV_MODE_CONTEXTS 9
#define DRL_MODE_CONTEXTS 5

#define ZEROMV_OFFSET 3
#define REFMV_OFFSET 4

#define NEWMV_CTX_MASK ((1 << ZEROMV_OFFSET) - 1)
#define ZEROMV_CTX_MASK ((1 << (REFMV_OFFSET - ZEROMV_OFFSET)) - 1)
#define REFMV_CTX_MASK ((1 << (8 - REFMV_OFFSET)) - 1)

#define ALL_ZERO_FLAG_OFFSET 8
#define SKIP_NEARESTMV_OFFSET 9
#define SKIP_NEARMV_OFFSET 10
#define SKIP_NEARESTMV_SUB8X8_OFFSET 11

#define INTER_MODE_CONTEXTS 7
#define DELTA_Q_SMALL 3
#define DELTA_Q_PROBS (DELTA_Q_SMALL)
#define DEFAULT_DELTA_Q_RES 4
#if CONFIG_EXT_DELTA_Q
#define DELTA_LF_SMALL 3
#define DELTA_LF_PROBS (DELTA_LF_SMALL)
#define DEFAULT_DELTA_LF_RES 2
#endif

/* Segment Feature Masks */
#define MAX_MV_REF_CANDIDATES 2

#define MAX_REF_MV_STACK_SIZE 16
#if CONFIG_EXT_PARTITION
#define REF_CAT_LEVEL 640
#else
#define REF_CAT_LEVEL 255
#endif  // CONFIG_EXT_PARTITION

#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS 5

#if CONFIG_EXT_COMP_REFS
#define COMP_REF_TYPE_CONTEXTS 5
#define UNI_COMP_REF_CONTEXTS 3
#endif  // CONFIG_EXT_COMP_REFS

#if CONFIG_COMPOUND_SINGLEREF
#define COMP_INTER_MODE_CONTEXTS 4
#endif  // CONFIG_COMPOUND_SINGLEREF

#if CONFIG_VAR_TX
#define TXFM_PARTITION_CONTEXTS ((TX_SIZES - TX_8X8) * 6 - 2)
typedef uint8_t TXFM_CONTEXT;
#endif

#define NONE_FRAME -1
#define INTRA_FRAME 0
#define LAST_FRAME 1

#if CONFIG_EXT_REFS
#define LAST2_FRAME 2
#define LAST3_FRAME 3
#define GOLDEN_FRAME 4
#define BWDREF_FRAME 5
#define ALTREF2_FRAME 6
#define ALTREF_FRAME 7
#define LAST_REF_FRAMES (LAST3_FRAME - LAST_FRAME + 1)
#else  // !CONFIG_EXT_REFS
#define GOLDEN_FRAME 2
#define ALTREF_FRAME 3
#endif  // CONFIG_EXT_REFS

#define INTER_REFS_PER_FRAME (ALTREF_FRAME - LAST_FRAME + 1)
#define TOTAL_REFS_PER_FRAME (ALTREF_FRAME - INTRA_FRAME + 1)

#define FWD_REFS (GOLDEN_FRAME - LAST_FRAME + 1)
#define FWD_RF_OFFSET(ref) (ref - LAST_FRAME)
#if CONFIG_EXT_REFS
#define BWD_REFS (ALTREF_FRAME - BWDREF_FRAME + 1)
#define BWD_RF_OFFSET(ref) (ref - BWDREF_FRAME)
#else
#define BWD_REFS 1
#define BWD_RF_OFFSET(ref) (ref - ALTREF_FRAME)
#endif  // CONFIG_EXT_REFS

#define SINGLE_REFS (FWD_REFS + BWD_REFS)
#if CONFIG_EXT_COMP_REFS
typedef enum {
  LAST_LAST2_FRAMES,     // { LAST_FRAME, LAST2_FRAME }
  LAST_LAST3_FRAMES,     // { LAST_FRAME, LAST3_FRAME }
  LAST_GOLDEN_FRAMES,    // { LAST_FRAME, GOLDEN_FRAME }
  BWDREF_ALTREF_FRAMES,  // { BWDREF_FRAME, ALTREF_FRAME }
  UNIDIR_COMP_REFS
} UNIDIR_COMP_REF;
#define COMP_REFS (FWD_REFS * BWD_REFS + UNIDIR_COMP_REFS)
#else  // !CONFIG_EXT_COMP_REFS
#define COMP_REFS (FWD_REFS * BWD_REFS)
#endif  // CONFIG_EXT_COMP_REFS

#define MODE_CTX_REF_FRAMES (TOTAL_REFS_PER_FRAME + COMP_REFS)

#if CONFIG_SUPERTX
#define PARTITION_SUPERTX_CONTEXTS 2
#define MAX_SUPERTX_BLOCK_SIZE BLOCK_32X32
#endif  // CONFIG_SUPERTX

#if CONFIG_LOOP_RESTORATION
typedef enum {
  RESTORE_NONE,
  RESTORE_WIENER,
  RESTORE_SGRPROJ,
  RESTORE_SWITCHABLE,
  RESTORE_SWITCHABLE_TYPES = RESTORE_SWITCHABLE,
  RESTORE_TYPES,
} RestorationType;
#endif  // CONFIG_LOOP_RESTORATION

#if CONFIG_FRAME_SUPERRES
#define SUPERRES_SCALE_BITS 3
#define SUPERRES_SCALE_DENOMINATOR_MIN 8
#endif  // CONFIG_FRAME_SUPERRES

#if CONFIG_LPF_DIRECT
typedef enum {
  VERT_HORZ,
  DEGREE_30,
  DEGREE_45,
  DEGREE_60,
  DEGREE_120,
  DEGREE_135,
  DEGREE_150,
  FILTER_DEGREES,
} FILTER_DEGREE;
#endif  // CONFIG_LPF_DIRECT

#if CONFIG_OBU
// R19
typedef enum {
  OBU_SEQUENCE_HEADER = 1,
  OBU_TD = 2,
  OBU_FRAME_HEADER = 3,
  OBU_TILE_GROUP = 4,
  OBU_METADATA = 5,
  OBU_PADDING = 15,
} OBU_TYPE;
#endif

#if CONFIG_LGT_FROM_PRED
#define LGT_SIZES 2
// Note: at least one of LGT_FROM_PRED_INTRA and LGT_FROM_PRED_INTER must be 1
#define LGT_FROM_PRED_INTRA 1
#define LGT_FROM_PRED_INTER 1
// LGT_SL_INTRA: LGTs with a mode-dependent first self-loop and a break point
#define LGT_SL_INTRA 0
#endif  // CONFIG_LGT_FROM_PRED

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENUMS_H_
