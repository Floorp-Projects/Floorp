/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportLog.h"
#include "Http3WebTransportSession.h"
#include "Http3WebTransportStream.h"
#include "WebTransportSessionProxy.h"
#include "WebTransportStreamProxy.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIHttpChannel.h"
#include "nsIRequest.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsSocketTransportService2.h"
#include "mozilla/Logging.h"

namespace mozilla::net {

LazyLogModule webTransportLog("nsWebTransport");

NS_IMPL_ISUPPORTS(WebTransportSessionProxy, WebTransportSessionEventListener,
                  nsIWebTransport, nsIRedirectResultListener, nsIStreamListener,
                  nsIChannelEventSink, nsIInterfaceRequestor);

WebTransportSessionProxy::WebTransportSessionProxy()
    : mMutex("WebTransportSessionProxy::mMutex") {
  LOG(("WebTransportSessionProxy constructor"));
}

WebTransportSessionProxy::~WebTransportSessionProxy() {
  if (OnSocketThread()) {
    return;
  }

  MutexAutoLock lock(mMutex);
  if ((mState != WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED) &&
      (mState != WebTransportSessionProxyState::ACTIVE) &&
      (mState != WebTransportSessionProxyState::SESSION_CLOSE_PENDING)) {
    return;
  }

  MOZ_ASSERT(mState != WebTransportSessionProxyState::SESSION_CLOSE_PENDING,
             "We can not be in the SESSION_CLOSE_PENDING state in destructor, "
             "because should e an runnable  that holds reference to this"
             "object.");

  Unused << gSocketTransportService->Dispatch(NS_NewRunnableFunction(
      "WebTransportSessionProxy::ProxyHttp3WebTransportSessionRelease",
      [self{std::move(mWebTransportSession)}]() {}));
}

//-----------------------------------------------------------------------------
// WebTransportSessionProxy::nsIWebTransport
//-----------------------------------------------------------------------------

nsresult WebTransportSessionProxy::AsyncConnect(
    nsIURI* aURI, nsIPrincipal* aPrincipal, uint32_t aSecurityFlags,
    WebTransportSessionEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("WebTransportSessionProxy::AsyncConnect"));
  {
    MutexAutoLock lock(mMutex);
    mListener = aListener;
  }
  nsSecurityFlags flags = nsILoadInfo::SEC_COOKIES_OMIT | aSecurityFlags;
  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL |
                          nsIRequest::LOAD_BYPASS_CACHE |
                          nsIRequest::INHIBIT_CACHING;
  nsresult rv = NS_NewChannel(getter_AddRefs(mChannel), aURI, aPrincipal, flags,
                              nsContentPolicyType::TYPE_OTHER,
                              /* aCookieJarSettings */ nullptr,
                              /* aPerformanceStorage */ nullptr,
                              /* aLoadGroup */ nullptr,
                              /* aCallbacks */ this, loadFlags);

  NS_ENSURE_SUCCESS(rv, rv);

  // configure HTTP specific stuff
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    mChannel = nullptr;
    return NS_ERROR_ABORT;
  }

  {
    MutexAutoLock lock(mMutex);
    ChangeState(WebTransportSessionProxyState::NEGOTIATING);
  }

  rv = mChannel->AsyncOpen(this);
  if (NS_FAILED(rv)) {
    MutexAutoLock lock(mMutex);
    ChangeState(WebTransportSessionProxyState::DONE);
  }
  return rv;
}

NS_IMETHODIMP
WebTransportSessionProxy::GetStats() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
WebTransportSessionProxy::CloseSession(uint32_t status,
                                       const nsACString& reason) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  mCloseStatus = status;
  mReason = reason;
  mListener = nullptr;
  mPendingEvents.Clear();
  switch (mState) {
    case WebTransportSessionProxyState::INIT:
    case WebTransportSessionProxyState::DONE:
      return NS_ERROR_NOT_INITIALIZED;
    case WebTransportSessionProxyState::NEGOTIATING:
      mChannel->Cancel(NS_ERROR_ABORT);
      mChannel = nullptr;
      ChangeState(WebTransportSessionProxyState::DONE);
      break;
    case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
      mChannel->Cancel(NS_ERROR_ABORT);
      mChannel = nullptr;
      ChangeState(WebTransportSessionProxyState::SESSION_CLOSE_PENDING);
      CloseSessionInternal();
      break;
    case WebTransportSessionProxyState::ACTIVE:
      ChangeState(WebTransportSessionProxyState::SESSION_CLOSE_PENDING);
      CloseSessionInternal();
      break;
    case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
      ChangeState(WebTransportSessionProxyState::DONE);
      break;
    case SESSION_CLOSE_PENDING:
      break;
  }
  return NS_OK;
}

