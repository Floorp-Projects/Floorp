/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpChannelParentListener.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/Unused.h"
#include "nsIRedirectChannelRegistrar.h"
#include "nsIHttpEventSink.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsQueryObject.h"

using mozilla::Unused;

namespace mozilla {
namespace net {

HttpChannelParentListener::HttpChannelParentListener(HttpChannelParent* aInitialChannel)
  : mNextListener(aInitialChannel)
  , mRedirectChannelId(0)
  , mSuspendedForDiversion(false)
  , mShouldIntercept(false)
  , mShouldSuspendIntercept(false)
{
}

HttpChannelParentListener::~HttpChannelParentListener()
{
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(HttpChannelParentListener)
NS_IMPL_RELEASE(HttpChannelParentListener)
NS_INTERFACE_MAP_BEGIN(HttpChannelParentListener)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIRedirectResultListener)
  NS_INTERFACE_MAP_ENTRY(nsINetworkInterceptController)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
  if (aIID.Equals(NS_GET_IID(HttpChannelParentListener))) {
    foundInterface = static_cast<nsIInterfaceRequestor*>(this);
  } else
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
    "Cannot call OnStartRequest if suspended for diversion!");

  if (!mNextListener)
    return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnStartRequest [this=%p]\n", this));
  return mNextListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
HttpChannelParentListener::OnStopRequest(nsIRequest *aRequest,
                                         nsISupports *aContext,
                                         nsresult aStatusCode)
{
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
    "Cannot call OnStopRequest if suspended for diversion!");

