/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "Http2Session.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsISocketProvider.h"
#include "nsSocketProviderService.h"
#include "nsISSLSocketControl.h"
#include "nsISocketTransport.h"
#include "nsISupportsPriority.h"
#include "nsNetAddr.h"
#include "prerror.h"
#include "prio.h"
#include "Http2ConnectTransaction.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsSocketTransport2.h"
#include "nsSocketTransportService2.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Mutex.h"
#include "nsIWeakReferenceUtils.h"

#if defined(FUZZING)
#  include "FuzzySecurityInfo.h"
#  include "mozilla/StaticPrefs_fuzzing.h"
#endif

namespace mozilla {
namespace net {

class WeakTransProxy final : public nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WeakTransProxy(Http2ConnectTransaction* aTrans) {
    MOZ_ASSERT(OnSocketThread());
    mWeakTrans = do_GetWeakReference(aTrans);
  }

  already_AddRefed<NullHttpTransaction> QueryTransaction() {
    MOZ_ASSERT(OnSocketThread());
    RefPtr<NullHttpTransaction> trans = do_QueryReferent(mWeakTrans);
    return trans.forget();
  }

 private:
  ~WeakTransProxy() { MOZ_ASSERT(OnSocketThread()); }

  nsWeakPtr mWeakTrans;
};

NS_IMPL_ISUPPORTS(WeakTransProxy, nsISupports)

class WeakTransFreeProxy final : public Runnable {
 public:
  explicit WeakTransFreeProxy(WeakTransProxy* proxy)
      : Runnable("WeakTransFreeProxy"), mProxy(proxy) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(OnSocketThread());
    mProxy = nullptr;
    return NS_OK;
  }

  void Dispatch() {
    MOZ_ASSERT(!OnSocketThread());
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    Unused << sts->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  }

 private:
  RefPtr<WeakTransProxy> mProxy;
};

class SocketTransportShim : public nsISocketTransport {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORT
  NS_DECL_NSISOCKETTRANSPORT

  explicit SocketTransportShim(Http2ConnectTransaction* aTrans,
                               nsISocketTransport* aWrapped, bool aIsWebsocket)
      : mWrapped(aWrapped), mIsWebsocket(aIsWebsocket) {
    mWeakTrans = new WeakTransProxy(aTrans);
  }

 private:
  virtual ~SocketTransportShim() {
    if (!OnSocketThread()) {
      RefPtr<WeakTransFreeProxy> p = new WeakTransFreeProxy(mWeakTrans);
      mWeakTrans = nullptr;
      p->Dispatch();
    }
  }

  nsCOMPtr<nsISocketTransport> mWrapped;
  bool mIsWebsocket;
  nsCOMPtr<nsIInterfaceRequestor> mSecurityCallbacks;
  RefPtr<WeakTransProxy> mWeakTrans;  // Http2ConnectTransaction *
};

class OutputStreamShim : public nsIAsyncOutputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM

  friend class Http2ConnectTransaction;
  friend class WebsocketHasDataToWrite;
  friend class OutputCloseTransaction;

  OutputStreamShim(Http2ConnectTransaction* aTrans, bool aIsWebsocket)
      : mCallback(nullptr),
        mStatus(NS_OK),
        mMutex("OutputStreamShim"),
        mIsWebsocket(aIsWebsocket) {
    mWeakTrans = new WeakTransProxy(aTrans);
  }

  already_AddRefed<nsIOutputStreamCallback> TakeCallback();

 private:
  virtual ~OutputStreamShim() {
    if (!OnSocketThread()) {
      RefPtr<WeakTransFreeProxy> p = new WeakTransFreeProxy(mWeakTrans);
      mWeakTrans = nullptr;
      p->Dispatch();
    }
  }

  RefPtr<WeakTransProxy> mWeakTrans;  // Http2ConnectTransaction *
  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  nsresult mStatus;
  mozilla::Mutex mMutex;

  // Websockets
  bool mIsWebsocket;
  nsresult CallTransactionHasDataToWrite();
  nsresult CloseTransaction(nsresult reason);
};

class InputStreamShim : public nsIAsyncInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  friend class Http2ConnectTransaction;
  friend class InputCloseTransaction;

  InputStreamShim(Http2ConnectTransaction* aTrans, bool aIsWebsocket)
      : mCallback(nullptr),
        mStatus(NS_OK),
        mMutex("InputStreamShim"),
        mIsWebsocket(aIsWebsocket) {
    mWeakTrans = new WeakTransProxy(aTrans);
  }

  already_AddRefed<nsIInputStreamCallback> TakeCallback();
  bool HasCallback();

 private:
  virtual ~InputStreamShim() {
    if (!OnSocketThread()) {
      RefPtr<WeakTransFreeProxy> p = new WeakTransFreeProxy(mWeakTrans);
      mWeakTrans = nullptr;
      p->Dispatch();
    }
  }

  RefPtr<WeakTransProxy> mWeakTrans;  // Http2ConnectTransaction *
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsresult mStatus;
  mozilla::Mutex mMutex;

  // Websockets
  bool mIsWebsocket;
  nsresult CloseTransaction(nsresult reason);
};

