/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Class: RCMonitor (ref prmonitor.h)
**
** RCMonitor.h - C++ wrapper around NSPR's monitors
*/
#if defined(_RCMONITOR_H)
#else
#define _RCMONITOR_H

#include "rcbase.h"
#include "rcinrval.h"

struct PRMonitor;

class PR_IMPLEMENT(RCMonitor): public RCBase
{
public:
    RCMonitor();                    /* timeout is infinity */
    virtual ~RCMonitor();

    virtual void Enter();           /* reentrant entry */
    virtual void Exit();

    virtual void Notify();          /* possibly enable one thread */
    virtual void NotifyAll();       /* enable all waiters */

    virtual void Wait();            /* applies object's timeout */

    virtual void SetTimeout(const RCInterval& timeout);

private:
    PRMonitor *monitor;
    RCInterval timeout;

public:
    RCInterval GetTimeout() const;  /* get the current value */

};  /* RCMonitor */

#endif /* defined(_RCMONITOR_H) */

/* RCMonitor.h */
