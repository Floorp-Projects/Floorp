/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"
#include "nsHttpHandler.h"

#include "ASpdySession.h"
#include "SpdySession2.h"
#include "SpdySession3.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace net {

ASpdySession *
ASpdySession::NewSpdySession(PRUint32 version,
                             nsAHttpTransaction *aTransaction,
                             nsISocketTransport *aTransport,
                             PRInt32 aPriority)
{
  // This is a necko only interface, so we can enforce version
  // requests as a precondition
  NS_ABORT_IF_FALSE(version == SpdyInformation::SPDY_VERSION_2 ||
                    version == SpdyInformation::SPDY_VERSION_3,
                    "Unsupported spdy version");

  // Don't do a runtime check of IsSpdyV?Enabled() here because pref value
  // may have changed since starting negotiation. The selected protocol comes
  // from a list provided in the SERVER HELLO filtered by our acceptable
  // versions, so there is no risk of the server ignoring our prefs.

  Telemetry::Accumulate(Telemetry::SPDY_VERSION2, version);
    
  if (version == SpdyInformation::SPDY_VERSION_2)
    return new SpdySession2(aTransaction, aTransport, aPriority);

  return new SpdySession3(aTransaction, aTransport, aPriority);
}

SpdyInformation::SpdyInformation()
{
  // list the preferred version first
  Version[0] = SPDY_VERSION_3;
  VersionString[0] = NS_LITERAL_CSTRING("spdy/3");
  AlternateProtocolString[0] = NS_LITERAL_CSTRING("443:npn-spdy/3");

  Version[1] = SPDY_VERSION_2;
  VersionString[1] = NS_LITERAL_CSTRING("spdy/2");
  AlternateProtocolString[1] = NS_LITERAL_CSTRING("443:npn-spdy/2");
}

bool
SpdyInformation::ProtocolEnabled(PRUint32 index)
{
  if (index == 0)
    return gHttpHandler->IsSpdyV3Enabled();

  if (index == 1)
    return gHttpHandler->IsSpdyV2Enabled();

  NS_ABORT_IF_FALSE(false, "index out of range");
  return false;
}

nsresult
SpdyInformation::GetNPNVersionIndex(const nsACString &npnString,
                                    PRUint8 *result)
{
  if (npnString.IsEmpty())
    return NS_ERROR_FAILURE;

  if (npnString.Equals(VersionString[0]))
    *result = Version[0];
  else if (npnString.Equals(VersionString[1]))
    *result = Version[1];
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
SpdyInformation::GetAlternateProtocolVersionIndex(const char *val,
                                                  PRUint8 *result)
{
  if (!val || !val[0])
    return NS_ERROR_FAILURE;

  if (ProtocolEnabled(0) && nsHttp::FindToken(val,
                                              AlternateProtocolString[0].get(),
                                              HTTP_HEADER_VALUE_SEPS))
    *result = Version[0];
  else if (ProtocolEnabled(1) && nsHttp::FindToken(val,
                                                   AlternateProtocolString[1].get(),
                                                   HTTP_HEADER_VALUE_SEPS))
    *result = Version[1];
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla

