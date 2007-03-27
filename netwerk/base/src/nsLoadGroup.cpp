/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=4 sts=4 et cin: */
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
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsLoadGroup.h"
#include "nsISupportsArray.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "prlog.h"
#include "nsCRT.h"
#include "netCore.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsVoidArray.h"

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
static PRLogModuleInfo* gLoadGroupLog = nsnull;
#endif

#define LOG(args) PR_LOG(gLoadGroupLog, PR_LOG_DEBUG, args)

////////////////////////////////////////////////////////////////////////////////

class RequestMapEntry : public PLDHashEntryHdr
{
public:
    RequestMapEntry(nsIRequest *aRequest) :
        mKey(aRequest)
    {
    }

    nsCOMPtr<nsIRequest> mKey;
};

PR_STATIC_CALLBACK(PRBool)
RequestHashMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *entry,
                      const void *key)
{
    const RequestMapEntry *e =
        NS_STATIC_CAST(const RequestMapEntry *, entry);
    const nsIRequest *request = NS_STATIC_CAST(const nsIRequest *, key);

    return e->mKey == request;
}

PR_STATIC_CALLBACK(void)
RequestHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    RequestMapEntry *e = NS_STATIC_CAST(RequestMapEntry *, entry);

    // An entry is being cleared, let the entry do its own cleanup.
    e->~RequestMapEntry();
}

PR_STATIC_CALLBACK(PRBool)
RequestHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                     const void *key)
{
    const nsIRequest *const_request = NS_STATIC_CAST(const nsIRequest *, key);
    nsIRequest *request = NS_CONST_CAST(nsIRequest *, const_request);

    // Initialize the entry with placement new
    new (entry) RequestMapEntry(request);
    return PR_TRUE;
}


static void
RescheduleRequest(nsIRequest *aRequest, PRInt32 delta)
{
    nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(aRequest);
    if (p)
        p->AdjustPriority(delta);
}

PR_STATIC_CALLBACK(PLDHashOperator)
RescheduleRequests(PLDHashTable *table, PLDHashEntryHdr *hdr,
                   PRUint32 number, void *arg)
{
    RequestMapEntry *e = NS_STATIC_CAST(RequestMapEntry *, hdr);
    PRInt32 *delta = NS_STATIC_CAST(PRInt32 *, arg);

    RescheduleRequest(e->mKey, *delta);
    return PL_DHASH_NEXT;
}


nsLoadGroup::nsLoadGroup(nsISupports* outer)
    : mForegroundCount(0)
    , mLoadFlags(LOAD_NORMAL)
    , mStatus(NS_OK)
    , mPriority(PRIORITY_NORMAL)
    , mIsCanceling(PR_FALSE)
{
    NS_INIT_AGGREGATED(outer);

#if defined(PR_LOGGING)
    // Initialize the global PRLogModule for nsILoadGroup logging
    if (nsnull == gLoadGroupLog)
        gLoadGroupLog = PR_NewLogModule("LoadGroup");
#endif

    LOG(("LOADGROUP [%x]: Created.\n", this));

    // Initialize the ops in the hash to null to make sure we get
    // consistent errors if someone fails to call ::Init() on an
    // nsLoadGroup.
    mRequests.ops = nsnull;
}

nsLoadGroup::~nsLoadGroup()
{
    nsresult rv;

    rv = Cancel(NS_BINDING_ABORTED);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed");

    if (mRequests.ops) {
        PL_DHashTableFinish(&mRequests);
    }

    mDefaultLoadRequest = 0;

    LOG(("LOADGROUP [%x]: Destroyed.\n", this));
}


