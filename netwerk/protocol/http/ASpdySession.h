/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ASpdySession_h
#define mozilla_net_ASpdySession_h

#include "nsAHttpTransaction.h"
#include "nsAHttpConnection.h"
#include "prinrval.h"
#include "nsString.h"

class nsISocketTransport;

namespace mozilla { namespace net {

// This is designed to handle up to 2 concrete protocol levels
// simultaneously
//
// Currently supported are v3 (preferred), and v2
// network.protocol.http.spdy.enabled.v2 (and v3) prefs can enable/disable
// them.

class ASpdySession : public nsAHttpTransaction
{
public:
  virtual bool AddStream(nsAHttpTransaction *, PRInt32) = 0;
  virtual bool CanReuse() = 0;
  virtual bool RoomForMoreStreams() = 0;
  virtual PRIntervalTime IdleTime() = 0;
  virtual void ReadTimeoutTick(PRIntervalTime now) = 0;
  virtual void DontReuse() = 0;

  static ASpdySession *NewSpdySession(PRUint32 version,
                                      nsAHttpTransaction *,
                                      nsISocketTransport *,
                                      PRInt32);

  const static PRUint32 kSendingChunkSize = 4096;
};

// this is essentially a single instantiation as a member of nsHttpHandler.
// It could be all static except using static ctors of XPCOM objects is a
// bad idea.
class SpdyInformation
{
public:
  SpdyInformation();
  ~SpdyInformation() {}

  // determine if a version of the protocol is enabled. The primary
  // version is index 0, the secondary version is index 1.
  bool ProtocolEnabled(PRUint32 index);

  // lookup a version enum based on an npn string. returns NS_OK if
  // string was known.
  nsresult GetNPNVersionIndex(const nsACString &npnString, PRUint8 *result);

  // lookup a version enum based on an alternate protocol string. returns NS_OK
  // if string was known and corresponding protocol is enabled.
  nsresult GetAlternateProtocolVersionIndex(const char *val,
                                            PRUint8 *result);

  enum {
    SPDY_VERSION_2 = 2,
    SPDY_VERSION_3 = 3
  };

  PRUint8   Version[2];
  nsCString VersionString[2];
  nsCString AlternateProtocolString[2];
};

}} // namespace mozilla::net

#endif // mozilla_net_ASpdySession_h
