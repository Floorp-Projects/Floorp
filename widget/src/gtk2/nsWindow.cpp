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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "prlink.h"

#include "nsWindow.h"
#include "nsToolkit.h"
#include "nsIDeviceContext.h"
#include "nsIRenderingContext.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIDOMNode.h"

#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSessionGTK.h"

#include "nsGtkKeyUtils.h"
#include "nsGtkCursors.h"

#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include "gtk2xtbin.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsGfxCIID.h"

#ifdef ACCESSIBILITY
#include "nsPIAccessNode.h"
#include "nsPIAccessible.h"
#include "nsIAccessibleEvent.h"
#include "prenv.h"
#include "stdlib.h"
static PRBool sAccessibilityChecked = PR_FALSE;
/* static */
PRBool nsWindow::sAccessibilityEnabled = PR_FALSE;
static const char sSysPrefService [] = "@mozilla.org/system-preference-service;1";
static const char sAccEnv [] = "GNOME_ACCESSIBILITY";
static const char sAccessibilityKey [] = "config.use_system_prefs.accessibility";
#endif

/* For SetIcon */
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

/* SetCursor(imgIContainer*) */
#include <gdk/gdk.h>
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsGfxCIID.h"
#include "nsIImage.h"
#include "nsImageToPixbuf.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"

#ifdef MOZ_CAIRO_GFX
#include "gfxPlatformGtk.h"
#include "gfxXlibSurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

#ifdef MOZ_ENABLE_GLITZ
#include "gfxGlitzSurface.h"
#include "glitz-glx.h"
#endif
#endif

/* For PrepareNativeWidget */
static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

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
static void     theme_changed_cb          (GtkSettings *settings,
                                           GParamSpec *pspec,
                                           nsWindow *data);
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
// Time of the last button release event. We use it to detect when the
// drag ended before we could properly setup drag and drop.
guint32   nsWindow::mLastButtonReleaseTime = 0;

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

// the current focus window
static nsWindow         *gFocusWindow          = NULL;
static PRBool            gGlobalsInitialized   = PR_FALSE;
static PRBool            gRaiseWindows         = PR_TRUE;
static nsWindow         *gPluginFocusWindow    = NULL;

nsCOMPtr  <nsIRollupListener> gRollupListener;
nsWeakPtr                     gRollupWindow;

#define NS_WINDOW_TITLE_MAX_LENGTH 4095

#ifdef USE_XIM

static nsWindow    *gIMEFocusWindow = NULL;
static GdkEventKey *gKeyEvent = NULL;
static PRBool       gKeyEventCommitted = PR_FALSE;
static PRBool       gKeyEventChanged = PR_FALSE;
static PRBool       gIMESuppressCommit = PR_FALSE;

static void IM_commit_cb              (GtkIMContext *aContext,
                                       const gchar *aString,
                                       nsWindow *aWindow);
static void IM_commit_cb_internal     (const gchar *aString,
                                       nsWindow *aWindow);
static void IM_preedit_changed_cb     (GtkIMContext *aContext,
                                       nsWindow *aWindow);
static void IM_set_text_range         (const PRInt32 aLen,
                                       const gchar *aPreeditString,
                                       const gint aCursorPos,
                                       const PangoAttrList *aFeedback,
                                       PRUint32 *aTextRangeListLengthResult,
                                       nsTextRangeArray *aTextRangeListResult);

static nsWindow *IM_get_owning_window(MozDrawingarea *aArea);

static GtkIMContext *IM_get_input_context(nsWindow *window);

// If after selecting profile window, the startup fail, please refer to
// http://bugzilla.gnome.org/show_bug.cgi?id=88940
#endif

// needed for imgIContainer cursors
// GdkDisplay* was added in 2.2
typedef struct _GdkDisplay GdkDisplay;
typedef GdkDisplay* (*_gdk_display_get_default_fn)(void);

typedef GdkCursor*  (*_gdk_cursor_new_from_pixbuf_fn)(GdkDisplay *display,
                                                      GdkPixbuf *pixbuf,
                                                      gint x,
                                                      gint y);
static _gdk_display_get_default_fn    _gdk_display_get_default;
static _gdk_cursor_new_from_pixbuf_fn _gdk_cursor_new_from_pixbuf;
static PRBool sPixbufCursorChecked;

// needed for GetAttention calls
// gdk_window_set_urgency_hint was added in 2.8
typedef void (*_gdk_window_set_urgency_hint_fn)(GdkWindow *window,
                                                gboolean urgency);

#define kWindowPositionSlop 20

// cursor cache
static GdkCursor *gCursorCache[eCursorCount];

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
    mIMEData = nsnull;
#endif

#ifdef ACCESSIBILITY
    mRootAccessible  = nsnull;
#endif

    mIsTranslucent = PR_FALSE;
    mTransparencyBitmap = nsnull;

    mTransparencyBitmapWidth  = 0;
    mTransparencyBitmapHeight = 0;
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

#ifndef USE_XIM
NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsCommonWidget,
                             nsISupportsWeakReference)
#else
NS_IMPL_ISUPPORTS_INHERITED2(nsWindow, nsCommonWidget,
                             nsISupportsWeakReference,
                             nsIKBStateControl)
#endif

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
    
    g_signal_handlers_disconnect_by_func(gtk_settings_get_default(),
                                         (gpointer)G_CALLBACK(theme_changed_cb),
                                         this);

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

#ifdef MOZ_CAIRO_GFX
    // Destroy thebes surface now. Badness can happen if we destroy
    // the surface after its X Window.
    mThebesSurface = nsnull;
