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
 */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsIAppShell.h"
#include "nsIEventQueueService.h"
#include "prtime.h"

class nsAppShell : public nsIAppShell
{
  public:
  nsAppShell(); 
  virtual ~nsAppShell();

  NS_DECL_ISUPPORTS

  // nsIAppShellInterface
  
  NS_IMETHOD            Create(int* argc, char ** argv);
  virtual nsresult      Run(); 
  NS_IMETHOD            Spinup();
  NS_IMETHOD            Spindown();
  NS_IMETHOD            ListenToEventQueue(nsIEventQueue *aQueue, PRBool aListen)
                          { return NS_OK; }
  NS_IMETHOD            GetNativeEvent(PRBool &aRealEvent, void *&aEvent);
  NS_IMETHOD            DispatchNativeEvent(PRBool aRealEvent, void * aEvent);
  
  NS_IMETHOD            SetDispatchListener(nsDispatchListener* aDispatchListener);
  NS_IMETHOD            Exit();
  virtual void *        GetNativeData(PRUint32 aDataType);
  static void           DispatchXEvent(XEvent *event);
 private:
  nsDispatchListener*     mDispatchListener;
  static void HandleButtonEvent(XEvent *event, nsWidget *aWidget);
  static void HandleMotionNotifyEvent(XEvent *event, nsWidget *aWidget);
  static void HandleExposeEvent(XEvent *event, nsWidget *aWidget);
  static void HandleConfigureNotifyEvent(XEvent *event, nsWidget *aWidget);
  static void HandleKeyPressEvent(XEvent *event, nsWidget *aWidget);
  static void HandleKeyReleaseEvent(XEvent *event, nsWidget *aWidget);
  static void HandleFocusInEvent(XEvent *event, nsWidget *aWidget);
  static void HandleFocusOutEvent(XEvent *event, nsWidget *aWidget);
  static void HandleVisibilityNotifyEvent(XEvent *event, nsWidget *aWidget);
  static void HandleMapNotifyEvent(XEvent *event, nsWidget *aWidget);
  static void HandleUnmapNotifyEvent(XEvent *event, nsWidget *aWidget);
  static void HandleEnterEvent(XEvent *event, nsWidget *aWidget);
  static void HandleLeaveEvent(XEvent *event, nsWidget *aWidget);
  static void HandleClientMessageEvent(XEvent *event, nsWidget *aWidget);
  static void HandleSelectionRequestEvent(XEvent *event, nsWidget *aWidget);
  static void HandleDragMotionEvent(XEvent *event, nsWidget *aWidget);
  static void HandleDragEnterEvent(XEvent *event, nsWidget *aWidget);
  static void HandleDragLeaveEvent(XEvent *event, nsWidget *aWidget);
  static void HandleDragDropEvent(XEvent *event, nsWidget *aWidget);
  static void ForwardEvent(XEvent *event, nsWidget *aWidget);
  static PRBool DieAppShellDie;
  static PRBool mClicked;
  static PRTime mClickTime;
  static PRInt16 mClicks;
  static PRUint16 mClickedButton;
  static Display * mDisplay;
  static PRBool mDragging;
  static PRBool mAltDown;
  static PRBool mShiftDown;
  static PRBool mCtrlDown;
  static PRBool mMetaDown;

protected:
  nsIEventQueueService * mEventQueueService;
  nsIEventQueue *mEventQueue;


  Screen *  mScreen;
};

#endif // nsAppShell_h__
