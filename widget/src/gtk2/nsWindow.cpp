/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code mozilla.org code.
 *
 * The Initial Developer of the Original Code Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
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

#include "nsWindow.h"
#include "nsToolkit.h"
#include "nsIRenderingContext.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"

#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSessionGTK.h"

#include "nsGtkKeyUtils.h"
#include "nsGtkCursors.h"
#include "nsGtkMozRemoteHelper.h"

#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include "gtk2xtbin.h"

#include "nsIPref.h"
#include "nsIServiceManager.h"

#ifdef ACCESSIBILITY
#include "nsPIAccessNode.h"
#include "nsPIAccessible.h"
#include "nsIAccessibleEvent.h"
#include "prenv.h"
#include "stdlib.h"
static PRBool sAccessibilityChecked = PR_FALSE;
static PRBool sAccessibilityEnabled = PR_FALSE;
static const char sSysPrefService [] = "@mozilla.org/system-preference-service;1";
static const char sAccEnv [] = "GNOME_ACCESSIBILITY";
static const char sAccessibilityKey [] = "config.use_system_prefs.accessibility";
#endif

/* For SetIcon */
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

/* utility functions */
static PRBool     check_for_rollup(GdkWindow *aWindow,
                                   gdouble aMouseX, gdouble aMouseY,
                                   PRBool aIsWheel);
static PRBool     is_mouse_in_window(GdkWindow* aWindow,
                                     gdouble aMouseX, gdouble aMouseY);
static nsWindow  *get_window_for_gtk_widget(GtkWidget *widget);
static nsWindow  *get_window_for_gdk_window(GdkWindow *window);
static nsWindow  *get_owning_window_for_gdk_window(GdkWindow *window);
static GtkWidget *get_gtk_widget_for_gdk_window(GdkWindow *window);
static GdkCursor *get_gtk_cursor(nsCursor aCursor);

static GdkWindow *get_inner_gdk_window (GdkWindow *aWindow,
                                        gint x, gint y,
                                        gint *retx, gint *rety);

static inline PRBool is_context_menu_key(const nsKeyEvent& inKeyEvent);
static void   key_event_to_context_menu_event(const nsKeyEvent* inKeyEvent,
                                              nsMouseEvent* outCMEvent);

static int    is_parent_ungrab_enter(GdkEventCrossing *aEvent);
static int    is_parent_grab_leave(GdkEventCrossing *aEvent);

/* callbacks from widgets */
static gboolean expose_event_cb           (GtkWidget *widget,
                                           GdkEventExpose *event);
static gboolean configure_event_cb        (GtkWidget *widget,
                                           GdkEventConfigure *event);
static void     size_allocate_cb          (GtkWidget *widget,
                                           GtkAllocation *allocation);
static gboolean delete_event_cb           (GtkWidget *widget,
                                           GdkEventAny *event);
static gboolean enter_notify_event_cb     (GtkWidget *widget,
                                           GdkEventCrossing *event);
static gboolean leave_notify_event_cb     (GtkWidget *widget,
                                           GdkEventCrossing *event);
static gboolean motion_notify_event_cb    (GtkWidget *widget,
                                           GdkEventMotion *event);
static gboolean button_press_event_cb     (GtkWidget *widget,
                                           GdkEventButton *event);
static gboolean button_release_event_cb   (GtkWidget *widget,
                                           GdkEventButton *event);
static gboolean focus_in_event_cb         (GtkWidget *widget,
                                           GdkEventFocus *event);
static gboolean focus_out_event_cb        (GtkWidget *widget,
                                           GdkEventFocus *event);
static gboolean key_press_event_cb        (GtkWidget *widget,
                                           GdkEventKey *event);
static gboolean key_release_event_cb      (GtkWidget *widget,
                                           GdkEventKey *event);
static gboolean scroll_event_cb           (GtkWidget *widget,
                                           GdkEventScroll *event);
static gboolean visibility_notify_event_cb(GtkWidget *widget,
                                           GdkEventVisibility *event);
static gboolean window_state_event_cb     (GtkWidget *widget,
                                           GdkEventWindowState *event);
static gboolean property_notify_event_cb  (GtkWidget *widget,
                                           GdkEventProperty *event);
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
static GdkFilterReturn plugin_window_filter_func (GdkXEvent *gdk_xevent, 
                                                  GdkEvent *event, 
                                                  gpointer data);
static GdkFilterReturn plugin_client_message_filter (GdkXEvent *xevent,
                                                     GdkEvent *event,
                                                     gpointer data);
#ifdef __cplusplus
}
#endif /* __cplusplus */

static gboolean drag_motion_event_cb      (GtkWidget *aWidget,
                                           GdkDragContext *aDragContext,
                                           gint aX,
                                           gint aY,
                                           guint aTime,
                                           gpointer aData);
static void     drag_leave_event_cb       (GtkWidget *aWidget,
                                           GdkDragContext *aDragContext,
                                           guint aTime,
                                           gpointer aData);
static gboolean drag_drop_event_cb        (GtkWidget *aWidget,
                                           GdkDragContext *aDragContext,
                                           gint aX,
                                           gint aY,
                                           guint aTime,
                                           gpointer *aData);
static void    drag_data_received_event_cb(GtkWidget *aWidget,
                                           GdkDragContext *aDragContext,
                                           gint aX,
                                           gint aY,
                                           GtkSelectionData  *aSelectionData,
                                           guint aInfo,
                                           guint32 aTime,
                                           gpointer aData);

/* initialization static functions */
static nsresult    initialize_prefs        (void);
  
// this is the last window that had a drag event happen on it.
nsWindow *nsWindow::mLastDragMotionWindow = NULL;
PRBool nsWindow::sIsDraggingOutOf = PR_FALSE;

// This is the time of the last button press event.  The drag service
// uses it as the time to start drags.
guint32   nsWindow::mLastButtonPressTime = 0;

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

// the current focus window
static nsWindow         *gFocusWindow          = NULL;
static PRBool            gGlobalsInitialized   = PR_FALSE;
static PRBool            gRaiseWindows         = PR_TRUE;
static nsWindow         *gPluginFocusWindow    = NULL;

nsCOMPtr  <nsIRollupListener> gRollupListener;
nsWeakPtr                     gRollupWindow;

#ifdef USE_XIM

static nsWindow    *gIMEFocusWindow = NULL;
static GdkEventKey *gKeyEvent = NULL;
static PRBool       gKeyEventCommitted = PR_FALSE;
static PRBool       gKeyEventChanged = PR_FALSE;

static void IM_commit_cb              (GtkIMContext *aContext,
                                       const gchar *aString,
                                       nsWindow *aWindow);
static void IM_preedit_changed_cb     (GtkIMContext *aContext,
                                       nsWindow *aWindow);
static void IM_set_text_range         (const PRInt32 aLen,
                                       const gchar *aPreeditString,
                                       const PangoAttrList *aFeedback,
                                       PRUint32 *aTextRangeListLengthResult,
                                       nsTextRangeArray *aTextRangeListResult);

static GtkIMContext *IM_get_input_context(MozDrawingarea *aArea);

// If after selecting profile window, the startup fail, please refer to
// http://bugzilla.gnome.org/show_bug.cgi?id=88940
#endif

#define kWindowPositionSlop 20

// cursor cache
GdkCursor *gCursorCache[eCursorCount];

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof(a[0]))

nsWindow::nsWindow()
{
    mContainer           = nsnull;
    mDrawingarea         = nsnull;
    mShell               = nsnull;
    mWindowGroup         = nsnull;
    mContainerGotFocus   = PR_FALSE;
    mContainerLostFocus  = PR_FALSE;
    mContainerBlockFocus = PR_FALSE;
    mInKeyRepeat         = PR_FALSE;
    mIsVisible           = PR_FALSE;
    mRetryPointerGrab    = PR_FALSE;
    mRetryKeyboardGrab   = PR_FALSE;
    mActivatePending     = PR_FALSE;
    mTransientParent     = nsnull;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mOldFocusWindow      = 0;
    mPluginType          = PluginType_NONE;

    if (!gGlobalsInitialized) {
        gGlobalsInitialized = PR_TRUE;

        // It's OK if either of these fail, but it may not be one day.
        initialize_prefs();
    }
    
    if (mLastDragMotionWindow == this)
        mLastDragMotionWindow = NULL;
    mDragMotionWidget = 0;
    mDragMotionContext = 0;
    mDragMotionX = 0;
    mDragMotionY = 0;
    mDragMotionTime = 0;
    mDragMotionTimerID = 0;

#ifdef USE_XIM
    mIMContext = nsnull;
    mComposingText = PR_FALSE;
#endif

#ifdef ACCESSIBILITY
    mRootAccessible  = nsnull;
#endif

    mIsTranslucent = PR_FALSE;
    mTransparencyBitmap = nsnull;
}

nsWindow::~nsWindow()
{
    LOG(("nsWindow::~nsWindow() [%p]\n", (void *)this));
    if (mLastDragMotionWindow == this) {
        mLastDragMotionWindow = NULL;
    }

    delete[] mTransparencyBitmap;
    mTransparencyBitmap = nsnull;

    Destroy();
}

