/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include "plevent.h"
#include "prmem.h"
#include "prcmon.h"
#include "prlog.h"
#if !defined(WIN32)
#include <errno.h>
#include <stddef.h>
#if !defined(OS2)
#include <unistd.h>
#endif
#endif

#if defined(XP_MAC)
#include <AppleEvents.h>
#include "pprthred.h"
#include "primpl.h"
#else
#include "private/pprthred.h"
#include "private/primpl.h"
#endif

static PRLogModuleInfo *event_lm = NULL;

/*******************************************************************************
 * Private Stuff
 ******************************************************************************/

struct PLEventQueue {
    char*        name;
    PRCList        queue;
    PRMonitor*        monitor;
    PRThread*        handlerThread;
#ifdef XP_UNIX
    PRInt32        eventPipe[2];
    PRPackedBool    nativeNotifier;
	int notifyCount;
#endif
};

#define PR_EVENT_PTR(_qp) \
    ((PLEvent*) ((char*) (_qp) - offsetof(PLEvent, link)))

static PRStatus    _pl_SetupNativeNotifier(PLEventQueue* self);
static void    _pl_CleanupNativeNotifier(PLEventQueue* self);
static PRStatus    _pl_NativeNotify(PLEventQueue* self);
static PRStatus    _pl_AcknowledgeNativeNotify(PLEventQueue* self);


#if defined(_WIN32) || defined(WIN16)
PLEventQueue * _pr_main_event_queue;
UINT _pr_PostEventMsgId;
HWND _pr_eventReceiverWindow;
static char *_pr_eventWindowClass = "NSPR:EventWindow";
#endif

/*******************************************************************************
 * Event Queue Operations
 ******************************************************************************/

PR_IMPLEMENT(PLEventQueue*)
PL_CreateEventQueue(char* name, PRThread* handlerThread)
{
    PRStatus err;
    PLEventQueue* self = NULL;
    PRMonitor* mon = NULL;

    if (event_lm == NULL)
        event_lm = PR_LOG_DEFINE("event");

    self = PR_NEWZAP(PLEventQueue);
    if (self == NULL) return NULL;

    mon = PR_NewNamedMonitor(name);
    if (mon == NULL) goto error;

    self->name = name;
    self->monitor = mon;
    self->handlerThread = handlerThread;
    PR_INIT_CLIST(&self->queue);
    err = _pl_SetupNativeNotifier(self);
    if (err) goto error;

    return self;

  error:
    if (mon != NULL)
    PR_DestroyMonitor(mon);
    PR_DELETE(self);
    return NULL;
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

    _pl_CleanupNativeNotifier(self);

    /* destroying the monitor also destroys the name */
    PR_ExitMonitor(self->monitor);
    PR_DestroyMonitor(self->monitor);
    PR_DELETE(self);

}

PR_IMPLEMENT(PRStatus)
PL_PostEvent(PLEventQueue* self, PLEvent* event)
{
    PRStatus err;
    PRMonitor* mon;

    if (self == NULL)
    return PR_FAILURE;

    mon = self->monitor;
    PR_EnterMonitor(mon);

    /* insert event into thread's event queue: */
    if (event != NULL) {
    PR_APPEND_LINK(&event->link, &self->queue);
    }

    /* notify even if event is NULL */
    err = _pl_NativeNotify(self);
    if (err != PR_SUCCESS) goto done;

    /*
     * This may fall on deaf ears if we're really notifying the native 
     * thread, and no one has called PL_WaitForEvent (or PL_EventLoop):
     */
    err = PR_Notify(mon);

  done:
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
    PR_CEnterMonitor(event);

    if (PR_CurrentThread() == self->handlerThread) {
	/* Handle the case where the thread requesting the event handling
	   is also the thread that's supposed to do the handling. */
	result = event->handler(event);
    }
    else {
	int inEventQueueMon = PR_GetMonitorEntryCount(self->monitor);
	int i, entryCount = self->monitor->entryCount;

	event->synchronousResult = (void*)PR_TRUE;
	PR_PostEvent(self, event);
	/* We need to temporarily give up our event queue monitor if
	   we're holding it, otherwise, the thread we're going to wait
	   for notification from won't be able to enter it to process
	   the event. */
	if (inEventQueueMon) {
	    for (i = 0; i < entryCount; i++)
		PR_ExitMonitor(self->monitor);
	}
	PR_CWait(event, PR_INTERVAL_NO_TIMEOUT);	/* wait for event to be handled or destroyed */
	if (inEventQueueMon) {
	    for (i = 0; i < entryCount; i++)
		PR_EnterMonitor(self->monitor);
	}
	result = event->synchronousResult;
	event->synchronousResult = NULL;
    }

    PR_CExitMonitor(event);

    /* For synchronous events, they're destroyed here on the caller's
       thread before the result is returned. See PL_HandleEvent. */
    PL_DestroyEvent(event);

    return result;
}

