/* -*- Mode: C; c-file-style: "stroustrup"; comment-column: 40 -*- */
/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Mailstone utility, 
 * released March 17, 2000.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 *			Mike Blakely
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License Version 2 or later (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the NPL or the GPL.
 */

#include "bench.h"

#ifdef USE_LRAND48_R			/* also AIX (only when using threads)*/

struct drand48_data drand48data;

/* should verify that the struct drand48_data can be shared between threads */
void
osf_srand48_r(unsigned int seed)
{
    srand48_r(seed, &drand48data);
}


long
osf_lrand48_r(void)
{
    long ret;

    lrand48_r(&drand48data, &ret);
    return ret;
}

#endif /* __OSF1__ */

#ifdef _WIN32
/* close socket library at exit() time */
void sock_cleanup(void) {
    WSACleanup();
}
#endif /* _WIN32 */

/* neither sleep or usleep re-start if interrupted */
void
MS_sleep(unsigned int secs)
{
#ifdef _WIN32
  Sleep(secs * 1000);
#else
  struct timeval sleeptime;

  /*D_PRINTF(stderr, "MS_sleep(%d)\n", secs);*/

  sleeptime.tv_sec  = secs;
  sleeptime.tv_usec = 0;
  if (select( (int)NULL, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL, 
	      &sleeptime ) < 0) {
      D_PRINTF (stderr, "MS_sleep %lu returned early\n", secs);
  }
#endif
}

void
MS_usleep(unsigned int microsecs)
{
#ifdef _WIN32
  Sleep(microsecs / 1000);
#else
  struct timeval sleeptime;

  /*D_PRINTF(stderr, "MS_usleep(%d)\n", microsecs);*/

  sleeptime.tv_sec  = USECS_2_SECS(microsecs);
  sleeptime.tv_usec = microsecs % USECINSEC;
  if (select( (int)NULL, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL, 
	      &sleeptime ) < 0) {
      D_PRINTF (stderr, "MS_usleep %lu returned early\n", microsecs);
  }
#endif
}

/* strerror() */
#ifndef HAVE_STRERROR
/* strerror is not available on SunOS 4.1.3 and others */
extern int sys_nerr;
extern char *sys_errlist[];
extern int errno;

char *strerror(int errnum)
{ 
    if (errnum<sys_nerr) {
        return(sys_errlist[errnum]);
    }

    return(NULL);
}

#endif /* strerror() */

/* stub routines for NT */ 

#ifdef WIN32
#include <winsock.h>
#include <process.h>

int getpid(void) {

    return GetCurrentThreadId();
}

#include <sys/timeb.h>		/* For prototype of "_ftime()" */
/*
 * gettimeofday() --  gets the current time in elapsed seconds and
 *                     microsends since GMT Jan 1, 1970.
 *
 * ARGUMENTS: - Pointer to a timeval struct to return the time into
 *
 * RETURN CODES: -  0 on success
 *                 -1 on failure
 */
int gettimeofday(struct timeval *curTimeP)
{
    struct _timeb  localTime;

    if (curTimeP == (struct timeval *) NULL) {
	 errno = EFAULT;
	 return (-1);
    }

   /*
    *  Compute the elapsed time since Jan 1, 1970 by first
    *  obtaining the elapsed time from the system using the
    *  _ftime(..) call and then convert to the "timeval"
    *  equivalent.
    */

    _ftime(&localTime);

    curTimeP->tv_sec  = localTime.time;
    curTimeP->tv_usec = localTime.millitm * 1000;

    return(0);
}

void
crank_limits(void)
{}

void
setup_signal_handlers(void)
{}

int
sysdep_thread_create(THREAD_ID *id, thread_fn_t tfn, void *arg)
{
    if (_beginthread(tfn, NT_STACKSIZE, arg) == -1) {
	errexit(stderr, "_beginthread failed: %d", GetLastError());
	return -1;
    }
    return 0;
}

int
sysdep_thread_join(THREAD_ID id, int *pstatus)
{
	return 0;
}

#else /* !_WIN32 */

#define MAX_TRYFDS (64*1024)

void
crank_limits(void)
{
    struct rlimit rlim;
    int cur_lim;
    int rc;

#ifdef __OSF1__
    D_PRINTF(stderr, "attempting to enable support for up to 64k file descriptors\n");

    rc = setsysinfo(SSI_FD_NEWMAX, NULL, 0, NULL, 1);
    if (rc == -1) {
	perror("setsysinfo()");
    }
#endif /* __OSF1__ */

    D_PRINTF(stderr, "attempting to increase our hard limit (up to %d)\n", MAX_TRYFDS);

    rc = getrlimit(RLIMIT_NOFILE, &rlim);
    if (rc == -1) {
	returnerr(stderr, "getrlimit()");
	exit(-1);
    }

    for (cur_lim = rlim.rlim_max; cur_lim < MAX_TRYFDS; cur_lim += 1024) {
	rlim.rlim_max = cur_lim;

	rc = setrlimit(RLIMIT_NOFILE, &rlim);
	if (rc == -1) {
	    D_PRINTF (stderr, "setrlimit(RLIMIT_NOFILE, [rlim_max=%d]): errno=%d: %s\n",
		      rlim.rlim_max, errno, strerror(errno));
	    break;
	}
    }

    D_PRINTF(stderr, "attempting to increase our soft limit\n");

    rc = getrlimit(RLIMIT_NOFILE, &rlim);
    if (rc == -1) {
	returnerr(stderr, "getrlimit()");
	exit(-1);
    }
    rlim.rlim_cur = rlim.rlim_max;

    rc = setrlimit(RLIMIT_NOFILE, &rlim);
    if (rc == -1) {
	D_PRINTF (stderr, "setrlimit(RLIMIT_NOFILE, [rlim_cur=%d]): errno=%d: %s\n",
		  rlim.rlim_cur, errno, strerror(errno));
	exit(-1);
    }
    getrlimit(RLIMIT_NOFILE, &rlim);
    if (rlim.rlim_cur < 256) {		/* show if lower than docs suggest */
	returnerr (stderr, "RLIMIT_NOFILE = %d.  max processes/threads ~ %d\n",
		   rlim.rlim_cur, rlim.rlim_cur-10);
    } else {
	D_PRINTF (stderr, "RLIMIT_NOFILE = %d.  max processes/threads ~ %d\n",
		   rlim.rlim_cur, rlim.rlim_cur-10);
    }
}

static void
nullHandler(int sig)
{
    /* Dont do anything, (trap SIGPIPE) */
    return;
}

static void
alarmHandler(int sig)
{
    /* Dont do anything, mainly break system calls */
    if (gf_timeexpired < EXIT_SOON) gf_timeexpired = EXIT_SOON;
    return;
}

static void
hupHandler(int sig)
{
    if (gf_timeexpired < EXIT_SOON) {	/* first time, go to clean stop */
	beginShutdown ();
    } else if (gf_timeexpired < EXIT_FAST) {	/* second time, faster */
	gf_timeexpired = EXIT_FAST;
    } else if (gf_timeexpired < EXIT_FASTEST) {	/* second time, fastest */
	gf_timeexpired = EXIT_FASTEST;
    } else {				/* third time, exit */
	exit (sig);
    }
}
    
#if 0					/* not used */
/* Received a signal that a child process has exited */
void
childHandler(int sig)
{
  int status;

  /*D_PRINTF(stderr, "A child process has exited\n" );*/
  while (wait3(&status, WNOHANG, (struct rusage *)0) > 0) {
      /* do nothing */
      /*D_PRINTF(stderr, "wait3() says %d died\n", status);;*/
  }
}
#endif

void
setup_signal_handlers(void)
{
    /* We will loop until the alarm goes off (should abort system calls). */
#ifdef __LINUX__
    {					/* setup signal handler */
	struct	sigaction	sig;
	sig.sa_flags = 0;
	sig.sa_restorer = 0;
	sigemptyset (&sig.sa_mask);

	sig.sa_handler = alarmHandler;
	sigaction (SIGALRM, &sig, NULL);

	sig.sa_handler = hupHandler;
	sigaction (SIGHUP, &sig, NULL);
	sigaction (SIGTERM, &sig, NULL);

	sig.sa_handler = nullHandler;
	sigaction (SIGPIPE, &sig, NULL);

    }
#else
    sigset(SIGALRM, alarmHandler);
    sigset(SIGHUP, hupHandler);
    sigset(SIGTERM, hupHandler);
    sigset(SIGPIPE, nullHandler);
#endif
}

int
sysdep_thread_create(THREAD_ID *id, thread_fn_t tfn, void *arg)
{
#ifdef USE_PTHREADS
    int ret;
    pthread_attr_t attr;

    if ((ret=pthread_attr_init(&attr)) != 0) {
	returnerr(stderr, "pthread_attr_init() ret=%d errno=%d: %s\n",
		  ret, errno, strerror(errno));
	return -1;
    }

#if 0 /* the default thread attributes */
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
    pthread_attr_setstackaddr(&attr, NULL); /* allocated by system */
    pthread_attr_setstacksize(&attr, NULL); /* 1 MB */
    pthread_attr_setschedparam(&attr, _PARENT_); /* priority of parent */
    pthread_attr_setschedpolicy(&attr, SCHED_OTHER); /* determined by system */
    /* also have SCHED_FIFO and SCHED_RR */
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
#endif
#ifdef __AIX__
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_UNDETACHED);
#else
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#endif

#ifdef THREADS_SCOPE_SYSTEM
    /* bound threads, one thread per LWP */
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#endif

    if ((ret=pthread_create(id, &attr, tfn, arg)) != 0) {
	returnerr(stderr, "pthread_create() failed ret=%d errno=%d: %s\n",
		  ret, errno, strerror(errno));
	return -1;
    }
    if ((ret=pthread_attr_destroy(&attr)) != 0) {
	returnerr(stderr, "pthread_attr_destroy() ret=%d errno=%d: %s\n",
		  ret, errno, strerror(errno));
	return -1;
    }
#endif
    return 0;
}

int
sysdep_thread_join(THREAD_ID id, int *pstatus)
{
    int ret;

    for (;;) {
	errno = 0;
	*pstatus = 0;
	if ((ret = pthread_join(id, (void **)pstatus)) == 0)
	    break;
	D_PRINTF(stderr, "pthread_join(%d) error ret=%d status=%d: errno=%d: %s\n",
		 id, ret, *pstatus, errno, strerror(errno));
	
    }
    return 0;
}

#endif /* WIN32 */
