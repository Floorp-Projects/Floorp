/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// SPDY Server Push

/*
  A pushed stream is put into a memory buffer (The SpdyPushTransactionBuffer)
  and spooled there until a GET is made that can be matched up with it. At
  that time we have two spdy streams - the GET (aka the sink) and the PUSH
  (aka the source). Data is copied between those two streams for the lifetime
  of the transaction. This is true even if the transaction buffer is empty,
  partly complete, or totally loaded at the time the GET correspondence is made.

  correspondence is done through a hash table of the full url, the spdy session,
  and the load group. The load group is implicit because that's where the
  hash is stored, the other items comprise the hash key.

  Pushed streams are subject to aggressive flow control before they are matched
  with a GET at which point flow control is effectively disabled to match the
  client pull behavior.
*/

#ifndef mozilla_net_SpdyPush_Public_h
#define mozilla_net_SpdyPush_Public_h

#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsISupports.h"
#include "nsStringFwd.h"

namespace mozilla {
namespace net {

class Http2PushedStream;

// One cache per load group
class SpdyPushCache
{
public:
  // The cache holds only weak pointers - no references
  SpdyPushCache() = default;
  virtual ~SpdyPushCache();
  MOZ_MUST_USE bool  RegisterPushedStreamHttp2(const nsCString& key,
                                               Http2PushedStream *stream);
  Http2PushedStream *RemovePushedStreamHttp2(const nsCString& key);
  Http2PushedStream *RemovePushedStreamHttp2ByID(const nsCString& key, const uint32_t& streamID);
private:
  nsDataHashtable<nsCStringHashKey, Http2PushedStream *> mHashHttp2;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_SpdyPush_Public_h
