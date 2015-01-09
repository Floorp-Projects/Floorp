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

namespace mozilla { namespace net {

class nsHttpConnectionInfo;

class ASpdySession : public nsAHttpTransaction
{
public:
  ASpdySession();
  virtual ~ASpdySession();

  virtual bool AddStream(nsAHttpTransaction *, int32_t,
                         bool, nsIInterfaceRequestor *) = 0;
  virtual bool CanReuse() = 0;
  virtual bool RoomForMoreStreams() = 0;
  virtual PRIntervalTime IdleTime() = 0;
  virtual uint32_t ReadTimeoutTick(PRIntervalTime now) = 0;
  virtual void DontReuse() = 0;

  static ASpdySession *NewSpdySession(uint32_t version, nsISocketTransport *);

  virtual void PrintDiagnostics (nsCString &log) = 0;

  bool ResponseTimeoutEnabled() const MOZ_OVERRIDE MOZ_FINAL {
    return true;
  }

  virtual void SendPing() = 0;

  const static uint32_t kSendingChunkSize = 4095;
  const static uint32_t kTCPSendBufferSize = 131072;

  // until we have an API that can push back on receiving data (right now
  // WriteSegments is obligated to accept data and buffer) there is no
  // reason to throttle with the rwin other than in server push
  // scenarios.
  const static uint32_t kInitialRwin = 256 * 1024 * 1024;

  // soft errors are errors that terminate a stream without terminating the
  // connection. In general non-network errors are stream errors as well
  // as network specific items like cancels.
  bool SoftStreamError(nsresult code)
  {
    if (NS_SUCCEEDED(code) || code == NS_BASE_STREAM_WOULD_BLOCK) {
      return false;
    }

    // this could go either way, but because there are network instances of
    // it being a hard error we should consider it hard.
    if (code == NS_ERROR_FAILURE) {
      return false;
    }

    if (NS_ERROR_GET_MODULE(code) != NS_ERROR_MODULE_NETWORK) {
      return true;
    }

    // these are network specific soft errors
    return (code == NS_BASE_STREAM_CLOSED || code == NS_BINDING_FAILED ||
            code == NS_BINDING_ABORTED || code == NS_BINDING_REDIRECTED ||
            code == NS_ERROR_INVALID_CONTENT_ENCODING ||
            code == NS_BINDING_RETARGETED || code == NS_ERROR_CORRUPTED_CONTENT);
  }
};

typedef bool (*ALPNCallback) (nsISupports *); // nsISSLSocketControl is typical

// this is essentially a single instantiation as a member of nsHttpHandler.
// It could be all static except using static ctors of XPCOM objects is a
// bad idea.
class SpdyInformation
{
public:
  SpdyInformation();
  ~SpdyInformation() {}

  static const uint32_t kCount = 5;

  // determine the index (0..kCount-1) of the spdy information that
  // correlates to the npn string. NS_FAILED() if no match is found.
  nsresult GetNPNIndex(const nsACString &npnString, uint32_t *result) const;

  // determine if a version of the protocol is enabled for index < kCount
  bool ProtocolEnabled(uint32_t index) const;

  uint8_t   Version[kCount]; // telemetry enum e.g. SPDY_VERSION_31
  nsCString VersionString[kCount]; // npn string e.g. "spdy/3.1"

  // the ALPNCallback function allows the protocol stack to decide whether or
  // not to offer a particular protocol based on the known TLS information
  // that we will offer in the client hello (such as version). There has
  // not been a Server Hello received yet, so not much else can be considered.
  // Stacks without restrictions can just use SpdySessionTrue()
  ALPNCallback ALPNCallbacks[kCount];
};

}} // namespace mozilla::net

#endif // mozilla_net_ASpdySession_h
