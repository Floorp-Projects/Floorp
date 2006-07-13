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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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
#include "nsProxyEventPrivate.h"
#include "nsIThreadInternal.h"

#include "nsServiceManagerUtils.h"

#include "nsHashtable.h"

#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"

#include "nsAutoLock.h"

nsProxyEventObject::nsProxyEventObject(nsProxyObject *aParent,
                                       nsProxyEventClass* aClass,
                                       nsISomeInterface* aRealInterface)
    : mRealInterface(aRealInterface),
      mClass(aClass),
      mProxyObject(aParent),
      mNext(nsnull)
{
}

nsProxyEventObject::~nsProxyEventObject()
{
    // This destructor is always called within the POM monitor.
    // XXX assert this!

    mProxyObject->LockedRemove(this);
}

//
// nsISupports implementation...
//

NS_IMPL_THREADSAFE_ADDREF(nsProxyEventObject)

NS_IMETHODIMP_(nsrefcnt)
nsProxyEventObject::Release(void)
{
    nsAutoMonitor mon(nsProxyObjectManager::GetInstance()->GetMonitor());

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
    if( aIID.Equals(GetClass()->GetProxiedIID()) )
    {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
        
    return mProxyObject->QueryInterface(aIID, aInstancePtr);
}

//
// nsXPTCStubBase implementation...
//

NS_IMETHODIMP
nsProxyEventObject::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    *info = mClass->GetInterfaceInfo();
    NS_ASSERTION(*info, "proxy class without interface");

    NS_ADDREF(*info);
    return NS_OK;
}

nsresult
nsProxyEventObject::convertMiniVariantToVariant(const nsXPTMethodInfo *methodInfo, 
                                                nsXPTCMiniVariant * params, 
                                                nsXPTCVariant **fullParam, 
                                                uint8 *outParamCount)
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
        if ((GetProxyType() & NS_PROXY_ASYNC) && paramInfo.IsDipper())
        {
            NS_WARNING("Async proxying of out parameters is not supported"); 
            free(*fullParam);
            return NS_ERROR_PROXY_INVALID_OUT_PARAMETER;
        }
        uint8 flags = paramInfo.IsOut() ? nsXPTCVariant::PTR_IS_DATA : 0;
        (*fullParam)[i].Init(params[i], paramInfo.GetType(), flags);
    }
    
    return NS_OK;
}

class nsProxyThreadFilter : public nsIThreadEventFilter
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITHREADEVENTFILTER
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsProxyThreadFilter, nsIThreadEventFilter)

NS_DEFINE_IID(kFilterIID, NS_PROXYEVENT_FILTER_IID);

NS_IMETHODIMP_(PRBool)
nsProxyThreadFilter::AcceptEvent(nsIRunnable *event)
{
    PROXY_LOG(("PROXY(%p): filter event [%p]\n", this, event));

    // If we encounter one of our proxy events that is for a synchronous method
    // call, then we want to put it in our event queue for processing.  Else,
    // we want to allow the event to be dispatched to the thread's event queue
    // for processing later once we complete the current sync method call.
    
    nsRefPtr<nsProxyObjectCallInfo> poci;
    event->QueryInterface(kFilterIID, getter_AddRefs(poci));
    return poci && poci->IsSync();
}

NS_IMETHODIMP
nsProxyEventObject::CallMethod(PRUint16 methodIndex,
                               const nsXPTMethodInfo* methodInfo,
                               nsXPTCMiniVariant * params)
{
    NS_ASSERTION(methodIndex > 2,
                 "Calling QI/AddRef/Release through CallMethod");
    nsresult rv;

    if (methodInfo->IsNotXPCOM())
        return NS_ERROR_PROXY_INVALID_IN_PARAMETER;

    nsXPTCVariant *fullParam;
    uint8 paramCount;
    rv = convertMiniVariantToVariant(methodInfo, params,
                                     &fullParam, &paramCount);
    if (NS_FAILED(rv))
        return rv;

    PRBool callDirectly = PR_FALSE;
    if (GetProxyType() & NS_PROXY_SYNC &&
        NS_SUCCEEDED(GetTarget()->IsOnCurrentThread(&callDirectly)) &&
        callDirectly) {

        // invoke directly using xptc
        rv = XPTC_InvokeByIndex(mRealInterface, methodIndex,
                                paramCount, fullParam);

        if (fullParam)
            free(fullParam);

        return rv;
    }

    nsRefPtr<nsProxyObjectCallInfo> proxyInfo =
        new nsProxyObjectCallInfo(this, methodInfo, methodIndex,
                                  fullParam, paramCount);
    if (!proxyInfo)
        return NS_ERROR_OUT_OF_MEMORY;

    if (! (GetProxyType() & NS_PROXY_SYNC)) {
        return GetTarget()->Dispatch(proxyInfo, NS_DISPATCH_NORMAL);
    }

    // Post synchronously

    nsIThread *thread = NS_GetCurrentThread();
    nsCOMPtr<nsIThreadInternal> threadInt = do_QueryInterface(thread);
    NS_ENSURE_STATE(threadInt);

    // Install  thread filter to limit event processing only to 
    // nsProxyObjectCallInfo instances.  XXX Add support for sequencing?
    nsRefPtr<nsProxyThreadFilter> filter = new nsProxyThreadFilter();
    if (!filter)
        return NS_ERROR_OUT_OF_MEMORY;
    threadInt->PushEventQueue(filter);

    proxyInfo->SetCallersTarget(thread);
    
    // Dispatch can fail if the thread is shutting down
    rv = GetTarget()->Dispatch(proxyInfo, NS_DISPATCH_NORMAL);
    if (NS_SUCCEEDED(rv)) {
        while (!proxyInfo->GetCompleted()) {
            if (!NS_ProcessNextEvent(thread)) {
                rv = NS_ERROR_UNEXPECTED;
                break;
            }
        }
    } else {
        NS_WARNING("Failed to dispatch nsProxyCallEvent");
    }

    threadInt->PopEventQueue();

    PROXY_LOG(("PROXY(%p): PostAndWait exit [%p %x]\n", this, proxyInfo, rv));
    return rv;
}
