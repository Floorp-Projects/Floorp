/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test_util.h"
#include "test_macros.h"
#include "rtp_dump.h"
#include <cmath>

using namespace webrtc;

// Normal Distribution
#define PI  3.14159265
double
NormalDist(double mean, double stdDev)
{
    // Creating a Normal distribution variable from two independent uniform
    // variables based on the Box-Muller transform
    double uniform1 = (std::rand() + 1.0) / (RAND_MAX + 1.0);
    double uniform2 = (std::rand() + 1.0) / (RAND_MAX + 1.0);
    return (mean + stdDev * sqrt(-2 * log(uniform1)) * cos(2 * PI * uniform2));
}

RTPVideoCodecTypes
ConvertCodecType(const char* plname)
{
    if (strncmp(plname,"VP8" , 3) == 0)
    {
        return kRTPVideoVP8;
    }
    else if (strncmp(plname,"I420" , 5) == 0)
    {
        return kRTPVideoI420;
    }
    else
    {
        return kRTPVideoNoVideo; // Default value
    }
}