/* static */ void
nsWindow::ReleaseGlobals()
{
  for (PRUint32 i = 0; i < ARRAY_LENGTH(gCursorCache); ++i) {
    if (gCursorCache[i]) {
      gdk_cursor_unref(gCursorCache[i]);
      gCursorCache[i] = nsnull;
    }
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsCommonWidget,
                             nsISupportsWeakReference)

NS_IMETHODIMP
nsWindow::Create(nsIWidget        *aParent,
                 const nsRect     &aRect,
                 EVENT_CALLBACK   aHandleEventFunction,
                 nsIDeviceContext *aContext,
                 nsIAppShell      *aAppShell,
                 nsIToolkit       *aToolkit,
                 nsWidgetInitData *aInitData)
{
    nsresult rv = NativeCreate(aParent, nsnull, aRect, aHandleEventFunction,
                               aContext, aAppShell, aToolkit, aInitData);
    return rv;
}

NS_IMETHODIMP
nsWindow::Create(nsNativeWidget aParent,
                 const nsRect     &aRect,
                 EVENT_CALLBACK   aHandleEventFunction,
                 nsIDeviceContext *aContext,
                 nsIAppShell      *aAppShell,
                 nsIToolkit       *aToolkit,
                 nsWidgetInitData *aInitData)
{
    nsresult rv = NativeCreate(nsnull, aParent, aRect, aHandleEventFunction,
                               aContext, aAppShell, aToolkit, aInitData);
    return rv;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    if (mIsDestroyed || !mCreated)
        return NS_OK;

    LOG(("nsWindow::Destroy [%p]\n", (void *)this));
    mIsDestroyed = PR_TRUE;
    mCreated = PR_FALSE;

    // ungrab if required
    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);
    if (NS_STATIC_CAST(nsIWidget *, this) == rollupWidget.get()) {
        if (gRollupListener)
            gRollupListener->Rollup();
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
    }

    NativeShow(PR_FALSE);

    // walk the list of children and call destroy on them.  Have to be
    // careful, though -- calling destroy on a kid may actually remove
    // it from our child list, losing its sibling links.
    for (nsIWidget* kid = mFirstChild; kid; ) {
        nsIWidget* next = kid->GetNextSibling();
        kid->Destroy();
        kid = next;
    }

#ifdef USE_XIM
    IMEDestroyContext();
#endif

    // make sure that we remove ourself as the focus window
    if (gFocusWindow == this) {
        LOGFOCUS(("automatically losing focus...\n"));
        gFocusWindow = nsnull;
    }

    // make sure that we remove ourself as the plugin focus window
    if (gPluginFocusWindow == this) {
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }

    // Remove our reference to the window group.  If there was a window
    // group destroying the widget will have automatically unreferenced
    // the group, destroying it if necessary.  And, if we're a child
    // window this isn't going to harm anything.
    mWindowGroup = nsnull;

    if (mShell) {
        gtk_widget_destroy(mShell);
        mShell = nsnull;
        mContainer = nsnull;
    }
    else if (mContainer) {
        gtk_widget_destroy(GTK_WIDGET(mContainer));
        mContainer = nsnull;
    }
    
    if (mDrawingarea) {
        g_object_unref(mDrawingarea);
        mDrawingarea = nsnull;
    }

    OnDestroy();

#ifdef ACCESSIBILITY
    if (mRootAccessible) {
        mRootAccessible = nsnull;
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(PRBool aModal)
{
    LOG(("nsWindow::SetModal [%p] %d\n", (void *)this, aModal));

    // find the toplevel window and add it to the grab list
    GtkWidget *grabWidget = nsnull;

    GetToplevelWidget(&grabWidget);

    if (!grabWidget)
        return NS_ERROR_FAILURE;

    if (aModal)
        gtk_grab_add(grabWidget);
    else
        gtk_grab_remove(grabWidget);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::IsVisible(PRBool & aState)
{
    aState = mIsVisible;
    if (mIsTopLevel && mShell && !GTK_WIDGET_MAPPED(mShell)) {
        /* we do not change mIsVisible to PR_FALSE here so we don't bother
           to change it back to PR_TRUE when the mShell is mapped again. */
        aState = PR_FALSE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
    if (mIsTopLevel && mShell) {
        PRInt32 screenWidth = gdk_screen_width();
        PRInt32 screenHeight = gdk_screen_height();
        if (aAllowSlop) {
            if (*aX < (kWindowPositionSlop - mBounds.width))
                *aX = kWindowPositionSlop - mBounds.width;
            if (*aX > (screenWidth - kWindowPositionSlop))
                *aX = screenWidth - kWindowPositionSlop;
            if (*aY < (kWindowPositionSlop - mBounds.height))
                *aY = kWindowPositionSlop - mBounds.height;
            if (*aY > (screenHeight - kWindowPositionSlop))
                *aY = screenHeight - kWindowPositionSlop;
        } else {
            if (*aX < 0)
                *aX = 0;
            if (*aX > (screenWidth - mBounds.width))
                *aX = screenWidth - mBounds.width;
            if (*aY < 0)
                *aY = 0;
            if (*aY > (screenHeight - mBounds.height))
                *aY = screenHeight - mBounds.height;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
    LOG(("nsWindow::Move [%p] %d %d\n", (void *)this,
         aX, aY));

    mPlaced = PR_TRUE;

    // Since a popup window's x/y coordinates are in relation to to
    // the parent, the parent might have moved so we always move a
    // popup window.
    if (aX == mBounds.x && aY == mBounds.y &&
        mWindowType != eWindowType_popup)
        return NS_OK;

    mBounds.x = aX;
    mBounds.y = aY;

    if (!mCreated)
        return NS_OK;

    if (mIsTopLevel) {
        if (mParent && mWindowType == eWindowType_popup) {
            nsRect oldrect, newrect;
            oldrect.x = aX;
            oldrect.y = aY;
            mParent->WidgetToScreen(oldrect, newrect);
            gtk_window_move(GTK_WINDOW(mShell), newrect.x, newrect.y);
        }
        else {
            // We only move the toplevel window if someone has
            // actually placed the window somewhere.  If no placement
            // has taken place, we just let the window manager Do The
            // Right Thing.
            if (mPlaced)
                gtk_window_move(GTK_WINDOW(mShell), aX, aY);
        }
    }
    else if (mDrawingarea) {
        moz_drawingarea_move(mDrawingarea, aX, aY);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                      nsIWidget                  *aWidget,
                      PRBool                      aActivate)
{
    return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsWindow::SetSizeMode(PRInt32 aMode)
{
    nsresult rv;

    LOG(("nsWindow::SetSizeMode [%p] %d\n", (void *)this, aMode));

    // Save the requested state.
    rv = nsBaseWidget::SetSizeMode(aMode);

    // return if there's no shell or our current state is the same as
    // the mode we were just set to.
    if (!mShell || mSizeState == mSizeMode) {
        return rv;
    }

    switch (aMode) {
    case nsSizeMode_Maximized:
        gtk_window_maximize(GTK_WINDOW(mShell));
        break;
    case nsSizeMode_Minimized:
        gtk_window_iconify(GTK_WINDOW(mShell));
        break;
    default:
        // nsSizeMode_Normal, really.
        if (mSizeState == nsSizeMode_Minimized)
            gtk_window_deiconify(GTK_WINDOW(mShell));
        else if (mSizeState == nsSizeMode_Maximized)
            gtk_window_unmaximize(GTK_WINDOW(mShell));
        break;
    }

    mSizeState = mSizeMode;

    return rv;
}

NS_IMETHODIMP
nsWindow::Enable(PRBool aState)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetFocus(PRBool aRaise)
{
    // Make sure that our owning widget has focus.  If it doesn't try to
    // grab it.  Note that we don't set our focus flag in this case.

    LOGFOCUS(("  SetFocus [%p]\n", (void *)this));

    if (!mDrawingarea)
        return NS_ERROR_FAILURE;

    GtkWidget *owningWidget =
        get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);
    if (!owningWidget)
        return NS_ERROR_FAILURE;

    // Raise the window if someone passed in PR_TRUE and the prefs are
    // set properly.
    GtkWidget *toplevelWidget = gtk_widget_get_toplevel(owningWidget);

    if (gRaiseWindows && aRaise && toplevelWidget &&
        !GTK_WIDGET_HAS_FOCUS(owningWidget) &&
        !GTK_WIDGET_HAS_FOCUS(toplevelWidget))
        GetAttention(-1);

    nsWindow  *owningWindow = get_window_for_gtk_widget(owningWidget);
    if (!owningWindow)
        return NS_ERROR_FAILURE;

    if (!GTK_WIDGET_HAS_FOCUS(owningWidget)) {
        LOGFOCUS(("  grabbing focus for the toplevel [%p]\n", (void *)this));
        owningWindow->mContainerBlockFocus = PR_TRUE;
        gtk_widget_grab_focus(owningWidget);
        owningWindow->mContainerBlockFocus = PR_FALSE;

        DispatchGotFocusEvent();

        // unset the activate flag
        if (owningWindow->mActivatePending) {
            owningWindow->mActivatePending = PR_FALSE;
            DispatchActivateEvent();
        }

        return NS_OK;
    }

    // If this is the widget that already has focus, return.
    if (gFocusWindow == this) {
        LOGFOCUS(("  already have focus [%p]\n", (void *)this));
        return NS_OK;
    }

    // If there is already a focused child window, dispatch a LOSTFOCUS
    // event from that widget and unset its got focus flag.
    if (gFocusWindow) {
#ifdef USE_XIM
        // If the focus window and this window share the same input
        // context we don't have to change the focus of the IME
        // context
        if (IM_get_input_context(this->mDrawingarea) !=
            IM_get_input_context(gFocusWindow->mDrawingarea))
            gFocusWindow->IMELoseFocus();
#endif
        gFocusWindow->LoseFocus();
    }

    // Set this window to be the focused child window, update our has
    // focus flag and dispatch a GOTFOCUS event.
    gFocusWindow = this;

#ifdef USE_XIM
    IMESetFocus();
#endif

    LOGFOCUS(("  widget now has focus - dispatching events [%p]\n", 
              (void *)this));

    DispatchGotFocusEvent();
    
    // unset the activate flag
    if (owningWindow->mActivatePending) {
        owningWindow->mActivatePending = PR_FALSE;
        DispatchActivateEvent();
    }

    LOGFOCUS(("  done dispatching events in SetFocus() [%p]\n",
              (void *)this));

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsRect &aRect)
{
    nsRect origin(0, 0, mBounds.width, mBounds.height);
    WidgetToScreen(origin, aRect);
    LOG(("GetScreenBounds %d %d | %d %d | %d %d\n",
         aRect.x, aRect.y,
         mBounds.width, mBounds.height,
         aRect.width, aRect.height));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetForegroundColor(const nscolor &aColor)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetBackgroundColor(const nscolor &aColor)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsIFontMetrics*
nsWindow::GetFont(void)
{
    return nsnull;
}

NS_IMETHODIMP
nsWindow::SetFont(const nsFont &aFont)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetCursor(nsCursor aCursor)
{
    // if we're not the toplevel window pass up the cursor request to
    // the toplevel window to handle it.
    if (!mContainer && mDrawingarea) {
        GtkWidget *widget =
            get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);
        nsWindow *window = get_window_for_gtk_widget(widget);
        return window->SetCursor(aCursor);
    }

    // Only change cursor if it's actually been changed
    if (aCursor != mCursor) {
        GdkCursor *newCursor = NULL;

        newCursor = get_gtk_cursor(aCursor);

        if (nsnull != newCursor) {
            mCursor = aCursor;

            if (!mContainer)
                return NS_OK;

            gdk_window_set_cursor(GTK_WIDGET(mContainer)->window, newCursor);

            XFlush(GDK_DISPLAY());
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsWindow::Validate()
{
    // Get the update for this window and, well, just drop it on the
    // floor.
    if (!mDrawingarea)
        return NS_OK;

    GdkRegion *region = gdk_window_get_update_area(mDrawingarea->inner_window);

    if (region)
        gdk_region_destroy(region);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(PRBool aIsSynchronous)
{
    GdkRectangle rect;

    rect.x = mBounds.x;
    rect.y = mBounds.y;
    rect.width = mBounds.width;
    rect.height = mBounds.height;

    LOGDRAW(("Invalidate (all) [%p]: %d %d %d %d\n", (void *)this,
             rect.x, rect.y, rect.width, rect.height));

    if (!mDrawingarea)
        return NS_OK;

    gdk_window_invalidate_rect(mDrawingarea->inner_window,
                               &rect, TRUE);
    if (aIsSynchronous)
        gdk_window_process_updates(mDrawingarea->inner_window, TRUE);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsRect &aRect,
                     PRBool        aIsSynchronous)
{
    GdkRectangle rect;

    rect.x = aRect.x;
    rect.y = aRect.y;
    rect.width = aRect.width;
    rect.height = aRect.height;

    LOGDRAW(("Invalidate (rect) [%p]: %d %d %d %d (sync: %d)\n", (void *)this,
             rect.x, rect.y, rect.width, rect.height, aIsSynchronous));

    if (!mDrawingarea)
        return NS_OK;

    gdk_window_invalidate_rect(mDrawingarea->inner_window,
                               &rect, TRUE);
    if (aIsSynchronous)
        gdk_window_process_updates(mDrawingarea->inner_window, TRUE);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::InvalidateRegion(const nsIRegion* aRegion,
                           PRBool           aIsSynchronous)
{
    GdkRegion *region = nsnull;
    aRegion->GetNativeRegion((void *&)region);

    if (region && mDrawingarea) {
        GdkRectangle rect;
        gdk_region_get_clipbox(region, &rect);

        LOGDRAW(("Invalidate (region) [%p]: %d %d %d %d (sync: %d)\n",
                 (void *)this,
                 rect.x, rect.y, rect.width, rect.height, aIsSynchronous));

        gdk_window_invalidate_region(mDrawingarea->inner_window,
                                     region, TRUE);
    }
    else {
        LOGDRAW(("Invalidate (region) [%p] with empty region\n",
                 (void *)this));
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Update()
{
    if (!mDrawingarea)
        return NS_OK;

    gdk_window_process_updates(mDrawingarea->inner_window, TRUE);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetColorMap(nsColorMap *aColorMap)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Scroll(PRInt32  aDx,
                 PRInt32  aDy,
                 nsRect  *aClipRect)
{
    if (!mDrawingarea)
        return NS_OK;

    moz_drawingarea_scroll(mDrawingarea, aDx, aDy);

    // Update bounds on our child windows
    for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
        nsRect bounds;
        kid->GetBounds(bounds);
        bounds.x += aDx;
        bounds.y += aDy;
        NS_STATIC_CAST(nsBaseWidget*, kid)->SetBounds(bounds);
    }

    // Process all updates so that everything is drawn.
    gdk_window_process_all_updates();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScrollWidgets(PRInt32 aDx,
                        PRInt32 aDy)
{
    if (!mDrawingarea)
        return NS_OK;

    moz_drawingarea_scroll(mDrawingarea, aDx, aDy);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScrollRect(nsRect  &aSrcRect,
                     PRInt32  aDx,
                     PRInt32  aDy)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void*
nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
        if (!mDrawingarea)
            return nsnull;

        return mDrawingarea->inner_window;
        break;
    }

    case NS_NATIVE_PLUGIN_PORT:
        return SetupPluginPort();
        break;

    case NS_NATIVE_DISPLAY:
        return GDK_DISPLAY();
        break;

    case NS_NATIVE_GRAPHIC: {
        NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
        return (void *)NS_STATIC_CAST(nsToolkit *, mToolkit)->GetSharedGC();
        break;
    }

    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nsnull;
    }
}

NS_IMETHODIMP
nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetTitle(const nsAString& aTitle)
{
    if (!mShell)
        return NS_OK;

    // convert the string into utf8 and set the title.
    NS_ConvertUCS2toUTF8 utf8title(aTitle);
    gtk_window_set_title(GTK_WINDOW(mShell), (const char *)utf8title.get());

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAString& aIconSpec)
{
    if (!mShell)
        return NS_OK;

    // Start at the app chrome directory.
    nsCOMPtr<nsIFile> chromeDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR,
                                         getter_AddRefs(chromeDir));
    if (NS_FAILED(rv))
        return rv;

    // Get the native file name of that directory.
    nsAutoString iconPath;
    chromeDir->GetPath(iconPath);

    // Now take input path...
    nsAutoString iconSpec(aIconSpec);
    // ...append ".xpm" to it
    iconSpec.AppendLiteral(".xpm");

    // ...and figure out where /chrome/... is within that
    // (and skip the "resource:///chrome" part).
    nsAutoString key(NS_LITERAL_STRING("/chrome/"));
    PRInt32 n = iconSpec.Find(key) + key.Length();

    // Append that to icon resource path.
    iconPath.Append(iconSpec.get() + n - 1);

    nsCOMPtr<nsILocalFile> pathConverter;
    rv = NS_NewLocalFile(iconPath, PR_TRUE,
                         getter_AddRefs(pathConverter));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString path;
    pathConverter->GetNativePath(path);

    nsCStringArray iconList;
    iconList.AppendCString(path);

    // Get the 16px icon path as well
    iconSpec = aIconSpec + NS_LITERAL_STRING("16.xpm");

    chromeDir->GetPath(iconPath);
    iconPath.Append(iconSpec.get() + n - 1);

    rv = NS_NewLocalFile(iconPath, PR_TRUE,
                         getter_AddRefs(pathConverter));
    if (NS_FAILED(rv))
        return rv;

    pathConverter->GetNativePath(path);
    iconList.AppendCString(path);

    return SetWindowIconList(iconList);
}

NS_IMETHODIMP
nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::ShowMenuBar(PRBool aShow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    gint x, y = 0;

    if (mContainer) {
        gdk_window_get_root_origin(GTK_WIDGET(mContainer)->window,
                                   &x, &y);
        LOG(("WidgetToScreen (container) %d %d\n", x, y));
    }
    else if (mDrawingarea) {
        gdk_window_get_origin(mDrawingarea->inner_window, &x, &y);
        LOG(("WidgetToScreen (drawing) %d %d\n", x, y));
    }

    aNewRect.x = x + aOldRect.x;
    aNewRect.y = y + aOldRect.y;
    aNewRect.width = aOldRect.width;
    aNewRect.height = aOldRect.height;

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::BeginResizingChildren(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::EndResizingChildren(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::EnableDragDrop(PRBool aEnable)
{
    return NS_OK;
}

void
nsWindow::ConvertToDeviceCoordinates(nscoord &aX,
                                     nscoord &aY)
{
}

NS_IMETHODIMP
nsWindow::PreCreateWidget(nsWidgetInitData *aWidgetInitData)
{
    if (nsnull != aWidgetInitData) {
        mWindowType = aWidgetInitData->mWindowType;
        mBorderStyle = aWidgetInitData->mBorderStyle;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindow::CaptureMouse(PRBool aCapture)
{
    LOG(("CaptureMouse %p\n", (void *)this));

    if (!mDrawingarea)
        return NS_OK;

    GtkWidget *widget = 
        get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);

    if (aCapture) {
        gtk_grab_add(widget);
        GrabPointer();
    }
    else {
        ReleaseGrabs();
        gtk_grab_remove(widget);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
                              PRBool             aDoCapture,
                              PRBool             aConsumeRollupEvent)
{
    if (!mDrawingarea)
        return NS_OK;

    GtkWidget *widget = 
        get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);

    LOG(("CaptureRollupEvents %p\n", (void *)this));

    if (aDoCapture) {
        gRollupListener = aListener;
        gRollupWindow = do_GetWeakReference(NS_STATIC_CAST(nsIWidget*,
                                                           this));
        // real grab is only done when there is no dragging
        if (!nsWindow::DragInProgress()) {
            gtk_grab_add(widget);
            GrabPointer();
            GrabKeyboard();
        }
    }
    else {
        if (!nsWindow::DragInProgress()) {
            ReleaseGrabs();
            gtk_grab_remove(widget);
        }
        gRollupListener = nsnull;
        gRollupWindow = nsnull;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetAttention(PRInt32 aCycleCount)
{
    LOG(("nsWindow::GetAttention [%p]\n", (void *)this));

    GtkWidget* top_window = nsnull;
    GetToplevelWidget(&top_window);
    if (top_window && GTK_WIDGET_VISIBLE(top_window)) {
        gdk_window_show(top_window->window);
    }
    
    return NS_OK;
}

void
nsWindow::LoseFocus(void)
{
    // make sure that we reset our repeat counter so the next keypress
    // for this widget will get the down event
    mInKeyRepeat = PR_FALSE;

    // Dispatch a lostfocus event
    DispatchLostFocusEvent();

    LOGFOCUS(("  widget lost focus [%p]\n", (void *)this));
}

gboolean
nsWindow::OnExposeEvent(GtkWidget *aWidget, GdkEventExpose *aEvent)
{
    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, (void *)aEvent->window));
        return NS_OK;
    }

    if (!mDrawingarea)
        return FALSE;

    // handle exposes for the inner window only
    if (aEvent->window != mDrawingarea->inner_window)
        return FALSE;

    nsCOMPtr<nsIRenderingContext> rc = getter_AddRefs(GetRenderingContext());

// defining NS_PAINT_SEPARATELY is useful for debugging invalidation
// problems since it limits repainting to the rects that were actually
// invalidated.
#undef NS_PAINT_SEPARATELY

#ifdef NS_PAINT_SEPARATELY
    GdkRectangle *rects;
    gint nrects;
    gdk_region_get_rectangles(aEvent->region, &rects, &nrects);
    for (GdkRectangle *r = rects, *r_end = rects + nrects; r < r_end; ++r) {

    nsRect rect(r->x, r->y, r->width, r->height);
#else
    // ok, send out the paint event
    // XXX figure out the region/rect stuff!
    nsRect rect(aEvent->area.x, aEvent->area.y,
                aEvent->area.width, aEvent->area.height);
#endif

    LOGDRAW(("sending expose event [%p] %p 0x%lx\n\t%d %d %d %d\n",
             (void *)this,
             (void *)aEvent->window,
             GDK_WINDOW_XWINDOW(aEvent->window),
             rect.x, rect.y, rect.width, rect.height));

    nsPaintEvent event(NS_PAINT, this);
    event.point.x = rect.x;
    event.point.y = rect.y;
    event.rect = &rect;
    // XXX fix this!
    event.region = nsnull;
    // XXX fix this!
    event.renderingContext = rc;

    nsEventStatus status;
    DispatchEvent(&event, status);

#ifdef NS_PAINT_SEPARATELY
    }

    g_free(rects);
#endif

    // check the return value!
    return TRUE;
}

gboolean
nsWindow::OnConfigureEvent(GtkWidget *aWidget, GdkEventConfigure *aEvent)
{
    LOG(("configure event [%p] %d %d %d %d\n", (void *)this,
         aEvent->x, aEvent->y, aEvent->width, aEvent->height));

    // can we shortcut?
    if (mBounds.x == aEvent->x &&
        mBounds.y == aEvent->y)
        return FALSE;

    // Toplevel windows need to have their bounds set so that we can
    // keep track of our location.  It's not often that the x,y is set
    // by the layout engine.  Width and height are set elsewhere.
    if (mIsTopLevel) {
        mPlaced = PR_TRUE;
        // Need to translate this into the right coordinates
        nsRect oldrect, newrect;
        WidgetToScreen(oldrect, newrect);
        mBounds.x = newrect.x;
        mBounds.y = newrect.y;
    }

    nsGUIEvent event(NS_MOVE, this);

    event.point.x = aEvent->x;
    event.point.y = aEvent->y;

    // XXX mozilla will invalidate the entire window after this move
    // complete.  wtf?
    nsEventStatus status;
    DispatchEvent(&event, status);

    return FALSE;
}

void
nsWindow::OnSizeAllocate(GtkWidget *aWidget, GtkAllocation *aAllocation)
{
    LOG(("size_allocate [%p] %d %d %d %d\n",
         (void *)this, aAllocation->x, aAllocation->y,
         aAllocation->width, aAllocation->height));

    nsRect rect(aAllocation->x, aAllocation->y,
                aAllocation->width, aAllocation->height);

    mBounds.width = rect.width;
    mBounds.height = rect.height;

    if (!mDrawingarea)
        return;

    moz_drawingarea_resize (mDrawingarea, rect.width, rect.height);

    nsEventStatus status;
    DispatchResizeEvent (rect, status);
}

void
nsWindow::OnDeleteEvent(GtkWidget *aWidget, GdkEventAny *aEvent)
{
    nsGUIEvent event(NS_XUL_CLOSE, this);

    event.point.x = 0;
    event.point.y = 0;

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnEnterNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
    if (aEvent->subwindow != NULL)
        return;

    nsMouseEvent event(NS_MOUSE_ENTER, this);

    event.point.x = nscoord(aEvent->x);
    event.point.y = nscoord(aEvent->y);

    LOG(("OnEnterNotify: %p\n", (void *)this));

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnLeaveNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
    if (aEvent->subwindow != NULL)
        return;

    nsMouseEvent event(NS_MOUSE_EXIT, this);

    event.point.x = nscoord(aEvent->x);
    event.point.y = nscoord(aEvent->y);

    LOG(("OnLeaveNotify: %p\n", (void *)this));

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnMotionNotifyEvent(GtkWidget *aWidget, GdkEventMotion *aEvent)
{
    // when we receive this, it must be that the gtk dragging is over,
    // it is dropped either in or out of mozilla, clear the flag
    sIsDraggingOutOf = PR_FALSE;

    // see if we can compress this event
    XEvent xevent;
    PRPackedBool synthEvent = PR_FALSE;
    while (XCheckWindowEvent(GDK_WINDOW_XDISPLAY(aEvent->window),
                             GDK_WINDOW_XWINDOW(aEvent->window),
                             ButtonMotionMask, &xevent)) {
        synthEvent = PR_TRUE;
    }

    // if plugins still keeps the focus, get it back
    if (gPluginFocusWindow && gPluginFocusWindow != this) {
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }

    nsMouseEvent event(NS_MOUSE_MOVE, this);

    if (synthEvent) {
        event.point.x = nscoord(xevent.xmotion.x);
        event.point.y = nscoord(xevent.xmotion.y);

        event.isShift   = (xevent.xmotion.state & GDK_SHIFT_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isControl = (xevent.xmotion.state & GDK_CONTROL_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isAlt     = (xevent.xmotion.state & GDK_MOD1_MASK)
            ? PR_TRUE : PR_FALSE;
    }
    else {
        event.point.x = nscoord(aEvent->x);
        event.point.y = nscoord(aEvent->y);

        event.isShift   = (aEvent->state & GDK_SHIFT_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isControl = (aEvent->state & GDK_CONTROL_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isAlt     = (aEvent->state & GDK_MOD1_MASK)
            ? PR_TRUE : PR_FALSE;
    }

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnButtonPressEvent(GtkWidget *aWidget, GdkEventButton *aEvent)
{
    PRUint32      eventType;
    nsEventStatus status;

    // If you double click in GDK, it will actually generate a single
    // click event before sending the double click event, and this is
    // different than the DOM spec.  GDK puts this in the queue
    // programatically, so it's safe to assume that if there's a
    // double click in the queue, it was generated so we can just drop
    // this click.
    GdkEvent *peekedEvent = gdk_event_peek();
    if (peekedEvent) {
        GdkEventType type = peekedEvent->any.type;
        gdk_event_free(peekedEvent);
        if (type == GDK_2BUTTON_PRESS || type == GDK_3BUTTON_PRESS)
            return;
    }

    // Always save the time of this event
    mLastButtonPressTime = aEvent->time;

    // check to see if we should rollup
    nsWindow *containerWindow;
    GetContainerWindow(&containerWindow);

    if (!gFocusWindow) {
        containerWindow->mActivatePending = PR_FALSE;
        DispatchActivateEvent();
    }
    if (check_for_rollup(aEvent->window, aEvent->x_root, aEvent->y_root,
                         PR_FALSE))
        return;

    switch (aEvent->button) {
    case 2:
        eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
        break;
    case 3:
        eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
        break;
    default:
        eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
        break;
    }

    nsMouseEvent event(eventType, this);
    InitButtonEvent(event, aEvent);

    DispatchEvent(&event, status);

    // right menu click on linux should also pop up a context menu
    if (eventType == NS_MOUSE_RIGHT_BUTTON_DOWN) {
        nsMouseEvent contextMenuEvent(NS_CONTEXTMENU, this);
        InitButtonEvent(contextMenuEvent, aEvent);
        DispatchEvent(&contextMenuEvent, status);
    }
}

void
nsWindow::OnButtonReleaseEvent(GtkWidget *aWidget, GdkEventButton *aEvent)
{
    PRUint32      eventType;

    switch (aEvent->button) {
    case 2:
        eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
        break;
    case 3:
        eventType = NS_MOUSE_RIGHT_BUTTON_UP;
        break;
        // don't send events for these types
    case 4:
    case 5:
        return;
        break;
        // default including button 1 is left button up
    default:
        eventType = NS_MOUSE_LEFT_BUTTON_UP;
        break;
    }

    nsMouseEvent  event(eventType, this);
    InitButtonEvent(event, aEvent);

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnContainerFocusInEvent(GtkWidget *aWidget, GdkEventFocus *aEvent)
{
    LOGFOCUS(("OnContainerFocusInEvent [%p]\n", (void *)this));
    // Return if someone has blocked events for this widget.  This will
    // happen if someone has called gtk_widget_grab_focus() from
    // nsWindow::SetFocus() and will prevent recursion.
    if (mContainerBlockFocus) {
        LOGFOCUS(("Container focus is blocked [%p]\n", (void *)this));
        return;
    }

    if (mIsTopLevel)
        mActivatePending = PR_TRUE;

    // dispatch a got focus event
    DispatchGotFocusEvent();

    // send the activate event if it wasn't already sent via any
    // SetFocus() calls that were the result of the GOTFOCUS event
    // above.
    if (mActivatePending) {
        mActivatePending = PR_FALSE;
        DispatchActivateEvent();
    }

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
}

void
nsWindow::OnContainerFocusOutEvent(GtkWidget *aWidget, GdkEventFocus *aEvent)
{
    LOGFOCUS(("OnContainerFocusOutEvent [%p]\n", (void *)this));

    // plugin lose focus
    if (gPluginFocusWindow) {
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }

    // Figure out if the focus widget is the child of this window.  If
    // it is, send a focus out and deactivate event for it.
    if (!gFocusWindow)
        return;

    GdkWindow *tmpWindow;
    tmpWindow = (GdkWindow *)gFocusWindow->GetNativeData(NS_NATIVE_WINDOW);
    nsWindow *tmpnsWindow = get_window_for_gdk_window(tmpWindow);

    while (tmpWindow && tmpnsWindow) {
        // found it!
        if (tmpnsWindow == this)
            goto foundit;

        tmpWindow = gdk_window_get_parent(tmpWindow);
        if (!tmpWindow)
            break;

        tmpnsWindow = get_owning_window_for_gdk_window(tmpWindow);
    }

    LOGFOCUS(("The focus widget was not a child of this window [%p]\n",
              (void *)this));

    return;

 foundit:

#ifdef USE_XIM
    gFocusWindow->IMELoseFocus();
#endif

    gFocusWindow->LoseFocus();

    // We only dispatch a deactivate event if we are a toplevel
    // window, otherwise the embedding code takes care of it.
    if (mIsTopLevel)
        gFocusWindow->DispatchDeactivateEvent();

    gFocusWindow = nsnull;

    mActivatePending = PR_FALSE;

    LOGFOCUS(("Done with container focus out [%p]\n", (void *)this));
}

gboolean
nsWindow::OnKeyPressEvent(GtkWidget *aWidget, GdkEventKey *aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));

#ifdef USE_XIM
    // if we are in the middle of composing text, XIM gets to see it
    // before mozilla does.
   LOGIM(("key press [%p]: composing %d val %d\n",
           (void *)this, mComposingText, aEvent->keyval));
   if (IMEFilterEvent(aEvent))
       return TRUE;
   LOGIM(("sending as regular key press event\n"));
#endif

    nsEventStatus status;

    // work around for annoying things.
    if (aEvent->keyval == GDK_Tab && aEvent->state & GDK_CONTROL_MASK &&
        aEvent->state & GDK_MOD1_MASK) {
        return TRUE;
    }


    // If the key repeat flag isn't set then set it so we don't send
    // another key down event on the next key press -- DOM events are
    // key down, key press and key up.  X only has key press and key
    // release.  gtk2 already filters the extra key release events for
    // us.

    if (!mInKeyRepeat) {
        mInKeyRepeat = PR_TRUE;

        // send the key down event
        nsKeyEvent downEvent(NS_KEY_DOWN, this);
        InitKeyEvent(downEvent, aEvent);
        DispatchEvent(&downEvent, status);
    }

    // Don't pass modifiers as NS_KEY_PRESS events.
    // TODO: Instead of selectively excluding some keys from NS_KEY_PRESS events,
    //       we should instead selectively include (as per MSDN spec; no official
    //       spec covers KeyPress events).
    if (aEvent->keyval == GDK_Shift_L
        || aEvent->keyval == GDK_Shift_R
        || aEvent->keyval == GDK_Control_L
        || aEvent->keyval == GDK_Control_R
        || aEvent->keyval == GDK_Alt_L
        || aEvent->keyval == GDK_Alt_R
        || aEvent->keyval == GDK_Meta_L
        || aEvent->keyval == GDK_Meta_R) {
        return TRUE;
    }
    nsKeyEvent event(NS_KEY_PRESS, this);
    InitKeyEvent(event, aEvent);
    event.charCode = nsConvertCharCodeToUnicode(aEvent);
    if (event.charCode) {
        event.keyCode = 0;
        // if the control, meta, or alt key is down, then we should leave
        // the isShift flag alone (probably not a printable character)
        // if none of the other modifier keys are pressed then we need to
        // clear isShift so the character can be inserted in the editor

        if (event.isControl || event.isAlt || event.isMeta) {
           // make Ctrl+uppercase functional as same as Ctrl+lowercase
           // when Ctrl+uppercase(eg.Ctrl+C) is pressed,convert the charCode
           // from uppercase to lowercase(eg.Ctrl+c),so do Alt and Meta Key
           // It is hack code for bug 61355, there is same code snip for
           // Windows platform in widget/src/windows/nsWindow.cpp: See bug 16486
           // Note: if Shift is pressed at the same time, do not to_lower()
           // Because Ctrl+Shift has different function with Ctrl
           if (!event.isShift &&
               event.charCode >= GDK_A &&
               event.charCode <= GDK_Z)
            event.charCode = gdk_keyval_to_lower(event.charCode);
        }
    }

    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (is_context_menu_key(event)) {
        nsMouseEvent contextMenuEvent;
        key_event_to_context_menu_event(&event, &contextMenuEvent);
        DispatchEvent(&contextMenuEvent, status);
    }
    else {
        // send the key press event
        DispatchEvent(&event, status);
    }

    // If the event was consumed, return.
    LOGIM(("status %d\n", status));
    if (status == nsEventStatus_eConsumeNoDefault) {
        LOGIM(("key press consumed\n"));
        return TRUE;
    }

    return FALSE;
}

gboolean
nsWindow::OnKeyReleaseEvent(GtkWidget *aWidget, GdkEventKey *aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));

#ifdef USE_XIM
    if (IMEFilterEvent(aEvent))
        return TRUE;
#endif

    nsEventStatus status;
    
    // unset the repeat flag
    mInKeyRepeat = PR_FALSE;

    // send the key event as a key up event
    nsKeyEvent event(NS_KEY_UP, this);
    InitKeyEvent(event, aEvent);

    DispatchEvent(&event, status);

    // If the event was consumed, return.
    if (status == nsEventStatus_eConsumeNoDefault) {
        LOGIM(("key release consumed\n"));
        return TRUE;
    }

    return FALSE;
}

void
nsWindow::OnScrollEvent(GtkWidget *aWidget, GdkEventScroll *aEvent)
{
    nsMouseScrollEvent event(NS_MOUSE_SCROLL, this);
    InitMouseScrollEvent(event, aEvent);

    // check to see if we should rollup
    if (check_for_rollup(aEvent->window, aEvent->x_root, aEvent->y_root,
                         PR_TRUE)) {
        return;
    }

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnVisibilityNotifyEvent(GtkWidget *aWidget,
                                  GdkEventVisibility *aEvent)
{
    switch (aEvent->state) {
    case GDK_VISIBILITY_UNOBSCURED:
    case GDK_VISIBILITY_PARTIAL:
        mIsVisible = PR_TRUE;
        // if we have to retry the grab, retry it.
        EnsureGrabs();
        break;
    default: // includes GDK_VISIBILITY_FULLY_OBSCURED
        mIsVisible = PR_FALSE;
        break;
    }
}

void
nsWindow::OnWindowStateEvent(GtkWidget *aWidget, GdkEventWindowState *aEvent)
{
    LOG(("nsWindow::OnWindowStateEvent [%p] changed %d new_window_state %d\n",
         (void *)this, aEvent->changed_mask, aEvent->new_window_state));

    nsSizeModeEvent event(NS_SIZEMODE, this);

    // We don't care about anything but changes in the maximized/icon
    // states
    if (aEvent->changed_mask &
        (GDK_WINDOW_STATE_ICONIFIED|GDK_WINDOW_STATE_MAXIMIZED) == 0) {
        return;
    }

    if (aEvent->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
        LOG(("\tIconified\n"));
        event.mSizeMode = nsSizeMode_Minimized;
        mSizeState = nsSizeMode_Minimized;
    }
    else if (aEvent->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
        LOG(("\tMaximized\n"));
        event.mSizeMode = nsSizeMode_Maximized;
        mSizeState = nsSizeMode_Maximized;
    }
    else {
        LOG(("\tNormal\n"));
        event.mSizeMode = nsSizeMode_Normal;
        mSizeState = nsSizeMode_Normal;
    }

    nsEventStatus status;
    DispatchEvent(&event, status);
}

gboolean
nsWindow::OnDragMotionEvent(GtkWidget *aWidget,
                            GdkDragContext *aDragContext,
                            gint aX,
                            gint aY,
                            guint aTime,
                            gpointer aData)
{
    LOG(("nsWindow::OnDragMotionSignal\n"));

    sIsDraggingOutOf = PR_FALSE;

    // Reset out drag motion timer
    ResetDragMotionTimer(aWidget, aDragContext, aX, aY, aTime);

    // get our drag context
    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
    nsCOMPtr<nsIDragSessionGTK> dragSessionGTK = do_QueryInterface(dragService);

    // first, figure out which internal widget this drag motion actually
    // happened on
    nscoord retx = 0;
    nscoord rety = 0;

    GdkWindow *thisWindow = aWidget->window;
    GdkWindow *returnWindow = NULL;
    returnWindow = get_inner_gdk_window(thisWindow, aX, aY,
                                        &retx, &rety);
    nsWindow *innerMostWidget = NULL;
    innerMostWidget = get_window_for_gdk_window(returnWindow);

    if (!innerMostWidget)
        innerMostWidget = this;

    // check to see if there was a drag motion window already in place
    if (mLastDragMotionWindow) {
        // if it wasn't this
        if (mLastDragMotionWindow != innerMostWidget) {
            // send a drag event to the last window that got a motion event
            mLastDragMotionWindow->OnDragLeave();
            // and enter on the new one
            innerMostWidget->OnDragEnter(retx, rety);
        }
    }
    else {
        // if there was no other motion window, then we're starting a
        // drag.
        dragService->StartDragSession();
        // if there was no other motion window, send an enter event
        innerMostWidget->OnDragEnter(retx, rety);
    }

    // set the last window to the innerMostWidget
    mLastDragMotionWindow = innerMostWidget;

    // update the drag context
    dragSessionGTK->TargetSetLastContext(aWidget, aDragContext, aTime);

    // notify the drag service that we are starting a drag motion.
    dragSessionGTK->TargetStartDragMotion();

    nsMouseEvent event(NS_DRAGDROP_OVER, innerMostWidget);

    InitDragEvent(event);

    // now that we have initialized the event update our drag status
    UpdateDragStatus(event, aDragContext, dragService);

    event.point.x = retx;
    event.point.y = rety;

    innerMostWidget->AddRef();

    nsEventStatus status;
    innerMostWidget->DispatchEvent(&event, status);

    innerMostWidget->Release();

    // we're done with the drag motion event.  notify the drag service.
    dragSessionGTK->TargetEndDragMotion(aWidget, aDragContext, aTime);
    
    // and unset our context
    dragSessionGTK->TargetSetLastContext(0, 0, 0);

    return TRUE;
}

void
nsWindow::OnDragLeaveEvent(GtkWidget *aWidget,
                           GdkDragContext *aDragContext,
                           guint aTime,
                           gpointer aData)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?

    LOG(("nsWindow::OnDragLeaveSignal(%p)\n", this));

    sIsDraggingOutOf = PR_TRUE;

    // make sure to unset any drag motion timers here.
    ResetDragMotionTimer(0, 0, 0, 0, 0);

    // create a fast timer - we're delaying the drag leave until the
    // next mainloop in hopes that we might be able to get a drag drop
    // signal
    mDragLeaveTimer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ASSERTION(mDragLeaveTimer, "Failed to create drag leave timer!");
    // fire this baby asafp, but not too quickly... see bug 216800 ;-)
    mDragLeaveTimer->InitWithFuncCallback(DragLeaveTimerCallback,
                                          (void *)this,
                                          20, nsITimer::TYPE_ONE_SHOT);
}

gboolean
nsWindow::OnDragDropEvent(GtkWidget *aWidget,
                          GdkDragContext *aDragContext,
                          gint aX,
                          gint aY,
                          guint aTime,
                          gpointer *aData)

{
    LOG(("nsWindow::OnDragDropSignal\n"));

    // get our drag context
    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
    nsCOMPtr<nsIDragSessionGTK> dragSessionGTK = do_QueryInterface(dragService);

    nscoord retx = 0;
    nscoord rety = 0;
    
    GdkWindow *thisWindow = aWidget->window;
    GdkWindow *returnWindow = NULL;
    returnWindow = get_inner_gdk_window(thisWindow, aX, aY, &retx, &rety);

    nsWindow *innerMostWidget = NULL;
    innerMostWidget = get_window_for_gdk_window(returnWindow);

    // set this now before any of the drag enter or leave events happen
    dragSessionGTK->TargetSetLastContext(aWidget, aDragContext, aTime);

    if (!innerMostWidget)
        innerMostWidget = this;

    // check to see if there was a drag motion window already in place
    if (mLastDragMotionWindow) {
        // if it wasn't this
        if (mLastDragMotionWindow != innerMostWidget) {
            // send a drag event to the last window that got a motion event
            mLastDragMotionWindow->OnDragLeave();
            // and enter on the new one
            innerMostWidget->OnDragEnter(retx, rety);
        }
    }
    else {
        // ok, fire up the drag session so that we think that a drag is in
        // progress
        dragService->StartDragSession();
        // if there was no other motion window, send an enter event
        innerMostWidget->OnDragEnter(retx, rety);
    }

    // clear any drag leave timer that might be pending so that it
    // doesn't get processed when we actually go out to get data.
    if (mDragLeaveTimer) {
        mDragLeaveTimer->Cancel();
        mDragLeaveTimer = 0;
    }

    // set the last window to this
    mLastDragMotionWindow = innerMostWidget;

    // What we do here is dispatch a new drag motion event to
    // re-validate the drag target and then we do the drop.  The events
    // look the same except for the type.

    innerMostWidget->AddRef();

    nsMouseEvent event(NS_DRAGDROP_OVER, innerMostWidget);

    InitDragEvent(event);

    // now that we have initialized the event update our drag status
    UpdateDragStatus(event, aDragContext, dragService);

    event.point.x = retx;
    event.point.y = rety;

    nsEventStatus status;
    innerMostWidget->DispatchEvent(&event, status);

    event.message = NS_DRAGDROP_DROP;
    event.widget = innerMostWidget;
    event.point.x = retx;
    event.point.y = rety;

    innerMostWidget->DispatchEvent(&event, status);

    innerMostWidget->Release();

    // before we unset the context we need to do a drop_finish

    gdk_drop_finish(aDragContext, TRUE, aTime);

    // after a drop takes place we need to make sure that the drag
    // service doesn't think that it still has a context.  if the other
    // way ( besides the drop ) to end a drag event is during the leave
    // event and and that case is handled in that handler.
    dragSessionGTK->TargetSetLastContext(0, 0, 0);

    // send our drag exit event
    innerMostWidget->OnDragLeave();
    // and clear the mLastDragMotion window
    mLastDragMotionWindow = 0;

    // and end our drag session
    dragService->EndDragSession();


    return TRUE;
}

void
nsWindow::OnDragDataReceivedEvent(GtkWidget *aWidget,
                                  GdkDragContext *aDragContext,
                                  gint aX,
                                  gint aY,
                                  GtkSelectionData  *aSelectionData,
                                  guint aInfo,
                                  guint aTime,
                                  gpointer aData)
{
    LOG(("nsWindow::OnDragDataReceived(%p)\n", this));

    // get our drag context
    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
    nsCOMPtr<nsIDragSessionGTK> dragSessionGTK = do_QueryInterface(dragService);

    dragSessionGTK->TargetDataReceived(aWidget, aDragContext, aX, aY,
                                       aSelectionData, aInfo, aTime);
}

void
nsWindow::OnDragLeave(void)
{
    LOG(("nsWindow::OnDragLeave(%p)\n", this));

    nsMouseEvent event(NS_DRAGDROP_EXIT, this);

    AddRef();

    nsEventStatus status;
    DispatchEvent(&event, status);

    Release();
}

void
nsWindow::OnDragEnter(nscoord aX, nscoord aY)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?

    LOG(("nsWindow::OnDragEnter(%p)\n", this));
    
    nsMouseEvent event(NS_DRAGDROP_ENTER, this);

    event.point.x = aX;
    event.point.y = aY;

    AddRef();

    nsEventStatus status;
    DispatchEvent(&event, status);

    Release();
}

nsresult
nsWindow::NativeCreate(nsIWidget        *aParent,
                       nsNativeWidget    aNativeParent,
                       const nsRect     &aRect,
                       EVENT_CALLBACK    aHandleEventFunction,
                       nsIDeviceContext *aContext,
                       nsIAppShell      *aAppShell,
                       nsIToolkit       *aToolkit,
                       nsWidgetInitData *aInitData)
{
    // only set the base parent if we're going to be a dialog or a
    // toplevel
    nsIWidget *baseParent = aInitData &&
        (aInitData->mWindowType == eWindowType_dialog ||
         aInitData->mWindowType == eWindowType_toplevel ||
         aInitData->mWindowType == eWindowType_invisible) ?
        nsnull : aParent;

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    // Do we need to listen for resizes?
    PRBool listenForResizes = PR_FALSE;;
    if (aNativeParent || (aInitData && aInitData->mListenForResizes))
        listenForResizes = PR_TRUE;

    // and do our common creation
    CommonCreate(aParent, listenForResizes);

    // save our bounds
    mBounds = aRect;

    // figure out our parent window
    MozDrawingarea *parentArea = nsnull;
    MozContainer   *parentMozContainer = nsnull;
    GtkContainer   *parentGtkContainer = nsnull;
    GdkWindow      *parentGdkWindow = nsnull;
    GtkWindow      *topLevelParent = nsnull;

    if (aParent)
        parentGdkWindow = GDK_WINDOW(aParent->GetNativeData(NS_NATIVE_WINDOW));
    else if (aNativeParent && GDK_IS_WINDOW(aNativeParent))
        parentGdkWindow = GDK_WINDOW(aNativeParent);
    else if (aNativeParent && GTK_IS_CONTAINER(aNativeParent))
        parentGtkContainer = GTK_CONTAINER(aNativeParent);

    if (parentGdkWindow) {
        // find the mozarea on that window
        gpointer user_data = nsnull;
        user_data = g_object_get_data(G_OBJECT(parentGdkWindow), 
                                      "mozdrawingarea");
        parentArea = MOZ_DRAWINGAREA(user_data);

        NS_ASSERTION(parentArea, "no drawingarea for parent widget!\n");
        if (!parentArea)
            return NS_ERROR_FAILURE;

        // get the user data for the widget - it should be a container
        user_data = nsnull;
        gdk_window_get_user_data(parentArea->inner_window, &user_data);
        NS_ASSERTION(user_data, "no user data for parentArea\n");
        if (!user_data)
            return NS_ERROR_FAILURE;

        // Get the parent moz container
        parentMozContainer = MOZ_CONTAINER(user_data);
        NS_ASSERTION(parentMozContainer,
                     "owning widget is not a mozcontainer!\n");
        if (!parentMozContainer)
            return NS_ERROR_FAILURE;

        // get the toplevel window just in case someone needs to use it
        // for setting transients or whatever.
        topLevelParent =
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(parentMozContainer)));
    }

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
    case eWindowType_popup:
    case eWindowType_toplevel:
    case eWindowType_invisible: {
        mIsTopLevel = PR_TRUE;
        if (mWindowType == eWindowType_dialog) {
            mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            SetDefaultIcon();
            gtk_window_set_type_hint(GTK_WINDOW(mShell),
                                     GDK_WINDOW_TYPE_HINT_DIALOG);
            gtk_window_set_transient_for(GTK_WINDOW(mShell),
                                         topLevelParent);
            mTransientParent = topLevelParent;
            // add ourselves to the parent window's window group
            if (!topLevelParent) {
                gtk_widget_realize(mShell);
                GdkWindow* dialoglead = mShell->window;
                gdk_window_set_group(dialoglead, dialoglead);
            }
            if (parentArea) {
                nsWindow *parentnsWindow =
                    get_window_for_gdk_window(parentArea->inner_window);
                NS_ASSERTION(parentnsWindow,
                             "no nsWindow for parentArea!");
                if (parentnsWindow && parentnsWindow->mWindowGroup) {
                    gtk_window_group_add_window(parentnsWindow->mWindowGroup,
                                                GTK_WINDOW(mShell));
                    // store this in case any children are created
                    mWindowGroup = parentnsWindow->mWindowGroup;
                    LOG(("adding window %p to group %p\n",
                         (void *)mShell, (void *)mWindowGroup));
                }
            }
        }
        else if (mWindowType == eWindowType_popup) {
            mShell = gtk_window_new(GTK_WINDOW_POPUP);
            if (topLevelParent) {
                gtk_window_set_transient_for(GTK_WINDOW(mShell), 
                                            topLevelParent);
                mTransientParent = topLevelParent;

                if (topLevelParent->group) {
                    gtk_window_group_add_window(topLevelParent->group,
                                            GTK_WINDOW(mShell));
                    mWindowGroup = topLevelParent->group;
                }
            }
        }
        else { // must be eWindowType_toplevel
            mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            SetDefaultIcon();

            // each toplevel window gets its own window group
            mWindowGroup = gtk_window_group_new();

            // and add ourselves to the window group
            LOG(("adding window %p to new group %p\n",
                 (void *)mShell, (void *)mWindowGroup));
            gtk_window_group_add_window(mWindowGroup, GTK_WINDOW(mShell));
        }

        // create our container
        mContainer = MOZ_CONTAINER(moz_container_new());
        gtk_container_add(GTK_CONTAINER(mShell), GTK_WIDGET(mContainer));
        gtk_widget_realize(GTK_WIDGET(mContainer));

        // make sure this is the focus widget in the container
        gtk_window_set_focus(GTK_WINDOW(mShell), GTK_WIDGET(mContainer));

        // and the drawing area
        mDrawingarea = moz_drawingarea_new(nsnull, mContainer);

        if (mWindowType == eWindowType_popup) {
            // gdk does not automatically set the cursor for "temporary"
            // windows, which are what gtk uses for popups.

            mCursor = eCursor_wait; // force SetCursor to actually set the
                                    // cursor, even though our internal state
                                    // indicates that we already have the
                                    // standard cursor.
            SetCursor(eCursor_standard);
        }
    }
        break;
    case eWindowType_child: {
        if (parentMozContainer) {
            mDrawingarea = moz_drawingarea_new(parentArea, parentMozContainer);
        }
        else if (parentGtkContainer) {
            mContainer = MOZ_CONTAINER(moz_container_new());
            gtk_container_add(parentGtkContainer, GTK_WIDGET(mContainer));
            gtk_widget_realize(GTK_WIDGET(mContainer));

            mDrawingarea = moz_drawingarea_new(nsnull, mContainer);
        }
        else {
            NS_WARNING("Warning: tried to create a new child widget with no parent!");
            return NS_ERROR_FAILURE;
        }
    }
        break;
    default:
        break;
    }
    // Disable the double buffer because it will make the caret crazy
    // For bug#153805 (Gtk2 double buffer makes carets misbehave) 
    if (mContainer)
        gtk_widget_set_double_buffered (GTK_WIDGET(mContainer),FALSE);

    // label the drawing area with this object so we can find our way
    // home
    g_object_set_data(G_OBJECT(mDrawingarea->clip_window), "nsWindow",
                      this);
    g_object_set_data(G_OBJECT(mDrawingarea->inner_window), "nsWindow",
                      this);

    g_object_set_data(G_OBJECT(mDrawingarea->clip_window), "mozdrawingarea",
                      mDrawingarea);
    g_object_set_data(G_OBJECT(mDrawingarea->inner_window), "mozdrawingarea",
                      mDrawingarea);

    if (mContainer)
        g_object_set_data(G_OBJECT(mContainer), "nsWindow", this);

    if (mShell)
        g_object_set_data(G_OBJECT(mShell), "nsWindow", this);

    // attach listeners for events
    if (mShell) {
        g_signal_connect(G_OBJECT(mShell), "configure_event",
                         G_CALLBACK(configure_event_cb), NULL);
        g_signal_connect(G_OBJECT(mShell), "delete_event",
                         G_CALLBACK(delete_event_cb), NULL);
        // we need to add this to the shell since versions of gtk
        // before 2.0.3 forgot to set property_notify events on the
        // shell window
        gtk_widget_add_events(mShell, GDK_PROPERTY_CHANGE_MASK);
        g_signal_connect(G_OBJECT(mShell), "window_state_event",
                         G_CALLBACK(window_state_event_cb), NULL);
        g_signal_connect(G_OBJECT(mShell), "property_notify_event",
                         G_CALLBACK(property_notify_event_cb), NULL);
    }

    if (mContainer) {
        g_signal_connect_after(G_OBJECT(mContainer), "size_allocate",
                               G_CALLBACK(size_allocate_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "expose_event",
                         G_CALLBACK(expose_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "enter_notify_event",
                         G_CALLBACK(enter_notify_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "leave_notify_event",
                         G_CALLBACK(leave_notify_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "motion_notify_event",
                         G_CALLBACK(motion_notify_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "button_press_event",
                         G_CALLBACK(button_press_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "button_release_event",
                         G_CALLBACK(button_release_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "focus_in_event",
                         G_CALLBACK(focus_in_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "focus_out_event",
                         G_CALLBACK(focus_out_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "key_press_event",
                         G_CALLBACK(key_press_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "key_release_event",
                         G_CALLBACK(key_release_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "scroll_event",
                         G_CALLBACK(scroll_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "visibility_notify_event",
                         G_CALLBACK(visibility_notify_event_cb), NULL);
        
        gtk_drag_dest_set((GtkWidget *)mContainer,
                          (GtkDestDefaults)0,
                          NULL,
                          0,
                          (GdkDragAction)0);

        g_signal_connect(G_OBJECT(mContainer), "drag_motion",
                         G_CALLBACK(drag_motion_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "drag_leave",
                         G_CALLBACK(drag_leave_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "drag_drop",
                         G_CALLBACK(drag_drop_event_cb), NULL);
        g_signal_connect(G_OBJECT(mContainer), "drag_data_received",
                         G_CALLBACK(drag_data_received_event_cb), NULL);

#ifdef USE_XIM
        // We create input contexts for all containers, except for
        // toplevel popup windows
        if (mWindowType != eWindowType_popup)
            IMECreateContext();
#endif
    }

    LOG(("nsWindow [%p]\n", (void *)this));
    if (mShell) {
        LOG(("\tmShell %p %p %lx\n", (void *)mShell, (void *)mShell->window,
             GDK_WINDOW_XWINDOW(mShell->window)));
    }

    if (mContainer) {
        LOG(("\tmContainer %p %p %lx\n", (void *)mContainer,
             (void *)GTK_WIDGET(mContainer)->window,
             GDK_WINDOW_XWINDOW(GTK_WIDGET(mContainer)->window)));
    }

    if (mDrawingarea) {
        LOG(("\tmDrawingarea %p %p %p %lx %lx\n", (void *)mDrawingarea,
             (void *)mDrawingarea->clip_window,
             (void *)mDrawingarea->inner_window,
             GDK_WINDOW_XWINDOW(mDrawingarea->clip_window),
             GDK_WINDOW_XWINDOW(mDrawingarea->inner_window)));
    }

    // resize so that everything is set to the right dimensions
    Resize(mBounds.width, mBounds.height, PR_FALSE);

#ifdef ACCESSIBILITY
    nsresult rv;
    if (!sAccessibilityChecked) {
        sAccessibilityChecked = PR_TRUE;

        //check if accessibility enabled/disabled by environment variable
        const char *envValue = PR_GetEnv(sAccEnv);
        if (envValue) {
            sAccessibilityEnabled = atoi(envValue);
            LOG(("Accessibility Env %s=%s\n", sAccEnv, envValue));
        }
        //check gconf-2 setting
        else {
            nsCOMPtr<nsIPrefBranch> sysPrefService =
                do_GetService(sSysPrefService, &rv);
            if (NS_SUCCEEDED(rv) && sysPrefService) {

                // do the work to get gconf setting.
                // will be done soon later.
                sysPrefService->GetBoolPref(sAccessibilityKey,
                                            &sAccessibilityEnabled);
            }

        }
    }
    if (sAccessibilityEnabled) {
        LOG(("nsWindow:: Create Toplevel Accessibility\n"));
        CreateRootAccessible();
    }
#endif

    return NS_OK;
}

void
nsWindow::NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool  aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));

    ResizeTransparencyBitmap(aWidth, aHeight);

    // clear our resize flag
    mNeedsResize = PR_FALSE;

    if (mIsTopLevel) {
        gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
    }
    else if (mContainer) {
        GtkAllocation allocation;
        allocation.x = 0;
        allocation.y = 0;
        allocation.width = aWidth;
        allocation.height = aHeight;
        gtk_widget_size_allocate(GTK_WIDGET(mContainer), &allocation);
    }

    moz_drawingarea_resize (mDrawingarea, aWidth, aHeight);
}

void
nsWindow::NativeResize(PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       PRBool  aRepaint)
{
    mNeedsResize = PR_FALSE;

    LOG(("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));

    ResizeTransparencyBitmap(aWidth, aHeight);

    if (mIsTopLevel) {
        if (mParent && mWindowType == eWindowType_popup) {
            nsRect oldrect, newrect;
            oldrect.x = aX;
            oldrect.y = aY;
            mParent->WidgetToScreen(oldrect, newrect);
            moz_drawingarea_resize(mDrawingarea, aWidth, aHeight);
            gtk_window_move(GTK_WINDOW(mShell), newrect.x, newrect.y);
            gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
        }
        else {
            // We only move the toplevel window if someone has
            // actually placed the window somewhere.  If no placement
            // has taken place, we just let the window manager Do The
            // Right Thing.
            if (mPlaced)
                gtk_window_move(GTK_WINDOW(mShell), aX, aY);

            gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
            moz_drawingarea_resize(mDrawingarea, aWidth, aHeight);
        }
    }
    else if (mContainer) {
        GtkAllocation allocation;
        allocation.x = 0;
        allocation.y = 0;
        allocation.width = aWidth;
        allocation.height = aHeight;
        gtk_widget_size_allocate(GTK_WIDGET(mContainer), &allocation);
        moz_drawingarea_move_resize(mDrawingarea, aX, aY, aWidth, aHeight);
    }
    else if (mDrawingarea) {
        moz_drawingarea_move_resize(mDrawingarea, aX, aY, aWidth, aHeight);
    }
}

void
nsWindow::NativeShow (PRBool  aAction)
{
    if (aAction) {
        // GTK wants us to set the window mask before we show the window
        // for the first time, or setting the mask later won't work.
        // GTK also wants us to NOT set the window mask if we're not really
        // going to need it, because GTK won't let us unset the mask properly
        // later.
        // So, we delay setting the mask until the last moment: when the window
        // is shown.
        // XXX that may or may not be true for GTK+ 2.x
        if (mTransparencyBitmap) {
          ApplyTransparencyBitmap();
        }

        // unset our flag now that our window has been shown
        mNeedsShow = PR_FALSE;

        if (mIsTopLevel) {
            moz_drawingarea_set_visibility(mDrawingarea, aAction);
            gtk_widget_show(GTK_WIDGET(mContainer));
            gtk_widget_show(mShell);
        }
        else if (mContainer) {
            moz_drawingarea_set_visibility(mDrawingarea, TRUE);
            gtk_widget_show(GTK_WIDGET(mContainer));
        }
        else if (mDrawingarea) {
            moz_drawingarea_set_visibility(mDrawingarea, TRUE);
        }
    }
    else {
        if (mIsTopLevel) {
            gtk_widget_hide(GTK_WIDGET(mShell));
            gtk_widget_hide(GTK_WIDGET(mContainer));
        }
        else if (mContainer) {
            gtk_widget_hide(GTK_WIDGET(mContainer));
            moz_drawingarea_set_visibility(mDrawingarea, FALSE);
        }
        if (mDrawingarea) {
            moz_drawingarea_set_visibility(mDrawingarea, FALSE);
        }
    }
}

void
nsWindow::EnsureGrabs(void)
{
    if (mRetryPointerGrab)
        GrabPointer();
    if (mRetryKeyboardGrab)
        GrabKeyboard();
}

#ifndef MOZ_XUL
void
nsWindow::ResizeTransparencyBitmap(PRInt32 aNewWidth, PRInt32 aNewHeight)
{
}

void
nsWindow::ApplyTransparencyBitmap()
{
}
#else
NS_IMETHODIMP
nsWindow::SetWindowTranslucency(PRBool aTranslucent)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = nsnull;
        GetToplevelWidget(&topWidget);
        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        return topWindow->SetWindowTranslucency(aTranslucent);
    }

    if (mIsTranslucent == aTranslucent)
        return NS_OK;

    if (!aTranslucent) {
        if (mTransparencyBitmap) {
            delete[] mTransparencyBitmap;
            mTransparencyBitmap = nsnull;
            gtk_widget_reset_shapes(mShell);
        }
    } // else the new default alpha values are "all 1", so we don't
    // need to change anything yet

    mIsTranslucent = aTranslucent;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetWindowTranslucency(PRBool& aTranslucent)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = nsnull;
        GetToplevelWidget(&topWidget);
        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        return topWindow->GetWindowTranslucency(aTranslucent);
    }

    aTranslucent = mIsTranslucent;
    return NS_OK;
}

void
nsWindow::ResizeTransparencyBitmap(PRInt32 aNewWidth, PRInt32 aNewHeight)
{
    if (!mTransparencyBitmap)
        return;

    PRInt32 newSize = ((aNewWidth+7)/8)*aNewHeight;
    gchar* newBits = new gchar[newSize];
    if (!newBits) {
        delete[] mTransparencyBitmap;
        mTransparencyBitmap = nsnull;
        return;
    }
    // fill new mask with "opaque", first
    memset(newBits, 255, newSize);

    // Now copy the intersection of the old and new areas into the new mask
    PRInt32 copyWidth = PR_MIN(aNewWidth, mBounds.width);
    PRInt32 copyHeight = PR_MIN(aNewHeight, mBounds.height);
    PRInt32 oldRowBytes = (mBounds.width+7)/8;
    PRInt32 newRowBytes = (aNewWidth+7)/8;
    PRInt32 copyBytes = (copyWidth+7)/8;

    PRInt32 i;
    gchar* fromPtr = mTransparencyBitmap;
    gchar* toPtr = newBits;
    for (i = 0; i < copyHeight; i++) {
        memcpy(toPtr, fromPtr, copyBytes);
        fromPtr += oldRowBytes;
        toPtr += newRowBytes;
    }

    delete[] mTransparencyBitmap;
    mTransparencyBitmap = newBits;
}

static PRBool
ChangedMaskBits(gchar* aMaskBits, PRInt32 aMaskWidth, PRInt32 aMaskHeight,
        const nsRect& aRect, PRUint8* aAlphas)
{
    PRInt32 x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    PRInt32 maskBytesPerRow = (aMaskWidth + 7)/8;
    for (y = aRect.y; y < yMax; y++) {
        gchar* maskBytes = aMaskBits + y*maskBytesPerRow;
        for (x = aRect.x; x < xMax; x++) {
            PRBool newBit = *aAlphas > 0;
            aAlphas++;

            gchar maskByte = maskBytes[x >> 3];
            PRBool maskBit = (maskByte & (1 << (x & 7))) != 0;

            if (maskBit != newBit) {
                return PR_TRUE;
            }
        }
    }

    return PR_FALSE;
}

static
void UpdateMaskBits(gchar* aMaskBits, PRInt32 aMaskWidth, PRInt32 aMaskHeight,
        const nsRect& aRect, PRUint8* aAlphas)
{
    PRInt32 x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    PRInt32 maskBytesPerRow = (aMaskWidth + 7)/8;
    for (y = aRect.y; y < yMax; y++) {
        gchar* maskBytes = aMaskBits + y*maskBytesPerRow;
        for (x = aRect.x; x < xMax; x++) {
            PRBool newBit = *aAlphas > 0;
            aAlphas++;

            gchar mask = 1 << (x & 7);
            gchar maskByte = maskBytes[x >> 3];
            // Note: '-newBit' turns 0 into 00...00 and 1 into 11...11
            maskBytes[x >> 3] = (maskByte & ~mask) | (-newBit & mask);
        }
    }
}

void
nsWindow::ApplyTransparencyBitmap()
{
    gtk_widget_reset_shapes(mShell);
    GdkBitmap* maskBitmap = gdk_bitmap_create_from_data(mShell->window,
            mTransparencyBitmap,
            mBounds.width, mBounds.height);
    if (!maskBitmap)
        return;

    gtk_widget_shape_combine_mask(mShell, maskBitmap, 0, 0);
    gdk_bitmap_unref(maskBitmap);
}

NS_IMETHODIMP
nsWindow::UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = nsnull;
        GetToplevelWidget(&topWidget);
        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        return topWindow->UpdateTranslucentWindowAlpha(aRect, aAlphas);
    }

    NS_ASSERTION(mIsTranslucent, "Window is not transparent");

    if (mTransparencyBitmap == nsnull) {
        PRInt32 size = ((mBounds.width+7)/8)*mBounds.height;
        mTransparencyBitmap = new gchar[size];
        if (mTransparencyBitmap == nsnull)
            return NS_ERROR_FAILURE;
        memset(mTransparencyBitmap, 255, size);
    }

    NS_ASSERTION(aRect.x >= 0 && aRect.y >= 0
            && aRect.XMost() <= mBounds.width && aRect.YMost() <= mBounds.height,
            "Rect is out of window bounds");

    if (!ChangedMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height, aRect, aAlphas))
        // skip the expensive stuff if the mask bits haven't changed; hopefully
        // this is the common case
        return NS_OK;

    UpdateMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height, aRect, aAlphas);

    if (!mNeedsShow) {
        ApplyTransparencyBitmap();
    }

    return NS_OK;
}
#endif

void
nsWindow::GrabPointer(void)
{
    LOG(("GrabPointer %d\n", mRetryPointerGrab));

    mRetryPointerGrab = PR_FALSE;

    // If the window isn't visible, just set the flag to retry the
    // grab.  When this window becomes visible, the grab will be
    // retried.
    PRBool visibility = PR_TRUE;
    IsVisible(visibility);
    if (!visibility) {
        LOG(("GrabPointer: window not visible\n"));
        mRetryPointerGrab = PR_TRUE;
        return;
    }

    if (!mDrawingarea)
        return;

    gint retval;
    retval = gdk_pointer_grab(mDrawingarea->inner_window, TRUE,
                              (GdkEventMask)(GDK_BUTTON_PRESS_MASK |
                                             GDK_BUTTON_RELEASE_MASK |
                                             GDK_ENTER_NOTIFY_MASK |
                                             GDK_LEAVE_NOTIFY_MASK |
                                             GDK_POINTER_MOTION_MASK),
                              (GdkWindow *)NULL, NULL, GDK_CURRENT_TIME);

    if (retval != GDK_GRAB_SUCCESS) {
        LOG(("GrabPointer: pointer grab failed\n"));
        mRetryPointerGrab = PR_TRUE;
    }
}

void
nsWindow::GrabKeyboard(void)
{
    LOG(("GrabKeyboard %d\n", mRetryKeyboardGrab));

    mRetryKeyboardGrab = PR_FALSE;

    // If the window isn't visible, just set the flag to retry the
    // grab.  When this window becomes visible, the grab will be
    // retried.
    PRBool visibility = PR_TRUE;
    IsVisible(visibility);
    if (!visibility) {
        LOG(("GrabKeyboard: window not visible\n"));
        mRetryKeyboardGrab = PR_TRUE;
        return;
    }

    // we need to grab the keyboard on the transient parent so that we
    // don't end up with any focus events that end up on the parent
    // window that will cause the popup to go away
    GdkWindow *grabWindow;

    if (mTransientParent)
        grabWindow = GTK_WIDGET(mTransientParent)->window;
    else if (mDrawingarea)
        grabWindow = mDrawingarea->inner_window;
    else
        return;

    gint retval;
    retval = gdk_keyboard_grab(grabWindow, TRUE, GDK_CURRENT_TIME);

    if (retval != GDK_GRAB_SUCCESS) {
        LOG(("GrabKeyboard: keyboard grab failed %d\n", retval));
        gdk_pointer_ungrab(GDK_CURRENT_TIME);
        mRetryKeyboardGrab = PR_TRUE;
    }
}

void
nsWindow::ReleaseGrabs(void)
{
    LOG(("ReleaseGrabs\n"));

    mRetryPointerGrab = PR_FALSE;
    mRetryKeyboardGrab = PR_FALSE;

    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
}

void
nsWindow::GetToplevelWidget(GtkWidget **aWidget)
{
    *aWidget = nsnull;

    if (mShell) {
        *aWidget = mShell;
        return;
    }

    if (!mDrawingarea) 
        return;

    GtkWidget *widget =
        get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);
    if (!widget)
        return;

    *aWidget = gtk_widget_get_toplevel(widget);
}

void
nsWindow::GetContainerWindow(nsWindow **aWindow)
{
    if (!mDrawingarea) 
        return;

    GtkWidget *owningWidget =
        get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);

    *aWindow = get_window_for_gtk_widget(owningWidget);
}

void *
nsWindow::SetupPluginPort(void)
{
    if (!mDrawingarea)
        return nsnull;

    if (GDK_WINDOW_OBJECT(mDrawingarea->inner_window)->destroyed == TRUE)
        return nsnull;

    // we have to flush the X queue here so that any plugins that
    // might be running on separate X connections will be able to use
    // this window in case it was just created
    XWindowAttributes xattrs;
    XGetWindowAttributes(GDK_DISPLAY (),
                         GDK_WINDOW_XWINDOW(mDrawingarea->inner_window),
                         &xattrs);
    XSelectInput (GDK_DISPLAY (),
                  GDK_WINDOW_XWINDOW(mDrawingarea->inner_window),
                  xattrs.your_event_mask |
                  SubstructureNotifyMask);

    gdk_window_add_filter(mDrawingarea->inner_window, 
                          plugin_window_filter_func,
                          this);

    XSync(GDK_DISPLAY(), False);

    return (void *)GDK_WINDOW_XWINDOW(mDrawingarea->inner_window);
}

nsresult
nsWindow::SetWindowIconList(const nsCStringArray &aIconList)
{
    GList *list = NULL;

    for (int i = 0; i < aIconList.Count(); ++i) {
        const char *path = aIconList[i]->get();
        LOG(("window [%p] Loading icon from %s\n", (void *)this, path));

        GdkPixbuf *icon = gdk_pixbuf_new_from_file(path, NULL);
        if (!icon)
            continue;

        list = g_list_append(list, icon);
    }

    if (!list)
        return NS_ERROR_FAILURE;

    gtk_window_set_icon_list(GTK_WINDOW(mShell), list);

    g_list_foreach(list, (GFunc) g_object_unref, NULL);
    g_list_free(list);

    return NS_OK;
}

void
nsWindow::SetDefaultIcon(void)
{
    // Set up the default window icon
    nsresult rv;
    nsCOMPtr<nsIFile> chromeDir;
    rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR,
                                getter_AddRefs(chromeDir));
    if (NS_FAILED(rv))
        return;

    nsAutoString defaultPath;
    chromeDir->GetPath(defaultPath);
            
    defaultPath.AppendLiteral("/icons/default/default.xpm");

    nsCOMPtr<nsILocalFile> defaultPathConverter;
    rv = NS_NewLocalFile(defaultPath, PR_TRUE,
                         getter_AddRefs(defaultPathConverter));
    if (NS_FAILED(rv))
        return;

    nsCAutoString path;
    defaultPathConverter->GetNativePath(path);

    nsCStringArray iconList;
    iconList.AppendCString(path);

    SetWindowIconList(iconList);
}

void
nsWindow::SetPluginType(PluginType aPluginType)
{
    mPluginType = aPluginType;
}

void
nsWindow::SetNonXEmbedPluginFocus()
{
    if (gPluginFocusWindow == this || mPluginType!=PluginType_NONXEMBED) {
        return;
    }

    if (gPluginFocusWindow) {
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }

    LOGFOCUS(("nsWindow::SetNonXEmbedPluginFocus\n"));

    Window curFocusWindow;
    int focusState;

    XGetInputFocus(GDK_WINDOW_XDISPLAY(mDrawingarea->inner_window),
                   &curFocusWindow,
                   &focusState);

    LOGFOCUS(("\t curFocusWindow=%p\n", curFocusWindow));

    GdkWindow* toplevel = gdk_window_get_toplevel
                                (mDrawingarea->inner_window);
    GdkWindow *gdkfocuswin = gdk_window_lookup(curFocusWindow);

    // lookup with the focus proxy window is supposed to get the
    // same GdkWindow as toplevel. If the current focused window
    // is not the focus proxy, we return without any change.
    if (gdkfocuswin != toplevel) {
        return;
    }

    // switch the focus from the focus proxy to the plugin window
    mOldFocusWindow = curFocusWindow;
    XRaiseWindow(GDK_WINDOW_XDISPLAY(mDrawingarea->inner_window),
                 GDK_WINDOW_XWINDOW(mDrawingarea->inner_window));
    gdk_error_trap_push();
    XSetInputFocus(GDK_WINDOW_XDISPLAY(mDrawingarea->inner_window),
                   GDK_WINDOW_XWINDOW(mDrawingarea->inner_window),
                   RevertToNone,
                   CurrentTime);
    gdk_flush();
    gdk_error_trap_pop();
    gPluginFocusWindow = this;
    gdk_window_add_filter(NULL, plugin_client_message_filter, this);
    
    LOGFOCUS(("nsWindow::SetNonXEmbedPluginFocus oldfocus=%p new=%p\n",
                mOldFocusWindow, 
                GDK_WINDOW_XWINDOW(mDrawingarea->inner_window)));
}

void
nsWindow::LoseNonXEmbedPluginFocus()
{
    LOGFOCUS(("nsWindow::LoseNonXEmbedPluginFocus\n"));

    // This method is only for the nsWindow which contains a
    // Non-XEmbed plugin, for example, JAVA plugin.
    if (gPluginFocusWindow != this || mPluginType!=PluginType_NONXEMBED) {
        return;
    }

    Window curFocusWindow;
    int focusState;

    XGetInputFocus(GDK_WINDOW_XDISPLAY(mDrawingarea->inner_window),
                   &curFocusWindow,
                   &focusState);

    // we only switch focus between plugin window and focus proxy. If the 
    // current focused window is not the plugin window, just removing the 
    // event filter that blocks the WM_TAKE_FOCUS is enough. WM and gtk2
    // will take care of the focus later.
    if (!curFocusWindow || 
        curFocusWindow == GDK_WINDOW_XWINDOW(mDrawingarea->inner_window)) {

        gdk_error_trap_push();
        XRaiseWindow(GDK_WINDOW_XDISPLAY(mDrawingarea->inner_window),
                     mOldFocusWindow);
        XSetInputFocus(GDK_WINDOW_XDISPLAY(mDrawingarea->inner_window),
                       mOldFocusWindow,
                       RevertToParent,
                       CurrentTime);
        gdk_flush();
        gdk_error_trap_pop();
    }
    gPluginFocusWindow = NULL;
    mOldFocusWindow = 0;
    gdk_window_remove_filter(NULL, plugin_client_message_filter, this);

    LOGFOCUS(("nsWindow::LoseNonXEmbedPluginFocus end\n"));
}


gint
nsWindow::ConvertBorderStyles(nsBorderStyle aStyle)
{
    gint w = 0;

    if (aStyle == eBorderStyle_default)
        return -1;

    if (aStyle & eBorderStyle_all)
        w |= GDK_DECOR_ALL;
    if (aStyle & eBorderStyle_border)
        w |= GDK_DECOR_BORDER;
    if (aStyle & eBorderStyle_resizeh)
        w |= GDK_DECOR_RESIZEH;
    if (aStyle & eBorderStyle_title)
        w |= GDK_DECOR_TITLE;
    if (aStyle & eBorderStyle_menu)
        w |= GDK_DECOR_MENU;
    if (aStyle & eBorderStyle_minimize)
        w |= GDK_DECOR_MINIMIZE;
    if (aStyle & eBorderStyle_maximize)
        w |= GDK_DECOR_MAXIMIZE;
    if (aStyle & eBorderStyle_close) {
#ifdef DEBUG
        printf("we don't handle eBorderStyle_close yet... please fix me\n");
#endif /* DEBUG */
    }

    return w;
}

NS_IMETHODIMP
nsWindow::HideWindowChrome(PRBool aShouldHide)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = nsnull;
        GetToplevelWidget(&topWidget);
        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        return topWindow->HideWindowChrome(aShouldHide);
    }

    // Sawfish, metacity, and presumably other window managers get
    // confused if we change the window decorations while the window
    // is visible.
#if GTK_CHECK_VERSION(2,2,0)
    if (aShouldHide) 
        gdk_window_fullscreen (mShell->window);
    else 
        gdk_window_unfullscreen (mShell->window);
#else
    gdk_window_hide(mShell->window);

    gint wmd;
    if (aShouldHide)
        wmd = 0;
    else
        wmd = ConvertBorderStyles(mBorderStyle);

    gdk_window_set_decorations(mShell->window, (GdkWMDecoration) wmd);

    gdk_window_show(mShell->window);
#endif

    // For some window managers, adding or removing window decorations
    // requires unmapping and remapping our toplevel window.  Go ahead
    // and flush the queue here so that we don't end up with a BadWindow
    // error later when this happens (when the persistence timer fires
    // and GetWindowPos is called)
    XSync(GDK_DISPLAY(), False);

    return NS_OK;
}

PRBool
check_for_rollup(GdkWindow *aWindow, gdouble aMouseX, gdouble aMouseY,
                 PRBool aIsWheel)
{
    PRBool retVal = PR_FALSE;
    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);

    if (rollupWidget && gRollupListener) {
        GdkWindow *currentPopup =
            (GdkWindow *)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);
        if (!is_mouse_in_window(currentPopup, aMouseX, aMouseY)) {
            PRBool rollup = PR_TRUE;
            if (aIsWheel) {
                gRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);
                retVal = PR_TRUE;
            }
            // if we're dealing with menus, we probably have submenus and
            // we don't want to rollup if the clickis in a parent menu of
            // the current submenu
            nsCOMPtr<nsIMenuRollup> menuRollup;
            menuRollup = (do_QueryInterface(gRollupListener));
            if (menuRollup) {
                nsCOMPtr<nsISupportsArray> widgetChain;
                menuRollup->GetSubmenuWidgetChain(getter_AddRefs(widgetChain));
                if (widgetChain) {
                    PRUint32 count = 0;
                    widgetChain->Count(&count);
                    for (PRUint32 i=0; i<count; ++i) {
                        nsCOMPtr<nsISupports> genericWidget;
                        widgetChain->GetElementAt(i,
                                                  getter_AddRefs(genericWidget));
                        nsCOMPtr<nsIWidget> widget(do_QueryInterface(genericWidget));
                        if (widget) {
                            GdkWindow* currWindow =
                                (GdkWindow*) widget->GetNativeData(NS_NATIVE_WINDOW);
                            if (is_mouse_in_window(currWindow, aMouseX, aMouseY)) {
                                rollup = PR_FALSE;
                                break;
                            }
                        }
                    } // foreach parent menu widget
                }
            } // if rollup listener knows about menus

            // if we've determined that we should still rollup, do it.
            if (rollup) {
                gRollupListener->Rollup();
                retVal = PR_TRUE;
            }
        }
    } else {
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
    }

    return retVal;
}

/* static */
PRBool
nsWindow::DragInProgress(void)
{
    // mLastDragMotionWindow means the drag arrow is over mozilla
    // sIsDraggingOutOf means the drag arrow is out of mozilla
    // both cases mean the dragging is happenning.
    return (mLastDragMotionWindow || sIsDraggingOutOf);
}

/* static */
PRBool
is_mouse_in_window (GdkWindow* aWindow, gdouble aMouseX, gdouble aMouseY)
{
    gint x = 0;
    gint y = 0;
    gint w, h;

    gint offsetX = 0;
    gint offsetY = 0;

    GtkWidget *widget;
    GdkWindow *window;

    window = aWindow;

    while (window) {
        gint tmpX = 0;
        gint tmpY = 0;

        gdk_window_get_position(window, &tmpX, &tmpY);
        widget = get_gtk_widget_for_gdk_window(window);

        // if this is a window, compute x and y given its origin and our
        // offset
        if (GTK_IS_WINDOW(widget)) {
            x = tmpX + offsetX;
            y = tmpY + offsetY;
            break;
        }

        offsetX += tmpX;
        offsetY += tmpY;
        window = gdk_window_get_parent(window);
    }

    gdk_window_get_size(aWindow, &w, &h);

    if (aMouseX > x && aMouseX < x + w &&
        aMouseY > y && aMouseY < y + h)
        return PR_TRUE;

    return PR_FALSE;
}

/* static */
nsWindow *
get_window_for_gtk_widget(GtkWidget *widget)
{
    gpointer user_data;
    user_data = g_object_get_data(G_OBJECT(widget), "nsWindow");

    if (!user_data)
        return nsnull;

    return NS_STATIC_CAST(nsWindow *, user_data);
}

/* static */
nsWindow *
get_window_for_gdk_window(GdkWindow *window)
{
    gpointer user_data;
    user_data = g_object_get_data(G_OBJECT(window), "nsWindow");

    if (!user_data)
        return nsnull;

    return NS_STATIC_CAST(nsWindow *, user_data);
}

/* static */
nsWindow *
get_owning_window_for_gdk_window(GdkWindow *window)
{
    GtkWidget *owningWidget = get_gtk_widget_for_gdk_window(window);
    if (!owningWidget)
        return nsnull;

    gpointer user_data;
    user_data = g_object_get_data(G_OBJECT(owningWidget), "nsWindow");

    if (!user_data)
        return nsnull;

    return (nsWindow *)user_data;
}

/* static */
GtkWidget *
get_gtk_widget_for_gdk_window(GdkWindow *window)
{
    gpointer user_data = NULL;
    gdk_window_get_user_data(window, &user_data);
    if (!user_data)
        return NULL;

    return GTK_WIDGET(user_data);
}

/* static */
GdkCursor *
get_gtk_cursor(nsCursor aCursor)
{
    GdkPixmap *cursor;
    GdkPixmap *mask;
    GdkColor fg, bg;
    GdkCursor *gdkcursor = nsnull;
    PRUint8 newType = 0xff;

    if ((gdkcursor = gCursorCache[aCursor])) {
        return gdkcursor;
    }

    switch (aCursor) {
    case eCursor_standard:
        gdkcursor = gdk_cursor_new(GDK_LEFT_PTR);
        break;
    case eCursor_wait:
        gdkcursor = gdk_cursor_new(GDK_WATCH);
        break;
    case eCursor_select:
        gdkcursor = gdk_cursor_new(GDK_XTERM);
        break;
    case eCursor_hyperlink:
        gdkcursor = gdk_cursor_new(GDK_HAND2);
        break;
    case eCursor_sizeWE:
        /* GDK_SB_H_DOUBLE_ARROW <==>.  The ideal choice is: =>||<= */
        gdkcursor = gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW);
        break;
    case eCursor_sizeNS:
        /* Again, should be =>||<= rotated 90 degrees. */
        gdkcursor = gdk_cursor_new(GDK_SB_V_DOUBLE_ARROW);
        break;
    case eCursor_sizeNW:
        gdkcursor = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
        break;
    case eCursor_sizeSE:
        gdkcursor = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
        break;
    case eCursor_sizeNE:
        gdkcursor = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
        break;
    case eCursor_sizeSW:
        gdkcursor = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
        break;
    case eCursor_arrow_north:
    case eCursor_arrow_north_plus:
        gdkcursor = gdk_cursor_new(GDK_TOP_SIDE);
        break;
    case eCursor_arrow_south:
    case eCursor_arrow_south_plus:
        gdkcursor = gdk_cursor_new(GDK_BOTTOM_SIDE);
        break;
    case eCursor_arrow_west:
    case eCursor_arrow_west_plus:
        gdkcursor = gdk_cursor_new(GDK_LEFT_SIDE);
        break;
    case eCursor_arrow_east:
    case eCursor_arrow_east_plus:
        gdkcursor = gdk_cursor_new(GDK_RIGHT_SIDE);
        break;
    case eCursor_crosshair:
        gdkcursor = gdk_cursor_new(GDK_CROSSHAIR);
        break;
    case eCursor_move:
        gdkcursor = gdk_cursor_new(GDK_FLEUR);
        break;
    case eCursor_help:
        newType = MOZ_CURSOR_QUESTION_ARROW;
        break;
    case eCursor_copy: // CSS3
        newType = MOZ_CURSOR_COPY;
        break;
    case eCursor_alias:
        newType = MOZ_CURSOR_ALIAS;
        break;
    case eCursor_context_menu:
        newType = MOZ_CURSOR_CONTEXT_MENU;
        break;
    case eCursor_cell:
        gdkcursor = gdk_cursor_new(GDK_PLUS);
        break;
    case eCursor_grab:
        newType = MOZ_CURSOR_HAND_GRAB;
        break;
    case eCursor_grabbing:
        newType = MOZ_CURSOR_HAND_GRABBING;
        break;
    case eCursor_spinning:
        newType = MOZ_CURSOR_SPINNING;
        break;
    case eCursor_count_up:
    case eCursor_count_down:
    case eCursor_count_up_down:
        // XXX: these CSS3 cursors need to be implemented
        gdkcursor = gdk_cursor_new(GDK_LEFT_PTR);
        break;
    case eCursor_zoom_in:
        newType = MOZ_CURSOR_ZOOM_IN;
        break;
    case eCursor_zoom_out:
        newType = MOZ_CURSOR_ZOOM_OUT;
        break;
    default:
        NS_ASSERTION(aCursor, "Invalid cursor type");
        break;
    }

    // if by now we dont have a xcursor, this means we have to make a
    // custom one
    if (!gdkcursor) {
        NS_ASSERTION(newType != 0xff,
                     "Unknown cursor type and no standard cursor");

        gdk_color_parse("#000000", &fg);
        gdk_color_parse("#ffffff", &bg);

        cursor = gdk_bitmap_create_from_data(NULL,
                                             (char *)GtkCursors[newType].bits,
                                             32, 32);
        mask =
            gdk_bitmap_create_from_data(NULL,
                                        (char *)GtkCursors[newType].mask_bits,
                                        32, 32);

        gdkcursor = gdk_cursor_new_from_pixmap(cursor, mask, &fg, &bg,
                                               GtkCursors[newType].hot_x,
                                               GtkCursors[newType].hot_y);

        gdk_bitmap_unref(mask);
        gdk_bitmap_unref(cursor);
    }

    gCursorCache[aCursor] = gdkcursor;

    return gdkcursor;
}

// gtk callbacks

/* static */
gboolean
expose_event_cb(GtkWidget *widget, GdkEventExpose *event)
{
    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return FALSE;

    // XXX We are so getting lucky here.  We are doing all of
    // mozilla's painting and then allowing default processing to occur.
    // This means that Mozilla paints in all of it's stuff and then
    // NO_WINDOW widgets (like scrollbars, for example) are painted by
    // Gtk on top of what we painted.

    // This return window->OnExposeEvent(widget, event); */

    window->OnExposeEvent(widget, event);
    return FALSE;
}

/* static */
gboolean
configure_event_cb(GtkWidget *widget,
                   GdkEventConfigure *event)
{
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    return window->OnConfigureEvent(widget, event);
}

/* static */
void
size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation)
{
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return;

    window->OnSizeAllocate(widget, allocation);
}

/* static */
gboolean
delete_event_cb(GtkWidget *widget, GdkEventAny *event)
{
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnDeleteEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
enter_notify_event_cb (GtkWidget *widget,
                       GdkEventCrossing *event)
{
    if (is_parent_ungrab_enter(event)) {
        return TRUE;
    }

    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return TRUE;

    window->OnEnterNotifyEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
leave_notify_event_cb (GtkWidget *widget,
                       GdkEventCrossing *event)
{
    if (is_parent_grab_leave(event)) {
        return TRUE;
    }

    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return TRUE;

    window->OnLeaveNotifyEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event)
{
    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return TRUE;

    window->OnMotionNotifyEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
button_press_event_cb   (GtkWidget *widget, GdkEventButton *event)
{
    LOG(("button_press_event_cb\n"));
    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return TRUE;

    window->OnButtonPressEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
button_release_event_cb (GtkWidget *widget, GdkEventButton *event)
{
    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return TRUE;

    window->OnButtonReleaseEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
focus_in_event_cb (GtkWidget *widget, GdkEventFocus *event)
{
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnContainerFocusInEvent(widget, event);

    return FALSE;
}

/* static */
gboolean
focus_out_event_cb (GtkWidget *widget, GdkEventFocus *event)
{
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnContainerFocusOutEvent(widget, event);

    return FALSE;
}

/* static */
GdkFilterReturn
plugin_window_filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
    GtkWidget *widget;
    GdkWindow *plugin_window;
    gpointer  user_data;
    XEvent    *xevent;

    nsWindow  *nswindow = (nsWindow*)data;
    GdkFilterReturn return_val;
      
    xevent = (XEvent *)gdk_xevent;
    return_val = GDK_FILTER_CONTINUE;

    switch (xevent->type)
    {
        case CreateNotify:
        case ReparentNotify:
            if (xevent->type==CreateNotify) {
                plugin_window = gdk_window_lookup(xevent->xcreatewindow.window);
            }
            else {
                if (xevent->xreparent.event != xevent->xreparent.parent)
                    break;
                plugin_window = gdk_window_lookup (xevent->xreparent.window);
            }
            if (plugin_window) {
                user_data = nsnull;
                gdk_window_get_user_data(plugin_window, &user_data);
                widget = GTK_WIDGET(user_data);

                if (GTK_IS_XTBIN(widget)) {
                    nswindow->SetPluginType(nsWindow::PluginType_NONXEMBED);
                    break;
                }
                else if(GTK_IS_SOCKET(widget)) {
                    nswindow->SetPluginType(nsWindow::PluginType_XEMBED);
                    break;
                }
            }
            nswindow->SetPluginType(nsWindow::PluginType_NONXEMBED);
            return_val = GDK_FILTER_REMOVE;
            break;
        case EnterNotify:
            nswindow->SetNonXEmbedPluginFocus();
            break;
        case DestroyNotify:
            gdk_window_remove_filter
                ((GdkWindow*)(nswindow->GetNativeData(NS_NATIVE_WINDOW)),
                 plugin_window_filter_func,
                 nswindow);
            // Currently we consider all plugins are non-xembed and calls
            // LoseNonXEmbedPluginFocus without any checking.
            nswindow->LoseNonXEmbedPluginFocus();
            break;
        default:
            break;
    }
    return return_val;
}

/* static */
GdkFilterReturn   
plugin_client_message_filter (GdkXEvent *gdk_xevent,
                              GdkEvent *event,
                              gpointer data)
{
    XEvent    *xevent;
    xevent = (XEvent *)gdk_xevent;

    GdkFilterReturn return_val;
    return_val = GDK_FILTER_CONTINUE;

    if (!gPluginFocusWindow || xevent->type!=ClientMessage) {
        return return_val;
    }

    // When WM sends out WM_TAKE_FOCUS, gtk2 will use XSetInputFocus
    // to set the focus to the focus proxy. To prevent this happen
    // while the focus is on the plugin, we filter the WM_TAKE_FOCUS
    // out.
    Display *dpy ;
    dpy = GDK_WINDOW_XDISPLAY((GdkWindow*)(gPluginFocusWindow->
                GetNativeData(NS_NATIVE_WINDOW)));
    if (gdk_x11_get_xatom_by_name("WM_PROTOCOLS") 
            != xevent->xclient.message_type) {
        return return_val;
    }
    
    if ((Atom) xevent->xclient.data.l[0] == 
            gdk_x11_get_xatom_by_name("WM_TAKE_FOCUS")) {
        // block it from gtk2.0 focus proxy
        return_val = GDK_FILTER_REMOVE;
    }

    return return_val;
}

/* static */
gboolean
key_press_event_cb (GtkWidget *widget, GdkEventKey *event)
{
    LOG(("key_press_event_cb\n"));
    // find the window with focus and dispatch this event to that widget
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    nsWindow *focusWindow = gFocusWindow ? gFocusWindow : window;

    return focusWindow->OnKeyPressEvent(widget, event);
}

gboolean
key_release_event_cb (GtkWidget *widget, GdkEventKey *event)
{
    LOG(("key_release_event_cb\n"));
    // find the window with focus and dispatch this event to that widget
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    nsWindow *focusWindow = gFocusWindow ? gFocusWindow : window;

    return focusWindow->OnKeyReleaseEvent(widget, event);
}

/* static */
gboolean
scroll_event_cb (GtkWidget *widget, GdkEventScroll *event)
{
    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return FALSE;

    window->OnScrollEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
visibility_notify_event_cb (GtkWidget *widget, GdkEventVisibility *event)
{
    nsWindow *window = get_window_for_gdk_window(event->window);
    if (!window)
        return FALSE;

    window->OnVisibilityNotifyEvent(widget, event);

    return TRUE;
}

/* static */
gboolean
window_state_event_cb (GtkWidget *widget, GdkEventWindowState *event)
{
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnWindowStateEvent(widget, event);

    return FALSE;
}

/* static */
gboolean
property_notify_event_cb  (GtkWidget *widget, GdkEventProperty *event)
{
    nsIWidget *nswidget = (nsIWidget *)get_window_for_gtk_widget(widget);
    if (!nswidget)
        return FALSE;

    nsGtkMozRemoteHelper::HandlePropertyChange(widget, event, nswidget);

    return FALSE;
}

//////////////////////////////////////////////////////////////////////
// These are all of our drag and drop operations

void
nsWindow::InitDragEvent(nsMouseEvent &aEvent)
{
    // set the keyboard modifiers
    gint x, y;
    GdkModifierType state = (GdkModifierType)0;
    gdk_window_get_pointer(NULL, &x, &y, &state);
    aEvent.isShift = (state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    aEvent.isControl = (state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    aEvent.isAlt = (state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    aEvent.isMeta = PR_FALSE; // GTK+ doesn't support the meta key
}

// This will update the drag action based on the information in the
// drag context.  Gtk gets this from a combination of the key settings
// and what the source is offering.

void
nsWindow::UpdateDragStatus(nsMouseEvent   &aEvent,
                           GdkDragContext *aDragContext,
                           nsIDragService *aDragService)
{
    // default is to do nothing
    int action = nsIDragService::DRAGDROP_ACTION_NONE;
    
    // set the default just in case nothing matches below
    if (aDragContext->actions & GDK_ACTION_DEFAULT)
        action = nsIDragService::DRAGDROP_ACTION_MOVE;
    
    // first check to see if move is set
    if (aDragContext->actions & GDK_ACTION_MOVE)
        action = nsIDragService::DRAGDROP_ACTION_MOVE;

    // then fall to the others
    else if (aDragContext->actions & GDK_ACTION_LINK)
        action = nsIDragService::DRAGDROP_ACTION_LINK;
   
    // copy is ctrl
    else if (aDragContext->actions & GDK_ACTION_COPY)
        action = nsIDragService::DRAGDROP_ACTION_COPY;

    // update the drag information
    nsCOMPtr<nsIDragSession> session;
    aDragService->GetCurrentSession(getter_AddRefs(session));

    if (session)
        session->SetDragAction(action);
}


/* static */
gboolean
drag_motion_event_cb(GtkWidget *aWidget,
                     GdkDragContext *aDragContext,
                     gint aX,
                     gint aY,
                     guint aTime,
                     gpointer aData)
{
    nsWindow *window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return FALSE;
    
    return window->OnDragMotionEvent(aWidget,
                                     aDragContext,
                                     aX, aY, aTime, aData);
}
/* static */
void
drag_leave_event_cb(GtkWidget *aWidget,
                    GdkDragContext *aDragContext,
                    guint aTime,
                    gpointer aData)
{
    nsWindow *window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return;
    window->OnDragLeaveEvent(aWidget, aDragContext, aTime, aData);
}


/* static */
gboolean
drag_drop_event_cb(GtkWidget *aWidget,
                   GdkDragContext *aDragContext,
                   gint aX,
                   gint aY,
                   guint aTime,
                   gpointer *aData)
{
    nsWindow *window = get_window_for_gtk_widget(aWidget);

    if (!window)
        return FALSE;
    
    return window->OnDragDropEvent(aWidget,
                                   aDragContext,
                                   aX, aY, aTime, aData);

}

/* static */
void
drag_data_received_event_cb (GtkWidget *aWidget,
                             GdkDragContext *aDragContext,
                             gint aX,
                             gint aY,
                             GtkSelectionData  *aSelectionData,
                             guint aInfo,
                             guint aTime,
                             gpointer aData)
{
    nsWindow *window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return;

    window->OnDragDataReceivedEvent(aWidget,
                                    aDragContext,
                                    aX, aY,
                                    aSelectionData,
                                    aInfo, aTime, aData);
}

/* static */
nsresult
initialize_prefs(void)
{
    // check to see if we should set our raise pref
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
    if (prefs) {
        PRBool val = PR_TRUE;
        nsresult rv;
        rv = prefs->GetBoolPref("mozilla.widget.raise-on-setfocus", &val);
        if (NS_SUCCEEDED(rv))
            gRaiseWindows = val;
    }

    return NS_OK;
}

void
nsWindow::ResetDragMotionTimer(GtkWidget *aWidget,
                               GdkDragContext *aDragContext,
                               gint aX, gint aY, guint aTime)
{
  
    // We have to be careful about ref ordering here.  if aWidget ==
    // mDraMotionWidget be careful not to let the refcnt drop to zero.
    // Same with the drag context.
    if (aWidget)
        gtk_widget_ref(aWidget);
    if (mDragMotionWidget)
        gtk_widget_unref(mDragMotionWidget);
    mDragMotionWidget = aWidget;

    if (aDragContext)
        gdk_drag_context_ref(aDragContext);
    if (mDragMotionContext)
        gdk_drag_context_unref(mDragMotionContext);
    mDragMotionContext = aDragContext;

    mDragMotionX = aX;
    mDragMotionY = aY;
    mDragMotionTime = aTime;

    // always clear the timer
    if (mDragMotionTimerID) {
        gtk_timeout_remove(mDragMotionTimerID);
        mDragMotionTimerID = 0;
        LOG(("*** canceled motion timer\n"));
    }

    // if no widget was passed in, just return instead of setting a new
    // timer
    if (!aWidget) {
        return;
    }
    
    // otherwise we create a new timer
    mDragMotionTimerID = gtk_timeout_add(100,
                                         (GtkFunction)DragMotionTimerCallback,
                                         this);
}

void
nsWindow::FireDragMotionTimer(void)
{
    LOG(("nsWindow::FireDragMotionTimer(%p)\n", this));

    OnDragMotionEvent(mDragMotionWidget, mDragMotionContext,
                      mDragMotionX, mDragMotionY, mDragMotionTime,
                      this);
}

void
nsWindow::FireDragLeaveTimer(void)
{
    LOG(("nsWindow::FireDragLeaveTimer(%p)\n", this));

    mDragLeaveTimer = 0;

    // clean up any pending drag motion window info
    if (mLastDragMotionWindow) {
        // send our leave signal
        mLastDragMotionWindow->OnDragLeave();
        mLastDragMotionWindow = 0;
        // since we're leaving a toplevel window, inform the drag service
        // that we're ending the drag
        nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
        dragService->EndDragSession();
    }
}

/* static */
guint
nsWindow::DragMotionTimerCallback(gpointer aClosure)
{
    nsWindow *window = NS_STATIC_CAST(nsWindow *, aClosure);
    window->FireDragMotionTimer();
    return FALSE;
}

/* static */
void
nsWindow::DragLeaveTimerCallback(nsITimer *aTimer, void *aClosure)
{
    nsWindow *window = NS_STATIC_CAST(nsWindow *, aClosure);
    window->FireDragLeaveTimer();
}

/* static */
GdkWindow *
get_inner_gdk_window (GdkWindow *aWindow,
                      gint x, gint y,
                      gint *retx, gint *rety)
{
    gint cx, cy, cw, ch, cd;
    GList * children = gdk_window_peek_children(aWindow);
    guint num = g_list_length(children);
    for (int i = 0; i < (int)num; i++) {
        GList * child = g_list_nth(children, num - i - 1) ;
        if (child) {
            GdkWindow * childWindow = (GdkWindow *) child->data;
            gdk_window_get_geometry (childWindow, &cx, &cy, &cw, &ch, &cd);
            if ((cx < x) && (x < (cx + cw)) &&
                (cy < y) && (y < (cy + ch)) &&
                gdk_window_is_visible (childWindow)) {
                return get_inner_gdk_window (childWindow,
                                             x - cx, y - cy,
                                             retx, rety);
            }
        }
    }
    *retx = x;
    *rety = y;
    return aWindow;
}

inline PRBool
is_context_menu_key(const nsKeyEvent& aKeyEvent)
{
    return ((aKeyEvent.keyCode == NS_VK_F10 && aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt) ||
            (aKeyEvent.keyCode == NS_VK_CONTEXT_MENU && !aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt));
}

void
key_event_to_context_menu_event(const nsKeyEvent* aKeyEvent,
                                nsMouseEvent* aCMEvent)
{
    memcpy(aCMEvent, aKeyEvent, sizeof(nsInputEvent));
    aCMEvent->message = NS_CONTEXTMENU_KEY;
    aCMEvent->isShift = aCMEvent->isControl = PR_FALSE;
    aCMEvent->isAlt = aCMEvent->isMeta = PR_FALSE;
    aCMEvent->clickCount = 0;
    aCMEvent->acceptActivation = PR_FALSE;
}

/* static */
int
is_parent_ungrab_enter(GdkEventCrossing *aEvent)
{
    return (GDK_CROSSING_UNGRAB == aEvent->mode) &&
        ((GDK_NOTIFY_ANCESTOR == aEvent->detail) ||
         (GDK_NOTIFY_VIRTUAL == aEvent->detail));

}

/* static */
int
is_parent_grab_leave(GdkEventCrossing *aEvent)
{
    return (GDK_CROSSING_GRAB == aEvent->mode) &&
        ((GDK_NOTIFY_ANCESTOR == aEvent->detail) ||
            (GDK_NOTIFY_VIRTUAL == aEvent->detail));
}

#ifdef ACCESSIBILITY
/**
 * void
 * nsWindow::CreateRootAccessible
 *
 * request to create the nsIAccessible Object for the toplevel window
 **/
void
nsWindow::CreateRootAccessible()
{
    if (mIsTopLevel && !mRootAccessible) {
        nsCOMPtr<nsIAccessible> acc;
        DispatchAccessibleEvent(getter_AddRefs(acc));

        if (acc) {
            mRootAccessible = acc;
        }
    }
}

void
nsWindow::GetRootAccessible(nsIAccessible** aAccessible)
{
    nsCOMPtr<nsIAccessible> docAcc, parentAcc;
    DispatchAccessibleEvent(getter_AddRefs(docAcc));
    PRUint32 role;

    while (docAcc) {
        docAcc->GetRole(&role);
        if (role == nsIAccessible::ROLE_FRAME) {
            *aAccessible = docAcc;
            NS_ADDREF(*aAccessible);
            break;
        } 
        docAcc->GetParent(getter_AddRefs(parentAcc));
        docAcc = parentAcc;
    }
}

/**
 * void
 * nsWindow::DispatchAccessibleEvent
 * @aAccessible: the out var, hold the new accessible object
 *
 * generate the NS_GETACCESSIBLE event, the event handler is
 * reponsible to create an nsIAccessible instant.
 **/
PRBool
nsWindow::DispatchAccessibleEvent(nsIAccessible** aAccessible)
{
    PRBool result = PR_FALSE;
    nsAccessibleEvent event(NS_GETACCESSIBLE, this);

    *aAccessible = nsnull;

    nsEventStatus status;
    DispatchEvent(&event, status);
    result = (nsEventStatus_eConsumeNoDefault == status) ? PR_TRUE : PR_FALSE;

    // if the event returned an accesssible get it.
    if (event.accessible)
        *aAccessible = event.accessible;

    return result;
}

void
nsWindow::DispatchActivateEvent(void)
{
    nsCommonWidget::DispatchActivateEvent();

    if (sAccessibilityEnabled) {
        nsCOMPtr<nsIAccessible> rootAcc;
        GetRootAccessible(getter_AddRefs(rootAcc));
        nsCOMPtr<nsPIAccessible> privAcc(do_QueryInterface(rootAcc));
        if (privAcc) {
            privAcc->FireToolkitEvent(
                         nsIAccessibleEvent::EVENT_ATK_WINDOW_ACTIVATE,
                         rootAcc, nsnull);
        }
    }
 }

void
nsWindow::DispatchDeactivateEvent(void)
{
    nsCommonWidget::DispatchDeactivateEvent();

    if (sAccessibilityEnabled) {
        nsCOMPtr<nsIAccessible> rootAcc;
        GetRootAccessible(getter_AddRefs(rootAcc));
        nsCOMPtr<nsPIAccessible> privAcc(do_QueryInterface(rootAcc));
        if (privAcc) {
            privAcc->FireToolkitEvent(
                         nsIAccessibleEvent::EVENT_ATK_WINDOW_DEACTIVATE,
                         rootAcc, nsnull);
        }
    }
}
#endif /* #ifdef ACCESSIBILITY */

// nsChildWindow class

nsChildWindow::nsChildWindow()
{
}

nsChildWindow::~nsChildWindow()
{
}

#ifdef USE_XIM

void
nsWindow::IMEDestroyContext(void)
{
    // If this is the focus window and we have an IM context we need
    // to unset the focus on this window before we destroy the window.
    if (gIMEFocusWindow == this) {
        gIMEFocusWindow->IMELoseFocus();
        gIMEFocusWindow = nsnull;
    }

    if (!mIMContext)
        return;

    gtk_im_context_set_client_window(mIMContext, NULL);
    g_object_unref(G_OBJECT(mIMContext));
    mIMContext = nsnull;
}


void
nsWindow::IMESetFocus(void)
{
    LOGIM(("IMESetFocus %p\n", (void *)this));
    GtkIMContext *im = IMEGetContext();
    if (!im)
        return;

    gtk_im_context_focus_in(im);
    gIMEFocusWindow = this;
}

void
nsWindow::IMELoseFocus(void)
{
    LOGIM(("IMELoseFocus %p\n", (void *)this));
    GtkIMContext *im = IMEGetContext();
    if (!im)
        return;

    gtk_im_context_focus_out(im);
}

void
nsWindow::IMEComposeStart(void)
{
    LOGIM(("IMEComposeStart [%p]\n", (void *)this));

    if (mComposingText) {
        NS_WARNING("tried to re-start text composition\n");
        return;
    }

    mComposingText = PR_TRUE;

    nsCompositionEvent compEvent(NS_COMPOSITION_START, this);

    nsEventStatus status;
    DispatchEvent(&compEvent, status);
}

void
nsWindow::IMEComposeText (const PRUnichar *aText,
                          const PRInt32 aLen,
                          const gchar *aPreeditString,
                          const PangoAttrList *aFeedback)
{
    // Send our start composition event if we need to
    if (!mComposingText)
        IMEComposeStart();

    LOGIM(("IMEComposeText\n"));
    nsTextEvent textEvent(NS_TEXT_TEXT, this);

    if (aLen != 0) {
        textEvent.theText = (PRUnichar*)aText;

        if (aPreeditString && aFeedback && (aLen > 0)) {
            IM_set_text_range(aLen, aPreeditString, aFeedback,
                              &(textEvent.rangeCount),
                              &(textEvent.rangeArray));
        }
    }

    nsEventStatus status;
    DispatchEvent(&textEvent, status);
  
    if (textEvent.rangeArray) {
        delete[] textEvent.rangeArray;
    }
}

void
nsWindow::IMEComposeEnd(void)
{
    LOGIM(("IMEComposeEnd [%p]\n", (void *)this));

    if (!mComposingText) {
        NS_WARNING("tried to end text composition before it was started");
        return;
    }

    mComposingText = PR_FALSE;

    nsCompositionEvent compEvent(NS_COMPOSITION_END, this);

    nsEventStatus status;
    DispatchEvent(&compEvent, status);
}

GtkIMContext*
nsWindow::IMEGetContext()
{
    return IM_get_input_context(this->mDrawingarea);
}

void
nsWindow::IMECreateContext(void)
{
    GtkIMContext *im = gtk_im_multicontext_new();
    if (!im)
        return;

    gtk_im_context_set_client_window(im, GTK_WIDGET(mContainer)->window);

    g_signal_connect(G_OBJECT(im), "preedit_changed",
                     G_CALLBACK(IM_preedit_changed_cb), this);
    g_signal_connect(G_OBJECT(im), "commit",
                     G_CALLBACK(IM_commit_cb), this);

    mIMContext = im;
}

PRBool
nsWindow::IMEFilterEvent(GdkEventKey *aEvent) 
{
    GtkIMContext *im = IMEGetContext();
    if (!im)
        return FALSE;

    gKeyEvent = aEvent;
    gboolean filtered = gtk_im_context_filter_keypress(im, aEvent);
    gKeyEvent = NULL;

    LOGIM(("key filtered: %d committed: %d changed: %d\n",
           filtered, gKeyEventCommitted, gKeyEventChanged));

    // We filter the key event if the event was not committed (because
    // it's probably part of a composition) or if the key event was
    // committed _and_ changed.  This way we still let key press
    // events go through as simple key press events instead of
    // composed characters.

    PRBool retval = PR_FALSE;
    if (filtered &&
        (!gKeyEventCommitted || (gKeyEventCommitted && gKeyEventChanged)))
        retval = PR_TRUE;

    gKeyEventChanged = PR_FALSE;
    gKeyEventCommitted = PR_FALSE;
    gKeyEventChanged = PR_FALSE;

    return retval;
}

/* static */
void
IM_preedit_changed_cb(GtkIMContext *aContext,
                      nsWindow     *aWindow)
{
    gchar *preedit_string;
    gint cursor_pos;
    PangoAttrList *feedback_list;

    // if gFocusWindow is null, use the last focused gIMEFocusWindow
    nsWindow *window = gFocusWindow ? gFocusWindow : gIMEFocusWindow;
    if (!window)
        return;
  
    // Should use cursor_pos ?
    gtk_im_context_get_preedit_string(aContext, &preedit_string,
                                      &feedback_list, &cursor_pos);
  
    LOGIM(("preedit string is: %s   length is: %d\n",
           preedit_string, strlen(preedit_string)));

    if (!preedit_string || !*preedit_string) {
        LOGIM(("preedit ended\n"));
        window->IMEComposeText(NULL, 0, NULL, NULL);
        window->IMEComposeEnd();
        return;
    }

    LOGIM(("preedit len %d\n", strlen(preedit_string)));

    gunichar2 * uniStr;
    glong uniStrLen;

    uniStr = NULL;
    uniStrLen = 0;
    uniStr = g_utf8_to_utf16(preedit_string, -1, NULL, &uniStrLen, NULL);

    if (!uniStr) {
        g_free(preedit_string);
        LOG(("utf8-utf16 string tranfer failed!\n"));
        if (feedback_list)
            pango_attr_list_unref(feedback_list);
        return;
    }

    if (uniStrLen) {
        window->IMEComposeText(NS_STATIC_CAST(const PRUnichar *, uniStr),
                               uniStrLen, preedit_string, feedback_list);
    }

    g_free(preedit_string);
    g_free(uniStr);

    if (feedback_list)
        pango_attr_list_unref(feedback_list);
}

/* static */
void
IM_commit_cb (GtkIMContext *aContext,
              const gchar  *aUtf8_str,
              nsWindow     *aWindow)
{
    gunichar2 *uniStr;
    glong uniStrLen;

    LOGIM(("IM_commit_cb\n"));

    gKeyEventCommitted = PR_TRUE;

    // if gFocusWindow is null, use the last focused gIMEFocusWindow
    nsWindow *window = gFocusWindow ? gFocusWindow : gIMEFocusWindow;

    if (!window)
        return;

    /* If IME doesn't change they keyevent that generated this commit,
       don't send it through XIM - just send it as a normal key press
       event. */

    if (gKeyEvent) {
        char keyval_utf8[8]; /* should have at least 6 bytes of space */
        gint keyval_utf8_len;
        guint32 keyval_unicode;

        keyval_unicode = gdk_keyval_to_unicode(gKeyEvent->keyval);
        keyval_utf8_len = g_unichar_to_utf8(keyval_unicode, keyval_utf8);
        keyval_utf8[keyval_utf8_len] = '\0';

        if (!strcmp(aUtf8_str, keyval_utf8)) {
            gKeyEventChanged = PR_FALSE;
            return;
        }
    }

    gKeyEventChanged = PR_TRUE;

    uniStr = NULL;
    uniStrLen = 0;
    uniStr = g_utf8_to_utf16(aUtf8_str, -1, NULL, &uniStrLen, NULL);

    if (!uniStr) {
        LOGIM(("utf80utf16 string tranfer failed!\n"));
        return;
    }

    if (uniStrLen) {
        window->IMEComposeText((const PRUnichar *)uniStr,
                               (PRInt32)uniStrLen, NULL, NULL);
        window->IMEComposeEnd();
    }

    g_free(uniStr);
}

#define	START_OFFSET(I)	\
    (*aTextRangeListResult)[I].mStartOffset

#define	END_OFFSET(I) \
    (*aTextRangeListResult)[I].mEndOffset

#define	SET_FEEDBACKTYPE(I,T) (*aTextRangeListResult)[I].mRangeType = T

/* static */
void
IM_set_text_range(const PRInt32 aLen,
                  const gchar *aPreeditString,
                  const PangoAttrList *aFeedback,
                  PRUint32 *aTextRangeListLengthResult,
                  nsTextRangeArray *aTextRangeListResult)
{
    if (aLen == 0) {
        aTextRangeListLengthResult = 0;
        aTextRangeListResult = NULL;
        return;
    }
    
    PangoAttrIterator * aFeedbackIterator;
    aFeedbackIterator = pango_attr_list_get_iterator((PangoAttrList*)aFeedback);
    //(NS_REINTERPRET_CAST(PangoAttrList*, aFeedback));
    // Since some compilers don't permit this casting -- from const to un-const
    if (aFeedbackIterator == NULL) return;

    /*
     * Don't use pango_attr_iterator_range, it'll mislead you.
     * As for going through the list to get the attribute number,
     * it take much time for performance.
     * Now it's simple and enough, although maybe waste a little space.
    */
    PRInt32 aMaxLenOfTextRange;
    aMaxLenOfTextRange = 2*aLen + 1;
    *aTextRangeListResult = new nsTextRange[aMaxLenOfTextRange];
    NS_ASSERTION(*aTextRangeListResult, "No enough memory.");
    
    // Set caret's postion
    SET_FEEDBACKTYPE(0, NS_TEXTRANGE_CARETPOSITION);
    START_OFFSET(0) = aLen;
    END_OFFSET(0) = aLen;

    int count = 0;
    PangoAttribute * aPangoAttr;
    PangoAttribute * aPangoAttrReverse, * aPangoAttrUnderline;
    /*
     * Depend on gtk2's implementation on XIM support.
     * In aFeedback got from gtk2, there are only three types of data:
     * PANGO_ATTR_UNDERLINE, PANGO_ATTR_FOREGROUND, PANGO_ATTR_BACKGROUND.
     * Corresponding to XIMUnderline, XIMReverse.
     * Don't take PANGO_ATTR_BACKGROUND into account, since
     * PANGO_ATTR_BACKGROUND and PANGO_ATTR_FOREGROUND are always
     * a couple.
     */
    gint start, end;
    gunichar2 * uniStr;
    glong uniStrLen;
    do {
        aPangoAttrUnderline = pango_attr_iterator_get(aFeedbackIterator,
                                                      PANGO_ATTR_UNDERLINE);
        aPangoAttrReverse = pango_attr_iterator_get(aFeedbackIterator,
                                                    PANGO_ATTR_FOREGROUND);
        if (!aPangoAttrUnderline && !aPangoAttrReverse)
            continue;

        // Get the range of the current attribute(s)
        pango_attr_iterator_range(aFeedbackIterator, &start, &end);

        PRUint32 feedbackType;
        // XIMReverse | XIMUnderline
        if (aPangoAttrUnderline && aPangoAttrReverse) {
            feedbackType = NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
            // Doesn't matter which attribute we use here since we
            // are using pango_attr_iterator_range to determine the
            // range of the attributes.
            aPangoAttr = aPangoAttrUnderline;
        }
        // XIMUnderline
        else if (aPangoAttrUnderline) {
            feedbackType = NS_TEXTRANGE_CONVERTEDTEXT;
            aPangoAttr = aPangoAttrUnderline;
        }
        // XIMReverse
        else if (aPangoAttrReverse) {
            feedbackType = NS_TEXTRANGE_SELECTEDRAWTEXT;
            aPangoAttr = aPangoAttrReverse;
        }

        count++;
        START_OFFSET(count) = 0;
        END_OFFSET(count) = 0;
        
        uniStr = NULL;
        if (start > 0) {
            uniStr = g_utf8_to_utf16(aPreeditString, start,
                                     NULL, &uniStrLen, NULL);
        }
        if (uniStr) {
            START_OFFSET(count) = uniStrLen;
            g_free(uniStr);
        }
        
        uniStr = NULL;
        uniStr = g_utf8_to_utf16(aPreeditString + start, end - start,
                                 NULL, &uniStrLen, NULL);
        if (uniStr) {
            END_OFFSET(count) = START_OFFSET(count) + uniStrLen;
            SET_FEEDBACKTYPE(count, feedbackType);
            g_free(uniStr);
        }

    } while ((count < aMaxLenOfTextRange - 1) &&
             (pango_attr_iterator_next(aFeedbackIterator)));

   *aTextRangeListLengthResult = count + 1;

   pango_attr_iterator_destroy(aFeedbackIterator);
}

/* static */
GtkIMContext *
IM_get_input_context(MozDrawingarea *aArea)
{
    GtkWidget *owningWidget =
        get_gtk_widget_for_gdk_window(aArea->inner_window);

    nsWindow *owningWindow = get_window_for_gtk_widget(owningWidget);

    return owningWindow->mIMContext;
}

#endif
