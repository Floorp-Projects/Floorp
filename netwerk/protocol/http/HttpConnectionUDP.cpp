/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#define TLS_EARLY_DATA_NOT_AVAILABLE 0
#define TLS_EARLY_DATA_AVAILABLE_BUT_NOT_USED 1
#define TLS_EARLY_DATA_AVAILABLE_AND_USED 2

#define ESNI_SUCCESSFUL 0
#define ESNI_FAILED 1
#define NO_ESNI_SUCCESSFUL 2
#define NO_ESNI_FAILED 3

#include "ASpdySession.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/Telemetry.h"
#include "HttpConnectionUDP.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsIClassOfService.h"
#include "nsIOService.h"
#include "nsISocketTransport.h"
#include "nsSocketTransportService2.h"
#include "nsISSLSocketControl.h"
#include "nsISupportsPriority.h"
#include "nsPreloadedStream.h"
#include "nsProxyRelease.h"
#include "nsSocketTransport2.h"
#include "nsStringStream.h"
#include "mozpkix/pkixnss.h"
#include "sslt.h"
#include "NSSErrorsService.h"
#include "TunnelUtils.h"
#include "TCPFastOpenLayer.h"
#include "Http3Session.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// HttpConnectionUDP <public>
//-----------------------------------------------------------------------------

HttpConnectionUDP::HttpConnectionUDP()
    : mHttpHandler(gHttpHandler),
      mLastReadTime(0),
      mLastWriteTime(0),
      mTotalBytesRead(0),
      mContentBytesWritten(0),
      mConnectedTransport(false),
      mDontReuse(false),
      mIsReused(false),
      mLastTransactionExpectedNoContent(false),
      mPriority(nsISupportsPriority::PRIORITY_NORMAL),
      mForceSendPending(false),
      mLastRequestBytesSentTime(0) {
  LOG(("Creating HttpConnectionUDP @%p\n", this));

  mThroughCaptivePortal = gHttpHandler->GetThroughCaptivePortal();
}

HttpConnectionUDP::~HttpConnectionUDP() {
  LOG(("Destroying HttpConnectionUDP @%p\n", this));

  if (mThroughCaptivePortal) {
    if (mTotalBytesRead || mTotalBytesWritten) {
      auto total =
          Clamp<uint32_t>((mTotalBytesRead >> 10) + (mTotalBytesWritten >> 10),
                          0, std::numeric_limits<uint32_t>::max());
      Telemetry::ScalarAdd(
          Telemetry::ScalarID::NETWORKING_DATA_TRANSFERRED_CAPTIVE_PORTAL,
          total);
    }

    Telemetry::ScalarAdd(
        Telemetry::ScalarID::NETWORKING_HTTP_CONNECTIONS_CAPTIVE_PORTAL, 1);
  }

  if (mForceSendTimer) {
    mForceSendTimer->Cancel();
    mForceSendTimer = nullptr;
  }
}

nsresult HttpConnectionUDP::Init(
    nsHttpConnectionInfo* info, uint16_t maxHangTime,
    nsISocketTransport* transport, nsIAsyncInputStream* instream,
    nsIAsyncOutputStream* outstream, bool connectedTransport,
    nsIInterfaceRequestor* callbacks, PRIntervalTime rtt) {
  LOG1(("HttpConnectionUDP::Init this=%p sockettransport=%p", this, transport));
  NS_ENSURE_ARG_POINTER(info);
  NS_ENSURE_TRUE(!mConnInfo, NS_ERROR_ALREADY_INITIALIZED);

  mConnectedTransport = connectedTransport;
  mConnInfo = info;
  MOZ_ASSERT(mConnInfo);

  mLastWriteTime = mLastReadTime = PR_IntervalNow();
  mRtt = rtt;

  mSocketTransport = transport;
  mSocketIn = instream;
  mSocketOut = outstream;

  MOZ_ASSERT(mConnInfo->IsHttp3());
  mHttp3Session = new Http3Session();
  nsresult rv =
      mHttp3Session->Init(mConnInfo->GetOrigin(), mSocketTransport, this);
  if (NS_FAILED(rv)) {
    LOG(
        ("HttpConnectionUDP::Init mHttp3Session->Init failed "
         "[this=%p rv=%x]\n",
         this, static_cast<uint32_t>(rv)));
    return rv;
  }

  // See explanation for non-strictness of this operation in
  // SetSecurityCallbacks.
  mCallbacks = new nsMainThreadPtrHolder<nsIInterfaceRequestor>(
      "HttpConnectionUDP::mCallbacks", callbacks, false);

  mSocketTransport->SetEventSink(this, nullptr);
  mSocketTransport->SetSecurityCallbacks(this);

  return NS_OK;
}

