/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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

/********************************************************************/
 
#include <prtypes.h>
#include <prmem.h>
#include <prlog.h>
#include <prtime.h>
#include <prclist.h>
#include <limits.h>

#include "schedulr.h"

#define SCHEDULER_SLOP	1000000
/*
	If an event is scheduled to happen within this time frame (in milliseconds)
	we'll pull it off the queue rather than entering another wait loop.
*/

#define SCHED_RUNNING     (PR_BIT(0))
#define SCHED_PAUSED      (PR_BIT(1))
#define SCHED_FROZEN      (PR_BIT(2))
#define SCHED_TERMINATED  (PR_BIT(31))
/*
	Status bits for the scheduler
*/

typedef struct  _SchedulerData {
	PRUint32	flags;		
	PRUint32	nextID;
	PRUint32	nextObserver;
	PRTime		nextEventTime;
	PRThread	*pSchedThread;
	PRLock		*pTimerLock;
	PRCondVar	*pTimerCVar;
    PRCList		*pEventQueue;
	PRCList		*pObserverQueue;
	PRCList		eventQ;
} SchedulerData;

/*
	SchedulerPtr resolves internally to this data type, which holds the
	state for a particular scheduler.  Note that pEventQueue points to
	the eventQ struct which is embedded within the data, so pEventQueue
	must never be free'd (since it was never allocated)
*/

typedef struct	_SchedulerEvent {
    PRCList				*next;
    PRCList				*prev;
	PRUint32			eventID;
	SchedulerTime		eventTime;
	PRTime				fireTime;
	SchedFuncPtr		pEventProc;
	SchedFuncDataPtr	pEventData;
	char				*eventName;
} SchedEvent;

typedef struct _EventParameters {
	SchedulerPtr		pScheduler;
	PRUint32			eventID;
	SchedFuncPtr		pEventProc;
	SchedFuncDataPtr	pEventData;
} EventParams;
/*
	EventParams is an encapsulation of the data that's passed to the
	event function.  Because thread creation only takes a single
	parameter, EventParams is necessary to allow all of the data
	to go across.
*/


typedef struct	_SchedulerObserver {
    PRCList				*next;
    PRCList				*prev;
	PRUint32			observerID;
	SchedObsType		typeMask;
	SchedObsPtr			pObserverProc;
	SchedObsDataPtr		pObserverData;
} SchedObserver;

/* Forward Definitions */

void			DoEvent(EventParams *pParms);

void			SchedulerRun(SchedulerData *pData);
SchedulerErr	SchedulerInit(SchedulerData *pData);
void			SchedulerDestroy(SchedulerData *pData);

SchedEvent *	EventFromID(SchedulerData *pData, PRUint32 eventID);
void			InsertEvent(SchedulerData *pData, SchedEvent *pEvent);
void			RescheduleEvent(SchedulerData *pScheduler, SchedEvent *pEvent);

void			NotifyObservers(SchedulerData *pScheduler, SchedObsType type,
								PRUint32 eventID, char *szEventName,
								SchedFuncDataPtr pEventData);
SchedObserver*	ObserverFromID(SchedulerData *pData, PRUint32 observerID);


/* Implementation */

void DoEvent(EventParams *pParms)
{
	pParms->pEventProc(pParms->pScheduler, pParms->eventID, (void *) pParms->pEventData);
}

