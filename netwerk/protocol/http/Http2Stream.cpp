/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Http2Stream.h"
#include "nsHttp.h"

namespace mozilla::net {

Http2Stream::~Http2Stream() { ClearPushSource(); }

void Http2Stream::Close(nsresult reason) {
  // In case we are connected to a push, make sure the push knows we are closed,
  // so it doesn't try to give us any more DATA that comes on it after our
  // close.
  ClearPushSource();

  mTransaction->Close(reason);
  mSession = nullptr;
}

void Http2Stream::ClearPushSource() {
  if (mPushSource) {
    mPushSource->SetConsumerStream(nullptr);
    mPushSource = nullptr;
  }
}

nsresult Http2Stream::CheckPushCache() {
  nsHttpRequestHead* head = mTransaction->RequestHead();

  // check the push cache for GET
  if (!head->IsGet()) {
    return NS_OK;
  }

  RefPtr<Http2Session> session = Session();

  nsAutoCString authorityHeader;
  nsAutoCString hashkey;
  nsresult rv = head->GetHeader(nsHttp::Host, authorityHeader);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false);
    return rv;
  }

  nsAutoCString requestURI;
  head->RequestURI(requestURI);

  mozilla::OriginAttributes originAttributes;
  mSocketTransport->GetOriginAttributes(&originAttributes);

  CreatePushHashKey(nsDependentCString(head->IsHTTPS() ? "https" : "http"),
                    authorityHeader, originAttributes, session->Serial(),
                    requestURI, mOrigin, hashkey);

  // from :scheme, :authority, :path
  nsIRequestContext* requestContext = mTransaction->RequestContext();
  SpdyPushCache* cache = nullptr;
  if (requestContext) {
    cache = requestContext->GetSpdyPushCache();
  }

  RefPtr<Http2PushedStreamWrapper> pushedStreamWrapper;
  Http2PushedStream* pushedStream = nullptr;

  // If a push stream is attached to the transaction via onPush, match only
  // with that one. This occurs when a push was made with in conjunction with
  // a nsIHttpPushListener
  nsHttpTransaction* trans = mTransaction->QueryHttpTransaction();
  if (trans && (pushedStreamWrapper = trans->TakePushedStream()) &&
      (pushedStream = pushedStreamWrapper->GetStream())) {
    RefPtr<Http2Session> pushSession = pushedStream->Session();
    if (pushSession == session) {
      LOG3(
          ("Pushed Stream match based on OnPush correlation %p", pushedStream));
    } else {
      LOG3(("Pushed Stream match failed due to stream mismatch %p %" PRId64
            " %" PRId64 "\n",
            pushedStream, pushSession->Serial(), session->Serial()));
      pushedStream->OnPushFailed();
      pushedStream = nullptr;
    }
  }

  // we remove the pushedstream from the push cache so that
  // it will not be used for another GET. This does not destroy the
  // stream itself - that is done when the transactionhash is done with it.
  if (cache && !pushedStream) {
    pushedStream = cache->RemovePushedStreamHttp2(hashkey);
  }

  LOG3(
      ("Pushed Stream Lookup "
       "session=%p key=%s requestcontext=%p cache=%p hit=%p\n",
       session.get(), hashkey.get(), requestContext, cache, pushedStream));

  if (pushedStream) {
    LOG3(("Pushed Stream Match located %p id=0x%X key=%s\n", pushedStream,
          pushedStream->StreamID(), hashkey.get()));
    pushedStream->SetConsumerStream(this);
    mPushSource = pushedStream;
    SetSentFin(true);
    AdjustPushedPriority();

    // There is probably pushed data buffered so trigger a read manually
    // as we can't rely on future network events to do it
    session->ConnectPushedStream(this);
    mOpenGenerated = 1;

    // if the "mother stream" had TRR, this one is a TRR stream too!
    RefPtr<nsHttpConnectionInfo> ci(Transaction()->ConnectionInfo());
    if (ci && ci->GetIsTrrServiceChannel()) {
      session->IncrementTrrCounter();
    }
  }

  return NS_OK;
}

uint32_t Http2Stream::GetWireStreamId() {
  // >0 even numbered IDs are pushed streams.
  // odd numbered IDs are pulled streams.
  // 0 is the sink for a pushed stream.
  if (!mStreamID) {
    MOZ_ASSERT(mPushSource);
    if (!mPushSource) {
      return 0;
    }

    MOZ_ASSERT(mPushSource->StreamID());
    MOZ_ASSERT(!(mPushSource->StreamID() & 1));  // is a push stream

    // If the pushed stream has recvd a FIN, there is no reason to update
    // the window
    if (mPushSource->RecvdFin() || mPushSource->RecvdReset() ||
        (mPushSource->HTTPState() == RESERVED_BY_REMOTE)) {
      return 0;
    }
    return mPushSource->StreamID();
  }

  if (mState == RESERVED_BY_REMOTE) {
    // h2-14 prevents sending a window update in this state
    return 0;
  }
  return mStreamID;
}

void Http2Stream::AdjustPushedPriority() {
  // >0 even numbered IDs are pushed streams. odd numbered IDs are pulled
  // streams. 0 is the sink for a pushed stream.

  if (mStreamID || !mPushSource) return;

  MOZ_ASSERT(mPushSource->StreamID() && !(mPushSource->StreamID() & 1));

  // If the pushed stream has recvd a FIN, there is no reason to update
  // the window
  if (mPushSource->RecvdFin() || mPushSource->RecvdReset()) return;

  // Ensure we pick up the right dependency to place the pushed stream under.
  UpdatePriorityDependency();

  EnsureBuffer(mTxInlineFrame,
               mTxInlineFrameUsed + Http2Session::kFrameHeaderBytes + 5,
               mTxInlineFrameUsed, mTxInlineFrameSize);
  uint8_t* packet = mTxInlineFrame.get() + mTxInlineFrameUsed;
  mTxInlineFrameUsed += Http2Session::kFrameHeaderBytes + 5;

  RefPtr<Http2Session> session = Session();
  session->CreateFrameHeader(packet, 5, Http2Session::FRAME_TYPE_PRIORITY, 0,
                             mPushSource->StreamID());

  mPushSource->SetPriorityDependency(mPriority, mPriorityDependency);
  uint32_t wireDep = PR_htonl(mPriorityDependency);
  memcpy(packet + Http2Session::kFrameHeaderBytes, &wireDep, 4);
  memcpy(packet + Http2Session::kFrameHeaderBytes + 4, &mPriorityWeight, 1);

  LOG3(("AdjustPushedPriority %p id 0x%X to dep %X weight %X\n", this,
        mPushSource->StreamID(), mPriorityDependency, mPriorityWeight));
}

bool Http2Stream::IsReadingFromPushStream() { return !!mPushSource; }

nsresult Http2Stream::OnWriteSegment(char* buf, uint32_t count,
                                     uint32_t* countWritten) {
  LOG3(("Http2Stream::OnWriteSegment %p count=%d state=%x 0x%X\n", this, count,
        mUpstreamState, mStreamID));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mSegmentWriter);

  if (mPushSource) {
    nsresult rv;
    rv = mPushSource->GetBufferedData(buf, count, countWritten);
    if (NS_FAILED(rv)) return rv;

    RefPtr<Http2Session> session = Session();
    session->ConnectPushedStream(this);
    return NS_OK;
  }

  return Http2StreamBase::OnWriteSegment(buf, count, countWritten);
}

}  // namespace mozilla::net
