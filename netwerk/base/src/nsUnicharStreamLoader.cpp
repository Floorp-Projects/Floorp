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

#include "nsUnicharStreamLoader.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIBufferInputStream.h"
#include "nsCOMPtr.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"

static NS_DEFINE_CID(kIOServiceCID,  NS_IOSERVICE_CID);

nsUnicharStreamLoader::nsUnicharStreamLoader()
  : mData(nsnull)
{
  NS_INIT_REFCNT();
}

NS_IMETHODIMP
nsUnicharStreamLoader::Init(nsIURI* aURL,
                            nsIUnicharStreamLoaderObserver* observer, 
                            nsILoadGroup* aGroup,
                            nsIInterfaceRequestor* notificationCallbacks,
                            nsLoadFlags loadAttributes)
{
  nsresult rv = NS_OK;
  mObserver = observer;
  mData = new nsString();
///  mLoadGroup = aLoadGroup;

///  rv = mLoadGroup->AddChannel(channel, nsnull);
///  if (NS_FAILED(rv)) return;

  NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsIChannel* channel;
  rv = serv->NewChannelFromURI("load", aURL, aGroup, notificationCallbacks,
                               loadAttributes, nsnull, &channel);
  if (NS_FAILED(rv)) return rv;

  rv = channel->AsyncRead(0, -1, nsnull, this);
  NS_RELEASE(channel);

  if (NS_FAILED(rv) && mObserver) {
    nsresult rv2 = mObserver->OnUnicharStreamComplete(this, rv, mData->Length(), 
                                                      mData->GetUnicode());
    if (NS_FAILED(rv2))
      rv = rv2; // take the user's error instead
  }

  return rv;
}

nsUnicharStreamLoader::~nsUnicharStreamLoader()
{
  if (nsnull != mData) {
    delete mData;
  }
}

NS_METHOD
nsUnicharStreamLoader::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsUnicharStreamLoader* it = new nsUnicharStreamLoader();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS3(nsUnicharStreamLoader, nsIUnicharStreamLoader,
                   nsIStreamObserver, nsIStreamListener)

NS_IMETHODIMP 
nsUnicharStreamLoader::GetNumCharsRead(PRUint32* aNumBytes)
{
  if (nsnull != mData) {
    *aNumBytes = mData->Length();
  }
  else {
    *aNumBytes = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsUnicharStreamLoader::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsUnicharStreamLoader::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
                                     nsresult status, const PRUnichar *errorMsg)
{
  nsresult rv = mObserver->OnUnicharStreamComplete(this, status, mData->Length(), 
                                                   mData->GetUnicode());
///  return mLoadGroup->RemoveChannel(channel, ctxt, status, errorMsg);
  return rv;
}

#define BUF_SIZE 1024

NS_IMETHODIMP 
nsUnicharStreamLoader::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, 
                                       nsIInputStream *inStr, 
                                       PRUint32 sourceOffset, PRUint32 count)
{
  nsresult rv = NS_OK;
  char buffer[BUF_SIZE];
  PRUint32 len, lenRead;
  
  inStr->Available(&len);

  while (len > 0) {
    if (len < BUF_SIZE) {
      lenRead = len;
    }
    else {
      lenRead = BUF_SIZE;
    }

    rv = inStr->Read(buffer, lenRead, &lenRead);
    if (NS_FAILED(rv) || lenRead == 0) {
      return rv;
    }

    mData->Append(buffer, lenRead);
    len -= lenRead;
  }

  return rv;
}
