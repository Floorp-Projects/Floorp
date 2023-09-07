/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"
#include "Http3WebTransportSession.h"
#include "Http3WebTransportStream.h"
#include "Http3Session.h"
#include "Http3Stream.h"
#include "nsHttpRequestHead.h"
#include "nsHttpTransaction.h"
#include "nsIClassOfService.h"
#include "nsISocketTransport.h"
#include "nsSocketTransportService2.h"
#include "nsIOService.h"
#include "nsHttpHandler.h"

namespace mozilla::net {

Http3WebTransportSession::Http3WebTransportSession(nsAHttpTransaction* trans,
                                                   Http3Session* aHttp3Session)
    : Http3StreamBase(trans, aHttp3Session) {}

Http3WebTransportSession::~Http3WebTransportSession() = default;

nsresult Http3WebTransportSession::ReadSegments() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportSession::ReadSegments %p mSendState=%d mRecvState=%d",
       this, mSendState, mRecvState));
  if (mSendState == PROCESSING_DATAGRAM) {
    return NS_OK;
  }

  if ((mRecvState == RECV_DONE) || (mRecvState == ACTIVE) ||
      (mRecvState == CLOSE_PENDING)) {
    // Don't transmit any request frames if the peer cannot respond or respone
    // is already done.
    LOG3((
        "Http3WebTransportSession %p ReadSegments request stream aborted due to"
        " response side closure\n",
        this));
    return NS_ERROR_ABORT;
  }

  nsresult rv = NS_OK;
  uint32_t transactionBytes;
  bool again = true;
  do {
    transactionBytes = 0;
    rv = mSocketOutCondition = NS_OK;
    LOG(("Http3WebTransportSession::ReadSegments state=%d [this=%p]",
         mSendState, this));
    switch (mSendState) {
      case PREPARING_HEADERS: {
        rv = mTransaction->ReadSegmentsAgain(
            this, nsIOService::gDefaultSegmentSize, &transactionBytes, &again);
      } break;
      case WAITING_TO_ACTIVATE: {
        // A transaction that had already generated its headers before it was
        // queued at the session level (due to concurrency concerns) may not
        // call onReadSegment off the ReadSegments() stack above.
        LOG3(
            ("Http3WebTransportSession %p ReadSegments forcing OnReadSegment "
             "call\n",
             this));
        uint32_t wasted = 0;
        nsresult rv2 = OnReadSegment("", 0, &wasted);
        LOG3(("  OnReadSegment returned 0x%08" PRIx32,
              static_cast<uint32_t>(rv2)));
      } break;
      default:
        transactionBytes = 0;
        rv = NS_OK;
        break;
    }

    LOG(("Http3WebTransportSession::ReadSegments rv=0x%" PRIx32
         " read=%u sock-cond=%" PRIx32 " again=%d [this=%p]",
         static_cast<uint32_t>(rv), transactionBytes,
         static_cast<uint32_t>(mSocketOutCondition), again, this));

    // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
    if (rv == NS_BASE_STREAM_CLOSED && !mTransaction->IsDone()) {
      rv = NS_OK;
      transactionBytes = 0;
    }

    if (NS_FAILED(rv)) {
      // if the transaction didn't want to write any more data, then
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
    } else if (!transactionBytes) {
      mTransaction->OnTransportStatus(nullptr, NS_NET_STATUS_WAITING_FOR, 0);

      mSendState = PROCESSING_DATAGRAM;
      rv = NS_OK;
      again = false;
    }
    // write more to the socket until error or end-of-request...
  } while (again && gHttpHandler->Active());
  return rv;
}

bool Http3WebTransportSession::ConsumeHeaders(const char* buf, uint32_t avail,
                                              uint32_t* countUsed) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http3WebTransportSession::ConsumeHeaders %p avail=%u.", this, avail));

  mFlatHttpRequestHeaders.Append(buf, avail);
  // We can use the simple double crlf because firefox is the
  // only client we are parsing
  int32_t endHeader = mFlatHttpRequestHeaders.Find("\r\n\r\n");

  if (endHeader == kNotFound) {
    // We don't have all the headers yet
    LOG3(
        ("Http3WebTransportSession::ConsumeHeaders %p "
         "Need more header bytes. Len = %zu",
         this, mFlatHttpRequestHeaders.Length()));
    *countUsed = avail;
    return false;
  }

  uint32_t oldLen = mFlatHttpRequestHeaders.Length();
  mFlatHttpRequestHeaders.SetLength(endHeader + 2);
  *countUsed = avail - (oldLen - endHeader) + 4;

  return true;
}

