/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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

#ifdef MOZ_TIMELINE

#define MAXINDENT 20

#ifdef XP_MAC
static PRIntervalTime initInterval = 0;
#endif

static PRTime initTime = 0;
static PRFileDesc *timelineFD = PR_STDERR;
static PRHashTable *timers;
static PRLock *timerLock;
static int indent;

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsTimelineService, nsITimelineService)

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
    PRTime getAccum();
    PRTime getAccum(PRTime now);

  private:
    PRTime mAccum;
    PRTime mStart;
    PRInt32 mRunning;
    PRLock *mLock;
};
    
nsTimelineServiceTimer::nsTimelineServiceTimer()
: mAccum(LL_ZERO), mStart(LL_ZERO), mRunning(0), mLock(PR_NewLock())
{
}
    
nsTimelineServiceTimer::~nsTimelineServiceTimer()
{
    PR_DestroyLock(mLock);
}

void nsTimelineServiceTimer::start()
{
    PR_Lock(mLock);
    if (!mRunning) {
        mStart = Now();
    }
    mRunning++;
    PR_Unlock(mLock);
}

void nsTimelineServiceTimer::stop(PRTime now)
{
    PR_Lock(mLock);
    mRunning--;
    if (mRunning == 0) {
        PRTime delta, accum;
        LL_SUB(delta, now, mStart);
        LL_ADD(accum, mAccum, delta);
        mAccum = accum;
    }
    PR_Unlock(mLock);
}

PRTime nsTimelineServiceTimer::getAccum()
{
    PRTime accum;

    PR_Lock(mLock);
    if (!mRunning) {
        accum = mAccum;
    } else {
        PRTime delta;
        LL_SUB(delta, Now(), mStart);
        LL_ADD(accum, mAccum, delta);
    }
    PR_Unlock(mLock);
    return accum;
}

PRTime nsTimelineServiceTimer::getAccum(PRTime now)
{
    PRTime accum;

    PR_Lock(mLock);
    if (!mRunning) {
        accum = mAccum;
    } else {
        PRTime delta;
        LL_SUB(delta, now, mStart);
        LL_ADD(accum, mAccum, delta);
    }
    PR_Unlock(mLock);
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

static void TimelineInit(void)
{
    char *timeStr;
    char *fileName;
    PRInt32 secs, msecs;
    PRFileDesc *fd;
    PRInt64 tmp1, tmp2;

    timeStr = PR_GetEnv("NS_TIMELINE_INIT_TIME");
#ifdef XP_MAC    
    initInterval = PR_IntervalNow();
#endif
    if (timeStr != NULL && 2 == PR_sscanf(timeStr, "%d.%d", &secs, &msecs)) {
        LL_MUL(tmp1, (PRInt64)secs, 1000000);
        LL_MUL(tmp2, (PRInt64)msecs, 1000);
        LL_ADD(initTime, tmp1, tmp2);
#ifdef XP_MAC
        initInterval -= PR_MicrosecondsToInterval(
            (PRUint32)(PR_Now() - initTime));
#endif
    } else {
        initTime = PR_Now();
    }
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
                   "NOTE: due to threading and asynchrony, the indentation\n"
                   "that you see does not necessarily correspond to nesting\n"
                   "in the code.\n\n");
    }
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
    char pbuf[500], *pc, tbuf[500];

    ParseTime(tm, secs, msecs);

    // snprintf/write rather than fprintf because we don't want
    // messages from multiple threads to garble one another.
    pc = Indent(pbuf);
    PR_vsnprintf(pc, sizeof pbuf - (pc - pbuf), text, args);
    PR_snprintf(tbuf, sizeof tbuf, "%05d.%03d: %s\n",
                secs, msecs, pbuf);
    PR_Write(timelineFD, tbuf, strlen(tbuf));
}

/*
 * Make this public if we need it.
 */
