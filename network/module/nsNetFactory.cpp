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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsINetService.h"
#include "nsNetService.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

/* This implementation of the network service factory is presently
 * only taking advantage of the service retrieval benefit that the
 * nsINetService interface provides. The module initialization and 
 * unloading is still bound to main program execution and closing.
 * Once we break this dependence, the
 * nsNetFactory will also appropriately handle loading/initializing/
 * unloading of the library.
 */

static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

extern nsNetlibService *gNetlibService; // See nsNetService.cpp

class nsNetFactory : public nsIFactory
{   
  public:   
    // nsISupports methods   
    NS_IMETHOD QueryInterface(const nsIID &aIID,    
                              void **aResult);   
    NS_IMETHOD_(nsrefcnt) AddRef(void);   
    NS_IMETHOD_(nsrefcnt) Release(void);   

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsNetFactory(const nsCID &aClass);   

  protected:
    virtual ~nsNetFactory();   

  private:   
    nsrefcnt  mRefCnt;   
    nsCID     mClassID;
};   

nsNetFactory::nsNetFactory(const nsCID &aClass)   
{   
  mRefCnt = 0;
  mClassID = aClass;
}   

nsNetFactory::~nsNetFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsNetFactory::QueryInterface(const nsIID &aIID,   
                                      void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

nsrefcnt nsNetFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt nsNetFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

nsresult nsNetFactory::CreateInstance(nsISupports *aOuter,  
                                      const nsIID &aIID,  
                                      void **aResult)  
{  
  nsresult res;
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  if (mClassID.Equals(kNetServiceCID)) {
#ifdef XP_PC
    /* This ifdef fixes bug 2414.  Unix and mac initialize NS_InitINetService in
       NS_NewINetService so this initialization was being done twice.  Therefore
       to fix the bug we are ifdef-ing this section to be done for windows only */

    // No need create a new one if we've already got one.
    if (!gNetlibService) {
        res = NS_InitINetService();
        if (res != NS_OK)
            return NS_ERROR_FAILURE;
    }
#endif

    // Hook the caller up.
    res = NS_NewINetService((nsINetService**)aResult, aOuter);
  }
  return res;  
}  

nsresult nsNetFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" NS_NET nsresult 
NSGetFactory_NETLIB_DLL(nsISupports* serviceMgr,
                        const nsCID &aClass,
                        const char *aClassName,
                        const char *aProgID,
                        nsIFactory **aFactory)
#else
extern "C" NS_NET nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
#endif
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsNetFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
