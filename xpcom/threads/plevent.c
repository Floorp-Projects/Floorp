/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#if defined(_WIN32) || defined(WIN16)
#include <windows.h>
#endif

#if defined(XP_OS2)
#define INCL_WIN
#include <os2.h>
#define DefWindowProc WinDefWindowProc
#endif /* XP_OS2 */

#include "nspr.h"
#include "plevent.h"

#if !defined(WIN32)
#include <errno.h>
#include <stddef.h>
#if !defined(XP_OS2)
#include <unistd.h>
#endif /* !XP_OS2 */
#endif /* !Win32 */

#if defined(XP_UNIX)
/* for fcntl */
#include <sys/types.h>
#include <fcntl.h>
#endif

#if defined(XP_BEOS)
#include <kernel/OS.h>
#endif

#if defined(XP_MACOSX)
#undef XP_UNIX
#define XP_MAC 1
#endif

#if defined(XP_MAC)
#include <AppleEvents.h>
#if !defined(DARWIN)
#include "pprthred.h"
#endif
#else
#include "private/pprthred.h"
#endif /* XP_MAC */

#if defined(VMS)
/*
** On OpenVMS, XtAppAddInput doesn't want a regular fd, instead it 
** wants an event flag. So, we don't create and use a pipe for 
** notification of when an event queue has something ready, instead
** we use an event flag. Shouldn't be a problem if we only have
** a few event queues.
*/
#include <lib$routines.h>
#include <starlet.h>
#include <stsdef.h>
#endif /* VMS */

static PRLogModuleInfo *event_lm = NULL;

/*******************************************************************************
 * Private Stuff
 ******************************************************************************/

/*
** EventQueueType -- Defines notification type for an event queue
**
*/
typedef enum 
{
    EventQueueIsNative = 1,
    EventQueueIsMonitored = 2
} EventQueueType;


struct PLEventQueue {
    char*        name;
    PRCList      queue;
    PRMonitor*   monitor;
    PRThread*    handlerThread;
    EventQueueType type;
    PRPackedBool   processingEvents;
    PRPackedBool   notified;
#if defined(XP_UNIX)
#if defined(VMS)
    int		 efn;
#else
    PRInt32      eventPipe[2];
#endif
    PLGetEventIDFunc idFunc;
    void            *idFuncClosure;
#elif defined(_WIN32) || defined(WIN16) || defined(XP_OS2)
    HWND         eventReceiverWindow;
    PRBool       removeMsg;
#elif defined(XP_BEOS)
    port_id      eventport;
#endif
};

#define PR_EVENT_PTR(_qp) \
    ((PLEvent*) ((char*) (_qp) - offsetof(PLEvent, link)))

static PRStatus    _pl_SetupNativeNotifier(PLEventQueue* self);
static void        _pl_CleanupNativeNotifier(PLEventQueue* self);
static PRStatus    _pl_NativeNotify(PLEventQueue* self);
static PRStatus    _pl_AcknowledgeNativeNotify(PLEventQueue* self);
static void        _md_CreateEventQueue( PLEventQueue *eventQueue );
static PRInt32     _pl_GetEventCount(PLEventQueue* self);


#if defined(_WIN32) || defined(WIN16) || defined(XP_OS2)
#if defined(XP_OS2)
ULONG _pr_PostEventMsgId;
#else
UINT _pr_PostEventMsgId;
#endif /* OS2 */
static char *_pr_eventWindowClass = "XPCOM:EventWindow";
#endif /* Win32, Win16, OS2 */

/*******************************************************************************
 * Event Queue Operations
 ******************************************************************************/

/*
** _pl_CreateEventQueue() -- Create the event queue
**
**
*/
static PLEventQueue * _pl_CreateEventQueue(char *name,
                                           PRThread *handlerThread,
                                           EventQueueType  qtype)
{
    PRStatus err;
    PLEventQueue* self = NULL;
    PRMonitor* mon = NULL;

    if (event_lm == NULL)
        event_lm = PR_NewLogModule("event");

    self = PR_NEWZAP(PLEventQueue);
    if (self == NULL) return NULL;

    mon = PR_NewNamedMonitor(name);
    if (mon == NULL) goto error;

    self->name = name;
    self->monitor = mon;
    self->handlerThread = handlerThread;
    self->processingEvents = PR_FALSE;
    self->type = qtype;
#if defined(_WIN32) || defined(WIN16) || defined(XP_OS2)
    self->removeMsg = PR_TRUE;
#endif
    self->notified = PR_FALSE;

    PR_INIT_CLIST(&self->queue);
    if ( qtype == EventQueueIsNative ) {
        err = _pl_SetupNativeNotifier(self);
        if (err) goto error;
        _md_CreateEventQueue( self );
    }
    return self;

  error:
    if (mon != NULL)
        PR_DestroyMonitor(mon);
    PR_DELETE(self);
    return NULL;
} 

