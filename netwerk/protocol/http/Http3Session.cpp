/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpLog.h"
#include "Http3Session.h"
#include "Http3Stream.h"
#include "mozilla/net/DNS.h"
#include "nsHttpHandler.h"
#include "mozilla/RefPtr.h"
#include "ASpdySession.h"  // because of SoftStreamError()
#include "nsIOService.h"
#include "nsISSLSocketControl.h"
#include "ScopedNSSTypes.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "QuicSocketControl.h"
#include "SSLServerCertVerification.h"
//#include "cert.h"
#include "sslerr.h"

namespace mozilla {
namespace net {

const uint64_t HTTP3_APP_ERROR_NO_ERROR = 0x100;
const uint64_t HTTP3_APP_ERROR_GENERAL_PROTOCOL_ERROR = 0x101;
const uint64_t HTTP3_APP_ERROR_INTERNAL_ERROR = 0x102;
const uint64_t HTTP3_APP_ERROR_STREAM_CREATION_ERROR = 0x103;
const uint64_t HTTP3_APP_ERROR_CLOSED_CRITICAL_STREAM = 0x104;
const uint64_t HTTP3_APP_ERROR_FRAME_UNEXPECTED = 0x105;
const uint64_t HTTP3_APP_ERROR_FRAME_ERROR = 0x106;
const uint64_t HTTP3_APP_ERROR_EXCESSIVE_LOAD = 0x107;
const uint64_t HTTP3_APP_ERROR_ID_ERROR = 0x108;
const uint64_t HTTP3_APP_ERROR_SETTINGS_ERROR = 0x109;
const uint64_t HTTP3_APP_ERROR_MISSING_SETTINGS = 0x10a;
const uint64_t HTTP3_APP_ERROR_REQUEST_REJECTED = 0x10b;
const uint64_t HTTP3_APP_ERROR_REQUEST_CANCELLED = 0x10c;
const uint64_t HTTP3_APP_ERROR_REQUEST_INCOMPLETE = 0x10d;
const uint64_t HTTP3_APP_ERROR_EARLY_RESPONSE = 0x10e;
const uint64_t HTTP3_APP_ERROR_CONNECT_ERROR = 0x10f;
const uint64_t HTTP3_APP_ERROR_VERSION_FALLBACK = 0x110;

const uint32_t UDP_MAX_PACKET_SIZE = 4096;

NS_IMPL_ADDREF(Http3Session)
NS_IMPL_RELEASE(Http3Session)
NS_INTERFACE_MAP_BEGIN(Http3Session)
  NS_INTERFACE_MAP_ENTRY(nsAHttpConnection)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(Http3Session)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
NS_INTERFACE_MAP_END

Http3Session::Http3Session()
    : mState(INITIALIZING),
      mAuthenticationStarted(false),
      mCleanShutdown(false),
      mGoawayReceived(false),
      mShouldClose(false),
      mIsClosedByNeqo(false),
      mError(NS_OK),
      mBeforeConnectedError(false) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3Session::Http3Session [this=%p]", this));

  mCurrentForegroundTabOuterContentWindowId =
      gHttpHandler->ConnMgr()->CurrentTopLevelOuterContentWindowId();
}

nsresult Http3Session::Init(const nsACString& aOrigin,
                            nsISocketTransport* aSocketTransport,
                            nsHttpConnection* readerWriter) {
  LOG3(("Http3Session::Init %p", this));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(aSocketTransport);
  MOZ_ASSERT(readerWriter);

  mSocketTransport = aSocketTransport;
  mSegmentReaderWriter = readerWriter;

  nsCOMPtr<nsISupports> info;
  Unused << mSocketTransport->GetSecurityInfo(getter_AddRefs(info));
  mSocketControl = do_QueryObject(info);

  // Get the local and remote address neqo needs it.
  NetAddr selfAddr;
  if (NS_FAILED(mSocketTransport->GetSelfAddr(&selfAddr))) {
    LOG3(("Http3Session::Init GetSelfAddr failed [this=%p]", this));
    return NS_ERROR_FAILURE;
  }
  char buf[kIPv6CStrBufSize];
  NetAddrToString(&selfAddr, buf, kIPv6CStrBufSize);

  nsAutoCString selfAddrStr;
  if (selfAddr.raw.family == AF_INET6) {
    selfAddrStr.Append("[");
  }
  // Append terminating ']' and port.
  selfAddrStr.Append(buf, strlen(buf));
  if (selfAddr.raw.family == AF_INET6) {
    selfAddrStr.Append("]:");
    selfAddrStr.AppendInt(ntohs(selfAddr.inet6.port));
  } else {
    selfAddrStr.Append(":");
    selfAddrStr.AppendInt(ntohs(selfAddr.inet.port));
  }

  NetAddr peerAddr;
  if (NS_FAILED(mSocketTransport->GetPeerAddr(&peerAddr))) {
    LOG3(("Http3Session::Init GetPeerAddr failed [this=%p]", this));
    return NS_ERROR_FAILURE;
  }
  NetAddrToString(&peerAddr, buf, kIPv6CStrBufSize);

  nsAutoCString peerAddrStr;
  if (peerAddr.raw.family == AF_INET6) {
    peerAddrStr.Append("[");
  }
  peerAddrStr.Append(buf, strlen(buf));
  // Append terminating ']' and port.
  if (peerAddr.raw.family == AF_INET6) {
    peerAddrStr.Append("]:");
    peerAddrStr.AppendInt(ntohs(peerAddr.inet6.port));
  } else {
    peerAddrStr.Append(':');
    peerAddrStr.AppendInt(ntohs(peerAddr.inet.port));
  }

  LOG3(
      ("Http3Session::Init origin=%s, alpn=%s, selfAddr=%s, peerAddr=%s,"
       " qpack table size=%u, max blocked streams=%u [this=%p]",
       PromiseFlatCString(aOrigin).get(),
       PromiseFlatCString(kHttp3Version).get(), selfAddrStr.get(),
       peerAddrStr.get(), gHttpHandler->DefaultQpackTableSize(),
       gHttpHandler->DefaultHttp3MaxBlockedStreams(), this));

  return NeqoHttp3Conn::Init(aOrigin, kHttp3Version, selfAddrStr, peerAddrStr,
                             gHttpHandler->DefaultQpackTableSize(),
                             gHttpHandler->DefaultHttp3MaxBlockedStreams(),
                             getter_AddRefs(mHttp3Connection));
}

// Shutdown the http3session and close all transactions.
void Http3Session::Shutdown() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  for (auto iter = mStreamTransactionHash.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<Http3Stream> stream = iter.Data();

    if (mBeforeConnectedError) {
      // We have an error before we were connected, just restart transactions.
      // The transaction restart code path will remove AltSvc mapping and the
      // direct path will be used.
      MOZ_ASSERT(NS_FAILED(mError));
      stream->Close(mError);
    } else if (!stream->HasStreamId()) {
      // Connection has not been started yet. We can restart it.
      stream->Transaction()->DoNotRemoveAltSvc();
      stream->Close(NS_ERROR_NET_RESET);
    } else if (stream->RecvdData()) {
      stream->Close(NS_ERROR_NET_PARTIAL_TRANSFER);
    } else {
      stream->Close(NS_ERROR_ABORT);
    }
    RemoveStreamFromQueues(stream);
    if (stream->HasStreamId()) {
      mStreamIdHash.Remove(stream->StreamId());
    }
  }

