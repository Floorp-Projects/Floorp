/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_HISTOGRAM_H_
#define WEBRTC_TEST_HISTOGRAM_H_

#include <string>

namespace webrtc {
namespace test {

// Returns the last added sample to a histogram (or -1 if the histogram is not
// found).
int LastHistogramSample(const std::string& name);

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_HISTOGRAM_H_

