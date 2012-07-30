/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpChannelParentListener.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/unused.h"
#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsIAuthPromptProvider.h"
#include "nsIDocShellTreeItem.h"
#include "nsIBadCertListener2.h"
#include "nsICacheEntryDescriptor.h"
#include "nsSerializationHelper.h"
#include "nsISerializable.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsISecureBrowserUI.h"
#include "nsIRedirectChannelRegistrar.h"

#include "nsIFTPChannel.h"

using mozilla::unused;

namespace mozilla {
namespace net {

HttpChannelParentListener::HttpChannelParentListener(HttpChannelParent* aInitialChannel)
  : mActiveChannel(aInitialChannel)
  , mRedirectChannelId(0)
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
  if (!mActiveChannel)
    return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnStartRequest [this=%x]\n", this));
  return mActiveChannel->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
HttpChannelParentListener::OnStopRequest(nsIRequest *aRequest, 
                                          nsISupports *aContext, 
                                          nsresult aStatusCode)
{
  if (!mActiveChannel)
    return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnStopRequest: [this=%x status=%ul]\n", 
       this, aStatusCode));
  nsresult rv = mActiveChannel->OnStopRequest(aRequest, aContext, aStatusCode);

  mActiveChannel = nullptr;
  return rv;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnDataAvailable(nsIRequest *aRequest, 
                                            nsISupports *aContext, 
                                            nsIInputStream *aInputStream, 
                                            PRUint32 aOffset, 
                                            PRUint32 aCount)
{
  if (!mActiveChannel)
    return NS_ERROR_UNEXPECTED;

  LOG(("HttpChannelParentListener::OnDataAvailable [this=%x]\n", this));
  return mActiveChannel->OnDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
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
  if (mActiveChannel &&
      NS_SUCCEEDED(CallQueryInterface(mActiveChannel.get(),
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
                                    PRUint32 redirectFlags,
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
      do_QueryInterface(mActiveChannel);
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
      NS_ASSERTION(newChannel, "Already registered channel not found");

      if (NS_SUCCEEDED(rv))
        newChannel->Cancel(NS_BINDING_ABORTED);
    }

    // Release all previously registered channels, they are no longer need to be
    // kept in the registrar from this moment.
    registrar->DeregisterChannels(mRedirectChannelId);

    mRedirectChannelId = 0;
  }

  nsCOMPtr<nsIParentRedirectingChannel> activeRedirectingChannel =
      do_QueryInterface(mActiveChannel);
  NS_ABORT_IF_FALSE(activeRedirectingChannel,
    "Channel finished a redirect response, but doesn't implement "
    "nsIParentRedirectingChannel to complete it.");

  activeRedirectingChannel->CompleteRedirect(succeeded);

  if (succeeded) {
    // Switch to redirect channel and delete the old one.
    mActiveChannel->Delete();
    mActiveChannel = redirectChannel;
  } else if (redirectChannel) {
    // Delete the redirect target channel: continue using old channel
    redirectChannel->Delete();
  }

  return NS_OK;
}

}} // mozilla::net
