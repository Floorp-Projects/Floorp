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

#ifndef nsWidget_h__
#define nsWidget_h__

#include "nsBaseWidget.h"
#include "nsWeakReference.h"
#include "nsIKBStateControl.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"

class nsILookAndFeel;
class nsIAppShell;
class nsIToolkit;

#include <gtk/gtk.h>

#include <gdk/gdkprivate.h>

#include "gtkmozbox.h"
#include "nsITimer.h"

#define NSRECT_TO_GDKRECT(ns,gdk) \
  PR_BEGIN_MACRO \
  gdk.x = ns.x; \
  gdk.y = ns.y; \
  gdk.width = ns.width; \
  gdk.height = ns.height; \
  PR_END_MACRO

#define NSCOLOR_TO_GDKCOLOR(n,g) \
  PR_BEGIN_MACRO \
  g.red = 256 * NS_GET_R(n); \
  g.green = 256 * NS_GET_G(n); \
  g.blue = 256 * NS_GET_B(n); \
  PR_END_MACRO


/**
 * Base of all GTK+ native widgets.
 */

class nsWidget : public nsBaseWidget, public nsIKBStateControl, public nsSupportsWeakReference
{
public:
  nsWidget();
  virtual ~nsWidget();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIWidget

  NS_IMETHOD            Create(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell = nsnull,
                               nsIToolkit *aToolkit = nsnull,
                               nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD            Create(nsNativeWidget aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell = nsnull,
                               nsIToolkit *aToolkit = nsnull,
                               nsWidgetInitData *aInitData = nsnull);

  NS_IMETHOD Destroy(void);
  nsIWidget* GetParent(void);
  virtual void OnDestroy();

  NS_IMETHOD Show(PRBool state);
  NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);
  NS_IMETHOD IsVisible(PRBool &aState);

  NS_IMETHOD ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                    PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD Enable(PRBool aState);
  NS_IMETHOD IsEnabled(PRBool *aState);
  NS_IMETHOD SetFocus(PRBool aRaise);

  virtual void LoseFocus(void);

  PRBool OnResize(nsSizeEvent *event);
  virtual PRBool OnResize(nsRect &aRect);
  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);

  nsIFontMetrics *GetFont(void);
  NS_IMETHOD SetFont(const nsFont &aFont);

  NS_IMETHOD SetBackgroundColor(const nscolor &aColor);

  NS_IMETHOD SetCursor(nsCursor aCursor);

  NS_IMETHOD SetColorMap(nsColorMap *aColorMap);

  void* GetNativeData(PRUint32 aDataType);

  NS_IMETHOD WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);
  NS_IMETHOD ScreenToWidget(const nsRect &aOldRect, nsRect &aNewRect);

  NS_IMETHOD BeginResizingChildren(void);
  NS_IMETHOD EndResizingChildren(void);

  NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
  NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

  // Use this to set the name of a widget for normal widgets.. not the same as the nsWindow version
  NS_IMETHOD SetTitle(const nsAString& aTitle);


  virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

  // the following are nsWindow specific, and just stubbed here
  NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) { return NS_ERROR_FAILURE; }
  NS_IMETHOD SetMenuBar(nsIMenuBar *aMenuBar) { return NS_ERROR_FAILURE; }
  NS_IMETHOD ShowMenuBar(PRBool aShow) { return NS_ERROR_FAILURE; }
  // *could* be done on a widget, but that would be silly wouldn't it?
  NS_IMETHOD CaptureMouse(PRBool aCapture) { return NS_ERROR_FAILURE; }

  NS_IMETHOD GetWindowClass(char *aClass);
  NS_IMETHOD SetWindowClass(char *aClass);

  NS_IMETHOD Validate();
  NS_IMETHOD Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);
  NS_IMETHOD Update(void);
  NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);


  // nsIKBStateControl
  NS_IMETHOD ResetInputState();
  NS_IMETHOD SetIMEOpenState(PRBool aState);
  NS_IMETHOD GetIMEOpenState(PRBool* aState);

  void InitEvent(nsGUIEvent& event, nsPoint* aPoint = nsnull);
    
  // Utility functions

  PRBool     ConvertStatus(nsEventStatus aStatus);
  PRBool     DispatchMouseEvent(nsMouseEvent& aEvent);
  PRBool     DispatchStandardEvent(PRUint32 aMsg);
  PRBool     DispatchFocus(nsGUIEvent &aEvent);

  // are we a "top level" widget?
  PRBool     mIsToplevel;

  virtual void DispatchSetFocusEvent(void);
  virtual void DispatchLostFocusEvent(void);
  virtual void DispatchActivateEvent(void);
  virtual void DispatchDeactivateEvent(void);

