/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"
#include "Http3Session.h"
#include "Http3Stream.h"
#include "nsHttpRequestHead.h"
#include "nsISocketTransport.h"
#include "nsSocketTransportService2.h"

#include <stdio.h>

namespace mozilla {
namespace net {

Http3Stream::Http3Stream(nsAHttpTransaction* httpTransaction,
                         Http3Session* session)
    : mState(PREPARING_HEADERS),
      mStreamId(UINT64_MAX),
      mSession(session),
      mTransaction(httpTransaction),
      mRequestHeadersDone(false),
      mRequestStarted(false),
      mQueued(false),
      mRequestBlockedOnRead(false),
      mDataReceived(false),
      mRequestBodyLenRemaining(0),
      mSocketTransport(session->SocketTransport()),
      mActivatingFailed(false),
      mTotalSent(0),
      mTotalRead(0),
      mFin(false) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http3Stream::Http3Stream [this=%p]", this));
}

void Http3Stream::Close(nsresult aResult) { mTransaction->Close(aResult); }

void Http3Stream::GetHeadersString(const char* buf, uint32_t avail,
                                   uint32_t* countUsed) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http3Stream::GetHeadersString %p avail=%u.", this, avail));

  mFlatHttpRequestHeaders.Append(buf, avail);
  // We can use the simple double crlf because firefox is the
  // only client we are parsing
  int32_t endHeader = mFlatHttpRequestHeaders.Find("\r\n\r\n");

  if (endHeader == kNotFound) {
    // We don't have all the headers yet
    LOG3(
        ("Http3Stream::GetHeadersString %p "
         "Need more header bytes. Len = %u",
         this, mFlatHttpRequestHeaders.Length()));
    *countUsed = avail;
    return;
  }

  uint32_t oldLen = mFlatHttpRequestHeaders.Length();
  mFlatHttpRequestHeaders.SetLength(endHeader + 2);
  *countUsed = avail - (oldLen - endHeader) + 4;

  FindRequestContentLength();
  mRequestHeadersDone = true;
}

void Http3Stream::FindRequestContentLength() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  // Look for Content-Length header to find out if we have request body and
  // how long it is.
  int32_t contentLengthStart = mFlatHttpRequestHeaders.Find("Content-Length:");
  if (contentLengthStart == -1) {
    // There is no content-Length.
    return;
  }

  // We have Content-Length header, find the end of it.
  int32_t crlfIndex =
      mFlatHttpRequestHeaders.Find("\r\n", false, contentLengthStart);
  if (crlfIndex == -1) {
    MOZ_ASSERT(false, "We must have \\r\\n at the end of the headers string.");
    return;
  }

  // Find the beginning.
  int32_t valueIndex =
      mFlatHttpRequestHeaders.Find(":", false, contentLengthStart) + 1;
  if (valueIndex > crlfIndex) {
    // Content-Length headers is empty.
    MOZ_ASSERT(false, "Content-Length must have a value.");
    return;
  }

  const char* beginBuffer = mFlatHttpRequestHeaders.BeginReading();
  while (valueIndex < crlfIndex && beginBuffer[valueIndex] == ' ') {
    ++valueIndex;
  }

  nsDependentCSubstring value =
      Substring(beginBuffer + valueIndex, beginBuffer + crlfIndex);

  int64_t len;
  nsCString tmp(value);
  if (nsHttp::ParseInt64(tmp.get(), nullptr, &len)) {
    mRequestBodyLenRemaining = len;
  }
}

nsresult Http3Stream::TryActivating() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3Stream::TryActivating [this=%p]", this));
  nsHttpRequestHead* head = mTransaction->RequestHead();

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

  rv = mSession->TryActivating(method, scheme, authorityHeader, path,
                               mFlatHttpRequestHeaders, &mStreamId, this);
  if (NS_SUCCEEDED(rv)) {
    mRequestStarted = true;
  }

  return rv;
}

