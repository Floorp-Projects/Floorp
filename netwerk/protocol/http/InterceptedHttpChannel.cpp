/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterceptedHttpChannel.h"
#include "NetworkMarker.h"
#include "nsContentSecurityManager.h"
#include "nsEscape.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "nsHttpChannel.h"
#include "nsIRedirectResultListener.h"
#include "nsStringStream.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS_INHERITED(InterceptedHttpChannel, HttpBaseChannel,
                            nsIInterceptedChannel, nsICacheInfoChannel,
                            nsIAsyncVerifyRedirectCallback, nsIRequestObserver,
                            nsIStreamListener, nsIThreadRetargetableRequest,
                            nsIThreadRetargetableStreamListener)

InterceptedHttpChannel::InterceptedHttpChannel(
    PRTime aCreationTime, const TimeStamp& aCreationTimestamp,
    const TimeStamp& aAsyncOpenTimestamp)
    : HttpAsyncAborter<InterceptedHttpChannel>(this),
      mProgress(0),
      mProgressReported(0),
      mSynthesizedStreamLength(-1),
      mResumeStartPos(0),
      mCallingStatusAndProgress(false),
      mTimeStamps() {
  // Pre-set the creation and AsyncOpen times based on the original channel
  // we are intercepting.  We don't want our extra internal redirect to mask
  // any time spent processing the channel.
  mChannelCreationTime = aCreationTime;
  mChannelCreationTimestamp = aCreationTimestamp;
  mInterceptedChannelCreationTimestamp = TimeStamp::Now();
  mAsyncOpenTime = aAsyncOpenTimestamp;
}

void InterceptedHttpChannel::ReleaseListeners() {
  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, mStatus);
  }
  HttpBaseChannel::ReleaseListeners();
  mSynthesizedResponseHead.reset();
  mRedirectChannel = nullptr;
  mBodyReader = nullptr;
  mReleaseHandle = nullptr;
  mProgressSink = nullptr;
  mBodyCallback = nullptr;
  mPump = nullptr;

  MOZ_DIAGNOSTIC_ASSERT(!LoadIsPending());
}

nsresult InterceptedHttpChannel::SetupReplacementChannel(
    nsIURI* aURI, nsIChannel* aChannel, bool aPreserveMethod,
    uint32_t aRedirectFlags) {
  nsresult rv = HttpBaseChannel::SetupReplacementChannel(
      aURI, aChannel, aPreserveMethod, aRedirectFlags);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = CheckRedirectLimit(aRedirectFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // While we can't resume an synthetic response, we can still propagate
  // the resume params across redirects for other channels to handle.
  if (mResumeStartPos > 0) {
    nsCOMPtr<nsIResumableChannel> resumable = do_QueryInterface(aChannel);
    if (!resumable) {
      return NS_ERROR_NOT_RESUMABLE;
    }

    resumable->ResumeAt(mResumeStartPos, mResumeEntityId);
  }

  return NS_OK;
}

void InterceptedHttpChannel::AsyncOpenInternal() {
  // If an error occurs in this file we must ensure mListener callbacks are
  // invoked in some way.  We either Cancel() or ResetInterception below
  // depending on which path we take.
  nsresult rv = NS_OK;

  // Start the interception, record the start time.
  mTimeStamps.Init(this);
  mTimeStamps.RecordTime();

  // We should have pre-set the AsyncOpen time based on the original channel if
  // timings are enabled.
  if (LoadTimingEnabled()) {
    MOZ_DIAGNOSTIC_ASSERT(!mAsyncOpenTime.IsNull());
  }

  StoreIsPending(true);
  StoreResponseCouldBeSynthesized(true);

  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }

  // If we already have a synthesized body then we are pre-synthesized.
  // This can happen for two reasons:
  //  1. We have a pre-synthesized redirect in e10s mode.  In this case
  //     we should follow the redirect.
  //  2. We are handling a "fake" redirect for an opaque response.  Here
  //     we should just process the synthetic body.
  if (mBodyReader) {
    // If we fail in this path, then cancel the channel.  We don't want
    // to ResetInterception() after a synthetic result has already been
    // produced by the ServiceWorker.
    auto autoCancel = MakeScopeExit([&] {
      if (NS_FAILED(rv)) {
        Cancel(rv);
      }
    });

    // The fetch event will not be dispatched, record current time for
    // FetchHandlerStart and FetchHandlerFinish.
    SetFetchHandlerStart(TimeStamp::Now());
    SetFetchHandlerFinish(TimeStamp::Now());

    if (ShouldRedirect()) {
      rv = FollowSyntheticRedirect();
      return;
    }

    rv = StartPump();
    return;
  }

  // If we fail the initial interception, then attempt to ResetInterception
  // to fall back to network.  We only cancel if the reset fails.
  auto autoReset = MakeScopeExit([&] {
    if (NS_FAILED(rv)) {
      rv = ResetInterception(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Cancel(rv);
      }
    }
  });

  // Otherwise we need to trigger a FetchEvent in a ServiceWorker.
  nsCOMPtr<nsINetworkInterceptController> controller;
  GetCallback(controller);

  if (NS_WARN_IF(!controller)) {
    rv = NS_ERROR_DOM_INVALID_STATE_ERR;
    return;
  }

  rv = controller->ChannelIntercepted(this);
  NS_ENSURE_SUCCESS_VOID(rv);
}

bool InterceptedHttpChannel::ShouldRedirect() const {
  // Determine if the synthetic response requires us to perform a real redirect.
  return nsHttpChannel::WillRedirect(*mResponseHead) &&
         !mLoadInfo->GetDontFollowRedirects();
}

