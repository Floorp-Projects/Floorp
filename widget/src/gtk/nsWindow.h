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

#ifdef USE_SUPERWIN
#include "gtkmozarea.h"
#include "gdksuperwin.h"
#endif // USE_SUPERWIN 

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

  NS_IMETHOD           WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);

  NS_IMETHOD           PreCreateWidget(nsWidgetInitData *aWidgetInitData);

  virtual void*        GetNativeData(PRUint32 aDataType);

  NS_IMETHOD           Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD           ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy);

  NS_IMETHOD           SetTitle(const nsString& aTitle);
  NS_IMETHOD           Show(PRBool aShow);
  NS_IMETHOD           CaptureMouse(PRBool aCapture);

  NS_IMETHOD           Move(PRInt32 aX, PRInt32 aY);

  NS_IMETHOD           Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD           Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                              PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD           BeginResizingChildren(void);
  NS_IMETHOD           EndResizingChildren(void);
  NS_IMETHOD           Destroy(void);

#ifdef USE_SUPERWIN
  NS_IMETHOD           GetAbsoluteBounds(nsRect &aRect);
  NS_IMETHOD           CaptureRollupEvents(nsIRollupListener * aListener,
                                           PRBool aDoCapture,
                                           PRBool aConsumeRollupEvent);
  NS_IMETHOD           Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD           Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD           InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous);
  NS_IMETHOD           SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD           SetCursor(nsCursor aCursor);
  NS_IMETHOD           SetFocus(void);
  void                 QueueDraw();
  void                 UnqueueDraw();
  void                 DoPaint(PRInt32 x, PRInt32 y, PRInt32 width, PRInt32 height,
                               nsIRegion *aClipRegion);
  static gboolean      UpdateIdle (gpointer data);
  // get the toplevel window for this widget
  virtual GtkWindow   *GetTopLevelWindow(void);
  NS_IMETHOD           Update(void);
  virtual void         OnFocusInSignal(GdkEventFocus * aGdkFocusEvent);
  virtual void         OnFocusOutSignal(GdkEventFocus * aGdkFocusEvent);
  virtual void         InstallFocusInSignal(GtkWidget * aWidget);
  virtual void         InstallFocusOutSignal(GtkWidget * aWidget);
  void                 HandleGDKEvent(GdkEvent *event);
#endif /* USE_SUPERWIN */

  gint                 ConvertBorderStyles(nsBorderStyle bs);

  // Add an XATOM property to this window.
  void                 StoreProperty(char *property, unsigned char *data);

  virtual PRBool IsChild() const;

  // Utility methods
  virtual  PRBool OnExpose(nsPaintEvent &event);
  virtual  PRBool OnDraw(nsPaintEvent &event);

  PRBool   OnKey(nsKeyEvent &aEvent);
  virtual  PRBool OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
  // in nsWidget now
  //    virtual  PRBool OnResize(nsSizeEvent &aEvent);
  
  static void SuperWinFilter(GdkSuperWin *superwin, XEvent *event, gpointer p);
  
  void HandleXlibExposeEvent(XEvent *event);
  void HandleXlibConfigureNotifyEvent(XEvent *event);
  void HandleXlibButtonEvent(XButtonEvent *aButtonEvent);
  void HandleXlibMotionNotifyEvent(XMotionEvent *aMotionEvent);
  void HandleXlibCrossingEvent(XCrossingEvent * aCrossingEvent);
 
  // Return the GtkMozArea that is the nearest parent of this widget
  GtkWidget *GetMozArea();

  // Return the Gdk window used for rendering
  virtual GdkWindow * GetRenderWindow(GtkObject * aGtkWidget);

  // grab in progress
  PRBool GrabInProgress(void);
  //  XXX Chris - fix these
  //  virtual void OnButtonPressSignal(GdkEventButton * aGdkButtonEvent);

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
  virtual void OnRealize(GtkWidget *aWidget);

  virtual void OnDestroySignal(GtkWidget* aGtkWidget);

  //////////////////////////////////////////////////////////////////////

  virtual void InitCallbacks(char * aName = nsnull);
  NS_IMETHOD CreateNative(GtkObject *parentWidget);

  nsIFontMetrics *mFontMetrics;
  PRBool      mVisible;
  PRBool      mDisplayed;
  PRBool      mIsTooSmall;

  // XXX Temporary, should not be caching the font
  nsFont *    mFont;

  // Resize event management
  nsRect mResizeRect;
  int    mResized;
  PRBool mLowerLeft;

  GtkWidget *mShell;  /* used for toplevel windows */
  GdkSuperWin *mSuperWin;
  GtkWidget   *mMozArea;
  GtkWidget   *mMozAreaClosestParent;

  nsIMenuBar *mMenuBar;
  // are we doing a grab?
  static PRBool    mIsGrabbing;
  static nsWindow *mGrabWindow;
private:
  nsresult     SetIcon(GdkPixmap *window_pixmap, 
                       GdkBitmap *window_mask);
  nsresult     SetIcon();
  PRBool       mIsUpdating;
  // this is the current GdkSuperWin with the focus
  static nsWindow  *focusWindow;
  // when this is PR_TRUE we will block focus
  // events to prevent recursion
  PRBool       mBlockFocusEvents;
  PRInt32      mScrollExposeCounter;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
public:
  ChildWindow();
  ~ChildWindow();
  virtual PRBool IsChild() const;
#ifndef USE_SUPERWIN
  NS_IMETHOD Destroy(void);
#endif
};

#endif // Window_h__
