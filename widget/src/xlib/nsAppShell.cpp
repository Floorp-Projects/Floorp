/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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

#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "nsWidget.h"
#include "nsAppShell.h"
#include "nsKeyCode.h"

#include "nsIWidget.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsITimer.h"

#ifndef MOZ_MONOLITHIC_TOOLKIT
#include "nsIXlibWindowService.h"
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

#include "xlibrgb.h"

#define CHAR_BUF_SIZE 40

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

#ifndef MOZ_MONOLITHIC_TOOLKIT
static NS_DEFINE_IID(kWindowServiceCID,NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,NS_XLIB_WINDOW_SERVICE_IID);
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

#ifndef !MOZ_MONOLITHIC_TOOLKIT
// // this is so that we can get the timers in the base.  most widget
// // toolkits do this through some set of globals.  not here though.  we
// // don't have that luxury
extern "C" int NS_TimeToNextTimeout(struct timeval *);
extern "C" void NS_ProcessTimeouts(void);
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

PRBool nsAppShell::DieAppShellDie = PR_FALSE;

// For debugging.
static char *event_names[] = {
  "",
  "",
  "KeyPress",
  "KeyRelease",
  "ButtonPress",
  "ButtonRelease",
  "MotionNotify",
  "EnterNotify",
  "LeaveNotify",
  "FocusIn",
  "FocusOut",
  "KeymapNotify",
  "Expose",
  "GraphicsExpose",
  "NoExpose",
  "VisibilityNotify",
  "CreateNotify",
  "DestroyNotify",
  "UnmapNotify",
  "MapNotify",
  "MapRequest",
  "ReparentNotify",
  "ConfigureNotify",
  "ConfigureRequest",
  "GravityNotify",
  "ResizeRequest",
  "CirculateNotify",
  "CirculateRequest",
  "PropertyNotify",
  "SelectionClear",
  "SelectionRequest",
  "SelectionNotify",
  "ColormapNotify",
  "ClientMessage",
  "MappingNotify"
};

#ifndef MOZ_MONOLITHIC_TOOLKIT
static nsXlibTimeToNextTimeoutFunc GetTimeToNextTimeoutFunc(void)
{
  static nsXlibTimeToNextTimeoutFunc sFunc = nsnull;

  if (!sFunc)
  {
    nsIXlibWindowService * xlibWindowService = nsnull;
    
    nsresult rv = nsServiceManager::GetService(kWindowServiceCID,
                                               kWindowServiceIID,
                                               (nsISupports **)&xlibWindowService);
    
    NS_ASSERTION(NS_SUCCEEDED(rv),"Couldn't obtain window service.");
    
    if (NS_OK == rv && nsnull != xlibWindowService)
    {
      xlibWindowService->GetTimeToNextTimeoutFunc(&sFunc);
    
      NS_ASSERTION(nsnull != sFunc,"Time to next timeout func is null.");

      static int once = 1;

      if (once && sFunc)
      {
        once = 0;

        printf("YES! Time to next timeout func is good.\n");
      }
      
      NS_RELEASE(xlibWindowService);
    }
  }

  return sFunc;
}

static nsXlibProcessTimeoutsProc GetProcessTimeoutsProc(void)
{
  static nsXlibProcessTimeoutsProc sProc = nsnull;

  if (!sProc)
  {
    nsIXlibWindowService * xlibWindowService = nsnull;
    
    nsresult rv = nsServiceManager::GetService(kWindowServiceCID,
                                               kWindowServiceIID,
                                               (nsISupports **)&xlibWindowService);
    
    NS_ASSERTION(NS_SUCCEEDED(rv),"Couldn't obtain window service.");
    
    if (NS_OK == rv && nsnull != xlibWindowService)
    {
      xlibWindowService->GetProcessTimeoutsProc(&sProc);
    
      NS_ASSERTION(nsnull != sProc,"process timeout proc is null.");

      static int once = 1;

      if (once && sProc)
      {
        once = 0;

        printf("YES! Process timeout proc is good.\n");
      }

      NS_RELEASE(xlibWindowService);
    }
  }

  return sProc;
}
#endif

