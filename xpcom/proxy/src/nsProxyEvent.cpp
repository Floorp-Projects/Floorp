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
#include "nsProxyEventPrivate.h"
#include "nsProxyObjectManager.h"

#include "prmem.h"          // for  PR_NEW
#include "xptcall.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
        
static NS_DEFINE_IID(kProxyObjectManagerIID, NS_IPROXYEVENT_MANAGER_IID);
static NS_DEFINE_IID(kProxyObject_Identity_Class_IID, NS_PROXYEVENT_IDENTITY_CLASS_IID);

static void* EventHandler(PLEvent *self);
static void DestroyHandler(PLEvent *self);

nsProxyObjectCallInfo::nsProxyObjectCallInfo(nsProxyObject* owner,
                                              PRUint32 methodIndex, 
                                              nsXPTCVariant* parameterList, 
                                              PRUint32 parameterCount, 
                                              PLEvent *event)
{
    mOwner            = owner;
    mMethodIndex      = methodIndex;
    mParameterList    = parameterList;
    mParameterCount   = parameterCount;
    mEvent            = event;
}


nsProxyObjectCallInfo::~nsProxyObjectCallInfo()
{
    PR_FREEIF(mEvent);
    
    if (mParameterList)  
        free( (void*) mParameterList);
}

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_IMPL_ISUPPORTS(nsProxyObject, kISupportsIID)

nsProxyObject::nsProxyObject()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
     
    mRealObjectOwned = PR_FALSE;
    mRealObject      = nsnull;
    mDestQueue       = nsnull;
    mProxyType       = PROXY_SYNC;
}


nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, ProxyType proxyType, nsISupports *realObject)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mRealObjectOwned = PR_FALSE;
    mRealObject      = realObject;
    mProxyType       = proxyType;
    mDestQueue       = destQueue;

    NS_ADDREF(mRealObject);
    NS_ADDREF(mDestQueue);

}


nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, ProxyType proxyType, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mRealObjectOwned = PR_TRUE;
    mProxyType       = proxyType;
    mDestQueue       = destQueue;

    NS_ADDREF(mDestQueue);
    
    nsresult rv = nsComponentManager::CreateInstance(aClass, 
                                                     aDelegate,
                                                     aIID,
                                                     (void**) &mRealObject);

    if (NS_FAILED(rv)) 
    {
        mRealObjectOwned = PR_FALSE;
        mRealObject      = nsnull;
    }
}

nsProxyObject::~nsProxyObject()
{
    if (mDestQueue)
        mDestQueue->EnterMonitor();
        // FIX!   mDestQueue->RevokeEvents(this);
    	
    if(mRealObject != nsnull)
    {
        if (!mRealObjectOwned)
            NS_RELEASE(mRealObject);
        else
            NS_RELEASE(mRealObject);
    }

    if (mDestQueue)
        mDestQueue->ExitMonitor();

    if (mDestQueue != nsnull)
        NS_RELEASE(mDestQueue);
}

