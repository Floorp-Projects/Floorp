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
#ifndef nsWindow_h__
#define nsWindow_h__

#define USE_XIM
#ifdef  USE_XIM
#include <gdk/gdkprivate.h>
#endif

#include "nsISupports.h"

#include "nsWidget.h"

#include "nsString.h"

class nsFont;
class nsIAppShell;

/**
 * Native GTK++ window wrapper.
 */

class nsWindow : public nsWidget
{

public:
      // nsIWidget interface

  nsWindow();
  virtual ~nsWindow();

  NS_IMETHOD WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);
  NS_IMETHOD ScreenToWidget(const nsRect &aOldRect, nsRect &aNewRect);  

  virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

  NS_IMETHOD           PreCreateWidget(nsWidgetInitData *aWidgetInitData);

  virtual void*        GetNativeData(PRUint32 aDataType);

  NS_IMETHOD           SetColorMap(nsColorMap *aColorMap);
  NS_IMETHOD           Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);

  NS_IMETHOD           SetTitle(const nsString& aTitle);
  nsresult             SetIcon(GdkPixmap *window_pixmap, 
                               GdkBitmap *window_mask);
  NS_IMETHOD           SetMenuBar(nsIMenuBar * aMenuBar);
  NS_IMETHOD           Show(PRBool aShow);
  NS_IMETHOD           ShowMenuBar(PRBool aShow);
  NS_IMETHOD           CaptureMouse(PRBool aCapture);

  NS_IMETHOD           Move(PRInt32 aX, PRInt32 aY);

  NS_IMETHOD           Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD           Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                              PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD           Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD           Invalidate(const nsRect &aRect, PRBool aIsSynchronous);

  NS_IMETHOD           BeginResizingChildren(void);
  NS_IMETHOD           EndResizingChildren(void);
  NS_IMETHOD           Destroy(void);

  gint                 ConvertBorderStyles(nsBorderStyle bs);

  virtual PRBool IsChild() const;

  void SetIsDestroying(PRBool val) {
    mIsDestroyingWindow = val;
  }

  PRBool IsDestroying() const {
    return mIsDestroyingWindow;
  }

  // Utility methods
  virtual  PRBool OnPaint(nsPaintEvent &event);
  PRBool   OnKey(nsKeyEvent &aEvent);
  virtual  PRBool OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
  // in nsWidget now
  //    virtual  PRBool OnResize(nsSizeEvent &aEvent);

#ifdef        USE_XIM
  PRBool          mIMEEnable;
  PRUnichar*      mIMECompositionUniString;
  PRInt32         mIMECompositionUniStringSize;
  void SetXICSpotLocation(nsPoint aPoint);
#endif

protected:

  //////////////////////////////////////////////////////////////////////
  //
  // Draw signal
  // 
  //////////////////////////////////////////////////////////////////////
  void InitDrawEvent(GdkRectangle * aArea,
                     nsPaintEvent & aPaintEvent,
                     PRUint32       aEventType);

  void UninitDrawEvent(GdkRectangle * area,
                       nsPaintEvent & aPaintEvent,
                       PRUint32       aEventType);
  
  static gint DrawSignal(GtkWidget *    aWidget,
                         GdkRectangle * aArea,
                         gpointer       aData);

  virtual gint OnDrawSignal(GdkRectangle * aArea);
  virtual void OnRealize();

#ifdef  USE_XIM
  virtual void OnFocusInSignal(GdkEventFocus * aGdkFocusEvent);
  virtual void OnFocusOutSignal(GdkEventFocus * aGdkFocusEvent);
#endif /* USE_XIM */

  virtual void OnDestroySignal(GtkWidget* aGtkWidget);

  //////////////////////////////////////////////////////////////////////

  virtual void InitCallbacks(char * aName = nsnull);
  NS_IMETHOD CreateNative(GtkWidget *parentWidget);
  nsresult     SetIcon();


  nsIFontMetrics *mFontMetrics;
  PRBool      mVisible;
  PRBool      mDisplayed;
  PRBool      mIsDestroyingWindow;

  // XXX Temporary, should not be caching the font
  nsFont *    mFont;

  // Resize event management
  nsRect mResizeRect;
  int    mResized;
  PRBool mLowerLeft;

  GtkWidget *mShell;  /* used for toplevel windows */
  
  nsIMenuBar *mMenuBar;

#ifdef        USE_XIM
  GdkICPrivate* mIC;
  GdkICPrivate* GetXIC();
  void SetXIC(GdkICPrivate *aIC);
  void GetXYFromPosition(unsigned long *aX, unsigned long *aY);
#endif /* USE_XIM */
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
public:
  ChildWindow();
  ~ChildWindow();
  virtual PRBool IsChild() const;
  NS_IMETHOD Destroy(void);
};

#endif // Window_h__
