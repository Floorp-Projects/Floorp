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
    : mWebTransportStream(aStream) {
  mWebTransportStream->GetWriterAndReader(getter_AddRefs(mWriter),
                                          getter_AddRefs(mReader));
}

WebTransportStreamProxy::~WebTransportStreamProxy() {
  // mWebTransportStream needs to be destroyed on the socket thread.
  NS_ProxyRelease("WebTransportStreamProxy::~WebTransportStreamProxy",
                  gSocketTransportService, mWebTransportStream.forget());
}

NS_IMETHODIMP WebTransportStreamProxy::SendStopSending(uint8_t aError) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    return gSocketTransportService->Dispatch(
        NS_NewRunnableFunction("WebTransportStreamProxy::SendStopSending",
                               [self{std::move(self)}, error(aError)]() {
                                 self->SendStopSending(error);
                               }));
  }

  mWebTransportStream->SendStopSending(aError);
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::SendFin(void) {
  if (!mWriter) {
    return NS_ERROR_UNEXPECTED;
  }

  mWriter->Close();

  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportStreamProxy::SendFin",
        [self{std::move(self)}]() { self->mWebTransportStream->SendFin(); }));
  }

  mWebTransportStream->SendFin();
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::Reset(uint8_t aErrorCode) {
  if (!mWriter) {
    return NS_ERROR_UNEXPECTED;
  }

  mWriter->Close();

  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    return gSocketTransportService->Dispatch(
        NS_NewRunnableFunction("WebTransportStreamProxy::Reset",
                               [self{std::move(self)}, error(aErrorCode)]() {
                                 self->mWebTransportStream->Reset(error);
                               }));
  }

  mWebTransportStream->Reset(aErrorCode);
  return NS_OK;
}

namespace {

class StatsCallbackWrapper : public nsIWebTransportStreamStatsCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit StatsCallbackWrapper(nsIWebTransportStreamStatsCallback* aCallback)
      : mCallback(aCallback), mTarget(GetCurrentEventTarget()) {}

  NS_IMETHOD OnSendStatsAvailable(
      nsIWebTransportSendStreamStats* aStats) override {
    if (!mTarget->IsOnCurrentThread()) {
      RefPtr<StatsCallbackWrapper> self(this);
      nsCOMPtr<nsIWebTransportSendStreamStats> stats = aStats;
      Unused << mTarget->Dispatch(NS_NewRunnableFunction(
          "StatsCallbackWrapper::OnSendStatsAvailable",
          [self{std::move(self)}, stats{std::move(stats)}]() {
            self->OnSendStatsAvailable(stats);
          }));
      return NS_OK;
    }

    mCallback->OnSendStatsAvailable(aStats);
    return NS_OK;
  }

  NS_IMETHOD OnReceiveStatsAvailable(
      nsIWebTransportReceiveStreamStats* aStats) override {
    if (!mTarget->IsOnCurrentThread()) {
      RefPtr<StatsCallbackWrapper> self(this);
      nsCOMPtr<nsIWebTransportReceiveStreamStats> stats = aStats;
      Unused << mTarget->Dispatch(NS_NewRunnableFunction(
          "StatsCallbackWrapper::OnReceiveStatsAvailable",
          [self{std::move(self)}, stats{std::move(stats)}]() {
            self->OnReceiveStatsAvailable(stats);
          }));
      return NS_OK;
    }

    mCallback->OnReceiveStatsAvailable(aStats);
    return NS_OK;
  }

 private:
  virtual ~StatsCallbackWrapper() {
    NS_ProxyRelease("StatsCallbackWrapper::~StatsCallbackWrapper", mTarget,
                    mCallback.forget());
  }

  nsCOMPtr<nsIWebTransportStreamStatsCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mTarget;
};

NS_IMPL_ISUPPORTS(StatsCallbackWrapper, nsIWebTransportStreamStatsCallback)

}  // namespace

NS_IMETHODIMP WebTransportStreamProxy::GetSendStreamStats(
    nsIWebTransportStreamStatsCallback* aCallback) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    nsCOMPtr<nsIWebTransportStreamStatsCallback> callback =
        new StatsCallbackWrapper(aCallback);
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportStreamProxy::GetSendStreamStats",
        [self{std::move(self)}, callback{std::move(callback)}]() {
          self->GetSendStreamStats(callback);
        }));
  }

  nsCOMPtr<nsIWebTransportSendStreamStats> stats =
      mWebTransportStream->GetSendStreamStats();
  aCallback->OnSendStatsAvailable(stats);
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::GetReceiveStreamStats(
    nsIWebTransportStreamStatsCallback* aCallback) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportStreamProxy> self(this);
    nsCOMPtr<nsIWebTransportStreamStatsCallback> callback =
        new StatsCallbackWrapper(aCallback);
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportStreamProxy::GetReceiveStreamStats",
        [self{std::move(self)}, callback{std::move(callback)}]() {
          self->GetReceiveStreamStats(callback);
        }));
  }

  nsCOMPtr<nsIWebTransportReceiveStreamStats> stats =
      mWebTransportStream->GetReceiveStreamStats();
  aCallback->OnReceiveStatsAvailable(stats);
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
  if (!mReader) {
    return NS_ERROR_UNEXPECTED;
  }

  return mReader->Read(aBuf, aCount, aResult);
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
  if (!mReader) {
    return NS_ERROR_UNEXPECTED;
  }

  return mReader->AsyncWait(aCallback, aFlags, aRequestedCount, aEventTarget);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncWaitForRead(
    nsIInputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aTarget) {
  return AsyncWait(aCallback, aFlags, aRequestedCount, aTarget);
}

NS_IMETHODIMP WebTransportStreamProxy::Flush() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP WebTransportStreamProxy::Write(const char* aBuf, uint32_t aCount,
                                             uint32_t* aResult) {
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
