/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: sslmutex.c,v 1.3 2001/06/12 22:53:00 nelsonb%netscape.com Exp $
 */

#include "sslmutex.h"
#include "prerr.h"

#if defined(LINUX) || defined(AIX)

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "unix_err.h"
#include "pratom.h"

#define SSL_MUTEX_MAGIC 0xfeedfd
#define NONBLOCKING_POSTS 1	/* maybe this is faster */

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
sslMutex_Init(sslMutex *pMutex, int shared)
{
    int  err;

    pMutex->mPipes[0] = -1;
    pMutex->mPipes[1] = -1;
    pMutex->mPipes[2] = -1;
    pMutex->nWaiters  =  0;

    err = pipe(pMutex->mPipes);
    if (err) {
	return err;
    }
    /* close-on-exec is false by default */
    if (!shared) {
	err = fcntl(pMutex->mPipes[0], F_SETFD, FD_CLOEXEC);
	if (err) 
	    goto loser;

	err = fcntl(pMutex->mPipes[1], F_SETFD, FD_CLOEXEC);
	if (err) 
	    goto loser;
    }

#if NONBLOCKING_POSTS
    err = setNonBlocking(pMutex->mPipes[1], 1);
    if (err)
	goto loser;
#endif

    pMutex->mPipes[2] = SSL_MUTEX_MAGIC;

#if defined(LINUX) && defined(i386)
    /* Pipe starts out empty */
    return SECSuccess;
#else
    /* Pipe starts with one byte. */
    return sslMutex_Unlock(pMutex);
#endif

loser:
    nss_MD_unix_map_default_error(errno);
    close(pMutex->mPipes[0]);
    close(pMutex->mPipes[1]);
    return SECFailure;
}

SECStatus
sslMutex_Destroy(sslMutex *pMutex)
{
    if (pMutex->mPipes[2] != SSL_MUTEX_MAGIC) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }
    close(pMutex->mPipes[0]);
    close(pMutex->mPipes[1]);

    pMutex->mPipes[0] = -1;
    pMutex->mPipes[1] = -1;
    pMutex->mPipes[2] = -1;
    pMutex->nWaiters  =  0;

    return SECSuccess;
}

#if defined(LINUX) && defined(i386)
/* No memory barrier needed for this platform */

