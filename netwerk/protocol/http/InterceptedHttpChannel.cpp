/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterceptedHttpChannel.h"
#include "nsContentSecurityManager.h"
#include "nsEscape.h"
#include "mozilla/dom/Performance.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS_INHERITED(InterceptedHttpChannel,
                            HttpBaseChannel,
                            nsIInterceptedChannel,
                            nsICacheInfoChannel,
                            nsIAsyncVerifyRedirectCallback,
                            nsIRequestObserver,
                            nsIStreamListener,
                            nsIChannelWithDivertableParentListener,
                            nsIThreadRetargetableRequest,
                            nsIThreadRetargetableStreamListener)

InterceptedHttpChannel::InterceptedHttpChannel(PRTime aCreationTime,
                                               const TimeStamp& aCreationTimestamp,
                                               const TimeStamp& aAsyncOpenTimestamp)
  : HttpAsyncAborter<InterceptedHttpChannel>(this)
  , mProgress(0)
  , mProgressReported(0)
  , mSynthesizedStreamLength(-1)
  , mResumeStartPos(0)
  , mSynthesizedOrReset(Invalid)
  , mCallingStatusAndProgress(false)
  , mDiverting(false)
{
  // Pre-set the creation and AsyncOpen times based on the original channel
  // we are intercepting.  We don't want our extra internal redirect to mask
  // any time spent processing the channel.
  mChannelCreationTime = aCreationTime;
  mChannelCreationTimestamp = aCreationTimestamp;
  mAsyncOpenTime = aAsyncOpenTimestamp;
}

void
InterceptedHttpChannel::ReleaseListeners()
{
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
  mParentChannel = nullptr;

  MOZ_DIAGNOSTIC_ASSERT(!mIsPending);
}

