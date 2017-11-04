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

#ifndef AV1_COMMON_ENTROPYMV_H_
#define AV1_COMMON_ENTROPYMV_H_

#include "./aom_config.h"

#include "aom_dsp/prob.h"

#include "av1/common/mv.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AV1Common;

void av1_init_mv_probs(struct AV1Common *cm);

void av1_adapt_mv_probs(struct AV1Common *cm, int usehp);

#define MV_UPDATE_PROB 252

/* Symbols for coding which components are zero jointly */
#define MV_JOINTS 4
typedef enum {
  MV_JOINT_ZERO = 0,   /* Zero vector */
  MV_JOINT_HNZVZ = 1,  /* Vert zero, hor nonzero */
  MV_JOINT_HZVNZ = 2,  /* Hor zero, vert nonzero */
  MV_JOINT_HNZVNZ = 3, /* Both components nonzero */
} MV_JOINT_TYPE;

static INLINE int mv_joint_vertical(MV_JOINT_TYPE type) {
  return type == MV_JOINT_HZVNZ || type == MV_JOINT_HNZVNZ;
}

static INLINE int mv_joint_horizontal(MV_JOINT_TYPE type) {
  return type == MV_JOINT_HNZVZ || type == MV_JOINT_HNZVNZ;
}

/* Symbols for coding magnitude class of nonzero components */
#define MV_CLASSES 11
typedef enum {
  MV_CLASS_0 = 0,   /* (0, 2]     integer pel */
  MV_CLASS_1 = 1,   /* (2, 4]     integer pel */
  MV_CLASS_2 = 2,   /* (4, 8]     integer pel */
  MV_CLASS_3 = 3,   /* (8, 16]    integer pel */
  MV_CLASS_4 = 4,   /* (16, 32]   integer pel */
  MV_CLASS_5 = 5,   /* (32, 64]   integer pel */
  MV_CLASS_6 = 6,   /* (64, 128]  integer pel */
  MV_CLASS_7 = 7,   /* (128, 256] integer pel */
  MV_CLASS_8 = 8,   /* (256, 512] integer pel */
  MV_CLASS_9 = 9,   /* (512, 1024] integer pel */
  MV_CLASS_10 = 10, /* (1024,2048] integer pel */
} MV_CLASS_TYPE;

#define CLASS0_BITS 1 /* bits at integer precision for class 0 */
#define CLASS0_SIZE (1 << CLASS0_BITS)
#define MV_OFFSET_BITS (MV_CLASSES + CLASS0_BITS - 2)
#if CONFIG_NEW_MULTISYMBOL
#define MV_BITS_CONTEXTS 6
#endif
#define MV_FP_SIZE 4

#define MV_MAX_BITS (MV_CLASSES + CLASS0_BITS + 2)
#define MV_MAX ((1 << MV_MAX_BITS) - 1)
#define MV_VALS ((MV_MAX << 1) + 1)

#define MV_IN_USE_BITS 14
#define MV_UPP ((1 << MV_IN_USE_BITS) - 1)
#define MV_LOW (-(1 << MV_IN_USE_BITS))

extern const aom_tree_index av1_mv_joint_tree[];
extern const aom_tree_index av1_mv_class_tree[];
extern const aom_tree_index av1_mv_class0_tree[];
extern const aom_tree_index av1_mv_fp_tree[];

typedef struct {
  aom_prob sign;
  aom_prob classes[MV_CLASSES - 1];
  aom_cdf_prob class_cdf[CDF_SIZE(MV_CLASSES)];
  aom_prob class0[CLASS0_SIZE - 1];
  aom_prob bits[MV_OFFSET_BITS];
  aom_prob class0_fp[CLASS0_SIZE][MV_FP_SIZE - 1];
  aom_prob fp[MV_FP_SIZE - 1];
  aom_cdf_prob class0_fp_cdf[CLASS0_SIZE][CDF_SIZE(MV_FP_SIZE)];
  aom_cdf_prob fp_cdf[CDF_SIZE(MV_FP_SIZE)];
  aom_prob class0_hp;
  aom_prob hp;
#if CONFIG_NEW_MULTISYMBOL
  aom_cdf_prob class0_hp_cdf[CDF_SIZE(2)];
  aom_cdf_prob hp_cdf[CDF_SIZE(2)];
  aom_cdf_prob class0_cdf[CDF_SIZE(CLASS0_SIZE)];
  aom_cdf_prob bits_cdf[MV_BITS_CONTEXTS][CDF_SIZE(2)];
#endif
} nmv_component;

typedef struct {
  aom_prob joints[MV_JOINTS - 1];
  aom_cdf_prob joint_cdf[CDF_SIZE(MV_JOINTS)];
  nmv_component comps[2];
} nmv_context;

static INLINE MV_JOINT_TYPE av1_get_mv_joint(const MV *mv) {
  if (mv->row == 0) {
    return mv->col == 0 ? MV_JOINT_ZERO : MV_JOINT_HNZVZ;
  } else {
    return mv->col == 0 ? MV_JOINT_HZVNZ : MV_JOINT_HNZVNZ;
  }
}

MV_CLASS_TYPE av1_get_mv_class(int z, int *offset);

typedef struct {
  unsigned int sign[2];
  unsigned int classes[MV_CLASSES];
  unsigned int class0[CLASS0_SIZE];
  unsigned int bits[MV_OFFSET_BITS][2];
  unsigned int class0_fp[CLASS0_SIZE][MV_FP_SIZE];
  unsigned int fp[MV_FP_SIZE];
  unsigned int class0_hp[2];
  unsigned int hp[2];
} nmv_component_counts;

typedef struct {
  unsigned int joints[MV_JOINTS];
  nmv_component_counts comps[2];
} nmv_context_counts;

typedef enum {
#if CONFIG_INTRABC || CONFIG_AMVR
  MV_SUBPEL_NONE = -1,
#endif
  MV_SUBPEL_LOW_PRECISION = 0,
  MV_SUBPEL_HIGH_PRECISION,
} MvSubpelPrecision;

void av1_inc_mv(const MV *mv, nmv_context_counts *mvctx,
                MvSubpelPrecision precision);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ENTROPYMV_H_
