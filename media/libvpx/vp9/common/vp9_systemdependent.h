/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_SYSTEMDEPENDENT_H_
#define VP9_COMMON_VP9_SYSTEMDEPENDENT_H_

#ifdef _MSC_VER
#include <math.h>
#define snprintf _snprintf
#endif

#include "./vpx_config.h"
#if ARCH_X86 || ARCH_X86_64
void vpx_reset_mmx_state(void);
#define vp9_clear_system_state() vpx_reset_mmx_state()
#else
#define vp9_clear_system_state()
#endif

#if defined(_MSC_VER) && _MSC_VER < 1800
// round is not defined in MSVC before VS2013.
static int round(double x) {
  if (x < 0)
    return (int)ceil(x - 0.5);
  else
    return (int)floor(x + 0.5);
}
#endif

struct VP9Common;
void vp9_machine_specific_config(struct VP9Common *cm);

#endif  // VP9_COMMON_VP9_SYSTEMDEPENDENT_H_
