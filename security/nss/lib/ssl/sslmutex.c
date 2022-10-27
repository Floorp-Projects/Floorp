/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "seccomon.h"
/* This ifdef should match the one in sslsnce.c */
#if defined(XP_UNIX) || defined(XP_WIN32) || defined(XP_OS2)

#include "sslmutex.h"
#include "prerr.h"

static SECStatus
single_process_sslMutex_Init(sslMutex* pMutex)
{
    PR_ASSERT(pMutex != 0 && pMutex->u.sslLock == 0);

    pMutex->u.sslLock = PR_NewLock();
    if (!pMutex->u.sslLock) {
        return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
single_process_sslMutex_Destroy(sslMutex* pMutex)
{
    PR_ASSERT(pMutex != 0);
    PR_ASSERT(pMutex->u.sslLock != 0);
    if (!pMutex->u.sslLock) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    PR_DestroyLock(pMutex->u.sslLock);
    return SECSuccess;
}

static SECStatus
single_process_sslMutex_Unlock(sslMutex* pMutex)
{
    PR_ASSERT(pMutex != 0);
    PR_ASSERT(pMutex->u.sslLock != 0);
    if (!pMutex->u.sslLock) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    PR_Unlock(pMutex->u.sslLock);
    return SECSuccess;
}

static SECStatus
single_process_sslMutex_Lock(sslMutex* pMutex)
{
    PR_ASSERT(pMutex != 0);
    PR_ASSERT(pMutex->u.sslLock != 0);
    if (!pMutex->u.sslLock) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    PR_Lock(pMutex->u.sslLock);
    return SECSuccess;
}

#if defined(LINUX) || defined(AIX) || defined(BSDI) || \
    (defined(NETBSD) && __NetBSD_Version__ < 500000000) || defined(OPENBSD) || defined(__GLIBC__)

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "unix_err.h"
#include "pratom.h"

#define SSL_MUTEX_MAGIC 0xfeedfd
#define NONBLOCKING_POSTS 1 /* maybe this is faster */

#if NONBLOCKING_POSTS

#ifndef FNONBLOCK
#define FNONBLOCK O_NONBLOCK
#endif

static int
setNonBlocking(int fd, int nonBlocking)
{
    int flags;
    int err;

    flags = fcntl(fd, F_GETFL, 0);
    if (0 > flags)
        return flags;
    if (nonBlocking)
        flags |= FNONBLOCK;
    else
        flags &= ~FNONBLOCK;
    err = fcntl(fd, F_SETFL, flags);
    return err;
}
#endif

SECStatus
sslMutex_Init(sslMutex* pMutex, int shared)
{
    int err;
    PR_ASSERT(pMutex);
    pMutex->isMultiProcess = (PRBool)(shared != 0);
    if (!shared) {
        return single_process_sslMutex_Init(pMutex);
    }
    pMutex->u.pipeStr.mPipes[0] = -1;
    pMutex->u.pipeStr.mPipes[1] = -1;
    pMutex->u.pipeStr.mPipes[2] = -1;
    pMutex->u.pipeStr.nWaiters = 0;

    err = pipe(pMutex->u.pipeStr.mPipes);
    if (err) {
        nss_MD_unix_map_default_error(errno);
        return err;
    }
#if NONBLOCKING_POSTS
    err = setNonBlocking(pMutex->u.pipeStr.mPipes[1], 1);
    if (err)
        goto loser;
#endif

    pMutex->u.pipeStr.mPipes[2] = SSL_MUTEX_MAGIC;

#if defined(LINUX) && defined(i386)
    /* Pipe starts out empty */
    return SECSuccess;
#else
    /* Pipe starts with one byte. */
    return sslMutex_Unlock(pMutex);
#endif

loser:
    nss_MD_unix_map_default_error(errno);
    close(pMutex->u.pipeStr.mPipes[0]);
    close(pMutex->u.pipeStr.mPipes[1]);
    return SECFailure;
}

SECStatus
sslMutex_Destroy(sslMutex* pMutex, PRBool processLocal)
{
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Destroy(pMutex);
    }
    if (pMutex->u.pipeStr.mPipes[2] != SSL_MUTEX_MAGIC) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    close(pMutex->u.pipeStr.mPipes[0]);
    close(pMutex->u.pipeStr.mPipes[1]);

    if (processLocal) {
        return SECSuccess;
    }

    pMutex->u.pipeStr.mPipes[0] = -1;
    pMutex->u.pipeStr.mPipes[1] = -1;
    pMutex->u.pipeStr.mPipes[2] = -1;
    pMutex->u.pipeStr.nWaiters = 0;

    return SECSuccess;
}

#if defined(LINUX) && defined(i386)
/* No memory barrier needed for this platform */

/* nWaiters includes the holder of the lock (if any) and the number
** threads waiting for it.  After incrementing nWaiters, if the count
** is exactly 1, then you have the lock and may proceed.  If the
** count is greater than 1, then you must wait on the pipe.
*/

SECStatus
sslMutex_Unlock(sslMutex* pMutex)
{
    PRInt32 newValue;
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Unlock(pMutex);
    }

    if (pMutex->u.pipeStr.mPipes[2] != SSL_MUTEX_MAGIC) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    /* Do Memory Barrier here. */
    newValue = PR_ATOMIC_DECREMENT(&pMutex->u.pipeStr.nWaiters);
    if (newValue > 0) {
        int cc;
        char c = 1;
        do {
            cc = write(pMutex->u.pipeStr.mPipes[1], &c, 1);
        } while (cc < 0 && (errno == EINTR || errno == EAGAIN));
        if (cc != 1) {
            if (cc < 0)
                nss_MD_unix_map_default_error(errno);
            else
                PORT_SetError(PR_UNKNOWN_ERROR);
            return SECFailure;
        }
    }
    return SECSuccess;
}