nsresult InterceptedHttpChannel::FollowSyntheticRedirect() {
  // Perform a real redirect based on the synthetic response.

  nsCOMPtr<nsIIOService> ioService;
  nsresult rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString location;
  rv = mResponseHead->GetHeader(nsHttp::Location, location);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  // make sure non-ASCII characters in the location header are escaped.
  nsAutoCString locationBuf;
  if (NS_EscapeURL(location.get(), -1, esc_OnlyNonASCII | esc_Spaces,
                   locationBuf)) {
    location = locationBuf;
  }

  nsCOMPtr<nsIURI> redirectURI;
  rv = ioService->NewURI(nsDependentCString(location.get()), nullptr, mURI,
                         getter_AddRefs(redirectURI));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_CORRUPTED_CONTENT);

  uint32_t redirectFlags = nsIChannelEventSink::REDIRECT_TEMPORARY;
  if (nsHttp::IsPermanentRedirect(mResponseHead->Status())) {
    redirectFlags = nsIChannelEventSink::REDIRECT_PERMANENT;
  }

  PropagateReferenceIfNeeded(mURI, redirectURI);

  bool rewriteToGET = ShouldRewriteRedirectToGET(mResponseHead->Status(),
                                                 mRequestHead.ParsedMethod());

  nsCOMPtr<nsIChannel> newChannel;
  nsCOMPtr<nsILoadInfo> redirectLoadInfo =
      CloneLoadInfoForRedirect(redirectURI, redirectFlags);
  rv = NS_NewChannelInternal(getter_AddRefs(newChannel), redirectURI,
                             redirectLoadInfo,
                             nullptr,  // PerformanceStorage
                             nullptr,  // aLoadGroup
                             nullptr,  // aCallbacks
                             mLoadFlags, ioService);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupReplacementChannel(redirectURI, newChannel, !rewriteToGET,
                               redirectFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  mRedirectChannel = std::move(newChannel);

  rv = gHttpHandler->AsyncOnChannelRedirect(this, mRedirectChannel,
                                            redirectFlags);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    OnRedirectVerifyCallback(rv);
  } else {
    // Redirect success, record the finish time and the final status.
    mTimeStamps.RecordTime(InterceptionTimeStamps::Redirected);
  }

  return rv;
}

