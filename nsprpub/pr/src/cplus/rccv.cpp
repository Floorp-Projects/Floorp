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
