/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsWindow_h__
#define nsWindow_h__



#include "nsISupports.h"

#include "nsWidget.h"

#include "nsString.h"

#include <Pt.h>
#include <Ap.h>

class nsFont;
class nsIAppShell;

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

/**
 * Native Photon window wrapper. 
 */

class nsWindow : public nsWidget
{

public:
  // nsIWidget interface

  nsWindow();
  virtual ~nsWindow();

  NS_IMETHOD           WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);

  NS_IMETHOD           PreCreateWidget(nsWidgetInitData *aWidgetInitData);

  virtual void*        GetNativeData(PRUint32 aDataType);

  NS_IMETHOD           Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD           ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD           ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy);

  NS_IMETHOD           SetTitle(const nsString& aTitle);
  NS_IMETHOD           Show(PRBool state);
  NS_IMETHOD           CaptureMouse(PRBool aCapture);
 
  NS_IMETHOD           ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD           Move(PRInt32 aX, PRInt32 aY);

  NS_IMETHOD           Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD           Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                              PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD           BeginResizingChildren(void);
  NS_IMETHOD           EndResizingChildren(void);

  NS_IMETHOD           CaptureRollupEvents(nsIRollupListener * aListener,
                                           PRBool aDoCapture,
                                           PRBool aConsumeRollupEvent);
  NS_IMETHOD           Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD           Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD           InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous);
  NS_IMETHOD           SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD           SetFocus(void);
  NS_IMETHOD           GetAttention(void);

  NS_IMETHOD           Update(void);
  
  virtual PRBool       IsChild() const;


  // Utility methods
  virtual PRBool        OnPaint(nsPaintEvent &event);
  PRBool                OnKey(nsKeyEvent &aEvent);
  PRBool                DispatchFocus(nsGUIEvent &aEvent);
  virtual PRBool        OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
  NS_IMETHOD            SetColorMap(nsColorMap *aColorMap);
  NS_IMETHOD            GetClientBounds( nsRect &aRect );
  NS_IMETHOD            SetModal(PRBool aModal);
  void                  ScreenToWidget( PhPoint_t &pt );

 // Native draw function... like doPaint()
 static void            RawDrawFunc( PtWidget_t *pWidget, PhTile_t *damage );

 nsIRegion              *GetRegion();

protected:
  // this is the "native" destroy code that will destroy any
  // native windows / widgets for this logical widget
  virtual void          DestroyNative(void);
  void                  DestroyNativeChildren(void);

  // grab in progress
  PRBool GrabInProgress(void);
  
  static int            MenuRegionCallback(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo);  
  static int            PopupMenuRegionCallback(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo);  

  NS_IMETHOD            CreateNative(PtWidget_t *parentWidget);

  static int            ResizeHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int            WindowCloseHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  PRBool                HandleEvent( PtCallbackInfo_t* aCbInfo );
  PhTile_t              *GetWindowClipping( );

  void                  ResizeHoldOff();
  void                  RemoveResizeWidget();
  static int            ResizeWorkProc( void *data );
  NS_IMETHOD            ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                         PRBool *aForWindow);

  PtWidget_t            *mClientWidget;
  PtWidget_t            *mShell;         /* used for toplevel windows */
  PtWidget_t            *mMenuRegion;
  
  PRBool                mIsDestroyingWindow;

  nsIFontMetrics        *mFontMetrics;
  PRBool                mVisible;
  PRBool                mDisplayed;
  PRBool                mIsTooSmall;
  PRBool                mClipChildren;
  PRBool                mClipSiblings;
  static PRBool         mResizeQueueInited;
  PRBool                mIsResizing;
  nsFont                *mFont;
  nsIMenuBar            *mMenuBar;
  PRBool                mMenuBarVis;
  PRBool                mIsUpdating;

  // when this is PR_TRUE we will block focus
  // events to prevent recursion
  PRBool                mBlockFocusEvents;
  
  static DamageQueueEntry *mResizeQueue;
  static PtWorkProcId_t *mResizeProcID;

  // are we doing a grab?
  static PRBool      mIsGrabbing;
  static nsWindow   *mGrabWindow;

  // this is the last window that had a drag event happen on it.
  static nsWindow  *mLastDragMotionWindow;
  static nsWindow  *mLastLeaveWindow;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
  public:
    ChildWindow();
    ~ChildWindow();
	virtual PRBool IsChild() const;
};


#endif // Window_h__
