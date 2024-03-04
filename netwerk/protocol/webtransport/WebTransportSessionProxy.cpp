/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportLog.h"
#include "Http3WebTransportSession.h"
#include "Http3WebTransportStream.h"
#include "ScopedNSSTypes.h"
#include "WebTransportSessionProxy.h"
#include "WebTransportStreamProxy.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIRequest.h"
#include "nsITransportSecurityInfo.h"
#include "nsIX509Cert.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsSocketTransportService2.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_network.h"

namespace mozilla::net {

LazyLogModule webTransportLog("nsWebTransport");

NS_IMPL_ISUPPORTS(WebTransportSessionProxy, WebTransportSessionEventListener,
                  nsIWebTransport, nsIRedirectResultListener, nsIStreamListener,
                  nsIChannelEventSink, nsIInterfaceRequestor);

WebTransportSessionProxy::WebTransportSessionProxy()
    : mMutex("WebTransportSessionProxy::mMutex"),
      mTarget(GetMainThreadSerialEventTarget()) {
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
    nsIURI* aURI,
    const nsTArray<RefPtr<nsIWebTransportHash>>& aServerCertHashes,
    nsIPrincipal* aPrincipal, uint32_t aSecurityFlags,
    WebTransportSessionEventListener* aListener) {
  return AsyncConnectWithClient(aURI, std::move(aServerCertHashes), aPrincipal,
                                aSecurityFlags, aListener,
                                Maybe<dom::ClientInfo>());
}

nsresult WebTransportSessionProxy::AsyncConnectWithClient(
    nsIURI* aURI,
    const nsTArray<RefPtr<nsIWebTransportHash>>& aServerCertHashes,
    nsIPrincipal* aPrincipal, uint32_t aSecurityFlags,
    WebTransportSessionEventListener* aListener,
    const Maybe<dom::ClientInfo>& aClientInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("WebTransportSessionProxy::AsyncConnect"));
  {
    MutexAutoLock lock(mMutex);
    mListener = aListener;
  }
  auto cleanup = MakeScopeExit([self = RefPtr<WebTransportSessionProxy>(this)] {
    MutexAutoLock lock(self->mMutex);
    self->mListener->OnSessionClosed(false, 0,
                                     ""_ns);  // TODO: find a better error.
    self->mChannel = nullptr;
    self->mListener = nullptr;
    self->ChangeState(WebTransportSessionProxyState::DONE);
  });

  nsSecurityFlags flags = nsILoadInfo::SEC_COOKIES_OMIT | aSecurityFlags;
  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL |
                          nsIRequest::LOAD_BYPASS_CACHE |
                          nsIRequest::INHIBIT_CACHING;
  nsresult rv = NS_ERROR_FAILURE;

  if (aClientInfo.isSome()) {
    rv = NS_NewChannel(getter_AddRefs(mChannel), aURI, aPrincipal,
                       aClientInfo.ref(), Maybe<dom::ServiceWorkerDescriptor>(),
                       flags, nsContentPolicyType::TYPE_WEB_TRANSPORT,
                       /* aCookieJarSettings */ nullptr,
                       /* aPerformanceStorage */ nullptr,
                       /* aLoadGroup */ nullptr,
                       /* aCallbacks */ this, loadFlags);
  } else {
    rv = NS_NewChannel(getter_AddRefs(mChannel), aURI, aPrincipal, flags,
                       nsContentPolicyType::TYPE_WEB_TRANSPORT,
                       /* aCookieJarSettings */ nullptr,
                       /* aPerformanceStorage */ nullptr,
                       /* aLoadGroup */ nullptr,
                       /* aCallbacks */ this, loadFlags);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // configure HTTP specific stuff
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    mChannel = nullptr;
    return NS_ERROR_ABORT;
  }

  if (!aServerCertHashes.IsEmpty()) {
    mServerCertHashes.AppendElements(aServerCertHashes);
  }

  {
    MutexAutoLock lock(mMutex);
    ChangeState(WebTransportSessionProxyState::NEGOTIATING);
  }

