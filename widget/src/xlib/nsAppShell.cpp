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
#include "nsIWidget.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsITimer.h"

static PRUint8 convertMaskToCount(unsigned long val);
static PRUint8 getShiftForMask(unsigned long val);

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

extern "C"
void
xlib_rgb_init (Display *display, Screen *screen);

extern "C"
Visual *
xlib_rgb_get_visual (void);

// this is so that we can get the timers in the base.  most widget
// toolkits do this through some set of globals.  not here though.  we
// don't have that luxury
extern "C" int NS_TimeToNextTimeout(struct timeval *);
extern "C" void NS_ProcessTimeouts(void);


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

NS_IMPL_ADDREF(nsAppShell)
NS_IMPL_RELEASE(nsAppShell)

nsAppShell::nsAppShell()  
{ 
  NS_INIT_REFCNT();
  mDispatchListener = 0;
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
  int      num_visuals = 0;
  XVisualInfo vis_template;
  // open the display
  if ((gDisplay = XOpenDisplay(NULL)) == NULL) {
    fprintf(stderr, "%s: Cannot connect to X server %s\n",
            argv[0], XDisplayName(NULL));
    exit(1);
  }
  _Xdebug = 1;
  gScreenNum = DefaultScreen(gDisplay);
  gScreen = DefaultScreenOfDisplay(gDisplay);
  // init the rgb layer.  this will provide
  // the visual information for us.
  xlib_rgb_init(gDisplay, gScreen);
  gVisual = xlib_rgb_get_visual();
  // set the static vars for this class so we can find our
  // way around...
  vis_template.visualid = XVisualIDFromVisual(gVisual);
  gVisualInfo = XGetVisualInfo(gDisplay, VisualIDMask,
                               &vis_template, &num_visuals);
  if (gVisualInfo == NULL) {
    printf("nsAppShell::Create(): Warning: Failed to get XVisualInfo\n");
  }
  if (num_visuals != 1) {
    printf("nsAppShell:Create(): Warning: %d XVisualInfo structs were returned.\n", num_visuals);
  }
  // get the depth for this display
  gDepth = gVisualInfo->depth;
  // set up the color info for this display
  // set up the masks
  gRedMask = gVisualInfo->red_mask;
  gGreenMask = gVisualInfo->green_mask;
  gBlueMask = gVisualInfo->blue_mask;
  gAlphaMask = 0;
  // set up the number of bits in each
  gRedCount = convertMaskToCount(gVisualInfo->red_mask);
  gGreenCount = convertMaskToCount(gVisualInfo->green_mask);
  gBlueCount = convertMaskToCount(gVisualInfo->blue_mask);
  gAlphaCount = 0;
  // set up the number of bits that you need to shift to get to
  // a specific mask
  gRedShift = getShiftForMask(gVisualInfo->red_mask);
  gGreenShift = getShiftForMask(gVisualInfo->green_mask);
  gBlueShift = getShiftForMask(gVisualInfo->blue_mask);
  gAlphaShift = 0;
  return NS_OK;
}

NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

nsresult nsAppShell::Run()
{
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
  xlib_fd = ConnectionNumber(gDisplay);
  queue_fd = EQueue->GetEventQueueSelectFD();
  if (xlib_fd >= queue_fd) {
    max_fd = xlib_fd + 1;
  } else {
    max_fd = queue_fd + 1;
  }
  // process events.
  while (1) {
    XEvent          event;
    struct timeval  cur_time;
    struct timeval *cur_time_ptr;
    int             please_run_timer_queue = 0;
    gettimeofday(&cur_time, NULL);
    FD_ZERO(&select_set);
    // add the queue and the xlib connection to the select set
    FD_SET(queue_fd, &select_set);
    FD_SET(xlib_fd, &select_set);
    if (NS_TimeToNextTimeout(&cur_time) == 0) {
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
      //printf("xlib data available.\n");
      XNextEvent(gDisplay, &event);
      DispatchEvent(&event);
    }
    if (please_run_timer_queue) {
      //printf("Running timer queue...\n");
      NS_ProcessTimeouts();
    }
  }

	NS_IF_RELEASE(EQueue);
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
  return NS_OK;
}

NS_METHOD nsAppShell::Exit()
{
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
nsAppShell::DispatchEvent(XEvent *event)
{
  nsWidget *widget;
  widget = nsWidget::getWidgetForWindow(event->xany.window);
  // switch on the type of event
  switch (event->type) {
  case Expose:
    HandleExposeEvent(event, widget);
    break;
  default:
    printf("Unhandled window event: Window %ld Got a %s event\n",
           event->xany.window, event_names[event->type]);
    break;
  }
}

void
nsAppShell::HandleExposeEvent(XEvent *event, nsWidget *aWidget)
{
  printf("Expose event for window %ld %d %d %d %d\n", event->xany.window,
         event->xexpose.x, event->xexpose.y, event->xexpose.width, event->xexpose.height);
  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.widget = aWidget;
  pevent.eventStructType = NS_PAINT_EVENT;
  pevent.rect = new nsRect (event->xexpose.x, event->xexpose.y,
                            event->xexpose.width, event->xexpose.height);
  // XXX fix this
  pevent.time = 0;
  NS_ADDREF(aWidget);
  aWidget->OnPaint(pevent);
  NS_RELEASE(aWidget);
  delete pevent.rect;
}

static PRUint8 convertMaskToCount(unsigned long val)
{
  PRUint8 retval = 0;
  PRUint8 cur_bit = 0;
  // walk through the number, incrementing the value if
  // the bit in question is set.
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      retval++;
    }
    cur_bit++;
  }
  return retval;
}

static PRUint8 getShiftForMask(unsigned long val)
{
  PRUint8 cur_bit = 0;
  // walk through the number, looking for the first 1
  while (cur_bit < (sizeof(unsigned long) * 8)) {
    if ((val >> cur_bit) & 0x1) {
      return cur_bit;
    }
    cur_bit++;
  }
  return cur_bit;
}
