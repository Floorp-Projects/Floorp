/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPACMan.h"
#include "nsThreadUtils.h"
#include "nsIDNSService.h"
#include "nsIDNSListener.h"
#include "nsICancelable.h"
#include "nsIAuthPrompt.h"
#include "nsIPromptFactory.h"
#include "nsIHttpChannel.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "prmon.h"
#include "nsIAsyncVerifyRedirectCallback.h"

//-----------------------------------------------------------------------------

// Check to see if the underlying request was not an error page in the case of
// a HTTP request.  For other types of channels, just return true.
static bool
HttpRequestSucceeded(nsIStreamLoader *loader)
{
  nsCOMPtr<nsIRequest> request;
  loader->GetRequest(getter_AddRefs(request));

  bool result = true;  // default to assuming success

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel)
    httpChannel->GetRequestSucceeded(&result);

  return result;
}

//-----------------------------------------------------------------------------

// These objects are stored in nsPACMan::mPendingQ

class PendingPACQuery : public PRCList, public nsIDNSListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  PendingPACQuery(nsPACMan *pacMan, nsIURI *uri, nsPACManCallback *callback)
    : mPACMan(pacMan)
    , mURI(uri)
    , mCallback(callback)
  {
    PR_INIT_CLIST(this);
  }

  nsresult Start(PRUint32 flags);
  void     Complete(nsresult status, const nsCString &pacString);

private:
  nsPACMan                  *mPACMan;  // weak reference
  nsCOMPtr<nsIURI>           mURI;
  nsRefPtr<nsPACManCallback> mCallback;
  nsCOMPtr<nsICancelable>    mDNSRequest;
};

// This is threadsafe because we implement nsIDNSListener
NS_IMPL_THREADSAFE_ISUPPORTS1(PendingPACQuery, nsIDNSListener)

nsresult
PendingPACQuery::Start(PRUint32 flags)
{
  if (mDNSRequest)
    return NS_OK;  // already started

  nsresult rv;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("unable to get the DNS service");
    return rv;
  }

  nsCAutoString host;
  rv = mURI->GetAsciiHost(host);
  if (NS_FAILED(rv))
    return rv;

  rv = dns->AsyncResolve(host, flags, this, NS_GetCurrentThread(),
                         getter_AddRefs(mDNSRequest));
  if (NS_FAILED(rv))
    NS_WARNING("DNS AsyncResolve failed");

  return rv;
}

// This may be called before or after OnLookupComplete
void
PendingPACQuery::Complete(nsresult status, const nsCString &pacString)
{
  if (!mCallback)
    return;

  mCallback->OnQueryComplete(status, pacString);
  mCallback = nsnull;

  if (mDNSRequest) {
    mDNSRequest->Cancel(NS_ERROR_ABORT);
    mDNSRequest = nsnull;
  }
}

