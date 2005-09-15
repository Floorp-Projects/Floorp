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
 * The Original Code is mozilla.org Code.
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

/**********************************************************************
NSPL Events

Defining Events
---------------

Events are essentially structures that represent argument lists for a
function that will run on another thread. All event structures you
define must include a PLEvent struct as their first field:

    typedef struct MyEventType {
        PLEvent	e;
		// arguments follow...
        int	x;
		char*	y;
    } MyEventType;

It is also essential that you establish a model of ownership for each
argument passed in an event record, i.e. whether particular arguments
will be deleted by the event destruction callback, or whether they
only loaned to the event handler callback, and guaranteed to persist
until the time at which the handler is called.

Sending Events
--------------

Events are initialized by PL_InitEvent and can be sent via
PL_PostEvent or PL_PostSynchronousEvent. Events can also have an
owner. The owner of an event can revoke all the events in a given
event-queue by calling PL_RevokeEvents. An owner might want
to do this if, for instance, it is being destroyed, and handling the
events after the owner's destruction would cause an error (e.g. an
MWContext).

Since the act of initializing and posting an event must be coordinated
with it's possible revocation, it is essential that the event-queue's
monitor be entered surrounding the code that constructs, initializes
and posts the event:

    void postMyEvent(MyOwner* owner, int x, char* y)
    {
		MyEventType* event;

		PL_ENTER_EVENT_QUEUE_MONITOR(myQueue);

		// construct
		event = PR_NEW(MyEventType);
		if (event == NULL) goto done;

		// initialize
		PL_InitEvent(event, owner,
					 (PLHandleEventProc)handleMyEvent,
					 (PLDestroyEventProc)destroyMyEvent);
		event->x = x;
		event->y = strdup(y);

		// post
		PL_PostEvent(myQueue, &event->e);

      done:
		PL_EXIT_EVENT_QUEUE_MONITOR(myQueue);
    }

If you don't call PL_InitEvent and PL_PostEvent within the
event-queue's monitor, you'll get a big red assert. 

Handling Events
---------------

To handle an event you must write a callback that is passed the event
record you defined containing the event's arguments:

    void* handleMyEvent(MyEventType* event)
    {
		doit(event->x, event->y);
		return NULL;	// you could return a value for a sync event
    }

Similarly for the destruction callback:

    void destroyMyEvent(MyEventType* event)
    {
		free(event->y);	// created by strdup
		free(event);
    }

Processing Events in Your Event Loop
------------------------------------

If your main loop only processes events delivered to the event queue,
things are rather simple. You just get the next event (which may
block), and then handle it:

    while (1) {
		event = PL_GetEvent(myQueue);
		PL_HandleEvent(event);
    }

However, if other things must be waited on, you'll need to obtain a
file-descriptor that represents your event queue, and hand it to select:

    fd = PL_GetEventQueueSelectFD(myQueue);
    ...add fd to select set...
    while (select(...)) {
		if (...fd...) {
		    PL_ProcessPendingEvents(myQueue);
		}
		...
    }

Of course, with Motif and Windows it's more complicated than that, and
on Mac it's completely different, but you get the picture.

Revoking Events
---------------
If at any time an owner of events is about to be destroyed, you must
take steps to ensure that no one tries to use the event queue after
the owner is gone (or a crash may result). You can do this by either
processing all the events in the queue before destroying the owner:

    {
		...
		PL_ENTER_EVENT_QUEUE_MONITOR(myQueue);
		PL_ProcessPendingEvents(myQueue);
		DestroyMyOwner(owner);
		PL_EXIT_EVENT_QUEUE_MONITOR(myQueue);
		...
    }

or by revoking the events that are in the queue for that owner. This
removes them from the queue and calls their destruction callback:

    {
		...
		PL_ENTER_EVENT_QUEUE_MONITOR(myQueue);
		PL_RevokeEvents(myQueue, owner);
		DestroyMyOwner(owner);
		PL_EXIT_EVENT_QUEUE_MONITOR(myQueue);
		...
    }

In either case it is essential that you be in the event-queue's monitor
to ensure that all events are removed from the queue for that owner, 
and to ensure that no more events will be delivered for that owner.
**********************************************************************/

#ifndef plevent_h___
#define plevent_h___

#include "prtypes.h"
#include "prclist.h"
#include "prthread.h"
#include "prlock.h"
#include "prcvar.h"
#include "prmon.h"

#include "nscore.h"

/* For HWND */
#if defined(XP_WIN32)
#include <windef.h>
#elif defined(XP_OS2)
#define INCL_DOSMISC
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_WINSHELLDATA
#include <os2.h>
#endif

PR_BEGIN_EXTERN_C

/* Typedefs */

