/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "prtypes.h"
#include "nspr.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"

#include "nsJVMManager.h"
#include "nsCJVMManagerFactory.h"
#include "nsRepository.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIServiceManagerIID,    NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kIFactoryIID,     NS_IFACTORY_IID);
static NS_DEFINE_IID(kCJVMManagerCID,  NS_JVMMANAGER_CID);

nsIFactory  *nsCJVMManagerFactory::m_pNSIFactory = NULL;
nsIServiceManager  *g_pNSIServiceManager = NULL;




/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NSGetFactory:
 * Provides entry point to liveconnect dll.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

extern "C" NS_EXPORT nsresult
NSGetFactory(const nsCID &aClass, nsISupports* servMgr, nsIFactory **aFactory)
{
    nsresult res = NS_OK;    
    if (!aClass.Equals(kCJVMManagerCID)) {
        return NS_ERROR_FACTORY_NOT_LOADED;     // XXX right error?
    }
    nsCJVMManagerFactory* pCJVMManagerFactory = new nsCJVMManagerFactory();
    if (pCJVMManagerFactory == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    pCJVMManagerFactory->AddRef();
    *aFactory = pCJVMManagerFactory;
    res = servMgr->QueryInterface(kIServiceManagerIID, (void**)&g_pNSIServiceManager);
    if ((NS_OK == res) && (nsnull != g_pNSIServiceManager))
    {
      return NS_OK;
    }
    return res;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(void)
{
    return PR_FALSE;
}





////////////////////////////////////////////////////////////////////////////
// from nsISupports 

NS_METHOD
nsCJVMManagerFactory::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    PR_ASSERT(NULL != aInstancePtr); 
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(kIFactoryIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    }
    return NS_NOINTERFACE; 
}

NS_IMPL_ADDREF(nsCJVMManagerFactory)
NS_IMPL_RELEASE(nsCJVMManagerFactory)


////////////////////////////////////////////////////////////////////////////
// from nsIFactory:

NS_METHOD
nsCJVMManagerFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
   nsJVMManager *pNSJVMManager = NULL;
   *aResult  = NULL;
   
   if (aOuter && !aIID.Equals(kISupportsIID))
       return NS_NOINTERFACE;   // XXX right error?
   pNSJVMManager = new nsJVMManager(aOuter);
   if (pNSJVMManager->QueryInterface(aIID,
                            (void**)aResult) != NS_OK) {
       // then we're trying get a interface other than nsISupports and
       // nsICapsManager
       return NS_ERROR_FAILURE;
   }
   return NS_OK;
}

NS_METHOD
nsCJVMManagerFactory::LockFactory(PRBool aLock)
{
   return NS_OK;
}



////////////////////////////////////////////////////////////////////////////
// from nsCJVMManagerFactory:

nsCJVMManagerFactory::nsCJVMManagerFactory(void)
{
      if( m_pNSIFactory != NULL)
      {
         return;
      }

      NS_INIT_REFCNT();
      nsresult     err         = NS_OK;
      NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

      err = this->QueryInterface(kIFactoryIID, (void**)&m_pNSIFactory);
      if ( (err == NS_OK) && (m_pNSIFactory != NULL) )
      {
         NS_DEFINE_CID(kCJVMManagerCID, NS_CCAPSMANAGER_CID);
         nsRepository::RegisterFactory(kCJVMManagerCID, m_pNSIFactory,
                                     PR_FALSE);
      }
}

nsCJVMManagerFactory::~nsCJVMManagerFactory()
{
    if(mRefCnt == 0)
    {
      NS_DEFINE_CID(kCJVMManagerCID, NS_CCAPSMANAGER_CID);
      nsRepository::UnregisterFactory(kCJVMManagerCID, (nsIFactory *)m_pNSIFactory);
      
    }
}


