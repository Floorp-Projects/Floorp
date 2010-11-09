/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdlib.h>
#include "blockd.h"


void vp8_predict_dc(short *lastdc, short *thisdc, short quant, short *cons)
{
    int diff;
    int sign;
    int last_dc = *lastdc;
    int this_dc = *thisdc;

    if (*cons  > DCPREDCNTTHRESH)
    {
        this_dc += last_dc;
    }

    diff = abs(last_dc - this_dc);
    sign  = (last_dc >> 31) ^(this_dc >> 31);
    sign |= (!last_dc | !this_dc);

    if (sign)
    {
        *cons = 0;
    }
    else
    {
        if (diff <= DCPREDSIMTHRESH * quant)
            (*cons)++ ;
    }

    *thisdc = this_dc;
    *lastdc = this_dc;
}
