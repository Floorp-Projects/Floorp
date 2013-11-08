/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_INCLUDE_EXP_FILTER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_INCLUDE_EXP_FILTER_H_

namespace webrtc
{

/**********************/
/* ExpFilter class    */
/**********************/

class VCMExpFilter
{
public:
    VCMExpFilter(float alpha, float max = -1.0) : _alpha(alpha), _filtered(-1.0), _max(max) {}

    // Resets the filter to its initial state, and resets alpha to the given value
    //
    // Input:
    //          - alpha     : the new value of the filter factor base.
    void Reset(float alpha);

    // Applies the filter with the given exponent on the provided sample
    //
    // Input:
    //          - exp       : Exponent T in y(k) = alpha^T * y(k-1) + (1 - alpha^T) * x(k)
    //          - sample    : x(k) in the above filter equation
    float Apply(float exp, float sample);

    // Return current filtered value: y(k)
    //
    // Return value         : The current filter output
    float Value() const;

    // Change the filter factor base
    //
    // Input:
    //          - alpha     : The new filter factor base.
    void UpdateBase(float alpha);

private:
    float          _alpha;     // Filter factor base
    float          _filtered;  // Current filter output
    const float    _max;
}; // end of ExpFilter class

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_UTILITY_INCLUDE_EXP_FILTER_H_