PR_IMPLEMENT(PLEventQueue*)
PL_CreateEventQueue(char* name, PRThread* handlerThread)
{
    return( _pl_CreateEventQueue( name, handlerThread, EventQueueIsNative ));
}

PR_EXTERN(PLEventQueue *) 
PL_CreateNativeEventQueue(char *name, PRThread *handlerThread)
{
    return( _pl_CreateEventQueue( name, handlerThread, EventQueueIsNative ));
} 

PR_EXTERN(PLEventQueue *) 
PL_CreateMonitoredEventQueue(char *name, PRThread *handlerThread)
{
    return( _pl_CreateEventQueue( name, handlerThread, EventQueueIsMonitored ));
} 

PR_IMPLEMENT(PRMonitor*)
PL_GetEventQueueMonitor(PLEventQueue* self)
{
    return self->monitor;
}

static void PR_CALLBACK
_pl_destroyEvent(PLEvent* event, void* data, PLEventQueue* queue)
{
#ifdef XP_MAC
#pragma unused (data, queue)
#endif
    PL_DequeueEvent(event, queue);
    PL_DestroyEvent(event);
}

PR_IMPLEMENT(void)
PL_DestroyEventQueue(PLEventQueue* self)
{
    PR_EnterMonitor(self->monitor);

    /* destroy undelivered events */
    PL_MapEvents(self, _pl_destroyEvent, NULL);

    if ( self->type == EventQueueIsNative )
        _pl_CleanupNativeNotifier(self);

    /* destroying the monitor also destroys the name */
    PR_ExitMonitor(self->monitor);
    PR_DestroyMonitor(self->monitor);
    PR_DELETE(self);

}

PR_IMPLEMENT(PRStatus)
PL_PostEvent(PLEventQueue* self, PLEvent* event)
{
    PRStatus err = PR_SUCCESS;
    PRMonitor* mon;

    if (self == NULL)
        return PR_FAILURE;

    mon = self->monitor;
    PR_EnterMonitor(mon);

#ifdef XP_UNIX
    if (self->idFunc && event)
      event->id = self->idFunc(self->idFuncClosure);
#endif

    /* insert event into thread's event queue: */
    if (event != NULL) {
      PR_APPEND_LINK(&event->link, &self->queue);
    }

    if ( self->type == EventQueueIsNative && !self->notified )
    {
      err = _pl_NativeNotify(self);
           
      if (err != PR_SUCCESS)
        goto error;
      
      self->notified = PR_TRUE; 
    }

      /*
       * This may fall on deaf ears if we're really notifying the native 
       * thread, and no one has called PL_WaitForEvent (or PL_EventLoop):
       */
      err = PR_Notify(mon);
      

error:
    PR_ExitMonitor(mon);
    return err;
}

PR_IMPLEMENT(void*)
PL_PostSynchronousEvent(PLEventQueue* self, PLEvent* event)
{
    void* result;

    if (self == NULL)
    return NULL;

    PR_ASSERT(event != NULL);
    PR_Lock(event->lock);

    if (PR_CurrentThread() == self->handlerThread) {
      /* Handle the case where the thread requesting the event handling
       is also the thread that's supposed to do the handling. */
      result = event->handler(event);
    }
    else {
      int i, entryCount = PR_GetMonitorEntryCount(self->monitor);

      event->synchronousResult = (void*)PR_TRUE;

      PL_PostEvent(self, event);

      /* We need to temporarily give up our event queue monitor if
         we're holding it, otherwise, the thread we're going to wait
         for notification from won't be able to enter it to process
         the event. */
      if (entryCount) {
	    for (i = 0; i < entryCount; i++)
          PR_ExitMonitor(self->monitor);
      }
	
      event->handled = PR_FALSE;

      while (!event->handled)
        PR_WaitCondVar(event->condVar, PR_INTERVAL_NO_TIMEOUT);	/* wait for event to be handled or destroyed */
	
      if (entryCount) {
        for (i = 0; i < entryCount; i++)
          PR_EnterMonitor(self->monitor);
      }
      
      result = event->synchronousResult;
      event->synchronousResult = NULL;
    }

    PR_Unlock(event->lock);

    /* For synchronous events, they're destroyed here on the caller's
       thread before the result is returned. See PL_HandleEvent. */
    PL_DestroyEvent(event);

    return result;
}

