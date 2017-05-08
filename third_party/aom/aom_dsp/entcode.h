/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#if !defined(_entcode_H)
#define _entcode_H (1)
#include <limits.h>
#include <stddef.h>
#include "av1/common/odintrin.h"

/*OPT: od_ec_window must be at least 32 bits, but if you have fast arithmetic
   on a larger type, you can speed up the decoder by using it here.*/
typedef uint32_t od_ec_window;

#define OD_EC_WINDOW_SIZE ((int)sizeof(od_ec_window) * CHAR_BIT)

/*The number of bits to use for the range-coded part of unsigned integers.*/
#define OD_EC_UINT_BITS (4)

/*The resolution of fractional-precision bit usage measurements, i.e.,
   3 => 1/8th bits.*/
#define OD_BITRES (3)

/*With CONFIG_EC_SMALLMUL, the value stored in a CDF is 32768 minus the actual
   Q15 cumulative probability (an "inverse" CDF).
  This function converts from one representation to the other (and is its own
   inverse).*/
#if CONFIG_EC_SMALLMUL
#define OD_ICDF(x) (32768U - (x))
#else
#define OD_ICDF(x) (x)
#endif

/*See entcode.c for further documentation.*/

OD_WARN_UNUSED_RESULT uint32_t od_ec_tell_frac(uint32_t nbits_total,
                                               uint32_t rng);

#endif
