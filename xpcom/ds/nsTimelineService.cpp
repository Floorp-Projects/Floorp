/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsTimelineService.h"
#include "prlong.h"
#include "prprf.h"
#include "prenv.h"
#include "plhash.h"
#include "prlock.h"
#include "prinit.h"
#include "prinrval.h"
#include "prthread.h"

#ifdef MOZ_TIMELINE

#define MAXINDENT 20

#ifdef XP_MAC
static PRIntervalTime initInterval = 0;
#endif

static PRFileDesc *timelineFD = PR_STDERR;
static PRBool gTimelineDisabled = PR_TRUE;

// Notes about threading:
// We avoid locks as we always use thread-local-storage.
// This means every other thread has its own private copy of
// data, and this thread can't re-enter (as our implemenation
// doesn't call back out anywhere).  Thus, we can avoid locks!
// TLS index
static const PRUintn BAD_TLS_INDEX = (PRUintn) -1;
static PRUintn gTLSIndex = BAD_TLS_INDEX;

class TimelineThreadData {
public:
    TimelineThreadData() : initTime(0), indent(0),
                           disabled(PR_TRUE), timers(nsnull) {}
    ~TimelineThreadData() {if (timers) PL_HashTableDestroy(timers);}
    PRTime initTime;
    PRHashTable *timers;
    int indent;
    PRBool disabled;
};

/* Implementation file */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsTimelineService, nsITimelineService)

static PRTime Now(void);

/*
 * Timer structure stored in a hash table to keep track of named
 * timers.
 */
class nsTimelineServiceTimer {
  public:
    nsTimelineServiceTimer();
    ~nsTimelineServiceTimer();
    void start();
    
    /*
     * Caller passes in "now" rather than having us calculate it so
     * that we can avoid including timer overhead in the time being
     * measured.
     */
    void stop(PRTime now);
    void reset();
    PRTime getAccum();
    PRTime getAccum(PRTime now);

  private:
    PRTime mAccum;
    PRTime mStart;
    PRInt32 mRunning;
    PRThread *mOwnerThread; // only used for asserts - could be #if MOZ_DEBUG
};

#define TIMER_CHECK_OWNER() \
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == mOwnerThread, \
                      "Timer used by non-owning thread")


nsTimelineServiceTimer::nsTimelineServiceTimer()
: mAccum(LL_ZERO), mStart(LL_ZERO), mRunning(0),
  mOwnerThread(PR_GetCurrentThread())
{
}

nsTimelineServiceTimer::~nsTimelineServiceTimer()
{
}

void nsTimelineServiceTimer::start()
{
    TIMER_CHECK_OWNER();
    if (!mRunning) {
        mStart = Now();
    }
    mRunning++;
}

void nsTimelineServiceTimer::stop(PRTime now)
{
    TIMER_CHECK_OWNER();
    mRunning--;
    if (mRunning == 0) {
        PRTime delta, accum;
        LL_SUB(delta, now, mStart);
        LL_ADD(accum, mAccum, delta);
        mAccum = accum;
    }
}

void nsTimelineServiceTimer::reset()
{
  TIMER_CHECK_OWNER();
  mStart = 0;
  mAccum = 0;
}

PRTime nsTimelineServiceTimer::getAccum()
{
    TIMER_CHECK_OWNER();
    PRTime accum;

    if (!mRunning) {
        accum = mAccum;
    } else {
        PRTime delta;
        LL_SUB(delta, Now(), mStart);
        LL_ADD(accum, mAccum, delta);
    }
    return accum;
}

PRTime nsTimelineServiceTimer::getAccum(PRTime now)
{
    TIMER_CHECK_OWNER();
    PRTime accum;

    if (!mRunning) {
        accum = mAccum;
    } else {
        PRTime delta;
        LL_SUB(delta, now, mStart);
        LL_ADD(accum, mAccum, delta);
    }
    return accum;
}

#ifdef XP_MAC
/*
 * PR_Now() on the Mac only gives us a resolution of seconds.  Using
 * PR_IntervalNow() gives us better resolution. with the drawback that 
 * the timeline is only good for about six hours.
 *
 * PR_IntervalNow() occasionally exhibits discontinuities on Windows,
 * so we only use it on the Mac.  Bleah!
 */
