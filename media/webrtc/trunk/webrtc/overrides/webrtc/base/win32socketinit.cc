/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Redirect Libjingle's winsock initialization activity into Chromium's
// singleton object that managest precisely that for the browser.

#include "webrtc/base/win32socketinit.h"

#include "net/base/winsock_init.h"

#if !defined(WEBRTC_WIN)
#error "Only compile this on Windows"
#endif

namespace rtc {

void EnsureWinsockInit() {
  net::EnsureWinsockInit();
}

}  // namespace rtc
