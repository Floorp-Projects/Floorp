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
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsINetService.h"
#include "nsIURLGroup.h"
#include "nsIHttpUrl.h"     /* NS_NewHttpUrl(...) */
#include "nsINetlibURL.h"
#include "nsIPostToServer.h"
#include "nsString.h"
#include <stdlib.h>

#include <stdio.h>/* XXX */
#include "plstr.h"
#include "prprf.h"  /* PR_snprintf(...) */
#include "prmem.h"  /* PR_Malloc(...) / PR_Free(...) */
#if defined(XP_MAC)
#include "xp_mcom.h"	/* XP_STRDUP() */
#endif  /* XP_MAC */

#if defined(XP_PC)
#include <windows.h>  // Needed for Interlocked APIs defined in nsISupports.h
#endif /* XP_PC */


class URLImpl : public nsIURL {
public:
   
  // from nsIURL:
  
  NS_IMETHOD Open(nsIInputStream* *result);
  NS_IMETHOD Open(nsIStreamListener* listener);
  NS_IMETHOD_(PRBool) Equals(const nsIURL *aURL);
  NS_IMETHOD GetSpec(const char* *result);
  NS_IMETHOD SetSpec(const char* spec);
  NS_IMETHOD GetProtocol(const char* *result);
  NS_IMETHOD SetProtocol(const char* protocol);
  NS_IMETHOD GetHost(const char* *result);
  NS_IMETHOD SetHost(const char* host);
  NS_IMETHOD GetHostPort(PRUint32 *result);
  NS_IMETHOD SetHostPort(PRUint32 port);
  NS_IMETHOD GetFile(const char* *result);
  NS_IMETHOD SetFile(const char* file);
  NS_IMETHOD GetRef(const char* *result);
  NS_IMETHOD SetRef(const char* ref);
  NS_IMETHOD GetSearch(const char* *result);
  NS_IMETHOD SetSearch(const char* search);
  NS_IMETHOD GetContainer(nsISupports* *result);
  NS_IMETHOD SetContainer(nsISupports* container);
  NS_IMETHOD GetLoadAttribs(nsILoadAttribs* *result);
  NS_IMETHOD SetLoadAttribs(nsILoadAttribs* loadAttribs);
  NS_IMETHOD GetGroup(nsIURLGroup* *result);
  NS_IMETHOD SetGroup(nsIURLGroup* group);
  NS_IMETHOD SetPostHeader(const char* name, const char* value);
  NS_IMETHOD SetPostData(nsIInputStream* input);
  NS_IMETHOD ToString(PRUnichar* *aString);
  
  // from nsINetlibURL:
  
  NS_IMETHOD InitializeURLInfo(URL_Struct_ *URL_s);
  
  // URLImpl:
  
  URLImpl(const nsIURL* aURL, 
          const nsString& aSpec, 
          nsISupports* container = nsnull, 
          nsIURLGroup* aGroup = nsnull);
  virtual ~URLImpl();

  NS_DECL_ISUPPORTS

protected:
  nsINetlibURL* mProtocolUrl;
  
  nsINetlibURL* GetProtocolURL(const nsString& aSpec, const nsIURL* aURL);
};

URLImpl::URLImpl(const nsIURL* aURL, const nsString& aSpec, 
                 nsISupports* aContainer, nsIURLGroup* aGroup)
{
  NS_INIT_REFCNT();
  mProtocolUrl = GetProtocolURL(aSpec, aURL);
}

NS_IMPL_THREADSAFE_ADDREF(URLImpl)
NS_IMPL_THREADSAFE_RELEASE(URLImpl)

NS_DEFINE_IID(kURLIID, NS_IURL_IID);

nsresult URLImpl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
      return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kURLIID)) {
      *aInstancePtr = (void*) ((nsIURL*)this);
      NS_ADDREF_THIS();
      return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
      *aInstancePtr = (void*) ((nsIURL *)this);
      NS_ADDREF_THIS();
      return NS_OK;
  }
  /*
   * Check for aggregated interfaces...
   */
  NS_LOCK_INSTANCE();
  if (nsnull != mProtocolUrl) {
      rv = mProtocolUrl->QueryInterface(aIID, aInstancePtr);
  }
  NS_UNLOCK_INSTANCE();

#if defined(NS_DEBUG) 
  /*
   * Check for the debug-only interface indicating thread-safety
   */
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
  if (aIID.Equals(kIsThreadsafeIID)) {
    return NS_OK;
  }
#endif /* NS_DEBUG */

  return rv;
}


