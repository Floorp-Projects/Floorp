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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "prprf.h"
#include "prmem.h"

#include "nscore.h"
#include "nsProxyEvent.h"
#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"

#include "nsHashtable.h"

#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"

#include "nsAutoLock.h"

static NS_DEFINE_IID(kProxyObject_Identity_Class_IID, NS_PROXYEVENT_IDENTITY_CLASS_IID);


////////////////////////////////////////////////////////////////////////////////

class nsProxyEventKey : public nsHashKey
{
public:
    nsProxyEventKey(void* rootObjectKey, void* destQueueKey, PRInt32 proxyType)
        : mRootObjectKey(rootObjectKey), mDestQueueKey(destQueueKey), mProxyType(proxyType) {
    }
  
    PRUint32 HashCode(void) const {
        return NS_PTR_TO_INT32(mRootObjectKey) ^ 
            NS_PTR_TO_INT32(mDestQueueKey) ^ mProxyType;
    }

    PRBool Equals(const nsHashKey *aKey) const {
        const nsProxyEventKey* other = (const nsProxyEventKey*)aKey;
        return mRootObjectKey == other->mRootObjectKey
            && mDestQueueKey == other->mDestQueueKey
            && mProxyType == other->mProxyType;
    }

    nsHashKey *Clone() const {
        return new nsProxyEventKey(mRootObjectKey, mDestQueueKey, mProxyType);
    }

protected:
    void*       mRootObjectKey;
    void*       mDestQueueKey;
    PRInt32     mProxyType;
};

////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_xpcom_proxy
static PRMonitor* mon = nsnull;
static PRUint32 totalProxyObjects = 0;
static PRUint32 outstandingProxyObjects = 0;

void
nsProxyEventObject::DebugDump(const char * message, PRUint32 hashKey)
{

    if (mon == nsnull)
    {
        mon = PR_NewMonitor();
    }

    PR_EnterMonitor(mon);

    if (message)
    {
        printf("\n-=-=-=-=-=-=-=-=-=-=-=-=-\n");
        printf("%s\n", message);

        if(strcmp(message, "Create") == 0)
        {
            totalProxyObjects++;
            outstandingProxyObjects++;
        }
        else if(strcmp(message, "Delete") == 0)
        {
            outstandingProxyObjects--;
        }
    }
    printf("nsProxyEventObject @ %x with mRefCnt = %d\n", this, mRefCnt);

    PRBool isRoot = mRoot == nsnull;
    printf("%s wrapper around  @ %x\n", isRoot ? "ROOT":"non-root\n", GetRealObject());

    nsCOMPtr<nsISupports> rootObject = do_QueryInterface(mProxyObject->mRealObject);
    nsCOMPtr<nsISupports> rootQueue = do_QueryInterface(mProxyObject->mDestQueue);
    nsProxyEventKey key(rootObject, rootQueue, mProxyObject->mProxyType);
    printf("Hashkey: %d\n", key.HashCode());
        
    char* name;
    GetClass()->GetInterfaceInfo()->GetName(&name);
    printf("interface name is %s\n", name);
    if(name)
        nsMemory::Free(name);
    char * iid = GetClass()->GetProxiedIID().ToString();
    printf("IID number is %s\n", iid);
    delete iid;
    printf("nsProxyEventClass @ %x\n", mClass);
    
    if(mNext)
    {
        if(isRoot)
        {
            printf("Additional wrappers for this object...\n");
        }
        mNext->DebugDump(nsnull, 0);
    }

    printf("[proxyobjects] %d total used in system, %d outstading\n", totalProxyObjects, outstandingProxyObjects);

    if (message)
        printf("-=-=-=-=-=-=-=-=-=-=-=-=-\n");

    PR_ExitMonitor(mon);
}
#endif