nsresult
InterceptedHttpChannel::SetupReplacementChannel(nsIURI *aURI,
                                                nsIChannel *aChannel,
                                                bool aPreserveMethod,
                                                uint32_t aRedirectFlags)
{
  nsresult rv = HttpBaseChannel::SetupReplacementChannel(aURI, aChannel,
                                                         aPreserveMethod,
                                                         aRedirectFlags);
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

void
InterceptedHttpChannel::AsyncOpenInternal()
{
  // If an error occurs in this file we must ensure mListener callbacks are
  // invoked in some way.  We either Cancel() or ResetInterception below
  // depending on which path we take.
  nsresult rv = NS_OK;

  // We should have pre-set the AsyncOpen time based on the original channel if
  // timings are enabled.
  if (mTimingEnabled) {
    MOZ_DIAGNOSTIC_ASSERT(!mAsyncOpenTime.IsNull());
  }

  mIsPending = true;
  mResponseCouldBeSynthesized = true;

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
      rv = ResetInterception();
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

bool
InterceptedHttpChannel::ShouldRedirect() const
{
  // Determine if the synthetic response requires us to perform a real redirect.
  return nsHttpChannel::WillRedirect(mResponseHead) &&
         !mLoadInfo->GetDontFollowRedirects();
}

nsresult
InterceptedHttpChannel::FollowSyntheticRedirect()
{
  // Perform a real redirect based on the synthetic response.

  nsCOMPtr<nsIIOService> ioService;
  nsresult rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString location;
  rv = mResponseHead->GetHeader(nsHttp::Location, location);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  // make sure non-ASCII characters in the location header are escaped.
  nsAutoCString locationBuf;
  if (NS_EscapeURL(location.get(), -1, esc_OnlyNonASCII, locationBuf)) {
    location = locationBuf;
  }

  nsCOMPtr<nsIURI> redirectURI;
  rv = ioService->NewURI(nsDependentCString(location.get()),
                         nullptr,
                         mURI,
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
  rv = NS_NewChannelInternal(getter_AddRefs(newChannel),
                             redirectURI,
                             redirectLoadInfo,
                             nullptr, // aLoadGroup
                             nullptr, // aCallbacks
                             mLoadFlags,
                             ioService);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupReplacementChannel(redirectURI, newChannel, !rewriteToGET,
                               redirectFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  mRedirectChannel = newChannel.forget();

  rv = gHttpHandler->AsyncOnChannelRedirect(this, mRedirectChannel, redirectFlags);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    OnRedirectVerifyCallback(rv);
  }

  return rv;
}

nsresult
InterceptedHttpChannel::RedirectForResponseURL(nsIURI* aResponseURI,
                                               bool aResponseRedirected)
{
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
  nsCOMPtr<nsIInterceptedBodyCallback> bodyCallback = mBodyCallback.forget();

  RefPtr<InterceptedHttpChannel> newChannel =
    CreateForSynthesis(mResponseHead, mBodyReader, bodyCallback,
                       mChannelCreationTime, mChannelCreationTimestamp,
                       mAsyncOpenTime);

  rv = newChannel->Init(aResponseURI, mCaps,
                        static_cast<nsProxyInfo*>(mProxyInfo.get()),
                        mProxyResolveFlags, mProxyURI, mChannelId);

  // If the response has been redirected, propagate all the URLs to content.
  // Thus, the exact value of the redirect flag does not matter as long as it's
  // not REDIRECT_INTERNAL.
  uint32_t flags = aResponseRedirected ? nsIChannelEventSink::REDIRECT_TEMPORARY
                                       : nsIChannelEventSink::REDIRECT_INTERNAL;

  nsCOMPtr<nsILoadInfo> redirectLoadInfo =
    CloneLoadInfoForRedirect(aResponseURI, flags);
  newChannel->SetLoadInfo(redirectLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupReplacementChannel(aResponseURI, newChannel, true, flags);
  NS_ENSURE_SUCCESS(rv, rv);

  mRedirectChannel = newChannel;

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

nsresult
InterceptedHttpChannel::StartPump()
{
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

  nsresult rv = nsInputStreamPump::Create(getter_AddRefs(mPump),
                                          mBodyReader,
                                          0, 0, true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPump->AsyncRead(this, mListenerContext);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t suspendCount = mSuspendCount;
  while (suspendCount--) {
    mPump->Suspend();
  }

  MOZ_DIAGNOSTIC_ASSERT(!mCanceled);

  return rv;
}

nsresult
InterceptedHttpChannel::OpenRedirectChannel()
{
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
  if (mLoadInfo && mLoadInfo->GetEnforceSecurity()) {
    MOZ_ASSERT(!mListenerContext, "mListenerContext should be null!");
    rv = mRedirectChannel->AsyncOpen2(mListener);
  }
  else {
    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  mStatus = NS_BINDING_REDIRECTED;

  return rv;
}

void
InterceptedHttpChannel::MaybeCallStatusAndProgress()
{
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

    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("InterceptedHttpChannel::MaybeCallStatusAndProgress",
                        this,
                        &InterceptedHttpChannel::MaybeCallStatusAndProgress);
    MOZ_ALWAYS_SUCCEEDS(
      SystemGroup::Dispatch(TaskCategory::Other, r.forget()));

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
  if (progress <= mProgressReported ||
      mCanceled ||
      !mProgressSink ||
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

  mProgressSink->OnStatus(this, mListenerContext, NS_NET_STATUS_READING,
                          mStatusHost.get());

  mProgressSink->OnProgress(this, mListenerContext, progress,
                            mSynthesizedStreamLength);

  mProgressReported = progress;
}

void
InterceptedHttpChannel::MaybeCallBodyCallback()
{
  nsCOMPtr<nsIInterceptedBodyCallback> callback = mBodyCallback.forget();
  if (callback) {
    callback->BodyComplete(mStatus);
  }
}

// static
already_AddRefed<InterceptedHttpChannel>
InterceptedHttpChannel::CreateForInterception(PRTime aCreationTime,
                                              const TimeStamp& aCreationTimestamp,
                                              const TimeStamp& aAsyncOpenTimestamp)
{
  // Create an InterceptedHttpChannel that will trigger a FetchEvent
  // in a ServiceWorker when opened.
  RefPtr<InterceptedHttpChannel> ref =
    new InterceptedHttpChannel(aCreationTime, aCreationTimestamp,
                               aAsyncOpenTimestamp);

  return ref.forget();
}

// static
already_AddRefed<InterceptedHttpChannel>
InterceptedHttpChannel::CreateForSynthesis(const nsHttpResponseHead* aHead,
                                           nsIInputStream* aBody,
                                           nsIInterceptedBodyCallback* aBodyCallback,
                                           PRTime aCreationTime,
                                           const TimeStamp& aCreationTimestamp,
                                           const TimeStamp& aAsyncOpenTimestamp)
{
  MOZ_DIAGNOSTIC_ASSERT(aHead);
  MOZ_DIAGNOSTIC_ASSERT(aBody);

  // Create an InterceptedHttpChannel that already has a synthesized response.
  // The synthetic response will be processed when opened.  A FetchEvent
  // will not be triggered.
  RefPtr<InterceptedHttpChannel> ref =
    new InterceptedHttpChannel(aCreationTime, aCreationTimestamp,
                               aAsyncOpenTimestamp);

  ref->mResponseHead = new nsHttpResponseHead(*aHead);
  ref->mBodyReader = aBody;
  ref->mBodyCallback = aBodyCallback;

  return ref.forget();
}

NS_IMETHODIMP
InterceptedHttpChannel::Cancel(nsresult aStatus)
{
  // Note: This class has been designed to send all error results through
  //       Cancel().  Don't add calls directly to AsyncAbort() or
  //       DoNotifyListener().  Instead call Cancel().

  if (mCanceled) {
    return NS_OK;
  }
  mCanceled = true;

  MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(aStatus));
  if (NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }

  // Everything is suspended during diversion until it completes.  Since the
  // intercepted channel could be a long-running stream, we need to request that
  // cancellation be triggered in the child, completing the diversion and
  // allowing cancellation to run to completion.
  if (mDiverting) {
    Unused << mParentChannel->CancelDiversion();
    // (We want the pump to be canceled as well, so don't directly return.)
  }

  if (mPump) {
    return mPump->Cancel(mStatus);
  }

  return AsyncAbort(mStatus);
}

NS_IMETHODIMP
InterceptedHttpChannel::Suspend(void)
{
  nsresult rv = SuspendInternal();

  nsresult rvParentChannel = NS_OK;
  if (mParentChannel) {
    rvParentChannel = mParentChannel->SuspendMessageDiversion();
  }

  return NS_FAILED(rv) ? rv : rvParentChannel;
}

NS_IMETHODIMP
InterceptedHttpChannel::Resume(void)
{
  nsresult rv = ResumeInternal();

  nsresult rvParentChannel = NS_OK;
  if (mParentChannel) {
    rvParentChannel = mParentChannel->ResumeMessageDiversion();
  }

  return NS_FAILED(rv) ? rv : rvParentChannel;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetSecurityInfo(nsISupports** aSecurityInfo)
{
  nsCOMPtr<nsISupports> ref(mSecurityInfo);
  ref.forget(aSecurityInfo);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::AsyncOpen(nsIStreamListener* aListener, nsISupports* aContext)
{
  if (mCanceled) {
    return mStatus;
  }

  // After this point we should try to return NS_OK and notify the listener
  // of the result.
  mListener = aListener;

  AsyncOpenInternal();

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::AsyncOpen2(nsIStreamListener* aListener)
{
  nsCOMPtr<nsIStreamListener> listener(aListener);
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(rv);
    return rv;
  }
  return AsyncOpen(listener, nullptr);
}

NS_IMETHODIMP
InterceptedHttpChannel::LogBlockedCORSRequest(const nsAString& aMessage)
{
  // Synthetic responses should not trigger CORS blocking.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetupFallbackChannel(const char*  aFallbackKey)
{
  // AppCache should not be used with service worker intercepted channels.
  // This should never be called.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetResponseSynthesized(bool* aResponseSynthesized)
{
  *aResponseSynthesized = mResponseHead || mBodyReader;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetPriority(int32_t aPriority)
{
  mPriority = clamped<int32_t>(aPriority, INT16_MIN, INT16_MAX);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetClassFlags(uint32_t aClassFlags)
{
  mClassOfService = aClassFlags;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::ClearClassFlags(uint32_t aClassFlags)
{
  mClassOfService &= ~aClassFlags;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::AddClassFlags(uint32_t aClassFlags)
{
  mClassOfService |= aClassFlags;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::ResumeAt(uint64_t aStartPos,
                                 const nsACString & aEntityId)
{
  // We don't support resuming synthesized responses, but we do track this
  // information so it can be passed on to the resulting nsHttpChannel if
  // ResetInterception is called.
  mResumeStartPos = aStartPos;
  mResumeEntityId = aEntityId;
  return NS_OK;
}

void
InterceptedHttpChannel::DoNotifyListenerCleanup()
{
  // Prefer to cleanup in ReleaseListeners() as it seems to be called
  // more consistently in necko.
}


NS_IMETHODIMP
InterceptedHttpChannel::ResetInterception(void)
{
  if (mCanceled) {
    return mStatus;
  }

  uint32_t flags = nsIChannelEventSink::REDIRECT_INTERNAL;

  nsCOMPtr<nsIChannel> newChannel;
  nsCOMPtr<nsILoadInfo> redirectLoadInfo =
    CloneLoadInfoForRedirect(mURI, flags);
  nsresult rv = NS_NewChannelInternal(getter_AddRefs(newChannel),
                                      mURI,
                                      redirectLoadInfo,
                                      nullptr, // aLoadGroup
                                      nullptr, // aCallbacks
                                      mLoadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupReplacementChannel(mURI, newChannel, true, flags);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRedirectMode != nsIHttpChannelInternal::REDIRECT_MODE_MANUAL) {
    nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
    rv = newChannel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    loadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
    rv = newChannel->SetLoadFlags(loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mRedirectChannel = newChannel.forget();

  rv = gHttpHandler->AsyncOnChannelRedirect(this, mRedirectChannel, flags);

  if (NS_FAILED(rv)) {
    OnRedirectVerifyCallback(rv);
  }

  return rv;
}

NS_IMETHODIMP
InterceptedHttpChannel::SynthesizeStatus(uint16_t aStatus,
                                         const nsACString& aReason)
{
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
                                         const nsACString& aValue)
{
  if (mCanceled) {
    return mStatus;
  }

  if (!mSynthesizedResponseHead) {
    mSynthesizedResponseHead.reset(new nsHttpResponseHead());
  }

  nsAutoCString header = aName + NS_LITERAL_CSTRING(": ") + aValue;
  // Overwrite any existing header.
  nsresult rv = mSynthesizedResponseHead->ParseHeaderLine(header);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::StartSynthesizedResponse(nsIInputStream* aBody,
                                                 nsIInterceptedBodyCallback* aBodyCallback,
                                                 nsICacheInfoChannel* aSynthesizedCacheInfo,
                                                 const nsACString& aFinalURLSpec,
                                                 bool aResponseRedirected)
{
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

  mResponseHead = mSynthesizedResponseHead.release();

  if (ShouldRedirect()) {
    rv = FollowSyntheticRedirect();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // Intercepted responses should already be decoded.
  SetApplyConversion(false);

  // Errors and redirects may not have a body.  Synthesize an empty string stream
  // here so later code can be simpler.
  mBodyReader = aBody;
  if (!mBodyReader) {
    rv = NS_NewCStringInputStream(getter_AddRefs(mBodyReader), EmptyCString());
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
InterceptedHttpChannel::FinishSynthesizedResponse()
{
  if (mCanceled) {
    // Return NS_OK.  The channel should fire callbacks with an error code
    // if it was cancelled before this point.
    return NS_OK;
  }

  // TODO: Remove this API after interception moves to the parent process in
  //       e10s mode.

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::CancelInterception(nsresult aStatus)
{
  return Cancel(aStatus);
}

NS_IMETHODIMP
InterceptedHttpChannel::GetChannel(nsIChannel** aChannel)
{
  nsCOMPtr<nsIChannel> ref(this);
  ref.forget(aChannel);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetSecureUpgradedChannelURI(nsIURI** aSecureUpgradedChannelURI)
{
  nsCOMPtr<nsIURI> ref(mURI);
  ref.forget(aSecureUpgradedChannelURI);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetChannelInfo(mozilla::dom::ChannelInfo* aChannelInfo)
{
  return aChannelInfo->ResurrectInfoOnChannel(this);
}

NS_IMETHODIMP
InterceptedHttpChannel::GetInternalContentPolicyType(nsContentPolicyType* aPolicyType)
{
  if (mLoadInfo) {
    *aPolicyType = mLoadInfo->InternalContentPolicyType();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetConsoleReportCollector(nsIConsoleReportCollector** aConsoleReportCollector)
{
  nsCOMPtr<nsIConsoleReportCollector> ref(this);
  ref.forget(aConsoleReportCollector);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetLaunchServiceWorkerStart(mozilla::TimeStamp* aTimeStamp)
{
  return HttpBaseChannel::GetLaunchServiceWorkerStart(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::SetLaunchServiceWorkerStart(mozilla::TimeStamp aTimeStamp)
{
  return HttpBaseChannel::SetLaunchServiceWorkerStart(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::GetLaunchServiceWorkerEnd(mozilla::TimeStamp* aTimeStamp)
{
  return HttpBaseChannel::GetLaunchServiceWorkerEnd(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::SetLaunchServiceWorkerEnd(mozilla::TimeStamp aTimeStamp)
{
  return HttpBaseChannel::SetLaunchServiceWorkerEnd(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::SetDispatchFetchEventStart(mozilla::TimeStamp aTimeStamp)
{
  return HttpBaseChannel::SetDispatchFetchEventStart(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::SetDispatchFetchEventEnd(mozilla::TimeStamp aTimeStamp)
{
  return HttpBaseChannel::SetDispatchFetchEventEnd(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::SetHandleFetchEventStart(mozilla::TimeStamp aTimeStamp)
{
  return HttpBaseChannel::SetHandleFetchEventStart(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::SetHandleFetchEventEnd(mozilla::TimeStamp aTimeStamp)
{
  return HttpBaseChannel::SetHandleFetchEventEnd(aTimeStamp);
}

NS_IMETHODIMP
InterceptedHttpChannel::SetFinishResponseStart(mozilla::TimeStamp aTimeStamp)
{
  mFinishResponseStart = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetFinishSynthesizedResponseEnd(mozilla::TimeStamp aTimeStamp)
{
  MOZ_ASSERT(mSynthesizedOrReset == Invalid);
  mSynthesizedOrReset = Synthesized;
  mFinishResponseEnd = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetChannelResetEnd(mozilla::TimeStamp aTimeStamp)
{
  MOZ_ASSERT(mSynthesizedOrReset == Invalid);
  mSynthesizedOrReset = Reset;
  mFinishResponseEnd = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SaveTimeStamps(void)
{
  bool isNonSubresourceRequest = nsContentUtils::IsNonSubresourceRequest(this);
  nsCString navigationOrSubresource = isNonSubresourceRequest ?
    NS_LITERAL_CSTRING("navigation") : NS_LITERAL_CSTRING("subresource");

  nsAutoCString subresourceKey(EmptyCString());
  GetSubresourceTimeStampKey(this, subresourceKey);

  // We may have null timestamps if the fetch dispatch runnable was cancelled
  // and we defaulted to resuming the request.
  if (!mFinishResponseStart.IsNull() && !mFinishResponseEnd.IsNull()) {
    Telemetry::HistogramID id = (mSynthesizedOrReset == Synthesized) ?
      Telemetry::SERVICE_WORKER_FETCH_EVENT_FINISH_SYNTHESIZED_RESPONSE_MS :
      Telemetry::SERVICE_WORKER_FETCH_EVENT_CHANNEL_RESET_MS;
    Telemetry::Accumulate(id, navigationOrSubresource,
      static_cast<uint32_t>((mFinishResponseEnd - mFinishResponseStart).ToMilliseconds()));
    if (!isNonSubresourceRequest && !subresourceKey.IsEmpty()) {
      Telemetry::Accumulate(id, subresourceKey,
        static_cast<uint32_t>((mFinishResponseEnd - mFinishResponseStart).ToMilliseconds()));
    }
  }

  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS,
    navigationOrSubresource,
    static_cast<uint32_t>((mHandleFetchEventStart - mDispatchFetchEventStart).ToMilliseconds()));

  if (!isNonSubresourceRequest && !subresourceKey.IsEmpty()) {
    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS,
      subresourceKey,
      static_cast<uint32_t>((mHandleFetchEventStart - mDispatchFetchEventStart).ToMilliseconds()));
  }

  if (!mFinishResponseEnd.IsNull()) {
    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS,
      navigationOrSubresource,
      static_cast<uint32_t>((mFinishResponseEnd - mDispatchFetchEventStart).ToMilliseconds()));
    if (!isNonSubresourceRequest && !subresourceKey.IsEmpty()) {
      Telemetry::Accumulate(Telemetry::SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS,
        subresourceKey,
        static_cast<uint32_t>((mFinishResponseEnd - mDispatchFetchEventStart).ToMilliseconds()));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetReleaseHandle(nsISupports* aHandle)
{
  mReleaseHandle = aHandle;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnRedirectVerifyCallback(nsresult rv)
{
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

  mIsPending = false;
  ReleaseListeners();

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnStartRequest(nsIRequest* aRequest,
                                       nsISupports* aContext)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mProgressSink) {
    GetCallback(mProgressSink);
  }

  if (mPump && mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS) {
    mPump->PeekStream(CallTypeSniffers, static_cast<nsIChannel*>(this));
  }

  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnStopRequest(nsIRequest* aRequest,
                                      nsISupports* aContext,
                                      nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }

  MaybeCallBodyCallback();

  // Its possible that we have any async runnable queued to report some
  // progress when OnStopRequest() is triggered.  Report any left over
  // progress immediately.  The extra runnable will then do nothing thanks
  // to the ReleaseListeners() call below.
  MaybeCallStatusAndProgress();

  mIsPending = false;

  // Register entry to the Performance resource timing
  mozilla::dom::Performance* documentPerformance = GetPerformance();
  if (documentPerformance) {
    documentPerformance->AddEntry(this, this);
  }

  if (mListener) {
    mListener->OnStopRequest(this, mListenerContext, mStatus);
  }

  gHttpHandler->OnStopRequest(this);

  ReleaseListeners();

  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::OnDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInputStream,
                                        uint64_t aOffset,
                                        uint32_t aCount)
{
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

  return mListener->OnDataAvailable(this, mListenerContext, aInputStream,
                                    aOffset, aCount);
}

NS_IMETHODIMP
InterceptedHttpChannel::MessageDiversionStarted(ADivertableParentChannel* aParentChannel)
{
  MOZ_ASSERT(!mParentChannel);
  mParentChannel = aParentChannel;
  mDiverting = true;
  uint32_t suspendCount = mSuspendCount;
  while(suspendCount--) {
    mParentChannel->SuspendMessageDiversion();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::MessageDiversionStop()
{
  MOZ_ASSERT(mParentChannel);
  mParentChannel = nullptr;
  mDiverting = false;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::SuspendInternal()
{
  ++mSuspendCount;
  if (mPump) {
    return mPump->Suspend();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::ResumeInternal()
{
  --mSuspendCount;
  if (mPump) {
    return mPump->Resume();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::RetargetDeliveryTo(nsIEventTarget* aNewTarget)
{
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
InterceptedHttpChannel::GetDeliveryTarget(nsIEventTarget** aEventTarget)
{
  if (!mPump) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mPump->GetDeliveryTarget(aEventTarget);
}

NS_IMETHODIMP
InterceptedHttpChannel::CheckListenerChain()
{
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
// interface, we tranfers parameters to the saved nsICacheInfoChannel(mSynthesizedCacheInfo)
// from StartSynthesizedResponse. And we return false in IsFromCache and
// NS_ERROR_NOT_AVAILABLE for all other methods while the saved
// mSynthesizedCacheInfo does not exist.
NS_IMETHODIMP
InterceptedHttpChannel::IsFromCache(bool *value)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->IsFromCache(value);
  }
  *value = false;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheEntryId(uint64_t *aCacheEntryId)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheEntryId(aCacheEntryId);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheTokenFetchCount(int32_t *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenFetchCount(_retval);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheTokenExpirationTime(uint32_t *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenExpirationTime(_retval);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheTokenCachedCharset(nsACString &_retval)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheTokenCachedCharset(_retval);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetCacheTokenCachedCharset(const nsACString &aCharset)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetCacheTokenCachedCharset(aCharset);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetAllowStaleCacheContent(bool aAllowStaleCacheContent)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetAllowStaleCacheContent(aAllowStaleCacheContent);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetAllowStaleCacheContent(bool *aAllowStaleCacheContent)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetAllowStaleCacheContent(aAllowStaleCacheContent);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::PreferAlternativeDataType(const nsACString & aType)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();
  mPreferredCachedAltDataType = aType;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetPreferredAlternativeDataType(nsACString & aType)
{
  aType = mPreferredCachedAltDataType;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetAlternativeDataType(nsACString & aType)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetAlternativeDataType(aType);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::OpenAlternativeOutputStream(const nsACString & type, nsIOutputStream * *_retval)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->OpenAlternativeOutputStream(type, _retval);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::GetCacheKey(nsISupports **key)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->GetCacheKey(key);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
InterceptedHttpChannel::SetCacheKey(nsISupports *key)
{
  if (mSynthesizedCacheInfo) {
    return mSynthesizedCacheInfo->SetCacheKey(key);
  }
  return NS_ERROR_NOT_AVAILABLE;
}

} // namespace net
} // namespace mozilla