nsresult Http3WebTransportSession::TryActivating() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportSession::TryActivating [this=%p]", this));
  nsHttpRequestHead* head = mTransaction->RequestHead();

  nsAutoCString host;
  nsresult rv = head->GetHeader(nsHttp::Host, host);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false);
    return rv;
  }
  nsAutoCString path;
  head->Path(path);

  return mSession->TryActivating(""_ns, ""_ns, host, path,
                                 mFlatHttpRequestHeaders, &mStreamId, this);
}

nsresult Http3WebTransportSession::OnReadSegment(const char* buf,
                                                 uint32_t count,
                                                 uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3WebTransportSession::OnReadSegment count=%u state=%d [this=%p]",
       count, mSendState, this));

  nsresult rv = NS_OK;

  switch (mSendState) {
    case PREPARING_HEADERS: {
      if (!ConsumeHeaders(buf, count, countRead)) {
        break;
      }
      mSendState = WAITING_TO_ACTIVATE;
    }
      [[fallthrough]];
    case WAITING_TO_ACTIVATE:
      rv = TryActivating();
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        LOG3(
            ("Http3WebTransportSession::OnReadSegment %p cannot activate now. "
             "queued.\n",
             this));
        break;
      }
      if (NS_FAILED(rv)) {
        LOG3(
            ("Http3WebTransportSession::OnReadSegment %p cannot activate "
             "error=0x%" PRIx32 ".",
             this, static_cast<uint32_t>(rv)));
        break;
      }

      // Successfully activated.
      mTransaction->OnTransportStatus(nullptr, NS_NET_STATUS_SENDING_TO, 0);

      mSendState = PROCESSING_DATAGRAM;
      break;
    default:
      MOZ_ASSERT(false, "We are done sending this request!");
      rv = NS_ERROR_UNEXPECTED;
      break;
  }

  mSocketOutCondition = rv;

  return mSocketOutCondition;
}

nsresult Http3WebTransportSession::WriteSegments() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportSession::WriteSegments [this=%p]", this));
  nsresult rv = NS_OK;
  uint32_t countWrittenSingle = 0;
  bool again = true;

  if (mRecvState == CLOSE_PENDING) {
    mSession->CloseWebTransport(mStreamId, mStatus, mReason);
    mRecvState = RECV_DONE;
    // This will closed the steam because the stream is Done().
    return NS_OK;
  }

  do {
    mSocketInCondition = NS_OK;
    countWrittenSingle = 0;
    rv = mTransaction->WriteSegmentsAgain(
        this, nsIOService::gDefaultSegmentSize, &countWrittenSingle, &again);
    LOG(("Http3WebTransportSession::WriteSegments rv=0x%" PRIx32
         " countWrittenSingle=%" PRIu32 " socketin=%" PRIx32 " [this=%p]",
         static_cast<uint32_t>(rv), countWrittenSingle,
         static_cast<uint32_t>(mSocketInCondition), this));
    if (mTransaction->IsDone()) {
      // An HTTP transaction used for setting up a  WebTransport session will
      // receive only response headers and afterward, it will be marked as
      // done. At this point, the session negotiation has finished and the
      // WebTransport session transfers into the ACTIVE state.
      mRecvState = ACTIVE;
    }

    if (NS_FAILED(rv)) {
      // if the transaction didn't want to take any more data, then
      // wait for the transaction to call ResumeRecv.
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        rv = NS_OK;
      }
      again = false;
    } else if (NS_FAILED(mSocketInCondition)) {
      if (mSocketInCondition != NS_BASE_STREAM_WOULD_BLOCK) {
        rv = mSocketInCondition;
      }
      again = false;
    }
    // read more from the socket until error...
  } while (again && gHttpHandler->Active());

  return rv;
}

void Http3WebTransportSession::SetResponseHeaders(
    nsTArray<uint8_t>& aResponseHeaders, bool fin, bool interim) {
  MOZ_ASSERT(mRecvState == BEFORE_HEADERS ||
             mRecvState == READING_INTERIM_HEADERS);
  mFlatResponseHeaders.AppendElements(aResponseHeaders);
  mRecvState = (interim) ? READING_INTERIM_HEADERS : READING_HEADERS;
}