Http2ConnectTransaction::Http2ConnectTransaction(
    nsHttpConnectionInfo* ci, nsIInterfaceRequestor* callbacks, uint32_t caps,
    nsHttpTransaction* trans, nsAHttpConnection* session, bool isWebsocket)
    : NullHttpTransaction(ci, callbacks, caps | NS_HTTP_ALLOW_KEEPALIVE),
      mConnectStringOffset(0),
      mSession(session),
      mSegmentReader(nullptr),
      mInputDataSize(0),
      mInputDataUsed(0),
      mInputDataOffset(0),
      mOutputDataSize(0),
      mOutputDataUsed(0),
      mOutputDataOffset(0),
      mForcePlainText(false),
      mIsWebsocket(isWebsocket),
      mConnRefTaken(false),
      mCreateShimErrorCalled(false) {
  LOG(("Http2ConnectTransaction ctor %p\n", this));

  mTimestampSyn = TimeStamp::Now();
  mRequestHead = new nsHttpRequestHead();
  if (mIsWebsocket) {
    // Ensure our request head has all the websocket headers duplicated from the
    // original transaction before calling the boilerplate stuff to create the
    // rest of the CONNECT headers.
    trans->RequestHead()->Enter();
    mRequestHead->SetHeaders(trans->RequestHead()->Headers());
    trans->RequestHead()->Exit();
  }
  DebugOnly<nsresult> rv = nsHttpConnection::MakeConnectString(
      trans, mRequestHead, mConnectString, mIsWebsocket,
      mCaps & NS_HTTP_USE_RFP);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  mDrivingTransaction = trans;
}

Http2ConnectTransaction::~Http2ConnectTransaction() {
  LOG(("Http2ConnectTransaction dtor %p\n", this));

  MOZ_ASSERT(OnSocketThread());

  if (mDrivingTransaction) {
    // requeue it I guess. This should be gone.
    mDrivingTransaction->SetH2WSTransaction(nullptr);
    Unused << gHttpHandler->InitiateTransaction(
        mDrivingTransaction, mDrivingTransaction->Priority());
  }
}

void Http2ConnectTransaction::ForcePlainText() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mInputDataUsed && !mInputDataSize && !mInputDataOffset);
  MOZ_ASSERT(!mForcePlainText);
  MOZ_ASSERT(!mTunnelTransport, "call before mapstreamtohttpconnection");
  MOZ_ASSERT(!mIsWebsocket);

  mForcePlainText = true;
}

