/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Duell <jduell.mcbugs@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/dom/TabParent.h"
#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsIAuthPromptProvider.h"
#include "nsIDocShellTreeItem.h"
#include "nsIBadCertListener2.h"
#include "nsICacheEntryDescriptor.h"

namespace mozilla {
namespace net {

// C++ file contents
HttpChannelParent::HttpChannelParent(PBrowserParent* iframeEmbedding)
: mIPCClosed(false)
{
  // Ensure gHttpHandler is initialized: we need the atom table up and running.
  nsIHttpProtocolHandler* handler;
  CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &handler);
  NS_ASSERTION(handler, "no http handler");

  mTabParent = do_QueryInterface(static_cast<nsITabParent*>(
      static_cast<TabParent*>(iframeEmbedding)));
}

HttpChannelParent::~HttpChannelParent()
{
  gHttpHandler->Release();
}

void
HttpChannelParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if nsHttpChannel hasn't called OnStopRequest
  // yet, but we must not send any more msgs to child.
  mIPCClosed = true;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(HttpChannelParent, 
                   nsIRequestObserver, 
                   nsIStreamListener,
                   nsIInterfaceRequestor,
                   nsIProgressEventSink);

//-----------------------------------------------------------------------------
// HttpChannelParent::PHttpChannelParent
//-----------------------------------------------------------------------------

bool 
HttpChannelParent::RecvAsyncOpen(const IPC::URI&            aURI,
                                 const IPC::URI&            aOriginalURI,
                                 const IPC::URI&            aDocURI,
                                 const IPC::URI&            aReferrerURI,
                                 const PRUint32&            loadFlags,
                                 const RequestHeaderTuples& requestHeaders,
                                 const nsHttpAtom&          requestMethod,
                                 const nsCString&           uploadStreamData,
                                 const PRInt32&             uploadStreamInfo,
                                 const PRUint16&            priority,
                                 const PRUint8&             redirectionLimit,
                                 const PRBool&              allowPipelining,
                                 const PRBool&              forceAllowThirdPartyCookie)
{
  nsCOMPtr<nsIURI> uri = aURI;
  nsCOMPtr<nsIURI> originalUri = aOriginalURI;
  nsCOMPtr<nsIURI> docUri = aDocURI;
  nsCOMPtr<nsIURI> referrerUri = aReferrerURI;
  
  nsCString uriSpec;
  uri->GetSpec(uriSpec);
  LOG(("HttpChannelParent RecvAsyncOpen [this=%x uri=%s]\n", 
       this, uriSpec.get()));

  nsresult rv;

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return false;       // TODO: cancel request (bug 536317), return true

  rv = NS_NewChannel(getter_AddRefs(mChannel), uri, ios, nsnull, nsnull, loadFlags);
  if (NS_FAILED(rv))
    return false;       // TODO: cancel request (bug 536317), return true

  nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
  httpChan->SetRemoteChannel(true);

  if (originalUri)
    httpChan->SetOriginalURI(originalUri);
  if (docUri)
    httpChan->SetDocumentURI(docUri);
  if (referrerUri)
    httpChan->SetReferrerInternal(referrerUri);
  if (loadFlags != nsIRequest::LOAD_NORMAL)
    httpChan->SetLoadFlags(loadFlags);

  for (PRUint32 i = 0; i < requestHeaders.Length(); i++) {
    httpChan->SetRequestHeader(requestHeaders[i].mHeader,
                               requestHeaders[i].mValue,
                               requestHeaders[i].mMerge);
  }

  httpChan->SetNotificationCallbacks(this);

  httpChan->SetRequestMethod(nsDependentCString(requestMethod.get()));

  if (uploadStreamInfo != eUploadStream_null) {
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewPostDataStream(getter_AddRefs(stream), false, uploadStreamData, 0);
    if (!NS_SUCCEEDED(rv)) {
      return false;   // TODO: cancel request (bug 536317), return true
    }
    httpChan->InternalSetUploadStream(stream);
    // We're casting uploadStreamInfo into PRBool here on purpose because
    // we know possible values are either 0 or 1. See uploadStreamInfoType.
    httpChan->SetUploadStreamHasHeaders((PRBool) uploadStreamInfo);
  }

  if (priority != nsISupportsPriority::PRIORITY_NORMAL)
    httpChan->SetPriority(priority);
  httpChan->SetRedirectionLimit(redirectionLimit);
  httpChan->SetAllowPipelining(allowPipelining);
  httpChan->SetForceAllowThirdPartyCookie(forceAllowThirdPartyCookie);

  rv = httpChan->AsyncOpen(this, nsnull);
  if (NS_FAILED(rv))
    return false;       // TODO: cancel request (bug 536317), return true

  return true;
}

bool 
HttpChannelParent::RecvSetPriority(const PRUint16& priority)
{
  nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
  httpChan->SetPriority(priority);
  return true;
}

bool
HttpChannelParent::RecvSetCacheTokenCachedCharset(const nsCString& charset)
{
  if (mCacheDescriptor)
    mCacheDescriptor->SetMetaDataElement("charset",
                                         PromiseFlatCString(charset).get());
  return true;
}

bool
HttpChannelParent::RecvOnStopRequestCompleted()
{
  mCacheDescriptor = nsnull;
  return true;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  LOG(("HttpChannelParent::OnStartRequest [this=%x]\n", this));

  nsHttpChannel *chan = static_cast<nsHttpChannel *>(aRequest);
  nsHttpResponseHead *responseHead = chan->GetResponseHead();

  PRBool isFromCache = false;
  chan->IsFromCache(&isFromCache);
  PRUint32 expirationTime;
  chan->GetCacheTokenExpirationTime(&expirationTime);
  nsCString cachedCharset;
  chan->GetCacheTokenCachedCharset(cachedCharset);

  // Keep the cache entry for future use in RecvSetCacheTokenCachedCharset().
  // It could be already released by nsHttpChannel at that time.
  chan->GetCacheToken(getter_AddRefs(mCacheDescriptor));

  if (mIPCClosed || 
      !SendOnStartRequest(responseHead ? *responseHead : nsHttpResponseHead(), 
                          !!responseHead, isFromCache,
                          mCacheDescriptor ? PR_TRUE : PR_FALSE,
                          expirationTime, cachedCharset)) {
    return NS_ERROR_UNEXPECTED; 
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::OnStopRequest(nsIRequest *aRequest, 
                                 nsISupports *aContext, 
                                 nsresult aStatusCode)
{
  LOG(("HttpChannelParent::OnStopRequest: [this=%x status=%ul]\n", 
       this, aStatusCode));

  if (mIPCClosed || !SendOnStopRequest(aStatusCode))
    return NS_ERROR_UNEXPECTED; 
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnDataAvailable(nsIRequest *aRequest, 
                                   nsISupports *aContext, 
                                   nsIInputStream *aInputStream, 
                                   PRUint32 aOffset, 
                                   PRUint32 aCount)
{
  LOG(("HttpChannelParent::OnDataAvailable [this=%x]\n", this));
 
  nsresult rv;

  nsCString data;
  data.SetLength(aCount);
  char * p = data.BeginWriting();
  PRUint32 bytesRead;
  rv = aInputStream->Read(p, aCount, &bytesRead);
  data.EndWriting();
  if (!NS_SUCCEEDED(rv) || bytesRead != aCount) {
    return rv;              // TODO: cancel request locally (bug 536317)
  }

  if (mIPCClosed || !SendOnDataAvailable(data, aOffset, bytesRead))
    return NS_ERROR_UNEXPECTED; 
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP 
HttpChannelParent::GetInterface(const nsIID& aIID, void **result)
{
  if (aIID.Equals(NS_GET_IID(nsIAuthPromptProvider))) {
    if (!mTabParent)
      return NS_NOINTERFACE;
    return mTabParent->QueryInterface(aIID, result);
  }

  // TODO: 575494: once we're confident we're handling all needed interfaces,
  // remove all code below and simply "return QueryInterface(aIID, result)"
  if (// Known interface calls:

      // FIXME: HTTP Authorization (bug 537782):
      // nsHttpChannel first tries to get this as an nsIAuthPromptProvider; if that
      // fails, it tries as an nsIAuthPrompt2, and if that fails, an nsIAuthPrompt.
      // See nsHttpChannel::GetAuthPrompt().  So if we can return any one of these,
      // HTTP auth should be all set.  The other two if checks can be eventually
      // deleted.
      aIID.Equals(NS_GET_IID(nsIAuthPrompt2)) ||
      aIID.Equals(NS_GET_IID(nsIAuthPrompt))  ||
      // FIXME: redirects (bug 536294):
      // The likely solution here is for this class to implement nsIChannelEventSink
      // and nsIHttpEventSink (and forward calls to any real sinks in the child), in
      // which case QueryInterface() will do the work here and these if statements
      // can be eventually discarded.
      aIID.Equals(NS_GET_IID(nsIChannelEventSink)) || 
      aIID.Equals(NS_GET_IID(nsIHttpEventSink))  ||
      // FIXME: application cache (bug 536295):
      aIID.Equals(NS_GET_IID(nsIApplicationCacheContainer)) ||
      aIID.Equals(NS_GET_IID(nsIProgressEventSink)) ||
      // FIXME:  bug 561830: when fixed, we shouldn't be asked for this interface
      aIID.Equals(NS_GET_IID(nsIDocShellTreeItem)) ||
      // Let this return NS_ERROR_NO_INTERFACE: it's OK to not provide it.
      aIID.Equals(NS_GET_IID(nsIBadCertListener2))) 
  {
    return QueryInterface(aIID, result);
  } else {
    nsPrintfCString msg(2000, 
       "HttpChannelParent::GetInterface: interface UUID=%s not yet supported! "
       "Use 'grep -ri UUID <mozilla_src>' to find the name of the interface, "
       "check http://tinyurl.com/255ojvu to see if a bug has already been "
       "filed, and if not, add one and make it block bug 516730. Thanks!",
       aIID.ToString());
    NECKO_MAYBE_ABORT(msg);
    return NS_NOINTERFACE;
  }
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIProgressEventSink
//-----------------------------------------------------------------------------
 
NS_IMETHODIMP
HttpChannelParent::OnProgress(nsIRequest *aRequest, 
                              nsISupports *aContext, 
                              PRUint64 aProgress, 
                              PRUint64 aProgressMax)
{
  if (mIPCClosed || !SendOnProgress(aProgress, aProgressMax))
    return NS_ERROR_UNEXPECTED;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::OnStatus(nsIRequest *aRequest, 
                            nsISupports *aContext, 
                            nsresult aStatus, 
                            const PRUnichar *aStatusArg)
{
  if (mIPCClosed || !SendOnStatus(aStatus, nsString(aStatusArg)))
    return NS_ERROR_UNEXPECTED;
  return NS_OK;
}


}} // mozilla::net

