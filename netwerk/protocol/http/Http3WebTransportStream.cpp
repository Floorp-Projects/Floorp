/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Http3WebTransportStream.h"

#include "HttpLog.h"
#include "Http3Session.h"
#include "Http3WebTransportSession.h"
#include "mozilla/TimeStamp.h"
#include "nsHttpHandler.h"
#include "nsIOService.h"
#include "nsIPipe.h"
#include "nsSocketTransportService2.h"
#include "nsIWebTransportStream.h"

namespace mozilla::net {

namespace {

// This is an nsAHttpTransaction that does nothing.
class DummyWebTransportStreamTransaction : public nsAHttpTransaction {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  DummyWebTransportStreamTransaction() = default;

  void SetConnection(nsAHttpConnection*) override {}
  nsAHttpConnection* Connection() override { return nullptr; }
  void GetSecurityCallbacks(nsIInterfaceRequestor**) override {}
  void OnTransportStatus(nsITransport* transport, nsresult status,
                         int64_t progress) override {}
  bool IsDone() override { return false; }
  nsresult Status() override { return NS_OK; }
  uint32_t Caps() override { return 0; }
  [[nodiscard]] nsresult ReadSegments(nsAHttpSegmentReader*, uint32_t,
                                      uint32_t*) override {
    return NS_OK;
  }
  [[nodiscard]] nsresult WriteSegments(nsAHttpSegmentWriter*, uint32_t,
                                       uint32_t*) override {
    return NS_OK;
  }
  void Close(nsresult reason) override {}
  nsHttpConnectionInfo* ConnectionInfo() override { return nullptr; }
  void SetProxyConnectFailed() override {}
  nsHttpRequestHead* RequestHead() override { return nullptr; }
  uint32_t Http1xTransactionCount() override { return 0; }
  [[nodiscard]] nsresult TakeSubTransactions(
      nsTArray<RefPtr<nsAHttpTransaction> >& outTransactions) override {
    return NS_OK;
  }

 private:
  virtual ~DummyWebTransportStreamTransaction() = default;
};

NS_IMPL_ISUPPORTS(DummyWebTransportStreamTransaction, nsISupportsWeakReference)

class WebTransportSendStreamStats : public nsIWebTransportSendStreamStats {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  WebTransportSendStreamStats(uint64_t aWritten, uint64_t aSent,
                              uint64_t aAcked)
      : mTimeStamp(TimeStamp::Now()),
        mTotalWritten(aWritten),
        mTotalSent(aSent),
        mTotalAcknowledged(aAcked) {}

  NS_IMETHOD GetTimestamp(mozilla::TimeStamp* aTimestamp) override {
    *aTimestamp = mTimeStamp;
    return NS_OK;
  }
  NS_IMETHOD GetBytesWritten(uint64_t* aBytesWritten) override {
    *aBytesWritten = mTotalWritten;
    return NS_OK;
  }
  NS_IMETHOD GetBytesSent(uint64_t* aBytesSent) override {
    *aBytesSent = mTotalSent;
    return NS_OK;
  }
  NS_IMETHOD GetBytesAcknowledged(uint64_t* aBytesAcknowledged) override {
    *aBytesAcknowledged = mTotalAcknowledged;
    return NS_OK;
  }

 private:
  virtual ~WebTransportSendStreamStats() = default;

  TimeStamp mTimeStamp;
  uint64_t mTotalWritten;
  uint64_t mTotalSent;
  uint64_t mTotalAcknowledged;
};

NS_IMPL_ISUPPORTS(WebTransportSendStreamStats, nsIWebTransportSendStreamStats)

}  // namespace

NS_IMPL_ISUPPORTS(Http3WebTransportStream, nsIInputStreamCallback)

Http3WebTransportStream::Http3WebTransportStream(
    Http3Session* aSession, uint64_t aSessionId, WebTransportStreamType aType,
    std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
        aCallback)
    : Http3StreamBase(new DummyWebTransportStreamTransaction(), aSession),
      mSessionId(aSessionId),
      mStreamType(aType),
      mStreamReadyCallback(std::move(aCallback)) {
  LOG(("Http3WebTransportStream ctor %p", this));
}

Http3WebTransportStream::~Http3WebTransportStream() {
  LOG(("Http3WebTransportStream dtor %p", this));
}

nsresult Http3WebTransportStream::TryActivating() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  return mSession->TryActivatingWebTransportStream(&mStreamId, this);
}