bool Http2ConnectTransaction::MapStreamToHttpConnection(
    nsISocketTransport* aTransport, nsHttpConnectionInfo* aConnInfo,
    const nsACString& aFlat407Headers, int32_t aHttpResponseCode) {
  MOZ_ASSERT(OnSocketThread());

  if (aHttpResponseCode >= 100 && aHttpResponseCode < 200) {
    LOG(
        ("Http2ConnectTransaction::MapStreamToHttpConnection %p skip "
         "pre-response with response code %d",
         this, aHttpResponseCode));
    return false;
  }

  mTunnelTransport = new SocketTransportShim(this, aTransport, mIsWebsocket);
  mTunnelStreamIn = new InputStreamShim(this, mIsWebsocket);
  mTunnelStreamOut = new OutputStreamShim(this, mIsWebsocket);
  mTunneledConn = new nsHttpConnection();

  // If aHttpResponseCode is -1, it means that proxy connect is not used. We
  // should not call HttpProxyResponseToErrorCode(), since this will create a
  // shim error.
  if (aHttpResponseCode > 0 && aHttpResponseCode != 200) {
    nsresult err = HttpProxyResponseToErrorCode(aHttpResponseCode);
    if (NS_FAILED(err)) {
      CreateShimError(err);
    }
  }

  // this new http connection has a specific hashkey (i.e. to a particular
  // host via the tunnel) and is associated with the tunnel streams
  LOG(("Http2ConnectTransaction %p new httpconnection %p %s\n", this,
       mTunneledConn.get(), aConnInfo->HashKey().get()));

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  GetSecurityCallbacks(getter_AddRefs(callbacks));
  mTunneledConn->SetTransactionCaps(Caps());
  MOZ_ASSERT(aConnInfo->UsingHttpsProxy() || mIsWebsocket);
  TimeDuration rtt = TimeStamp::Now() - mTimestampSyn;
  DebugOnly<nsresult> rv = mTunneledConn->Init(
      aConnInfo, gHttpHandler->ConnMgr()->MaxRequestDelay(), mTunnelTransport,
      mTunnelStreamIn, mTunnelStreamOut, true, NS_OK, callbacks,
      PR_MillisecondsToInterval(static_cast<uint32_t>(rtt.ToMilliseconds())),
      false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (mForcePlainText) {
    mTunneledConn->ForcePlainText();
  } else if (mIsWebsocket) {
    LOG(("Http2ConnectTransaction::MapStreamToHttpConnection %p websocket",
         this));
    // Let the root transaction know about us, so it can pass our own conn
    // to the websocket.
    mDrivingTransaction->SetH2WSTransaction(this);
  } else {
    mTunneledConn->SetupSecondaryTLS(this);
    mTunneledConn->SetInSpdyTunnel();
  }

  // make the originating transaction stick to the tunneled conn
  RefPtr<nsAHttpConnection> wrappedConn =
      gHttpHandler->ConnMgr()->MakeConnectionHandle(mTunneledConn);
  mDrivingTransaction->SetConnection(wrappedConn);
  mDrivingTransaction->MakeSticky();

  if (!mIsWebsocket) {
    mDrivingTransaction->OnProxyConnectComplete(aHttpResponseCode);

    if (aHttpResponseCode == 407) {
      mDrivingTransaction->SetFlat407Headers(aFlat407Headers);
      mDrivingTransaction->SetProxyConnectFailed();
    }

    // jump the priority and start the dispatcher
    Unused << gHttpHandler->InitiateTransaction(
        mDrivingTransaction, nsISupportsPriority::PRIORITY_HIGHEST - 60);
    mDrivingTransaction = nullptr;
  }

  return true;
}

nsresult Http2ConnectTransaction::Flush(uint32_t count, uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http2ConnectTransaction::Flush %p count %d avail %d\n", this, count,
       mOutputDataUsed - mOutputDataOffset));

  if (!mSegmentReader) {
    return NS_ERROR_UNEXPECTED;
  }

  *countRead = 0;
  count = std::min(count, (mOutputDataUsed - mOutputDataOffset));
  if (count) {
    nsresult rv;
    rv = mSegmentReader->OnReadSegment(&mOutputData[mOutputDataOffset], count,
                                       countRead);
    if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
      LOG(("Http2ConnectTransaction::Flush %p Error %" PRIx32 "\n", this,
           static_cast<uint32_t>(rv)));
      CreateShimError(rv);
      return rv;
    }
  }

  mOutputDataOffset += *countRead;
  if (mOutputDataOffset == mOutputDataUsed) {
    mOutputDataOffset = mOutputDataUsed = 0;
  }
  if (!(*countRead)) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (mOutputDataUsed != mOutputDataOffset) {
    LOG(("Http2ConnectTransaction::Flush %p Incomplete %d\n", this,
         mOutputDataUsed - mOutputDataOffset));
    mSession->TransactionHasDataToWrite(this);
  }

  return NS_OK;
}

nsresult Http2ConnectTransaction::ReadSegments(nsAHttpSegmentReader* reader,
                                               uint32_t count,
                                               uint32_t* countRead) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http2ConnectTransaction::ReadSegments %p count %d conn %p\n", this,
       count, mTunneledConn.get()));

  mSegmentReader = reader;

  // spdy stream carrying tunnel is not setup yet.
  if (!mTunneledConn) {
    uint32_t toWrite = mConnectString.Length() - mConnectStringOffset;
    toWrite = std::min(toWrite, count);
    *countRead = toWrite;
    if (toWrite) {
      nsresult rv = mSegmentReader->OnReadSegment(
          mConnectString.BeginReading() + mConnectStringOffset, toWrite,
          countRead);
      if (NS_FAILED(rv) && (rv != NS_BASE_STREAM_WOULD_BLOCK)) {
        LOG(
            ("Http2ConnectTransaction::ReadSegments %p OnReadSegmentError "
             "%" PRIx32 "\n",
             this, static_cast<uint32_t>(rv)));
        CreateShimError(rv);
      } else {
        mConnectStringOffset += toWrite;
        if (mConnectString.Length() == mConnectStringOffset) {
          mConnectString.Truncate();
          mConnectStringOffset = 0;
        }
      }
      return rv;
    }

    LOG(("SpdyConnectTransaciton::ReadSegments %p connect request consumed",
         this));
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (mForcePlainText) {
    // this path just ignores sending the request so that we can
    // send a synthetic reply in writesegments()
    LOG(
        ("SpdyConnectTransaciton::ReadSegments %p dropping %d output bytes "
         "due to synthetic reply\n",
         this, mOutputDataUsed - mOutputDataOffset));
    *countRead = mOutputDataUsed - mOutputDataOffset;
    mOutputDataOffset = mOutputDataUsed = 0;
    mTunneledConn->DontReuse();
    return NS_OK;
  }

  *countRead = 0;
  nsresult rv = Flush(count, countRead);
  if (!(*countRead)) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  nsCOMPtr<nsIOutputStreamCallback> cb = mTunnelStreamOut->TakeCallback();
  if (!cb) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  // See if there is any more data available
  rv = cb->OnOutputStreamReady(mTunnelStreamOut);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Write out anything that may have come out of the stream just above
  uint32_t subtotal;
  count -= *countRead;
  rv = Flush(count, &subtotal);
  *countRead += subtotal;

  return rv;
}

