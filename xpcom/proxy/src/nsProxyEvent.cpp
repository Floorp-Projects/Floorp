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
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      Added PR_CALLBACK for Optlink use in OS2
 */

#include "nsProxyEvent.h"
#include "nsProxyEventPrivate.h"
#include "nsIProxyObjectManager.h"

#include "pratom.h"
#include "prmem.h"
#include "xptcall.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsIEventQueueService.h"
#include "nsIThread.h"

#include "nsIAtom.h"  //hack!  Need a way to define a component as threadsafe (ie. sta).

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
        
static void* PR_CALLBACK EventHandler(PLEvent *self);
static void  PR_CALLBACK DestroyHandler(PLEvent *self);
static void* PR_CALLBACK CompletedEventHandler(PLEvent *self);
static void PR_CALLBACK CompletedDestroyHandler(PLEvent *self) ;
static void* PR_CALLBACK ProxyDestructorEventHandler(PLEvent *self);
static void PR_CALLBACK ProxyDestructorDestroyHandler(PLEvent *self) ;

nsProxyObjectCallInfo::nsProxyObjectCallInfo( nsProxyObject* owner,
                                              nsXPTMethodInfo *methodInfo,
                                              PRUint32 methodIndex, 
                                              nsXPTCVariant* parameterList, 
                                              PRUint32 parameterCount, 
                                              PLEvent *event)
{
    NS_ASSERTION(owner, "No nsProxyObject!");
    NS_ASSERTION(methodInfo, "No nsXPTMethodInfo!");
    NS_ASSERTION(event, "No PLEvent!");
    
    mCompleted        = 0;
    mMethodIndex      = methodIndex;
    mParameterList    = parameterList;
    mParameterCount   = parameterCount;
    mEvent            = event;
    mMethodInfo       = methodInfo;
    mCallersEventQ    = nsnull;

    mOwner            = owner;
    
    RefCountInInterfacePointers(PR_TRUE);
    if (mOwner->GetProxyType() & PROXY_ASYNC)
        CopyStrings(PR_TRUE);
}


nsProxyObjectCallInfo::~nsProxyObjectCallInfo()
{
    RefCountInInterfacePointers(PR_FALSE);
    if (mOwner->GetProxyType() & PROXY_ASYNC)
        CopyStrings(PR_FALSE);

    mOwner = nsnull;
    
    PR_FREEIF(mEvent);
    
    if (mParameterList)  
        free( (void*) mParameterList);
}

void
nsProxyObjectCallInfo::RefCountInInterfacePointers(PRBool addRef)
{
    for (PRUint32 i = 0; i < mParameterCount; i++)
    {
        nsXPTParamInfo paramInfo = mMethodInfo->GetParam(i);

        if (paramInfo.GetType().IsInterfacePointer() )
        {
            nsISupports* anInterface = nsnull;

            if (paramInfo.IsIn())
            {
                anInterface = ((nsISupports*)mParameterList[i].val.p);
                
                if (anInterface)
                {
                    if(addRef)
                        anInterface->AddRef();
                    else
                        anInterface->Release();
            
                }
            }
        }
    }
}

void
nsProxyObjectCallInfo::CopyStrings(PRBool copy)
{
    for (PRUint32 i = 0; i < mParameterCount; i++)
    {
        const nsXPTParamInfo paramInfo = mMethodInfo->GetParam(i);

        if(paramInfo.IsIn())
        {
            const nsXPTType& type = paramInfo.GetType();
            uint8 type_tag = type.TagPart();

            if (type_tag == nsXPTType::T_CHAR_STR || type_tag == nsXPTType::T_WCHAR_STR)
            {
                if (mParameterList[i].val.p != nsnull)
                {
                    if (copy)
                    {
                        void *ptr = mParameterList[i].val.p;

                        if (type_tag == nsXPTType::T_CHAR_STR)
                        {
                            mParameterList[i].val.p =
                                nsCRT::strdup((const char *)ptr);
                        }
                        else if (type_tag == nsXPTType::T_WCHAR_STR)
                        {
                            mParameterList[i].val.p =
                                nsCRT::strdup((const PRUnichar *)ptr);
                        }
                    }
                    else
                    {
                        Recycle((char*)mParameterList[i].val.p);
                    }
                }
            }
        }
    }
}

PRBool                
nsProxyObjectCallInfo::GetCompleted()
{
    return (PRBool)mCompleted;
}

void
nsProxyObjectCallInfo::SetCompleted()
{
    PR_AtomicSet(&mCompleted, 1);
}

