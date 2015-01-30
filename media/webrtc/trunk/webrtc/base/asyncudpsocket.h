/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ASYNCUDPSOCKET_H_
#define WEBRTC_BASE_ASYNCUDPSOCKET_H_

#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socketfactory.h"

namespace rtc {

// Provides the ability to receive packets asynchronously.  Sends are not
// buffered since it is acceptable to drop packets under high load.
class AsyncUDPSocket : public AsyncPacketSocket {
 public:
  // Binds |socket| and creates AsyncUDPSocket for it. Takes ownership
  // of |socket|. Returns NULL if bind() fails (|socket| is destroyed
  // in that case).
  static AsyncUDPSocket* Create(AsyncSocket* socket,
                                const SocketAddress& bind_address);
  // Creates a new socket for sending asynchronous UDP packets using an
  // asynchronous socket from the given factory.
  static AsyncUDPSocket* Create(SocketFactory* factory,
                                const SocketAddress& bind_address);
  explicit AsyncUDPSocket(AsyncSocket* socket);
  virtual ~AsyncUDPSocket();

  virtual SocketAddress GetLocalAddress() const;
  virtual SocketAddress GetRemoteAddress() const;
  virtual int Send(const void *pv, size_t cb,
                   const rtc::PacketOptions& options);
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr,
                     const rtc::PacketOptions& options);
  virtual int Close();

  virtual State GetState() const;
  virtual int GetOption(Socket::Option opt, int* value);
  virtual int SetOption(Socket::Option opt, int value);
  virtual int GetError() const;
  virtual void SetError(int error);

 private:
  // Called when the underlying socket is ready to be read from.
  void OnReadEvent(AsyncSocket* socket);
  // Called when the underlying socket is ready to send.
  void OnWriteEvent(AsyncSocket* socket);

  scoped_ptr<AsyncSocket> socket_;
  char* buf_;
  size_t size_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_ASYNCUDPSOCKET_H_
