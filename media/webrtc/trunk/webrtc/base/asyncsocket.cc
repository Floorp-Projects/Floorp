/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/asyncsocket.h"

namespace rtc {

AsyncSocket::AsyncSocket() {
}

AsyncSocket::~AsyncSocket() {
}

AsyncSocketAdapter::AsyncSocketAdapter(AsyncSocket* socket) : socket_(NULL) {
  Attach(socket);
}

AsyncSocketAdapter::~AsyncSocketAdapter() {
  delete socket_;
}

void AsyncSocketAdapter::Attach(AsyncSocket* socket) {
  ASSERT(!socket_);
  socket_ = socket;
  if (socket_) {
    socket_->SignalConnectEvent.connect(this,
        &AsyncSocketAdapter::OnConnectEvent);
    socket_->SignalReadEvent.connect(this,
        &AsyncSocketAdapter::OnReadEvent);
    socket_->SignalWriteEvent.connect(this,
        &AsyncSocketAdapter::OnWriteEvent);
    socket_->SignalCloseEvent.connect(this,
        &AsyncSocketAdapter::OnCloseEvent);
  }
}

}  // namespace rtc
