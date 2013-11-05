/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:sw=4:et:ts=8:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The linux glibc hides part of sigaction if _POSIX_SOURCE is defined
#if defined(linux)
#undef _POSIX_SOURCE
#undef _SVID_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

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
#include <sys/syscall.h>
#include <ucontext.h>
#include <execinfo.h>

#include "libmalloc.h"
#include "jprof.h"
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

// Must define before including jprof.h
void *moz_xmalloc(size_t size)
{
    return malloc(size);
}

void moz_xfree(void *mem)
{
    free(mem);
}

#ifdef NTO
#include <sys/link.h>
extern r_debug _r_debug;
#else
#include <link.h>
#endif

#define USE_GLIBC_BACKTRACE 1
// To debug, use #define JPROF_STATIC
#define JPROF_STATIC //static

static int gLogFD = -1;
static pthread_t main_thread;

static void startSignalCounter(unsigned long millisec);
static int enableRTCSignals(bool enable);


//----------------------------------------------------------------------
// replace use of atexit()

static void DumpAddressMap();

struct JprofShutdown {
    JprofShutdown() {}
    ~JprofShutdown() {
        DumpAddressMap();
    }
};

static void RegisterJprofShutdown() {
    // This instanciates the dummy class above, and will trigger the class
    // destructor when libxul is unloaded. This is equivalent to atexit(),
    // but gracefully handles dlclose().
    static JprofShutdown t;
}

#if defined(i386) || defined(_i386) || defined(__x86_64__)
JPROF_STATIC void CrawlStack(malloc_log_entry* me,
                             void* stack_top, void* top_instr_ptr)
{
#if USE_GLIBC_BACKTRACE
    // This probably works on more than x86!  But we need a way to get the
    // top instruction pointer, which is kindof arch-specific
    void *array[500];
    int cnt, i;
    u_long numpcs = 0;
    bool tracing = false;

    // This is from glibc.  A more generic version might use
    // libunwind and/or CaptureStackBackTrace() on Windows
    cnt = backtrace(&array[0],sizeof(array)/sizeof(array[0]));

    // StackHook->JprofLog->CrawlStack
    // Then we have sigaction, which replaced top_instr_ptr
    array[3] = top_instr_ptr;
    for (i = 3; i < cnt; i++)
    {
        me->pcs[numpcs++] = (char *) array[i];
    }
    me->numpcs = numpcs;

#else
  // original code - this breaks on many platforms
  void **bp;
#if defined(__i386)
  __asm__( "movl %%ebp, %0" : "=g"(bp));
#elif defined(__x86_64__)
  __asm__( "movq %%rbp, %0" : "=g"(bp));
#else
  // It would be nice if this worked uniformly, but at least on i386 and
  // x86_64, it stopped working with gcc 4.1, because it points to the
  // end of the saved registers instead of the start.
  bp = __builtin_frame_address(0);
#endif
  u_long numpcs = 0;
  bool tracing = false;

  me->pcs[numpcs++] = (char*) top_instr_ptr;

  while (numpcs < MAX_STACK_CRAWL) {
    void** nextbp = (void**) *bp++;
    void* pc = *bp;
    if (nextbp < bp) {
      break;
    }
    if (tracing) {
      // Skip the signal handling.
      me->pcs[numpcs++] = (char*) pc;
    }
    else if (pc == top_instr_ptr) {
      tracing = true;
    }
    bp = nextbp;
  }
  me->numpcs = numpcs;
#endif
}
#endif

//----------------------------------------------------------------------

static int rtcHz;
static int rtcFD = -1;
static bool circular = false;