void Http2ConnectTransaction::CreateShimError(nsresult code) {
  LOG(("Http2ConnectTransaction::CreateShimError %p 0x%08" PRIx32, this,
       static_cast<uint32_t>(code)));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(NS_FAILED(code));

  MOZ_ASSERT(!mCreateShimErrorCalled);
  if (mCreateShimErrorCalled) {
    return;
  }
  mCreateShimErrorCalled = true;

  if (mTunnelStreamOut && NS_SUCCEEDED(mTunnelStreamOut->mStatus)) {
    mTunnelStreamOut->mStatus = code;
  }

  if (mTunnelStreamIn && NS_SUCCEEDED(mTunnelStreamIn->mStatus)) {
    mTunnelStreamIn->mStatus = code;
  }

  if (mTunnelStreamIn) {
    nsCOMPtr<nsIInputStreamCallback> cb = mTunnelStreamIn->TakeCallback();
    if (cb) {
      cb->OnInputStreamReady(mTunnelStreamIn);
    }
  }

  if (mTunnelStreamOut) {
    nsCOMPtr<nsIOutputStreamCallback> cb = mTunnelStreamOut->TakeCallback();
    if (cb) {
      cb->OnOutputStreamReady(mTunnelStreamOut);
    }
  }
  mCreateShimErrorCalled = false;
}

nsresult Http2ConnectTransaction::WriteDataToBuffer(
    nsAHttpSegmentWriter* writer, uint32_t count, uint32_t* countWritten) {
  EnsureBuffer(mInputData, mInputDataUsed + count, mInputDataUsed,
               mInputDataSize);
  nsresult rv =
      writer->OnWriteSegment(&mInputData[mInputDataUsed], count, countWritten);
  if (NS_FAILED(rv)) {
    if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
      LOG(
          ("Http2ConnectTransaction::WriteSegments wrapped writer %p Error "
           "%" PRIx32 "\n",
           this, static_cast<uint32_t>(rv)));
      CreateShimError(rv);
    }
    return rv;
  }
  mInputDataUsed += *countWritten;
  LOG(
      ("Http2ConnectTransaction %p %d new bytes [%d total] of ciphered data "
       "buffered\n",
       this, *countWritten, mInputDataUsed - mInputDataOffset));

  return rv;
}

nsresult Http2ConnectTransaction::WriteSegments(nsAHttpSegmentWriter* writer,
                                                uint32_t count,
                                                uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("Http2ConnectTransaction::WriteSegments %p max=%d", this, count));

  // For websockets, we need to forward the initial response through to the base
  // transaction so the normal websocket plumbing can do all the things it needs
  // to do.
  if (mIsWebsocket) {
    return WebsocketWriteSegments(writer, count, countWritten);
  }

  // first call into the tunnel stream to get the demux'd data out of the
  // spdy session.
  nsresult rv = WriteDataToBuffer(writer, count, countWritten);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInputStreamCallback> cb =
      mTunneledConn ? mTunnelStreamIn->TakeCallback() : nullptr;
  LOG(("Http2ConnectTransaction::WriteSegments %p cb=%p", this, cb.get()));

  if (!cb) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  rv = cb->OnInputStreamReady(mTunnelStreamIn);
  LOG(
      ("Http2ConnectTransaction::WriteSegments %p "
       "after InputStreamReady callback %d total of ciphered data buffered "
       "rv=%" PRIx32 "\n",
       this, mInputDataUsed - mInputDataOffset, static_cast<uint32_t>(rv)));
  LOG(
      ("Http2ConnectTransaction::WriteSegments %p "
       "goodput %p out %" PRId64 "\n",
       this, mTunneledConn.get(), mTunneledConn->ContentBytesWritten()));
  if (NS_SUCCEEDED(rv) && !mTunneledConn->ContentBytesWritten()) {
    nsCOMPtr<nsIOutputStreamCallback> ocb = mTunnelStreamOut->TakeCallback();
    mTunnelStreamOut->AsyncWait(ocb, 0, 0, nullptr);
  }
  return rv;
}