// called on the socket thread
nsresult HttpConnectionUDP::Activate(nsAHttpTransaction* trans, uint32_t caps,
                                     int32_t pri) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG1(("HttpConnectionUDP::Activate [this=%p trans=%p caps=%x]\n", this, trans,
        caps));

  if (!mExperienced && !trans->IsNullTransaction()) {
    // For QUIC and TFO we have HttpConnecitonUDP before the actual connection
    // has been establish so wait fo TFO and TLS handshake to be finished before
    // we mark the connection 'experienced'.
    if (!mExperienced && mHttp3Session->IsConnected()) {
      mExperienced = true;
    }
    if (mBootstrappedTimingsSet) {
      mBootstrappedTimingsSet = false;
      nsHttpTransaction* hTrans = trans->QueryHttpTransaction();
      if (hTrans) {
        hTrans->BootstrapTimings(mBootstrappedTimings);
      }
    }
    mBootstrappedTimings = TimingStruct();
  }

  mTransactionCaps = caps;
  mPriority = pri;

  NS_ENSURE_ARG_POINTER(trans);

  // reset the read timers to wash away any idle time
  mLastWriteTime = mLastReadTime = PR_IntervalNow();

  // Connection failures are Activated() just like regular transacions.
  // If we don't have a confirmation of a connected socket then test it
  // with a write() to get relevant error code.
  if (!mConnectedTransport) {
    uint32_t count;
    nsresult rv = NS_OK;
    if (mSocketOut) {
      rv = mSocketOut->Write("", 0, &count);
    }
    if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
      LOG(("HttpConnectionUDP::Activate [this=%p] Bad Socket %" PRIx32 "\n",
           this, static_cast<uint32_t>(rv)));
      mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
      CloseTransaction(mHttp3Session, rv);
      trans->Close(rv);
      return rv;
    }
  }

  if (!mHttp3Session->AddStream(trans, pri, mCallbacks)) {
    MOZ_ASSERT(false);  // this cannot happen!
    trans->Close(NS_ERROR_ABORT);
    return NS_ERROR_FAILURE;
  }

  Unused << ResumeSend();
  return NS_OK;
}

