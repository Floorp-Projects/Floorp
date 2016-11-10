/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Avoid DMD-specific parts of MOZ_DEFINE_MALLOC_SIZE_OF
#undef MOZ_DMD

#include "nsIMemoryReporter.h"
#include "mozilla/Mutex.h"

#include "gtest/gtest.h"

//-----------------------------------------------------------------------------

static void
AllocLockRecurseUnlockFree(int i)
{
    if (0 == i)
        return;

    mozilla::Mutex* lock = new mozilla::Mutex("deadlockDetector.scalability.t1");
    {
        mozilla::MutexAutoLock _(*lock);
        AllocLockRecurseUnlockFree(i - 1);
    }
    delete lock;
}

// This test creates a resource dependency chain N elements long, then
// frees all the resources in the chain.
TEST(DeadlockDetectorScalability, LengthNDepChain)
{
    const int N = 1 << 14; // 16K
    AllocLockRecurseUnlockFree(N);
    ASSERT_TRUE(true);
}

//-----------------------------------------------------------------------------

// This test creates a single lock that is ordered < N resources, then
// repeatedly exercises this order k times.
//
// NB: It takes a minute or two to run so it is disabled by default.
TEST(DeadlockDetectorScalability, DISABLED_OneLockNDeps)
{
    // NB: Using a larger test size to stress our traversal logic.
    const int N = 1 << 17; // 131k
    const int K = 100;

    mozilla::Mutex* lock = new mozilla::Mutex("deadlockDetector.scalability.t2.master");
    mozilla::Mutex** locks = new mozilla::Mutex*[N];
    if (!locks)
        NS_RUNTIMEABORT("couldn't allocate lock array");

    for (int i = 0; i < N; ++i)
        locks[i] =
            new mozilla::Mutex("deadlockDetector.scalability.t2.dep");

    // establish orders
    {mozilla::MutexAutoLock m(*lock);
        for (int i = 0; i < N; ++i)
            mozilla::MutexAutoLock s(*locks[i]);
    }

    // exercise order check
    {mozilla::MutexAutoLock m(*lock);
        for (int i = 0; i < K; ++i)
            for (int j = 0; j < N; ++j)
                mozilla::MutexAutoLock s(*locks[i]);
    }

    for (int i = 0; i < N; ++i)
        delete locks[i];
    delete[] locks;

    ASSERT_TRUE(true);
}

//-----------------------------------------------------------------------------

// This test creates N resources and adds the theoretical maximum number
// of dependencies, O(N^2).  It then repeats that sequence of
// acquisitions k times.  Finally, all resources are freed.
//
// It's very difficult to perform well on this test.  It's put forth as a
// challenge problem.

TEST(DeadlockDetectorScalability, MaxDepsNsq)
{
    const int N = 1 << 10; // 1k
    const int K = 10;

    mozilla::Mutex** locks = new mozilla::Mutex*[N];
    if (!locks)
        NS_RUNTIMEABORT("couldn't allocate lock array");

    for (int i = 0; i < N; ++i)
        locks[i] = new mozilla::Mutex("deadlockDetector.scalability.t3");

    for (int i = 0; i < N; ++i) {
        mozilla::MutexAutoLock al1(*locks[i]);
        for (int j = i+1; j < N; ++j)
            mozilla::MutexAutoLock al2(*locks[j]);
    }

    for (int i = 0; i < K; ++i) {
        for (int j = 0; j < N; ++j) {
            mozilla::MutexAutoLock al1(*locks[j]);
            for (int k = j+1; k < N; ++k)
                mozilla::MutexAutoLock al2(*locks[k]);
        }
    }

    for (int i = 0; i < N; ++i)
        delete locks[i];
    delete[] locks;

    ASSERT_TRUE(true);
}

//-----------------------------------------------------------------------------

// This test creates a single lock that is ordered < N resources. The
// resources are allocated, exercised K times, and deallocated one at
// a time.

TEST(DeadlockDetectorScalability, OneLockNDepsUsedSeveralTimes)
{
    const size_t N = 1 << 17; // 131k
    const size_t K = 3;

    // Create master lock.
    mozilla::Mutex* lock_1 = new mozilla::Mutex("deadlockDetector.scalability.t4.master");
    for (size_t n = 0; n < N; n++) {
        // Create child lock.
        mozilla::Mutex* lock_2 = new mozilla::Mutex("deadlockDetector.scalability.t4.child");

        // First lock the master.
        mozilla::MutexAutoLock m(*lock_1);

        // Now lock and unlock the child a few times.
        for (size_t k = 0; k < K; k++) {
            mozilla::MutexAutoLock c(*lock_2);
        }

        // Destroy the child lock.
        delete lock_2;
    }

    // Cleanup the master lock.
    delete lock_1;

    ASSERT_TRUE(true);
}

//-----------------------------------------------------------------------------

MOZ_DEFINE_MALLOC_SIZE_OF(DeadlockDetectorMallocSizeOf)

// This is a simple test that exercises the deadlock detector memory reporting
// functionality.
TEST(DeadlockDetectorScalability, SizeOf)
{
    size_t memory_used = mozilla::BlockingResourceBase::SizeOfDeadlockDetector(
        DeadlockDetectorMallocSizeOf);

    ASSERT_GT(memory_used, size_t(0));
}
