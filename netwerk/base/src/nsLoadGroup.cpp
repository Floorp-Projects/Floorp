/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsLoadGroup.h"
#include "nsIStreamObserver.h"
#include "nsIChannel.h"
#include "nsISupportsArray.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kLoadGroupCID, NS_LOADGROUP_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsLoadGroup::nsLoadGroup(nsISupports* outer)
    : mChannels(nsnull), mSubGroups(nsnull), 
      mDefaultLoadAttributes(nsIChannel::LOAD_NORMAL),
      mObserver(nsnull), mParent(nsnull), mForegroundCount(0)
{
    NS_INIT_AGGREGATED(outer);
}

nsLoadGroup::~nsLoadGroup()
{
    nsresult rv = Cancel();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed");
    NS_IF_RELEASE(mChannels);
    NS_IF_RELEASE(mSubGroups);
    NS_IF_RELEASE(mObserver);
    NS_IF_RELEASE(mParent);
}
    
NS_METHOD
nsLoadGroup::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsLoadGroup* group = new nsLoadGroup(aOuter);
    if (group == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(group);
    nsresult rv = group->QueryInterface(aIID, aResult);
    NS_RELEASE(group);
    return rv;
}

NS_IMPL_AGGREGATED(nsLoadGroup);
    
NS_IMETHODIMP
nsLoadGroup::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(kLoadGroupCID) ||   // for internal use only (to set parent)
        aIID.Equals(nsCOMTypeInfo<nsILoadGroup>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIRequest>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsILoadGroup*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

nsresult
nsLoadGroup::PropagateDown(PropagateDownFun fun)
{
    nsresult rv;
    PRUint32 i;
    PRUint32 count = 0;
    if (mChannels) {
        rv = mChannels->Count(&count);
        if (NS_FAILED(rv)) return rv;
    }
    for (i = 0; i < count; i++) {
        // Operate the elements from back to front so that if items get
        // get removed from the list it won't affect our iteration
        nsIRequest* req =
            NS_STATIC_CAST(nsIRequest*, mChannels->ElementAt(count - 1 - i));
        if (req == nsnull)
            continue;
        rv = fun(req);
        NS_RELEASE(req);
        if (NS_FAILED(rv)) return rv;
    }

    count = 0;
    if (mSubGroups) {
        rv = mSubGroups->Count(&count);
        if (NS_FAILED(rv)) return rv;
    }
    for (i = 0; i < count; i++) {
        // Operate the elements from back to front so that if items get
        // get removed from the list it won't affect our iteration
        nsIRequest* req =
            NS_STATIC_CAST(nsIRequest*, mSubGroups->ElementAt(count - 1 - i));
        if (req == nsnull)
            continue;
        rv = fun(req);
        NS_RELEASE(req);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::IsPending(PRBool *result)
{
    nsresult rv;
    if (mForegroundCount > 0) {
        *result = PR_TRUE;
        return NS_OK;
    }

    // else check whether any of our sub-groups are pending
    PRUint32 i;
    PRUint32 count = 0;
    if (mSubGroups) {
        rv = mSubGroups->Count(&count);
        if (NS_FAILED(rv)) return rv;
    }
    for (i = 0; i < count; i++) {
        nsIRequest* req = NS_STATIC_CAST(nsIRequest*, mSubGroups->ElementAt(i));
        if (req == nsnull)
            continue;
        PRBool pending;
        rv = req->IsPending(&pending);
        NS_RELEASE(req);
        if (NS_FAILED(rv)) return rv;
        if (pending) {
            *result = PR_TRUE;
            return NS_OK;
        }
    }

    *result = PR_FALSE;
    return NS_OK;
}

static nsresult
CancelFun(nsIRequest* req)
{
    return req->Cancel();
}

NS_IMETHODIMP
nsLoadGroup::Cancel()
{
    return PropagateDown(CancelFun);
}

static nsresult
SuspendFun(nsIRequest* req)
{
    return req->Suspend();
}

NS_IMETHODIMP
nsLoadGroup::Suspend()
{
    return PropagateDown(SuspendFun);
}

static nsresult
ResumeFun(nsIRequest* req)
{
    return req->Resume();
}

NS_IMETHODIMP
nsLoadGroup::Resume()
{
    return PropagateDown(ResumeFun);
}

////////////////////////////////////////////////////////////////////////////////
// nsILoadGroup methods:

NS_IMETHODIMP
nsLoadGroup::Init(nsIStreamObserver *observer, nsILoadGroup *parent)
{
    nsresult rv;

    NS_IF_RELEASE(mObserver);
    if (observer) {
        nsCOMPtr<nsIEventQueue> eventQueue;
        NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), 
                                                getter_AddRefs(eventQueue));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIStreamObserver> asyncObserver;
        rv = NS_NewAsyncStreamObserver(getter_AddRefs(asyncObserver),
                                       eventQueue, observer);
        if (NS_FAILED(rv)) return rv;

        mObserver = asyncObserver;
        NS_ADDREF(mObserver);
    }

    if (parent) {
        rv = parent->AddSubGroup(this);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetDefaultLoadAttributes(PRUint32 *aDefaultLoadAttributes)
{
    *aDefaultLoadAttributes = mDefaultLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetDefaultLoadAttributes(PRUint32 aDefaultLoadAttributes)
{
    mDefaultLoadAttributes = aDefaultLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::AddChannel(nsIChannel *channel, nsISupports* ctxt)
{
    nsresult rv;
    if (mChannels == nsnull) {
        rv = NS_NewISupportsArray(&mChannels);
        if (NS_FAILED(rv)) return rv;
    }
    rv = mChannels->AppendElement(channel) ? NS_OK : NS_ERROR_FAILURE;	// XXX this method incorrectly returns a bool
    if (NS_FAILED(rv)) return rv;

    rv = channel->SetLoadAttributes(mDefaultLoadAttributes);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags flags;
    rv = channel->GetLoadAttributes(&flags);
    if (NS_SUCCEEDED(rv)) {
        if (!(flags & nsIChannel::LOAD_BACKGROUND)) {
            mForegroundCount++;
            if (mForegroundCount == 1 && mObserver) {
                rv = mObserver->OnStartRequest(channel, ctxt);
                // return with rv, below
            }
        }
    }

    return rv;
}

NS_IMETHODIMP
nsLoadGroup::RemoveChannel(nsIChannel *channel, nsISupports* ctxt,
                           nsresult status, const PRUnichar *errorMsg)
{
    nsresult rv;
    NS_ASSERTION(mChannels, "Forgot to call AddChannel");

    nsLoadFlags flags;
    rv = channel->GetLoadAttributes(&flags);
    if (NS_SUCCEEDED(rv)) {
        if (!(flags & nsIChannel::LOAD_BACKGROUND)) {
			NS_ASSERTION(mForegroundCount > 0, "mForegroundCount messed up");
            --mForegroundCount;
            if (mObserver) {
                PRBool pending;
                rv = IsPending(&pending);
                if (NS_SUCCEEDED(rv) && !pending) {
                    rv = mObserver->OnStopRequest(channel, ctxt, status, errorMsg);
                    // return with rv, below
                }
            }
        }
    }

    nsresult rv2 = mChannels->RemoveElement(channel) ? NS_OK : NS_ERROR_FAILURE;	// XXX this method incorrectly returns a bool
    return rv || rv2;
}

NS_IMETHODIMP
nsLoadGroup::GetChannels(nsISimpleEnumerator * *aChannels)
{
    nsresult rv;
    if (mChannels == nsnull) {
        rv = NS_NewISupportsArray(&mChannels);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_NewArrayEnumerator(aChannels, mChannels);
}

NS_IMETHODIMP
nsLoadGroup::AddSubGroup(nsILoadGroup *group)
{
    // set the parent pointer
    nsLoadGroup* subGroup;
    nsresult rv = group->QueryInterface(kLoadGroupCID, (void**)&subGroup);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsLoadGroup can't deal with other implementations yet");
    if (NS_FAILED(rv))
        return rv;
    subGroup->mParent = this;   // weak ref -- don't AddRef
    NS_RELEASE(subGroup);

    if (mSubGroups == nsnull) {
        rv = NS_NewISupportsArray(&mSubGroups);
        if (NS_FAILED(rv)) return rv;
    }
    return mSubGroups->AppendElement(group) ? NS_OK : NS_ERROR_FAILURE;	// XXX this method incorrectly returns a bool
}

NS_IMETHODIMP
nsLoadGroup::RemoveSubGroup(nsILoadGroup *group)
{
    NS_ASSERTION(mSubGroups, "Forgot to call AddSubGroup");

    // clear the parent pointer
    nsLoadGroup* subGroup;
    nsresult rv = group->QueryInterface(kLoadGroupCID, (void**)&subGroup);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsLoadGroup can't deal with other implementations yet");
    if (NS_FAILED(rv))
        return rv;
    subGroup->mParent = nsnull;   // weak ref -- don't Release
    NS_RELEASE(subGroup);

    return mSubGroups->RemoveElement(group) ? NS_OK : NS_ERROR_FAILURE;	// XXX this method incorrectly returns a bool
}

NS_IMETHODIMP
nsLoadGroup::GetSubGroups(nsISimpleEnumerator * *aSubGroups)
{
    nsresult rv;
    if (mSubGroups == nsnull) {
        rv = NS_NewISupportsArray(&mSubGroups);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_NewArrayEnumerator(aSubGroups, mSubGroups);
}

////////////////////////////////////////////////////////////////////////////////
