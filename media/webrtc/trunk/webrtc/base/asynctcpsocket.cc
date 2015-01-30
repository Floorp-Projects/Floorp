/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/asynctcpsocket.h"

#include <string.h>

#include "webrtc/base/byteorder.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"

#if defined(WEBRTC_POSIX)
#include <errno.h>
#endif  // WEBRTC_POSIX

namespace rtc {

static const size_t kMaxPacketSize = 64 * 1024;

typedef uint16 PacketLength;
static const size_t kPacketLenSize = sizeof(PacketLength);

static const size_t kBufSize = kMaxPacketSize + kPacketLenSize;

static const int kListenBacklog = 5;

// Binds and connects |socket|
AsyncSocket* AsyncTCPSocketBase::ConnectSocket(
    rtc::AsyncSocket* socket,
    const rtc::SocketAddress& bind_address,
    const rtc::SocketAddress& remote_address) {
  rtc::scoped_ptr<rtc::AsyncSocket> owned_socket(socket);
  if (socket->Bind(bind_address) < 0) {
    LOG(LS_ERROR) << "Bind() failed with error " << socket->GetError();
    return NULL;
  }
  if (socket->Connect(remote_address) < 0) {
    LOG(LS_ERROR) << "Connect() failed with error " << socket->GetError();
    return NULL;
  }
  return owned_socket.release();
}

AsyncTCPSocketBase::AsyncTCPSocketBase(AsyncSocket* socket, bool listen,
                                       size_t max_packet_size)
    : socket_(socket),
      listen_(listen),
      insize_(max_packet_size),
      inpos_(0),
      outsize_(max_packet_size),
      outpos_(0) {
  inbuf_ = new char[insize_];
  outbuf_ = new char[outsize_];

  ASSERT(socket_.get() != NULL);
  socket_->SignalConnectEvent.connect(
      this, &AsyncTCPSocketBase::OnConnectEvent);
  socket_->SignalReadEvent.connect(this, &AsyncTCPSocketBase::OnReadEvent);
  socket_->SignalWriteEvent.connect(this, &AsyncTCPSocketBase::OnWriteEvent);
  socket_->SignalCloseEvent.connect(this, &AsyncTCPSocketBase::OnCloseEvent);

  if (listen_) {
    if (socket_->Listen(kListenBacklog) < 0) {
      LOG(LS_ERROR) << "Listen() failed with error " << socket_->GetError();
    }
  }
}

AsyncTCPSocketBase::~AsyncTCPSocketBase() {
  delete [] inbuf_;
  delete [] outbuf_;
}

SocketAddress AsyncTCPSocketBase::GetLocalAddress() const {
  return socket_->GetLocalAddress();
}

SocketAddress AsyncTCPSocketBase::GetRemoteAddress() const {
  return socket_->GetRemoteAddress();
}

int AsyncTCPSocketBase::Close() {
  return socket_->Close();
}

AsyncTCPSocket::State AsyncTCPSocketBase::GetState() const {
  switch (socket_->GetState()) {
    case Socket::CS_CLOSED:
      return STATE_CLOSED;
    case Socket::CS_CONNECTING:
      if (listen_) {
        return STATE_BOUND;
      } else {
        return STATE_CONNECTING;
      }
    case Socket::CS_CONNECTED:
      return STATE_CONNECTED;
    default:
      ASSERT(false);
      return STATE_CLOSED;
  }
}

int AsyncTCPSocketBase::GetOption(Socket::Option opt, int* value) {
  return socket_->GetOption(opt, value);
}

int AsyncTCPSocketBase::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int AsyncTCPSocketBase::GetError() const {
  return socket_->GetError();
}

void AsyncTCPSocketBase::SetError(int error) {
  return socket_->SetError(error);
}

int AsyncTCPSocketBase::SendTo(const void *pv, size_t cb,
                               const SocketAddress& addr,
                               const rtc::PacketOptions& options) {
  if (addr == GetRemoteAddress())
    return Send(pv, cb, options);

  ASSERT(false);
  socket_->SetError(ENOTCONN);
  return -1;
}

int AsyncTCPSocketBase::SendRaw(const void * pv, size_t cb) {
  if (outpos_ + cb > outsize_) {
    socket_->SetError(EMSGSIZE);
    return -1;
  }

  memcpy(outbuf_ + outpos_, pv, cb);
  outpos_ += cb;

  return FlushOutBuffer();
}

int AsyncTCPSocketBase::FlushOutBuffer() {
  int res = socket_->Send(outbuf_, outpos_);
  if (res <= 0) {
    return res;
  }
  if (static_cast<size_t>(res) <= outpos_) {
    outpos_ -= res;
  } else {
    ASSERT(false);
    return -1;
  }
  if (outpos_ > 0) {
    memmove(outbuf_, outbuf_ + res, outpos_);
  }
  return res;
}

void AsyncTCPSocketBase::AppendToOutBuffer(const void* pv, size_t cb) {
  ASSERT(outpos_ + cb < outsize_);
  memcpy(outbuf_ + outpos_, pv, cb);
  outpos_ += cb;
}

void AsyncTCPSocketBase::OnConnectEvent(AsyncSocket* socket) {
  SignalConnect(this);
}

void AsyncTCPSocketBase::OnReadEvent(AsyncSocket* socket) {
  ASSERT(socket_.get() == socket);

  if (listen_) {
    rtc::SocketAddress address;
    rtc::AsyncSocket* new_socket = socket->Accept(&address);
    if (!new_socket) {
      // TODO: Do something better like forwarding the error
      // to the user.
      LOG(LS_ERROR) << "TCP accept failed with error " << socket_->GetError();
      return;
    }

    HandleIncomingConnection(new_socket);

    // Prime a read event in case data is waiting.
    new_socket->SignalReadEvent(new_socket);
  } else {
    int len = socket_->Recv(inbuf_ + inpos_, insize_ - inpos_);
    if (len < 0) {
      // TODO: Do something better like forwarding the error to the user.
      if (!socket_->IsBlocking()) {
        LOG(LS_ERROR) << "Recv() returned error: " << socket_->GetError();
      }
      return;
    }

    inpos_ += len;

    ProcessInput(inbuf_, &inpos_);

    if (inpos_ >= insize_) {
      LOG(LS_ERROR) << "input buffer overflow";
      ASSERT(false);
      inpos_ = 0;
    }
  }
}

void AsyncTCPSocketBase::OnWriteEvent(AsyncSocket* socket) {
  ASSERT(socket_.get() == socket);

  if (outpos_ > 0) {
    FlushOutBuffer();
  }

  if (outpos_ == 0) {
    SignalReadyToSend(this);
  }
}

void AsyncTCPSocketBase::OnCloseEvent(AsyncSocket* socket, int error) {
  SignalClose(this, error);
}

// AsyncTCPSocket
// Binds and connects |socket| and creates AsyncTCPSocket for
// it. Takes ownership of |socket|. Returns NULL if bind() or
// connect() fail (|socket| is destroyed in that case).
AsyncTCPSocket* AsyncTCPSocket::Create(
    AsyncSocket* socket,
    const SocketAddress& bind_address,
    const SocketAddress& remote_address) {
  return new AsyncTCPSocket(AsyncTCPSocketBase::ConnectSocket(
      socket, bind_address, remote_address), false);
}

AsyncTCPSocket::AsyncTCPSocket(AsyncSocket* socket, bool listen)
    : AsyncTCPSocketBase(socket, listen, kBufSize) {
}

int AsyncTCPSocket::Send(const void *pv, size_t cb,
                         const rtc::PacketOptions& options) {
  if (cb > kBufSize) {
    SetError(EMSGSIZE);
    return -1;
  }

  // If we are blocking on send, then silently drop this packet
  if (!IsOutBufferEmpty())
    return static_cast<int>(cb);

  PacketLength pkt_len = HostToNetwork16(static_cast<PacketLength>(cb));
  AppendToOutBuffer(&pkt_len, kPacketLenSize);
  AppendToOutBuffer(pv, cb);

  int res = FlushOutBuffer();
  if (res <= 0) {
    // drop packet if we made no progress
    ClearOutBuffer();
    return res;
  }

  // We claim to have sent the whole thing, even if we only sent partial
  return static_cast<int>(cb);
}

void AsyncTCPSocket::ProcessInput(char * data, size_t* len) {
  SocketAddress remote_addr(GetRemoteAddress());

  while (true) {
    if (*len < kPacketLenSize)
      return;

    PacketLength pkt_len = rtc::GetBE16(data);
    if (*len < kPacketLenSize + pkt_len)
      return;

    SignalReadPacket(this, data + kPacketLenSize, pkt_len, remote_addr,
                     CreatePacketTime(0));

    *len -= kPacketLenSize + pkt_len;
    if (*len > 0) {
      memmove(data, data + kPacketLenSize + pkt_len, *len);
    }
  }
}

void AsyncTCPSocket::HandleIncomingConnection(AsyncSocket* socket) {
  SignalNewConnection(this, new AsyncTCPSocket(socket, false));
}

}  // namespace rtc
