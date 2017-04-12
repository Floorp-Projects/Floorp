/*
 *  Copyright 2015 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate.h"
#include "libyuv/rotate_row.h"

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#define TANY(NAMEANY, TPOS_SIMD, TPOS_C, MASK)                                 \
    void NAMEANY(const uint8* src, int src_stride,                             \
                 uint8* dst, int dst_stride, int width) {                      \
      int r = width & MASK;                                                    \
      int n = width - r;                                                       \
      if (n > 0) {                                                             \
        TPOS_SIMD(src, src_stride, dst, dst_stride, n);                        \
      }                                                                        \
      TPOS_C(src + n, src_stride, dst + n * dst_stride, dst_stride, r);        \
    }

#ifdef HAS_TRANSPOSEWX8_NEON
TANY(TransposeWx8_Any_NEON, TransposeWx8_NEON, TransposeWx8_C, 7)
#endif
#ifdef HAS_TRANSPOSEWX8_SSSE3
TANY(TransposeWx8_Any_SSSE3, TransposeWx8_SSSE3, TransposeWx8_C, 7)
#endif
#ifdef HAS_TRANSPOSEWX8_FAST_SSSE3
TANY(TransposeWx8_Fast_Any_SSSE3, TransposeWx8_Fast_SSSE3, TransposeWx8_C, 15)
#endif
#ifdef HAS_TRANSPOSEWX8_MIPS_DSPR2
TANY(TransposeWx8_Any_MIPS_DSPR2, TransposeWx8_MIPS_DSPR2, TransposeWx8_C, 7)
#endif

#undef TANY

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif





