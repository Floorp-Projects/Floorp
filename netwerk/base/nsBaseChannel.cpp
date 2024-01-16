/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseChannel.h"
#include "nsContentUtils.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"
#include "nsMimeTypes.h"
#include "nsUnknownDecoder.h"
#include "nsIScriptSecurityManager.h"
#include "nsMimeTypes.h"
#include "nsICancelable.h"
#include "nsIChannelEventSink.h"
#include "nsIStreamConverterService.h"
#include "nsChannelClassifier.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsProxyRelease.h"
#include "nsXULAppAPI.h"
#include "nsContentSecurityManager.h"
#include "LoadInfo.h"
#include "nsServiceManagerUtils.h"
#include "nsRedirectHistoryEntry.h"
#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/BasePrincipal.h"

using namespace mozilla;

// This class is used to suspend a request across a function scope.
class ScopedRequestSuspender {
 public:
  explicit ScopedRequestSuspender(nsIRequest* request) : mRequest(request) {
    if (mRequest && NS_FAILED(mRequest->Suspend())) {
      NS_WARNING("Couldn't suspend pump");
      mRequest = nullptr;
    }
  }
  ~ScopedRequestSuspender() {
    if (mRequest) mRequest->Resume();
  }

 private:
  nsIRequest* mRequest;
};

// Used to suspend data events from mRequest within a function scope.  This is
// usually needed when a function makes callbacks that could process events.
#define SUSPEND_PUMP_FOR_SCOPE() \
  ScopedRequestSuspender pump_suspender__(mRequest)

//-----------------------------------------------------------------------------
// nsBaseChannel

nsBaseChannel::nsBaseChannel() : NeckoTargetHolder(nullptr) {
  mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
}

nsBaseChannel::~nsBaseChannel() {
  NS_ReleaseOnMainThread("nsBaseChannel::mLoadInfo", mLoadInfo.forget());
}

nsresult nsBaseChannel::Redirect(nsIChannel* newChannel, uint32_t redirectFlags,
                                 bool openNewChannel) {
  SUSPEND_PUMP_FOR_SCOPE();

  // Transfer properties

  newChannel->SetLoadGroup(mLoadGroup);
  newChannel->SetNotificationCallbacks(mCallbacks);
  newChannel->SetLoadFlags(mLoadFlags | LOAD_REPLACE);

  // make a copy of the loadinfo, append to the redirectchain
  // and set it on the new channel
  nsSecurityFlags secFlags =
      mLoadInfo->GetSecurityFlags() & ~nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  nsCOMPtr<nsILoadInfo> newLoadInfo =
      static_cast<net::LoadInfo*>(mLoadInfo.get())
          ->CloneWithNewSecFlags(secFlags);

  bool isInternalRedirect =
      (redirectFlags & (nsIChannelEventSink::REDIRECT_INTERNAL |
                        nsIChannelEventSink::REDIRECT_STS_UPGRADE));

  newLoadInfo->AppendRedirectHistoryEntry(this, isInternalRedirect);

  // Ensure the channel's loadInfo's result principal URI so that it's
  // either non-null or updated to the redirect target URI.
  // We must do this because in case the loadInfo's result principal URI
  // is null, it would be taken from OriginalURI of the channel.  But we
  // overwrite it with the whole redirect chain first URI before opening
  // the target channel, hence the information would be lost.
  // If the protocol handler that created the channel wants to use
  // the originalURI of the channel as the principal URI, it has left
  // the result principal URI on the load info null.
  nsCOMPtr<nsIURI> resultPrincipalURI;

  nsCOMPtr<nsILoadInfo> existingLoadInfo = newChannel->LoadInfo();
  if (existingLoadInfo) {
    existingLoadInfo->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI));
  }
  if (!resultPrincipalURI) {
    newChannel->GetOriginalURI(getter_AddRefs(resultPrincipalURI));
  }

  newLoadInfo->SetResultPrincipalURI(resultPrincipalURI);

  newChannel->SetLoadInfo(newLoadInfo);

  // Preserve the privacy bit if it has been overridden
  if (mPrivateBrowsingOverriden) {
    nsCOMPtr<nsIPrivateBrowsingChannel> newPBChannel =
        do_QueryInterface(newChannel);
    if (newPBChannel) {
      newPBChannel->SetPrivate(mPrivateBrowsing);
    }
  }

  if (nsCOMPtr<nsIWritablePropertyBag> bag = ::do_QueryInterface(newChannel)) {
    nsHashPropertyBag::CopyFrom(bag, static_cast<nsIPropertyBag2*>(this));
  }

  // Notify consumer, giving chance to cancel redirect.

  auto redirectCallbackHelper = MakeRefPtr<net::nsAsyncRedirectVerifyHelper>();

  bool checkRedirectSynchronously = !openNewChannel;
  nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();

  mRedirectChannel = newChannel;
  mRedirectFlags = redirectFlags;
  mOpenRedirectChannel = openNewChannel;
  nsresult rv = redirectCallbackHelper->Init(
      this, newChannel, redirectFlags, target, checkRedirectSynchronously);
  if (NS_FAILED(rv)) return rv;

  if (checkRedirectSynchronously && NS_FAILED(mStatus)) return mStatus;

  return NS_OK;
}

