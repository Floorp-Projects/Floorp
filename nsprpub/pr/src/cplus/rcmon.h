/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
