/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *   Caspian Maclean <caspian@igelaus.com.au>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xlocale.h>

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

#include "xlibrgb.h"

#define CHAR_BUF_SIZE 80

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

/* nsAppShell static members */
Display *nsAppShell::mDisplay = nsnull;
XlibRgbHandle *nsAppShell::mXlib_rgb_handle = nsnull;
XtAppContext nsAppShell::mAppContext;
PRTime nsAppShell::mClickTime = 0;
PRInt16 nsAppShell::mClicks = 1;
PRUint16 nsAppShell::mClickedButton = 0;
PRPackedBool nsAppShell::mClicked = PR_FALSE;
PRPackedBool nsAppShell::mDragging  = PR_FALSE;
PRPackedBool nsAppShell::mAltDown   = PR_FALSE;
PRPackedBool nsAppShell::mShiftDown = PR_FALSE;
PRPackedBool nsAppShell::mCtrlDown  = PR_FALSE;
PRPackedBool nsAppShell::mMetaDown  = PR_FALSE;
PRPackedBool nsAppShell::DieAppShellDie = PR_FALSE;

static PLHashTable *sQueueHashTable = nsnull;
static PLHashTable *sCountHashTable = nsnull;
static nsVoidArray *sEventQueueList = nsnull;


// For debugging.
static const char *event_names[] = 
{
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

#define COMPARE_FLAG1( a,b) ((b)[0]=='-' && !strcmp((a), &(b)[1]))
#define COMPARE_FLAG2( a,b) ((b)[0]=='-' && (b)[1]=='-' && !strcmp((a), &(b)[2]))
#define COMPARE_FLAG12(a,b) ((b)[0]=='-' && !strcmp((a), (b)[1]=='-'?&(b)[2]:&(b)[1]))

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

nsAppShell::nsAppShell()  
{ 
  if (!sEventQueueList)
    sEventQueueList = new nsVoidArray();

  mEventQueue = nsnull;
}

NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

PR_BEGIN_EXTERN_C
static 
int xerror_handler( Display *display, XErrorEvent *ev )
{
  /* this should _never_ be happen... but if this happens - debug mode or not - scream !!! */
  char errmsg[80];
  XGetErrorText(display, ev->error_code, errmsg, sizeof(errmsg));
  fprintf(stderr, "nsAppShellXlib: Warning (X Error) -  %s\n", errmsg);
  abort(); // die !!
  
  return 0;
}
PR_END_EXTERN_C

NS_METHOD nsAppShell::Create(int* bac, char ** bav)
{
  /* Create the Xt Application context... */
  if (mAppContext == nsnull) {
    int      argc = bac ? *bac : 0;
    char   **argv = bav;
    nsresult rv;

    char        *displayName    = nsnull;
    Bool         synchronize    = False;
    int          i;
    XlibRgbArgs  xargs;
    memset(&xargs, 0, sizeof(xargs));
    /* Use a "well-known" name that other modules can "look-up" this handle
     * via |xxlib_find_handle| ... */
    xargs.handle_name = XXLIBRGB_DEFAULT_HANDLE;

    for (i = 0; ++i < argc-1; ) {
      /* allow both --display and -display */
      if (COMPARE_FLAG12 ("display", argv[i])) {
        displayName=argv[i+1];
        break;
      }
    }
    for (i = 0; ++i < argc-1; ) {
      if (COMPARE_FLAG1 ("visual", argv[i])) {
        xargs.xtemplate_mask |= VisualIDMask;
        xargs.xtemplate.visualid = strtol(argv[i+1], NULL, 0);
        break;
      }
    }   
    for (i = 0; ++i < argc; ) {
      if (COMPARE_FLAG1 ("sync", argv[i])) {
        synchronize = True;
        break;
      }
    }
    for (i = 0; ++i < argc; ) {
      /* allow both --no-xshm and -no-xshm */
      if (COMPARE_FLAG12 ("no-xshm", argv[i])) {
        xargs.disallow_mit_shmem = True;
        break;
      }
    }    
    for (i = 0; ++i < argc; ) {
      if (COMPARE_FLAG1 ("install_colormap", argv[i])) {
        xargs.install_colormap = True;
        break;
      }
    }
    
    /* setup locale */
    if (!setlocale (LC_ALL,""))
      NS_WARNING("locale not supported by C library");
  
    if (!XSupportsLocale ()) {
      NS_WARNING("locale not supported by Xlib, locale set to C");
      setlocale (LC_ALL, "C");
    }
  
    if (!XSetLocaleModifiers (""))
      NS_WARNING("can not set locale modifiers");

    XtToolkitInitialize();
    mAppContext = XtCreateApplicationContext();

    if (!(mDisplay = XtOpenDisplay (mAppContext, displayName, 
                                    "Mozilla5", "Mozilla5", nsnull, 0, 
                                    &argc, argv))) 
    {
      fprintf (stderr, "%s:  unable to open display \"%s\"\n", argv[0], XDisplayName(displayName));
      exit (EXIT_FAILURE);
    }
    
    // Requires XSynchronize(mDisplay, True); To stop X buffering. Use this
    // to make debugging easier. KenF
    if (synchronize)
    {
      /* Set error handler which calls abort() - only usefull with an 
       * unbuffered X connection */
      (void)XSetErrorHandler(xerror_handler);
      
      NS_WARNING("running via unbuffered X connection.");
      XSynchronize(mDisplay, True);
    }
       
    mXlib_rgb_handle = xxlib_rgb_create_handle(mDisplay, XDefaultScreenOfDisplay(mDisplay),
                                               &xargs);
    if (!mXlib_rgb_handle)
    {
      fprintf (stderr, "%s:  unable to create Xlib context\n", argv[0]);
      exit (EXIT_FAILURE);
    }
  }

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsAppShell::Create(dpy=%p)\n",
         mDisplay));

  return NS_OK;
}