static nsresult NS_TimelineMarkV(const char *text, va_list args)
{
    PRTime elapsed,tmp;

    if (LL_IS_ZERO(initTime)) {
        TimelineInit();
    }

    tmp = Now();
    LL_SUB(elapsed, tmp, initTime);

    PrintTime(elapsed, text, args);
   
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineMark(const char *text, ...)
{
    va_list args;
    va_start(args, text);
    NS_TimelineMarkV(text, args);

    return NS_OK;
}

/*
 * PRCallOnceFN that initializes stuff for the timing service.
 */
static PRStatus InitTimers(void)
{
    timerLock = PR_NewLock();
    if (timerLock == NULL) {
        return PR_FAILURE;
    }
    timers = PL_NewHashTable(100, PL_HashString, PL_CompareStrings,
                             PL_CompareValues, NULL, NULL);
    return timers != NULL ? PR_SUCCESS : PR_FAILURE;
}

PR_IMPLEMENT(nsresult) NS_TimelineStartTimer(const char *timerName)
{
    static PRCallOnceType once;
    PR_CallOnce(&once, InitTimers);

    if (timers == NULL) {
        return NS_ERROR_FAILURE;
    }

    PR_Lock(timerLock);
    nsTimelineServiceTimer *timer
        = (nsTimelineServiceTimer *)PL_HashTableLookup(timers, timerName);
    if (timer == NULL) {
        timer = new nsTimelineServiceTimer;
        PL_HashTableAdd(timers, timerName, timer);
    }
    PR_Unlock(timerLock);
        
    timer->start();
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineStopTimer(const char *timerName)
{
    if (timers == NULL) {
        return NS_ERROR_FAILURE;
    }

    /*
     * Strange-looking now/timer->stop() interaction is to avoid
     * including time spent in PR_Lock and PL_HashTableLookup in the
     * timer.
     */
    PRTime now = Now();

    PR_Lock(timerLock);
    nsTimelineServiceTimer *timer
        = (nsTimelineServiceTimer *)PL_HashTableLookup(timers, timerName);
    PR_Unlock(timerLock);
    if (timer == NULL) {
        return NS_ERROR_FAILURE;
    }

    timer->stop(now);

    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineMarkTimer(const char *timerName)
{
    if (timers == NULL) {
        return NS_ERROR_FAILURE;
    }

    PR_Lock(timerLock);
    nsTimelineServiceTimer *timer
        = (nsTimelineServiceTimer *)PL_HashTableLookup(timers, timerName);
    PR_Unlock(timerLock);
    if (timer == NULL) {
        return NS_ERROR_FAILURE;
    }
    PRTime accum = timer->getAccum();

    char buf[500];
    PRInt32 sec, msec;
    ParseTime(accum, sec, msec);
    PR_snprintf(buf, sizeof buf, "%s total: %d.%03d",
                timerName, sec, msec);
    NS_TimelineMark(buf);

    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineIndent()
{
    indent++;                   // Could have threading issues here.
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineOutdent()
{
    indent--;                   // Could have threading issues here.
    return NS_OK;
}

PR_IMPLEMENT(nsresult) NS_TimelineEnter(const char *text)
{
    nsresult rv = NS_TimelineMark("%s...", text);
    if (!NS_SUCCEEDED(rv)) {
        return rv;
    }
    return NS_TimelineIndent();
}

PR_IMPLEMENT(nsresult) NS_TimelineLeave(const char *text)
{
    nsresult rv = NS_TimelineOutdent();
    if (!NS_SUCCEEDED(rv)) {
        return rv;
    }
    return NS_TimelineMark("...%s", text);
}

nsTimelineService::nsTimelineService()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsTimelineService::~nsTimelineService()
{
  /* destructor code */
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

/* void markTimer (in string timerName, in string text); */
NS_IMETHODIMP nsTimelineService::MarkTimer(const char *timerName)
{
    return NS_TimelineMarkTimer(timerName);
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