void                
nsProxyObjectCallInfo::PostCompleted()
{
    if (mCallersEventQ)
    {
        PLEvent *event = PR_NEW(PLEvent);
    
        PL_InitEvent(event, 
                     this,
                     CompletedEventHandler,
                     CompletedDestroyHandler);
   
        mCallersEventQ->PostSynchronousEvent(event, nsnull);
        PR_FREEIF(event);
    }
    else
    {
        // caller does not have an eventQ? This is an error!
        SetCompleted();
    }
}
  
nsIEventQueue*      
nsProxyObjectCallInfo::GetCallersQueue() 
{ 
    return mCallersEventQ;
}   
void
nsProxyObjectCallInfo::SetCallersQueue(nsIEventQueue* queue)
{
    mCallersEventQ = queue;
}   


NS_IMPL_THREADSAFE_ADDREF(nsProxyObject)
NS_IMPL_THREADSAFE_QUERY_INTERFACE0(nsProxyObject)

nsProxyObject::nsProxyObject()
{
  // the mac compiler demands that I have this useless constructor.
}
nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, PRInt32 proxyType, nsISupports *realObject)
{
    NS_INIT_REFCNT();
    
    mEventQService = do_GetService(kEventQueueServiceCID);

    mRealObject      = realObject;
    mDestQueue       = do_QueryInterface(destQueue);
    mProxyType       = proxyType;
}


nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, PRInt32  proxyType, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID)
{
    NS_INIT_REFCNT();

    mEventQService = do_GetService(kEventQueueServiceCID);

    nsComponentManager::CreateInstance(aClass, 
                                       aDelegate,
                                       aIID,
                                       getter_AddRefs(mRealObject));

    mDestQueue       = do_QueryInterface(destQueue);
    mProxyType       = proxyType;
}

nsProxyObject::~nsProxyObject()
{   
    // I am worried about order of destruction here.  
    // do not remove assignments.
    
    mRealObject = 0;
    mDestQueue  = 0;
}



NS_IMETHODIMP_(nsrefcnt)
nsProxyObject::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");             

  nsrefcnt count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
  NS_LOG_RELEASE(this, count, "nsProxyObject");

  if (count == 0)
  {
       mRefCnt = 1; /* stabilize */

        PRBool callDirectly;
        mDestQueue->IsQueueOnCurrentThread(&callDirectly);

        if (callDirectly)
        {
            NS_DELETEXPCOM(this); 
            return 0;
        }

      // need to do something special here so that
      // the real object will always be deleted on
      // the correct thread..

        PLEvent *event = PR_NEW(PLEvent);
        if (event == nsnull)
        {
            NS_ASSERTION(0, "Could not create a plevent. Leaking nsProxyObject!");
             return 0;  // if this happens we are going to leak.
        }
       
        PL_InitEvent(event, 
                     this,
                     ProxyDestructorEventHandler,
                     ProxyDestructorDestroyHandler);  

        mDestQueue->PostEvent(event);
        return 0;
  }                          
  return count;
}                


// GetRealObject
//  This function must return the real pointer to the object to be proxied.
//  It must not be a comptr or be addreffed.
nsISupports*        
nsProxyObject::GetRealObject() const
{ 
    return mRealObject.get(); 
} 

nsIEventQueue*      
nsProxyObject::GetQueue() const
{ 
    return mDestQueue; 
}

