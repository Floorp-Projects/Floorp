/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDownloader.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsProxiedService.h"
#include "nsIFile.h"
#include "nsXPIDLString.h"

#include "nsCacheManager.h"
#include "nsICachedNetData.h"
#include "nsIStreamAsFile.h"

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kNetworkCacheManagerCID, NS_CACHE_MANAGER_CID);

NS_IMETHODIMP
nsDownloader::Init(nsIURI* aURL,
                   nsIDownloadObserver* aObserver,
                   nsISupports* aContext,
                   PRBool aIsSynchronous,
                   nsILoadGroup* aGroup,
                   nsIInterfaceRequestor* aNotificationCallbacks,
                   nsLoadFlags aLoadAttributes,
                   PRUint32 aBufferSegmentSize,
                   PRUint32 aBufferMaxSize)
{
  nsresult rv;
  mObserver = aObserver;
  mContext  = aContext;
  nsCOMPtr<nsIFile> localFile;
  nsCOMPtr<nsIChannel> channel;

  aLoadAttributes |= nsIChannel::CACHE_AS_FILE;
  rv = NS_OpenURI(getter_AddRefs(channel), aURL, nsnull, aGroup, aNotificationCallbacks,
                  aLoadAttributes, aBufferSegmentSize, aBufferMaxSize);
  if (NS_SUCCEEDED(rv) && channel)
    rv = channel->GetLocalFile(getter_AddRefs(localFile));

  if (mObserver && (NS_FAILED(rv) || localFile)) 
  {
     if (aIsSynchronous)
       return mObserver->OnDownloadComplete(this, mContext, rv, localFile);
     else
     {
       // If the open failed or the file is local, call the observer.
       // don't callback synchronously as it puts the caller
       // in a recursive situation and breaks the asynchronous
       // semantics of nsIDownloader
       nsresult rv2 = NS_OK;
       NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, 
                       kProxyObjectManagerCID, &rv);
       if (NS_FAILED(rv2)) return rv2;
           nsCOMPtr<nsIDownloadObserver> pObserver;
           rv2 = pIProxyObjectManager->GetProxyForObject(NS_CURRENT_EVENTQ, 
                     NS_GET_IID(nsIDownloadObserver), mObserver, 
                     PROXY_ASYNC | PROXY_ALWAYS, getter_AddRefs(pObserver));
           if (NS_FAILED(rv2)) return rv2;
           return pObserver->OnDownloadComplete(this, mContext, rv, localFile);
     }
  }

  return channel->AsyncRead(this, aContext);
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
                   nsIStreamObserver, nsIStreamListener)

NS_IMETHODIMP 
nsDownloader::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsDownloader::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
                              nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsCOMPtr<nsIFile> file;
  if (NS_SUCCEEDED(aStatus))
  {
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString spec;
    rv = uri->GetSpec(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;
  
    NS_WITH_SERVICE(nsINetDataCacheManager, cacheMgr, kNetworkCacheManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsICachedNetData> cachedData;
    rv = cacheMgr->GetCachedNetData(spec, nsnull, 0, nsINetDataCacheManager::CACHE_AS_FILE,
                                      getter_AddRefs(cachedData));
    

    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIStreamAsFile> streamAsFile;
      streamAsFile = do_QueryInterface(cachedData, &rv);
      if (NS_FAILED(rv)) return rv;

      rv = streamAsFile->GetFile(getter_AddRefs(file));
      if (NS_FAILED(rv)) return rv;
    }
  }

  return mObserver->OnDownloadComplete(this, mContext, aStatus, file);
}

#define BUF_SIZE 1024

NS_IMETHODIMP 
nsDownloader::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, 
                                nsIInputStream *inStr, 
                                PRUint32 sourceOffset, PRUint32 count)
{
  // This function simply disposes of the data as it's read in. 
  // We assume it's already been cached and that's what we're interested in.
  nsresult rv = NS_OK;
  char buffer[BUF_SIZE];
  PRUint32 len, lenRead;
  
  rv = inStr->Available(&len);
  if (NS_FAILED(rv)) return rv;

  while (len > 0) {
    lenRead = PR_MIN(len, BUF_SIZE);
    rv = inStr->Read(buffer, lenRead, &lenRead);
    if (NS_FAILED(rv) || lenRead == 0) {
      return rv;
    }
    len -= lenRead;
  }

  return rv;
}
