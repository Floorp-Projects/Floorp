/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef DBOOLHUFF_H
#define DBOOLHUFF_H
#include <stddef.h>
#include <limits.h>
#include "vpx_ports/config.h"
#include "vpx_ports/mem.h"
#include "vpx/vpx_integer.h"

typedef size_t VP8_BD_VALUE;

# define VP8_BD_VALUE_SIZE ((int)sizeof(VP8_BD_VALUE)*CHAR_BIT)
/*This is meant to be a large, positive constant that can still be efficiently
   loaded as an immediate (on platforms like ARM, for example).
  Even relatively modest values like 100 would work fine.*/
# define VP8_LOTS_OF_BITS (0x40000000)



struct vp8_dboolhuff_rtcd_vtable;

typedef struct
{
    const unsigned char *user_buffer_end;
    const unsigned char *user_buffer;
    VP8_BD_VALUE         value;
    int                  count;
    unsigned int         range;
#if CONFIG_RUNTIME_CPU_DETECT
    struct vp8_dboolhuff_rtcd_vtable *rtcd;
#endif
} BOOL_DECODER;

#define prototype_dbool_start(sym) int sym(BOOL_DECODER *br, \
    const unsigned char *source, unsigned int source_sz)
#define prototype_dbool_fill(sym) void sym(BOOL_DECODER *br)
#define prototype_dbool_debool(sym) int sym(BOOL_DECODER *br, int probability)
#define prototype_dbool_devalue(sym) int sym(BOOL_DECODER *br, int bits);

#if ARCH_ARM
#include "arm/dboolhuff_arm.h"
#endif

#ifndef vp8_dbool_start
#define vp8_dbool_start vp8dx_start_decode_c
#endif

#ifndef vp8_dbool_fill
#define vp8_dbool_fill vp8dx_bool_decoder_fill_c
#endif

#ifndef vp8_dbool_debool
#define vp8_dbool_debool vp8dx_decode_bool_c
#endif

#ifndef vp8_dbool_devalue
#define vp8_dbool_devalue vp8dx_decode_value_c
#endif

extern prototype_dbool_start(vp8_dbool_start);
extern prototype_dbool_fill(vp8_dbool_fill);
extern prototype_dbool_debool(vp8_dbool_debool);
extern prototype_dbool_devalue(vp8_dbool_devalue);

typedef prototype_dbool_start((*vp8_dbool_start_fn_t));
typedef prototype_dbool_fill((*vp8_dbool_fill_fn_t));
typedef prototype_dbool_debool((*vp8_dbool_debool_fn_t));
typedef prototype_dbool_devalue((*vp8_dbool_devalue_fn_t));

typedef struct vp8_dboolhuff_rtcd_vtable {
    vp8_dbool_start_fn_t   start;
    vp8_dbool_fill_fn_t    fill;
    vp8_dbool_debool_fn_t  debool;
    vp8_dbool_devalue_fn_t devalue;
} vp8_dboolhuff_rtcd_vtable_t;

// There are no processor-specific versions of these
// functions right now. Disable RTCD to avoid using
// function pointers which gives a speed boost
//#ifdef ENABLE_RUNTIME_CPU_DETECT
//#define DBOOLHUFF_INVOKE(ctx,fn) (ctx)->fn
//#define IF_RTCD(x) (x)
//#else
#define DBOOLHUFF_INVOKE(ctx,fn) vp8_dbool_##fn
#define IF_RTCD(x) NULL
//#endif

DECLARE_ALIGNED(16, extern const unsigned char, vp8dx_bitreader_norm[256]);

/* wrapper functions to hide RTCD. static means inline means hopefully no
 * penalty
 */
static int vp8dx_start_decode(BOOL_DECODER *br,
        struct vp8_dboolhuff_rtcd_vtable *rtcd,
        const unsigned char *source, unsigned int source_sz) {
#if CONFIG_RUNTIME_CPU_DETECT
    br->rtcd = rtcd;
#endif
    return DBOOLHUFF_INVOKE(rtcd, start)(br, source, source_sz);
}
static void vp8dx_bool_decoder_fill(BOOL_DECODER *br) {
    DBOOLHUFF_INVOKE(br->rtcd, fill)(br);
}

/*The refill loop is used in several places, so define it in a macro to make
   sure they're all consistent.
  An inline function would be cleaner, but has a significant penalty, because
   multiple BOOL_DECODER fields must be modified, and the compiler is not smart
   enough to eliminate the stores to those fields and the subsequent reloads
   from them when inlining the function.*/
#define VP8DX_BOOL_DECODER_FILL(_count,_value,_bufptr,_bufend) \
    do \
    { \
        int shift; \
        for(shift = VP8_BD_VALUE_SIZE - 8 - ((_count) + 8); shift >= 0; ) \
        { \
            if((_bufptr) >= (_bufend)) { \
                (_count) = VP8_LOTS_OF_BITS; \
                break; \
            } \
            (_count) += 8; \
            (_value) |= (VP8_BD_VALUE)*(_bufptr)++ << shift; \
            shift -= 8; \
        } \
    } \
    while(0)


static int vp8dx_decode_bool(BOOL_DECODER *br, int probability) {
  /*
   * Until optimized versions of this function are available, we
   * keep the implementation in the header to allow inlining.
   *
   *return DBOOLHUFF_INVOKE(br->rtcd, debool)(br, probability);
   */
    unsigned int bit = 0;
    VP8_BD_VALUE value;
    unsigned int split;
    VP8_BD_VALUE bigsplit;
    int count;
    unsigned int range;

    value = br->value;
    count = br->count;
    range = br->range;

    split = 1 + (((range - 1) * probability) >> 8);
    bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8);

    range = split;

    if (value >= bigsplit)
    {
        range = br->range - split;
        value = value - bigsplit;
        bit = 1;
    }

    /*if(range>=0x80)
    {
        br->value = value;
        br->range = range;
        return bit
    }*/

    {
        register unsigned int shift = vp8dx_bitreader_norm[range];
        range <<= shift;
        value <<= shift;
        count -= shift;
    }
    br->value = value;
    br->count = count;
    br->range = range;
    if(count < 0)
        vp8dx_bool_decoder_fill(br);
    return bit;
}

static int vp8_decode_value(BOOL_DECODER *br, int bits)
{
  /*
   * Until optimized versions of this function are available, we
   * keep the implementation in the header to allow inlining.
   *
   *return DBOOLHUFF_INVOKE(br->rtcd, devalue)(br, bits);
   */
    int z = 0;
    int bit;

    for (bit = bits - 1; bit >= 0; bit--)
    {
        z |= (vp8dx_decode_bool(br, 0x80) << bit);
    }

    return z;
}
#endif
