/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 *   Roland.Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsIAppShell.h"
#include "nsIEventQueueService.h"
#include "nsWidget.h"
#include "prtime.h"
#include "xlibrgb.h"

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
  /* |xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE)| would be the official 
   * way - but |nsAppShell::GetXlibRgbHandle()| one is little bit faster... :-)
   */
  static XlibRgbHandle *GetXlibRgbHandle() { return mXlib_rgb_handle; }
  static Display * mDisplay;
 private:
  static XlibRgbHandle *mXlib_rgb_handle;
  int                   xlib_fd;
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
  static PRBool mDragging;
  static PRBool mAltDown;
  static PRBool mShiftDown;
  static PRBool mCtrlDown;
  static PRBool mMetaDown;


protected:
  nsIEventQueueService * mEventQueueService;
  nsIEventQueue *mEventQueue;
  Screen *mScreen;
};

#endif // nsAppShell_h__
