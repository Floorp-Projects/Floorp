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


#include "nsProxyEvent.h"
#include "nsProxyEventPrivate.h"
#include "nsProxyObjectManager.h"

#include "pratom.h"
#include "prmem.h"
#include "xptcall.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "nsIEventQueueService.h"
#include "nsIThread.h"

#include "nsIAtom.h"  //hack!  Need a way to define a component as threadsafe (ie. sta).

//#define NESTED_Q

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
        
static void* EventHandler(PLEvent *self);
static void DestroyHandler(PLEvent *self);
static void* CompletedEventHandler(PLEvent *self);
static void CompletedDestroyHandler(PLEvent *self) ;

nsProxyObjectCallInfo::nsProxyObjectCallInfo( nsProxyObject* owner,
                                              nsXPTMethodInfo *methodInfo,
                                              PRUint32 methodIndex, 
                                              nsXPTCVariant* parameterList, 
                                              PRUint32 parameterCount, 
                                              PLEvent *event)
{
    NS_ASSERTION(owner, "No nsProxyObject!");
    NS_ASSERTION(methodInfo, "No nsXPTMethodInfo!");
    NS_ASSERTION(parameterList, "No parameterList!");
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

    mOwner = 0;
    
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
NS_IMPL_THREADSAFE_RELEASE(nsProxyObject)
NS_IMPL_QUERY_INTERFACE0(nsProxyObject)

nsProxyObject::nsProxyObject()
{
  // the mac compiler demands that I have this useless constructor.
}
nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, PRInt32 proxyType, nsISupports *realObject)
{
    NS_INIT_REFCNT();
    
    nsServiceManager::GetService(kEventQueueServiceCID, NS_GET_IID(nsIEventQueueService), getter_AddRefs(mEventQService));

    mRealObject      = realObject;
    mDestQueue       = do_QueryInterface(destQueue);
    mProxyType       = proxyType;
}


nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, PRInt32  proxyType, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID)
{
    NS_INIT_REFCNT();

    nsServiceManager::GetService(kEventQueueServiceCID, NS_GET_IID(nsIEventQueueService), getter_AddRefs(mEventQService));

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

// GetRealObject
//  This function must return the real pointer to the object to be proxied.
//  It must not be a comptr or be addreffed.
nsISupports*        
nsProxyObject::GetRealObject()
{ 
    return mRealObject.get(); 
} 

nsIEventQueue*      
nsProxyObject::GetQueue() 
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
        rv = mEventQService->CreateThreadEventQueue();
        eventLoopCreated = PR_TRUE;
        if (NS_FAILED(rv))
            return rv;
        
        rv = mEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    }
#ifdef NESTED_Q
    else
    {
        rv = mEventQService->PushThreadEventQueue(getter_AddRefs(eventQ));
    }
#endif

    if (NS_FAILED(rv))
        return rv;
//---------------------
    
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
#ifdef NESTED_Q
    else
    {
        nsIEventQueue *dumbAddref = eventQ;
        NS_ADDREF(dumbAddref);  // PopThreadEventQueue released the nsCOMPtr, 
                            // then we crash while leaving this functions.
        mEventQService->PopThreadEventQueue(dumbAddref);  // this is totally evil
    }