nsresult
nsProxyObject::Post( PRUint32 methodIndex, nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, nsIInterfaceInfo *interfaceInfo)            
{
    if (mDestQueue == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mRealObject == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

  
    ///////////////////////////////////////////////////////////////////////
    // Auto-proxification
    ///////////////////////////////////////////////////////////////////////
    nsresult rv = AutoProxyParameterList(methodIndex, methodInfo, params, interfaceInfo, convertInParameters);
    ///////////////////////////////////////////////////////////////////////
    
    if (NS_FAILED(rv))
        return rv;

    // convert the nsXPTCMiniVariant to a nsXPTCVariant

    uint8 paramCount = methodInfo->GetParamCount();

    nsXPTCVariant   *fullParam = (nsXPTCVariant*)malloc(sizeof(nsXPTCVariant) * paramCount);
    
    for (int index = 0; index < paramCount; index++)
    {
        fullParam[index].flags = 0;
        fullParam[index].val   = params[index].val;
    }
    
    
    mDestQueue->EnterMonitor();
    
    NS_ASSERTION(mRealObject, "no native object");
    
    NS_ADDREF_THIS();  // so that our destructor does not pull out from under us.  This will be released in the DestroyHandler()
    
    PLEvent *event = PR_NEW(PLEvent);
    
    if (event == nsnull) 
    {
        mDestQueue->ExitMonitor();
            return NS_ERROR_OUT_OF_MEMORY;   
    }

    nsProxyObjectCallInfo *proxyInfo = new nsProxyObjectCallInfo(this, methodIndex, fullParam, paramCount, event);
    
    if (proxyInfo == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;  

    PL_InitEvent(event, 
                 proxyInfo,
                 EventHandler,
                 DestroyHandler);
   
    if (mProxyType == PROXY_SYNC)
    {
        mDestQueue->PostSynchronousEvent(event, nsnull);
        
        rv = proxyInfo->GetResult();
        delete proxyInfo;

        ///////////////////////////////////////////////////////////////////////
        // Auto-proxification
        ///////////////////////////////////////////////////////////////////////
        rv = AutoProxyParameterList(methodIndex, methodInfo, params, interfaceInfo, convertOutParameters);
        ///////////////////////////////////////////////////////////////////////
        
        mDestQueue->ExitMonitor();
        return rv;
    }
    else if (mProxyType == PROXY_ASYNC)
    {
        mDestQueue->PostEvent(event);
        mDestQueue->ExitMonitor();
        return NS_OK;
    }
    
    mDestQueue->ExitMonitor();
    return NS_ERROR_UNEXPECTED;
    
}


nsresult
nsProxyObject::AutoProxyParameterList(PRUint32 methodIndex, nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, 
                                      nsIInterfaceInfo *interfaceInfo, AutoProxyConvertTypes convertType)
{
    nsresult rv = NS_OK;

    uint8 paramCount = methodInfo->GetParamCount();

    for (PRUint32 i = 0; i < paramCount; i++)
    {
        nsXPTParamInfo paramInfo = methodInfo->GetParam(i);

        if (paramInfo.GetType().TagPart() != nsXPTType::T_INTERFACE )
            continue;

        if ( (convertType == convertOutParameters && paramInfo.IsOut())  || 
             (convertType == convertInParameters && paramInfo.IsIn())    || 
             (convertType == convertAllParameters))
        {
            // We found an out parameter which is a interface, check for proxy
            if (params[i].val.p == nsnull)
                continue;
            
            nsISupports* anInterface = nsnull;

            if (paramInfo.IsOut())
                anInterface = *((nsISupports**)params[i].val.p);
            else
                anInterface = ((nsISupports*)params[i].val.p);


            if (anInterface == nsnull)
                continue;

            nsISupports *aProxyObject;
            rv = anInterface->QueryInterface(kProxyObject_Identity_Class_IID, (void**)&aProxyObject);
        
            if (NS_FAILED(rv))
            {
                // create a proxy
                nsIProxyObjectManager*  manager;

                rv = nsServiceManager::GetService( NS_XPCOMPROXY_PROGID, 
                                                   kProxyObjectManagerIID,
                                                   (nsISupports **)&manager);
        
                if (NS_SUCCEEDED(rv))
                {
                    nsIID* iid;

                    interfaceInfo->GetIIDForParam((PRUint16)methodIndex, &paramInfo, &iid);

                    rv = manager->GetProxyObject(GetQueue(), 
                                                 *iid,
                                                 anInterface, 
                                                 GetProxyType(), 
                                                 (void**) &aProxyObject);

                    nsAllocator::Free((void*)iid);
                    NS_RELEASE(manager);

                    if (NS_FAILED(rv))
                        return rv;

                    if (paramInfo.IsOut())
                        *((void**)params[i].val.p) = ((void*)aProxyObject);
                    else
                        (params[i].val.p)  = ((void*)aProxyObject);                    
                }
                else
                {   
                    // Could not get nsIProxyObjectManager
                    return rv;
                }
            } 
            else
            {
                // It already is a proxy!
            }
        }
    }
    return rv;
}

void DestroyHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo* owner = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    nsProxyObject* proxyObject = owner->GetProxyObject();

    if (proxyObject->GetProxyType() == PROXY_ASYNC)
    {        
        delete owner;
    }

    // decrement once since we increased it during the Post()
    NS_RELEASE(proxyObject);
}

void* EventHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo *info        = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    nsProxyObject         *proxyObject = info->GetProxyObject();
    nsIEventQueue         *eventQ      = proxyObject->GetQueue();

    if (info != nsnull)
    {
       if (eventQ == nsnull || proxyObject == nsnull)
       {
            info->SetResult(NS_ERROR_OUT_OF_MEMORY) ;
            return NULL;
       }

       eventQ->EnterMonitor();

       // invoke the magic of xptc...
       nsresult rv = XPTC_InvokeByIndex( proxyObject->GetRealObject(), 
                                         info->GetMethodIndex(),
                                         info->GetParameterCount(), 
                                         info->GetParameterList());
    
       info->SetResult(rv);
       
       eventQ->ExitMonitor();
    }
    return NULL;
}