PR_IMPLEMENT(PLEvent*)
PL_GetEvent(PLEventQueue* self)
{
    PLEvent* event = NULL;
    PRStatus err = PR_SUCCESS;

    if (self == NULL)
      return NULL;

    PR_EnterMonitor(self->monitor);

    if (!PR_CLIST_IS_EMPTY(&self->queue)) 
    {      
      if ( self->type == EventQueueIsNative && 
           self->notified                   && 
           !self->processingEvents          &&
           0 == _pl_GetEventCount(self)     )
      {
        err = _pl_AcknowledgeNativeNotify(self);
        self->notified = PR_FALSE; 
      }
      if (err) goto done;

      /* then grab the event and return it: */
      event = PR_EVENT_PTR(self->queue.next);
      PR_REMOVE_AND_INIT_LINK(&event->link);
    }

  done:
    PR_ExitMonitor(self->monitor);
    return event;
}

PR_IMPLEMENT(PRBool)
PL_EventAvailable(PLEventQueue* self)
{
    PRBool result = PR_FALSE;

    if (self == NULL)
    return PR_FALSE;

    PR_EnterMonitor(self->monitor);

    if (!PR_CLIST_IS_EMPTY(&self->queue)) 
    result = PR_TRUE;

    PR_ExitMonitor(self->monitor);
    return result;
}

PR_IMPLEMENT(void)
PL_MapEvents(PLEventQueue* self, PLEventFunProc fun, void* data)
{
    PRCList* qp;

    if (self == NULL)
    return;

    PR_EnterMonitor(self->monitor);
    qp = self->queue.next;
    while (qp != &self->queue) {
    PLEvent* event = PR_EVENT_PTR(qp);
    qp = qp->next;
    (*fun)(event, data, self);
    }
    PR_ExitMonitor(self->monitor);
}

static void PR_CALLBACK
_pl_DestroyEventForOwner(PLEvent* event, void* owner, PLEventQueue* queue)
{
    PR_ASSERT(PR_GetMonitorEntryCount(queue->monitor) > 0);
    if (event->owner == owner) {
    PR_LOG(event_lm, PR_LOG_DEBUG,
           ("$$$ \tdestroying event %0x for owner %0x", event, owner));
    PL_DequeueEvent(event, queue);
    if (event->synchronousResult == (void*)PR_TRUE) {
        PR_Lock(event->lock);
        event->synchronousResult = NULL;
        event->handled = PR_TRUE;
        PR_NotifyCondVar(event->condVar);
        PR_Unlock(event->lock);
    }
    else {
        PL_DestroyEvent(event);
    }
    }
    else {
    PR_LOG(event_lm, PR_LOG_DEBUG,
           ("$$$ \tskipping event %0x for owner %0x", event, owner));
    }
}

PR_IMPLEMENT(void)
PL_RevokeEvents(PLEventQueue* self, void* owner)
{
    if (self == NULL)
    return;

    PR_LOG(event_lm, PR_LOG_DEBUG,
         ("$$$ revoking events for owner %0x", owner));

    /*
    ** First we enter the monitor so that no one else can post any events
    ** to the queue:
    */
    PR_EnterMonitor(self->monitor);
    PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ owner %0x, entered monitor", owner));

    /*
    ** Discard any pending events for this owner:
    */
    PL_MapEvents(self, _pl_DestroyEventForOwner, owner);

#ifdef DEBUG
    {
	PRCList* qp = self->queue.next;
	while (qp != &self->queue) {
	    PLEvent* event = PR_EVENT_PTR(qp);
	    qp = qp->next;
	    PR_ASSERT(event->owner != owner);
	}
    }
#endif /* DEBUG */

    PR_ExitMonitor(self->monitor);

    PR_LOG(event_lm, PR_LOG_DEBUG,
           ("$$$ revoking events for owner %0x", owner));
}

static PRInt32
_pl_GetEventCount(PLEventQueue* self)
{
    PRCList* node;
    PRInt32  count = 0;

    PR_EnterMonitor(self->monitor);
    node = PR_LIST_HEAD(&self->queue);
    while (node != &self->queue) {
        count++;
        node = PR_NEXT_LINK(node);
    }
    PR_ExitMonitor(self->monitor);

    return count;
}