static PRTime Now(void)
{
    PRIntervalTime numTicks = PR_IntervalNow() - initInterval;
    PRTime now;
    LL_ADD(now, initTime, PR_IntervalToMilliseconds(numTicks) * 1000);
    return now;
}
#else
static PRTime Now(void)
{
    return PR_Now();
}
#endif

static TimelineThreadData *GetThisThreadData()
{
    NS_ABORT_IF_FALSE(gTLSIndex!=BAD_TLS_INDEX, "Our TLS not initialized");
    TimelineThreadData *new_data = nsnull;
    TimelineThreadData *data = (TimelineThreadData *)PR_GetThreadPrivate(gTLSIndex);
    if (data == nsnull) {
        // First request for this thread - allocate it.
        new_data = new TimelineThreadData();
        if (!new_data)
            goto done;

        // Fill it
        new_data->timers = PL_NewHashTable(100, PL_HashString, PL_CompareStrings,
                                 PL_CompareValues, NULL, NULL);
        if (new_data->timers==NULL)
            goto done;
        new_data->initTime = PR_Now();
        NS_WARN_IF_FALSE(!gTimelineDisabled,
                         "Why are we creating new state when disabled?");
        new_data->disabled = PR_FALSE;
        data = new_data;
        new_data = nsnull;
        PR_SetThreadPrivate(gTLSIndex, data);
    }
done:
    if (new_data) // eeek - error during creation!
        delete new_data;
    NS_WARN_IF_FALSE(data, "TimelineService could not get thread-local data");
    return data;
}

extern "C" {
  static void ThreadDestruct (void *data);
  static PRStatus TimelineInit(void);
}

void ThreadDestruct( void *data )
{
    if (data)
        delete (TimelineThreadData *)data;
}

/*
* PRCallOnceFN that initializes stuff for the timing service.
*/
static PRCallOnceType initonce;

PRStatus TimelineInit(void)
{
    char *timeStr;
    char *fileName;
    PRInt32 secs, msecs;
    PRFileDesc *fd;
    PRInt64 tmp1, tmp2;

    PRStatus status = PR_NewThreadPrivateIndex( &gTLSIndex, ThreadDestruct );
    NS_WARN_IF_FALSE(status==0, "TimelineService could not allocate TLS storage.");

    timeStr = PR_GetEnv("NS_TIMELINE_INIT_TIME");
#ifdef XP_MAC    
    initInterval = PR_IntervalNow();
#endif
    // NS_TIMELINE_INIT_TIME only makes sense for the main thread, so if it
    // exists, set it there.  If not, let normal thread management code take
    // care of setting the init time.
    if (timeStr != NULL && 2 == PR_sscanf(timeStr, "%d.%d", &secs, &msecs)) {
        PRTime &initTime = GetThisThreadData()->initTime;
        LL_MUL(tmp1, (PRInt64)secs, 1000000);
        LL_MUL(tmp2, (PRInt64)msecs, 1000);
        LL_ADD(initTime, tmp1, tmp2);
#ifdef XP_MAC
        initInterval -= PR_MicrosecondsToInterval(
            (PRUint32)(PR_Now() - initTime));
#endif
    }
    // Get the log file.
#ifdef XP_MAC
    fileName = "timeline.txt";
#else
    fileName = PR_GetEnv("NS_TIMELINE_LOG_FILE");
#endif
    if (fileName != NULL
        && (fd = PR_Open(fileName, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                         0666)) != NULL) {
        timelineFD = fd;
        PR_fprintf(fd,
                   "NOTE: due to asynchrony, the indentation that you see does"
                   " not necessarily correspond to nesting in the code.\n\n");
    }

    // Runtime disable of timeline
    if (PR_GetEnv("NS_TIMELINE_ENABLE"))
        gTimelineDisabled = PR_FALSE;
    return PR_SUCCESS;
}

static void ParseTime(PRTime tm, PRInt32& secs, PRInt32& msecs)
{
    PRTime llsecs, llmsecs, tmp;

    LL_DIV(llsecs, tm, 1000000);
    LL_MOD(tmp, tm, 1000000);
    LL_DIV(llmsecs, tmp, 1000);

    LL_L2I(secs, llsecs);
    LL_L2I(msecs, llmsecs);
}

