/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *   Caspian Maclean <caspian@igelaus.com.au>
 */

#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include "nsWindow.h"
#include "nsWidget.h"
#include "nsAppShell.h"
#include "nsKeyCode.h"
#include "nsWidgetsCID.h"

#include "nsIWidget.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIDragService.h"
#include "nsIDragSessionXlib.h"
#include "nsITimer.h"

#ifndef MOZ_MONOLITHIC_TOOLKIT
#include "nsIXlibWindowService.h"
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

#include "xlibrgb.h"

#define CHAR_BUF_SIZE 40

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

#ifndef MOZ_MONOLITHIC_TOOLKIT
static NS_DEFINE_IID(kWindowServiceCID,NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,NS_XLIB_WINDOW_SERVICE_IID);
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

#ifndef MOZ_MONOLITHIC_TOOLKIT
// // this is so that we can get the timers in the base.  most widget
// // toolkits do this through some set of globals.  not here though.  we
// // don't have that luxury
extern "C" int NS_TimeToNextTimeout(struct timeval *);
extern "C" void NS_ProcessTimeouts(void);
#endif /* !MOZ_MONOLITHIC_TOOLKIT */

PRBool nsAppShell::DieAppShellDie = PR_FALSE;
PRBool nsAppShell::mClicked = PR_FALSE;
PRTime nsAppShell::mClickTime = 0;
PRInt16 nsAppShell::mClicks = 1;
PRUint16 nsAppShell::mClickedButton = 0;
Display *nsAppShell::mDisplay = nsnull;
PRBool nsAppShell::mDragging = PR_FALSE;
PRBool nsAppShell::mAltDown = PR_FALSE;
PRBool nsAppShell::mShiftDown = PR_FALSE;
PRBool nsAppShell::mCtrlDown = PR_FALSE;
PRBool nsAppShell::mMetaDown = PR_FALSE;


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

#ifdef DEBUG_ramiro
      printf("Time to next timeout func is null - for now.\n");

      static int once = 1;

      if (once && sFunc)
      {
        once = 0;

        printf("YES! Time to next timeout func is good.\n");
      }
#endif
      
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
    
#ifdef DEBUG_ramiro
      printf("Process timeout proc is null - for now.\n");

      static int once = 1;

      if (once && sProc)
      {
        once = 0;

        printf("YES! Process timeout proc is good.\n");
      }
#endif

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

static void CallProcessTimeoutsProc(Display *aDisplay)
{
#ifdef MOZ_MONOLITHIC_TOOLKIT
  NS_ProcessTimeouts();
#else
  nsXlibProcessTimeoutsProc proc = GetProcessTimeoutsProc();

  if (proc)
  {
    (*proc)(aDisplay);
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

  mScreen = nsnull;
  mEventQueue = nsnull;
  xlib_fd = -1;
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
  char *mArgv[1]; 
  int mArgc = 0;
  XtAppContext app_context;

  // Open the display
  if (mDisplay == nsnull) {
    mDisplay = XOpenDisplay(NULL);

    //XSynchronize(mDisplay, True);
    // Requires XSynchronize(mDisplay, True); To stop X buffering. Use this
    // to make debugging easier. KenF

    if (mDisplay == NULL) 
    {
      fprintf(stderr, "%s: Cannot connect to X server %s\n",
              argv[0], 
              XDisplayName(NULL));
    
      exit(1);
    }

    XtToolkitInitialize();
    app_context = XtCreateApplicationContext();
    XtDisplayInitialize(app_context, mDisplay, NULL, "Wrapper", 
                        NULL, 0, &mArgc, mArgv);

  }
  //  _Xdebug = 1;

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

NS_METHOD nsAppShell::Spinup()
{
  nsresult rv;

  NS_ADDREF_THIS();

  // get the event queue service
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **) &mEventQueueService);
  if (NS_OK != rv) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }
  rv = mEventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);  // If a queue already present use it.
  if (mEventQueue)
    return NS_OK;

  // Create the event queue for the thread
  rv = mEventQueueService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }
  //Get the event queue for the thread
  rv = mEventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);  if (NS_OK != rv) {
      NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
      return rv;
  }   
  return NS_OK;
}

