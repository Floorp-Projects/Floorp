/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsMemoryImpl.h"
#include "prmem.h"
#include "nsAlgorithm.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsAutoLock.h"
#include "nsIThread.h"
#include "nsIEventQueueService.h"
#include "nsString.h"

#if defined(XP_WIN)
#include <windows.h>
#define NS_MEMORY_FLUSHER_THREAD
#elif defined(XP_MAC)
#include <MacMemory.h>
#define NS_MEMORY_FLUSHER_THREAD
#else
// Need to implement the nsIMemory::IsLowMemory() predicate
#undef NS_MEMORY_FLUSHER_THREAD
#endif

//----------------------------------------------------------------------

#if defined(XDEBUG_waterson)
#define NS_TEST_MEMORY_FLUSHER
#endif

/**
 * A runnable that is used to periodically check the status
 * of the system, determine if too much memory is in use,
 * and if so, trigger a "memory flush".
 */
class MemoryFlusher : public nsIRunnable
{
protected:
    nsMemoryImpl*  mMemoryImpl; // WEAK, it owns us.
    PRBool         mRunning;
    PRIntervalTime mTimeout;
    PRLock*        mLock;
    PRCondVar*     mCVar;
    
    MemoryFlusher(nsMemoryImpl* aMemoryImpl);
    virtual ~MemoryFlusher();

    enum {
        kInitialTimeout = 60 /*seconds*/
    };

public:
    /**
     * Create a memory flusher.
     * @param aResult the memory flusher
     * @param aMemoryImpl the owning nsMemoryImpl object
     * @return NS_OK if the memory flusher was created successfully
     */
    static nsresult
    Create(MemoryFlusher** aResult, nsMemoryImpl* aMemoryImpl);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    /**
     * Stop the memory flusher.
     */
    nsresult Stop();
};


MemoryFlusher::MemoryFlusher(nsMemoryImpl* aMemoryImpl)
    : mMemoryImpl(aMemoryImpl),
      mRunning(PR_FALSE),
      mTimeout(PR_SecondsToInterval(kInitialTimeout)),
      mLock(nsnull),
      mCVar(nsnull)
{
    NS_INIT_REFCNT();
}

MemoryFlusher::~MemoryFlusher()
{
    if (mLock)
        PR_DestroyLock(mLock);

    if (mCVar)
        PR_DestroyCondVar(mCVar);
}


nsresult
MemoryFlusher::Create(MemoryFlusher** aResult, nsMemoryImpl* aMemoryImpl)
{
    MemoryFlusher* result = new MemoryFlusher(aMemoryImpl);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    do {
        if ((result->mLock = PR_NewLock()) == nsnull)
            break;
        
        if ((result->mCVar = PR_NewCondVar(result->mLock)) == nsnull)
            break;

        NS_ADDREF(*aResult = result);
        return NS_OK;
    } while (0);

    // Something bad happened if we get here...
    delete result;
    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(MemoryFlusher, nsIRunnable)

NS_IMETHODIMP
MemoryFlusher::Run()
{
    nsresult rv;

    mRunning = PR_TRUE;

    while (1) {
        PRStatus status;

        {
            nsAutoLock l(mLock);
            if (! mRunning) {
                rv = NS_OK;
                break;
            }

            status = PR_WaitCondVar(mCVar, mTimeout);
        }

        if (status != PR_SUCCESS) {
            rv = NS_ERROR_FAILURE;
            break;
        }

        if (! mRunning) {
            rv = NS_OK;
            break;
        }

        PRBool isLowMemory;
        rv = mMemoryImpl->IsLowMemory(&isLowMemory);
        if (NS_FAILED(rv))
            break;

#ifdef NS_TEST_MEMORY_FLUSHER
        // Fire the flusher *every* time
        isLowMemory = PR_TRUE;
#endif

        if (isLowMemory) {
            mMemoryImpl->FlushMemory(NS_LITERAL_STRING(NS_MEMORY_PRESSURE_LOW_MEMORY).get(), PR_FALSE);
        }
    }

    mRunning = PR_FALSE;

    return rv;
}


nsresult
MemoryFlusher::Stop()
{
    if (mRunning) {
        nsAutoLock l(mLock);
        mRunning = PR_FALSE;
        PR_NotifyCondVar(mCVar);
    }

    return NS_OK;
}

//----------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryImpl, nsIMemory)

NS_METHOD
nsMemoryImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsMemoryImpl* mm = new nsMemoryImpl();
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    do {
        rv = mm->QueryInterface(aIID, aInstancePtr);
        if (NS_FAILED(rv))
            break;

        rv = NS_ERROR_OUT_OF_MEMORY;

        mm->mFlushLock = PR_NewLock();
        if (! mm->mFlushLock)
            break;

        rv = NS_OK;
    } while (0);

    if (NS_FAILED(rv))
        delete mm;

    return rv;
}