  mStreamTransactionHash.Clear();
}

Http3Session::~Http3Session() {
  LOG3(("Http3Session::~Http3Session %p", this));

  Shutdown();
}

PRIntervalTime Http3Session::IdleTime() {
  // Seting this value to 0 will never triger PruneDeadConnections for
  // this connection. We want to let neqo-transport perform close on idle
  // connections.
  return 0;
}

nsresult Http3Session::ProcessInput() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mSegmentReaderWriter);

  LOG(("Http3Session::ProcessInput writer=%p [this=%p]",
       mSegmentReaderWriter.get(), this));

  uint8_t packet[UDP_MAX_PACKET_SIZE];
  uint32_t read = 0;
  nsresult rv = mSegmentReaderWriter->OnWriteSegment(
      (char*)packet, UDP_MAX_PACKET_SIZE, &read);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mHttp3Connection->ProcessInput(packet, read);
  mHttp3Connection->ProcessHttp3();
  LOG(("Http3Session::Process status: state=%d [this=%p]", mState, this));
  return NS_OK;
}

nsresult Http3Session::ProcessEvents(uint32_t count, uint32_t* countWritten,
                                     bool* again) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3Session::ProcessEvents [this=%p]", this));
  Http3Event event = mHttp3Connection->GetEvent();

  while (event.tag != Http3Event::Tag::NoEvent) {
    switch (event.tag) {
      case Http3Event::Tag::HeaderReady:
      case Http3Event::Tag::DataReadable: {
        MOZ_ASSERT(mState == CONNECTED);
        uint64_t id;
        if (event.tag == Http3Event::Tag::HeaderReady) {
          LOG(("Http3Session::ProcessEvents - HeaderReady"));
          id = event.header_ready.stream_id;
        } else {
          LOG(("Http3Session::ProcessEvents - DataReadable"));
          id = event.data_readable.stream_id;
        }

        RefPtr<Http3Stream> stream = mStreamIdHash.Get(id);
        if (!stream) {
          // This is an old event. This may happen because we store events in
          // neqo_glue.
          // TODO: maybe change neqo interface to return only one event.
          LOG(
              ("Http3Session::ProcessEvents - stream not found "
               "stream_id=0x%" PRIx64 " [this=%p].",
               id, this));
          event = mHttp3Connection->GetEvent();
          continue;
        }

        nsresult rv = stream->WriteSegments(this, count, countWritten);
        if (ASpdySession::SoftStreamError(rv)) {
          CloseStream(stream, (rv == NS_BINDING_RETARGETED)
                                  ? NS_BINDING_RETARGETED
                                  : NS_OK);
          *again = false;
          rv = ResumeRecv();
          if (NS_FAILED(rv)) {
            LOG3(("ResumeRecv returned code 0x%" PRIx32 ".",
                  static_cast<uint32_t>(rv)));
          }
          return NS_OK;
        }

        if (stream->RecvdFin() && !stream->Done() && NS_SUCCEEDED(rv)) {
          // In RECEIVED_FIN state we need to give the httpTransaction the info
          // that the transaction is closed.
          rv = stream->WriteSegments(this, count, countWritten);
          if (ASpdySession::SoftStreamError(rv)) {
            CloseStream(stream, (rv == NS_BINDING_RETARGETED)
                                    ? NS_BINDING_RETARGETED
                                    : NS_OK);
            *again = false;
            rv = ResumeRecv();
            if (NS_FAILED(rv)) {
              LOG3(("ResumeRecv returned code 0x%" PRIx32 ".",
                    static_cast<uint32_t>(rv)));
            }
            return NS_OK;
          }
        }

        if (stream->Done()) {
          LOG3(("Http3Session::ProcessEvents session=%p stream=%p 0x%" PRIx64
                " cleanup stream.\n",
                this, stream.get(), stream->StreamId()));
          CloseStream(stream, NS_OK);
        }

        if (NS_FAILED(rv)) {
          LOG3(("Http3Session::ProcessEvents failed rv=0x%" PRIx32
                " [this=%p].",
                static_cast<uint32_t>(rv), this));
          // maybe just blocked reading from network
          if (rv == NS_BASE_STREAM_WOULD_BLOCK) rv = NS_OK;
        }
        return rv;
      } break;
      case Http3Event::Tag::Reset:
        LOG(("Http3Session::ProcessEvents - Reset"));
        ResetRecvd(event.reset.stream_id, event.reset.error);
        break;
      case Http3Event::Tag::NewPushStream:
        LOG(("Http3Session::ProcessEvents - NewPushStream"));
        break;
      case Http3Event::Tag::RequestsCreatable:
        LOG(("Http3Session::ProcessEvents - StreamCreatable"));
        ProcessPending();
        break;
      case Http3Event::Tag::AuthenticationNeeded:
        LOG(("Http3Session::ProcessEvents - AuthenticationNeeded %d",
             mAuthenticationStarted));
        if (!mAuthenticationStarted) {
          mAuthenticationStarted = true;
          LOG(("Http3Session::ProcessEvents - AuthenticationNeeded called"));
          CallCertVerification();
        }
        break;
      case Http3Event::Tag::ConnectionConnected:
        LOG(("Http3Session::ProcessEvents - ConnectionConnected"));
        mState = CONNECTED;
        SetSecInfo();
        mSocketControl->HandshakeCompleted();
        break;
      case Http3Event::Tag::GoawayReceived:
        LOG(("Http3Session::ProcessEvents - GoawayReceived"));
        MOZ_ASSERT(!mGoawayReceived);
        mGoawayReceived = true;
        break;
      case Http3Event::Tag::ConnectionClosing:
        LOG(("Http3Session::ProcessEvents - ConnectionClosing"));
        if (NS_SUCCEEDED(mError) && !IsClosing()) {
          mError = NS_ERROR_NET_HTTP3_PROTOCOL_ERROR;
        }
        CloseInternal(false);
        break;
      case Http3Event::Tag::ConnectionClosed:
        LOG(("Http3Session::ProcessEvents - ConnectionClosed"));
        if (NS_SUCCEEDED(mError) && !IsClosing()) {
          mError = NS_ERROR_NET_HTTP3_PROTOCOL_ERROR;
        }
        mIsClosedByNeqo = true;
        // We need to return here and let nsHttpConnection close the session.
        return mError;
        break;
      default:
        break;
    }
    event = mHttp3Connection->GetEvent();
  }

  *again = false;
  Unused << ResumeRecv();
  return NS_OK;
}

