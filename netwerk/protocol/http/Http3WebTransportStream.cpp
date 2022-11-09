/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Http3WebTransportStream.h"

#include "HttpLog.h"
#include "Http3Session.h"
#include "Http3WebTransportSession.h"
#include "nsHttpHandler.h"
#include "nsSocketTransportService2.h"

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

}  // namespace

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
        // TODO: To be implemented.
        rv = NS_BASE_STREAM_CLOSED;
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
         " read=%u sock-cond=%" PRIx32 " again=%d [this=%p]",
         static_cast<uint32_t>(rv), sendBytes,
         static_cast<uint32_t>(mSocketOutCondition), again, this));

    // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
    if (rv == NS_BASE_STREAM_CLOSED) {
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
  // Will be implemented in Bug 1790402
}

void Http3WebTransportStream::Reset(uint8_t aErrorCode) {
  // Will be implemented in Bug 1790402
}

}  // namespace mozilla::net