nsresult nsBaseChannel::ContinueRedirect() {
  // Make sure to do this _after_ making all the OnChannelRedirect calls
  mRedirectChannel->SetOriginalURI(OriginalURI());

  // If we fail to open the new channel, then we want to leave this channel
  // unaffected, so we defer tearing down our channel until we have succeeded
  // with the redirect.

  if (mOpenRedirectChannel) {
    nsresult rv = NS_OK;
    rv = mRedirectChannel->AsyncOpen(mListener);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mRedirectChannel = nullptr;

  // close down this channel
  Cancel(NS_BINDING_REDIRECTED);
  ChannelDone();

  return NS_OK;
}

bool nsBaseChannel::HasContentTypeHint() const {
  NS_ASSERTION(!Pending(), "HasContentTypeHint called too late");
  return !mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE);
}

nsresult nsBaseChannel::BeginPumpingData() {
  nsresult rv;

  rv = BeginAsyncRead(this, getter_AddRefs(mRequest),
                      getter_AddRefs(mCancelableAsyncRequest));
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(mRequest || mCancelableAsyncRequest,
               "should have got a request or cancelable");
    mPumpingData = true;
    return NS_OK;
  }
  if (rv != NS_ERROR_NOT_IMPLEMENTED) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream;
  nsCOMPtr<nsIChannel> channel;
  rv = OpenContentStream(true, getter_AddRefs(stream), getter_AddRefs(channel));
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(!stream || !channel, "Got both a channel and a stream?");

  if (channel) {
    nsCOMPtr<nsIRunnable> runnable = new RedirectRunnable(this, channel);
    rv = Dispatch(runnable.forget());
    if (NS_SUCCEEDED(rv)) mWaitingOnAsyncRedirect = true;
    return rv;
  }

  // By assigning mPump, we flag this channel as pending (see Pending).  It's
  // important that the pending flag is set when we call into the stream (the
  // call to AsyncRead results in the stream's AsyncWait method being called)
  // and especially when we call into the loadgroup.  Our caller takes care to
  // release mPump if we return an error.

  nsCOMPtr<nsISerialEventTarget> target = GetNeckoTarget();
  rv = nsInputStreamPump::Create(getter_AddRefs(mPump), stream, 0, 0, true,
                                 target);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mPumpingData = true;
  mRequest = mPump;
  rv = mPump->AsyncRead(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<BlockingPromise> promise;
  rv = ListenerBlockingPromise(getter_AddRefs(promise));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (promise) {
    mPump->Suspend();

    RefPtr<nsBaseChannel> self(this);

    promise->Then(
        target, __func__,
        [self, this](nsresult rv) {
          MOZ_ASSERT(mPump);
          MOZ_ASSERT(NS_SUCCEEDED(rv));
          mPump->Resume();
        },
        [self, this](nsresult rv) {
          MOZ_ASSERT(mPump);
          MOZ_ASSERT(NS_FAILED(rv));
          Cancel(rv);
          mPump->Resume();
        });
  }

  return NS_OK;
}

