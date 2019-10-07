/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
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

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    if (_PR_MakeNativeIPCName(name, osname, sizeof(osname), _PRIPCSem)
        == PR_FAILURE) {
        return PR_FAILURE;
    }
    return _PR_MD_DELETE_SEMAPHORE(osname);
}

#endif /* _PR_PTHREADS */
