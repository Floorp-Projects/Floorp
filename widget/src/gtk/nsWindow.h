/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifndef nsWindow_h__
#define nsWindow_h__

#include "nsISupports.h"

#include "nsWidget.h"

#include "nsString.h"

#include "nsIDragService.h"

#include "gtkmozarea.h"
#include "gdksuperwin.h"

#ifdef USE_XIM
#include "pldhash.h"
class nsIMEGtkIC;
class nsIMEPreedit;
#endif // USE_XIM

class nsFont;
class nsIAppShell;

/**
 * Native GTK+ window wrapper.
 */

class nsWindow : public nsWidget
{

public:
  // nsIWidget interface

  nsWindow();
  virtual ~nsWindow();

  static void ReleaseGlobals();

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD           WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);

  NS_IMETHOD           PreCreateWidget(nsWidgetInitData *aWidgetInitData);

  virtual void*        GetNativeData(PRUint32 aDataType);

  NS_IMETHOD           Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD           ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD           ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy);

  NS_IMETHOD           SetTitle(const nsAString& aTitle);
  NS_IMETHOD           Show(PRBool aShow);
  NS_IMETHOD           CaptureMouse(PRBool aCapture);

  NS_IMETHOD           ConstrainPosition(PRBool aAllowSlop,
                                         PRInt32 *aX, PRInt32 *aY);
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
  NS_IMETHOD           Validate();
  NS_IMETHOD           Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD           Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD           InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous);
  NS_IMETHOD           SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD           SetCursor(nsCursor aCursor);
  NS_IMETHOD           Enable (PRBool aState);
  NS_IMETHOD           IsEnabled (PRBool *aState);
  NS_IMETHOD           SetFocus(PRBool aRaise);
  NS_IMETHOD           GetAttention(PRInt32 aCycleCount);
  NS_IMETHOD           Destroy();
  void                 ResizeTransparencyBitmap(PRInt32 aNewWidth, PRInt32 aNewHeight);
  void                 ApplyTransparencyBitmap();
#ifdef MOZ_XUL
  NS_IMETHOD           SetWindowTranslucency(PRBool aTransparent);
  NS_IMETHOD           GetWindowTranslucency(PRBool& aTransparent);
  NS_IMETHOD           UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas);
  NS_IMETHOD           HideWindowChrome(PRBool aShouldHide);
  NS_IMETHOD           MakeFullScreen(PRBool aFullScreen);
#endif
  NS_IMETHOD           SetIcon(const nsAString& aIcon);
  GdkCursor           *GtkCreateCursor(nsCursor aCursorType);
  virtual void         LoseFocus(void);

  // nsIKBStateControl
  NS_IMETHOD           ResetInputState();

  void                 QueueDraw();
  void                 UnqueueDraw();
  void                 DoPaint(nsIRegion *aClipRegion);
  static gboolean      UpdateIdle (gpointer data);
  // get the toplevel window for this widget
  virtual GtkWindow   *GetTopLevelWindow(void);
  NS_IMETHOD           Update(void);
  virtual void         OnMotionNotifySignal(GdkEventMotion *aGdkMotionEvent);
  virtual void         OnEnterNotifySignal(GdkEventCrossing *aGdkCrossingEvent);
  virtual void         OnLeaveNotifySignal(GdkEventCrossing *aGdkCrossingEvent);
  virtual void         OnButtonPressSignal(GdkEventButton *aGdkButtonEvent);
  virtual void         OnButtonReleaseSignal(GdkEventButton *aGdkButtonEvent);
  virtual void         OnFocusInSignal(GdkEventFocus * aGdkFocusEvent);
  virtual void         OnFocusOutSignal(GdkEventFocus * aGdkFocusEvent);
  virtual void         InstallFocusInSignal(GtkWidget * aWidget);
  virtual void         InstallFocusOutSignal(GtkWidget * aWidget);
  void                 HandleGDKEvent(GdkEvent *event);

  gint                 ConvertBorderStyles(nsBorderStyle bs);

  void                 InvalidateWindowPos(void);


  // in nsWidget now
  //    virtual  PRBool OnResize(nsSizeEvent &aEvent);
  
  void HandleXlibConfigureNotifyEvent(XEvent *event);

  // Return the GtkMozArea that is the nearest parent of this widget
  virtual GtkWidget *GetOwningWidget();

  // Get the owning window for this window
  nsWindow *GetOwningWindow();

  // Return the type of the window that is the toplevel mozilla window
  // for this (possibly) inner window
  nsWindowType GetOwningWindowType();

  // Return the Gdk window used for rendering
  virtual GdkWindow * GetRenderWindow(GtkObject * aGtkWidget);

  // this is the "native" destroy code that will destroy any
  // native windows / widgets for this logical widget
  virtual void DestroyNative(void);

  // grab in progress
  PRBool GrabInProgress(void);
  // drag in progress
  static PRBool DragInProgress(void);

  //  XXX Chris - fix these
  //  virtual void OnButtonPressSignal(GdkEventButton * aGdkButtonEvent);

  // this will get the nsWindow with the grab.  The return will not be
  // addrefed.
  static nsWindow *GetGrabWindow(void);
  GdkWindow *GetGdkGrabWindow(void);
  
  virtual void DispatchSetFocusEvent(void);
  virtual void DispatchLostFocusEvent(void);
  virtual void DispatchActivateEvent(void);
  virtual void DispatchDeactivateEvent(void);

  PRBool mBlockMozAreaFocusIn;

  void HandleMozAreaFocusIn(void);
  void HandleMozAreaFocusOut(void);

  // Window icon cache
  static PRBool IconEntryMatches(PLDHashTable* aTable,
                                 const PLDHashEntryHdr* aHdr,
                                 const void* aKey);
  static void ClearIconEntry(PLDHashTable* aTable, PLDHashEntryHdr* aHdr);