void nsBaseChannel::HandleAsyncRedirect(nsIChannel* newChannel) {
  NS_ASSERTION(!mPumpingData, "Shouldn't have gotten here");

  nsresult rv = mStatus;
  if (NS_SUCCEEDED(mStatus)) {
    rv = Redirect(newChannel, nsIChannelEventSink::REDIRECT_TEMPORARY, true);
    if (NS_SUCCEEDED(rv)) {
      // OnRedirectVerifyCallback will be called asynchronously
      return;
    }
  }

  ContinueHandleAsyncRedirect(rv);
}

void nsBaseChannel::ContinueHandleAsyncRedirect(nsresult result) {
  mWaitingOnAsyncRedirect = false;

  if (NS_FAILED(result)) Cancel(result);

  if (NS_FAILED(result) && mListener) {
    // Notify our consumer ourselves
    mListener->OnStartRequest(this);
    mListener->OnStopRequest(this, mStatus);
    ChannelDone();
  }

  if (mLoadGroup) mLoadGroup->RemoveRequest(this, nullptr, mStatus);

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;
  CallbacksChanged();
}

void nsBaseChannel::ClassifyURI() {
  // For channels created in the child process, delegate to the parent to
  // classify URIs.
  if (!XRE_IsParentProcess()) {
    return;
  }

  if (NS_ShouldClassifyChannel(this)) {
    auto classifier = MakeRefPtr<net::nsChannelClassifier>(this);
    classifier->Start();
  }
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsISupports

NS_IMPL_ADDREF(nsBaseChannel)
NS_IMPL_RELEASE(nsBaseChannel)

NS_INTERFACE_MAP_BEGIN(nsBaseChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableRequest)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateBrowsingChannel)
NS_INTERFACE_MAP_END_INHERITING(nsHashPropertyBag)

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIRequest

NS_IMETHODIMP
nsBaseChannel::GetName(nsACString& result) {
  if (!mURI) {
    result.Truncate();
    return NS_OK;
  }
  return mURI->GetSpec(result);
}

