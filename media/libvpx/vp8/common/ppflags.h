/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_PPFLAGS_H
#define __INC_PPFLAGS_H
enum
{
    VP8D_NOFILTERING    = 0,
    VP8D_DEBLOCK        = 1<<0,
    VP8D_DEMACROBLOCK   = 1<<1,
    VP8D_ADDNOISE       = 1<<2,
    VP8D_DEBUG_LEVEL1   = 1<<3,
    VP8D_DEBUG_LEVEL2   = 1<<4,
    VP8D_DEBUG_LEVEL3   = 1<<5,
    VP8D_DEBUG_LEVEL4   = 1<<6,
    VP8D_DEBUG_LEVEL5   = 1<<7,
    VP8D_DEBUG_LEVEL6   = 1<<8,
    VP8D_DEBUG_LEVEL7   = 1<<9
};

#endif