NS_IMETHODIMP
PendingPACQuery::OnLookupComplete(nsICancelable *request,
                                  nsIDNSRecord *record,
                                  nsresult status)
{
  // NOTE: we don't care about the results of this DNS query.  We issued
  //       this DNS query just to pre-populate our DNS cache.
 
  mDNSRequest = nsnull;  // break reference cycle

  // If we've already completed this query then do nothing.
  if (!mCallback)
    return NS_OK;

  // We're no longer pending, so we can remove ourselves.
  PR_REMOVE_LINK(this);

  nsCAutoString pacString;
  status = mPACMan->GetProxyForURI(mURI, pacString);
  Complete(status, pacString);

  NS_RELEASE_THIS();
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsPACMan::nsPACMan()
  : mLoadPending(false)
  , mShutdown(false)
  , mScheduledReload(LL_MAXINT)
  , mLoadFailureCount(0)
{
  PR_INIT_CLIST(&mPendingQ);
}

nsPACMan::~nsPACMan()
{
  NS_ASSERTION(mLoader == nsnull, "pac man not shutdown properly");
  NS_ASSERTION(mPAC == nsnull, "pac man not shutdown properly");
  NS_ASSERTION(PR_CLIST_IS_EMPTY(&mPendingQ), "pac man not shutdown properly");
}

void
nsPACMan::Shutdown()
{
  CancelExistingLoad();
  ProcessPendingQ(NS_ERROR_ABORT);

  mPAC = nsnull;
  mShutdown = true;
}

nsresult
nsPACMan::GetProxyForURI(nsIURI *uri, nsACString &result)
{
  NS_ENSURE_STATE(!mShutdown);

  if (IsPACURI(uri)) {
    result.Truncate();
    return NS_OK;
  }

  MaybeReloadPAC();

  if (IsLoading())
    return NS_ERROR_IN_PROGRESS;
  if (!mPAC)
    return NS_ERROR_NOT_AVAILABLE;

  nsCAutoString spec, host;
  uri->GetAsciiSpec(spec);
  uri->GetAsciiHost(host);

  return mPAC->GetProxyForURI(spec, host, result);
}

nsresult
nsPACMan::AsyncGetProxyForURI(nsIURI *uri, nsPACManCallback *callback)
{
  NS_ENSURE_STATE(!mShutdown);

  MaybeReloadPAC();

  PendingPACQuery *query = new PendingPACQuery(this, uri, callback);
  if (!query)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(query);
  PR_APPEND_LINK(query, &mPendingQ);

  // If we're waiting for the PAC file to load, then delay starting the query.
  // See OnStreamComplete.  However, if this is the PAC URI then query right
  // away since we know the result will be DIRECT.  We could shortcut some code
  // in this case by issuing the callback directly from here, but that would
  // require extra code, so we just go through the usual async code path.
  int isPACURI = IsPACURI(uri);

  if (IsLoading() && !isPACURI)
    return NS_OK;

  nsresult rv = query->Start(isPACURI ? 0 : nsIDNSService::RESOLVE_SPECULATE);
  if (rv == NS_ERROR_DNS_LOOKUP_QUEUE_FULL && !isPACURI) {
    query->OnLookupComplete(NULL, NULL, NS_OK);
    rv = NS_OK;
  } else if (NS_FAILED(rv)) {
    NS_WARNING("failed to start PAC query");
    PR_REMOVE_LINK(query);
    NS_RELEASE(query);
  }

  return rv;
}

nsresult
nsPACMan::LoadPACFromURI(nsIURI *pacURI)
{
  NS_ENSURE_STATE(!mShutdown);
  NS_ENSURE_ARG(pacURI || mPACURI);

  nsCOMPtr<nsIStreamLoader> loader =
      do_CreateInstance(NS_STREAMLOADER_CONTRACTID);
  NS_ENSURE_STATE(loader);

  // Since we might get called from nsProtocolProxyService::Init, we need to
  // post an event back to the main thread before we try to use the IO service.
  //
  // But, we need to flag ourselves as loading, so that we queue up any PAC
  // queries the enter between now and when we actually load the PAC file.

  if (!mLoadPending) {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &nsPACMan::StartLoading);
    nsresult rv;
    if (NS_FAILED(rv = NS_DispatchToCurrentThread(event)))
      return rv;
    mLoadPending = true;
  }

  CancelExistingLoad();

  mLoader = loader;
  if (pacURI) {
    mPACURI = pacURI;
    mLoadFailureCount = 0;  // reset
  }
  mScheduledReload = LL_MAXINT;
  mPAC = nsnull;
  return NS_OK;
}

void
nsPACMan::StartLoading()
{
  mLoadPending = false;

  // CancelExistingLoad was called...
  if (!mLoader) {
    ProcessPendingQ(NS_ERROR_ABORT);
    return;
  }

  if (NS_SUCCEEDED(mLoader->Init(this))) {
    // Always hit the origin server when loading PAC.
    nsCOMPtr<nsIIOService> ios = do_GetIOService();
    if (ios) {
      nsCOMPtr<nsIChannel> channel;

      // NOTE: This results in GetProxyForURI being called
      ios->NewChannelFromURI(mPACURI, getter_AddRefs(channel));

      if (channel) {
        channel->SetLoadFlags(nsIRequest::LOAD_BYPASS_CACHE);
        channel->SetNotificationCallbacks(this);
        if (NS_SUCCEEDED(channel->AsyncOpen(mLoader, nsnull)))
          return;
      }
    }
  }

  CancelExistingLoad();
  ProcessPendingQ(NS_ERROR_UNEXPECTED);
}

void
nsPACMan::MaybeReloadPAC()
{
  if (!mPACURI)
    return;

  if (PR_Now() > mScheduledReload)
    LoadPACFromURI(nsnull);
}

void
nsPACMan::OnLoadFailure()
{
  PRInt32 minInterval = 5;    // 5 seconds
  PRInt32 maxInterval = 300;  // 5 minutes

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->GetIntPref("network.proxy.autoconfig_retry_interval_min",
                      &minInterval);
    prefs->GetIntPref("network.proxy.autoconfig_retry_interval_max",
                      &maxInterval);
  }

  PRInt32 interval = minInterval << mLoadFailureCount++;  // seconds
  if (!interval || interval > maxInterval)
    interval = maxInterval;

#ifdef DEBUG
  printf("PAC load failure: will retry in %d seconds\n", interval);