#endif

    if (mDragMotionTimerID) {
        gtk_timeout_remove(mDragMotionTimerID);
        mDragMotionTimerID = 0;
    }

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
nsWindow::SetParent(nsIWidget *aNewParent)
{
    NS_ENSURE_ARG_POINTER(aNewParent);

    GdkWindow* newParentWindow =
        NS_STATIC_CAST(GdkWindow*, aNewParent->GetNativeData(NS_NATIVE_WINDOW));
    NS_ASSERTION(newParentWindow, "Parent widget has a null native window handle");

    if (!mShell && mDrawingarea) {
        moz_drawingarea_reparent(mDrawingarea, newParentWindow);
    } else {
        NS_NOTREACHED("nsWindow::SetParent - reparenting a non-child window");
    }
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
    if (mIsTopLevel && mShell) {
        aState = GTK_WIDGET_VISIBLE(mShell);
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

    // XXX Should we do some AreBoundsSane check here?

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
nsWindow::SetZIndex(PRInt32 aZIndex)
{
    nsIWidget* oldPrev = GetPrevSibling();

    nsBaseWidget::SetZIndex(aZIndex);

    if (GetPrevSibling() == oldPrev) {
        return NS_OK;
    }

    NS_ASSERTION(!mContainer, "Expected Mozilla child widget");

    // We skip the nsWindows that don't have mDrawingareas.
    // These are probably in the process of being destroyed.

    if (!GetNextSibling()) {
        // We're to be on top.
        if (mDrawingarea)
            gdk_window_raise(mDrawingarea->clip_window);
    } else {
        // All the siblings before us need to be below our widget. 
        for (nsWindow* w = this; w;
             w = NS_STATIC_CAST(nsWindow*, w->GetPrevSibling())) {
            if (w->mDrawingarea)
                gdk_window_lower(w->mDrawingarea->clip_window);
        }
    }
    return NS_OK;
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
        !GTK_WIDGET_HAS_FOCUS(toplevelWidget)) {
        GtkWidget* top_window = nsnull;
        GetToplevelWidget(&top_window);
        if (top_window && (GTK_WIDGET_VISIBLE(top_window)))
        {
            gdk_window_show_unraised(top_window->window);
            // Unset the urgency hint if possible.
            SetUrgencyHint(top_window, PR_FALSE);
        }
    }

    nsWindow  *owningWindow = get_window_for_gtk_widget(owningWidget);
    if (!owningWindow)
        return NS_ERROR_FAILURE;

    if (!GTK_WIDGET_HAS_FOCUS(owningWidget)) {
        LOGFOCUS(("  grabbing focus for the toplevel [%p]\n", (void *)this));
        owningWindow->mContainerBlockFocus = PR_TRUE;
        
        // Set focus to the window
        if (gRaiseWindows && aRaise && toplevelWidget &&
            !GTK_WIDGET_HAS_FOCUS(toplevelWidget) &&
            owningWindow->mIsShown && GTK_IS_WINDOW(owningWindow->mShell))
          gtk_window_present(GTK_WINDOW(owningWindow->mShell));
        
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
        if (IM_get_input_context(this) !=
            IM_get_input_context(gFocusWindow))
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


static
PRUint8* Data32BitTo1Bit(PRUint8* aImageData,
                         PRUint32 aImageBytesPerRow,
                         PRUint32 aWidth, PRUint32 aHeight)
{
  PRUint32 outBpr = (aWidth + 7) / 8;
  
  PRUint8* outData = new PRUint8[outBpr * aHeight];
  if (!outData)
      return NULL;

  PRUint8 *outRow = outData,
          *imageRow = aImageData;

  for (PRUint32 curRow = 0; curRow < aHeight; curRow++) {
      PRUint8 *irow = imageRow;
      PRUint8 *orow = outRow;
      PRUint8 imagePixels = 0;
      PRUint8 offset = 0;

      for (PRUint32 curCol = 0; curCol < aWidth; curCol++) {
          PRUint8 r = *imageRow++,
                  g = *imageRow++,
                  b = *imageRow++;
               /* a = * */imageRow++;

          if ((r + b + g) < 3 * 128)
              imagePixels |= (1 << offset);

          if (offset == 7) {
              *outRow++ = imagePixels;
              offset = 0;
              imagePixels = 0;
          } else {
              offset++;
          }
      }
      if (offset != 0)
          *outRow++ = imagePixels;

      imageRow = irow + aImageBytesPerRow;
      outRow = orow + outBpr;
  }

  return outData;
}



NS_IMETHODIMP
nsWindow::SetCursor(imgIContainer* aCursor,
                    PRUint32 aHotspotX, PRUint32 aHotspotY)
{
    // if we're not the toplevel window pass up the cursor request to
    // the toplevel window to handle it.
    if (!mContainer && mDrawingarea) {
        GtkWidget *widget =
            get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);
        nsWindow *window = get_window_for_gtk_widget(widget);
        return window->SetCursor(aCursor, aHotspotX, aHotspotY);
    }

    if (!sPixbufCursorChecked) {
        PRLibrary* lib;
        _gdk_cursor_new_from_pixbuf = (_gdk_cursor_new_from_pixbuf_fn)
            PR_FindFunctionSymbolAndLibrary("gdk_cursor_new_from_pixbuf", &lib);
        _gdk_display_get_default = (_gdk_display_get_default_fn)
            PR_FindFunctionSymbolAndLibrary("gdk_display_get_default", &lib);
        sPixbufCursorChecked = PR_TRUE;
    }
    mCursor = nsCursor(-1);

    // Get first image frame
    nsCOMPtr<gfxIImageFrame> frame;
    aCursor->GetFrameAt(0, getter_AddRefs(frame));
    if (!frame)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsIImage> img(do_GetInterface(frame));
    if (!img)
        return NS_ERROR_NOT_AVAILABLE;

    GdkPixbuf* pixbuf = nsImageToPixbuf::ImageToPixbuf(img);
    if (!pixbuf)
        return NS_ERROR_NOT_AVAILABLE;

    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    // Reject cursors greater than 128 pixels in some direction, to prevent
    // spoofing.
    // XXX ideally we should rescale. Also, we could modify the API to
    // allow trusted content to set larger cursors.
    if (width > 128 || height > 128)
        return NS_ERROR_NOT_AVAILABLE;

    // Looks like all cursors need an alpha channel (tested on Gtk 2.4.4). This
    // is of course not documented anywhere...
    // So add one if there isn't one yet
    if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
        GdkPixbuf* alphaBuf = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
        gdk_pixbuf_unref(pixbuf);
        if (!alphaBuf) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        pixbuf = alphaBuf;
    }

    GdkCursor* cursor;
    if (!_gdk_cursor_new_from_pixbuf || !_gdk_display_get_default) {
        // Fallback to a monochrome cursor
        GdkPixmap* mask = gdk_pixmap_new(NULL, width, height, 1);
        if (!mask)
            return NS_ERROR_OUT_OF_MEMORY;

        PRUint8* data = Data32BitTo1Bit(gdk_pixbuf_get_pixels(pixbuf),
                                        gdk_pixbuf_get_rowstride(pixbuf),
                                        width, height);
        if (!data) {
            g_object_unref(mask);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        GdkPixmap* image = gdk_bitmap_create_from_data(NULL, (const gchar*)data, width,
                                                       height);
        delete[] data;
        if (!image) {
            g_object_unref(mask);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        gdk_pixbuf_render_threshold_alpha(pixbuf, mask, 0, 0, 0, 0, width,
                                          height, 1);

        GdkColor fg = { 0, 0, 0, 0 }; // Black
        GdkColor bg = { 0, 0xFFFF, 0xFFFF, 0xFFFF }; // White

        cursor = gdk_cursor_new_from_pixmap(image, mask, &fg, &bg, aHotspotX,
                                            aHotspotY);
        g_object_unref(image);
        g_object_unref(mask);
    } else {
        // Now create the cursor
        cursor = _gdk_cursor_new_from_pixbuf(_gdk_display_get_default(),
                                             pixbuf,
                                             aHotspotX, aHotspotY);
    }
    gdk_pixbuf_unref(pixbuf);
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    if (cursor) {
        if (mContainer) {
            gdk_window_set_cursor(GTK_WIDGET(mContainer)->window, cursor);
            XFlush(GDK_DISPLAY());
            rv = NS_OK;
        }
        gdk_cursor_unref(cursor);
    }

    return rv;
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
                               &rect, FALSE);
    if (aIsSynchronous)
        gdk_window_process_updates(mDrawingarea->inner_window, FALSE);

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
                               &rect, FALSE);
    if (aIsSynchronous)
        gdk_window_process_updates(mDrawingarea->inner_window, FALSE);

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
                                     region, FALSE);
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

    gdk_window_process_updates(mDrawingarea->inner_window, FALSE);
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

    case NS_NATIVE_SHELLWIDGET:
        return (void *) mShell;

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
#define UTF8_FOLLOWBYTE(ch) (((ch) & 0xC0) == 0x80)
    NS_ConvertUTF16toUTF8 titleUTF8(aTitle);
    if (titleUTF8.Length() > NS_WINDOW_TITLE_MAX_LENGTH) {
        // Truncate overlong titles (bug 167315). Make sure we chop after a
        // complete sequence by making sure the next char isn't a follow-byte.
        PRUint32 len = NS_WINDOW_TITLE_MAX_LENGTH;
        while(UTF8_FOLLOWBYTE(titleUTF8[len]))
            --len;
        titleUTF8.Truncate(len);
    }
    gtk_window_set_title(GTK_WINDOW(mShell), (const char *)titleUTF8.get());

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAString& aIconSpec)
{
    if (!mShell)
        return NS_OK;

    nsCOMPtr<nsILocalFile> iconFile;
    nsCAutoString path;
    nsCStringArray iconList;

    // Assume the given string is a local identifier for an icon file.

    ResolveIconName(aIconSpec, NS_LITERAL_STRING(".xpm"),
                    getter_AddRefs(iconFile));
    if (iconFile) {
        iconFile->GetNativePath(path);
        iconList.AppendCString(path);
    }

    // Get the 16px icon path as well
    ResolveIconName(aIconSpec, NS_LITERAL_STRING("16.xpm"),
                    getter_AddRefs(iconFile));
    if (iconFile) {
        iconFile->GetNativePath(path);
        iconList.AppendCString(path);
    }

    // leave the default icon intact if no matching icons were found
    if (iconList.Count() == 0)
        return NS_OK;

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
    gint x = 0, y = 0;

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
    GtkWidget* top_focused_window = nsnull;
    GetToplevelWidget(&top_window);
    if (gFocusWindow)
        gFocusWindow->GetToplevelWidget(&top_focused_window);

    // Don't get attention if the window is focused anyway.
    if (top_window && (GTK_WIDGET_VISIBLE(top_window)) &&
        top_window != top_focused_window) {
        SetUrgencyHint(top_window, PR_TRUE);
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

#ifdef DEBUG
// Paint flashing code

#define CAPS_LOCK_IS_ON \
(gdk_keyboard_get_modifiers() & GDK_LOCK_MASK)

#define WANT_PAINT_FLASHING \
(debug_WantPaintFlashing() && CAPS_LOCK_IS_ON)

static GdkModifierType
gdk_keyboard_get_modifiers()
{
  GdkModifierType m = (GdkModifierType) 0;

  gdk_window_get_pointer(NULL, NULL, NULL, &m);

  return m;
}

static void
gdk_window_flash(GdkWindow *    aGdkWindow,
                 unsigned int   aTimes,
                 unsigned int   aInterval,  // Milliseconds
                 GdkRegion *    aRegion)
{
  gint         x;
  gint         y;
  gint         width;
  gint         height;
  guint        i;
  GdkGC *      gc = 0;
  GdkColor     white;

  gdk_window_get_geometry(aGdkWindow,
                          NULL,
                          NULL,
                          &width,
                          &height,
                          NULL);

  gdk_window_get_origin (aGdkWindow,
                         &x,
                         &y);

  gc = gdk_gc_new(GDK_ROOT_PARENT());

  white.pixel = WhitePixel(gdk_display,DefaultScreen(gdk_display));

  gdk_gc_set_foreground(gc,&white);
  gdk_gc_set_function(gc,GDK_XOR);
  gdk_gc_set_subwindow(gc,GDK_INCLUDE_INFERIORS);
  
  gdk_region_offset(aRegion, x, y);
  gdk_gc_set_clip_region(gc, aRegion);

  /*
   * Need to do this twice so that the XOR effect can replace 
   * the original window contents.
   */
  for (i = 0; i < aTimes * 2; i++)
  {
    gdk_draw_rectangle(GDK_ROOT_PARENT(),
                       gc,
                       TRUE,
                       x,
                       y,
                       width,
                       height);

    gdk_flush();
    
    PR_Sleep(PR_MillisecondsToInterval(aInterval));
  }

  gdk_gc_destroy(gc);

  gdk_region_offset(aRegion, -x, -y);
}
#endif // DEBUG

gboolean
nsWindow::OnExposeEvent(GtkWidget *aWidget, GdkEventExpose *aEvent)
{
    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, (void *)aEvent->window));
        return FALSE;
    }

    if (!mDrawingarea)
        return FALSE;

    // handle exposes for the inner window only
    if (aEvent->window != mDrawingarea->inner_window)
        return FALSE;

    static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

    nsCOMPtr<nsIRegion> updateRegion = do_CreateInstance(kRegionCID);
    if (!updateRegion)
        return FALSE;

    updateRegion->Init();

    GdkRectangle *rects;
    gint nrects;
    gdk_region_get_rectangles(aEvent->region, &rects, &nrects);
    LOGDRAW(("sending expose event [%p] %p 0x%lx (rects follow):\n",
             (void *)this, (void *)aEvent->window,
             GDK_WINDOW_XWINDOW(aEvent->window)));

    GdkRectangle *r;
    GdkRectangle *r_end = rects + nrects;
    for (r = rects; r < r_end; ++r) {
        updateRegion->Union(r->x, r->y, r->width, r->height);
        LOGDRAW(("\t%d %d %d %d\n", r->x, r->y, r->width, r->height));
    }

    nsCOMPtr<nsIRenderingContext> rc = getter_AddRefs(GetRenderingContext());

