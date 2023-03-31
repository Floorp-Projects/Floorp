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

#ifndef API_RTP_SENDER_SETPARAMETERS_CALLBACK_H_
#define API_RTP_SENDER_SETPARAMETERS_CALLBACK_H_

#include "api/rtc_error.h"
#include "absl/functional/any_invocable.h"

namespace webrtc {

using SetParametersCallback = absl::AnyInvocable<void(RTCError) &&>;

webrtc::RTCError InvokeSetParametersCallback(SetParametersCallback& callback,
                                             RTCError error);
} // namespace webrtc

#endif  // API_RTP_SENDER_SETPARAMETERS_CALLBACK_H_
