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
 */

#include "nsLoadGroup.h"
#include "nsIStreamObserver.h"
#include "nsIChannel.h"
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

#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////

nsLoadGroup::nsLoadGroup(nsISupports* outer)
    : mDefaultLoadAttributes(nsIChannel::LOAD_NORMAL),
      mForegroundCount(0),
      mRequests(nsnull),
      mStatus(NS_OK)
{
    NS_INIT_AGGREGATED(outer);

#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for nsILoadGroup logging
    // if necessary...
    //
    if (nsnull == gLoadGroupLog) {
        gLoadGroupLog = PR_NewLogModule("LoadGroup");
    }
#endif /* PR_LOGGING */

    PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
           ("LOADGROUP [%x]: Created.\n", this));
}

nsLoadGroup::~nsLoadGroup()
{
    nsresult rv;

    rv = Cancel(NS_BINDING_ABORTED);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed");

    NS_IF_RELEASE(mRequests);
    mDefaultLoadRequest = null_nsCOMPtr();

    PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
           ("LOADGROUP [%x]: Destroyed.\n", this));
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

    if (NS_FAILED(rv)) {
        delete group;
    }

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
    if (mForegroundCount > 0) {
        *aResult = PR_TRUE;
    } else {
        *aResult = PR_FALSE;
    }

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
            if (!request) {
                continue;
            }

#if defined(PR_LOGGING)
            nsXPIDLString nameStr;
            request->GetName(getter_Copies(nameStr));
            PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
                   ("LOADGROUP [%x]: Canceling request %x %s.\n",
                   this, request, NS_ConvertUCS2toUTF8(nameStr).get()));
#endif /* PR_LOGGING */

            //
            // Remove the request from the load group...  This may cause
            // the OnStopRequest notification to fire...
            //
            // XXX: What should the context and error message be?
            //
            (void)RemoveRequest(request, nsnull, status, nsnull);

            // Cancel the request...
            rv = request->Cancel(status);

            // Remember the first failure and return it...
            if (NS_FAILED(rv) && NS_SUCCEEDED(firstError)) {
              firstError = rv;
            }

            NS_RELEASE(request);
        }

#if defined(DEBUG)
        (void)mRequests->Count(&count);

        NS_ASSERTION(count == 0,            "Request list is not empty.");
        NS_ASSERTION(mForegroundCount == 0, "Foreground URLs are active.");
#endif /* DEBUG */
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
        if (!request) {
            continue;
        }

#if defined(PR_LOGGING)
            nsXPIDLString nameStr;
            request->GetName(getter_Copies(nameStr));
            PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
                   ("LOADGROUP [%x]: Suspending request %x %s.\n",
                   this, request, NS_ConvertUCS2toUTF8(nameStr).get()));
#endif /* PR_LOGGING */

        // Suspend the request...
        rv = request->Suspend();

        // Remember the first failure and return it...
        if (NS_FAILED(rv) && NS_SUCCEEDED(firstError)) {
          firstError = rv;
        }

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
        if (!request) {
            continue;
        }

#if defined(PR_LOGGING)
        nsXPIDLString nameStr;
        request->GetName(getter_Copies(nameStr));
        PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
              ("LOADGROUP [%x]: Resuming request %x %s.\n",
              this, request, NS_ConvertUCS2toUTF8(nameStr).get()));
#endif /* PR_LOGGING */

        // Resume the request...
        rv = request->Resume();

        // Remember the first failure and return it...
        if (NS_FAILED(rv) && NS_SUCCEEDED(firstError)) {
          firstError = rv;
        }

        NS_RELEASE(request);
    }

    return firstError;
}

////////////////////////////////////////////////////////////////////////////////
// nsILoadGroup methods:

NS_IMETHODIMP
nsLoadGroup::Init(nsIStreamObserver *aObserver)
{
    nsresult rv;

    rv = SetGroupObserver(aObserver);

    return rv;
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
    PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
              ("LOADGROUP [%x]: Adding request %x %s (count=%d).\n",
              this, request, NS_ConvertUCS2toUTF8(nameStr).get(), count));