void HttpConnectionUDP::Close(nsresult reason, bool aIsShutdown) {
  LOG(("HttpConnectionUDP::Close [this=%p reason=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(reason)));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mForceSendTimer) {
    mForceSendTimer->Cancel();
    mForceSendTimer = nullptr;
  }

  if (!mTrafficCategory.IsEmpty()) {
    HttpTrafficAnalyzer* hta = gHttpHandler->GetHttpTrafficAnalyzer();
    if (hta) {
      hta->IncrementHttpConnection(std::move(mTrafficCategory));
      MOZ_ASSERT(mTrafficCategory.IsEmpty());
    }
  }

  if (mSocketTransport) {
    mSocketTransport->SetEventSink(nullptr, nullptr);
    mSocketTransport->SetSecurityCallbacks(nullptr);
    mSocketTransport->Close(reason);
    if (mSocketOut) {
      mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
    }

    if (mSocketIn) {
      mSocketIn->AsyncWait(nullptr, 0, 0, nullptr);
    }
  }
}

void HttpConnectionUDP::DontReuse() {
  LOG(("HttpConnectionUDP::DontReuse %p http3session=%p\n", this,
       mHttp3Session.get()));
  mDontReuse = true;
  if (mHttp3Session) {
    mHttp3Session->DontReuse();
  }
}

bool HttpConnectionUDP::TestJoinConnection(const nsACString& hostname,
                                           int32_t port) {
  if (mHttp3Session && CanDirectlyActivate()) {
    return mHttp3Session->TestJoinConnection(hostname, port);
  }

  return false;
}

bool HttpConnectionUDP::JoinConnection(const nsACString& hostname,
                                       int32_t port) {
  if (mHttp3Session && CanDirectlyActivate()) {
    return mHttp3Session->JoinConnection(hostname, port);
  }

  return false;
}

bool HttpConnectionUDP::CanReuse() {
  if (!mSocketTransport || !mConnectedTransport) return false;
  if (mDontReuse) {
    return false;
  }

  if (mHttp3Session) {
    return mHttp3Session->CanReuse();
  }
  return false;
}

bool HttpConnectionUDP::CanDirectlyActivate() {
  // return true if a new transaction can be addded to ths connection at any
  // time through Activate(). In practice this means this is a healthy SPDY
  // connection with room for more concurrent streams.

  if (mHttp3Session) {
    return CanReuse();
  }
  return false;
}

//----------------------------------------------------------------------------
// HttpConnectionUDP::nsAHttpConnection compatible methods
//----------------------------------------------------------------------------

nsresult HttpConnectionUDP::OnHeadersAvailable(nsAHttpTransaction* trans,
                                               nsHttpRequestHead* requestHead,
                                               nsHttpResponseHead* responseHead,
                                               bool* reset) {
  LOG(
      ("HttpConnectionUDP::OnHeadersAvailable [this=%p trans=%p "
       "response-head=%p]\n",
       this, trans, responseHead));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  NS_ENSURE_ARG_POINTER(trans);
  MOZ_ASSERT(responseHead, "No response head?");

  // deal with 408 Server Timeouts
  uint16_t responseStatus = responseHead->Status();
  static const PRIntervalTime k1000ms = PR_MillisecondsToInterval(1000);
  if (responseStatus == 408) {
    // If this error could be due to a persistent connection reuse then
    // we pass an error code of NS_ERROR_NET_RESET to
    // trigger the transaction 'restart' mechanism.  We tell it to reset its
    // response headers so that it will be ready to receive the new response.
    if (mIsReused && ((PR_IntervalNow() - mLastWriteTime) < k1000ms)) {
      Close(NS_ERROR_NET_RESET);
      *reset = true;
      return NS_OK;
    }
  }

  return NS_OK;
}

bool HttpConnectionUDP::IsReused() { return mIsReused; }

nsresult HttpConnectionUDP::TakeTransport(
    nsISocketTransport** aTransport, nsIAsyncInputStream** aInputStream,
    nsIAsyncOutputStream** aOutputStream) {
  return NS_ERROR_FAILURE;
}

void HttpConnectionUDP::GetSecurityInfo(nsISupports** secinfo) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("HttpConnectionUDP::GetSecurityInfo http3Session=%p socket=%p\n",
       mHttp3Session.get(), mSocketTransport.get()));

  if (mHttp3Session &&
      NS_SUCCEEDED(mHttp3Session->GetTransactionSecurityInfo(secinfo))) {
    return;
  }

  if (mSocketTransport &&
      NS_SUCCEEDED(mSocketTransport->GetSecurityInfo(secinfo))) {
    return;
  }

  *secinfo = nullptr;
}

nsresult HttpConnectionUDP::PushBack(const char* data, uint32_t length) {
  LOG(("HttpConnectionUDP::PushBack [this=%p, length=%d]\n", this, length));

  return NS_ERROR_UNEXPECTED;
}

class HttpConnectionUDPForceIO : public Runnable {
 public:
  HttpConnectionUDPForceIO(HttpConnectionUDP* aConn, bool doRecv)
      : Runnable("net::HttpConnectionUDPForceIO"),
        mConn(aConn),
        mDoRecv(doRecv) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (mDoRecv) {
      if (!mConn->mSocketIn) return NS_OK;
      return mConn->OnInputStreamReady(mConn->mSocketIn);
    }

    MOZ_ASSERT(mConn->mForceSendPending);
    mConn->mForceSendPending = false;

    if (!mConn->mSocketOut) {
      return NS_OK;
    }
    return mConn->OnOutputStreamReady(mConn->mSocketOut);
  }

 private:
  RefPtr<HttpConnectionUDP> mConn;
  bool mDoRecv;
};

nsresult HttpConnectionUDP::ResumeSend() {
  LOG(("HttpConnectionUDP::ResumeSend [this=%p]\n", this));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mSocketOut) {
    nsresult rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);
    LOG(("HttpConnectionUDP::ResumeSend [this=%p]\n", this));
    return rv;
  }

  MOZ_ASSERT_UNREACHABLE("no socket output stream");
  return NS_ERROR_UNEXPECTED;
}

