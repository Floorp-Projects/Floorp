/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ASpdySession_h
#define mozilla_net_ASpdySession_h

#include "nsAHttpTransaction.h"
#include "prinrval.h"
#include "nsString.h"

class nsISocketTransport;

namespace mozilla {
namespace net {

class ASpdySession : public nsAHttpTransaction {
 public:
  ASpdySession() = default;
  virtual ~ASpdySession() = default;

  virtual MOZ_MUST_USE bool AddStream(nsAHttpTransaction*, int32_t, bool, bool,
                                      nsIInterfaceRequestor*) = 0;
  virtual bool CanReuse() = 0;
  virtual bool RoomForMoreStreams() = 0;
  virtual PRIntervalTime IdleTime() = 0;
  virtual uint32_t ReadTimeoutTick(PRIntervalTime now) = 0;
  virtual void DontReuse() = 0;
  virtual enum SpdyVersion SpdyVersion() = 0;

  static ASpdySession* NewSpdySession(net::SpdyVersion version,
                                      nsISocketTransport*, bool);

  virtual bool TestJoinConnection(const nsACString& hostname, int32_t port) = 0;
  virtual bool JoinConnection(const nsACString& hostname, int32_t port) = 0;

  // MaybeReTunnel() is called by the connection manager when it cannot
  // dispatch a tunneled transaction. That might be because the tunnels it
  // expects to see are dead (and we may or may not be able to make more),
  // or it might just need to wait longer for one of them to become free.
  //
  // return true if the session takes back ownership of the transaction from
  // the connection manager.
  virtual bool MaybeReTunnel(nsAHttpTransaction*) = 0;

  virtual void PrintDiagnostics(nsCString& log) = 0;

  bool ResponseTimeoutEnabled() const final { return true; }

  virtual void SendPing() = 0;

  const static uint32_t kSendingChunkSize = 4095;
  const static uint32_t kTCPSendBufferSize = 131072;

  // This is roughly the amount of data a suspended channel will have to
  // buffer before h2 flow control kicks in.
  const static uint32_t kInitialRwin = 12 * 1024 * 1024;  // 12MB

  const static uint32_t kDefaultMaxConcurrent = 100;

  // soft errors are errors that terminate a stream without terminating the
  // connection. In general non-network errors are stream errors as well
  // as network specific items like cancels.
  bool SoftStreamError(nsresult code) {
    if (NS_SUCCEEDED(code) || code == NS_BASE_STREAM_WOULD_BLOCK) {
      return false;
    }

    // this could go either way, but because there are network instances of
    // it being a hard error we should consider it hard.
    if (code == NS_ERROR_FAILURE || code == NS_ERROR_OUT_OF_MEMORY) {
      return false;
    }

    if (NS_ERROR_GET_MODULE(code) != NS_ERROR_MODULE_NETWORK) {
      return true;
    }

    // these are network specific soft errors
    return (code == NS_BASE_STREAM_CLOSED || code == NS_BINDING_FAILED ||
            code == NS_BINDING_ABORTED || code == NS_BINDING_REDIRECTED ||
            code == NS_ERROR_INVALID_CONTENT_ENCODING ||
            code == NS_BINDING_RETARGETED ||
            code == NS_ERROR_CORRUPTED_CONTENT);
  }

  virtual void SetCleanShutdown(bool) = 0;
  virtual bool CanAcceptWebsocket() = 0;
};

typedef bool (*ALPNCallback)(nsISupports*);  // nsISSLSocketControl is typical

// this is essentially a single instantiation as a member of nsHttpHandler.
// It could be all static except using static ctors of XPCOM objects is a
// bad idea.
class SpdyInformation {
 public:
  SpdyInformation();
  ~SpdyInformation() = default;

  static const uint32_t kCount = 1;

  // determine the index (0..kCount-1) of the spdy information that
  // correlates to the npn string. NS_FAILED() if no match is found.
  MOZ_MUST_USE nsresult GetNPNIndex(const nsACString& npnString,
                                    uint32_t* result) const;

  // determine if a version of the protocol is enabled for index < kCount
  bool ProtocolEnabled(uint32_t index) const;

  SpdyVersion Version[kCount];      // telemetry enum e.g. SPDY_VERSION_31
  nsCString VersionString[kCount];  // npn string e.g. "spdy/3.1"

  // the ALPNCallback function allows the protocol stack to decide whether or
  // not to offer a particular protocol based on the known TLS information
  // that we will offer in the client hello (such as version). There has
  // not been a Server Hello received yet, so not much else can be considered.
  ALPNCallback ALPNCallbacks[kCount];
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ASpdySession_h
