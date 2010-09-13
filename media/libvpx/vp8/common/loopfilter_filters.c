/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdlib.h>
#include "loopfilter.h"
#include "onyxc_int.h"

#ifdef __SUNPRO_C
#define __inline static inline
#endif

#define NEW_LOOPFILTER_MASK

typedef unsigned char uc;

static __inline signed char vp8_signed_char_clamp(int t)
{
    t = (t < -128 ? -128 : t);
    t = (t > 127 ? 127 : t);
    return (signed char) t;
}


// should we apply any filter at all ( 11111111 yes, 00000000 no)
static __inline signed char vp8_filter_mask(signed char limit, signed char flimit,
                                     uc p3, uc p2, uc p1, uc p0, uc q0, uc q1, uc q2, uc q3)
{
    signed char mask = 0;
    mask |= (abs(p3 - p2) > limit) * -1;
    mask |= (abs(p2 - p1) > limit) * -1;
    mask |= (abs(p1 - p0) > limit) * -1;
    mask |= (abs(q1 - q0) > limit) * -1;
    mask |= (abs(q2 - q1) > limit) * -1;
    mask |= (abs(q3 - q2) > limit) * -1;
#ifndef NEW_LOOPFILTER_MASK
    mask |= (abs(p0 - q0) > flimit) * -1;
#else
    mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > flimit * 2 + limit) * -1;
#endif
    mask = ~mask;
    return mask;
}

// is there high variance internal edge ( 11111111 yes, 00000000 no)
static __inline signed char vp8_hevmask(signed char thresh, uc p1, uc p0, uc q0, uc q1)
{
    signed char hev = 0;
    hev  |= (abs(p1 - p0) > thresh) * -1;
    hev  |= (abs(q1 - q0) > thresh) * -1;
    return hev;
}

static __inline void vp8_filter(signed char mask, signed char hev, uc *op1, uc *op0, uc *oq0, uc *oq1)

{
    signed char ps0, qs0;
    signed char ps1, qs1;
    signed char vp8_filter, Filter1, Filter2;
    signed char u;

    ps1 = (signed char) * op1 ^ 0x80;
    ps0 = (signed char) * op0 ^ 0x80;
    qs0 = (signed char) * oq0 ^ 0x80;
    qs1 = (signed char) * oq1 ^ 0x80;

    // add outer taps if we have high edge variance
    vp8_filter = vp8_signed_char_clamp(ps1 - qs1);
    vp8_filter &= hev;

    // inner taps
    vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * (qs0 - ps0));
    vp8_filter &= mask;

    // save bottom 3 bits so that we round one side +4 and the other +3
    // if it equals 4 we'll set to adjust by -1 to account for the fact
    // we'd round 3 the other way
    Filter1 = vp8_signed_char_clamp(vp8_filter + 4);
    Filter2 = vp8_signed_char_clamp(vp8_filter + 3);
    Filter1 >>= 3;
    Filter2 >>= 3;
    u = vp8_signed_char_clamp(qs0 - Filter1);
    *oq0 = u ^ 0x80;
    u = vp8_signed_char_clamp(ps0 + Filter2);
    *op0 = u ^ 0x80;
    vp8_filter = Filter1;

    // outer tap adjustments
    vp8_filter += 1;
    vp8_filter >>= 1;
    vp8_filter &= ~hev;

    u = vp8_signed_char_clamp(qs1 - vp8_filter);
    *oq1 = u ^ 0x80;
    u = vp8_signed_char_clamp(ps1 + vp8_filter);
    *op1 = u ^ 0x80;

}
void vp8_loop_filter_horizontal_edge_c
(
    unsigned char *s,
    int p, //pitch
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh,
    int count
)
{
    int  hev = 0; // high edge variance
    signed char mask = 0;
    int i = 0;

    // loop filter designed to work using chars so that we can make maximum use
    // of 8 bit simd instructions.
    do
    {
        mask = vp8_filter_mask(limit[i], flimit[i],
                               s[-4*p], s[-3*p], s[-2*p], s[-1*p],
                               s[0*p], s[1*p], s[2*p], s[3*p]);

        hev = vp8_hevmask(thresh[i], s[-2*p], s[-1*p], s[0*p], s[1*p]);

        vp8_filter(mask, hev, s - 2 * p, s - 1 * p, s, s + 1 * p);

        ++s;
    }
    while (++i < count * 8);
}

