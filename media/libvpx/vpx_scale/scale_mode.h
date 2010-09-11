/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/****************************************************************************
*
*****************************************************************************
*/

#ifndef SCALE_MODE_H
#define SCALE_MODE_H

typedef enum
{
    MAINTAIN_ASPECT_RATIO   = 0x0,
    SCALE_TO_FIT            = 0x1,
    CENTER                  = 0x2,
    OTHER                   = 0x3
} SCALE_MODE;


#endif