nsresult Http3WebTransportSession::OnWriteSegment(char* buf, uint32_t count,
                                                  uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3WebTransportSession::OnWriteSegment [this=%p, state=%d", this,
       mRecvState));
  nsresult rv = NS_OK;
  switch (mRecvState) {
    case BEFORE_HEADERS: {
      *countWritten = 0;
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    } break;
    case READING_HEADERS:
    case READING_INTERIM_HEADERS: {
      // SetResponseHeaders should have been previously called.
      MOZ_ASSERT(!mFlatResponseHeaders.IsEmpty(), "Headers empty!");
      *countWritten = (mFlatResponseHeaders.Length() > count)
                          ? count
                          : mFlatResponseHeaders.Length();
      memcpy(buf, mFlatResponseHeaders.Elements(), *countWritten);

      mFlatResponseHeaders.RemoveElementsAt(0, *countWritten);
      if (mFlatResponseHeaders.Length() == 0) {
        if (mRecvState == READING_INTERIM_HEADERS) {
          // neqo makes sure that fin cannot be received before the final
          // headers are received.
          mRecvState = BEFORE_HEADERS;
        } else {
          mRecvState = ACTIVE;
        }
      }

      if (*countWritten == 0) {
        rv = NS_BASE_STREAM_WOULD_BLOCK;
      } else {
        mTransaction->OnTransportStatus(nullptr, NS_NET_STATUS_RECEIVING_FROM,
                                        0);
      }
    } break;
    case ACTIVE:
    case CLOSE_PENDING:
    case RECV_DONE:
      rv = NS_ERROR_UNEXPECTED;
  }

  // Remember the error received from lower layers. A stream pipe may overwrite
  // it.
  // If rv == NS_OK this will reset mSocketInCondition.
  mSocketInCondition = rv;

  return rv;
}

void Http3WebTransportSession::Close(nsresult aResult) {
  LOG(("Http3WebTransportSession::Close %p", this));
  if (mListener) {
    mListener->OnSessionClosed(NS_SUCCEEDED(aResult), 0, ""_ns);
    mListener = nullptr;
  }
  if (mTransaction) {
    mTransaction->Close(aResult);
    mTransaction = nullptr;
  }
  mRecvState = RECV_DONE;
  mSendState = SEND_DONE;

  if (mSession) {
    mSession->CloseWebTransportConn();
    mSession = nullptr;
  }
}

void Http3WebTransportSession::OnSessionClosed(bool aCleanly, uint32_t aStatus,
                                               const nsACString& aReason) {
  if (mTransaction) {
    mTransaction->Close(NS_BASE_STREAM_CLOSED);
    mTransaction = nullptr;
  }
  if (mListener) {
    mListener->OnSessionClosed(aCleanly, aStatus, aReason);
    mListener = nullptr;
  }
  mRecvState = RECV_DONE;
  mSendState = SEND_DONE;

  mSession->CloseWebTransportConn();
}

void Http3WebTransportSession::CloseSession(uint32_t aStatus,
                                            const nsACString& aReason) {
  if ((mRecvState != CLOSE_PENDING) && (mRecvState != RECV_DONE)) {
    mStatus = aStatus;
    mReason = aReason;
    mSession->ConnectSlowConsumer(this);
    mRecvState = CLOSE_PENDING;
    mSendState = SEND_DONE;
  }
  mListener = nullptr;
}

void Http3WebTransportSession::TransactionIsDone(nsresult aResult) {
  mTransaction->Close(aResult);
  mTransaction = nullptr;
}

void Http3WebTransportSession::CreateOutgoingBidirectionalStream(
    std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
        aCallback) {
  return CreateStreamInternal(true, std::move(aCallback));
}

void Http3WebTransportSession::CreateOutgoingUnidirectionalStream(
    std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
        aCallback) {
  return CreateStreamInternal(false, std::move(aCallback));
}

