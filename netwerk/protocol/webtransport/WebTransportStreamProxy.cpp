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
NS_INTERFACE_MAP_END

WebTransportStreamProxy::WebTransportStreamProxy(
    Http3WebTransportStream* aStream)
    : mWebTransportStream(aStream) {
  nsCOMPtr<nsIAsyncInputStream> inputStream;
  nsCOMPtr<nsIAsyncOutputStream> outputStream;
  mWebTransportStream->GetWriterAndReader(getter_AddRefs(outputStream),
                                          getter_AddRefs(inputStream));
  if (outputStream) {
    mWriter = new AsyncOutputStreamWrapper(outputStream);
  }
  if (inputStream) {
    mReader = new AsyncInputStreamWrapper(inputStream, mWebTransportStream);
  }
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
      : mCallback(aCallback), mTarget(GetCurrentSerialEventTarget()) {}

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

NS_IMETHODIMP WebTransportStreamProxy::GetHasReceivedFIN(
    bool* aHasReceivedFIN) {
  *aHasReceivedFIN = mWebTransportStream->RecvDone();
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::GetInputStream(
    nsIAsyncInputStream** aOut) {
  if (!mReader) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<AsyncInputStreamWrapper> stream = mReader;
  stream.forget(aOut);
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::GetOutputStream(
    nsIAsyncOutputStream** aOut) {
  if (!mWriter) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<AsyncOutputStreamWrapper> stream = mWriter;
  stream.forget(aOut);
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::GetStreamId(uint64_t* aId) {
  *aId = mWebTransportStream->StreamId();
  return NS_OK;
}

NS_IMETHODIMP WebTransportStreamProxy::SetSendOrder(Maybe<int64_t> aSendOrder) {
  if (!OnSocketThread()) {
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "SetSendOrder", [stream = mWebTransportStream, aSendOrder]() {
          stream->SetSendOrder(aSendOrder);
        }));
  }
  mWebTransportStream->SetSendOrder(aSendOrder);
  return NS_OK;
}

//------------------------------------------------------------------------------
// WebTransportStreamProxy::AsyncInputStreamWrapper
//------------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(WebTransportStreamProxy::AsyncInputStreamWrapper,
                  nsIInputStream, nsIAsyncInputStream)

WebTransportStreamProxy::AsyncInputStreamWrapper::AsyncInputStreamWrapper(
    nsIAsyncInputStream* aStream, Http3WebTransportStream* aWebTransportStream)
    : mStream(aStream), mWebTransportStream(aWebTransportStream) {}

WebTransportStreamProxy::AsyncInputStreamWrapper::~AsyncInputStreamWrapper() =
    default;

void WebTransportStreamProxy::AsyncInputStreamWrapper::MaybeCloseStream() {
  if (!mWebTransportStream->RecvDone()) {
    return;
  }

  uint64_t available = 0;
  Unused << Available(&available);
  if (available) {
    // Don't close the InputStream if there's unread data available, since it
    // would be lost. We exit above unless we know no more data will be received
    // for the stream.
    return;
  }

  LOG(
      ("AsyncInputStreamWrapper::MaybeCloseStream close stream due to FIN "
       "stream=%p",
       mWebTransportStream.get()));
  Close();
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::Close() {
  return mStream->CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::Available(
    uint64_t* aAvailable) {
  return mStream->Available(aAvailable);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::StreamStatus() {
  return mStream->StreamStatus();
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::Read(
    char* aBuf, uint32_t aCount, uint32_t* aResult) {
  LOG(("WebTransportStreamProxy::AsyncInputStreamWrapper::Read %p", this));
  nsresult rv = mStream->Read(aBuf, aCount, aResult);
  MaybeCloseStream();
  return rv;
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::ReadSegments(
    nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
    uint32_t* aResult) {
  LOG(("WebTransportStreamProxy::AsyncInputStreamWrapper::ReadSegments %p",
       this));
  nsresult rv = mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  if (*aResult > 0) {
    LOG(("   Read %u bytes", *aResult));
  }
  MaybeCloseStream();
  return rv;
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::IsNonBlocking(
    bool* aResult) {
  return mStream->IsNonBlocking(aResult);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::CloseWithStatus(
    nsresult aStatus) {
  return mStream->CloseWithStatus(aStatus);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncInputStreamWrapper::AsyncWait(
    nsIInputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  return mStream->AsyncWait(aCallback, aFlags, aRequestedCount, aEventTarget);
}

//------------------------------------------------------------------------------
// WebTransportStreamProxy::AsyncOutputStreamWrapper
//------------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(WebTransportStreamProxy::AsyncOutputStreamWrapper,
                  nsIOutputStream, nsIAsyncOutputStream)

WebTransportStreamProxy::AsyncOutputStreamWrapper::AsyncOutputStreamWrapper(
    nsIAsyncOutputStream* aStream)
    : mStream(aStream) {}

WebTransportStreamProxy::AsyncOutputStreamWrapper::~AsyncOutputStreamWrapper() =
    default;

NS_IMETHODIMP WebTransportStreamProxy::AsyncOutputStreamWrapper::Flush() {
  return mStream->Flush();
}

NS_IMETHODIMP
WebTransportStreamProxy::AsyncOutputStreamWrapper::StreamStatus() {
  return mStream->StreamStatus();
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncOutputStreamWrapper::Write(
    const char* aBuf, uint32_t aCount, uint32_t* aResult) {
  LOG(
      ("WebTransportStreamProxy::AsyncOutputStreamWrapper::Write %p %u bytes, "
       "first byte %c",
       this, aCount, aBuf[0]));
  return mStream->Write(aBuf, aCount, aResult);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncOutputStreamWrapper::WriteFrom(
    nsIInputStream* aFromStream, uint32_t aCount, uint32_t* aResult) {
  return mStream->WriteFrom(aFromStream, aCount, aResult);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncOutputStreamWrapper::WriteSegments(
    nsReadSegmentFun aReader, void* aClosure, uint32_t aCount,
    uint32_t* aResult) {
  return mStream->WriteSegments(aReader, aClosure, aCount, aResult);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncOutputStreamWrapper::AsyncWait(
    nsIOutputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  return mStream->AsyncWait(aCallback, aFlags, aRequestedCount, aEventTarget);
}

NS_IMETHODIMP
WebTransportStreamProxy::AsyncOutputStreamWrapper::CloseWithStatus(
    nsresult aStatus) {
  return mStream->CloseWithStatus(aStatus);
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncOutputStreamWrapper::Close() {
  return mStream->Close();
}

NS_IMETHODIMP WebTransportStreamProxy::AsyncOutputStreamWrapper::IsNonBlocking(
    bool* aResult) {
  return mStream->IsNonBlocking(aResult);
}

}  // namespace mozilla::net