#ifdef MOZ_CAIRO_GFX
    PRBool translucent;
    GetWindowTranslucency(translucent);
    nsIntRect boundsRect;
    GdkPixmap* bufferPixmap = nsnull;
    nsRefPtr<gfxXlibSurface> bufferPixmapSurface;

    updateRegion->GetBoundingBox(&boundsRect.x, &boundsRect.y,
                                 &boundsRect.width, &boundsRect.height);

    // do double-buffering and clipping here
    nsRefPtr<gfxContext> ctx
        = (gfxContext*)rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);
    ctx->Save();
    ctx->NewPath();
    if (translucent) {
        // Collapse update area to the bounding box. This is so we only have to
        // call UpdateTranslucentWindowAlpha once. After we have dropped
        // support for non-Thebes graphics, UpdateTranslucentWindowAlpha will be
        // our private interface so we can rework things to avoid this.
        ctx->Rectangle(gfxRect(boundsRect.x, boundsRect.y,
                               boundsRect.width, boundsRect.height));
    } else {
        for (r = rects; r < r_end; ++r) {
            ctx->Rectangle(gfxRect(r->x, r->y, r->width, r->height));
        }
    }
    ctx->Clip();

    // double buffer
    if (translucent) {
        ctx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
    } else {
#ifdef MOZ_ENABLE_GLITZ
        ctx->PushGroup(gfxASurface::CONTENT_COLOR);
#else // MOZ_ENABLE_GLITZ
        // Instead of just doing PushGroup we're going to do a little dance
        // to ensure that GDK creates the pixmap, so it doesn't go all
        // XGetGeometry on us in gdk_pixmap_foreign_new_for_display when we
        // paint native themes
        GdkDrawable* d = GDK_DRAWABLE(mDrawingarea->inner_window);
        gint depth = gdk_drawable_get_depth(d);
        bufferPixmap = gdk_pixmap_new(d, boundsRect.width, boundsRect.height, depth);
        if (bufferPixmap) {
            GdkVisual* visual = gdk_drawable_get_visual(GDK_DRAWABLE(bufferPixmap));
            Visual* XVisual = gdk_x11_visual_get_xvisual(visual);
            Display* display = gdk_x11_drawable_get_xdisplay(GDK_DRAWABLE(bufferPixmap));
            Drawable drawable = gdk_x11_drawable_get_xid(GDK_DRAWABLE(bufferPixmap));
            bufferPixmapSurface =
                new gfxXlibSurface(display, drawable, XVisual,
                                   boundsRect.width, boundsRect.height);
            if (bufferPixmapSurface) {
                bufferPixmapSurface->SetDeviceOffset(gfxPoint(-boundsRect.x, -boundsRect.y));
                nsCOMPtr<nsIRenderingContext> newRC;
                nsresult rv = GetDeviceContext()->
                    CreateRenderingContextInstance(*getter_AddRefs(newRC));
                if (NS_FAILED(rv)) {
                    bufferPixmapSurface = nsnull;
                } else {
                    rv = newRC->Init(GetDeviceContext(), bufferPixmapSurface);
                    if (NS_FAILED(rv)) {
                        bufferPixmapSurface = nsnull;
                    } else {
                        rc = newRC;
                    }
                }
            }
        }
#endif // MOZ_ENABLE_GLITZ
    }
#endif // MOZ_CAIRO_GFX

    // NOTE: Paint flashing region would be wrong for cairo, since
    // cairo inflates the update region, etc.  So don't paint flash
    // for cairo.
#if !defined(MOZ_CAIRO_GFX) && defined(DEBUG)
    if (WANT_PAINT_FLASHING && aEvent->window)
        gdk_window_flash(aEvent->window, 1, 100, aEvent->region);
#endif // !defined(MOZ_CAIRO_GFX) && defined(DEBUG)
    
    nsPaintEvent event(PR_TRUE, NS_PAINT, this);
    event.refPoint.x = aEvent->area.x;
    event.refPoint.y = aEvent->area.y;
    event.rect = nsnull;
    event.region = updateRegion;
    event.renderingContext = rc;

    nsEventStatus status;
    DispatchEvent(&event, status);

#ifdef MOZ_CAIRO_GFX
    if (status != nsEventStatus_eIgnore) {
        if (translucent) {
            nsRefPtr<gfxPattern> pattern = ctx->PopGroup();
            ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
            ctx->SetPattern(pattern);
            ctx->Paint();

            nsRefPtr<gfxImageSurface> img =
                new gfxImageSurface(gfxIntSize(boundsRect.width, boundsRect.height),
                                    gfxImageSurface::ImageFormatA8);
            img->SetDeviceOffset(gfxPoint(-boundsRect.x, -boundsRect.y));
            
            nsRefPtr<gfxContext> imgCtx = new gfxContext(img);
            imgCtx->SetPattern(pattern);
            imgCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
            imgCtx->Paint();
        
            UpdateTranslucentWindowAlphaInternal(nsRect(boundsRect.x, boundsRect.y,
                                                        boundsRect.width, boundsRect.height),
                                                 img->Data(), img->Stride());
        } else {
#ifdef MOZ_ENABLE_GLITZ
            ctx->PopGroupToSource();
            ctx->Paint();
#else // MOZ_ENABLE_GLITZ
            if (bufferPixmapSurface) {
                ctx->SetSource(bufferPixmapSurface);
                ctx->Paint();
            }
#endif // MOZ_ENABLE_GLITZ
        }
    } else {
        // ignore
        if (translucent) {
            ctx->PopGroup();
        } else {
#ifdef MOZ_ENABLE_GLITZ
            ctx->PopGroup();
#endif // MOZ_ENABLE_GLITZ
        }
    }

    if (bufferPixmap) {
        g_object_unref(G_OBJECT(bufferPixmap));
    }

    ctx->Restore();