nsresult Http2ConnectTransaction::WebsocketWriteSegments(
    nsAHttpSegmentWriter* writer, uint32_t count, uint32_t* countWritten) {
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(mIsWebsocket);
  LOG(("Http2ConnectTransaction::WebsocketWriteSegments %p max=%d", this,
       count));

  if (mDrivingTransaction && !mDrivingTransaction->IsDone()) {
    // Transaction hasn't received end of headers yet, so keep passing data to
    // it until it has. Then we can take over.
    nsresult rv =
        mDrivingTransaction->WriteSegments(writer, count, countWritten);
    if (NS_SUCCEEDED(rv) && mDrivingTransaction->IsDone() && !mConnRefTaken) {
      mDrivingTransaction->Close(NS_OK);
    }
  }

  if (!mConnRefTaken) {
    // Force driving transaction to finish so the websocket channel can get its
    // notifications correctly and start driving.
    MOZ_ASSERT(mDrivingTransaction);
    mDrivingTransaction->Close(NS_OK);
  }

  nsresult rv = WriteDataToBuffer(writer, count, countWritten);
  if (NS_SUCCEEDED(rv)) {
    if (!mTunneledConn) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    nsCOMPtr<nsIInputStreamCallback> cb = mTunnelStreamIn->TakeCallback();
    if (!cb) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    rv = cb->OnInputStreamReady(mTunnelStreamIn);
  }

  return rv;
}

bool Http2ConnectTransaction::ConnectedReadyForInput() {
  return mTunneledConn && mTunnelStreamIn->HasCallback();
}

nsHttpRequestHead* Http2ConnectTransaction::RequestHead() {
  return mRequestHead;
}

void Http2ConnectTransaction::Close(nsresult code) {
  LOG(("Http2ConnectTransaction close %p %" PRIx32 "\n", this,
       static_cast<uint32_t>(code)));

  MOZ_ASSERT(OnSocketThread());

  if (mIsWebsocket && mDrivingTransaction) {
    mDrivingTransaction->SetH2WSTransaction(nullptr);
    if (!mConnRefTaken) {
      // This indicates that the websocket failed to set up, so just close down
      // the transaction as usual.
      mDrivingTransaction->Close(code);
      mDrivingTransaction = nullptr;
    }
  }
  NullHttpTransaction::Close(code);
  if (NS_FAILED(code) && (code != NS_BASE_STREAM_WOULD_BLOCK)) {
    CreateShimError(code);
  } else {
    CreateShimError(NS_BASE_STREAM_CLOSED);
  }
}

void Http2ConnectTransaction::SetConnRefTaken() {
  MOZ_ASSERT(OnSocketThread());

  mConnRefTaken = true;
  mDrivingTransaction = nullptr;  // Just in case
}

already_AddRefed<nsIOutputStreamCallback> OutputStreamShim::TakeCallback() {
  mozilla::MutexAutoLock lock(mMutex);
  return mCallback.forget();
}

class WebsocketHasDataToWrite final : public Runnable {
 public:
  explicit WebsocketHasDataToWrite(OutputStreamShim* shim)
      : Runnable("WebsocketHasDataToWrite"), mShim(shim) {}

  ~WebsocketHasDataToWrite() = default;

  NS_IMETHOD Run() override { return mShim->CallTransactionHasDataToWrite(); }

  [[nodiscard]] nsresult Dispatch() {
    if (OnSocketThread()) {
      return Run();
    }

    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  }

 private:
  RefPtr<OutputStreamShim> mShim;
};