nsresult nsAppShell::Run()
{
  nsresult rv = NS_OK;
  int queue_fd = -1;
  fd_set select_set;
  int select_retval;
  int max_fd;
  XtInputMask mask;
  XtAppContext app_context = XtDisplayToApplicationContext(mDisplay);

  if (mEventQueue == nsnull)
    Spinup();

  if (mEventQueue == nsnull) {
    NS_ASSERTION("Cannot initialize the Event Queue", PR_FALSE);
    return NS_ERROR_NOT_INITIALIZED;
  }

  printf("Getting the xlib connection number.\n");
  xlib_fd = ConnectionNumber(mDisplay);
  // process events.
  while (DieAppShellDie == PR_FALSE) {

    // Dont think these should be here, but go with the flow. (KenF)
    XEvent          event;
    struct timeval  cur_time;
    struct timeval *cur_time_ptr;
    int             please_run_timer_queue = 0;
    struct timeval DelayTime ;

    gettimeofday(&cur_time, NULL);
    // set up our fds
    queue_fd = mEventQueue->GetEventQueueSelectFD();
    
    // Get max of fd's
    if (xlib_fd >= queue_fd) {
      max_fd = xlib_fd + 1;
    } else {
      max_fd = queue_fd + 1;
    }

    FD_ZERO(&select_set);
    // add the queue and the xlib connection to the select set
    FD_SET(queue_fd, &select_set);
    FD_SET(xlib_fd, &select_set);

    //printf("cur time: %ld %ld\n", cur_time.tv_sec, cur_time.tv_usec);
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

    // FIXME!!! 
    // Block for events. For moment, use a artificial timer to 
    // only block for a certain amount of time.
    // When properly fixed, this delay should be cur_time_ptr
    // and  we need a way to detect an Xt event.
    DelayTime.tv_sec = 0;
    DelayTime.tv_usec = 100;
    select_retval = select(max_fd, &select_set, NULL, NULL, &DelayTime);
    if (select_retval == -1) {
      fprintf(stderr, "Select returned error: %s.\n", strerror(errno));
      return NS_ERROR_FAILURE;
    }

    if (select_retval == 0) {
      // the select timed out, process the timeout queue.
      please_run_timer_queue = 1;
    }

    // Xt event handler. (used for legacy plugins)
    while ((mask = XtAppPending(app_context)) > 0) {
      if (mask & XtIMXEvent) {
        XtAppNextEvent(app_context, &event);
        XtDispatchEvent(&event);
        DispatchXEvent(&event);
      } else {
        XtAppProcessEvent(app_context, mask);
      }
    }
    
    // check to see if there's data avilable for the queue
    if (FD_ISSET(queue_fd, &select_set)) {
      mEventQueue->ProcessPendingEvents();
    }

    // Xlib event dispatcher.
    if (FD_ISSET(xlib_fd, &select_set)) {
      while (XPending(mDisplay)) {
        XNextEvent(mDisplay, &event);
        DispatchXEvent(&event);
      }
    }

    if (please_run_timer_queue) {
      CallProcessTimeoutsProc(mDisplay);
    }

    // Flush the nsWindow's drawing queue
    nsWindow::UpdateIdle(nsnull);
    XFlush(mDisplay);
  }
  DieAppShellDie = PR_FALSE;

  Spindown();
  return rv;
}

NS_METHOD nsAppShell::Spindown()
{
  NS_IF_RELEASE(mEventQueueService);
  Release();

  if (mEventQueue) {
    mEventQueue->ProcessPendingEvents();
    NS_IF_RELEASE(mEventQueue);
  }

  return NS_OK;
}

NS_METHOD
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  fd_set select_set;
  int select_retval;
  int max_fd;
  struct timeval DelayTime;
  XEvent *event;

  int queue_fd = mEventQueue->GetEventQueueSelectFD();

  if (xlib_fd == -1)
    xlib_fd = ConnectionNumber(mDisplay);

  if (xlib_fd >= queue_fd) {
    max_fd = xlib_fd + 1;
  } else {
    max_fd = queue_fd + 1;
  }

  FD_ZERO(&select_set);
  FD_SET(queue_fd, &select_set);
  FD_SET(xlib_fd, &select_set);

  DelayTime.tv_sec = 0;
  DelayTime.tv_usec = 100;

  select_retval = select(max_fd, &select_set, NULL, NULL, &DelayTime);

  if (select_retval == -1)
    return NS_ERROR_FAILURE;

  if (XPending(mDisplay)) {
    event = (XEvent *)malloc(sizeof(XEvent));
    XNextEvent(mDisplay, event);
    aRealEvent = PR_TRUE;
    aEvent = event;
    return NS_OK;
  }
  aRealEvent = PR_FALSE;
  aEvent = nsnull;
  return NS_OK;
}

