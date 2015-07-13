/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

/*
  Currently supported are h2 and spdy/3.1
*/

#include "nsHttp.h"
#include "nsHttpHandler.h"

#include "ASpdySession.h"
#include "PSpdyPush.h"
#include "SpdyPush31.h"
#include "Http2Push.h"
#include "SpdySession31.h"
#include "Http2Session.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace net {

ASpdySession::ASpdySession()
{
}

ASpdySession::~ASpdySession()
{
}

ASpdySession *
ASpdySession::NewSpdySession(uint32_t version,
                             nsISocketTransport *aTransport)
{
  // This is a necko only interface, so we can enforce version
  // requests as a precondition
  MOZ_ASSERT(version == SPDY_VERSION_31 ||
             version == HTTP_VERSION_2,
             "Unsupported spdy version");

  // Don't do a runtime check of IsSpdyV?Enabled() here because pref value
  // may have changed since starting negotiation. The selected protocol comes
  // from a list provided in the SERVER HELLO filtered by our acceptable
  // versions, so there is no risk of the server ignoring our prefs.

  Telemetry::Accumulate(Telemetry::SPDY_VERSION2, version);

  if (version == SPDY_VERSION_31) {
    return new SpdySession31(aTransport);
  } else if (version == HTTP_VERSION_2) {
    return new Http2Session(aTransport, version);
  }

  return nullptr;
}
static bool SpdySessionTrue(nsISupports *securityInfo)
{
  return true;
}

SpdyInformation::SpdyInformation()
{
  // highest index of enabled protocols is the
  // most preferred for ALPN negotiaton
  Version[0] = SPDY_VERSION_31;
  VersionString[0] = NS_LITERAL_CSTRING("spdy/3.1");
  ALPNCallbacks[0] = SpdySessionTrue;

  Version[1] = HTTP_VERSION_2;
  VersionString[1] = NS_LITERAL_CSTRING("h2");
  ALPNCallbacks[1] = Http2Session::ALPNCallback;
}

bool
SpdyInformation::ProtocolEnabled(uint32_t index) const
{
  MOZ_ASSERT(index < kCount, "index out of range");

  switch (index) {
  case 0:
    return gHttpHandler->IsSpdyV31Enabled();
  case 1:
    return gHttpHandler->IsHttp2Enabled();
  }
  return false;
}

nsresult
SpdyInformation::GetNPNIndex(const nsACString &npnString,
                             uint32_t *result) const
{
  if (npnString.IsEmpty())
    return NS_ERROR_FAILURE;

  for (uint32_t index = 0; index < kCount; ++index) {
    if (npnString.Equals(VersionString[index])) {
      *result = index;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

//////////////////////////////////////////
// SpdyPushCache
//////////////////////////////////////////

SpdyPushCache::SpdyPushCache()
{
}

SpdyPushCache::~SpdyPushCache()
{
  mHashSpdy31.Clear();
  mHashHttp2.Clear();
}

bool
SpdyPushCache::RegisterPushedStreamSpdy31(nsCString key,
                                          SpdyPushedStream31 *stream)
{
  LOG3(("SpdyPushCache::RegisterPushedStreamSpdy31 %s 0x%X\n",
        key.get(), stream->StreamID()));
  if(mHashSpdy31.Get(key)) {
    LOG3(("SpdyPushCache::RegisterPushedStreamSpdy31 %s 0x%X duplicate key\n",
          key.get(), stream->StreamID()));
    return false;
  }
  mHashSpdy31.Put(key, stream);
  return true;
}

SpdyPushedStream31 *
SpdyPushCache::RemovePushedStreamSpdy31(nsCString key)
{
  SpdyPushedStream31 *rv = mHashSpdy31.Get(key);
  LOG3(("SpdyPushCache::RemovePushedStream %s 0x%X\n",
        key.get(), rv ? rv->StreamID() : 0));
  if (rv)
    mHashSpdy31.Remove(key);
  return rv;
}

bool
SpdyPushCache::RegisterPushedStreamHttp2(nsCString key,
                                         Http2PushedStream *stream)
{
  LOG3(("SpdyPushCache::RegisterPushedStreamHttp2 %s 0x%X\n",
        key.get(), stream->StreamID()));
  if(mHashHttp2.Get(key)) {
    LOG3(("SpdyPushCache::RegisterPushedStreamHttp2 %s 0x%X duplicate key\n",
          key.get(), stream->StreamID()));
    return false;
  }
  mHashHttp2.Put(key, stream);
  return true;
}

Http2PushedStream *
SpdyPushCache::RemovePushedStreamHttp2(nsCString key)
{
  Http2PushedStream *rv = mHashHttp2.Get(key);
  LOG3(("SpdyPushCache::RemovePushedStreamHttp2 %s 0x%X\n",
        key.get(), rv ? rv->StreamID() : 0));
  if (rv)
    mHashHttp2.Remove(key);
  return rv;
}

} // namespace net
} // namespace mozilla

