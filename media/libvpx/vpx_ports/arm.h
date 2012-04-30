/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPX_PORTS_ARM_H
#define VPX_PORTS_ARM_H
#include <stdlib.h>
#include "vpx_config.h"

/*ARMv5TE "Enhanced DSP" instructions.*/
#define HAS_EDSP  0x01
/*ARMv6 "Parallel" or "Media" instructions.*/
#define HAS_MEDIA 0x02
/*ARMv7 optional NEON instructions.*/
#define HAS_NEON  0x04

int arm_cpu_caps(void);

#endif