void SchedulerRun(SchedulerData	*pData)
{
	PRUint32			uSecDifference;
	PRTime				timeDifference;
	PRInt64				scratchVal;
	PRIntervalTime		timeout;
	SchedEvent			*pCurrentEvent;

	pData->flags |= SCHED_RUNNING;

	while (!(pData->flags & SCHED_TERMINATED)) {
		if (pData->flags & SCHED_RUNNING) {
			pCurrentEvent = NULL;

			/* Then, compute the next scheduler interval and wait */
			PR_Lock(pData->pTimerLock);

			if ((PR_CLIST_IS_EMPTY(pData->pEventQueue)) ||
				(LL_IS_ZERO(pData->nextEventTime))) {

				/* if there's nothing on the queue, the timeout is infinite */

				timeout = PR_INTERVAL_NO_TIMEOUT;
			} else {
				/* otherwise, it's the time between now and the next event */

				LL_SUB(timeDifference, pData->nextEventTime, PR_Now());

				LL_I2L(scratchVal, LONG_MAX);
				if (LL_CMP(timeDifference, <, scratchVal)) {

					if (!(LL_GE_ZERO(timeDifference))) {
						timeout = PR_INTERVAL_NO_WAIT;

					} else {
						LL_L2UI(uSecDifference, timeDifference);

						timeout = PR_MicrosecondsToInterval(uSecDifference);
					}
				} else {
					timeout = ULONG_MAX;
				}
			}

			PR_WaitCondVar(pData->pTimerCVar, timeout);

			/* if we've come out of the wait condition, it's because:

				1) we've scheduled a new event before the current one
				2) the current event was deleted
				3) it's time for the current event to fire
				4) the crawler is terminating
				5) the thread was spuriously interrupted

			   all of these conditions will get checked subsequent to
			   here, or the next time the loop is evaluated
			*/

			/* First, check that the stated next event time is accurate
			   (to check for addition/deletion cases) */

			if (PR_CLIST_IS_EMPTY(pData->pEventQueue)) {
				pData->nextEventTime = LL_ZERO;
			} else {
				pData->nextEventTime = ((SchedEvent *) PR_LIST_HEAD(pData->pEventQueue))->fireTime;

				/* Now, see if the nextEventTime is close enough to 
				   pop the event off of the queue and process it */

				LL_SUB(timeDifference, pData->nextEventTime,
					PR_Now());

				LL_I2L(scratchVal, LONG_MAX);
				if (LL_CMP(timeDifference, <, scratchVal)) {

					LL_L2UI(uSecDifference, timeDifference);

					if (!(LL_GE_ZERO(timeDifference)) || (uSecDifference < SCHEDULER_SLOP)) {
						/* yup, take the first item off the queue and use it. */
						pCurrentEvent = (SchedEvent *) PR_LIST_HEAD(pData->pEventQueue);

						PR_REMOVE_LINK(pCurrentEvent);
					}
				}
			}

			PR_Unlock(pData->pTimerLock);

			/* we didn't want to actually process the event while we were in the lock */

			if (pCurrentEvent != NULL) {
				if (pData->flags & SCHED_PAUSED) {
					/* we missed an event */
					/* not really, we only missed it if expires in this time frame */
					/* if we missed it, send out a missed notification */

				} else {
					/* process event */
					EventParams		parms;

					parms.pScheduler = pData;
					parms.pEventProc = pCurrentEvent->pEventProc;
					parms.pEventData = pCurrentEvent->pEventData;
					parms.eventID = pCurrentEvent->eventID;

					PR_CreateThread(PR_USER_THREAD,
						(void (*)(void *arg)) DoEvent,
						&parms,
						PR_PRIORITY_NORMAL,
						PR_LOCAL_THREAD,
						PR_UNJOINABLE_THREAD,
						0);

					/* notify observers */
					NotifyObservers(pData, SCHED_OBSERVE_FIRE, pCurrentEvent->eventID,
						pCurrentEvent->eventName, pCurrentEvent->pEventData);
				}

				/* if the event is a repeating event, reschedule it */
				if (pCurrentEvent->eventTime.repeating != 0) {
					RescheduleEvent(pData, pCurrentEvent);
				} else {
					if (pCurrentEvent->eventName != NULL) {
						PR_Free(pCurrentEvent->eventName);
					}

					PR_DELETE(pCurrentEvent);
				}
			}
		}
	}

	SchedulerDestroy(pData);
}