nsMemoryImpl::nsMemoryImpl()
    : mFlusher(nsnull),
      mFlushLock(nsnull),
      mIsFlushing(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsMemoryImpl::~nsMemoryImpl()
{
    if (mFlushLock)
        PR_DestroyLock(mFlushLock);
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

#else

#define MALLOC1(s)       PR_Malloc(s)
#define REALLOC1(p, s)   PR_Realloc(p, s)

#endif // NS_OUT_OF_MEMORY_TESTER

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP_(void *) 
nsMemoryImpl::Alloc(PRSize size)
{
    void* result = MALLOC1(size);
    if (! result) {
        // Request an asynchronous flush
        FlushMemory(NS_LITERAL_STRING(NS_MEMORY_PRESSURE_ALLOC_FAILURE).get(), PR_FALSE);
    }
    return result;
}

NS_IMETHODIMP_(void *)
nsMemoryImpl::Realloc(void * ptr, PRSize size)
{
    void* result = REALLOC1(ptr, size);
    if (! result) {
        // Request an asynchronous flush
        FlushMemory(NS_LITERAL_STRING(NS_MEMORY_PRESSURE_ALLOC_FAILURE).get(), PR_FALSE);
    }
    return result;
}

NS_IMETHODIMP_(void)
nsMemoryImpl::Free(void * ptr)
{
    PR_Free(ptr);
}

NS_IMETHODIMP
nsMemoryImpl::HeapMinimize(PRBool aImmediate)
{
    return FlushMemory(NS_LITERAL_STRING(NS_MEMORY_PRESSURE_HEAP_MINIMIZE).get(), aImmediate);
}

NS_IMETHODIMP
nsMemoryImpl::IsLowMemory(PRBool *result)
{
#if defined(XP_WIN)
    MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
    *result = ((float)stat.dwAvailPageFile / stat.dwTotalPageFile) < 0.1;
#elif defined(XP_MAC)

  const long kReserveHeapFreeSpace = (256 * 1024);
  const long kReserveHeapContigSpace = (128 * 1024);

  long totalSpace, contiguousSpace;
  // this call measures how much memory would be available if the OS
  // purged. Despite the name, it does not purge (that happens
  // automatically when heap space is low).
  ::PurgeSpace(&totalSpace, &contiguousSpace);
  if (totalSpace < kReserveHeapFreeSpace || contiguousSpace < kReserveHeapContigSpace)
  {
    NS_WARNING("Found that heap mem is low");
    *result = PR_TRUE;
    return NS_OK;
  }

  // see how much temp mem is available (since our allocators allocate 1Mb chunks
  // in temp mem. We don't use TempMaxMem() (to get contig space) here, because it
  // compacts the application heap, so can be slow.
  const long kReserveTempFreeSpace = (2 * 1024 * 1024);     // 2Mb  
  long  totalTempSpace = ::TempFreeMem();  
  if (totalTempSpace < kReserveTempFreeSpace)
  {
    NS_WARNING("Found that temp mem is low");
    *result = PR_TRUE;
    return NS_OK;
  }  

  *result = PR_FALSE;
  
#else
    *result = PR_FALSE;
#endif
    return NS_OK;
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
        nsAutoLock l(mFlushLock);
        if (mIsFlushing)
            return NS_OK;

        // Well, we are now!
        mIsFlushing = PR_TRUE;
    }

    // Run the flushers immediately if we can; otherwise, proxy to the
    // UI thread an run 'em asynchronously.
    if (aImmediate) {
        rv = RunFlushers(this, aReason);
    }
    else {
        nsCOMPtr<nsIEventQueueService> eqs = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
        if (eqs) {
            nsCOMPtr<nsIEventQueue> eq;
            rv = eqs->GetThreadEventQueue(NS_UI_THREAD, getter_AddRefs(eq));
            if (NS_SUCCEEDED(rv)) {
                PL_InitEvent(&mFlushEvent.mEvent, this, HandleFlushEvent, DestroyFlushEvent);
                mFlushEvent.mReason = aReason;

                rv = eq->PostEvent(NS_REINTERPRET_CAST(PLEvent*, &mFlushEvent));
            }
        }
    }

    return rv;
}

nsresult
nsMemoryImpl::RunFlushers(nsMemoryImpl* aSelf, const PRUnichar* aReason)
{
    nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
    if (os) {
        os->NotifyObservers(aSelf, NS_MEMORY_PRESSURE_TOPIC, aReason);
    }

    {
        // Done flushing
        nsAutoLock l(aSelf->mFlushLock);
        aSelf->mIsFlushing = PR_FALSE;
    }

    return NS_OK;
}

void*
nsMemoryImpl::HandleFlushEvent(PLEvent* aEvent)
{
    nsMemoryImpl* self = NS_STATIC_CAST(nsMemoryImpl*, PL_GetEventOwner(aEvent));
    FlushEvent* event = NS_REINTERPRET_CAST(FlushEvent*, aEvent);

    RunFlushers(self, event->mReason);
    return 0;
}

void
nsMemoryImpl::DestroyFlushEvent(PLEvent* aEvent)
{
    // no-op, since mEvent is a member of nsMemoryImpl
}

nsMemoryImpl* gMemory = nsnull;

static void
EnsureGlobalMemoryService()
{
    if (gMemory) return;
    nsresult rv = nsMemoryImpl::Create(nsnull, NS_GET_IID(nsIMemory), (void**)&gMemory);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsMemoryImpl::Create failed");
    NS_ASSERTION(gMemory, "improper xpcom initialization");
}

nsresult 
nsMemoryImpl::Startup()
{
    EnsureGlobalMemoryService();
    if (! gMemory)
        return NS_ERROR_FAILURE;

#ifdef NS_MEMORY_FLUSHER_THREAD
    nsresult rv;

    // Create and start a memory flusher thread
    rv = MemoryFlusher::Create(&gMemory->mFlusher, gMemory);
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewThread(getter_AddRefs(gMemory->mFlusherThread),
                      gMemory->mFlusher,
                      0, /* XXX use default stack size? */
                      PR_JOINABLE_THREAD);

    if (NS_FAILED(rv)) return rv;
#endif

    return NS_OK;
}

nsresult 
nsMemoryImpl::Shutdown()
{
    if (gMemory) {
#ifdef NS_MEMORY_FLUSHER_THREAD
        if (gMemory->mFlusher) {
            // Stop the runnable...
            gMemory->mFlusher->Stop();
            NS_RELEASE(gMemory->mFlusher);

            // ...and wait for the thread to exit
            if (gMemory->mFlusherThread)
                gMemory->mFlusherThread->Join();
        }
#endif

        NS_RELEASE(gMemory);
        gMemory = nsnull;
    }

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsMemory static helper routines

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
nsMemory::HeapMinimize(PRBool aImmediate)
{
    if (gMemory == nsnull) {
        EnsureGlobalMemoryService();
    }
    return gMemory->HeapMinimize(aImmediate);
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

//----------------------------------------------------------------------