typedef struct PLEvent PLEvent;
typedef struct PLEventQueue PLEventQueue;

/*******************************************************************************
 * Event Queue Operations
 ******************************************************************************/

/*
** Creates a new event queue. Returns NULL on failure.
*/
NS_COM PLEventQueue*
PL_CreateEventQueue(const char* name, PRThread* handlerThread);


/* -----------------------------------------------------------------------
** FUNCTION: PL_CreateNativeEventQueue()
** 
** DESCRIPTION:
** PL_CreateNativeEventQueue() creates an event queue that
** uses platform specific notify mechanisms.
** 
** For Unix, the platform specific notify mechanism provides
** an FD that may be extracted using the function
** PL_GetEventQueueSelectFD(). The FD returned may be used in
** a select() function call.
** 
** For Windows, the platform specific notify mechanism
** provides an event receiver window that is called by
** Windows to process the event using the windows message
** pump engine.
** 
** INPUTS: 
**  name:   A name, as a diagnostic aid.
** 
**  handlerThread: A pointer to the PRThread structure for
** the thread that will "handle" events posted to this event
** queue.
**
** RETURNS: 
** A pointer to a PLEventQueue structure or NULL.
** 
*/
NS_COM PLEventQueue * 
    PL_CreateNativeEventQueue(
        const char *name, 
        PRThread *handlerThread
    );

/* -----------------------------------------------------------------------
** FUNCTION: PL_CreateMonitoredEventQueue()
** 
** DESCRIPTION:
** PL_CreateMonitoredEventQueue() creates an event queue. No
** platform specific notify mechanism is created with the
** event queue.
** 
** Users of this type of event queue must explicitly poll the
** event queue to retreive and process events.
** 
** 
** INPUTS: 
**  name:   A name, as a diagnostic aid.
** 
**  handlerThread: A pointer to the PRThread structure for
** the thread that will "handle" events posted to this event
** queue.
**
** RETURNS: 
** A pointer to a PLEventQueue structure or NULL.
** 
*/
NS_COM PLEventQueue * 
    PL_CreateMonitoredEventQueue(
        const char *name,
        PRThread *handlerThread
    );

/*
** Destroys an event queue.
*/
NS_COM void
PL_DestroyEventQueue(PLEventQueue* self);

/* 
** Returns the monitor associated with an event queue. This monitor is 
** selectable. The monitor should be entered to protect against anyone 
** calling PL_RevokeEvents while the event is trying to be constructed
** and delivered.
*/
NS_COM PRMonitor*
PL_GetEventQueueMonitor(PLEventQueue* self);

#define PL_ENTER_EVENT_QUEUE_MONITOR(queue)	\
	PR_EnterMonitor(PL_GetEventQueueMonitor(queue))

#define PL_EXIT_EVENT_QUEUE_MONITOR(queue)	\
	PR_ExitMonitor(PL_GetEventQueueMonitor(queue))

/*
** Posts an event to an event queue, waking up any threads waiting for an
** event. If event is NULL, notification still occurs, but no event will
** be available. 
**
** Any events delivered by this routine will be destroyed by PL_HandleEvent
** when it is called (by the event-handling thread).
*/
NS_COM PRStatus
PL_PostEvent(PLEventQueue* self, PLEvent* event);

/*
** Like PL_PostEvent, this routine posts an event to the event handling
** thread, but does so synchronously, waiting for the result. The result
** which is the value of the handler routine is returned.
**
** Any events delivered by this routine will be not be destroyed by 
** PL_HandleEvent, but instead will be destroyed just before the result is
** returned (by the current thread).
*/
NS_COM void*
PL_PostSynchronousEvent(PLEventQueue* self, PLEvent* event);

/*
** Gets an event from an event queue. Returns NULL if no event is
** available.
*/
NS_COM PLEvent*
PL_GetEvent(PLEventQueue* self);

/*
** Returns true if there is an event available for PL_GetEvent.
*/
NS_COM PRBool
PL_EventAvailable(PLEventQueue* self);

/*
** This is the type of the function that must be passed to PL_MapEvents
** (see description below).
*/
typedef void 
(PR_CALLBACK *PLEventFunProc)(PLEvent* event, void* data, PLEventQueue* queue);

/*
** Applies a function to every event in the event queue. This can be used
** to selectively handle, filter, or remove events. The data pointer is
** passed to each invocation of the function fun.
*/
NS_COM void
PL_MapEvents(PLEventQueue* self, PLEventFunProc fun, void* data);

/*
** This routine walks an event queue and destroys any event whose owner is
** the owner specified. The == operation is used to compare owners.
*/
NS_COM void
PL_RevokeEvents(PLEventQueue* self, void* owner);