// This function is used to drive quic handshake.
nsresult Http3Session::Process() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  nsresult rv = ProcessInput();
  if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
    return rv;
  }

  bool notUsed;
  uint32_t n = 0;
  rv = ProcessEvents(nsIOService::gDefaultSegmentSize, &n, &notUsed);
  if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
    return rv;
  }

  rv = ProcessOutput();
  if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
    return rv;
  }

  n = 0;
  return ProcessEvents(nsIOService::gDefaultSegmentSize, &n, &notUsed);
}

nsresult Http3Session::ProcessOutput() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mSegmentReaderWriter);

  LOG(("Http3Session::ProcessOutput reader=%p, [this=%p]",
       mSegmentReaderWriter.get(), this));

  nsresult rv = NS_OK;
  // Check if we have a packet that could not have been sent in a previous
  // iteration.
  if (mPacketToSend.Length()) {
    uint32_t written = 0;
    rv = mSegmentReaderWriter->OnReadSegment(
        (const char*)mPacketToSend.Elements(), mPacketToSend.Length(),
        &written);
    if (NS_FAILED(rv)) {
      if ((rv == NS_BASE_STREAM_WOULD_BLOCK) && mConnection) {
        // The socket is still blocked, wait again.
        Unused << mConnection->ResumeSend();
      }
      return rv;
    }
    MOZ_ASSERT(written == mPacketToSend.Length());
    mPacketToSend.TruncateLength(0);
  }

  // Process neqo.
  mHttp3Connection->ProcessHttp3();
  uint64_t timeout = mHttp3Connection->ProcessOutput();

  // Maybe get new packets to send.
  nsresult getDataRv = mHttp3Connection->GetDataToSend(mPacketToSend);
  while (NS_SUCCEEDED(getDataRv) && mPacketToSend.Length()) {
    LOG(("Http3Session::ProcessOutput sending packet with %u bytes [this=%p].",
         (uint32_t)mPacketToSend.Length(), this));
    uint32_t written = 0;
    rv = mSegmentReaderWriter->OnReadSegment(
        (const char*)mPacketToSend.Elements(), mPacketToSend.Length(),
        &written);
    if (NS_FAILED(rv)) {
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        // The socket is blocked, keep the packet and we will send it when the
        // socket is ready to send data again.
        if (mConnection) {
          Unused << mConnection->ResumeSend();
        }
      }
      break;
    }
    MOZ_ASSERT(written == mPacketToSend.Length());
    mPacketToSend.TruncateLength(0);
    getDataRv = mHttp3Connection->GetDataToSend(mPacketToSend);
  }

  SetupTimer(timeout);
  return rv;
}