protected:
  virtual void ResetInternalVisibility();
  virtual void SetInternalVisibility(PRBool aVisible);

  virtual void OnRealize(GtkWidget *aWidget);

  virtual void OnDestroySignal(GtkWidget* aGtkWidget);

  //////////////////////////////////////////////////////////////////////

  virtual void InitCallbacks(char * aName = nsnull);
  NS_IMETHOD CreateNative(GtkObject *parentWidget);

  PRBool      mIsTooSmall;

  GtkWidget *mShell;  /* used for toplevel windows */
  GdkSuperWin *mSuperWin;
  GtkWidget   *mMozArea;
  GtkWidget   *mMozAreaClosestParent;

  PRBool      GetWindowPos(nscoord &x, nscoord &y);
  nscoord     mCachedX, mCachedY; 

  // are we doing a grab?
  static PRBool      sIsGrabbing;
  static nsWindow   *sGrabWindow;
  static PRBool      sIsDraggingOutOf;

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

  static gint ClientEventSignal(GtkWidget* widget, GdkEventClient* event, void* data);
  virtual void ThemeChanged();

  // all of our DND stuff

  // this is the last window that had a drag event happen on it.
  static nsWindow  *mLastDragMotionWindow;
  static GdkCursor *gsGtkCursorCache[eCursorCount];

  void   InitDragEvent(nsMouseEvent &aEvent);
  void   UpdateDragStatus(nsMouseEvent &aEvent,
                          GdkDragContext *aDragContext,
                          nsIDragService *aDragService);

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
  guint              mDragMotionTimerID;
  nsCOMPtr<nsITimer> mDragLeaveTimer;

  void        ResetDragMotionTimer    (GtkWidget      *aWidget,
                                       GdkDragContext *aDragContext,
                                       gint           aX,
                                       gint           aY,
                                       guint          aTime);
  void        FireDragMotionTimer     (void);
  void        FireDragLeaveTimer      (void);
  static guint DragMotionTimerCallback (gpointer aClosure);
  static void  DragLeaveTimerCallback  (nsITimer *aTimer, void *aClosure);

  // Hash table for icon spec -> icon set lookup
  static PLDHashTable* sIconCache;

#ifdef USE_XIM
protected:
  static GdkFont      *gPreeditFontset;
  static GdkFont      *gStatusFontset;
  static GdkIMStyle   gInputStyle;
  static PLDHashTable gXICLookupTable;
  PRPackedBool        mIMEEnable;
  PRPackedBool        mIMECallComposeStart;
  PRPackedBool        mIMECallComposeEnd;
  PRPackedBool        mIMEIsBeingActivate;
  nsWindow*           mIMEShellWindow;
  void                SetXICSpotLocation(nsIMEGtkIC* aXIC, nsPoint aPoint);
  void                SetXICBaseFontSize(nsIMEGtkIC* aXIC, int height);
  nsCOMPtr<nsITimer>  mICSpotTimer;
  static void         ICSpotCallback(nsITimer* aTimer, void* aClosure);
  nsresult            KillICSpotTimer();
  nsresult            PrimeICSpotTimer();
  nsresult            UpdateICSpot(nsIMEGtkIC* aXIC);
  int                 mXICFontSize;

public:
  nsIMEGtkIC*        IMEGetInputContext(PRBool aCreate);

  void        ime_preedit_start();
  void        ime_preedit_draw(nsIMEGtkIC* aXIC);
  void        ime_preedit_done();
  void        ime_status_draw();

  void         IMEUnsetFocusWindow();
  void         IMESetFocusWindow();
  void         IMEGetShellWindow();
  void         IMEDestroyIC();
  void         IMEBeingActivate(PRBool aActive);
#endif // USE_XIM 

protected:
  void              IMEComposeStart(guint aTime);
  void              IMEComposeText(GdkEventKey*,
                             const PRUnichar *aText,
                             const PRInt32 aLen,
                             const char *aFeedback);
  void              IMEComposeEnd(guint aTime);

public:
  virtual void IMECommitEvent(GdkEventKey *aEvent);

private:
  PRUnichar*   mIMECompositionUniString;
  PRInt32      mIMECompositionUniStringSize;

  nsresult     SetMiniIcon(GdkPixmap *window_pixmap,
                           GdkBitmap *window_mask);
  nsresult     SetIcon(GdkPixmap *window_pixmap, 
                       GdkBitmap *window_mask);

  void         NativeGrab(PRBool aGrab);

  PRPackedBool mLastGrabFailed;
  PRPackedBool mIsUpdating;
  PRPackedBool mLeavePending;
  PRPackedBool mRestoreFocus;

  PRPackedBool mIsTranslucent;
  // This bitmap tracks which pixels are transparent. We don't support
  // full translucency at this time; each pixel is either fully opaque
  // or fully transparent.
  gchar*       mTransparencyBitmap;

  void DestroyNativeChildren(void);

  GtkWindow *mTransientParent;

};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
public:
  ChildWindow();
  ~ChildWindow();
};

#endif // Window_h__