#endif // MOZ_CAIRO_GFX

    g_free(rects);

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

    nsGUIEvent event(PR_TRUE, NS_MOVE, this);

    event.refPoint.x = aEvent->x;
    event.refPoint.y = aEvent->y;

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

    ResizeTransparencyBitmap(rect.width, rect.height);

    mBounds.width = rect.width;
    mBounds.height = rect.height;

    if (!mDrawingarea)
        return;

    moz_drawingarea_resize (mDrawingarea, rect.width, rect.height);

    if (mTransparencyBitmap) {
      ApplyTransparencyBitmap();
    }

    nsEventStatus status;
    DispatchResizeEvent (rect, status);
}

void
nsWindow::OnDeleteEvent(GtkWidget *aWidget, GdkEventAny *aEvent)
{
    nsGUIEvent event(PR_TRUE, NS_XUL_CLOSE, this);

    event.refPoint.x = 0;
    event.refPoint.y = 0;

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnEnterNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
    // XXXldb Is this the right test for embedding cases?
    if (aEvent->subwindow != NULL)
        return;

    nsMouseEvent event(PR_TRUE, NS_MOUSE_ENTER, this, nsMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->x);
    event.refPoint.y = nscoord(aEvent->y);

    event.time = aEvent->time;

    LOG(("OnEnterNotify: %p\n", (void *)this));

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnLeaveNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
    // XXXldb Is this the right test for embedding cases?
    if (aEvent->subwindow != NULL)
        return;

    nsMouseEvent event(PR_TRUE, NS_MOUSE_EXIT, this, nsMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->x);
    event.refPoint.y = nscoord(aEvent->y);

    event.time = aEvent->time;

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
    // XXXldb Why skip every other motion event when we have multiple,
    // but not more than that?
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

    nsMouseEvent event(PR_TRUE, NS_MOUSE_MOVE, this, nsMouseEvent::eReal);

    if (synthEvent) {
        event.refPoint.x = nscoord(xevent.xmotion.x);
        event.refPoint.y = nscoord(xevent.xmotion.y);

        event.isShift   = (xevent.xmotion.state & GDK_SHIFT_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isControl = (xevent.xmotion.state & GDK_CONTROL_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isAlt     = (xevent.xmotion.state & GDK_MOD1_MASK)
            ? PR_TRUE : PR_FALSE;

        event.time = xevent.xmotion.time;
    }
    else {
        event.refPoint.x = nscoord(aEvent->x);
        event.refPoint.y = nscoord(aEvent->y);

        event.isShift   = (aEvent->state & GDK_SHIFT_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isControl = (aEvent->state & GDK_CONTROL_MASK)
            ? PR_TRUE : PR_FALSE;
        event.isAlt     = (aEvent->state & GDK_MOD1_MASK)
            ? PR_TRUE : PR_FALSE;

        event.time = aEvent->time;
    }

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnButtonPressEvent(GtkWidget *aWidget, GdkEventButton *aEvent)
{
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
    mLastButtonReleaseTime = 0;

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

    PRUint16 domButton;
    switch (aEvent->button) {
    case 2:
        domButton = nsMouseEvent::eMiddleButton;
        break;
    case 3:
        domButton = nsMouseEvent::eRightButton;
        break;
    default:
        domButton = nsMouseEvent::eLeftButton;
        break;
    }

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

    nsMouseEvent event(PR_TRUE, NS_MOUSE_BUTTON_DOWN, this, nsMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent);

    DispatchEvent(&event, status);

    // right menu click on linux should also pop up a context menu
    if (domButton == nsMouseEvent::eRightButton) {
        nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, this,
                                      nsMouseEvent::eReal);
        InitButtonEvent(contextMenuEvent, aEvent);
        DispatchEvent(&contextMenuEvent, status);
    }
}

void
nsWindow::OnButtonReleaseEvent(GtkWidget *aWidget, GdkEventButton *aEvent)
{
    PRUint16 domButton;
    mLastButtonReleaseTime = aEvent->time;

    switch (aEvent->button) {
    case 2:
        domButton = nsMouseEvent::eMiddleButton;
        break;
    case 3:
        domButton = nsMouseEvent::eRightButton;
        break;
        // don't send events for these types
    case 4:
    case 5:
        return;
        break;
        // default including button 1 is left button up
    default:
        domButton = nsMouseEvent::eLeftButton;
        break;
    }

    nsMouseEvent event(PR_TRUE, NS_MOUSE_BUTTON_UP, this, nsMouseEvent::eReal);
    event.button = domButton;
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

    // Unset the urgency hint, if possible
    GtkWidget* top_window = nsnull;
    GetToplevelWidget(&top_window);
    if (top_window && (GTK_WIDGET_VISIBLE(top_window)))
        SetUrgencyHint(top_window, PR_FALSE);

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
           (void *)this, IMEComposingWindow() != nsnull, aEvent->keyval));
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

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

    // If the key repeat flag isn't set then set it so we don't send
    // another key down event on the next key press -- DOM events are
    // key down, key press and key up.  X only has key press and key
    // release.  gtk2 already filters the extra key release events for
    // us.

    PRBool isKeyDownCancelled = PR_FALSE;
    if (!mInKeyRepeat) {
        mInKeyRepeat = PR_TRUE;

        // send the key down event
        nsKeyEvent downEvent(PR_TRUE, NS_KEY_DOWN, this);
        InitKeyEvent(downEvent, aEvent);
        DispatchEvent(&downEvent, status);
        isKeyDownCancelled = (status == nsEventStatus_eConsumeNoDefault);
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
        // reset the key repeat flag so that the next keypress gets the
        // key down event
        mInKeyRepeat = PR_FALSE;
        return TRUE;
    }
    nsKeyEvent event(PR_TRUE, NS_KEY_PRESS, this);
    InitKeyEvent(event, aEvent);
    if (isKeyDownCancelled) {
      // If prevent default set for onkeydown, do the same for onkeypress
      event.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    }
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

           // Keep the characters unshifted for shortcuts and accesskeys and
           // make sure that numbers are always passed as such (among others:
           // bugs 50255 and 351310)
           if (!event.isControl && event.isShift &&
               (event.charCode < GDK_0 || event.charCode > GDK_9)) {
               GdkKeymapKey k = { aEvent->hardware_keycode, aEvent->group, 0 };
               guint savedKeyval = aEvent->keyval;
               aEvent->keyval = gdk_keymap_lookup_key(gdk_keymap_get_default(), &k);
               PRUint32 unshiftedCharCode = nsConvertCharCodeToUnicode(aEvent);
               if (unshiftedCharCode)
                   event.charCode = unshiftedCharCode;
               else
                   aEvent->keyval = savedKeyval;
           }
        }
    }

    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (is_context_menu_key(event)) {
        nsMouseEvent contextMenuEvent(PR_TRUE, 0, nsnull, nsMouseEvent::eReal);
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
    nsKeyEvent event(PR_TRUE, NS_KEY_UP, this);
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
    nsMouseScrollEvent event(PR_TRUE, NS_MOUSE_SCROLL, this);
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

    nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);

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

