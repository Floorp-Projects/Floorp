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

#include "gtkmozarea.h"
#include "gdksuperwin.h"

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

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD           WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);

  NS_IMETHOD           PreCreateWidget(nsWidgetInitData *aWidgetInitData);

  virtual void*        GetNativeData(PRUint32 aDataType);

  NS_IMETHOD           Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD           ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD           ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy);

  NS_IMETHOD           SetTitle(const nsString& aTitle);
  NS_IMETHOD           Show(PRBool aShow);
  NS_IMETHOD           CaptureMouse(PRBool aCapture);

  NS_IMETHOD           ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD           Move(PRInt32 aX, PRInt32 aY);

  NS_IMETHOD           Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD           Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                              PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD           BeginResizingChildren(void);
  NS_IMETHOD           EndResizingChildren(void);

  NS_IMETHOD           GetScreenBounds(nsRect &aRect);

  NS_IMETHOD           CaptureRollupEvents(nsIRollupListener * aListener,
                                           PRBool aDoCapture,
                                           PRBool aConsumeRollupEvent);
  NS_IMETHOD           Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD           Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD           InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous);
  NS_IMETHOD           SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD           SetCursor(nsCursor aCursor);
  NS_IMETHOD           SetFocus(void);
  NS_IMETHOD           GetAttention(void);
  NS_IMETHOD           Destroy();

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

  gint                 ConvertBorderStyles(nsBorderStyle bs);

  // Add an XATOM property to this window.
  void                 StoreProperty(char *property, unsigned char *data);
  void                 InvalidateWindowPos(void);


  virtual PRBool IsChild() const;

  // Utility methods
  virtual  PRBool OnExpose(nsPaintEvent &event);
  virtual  PRBool OnDraw(nsPaintEvent &event);

  virtual  PRBool OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
  // in nsWidget now
  //    virtual  PRBool OnResize(nsSizeEvent &aEvent);
  
  static void SuperWinFilter(GdkSuperWin *superwin, XEvent *event, gpointer p);
  
  void HandleXlibConfigureNotifyEvent(XEvent *event);
  // Return the GtkMozArea that is the nearest parent of this widget
  GtkWidget *GetMozArea();

  // Return the Gdk window used for rendering
  virtual GdkWindow * GetRenderWindow(GtkObject * aGtkWidget);

  // this is the "native" destroy code that will destroy any
  // native windows / widgets for this logical widget
  virtual void DestroyNative(void);

  // grab in progress
  PRBool GrabInProgress(void);
  //  XXX Chris - fix these
  //  virtual void OnButtonPressSignal(GdkEventButton * aGdkButtonEvent);
  void ShowCrossAtLocation(guint x, guint y);

  // this will get the nsWindow with the grab.  this will AddRef()
  // before returning.
  static nsWindow *GetGrabWindow(void);
  
  virtual void DispatchSetFocusEvent(void);
  virtual void DispatchLostFocusEvent(void);
  virtual void DispatchActivateEvent(void);
  virtual void DispatchDeactivateEvent(void);

  PRBool mBlockMozAreaFocusIn;

  void HandleMozAreaFocusIn(void);
  void HandleMozAreaFocusOut(void);
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

  PRBool      mVisible;
  PRBool      mDisplayed;
  PRBool      mIsTooSmall;

  // Resize event management
  nsRect mResizeRect;
  int    mResized;
  PRBool mLowerLeft;

  GtkWidget *mShell;  /* used for toplevel windows */
  GdkSuperWin *mSuperWin;
  GtkWidget   *mMozArea;
  GtkWidget   *mMozAreaClosestParent;

  PRBool      GetWindowPos(nscoord &x, nscoord &y);
  nscoord     mCachedX, mCachedY; 

  // are we doing a grab?
  static PRBool      sIsGrabbing;
  static nsWindow   *sGrabWindow;

  // our wonderful hash table with our window -> nsWindow * lookup
  static GHashTable *mWindowLookupTable;

  // this will query all of it's children trying to find a child
  // that has a containing rectangle and return it.
  static Window     GetInnerMostWindow(Window aOriginWindow,
                                       Window aWindow,
                                       nscoord x, nscoord y,
                                       nscoord *retx, nscoord *rety,
                                       int depth);
  // given an X window this will find the nsWindow * for it.
  static nsWindow  *GetnsWindowFromXWindow(Window aWindow);

  // all of our DND stuff

  // this is the last window that had a drag event happen on it.
  static nsWindow  *mLastDragMotionWindow;
  static nsWindow  *mLastLeaveWindow;

  // DragBegin not needed ?
  // always returns TRUE
  static gint DragMotionSignal (GtkWidget *      aWidget,
                                GdkDragContext   *aDragContext,
                                gint             aX,
                                gint             aY,
                                guint            aTime,
                                void             *aData);
  gint OnDragMotionSignal      (GtkWidget *      aWidget,
                                GdkDragContext   *aDragContext,
                                gint             aX,
                                gint             aY,
                                guint            aTime,
                                void             *aData);

  static void DragLeaveSignal  (GtkWidget *      aWidget,
                                GdkDragContext   *aDragContext,
                                guint            aTime,
                                gpointer         aData);
  void OnDragLeaveSignal       (GtkWidget *      aWidget,
                                GdkDragContext   *aDragContext,
                                guint            aTime,
                                gpointer         aData);

  // always returns TRUE
  static gint DragDropSignal   (GtkWidget        *aWidget,
                                GdkDragContext   *aDragContext,
                                gint             aX,
                                gint             aY,
                                guint            aTime,
                                void             *aData);
  gint OnDragDropSignal        (GtkWidget        *aWidget,
                                GdkDragContext   *aDragContext,
                                gint             aX,
                                gint             aY,
                                guint            aTime,
                                void             *aData);
  // when the data has been received
  static void DragDataReceived (GtkWidget         *aWidget,
                                GdkDragContext    *aDragContext,
                                gint               aX,
                                gint               aY,
                                GtkSelectionData  *aSelectionData,
                                guint              aInfo,
                                guint32            aTime,
                                gpointer           aData);
  void OnDragDataReceived      (GtkWidget         *aWidget,
                                GdkDragContext    *aDragContext,
                                gint               aX,
                                gint               aY,
                                GtkSelectionData  *aSelectionData,
                                guint              aInfo,
                                guint32            aTime,
                                gpointer           aData);

  // these are drag and drop events that aren't generated by widget
  // events but we do synthesize

  void OnDragLeave(void);
  void OnDragEnter(nscoord aX, nscoord aY);

  // this is everything we need to be able to fire motion events
  // repeatedly
  GtkWidget         *mDragMotionWidget;
  GdkDragContext    *mDragMotionContext;
  gint               mDragMotionX;
  gint               mDragMotionY;
  guint              mDragMotionTime;
  nsCOMPtr<nsITimer> mDragMotionTimer;

  void        ResetDragMotionTimer    (GtkWidget      *aWidget,
                                       GdkDragContext *aDragContext,
                                       gint           aX,
                                       gint           aY,
                                       guint          aTime);
  void        FireDragMotionTimer     (void);
  static void DragMotionTimerCallback (nsITimer *aTimer, void *aClosure);

#ifdef NS_DEBUG
  void        DumpWindowTree(void);
  static void dumpWindowChildren(Window aWindow, unsigned int depth);
#endif

private:
  nsresult     SetMiniIcon(GdkPixmap *window_pixmap,
                           GdkBitmap *window_mask);
  nsresult     SetIcon(GdkPixmap *window_pixmap, 
                       GdkBitmap *window_mask);
  nsresult     SetIcon();
  void         SendExposeEvent();
  PRBool       mLastGrabFailed;
  void         NativeGrab(PRBool aGrab);

  PRBool       mIsUpdating;

  void DestroyNativeChildren(void);

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
