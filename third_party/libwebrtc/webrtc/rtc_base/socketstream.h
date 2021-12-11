/*
 *  Copyright 2005 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SOCKETSTREAM_H_
#define RTC_BASE_SOCKETSTREAM_H_

#include "rtc_base/asyncsocket.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/stream.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////

class SocketStream : public StreamInterface, public sigslot::has_slots<> {
 public:
  explicit SocketStream(AsyncSocket* socket);
  ~SocketStream() override;

  void Attach(AsyncSocket* socket);
  AsyncSocket* Detach();

  AsyncSocket* GetSocket() { return socket_; }

  StreamState GetState() const override;

  StreamResult Read(void* buffer,
                    size_t buffer_len,
                    size_t* read,
                    int* error) override;

  StreamResult Write(const void* data,
                     size_t data_len,
                     size_t* written,
                     int* error) override;

  void Close() override;

 private:
  void OnConnectEvent(AsyncSocket* socket);
  void OnReadEvent(AsyncSocket* socket);
  void OnWriteEvent(AsyncSocket* socket);
  void OnCloseEvent(AsyncSocket* socket, int err);

  AsyncSocket* socket_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SocketStream);
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace rtc

#endif  // RTC_BASE_SOCKETSTREAM_H_
