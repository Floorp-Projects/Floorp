/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:sw=4:et:ts=8:
// The contents of this file are subject to the Mozilla Public License
// Version 1.1 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

// Portions Copyright 1999 by Jim Nance

// Additional contributors:
//  L. David Baron - JP_REALTIME, JPROF_PTHREAD_HACK, and SIGUSR1 handling
//  Mike Shaver - JP_RTC_HZ support

// The linux glibc hides part of sigaction if _POSIX_SOURCE is defined
#if defined(linux)
#undef _POSIX_SOURCE
#undef _SVID_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

// Some versions of glibc (i.e., the one that comes with RedHat 6.0 rather
// than 6.1) seem to do things a bit differently when libpthread is involved.
// If things don't work for you, try defining this:
//#define JPROF_PTHREAD_HACK

#include <errno.h>
#if defined(linux)
#include <linux/rtc.h>
#include <pthread.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "libmalloc.h"
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <dlfcn.h>


#ifdef NTO
#include <sys/link.h>
extern r_debug _r_debug;
#else
#include <link.h>
#endif

#ifdef NTO
#define JB_BP 0x08
#include <setjmp.h>
#endif


static int gLogFD = -1;
static pthread_t main_thread;

static void startSignalCounter(unsigned long milisec);
static int enableRTCSignals(bool enable);


//----------------------------------------------------------------------

