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
    if (NULL != lock) PR_DestroyLock(lock);
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

