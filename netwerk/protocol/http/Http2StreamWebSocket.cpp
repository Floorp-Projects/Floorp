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
#include "Http2ConnectTransaction.h"
#include "Http2Session.h"
#include "mozilla/Telemetry.h"
#include "nsHttpTransaction.h"

namespace mozilla::net {

Http2StreamWebSocket::Http2StreamWebSocket(nsAHttpTransaction* httpTransaction,
                                           Http2Session* session,
                                           int32_t priority, uint64_t bcId)
    : Http2StreamBase(
          (httpTransaction->QueryHttpTransaction())
              ? httpTransaction->QueryHttpTransaction()->TopBrowsingContextId()
              : 0,
          session, priority, bcId),
      mTransaction(httpTransaction) {
  LOG1(("Http2Stream::Http2Stream %p trans=%p", this, httpTransaction));
}

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

nsresult Http2StreamWebSocket::CallToReadData(uint32_t count,
                                              uint32_t* countRead) {
  return mTransaction->ReadSegments(this, count, countRead);
}

nsresult Http2StreamWebSocket::CallToWriteData(uint32_t count,
                                               uint32_t* countWritten) {
  return mTransaction->WriteSegments(this, count, countWritten);
}

nsresult Http2StreamWebSocket::GenerateHeaders(nsCString& aCompressedData,
                                               uint8_t& firstFrameFlags) {
  nsHttpRequestHead* head = mTransaction->RequestHead();
  nsAutoCString requestURI;
  head->RequestURI(requestURI);

  RefPtr<Http2Session> session = Session();

  LOG3(("Http2StreamWebSocket %p Stream ID 0x%X [session=%p] for URI %s\n",
        this, mStreamID, session.get(), requestURI.get()));

  nsAutoCString authorityHeader;
  nsresult rv = head->GetHeader(nsHttp::Host, authorityHeader);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false);
    return rv;
  }

  nsDependentCString scheme(head->IsHTTPS() ? "https" : "http");

  nsAutoCString method;
  nsAutoCString path;
  head->Method(method);
  head->Path(path);

  rv = session->Compressor()->EncodeHeaderBlock(
      mFlatHttpRequestHeaders, method, path, authorityHeader, scheme,
      "websocket"_ns, false, aCompressedData);
  NS_ENSURE_SUCCESS(rv, rv);

  mRequestBodyLenRemaining = 0x0fffffffffffffffULL;

  // The size of the input headers is approximate
  uint32_t ratio =
      aCompressedData.Length() * 100 /
      (11 + requestURI.Length() + mFlatHttpRequestHeaders.Length());

  Telemetry::Accumulate(Telemetry::SPDY_SYN_RATIO, ratio);

  return NS_OK;
}

void Http2StreamWebSocket::CloseStream(nsresult reason) {
  mTransaction->Close(reason);
  mSession = nullptr;
}

}  // namespace mozilla::net
