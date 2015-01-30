/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_NULLSOCKETSERVER_H_
#define WEBRTC_BASE_NULLSOCKETSERVER_H_

#include "webrtc/base/event.h"
#include "webrtc/base/physicalsocketserver.h"

namespace rtc {

// NullSocketServer

class NullSocketServer : public rtc::SocketServer {
 public:
  NullSocketServer() : event_(false, false) {}

  virtual bool Wait(int cms, bool process_io) {
    event_.Wait(cms);
    return true;
  }

  virtual void WakeUp() {
    event_.Set();
  }

  virtual rtc::Socket* CreateSocket(int type) {
    ASSERT(false);
    return NULL;
  }

  virtual rtc::Socket* CreateSocket(int family, int type) {
    ASSERT(false);
    return NULL;
  }

  virtual rtc::AsyncSocket* CreateAsyncSocket(int type) {
    ASSERT(false);
    return NULL;
  }

  virtual rtc::AsyncSocket* CreateAsyncSocket(int family, int type) {
    ASSERT(false);
    return NULL;
  }


 private:
  rtc::Event event_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_NULLSOCKETSERVER_H_
