/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** RCCondition - C++ wrapper around NSPR's PRCondVar
*/

#include "rccv.h"

#include <prlog.h>
#include <prerror.h>
#include <prcvar.h>

RCCondition::RCCondition(class RCLock *lock): RCBase()
{
    cv = PR_NewCondVar(lock->lock);
    PR_ASSERT(NULL != cv);
    timeout = PR_INTERVAL_NO_TIMEOUT;
}  /* RCCondition::RCCondition */

RCCondition::~RCCondition()
{
    if (NULL != cv) PR_DestroyCondVar(cv);
}  /* RCCondition::~RCCondition */

PRStatus RCCondition::Wait()
{
    PRStatus rv;
    PR_ASSERT(NULL != cv);
    if (NULL == cv)
    {
        SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        rv = PR_FAILURE;
    }
    else
        rv = PR_WaitCondVar(cv, timeout.interval);
    return rv;
}  /* RCCondition::Wait */

PRStatus RCCondition::Notify()
{
    return PR_NotifyCondVar(cv);
}  /* RCCondition::Notify */

PRStatus RCCondition::Broadcast()
{
    return PR_NotifyAllCondVar(cv);
}  /* RCCondition::Broadcast */

PRStatus RCCondition::SetTimeout(const RCInterval& tmo)
{
    if (NULL == cv)
    {
        SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    timeout = tmo;
    return PR_SUCCESS;
}  /* RCCondition::SetTimeout */

RCInterval RCCondition::GetTimeout() const { return timeout; }

/* rccv.cpp */