static int CallTimeToNextTimeoutFunc(struct timeval * aTimeval)
{
#ifdef MOZ_MONOLITHIC_TOOLKIT
  return NS_TimeToNextTimeout(aTimeval);
#else
  nsXlibTimeToNextTimeoutFunc func = GetTimeToNextTimeoutFunc();

  if (func)
  {
    return (*func)(aTimeval);
  }

  return 0;
#endif /* MOZ_MONOLITHIC_TOOLKIT */
}

static void CallProcessTimeoutsProc(void)
{
#ifdef MOZ_MONOLITHIC_TOOLKIT
  NS_ProcessTimeouts();
#else
  nsXlibProcessTimeoutsProc proc = GetProcessTimeoutsProc();

  if (proc)
  {
    (*proc)();
  }
#endif /* MOZ_MONOLITHIC_TOOLKIT */
}

#define ALL_EVENTS ( KeyPressMask | KeyReleaseMask | ButtonPressMask | \
                     ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | \
                     PointerMotionMask | PointerMotionHintMask | Button1MotionMask | \
                     Button2MotionMask | Button3MotionMask | \
                     Button4MotionMask | Button5MotionMask | ButtonMotionMask | \
                     KeymapStateMask | ExposureMask | VisibilityChangeMask | \
                     StructureNotifyMask | ResizeRedirectMask | \
                     SubstructureNotifyMask | SubstructureRedirectMask | \
                     FocusChangeMask | PropertyChangeMask | \
                     ColormapChangeMask | OwnerGrabButtonMask )

NS_IMPL_ADDREF(nsAppShell)
NS_IMPL_RELEASE(nsAppShell)

nsAppShell::nsAppShell()  
{ 
  NS_INIT_REFCNT();
  mDispatchListener = 0;

  mDisplay = nsnull;
  mScreen = nsnull;
}

nsresult nsAppShell::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = NS_NOINTERFACE;

  static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kIAppShellIID)) {
    *aInstancePtr = (void*) ((nsIAppShell*)this);
    NS_ADDREF_THIS();
    result = NS_OK;
  }
  return result;
}


NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  // Open the display
  mDisplay = XOpenDisplay(NULL);

  if (mDisplay == NULL) 
  {
    fprintf(stderr, "%s: Cannot connect to X server %s\n",
            argv[0], 
            XDisplayName(NULL));
    
    exit(1);
  }

  _Xdebug = 1;

  mScreen = DefaultScreenOfDisplay(mDisplay);

  xlib_rgb_init(mDisplay, mScreen);

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsAppShell::Create(dpy=%p  screen=%p)\n",
         mDisplay,
         mScreen));

  return NS_OK;
}

NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