nsresult nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  nsresult rv;
  XEvent *event;
  if (mEventQueue == nsnull) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  rv = mEventQueue->ProcessPendingEvents();

  if (aRealEvent) {
    event = (XEvent *)aEvent;
    DispatchXEvent(event);
    free(event);
  }

  CallProcessTimeoutsProc(mDisplay);

  nsWindow::UpdateIdle(nsnull);
  return rv;
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


  // did someone pass us an x event for a window we don't own?
  // bad! bad!
  if (widget == nsnull)
    return;

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
  case SelectionRequest:
    HandleSelectionRequestEvent(event, widget);
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
  XEvent aEvent;

  if (mDragging) {
    HandleDragMotionEvent(event, aWidget);
  }

  mevent.widget = aWidget;
  mevent.time = 0;
  mevent.point.x = event->xmotion.x;
  mevent.point.y = event->xmotion.y;

  mevent.isShift = mShiftDown;
  mevent.isControl = mCtrlDown;
  mevent.isAlt = mAltDown;
  mevent.isMeta = mMetaDown;
  
  Display * dpy = (Display *)aWidget->GetNativeData(NS_NATIVE_DISPLAY);
  Window win = (Window)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  // We are only interested in the LAST (newest) location of the pointer
  while(XCheckWindowEvent(dpy,
                          win,
                          ButtonMotionMask,
                          &aEvent)) {
    mevent.point.x = aEvent.xmotion.x;
    mevent.point.y = aEvent.xmotion.y;
  }
  mevent.message = NS_MOUSE_MOVE;
  mevent.eventStructType = NS_MOUSE_EVENT;
  NS_ADDREF(aWidget);
  aWidget->DispatchMouseEvent(mevent);
  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleButtonEvent(XEvent *event, nsWidget *aWidget)
{
  nsMouseEvent mevent;
  mevent.isShift = mShiftDown;
  mevent.isControl = mCtrlDown;
  mevent.isAlt = mAltDown;
  mevent.isMeta = mMetaDown;
  PRUint32 eventType = 0;
  PRBool currentlyDragging = mDragging;
  nsMouseScrollEvent scrollEvent;

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Button event for window 0x%lx button %d type %s\n",
                                       event->xany.window,
                                       event->xbutton.button,
                                       (event->type == ButtonPress ? "ButtonPress" : "ButtonRelease")));
  switch(event->type) {
  case ButtonPress:
    switch(event->xbutton.button) {
    case 1:
      eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
      mDragging = PR_TRUE;
      break;
    case 2:
      eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
      break;
    case 3:
      eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
      break;
    case 4:
    case 5:
      scrollEvent.deltaLines = (event->xbutton.button == 4) ? -3 : 3;
      scrollEvent.message = NS_MOUSE_SCROLL;
      scrollEvent.widget = aWidget;
      scrollEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;

      scrollEvent.point.x = event->xbutton.x;
      scrollEvent.point.y = event->xbutton.y;

      scrollEvent.isShift = mShiftDown;
      scrollEvent.isControl = mCtrlDown;
      scrollEvent.isAlt = mAltDown;
      scrollEvent.isMeta = mMetaDown;
      scrollEvent.time = PR_Now();
      NS_IF_ADDREF(aWidget);
      aWidget->DispatchWindowEvent(scrollEvent);
      NS_IF_RELEASE(aWidget);
      return;
    }
    break;
  case ButtonRelease:
    switch(event->xbutton.button) {
    case 1:
      eventType = NS_MOUSE_LEFT_BUTTON_UP;
      mDragging = PR_FALSE;
      break;
    case 2:
      eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
      break;
    case 3:
      eventType = NS_MOUSE_RIGHT_BUTTON_UP;
      break;
    case 4:
    case 5:
      return;
    }
    break;
  }

  mevent.widget = aWidget;
  mevent.point.x = event->xbutton.x;
  mevent.point.y = event->xbutton.y;
  mevent.time = PR_Now();
  
  // If we are waiting longer than 1 sec for the second click, this is not a
  // double click.
  if (PR_Now() - mClickTime > 1000000)
    mClicked = PR_FALSE;               

  if (event->type == ButtonPress) {
    if (!mClicked) {
      mClicked = PR_TRUE;
      mClickTime = PR_Now();
      mClicks = 1;
      mClickedButton = event->xbutton.button;
    } else {
      mClickTime = PR_Now() - mClickTime;
      if ((mClickTime < 500000) && (mClickedButton == event->xbutton.button))
        mClicks = 2;
      else
        mClicks = 1;
      mClicked = PR_FALSE;
    }
  }

  if (currentlyDragging && !mDragging) {
    HandleDragDropEvent(event, aWidget);
  }

  mevent.message = eventType;
  mevent.eventStructType = NS_MOUSE_EVENT;
  mevent.clickCount = mClicks;
  NS_IF_ADDREF(aWidget);
  aWidget->DispatchMouseEvent(mevent);
  NS_IF_RELEASE(aWidget);
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
  //pevent.time = PR_Now();
    //pevent.region = nsnull;
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
  while (XCheckTypedWindowEvent(event->xany.display, 
                                event->xany.window, 
                                ConfigureNotify,
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
    else {
      PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("EVENT LOSSAGE\n"));
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

PRUint32 nsConvertCharCodeToUnicode(XKeyEvent* xkey)
{
  // The only Unicode specific at the moment is casting to PRUint32.

  // For control characters convert from the event ascii code (e.g. 1 for
  // control-a) to the ascii code for the key, e.g., 'a' for
  // control-a.

  KeySym         keysym;
  XComposeStatus compose;
  unsigned char  string_buf[CHAR_BUF_SIZE];
  int            len = 0;

  len = XLookupString(xkey, (char *)string_buf, CHAR_BUF_SIZE, &keysym, &compose);
  if (0 == len) return 0;

  if (xkey->state & ControlMask) {
    if (xkey->state & ShiftMask) {
      return (PRUint32)(string_buf[0] + 'A' - 1);
    }
    else {
      return (PRUint32)(string_buf[0] + 'a' - 1);
    }
  }
  if (!isprint(string_buf[0])) {
    return 0;
  }
  else {
    return (PRUint32)(string_buf[0]);
  }
}

void
nsAppShell::HandleKeyPressEvent(XEvent *event, nsWidget *aWidget)
{
  char      string_buf[CHAR_BUF_SIZE];
  int       len = 0;
  Window    focusWindow = 0;
  nsWidget *focusWidget = 0;

  // figure out where the real focus should go...
  focusWindow = nsWidget::GetFocusWindow();
  if (focusWindow) {
    focusWidget = nsWidget::GetWidgetForWindow(focusWindow);
  }

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("KeyPress event for window 0x%lx ( 0x%lx focus window )\n",
                                       event->xkey.window,
                                       focusWindow));

  // don't even bother...
  if (focusWidget == 0) {
    return;
  }

  KeySym     keysym = nsKeyCode::ConvertKeyCodeToKeySym(event->xkey.display,
                                                        event->xkey.keycode);

  switch (keysym) {
    case XK_Alt_L:
    case XK_Alt_R:
      mAltDown = PR_TRUE;
      break;
    case XK_Control_L:
    case XK_Control_R:
      mCtrlDown = PR_TRUE;
      break;
    case XK_Shift_L:
    case XK_Shift_R:
      mShiftDown = PR_TRUE;
      break;
    case XK_Meta_L:
    case XK_Meta_R:
      mMetaDown = PR_TRUE;
      break;
    default:
      break;
  }

  // Dont dispatch events for modifier keys pressed ALONE
  if (nsKeyCode::KeyCodeIsModifier(event->xkey.keycode))
  {
    return;
  }

  nsKeyEvent keyEvent;

  XComposeStatus compose;

  len = XLookupString(&event->xkey, string_buf, CHAR_BUF_SIZE, &keysym, &compose);
  string_buf[len] = '\0';

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = 0;
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = (event->xkey.state & ShiftMask) ? PR_TRUE : PR_FALSE;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  // I think 'meta' is the same as 'alt' in X11. Is this OK for other systems?
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.point.x = 0;
  keyEvent.point.y = 0;
  keyEvent.message = NS_KEY_DOWN;
  keyEvent.widget = focusWidget;
  keyEvent.eventStructType = NS_KEY_EVENT;

  //  printf("keysym = %x, keycode = %x, vk = %x\n",
  //         keysym,
  //         event->xkey.keycode,
  //         keyEvent.keyCode);

  focusWidget->DispatchKeyEvent(keyEvent);

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = nsConvertCharCodeToUnicode(&event->xkey);
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = (event->xkey.state & ShiftMask) ? PR_TRUE : PR_FALSE;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.point.x = 0;
  keyEvent.point.y = 0;
  keyEvent.message = NS_KEY_PRESS;
  keyEvent.widget = focusWidget;
  keyEvent.eventStructType = NS_KEY_EVENT;

  if (keyEvent.charCode)
  {
    /* This is the comment from the GTK code. Hope it makes more sense to you 
     * than it did for me.                                                    
     *  
     * if the control, meta, or alt key is down, then we should leave
     * the isShift flag alone (probably not a printable character)
     * if none of the other modifier keys are pressed then we need to
     * clear isShift so the character can be inserted in the editor
     */
    if (!keyEvent.isControl && !keyEvent.isAlt && !keyEvent.isMeta)
      keyEvent.isShift = PR_FALSE;
  }

  focusWidget->DispatchKeyEvent(keyEvent);

}

void
nsAppShell::HandleKeyReleaseEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("KeyRelease event for window 0x%lx\n",
                                       event->xkey.window));

  KeySym     keysym = nsKeyCode::ConvertKeyCodeToKeySym(event->xkey.display,
                                                        event->xkey.keycode);

  switch (keysym) {
    case XK_Alt_L:
    case XK_Alt_R:
      mAltDown = PR_FALSE;
      break;
    case XK_Control_L:
    case XK_Control_R:
      mCtrlDown = PR_FALSE;
      break;
    case XK_Shift_L:
    case XK_Shift_R:
      mShiftDown = PR_FALSE;
      break;
    case XK_Meta_L:
    case XK_Meta_R:
      mMetaDown = PR_FALSE;
      break;
    default:
      break;
  }

  // Dont dispatch events for modifier keys pressed ALONE
  if (nsKeyCode::KeyCodeIsModifier(event->xkey.keycode))
  {
    return;
  }

  nsKeyEvent keyEvent;

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.charCode = 0;
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;
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
  // FIXME FIXME FIXME
  // We now have to tell the widget that is has focus here, this was
  // Previously done in cross platform code.
  // FIXME FIXME FIXME
  aWidget->SetFocus();
  aWidget->DispatchWindowEvent(focusEvent);
  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleFocusOutEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("FocusOut event for window 0x%lx\n",
                                       event->xfocus.window));
  nsGUIEvent focusEvent;
  
  focusEvent.message = NS_DEACTIVATE;
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

  if (mDragging) {
    HandleDragEnterEvent(event, aWidget);
  }

  enterEvent.widget  = aWidget;
  
  enterEvent.time = event->xcrossing.time;
  enterEvent.point.x = nscoord(event->xcrossing.x);
  enterEvent.point.y = nscoord(event->xcrossing.y);
  
  enterEvent.message = NS_MOUSE_ENTER;
  enterEvent.eventStructType = NS_MOUSE_EVENT;

  // make sure this is in focus. This will do until I rewrite all the 
  // focus routines. KenF
  aWidget->SetFocus();

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
  
  if (mDragging) {
    HandleDragLeaveEvent(event, aWidget);
  }

  leaveEvent.widget  = aWidget;
  
  leaveEvent.time = event->xcrossing.time;
  leaveEvent.point.x = nscoord(event->xcrossing.x);
  leaveEvent.point.y = nscoord(event->xcrossing.y);
  
  leaveEvent.message = NS_MOUSE_EXIT;
  leaveEvent.eventStructType = NS_MOUSE_EVENT;
  
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
  // XXX set map status is gone now..
  //  aWidget->SetMapStatus(PR_TRUE);
}