void WebTransportSessionProxy::CloseSessionInternal() {
  if (!OnSocketThread()) {
    mMutex.AssertCurrentThreadOwns();
    RefPtr<WebTransportSessionProxy> self(this);
    Unused << gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::CallCloseWebTransportSession",
        [self{std::move(self)}]() { self->CloseSessionInternal(); }));
    return;
  }

  RefPtr<Http3WebTransportSession> wt;
  uint32_t closeStatus = 0;
  nsCString reason;
  {
    MutexAutoLock lock(mMutex);
    if (mState == WebTransportSessionProxyState::SESSION_CLOSE_PENDING) {
      MOZ_ASSERT(mWebTransportSession);
      wt = mWebTransportSession;
      mWebTransportSession = nullptr;
      closeStatus = mCloseStatus;
      reason = mReason;
      ChangeState(WebTransportSessionProxyState::DONE);
    } else {
      MOZ_ASSERT(mState == WebTransportSessionProxyState::DONE);
    }
  }
  if (wt) {
    wt->CloseSession(closeStatus, reason);
  }
}

class WebTransportStreamCallbackWrapper final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebTransportStreamCallbackWrapper)

  explicit WebTransportStreamCallbackWrapper(
      nsIWebTransportStreamCallback* aCallback, bool aBidi)
      : mCallback(aCallback), mTarget(GetCurrentEventTarget()), mBidi(aBidi) {}

  void CallOnError(nsresult aError) {
    if (!mTarget->IsOnCurrentThread()) {
      RefPtr<WebTransportStreamCallbackWrapper> self(this);
      Unused << mTarget->Dispatch(NS_NewRunnableFunction(
          "WebTransportStreamCallbackWrapper::CallOnError",
          [self{std::move(self)}, error{aError}]() {
            self->CallOnError(error);
          }));
      return;
    }

    LOG(("WebTransportStreamCallbackWrapper::OnError aError=0x%" PRIx32,
         static_cast<uint32_t>(aError)));
    Unused << mCallback->OnError(nsIWebTransport::INVALID_STATE_ERROR);
  }

  void CallOnStreamReady(WebTransportStreamProxy* aStream) {
    if (!mTarget->IsOnCurrentThread()) {
      RefPtr<WebTransportStreamCallbackWrapper> self(this);
      RefPtr<WebTransportStreamProxy> stream = aStream;
      Unused << mTarget->Dispatch(NS_NewRunnableFunction(
          "WebTransportStreamCallbackWrapper::CallOnStreamReady",
          [self{std::move(self)}, stream{std::move(stream)}]() {
            self->CallOnStreamReady(stream);
          }));
      return;
    }

    if (mBidi) {
      Unused << mCallback->OnBidirectionalStreamReady(aStream);
      return;
    }

    Unused << mCallback->OnUnidirectionalStreamReady(aStream);
  }

 private:
  virtual ~WebTransportStreamCallbackWrapper() {
    NS_ProxyRelease(
        "WebTransportStreamCallbackWrapper::~WebTransportStreamCallbackWrapper",
        mTarget, mCallback.forget());
  }

  nsCOMPtr<nsIWebTransportStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mTarget;
  bool mBidi = false;
};

