/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Chris Jones <jones.chris.g@gmail.com>
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

#include "TestHarness.h"

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
    if (NS_FAILED(OneLockNDeps(1 << 14, 100))) // 16k
        rv = 1;
#endif

#ifndef DD_TEST3
    puts("Skipping not-requested MaxDepsNsq() test");
#else
    if (NS_FAILED(MaxDepsNsq(1 << 10, 10))) // 1k
        rv = 1;
#endif

    return rv;
}