SchedulerErr SchedulerInit(SchedulerData *pData) {

	pData->nextID = 1;
	pData->nextObserver = 1000;
	pData->pTimerLock = PR_NewLock();
	pData->pTimerCVar = PR_NewCondVar(pData->pTimerLock);

	pData->pSchedThread = PR_CreateThread(PR_USER_THREAD,
			(void (*)(void *)) SchedulerRun,
			(void *) pData, 
			PR_PRIORITY_NORMAL,
			PR_LOCAL_THREAD,
			PR_JOINABLE_THREAD,
			0);

	pData->pEventQueue = &(pData->eventQ);
	PR_INIT_CLIST(pData->pEventQueue);

	return SCHED_ERR_NOERR;
}

void SchedulerDestroy(SchedulerData *pData) {
	SchedObserver *pHead, *pCurrent, *pOld;

	PR_ASSERT(pData != NULL);

	/* First, clear out the observer list */

	if (pData->pObserverQueue != NULL) {

		PR_Lock(pData->pTimerLock);

		if (!PR_CLIST_IS_EMPTY(pData->pObserverQueue)) {
			pHead = pCurrent = (SchedObserver *) PR_LIST_HEAD(pData->pObserverQueue);

			do {
				pOld = pCurrent;
				pCurrent = (SchedObserver *) PR_NEXT_LINK(pCurrent);

				PR_REMOVE_LINK(pOld);
				PR_DELETE(pOld);
			} while (!PR_CLIST_IS_EMPTY(pData->pObserverQueue));
		}

		PR_Free(pData->pObserverQueue);
		pData->pObserverQueue = NULL;

		PR_Unlock(pData->pTimerLock);
	}

	/*	FIXME! Need to remove all events*/


	/* Release the locks and condition variable */

	if (pData->pTimerCVar != NULL) {
		PR_DestroyCondVar(pData->pTimerCVar); 
		pData->pTimerCVar = NULL;
	}

	if (pData->pTimerLock != NULL) {
		PR_DestroyLock(pData->pTimerLock); 
		pData->pTimerLock = NULL;
	}

	/* Finally, delete the instance */

	PR_DELETE(pData);

	return;
}

/* EventFromID should always (and only) be called from inside a lock */

SchedEvent *
EventFromID(SchedulerData *pData, PRUint32 eventID)
{
	SchedEvent	*pReturn = NULL;
	SchedEvent	*pHead, *pCurrent;

	if ((pData != NULL) && (!PR_CLIST_IS_EMPTY(pData->pEventQueue))) {

		pHead = pCurrent = (SchedEvent *) PR_LIST_HEAD(pData->pEventQueue);

		do {
			if (pCurrent->eventID == eventID) {
				pReturn = pCurrent;
			}

			pCurrent = (SchedEvent *) PR_NEXT_LINK(pCurrent);
		} while ((pReturn == NULL) && (pCurrent != pHead));
	}

	return pReturn;
}

/* InsertEvent should always (and only) be called from inside a lock */

/* InsertEvent is really pretty inefficient.  It works by checking to
   see if the event is before the first one, and if so, inserts it there.
   Then, it checks to see if it's after the last one.  Otherwise, it
   walks forward from the first element using an insertion sort.
*/

void
InsertEvent(SchedulerData *pData, SchedEvent *pEvent)
{

	PR_ASSERT(pData != NULL);
	PR_ASSERT(pEvent != NULL);

	if (PR_CLIST_IS_EMPTY(pData->pEventQueue)) {
		PR_INSERT_LINK((PRCList *) pEvent, pData->pEventQueue);

		/* update the scheduler's nextEventTime */
		pData->nextEventTime = pEvent ->fireTime;
	} else {
		SchedEvent	*pCurrent;

		pCurrent = (SchedEvent *) PR_LIST_HEAD(pData->pEventQueue);

		if (LL_CMP(pEvent->fireTime, <, pCurrent->fireTime)) {
			PR_INSERT_LINK((PRCList *) pEvent, pData->pEventQueue);

		 	/* update the scheduler's nextEventTime */
			pData->nextEventTime = pEvent ->fireTime;
	
		} else if (!(LL_CMP(pEvent->fireTime, <, 
			((SchedEvent *) PR_LIST_TAIL(pData->pEventQueue))->fireTime))) {
			PR_APPEND_LINK((PRCList *) pEvent, pData->pEventQueue);
		} else {
			/* it's not the first, or the last item, so do the painful
			   insertion thang. */
			int		done = 0;

			while (!done) {
				pCurrent = (SchedEvent *) PR_NEXT_LINK(pCurrent);

				if (LL_CMP(pEvent->fireTime, <, pCurrent->fireTime)) {
					PR_INSERT_BEFORE((PRCList *) pEvent, (PRCList *) pCurrent);
					done = 1;
				}

			} /* while */
		} /* else */
	} /* else (not empty list */

	return;
}