NS_IMETHODIMP
nsBaseChannel::IsPending(bool* result) {
  *result = Pending();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetStatus(nsresult* status) {
  if (mRequest && NS_SUCCEEDED(mStatus)) {
    mRequest->GetStatus(status);
  } else {
    *status = mStatus;
  }
  return NS_OK;
}

NS_IMETHODIMP nsBaseChannel::SetCanceledReason(const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsBaseChannel::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsBaseChannel::CancelWithReason(nsresult aStatus,
                                              const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

NS_IMETHODIMP
nsBaseChannel::Cancel(nsresult status) {
  // Ignore redundant cancelation
  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  mStatus = status;

  if (mCancelableAsyncRequest) {
    mCancelableAsyncRequest->Cancel(status);
  }

  if (mRequest) {
    mRequest->Cancel(status);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::Suspend() {
  NS_ENSURE_TRUE(mPumpingData, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mRequest, NS_ERROR_NOT_IMPLEMENTED);
  return mRequest->Suspend();
}

NS_IMETHODIMP
nsBaseChannel::Resume() {
  NS_ENSURE_TRUE(mPumpingData, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mRequest, NS_ERROR_NOT_IMPLEMENTED);
  return mRequest->Resume();
}

NS_IMETHODIMP
nsBaseChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsBaseChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsBaseChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  nsCOMPtr<nsILoadGroup> loadGroup(mLoadGroup);
  loadGroup.forget(aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  if (!CanSetLoadGroup(aLoadGroup)) {
    return NS_ERROR_FAILURE;
  }

  mLoadGroup = aLoadGroup;
  CallbacksChanged();
  UpdatePrivateBrowsing();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIChannel

NS_IMETHODIMP
nsBaseChannel::GetOriginalURI(nsIURI** aURI) {
  RefPtr<nsIURI> uri = OriginalURI();
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetOriginalURI(nsIURI* aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetURI(nsIURI** aURI) {
  nsCOMPtr<nsIURI> uri(mURI);
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetOwner(nsISupports** aOwner) {
  nsCOMPtr<nsISupports> owner(mOwner);
  owner.forget(aOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetOwner(nsISupports* aOwner) {
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_RELEASE_ASSERT(aLoadInfo, "loadinfo can't be null");
  mLoadInfo = aLoadInfo;

  // Need to update |mNeckoTarget| when load info has changed.
  SetupNeckoTarget();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  nsCOMPtr<nsILoadInfo> loadInfo(mLoadInfo);
  loadInfo.forget(aLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP
nsBaseChannel::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks) {
  nsCOMPtr<nsIInterfaceRequestor> callbacks(mCallbacks);
  callbacks.forget(aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks) {
  if (!CanSetCallbacks(aCallbacks)) {
    return NS_ERROR_FAILURE;
  }

  mCallbacks = aCallbacks;
  CallbacksChanged();
  UpdatePrivateBrowsing();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) {
  *aSecurityInfo = do_AddRef(mSecurityInfo).take();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentType(nsACString& aContentType) {
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentType(const nsACString& aContentType) {
  // mContentCharset is unchanged if not parsed
  bool dummy;
  net_ParseContentType(aContentType, mContentType, mContentCharset, &dummy);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentCharset(nsACString& aContentCharset) {
  aContentCharset = mContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentCharset(const nsACString& aContentCharset) {
  mContentCharset = aContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentDisposition(uint32_t* aContentDisposition) {
  // preserve old behavior, fail unless explicitly set.
  if (mContentDispositionHint == UINT32_MAX) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aContentDisposition = mContentDispositionHint;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentDisposition(uint32_t aContentDisposition) {
  mContentDispositionHint = aContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  if (!mContentDispositionFilename) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aContentDispositionFilename = *mContentDispositionFilename;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  mContentDispositionFilename =
      MakeUnique<nsString>(aContentDispositionFilename);

  // For safety reasons ensure the filename doesn't contain null characters and
  // replace them with underscores. We may later pass the extension to system
  // MIME APIs that expect null terminated strings.
  mContentDispositionFilename->ReplaceChar(char16_t(0), '_');

  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsBaseChannel::GetContentLength(int64_t* aContentLength) {
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::SetContentLength(int64_t aContentLength) {
  mContentLength = aContentLength;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::Open(nsIInputStream** aStream) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(!mPumpingData, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_IN_PROGRESS);

  nsCOMPtr<nsIChannel> chan;
  rv = OpenContentStream(false, aStream, getter_AddRefs(chan));
  NS_ASSERTION(!chan || !*aStream, "Got both a channel and a stream?");
  if (NS_SUCCEEDED(rv) && chan) {
    rv = Redirect(chan, nsIChannelEventSink::REDIRECT_INTERNAL, false);
    if (NS_FAILED(rv)) return rv;
    rv = chan->Open(aStream);
  } else if (rv == NS_ERROR_NOT_IMPLEMENTED) {
    return NS_ImplementChannelOpen(this, aStream);
  }

  if (NS_SUCCEEDED(rv)) {
    mWasOpened = true;
    ClassifyURI();
  }

  return rv;
}

NS_IMETHODIMP
nsBaseChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;

  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  MOZ_ASSERT(
      mLoadInfo->GetSecurityMode() == 0 ||
          mLoadInfo->GetInitialSecurityCheckDone() ||
          (mLoadInfo->GetSecurityMode() ==
               nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL &&
           mLoadInfo->GetLoadingPrincipal() &&
           mLoadInfo->GetLoadingPrincipal()->IsSystemPrincipal()),
      "security flags in loadInfo but doContentSecurityCheck() not called");

  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(!mPumpingData, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);
  NS_ENSURE_ARG(listener);

  SetupNeckoTarget();

  // Skip checking for chrome:// sub-resources.
  nsAutoCString scheme;
  mURI->GetScheme(scheme);
  if (!scheme.EqualsLiteral("file")) {
    NS_CompareLoadInfoAndLoadContext(this);
  }

  // Ensure that this is an allowed port before proceeding.
  rv = NS_CheckPortSafety(mURI);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  AntiTrackingUtils::UpdateAntiTrackingInfoForChannel(this);

  // Store the listener and context early so that OpenContentStream and the
  // stream's AsyncWait method (called by AsyncRead) can have access to them
  // via the StreamListener methods.  However, since
  // this typically introduces a reference cycle between this and the listener,
  // we need to be sure to break the reference if this method does not succeed.
  mListener = listener;

  // This method assigns mPump as a side-effect.  We need to clear mPump if
  // this method fails.
  rv = BeginPumpingData();
  if (NS_FAILED(rv)) {
    mPump = nullptr;
    mRequest = nullptr;
    mPumpingData = false;
    ChannelDone();
    mCallbacks = nullptr;
    return rv;
  }

  // At this point, we are going to return success no matter what.

  mWasOpened = true;

  SUSPEND_PUMP_FOR_SCOPE();

  if (mLoadGroup) mLoadGroup->AddRequest(this, nullptr);

  ClassifyURI();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsITransportEventSink

NS_IMETHODIMP
nsBaseChannel::OnTransportStatus(nsITransport* transport, nsresult status,
                                 int64_t progress, int64_t progressMax) {
  // In some cases, we may wish to suppress transport-layer status events.

  if (!mPumpingData || NS_FAILED(mStatus)) {
    return NS_OK;
  }

  SUSPEND_PUMP_FOR_SCOPE();

  // Lazily fetch mProgressSink
  if (!mProgressSink) {
    if (mQueriedProgressSink) {
      return NS_OK;
    }
    GetCallback(mProgressSink);
    mQueriedProgressSink = true;
    if (!mProgressSink) {
      return NS_OK;
    }
  }

  if (!HasLoadFlag(LOAD_BACKGROUND)) {
    nsAutoString statusArg;
    if (GetStatusArg(status, statusArg)) {
      mProgressSink->OnStatus(this, status, statusArg.get());
    }
  }

  if (progress) {
    mProgressSink->OnProgress(this, progress, progressMax);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIInterfaceRequestor

NS_IMETHODIMP
nsBaseChannel::GetInterface(const nsIID& iid, void** result) {
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup, iid, result);
  return *result ? NS_OK : NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIRequestObserver

static void CallTypeSniffers(void* aClosure, const uint8_t* aData,
                             uint32_t aCount) {
  nsIChannel* chan = static_cast<nsIChannel*>(aClosure);

  nsAutoCString newType;
  NS_SniffContent(NS_CONTENT_SNIFFER_CATEGORY, chan, aData, aCount, newType);
  if (!newType.IsEmpty()) {
    chan->SetContentType(newType);
  }
}

static void CallUnknownTypeSniffer(void* aClosure, const uint8_t* aData,
                                   uint32_t aCount) {
  nsIChannel* chan = static_cast<nsIChannel*>(aClosure);

  RefPtr<nsUnknownDecoder> sniffer = new nsUnknownDecoder();

  nsAutoCString detected;
  nsresult rv = sniffer->GetMIMETypeFromContent(chan, aData, aCount, detected);
  if (NS_SUCCEEDED(rv)) chan->SetContentType(detected);
}

NS_IMETHODIMP
nsBaseChannel::OnStartRequest(nsIRequest* request) {
  MOZ_ASSERT_IF(mRequest, request == mRequest);
  MOZ_ASSERT_IF(mCancelableAsyncRequest, !mRequest);

  if (mPump) {
    // If our content type is unknown, use the content type
    // sniffer. If the sniffer is not available for some reason, then we just
    // keep going as-is.
    if (NS_SUCCEEDED(mStatus) &&
        mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
      mPump->PeekStream(CallUnknownTypeSniffer, static_cast<nsIChannel*>(this));
    }

    // Now, the general type sniffers. Skip this if we have none.
    if (mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS) {
      mPump->PeekStream(CallTypeSniffers, static_cast<nsIChannel*>(this));
    }
  }

  SUSPEND_PUMP_FOR_SCOPE();

  if (mListener) {  // null in case of redirect
    return mListener->OnStartRequest(this);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::OnStopRequest(nsIRequest* request, nsresult status) {
  // If both mStatus and status are failure codes, we keep mStatus as-is since
  // that is consistent with our GetStatus and Cancel methods.
  if (NS_SUCCEEDED(mStatus)) mStatus = status;

  // Cause Pending to return false.
  mPump = nullptr;
  mRequest = nullptr;
  mCancelableAsyncRequest = nullptr;
  mPumpingData = false;

  if (mListener) {  // null in case of redirect
    mListener->OnStopRequest(this, mStatus);
  }
  ChannelDone();

  // No need to suspend pump in this scope since we will not be receiving
  // any more events from it.

  if (mLoadGroup) mLoadGroup->RemoveRequest(this, nullptr, mStatus);

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;
  CallbacksChanged();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIStreamListener

NS_IMETHODIMP
nsBaseChannel::OnDataAvailable(nsIRequest* request, nsIInputStream* stream,
                               uint64_t offset, uint32_t count) {
  SUSPEND_PUMP_FOR_SCOPE();

  nsresult rv = mListener->OnDataAvailable(this, stream, offset, count);
  if (mSynthProgressEvents && NS_SUCCEEDED(rv)) {
    int64_t prog = offset + count;
    if (NS_IsMainThread()) {
      OnTransportStatus(nullptr, NS_NET_STATUS_READING, prog, mContentLength);
    } else {
      class OnTransportStatusAsyncEvent : public Runnable {
        RefPtr<nsBaseChannel> mChannel;
        int64_t mProgress;
        int64_t mContentLength;

       public:
        OnTransportStatusAsyncEvent(nsBaseChannel* aChannel, int64_t aProgress,
                                    int64_t aContentLength)
            : Runnable("OnTransportStatusAsyncEvent"),
              mChannel(aChannel),
              mProgress(aProgress),
              mContentLength(aContentLength) {}

        NS_IMETHOD Run() override {
          return mChannel->OnTransportStatus(nullptr, NS_NET_STATUS_READING,
                                             mProgress, mContentLength);
        }
      };

      nsCOMPtr<nsIRunnable> runnable =
          new OnTransportStatusAsyncEvent(this, prog, mContentLength);
      Dispatch(runnable.forget());
    }
  }

  return rv;
}

NS_IMETHODIMP
nsBaseChannel::OnRedirectVerifyCallback(nsresult result) {
  if (NS_SUCCEEDED(result)) result = ContinueRedirect();

  if (NS_FAILED(result) && !mWaitingOnAsyncRedirect) {
    if (NS_SUCCEEDED(mStatus)) mStatus = result;
    return NS_OK;
  }

  if (mWaitingOnAsyncRedirect) ContinueHandleAsyncRedirect(result);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseChannel::RetargetDeliveryTo(nsISerialEventTarget* aEventTarget) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRequest) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIThreadRetargetableRequest> req;
  if (mAllowThreadRetargeting) {
    req = do_QueryInterface(mRequest);
  }

  NS_ENSURE_TRUE(req, NS_ERROR_NOT_IMPLEMENTED);

  return req->RetargetDeliveryTo(aEventTarget);
}

NS_IMETHODIMP
nsBaseChannel::GetDeliveryTarget(nsISerialEventTarget** aEventTarget) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(mRequest, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIThreadRetargetableRequest> req;
  req = do_QueryInterface(mRequest);

  NS_ENSURE_TRUE(req, NS_ERROR_NOT_IMPLEMENTED);
  return req->GetDeliveryTarget(aEventTarget);
}

NS_IMETHODIMP
nsBaseChannel::CheckListenerChain() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mAllowThreadRetargeting) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIThreadRetargetableStreamListener> listener =
      do_QueryInterface(mListener);
  if (!listener) {
    return NS_ERROR_NO_INTERFACE;
  }

  return listener->CheckListenerChain();
}

NS_IMETHODIMP
nsBaseChannel::OnDataFinished(nsresult aStatus) {
  if (!mListener) {
    return NS_ERROR_FAILURE;
  }

  if (!mAllowThreadRetargeting) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIThreadRetargetableStreamListener> listener =
      do_QueryInterface(mListener);
  if (listener) {
    return listener->OnDataFinished(aStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP nsBaseChannel::GetCanceled(bool* aCanceled) {
  *aCanceled = mCanceled;
  return NS_OK;
}

void nsBaseChannel::SetupNeckoTarget() {
  mNeckoTarget = GetMainThreadSerialEventTarget();
}

nsBaseChannel::ContentRange::ContentRange(const nsACString& aRangeHeader,
                                          uint64_t aSize)
    : mStart(0), mEnd(0), mSize(0) {
  auto parsed = nsContentUtils::ParseSingleRangeRequest(aRangeHeader, true);
  // https://fetch.spec.whatwg.org/#ref-for-simple-range-header-value%E2%91%A1
  // If rangeValue is failure, then return a network error.
  if (!parsed) {
    return;
  }

  // Sanity check: ParseSingleRangeRequest should handle these two cases.
  // If rangeEndValue and rangeStartValue are null, then return failure.
  MOZ_ASSERT(parsed->Start().isSome() || parsed->End().isSome());
  // If rangeStartValue and rangeEndValue are numbers, and rangeStartValue
  // is greater than rangeEndValue, then return failure.
  MOZ_ASSERT(parsed->Start().isNothing() || parsed->End().isNothing() ||
             *parsed->Start() <= *parsed->End());

  // https://fetch.spec.whatwg.org/#ref-for-simple-range-header-value%E2%91%A1
  // If rangeStart is null:
  if (parsed->Start().isNothing()) {
    // Set rangeStart to fullLength − rangeEnd.
    mStart = aSize - *parsed->End();

    // Set rangeEnd to rangeStart + rangeEnd − 1.
    mEnd = mStart + *parsed->End() - 1;

    // Otherwise:
  } else {
    // If rangeStart is greater than or equal to fullLength, then return a
    // network error.
    if (*parsed->Start() >= aSize) {
      return;
    }
    mStart = *parsed->Start();

    // If rangeEnd is null or rangeEnd is greater than or equal to fullLength,
    // then set rangeEnd to fullLength − 1.
    if (parsed->End().isNothing() || *parsed->End() >= aSize) {
      mEnd = aSize - 1;
    } else {
      mEnd = *parsed->End();
    }
  }
  mSize = aSize;
}

void nsBaseChannel::ContentRange::AsHeader(nsACString& aOutString) const {
  aOutString.Assign("bytes "_ns);
  aOutString.AppendInt(mStart);
  aOutString.AppendLiteral("-");
  aOutString.AppendInt(mEnd);
  aOutString.AppendLiteral("/");
  aOutString.AppendInt(mSize);
}
