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
      nsTArray<RefPtr<nsAHttpTransaction>>& outTransactions) override {
    return NS_OK;
  }

 private:
  virtual ~DummyWebTransportStreamTransaction() = default;
};

NS_IMPL_ISUPPORTS(DummyWebTransportStreamTransaction, nsISupportsWeakReference)

class WebTransportSendStreamStats : public nsIWebTransportSendStreamStats {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WebTransportSendStreamStats(uint64_t aSent, uint64_t aAcked)
      : mTimeStamp(TimeStamp::Now()),
        mTotalSent(aSent),
        mTotalAcknowledged(aAcked) {}

  NS_IMETHOD GetTimestamp(mozilla::TimeStamp* aTimestamp) override {
    *aTimestamp = mTimeStamp;
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
  uint64_t mTotalSent;
  uint64_t mTotalAcknowledged;
};

NS_IMPL_ISUPPORTS(WebTransportSendStreamStats, nsIWebTransportSendStreamStats)

class WebTransportReceiveStreamStats
    : public nsIWebTransportReceiveStreamStats {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WebTransportReceiveStreamStats(uint64_t aReceived)
      : mTimeStamp(TimeStamp::Now()), mTotalReceived(aReceived) {}

  NS_IMETHOD GetTimestamp(mozilla::TimeStamp* aTimestamp) override {
    *aTimestamp = mTimeStamp;
    return NS_OK;
  }
  NS_IMETHOD GetBytesReceived(uint64_t* aByteReceived) override {
    *aByteReceived = mTotalReceived;
    return NS_OK;
  }

 private:
  virtual ~WebTransportReceiveStreamStats() = default;

  TimeStamp mTimeStamp;
  uint64_t mTotalReceived;
};

NS_IMPL_ISUPPORTS(WebTransportReceiveStreamStats,
                  nsIWebTransportReceiveStreamStats)

}  // namespace

NS_IMPL_ISUPPORTS(Http3WebTransportStream, nsIInputStreamCallback)

Http3WebTransportStream::Http3WebTransportStream(
    Http3Session* aSession, uint64_t aSessionId, WebTransportStreamType aType,
    std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
        aCallback)
    : Http3StreamBase(new DummyWebTransportStreamTransaction(), aSession),
      mSessionId(aSessionId),
      mStreamType(aType),
      mStreamRole(OUTGOING),
      mStreamReadyCallback(std::move(aCallback)) {
  LOG(("Http3WebTransportStream outgoing ctor %p", this));
}

Http3WebTransportStream::Http3WebTransportStream(Http3Session* aSession,
                                                 uint64_t aSessionId,
                                                 WebTransportStreamType aType,
                                                 uint64_t aStreamId)
    : Http3StreamBase(new DummyWebTransportStreamTransaction(), aSession),
      mSessionId(aSessionId),
      mStreamType(aType),
      mStreamRole(INCOMING),
      // WAITING_DATA indicates we are waiting
      // Http3WebTransportStream::OnInputStreamReady to be called.
      mSendState(WAITING_DATA),
      mStreamReadyCallback(nullptr) {
  LOG(("Http3WebTransportStream incoming ctor %p", this));
  mStreamId = aStreamId;
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
  LOG1(
      ("Http3WebTransportStream::OnInputStreamReady [this=%p stream=%p "
       "state=%d]",
       this, aStream, mSendState));
  if (mSendState == SEND_DONE) {
    // already closed
    return NS_OK;
  }

  mSendState = SENDING;
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

  nsresult rv =
      mSendStreamPipeIn->AsyncWait(this, 0, 0, gSocketTransportService);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mSendState = WAITING_DATA;
  return NS_OK;
}

nsresult Http3WebTransportStream::InitInputPipe() {
  nsCOMPtr<nsIAsyncOutputStream> out;
  nsCOMPtr<nsIAsyncInputStream> in;
  NS_NewPipe2(getter_AddRefs(in), getter_AddRefs(out), true, true,
              nsIOService::gDefaultSegmentSize,
              nsIOService::gDefaultSegmentCount);

  {
    MutexAutoLock lock(mMutex);
    mReceiveStreamPipeIn = std::move(in);
    mReceiveStreamPipeOut = std::move(out);
  }

  mRecvState = READING;
  return NS_OK;
}