#endif

    
    return rv;
}
        
        
nsresult
nsProxyObject::convertMiniVariantToVariant(nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, nsXPTCVariant **fullParam, uint8 *paramCount)
{
    uint8 pCount = methodInfo->GetParamCount();

    *fullParam = (nsXPTCVariant*)malloc(sizeof(nsXPTCVariant) * pCount);
    
    if (*fullParam == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    for (int i = 0; i < pCount; i++)
        (*fullParam)[i].Init(params[i], methodInfo->GetParam(i).GetType());
    
    *paramCount = pCount;

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
    
    PLEvent *event = PR_NEW(PLEvent);
    
    if (event == nsnull) 
        return NS_ERROR_OUT_OF_MEMORY;   
        
#ifdef AUTOPROXIFICATION  
    // this should move into the nsProxyObjectCallInfo.
    rv = AutoProxyParameterList(methodIndex, methodInfo, params, interfaceInfo, convertInParameters);
    
    if (NS_FAILED(rv))
    {
        delete event;
        return rv;
    }
#endif
    
    nsXPTCVariant *fullParam;
    uint8 paramCount; 
    rv = convertMiniVariantToVariant(methodInfo, params, &fullParam, &paramCount);
    
    if (NS_FAILED(rv))
    {
        delete event;
        return rv;
    }

    nsProxyObjectCallInfo *proxyInfo = new nsProxyObjectCallInfo(this, 
                                                                 methodInfo, 
                                                                 methodIndex, 
                                                                 fullParam,   // will be deleted by ~()
                                                                 paramCount, 
                                                                 event);      // will be deleted by ~()
    
    if (proxyInfo == nsnull)
    {
        delete event;
        free(fullParam);  // allocated with malloc
        return NS_ERROR_OUT_OF_MEMORY;  
    }

    PL_InitEvent(event, 
                 proxyInfo,
                 EventHandler,
                 DestroyHandler);
   
    if (mProxyType & PROXY_SYNC)
    {
        PRBool callDirectly;
        mDestQueue->IsQueueOnCurrentThread(&callDirectly);

        if (callDirectly)
        {
            EventHandler(event); 
            // there is no need to call the DestroyHandler() because
            // there is no need to wake up the nested event loop.
        }
        else
        {
            rv = PostAndWait(proxyInfo);
            
            if (NS_FAILED(rv))
                return rv;
        }
        
        rv = proxyInfo->GetResult();
        delete proxyInfo;

#ifdef AUTOPROXIFICATION
        rv = AutoProxyParameterList(methodIndex, methodInfo, params, interfaceInfo, convertOutParameters);
#endif
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

#ifdef AUTOPROXIFICATION
/* ssc@netscape.com wishes he could get rid of this instance of
 * |NS_DEFINE_IID|, but |ProxyEventClassIdentity| is not visible from
 * here.
 */
static NS_DEFINE_IID(kProxyObject_Identity_Class_IID, NS_PROXYEVENT_IDENTITY_CLASS_IID);

nsresult
AutoProxyParameterList(PRUint32 methodIndex, nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, 
                                      nsIInterfaceInfo *interfaceInfo, AutoProxyConvertTypes convertType)
{
    nsresult rv = NS_OK;

    uint8 paramCount = methodInfo->GetParamCount();

    for (PRUint32 i = 0; i < paramCount; i++)
    {
        nsXPTParamInfo paramInfo = methodInfo->GetParam(i);

        if (! paramInfo.GetType().IsInterfacePointer() )
            continue;

        if ( (convertType == convertOutParameters && paramInfo.IsOut())  || 
             (convertType == convertInParameters && paramInfo.IsIn())    )
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
            nsISupports *aIdentificationObject;

            rv = anInterface->QueryInterface(kProxyObject_Identity_Class_IID, (void**)&aIdentificationObject);
        
            if (NS_FAILED(rv))
            {
                // create a proxy
                nsIProxyObjectManager*  manager;

                rv = nsServiceManager::GetService( NS_XPCOMPROXY_PROGID, 
                                                   NS_GET_IID(nsIProxyObjectManager),
                                                   (nsISupports **)&manager);
        
                if (NS_SUCCEEDED(rv))
                {   
                    const nsXPTType& type = paramInfo.GetType();
                    nsIID* iid = nsnull;
                    if(type.TagPart() == nsXPTType::T_INTERFACE_IS)
                    {
                        uint8 arg_num;
                        
                        rv = interfaceInfo->GetInterfaceIsArgNumberForParam(methodIndex, &paramInfo, &arg_num);
                        if(NS_FAILED(rv))
                        {
                            // This is really bad that we are here.  
                            rv = NS_ERROR_PROXY_INVALID_IN_PARAMETER;
                            continue;
                        }

                        const nsXPTParamInfo& param = methodInfo->GetParam(arg_num);
                        const nsXPTType& currentType = param.GetType();

                        if( !currentType.IsPointer() || 
                            currentType.TagPart() != nsXPTType::T_IID ||
                            !(iid = (nsIID*) nsAllocator::Clone(params[arg_num].val.p, sizeof(nsIID))))
                        {
                            // This is really bad that we are here.  
                            rv = NS_ERROR_PROXY_INVALID_IN_PARAMETER;
                        }
                    }
                    else
                    {
                        interfaceInfo->GetIIDForParam((PRUint16)methodIndex, &paramInfo, &iid);
                    }
                    
                    if (iid == nsnull)
                    {
                        // could not get a IID for the parameter that is in question!

#ifdef DEBUG
                        char* interfaceName;

                        interfaceInfo->GetName(&interfaceName);
                        
                        printf("**************************************************\n");
                        printf("xpcom-proxy: could not invoke method: %s::%s().\n", interfaceName, methodInfo->GetName());
                        printf("             could not find an IID for a parameter: %d\n", (i) );
                        printf("**************************************************\n");

                        nsAllocator::Free((void*)interfaceName);
#endif /* DEBUG */


                        rv = NS_ERROR_PROXY_INVALID_IN_PARAMETER; //TODO: Change error code
                    }
                    else
                    {

                        nsIEventQueue *eventQ = nsnull;
                        /* 
                           if the parameter is coming |in|, it should only be called on the callers thread.
                           else, if the parameter is an |out| thread, it should only be called on the proxy
                           objects thread
                        */

                        if (paramInfo.IsOut())
                        {
                            eventQ = GetQueue();
                        }
                        else
                        {
                            if (mEventQService == nsnull) return NS_ERROR_NULL_POINTER;

                            rv = mEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQ);
                            if ( NS_FAILED( rv ) )
                            {
                                // the caller does not have an eventQ of their own.  bad.
                                eventQ = GetQueue();
                                rv = NS_OK; // todo: remove!
#ifdef DEBUG
                                printf("**************************************************\n");
                                printf("xpcom-proxy: Caller does not have an EventQ\n");
                                printf("**************************************************\n");
#endif /* DEBUG */
                            }   
                        }

                        /* This is a hack until we can do some lookup 
                           in the register to find the threading model
                           of an object.  right now, nsIAtom should be
                           treated as a free threaded object since layout
                           does evil things like compare pointer for direct
                           equality.  Again, this will go away when we can 
                           use co-classes to find the ClassID.  Then look in
                           the register for the thread model
                        */

                        if (NS_SUCCEEDED( rv ) && iid && iid->Equals(NS_GET_IID(nsIAtom)))
                        {
                            nsAllocator::Free((void*)iid);
                            NS_RELEASE(manager);
                            NS_RELEASE(eventQ);
                            continue;
                        }
                

                        if ( NS_SUCCEEDED( rv ) )
                        {
                                rv = manager->GetProxyObject(eventQ, 
                                                             *iid,
                                                             anInterface, 
                                                             GetProxyType(), 
                                                             (void**) &aProxyObject);
                        }
                        
                        NS_RELEASE(eventQ);
                    }

                    nsAllocator::Free((void*)iid);
                    NS_RELEASE(manager);

                    if (NS_FAILED(rv))
                        return rv;

                    // stuff the new proxies into the val.p.  If it is an OUT parameter, release once
                    // the real object to force the proxy to be the owner.
                    if (paramInfo.IsOut())
                    {
                        anInterface->Release();    
                        *((void**)params[i].val.p) = ((void*)aProxyObject);
                    }
                    else
                    {
                        (params[i].val.p)  = ((void*)aProxyObject);                    
                    }
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
        else if ( (convertType == convertOutParameters) && paramInfo.IsIn() )
        {
            // we need to Release() any |in| parameters that we created proxies for
            // and replace the proxied object with the real object on the stack.
            nsProxyEventObject* replaceInterface = ((nsProxyEventObject*)params[i].val.p);
            if (replaceInterface)
            {
                /* ssc@netscape.com wishes he could get rid of this
                 * instance of |NS_DEFINE_IID|, but
                 * |ProxyEventClassIdentity| is not visible from here
                 */
                nsISupports *aIdentificationObject;
                rv = replaceInterface->QueryInterface(kProxyObject_Identity_Class_IID, (void**)&aIdentificationObject);
        
                if (NS_SUCCEEDED(rv))
                {
                    nsISupports* realObject = replaceInterface->GetRealObject();
                    replaceInterface->Release();   
                    (params[i].val.p)  = ((void*)realObject);  
                }
            }
        }
    }
    return rv;
}
#endif
