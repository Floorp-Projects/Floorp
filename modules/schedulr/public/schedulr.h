/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*** schedulr.h *****************************************************

  The scheduling service allows clients to specify a time or range
  of times at which a particular event is to occur. These events,
  specified as callback functions, execute in their own thread and
  can be set up to occur repeatedly or as a one-shot deal.

  Full HTML-based specifications for the operation of the scheduler,
  including a complete API reference, are checked in to this
  directory as schedulr.htm.  The information given there is a
  superset of the comments in this file.

  $Revision: 3.1 $
  $Date: 1998/03/28 03:36:00 $

 *********************************************************************/

#ifndef schedulr_h___
#define schedulr_h___

#include <prcvar.h>
#include <prlock.h>
#include <prtime.h>
#include <prtypes.h>
#include <prthread.h>

/*********** Types **********/

typedef void *SchedulerPtr;

/*
	All Scheduler APIs (with the exception of SchedulerStart) require a reference
	to a scheduler instance in the form of a reference of type SchedulerPtr.  A
	SchedulerPtr is returned from SchedulerStart, and its value is used as a handle
	for a scheduler.  The structure referenced by a SchedulerPtr should be considered
	an opaque entity; do not directly manipulate any data referenced by a
	SchedulerPtr reference.
*/

typedef enum SchedulerErr {
	SCHED_ERR_BEGIN = -5,
	SCHED_ERR_BAD_EVENT = -5,
	SCHED_ERR_BAD_TIME,
	SCHED_ERR_BAD_PARAMETER,
	SCHED_ERR_OUT_OF_MEMORY,
	SCHED_ERR_INVALID_SCHEDULER,
	SCHED_ERR_NOERR = 0,
	SCHED_WARN_EVENTS_DROPPED,
	SCHED_WARN_END = 1
} SchedulerErr;

/*
	Most APIs for the scheduler return a SchedulerErr type.  SchedulerErr is a typedef
	for a signed integer value; zero is defined as no error (successful operation).
	Negative values are errors (could not complete operation), and positive values are
	warnings (operation completed with comments). All functions (even those which below
	indicate that they "cannot fail") can return a ERR_SCHEDULER_INVALID if the passed
	in scheduler reference is NULL. 
*/

typedef struct  _SchedulerTime {
	PRUint32	repeating;		/* 0 = not repeating, otherwise seconds to add to base time for next time */
	PRInt32		range;			/* Range must be a positive number of seconds */
	PRTime		baseTime;
	PRTime		start;
	PRTime		end;
} SchedulerTime;

/*
	In order to instruct the scheduler to execute an event at a particular time, clients
	specify a scheduled time expressed in a SchedulerTime structure.  The SchedulerTime
	structure has values which correspond to the event's firing time (baseTime), and a
	start and end time from which the event is valid (start and end, respectively).  
	Additionally, SchedulerTime specifies a range, which acts as a randomization value
	within which the event's scheduled time can "drift," and a repeating interval, both
	of whch are expressed in seconds.

	See schedulr.htm for more information on how events are sscheduled.
*/

typedef void *SchedFuncDataPtr;
typedef void (PR_CALLBACK *SchedFuncPtr)(SchedulerPtr pScheduler, PRUint32 eventID,
										 SchedFuncDataPtr pData);

/*
	SchedFuncPtr is a prototype for the function which is called when the
	scheduler fires an event.  The function returns nothing and accepts
	three parameters, a reference to the scheduler from which the event
	was dispatched, the event ID which identifies the event that executed
	the function and an arbitrary data argument passed in at event creation.
	The data argument is opaque to the scheduler and can be used by clients
	to transmit or store state information, etc.  The function is called
	asynchronously in its own thread, which terminates when the function exits.
*/

typedef PRInt32 SchedObsType;

#define	SCHED_OBSERVE_ADD		1
#define	SCHED_OBSERVE_FIRE		2
#define	SCHED_OBSERVE_REMOVE	4
#define	SCHED_OBSERVE_PAUSE		8	/* not implemented */
#define	SCHED_OBSERVE_RESUME	16	/* not implemented */

