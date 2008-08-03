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
 * File: w32ipcsem.c
 * Description: implements named semaphores for NT and WIN95.
 */

#include "primpl.h"

/*
 * NSPR-to-NT access right mapping table for semaphore objects.
 *
 * The SYNCHRONIZE access is required by WaitForSingleObject.
 * The SEMAPHORE_MODIFY_STATE access is required by ReleaseSemaphore.
 * The OR of these three access masks must equal SEMAPHORE_ALL_ACCESS.
 * This is because if a semaphore object with the specified name
 * exists, CreateSemaphore requests SEMAPHORE_ALL_ACCESS access to
 * the existing object.
 */
static DWORD semAccessTable[] = {
    STANDARD_RIGHTS_REQUIRED|0x1, /* read (0x1 is "query state") */
    STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|SEMAPHORE_MODIFY_STATE, /* write */
    0 /* execute */
};

#ifndef _PR_GLOBAL_THREADS_ONLY

/*
 * A fiber cannot call WaitForSingleObject because that
 * will block the other fibers running on the same thread.
 * If a fiber needs to wait on a (semaphore) handle, we
 * create a native thread to call WaitForSingleObject and
 * have the fiber join the native thread.
 */

/*
 * Arguments, return value, and error code for WaitForSingleObject
 */
struct WaitSingleArg {
    HANDLE handle;
    DWORD timeout;
    DWORD rv;
    DWORD error;
};

static void WaitSingleThread(void *arg)
{
    struct WaitSingleArg *warg = (struct WaitSingleArg *) arg;

    warg->rv = WaitForSingleObject(warg->handle, warg->timeout);
    if (warg->rv == WAIT_FAILED) {
        warg->error = GetLastError();
    }
}

static DWORD FiberSafeWaitForSingleObject(
    HANDLE hHandle,
    DWORD dwMilliseconds
)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (_PR_IS_NATIVE_THREAD(me)) {
        return WaitForSingleObject(hHandle, dwMilliseconds);
    } else {
        PRThread *waitThread;
        struct WaitSingleArg warg;
        PRStatus rv;

        warg.handle = hHandle;
        warg.timeout = dwMilliseconds;
        waitThread = PR_CreateThread(
            PR_USER_THREAD, WaitSingleThread, &warg,
            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
        if (waitThread == NULL) {
            return WAIT_FAILED;
        }

        rv = PR_JoinThread(waitThread);
        PR_ASSERT(rv == PR_SUCCESS);
        if (rv == PR_FAILURE) {
            return WAIT_FAILED;
        }
        if (warg.rv == WAIT_FAILED) {
            SetLastError(warg.error);
        }
        return warg.rv;
    }
}

#endif /* !_PR_GLOBAL_THREADS_ONLY */

PRSem *_PR_MD_OPEN_SEMAPHORE(
    const char *osname, PRIntn flags, PRIntn mode, PRUintn value)
{
    PRSem *sem;
    SECURITY_ATTRIBUTES sa;
    LPSECURITY_ATTRIBUTES lpSA = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pACL = NULL;

    sem = PR_NEW(PRSem);
    if (sem == NULL) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }
    if (flags & PR_SEM_CREATE) {
        if (_PR_NT_MakeSecurityDescriptorACL(mode, semAccessTable,
                &pSD, &pACL) == PR_SUCCESS) {
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = pSD;
            sa.bInheritHandle = FALSE;
            lpSA = &sa;
        }
        sem->sem = CreateSemaphore(lpSA, value, 0x7fffffff, osname);
        if (lpSA != NULL) {
            _PR_NT_FreeSecurityDescriptorACL(pSD, pACL);
        }
        if (sem->sem == NULL) {
            _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
            PR_DELETE(sem);
            return NULL;
        }
        if ((flags & PR_SEM_EXCL) && (GetLastError() == ERROR_ALREADY_EXISTS)) {
            PR_SetError(PR_FILE_EXISTS_ERROR, ERROR_ALREADY_EXISTS);
            CloseHandle(sem->sem);
            PR_DELETE(sem);
            return NULL;
        }
    } else {
        sem->sem = OpenSemaphore(
                SEMAPHORE_MODIFY_STATE|SYNCHRONIZE, FALSE, osname);
        if (sem->sem == NULL) {
            DWORD err = GetLastError();

            /*
             * If we open a nonexistent named semaphore, NT
             * returns ERROR_FILE_NOT_FOUND, while Win95
             * returns ERROR_INVALID_NAME
             */
            if (err == ERROR_INVALID_NAME) {
                PR_SetError(PR_FILE_NOT_FOUND_ERROR, err);
            } else {
                _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
            }
            PR_DELETE(sem);
            return NULL;
        }
    }
    return sem;
}

PRStatus _PR_MD_WAIT_SEMAPHORE(PRSem *sem)
{
    DWORD rv;

#ifdef _PR_GLOBAL_THREADS_ONLY
    rv = WaitForSingleObject(sem->sem, INFINITE);
#else
    rv = FiberSafeWaitForSingleObject(sem->sem, INFINITE);
#endif
    PR_ASSERT(rv == WAIT_FAILED || rv == WAIT_OBJECT_0);
    if (rv == WAIT_FAILED) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        return PR_FAILURE;
    }
    if (rv != WAIT_OBJECT_0) {
        /* Should not happen */
        PR_SetError(PR_UNKNOWN_ERROR, 0);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PRStatus _PR_MD_POST_SEMAPHORE(PRSem *sem)
{
    if (ReleaseSemaphore(sem->sem, 1, NULL) == FALSE) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PRStatus _PR_MD_CLOSE_SEMAPHORE(PRSem *sem)
{
    if (CloseHandle(sem->sem) == FALSE) {
        _PR_MD_MAP_CLOSE_ERROR(GetLastError());
        return PR_FAILURE;
    }
    PR_DELETE(sem);
    return PR_SUCCESS;
}
