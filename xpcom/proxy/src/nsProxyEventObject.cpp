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

#include "prprf.h"
#include "prmem.h"

#include "nsProxyEvent.h"
#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"

#include "nsHashtable.h"

#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"

#include "nsAutoLock.h"
#include "nslog.h"

NS_IMPL_LOG(nsProxyEventObjectLog)
#define PRINTF NS_LOG_PRINTF(nsProxyEventObjectLog)
#define FLUSH  NS_LOG_FLUSH(nsProxyEventObjectLog)

static NS_DEFINE_IID(kProxyObject_Identity_Class_IID, NS_PROXYEVENT_IDENTITY_CLASS_IID);

////////////////////////////////////////////////////////////////////////////////

class nsProxyEventKey : public nsHashKey
{
public:
    nsProxyEventKey(void* rootObjectKey, void* destQueueKey, PRInt32 proxyType)
        : mRootObjectKey(rootObjectKey), mDestQueueKey(destQueueKey), mProxyType(proxyType) {
    }
  
    PRUint32 HashCode(void) const {
        // XXX what about 64-bit machines?
        return (PRUint32)mRootObjectKey ^ (PRUint32)mDestQueueKey ^ mProxyType;
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
        PRINTF("\n-=-=-=-=-=-=-=-=-=-=-=-=-\n");
        PRINTF("%s\n", message);

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
    PRINTF("nsProxyEventObject @ %x with mRefCnt = %d\n", this, mRefCnt);

    PRBool isRoot = mRoot == nsnull;
    PRINTF("%s wrapper around  @ %x\n", isRoot ? "ROOT":"non-root\n", GetRealObject());

    nsCOMPtr<nsISupports> rootObject = do_QueryInterface(mProxyObject->mRealObject);
    nsCOMPtr<nsISupports> rootQueue = do_QueryInterface(mProxyObject->mDestQueue);
    nsProxyEventKey key(rootObject, rootQueue, mProxyObject->mProxyType);
    PRINTF("Hashkey: %d\n", key.HashCode());
        
    char* name;
    GetClass()->GetInterfaceInfo()->GetName(&name);
    PRINTF("interface name is %s\n", name);
    if(name)
        nsMemory::Free(name);
    char * iid = GetClass()->GetProxiedIID().ToString();
    PRINTF("IID number is %s\n", iid);
    delete iid;
    PRINTF("nsProxyEventClass @ %x\n", mClass);
    
    if(mNext)
    {
        if(isRoot)
        {
            PRINTF("Additional wrappers for this object...\n");
        }
        mNext->DebugDump(nsnull, 0);
    }

    PRINTF("[proxyobjects] %d total used in system, %d outstading\n", totalProxyObjects, outstandingProxyObjects);

    if (message)
        PRINTF("-=-=-=-=-=-=-=-=-=-=-=-=-\n");

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
    
    nsCOMPtr<nsProxyEventObject> proxy;
    nsCOMPtr<nsProxyEventObject> root;
    nsProxyEventObject* peo;

    // Get a class for this IID.
    nsCOMPtr<nsProxyEventClass> clazz = getter_AddRefs( nsProxyEventClass::GetNewOrUsedClass(aIID) );
    if(!clazz) return nsnull;
    
    // make sure that the object pass in is not a proxy.
    nsCOMPtr<nsProxyEventObject> aIdentificationObject;
    if (NS_SUCCEEDED(aObj->QueryInterface(kProxyObject_Identity_Class_IID, getter_AddRefs(aIdentificationObject))))
    {
        // someone is asking us to create a proxy for a proxy.  Lets get
        // the real object and build aproxy for that!
        aObj = aIdentificationObject->GetRealObject();
        aIdentificationObject = 0;
        if (aObj == nsnull) return nsnull;
    }

    // always find the native root if the |real| object.
	// this must not be a nsCOMPtr since we need to make sure that we do a QI.
    nsCOMPtr<nsISupports> rootObject;
	if(NS_FAILED(aObj->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(rootObject))))
    return nsnull;

    /* get our hash table */    
    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    if (manager == nsnull) return nsnull;
    nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();
    if (realToProxyMap == nsnull) return nsnull;

    // we need to do make sure that we addref the passed in object as well as ensure
    // that it is of the requested IID;  
	// this must not be a nsCOMPtr since we need to make sure that we do a QI.

    nsCOMPtr<nsISupports> requestedInterface;
	if(NS_FAILED(aObj->QueryInterface(aIID, getter_AddRefs(requestedInterface))))
    return nsnull;
    

    // this will be our key in the hash table.  
    // this must not be a nsCOMPtr since we need to make sure that we do a QI.
    nsCOMPtr<nsISupports> destQRoot;
    nsresult rv;
    destQRoot = do_QueryInterface(destQueue, &rv);
    if (NS_FAILED(rv))
        return nsnull;

    nsProxyEventKey rootkey(rootObject.get(), destQRoot.get(), proxyType);