#ifdef DEBUG
  void IndentByDepth(FILE* out);
#endif

  // Return the Gdk window used for rendering
  virtual GdkWindow * GetRenderWindow(GtkObject * aGtkWidget);

  // get the toplevel window for this widget
  virtual GtkWindow *GetTopLevelWindow(void);


  PRBool   OnKey(nsKeyEvent &aEvent);
  PRBool   OnText(nsTextEvent &aEvent)       { return OnInput(aEvent); };
  PRBool   OnComposition(nsCompositionEvent &aEvent) { return OnInput(aEvent); };
  PRBool   OnInput(nsInputEvent &aEvent);

  // this will return the GtkWidget * that owns this object
  virtual GtkWidget *GetOwningWidget();

  // the event handling code needs to let us know the time of the last event
  static void SetLastEventTime(guint32 aTime);
  static void GetLastEventTime(guint32 *aTime);

  static void DropMotionTarget(void);

  // inform the widget code that we are about to do a drag - it must
  // release the sButtonMotionTarget if it is set.
  static void DragStarted(void) { DropMotionTarget(); };

  virtual void ThemeChanged();

protected:

  virtual void InitCallbacks(char * aName = nsnull);

  NS_IMETHOD CreateNative(GtkObject *parentWindow) { return NS_OK; }

  nsresult CreateWidget(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeParent = nsnull);


  PRBool DispatchWindowEvent(nsGUIEvent* event);

  // this is the "native" destroy code that will destroy any
  // native windows / widgets for this logical widget
  virtual void DestroyNative(void);

  // Sets font for widgets
  virtual void SetFontNative(GdkFont *aFont);
  // Sets backround for widgets
  virtual void SetBackgroundColorNative(GdkColor *aColorNor,
                                        GdkColor *aColorBri,
                                        GdkColor *aColorDark);

  // this is set when a given widget has the focus.
  PRBool       mHasFocus;


  // if anyone uses this for public access other than the key
  // press/release code on the main window, i will kill you. pav
public:
  // this is the current GdkSuperWin with the focus
  static nsWidget *sFocusWindow;

protected:
  // 

  //////////////////////////////////////////////////////////////////
  //
  // GTK signal installers
  //
  //////////////////////////////////////////////////////////////////
  void InstallEnterNotifySignal(GtkWidget * aWidget);

  void InstallLeaveNotifySignal(GtkWidget * aWidget);

  void InstallButtonPressSignal(GtkWidget * aWidget);

  void InstallButtonReleaseSignal(GtkWidget * aWidget);

  virtual
  void InstallFocusInSignal(GtkWidget * aWidget);

  virtual
  void InstallFocusOutSignal(GtkWidget * aWidget);

  void InstallRealizeSignal(GtkWidget * aWidget);

  void AddToEventMask(GtkWidget * aWidget,
                      gint        aEventMask);

  //////////////////////////////////////////////////////////////////
  //
  // OnSomething handlers
  //
  //////////////////////////////////////////////////////////////////
  virtual void OnMotionNotifySignal(GdkEventMotion * aGdkMotionEvent);
  virtual void OnEnterNotifySignal(GdkEventCrossing * aGdkCrossingEvent);
  virtual void OnLeaveNotifySignal(GdkEventCrossing * aGdkCrossingEvent);
  virtual void OnButtonPressSignal(GdkEventButton * aGdkButtonEvent);
  virtual void OnButtonReleaseSignal(GdkEventButton * aGdkButtonEvent);
  virtual void OnFocusInSignal(GdkEventFocus * aGdkFocusEvent);
  virtual void OnFocusOutSignal(GdkEventFocus * aGdkFocusEvent);
  virtual void OnRealize(GtkWidget *aWidget);

  virtual void OnDestroySignal(GtkWidget* aGtkWidget);

  // Static method used to trampoline to OnDestroySignal
  static gint  DestroySignal(GtkWidget *      aGtkWidget,
                             nsWidget*        aWidget);

