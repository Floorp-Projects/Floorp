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

#include <sys/time.h>
#include <unistd.h>
#include "nsWidget.h"
#include "nsAppShell.h"
#include "nsIWidget.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsIServiceManager.h"
#include "nsITimer.h"


static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

// this is so that we can get the timers in the base.  most widget
// toolkits do this through some set of globals.  not here though.  we
// don't have that luxury
extern "C" int NS_TimeToNextTimeout(struct timeval *);
extern "C" void NS_ProcessTimeouts(void);

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
  // set the static vars for this class so we can find our
  // way around...
  gScreenNum = DefaultScreen(gDisplay);
  gScreen = DefaultScreenOfDisplay(gDisplay);
  gVisual = DefaultVisual(gDisplay, gScreenNum);
  vis_template.visualid = XVisualIDFromVisual(gVisual);
  gVisualInfo = XGetVisualInfo(gDisplay, VisualIDMask,
                               &vis_template, &num_visuals);
  if (gVisualInfo == NULL) {
    printf("nsAppShell::Create(): Warning: Failed to get XVisualInfo\n");
  }
  if (num_visuals != 0) {
    printf("nsAppShell:Create(): Warning: %d XVisualInfo structs were returned.\n", num_visuals);
  }
  // get the depth for this display
  gDepth = DefaultDepth(gDisplay, gScreenNum);
  return NS_OK;
}

NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
{
  return NS_OK;
}

nsresult nsAppShell::Run()
{
  nsresult rv = NS_OK;
  PLEventQueue *EQueue = nsnull;
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
  if (nsnull != EQueue)
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
  queue_fd = PR_GetEventQueueSelectFD(EQueue);
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
      printf("Timer ran out...\n");
      please_run_timer_queue = 1;
    }
    // check to see if there's data avilable for the queue
    if (FD_ISSET(queue_fd, &select_set)) {
      printf("queue data available.\n");
      PR_ProcessPendingEvents(EQueue);
    }
    // check to see if there's data avilable for
    // xlib
    if (FD_ISSET(xlib_fd, &select_set)) {
      printf("xlib data available.\n");
      XNextEvent(gDisplay, &event);
    }
    if (please_run_timer_queue) {
      printf("Running timer queue...\n");
      NS_ProcessTimeouts();
    }
  }
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

NS_METHOD
nsAppShell::GetSelectionMgr(nsISelectionMgr** aSelectionMgr)
{
  return NS_OK;
}
