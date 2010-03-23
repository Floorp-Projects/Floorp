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

#include "nsHttp.h"
#include "mozilla/net/HttpChannelParent.h"
#include "nsHttpChannel.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

// C++ file contents
HttpChannelParent::HttpChannelParent()
{
}

HttpChannelParent::~HttpChannelParent()
{
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
HttpChannelParent::RecvAsyncOpen(const nsCString&           uriSpec, 
                                 const nsCString&           charset,
                                 const nsCString&           originalUriSpec, 
                                 const nsCString&           originalCharset,
                                 const nsCString&           docUriSpec, 
                                 const nsCString&           docCharset,
                                 const PRUint32&            loadFlags,
                                 const RequestHeaderTuples& requestHeaders)
{
  nsresult rv;

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return false;       // TODO: send fail msg to child, return true

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), uriSpec, charset.get(), nsnull, ios);
  if (NS_FAILED(rv))
    return false;       // TODO: send fail msg to child, return true

  // Delay log to here, as gHttpLog may not exist in parent until we init
  // gHttpHandler via above call to NS_NewURI.  Avoids segfault :)
  LOG(("HttpChannelParent RecvAsyncOpen [this=%x uri=%s (%s)]\n", 
       this, uriSpec.get(), charset.get()));

  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannel(getter_AddRefs(chan), uri, ios, nsnull, nsnull, loadFlags);
  if (NS_FAILED(rv))
    return false;       // TODO: send fail msg to child, return true

  if (!originalUriSpec.IsEmpty()) {
    nsCOMPtr<nsIURI> originalUri;
    rv = NS_NewURI(getter_AddRefs(originalUri), originalUriSpec, 
                   originalCharset.get(), nsnull, ios);
    if (!NS_FAILED(rv))
      chan->SetOriginalURI(originalUri);
  }
  if (!docUriSpec.IsEmpty()) {
    nsCOMPtr<nsIURI> docUri;
    rv = NS_NewURI(getter_AddRefs(docUri), docUriSpec, 
                   docCharset.get(), nsnull, ios);
    if (!NS_FAILED(rv)) {
      nsCOMPtr<nsIHttpChannelInternal> iChan(do_QueryInterface(chan));
      if (iChan) 
        iChan->SetDocumentURI(docUri);
    }
  }
  if (loadFlags != nsIRequest::LOAD_NORMAL)
    chan->SetLoadFlags(loadFlags);

  nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(chan));
  for (PRUint32 i = 0; i < requestHeaders.Length(); i++)
    httpChan->SetRequestHeader(requestHeaders[i].mHeader,
                               requestHeaders[i].mValue,
                               requestHeaders[i].mMerge);

  // TODO: implement needed interfaces, and either proxy calls back to child
  // process, or rig up appropriate hacks.
//  chan->SetNotificationCallbacks(this);

  rv = chan->AsyncOpen(this, nsnull);
  if (NS_FAILED(rv))
    return false;       // TODO: send fail msg to child, return true

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

  if (!SendOnStartRequest(*responseHead)) {
    // IPDL error--child dead/dying & our own destructor will be called
    // automatically
    // -- TODO: verify that that's the case :)
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

  if (!SendOnStopRequest(aStatusCode)) {
    // IPDL error--child dead/dying & our own destructor will be called
    // automatically
    return NS_ERROR_UNEXPECTED; 
  }
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

  if (!SendOnDataAvailable(data, aOffset, bytesRead)) {
    // IPDL error--child dead/dying & our own destructor will be called
    // automatically
    return NS_ERROR_UNEXPECTED; 
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP 
HttpChannelParent::GetInterface(const nsIID& uuid, void **result)
{
  DROP_DEAD();
}


}} // mozilla::net