/*
** This routine processes all pending events in the event queue. It can be
** called from the thread's main event-processing loop whenever the event
** queue's selectFD is ready (returned by PL_GetEventQueueSelectFD).
*/
NS_COM void
PL_ProcessPendingEvents(PLEventQueue* self);

/*******************************************************************************
 * Pure Event Queues
 *
 * For when you're only processing PLEvents and there is no native
 * select, thread messages, or AppleEvents.
 ******************************************************************************/

/*
** Blocks until an event can be returned from the event queue. This routine
** may return NULL if the current thread is interrupted.
*/
NS_COM PLEvent*
PL_WaitForEvent(PLEventQueue* self);

/*
** One stop shopping if all you're going to do is process PLEvents. Just
** call this and it loops forever processing events as they arrive. It will
** terminate when your thread is interrupted or dies.
*/
NS_COM void
PL_EventLoop(PLEventQueue* self);

/*******************************************************************************
 * Native Event Queues
 *
 * For when you need to call select, or WaitNextEvent, and yet also want
 * to handle PLEvents.
 ******************************************************************************/

/*
** This routine allows you to grab the file descriptor associated with an
** event queue and use it in the readFD set of select. Useful for platforms
** that support select, and must wait on other things besides just PLEvents.
*/
NS_COM PRInt32
PL_GetEventQueueSelectFD(PLEventQueue* self);

/*
**  This routine will allow you to check to see if the given eventQueue in
**  on the current thread.  It will return PR_TRUE if so, else it will return
**  PR_FALSE
*/
NS_COM PRBool
    PL_IsQueueOnCurrentThread( PLEventQueue *queue );

/*
** Returns whether the queue is native (true) or monitored (false)
*/
NS_COM PRBool
PL_IsQueueNative(PLEventQueue *queue);

/*******************************************************************************
 * Event Operations
 ******************************************************************************/

/*
** The type of an event handler function. This function is passed as an
** initialization argument to PL_InitEvent, and called by
** PL_HandleEvent. If the event is called synchronously, a void* result 
** may be returned (otherwise any result will be ignored).
*/
typedef void*
(PR_CALLBACK *PLHandleEventProc)(PLEvent* self);

/*
** The type of an event destructor function. This function is passed as
** an initialization argument to PL_InitEvent, and called by
** PL_DestroyEvent.
*/
typedef void
(PR_CALLBACK *PLDestroyEventProc)(PLEvent* self);

/*
** Initializes an event. Usually events are embedded in a larger event
** structure which holds event-specific data, so this is an initializer
** for that embedded part of the structure.
*/
NS_COM void
PL_InitEvent(PLEvent* self, void* owner,
			 PLHandleEventProc handler,
			 PLDestroyEventProc destructor);

/*
** Returns the owner of an event. 
*/
NS_COM void*
PL_GetEventOwner(PLEvent* self);

/*
** Handles an event, calling the event's handler routine.
*/
NS_COM void
PL_HandleEvent(PLEvent* self);

/*
** Destroys an event, calling the event's destructor.
*/
NS_COM void
PL_DestroyEvent(PLEvent* self);

/*
** Removes an event from an event queue.
*/
NS_COM void
PL_DequeueEvent(PLEvent* self, PLEventQueue* queue);


/*
 * Give hint to native PL_Event notification mechanism. If the native 
 * platform needs to tradeoff performance vs. native event starvation
 * this hint tells the native dispatch code which to favor.
 * The default is to prevent event starvation. 
 * 
 * Calls to this function may be nested. When the number of calls that 
 * pass PR_TRUE is subtracted from the number of calls that pass PR_FALSE 
 * is greater than 0, performance is given precedence over preventing 
 * event starvation.
 *
 * The starvationDelay arg is only used when 
 * favorPerformanceOverEventStarvation is PR_FALSE. It is the
 * amount of time in milliseconds to wait before the PR_FALSE actually 
 * takes effect.
 */
NS_COM void
PL_FavorPerformanceHint(PRBool favorPerformanceOverEventStarvation, PRUint32 starvationDelay);


/*******************************************************************************
 * Private Stuff
 ******************************************************************************/

struct PLEvent {
    PRCList				link;
    PLHandleEventProc	handler;
    PLDestroyEventProc	destructor;
    void*				owner;
    void*				synchronousResult;
    PRLock*             lock;
    PRCondVar*          condVar;
    PRBool              handled;
#ifdef PL_POST_TIMINGS
    PRIntervalTime      postTime;
#endif
#ifdef XP_UNIX
    unsigned long       id;
#endif /* XP_UNIX */
    /* other fields follow... */
};

/******************************************************************************/