void nsAppShell::HandleUnmapNotifyEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("UnmapNotifyEvent for window 0x%lx\n",
                                       event->xunmap.window));
  // XXX set map status is gone now..
  //aWidget->SetMapStatus(PR_FALSE);
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

void nsAppShell::HandleSelectionRequestEvent(XEvent *event, nsWidget *aWidget)
{
  nsGUIEvent ev;

  ev.widget = (nsIWidget *)aWidget;
  ev.nativeMsg = (void *)event;

  aWidget->DispatchWindowEvent(ev);
}

void nsAppShell::HandleDragMotionEvent(XEvent *event, nsWidget *aWidget) {
  nsMouseEvent mevent;
  PRBool currentlyDragging = PR_FALSE;

  nsCOMPtr<nsIDragService> dragService;
  nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports**)&dragService);
  if (NS_SUCCEEDED(rv)) {
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    dragServiceXlib->UpdatePosition(event->xmotion.x, event->xmotion.y);
    mevent.widget = aWidget;
    mevent.point.x = event->xmotion.x;
    mevent.point.y = event->xmotion.y;

    mevent.message = NS_DRAGDROP_OVER;
    mevent.eventStructType = NS_DRAGDROP_EVENT;

    NS_ADDREF(aWidget);
    aWidget->DispatchMouseEvent(mevent);
    NS_RELEASE(aWidget);
  }
}

