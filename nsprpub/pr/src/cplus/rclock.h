/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** C++ access to NSPR locks (PRLock)
*/

#if defined(_RCLOCK_H)
#else
#define _RCLOCK_H

#include "rcbase.h"

#include <prlock.h>

class PR_IMPLEMENT(RCLock): public RCBase
{
public:
    RCLock();
    virtual ~RCLock();

    void Acquire();                 /* non-reentrant */
    void Release();                 /* should be by owning thread */

    friend class RCCondition;

private:
    RCLock(const RCLock&);          /* can't do that */
    void operator=(const RCLock&);  /* nor that */

    PRLock *lock;
};  /* RCLock */

/*
** Class: RCEnter
**
** In scope locks. You can only allocate them on the stack. The language
** will insure that they get released (by calling the destructor) when
** the thread leaves scope, even if via an exception.
*/
class PR_IMPLEMENT(RCEnter)
{
public:
    ~RCEnter();                     /* releases the lock */
    RCEnter(RCLock*);               /* acquires the lock */

private:
    RCLock *lock;

    RCEnter();
    RCEnter(const RCEnter&);
    void operator=(const RCEnter&);

    void *operator new(PRSize) {
        return NULL;
    }
    void operator delete(void*) { }
};  /* RCEnter */


inline RCEnter::RCEnter(RCLock* ml) {
    lock = ml;
    lock->Acquire();
}
inline RCEnter::~RCEnter() {
    lock->Release();
    lock = NULL;
}

#endif /* defined(_RCLOCK_H) */

/* RCLock.h */