void RescheduleEvent(SchedulerData *pScheduler, SchedEvent *pEvent) {

	/* compute the new firing time */

	/* rescheduled event times are computed by taking the event's scheduled time
	   and adding the appropriate number of seconds to the base.  Then, a randomized
	   value (plus or minus) is added, and the event is inserted into the queue.
	   This implies that examination of the event structure after the first firing
	   of an event will yield the adjusted base time, not the original base time.

	   If the newly scheduled event base time falls outside of the end time for the event,
	   the event is discarded and is not scheduled.  However, if the event's scheduled
	   time + its randomized value within range falls outside of the end time, the
	   event's firing time is pinned to the end time and still gets scheduled.
	 */

	PRTime				fireTime;
	PRInt32				random, randomSecs;
	PRInt64				randomizer, repeatValue;
	PRInt64				secs64, us2s;
	SchedulerTime		eventTime;

	LL_UI2L(us2s, PR_USEC_PER_SEC);
	LL_I2L(secs64, pEvent->eventTime.repeating);

	LL_MUL(repeatValue, secs64, us2s);
	LL_ADD(pEvent->eventTime.baseTime, pEvent->eventTime.baseTime, repeatValue);

	eventTime = pEvent->eventTime;

	if ((LL_NE(eventTime.end, LL_ZERO)) && (LL_CMP(eventTime.baseTime, >, eventTime.end))) {
		/* event is out of range; delete and move on */

		if (pEvent->eventName != NULL) {
			PR_Free(pEvent->eventName);
		}

		PR_DELETE(pEvent);
	} else {

		if (eventTime.range != 0) {

			random = (PRInt32) rand();
			randomSecs = random % eventTime.range;

			LL_I2L(secs64, randomSecs);
			LL_MUL(randomizer, secs64, us2s);

			if ((random % 2) == 0) {
				LL_NEG(randomizer, randomizer);
			}

			LL_ADD(fireTime, eventTime.baseTime, randomizer);
		} else {
			fireTime = eventTime.baseTime;
		}

		if ((LL_NE(LL_ZERO, eventTime.end)) && (LL_CMP(fireTime, >, eventTime.end))) {
			pEvent->fireTime = eventTime.end;
		} else if (LL_CMP(fireTime, <, eventTime.start)) {
			pEvent->fireTime = eventTime.start;
		} else {
			pEvent->fireTime = fireTime;
		}


		PR_Lock(pScheduler->pTimerLock);

		InsertEvent(pScheduler, pEvent);

		PR_NotifyCondVar(pScheduler->pTimerCVar);

		PR_Unlock(pScheduler->pTimerLock);
	}

}

void
NotifyObservers(SchedulerData *pScheduler, SchedObsType type,
				PRUint32 eventID, char *szEventName, SchedFuncDataPtr pEventData)
{

	SchedObserver	*pHead, *pCurrent;

	PR_ASSERT(pScheduler != NULL);

	if ((pScheduler->pObserverQueue != NULL) && (!PR_CLIST_IS_EMPTY(pScheduler->pObserverQueue))) {

		pHead = pCurrent = (SchedObserver *) PR_LIST_HEAD(pScheduler->pObserverQueue);

		do {
			if (pCurrent->typeMask & type) {
				pCurrent->pObserverProc(pScheduler, pCurrent->observerID, pCurrent->pObserverData,
										type, eventID, szEventName, pEventData);
			}

			pCurrent = (SchedObserver *) PR_NEXT_LINK(pCurrent);
		} while (pCurrent != pHead);
	}


	return;
}