public:
  virtual void IMECommitEvent(GdkEventKey *aEvent);

  // This MUST be called after you change the widget bounds
  // or after the parent's size changes
  // or when you show the widget
  // We will decide whether to really show the widget or not
  // We will hide the widget if it doesn't overlap the parent bounds
  // This reduces problems with 16-bit coordinates wrapping.
  virtual void ResetInternalVisibility();

protected:
  // override this method to do whatever you have to do to make this widget
  // visible or invisibile --- i.e., the real work of Show()
  virtual void SetInternalVisibility(PRBool aVisible);

  //////////////////////////////////////////////////////////////////
  //
  // GTK widget signals
  //
  //////////////////////////////////////////////////////////////////

  static gint EnterNotifySignal(GtkWidget *        aWidget, 
                                GdkEventCrossing * aGdkCrossingEvent, 
                                gpointer           aData);
  
  static gint LeaveNotifySignal(GtkWidget *        aWidget, 
                                GdkEventCrossing * aGdkCrossingEvent, 
                                gpointer           aData);

  static gint ButtonPressSignal(GtkWidget *      aWidget, 
                                GdkEventButton * aGdkButtonEvent, 
                                gpointer         aData);

  static gint ButtonReleaseSignal(GtkWidget *      aWidget, 
                                  GdkEventButton * aGdkButtonEvent, 
                                  gpointer         aData);

  static gint RealizeSignal(GtkWidget *      aWidget, 
                            gpointer         aData);


  static gint FocusInSignal(GtkWidget *      aWidget, 
                            GdkEventFocus *  aGdkFocusEvent, 
                            gpointer         aData);

  static gint FocusOutSignal(GtkWidget *      aWidget, 
                             GdkEventFocus *  aGdkFocusEvent, 
                             gpointer         aData);

protected:

  //////////////////////////////////////////////////////////////////
  //
  // GTK event support methods
  //
  //////////////////////////////////////////////////////////////////
  void InstallSignal(GtkWidget *   aWidget,
                     gchar *       aSignalName,
                     GtkSignalFunc aSignalFunction);
  
  void InitMouseEvent(GdkEventButton * aGdkButtonEvent,
                      nsMouseEvent &   anEvent);

#ifdef DEBUG
  nsCAutoString  debug_GetName(GtkObject * aGtkWidget);
  nsCAutoString  debug_GetName(GtkWidget * aGtkWidget);
  PRInt32       debug_GetRenderXID(GtkObject * aGtkWidget);
  PRInt32       debug_GetRenderXID(GtkWidget * aGtkWidget);
#endif

  GtkWidget *mWidget;
  // our mozbox for those native widgets
  GtkWidget *mMozBox;

  nsCOMPtr<nsIWidget> mParent;

  // This is the composite update area (union of all the calls to
  // Invalidate)
  nsCOMPtr<nsIRegion> mUpdateArea;

  PRUint32 mPreferredWidth, mPreferredHeight;
  PRPackedBool mListenForResizes;
  PRPackedBool mShown;
  PRPackedBool mInternalShown;

  // this is the rollup listener variables
  static nsCOMPtr<nsIRollupListener> gRollupListener;
  static nsWeakPtr                   gRollupWidget;

  // this is the last time that an event happened.  we keep this
  // around so that we can synth drag events properly
  static guint32 sLastEventTime;

private:
  // this will keep track of whether or not the gdk handler
  // is installed yet
  static PRBool mGDKHandlerInstalled;
  // this will keep track of whether or not we've told the drag
  // service how to call back into us to get the last event time
  static PRBool sTimeCBSet;

  //
  // Keep track of the last widget being "dragged"
  //
public:
  static nsWidget *sButtonMotionTarget;
  static gint sButtonMotionRootX;
  static gint sButtonMotionRootY;
  static gint sButtonMotionWidgetX;
  static gint sButtonMotionWidgetY;
};

#endif /* nsWidget_h__ */
