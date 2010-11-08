/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef DETOKENIZE_ARM_H
#define DETOKENIZE_ARM_H

#if HAVE_ARMV6
#if CONFIG_ARM_ASM_DETOK
void vp8_init_detokenizer(VP8D_COMP *dx);
void vp8_decode_mb_tokens_v6(DETOK *detoken, int type);
#endif
#endif

#endif
