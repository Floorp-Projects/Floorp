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

#include "nsHashtable.h"

#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"
static NS_DEFINE_IID(kProxyObject_Identity_Class_IID, NS_PROXYEVENT_IDENTITY_CLASS_IID);


#ifdef DEBUG_dougt
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
    
    if (mHashKey.HashValue()!=0)
        printf("Hashkey: %d\n", mHashKey.HashValue());
        
    char* name;
    GetClass()->GetInterfaceInfo()->GetName(&name);
    printf("interface name is %s\n", name);
    if(name)
        nsAllocator::Free(name);
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
    
    nsCOMPtr<nsProxyEventObject> proxy = 0;
    nsCOMPtr<nsProxyEventObject> root  = 0;
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
    nsCOMPtr<nsISupports> rootObject;
    if(NS_FAILED(aObj->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), getter_AddRefs(rootObject))))
        return nsnull;

    // this will be our key in the hash table.  
    nsVoidKey rootkey( (void*) GetProxyHashKey(rootObject.get(), destQueue, proxyType) );
    
    /* get our hash table */    
    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    if (manager == nsnull) return nsnull;
    
    nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();
    if (realToProxyMap == nsnull) return nsnull;

    // we need to do make sure that we addref the passed in object as well as ensure
    // that it is of the requested IID;
    nsCOMPtr<nsISupports> requestedInterface;
    if(NS_FAILED(aObj->QueryInterface(aIID, getter_AddRefs(requestedInterface))))
        return nsnull;

   /* find in our hash table */    
    if(realToProxyMap->Exists(&rootkey))
    {
        root  = (nsProxyEventObject*) realToProxyMap->Get(&rootkey);
        proxy = root->Find(aIID);
        
        if(proxy)
            goto return_proxy;
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
                                         nsnull, 
                                         rootkey.HashValue());
        
            proxy = do_QueryInterface(peo);
            
            if(proxy)
                realToProxyMap->Put(&rootkey, peo);
            
            goto return_proxy;
        }
        else
        {
            // just a root proxy
            nsCOMPtr<nsProxyEventClass> rootClazz = getter_AddRefs ( nsProxyEventClass::GetNewOrUsedClass(
                                                                      nsCOMTypeInfo<nsISupports>::GetIID()) );
            
            peo = new nsProxyEventObject(destQueue, 
                                         proxyType, 
                                         rootObject, 
                                         rootClazz, 
                                         nsnull, 
                                         rootkey.HashValue());

            if(!peo || !rootClazz)
                return nsnull;

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
                                     root, 
                                     0);

        proxy = do_QueryInterface(peo);
        
        if(!proxy)
            return nsnull;
    }

    proxy->mNext = root->mNext;
    root->mNext = proxy;

return_proxy:

    if(proxy)  
    {
        peo = proxy;
        NS_ADDREF(peo);
        return peo;  
    }

    return nsnull;
}
nsProxyEventObject::nsProxyEventObject()
: mNext(nsnull),
  mHashKey((void*)0)
{
}

nsProxyEventObject::nsProxyEventObject(nsIEventQueue *destQueue,
                                       PRInt32 proxyType,
                                       nsISupports* aObj,
                                       nsProxyEventClass* aClass,
                                       nsProxyEventObject* root,
                                       PRUint32 hashValue)
    : mNext(nsnull),
      mHashKey((void*)hashValue)
{
    NS_INIT_REFCNT();

    mRoot        = root;
    mClass       = aClass;
    
    mProxyObject = new nsProxyObject(destQueue, proxyType, aObj);
            
#ifdef DEBUG_dougt
DebugDump("Create", 0);
#endif
}

nsProxyEventObject::~nsProxyEventObject()
{
#ifdef DEBUG_dougt
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
        nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
        nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();

        if (realToProxyMap != nsnull && mHashKey.HashValue() != 0)
        {
            realToProxyMap->Remove(&mHashKey);
        }
    }   
    // I am worried about ordering.
    // do not remove assignments.
    mProxyObject = 0;
    mClass       = 0;
    mRoot        = 0;
}

NS_IMPL_ADDREF(nsProxyEventObject);
NS_IMPL_RELEASE(nsProxyEventObject);

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

PRUint32 
nsProxyEventObject::GetProxyHashKey(nsISupports *rootProxy, nsIEventQueue *destQueue, PRInt32 proxyType)
{ // FIX I need to worry about nsCOMPtr for rootProxy, and destQueue!
    
    nsISupports* destQRoot;
    if(NS_FAILED(destQueue->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), (void**)&destQRoot)))
        return 0;
    PRInt32 value = (PRUint32)rootProxy + (PRUint32)destQRoot + proxyType;
    NS_RELEASE(destQRoot);
    return value;
}

nsProxyEventObject*
nsProxyEventObject::Find(REFNSIID aIID)
{
    nsProxyEventObject* cur = (mRoot ? mRoot.get() : this);

    if(aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
    {
        return cur;
    }

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

