SECStatus
sslMutex_Lock(sslMutex* pMutex)
{
    PRInt32 newValue;
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Lock(pMutex);
    }

    if (pMutex->u.pipeStr.mPipes[2] != SSL_MUTEX_MAGIC) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    newValue = PR_ATOMIC_INCREMENT(&pMutex->u.pipeStr.nWaiters);
    /* Do Memory Barrier here. */
    if (newValue > 1) {
        int cc;
        char c;
        do {
            cc = read(pMutex->u.pipeStr.mPipes[0], &c, 1);
        } while (cc < 0 && errno == EINTR);
        if (cc != 1) {
            if (cc < 0)
                nss_MD_unix_map_default_error(errno);
            else
                PORT_SetError(PR_UNKNOWN_ERROR);
            return SECFailure;
        }
    }
    return SECSuccess;
}

#else

/* Using Atomic operations requires the use of a memory barrier instruction
** on PowerPC, Sparc, and Alpha.  NSPR's PR_Atomic functions do not perform
** them, and NSPR does not provide a function that does them (e.g. PR_Barrier).
** So, we don't use them on those platforms.
*/

SECStatus
sslMutex_Unlock(sslMutex* pMutex)
{
    int cc;
    char c = 1;

    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Unlock(pMutex);
    }

    if (pMutex->u.pipeStr.mPipes[2] != SSL_MUTEX_MAGIC) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    do {
        cc = write(pMutex->u.pipeStr.mPipes[1], &c, 1);
    } while (cc < 0 && (errno == EINTR || errno == EAGAIN));
    if (cc != 1) {
        if (cc < 0)
            nss_MD_unix_map_default_error(errno);
        else
            PORT_SetError(PR_UNKNOWN_ERROR);
        return SECFailure;
    }

    return SECSuccess;
}

SECStatus
sslMutex_Lock(sslMutex* pMutex)
{
    int cc;
    char c;

    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Lock(pMutex);
    }

    if (pMutex->u.pipeStr.mPipes[2] != SSL_MUTEX_MAGIC) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    do {
        cc = read(pMutex->u.pipeStr.mPipes[0], &c, 1);
    } while (cc < 0 && errno == EINTR);
    if (cc != 1) {
        if (cc < 0)
            nss_MD_unix_map_default_error(errno);
        else
            PORT_SetError(PR_UNKNOWN_ERROR);
        return SECFailure;
    }

    return SECSuccess;
}

#endif

#elif defined(WIN32)

#include "win32err.h"

/* on Windows, we need to find the optimal type of locking mechanism to use
 for the sslMutex.

 There are 3 cases :
 1) single-process, use a PRLock, as for all other platforms
 2) Win95 multi-process, use a Win32 mutex
 3) on WINNT multi-process, use a PRLock + a Win32 mutex

*/