PR_IMPLEMENT(void)
PL_ProcessPendingEvents(PLEventQueue* self)
{
    PRInt32 count;

    if (self == NULL)
      return;
    
    
    PR_EnterMonitor(self->monitor);
    
    if (self->processingEvents) {
      _pl_AcknowledgeNativeNotify(self);
      self->notified = PR_FALSE;
      PR_ExitMonitor(self->monitor);
      return;
    }
    self->processingEvents = PR_TRUE;

    /* Only process the events that are already in the queue, and
     * not any new events that get added. Do this by counting the
     * number of events currently in the queue
     */
    count = _pl_GetEventCount(self);
    PR_ExitMonitor(self->monitor);
    
    while (count-- > 0) {
      PLEvent* event = PL_GetEvent(self);
      if (event == NULL)
        break;

      PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ processing event"));
      PL_HandleEvent(event);
      PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
    }

    PR_EnterMonitor(self->monitor);

    if (self->type == EventQueueIsNative)
    {
      count = _pl_GetEventCount(self);

      if (count <= 0)
      {         
        _pl_AcknowledgeNativeNotify(self);
        self->notified = PR_FALSE;
      }
      else
      {
        _pl_NativeNotify(self);
        self->notified = PR_TRUE;
      }
      
    }
    self->processingEvents = PR_FALSE;

    PR_ExitMonitor(self->monitor);
}

/*******************************************************************************
 * Event Operations
 ******************************************************************************/

PR_IMPLEMENT(void)
PL_InitEvent(PLEvent* self, void* owner,
             PLHandleEventProc handler,
             PLDestroyEventProc destructor)
{
#ifdef PL_POST_TIMINGS
    self->postTime = PR_IntervalNow();
#endif
    PR_INIT_CLIST(&self->link);
    self->handler = handler;
    self->destructor = destructor;
    self->owner = owner;
    self->synchronousResult = NULL;
    self->handled = PR_FALSE;
    self->lock = PR_NewLock();
    PR_ASSERT(self->lock);
    self->condVar = PR_NewCondVar(self->lock);
    PR_ASSERT(self->condVar);
#ifdef XP_UNIX
    self->id = 0;
#endif
}

PR_IMPLEMENT(void*)
PL_GetEventOwner(PLEvent* self)
{
    return self->owner;
}

PR_IMPLEMENT(void)
PL_HandleEvent(PLEvent* self)
{    
    void* result;
    if (self == NULL)
        return;

    /* This event better not be on an event queue anymore. */
    PR_ASSERT(PR_CLIST_IS_EMPTY(&self->link));

    result = (*self->handler)(self);
    if (NULL != self->synchronousResult) {
      PR_Lock(self->lock);
      self->synchronousResult = result;
      self->handled = PR_TRUE;
      PR_NotifyCondVar(self->condVar);
      PR_Unlock(self->lock);
    }
    else {
      /* For asynchronous events, they're destroyed by the event-handler
         thread. See PR_PostSynchronousEvent. */
      PL_DestroyEvent(self);
    }
}
#ifdef PL_POST_TIMINGS
static long s_eventCount = 0;
static long s_totalTime  = 0;
#endif

PR_IMPLEMENT(void)
PL_DestroyEvent(PLEvent* self)
{
    if (self == NULL)
        return;

    /* This event better not be on an event queue anymore. */
    PR_ASSERT(PR_CLIST_IS_EMPTY(&self->link));

    PR_DestroyCondVar(self->condVar);
    PR_DestroyLock(self->lock);
   
#ifdef PL_POST_TIMINGS
    s_totalTime += PR_IntervalNow() - self->postTime;
    s_eventCount++;
    printf("$$$ running avg (%d) \n", PR_IntervalToMilliseconds(s_totalTime/s_eventCount));
#endif

    (*self->destructor)(self);
}

PR_IMPLEMENT(void)
PL_DequeueEvent(PLEvent* self, PLEventQueue* queue)
{
#ifdef XP_MAC
#pragma unused (queue)
#endif
    if (self == NULL)
    return;

    /* Only the owner is allowed to dequeue events because once the 
       client has put it in the queue, they have no idea whether it's 
       been processed and destroyed or not. */

    PR_ASSERT(queue->handlerThread == PR_CurrentThread());

    PR_EnterMonitor(queue->monitor);

    PR_ASSERT(!PR_CLIST_IS_EMPTY(&self->link));

#if 0
    /*  I do not think that we need to do this anymore.
        if we do not acknowledge and this is the only
        only event in the queue, any calls to process
        the eventQ will be effective noop.
    */
    if (queue->type == EventQueueIsNative)
      _pl_AcknowledgeNativeNotify(queue);
#endif

    PR_REMOVE_AND_INIT_LINK(&self->link);

    PR_ExitMonitor(queue->monitor);
}

