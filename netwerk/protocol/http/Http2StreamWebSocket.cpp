/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#include "Http2StreamWebSocket.h"

namespace mozilla::net {
// ConvertResponseHeaders is used to convert the response headers
// into HTTP/1 format and report some telemetry
void Http2StreamWebSocket::HandleResponseHeaders(nsACString& aHeadersOut,
                                                 int32_t httpResponseCode) {
  LOG3(("Http2StreamBase %p websocket response code %d", this,
        httpResponseCode));
  if (httpResponseCode == 200) {
    MapStreamToHttpConnection(aHeadersOut);
  }
}

bool Http2StreamWebSocket::MapStreamToHttpConnection(
    const nsACString& aFlat407Headers) {
  RefPtr<Http2ConnectTransaction> qiTrans(
      mTransaction->QueryHttp2ConnectTransaction());
  MOZ_ASSERT(qiTrans);

  return qiTrans->MapStreamToHttpConnection(
      mSocketTransport, mTransaction->ConnectionInfo(), aFlat407Headers, -1);
}

}  // namespace mozilla::net
