/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMemoryImpl.h"
#include "nsThreadUtils.h"

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"

#include "nsCOMPtr.h"
#include "mozilla/Services.h"

#ifdef ANDROID
#include <stdio.h>

// Minimum memory threshold for a device to be considered
// a low memory platform. This value has be in sync with
// Java's equivalent threshold, defined in
// mobile/android/base/util/HardwareUtils.java
#define LOW_MEMORY_THRESHOLD_KB (384 * 1024)
#endif

static nsMemoryImpl sGlobalMemory;

NS_IMPL_QUERY_INTERFACE1(nsMemoryImpl, nsIMemory)

NS_IMETHODIMP_(void*)
nsMemoryImpl::Alloc(size_t size)
{
    return NS_Alloc(size);
}

NS_IMETHODIMP_(void*)
nsMemoryImpl::Realloc(void* ptr, size_t size)
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
    return FlushMemory(MOZ_UTF16("heap-minimize"), aImmediate);
}

NS_IMETHODIMP
nsMemoryImpl::IsLowMemory(bool *result)
{
    NS_ERROR("IsLowMemory is deprecated.  See bug 592308.");
    *result = false;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryImpl::IsLowMemoryPlatform(bool *result)
{
#ifdef ANDROID
    static int sLowMemory = -1; // initialize to unknown, lazily evaluate to 0 or 1
    if (sLowMemory == -1) {
        sLowMemory = 0; // assume "not low memory" in case file operations fail
        *result = false;

        // check if MemTotal from /proc/meminfo is less than LOW_MEMORY_THRESHOLD_KB
        FILE* fd = fopen("/proc/meminfo", "r");
        if (!fd) {
            return NS_OK;
        }
        uint64_t mem = 0;
        int rv = fscanf(fd, "MemTotal: %llu kB", &mem);
        if (fclose(fd)) {
            return NS_OK;
        }
        if (rv != 1) {
            return NS_OK;
        }
        sLowMemory = (mem < LOW_MEMORY_THRESHOLD_KB) ? 1 : 0;
    }
    *result = (sLowMemory == 1);
#else
    *result = false;
#endif
    return NS_OK;
}

/*static*/ nsresult
nsMemoryImpl::Create(nsISupports* outer, const nsIID& aIID, void **aResult)
{
    if (NS_WARN_IF(outer))
        return NS_ERROR_NO_AGGREGATION;
    return sGlobalMemory.QueryInterface(aIID, aResult);
}

nsresult
nsMemoryImpl::FlushMemory(const char16_t* aReason, bool aImmediate)
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

    bool lastVal = sIsFlushing.exchange(true);
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
nsMemoryImpl::RunFlushers(const char16_t* aReason)
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
              nsCOMPtr<nsISupports> supports;
              e->GetNext(getter_AddRefs(supports));

              if (!supports)
                  continue;

              observer = do_QueryInterface(supports);
              observer->Observe(observer, "memory-pressure", aReason);
          }
        }
    }

    sIsFlushing = false;
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

mozilla::Atomic<bool>
nsMemoryImpl::sIsFlushing;

PRIntervalTime
nsMemoryImpl::sLastFlushTime = 0;

nsMemoryImpl::FlushEvent
nsMemoryImpl::sFlushEvent;

XPCOM_API(void*)
NS_Alloc(size_t size)
{
    return moz_xmalloc(size);
}

XPCOM_API(void*)
NS_Realloc(void* ptr, size_t size)
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