#endif /* PR_LOGGING */

    nsLoadFlags flags;
    rv = MergeLoadAttributes(request, flags);
    if (NS_FAILED(rv)) return rv;
    
    //
    // Add the request to the list of active requests...
    //
    // XXX this method incorrectly returns a bool
    //
    rv = mRequests->AppendElement(request) ? NS_OK : NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) return rv;

    if (!(flags & nsIChannel::LOAD_BACKGROUND)) {
        // Update the count of foreground URIs..
        mForegroundCount += 1;

        //
        // Fire the OnStartRequest notification out to the observer...
        //
        // If the notification fails then DO NOT add the request to
        // the load group.
        //
        nsCOMPtr<nsIStreamObserver> observer (do_QueryReferent(mObserver));
        if (observer) {
            PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
                   ("LOADGROUP [%x]: Firing OnStartRequest for request %x."
                   "(foreground count=%d).\n",
                   this, request, mForegroundCount));

            rv = observer->OnStartRequest(request, ctxt);
            if (NS_FAILED(rv)) {
                PR_LOG(gLoadGroupLog, PR_LOG_ERROR,
                       ("LOADGROUP [%x]: OnStartRequest for request %x FAILED.\n",
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
nsLoadGroup::RemoveRequest(nsIRequest *request, nsISupports* ctxt, 
                           nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv;

#if defined(PR_LOGGING)
        PRUint32 count = 0;
        (void)mRequests->Count(&count);
        nsXPIDLString nameStr;
        request->GetName(getter_Copies(nameStr));
        PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
              ("LOADGROUP [%x]: Removing request %x %s status %x (count=%d).\n",
              this, request, NS_ConvertUCS2toUTF8(nameStr).get(), aStatus, count-1));
#endif /* PR_LOGGING */

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
        PR_LOG(gLoadGroupLog, PR_LOG_ERROR,
               ("LOADGROUP [%x]: Unable to remove request %x. Not in group!\n",
                this, request));
        return rv;
    }

    nsLoadFlags flags;
    nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) 
      return NS_ERROR_FAILURE;

    rv = aChannel->GetLoadAttributes(&flags);
    if (NS_FAILED(rv)) return rv;

    if (!(flags & nsIChannel::LOAD_BACKGROUND)) {
        NS_ASSERTION(mForegroundCount > 0, "ForegroundCount messed up");
        mForegroundCount -= 1;

        // Fire the OnStopRequest out to the observer...
        nsCOMPtr<nsIStreamObserver> observer (do_QueryReferent(mObserver));
        if (observer) {
            PR_LOG(gLoadGroupLog, PR_LOG_DEBUG,
                   ("LOADGROUP [%x]: Firing OnStopRequest for request %x."
                    "(foreground count=%d).\n",
                    this, request, mForegroundCount));

            rv = observer->OnStopRequest(request, ctxt, aStatus, aStatusArg);
            if (NS_FAILED(rv)) {
                PR_LOG(gLoadGroupLog, PR_LOG_ERROR,
                       ("LOADGROUP [%x]: OnStopRequest for request %x FAILED.\n",
                       this, request));
            }
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
nsLoadGroup::GetGroupListenerFactory(nsILoadGroupListenerFactory * *aFactory)
{
    if (mGroupListenerFactory) {
        mGroupListenerFactory->QueryReferent(NS_GET_IID(nsILoadGroupListenerFactory), (void**)aFactory);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetGroupListenerFactory(nsILoadGroupListenerFactory *aFactory)
{
    mGroupListenerFactory = getter_AddRefs(NS_GetWeakReference(aFactory));
    return NS_OK;
}


NS_IMETHODIMP
nsLoadGroup::SetGroupObserver(nsIStreamObserver* aObserver)
{
    nsresult rv = NS_OK;

    // Release the old observer (if any...)
    mObserver = null_nsCOMPtr();
#if 0
    if (aObserver) {
        nsCOMPtr<nsIEventQueue> eventQueue;
        NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQueue));
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewAsyncStreamObserver(getter_AddRefs(mObserver),
                                       eventQueue, aObserver);
    }
#else
    //mObserver = aObserver;
    mObserver = getter_AddRefs(NS_GetWeakReference(aObserver));
#endif

    return rv;
}


NS_IMETHODIMP
nsLoadGroup::GetGroupObserver(nsIStreamObserver* *aResult)
{
  nsCOMPtr<nsIStreamObserver> observer (do_QueryReferent(mObserver));
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

////////////////////////////////////////////////////////////////////////////////

nsresult nsLoadGroup::MergeLoadAttributes(nsIRequest *aRequest, nsLoadFlags& outFlags)
{
  nsresult rv;
  nsLoadFlags flags, oldFlags;

  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(aRequest, &rv);
  if (NS_FAILED(rv)) 
      return NS_ERROR_FAILURE;

  rv = aChannel->GetLoadAttributes(&flags);
  if (NS_FAILED(rv)) 
    return rv;

  oldFlags = flags;
  //
  // Inherit the group cache validation policy (bits 12-15)
  //
  if ( !((nsIChannel::VALIDATE_NEVER            | 
          nsIChannel::VALIDATE_ALWAYS           | 
          nsIChannel::VALIDATE_ONCE_PER_SESSION | 
          nsIChannel::VALIDATE_HEURISTICALLY) & flags)) {
    flags |= (nsIChannel::VALIDATE_NEVER            |
              nsIChannel::VALIDATE_ALWAYS           |
              nsIChannel::VALIDATE_ONCE_PER_SESSION |
              nsIChannel::VALIDATE_HEURISTICALLY) & mDefaultLoadAttributes;
  }
  //
  // Inherit the group reload policy (bits 9-10)
  //
  if (!(nsIChannel::FORCE_VALIDATION & flags)) {
    flags |= (nsIChannel::FORCE_VALIDATION & mDefaultLoadAttributes);
  }

  if (!(nsIChannel::FORCE_RELOAD & flags)) {
    flags |= (nsIChannel::FORCE_RELOAD & mDefaultLoadAttributes);
  }
  //
  // Inherit the group persistent cache policy (bit 8)
  //
  if (!(nsIChannel::INHIBIT_PERSISTENT_CACHING & flags)) {
    flags |= (nsIChannel::INHIBIT_PERSISTENT_CACHING & mDefaultLoadAttributes);
  }
  //
  // Inherit the group loading policy (bit 0)
  //
  if (!(nsIChannel::LOAD_BACKGROUND & flags)) {
    flags |= (nsIChannel::LOAD_BACKGROUND & mDefaultLoadAttributes);
  }

  if (flags != oldFlags) {
    rv = aChannel->SetLoadAttributes(flags);
  }

  outFlags = flags;

  return rv;
}