void WebTransportSessionProxy::CreateStreamInternal(
    WebTransportStreamCallbackWrapper* aCallback, bool aBidi) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportSessionProxy> self(this);
    RefPtr<WebTransportStreamCallbackWrapper> wrapper(aCallback);
    Unused << gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::CreateStreamInternal",
        [self{std::move(self)}, wrapper{std::move(wrapper)}, bidi(aBidi)]() {
          self->CreateStreamInternal(wrapper, bidi);
        }));
    return;
  }

  RefPtr<WebTransportStreamCallbackWrapper> wrapper(aCallback);
  auto callback =
      [wrapper{std::move(wrapper)}](
          Result<RefPtr<Http3WebTransportStream>, nsresult>&& aResult) {
        if (aResult.isErr()) {
          wrapper->CallOnError(aResult.unwrapErr());
          return;
        }

        RefPtr<Http3WebTransportStream> stream = aResult.unwrap();
        RefPtr<WebTransportStreamProxy> streamProxy =
            new WebTransportStreamProxy(stream);
        wrapper->CallOnStreamReady(streamProxy);
      };

  RefPtr<Http3WebTransportSession> session;
  {
    MutexAutoLock lock(mMutex);
    session = mWebTransportSession;
  }

  if (!session) {
    MOZ_ASSERT(false, "This should not happen");
    callback(Err(NS_ERROR_UNEXPECTED));
    return;
  }

  if (aBidi) {
    session->CreateOutgoingBidirectionalStream(std::move(callback));
  } else {
    session->CreateOutgoingUnidirectionalStream(std::move(callback));
  }
}

NS_IMETHODIMP
WebTransportSessionProxy::CreateOutgoingUnidirectionalStream(
    nsIWebTransportStreamCallback* callback) {
  if (!callback) {
    return NS_ERROR_INVALID_ARG;
  }

  {
    MutexAutoLock lock(mMutex);
    if (mState != WebTransportSessionProxyState::ACTIVE ||
        !mWebTransportSession) {
      nsCOMPtr<nsIWebTransportStreamCallback> cb(callback);
      NS_DispatchToCurrentThread(NS_NewRunnableFunction(
          "WebTransportSessionProxy::CreateOutgoingUnidirectionalStream",
          [cb{std::move(cb)}]() {
            cb->OnError(nsIWebTransport::INVALID_STATE_ERROR);
          }));
      return NS_OK;
    }
  }

  RefPtr<WebTransportStreamCallbackWrapper> wrapper =
      new WebTransportStreamCallbackWrapper(callback, false);
  CreateStreamInternal(wrapper, false);
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::CreateOutgoingBidirectionalStream(
    nsIWebTransportStreamCallback* callback) {
  if (!callback) {
    return NS_ERROR_INVALID_ARG;
  }

  {
    MutexAutoLock lock(mMutex);
    if (mState != WebTransportSessionProxyState::ACTIVE ||
        !mWebTransportSession) {
      nsCOMPtr<nsIWebTransportStreamCallback> cb(callback);
      NS_DispatchToCurrentThread(NS_NewRunnableFunction(
          "WebTransportSessionProxy::CreateOutgoingBidirectionalStream",
          [cb{std::move(cb)}]() {
            cb->OnError(nsIWebTransport::INVALID_STATE_ERROR);
          }));
      return NS_OK;
    }
  }

  RefPtr<WebTransportStreamCallbackWrapper> wrapper =
      new WebTransportStreamCallbackWrapper(callback, true);
  CreateStreamInternal(wrapper, true);
  return NS_OK;
}

void WebTransportSessionProxy::SendDatagramInternal(
    const RefPtr<Http3WebTransportSession>& aSession, nsTArray<uint8_t>&& aData,
    uint64_t aTrackingId) {
  MOZ_ASSERT(OnSocketThread());

  aSession->SendDatagram(std::move(aData), aTrackingId);
}

NS_IMETHODIMP
WebTransportSessionProxy::SendDatagram(const nsTArray<uint8_t>& aData,
                                       uint64_t aTrackingId) {
  RefPtr<Http3WebTransportSession> session;
  {
    MutexAutoLock lock(mMutex);
    if (mState != WebTransportSessionProxyState::ACTIVE ||
        !mWebTransportSession) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    session = mWebTransportSession;
  }

  nsTArray<uint8_t> copied;
  copied.Assign(aData);
  if (!OnSocketThread()) {
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::SendDatagramInternal",
        [self = RefPtr{this}, session{std::move(session)},
         data{std::move(copied)}, trackingId(aTrackingId)]() mutable {
          self->SendDatagramInternal(session, std::move(data), trackingId);
        }));
  }

  SendDatagramInternal(session, std::move(copied), aTrackingId);
  return NS_OK;
}

void WebTransportSessionProxy::GetMaxDatagramSizeInternal(
    const RefPtr<Http3WebTransportSession>& aSession) {
  MOZ_ASSERT(OnSocketThread());

  aSession->GetMaxDatagramSize();
}