typedef void *SchedObsDataPtr;
typedef void (PR_CALLBACK *SchedObsPtr)(SchedulerPtr pScheduler, PRUint32 observerID,
										SchedObsDataPtr pData, SchedObsType type,
										PRUint32 eventID, const char *eventName, 
										SchedFuncDataPtr pEventData);

/*
	Observer callbacks are specified using the prototype given as
	SchedObsPtr.  When an event is added or removed from the
	scheduler, or when an event fires, each observer who has registered
	with that instance of the scheduler receives a notification
	message.  The function returns nothing and receives information
	on the event that triggered the notification (its id, name, and event
	data), what kind of operation (add, remove, fire.) occurred, and
	the id and data associated with the specific observer.  Both data
	pointers are opaque to the scheduler.  The event data passed in
	(pEventData) is the actual data pointer for the event, not a copy,
	so observers must be aware that changing the information referred
	to in pEventData may have unexpected results.  Additionally, because
	there is no type information associated with pEventData, observers
	must take particular care when making assumptions about the structure
	or usage of pEventData.
*/


/*********** Public APIs **********/

NSPR_BEGIN_EXTERN_C

PR_EXTERN(SchedulerPtr)
SchedulerStart(void);

/*
	Creates and initializes an instance of the scheduler.  SchedulerStart
	creates a new (local) NSPR thread in which to run the instance
	of the scheduler and returns a reference to a newly created scheduler
	object.  This object is required for all calls to the scheduler API.
	The newly created scheduler object is immediately ready to register and
	dispatch events.  SchedulerStart allocates only enough memory for the
	scheduler object itself; any supplementary data structures including the
	event and observer queues are allocated when they are first referenced by
	EventAdd or ObserverAdd.  If the thread can not be created or memory can
	not be allocated, SchedulerStart returns NULL.  In order to properly
	cleanup, SchedulerStop() should be called when the scheduler is to cease
	operation. 
*/

PR_EXTERN(SchedulerErr)
SchedulerStop(SchedulerPtr pScheduler);

/*
	SchedulerStop halts scheduling of the specified scheduler instance,
	deletes any memory referenced by the scheduler's event and observer
	queues, and frees the instance of the Scheduler.  After calling
	SchedulerStop, the instance of the scheduler referenced by the specified
	SchedulerPtr is invalid and must not be used.  SchedulerStop can not fail
	and will always return a successful result code. 
*/

PR_EXTERN(SchedulerErr)
SchedulerPause(SchedulerPtr pScheduler);

/*
	SchedulerPause causes the scheduler to cease firing events.  Pausing the
	scheduler does not affect the the addition of new events to the scheduling
	queue, nor does it prevent management of the observer list.  While the
	scheduler is paused, any events scheduled to fire will be held in the
	queue until the scheduler's operation is resumed with SchedulerResume.  
	Calling SchedulerPause multiple times will have no effect beyond the first;
	SchedulerPause can not fail and will always return a successful result code. 
*/

PR_EXTERN(SchedulerErr)
SchedulerResume(SchedulerPtr pScheduler);

/*
	SchedulerResume reverses the effect of a previous SchedulerPause call.
	Events that did not fire while the scheduler queue was paused will
	immediately fire, unless their scheduled end time (expiration) has passed,
	in which case those items are removed from the queue.  Items removed from
	the queue in this way cause an event deletion notification to be sent to
	the scheduler's observers.  All other conditions, including sending
	SchedulerResume to a scheduler which has not been paused will always
	return a successful result code. 

	Pausing and resuming the scheduler is not reference counted; if multiple
	threads have paused the scheduler, the first to resume it will cause the
	scheduler to start actively processing events. 
*/


PR_EXTERN(SchedulerErr)
SchedulerAddEvent(SchedulerPtr pScheduler,
							   PRUint32 *pEventID,
							   const char *szName,
							   SchedulerTime *pTime,
							   SchedFuncPtr function,
							   SchedFuncDataPtr pData);

