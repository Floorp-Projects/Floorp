/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsProxyEvent.h"
#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"

#include "nsIProxyCreateInstance.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsIEventQueueService.h"
#include "nsIThread.h"


static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

/***************************************************************************/
/* nsProxyCreateInstance                                                   */
/* This private class will allow us to create Instances on another thread  */
/***************************************************************************/
class nsProxyCreateInstance : public nsIProxyCreateInstance
{
    NS_DECL_ISUPPORTS
    NS_IMETHOD CreateInstanceByIID(const nsIID & cid, nsISupports *aOuter, const nsIID & iid, void * *result);
    NS_IMETHOD CreateInstanceByContractID(const char *aContractID, nsISupports *aOuter, const nsIID & iid, void * *result);

    nsProxyCreateInstance();
    virtual ~nsProxyCreateInstance();
};

nsProxyCreateInstance::nsProxyCreateInstance()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsProxyCreateInstance::~nsProxyCreateInstance()
{
}

NS_IMPL_ISUPPORTS1(nsProxyCreateInstance, nsIProxyCreateInstance)

NS_IMETHODIMP nsProxyCreateInstance::CreateInstanceByIID(const nsIID & cid, nsISupports *aOuter, const nsIID & iid, void * *result)
{
    return nsComponentManager::CreateInstance(  cid, 
                                                aOuter,
                                                iid,
                                                result);
}


NS_IMETHODIMP nsProxyCreateInstance::CreateInstanceByContractID(const char *aContractID, nsISupports *aOuter, const nsIID & iid, void * *result)
{
    return nsComponentManager::CreateInstance(  aContractID, 
                                                aOuter,
                                                iid,
                                                result);
}

/////////////////////////////////////////////////////////////////////////
// nsProxyObjectManager
/////////////////////////////////////////////////////////////////////////

nsProxyObjectManager* nsProxyObjectManager::mInstance = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsProxyObjectManager, nsIProxyObjectManager)

nsProxyObjectManager::nsProxyObjectManager()
{
    NS_INIT_REFCNT();
    
    mProxyClassMap = new nsHashtable(256, PR_TRUE);
    mProxyObjectMap = new nsHashtable(256, PR_TRUE);
}

static PRBool PurgeProxyClasses(nsHashKey *aKey, void *aData, void* closure)
{
    nsProxyEventClass* ptr = NS_REINTERPRET_CAST(nsProxyEventClass*, aData);
    NS_RELEASE(ptr);
    return PR_TRUE;
}

nsProxyObjectManager::~nsProxyObjectManager()
{
    if (mProxyClassMap)
    {
        mProxyClassMap->Reset((nsHashtableEnumFunc)PurgeProxyClasses, nsnull);
        delete mProxyClassMap;
    }

    delete mProxyObjectMap;
    nsProxyObjectManager::mInstance = nsnull;
}

PRBool
nsProxyObjectManager::IsManagerShutdown()
{
    if (mInstance) 
        return PR_FALSE;
    return PR_TRUE;
}

nsProxyObjectManager *
nsProxyObjectManager::GetInstance()
{
    if (! mInstance) 
    {
        mInstance = new nsProxyObjectManager();
    }
    return mInstance;
}


// Helpers
NS_IMETHODIMP 
nsProxyObjectManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    nsProxyObjectManager *proxyObjectManager = GetInstance();

    if (proxyObjectManager == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    return proxyObjectManager->QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsProxyObjectManager::GetProxyForObject(nsIEventQueue *destQueue, 
                                        REFNSIID aIID, 
                                        nsISupports* aObj, 
                                        PRInt32 proxyType, 
                                        void** aProxyObject)
{
    nsresult rv;
    nsCOMPtr<nsIEventQueue> postQ;
    

    *aProxyObject = nsnull;

    //  check to see if the destination Q is a special case.
    
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) 
        return rv;
    
    rv = eventQService->ResolveEventQueue(destQueue, getter_AddRefs(postQ));
    if (NS_FAILED(rv)) 
        return rv;
    
    // check to see if the eventQ is on our thread.  If so, just return the real object.
    
    if (postQ && !(proxyType & PROXY_ASYNC) && !(proxyType & PROXY_ALWAYS))
    {
        PRBool aResult;
        postQ->IsQueueOnCurrentThread(&aResult);
     
        if (aResult)
        {
            return aObj->QueryInterface(aIID, aProxyObject);
        }
    }
    
    // check to see if proxy is there or not.
    *aProxyObject = nsProxyEventObject::GetNewOrUsedProxy(postQ, proxyType, aObj, aIID);
    
    if (*aProxyObject == nsnull)
        return NS_ERROR_NO_INTERFACE; //fix error code?
        
    return NS_OK;   
}   


NS_IMETHODIMP 
nsProxyObjectManager::GetProxy(  nsIEventQueue *destQueue, 
                                 const nsCID &aClass, 
                                 nsISupports *aDelegate, 
                                 const nsIID &aIID, 
                                 PRInt32 proxyType, 
                                 void** aProxyObject)
{

    *aProxyObject = nsnull;
    
    // 1. Create a proxy for creating an instance on another thread.

    nsIProxyCreateInstance* ciProxy = nsnull;
    nsProxyCreateInstance* ciObject = new nsProxyCreateInstance();
    
    if (ciObject == nsnull)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = GetProxyForObject(destQueue, 
                                    NS_GET_IID(nsIProxyCreateInstance), 
                                    ciObject, 
                                    PROXY_SYNC, 
                                    (void**)&ciProxy);
    
    if (NS_FAILED(rv))
    {
        delete ciObject;
        return rv;
    }
        
    // 2. now create a new instance of the request object via our proxy.

    nsISupports* aObj;

    rv = ciProxy->CreateInstanceByIID(aClass, 
                                      aDelegate, 
                                      aIID, 
                                      (void**)&aObj);

    
    // 3.  Delete the create instance proxy and its real object.
    
    NS_RELEASE(ciProxy);
    delete ciObject;
    ciObject = nsnull;


    // 4.  Check to see if creating the requested instance failed.
    if ( NS_FAILED(rv))
    {
        return rv;
    }

    // 5.  Now create a proxy object for the requested object.

    rv = GetProxyForObject(destQueue, aIID, aObj, proxyType, aProxyObject);

    
    // 6. release ownership of aObj so that aProxyObject owns it.
    
    NS_RELEASE(aObj);

    // 7. return the error returned from GetProxyForObject.  Either way, we our out of here.

    return rv;   
}



