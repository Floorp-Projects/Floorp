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

#include "nsStreamLoader.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsProxiedService.h"

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

NS_IMETHODIMP
nsStreamLoader::Init(nsIURI* aURL,
                     nsIStreamLoaderObserver* observer,
                     nsISupports* context,
                     nsILoadGroup* aGroup,
                     nsIInterfaceRequestor* notificationCallbacks,
                     nsLoadFlags loadAttributes)
{
  nsresult rv = NS_OK;
  mObserver = observer;
  mContext  = context;

  rv = NS_OpenURI(this, nsnull, aURL, nsnull, aGroup, notificationCallbacks,
                  loadAttributes);
  if (NS_FAILED(rv) && mObserver) {
    // don't callback synchronously as it puts the caller
    // in a recursive situation and breaks the asynchronous
    // semantics of nsIStreamLoader
    nsresult rv2 = NS_OK;
    NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, 
                    kProxyObjectManagerCID, &rv);
    if (NS_FAILED(rv2)) return rv2;

    nsCOMPtr<nsIStreamLoaderObserver> pObserver;
    rv2 = pIProxyObjectManager->GetProxyForObject(NS_CURRENT_EVENTQ, 
              NS_GET_IID(nsIStreamLoaderObserver), observer, 
              PROXY_ASYNC | PROXY_ALWAYS, getter_AddRefs(pObserver));
    if (NS_FAILED(rv2)) return rv2;

    rv = pObserver->OnStreamComplete(this, mContext, rv, 0, nsnull);
  }

  return rv;
}

NS_METHOD
nsStreamLoader::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsStreamLoader* it = new nsStreamLoader();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS3(nsStreamLoader, nsIStreamLoader,
                   nsIStreamObserver, nsIStreamListener)

NS_IMETHODIMP 
nsStreamLoader::GetNumBytesRead(PRUint32* aNumBytes)
{
  if (!mData.IsEmpty()) {
    *aNumBytes = mData.Length();
  }
  else {
    *aNumBytes = 0;
  }

  return NS_OK;
}

/* readonly attribute nsIRequest request; */
NS_IMETHODIMP 
nsStreamLoader::GetRequest(nsIRequest * *aRequest)
{
    NS_IF_ADDREF(*aRequest=mRequest);
    return NS_OK;
}


NS_IMETHODIMP 
nsStreamLoader::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsStreamLoader::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                              nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsresult rv;
  mRequest = request;
  rv = mObserver->OnStreamComplete(this, mContext, aStatus, 
                                   mData.Length(),
                                   mData.GetBuffer());
  return rv;
}

#define BUF_SIZE 1024

NS_IMETHODIMP 
nsStreamLoader::OnDataAvailable(nsIRequest* request, nsISupports *ctxt, 
                                nsIInputStream *inStr, 
                                PRUint32 sourceOffset, PRUint32 count)
{
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

    mData.Append(buffer, lenRead);
    len -= lenRead;
  }

  return rv;
}
