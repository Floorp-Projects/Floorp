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

#include "nsIPref.h"
#include "nsIServiceManager.h"

/* For SetIcon */
#include "nsSpecialSystemDirectory.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"

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
static GdkFilterReturn plugin_window_filter_func (GdkXEvent *gdk_xevent, 
                                                  GdkEvent *event, 
                                                  gpointer data);
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
static nsresult    initialize_default_icon (void);
  
// this is the last window that had a drag event happen on it.
nsWindow *nsWindow::mLastDragMotionWindow = NULL;

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

static PRBool            gJustGotActivate      = PR_FALSE;
static PRBool            gGlobalsInitialized   = PR_FALSE;
static PRBool            gRaiseWindows         = PR_TRUE;

nsCOMPtr  <nsIRollupListener> gRollupListener;
nsWeakPtr                     gRollupWindow;

#ifdef USE_XIM

struct nsXICLookupEntry {
    PLDHashEntryHdr mKeyHash;
    nsWindow*   mShellWindow;
    GtkIMContext* mXIC;
};

PLDHashTable nsWindow::gXICLookupTable;
nsWindow *nsWindow::gFocusedWindow;

static void IM_commit_cb              (GtkIMContext *context,
                                       const gchar *str,
                                       nsWindow *window);
static void IM_preedit_changed_cb     (GtkIMContext *context,
                                       nsWindow *window);
static void IMSetTextRange            (const PRInt32 aLen,
                                       const gchar *aPreeditString,
                                       const PangoAttrList *aFeedback,
                                       PRUint32 *aTextRangeListLengthResult,
                                       nsTextRangeArray *aTextRangeListResult);

// If after selecting profile window, the startup fail, please refer to
// http://bugzilla.gnome.org/show_bug.cgi?id=88940
#endif

#define kWindowPositionSlop 20

// cursor cache
GdkCursor *gCursorCache[eCursor_count_up_down + 1];

nsWindow::nsWindow()
{
    mFocusChild          = nsnull;
    mContainer           = nsnull;
    mDrawingarea         = nsnull;
    mShell               = nsnull;
    mWindowGroup         = nsnull;
    mContainerGotFocus   = PR_FALSE;
    mContainerLostFocus  = PR_FALSE;
    mContainerBlockFocus = PR_FALSE;
    mHasFocus            = PR_FALSE;
    mInKeyRepeat         = PR_FALSE;
    mIsVisible           = PR_FALSE;
    mRetryPointerGrab    = PR_FALSE;
    mRetryKeyboardGrab   = PR_FALSE;
    mHasNonXembedPlugin  = PR_FALSE;
    mTransientParent     = nsnull;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mOldFocusWindow      = 0;

    if (!gGlobalsInitialized) {
        gGlobalsInitialized = PR_TRUE;

        // It's OK if either of these fail, but it may not be one day.
        initialize_prefs();
        initialize_default_icon();
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
    if (gXICLookupTable.ops == NULL) {
        PL_DHashTableInit(&gXICLookupTable, PL_DHashGetStubOps(), nsnull,
            sizeof(nsXICLookupEntry), PL_DHASH_MIN_SIZE);
    }
    mIMEShellWindow = nsnull;
#endif

#ifdef ACCESSIBILITY
    mTopLevelAccessible  = nsnull;
#endif
}

#ifdef USE_XIM
void
nsWindow::IMEGetShellWindow(void)
{
    GtkWidget* top_window = nsnull;
    GetToplevelWidget(&top_window);
    if (top_window) {
        mIMEShellWindow = get_window_for_gtk_widget(top_window);
    }
}

GtkIMContext*
nsWindow::IMEGetContext()
{
    if (!mIMEShellWindow) {
        return NULL;
    }
    PLDHashEntryHdr* hash_entry;
    nsXICLookupEntry* entry;

    hash_entry = PL_DHashTableOperate(&gXICLookupTable,
                                      mIMEShellWindow, PL_DHASH_LOOKUP);

    if (hash_entry) {
        entry = NS_REINTERPRET_CAST(nsXICLookupEntry *, hash_entry);
        if (entry->mXIC) {
            return entry->mXIC;
        }
    }
    return NULL;
}

void
nsWindow::IMECreateContext(GdkWindow* aGdkWindow)
{
    PLDHashEntryHdr* hash_entry;
    nsXICLookupEntry* entry;
    GtkIMContext *im = gtk_im_multicontext_new();
    if (im) {
        hash_entry = PL_DHashTableOperate(&gXICLookupTable, this, PL_DHASH_ADD);
        if (hash_entry) {
            entry = NS_REINTERPRET_CAST(nsXICLookupEntry *, hash_entry);
            entry->mShellWindow = this;
            entry->mXIC = im;
        }
        gtk_im_context_set_client_window(im, aGdkWindow);
        g_signal_connect(G_OBJECT(im), "commit",
                         G_CALLBACK(IM_commit_cb), this);
        g_signal_connect(G_OBJECT(im), "preedit_changed",
                         G_CALLBACK(IM_preedit_changed_cb), this);
        this->mIMEShellWindow = this;
    }
}
#endif

nsWindow::~nsWindow()
{
    LOG(("nsWindow::~nsWindow() [%p]\n", (void *)this));
    if (mLastDragMotionWindow == this) {
        mLastDragMotionWindow = NULL;
    }
    Destroy();
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
#ifdef ACCESSIBILITY
    CreateTopLevelAccessible();
#endif
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
#ifdef ACCESSIBILITY
    CreateTopLevelAccessible();
#endif
    return rv;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    if (mIsDestroyed)
        return NS_OK;

    LOG(("nsWindow::Destroy [%p]\n", (void *)this));
    mIsDestroyed = PR_TRUE;

    // ungrab if required
    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);
    if (NS_STATIC_CAST(nsIWidget *, this) == rollupWidget.get()) {
        if (gRollupListener)
            gRollupListener->Rollup();
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
    }

    NativeShow(PR_FALSE);

    // walk the list of children and call destroy on them.
    nsCOMPtr<nsIEnumerator> children = dont_AddRef(GetChildren());
    if (children) {
        nsCOMPtr<nsISupports> isupp;
        nsCOMPtr<nsIWidget> child;
        while (NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(isupp))
                            && isupp)) {
            child = do_QueryInterface(isupp);
            if (child) {
                child->Destroy();
            }

            if (NS_FAILED(children->Next()))
                break;
        }
    }

