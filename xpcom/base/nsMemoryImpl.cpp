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

#include "nsIEventQueueService.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIThread.h"

#include "prmem.h"
#include "plevent.h"

#include "nsAlgorithm.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#if defined(XP_WIN)
#include <windows.h>
#define NS_MEMORY_FLUSHER_THREAD
#else
// Need to implement the nsIMemory::IsLowMemory() predicate
#undef NS_MEMORY_FLUSHER_THREAD
#endif

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

#else

#define MALLOC1(s)       PR_Malloc(s)
#define REALLOC1(p, s)   PR_Realloc(p, s)

#endif // NS_OUT_OF_MEMORY_TESTER

#if defined(XDEBUG_waterson)
#define NS_TEST_MEMORY_FLUSHER
#endif

#ifdef NS_MEMORY_FLUSHER_THREAD
/**
 * A runnable that is used to periodically check the status
 * of the system, determine if too much memory is in use,
 * and if so, trigger a "memory flush".
 */

class MemoryFlusher : public nsIRunnable
{
public:
    // We don't use the generic macros because we are a special static object
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aResult);
    NS_IMETHOD_(nsrefcnt) AddRef(void) { return 1; }
    NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

    NS_DECL_NSIRUNNABLE

    nsresult Init();
    void StopAndJoin();

private:
    static PRBool         sRunning;
    static PRIntervalTime sTimeout;
    static PRLock*        sLock;
    static PRCondVar*     sCVar;
    static nsIThread*     sThread;
    
    enum {
        kInitialTimeout = 60 /*seconds*/
    };
};

static MemoryFlusher sGlobalMemoryFlusher;

#endif // NS_MEMORY_FLUSHER_THREAD

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
nsMemoryImpl::HeapMinimize(PRBool aImmediate)
{
    return FlushMemory(NS_LITERAL_STRING("heap-minimize").get(), aImmediate);
}

NS_IMETHODIMP
nsMemoryImpl::IsLowMemory(PRBool *result)
{
#if defined(WINCE)
    MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
    *result = ((float)stat.dwAvailPhys / stat.dwTotalPhys) < 0.1;
#elif defined(XP_WIN)
    MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
    *result = ((float)stat.dwAvailPageFile / stat.dwTotalPageFile) < 0.1;
#else
    *result = PR_FALSE;
#endif
    return NS_OK;
}


nsresult 
nsMemoryImpl::Startup()
{
    sFlushLock = PR_NewLock();
    if (!sFlushLock) return NS_ERROR_FAILURE;

#ifdef NS_MEMORY_FLUSHER_THREAD
    return sGlobalMemoryFlusher.Init();
#else
    return NS_OK;
#endif
}

void
nsMemoryImpl::Shutdown()
{
#ifdef NS_MEMORY_FLUSHER_THREAD
    sGlobalMemoryFlusher.StopAndJoin();
#endif

    if (sFlushLock) PR_DestroyLock(sFlushLock);
    sFlushLock = nsnull;
}

nsresult
nsMemoryImpl::Create(nsISupports* outer, const nsIID& aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(outer);
    return sGlobalMemory.QueryInterface(aIID, aResult);
}

nsresult
nsMemoryImpl::FlushMemory(const PRUnichar* aReason, PRBool aImmediate)
{
    nsresult rv;

    if (aImmediate) {
        // They've asked us to run the flusher *immediately*. We've
        // got to be on the UI main thread for us to be able to do
        // that...are we?
        PRBool isOnUIThread = PR_FALSE;

        nsCOMPtr<nsIThread> main;
        rv = nsIThread::GetMainThread(getter_AddRefs(main));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIThread> current;
            rv = nsIThread::GetCurrent(getter_AddRefs(current));
            if (NS_SUCCEEDED(rv)) {
                if (current == main)
                    isOnUIThread = PR_TRUE;
            }
        }

        if (! isOnUIThread) {
            NS_ERROR("can't synchronously flush memory: not on UI thread");
            return NS_ERROR_FAILURE;
        }
    }

    {
        // Are we already flushing?
        nsAutoLock l(sFlushLock);
        if (sIsFlushing)
            return NS_OK;

        // Well, we are now!
        sIsFlushing = PR_TRUE;
    }

    // Run the flushers immediately if we can; otherwise, proxy to the
    // UI thread an run 'em asynchronously.
    if (aImmediate) {
        rv = RunFlushers(aReason);
    }
    else {
        nsCOMPtr<nsIEventQueueService> eqs = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
        if (eqs) {
            nsCOMPtr<nsIEventQueue> eq;
            rv = eqs->GetThreadEventQueue(NS_UI_THREAD, getter_AddRefs(eq));
            if (NS_SUCCEEDED(rv)) {
                PL_InitEvent(&sFlushEvent.mEvent, this, HandleFlushEvent, DestroyFlushEvent);
                sFlushEvent.mReason = aReason;

                rv = eq->PostEvent(NS_REINTERPRET_CAST(PLEvent*, &sFlushEvent));
            }
        }
    }

    return rv;
}