nsresult
nsProxyObject::PostAndWait(nsProxyObjectCallInfo *proxyInfo)
{
    if (proxyInfo == nsnull || mEventQService == nsnull) 
        return NS_ERROR_NULL_POINTER;
    
    PRBool eventLoopCreated = PR_FALSE;
    nsresult rv; 

    nsCOMPtr<nsIEventQueue> eventQ;
    rv = mEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    if (NS_FAILED(rv))
    {
        rv = mEventQService->CreateMonitoredThreadEventQueue();
        eventLoopCreated = PR_TRUE;
        if (NS_FAILED(rv))
            return rv;
        
        rv = mEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    }

    if (NS_FAILED(rv))
        return rv;
    
    proxyInfo->SetCallersQueue(eventQ);

    PLEvent* event = proxyInfo->GetPLEvent();
    if (!event)
        return NS_ERROR_NULL_POINTER;
    
    mDestQueue->PostEvent(event);

    while (! proxyInfo->GetCompleted())
    {
        PLEvent *nextEvent;
        rv = eventQ->WaitForEvent(&nextEvent);
        if (NS_FAILED(rv)) break;
                
        eventQ->HandleEvent(nextEvent);
    }  

    if (eventLoopCreated)
    {
         mEventQService->DestroyThreadEventQueue();
         eventQ = 0;
    }
    
    return rv;
}
        
        
nsresult
nsProxyObject::convertMiniVariantToVariant(nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, nsXPTCVariant **fullParam, uint8 *outParamCount)
{
    uint8 paramCount = methodInfo->GetParamCount();
    *outParamCount = paramCount;
    *fullParam = nsnull;

    if (!paramCount) return NS_OK;
        
    *fullParam = (nsXPTCVariant*)malloc(sizeof(nsXPTCVariant) * paramCount);
    
    if (*fullParam == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    for (int i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& paramInfo = methodInfo->GetParam(i);
        uint8 flags = paramInfo.IsOut() ? nsXPTCVariant::PTR_IS_DATA : 0;
        (*fullParam)[i].Init(params[i], paramInfo.GetType(), flags);
    }
    
    return NS_OK;
}
        
nsresult
nsProxyObject::Post( PRUint32 methodIndex, nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, nsIInterfaceInfo *interfaceInfo)            
{
    nsresult rv = NS_OK; 

    if (! mDestQueue  || ! mRealObject)
        return NS_ERROR_OUT_OF_MEMORY;

    if (methodInfo->IsNotXPCOM())
        return NS_ERROR_PROXY_INVALID_IN_PARAMETER;
    
    nsXPTCVariant *fullParam;
    uint8 paramCount; 
    rv = convertMiniVariantToVariant(methodInfo, params, &fullParam, &paramCount);
    
    if (NS_FAILED(rv))
        return rv;

    PRBool callDirectly;

    // see if we should call into the method directly. Either it is a QI function call
    // (methodIndex == 0), or it is a sync proxy and this code is running on the same thread
    // as the destination event queue. 
    if ( (methodIndex == 0) ||
         (mProxyType & PROXY_SYNC && 
          NS_SUCCEEDED(mDestQueue->IsQueueOnCurrentThread(&callDirectly)) &&
          callDirectly))
    {

        // invoke the magic of xptc...
        nsresult rv = XPTC_InvokeByIndex( mRealObject, 
                                          methodIndex,
                                          paramCount, 
                                          fullParam);
        
        if (fullParam) 
            free(fullParam);
        return rv;
    }

    PLEvent *event = PR_NEW(PLEvent);
    
    if (event == nsnull) {
        if (fullParam) 
            free(fullParam);
        return NS_ERROR_OUT_OF_MEMORY;   
    }

    nsProxyObjectCallInfo *proxyInfo = new nsProxyObjectCallInfo(this, 
                                                                 methodInfo, 
                                                                 methodIndex, 
                                                                 fullParam,   // will be deleted by ~()
                                                                 paramCount, 
                                                                 event);      // will be deleted by ~()
    
    if (proxyInfo == nsnull) {
        PR_DELETE(event);
        if (fullParam)
            free(fullParam);
        return NS_ERROR_OUT_OF_MEMORY;  
    }

    PL_InitEvent(event, 
                 proxyInfo,
                 EventHandler,
                 DestroyHandler);
   
    if (mProxyType & PROXY_SYNC)
    {
        rv = PostAndWait(proxyInfo);
        
        if (NS_SUCCEEDED(rv))
            rv = proxyInfo->GetResult();
        delete proxyInfo;
        return rv;
    }
    
    if (mProxyType & PROXY_ASYNC)
    {
        mDestQueue->PostEvent(event);
        return NS_OK;
    }
    return NS_ERROR_UNEXPECTED;
}



static void DestroyHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo* owner = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    nsProxyObject* proxyObject = owner->GetProxyObject();
    
    if (proxyObject == nsnull)
        return;
 
    if (proxyObject->GetProxyType() & PROXY_ASYNC)
    {        
        delete owner;
    }
    else
    {
        owner->PostCompleted();
    }

}

static void* EventHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo *info = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    NS_ASSERTION(info, "No nsProxyObjectCallInfo!");
    
    nsProxyObject *proxyObject = info->GetProxyObject();
        
    if (proxyObject)
    {
        // invoke the magic of xptc...
        nsresult rv = XPTC_InvokeByIndex( proxyObject->GetRealObject(), 
                                          info->GetMethodIndex(),
                                          info->GetParameterCount(), 
                                          info->GetParameterList());
        info->SetResult(rv);
    }
    else
    {
        info->SetResult(NS_ERROR_OUT_OF_MEMORY);
    }
    return NULL;
}

static void CompletedDestroyHandler(PLEvent *self) 
{
}

static void* CompletedEventHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo* owner = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    owner->SetCompleted();
    return nsnull;
}

static void* ProxyDestructorEventHandler(PLEvent *self)
{              
    nsProxyObject* owner = (nsProxyObject*) PL_GetEventOwner(self);               
    NS_DELETEXPCOM(owner);                                                                                              
    return nsnull;                                            
}

static void ProxyDestructorDestroyHandler(PLEvent *self)
{
    PR_DELETE(self);
}