#ifdef USE_XIM
    GtkIMContext *im = IMEGetContext();
    // unset focus before destroy
    if (im && !mShell && mIMEShellWindow == gFocusedWindow) {
        gtk_im_context_focus_out(im);
    }
    // if shell, delete GtkIMContext
    if (im && mShell) {
        gtk_im_context_reset(im);
        PL_DHashTableOperate(&gXICLookupTable, this, PL_DHASH_REMOVE);
        g_object_unref(G_OBJECT(im));
    }
    mIMEShellWindow = nsnull;
#endif

    // make sure that we remove ourself as the focus window
    if (mHasFocus) {
        LOG(("automatically losing focus...\n"));
        mHasFocus = PR_FALSE;
        // get the owning gtk widget and the nsWindow for that widget and
        // remove ourselves as the focus widget tracked in that window
        nsWindow *owningWindow =
            get_owning_window_for_gdk_window(mDrawingarea->inner_window);
        owningWindow->mFocusChild = nsnull;
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
    if (mTopLevelAccessible) {
        nsAccessibilityInterface::RemoveTopLevel(mTopLevelAccessible);
        mTopLevelAccessible = nsnull;
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
    if (aX == mBounds.x && aY == mBounds.y)
        return NS_OK;

    LOG(("nsWindow::Move [%p] %d %d\n", (void *)this,
         aX, aY));

    mBounds.x = aX;
    mBounds.y = aY;

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
    else {
        moz_drawingarea_move(mDrawingarea, aX, aY);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsIWidget *aWidget,
                      PRBool     aActivate)
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

    LOG(("SetFocus [%p]\n", (void *)this));

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
        GetAttention();

    nsWindow  *owningWindow = get_window_for_gtk_widget(owningWidget);
    if (!owningWindow)
        return NS_ERROR_FAILURE;

    if (!GTK_WIDGET_HAS_FOCUS(owningWidget)) {

        LOG(("grabbing focus for the toplevel\n"));
        owningWindow->mContainerBlockFocus = PR_TRUE;
        gtk_widget_grab_focus(owningWidget);
        owningWindow->mContainerBlockFocus = PR_FALSE;

        DispatchGotFocusEvent();

        // unset the activate flag
        if (gJustGotActivate) {
            gJustGotActivate = PR_FALSE;
            DispatchActivateEvent();
        }

        return NS_OK;
    }

    // If this is the widget that already has focus, return.
    if (mHasFocus) {
        LOG(("already have focus...\n"));
        return NS_OK;
    }

    // If there is already a focued child window, dispatch a LOSTFOCUS
    // event from that widget and unset its got focus flag.
    if (owningWindow->mFocusChild) {
        LOG(("removing focus child %p\n", (void *)owningWindow->mFocusChild));
        owningWindow->mFocusChild->LoseFocus();
    }

    // Set this window to be the focused child window, update our has
    // focus flag and dispatch a GOTFOCUS event.
    owningWindow->mFocusChild = this;
    mHasFocus = PR_TRUE;

#ifdef USE_XIM
    GtkIMContext *im = IMEGetContext();
    if (!mIsTopLevel && im && !mFocusChild) {
        gFocusedWindow = this;
        gtk_im_context_focus_in(im);
    }
#endif

    DispatchGotFocusEvent();
    
    // make sure to unset the activate flag and send an activate event
    if (gJustGotActivate) {
        gJustGotActivate = PR_FALSE;
        DispatchActivateEvent();
    }

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
    if (!mContainer) {
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

    LOG(("Invalidate (all) [%p]: %d %d %d %d\n", (void *)this,
         rect.x, rect.y, rect.width, rect.height));

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

    LOG(("Invalidate (rect) [%p]: %d %d %d %d\n", (void *)this,
         rect.x, rect.y, rect.width, rect.height));

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

    if (region) {
        GdkRectangle rect;
        gdk_region_get_clipbox(region, &rect);

        LOG(("Invalidate (region) [%p]: %d %d %d %d\n", (void *)this,
             rect.x, rect.y, rect.width, rect.height));

        gdk_window_invalidate_region(mDrawingarea->inner_window,
                                     region, TRUE);
    }
    else {
        LOG(("Invalidate (region) [%p] with empty region\n",
             (void *)this));
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Update()
{
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
    moz_drawingarea_scroll(mDrawingarea, aDx, aDy);

    // Update bounds on our child windows
    nsCOMPtr<nsIEnumerator> children = dont_AddRef(GetChildren());
    if (children) {
        nsCOMPtr<nsISupports> isupp;
        nsCOMPtr<nsIWidget> child;
        while (NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(isupp))
                            && isupp)) {
            child = do_QueryInterface(isupp);
            if (child) {
                nsRect bounds;
                child->GetBounds(bounds);
                bounds.x += aDx;
                bounds.y += aDy;
                NS_STATIC_CAST(nsBaseWidget*, 
                               (nsIWidget*)child)->SetBounds(bounds);
            }

            if (NS_FAILED(children->Next()))
                break;
        }
    }

    // Process all updates so that everything is drawn.
    gdk_window_process_all_updates();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScrollWidgets(PRInt32 aDx,
                        PRInt32 aDy)
{
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
    case NS_NATIVE_WIDGET:
        return mDrawingarea->inner_window;
        break;

    case NS_NATIVE_PLUGIN_PORT:
        return SetupPluginPort();
        break;

    case NS_NATIVE_DISPLAY:
        return GDK_DISPLAY();
        break;

    case NS_NATIVE_GRAPHIC:
        NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
        return (void *)NS_STATIC_CAST(nsToolkit *, mToolkit)->GetSharedGC();
        break;

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
nsWindow::SetTitle(const nsString& aTitle)
{
    if (!mShell)
        return NS_OK;

    // convert the string into utf8 and set the title.
    NS_ConvertUCS2toUTF8 utf8title(aTitle);
    gtk_window_set_title(GTK_WINDOW(mShell), (const char *)utf8title.get());

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAString& anIconSpec)
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
    nsAutoString iconSpec(anIconSpec);
    // ...append ".xpm" to it
    iconSpec.Append(NS_LITERAL_STRING(".xpm"));

    // ...and figure out where /chrome/... is within that
    // (and skip the "resource:///chrome" part).
    nsAutoString key(NS_LITERAL_STRING("/chrome/"));
    PRInt32 n = iconSpec.Find(key) + key.Length();

    // Append that to icon resource path.
    iconPath.Append(iconSpec.get() + n - 1 );

    nsCOMPtr<nsILocalFile> pathConverter;
    rv = NS_NewLocalFile(iconPath, PR_TRUE,
                         getter_AddRefs(pathConverter));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString aPath;
    pathConverter->GetNativePath(aPath);

    LOG(("nsWindow::SetIcon using path %s\n", aPath.get()));

    GdkPixbuf *iconPixbuf = gdk_pixbuf_new_from_file(aPath.get(), NULL);
    if (!iconPixbuf)
        return NS_ERROR_FAILURE;

    gtk_window_set_icon(GTK_WINDOW(mShell), iconPixbuf);
    g_object_unref(G_OBJECT(iconPixbuf));
    return NS_OK;
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
    else {
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
    GtkWidget *widget = 
        get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);

    LOG(("CaptureRollupEvents %p\n", (void *)this));

    if (aDoCapture) {
        gRollupListener = aListener;
        gRollupWindow =
            getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWidget*,
                                                              this)));
        gtk_grab_add(widget);
        GrabPointer();
        GrabKeyboard();
    }
    else {
        ReleaseGrabs();
        gtk_grab_remove(widget);
        gRollupListener = nsnull;
        gRollupWindow = nsnull;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetAttention()
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
#ifdef USE_XIM
    GtkIMContext *im = IMEGetContext();
    if (!mIsTopLevel && im && !mFocusChild) {
        gtk_im_context_focus_out(im);
        IMEComposeStart();
        IMEComposeText(NULL, 0, NULL, NULL);
        IMEComposeEnd();
        LOG(("gtk_im_context_focus_out\n"));
    }
#endif

    // we don't have focus
    mHasFocus = PR_FALSE;

    // make sure that we reset our repeat counter so the next keypress
    // for this widget will get the down event
    mInKeyRepeat = PR_FALSE;

    // Dispatch a lostfocus event
    DispatchLostFocusEvent();
}

gboolean
nsWindow::OnExposeEvent(GtkWidget *aWidget, GdkEventExpose *aEvent)
{
    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, (void *)aEvent->window));
        return NS_OK;
    }

    // handle exposes for the inner window only
    if (aEvent->window != mDrawingarea->inner_window)
        return FALSE;

    LOG(("sending expose event [%p] %p 0x%lx\n\t%d %d %d %d\n",
         (void *)this,
         (void *)aEvent->window,
         GDK_WINDOW_XWINDOW(aEvent->window),
         aEvent->area.x, aEvent->area.y,
         aEvent->area.width, aEvent->area.height));

    // ok, send out the paint event
    // XXX figure out the region/rect stuff!
    nsRect rect(aEvent->area.x, aEvent->area.y,
                aEvent->area.width, aEvent->area.height);
    nsPaintEvent event;

    InitPaintEvent(event);

    event.point.x = aEvent->area.x;
    event.point.y = aEvent->area.y;
    event.rect = &rect;
    // XXX fix this!
    event.region = nsnull;
    // XXX fix this!
    event.renderingContext = GetRenderingContext();

    nsEventStatus status;
    DispatchEvent(&event, status);

    NS_RELEASE(event.renderingContext);

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

    nsGUIEvent event;
    InitGUIEvent(event, NS_MOVE);

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

    moz_drawingarea_resize (mDrawingarea, rect.width, rect.height);

    nsEventStatus status;
    DispatchResizeEvent (rect, status);
}