SECStatus 
sslMutex_Unlock(sslMutex *pMutex)
{
    PRInt32 oldValue;

    if (pMutex->mPipes[2] != SSL_MUTEX_MAGIC) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }
    /* Do Memory Barrier here. */
    oldValue = PR_AtomicDecrement(&pMutex->nWaiters);
    if (oldValue > 1) {
	int  cc;
	char c  = 1;
	do {
	    cc = write(pMutex->mPipes[1], &c, 1);
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
sslMutex_Lock(sslMutex *pMutex)
{
    PRInt32 oldValue;

    if (pMutex->mPipes[2] != SSL_MUTEX_MAGIC) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }
    oldValue = PR_AtomicDecrement(&pMutex->nWaiters);
    /* Do Memory Barrier here. */
    if (oldValue > 0) {
	int   cc;
	char  c;
	do {
	    cc = read(pMutex->mPipes[0], &c, 1);
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
sslMutex_Unlock(sslMutex *pMutex)
{
    int  cc;
    char c  = 1;

    if (pMutex->mPipes[2] != SSL_MUTEX_MAGIC) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }
    do {
	cc = write(pMutex->mPipes[1], &c, 1);
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
sslMutex_Lock(sslMutex *pMutex)
{
    int   cc;
    char  c;

    if (pMutex->mPipes[2] != SSL_MUTEX_MAGIC) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }

    do {
	cc = read(pMutex->mPipes[0], &c, 1);
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

/* The presence of the TRUE element in this struct makes the semaphore
 * inheritable. The NULL means use process's default security descriptor.
 */

SECStatus
sslMutex_Init(sslMutex *pMutex, int shared)
{

    HANDLE hMutex;
    SECURITY_ATTRIBUTES attributes =
                                { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    PR_ASSERT(pMutex != 0 && (*pMutex == 0 || *pMutex == INVALID_HANDLE_VALUE));
    if (!pMutex || ((hMutex = *pMutex) != 0 && hMutex != INVALID_HANDLE_VALUE)) {
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
    *pMutex = hMutex;
    return SECSuccess;
}

int
sslMutex_Destroy(sslMutex *pMutex)
{
    HANDLE hMutex;
    int    rv;

    PR_ASSERT(pMutex != 0 && *pMutex != 0 && *pMutex != INVALID_HANDLE_VALUE);
    if (!pMutex || (hMutex = *pMutex) == 0 || hMutex == INVALID_HANDLE_VALUE) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }
    rv = CloseHandle(hMutex); /* ignore error */
    if (rv) {
	*pMutex = hMutex = INVALID_HANDLE_VALUE;
	return SECSuccess;
    }
    nss_MD_win32_map_default_error(GetLastError());
    return SECFailure;
}

int 
sslMutex_Unlock(sslMutex *pMutex)
{
    BOOL   success = FALSE;
    HANDLE hMutex;

    PR_ASSERT(pMutex != 0 && *pMutex != 0 && *pMutex != INVALID_HANDLE_VALUE);
    if (!pMutex || (hMutex = *pMutex) == 0 || hMutex == INVALID_HANDLE_VALUE) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }
    success = ReleaseMutex(hMutex);
    if (!success) {
        nss_MD_win32_map_default_error(GetLastError());
        return SECFailure;
    }
    return SECSuccess;
}

int 
sslMutex_Lock(sslMutex *pMutex)
{
    HANDLE    hMutex;
    DWORD     event;
    DWORD     lastError;
    SECStatus rv;

    PR_ASSERT(pMutex != 0 && *pMutex != 0 && *pMutex != INVALID_HANDLE_VALUE);
    if (!pMutex || (hMutex = *pMutex) == 0 || hMutex == INVALID_HANDLE_VALUE) {
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;      /* what else ? */
    }
    event = WaitForSingleObject(hMutex, INFINITE);
    switch (event) {
    case WAIT_OBJECT_0:
    case WAIT_ABANDONED:
        rv = SECSuccess;
        break;

    case WAIT_TIMEOUT:
    case WAIT_IO_COMPLETION:
    default:            /* should never happen. nothing we can do. */
        PR_ASSERT(!("WaitForSingleObject returned invalid value."));
	PORT_SetError(PR_UNKNOWN_ERROR);
	rv = SECFailure;
	break;

    case WAIT_FAILED:           /* failure returns this */
        rv = SECFailure;
        lastError = GetLastError();     /* for debugging */
        nss_MD_win32_map_default_error(lastError);
        break;
    }
    return rv;
}

#elif defined(XP_UNIX)

#include <errno.h>
#include "unix_err.h"

SECStatus 
sslMutex_Init(sslMutex *pMutex, int shared)
{
    int rv;
    do {
	rv = sem_init(pMutex, shared, 1);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
	nss_MD_unix_map_default_error(errno);
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus 
sslMutex_Destroy(sslMutex *pMutex)
{
    int rv;
    do {
	rv = sem_destroy(pMutex);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
	nss_MD_unix_map_default_error(errno);
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus 
sslMutex_Unlock(sslMutex *pMutex)
{
    int rv;
    do {
	rv = sem_post(pMutex);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
	nss_MD_unix_map_default_error(errno);
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus 
sslMutex_Lock(sslMutex *pMutex)
{
    int rv;
    do {
	rv = sem_wait(pMutex);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
	nss_MD_unix_map_default_error(errno);
	return SECFailure;
    }
    return SECSuccess;
}

#else

SECStatus 
sslMutex_Init(sslMutex *pMutex, int shared)
{
    PORT_Assert(!("sslMutex_Init not implemented!"));
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

SECStatus 
sslMutex_Destroy(sslMutex *pMutex)
{
    PORT_Assert(!("sslMutex_Destroy not implemented!"));
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

SECStatus 
sslMutex_Unlock(sslMutex *pMutex)
{
    PORT_Assert(!("sslMutex_Unlock not implemented!"));
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

SECStatus 
sslMutex_Lock(sslMutex *pMutex)
{
    PORT_Assert(!("sslMutex_Lock not implemented!"));
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

#endif