NS_IMETHODIMP
OutputStreamShim::AsyncWait(nsIOutputStreamCallback* callback,
                            unsigned int flags, unsigned int requestedCount,
                            nsIEventTarget* target) {
  if (mIsWebsocket) {
    // With websockets, AsyncWait may be called from the main thread, but the
    // target is on the socket thread. That's all we really care about.
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT((!target && !callback) || (target == sts));
    if (target && (target != sts)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    bool currentThread;

    if (target && (NS_FAILED(target->IsOnCurrentThread(&currentThread)) ||
                   !currentThread)) {
      return NS_ERROR_FAILURE;
    }
  }

  LOG(("OutputStreamShim::AsyncWait %p callback %p\n", this, callback));

  {
    mozilla::MutexAutoLock lock(mMutex);
    mCallback = callback;
  }

  RefPtr<WebsocketHasDataToWrite> wsdw = new WebsocketHasDataToWrite(this);
  Unused << wsdw->Dispatch();

  return NS_OK;
}

class OutputCloseTransaction final : public Runnable {
 public:
  OutputCloseTransaction(OutputStreamShim* shim, nsresult reason)
      : Runnable("OutputCloseTransaction"), mShim(shim), mReason(reason) {}

  ~OutputCloseTransaction() = default;

  NS_IMETHOD Run() override { return mShim->CloseTransaction(mReason); }

 private:
  RefPtr<OutputStreamShim> mShim;
  nsresult mReason;
};

NS_IMETHODIMP
OutputStreamShim::CloseWithStatus(nsresult reason) {
  if (!OnSocketThread()) {
    RefPtr<OutputCloseTransaction> oct =
        new OutputCloseTransaction(this, reason);
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(oct, nsIEventTarget::DISPATCH_NORMAL);
  }

  return CloseTransaction(reason);
}

nsresult OutputStreamShim::CloseTransaction(nsresult reason) {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  Http2ConnectTransaction* trans = baseTrans->QueryHttp2ConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  trans->mSession->CloseTransaction(trans, reason);
  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::Close() { return CloseWithStatus(NS_OK); }

NS_IMETHODIMP
OutputStreamShim::Flush() {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  Http2ConnectTransaction* trans = baseTrans->QueryHttp2ConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t count = trans->mOutputDataUsed - trans->mOutputDataOffset;
  if (!count) {
    return NS_OK;
  }

  uint32_t countRead;
  nsresult rv = trans->Flush(count, &countRead);
  LOG(("OutputStreamShim::Flush %p before %d after %d\n", this, count,
       trans->mOutputDataUsed - trans->mOutputDataOffset));
  return rv;
}

nsresult OutputStreamShim::CallTransactionHasDataToWrite() {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  Http2ConnectTransaction* trans = baseTrans->QueryHttp2ConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }
  trans->mSession->TransactionHasDataToWrite(trans);
  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  Http2ConnectTransaction* trans = baseTrans->QueryHttp2ConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  if ((trans->mOutputDataUsed + aCount) >= 512000) {
    *_retval = 0;
    // time for some flow control;
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  EnsureBuffer(trans->mOutputData, trans->mOutputDataUsed + aCount,
               trans->mOutputDataUsed, trans->mOutputDataSize);
  memcpy(&trans->mOutputData[trans->mOutputDataUsed], aBuf, aCount);
  trans->mOutputDataUsed += aCount;
  *_retval = aCount;
  LOG(("OutputStreamShim::Write %p new %d total %d\n", this, aCount,
       trans->mOutputDataUsed));

  trans->mSession->TransactionHasDataToWrite(trans);

  return NS_OK;
}

NS_IMETHODIMP
OutputStreamShim::WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                            uint32_t* _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: OutputStreamShim::WriteFrom %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OutputStreamShim::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                uint32_t aCount, uint32_t* _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: OutputStreamShim::WriteSegments %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OutputStreamShim::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

already_AddRefed<nsIInputStreamCallback> InputStreamShim::TakeCallback() {
  mozilla::MutexAutoLock lock(mMutex);
  return mCallback.forget();
}

bool InputStreamShim::HasCallback() {
  mozilla::MutexAutoLock lock(mMutex);
  return mCallback != nullptr;
}

class CheckAvailData final : public Runnable {
 public:
  explicit CheckAvailData(InputStreamShim* shim)
      : Runnable("CheckAvailData"), mShim(shim) {}

  ~CheckAvailData() = default;

  NS_IMETHOD Run() override {
    uint64_t avail = 0;
    if (NS_SUCCEEDED(mShim->Available(&avail)) && avail) {
      nsCOMPtr<nsIInputStreamCallback> cb = mShim->TakeCallback();
      if (cb) {
        cb->OnInputStreamReady(mShim);
      }
    }
    return NS_OK;
  }

  [[nodiscard]] nsresult Dispatch() {
    // Dispatch the event even if we're on socket thread to avoid closing and
    // destructing Http2Session in case this call is comming from
    // Http2Session::ReadSegments() and the callback closes the transaction in
    // OnInputStreamRead().
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
  }

 private:
  RefPtr<InputStreamShim> mShim;
};

NS_IMETHODIMP
InputStreamShim::AsyncWait(nsIInputStreamCallback* callback, unsigned int flags,
                           unsigned int requestedCount,
                           nsIEventTarget* target) {
  if (mIsWebsocket) {
    // With websockets, AsyncWait may be called from the main thread, but the
    // target is on the socket thread. That's all we really care about.
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT((!target && !callback) || (target == sts));
    if (target && (target != sts)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    bool currentThread;

    if (target && (NS_FAILED(target->IsOnCurrentThread(&currentThread)) ||
                   !currentThread)) {
      return NS_ERROR_FAILURE;
    }
  }

  LOG(("InputStreamShim::AsyncWait %p callback %p\n", this, callback));

  {
    mozilla::MutexAutoLock lock(mMutex);
    mCallback = callback;
  }

  if (callback) {
    RefPtr<CheckAvailData> cad = new CheckAvailData(this);
    Unused << cad->Dispatch();
  }

  return NS_OK;
}

class InputCloseTransaction final : public Runnable {
 public:
  InputCloseTransaction(InputStreamShim* shim, nsresult reason)
      : Runnable("InputCloseTransaction"), mShim(shim), mReason(reason) {}

  ~InputCloseTransaction() = default;

  NS_IMETHOD Run() override { return mShim->CloseTransaction(mReason); }

 private:
  RefPtr<InputStreamShim> mShim;
  nsresult mReason;
};

NS_IMETHODIMP
InputStreamShim::CloseWithStatus(nsresult reason) {
  if (!OnSocketThread()) {
    RefPtr<InputCloseTransaction> ict = new InputCloseTransaction(this, reason);
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1");
    return sts->Dispatch(ict, nsIEventTarget::DISPATCH_NORMAL);
  }

  return CloseTransaction(reason);
}

nsresult InputStreamShim::CloseTransaction(nsresult reason) {
  MOZ_ASSERT(OnSocketThread());
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  Http2ConnectTransaction* trans = baseTrans->QueryHttp2ConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  trans->mSession->CloseTransaction(trans, reason);
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::Close() { return CloseWithStatus(NS_OK); }

NS_IMETHODIMP
InputStreamShim::Available(uint64_t* _retval) {
  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  Http2ConnectTransaction* trans = baseTrans->QueryHttp2ConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  *_retval = trans->mInputDataUsed - trans->mInputDataOffset;
  return NS_OK;
}

NS_IMETHODIMP
InputStreamShim::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  RefPtr<NullHttpTransaction> baseTrans = mWeakTrans->QueryTransaction();
  if (!baseTrans) {
    return NS_ERROR_FAILURE;
  }
  Http2ConnectTransaction* trans = baseTrans->QueryHttp2ConnectTransaction();
  MOZ_ASSERT(trans);
  if (!trans) {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t avail = trans->mInputDataUsed - trans->mInputDataOffset;
  uint32_t tocopy = std::min(aCount, avail);
  *_retval = tocopy;
  memcpy(aBuf, &trans->mInputData[trans->mInputDataOffset], tocopy);
  trans->mInputDataOffset += tocopy;
  if (trans->mInputDataOffset == trans->mInputDataUsed) {
    trans->mInputDataOffset = trans->mInputDataUsed = 0;
  }

  return tocopy ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
}

NS_IMETHODIMP
InputStreamShim::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                              uint32_t aCount, uint32_t* _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: InputStreamShim::ReadSegments %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InputStreamShim::IsNonBlocking(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
SocketTransportShim::SetKeepaliveEnabled(bool aKeepaliveEnabled) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::SetKeepaliveEnabled %p called", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::SetKeepaliveVals(int32_t keepaliveIdleTime,
                                      int32_t keepaliveRetryInterval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::SetKeepaliveVals %p called", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::GetSecurityCallbacks(
    nsIInterfaceRequestor** aSecurityCallbacks) {
  if (mIsWebsocket) {
    nsCOMPtr<nsIInterfaceRequestor> out(mSecurityCallbacks);
    *aSecurityCallbacks = out.forget().take();
    return NS_OK;
  }

  return mWrapped->GetSecurityCallbacks(aSecurityCallbacks);
}

NS_IMETHODIMP
SocketTransportShim::SetSecurityCallbacks(
    nsIInterfaceRequestor* aSecurityCallbacks) {
  if (mIsWebsocket) {
    mSecurityCallbacks = aSecurityCallbacks;
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::OpenInputStream(uint32_t aFlags, uint32_t aSegmentSize,
                                     uint32_t aSegmentCount,
                                     nsIInputStream** _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::OpenInputStream %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::OpenOutputStream(uint32_t aFlags, uint32_t aSegmentSize,
                                      uint32_t aSegmentCount,
                                      nsIOutputStream** _retval) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::OpenOutputStream %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::Close(nsresult aReason) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::Close %p", this));
  } else {
    LOG(("SocketTransportShim::Close %p", this));
  }

  // Must always post, because mSession->CloseTransaction releases the
  // Http2Stream which is still on stack.
  RefPtr<SocketTransportShim> self(this);

  nsCOMPtr<nsIEventTarget> sts =
      do_GetService("@mozilla.org/network/socket-transport-service;1");
  Unused << sts->Dispatch(NS_NewRunnableFunction(
      "SocketTransportShim::Close", [self = std::move(self), aReason]() {
        RefPtr<NullHttpTransaction> baseTrans =
            self->mWeakTrans->QueryTransaction();
        if (!baseTrans) {
          return;
        }
        Http2ConnectTransaction* trans =
            baseTrans->QueryHttp2ConnectTransaction();
        MOZ_ASSERT(trans);
        if (!trans) {
          return;
        }

        trans->mSession->CloseTransaction(trans, aReason);
      }));

  return NS_OK;
}

NS_IMETHODIMP
SocketTransportShim::SetEventSink(nsITransportEventSink* aSink,
                                  nsIEventTarget* aEventTarget) {
  if (mIsWebsocket) {
    // Need to pretend, since websockets expect this to work
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::Bind(NetAddr* aLocalAddr) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::Bind %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::GetEchConfigUsed(bool* aEchConfigUsed) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::GetEchConfigUsed %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::SetEchConfig(const nsACString& aEchConfig) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::SetEchConfig %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SocketTransportShim::ResolvedByTRR(bool* aResolvedByTRR) {
  if (mIsWebsocket) {
    LOG3(("WARNING: SocketTransportShim::IsTRR %p", this));
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

#define FWD_TS_PTR(fx, ts) \
  NS_IMETHODIMP            \
  SocketTransportShim::fx(ts* arg) { return mWrapped->fx(arg); }

#define FWD_TS_ADDREF(fx, ts) \
  NS_IMETHODIMP               \
  SocketTransportShim::fx(ts** arg) { return mWrapped->fx(arg); }

#define FWD_TS(fx, ts) \
  NS_IMETHODIMP        \
  SocketTransportShim::fx(ts arg) { return mWrapped->fx(arg); }

FWD_TS_PTR(GetKeepaliveEnabled, bool);
FWD_TS_PTR(GetSendBufferSize, uint32_t);
FWD_TS(SetSendBufferSize, uint32_t);
FWD_TS_PTR(GetPort, int32_t);
FWD_TS_PTR(GetPeerAddr, mozilla::net::NetAddr);
FWD_TS_PTR(GetSelfAddr, mozilla::net::NetAddr);
FWD_TS_ADDREF(GetScriptablePeerAddr, nsINetAddr);
FWD_TS_ADDREF(GetScriptableSelfAddr, nsINetAddr);
FWD_TS_ADDREF(GetTlsSocketControl, nsISSLSocketControl);
FWD_TS_PTR(IsAlive, bool);
FWD_TS_PTR(GetConnectionFlags, uint32_t);
FWD_TS(SetConnectionFlags, uint32_t);
FWD_TS(SetIsPrivate, bool);
FWD_TS_PTR(GetTlsFlags, uint32_t);
FWD_TS(SetTlsFlags, uint32_t);
FWD_TS_PTR(GetRecvBufferSize, uint32_t);
FWD_TS(SetRecvBufferSize, uint32_t);
FWD_TS_PTR(GetResetIPFamilyPreference, bool);

nsresult SocketTransportShim::GetOriginAttributes(
    mozilla::OriginAttributes* aOriginAttributes) {
  return mWrapped->GetOriginAttributes(aOriginAttributes);
}

nsresult SocketTransportShim::SetOriginAttributes(
    const mozilla::OriginAttributes& aOriginAttributes) {
  return mWrapped->SetOriginAttributes(aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::GetScriptableOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aOriginAttributes) {
  return mWrapped->GetScriptableOriginAttributes(aCx, aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::SetScriptableOriginAttributes(
    JSContext* aCx, JS::Handle<JS::Value> aOriginAttributes) {
  return mWrapped->SetScriptableOriginAttributes(aCx, aOriginAttributes);
}

NS_IMETHODIMP
SocketTransportShim::GetHost(nsACString& aHost) {
  return mWrapped->GetHost(aHost);
}

NS_IMETHODIMP
SocketTransportShim::GetTimeout(uint32_t aType, uint32_t* _retval) {
  return mWrapped->GetTimeout(aType, _retval);
}

NS_IMETHODIMP
SocketTransportShim::SetTimeout(uint32_t aType, uint32_t aValue) {
  return mWrapped->SetTimeout(aType, aValue);
}

NS_IMETHODIMP
SocketTransportShim::SetReuseAddrPort(bool aReuseAddrPort) {
  return mWrapped->SetReuseAddrPort(aReuseAddrPort);
}

NS_IMETHODIMP
SocketTransportShim::SetLinger(bool aPolarity, int16_t aTimeout) {
  return mWrapped->SetLinger(aPolarity, aTimeout);
}

NS_IMETHODIMP
SocketTransportShim::GetQoSBits(uint8_t* aQoSBits) {
  return mWrapped->GetQoSBits(aQoSBits);
}

NS_IMETHODIMP
SocketTransportShim::SetQoSBits(uint8_t aQoSBits) {
  return mWrapped->SetQoSBits(aQoSBits);
}

NS_IMETHODIMP
SocketTransportShim::GetRetryDnsIfPossible(bool* aRetry) {
  return mWrapped->GetRetryDnsIfPossible(aRetry);
}

NS_IMETHODIMP
SocketTransportShim::GetStatus(nsresult* aStatus) {
  return mWrapped->GetStatus(aStatus);
}

NS_IMPL_ISUPPORTS(SocketTransportShim, nsISocketTransport, nsITransport)
NS_IMPL_ISUPPORTS(InputStreamShim, nsIInputStream, nsIAsyncInputStream)
NS_IMPL_ISUPPORTS(OutputStreamShim, nsIOutputStream, nsIAsyncOutputStream)

}  // namespace net
}  // namespace mozilla