void vp8_loop_filter_vertical_edge_c
(
    unsigned char *s,
    int p,
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh,
    int count
)
{
    int  hev = 0; // high edge variance
    signed char mask = 0;
    int i = 0;

    // loop filter designed to work using chars so that we can make maximum use
    // of 8 bit simd instructions.
    do
    {
        mask = vp8_filter_mask(limit[i], flimit[i],
                               s[-4], s[-3], s[-2], s[-1], s[0], s[1], s[2], s[3]);

        hev = vp8_hevmask(thresh[i], s[-2], s[-1], s[0], s[1]);

        vp8_filter(mask, hev, s - 2, s - 1, s, s + 1);

        s += p;
    }
    while (++i < count * 8);
}

static __inline void vp8_mbfilter(signed char mask, signed char hev,
                           uc *op2, uc *op1, uc *op0, uc *oq0, uc *oq1, uc *oq2)
{
    signed char s, u;
    signed char vp8_filter, Filter1, Filter2;
    signed char ps2 = (signed char) * op2 ^ 0x80;
    signed char ps1 = (signed char) * op1 ^ 0x80;
    signed char ps0 = (signed char) * op0 ^ 0x80;
    signed char qs0 = (signed char) * oq0 ^ 0x80;
    signed char qs1 = (signed char) * oq1 ^ 0x80;
    signed char qs2 = (signed char) * oq2 ^ 0x80;

    // add outer taps if we have high edge variance
    vp8_filter = vp8_signed_char_clamp(ps1 - qs1);
    vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * (qs0 - ps0));
    vp8_filter &= mask;

    Filter2 = vp8_filter;
    Filter2 &= hev;

    // save bottom 3 bits so that we round one side +4 and the other +3
    Filter1 = vp8_signed_char_clamp(Filter2 + 4);
    Filter2 = vp8_signed_char_clamp(Filter2 + 3);
    Filter1 >>= 3;
    Filter2 >>= 3;
    qs0 = vp8_signed_char_clamp(qs0 - Filter1);
    ps0 = vp8_signed_char_clamp(ps0 + Filter2);


    // only apply wider filter if not high edge variance
    vp8_filter &= ~hev;
    Filter2 = vp8_filter;

    // roughly 3/7th difference across boundary
    u = vp8_signed_char_clamp((63 + Filter2 * 27) >> 7);
    s = vp8_signed_char_clamp(qs0 - u);
    *oq0 = s ^ 0x80;
    s = vp8_signed_char_clamp(ps0 + u);
    *op0 = s ^ 0x80;

    // roughly 2/7th difference across boundary
    u = vp8_signed_char_clamp((63 + Filter2 * 18) >> 7);
    s = vp8_signed_char_clamp(qs1 - u);
    *oq1 = s ^ 0x80;
    s = vp8_signed_char_clamp(ps1 + u);
    *op1 = s ^ 0x80;

    // roughly 1/7th difference across boundary
    u = vp8_signed_char_clamp((63 + Filter2 * 9) >> 7);
    s = vp8_signed_char_clamp(qs2 - u);
    *oq2 = s ^ 0x80;
    s = vp8_signed_char_clamp(ps2 + u);
    *op2 = s ^ 0x80;
}

