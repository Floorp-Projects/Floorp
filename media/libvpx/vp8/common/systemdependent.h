/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#if ARCH_X86 || ARCH_X86_64
void vpx_reset_mmx_state(void);
#define vp8_clear_system_state() vpx_reset_mmx_state()
#else
#define vp8_clear_system_state()
#endif

struct VP8Common;
void vp8_machine_specific_config(struct VP8Common *);
