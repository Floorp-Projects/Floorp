/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "primpl.h"

/* Lock used to lock the environment */
#if defined(_PR_NO_PREEMPT)
#define _PR_NEW_LOCK_ENV()
#define _PR_DELETE_LOCK_ENV()
#define _PR_LOCK_ENV()
#define _PR_UNLOCK_ENV()
#elif defined(_PR_LOCAL_THREADS_ONLY)
extern _PRCPU * _pr_primordialCPU;
static PRIntn _is;
#define _PR_NEW_LOCK_ENV()
#define _PR_DELETE_LOCK_ENV()
#define _PR_LOCK_ENV() if (_pr_primordialCPU) _PR_INTSOFF(_is);
#define _PR_UNLOCK_ENV() if (_pr_primordialCPU) _PR_INTSON(_is);
#else
static PRLock *_pr_envLock = NULL;
#define _PR_NEW_LOCK_ENV() {_pr_envLock = PR_NewLock();}
#define _PR_DELETE_LOCK_ENV() \
    { if (_pr_envLock) { PR_DestroyLock(_pr_envLock); _pr_envLock = NULL; } }
#define _PR_LOCK_ENV() { if (_pr_envLock) PR_Lock(_pr_envLock); }
#define _PR_UNLOCK_ENV() { if (_pr_envLock) PR_Unlock(_pr_envLock); }
#endif

/************************************************************************/

void _PR_InitEnv(void)
{
	_PR_NEW_LOCK_ENV();
}

void _PR_CleanupEnv(void)
{
    _PR_DELETE_LOCK_ENV();
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

PR_IMPLEMENT(PRStatus) PR_SetEnv(const char *string)
{
    PRIntn result;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if ( !strchr(string, '=')) return(PR_FAILURE);

    _PR_LOCK_ENV();
    result = _PR_MD_PUT_ENV(string);
    _PR_UNLOCK_ENV();
    return (result)? PR_FAILURE : PR_SUCCESS;
}