static char *Indent(char *buf)
{
    int &indent = GetThisThreadData()->indent;
    int amount = indent;
    if (amount > MAXINDENT) {
        amount = MAXINDENT;
    }
    if (amount < 0) {
        amount = 0;
        indent = 0;
        PR_Write(timelineFD, "indent underflow!\n", 18);
    }
    while (amount--) {
        *buf++ = ' ';
    }
    return buf;
}

static void PrintTime(PRTime tm, const char *text, va_list args)
{
    PRInt32 secs, msecs;
    char pbuf[550], *pc, tbuf[550];

    ParseTime(tm, secs, msecs);

    // snprintf/write rather than fprintf because we don't want
    // messages from multiple threads to garble one another.
    pc = Indent(pbuf);
    PR_vsnprintf(pc, sizeof pbuf - (pc - pbuf), text, args);
    PR_snprintf(tbuf, sizeof tbuf, "%05d.%03d (%08p): %s\n",
                secs, msecs, PR_GetCurrentThread(), pbuf);
    PR_Write(timelineFD, tbuf, strlen(tbuf));
}

/*
 * Make this public if we need it.
 */
static nsresult NS_TimelineMarkV(const char *text, va_list args)
{
    PRTime elapsed,tmp;

    PR_CallOnce(&initonce, TimelineInit);

    TimelineThreadData *thread = GetThisThreadData();

    tmp = Now();
    LL_SUB(elapsed, tmp, thread->initTime);

    PrintTime(elapsed, text, args);

    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineForceMark(const char *text, ...)
{
    va_list args;
    va_start(args, text);
    NS_TimelineMarkV(text, args);

    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineMark(const char *text, ...)
{
    va_list args;
    va_start(args, text);

    PR_CallOnce(&initonce, TimelineInit);

    if (gTimelineDisabled)
        return NS_ERROR_NOT_AVAILABLE;

    TimelineThreadData *thread = GetThisThreadData();

    if (thread->disabled)
        return NS_ERROR_NOT_AVAILABLE;

    NS_TimelineMarkV(text, args);

    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineStartTimer(const char *timerName)
{
    PR_CallOnce(&initonce, TimelineInit);

    if (gTimelineDisabled)
        return NS_ERROR_NOT_AVAILABLE;

    TimelineThreadData *thread = GetThisThreadData();

    if (thread->timers == NULL)
        return NS_ERROR_FAILURE;
    if (thread->disabled)
        return NS_ERROR_NOT_AVAILABLE;

    nsTimelineServiceTimer *timer
        = (nsTimelineServiceTimer *)PL_HashTableLookup(thread->timers, timerName);
    if (timer == NULL) {
        timer = new nsTimelineServiceTimer;
        if (!timer)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableAdd(thread->timers, timerName, timer);
    }
    timer->start();
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineStopTimer(const char *timerName)
{
    if (gTimelineDisabled)
        return NS_ERROR_NOT_AVAILABLE;
    /*
     * Strange-looking now/timer->stop() interaction is to avoid
     * including time spent in TLS and PL_HashTableLookup in the
     * timer.
     */
    PRTime now = Now();

    TimelineThreadData *thread = GetThisThreadData();
    if (thread->timers == NULL)
        return NS_ERROR_FAILURE;
    if (thread->disabled)
        return NS_ERROR_NOT_AVAILABLE;
    nsTimelineServiceTimer *timer
        = (nsTimelineServiceTimer *)PL_HashTableLookup(thread->timers, timerName);
    if (timer == NULL) {
        return NS_ERROR_FAILURE;
    }

    timer->stop(now);

    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineMarkTimer(const char *timerName, const char *str)
{
    PR_CallOnce(&initonce, TimelineInit);

    if (gTimelineDisabled)
        return NS_ERROR_NOT_AVAILABLE;

    TimelineThreadData *thread = GetThisThreadData();
    if (thread->timers == NULL)
        return NS_ERROR_FAILURE;
    if (thread->disabled)
        return NS_ERROR_NOT_AVAILABLE;
    nsTimelineServiceTimer *timer
        = (nsTimelineServiceTimer *)PL_HashTableLookup(thread->timers, timerName);
    if (timer == NULL) {
        return NS_ERROR_FAILURE;
    }
    PRTime accum = timer->getAccum();

    char buf[500];
    PRInt32 sec, msec;
    ParseTime(accum, sec, msec);
    if (!str)
        PR_snprintf(buf, sizeof buf, "%s total: %d.%03d",
                    timerName, sec, msec);
    else
        PR_snprintf(buf, sizeof buf, "%s total: %d.%03d (%s)",
                    timerName, sec, msec, str);
    NS_TimelineMark(buf);

    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineResetTimer(const char *timerName)
{
    if (gTimelineDisabled)
        return NS_ERROR_NOT_AVAILABLE;

    TimelineThreadData *thread = GetThisThreadData();
    if (thread->timers == NULL)
        return NS_ERROR_FAILURE;
    if (thread->disabled)
        return NS_ERROR_NOT_AVAILABLE;
    nsTimelineServiceTimer *timer
        = (nsTimelineServiceTimer *)PL_HashTableLookup(thread->timers, timerName);
    if (timer == NULL) {
        return NS_ERROR_FAILURE;
    }

    timer->reset();
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineIndent()
{
    if (gTimelineDisabled)
        return NS_ERROR_NOT_AVAILABLE;

    TimelineThreadData *thread = GetThisThreadData();
    if (thread->disabled)
        return NS_ERROR_NOT_AVAILABLE;
    thread->indent++;
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineOutdent()
{
    if (gTimelineDisabled)
        return NS_ERROR_NOT_AVAILABLE;

    TimelineThreadData *thread = GetThisThreadData();
    if (thread->disabled)
        return NS_ERROR_NOT_AVAILABLE;
    thread->indent--;
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineEnter(const char *text)
{
    nsresult rv = NS_TimelineMark("%s...", text);
    if (NS_FAILED(rv)) {
        return rv;
    }
    return NS_TimelineIndent();
}

PR_IMPLEMENT(nsresult) NS_TimelineLeave(const char *text)
{
    nsresult rv = NS_TimelineOutdent();
    if (NS_FAILED(rv)) {
        return rv;
    }
    return NS_TimelineMark("...%s", text);
}

nsTimelineService::nsTimelineService()
{
  /* member initializers and constructor code */
}

/* void mark (in string text); */
NS_IMETHODIMP nsTimelineService::Mark(const char *text)
{
    return NS_TimelineMark(text);
}

/* void startTimer (in string timerName); */
NS_IMETHODIMP nsTimelineService::StartTimer(const char *timerName)
{
    return NS_TimelineStartTimer(timerName);
}

/* void stopTimer (in string timerName); */
NS_IMETHODIMP nsTimelineService::StopTimer(const char *timerName)
{
    return NS_TimelineStopTimer(timerName);
}

/* void markTimer (in string timerName); */
NS_IMETHODIMP nsTimelineService::MarkTimer(const char *timerName)
{
    return NS_TimelineMarkTimer(timerName);
}

/* void markTimerWithComment(in string timerName, in string comment); */
NS_IMETHODIMP nsTimelineService::MarkTimerWithComment(const char *timerName, const char *comment)
{
    return NS_TimelineMarkTimer(timerName, comment);
}

/* void resetTimer (in string timerName); */
NS_IMETHODIMP nsTimelineService::ResetTimer(const char *timerName)
{
    return NS_TimelineResetTimer(timerName);
}

/* void indent (); */
NS_IMETHODIMP nsTimelineService::Indent()
{
    return NS_TimelineIndent();
}

/* void outdent (); */
NS_IMETHODIMP nsTimelineService::Outdent()
{
    return NS_TimelineOutdent();
}

/* void enter (in string text); */
NS_IMETHODIMP nsTimelineService::Enter(const char *text)
{
    return NS_TimelineEnter(text);
}

/* void leave (in string text); */
NS_IMETHODIMP nsTimelineService::Leave(const char *text)
{
    return NS_TimelineLeave(text);
}

#endif /* MOZ_TIMELINE */
