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
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsEnumeratorUtils.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsLoadGroup::nsLoadGroup(nsISupports* outer)
    : mChannels(nsnull), mSubGroups(nsnull), 
      mDefaultLoadAttributes(nsIChannel::LOAD_NORMAL)
{
    NS_INIT_AGGREGATED(outer);
}

nsLoadGroup::~nsLoadGroup()
{
    NS_IF_RELEASE(mChannels);
    NS_IF_RELEASE(mSubGroups);
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
    if (aIID.Equals(nsILoadGroup::GetIID()) ||
        aIID.Equals(nsIRequest::GetIID()) ||
        aIID.Equals(nsISupports::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsILoadGroup*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

nsresult
nsLoadGroup::Iterate(IterateFun fun)
{
    nsresult rv;
    PRUint32 i;
    PRUint32 count;
    rv = mChannels->Count(&count);
    if (NS_FAILED(rv)) return rv;
    for (i = 0; i < count; i++) {
        nsIRequest* req = NS_STATIC_CAST(nsIRequest*, mChannels->ElementAt(i));
        if (req == nsnull)
            return NS_ERROR_FAILURE;
        rv = fun(req);
        NS_RELEASE(req);
        if (NS_FAILED(rv)) return rv;
    }

    rv = mSubGroups->Count(&count);
    if (NS_FAILED(rv)) return rv;
    for (i = 0; i < count; i++) {
        nsIRequest* req = NS_STATIC_CAST(nsIRequest*, mSubGroups->ElementAt(i));
        if (req == nsnull)
            return NS_ERROR_FAILURE;
        rv = fun(req);
        NS_RELEASE(req);
        if (NS_FAILED(rv)) return rv;
    }
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
    return Iterate(CancelFun);
}

static nsresult
SuspendFun(nsIRequest* req)
{
    return req->Suspend();
}

NS_IMETHODIMP
nsLoadGroup::Suspend()
{
    return Iterate(SuspendFun);
}

static nsresult
ResumeFun(nsIRequest* req)
{
    return req->Resume();
}

NS_IMETHODIMP
nsLoadGroup::Resume()
{
    return Iterate(ResumeFun);
}

////////////////////////////////////////////////////////////////////////////////
// nsILoadGroup methods:
    
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
nsLoadGroup::AddChannel(nsIChannel *channel)
{
    nsresult rv = mChannels->AppendElement(channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->SetLoadAttributes(mDefaultLoadAttributes);
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::RemoveChannel(nsIChannel *channel)
{
    return mChannels->RemoveElement(channel);
}

NS_IMETHODIMP
nsLoadGroup::GetChannels(nsISimpleEnumerator * *aChannels)
{
    return NS_NewArrayEnumerator(aChannels, mChannels);
}

NS_IMETHODIMP
nsLoadGroup::AddSubGroup(nsILoadGroup *group)
{
    return mSubGroups->AppendElement(group);
}

NS_IMETHODIMP
nsLoadGroup::RemoveSubGroup(nsILoadGroup *group)
{
    return mSubGroups->RemoveElement(group);
}

NS_IMETHODIMP
nsLoadGroup::GetSubGroups(nsISimpleEnumerator * *aSubGroups)
{
    return NS_NewArrayEnumerator(aSubGroups, mSubGroups);
}

////////////////////////////////////////////////////////////////////////////////
