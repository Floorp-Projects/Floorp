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
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
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
 * File: pripcsem.c
 *
 * Description: implements the named semaphores API in prsemipc.h
 * for classic NSPR.  If _PR_HAVE_NAMED_SEMAPHORES is not defined,
 * the named semaphore functions all fail with the error code
 * PR_NOT_IMPLEMENTED_ERROR.
 */

#include "primpl.h"

#ifdef _PR_PTHREADS

#error "This file should not be compiled for the pthreads version"

#else

#ifndef _PR_HAVE_NAMED_SEMAPHORES

PRSem * _PR_MD_OPEN_SEMAPHORE(
    const char *osname, PRIntn flags, PRIntn mode, PRUintn value)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PRStatus _PR_MD_WAIT_SEMAPHORE(PRSem *sem)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _PR_MD_POST_SEMAPHORE(PRSem *sem)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _PR_MD_CLOSE_SEMAPHORE(PRSem *sem)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PRStatus _PR_MD_DELETE_SEMAPHORE(const char *osname)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

#endif /* !_PR_HAVE_NAMED_SEMAPHORES */

PR_IMPLEMENT(PRSem *) PR_OpenSemaphore(
    const char *name, PRIntn flags, PRIntn mode, PRUintn value)
{
    char osname[PR_IPC_NAME_SIZE];

    if (!_pr_initialized) _PR_ImplicitInitialization();
    if (_PR_MakeNativeIPCName(name, osname, sizeof(osname), _PRIPCSem)
            == PR_FAILURE) {
        return NULL;
    }
    return _PR_MD_OPEN_SEMAPHORE(osname, flags, mode, value);
}

PR_IMPLEMENT(PRStatus) PR_WaitSemaphore(PRSem *sem)
{
    return _PR_MD_WAIT_SEMAPHORE(sem);
}

PR_IMPLEMENT(PRStatus) PR_PostSemaphore(PRSem *sem)
{
    return _PR_MD_POST_SEMAPHORE(sem);
}

PR_IMPLEMENT(PRStatus) PR_CloseSemaphore(PRSem *sem)
{
    return _PR_MD_CLOSE_SEMAPHORE(sem);
}

PR_IMPLEMENT(PRStatus) PR_DeleteSemaphore(const char *name)
{
    char osname[PR_IPC_NAME_SIZE];

    if (!_pr_initialized) _PR_ImplicitInitialization();
    if (_PR_MakeNativeIPCName(name, osname, sizeof(osname), _PRIPCSem)
            == PR_FAILURE) {
        return PR_FAILURE;
    }
    return _PR_MD_DELETE_SEMAPHORE(osname);
}

#endif /* _PR_PTHREADS */
