/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *		John C. Griggs <johng@corel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//JCG #define DBG_JCG 1

#include "nsQApplication.h"
#include "nsQWidget.h"

#include <qwindowsstyle.h>
#include <qcursor.h>

#ifdef DBG_JCG
/* Required for x11EventFilter & _x_error debugging hooks */
#include "X11/Xlib.h"
#include <assert.h>

PRInt32 gQAppID = 0;
PRInt32 gQAppCount = 0;

PRInt32 gQQueueID = 0;
PRInt32 gQQueueCount = 0;
#endif

nsQApplication *nsQApplication::mInstance = nsnull;
QIntDict<nsQtEventQueue> nsQApplication::mQueueDict;
PRUint32 nsQApplication::mRefCnt = 0;
QWidget *nsQApplication::mMasterWidget = nsnull;

#ifdef DBG_JCG
int _x_error(Display *display, XErrorEvent *error)
{
  if (error->error_code) {
    char buf[64];
	  
    XGetErrorText (display, error->error_code, buf, 63);

    fprintf(stderr,
     "X-ERROR **: %s\n  serial %ld error_code %d request_code %d minor_code %d, resourceId: %ld\n",
            buf, error->serial, error->error_code, error->request_code,
            error->minor_code,error->resourceid);

    printf("X-ERROR **: %s\n  serial %ld error_code %d request_code %d minor_code %d, resourceId: %ld\n",
           buf, error->serial, error->error_code, error->request_code,
           error->minor_code,error->resourceid);

    assert(0);
  }
  return 0;
}
#endif  //DBG_JCG

nsQApplication* nsQApplication::Instance(int argc,char** argv)
{
  if (!mInstance)
    mInstance = new nsQApplication(argc,argv);

  mRefCnt++;

  return mInstance;
}

void nsQApplication::Release()
{
  mRefCnt--;
  if (mRefCnt <= 0) {
    if (mMasterWidget) {
      delete mMasterWidget;
      mMasterWidget = nsnull;
    }
    delete mInstance;
    mInstance = nsnull;
  }
}

QWidget *nsQApplication::GetMasterWidget()
{
  if (!mMasterWidget)
    mMasterWidget = new QWidget();

  return mMasterWidget;
}

nsQApplication::nsQApplication(int argc,char** argv)
	: QApplication(argc,argv)
{
#ifdef DBG_JCG
  gQAppCount++;
  mID = gQAppID++;
  printf("JCG nsQApplication CTOR (%p) ID: %d, Count: %d\n",this,mID,gQAppCount);

  XSetErrorHandler (_x_error);
#endif
  if (mInstance)
    NS_ASSERTION(mInstance == nsnull,
                 "Attempt to create duplicate QApplication Object.");
  mInstance = this;
  setGlobalMouseTracking(true);
  setStyle(new QWindowsStyle);
  setOverrideCursor(QCursor(ArrowCursor),PR_TRUE);
  connect(this,SIGNAL(lastWindowClosed()),this,SLOT(quit()));
}

nsQApplication::~nsQApplication()
{
#ifdef DBG_JCG
  gQAppCount--;
  printf("JCG nsQApplication DTOR (%p) ID: %d, Count: %d\n",this,mID,gQAppCount);
#endif
  setGlobalMouseTracking(false);
}

void nsQApplication::AddEventProcessorCallback(nsIEventQueue* EQueue)
{
  nsQtEventQueue* que = nsnull;

  if ((que = mQueueDict.find(EQueue->GetEventQueueSelectFD()))) {
    que->IncRefCnt();
  }
  else {
     mQueueDict.insert(EQueue->GetEventQueueSelectFD(),
                       new nsQtEventQueue(EQueue));
  }
}

void nsQApplication::RemoveEventProcessorCallback(nsIEventQueue* EQueue)
{
  nsQtEventQueue* que = nsnull;

  if ((que = mQueueDict.find(EQueue->GetEventQueueSelectFD()))) {
     que->DataReceived();
     if (que->DecRefCnt() <= 0) {
       mQueueDict.take(EQueue->GetEventQueueSelectFD());
       delete que;
     }
  }
}