// This is only called when timer expires.
nsresult Http3Session::ProcessOutputAndEvents() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  mHttp3Connection->ProcessTimer();
  nsresult rv = ProcessOutput();
  if (NS_FAILED(rv)) {
    return rv;
  }
  mHttp3Connection->ProcessHttp3();
  bool notUsed;
  uint32_t n = 0;
  rv = ProcessEvents(nsIOService::gDefaultSegmentSize, &n, &notUsed);
  if (NS_FAILED(rv)) {
    // Now we can remove all references and Http3Session will be destroyed.
    if (mTimer) {
      mTimer->Cancel();
    }
    mConnection = nullptr;
    mSocketTransport = nullptr;
    mSegmentReaderWriter = nullptr;
    mState = CLOSED;
  }
  return NS_OK;
}

void Http3Session::SetupTimer(uint64_t aTimeout) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3Session::SetupTimer to %" PRIu64 "ms [this=%p].", aTimeout, this));
  if (!mTimer) {
    mTimer = NS_NewTimer();
  }

  if (!mTimer || NS_FAILED(mTimer->InitWithCallback(this, aTimeout,
                                                    nsITimer::TYPE_ONE_SHOT))) {
    NS_DispatchToCurrentThread(
        NewRunnableMethod("net::Http3Session::ProcessOutputAndEvents", this,
                          &Http3Session::ProcessOutputAndEvents));
  }
}

NS_IMETHODIMP
Http3Session::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(aTimer == mTimer, "wrong timer");
  LOG(("Http3Session::Notify [this=%p].", this));
  Unused << ProcessOutputAndEvents();
  return NS_OK;
}

bool Http3Session::AddStream(nsAHttpTransaction* aHttpTransaction,
                             int32_t aPriority,
                             nsIInterfaceRequestor* aCallbacks) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsHttpTransaction* trans = aHttpTransaction->QueryHttpTransaction();

  if (!mConnection) {
    // Get the connection from the first transaction.
    mConnection = aHttpTransaction->Connection();
  }

  if (IsClosing()) {
    LOG3(
        ("Http3Session::AddStream %p atrans=%p trans=%p session unusable - "
         "resched.\n",
         this, aHttpTransaction, trans));
    aHttpTransaction->SetConnection(nullptr);
    nsresult rv = gHttpHandler->InitiateTransaction(trans, trans->Priority());
    if (NS_FAILED(rv)) {
      LOG3(
          ("Http3Session::AddStream %p atrans=%p trans=%p failed to initiate "
           "transaction (0x%" PRIx32 ").\n",
           this, aHttpTransaction, trans, static_cast<uint32_t>(rv)));
    }
    return true;
  }

  aHttpTransaction->SetConnection(this);
  aHttpTransaction->OnActivated();

  LOG3(("Http3Session::AddStream %p atrans=%p.\n", this, aHttpTransaction));
  Http3Stream* stream = new Http3Stream(aHttpTransaction, this);
  mStreamTransactionHash.Put(aHttpTransaction, stream);

  mReadyForWrite.Push(stream);

  if (mState == INITIALIZING) {
    // Don't call ReadSegments yet, wait untill handshake is done or fails.
    return true;
  }
  // Kick off the SYN transmit without waiting for the poll loop
  uint32_t countRead;
  Unused << ReadSegments(nullptr, kDefaultReadAmount, &countRead);

  return true;
}

bool Http3Session::CanReuse() {
  return (mState == CONNECTED) && !(mGoawayReceived || mShouldClose);
}

void Http3Session::QueueStream(Http3Stream* stream) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!stream->Queued());

  LOG3(("Http3Session::QueueStream %p stream %p queued.", this, stream));

  stream->SetQueued(true);
  mQueuedStreams.Push(stream);
}

void Http3Session::ProcessPending() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  Http3Stream* stream;
  while ((stream = static_cast<Http3Stream*>(mQueuedStreams.PopFront()))) {
    LOG3(("Http3Session::ProcessPending %p stream %p woken from queue.", this,
          stream));
    MOZ_ASSERT(stream->Queued());
    stream->SetQueued(false);
    mReadyForWrite.Push(stream);
    Unused << mConnection->ResumeSend();
  }
}

static void RemoveStreamFromQueue(Http3Stream* aStream, nsDeque& queue) {
  size_t size = queue.GetSize();
  for (size_t count = 0; count < size; ++count) {
    Http3Stream* stream = static_cast<Http3Stream*>(queue.PopFront());
    if (stream != aStream) {
      queue.Push(stream);
    }
  }
}

void Http3Session::RemoveStreamFromQueues(Http3Stream* aStream) {
  RemoveStreamFromQueue(aStream, mReadyForWrite);
  RemoveStreamFromQueue(aStream, mQueuedStreams);
}