void vp8_mbloop_filter_horizontal_edge_c
(
    unsigned char *s,
    int p,
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh,
    int count
)
{
    signed char hev = 0; // high edge variance
    signed char mask = 0;
    int i = 0;

    // loop filter designed to work using chars so that we can make maximum use
    // of 8 bit simd instructions.
    do
    {

        mask = vp8_filter_mask(limit[i], flimit[i],
                               s[-4*p], s[-3*p], s[-2*p], s[-1*p],
                               s[0*p], s[1*p], s[2*p], s[3*p]);

        hev = vp8_hevmask(thresh[i], s[-2*p], s[-1*p], s[0*p], s[1*p]);

        vp8_mbfilter(mask, hev, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p, s + 2 * p);

        ++s;
    }
    while (++i < count * 8);

}


void vp8_mbloop_filter_vertical_edge_c
(
    unsigned char *s,
    int p,
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh,
    int count
)
{
    signed char hev = 0; // high edge variance
    signed char mask = 0;
    int i = 0;

    do
    {

        mask = vp8_filter_mask(limit[i], flimit[i],
                               s[-4], s[-3], s[-2], s[-1], s[0], s[1], s[2], s[3]);

        hev = vp8_hevmask(thresh[i], s[-2], s[-1], s[0], s[1]);

        vp8_mbfilter(mask, hev, s - 3, s - 2, s - 1, s, s + 1, s + 2);

        s += p;
    }
    while (++i < count * 8);

}

// should we apply any filter at all ( 11111111 yes, 00000000 no)
static __inline signed char vp8_simple_filter_mask(signed char limit, signed char flimit, uc p1, uc p0, uc q0, uc q1)
{
// Why does this cause problems for win32?
// error C2143: syntax error : missing ';' before 'type'
//  (void) limit;
#ifndef NEW_LOOPFILTER_MASK
    signed char mask = (abs(p0 - q0) <= flimit) * -1;
#else
    signed char mask = (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  <= flimit * 2 + limit) * -1;
#endif
    return mask;
}

static __inline void vp8_simple_filter(signed char mask, uc *op1, uc *op0, uc *oq0, uc *oq1)
{
    signed char vp8_filter, Filter1, Filter2;
    signed char p1 = (signed char) * op1 ^ 0x80;
    signed char p0 = (signed char) * op0 ^ 0x80;
    signed char q0 = (signed char) * oq0 ^ 0x80;
    signed char q1 = (signed char) * oq1 ^ 0x80;
    signed char u;

    vp8_filter = vp8_signed_char_clamp(p1 - q1);
    vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * (q0 - p0));
    vp8_filter &= mask;

    // save bottom 3 bits so that we round one side +4 and the other +3
    Filter1 = vp8_signed_char_clamp(vp8_filter + 4);
    Filter1 >>= 3;
    u = vp8_signed_char_clamp(q0 - Filter1);
    *oq0  = u ^ 0x80;

    Filter2 = vp8_signed_char_clamp(vp8_filter + 3);
    Filter2 >>= 3;
    u = vp8_signed_char_clamp(p0 + Filter2);
    *op0 = u ^ 0x80;
}

void vp8_loop_filter_simple_horizontal_edge_c
(
    unsigned char *s,
    int p,
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh,
    int count
)
{
    signed char mask = 0;
    int i = 0;
    (void) thresh;

    do
    {
        //mask = vp8_simple_filter_mask( limit[i], flimit[i],s[-1*p],s[0*p]);
        mask = vp8_simple_filter_mask(limit[i], flimit[i], s[-2*p], s[-1*p], s[0*p], s[1*p]);
        vp8_simple_filter(mask, s - 2 * p, s - 1 * p, s, s + 1 * p);
        ++s;
    }
    while (++i < count * 8);
}

void vp8_loop_filter_simple_vertical_edge_c
(
    unsigned char *s,
    int p,
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh,
    int count
)
{
    signed char mask = 0;
    int i = 0;
    (void) thresh;

    do
    {
        //mask = vp8_simple_filter_mask( limit[i], flimit[i],s[-1],s[0]);
        mask = vp8_simple_filter_mask(limit[i], flimit[i], s[-2], s[-1], s[0], s[1]);
        vp8_simple_filter(mask, s - 2, s - 1, s, s + 1);
        s += p;
    }
    while (++i < count * 8);

}