PR_IMPLEMENT(PLEvent*)
PL_GetEvent(PLEventQueue* self)
{
    PLEvent* event = NULL;
    PRStatus err;
    PRMonitor* mon;

    if (self == NULL)
    return NULL;

    mon = self->monitor;
    PR_EnterMonitor(mon);

    err = _pl_AcknowledgeNativeNotify(self);
    if (err) goto done;

    if (!PR_CLIST_IS_EMPTY(&self->queue)) {
    /* then grab the event and return it: */
    event = PR_EVENT_PTR(self->queue.next);
    PR_REMOVE_AND_INIT_LINK(&event->link);
    }

  done:
    PR_ExitMonitor(mon);
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
        PR_CEnterMonitor(event);
        event->synchronousResult = NULL;
        PR_CNotify(event);
        PR_CExitMonitor(event);
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

    PR_LOG_BEGIN(event_lm, PR_LOG_DEBUG,
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
	    PREvent* event = PR_EVENT_PTR(qp);
	    qp = qp->next;
	    PR_ASSERT(event->owner != owner);
	}
    }
#endif /* DEBUG */

    PR_ExitMonitor(self->monitor);

    PR_LOG_END(event_lm, PR_LOG_DEBUG,
           ("$$$ revoking events for owner %0x", owner));
}

PR_IMPLEMENT(void)
PL_ProcessPendingEvents(PLEventQueue* self)
{
    if (self == NULL)
    return;

    while (PR_TRUE) {
    PLEvent* event = PL_GetEvent(self);
        if (event == NULL) return;

    PR_LOG_BEGIN(event_lm, PR_LOG_DEBUG, ("$$$ processing event"));
    PL_HandleEvent(event);
    PR_LOG_END(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
    }
}

/*******************************************************************************
 * Event Operations
 ******************************************************************************/

PR_IMPLEMENT(void)
PL_InitEvent(PLEvent* self, void* owner,
         PLHandleEventProc handler,
         PLDestroyEventProc destructor)
{
    PR_INIT_CLIST(&self->link);
    self->handler = handler;
    self->destructor = destructor;
    self->owner = owner;
    self->synchronousResult = NULL;
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
    PR_CEnterMonitor(self);
    self->synchronousResult = result;
    PR_CNotify(self);    /* wake up the guy waiting for the result */
    PR_CExitMonitor(self);
    }
    else {
    /* For asynchronous events, they're destroyed by the event-handler
       thread. See PR_PostSynchronousEvent. */
    PL_DestroyEvent(self);
    }
}

