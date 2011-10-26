/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef __DEFAULTCOEFCOUNTS_H
#define __DEFAULTCOEFCOUNTS_H

#include "entropy.h"

extern const unsigned int vp8_default_coef_counts[BLOCK_TYPES]
                                                 [COEF_BANDS]
                                                 [PREV_COEF_CONTEXTS]
                                                 [MAX_ENTROPY_TOKENS];

#endif //__DEFAULTCOEFCOUNTS_H
