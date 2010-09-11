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
 *   Honza Bambas <honzab@firemni.cz> 
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

#include "HttpChannelParentListener.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/net/NeckoParent.h"
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

namespace mozilla {
namespace net {

HttpChannelParentListener::HttpChannelParentListener(HttpChannelParent* aInitialChannel)
  : mActiveChannel(aInitialChannel)
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

  mActiveChannel = nsnull;
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
  if (aIID.Equals(NS_GET_IID(nsIAuthPromptProvider))) {
    if (!mActiveChannel || !mActiveChannel->mTabParent)
      return NS_NOINTERFACE;
    return mActiveChannel->mTabParent->QueryInterface(aIID, result);
  }

  if (aIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
    if (!mActiveChannel)
      return NS_NOINTERFACE;
    return mActiveChannel->QueryInterface(aIID, result);
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
      aIID.Equals(NS_GET_IID(nsIRedirectResultListener))  ||
      // FIXME: application cache (bug 536295):
      aIID.Equals(NS_GET_IID(nsIApplicationCacheContainer)) ||
      // FIXME:  bug 561830: when fixed, we shouldn't be asked for this interface
      aIID.Equals(NS_GET_IID(nsIDocShellTreeItem)) ||
      // Let this return NS_ERROR_NO_INTERFACE: it's OK to not provide it.
      aIID.Equals(NS_GET_IID(nsIBadCertListener2))) 
  {
    return QueryInterface(aIID, result);
  } else {
    nsPrintfCString msg(2000, 
       "HttpChannelParentListener::GetInterface: interface UUID=%s not yet supported! "
       "Use 'grep -ri UUID <mozilla_src>' to find the name of the interface, "
       "check http://tinyurl.com/255ojvu to see if a bug has already been "
       "filed, and if not, add one and make it block bug 516730. Thanks!",
       aIID.ToString());
    NECKO_MAYBE_ABORT(msg);
    return NS_NOINTERFACE;
  }
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
  if (mActiveChannel->mIPCClosed)
    return NS_BINDING_ABORTED;

  // Create new PHttpChannel
  PBrowserParent* browser = mActiveChannel->mTabParent ?
      static_cast<TabParent*>(mActiveChannel->mTabParent.get()) : nsnull;
  mRedirectChannel = static_cast<HttpChannelParent *>
      (mActiveChannel->Manager()->SendPHttpChannelConstructor(browser));

  // Join it with the correct channel
  mRedirectChannel->mChannel = newChannel;

  // Let the new channel also keep the wrapper in case we get another redirect
  // response, it wouldn't be able to send back the redirect result.
  mRedirectChannel->mChannelListener = this;

  // And finally, let the content process decide to redirect or not.
  mRedirectCallback = callback;

  nsCOMPtr<nsIURI> newURI;
  newChannel->GetURI(getter_AddRefs(newURI));

  nsHttpChannel *oldHttpChannel = static_cast<nsHttpChannel *>(oldChannel);
  nsHttpResponseHead *responseHead = oldHttpChannel->GetResponseHead();

  // TODO: check mActiveChannel->mIPCClosed and return val from Send function
  mActiveChannel->SendRedirect1Begin(mRedirectChannel,
                                     IPC::URI(newURI),
                                     redirectFlags,
                                     responseHead ? *responseHead 
                                                  : nsHttpResponseHead());

  // mActiveChannel gets the response in RecvRedirect2Result and forwards it
  // to this wrapper through OnContentRedirectResultReceived

  return NS_OK;
}

void
HttpChannelParentListener::OnContentRedirectResultReceived(
                                const nsresult result,
                                const RequestHeaderTuples& changedHeaders)
{
  nsHttpChannel* newHttpChannel = 
      static_cast<nsHttpChannel*>(mRedirectChannel->mChannel.get());

  if (NS_SUCCEEDED(result)) {
    for (PRUint32 i = 0; i < changedHeaders.Length(); i++) {
      newHttpChannel->SetRequestHeader(changedHeaders[i].mHeader,
                                       changedHeaders[i].mValue,
                                       changedHeaders[i].mMerge);
    }
  }

  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback = nsnull;
}

//-----------------------------------------------------------------------------
// HttpChannelParentListener::nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParentListener::OnRedirectResult(PRBool succeeded)
{
  if (!mRedirectChannel) {
    // Redirect might get canceled before we got AsyncOnChannelRedirect
    return NS_OK;
  }

  if (succeeded && !mActiveChannel->mIPCClosed) {
    // TODO: check return value: assume child dead if failed
    mActiveChannel->SendRedirect3Complete();
  }

  HttpChannelParent* channelToDelete;
  if (succeeded) {
    // Switch to redirect channel and delete the old one.
    channelToDelete = mActiveChannel;
    mActiveChannel = mRedirectChannel;
  } else {
    // Delete the redirect target channel: continue using old channel
    channelToDelete = mRedirectChannel;
  }

  if (!channelToDelete->mIPCClosed)
    HttpChannelParent::Send__delete__(channelToDelete);
  mRedirectChannel = nsnull;

  return NS_OK;
}

}} // mozilla::net