nsresult nsAppShell::Run()
{
  NS_ADDREF_THIS();
  nsresult rv = NS_OK;
  nsIEventQueue *EQueue = nsnull;
  int xlib_fd = -1;
  int queue_fd = -1;
  int max_fd;
  fd_set select_set;
  int select_retval;

  // get the event queue service
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **) &mEventQueueService);
  if (NS_OK != rv) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }
  rv = mEventQueueService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
  // If a queue already present use it.
  if (EQueue)
     goto done;

  // Create the event queue for the thread
  rv = mEventQueueService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }
  //Get the event queue for the thread
  rv = mEventQueueService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
  if (NS_OK != rv) {
      NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
      return rv;
  }    
 done:
  printf("Getting the xlib connection number.\n");
  xlib_fd = ConnectionNumber(mDisplay);
  queue_fd = EQueue->GetEventQueueSelectFD();
  if (xlib_fd >= queue_fd) {
    max_fd = xlib_fd + 1;
  } else {
    max_fd = queue_fd + 1;
  }
  // process events.
  while (DieAppShellDie == PR_FALSE) {
    XEvent          event;
    struct timeval  cur_time;
    struct timeval *cur_time_ptr;
    int             please_run_timer_queue = 0;
    gettimeofday(&cur_time, NULL);
    FD_ZERO(&select_set);
    // add the queue and the xlib connection to the select set
    FD_SET(queue_fd, &select_set);
    FD_SET(xlib_fd, &select_set);

    if (CallTimeToNextTimeoutFunc(&cur_time) == 0) {
      cur_time_ptr = NULL;
    }
    else {
      cur_time_ptr = &cur_time;
      // check to see if something is waiting right now
      if ((cur_time.tv_sec == 0) && (cur_time.tv_usec == 0)) {
        please_run_timer_queue = 1;
      }
    }
    // note that we are passing in the timeout_ptr from above.
    // if there are no timers, this will be null and this will
    // block until hell freezes over
    select_retval = select(max_fd, &select_set, NULL, NULL, cur_time_ptr);
    if (select_retval == -1) {
      printf("Select returned error.\n");
      return NS_ERROR_FAILURE;
    }
    if (select_retval == 0) {
      // the select timed out, process the timeout queue.
      //      printf("Timer ran out...\n");
      please_run_timer_queue = 1;
    }
    // check to see if there's data avilable for the queue
    if (FD_ISSET(queue_fd, &select_set)) {
      //printf("queue data available.\n");
      EQueue->ProcessPendingEvents();
    }
    // check to see if there's data avilable for
    // xlib
    if (FD_ISSET(xlib_fd, &select_set)) {
      while (XPending(mDisplay)) {
        XNextEvent(mDisplay, &event);
        DispatchXEvent(&event);
      }
    }
    if (please_run_timer_queue) {
      //printf("Running timer queue...\n");
      CallProcessTimeoutsProc();
    }
    // make sure that any pending X requests are flushed.
    XFlush(mDisplay);
  }

	NS_IF_RELEASE(EQueue);
  NS_IF_RELEASE(mEventQueueService);
  Release();
  return rv;
}

//-------------------------------------------------------------------------
//
// PushThreadEventQueue - begin processing events from a new queue
//   note this is the Windows implementation and may suffice, but
//   this is untested on xlib.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PushThreadEventQueue()
{
  nsresult rv;

  // push a nested event queue for event processing from netlib
  // onto our UI thread queue stack.
  NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = eQueueService->PushThreadEventQueue();
  else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

//-------------------------------------------------------------------------
//
// PopThreadEventQueue - stop processing on a previously pushed event queue
//   note this is the Windows implementation and may suffice, but
//   this is untested on xlib.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PopThreadEventQueue()
{
  nsresult rv;

  NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = eQueueService->PopThreadEventQueue();
  else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

NS_METHOD
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  return NS_OK;
}

NS_METHOD
nsAppShell::EventIsForModalWindow(PRBool aRealEvent, void *aEvent,
                            nsIWidget *aWidget, PRBool *aForWindow)
{
  return NS_OK;
}

nsresult nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  if (aEvent == 0) {
    return NS_ERROR_FAILURE;
  }
  XEvent *event = (XEvent *)aEvent;
  DispatchXEvent(event);
  return NS_OK;
}

NS_METHOD nsAppShell::Exit()
{
  DieAppShellDie = PR_TRUE;
  return NS_OK;
}

nsAppShell::~nsAppShell()
{
}

void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  return nsnull;
}

void
nsAppShell::DispatchXEvent(XEvent *event)
{
  nsWidget *widget;
  widget = nsWidget::GetWidgetForWindow(event->xany.window);

  // switch on the type of event
  switch (event->type) 
  {
  case Expose:
	HandleExposeEvent(event, widget);
    break;

  case ConfigureNotify:
    // we need to make sure that this is the LAST of the
    // config events.
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("DispatchEvent: ConfigureNotify event for window 0x%lx %d %d %d %d\n",
                                         event->xconfigure.window,
                                         event->xconfigure.x, 
                                         event->xconfigure.y,
                                         event->xconfigure.width, 
                                         event->xconfigure.height));

    HandleConfigureNotifyEvent(event, widget);

    break;

  case ButtonPress:
  case ButtonRelease:
    HandleFocusInEvent(event, widget);
    HandleButtonEvent(event, widget);
    break;

  case MotionNotify:
    HandleMotionNotifyEvent(event, widget);
    break;

  case KeyPress:
    HandleKeyPressEvent(event, widget);
    break;
  case KeyRelease:
    HandleKeyReleaseEvent(event, widget);
    break;

  case FocusIn:
    HandleFocusInEvent(event, widget);
    break;

  case FocusOut:
    HandleFocusOutEvent(event, widget);
    break;

  case EnterNotify:
    HandleEnterEvent(event, widget);
    break;

  case LeaveNotify:
    HandleLeaveEvent(event, widget);
    break;

  case NoExpose:
    // these annoy me.
    break;
  case VisibilityNotify:
    HandleVisibilityNotifyEvent(event, widget);
    break;
  case ClientMessage:
    HandleClientMessageEvent(event, widget);
    break;
  default:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Unhandled window event: Window 0x%lx Got a %s event\n",
                                         event->xany.window, event_names[event->type]));

    break;
  }
}