//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  nsProxyEventObject
//
//////////////////////////////////////////////////////////////////////////////////////////////////
nsProxyEventObject* 
nsProxyEventObject::GetNewOrUsedProxy(nsIEventQueue *destQueue,
                                      PRInt32 proxyType, 
                                      nsISupports *aObj,
                                      REFNSIID aIID)
{
    nsresult rv;

    if (!aObj)
        return nsnull;

    nsISupports* rawObject = aObj;

    //
    // make sure that the object pass in is not a proxy...
    // if the object *is* a proxy, then be nice and build the proxy
    // for the real object...
    //
    nsCOMPtr<nsProxyEventObject> identificationObject;

    rv = rawObject->QueryInterface(kProxyObject_Identity_Class_IID,
                                   getter_AddRefs(identificationObject));
    if (NS_SUCCEEDED(rv)) {
        //
        // ATTENTION!!!!
        //
        // If you are hitting any of the assertions in this block of code, 
        // please contact dougt@netscape.com.
        //

        // if you hit this assertion, you might want to check out how 
        // you are using proxies.  You shouldn't need to be creating
        // a proxy from a proxy.  -- dougt@netscape.com
        NS_ASSERTION(0, "Someone is building a proxy from a proxy");
        
        NS_ASSERTION(identificationObject, "where did my identification object go!");
        if (!identificationObject) {
            return nsnull;
        }

        // someone is asking us to create a proxy for a proxy.  Lets get
        // the real object and build aproxy for that!
        rawObject = identificationObject->GetRealObject();
        
        NS_ASSERTION(rawObject, "where did my real object go!");
        if (!rawObject) {
            return nsnull;
        }
    }

    //
    // Get the root nsISupports of the |real| object.
    //
    nsCOMPtr<nsISupports> rootObject;

    rootObject = do_QueryInterface(rawObject, &rv);
    if (NS_FAILED(rv) || !rootObject) {
        NS_ASSERTION(NS_FAILED(rv), "where did my root object go!");
        return nsnull;
    }

    // Get the root nsISupports of the event queue...  This is used later,
    // as part of the hashtable key...
    nsCOMPtr<nsISupports> destQRoot = do_QueryInterface(destQueue, &rv);
    if (NS_FAILED(rv) || !destQRoot) {
        return nsnull;
    }

    //
    // Enter the proxy object creation lock.
    //
    // This lock protects thev linked list which chains proxies together 
    // (ie. mRoot and mNext) and ensures that there is no hashtable contention
    // between the time that a proxy is looked up (and not found) in the
    // hashtable and then added...
    //
    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    if (!manager) {
        return nsnull;
    }

    nsAutoMonitor mon(manager->GetMonitor());

    // Get the hash table containing root proxy objects...
    nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();
    if (!realToProxyMap) {
        return nsnull;
    }

    // Now, lookup the root nsISupports of the raw object in the hashtable
    // The key consists of 3 pieces of information:
    //    - root nsISupports of the raw object
    //    - event queue of the current thread
    //    - type of proxy being constructed...
    //
    nsProxyEventKey rootkey(rootObject.get(), destQRoot.get(), proxyType);

    nsCOMPtr<nsProxyEventObject> rootProxy;
    nsCOMPtr<nsProxyEventObject> proxy;
    nsProxyEventObject* peo;

    // find in our hash table 
    rootProxy = (nsProxyEventObject*) realToProxyMap->Get(&rootkey);

    if(rootProxy) {
        //
        // At least one proxy has already been created for this raw object...
        //
        // Look for the specific interface proxy in the list off of
        // the root proxy...
        //
        peo = rootProxy->LockedFind(aIID);

        if(peo) {
            // An existing proxy is available... So use it.
            NS_ADDREF(peo);
            return peo;  
        }
    }
    else {
        // build the root proxy
        nsCOMPtr<nsProxyEventClass> rootClazz;

        rootClazz = dont_AddRef(nsProxyEventClass::GetNewOrUsedClass(
                                NS_GET_IID(nsISupports)));
        if (!rootClazz) {
            return nsnull;
        }

        peo = new nsProxyEventObject(destQueue, 
                                     proxyType, 
                                     rootObject, 
                                     rootClazz, 
                                     nsnull);
        if(!peo) {
            // Ouch... Out of memory!
            return nsnull;
        }

        // Add this root proxy into the hash table...
        realToProxyMap->Put(&rootkey, peo);

        if(aIID.Equals(NS_GET_IID(nsISupports))) {
            //
            // Since the requested proxy is for the nsISupports interface of
            // the raw object, use the new root proxy as the specific proxy
            // too...
            //
            NS_ADDREF(peo);
            return peo;
        }

        // This assignment is an owning reference to the new ProxyEventObject.
        // So, it will automatically get deleted if any subsequent early 
        // returns are taken...
        rootProxy = do_QueryInterface(peo);
    }

    //
    // at this point we have a proxy for the root nsISupports (ie. rootProxy)
    // but we need to build the specific proxy for this interface...
    //
    NS_ASSERTION(rootProxy, "What happened to the root proxy!");

    // Get a class for this IID.
    nsCOMPtr<nsProxyEventClass> proxyClazz;
        
    proxyClazz = dont_AddRef(nsProxyEventClass::GetNewOrUsedClass(aIID));
    if(!proxyClazz) {
        return nsnull;
    }

    // Get the raw interface for this IID
    nsCOMPtr<nsISupports> rawInterface;

    rv = rawObject->QueryInterface(aIID, getter_AddRefs(rawInterface));
    if (NS_FAILED(rv) || !rawInterface) {
        NS_ASSERTION(NS_FAILED(rv), "where did my rawInterface object go!");
        return nsnull;
    }

    peo = new nsProxyEventObject(destQueue, 
                                 proxyType, 
                                 rawInterface, 
                                 proxyClazz, 
                                 rootProxy);
    if (!peo) {
        // Ouch... Out of memory!
        return nsnull;
    }

    //
    // Add the new specific proxy to the head of the list of proxies hanging
    // off of the rootProxy...
    //
    peo->mNext       = rootProxy->mNext;
    rootProxy->mNext = peo;

    NS_ADDREF(peo);
    return peo;  

}

