/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "exp_filter.h"

#include <math.h>

namespace webrtc {

void
VCMExpFilter::Reset(float alpha)
{
    _alpha = alpha;
    _filtered = -1.0;
}

float
VCMExpFilter::Apply(float exp, float sample)
{
    if (_filtered == -1.0)
    {
        // Initialize filtered bit rates
        _filtered = sample;
    }
    else if (exp == 1.0)
    {
        _filtered = _alpha * _filtered + (1 - _alpha) * sample;
    }
    else
    {
        float alpha = pow(_alpha, exp);
        _filtered = alpha * _filtered + (1 - alpha) * sample;
    }
    if (_max != -1 && _filtered > _max)
    {
        _filtered = _max;
    }
    return _filtered;
}

void
VCMExpFilter::UpdateBase(float alpha)
{
    _alpha = alpha;
}

float
VCMExpFilter::Value() const
{
    return _filtered;
}

}
