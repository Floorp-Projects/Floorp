/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/*
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
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

#include "nsProxyEventPrivate.h"
#include "nsProxyRelease.h"
#include "nsIProxyObjectManager.h"
#include "nsCRT.h"

#include "pratom.h"
#include "prmem.h"
#include "xptcall.h"

#include "nsAutoLock.h"
#include "nsXPCOMCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsThreadUtils.h"
#include "nsEventQueue.h"
#include "nsMemory.h"

/**
 * Map the nsAUTF8String, nsUTF8String classes to the nsACString and
 * nsCString classes respectively for now.  These defines need to be removed
 * once Jag lands his nsUTF8String implementation.
 */
#define nsAUTF8String nsACString
#define nsUTF8String nsCString

class nsProxyCallCompletedEvent : public nsRunnable
{
public:
    nsProxyCallCompletedEvent(nsProxyObjectCallInfo *info)
        : mInfo(info)
    {}

    NS_DECL_NSIRUNNABLE

    NS_IMETHOD QueryInterface(REFNSIID aIID, void **aResult);

private:
    nsProxyObjectCallInfo *mInfo;
};

NS_IMETHODIMP
nsProxyCallCompletedEvent::Run()
{
    NS_ASSERTION(mInfo, "no info");
    mInfo->SetCompleted();
    return NS_OK;
}

NS_DEFINE_IID(kFilterIID, NS_PROXYEVENT_FILTER_IID);