void
nsWindow::OnDeleteEvent(GtkWidget *aWidget, GdkEventAny *aEvent)
{
    nsGUIEvent event;

    InitGUIEvent(event, NS_XUL_CLOSE);

    event.point.x = 0;
    event.point.y = 0;

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnEnterNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
    nsMouseEvent event;
    InitMouseEvent(event, NS_MOUSE_ENTER);

    event.point.x = nscoord(aEvent->x);
    event.point.y = nscoord(aEvent->y);

    LOG(("OnEnterNotify: %p\n", (void *)this));

    // if we have a non-xembed plugin (java, basically) dispatch focus
    // to it because it's too dumb not to know how to do it itself.
    if( mHasNonXembedPlugin ) {
        Window curFocusWindow;
        int    focusState;
        XGetInputFocus(GDK_WINDOW_XDISPLAY (aEvent->window), 
                       &curFocusWindow, 
                       &focusState);
        if(curFocusWindow != GDK_WINDOW_XWINDOW(aEvent->window)) {
            mOldFocusWindow = curFocusWindow;
            XSetInputFocus(GDK_WINDOW_XDISPLAY (aEvent->window),
                           GDK_WINDOW_XWINDOW(aEvent->window),
                           RevertToParent,
                           gtk_get_current_event_time ());
        }
        gdk_flush();
    }

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnLeaveNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
    nsMouseEvent event;
    InitMouseEvent(event, NS_MOUSE_EXIT);

    event.point.x = nscoord(aEvent->x);
    event.point.y = nscoord(aEvent->y);

    LOG(("OnLeaveNotify: %p\n", (void *)this));

    // if we have a non-xembed plugin (java, basically) take focus
    // back since it's not going to give it up by itself.
    if( mHasNonXembedPlugin ) {
        XSetInputFocus(GDK_WINDOW_XDISPLAY (aEvent->window),
                       mOldFocusWindow,
                       RevertToParent,
                       gtk_get_current_event_time ());
        gdk_flush();
    }

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnMotionNotifyEvent(GtkWidget *aWidget, GdkEventMotion *aEvent)
{
    // see if we can compress this event
    XEvent xevent;
    PRPackedBool synthEvent = PR_FALSE;
    while (XCheckWindowEvent(GDK_WINDOW_XDISPLAY(aEvent->window),
                             GDK_WINDOW_XWINDOW(aEvent->window),
                             ButtonMotionMask, &xevent)) {
        synthEvent = PR_TRUE;
    }

    nsMouseEvent event;
    InitMouseEvent(event, NS_MOUSE_MOVE);

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
    nsMouseEvent  event;
    PRUint32      eventType;
    nsEventStatus status;

    // check to see if we should rollup
    if (gJustGotActivate) {
        gJustGotActivate = PR_FALSE;
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

    InitButtonEvent(event, eventType, aEvent);

    DispatchEvent(&event, status);

    // right menu click on linux should also pop up a context menu
    if (eventType == NS_MOUSE_RIGHT_BUTTON_DOWN) {
        InitButtonEvent(event, NS_CONTEXTMENU, aEvent);
        DispatchEvent(&event, status);
    }
}

void
nsWindow::OnButtonReleaseEvent(GtkWidget *aWidget, GdkEventButton *aEvent)
{
    nsMouseEvent  event;
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

    InitButtonEvent(event, eventType, aEvent);

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnContainerFocusInEvent(GtkWidget *aWidget, GdkEventFocus *aEvent)
{
    // Return if someone has blocked events for this widget.  This will
    // happen if someone has called gtk_widget_grab_focus() from
    // nsWindow::SetFocus() and will prevent recursion.
    if (mContainerBlockFocus)
        return;

    if (mIsTopLevel)
        gJustGotActivate = PR_TRUE;

    // dispatch a got focus event
    DispatchGotFocusEvent();

    // send the activate event if it wasn't already sent via any
    // SetFocus() calls that were the result of the GOTFOCUS event
    // above.
    if (gJustGotActivate) {
        gJustGotActivate = PR_FALSE;
        DispatchActivateEvent();
    }

#ifdef USE_XIM
    nsWindow * aTmpWindow;
    aTmpWindow = this;

    if (mFocusChild)
        while (aTmpWindow->mFocusChild)
            aTmpWindow = aTmpWindow->mFocusChild;

    GtkIMContext *im = aTmpWindow->IMEGetContext();
    if (!(aTmpWindow->mIsTopLevel) && im) {
        gtk_im_context_focus_in(im);
        LOG(("OnContainFocusIn-set IC focus in. aTmpWindow:%x\n", aTmpWindow));
    }
#endif
}

void
nsWindow::OnContainerFocusOutEvent(GtkWidget *aWidget, GdkEventFocus *aEvent)
{
    // send a lost focus event for the child window
    if (mFocusChild) {
        mFocusChild->LoseFocus();
        mFocusChild->DispatchDeactivateEvent();
        mFocusChild = nsnull;
        gJustGotActivate = PR_TRUE;
    }
}

gboolean
nsWindow::OnKeyPressEvent(GtkWidget *aWidget, GdkEventKey *aEvent)
{
#ifdef USE_XIM
    GtkIMContext *im = IMEGetContext();
    if (im) {
        if (gtk_im_context_filter_keypress(im, aEvent)) {
            return TRUE;
        }
    }
#endif
    
    // work around for annoying things.
    if (aEvent->keyval == GDK_Tab)
        if (aEvent->state & GDK_CONTROL_MASK)
            if (aEvent->state & GDK_MOD1_MASK)
                return FALSE;

    // Don't pass shift, control and alt as key press events
    if (aEvent->keyval == GDK_Shift_L
        || aEvent->keyval == GDK_Shift_R
        || aEvent->keyval == GDK_Control_L
        || aEvent->keyval == GDK_Control_R
        || aEvent->keyval == GDK_Alt_L
        || aEvent->keyval == GDK_Alt_R)
        return TRUE;


    // If the key repeat flag isn't set then set it so we don't send
    // another key down event on the next key press -- DOM events are
    // key down, key press and key up.  X only has key press and key
    // release.  gtk2 already filters the extra key release events for
    // us.

    if (!mInKeyRepeat) {
        mInKeyRepeat = PR_TRUE;
        // send the key down event
        nsEventStatus status;
        nsKeyEvent event;
        InitKeyEvent(event, aEvent, NS_KEY_DOWN);
        DispatchEvent(&event, status);
    }

    nsEventStatus status;
    nsKeyEvent event;
    InitKeyEvent(event, aEvent, NS_KEY_PRESS);
    event.charCode = nsConvertCharCodeToUnicode(aEvent);
    if (event.charCode) {
        event.keyCode = 0;
        // if the control, meta, or alt key is down, then we should leave
        // the isShift flag alone (probably not a printable character)
        // if none of the other modifier keys are pressed then we need to
        // clear isShift so the character can be inserted in the editor

        if ( event.isControl || event.isAlt || event.isMeta ) {
           // make Ctrl+uppercase functional as same as Ctrl+lowercase
           // when Ctrl+uppercase(eg.Ctrl+C) is pressed,convert the charCode
           // from uppercase to lowercase(eg.Ctrl+c),so do Alt and Meta Key
           // It is hack code for bug 61355, there is same code snip for
           // Windows platform in widget/src/windows/nsWindow.cpp: See bug 16486
           // Note: if Shift is pressed at the same time, do not to_lower()
           // Because Ctrl+Shift has different function with Ctrl
           if ( !event.isShift &&
                event.charCode >= GDK_A &&
                event.charCode <= GDK_Z )
            event.charCode = gdk_keyval_to_lower(event.charCode);
        } else        
            event.isShift = PR_FALSE;
    }

    // send the key press event
    DispatchEvent(&event, status);
    return TRUE;
}

gboolean
nsWindow::OnKeyReleaseEvent(GtkWidget *aWidget, GdkEventKey *aEvent)
{
#ifdef USE_XIM
    GtkIMContext *im = IMEGetContext();
    if (!mIsTopLevel && im && !mFocusChild) {
        if (gtk_im_context_filter_keypress(im, aEvent)) {
           return TRUE;
        }
    }
#endif
    
    // unset the repeat flag
    mInKeyRepeat = PR_FALSE;

    // send the key event as a key up event
    // Don't pass shift, control and alt as key press events
    if (aEvent->keyval == GDK_Shift_L
        || aEvent->keyval == GDK_Shift_R
        || aEvent->keyval == GDK_Control_L
        || aEvent->keyval == GDK_Control_R
        || aEvent->keyval == GDK_Alt_L
        || aEvent->keyval == GDK_Alt_R)
        return TRUE;

    nsKeyEvent event;
    InitKeyEvent(event, aEvent, NS_KEY_UP);

    nsEventStatus status;
    DispatchEvent(&event, status);

    return TRUE;
}

void
nsWindow::OnScrollEvent(GtkWidget *aWidget, GdkEventScroll *aEvent)
{
    nsMouseScrollEvent event;
    InitMouseScrollEvent(event, aEvent, NS_MOUSE_SCROLL);

    // check to see if we should rollup
    if (check_for_rollup(aEvent->window, aEvent->x_root, aEvent->y_root,
                         PR_TRUE)) {
        printf("ignoring event\n");
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

    nsSizeModeEvent event;
    InitSizeModeEvent(event);

    // did we change to maximized?
    if (aEvent->changed_mask & GDK_WINDOW_STATE_MAXIMIZED &&
        aEvent->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
        LOG(("\tMaximized\n"));
        event.mSizeMode = nsSizeMode_Maximized;
        mSizeState = nsSizeMode_Maximized;
    }
    // did we change to iconified?
    else if (aEvent->changed_mask & GDK_WINDOW_STATE_ICONIFIED &&
             aEvent->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
        LOG(("\tMinimized\n"));
        event.mSizeMode = nsSizeMode_Minimized;
        mSizeState = nsSizeMode_Minimized;
    }
    // are we now normal?
    else if (aEvent->changed_mask == 0 && aEvent->new_window_state == 0) {
        LOG(("\tNormal\n"));
        event.mSizeMode = nsSizeMode_Normal;
        mSizeState = nsSizeMode_Normal;
    }
    else {
          // if we got here it means that it's not an event we want to
          // dispatch
          return;
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

    nsMouseEvent event;

    InitDragEvent(event);

    // now that we have initialized the event update our drag status
    UpdateDragStatus(event, aDragContext, dragService);

    event.message = NS_DRAGDROP_OVER;
    event.eventStructType = NS_DRAGDROP_EVENT;

    event.widget = innerMostWidget;

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
    LOG(("nsWindow::OnDragLeaveSignal(%p)\n", this));

    // make sure to unset any drag motion timers here.
    ResetDragMotionTimer(0, 0, 0, 0, 0);

    // create a fast timer - we're delaying the drag leave until the
    // next mainloop in hopes that we might be able to get a drag drop
    // signal
    mDragLeaveTimer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ASSERTION(mDragLeaveTimer, "Failed to create drag leave timer!");
    // fire this baby asafp
    mDragLeaveTimer->InitWithFuncCallback(DragLeaveTimerCallback,
                                          (void *)this,
                                          0, nsITimer::TYPE_ONE_SHOT);
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
    mDragLeaveTimer = 0;

    // set the last window to this
    mLastDragMotionWindow = innerMostWidget;

    // What we do here is dispatch a new drag motion event to
    // re-validate the drag target and then we do the drop.  The events
    // look the same except for the type.

    innerMostWidget->AddRef();

    nsMouseEvent event;

    InitDragEvent(event);

    // now that we have initialized the event update our drag status
    UpdateDragStatus(event, aDragContext, dragService);

    event.message = NS_DRAGDROP_OVER;
    event.eventStructType = NS_DRAGDROP_EVENT;
    event.widget = innerMostWidget;
    event.point.x = retx;
    event.point.y = rety;

    nsEventStatus status;
    innerMostWidget->DispatchEvent(&event, status);

    InitDragEvent(event);

    event.message = NS_DRAGDROP_DROP;
    event.eventStructType = NS_DRAGDROP_EVENT;
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

    nsMouseEvent event;

    event.message = NS_DRAGDROP_EXIT;
    event.eventStructType = NS_DRAGDROP_EVENT;

    event.widget = this;

    event.point.x = 0;
    event.point.y = 0;

    AddRef();

    nsEventStatus status;
    DispatchEvent(&event, status);

    Release();
}

void
nsWindow::OnDragEnter(nscoord aX, nscoord aY)
{
    LOG(("nsWindow::OnDragEnter(%p)\n", this));
    
    nsMouseEvent event;

    event.message = NS_DRAGDROP_ENTER;
    event.eventStructType = NS_DRAGDROP_EVENT;

    event.widget = this;

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
         aInitData->mWindowType == eWindowType_toplevel) ?
        nsnull : aParent;

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    // and do our common creation
    CommonCreate(aParent, aNativeParent);

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
    case eWindowType_toplevel: {
        mIsTopLevel = PR_TRUE;
        if (mWindowType == eWindowType_dialog) {
            mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            gtk_window_set_type_hint(GTK_WINDOW(mShell),
                                     GDK_WINDOW_TYPE_HINT_DIALOG);
            gtk_window_set_transient_for(GTK_WINDOW(mShell),
                                         topLevelParent);
            mTransientParent = topLevelParent;
            // add ourselves to the parent window's window group
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
            if(topLevelParent) {
                gtk_window_set_transient_for(GTK_WINDOW(mShell), 
                                            topLevelParent);
                mTransientParent = topLevelParent;
                if(topLevelParent->group) {
                    gtk_window_group_add_window(topLevelParent->group,
                                            GTK_WINDOW(mShell));
                    mWindowGroup = topLevelParent->group;
                }
            }
        }
        else { // must be eWindowType_toplevel
            mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
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
    }
        break;
    case eWindowType_child: {
        if (parentMozContainer) {
            mDrawingarea = moz_drawingarea_new(parentArea, parentMozContainer);
        }
        else {
            mContainer = MOZ_CONTAINER(moz_container_new());
            gtk_container_add(parentGtkContainer, GTK_WIDGET(mContainer));
            gtk_widget_realize(GTK_WIDGET(mContainer));

            mDrawingarea = moz_drawingarea_new(nsnull, mContainer);
        }
#ifdef USE_XIM
        // get mIMEShellWindow and keep it
        IMEGetShellWindow();
#endif
    }
        break;
    default:
        break;
    }
    // Disable the double buffer because it will make the caret crazy
    // For bug#153805 (Gtk2 double buffer makes carets misbehave) 
    if( mContainer )
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

#ifdef USE_XIM
    if (mShell) {
        // init GtkIMContext for shell
        IMECreateContext(mShell->window);
    }
#endif

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

    return NS_OK;
}

void
nsWindow::NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool  aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));

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
    else {
        moz_drawingarea_move_resize(mDrawingarea, aX, aY, aWidth, aHeight);
    }
}

void
nsWindow::NativeShow (PRBool  aAction)
{
    if (aAction) {
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
        else {
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
        moz_drawingarea_set_visibility(mDrawingarea, FALSE);
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
    else
        grabWindow = mDrawingarea->inner_window;

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

    GtkWidget *widget =
        get_gtk_widget_for_gdk_window(mDrawingarea->inner_window);
    if (!widget)
        return;

    *aWidget = gtk_widget_get_toplevel(widget);
}

void *
nsWindow::SetupPluginPort(void)
{
    if (!mDrawingarea)
        return nsnull;

    if (GDK_WINDOW_OBJECT(mDrawingarea->inner_window)->destroyed == TRUE)
        return nsnull;

    // we have to flush the X queue here so that any plugins that
    // might be running on seperate X connections will be able to use
    // this window in case it was just created
    XWindowAttributes xattrs;
    XGetWindowAttributes(GDK_DISPLAY (),
                         GDK_WINDOW_XWINDOW(mDrawingarea->inner_window),
                         &xattrs);
    XSelectInput (GDK_DISPLAY (),
                  GDK_WINDOW_XWINDOW(mDrawingarea->inner_window),
                  xattrs.your_event_mask |
                  SubstructureNotifyMask );

    gdk_window_add_filter(mDrawingarea->inner_window, 
                          plugin_window_filter_func,
                          this);

    XSync(GDK_DISPLAY(), False);

    return (void *)GDK_WINDOW_XWINDOW(mDrawingarea->inner_window);
}

void
nsWindow::SetPluginType(PRBool aIsXembed)
{
    mHasNonXembedPlugin = !aIsXembed;
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
is_mouse_in_window (GdkWindow* aWindow, gdouble aMouseX, gdouble aMouseY)
{
    gint x = 0;
    gint y = y;
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

    if ( aMouseX > x && aMouseX < x + w &&
         aMouseY > y && aMouseY < y + h )
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
            if(xevent->type==CreateNotify) {
                plugin_window = gdk_window_lookup(xevent->xcreatewindow.window);
            }
            else {
                if(xevent->xreparent.event != xevent->xreparent.parent)
                    break;
                plugin_window = gdk_window_lookup (xevent->xreparent.window);
            }
            if(plugin_window) {
                user_data = nsnull;
                gdk_window_get_user_data(plugin_window, &user_data);
                widget = GTK_WIDGET(user_data);
                if(GTK_IS_SOCKET(widget)){
                    break;
                }
            }
            nswindow->SetPluginType(PR_FALSE);
            return_val = GDK_FILTER_REMOVE;
            break;
        default:
            break;
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

    nsWindow *focusWindow = window->mFocusChild;
    if (!focusWindow)
        focusWindow = window;

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

    nsWindow *focusWindow = window->mFocusChild;
    if (!focusWindow)
        focusWindow = window;

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
    // set everything to zero
    memset(&aEvent, 0, sizeof(nsMouseEvent));
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

/* static */
nsresult
initialize_default_icon(void)
{
    // Set up the default icon for all windows
    nsresult rv;
    nsCOMPtr<nsIFile> chromeDir;
    rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR,
                                getter_AddRefs(chromeDir));
    if (NS_FAILED(rv))
        return rv;

    nsAutoString defaultPath;
    chromeDir->GetPath(defaultPath);
            
    defaultPath.Append(NS_LITERAL_STRING("/icons/default/default.xpm"));

    nsCOMPtr<nsILocalFile> defaultPathConverter;
    rv = NS_NewLocalFile(defaultPath, PR_TRUE,
                         getter_AddRefs(defaultPathConverter));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString aPath;
    defaultPathConverter->GetNativePath(aPath);

    LOG(("Loading default icon from %s\n", aPath.get()));

    GdkPixbuf *defaultIcon = gdk_pixbuf_new_from_file(aPath.get(), NULL);
    if (!defaultIcon)
        return NS_ERROR_FAILURE;

    GList *list = NULL;
    list = g_list_append(list, defaultIcon);
    gtk_window_set_default_icon_list(list);
    g_object_unref(G_OBJECT(defaultIcon));
    g_list_free(list);

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
            if (( cx < x ) && (x < (cx + cw)) &&
                ( cy < y ) && (y < (cy + ch)) &&
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

#ifdef ACCESSIBILITY
/**
 * void
 * nsWindow::CreateTopLevelAccessible
 *
 * request to create the nsIAccessible Object for the toplevel window
 **/
void
nsWindow::CreateTopLevelAccessible()
{
    if (mIsTopLevel && !mTopLevelAccessible &&
        nsAccessibilityInterface::IsInitialized()) {

        nsCOMPtr<nsIAccessible> acc;
        DispatchAccessibleEvent(getter_AddRefs(acc));

        if (acc) {
            mTopLevelAccessible = acc;
            nsAccessibilityInterface::AddTopLevel(acc);
        }
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
    nsAccessibleEvent event;

    *aAccessible = nsnull;

    InitAccessibleEvent(event);
    nsEventStatus status;
    DispatchEvent(&event, status);
    result = (nsEventStatus_eConsumeNoDefault == status) ? PR_TRUE : PR_FALSE;

    // if the event returned an accesssible get it.
    if (event.accessible)
        *aAccessible = event.accessible;

    return result;
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
nsWindow::IMEComposeStart(void)
{
    nsCompositionEvent compEvent;

    compEvent.widget = NS_STATIC_CAST(nsIWidget *, this);
    compEvent.point.x = compEvent.point.y = 0;
    compEvent.time = 0;    // Potential problem ?
    compEvent.message = compEvent.eventStructType
        = compEvent.compositionMessage = NS_COMPOSITION_START;

    nsEventStatus status;
    DispatchEvent(&compEvent, status);
}

void
nsWindow::IMEComposeText (const PRUnichar *aText,
                          const PRInt32 aLen,
                          const gchar *aPreeditString,
                          const PangoAttrList *aFeedback)
{
    nsTextEvent textEvent;

    textEvent.time = 0;
    textEvent.isShift = textEvent.isControl =
    textEvent.isAlt = textEvent.isMeta = PR_FALSE;
  
    textEvent.message = textEvent.eventStructType = NS_TEXT_EVENT;
    textEvent.widget = NS_STATIC_CAST(nsIWidget *, this);
    textEvent.point.x = textEvent.point.y = 0;

    if (aLen == 0) {
        textEvent.theText = nsnull;
        textEvent.rangeCount = 0;
        textEvent.rangeArray = nsnull;
    } else {
        textEvent.theText = (PRUnichar*)aText;
        textEvent.rangeCount = 0;
        textEvent.rangeArray = nsnull;

        if (aPreeditString && aFeedback && (aLen > 0)) {
            IMSetTextRange(aLen, aPreeditString, aFeedback,
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
    nsCompositionEvent compEvent;
    compEvent.widget = NS_STATIC_CAST(nsIWidget *, this);
    compEvent.point.x = compEvent.point.y = 0;
    compEvent.time = 0;
    compEvent.message = compEvent.eventStructType
        = compEvent.compositionMessage = NS_COMPOSITION_END;

    nsEventStatus status;
    DispatchEvent(&compEvent, status);
}

/* static */
void
IM_preedit_changed_cb(GtkIMContext *context,
                      nsWindow     *awindow)
{
    gchar *preedit_string;
    gint cursor_pos;
    PangoAttrList *feedback_list;

    // call for focused window
    nsWindow *window = awindow->gFocusedWindow;
    if (!window) return;
  
    // Should use cursor_pos ?
    gtk_im_context_get_preedit_string(context, &preedit_string,
                                      &feedback_list, &cursor_pos);
  
    LOG(("preedit string is: %s   length is: %d\n",
         preedit_string, strlen(preedit_string)));

    if ((preedit_string == NULL) || (0 == strlen(preedit_string))) {
        window->IMEComposeStart();
        window->IMEComposeText(NULL, 0, NULL, NULL);
        return;
    }

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

    if (window && (uniStrLen > 0)) {
        window->IMEComposeStart();
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
IM_commit_cb (GtkIMContext *context,
              const gchar  *utf8_str,
              nsWindow   *awindow)
{
    gunichar2 * uniStr;
    glong uniStrLen;

    // call for focused window
    nsWindow *window = awindow->gFocusedWindow;
    if (!window) return;

    uniStr = NULL;
    uniStrLen = 0;
    uniStr = g_utf8_to_utf16(utf8_str, -1, NULL, &uniStrLen, NULL);

    // Will free it in call function, don't need
    // g_free((void *)utf8_str));

    if (!uniStr) {
        LOG(("utf80utf16 string tranfer failed!\n"));
        return;
    }

    if (window && (uniStrLen > 0)) {
        window->IMEComposeStart();
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
IMSetTextRange (const PRInt32 aLen,
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

    int count;
    PangoAttribute * aPangoAttr;
    count = 0;
    /*
     * Depend on gtk2's implementation on XIM support.
     * In aFeedback got from gtk2, there are only three types of data:
     * PANGO_ATTR_UNDERLINE, PANGO_ATTR_FOREGROUND, PANGO_ATTR_BACKGROUND.
     * Corresponding to XIMUnderline, XIMReverse.
     * Don't take PANGO_ATTR_BACKGROUND into account, since
     * PANGO_ATTR_BACKGROUND and PANGO_ATTR_FOREGROUND are always
     * a couple.
     */
    gunichar2 * uniStr;
    glong uniStrLen;
    do {
        aPangoAttr = pango_attr_iterator_get(aFeedbackIterator,
                                             PANGO_ATTR_UNDERLINE);
        if (aPangoAttr) {
            // Stands for XIMUnderline
            count++;
            START_OFFSET(count) = 0;
            END_OFFSET(count) = 0;
            
            uniStr = NULL;
            if (aPangoAttr->start_index > 0)
                uniStr = g_utf8_to_utf16(aPreeditString,
                                         aPangoAttr->start_index,
                                         NULL, &uniStrLen, NULL);
            if (uniStr)
                START_OFFSET(count) = uniStrLen;
            
            uniStr = NULL;
            uniStr = g_utf8_to_utf16(aPreeditString + aPangoAttr->start_index,
                         aPangoAttr->end_index - aPangoAttr->start_index,
                         NULL, &uniStrLen, NULL);
            if (uniStr) {
                END_OFFSET(count) = START_OFFSET(count) + uniStrLen;
                SET_FEEDBACKTYPE(count, NS_TEXTRANGE_CONVERTEDTEXT);
            }
        } else {
            aPangoAttr = pango_attr_iterator_get(aFeedbackIterator,
                                                 PANGO_ATTR_FOREGROUND);
            if (aPangoAttr) {
                count++;
                START_OFFSET(count) = 0;
                END_OFFSET(count) = 0;
                
                uniStr = NULL;
                if (aPangoAttr->start_index > 0)
                    uniStr = g_utf8_to_utf16(aPreeditString,
                                             aPangoAttr->start_index,
                                             NULL, &uniStrLen, NULL);
                if (uniStr)
                    START_OFFSET(count) = uniStrLen;
            
                uniStr = NULL;
                uniStr = g_utf8_to_utf16(
                             aPreeditString + aPangoAttr->start_index,
                             aPangoAttr->end_index - aPangoAttr->start_index,
                             NULL, &uniStrLen, NULL);

                if (uniStr) {
                    END_OFFSET(count) = START_OFFSET(count) + uniStrLen;
                    SET_FEEDBACKTYPE(count, NS_TEXTRANGE_SELECTEDRAWTEXT);
                }
            }
        }
    } while ((count < aMaxLenOfTextRange - 1) &&
             (pango_attr_iterator_next(aFeedbackIterator)));

   *aTextRangeListLengthResult = count + 1;

   pango_attr_iterator_destroy(aFeedbackIterator);
}

#endif