#ifdef WINNT

SECStatus
sslMutex_2LevelInit(sslMutex *sem)
{
    /*  the following adds a PRLock to sslMutex . This is done in each
        process of a multi-process server and is only needed on WINNT, if
        using fibers. We can't tell if native threads or fibers are used, so
        we always do it on WINNT
    */
    PR_ASSERT(sem);
    if (sem) {
        /* we need to reset the sslLock in the children or the single_process init
           function below will assert */
        sem->u.sslLock = NULL;
    }
    return single_process_sslMutex_Init(sem);
}

static SECStatus
sslMutex_2LevelDestroy(sslMutex *sem)
{
    return single_process_sslMutex_Destroy(sem);
}

#endif

SECStatus
sslMutex_Init(sslMutex *pMutex, int shared)
{
#ifdef WINNT
    SECStatus retvalue;
#endif
    HANDLE hMutex;
    SECURITY_ATTRIBUTES attributes = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    PR_ASSERT(pMutex != 0 && (pMutex->u.sslMutx == 0 ||
                              pMutex->u.sslMutx ==
                                  INVALID_HANDLE_VALUE));

    pMutex->isMultiProcess = (PRBool)(shared != 0);

    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Init(pMutex);
    }

#ifdef WINNT
    /*  we need a lock on WINNT for fibers in the parent process */
    retvalue = sslMutex_2LevelInit(pMutex);
    if (SECSuccess != retvalue)
        return SECFailure;
#endif

    if (!pMutex || ((hMutex = pMutex->u.sslMutx) != 0 &&
                    hMutex !=
                        INVALID_HANDLE_VALUE)) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    attributes.bInheritHandle = (shared ? TRUE : FALSE);
    hMutex = CreateMutex(&attributes, FALSE, NULL);
    if (hMutex == NULL) {
        hMutex = INVALID_HANDLE_VALUE;
        nss_MD_win32_map_default_error(GetLastError());
        return SECFailure;
    }
    pMutex->u.sslMutx = hMutex;
    return SECSuccess;
}

SECStatus
sslMutex_Destroy(sslMutex *pMutex, PRBool processLocal)
{
    HANDLE hMutex;
    int rv;
    int retvalue = SECSuccess;

    PR_ASSERT(pMutex != 0);
    if (!pMutex) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Destroy(pMutex);
    }

/*  multi-process mode */
#ifdef WINNT
    /* on NT, get rid of the PRLock used for fibers within a process */
    retvalue = sslMutex_2LevelDestroy(pMutex);
#endif

    PR_ASSERT(pMutex->u.sslMutx != 0 &&
              pMutex->u.sslMutx != INVALID_HANDLE_VALUE);
    if ((hMutex = pMutex->u.sslMutx) == 0 || hMutex == INVALID_HANDLE_VALUE) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    rv = CloseHandle(hMutex); /* ignore error */
    if (!processLocal && rv) {
        pMutex->u.sslMutx = hMutex = INVALID_HANDLE_VALUE;
    }
    if (!rv) {
        nss_MD_win32_map_default_error(GetLastError());
        retvalue = SECFailure;
    }
    return retvalue;
}

int
sslMutex_Unlock(sslMutex *pMutex)
{
    BOOL success = FALSE;
    HANDLE hMutex;

    PR_ASSERT(pMutex != 0);
    if (!pMutex) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Unlock(pMutex);
    }

    PR_ASSERT(pMutex->u.sslMutx != 0 &&
              pMutex->u.sslMutx != INVALID_HANDLE_VALUE);
    if ((hMutex = pMutex->u.sslMutx) == 0 || hMutex == INVALID_HANDLE_VALUE) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }
    success = ReleaseMutex(hMutex);
    if (!success) {
        nss_MD_win32_map_default_error(GetLastError());
        return SECFailure;
    }
#ifdef WINNT
    return single_process_sslMutex_Unlock(pMutex);
/* release PRLock for other fibers in the process */
#else
    return SECSuccess;
#endif
}

int
sslMutex_Lock(sslMutex *pMutex)
{
    HANDLE hMutex;
    DWORD event;
    DWORD lastError;
    SECStatus rv;
    SECStatus retvalue = SECSuccess;

    PR_ASSERT(pMutex != 0);
    if (!pMutex) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Lock(pMutex);
    }
