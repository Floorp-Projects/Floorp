/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/proxy.h"

#include "rtc_base/trace_event.h"

namespace webrtc {
namespace proxy_internal {
void TraceApiCall(const char* class_name, const char* method_name) {
  TRACE_EVENT1("webrtc", class_name, "method", method_name);
}
}  // namespace proxy_internal
}  // namespace webrtc
