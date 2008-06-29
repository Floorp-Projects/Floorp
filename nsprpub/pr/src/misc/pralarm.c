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

/**********************************************************************/
/******************************* PRALARM ******************************/
/**********************************************************************/

#ifdef XP_MAC
#include "pralarm.h"
#else
#include "obsolete/pralarm.h"
#endif

struct PRAlarmID {                       /* typedef'd in pralarm.h       */
    PRCList list;                        /* circular list linkage        */
    PRAlarm *alarm;                      /* back pointer to owning alarm */
    PRPeriodicAlarmFn function;          /* function to call for notify  */
    void *clientData;                    /* opaque client context        */
    PRIntervalTime period;               /* the client defined period    */
    PRUint32 rate;                       /* rate of notification         */

    PRUint32 accumulator;                /* keeps track of # notifies    */
    PRIntervalTime epoch;                /* when timer was started       */
    PRIntervalTime nextNotify;           /* when we'll next do our thing */
    PRIntervalTime lastNotify;           /* when we last did our thing   */
};

typedef enum {alarm_active, alarm_inactive} _AlarmState;

struct PRAlarm {                         /* typedef'd in pralarm.h       */
    PRCList timers;                      /* base of alarm ids list       */
    PRLock *lock;                        /* lock used to protect data    */
    PRCondVar *cond;                     /* condition that used to wait  */
    PRThread *notifier;                  /* thread to deliver notifies   */
    PRAlarmID *current;                  /* current alarm being served   */
    _AlarmState state;                   /* used to delete the alarm     */
};

static PRAlarmID *pr_getNextAlarm(PRAlarm *alarm, PRAlarmID *id)
{
/*
 * Puts 'id' back into the sorted list iff it's not NULL.
 * Removes the first element from the list and returns it (or NULL).
 * List is "assumed" to be short.
 *
 * NB: Caller is providing locking
 */
    PRCList *timer;
    PRAlarmID *result = id;
    PRIntervalTime now = PR_IntervalNow();

    if (!PR_CLIST_IS_EMPTY(&alarm->timers))
    {    
        if (id != NULL)  /* have to put this id back in */
        {        
            PRIntervalTime idDelta = now - id->nextNotify;
            timer = alarm->timers.next;
            do
            {
                result = (PRAlarmID*)timer;
                if ((PRIntervalTime)(now - result->nextNotify) > idDelta)
                {
                    PR_INSERT_BEFORE(&id->list, &alarm->timers);
                    break;
                }
                timer = timer->next;
            } while (timer != &alarm->timers);
        }
        result = (PRAlarmID*)(timer = PR_LIST_HEAD(&alarm->timers));
        PR_REMOVE_LINK(timer);  /* remove it from the list */
    }

    return result;
}  /* pr_getNextAlarm */

static PRIntervalTime pr_PredictNextNotifyTime(PRAlarmID *id)
{
    PRIntervalTime delta;
    PRFloat64 baseRate = (PRFloat64)id->period / (PRFloat64)id->rate;
    PRFloat64 offsetFromEpoch = (PRFloat64)id->accumulator * baseRate;

    id->accumulator += 1;  /* every call advances to next period */
    id->lastNotify = id->nextNotify;  /* just keeping track of things */
    id->nextNotify = (PRIntervalTime)(offsetFromEpoch + 0.5);

    delta = id->nextNotify - id->lastNotify;
    return delta;
}  /* pr_PredictNextNotifyTime */