URLImpl::~URLImpl()
{
  NS_IF_RELEASE(mProtocolUrl);
}

nsINetlibURL* URLImpl::GetProtocolURL(const nsString& aSpec, const nsIURL* aURL)
{
  // XXX this is cheating -- there really should be a table-driven thing here
  // based on the protocol part of aSpec
 
  nsresult err;
  nsINetlibURL* result;
  // XXX parse aSpec, aURL
  err = NS_NewHttpURL((nsISupports**)&result, (nsIURL*)this);    // XXX cast
  return result;
}
  
////////////////////////////////////////////////////////////////////////////////
  
PRBool URLImpl::Equals(const nsIURL* aURL)
{
  if (mProtocolUrl)
    return mProtocolUrl->Equals(aURL);
  else
    return PR_FALSE;
}
  
nsresult URLImpl::GetProtocol(const char* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetProtocol(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetProtocol(const char *aNewProtocol)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetProtocol(aNewProtocol);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetHost(const char* *result) 
{
  if (mProtocolUrl)
    return mProtocolUrl->GetHost(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetHost(const char *aNewHost)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetHost(aNewHost);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetFile(const char* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetFile(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetFile(const char *aNewFile)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetFile(aNewFile);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetSpec(const char* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetSpec(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetSpec(const char *aNewSpec)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetSpec(aNewSpec);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetRef(const char* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetRef(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetRef(const char* ref)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetRef(ref);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetHostPort(PRUint32 *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetHostPort(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetHostPort(PRUint32 port)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetHostPort(port);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetSearch(const char* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetSearch(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetSearch(const char* search)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetSearch(search);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetContainer(nsISupports* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetContainer(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetContainer(nsISupports* container)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetContainer(container);
  else
    return NS_ERROR_FAILURE;
}

nsresult URLImpl::GetLoadAttribs(nsILoadAttribs* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetLoadAttribs(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetLoadAttribs(nsILoadAttribs* loadAttribs)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetLoadAttribs(loadAttribs);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::GetGroup(nsIURLGroup* *result)
{
  if (mProtocolUrl)
    return mProtocolUrl->GetGroup(result);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetGroup(nsIURLGroup* group)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetGroup(group);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetPostHeader(const char* name, const char* value)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetPostHeader(name, value);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::SetPostData(nsIInputStream* input)
{
  if (mProtocolUrl)
    return mProtocolUrl->SetPostData(input);
  else
    return NS_ERROR_FAILURE;
}
  
nsresult URLImpl::ToString(PRUnichar* *aString)
{
  if (mProtocolUrl)
    return mProtocolUrl->ToString(aString);
  else
    return NS_ERROR_FAILURE;
}

nsresult URLImpl::Open(nsIInputStream* *aStream)
{
  if (mProtocolUrl)
    return mProtocolUrl->Open(aStream);
  else
    return NS_ERROR_FAILURE;
}

nsresult URLImpl::Open(nsIStreamListener *aListener)
{
  if (mProtocolUrl)
    return mProtocolUrl->Open(aListener);
  else
    return NS_ERROR_FAILURE;
}

nsresult URLImpl::InitializeURLInfo(URL_Struct_ *URL_s)
{
  if (mProtocolUrl)
    return mProtocolUrl->InitializeURLInfo(URL_s);
  else
    return NS_ERROR_FAILURE;
}
  
////////////////////////////////////////////////////////////////////////////////

NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                          const nsString& aSpec)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  URLImpl* it = new URLImpl(nsnull, aSpec);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kURLIID, (void **) aInstancePtrResult);
}

NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                          const nsIURL* aURL,
                          const nsString& aSpec,
                          nsISupports* aContainer,
                          nsIURLGroup* aGroup)
{
  URLImpl* it = new URLImpl(aURL, aSpec, aContainer, aGroup);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kURLIID, (void **) aInstancePtrResult);
}

NS_NET nsresult NS_MakeAbsoluteURL(nsIURL* aURL,
                                   const nsString& aBaseURL,
                                   const nsString& aSpec,
                                   nsString& aResult)
{
  if (0 < aBaseURL.Length()) {
    URLImpl base(nsnull, aBaseURL);
    URLImpl url(&base, aSpec);
    PRUnichar* str;
    url.ToString(&str);
    aResult = *new nsString(str);
    delete str;
  } else {
    URLImpl url(aURL, aSpec);
    PRUnichar* str;
    url.ToString(&str);
    aResult = *new nsString(str);
    delete str;
  }
  return NS_OK;
}
