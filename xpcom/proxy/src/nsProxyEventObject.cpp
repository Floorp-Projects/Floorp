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


//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  nsProxyEventObject
//
//////////////////////////////////////////////////////////////////////////////////////////////////



nsProxyEventObject* 
nsProxyEventObject::GetNewOrUsedProxy(nsIEventQueue *destQueue,
                                      ProxyType proxyType, 
                                      nsISupports *aObj,
                                      REFNSIID aIID)
{
    nsProxyEventObject* proxy = NULL;
    nsProxyEventObject* root = NULL;
    

    nsProxyEventClass* clazz = nsProxyEventClass::GetNewOrUsedClass(aIID);
    if(!clazz)
        return NULL;

     nsISupports* rootObject;
    // always find the native root
    if(NS_FAILED(aObj->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), (void**)&rootObject)))
        return NULL;


    /* find in our hash table */
    
    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();

    if (realToProxyMap == nsnull)
    {
        if(clazz)
        NS_RELEASE(clazz);
        return nsnull;
    }

    nsVoidKey rootkey(rootObject);
    if(realToProxyMap->Exists(&rootkey))
    {
        root = (nsProxyEventObject*) realToProxyMap->Get(&rootkey);
        proxy = root->Find(aIID);

        if(proxy)
        {
            NS_ADDREF(proxy);
            goto return_wrapper;
        }
    }
    else
    {
        // build the root proxy
        if (aObj == rootObject)
        {
            // the root will do double duty as the interface wrapper
            proxy = root = new nsProxyEventObject(destQueue, proxyType, aObj, clazz, nsnull);
            if(root)
            {
            	nsVoidKey aKey(root);	
                realToProxyMap->Put(&aKey, root);
            }
            goto return_wrapper;
        }
        else
        {
            // just a root proxy
            nsProxyEventClass* rootClazz = nsProxyEventClass::GetNewOrUsedClass(nsCOMTypeInfo<nsISupports>::GetIID());
            if(!rootClazz)
            {
                goto return_wrapper;
            }

            root = new nsProxyEventObject(destQueue, proxyType, rootObject, rootClazz, nsnull);
            NS_RELEASE(rootClazz);

            if(!root)
            {
                goto return_wrapper;
            }
            
            nsVoidKey aKey(root);
            realToProxyMap->Put(&aKey, root);
        }
    }
    // at this point we have a root and may need to build the specific proxy
    NS_ASSERTION(root,"bad root");
    NS_ASSERTION(clazz,"bad clazz");

    if(!proxy)
    {
        proxy = new nsProxyEventObject(destQueue, proxyType, aObj, clazz, root);
        if(!proxy)
        {
            goto return_wrapper;
        }
    }
    proxy->mNext = root->mNext;
    root->mNext = proxy;

return_wrapper:

    if(clazz)
        NS_RELEASE(clazz);
    
    
    // Since we do not want to extra references, we will release the rootObject.
    // 
    // There are one or more references to aObj which is passed in.  When we create
    // a new proxy object via nsProxyEventObject(), that will addRef the aObj.  Because
    // we QI addref to insure that we have a nsISupports ptr, its refcount goes up by one.
    // here we will decrement that.
    //
    if (rootObject)
        NS_RELEASE(rootObject);
    
    return proxy;
}





nsProxyEventObject::nsProxyEventObject(nsIEventQueue *destQueue,
                                       ProxyType proxyType,
                                       nsISupports* aObj,
                                       nsProxyEventClass* aClass,
                                       nsProxyEventObject* root)
    : mClass(aClass),
      mNext(NULL)
{
    mRoot           = (root ? root : this);
    mProxyObject    = new nsProxyObject(destQueue, proxyType, aObj);

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    NS_ADDREF(aClass);
}

nsProxyEventObject::~nsProxyEventObject()
{
    NS_PRECONDITION(0 == mRefCnt, "refcounting error");
    if(mRoot == this && GetClass())
    {   
        nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
        nsHashtable *realToProxyMap = manager->GetRealObjectToProxyObjectMap();

        if (realToProxyMap != nsnull)
        {
        	nsVoidKey key(this);
            realToProxyMap->Remove(&key);
        }
    }

    if (mProxyObject != nsnull)
       mProxyObject->Release();

    NS_RELEASE(mClass);
    
    if(mNext)
        NS_DELETEXPCOM(mNext);  // cascaded delete
}


NS_IMETHODIMP
nsProxyEventObject::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if(NULL == aInstancePtr)
    {
        NS_PRECONDITION(0, "null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}


nsrefcnt
nsProxyEventObject::AddRef(void)
{
    NS_PRECONDITION(mRoot, "bad root");

    if(1 == ++mRefCnt && mRoot && mRoot != this)
        NS_ADDREF(mRoot);

    return mRefCnt;
}

nsrefcnt
nsProxyEventObject::Release(void)
{
    NS_PRECONDITION(mRoot, "bad root");
    NS_PRECONDITION(0 != mRefCnt, "dup release");

    if(0 == --mRefCnt)
    {
        if(mRoot == this)
        {
            NS_DELETEXPCOM(this);   // cascaded delete
        }
        else
        {
            mRoot->Release();
        }
        return 0;
    }
    return mRefCnt;
}


nsProxyEventObject*
nsProxyEventObject::Find(REFNSIID aIID)
{
    if(aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
        return mRoot;

    nsProxyEventObject* cur = mRoot;
    do
    {
        if(aIID.Equals(GetIID()))
            return cur;

    } while(NULL != (cur = cur->mNext));

    return NULL;
}


NS_IMETHODIMP
nsProxyEventObject::GetInterfaceInfo(nsIInterfaceInfo** info)
{
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
    return mProxyObject->Post(methodIndex, (nsXPTMethodInfo*)info, params, GetClass()->GetInterfaceInfo());
}




