NS_IMETHODIMP
nsProxyCallCompletedEvent::QueryInterface(REFNSIID aIID, void **aResult)
{
    // We are explicitly breaking XPCOM rules here by returning a different
    // object from QueryInterface. We do this so that
    // nsProxyThreadFilter::AcceptEvent can know whether we are an event that
    // needs to be allowed through during a synchronous proxy call.
    if (aIID.Equals(kFilterIID)) {
        *aResult = mInfo;
        mInfo->AddRef();
        return NS_OK;
    }
    return nsRunnable::QueryInterface(aIID, aResult);
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsProxyObject::nsProxyObjectDestructorEvent::Run()
{
    delete mDoomed;
    return NS_OK;
}

//-----------------------------------------------------------------------------

nsProxyObjectCallInfo::nsProxyObjectCallInfo(nsProxyEventObject* owner,
                                             const nsXPTMethodInfo *methodInfo,
                                             PRUint32 methodIndex, 
                                             nsXPTCVariant* parameterList, 
                                             PRUint32 parameterCount) :
    mResult(NS_ERROR_FAILURE),
    mMethodInfo(methodInfo),
    mMethodIndex(methodIndex),
    mParameterList(parameterList),
    mParameterCount(parameterCount),
    mCompleted(0),
    mOwner(owner)
{
    NS_ASSERTION(owner, "No nsProxyObject!");
    NS_ASSERTION(methodInfo, "No nsXPTMethodInfo!");

    RefCountInInterfacePointers(PR_TRUE);
    if (mOwner->GetProxyType() & NS_PROXY_ASYNC)
        CopyStrings(PR_TRUE);
}

nsProxyObjectCallInfo::~nsProxyObjectCallInfo()
{
    RefCountInInterfacePointers(PR_FALSE);
    if (mOwner->GetProxyType() & NS_PROXY_ASYNC)
        CopyStrings(PR_FALSE);

    mOwner = nsnull;
    
    if (mParameterList)  
        free(mParameterList);
}

NS_IMETHODIMP
nsProxyObjectCallInfo::QueryInterface(REFNSIID aIID, void **aResult)
{
    if (aIID.Equals(kFilterIID)) {
        *aResult = this;
        AddRef();
        return NS_OK;
    }
    return nsRunnable::QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsProxyObjectCallInfo::Run()
{
    PROXY_LOG(("PROXY(%p): Run\n", this));

    mResult = XPTC_InvokeByIndex(mOwner->GetProxiedInterface(),
                                 mMethodIndex,
                                 mParameterCount,
                                 mParameterList);

    if (IsSync()) {
        PostCompleted();
    }

    return NS_OK;
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
                    if (addRef)
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

        if (paramInfo.IsIn())
        {
            const nsXPTType& type = paramInfo.GetType();
            uint8 type_tag = type.TagPart();
            void *ptr = mParameterList[i].val.p;

            if (!ptr)
                continue;

            if (copy)
            {                
                switch (type_tag) 
                {
                    case nsXPTType::T_CHAR_STR:                                
                        mParameterList[i].val.p =
                            PL_strdup((const char *)ptr);
                        break;
                    case nsXPTType::T_WCHAR_STR:
                        mParameterList[i].val.p =
                            nsCRT::strdup((const PRUnichar *)ptr);
                        break;
                    case nsXPTType::T_DOMSTRING:
                    case nsXPTType::T_ASTRING:
                        mParameterList[i].val.p = 
                            new nsString(*((nsAString*) ptr));
                        break;
                    case nsXPTType::T_CSTRING:
                        mParameterList[i].val.p = 
                            new nsCString(*((nsACString*) ptr));
                        break;
                    case nsXPTType::T_UTF8STRING:                        
                        mParameterList[i].val.p = 
                            new nsUTF8String(*((nsAUTF8String*) ptr));
                        break;
                    default:
                        // Other types are ignored
                        break;                    
                }
            }
            else
            {
                switch (type_tag) 
                {
                    case nsXPTType::T_CHAR_STR:
                    case nsXPTType::T_WCHAR_STR:
                        PL_strfree((char*) ptr);
                        break;
                    case nsXPTType::T_DOMSTRING:
                    case nsXPTType::T_ASTRING:
                        delete (nsString*) ptr;
                        break;
                    case nsXPTType::T_CSTRING:
                        delete (nsCString*) ptr;
                        break;
                    case nsXPTType::T_UTF8STRING:
                        delete (nsUTF8String*) ptr;
                        break;
                    default:
                        // Other types are ignored
                        break;
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
    PROXY_LOG(("PROXY(%p): SetCompleted\n", this));
    PR_AtomicSet(&mCompleted, 1);
}

void                
nsProxyObjectCallInfo::PostCompleted()
{
    PROXY_LOG(("PROXY(%p): PostCompleted\n", this));

    if (mCallersTarget) {
        nsCOMPtr<nsIRunnable> event =
                new nsProxyCallCompletedEvent(this);
        if (event &&
            NS_SUCCEEDED(mCallersTarget->Dispatch(event, NS_DISPATCH_NORMAL)))
            return;
    }

    // OOM?  caller does not have a target?  This is an error!
    NS_WARNING("Failed to dispatch nsProxyCallCompletedEvent");
    SetCompleted();
}
  
nsIEventTarget*      
nsProxyObjectCallInfo::GetCallersTarget() 
{ 
    return mCallersTarget;
}

void
nsProxyObjectCallInfo::SetCallersTarget(nsIEventTarget* target)
{
    mCallersTarget = target;
}   

nsProxyObject::nsProxyObject(nsIEventTarget *target, PRInt32 proxyType,
                             nsISupports *realObject) :
  mProxyType(proxyType),
  mTarget(target),
  mRealObject(realObject),
  mFirst(nsnull)
{
    nsProxyObjectManager *pom = nsProxyObjectManager::GetInstance();
    NS_ASSERTION(pom, "Creating a proxy without a global proxy-object-manager.");
    pom->AddRef();
}

nsProxyObject::~nsProxyObject()
{   
    // Proxy the release of mRealObject to protect against it being deleted on
    // the wrong thread.
    nsISupports *doomed = nsnull;
    mRealObject.swap(doomed);
    NS_ProxyRelease(mTarget, doomed);
}

NS_IMPL_THREADSAFE_ADDREF(nsProxyObject)

NS_IMETHODIMP_(nsrefcnt)
nsProxyObject::Release()
{
    nsrefcnt count;
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    count = PR_AtomicDecrement((PRInt32*) &mRefCnt);
    NS_LOG_RELEASE(this, count, "nsProxyRelease");
    if (count)
        return count;

    nsProxyObjectManager *pom = nsProxyObjectManager::GetInstance();
    NS_ASSERTION(pom, "Deleting a proxy without a global proxy-object-manager.");

    pom->Remove(this);
    pom->Release();

    return 0;
}

NS_IMETHODIMP
nsProxyObject::QueryInterface(REFNSIID aIID, void **aResult)
{
    if (aIID.Equals(GetIID())) {
        *aResult = this;
        AddRef();
        return NS_OK;
    }

    if (aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsISupports*, this);
        AddRef();
        return NS_OK;
    }

    nsProxyObjectManager *pom = nsProxyObjectManager::GetInstance();
    NS_ASSERTION(pom, "Deleting a proxy without a global proxy-object-manager.");

    nsAutoMonitor mon(pom->GetMonitor());

    return LockedFind(aIID, aResult);
}

nsresult
nsProxyObject::LockedFind(REFNSIID aIID, void **aResult)
{
    // This method is only called when the global monitor is held.
    // XXX assert this

    nsProxyEventObject *peo;

    for (peo = mFirst; peo; peo = peo->mNext) {
        if (peo->GetClass()->GetProxiedIID().Equals(aIID)) {
            *aResult = NS_STATIC_CAST(nsISupports*, peo);
            peo->AddRef();
            return NS_OK;
        }
    }

    nsProxyEventClass *pec;
    nsresult rv = nsProxyObjectManager::GetInstance()->GetClass(aIID, &pec);
    if (NS_FAILED(rv))
        return rv;

    nsISomeInterface* newInterface;
    rv = mRealObject->QueryInterface(aIID, (void**) &newInterface);
    if (NS_FAILED(rv))
        return rv;

    peo = new nsProxyEventObject(this, pec, newInterface);
    if (!peo)
        return NS_ERROR_OUT_OF_MEMORY;

    peo->mNext = mFirst;
    mFirst = peo;

    NS_ADDREF(peo);

    *aResult = (nsISupports*) peo;
    return NS_OK;
}

void
nsProxyObject::LockedRemove(nsProxyEventObject *peo)
{
    nsProxyEventObject **i;
    for (i = &mFirst; *i; i = &((*i)->mNext)) {
        if (*i == peo) {
            *i = peo->mNext;
            break;
        }
    }
}