  // https://www.ietf.org/archive/id/draft-ietf-webtrans-http3-04.html#section-6
  rv = httpChannel->SetRequestHeader("Sec-Webtransport-Http3-Draft02"_ns,
                                     "1"_ns, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // To establish a WebTransport session with an origin origin, follow
  // [WEB-TRANSPORT-HTTP3] section 3.3, with using origin, serialized and
  // isomorphic encoded, as the `Origin` header of the request.
  // https://www.w3.org/TR/webtransport/#protocol-concepts
  nsAutoCString serializedOrigin;
  if (NS_FAILED(
          aPrincipal->GetWebExposedOriginSerialization(serializedOrigin))) {
    // origin/URI will be missing for system principals
    // assign null origin
    serializedOrigin = "null"_ns;
  }

  rv = httpChannel->SetRequestHeader("Origin"_ns, serializedOrigin, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIHttpChannelInternal> internalChannel =
      do_QueryInterface(mChannel);
  if (!internalChannel) {
    mChannel = nullptr;
    return NS_ERROR_ABORT;
  }
  Unused << internalChannel->SetWebTransportSessionEventListener(this);

  rv = mChannel->AsyncOpen(this);
  if (NS_SUCCEEDED(rv)) {
    cleanup.release();
  }
  return rv;
}

NS_IMETHODIMP
WebTransportSessionProxy::RetargetTo(nsIEventTarget* aTarget) {
  if (!aTarget) {
    return NS_ERROR_INVALID_ARG;
  }

  {
    MutexAutoLock lock(mMutex);
    LOG(("WebTransportSessionProxy::RetargetTo mState=%d", mState));
    // RetargetTo should be only called after the session is ready.
    if (mState != WebTransportSessionProxyState::ACTIVE) {
      return NS_ERROR_UNEXPECTED;
    }

    mTarget = aTarget;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::GetStats() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
WebTransportSessionProxy::CloseSession(uint32_t status,
                                       const nsACString& reason) {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mTarget->IsOnCurrentThread());
  mCloseStatus = status;
  mReason = reason;
  mListener = nullptr;
  mPendingEvents.Clear();
  mServerCertHashes.Clear();
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

void WebTransportSessionProxy::CloseSessionInternalLocked() {
  MutexAutoLock lock(mMutex);
  CloseSessionInternal();
}

void WebTransportSessionProxy::CloseSessionInternal() {
  if (!OnSocketThread()) {
    mMutex.AssertCurrentThreadOwns();
    RefPtr<WebTransportSessionProxy> self(this);
    Unused << gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::CallCloseWebTransportSession",
        [self{std::move(self)}]() { self->CloseSessionInternalLocked(); }));
    return;
  }

  mMutex.AssertCurrentThreadOwns();

  RefPtr<Http3WebTransportSession> wt;
  uint32_t closeStatus = 0;
  nsCString reason;

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

  if (wt) {
    MutexAutoUnlock unlock(mMutex);
    wt->CloseSession(closeStatus, reason);
  }
}

class WebTransportStreamCallbackWrapper final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebTransportStreamCallbackWrapper)

  explicit WebTransportStreamCallbackWrapper(
      nsIWebTransportStreamCallback* aCallback, bool aBidi)
      : mCallback(aCallback),
        mTarget(GetCurrentSerialEventTarget()),
        mBidi(aBidi) {}

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
    nsIWebTransportStreamCallback* callback, bool aBidi) {
  mMutex.AssertCurrentThreadOwns();
  LOG(
      ("WebTransportSessionProxy::CreateStreamInternal %p "
       "mState=%d, bidi=%d",
       this, mState, aBidi));
  switch (mState) {
    case WebTransportSessionProxyState::INIT:
    case WebTransportSessionProxyState::NEGOTIATING:
    case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
    case WebTransportSessionProxyState::ACTIVE: {
      RefPtr<WebTransportStreamCallbackWrapper> wrapper =
          new WebTransportStreamCallbackWrapper(callback, aBidi);
      if (mState == WebTransportSessionProxyState::ACTIVE &&
          mWebTransportSession) {
        DoCreateStream(wrapper, mWebTransportSession, aBidi);
      } else {
        LOG(
            ("WebTransportSessionProxy::CreateStreamInternal %p "
             " queue create stream event",
             this));
        auto task = [self = RefPtr{this}, wrapper{std::move(wrapper)},
                     bidi(aBidi)](nsresult aStatus) {
          if (NS_FAILED(aStatus)) {
            wrapper->CallOnError(aStatus);
            return;
          }

          self->DoCreateStream(wrapper, nullptr, bidi);
        };
        // TODO: we should do this properly in bug 1830362.
        mPendingCreateStreamEvents.AppendElement(std::move(task));
      }
    } break;
    case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
    case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
    case WebTransportSessionProxyState::DONE: {
      nsCOMPtr<nsIWebTransportStreamCallback> cb(callback);
      NS_DispatchToCurrentThread(NS_NewRunnableFunction(
          "WebTransportSessionProxy::CreateStreamInternal",
          [cb{std::move(cb)}]() {
            cb->OnError(nsIWebTransport::INVALID_STATE_ERROR);
          }));
    } break;
  }
}