// This is called by Http3Stream::OnReadSegment.
// ProcessOutput will be called in Http3Session::ReadSegment that
// calls Http3Stream::OnReadSegment.
nsresult Http3Session::TryActivating(
    const nsACString& aMethod, const nsACString& aScheme,
    const nsACString& aAuthorityHeader, const nsACString& aPath,
    const nsACString& aHeaders, uint64_t* aStreamId, Http3Stream* aStream) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(*aStreamId == UINT64_MAX);

  LOG(("Http3Session::TryActivating [stream=%p, this=%p state=%d]", aStream,
       this, mState));

  if (IsClosing()) {
    if (NS_FAILED(mError)) {
      return mError;
    }
    return NS_ERROR_FAILURE;
  }

  if (aStream->Queued()) {
    LOG3(("Http3Session::TryActivating %p stream=%p already queued.\n", this,
          aStream));
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  nsresult rv = mHttp3Connection->Fetch(aMethod, aScheme, aAuthorityHeader,
                                        aPath, aHeaders, aStreamId);
  if (NS_FAILED(rv)) {
    LOG(("Http3Session::TryActivating returns error=0x%" PRIx32 "[stream=%p, "
         "this=%p]",
         static_cast<uint32_t>(rv), aStream, this));
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      LOG3(
          ("Http3Session::TryActivating %p stream=%p no room for more "
           "concurrent streams\n",
           this, aStream));
      QueueStream(aStream);
    }
    return rv;
  }

  LOG(("Http3Session::TryActivating streamId=0x%" PRIx64
       " for stream=%p [this=%p].",
       *aStreamId, aStream, this));

  MOZ_ASSERT(*aStreamId != UINT64_MAX);
  mStreamIdHash.Put(*aStreamId, aStream);
  mHttp3Connection->ProcessHttp3();
  return NS_OK;
}

// This is only called by Http3Stream::OnReadSegment.
// ProcessOutput will be called in Http3Session::ReadSegment that
// calls Http3Stream::OnReadSegment.
void Http3Session::CloseSendingSide(uint64_t aStreamId) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  mHttp3Connection->CloseStream(aStreamId);
}

// This is only called by Http3Stream::OnReadSegment.
// ProcessOutput will be called in Http3Session::ReadSegment that
// calls Http3Stream::OnReadSegment.
nsresult Http3Session::SendRequestBody(uint64_t aStreamId, const char* buf,
                                       uint32_t count, uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  return mHttp3Connection->SendRequestBody(aStreamId, (const uint8_t*)buf,
                                           count, countRead);
}

void Http3Session::ResetRecvd(uint64_t aStreamId, Http3AppError aError) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  RefPtr<Http3Stream> stream = mStreamIdHash.Get(aStreamId);
  if (!stream) {
    return;
  }

  stream->SetRecvdReset();

  // We only handle some of Http3 error as epecial, the res are just equivalent
  // to cancel.
  if (aError.tag == Http3AppError::Tag::VersionFallback) {
    // We will restart the request and the alt-svc will be removed
    // automatically.
    // Also disable spdy we want http/1.1.
    stream->Transaction()->DisableSpdy();
    CloseStream(stream, NS_ERROR_NET_RESET);
  } else if (aError.tag == Http3AppError::Tag::RequestRejected) {
    // This request was rejected because server is probably busy or going away.
    // We can restart the request using alt-svc. Without calling
    // DoNotRemoveAltSvc the alt-svc route will be removed.
    stream->Transaction()->DoNotRemoveAltSvc();
    CloseStream(stream, NS_ERROR_NET_RESET);
  } else {
    if (stream->RecvdData()) {
      CloseStream(stream, NS_ERROR_NET_PARTIAL_TRANSFER);
    } else {
      CloseStream(stream, NS_ERROR_NET_INTERRUPT);
    }
  }
}

void Http3Session::SetConnection(nsAHttpConnection* aConn) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  mConnection = aConn;
}

void Http3Session::GetSecurityCallbacks(nsIInterfaceRequestor** aOut) {
  *aOut = nullptr;
}

// TODO
void Http3Session::OnTransportStatus(nsITransport* aTransport, nsresult aStatus,
                                     int64_t aProgress) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
}

bool Http3Session::IsDone() { return mState == CLOSED; }

nsresult Http3Session::Status() {
  MOZ_ASSERT(false, "Http3Session::Status()");
  return NS_ERROR_UNEXPECTED;
}

uint32_t Http3Session::Caps() {
  MOZ_ASSERT(false, "Http3Session::Caps()");
  return 0;
}

nsresult Http3Session::ReadSegments(nsAHttpSegmentReader* reader,
                                    uint32_t count, uint32_t* countRead) {
  bool again = false;
  return ReadSegmentsAgain(reader, count, countRead, &again);
}

nsresult Http3Session::ReadSegmentsAgain(nsAHttpSegmentReader* reader,
                                         uint32_t count, uint32_t* countRead,
                                         bool* again) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  LOG(("Http3Session::ReadSegmentsAgain [this=%p]", this));
  *again = false;

  *countRead = 0;

  Http3Stream* stream = static_cast<Http3Stream*>(mReadyForWrite.PopFront());
  if (!stream) {
    LOG(
        ("Http3Session::ReadSegmentsAgain we do not have a stream ready to "
         "write."));
    ProcessOutput();
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  LOG(
      ("Http3Session::ReadSegmentsAgain call ReadSegments from stream=%p "
       "[this=%p]",
       stream, this));
  nsresult rv = stream->ReadSegments(this, count, countRead);

  if (stream->RequestBlockedOnRead()) {
    // We are blocked waiting for input - either more http headers or
    // any request body data. When more data from the request stream
    // becomes available the httptransaction will call conn->ResumeSend().

    LOG3(("Http3Session::ReadSegments %p dealing with block on read", this));

    // call readsegments again if there are other streams ready
    // to run in this session
    if (mReadyForWrite.GetSize() > 0) {
      rv = NS_OK;
    } else {
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    }

  } else if (NS_FAILED(rv)) {
    LOG3(("Http3Session::ReadSegmentsAgain %p returns error code 0x%" PRIx32,
          this, static_cast<uint32_t>(rv)));
    if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
      CloseStream(stream, rv);
      if (ASpdySession::SoftStreamError(rv)) {
        LOG3(("Http3Session::ReadSegments %p soft error override\n", this));
        *again = false;
        rv = NS_OK;
      }
    }
  } else if (*countRead > 0) {
    mReadyForWrite.Push(stream);
  }

  // Call neqo-transaction.
  ProcessOutput();

  Unused << mConnection->ResumeRecv();
  // TODO block on max_stream_data

  if (mReadyForWrite.GetSize() > 0) {
    Unused << mConnection->ResumeSend();
  }
  return rv;
}