nsresult
nsMemoryImpl::RunFlushers(const PRUnichar* aReason)
{
    nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
    if (os) {
        os->NotifyObservers(this, "memory-pressure", aReason);
    }

    {
        // Done flushing
        nsAutoLock l(sFlushLock);
        sIsFlushing = PR_FALSE;
    }

    return NS_OK;
}

void*
nsMemoryImpl::HandleFlushEvent(PLEvent* aEvent)
{
    FlushEvent* event = NS_REINTERPRET_CAST(FlushEvent*, aEvent);

    sGlobalMemory.RunFlushers(event->mReason);
    return 0;
}

void
nsMemoryImpl::DestroyFlushEvent(PLEvent* aEvent)
{
    // no-op, since mEvent is a member of nsMemoryImpl
}

PRLock*
nsMemoryImpl::sFlushLock;

PRBool
nsMemoryImpl::sIsFlushing = PR_FALSE;

nsMemoryImpl::FlushEvent
nsMemoryImpl::sFlushEvent;

XPCOM_API(void*)
NS_Alloc(PRSize size)
{
    void* result = MALLOC1(size);
    if (! result) {
        // Request an asynchronous flush
        sGlobalMemory.FlushMemory(NS_LITERAL_STRING("alloc-failure").get(), PR_FALSE);
    }
    return result;
}

XPCOM_API(void*)
NS_Realloc(void* ptr, PRSize size)
{
    void* result = REALLOC1(ptr, size);
    if (! result && size != 0) {
        // Request an asynchronous flush
        sGlobalMemory.FlushMemory(NS_LITERAL_STRING("alloc-failure").get(), PR_FALSE);
    }
    return result;
}

XPCOM_API(void)
NS_Free(void* ptr)
{
    PR_Free(ptr);
}

#ifdef NS_MEMORY_FLUSHER_THREAD

NS_IMPL_QUERY_INTERFACE1(MemoryFlusher, nsIRunnable)

NS_IMETHODIMP
MemoryFlusher::Run()
{
    nsresult rv;

    sRunning = PR_TRUE;

    while (1) {
        PRStatus status;

        {
            nsAutoLock l(sLock);
            if (! sRunning) {
                rv = NS_OK;
                break;
            }

            status = PR_WaitCondVar(sCVar, sTimeout);
        }

        if (status != PR_SUCCESS) {
            rv = NS_ERROR_FAILURE;
            break;
        }

        if (! sRunning) {
            rv = NS_OK;
            break;
        }

        PRBool isLowMemory;
        rv = sGlobalMemory.IsLowMemory(&isLowMemory);
        if (NS_FAILED(rv))
            break;

#ifdef NS_TEST_MEMORY_FLUSHER
        // Fire the flusher *every* time
        isLowMemory = PR_TRUE;
#endif

        if (isLowMemory) {
            sGlobalMemory.FlushMemory(NS_LITERAL_STRING("low-memory").get(), PR_FALSE);
        }
    }

    sRunning = PR_FALSE;

    return rv;
}

nsresult
MemoryFlusher::Init()
{
    sTimeout = PR_SecondsToInterval(kInitialTimeout);
    sLock = PR_NewLock();
    if (!sLock) return NS_ERROR_FAILURE;

    sCVar = PR_NewCondVar(sLock);
    if (!sCVar) return NS_ERROR_FAILURE;

    return NS_NewThread(&sThread,
                        this,
                        0, /* XXX use default stack size? */
                        PR_JOINABLE_THREAD);
}

void
MemoryFlusher::StopAndJoin()
{
    if (sRunning) {
        {
            nsAutoLock l(sLock);
            sRunning = PR_FALSE;
            PR_NotifyCondVar(sCVar);
        }

        if (sThread)
            sThread->Join();
    }

    NS_IF_RELEASE(sThread);

    if (sLock)
        PR_DestroyLock(sLock);

    if (sCVar)
        PR_DestroyCondVar(sCVar);
}


PRBool
MemoryFlusher::sRunning;

PRIntervalTime
MemoryFlusher::sTimeout;

PRLock*
MemoryFlusher::sLock;

PRCondVar*
MemoryFlusher::sCVar;

nsIThread*
MemoryFlusher::sThread;

#endif // NS_MEMORY_FLUSHER_THREAD

nsresult
NS_GetMemoryManager(nsIMemory* *result)
{
    return sGlobalMemory.QueryInterface(NS_GET_IID(nsIMemory), (void**) result);
}
