/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**  1996 - Netscape Communications Corporation
**
** Name: alarmtst.c
**
** Description: Test alarms
**
** Modification History:
** 13-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/

#include "prlog.h"
#include "prinit.h"
#include "obsolete/pralarm.h"
#include "prlock.h"
#include "prlong.h"
#include "prcvar.h"
#include "prinrval.h"
#include "prtime.h"

/* Used to get the command line option */
#include "plgetopt.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(XP_UNIX)
#include <sys/time.h>
#endif

static PRIntn debug_mode;
static PRIntn failed_already=0;
static PRThreadScope thread_scope = PR_LOCAL_THREAD;

typedef struct notifyData {
    PRLock *ml;
    PRCondVar *child;
    PRCondVar *parent;
    PRBool pending;
    PRUint32 counter;
} NotifyData;

static void Notifier(void *arg)
{
    NotifyData *notifyData = (NotifyData*)arg;
    PR_Lock(notifyData->ml);
    while (notifyData->counter > 0)
    {
        while (!notifyData->pending)
            PR_WaitCondVar(notifyData->child, PR_INTERVAL_NO_TIMEOUT);
        notifyData->counter -= 1;
        notifyData->pending = PR_FALSE;
        PR_NotifyCondVar(notifyData->parent);
    }
    PR_Unlock(notifyData->ml);
}  /* Notifier */
/***********************************************************************
** PRIVATE FUNCTION:    ConditionNotify
** DESCRIPTION:
** 
** INPUTS:      loops
** OUTPUTS:     None
** RETURN:      overhead
** SIDE EFFECTS:
**      
** RESTRICTIONS:
**      None
** MEMORY:      NA
** ALGORITHM:
**      
***********************************************************************/