nsresult Http3Session::WriteSegments(nsAHttpSegmentWriter* writer,
                                     uint32_t count, uint32_t* countWritten) {
  bool again = false;
  return WriteSegmentsAgain(writer, count, countWritten, &again);
}

nsresult Http3Session::WriteSegmentsAgain(nsAHttpSegmentWriter* writer,
                                          uint32_t count,
                                          uint32_t* countWritten, bool* again) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (mState == CLOSED) return NS_ERROR_FAILURE;

  nsresult rv = ProcessInput();
  if (NS_FAILED(rv)) {
    LOG3(("Http3Session %p processInput returns 0x%" PRIx32 "\n", this,
          static_cast<uint32_t>(rv)));
    // maybe just blocked reading from network
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      rv = NS_OK;
    }
    return rv;
  }
  rv = ProcessEvents(count, countWritten, again);
  if (NS_SUCCEEDED(rv)) {
    Unused << mConnection->ResumeRecv();
  }

  uint64_t timeout = mHttp3Connection->ProcessOutput();

  // Check if we have datagrams to send. If we have let's poll for writing.
  if (mConnection && mHttp3Connection->HasDataToSend()) {
    Unused << mConnection->ResumeSend();
  }
  SetupTimer(timeout);
  return rv;
}

void Http3Session::Close(nsresult aReason) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  mError = aReason;
  CloseInternal(true);

  if (mCleanShutdown || mIsClosedByNeqo) {
    // It is network-tear-down or neqo is state CLOSED(it does not need to send
    // any more packets or wait for new packets).
    // We need to remove all references, so that
    // Http3Session will be destroyed.
    if (mTimer) {
      mTimer->Cancel();
    }
    mConnection = nullptr;
    mSocketTransport = nullptr;
    mSegmentReaderWriter = nullptr;
    mState = CLOSED;
  }
}

void Http3Session::CloseInternal(bool aCallNeqoClose) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (IsClosing()) {
    return;
  }

  LOG(("Http3Session::Closing [this=%p]", this));

  if (mState != CONNECTED) {
    mBeforeConnectedError = true;
  }
  mState = CLOSING;
  Shutdown();

  if (aCallNeqoClose) {
    mHttp3Connection->Close(HTTP3_APP_ERROR_NO_ERROR);
  }

  mStreamIdHash.Clear();
  mStreamTransactionHash.Clear();
}

nsHttpConnectionInfo* Http3Session::ConnectionInfo() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  RefPtr<nsHttpConnectionInfo> ci;
  GetConnectionInfo(getter_AddRefs(ci));
  return ci.get();
}

void Http3Session::SetProxyConnectFailed() {
  MOZ_ASSERT(false, "Http3Session::SetProxyConnectFailed()");
}

nsHttpRequestHead* Http3Session::RequestHead() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(false,
             "Http3Session::RequestHead() "
             "should not be called after http/3 is setup");
  return nullptr;
}

uint32_t Http3Session::Http1xTransactionCount() { return 0; }

nsresult Http3Session::TakeSubTransactions(
    nsTArray<RefPtr<nsAHttpTransaction>>& outTransactions) {
  return NS_OK;
}

PRIntervalTime Http3Session::ResponseTimeout() {
  return gHttpHandler->ResponseTimeout();
}

//-----------------------------------------------------------------------------
// Pass through methods of nsAHttpConnection
//-----------------------------------------------------------------------------

nsAHttpConnection* Http3Session::Connection() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  return mConnection;
}

nsresult Http3Session::OnHeadersAvailable(nsAHttpTransaction* transaction,
                                          nsHttpRequestHead* requestHead,
                                          nsHttpResponseHead* responseHead,
                                          bool* reset) {
  return mConnection->OnHeadersAvailable(transaction, requestHead, responseHead,
                                         reset);
}

bool Http3Session::IsReused() { return mConnection->IsReused(); }

nsresult Http3Session::PushBack(const char* buf, uint32_t len) {
  return mConnection->PushBack(buf, len);
}

already_AddRefed<nsHttpConnection> Http3Session::TakeHttpConnection() {
  MOZ_ASSERT(false, "TakeHttpConnection of Http3Session");
  return nullptr;
}

already_AddRefed<nsHttpConnection> Http3Session::HttpConnection() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (mConnection) {
    return mConnection->HttpConnection();
  }
  return nullptr;
}

