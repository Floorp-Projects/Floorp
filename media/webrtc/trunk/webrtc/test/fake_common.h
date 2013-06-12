/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_FAKE_COMMON_H_
#define WEBRTC_TEST_FAKE_COMMON_H_

// Borrowed from libjingle's talk/media/webrtc/fakewebrtccommon.h.

#include "webrtc/typedefs.h"

#define WEBRTC_STUB(method, args) \
  virtual int method args OVERRIDE { return 0; }

#define WEBRTC_STUB_CONST(method, args) \
  virtual int method args const OVERRIDE { return 0; }

#define WEBRTC_BOOL_STUB(method, args) \
  virtual bool method args OVERRIDE { return true; }

#define WEBRTC_VOID_STUB(method, args) \
  virtual void method args OVERRIDE {}

#define WEBRTC_FUNC(method, args) \
  virtual int method args OVERRIDE

#define WEBRTC_FUNC_CONST(method, args) \
  virtual int method args const OVERRIDE

#define WEBRTC_BOOL_FUNC(method, args) \
  virtual bool method args OVERRIDE

#define WEBRTC_VOID_FUNC(method, args) \
  virtual void method args OVERRIDE

#define WEBRTC_CHECK_CHANNEL(channel) \
  if (channels_.find(channel) == channels_.end()) return -1;

#define WEBRTC_ASSERT_CHANNEL(channel) \
  ASSERT(channels_.find(channel) != channels_.end());

#endif  // WEBRTC_TEST_FAKE_COMMON_H_