void
nsWindow::ThemeChanged()
{
    nsGUIEvent event(PR_TRUE, NS_THEMECHANGED, this);
    nsEventStatus status = nsEventStatus_eIgnore;
    DispatchEvent(&event, status);

    if (!mDrawingarea)
        return;

    // Dispatch NS_THEMECHANGED to all child windows
    GList *children =
        gdk_window_peek_children(mDrawingarea->inner_window);
    while (children) {
        GdkWindow *gdkWin = GDK_WINDOW(children->data);

        nsWindow *win = (nsWindow*) g_object_get_data(G_OBJECT(gdkWin),
                                                      "nsWindow");

        if (win && win != this)   // guard against infinite recursion
            win->ThemeChanged();

        children = children->next;
    }
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

    if (mLastButtonReleaseTime) {
      // The drag ended before it was even setup to handle the end of the drag
      // So, we fake the button getting released again to release the drag
      GtkWidget *widget = gtk_grab_get_current();
      GdkEvent event;
      gboolean retval;
      memset(&event, 0, sizeof(event));
      event.type = GDK_BUTTON_RELEASE;
      event.button.time = mLastButtonReleaseTime;
      event.button.button = 1;
      mLastButtonReleaseTime = 0;
      if (widget) {
        g_signal_emit_by_name(widget, "button_release_event", &event, &retval);
        return TRUE;
      }
    }

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
        // drag. Send an enter event to initiate the drag.

        innerMostWidget->OnDragEnter(retx, rety);
    }

    // set the last window to the innerMostWidget
    mLastDragMotionWindow = innerMostWidget;

    // update the drag context
    dragSessionGTK->TargetSetLastContext(aWidget, aDragContext, aTime);

    // notify the drag service that we are starting a drag motion.
    dragSessionGTK->TargetStartDragMotion();

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_OVER, innerMostWidget,
                       nsMouseEvent::eReal);

    InitDragEvent(event);

    // now that we have initialized the event update our drag status
    UpdateDragStatus(event, aDragContext, dragService);

    event.refPoint.x = retx;
    event.refPoint.y = rety;
    event.time = aTime;

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
        // if there was no other motion window, send an enter event to
        // initiate the drag session.
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

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_OVER, innerMostWidget,
                       nsMouseEvent::eReal);

    InitDragEvent(event);

    // now that we have initialized the event update our drag status
    UpdateDragStatus(event, aDragContext, dragService);

    event.refPoint.x = retx;
    event.refPoint.y = rety;
    event.time = aTime;

    nsEventStatus status;
    innerMostWidget->DispatchEvent(&event, status);

    event.message = NS_DRAGDROP_DROP;
    event.widget = innerMostWidget;
    event.refPoint.x = retx;
    event.refPoint.y = rety;

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

    // Make sure to end the drag session. If this drag started in a
    // different app, we won't get a drag_end signal to end it from.
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

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_EXIT, this, nsMouseEvent::eReal);

    AddRef();

    nsEventStatus status;
    DispatchEvent(&event, status);

    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);

    if (dragService) {
        nsCOMPtr<nsIDragSession> currentDragSession;
        dragService->GetCurrentSession(getter_AddRefs(currentDragSession));

        if (currentDragSession) {
            nsCOMPtr<nsIDOMNode> sourceNode;
            currentDragSession->GetSourceNode(getter_AddRefs(sourceNode));

            if (!sourceNode) {
                // We're leaving a window while doing a drag that was
                // initiated in a different app. End the drag session,
                // since we're done with it for now (until the user
                // drags back into mozilla).
                dragService->EndDragSession();
            }
        }
    }

    Release();
}

void
nsWindow::OnDragEnter(nscoord aX, nscoord aY)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?

    LOG(("nsWindow::OnDragEnter(%p)\n", this));

    nsRefPtr<nsWindow> kungFuDeathGrip(this);

    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);

    if (dragService) {
        // Make sure that the drag service knows we're now dragging.
        dragService->StartDragSession();
    }

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_ENTER, this, nsMouseEvent::eReal);

    event.refPoint.x = aX;
    event.refPoint.y = aY;

    nsEventStatus status;
    DispatchEvent(&event, status);
}

