/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsReadableUtils.h"
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
    : mForegroundCount(0)
    , mLoadFlags(LOAD_NORMAL)
    , mRequests(nsnull)
    , mStatus(NS_OK)
    , mIsCanceling(PR_FALSE)
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
            *result = ToNewUnicode(name);
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

    rv = mRequests->Count(&count);
    if (NS_FAILED(rv)) return rv;

    // set the load group status to our cancel status while we cancel 
    // all our requests...once the cancel is done, we'll reset it...
    //
    mStatus = status;

    // Set the flag indicating that the loadgroup is being canceled...  This
    // prevents any new channels from being added during the operation.
    //
    mIsCanceling = PR_TRUE;

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
    mIsCanceling = PR_FALSE;

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
    // Inherit the group load flags from the default load request
    if (mDefaultLoadRequest)
        mDefaultLoadRequest->GetLoadFlags(&mLoadFlags);
    // Else, do not change the group's load flags (see bug 95981)
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
#endif /* PR_LOGGING */

    //
    // Do not add the channel, if the loadgroup is being canceled...
    //
    if (mIsCanceling) {

#if defined(PR_LOGGING)
        LOG(("LOADGROUP [%x]: AddChannel() ABORTED because LoadGroup is"
             " being canceled!!\n", this));
#endif /* PR_LOGGING */

        return NS_BINDING_ABORTED;
    }

    nsLoadFlags flags;
    // if the request is the default load request or if the default load request
    // is null, then the load group should inherit its load flags from the request.
    if (mDefaultLoadRequest == request || !mDefaultLoadRequest)
        rv = request->GetLoadFlags(&flags);
    else
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

    // Inherit the following bits...
    flags |= (mLoadFlags & (LOAD_BACKGROUND |
                            LOAD_BYPASS_CACHE |
                            LOAD_FROM_CACHE |
                            VALIDATE_ALWAYS |
                            VALIDATE_ONCE_PER_SESSION |
                            VALIDATE_NEVER));

    if (flags != oldFlags)
        rv = aRequest->SetLoadFlags(flags);

    outFlags = flags;
    return rv;
}