#endif

  mScheduledReload = PR_Now() + PRInt64(interval) * PR_USEC_PER_SEC;
}

void
nsPACMan::CancelExistingLoad()
{
  if (mLoader) {
    nsCOMPtr<nsIRequest> request;
    mLoader->GetRequest(getter_AddRefs(request));
    if (request)
      request->Cancel(NS_ERROR_ABORT);
    mLoader = nsnull;
  }
}

void
nsPACMan::ProcessPendingQ(nsresult status)
{
  // Now, start any pending queries
  PRCList *node = PR_LIST_HEAD(&mPendingQ);
  while (node != &mPendingQ) {
    PendingPACQuery *query = static_cast<PendingPACQuery *>(node);
    node = PR_NEXT_LINK(node);
    if (NS_SUCCEEDED(status)) {
      // keep the query in the list (so we can complete it from Shutdown if
      // necessary).
      status = query->Start(nsIDNSService::RESOLVE_SPECULATE);
    }
    if (status == NS_ERROR_DNS_LOOKUP_QUEUE_FULL) {
      query->OnLookupComplete(NULL, NULL, NS_OK);
      status = NS_OK;
    } else if (NS_FAILED(status)) {
      // remove the query from the list
      PR_REMOVE_LINK(query);
      query->Complete(status, EmptyCString());
      NS_RELEASE(query);
    }
  }
}

NS_IMPL_ISUPPORTS3(nsPACMan, nsIStreamLoaderObserver, nsIInterfaceRequestor,
                   nsIChannelEventSink)

NS_IMETHODIMP
nsPACMan::OnStreamComplete(nsIStreamLoader *loader,
                           nsISupports *context,
                           nsresult status,
                           PRUint32 dataLen,
                           const PRUint8 *data)
{
  if (mLoader != loader) {
    // If this happens, then it means that LoadPACFromURI was called more
    // than once before the initial call completed.  In this case, status
    // should be NS_ERROR_ABORT, and if so, then we know that we can and
    // should delay any processing.
    if (status == NS_ERROR_ABORT)
      return NS_OK;
  }

  mLoader = nsnull;

  if (NS_SUCCEEDED(status) && HttpRequestSucceeded(loader)) {
    // Get the URI spec used to load this PAC script.
    nsCAutoString pacURI;
    {
      nsCOMPtr<nsIRequest> request;
      loader->GetRequest(getter_AddRefs(request));
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri)
          uri->GetAsciiSpec(pacURI);
      }
    }

    if (!mPAC) {
      mPAC = do_CreateInstance(NS_PROXYAUTOCONFIG_CONTRACTID, &status);
      if (!mPAC)
        NS_WARNING("failed to instantiate PAC component");
    }
    if (NS_SUCCEEDED(status)) {
      // We assume that the PAC text is ASCII (or ISO-Latin-1).  We've had this
      // assumption forever, and some real-world PAC scripts actually have some
      // non-ASCII text in comment blocks (see bug 296163).
      const char *text = (const char *) data;
      status = mPAC->Init(pacURI, NS_ConvertASCIItoUTF16(text, dataLen));
    }

    // Even if the PAC file could not be parsed, we did succeed in loading the
    // data for it.
    mLoadFailureCount = 0;
  } else {
    // We were unable to load the PAC file (presumably because of a network
    // failure).  Try again a little later.
    OnLoadFailure();
  }

  // Reset mPAC if necessary
  if (mPAC && NS_FAILED(status))
    mPAC = nsnull;

  ProcessPendingQ(status);
  return NS_OK;
}

NS_IMETHODIMP
nsPACMan::GetInterface(const nsIID &iid, void **result)
{
  // In case loading the PAC file requires authentication.
  if (iid.Equals(NS_GET_IID(nsIAuthPrompt))) {
    nsCOMPtr<nsIPromptFactory> promptFac = do_GetService("@mozilla.org/prompter;1");
    NS_ENSURE_TRUE(promptFac, NS_ERROR_FAILURE);
    return promptFac->GetPrompt(nsnull, iid, reinterpret_cast<void**>(result));
  }

  // In case loading the PAC file results in a redirect.
  if (iid.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *result = static_cast<nsIChannelEventSink *>(this);
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsPACMan::AsyncOnChannelRedirect(nsIChannel *oldChannel, nsIChannel *newChannel,
                                 PRUint32 flags,
                                 nsIAsyncVerifyRedirectCallback *callback)
{
  nsresult rv = NS_OK;
  if (NS_FAILED((rv = newChannel->GetURI(getter_AddRefs(mPACURI)))))
      return rv;

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}