void WebTransportSessionProxy::DoCreateStream(
    WebTransportStreamCallbackWrapper* aCallback,
    Http3WebTransportSession* aSession, bool aBidi) {
  if (!OnSocketThread()) {
    RefPtr<WebTransportSessionProxy> self(this);
    RefPtr<WebTransportStreamCallbackWrapper> wrapper(aCallback);
    Unused << gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::DoCreateStream",
        [self{std::move(self)}, wrapper{std::move(wrapper)}, bidi(aBidi)]() {
          self->DoCreateStream(wrapper, nullptr, bidi);
        }));
    return;
  }

  LOG(("WebTransportSessionProxy::DoCreateStream %p bidi=%d", this, aBidi));

  RefPtr<Http3WebTransportSession> session = aSession;
  // Having no session here means that this is called by dispatching tasks.
  // The mState may be already changed, so we need to check it again.
  if (!aSession) {
    MutexAutoLock lock(mMutex);
    switch (mState) {
      case WebTransportSessionProxyState::INIT:
      case WebTransportSessionProxyState::NEGOTIATING:
      case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
        MOZ_ASSERT(false, "DoCreateStream called with invalid state");
        aCallback->CallOnError(NS_ERROR_UNEXPECTED);
        return;
      case WebTransportSessionProxyState::ACTIVE: {
        session = mWebTransportSession;
      } break;
      case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
      case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
      case WebTransportSessionProxyState::DONE:
        // Session is going to be closed.
        aCallback->CallOnError(NS_ERROR_NOT_AVAILABLE);
        return;
    }
  }

  if (!session) {
    MOZ_ASSERT_UNREACHABLE("This should not happen");
    aCallback->CallOnError(NS_ERROR_UNEXPECTED);
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

  MutexAutoLock lock(mMutex);
  CreateStreamInternal(callback, false);
  return NS_OK;
}