void Http3WebTransportStream::GetWriterAndReader(
    nsIAsyncOutputStream** aOutOutputStream,
    nsIAsyncInputStream** aOutInputStream) {
  nsCOMPtr<nsIAsyncOutputStream> output;
  nsCOMPtr<nsIAsyncInputStream> input;
  {
    MutexAutoLock lock(mMutex);
    output = mSendStreamPipeOut;
    input = mReceiveStreamPipeIn;
  }

  output.forget(aOutOutputStream);
  input.forget(aOutInputStream);
}

already_AddRefed<nsIWebTransportSendStreamStats>
Http3WebTransportStream::GetSendStreamStats() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsCOMPtr<nsIWebTransportSendStreamStats> stats =
      new WebTransportSendStreamStats(mTotalSent, mTotalAcknowledged);
  return stats.forget();
}

already_AddRefed<nsIWebTransportReceiveStreamStats>
Http3WebTransportStream::GetReceiveStreamStats() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsCOMPtr<nsIWebTransportReceiveStreamStats> stats =
      new WebTransportReceiveStreamStats(mTotalReceived);
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
        mStreamReadyCallback = nullptr;
        break;
      }

      rv = InitOutputPipe();
      if (NS_SUCCEEDED(rv) && mStreamType == WebTransportStreamType::BiDi) {
        rv = InitInputPipe();
      }
      if (NS_FAILED(rv)) {
        LOG3(
            ("Http3WebTransportStream::OnReadSegment %p failed to create pipe "
             "error=0x%" PRIx32 ".",
             this, static_cast<uint32_t>(rv)));
        mSendState = SEND_DONE;
        mStreamReadyCallback(Err(rv));
        mStreamReadyCallback = nullptr;
        break;
      }

      // Successfully activated.
      mStreamReadyCallback(RefPtr{this});
      mStreamReadyCallback = nullptr;
      break;
    case SENDING: {
      rv = mSession->SendRequestBody(mStreamId, buf, count, countRead);
      LOG3(
          ("Http3WebTransportStream::OnReadSegment %p sending body returns "
           "error=0x%" PRIx32 ".",
           this, static_cast<uint32_t>(rv)));
      mTotalSent += *countRead;
    } break;
    case WAITING_DATA:
      // Still waiting
      LOG3((
          "Http3WebTransportStream::OnReadSegment %p Still waiting for data...",
          this));
      break;
    case SEND_DONE:
      LOG3(("Http3WebTransportStream::OnReadSegment %p called after SEND_DONE ",
            this));
      MOZ_ASSERT(false, "We are done sending this request!");
      MOZ_ASSERT(mStreamReadyCallback);
      rv = NS_ERROR_UNEXPECTED;
      mStreamReadyCallback(Err(rv));
      mStreamReadyCallback = nullptr;
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
        if (mSendState != WAITING_DATA) {
          break;
        }
      }
        [[fallthrough]];
      case WAITING_DATA:
        [[fallthrough]];
      case SENDING: {
        if (mStreamRole == INCOMING &&
            mStreamType == WebTransportStreamType::UniDi) {
          rv = NS_OK;
          break;
        }
        mSendState = SENDING;
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
    if (rv == NS_BASE_STREAM_CLOSED || !mPendingTasks.IsEmpty()) {
      rv = NS_OK;
      sendBytes = 0;
    }

    if (NS_FAILED(rv)) {
      // if the writer didn't want to write any more data, then
      // wait for the transaction to call ResumeSend.
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        mSendState = WAITING_DATA;
        rv = mSendStreamPipeIn->AsyncWait(this, 0, 0, gSocketTransportService);
      }
      again = false;

      // Got a  WebTransport specific error
      if (rv >= NS_ERROR_WEBTRANSPORT_CODE_BASE &&
          rv <= NS_ERROR_WEBTRANSPORT_CODE_END) {
        uint8_t errorCode = GetWebTransportErrorFromNSResult(rv);
        mSendState = SEND_DONE;
        Reset(WebTransportErrorToHttp3Error(errorCode));
        rv = NS_OK;
      }
    } else if (NS_FAILED(mSocketOutCondition)) {
      if (mSocketOutCondition != NS_BASE_STREAM_WOULD_BLOCK) {
        rv = mSocketOutCondition;
      }
      again = false;
    } else if (!sendBytes) {
      mSendState = SEND_DONE;
      rv = NS_OK;
      again = false;
      if (!mPendingTasks.IsEmpty()) {
        LOG(("Has pending tasks to do"));
        nsTArray<std::function<void()>> tasks = std::move(mPendingTasks);
        for (const auto& task : tasks) {
          task();
        }
      }
      // Tell the underlying stream we're done
      SendFin();
    }

    // write more to the socket until error or end-of-request...
  } while (again && gHttpHandler->Active());
  return rv;
}