/*******************************************************************************
 * Pure Event Queues
 *
 * For when you're only processing PLEvents and there is no native
 * select, thread messages, or AppleEvents.
 ******************************************************************************/

PR_IMPLEMENT(PLEvent*)
PL_WaitForEvent(PLEventQueue* self)
{
    PLEvent* event;
    PRMonitor* mon;

    if (self == NULL)
    return NULL;

    mon = self->monitor;
    PR_EnterMonitor(mon);

    while ((event = PL_GetEvent(self)) == NULL) {
        PRStatus err;
        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ waiting for event"));
        err = PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
        if ((err == PR_FAILURE)
            && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) break;
    }

    PR_ExitMonitor(mon);
    return event;
}

PR_IMPLEMENT(void)
PL_EventLoop(PLEventQueue* self)
{
    if (self == NULL)
    return;

    while (PR_TRUE) {
        PLEvent* event = PL_WaitForEvent(self);
        if (event == NULL) {
            /* This can only happen if the current thread is interrupted */
            return;
        }

        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ processing event"));
        PL_HandleEvent(event);
        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
    }
}

/*******************************************************************************
 * Native Event Queues
 *
 * For when you need to call select, or WaitNextEvent, and yet also want
 * to handle PLEvents.
 ******************************************************************************/

static PRStatus
_pl_SetupNativeNotifier(PLEventQueue* self)
{
#if defined(XP_MAC)
#pragma unused (self)
#endif

#if defined(VMS)
    unsigned int status;
    self->idFunc = 0;
    self->idFuncClosure = 0;
    status = LIB$GET_EF(&self->efn);
    if (!$VMS_STATUS_SUCCESS(status))
        return PR_FAILURE;
    PR_LOG(event_lm, PR_LOG_DEBUG,
       ("$$$ Allocated event flag %d", self->efn));
    return PR_SUCCESS;
#elif defined(XP_UNIX)
    int err;
    int flags;

    self->idFunc = 0;
    self->idFuncClosure = 0;
    
    err = pipe(self->eventPipe);
    if (err != 0) {
        return PR_FAILURE;
    }

    /* make the pipe nonblocking */
    flags = fcntl(self->eventPipe[0], F_GETFL, 0);
    if (flags == -1) {
        goto failed;
    }
    err = fcntl(self->eventPipe[0], F_SETFL, flags | O_NONBLOCK);
    if (err == -1) {
        goto failed;
    }
    flags = fcntl(self->eventPipe[1], F_GETFL, 0);
    if (flags == -1) {
        goto failed;
    }
    err = fcntl(self->eventPipe[1], F_SETFL, flags | O_NONBLOCK);
    if (err == -1) {
        goto failed;
    }
    return PR_SUCCESS;

failed:
    close(self->eventPipe[0]);
    close(self->eventPipe[1]);
    return PR_FAILURE;
#elif defined(XP_BEOS)
	/* hook up to the nsToolkit queue, however the appshell
	 * isn't necessairly started, so we might have to create
	 * the queue ourselves
	 */
	char		portname[64];
	char		semname[64];
	sprintf(portname, "event%lx", self->handlerThread);
	sprintf(semname, "sync%lx", self->handlerThread);

	if((self->eventport = find_port(portname)) < 0)
	{
		/* create port
		 */
		self->eventport = create_port(500, portname);

		/* We don't use the sem, but it has to be there
		 */
		create_sem(0, semname);
	}

    return PR_SUCCESS;
#else
    return PR_SUCCESS;
#endif
}

static void
_pl_CleanupNativeNotifier(PLEventQueue* self)
{
#if defined(XP_MAC)
#pragma unused (self)
#endif

#if defined(VMS)
    {
        unsigned int status;
        PR_LOG(event_lm, PR_LOG_DEBUG,
           ("$$$ Freeing event flag %d", self->efn));
        status = LIB$FREE_EF(&self->efn);
    }
#elif defined(XP_UNIX)
    close(self->eventPipe[0]);
    close(self->eventPipe[1]);
#elif defined(_WIN32) || defined(WIN16)
    DestroyWindow(self->eventReceiverWindow);
#elif defined(XP_OS2)
    WinDestroyWindow(self->eventReceiverWindow);
#endif
}

