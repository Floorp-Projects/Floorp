/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#include <string.h>
#include "primpl.h"

/* Lock used to lock the environment */
#if defined(_PR_NO_PREEMPT)
#define _PR_NEW_LOCK_ENV()
#define _PR_LOCK_ENV()
#define _PR_UNLOCK_ENV()
#elif defined(_PR_LOCAL_THREADS_ONLY)
#define _PR_NEW_LOCK_ENV()
#define _PR_LOCK_ENV() { PRIntn _is; _PR_INTSOFF(_is)
#define _PR_UNLOCK_ENV() _PR_INTSON(_is); }
#else
static PRLock *_pr_envLock;
#define _PR_NEW_LOCK_ENV() {_pr_envLock = PR_NewLock();}
#define _PR_LOCK_ENV() PR_Lock(_pr_envLock)
#define _PR_UNLOCK_ENV() PR_Unlock(_pr_envLock)
#endif

/************************************************************************/

void _PR_InitEnv()
{
	_PR_NEW_LOCK_ENV();
}

PR_IMPLEMENT(char*) PR_GetEnv(const char *var)
{
    char *ev;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    _PR_LOCK_ENV();
    ev = _PR_MD_GET_ENV(var);
    _PR_UNLOCK_ENV();
    return ev;
}