void nsAppShell::HandleDragEnterEvent(XEvent *event, nsWidget *aWidget) {
  nsMouseEvent enterEvent;
  PRBool currentlyDragging = PR_FALSE;

  nsCOMPtr<nsIDragService> dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports**)&dragService);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    enterEvent.widget  = aWidget;
  
    enterEvent.point.x = event->xcrossing.x;
    enterEvent.point.y = event->xcrossing.y;
  
    enterEvent.message = NS_DRAGDROP_ENTER;
    enterEvent.eventStructType = NS_DRAGDROP_EVENT;

    NS_ADDREF(aWidget);
    aWidget->DispatchWindowEvent(enterEvent);
    NS_RELEASE(aWidget);
  }
}

void nsAppShell::HandleDragLeaveEvent(XEvent *event, nsWidget *aWidget) {
  nsMouseEvent leaveEvent;
  PRBool currentlyDragging = PR_FALSE;
  
  nsCOMPtr<nsIDragService> dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports**)&dragService);

  // FIXME: Not sure if currentlyDragging is required. KenF
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    leaveEvent.widget  = aWidget;
  
    leaveEvent.point.x = event->xcrossing.x;
    leaveEvent.point.y = event->xcrossing.y;
  
    leaveEvent.message = NS_DRAGDROP_EXIT;
    leaveEvent.eventStructType = NS_DRAGDROP_EVENT;

    NS_ADDREF(aWidget);
    aWidget->DispatchWindowEvent(leaveEvent);
    NS_RELEASE(aWidget);
  }
}

void nsAppShell::HandleDragDropEvent(XEvent *event, nsWidget *aWidget) {
  nsMouseEvent mevent;
  PRBool currentlyDragging = PR_FALSE;

  nsCOMPtr<nsIDragService> dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports**)&dragService);

  // FIXME: Dont think the currentlyDragging check is required. KenF
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    mevent.widget = aWidget;
    mevent.point.x = event->xbutton.x;
    mevent.point.y = event->xbutton.y;
  
    mevent.message = NS_DRAGDROP_DROP;
    mevent.eventStructType = NS_DRAGDROP_EVENT;

    NS_IF_ADDREF(aWidget);
    aWidget->DispatchMouseEvent(mevent);
    NS_IF_RELEASE(aWidget);

    dragService->EndDragSession();
  }
}

void nsAppShell::ForwardEvent(XEvent *event, nsWidget *aWidget)
{
  nsGUIEvent ev;
  ev.widget = (nsIWidget *)aWidget;
  ev.nativeMsg = (void *)event;

  aWidget->DispatchWindowEvent(ev);
}