NS_IMETHODIMP nsAppShell::Spinup()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_APPSHELL
  printf("nsAppShell::Spinup()\n");
#endif

  /* Get the event queue service */
  nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID, &rv);

  if (NS_FAILED(rv)) {
    NS_WARNING("Could not obtain event queue service");
    return rv;
  }

  /* Get the event queue for the thread.*/
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
  
  /* If we got an event queue, use it. */
  if (!mEventQueue) {
    /* otherwise create a new event queue for the thread */
    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) {
      NS_WARNING("Could not create the thread event queue");
      return rv;
    }

    /* Ask again nicely for the event queue now that we have created one. */
    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
    if (NS_FAILED(rv)) {
      NS_WARNING("Could not get the thread event queue");
      return rv;
    }
  }

  ListenToEventQueue(mEventQueue, PR_TRUE);

  return rv;
}

/* must be a |XtInputCallbackProc| !! */
PR_BEGIN_EXTERN_C
static
void HandleQueueXtProc(XtPointer ptr, int *source_fd, XtInputId* id)
{
  nsIEventQueue *queue = (nsIEventQueue *)ptr;
  queue->ProcessPendingEvents();
}
PR_END_EXTERN_C

nsresult nsAppShell::Run()
{
  if (mEventQueue == nsnull)
    Spinup();

  if (mEventQueue == nsnull) {
    NS_WARNING("Cannot initialize the Event Queue");
    return NS_ERROR_NOT_INITIALIZED;
  }
               
  XEvent xevent;
  
  /* process events. */
  while (!DieAppShellDie) 
  {   
    XtAppNextEvent(mAppContext, &xevent);
  
    if (XtDispatchEvent(&xevent) == False)
      DispatchXEvent(&xevent);
    
    if (XEventsQueued(mDisplay, QueuedAlready) == 0)
    {
      /* Flush the nsWindow's drawing queue */
      nsWindow::UpdateIdle(nsnull);
    }
  }
  
  Spindown();
  return NS_OK;
}

NS_METHOD nsAppShell::Spindown()
{
  if (mEventQueue) {
    ListenToEventQueue(mEventQueue, PR_FALSE);
    mEventQueue->ProcessPendingEvents();
    mEventQueue = nsnull;
  }

  return NS_OK;
}

#define NUMBER_HASH_KEY(_num) ((PLHashNumber) _num)

static PLHashNumber
IntHashKey(PRInt32 key)
{
  return NUMBER_HASH_KEY(key);
}
// wrapper so we can call a macro
PR_BEGIN_EXTERN_C
static unsigned long getNextRequest (void *aClosure) {
  return XNextRequest(nsAppShell::mDisplay);
}
PR_END_EXTERN_C

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  if (!mEventQueue) {
    NS_WARNING("nsAppShell::ListenToEventQueue(): No event queue available.");
    return NS_ERROR_NOT_INITIALIZED;
  }
  
#ifdef DEBUG_APPSHELL
  printf("ListenToEventQueue(%p, %d) this=%p\n", aQueue, aListen, this);