nsresult Http3Stream::OnReadSegment(const char* buf, uint32_t count,
                                    uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3Stream::OnReadSegment count=%u state=%d [this=%p]", count, mState,
       this));

  switch (mState) {
    case PREPARING_HEADERS:
      if (mActivatingFailed) {
        MOZ_ASSERT(count, "There must be at least one byte in the buffer.");
        // We already read all headers but TryActivating() failed, so we left
        // one fake byte in the buffer. Read this byte and try to activate the
        // stream again.
        MOZ_ASSERT(mRequestHeadersDone, "Must have already all headers!");
        *countRead = 1;
        mActivatingFailed = false;
      } else {
        GetHeadersString(buf, count, countRead);
        if (*countRead) {
          mTotalSent += *countRead;
        }
      }

      MOZ_ASSERT(!mRequestStarted, "We should be in one of the next states.");
      if (mRequestHeadersDone && !mRequestStarted) {
        nsresult rv = TryActivating();
        if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
          LOG3(("Http3Stream::OnReadSegment %p cannot activate now. queued.\n",
                this));
          if (*countRead) {
            // Keep at least one byte in the buffer to ensure this method is
            // called again and set the flag to ignore this byte.
            --*countRead;
            mActivatingFailed = true;
          }
          return *countRead ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
        }
        if (NS_FAILED(rv)) {
          LOG3(("Http3Stream::OnReadSegment %p cannot activate error=0x%" PRIx32
                ".",
                this, static_cast<uint32_t>(rv)));
          return rv;
        }

        mTransaction->OnTransportStatus(mSocketTransport,
                                        NS_NET_STATUS_SENDING_TO, mTotalSent);
      }

      if (mRequestStarted) {
        if (mRequestBodyLenRemaining) {
          mState = SENDING_BODY;
        } else {
          mTransaction->OnTransportStatus(mSocketTransport,
                                          NS_NET_STATUS_WAITING_FOR, 0);
          mSession->CloseSendingSide(mStreamId);
          mState = READING_HEADERS;
        }
      }
      break;
    case SENDING_BODY: {
      nsresult rv = mSession->SendRequestBody(mStreamId, buf, count, countRead);
      MOZ_ASSERT(mRequestBodyLenRemaining >= *countRead,
                 "We cannot send more that than we promised.");
      if (mRequestBodyLenRemaining < *countRead) {
        rv = NS_ERROR_UNEXPECTED;
      }
      if (NS_FAILED(rv)) {
        LOG3(
            ("Http3Stream::OnReadSegment %p sending body returns "
             "error=0x%" PRIx32 ".",
             this, static_cast<uint32_t>(rv)));
        return rv;
      }

      mRequestBodyLenRemaining -= *countRead;
      if (!mRequestBodyLenRemaining) {
        mTransaction->OnTransportStatus(mSocketTransport,
                                        NS_NET_STATUS_WAITING_FOR, 0);
        mSession->CloseSendingSide(mStreamId);
        mState = READING_HEADERS;
      }
    } break;
    case EARLY_RESPONSE:
      // We do not need to send the rest of the request, so just ignore it.
      *countRead = count;
      mRequestBodyLenRemaining -= count;
      if (!mRequestBodyLenRemaining) {
        mTransaction->OnTransportStatus(mSocketTransport,
                                        NS_NET_STATUS_WAITING_FOR, 0);
        mState = READING_HEADERS;
      }
      break;
    default:
      MOZ_ASSERT(false, "We are done sending this request!");
      break;
  }
  return NS_OK;
}

nsresult Http3Stream::OnWriteSegment(char* buf, uint32_t count,
                                     uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3Stream::OnWriteSegment [this=%p, state=%d", this, mState));
  nsresult rv = NS_OK;
  switch (mState) {
    case PREPARING_HEADERS:
    case SENDING_BODY:
    case EARLY_RESPONSE:
      break;
    case READING_HEADERS: {
      if (mFlatResponseHeaders.IsEmpty()) {
        nsresult rv = mSession->ReadResponseHeaders(
            mStreamId, mFlatResponseHeaders, &mFin);
        if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
          return rv;
        }
        LOG(("Http3Stream::OnWriteSegment [this=%p, read %u bytes of headers",
             this, (uint32_t)mFlatResponseHeaders.Length()));
      }
      *countWritten = (mFlatResponseHeaders.Length() > count)
                          ? count
                          : mFlatResponseHeaders.Length();
      memcpy(buf, mFlatResponseHeaders.Elements(), *countWritten);

      mFlatResponseHeaders.RemoveElementsAt(0, *countWritten);
      if (mFlatResponseHeaders.Length() == 0) {
        mState = mFin ? RECEIVED_FIN : READING_DATA;
      }

      if (*countWritten == 0) {
        rv = NS_BASE_STREAM_WOULD_BLOCK;
      } else {
        mTotalRead += *countWritten;
        mTransaction->OnTransportStatus(
            mSocketTransport, NS_NET_STATUS_RECEIVING_FROM, mTotalRead);
      }
    } break;
    case READING_DATA: {
      rv = mSession->ReadResponseData(mStreamId, buf, count, countWritten,
                                      &mFin);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (*countWritten == 0) {
        if (mFin) {
          mState = DONE;
          rv = NS_BASE_STREAM_CLOSED;
        } else {
          rv = NS_BASE_STREAM_WOULD_BLOCK;
        }
      } else {
        mTotalRead += *countWritten;
        mTransaction->OnTransportStatus(
            mSocketTransport, NS_NET_STATUS_RECEIVING_FROM, mTotalRead);

        if (mFin) {
          mState = RECEIVED_FIN;
        }
      }
    } break;
    case RECEIVED_FIN:
    case RECEIVED_RESET:
      rv = NS_BASE_STREAM_CLOSED;
      mState = DONE;
      break;
    case DONE:
      rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

nsresult Http3Stream::ReadSegments(nsAHttpSegmentReader* reader, uint32_t count,
                                   uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  mRequestBlockedOnRead = false;

  nsresult rv = NS_OK;
  switch (mState) {
    case PREPARING_HEADERS:
    case SENDING_BODY: {
      rv = mTransaction->ReadSegments(this, count, countRead);
      LOG(("Http3Stream::ReadSegments rv=0x%" PRIx32 " [this=%p]",
           static_cast<uint32_t>(rv), this));

      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        mRequestBlockedOnRead = true;
      }
    } break;
    default:
      *countRead = 0;
      rv = NS_OK;
      break;
  }
  LOG(("Http3Stream::ReadSegments rv=0x%" PRIx32 " [this=%p]",
       static_cast<uint32_t>(rv), this));
  return rv;
}

nsresult Http3Stream::WriteSegments(nsAHttpSegmentWriter* writer,
                                    uint32_t count, uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3Stream::WriteSegments [this=%p]", this));
  nsresult rv = mTransaction->WriteSegments(this, count, countWritten);
  LOG(("Http3Stream::WriteSegments rv=0x%" PRIx32 " [this=%p]",
       static_cast<uint32_t>(rv), this));
  return rv;
}

}  // namespace net
}  // namespace mozilla