nsresult HttpConnectionUDP::ResumeRecv() {
  LOG(("HttpConnectionUDP::ResumeRecv [this=%p]\n", this));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // the mLastReadTime timestamp is used for finding slowish readers
  // and can be pretty sensitive. For that reason we actually reset it
  // when we ask to read (resume recv()) so that when we get called back
  // with actual read data in OnSocketReadable() we are only measuring
  // the latency between those two acts and not all the processing that
  // may get done before the ResumeRecv() call
  mLastReadTime = PR_IntervalNow();

  if (mSocketIn) {
    return mSocketIn->AsyncWait(this, 0, 0, nullptr);
  }

  MOZ_ASSERT_UNREACHABLE("no socket input stream");
  return NS_ERROR_UNEXPECTED;
}

void HttpConnectionUDP::ForceSendIO(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  HttpConnectionUDP* self = static_cast<HttpConnectionUDP*>(aClosure);
  MOZ_ASSERT(aTimer == self->mForceSendTimer);
  self->mForceSendTimer = nullptr;
  NS_DispatchToCurrentThread(new HttpConnectionUDPForceIO(self, false));
}

nsresult HttpConnectionUDP::MaybeForceSendIO() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  // due to bug 1213084 sometimes real I/O events do not get serviced when
  // NSPR derived I/O events are ready and this can cause a deadlock with
  // https over https proxying. Normally we would expect the write callback to
  // be invoked before this timer goes off, but set it at the old windows
  // tick interval (kForceDelay) as a backup for those circumstances.
  static const uint32_t kForceDelay = 17;  // ms

  if (mForceSendPending) {
    return NS_OK;
  }
  MOZ_ASSERT(!mForceSendTimer);
  mForceSendPending = true;
  return NS_NewTimerWithFuncCallback(
      getter_AddRefs(mForceSendTimer), HttpConnectionUDP::ForceSendIO, this,
      kForceDelay, nsITimer::TYPE_ONE_SHOT,
      "net::HttpConnectionUDP::MaybeForceSendIO");
}

