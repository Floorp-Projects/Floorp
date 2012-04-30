/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef FILTER_X86_H
#define FILTER_X86_H

/* x86 assembly specific copy of vp8/common/filter.c:vp8_bilinear_filters with
 * duplicated values */
extern const short vp8_bilinear_filters_x86_4[8][8];  /* duplicated 4x */
extern const short vp8_bilinear_filters_x86_8[8][16]; /* duplicated 8x */

#endif /* FILTER_X86_H */