#ifdef WINNT
    /* lock first to preserve from other threads/fibers in the same process */
    retvalue = single_process_sslMutex_Lock(pMutex);
#endif
    PR_ASSERT(pMutex->u.sslMutx != 0 &&
              pMutex->u.sslMutx != INVALID_HANDLE_VALUE);
    if ((hMutex = pMutex->u.sslMutx) == 0 || hMutex == INVALID_HANDLE_VALUE) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure; /* what else ? */
    }
    /* acquire the mutex to be the only owner accross all other processes */
    event = WaitForSingleObject(hMutex, INFINITE);
    switch (event) {
        case WAIT_OBJECT_0:
        case WAIT_ABANDONED:
            rv = SECSuccess;
            break;

        case WAIT_TIMEOUT:
#if defined(WAIT_IO_COMPLETION)
        case WAIT_IO_COMPLETION:
#endif
        default: /* should never happen. nothing we can do. */
            PR_ASSERT(PR_FALSE && "WaitForSingleObject returned invalid value.");
            PORT_SetError(PR_UNKNOWN_ERROR);
            rv = SECFailure;
            break;

        case WAIT_FAILED: /* failure returns this */
            rv = SECFailure;
            lastError = GetLastError(); /* for debugging */
            nss_MD_win32_map_default_error(lastError);
            break;
    }

    if (!(SECSuccess == retvalue && SECSuccess == rv)) {
        return SECFailure;
    }

    return SECSuccess;
}

#elif defined(XP_UNIX) && !defined(DARWIN)

#include <errno.h>
#include "unix_err.h"

SECStatus
sslMutex_Init(sslMutex* pMutex, int shared)
{
    int rv;
    PR_ASSERT(pMutex);
    pMutex->isMultiProcess = (PRBool)(shared != 0);
    if (!shared) {
        return single_process_sslMutex_Init(pMutex);
    }
    do {
        rv = sem_init(&pMutex->u.sem, shared, 1);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
        nss_MD_unix_map_default_error(errno);
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus
sslMutex_Destroy(sslMutex* pMutex, PRBool processLocal)
{
    int rv;
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Destroy(pMutex);
    }

    /* semaphores are global resources. See SEM_DESTROY(3) man page */
    if (processLocal) {
        return SECSuccess;
    }
    do {
        rv = sem_destroy(&pMutex->u.sem);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
        nss_MD_unix_map_default_error(errno);
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus
sslMutex_Unlock(sslMutex* pMutex)
{
    int rv;
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Unlock(pMutex);
    }
    do {
        rv = sem_post(&pMutex->u.sem);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
        nss_MD_unix_map_default_error(errno);
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus
sslMutex_Lock(sslMutex* pMutex)
{
    int rv;
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Lock(pMutex);
    }
    do {
        rv = sem_wait(&pMutex->u.sem);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
        nss_MD_unix_map_default_error(errno);
        return SECFailure;
    }
    return SECSuccess;
}

#else

SECStatus
sslMutex_Init(sslMutex* pMutex, int shared)
{
    PR_ASSERT(pMutex);
    pMutex->isMultiProcess = (PRBool)(shared != 0);
    if (!shared) {
        return single_process_sslMutex_Init(pMutex);
    }
    PORT_Assert(PR_FALSE && "sslMutex_Init not implemented for multi-process applications !");
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

SECStatus
sslMutex_Destroy(sslMutex* pMutex, PRBool processLocal)
{
    PR_ASSERT(pMutex);
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Destroy(pMutex);
    }
    PORT_Assert(PR_FALSE && "sslMutex_Destroy not implemented for multi-process applications !");
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

SECStatus
sslMutex_Unlock(sslMutex* pMutex)
{
    PR_ASSERT(pMutex);
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Unlock(pMutex);
    }
    PORT_Assert(PR_FALSE && "sslMutex_Unlock not implemented for multi-process applications !");
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

SECStatus
sslMutex_Lock(sslMutex* pMutex)
{
    PR_ASSERT(pMutex);
    if (PR_FALSE == pMutex->isMultiProcess) {
        return single_process_sslMutex_Lock(pMutex);
    }
    PORT_Assert(PR_FALSE && "sslMutex_Lock not implemented for multi-process applications !");
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

#endif

#endif
