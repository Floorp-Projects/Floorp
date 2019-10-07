/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
** C++ access to NSPR locks (PRLock)
*/

#include "rclock.h"
#include <prlog.h>

RCLock::RCLock()
{
    lock = PR_NewLock();  /* it might be NULL */
    PR_ASSERT(NULL != lock);
}  /* RCLock::RCLock */

RCLock::~RCLock()
{
    if (NULL != lock) {
        PR_DestroyLock(lock);
    }
    lock = NULL;
}  /* RCLock::~RCLock */

void RCLock::Acquire()
{
    PR_ASSERT(NULL != lock);
    PR_Lock(lock);
}  /* RCLock::Acquire */

void RCLock::Release()
{
    PRStatus rv;
    PR_ASSERT(NULL != lock);
    rv = PR_Unlock(lock);
    PR_ASSERT(PR_SUCCESS == rv);
}  /* RCLock::Release */

/* RCLock.cpp */

