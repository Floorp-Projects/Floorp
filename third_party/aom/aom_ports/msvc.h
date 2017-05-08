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

#ifndef AOM_PORTS_MSVC_H_
#define AOM_PORTS_MSVC_H_
#ifdef _MSC_VER

#include "./aom_config.h"

#if _MSC_VER < 1900  // VS2015 provides snprintf
#define snprintf _snprintf
#endif  // _MSC_VER < 1900

#if _MSC_VER < 1800  // VS2013 provides round
#include <math.h>
static INLINE double round(double x) {
  if (x < 0)
    return ceil(x - 0.5);
  else
    return floor(x + 0.5);
}

static INLINE float roundf(float x) {
  if (x < 0)
    return (float)ceil(x - 0.5f);
  else
    return (float)floor(x + 0.5f);
}

static INLINE long lroundf(float x) {
  if (x < 0)
    return (long)(x - 0.5f);
  else
    return (long)(x + 0.5f);
}
#endif  // _MSC_VER < 1800

#endif  // _MSC_VER
#endif  // AOM_PORTS_MSVC_H_
