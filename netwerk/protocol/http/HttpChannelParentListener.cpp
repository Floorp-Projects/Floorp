/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpChannelParentListener.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/unused.h"
#include "nsIRedirectChannelRegistrar.h"
#include "nsIHttpEventSink.h"

using mozilla::unused;

namespace mozilla {
namespace net {

HttpChannelParentListener::HttpChannelParentListener(HttpChannelParent* aInitialChannel)
  : mNextListener(aInitialChannel)
  , mRedirectChannelId(0)
  , mSuspendedForDiversion(false)
{
}

HttpChannelParentListener::~HttpChannelParentListener()
{
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS5(HttpChannelParentListener,
                   nsIInterfaceRequestor,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIChannelEventSink,
                   nsIRedirectResultListener)

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

  // Register the new channel and obtain id for it
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      do_GetService("@mozilla.org/redirectchannelregistrar;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = registrar->RegisterChannel(newChannel, &mRedirectChannelId);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Registered %p channel under id=%d", newChannel, mRedirectChannelId));

  nsCOMPtr<nsIParentRedirectingChannel> activeRedirectingChannel =
      do_QueryInterface(mNextListener);
  if (!activeRedirectingChannel) {
    NS_RUNTIMEABORT("Channel got a redirect response, but doesn't implement "
                    "nsIParentRedirectingChannel to handle it.");
  }

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
  } else if (redirectChannel) {
    // Delete the redirect target channel: continue using old channel
    redirectChannel->Delete();
  }

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

}} // mozilla::net
