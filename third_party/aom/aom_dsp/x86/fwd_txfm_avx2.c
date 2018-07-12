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

#include "./aom_config.h"

#define FDCT32x32_2D_AVX2 aom_fdct32x32_rd_avx2
#define FDCT32x32_HIGH_PRECISION 0
#include "aom_dsp/x86/fwd_dct32x32_impl_avx2.h"
#undef FDCT32x32_2D_AVX2
#undef FDCT32x32_HIGH_PRECISION

#define FDCT32x32_2D_AVX2 aom_fdct32x32_avx2
#define FDCT32x32_HIGH_PRECISION 1
#include "aom_dsp/x86/fwd_dct32x32_impl_avx2.h"  // NOLINT
#undef FDCT32x32_2D_AVX2
#undef FDCT32x32_HIGH_PRECISION
