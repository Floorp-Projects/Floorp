/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


/*

  A stack-based lock object that makes using PRLock a bit more
  convenient. It acquires the monitor when constructed, and releases
  it when it goes out of scope.

  For example,

    class Foo {
    private:
        PRLock* mLock;

    public:
        Foo(void) {
            mLock = PR_NewLock();
        }

        virtual ~Foo(void) {
            PR_DestroyLock(mLock);
        }

        void ThreadSafeMethod(void) {
            // we're don't hold the lock yet...

            nsAutoLock lock(mLock);
            // ...but now we do.

            // we even can do wacky stuff like return from arbitrary places w/o
            // worrying about forgetting to release the lock
            if (some_weird_condition)
                return;

            // otherwise do some other stuff
        }

        void ThreadSafeBlockScope(void) {
            // we're not in the lock here...

            {
                nsAutoLock lock(mLock);
                // but we are now, at least until the block scope closes
            }

            // ...now we're not in the lock anymore
        }
    };

 */

#ifndef nsAutoLock_h__
#define nsAutoLock_h__

#include "nscore.h"
#include "prlock.h"
#include "prlog.h"

// If you ever decide that you need to add a non-inline method to this
// class, be sure to change the class declaration to "class NS_BASE
// nsAutoLock".

class nsAutoLock {
private:
    PRLock* mLock;

    // Not meant to be implemented. This makes it a compiler error to
    // construct or assign an nsAutoLock object incorrectly.
    nsAutoLock(void) {}
    nsAutoLock(nsAutoLock& aLock) {}
    nsAutoLock& operator =(nsAutoLock& aLock) {
        return *this;
    }

    // Not meant to be implemented. This makes it a compiler error to
    // attempt to create an nsAutoLock object on the heap.
    static void* operator new(size_t size) {
        return nsnull;
    }
    static void operator delete(void* memory) {}

public:
    nsAutoLock(PRLock* aLock) : mLock(aLock) {
        PR_ASSERT(mLock);

        // This will assert deep in the bowels of NSPR if you attempt
        // to re-enter the lock.
        PR_Lock(mLock);
    }

    ~nsAutoLock(void) {
        PR_Unlock(mLock);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Same sort of shit here. Imagine if you will:
//
//    nsresult MyClass::MyMethod(...) {
//       nsAutoMonitor mon(this);   // or some random object as a monitor
//       ...
//       // go ahead and do deeply nested returns...
//                    return NS_ERROR_FAILURE;
//       ...
//       // or call Wait or Notify...
//       mon.Wait();
//       ...
//       // cleanup is automatic
//    }

#include "prcmon.h"

class nsAutoMonitor {
public:
    nsAutoMonitor(void* lockObject)
        : mLockObject(lockObject)
    {
        NS_ASSERTION(lockObject, "null lock object");
        PR_CEnterMonitor(mLockObject);
    }

    ~nsAutoMonitor() {
        PR_CExitMonitor(mLockObject);
    }

    PRStatus Wait(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT) {
        return PR_CWait(mLockObject, interval);
    }

    PRStatus Notify() {
        return PR_CNotify(mLockObject);
    }

    PRStatus NotifyAll() {
        return PR_CNotifyAll(mLockObject);
    }

private:
    void* mLockObject;

    // Not meant to be implemented. This makes it a compiler error to
    // construct or assign an nsAutoLock object incorrectly.
    nsAutoMonitor(void) {}
    nsAutoMonitor(nsAutoMonitor& aMon) {}
    nsAutoMonitor& operator =(nsAutoMonitor& aMon) {
        return *this;
    }

    // Not meant to be implemented. This makes it a compiler error to
    // attempt to create an nsAutoLock object on the heap.
    static void* operator new(size_t size) {
        return nsnull;
    }
    static void operator delete(void* memory) {}
};

#endif // nsAutoLock_h__