/*
** Returns the event queue associated with the main thread.
** 
*/
#if defined(XP_WIN) || defined(XP_OS2)
/* -----------------------------------------------------------------------
** FUNCTION: PL_GetNativeEventReceiverWindow()
** 
** DESCRIPTION:
** PL_GetNativeEventReceiverWindow() returns the windows
** handle of the event receiver window associated with the
** referenced PLEventQueue argument.
** 
** INPUTS: 
**  PLEventQueue pointer
**
** RETURNS:
**  event receiver window handle.
** 
** RESTRICTIONS: MS-Windows ONLY.
** 
*/
NS_COM HWND 
    PL_GetNativeEventReceiverWindow( 
        PLEventQueue *eqp 
    );
#endif /* XP_WIN || XP_OS2 */

#ifdef XP_UNIX
/* -----------------------------------------------------------------------
** FUNCTION: PL_ProcessEventsBeforeID()
**
** DESCRIPTION:
**
** PL_ProcessEventsBeforeID() will process events in a native event
** queue that have an id that is older than the ID passed in.
**
** INPUTS:
**  PLEventQueue *aSelf
**  unsigned long aID
**
** RETURNS:
**  PRInt32 number of requests processed, -1 on error.
**
** RESTRICTIONS: Unix only (well, X based unix only)
*/
NS_COM PRInt32
PL_ProcessEventsBeforeID(PLEventQueue *aSelf, unsigned long aID);

/* This prototype is a function that can be called when an event is
   posted to stick an ID on it.  */

typedef unsigned long
(PR_CALLBACK *PLGetEventIDFunc)(void *aClosure);


/* -----------------------------------------------------------------------
** FUNCTION: PL_RegisterEventIDFunc()
**
** DESCRIPTION:
**
** This function registers a function for getting the ID on unix for
** this event queue.
**
** INPUTS:
**  PLEventQueue *aSelf
**  PLGetEventIDFunc func
**  void *aClosure
**
** RETURNS:
**  void
**
** RESTRICTIONS: Unix only (well, X based unix only) */
NS_COM void
PL_RegisterEventIDFunc(PLEventQueue *aSelf, PLGetEventIDFunc aFunc,
                       void *aClosure);

/* -----------------------------------------------------------------------
** FUNCTION: PL_RegisterEventIDFunc()
**
** DESCRIPTION:
**
** This function unregisters a function for getting the ID on unix for
** this event queue.
**
** INPUTS:
**  PLEventQueue *aSelf
**
** RETURNS:
**  void
**
** RESTRICTIONS: Unix only (well, X based unix only) */
NS_COM void
PL_UnregisterEventIDFunc(PLEventQueue *aSelf);

#endif /* XP_UNIX */


/* ----------------------------------------------------------------------- */

#if defined(NO_NSPR_10_SUPPORT)
#else
/********* ???????????????? FIX ME       ??????????????????????????? *****/
/********************** Some old definitions *****************************/

/* Re: prevent.h->plevent.h */
#define PREvent PLEvent
#define PREventQueue PLEventQueue
#define PR_CreateEventQueue PL_CreateEventQueue
#define PR_DestroyEventQueue PL_DestroyEventQueue
#define PR_GetEventQueueMonitor PL_GetEventQueueMonitor
#define PR_ENTER_EVENT_QUEUE_MONITOR PL_ENTER_EVENT_QUEUE_MONITOR
#define PR_EXIT_EVENT_QUEUE_MONITOR PL_EXIT_EVENT_QUEUE_MONITOR
#define PR_PostEvent PL_PostEvent
#define PR_PostSynchronousEvent PL_PostSynchronousEvent
#define PR_GetEvent PL_GetEvent
#define PR_EventAvailable PL_EventAvailable
#define PREventFunProc PLEventFunProc
#define PR_MapEvents PL_MapEvents
#define PR_RevokeEvents PL_RevokeEvents
#define PR_ProcessPendingEvents PL_ProcessPendingEvents
#define PR_WaitForEvent PL_WaitForEvent
#define PR_EventLoop PL_EventLoop
#define PR_GetEventQueueSelectFD PL_GetEventQueueSelectFD
#define PRHandleEventProc PLHandleEventProc
#define PRDestroyEventProc PLDestroyEventProc
#define PR_InitEvent PL_InitEvent
#define PR_GetEventOwner PL_GetEventOwner
#define PR_HandleEvent PL_HandleEvent
#define PR_DestroyEvent PL_DestroyEvent
#define PR_DequeueEvent PL_DequeueEvent
#define PR_GetMainEventQueue PL_GetMainEventQueue

/********* ????????????? End Fix me ?????????????????????????????? *****/
#endif /* NO_NSPR_10_SUPPORT */

PR_END_EXTERN_C

#endif /* plevent_h___ */