static PRIntervalTime ConditionNotify(PRUint32 loops)
{
    PRThread *thread;
    NotifyData notifyData;
    PRIntervalTime timein, overhead;
    
    timein = PR_IntervalNow();

    notifyData.counter = loops;
    notifyData.ml = PR_NewLock();
    notifyData.child = PR_NewCondVar(notifyData.ml);
    notifyData.parent = PR_NewCondVar(notifyData.ml);
    thread = PR_CreateThread(
        PR_USER_THREAD, Notifier, &notifyData,
        PR_GetThreadPriority(PR_GetCurrentThread()),
        thread_scope, PR_JOINABLE_THREAD, 0);

    overhead = PR_IntervalNow() - timein;  /* elapsed so far */

    PR_Lock(notifyData.ml);
    while (notifyData.counter > 0)
    {
        notifyData.pending = PR_TRUE;
        PR_NotifyCondVar(notifyData.child);
        while (notifyData.pending)
            PR_WaitCondVar(notifyData.parent, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(notifyData.ml);

    timein = PR_IntervalNow();

    (void)PR_JoinThread(thread);
    PR_DestroyCondVar(notifyData.child);
    PR_DestroyCondVar(notifyData.parent);
    PR_DestroyLock(notifyData.ml);
    
    overhead += (PR_IntervalNow() - timein);  /* more overhead */

    return overhead;
}  /* ConditionNotify */

static PRIntervalTime ConditionTimeout(PRUint32 loops)
{
    PRUintn count;
    PRIntervalTime overhead, timein = PR_IntervalNow();

    PRLock *ml = PR_NewLock();
    PRCondVar *cv = PR_NewCondVar(ml);
    PRIntervalTime interval = PR_MillisecondsToInterval(50);

    overhead = PR_IntervalNow() - timein;

    PR_Lock(ml);
    for (count = 0; count < loops; ++count)
    {
        overhead += interval;
        PR_ASSERT(PR_WaitCondVar(cv, interval) == PR_SUCCESS);
    }
    PR_Unlock(ml);

    timein = PR_IntervalNow();
    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    overhead += (PR_IntervalNow() - timein);

    return overhead;
}  /* ConditionTimeout */

typedef struct AlarmData {
    PRLock *ml;
    PRCondVar *cv;
    PRUint32 rate, late, times;
    PRIntervalTime duration, timein, period;
} AlarmData;

static PRBool AlarmFn1(PRAlarmID *id, void *clientData, PRUint32 late)
{
    PRStatus rv = PR_SUCCESS;
    PRBool keepGoing, resetAlarm;
    PRIntervalTime interval, now = PR_IntervalNow();
    AlarmData *ad = (AlarmData*)clientData;

    PR_Lock(ad->ml);
    ad->late += late;
    ad->times += 1;
    keepGoing = ((PRIntervalTime)(now - ad->timein) < ad->duration) ?
        PR_TRUE : PR_FALSE;
    if (!keepGoing)
        rv = PR_NotifyCondVar(ad->cv);
    resetAlarm = ((ad->times % 31) == 0) ? PR_TRUE : PR_FALSE;
                                         
    interval = (ad->period + ad->rate - 1) / ad->rate;
    if (!late && (interval > 10))
    {
        interval &= (now & 0x03) + 1;
        PR_WaitCondVar(ad->cv, interval);
    }
          
    PR_Unlock(ad->ml);

    if (rv != PR_SUCCESS)
    {
		if (!debug_mode) failed_already=1;
		else
		 printf("AlarmFn: notify status: FAIL\n");
		
	}

    if (resetAlarm)
    {   
        ad->rate += 3;
        ad->late = ad->times = 0;
        if (PR_ResetAlarm(id, ad->period, ad->rate) != PR_SUCCESS)
        {
			if (!debug_mode)
				failed_already=1;
			else		
				printf("AlarmFn: Resetting alarm status: FAIL\n");

            keepGoing = PR_FALSE;
        }

    }

    return keepGoing;
}  /* AlarmFn1 */

static PRIntervalTime Alarms1(PRUint32 loops)
{
    PRAlarm *alarm;
    AlarmData ad;
    PRIntervalTime overhead, timein = PR_IntervalNow();
    PRIntervalTime duration = PR_SecondsToInterval(3);

    PRLock *ml = PR_NewLock();
    PRCondVar *cv = PR_NewCondVar(ml);

    ad.ml = ml;
    ad.cv = cv;
    ad.rate = 1;
    ad.times = loops;
    ad.late = ad.times = 0;
    ad.duration = duration;
    ad.timein = PR_IntervalNow();
    ad.period = PR_SecondsToInterval(1);

    alarm = PR_CreateAlarm();

    (void)PR_SetAlarm(
        alarm, ad.period, ad.rate, AlarmFn1, &ad);
        
    overhead = PR_IntervalNow() - timein;

    PR_Lock(ml);
    while ((PRIntervalTime)(PR_IntervalNow() - ad.timein) < duration)
        PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);
    PR_Unlock(ml);

    timein = PR_IntervalNow();
    (void)PR_DestroyAlarm(alarm);
    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    overhead += (PR_IntervalNow() - timein);
    
    return duration + overhead;
}  /* Alarms1 */

static PRBool AlarmFn2(PRAlarmID *id, void *clientData, PRUint32 late)
{
    PRBool keepGoing;
    PRStatus rv = PR_SUCCESS;
    AlarmData *ad = (AlarmData*)clientData;
    PRIntervalTime interval, now = PR_IntervalNow();

    PR_Lock(ad->ml);
    ad->times += 1;
    keepGoing = ((PRIntervalTime)(now - ad->timein) < ad->duration) ?
        PR_TRUE : PR_FALSE;
    interval = (ad->period + ad->rate - 1) / ad->rate;

    if (!late && (interval > 10))
    {
        interval &= (now & 0x03) + 1;
        PR_WaitCondVar(ad->cv, interval);
    }

    if (!keepGoing) rv = PR_NotifyCondVar(ad->cv);

    PR_Unlock(ad->ml);


    if (rv != PR_SUCCESS)
		failed_already=1;;

    return keepGoing;
}  /* AlarmFn2 */

static PRIntervalTime Alarms2(PRUint32 loops)
{
    PRStatus rv;
    PRAlarm *alarm;
    PRIntervalTime overhead, timein = PR_IntervalNow();
    AlarmData ad;
    PRIntervalTime duration = PR_SecondsToInterval(30);

    PRLock *ml = PR_NewLock();
    PRCondVar *cv = PR_NewCondVar(ml);

    ad.ml = ml;
    ad.cv = cv;
    ad.rate = 1;
    ad.times = loops;
    ad.late = ad.times = 0;
    ad.duration = duration;
    ad.timein = PR_IntervalNow();
    ad.period = PR_SecondsToInterval(1);

    alarm = PR_CreateAlarm();

    (void)PR_SetAlarm(
        alarm, ad.period, ad.rate, AlarmFn2, &ad);
        
    overhead = PR_IntervalNow() - timein;

    PR_Lock(ml);
    while ((PRIntervalTime)(PR_IntervalNow() - ad.timein) < duration)
        PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);
    PR_Unlock(ml);
    
    timein = PR_IntervalNow();

    rv = PR_DestroyAlarm(alarm);
    if (rv != PR_SUCCESS)
    {
		if (!debug_mode)
			failed_already=1;
		else	
			printf("***Destroying alarm status: FAIL\n");
    }
		

    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    
    overhead += (PR_IntervalNow() - timein);
    
    return duration + overhead;
}  /* Alarms2 */