SchedObserver *
ObserverFromID(SchedulerData *pData, PRUint32 observerID)
{
	SchedObserver *pReturn = NULL;
	SchedObserver *pHead, *pCurrent;

	PR_ASSERT(pData != NULL);

	if ((pData->pObserverQueue != NULL) && (!PR_CLIST_IS_EMPTY(pData->pObserverQueue))) {

		pHead = pCurrent = (SchedObserver *) PR_LIST_HEAD(pData->pObserverQueue);

		do {
			if (pCurrent->observerID == observerID) {
				pReturn = pCurrent;
			}

			pCurrent = (SchedObserver *) PR_NEXT_LINK(pCurrent);
		} while ((pReturn == NULL) && (pCurrent != pHead));
	}

	return pReturn;
}

/* Implementation of publically callable functions */

PR_IMPLEMENT(SchedulerPtr)
SchedulerStart(void)
{
	SchedulerData	*pNewScheduler = PR_NEWZAP(SchedulerData);

	if (pNewScheduler != NULL) {

		SchedulerInit(pNewScheduler);
	}

	return (SchedulerPtr) pNewScheduler;
}


PR_IMPLEMENT(SchedulerErr)
SchedulerStop(SchedulerPtr pArg)
{
	SchedulerData		*pData = (SchedulerData *) pArg;

	if (pData == NULL) {
		PR_ASSERT(pData != NULL);

		return SCHED_ERR_INVALID_SCHEDULER;
	}

	pData->flags |= SCHED_TERMINATED;
	
	PR_Lock(pData->pTimerLock);
	PR_NotifyCondVar(pData->pTimerCVar);
	PR_Unlock(pData->pTimerLock);

	return SCHED_ERR_NOERR;
}

PR_IMPLEMENT(SchedulerErr)
SchedulerPause(SchedulerPtr pArg)
{
	SchedulerData		*pData = (SchedulerData *) pArg;

	if (pData == NULL) {
		PR_ASSERT(pData != NULL);

		return SCHED_ERR_INVALID_SCHEDULER;
	}

	pData->flags |= SCHED_PAUSED;
	return SCHED_ERR_NOERR;
}

PR_IMPLEMENT(SchedulerErr)
SchedulerResume(SchedulerPtr pArg)
{
	SchedulerData		*pData = (SchedulerData *) pArg;

	if (pData == NULL) {
		PR_ASSERT(pData != NULL);

		return SCHED_ERR_INVALID_SCHEDULER;
	}

	pData->flags = (pData->flags & ~SCHED_PAUSED);
	return SCHED_ERR_NOERR;
}