nsProxyEventObject* nsProxyEventObject::LockedFind(REFNSIID aIID)
{
    if(aIID.Equals(mClass->GetProxiedIID())) {
        return this;
    }

    if(aIID.Equals(NS_GET_IID(nsISupports))) {
        return this;
    }

    nsProxyEventObject* cur = (mRoot ? mRoot : mNext);
    while(cur) {
        if(aIID.Equals(cur->GetClass()->GetProxiedIID())) {
            return cur;
        }
        cur = cur->mNext;
    }

    return nsnull;
}

nsProxyEventObject::nsProxyEventObject()
: mNext(nsnull)
{
     NS_WARNING("This constructor should never be called");
}

nsProxyEventObject::nsProxyEventObject(nsIEventQueue *destQueue,
                                       PRInt32 proxyType,
                                       nsISupports* aObj,
                                       nsProxyEventClass* aClass,
                                       nsProxyEventObject* root)
    : mClass(aClass),
      mRoot(root),
      mNext(nsnull)
{
    NS_IF_ADDREF(mRoot);

    mProxyObject = new nsProxyObject(destQueue, proxyType, aObj);

#ifdef DEBUG_xpcom_proxy
    DebugDump("Create", 0);
#endif
}

nsProxyEventObject::~nsProxyEventObject()
{
#ifdef DEBUG_xpcom_proxy
    DebugDump("Delete", 0);
#endif
    if (mRoot) {
        //
        // This proxy is not the root interface so it must be removed
        // from the chain of proxies...
        //
        nsProxyEventObject* cur = mRoot;
        while(cur) {
            if(cur->mNext == this) {
                cur->mNext = mNext;
                mNext = nsnull;
                break;
            }
            cur = cur->mNext;
        }
        NS_ASSERTION(cur, "failed to find wrapper in its own chain");
    }
    else {
        //
        // This proxy is for the root interface.  Each proxy in the chain
        // has a strong reference to the root... So, when its refcount goes
        // to zero, it safe to remove it because no proxies are in its chain.
        //
        if (! nsProxyObjectManager::IsManagerShutdown()) {
            nsProxyObjectManager* manager = nsProxyObjectManager::GetInstance();
            nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();

            NS_ASSERTION(!mNext, "There are still proxies in the chain!");

            if (realToProxyMap != nsnull) {
                nsCOMPtr<nsISupports> rootObject = do_QueryInterface(mProxyObject->mRealObject);
                nsCOMPtr<nsISupports> rootQueue = do_QueryInterface(mProxyObject->mDestQueue);
                nsProxyEventKey key(rootObject, rootQueue, mProxyObject->mProxyType);
#ifdef DEBUG_dougt
                void* value =
#endif
                    realToProxyMap->Remove(&key);
#ifdef DEBUG_dougt
                NS_ASSERTION(value, "failed to remove from realToProxyMap");
#endif
            }
        }
    }

    // I am worried about ordering.
    // do not remove assignments.
    mProxyObject = nsnull;
    mClass       = nsnull;
    NS_IF_RELEASE(mRoot);
}

//
// nsISupports implementation...
//

NS_IMPL_THREADSAFE_ADDREF(nsProxyEventObject)

NS_IMETHODIMP_(nsrefcnt)
nsProxyEventObject::Release(void)
{
    //
    // Be pessimistic about whether the manager or even the monitor exist...
    // This is to protect against shutdown issues where a proxy object could
    // be destroyed after (or while) the Proxy Manager is being destroyed...
    //
    nsProxyObjectManager* manager = nsProxyObjectManager::GetInstance();
    nsAutoMonitor mon(manager ? manager->GetMonitor() : nsnull);

    nsrefcnt count;
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    // Decrement atomically - in case the Proxy Object Manager has already
    // been deleted and the monitor is unavailable...
    count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
    NS_LOG_RELEASE(this, count, "nsProxyEventObject");
    if (0 == count) {
        mRefCnt = 1; /* stabilize */
        //
        // Remove the proxy from the hashtable (if necessary) or its
        // proxy chain.  This must be done inside of the proxy lock to
        // prevent GetNewOrUsedProxy(...) from ressurecting it...
        //
        NS_DELETEXPCOM(this);
        return 0;
    }
    return count;
}

NS_IMETHODIMP
nsProxyEventObject::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if( aIID.Equals(GetIID()) )
    {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
        
    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}

//
// nsXPTCStubBase implementation...
//

NS_IMETHODIMP
nsProxyEventObject::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    NS_ENSURE_ARG_POINTER(info);

    *info = mClass->GetInterfaceInfo();

    NS_ASSERTION(*info, "proxy class without interface");
    if (!*info) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_ADDREF(*info);
    return NS_OK;
}

NS_IMETHODIMP
nsProxyEventObject::CallMethod(PRUint16 methodIndex,
                               const nsXPTMethodInfo* info,
                               nsXPTCMiniVariant * params)
{
    nsresult rv;

    if (mProxyObject) {
        rv = mProxyObject->Post(methodIndex,
                                (nsXPTMethodInfo*)info,
                                params,
                                mClass->GetInterfaceInfo());
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}
