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
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult res = NS_OK;    
    if (!aClass.Equals(kCJVMManagerCID)) {
        return NS_ERROR_FACTORY_NOT_LOADED;     // XXX right error?
    }
    nsCJVMManagerFactory* factory = new nsCJVMManagerFactory();
    if (factory == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(factory);
    *aFactory = factory;
    res = serviceMgr->QueryInterface(kIServiceManagerIID, (void**)&g_pNSIServiceManager);
    if ((NS_OK == res) && (nsnull != g_pNSIServiceManager))
    {
      return NS_OK;
    }
    return res;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    return PR_FALSE;
}


////////////////////////////////////////////////////////////////////////////
// from nsISupports 

#if 1
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
#else
NS_IMPL_ISUPPORTS(nsCJVMManagerFactory, kIFactoryIID)
#endif

////////////////////////////////////////////////////////////////////////////
// from nsIFactory:

NS_METHOD
nsCJVMManagerFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
	*aResult  = NULL;

	if (aOuter && !aIID.Equals(kISupportsIID))
		return NS_NOINTERFACE;   // XXX right error?

	nsresult res = NS_ERROR_OUT_OF_MEMORY;  
	nsJVMManager *manager = new nsJVMManager(aOuter);
	if (manager != NULL) {
		res = manager->QueryInterface(aIID, (void**)aResult);
		if (res != NS_OK)
			delete manager;
	}
	return res;
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
      NS_INIT_REFCNT();

// this code is totally bogus in the new service manager world.
#if 0
      if( m_pNSIFactory != NULL)
      {
         return;
      }

      nsresult     err         = NS_OK;
      NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

      err = this->QueryInterface(kIFactoryIID, (void**)&m_pNSIFactory);
      if ( (err == NS_OK) && (m_pNSIFactory != NULL) )
      {
         NS_DEFINE_CID(kCJVMManagerCID, NS_CCAPSMANAGER_CID);
         nsRepository::RegisterFactory(kCJVMManagerCID, NULL, NULL,
                                     m_pNSIFactory, PR_FALSE);
      }
#endif
}

nsCJVMManagerFactory::~nsCJVMManagerFactory()
{
// this code is totally wrong. we're in a destructor, and whatever our reference count is our object is going
// away.
#if 0
    if(mRefCnt == 0)
    {
      NS_DEFINE_CID(kCJVMManagerCID, NS_CCAPSMANAGER_CID);
      nsRepository::UnregisterFactory(kCJVMManagerCID, (nsIFactory *)m_pNSIFactory);
      
    }
#endif
}
