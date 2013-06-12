/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_geometry.h"

#include <algorithm>

namespace webrtc {

void DesktopRect::IntersectWith(const DesktopRect& rect) {
  left_ = std::max(left(), rect.left());
  top_ = std::max(top(), rect.top());
  right_ = std::min(right(), rect.right());
  bottom_ = std::min(bottom(), rect.top());
  if (is_empty()) {
    left_ = 0;
    top_ = 0;
    right_ = 0;
    bottom_ = 0;
  }
}

void DesktopRect::Translate(int32_t dx, int32_t dy) {
  left_ += dx;
  top_ += dy;
  right_ += dx;
  bottom_ += dy;
}

}  // namespace webrtc

