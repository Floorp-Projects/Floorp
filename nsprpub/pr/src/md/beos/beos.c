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

#include "primpl.h"

#include <signal.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>

/*
 * Make sure _PRSockLen_t is 32-bit, because we will cast a PRUint32* or
 * PRInt32* pointer to a _PRSockLen_t* pointer.
 */
#define _PRSockLen_t int

/*
** Global lock variable used to bracket calls into rusty libraries that
** aren't thread safe (like libc, libX, etc).
*/
static PRLock *_pr_rename_lock = NULL;
static PRMonitor *_pr_Xfe_mon = NULL;

/*
 * Variables used by the GC code, initialized in _MD_InitSegs().
 * _pr_zero_fd should be a static variable.  Unfortunately, there is
 * still some Unix-specific code left in function PR_GrowSegment()
 * in file memory/prseg.c that references it, so it needs
 * to be a global variable for now.
 */
PRInt32 _pr_zero_fd = -1;
static PRLock *_pr_md_lock = NULL;

sigset_t timer_set;

void _PR_UnixInit()
{
	struct sigaction sigact;
	int rv;

	sigemptyset(&timer_set);

	sigact.sa_handler = SIG_IGN;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	rv = sigaction(SIGPIPE, &sigact, 0);
	PR_ASSERT(0 == rv);

	_pr_rename_lock = PR_NewLock();
	PR_ASSERT(NULL != _pr_rename_lock);
	_pr_Xfe_mon = PR_NewMonitor();
	PR_ASSERT(NULL != _pr_Xfe_mon);
}

/*
 *-----------------------------------------------------------------------
 *
 * PR_Now --
 *
 *     Returns the current time in microseconds since the epoch.
 *     The epoch is midnight January 1, 1970 GMT.
 *     The implementation is machine dependent.  This is the Unix
 *     implementation.
 *     Cf. time_t time(time_t *tp)
 *
 *-----------------------------------------------------------------------
 */

PR_IMPLEMENT(PRTime)
PR_Now(void)
{
	struct timeval tv;
	PRInt64 s, us, s2us;

	GETTIMEOFDAY(&tv);
	LL_I2L(s2us, PR_USEC_PER_SEC);
	LL_I2L(s, tv.tv_sec);
	LL_I2L(us, tv.tv_usec);
	LL_MUL(s, s, s2us);
	LL_ADD(s, s, us);
	return s;
}

PRIntervalTime
_PR_UNIX_GetInterval()
{
	struct timeval time;
	PRIntervalTime ticks;

	(void)GETTIMEOFDAY(&time);  /* fallicy of course */
	ticks = (PRUint32)time.tv_sec * PR_MSEC_PER_SEC;  /* that's in milliseconds */
	ticks += (PRUint32)time.tv_usec / PR_USEC_PER_MSEC;  /* so's that */
	return ticks;
}  /* _PR_SUNOS_GetInterval */

PRIntervalTime _PR_UNIX_TicksPerSecond()
{
	return 1000;  /* this needs some work :) */
}

/************************************************************************/

/*
** Special hacks for xlib. Xlib/Xt/Xm is not re-entrant nor is it thread
** safe.  Unfortunately, neither is mozilla. To make these programs work
** in a pre-emptive threaded environment, we need to use a lock.
*/

void PR_XLock()
{
	PR_EnterMonitor(_pr_Xfe_mon);
}

void PR_XUnlock()
{
	PR_ExitMonitor(_pr_Xfe_mon);
}

PRBool PR_XIsLocked()
{
	return (PR_InMonitor(_pr_Xfe_mon)) ? PR_TRUE : PR_FALSE;
}

void PR_XWait(int ms)
{
	PR_Wait(_pr_Xfe_mon, PR_MillisecondsToInterval(ms));
}

void PR_XNotify(void)
{
	PR_Notify(_pr_Xfe_mon);
}

void PR_XNotifyAll(void)
{
	PR_NotifyAll(_pr_Xfe_mon);
}

#if !defined(BEOS)
#ifdef HAVE_BSD_FLOCK

#include <sys/file.h>

PR_IMPLEMENT(PRStatus)
_MD_LOCKFILE (PRInt32 f)
{
	PRInt32 rv;
	rv = flock(f, LOCK_EX);
	if (rv == 0)
		return PR_SUCCESS;
	_PR_MD_MAP_FLOCK_ERROR(_MD_ERRNO());
	return PR_FAILURE;
}

PR_IMPLEMENT(PRStatus)
_MD_TLOCKFILE (PRInt32 f)
{
	PRInt32 rv;
	rv = flock(f, LOCK_EX|LOCK_NB);
	if (rv == 0)
		return PR_SUCCESS;
	_PR_MD_MAP_FLOCK_ERROR(_MD_ERRNO());
	return PR_FAILURE;
}

PR_IMPLEMENT(PRStatus)
_MD_UNLOCKFILE (PRInt32 f)
{
	PRInt32 rv;
	rv = flock(f, LOCK_UN);
	if (rv == 0)
		return PR_SUCCESS;
	_PR_MD_MAP_FLOCK_ERROR(_MD_ERRNO());
	return PR_FAILURE;
}
#else

PR_IMPLEMENT(PRStatus)
_MD_LOCKFILE (PRInt32 f)
{
	PRInt32 rv;
	rv = lockf(f, F_LOCK, 0);
	if (rv == 0)
		return PR_SUCCESS;
	_PR_MD_MAP_LOCKF_ERROR(_MD_ERRNO());
	return PR_FAILURE;
}

PR_IMPLEMENT(PRStatus)
_MD_TLOCKFILE (PRInt32 f)
{
	PRInt32 rv;
	rv = lockf(f, F_TLOCK, 0);
	if (rv == 0)
		return PR_SUCCESS;
	_PR_MD_MAP_LOCKF_ERROR(_MD_ERRNO());
	return PR_FAILURE;
}

PR_IMPLEMENT(PRStatus)
_MD_UNLOCKFILE (PRInt32 f)
{
	PRInt32 rv;
	rv = lockf(f, F_ULOCK, 0);
	if (rv == 0)
		return PR_SUCCESS;
	_PR_MD_MAP_LOCKF_ERROR(_MD_ERRNO());
	return PR_FAILURE;
}
#endif

PR_IMPLEMENT(PRStatus)
  _MD_GETHOSTNAME (char *name, PRUint32 namelen)
{
    PRIntn rv;

    rv = gethostname(name, namelen);
    if (0 == rv) {
		return PR_SUCCESS;
    }
	_PR_MD_MAP_GETHOSTNAME_ERROR(_MD_ERRNO());
    return PR_FAILURE;
}

#endif
