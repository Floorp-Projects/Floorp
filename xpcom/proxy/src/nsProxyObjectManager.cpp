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

#include "nsProxyEventPrivate.h"

#include "nsIComponentManager.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsIThread.h"

#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "xptiprivate.h"

using namespace mozilla;

#ifdef PR_LOGGING
PRLogModuleInfo *nsProxyObjectManager::sLog = PR_NewLogModule("xpcomproxy");
#endif

class nsProxyEventKey : public nsHashKey
{
public:
    nsProxyEventKey(void* rootObjectKey, void* targetKey, PRInt32 proxyType)
        : mRootObjectKey(rootObjectKey), mTargetKey(targetKey), mProxyType(proxyType) {
    }
  
    PRUint32 HashCode(void) const {
        return NS_PTR_TO_INT32(mRootObjectKey) ^ 
            NS_PTR_TO_INT32(mTargetKey) ^ mProxyType;
    }

    bool Equals(const nsHashKey *aKey) const {
        const nsProxyEventKey* other = (const nsProxyEventKey*)aKey;
        return mRootObjectKey == other->mRootObjectKey
            && mTargetKey == other->mTargetKey
            && mProxyType == other->mProxyType;
    }

    nsHashKey *Clone() const {
        return new nsProxyEventKey(mRootObjectKey, mTargetKey, mProxyType);
    }

protected:
    void*       mRootObjectKey;
    void*       mTargetKey;
    PRInt32     mProxyType;
};

/////////////////////////////////////////////////////////////////////////
// nsProxyObjectManager
/////////////////////////////////////////////////////////////////////////

nsProxyObjectManager* nsProxyObjectManager::gInstance = nsnull;

NS_IMPL_QUERY_INTERFACE1(nsProxyObjectManager, nsIProxyObjectManager)

NS_IMETHODIMP_(nsrefcnt)
nsProxyObjectManager::AddRef()
{
    return 2;
}

NS_IMETHODIMP_(nsrefcnt)
nsProxyObjectManager::Release()
{
    return 1;
}

nsProxyObjectManager::nsProxyObjectManager()
    : mProxyObjectMap(256, PR_FALSE)
    , mProxyCreationLock("nsProxyObjectManager.mProxyCreationLock")
{
    mProxyClassMap.Init(256);
}

nsProxyObjectManager::~nsProxyObjectManager()
{
    mProxyClassMap.Clear();

    nsProxyObjectManager::gInstance = nsnull;
}

bool
nsProxyObjectManager::IsManagerShutdown()
{
    return gInstance == nsnull;
}

nsProxyObjectManager *
nsProxyObjectManager::GetInstance()
{
    if (!gInstance) 
        gInstance = new nsProxyObjectManager();
    return gInstance;
}

void
nsProxyObjectManager::Shutdown()
{
    delete gInstance;
    NS_ASSERTION(!gInstance, "Destructor didn't null gInstance?");
}

nsresult
nsProxyObjectManager::Create(nsISupports* outer, const nsIID& aIID,
                             void* *aInstancePtr)
{
    nsProxyObjectManager *proxyObjectManager = GetInstance();
    if (!proxyObjectManager)
        return NS_ERROR_OUT_OF_MEMORY;

    return proxyObjectManager->QueryInterface(aIID, aInstancePtr);
}

class nsProxyLockedRefPtr
{
public:
    nsProxyLockedRefPtr(nsProxyObject* aPtr) :
        mProxyObject(aPtr)
    {
        if (mProxyObject)
            mProxyObject->LockedAddRef();
    }

    ~nsProxyLockedRefPtr()
    {
        if (mProxyObject)
            mProxyObject->LockedRelease();
    }

    operator nsProxyObject*() const
    {
        return mProxyObject;
    }

    nsProxyObject* operator->() const
    {
        return mProxyObject;
    }

private:
    nsProxyObject *mProxyObject;
};

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
        bool result;
        aTarget->IsOnCurrentThread(&result);
     
        if (result)
            return aObj->QueryInterface(aIID, aProxyObject);
    }
    
    nsCOMPtr<nsISupports> realObj = do_QueryInterface(aObj);

    // Make sure the object passed in is not a proxy; if it is, be nice and
    // build the proxy for the real object.
    nsCOMPtr<nsProxyObject> po = do_QueryInterface(aObj);
    if (po) {
        realObj = po->GetRealObject();
    }

    nsCOMPtr<nsISupports> realEQ = do_QueryInterface(aTarget);

    nsProxyEventKey rootKey(realObj, realEQ, proxyType);

    {
        MutexAutoLock lock(mProxyCreationLock);
        nsProxyLockedRefPtr root =
            (nsProxyObject*) mProxyObjectMap.Get(&rootKey);
        if (root)
            return root->LockedFind(aIID, aProxyObject);
    }

    // don't lock while creating the nsProxyObject
    nsProxyObject *newRoot = new nsProxyObject(aTarget, proxyType, realObj);
    if (!newRoot)
        return NS_ERROR_OUT_OF_MEMORY;

    // lock again, and check for a race putting into mProxyObjectMap
    {
        MutexAutoLock lock(mProxyCreationLock);
        nsProxyLockedRefPtr root = 
            (nsProxyObject*) mProxyObjectMap.Get(&rootKey);
        if (root) {
            delete newRoot;
            return root->LockedFind(aIID, aProxyObject);
        }

        mProxyObjectMap.Put(&rootKey, newRoot);

        nsProxyLockedRefPtr kungFuDeathGrip(newRoot);
        return newRoot->LockedFind(aIID, aProxyObject);
    }
}

void
nsProxyObjectManager::LockedRemove(nsProxyObject *aProxy)
{
    nsCOMPtr<nsISupports> realEQ = do_QueryInterface(aProxy->GetTarget());

    nsProxyEventKey rootKey(aProxy->GetRealObject(), realEQ, aProxy->GetProxyType());

    if (!mProxyObjectMap.Remove(&rootKey)) {
        NS_ERROR("nsProxyObject not found in global hash.");
    }
}

nsresult
nsProxyObjectManager::GetClass(REFNSIID aIID, nsProxyEventClass **aResult)
{
    {
        MutexAutoLock lock(mProxyCreationLock);
        if (mProxyClassMap.Get(aIID, aResult)) {
            NS_ASSERTION(*aResult, "Null data in mProxyClassMap");
            return NS_OK;
        }
    }

    nsIInterfaceInfoManager *iim =
        xptiInterfaceInfoManager::GetSingleton();
    if (!iim)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIInterfaceInfo> ii;
    nsresult rv = iim->GetInfoForIID(&aIID, getter_AddRefs(ii));
    if (NS_FAILED(rv))
        return rv;

    nsProxyEventClass *pec = new nsProxyEventClass(aIID, ii);
    if (!pec)
        return NS_ERROR_OUT_OF_MEMORY;

    // Re-lock to put this class into our map. Before putting, check to see
    // if another thread raced to put before us
    MutexAutoLock lock(mProxyCreationLock);

    if (mProxyClassMap.Get(aIID, aResult)) {
        NS_ASSERTION(*aResult, "Null data in mProxyClassMap");
        delete pec;
        return NS_OK;
    }

    if (!mProxyClassMap.Put(aIID, pec)) {
        delete pec;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    *aResult = pec;
    return NS_OK;
}

/**
 * Helper function for code that already has a link-time dependency on
 * libxpcom and needs to get proxies in a bunch of different places.
 * This way, the caller isn't forced to get the proxy object manager
 * themselves every single time, thus making the calling code more
 * readable.
 */
nsresult
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
