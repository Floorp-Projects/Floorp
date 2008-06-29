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

/*
** DEPRECATED.  Use PR_SetEnv() instead.
*/
#ifdef XP_MAC
PR_IMPLEMENT(PRIntn) PR_PutEnv(const char *string)
{
    return (PR_SetEnv(string) == PR_SUCCESS) ? PR_TRUE : PR_FALSE;
}
#endif