NS_IMETHODIMP Http3WebTransportStream::OnInputStreamReady(
    nsIAsyncInputStream* aStream) {
  LOG(("Http3WebTransportStream::OnInputStreamReady [this=%p stream=%p]", this,
       aStream));

  uint64_t avail = 0;
  Unused << aStream->Available(&avail);
  mTotalWritten += avail;

  mSession->StreamHasDataToWrite(this);
  return NS_OK;
}

nsresult Http3WebTransportStream::InitOutputPipe() {
  nsCOMPtr<nsIAsyncOutputStream> out;
  nsCOMPtr<nsIAsyncInputStream> in;
  NS_NewPipe2(getter_AddRefs(in), getter_AddRefs(out), true, true,
              nsIOService::gDefaultSegmentSize,
              nsIOService::gDefaultSegmentCount);

  {
    MutexAutoLock lock(mMutex);
    mSendStreamPipeIn = std::move(in);
    mSendStreamPipeOut = std::move(out);
  }

  return mSendStreamPipeIn->AsyncWait(this, 0, 0, gSocketTransportService);
}

already_AddRefed<nsIAsyncOutputStream> Http3WebTransportStream::GetWriter() {
  nsCOMPtr<nsIAsyncOutputStream> stream;
  {
    MutexAutoLock lock(mMutex);
    stream = mSendStreamPipeOut;
  }
  return stream.forget();
}

already_AddRefed<nsIWebTransportSendStreamStats>
Http3WebTransportStream::GetSendStreamStats() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsCOMPtr<nsIWebTransportSendStreamStats> stats =
      new WebTransportSendStreamStats(mTotalWritten, mTotalSent,
                                      mTotalAcknowledged);
  return stats.forget();
}

nsresult Http3WebTransportStream::OnReadSegment(const char* buf, uint32_t count,
                                                uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3WebTransportStream::OnReadSegment count=%u state=%d [this=%p]",
       count, mSendState, this));

  nsresult rv = NS_OK;

  switch (mSendState) {
    case WAITING_TO_ACTIVATE:
      rv = TryActivating();
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        LOG3(
            ("Http3WebTransportStream::OnReadSegment %p cannot activate now. "
             "queued.\n",
             this));
        break;
      }
      if (NS_FAILED(rv)) {
        LOG3(
            ("Http3WebTransportStream::OnReadSegment %p cannot activate "
             "error=0x%" PRIx32 ".",
             this, static_cast<uint32_t>(rv)));
        mStreamReadyCallback(Err(rv));
        break;
      }

      rv = InitOutputPipe();
      if (NS_FAILED(rv)) {
        LOG3(
            ("Http3WebTransportStream::OnReadSegment %p failed to create pipe "
             "error=0x%" PRIx32 ".",
             this, static_cast<uint32_t>(rv)));
        mSendState = SEND_DONE;
        mStreamReadyCallback(Err(rv));
        break;
      }

      // Successfully activated.
      mSendState = SENDING;
      mStreamReadyCallback(RefPtr{this});
      break;
    case SENDING: {
      rv = mSession->SendRequestBody(mStreamId, buf, count, countRead);
      LOG3(
          ("Http3WebTransportStream::OnReadSegment %p sending body returns "
           "error=0x%" PRIx32 ".",
           this, static_cast<uint32_t>(rv)));
      mTotalSent += *countRead;
    } break;
    default:
      MOZ_ASSERT(false, "We are done sending this request!");
      rv = NS_ERROR_UNEXPECTED;
      break;
  }

  mSocketOutCondition = rv;

  return mSocketOutCondition;
}

// static
nsresult Http3WebTransportStream::ReadRequestSegment(
    nsIInputStream* stream, void* closure, const char* buf, uint32_t offset,
    uint32_t count, uint32_t* countRead) {
  Http3WebTransportStream* wtStream = (Http3WebTransportStream*)closure;
  nsresult rv = wtStream->OnReadSegment(buf, count, countRead);
  LOG(("Http3WebTransportStream::ReadRequestSegment %p read=%u", wtStream,
       *countRead));
  return rv;
}