nsresult Http3WebTransportStream::OnWriteSegment(char* buf, uint32_t count,
                                                 uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3WebTransportStream::OnWriteSegment [this=%p, state=%d", this,
       static_cast<uint32_t>(mRecvState)));
  nsresult rv = NS_OK;
  switch (mRecvState) {
    case READING: {
      rv = mSession->ReadResponseData(mStreamId, buf, count, countWritten,
                                      &mFin);
      if (*countWritten == 0) {
        if (mFin) {
          mRecvState = RECV_DONE;
          rv = NS_BASE_STREAM_CLOSED;
        } else {
          rv = NS_BASE_STREAM_WOULD_BLOCK;
        }
      } else {
        mTotalReceived += *countWritten;
        if (mFin) {
          mRecvState = RECEIVED_FIN;
        }
      }
    } break;
    case RECEIVED_FIN:
      rv = NS_BASE_STREAM_CLOSED;
      mRecvState = RECV_DONE;
      break;
    case RECV_DONE:
      rv = NS_ERROR_UNEXPECTED;
      break;
    default:
      rv = NS_ERROR_UNEXPECTED;
      break;
  }

  // Remember the error received from lower layers. A stream pipe may overwrite
  // it.
  // If rv == NS_OK this will reset mSocketInCondition.
  mSocketInCondition = rv;

  return rv;
}

// static
nsresult Http3WebTransportStream::WritePipeSegment(nsIOutputStream* stream,
                                                   void* closure, char* buf,
                                                   uint32_t offset,
                                                   uint32_t count,
                                                   uint32_t* countWritten) {
  Http3WebTransportStream* self = (Http3WebTransportStream*)closure;

  nsresult rv = self->OnWriteSegment(buf, count, countWritten);
  if (NS_FAILED(rv)) {
    return rv;
  }

  LOG(("Http3WebTransportStream::WritePipeSegment %p written=%u", self,
       *countWritten));

  return rv;
}

nsresult Http3WebTransportStream::WriteSegments() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (!mReceiveStreamPipeOut) {
    return NS_OK;
  }

  LOG(("Http3WebTransportStream::WriteSegments [this=%p]", this));

  nsresult rv = NS_OK;
  uint32_t countWrittenSingle = 0;
  bool again = true;

  do {
    mSocketInCondition = NS_OK;
    countWrittenSingle = 0;
    rv = mReceiveStreamPipeOut->WriteSegments(WritePipeSegment, this,
                                              nsIOService::gDefaultSegmentSize,
                                              &countWrittenSingle);
    LOG(("Http3WebTransportStream::WriteSegments rv=0x%" PRIx32
         " countWrittenSingle=%" PRIu32 " socketin=%" PRIx32 " [this=%p]",
         static_cast<uint32_t>(rv), countWrittenSingle,
         static_cast<uint32_t>(mSocketInCondition), this));
    if (NS_FAILED(rv)) {
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        rv = NS_OK;
      }
      again = false;
    } else if (NS_FAILED(mSocketInCondition)) {
      if (mSocketInCondition != NS_BASE_STREAM_WOULD_BLOCK) {
        rv = mSocketInCondition;
        if (rv == NS_BASE_STREAM_CLOSED) {
          mReceiveStreamPipeOut->Close();
          rv = NS_OK;
        }
      }
      again = false;
    }
    // read more from the socket until error...
  } while (again && gHttpHandler->Active());

  return rv;
}