#if defined(linux) || defined(NTO)
static void DumpAddressMap()
{
  // Turn off the timer so we don't get interrupts during shutdown
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

static bool was_paused = true;

JPROF_STATIC void JprofBufferDump();
JPROF_STATIC void JprofBufferClear();

static void ClearProfilingHook(int signum)
{
    if (circular) {
        JprofBufferClear();
        puts("Jprof: cleared circular buffer.");
    }
}

static void EndProfilingHook(int signum)
{
    if (circular)
        JprofBufferDump();

    DumpAddressMap();
    was_paused = true;
    puts("Jprof: profiling paused.");
}



//----------------------------------------------------------------------
// proper usage would be a template, including the function to find the
// size of an entry, or include a size header explicitly to each entry.
#if defined(linux)
#define DUMB_LOCK()   pthread_mutex_lock(&mutex);
#define DUMB_UNLOCK() pthread_mutex_unlock(&mutex);
#else
#define DUMB_LOCK()   FIXME()
#define DUMB_UNLOCK() FIXME()
#endif


class DumbCircularBuffer
{
public:
    DumbCircularBuffer(size_t init_buffer_size) {
        used = 0;
        buffer_size = init_buffer_size;
        buffer = (unsigned char *) malloc(buffer_size);
        head = tail = buffer;

#if defined(linux)
        pthread_mutexattr_t mAttr;
        pthread_mutexattr_settype(&mAttr, PTHREAD_MUTEX_RECURSIVE_NP);
        pthread_mutex_init(&mutex, &mAttr);
        pthread_mutexattr_destroy(&mAttr);
#endif
    }
    ~DumbCircularBuffer() {
        free(buffer);
#if defined(linux)
        pthread_mutex_destroy (&mutex);
#endif
    }

    void clear() {
        DUMB_LOCK();
        head = tail;
        used = 0;
        DUMB_UNLOCK();
    }

    bool empty() {
        return head == tail;
    }

    size_t space_available() {
        size_t result;
        DUMB_LOCK();
        if (tail > head)
            result = buffer_size - (tail-head) - 1;
        else
            result = head-tail - 1;
        DUMB_UNLOCK();
        return result;
    }

    void drop(size_t size) {
        // assumes correctness!
        DUMB_LOCK();
        head += size;
        if (head >= &buffer[buffer_size])
            head -= buffer_size;
        used--;
        DUMB_UNLOCK();
    }

    bool insert(void *data, size_t size) {
        // can fail if not enough space in the entire buffer
        DUMB_LOCK();
        if (space_available() < size)
            return false;

        size_t max_without_wrap = &buffer[buffer_size] - tail;
        size_t initial = size > max_without_wrap ? max_without_wrap : size;
#if DEBUG_CIRCULAR
        fprintf(stderr,"insert(%d): max_without_wrap %d, size %d, initial %d\n",used,max_without_wrap,size,initial);
#endif
        memcpy(tail,data,initial);
        tail += initial;
        data = ((char *)data)+initial;
        size -= initial;
        if (size != 0) {
#if DEBUG_CIRCULAR
            fprintf(stderr,"wrapping by %d bytes\n",size);
#endif
            memcpy(buffer,data,size);
            tail = &(((unsigned char *)buffer)[size]);
        }
            
        used++;
        DUMB_UNLOCK();

        return true;
    }

    // for external access to the buffer (saving)
    void lock() {
        DUMB_LOCK();
    }

    void unlock() {
        DUMB_UNLOCK();
    }

    // XXX These really shouldn't be public...
    unsigned char *head;
    unsigned char *tail;
    unsigned int used;
    unsigned char *buffer;
    size_t buffer_size;

private:
    pthread_mutex_t mutex;
};

class DumbCircularBuffer *JprofBuffer;

JPROF_STATIC void
JprofBufferInit(size_t size)
{
    JprofBuffer = new DumbCircularBuffer(size);
}

JPROF_STATIC void
JprofBufferClear()
{
    fprintf(stderr,"Told to clear JPROF circular buffer\n");
    JprofBuffer->clear();
}

JPROF_STATIC size_t
JprofEntrySizeof(malloc_log_entry *me)
{
    return offsetof(malloc_log_entry, pcs) + me->numpcs*sizeof(char*);
}

JPROF_STATIC void
JprofBufferAppend(malloc_log_entry *me)
{
    size_t size = JprofEntrySizeof(me);

    do {
        while (JprofBuffer->space_available() < size &&
               JprofBuffer->used > 0) {
#if DEBUG_CIRCULAR
            fprintf(stderr,"dropping entry: %d in use, %d free, need %d, size_to_free = %d\n",
                    JprofBuffer->used,JprofBuffer->space_available(),size,JprofEntrySizeof((malloc_log_entry *) JprofBuffer->head));
#endif
            JprofBuffer->drop(JprofEntrySizeof((malloc_log_entry *) JprofBuffer->head));
        }
        if (JprofBuffer->space_available() < size)
            return;

    } while (!JprofBuffer->insert(me,size));
}

JPROF_STATIC void
JprofBufferDump()
{
    JprofBuffer->lock();
#if DEBUG_CIRCULAR
    fprintf(stderr,"dumping JP_CIRCULAR buffer, %d of %d bytes\n",
            JprofBuffer->tail > JprofBuffer->head ? 
              JprofBuffer->tail - JprofBuffer->head :
              JprofBuffer->buffer_size + JprofBuffer->tail - JprofBuffer->head,
            JprofBuffer->buffer_size);
#endif
    if (JprofBuffer->tail >= JprofBuffer->head) {
        write(gLogFD, JprofBuffer->head, JprofBuffer->tail - JprofBuffer->head);
    } else {
        write(gLogFD, JprofBuffer->head, &(JprofBuffer->buffer[JprofBuffer->buffer_size]) - JprofBuffer->head);
        write(gLogFD, JprofBuffer->buffer, JprofBuffer->tail - JprofBuffer->buffer);
    }
    JprofBuffer->clear();
    JprofBuffer->unlock();
}

//----------------------------------------------------------------------

JPROF_STATIC void
JprofLog(u_long aTime, void* stack_top, void* top_instr_ptr)
{
  // Static is simply to make debugging tolerable
  static malloc_log_entry me;

  me.delTime = aTime;
  me.thread = syscall(SYS_gettid); //gettid();
  if (was_paused) {
      me.flags = JP_FIRST_AFTER_PAUSE;
      was_paused = 0;
  } else {
      me.flags = 0;
  }

  CrawlStack(&me, stack_top, top_instr_ptr);

#ifndef NTO
  if (circular) {
      JprofBufferAppend(&me);
  } else {
      write(gLogFD, &me, JprofEntrySizeof(&me));
  }
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
static void startSignalCounter(unsigned long millisec)
{
    struct itimerval tvalue;

    tvalue.it_interval.tv_sec = 0;
    tvalue.it_interval.tv_usec = 0;
    tvalue.it_value.tv_sec = millisec/1000;
    tvalue.it_value.tv_usec = (millisec%1000)*1000;

    if (realTime) {
	setitimer(ITIMER_REAL, &tvalue, NULL);
    } else {
    	setitimer(ITIMER_PROF, &tvalue, NULL);
    }
}

static long timerMilliSec = 50;

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
    static bool enabled = false;
    if (enabled == enable) {
        return 0;
    }
    enabled = enable;
    
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

JPROF_STATIC void StackHook(
int signum,
siginfo_t *info,
void *ucontext)
{
    static struct timeval tFirst;
    static int first=1;
    size_t millisec = 0;

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
            millisec = 0;
        }
    } else {
#if defined(linux)
        if (rtcHz) {
            enableRTCSignals(true);
        } else
#endif
        {
            struct timeval tNow;
            gettimeofday(&tNow, 0);
            double usec = 1e6*(tNow.tv_sec - tFirst.tv_sec);
            usec += (tNow.tv_usec - tFirst.tv_usec);
            millisec = static_cast<size_t>(usec*1e-3);
        }
    }

    gregset_t &gregs = ((ucontext_t*)ucontext)->uc_mcontext.gregs;
#ifdef __x86_64__
    JprofLog(millisec, (void*)gregs[REG_RSP], (void*)gregs[REG_RIP]);
#else
    JprofLog(millisec, (void*)gregs[REG_ESP], (void*)gregs[REG_EIP]);
#endif

    if (!rtcHz)
        startSignalCounter(timerMilliSec);
}

