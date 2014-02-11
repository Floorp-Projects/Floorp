/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMemoryImpl_h__
#define nsMemoryImpl_h__

#include "mozilla/Atomics.h"

#include "nsIMemory.h"
#include "nsIRunnable.h"

// nsMemoryImpl is a static object. We can do this because it doesn't have
// a constructor/destructor or any instance members. Please don't add
// instance member variables, only static member variables.

class nsMemoryImpl : public nsIMemory
{
public:
    // We don't use the generic macros because we are a special static object
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aResult);
    NS_IMETHOD_(nsrefcnt) AddRef(void) { return 1; }
    NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

    NS_DECL_NSIMEMORY

    static nsresult Create(nsISupports* outer,
                           const nsIID& aIID, void **aResult);

    NS_HIDDEN_(nsresult) FlushMemory(const char16_t* aReason, bool aImmediate);
    NS_HIDDEN_(nsresult) RunFlushers(const char16_t* aReason);

protected:
    struct FlushEvent : public nsIRunnable {
        NS_DECL_ISUPPORTS_INHERITED
        NS_DECL_NSIRUNNABLE
        const char16_t* mReason;
    };

    static mozilla::Atomic<bool> sIsFlushing;
    static FlushEvent sFlushEvent;
    static PRIntervalTime sLastFlushTime;
};

#endif // nsMemoryImpl_h__
