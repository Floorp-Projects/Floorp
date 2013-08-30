/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_TRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_TRANSPORT_H_

#include <stddef.h>

namespace webrtc {
namespace newapi {

class Transport {
 public:
  virtual bool SendRTP(const void* packet, size_t length) = 0;
  virtual bool SendRTCP(const void* packet, size_t length) = 0;

 protected:
  virtual ~Transport() {}
};
}  // namespace newapi
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_TRANSPORT_H_