    // find in our hash table 
    root  = (nsProxyEventObject*) realToProxyMap->Get(&rootkey);
	if(root)
    {
		proxy = root->Find(aIID);
        
        if(proxy)  
        {
            peo = proxy;
            NS_ADDREF(peo);
            return peo;  
        }
    }
    else
    {
        // build the root proxy
        if (aObj == rootObject.get())
        {
            // the root will do double duty as the interface wrapper
            peo = new nsProxyEventObject(destQueue, 
                                         proxyType, 
                                         requestedInterface, 
                                         clazz, 
                                         nsnull);
        
            proxy = do_QueryInterface(peo);
            
            if(proxy)
            {
                realToProxyMap->Put(&rootkey, peo);
                peo = proxy;
                NS_ADDREF(peo);
                return peo;  
            }
        }
        else
        {
            // just a root proxy
            nsCOMPtr<nsProxyEventClass> rootClazz = getter_AddRefs ( nsProxyEventClass::GetNewOrUsedClass(
                                                                      NS_GET_IID(nsISupports)) );
            
            if (!rootClazz)
            {
                return nsnull;
            }
                
            peo = new nsProxyEventObject(destQueue, 
                                         proxyType, 
                                         rootObject, 
                                         rootClazz, 
                                         nsnull);

            if(!peo)
            {
                return nsnull;
            }

            root = do_QueryInterface(peo);

            realToProxyMap->Put(&rootkey, peo);
        }
    }
    // at this point we have a root and may need to build the specific proxy
    NS_ASSERTION(root,"bad root");
    NS_ASSERTION(clazz,"bad clazz");

    if(!proxy)
    {
        peo = new nsProxyEventObject(destQueue, 
                                     proxyType, 
                                     requestedInterface, 
                                     clazz, 
                                     root);

        proxy = do_QueryInterface(peo);
        
        if(!proxy)
        {
            return nsnull;
        }
    }

    proxy->mNext = root->mNext;
    root->mNext = proxy;

    peo = proxy;
    NS_ADDREF(peo);
    return peo;  

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
    NS_INIT_REFCNT();
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
    if (mRoot != nsnull)
    {
        nsProxyEventObject* cur = mRoot;
        while(1)
        {
            if(cur->mNext == this)
            {
                cur->mNext = mNext;
                break;
            }
            cur = cur->mNext;
            NS_ASSERTION(cur, "failed to find wrapper in its own chain");
        }
    }
    else
    {
        if (! nsProxyObjectManager::IsManagerShutdown())
        {
            nsCOMPtr<nsProxyObjectManager> manager = nsProxyObjectManager::GetInstance();
            nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();

            if (realToProxyMap != nsnull)
            {
                nsCOMPtr<nsISupports> rootObject = do_QueryInterface(mProxyObject->mRealObject);
                nsCOMPtr<nsISupports> rootQueue = do_QueryInterface(mProxyObject->mDestQueue);
                nsProxyEventKey key(rootObject, rootQueue, mProxyObject->mProxyType);
                void* value = realToProxyMap->Remove(&key);
                NS_ASSERTION(value, "failed to remove from realToProxyMap");
            }
        }
    }
    // I am worried about ordering.
    // do not remove assignments.
    mProxyObject = 0;
    mClass       = 0;
    NS_IF_RELEASE(mRoot);
}

NS_IMPL_THREADSAFE_ADDREF(nsProxyEventObject);
NS_IMPL_THREADSAFE_RELEASE(nsProxyEventObject);

NS_IMETHODIMP
nsProxyEventObject::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if( aIID.Equals(GetIID()) )
    {
        *aInstancePtr = (void*) ( (nsISupports*)this );  //todo should use standard cast.
        NS_ADDREF_THIS();
        return NS_OK;
    }
        
    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}

nsProxyEventObject*
nsProxyEventObject::Find(REFNSIID aIID)
{
    if(aIID.Equals(GetClass()->GetProxiedIID()))
    {
		return this;
	}

    if(aIID.Equals(NS_GET_IID(nsISupports)))
    {
        return this;
    }

	nsProxyEventObject* cur = (mRoot ? mRoot : this);

    do
    {
        if(aIID.Equals(GetClass()->GetProxiedIID()))
        {
            return cur;
        }

    } while(NULL != (cur = cur->mNext));

    return NULL;
}


NS_IMETHODIMP
nsProxyEventObject::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    NS_ENSURE_ARG_POINTER(info);
    NS_ASSERTION(GetClass(), "proxy without class");
    NS_ASSERTION(GetClass()->GetInterfaceInfo(), "proxy class without interface");

    if(!(*info = GetClass()->GetInterfaceInfo()))
        return NS_ERROR_UNEXPECTED;

    NS_ADDREF(*info);
    return NS_OK;
}

NS_IMETHODIMP
nsProxyEventObject::CallMethod(PRUint16 methodIndex,
                               const nsXPTMethodInfo* info,
                               nsXPTCMiniVariant * params)
{
    if (mProxyObject)
        return mProxyObject->Post(methodIndex, (nsXPTMethodInfo*)info, params, GetClass()->GetInterfaceInfo());

    return NS_ERROR_NULL_POINTER;
}
