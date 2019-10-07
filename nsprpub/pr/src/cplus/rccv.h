/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** RCCondition - C++ wrapper around NSPR's PRCondVar
**
** Conditions have a notion of timeouts. A thread that waits on a condition
** will resume execution when the condition is notified OR when a specified
** interval of time has expired.
**
** Most applications don't adjust the timeouts on conditions. The literature
** would argue that all threads waiting on a single condition must have the
** same semantics. But if an application wishes to modify the timeout with
** (perhaps) each wait call, that modification should be done consistantly
** and under protection of the condition's associated lock.
**
** The default timeout is infinity.
*/

#if defined(_RCCOND_H)
#else
#define _RCCOND_H

#include "rclock.h"
#include "rcbase.h"
#include "rcinrval.h"

struct PRCondVar;

class PR_IMPLEMENT(RCCondition): public RCBase
{
public:
    RCCondition(RCLock*);           /* associates CV with a lock and infinite tmo */
    virtual ~RCCondition();

    virtual PRStatus Wait();        /* applies object's current timeout */

    virtual PRStatus Notify();      /* perhaps ready one thread */
    virtual PRStatus Broadcast();   /* perhaps ready many threads */

    virtual PRStatus SetTimeout(const RCInterval&);
    /* set object's current timeout value */

private:
    PRCondVar *cv;
    RCInterval timeout;

    RCCondition();
    RCCondition(const RCCondition&);
    void operator=(const RCCondition&);

public:
    RCInterval GetTimeout() const;
};  /* RCCondition */

inline RCCondition::RCCondition(): RCBase() { }
inline RCCondition::RCCondition(const RCCondition&): RCBase() { }
inline void RCCondition::operator=(const RCCondition&) { }

#endif /* defined(_RCCOND_H) */

/* RCCond.h */
