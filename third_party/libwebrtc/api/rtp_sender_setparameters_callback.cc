/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// File added by mozilla, to decouple this from libwebrtc's implementation of
// RTCRtpSender.

#include "api/rtp_sender_setparameters_callback.h"

namespace webrtc {

webrtc::RTCError InvokeSetParametersCallback(SetParametersCallback& callback,
                                             RTCError error) {
  if (callback) {
    std::move(callback)(error);
    callback = nullptr;
  }
  return error;
}

} // namespace webrtc
