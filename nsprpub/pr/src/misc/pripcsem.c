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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