void
nsAppShell::HandleMotionNotifyEvent(XEvent *event, nsWidget *aWidget)
{
  nsMouseEvent mevent;
  mevent.message = NS_MOUSE_MOVE;
  mevent.widget = aWidget;
  mevent.eventStructType = NS_MOUSE_EVENT;
  mevent.point.x = event->xmotion.x;
  mevent.point.y = event->xmotion.y;
  mevent.time = 0;
  NS_ADDREF(aWidget);
  aWidget->DispatchMouseEvent(mevent);
  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleButtonEvent(XEvent *event, nsWidget *aWidget)
{
  nsMouseEvent mevent;
  mevent.isShift = 0;
  mevent.isControl = 0;
  PRUint32 eventType = 0;

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Button event for window 0x%lx button %d type %s\n",
         event->xany.window, event->xbutton.button, (event->type == ButtonPress ? "ButtonPress" : "ButtonRelease")));
  switch(event->type) {
  case ButtonPress:
    switch(event->xbutton.button) {
    case 1:
      eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
      break;
    case 2:
      eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
      break;
    case 3:
      eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
      break;
    }
    break;
  case ButtonRelease:
    switch(event->xbutton.button) {
    case 1:
      eventType = NS_MOUSE_LEFT_BUTTON_UP;
      break;
    case 2:
      eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
      break;
    case 3:
      eventType = NS_MOUSE_RIGHT_BUTTON_UP;
      break;
    }
    break;
  }

  mevent.message = eventType;
  mevent.widget = aWidget;
  mevent.eventStructType = NS_MOUSE_EVENT;
  mevent.point.x = event->xbutton.x;
  mevent.point.y = event->xbutton.y;
  // XXX fix this.  We need clicks
  mevent.clickCount = 1;
  mevent.time = 0;
  NS_ADDREF(aWidget);
  aWidget->DispatchMouseEvent(mevent);
  NS_ADDREF(aWidget);
}

void
nsAppShell::HandleExposeEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Expose event for window 0x%lx %d %d %d %d\n", event->xany.window,
                                       event->xexpose.x, event->xexpose.y, event->xexpose.width, event->xexpose.height));

  nsRect *dirtyRect = new nsRect(event->xexpose.x, event->xexpose.y, 
                                 event->xexpose.width, event->xexpose.height);

  /* compress expose events...
   */
  if (event->xexpose.count!=0) {
     XEvent txe;
     do {
        XWindowEvent(event->xany.display, event->xany.window, ExposureMask, (XEvent *)&txe);
        dirtyRect->UnionRect(*dirtyRect, nsRect(txe.xexpose.x, txe.xexpose.y, 
                                                txe.xexpose.width, txe.xexpose.height));
     } while (txe.xexpose.count>0);
  }

  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.widget = aWidget;
  pevent.eventStructType = NS_PAINT_EVENT;
  pevent.rect = dirtyRect;

  // XXX fix this
  pevent.time = 0;
  NS_ADDREF(aWidget);
  aWidget->OnPaint(pevent);
  NS_RELEASE(aWidget);
  delete pevent.rect;
}

