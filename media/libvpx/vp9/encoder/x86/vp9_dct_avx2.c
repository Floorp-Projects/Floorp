/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <immintrin.h>  // AVX2
#include "vp9/common/vp9_idct.h"  // for cospi constants
#include "vpx_ports/mem.h"


#define FDCT32x32_2D_AVX2 vp9_fdct32x32_rd_avx2
#define FDCT32x32_HIGH_PRECISION 0
#include "vp9/encoder/x86/vp9_dct32x32_avx2_impl.h"
#undef  FDCT32x32_2D_AVX2
#undef  FDCT32x32_HIGH_PRECISION

#define FDCT32x32_2D_AVX2 vp9_fdct32x32_avx2
#define FDCT32x32_HIGH_PRECISION 1
#include "vp9/encoder/x86/vp9_dct32x32_avx2_impl.h" // NOLINT
#undef  FDCT32x32_2D_AVX2
#undef  FDCT32x32_HIGH_PRECISION
