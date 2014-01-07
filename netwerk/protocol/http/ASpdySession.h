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

class ASpdySession : public nsAHttpTransaction
{
public:
  virtual bool AddStream(nsAHttpTransaction *, int32_t) = 0;
  virtual bool CanReuse() = 0;
  virtual bool RoomForMoreStreams() = 0;
  virtual PRIntervalTime IdleTime() = 0;
  virtual void ReadTimeoutTick(PRIntervalTime now) = 0;
  virtual void DontReuse() = 0;

  static ASpdySession *NewSpdySession(uint32_t version,
                                      nsAHttpTransaction *,
                                      nsISocketTransport *,
                                      int32_t);

  virtual void PrintDiagnostics (nsCString &log) = 0;

  bool ResponseTimeoutEnabled() const MOZ_OVERRIDE MOZ_FINAL {
    return true;
  }

  const static uint32_t kSendingChunkSize = 4096;
  const static uint32_t kTCPSendBufferSize = 131072;

  // until we have an API that can push back on receiving data (right now
  // WriteSegments is obligated to accept data and buffer) there is no
  // reason to throttle with the rwin other than in server push
  // scenarios.
  const static uint32_t kInitialRwin = 256 * 1024 * 1024;

  bool SoftStreamError(nsresult code)
  {
    return (code == NS_BASE_STREAM_CLOSED || code == NS_BINDING_FAILED ||
            code == NS_BINDING_ABORTED || code == NS_BINDING_REDIRECTED ||
            code == NS_BINDING_RETARGETED);
  }
};

// this is essentially a single instantiation as a member of nsHttpHandler.
// It could be all static except using static ctors of XPCOM objects is a
// bad idea.
class SpdyInformation
{
public:
  SpdyInformation();
  ~SpdyInformation() {}

  static const uint32_t kCount = 2;

  // determine if a version of the protocol is enabled for index <= kCount
  bool ProtocolEnabled(uint32_t index);

  // lookup a version enum based on an npn string. returns NS_OK if
  // string was known.
  nsresult GetNPNVersionIndex(const nsACString &npnString, uint8_t *result);

  uint8_t   Version[kCount];
  nsCString VersionString[kCount];
};

}} // namespace mozilla::net

#endif // mozilla_net_ASpdySession_h