NS_EXPORT_(void) setupProfilingStuff(void)
{
    static int gFirstTime = 1;
    char filename[2048]; // XXX fix

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
         *   JP_FILENAME -> base filename to use when saving logs.  Note that
         *               this does not affect the mapfile.
         *   JP_CIRCULAR -> use a circular buffer of size N, write/clear on SIGUSR1
         *
         * JPROF_SLAVE is set if this is not the first process.
	*/

        circular = false;

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
                double tmp = strtod(delay+strlen("JP_PERIOD="), NULL);
                if (tmp>=1e-3) {
		    timerMilliSec = static_cast<unsigned long>(1000 * tmp);
                } else {
                    fprintf(stderr,
                            "JP_PERIOD of %g less than 0.001 (1ms), using 1ms\n",
                            tmp);
                    timerMilliSec = 1;
                }
	    }

	    char *circular_op = strstr(tst,"JP_CIRCULAR=");
	    if(circular_op) {
                size_t size = atol(circular_op+strlen("JP_CIRCULAR="));
                if (size < 1000) {
                    fprintf(stderr,
                            "JP_CIRCULAR of %d less than 1000, using 10000\n",
                            size);
                    size = 10000;
                }
                JprofBufferInit(size);
                fprintf(stderr,"JP_CIRCULAR buffer of %d bytes\n",size);
                circular = true;
	    }

	    char *first = strstr(tst, "JP_FIRST=");
	    if(first) {
                firstDelay = atol(first+strlen("JP_FIRST="));
	    }

            char *rtc = strstr(tst, "JP_RTC_HZ=");
            if (rtc) {
#if defined(linux)
                rtcHz = atol(rtc+strlen("JP_RTC_HZ="));
                timerMilliSec = 0; /* This makes JP_FIRST work right. */
                realTime = 1; /* It's the _R_TC and all.  ;) */

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
            char *f = strstr(tst,"JP_FILENAME=");
            if (f)
                f = f + strlen("JP_FILENAME=");
            else
                f = M_LOGFILE;

            char *is_slave = getenv("JPROF_SLAVE");
            if (!is_slave)
                setenv("JPROF_SLAVE","", 0);

            int pid = syscall(SYS_gettid); //gettid();
            if (is_slave)
                snprintf(filename,sizeof(filename),"%s-%d",f,pid);
            else
                snprintf(filename,sizeof(filename),"%s",f);

            // XXX FIX! inherit current capture state!
	}

	if(!doNotStart) {

	    if(gLogFD<0) {
		gLogFD = open(filename, O_CREAT | O_WRONLY | append, 0666);
		if(gLogFD<0) {
		    fprintf(stderr, "Unable to create " M_LOGFILE);
		    perror(":");
		} else {
		    struct sigaction action;
		    sigset_t mset;

		    // Dump out the address map when we terminate
		    RegisterJprofShutdown();

		    main_thread = pthread_self();
                    //fprintf(stderr,"jprof: main_thread = %u\n",
                    //        (unsigned int)main_thread);

                    // FIX!  probably should block these against each other
                    // Very unlikely.
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
                    }

                    if (!rtcHz || firstDelay != 0)
#endif
                    {
                        if (realTime) {
                            sigaction(SIGALRM, &action, NULL);
                        }
                    }
                    // enable PROF in all cases to simplify JP_DEFER/pause/restart
                    sigaction(SIGPROF, &action, NULL);

		    // make it so a SIGUSR1 will stop the profiling
		    // Note:  It currently does not close the logfile.
		    // This could be configurable (so that it could
		    // later be reopened).

		    struct sigaction stop_action;
		    stop_action.sa_handler = EndProfilingHook;
		    stop_action.sa_mask  = mset;
		    stop_action.sa_flags = SA_RESTART;
		    sigaction(SIGUSR1, &stop_action, NULL);

		    // make it so a SIGUSR2 will clear the circular buffer

		    stop_action.sa_handler = ClearProfilingHook;
		    stop_action.sa_mask  = mset;
		    stop_action.sa_flags = SA_RESTART;
		    sigaction(SIGUSR2, &stop_action, NULL);

                    printf("Jprof: Initialized signal handler and set "
                           "timer for %lu %s, %d s "
                           "initial delay\n",
                           rtcHz ? rtcHz : timerMilliSec, 
                           rtcHz ? "Hz" : "ms",
                           firstDelay);

		    if(startTimer) {
#if defined(linux)
                        /* If we have an initial delay we can just use
                           startSignalCounter to set up a timer to fire the
                           first stackHook after that delay.  When that happens
                           we'll go and switch to RTC profiling. */
                        if (rtcHz && firstDelay == 0) {
                            puts("Jprof: enabled RTC signals");
                            enableRTCSignals(true);
                        } else
#endif
                        {
                            puts("Jprof: started timer");
                            startSignalCounter(firstDelay*1000 + timerMilliSec);
                        }
		    }
		}
	    }
	}
    } else {
        printf("setupProfilingStuff() called multiple times\n");
    }
}
