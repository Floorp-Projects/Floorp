/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com> (Original Author)
 *   Judson Valeski <valeski@netscape.com>
 *   Dan Matejka <danm@netscape.com>
 *   Scott Collins <scc@netscape.com>
 *   Heikki Toivonen <heiki@citec.fi>
 *   Patrick Beard <beard@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Warren Harris <warren@netscape.com>
 *   Chris Seawood <cls@seawood.org>
 *   Chris Waterson <waterson@netscape.com>
 *   Dan Mosedale <dmose@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsProxyEvent.h"
#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"

#include "nsIProxyCreateInstance.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerObsolete.h"
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

    nsProxyCreateInstance()
    {
        NS_GetComponentManager(getter_AddRefs(mCompMgr));
        NS_ASSERTION(mCompMgr, "no component manager");
    }

private:

    nsCOMPtr<nsIComponentManager> mCompMgr;
};

NS_IMPL_ISUPPORTS1(nsProxyCreateInstance, nsIProxyCreateInstance)

NS_IMETHODIMP nsProxyCreateInstance::CreateInstanceByIID(const nsIID & cid, nsISupports *aOuter, const nsIID & iid, void * *result)
{
    return mCompMgr->CreateInstance(cid, 
                                    aOuter,
                                    iid,
                                    result);
}


NS_IMETHODIMP nsProxyCreateInstance::CreateInstanceByContractID(const char *aContractID, nsISupports *aOuter, const nsIID & iid, void * *result)
{
    return mCompMgr->CreateInstanceByContractID(aContractID, 
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
    : mProxyObjectMap(256, PR_TRUE),
      mProxyClassMap(256, PR_TRUE)
{
    mProxyCreationMonitor = PR_NewMonitor();
}

static PRBool PurgeProxyClasses(nsHashKey *aKey, void *aData, void* closure)
{
    nsProxyEventClass* ptr = NS_REINTERPRET_CAST(nsProxyEventClass*, aData);
    NS_RELEASE(ptr);
    return PR_TRUE;
}

nsProxyObjectManager::~nsProxyObjectManager()
{
    mProxyClassMap.Reset((nsHashtableEnumFunc)PurgeProxyClasses, nsnull);

    if (mProxyCreationMonitor) {
        PR_DestroyMonitor(mProxyCreationMonitor);
    }

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


void
nsProxyObjectManager::Shutdown()
{
    mInstance = nsnull;
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
    if (!aObj) return NS_ERROR_NULL_POINTER;
    if (!aProxyObject) return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsCOMPtr<nsIEventQueue> postQ;
    

    *aProxyObject = nsnull;

    //  check to see if the destination Q is a special case.
    
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) 
        return rv;
    
    rv = eventQService->ResolveEventQueue(destQueue, getter_AddRefs(postQ));
    if (NS_FAILED(rv)) 
        return rv;
    
    // check to see if the eventQ is on our thread.  If so, just return the real object.
    
    if (postQ && !(proxyType & PROXY_ASYNC) && !(proxyType & PROXY_ALWAYS))
    {
        PRBool aResult;
        postQ->IsOnCurrentThread(&aResult);
     
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
    if (!aProxyObject) return NS_ERROR_NULL_POINTER;
    *aProxyObject = nsnull;
    
    // 1. Create a proxy for creating an instance on another thread.

    nsIProxyCreateInstance* ciProxy = nsnull;

    nsProxyCreateInstance* ciObject = new nsProxyCreateInstance(); 
    
    if (ciObject == nsnull)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(ciObject);
    
    nsresult rv = GetProxyForObject(destQueue, 
                                    NS_GET_IID(nsIProxyCreateInstance), 
                                    ciObject, 
                                    PROXY_SYNC, 
                                    (void**)&ciProxy);
    
    if (NS_FAILED(rv))
    {
        NS_RELEASE(ciObject);
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
    NS_RELEASE(ciObject);

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

/**
 * Helper function for code that already has a link-time dependency on
 * libxpcom and needs to get proxies in a bunch of different places.
 * This way, the caller isn't forced to get the proxy object manager
 * themselves every single time, thus making the calling code more
 * readable.
 */
NS_COM nsresult
NS_GetProxyForObject(nsIEventQueue *destQueue, 
                     REFNSIID aIID, 
                     nsISupports* aObj, 
                     PRInt32 proxyType, 
                     void** aProxyObject) 
{
    static NS_DEFINE_CID(proxyObjMgrCID, NS_PROXYEVENT_MANAGER_CID);

    nsresult rv;    // temp for return value

    // get the proxy object manager
    //
    nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = 
        do_GetService(proxyObjMgrCID, &rv);

    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    
    // and try to get the proxy object
    //
    return proxyObjMgr->GetProxyForObject(destQueue, aIID, aObj,
                                          proxyType, aProxyObject);
}
