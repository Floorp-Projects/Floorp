/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsMemoryImpl.h"
#include "prmem.h"
#include "nsIServiceManager.h"

#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryImpl, nsIMemory)

NS_METHOD
nsMemoryImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsMemoryImpl* mm = new nsMemoryImpl();
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = mm->QueryInterface(aIID, aInstancePtr);
	 
    if (NS_FAILED(rv))
        delete mm;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// Define NS_OUT_OF_MEMORY_TESTER if you want to force memory failures

#ifdef DEBUG_xwarren
#define NS_OUT_OF_MEMORY_TESTER
#endif

#ifdef NS_OUT_OF_MEMORY_TESTER

// flush memory one in this number of times:
#define NS_FLUSH_FREQUENCY        100000

// fail allocation one in this number of flushes:
#define NS_FAIL_FREQUENCY         10

PRUint32 gFlushFreq = 0;
PRUint32 gFailFreq = 0;

static void*
mallocator(PRSize size, PRUint32& counter, PRUint32 max)
{
    if (counter++ >= max) {
        counter = 0;
        NS_ASSERTION(0, "about to fail allocation... watch out");
        return nsnull;
    }
    return PR_Malloc(size);
}

static void*
reallocator(void* ptr, PRSize size, PRUint32& counter, PRUint32 max)
{
    if (counter++ >= max) {
        counter = 0;
        NS_ASSERTION(0, "about to fail reallocation... watch out");
        return nsnull;
    }
    return PR_Realloc(ptr, size);
}

#define MALLOC1(s)       mallocator(s, gFlushFreq, NS_FLUSH_FREQUENCY)
#define REALLOC1(p, s)   reallocator(p, s, gFlushFreq, NS_FLUSH_FREQUENCY)
#define MALLOC2(s)       mallocator(s, gFailFreq, NS_FAIL_FREQUENCY)
#define REALLOC2(p, s)   reallocator(p, s, gFailFreq, NS_FAIL_FREQUENCY)

#else

#define MALLOC1(s)       PR_Malloc(s)
#define REALLOC1(p, s)   PR_Realloc(p, s)
#define MALLOC2(s)       PR_Malloc(s)
#define REALLOC2(p, s)   PR_Realloc(p, s)

#endif // NS_OUT_OF_MEMORY_TESTER

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP_(void *) 
nsMemoryImpl::Alloc(PRSize size)
{
    nsresult rv;
    void* result = MALLOC1(size);
    if (result == nsnull) {
        rv = FlushMemory(nsIMemoryPressureObserver::REASON_ALLOC_FAILURE, size);
        if (NS_FAILED(rv)) {
            NS_WARNING("FlushMemory failed");
        }
        else {
            result = MALLOC2(size);
        }
    }
    return result;
}

NS_IMETHODIMP_(void *)
nsMemoryImpl::Realloc(void * ptr, PRSize size)
{
    nsresult rv;
    void* result = REALLOC1(ptr, size);
    if (result == nsnull) {
        rv = FlushMemory(nsIMemoryPressureObserver::REASON_ALLOC_FAILURE, size);
        if (NS_FAILED(rv)) {
            NS_WARNING("FlushMemory failed");
        }
        else {
            result = REALLOC2(ptr, size);
        }
    }
    return result;
}

NS_IMETHODIMP_(void)
nsMemoryImpl::Free(void * ptr)
{
    PR_Free(ptr);
}

NS_IMETHODIMP
nsMemoryImpl::HeapMinimize(void)
{
    return FlushMemory(nsIMemoryPressureObserver::REASON_HEAP_MINIMIZE, 0);
}

NS_IMETHODIMP
nsMemoryImpl::RegisterObserver(nsIMemoryPressureObserver* obs)
{
    nsresult rv;
    if (mObservers.get() == nsnull) {
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv)) return rv;
    }
    return mObservers->AppendElement(obs);
}

NS_IMETHODIMP
nsMemoryImpl::UnregisterObserver(nsIMemoryPressureObserver* obs)
{
    if (!mObservers)
        return NS_OK;
    return mObservers->RemoveElement(obs);
}

NS_IMETHODIMP
nsMemoryImpl::IsLowMemory(PRBool *result)
{
#if defined(XP_PC) && !defined(XP_OS2)
    MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
    *result = ((float)stat.dwAvailPageFile / stat.dwTotalPageFile) < 0.1;
    return NS_OK;
#else
    NS_NOTREACHED("nsMemoryImpl::IsLowMemory");
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult
nsMemoryImpl::FlushMemory(PRUint32 reason, PRSize size)
{
    if (mObservers.get() == nsnull) 
        return NS_OK;

    nsresult rv;
    PRUint32 count;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRUint32 i = 0; i < count; i++) {
        nsCOMPtr<nsIMemoryPressureObserver> obs;
        rv = mObservers->GetElementAt(i, (nsISupports**)getter_AddRefs(obs));
        if (NS_FAILED(rv)) return rv;
        
        rv = obs->FlushMemory(reason, size);
        NS_ASSERTION(NS_SUCCEEDED(rv), "call to nsIMemoryPressureObserver::FlushMemory failed");
        // keep going...
    }
    return NS_OK;
}

nsIMemory* gMemory = nsnull;

nsresult 
nsMemoryImpl::Startup()
{
    return Create(nsnull, NS_GET_IID(nsIMemory), (void**)&gMemory);
}

nsresult 
nsMemoryImpl::ReleaseObservers()
{
    // set mObservers to null to release observers
    ((nsMemoryImpl*)gMemory)->mObservers = nsnull;

    return NS_OK;
}

nsresult 
nsMemoryImpl::Shutdown()
{
    NS_RELEASE(gMemory);
    gMemory = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsMemory static helper routines

static void
EnsureGlobalMemoryService()
{
    if (gMemory) return;
    nsresult rv = nsMemoryImpl::Startup();
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsMemoryImpl::Startup failed");
    NS_ASSERTION(gMemory, "improper xpcom initialization");
}

NS_EXPORT void*
nsMemory::Alloc(PRSize size)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    return gMemory->Alloc(size);
}

NS_EXPORT void*
nsMemory::Realloc(void* ptr, PRSize size)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    return gMemory->Realloc(ptr, size);
}

NS_EXPORT void
nsMemory::Free(void* ptr)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    gMemory->Free(ptr);
}

NS_EXPORT nsresult
nsMemory::HeapMinimize(void)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    return gMemory->HeapMinimize();
}

NS_EXPORT nsresult
nsMemory::RegisterObserver(nsIMemoryPressureObserver* obs)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    return gMemory->RegisterObserver(obs);
}

NS_EXPORT nsresult
nsMemory::UnregisterObserver(nsIMemoryPressureObserver* obs)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    return gMemory->UnregisterObserver(obs);
}

NS_EXPORT void*
nsMemory::Clone(const void* ptr, PRSize size)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    void* newPtr = gMemory->Alloc(size);
    if (newPtr)
        memcpy(newPtr, ptr, size);
    return newPtr;
}

NS_EXPORT nsIMemory*
nsMemory::GetGlobalMemoryService()
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    NS_ADDREF(gMemory);
    return gMemory;
}

////////////////////////////////////////////////////////////////////////////////

