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
#include "nsThreadUtils.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsIThread.h"


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

static PRIntn
PurgeProxyClasses(nsHashKey *aKey, void *aData, void* closure)
{
    nsProxyEventClass* ptr = NS_STATIC_CAST(nsProxyEventClass*, aData);
    NS_RELEASE(ptr);
    return kHashEnumerateNext;
}

nsProxyObjectManager::~nsProxyObjectManager()
{
    mProxyClassMap.Reset(PurgeProxyClasses, nsnull);

    if (mProxyCreationMonitor)
        PR_DestroyMonitor(mProxyCreationMonitor);

    nsProxyObjectManager::mInstance = nsnull;
}

PRBool
nsProxyObjectManager::IsManagerShutdown()
{
    return mInstance == nsnull;
}

nsProxyObjectManager *
nsProxyObjectManager::GetInstance()
{
    if (!mInstance) 
        mInstance = new nsProxyObjectManager();
    return mInstance;
}

void
nsProxyObjectManager::Shutdown()
{
    mInstance = nsnull;
}

NS_IMETHODIMP 
nsProxyObjectManager::Create(nsISupports* outer, const nsIID& aIID,
                             void* *aInstancePtr)
{
    nsProxyObjectManager *proxyObjectManager = GetInstance();
    if (!proxyObjectManager)
        return NS_ERROR_OUT_OF_MEMORY;

    return proxyObjectManager->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP 
nsProxyObjectManager::GetProxyForObject(nsIEventTarget* aTarget, 
                                        REFNSIID aIID, 
                                        nsISupports* aObj, 
                                        PRInt32 proxyType, 
                                        void** aProxyObject)
{
    NS_ENSURE_ARG_POINTER(aObj);

    *aProxyObject = nsnull;

    // handle special values
    nsCOMPtr<nsIThread> thread;
    if (aTarget == NS_PROXY_TO_CURRENT_THREAD) {
      aTarget = NS_GetCurrentThread();
    } else if (aTarget == NS_PROXY_TO_MAIN_THREAD) {
      thread = do_GetMainThread();
      aTarget = thread.get();
    }

    // check to see if the target is on our thread.  If so, just return the
    // real object.
    
    if (!(proxyType & NS_PROXY_ASYNC) && !(proxyType & NS_PROXY_ALWAYS))
    {
        PRBool result;
        aTarget->IsOnCurrentThread(&result);
     
        if (result)
            return aObj->QueryInterface(aIID, aProxyObject);
    }
    
    // check to see if proxy is there or not.
    *aProxyObject = nsProxyEventObject::GetNewOrUsedProxy(aTarget, proxyType,
                                                          aObj, aIID);
    if (!*aProxyObject)
        return NS_ERROR_UNEXPECTED;
        
    return NS_OK;
}

/**
 * Helper function for code that already has a link-time dependency on
 * libxpcom and needs to get proxies in a bunch of different places.
 * This way, the caller isn't forced to get the proxy object manager
 * themselves every single time, thus making the calling code more
 * readable.
 */
NS_COM nsresult
NS_GetProxyForObject(nsIEventTarget *target, 
                     REFNSIID aIID, 
                     nsISupports* aObj, 
                     PRInt32 proxyType, 
                     void** aProxyObject) 
{
    static NS_DEFINE_CID(proxyObjMgrCID, NS_PROXYEVENT_MANAGER_CID);

    nsresult rv;

    // get the proxy object manager
    //
    nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = 
        do_GetService(proxyObjMgrCID, &rv);
    if (NS_FAILED(rv))
        return rv;
    
    // and try to get the proxy object
    //
    return proxyObjMgr->GetProxyForObject(target, aIID, aObj,
                                          proxyType, aProxyObject);
}