/* Hook for capturing X11 Events before they are processed by Qt */
bool nsQApplication::x11EventFilter(XEvent* event)
{
#ifdef DBG_JCG
  switch (event->type) {
    case ButtonPress:
    case ButtonRelease:
    {
       XButtonPressedEvent *ptr = (XButtonPressedEvent*)event;
       printf("JCG: ButtonPress/Release: serial %ld, Window: %ld, Root: %ld, Child: %ld\n",ptr->serial,ptr->window,ptr->root,ptr->subwindow);
    }
    break;

    case CirculateNotify:
    {
       XCirculateEvent *ptr = (XCirculateEvent*)event;
       printf("JCG: CirculateNotify: serial %ld, Event: %ld, Window: %ld\n",ptr->serial,ptr->event,ptr->window);
    }
    break;

    case CirculateRequest:
    {
       XCirculateRequestEvent *ptr = (XCirculateRequestEvent*)event;
       printf("JCG: CirculateRequest: serial %ld, Parent: %ld, Window: %ld\n",ptr->serial,ptr->parent,ptr->window);
    }
    break;

    case ClientMessage:
    {
       XClientMessageEvent *ptr = (XClientMessageEvent*)event;
       printf("JCG: ClientMessage: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case ColormapNotify:
    {
       XColormapEvent *ptr = (XColormapEvent*)event;
       printf("JCG: ColormapNotify: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case ConfigureNotify:
    {
       XConfigureEvent *ptr = (XConfigureEvent*)event;
       printf("JCG: ConfigureNotify: serial %ld, Event: %ld, Window: %ld\n",ptr->serial,ptr->event,ptr->window);
    }
    break;

    case ConfigureRequest:
    {
       XConfigureRequestEvent *ptr = (XConfigureRequestEvent*)event;
       printf("JCG: ConfigureRequest: serial %ld, Parent: %ld, Window: %ld\n",ptr->serial,ptr->parent,ptr->window);
    }
    break;

    case CreateNotify:
    {
       XCreateWindowEvent *ptr = (XCreateWindowEvent*)event;
       printf("JCG: CreateNotify: serial %ld, Parent: %ld, Window: %ld\n",ptr->serial,ptr->parent,ptr->window);
    }
    break;

    case DestroyNotify:
    {
       XDestroyWindowEvent *ptr = (XDestroyWindowEvent*)event;
       printf("JCG: DestroyNotify: serial %ld, Event: %ld, Window: %ld\n",ptr->serial,ptr->event,ptr->window);
    }
    break;

    case EnterNotify:
    case LeaveNotify:
    {
       XCrossingEvent *ptr = (XCrossingEvent*)event;
       printf("JCG: Enter/LeaveNotify: serial %ld, Window: %ld, Parent: %ld, Child: %ld\n",ptr->serial,ptr->window,ptr->root,ptr->subwindow);
    }
    break;

    case Expose:
    {
       XExposeEvent *ptr = (XExposeEvent*)event;
       printf("JCG: Expose: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case FocusIn:
    case FocusOut:
    {
       XFocusChangeEvent *ptr = (XFocusChangeEvent*)event;
       printf("JCG: FocusIn/Out: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case GraphicsExpose:
    case NoExpose:
    {
       printf("JCG: Graphics/NoExpose\n");
    }
    break;

    case GravityNotify:
    {
       XGravityEvent *ptr = (XGravityEvent*)event;
       printf("JCG: GravityNotify: serial %ld, Event: %ld, Window: %ld\n",ptr->serial,ptr->event,ptr->window);
    }
    break;

    case KeymapNotify:
    {
       XKeymapEvent *ptr = (XKeymapEvent*)event;
       printf("JCG: KeymapNotify: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case KeyPress:
    case KeyRelease:
    {
       XKeyEvent *ptr = (XKeyEvent*)event;
       printf("JCG: KeyPress/Release: serial %ld, Window: %ld, Parent: %ld, Child: %ld\n",ptr->serial,ptr->window,ptr->root,ptr->subwindow);
    }
    break;

    case MapNotify:
    case UnmapNotify:
    {
       XMapEvent *ptr = (XMapEvent*)event;
       printf("JCG: Map/UnmapNotify: serial %ld, Window: %ld, Event: %ld\n",ptr->serial,ptr->window,ptr->event);
    }
    break;

    case MappingNotify:
    {
       XMappingEvent *ptr = (XMappingEvent*)event;
       printf("JCG: MappingNotify: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case MapRequest:
    {
       XMapRequestEvent *ptr = (XMapRequestEvent*)event;
       printf("JCG: MapRequest: serial %ld, Window: %ld, Parent: %ld\n",ptr->serial,ptr->window,ptr->parent);
    }
    break;

    case MotionNotify:
    {
       XMotionEvent *ptr = (XMotionEvent*)event;
       printf("JCG: MotionNotify: serial %ld, Window: %ld, Parent: %ld, Child: %ld\n",ptr->serial,ptr->window,ptr->root,ptr->subwindow);
    }
    break;

    case PropertyNotify:
    {
       XPropertyEvent *ptr = (XPropertyEvent*)event;
       printf("JCG: PropertyNotify: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case ReparentNotify:
    {
       XReparentEvent *ptr = (XReparentEvent*)event;
       printf("JCG: ReparentNotify: serial %ld, Window: %ld, Parent: %ld, Event: %ld\n",ptr->serial,ptr->window,ptr->parent,ptr->event);
    }
    break;

    case ResizeRequest:
    {
       XResizeRequestEvent *ptr = (XResizeRequestEvent*)event;
       printf("JCG: ResizeRequest: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case SelectionClear:
    {
       XSelectionClearEvent *ptr = (XSelectionClearEvent*)event;
       printf("JCG: SelectionClear: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    case SelectionNotify:
    {
       XSelectionEvent *ptr = (XSelectionEvent*)event;
       printf("JCG: SelectionNotify: serial %ld, Requestor: %ld\n",ptr->serial,ptr->requestor);
    }
    break;

    case SelectionRequest:
    {
       XSelectionRequestEvent *ptr = (XSelectionRequestEvent*)event;
       printf("JCG: SelectionRequest: serial %ld, Owner: %ld, Requestor: %ld\n",ptr->serial,ptr->owner,ptr->requestor);
    }
    break;

    case VisibilityNotify:
    {
       XVisibilityEvent *ptr = (XVisibilityEvent*)event;
       printf("JCG: VisibilityNotify: serial %ld, Window: %ld\n",ptr->serial,ptr->window);
    }
    break;

    default:
    {
      printf("JCG: Unknown Event: %d\n",event->type);
    }
    break;
  }
#endif  //DBG_JCG
  return FALSE;
}

nsQtEventQueue::nsQtEventQueue(nsIEventQueue* EQueue)
{
#ifdef DBG_JCG
  mID = gQQueueID++;
  gQQueueCount++;
  printf("JCG nsQtEventQueue CTOR (%p) ID: %d, Count: %d\n",this,mID,gQQueueCount);
#endif
  mQSocket = nsnull;
  mEventQueue = EQueue;
  NS_IF_ADDREF(mEventQueue);
  mRefCnt = 1;

  mQSocket = new QSocketNotifier(mEventQueue->GetEventQueueSelectFD(),
                                 QSocketNotifier::Read,this);
  if (mQSocket)
    connect(mQSocket,SIGNAL(activated(int)),this,SLOT(DataReceived()));
}

nsQtEventQueue::~nsQtEventQueue()
{
#ifdef DBG_JCG
  gQQueueCount--;
  printf("JCG nsQtEventQueue DTOR (%p) ID: %d, Count: %d\n",this,mID,gQQueueCount);
#endif
  if (mQSocket)
    delete mQSocket;
  NS_IF_RELEASE(mEventQueue);
}

unsigned long nsQtEventQueue::IncRefCnt()
{
  return ++mRefCnt;
}

unsigned long nsQtEventQueue::DecRefCnt()
{
  return --mRefCnt;
}

void nsQtEventQueue::DataReceived()
{
  if (mEventQueue)
    mEventQueue->ProcessPendingEvents();
}