#endif
  if (!sQueueHashTable) {
    sQueueHashTable = PL_NewHashTable(3, (PLHashFunction)IntHashKey,
                                      PL_CompareValues, PL_CompareValues, 0, 0);
  }
  if (!sCountHashTable) {
    sCountHashTable = PL_NewHashTable(3, (PLHashFunction)IntHashKey,
                                      PL_CompareValues, PL_CompareValues, 0, 0);
  }    

  int   queue_fd = aQueue->GetEventQueueSelectFD();
  void *key      = aQueue;
  if (aListen) {
    /* Add listener -
     * but only if we arn't already in the table... */
    if (!PL_HashTableLookup(sQueueHashTable, key)) {
      long tag;
        
      /* set up our fds callbacks */
      tag = (long)XtAppAddInput(mAppContext,
                                queue_fd,
                                (XtPointer)(long)(XtInputReadMask),
                                HandleQueueXtProc,
                                (XtPointer)mEventQueue);

/* This hack would not be neccesary if we would have a hashtable function
 * which returns success/failure in a separate var ...
 */
#define NEVER_BE_ZERO_MAGIC (54321) 
      tag += NEVER_BE_ZERO_MAGIC; /* be sure that |tag| is _never_ 0 */
      NS_ASSERTION(tag!=0, "tag is 0 while adding");
      
      if (tag) {
        PL_HashTableAdd(sQueueHashTable, key, (void *)tag);
      }
      
      PLEventQueue *plqueue;
      aQueue->GetPLEventQueue(&plqueue);
      PL_RegisterEventIDFunc(plqueue, getNextRequest, 0);
      sEventQueueList->AppendElement(plqueue);
    }
  } else {
    /* Remove listener... */   
    PLEventQueue *plqueue;
    aQueue->GetPLEventQueue(&plqueue);
    PL_UnregisterEventIDFunc(plqueue);
    sEventQueueList->RemoveElement(plqueue);

    int tag = long(PL_HashTableLookup(sQueueHashTable, key));
    if (tag) {
      tag -= NEVER_BE_ZERO_MAGIC;
      XtRemoveInput((XtInputId)tag);
      PL_HashTableRemove(sQueueHashTable, key);
    }  
  }

  return NS_OK;
}

/* Does nothing. Used by xp code with non-gtk expectations.
 * this method will be removed once xp eventloops are working.
 */
NS_METHOD
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  aRealEvent = PR_FALSE;
  aEvent     = nsnull;

  return NS_OK;
}

nsresult nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  XEvent xevent;
  
  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;

#if 1
  /* gisburn: Why do we have to call this explicitly ?
   * I have registered a callback via XtAddAppInput() above... 
   */  
  mEventQueue->ProcessPendingEvents();  
