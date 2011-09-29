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

#include "nsXPCOMCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsThreadUtils.h"
#include "nsEventQueue.h"
#include "nsMemory.h"

using namespace mozilla;

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
                                             const XPTMethodDescriptor *methodInfo,
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

    mResult = NS_InvokeByIndex(mOwner->GetProxiedInterface(),
                               mMethodIndex,
                               mParameterCount,
                               mParameterList);

    if (IsSync()) {
        PostCompleted();
    }

    return NS_OK;
}

void
nsProxyObjectCallInfo::RefCountInInterfacePointers(bool addRef)
{
    for (PRUint32 i = 0; i < mParameterCount; i++)
    {
        nsXPTParamInfo paramInfo = mMethodInfo->params[i];

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
nsProxyObjectCallInfo::CopyStrings(bool copy)
{
    for (PRUint32 i = 0; i < mParameterCount; i++)
    {
        const nsXPTParamInfo paramInfo = mMethodInfo->params[i];

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
                        PL_strfree((char*) ptr);
                        break;
                    case nsXPTType::T_WCHAR_STR:
                        nsCRT::free((PRUnichar*)ptr);
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

bool                  
nsProxyObjectCallInfo::GetCompleted()
{
    return !!mCompleted;
}

void
nsProxyObjectCallInfo::SetCompleted()
{
    PROXY_LOG(("PROXY(%p): SetCompleted\n", this));
    PR_ATOMIC_SET(&mCompleted, 1);
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
    MOZ_COUNT_CTOR(nsProxyObject);

#ifdef DEBUG
    nsCOMPtr<nsISupports> canonicalTarget = do_QueryInterface(target);
    NS_ASSERTION(target == canonicalTarget,
                 "Non-canonical nsISupports passed to nsProxyObject constructor");
#endif
}

nsProxyObject::~nsProxyObject()
{
    // Proxy the release of mRealObject to protect against it being deleted on
    // the wrong thread.
    nsISupports *doomed = nsnull;
    mRealObject.swap(doomed);
    NS_ProxyRelease(mTarget, doomed);

    MOZ_COUNT_DTOR(nsProxyObject);
}

NS_IMETHODIMP_(nsrefcnt)
nsProxyObject::AddRef()
{
    MutexAutoLock lock(nsProxyObjectManager::GetInstance()->GetLock());
    return LockedAddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsProxyObject::Release()
{
    MutexAutoLock lock(nsProxyObjectManager::GetInstance()->GetLock());
    return LockedRelease();
}

nsrefcnt
nsProxyObject::LockedAddRef()
{
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsProxyObject", sizeof(nsProxyObject));
    return mRefCnt;
}

nsrefcnt
nsProxyObject::LockedRelease()
{
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsProxyObject");
    if (mRefCnt)
        return mRefCnt;

    nsProxyObjectManager *pom = nsProxyObjectManager::GetInstance();
    pom->LockedRemove(this);

    MutexAutoUnlock unlock(pom->GetLock());
    delete this;

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
        *aResult = static_cast<nsISupports*>(this);
        AddRef();
        return NS_OK;
    }

    nsProxyObjectManager *pom = nsProxyObjectManager::GetInstance();
    NS_ASSERTION(pom, "Deleting a proxy without a global proxy-object-manager.");

    MutexAutoLock lock(pom->GetLock());
    return LockedFind(aIID, aResult);
}

nsresult
nsProxyObject::LockedFind(REFNSIID aIID, void **aResult)
{
    // This method is only called when the global lock is held.
#ifdef DEBUG
    nsProxyObjectManager::GetInstance()->GetLock().AssertCurrentThreadOwns();
#endif

    nsProxyEventObject *peo;

    for (peo = mFirst; peo; peo = peo->mNext) {
        if (peo->GetClass()->GetProxiedIID().Equals(aIID)) {
            *aResult = static_cast<nsISupports*>(peo->mXPTCStub);
            peo->LockedAddRef();
            return NS_OK;
        }
    }

    nsProxyEventObject *newpeo;

    // Both GetClass and QueryInterface call out to XPCOM, so we unlock for them
    nsProxyObjectManager* pom = nsProxyObjectManager::GetInstance();
    {
        MutexAutoUnlock unlock(pom->GetLock());

        nsProxyEventClass *pec;
        nsresult rv = pom->GetClass(aIID, &pec);
        if (NS_FAILED(rv))
            return rv;

        nsISomeInterface* newInterface;
        rv = mRealObject->QueryInterface(aIID, (void**) &newInterface);
        if (NS_FAILED(rv))
            return rv;

        newpeo = new nsProxyEventObject(this, pec, 
                       already_AddRefed<nsISomeInterface>(newInterface), &rv);
        if (!newpeo) {
            NS_RELEASE(newInterface);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        if (NS_FAILED(rv)) {
            delete newpeo;
            return rv;
        }
    }

    // Now that we're locked again, check for races by repeating the
    // linked-list check.
    for (peo = mFirst; peo; peo = peo->mNext) {
        if (peo->GetClass()->GetProxiedIID().Equals(aIID)) {
            // Best to AddRef for our caller before unlocking.
            peo->LockedAddRef();

            {
                // Deleting an nsProxyEventObject can call Release on an
                // nsProxyObject, which can only happen when not holding
                // the lock.
                MutexAutoUnlock unlock(pom->GetLock());
                delete newpeo;
            }
            *aResult = static_cast<nsISupports*>(peo->mXPTCStub);
            return NS_OK;
        }
    }

    newpeo->mNext = mFirst;
    mFirst = newpeo;

    newpeo->LockedAddRef();

    *aResult = static_cast<nsISupports*>(newpeo->mXPTCStub);
    return NS_OK;
}

void
nsProxyObject::LockedRemove(nsProxyEventObject *peo)
{
    nsProxyEventObject **i;
    for (i = &mFirst; *i; i = &((*i)->mNext)) {
        if (*i == peo) {
            *i = peo->mNext;
            return;
        }
    }
    NS_ERROR("Didn't find nsProxyEventObject in nsProxyObject chain!");
}
