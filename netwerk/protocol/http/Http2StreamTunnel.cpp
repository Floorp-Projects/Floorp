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

#include "Http2StreamTunnel.h"

namespace mozilla::net {

Http2StreamTunnel::~Http2StreamTunnel() { ClearTransactionsBlockedOnTunnel(); }

void Http2StreamTunnel::HandleResponseHeaders(nsACString& aHeadersOut,
                                              int32_t httpResponseCode) {
  LOG3(
      ("Http2StreamTunnel %p Tunnel Response code %d", this, httpResponseCode));
  // 1xx response is simply skipeed and a final response is expected.
  // 2xx response needs to be encrypted.
  if ((httpResponseCode / 100) > 2) {
    MapStreamToPlainText();
  }
  if (MapStreamToHttpConnection(aHeadersOut, httpResponseCode)) {
    // Process transactions only if we have a final response, i.e., response
    // code >= 200.
    ClearTransactionsBlockedOnTunnel();
  }

  if (!mPlainTextTunnel) {
    aHeadersOut.Truncate();
    LOG(
        ("Http2StreamTunnel::ConvertHeaders %p 0x%X headers removed for "
         "tunnel\n",
         this, mStreamID));
  }
}

bool Http2StreamTunnel::MapStreamToHttpConnection(
    const nsACString& aFlat407Headers, int32_t aHttpResponseCode) {
  RefPtr<Http2ConnectTransaction> qiTrans(
      mTransaction->QueryHttp2ConnectTransaction());
  MOZ_ASSERT(qiTrans);

  return qiTrans->MapStreamToHttpConnection(mSocketTransport,
                                            mTransaction->ConnectionInfo(),
                                            aFlat407Headers, aHttpResponseCode);
}

void Http2StreamTunnel::MapStreamToPlainText() {
  RefPtr<Http2ConnectTransaction> qiTrans(
      mTransaction->QueryHttp2ConnectTransaction());
  MOZ_ASSERT(qiTrans);
  mPlainTextTunnel = true;
  qiTrans->ForcePlainText();
}

nsCString& Http2StreamTunnel::RegistrationKey() {
  if (mRegistrationKey.IsEmpty()) {
    MOZ_ASSERT(Transaction());
    MOZ_ASSERT(Transaction()->ConnectionInfo());

    mRegistrationKey = Transaction()->ConnectionInfo()->HashKey();
  }

  return mRegistrationKey;
}

void Http2StreamTunnel::ClearTransactionsBlockedOnTunnel() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsresult rv =
      gHttpHandler->ConnMgr()->ProcessPendingQ(mTransaction->ConnectionInfo());
  if (NS_FAILED(rv)) {
    LOG3(
        ("Http2StreamTunnel::ClearTransactionsBlockedOnTunnel %p\n"
         "  ProcessPendingQ failed: %08x\n",
         this, static_cast<uint32_t>(rv)));
  }
}

}  // namespace mozilla::net
