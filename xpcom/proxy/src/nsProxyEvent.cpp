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
#include "prmem.h"          // for  PR_NEW
#include "xptcall.h"

#include "nsRepository.h"

        
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

static NS_DEFINE_IID(kIEventQIID, NS_IEVENTQUEUE_IID);

nsProxyObject::nsProxyObject()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
     
    mRealObjectOwned = PR_FALSE;
    mRealObject      = nsnull;
    mDestQueue       = nsnull;
    mProxyType       = PROXY_SYNC;
}


nsProxyObject::nsProxyObject(PLEventQueue *destQueue, ProxyType proxyType, nsISupports *realObject)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mRealObjectOwned = PR_FALSE;
    mRealObject      = realObject;
    mProxyType       = proxyType;

    mRealObject->AddRef();

    nsresult rv = nsComponentManager::CreateInstance(NS_EVENTQUEUE_PROGID,
                                                     nsnull,
                                                     kIEventQIID,
                                                     (void **)&mDestQueue);

    if (NS_SUCCEEDED(rv))
    {
        mDestQueue->InitFromPLQueue(destQueue);
    }

}


nsProxyObject::nsProxyObject(PLEventQueue *destQueue, ProxyType proxyType, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mRealObjectOwned = PR_TRUE;
    mProxyType       = proxyType;
    
    nsresult rv = nsComponentManager::CreateInstance(aClass, 
                                                     aDelegate,
                                                     aIID,
                                                     (void**) &mRealObject);

    if (NS_FAILED(rv)) 
    {
        mRealObjectOwned = PR_FALSE;
        mRealObject      = nsnull;
    }

    rv = nsComponentManager::CreateInstance(NS_EVENTQUEUE_PROGID,
                                            nsnull,
                                            kIEventQIID,
                                            (void **)&mDestQueue);

    if (NS_SUCCEEDED(rv))
    {
        mDestQueue->InitFromPLQueue(destQueue);
    }

}

nsProxyObject::~nsProxyObject()
{
    if (mDestQueue)
        mDestQueue->EnterMonitor();
        // Shit!   mDestQueue->RevokeEvents(this);
    	
    if(mRealObject != nsnull)
    {
        if (!mRealObjectOwned)
            mRealObject->Release();
        else
            NS_RELEASE(mRealObject);
    }

    if (mDestQueue)
        mDestQueue->ExitMonitor();

    if (mDestQueue != nsnull)
        NS_RELEASE(mDestQueue);
}

nsresult
nsProxyObject::Post(  PRUint32        methodIndex,           /* which method to be called? */
                      PRUint32        paramCount,            /* number of params */
                      nsXPTCVariant   *params)
{
    
    if (mDestQueue == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mRealObject == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    mDestQueue->EnterMonitor();
    
    NS_ASSERTION(mRealObject, "no native object");
    
    NS_ADDREF_THIS();  // so that our destructor does not pull out from under us.  This will be released in the DestroyHandler()
    
    PLEvent *event = PR_NEW(PLEvent);
    
    if (event == nsnull) 
    {
        mDestQueue->ExitMonitor();
        return NS_ERROR_OUT_OF_MEMORY;   
    }

    nsProxyObjectCallInfo *info = new nsProxyObjectCallInfo(this, methodIndex, params, paramCount, event);
    

    PL_InitEvent(event, 
                 info,
                 EventHandler,
                 DestroyHandler);
   
    if (mProxyType == PROXY_SYNC)
    {
        mDestQueue->PostSynchronousEvent(event, nsnull);
        
        nsresult rv = info->GetResult();
        delete info;

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


void DestroyHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo* owner = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    nsProxyObject* proxyObject = owner->GetProxyObject();

    if (proxyObject->GetProxyType() == PROXY_ASYNC)
    {        
        delete owner;
    }

    // decrement once since we increased it during the Post()
    proxyObject->Release();
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












