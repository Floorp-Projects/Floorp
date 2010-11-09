/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef tree_reader_h
#define tree_reader_h 1

#include "treecoder.h"

#include "dboolhuff.h"

typedef BOOL_DECODER vp8_reader;

#define vp8_read vp8dx_decode_bool
#define vp8_read_literal vp8_decode_value
#define vp8_read_bit( R) vp8_read( R, vp8_prob_half)


/* Intent of tree data structure is to make decoding trivial. */

static int vp8_treed_read(
    vp8_reader *const r,        /* !!! must return a 0 or 1 !!! */
    vp8_tree t,
    const vp8_prob *const p
)
{
    register vp8_tree_index i = 0;

    while ((i = t[ i + vp8_read(r, p[i>>1])]) > 0) ;

    return -i;
}


/* Variant reads a binary number given distributions on each bit.
   Note that tree is arbitrary; probability of decoding a zero
   may or may not depend on previously decoded bits. */

static int vp8_treed_read_num(
    vp8_reader *const r,        /* !!! must return a 0 or 1 !!! */
    vp8_tree t,
    const vp8_prob *const p
)
{
    vp8_tree_index i = 0;
    int v = 0, b;

    do
    {
        b = vp8_read(r, p[i>>1]);
        v = (v << 1) + b;
    }
    while ((i = t[i+b]) > 0);

    return v;
}
#endif /* tree_reader_h */