void Http3WebTransportSession::CreateStreamInternal(
    bool aBidi,
    std::function<void(Result<RefPtr<Http3WebTransportStream>, nsresult>&&)>&&
        aCallback) {
  LOG(("Http3WebTransportSession::CreateStreamInternal this=%p aBidi=%d", this,
       aBidi));
  if (mRecvState != ACTIVE) {
    aCallback(Err(NS_ERROR_NOT_AVAILABLE));
    return;
  }

  RefPtr<Http3WebTransportStream> stream =
      aBidi ? new Http3WebTransportStream(mSession, mStreamId,
                                          WebTransportStreamType::BiDi,
                                          std::move(aCallback))
            : new Http3WebTransportStream(mSession, mStreamId,
                                          WebTransportStreamType::UniDi,
                                          std::move(aCallback));
  mSession->StreamHasDataToWrite(stream);
  // Put the newly created stream in to |mStreams| to keep it alive.
  mStreams.AppendElement(std::move(stream));
}

// This is called by Http3Session::TryActivatingWebTransportStream. When called,
// this means a WebTransport stream is successfully activated and the stream
// will be managed by Http3Session.
void Http3WebTransportSession::RemoveWebTransportStream(
    Http3WebTransportStream* aStream) {
  LOG(
      ("Http3WebTransportSession::RemoveWebTransportStream "
       "this=%p aStream=%p",
       this, aStream));
  DebugOnly<bool> existed = mStreams.RemoveElement(aStream);
  MOZ_ASSERT(existed);
}

already_AddRefed<Http3WebTransportStream>
Http3WebTransportSession::OnIncomingWebTransportStream(
    WebTransportStreamType aType, uint64_t aId) {
  LOG(
      ("Http3WebTransportSession::OnIncomingWebTransportStream "
       "this=%p",
       this));

  if (mRecvState != ACTIVE) {
    return nullptr;
  }

  MOZ_ASSERT(!mTransaction);
  RefPtr<Http3WebTransportStream> stream =
      new Http3WebTransportStream(mSession, mStreamId, aType, aId);
  if (NS_FAILED(stream->InitInputPipe())) {
    return nullptr;
  }

  if (aType == WebTransportStreamType::BiDi) {
    if (NS_FAILED(stream->InitOutputPipe())) {
      return nullptr;
    }
  }

  if (!mListener) {
    return nullptr;
  }

  mListener->OnIncomingStreamAvailableInternal(stream);
  return stream.forget();
}

void Http3WebTransportSession::SendDatagram(nsTArray<uint8_t>&& aData,
                                            uint64_t aTrackingId) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportSession::SendDatagram this=%p", this));
  if (mSendState != PROCESSING_DATAGRAM) {
    return;
  }

  mSession->SendDatagram(this, aData, aTrackingId);
  mSession->StreamHasDataToWrite(this);
}

void Http3WebTransportSession::OnDatagramReceived(nsTArray<uint8_t>&& aData) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportSession::OnDatagramReceived this=%p", this));
  if (mRecvState != ACTIVE || !mListener) {
    return;
  }

  mListener->OnDatagramReceivedInternal(std::move(aData));
}

void Http3WebTransportSession::GetMaxDatagramSize() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (mRecvState != ACTIVE || !mListener) {
    return;
  }

  uint64_t size = mSession->MaxDatagramSize(mStreamId);
  mListener->OnMaxDatagramSize(size);
}

void Http3WebTransportSession::OnOutgoingDatagramOutCome(
    uint64_t aId, WebTransportSessionEventListener::DatagramOutcome aOutCome) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3WebTransportSession::OnOutgoingDatagramOutCome this=%p id=%" PRIx64
       ", outCome=%d mRecvState=%d",
       this, aId, static_cast<uint32_t>(aOutCome), mRecvState));
  if (mRecvState != ACTIVE || !mListener || !aId) {
    return;
  }

  mListener->OnOutgoingDatagramOutCome(aId, aOutCome);
}

void Http3WebTransportSession::OnStreamStopSending(uint64_t aId,
                                                   nsresult aError) {
  LOG(("OnStreamStopSending id:%" PRId64, aId));
  if (!mListener) {
    return;
  }

  mListener->OnStopSending(aId, aError);
}

void Http3WebTransportSession::OnStreamReset(uint64_t aId, nsresult aError) {
  LOG(("OnStreamReset id:%" PRId64, aId));
  if (!mListener) {
    return;
  }

  mListener->OnResetReceived(aId, aError);
}

}  // namespace mozilla::net
