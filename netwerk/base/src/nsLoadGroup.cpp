/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Darin Fisher <darin@netscape.com>
 */

#include "nsLoadGroup.h"
#include "nsISupportsArray.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "prlog.h"
#include "nsCRT.h"
#include "netCore.h"
#include "nsXPIDLString.h"
#include "nsString.h"

#if defined(PR_LOGGING)
//
// Log module for nsILoadGroup logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=LoadGroup:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gLoadGroupLog = nsnull;
#endif

#define LOG(args) PR_LOG(gLoadGroupLog, PR_LOG_DEBUG, args)

////////////////////////////////////////////////////////////////////////////////

nsLoadGroup::nsLoadGroup(nsISupports* outer)
    : mLoadFlags(LOAD_NORMAL)
    , mForegroundCount(0)
    , mRequests(nsnull)
    , mStatus(NS_OK)
{
    NS_INIT_AGGREGATED(outer);

#if defined(PR_LOGGING)
    // Initialize the global PRLogModule for nsILoadGroup logging
    if (nsnull == gLoadGroupLog)
        gLoadGroupLog = PR_NewLogModule("LoadGroup");
#endif

    LOG(("LOADGROUP [%x]: Created.\n", this));
}

nsLoadGroup::~nsLoadGroup()
{
    nsresult rv;

    rv = Cancel(NS_BINDING_ABORTED);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed");

    NS_IF_RELEASE(mRequests);
    mDefaultLoadRequest = 0;

    LOG(("LOADGROUP [%x]: Destroyed.\n", this));
}


nsresult nsLoadGroup::Init()
{
    return NS_NewISupportsArray(&mRequests);
}


NS_METHOD
nsLoadGroup::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_PROPER_AGGREGATION(aOuter, aIID);

    nsresult rv;
    nsLoadGroup* group = new nsLoadGroup(aOuter);
    if (group == nsnull) {
        *aResult = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = group->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = group->AggregatedQueryInterface(aIID, aResult);
    }

    if (NS_FAILED(rv))
        delete group;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_AGGREGATED(nsLoadGroup);

