/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// XXX NECKO this class belongs outside necko. I've temporarily ( ;) ) added it
// to the necko util lib to satisfy the NECKO build. It needs to be factored out.

#include "nsIUnicharStreamLoader.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsNeckoUtil.h"
#include "nsIBufferInputStream.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStreamListenerIID,  NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIUnicharStreamLoaderIID,  NS_IUNICHARSTREAMLOADER_IID);

class nsUnicharStreamLoader : public nsIUnicharStreamLoader,
                              public nsIStreamListener
{
public:
  nsUnicharStreamLoader(nsIURI* aURL,
                        nsStreamCompleteFunc aFunc,
                        void* aRef);
  virtual ~nsUnicharStreamLoader();

  NS_DECL_ISUPPORTS
  
  // nsIUnicharStreamLoader methods
  NS_IMETHOD GetNumCharsRead(PRInt32* aNumBytes);

  // nsIStreamObserver methods
  NS_IMETHOD OnStartBinding(nsISupports *ctxt);
  NS_IMETHOD OnStopBinding(nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);
  NS_IMETHOD OnStartRequest(nsISupports *ctxt);
  NS_IMETHOD OnStopRequest(nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);

  // nsIStreamListener methods
  NS_IMETHOD OnDataAvailable(nsISupports *ctxt, nsIBufferInputStream *inStr, PRUint32 sourceOffset, PRUint32 count);

#if 0
  NS_IMETHOD GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo);
#endif // 0
  
protected:
  nsStreamCompleteFunc mFunc;
  void* mRef;
  nsString* mData;
};


nsUnicharStreamLoader::nsUnicharStreamLoader(nsIURI* aURL,
                                             nsStreamCompleteFunc aFunc,
                                             void* aRef)
{
  NS_INIT_REFCNT();
  mFunc = aFunc;
  mRef = aRef;
  mData = new nsString();

  // XXX This is vile vile vile!!!
  nsresult rv;
  if (aURL) {
    rv = NS_OpenURI(this, aURL);
    if ((NS_OK != rv) && (nsnull != mFunc)) {
      // Thou shalt not call out of scope whilst ones refcnt is zero
      mRefCnt = 999;
      (*mFunc)(this, *mData, mRef, rv);
      mRefCnt = 0;
    }
  }
}

nsUnicharStreamLoader::~nsUnicharStreamLoader()
{
  if (nsnull != mData) {
    delete mData;
  }
}

NS_IMPL_ADDREF(nsUnicharStreamLoader)
NS_IMPL_RELEASE(nsUnicharStreamLoader)

nsresult 
nsUnicharStreamLoader::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIStreamListenerIID)) {
    nsIStreamListener* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIUnicharStreamLoaderIID)) {
    nsIUnicharStreamLoader* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIUnicharStreamLoader* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMETHODIMP 
nsUnicharStreamLoader::GetNumCharsRead(PRInt32* aNumBytes)
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
nsUnicharStreamLoader::OnStartBinding(nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsUnicharStreamLoader::OnStopBinding(nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg)
{
  (*mFunc)(this, *mData, mRef, status);

  return NS_OK;
}

NS_IMETHODIMP
nsUnicharStreamLoader::OnStartRequest(nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP
nsUnicharStreamLoader::OnStopRequest(nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg)
{
  return NS_OK;
}

#if 0
NS_IMETHODIMP 
nsUnicharStreamLoader::GetBindInfo(nsIURI* aURL,
                                   nsStreamBindingInfo* aInfo)
{
  return NS_OK;
}
#endif // 0

#define BUF_SIZE 1024

NS_IMETHODIMP 
nsUnicharStreamLoader::OnDataAvailable(nsISupports *ctxt, 
                                       nsIBufferInputStream *inStr, 
                                       PRUint32 sourceOffset, PRUint32 count)
{
  nsresult rv = NS_OK;
  char buffer[BUF_SIZE];
  PRUint32 len, lenRead;
  
  inStr->GetLength(&len);

  while (len > 0) {
    if (len < BUF_SIZE) {
      lenRead = len;
    }
    else {
      lenRead = BUF_SIZE;
    }

    rv = inStr->Read(buffer, lenRead, &lenRead);
    if (NS_OK != rv) {
      return rv;
    }

    mData->Append(buffer, lenRead);
    len -= lenRead;
  }

  return rv;
}

extern NS_NET nsresult 
NS_NewUnicharStreamLoader(nsIUnicharStreamLoader** aInstancePtrResult,
                          nsIURI* aURL,
                          nsStreamCompleteFunc aFunc,
                          void* aRef)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsUnicharStreamLoader* it = new nsUnicharStreamLoader(aURL,
                                                        aFunc,
                                                        aRef);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIUnicharStreamLoaderIID, 
                            (void **) aInstancePtrResult);
}