void
nsAppShell::HandleConfigureNotifyEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("ConfigureNotify event for window 0x%lx %d %d %d %d\n",
                                       event->xconfigure.window,
                                       event->xconfigure.x, event->xconfigure.y,
                                       event->xconfigure.width, event->xconfigure.height));
  XEvent    config_event;
  while (XCheckWindowEvent(event->xany.display, 
                           event->xany.window, 
                           StructureNotifyMask, 
                           &config_event) == True) {
    // make sure that we don't get other types of events.  
    // StructureNotifyMask includes other kinds of events, too.
    if (config_event.type == ConfigureNotify) 
      {
        *event = config_event;
        
        PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("DispatchEvent: Extra ConfigureNotify event for window 0x%lx %d %d %d %d\n",
                                             event->xconfigure.window,
                                             event->xconfigure.x, 
                                             event->xconfigure.y,
                                             event->xconfigure.width, 
                                             event->xconfigure.height));
      }
  }

  nsSizeEvent sevent;
  sevent.message = NS_SIZE;
  sevent.widget = aWidget;
  sevent.eventStructType = NS_SIZE_EVENT;
  sevent.windowSize = new nsRect (event->xconfigure.x, event->xconfigure.y,
                                  event->xconfigure.width, event->xconfigure.height);
  sevent.point.x = event->xconfigure.x;
  sevent.point.y = event->xconfigure.y;
  sevent.mWinWidth = event->xconfigure.width;
  sevent.mWinHeight = event->xconfigure.height;
  // XXX fix this
  sevent.time = 0;
  NS_ADDREF(aWidget);
  aWidget->OnResize(sevent);
  NS_RELEASE(aWidget);
  delete sevent.windowSize;
}

void
nsAppShell::HandleKeyPressEvent(XEvent *event, nsWidget *aWidget)
{
  char string_buf[CHAR_BUF_SIZE];
  int  len = 0;

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("KeyPress event for window 0x%lx\n",
                                       event->xkey.window));

  // Dont dispatch events for modifier keys pressed ALONE
  if (nsKeyCode::KeyCodeIsModifier(event->xkey.keycode))
  {
    return;
  }

  nsKeyEvent keyEvent;

  KeySym     keysym = nsKeyCode::ConvertKeyCodeToKeySym(event->xkey.display,
                                                        event->xkey.keycode);
  XComposeStatus compose;

  len = XLookupString(&event->xkey, string_buf, CHAR_BUF_SIZE, &keysym, &compose);
  string_buf[len] = '\0';

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = 0;
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = event->xkey.state & ControlMask;
  keyEvent.isAlt = event->xkey.state & Mod1Mask;
  keyEvent.point.x = 0;
  keyEvent.point.y = 0;
  keyEvent.message = NS_KEY_DOWN;
  keyEvent.widget = aWidget;
  keyEvent.eventStructType = NS_KEY_EVENT;

  //  printf("keysym = %x, keycode = %x, vk = %x\n",
  //         keysym,
  //         event->xkey.keycode,
  //         keyEvent.keyCode);

  aWidget->DispatchKeyEvent(keyEvent);

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = string_buf[0];
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = event->xkey.state & ControlMask;
  keyEvent.isAlt = event->xkey.state & Mod1Mask;
  keyEvent.point.x = 0;
  keyEvent.point.y = 0;
  keyEvent.message = NS_KEY_PRESS;
  keyEvent.widget = aWidget;
  keyEvent.eventStructType = NS_KEY_EVENT;

  aWidget->DispatchKeyEvent(keyEvent);

}