NS_IMETHODIMP
nsLoadGroup::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

    if (aIID.Equals(NS_GET_IID(nsISupports)))
        *aInstancePtr = GetInner();
    else if (aIID.Equals(NS_GET_IID(nsILoadGroup)) ||
        aIID.Equals(NS_GET_IID(nsIRequest)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = NS_STATIC_CAST(nsILoadGroup*, this);
    }
    else if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
        *aInstancePtr = NS_STATIC_CAST(nsISupportsWeakReference*,this);
    }
    else {
        *aInstancePtr = nsnull;
        return NS_NOINTERFACE;
    }

    NS_ADDREF((nsISupports*)*aInstancePtr);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsLoadGroup::GetName(PRUnichar* *result)
{
    // XXX is this the right "name" for a load group?
    *result = nsnull;
    
    if (mDefaultLoadRequest) {
        nsXPIDLString nameStr;
        nsresult rv = mDefaultLoadRequest->GetName(getter_Copies(nameStr));
        if (NS_SUCCEEDED(rv)) {
            nsString name;
            name.Assign(nameStr);
            *result = name.ToNewUnicode();
            return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
        }
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::IsPending(PRBool *aResult)
{
    *aResult = (mForegroundCount > 0) ? PR_TRUE : PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetStatus(nsresult *status)
{
    if (NS_SUCCEEDED(mStatus) && mDefaultLoadRequest)
        return mDefaultLoadRequest->GetStatus(status);
    
    *status = mStatus;
    return NS_OK; 
}

NS_IMETHODIMP
nsLoadGroup::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsresult rv, firstError;
    PRUint32 count;

    // set the load group status to our cancel status while we cancel 
    // all our requests...once the cancel is done, we'll reset it...

    mStatus = status;

    rv = mRequests->Count(&count);
    if (NS_FAILED(rv)) return rv;

    firstError = NS_OK;

    if (count) {
        nsIRequest* request;

        //
        // Operate the elements from back to front so that if items get
        // get removed from the list it won't affect our iteration
        //
        while (count > 0) {
            request = NS_STATIC_CAST(nsIRequest*, mRequests->ElementAt(--count));

            NS_ASSERTION(request, "NULL request found in list.");
            if (!request)
                continue;

#if defined(PR_LOGGING)
            nsXPIDLString nameStr;
            request->GetName(getter_Copies(nameStr));
            LOG(("LOADGROUP [%x]: Canceling request %x %s.\n",
                this, request, NS_ConvertUCS2toUTF8(nameStr).get()));
#endif

            //
            // Remove the request from the load group...  This may cause
            // the OnStopRequest notification to fire...
            //
            // XXX: What should the context be?
            //
            (void)RemoveRequest(request, nsnull, status);

            // Cancel the request...
            rv = request->Cancel(status);

            // Remember the first failure and return it...
            if (NS_FAILED(rv) && NS_SUCCEEDED(firstError))
                firstError = rv;

            NS_RELEASE(request);
        }

#if defined(DEBUG)
        (void)mRequests->Count(&count);
        NS_ASSERTION(count == 0,            "Request list is not empty.");
        NS_ASSERTION(mForegroundCount == 0, "Foreground URLs are active.");
#endif
    }

    mStatus = NS_OK;
    return firstError;
}


NS_IMETHODIMP
nsLoadGroup::Suspend()
{
    nsresult rv, firstError;
    PRUint32 count;

    rv = mRequests->Count(&count);
    if (NS_FAILED(rv)) return rv;

    firstError = NS_OK;
    //
    // Operate the elements from back to front so that if items get
    // get removed from the list it won't affect our iteration
    //
    while (count > 0) {
        nsIRequest* request = NS_STATIC_CAST(nsIRequest*, mRequests->ElementAt(--count));

        NS_ASSERTION(request, "NULL request found in list.");
        if (!request)
            continue;

#if defined(PR_LOGGING)
        nsXPIDLString nameStr;
        request->GetName(getter_Copies(nameStr));
        LOG(("LOADGROUP [%x]: Suspending request %x %s.\n",
            this, request, NS_ConvertUCS2toUTF8(nameStr).get()));
#endif

        // Suspend the request...
        rv = request->Suspend();

        // Remember the first failure and return it...
        if (NS_FAILED(rv) && NS_SUCCEEDED(firstError))
            firstError = rv;

        NS_RELEASE(request);
    }

    return firstError;
}


NS_IMETHODIMP
nsLoadGroup::Resume()
{
    nsresult rv, firstError;
    PRUint32 count;

    rv = mRequests->Count(&count);
    if (NS_FAILED(rv)) return rv;

    firstError = NS_OK;
    //
    // Operate the elements from back to front so that if items get
    // get removed from the list it won't affect our iteration
    //
    while (count > 0) {
        nsIRequest* request = NS_STATIC_CAST(nsIRequest*, mRequests->ElementAt(--count));

        NS_ASSERTION(request, "NULL request found in list.");
        if (!request)
            continue;

#if defined(PR_LOGGING)
        nsXPIDLString nameStr;
        request->GetName(getter_Copies(nameStr));
        LOG(("LOADGROUP [%x]: Resuming request %x %s.\n",
            this, request, NS_ConvertUCS2toUTF8(nameStr).get()));
#endif

        // Resume the request...
        rv = request->Resume();

        // Remember the first failure and return it...
        if (NS_FAILED(rv) && NS_SUCCEEDED(firstError))
            firstError = rv;

        NS_RELEASE(request);
    }

    return firstError;
}

NS_IMETHODIMP
nsLoadGroup::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetLoadGroup(nsILoadGroup **loadGroup)
{
    *loadGroup = mLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetLoadGroup(nsILoadGroup *loadGroup)
{
    mLoadGroup = loadGroup;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsILoadGroup methods:

NS_IMETHODIMP
nsLoadGroup::GetDefaultLoadRequest(nsIRequest * *aRequest)
{
    *aRequest = mDefaultLoadRequest;
    NS_IF_ADDREF(*aRequest);
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetDefaultLoadRequest(nsIRequest *aRequest)
{
    mDefaultLoadRequest = aRequest;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::AddRequest(nsIRequest *request, nsISupports* ctxt)
{
    nsresult rv;

#if defined(PR_LOGGING)
    PRUint32 count = 0;
    (void)mRequests->Count(&count);
    nsXPIDLString nameStr;
    request->GetName(getter_Copies(nameStr));
    LOG(("LOADGROUP [%x]: Adding request %x %s (count=%d).\n",
        this, request, NS_ConvertUCS2toUTF8(nameStr).get(), count));
#endif

    nsLoadFlags flags;
    rv = MergeLoadFlags(request, flags);
    if (NS_FAILED(rv)) return rv;
    
    //
    // Add the request to the list of active requests...
    //
    // XXX this method incorrectly returns a bool
    //
    rv = mRequests->AppendElement(request) ? NS_OK : NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) return rv;

    if (!(flags & nsIRequest::LOAD_BACKGROUND)) {
        // Update the count of foreground URIs..
        mForegroundCount += 1;

        //
        // Fire the OnStartRequest notification out to the observer...
        //
        // If the notification fails then DO NOT add the request to
        // the load group.
        //
        nsCOMPtr<nsIRequestObserver> observer = do_QueryReferent(mObserver);
        if (observer) {
            LOG(("LOADGROUP [%x]: Firing OnStartRequest for request %x."
                 "(foreground count=%d).\n", this, request, mForegroundCount));

            rv = observer->OnStartRequest(request, ctxt);
            if (NS_FAILED(rv)) {
                LOG(("LOADGROUP [%x]: OnStartRequest for request %x FAILED.\n",
                    this, request));
                //
                // The URI load has been canceled by the observer.  Clean up
                // the damage...
                //
                // XXX this method incorrectly returns a bool
                //
                rv = mRequests->RemoveElement(request) ? NS_OK : NS_ERROR_FAILURE;
                mForegroundCount -= 1;
            }
        }
    }

    return rv;
}

NS_IMETHODIMP
nsLoadGroup::RemoveRequest(nsIRequest *request, nsISupports* ctxt, nsresult aStatus)
{
    nsresult rv;

#if defined(PR_LOGGING)
        PRUint32 count = 0;
        (void)mRequests->Count(&count);
        nsXPIDLString nameStr;
        request->GetName(getter_Copies(nameStr));
        LOG(("LOADGROUP [%x]: Removing request %x %s status %x (count=%d).\n",
            this, request, NS_ConvertUCS2toUTF8(nameStr).get(), aStatus, count-1));
#endif

    //
    // Remove the request from the group.  If this fails, it means that
    // the request was *not* in the group so do not update the foreground
    // count or it will get messed up...
    //
    //
    // XXX this method incorrectly returns a bool
    //
    rv = mRequests->RemoveElement(request) ? NS_OK : NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) {
        LOG(("LOADGROUP [%x]: Unable to remove request %x. Not in group!\n",
            this, request));
        return rv;
    }

    nsLoadFlags flags;
    rv = request->GetLoadFlags(&flags);
    if (NS_FAILED(rv)) return rv;

    if (!(flags & nsIRequest::LOAD_BACKGROUND)) {
        NS_ASSERTION(mForegroundCount > 0, "ForegroundCount messed up");
        mForegroundCount -= 1;

        // Fire the OnStopRequest out to the observer...
        nsCOMPtr<nsIRequestObserver> observer = do_QueryReferent(mObserver);
        if (observer) {
            LOG(("LOADGROUP [%x]: Firing OnStopRequest for request %x."
                 "(foreground count=%d).\n", this, request, mForegroundCount));

            rv = observer->OnStopRequest(request, ctxt, aStatus);
            if (NS_FAILED(rv))
                LOG(("LOADGROUP [%x]: OnStopRequest for request %x FAILED.\n",
                    this, request));
        }
    }

    return rv;
}

NS_IMETHODIMP
nsLoadGroup::GetRequests(nsISimpleEnumerator * *aRequests)
{
    return NS_NewArrayEnumerator(aRequests, mRequests);
}

NS_IMETHODIMP
nsLoadGroup::SetGroupObserver(nsIRequestObserver* aObserver)
{
    mObserver = getter_AddRefs(NS_GetWeakReference(aObserver));
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetGroupObserver(nsIRequestObserver* *aResult)
{
    nsCOMPtr<nsIRequestObserver> observer = do_QueryReferent(mObserver);
    *aResult = observer;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetActiveCount(PRUint32* aResult)
{
    *aResult = mForegroundCount;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
    NS_ENSURE_ARG_POINTER(aCallbacks);
    *aCallbacks = mCallbacks;
    NS_IF_ADDREF(*aCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks)
{
    mCallbacks = aCallbacks;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult nsLoadGroup::MergeLoadFlags(nsIRequest *aRequest, nsLoadFlags& outFlags)
{
    nsresult rv;
    nsLoadFlags flags, oldFlags;

    rv = aRequest->GetLoadFlags(&flags);
    if (NS_FAILED(rv)) 
        return rv;

    oldFlags = flags;
    //
    // Inherit the group cache validation policy
    //
    if ( !((nsIRequest::VALIDATE_NEVER | 
            nsIRequest::VALIDATE_ALWAYS | 
            nsIRequest::VALIDATE_ONCE_PER_SESSION) & flags) ) {
        flags |= (nsIRequest::VALIDATE_NEVER |
                  nsIRequest::VALIDATE_ALWAYS |
                  nsIRequest::VALIDATE_ONCE_PER_SESSION) & mLoadFlags;
    }
    //
    // Inherit the group reload policy
    //
    if (!(nsIRequest::FORCE_VALIDATION & flags))
        flags |= (nsIRequest::FORCE_VALIDATION & mLoadFlags);
    if (!(nsIRequest::FORCE_RELOAD & flags))
        flags |= (nsIRequest::FORCE_RELOAD & mLoadFlags);

    //
    // Inherit the group persistent cache policy
    //
    if (!(nsIRequest::INHIBIT_PERSISTENT_CACHING & flags))
        flags |= (nsIRequest::INHIBIT_PERSISTENT_CACHING & mLoadFlags);

    //
    // Inherit the group loading policy
    //
    if (!(nsIRequest::LOAD_BACKGROUND & flags))
        flags |= (nsIRequest::LOAD_BACKGROUND & mLoadFlags);

    if (flags != oldFlags)
        rv = aRequest->SetLoadFlags(flags);

    outFlags = flags;
    return rv;
}
