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


#include "nsProxyEvent.h"
#include "nsProxyObjectManager.h"
#include "nsProxyEventPrivate.h"

#include "nsIProxyCreateInstance.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "plevent.h"

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);




/***************************************************************************/
/* nsProxyCreateInstance                                                   */
/* This private class will allow us to create Instances on another thread  */
/***************************************************************************/
class nsProxyCreateInstance : public nsIProxyCreateInstance
{
    NS_DECL_ISUPPORTS
    NS_IMETHOD CreateInstanceByIID(const nsIID & cid, nsISupports *aOuter, const nsIID & iid, void * *result);
    NS_IMETHOD CreateInstanceByProgID(const char *aProgID, nsISupports *aOuter, const nsIID & iid, void * *result);

    nsProxyCreateInstance();
    ~nsProxyCreateInstance();

};

nsProxyCreateInstance::nsProxyCreateInstance()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsProxyCreateInstance::~nsProxyCreateInstance()
{
}

NS_IMPL_ISUPPORTS(nsProxyCreateInstance,NS_IPROXYCREATEINSTANCE_IID)

NS_IMETHODIMP nsProxyCreateInstance::CreateInstanceByIID(const nsIID & cid, nsISupports *aOuter, const nsIID & iid, void * *result)
{
    return nsComponentManager::CreateInstance(  cid, 
                                                aOuter,
                                                iid,
                                                result);
}


NS_IMETHODIMP nsProxyCreateInstance::CreateInstanceByProgID(const char *aProgID, nsISupports *aOuter, const nsIID & iid, void * *result)
{
    return nsComponentManager::CreateInstance(  aProgID, 
                                                aOuter,
                                                iid,
                                                result);
}











/////////////////////////////////////////////////////////////////////////
// nsProxyObjectManager
/////////////////////////////////////////////////////////////////////////

nsProxyObjectManager* nsProxyObjectManager::mInstance = nsnull;


NS_IMPL_ISUPPORTS(nsProxyObjectManager, NS_IPROXYEVENT_MANAGER_IID)

nsProxyObjectManager::nsProxyObjectManager()
{
    NS_INIT_REFCNT();

    mProxyClassMap = new nsHashtable(256, PR_TRUE);
    mProxyObjectMap = new nsHashtable(256, PR_TRUE);
}

nsProxyObjectManager::~nsProxyObjectManager()
{
    delete mProxyClassMap;
    delete mProxyObjectMap;
}

nsProxyObjectManager *
nsProxyObjectManager::GetInstance()
{
    if (mInstance == NULL) 
    {
        mInstance = new nsProxyObjectManager();
    }
    return mInstance;
}



NS_IMETHODIMP 
nsProxyObjectManager::GetProxyObject(PLEventQueue *destQueue, REFNSIID aIID, nsISupports* aObj, void** aProxyObject)
{
    
    *aProxyObject = nsnull;
    
    // check to see if proxy is there or not.
    *aProxyObject = nsProxyEventObject::GetNewOrUsedProxy(destQueue, aObj, aIID);
    if (*aProxyObject != nsnull)
    {
        return NS_OK;
    }
    
    return NS_ERROR_FACTORY_NOT_REGISTERED; //fix error code?
}   


NS_IMETHODIMP 
nsProxyObjectManager::GetProxyObject(PLEventQueue *destQueue, 
                                     const nsCID &aClass, 
                                     nsISupports *aDelegate, 
                                     const nsIID &aIID, 
                                     void** aProxyObject)
{
    *aProxyObject = nsnull;
    
    nsIProxyCreateInstance* ciProxy = nsnull;
    nsProxyCreateInstance* ciObject = new nsProxyCreateInstance();
    
    if (ciObject == nsnull)
        return NS_ERROR_NULL_POINTER;

    GetProxyObject(destQueue, nsIProxyCreateInstance::GetIID(), ciObject, (void**)&ciProxy);
    
    // release ownership of ciObject so that ciProxy owns it.
    if (ciObject)
        NS_RELEASE(ciObject);

    
    if (ciProxy == nsnull)
    {
        return NS_ERROR_NULL_POINTER;
    }

    
    nsISupports* aObj;

    nsresult rv = ciProxy->CreateInstanceByIID(aClass, 
                                               aDelegate, 
                                               aIID, 
                                               (void**)&aObj);
    

    if (aObj == nsnull)
        return rv;


    GetProxyObject(destQueue, aIID, aObj, aProxyObject);

    // release ownership of aObj so that aProxyObject owns it.
    if (aObj)
        NS_RELEASE(aObj);

    if (aProxyObject != nsnull)
    {
        return NS_OK;
    }

    return NS_ERROR_FACTORY_NOT_REGISTERED; //fix error code?
}

nsHashtable* 
nsProxyObjectManager::GetRealObjectToProxyObjectMap()
{
    NS_ASSERTION(mProxyObjectMap, "no hashtable");
    return mProxyObjectMap;
}   

nsHashtable*
nsProxyObjectManager::GetIIDToProxyClassMap()
{
    NS_ASSERTION(mProxyClassMap, "no hashtable");
    return mProxyClassMap;
}   



/////////////////////////////////////////////////////////////////////////
// nsProxyEventFactory
/////////////////////////////////////////////////////////////////////////
nsProxyEventFactory::nsProxyEventFactory(void)
{
    NS_INIT_REFCNT();
}

nsProxyEventFactory::~nsProxyEventFactory(void)
{
}

NS_IMPL_ISUPPORTS(nsProxyEventFactory,kIFactoryIID)

NS_IMETHODIMP
nsProxyEventFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    nsProxyObjectManager *inst = nsProxyObjectManager::GetInstance();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(result)) 
    {
        *aResult = NULL;
    }

    NS_ADDREF(inst);  // Are we sure that we need to addref???

    return result;

}

NS_IMETHODIMP
nsProxyEventFactory::LockFactory(PRBool aLock)
{
// not implemented.
    return NS_ERROR_NOT_IMPLEMENTED;
}




////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* aServMgr)
{
    return 0;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

#ifdef NS_DEBUG
    printf("*** nsProxyObjectManager is being registered.  Hold on to your seat...\n");
#endif

    rv = compMgr->RegisterComponent(kProxyObjectManagerCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
  
  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

#ifdef NS_DEBUG
    printf("*** nsProxyObjectManager is being unregistered.  Na na na na hey hey\n");
#endif
    
    rv = compMgr->UnregisterFactory(kProxyObjectManagerCID, path);
    if (NS_FAILED(rv)) goto done;

  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}



extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (aFactory == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = NULL;
    nsISupports *inst;

    
    if (aClass.Equals(kProxyObjectManagerCID) )
        inst = new nsProxyEventFactory();
    else
        return NS_ERROR_ILLEGAL_VALUE;

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult res = inst->QueryInterface(kIFactoryIID, (void**) aFactory);

    if (NS_FAILED(res)) 
    {   
        delete inst;
    }

    return res;
}