#if defined(_WIN32) || defined(WIN16)
static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
    PostMessage( self->eventReceiverWindow, _pr_PostEventMsgId,
                 (WPARAM)0, (LPARAM)self);
    return PR_SUCCESS;
}/* --- end _pl_NativeNotify() --- */
#endif

#if defined(XP_OS2)
static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
    BOOL rc = WinPostMsg( self->eventReceiverWindow, _pr_PostEventMsgId,
                          0, MPFROMP(self));
    return (rc == TRUE) ? PR_SUCCESS : PR_FAILURE;
}/* --- end _pl_NativeNotify() --- */
#endif /* XP_OS2 */

#if defined(VMS)
/* Just set the event flag */
static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
        unsigned int status;
        PR_LOG(event_lm, PR_LOG_DEBUG,
           ("_pl_NativeNotify: self=%p efn=%d",
            self, self->efn));
        status = SYS$SETEF(self->efn);
        if ($VMS_STATUS_SUCCESS(status))
            return PR_SUCCESS;
        else
            return PR_FAILURE;
}/* --- end _pl_NativeNotify() --- */
#elif defined(XP_UNIX)

static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
#define NOTIFY_TOKEN    0xFA
    PRInt32 count;
    unsigned char buf[] = { NOTIFY_TOKEN };

    PR_LOG(event_lm, PR_LOG_DEBUG,
           ("_pl_NativeNotify: self=%p",
            self));
    count = write(self->eventPipe[1], buf, 1);
    if (count == 1) {
		return PR_SUCCESS;
	} else if ((count == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
		return PR_SUCCESS;
	} else
		return PR_FAILURE;
}/* --- end _pl_NativeNotify() --- */
#endif /* XP_UNIX */

#if defined(XP_BEOS)
struct ThreadInterfaceData
{
	void	*data;
	int32	sync;
};

static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
	struct ThreadInterfaceData	 id;
	id.data = self;
	id.sync = false;
	write_port(self->eventport, 'natv', &id, sizeof(id));

    return PR_SUCCESS;    /* Is this correct? */
}
#endif /* XP_BEOS */

#if defined(XP_MAC)
static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
#pragma unused (self)
    return PR_SUCCESS;    /* XXX can fail? */
}
#endif /* XP_MAC */

static PRStatus
_pl_AcknowledgeNativeNotify(PLEventQueue* self)
{
#if defined(_WIN32) || defined(WIN16) || defined(XP_OS2)
#ifdef XP_OS2
    QMSG aMsg;
#else
    MSG aMsg;
#endif
    /*
     * only remove msg when we've been called directly by
     * PL_ProcessPendingEvents, not when we've been called by
     * the window proc because the window proc will remove the
     * msg for us.
     */
    if (self->removeMsg) {
        PR_LOG(event_lm, PR_LOG_DEBUG,
                ("_pl_AcknowledgeNativeNotify: self=%p", self));
#ifdef XP_OS2
        WinPeekMsg((HAB)0, &aMsg, self->eventReceiverWindow,
                   _pr_PostEventMsgId, _pr_PostEventMsgId, PM_REMOVE);
#else
        PeekMessage(&aMsg, self->eventReceiverWindow,
                _pr_PostEventMsgId, _pr_PostEventMsgId, PM_REMOVE);
#endif
    }
    return PR_SUCCESS;
#elif defined(VMS)
    PR_LOG(event_lm, PR_LOG_DEBUG,
            ("_pl_AcknowledgeNativeNotify: self=%p efn=%d",
             self, self->efn));
    /*
    ** If this is the last entry, then clear the event flag. Also make sure
    ** the flag is cleared on any spurious wakeups.
    */
    sys$clref(self->efn);
    return PR_SUCCESS;
#elif defined(XP_UNIX)

    PRInt32 count;
    unsigned char c;
    PR_LOG(event_lm, PR_LOG_DEBUG,
            ("_pl_AcknowledgeNativeNotify: self=%p",
             self));
	/* consume the byte NativeNotify put in our pipe: */
    count = read(self->eventPipe[0], &c, 1);
	if ((count == 1) && (c == NOTIFY_TOKEN)) {
		return PR_SUCCESS;
	} else if ((count == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
		return PR_SUCCESS;
	} else
		return PR_FAILURE;
#else

#if defined(XP_MAC)
#pragma unused (self)
#endif

    /* nothing to do on the other platforms */
    return PR_SUCCESS;
#endif
}

PR_IMPLEMENT(PRInt32)
PL_GetEventQueueSelectFD(PLEventQueue* self)
{
    if (self == NULL)
    return -1;

#if defined(VMS)
    return -(self->efn);
#elif defined(XP_UNIX)
    return self->eventPipe[0];
#else
    return -1;    /* other platforms don't handle this (yet) */
#endif
}

PR_IMPLEMENT(PRBool)
PL_IsQueueOnCurrentThread( PLEventQueue *queue )
{
    PRThread *me = PR_GetCurrentThread();
    if ( me == queue->handlerThread )
        return PR_TRUE;
    else
        return PR_FALSE;
} /* end PL_IsQueueOnCurrentThread() */

PR_EXTERN(PRBool)
PL_IsQueueNative(PLEventQueue *queue)
{
    return queue->type == EventQueueIsNative ? PR_TRUE : PR_FALSE;
}

#if defined(WIN16) || defined(_WIN32)
/*
** Global Instance handle...
** In Win32 this is the module handle of the DLL.
**
*/
static HINSTANCE _pr_hInstance;
#endif

#if defined(WIN16)
/*
** Function LibMain() is required by Win16
**
*/
int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg, 
                      WORD cbHeapSize, LPSTR lpszCmdLine )
{
    _pr_hInstance = hInst;
    return TRUE;
}
#endif /* WIN16 */

