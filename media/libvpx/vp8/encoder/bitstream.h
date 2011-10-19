/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_BITSTREAM_H
#define __INC_BITSTREAM_H

#if HAVE_ARMV5TE
void vp8cx_pack_tokens_armv5(vp8_writer *w, const TOKENEXTRA *p, int xcount,
                             vp8_token *,
                             vp8_extra_bit_struct *,
                             const vp8_tree_index *);
void vp8cx_pack_tokens_into_partitions_armv5(VP8_COMP *, unsigned char *, int , int *,
        vp8_token *,
        vp8_extra_bit_struct *,
        const vp8_tree_index *);
void vp8cx_pack_mb_row_tokens_armv5(VP8_COMP *cpi, vp8_writer *w,
                                    vp8_token *,
                                    vp8_extra_bit_struct *,
                                    const vp8_tree_index *);
# define pack_tokens(a,b,c)                  \
    vp8cx_pack_tokens_armv5(a,b,c,vp8_coef_encodings,vp8_extra_bits,vp8_coef_tree)
# define pack_tokens_into_partitions(a,b,c,d)  \
    vp8cx_pack_tokens_into_partitions_armv5(a,b,c,d,vp8_coef_encodings,vp8_extra_bits,vp8_coef_tree)
# define pack_mb_row_tokens(a,b)               \
    vp8cx_pack_mb_row_tokens_armv5(a,b,vp8_coef_encodings,vp8_extra_bits,vp8_coef_tree)
#else
# define pack_tokens(a,b,c)                  pack_tokens_c(a,b,c)
# define pack_tokens_into_partitions(a,b,c,d)  pack_tokens_into_partitions_c(a,b,c,d)
# define pack_mb_row_tokens(a,b)               pack_mb_row_tokens_c(a,b)
#endif
#endif
