/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef Window_h__
#define Window_h__

#include "nsBaseWidget.h"
#include "nsToolkit.h"
#include "nsWidget.h"
#include "nsIMenuBar.h"
#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsStringUtil.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include <Pt.h>

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

  // nsIsupports
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  NS_IMETHOD            QueryInterface(const nsIID& aIID, void** aInstancePtr);
  
  virtual void          ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);
  NS_IMETHOD            PreCreateWidget(nsWidgetInitData *aWidgetInitData);
  virtual void*         GetNativeData(PRUint32 aDataType);
  NS_IMETHOD            SetColorMap(nsColorMap *aColorMap);
  NS_IMETHOD            Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD            SetTitle(const nsString& aTitle);
  NS_IMETHOD            SetMenuBar(nsIMenuBar * aMenuBar);
  NS_IMETHOD            ShowMenuBar(PRBool aShow);
  NS_IMETHOD            GetClientBounds( nsRect &aRect );
  NS_IMETHOD            Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
//  NS_IMETHOD            Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
//		                      PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD            SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);
  NS_IMETHOD            UpdateTooltips(nsRect* aNewTips[]);
  NS_IMETHOD            RemoveTooltips();
  NS_IMETHOD            BeginResizingChildren(void);
  NS_IMETHOD            EndResizingChildren(void);
  virtual PRBool        IsChild() { return(PR_FALSE); };
  virtual void          SetIsDestroying( PRBool val) { mIsDestroying = val; };
  virtual int           GetMenuBarHeight();
//  void                  NativePaint( PhRect_t &extent );
  NS_IMETHOD            Destroy(void);

  /* Add this because of bug 11088 */
  virtual NS_IMETHOD    Move(PRInt32 aX, PRInt32 aY);

  // Utility methods

  virtual PRBool        OnPaint(nsPaintEvent &event);
  PRBool                OnKey(nsKeyEvent &aEvent);
  PRBool                DispatchFocus(nsGUIEvent &aEvent);
  virtual PRBool        OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
  
protected:

  static void           RawDrawFunc( PtWidget_t *pWidget, PhTile_t *damage );

  NS_IMETHOD            CreateNative(PtWidget_t *parentWidget);
  static int            ResizeHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int            WindowCloseHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  PRBool                HandleEvent( PtCallbackInfo_t* aCbInfo );
  void                  ScreenToWidget( PhPoint_t &pt );
  NS_METHOD             GetSiblingClippedRegion( PhTile_t **btiles, PhTile_t **ctiles );
  NS_METHOD             SetWindowClipping( PhTile_t *damage, PhPoint_t &offset );
//  void                  StartResizeHoldOff( PtWidget_t *top );
  void                  ResizeHoldOff();
  void                  RemoveResizeWidget();
  static int            ResizeWorkProc( void *data );

  PtWidget_t            *mClientWidget;
  nsIFontMetrics        *mFontMetrics;
  PRBool                mClipChildren;
  PRBool                mClipSiblings;
//  nsWindowType          mWindowType;
//  nsBorderStyle         mBorderStyle;
  static PRBool         mResizeQueueInited;
  PRBool                mIsResizing;
  nsFont                *mFont;
  nsIMenuBar            *mMenuBar;
  PRBool                mMenuBarVis;
  int                   mFrameLeft;
  int                   mFrameRight;
  int                   mFrameTop;
  int                   mFrameBottom;

  static DamageQueueEntry *mResizeQueue;
  static PtWorkProcId_t *mResizeProcID;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
  public:
    ChildWindow();
    virtual PRBool IsChild() { return(PR_TRUE); };
};


#endif // Window_h__