NS_IMETHODIMP
WebTransportSessionProxy::CreateOutgoingBidirectionalStream(
    nsIWebTransportStreamCallback* callback) {
  if (!callback) {
    return NS_ERROR_INVALID_ARG;
  }

  MutexAutoLock lock(mMutex);
  CreateStreamInternal(callback, true);
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
            !(status >= 200 && status < 300) ||
            !CheckServerCertificateIfNeeded()) {
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
    listener->OnSessionClosed(false, closeStatus, reason);
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
  nsTArray<std::function<void(nsresult)>> pendingCreateStreamEvents;
  {
    MutexAutoLock lock(mMutex);
    switch (mState) {
      case WebTransportSessionProxyState::INIT:
      case WebTransportSessionProxyState::ACTIVE:
      case WebTransportSessionProxyState::NEGOTIATING:
        MOZ_ASSERT(false, "OnStopRequest cannot be called in this state.");
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
        }
        break;
      case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
      case WebTransportSessionProxyState::DONE:
        break;
    }
    pendingEvents = std::move(mPendingEvents);
    pendingCreateStreamEvents = std::move(mPendingCreateStreamEvents);
    if (!pendingCreateStreamEvents.IsEmpty()) {
      if (NS_SUCCEEDED(aStatus) &&
          (mState == WebTransportSessionProxyState::DONE ||
           mState == WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING ||
           mState == WebTransportSessionProxyState::SESSION_CLOSE_PENDING)) {
        aStatus = NS_ERROR_FAILURE;
      }
    }

    mStopRequestCalled = true;
  }

  if (!pendingCreateStreamEvents.IsEmpty()) {
    Unused << gSocketTransportService->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::DispatchPendingCreateStreamEvents",
        [pendingCreateStreamEvents = std::move(pendingCreateStreamEvents),
         status(aStatus)]() {
          for (const auto& event : pendingCreateStreamEvents) {
            event(status);
          }
        }));
  }  // otherwise let the CreateStreams just go away

  if (listener) {
    if (succeeded) {
      listener->OnSessionReady(sessionId);
      if (!pendingEvents.IsEmpty()) {
        Unused << gSocketTransportService->Dispatch(NS_NewRunnableFunction(
            "WebTransportSessionProxy::DispatchPendingEvents",
            [pendingEvents = std::move(pendingEvents)]() {
              for (const auto& event : pendingEvents) {
                event();
              }
            }));
      }
    } else {
      listener->OnSessionClosed(false, closeStatus,
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
  // Currently implementation we do not reach this part of the code
  // as location headers are not forwarded by the http3 stack to the applicaion.
  // Hence, the channel is aborted due to the location header check in
  // nsHttpChannel::AsyncProcessRedirection This comment must be removed  after
  // the  following neqo bug is resolved
  // https://github.com/mozilla/neqo/issues/1364
  if (!StaticPrefs::network_webtransport_redirect_enabled()) {
    LOG(("Channel Redirects are disabled for WebTransport sessions"));
    return NS_ERROR_ABORT;
  }

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
  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);

    LOG(
        ("WebTransportSessionProxy::OnIncomingStreamAvailableInternal %p "
         "mState=%d "
         "mStopRequestCalled=%d",
         this, mState, mStopRequestCalled));
    // Since OnSessionReady on the listener is called on the main thread,
    // OnIncomingStreamAvailableInternal and OnSessionReady can be racy. If
    // OnStopRequest is not called yet, OnIncomingStreamAvailableInternal needs
    // to wait.
    if (!mStopRequestCalled) {
      mPendingEvents.AppendElement(
          [self = RefPtr{this}, stream = RefPtr{aStream}]() {
            self->OnIncomingStreamAvailableInternal(stream);
          });
      return NS_OK;
    }

    if (!mTarget->IsOnCurrentThread()) {
      RefPtr<WebTransportSessionProxy> self(this);
      RefPtr<Http3WebTransportStream> stream = aStream;
      Unused << mTarget->Dispatch(NS_NewRunnableFunction(
          "WebTransportSessionProxy::OnIncomingStreamAvailableInternal",
          [self{std::move(self)}, stream{std::move(stream)}]() {
            self->OnIncomingStreamAvailableInternal(stream);
          }));
      return NS_OK;
    }

    LOG(
        ("WebTransportSessionProxy::OnIncomingStreamAvailableInternal %p "
         "mState=%d mListener=%p",
         this, mState, mListener.get()));
    if (mState == WebTransportSessionProxyState::ACTIVE) {
      listener = mListener;
    }
  }

  if (!listener) {
    // Session can be already closed.
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
WebTransportSessionProxy::OnSessionClosed(bool aCleanly, uint32_t aStatus,
                                          const nsACString& aReason) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MutexAutoLock lock(mMutex);
  LOG(
      ("WebTransportSessionProxy::OnSessionClosed %p mState=%d "
       "mStopRequestCalled=%d",
       this, mState, mStopRequestCalled));
  // Since OnSessionReady on the listener is called on the main thread,
  // OnSessionClosed and OnSessionReady can be racy. If OnStopRequest is not
  // called yet, OnSessionClosed needs to wait.
  if (!mStopRequestCalled) {
    nsCString closeReason(aReason);
    mPendingEvents.AppendElement([self = RefPtr{this}, status(aStatus),
                                  closeReason(std::move(closeReason)),
                                  cleanly(aCleanly)]() {
      Unused << self->OnSessionClosed(cleanly, status, closeReason);
    });
    return NS_OK;
  }

  switch (mState) {
    case WebTransportSessionProxyState::INIT:
    case WebTransportSessionProxyState::NEGOTIATING:
    case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
      MOZ_ASSERT(false, "OnSessionClosed cannot be called in this state.");
      return NS_ERROR_ABORT;
    case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
    case WebTransportSessionProxyState::ACTIVE: {
      mCleanly = aCleanly;
      mCloseStatus = aStatus;
      mReason = aReason;
      mWebTransportSession = nullptr;
      ChangeState(WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING);
      CallOnSessionClosed();
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

void WebTransportSessionProxy::CallOnSessionClosedLocked() {
  MutexAutoLock lock(mMutex);
  CallOnSessionClosed();
}

void WebTransportSessionProxy::CallOnSessionClosed() {
  mMutex.AssertCurrentThreadOwns();

  if (!mTarget->IsOnCurrentThread()) {
    RefPtr<WebTransportSessionProxy> self(this);
    Unused << mTarget->Dispatch(NS_NewRunnableFunction(
        "WebTransportSessionProxy::CallOnSessionClosed",
        [self{std::move(self)}]() { self->CallOnSessionClosedLocked(); }));
    return;
  }

  MOZ_ASSERT(mTarget->IsOnCurrentThread());
  nsCOMPtr<WebTransportSessionEventListener> listener;
  bool cleanly = false;
  nsAutoCString reason;
  uint32_t closeStatus = 0;

  switch (mState) {
    case WebTransportSessionProxyState::INIT:
    case WebTransportSessionProxyState::NEGOTIATING:
    case WebTransportSessionProxyState::NEGOTIATING_SUCCEEDED:
    case WebTransportSessionProxyState::ACTIVE:
    case WebTransportSessionProxyState::SESSION_CLOSE_PENDING:
      MOZ_ASSERT(false, "CallOnSessionClosed cannot be called in this state.");
      break;
    case WebTransportSessionProxyState::CLOSE_CALLBACK_PENDING:
      listener = mListener;
      mListener = nullptr;
      cleanly = mCleanly;
      reason = mReason;
      closeStatus = mCloseStatus;
      ChangeState(WebTransportSessionProxyState::DONE);
      break;
    case WebTransportSessionProxyState::DONE:
      break;
  }

  if (listener) {
    // Don't invoke the callback under the lock.
    MutexAutoUnlock unlock(mMutex);
    listener->OnSessionClosed(cleanly, closeStatus, reason);
  }
}

bool WebTransportSessionProxy::CheckServerCertificateIfNeeded() {
  if (mServerCertHashes.IsEmpty()) {
    return true;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  MOZ_ASSERT(httpChannel, "Not a http channel ?");
  nsCOMPtr<nsITransportSecurityInfo> tsi;
  httpChannel->GetSecurityInfo(getter_AddRefs(tsi));
  MOZ_ASSERT(tsi,
             "We shouln't reach this code before setting the security info.");
  nsCOMPtr<nsIX509Cert> cert;
  nsresult rv = tsi->GetServerCert(getter_AddRefs(cert));
  if (!cert || NS_WARN_IF(NS_FAILED(rv))) return true;
  nsTArray<uint8_t> certDER;
  if (NS_FAILED(cert->GetRawDER(certDER))) {
    return false;
  }
  // https://w3c.github.io/webtransport/#compute-a-certificate-hash
  nsTArray<uint8_t> certHash;
  if (NS_FAILED(Digest::DigestBuf(SEC_OID_SHA256, certDER.Elements(),
                                  certDER.Length(), certHash)) ||
      certHash.Length() != SHA256_LENGTH) {
    return false;
  }
  auto verifyCertDer = [&certHash](const auto& hash) {
    return certHash.Length() == hash.Length() &&
           memcmp(certHash.Elements(), hash.Elements(), certHash.Length()) == 0;
  };

  // https://w3c.github.io/webtransport/#verify-a-certificate-hash
  for (const auto& hash : mServerCertHashes) {
    nsCString algorithm;
    if (NS_FAILED(hash->GetAlgorithm(algorithm)) || algorithm != "sha-256") {
      continue;
      LOG(("Unexpected non-SHA-256 hash"));
    }

    nsTArray<uint8_t> value;
    if (NS_FAILED(hash->GetValue(value))) {
      continue;
      LOG(("Unexpected corrupted hash"));
    }

    if (verifyCertDer(value)) {
      return true;
    }
  }
  return false;
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
  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mTarget->IsOnCurrentThread());

    if (!mStopRequestCalled) {
      CopyableTArray<uint8_t> copied(aData);
      mPendingEvents.AppendElement(
          [self = RefPtr{this}, data = std::move(copied)]() mutable {
            self->NotifyDatagramReceived(std::move(data));
          });
      return;
    }

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

  {
    MutexAutoLock lock(mMutex);
    if (!mTarget->IsOnCurrentThread()) {
      return mTarget->Dispatch(NS_NewRunnableFunction(
          "WebTransportSessionProxy::OnDatagramReceived",
          [self = RefPtr{this}, data{std::move(aData)}]() mutable {
            self->NotifyDatagramReceived(std::move(data));
          }));
    }
  }

  NotifyDatagramReceived(std::move(aData));
  return NS_OK;
}

NS_IMETHODIMP WebTransportSessionProxy::OnDatagramReceived(
    const nsTArray<uint8_t>& aData) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void WebTransportSessionProxy::OnMaxDatagramSizeInternal(uint64_t aSize) {
  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mTarget->IsOnCurrentThread());

    if (!mStopRequestCalled) {
      mPendingEvents.AppendElement([self = RefPtr{this}, size(aSize)]() {
        self->OnMaxDatagramSizeInternal(size);
      });
      return;
    }

    if (mState != WebTransportSessionProxyState::ACTIVE || !mListener) {
      return;
    }
    listener = mListener;
  }

  listener->OnMaxDatagramSize(aSize);
}

NS_IMETHODIMP WebTransportSessionProxy::OnMaxDatagramSize(uint64_t aSize) {
  MOZ_ASSERT(OnSocketThread());

  {
    MutexAutoLock lock(mMutex);
    if (!mTarget->IsOnCurrentThread()) {
      return mTarget->Dispatch(
          NS_NewRunnableFunction("WebTransportSessionProxy::OnMaxDatagramSize",
                                 [self = RefPtr{this}, size(aSize)] {
                                   self->OnMaxDatagramSizeInternal(size);
                                 }));
    }
  }

  OnMaxDatagramSizeInternal(aSize);
  return NS_OK;
}

void WebTransportSessionProxy::OnOutgoingDatagramOutComeInternal(
    uint64_t aId, WebTransportSessionEventListener::DatagramOutcome aOutCome) {
  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mTarget->IsOnCurrentThread());
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

  {
    MutexAutoLock lock(mMutex);
    if (!mTarget->IsOnCurrentThread()) {
      return mTarget->Dispatch(NS_NewRunnableFunction(
          "WebTransportSessionProxy::OnOutgoingDatagramOutCome",
          [self = RefPtr{this}, id(aId), outcome(aOutCome)] {
            self->OnOutgoingDatagramOutComeInternal(id, outcome);
          }));
    }
  }

  OnOutgoingDatagramOutComeInternal(aId, aOutCome);
  return NS_OK;
}

NS_IMETHODIMP WebTransportSessionProxy::OnStopSending(uint64_t aStreamId,
                                                      nsresult aError) {
  MOZ_ASSERT(OnSocketThread());
  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mTarget->IsOnCurrentThread());

    if (mState != WebTransportSessionProxyState::ACTIVE || !mListener) {
      return NS_OK;
    }
    listener = mListener;
  }

  listener->OnStopSending(aStreamId, aError);
  return NS_OK;
}

NS_IMETHODIMP WebTransportSessionProxy::OnResetReceived(uint64_t aStreamId,
                                                        nsresult aError) {
  MOZ_ASSERT(OnSocketThread());
  nsCOMPtr<WebTransportSessionEventListener> listener;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mTarget->IsOnCurrentThread());

    if (mState != WebTransportSessionProxyState::ACTIVE || !mListener) {
      return NS_OK;
    }
    listener = mListener;
  }

  listener->OnResetReceived(aStreamId, aError);
  return NS_OK;
}

}  // namespace mozilla::net