PR_IMPLEMENT(SchedulerErr)
SchedulerAddEvent(SchedulerPtr pArg,
							   PRUint32 *pEventID,
							   const char *szName,
							   SchedulerTime *pTime,
							   SchedFuncPtr function,
							   SchedFuncDataPtr pData) 
{
	SchedulerData		*pScheduler = (SchedulerData *) pArg;
	SchedEvent			*pNewEvent;	
	SchedulerErr		err = SCHED_ERR_NOERR;

	if (pScheduler == NULL) {
		PR_ASSERT(pScheduler != NULL);

		return SCHED_ERR_INVALID_SCHEDULER;
	}

	if (function == NULL) {
		return SCHED_ERR_BAD_PARAMETER;
	}

	if ((pTime == NULL) || (pTime->range < 0) ||
		(LL_CMP(pTime->baseTime, <, pTime->start)) ||
		((!LL_IS_ZERO(pTime->end)) && (LL_CMP(pTime->baseTime, >, pTime->end)))) {
		return SCHED_ERR_BAD_TIME;
	}

	pNewEvent = PR_NEWZAP(SchedEvent);

	if (pNewEvent == NULL) {
		return SCHED_ERR_OUT_OF_MEMORY;
	}

	pNewEvent->eventTime = *pTime;
	pNewEvent->pEventProc = function;
	pNewEvent->pEventData = pData;

	if (szName != NULL) {
		pNewEvent->eventName = PR_Malloc(strlen(szName) + 1);
		strcpy(pNewEvent->eventName, szName);
	}

	PR_Lock(pScheduler->pTimerLock);

	pNewEvent->eventID = pScheduler->nextID++;
	
	if (pEventID != NULL) {
		*pEventID = pNewEvent->eventID;
	}

	/* compute the firing time */
	/* The first firing time is computed as follows: Select a random positive integer
	   within the range.  Multiply the number by 60 to obtain seconds,
	   then add that to the base time to get a time.  If the time is after the end time,
	   pin to the end time.  If it's before the start time, pin to the start time.*/
	{
		PRTime				fireTime;
		PRInt32				random, randomSecs;
		PRInt64				randomizer, us2s;

		if (pTime->range != 0) {
			random = (PRInt32) rand();
			randomSecs = random % pTime->range;

			LL_UI2L(us2s, PR_USEC_PER_SEC);
			LL_I2L(randomizer, randomSecs);
			LL_MUL(randomizer, randomizer, us2s);

			if ((random % 2) == 0) {
				LL_NEG(randomizer, randomizer);
			}

			LL_ADD(fireTime, pTime->baseTime, randomizer);

		} else {
			fireTime = pTime->baseTime;
		}

		if ((LL_NE(LL_ZERO, pTime->end)) && (LL_CMP(fireTime, >, pTime->end))) {
			pNewEvent->fireTime = pTime->end;
		} else if (LL_CMP(fireTime, <, pTime->start)) {
			pNewEvent->fireTime = pTime->start;
		} else {
			pNewEvent->fireTime = fireTime;
		}
	}

	/* insert the item in the proper place in the list */

	InsertEvent(pScheduler, pNewEvent);

	PR_NotifyCondVar(pScheduler->pTimerCVar);

	PR_Unlock(pScheduler->pTimerLock);

	/* notify observers */
	NotifyObservers(pScheduler, SCHED_OBSERVE_ADD, pNewEvent->eventID, pNewEvent->eventName, pData);

	return err;
}

PR_IMPLEMENT(SchedulerErr)
SchedulerRemoveEvent(SchedulerPtr pArg, PRUint32 eventID)
{
	SchedulerData	*pScheduler = (SchedulerData *) pArg;
	SchedEvent		*pEvent;
	SchedulerErr	err = SCHED_ERR_BAD_EVENT;

	if (pScheduler == NULL) {
		PR_ASSERT(pScheduler != NULL);

		return SCHED_ERR_INVALID_SCHEDULER;
	}

	PR_Lock(pScheduler->pTimerLock);

	pEvent = EventFromID(pScheduler, eventID);

	if (pEvent != NULL) {

		PR_REMOVE_LINK(pEvent);

		if (LL_EQ(pScheduler->nextEventTime, pEvent->fireTime)) {

			/* the item we just removed was the next item to fire,
			   so reinitialize the firing time */

			if (PR_CLIST_IS_EMPTY(pScheduler->pEventQueue)) {
				pScheduler->nextEventTime = LL_ZERO;
			} else {
				SchedEvent		*pNextEvent = (SchedEvent *) PR_LIST_HEAD(pScheduler->pEventQueue);

				pScheduler->nextEventTime = pNextEvent->fireTime;
			}

			PR_NotifyCondVar(pScheduler->pTimerCVar);
		}
		
		err = SCHED_ERR_NOERR; 
	}

	PR_Unlock(pScheduler->pTimerLock);

	if (pEvent != NULL) {

		/* notify observers */
		NotifyObservers(pScheduler, SCHED_OBSERVE_REMOVE, pEvent->eventID, pEvent->eventName, pEvent->pEventData);

		/* free the event */
		if (pEvent->eventName != NULL) {
			PR_Free(pEvent->eventName);
		}

		PR_DELETE(pEvent);
	}


	return err;
}

