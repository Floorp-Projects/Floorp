/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsIMemoryReporter.h"

//#define OLD_API

#define PASS()                                  \
    do {                                        \
        passed(__FUNCTION__);                   \
        return NS_OK;                           \
    } while (0)

#define FAIL(why)                               \
    do {                                        \
        fail("%s | %s - %s", __FILE__, __FUNCTION__, why); \
        return NS_ERROR_FAILURE;                \
    } while (0)

#ifdef OLD_API
#  include "nsAutoLock.h"
   typedef PRLock* moz_lock_t;
#  define NEWLOCK(n) nsAutoLock::NewLock(n)
#  define DELETELOCK(v) nsAutoLock::DestroyLock(v)
#  define AUTOLOCK(v, l) nsAutoLock v(l)
#else
#  include "mozilla/Mutex.h"
   typedef mozilla::Mutex* moz_lock_t;
#  define NEWLOCK(n) new mozilla::Mutex(n)
#  define DELETELOCK(v) delete (v)
#  define AUTOLOCK(v, l) mozilla::MutexAutoLock v(*l)
#endif

// def/undef these to run particular tests.
#undef DD_TEST1
#undef DD_TEST2
#undef DD_TEST3
#undef DD_TEST4

//-----------------------------------------------------------------------------

#ifdef DD_TEST1

static void
AllocLockRecurseUnlockFree(int i)
{
    if (0 == i)
        return;

    moz_lock_t lock = NEWLOCK("deadlockDetector.scalability.t1");
    {
        AUTOLOCK(_, lock);
        AllocLockRecurseUnlockFree(i - 1);
    }
    DELETELOCK(lock);
}

// This test creates a resource dependency chain N elements long, then
// frees all the resources in the chain.
static nsresult
LengthNDepChain(int N)
{
    AllocLockRecurseUnlockFree(N);
    PASS();
}

#endif

//-----------------------------------------------------------------------------

#ifdef DD_TEST2

// This test creates a single lock that is ordered < N resources, then
// repeatedly exercises this order k times.
static nsresult
OneLockNDeps(const int N, const int K)
{
    moz_lock_t lock = NEWLOCK("deadlockDetector.scalability.t2.master");
    moz_lock_t* locks = new moz_lock_t[N];
    if (!locks)
        NS_RUNTIMEABORT("couldn't allocate lock array");

    for (int i = 0; i < N; ++i)
        locks[i] =
            NEWLOCK("deadlockDetector.scalability.t2.dep");

    // establish orders
    {AUTOLOCK(m, lock);
        for (int i = 0; i < N; ++i)
            AUTOLOCK(s, locks[i]);
    }

    // exercise order check
    {AUTOLOCK(m, lock);
        for (int i = 0; i < K; ++i)
            for (int j = 0; j < N; ++j)
                AUTOLOCK(s, locks[i]);
    }

    for (int i = 0; i < N; ++i)
        DELETELOCK(locks[i]);
    delete[] locks;

    PASS();
}

#endif

//-----------------------------------------------------------------------------

#ifdef DD_TEST3

// This test creates N resources and adds the theoretical maximum number
// of dependencies, O(N^2).  It then repeats that sequence of
// acquisitions k times.  Finally, all resources are freed.
//
// It's very difficult to perform well on this test.  It's put forth as a
// challenge problem.

static nsresult
MaxDepsNsq(const int N, const int K)
{
    moz_lock_t* locks = new moz_lock_t[N];
    if (!locks)
        NS_RUNTIMEABORT("couldn't allocate lock array");

    for (int i = 0; i < N; ++i)
        locks[i] = NEWLOCK("deadlockDetector.scalability.t3");

    for (int i = 0; i < N; ++i) {
        AUTOLOCK(al1, locks[i]);
        for (int j = i+1; j < N; ++j)
            AUTOLOCK(al2, locks[j]);
    }

    for (int i = 0; i < K; ++i) {
        for (int j = 0; j < N; ++j) {
            AUTOLOCK(al1, locks[j]);
            for (int k = j+1; k < N; ++k)
                AUTOLOCK(al2, locks[k]);
        }
    }

    for (int i = 0; i < N; ++i)
        DELETELOCK(locks[i]);
    delete[] locks;

    PASS();
}

#endif

//-----------------------------------------------------------------------------

#ifdef DD_TEST4

// This test creates a single lock that is ordered < N resources. The
// resources are allocated, exercised K times, and deallocated one at
// a time.

static nsresult
OneLockNDepsUsedSeveralTimes(const size_t N, const size_t K)
{
    // Create master lock.
    moz_lock_t lock_1 = NEWLOCK("deadlockDetector.scalability.t4.master");
    for (size_t n = 0; n < N; n++) {
        // Create child lock.
        moz_lock_t lock_2 = NEWLOCK("deadlockDetector.scalability.t4.child");

        // First lock the master.
        AUTOLOCK(m, lock_1);

        // Now lock and unlock the child a few times.
        for (size_t k = 0; k < K; k++) {
            AUTOLOCK(c, lock_2);
        }

        // Destroy the child lock.
        DELETELOCK(lock_2);
    }

    // Cleanup the master lock.
    DELETELOCK(lock_1);

    PASS();
}

#endif

//-----------------------------------------------------------------------------

MOZ_DEFINE_MALLOC_SIZE_OF(DeadlockDetectorMallocSizeOf)

int
main(int argc, char** argv)
{
    ScopedXPCOM xpcom("Deadlock detector scalability (" __FILE__ ")");
    if (xpcom.failed())
        return 1;

    int rv = 0;

    // Uncomment these tests to run them.  Not expected to be common.

#ifndef DD_TEST1
    puts("Skipping not-requested LengthNDepChain() test");
#else
    if (NS_FAILED(LengthNDepChain(1 << 14))) // 16K
        rv = 1;
#endif

#ifndef DD_TEST2
    puts("Skipping not-requested OneLockNDeps() test");
#else
    // NB: Using a larger test size to stress our traversal logic.
    if (NS_FAILED(OneLockNDeps(1 << 17, 100))) // 131k
        rv = 1;
#endif

#ifndef DD_TEST3
    puts("Skipping not-requested MaxDepsNsq() test");
#else
    if (NS_FAILED(MaxDepsNsq(1 << 10, 10))) // 1k
        rv = 1;
#endif

#ifndef DD_TEST4
    puts("Skipping not-requested OneLockNDepsUsedSeveralTimes() test");
#else
    if (NS_FAILED(OneLockNDepsUsedSeveralTimes(1 << 17, 3))) // 131k
        rv = 1;
#endif

    size_t memory_used = mozilla::BlockingResourceBase::SizeOfDeadlockDetector(
        DeadlockDetectorMallocSizeOf);
    printf_stderr("Used %d bytes\n", (int)memory_used);

    return rv;
}