static void PR_CALLBACK pr_alarmNotifier(void *arg)
{
    /*
     * This is the root of the notifier thread. There is one such thread
     * for each PRAlarm. It may service an arbitrary (though assumed to be
     * small) number of alarms using the same thread and structure. It
     * continues to run until the alarm is destroyed.
     */
    PRAlarmID *id = NULL;
    PRAlarm *alarm = (PRAlarm*)arg;
    enum {notify, abort, scan} why = scan;

    while (why != abort)
    {
        PRIntervalTime pause;

        PR_Lock(alarm->lock);
        while (why == scan)
        {
            alarm->current = NULL;  /* reset current id */
            if (alarm->state == alarm_inactive) why = abort;  /* we're toast */
            else if (why == scan)  /* the dominant case */
            {
                id = pr_getNextAlarm(alarm, id);  /* even if it's the same */
                if (id == NULL)  /* there are no alarms set */
                    (void)PR_WaitCondVar(alarm->cond, PR_INTERVAL_NO_TIMEOUT);
                else
                {
                    pause = id->nextNotify - (PR_IntervalNow() - id->epoch);
                    if ((PRInt32)pause <= 0)  /* is this one's time up? */
                    {
                        why = notify;  /* set up to do our thing */
                        alarm->current = id;  /* id we're about to schedule */
                    }
                    else
                        (void)PR_WaitCondVar(alarm->cond, pause);  /* dally */
                }
            }
        }
        PR_Unlock(alarm->lock);

        if (why == notify)
        {
            (void)pr_PredictNextNotifyTime(id);
            if (!id->function(id, id->clientData, ~pause))
            {
                /*
                 * Notified function decided not to continue. Free
                 * the alarm id to make sure it doesn't get back on
                 * the list.
                 */
                PR_DELETE(id);  /* free notifier object */
                id = NULL;  /* so it doesn't get back into the list */
            }
            why = scan;  /* so we can cycle through the loop again */
        }
    }

}  /* pr_alarm_notifier */

PR_IMPLEMENT(PRAlarm*) PR_CreateAlarm(void)
{
    PRAlarm *alarm = PR_NEWZAP(PRAlarm);
    if (alarm != NULL)
    {
        if ((alarm->lock = PR_NewLock()) == NULL) goto done;
        if ((alarm->cond = PR_NewCondVar(alarm->lock)) == NULL) goto done;
        alarm->state = alarm_active;
        PR_INIT_CLIST(&alarm->timers);
        alarm->notifier = PR_CreateThread(
            PR_USER_THREAD, pr_alarmNotifier, alarm,
            PR_GetThreadPriority(PR_GetCurrentThread()),
            PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
        if (alarm->notifier == NULL) goto done;
    }
    return alarm;

done:
    if (alarm->cond != NULL) PR_DestroyCondVar(alarm->cond);
    if (alarm->lock != NULL) PR_DestroyLock(alarm->lock);
    PR_DELETE(alarm);
    return NULL;
}  /* CreateAlarm */

PR_IMPLEMENT(PRStatus) PR_DestroyAlarm(PRAlarm *alarm)
{
    PRStatus rv;

    PR_Lock(alarm->lock);
    alarm->state = alarm_inactive;
    rv = PR_NotifyCondVar(alarm->cond);
    PR_Unlock(alarm->lock);

    if (rv == PR_SUCCESS)
        rv = PR_JoinThread(alarm->notifier);
    if (rv == PR_SUCCESS)
    {
        PR_DestroyCondVar(alarm->cond);
        PR_DestroyLock(alarm->lock);
        PR_DELETE(alarm);
    }
    return rv;
}  /* PR_DestroyAlarm */

PR_IMPLEMENT(PRAlarmID*) PR_SetAlarm(
    PRAlarm *alarm, PRIntervalTime period, PRUint32 rate,
    PRPeriodicAlarmFn function, void *clientData)
{
    /*
     * Create a new periodic alarm an existing current structure.
     * Set up the context and compute the first notify time (immediate).
     * Link the new ID into the head of the list (since it's notifying
     * immediately).
     */

    PRAlarmID *id = PR_NEWZAP(PRAlarmID);

    if (!id)
        return NULL;

    id->alarm = alarm;
    PR_INIT_CLIST(&id->list);
    id->function = function;
    id->clientData = clientData;
    id->period = period;
    id->rate = rate;
    id->epoch = id->nextNotify = PR_IntervalNow();
    (void)pr_PredictNextNotifyTime(id);

    PR_Lock(alarm->lock);
    PR_INSERT_BEFORE(&id->list, &alarm->timers);
    PR_NotifyCondVar(alarm->cond);
    PR_Unlock(alarm->lock);

    return id;
}  /* PR_SetAlarm */

PR_IMPLEMENT(PRStatus) PR_ResetAlarm(
    PRAlarmID *id, PRIntervalTime period, PRUint32 rate)
{
    /*
     * Can only be called from within the notify routine. Doesn't
     * need locking because it can only be called from within the
     * notify routine.
     */
    if (id != id->alarm->current)
        return PR_FAILURE;
    id->period = period;
    id->rate = rate;
    id->accumulator = 1;
    id->epoch = PR_IntervalNow();
    (void)pr_PredictNextNotifyTime(id);
    return PR_SUCCESS;
}  /* PR_ResetAlarm */