void
nsAppShell::HandleKeyReleaseEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("KeyRelease event for window 0x%lx\n",
                                       event->xkey.window));

  // Dont dispatch events for modifier keys pressed ALONE
  if (nsKeyCode::KeyCodeIsModifier(event->xkey.keycode))
  {
    return;
  }

  nsKeyEvent keyEvent;
  KeySym     keysym = nsKeyCode::ConvertKeyCodeToKeySym(event->xkey.display,
                                                        event->xkey.keycode);

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = 0;
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = event->xkey.state & ControlMask;
  keyEvent.isAlt = event->xkey.state & Mod1Mask;
  keyEvent.point.x = event->xkey.x;
  keyEvent.point.y = event->xkey.y;
  keyEvent.message = NS_KEY_UP;
  keyEvent.widget = aWidget;
  keyEvent.eventStructType = NS_KEY_EVENT;

  NS_ADDREF(aWidget);

  aWidget->DispatchKeyEvent(keyEvent);

  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleFocusInEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("FocusIn event for window 0x%lx\n",
                                       event->xfocus.window));
  nsGUIEvent focusEvent;
  
  focusEvent.message = NS_GOTFOCUS;
  focusEvent.widget  = aWidget;
  
  focusEvent.eventStructType = NS_GUI_EVENT;
  
  focusEvent.time = 0;
  focusEvent.point.x = 0;
  focusEvent.point.y = 0;
  
  NS_ADDREF(aWidget);
  aWidget->DispatchWindowEvent(focusEvent);
  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleFocusOutEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("FocusOut event for window 0x%lx\n",
                                       event->xfocus.window));
  nsGUIEvent focusEvent;
  
  focusEvent.message = NS_LOSTFOCUS;
  focusEvent.widget  = aWidget;
  
  focusEvent.eventStructType = NS_GUI_EVENT;
  
  focusEvent.time = 0;
  focusEvent.point.x = 0;
  focusEvent.point.y = 0;
  
  NS_ADDREF(aWidget);
  aWidget->DispatchWindowEvent(focusEvent);
  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleEnterEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Enter event for window 0x%lx\n",
                                       event->xcrossing.window));
  nsMouseEvent enterEvent;

  enterEvent.message = NS_MOUSE_ENTER;
  enterEvent.widget  = aWidget;
  
  enterEvent.eventStructType = NS_MOUSE_EVENT;
  
  enterEvent.time = event->xcrossing.time;
  enterEvent.point.x = nscoord(event->xcrossing.x);
  enterEvent.point.y = nscoord(event->xcrossing.y);
  
  NS_ADDREF(aWidget);

  aWidget->DispatchWindowEvent(enterEvent);

  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleLeaveEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Leave event for window 0x%lx\n",
                                       event->xcrossing.window));

  nsMouseEvent leaveEvent;
  
  leaveEvent.message = NS_MOUSE_EXIT;
  leaveEvent.widget  = aWidget;
  
  leaveEvent.eventStructType = NS_MOUSE_EVENT;
  
  leaveEvent.time = event->xcrossing.time;
  leaveEvent.point.x = nscoord(event->xcrossing.x);
  leaveEvent.point.y = nscoord(event->xcrossing.y);
  
  NS_ADDREF(aWidget);

  aWidget->DispatchWindowEvent(leaveEvent);

  NS_RELEASE(aWidget);
}

void nsAppShell::HandleVisibilityNotifyEvent(XEvent *event, nsWidget *aWidget)
{
#ifdef DEBUG
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("VisibilityNotify event for window 0x%lx ",
                                       event->xfocus.window));
  switch(event->xvisibility.state) {
  case VisibilityFullyObscured:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Fully Obscured\n"));
    break;
  case VisibilityPartiallyObscured:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Partially Obscured\n"));
    break;
  case VisibilityUnobscured:
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Unobscured\n"));
  }
#endif
  aWidget->SetVisibility(event->xvisibility.state);
}

void nsAppShell::HandleMapNotifyEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("MapNotify event for window 0x%lx\n",
                                       event->xmap.window));
  aWidget->SetMapStatus(PR_TRUE);
}

void nsAppShell::HandleUnmapNotifyEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("UnmapNotifyEvent for window 0x%lx\n",
                                       event->xunmap.window));
  aWidget->SetMapStatus(PR_FALSE);
}

void nsAppShell::HandleClientMessageEvent(XEvent *event, nsWidget *aWidget)
{
  // check to see if it's a WM_DELETE message
  printf("handling client message\n");
  if (nsWidget::WMProtocolsInitialized) {
    if ((Atom)event->xclient.data.l[0] == nsWidget::WMDeleteWindow) {
      printf("got a delete window event\n");
      aWidget->OnDeleteWindow();
    }
  }
}
