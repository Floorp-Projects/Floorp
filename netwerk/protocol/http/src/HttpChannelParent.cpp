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
#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsIAuthPromptProvider.h"
#include "nsIDocShellTreeItem.h"

namespace mozilla {
namespace net {

// C++ file contents
HttpChannelParent::HttpChannelParent()
: mIPCClosed(false)
{
  // Ensure gHttpHandler is initialized: we need the atom table up and running.
  nsIHttpProtocolHandler* handler;
  CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &handler);
  NS_ASSERTION(handler, "no http handler");
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

NS_IMPL_ISUPPORTS3(HttpChannelParent, 
                   nsIRequestObserver, 
                   nsIStreamListener,
                   nsIInterfaceRequestor);

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
    return false;       // TODO: send fail msg to child, return true

  rv = NS_NewChannel(getter_AddRefs(mChannel), uri, ios, nsnull, nsnull, loadFlags);
  if (NS_FAILED(rv))
    return false;       // TODO: send fail msg to child, return true

  nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
  httpChan->SetRemoteChannel();

  if (originalUri)
    httpChan->SetOriginalURI(originalUri);
  if (docUri)
    httpChan->SetDocumentURI(docUri);
  if (referrerUri)
    httpChan->SetReferrerInternal(referrerUri);
  if (loadFlags != nsIRequest::LOAD_NORMAL)
    httpChan->SetLoadFlags(loadFlags);

  for (PRUint32 i = 0; i < requestHeaders.Length(); i++)
    httpChan->SetRequestHeader(requestHeaders[i].mHeader,
                               requestHeaders[i].mValue,
                               requestHeaders[i].mMerge);

  httpChan->SetNotificationCallbacks(this);

  httpChan->SetRequestMethod(nsDependentCString(requestMethod.get()));
  if (priority != nsISupportsPriority::PRIORITY_NORMAL)
    httpChan->SetPriority(priority);
  httpChan->SetRedirectionLimit(redirectionLimit);
  httpChan->SetAllowPipelining(allowPipelining);
  httpChan->SetForceAllowThirdPartyCookie(forceAllowThirdPartyCookie);

  rv = httpChan->AsyncOpen(this, nsnull);
  if (NS_FAILED(rv))
    return false;       // TODO: send fail msg to child, return true

  return true;
}

bool 
HttpChannelParent::RecvSetPriority(const PRUint16& priority)
{
  nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
  httpChan->SetPriority(priority);
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
  NS_ABORT_IF_FALSE(responseHead, "Missing HTTP responseHead!");

  if (mIPCClosed || !SendOnStartRequest(*responseHead))
    return NS_ERROR_UNEXPECTED; 
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
    return rv;              // TODO: figure out error handling
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
  if (// Known interface calls:

      // FIXME: HTTP Authorization (bug 537782):
      // nsHttpChannel first tries to get this as an nsIAuthPromptProvider; if that
      // fails, it tries as an nsIAuthPrompt2, and if that fails, an nsIAuthPrompt.
      // See nsHttpChannel::GetAuthPrompt().  So if we can return any one of these,
      // HTTP auth should be all set.  The other two if checks can be eventually
      // deleted.
      aIID.Equals(NS_GET_IID(nsIAuthPromptProvider)) || 
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
      // FIXME:  progress notifications
      aIID.Equals(NS_GET_IID(nsIProgressEventSink)) ||
      // FIXME:  bug 561830: when fixed, we shouldn't be asked for this interface
      aIID.Equals(NS_GET_IID(nsIDocShellTreeItem))) 
  {
    return QueryInterface(aIID, result);
  } 

  // Interface we haven't dealt with yet. Make sure we know by dying.
  // - use "grep -ri [uuid] ROOT_SRC_DIR" with the uuid from the printf to
  //   find the offending interface.
  // - FIXME: make non-fatal before we ship
  printf("*&*&*& HttpChannelParent::GetInterface: uuid=%s not impl'd yet! "
         "File a bug!\n", 
         aIID.ToString());
  DROP_DEAD();
}


}} // mozilla::net