/*
	SchedulerAddEvent adds an event to the scheduler's event queue.  A
	client which wishes to add an event needs to specify a time at which
	the event should fire, a function to call when the time occurs, and
	any amount of data which will be passed to the specified function at
	the time that the event is fired.  A client may also supply a user-visible
	name with which to identify the event.  Note that this name is not used by
	the scheduler at all and may be NULL if no user-visible name is desired.
	Both the time and name are copied into the scheduler's internal structures,
	so the memory referenced by these events may be released after making this call. 

	If successful, SchedulerAddEvent adds the event to the event queue, assigns
	a unique event ID which can be used to later refer to this specific event,
	and returns a successful result code.  If NULL is passed in as the reference
	to the pEventID, an event ID is still generated, but the client will be unable
	to change or delete the event in the future.  Note that event IDs are unique
	only to a particular scheduler instance; they are not guaranteed to be globally
	unique across scheduler instances. 

	SchedulerAddEvent can fail for a number of reasons.  The most common is lack of
	memory.  If the event queue needs to be created or expanded in size to accomodate
	the new event, the scheduler will fail, returning SCHED_ERR_OUT_OF_MEMORY, and will
	return 0 as the eventID.  Event addition will also fail if the time specified is
	invalid, is outside the bounds specified by the event's start and end times,
	or if the given range is negative (SCHED_ERR_BAD_TIME); or if a NULL value is given
	for the function (SCHED_ERR_BAD_PARAMETER).

	See schedulr.htm for more detailed information, including the mechanism for
	notifying observers.
*/


PR_EXTERN(SchedulerErr)
SchedulerRemoveEvent(SchedulerPtr pScheduler, PRUint32 eventID);

/*
	SchedulerRemoveEvent removes a previously added event from the scheduler's
	event queue.  The event to remove is specified by its eventID, previously
	assigned by SchedulerAddEvent.  If the specified event ID is invalid, the
	scheduler will return aan error, SCHED_ERR_BAD_EVENT, and no operation will
	be performed. 

	See schedulr.htm for more detailed information, including the mechanism for
	notifying observers.
*/

PR_EXTERN(SchedulerErr)
SchedulerAddObserver(SchedulerPtr pScheduler,
					 PRUint32 *pObserverID,
					 SchedObsType type,
					 SchedObsPtr function,
					 SchedObsDataPtr pData);

/*
	SchedulerAddObserver adds an observer to the specified scheduler's
	observer list. An observer is a function which is called when the
	scheduler adds, removes, or fires an event.  Which of these actions
	trigger a notification for a given observer is specified in the
	observer's type, which is a bitfield made up of the values specified
	under SchedObsType, above.

	Like the corresponding AddEvent function, above, adding an observer
	causes the scheduler to generate a unique observerID, which can later
	be used to remove the observer.  Also like AddEvent, the observerID
	is unique only within a scheduler instance.  ObserverIDs and EventIDs
	are not interchangeable; their namespace may overlap.  Therefore, it's
	important that the caller keep track of which identifiers represent
	events and which represent observers. If NULL is passed in for the
	observerID, an observer ID is generated, but it is not returned to
	the caller. 

	SchedulerAddObserver can fail if memory can not be allocated to
	store the observer or create the observer queue, or if the function
	specified is NULL.  In each failure case, the observerID returned is
	undefined.

	See schedulr.htm for more detailed information on observers and the
	observer notification process.
*/

PR_EXTERN(SchedulerErr)
SchedulerRemoveObserver(SchedulerPtr pScheduler, PRUint32 observerID);

/*
	SchedulerRemoveObserver removes an observer from the scheduer's
	observer list.  That observer no longer receives notification
	messages and the scheduler frees any memory it has allocated
	to track the specified observer.  If no observer with the given
	ID exists, the function returns SCHED_ERR_BAD_PARAMETER, and no
	operation is performed. 
*/

#ifdef DEBUG

PR_EXTERN(void)
Sched_FormatEventTime(char *output, int len, SchedulerTime *pTime);

/*
	Available only in DEBUG mode, this function takes a schedulerTime
	and creates a string representation of it, suitable for displaying
	in a debug message.  The string it returns, the first len characters
	of which are copied into output, is of the format:

    {time} on {date} +/- {range} seconds, repeating every {repeat} seconds},
	beginning on {start} and stopping on {end}

    The algorithm is smart enough to substitue appropriate language for
	non-repeating and never-ending events.
*/

#endif

NSPR_END_EXTERN_C

#endif