nsresult InterceptedHttpChannel::RedirectForResponseURL(
    nsIURI* aResponseURI, bool aResponseRedirected) {
  // Perform a service worker redirect to another InterceptedHttpChannel using
  // the given response URL. It allows content to see the final URL where
  // appropriate and also helps us enforce cross-origin restrictions. The
  // resulting channel will then process the synthetic response as normal. This
  // extra redirect is performed so that listeners treat the result as unsafe
  // cross-origin data.

  nsresult rv = NS_OK;

  // We want to pass ownership of the body callback to the new synthesized
  // channel.  We need to hold a reference to the callbacks on the stack
  // as well, though, so we can call them if a failure occurs.
  nsCOMPtr<nsIInterceptedBodyCallback> bodyCallback = std::move(mBodyCallback);

  RefPtr<InterceptedHttpChannel> newChannel = CreateForSynthesis(
      mResponseHead.get(), mBodyReader, bodyCallback, mChannelCreationTime,
      mChannelCreationTimestamp, mAsyncOpenTime);

  // If the response has been redirected, propagate all the URLs to content.
  // Thus, the exact value of the redirect flag does not matter as long as it's
  // not REDIRECT_INTERNAL.
  uint32_t flags = aResponseRedirected ? nsIChannelEventSink::REDIRECT_TEMPORARY
                                       : nsIChannelEventSink::REDIRECT_INTERNAL;

  nsCOMPtr<nsILoadInfo> redirectLoadInfo =
      CloneLoadInfoForRedirect(aResponseURI, flags);

  ExtContentPolicyType contentPolicyType =
      redirectLoadInfo->GetExternalContentPolicyType();

  rv = newChannel->Init(
      aResponseURI, mCaps, static_cast<nsProxyInfo*>(mProxyInfo.get()),
      mProxyResolveFlags, mProxyURI, mChannelId, contentPolicyType);

  newChannel->SetLoadInfo(redirectLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Normally we don't propagate the LoadInfo's service worker tainting
  // synthesis flag on redirect.  A real redirect normally will want to allow
  // normal tainting to proceed from its starting taint.  For this particular
  // redirect, though, we are performing a redirect to communicate the URL of
  // the service worker synthetic response itself.  This redirect still
  // represents the synthetic response, so we must preserve the flag.
  if (redirectLoadInfo && mLoadInfo &&
      mLoadInfo->GetServiceWorkerTaintingSynthesized()) {
    redirectLoadInfo->SynthesizeServiceWorkerTainting(mLoadInfo->GetTainting());
  }

  rv = SetupReplacementChannel(aResponseURI, newChannel, true, flags);
  NS_ENSURE_SUCCESS(rv, rv);

  mRedirectChannel = newChannel;

  MOZ_ASSERT(mBodyReader);
  MOZ_ASSERT(!LoadApplyConversion());
  newChannel->SetApplyConversion(false);

  rv = gHttpHandler->AsyncOnChannelRedirect(this, mRedirectChannel, flags);

  if (NS_FAILED(rv)) {
    // Make sure to call the body callback since we took ownership
    // above.  Neither the new channel or our standard
    // OnRedirectVerifyCallback() code will invoke the callback.  Do it here.
    bodyCallback->BodyComplete(rv);

    OnRedirectVerifyCallback(rv);
  }

  return rv;
}

nsresult InterceptedHttpChannel::StartPump() {
  MOZ_DIAGNOSTIC_ASSERT(!mPump);
  MOZ_DIAGNOSTIC_ASSERT(mBodyReader);

  // We don't support resuming an intercepted channel.  We can't guarantee the
  // ServiceWorker will always return the same data and we can't rely on the
  // http cache code to detect changes.  For now, just force the channel to
  // NS_ERROR_NOT_RESUMABLE which should cause the front-end to recreate the
  // channel without calling ResumeAt().
  //
  // It would also be possible to convert this information to a range request,
  // but its unclear if we should do that for ServiceWorker FetchEvents.  See:
  //
  //  https://github.com/w3c/ServiceWorker/issues/1201
  if (mResumeStartPos > 0) {
    return NS_ERROR_NOT_RESUMABLE;
  }

  // For progress we trust the content-length for the "maximum" size.
  // We can't determine the full size from the stream itself since
  // we may only receive the data incrementally.  We can't trust
  // Available() here.
  // TODO: We could implement an nsIFixedLengthInputStream interface and
  //       QI to it here.  This would let us determine the total length
  //       for streams that support it.  See bug 1388774.
  Unused << GetContentLength(&mSynthesizedStreamLength);

  nsresult rv =
      nsInputStreamPump::Create(getter_AddRefs(mPump), mBodyReader, 0, 0, true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPump->AsyncRead(this);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t suspendCount = mSuspendCount;
  while (suspendCount--) {
    mPump->Suspend();
  }

  MOZ_DIAGNOSTIC_ASSERT(!mCanceled);

  return rv;
}

nsresult InterceptedHttpChannel::OpenRedirectChannel() {
  nsresult rv = NS_OK;

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  if (!mRedirectChannel) {
    return NS_ERROR_DOM_ABORT_ERR;
  }

  // Make sure to do this after we received redirect veto answer,
  // i.e. after all sinks had been notified
  mRedirectChannel->SetOriginalURI(mOriginalURI);

  // open new channel
  rv = mRedirectChannel->AsyncOpen(mListener);
  NS_ENSURE_SUCCESS(rv, rv);

  mStatus = NS_BINDING_REDIRECTED;

  return rv;
}

void InterceptedHttpChannel::MaybeCallStatusAndProgress() {
  // OnStatus() and OnProgress() must only be called on the main thread.  If
  // we are on a separate thread, then we maybe need to schedule a runnable
  // to call them asynchronousnly.
  if (!NS_IsMainThread()) {
    // Check to see if we are already trying to call OnStatus/OnProgress
    // asynchronously.  If we are, then don't queue up another runnable.
    // We don't want to flood the main thread.
    if (mCallingStatusAndProgress) {
      return;
    }
    mCallingStatusAndProgress = true;

    nsCOMPtr<nsIRunnable> r = NewRunnableMethod(
        "InterceptedHttpChannel::MaybeCallStatusAndProgress", this,
        &InterceptedHttpChannel::MaybeCallStatusAndProgress);
    MOZ_ALWAYS_SUCCEEDS(
        SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

    return;
  }

  MOZ_ASSERT(NS_IsMainThread());

  // We are about to capture out progress position.  Clear the flag we use
  // to de-duplicate progress report runnables.  We want any further progress
  // updates to trigger another runnable.  We do this before capture the
  // progress value since we're using atomics and not a mutex lock.
  mCallingStatusAndProgress = false;

  // Capture the current status from our atomic count.
  int64_t progress = mProgress;

  MOZ_DIAGNOSTIC_ASSERT(progress >= mProgressReported);

  // Do nothing if we've already made the calls for this amount of progress
  // or if the channel is not configured for these calls.  Note, the check
  // for mProgressSink here means we will not fire any spurious late calls
  // after ReleaseListeners() is executed.
  if (progress <= mProgressReported || mCanceled || !mProgressSink ||
      (mLoadFlags & HttpBaseChannel::LOAD_BACKGROUND)) {
    return;
  }

  // Capture the host name on the first set of calls to avoid doing this
  // string processing repeatedly.
  if (mProgressReported == 0) {
    nsAutoCString host;
    MOZ_ALWAYS_SUCCEEDS(mURI->GetHost(host));
    CopyUTF8toUTF16(host, mStatusHost);
  }

  mProgressSink->OnStatus(this, NS_NET_STATUS_READING, mStatusHost.get());

  mProgressSink->OnProgress(this, progress, mSynthesizedStreamLength);

  mProgressReported = progress;
}

void InterceptedHttpChannel::MaybeCallBodyCallback() {
  nsCOMPtr<nsIInterceptedBodyCallback> callback = std::move(mBodyCallback);
  if (callback) {
    callback->BodyComplete(mStatus);
  }
}

// static
already_AddRefed<InterceptedHttpChannel>
InterceptedHttpChannel::CreateForInterception(
    PRTime aCreationTime, const TimeStamp& aCreationTimestamp,
    const TimeStamp& aAsyncOpenTimestamp) {
  // Create an InterceptedHttpChannel that will trigger a FetchEvent
  // in a ServiceWorker when opened.
  RefPtr<InterceptedHttpChannel> ref = new InterceptedHttpChannel(
      aCreationTime, aCreationTimestamp, aAsyncOpenTimestamp);

  return ref.forget();
}

// static
already_AddRefed<InterceptedHttpChannel>
InterceptedHttpChannel::CreateForSynthesis(
    const nsHttpResponseHead* aHead, nsIInputStream* aBody,
    nsIInterceptedBodyCallback* aBodyCallback, PRTime aCreationTime,
    const TimeStamp& aCreationTimestamp, const TimeStamp& aAsyncOpenTimestamp) {
  MOZ_DIAGNOSTIC_ASSERT(aHead);
  MOZ_DIAGNOSTIC_ASSERT(aBody);

  // Create an InterceptedHttpChannel that already has a synthesized response.
  // The synthetic response will be processed when opened.  A FetchEvent
  // will not be triggered.
  RefPtr<InterceptedHttpChannel> ref = new InterceptedHttpChannel(
      aCreationTime, aCreationTimestamp, aAsyncOpenTimestamp);

  ref->mResponseHead = MakeUnique<nsHttpResponseHead>(*aHead);
  ref->mBodyReader = aBody;
  ref->mBodyCallback = aBodyCallback;

  return ref.forget();
}

NS_IMETHODIMP
InterceptedHttpChannel::Cancel(nsresult aStatus) {
  // Note: This class has been designed to send all error results through
  //       Cancel().  Don't add calls directly to AsyncAbort() or
  //       DoNotifyListener().  Instead call Cancel().

  if (mCanceled) {
    return NS_OK;
  }

  // The interception is canceled, record the finish time stamp and the final
  // status
  mTimeStamps.RecordTime(InterceptionTimeStamps::Canceled);

  mCanceled = true;

  MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(aStatus));
  if (NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }

  if (mPump) {
    return mPump->Cancel(mStatus);
  }

  return AsyncAbort(mStatus);
}

NS_IMETHODIMP
InterceptedHttpChannel::Suspend(void) {
  ++mSuspendCount;
  if (mPump) {
    return mPump->Suspend();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::Resume(void) {
  --mSuspendCount;
  if (mPump) {
    return mPump->Resume();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetSecurityInfo(nsISupports** aSecurityInfo) {
  nsCOMPtr<nsISupports> ref(mSecurityInfo);
  ref.forget(aSecurityInfo);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener(aListener);
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(rv);
    return rv;
  }
  if (mCanceled) {
    return mStatus;
  }

  // This is outside of the if block in case we enable the profiler after
  // AsyncOpen().
  mLastStatusReported = TimeStamp::Now();
  if (profiler_can_accept_markers()) {
    nsAutoCString requestMethod;
    GetRequestMethod(requestMethod);

    profiler_add_network_marker(
        mURI, requestMethod, mPriority, mChannelId, NetworkLoadType::LOAD_START,
        mChannelCreationTimestamp, mLastStatusReported, 0, kCacheUnknown,
        mLoadInfo->GetInnerWindowID());
  }

  // After this point we should try to return NS_OK and notify the listener
  // of the result.
  mListener = aListener;

  AsyncOpenInternal();

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::LogBlockedCORSRequest(const nsAString& aMessage,
                                              const nsACString& aCategory) {
  // Synthetic responses should not trigger CORS blocking.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InterceptedHttpChannel::LogMimeTypeMismatch(const nsACString& aMessageName,
                                            bool aWarning,
                                            const nsAString& aURL,
                                            const nsAString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetIsAuthChannel(bool* aIsAuthChannel) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetPriority(int32_t aPriority) {
  mPriority = clamped<int32_t>(aPriority, INT16_MIN, INT16_MAX);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetClassFlags(uint32_t aClassFlags) {
  mClassOfService = aClassFlags;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::ClearClassFlags(uint32_t aClassFlags) {
  mClassOfService &= ~aClassFlags;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::AddClassFlags(uint32_t aClassFlags) {
  mClassOfService |= aClassFlags;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::ResumeAt(uint64_t aStartPos,
                                 const nsACString& aEntityId) {
  // We don't support resuming synthesized responses, but we do track this
  // information so it can be passed on to the resulting nsHttpChannel if
  // ResetInterception is called.
  mResumeStartPos = aStartPos;
  mResumeEntityId = aEntityId;
  return NS_OK;
}

void InterceptedHttpChannel::DoNotifyListenerCleanup() {
  // Prefer to cleanup in ReleaseListeners() as it seems to be called
  // more consistently in necko.
}

void InterceptedHttpChannel::DoAsyncAbort(nsresult aStatus) {
  Unused << AsyncAbort(aStatus);
}

NS_IMETHODIMP
InterceptedHttpChannel::ResetInterception(bool aBypass) {
  if (mCanceled) {
    return mStatus;
  }

  uint32_t flags = nsIChannelEventSink::REDIRECT_INTERNAL;

  nsCOMPtr<nsIChannel> newChannel;
  nsCOMPtr<nsILoadInfo> redirectLoadInfo =
      CloneLoadInfoForRedirect(mURI, flags);

  if (aBypass) {
    redirectLoadInfo->ClearController();
    // TODO: Audit whether we should also be calling
    // ServiceWorkerManager::StopControllingClient for maximum correctness.
  }

  nsresult rv =
      NS_NewChannelInternal(getter_AddRefs(newChannel), mURI, redirectLoadInfo,
                            nullptr,  // PerformanceStorage
                            nullptr,  // aLoadGroup
                            nullptr,  // aCallbacks
                            mLoadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  if (profiler_can_accept_markers()) {
    nsAutoCString requestMethod;
    GetRequestMethod(requestMethod);

    int32_t priority = PRIORITY_NORMAL;
    GetPriority(&priority);

    uint64_t size = 0;
    GetEncodedBodySize(&size);

    nsAutoCString contentType;
    if (mResponseHead) {
      mResponseHead->ContentType(contentType);
    }

    RefPtr<HttpBaseChannel> newBaseChannel = do_QueryObject(newChannel);
    MOZ_ASSERT(newBaseChannel,
               "The redirect channel should be a base channel.");
    profiler_add_network_marker(mURI, requestMethod, priority, mChannelId,
                                NetworkLoadType::LOAD_REDIRECT,
                                mLastStatusReported, TimeStamp::Now(), size,
                                kCacheUnknown, mLoadInfo->GetInnerWindowID(),
                                &mTransactionTimings, std::move(mSource),
                                Some(nsDependentCString(contentType.get())),
                                mURI, flags, newBaseChannel->ChannelId());
  }

  rv = SetupReplacementChannel(mURI, newChannel, true, flags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITimedChannel> newTimedChannel = do_QueryInterface(newChannel);
  if (newTimedChannel) {
    if (!mAsyncOpenTime.IsNull()) {
      newTimedChannel->SetAsyncOpen(mAsyncOpenTime);
    }
    if (!mChannelCreationTimestamp.IsNull()) {
      newTimedChannel->SetChannelCreation(mChannelCreationTimestamp);
    }
  }

  if (mRedirectMode != nsIHttpChannelInternal::REDIRECT_MODE_MANUAL) {
    nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
    rv = newChannel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    loadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
    rv = newChannel->SetLoadFlags(loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mRedirectChannel = std::move(newChannel);

  rv = gHttpHandler->AsyncOnChannelRedirect(this, mRedirectChannel, flags);

  if (NS_FAILED(rv)) {
    OnRedirectVerifyCallback(rv);
  } else {
    // ResetInterception success, record the finish time stamps and the final
    // status.
    mTimeStamps.RecordTime(InterceptionTimeStamps::Reset);
  }

  return rv;
}

NS_IMETHODIMP
InterceptedHttpChannel::SynthesizeStatus(uint16_t aStatus,
                                         const nsACString& aReason) {
  if (mCanceled) {
    return mStatus;
  }

  if (!mSynthesizedResponseHead) {
    mSynthesizedResponseHead.reset(new nsHttpResponseHead());
  }

  nsAutoCString statusLine;
  statusLine.AppendLiteral("HTTP/1.1 ");
  statusLine.AppendInt(aStatus);
  statusLine.AppendLiteral(" ");
  statusLine.Append(aReason);

  mSynthesizedResponseHead->ParseStatusLine(statusLine);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SynthesizeHeader(const nsACString& aName,
                                         const nsACString& aValue) {
  if (mCanceled) {
    return mStatus;
  }

  if (!mSynthesizedResponseHead) {
    mSynthesizedResponseHead.reset(new nsHttpResponseHead());
  }

  nsAutoCString header = aName + ": "_ns + aValue;
  // Overwrite any existing header.
  nsresult rv = mSynthesizedResponseHead->ParseHeaderLine(header);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::StartSynthesizedResponse(
    nsIInputStream* aBody, nsIInterceptedBodyCallback* aBodyCallback,
    nsICacheInfoChannel* aSynthesizedCacheInfo, const nsACString& aFinalURLSpec,
    bool aResponseRedirected) {
  nsresult rv = NS_OK;

  auto autoCleanup = MakeScopeExit([&] {
    // Auto-cancel on failure.  Do this first to get mStatus set, if necessary.
    if (NS_FAILED(rv)) {
      Cancel(rv);
    }

    // If we early exit before taking ownership of the body, then automatically
    // invoke the callback.  This could be due to an error or because we're not
    // going to consume it due to a redirect, etc.
    if (aBodyCallback) {
      aBodyCallback->BodyComplete(mStatus);
    }
  });

  if (NS_FAILED(mStatus)) {
    // Return NS_OK.  The channel should fire callbacks with an error code
    // if it was cancelled before this point.
    return NS_OK;
  }

  // Take ownership of the body callbacks  If a failure occurs we will
  // automatically Cancel() the channel.  This will then invoke OnStopRequest()
  // which will invoke the correct callback.  In the case of an opaque response
  // redirect we pass ownership of the callback to the new channel.
  mBodyCallback = aBodyCallback;
  aBodyCallback = nullptr;

  mSynthesizedCacheInfo = aSynthesizedCacheInfo;

  if (!mSynthesizedResponseHead) {
    mSynthesizedResponseHead.reset(new nsHttpResponseHead());
  }

  mResponseHead = std::move(mSynthesizedResponseHead);

  if (ShouldRedirect()) {
    rv = FollowSyntheticRedirect();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // Intercepted responses should already be decoded.
  SetApplyConversion(false);

  // Errors and redirects may not have a body.  Synthesize an empty string
  // stream here so later code can be simpler.
  mBodyReader = aBody;
  if (!mBodyReader) {
    rv = NS_NewCStringInputStream(getter_AddRefs(mBodyReader), ""_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIURI> responseURI;
  if (!aFinalURLSpec.IsEmpty()) {
    rv = NS_NewURI(getter_AddRefs(responseURI), aFinalURLSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    responseURI = mURI;
  }

  bool equal = false;
  Unused << mURI->Equals(responseURI, &equal);
  if (!equal) {
    rv = RedirectForResponseURL(responseURI, aResponseRedirected);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  rv = StartPump();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::FinishSynthesizedResponse() {
  if (mCanceled) {
    // Return NS_OK.  The channel should fire callbacks with an error code
    // if it was cancelled before this point.
    return NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::CancelInterception(nsresult aStatus) {
  return Cancel(aStatus);
}

NS_IMETHODIMP
InterceptedHttpChannel::GetChannel(nsIChannel** aChannel) {
  nsCOMPtr<nsIChannel> ref(this);
  ref.forget(aChannel);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetSecureUpgradedChannelURI(
    nsIURI** aSecureUpgradedChannelURI) {
  nsCOMPtr<nsIURI> ref(mURI);
  ref.forget(aSecureUpgradedChannelURI);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetChannelInfo(
    mozilla::dom::ChannelInfo* aChannelInfo) {
  return aChannelInfo->ResurrectInfoOnChannel(this);
}

NS_IMETHODIMP
InterceptedHttpChannel::GetInternalContentPolicyType(
    nsContentPolicyType* aPolicyType) {
  if (mLoadInfo) {
    *aPolicyType = mLoadInfo->InternalContentPolicyType();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetConsoleReportCollector(
    nsIConsoleReportCollector** aConsoleReportCollector) {
  nsCOMPtr<nsIConsoleReportCollector> ref(this);
  ref.forget(aConsoleReportCollector);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetFetchHandlerStart(TimeStamp aTimeStamp) {
  mTimeStamps.RecordTime(std::move(aTimeStamp));
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetFetchHandlerFinish(TimeStamp aTimeStamp) {
  mTimeStamps.RecordTime(std::move(aTimeStamp));
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetReleaseHandle(nsISupports* aHandle) {
  mReleaseHandle = aHandle;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnRedirectVerifyCallback(nsresult rv) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_SUCCEEDED(rv)) {
    rv = OpenRedirectChannel();
  }

  nsCOMPtr<nsIRedirectResultListener> hook;
  GetCallback(hook);
  if (hook) {
    hook->OnRedirectResult(NS_SUCCEEDED(rv));
  }

  if (NS_FAILED(rv)) {
    Cancel(rv);
  }

  MaybeCallBodyCallback();

  StoreIsPending(false);
  // We can only release listeners after the redirected channel really owns
  // mListener. Otherwise, the OnStart/OnStopRequest functions of mListener will
  // not be called.
  if (NS_SUCCEEDED(rv)) {
    ReleaseListeners();
  }

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnStartRequest(nsIRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mProgressSink) {
    GetCallback(mProgressSink);
  }

  if (!EnsureOpaqueResponseIsAllowed()) {
    // XXXtt: Return an error code or make the response body null.
    // We silence the error result now because we only want to get how many
    // response will get allowed or blocked by ORB.
  }

  if (mPump && mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS) {
    mPump->PeekStream(CallTypeSniffers, static_cast<nsIChannel*>(this));
  }

  auto isAllowedOrErr = EnsureOpaqueResponseIsAllowedAfterSniff();
  if (isAllowedOrErr.isErr() || !isAllowedOrErr.inspect()) {
    // XXXtt: Return an error code or make the response body null.
    // We silence the error result now because we only want to get how many
    // response will get allowed or blocked by ORB.
  }

  nsresult rv = ProcessCrossOriginEmbedderPolicyHeader();
  if (NS_FAILED(rv)) {
    mStatus = NS_ERROR_BLOCKED_BY_POLICY;
    Cancel(mStatus);
  }

  rv = ProcessCrossOriginResourcePolicyHeader();
  if (NS_FAILED(rv)) {
    mStatus = NS_ERROR_DOM_CORP_FAILED;
    Cancel(mStatus);
  }

  rv = ComputeCrossOriginOpenerPolicyMismatch();
  if (rv == NS_ERROR_BLOCKED_BY_POLICY) {
    mStatus = NS_ERROR_BLOCKED_BY_POLICY;
    Cancel(mStatus);
  }

  rv = ValidateMIMEType();
  if (NS_FAILED(rv)) {
    mStatus = rv;
    Cancel(mStatus);
  }

  StoreOnStartRequestCalled(true);
  if (mListener) {
    return mListener->OnStartRequest(this);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }

  MaybeCallBodyCallback();

  mTimeStamps.RecordTime(InterceptionTimeStamps::Synthesized);

  // Its possible that we have any async runnable queued to report some
  // progress when OnStopRequest() is triggered.  Report any left over
  // progress immediately.  The extra runnable will then do nothing thanks
  // to the ReleaseListeners() call below.
  MaybeCallStatusAndProgress();

  StoreIsPending(false);

  // Register entry to the PerformanceStorage resource timing
  MaybeReportTimingData();

  if (profiler_can_accept_markers()) {
    // These do allocations/frees/etc; avoid if not active
    nsAutoCString requestMethod;
    GetRequestMethod(requestMethod);

    int32_t priority = PRIORITY_NORMAL;
    GetPriority(&priority);

    uint64_t size = 0;
    GetEncodedBodySize(&size);

    nsAutoCString contentType;
    if (mResponseHead) {
      mResponseHead->ContentType(contentType);
    }
    profiler_add_network_marker(
        mURI, requestMethod, priority, mChannelId, NetworkLoadType::LOAD_STOP,
        mLastStatusReported, TimeStamp::Now(), size, kCacheUnknown,
        mLoadInfo->GetInnerWindowID(), &mTransactionTimings, std::move(mSource),
        Some(nsDependentCString(contentType.get())));
  }

  nsresult rv = NS_OK;
  if (mListener) {
    rv = mListener->OnStopRequest(this, mStatus);
  }

  gHttpHandler->OnStopRequest(this);

  ReleaseListeners();

  return rv;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnDataAvailable(nsIRequest* aRequest,
                                        nsIInputStream* aInputStream,
                                        uint64_t aOffset, uint32_t aCount) {
  // Any thread if the channel has been retargeted.

  if (mCanceled || !mListener) {
    // If there is no listener, we still need to drain the stream in order
    // maintain necko invariants.
    uint32_t unused = 0;
    aInputStream->ReadSegments(NS_DiscardSegment, nullptr, aCount, &unused);
    return mStatus;
  }
  if (mProgressSink) {
    if (!(mLoadFlags & HttpBaseChannel::LOAD_BACKGROUND)) {
      mProgress = aOffset + aCount;
      MaybeCallStatusAndProgress();
    }
  }

  return mListener->OnDataAvailable(this, aInputStream, aOffset, aCount);
}

NS_IMETHODIMP
InterceptedHttpChannel::RetargetDeliveryTo(nsIEventTarget* aNewTarget) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aNewTarget);

  // If retargeting to the main thread, do nothing.
  if (aNewTarget->IsOnCurrentThread()) {
    return NS_OK;
  }

  // Retargeting is only valid during OnStartRequest for nsIChannels.  So
  // we should only be called if we have a pump.
  if (!mPump) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mPump->RetargetDeliveryTo(aNewTarget);
}

NS_IMETHODIMP
InterceptedHttpChannel::GetDeliveryTarget(nsIEventTarget** aEventTarget) {
  if (!mPump) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mPump->GetDeliveryTarget(aEventTarget);
}

NS_IMETHODIMP
InterceptedHttpChannel::CheckListenerChain() {
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = NS_OK;
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
      do_QueryInterface(mListener, &rv);
  if (retargetableListener) {
    rv = retargetableListener->CheckListenerChain();
  }
  return rv;
}

//-----------------------------------------------------------------------------
// InterceptedHttpChannel::nsICacheInfoChannel
//-----------------------------------------------------------------------------
// InterceptedHttpChannel does not really implement the nsICacheInfoChannel
// interface, we tranfers parameters to the saved
// nsICacheInfoChannel(mSynthesizedCacheInfo) from StartSynthesizedResponse. And
// we return false in IsFromCache and NS_ERROR_NOT_AVAILABLE for all other
// methods while the saved mSynthesizedCacheInfo does not exist.
NS_IMETHODIMP
InterceptedHttpChannel::IsFromCache(bool* value) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->IsFromCache(value);
  }
  *value = false;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::IsRacing(bool* value) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->IsRacing(value);
  }
  *value = false;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheEntryId(uint64_t* aCacheEntryId) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheEntryId(aCacheEntryId);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheTokenFetchCount(int32_t* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenFetchCount(_retval);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheTokenExpirationTime(uint32_t* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenExpirationTime(_retval);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetAllowStaleCacheContent(
    bool aAllowStaleCacheContent) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetAllowStaleCacheContent(
        aAllowStaleCacheContent);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetAllowStaleCacheContent(
    bool* aAllowStaleCacheContent) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetAllowStaleCacheContent(
        aAllowStaleCacheContent);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetPreferCacheLoadOverBypass(
    bool* aPreferCacheLoadOverBypass) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetPreferCacheLoadOverBypass(
        aPreferCacheLoadOverBypass);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetPreferCacheLoadOverBypass(
    bool aPreferCacheLoadOverBypass) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetPreferCacheLoadOverBypass(
        aPreferCacheLoadOverBypass);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::PreferAlternativeDataType(
    const nsACString& aType, const nsACString& aContentType,
    bool aDeliverAltData) {
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();
  mPreferredCachedAltDataTypes.AppendElement(PreferredAlternativeDataTypeParams(
      nsCString(aType), nsCString(aContentType), aDeliverAltData));
  return NS_OK;
}

const nsTArray<PreferredAlternativeDataTypeParams>&
InterceptedHttpChannel::PreferredAlternativeDataTypes() {
  return mPreferredCachedAltDataTypes;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetAlternativeDataType(nsACString& aType) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetAlternativeDataType(aType);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::OpenAlternativeOutputStream(
    const nsACString& type, int64_t predictedSize,
    nsIAsyncOutputStream** _retval) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->OpenAlternativeOutputStream(
        type, predictedSize, _retval);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetOriginalInputStream(
    nsIInputStreamReceiver* aReceiver) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetOriginalInputStream(aReceiver);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetAltDataInputStream(
    const nsACString& aType, nsIInputStreamReceiver* aReceiver) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetAltDataInputStream(aType, aReceiver);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheKey(uint32_t* key) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheKey(key);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetCacheKey(uint32_t key) {
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetCacheKey(key);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

// InterceptionTimeStamps implementation
InterceptedHttpChannel::InterceptionTimeStamps::InterceptionTimeStamps()
    : mStage(InterceptedHttpChannel::InterceptionTimeStamps::InterceptionStart),
      mStatus(InterceptedHttpChannel::InterceptionTimeStamps::Created) {}

void InterceptedHttpChannel::InterceptionTimeStamps::Init(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(mStatus == Created);

  mStatus = Initialized;

  mIsNonSubresourceRequest = nsContentUtils::IsNonSubresourceRequest(aChannel);
  mKey = mIsNonSubresourceRequest ? "navigation"_ns : "subresource"_ns;
  nsCOMPtr<nsIInterceptedChannel> interceptedChannel =
      do_QueryInterface(aChannel);
  // It must be a InterceptedHttpChannel
  MOZ_ASSERT(interceptedChannel);
  if (!mIsNonSubresourceRequest) {
    interceptedChannel->GetSubresourceTimeStampKey(aChannel, mSubresourceKey);
  }
}

void InterceptedHttpChannel::InterceptionTimeStamps::RecordTime(
    InterceptedHttpChannel::InterceptionTimeStamps::Status&& aStatus,
    TimeStamp&& aTimeStamp) {
  // Only allow passing Synthesized, Reset, Redirected, and Canceled in this
  // method.
  MOZ_ASSERT(aStatus == Synthesized || aStatus == Reset ||
             aStatus == Canceled || aStatus == Redirected);
  if (mStatus == Canceled) {
    return;
  }

  // If current status is not Initialized, only Canceled can be recorded.
  // That means it is canceled after other operation is done, ex. synthesized.
  MOZ_ASSERT(mStatus == Initialized || aStatus == Canceled);

  if (mStatus == Initialized) {
    mStatus = aStatus;
  } else {
    switch (mStatus) {
      case Synthesized:
        mStatus = CanceledAfterSynthesized;
        break;
      case Reset:
        mStatus = CanceledAfterReset;
        break;
      case Redirected:
        mStatus = CanceledAfterRedirected;
        break;
      default:
        MOZ_ASSERT(false);
        break;
    }
  }

  RecordTimeInternal(std::move(aTimeStamp));
}

void InterceptedHttpChannel::InterceptionTimeStamps::RecordTime(
    TimeStamp&& aTimeStamp) {
  MOZ_ASSERT(mStatus == Initialized || mStatus == Canceled);
  if (mStatus == Canceled) {
    return;
  }
  RecordTimeInternal(std::move(aTimeStamp));
}

void InterceptedHttpChannel::InterceptionTimeStamps::RecordTimeInternal(
    TimeStamp&& aTimeStamp) {
  MOZ_ASSERT(mStatus != Created);

  if (mStatus == Canceled && mStage != InterceptionFinish) {
    mFetchHandlerStart = aTimeStamp;
    mFetchHandlerFinish = aTimeStamp;
    mStage = InterceptionFinish;
  }

  switch (mStage) {
    case InterceptionStart: {
      MOZ_ASSERT(mInterceptionStart.IsNull());
      mInterceptionStart = aTimeStamp;
      mStage = FetchHandlerStart;
      break;
    }
    case (FetchHandlerStart): {
      MOZ_ASSERT(mFetchHandlerStart.IsNull());
      mFetchHandlerStart = aTimeStamp;
      mStage = FetchHandlerFinish;
      break;
    }
    case (FetchHandlerFinish): {
      MOZ_ASSERT(mFetchHandlerFinish.IsNull());
      mFetchHandlerFinish = aTimeStamp;
      mStage = InterceptionFinish;
      break;
    }
    case InterceptionFinish: {
      mInterceptionFinish = aTimeStamp;
      SaveTimeStamps();
      return;
    }
    default: {
      return;
    }
  }
}

void InterceptedHttpChannel::InterceptionTimeStamps::GenKeysWithStatus(
    nsCString& aKey, nsCString& aSubresourceKey) {
  nsAutoCString statusString;
  switch (mStatus) {
    case Synthesized:
      statusString = "synthesized"_ns;
      break;
    case Reset:
      statusString = "reset"_ns;
      break;
    case Redirected:
      statusString = "redirected"_ns;
      break;
    case Canceled:
      statusString = "canceled"_ns;
      break;
    case CanceledAfterSynthesized:
      statusString = "canceled-after-synthesized"_ns;
      break;
    case CanceledAfterReset:
      statusString = "canceled-after-reset"_ns;
      break;
    case CanceledAfterRedirected:
      statusString = "canceled-after-redirected"_ns;
      break;
    default:
      return;
  }
  aKey = mKey;
  aSubresourceKey = mSubresourceKey;
  aKey.AppendLiteral("_");
  aSubresourceKey.AppendLiteral("_");
  aKey.Append(statusString);
  aSubresourceKey.Append(statusString);
}

void InterceptedHttpChannel::InterceptionTimeStamps::SaveTimeStamps() {
  MOZ_ASSERT(mStatus != Initialized && mStatus != Created);

  if (mStatus == Synthesized || mStatus == Reset) {
    Telemetry::HistogramID id =
        Telemetry::SERVICE_WORKER_FETCH_EVENT_FINISH_SYNTHESIZED_RESPONSE_MS_2;
    if (mStatus == Reset) {
      id = Telemetry::SERVICE_WORKER_FETCH_EVENT_CHANNEL_RESET_MS_2;
    }

    Telemetry::Accumulate(
        id, mKey,
        static_cast<uint32_t>(
            (mInterceptionFinish - mFetchHandlerFinish).ToMilliseconds()));
    if (!mIsNonSubresourceRequest && !mSubresourceKey.IsEmpty()) {
      Telemetry::Accumulate(
          id, mSubresourceKey,
          static_cast<uint32_t>(
              (mInterceptionFinish - mFetchHandlerFinish).ToMilliseconds()));
    }
  }

  if (!mFetchHandlerStart.IsNull()) {
    Telemetry::Accumulate(
        Telemetry::SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS_2, mKey,
        static_cast<uint32_t>(
            (mFetchHandlerStart - mInterceptionStart).ToMilliseconds()));

    if (!mIsNonSubresourceRequest && !mSubresourceKey.IsEmpty()) {
      Telemetry::Accumulate(
          Telemetry::SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS_2, mSubresourceKey,
          static_cast<uint32_t>(
              (mFetchHandlerStart - mInterceptionStart).ToMilliseconds()));
    }
  }

  nsAutoCString key, subresourceKey;
  GenKeysWithStatus(key, subresourceKey);

  Telemetry::Accumulate(
      Telemetry::SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS_2, key,
      static_cast<uint32_t>(
          (mInterceptionFinish - mInterceptionStart).ToMilliseconds()));
  if (!mIsNonSubresourceRequest && !mSubresourceKey.IsEmpty()) {
    Telemetry::Accumulate(
        Telemetry::SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS_2,
        subresourceKey,
        static_cast<uint32_t>(
            (mInterceptionFinish - mInterceptionStart).ToMilliseconds()));
  }
}

}  // namespace net
}  // namespace mozilla
