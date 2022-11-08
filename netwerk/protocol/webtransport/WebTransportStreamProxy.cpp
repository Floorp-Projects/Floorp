/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportStreamProxy.h"

#include "WebTransportLog.h"
#include "Http3WebTransportStream.h"
#include "nsProxyRelease.h"
#include "nsSocketTransportService2.h"

namespace mozilla::net {

NS_IMPL_ADDREF(WebTransportStreamProxy)
NS_IMPL_RELEASE(WebTransportStreamProxy)

NS_INTERFACE_MAP_BEGIN(WebTransportStreamProxy)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebTransportReceiveStream)
  NS_INTERFACE_MAP_ENTRY(nsIWebTransportReceiveStream)
  NS_INTERFACE_MAP_ENTRY(nsIWebTransportSendStream)
  NS_INTERFACE_MAP_ENTRY(nsIWebTransportBidirectionalStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncOutputStream)
NS_INTERFACE_MAP_END

WebTransportStreamProxy::WebTransportStreamProxy(
    Http3WebTransportStream* aStream)
    : mWebTransportStream(aStream) {}

WebTransportStreamProxy::~WebTransportStreamProxy() {
  // mWebTransportStream needs to be destroyed on the socket thread.
  NS_ProxyRelease("WebTransportStreamProxy::~WebTransportStreamProxy",
                  gSocketTransportService, mWebTransportStream.forget());
}

NS_IMETHODIMP WebTransportStreamProxy::SendStopSending(uint8_t aError) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::SendFin(void) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    return gSocketTransportService->Dispatch(
        NS_NewRunnableFunction("WebTransportStreamProxy::SendFin",
                               [self{std::move(self)}]() { self->SendFin(); }));
  }

  mWebTransportStream->SendFin();
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::Reset(uint8_t aErrorCode) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportStreamProxy::Reset",
        [self{std::move(self)}, error(aErrorCode)]() { self->Reset(error); }));
  }

  mWebTransportStream->Reset(aErrorCode);
  return NS_OK;
}

namespace {

class StatsCallbackWrapper : public nsIWebTransportSendStreamStatsCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit StatsCallbackWrapper(
      nsIWebTransportSendStreamStatsCallback* aCallback)
      : mCallback(aCallback), mTarget(GetCurrentEventTarget()) {}

  NS_IMETHOD OnStatsAvailable(nsIWebTransportSendStreamStats* aStats) override {
    if (!mTarget->IsOnCurrentThread()) {
      RefPtr<StatsCallbackWrapper> self(this);
      nsCOMPtr<nsIWebTransportSendStreamStats> stats = aStats;
      Unused << mTarget->Dispatch(NS_NewRunnableFunction(
          "StatsCallbackWrapper::OnStatsAvailable",
          [self{std::move(self)}, stats{std::move(stats)}]() {
            self->OnStatsAvailable(stats);
          }));
      return NS_OK;
    }

    mCallback->OnStatsAvailable(aStats);
    return NS_OK;
  }

 private:
  virtual ~StatsCallbackWrapper() {
    NS_ProxyRelease("StatsCallbackWrapper::~StatsCallbackWrapper", mTarget,
                    mCallback.forget());
  }

  nsCOMPtr<nsIWebTransportSendStreamStatsCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mTarget;
};

NS_IMPL_ISUPPORTS(StatsCallbackWrapper, nsIWebTransportSendStreamStatsCallback)

}  // namespace

NS_IMETHODIMP WebTransportStreamProxy::GetSendStreamStats(
    nsIWebTransportSendStreamStatsCallback* aCallback) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    nsCOMPtr<nsIWebTransportSendStreamStatsCallback> callback =
        new StatsCallbackWrapper(aCallback);
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportStreamProxy::GetSendStreamStats",
        [self{std::move(self)}, callback{std::move(callback)}]() {
          self->GetSendStreamStats(callback);
        }));
  }

  nsCOMPtr<nsIWebTransportSendStreamStats> stats =
      mWebTransportStream->GetSendStreamStats();
  aCallback->OnStatsAvailable(stats);
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::Close() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Available(uint64_t* aAvailable) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Read(char* aBuf, uint32_t aCount,
                                            uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::ReadSegments(nsWriteSegmentFun aWriter,
                                                    void* aClosure,
                                                    uint32_t aCount,
                                                    uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::IsNonBlocking(bool* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::CloseWithStatus(nsresult aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncWait(
    nsIInputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Flush() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void WebTransportStreamProxy::EnsureWriter() {
  if (mWriter) {
    return;
  }

  mWriter = mWebTransportStream->GetWriter();
}

NS_IMETHODIMP WebTransportStreamProxy::Write(const char* aBuf, uint32_t aCount,
                                             uint32_t* aResult) {
  EnsureWriter();
  if (!mWriter) {
    return NS_ERROR_UNEXPECTED;
  }

  return mWriter->Write(aBuf, aCount, aResult);
}

// static
nsresult WebTransportStreamProxy::WriteFromSegments(
    nsIInputStream* input, void* closure, const char* fromSegment,
    uint32_t offset, uint32_t count, uint32_t* countRead) {
  WebTransportStreamProxy* self = (WebTransportStreamProxy*)closure;
  return self->Write(fromSegment, count, countRead);
}

NS_IMETHODIMP WebTransportStreamProxy::WriteFrom(nsIInputStream* aFromStream,
                                                 uint32_t aCount,
                                                 uint32_t* aResult) {
  return aFromStream->ReadSegments(WriteFromSegments, this, aCount, aResult);
}

NS_IMETHODIMP WebTransportStreamProxy::WriteSegments(nsReadSegmentFun aReader,
                                                     void* aClosure,
                                                     uint32_t aCount,
                                                     uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncWait(
    nsIOutputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla::net