static PRIntervalTime Alarms3(PRUint32 loops)
{
    PRIntn i;
    PRStatus rv;
    PRAlarm *alarm;
    AlarmData ad[3];
    PRIntervalTime duration = PR_SecondsToInterval(30);
    PRIntervalTime overhead, timein = PR_IntervalNow();

    PRLock *ml = PR_NewLock();
    PRCondVar *cv = PR_NewCondVar(ml);

    for (i = 0; i < 3; ++i)
    {
        ad[i].ml = ml;
        ad[i].cv = cv;
        ad[i].rate = 1;
        ad[i].times = loops;
        ad[i].duration = duration;
        ad[i].late = ad[i].times = 0;
        ad[i].timein = PR_IntervalNow();
        ad[i].period = PR_SecondsToInterval(1);

        /* more loops, faster rate => same elapsed time */
        ad[i].times = (i + 1) * loops;
        ad[i].rate = (i + 1) * 10;
    }

    alarm = PR_CreateAlarm();

    for (i = 0; i < 3; ++i)
    {
        (void)PR_SetAlarm(
            alarm, ad[i].period, ad[i].rate,
            AlarmFn2, &ad[i]);
    }
        
    overhead = PR_IntervalNow() - timein;

    PR_Lock(ml);
    for (i = 0; i < 3; ++i)
    {
        while ((PRIntervalTime)(PR_IntervalNow() - ad[i].timein) < duration)
            PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(ml);

    timein = PR_IntervalNow();

	if (debug_mode)
	printf
        ("Alarms3 finished at %u, %u, %u\n",
        ad[0].timein, ad[1].timein, ad[2].timein);
    
    rv = PR_DestroyAlarm(alarm);
    if (rv != PR_SUCCESS)
    {
		if (!debug_mode)		
			failed_already=1;
		else	
		   printf("***Destroying alarm status: FAIL\n");
	}
    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    
    overhead += (duration / 3);
    overhead += (PR_IntervalNow() - timein);

    return overhead;
}  /* Alarms3 */

static PRUint32 TimeThis(
    const char *msg, PRUint32 (*func)(PRUint32 loops), PRUint32 loops)
{
    PRUint32 overhead, usecs;
    PRIntervalTime predicted, timein, timeout, ticks;

    if (debug_mode)
      printf("Testing %s ...", msg);

    timein = PR_IntervalNow();
    predicted = func(loops);
    timeout = PR_IntervalNow();

    if (debug_mode)
      printf(" done\n");

    ticks = timeout - timein;
    usecs = PR_IntervalToMicroseconds(ticks);
    overhead = PR_IntervalToMicroseconds(predicted);

    if(ticks < predicted)
    {
		if (debug_mode) {
        printf("\tFinished in negative time\n");
        printf("\tpredicted overhead was %d usecs\n", overhead);
        printf("\ttest completed in %d usecs\n\n", usecs);
		}
    }
    else
    {
	if (debug_mode)		
        printf(
            "\ttotal: %d usecs\n\toverhead: %d usecs\n\tcost: %6.3f usecs\n\n",
            usecs, overhead, ((double)(usecs - overhead) / (double)loops));
    }

    return overhead;
}  /* TimeThis */

int prmain(int argc, char** argv)
{
    PRUint32 cpu, cpus = 0, loops = 0;

	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name [-d]
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "Gdl:c:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'G':  /* GLOBAL threads */
			thread_scope = PR_GLOBAL_THREAD;
            break;
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 'l':  /* loop count */
			loops = atoi(opt->value);
            break;
        case 'c':  /* concurrency limit */
			cpus = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);


    if (cpus == 0) cpus = 1;
    if (loops == 0) loops = 4;

	if (debug_mode)
		printf("Alarm: Using %d loops\n", loops);

	if (debug_mode)		
        printf("Alarm: Using %d cpu(s)\n", cpus);

    for (cpu = 1; cpu <= cpus; ++cpu)
    {
      if (debug_mode)
        printf("\nAlarm: Using %d CPU(s)\n", cpu);

      PR_SetConcurrency(cpu);

      /* some basic time test */
      (void)TimeThis("ConditionNotify", ConditionNotify, loops);
      (void)TimeThis("ConditionTimeout", ConditionTimeout, loops);
      (void)TimeThis("Alarms1", Alarms1, loops);
      (void)TimeThis("Alarms2", Alarms2, loops);
      (void)TimeThis("Alarms3", Alarms3, loops);
    }
    return 0;
}

int main(int argc, char** argv)
{
     PR_Initialize(prmain, argc, argv, 0);
     PR_STDIO_INIT();
	 if (failed_already) return 1;
	 else return 0;

}  /* main */


/* alarmtst.c */
