/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
