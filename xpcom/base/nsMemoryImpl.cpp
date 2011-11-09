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

#include "nsXPCOM.h"
#include "nsMemoryImpl.h"
#include "nsThreadUtils.h"

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"

#include "prmem.h"
#include "prcvar.h"
#include "pratom.h"

#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/Services.h"

static nsMemoryImpl sGlobalMemory;

NS_IMPL_QUERY_INTERFACE1(nsMemoryImpl, nsIMemory)

NS_IMETHODIMP_(void*)
nsMemoryImpl::Alloc(PRSize size)
{
    return NS_Alloc(size);
}

NS_IMETHODIMP_(void*)
nsMemoryImpl::Realloc(void* ptr, PRSize size)
{
    return NS_Realloc(ptr, size);
}

NS_IMETHODIMP_(void)
nsMemoryImpl::Free(void* ptr)
{
    NS_Free(ptr);
}

NS_IMETHODIMP
nsMemoryImpl::HeapMinimize(bool aImmediate)
{
    return FlushMemory(NS_LITERAL_STRING("heap-minimize").get(), aImmediate);
}

NS_IMETHODIMP
nsMemoryImpl::IsLowMemory(bool *result)
{
    NS_ERROR("IsLowMemory is deprecated.  See bug 592308.");
    *result = false;
    return NS_OK;
}

/*static*/ nsresult
nsMemoryImpl::Create(nsISupports* outer, const nsIID& aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(outer);
    return sGlobalMemory.QueryInterface(aIID, aResult);
}

nsresult
nsMemoryImpl::FlushMemory(const PRUnichar* aReason, bool aImmediate)
{
    nsresult rv = NS_OK;

    if (aImmediate) {
        // They've asked us to run the flusher *immediately*. We've
        // got to be on the UI main thread for us to be able to do
        // that...are we?
        if (!NS_IsMainThread()) {
            NS_ERROR("can't synchronously flush memory: not on UI thread");
            return NS_ERROR_FAILURE;
        }
    }

    PRInt32 lastVal = PR_ATOMIC_SET(&sIsFlushing, 1);
    if (lastVal)
        return NS_OK;

    PRIntervalTime now = PR_IntervalNow();

    // Run the flushers immediately if we can; otherwise, proxy to the
    // UI thread an run 'em asynchronously.
    if (aImmediate) {
        rv = RunFlushers(aReason);
    }
    else {
        // Don't broadcast more than once every 1000ms to avoid being noisy
        if (PR_IntervalToMicroseconds(now - sLastFlushTime) > 1000) {
            sFlushEvent.mReason = aReason;
            rv = NS_DispatchToMainThread(&sFlushEvent, NS_DISPATCH_NORMAL);
        }
    }

    sLastFlushTime = now;
    return rv;
}

nsresult
nsMemoryImpl::RunFlushers(const PRUnichar* aReason)
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {

        // Instead of:
        //  os->NotifyObservers(this, "memory-pressure", aReason);
        // we are going to do this manually to see who/what is
        // deallocating.

        nsCOMPtr<nsISimpleEnumerator> e;
        os->EnumerateObservers("memory-pressure", getter_AddRefs(e));

        if ( e ) {
          nsCOMPtr<nsIObserver> observer;
          bool loop = true;

          while (NS_SUCCEEDED(e->HasMoreElements(&loop)) && loop) 
          {
              e->GetNext(getter_AddRefs(observer));

              if (!observer)
                  continue;

              observer->Observe(observer, "memory-pressure", aReason);
          }
        }
    }

    sIsFlushing = 0;
    return NS_OK;
}

// XXX need NS_IMPL_STATIC_ADDREF/RELEASE
NS_IMETHODIMP_(nsrefcnt) nsMemoryImpl::FlushEvent::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) nsMemoryImpl::FlushEvent::Release() { return 1; }
NS_IMPL_QUERY_INTERFACE1(nsMemoryImpl::FlushEvent, nsIRunnable)

NS_IMETHODIMP
nsMemoryImpl::FlushEvent::Run()
{
    sGlobalMemory.RunFlushers(mReason);
    return NS_OK;
}

PRInt32
nsMemoryImpl::sIsFlushing = 0;

PRIntervalTime
nsMemoryImpl::sLastFlushTime = 0;

nsMemoryImpl::FlushEvent
nsMemoryImpl::sFlushEvent;

XPCOM_API(void*)
NS_Alloc(PRSize size)
{
    return moz_xmalloc(size);
}

XPCOM_API(void*)
NS_Realloc(void* ptr, PRSize size)
{
    return moz_xrealloc(ptr, size);
}

XPCOM_API(void)
NS_Free(void* ptr)
{
    moz_free(ptr);
}

nsresult
NS_GetMemoryManager(nsIMemory* *result)
{
    return sGlobalMemory.QueryInterface(NS_GET_IID(nsIMemory), (void**) result);
}
