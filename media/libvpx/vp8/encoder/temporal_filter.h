/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_VP8_TEMPORAL_FILTER_H
#define __INC_VP8_TEMPORAL_FILTER_H

#define prototype_apply(sym)\
    void (sym) \
    ( \
     unsigned char *frame1, \
     unsigned int stride, \
     unsigned char *frame2, \
     unsigned int block_size, \
     int strength, \
     int filter_weight, \
     unsigned int *accumulator, \
     unsigned short *count \
    )

#if ARCH_X86 || ARCH_X86_64
#include "x86/temporal_filter_x86.h"
#endif

#ifndef vp8_temporal_filter_apply
#define vp8_temporal_filter_apply vp8_temporal_filter_apply_c
#endif
extern prototype_apply(vp8_temporal_filter_apply);

typedef struct
{
    prototype_apply(*apply);
} vp8_temporal_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define TEMPORAL_INVOKE(ctx,fn) (ctx)->fn
#else
#define TEMPORAL_INVOKE(ctx,fn) vp8_temporal_filter_##fn
#endif

#endif // __INC_VP8_TEMPORAL_FILTER_H