NS_IMETHODIMP
WebTransportSessionProxy::GetMaxDatagramSize() {
  RefPtr<Http3WebTransportSession> session;
  {
    MutexAutoLock lock(mMutex);
    if (mState != WebTransportSessionProxyState::ACTIVE ||
        !mWebTransportSession) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    session = mWebTransportSession;
  }

  if (!OnSocketThread()) {
    return gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::GetMaxDatagramSizeInternal",
        [self = RefPtr{this}, session{std::move(session)}]() {
          self->GetMaxDatagramSizeInternal(session);
        }));
  }

  GetMaxDatagramSizeInternal(session);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebTransportSessionProxy::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebTransportSessionProxy::OnStartRequest(nsIRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("WebTransportSessionProxy::OnStartRequest\n"));
  nsCOMPtr<WebTransportSessionEventListener> listener;
  nsAutoCString reason;
  uint32_t closeStatus = 0;
  {
    MutexAutoLock lock(mMutex);
    switch (mState) {
      case WebTransportSessionProxyState::INIT:
      case WebTransportSessionProxyState::DONE:
      case WebTransportSessionProxyState::ACTIVE:
      case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
        MOZ_ASSERT(false, "OnStartRequest cannot be called in this state.");
        break;
      case WebTransportSessionProxyState::NEGOTIATING:
      case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
        listener = mListener;
        mListener = nullptr;
        mChannel = nullptr;
        reason = mReason;
        closeStatus = mCloseStatus;
        ChangeState(WebTransportSessionProxyState::DONE);
        break;
      case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED: {
        uint32_t status;

        nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
        if (!httpChannel ||
            NS_FAILED(httpChannel->GetResponseStatus(&status)) ||
            status != 200) {
          listener = mListener;
          mListener = nullptr;
          mChannel = nullptr;
          mReason = ""_ns;
          reason = ""_ns;
          mCloseStatus =
              0;  // TODO: find a better error. Currently error code 0 is used
          ChangeState(WebTransportSessionProxyState::SESSION_CLOSE_PENDING);
          CloseSessionInternal();  // TODO: find a better error. Currently error
                                   // code 0 is used.
        }
        // The success cases will be handled in OnStopRequest.
      } break;
    }
  }
  if (listener) {
    listener->OnSessionClosed(closeStatus, reason);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::OnDataAvailable(nsIRequest* aRequest,
                                          nsIInputStream* aStream,
                                          uint64_t aOffset, uint32_t aCount) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(
      false, "WebTransportSessionProxy::OnDataAvailable should not be called");
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::OnStopRequest(nsIRequest* aRequest,
                                        nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  mChannel = nullptr;
  nsCOMPtr<WebTransportSessionEventListener> listener;
  nsAutoCString reason;
  uint32_t closeStatus = 0;
  uint64_t sessionId;
  bool succeeded = false;
  nsTArray<std::function<void()>> pendingEvents;
  {
    MutexAutoLock lock(mMutex);
    switch (mState) {
      case WebTransportSessionProxyState::INIT:
      case WebTransportSessionProxyState::ACTIVE:
      case WebTransportSessionProxyState::NEGOTIATING:
        MOZ_ASSERT(false, "OnStotRequest cannot be called in this state.");
        break;
      case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
        reason = mReason;
        closeStatus = mCloseStatus;
        listener = mListener;
        mListener = nullptr;
        ChangeState(WebTransportSessionProxyState::DONE);
        break;
      case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
        if (NS_FAILED(aStatus)) {
          listener = mListener;
          mListener = nullptr;
          mReason = ""_ns;
          reason = ""_ns;
          mCloseStatus = 0;
          ChangeState(WebTransportSessionProxyState::SESSION_CLOSE_PENDING);
          CloseSessionInternal();  // TODO: find a better error. Currently error
                                   // code 0 is used.
        } else {
          succeeded = true;
          sessionId = mSessionId;
          listener = mListener;
          ChangeState(WebTransportSessionProxyState::ACTIVE);
          pendingEvents = std::move(mPendingEvents);
        }
        break;
      case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
      case WebTransportSessionProxyState::DONE:
        break;
    }
  }
  if (listener) {
    if (succeeded) {
      listener->OnSessionReady(sessionId);
      if (!pendingEvents.IsEmpty()) {
        for (const auto& event : pendingEvents) {
          event();
        }
      }
    } else {
      listener->OnSessionClosed(closeStatus,
                                reason);  // TODO: find a better error.
                                          // Currently error code 0 is used.
    }
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebTransportSessionProxy::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebTransportSessionProxy::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* callback) {
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  if (NS_FAILED(rv)) {
    callback->OnRedirectVerifyCallback(rv);
    return NS_OK;
  }

  // abort the request if redirecting to insecure context
  if (!newURI->SchemeIs("https")) {
    callback->OnRedirectVerifyCallback(NS_ERROR_ABORT);
    return NS_OK;
  }

  // Assign to mChannel after we get notification about success of the
  // redirect in OnRedirectResult.
  mRedirectChannel = aNewChannel;

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebTransportSessionProxy::nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebTransportSessionProxy::OnRedirectResult(nsresult aStatus) {
  if (NS_SUCCEEDED(aStatus) && mRedirectChannel) {
    mChannel = mRedirectChannel;
  }

  mRedirectChannel = nullptr;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebTransportSessionProxy::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebTransportSessionProxy::GetInterface(const nsIID& aIID, void** aResult) {
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIChannelEventSink*>(this);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIRedirectResultListener))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIRedirectResultListener*>(this);
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// WebTransportSessionProxy::WebTransportSessionEventListener
//-----------------------------------------------------------------------------

// This function is called when the Http3WebTransportSession is ready. After
// this call WebTransportSessionProxy is responsible for the
// Http3WebTransportSession, i.e. it is responsible for closing it.
// The listener of the WebTransportSessionProxy will be informed during
// OnStopRequest call.
NS_IMETHODIMP
WebTransportSessionProxy::OnSessionReadyInternal(
    Http3WebTransportSession* aSession) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("WebTransportSessionProxy::OnSessionReadyInternal"));
  MutexAutoLock lock(mMutex);
  switch (mState) {
    case WebTransportSessionProxyState::INIT:
    case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
    case WebTransportSessionProxyState::ACTIVE:
    case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
    case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
      MOZ_ASSERT(false,
                 "OnSessionReadyInternal cannot be called in this state.");
      return NS_ERROR_ABORT;
    case WebTransportSessionProxyState::NEGOTIATING:
      mWebTransportSession = aSession;
      mSessionId = aSession->StreamId();
      ChangeState(WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED);
      break;
    case WebTransportSessionProxyState::DONE:
      // The session has been canceled. We do not need to set
      // mWebTransportSession.
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::OnIncomingStreamAvailableInternal(
    Http3WebTransportStream* aStream) {
  if (!NS_IsMainThread()) {
    RefPtr<WebTransportSessionProxy> self(this);
    RefPtr<Http3WebTransportStream> stream = aStream;
    Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(
        "WebTransportSessionProxy::OnIncomingStreamAvailableInternal",
        [self{std::move(self)}, stream{std::move(stream)}]() {
          self->OnIncomingStreamAvailableInternal(stream);
        }));
    return NS_OK;
  }

  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    LOG(
        ("WebTransportSessionProxy::OnIncomingStreamAvailableInternal %p "
         "mState=%d mListener=%p",
         this, mState, mListener.get()));
    switch (mState) {
      case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
        // OnSessionReady is not called yet, so we need to wait.
        mPendingEvents.AppendElement(
            [self = RefPtr{this}, stream = RefPtr{aStream}]() {
              self->OnIncomingStreamAvailableInternal(stream);
            });
        break;
      case WebTransportSessionProxyState::ACTIVE:
        listener = mListener;
        break;
      default:
        return NS_ERROR_ABORT;
    }
  }

  if (!listener) {
    return NS_OK;
  }

  RefPtr<WebTransportStreamProxy> streamProxy =
      new WebTransportStreamProxy(aStream);
  if (aStream->StreamType() == WebTransportStreamType::BiDi) {
    Unused << listener->OnIncomingBidirectionalStreamAvailable(streamProxy);
  } else {
    Unused << listener->OnIncomingUnidirectionalStreamAvailable(streamProxy);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::OnIncomingBidirectionalStreamAvailable(
    nsIWebTransportBidirectionalStream* aStream) {
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::OnIncomingUnidirectionalStreamAvailable(
    nsIWebTransportReceiveStream* aStream) {
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::OnSessionReady(uint64_t ready) {
  MOZ_ASSERT(false, "Should not b called");
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::OnSessionClosed(uint32_t status,
                                          const nsACString& reason) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("WebTransportSessionProxy::OnSessionClosed"));
  MutexAutoLock lock(mMutex);
  switch (mState) {
    case WebTransportSessionProxyState::INIT:
    case WebTransportSessionProxyState::NEGOTIATING:
    case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
      MOZ_ASSERT(false, "OnSessionClosed cannot be called in this state.");
      return NS_ERROR_ABORT;
    case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
    case WebTransportSessionProxyState::ACTIVE: {
      mCloseStatus = status;
      mReason = reason;
      mWebTransportSession = nullptr;
      ChangeState(WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING);
      RefPtr<WebTransportSessionProxy> self(this);
      Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(
          "WebTransportSessionProxy::CallOnSessionClose",
          [self{std::move(self)}]() { self->CallOnSessionClosed(); }));
    } break;
    case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
      ChangeState(WebTransportSessionProxyState::DONE);
      break;
    case WebTransportSessionProxyState::DONE:
      // The session has been canceled. We do not need to set
      // mWebTransportSession.
      break;
  }
  return NS_OK;
}

void WebTransportSessionProxy::CallOnSessionClosed() {
  MOZ_ASSERT(NS_IsMainThread(), "not on socket thread");
  nsCOMPtr<WebTransportSessionEventListener> listener;
  nsAutoCString reason;
  uint32_t closeStatus = 0;
  {
    MutexAutoLock lock(mMutex);
    switch (mState) {
      case WebTransportSessionProxyState::INIT:
      case WebTransportSessionProxyState::NEGOTIATING:
      case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
      case WebTransportSessionProxyState::ACTIVE:
      case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
        MOZ_ASSERT(false,
                   "CallOnSessionClosed cannot be called in this state.");
        break;
      case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
        listener = mListener;
        mListener = nullptr;
        reason = mReason;
        closeStatus = mCloseStatus;
        ChangeState(WebTransportSessionProxyState::DONE);
        break;
      case WebTransportSessionProxyState::DONE:
        break;
    }
  }
  if (listener) {
    listener->OnSessionClosed(closeStatus, reason);
  }
}

void WebTransportSessionProxy::ChangeState(
    WebTransportSessionProxyState newState) {
  mMutex.AssertCurrentThreadOwns();
  LOG(("WebTransportSessionProxy::ChangeState %d -> %d [this=%p]", mState,
       newState, this));
  switch (newState) {
    case WebTransportSessionProxyState::INIT:
      MOZ_ASSERT(false, "Cannot change into INIT sate.");
      break;
    case WebTransportSessionProxyState::NEGOTIATING:
      MOZ_ASSERT(mState == WebTransportSessionProxyState::INIT,
                 "Only from INIT can be change into NEGOTIATING");
      MOZ_ASSERT(mChannel);
      MOZ_ASSERT(mListener);
      break;
    case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
      MOZ_ASSERT(
          mState == WebTransportSessionProxyState::NEGOTIATING,
          "Only from NEGOTIATING can be change into NEGOTIATING_SUCCEEDED");
      MOZ_ASSERT(mChannel);
      MOZ_ASSERT(mWebTransportSession);
      MOZ_ASSERT(mListener);
      break;
    case WebTransportSessionProxyState::ACTIVE:
      MOZ_ASSERT(mState == WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED,
                 "Only from NEGOTIATING_SUCCEEDED can be change into ACTIVE");
      MOZ_ASSERT(!mChannel);
      MOZ_ASSERT(mWebTransportSession);
      MOZ_ASSERT(mListener);
      break;
    case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
      MOZ_ASSERT(
          (mState == WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED) ||
              (mState == WebTransportSessionProxyState::ACTIVE),
          "Only from NEGOTIATING_SUCCEEDED and ACTIVE can be change into"
          " SESSION_CLOSE_PENDING");
      MOZ_ASSERT(!mChannel);
      MOZ_ASSERT(mWebTransportSession);
      MOZ_ASSERT(!mListener);
      break;
    case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
      MOZ_ASSERT(
          (mState == WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED) ||
              (mState == WebTransportSessionProxyState::ACTIVE),
          "Only from NEGOTIATING_SUCCEEDED and ACTIVE can be change into"
          " CLOSE_CALLBACK_PENDING");
      MOZ_ASSERT(!mWebTransportSession);
      MOZ_ASSERT(mListener);
      break;
    case WebTransportSessionProxyState::DONE:
      MOZ_ASSERT(
          (mState == WebTransportSessionProxyState::NEGOTIATING) ||
              (mState ==
               WebTransportSessionProxyState::SESSION_CLOSE_PENDING) ||
              (mState == WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING),
          "Only from NEGOTIATING, SESSION_CLOSE_PENDING and "
          "CLOSE_CALLBACK_PENDING can be change into DONE");
      MOZ_ASSERT(!mChannel);
      MOZ_ASSERT(!mWebTransportSession);
      MOZ_ASSERT(!mListener);
      break;
  }
  mState = newState;
}

void WebTransportSessionProxy::NotifyDatagramReceived(
    nsTArray<uint8_t>&& aData) {
  // TODO: this should be on the target thread, but the target thread is main
  // thread for now.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    if (mState != WebTransportSessionProxyState::ACTIVE || !mListener) {
      return;
    }
    listener = mListener;
  }

  listener->OnDatagramReceived(aData);
}

NS_IMETHODIMP WebTransportSessionProxy::OnDatagramReceivedInternal(
    nsTArray<uint8_t>&& aData) {
  MOZ_ASSERT(OnSocketThread());

  if (!NS_IsMainThread()) {
    return NS_DispatchToMainThread(NS_NewRunnableFunction(
        "WebTransportSessionProxy::OnDatagramReceived",
        [self = RefPtr{this}, data{std::move(aData)}]() mutable {
          self->NotifyDatagramReceived(std::move(data));
        }));
  }

  NotifyDatagramReceived(std::move(aData));
  return NS_OK;
}

NS_IMETHODIMP WebTransportSessionProxy::OnDatagramReceived(
    const nsTArray<uint8_t>& aData) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void WebTransportSessionProxy::OnMaxDatagramSizeInternal(uint64_t aSize) {
  // TODO: this should be on the target thread, but the target thread is main
  // thread for now.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    if (mState != WebTransportSessionProxyState::ACTIVE || !mListener) {
      return;
    }
    listener = mListener;
  }

  listener->OnMaxDatagramSize(aSize);
}

