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

/* RCThread.cpp - C++ wrapper on NSPR */

#include "rcthread.h"
#include "rcinrval.h"

#include <prmem.h>
#include <prlog.h>
#include <stdio.h>
#include <prinit.h>

static RCPrimordialThread *primordial = NULL;

void nas_Root(void *arg)
{
    RCThread *him = (RCThread*)arg;
    while (RCThread::ex_unstarted == him->execution)
        (void)PR_Sleep(PR_INTERVAL_NO_TIMEOUT);  /* wait for Start() */
    him->RootFunction();  /* he gets a self reference */
    if (PR_UNJOINABLE_THREAD == PR_GetThreadState(him->identity))
        delete him;
}  /* nas_Root */

RCThread::~RCThread() { }

RCThread::RCThread(): RCBase() { }

RCThread::RCThread(const RCThread&): RCBase()
{
    PR_NOT_REACHED("Cannot call thread copy constructor");
}  /* RCThread::RCThread */

RCThread::RCThread(
    RCThread::Scope scope, RCThread::State join, PRUint32 stackSize):
    RCBase()
{
    execution = ex_unstarted;
    identity = PR_CreateThread(
        PR_USER_THREAD, nas_Root, this,
        PR_GetThreadPriority(PR_GetCurrentThread()),
        (PRThreadScope)scope, (PRThreadState)join, stackSize);
}  /* RCThread::RCThread */

void RCThread::operator=(const RCThread&)
{
    PR_NOT_REACHED("Cannot call thread assignment operator");
}  /* RCThread::operator= */


PRStatus RCThread::Start()
{
    PRStatus rv;
    /* This is an unsafe check, but not too critical */
    if (RCThread::ex_unstarted == execution)
    {
        execution = RCThread::ex_started;
        rv = PR_Interrupt(identity);
        PR_ASSERT(PR_SUCCESS == rv);
    }
    else
    {
        rv = PR_FAILURE;
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
    }
    return rv;
}  /* RCThread::Start */

PRStatus RCThread::Join()
{
    PRStatus rv;
    if (RCThread::ex_unstarted == execution)
    {
        rv = PR_FAILURE;
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
    }
    else rv = PR_JoinThread(identity);
    if (PR_SUCCESS == rv) delete this;
    return rv;
}  /* RCThread::Join */

PRStatus RCThread::Interrupt()
{
    PRStatus rv;
    if (RCThread::ex_unstarted == execution)
    {
        rv = PR_FAILURE;
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
    }
    else rv = PR_Interrupt(identity);
    return rv;
}  /* RCThread::Interrupt */

void RCThread::ClearInterrupt() { PR_ClearInterrupt(); }

void RCThread::SetPriority(RCThread::Priority new_priority)
    { PR_SetThreadPriority(identity, (PRThreadPriority)new_priority); }

PRThread *RCThread::Self()
    { return PR_GetCurrentThread(); }

RCThread::Scope RCThread::GetScope() const
    { return (RCThread::Scope)PR_GetThreadScope(identity); }

RCThread::State RCThread::GetState() const
    { return (RCThread::State)PR_GetThreadState(identity); }

RCThread::Priority RCThread::GetPriority() const
    { return (RCThread::Priority)PR_GetThreadPriority(identity); }
    
static void _rc_PDDestructor(RCThreadPrivateData* privateData)
{
    PR_ASSERT(NULL != privateData);
    privateData->Release();
}

static PRThreadPrivateDTOR _tpd_dtor = (PRThreadPrivateDTOR)_rc_PDDestructor;

PRStatus RCThread::NewPrivateIndex(PRUintn* index)
    { return PR_NewThreadPrivateIndex(index, _tpd_dtor); }

PRStatus RCThread::SetPrivateData(PRUintn index)
    { return PR_SetThreadPrivate(index, NULL); }

PRStatus RCThread::SetPrivateData(PRUintn index, RCThreadPrivateData* data)
{
    return PR_SetThreadPrivate(index, data);
}

RCThreadPrivateData* RCThread::GetPrivateData(PRUintn index)
    { return (RCThreadPrivateData*)PR_GetThreadPrivate(index); }

PRStatus RCThread::Sleep(const RCInterval& ticks)
    { PRIntervalTime tmo = ticks; return PR_Sleep(tmo); }

RCPrimordialThread *RCThread::WrapPrimordialThread()
{
    /*
    ** This needs to take more care in insuring that the thread
    ** being wrapped is really the primordial thread. This code
    ** is assuming that the caller is the primordial thread, and
    ** there's nothing to insure that.
    */
    if (NULL == primordial)
    {
        /* it doesn't have to be perfect */
        RCPrimordialThread *me = new RCPrimordialThread();
        PR_ASSERT(NULL != me);
        if (NULL == primordial)
        {
            primordial = me;
            me->execution = RCThread::ex_started;
            me->identity = PR_GetCurrentThread();
        }
        else delete me;  /* somebody beat us to it */
    }
    return primordial;
}  /* RCThread::WrapPrimordialThread */

RCPrimordialThread::RCPrimordialThread(): RCThread() { }

RCPrimordialThread::~RCPrimordialThread() { }

void RCPrimordialThread::RootFunction()
{
    PR_NOT_REACHED("Primordial thread calling root function"); 
}  /* RCPrimordialThread::RootFunction */
 
PRStatus RCPrimordialThread::Cleanup() { return PR_Cleanup(); }

PRStatus RCPrimordialThread::SetVirtualProcessors(PRIntn count)
{
    PR_SetConcurrency(count);
    return PR_SUCCESS;
}  /* SetVirutalProcessors */

RCThreadPrivateData::RCThreadPrivateData() { }

RCThreadPrivateData::RCThreadPrivateData(
    const RCThreadPrivateData& him) { }

RCThreadPrivateData::~RCThreadPrivateData() { }

/* RCThread.c */

