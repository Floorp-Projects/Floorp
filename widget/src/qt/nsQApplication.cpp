/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 */

#include "nsQApplication.h"

#ifdef DEBUG
/* Required for x11EventFilter debugging hook */
#include "X11/Xlib.h"
#endif

nsQApplication *nsQApplication::mInstance = nsnull;
QIntDict<nsQtEventQueue> nsQApplication::mQueueDict;
PRUint32 nsQApplication::mRefCnt = 0;

nsQApplication *nsQApplication::Instance(int argc, char ** argv)
{
  if (!mInstance)
    mInstance = new nsQApplication(argc,argv);

  mRefCnt++;
  return mInstance;
}

nsQApplication *nsQApplication::Instance(Display * display)
{
  if (!mInstance)
    mInstance = new nsQApplication(display);

  mRefCnt++;
  return mInstance;
}

void nsQApplication::Release()
{
  mRefCnt--;
  if (mRefCnt <= 0)
    delete mInstance;
}

nsQApplication::nsQApplication(int argc, char ** argv)
	: QApplication(argc, argv)
{
   if (mInstance)
     NS_ASSERTION(mInstance == nsnull,"Attempt to create duplicate QApplication Object.");
   mInstance = this;
   setGlobalMouseTracking(true);
}

nsQApplication::nsQApplication(Display * display)
	: QApplication(display)
{
   if (mInstance)
     NS_ASSERTION(mInstance == nsnull,"Attempt to create duplicate QApplication Object.");
   mInstance = this;
   setGlobalMouseTracking(true);
}

nsQApplication::~nsQApplication()
{
    setGlobalMouseTracking(false);
}

void nsQApplication::AddEventProcessorCallback(nsIEventQueue * EQueue)
{
  nsQtEventQueue *que = nsnull;

  if ((que = mQueueDict.find(EQueue->GetEventQueueSelectFD()))) {
    que->IncRefCnt();
  }
  else {
     mQueueDict.insert(EQueue->GetEventQueueSelectFD(),new nsQtEventQueue(EQueue));
  }
}

void nsQApplication::RemoveEventProcessorCallback(nsIEventQueue * EQueue)
{
  nsQtEventQueue *que = nsnull;

  if ((que = mQueueDict.find(EQueue->GetEventQueueSelectFD()))) {
     que->DataReceived();
     if (que->DecRefCnt() <= 0) {
       mQueueDict.take(EQueue->GetEventQueueSelectFD());
       delete que;
     }
  }
}

#ifdef DEBUG
/* Hook for capturing X11 Events before they are processed by Qt */
bool nsQApplication::x11EventFilter(XEvent *event)
{
  if (event->type == ButtonPress || event->type == ButtonRelease) {
    int i;

    /* Break here to see Mouse Button Events */
    i = 0;
  }
  return FALSE;
}
#endif

nsQtEventQueue::nsQtEventQueue(nsIEventQueue * EQueue)
{
    mQSocket = nsnull;
    mEventQueue = EQueue;
    NS_IF_ADDREF(mEventQueue);
    mRefCnt = 1;

    mQSocket = new QSocketNotifier(mEventQueue->GetEventQueueSelectFD(),
                                   QSocketNotifier::Read,this);
    if (mQSocket)
      connect(mQSocket, SIGNAL(activated(int)), this, SLOT(DataReceived()));
}

nsQtEventQueue::~nsQtEventQueue()
{
    if (mQSocket)
      delete mQSocket;
    NS_IF_RELEASE(mEventQueue);
}

unsigned long nsQtEventQueue::IncRefCnt()
{
  return --mRefCnt;
}

unsigned long nsQtEventQueue::DecRefCnt()
{
  return ++mRefCnt;
}

void nsQtEventQueue::DataReceived()
{
    if (mEventQueue)
        mEventQueue->ProcessPendingEvents();
}
