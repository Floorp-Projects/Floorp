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

#include "pratom.h"
#include "prmem.h"          // for  PR_NEW
#include "xptcall.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "nsIEventQueueService.h"
#include "nsIThread.h"

#include "nsIAtom.h"  //hack!  Need a way to define a component as threadsafe (ie. sta).

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
        
static void* EventHandler(PLEvent *self);
static void DestroyHandler(PLEvent *self);

nsProxyObjectCallInfo::nsProxyObjectCallInfo(nsProxyObject* owner,
                                              PRUint32 methodIndex, 
                                              nsXPTCVariant* parameterList, 
                                              PRUint32 parameterCount, 
                                              PLEvent *event)
{
    mCompleted        = 0;
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


#ifdef debug_DOUGT
static PRUint32 totalProxyObjects = 0;
static PRUint32 outstandingProxyObjects = 0;
#endif

NS_IMPL_ISUPPORTS0(nsProxyObject)

nsProxyObject::nsProxyObject()
{
  // the mac compiler demands that I have this useless constructor.
}
nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, PRInt32 proxyType, nsISupports *realObject)
{
#ifdef debug_DOUGT
totalProxyObjects++;
outstandingProxyObjects++;
#endif

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mRealObject      = realObject;
    mProxyType       = proxyType;
    mDestQueue       = destQueue;
}


nsProxyObject::nsProxyObject(nsIEventQueue *destQueue, PRInt32  proxyType, const nsCID &aClass,  nsISupports *aDelegate,  const nsIID &aIID)
{
#ifdef debug_DOUGT
totalProxyObjects++;
outstandingProxyObjects++;
#endif

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    nsComponentManager::CreateInstance(aClass, 
                                       aDelegate,
                                       aIID,
                                       (void**) &mRealObject);

    mProxyType       = proxyType;
    mDestQueue       = destQueue;
}

nsProxyObject::~nsProxyObject()
{   
#ifdef debug_DOUGT
outstandingProxyObjects--;
printf("[proxyobjects] %d total used in system, %d outstading\n", totalProxyObjects, outstandingProxyObjects);
#endif
}


nsIEventQueue*      
nsProxyObject::GetQueue() 
{ 
    if (mDestQueue)
        NS_ADDREF(mDestQueue);
    return mDestQueue; 
}
        
nsresult
nsProxyObject::NestedEventLoop(nsProxyObjectCallInfo *proxyInfo)
{
    if (proxyInfo == nsnull) return NS_ERROR_NULL_POINTER;

    PLEvent*  event = proxyInfo->GetPLEvent();
    PRBool eventLoopCreated = PR_FALSE;
    nsresult rv; 

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsIEventQueue *eventQ;
    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
    if (NS_FAILED(rv))
    {
        rv = eventQService->CreateThreadEventQueue();
        eventLoopCreated = PR_TRUE;
        if (NS_FAILED(rv))
            return rv;
        
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
    }
    else
    {
        NS_RELEASE(eventQ);
        rv = eventQService->PushThreadEventQueue(&eventQ);
    }

    if (NS_FAILED(rv))
        return rv;

    while (! proxyInfo->GetCompleted())
    {
        rv = eventQ->GetEvent(&event);
        if (NS_FAILED(rv)) break;
        eventQ->HandleEvent(event);

        PR_Sleep( PR_MillisecondsToInterval(5) );
    }  

   

    if (eventLoopCreated)
    {
         NS_RELEASE(eventQ);
         eventQService->DestroyThreadEventQueue();
    }
    else
    {
        eventQService->PopThreadEventQueue(eventQ);
    }

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

    if (mDestQueue == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mRealObject == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (methodInfo->IsNotXPCOM())
        return NS_ERROR_PROXY_INVALID_IN_PARAMETER;
    
    PLEvent *event = PR_NEW(PLEvent);
    
    if (event == nsnull) 
        return NS_ERROR_OUT_OF_MEMORY;   
        
    ///////////////////////////////////////////////////////////////////////
    // Auto-proxification
    ///////////////////////////////////////////////////////////////////////
#ifdef AUTOPROXIFICATION
    rv = AutoProxyParameterList(methodIndex, methodInfo, params, interfaceInfo, convertInParameters);
#endif
    ///////////////////////////////////////////////////////////////////////
    
    if (NS_FAILED(rv))
        return rv;

    nsXPTCVariant *fullParam;
    uint8 paramCount; 
    rv = convertMiniVariantToVariant(methodInfo, params, &fullParam, &paramCount);
    
    if (NS_FAILED(rv))
        return rv;

    nsProxyObjectCallInfo *proxyInfo = new nsProxyObjectCallInfo(this, methodIndex, fullParam, paramCount, event);
    
    if (proxyInfo == nsnull)
    {
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
        }
        else
        {
            mDestQueue->PostEvent(event);
            rv = NestedEventLoop(proxyInfo);
            //mDestQueue->PostSynchronousEvent(event, nsnull);
            if (NS_FAILED(rv))
                return rv;
        }
        
        rv = proxyInfo->GetResult();
        delete proxyInfo;

        ///////////////////////////////////////////////////////////////////////
        // Auto-proxification
        ///////////////////////////////////////////////////////////////////////
#ifdef AUTOPROXIFICATION
        rv = AutoProxyParameterList(methodIndex, methodInfo, params, interfaceInfo, convertOutParameters);
#endif
        ///////////////////////////////////////////////////////////////////////
        
        return rv;
    }
    
    if (mProxyType & PROXY_ASYNC)
    {
        mDestQueue->PostEvent(event);
        return NS_OK;
    }
    
    return NS_ERROR_UNEXPECTED;
    
}
#ifdef AUTOPROXIFICATION
// ssc@netscape.com wishes he could get rid of this instance of |NS_DEFINE_IID|, but |ProxyEventClassIdentity| is not visible from here
static NS_DEFINE_IID(kProxyObject_Identity_Class_IID, NS_PROXYEVENT_IDENTITY_CLASS_IID);

nsresult
nsProxyObject::AutoProxyParameterList(PRUint32 methodIndex, nsXPTMethodInfo *methodInfo, nsXPTCMiniVariant * params, 
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
                            NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
                            if ( NS_FAILED( rv ) )  
                                return rv;

                            rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
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
                // ssc@netscape.com wishes he could get rid of this instance of |NS_DEFINE_IID|, but |ProxyEventClassIdentity| is not visible from here
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


void DestroyHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo* owner = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    nsProxyObject* proxyObject = owner->GetProxyObject();

    if (proxyObject->GetProxyType() & PROXY_ASYNC)
    {        
        delete owner;
    }
    else
    {
        owner->SetCompleted();
    }
}

void* EventHandler(PLEvent *self) 
{
    nsProxyObjectCallInfo *info = (nsProxyObjectCallInfo*)PL_GetEventOwner(self);
    
    if (info != nsnull)
    {
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
    }
    return NULL;
}