void Http3Session::CloseTransaction(nsAHttpTransaction* aTransaction,
                                    nsresult aResult) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http3Session::CloseTransaction %p %p 0x%" PRIx32, this, aTransaction,
        static_cast<uint32_t>(aResult)));

  // Generally this arrives as a cancel event from the connection manager.

  // need to find the stream and call CloseStream() on it.
  RefPtr<Http3Stream> stream = mStreamTransactionHash.Get(aTransaction);
  if (!stream) {
    LOG3(("Http3Session::CloseTransaction %p %p 0x%" PRIx32 " - not found.",
          this, aTransaction, static_cast<uint32_t>(aResult)));
    return;
  }
  LOG3(
      ("Http3Session::CloseTransaction probably a cancel. this=%p, "
       "trans=%p, result=0x%" PRIx32 ", streamId=0x%" PRIx64 " stream=%p",
       this, aTransaction, static_cast<uint32_t>(aResult), stream->StreamId(),
       stream.get()));
  CloseStream(stream, aResult);
  Unused << mConnection->ResumeSend();
}

void Http3Session::CloseStream(Http3Stream* aStream, nsresult aResult) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (!aStream->RecvdFin() && !aStream->RecvdReset() &&
      (aStream->HasStreamId())) {
    mHttp3Connection->ResetStream(aStream->StreamId(),
                                  HTTP3_APP_ERROR_REQUEST_CANCELLED);
  }
  aStream->Close(aResult);
  if (aStream->HasStreamId()) {
    mStreamIdHash.Remove(aStream->StreamId());
  }
  RemoveStreamFromQueues(aStream);
  mStreamTransactionHash.Remove(aStream->Transaction());
  if ((mShouldClose || mGoawayReceived) && !mStreamTransactionHash.Count()) {
    MOZ_ASSERT(!IsClosing());
    Close(NS_OK);
  }
}

nsresult Http3Session::TakeTransport(nsISocketTransport**,
                                     nsIAsyncInputStream**,
                                     nsIAsyncOutputStream**) {
  MOZ_ASSERT(false, "TakeTransport of Http3Session");
  return NS_ERROR_UNEXPECTED;
}

bool Http3Session::IsPersistent() { return true; }

void Http3Session::DontReuse() {
  LOG3(("Http3Session::DontReuse %p\n", this));
  if (!OnSocketThread()) {
    LOG3(("Http3Session %p not on socket thread\n", this));
    nsCOMPtr<nsIRunnable> event = NewRunnableMethod(
        "Http3Session::DontReuse", this, &Http3Session::DontReuse);
    gSocketTransportService->Dispatch(event, NS_DISPATCH_NORMAL);
    return;
  }

  if (mGoawayReceived || IsClosing()) {
    return;
  }

  mShouldClose = true;
  if (!mStreamTransactionHash.Count()) {
    Close(NS_OK);
  }
}

void Http3Session::TopLevelOuterContentWindowIdChanged(uint64_t windowId) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  mCurrentForegroundTabOuterContentWindowId = windowId;

  for (auto iter = mStreamTransactionHash.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->TopLevelOuterContentWindowIdChanged(windowId);
  }
}

nsresult Http3Session::OnReadSegment(const char* buf, uint32_t count,
                                     uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http3Session::OnReadSegment"));
  *countRead = 0;
  return NS_OK;
}

nsresult Http3Session::OnWriteSegment(char* buf, uint32_t count,
                                      uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http3Session::OnWriteSegment"));
  *countWritten = 0;
  return NS_OK;
}

// This is called by Http3Stream::OnWriteSegment.
nsresult Http3Session::ReadResponseHeaders(uint64_t aStreamId,
                                           nsTArray<uint8_t>& aResponseHeaders,
                                           bool* aFin) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  return mHttp3Connection->ReadResponseHeaders(aStreamId, aResponseHeaders,
                                               aFin);
}

// This is called by Http3Stream::OnWriteSegment.
nsresult Http3Session::ReadResponseData(uint64_t aStreamId, char* aBuf,
                                        uint32_t aCount,
                                        uint32_t* aCountWritten, bool* aFin) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  return mHttp3Connection->ReadResponseData(aStreamId, (uint8_t*)aBuf, aCount,
                                            aCountWritten, aFin);
}

void Http3Session::TransactionHasDataToWrite(nsAHttpTransaction* caller) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG3(("Http3Session::TransactionHasDataToWrite %p trans=%p", this, caller));

  // a trapped signal from the http transaction to the connection that
  // it is no longer blocked on read.

  RefPtr<Http3Stream> stream = mStreamTransactionHash.Get(caller);
  if (!stream) {
    LOG3(("Http3Session::TransactionHasDataToWrite %p caller %p not found",
          this, caller));
    return;
  }

  LOG3(("Http3Session::TransactionHasDataToWrite %p ID is 0x%" PRIx64, this,
        stream->StreamId()));

  if (!IsClosing()) {
    mReadyForWrite.Push(stream);
    Unused << mConnection->ResumeSend();
  } else {
    LOG3(
        ("Http3Session::TransactionHasDataToWrite %p closed so not setting "
         "Ready4Write\n",
         this));
  }

  // NSPR poll will not poll the network if there are non system PR_FileDesc's
  // that are ready - so we can get into a deadlock waiting for the system IO
  // to come back here if we don't force the send loop manually.
  Unused << ForceSend();
}

bool Http3Session::TestJoinConnection(const nsACString& hostname,
                                      int32_t port) {
  return RealJoinConnection(hostname, port, true);
}