NS_IMETHODIMP WebTransportSessionProxy::OnMaxDatagramSize(uint64_t aSize) {
  MOZ_ASSERT(OnSocketThread());

  if (!NS_IsMainThread()) {
    return NS_DispatchToMainThread(
        NS_NewRunnableFunction("WebTransportSessionProxy::OnMaxDatagramSize",
                               [self = RefPtr{this}, size(aSize)] {
                                 self->OnMaxDatagramSizeInternal(size);
                               }));
  }

  OnMaxDatagramSizeInternal(aSize);
  return NS_OK;
}

void WebTransportSessionProxy::OnOutgoingDatagramOutComeInternal(
    uint64_t aId, WebTransportSessionEventListener::DatagramOutcome aOutCome) {
  // TODO: this should be on the target thread, but the target thread is main
  // thread for now.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    if (mState != WebTransportSessionProxyState::ACTIVE || !mListener) {
      return;
    }
    listener = mListener;
  }

  listener->OnOutgoingDatagramOutCome(aId, aOutCome);
}

NS_IMETHODIMP
WebTransportSessionProxy::OnOutgoingDatagramOutCome(
    uint64_t aId, WebTransportSessionEventListener::DatagramOutcome aOutCome) {
  MOZ_ASSERT(OnSocketThread());

  if (!NS_IsMainThread()) {
    return NS_DispatchToMainThread(NS_NewRunnableFunction(
        "WebTransportSessionProxy::OnOutgoingDatagramOutCome",
        [self = RefPtr{this}, id(aId), outcome(aOutCome)] {
          self->OnOutgoingDatagramOutComeInternal(id, outcome);
        }));
  }

  OnOutgoingDatagramOutComeInternal(aId, aOutCome);
  return NS_OK;
}

}  // namespace mozilla::net