PR_IMPLEMENT(void)
PL_DestroyEvent(PLEvent* self)
{
    if (self == NULL)
    return;

    /* This event better not be on an event queue anymore. */
    PR_ASSERT(PR_CLIST_IS_EMPTY(&self->link));

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
/*    PR_ASSERT(queue->handlerThread == PR_CurrentThread());*/

    PR_EnterMonitor(queue->monitor);

    PR_ASSERT(!PR_CLIST_IS_EMPTY(&self->link));
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
        PR_LOG_END(event_lm, PR_LOG_DEBUG, ("$$$ waiting for event"));
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

        PR_LOG_BEGIN(event_lm, PR_LOG_DEBUG, ("$$$ processing event"));
    PL_HandleEvent(event);
    PR_LOG_END(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
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

    PRInt32 err = 0;
#if defined(XP_UNIX)
    err = pipe(self->eventPipe);
#endif
    return err == 0 ? PR_SUCCESS : PR_FAILURE;
}

static void
_pl_CleanupNativeNotifier(PLEventQueue* self)
{
#if defined(XP_MAC)
#pragma unused (self)
#endif

#if defined(XP_UNIX)
    close(self->eventPipe[0]);
    close(self->eventPipe[1]);
#endif
}

static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
#if defined(XP_UNIX)

#   define NOTIFY_TOKEN    0xFA
    PRInt32 count;
    unsigned char buf[] = { NOTIFY_TOKEN };

    count = write(self->eventPipe[1], buf, 1);
	self->notifyCount++;
    return (count == 1) ? PR_SUCCESS : PR_FAILURE;

#elif defined(XP_PC) && ( defined(WINNT) || defined(WIN95) || defined(WIN16))
    /*
    ** Post a message to the NSPR window on the main thread requesting 
    ** it to process the pending events. This is only necessary for the
    ** main event queue, since the main thread is waiting for OS events.
    */
    if (self == _pr_main_event_queue) {
       PostMessage( _pr_eventReceiverWindow, _pr_PostEventMsgId,
                      (WPARAM)0, (LPARAM)self);
    }
    return PR_SUCCESS;

#elif defined(XP_MAC)

#pragma unused (self)
    return PR_SUCCESS;    /* XXX can fail? */

#endif
}

static PRStatus
_pl_AcknowledgeNativeNotify(PLEventQueue* self)
{
#if defined(XP_UNIX)

    PRInt32 count;
    unsigned char c;

	if (self->notifyCount <= 0) return PR_SUCCESS;
    /* consume the byte NativeNotify put in our pipe: */
    count = read(self->eventPipe[0], &c, 1);
	self->notifyCount--;
    return ((count == 1 && c == NOTIFY_TOKEN) || count == 0)
    ? PR_SUCCESS : PR_FAILURE;

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

#if defined(XP_UNIX)
    return self->eventPipe[0];
#else
    return -1;    /* other platforms don't handle this (yet) */
#endif
}



#if defined(WIN16) || defined(_WIN32)
/*
** Global Instance handle...
** In Win32 this is the module handle of the DLL.
**
*/
HINSTANCE _pr_hInstance;
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
** Initialization routine for the NSPR DLL...
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


#if defined(WIN16) || defined(_WIN32)
PR_IMPLEMENT(PLEventQueue *)
PL_GetMainEventQueue()
{
   PR_ASSERT(_pr_main_event_queue);

   return _pr_main_event_queue;
}

LRESULT CALLBACK 
#if defined(WIN16)
__loadds
#endif
_md_EventReceiverProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (_pr_PostEventMsgId == uMsg )
    {
        PREventQueue *queue = (PREventQueue *)lParam;

        PR_ASSERT(queue == PL_GetMainEventQueue());
        if (queue == PL_GetMainEventQueue()) 
        {
            PR_ProcessPendingEvents(queue);
            return TRUE;
        }
    } 
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

PR_IMPLEMENT(void)
PL_InitializeEventsLib(char *name)
{
    WNDCLASS wc;

    _pr_main_event_queue = PL_CreateEventQueue(name, PR_GetCurrentThread());

    /* Register the windows message for NSPR Event notification */
    _pr_PostEventMsgId = RegisterWindowMessage("NSPR_PostEvent");

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
    _pr_eventReceiverWindow = CreateWindow(_pr_eventWindowClass,
                                           "NSPR:EventReceiver",
                                            0, 0, 0, 10, 10,
                                            NULL, NULL, _pr_hInstance,
                                            NULL);
    PR_ASSERT(_pr_eventReceiverWindow);
}
#endif

#if defined(_WIN32) || defined(WIN16)
PR_IMPLEMENT(HWND)
PR_GetEventReceiverWindow()
{
    if(_pr_eventReceiverWindow != NULL)
    {
	return _pr_eventReceiverWindow;
    }

    return NULL;

}
#endif

/******************************************************************************/