nsresult Http3WebTransportStream::ReadSegments() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::ReadSegments [this=%p]", this));
  nsresult rv = NS_OK;
  uint32_t sendBytes = 0;
  bool again = true;
  do {
    sendBytes = 0;
    rv = mSocketOutCondition = NS_OK;
    LOG(("Http3WebTransportStream::ReadSegments state=%d [this=%p]", mSendState,
         this));
    switch (mSendState) {
      case WAITING_TO_ACTIVATE: {
        LOG3(
            ("Http3WebTransportStream %p ReadSegments forcing OnReadSegment "
             "call\n",
             this));
        uint32_t wasted = 0;
        nsresult rv2 = OnReadSegment("", 0, &wasted);
        LOG3(("  OnReadSegment returned 0x%08" PRIx32,
              static_cast<uint32_t>(rv2)));
      }
        [[fallthrough]];
      case SENDING: {
        rv = mSendStreamPipeIn->ReadSegments(ReadRequestSegment, this,
                                             nsIOService::gDefaultSegmentSize,
                                             &sendBytes);
      } break;
      case SEND_DONE: {
        return NS_OK;
      }
      default:
        sendBytes = 0;
        rv = NS_OK;
        break;
    }

    LOG(("Http3WebTransportStream::ReadSegments rv=0x%" PRIx32
         " read=%u sock-cond=%" PRIx32 " again=%d mSendFin=%d [this=%p]",
         static_cast<uint32_t>(rv), sendBytes,
         static_cast<uint32_t>(mSocketOutCondition), again, mSendFin, this));

    // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
    if (rv == NS_BASE_STREAM_CLOSED || mSendFin) {
      rv = NS_OK;
      sendBytes = 0;
    }

    if (NS_FAILED(rv)) {
      // if the writer didn't want to write any more data, then
      // wait for the transaction to call ResumeSend.
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        rv = NS_OK;
      }
      again = false;
    } else if (NS_FAILED(mSocketOutCondition)) {
      if (mSocketOutCondition != NS_BASE_STREAM_WOULD_BLOCK) {
        rv = mSocketOutCondition;
      }
      again = false;
    } else if (!sendBytes) {
      if (mSendFin) {
        mSession->CloseSendingSide(mStreamId);
      }
      mSendState = SEND_DONE;
      rv = NS_OK;
      again = false;
    }
    // write more to the socket until error or end-of-request...
  } while (again && gHttpHandler->Active());
  return rv;
}

nsresult Http3WebTransportStream::OnWriteSegment(char* buf, uint32_t count,
                                                 uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::OnWriteSegment [this=%p]", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult Http3WebTransportStream::WriteSegments() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::WriteSegments [this=%p]", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool Http3WebTransportStream::Done() const {
  // To be implemented in bug 1790403.
  return false;
}

void Http3WebTransportStream::Close(nsresult aResult) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::Close [this=%p]", this));
}

void Http3WebTransportStream::SendFin() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::SendFin [this=%p]", this));

  mSendFin = true;
  // To make Http3WebTransportStream::ReadSegments be called.
  mSession->StreamHasDataToWrite(this);
}

void Http3WebTransportStream::ResetInternal(bool aDispatch) {
  if (aDispatch) {
    RefPtr<Http3WebTransportStream> self = this;
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "Http3WebTransportStream::ResetInternal", [self]() {
          self->mSession->ResetWebTransportStream(self, *self->mResetError);
        }));
    return;
  }

  mSession->ResetWebTransportStream(this, *mResetError);
}

void Http3WebTransportStream::Reset(uint8_t aErrorCode) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::Reset [this=%p, mSendState=%d]", this,
       mSendState));

  if (mResetError) {
    // The stream is already reset.
    return;
  }

  mResetError = Some(aErrorCode);

  switch (mSendState) {
    case SENDING: {
      // If we are still sending, we can't reset the stream immediately, since
      // neqo could drop the last piece of data.
      // TODO: We should come up a better solution in bug 1799636.
      ResetInternal(true);
    } break;
    case SEND_DONE: {
      ResetInternal(false);
    } break;
    default:
      MOZ_ASSERT_UNREACHABLE("invalid mSendState!");
      break;
  }
}

}  // namespace mozilla::net