  if (!mNextListener)
    return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnStopRequest: [this=%p status=%ul]\n",
       this, aStatusCode));
  nsresult rv = mNextListener->OnStopRequest(aRequest, aContext, aStatusCode);

  mNextListener = nullptr;
  return rv;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnDataAvailable(nsIRequest *aRequest,
                                           nsISupports *aContext,
                                           nsIInputStream *aInputStream,
                                           uint64_t aOffset,
                                           uint32_t aCount)
{
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
    "Cannot call OnDataAvailable if suspended for diversion!");

  if (!mNextListener)
    return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnDataAvailable [this=%p]\n", this));
  return mNextListener->OnDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::GetInterface(const nsIID& aIID, void **result)
{
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink)) ||
      aIID.Equals(NS_GET_IID(nsIHttpEventSink))  ||
      aIID.Equals(NS_GET_IID(nsINetworkInterceptController))  ||
      aIID.Equals(NS_GET_IID(nsIRedirectResultListener)))
  {
    return QueryInterface(aIID, result);
  }

  nsCOMPtr<nsIInterfaceRequestor> ir;
  if (mNextListener &&
      NS_SUCCEEDED(CallQueryInterface(mNextListener.get(),
                                      getter_AddRefs(ir))))
  {
    return ir->GetInterface(aIID, result);
  }

  return NS_NOINTERFACE;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::AsyncOnChannelRedirect(
                                    nsIChannel *oldChannel,
                                    nsIChannel *newChannel,
                                    uint32_t redirectFlags,
                                    nsIAsyncVerifyRedirectCallback* callback)
{
  nsresult rv;

  nsCOMPtr<nsIParentRedirectingChannel> activeRedirectingChannel =
      do_QueryInterface(mNextListener);
  if (!activeRedirectingChannel) {
    NS_ERROR("Channel got a redirect response, but doesn't implement "
             "nsIParentRedirectingChannel to handle it.");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Register the new channel and obtain id for it
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      do_GetService("@mozilla.org/redirectchannelregistrar;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = registrar->RegisterChannel(newChannel, &mRedirectChannelId);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Registered %p channel under id=%d", newChannel, mRedirectChannelId));

  return activeRedirectingChannel->StartRedirect(mRedirectChannelId,
                                                 newChannel,
                                                 redirectFlags,
                                                 callback);
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnRedirectResult(bool succeeded)
{
  nsresult rv;

  nsCOMPtr<nsIParentChannel> redirectChannel;
  if (mRedirectChannelId) {
    nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
        do_GetService("@mozilla.org/redirectchannelregistrar;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = registrar->GetParentChannel(mRedirectChannelId,
                                     getter_AddRefs(redirectChannel));
    if (NS_FAILED(rv) || !redirectChannel) {
      // Redirect might get canceled before we got AsyncOnChannelRedirect
      LOG(("Registered parent channel not found under id=%d", mRedirectChannelId));

      nsCOMPtr<nsIChannel> newChannel;
      rv = registrar->GetRegisteredChannel(mRedirectChannelId,
                                           getter_AddRefs(newChannel));
      MOZ_ASSERT(newChannel, "Already registered channel not found");

      if (NS_SUCCEEDED(rv))
        newChannel->Cancel(NS_BINDING_ABORTED);
    }

    // Release all previously registered channels, they are no longer need to be
    // kept in the registrar from this moment.
    registrar->DeregisterChannels(mRedirectChannelId);

    mRedirectChannelId = 0;
  }

  if (!redirectChannel) {
    succeeded = false;
  }

  nsCOMPtr<nsIParentRedirectingChannel> activeRedirectingChannel =
      do_QueryInterface(mNextListener);
  MOZ_ASSERT(activeRedirectingChannel,
    "Channel finished a redirect response, but doesn't implement "
    "nsIParentRedirectingChannel to complete it.");

  if (activeRedirectingChannel) {
    activeRedirectingChannel->CompleteRedirect(succeeded);
  } else {
    succeeded = false;
  }

  if (succeeded) {
    // Switch to redirect channel and delete the old one.
    nsCOMPtr<nsIParentChannel> parent;
    parent = do_QueryInterface(mNextListener);
    MOZ_ASSERT(parent);
    parent->Delete();
    mNextListener = do_QueryInterface(redirectChannel);
    MOZ_ASSERT(mNextListener);
    redirectChannel->SetParentListener(this);
  } else if (redirectChannel) {
    // Delete the redirect target channel: continue using old channel
    redirectChannel->Delete();
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsINetworkInterceptController
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::ShouldPrepareForIntercept(nsIURI* aURI,
                                                     bool aIsNonSubresourceRequest,
                                                     bool* aShouldIntercept)
{
  *aShouldIntercept = mShouldIntercept;
  return NS_OK;
}

class HeaderVisitor final : public nsIHttpHeaderVisitor
{
  nsCOMPtr<nsIInterceptedChannel> mChannel;
  ~HeaderVisitor()
  {
  }
public:
  explicit HeaderVisitor(nsIInterceptedChannel* aChannel) : mChannel(aChannel)
  {
  }

  NS_DECL_ISUPPORTS

  NS_IMETHOD VisitHeader(const nsACString& aHeader, const nsACString& aValue) override
  {
    mChannel->SynthesizeHeader(aHeader, aValue);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(HeaderVisitor, nsIHttpHeaderVisitor)

class FinishSynthesizedResponse : public Runnable
{
  nsCOMPtr<nsIInterceptedChannel> mChannel;
public:
  explicit FinishSynthesizedResponse(nsIInterceptedChannel* aChannel)
  : mChannel(aChannel)
  {
  }

  NS_IMETHOD Run() override
  {
    // The URL passed as an argument here doesn't matter, since the child will
    // receive a redirection notification as a result of this synthesized response.
    mChannel->FinishSynthesizedResponse(EmptyCString());
    return NS_OK;
  }
};

NS_IMETHODIMP
HttpChannelParentListener::ChannelIntercepted(nsIInterceptedChannel* aChannel)
{
  if (mShouldSuspendIntercept) {
    mInterceptedChannel = aChannel;
    return NS_OK;
  }

  nsAutoCString statusText;
  mSynthesizedResponseHead->StatusText(statusText);
  aChannel->SynthesizeStatus(mSynthesizedResponseHead->Status(), statusText);
  nsCOMPtr<nsIHttpHeaderVisitor> visitor = new HeaderVisitor(aChannel);
  mSynthesizedResponseHead->VisitHeaders(visitor,
                                         nsHttpHeaderArray::eFilterResponse);

  nsCOMPtr<nsIRunnable> event = new FinishSynthesizedResponse(aChannel);
  NS_DispatchToCurrentThread(event);

  mSynthesizedResponseHead = nullptr;

  MOZ_ASSERT(mNextListener);
  RefPtr<HttpChannelParent> channel = do_QueryObject(mNextListener);
  MOZ_ASSERT(channel);
  channel->ResponseSynthesized();

  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult
HttpChannelParentListener::SuspendForDiversion()
{
  if (NS_WARN_IF(mSuspendedForDiversion)) {
    MOZ_ASSERT(!mSuspendedForDiversion, "Cannot SuspendForDiversion twice!");
    return NS_ERROR_UNEXPECTED;
  }

  // While this is set, no OnStart/OnData/OnStop callbacks should be forwarded
  // to mNextListener.
  mSuspendedForDiversion = true;

  return NS_OK;
}

nsresult
HttpChannelParentListener::ResumeForDiversion()
{
  MOZ_RELEASE_ASSERT(mSuspendedForDiversion, "Must already be suspended!");

  // Allow OnStart/OnData/OnStop callbacks to be forwarded to mNextListener.
  mSuspendedForDiversion = false;

  return NS_OK;
}

nsresult
HttpChannelParentListener::DivertTo(nsIStreamListener* aListener)
{
  MOZ_ASSERT(aListener);
  MOZ_RELEASE_ASSERT(mSuspendedForDiversion, "Must already be suspended!");

  mNextListener = aListener;

  return ResumeForDiversion();
}

void
HttpChannelParentListener::SetupInterception(const nsHttpResponseHead& aResponseHead)
{
  mSynthesizedResponseHead = new nsHttpResponseHead(aResponseHead);
  mShouldIntercept = true;
}

void
HttpChannelParentListener::SetupInterceptionAfterRedirect(bool aShouldIntercept)
{
  mShouldIntercept = aShouldIntercept;
  if (mShouldIntercept) {
    // When an interception occurs, this channel should suspend all further activity.
    // It will be torn down and recreated if necessary.
    mShouldSuspendIntercept = true;
  }
}

void
HttpChannelParentListener::ClearInterceptedChannel()
{
  if (mInterceptedChannel) {
    mInterceptedChannel->Cancel(NS_ERROR_INTERCEPTION_FAILED);
    mInterceptedChannel = nullptr;
  }
}

} // namespace net
} // namespace mozilla