bool Http3WebTransportStream::Done() const {
  return mSendState == SEND_DONE && mRecvState == RECV_DONE;
}

void Http3WebTransportStream::Close(nsresult aResult) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::Close [this=%p]", this));
  mTransaction = nullptr;
  if (mSendStreamPipeIn) {
    mSendStreamPipeIn->AsyncWait(nullptr, 0, 0, nullptr);
    mSendStreamPipeIn->CloseWithStatus(aResult);
  }
  if (mReceiveStreamPipeOut) {
    mReceiveStreamPipeOut->AsyncWait(nullptr, 0, 0, nullptr);
    mReceiveStreamPipeOut->CloseWithStatus(aResult);
  }
  mSendState = SEND_DONE;
  mRecvState = RECV_DONE;
  mSession = nullptr;
}

void Http3WebTransportStream::SendFin() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::SendFin [this=%p mSendState=%d]", this,
       mSendState));

  if (mSendFin || !mSession || mResetError) {
    // Already closed.
    return;
  }

  mSendFin = true;

  switch (mSendState) {
    case SENDING: {
      mPendingTasks.AppendElement([self = RefPtr{this}]() {
        self->mSession->CloseSendingSide(self->mStreamId);
      });
    } break;
    case WAITING_DATA:
      mSendState = SEND_DONE;
      [[fallthrough]];
    case SEND_DONE:
      mSession->CloseSendingSide(mStreamId);
      // StreamHasDataToWrite needs to be called to trigger ProcessOutput.
      mSession->StreamHasDataToWrite(this);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("invalid mSendState!");
      break;
  }
}

void Http3WebTransportStream::Reset(uint64_t aErrorCode) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::Reset [this=%p, mSendState=%d]", this,
       mSendState));

  if (mResetError || !mSession || mSendFin) {
    // The stream is already reset.
    return;
  }

  mResetError = Some(aErrorCode);

  switch (mSendState) {
    case SENDING: {
      LOG(("Http3WebTransportStream::Reset [this=%p] reset after sending data",
           this));
      mPendingTasks.AppendElement([self = RefPtr{this}]() {
        // "Reset" needs a special treatment here. If we are sending data and
        // ResetWebTransportStream is called before Http3Session::ProcessOutput,
        // neqo will drop the last piece of data.
        NS_DispatchToCurrentThread(
            NS_NewRunnableFunction("Http3WebTransportStream::Reset", [self]() {
              self->mSession->ResetWebTransportStream(self, *self->mResetError);
              self->mSession->StreamHasDataToWrite(self);
              self->mSession->ConnectSlowConsumer(self);
            }));
      });
    } break;
    case WAITING_DATA:
      mSendState = SEND_DONE;
      [[fallthrough]];
    case SEND_DONE:
      mSession->ResetWebTransportStream(this, *mResetError);
      // StreamHasDataToWrite needs to be called to trigger ProcessOutput.
      mSession->StreamHasDataToWrite(this);
      mSession->ConnectSlowConsumer(this);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("invalid mSendState!");
      break;
  }
}

void Http3WebTransportStream::SendStopSending(uint8_t aErrorCode) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportStream::SendStopSending [this=%p, mSendState=%d]",
       this, mSendState));

  if (mSendState == WAITING_TO_ACTIVATE) {
    return;
  }

  if (mStopSendingError || !mSession) {
    return;
  }

  mStopSendingError = Some(aErrorCode);

  mSession->StreamStopSending(this, *mStopSendingError);
  // StreamHasDataToWrite needs to be called to trigger ProcessOutput.
  mSession->StreamHasDataToWrite(this);
}

void Http3WebTransportStream::SetSendOrder(Maybe<int64_t> aSendOrder) {
  mSession->SetSendOrder(this, aSendOrder);
}

}  // namespace mozilla::net