#endif

  XtAppNextEvent(mAppContext, &xevent);
    
  if (XtDispatchEvent(&xevent) == False)
    DispatchXEvent(&xevent);
   
  if (XEventsQueued(mDisplay, QueuedAlready) == 0)
  {
    /* Flush the nsWindow's drawing queue */
    nsWindow::UpdateIdle(nsnull);
  }
    
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
  if (mDragging) {
    HandleDragMotionEvent(event, aWidget);
  }

  nsMouseEvent mevent(NS_MOUSE_MOVE, aWidget);
  XEvent aEvent;

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
  NS_ADDREF(aWidget);
  aWidget->DispatchMouseEvent(mevent);
  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleButtonEvent(XEvent *event, nsWidget *aWidget)
{
  PRUint32 eventType = 0;
  PRBool currentlyDragging = mDragging;
  nsMouseScrollEvent scrollEvent(NS_MOUSE_SCROLL, aWidget);

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
      /* look back into this in case anything actually needs a
       * NS_MOUSE_RIGHT_BUTTON_DOWN */
      eventType = NS_CONTEXTMENU;
      break;
    case 4:
    case 5:
      scrollEvent.delta = (event->xbutton.button == 4) ? -3 : 3;
      scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;

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

  nsMouseEvent mevent(eventType, aWidget);
  mevent.isShift = mShiftDown;
  mevent.isControl = mCtrlDown;
  mevent.isAlt = mAltDown;
  mevent.isMeta = mMetaDown;
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

  if (currentlyDragging && !mDragging)
    HandleDragDropEvent(event, aWidget);

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

  nsRect dirtyRect(event->xexpose.x, event->xexpose.y, 
                   event->xexpose.width, event->xexpose.height);

  /* compress expose events...
   */
  if (event->xexpose.count!=0) {
     XEvent txe;
     do {
        XWindowEvent(event->xany.display, event->xany.window, ExposureMask, (XEvent *)&txe);
        dirtyRect.UnionRect(dirtyRect, nsRect(txe.xexpose.x, txe.xexpose.y, 
                                              txe.xexpose.width, txe.xexpose.height));
     } while (txe.xexpose.count>0);
  }

  aWidget->Invalidate(dirtyRect, PR_FALSE);
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

  nsSizeEvent sevent(NS_SIZE, aWidget);
  sevent.windowSize = new nsRect (event->xconfigure.x, event->xconfigure.y,
                                  event->xconfigure.width, event->xconfigure.height);
  sevent.point.x = event->xconfigure.x;
  sevent.point.y = event->xconfigure.y;
  sevent.mWinWidth = event->xconfigure.width;
  sevent.mWinHeight = event->xconfigure.height;
  // XXX fix sevent.time
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

  len = XLookupString(xkey, (char *)string_buf, CHAR_BUF_SIZE-1, &keysym, &compose);
  if (0 == len)
    return 0;

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
  Window    focusWindow = None;
  nsWidget *focusWidget = 0;

  // figure out where the real focus should go...
  focusWindow = nsWidget::GetFocusWindow();
  if (focusWindow != None) {
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

  nsKeyEvent keyEvent(NS_KEY_DOWN, focusWidget);

  XComposeStatus compose;

  len = XLookupString(&event->xkey, string_buf, CHAR_BUF_SIZE-1, &keysym, &compose);
  string_buf[len] = '\0';

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = (event->xkey.state & ShiftMask) ? PR_TRUE : PR_FALSE;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  // I think 'meta' is the same as 'alt' in X11. Is this OK for other systems?
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;

  //  printf("keysym = %x, keycode = %x, vk = %x\n",
  //         keysym,
  //         event->xkey.keycode,
  //         keyEvent.keyCode);

  focusWidget->DispatchKeyEvent(keyEvent);

  nsKeyEvent pressEvent(NS_KEY_PRESS, focusWidget);
  pressEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  pressEvent.charCode = nsConvertCharCodeToUnicode(&event->xkey);
  pressEvent.time = event->xkey.time;
  pressEvent.isShift = (event->xkey.state & ShiftMask) ? PR_TRUE : PR_FALSE;
  pressEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  pressEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  pressEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;

  focusWidget->DispatchKeyEvent(pressEvent);

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

  nsKeyEvent keyEvent(NS_KEY_UP, aWidget);

  keyEvent.keyCode = nsKeyCode::ConvertKeySymToVirtualKey(keysym);
  keyEvent.time = event->xkey.time;
  keyEvent.isShift = event->xkey.state & ShiftMask;
  keyEvent.isControl = (event->xkey.state & ControlMask) ? 1 : 0;
  keyEvent.isAlt = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.isMeta = (event->xkey.state & Mod1Mask) ? 1 : 0;
  keyEvent.point.x = event->xkey.x;
  keyEvent.point.y = event->xkey.y;

  NS_ADDREF(aWidget);

  aWidget->DispatchKeyEvent(keyEvent);

  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleFocusInEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("FocusIn event for window 0x%lx\n",
                                       event->xfocus.window));
  nsFocusEvent focusEvent(NS_GOTFOCUS, aWidget);
  
  NS_ADDREF(aWidget);
  aWidget->DispatchWindowEvent(focusEvent);
  NS_RELEASE(aWidget);
}

void
nsAppShell::HandleFocusOutEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("FocusOut event for window 0x%lx\n",
                                       event->xfocus.window));
  nsFocusEvent focusEvent(NS_LOSTFOCUS, aWidget);

  NS_ADDREF(aWidget);
  aWidget->DispatchWindowEvent(focusEvent);
  NS_RELEASE(aWidget);
}

/* Identify pointer grab/ungrab events due to window manager activity */
static inline int
is_wm_ungrab_enter(XCrossingEvent *event)
{
  return (NotifyGrab == event->mode) &&
    ((NotifyAncestor == event->detail) ||
     (NotifyVirtual == event->detail));
}

static inline int
is_wm_grab_leave(XCrossingEvent *event)
{
  return (NotifyGrab == event->mode) &&
    ((NotifyAncestor == event->detail) ||
     (NotifyVirtual == event->detail));
}

