/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketConnectionBase_h
#define mozilla_net_WebSocketConnectionBase_h

// Architecture view:
//    Parent Process                        Socket Process
//  ┌─────────────────┐         IPC    ┌────────────────────────┐
//  │ WebSocketChannel│          ┌─────►WebSocketConnectionChild│
//  └─────────┬───────┘          │     └─────────┬──────────────┘
//            │                  │               │
// ┌──────────▼───────────────┐  │     ┌─────────▼─────────┐
// │ WebSocketConnectionParent├──┘     │WebSocketConnection│
// └──────────────────────────┘        └─────────┬─────────┘
//                                               │
//                                     ┌─────────▼─────────┐
//                                     │ nsSocketTransport │
//                                     └───────────────────┘
// The main reason that we need WebSocketConnectionBase is that we need to
// encapsulate nsSockTransport, nsSocketOutoutStream, and nsSocketInputStream.
// These three objects only live in socket process, so we provide the necessary
// interfaces in WebSocketConnectionBase and WebSocketConnectionListener for
// reading/writing the real socket.
//
// The output path when WebSocketChannel wants to write data to socket:
// - WebSocketConnectionParent::WriteOutputData
// - WebSocketConnectionParent::SendWriteOutputData
// - WebSocketConnectionChild::RecvWriteOutputData
// - WebSocketConnection::WriteOutputData
// - WebSocketConnection::OnOutputStreamReady (writes data to the real socket)
//
// The input path when data is able to read from the socket
// - WebSocketConnection::OnInputStreamReady
// - WebSocketConnectionChild::OnDataReceived
// - WebSocketConnectionChild::SendOnDataReceived
// - WebSocketConnectionParent::RecvOnDataReceived
// - WebSocketChannel::OnDataReceived
//
// The path that WebSocketConnection is constructed.
// - nsHttpChannel::OnStopRequest
// - HttpConnectionMgrShell::CompleteUpgrade
// - HttpConnectionMgrParent::CompleteUpgrade (we store the
//   nsIHttpUpgradeListener in a table and send an id to socket process)
// - HttpConnectionMgrParent::SendStartWebSocketConnection
// - HttpConnectionMgrChild::RecvStartWebSocketConnection (the listener id is
//   saved in WebSocketConnectionChild and will be used when the socket
//   transport is available)
// - WebSocketConnectionChild::Init (an IPC channel between socket thread in
//   socket process and background thread in parent process is created)
// - nsHttpConnectionMgr::CompleteUpgrade
// - WebSocketConnectionChild::OnTransportAvailable (WebSocketConnection is
//   created)
// - WebSocketConnectionChild::SendOnTransportAvailable
// - WebSocketConnectionParent::RecvOnTransportAvailable
// - WebSocketChannel::OnWebSocketConnectionAvailable

class nsITransportSecurityInfo;

namespace mozilla {
namespace net {

class WebSocketConnectionListener;

class WebSocketConnectionBase : public nsISupports {
 public:
  virtual nsresult Init(WebSocketConnectionListener* aListener) = 0;
  virtual void GetIoTarget(nsIEventTarget** aTarget) = 0;
  virtual void Close() = 0;
  virtual nsresult WriteOutputData(const uint8_t* aHdrBuf,
                                   uint32_t aHdrBufLength,
                                   const uint8_t* aPayloadBuf,
                                   uint32_t aPayloadBufLength) = 0;
  virtual nsresult StartReading() = 0;
  virtual void DrainSocketData() = 0;
  virtual nsresult GetSecurityInfo(
      nsITransportSecurityInfo** aSecurityInfo) = 0;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketConnectionBase_h