bool Http3Session::JoinConnection(const nsACString& hostname, int32_t port) {
  return RealJoinConnection(hostname, port, false);
}

// TODO test
bool Http3Session::RealJoinConnection(const nsACString& hostname, int32_t port,
                                      bool justKidding) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (!mConnection || (mState != CONNECTED) || mShouldClose ||
      mGoawayReceived) {
    return false;
  }

  nsHttpConnectionInfo* ci = ConnectionInfo();
  if (nsCString(hostname).EqualsIgnoreCase(ci->Origin()) &&
      (port == ci->OriginPort())) {
    return true;
  }

  nsAutoCString key(hostname);
  key.Append(':');
  key.Append(justKidding ? 'k' : '.');
  key.AppendInt(port);
  bool cachedResult;
  if (mJoinConnectionCache.Get(key, &cachedResult)) {
    LOG(("joinconnection [%p %s] %s result=%d cache\n", this,
         ConnectionInfo()->HashKey().get(), key.get(), cachedResult));
    return cachedResult;
  }

  nsresult rv;
  bool isJoined = false;

  nsCOMPtr<nsISupports> securityInfo;
  nsCOMPtr<nsISSLSocketControl> sslSocketControl;

  mConnection->GetSecurityInfo(getter_AddRefs(securityInfo));
  sslSocketControl = do_QueryInterface(securityInfo, &rv);
  if (NS_FAILED(rv) || !sslSocketControl) {
    return false;
  }

  bool joinedReturn = false;
  if (justKidding) {
    rv = sslSocketControl->TestJoinConnection(kHttp3Version, hostname, port,
                                              &isJoined);
  } else {
    rv = sslSocketControl->JoinConnection(kHttp3Version, hostname, port,
                                          &isJoined);
  }
  if (NS_SUCCEEDED(rv) && isJoined) {
    joinedReturn = true;
  }

  LOG(("joinconnection [%p %s] %s result=%d lookup\n", this,
       ConnectionInfo()->HashKey().get(), key.get(), joinedReturn));
  mJoinConnectionCache.Put(key, joinedReturn);
  if (!justKidding) {
    // cache a kidding entry too as this one is good for both
    nsAutoCString key2(hostname);
    key2.Append(':');
    key2.Append('k');
    key2.AppendInt(port);
    if (!mJoinConnectionCache.Get(key2)) {
      mJoinConnectionCache.Put(key2, joinedReturn);
    }
  }
  return joinedReturn;
}

void Http3Session::CallCertVerification() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http3Session::CallCertVerification [this=%p]", this));

  NeqoCertificateInfo certInfo;
  if (NS_FAILED(mHttp3Connection->PeerCertificateInfo(&certInfo))) {
    LOG(("Http3Session::CallCertVerification [this=%p] - no cert", this));
    mHttp3Connection->PeerAuthenticated(SSL_ERROR_BAD_CERTIFICATE);
    mError = psm::GetXPCOMFromNSSError(SSL_ERROR_BAD_CERTIFICATE);
    return;
  }

  Maybe<nsTArray<nsTArray<uint8_t>>> stapledOCSPResponse;
  if (certInfo.stapled_ocsp_responses_present) {
    stapledOCSPResponse.emplace(certInfo.stapled_ocsp_responses);
  }

  Maybe<nsTArray<uint8_t>> sctsFromTLSExtension;
  if (certInfo.signed_cert_timestamp_present) {
    sctsFromTLSExtension.emplace(certInfo.signed_cert_timestamp);
  }

  mSocketControl->SetAuthenticationCallback(this);
  uint32_t providerFlags;
  // the return value is always NS_OK, just ignore it.
  Unused << mSocketControl->GetProviderFlags(&providerFlags);

  SECStatus rv = AuthCertificateHookWithInfo(
      mSocketControl, static_cast<const void*>(this), certInfo.certs,
      stapledOCSPResponse, sctsFromTLSExtension, providerFlags);
  if ((rv != SECSuccess) && (rv != SECWouldBlock)) {
    LOG(("Http3Session::CallCertVerification [this=%p] AuthCertificate failed",
         this));
    mHttp3Connection->PeerAuthenticated(SSL_ERROR_BAD_CERTIFICATE);
    mError = psm::GetXPCOMFromNSSError(SSL_ERROR_BAD_CERTIFICATE);
  }
}

void Http3Session::Authenticated(int32_t aError) {
  LOG(("Http3Session::Authenticated error=0x%" PRIx32 " [this=%p].", aError,
       this));
  if (mState == INITIALIZING) {
    if (psm::IsNSSErrorCode(aError)) {
      mError = psm::GetXPCOMFromNSSError(aError);
      LOG(("Http3Session::Authenticated psm-error=0x%" PRIx32 " [this=%p].",
           static_cast<uint32_t>(mError), this));
    }
    mHttp3Connection->PeerAuthenticated(aError);
  }

  if (mConnection) {
    Unused << mConnection->ResumeSend();
  }
}

void Http3Session::SetSecInfo() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  NeqoSecretInfo secInfo;
  if (NS_SUCCEEDED(mHttp3Connection->GetSecInfo(&secInfo))) {
    mSocketControl->SetSSLVersionUsed(secInfo.version);
    mSocketControl->SetResumed(secInfo.resumed);

    mSocketControl->SetInfo(secInfo.cipher, secInfo.version, secInfo.group,
                            secInfo.signature_scheme);
  }
}

}  // namespace net
}  // namespace mozilla
