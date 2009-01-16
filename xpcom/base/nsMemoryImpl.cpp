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

#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"

#include "prmem.h"
#include "prcvar.h"
#include "pratom.h"

#include "nsAlgorithm.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#if defined(XP_WIN)
#include <windows.h>
#define NS_MEMORY_FLUSHER
#elif defined (NS_OSSO)
#include <osso-mem.h>
#include <fcntl.h>
#include <unistd.h>
const char* kHighMark = "/sys/kernel/high_watermark";
#else
// Need to implement the nsIMemory::IsLowMemory() predicate
#undef NS_MEMORY_FLUSHER
#endif

#ifdef NS_MEMORY_FLUSHER
#include "nsITimer.h"
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

#ifdef NS_MEMORY_FLUSHER
/**
 * A class that is used to periodically check the status of the system,
 * determine if too much memory is in use, and if so, trigger a "memory flush".
 */
class MemoryFlusher : public nsITimerCallback
{
public:
    // We don't use the generic macros because we are a special static object
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aResult);
    NS_IMETHOD_(nsrefcnt) AddRef(void) { return 2; }
    NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

    NS_DECL_NSITIMERCALLBACK

    nsresult Init();
    void StopAndJoin();

private:
    static PRIntervalTime sTimeout;
    static PRLock*        sLock;
    static PRCondVar*     sCVar;
    
    enum {
        kTimeout = 60000 // milliseconds
    };
};

static MemoryFlusher sGlobalMemoryFlusher;

#endif // NS_MEMORY_FLUSHER

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

/* this magic number is something greater than 40mb
 * and after all, 40mb should be good enough for any web app
 * unless it's part of an office suite.
 */
static const int kRequiredMemory = 0x3000000;

NS_IMETHODIMP
nsMemoryImpl::IsLowMemory(PRBool *result)
{
#if defined(WINCE)
    MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
    *result = ((float)stat.dwAvailPhys / stat.dwTotalPhys) < 0.1;
#elif defined(XP_WIN)
    MEMORYSTATUSEX stat;
    stat.dwLength = sizeof stat;
    GlobalMemoryStatusEx(&stat);
    *result = (stat.ullAvailPageFile < kRequiredMemory) &&
        ((float)stat.ullAvailPageFile / stat.ullTotalPageFile) < 0.1;
#elif defined(NS_OSSO)
    int fd = open (kHighMark, O_RDONLY);
    if (fd == -1) {
        *result = PR_FALSE;
        return NS_OK;
    }
    int c = 0;
    read (fd, &c, 1);
    close(fd);
    *result = (c == '1');
#else
    *result = PR_FALSE;
#endif
    return NS_OK;
}


/*static*/ nsresult 
nsMemoryImpl::InitFlusher()
{
#ifdef NS_MEMORY_FLUSHER
    return sGlobalMemoryFlusher.Init();
#else
    return NS_OK;
#endif
}

/*static*/ nsresult
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
        if (!NS_IsMainThread()) {
            NS_ERROR("can't synchronously flush memory: not on UI thread");
            return NS_ERROR_FAILURE;
        }
    }

    PRInt32 lastVal = PR_AtomicSet(&sIsFlushing, 1);
    if (lastVal)
        return NS_OK;

    // Run the flushers immediately if we can; otherwise, proxy to the
    // UI thread an run 'em asynchronously.
    if (aImmediate) {
        rv = RunFlushers(aReason);
    }
    else {
        sFlushEvent.mReason = aReason;
        rv = NS_DispatchToMainThread(&sFlushEvent, NS_DISPATCH_NORMAL);
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

nsMemoryImpl::FlushEvent
nsMemoryImpl::sFlushEvent;

XPCOM_API(void*)
NS_Alloc(PRSize size)
{
    if (size > PR_INT32_MAX)
        return nsnull;

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
    if (size > PR_INT32_MAX)
        return nsnull;

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

#ifdef NS_MEMORY_FLUSHER

NS_IMPL_QUERY_INTERFACE1(MemoryFlusher, nsITimerCallback)

NS_IMETHODIMP
MemoryFlusher::Notify(nsITimer *timer)
{
    PRBool isLowMemory;
    sGlobalMemory.IsLowMemory(&isLowMemory);

#ifdef NS_TEST_MEMORY_FLUSHER
    // Fire the flusher *every* time
    isLowMemory = PR_TRUE;
#endif

    if (isLowMemory)
        sGlobalMemory.FlushMemory(NS_LITERAL_STRING("low-memory").get(),
                                  PR_FALSE);
    return NS_OK;
}

nsresult
MemoryFlusher::Init()
{
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_STATE(timer);

    // There is no need to keep a reference to the timer object because we
    // don't need to kill the timer.  It will be killed automatically when
    // XPCOM is shutdown.

    return timer->InitWithCallback(this, kTimeout,
                                   nsITimer::TYPE_REPEATING_SLACK);
}

#endif // NS_MEMORY_FLUSHER

nsresult
NS_GetMemoryManager(nsIMemory* *result)
{
    return sGlobalMemory.QueryInterface(NS_GET_IID(nsIMemory), (void**) result);
}