PR_EXTERN(SchedulerErr)
SchedulerAddObserver(SchedulerPtr pArg,
					 PRUint32 *pObserverID,
					 SchedObsType typeMask,
					 SchedObsPtr function,
					 SchedObsDataPtr pData)
{
	SchedulerData	*pScheduler = (SchedulerData *) pArg;
	SchedObserver	*pNewObserver = NULL;

	if (pScheduler == NULL) {
		PR_ASSERT(pScheduler != NULL);

		return SCHED_ERR_INVALID_SCHEDULER;
	}

	if (function == NULL) {
		return SCHED_ERR_BAD_PARAMETER;
	}

	pNewObserver = PR_NEWZAP(SchedObserver);

	if (pNewObserver == NULL) {
		return SCHED_ERR_OUT_OF_MEMORY;
	}

	PR_Lock(pScheduler->pTimerLock);

	if (pScheduler->pObserverQueue == NULL) {
		pScheduler->pObserverQueue = PR_Malloc(sizeof(PRCList));

		if (pScheduler->pObserverQueue == NULL) {
			if (pNewObserver != NULL) {
				PR_Free(pNewObserver);
			}

			PR_Unlock(pScheduler->pTimerLock);

			return SCHED_ERR_OUT_OF_MEMORY;
		} else {
			PR_INIT_CLIST(pScheduler->pObserverQueue);
		}
	}

	pNewObserver->observerID = pScheduler->nextObserver++;
	
	if (pObserverID != NULL) {
		*pObserverID = pNewObserver->observerID;
	}

	pNewObserver->typeMask = typeMask;
	pNewObserver->pObserverProc = function;
	pNewObserver->pObserverData = pData;

	PR_APPEND_LINK((PRCList *) pNewObserver, pScheduler->pObserverQueue);

	PR_Unlock(pScheduler->pTimerLock);

	return SCHED_ERR_NOERR;
}


PR_IMPLEMENT(SchedulerErr)
SchedulerRemoveObserver(SchedulerPtr pArg, PRUint32 observerID)
{
	SchedulerData	*pScheduler = (SchedulerData *) pArg;
	SchedObserver	*pObserver;
	SchedulerErr	err = SCHED_ERR_BAD_PARAMETER;

	if (pScheduler == NULL) {
		PR_ASSERT(pScheduler != NULL);

		return SCHED_ERR_INVALID_SCHEDULER;
	}

	PR_Lock(pScheduler->pTimerLock);

	pObserver = ObserverFromID(pScheduler, observerID);

	if (pObserver != NULL) {
		PR_REMOVE_LINK(pObserver);
		PR_DELETE(pObserver);

		err = SCHED_ERR_NOERR; 
	}

	PR_Unlock(pScheduler->pTimerLock);

	return err;
}


#ifdef DEBUG

PR_IMPLEMENT(void)
Sched_FormatEventTime(char *output, int len, SchedulerTime *pTime)
{
	PRExplodedTime	exp_time;
	char			scratch[100];
	char			returnValue[1024];

	PR_ExplodeTime(pTime->baseTime, PR_LocalTimeParameters, &exp_time);
	PR_FormatTime(scratch, 100, "%X on %x", &exp_time);
	sprintf(returnValue, "%s (+/- %ld seconds)", scratch, pTime->range);

	if (pTime->repeating !=0) {
		sprintf(scratch, "repeating every %d seconds,\n", pTime->repeating);
	} else {
		strcpy(scratch, "(nonrepeating),\n");
	}

	strcat(returnValue, scratch);

	PR_ExplodeTime(pTime->start, PR_LocalTimeParameters, &exp_time);
	PR_FormatTime(scratch, 100, "beginning at %X on %x\n", &exp_time);
	strcat(returnValue, scratch);

	if (LL_IS_ZERO(pTime->end)) {
		strcpy(scratch, "and continuing until cancelled");
	} else {
		PR_ExplodeTime(pTime->end, PR_LocalTimeParameters, &exp_time);
		PR_FormatTime(scratch, 100, "and stopping at %X on %x\n", &exp_time);
	}

	strcat(returnValue, scratch);

	strncpy(output, returnValue, len);
}

#endif