static void
GetBrandName(nsXPIDLString& brandName)
{
    nsCOMPtr<nsIStringBundleService> bundleService = 
        do_GetService(NS_STRINGBUNDLE_CONTRACTID);

    nsCOMPtr<nsIStringBundle> bundle;
    if (bundleService)
        bundleService->CreateBundle(
            "chrome://branding/locale/brand.properties",
            getter_AddRefs(bundle));

    if (bundle)
        bundle->GetStringFromName(
            NS_LITERAL_STRING("brandShortName").get(),
            getter_Copies(brandName));

    if (brandName.IsEmpty())
        brandName.Assign(NS_LITERAL_STRING("Mozilla"));
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

    NS_ASSERTION(aInitData->mWindowType != eWindowType_popup ||
                 !aParent, "Popups should not be hooked into nsIWidget hierarchy");

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
    if (mWindowType != eWindowType_child) {
        // The window manager might place us. Indicate that if we're
        // shown, we want to go through
        // nsWindow::NativeResize(x,y,w,h) to maybe set our own
        // position.
        mNeedsMove = PR_TRUE;
    }

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

    GdkVisual* visual = nsnull;
#ifdef MOZ_ENABLE_GLITZ
    if (gfxPlatform::UseGlitz()) {
        nsCOMPtr<nsIDeviceContext> dc = aContext;
        if (!dc) {
            nsCOMPtr<nsIDeviceContext> dc = do_CreateInstance(kDeviceContextCID);
            // no parent widget to initialize with
            dc->Init(nsnull);
        }

        Display* dpy = GDK_DISPLAY();
        int defaultScreen = gdk_x11_get_default_screen();
        glitz_drawable_format_t* format = glitz_glx_find_window_format (dpy, defaultScreen,
                                                                        0, NULL, 0);
        if (format) {
            XVisualInfo* vinfo = glitz_glx_get_visual_info_from_format(dpy, defaultScreen, format);
            GdkScreen* screen = gdk_display_get_screen(gdk_x11_lookup_xdisplay(dpy), defaultScreen);
            visual = gdk_x11_screen_lookup_visual(screen, vinfo->visualid);
        } else {
            // couldn't find a GLX visual; force Glitz off
            gfxPlatform::SetUseGlitz(PR_FALSE);
        }
    }
#endif

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
    case eWindowType_popup:
    case eWindowType_toplevel:
    case eWindowType_invisible: {
        mIsTopLevel = PR_TRUE;

        nsXPIDLString brandName;
        GetBrandName(brandName);
        NS_ConvertUTF16toUTF8 cBrand(brandName);

        if (mWindowType == eWindowType_dialog) {
            mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            SetDefaultIcon();
            gtk_window_set_wmclass(GTK_WINDOW(mShell), "Dialog", cBrand.get());
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
            gtk_window_set_wmclass(GTK_WINDOW(mShell), "Popup", cBrand.get());

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
            gtk_window_set_wmclass(GTK_WINDOW(mShell), "Toplevel", cBrand.get());

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
        mDrawingarea = moz_drawingarea_new(nsnull, mContainer, visual);

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
            mDrawingarea = moz_drawingarea_new(parentArea, parentMozContainer, visual);
        }
        else if (parentGtkContainer) {
            mContainer = MOZ_CONTAINER(moz_container_new());
            gtk_container_add(parentGtkContainer, GTK_WIDGET(mContainer));
            gtk_widget_realize(GTK_WIDGET(mContainer));

            mDrawingarea = moz_drawingarea_new(nsnull, mContainer, visual);
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
        g_signal_connect(G_OBJECT(mShell), "window_state_event",
                         G_CALLBACK(window_state_event_cb), NULL);

        GtkSettings* default_settings = gtk_settings_get_default();
        g_signal_connect_after(default_settings,
                               "notify::gtk-theme-name",
                               G_CALLBACK(theme_changed_cb), this);
        g_signal_connect_after(default_settings,
                               "notify::gtk-font-name",
                               G_CALLBACK(theme_changed_cb), this);
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

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
  if (!mShell)
    return NS_ERROR_FAILURE;

  nsXPIDLString brandName;
  GetBrandName(brandName);

  XClassHint *class_hint = XAllocClassHint();
  if (!class_hint)
    return NS_ERROR_OUT_OF_MEMORY;
  const char *role = NULL;
  class_hint->res_name = ToNewCString(xulWinType);
  if (!class_hint->res_name) {
    XFree(class_hint);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  class_hint->res_class = ToNewCString(brandName);
  if (!class_hint->res_class) {
    nsMemory::Free(class_hint->res_name);
    XFree(class_hint);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Parse res_name into a name and role. Characters other than
  // [A-Za-z0-9_-] are converted to '_'. Anything after the first
  // colon is assigned to role; if there's no colon, assign the
  // whole thing to both role and res_name.
  for (char *c = class_hint->res_name; *c; c++) {
    if (':' == *c) {
      *c = 0;
      role = c + 1;
    }
    else if (!isascii(*c) || (!isalnum(*c) && ('_' != *c) && ('-' != *c)))
      *c = '_';
  }
  class_hint->res_name[0] = toupper(class_hint->res_name[0]);
  if (!role) role = class_hint->res_name;

  gdk_window_set_role(GTK_WIDGET(mShell)->window, role);
  // Can't use gtk_window_set_wmclass() for this; it prints
  // a warning & refuses to make the change.
  XSetClassHint(GDK_DISPLAY(),
                GDK_WINDOW_XWINDOW(GTK_WIDGET(mShell)->window),
                class_hint);
  nsMemory::Free(class_hint->res_class);
  nsMemory::Free(class_hint->res_name);
  XFree(class_hint);
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
    mNeedsMove = PR_FALSE;

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
        if (!topWidget)
            return NS_ERROR_FAILURE;

        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        if (!topWindow)
            return NS_ERROR_FAILURE;

        return topWindow->SetWindowTranslucency(aTranslucent);
    }

    if (mIsTranslucent == aTranslucent)
        return NS_OK;

    if (!aTranslucent) {
        if (mTransparencyBitmap) {
            delete[] mTransparencyBitmap;
            mTransparencyBitmap = nsnull;
            mTransparencyBitmapWidth = 0;
            mTransparencyBitmapHeight = 0;
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
        if (!topWidget) {
            aTranslucent = PR_FALSE;
            return NS_ERROR_FAILURE;
        }

        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        if (!topWindow) {
            aTranslucent = PR_FALSE;
            return NS_ERROR_FAILURE;
        }

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

    if (aNewWidth == mTransparencyBitmapWidth &&
        aNewHeight == mTransparencyBitmapHeight)
        return;

    PRInt32 newSize = ((aNewWidth+7)/8)*aNewHeight;
    gchar* newBits = new gchar[newSize];
    if (!newBits) {
        delete[] mTransparencyBitmap;
        mTransparencyBitmap = nsnull;
        mTransparencyBitmapWidth = 0;
        mTransparencyBitmapHeight = 0;
        return;
    }
    // fill new mask with "opaque", first
    memset(newBits, 255, newSize);

    // Now copy the intersection of the old and new areas into the new mask
    PRInt32 copyWidth = PR_MIN(aNewWidth, mTransparencyBitmapWidth);
    PRInt32 copyHeight = PR_MIN(aNewHeight, mTransparencyBitmapHeight);
    PRInt32 oldRowBytes = (mTransparencyBitmapWidth+7)/8;
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
    mTransparencyBitmapWidth = aNewWidth;
    mTransparencyBitmapHeight = aNewHeight;
}

static PRBool
ChangedMaskBits(gchar* aMaskBits, PRInt32 aMaskWidth, PRInt32 aMaskHeight,
        const nsRect& aRect, PRUint8* aAlphas, PRInt32 aStride)
{
    PRInt32 x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    PRInt32 maskBytesPerRow = (aMaskWidth + 7)/8;
    for (y = aRect.y; y < yMax; y++) {
        gchar* maskBytes = aMaskBits + y*maskBytesPerRow;
        PRUint8* alphas = aAlphas;
        for (x = aRect.x; x < xMax; x++) {
            PRBool newBit = *alphas > 0;
            alphas++;

            gchar maskByte = maskBytes[x >> 3];
            PRBool maskBit = (maskByte & (1 << (x & 7))) != 0;

            if (maskBit != newBit) {
                return PR_TRUE;
            }
        }
        aAlphas += aStride;
    }

    return PR_FALSE;
}

static
void UpdateMaskBits(gchar* aMaskBits, PRInt32 aMaskWidth, PRInt32 aMaskHeight,
        const nsRect& aRect, PRUint8* aAlphas, PRInt32 aStride)
{
    PRInt32 x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    PRInt32 maskBytesPerRow = (aMaskWidth + 7)/8;
    for (y = aRect.y; y < yMax; y++) {
        gchar* maskBytes = aMaskBits + y*maskBytesPerRow;
        PRUint8* alphas = aAlphas;
        for (x = aRect.x; x < xMax; x++) {
            PRBool newBit = *alphas > 0;
            alphas++;

            gchar mask = 1 << (x & 7);
            gchar maskByte = maskBytes[x >> 3];
            // Note: '-newBit' turns 0 into 00...00 and 1 into 11...11
            maskBytes[x >> 3] = (maskByte & ~mask) | (-newBit & mask);
        }
        aAlphas += aStride;
    }
}

void
nsWindow::ApplyTransparencyBitmap()
{
    gtk_widget_reset_shapes(mShell);
    GdkBitmap* maskBitmap = gdk_bitmap_create_from_data(mShell->window,
            mTransparencyBitmap,
            mTransparencyBitmapWidth, mTransparencyBitmapHeight);
    if (!maskBitmap)
        return;

    gtk_widget_shape_combine_mask(mShell, maskBitmap, 0, 0);
    gdk_bitmap_unref(maskBitmap);
}

nsresult
nsWindow::UpdateTranslucentWindowAlphaInternal(const nsRect& aRect,
                                               PRUint8* aAlphas, PRInt32 aStride)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = nsnull;
        GetToplevelWidget(&topWidget);
        if (!topWidget)
            return NS_ERROR_FAILURE;

        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        if (!topWindow)
            return NS_ERROR_FAILURE;

        return topWindow->UpdateTranslucentWindowAlphaInternal(aRect, aAlphas, aStride);
    }

    NS_ASSERTION(mIsTranslucent, "Window is not transparent");

    if (mTransparencyBitmap == nsnull) {
        PRInt32 size = ((mBounds.width+7)/8)*mBounds.height;
        mTransparencyBitmap = new gchar[size];
        if (mTransparencyBitmap == nsnull)
            return NS_ERROR_FAILURE;
        memset(mTransparencyBitmap, 255, size);
        mTransparencyBitmapWidth = mBounds.width;
        mTransparencyBitmapHeight = mBounds.height;
    }

    NS_ASSERTION(aRect.x >= 0 && aRect.y >= 0
            && aRect.XMost() <= mBounds.width && aRect.YMost() <= mBounds.height,
            "Rect is out of window bounds");

    if (!ChangedMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height,
                         aRect, aAlphas, aStride))
        // skip the expensive stuff if the mask bits haven't changed; hopefully
        // this is the common case
        return NS_OK;

    UpdateMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height,
                   aRect, aAlphas, aStride);

    if (!mNeedsShow) {
        ApplyTransparencyBitmap();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas)
{
    return UpdateTranslucentWindowAlphaInternal(aRect, aAlphas, aRect.width);
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

void
nsWindow::SetUrgencyHint(GtkWidget *top_window, PRBool state)
{
    if (!top_window)
        return;

    // Try to get a pointer to gdk_window_set_urgency_hint
    PRLibrary* lib;
    _gdk_window_set_urgency_hint_fn _gdk_window_set_urgency_hint = nsnull;
    _gdk_window_set_urgency_hint = (_gdk_window_set_urgency_hint_fn)
           PR_FindFunctionSymbolAndLibrary("gdk_window_set_urgency_hint", &lib);

    if (_gdk_window_set_urgency_hint) {
        _gdk_window_set_urgency_hint(top_window->window, state);
        PR_UnloadLibrary(lib);
    }
    else if (state) {
        gdk_window_show_unraised(top_window->window);
    }
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
    nsCOMPtr<nsILocalFile> iconFile;
    ResolveIconName(NS_LITERAL_STRING("default"),
                    NS_LITERAL_STRING(".xpm"),
                    getter_AddRefs(iconFile));
    if (!iconFile) {
        NS_WARNING("default.xpm not found");
        return;
    }

    nsCAutoString path;
    iconFile->GetNativePath(path);

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
nsWindow::MakeFullScreen(PRBool aFullScreen)
{
#if GTK_CHECK_VERSION(2,2,0)
    if (aFullScreen)
        gdk_window_fullscreen (mShell->window);
    else
        gdk_window_unfullscreen (mShell->window);
#else
    HideWindowChrome(aFullScreen);
#endif
    return MakeFullScreenInternal(aFullScreen);
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
    gdk_window_hide(mShell->window);

    gint wmd;
    if (aShouldHide)
        wmd = 0;
    else
        wmd = ConvertBorderStyles(mBorderStyle);

    gdk_window_set_decorations(mShell->window, (GdkWMDecoration) wmd);

    gdk_window_show(mShell->window);

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
    case eCursor_n_resize:
        gdkcursor = gdk_cursor_new(GDK_TOP_SIDE);
        break;
    case eCursor_s_resize:
        gdkcursor = gdk_cursor_new(GDK_BOTTOM_SIDE);
        break;
    case eCursor_w_resize:
        gdkcursor = gdk_cursor_new(GDK_LEFT_SIDE);
        break;
    case eCursor_e_resize:
        gdkcursor = gdk_cursor_new(GDK_RIGHT_SIDE);
        break;
    case eCursor_nw_resize:
        gdkcursor = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
        break;
    case eCursor_se_resize:
        gdkcursor = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
        break;
    case eCursor_ne_resize:
        gdkcursor = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
        break;
    case eCursor_sw_resize:
        gdkcursor = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
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
    case eCursor_zoom_in:
        newType = MOZ_CURSOR_ZOOM_IN;
        break;
    case eCursor_zoom_out:
        newType = MOZ_CURSOR_ZOOM_OUT;
        break;
    case eCursor_not_allowed:
    case eCursor_no_drop:
        newType = MOZ_CURSOR_NOT_ALLOWED;
        break;
    case eCursor_col_resize:
        newType = MOZ_CURSOR_COL_RESIZE;
        break;
    case eCursor_row_resize:
        newType = MOZ_CURSOR_ROW_RESIZE;
        break;
    case eCursor_vertical_text:
        newType = MOZ_CURSOR_VERTICAL_TEXT;
        break;
    case eCursor_all_scroll:
        gdkcursor = gdk_cursor_new(GDK_FLEUR);
        break;
    case eCursor_nesw_resize:
        newType = MOZ_CURSOR_NESW_RESIZE;
        break;
    case eCursor_nwse_resize:
        newType = MOZ_CURSOR_NWSE_RESIZE;
        break;
    case eCursor_ns_resize:
        gdkcursor = gdk_cursor_new(GDK_SB_V_DOUBLE_ARROW);
        break;
    case eCursor_ew_resize:
        gdkcursor = gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW);
        break;
    default:
        NS_ASSERTION(aCursor, "Invalid cursor type");
        gdkcursor = gdk_cursor_new(GDK_LEFT_PTR);
        break;
    }

    // if by now we don't have a xcursor, this means we have to make a
    // custom one
    if (newType != 0xff) {
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
void
theme_changed_cb (GtkSettings *settings, GParamSpec *pspec, nsWindow *data)
{
    data->ThemeChanged();
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
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
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
    aCMEvent->eventStructType = NS_MOUSE_EVENT;
    aCMEvent->message = NS_CONTEXTMENU;
    aCMEvent->context = nsMouseEvent::eContextMenuKey;
    aCMEvent->button = nsMouseEvent::eRightButton;
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
    nsCOMPtr<nsIAccessible> accessible, parentAccessible;
    DispatchAccessibleEvent(getter_AddRefs(accessible));
    PRUint32 role;

    if (!accessible) {
        return;
    }
    while (PR_TRUE) {
        accessible->GetParent(getter_AddRefs(parentAccessible));
        if (!parentAccessible) {
            break;
        }
        parentAccessible->GetRole(&role);
        if (role == nsIAccessible::ROLE_APP_ROOT) {
            NS_ADDREF(*aAccessible = accessible);
            break;
        }
        accessible = parentAccessible;
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
    nsAccessibleEvent event(PR_TRUE, NS_GETACCESSIBLE, this);

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

    nsCommonWidget::DispatchActivateEvent();
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
nsWindow::IMEInitData(void)
{
    if (mIMEData)
        return;
    nsWindow *win = IMEGetOwningWindow();
    if (!win)
        return;
    mIMEData = win->mIMEData;
    if (!mIMEData)
        return;
    mIMEData->mRefCount++;
}

void
nsWindow::IMEReleaseData(void)
{
    if (!mIMEData)
        return;

    mIMEData->mRefCount--;
    if (mIMEData->mRefCount != 0)
        return;

    delete mIMEData;
    mIMEData = nsnull;
}

void
nsWindow::IMEDestroyContext(void)
{
    if (!mIMEData || mIMEData->mOwner != this) {
        // This nsWindow is not the owner of the instance of mIMEData.
        if (IMEComposingWindow() == this)
            CancelIMEComposition();
        if (gIMEFocusWindow == this)
            gIMEFocusWindow = nsnull;
        IMEReleaseData();
        return;
    }

    /* NOTE:
     * This nsWindow is the owner of the instance of mIMEData,
     * so, we must release the contexts now. But that might be referred from
     * other nsWindows.(They are children of this. But we don't know why there
     * are the cases.) So, we need to clear the pointers that refers to contexts
     * and this if the other referrers are still alive. See bug 349727.
     */

    // If this is the focus window and we have an IM context we need
    // to unset the focus on this window before we destroy the window.
    GtkIMContext *im = IMEGetContext();
    if (im && gIMEFocusWindow && gIMEFocusWindow->IMEGetContext() == im) {
        gIMEFocusWindow->IMELoseFocus();
        gIMEFocusWindow = nsnull;
    }

    mIMEData->mOwner   = nsnull;
    mIMEData->mEnabled = PR_FALSE;

    if (mIMEData->mContext) {
        gtk_im_context_set_client_window(mIMEData->mContext, nsnull);
        g_object_unref(G_OBJECT(mIMEData->mContext));
        mIMEData->mContext = nsnull;
    }

    if (mIMEData->mDummyContext) {
        gtk_im_context_set_client_window(mIMEData->mDummyContext, nsnull);
        g_object_unref(G_OBJECT(mIMEData->mDummyContext));
        mIMEData->mDummyContext = nsnull;
    }

    IMEReleaseData();
}

void
nsWindow::IMESetFocus(void)
{
    IMEInitData();

    LOGIM(("IMESetFocus %p\n", (void *)this));
    GtkIMContext *im = IMEGetContext();
    if (!im)
        return;

    gtk_im_context_focus_in(im);
    gIMEFocusWindow = this;

    if (!IMEIsEnabled()) {
        // We should release IME focus for uim and scim.
        // These IMs are using snooper that is released at losing focus.
        IMELoseFocus();
    }
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

    if (!mIMEData) {
        NS_ERROR("This widget doesn't support IM");
        return;
    }

    if (IMEComposingWindow()) {
        NS_WARNING("tried to re-start text composition\n");
        return;
    }

    mIMEData->mComposingWindow = this;

    nsCompositionEvent compEvent(PR_TRUE, NS_COMPOSITION_START, this);

    nsEventStatus status;
    DispatchEvent(&compEvent, status);

    gint x1, y1, x2, y2;
    GtkWidget *widget =
        get_gtk_widget_for_gdk_window(this->mDrawingarea->inner_window);

    gdk_window_get_origin(widget->window, &x1, &y1);
    gdk_window_get_origin(this->mDrawingarea->inner_window, &x2, &y2);

    GdkRectangle area;
    area.x = compEvent.theReply.mCursorPosition.x + (x2 - x1);
    area.y = compEvent.theReply.mCursorPosition.y + (y2 - y1);
    area.width  = 0;
    area.height = compEvent.theReply.mCursorPosition.height;

    gtk_im_context_set_cursor_location(IMEGetContext(), &area);
}

void
nsWindow::IMEComposeText (const PRUnichar *aText,
                          const PRInt32 aLen,
                          const gchar *aPreeditString,
                          const gint aCursorPos,
                          const PangoAttrList *aFeedback)
{
    if (!mIMEData) {
        NS_ERROR("This widget doesn't support IM");
        return;
    }

    // Send our start composition event if we need to
    if (!IMEComposingWindow())
        IMEComposeStart();

    LOGIM(("IMEComposeText\n"));
    nsTextEvent textEvent(PR_TRUE, NS_TEXT_TEXT, this);

    if (aLen != 0) {
        textEvent.theText = (PRUnichar*)aText;

        if (aPreeditString && aFeedback && (aLen > 0)) {
            IM_set_text_range(aLen, aPreeditString, aCursorPos, aFeedback,
                              &(textEvent.rangeCount),
                              &(textEvent.rangeArray));
        }
    }

    nsEventStatus status;
    DispatchEvent(&textEvent, status);

    if (textEvent.rangeArray) {
        delete[] textEvent.rangeArray;
    }

    gint x1, y1, x2, y2;
    GtkWidget *widget =
        get_gtk_widget_for_gdk_window(this->mDrawingarea->inner_window);

    gdk_window_get_origin(widget->window, &x1, &y1);
    gdk_window_get_origin(this->mDrawingarea->inner_window, &x2, &y2);

    GdkRectangle area;
    area.x = textEvent.theReply.mCursorPosition.x + (x2 - x1);
    area.y = textEvent.theReply.mCursorPosition.y + (y2 - y1);
    area.width  = 0;
    area.height = textEvent.theReply.mCursorPosition.height;

    gtk_im_context_set_cursor_location(IMEGetContext(), &area);
}

void
nsWindow::IMEComposeEnd(void)
{
    LOGIM(("IMEComposeEnd [%p]\n", (void *)this));
    NS_ASSERTION(mIMEData, "This widget doesn't support IM");

    if (!IMEComposingWindow()) {
        NS_WARNING("tried to end text composition before it was started");
        return;
    }

    mIMEData->mComposingWindow = nsnull;

    nsCompositionEvent compEvent(PR_TRUE, NS_COMPOSITION_END, this);

    nsEventStatus status;
    DispatchEvent(&compEvent, status);
}

GtkIMContext*
nsWindow::IMEGetContext()
{
    return IM_get_input_context(this);
}

PRBool
nsWindow::IMEIsEnabled(void)
{
    return mIMEData ? mIMEData->mEnabled : PR_FALSE;
}

nsWindow*
nsWindow::IMEComposingWindow(void)
{
    return mIMEData ? mIMEData->mComposingWindow : nsnull;
}

nsWindow*
nsWindow::IMEGetOwningWindow(void)
{
    nsWindow *window = IM_get_owning_window(this->mDrawingarea);
    return window;
}

void
nsWindow::IMECreateContext(void)
{
    mIMEData = new nsIMEData(this);
    if (!mIMEData)
        return;

    mIMEData->mContext = gtk_im_multicontext_new();
    mIMEData->mDummyContext = gtk_im_multicontext_new();
    if (!mIMEData->mContext || !mIMEData->mDummyContext) {
        NS_ERROR("failed to create IM context.");
        IMEDestroyContext();
        return;
    }

    gtk_im_context_set_client_window(mIMEData->mContext,
                                     GTK_WIDGET(mContainer)->window);
    gtk_im_context_set_client_window(mIMEData->mDummyContext,
                                     GTK_WIDGET(mContainer)->window);

    g_signal_connect(G_OBJECT(mIMEData->mContext), "preedit_changed",
                     G_CALLBACK(IM_preedit_changed_cb), this);
    g_signal_connect(G_OBJECT(mIMEData->mContext), "commit",
                     G_CALLBACK(IM_commit_cb), this);
}

PRBool
nsWindow::IMEFilterEvent(GdkEventKey *aEvent)
{
    if (!IMEIsEnabled())
        return FALSE;

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

/* nsIKBStateControl */
NS_IMETHODIMP
nsWindow::ResetInputState()
{
    IMEInitData();

    nsWindow *win = IMEComposingWindow();
    if (win) {
        GtkIMContext *im = IMEGetContext();
        if (!im)
            return NS_OK;

        gchar *preedit_string;
        gint cursor_pos;
        PangoAttrList *feedback_list;
        gtk_im_context_get_preedit_string(im, &preedit_string,
                                          &feedback_list, &cursor_pos);
        if (preedit_string && *preedit_string) {
            IM_commit_cb_internal(preedit_string, win);
            g_free(preedit_string);
        }
        if (feedback_list)
            pango_attr_list_unref(feedback_list);
    }

    CancelIMEComposition();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIMEOpenState(PRBool aState)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::GetIMEOpenState(PRBool* aState)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetIMEEnabled(PRBool aState)
{
    IMEInitData();

    if (!mIMEData || mIMEData->mEnabled == aState)
        return NS_OK;

    GtkIMContext *focusedIm = nsnull;
    // XXX Don't we need to check gFocusWindow?
    nsWindow *focusedWin = gIMEFocusWindow;
    if (focusedWin && focusedWin->mIMEData)
        focusedIm = focusedWin->mIMEData->mContext;

    if (focusedIm && focusedIm == mIMEData->mContext) {
        // Release current IME focus if IME is enabled.
        if (mIMEData->mEnabled) {
            focusedWin->ResetInputState();
            focusedWin->IMELoseFocus();
        }

        mIMEData->mEnabled = aState;

        // Even when aState is not PR_TRUE, we need to set IME focus.
        // Because some IMs are updating the status bar of them in this time.
        focusedWin->IMESetFocus();
    } else {
        if (mIMEData->mEnabled)
            ResetInputState();
        mIMEData->mEnabled = aState;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetIMEEnabled(PRBool* aState)
{
    NS_ENSURE_ARG_POINTER(aState);

    IMEInitData();

    *aState = IMEIsEnabled();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CancelIMEComposition()
{
    IMEInitData();

    GtkIMContext *im = IMEGetContext();
    if (!im)
        return NS_OK;

    NS_ASSERTION(!gIMESuppressCommit,
                 "CancelIMEComposition is already called!");
    gIMESuppressCommit = PR_TRUE;
    gtk_im_context_reset(im);
    gIMESuppressCommit = PR_FALSE;

    nsWindow *win = IMEComposingWindow();
    if (win) {
        win->IMEComposeText(nsnull, 0, nsnull, 0, nsnull);
        win->IMEComposeEnd();
    }

    return NS_OK;
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
    // Of course!!!
    gtk_im_context_get_preedit_string(aContext, &preedit_string,
                                      &feedback_list, &cursor_pos);

    LOGIM(("preedit string is: %s   length is: %d\n",
           preedit_string, strlen(preedit_string)));

    if (!preedit_string || !*preedit_string) {
        LOGIM(("preedit ended\n"));
        window->IMEComposeText(NULL, 0, NULL, 0, NULL);
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
                               uniStrLen, preedit_string, cursor_pos, feedback_list);
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
    if (gIMESuppressCommit)
        return;

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
    IM_commit_cb_internal(aUtf8_str, window);
}

/* static */
void
IM_commit_cb_internal (const gchar  *aUtf8_str,
                       nsWindow     *aWindow)
{
    gunichar2 *uniStr = nsnull;
    glong uniStrLen = 0;
    uniStr = g_utf8_to_utf16(aUtf8_str, -1, NULL, &uniStrLen, NULL);

    if (!uniStr) {
        LOGIM(("utf80utf16 string tranfer failed!\n"));
        return;
    }

    if (uniStrLen) {
        aWindow->IMEComposeText((const PRUnichar *)uniStr,
                                (PRInt32)uniStrLen, nsnull, 0, nsnull);
        aWindow->IMEComposeEnd();
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
                  const gint aCursorPos,
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
    START_OFFSET(0) = aCursorPos;
    END_OFFSET(0) = aCursorPos;

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

        PRUint32 feedbackType = 0;
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
nsWindow *
IM_get_owning_window(MozDrawingarea *aArea)
{
    if (!aArea)
        return nsnull;
    GtkWidget *owningWidget =
        get_gtk_widget_for_gdk_window(aArea->inner_window);
    if (!owningWidget)
        return nsnull;
    return get_window_for_gtk_widget(owningWidget);
}

/* static */
GtkIMContext *
IM_get_input_context(nsWindow *aWindow)
{
    if (!aWindow)
        return nsnull;
    nsWindow::nsIMEData *data = aWindow->mIMEData;
    if (!data)
        return nsnull;
    return data->mEnabled ? data->mContext : data->mDummyContext;
}

#endif

#ifdef MOZ_CAIRO_GFX
// return the gfxASurface for rendering to this widget
gfxASurface*
nsWindow::GetThebesSurface()
{
    // XXXvlad always create a new thebes surface for now,
    // because the old clip doesn't get cleared otherwise.
    // we should fix this at some point, and just reset
    // the clip.
    mThebesSurface = nsnull;

    if (!mThebesSurface) {
        GdkDrawable* d = GDK_DRAWABLE(mDrawingarea->inner_window);
        gint width, height;
        gdk_drawable_get_size(d, &width, &height);
        if (!gfxPlatform::UseGlitz()) {
            mThebesSurface = new gfxXlibSurface
                (GDK_WINDOW_XDISPLAY(d),
                 GDK_WINDOW_XWINDOW(d),
                 GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(d)),
                 gfxIntSize(width, height));
            gfxPlatformGtk::GetPlatform()->SetSurfaceGdkWindow(mThebesSurface, GDK_WINDOW(d));
        } else {
#ifdef MOZ_ENABLE_GLITZ
            glitz_surface_t *gsurf;
            glitz_drawable_t *gdraw;

            glitz_drawable_format_t *gdformat = glitz_glx_find_window_format (GDK_DISPLAY(),
                                                                              gdk_x11_get_default_screen(),
                                                                              0, NULL, 0);
            if (!gdformat)
                NS_ERROR("Failed to find glitz drawable format");

            Display* dpy = GDK_WINDOW_XDISPLAY(d);
            Window wnd = GDK_WINDOW_XWINDOW(d);
            
            gdraw =
                glitz_glx_create_drawable_for_window (dpy,
                                                      DefaultScreen(dpy),
                                                      gdformat,
                                                      wnd,
                                                      width,
                                                      height);
            glitz_format_t *gformat =
                glitz_find_standard_format (gdraw, GLITZ_STANDARD_RGB24);
            gsurf =
                glitz_surface_create (gdraw,
                                      gformat,
                                      width,
                                      height,
                                      0,
                                      NULL);
            glitz_surface_attach (gsurf, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);


            //fprintf (stderr, "## nsThebesDrawingSurface::Init Glitz DRAWABLE %p (DC: %p)\n", aWidget, aDC);
            mThebesSurface = new gfxGlitzSurface (gdraw, gsurf, PR_TRUE);
#endif
        }
    }

    return mThebesSurface;
}
#endif