// trigger an asynchronous read
nsresult HttpConnectionUDP::ForceRecv() {
  LOG(("HttpConnectionUDP::ForceRecv [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  return NS_DispatchToCurrentThread(new HttpConnectionUDPForceIO(this, true));
}

// trigger an asynchronous write
nsresult HttpConnectionUDP::ForceSend() {
  LOG(("HttpConnectionUDP::ForceSend [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  return MaybeForceSendIO();
}

HttpVersion HttpConnectionUDP::Version() { return HttpVersion::v3_0; }

//-----------------------------------------------------------------------------
// HttpConnectionUDP <private>
//-----------------------------------------------------------------------------

void HttpConnectionUDP::CloseTransaction(nsAHttpTransaction* trans,
                                         nsresult reason, bool aIsShutdown) {
  LOG(("HttpConnectionUDP::CloseTransaction[this=%p trans=%p reason=%" PRIx32
       "]\n",
       this, trans, static_cast<uint32_t>(reason)));

  MOZ_ASSERT(trans == mHttp3Session);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (NS_SUCCEEDED(reason) || (reason == NS_BASE_STREAM_CLOSED)) {
    MOZ_ASSERT(false);
    return;
  }

  // The connection and security errors clear out alt-svc mappings
  // in case any previously validated ones are now invalid
  if (((reason == NS_ERROR_NET_RESET) ||
       (NS_ERROR_GET_MODULE(reason) == NS_ERROR_MODULE_SECURITY)) &&
      mConnInfo && !(mTransactionCaps & NS_HTTP_ERROR_SOFTLY)) {
    gHttpHandler->ClearHostMapping(mConnInfo);
  }

  mDontReuse = true;
  if (mHttp3Session) {
    mHttp3Session->SetCleanShutdown(aIsShutdown);
    mHttp3Session->Close(reason);
    if (!mHttp3Session->IsClosed()) {
      // During closing phase we still keep mHttp3Session session,
      // to resend CLOSE_CONNECTION frames.
      return;
    }
  }

  mHttp3Session = nullptr;

  {
    MutexAutoLock lock(mCallbacksLock);
    mCallbacks = nullptr;
  }

  Close(reason, aIsShutdown);

  // flag the connection as reused here for convenience sake. certainly
  // it might be going away instead ;-)
  mIsReused = true;
}

nsresult HttpConnectionUDP::OnReadSegment(const char* buf, uint32_t count,
                                          uint32_t* countRead) {
  LOG(("HttpConnectionUDP::OnReadSegment [this=%p]\n", this));
  if (count == 0) {
    // some ReadSegments implementations will erroneously call the writer
    // to consume 0 bytes worth of data.  we must protect against this case
    // or else we'd end up closing the socket prematurely.
    NS_ERROR("bad ReadSegments implementation");
    return NS_ERROR_FAILURE;  // stop iterating
  }

  nsresult rv = mSocketOut->Write(buf, count, countRead);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (*countRead == 0) {
    return NS_BASE_STREAM_CLOSED;
  }

  mLastWriteTime = PR_IntervalNow();
  mTotalBytesWritten += *countRead;

  return NS_OK;
}

nsresult HttpConnectionUDP::OnSocketWritable() {
  LOG(("HttpConnectionUDP::OnSocketWritable [this=%p] host=%s\n", this,
       mConnInfo->Origin()));

  if (!mHttp3Session) {
    LOG(("  No session In OnSocketWritable\n"));
    return NS_ERROR_FAILURE;
  }

  uint32_t transactionBytes = 0;
  bool again = true;
  LOG(("  writing transaction request stream\n"));
  nsresult rv = mHttp3Session->ReadSegmentsAgain(
      this, nsIOService::gDefaultSegmentSize, &transactionBytes, &again);
  mContentBytesWritten += transactionBytes;
  return rv;
}

nsresult HttpConnectionUDP::OnWriteSegment(char* buf, uint32_t count,
                                           uint32_t* countWritten) {
  if (count == 0) {
    // some WriteSegments implementations will erroneously call the reader
    // to provide 0 bytes worth of data.  we must protect against this case
    // or else we'd end up closing the socket prematurely.
    NS_ERROR("bad WriteSegments implementation");
    return NS_ERROR_FAILURE;  // stop iterating
  }

  if (ChaosMode::isActive(ChaosFeature::IOAmounts) &&
      ChaosMode::randomUint32LessThan(2)) {
    // read 1...count bytes
    count = ChaosMode::randomUint32LessThan(count) + 1;
  }

  nsresult rv = mSocketIn->Read(buf, count, countWritten);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (*countWritten == 0) {
    return NS_BASE_STREAM_CLOSED;
  }

  return NS_OK;
}

void HttpConnectionUDP::OnQuicTimeout(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("HttpConnectionUDP::OnQuicTimeout [this=%p]\n", aClosure));

  HttpConnectionUDP* self = static_cast<HttpConnectionUDP*>(aClosure);
  self->OnQuicTimeoutExpired();
}

void HttpConnectionUDP::OnQuicTimeoutExpired() {
  // if the transaction was dropped...
  if (!mHttp3Session) {
    LOG(("  no transaction; ignoring event\n"));
    return;
  }

  nsresult rv = mHttp3Session->ProcessOutputAndEvents();
  if (NS_FAILED(rv)) {
    CloseTransaction(mHttp3Session, rv);
  }
}

nsresult HttpConnectionUDP::OnSocketReadable() {
  LOG(("HttpConnectionUDP::OnSocketReadable [this=%p]\n", this));

  if (!mHttp3Session) {
    LOG(("  No session In OnSocketReadable\n"));
    return NS_ERROR_FAILURE;
  }

  PRIntervalTime now = PR_IntervalNow();

  // Reduce the estimate of the time since last read by up to 1 RTT to
  // accommodate exhausted sender TCP congestion windows or minor I/O delays.
  mLastReadTime = now;

  uint32_t n = 0;
  bool again = true;

  nsresult rv = mHttp3Session->WriteSegmentsAgain(
      this, nsIOService::gDefaultSegmentSize, &n, &again);
  LOG(("HttpConnectionUDP::OnSocketReadable %p trans->ws rv=%" PRIx32
       " n=%d \n",
       this, static_cast<uint32_t>(rv), n));
  if (NS_FAILED(rv)) {
    return rv;
  }

  mTotalBytesRead += n;

  return rv;
}

//-----------------------------------------------------------------------------
// HttpConnectionUDP::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(HttpConnectionUDP)
NS_IMPL_RELEASE(HttpConnectionUDP)

NS_INTERFACE_MAP_BEGIN(HttpConnectionUDP)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(HttpConnectionBase)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(HttpConnectionUDP)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// HttpConnectionUDP::nsIInputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket transport thread
NS_IMETHODIMP
HttpConnectionUDP::OnInputStreamReady(nsIAsyncInputStream* in) {
  MOZ_ASSERT(in == mSocketIn, "unexpected stream");
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // if the transaction was dropped...
  if (!mHttp3Session) {
    LOG(("  no transaction; ignoring event\n"));
    return NS_OK;
  }

  nsresult rv = OnSocketReadable();
  if (NS_FAILED(rv)) CloseTransaction(mHttp3Session, rv);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpConnectionUDP::nsIOutputStreamCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpConnectionUDP::OnOutputStreamReady(nsIAsyncOutputStream* out) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(out == mSocketOut, "unexpected socket");
  // if the transaction was dropped...
  if (!mHttp3Session) {
    LOG(("  no transaction; ignoring event\n"));
    return NS_OK;
  }

  nsresult rv = OnSocketWritable();
  if (NS_FAILED(rv)) CloseTransaction(mHttp3Session, rv);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpConnectionUDP::nsITransportEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpConnectionUDP::OnTransportStatus(nsITransport* trans, nsresult status,
                                     int64_t progress, int64_t progressMax) {
  if (mHttp3Session) mHttp3Session->OnTransportStatus(trans, status, progress);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpConnectionUDP::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

// not called on the socket transport thread
NS_IMETHODIMP
HttpConnectionUDP::GetInterface(const nsIID& iid, void** result) {
  // NOTE: This function is only called on the UI thread via sync proxy from
  //       the socket transport thread.  If that weren't the case, then we'd
  //       have to worry about the possibility of mHttp3Session going away
  //       part-way through this function call.  See CloseTransaction.

  // NOTE - there is a bug here, the call to getinterface is proxied off the
  // nss thread, not the ui thread as the above comment says. So there is
  // indeed a chance of mSession going away. bug 615342

  MOZ_ASSERT(!OnSocketThread(), "on socket thread");

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  {
    MutexAutoLock lock(mCallbacksLock);
    callbacks = mCallbacks;
  }
  if (callbacks) return callbacks->GetInterface(iid, result);
  return NS_ERROR_NO_INTERFACE;
}

void HttpConnectionUDP::SetEvent(nsresult aStatus) {
  switch (aStatus) {
    case NS_NET_STATUS_RESOLVING_HOST:
      mBootstrappedTimings.domainLookupStart = TimeStamp::Now();
      break;
    case NS_NET_STATUS_RESOLVED_HOST:
      mBootstrappedTimings.domainLookupEnd = TimeStamp::Now();
      break;
    case NS_NET_STATUS_CONNECTING_TO:
      mBootstrappedTimings.connectStart = TimeStamp::Now();
      break;
    case NS_NET_STATUS_CONNECTED_TO: {
      TimeStamp tnow = TimeStamp::Now();
      mBootstrappedTimings.tcpConnectEnd = tnow;
      mBootstrappedTimings.connectEnd = tnow;
      if (!mBootstrappedTimings.secureConnectionStart.IsNull()) {
        mBootstrappedTimings.secureConnectionStart = tnow;
      }
      break;
    }
    case NS_NET_STATUS_TLS_HANDSHAKE_STARTING:
      mBootstrappedTimings.secureConnectionStart = TimeStamp::Now();
      break;
    case NS_NET_STATUS_TLS_HANDSHAKE_ENDED:
      mBootstrappedTimings.connectEnd = TimeStamp::Now();
      break;
    default:
      break;
  }
}

bool HttpConnectionUDP::IsProxyConnectInProgress() { return false; }

bool HttpConnectionUDP::LastTransactionExpectedNoContent() {
  return mLastTransactionExpectedNoContent;
}

void HttpConnectionUDP::SetLastTransactionExpectedNoContent(bool val) {
  mLastTransactionExpectedNoContent = val;
}

bool HttpConnectionUDP::IsPersistent() { return !mDontReuse; }

nsAHttpTransaction* HttpConnectionUDP::Transaction() { return mHttp3Session; }

}  // namespace net
}  // namespace mozilla
