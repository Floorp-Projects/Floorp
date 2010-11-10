/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef loopfilter_h
#define loopfilter_h

#include "vpx_ports/mem.h"

#define MAX_LOOP_FILTER 63

typedef enum
{
    NORMAL_LOOPFILTER = 0,
    SIMPLE_LOOPFILTER = 1
} LOOPFILTERTYPE;

/* FRK
 * Need to align this structure so when it is declared and
 * passed it can be loaded into vector registers.
 */
typedef struct
{
    DECLARE_ALIGNED(16, signed char, lim[16]);
    DECLARE_ALIGNED(16, signed char, flim[16]);
    DECLARE_ALIGNED(16, signed char, thr[16]);
    DECLARE_ALIGNED(16, signed char, mbflim[16]);
    DECLARE_ALIGNED(16, signed char, mbthr[16]);
    DECLARE_ALIGNED(16, signed char, uvlim[16]);
    DECLARE_ALIGNED(16, signed char, uvflim[16]);
    DECLARE_ALIGNED(16, signed char, uvthr[16]);
    DECLARE_ALIGNED(16, signed char, uvmbflim[16]);
    DECLARE_ALIGNED(16, signed char, uvmbthr[16]);
} loop_filter_info;


#define prototype_loopfilter(sym) \
    void sym(unsigned char *src, int pitch, const signed char *flimit,\
             const signed char *limit, const signed char *thresh, int count)

#define prototype_loopfilter_block(sym) \
    void sym(unsigned char *y, unsigned char *u, unsigned char *v,\
             int ystride, int uv_stride, loop_filter_info *lfi, int simpler)

#if ARCH_X86 || ARCH_X86_64
#include "x86/loopfilter_x86.h"
#endif

#if ARCH_ARM
#include "arm/loopfilter_arm.h"
#endif

#ifndef vp8_lf_normal_mb_v
#define vp8_lf_normal_mb_v vp8_loop_filter_mbv_c
#endif
extern prototype_loopfilter_block(vp8_lf_normal_mb_v);

#ifndef vp8_lf_normal_b_v
#define vp8_lf_normal_b_v vp8_loop_filter_bv_c
#endif
extern prototype_loopfilter_block(vp8_lf_normal_b_v);

#ifndef vp8_lf_normal_mb_h
#define vp8_lf_normal_mb_h vp8_loop_filter_mbh_c
#endif
extern prototype_loopfilter_block(vp8_lf_normal_mb_h);

#ifndef vp8_lf_normal_b_h
#define vp8_lf_normal_b_h vp8_loop_filter_bh_c
#endif
extern prototype_loopfilter_block(vp8_lf_normal_b_h);


#ifndef vp8_lf_simple_mb_v
#define vp8_lf_simple_mb_v vp8_loop_filter_mbvs_c
#endif
extern prototype_loopfilter_block(vp8_lf_simple_mb_v);

#ifndef vp8_lf_simple_b_v
#define vp8_lf_simple_b_v vp8_loop_filter_bvs_c
#endif
extern prototype_loopfilter_block(vp8_lf_simple_b_v);

#ifndef vp8_lf_simple_mb_h
#define vp8_lf_simple_mb_h vp8_loop_filter_mbhs_c
#endif
extern prototype_loopfilter_block(vp8_lf_simple_mb_h);

#ifndef vp8_lf_simple_b_h
#define vp8_lf_simple_b_h vp8_loop_filter_bhs_c
#endif
extern prototype_loopfilter_block(vp8_lf_simple_b_h);

typedef prototype_loopfilter_block((*vp8_lf_block_fn_t));
typedef struct
{
    vp8_lf_block_fn_t  normal_mb_v;
    vp8_lf_block_fn_t  normal_b_v;
    vp8_lf_block_fn_t  normal_mb_h;
    vp8_lf_block_fn_t  normal_b_h;
    vp8_lf_block_fn_t  simple_mb_v;
    vp8_lf_block_fn_t  simple_b_v;
    vp8_lf_block_fn_t  simple_mb_h;
    vp8_lf_block_fn_t  simple_b_h;
} vp8_loopfilter_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define LF_INVOKE(ctx,fn) (ctx)->fn
#else
#define LF_INVOKE(ctx,fn) vp8_lf_##fn
#endif

typedef void loop_filter_uvfunction
(
    unsigned char *u,   /* source pointer */
    int p,              /* pitch */
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh,
    unsigned char *v
);

#endif
