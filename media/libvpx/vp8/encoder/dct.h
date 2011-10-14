/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_DCT_H
#define __INC_DCT_H

#define prototype_fdct(sym) void (sym)(short *input, short *output, int pitch)

#if ARCH_X86 || ARCH_X86_64
#include "x86/dct_x86.h"
#endif

#if ARCH_ARM
#include "arm/dct_arm.h"
#endif

#ifndef vp8_fdct_short4x4
#define vp8_fdct_short4x4  vp8_short_fdct4x4_c
#endif
extern prototype_fdct(vp8_fdct_short4x4);

#ifndef vp8_fdct_short8x4
#define vp8_fdct_short8x4  vp8_short_fdct8x4_c
#endif
extern prototype_fdct(vp8_fdct_short8x4);

// There is no fast4x4 (for now)
#ifndef vp8_fdct_fast4x4
#define vp8_fdct_fast4x4  vp8_short_fdct4x4_c
#endif

#ifndef vp8_fdct_fast8x4
#define vp8_fdct_fast8x4  vp8_short_fdct8x4_c
#endif

#ifndef vp8_fdct_walsh_short4x4
#define vp8_fdct_walsh_short4x4  vp8_short_walsh4x4_c
#endif
extern prototype_fdct(vp8_fdct_walsh_short4x4);

typedef prototype_fdct(*vp8_fdct_fn_t);
typedef struct
{
    vp8_fdct_fn_t    short4x4;
    vp8_fdct_fn_t    short8x4;
    vp8_fdct_fn_t    fast4x4;
    vp8_fdct_fn_t    fast8x4;
    vp8_fdct_fn_t    walsh_short4x4;
} vp8_fdct_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define FDCT_INVOKE(ctx,fn) (ctx)->fn
#else
#define FDCT_INVOKE(ctx,fn) vp8_fdct_##fn
#endif

#endif