nsresult nsLoadGroup::Init()
{
    static PLDHashTableOps hash_table_ops =
    {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        PL_DHashVoidPtrKeyStub,
        RequestHashMatchEntry,
        PL_DHashMoveEntryStub,
        RequestHashClearEntry,
        PL_DHashFinalizeStub,
        RequestHashInitEntry
    };

    if (!PL_DHashTableInit(&mRequests, &hash_table_ops, nsnull,
                           sizeof(RequestMapEntry), 16)) {
        mRequests.ops = nsnull;

        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_AGGREGATED(nsLoadGroup)
NS_INTERFACE_MAP_BEGIN_AGGREGATED(nsLoadGroup)
    NS_INTERFACE_MAP_ENTRY(nsILoadGroup)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsLoadGroup::GetName(nsACString &result)
{
    // XXX is this the right "name" for a load group?

    if (!mDefaultLoadRequest) {
        result.Truncate();
        return NS_OK;
    }
    
    return mDefaultLoadRequest->GetName(result);
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

// PLDHashTable enumeration callback that appends strong references to
// all nsIRequest to an nsVoidArray.
PR_STATIC_CALLBACK(PLDHashOperator)
AppendRequestsToVoidArray(PLDHashTable *table, PLDHashEntryHdr *hdr,
                          PRUint32 number, void *arg)
{
    RequestMapEntry *e = NS_STATIC_CAST(RequestMapEntry *, hdr);
    nsVoidArray *array = NS_STATIC_CAST(nsVoidArray *, arg);

    nsIRequest *request = e->mKey;
    NS_ASSERTION(request, "What? Null key in pldhash entry?");

    PRBool ok = array->AppendElement(request);

    if (!ok) {
        return PL_DHASH_STOP;
    }

    NS_ADDREF(request);

    return PL_DHASH_NEXT;
}

// nsVoidArray enumeration callback that releases all items in the
// nsVoidArray
PR_STATIC_CALLBACK(PRBool)
ReleaseVoidArrayItems(void *aElement, void *aData)
{
    nsISupports *s = NS_STATIC_CAST(nsISupports *, aElement);

    NS_RELEASE(s);

    return PR_TRUE;
}

NS_IMETHODIMP
nsLoadGroup::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsresult rv;
    PRUint32 count = mRequests.entryCount;

    nsAutoVoidArray requests;

    PL_DHashTableEnumerate(&mRequests, AppendRequestsToVoidArray,
                           NS_STATIC_CAST(nsVoidArray *, &requests));

    if (requests.Count() != (PRInt32)count) {
        requests.EnumerateForwards(ReleaseVoidArrayItems, nsnull);

        return NS_ERROR_OUT_OF_MEMORY;
    }

    // set the load group status to our cancel status while we cancel 
    // all our requests...once the cancel is done, we'll reset it...
    //
    mStatus = status;

    // Set the flag indicating that the loadgroup is being canceled...  This
    // prevents any new channels from being added during the operation.
    //
    mIsCanceling = PR_TRUE;

    nsresult firstError = NS_OK;

    while (count > 0) {
        nsIRequest* request = NS_STATIC_CAST(nsIRequest*, requests.ElementAt(--count));

        NS_ASSERTION(request, "NULL request found in list.");

        RequestMapEntry *entry =
            NS_STATIC_CAST(RequestMapEntry *,
                           PL_DHashTableOperate(&mRequests, request,
                                                PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_FREE(entry)) {
            // |request| was removed already

            NS_RELEASE(request);

            continue;
        }

#if defined(PR_LOGGING)
        nsCAutoString nameStr;
        request->GetName(nameStr);
        LOG(("LOADGROUP [%x]: Canceling request %x %s.\n",
             this, request, nameStr.get()));
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
    NS_ASSERTION(mRequests.entryCount == 0, "Request list is not empty.");
    NS_ASSERTION(mForegroundCount == 0, "Foreground URLs are active.");
#endif

    mStatus = NS_OK;
    mIsCanceling = PR_FALSE;

    return firstError;
}


NS_IMETHODIMP
nsLoadGroup::Suspend()
{
    nsresult rv, firstError;
    PRUint32 count = mRequests.entryCount;

    nsAutoVoidArray requests;

    PL_DHashTableEnumerate(&mRequests, AppendRequestsToVoidArray,
                           NS_STATIC_CAST(nsVoidArray *, &requests));

    if (requests.Count() != (PRInt32)count) {
        requests.EnumerateForwards(ReleaseVoidArrayItems, nsnull);

        return NS_ERROR_OUT_OF_MEMORY;
    }

    firstError = NS_OK;
    //
    // Operate the elements from back to front so that if items get
    // get removed from the list it won't affect our iteration
    //
    while (count > 0) {
        nsIRequest* request =
            NS_STATIC_CAST(nsIRequest*, requests.ElementAt(--count));

        NS_ASSERTION(request, "NULL request found in list.");
        if (!request)
            continue;

#if defined(PR_LOGGING)
        nsCAutoString nameStr;
        request->GetName(nameStr);
        LOG(("LOADGROUP [%x]: Suspending request %x %s.\n",
            this, request, nameStr.get()));
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
    PRUint32 count = mRequests.entryCount;

    nsAutoVoidArray requests;

    PL_DHashTableEnumerate(&mRequests, AppendRequestsToVoidArray,
                           NS_STATIC_CAST(nsVoidArray *, &requests));

    if (requests.Count() != (PRInt32)count) {
        requests.EnumerateForwards(ReleaseVoidArrayItems, nsnull);

        return NS_ERROR_OUT_OF_MEMORY;
    }

    firstError = NS_OK;
    //
    // Operate the elements from back to front so that if items get
    // get removed from the list it won't affect our iteration
    //
    while (count > 0) {
        nsIRequest* request =
            NS_STATIC_CAST(nsIRequest*, requests.ElementAt(--count));

        NS_ASSERTION(request, "NULL request found in list.");
        if (!request)
            continue;

#if defined(PR_LOGGING)
        nsCAutoString nameStr;
        request->GetName(nameStr);
        LOG(("LOADGROUP [%x]: Resuming request %x %s.\n",
            this, request, nameStr.get()));
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
    NS_IF_ADDREF(*loadGroup);
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
    if (mDefaultLoadRequest) {
        mDefaultLoadRequest->GetLoadFlags(&mLoadFlags);
        //
        // Mask off any bits that are not part of the nsIRequest flags.
        // in particular, nsIChannel::LOAD_DOCUMENT_URI...
        //
        mLoadFlags &= 0xFFFF;
    }
    // Else, do not change the group's load flags (see bug 95981)
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::AddRequest(nsIRequest *request, nsISupports* ctxt)
{
    nsresult rv;

#if defined(PR_LOGGING)
    {
        nsCAutoString nameStr;
        request->GetName(nameStr);
        LOG(("LOADGROUP [%x]: Adding request %x %s (count=%d).\n",
             this, request, nameStr.get(), mRequests.entryCount));
    }
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
    // if the request is the default load request or if the default
    // load request is null, then the load group should inherit its
    // load flags from the request.
    if (mDefaultLoadRequest == request || !mDefaultLoadRequest)
        rv = request->GetLoadFlags(&flags);
    else
        rv = MergeLoadFlags(request, flags);
    if (NS_FAILED(rv)) return rv;
    
    //
    // Add the request to the list of active requests...
    //

    RequestMapEntry *entry =
        NS_STATIC_CAST(RequestMapEntry *,
                       PL_DHashTableOperate(&mRequests, request,
                                        PL_DHASH_ADD));

    if (!entry) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mPriority != 0)
        RescheduleRequest(request, mPriority);

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

                PL_DHashTableOperate(&mRequests, request, PL_DHASH_REMOVE);

                rv = NS_OK;

                mForegroundCount -= 1;
            }
        }

        // Ensure that we're part of our loadgroup while pending
        if (mForegroundCount == 1 && mLoadGroup) {
            mLoadGroup->AddRequest(this, nsnull);
        }

    }

    return rv;
}

NS_IMETHODIMP
nsLoadGroup::RemoveRequest(nsIRequest *request, nsISupports* ctxt,
                           nsresult aStatus)
{
    NS_ENSURE_ARG_POINTER(request);
    nsresult rv;

#if defined(PR_LOGGING)
    {
        nsCAutoString nameStr;
        request->GetName(nameStr);
        LOG(("LOADGROUP [%x]: Removing request %x %s status %x (count=%d).\n",
            this, request, nameStr.get(), aStatus, mRequests.entryCount-1));
    }
#endif

    // Make sure we have a owning reference to the request we're about
    // to remove.

    nsCOMPtr<nsIRequest> kungFuDeathGrip(request);

    //
    // Remove the request from the group.  If this fails, it means that
    // the request was *not* in the group so do not update the foreground
    // count or it will get messed up...
    //
    RequestMapEntry *entry =
        NS_STATIC_CAST(RequestMapEntry *,
                       PL_DHashTableOperate(&mRequests, request,
                                        PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(entry)) {
        LOG(("LOADGROUP [%x]: Unable to remove request %x. Not in group!\n",
            this, request));

        return NS_ERROR_FAILURE;
    }

    PL_DHashTableRawRemove(&mRequests, entry);

    // Undo any group priority delta...
    if (mPriority != 0)
        RescheduleRequest(request, -mPriority);

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

#if defined(PR_LOGGING)
            if (NS_FAILED(rv)) {
                LOG(("LOADGROUP [%x]: OnStopRequest for request %x FAILED.\n",
                    this, request));
            }
#endif
        }

        // If that was the last request -> remove ourselves from loadgroup
        if (mForegroundCount == 0 && mLoadGroup) {
            mLoadGroup->RemoveRequest(this, nsnull, aStatus);
        }
    }

    return rv;
}

// PLDHashTable enumeration callback that appends all items in the
// hash to an nsISupportsArray.
PR_STATIC_CALLBACK(PLDHashOperator)
AppendRequestsToISupportsArray(PLDHashTable *table, PLDHashEntryHdr *hdr,
                               PRUint32 number, void *arg)
{
    RequestMapEntry *e = NS_STATIC_CAST(RequestMapEntry *, hdr);
    nsISupportsArray *array = NS_STATIC_CAST(nsISupportsArray *, arg);

    PRBool ok = array->AppendElement(e->mKey);

    if (!ok) {
        return PL_DHASH_STOP;
    }

    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsLoadGroup::GetRequests(nsISimpleEnumerator * *aRequests)
{
    nsCOMPtr<nsISupportsArray> array;
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(array));
    NS_ENSURE_SUCCESS(rv, rv);

    PL_DHashTableEnumerate(&mRequests, AppendRequestsToISupportsArray,
                           array.get());

    PRUint32 count;
    array->Count(&count);

    if (count != mRequests.entryCount) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_NewArrayEnumerator(aRequests, array);
}

NS_IMETHODIMP
nsLoadGroup::SetGroupObserver(nsIRequestObserver* aObserver)
{
    mObserver = do_GetWeakReference(aObserver);
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
// nsISupportsPriority methods:

NS_IMETHODIMP
nsLoadGroup::GetPriority(PRInt32 *aValue)
{
    *aValue = mPriority;
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetPriority(PRInt32 aValue)
{
    return AdjustPriority(aValue - mPriority);
}

NS_IMETHODIMP
nsLoadGroup::AdjustPriority(PRInt32 aDelta)
{
    // Update the priority for each request that supports nsISupportsPriority
    if (aDelta != 0) {
        mPriority += aDelta;
        PL_DHashTableEnumerate(&mRequests, RescheduleRequests, &aDelta);
    }
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