#if defined(_WIN32)

/*
** Initialization routine for the DLL...
*/

BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
  switch (dwReason)
  {
    case DLL_PROCESS_ATTACH:
      _pr_hInstance = hDLL;
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:
      _pr_hInstance = NULL;
      break;
  }

    return TRUE;
}
#endif


#if defined(WIN16) || defined(_WIN32) || defined(XP_OS2)
#ifdef XP_OS2
MRESULT EXPENTRY
_md_EventReceiverProc(HWND hwnd, ULONG uMsg, MPARAM wParam, MPARAM lParam)
#else
LRESULT CALLBACK 
#if defined(WIN16)
__loadds
#endif
_md_EventReceiverProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
#endif
{
    if (_pr_PostEventMsgId == uMsg )
    {
        PREventQueue *queue = (PREventQueue *)lParam;
        queue->removeMsg = PR_FALSE;
        PL_ProcessPendingEvents(queue);
        queue->removeMsg = PR_TRUE;
#ifdef XP_OS2
        return MRFROMLONG(TRUE);
#else
        return TRUE;
#endif
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


static PRBool   isInitialized;
static PRCallOnceType once;
static PRLock   *initLock;

/*
** InitWinEventLib() -- Create the Windows initialization lock
**
*/
static PRStatus InitEventLib( void )
{
    PR_ASSERT( initLock == NULL );
    
    initLock = PR_NewLock();
    if ( NULL == initLock )
        return PR_FAILURE;
    else
        return PR_SUCCESS;
} /* end InitWinEventLib() */

#endif /* Win16, Win32, OS2 */

#if defined(_WIN32) || defined(WIN16)
/*
** _md_CreateEventQueue() -- ModelDependent initializer
*/
static void _md_CreateEventQueue( PLEventQueue *eventQueue )
{
    WNDCLASS wc;
    
    /*
    ** If this is the first call to PL_InitializeEventsLib(),
    ** make the call to InitWinEventLib() to create the initLock.
    **
    ** Then lock the initializer lock to insure that
    ** we have exclusive control over the initialization sequence.
    **
    */


    /* Register the windows message for XPCOM Event notification */
    _pr_PostEventMsgId = RegisterWindowMessage("XPCOM_PostEvent");

    /* Register the class for the event receiver window */
    if (!GetClassInfo(_pr_hInstance, _pr_eventWindowClass, &wc)) {
        wc.style         = 0;
        wc.lpfnWndProc   = _md_EventReceiverProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = _pr_hInstance;
        wc.hIcon         = NULL;
        wc.hCursor       = NULL;
        wc.hbrBackground = (HBRUSH) NULL;
        wc.lpszMenuName  = (LPCSTR) NULL;
        wc.lpszClassName = _pr_eventWindowClass;
        RegisterClass(&wc);
        }
        
    /* Create the event receiver window */
    eventQueue->eventReceiverWindow = CreateWindow(_pr_eventWindowClass,
                                        "XPCOM:EventReceiver",
                                            0, 0, 0, 10, 10,
                                            NULL, NULL, _pr_hInstance,
                                            NULL);
    PR_ASSERT(eventQueue->eventReceiverWindow);

    return;    
} /* end _md_CreateEventQueue() */
#endif /* Winxx */

#if defined(XP_OS2)
/*
** _md_CreateEventQueue() -- ModelDependent initializer
*/
static void _md_CreateEventQueue( PLEventQueue *eventQueue )
{
    /* Must have HMQ for this & can't assume we already have appshell */
    if( FALSE == WinQueryQueueInfo( HMQ_CURRENT, NULL, 0))
    {
       HAB hab = WinInitialize( 0);
       WinCreateMsgQueue( hab, 0);
    }

    if( !_pr_PostEventMsgId)
    {
       WinRegisterClass( 0 /* hab_current */,
                         _pr_eventWindowClass,
                         _md_EventReceiverProc,
                         0, 0);

       _pr_PostEventMsgId = WinAddAtom( WinQuerySystemAtomTable(),
                                        "XPCOM_PostEvent");
    }

    eventQueue->eventReceiverWindow = WinCreateWindow( HWND_DESKTOP,
                                                       _pr_eventWindowClass,
                                                       "", 0,
                                                       0, 0, 0, 0,
                                                       HWND_DESKTOP,
                                                       HWND_TOP,
                                                       0,
                                                       NULL,
                                                       NULL);
    PR_ASSERT(eventQueue->eventReceiverWindow);

    return;    
} /* end _md_CreateEventQueue() */
#endif /* XP_OS2 */

#if defined(XP_UNIX) || defined(XP_MAC) || defined(XP_BEOS)
/*
** _md_CreateEventQueue() -- ModelDependent initializer
*/
static void _md_CreateEventQueue( PLEventQueue *eventQueue )
{
#ifdef XP_MAC 
#pragma unused(eventQueue) 
#endif 
    /* there's really nothing special to do here,
    ** the guts of the unix stuff is in the setupnativenotify
    ** and related functions.
    */
    return;    
} /* end _md_CreateEventQueue() */
#endif /* XP_UNIX */

/* extra functions for unix */

#ifdef XP_UNIX

PR_IMPLEMENT(PRInt32)
PL_ProcessEventsBeforeID(PLEventQueue *aSelf, unsigned long aID)
{
  PRInt32 count = 0;
  PRInt32 fullCount;

  if (aSelf == NULL)
    return -1;
  
  PR_EnterMonitor(aSelf->monitor);

  if (aSelf->processingEvents) {
    PR_ExitMonitor(aSelf->monitor);
    return 0;
  }

  aSelf->processingEvents = PR_TRUE;

  /* Only process the events that are already in the queue, and
   * not any new events that get added. Do this by counting the
   * number of events currently in the queue
   */
  fullCount = _pl_GetEventCount(aSelf);
  PR_LOG(event_lm, PR_LOG_DEBUG, 
         ("$$$ fullCount is %d id is %ld\n", fullCount, aID));

  if (fullCount == 0) {
    aSelf->processingEvents = PR_FALSE;
    PR_ExitMonitor(aSelf->monitor);
    return 0;
  }

  PR_ExitMonitor(aSelf->monitor);

  while(fullCount-- > 0) {
    /* peek at the next event */
    PLEvent *event;
    event = PR_EVENT_PTR(aSelf->queue.next);
    if (event == NULL)
      break;
    PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ processing event %ld\n",
                                    event->id));
    if (event->id < aID) {
      event = PL_GetEvent(aSelf);
      PL_HandleEvent(event);
      PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
      count++;
    }
    else {
      PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ skipping event and breaking"));
      break;
    }
  }

  PR_EnterMonitor(aSelf->monitor);

  /* if full count still had items left then there's still items left
     in the queue.  Let the native notify token stay. */

  if (aSelf->type == EventQueueIsNative)
  {
    fullCount = _pl_GetEventCount(aSelf);

    if (fullCount <= 0)
    {
      _pl_AcknowledgeNativeNotify(aSelf);
      aSelf->notified = PR_FALSE;
    }
  }

  aSelf->processingEvents = PR_FALSE;
  
  PR_ExitMonitor(aSelf->monitor);

  return count;
}

PR_IMPLEMENT(void)
PL_RegisterEventIDFunc(PLEventQueue *aSelf, PLGetEventIDFunc aFunc,
                       void *aClosure)
{
  aSelf->idFunc = aFunc;
  aSelf->idFuncClosure = aClosure;
}

PR_IMPLEMENT(void)
PL_UnregisterEventIDFunc(PLEventQueue *aSelf)
{
  aSelf->idFunc = 0;
  aSelf->idFuncClosure = 0;
}

#endif /* XP_UNIX */

/* --- end plevent.c --- */