void
nsAppShell::HandleEnterEvent(XEvent *event, nsWidget *aWidget)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Enter event for window 0x%lx\n",
                                       event->xcrossing.window));

  if (event->xcrossing.subwindow != None)
    return;

  if(is_wm_ungrab_enter(&event->xcrossing))
    return;

  if (mDragging) {
    HandleDragEnterEvent(event, aWidget);
  }

  nsMouseEvent enterEvent(NS_MOUSE_ENTER, aWidget);

  enterEvent.time = event->xcrossing.time;
  enterEvent.point.x = nscoord(event->xcrossing.x);
  enterEvent.point.y = nscoord(event->xcrossing.y);
  
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

  if (event->xcrossing.subwindow != None)
    return;

  if(is_wm_grab_leave(&event->xcrossing))
    return;

  if (mDragging) {
    HandleDragLeaveEvent(event, aWidget);
  }

  nsMouseEvent leaveEvent(NS_MOUSE_EXIT, aWidget);

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
#if defined(DEBUG_warren) || defined(DEBUG_quy)
  printf("handling client message\n");
#endif
  if (nsWidget::WMProtocolsInitialized) {
    if ((Atom)event->xclient.data.l[0] == nsWidget::WMDeleteWindow) {
#ifdef DEBUG
      printf("got a delete window event\n");
#endif /* DEBUG */      
      aWidget->OnDeleteWindow();
    }
  }
}

void nsAppShell::HandleSelectionRequestEvent(XEvent *event, nsWidget *aWidget)
{
  nsGUIEvent ev(0, aWidget);

  ev.nativeMsg = (void *)event;

  aWidget->DispatchWindowEvent(ev);
}

void nsAppShell::HandleDragMotionEvent(XEvent *event, nsWidget *aWidget) {
  PRBool currentlyDragging = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIDragService> dragService( do_GetService(kCDragServiceCID, &rv) );
  nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
  if (NS_SUCCEEDED(rv)) {
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    dragServiceXlib->UpdatePosition(event->xmotion.x, event->xmotion.y);

    nsMouseEvent mevent(NS_DRAGDROP_OVER, aWidget);
    mevent.point.x = event->xmotion.x;
    mevent.point.y = event->xmotion.y;

    NS_ADDREF(aWidget);
    aWidget->DispatchMouseEvent(mevent);
    NS_RELEASE(aWidget);
  }
}

void nsAppShell::HandleDragEnterEvent(XEvent *event, nsWidget *aWidget) {
  PRBool currentlyDragging = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIDragService> dragService( do_GetService(kCDragServiceCID, &rv) );
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    nsMouseEvent enterEvent(NS_DRAGDROP_ENTER, aWidget);
  
    enterEvent.point.x = event->xcrossing.x;
    enterEvent.point.y = event->xcrossing.y;
  
    NS_ADDREF(aWidget);
    aWidget->DispatchWindowEvent(enterEvent);
    NS_RELEASE(aWidget);
  }
}

void nsAppShell::HandleDragLeaveEvent(XEvent *event, nsWidget *aWidget) {
  PRBool currentlyDragging = PR_FALSE;
  
  nsresult rv;
  nsCOMPtr<nsIDragService> dragService( do_GetService(kCDragServiceCID, &rv) );

  // FIXME: Not sure if currentlyDragging is required. KenF
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    nsMouseEvent leaveEvent(NS_DRAGDROP_EXIT, aWidget);
  
    leaveEvent.point.x = event->xcrossing.x;
    leaveEvent.point.y = event->xcrossing.y;
  
    NS_ADDREF(aWidget);
    aWidget->DispatchWindowEvent(leaveEvent);
    NS_RELEASE(aWidget);
  }
}

void nsAppShell::HandleDragDropEvent(XEvent *event, nsWidget *aWidget) {
  PRBool currentlyDragging = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIDragService> dragService( do_GetService(kCDragServiceCID, &rv) );

  // FIXME: Dont think the currentlyDragging check is required. KenF
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDragSessionXlib> dragServiceXlib;
    dragServiceXlib = do_QueryInterface(dragService);
    if (dragServiceXlib) {
      dragServiceXlib->IsDragging(&currentlyDragging);
    }
  }

  if (currentlyDragging) {
    nsMouseEvent mevent(NS_DRAGDROP_DROP, aWidget);
    mevent.point.x = event->xbutton.x;
    mevent.point.y = event->xbutton.y;
  
    NS_IF_ADDREF(aWidget);
    aWidget->DispatchMouseEvent(mevent);
    NS_IF_RELEASE(aWidget);

    dragService->EndDragSession();
  }
}

void nsAppShell::ForwardEvent(XEvent *event, nsWidget *aWidget)
{
  nsGUIEvent ev(0, aWidget);
  ev.nativeMsg = (void *)event;

  aWidget->DispatchWindowEvent(ev);
}

