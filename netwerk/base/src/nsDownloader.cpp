/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDownloader.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIFileChannel.h"
#include "nsICachingChannel.h"
#include "nsProxiedService.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

// XXX this API seems all wrong for "sync" downloading

NS_IMETHODIMP
nsDownloader::Init(nsIURI* aURL,
                   nsIDownloadObserver* aObserver,
                   nsISupports* aContext,
                   PRBool aIsSynchronous,
                   nsILoadGroup* aGroup,
                   nsIInterfaceRequestor* aCallbacks,
                   nsLoadFlags aLoadFlags)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  nsresult rv;

  mObserver = aObserver;
  mContext  = aContext;

  nsCOMPtr<nsIFile> file;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURL);
  if (fileURL)
    fileURL->GetFile(getter_AddRefs(file));

  if (file) {
    if (aIsSynchronous)
      return mObserver->OnDownloadComplete(this, mContext, rv, file);

    // If the open failed or the file is local, call the observer.
    // don't callback synchronously as it puts the caller
    // in a recursive situation and breaks the asynchronous
    // semantics of nsIDownloader
    nsCOMPtr<nsIProxyObjectManager> pIProxyObjectManager = 
            do_GetService(kProxyObjectManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDownloadObserver> pObserver;
    rv = pIProxyObjectManager->GetProxyForObject(NS_CURRENT_EVENTQ, 
            NS_GET_IID(nsIDownloadObserver), mObserver, 
            PROXY_ASYNC | PROXY_ALWAYS, getter_AddRefs(pObserver));
    if (NS_FAILED(rv)) return rv;

    return pObserver->OnDownloadComplete(this, mContext, NS_OK, file);
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), aURL, nsnull,
                     aGroup, aCallbacks, aLoadFlags);
  if (NS_FAILED(rv)) return rv;

  return channel->AsyncOpen(this, aContext);
}

NS_METHOD
nsDownloader::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsDownloader* it = new nsDownloader();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS3(nsDownloader, nsIDownloader,
                   nsIRequestObserver, nsIStreamListener)

NS_IMETHODIMP 
nsDownloader::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  nsresult rv;
  nsCOMPtr<nsICachingChannel> caching = do_QueryInterface(request, &rv);
  if (caching)
    rv = caching->SetCacheAsFile(PR_TRUE);
  // Returning failure from here will cancel the load
  return rv;
}

NS_IMETHODIMP 
nsDownloader::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                              nsresult aStatus)
{
  nsCOMPtr<nsIFile> file;
  if (NS_SUCCEEDED(aStatus))
  {
    nsCOMPtr<nsICachingChannel> caching = do_QueryInterface(request, &aStatus);
    if (caching)
      aStatus = caching->GetCacheFile(getter_AddRefs(file));
  }

  return mObserver->OnDownloadComplete(this, mContext, aStatus, file);
}

NS_METHOD
nsDownloader::ConsumeData(nsIInputStream* in,
                          void* closure,
                          const char* fromRawSegment,
                          PRUint32 toOffset,
                          PRUint32 count,
                          PRUint32 *writeCount)
{
  *writeCount = count;
  return NS_OK;
}

NS_IMETHODIMP 
nsDownloader::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, 
                                nsIInputStream *inStr, 
                                PRUint32 sourceOffset, PRUint32 count)
{
  // This function simply disposes of the data as it's read in. 
  // We assume it's already been cached and that's what we're interested in.
  PRUint32 lenRead;  
  return inStr->ReadSegments(nsDownloader::ConsumeData, nsnull, count, &lenRead);
}