#if defined(i386) || defined(_i386)
static void CrawlStack(malloc_log_entry* me, jmp_buf jb, char* first)
{
#ifdef NTO
  u_long* bp = (u_long*) (jb[0].__jmpbuf_un.__savearea[JB_BP]);
#else
  u_long* bp = (u_long*) (jb[0].__jmpbuf[JB_BP]);
#endif
  u_long numpcs = 0;

#ifdef JPROF_PTHREAD_HACK
  int skip = 3;
#else
  me->pcs[numpcs++] = first;

  // skip 2 frames: StackHook, __restore_rt.
  // The next frame is the frame _above_ |first|.
  int skip = 2;
#endif
  while (numpcs < MAX_STACK_CRAWL) {
    u_long* nextbp = (u_long*) *bp++;
    u_long pc = *bp;
#ifdef JPROF_PTHREAD_HACK
    if ((pc < 0x08000000) || ((pc > 0x7fffffff) && (skip <= 0)) || (nextbp < bp)) {
#else
    if ((nextbp < bp)) {
#endif
      break;
    }
    if (--skip < 0) {
      me->pcs[numpcs++] = (char*) pc;
    }
    bp = nextbp;
  }
  me->numpcs = numpcs;
}
#endif

//----------------------------------------------------------------------

static int rtcHz;
static int rtcFD = -1;

#if defined(linux) || defined(NTO)
static void DumpAddressMap()
{
  // Turn off the timer so we dont get interrupts during shutdown
#if defined(linux)
  if (rtcHz) {
    enableRTCSignals(false);
  } else
#endif
  {
    startSignalCounter(0);
  }

  int mfd = open(M_MAPFILE, O_CREAT|O_WRONLY|O_TRUNC, 0666);
  if (mfd >= 0) {
    malloc_map_entry mme;
    link_map* map = _r_debug.r_map;
    while (NULL != map) {
      if (map->l_name && *map->l_name) {
	mme.nameLen = strlen(map->l_name);
	mme.address = map->l_addr;
	write(mfd, &mme, sizeof(mme));
	write(mfd, map->l_name, mme.nameLen);
#if 0
	write(1, map->l_name, mme.nameLen);
	write(1, "\n", 1);
#endif
      }
      map = map->l_next;
    }
    close(mfd);
  }
}
#endif

static void EndProfilingHook(int signum)
{
    DumpAddressMap();
    puts("Jprof: profiling paused.");
}

//----------------------------------------------------------------------

static void
Log(u_long aTime, char *first)
{
  // Static is simply to make debugging tollerable
  static malloc_log_entry me;

  me.delTime = aTime;

  jmp_buf jb;
  setjmp(jb);
  CrawlStack(&me, jb, first);

#ifndef NTO
  write(gLogFD, &me, offsetof(malloc_log_entry, pcs) + me.numpcs*sizeof(char*));
#else
  printf("Neutrino is missing the pcs member of malloc_log_entry!! \n");
#endif
}

static int realTime;

/* Lets interrupt at 10 Hz.  This is so my log files don't get too large.
 * This can be changed to a faster value latter.  This timer is not
 * programmed to reset, even though it is capable of doing so.  This is
 * to keep from getting interrupts from inside of the handler.
*/
static void startSignalCounter(unsigned long milisec)
{
    struct itimerval tvalue;

    tvalue.it_interval.tv_sec = 0;
    tvalue.it_interval.tv_usec = 0;
    tvalue.it_value.tv_sec = milisec/1000;
    tvalue.it_value.tv_usec = (milisec%1000)*1000;

    if (realTime) {
	setitimer(ITIMER_REAL, &tvalue, NULL);
    } else {
    	setitimer(ITIMER_PROF, &tvalue, NULL);
    }
}

static long timerMiliSec = 50;

#if defined(linux)
static int setupRTCSignals(int hz, struct sigaction *sap)
{
    /* global */ rtcFD = open("/dev/rtc", O_RDONLY);
    if (rtcFD < 0) {
        perror("JPROF_RTC setup: open(\"/dev/rtc\", O_RDONLY)");
        return 0;
    }

    if (sigaction(SIGIO, sap, NULL) == -1) {
        perror("JPROF_RTC setup: sigaction(SIGIO)");
        return 0;
    }

    if (ioctl(rtcFD, RTC_IRQP_SET, hz) == -1) {
        perror("JPROF_RTC setup: ioctl(/dev/rtc, RTC_IRQP_SET, $JPROF_RTC_HZ)");
        return 0;
    }

    if (ioctl(rtcFD, RTC_PIE_ON, 0) == -1) {
        perror("JPROF_RTC setup: ioctl(/dev/rtc, RTC_PIE_ON)");
        return 0;
    }

    if (fcntl(rtcFD, F_SETSIG, 0) == -1) {
        perror("JPROF_RTC setup: fcntl(/dev/rtc, F_SETSIG, 0)");
        return 0;
    }

    if (fcntl(rtcFD, F_SETOWN, getpid()) == -1) {
        perror("JPROF_RTC setup: fcntl(/dev/rtc, F_SETOWN, getpid())");
        return 0;
    }

    return 1;
}

static int enableRTCSignals(bool enable)
{
    int flags = fcntl(rtcFD, F_GETFL);
    if (flags < 0) {
        perror("JPROF_RTC setup: fcntl(/dev/rtc, F_GETFL)");
        return 0;
    }

    if (enable) {
        flags |= FASYNC;
    } else {
        flags &= ~FASYNC;
    }

    if (fcntl(rtcFD, F_SETFL, flags) == -1) {
        if (enable) {
            perror("JPROF_RTC setup: fcntl(/dev/rtc, F_SETFL, flags | FASYNC)");
        } else {
            perror("JPROF_RTC setup: fcntl(/dev/rtc, F_SETFL, flags & ~FASYNC)");
        }            
        return 0;
    }

    return 1;
}
#endif

static void StackHook(
int signum,
siginfo_t *info,
void *mystry)
{
    static struct timeval tFirst;
    static int first=1;
    size_t milisec = 0;

#if defined(linux)
    if (rtcHz && pthread_self() != main_thread) {
      // Only collect stack data on the main thread, for now.
      return;
    }
#endif

    if(first && !(first=0)) {
        puts("Jprof: received first signal");
#if defined(linux)
        if (rtcHz) {
            enableRTCSignals(true);
        } else
#endif
        {
            gettimeofday(&tFirst, 0);
            milisec = 0;
        }
    } else {
	struct timeval tNow;
        gettimeofday(&tNow, 0);
	double usec = 1e6*(tNow.tv_sec - tFirst.tv_sec);
	usec += (tNow.tv_usec - tFirst.tv_usec);
	milisec = static_cast<size_t>(usec*1e-3);
    }

#ifdef JPROF_PTHREAD_HACK
    Log(milisec, NULL);
#else
    // The mystry[19] thing is a hack to figure out where we were called from.
    // By playing around with the debugger it looks like [19] contains the
    // information I need.
    // it's really ((ucontext_t *)mystry)->uc_mcontext.gregs[14] which is
    // the EIP register when the handler was called
    Log(milisec, ((char**)mystry)[19]);
#endif
    if (!rtcHz)
        startSignalCounter(timerMiliSec);
}

void setupProfilingStuff(void)
{
    static int gFirstTime = 1;
    if(gFirstTime && !(gFirstTime=0)) {
	int  startTimer = 1;
	int  doNotStart = 1;
	int  firstDelay = 0;
        int  append = O_TRUNC;
        char *tst  = getenv("JPROF_FLAGS");

	/* Options from JPROF_FLAGS environment variable:
	 *   JP_DEFER  -> Wait for a SIGPROF (or SIGALRM, if JP_REALTIME
	 *               is set) from userland before starting
	 *               to generate them internally
	 *   JP_START  -> Install the signal handler
	 *   JP_PERIOD -> Time between profiler ticks
	 *   JP_FIRST  -> Extra delay before starting
	 *   JP_REALTIME -> Take stack traces in intervals of real time
	 *               rather than time used by the process (and the
	 *               system for the process).  This is useful for
	 *               finding time spent by the X server.
         *   JP_APPEND -> Append to jprof-log rather than overwriting it.
         *               This is somewhat risky since it depends on the
         *               address map staying constant across multiple runs.
	*/
	if(tst) {
	    if(strstr(tst, "JP_DEFER"))
	    {
		doNotStart = 0;
		startTimer = 0;
	    }
	    if(strstr(tst, "JP_START")) doNotStart = 0;
	    if(strstr(tst, "JP_REALTIME")) realTime = 1;
	    if(strstr(tst, "JP_APPEND")) append = O_APPEND;

	    char *delay = strstr(tst,"JP_PERIOD=");
	    if(delay) {
	        double tmp = strtod(delay+10, NULL);
		if(tmp>1e-3) {
		    timerMiliSec = static_cast<unsigned long>(1000 * tmp);
		}
	    }

	    char *first = strstr(tst, "JP_FIRST=");
	    if(first) {
	        firstDelay = atol(first+9);
	    }

            char *rtc = strstr(tst, "JP_RTC_HZ=");
            if (rtc) {
#if defined(linux)
                rtcHz = atol(rtc+10);

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)

                if (!IS_POWER_OF_TWO(rtcHz) || rtcHz < 2) {
                    fprintf(stderr, "JP_RTC_HZ must be power of two and >= 2, "
                            "but %d was provided; using default of 2048\n",
                            rtcHz);
                    rtcHz = 2048;
                }
#else
                fputs("JP_RTC_HZ found, but RTC profiling only supported on "
                      "Linux!\n", stderr);
                  
#endif
            }
	}

	if(!doNotStart) {

	    if(gLogFD<0) {
		gLogFD = open(M_LOGFILE, O_CREAT | O_WRONLY | append, 0666);
		if(gLogFD<0) {
		    fprintf(stderr, "Unable to create " M_LOGFILE);
		    perror(":");
		} else {
		    struct sigaction action;
		    sigset_t mset;

		    // Dump out the address map when we terminate
		    atexit(DumpAddressMap);

		    main_thread = pthread_self();

		    sigemptyset(&mset);
		    action.sa_handler = NULL;
		    action.sa_sigaction = StackHook;
		    action.sa_mask  = mset;
		    action.sa_flags = SA_RESTART | SA_SIGINFO;
#if defined(linux)
                    if (rtcHz) {
                        if (!setupRTCSignals(rtcHz, &action)) {
                            fputs("jprof: Error initializing RTC, NOT "
                                  "profiling\n", stderr);
                            return;
                        }
                    } else
#endif
                    if (realTime) {
                        sigaction(SIGALRM, &action, NULL);
                    } else {
                        sigaction(SIGPROF, &action, NULL);
                    }

		    // make it so a SIGUSR1 will stop the profiling
		    // Note:  It currently does not close the logfile.
		    // This could be configurable (so that it could
		    // later be reopened).

		    struct sigaction stop_action;
		    stop_action.sa_handler = EndProfilingHook;
		    stop_action.sa_mask  = mset;
		    stop_action.sa_flags = SA_RESTART;
		    sigaction(SIGUSR1, &stop_action, NULL);

                    printf("Jprof: Initialized signal handler and set "
                           "timer for %lu %s, %d s "
                           "initial delay\n",
                           rtcHz ? rtcHz : timerMiliSec, 
                           rtcHz ? "Hz" : "ms",
                           firstDelay);

		    if(startTimer) {
#if defined(linux)
                        if (rtcHz) {
                            puts("Jprof: enabled RTC signals");
                            enableRTCSignals(true);
                        } else
#endif
                        {
                            puts("Jprof: started timer");
                            startSignalCounter(firstDelay*1000 + timerMiliSec);
                        }
		    }
		}
	    }
	}
    } else {
        printf("setupProfilingStuff() called multiple times\n");
    }
}
