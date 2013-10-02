/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/Util.h"
#include <algorithm>

#include "prlink.h"
#include "nsGTKToolkit.h"
#include "nsIRollupListener.h"
#include "nsIDOMNode.h"

#include "nsWidgetsCID.h"
#include "nsDragService.h"
#include "nsIWidgetListener.h"

#include "nsGtkKeyUtils.h"
#include "nsGtkCursors.h"

#include <gtk/gtk.h>
#if (MOZ_WIDGET_GTK == 3)
#include <gtk/gtkx.h>
#endif
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/shape.h>
#if (MOZ_WIDGET_GTK == 3)
#include <gdk/gdkkeysyms-compat.h>
#endif

#ifdef AIX
#include <X11/keysym.h>
#else
#include <X11/XF86keysym.h>
#endif

#if (MOZ_WIDGET_GTK == 2)
#include "gtk2xtbin.h"
#endif
#endif /* MOZ_X11 */
#include <gdk/gdkkeysyms.h>
#if (MOZ_WIDGET_GTK == 2)
#include <gtk/gtkprivate.h>
#endif

#include "nsGkAtoms.h"

#ifdef MOZ_ENABLE_STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <startup-notification-1.0/libsn/sn.h>
#endif

#include "mozilla/Likely.h"
#include "mozilla/Preferences.h"
#include "nsIPrefService.h"
#include "nsIGConfService.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsGfxCIID.h"
#include "nsGtkUtils.h"
#include "nsIObserverService.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsIIdleServiceInternal.h"
#include "nsIPropertyBag2.h"
#include "GLContext.h"

#ifdef ACCESSIBILITY
#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/Platform.h"
#include "nsAccessibilityService.h"

using namespace mozilla;
using namespace mozilla::widget;
#endif

/* For SetIcon */
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"

/* SetCursor(imgIContainer*) */
#include <gdk/gdk.h>
#include <wchar.h>
#include "imgIContainer.h"
#include "nsGfxCIID.h"
#include "nsImageToPixbuf.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"
#include "ClientLayerManager.h"

extern "C" {
#define PIXMAN_DONT_DEFINE_STDINT
#include "pixman.h"
}
#include "gfxPlatformGtk.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "Layers.h"
#include "LayerManagerOGL.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/CompositorParent.h"

#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#include "cairo-xlib.h"
#endif
  
#include "nsShmImage.h"

#include "nsIDOMWheelEvent.h"

#include "nsWindow.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;
using namespace mozilla::layers;
using mozilla::gl::GLContext;
using mozilla::layers::LayerManagerOGL;

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION 100

const gint kEvents = GDK_EXPOSURE_MASK | GDK_STRUCTURE_MASK |
                     GDK_VISIBILITY_NOTIFY_MASK |
                     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                     GDK_SCROLL_MASK |
                     GDK_POINTER_MOTION_MASK;

/* utility functions */
static bool       is_mouse_in_window(GdkWindow* aWindow,
                                     gdouble aMouseX, gdouble aMouseY);
static nsWindow  *get_window_for_gtk_widget(GtkWidget *widget);
static nsWindow  *get_window_for_gdk_window(GdkWindow *window);
static GtkWidget *get_gtk_widget_for_gdk_window(GdkWindow *window);
static GdkCursor *get_gtk_cursor(nsCursor aCursor);

static GdkWindow *get_inner_gdk_window (GdkWindow *aWindow,
                                        gint x, gint y,
                                        gint *retx, gint *rety);

static inline bool is_context_menu_key(const WidgetKeyboardEvent& inKeyEvent);

static int    is_parent_ungrab_enter(GdkEventCrossing *aEvent);
static int    is_parent_grab_leave(GdkEventCrossing *aEvent);

static void GetBrandName(nsXPIDLString& brandName);

/* callbacks from widgets */
#if (MOZ_WIDGET_GTK == 2)
static gboolean expose_event_cb           (GtkWidget *widget,
                                           GdkEventExpose *event);
#else
static gboolean expose_event_cb           (GtkWidget *widget,
                                           cairo_t *rect);
#endif
static gboolean configure_event_cb        (GtkWidget *widget,
                                           GdkEventConfigure *event);
static void     container_unrealize_cb    (GtkWidget *widget);
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
static void     hierarchy_changed_cb      (GtkWidget *widget,
                                           GtkWidget *previous_toplevel);
static gboolean window_state_event_cb     (GtkWidget *widget,
                                           GdkEventWindowState *event);
static void     theme_changed_cb          (GtkSettings *settings,
                                           GParamSpec *pspec,
                                           nsWindow *data);
static nsWindow* GetFirstNSWindowForGDKWindow (GdkWindow *aGdkWindow);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#ifdef MOZ_X11
static GdkFilterReturn popup_take_focus_filter (GdkXEvent *gdk_xevent,
                                                GdkEvent *event,
                                                gpointer data);
static GdkFilterReturn plugin_window_filter_func (GdkXEvent *gdk_xevent,
                                                  GdkEvent *event,
                                                  gpointer data);
static GdkFilterReturn plugin_client_message_filter (GdkXEvent *xevent,
                                                     GdkEvent *event,
                                                     gpointer data);
#endif /* MOZ_X11 */
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
                                           gpointer aData);
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

static guint32 sLastUserInputTime = GDK_CURRENT_TIME;
static guint32 sRetryGrabTime;

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

// The window from which the focus manager asks us to dispatch key events.
static nsWindow         *gFocusWindow          = NULL;
static bool              gBlockActivateEvent   = false;
static bool              gGlobalsInitialized   = false;
static bool              gRaiseWindows         = true;
static nsWindow         *gPluginFocusWindow    = NULL;


#define NS_WINDOW_TITLE_MAX_LENGTH 4095

// If after selecting profile window, the startup fail, please refer to
// http://bugzilla.gnome.org/show_bug.cgi?id=88940

// needed for imgIContainer cursors
// GdkDisplay* was added in 2.2
typedef struct _GdkDisplay GdkDisplay;

#define kWindowPositionSlop 20

// cursor cache
static GdkCursor *gCursorCache[eCursorCount];

static GtkWidget *gInvisibleContainer = NULL;

// Sometimes this actually also includes the state of the modifier keys, but
// only the button state bits are used.
static guint gButtonState;

// nsAutoRef<pixman_region32> uses nsSimpleRef<> to know how to automatically
// destroy regions.
template <>
class nsSimpleRef<pixman_region32> : public pixman_region32 {
protected:
    typedef pixman_region32 RawRef;

    nsSimpleRef() { data = nullptr; }
    nsSimpleRef(const RawRef &aRawRef) : pixman_region32(aRawRef) { }

    static void Release(pixman_region32& region) {
        pixman_region32_fini(&region);
    }
    // Whether this needs to be released:
    bool HaveResource() const { return data != nullptr; }

    pixman_region32& get() { return *this; }
};

static inline int32_t
GetBitmapStride(int32_t width)
{
#if defined(MOZ_X11) || (MOZ_WIDGET_GTK == 2)
  return (width+7)/8;
#else
  return cairo_format_stride_for_width(CAIRO_FORMAT_A1, width);
#endif
}

static inline bool TimestampIsNewerThan(guint32 a, guint32 b)
{
    // Timestamps are just the least significant bits of a monotonically
    // increasing function, and so the use of unsigned overflow arithmetic.
    return a - b <= G_MAXUINT32/2;
}

static void
UpdateLastInputEventTime(void *aGdkEvent)
{
    nsCOMPtr<nsIIdleServiceInternal> idleService =
        do_GetService("@mozilla.org/widget/idleservice;1");
    if (idleService) {
        idleService->ResetIdleTimeOut(0);
    }

    guint timestamp = gdk_event_get_time(static_cast<GdkEvent*>(aGdkEvent));
    if (timestamp == GDK_CURRENT_TIME)
        return;

    sLastUserInputTime = timestamp;
}

nsWindow::nsWindow()
{
    mIsTopLevel       = false;
    mIsDestroyed      = false;
    mNeedsResize      = false;
    mNeedsMove        = false;
    mListenForResizes = false;
    mIsShown          = false;
    mNeedsShow        = false;
    mEnabled          = true;
    mCreated          = false;

    mContainer           = nullptr;
    mGdkWindow           = nullptr;
    mShell               = nullptr;
    mHasMappedToplevel   = false;
    mIsFullyObscured     = false;
    mRetryPointerGrab    = false;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mLastSizeMode        = nsSizeMode_Normal;
    mSizeConstraints.mMaxSize = GetSafeWindowSize(mSizeConstraints.mMaxSize);

#ifdef MOZ_X11
    mOldFocusWindow      = 0;
#endif /* MOZ_X11 */
    mPluginType          = PluginType_NONE;

    if (!gGlobalsInitialized) {
        gGlobalsInitialized = true;

        // It's OK if either of these fail, but it may not be one day.
        initialize_prefs();
    }

    mLastMotionPressure = 0;

#ifdef ACCESSIBILITY
    mRootAccessible  = nullptr;
#endif

    mIsTransparent = false;
    mTransparencyBitmap = nullptr;

    mTransparencyBitmapWidth  = 0;
    mTransparencyBitmapHeight = 0;
}

nsWindow::~nsWindow()
{
    LOG(("nsWindow::~nsWindow() [%p]\n", (void *)this));

    delete[] mTransparencyBitmap;
    mTransparencyBitmap = nullptr;

    Destroy();
}

/* static */ void
nsWindow::ReleaseGlobals()
{
  for (uint32_t i = 0; i < ArrayLength(gCursorCache); ++i) {
    if (gCursorCache[i]) {
      gdk_cursor_unref(gCursorCache[i]);
      gCursorCache[i] = nullptr;
    }
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsBaseWidget,
                             nsISupportsWeakReference)

void
nsWindow::CommonCreate(nsIWidget *aParent, bool aListenForResizes)
{
    mParent = aParent;
    mListenForResizes = aListenForResizes;
    mCreated = true;
}

void
nsWindow::DispatchActivateEvent(void)
{
    NS_ASSERTION(mContainer || mIsDestroyed,
                 "DispatchActivateEvent only intended for container windows");

#ifdef ACCESSIBILITY
    DispatchActivateEventAccessible();
#endif //ACCESSIBILITY

    if (mWidgetListener)
      mWidgetListener->WindowActivated();
}

void
nsWindow::DispatchDeactivateEvent(void)
{
    if (mWidgetListener)
      mWidgetListener->WindowDeactivated();

#ifdef ACCESSIBILITY
    DispatchDeactivateEventAccessible();
#endif //ACCESSIBILITY
}

void
nsWindow::DispatchResized(int32_t aWidth, int32_t aHeight)
{
    nsIWidgetListener *listeners[] =
        { mWidgetListener, mAttachedWidgetListener };
    for (size_t i = 0; i < ArrayLength(listeners); ++i) {
        if (listeners[i]) {
            listeners[i]->WindowResized(this, aWidth, aHeight);
        }
    }
}

nsresult
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
#ifdef DEBUG
    debug_DumpEvent(stdout, aEvent->widget, aEvent,
                    nsAutoCString("something"), 0);
#endif

    aStatus = nsEventStatus_eIgnore;
    nsIWidgetListener* listener =
        mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
    if (listener) {
      aStatus = listener->HandleEvent(aEvent, mUseAttachedEvents);
    }

    return NS_OK;
}

void
nsWindow::OnDestroy(void)
{
    if (mOnDestroyCalled)
        return;

    mOnDestroyCalled = true;
    
    // Prevent deletion.
    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

    // release references to children, device context, toolkit + app shell
    nsBaseWidget::OnDestroy(); 
    
    // Remove association between this object and its parent and siblings.
    nsBaseWidget::Destroy();
    mParent = nullptr;

    NotifyWindowDestroyed();
}

bool
nsWindow::AreBoundsSane(void)
{
    if (mBounds.width > 0 && mBounds.height > 0)
        return true;

    return false;
}

static GtkWidget*
EnsureInvisibleContainer()
{
    if (!gInvisibleContainer) {
        // GtkWidgets need to be anchored to a GtkWindow to be realized (to
        // have a window).  Using GTK_WINDOW_POPUP rather than
        // GTK_WINDOW_TOPLEVEL in the hope that POPUP results in less
        // initialization and window manager interaction.
        GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
        gInvisibleContainer = moz_container_new();
        gtk_container_add(GTK_CONTAINER(window), gInvisibleContainer);
        gtk_widget_realize(gInvisibleContainer);

    }
    return gInvisibleContainer;
}

static void
CheckDestroyInvisibleContainer()
{
    NS_PRECONDITION(gInvisibleContainer, "oh, no");

    if (!gdk_window_peek_children(gtk_widget_get_window(gInvisibleContainer))) {
        // No children, so not in use.
        // Make sure to destroy the GtkWindow also.
        gtk_widget_destroy(gtk_widget_get_parent(gInvisibleContainer));
        gInvisibleContainer = NULL;
    }
}

// Change the containing GtkWidget on a sub-hierarchy of GdkWindows belonging
// to aOldWidget and rooted at aWindow, and reparent any child GtkWidgets of
// the GdkWindow hierarchy to aNewWidget.
static void
SetWidgetForHierarchy(GdkWindow *aWindow,
                      GtkWidget *aOldWidget,
                      GtkWidget *aNewWidget)
{
    gpointer data;
    gdk_window_get_user_data(aWindow, &data);

    if (data != aOldWidget) {
        if (!GTK_IS_WIDGET(data))
            return;

        GtkWidget* widget = static_cast<GtkWidget*>(data);
        if (gtk_widget_get_parent(widget) != aOldWidget)
            return;

        // This window belongs to a child widget, which will no longer be a
        // child of aOldWidget.
        gtk_widget_reparent(widget, aNewWidget);

        return;
    }

    GList *children = gdk_window_get_children(aWindow);
    for(GList *list = children; list; list = list->next) {
        SetWidgetForHierarchy(GDK_WINDOW(list->data), aOldWidget, aNewWidget);
    }
    g_list_free(children);

    gdk_window_set_user_data(aWindow, aNewWidget);
}

// Walk the list of child windows and call destroy on them.
void
nsWindow::DestroyChildWindows()
{
    if (!mGdkWindow)
        return;

    while (GList *children = gdk_window_peek_children(mGdkWindow)) {
        GdkWindow *child = GDK_WINDOW(children->data);
        nsWindow *kid = get_window_for_gdk_window(child);
        if (kid) {
            kid->Destroy();
        } else {
            // This child is not an nsWindow.
            // Destroy the child GtkWidget.
            gpointer data;
            gdk_window_get_user_data(child, &data);
            if (GTK_IS_WIDGET(data)) {
                gtk_widget_destroy(static_cast<GtkWidget*>(data));
            }
        }
    }
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    if (mIsDestroyed || !mCreated)
        return NS_OK;

    LOG(("nsWindow::Destroy [%p]\n", (void *)this));
    mIsDestroyed = true;
    mCreated = false;

    /** Need to clean our LayerManager up while still alive */
    if (mLayerManager) {
        nsRefPtr<GLContext> gl = nullptr;
        if (mLayerManager->GetBackendType() == mozilla::layers::LAYERS_OPENGL) {
            LayerManagerOGL *ogllm = static_cast<LayerManagerOGL*>(mLayerManager.get());
            gl = ogllm->gl();
        }

        mLayerManager->Destroy();

        if (gl) {
            gl->MarkDestroyed();
        }
    }
    mLayerManager = nullptr;

    // It is safe to call DestroyeCompositor several times (here and 
    // in the parent class) since it will take effect only once.
    // The reason we call it here is because on gtk platforms we need 
    // to destroy the compositor before we destroy the gdk window (which
    // destroys the the gl context attached to it).
    DestroyCompositor();

    ClearCachedResources();

    g_signal_handlers_disconnect_by_func(gtk_settings_get_default(),
                                         FuncToGpointer(theme_changed_cb),
                                         this);

    nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
    if (rollupListener) {
        nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
        if (static_cast<nsIWidget *>(this) == rollupWidget) {
            rollupListener->Rollup(0, nullptr);
        }
    }

    // dragService will be null after shutdown of the service manager.
    nsDragService *dragService = nsDragService::GetInstance();
    if (dragService && this == dragService->GetMostRecentDestWindow()) {
        dragService->ScheduleLeaveEvent();
    }

    NativeShow(false);

    if (mIMModule) {
        mIMModule->OnDestroyWindow(this);
    }

    // make sure that we remove ourself as the focus window
    if (gFocusWindow == this) {
        LOGFOCUS(("automatically losing focus...\n"));
        gFocusWindow = nullptr;
    }

#if (MOZ_WIDGET_GTK == 2) && defined(MOZ_X11)
    // make sure that we remove ourself as the plugin focus window
    if (gPluginFocusWindow == this) {
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }
#endif /* MOZ_X11 && MOZ_WIDGET_GTK2 */
  
    // Destroy thebes surface now. Badness can happen if we destroy
    // the surface after its X Window.
    mThebesSurface = nullptr;

    GtkWidget *owningWidget = GetMozContainerWidget();
    if (mShell) {
        gtk_widget_destroy(mShell);
        mShell = nullptr;
        mContainer = nullptr;
        NS_ABORT_IF_FALSE(!mGdkWindow,
                          "mGdkWindow should be NULL when mContainer is destroyed");
    }
    else if (mContainer) {
        gtk_widget_destroy(GTK_WIDGET(mContainer));
        mContainer = nullptr;
        NS_ABORT_IF_FALSE(!mGdkWindow,
                          "mGdkWindow should be NULL when mContainer is destroyed");
    }
    else if (mGdkWindow) {
        // Destroy child windows to ensure that their mThebesSurfaces are
        // released and to remove references from GdkWindows back to their
        // container widget.  (OnContainerUnrealize() does this when the
        // MozContainer widget is destroyed.)
        DestroyChildWindows();

        gdk_window_set_user_data(mGdkWindow, NULL);
        g_object_set_data(G_OBJECT(mGdkWindow), "nsWindow", NULL);
        gdk_window_destroy(mGdkWindow);
        mGdkWindow = nullptr;
    }

    if (gInvisibleContainer && owningWidget == gInvisibleContainer) {
        CheckDestroyInvisibleContainer();
    }

#ifdef ACCESSIBILITY
     if (mRootAccessible) {
         mRootAccessible = nullptr;
     }
#endif

    // Save until last because OnDestroy() may cause us to be deleted.
    OnDestroy();

    return NS_OK;
}

nsIWidget *
nsWindow::GetParent(void)
{
    return mParent;
}

float
nsWindow::GetDPI()
{
    Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    int defaultScreen = DefaultScreen(dpy);
    double heightInches = DisplayHeightMM(dpy, defaultScreen)/MM_PER_INCH_FLOAT;
    if (heightInches < 0.25) {
        // Something's broken, but we'd better not crash.
        return 96.0f;
    }
    return float(DisplayHeight(dpy, defaultScreen)/heightInches);
}

NS_IMETHODIMP
nsWindow::SetParent(nsIWidget *aNewParent)
{
    if (mContainer || !mGdkWindow) {
        NS_NOTREACHED("nsWindow::SetParent called illegally");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;
    if (mParent) {
        mParent->RemoveChild(this);
    }

    mParent = aNewParent;

    GtkWidget* oldContainer = GetMozContainerWidget();
    if (!oldContainer) {
        // The GdkWindows have been destroyed so there is nothing else to
        // reparent.
        NS_ABORT_IF_FALSE(gdk_window_is_destroyed(mGdkWindow),
                          "live GdkWindow with no widget");
        return NS_OK;
    }

    if (aNewParent) {
        aNewParent->AddChild(this);
        ReparentNativeWidget(aNewParent);
    } else {
        // aNewParent is NULL, but reparent to a hidden window to avoid
        // destroying the GdkWindow and its descendants.
        // An invisible container widget is needed to hold descendant
        // GtkWidgets.
        GtkWidget* newContainer = EnsureInvisibleContainer();
        GdkWindow* newParentWindow = gtk_widget_get_window(newContainer);
        ReparentNativeWidgetInternal(aNewParent, newContainer, newParentWindow,
                                     oldContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
    NS_PRECONDITION(aNewParent, "");
    NS_ASSERTION(!mIsDestroyed, "");
    NS_ASSERTION(!static_cast<nsWindow*>(aNewParent)->mIsDestroyed, "");

    GtkWidget* oldContainer = GetMozContainerWidget();
    if (!oldContainer) {
        // The GdkWindows have been destroyed so there is nothing else to
        // reparent.
        NS_ABORT_IF_FALSE(gdk_window_is_destroyed(mGdkWindow),
                          "live GdkWindow with no widget");
        return NS_OK;
    }
    NS_ABORT_IF_FALSE(!gdk_window_is_destroyed(mGdkWindow),
                      "destroyed GdkWindow with widget");
    
    nsWindow* newParent = static_cast<nsWindow*>(aNewParent);
    GdkWindow* newParentWindow = newParent->mGdkWindow;
    GtkWidget* newContainer = newParent->GetMozContainerWidget();
    GtkWindow* shell = GTK_WINDOW(mShell);

    if (shell && gtk_window_get_transient_for(shell)) {
      GtkWindow* topLevelParent =
          GTK_WINDOW(gtk_widget_get_toplevel(newContainer));
      gtk_window_set_transient_for(shell, topLevelParent);
    }

    ReparentNativeWidgetInternal(aNewParent, newContainer, newParentWindow,
                                 oldContainer);
    return NS_OK;
}

void
nsWindow::ReparentNativeWidgetInternal(nsIWidget* aNewParent,
                                       GtkWidget* aNewContainer,
                                       GdkWindow* aNewParentWindow,
                                       GtkWidget* aOldContainer)
{
    if (!aNewContainer) {
        // The new parent GdkWindow has been destroyed.
        NS_ABORT_IF_FALSE(!aNewParentWindow ||
                          gdk_window_is_destroyed(aNewParentWindow),
                          "live GdkWindow with no widget");
        Destroy();
    } else {
        if (aNewContainer != aOldContainer) {
            NS_ABORT_IF_FALSE(!gdk_window_is_destroyed(aNewParentWindow),
                              "destroyed GdkWindow with widget");
            SetWidgetForHierarchy(mGdkWindow, aOldContainer, aNewContainer);

            if (aOldContainer == gInvisibleContainer) {
                CheckDestroyInvisibleContainer();
            }
        }

        if (!mIsTopLevel) {
            gdk_window_reparent(mGdkWindow, aNewParentWindow, mBounds.x,
                                mBounds.y);
        }
    }

    nsWindow* newParent = static_cast<nsWindow*>(aNewParent);
    bool parentHasMappedToplevel =
        newParent && newParent->mHasMappedToplevel;
    if (mHasMappedToplevel != parentHasMappedToplevel) {
        SetHasMappedToplevel(parentHasMappedToplevel);
    }
}

NS_IMETHODIMP
nsWindow::SetModal(bool aModal)
{
    LOG(("nsWindow::SetModal [%p] %d\n", (void *)this, aModal));
    if (mIsDestroyed)
        return aModal ? NS_ERROR_NOT_AVAILABLE : NS_OK;
    if (!mIsTopLevel || !mShell)
        return NS_ERROR_FAILURE;
    gtk_window_set_modal(GTK_WINDOW(mShell), aModal ? TRUE : FALSE);
    return NS_OK;
}

// nsIWidget method, which means IsShown.
bool
nsWindow::IsVisible() const
{
    return mIsShown;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY)
{
    if (mIsTopLevel && mShell) {
        int32_t screenWidth = gdk_screen_width();
        int32_t screenHeight = gdk_screen_height();
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

void nsWindow::SetSizeConstraints(const SizeConstraints& aConstraints)
{
    mSizeConstraints.mMinSize = GetSafeWindowSize(aConstraints.mMinSize);
    mSizeConstraints.mMaxSize = GetSafeWindowSize(aConstraints.mMaxSize);

    if (mShell) {
        GdkGeometry geometry;
        geometry.min_width = mSizeConstraints.mMinSize.width;
        geometry.min_height = mSizeConstraints.mMinSize.height;
        geometry.max_width = mSizeConstraints.mMaxSize.width;
        geometry.max_height = mSizeConstraints.mMaxSize.height;

        uint32_t hints = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
        gtk_window_set_geometry_hints(GTK_WINDOW(mShell), nullptr,
                                      &geometry, GdkWindowHints(hints));
    }
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    if (aState == mIsShown)
        return NS_OK;

    // Clear our cached resources when the window is hidden.
    if (mIsShown && !aState) {
        ClearCachedResources();
    }

    mIsShown = aState;

    LOG(("nsWindow::Show [%p] state %d\n", (void *)this, aState));

    if (aState) {
        // Now that this window is shown, mHasMappedToplevel needs to be
        // tracked on viewable descendants.
        SetHasMappedToplevel(mHasMappedToplevel);
    }

    // Ok, someone called show on a window that isn't sized to a sane
    // value.  Mark this window as needing to have Show() called on it
    // and return.
    if ((aState && !AreBoundsSane()) || !mCreated) {
        LOG(("\tbounds are insane or window hasn't been created yet\n"));
        mNeedsShow = true;
        return NS_OK;
    }

    // If someone is hiding this widget, clear any needing show flag.
    if (!aState)
        mNeedsShow = false;

    // If someone is showing this window and it needs a resize then
    // resize the widget.
    if (aState) {
        if (mNeedsMove) {
            NativeResize(mBounds.x, mBounds.y, mBounds.width, mBounds.height,
                         false);
        } else if (mNeedsResize) {
            NativeResize(mBounds.width, mBounds.height, false);
        }
    }

#ifdef ACCESSIBILITY
    if (aState && a11y::ShouldA11yBeEnabled())
        CreateRootAccessible();
#endif

    NativeShow(aState);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
    CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                    : CSSToLayoutDeviceScale(1.0);
    int32_t width = NSToIntRound(scale.scale * aWidth);
    int32_t height = NSToIntRound(scale.scale * aHeight);
    ConstrainSize(&width, &height);

    // For top-level windows, aWidth and aHeight should possibly be
    // interpreted as frame bounds, but NativeResize treats these as window
    // bounds (Bug 581866).

    mBounds.SizeTo(width, height);

    if (!mCreated)
        return NS_OK;

    // There are several cases here that we need to handle, based on a
    // matrix of the visibility of the widget, the sanity of this resize
    // and whether or not the widget was previously sane.

    // Has this widget been set to visible?
    if (mIsShown) {
        // Are the bounds sane?
        if (AreBoundsSane()) {
            // Yep?  Resize the window
            //Maybe, the toplevel has moved

            // Note that if the widget needs to be positioned because its
            // size was previously insane in Resize(x,y,w,h), then we need
            // to set the x and y here too, because the widget wasn't
            // moved back then
            if (mNeedsMove)
                NativeResize(mBounds.x, mBounds.y,
                             mBounds.width, mBounds.height, aRepaint);
            else
                NativeResize(mBounds.width, mBounds.height, aRepaint);

            // Does it need to be shown because it was previously insane?
            if (mNeedsShow)
                NativeShow(true);
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(false) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = true;
                NativeShow(false);
            }
        }
    }
    // If the widget hasn't been shown, mark the widget as needing to be
    // resized before it is shown.
    else {
        if (AreBoundsSane() && mListenForResizes) {
            // For widgets that we listen for resizes for (widgets created
            // with native parents) we apparently _always_ have to resize.  I
            // dunno why, but apparently we're lame like that.
            NativeResize(width, height, aRepaint);
        }
        else {
            mNeedsResize = true;
        }
    }

    NotifyRollupGeometryChange();

    // send a resize notification if this is a toplevel
    if (mIsTopLevel || mListenForResizes) {
        DispatchResized(width, height);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                 bool aRepaint)
{
    CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                    : CSSToLayoutDeviceScale(1.0);
    int32_t width = NSToIntRound(scale.scale * aWidth);
    int32_t height = NSToIntRound(scale.scale * aHeight);
    ConstrainSize(&width, &height);

    int32_t x = NSToIntRound(scale.scale * aX);
    int32_t y = NSToIntRound(scale.scale * aY);
    mBounds.x = x;
    mBounds.y = y;
    mBounds.SizeTo(width, height);

    mNeedsMove = true;

    if (!mCreated)
        return NS_OK;

    // There are several cases here that we need to handle, based on a
    // matrix of the visibility of the widget, the sanity of this resize
    // and whether or not the widget was previously sane.

    // Has this widget been set to visible?
    if (mIsShown) {
        // Are the bounds sane?
        if (AreBoundsSane()) {
            // Yep?  Resize the window
            NativeResize(x, y, width, height, aRepaint);
            // Does it need to be shown because it was previously insane?
            if (mNeedsShow)
                NativeShow(true);
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(false) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = true;
                NativeShow(false);
            }
        }
    }
    // If the widget hasn't been shown, mark the widget as needing to be
    // resized before it is shown
    else {
        if (AreBoundsSane() && mListenForResizes){
            // For widgets that we listen for resizes for (widgets created
            // with native parents) we apparently _always_ have to resize.  I
            // dunno why, but apparently we're lame like that.
            NativeResize(x, y, width, height, aRepaint);
        }
        else {
            mNeedsResize = true;
        }
    }

    NotifyRollupGeometryChange();

    if (mIsTopLevel || mListenForResizes) {
        DispatchResized(width, height);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Enable(bool aState)
{
    mEnabled = aState;

    return NS_OK;
}

bool
nsWindow::IsEnabled() const
{
    return mEnabled;
}



NS_IMETHODIMP
nsWindow::Move(double aX, double aY)
{
    LOG(("nsWindow::Move [%p] %f %f\n", (void *)this,
         aX, aY));

    CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                   : CSSToLayoutDeviceScale(1.0);
    int32_t x = NSToIntRound(aX * scale.scale);
    int32_t y = NSToIntRound(aY * scale.scale);

    if (mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog) {
        SetSizeMode(nsSizeMode_Normal);
    }

    // Since a popup window's x/y coordinates are in relation to to
    // the parent, the parent might have moved so we always move a
    // popup window.
    if (x == mBounds.x && y == mBounds.y &&
        mWindowType != eWindowType_popup)
        return NS_OK;

    // XXX Should we do some AreBoundsSane check here?

    mBounds.x = x;
    mBounds.y = y;

    if (!mCreated)
        return NS_OK;

    mNeedsMove = false;

    if (mIsTopLevel) {
        gtk_window_move(GTK_WINDOW(mShell), x, y);
    }
    else if (mGdkWindow) {
        gdk_window_move(mGdkWindow, x, y);
    }

    NotifyRollupGeometryChange();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                      nsIWidget                  *aWidget,
                      bool                        aActivate)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetZIndex(int32_t aZIndex)
{
    nsIWidget* oldPrev = GetPrevSibling();

    nsBaseWidget::SetZIndex(aZIndex);

    if (GetPrevSibling() == oldPrev) {
        return NS_OK;
    }

    NS_ASSERTION(!mContainer, "Expected Mozilla child widget");

    // We skip the nsWindows that don't have mGdkWindows.
    // These are probably in the process of being destroyed.

    if (!GetNextSibling()) {
        // We're to be on top.
        if (mGdkWindow)
            gdk_window_raise(mGdkWindow);
    } else {
        // All the siblings before us need to be below our widget.
        for (nsWindow* w = this; w;
             w = static_cast<nsWindow*>(w->GetPrevSibling())) {
            if (w->mGdkWindow)
                gdk_window_lower(w->mGdkWindow);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetSizeMode(int32_t aMode)
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
    case nsSizeMode_Fullscreen:
        MakeFullScreen(true);
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

typedef void (* SetUserTimeFunc)(GdkWindow* aWindow, guint32 aTimestamp);

// This will become obsolete when new GTK APIs are widely supported,
// as described here: http://bugzilla.gnome.org/show_bug.cgi?id=347375
static void
SetUserTimeAndStartupIDForActivatedWindow(GtkWidget* aWindow)
{
    nsGTKToolkit* GTKToolkit = nsGTKToolkit::GetToolkit();
    if (!GTKToolkit)
        return;

    nsAutoCString desktopStartupID;
    GTKToolkit->GetDesktopStartupID(&desktopStartupID);
    if (desktopStartupID.IsEmpty()) {
        // We don't have the data we need. Fall back to an
        // approximation ... using the timestamp of the remote command
        // being received as a guess for the timestamp of the user event
        // that triggered it.
        uint32_t timestamp = GTKToolkit->GetFocusTimestamp();
        if (timestamp) {
            gdk_window_focus(gtk_widget_get_window(aWindow), timestamp);
            GTKToolkit->SetFocusTimestamp(0);
        }
        return;
    }

#if defined(MOZ_ENABLE_STARTUP_NOTIFICATION)
    GdkWindow* gdkWindow = gtk_widget_get_window(aWindow);
  
    GdkScreen* screen = gdk_window_get_screen(gdkWindow);
    SnDisplay* snd =
        sn_display_new(gdk_x11_display_get_xdisplay(gdk_window_get_display(gdkWindow)), 
                       nullptr, nullptr);
    if (!snd)
        return;
    SnLauncheeContext* ctx =
        sn_launchee_context_new(snd, gdk_screen_get_number(screen),
                                desktopStartupID.get());
    if (!ctx) {
        sn_display_unref(snd);
        return;
    }

    if (sn_launchee_context_get_id_has_timestamp(ctx)) {
        PRLibrary* gtkLibrary;
        SetUserTimeFunc setUserTimeFunc = (SetUserTimeFunc)
            PR_FindFunctionSymbolAndLibrary("gdk_x11_window_set_user_time", &gtkLibrary);
        if (setUserTimeFunc) {
            setUserTimeFunc(gdkWindow, sn_launchee_context_get_timestamp(ctx));
            PR_UnloadLibrary(gtkLibrary);
        }
    }

    sn_launchee_context_setup_window(ctx, gdk_x11_window_get_xid(gdkWindow));
    sn_launchee_context_complete(ctx);

    sn_launchee_context_unref(ctx);
    sn_display_unref(snd);
#endif

    // If we used the startup ID, that already contains the focus timestamp;
    // we don't want to reuse the timestamp next time we raise the window
    GTKToolkit->SetFocusTimestamp(0);
    GTKToolkit->SetDesktopStartupID(EmptyCString());
}

/* static */ guint32
nsWindow::GetLastUserInputTime()
{
    // gdk_x11_display_get_user_time tracks button and key presses,
    // DESKTOP_STARTUP_ID used to start the app, drop events from external
    // drags, WM_DELETE_WINDOW delete events, but not usually mouse motion nor
    // button and key releases.  Therefore use the most recent of
    // gdk_x11_display_get_user_time and the last time that we have seen.
    guint32 timestamp =
            gdk_x11_display_get_user_time(gdk_display_get_default());
    if (sLastUserInputTime != GDK_CURRENT_TIME &&
        TimestampIsNewerThan(sLastUserInputTime, timestamp)) {
        return sLastUserInputTime;
    }       

    return timestamp;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    // Make sure that our owning widget has focus.  If it doesn't try to
    // grab it.  Note that we don't set our focus flag in this case.

    LOGFOCUS(("  SetFocus %d [%p]\n", aRaise, (void *)this));

    GtkWidget *owningWidget = GetMozContainerWidget();
    if (!owningWidget)
        return NS_ERROR_FAILURE;

    // Raise the window if someone passed in true and the prefs are
    // set properly.
    GtkWidget *toplevelWidget = gtk_widget_get_toplevel(owningWidget);

    if (gRaiseWindows && aRaise && toplevelWidget &&
        !gtk_widget_has_focus(owningWidget) &&
        !gtk_widget_has_focus(toplevelWidget)) {
        GtkWidget* top_window = GetToplevelWidget();
        if (top_window && (gtk_widget_get_visible(top_window)))
        {
            gdk_window_show_unraised(gtk_widget_get_window(top_window));
            // Unset the urgency hint if possible.
            SetUrgencyHint(top_window, false);
        }
    }

    nsRefPtr<nsWindow> owningWindow = get_window_for_gtk_widget(owningWidget);
    if (!owningWindow)
        return NS_ERROR_FAILURE;

    if (aRaise) {
        // aRaise == true means request toplevel activation.

        // This is asynchronous.
        // If and when the window manager accepts the request, then the focus
        // widget will get a focus-in-event signal.
        if (gRaiseWindows && owningWindow->mIsShown && owningWindow->mShell &&
            !gtk_window_is_active(GTK_WINDOW(owningWindow->mShell))) {

            uint32_t timestamp = GDK_CURRENT_TIME;

            nsGTKToolkit* GTKToolkit = nsGTKToolkit::GetToolkit();
            if (GTKToolkit)
                timestamp = GTKToolkit->GetFocusTimestamp();

            LOGFOCUS(("  requesting toplevel activation [%p]\n", (void *)this));
            NS_ASSERTION(owningWindow->mWindowType != eWindowType_popup
                         || mParent,
                         "Presenting an override-redirect window");
            gtk_window_present_with_time(GTK_WINDOW(owningWindow->mShell), timestamp);

            if (GTKToolkit)
                GTKToolkit->SetFocusTimestamp(0);
        }

        return NS_OK;
    }

    // aRaise == false means that keyboard events should be dispatched
    // from this widget.

    // Ensure owningWidget is the focused GtkWidget within its toplevel window.
    //
    // For eWindowType_popup, this GtkWidget may not actually be the one that
    // receives the key events as it may be the parent window that is active.
    if (!gtk_widget_is_focus(owningWidget)) {
        // This is synchronous.  It takes focus from a plugin or from a widget
        // in an embedder.  The focus manager already knows that this window
        // is active so gBlockActivateEvent avoids another (unnecessary)
        // activate notification.
        gBlockActivateEvent = true;
        gtk_widget_grab_focus(owningWidget);
        gBlockActivateEvent = false;
    }

    // If this is the widget that already has focus, return.
    if (gFocusWindow == this) {
        LOGFOCUS(("  already have focus [%p]\n", (void *)this));
        return NS_OK;
    }

    // Set this window to be the focused child window
    gFocusWindow = this;

    if (mIMModule) {
        mIMModule->OnFocusWindow(this);
    }

    LOGFOCUS(("  widget now has focus in SetFocus() [%p]\n",
              (void *)this));

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsIntRect &aRect)
{
    if (mIsTopLevel && mContainer) {
        // use the point including window decorations
        gint x, y;
        gdk_window_get_root_origin(gtk_widget_get_window(GTK_WIDGET(mContainer)), &x, &y);
        aRect.MoveTo(x, y);
    }
    else {
        aRect.MoveTo(WidgetToScreenOffset());
    }
    // mBounds.Size() is the window bounds, not the window-manager frame
    // bounds (bug 581863).  gdk_window_get_frame_extents would give the
    // frame bounds, but mBounds.Size() is returned here for consistency
    // with Resize.
    aRect.SizeTo(mBounds.Size());
    LOG(("GetScreenBounds %d,%d | %dx%d\n",
         aRect.x, aRect.y, aRect.width, aRect.height));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetClientBounds(nsIntRect &aRect)
{
    // GetBounds returns a rect whose top left represents the top left of the
    // outer bounds, but whose width/height represent the size of the inner
    // bounds (which is messed up).
    GetBounds(aRect);
    aRect.MoveBy(GetClientOffset());

    return NS_OK;
}

nsIntPoint
nsWindow::GetClientOffset()
{
    if (!mIsTopLevel) {
        return nsIntPoint(0, 0);
    }

    GdkAtom cardinal_atom = gdk_x11_xatom_to_atom(XA_CARDINAL);

    GdkAtom type_returned;
    int format_returned;
    int length_returned;
    long *frame_extents;
    GdkWindow* window;

    if (!mShell || !(window = gtk_widget_get_window(mShell)) ||
        !gdk_property_get(window,
                          gdk_atom_intern ("_NET_FRAME_EXTENTS", FALSE),
                          cardinal_atom,
                          0, // offset
                          4*4, // length
                          FALSE, // delete
                          &type_returned,
                          &format_returned,
                          &length_returned,
                          (guchar **) &frame_extents) ||
        length_returned/sizeof(glong) != 4) {

        return nsIntPoint(0, 0);
    }

    // data returned is in the order left, right, top, bottom
    int32_t left = int32_t(frame_extents[0]);
    int32_t top = int32_t(frame_extents[2]);

    g_free(frame_extents);

    return nsIntPoint(left, top);
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

NS_IMETHODIMP
nsWindow::SetCursor(nsCursor aCursor)
{
    // if we're not the toplevel window pass up the cursor request to
    // the toplevel window to handle it.
    if (!mContainer && mGdkWindow) {
        nsWindow *window = GetContainerWindow();
        if (!window)
            return NS_ERROR_FAILURE;

        return window->SetCursor(aCursor);
    }

    // Only change cursor if it's actually been changed
    if (aCursor != mCursor) {
        GdkCursor *newCursor = NULL;

        newCursor = get_gtk_cursor(aCursor);

        if (nullptr != newCursor) {
            mCursor = aCursor;

            if (!mContainer)
                return NS_OK;

            gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(mContainer)), newCursor);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetCursor(imgIContainer* aCursor,
                    uint32_t aHotspotX, uint32_t aHotspotY)
{
    // if we're not the toplevel window pass up the cursor request to
    // the toplevel window to handle it.
    if (!mContainer && mGdkWindow) {
        nsWindow *window = GetContainerWindow();
        if (!window)
            return NS_ERROR_FAILURE;

        return window->SetCursor(aCursor, aHotspotX, aHotspotY);
    }

    mCursor = nsCursor(-1);

    // Get the image's current frame
    GdkPixbuf* pixbuf = nsImageToPixbuf::ImageToPixbuf(aCursor);
    if (!pixbuf)
        return NS_ERROR_NOT_AVAILABLE;

    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    // Reject cursors greater than 128 pixels in some direction, to prevent
    // spoofing.
    // XXX ideally we should rescale. Also, we could modify the API to
    // allow trusted content to set larger cursors.
    if (width > 128 || height > 128) {
        g_object_unref(pixbuf);
        return NS_ERROR_NOT_AVAILABLE;
    }

    // Looks like all cursors need an alpha channel (tested on Gtk 2.4.4). This
    // is of course not documented anywhere...
    // So add one if there isn't one yet
    if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
        GdkPixbuf* alphaBuf = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
        g_object_unref(pixbuf);
        if (!alphaBuf) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        pixbuf = alphaBuf;
    }

    GdkCursor* cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(),
                                                   pixbuf,
                                                   aHotspotX, aHotspotY);
    g_object_unref(pixbuf);
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    if (cursor) {
        if (mContainer) {
            gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(mContainer)), cursor);
            rv = NS_OK;
        }
        gdk_cursor_unref(cursor);
    }

    return rv;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect)
{
    if (!mGdkWindow)
        return NS_OK;

    GdkRectangle rect;
    rect.x = aRect.x;
    rect.y = aRect.y;
    rect.width = aRect.width;
    rect.height = aRect.height;

    LOGDRAW(("Invalidate (rect) [%p]: %d %d %d %d\n", (void *)this,
             rect.x, rect.y, rect.width, rect.height));

    gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);

    return NS_OK;
}

void*
nsWindow::GetNativeData(uint32_t aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
        if (!mGdkWindow)
            return nullptr;

        return mGdkWindow;
        break;
    }

    case NS_NATIVE_PLUGIN_PORT:
        return SetupPluginPort();
        break;

    case NS_NATIVE_DISPLAY:
#ifdef MOZ_X11
        return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
#else
        return nullptr;
#endif /* MOZ_X11 */
        break;

    case NS_NATIVE_SHELLWIDGET:
        return GetToplevelWidget();

    case NS_NATIVE_SHAREABLE_WINDOW:
        return (void *) GDK_WINDOW_XID(gdk_window_get_toplevel(mGdkWindow));

    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nullptr;
    }
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
        uint32_t len = NS_WINDOW_TITLE_MAX_LENGTH;
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

    nsAutoCString iconName;
    
    if (aIconSpec.EqualsLiteral("default")) {
        nsXPIDLString brandName;
        GetBrandName(brandName);
        AppendUTF16toUTF8(brandName, iconName);
        ToLowerCase(iconName);
    } else {
        AppendUTF16toUTF8(aIconSpec, iconName);
    }
    
    nsCOMPtr<nsIFile> iconFile;
    nsAutoCString path;

    gint *iconSizes =
        gtk_icon_theme_get_icon_sizes(gtk_icon_theme_get_default(),
                                      iconName.get());
    bool foundIcon = (iconSizes[0] != 0);
    g_free(iconSizes);

    if (!foundIcon) {
        // Look for icons with the following suffixes appended to the base name
        // The last two entries (for the old XPM format) will be ignored unless
        // no icons are found using other suffixes. XPM icons are deprecated.

        const char extensions[6][7] = { ".png", "16.png", "32.png", "48.png",
                                    ".xpm", "16.xpm" };

        for (uint32_t i = 0; i < ArrayLength(extensions); i++) {
            // Don't bother looking for XPM versions if we found a PNG.
            if (i == ArrayLength(extensions) - 2 && foundIcon)
                break;

            nsAutoString extension;
            extension.AppendASCII(extensions[i]);

            ResolveIconName(aIconSpec, extension, getter_AddRefs(iconFile));
            if (iconFile) {
                iconFile->GetNativePath(path);
                GdkPixbuf *icon = gdk_pixbuf_new_from_file(path.get(), NULL);
                if (icon){
                    gtk_icon_theme_add_builtin_icon(iconName.get(),
                                                    gdk_pixbuf_get_height(icon),
                                                    icon);
                    g_object_unref(icon);
                    foundIcon = true;
                }
            }
        }
    }

    // leave the default icon intact if no matching icons were found
    if (foundIcon) {
        gtk_window_set_icon_name(GTK_WINDOW(mShell), iconName.get());
    }

    return NS_OK;
}


nsIntPoint
nsWindow::WidgetToScreenOffset()
{
    gint x = 0, y = 0;

    if (mGdkWindow) {
        gdk_window_get_origin(mGdkWindow, &x, &y);
    }

    return nsIntPoint(x, y);
}

NS_IMETHODIMP
nsWindow::EnableDragDrop(bool aEnable)
{
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureMouse(bool aCapture)
{
    LOG(("CaptureMouse %p\n", (void *)this));

    if (!mGdkWindow)
        return NS_OK;

    if (!mShell)
        return NS_ERROR_FAILURE;

    if (aCapture) {
        gtk_grab_add(mShell);
        GrabPointer(GetLastUserInputTime());
    }
    else {
        ReleaseGrabs();
        gtk_grab_remove(mShell);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
                              bool               aDoCapture)
{
    if (!mGdkWindow)
        return NS_OK;

    if (!mShell)
        return NS_ERROR_FAILURE;

    LOG(("CaptureRollupEvents %p %i\n", this, int(aDoCapture)));

    if (aDoCapture) {
        gRollupListener = aListener;
        // real grab is only done when there is no dragging
        if (!nsWindow::DragInProgress()) {
            // This widget grab ensures that a Gecko GtkWidget receives mouse
            // events even when embedded in non-Gecko-owned GtkWidgets.
            // The grab is placed on the toplevel GtkWindow instead of the
            // MozContainer to avoid double dispatch of keyboard events
            // (bug 707623).
            gtk_grab_add(mShell);
            GrabPointer(GetLastUserInputTime());
        }
    }
    else {
        if (!nsWindow::DragInProgress()) {
            ReleaseGrabs();
        }
        // There may not have been a drag in process when aDoCapture was set,
        // so make sure to remove any added grab.  This is a no-op if the grab
        // was not added to this widget.
        gtk_grab_remove(mShell);
        gRollupListener = nullptr;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetAttention(int32_t aCycleCount)
{
    LOG(("nsWindow::GetAttention [%p]\n", (void *)this));

    GtkWidget* top_window = GetToplevelWidget();
    GtkWidget* top_focused_window =
        gFocusWindow ? gFocusWindow->GetToplevelWidget() : nullptr;

    // Don't get attention if the window is focused anyway.
    if (top_window && (gtk_widget_get_visible(top_window)) &&
        top_window != top_focused_window) {
        SetUrgencyHint(top_window, true);
    }

    return NS_OK;
}

bool
nsWindow::HasPendingInputEvent()
{
    // This sucks, but gtk/gdk has no way to answer the question we want while
    // excluding paint events, and there's no X API that will let us peek
    // without blocking or removing.  To prevent event reordering, peek
    // anything except expose events.  Reordering expose and others should be
    // ok, hopefully.
    bool haveEvent;
#ifdef MOZ_X11
    XEvent ev;
    Display *display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    haveEvent =
        XCheckMaskEvent(display,
                        KeyPressMask | KeyReleaseMask | ButtonPressMask |
                        ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
                        PointerMotionMask | PointerMotionHintMask |
                        Button1MotionMask | Button2MotionMask |
                        Button3MotionMask | Button4MotionMask |
                        Button5MotionMask | ButtonMotionMask | KeymapStateMask |
                        VisibilityChangeMask | StructureNotifyMask |
                        ResizeRedirectMask | SubstructureNotifyMask |
                        SubstructureRedirectMask | FocusChangeMask |
                        PropertyChangeMask | ColormapChangeMask |
                        OwnerGrabButtonMask, &ev);
    if (haveEvent) {
        XPutBackEvent(display, &ev);
    }
#else
    haveEvent = false;
#endif
    return haveEvent;
}

#if 0
#ifdef DEBUG
// Paint flashing code (disabled for cairo - see below)

#define CAPS_LOCK_IS_ON \
(KeymapWrapper::AreModifiersCurrentlyActive(KeymapWrapper::CAPS_LOCK))

#define WANT_PAINT_FLASHING \
(debug_WantPaintFlashing() && CAPS_LOCK_IS_ON)

#ifdef MOZ_X11
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

#if (MOZ_WIDGET_GTK == 2)
  gdk_window_get_geometry(aGdkWindow,NULL,NULL,&width,&height,NULL);
#else
  gdk_window_get_geometry(aGdkWindow,NULL,NULL,&width,&height);
#endif

  gdk_window_get_origin (aGdkWindow,
                         &x,
                         &y);

  gc = gdk_gc_new(gdk_get_default_root_window());

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
    gdk_draw_rectangle(gdk_get_default_root_window(),
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
#endif /* MOZ_X11 */
#endif // DEBUG
#endif

#if (MOZ_WIDGET_GTK == 2)
gboolean
nsWindow::OnExposeEvent(GdkEventExpose *aEvent)
#else
gboolean
nsWindow::OnExposeEvent(cairo_t *cr)
#endif
{
    if (mIsDestroyed) {
        return FALSE;
    }

    // Windows that are not visible will be painted after they become visible.
    if (!mGdkWindow || mIsFullyObscured || !mHasMappedToplevel)
        return FALSE;

    nsIWidgetListener *listener =
        mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
    if (!listener)
        return FALSE;

    // Do an early async composite so that we at least have something on screen
    // in the right place, even if the content is out of date.
    if (GetLayerManager()->GetBackendType() == LAYERS_CLIENT && mCompositorParent) {
        mCompositorParent->ScheduleRenderOnCompositorThread();
    }

    // Dispatch WillPaintWindow notification to allow scripts etc. to run
    // before we paint
    {
        listener->WillPaintWindow(this);

        // If the window has been destroyed during the will paint notification,
        // there is nothing left to do.
        if (!mGdkWindow)
            return TRUE;

        // Re-get the listener since the will paint notification might have
        // killed it.
        listener =
            mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
        if (!listener)
            return FALSE;
    }

#if (MOZ_WIDGET_GTK == 2)
    GdkRectangle *rects;
    gint nrects;
    gdk_region_get_rectangles(aEvent->region, &rects, &nrects);
    if (MOZ_UNLIKELY(!rects)) // OOM
        return FALSE;
#else
#ifdef cairo_copy_clip_rectangle_list
#error "Looks like we're including Mozilla's cairo instead of system cairo"
#else
    cairo_rectangle_list_t *rects;
    rects = cairo_copy_clip_rectangle_list(cr);  
    if (MOZ_UNLIKELY(rects->status != CAIRO_STATUS_SUCCESS)) {
       NS_WARNING("Failed to obtain cairo rectangle list.");
       return FALSE;
    }
#endif
#endif

// GTK3 TODO?
#if (MOZ_WIDGET_GTK == 2)
    if (nrects > MAX_RECTS_IN_REGION) {
        // Just use the bounding box
        rects[0] = aEvent->area;
        nrects = 1;
    }
#endif

    LOGDRAW(("sending expose event [%p] %p 0x%lx (rects follow):\n",
             (void *)this, (void *)mGdkWindow,
             gdk_x11_window_get_xid(mGdkWindow)));

    nsIntRegion region;
  
#if (MOZ_WIDGET_GTK == 2)
    GdkRectangle *r = rects;
    GdkRectangle *r_end = rects + nrects;
#else
    cairo_rectangle_t *r = rects->rectangles;
    cairo_rectangle_t *r_end = r + rects->num_rectangles;
#endif
    for (; r < r_end; ++r) {
        region.Or(region, nsIntRect(r->x, r->y, r->width, r->height));
        LOGDRAW(("\t%d %d %d %d\n", r->x, r->y, r->width, r->height));
    }

    // Our bounds may have changed after calling WillPaintWindow.  Clip
    // to the new bounds here.  The region is relative to this
    // window.
    region.And(region, nsIntRect(0, 0, mBounds.width, mBounds.height));

    bool shaped = false;
    if (eTransparencyTransparent == GetTransparencyMode()) {
        GdkScreen *screen = gdk_window_get_screen(mGdkWindow);
        if (gdk_screen_is_composited(screen) &&
            gdk_window_get_visual(mGdkWindow) ==
            gdk_screen_get_rgba_visual(screen)) {
            // Remove possible shape mask from when window manger was not
            // previously compositing.
            static_cast<nsWindow*>(GetTopLevelWidget())->
                ClearTransparencyBitmap();
        } else {
            shaped = true;
        }
    }

    if (!shaped) {
        GList *children =
            gdk_window_peek_children(mGdkWindow);
        while (children) {
            GdkWindow *gdkWin = GDK_WINDOW(children->data);
            nsWindow *kid = get_window_for_gdk_window(gdkWin);
            if (kid && gdk_window_is_visible(gdkWin)) {
                nsAutoTArray<nsIntRect,1> clipRects;
                kid->GetWindowClipRegion(&clipRects);
                nsIntRect bounds;
                kid->GetBounds(bounds);
                for (uint32_t i = 0; i < clipRects.Length(); ++i) {
                    nsIntRect r = clipRects[i] + bounds.TopLeft();
                    region.Sub(region, r);
                }
            }
            children = children->next;
        }
    }

    if (region.IsEmpty()) {
#if (MOZ_WIDGET_GTK == 2)
        g_free(rects);
#else
        cairo_rectangle_list_destroy(rects);
#endif
        return TRUE;
    }
    // If this widget uses OMTC...
    if (GetLayerManager()->GetBackendType() == LAYERS_CLIENT) {
        listener->PaintWindow(this, region);
        listener->DidPaintWindow();

        g_free(rects);
        return TRUE;
    } else if (GetLayerManager()->GetBackendType() == mozilla::layers::LAYERS_OPENGL) {
        LayerManagerOGL *manager = static_cast<LayerManagerOGL*>(GetLayerManager());
        manager->SetClippingRegion(region);

        listener->PaintWindow(this, region);
        listener->DidPaintWindow();

        g_free(rects);
        return TRUE;
    }

    gfxASurface* surf;
#if (MOZ_WIDGET_GTK == 2)
    surf = GetThebesSurface();
#else
    surf = GetThebesSurface(cr);
#endif

    nsRefPtr<gfxContext> ctx;
    if (gfxPlatform::GetPlatform()->
            SupportsAzureContentForType(BACKEND_CAIRO)) {
        IntSize intSize(surf->GetSize().width, surf->GetSize().height);
        ctx = new gfxContext(gfxPlatform::GetPlatform()->
            CreateDrawTargetForSurface(surf, intSize));
    } else if (gfxPlatform::GetPlatform()->
                   SupportsAzureContentForType(BACKEND_SKIA) &&
               surf->GetType() != gfxSurfaceTypeImage) {
       gfxImageSurface* imgSurf = static_cast<gfxImageSurface*>(surf);
       SurfaceFormat format = ImageFormatToSurfaceFormat(imgSurf->Format());
       IntSize intSize(surf->GetSize().width, surf->GetSize().height);
       ctx = new gfxContext(gfxPlatform::GetPlatform()->CreateDrawTargetForData(
           imgSurf->Data(), intSize, imgSurf->Stride(), format));
    }  else {
        ctx = new gfxContext(surf);
    }

#ifdef MOZ_X11
    nsIntRect boundsRect; // for shaped only

    ctx->NewPath();
    if (shaped) {
        // Collapse update area to the bounding box. This is so we only have to
        // call UpdateTranslucentWindowAlpha once. After we have dropped
        // support for non-Thebes graphics, UpdateTranslucentWindowAlpha will be
        // our private interface so we can rework things to avoid this.
        boundsRect = region.GetBounds();
        ctx->Rectangle(gfxRect(boundsRect.x, boundsRect.y,
                               boundsRect.width, boundsRect.height));
    } else {
        gfxUtils::PathFromRegion(ctx, region);
    }
    ctx->Clip();

    BufferMode layerBuffering;
    if (shaped) {
        // The double buffering is done here to extract the shape mask.
        // (The shape mask won't be necessary when a visual with an alpha
        // channel is used on compositing window managers.)
        layerBuffering = mozilla::layers::BUFFER_NONE;
        ctx->PushGroup(GFX_CONTENT_COLOR_ALPHA);
#ifdef MOZ_HAVE_SHMIMAGE
    } else if (nsShmImage::UseShm()) {
        // We're using an xshm mapping as a back buffer.
        layerBuffering = mozilla::layers::BUFFER_NONE;
#endif // MOZ_HAVE_SHMIMAGE
    } else {
        // Get the layer manager to do double buffering (if necessary).
        layerBuffering = mozilla::layers::BUFFER_BUFFERED;
    }

#if 0
    // NOTE: Paint flashing region would be wrong for cairo, since
    // cairo inflates the update region, etc.  So don't paint flash
    // for cairo.
#ifdef DEBUG
    // XXX aEvent->region may refer to a newly-invalid area.  FIXME
    if (0 && WANT_PAINT_FLASHING && gtk_widget_get_window(aEvent))
        gdk_window_flash(mGdkWindow, 1, 100, aEvent->region);
#endif
#endif

#endif // MOZ_X11

    bool painted = false;
    {
      if (GetLayerManager()->GetBackendType() == LAYERS_BASIC) {
        AutoLayerManagerSetup setupLayerManager(this, ctx, layerBuffering);
        painted = listener->PaintWindow(this, region);
      }
    }

#ifdef MOZ_X11
    // PaintWindow can Destroy us (bug 378273), avoid doing any paint
    // operations below if that happened - it will lead to XError and exit().
    if (shaped) {
        if (MOZ_LIKELY(!mIsDestroyed)) {
            if (painted) {
                nsRefPtr<gfxPattern> pattern = ctx->PopGroup();

                UpdateAlpha(pattern, boundsRect);

                ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
                ctx->SetPattern(pattern);
                ctx->Paint();
            }
        }
    }
#  ifdef MOZ_HAVE_SHMIMAGE
    if (nsShmImage::UseShm() && MOZ_LIKELY(!mIsDestroyed)) {
#if (MOZ_WIDGET_GTK == 2)
        mShmImage->Put(mGdkWindow, rects, r_end);
#else
        mShmImage->Put(mGdkWindow, rects);
#endif
    }
#  endif  // MOZ_HAVE_SHMIMAGE
#endif // MOZ_X11

#if (MOZ_WIDGET_GTK == 2)
    g_free(rects);
#else
    cairo_rectangle_list_destroy(rects);
#endif

    listener->DidPaintWindow();

    // Synchronously flush any new dirty areas
#if (MOZ_WIDGET_GTK == 2)
    GdkRegion* dirtyArea = gdk_window_get_update_area(mGdkWindow);
#else
    cairo_region_t* dirtyArea = gdk_window_get_update_area(mGdkWindow);
#endif

    if (dirtyArea) {
        gdk_window_invalidate_region(mGdkWindow, dirtyArea, false);
#if (MOZ_WIDGET_GTK == 2)
        gdk_region_destroy(dirtyArea);
#else
        cairo_region_destroy(dirtyArea);
#endif
        gdk_window_process_updates(mGdkWindow, false);
    }

    // check the return value!
    return TRUE;
}

void
nsWindow::UpdateAlpha(gfxPattern* aPattern, nsIntRect aBoundsRect)
{
  if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
      // We need to create our own buffer to force the stride to match the
      // expected stride.
      int32_t stride = GetAlignedStride<4>(BytesPerPixel(FORMAT_A8) *
                                           aBoundsRect.width);
      int32_t bufferSize = stride * aBoundsRect.height;
      nsAutoArrayPtr<uint8_t> imageBuffer(new (std::nothrow) uint8_t[bufferSize]);
      RefPtr<DrawTarget> drawTarget = gfxPlatform::GetPlatform()->
          CreateDrawTargetForData(imageBuffer, ToIntSize(aBoundsRect.Size()),
                                  stride, FORMAT_A8);

      if (drawTarget) {
          drawTarget->FillRect(ToRect(aBoundsRect),
                               *aPattern->GetPattern(drawTarget),
                               DrawOptions(1.0, OP_SOURCE));
      }
      UpdateTranslucentWindowAlphaInternal(aBoundsRect, imageBuffer, stride);
  } else {
      nsRefPtr<gfxImageSurface> img =
          new gfxImageSurface(ThebesIntSize(aBoundsRect.Size()),
                              gfxImageFormatA8);
      if (img && !img->CairoStatus()) {
          img->SetDeviceOffset(-aBoundsRect.TopLeft());

          nsRefPtr<gfxContext> imgCtx = new gfxContext(img);
          imgCtx->SetPattern(aPattern);
          imgCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
          imgCtx->Paint();

          UpdateTranslucentWindowAlphaInternal(aBoundsRect, img->Data(),
                                               img->Stride());
      }
  }
}

gboolean
nsWindow::OnConfigureEvent(GtkWidget *aWidget, GdkEventConfigure *aEvent)
{
    // These events are only received on toplevel windows.
    //
    // GDK ensures that the coordinates are the client window top-left wrt the
    // root window.
    //
    //   GDK calculates the cordinates for real ConfigureNotify events on
    //   managed windows (that would normally be relative to the parent
    //   window).
    //
    //   Synthetic ConfigureNotify events are from the window manager and
    //   already relative to the root window.  GDK creates all X windows with
    //   border_width = 0, so synthetic events also indicate the top-left of
    //   the client window.
    //
    //   Override-redirect windows are children of the root window so parent
    //   coordinates are root coordinates.

    LOG(("configure event [%p] %d %d %d %d\n", (void *)this,
         aEvent->x, aEvent->y, aEvent->width, aEvent->height));

    nsIntRect screenBounds;
    GetScreenBounds(screenBounds);

    if (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog) {
        // This check avoids unwanted rollup on spurious configure events from
        // Cygwin/X (bug 672103).
        if (mBounds.x != screenBounds.x ||
            mBounds.y != screenBounds.y) {
            CheckForRollup(0, 0, false, true);
        }
    }

    // This event indicates that the window position may have changed.
    // mBounds.Size() is updated in OnSizeAllocate().

    // (The gtk_window_get_window_type() function is only available from
    // version 2.20.)
    NS_ASSERTION(GTK_IS_WINDOW(aWidget),
                 "Configure event on widget that is not a GtkWindow");
    gint type;
    g_object_get(aWidget, "type", &type, NULL);
    if (type == GTK_WINDOW_POPUP) {
        // Override-redirect window
        //
        // These windows should not be moved by the window manager, and so any
        // change in position is a result of our direction.  mBounds has
        // already been set in Move() or Resize(), and that is more
        // up-to-date than the position in the ConfigureNotify event if the
        // event is from an earlier window move.
        //
        // Skipping the WindowMoved call saves context menus from an infinite
        // loop when nsXULPopupManager::PopupMoved moves the window to the new
        // position and nsMenuPopupFrame::SetPopupPosition adds
        // offsetForContextMenu on each iteration.
        return FALSE;
    }

    mBounds.MoveTo(screenBounds.TopLeft());

    // XXX mozilla will invalidate the entire window after this move
    // complete.  wtf?
    if (mWidgetListener)
      mWidgetListener->WindowMoved(this, mBounds.x, mBounds.y);

    return FALSE;
}

void
nsWindow::OnContainerUnrealize()
{
    // The GdkWindows are about to be destroyed (but not deleted), so remove
    // their references back to their container widget while the GdkWindow
    // hierarchy is still available.

    if (mGdkWindow) {
        DestroyChildWindows();

        g_object_set_data(G_OBJECT(mGdkWindow), "nsWindow", NULL);
        mGdkWindow = NULL;
    }
}

void
nsWindow::OnSizeAllocate(GtkAllocation *aAllocation)
{
    LOG(("size_allocate [%p] %d %d %d %d\n",
         (void *)this, aAllocation->x, aAllocation->y,
         aAllocation->width, aAllocation->height));

    nsIntSize size(aAllocation->width, aAllocation->height);
    if (mBounds.Size() == size)
        return;

    // Invalidate the new part of the window now for the pending paint to
    // minimize background flashes (GDK does not do this for external resizes
    // of toplevels.)
    if (mBounds.width < size.width) {
        GdkRectangle rect =
            { mBounds.width, 0, size.width - mBounds.width, size.height };
        gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);
    }
    if (mBounds.height < size.height) {
        GdkRectangle rect =
            { 0, mBounds.height, size.width, size.height - mBounds.height };
        gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);
    }

    mBounds.SizeTo(size);

    if (!mGdkWindow)
        return;

    DispatchResized(size.width, size.height);
}

void
nsWindow::OnDeleteEvent()
{
    if (mWidgetListener)
        mWidgetListener->RequestWindowClose(this);
}

void
nsWindow::OnEnterNotifyEvent(GdkEventCrossing *aEvent)
{
    // This skips NotifyVirtual and NotifyNonlinearVirtual enter notify events
    // when the pointer enters a child window.  If the destination window is a
    // Gecko window then we'll catch the corresponding event on that window,
    // but we won't notice when the pointer directly enters a foreign (plugin)
    // child window without passing over a visible portion of a Gecko window.
    if (aEvent->subwindow != NULL)
        return;

    // Check before is_parent_ungrab_enter() as the button state may have
    // changed while a non-Gecko ancestor window had a pointer grab.
    DispatchMissedButtonReleases(aEvent);

    if (is_parent_ungrab_enter(aEvent))
        return;

    WidgetMouseEvent event(true, NS_MOUSE_ENTER, this, WidgetMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->x);
    event.refPoint.y = nscoord(aEvent->y);

    event.time = aEvent->time;

    LOG(("OnEnterNotify: %p\n", (void *)this));

    nsEventStatus status;
    DispatchEvent(&event, status);
}

// XXX Is this the right test for embedding cases?
static bool
is_top_level_mouse_exit(GdkWindow* aWindow, GdkEventCrossing *aEvent)
{
    gint x = gint(aEvent->x_root);
    gint y = gint(aEvent->y_root);
    GdkDisplay* display = gdk_window_get_display(aWindow);
    GdkWindow* winAtPt = gdk_display_get_window_at_pointer(display, &x, &y);
    if (!winAtPt)
        return true;
    GdkWindow* topLevelAtPt = gdk_window_get_toplevel(winAtPt);
    GdkWindow* topLevelWidget = gdk_window_get_toplevel(aWindow);
    return topLevelAtPt != topLevelWidget;
}

void
nsWindow::OnLeaveNotifyEvent(GdkEventCrossing *aEvent)
{
    // This ignores NotifyVirtual and NotifyNonlinearVirtual leave notify
    // events when the pointer leaves a child window.  If the destination
    // window is a Gecko window then we'll catch the corresponding event on
    // that window.
    //
    // XXXkt However, we will miss toplevel exits when the pointer directly
    // leaves a foreign (plugin) child window without passing over a visible
    // portion of a Gecko window.
    if (aEvent->subwindow != NULL)
        return;

    WidgetMouseEvent event(true, NS_MOUSE_EXIT, this, WidgetMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->x);
    event.refPoint.y = nscoord(aEvent->y);

    event.time = aEvent->time;

    event.exit = is_top_level_mouse_exit(mGdkWindow, aEvent)
        ? WidgetMouseEvent::eTopLevel : WidgetMouseEvent::eChild;

    LOG(("OnLeaveNotify: %p\n", (void *)this));

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::OnMotionNotifyEvent(GdkEventMotion *aEvent)
{
    // see if we can compress this event
    // XXXldb Why skip every other motion event when we have multiple,
    // but not more than that?
    bool synthEvent = false;
#ifdef MOZ_X11
    XEvent xevent;

    while (XPending (GDK_WINDOW_XDISPLAY(aEvent->window))) {
        XEvent peeked;
        XPeekEvent (GDK_WINDOW_XDISPLAY(aEvent->window), &peeked);
        if (peeked.xany.window != gdk_x11_window_get_xid(aEvent->window)
            || peeked.type != MotionNotify)
            break;

        synthEvent = true;
        XNextEvent (GDK_WINDOW_XDISPLAY(aEvent->window), &xevent);
    }
#if (MOZ_WIDGET_GTK == 2)
    // if plugins still keeps the focus, get it back
    if (gPluginFocusWindow && gPluginFocusWindow != this) {
        nsRefPtr<nsWindow> kungFuDeathGrip = gPluginFocusWindow;
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }
#endif /* MOZ_WIDGET_GTK2 */
#endif /* MOZ_X11 */

    WidgetMouseEvent event(true, NS_MOUSE_MOVE, this, WidgetMouseEvent::eReal);

    gdouble pressure = 0;
    gdk_event_get_axis ((GdkEvent*)aEvent, GDK_AXIS_PRESSURE, &pressure);
    // Sometime gdk generate 0 pressure value between normal values
    // We have to ignore that and use last valid value
    if (pressure)
      mLastMotionPressure = pressure;
    event.pressure = mLastMotionPressure;

    guint modifierState;
    if (synthEvent) {
#ifdef MOZ_X11
        event.refPoint.x = nscoord(xevent.xmotion.x);
        event.refPoint.y = nscoord(xevent.xmotion.y);

        modifierState = xevent.xmotion.state;

        event.time = xevent.xmotion.time;
#else
        event.refPoint.x = nscoord(aEvent->x);
        event.refPoint.y = nscoord(aEvent->y);

        modifierState = aEvent->state;

        event.time = aEvent->time;
#endif /* MOZ_X11 */
    }
    else {
        // XXX see OnScrollEvent()
        if (aEvent->window == mGdkWindow) {
            event.refPoint.x = nscoord(aEvent->x);
            event.refPoint.y = nscoord(aEvent->y);
        } else {
            LayoutDeviceIntPoint point(NSToIntFloor(aEvent->x_root),
                                       NSToIntFloor(aEvent->y_root));
            event.refPoint = point -
                LayoutDeviceIntPoint::FromUntyped(WidgetToScreenOffset());
        }

        modifierState = aEvent->state;

        event.time = aEvent->time;
    }

    KeymapWrapper::InitInputEvent(event, modifierState);

    nsEventStatus status;
    DispatchEvent(&event, status);
}

// If the automatic pointer grab on ButtonPress has deactivated before
// ButtonRelease, and the mouse button is released while the pointer is not
// over any a Gecko window, then the ButtonRelease event will not be received.
// (A similar situation exists when the pointer is grabbed with owner_events
// True as the ButtonRelease may be received on a foreign [plugin] window).
// Use this method to check for released buttons when the pointer returns to a
// Gecko window.
void
nsWindow::DispatchMissedButtonReleases(GdkEventCrossing *aGdkEvent)
{
    guint changed = aGdkEvent->state ^ gButtonState;
    // Only consider button releases.
    // (Ignore button presses that occurred outside Gecko.)
    guint released = changed & gButtonState;
    gButtonState = aGdkEvent->state;

    // Loop over each button, excluding mouse wheel buttons 4 and 5 for which
    // GDK ignores releases.
    for (guint buttonMask = GDK_BUTTON1_MASK;
         buttonMask <= GDK_BUTTON3_MASK;
         buttonMask <<= 1) {

        if (released & buttonMask) {
            int16_t buttonType;
            switch (buttonMask) {
            case GDK_BUTTON1_MASK:
                buttonType = WidgetMouseEvent::eLeftButton;
                break;
            case GDK_BUTTON2_MASK:
                buttonType = WidgetMouseEvent::eMiddleButton;
                break;
            default:
                NS_ASSERTION(buttonMask == GDK_BUTTON3_MASK,
                             "Unexpected button mask");
                buttonType = WidgetMouseEvent::eRightButton;
            }

            LOG(("Synthesized button %u release on %p\n",
                 guint(buttonType + 1), (void *)this));

            // Dispatch a synthesized button up event to tell Gecko about the
            // change in state.  This event is marked as synthesized so that
            // it is not dispatched as a DOM event, because we don't know the
            // position, widget, modifiers, or time/order.
            WidgetMouseEvent synthEvent(true, NS_MOUSE_BUTTON_UP, this,
                                        WidgetMouseEvent::eSynthesized);
            synthEvent.button = buttonType;
            nsEventStatus status;
            DispatchEvent(&synthEvent, status);
        }
    }
}

void
nsWindow::InitButtonEvent(WidgetMouseEvent& aEvent,
                          GdkEventButton* aGdkEvent)
{
    // XXX see OnScrollEvent()
    if (aGdkEvent->window == mGdkWindow) {
        aEvent.refPoint.x = nscoord(aGdkEvent->x);
        aEvent.refPoint.y = nscoord(aGdkEvent->y);
    } else {
        LayoutDeviceIntPoint point(NSToIntFloor(aGdkEvent->x_root),
                                   NSToIntFloor(aGdkEvent->y_root));
        aEvent.refPoint = point -
            LayoutDeviceIntPoint::FromUntyped(WidgetToScreenOffset());
    }

    guint modifierState = aGdkEvent->state;
    // aEvent's state doesn't include this event's information.  Therefore,
    // if aEvent is mouse button down event, we need to set it manually.
    // Note that we cannot do same thing for NS_MOUSE_BUTTON_UP because
    // system may have two or more mice and same button of another mouse
    // may be still pressed.
    if (aGdkEvent->type != GDK_BUTTON_RELEASE) {
        switch (aGdkEvent->button) {
            case 1:
                modifierState |= GDK_BUTTON1_MASK;
                break;
            case 2:
                modifierState |= GDK_BUTTON2_MASK;
                break;
            case 3:
                modifierState |= GDK_BUTTON3_MASK;
                break;
        }
    }

    KeymapWrapper::InitInputEvent(aEvent, modifierState);

    aEvent.time = aGdkEvent->time;

    switch (aGdkEvent->type) {
    case GDK_2BUTTON_PRESS:
        aEvent.clickCount = 2;
        break;
    case GDK_3BUTTON_PRESS:
        aEvent.clickCount = 3;
        break;
        // default is one click
    default:
        aEvent.clickCount = 1;
    }
}

static guint ButtonMaskFromGDKButton(guint button)
{
    return GDK_BUTTON1_MASK << (button - 1);
}

void
nsWindow::OnButtonPressEvent(GdkEventButton *aEvent)
{
    LOG(("Button %u press on %p\n", aEvent->button, (void *)this));

    nsEventStatus status;

    // If you double click in GDK, it will actually generate a second
    // GDK_BUTTON_PRESS before sending the GDK_2BUTTON_PRESS, and this is
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

    nsWindow *containerWindow = GetContainerWindow();
    if (!gFocusWindow && containerWindow) {
        containerWindow->DispatchActivateEvent();
    }

    // check to see if we should rollup
    if (CheckForRollup(aEvent->x_root, aEvent->y_root, false, false))
        return;

    gdouble pressure = 0;
    gdk_event_get_axis ((GdkEvent*)aEvent, GDK_AXIS_PRESSURE, &pressure);
    mLastMotionPressure = pressure;

    uint16_t domButton;
    switch (aEvent->button) {
    case 1:
        domButton = WidgetMouseEvent::eLeftButton;
        break;
    case 2:
        domButton = WidgetMouseEvent::eMiddleButton;
        break;
    case 3:
        domButton = WidgetMouseEvent::eRightButton;
        break;
    // These are mapped to horizontal scroll
    case 6:
    case 7:
        NS_WARNING("We're not supporting legacy horizontal scroll event");
        return;
    // Map buttons 8-9 to back/forward
    case 8:
        DispatchCommandEvent(nsGkAtoms::Back);
        return;
    case 9:
        DispatchCommandEvent(nsGkAtoms::Forward);
        return;
    default:
        return;
    }

    gButtonState |= ButtonMaskFromGDKButton(aEvent->button);

    WidgetMouseEvent event(true, NS_MOUSE_BUTTON_DOWN, this,
                           WidgetMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent);
    event.pressure = mLastMotionPressure;

    DispatchEvent(&event, status);

    // right menu click on linux should also pop up a context menu
    if (domButton == WidgetMouseEvent::eRightButton &&
        MOZ_LIKELY(!mIsDestroyed)) {
        WidgetMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                          WidgetMouseEvent::eReal);
        InitButtonEvent(contextMenuEvent, aEvent);
        contextMenuEvent.pressure = mLastMotionPressure;
        DispatchEvent(&contextMenuEvent, status);
    }
}

void
nsWindow::OnButtonReleaseEvent(GdkEventButton *aEvent)
{
    LOG(("Button %u release on %p\n", aEvent->button, (void *)this));

    uint16_t domButton;
    switch (aEvent->button) {
    case 1:
        domButton = WidgetMouseEvent::eLeftButton;
        break;
    case 2:
        domButton = WidgetMouseEvent::eMiddleButton;
        break;
    case 3:
        domButton = WidgetMouseEvent::eRightButton;
        break;
    default:
        return;
    }

    gButtonState &= ~ButtonMaskFromGDKButton(aEvent->button);

    WidgetMouseEvent event(true, NS_MOUSE_BUTTON_UP, this,
                           WidgetMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent);
    gdouble pressure = 0;
    gdk_event_get_axis ((GdkEvent*)aEvent, GDK_AXIS_PRESSURE, &pressure);
    event.pressure = pressure ? pressure : mLastMotionPressure;

    nsEventStatus status;
    DispatchEvent(&event, status);
    mLastMotionPressure = pressure;
}

void
nsWindow::OnContainerFocusInEvent(GdkEventFocus *aEvent)
{
    NS_ASSERTION(mWindowType != eWindowType_popup,
                 "Unexpected focus on a popup window");

    LOGFOCUS(("OnContainerFocusInEvent [%p]\n", (void *)this));

    // Unset the urgency hint, if possible
    GtkWidget* top_window = GetToplevelWidget();
    if (top_window && (gtk_widget_get_visible(top_window)))
        SetUrgencyHint(top_window, false);

    // Return if being called within SetFocus because the focus manager
    // already knows that the window is active.
    if (gBlockActivateEvent) {
        LOGFOCUS(("activated notification is blocked [%p]\n", (void *)this));
        return;
    }

    // If keyboard input will be accepted, the focus manager will call
    // SetFocus to set the correct window.
    gFocusWindow = nullptr;

    DispatchActivateEvent();

    if (!gFocusWindow) {
        // We don't really have a window for dispatching key events, but
        // setting a non-NULL value here prevents OnButtonPressEvent() from
        // dispatching an activation notification if the widget is already
        // active.
        gFocusWindow = this;
    }

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
}

void
nsWindow::OnContainerFocusOutEvent(GdkEventFocus *aEvent)
{
    LOGFOCUS(("OnContainerFocusOutEvent [%p]\n", (void *)this));

    if (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog) {
        nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
        nsCOMPtr<nsIDragSession> dragSession;
        dragService->GetCurrentSession(getter_AddRefs(dragSession));

        // Rollup popups when a window is focused out unless a drag is occurring.
        // This check is because drags grab the keyboard and cause a focus out on
        // versions of GTK before 2.18.
        bool shouldRollup = !dragSession;
        if (!shouldRollup) {
            // we also roll up when a drag is from a different application
            nsCOMPtr<nsIDOMNode> sourceNode;
            dragSession->GetSourceNode(getter_AddRefs(sourceNode));
            shouldRollup = (sourceNode == nullptr);
        }

        if (shouldRollup) {
            CheckForRollup(0, 0, false, true);
        }
    }

#if (MOZ_WIDGET_GTK == 2) && defined(MOZ_X11)
    // plugin lose focus
    if (gPluginFocusWindow) {
        nsRefPtr<nsWindow> kungFuDeathGrip = gPluginFocusWindow;
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }
#endif /* MOZ_X11 && MOZ_WIDGET_GTK2 */

    if (gFocusWindow) {
        nsRefPtr<nsWindow> kungFuDeathGrip = gFocusWindow;
        if (gFocusWindow->mIMModule) {
            gFocusWindow->mIMModule->OnBlurWindow(gFocusWindow);
        }
        gFocusWindow = nullptr;
    }

    DispatchDeactivateEvent();

    LOGFOCUS(("Done with container focus out [%p]\n", (void *)this));
}

bool
nsWindow::DispatchCommandEvent(nsIAtom* aCommand)
{
    nsEventStatus status;
    WidgetCommandEvent event(true, nsGkAtoms::onAppCommand, aCommand, this);
    DispatchEvent(&event, status);
    return TRUE;
}

bool
nsWindow::DispatchContentCommandEvent(int32_t aMsg)
{
  nsEventStatus status;
  WidgetContentCommandEvent event(true, aMsg, this);
  DispatchEvent(&event, status);
  return TRUE;
}

static bool
IsCtrlAltTab(GdkEventKey *aEvent)
{
    return aEvent->keyval == GDK_Tab &&
        KeymapWrapper::AreModifiersActive(
            KeymapWrapper::CTRL | KeymapWrapper::ALT, aEvent->state);
}

bool
nsWindow::DispatchKeyDownEvent(GdkEventKey *aEvent, bool *aCancelled)
{
    NS_PRECONDITION(aCancelled, "aCancelled must not be null");

    *aCancelled = false;

    if (IsCtrlAltTab(aEvent)) {
        return false;
    }

    // send the key down event
    nsEventStatus status;
    WidgetKeyboardEvent downEvent(true, NS_KEY_DOWN, this);
    KeymapWrapper::InitKeyEvent(downEvent, aEvent);
    DispatchEvent(&downEvent, status);
    *aCancelled = (status == nsEventStatus_eConsumeNoDefault);
    return true;
}

gboolean
nsWindow::OnKeyPressEvent(GdkEventKey *aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));

    // if we are in the middle of composing text, XIM gets to see it
    // before mozilla does.
    bool IMEWasEnabled = false;
    if (mIMModule) {
        IMEWasEnabled = mIMModule->IsEnabled();
        if (mIMModule->OnKeyEvent(this, aEvent)) {
            return TRUE;
        }
    }

    nsEventStatus status;

    // work around for annoying things.
    if (IsCtrlAltTab(aEvent)) {
        return TRUE;
    }

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

    // Dispatch keydown event always.  At auto repeating, we should send
    // KEYDOWN -> KEYPRESS -> KEYDOWN -> KEYPRESS ... -> KEYUP
    // However, old distributions (e.g., Ubuntu 9.10) sent native key
    // release event, so, on such platform, the DOM events will be:
    // KEYDOWN -> KEYPRESS -> KEYUP -> KEYDOWN -> KEYPRESS -> KEYUP...

    bool isKeyDownCancelled = false;
    if (DispatchKeyDownEvent(aEvent, &isKeyDownCancelled) &&
        (MOZ_UNLIKELY(mIsDestroyed) || isKeyDownCancelled)) {
        return TRUE;
    }

    // If a keydown event handler causes to enable IME, i.e., it moves
    // focus from IME unusable content to IME usable editor, we should
    // send the native key event to IME for the first input on the editor.
    if (!IMEWasEnabled && mIMModule && mIMModule->IsEnabled()) {
        // Notice our keydown event was already dispatched.  This prevents
        // unnecessary DOM keydown event in the editor.
        if (mIMModule->OnKeyEvent(this, aEvent, true)) {
            return TRUE;
        }
    }

    // Don't pass modifiers as NS_KEY_PRESS events.
    // TODO: Instead of selectively excluding some keys from NS_KEY_PRESS events,
    //       we should instead selectively include (as per MSDN spec; no official
    //       spec covers KeyPress events).
    if (!KeymapWrapper::IsKeyPressEventNecessary(aEvent)) {
        return TRUE;
    }

#ifdef MOZ_X11
#if ! defined AIX // no XFree86 on AIX 5L
    // Look for specialized app-command keys
    switch (aEvent->keyval) {
        case XF86XK_Back:
            return DispatchCommandEvent(nsGkAtoms::Back);
        case XF86XK_Forward:
            return DispatchCommandEvent(nsGkAtoms::Forward);
        case XF86XK_Refresh:
            return DispatchCommandEvent(nsGkAtoms::Reload);
        case XF86XK_Stop:
            return DispatchCommandEvent(nsGkAtoms::Stop);
        case XF86XK_Search:
            return DispatchCommandEvent(nsGkAtoms::Search);
        case XF86XK_Favorites:
            return DispatchCommandEvent(nsGkAtoms::Bookmarks);
        case XF86XK_HomePage:
            return DispatchCommandEvent(nsGkAtoms::Home);
        case XF86XK_Copy:
        case GDK_F16:  // F16, F20, F18, F14 are old keysyms for Copy Cut Paste Undo
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_COPY);
        case XF86XK_Cut:
        case GDK_F20:
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_CUT);
        case XF86XK_Paste:
        case GDK_F18:
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_PASTE);
        case GDK_Redo:
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_REDO);
        case GDK_Undo:
        case GDK_F14:
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_UNDO);
    }
#endif /* ! AIX */
#endif /* MOZ_X11 */

    WidgetKeyboardEvent event(true, NS_KEY_PRESS, this);
    KeymapWrapper::InitKeyEvent(event, aEvent);

    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (is_context_menu_key(event)) {
        WidgetMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                          WidgetMouseEvent::eReal,
                                          WidgetMouseEvent::eContextMenuKey);

        contextMenuEvent.refPoint = LayoutDeviceIntPoint(0, 0);
        contextMenuEvent.time = aEvent->time;
        contextMenuEvent.clickCount = 1;
        KeymapWrapper::InitInputEvent(contextMenuEvent, aEvent->state);
        DispatchEvent(&contextMenuEvent, status);
    }
    else {
        // If the character code is in the BMP, send the key press event.
        // Otherwise, send a text event with the equivalent UTF-16 string.
        if (IS_IN_BMP(event.charCode)) {
            DispatchEvent(&event, status);
        }
        else {
            WidgetTextEvent textEvent(true, NS_TEXT_TEXT, this);
            PRUnichar textString[3];
            textString[0] = H_SURROGATE(event.charCode);
            textString[1] = L_SURROGATE(event.charCode);
            textString[2] = 0;
            textEvent.theText = textString;
            textEvent.time = event.time;
            DispatchEvent(&textEvent, status);
        }
    }

    // If the event was consumed, return.
    if (status == nsEventStatus_eConsumeNoDefault) {
        return TRUE;
    }

    return FALSE;
}

gboolean
nsWindow::OnKeyReleaseEvent(GdkEventKey *aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));

    if (mIMModule && mIMModule->OnKeyEvent(this, aEvent)) {
        return TRUE;
    }

    // send the key event as a key up event
    WidgetKeyboardEvent event(true, NS_KEY_UP, this);
    KeymapWrapper::InitKeyEvent(event, aEvent);

    nsEventStatus status;
    DispatchEvent(&event, status);

    // If the event was consumed, return.
    if (status == nsEventStatus_eConsumeNoDefault) {
        return TRUE;
    }

    return FALSE;
}

void
nsWindow::OnScrollEvent(GdkEventScroll *aEvent)
{
    // check to see if we should rollup
    if (CheckForRollup(aEvent->x_root, aEvent->y_root, true, false))
        return;

    WheelEvent wheelEvent(true, NS_WHEEL_WHEEL, this);
    wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;
    switch (aEvent->direction) {
    case GDK_SCROLL_UP:
        wheelEvent.deltaY = wheelEvent.lineOrPageDeltaY = -3;
        break;
    case GDK_SCROLL_DOWN:
        wheelEvent.deltaY = wheelEvent.lineOrPageDeltaY = 3;
        break;
    case GDK_SCROLL_LEFT:
        wheelEvent.deltaX = wheelEvent.lineOrPageDeltaX = -1;
        break;
    case GDK_SCROLL_RIGHT:
        wheelEvent.deltaX = wheelEvent.lineOrPageDeltaX = 1;
        break;
    }

    NS_ASSERTION(wheelEvent.deltaX || wheelEvent.deltaY,
                 "deltaX or deltaY must be non-zero");

    if (aEvent->window == mGdkWindow) {
        // we are the window that the event happened on so no need for expensive WidgetToScreenOffset
        wheelEvent.refPoint.x = nscoord(aEvent->x);
        wheelEvent.refPoint.y = nscoord(aEvent->y);
    } else {
        // XXX we're never quite sure which GdkWindow the event came from due to our custom bubbling
        // in scroll_event_cb(), so use ScreenToWidget to translate the screen root coordinates into
        // coordinates relative to this widget.
        LayoutDeviceIntPoint point(NSToIntFloor(aEvent->x_root),
                                   NSToIntFloor(aEvent->y_root));
        wheelEvent.refPoint = point -
            LayoutDeviceIntPoint::FromUntyped(WidgetToScreenOffset());
    }

    KeymapWrapper::InitInputEvent(wheelEvent, aEvent->state);

    wheelEvent.time = aEvent->time;

    nsEventStatus status;
    DispatchEvent(&wheelEvent, status);
}

void
nsWindow::OnVisibilityNotifyEvent(GdkEventVisibility *aEvent)
{
    LOGDRAW(("Visibility event %i on [%p] %p\n",
             aEvent->state, this, aEvent->window));

    if (!mGdkWindow)
        return;

    switch (aEvent->state) {
    case GDK_VISIBILITY_UNOBSCURED:
    case GDK_VISIBILITY_PARTIAL:
        if (mIsFullyObscured && mHasMappedToplevel) {
            // GDK_EXPOSE events have been ignored, so make sure GDK
            // doesn't think that the window has already been painted.
            gdk_window_invalidate_rect(mGdkWindow, NULL, FALSE);
        }

        mIsFullyObscured = false;

        if (!nsGtkIMModule::IsVirtualKeyboardOpened()) {
            // if we have to retry the grab, retry it.
            EnsureGrabs();
        }
        break;
    default: // includes GDK_VISIBILITY_FULLY_OBSCURED
        mIsFullyObscured = true;
        break;
    }
}

void
nsWindow::OnWindowStateEvent(GtkWidget *aWidget, GdkEventWindowState *aEvent)
{
    LOG(("nsWindow::OnWindowStateEvent [%p] changed %d new_window_state %d\n",
         (void *)this, aEvent->changed_mask, aEvent->new_window_state));

    if (IS_MOZ_CONTAINER(aWidget)) {
        // This event is notifying the container widget of changes to the
        // toplevel window.  Just detect changes affecting whether windows are
        // viewable.
        //
        // (A visibility notify event is sent to each window that becomes
        // viewable when the toplevel is mapped, but we can't rely on that for
        // setting mHasMappedToplevel because these toplevel window state
        // events are asynchronous.  The windows in the hierarchy now may not
        // be the same windows as when the toplevel was mapped, so they may
        // not get VisibilityNotify events.)
        bool mapped =
            !(aEvent->new_window_state &
              (GDK_WINDOW_STATE_ICONIFIED|GDK_WINDOW_STATE_WITHDRAWN));
        if (mHasMappedToplevel != mapped) {
            SetHasMappedToplevel(mapped);
        }
        return;
    }
    // else the widget is a shell widget.

    // We don't care about anything but changes in the maximized/icon/fullscreen
    // states
    if ((aEvent->changed_mask
         & (GDK_WINDOW_STATE_ICONIFIED |
            GDK_WINDOW_STATE_MAXIMIZED |
            GDK_WINDOW_STATE_FULLSCREEN)) == 0) {
        return;
    }

    if (aEvent->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
        LOG(("\tIconified\n"));
        mSizeState = nsSizeMode_Minimized;
#ifdef ACCESSIBILITY
        DispatchMinimizeEventAccessible();
#endif //ACCESSIBILITY
    }
    else if (aEvent->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
        LOG(("\tFullscreen\n"));
        mSizeState = nsSizeMode_Fullscreen;
    }
    else if (aEvent->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
        LOG(("\tMaximized\n"));
        mSizeState = nsSizeMode_Maximized;
#ifdef ACCESSIBILITY
        DispatchMaximizeEventAccessible();
#endif //ACCESSIBILITY
    }
    else {
        LOG(("\tNormal\n"));
        mSizeState = nsSizeMode_Normal;
#ifdef ACCESSIBILITY
        DispatchRestoreEventAccessible();
#endif //ACCESSIBILITY
    }

    if (mWidgetListener)
      mWidgetListener->SizeModeChanged(mSizeState);
}

void
nsWindow::ThemeChanged()
{
    NotifyThemeChanged();

    if (!mGdkWindow || MOZ_UNLIKELY(mIsDestroyed))
        return;

    // Dispatch theme change notification to all child windows
    GList *children =
        gdk_window_peek_children(mGdkWindow);
    while (children) {
        GdkWindow *gdkWin = GDK_WINDOW(children->data);

        nsWindow *win = (nsWindow*) g_object_get_data(G_OBJECT(gdkWin),
                                                      "nsWindow");

        if (win && win != this) { // guard against infinite recursion
            nsRefPtr<nsWindow> kungFuDeathGrip = win;
            win->ThemeChanged();
        }

        children = children->next;
    }
}

void
nsWindow::DispatchDragEvent(uint32_t aMsg, const nsIntPoint& aRefPoint,
                            guint aTime)
{
    WidgetDragEvent event(true, aMsg, this);

    if (aMsg == NS_DRAGDROP_OVER) {
        InitDragEvent(event);
    }

    event.refPoint = LayoutDeviceIntPoint::FromUntyped(aRefPoint);
    event.time = aTime;

    nsEventStatus status;
    DispatchEvent(&event, status);
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
    LOGDRAG(("nsWindow::OnDragDataReceived(%p)\n", (void*)this));

    nsDragService::GetInstance()->
        TargetDataReceived(aWidget, aDragContext, aX, aY,
                           aSelectionData, aInfo, aTime);
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

static GdkWindow *
CreateGdkWindow(GdkWindow *parent, GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint          attributes_mask = GDK_WA_VISUAL;

    attributes.event_mask = kEvents;

    attributes.width = 1;
    attributes.height = 1;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.window_type = GDK_WINDOW_CHILD;

#if (MOZ_WIDGET_GTK == 2)
    attributes_mask |= GDK_WA_COLORMAP;
    attributes.colormap = gtk_widget_get_colormap(widget);
#endif

    GdkWindow *window = gdk_window_new(parent, &attributes, attributes_mask);
    gdk_window_set_user_data(window, widget);

// GTK3 TODO?
#if (MOZ_WIDGET_GTK == 2)
    /* set the default pixmap to None so that you don't end up with the
       gtk default which is BlackPixel. */
    gdk_window_set_back_pixmap(window, NULL, FALSE);
#endif

    return window;
}

nsresult
nsWindow::Create(nsIWidget        *aParent,
                 nsNativeWidget    aNativeParent,
                 const nsIntRect  &aRect,
                 nsDeviceContext *aContext,
                 nsWidgetInitData *aInitData)
{
    // only set the base parent if we're going to be a dialog or a
    // toplevel
    nsIWidget *baseParent = aInitData &&
        (aInitData->mWindowType == eWindowType_dialog ||
         aInitData->mWindowType == eWindowType_toplevel ||
         aInitData->mWindowType == eWindowType_invisible) ?
        nullptr : aParent;

#ifdef ACCESSIBILITY
    // Send a DBus message to check whether a11y is enabled
    a11y::PreInit();
#endif

    // Ensure that the toolkit is created.
    nsGTKToolkit::GetToolkit();

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aContext, aInitData);

    // Do we need to listen for resizes?
    bool listenForResizes = false;;
    if (aNativeParent || (aInitData && aInitData->mListenForResizes))
        listenForResizes = true;

    // and do our common creation
    CommonCreate(aParent, listenForResizes);

    // save our bounds
    mBounds = aRect;
    ConstrainSize(&mBounds.width, &mBounds.height);

    // figure out our parent window
    GtkWidget      *parentMozContainer = nullptr;
    GtkContainer   *parentGtkContainer = nullptr;
    GdkWindow      *parentGdkWindow = nullptr;
    GtkWindow      *topLevelParent = nullptr;
    nsWindow       *parentnsWindow = nullptr;
    GtkWidget      *eventWidget = nullptr;

    if (aParent) {
        parentnsWindow = static_cast<nsWindow*>(aParent);
        parentGdkWindow = parentnsWindow->mGdkWindow;
    } else if (aNativeParent && GDK_IS_WINDOW(aNativeParent)) {
        parentGdkWindow = GDK_WINDOW(aNativeParent);
        parentnsWindow = get_window_for_gdk_window(parentGdkWindow);
        if (!parentnsWindow)
            return NS_ERROR_FAILURE;

    } else if (aNativeParent && GTK_IS_CONTAINER(aNativeParent)) {
        parentGtkContainer = GTK_CONTAINER(aNativeParent);
    }

    if (parentGdkWindow) {
        // get the widget for the window - it should be a moz container
        parentMozContainer = parentnsWindow->GetMozContainerWidget();
        if (!parentMozContainer)
            return NS_ERROR_FAILURE;

        // get the toplevel window just in case someone needs to use it
        // for setting transients or whatever.
        topLevelParent =
            GTK_WINDOW(gtk_widget_get_toplevel(parentMozContainer));
    }

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
    case eWindowType_popup:
    case eWindowType_toplevel:
    case eWindowType_invisible: {
        mIsTopLevel = true;

        // We only move a general managed toplevel window if someone has
        // actually placed the window somewhere.  If no placement has taken
        // place, we just let the window manager Do The Right Thing.
        //
        // Indicate that if we're shown, we at least need to have our size set.
        // If we get explicitly moved, the position will also be set.
        mNeedsResize = true;

        if (mWindowType == eWindowType_dialog) {
            mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            SetDefaultIcon();
            gtk_window_set_wmclass(GTK_WINDOW(mShell), "Dialog", 
                                   gdk_get_program_class());
            gtk_window_set_type_hint(GTK_WINDOW(mShell),
                                     GDK_WINDOW_TYPE_HINT_DIALOG);
            gtk_window_set_transient_for(GTK_WINDOW(mShell),
                                         topLevelParent);
        }
        else if (mWindowType == eWindowType_popup) {
            // With popup windows, we want to control their position, so don't
            // wait for the window manager to place them (which wouldn't
            // happen with override-redirect windows anyway).
            mNeedsMove = true;

            // Popups that are not noautohide are only temporary. The are used
            // for menus and the like and disappear when another window is used.
            // For most popups, use the standard GtkWindowType GTK_WINDOW_POPUP,
            // which will use a Window with the override-redirect attribute
            // (for temporary windows).
            // For long-lived windows, their stacking order is managed by the
            // window manager, as indicated by GTK_WINDOW_TOPLEVEL ...
            GtkWindowType type = aInitData->mNoAutoHide ?
                                     GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP;
            mShell = gtk_window_new(type);
            gtk_window_set_wmclass(GTK_WINDOW(mShell), "Popup",
                                   gdk_get_program_class());

            if (aInitData->mSupportTranslucency) {
                // We need to select an ARGB visual here instead of in
                // SetTransparencyMode() because it has to be done before the
                // widget is realized.  An ARGB visual is only useful if we
                // are on a compositing window manager.
                GdkScreen *screen = gtk_widget_get_screen(mShell);
                if (gdk_screen_is_composited(screen)) {
#if (MOZ_WIDGET_GTK == 2)
                    GdkColormap *colormap =
                        gdk_screen_get_rgba_colormap(screen);
                    gtk_widget_set_colormap(mShell, colormap);
#else
                    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
                    gtk_widget_set_visual(mShell, visual);
#endif
                }
            }
            if (aInitData->mNoAutoHide) {
                // ... but the window manager does not decorate this window,
                // nor provide a separate taskbar icon.
                if (mBorderStyle == eBorderStyle_default) {
                  gtk_window_set_decorated(GTK_WINDOW(mShell), FALSE);
                }
                else {
                  bool decorate = mBorderStyle & eBorderStyle_title;
                  gtk_window_set_decorated(GTK_WINDOW(mShell), decorate);
                  if (decorate) {
                    gtk_window_set_deletable(GTK_WINDOW(mShell), mBorderStyle & eBorderStyle_close);
                  }
                }
                gtk_window_set_skip_taskbar_hint(GTK_WINDOW(mShell), TRUE);
                // Element focus is managed by the parent window so the
                // WM_HINTS input field is set to False to tell the window
                // manager not to set input focus to this window ...
                gtk_window_set_accept_focus(GTK_WINDOW(mShell), FALSE);
#ifdef MOZ_X11
                // ... but when the window manager offers focus through
                // WM_TAKE_FOCUS, focus is requested on the parent window.
                gtk_widget_realize(mShell);
                gdk_window_add_filter(gtk_widget_get_window(mShell),
                                      popup_take_focus_filter, NULL); 
#endif
            }

            GdkWindowTypeHint gtkTypeHint;
            if (aInitData->mIsDragPopup) {
                gtkTypeHint = GDK_WINDOW_TYPE_HINT_DND;
            }
            else {
                switch (aInitData->mPopupHint) {
                    case ePopupTypeMenu:
                        gtkTypeHint = GDK_WINDOW_TYPE_HINT_POPUP_MENU;
                        break;
                    case ePopupTypeTooltip:
                        gtkTypeHint = GDK_WINDOW_TYPE_HINT_TOOLTIP;
                        break;
                    default:
                        gtkTypeHint = GDK_WINDOW_TYPE_HINT_UTILITY;
                        break;
                }
            }
            gtk_window_set_type_hint(GTK_WINDOW(mShell), gtkTypeHint);

            if (topLevelParent) {
                gtk_window_set_transient_for(GTK_WINDOW(mShell),
                                            topLevelParent);
            }
        }
        else { // must be eWindowType_toplevel
            mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            SetDefaultIcon();
            gtk_window_set_wmclass(GTK_WINDOW(mShell), "Toplevel", 
                                   gdk_get_program_class());

            // each toplevel window gets its own window group
            GtkWindowGroup *group = gtk_window_group_new();
            gtk_window_group_add_window(group, GTK_WINDOW(mShell));
            g_object_unref(group);
        }

        // Prevent GtkWindow from painting a background to flicker.
        gtk_widget_set_app_paintable(mShell, TRUE);

        // Create a container to hold child windows and child GtkWidgets.
        GtkWidget *container = moz_container_new();
        mContainer = MOZ_CONTAINER(container);
        // Use mShell's window for drawing and events.
        gtk_widget_set_has_window(container, FALSE);
        eventWidget = mShell;
        gtk_widget_add_events(eventWidget, kEvents);
        gtk_container_add(GTK_CONTAINER(mShell), container);
        gtk_widget_realize(container);

        // make sure this is the focus widget in the container
        gtk_widget_show(container);
        gtk_widget_grab_focus(container);

        // the drawing window
        mGdkWindow = gtk_widget_get_window(mShell);

        if (mWindowType == eWindowType_popup) {
            // gdk does not automatically set the cursor for "temporary"
            // windows, which are what gtk uses for popups.

            mCursor = eCursor_wait; // force SetCursor to actually set the
                                    // cursor, even though our internal state
                                    // indicates that we already have the
                                    // standard cursor.
            SetCursor(eCursor_standard);

            if (aInitData->mNoAutoHide) {
                gint wmd = ConvertBorderStyles(mBorderStyle);
                if (wmd != -1)
                  gdk_window_set_decorations(gtk_widget_get_window(mShell), (GdkWMDecoration) wmd);
            }
        }
    }
        break;
    case eWindowType_plugin:
    case eWindowType_child: {
        if (parentMozContainer) {
            mGdkWindow = CreateGdkWindow(parentGdkWindow, parentMozContainer);
            mHasMappedToplevel = parentnsWindow->mHasMappedToplevel;
        }
        else if (parentGtkContainer) {
            // This MozContainer has its own window for drawing and receives
            // events because there is no mShell widget (corresponding to this
            // nsWindow).
            GtkWidget *container = moz_container_new();
            mContainer = MOZ_CONTAINER(container);
            eventWidget = container;
            gtk_widget_add_events(eventWidget, kEvents);
            gtk_container_add(parentGtkContainer, container);
            gtk_widget_realize(container);

            mGdkWindow = gtk_widget_get_window(container);
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

    // label the drawing window with this object so we can find our way home
    g_object_set_data(G_OBJECT(mGdkWindow), "nsWindow", this);

    if (mContainer)
        g_object_set_data(G_OBJECT(mContainer), "nsWindow", this);

    if (mShell)
        g_object_set_data(G_OBJECT(mShell), "nsWindow", this);

    // attach listeners for events
    if (mShell) {
        g_signal_connect(mShell, "configure_event",
                         G_CALLBACK(configure_event_cb), NULL);
        g_signal_connect(mShell, "delete_event",
                         G_CALLBACK(delete_event_cb), NULL);
        g_signal_connect(mShell, "window_state_event",
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
        // Widget signals
        g_signal_connect(mContainer, "unrealize",
                         G_CALLBACK(container_unrealize_cb), NULL);
        g_signal_connect_after(mContainer, "size_allocate",
                               G_CALLBACK(size_allocate_cb), NULL);
        g_signal_connect(mContainer, "hierarchy-changed",
                         G_CALLBACK(hierarchy_changed_cb), NULL);
        // Initialize mHasMappedToplevel.
        hierarchy_changed_cb(GTK_WIDGET(mContainer), NULL);
        // Expose, focus, key, and drag events are sent even to GTK_NO_WINDOW
        // widgets.
#if (MOZ_WIDGET_GTK == 2)
        g_signal_connect(mContainer, "expose_event",
                         G_CALLBACK(expose_event_cb), NULL);
#else
        g_signal_connect(G_OBJECT(mContainer), "draw",
                         G_CALLBACK(expose_event_cb), NULL);
#endif
        g_signal_connect(mContainer, "focus_in_event",
                         G_CALLBACK(focus_in_event_cb), NULL);
        g_signal_connect(mContainer, "focus_out_event",
                         G_CALLBACK(focus_out_event_cb), NULL);
        g_signal_connect(mContainer, "key_press_event",
                         G_CALLBACK(key_press_event_cb), NULL);
        g_signal_connect(mContainer, "key_release_event",
                         G_CALLBACK(key_release_event_cb), NULL);

        gtk_drag_dest_set((GtkWidget *)mContainer,
                          (GtkDestDefaults)0,
                          NULL,
                          0,
                          (GdkDragAction)0);

        g_signal_connect(mContainer, "drag_motion",
                         G_CALLBACK(drag_motion_event_cb), NULL);
        g_signal_connect(mContainer, "drag_leave",
                         G_CALLBACK(drag_leave_event_cb), NULL);
        g_signal_connect(mContainer, "drag_drop",
                         G_CALLBACK(drag_drop_event_cb), NULL);
        g_signal_connect(mContainer, "drag_data_received",
                         G_CALLBACK(drag_data_received_event_cb), NULL);

        GtkWidget *widgets[] = { GTK_WIDGET(mContainer), mShell };
        for (size_t i = 0; i < ArrayLength(widgets) && widgets[i]; ++i) {
            // Visibility events are sent to the owning widget of the relevant
            // window but do not propagate to parent widgets so connect on
            // mShell (if it exists) as well as mContainer.
            g_signal_connect(widgets[i], "visibility-notify-event",
                             G_CALLBACK(visibility_notify_event_cb), NULL);
            // Similarly double buffering is controlled by the window's owning
            // widget.  Disable double buffering for painting directly to the
            // X Window.
            gtk_widget_set_double_buffered(widgets[i], FALSE);
        }

        // We create input contexts for all containers, except for
        // toplevel popup windows
        if (mWindowType != eWindowType_popup) {
            mIMModule = new nsGtkIMModule(this);
        }
    } else if (!mIMModule) {
        nsWindow *container = GetContainerWindow();
        if (container) {
            mIMModule = container->mIMModule;
        }
    }

    if (eventWidget) {
#if (MOZ_WIDGET_GTK == 2)
        // Don't let GTK mess with the shapes of our GdkWindows
        GTK_PRIVATE_SET_FLAG(eventWidget, GTK_HAS_SHAPE_MASK);
#endif

        // These events are sent to the owning widget of the relevant window
        // and propagate up to the first widget that handles the events, so we
        // need only connect on mShell, if it exists, to catch events on its
        // window and windows of mContainer.
        g_signal_connect(eventWidget, "enter-notify-event",
                         G_CALLBACK(enter_notify_event_cb), NULL);
        g_signal_connect(eventWidget, "leave-notify-event",
                         G_CALLBACK(leave_notify_event_cb), NULL);
        g_signal_connect(eventWidget, "motion-notify-event",
                         G_CALLBACK(motion_notify_event_cb), NULL);
        g_signal_connect(eventWidget, "button-press-event",
                         G_CALLBACK(button_press_event_cb), NULL);
        g_signal_connect(eventWidget, "button-release-event",
                         G_CALLBACK(button_release_event_cb), NULL);
        g_signal_connect(eventWidget, "scroll-event",
                         G_CALLBACK(scroll_event_cb), NULL);
    }

    LOG(("nsWindow [%p]\n", (void *)this));
    if (mShell) {
        LOG(("\tmShell %p mContainer %p mGdkWindow %p 0x%lx\n",
             mShell, mContainer, mGdkWindow,
             gdk_x11_window_get_xid(mGdkWindow)));
    } else if (mContainer) {
        LOG(("\tmContainer %p mGdkWindow %p\n", mContainer, mGdkWindow));
    }
    else if (mGdkWindow) {
        LOG(("\tmGdkWindow %p parent %p\n",
             mGdkWindow, gdk_window_get_parent(mGdkWindow)));
    }

    // resize so that everything is set to the right dimensions
    if (!mIsTopLevel)
        Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
  if (!mShell)
    return NS_ERROR_FAILURE;

  const char *res_class = gdk_get_program_class();
  if (!res_class)
    return NS_ERROR_FAILURE;
  
  char *res_name = ToNewCString(xulWinType);
  if (!res_name)
    return NS_ERROR_OUT_OF_MEMORY;

  const char *role = NULL;

  // Parse res_name into a name and role. Characters other than
  // [A-Za-z0-9_-] are converted to '_'. Anything after the first
  // colon is assigned to role; if there's no colon, assign the
  // whole thing to both role and res_name.
  for (char *c = res_name; *c; c++) {
    if (':' == *c) {
      *c = 0;
      role = c + 1;
    }
    else if (!isascii(*c) || (!isalnum(*c) && ('_' != *c) && ('-' != *c)))
      *c = '_';
  }
  res_name[0] = toupper(res_name[0]);
  if (!role) role = res_name;

  GdkWindow *shellWindow = gtk_widget_get_window(GTK_WIDGET(mShell));
  gdk_window_set_role(shellWindow, role);

#ifdef MOZ_X11
  XClassHint *class_hint = XAllocClassHint();
  if (!class_hint) {
    nsMemory::Free(res_name);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  class_hint->res_name = res_name;
  class_hint->res_class = const_cast<char*>(res_class);

  // Can't use gtk_window_set_wmclass() for this; it prints
  // a warning & refuses to make the change.
  XSetClassHint(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                gdk_x11_window_get_xid(shellWindow),
                class_hint);
  XFree(class_hint);
#endif /* MOZ_X11 */

  nsMemory::Free(res_name);

  return NS_OK;
}

void
nsWindow::NativeResize(int32_t aWidth, int32_t aHeight, bool    aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));

    // clear our resize flag
    mNeedsResize = false;

    if (mIsTopLevel) {
        gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
    }
    else if (mContainer) {
        GtkWidget *widget = GTK_WIDGET(mContainer);
        GtkAllocation allocation, prev_allocation;
        gtk_widget_get_allocation(widget, &prev_allocation);
        allocation.x = prev_allocation.x;
        allocation.y = prev_allocation.y;
        allocation.width = aWidth;
        allocation.height = aHeight;
        gtk_widget_size_allocate(widget, &allocation);
    }
    else if (mGdkWindow) {
        gdk_window_resize(mGdkWindow, aWidth, aHeight);
    }
}

void
nsWindow::NativeResize(int32_t aX, int32_t aY,
                       int32_t aWidth, int32_t aHeight,
                       bool    aRepaint)
{
    mNeedsResize = false;
    mNeedsMove = false;

    LOG(("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));

    if (mIsTopLevel) {
        // aX and aY give the position of the window manager frame top-left.
        gtk_window_move(GTK_WINDOW(mShell), aX, aY);
        // This sets the client window size.
        gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
    }
    else if (mContainer) {
        GtkAllocation allocation;
        allocation.x = aX;
        allocation.y = aY;
        allocation.width = aWidth;
        allocation.height = aHeight;
        gtk_widget_size_allocate(GTK_WIDGET(mContainer), &allocation);
    }
    else if (mGdkWindow) {
        gdk_window_move_resize(mGdkWindow, aX, aY, aWidth, aHeight);
    }
}

void
nsWindow::NativeShow(bool aAction)
{
    if (aAction) {
        // unset our flag now that our window has been shown
        mNeedsShow = false;

        if (mIsTopLevel) {
            // Set up usertime/startupID metadata for the created window.
            if (mWindowType != eWindowType_invisible) {
                SetUserTimeAndStartupIDForActivatedWindow(mShell);
            }

            gtk_widget_show(mShell);
        }
        else if (mContainer) {
            gtk_widget_show(GTK_WIDGET(mContainer));
        }
        else if (mGdkWindow) {
            gdk_window_show_unraised(mGdkWindow);
        }
    }
    else {
        if (mIsTopLevel) {
            gtk_widget_hide(GTK_WIDGET(mShell));

            ClearTransparencyBitmap(); // Release some resources
        }
        else if (mContainer) {
            gtk_widget_hide(GTK_WIDGET(mContainer));
        }
        else if (mGdkWindow) {
            gdk_window_hide(mGdkWindow);
        }
    }
}

void
nsWindow::SetHasMappedToplevel(bool aState)
{
    // Even when aState == mHasMappedToplevel (as when this method is called
    // from Show()), child windows need to have their state checked, so don't
    // return early.
    bool oldState = mHasMappedToplevel;
    mHasMappedToplevel = aState;

    // mHasMappedToplevel is not updated for children of windows that are
    // hidden; GDK knows not to send expose events for these windows.  The
    // state is recorded on the hidden window itself, but, for child trees of
    // hidden windows, their state essentially becomes disconnected from their
    // hidden parent.  When the hidden parent gets shown, the child trees are
    // reconnected, and the state of the window being shown can be easily
    // propagated.
    if (!mIsShown || !mGdkWindow)
        return;

    if (aState && !oldState && !mIsFullyObscured) {
        // GDK_EXPOSE events have been ignored but the window is now visible,
        // so make sure GDK doesn't think that the window has already been
        // painted.
        gdk_window_invalidate_rect(mGdkWindow, NULL, FALSE);

        // Check that a grab didn't fail due to the window not being
        // viewable.
        EnsureGrabs();
    }

    for (GList *children = gdk_window_peek_children(mGdkWindow);
         children;
         children = children->next) {
        GdkWindow *gdkWin = GDK_WINDOW(children->data);
        nsWindow *child = get_window_for_gdk_window(gdkWin);

        if (child && child->mHasMappedToplevel != aState) {
            child->SetHasMappedToplevel(aState);
        }
    }
}

nsIntSize
nsWindow::GetSafeWindowSize(nsIntSize aSize)
{
    // The X protocol uses CARD32 for window sizes, but the server (1.11.3)
    // reads it as CARD16.  Sizes of pixmaps, used for drawing, are (unsigned)
    // CARD16 in the protocol, but the server's ProcCreatePixmap returns
    // BadAlloc if dimensions cannot be represented by signed shorts.
    nsIntSize result = aSize;
    const int32_t kInt16Max = 32767;
    if (result.width > kInt16Max) {
        result.width = kInt16Max;
    }
    if (result.height > kInt16Max) {
        result.height = kInt16Max;
    }
    return result;
}

void
nsWindow::EnsureGrabs(void)
{
    if (mRetryPointerGrab)
        GrabPointer(sRetryGrabTime);
}

void
nsWindow::CleanLayerManagerRecursive(void) {
    if (mLayerManager) {
        mLayerManager->Destroy();
        mLayerManager = nullptr;
    }

    DestroyCompositor();

    GList* children = gdk_window_peek_children(mGdkWindow);
    for (GList* list = children; list; list = list->next) {
        nsWindow* window = get_window_for_gdk_window(GDK_WINDOW(list->data));
        if (window) {
            window->CleanLayerManagerRecursive();
        }
    }
}

void
nsWindow::SetTransparencyMode(nsTransparencyMode aMode)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = GetToplevelWidget();
        if (!topWidget)
            return;

        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        if (!topWindow)
            return;

        topWindow->SetTransparencyMode(aMode);
        return;
    }
    bool isTransparent = aMode == eTransparencyTransparent;

    if (mIsTransparent == isTransparent)
        return;

    if (!isTransparent) {
        ClearTransparencyBitmap();
    } // else the new default alpha values are "all 1", so we don't
    // need to change anything yet

    mIsTransparent = isTransparent;

    // Need to clean our LayerManager up while still alive because
    // we don't want to use layers acceleration on shaped windows
    CleanLayerManagerRecursive();
}

nsTransparencyMode
nsWindow::GetTransparencyMode()
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = GetToplevelWidget();
        if (!topWidget) {
            return eTransparencyOpaque;
        }

        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        if (!topWindow) {
            return eTransparencyOpaque;
        }

        return topWindow->GetTransparencyMode();
    }

    return mIsTransparent ? eTransparencyTransparent : eTransparencyOpaque;
}

nsresult
nsWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
    for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
        const Configuration& configuration = aConfigurations[i];
        nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
        NS_ASSERTION(w->GetParent() == this,
                     "Configured widget is not a child");
        w->SetWindowClipRegion(configuration.mClipRegion, true);
        if (w->mBounds.Size() != configuration.mBounds.Size()) {
            w->Resize(configuration.mBounds.x, configuration.mBounds.y,
                      configuration.mBounds.width, configuration.mBounds.height,
                      true);
        } else if (w->mBounds.TopLeft() != configuration.mBounds.TopLeft()) {
            w->Move(configuration.mBounds.x, configuration.mBounds.y);
        } 
        w->SetWindowClipRegion(configuration.mClipRegion, false);
    }
    return NS_OK;
}

static pixman_box32
ToPixmanBox(const nsIntRect& aRect)
{
    pixman_box32_t result;
    result.x1 = aRect.x;
    result.y1 = aRect.y;
    result.x2 = aRect.XMost();
    result.y2 = aRect.YMost();
    return result;
}

static nsIntRect
ToIntRect(const pixman_box32& aBox)
{
    nsIntRect result;
    result.x = aBox.x1;
    result.y = aBox.y1;
    result.width = aBox.x2 - aBox.x1;
    result.height = aBox.y2 - aBox.y1;
    return result;
}

static void
InitRegion(pixman_region32* aRegion,
           const nsTArray<nsIntRect>& aRects)
{
    nsAutoTArray<pixman_box32,10> rects;
    rects.SetCapacity(aRects.Length());
    for (uint32_t i = 0; i < aRects.Length (); ++i) {
        if (!aRects[i].IsEmpty()) {
            rects.AppendElement(ToPixmanBox(aRects[i]));
        }
    }

    pixman_region32_init_rects(aRegion,
                               rects.Elements(), rects.Length());
}

static void
GetIntRects(pixman_region32& aRegion, nsTArray<nsIntRect>* aRects)
{
    int nRects;
    pixman_box32* boxes = pixman_region32_rectangles(&aRegion, &nRects);
    aRects->SetCapacity(aRects->Length() + nRects);
    for (int i = 0; i < nRects; ++i) {
        aRects->AppendElement(ToIntRect(boxes[i]));
    }
}

void
nsWindow::SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                              bool aIntersectWithExisting)
{
    const nsTArray<nsIntRect>* newRects = &aRects;

    nsAutoTArray<nsIntRect,1> intersectRects;
    if (aIntersectWithExisting) {
        nsAutoTArray<nsIntRect,1> existingRects;
        GetWindowClipRegion(&existingRects);

        nsAutoRef<pixman_region32> existingRegion;
        InitRegion(&existingRegion, existingRects);
        nsAutoRef<pixman_region32> newRegion;
        InitRegion(&newRegion, aRects);
        nsAutoRef<pixman_region32> intersectRegion;
        pixman_region32_init(&intersectRegion);
        pixman_region32_intersect(&intersectRegion,
                                  &newRegion, &existingRegion);

        // If mClipRects is null we haven't set a clip rect yet, so we
        // need to set the clip even if it is equal.
        if (mClipRects &&
            pixman_region32_equal(&intersectRegion, &existingRegion)) {
            return;
        }

        if (!pixman_region32_equal(&intersectRegion, &newRegion)) {
            GetIntRects(intersectRegion, &intersectRects);
            newRects = &intersectRects;
        }
    }

    if (!StoreWindowClipRegion(*newRects))
        return;

    if (!mGdkWindow)
        return;

#if (MOZ_WIDGET_GTK == 2)
    GdkRegion *region = gdk_region_new(); // aborts on OOM
    for (uint32_t i = 0; i < newRects->Length(); ++i) {
        const nsIntRect& r = newRects->ElementAt(i);
        GdkRectangle rect = { r.x, r.y, r.width, r.height };
        gdk_region_union_with_rect(region, &rect);
    }

    gdk_window_shape_combine_region(mGdkWindow, region, 0, 0);
    gdk_region_destroy(region);
#else
    cairo_region_t *region = cairo_region_create();
    for (uint32_t i = 0; i < newRects->Length(); ++i) {
        const nsIntRect& r = newRects->ElementAt(i);
        cairo_rectangle_int_t rect = { r.x, r.y, r.width, r.height };
        cairo_region_union_rectangle(region, &rect);
    }

    gdk_window_shape_combine_region(mGdkWindow, region, 0, 0);
    cairo_region_destroy(region);
#endif
  
    return;
}

void
nsWindow::ResizeTransparencyBitmap()
{
    if (!mTransparencyBitmap)
        return;

    if (mBounds.width == mTransparencyBitmapWidth &&
        mBounds.height == mTransparencyBitmapHeight)
        return;

    int32_t newRowBytes = GetBitmapStride(mBounds.width);
    int32_t newSize = newRowBytes * mBounds.height;
    gchar* newBits = new gchar[newSize];
    // fill new mask with "transparent", first
    memset(newBits, 0, newSize);

    // Now copy the intersection of the old and new areas into the new mask
    int32_t copyWidth = std::min(mBounds.width, mTransparencyBitmapWidth);
    int32_t copyHeight = std::min(mBounds.height, mTransparencyBitmapHeight);
    int32_t oldRowBytes = GetBitmapStride(mTransparencyBitmapWidth);
    int32_t copyBytes = GetBitmapStride(copyWidth);

    int32_t i;
    gchar* fromPtr = mTransparencyBitmap;
    gchar* toPtr = newBits;
    for (i = 0; i < copyHeight; i++) {
        memcpy(toPtr, fromPtr, copyBytes);
        fromPtr += oldRowBytes;
        toPtr += newRowBytes;
    }

    delete[] mTransparencyBitmap;
    mTransparencyBitmap = newBits;
    mTransparencyBitmapWidth = mBounds.width;
    mTransparencyBitmapHeight = mBounds.height;
}

static bool
ChangedMaskBits(gchar* aMaskBits, int32_t aMaskWidth, int32_t aMaskHeight,
        const nsIntRect& aRect, uint8_t* aAlphas, int32_t aStride)
{
    int32_t x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    int32_t maskBytesPerRow = GetBitmapStride(aMaskWidth);
    for (y = aRect.y; y < yMax; y++) {
        gchar* maskBytes = aMaskBits + y*maskBytesPerRow;
        uint8_t* alphas = aAlphas;
        for (x = aRect.x; x < xMax; x++) {
            bool newBit = *alphas > 0x7f;
            alphas++;

            gchar maskByte = maskBytes[x >> 3];
            bool maskBit = (maskByte & (1 << (x & 7))) != 0;

            if (maskBit != newBit) {
                return true;
            }
        }
        aAlphas += aStride;
    }

    return false;
}

static
void UpdateMaskBits(gchar* aMaskBits, int32_t aMaskWidth, int32_t aMaskHeight,
        const nsIntRect& aRect, uint8_t* aAlphas, int32_t aStride)
{
    int32_t x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    int32_t maskBytesPerRow = GetBitmapStride(aMaskWidth);
    for (y = aRect.y; y < yMax; y++) {
        gchar* maskBytes = aMaskBits + y*maskBytesPerRow;
        uint8_t* alphas = aAlphas;
        for (x = aRect.x; x < xMax; x++) {
            bool newBit = *alphas > 0x7f;
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
#ifdef MOZ_X11
    // We use X11 calls where possible, because GDK handles expose events
    // for shaped windows in a way that's incompatible with us (Bug 635903).
    // It doesn't occur when the shapes are set through X.
    GdkWindow *shellWindow = gtk_widget_get_window(mShell);
    Display* xDisplay = GDK_WINDOW_XDISPLAY(shellWindow);
    Window xDrawable = GDK_WINDOW_XID(shellWindow);
    Pixmap maskPixmap = XCreateBitmapFromData(xDisplay,
                                              xDrawable,
                                              mTransparencyBitmap,
                                              mTransparencyBitmapWidth,
                                              mTransparencyBitmapHeight);
    XShapeCombineMask(xDisplay, xDrawable,
                      ShapeBounding, 0, 0,
                      maskPixmap, ShapeSet);
    XFreePixmap(xDisplay, maskPixmap);
#else
#if (MOZ_WIDGET_GTK == 2)
    gtk_widget_reset_shapes(mShell);
    GdkBitmap* maskBitmap = gdk_bitmap_create_from_data(gtk_widget_get_window(mShell),
            mTransparencyBitmap,
            mTransparencyBitmapWidth, mTransparencyBitmapHeight);
    if (!maskBitmap)
        return;

    gtk_widget_shape_combine_mask(mShell, maskBitmap, 0, 0);
    g_object_unref(maskBitmap);
#else
    cairo_surface_t *maskBitmap;
    maskBitmap = cairo_image_surface_create_for_data((unsigned char*)mTransparencyBitmap, 
                                                     CAIRO_FORMAT_A1, 
                                                     mTransparencyBitmapWidth, 
                                                     mTransparencyBitmapHeight,
                                                     GetBitmapStride(mTransparencyBitmapWidth));
    if (!maskBitmap)
        return;

    cairo_region_t * maskRegion = gdk_cairo_region_create_from_surface(maskBitmap);
    gtk_widget_shape_combine_region(mShell, maskRegion);
    cairo_region_destroy(maskRegion);
    cairo_surface_destroy(maskBitmap);
#endif // MOZ_WIDGET_GTK2
#endif // MOZ_X11
}

void
nsWindow::ClearTransparencyBitmap()
{
    if (!mTransparencyBitmap)
        return;

    delete[] mTransparencyBitmap;
    mTransparencyBitmap = nullptr;
    mTransparencyBitmapWidth = 0;
    mTransparencyBitmapHeight = 0;

    if (!mShell)
        return;

#ifdef MOZ_X11
    GdkWindow *window = gtk_widget_get_window(mShell);
    if (!window)
        return;

    Display* xDisplay = GDK_WINDOW_XDISPLAY(window);
    Window xWindow = gdk_x11_window_get_xid(window);

    XShapeCombineMask(xDisplay, xWindow, ShapeBounding, 0, 0, None, ShapeSet);
#endif
}

nsresult
nsWindow::UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                               uint8_t* aAlphas, int32_t aStride)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = GetToplevelWidget();
        if (!topWidget)
            return NS_ERROR_FAILURE;

        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        if (!topWindow)
            return NS_ERROR_FAILURE;

        return topWindow->UpdateTranslucentWindowAlphaInternal(aRect, aAlphas, aStride);
    }

    NS_ASSERTION(mIsTransparent, "Window is not transparent");

    if (mTransparencyBitmap == nullptr) {
        int32_t size = GetBitmapStride(mBounds.width)*mBounds.height;
        mTransparencyBitmap = new gchar[size];
        memset(mTransparencyBitmap, 255, size);
        mTransparencyBitmapWidth = mBounds.width;
        mTransparencyBitmapHeight = mBounds.height;
    } else {
        ResizeTransparencyBitmap();
    }

    nsIntRect rect;
    rect.IntersectRect(aRect, nsIntRect(0, 0, mBounds.width, mBounds.height));

    if (!ChangedMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height,
                         rect, aAlphas, aStride))
        // skip the expensive stuff if the mask bits haven't changed; hopefully
        // this is the common case
        return NS_OK;

    UpdateMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height,
                   rect, aAlphas, aStride);

    if (!mNeedsShow) {
        ApplyTransparencyBitmap();
    }
    return NS_OK;
}

void
nsWindow::GrabPointer(guint32 aTime)
{
    LOG(("GrabPointer time=0x%08x retry=%d\n",
         (unsigned int)aTime, mRetryPointerGrab));

    mRetryPointerGrab = false;
    sRetryGrabTime = aTime;

    // If the window isn't visible, just set the flag to retry the
    // grab.  When this window becomes visible, the grab will be
    // retried.
    if (!mHasMappedToplevel || mIsFullyObscured) {
        LOG(("GrabPointer: window not visible\n"));
        mRetryPointerGrab = true;
        return;
    }

    if (!mGdkWindow)
        return;

    gint retval;
    retval = gdk_pointer_grab(mGdkWindow, TRUE,
                              (GdkEventMask)(GDK_BUTTON_PRESS_MASK |
                                             GDK_BUTTON_RELEASE_MASK |
                                             GDK_ENTER_NOTIFY_MASK |
                                             GDK_LEAVE_NOTIFY_MASK |
                                             GDK_POINTER_MOTION_MASK),
                              (GdkWindow *)NULL, NULL, aTime);

    if (retval == GDK_GRAB_NOT_VIEWABLE) {
        LOG(("GrabPointer: window not viewable; will retry\n"));
        mRetryPointerGrab = true;
    } else if (retval != GDK_GRAB_SUCCESS) {
        LOG(("GrabPointer: pointer grab failed: %i\n", retval));
        // A failed grab indicates that another app has grabbed the pointer.
        // Check for rollup now, because, without the grab, we likely won't
        // get subsequent button press events.
        CheckForRollup(0, 0, false, true);
    }
}

void
nsWindow::ReleaseGrabs(void)
{
    LOG(("ReleaseGrabs\n"));

    mRetryPointerGrab = false;
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
}

GtkWidget *
nsWindow::GetToplevelWidget()
{
    if (mShell) {
        return mShell;
    }

    GtkWidget *widget = GetMozContainerWidget();
    if (!widget)
        return nullptr;

    return gtk_widget_get_toplevel(widget);
}

GtkWidget *
nsWindow::GetMozContainerWidget()
{
    if (!mGdkWindow)
        return NULL;

    if (mContainer)
        return GTK_WIDGET(mContainer);

    GtkWidget *owningWidget =
        get_gtk_widget_for_gdk_window(mGdkWindow);
    return owningWidget;
}

nsWindow *
nsWindow::GetContainerWindow()
{
    GtkWidget *owningWidget = GetMozContainerWidget();
    if (!owningWidget)
        return nullptr;

    nsWindow *window = get_window_for_gtk_widget(owningWidget);
    NS_ASSERTION(window, "No nsWindow for container widget");
    return window;
}

void
nsWindow::SetUrgencyHint(GtkWidget *top_window, bool state)
{
    if (!top_window)
        return;
        
    gdk_window_set_urgency_hint(gtk_widget_get_window(top_window), state);
}

void *
nsWindow::SetupPluginPort(void)
{
    if (!mGdkWindow)
        return nullptr;

    if (gdk_window_is_destroyed(mGdkWindow) == TRUE)
        return nullptr;

    Window window = gdk_x11_window_get_xid(mGdkWindow);
    
    // we have to flush the X queue here so that any plugins that
    // might be running on separate X connections will be able to use
    // this window in case it was just created
#ifdef MOZ_X11
    XWindowAttributes xattrs;    
    Display *display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XGetWindowAttributes(display, window, &xattrs);
    XSelectInput (display, window,
                  xattrs.your_event_mask |
                  SubstructureNotifyMask);

    gdk_window_add_filter(mGdkWindow, plugin_window_filter_func, this);

    XSync(display, False);
#endif /* MOZ_X11 */
    
    return (void *)window;
}

void
nsWindow::SetDefaultIcon(void)
{
    SetIcon(NS_LITERAL_STRING("default"));
}

void
nsWindow::SetPluginType(PluginType aPluginType)
{
    mPluginType = aPluginType;
}

#ifdef MOZ_X11
void
nsWindow::SetNonXEmbedPluginFocus()
{
    if (gPluginFocusWindow == this || mPluginType!=PluginType_NONXEMBED) {
        return;
    }

    if (gPluginFocusWindow) {
        nsRefPtr<nsWindow> kungFuDeathGrip = gPluginFocusWindow;
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }

    LOGFOCUS(("nsWindow::SetNonXEmbedPluginFocus\n"));

    Window curFocusWindow;
    int focusState;
    
    GdkDisplay *gdkDisplay = gdk_window_get_display(mGdkWindow);
    XGetInputFocus(gdk_x11_display_get_xdisplay(gdkDisplay),
                   &curFocusWindow,
                   &focusState);

    LOGFOCUS(("\t curFocusWindow=%p\n", curFocusWindow));

    GdkWindow* toplevel = gdk_window_get_toplevel(mGdkWindow);
#if (MOZ_WIDGET_GTK == 2)
    GdkWindow *gdkfocuswin = gdk_window_lookup(curFocusWindow);
#else
    GdkWindow *gdkfocuswin = gdk_x11_window_lookup_for_display(gdkDisplay,
                                                               curFocusWindow);
#endif

    // lookup with the focus proxy window is supposed to get the
    // same GdkWindow as toplevel. If the current focused window
    // is not the focus proxy, we return without any change.
    if (gdkfocuswin != toplevel) {
        return;
    }

    // switch the focus from the focus proxy to the plugin window
    mOldFocusWindow = curFocusWindow;
    XRaiseWindow(GDK_WINDOW_XDISPLAY(mGdkWindow),
                 gdk_x11_window_get_xid(mGdkWindow));
    gdk_error_trap_push();
    XSetInputFocus(GDK_WINDOW_XDISPLAY(mGdkWindow),
                   gdk_x11_window_get_xid(mGdkWindow),
                   RevertToNone,
                   CurrentTime);
    gdk_flush();
    gdk_error_trap_pop();
    gPluginFocusWindow = this;
    gdk_window_add_filter(NULL, plugin_client_message_filter, this);

    LOGFOCUS(("nsWindow::SetNonXEmbedPluginFocus oldfocus=%p new=%p\n",
              mOldFocusWindow, gdk_x11_window_get_xid(mGdkWindow)));
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

    XGetInputFocus(GDK_WINDOW_XDISPLAY(mGdkWindow),
                   &curFocusWindow,
                   &focusState);

    // we only switch focus between plugin window and focus proxy. If the
    // current focused window is not the plugin window, just removing the
    // event filter that blocks the WM_TAKE_FOCUS is enough. WM and gtk2
    // will take care of the focus later.
    if (!curFocusWindow ||
        curFocusWindow == gdk_x11_window_get_xid(mGdkWindow)) {

        gdk_error_trap_push();
        XRaiseWindow(GDK_WINDOW_XDISPLAY(mGdkWindow),
                     mOldFocusWindow);
        XSetInputFocus(GDK_WINDOW_XDISPLAY(mGdkWindow),
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
#endif /* MOZ_X11 */

gint
nsWindow::ConvertBorderStyles(nsBorderStyle aStyle)
{
    gint w = 0;

    if (aStyle == eBorderStyle_default)
        return -1;

    // note that we don't handle eBorderStyle_close yet
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

    return w;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen)
{
    LOG(("nsWindow::MakeFullScreen [%p] aFullScreen %d\n",
         (void *)this, aFullScreen));

    if (aFullScreen) {
        if (mSizeMode != nsSizeMode_Fullscreen)
            mLastSizeMode = mSizeMode;

        mSizeMode = nsSizeMode_Fullscreen;
        gtk_window_fullscreen(GTK_WINDOW(mShell));
    }
    else {
        mSizeMode = mLastSizeMode;
        gtk_window_unfullscreen(GTK_WINDOW(mShell));
    }

    NS_ASSERTION(mLastSizeMode != nsSizeMode_Fullscreen,
                 "mLastSizeMode should never be fullscreen");
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::HideWindowChrome(bool aShouldHide)
{
    if (!mShell) {
        // Pass the request to the toplevel window
        GtkWidget *topWidget = GetToplevelWidget();
        if (!topWidget)
            return NS_ERROR_FAILURE;

        nsWindow *topWindow = get_window_for_gtk_widget(topWidget);
        if (!topWindow)
            return NS_ERROR_FAILURE;

        return topWindow->HideWindowChrome(aShouldHide);
    }

    // Sawfish, metacity, and presumably other window managers get
    // confused if we change the window decorations while the window
    // is visible.
    bool wasVisible = false;
    GdkWindow *shellWindow = gtk_widget_get_window(mShell);
    if (gdk_window_is_visible(shellWindow)) {
        gdk_window_hide(shellWindow);
        wasVisible = true;
    }

    gint wmd;
    if (aShouldHide)
        wmd = 0;
    else
        wmd = ConvertBorderStyles(mBorderStyle);

    if (wmd != -1)
      gdk_window_set_decorations(shellWindow, (GdkWMDecoration) wmd);

    if (wasVisible)
        gdk_window_show(shellWindow);

    // For some window managers, adding or removing window decorations
    // requires unmapping and remapping our toplevel window.  Go ahead
    // and flush the queue here so that we don't end up with a BadWindow
    // error later when this happens (when the persistence timer fires
    // and GetWindowPos is called)
#ifdef MOZ_X11
    XSync(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()) , False);
#else
    gdk_flush ();
#endif /* MOZ_X11 */

    return NS_OK;
}

bool
nsWindow::CheckForRollup(gdouble aMouseX, gdouble aMouseY,
                         bool aIsWheel, bool aAlwaysRollup)
{
    nsIRollupListener* rollupListener = GetActiveRollupListener();
    nsCOMPtr<nsIWidget> rollupWidget;
    if (rollupListener) {
        rollupWidget = rollupListener->GetRollupWidget();
    }
    if (!rollupWidget) {
        nsBaseWidget::gRollupListener = nullptr;
        return false;
    }

    bool retVal = false;
    GdkWindow *currentPopup =
        (GdkWindow *)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);
    if (aAlwaysRollup || !is_mouse_in_window(currentPopup, aMouseX, aMouseY)) {
        bool rollup = true;
        if (aIsWheel) {
            rollup = rollupListener->ShouldRollupOnMouseWheelEvent();
            retVal = true;
        }
        // if we're dealing with menus, we probably have submenus and
        // we don't want to rollup if the click is in a parent menu of
        // the current submenu
        uint32_t popupsToRollup = UINT32_MAX;
        if (!aAlwaysRollup) {
            nsAutoTArray<nsIWidget*, 5> widgetChain;
            uint32_t sameTypeCount = rollupListener->GetSubmenuWidgetChain(&widgetChain);
            for (uint32_t i=0; i<widgetChain.Length(); ++i) {
                nsIWidget* widget = widgetChain[i];
                GdkWindow* currWindow =
                    (GdkWindow*) widget->GetNativeData(NS_NATIVE_WINDOW);
                if (is_mouse_in_window(currWindow, aMouseX, aMouseY)) {
                  // don't roll up if the mouse event occurred within a
                  // menu of the same type. If the mouse event occurred
                  // in a menu higher than that, roll up, but pass the
                  // number of popups to Rollup so that only those of the
                  // same type close up.
                  if (i < sameTypeCount) {
                    rollup = false;
                  }
                  else {
                    popupsToRollup = sameTypeCount;
                  }
                  break;
                }
            } // foreach parent menu widget
        } // if rollup listener knows about menus

        // if we've determined that we should still rollup, do it.
        if (rollup && rollupListener->Rollup(popupsToRollup, nullptr)) {
            retVal = true;
        }
    }
    return retVal;
}

/* static */
bool
nsWindow::DragInProgress(void)
{
    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);

    if (!dragService)
        return false;

    nsCOMPtr<nsIDragSession> currentDragSession;
    dragService->GetCurrentSession(getter_AddRefs(currentDragSession));

    return currentDragSession != nullptr;
}

static bool
is_mouse_in_window (GdkWindow* aWindow, gdouble aMouseX, gdouble aMouseY)
{
    gint x = 0;
    gint y = 0;
    gint w, h;

    gint offsetX = 0;
    gint offsetY = 0;

    GdkWindow *window = aWindow;

    while (window) {
        gint tmpX = 0;
        gint tmpY = 0;

        gdk_window_get_position(window, &tmpX, &tmpY);
        GtkWidget *widget = get_gtk_widget_for_gdk_window(window);

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

#if (MOZ_WIDGET_GTK == 2)
    gdk_drawable_get_size(aWindow, &w, &h);
#else
    w = gdk_window_get_width(aWindow);
    h = gdk_window_get_height(aWindow);
#endif

    if (aMouseX > x && aMouseX < x + w &&
        aMouseY > y && aMouseY < y + h)
        return true;

    return false;
}

static nsWindow *
get_window_for_gtk_widget(GtkWidget *widget)
{
    gpointer user_data = g_object_get_data(G_OBJECT(widget), "nsWindow");

    return static_cast<nsWindow *>(user_data);
}

static nsWindow *
get_window_for_gdk_window(GdkWindow *window)
{
    gpointer user_data = g_object_get_data(G_OBJECT(window), "nsWindow");

    return static_cast<nsWindow *>(user_data);
}

static GtkWidget *
get_gtk_widget_for_gdk_window(GdkWindow *window)
{
    gpointer user_data = NULL;
    gdk_window_get_user_data(window, &user_data);

    return GTK_WIDGET(user_data);
}

static GdkCursor *
get_gtk_cursor(nsCursor aCursor)
{
    GdkCursor *gdkcursor = nullptr;
    uint8_t newType = 0xff;

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
        gdkcursor = gdk_cursor_new(GDK_QUESTION_ARROW);
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
    case eCursor_row_resize:
        gdkcursor = gdk_cursor_new(GDK_SB_V_DOUBLE_ARROW);
        break;
    case eCursor_ew_resize:
    case eCursor_col_resize:
        gdkcursor = gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW);
        break;
    case eCursor_none:
        newType = MOZ_CURSOR_NONE;
        break;
    default:
        NS_ASSERTION(aCursor, "Invalid cursor type");
        gdkcursor = gdk_cursor_new(GDK_LEFT_PTR);
        break;
    }

    // If by now we don't have a xcursor, this means we have to make a custom
    // one. First, we try creating a named cursor based on the hash of our
    // custom bitmap, as libXcursor has some magic to convert bitmapped cursors
    // to themed cursors
    if (newType != 0xFF && GtkCursors[newType].hash) {
        gdkcursor = gdk_cursor_new_from_name(gdk_display_get_default(),
                                             GtkCursors[newType].hash);
    }

    // If we still don't have a xcursor, we now really create a bitmap cursor
    if (newType != 0xff && !gdkcursor) {
        GdkPixbuf * cursor_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
        if (!cursor_pixbuf)
            return NULL;
      
        guchar *data = gdk_pixbuf_get_pixels(cursor_pixbuf);
        
        // Read data from GtkCursors and compose RGBA surface from 1bit bitmap and mask
        // GtkCursors bits and mask are 32x32 monochrome bitmaps (1 bit for each pixel)
        // so it's 128 byte array (4 bytes for are one bitmap row and there are 32 rows here).
        const unsigned char *bits = GtkCursors[newType].bits;
        const unsigned char *mask_bits = GtkCursors[newType].mask_bits;
        
        for (int i = 0; i < 128; i++) {
            char bit = *bits++;
            char mask = *mask_bits++;
            for (int j = 0; j < 8; j++) {
                unsigned char pix = ~(((bit >> j) & 0x01) * 0xff);
                *data++ = pix;
                *data++ = pix;
                *data++ = pix;
                *data++ = (((mask >> j) & 0x01) * 0xff);
            }
        }
      
        gdkcursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), cursor_pixbuf,
                                               GtkCursors[newType].hot_x,
                                               GtkCursors[newType].hot_y);
        
        g_object_unref(cursor_pixbuf);
    }

    gCursorCache[aCursor] = gdkcursor;

    return gdkcursor;
}

// gtk callbacks

#if (MOZ_WIDGET_GTK == 2)
static gboolean
expose_event_cb(GtkWidget *widget, GdkEventExpose *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
    if (!window)
        return FALSE;

    window->OnExposeEvent(event);
    return FALSE;
}
#else
void
draw_window_of_widget(GtkWidget *widget, GdkWindow *aWindow, cairo_t *cr)
{
    if (gtk_cairo_should_draw_window(cr, aWindow)) {
        nsRefPtr<nsWindow> window = get_window_for_gdk_window(aWindow);
        if (!window) {
            NS_WARNING("Cannot get nsWindow from GtkWidget");
        }
        else {      
            cairo_save(cr);      
            gtk_cairo_transform_to_window(cr, widget, aWindow);  
            // TODO - window->OnExposeEvent() can destroy this or other windows,
            // do we need to handle it somehow?
            window->OnExposeEvent(cr);
            cairo_restore(cr);
        }
    }
    
    GList *children = gdk_window_get_children(aWindow);
    GList *child = children;
    while (child) {
        GdkWindow *window = GDK_WINDOW(child->data);
        gpointer windowWidget;
        gdk_window_get_user_data(window, &windowWidget);
        if (windowWidget == widget) {
            draw_window_of_widget(widget, window, cr);
        }
        child = g_list_next(child);
    }  
    g_list_free(children);
}

/* static */
gboolean
expose_event_cb(GtkWidget *widget, cairo_t *cr)
{
    draw_window_of_widget(widget, gtk_widget_get_window(widget), cr);
    return FALSE;
}
#endif //MOZ_WIDGET_GTK2

static gboolean
configure_event_cb(GtkWidget *widget,
                   GdkEventConfigure *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    return window->OnConfigureEvent(widget, event);
}

static void
container_unrealize_cb (GtkWidget *widget)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return;

    window->OnContainerUnrealize();
}

static void
size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return;

    window->OnSizeAllocate(allocation);
}

static gboolean
delete_event_cb(GtkWidget *widget, GdkEventAny *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnDeleteEvent();

    return TRUE;
}

static gboolean
enter_notify_event_cb(GtkWidget *widget,
                      GdkEventCrossing *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
    if (!window)
        return TRUE;

    window->OnEnterNotifyEvent(event);

    return TRUE;
}

static gboolean
leave_notify_event_cb(GtkWidget *widget,
                      GdkEventCrossing *event)
{
    if (is_parent_grab_leave(event)) {
        return TRUE;
    }

    // bug 369599: Suppress LeaveNotify events caused by pointer grabs to
    // avoid generating spurious mouse exit events.
    gint x = gint(event->x_root);
    gint y = gint(event->y_root);
    GdkDisplay* display = gtk_widget_get_display(widget);
    GdkWindow* winAtPt = gdk_display_get_window_at_pointer(display, &x, &y);
    if (winAtPt == event->window) {
        return TRUE;
    }

    nsRefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
    if (!window)
        return TRUE;

    window->OnLeaveNotifyEvent(event);

    return TRUE;
}

static nsWindow*
GetFirstNSWindowForGDKWindow(GdkWindow *aGdkWindow)
{
    nsWindow* window;
    while (!(window = get_window_for_gdk_window(aGdkWindow))) {
        // The event has bubbled to the moz_container widget as passed into each caller's *widget parameter,
        // but its corresponding nsWindow is an ancestor of the window that we need.  Instead, look at
        // event->window and find the first ancestor nsWindow of it because event->window may be in a plugin.
        aGdkWindow = gdk_window_get_parent(aGdkWindow);
        if (!aGdkWindow) {
            window = nullptr;
            break;
        }
    }
    return window;
}

static gboolean
motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event)
{
    UpdateLastInputEventTime(event);

    nsWindow *window = GetFirstNSWindowForGDKWindow(event->window);
    if (!window)
        return FALSE;

    window->OnMotionNotifyEvent(event);

    return TRUE;
}

static gboolean
button_press_event_cb(GtkWidget *widget, GdkEventButton *event)
{
    UpdateLastInputEventTime(event);

    nsWindow *window = GetFirstNSWindowForGDKWindow(event->window);
    if (!window)
        return FALSE;

    window->OnButtonPressEvent(event);

    return TRUE;
}

static gboolean
button_release_event_cb(GtkWidget *widget, GdkEventButton *event)
{
    UpdateLastInputEventTime(event);

    nsWindow *window = GetFirstNSWindowForGDKWindow(event->window);
    if (!window)
        return FALSE;

    window->OnButtonReleaseEvent(event);

    return TRUE;
}

static gboolean
focus_in_event_cb(GtkWidget *widget, GdkEventFocus *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnContainerFocusInEvent(event);

    return FALSE;
}

static gboolean
focus_out_event_cb(GtkWidget *widget, GdkEventFocus *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnContainerFocusOutEvent(event);

    return FALSE;
}

#ifdef MOZ_X11
// For long-lived popup windows that don't really take focus themselves but
// may have elements that accept keyboard input when the parent window is
// active, focus is handled specially.  These windows include noautohide
// panels.  (This special handling is not necessary for temporary popups where
// the keyboard is grabbed.)
//
// Mousing over or clicking on these windows should not cause them to steal
// focus from their parent windows, so, the input field of WM_HINTS is set to
// False to request that the window manager not set the input focus to this
// window.  http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.7
//
// However, these windows can still receive WM_TAKE_FOCUS messages from the
// window manager, so they can still detect when the user has indicated that
// they wish to direct keyboard input at these windows.  When the window
// manager offers focus to these windows (after a mouse over or click, for
// example), a request to make the parent window active is issued.  When the
// parent window becomes active, keyboard events will be received.

static GdkFilterReturn
popup_take_focus_filter(GdkXEvent *gdk_xevent,
                        GdkEvent *event,
                        gpointer data)
{
    XEvent* xevent = static_cast<XEvent*>(gdk_xevent);
    if (xevent->type != ClientMessage)
        return GDK_FILTER_CONTINUE;

    XClientMessageEvent& xclient = xevent->xclient;
    if (xclient.message_type != gdk_x11_get_xatom_by_name("WM_PROTOCOLS"))
        return GDK_FILTER_CONTINUE;

    Atom atom = xclient.data.l[0];
    if (atom != gdk_x11_get_xatom_by_name("WM_TAKE_FOCUS"))
        return GDK_FILTER_CONTINUE;

    guint32 timestamp = xclient.data.l[1];

    GtkWidget* widget = get_gtk_widget_for_gdk_window(event->any.window);
    if (!widget)
        return GDK_FILTER_CONTINUE;

    GtkWindow* parent = gtk_window_get_transient_for(GTK_WINDOW(widget));
    if (!parent)
        return GDK_FILTER_CONTINUE;

    if (gtk_window_is_active(parent))
        return GDK_FILTER_REMOVE; // leave input focus on the parent

    GdkWindow* parent_window = gtk_widget_get_window(GTK_WIDGET(parent));
    if (!parent_window)
        return GDK_FILTER_CONTINUE;

    // In case the parent has not been deconified.
    gdk_window_show_unraised(parent_window);

    // Request focus on the parent window.
    // Use gdk_window_focus rather than gtk_window_present to avoid
    // raising the parent window.
    gdk_window_focus(parent_window, timestamp);
    return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
plugin_window_filter_func(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
    GdkWindow  *plugin_window;
    XEvent     *xevent;
    Window      xeventWindow;

    nsRefPtr<nsWindow> nswindow = (nsWindow*)data;
    GdkFilterReturn return_val;

    xevent = (XEvent *)gdk_xevent;
    return_val = GDK_FILTER_CONTINUE;

    switch (xevent->type)
    {
        case CreateNotify:
        case ReparentNotify:
            if (xevent->type==CreateNotify) {
                xeventWindow = xevent->xcreatewindow.window;
            }
            else {
                if (xevent->xreparent.event != xevent->xreparent.parent)
                    break;
                xeventWindow = xevent->xreparent.window;
            }
#if (MOZ_WIDGET_GTK == 2)
            plugin_window = gdk_window_lookup(xeventWindow);
#else
            plugin_window = gdk_x11_window_lookup_for_display(
                                  gdk_x11_lookup_xdisplay(xevent->xcreatewindow.display), xeventWindow);
#endif        
            if (plugin_window) {
                GtkWidget *widget =
                    get_gtk_widget_for_gdk_window(plugin_window);

// TODO GTK3
#if (MOZ_WIDGET_GTK == 2)
                if (GTK_IS_XTBIN(widget)) {
                    nswindow->SetPluginType(nsWindow::PluginType_NONXEMBED);
                    break;
                }
                else 
#endif
                if(GTK_IS_SOCKET(widget)) {
                    if (!g_object_get_data(G_OBJECT(widget), "enable-xt-focus")) {
                        nswindow->SetPluginType(nsWindow::PluginType_XEMBED);
                        break;
                    }
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

static GdkFilterReturn
plugin_client_message_filter(GdkXEvent *gdk_xevent,
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
#endif /* MOZ_X11 */

static gboolean
key_press_event_cb(GtkWidget *widget, GdkEventKey *event)
{
    LOG(("key_press_event_cb\n"));

    UpdateLastInputEventTime(event);

    // find the window with focus and dispatch this event to that widget
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    nsRefPtr<nsWindow> focusWindow = gFocusWindow ? gFocusWindow : window;

#ifdef MOZ_X11
    // Keyboard repeat can cause key press events to queue up when there are
    // slow event handlers (bug 301029).  Throttle these events by removing
    // consecutive pending duplicate KeyPress events to the same window.
    // We use the event time of the last one.
    // Note: GDK calls XkbSetDetectableAutorepeat so that KeyRelease events
    // are generated only when the key is physically released.
#define NS_GDKEVENT_MATCH_MASK 0x1FFF /* GDK_SHIFT_MASK .. GDK_BUTTON5_MASK */
    GdkDisplay* gdkDisplay = gtk_widget_get_display(widget);
    Display* dpy = GDK_DISPLAY_XDISPLAY(gdkDisplay);
    while (XPending(dpy)) {
        XEvent next_event;
        XPeekEvent(dpy, &next_event);
        GdkWindow* nextGdkWindow =
            gdk_x11_window_lookup_for_display(gdkDisplay, next_event.xany.window);
        if (nextGdkWindow != event->window ||
            next_event.type != KeyPress ||
            next_event.xkey.keycode != event->hardware_keycode ||
            next_event.xkey.state != (event->state & NS_GDKEVENT_MATCH_MASK)) {
            break;
        }
        XNextEvent(dpy, &next_event);
        event->time = next_event.xkey.time;
    }
#endif

    return focusWindow->OnKeyPressEvent(event);
}

static gboolean
key_release_event_cb(GtkWidget *widget, GdkEventKey *event)
{
    LOG(("key_release_event_cb\n"));

    UpdateLastInputEventTime(event);

    // find the window with focus and dispatch this event to that widget
    nsWindow *window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    nsRefPtr<nsWindow> focusWindow = gFocusWindow ? gFocusWindow : window;

    return focusWindow->OnKeyReleaseEvent(event);
}

static gboolean
scroll_event_cb(GtkWidget *widget, GdkEventScroll *event)
{
    nsWindow *window = GetFirstNSWindowForGDKWindow(event->window);
    if (!window)
        return FALSE;

    window->OnScrollEvent(event);

    return TRUE;
}

static gboolean
visibility_notify_event_cb (GtkWidget *widget, GdkEventVisibility *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
    if (!window)
        return FALSE;

    window->OnVisibilityNotifyEvent(event);

    return TRUE;
}

static void
hierarchy_changed_cb (GtkWidget *widget,
                      GtkWidget *previous_toplevel)
{
    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
    GdkWindowState old_window_state = GDK_WINDOW_STATE_WITHDRAWN;
    GdkEventWindowState event;

    event.new_window_state = GDK_WINDOW_STATE_WITHDRAWN;

    if (GTK_IS_WINDOW(previous_toplevel)) {
        g_signal_handlers_disconnect_by_func(previous_toplevel,
                                             FuncToGpointer(window_state_event_cb),
                                             widget);
        GdkWindow *win = gtk_widget_get_window(previous_toplevel);
        if (win) {
            old_window_state = gdk_window_get_state(win);
        }
    }

    if (GTK_IS_WINDOW(toplevel)) {
        g_signal_connect_swapped(toplevel, "window-state-event",
                                 G_CALLBACK(window_state_event_cb), widget);
        GdkWindow *win = gtk_widget_get_window(toplevel);
        if (win) {
            event.new_window_state = gdk_window_get_state(win);
        }
    }

    event.changed_mask = static_cast<GdkWindowState>
        (old_window_state ^ event.new_window_state);

    if (event.changed_mask) {
        event.type = GDK_WINDOW_STATE;
        event.window = NULL;
        event.send_event = TRUE;
        window_state_event_cb(widget, &event);
    }
}

static gboolean
window_state_event_cb (GtkWidget *widget, GdkEventWindowState *event)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnWindowStateEvent(widget, event);

    return FALSE;
}

static void
theme_changed_cb (GtkSettings *settings, GParamSpec *pspec, nsWindow *data)
{
    nsRefPtr<nsWindow> window = data;
    window->ThemeChanged();
}

//////////////////////////////////////////////////////////////////////
// These are all of our drag and drop operations

void
nsWindow::InitDragEvent(WidgetDragEvent &aEvent)
{
    // set the keyboard modifiers
    guint modifierState = KeymapWrapper::GetCurrentModifierState();
    KeymapWrapper::InitInputEvent(aEvent, modifierState);
}

static gboolean
drag_motion_event_cb(GtkWidget *aWidget,
                     GdkDragContext *aDragContext,
                     gint aX,
                     gint aY,
                     guint aTime,
                     gpointer aData)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return FALSE;

    // figure out which internal widget this drag motion actually happened on
    nscoord retx = 0;
    nscoord rety = 0;

    GdkWindow *innerWindow =
        get_inner_gdk_window(gtk_widget_get_window(aWidget), aX, aY,
                             &retx, &rety);
    nsRefPtr<nsWindow> innerMostWindow = get_window_for_gdk_window(innerWindow);

    if (!innerMostWindow) {
        innerMostWindow = window;
    }

    LOGDRAG(("nsWindow drag-motion signal for %p\n", (void*)innerMostWindow));

    return nsDragService::GetInstance()->
        ScheduleMotionEvent(innerMostWindow, aDragContext,
                            nsIntPoint(retx, rety), aTime);
}

static void
drag_leave_event_cb(GtkWidget *aWidget,
                    GdkDragContext *aDragContext,
                    guint aTime,
                    gpointer aData)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return;

    nsDragService *dragService = nsDragService::GetInstance();

    nsWindow *mostRecentDragWindow = dragService->GetMostRecentDestWindow();
    if (!mostRecentDragWindow) {
        // This can happen when the target will not accept a drop.  A GTK drag
        // source sends the leave message to the destination before the
        // drag-failed signal on the source widget, but the leave message goes
        // via the X server, and so doesn't get processed at least until the
        // event loop runs again.
        return;
    }

    GtkWidget *mozContainer = mostRecentDragWindow->GetMozContainerWidget();
    if (aWidget != mozContainer)
    {
        // When the drag moves between widgets, GTK can send leave signal for
        // the old widget after the motion or drop signal for the new widget.
        // We'll send the leave event when the motion or drop event is run.
        return;
    }

    LOGDRAG(("nsWindow drag-leave signal for %p\n",
             (void*)mostRecentDragWindow));

    dragService->ScheduleLeaveEvent();
}


static gboolean
drag_drop_event_cb(GtkWidget *aWidget,
                   GdkDragContext *aDragContext,
                   gint aX,
                   gint aY,
                   guint aTime,
                   gpointer aData)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return FALSE;

    // figure out which internal widget this drag motion actually happened on
    nscoord retx = 0;
    nscoord rety = 0;

    GdkWindow *innerWindow =
        get_inner_gdk_window(gtk_widget_get_window(aWidget), aX, aY,
                             &retx, &rety);
    nsRefPtr<nsWindow> innerMostWindow = get_window_for_gdk_window(innerWindow);

    if (!innerMostWindow) {
        innerMostWindow = window;
    }

    LOGDRAG(("nsWindow drag-drop signal for %p\n", (void*)innerMostWindow));

    return nsDragService::GetInstance()->
        ScheduleDropEvent(innerMostWindow, aDragContext,
                          nsIntPoint(retx, rety), aTime);
}

static void
drag_data_received_event_cb(GtkWidget *aWidget,
                            GdkDragContext *aDragContext,
                            gint aX,
                            gint aY,
                            GtkSelectionData  *aSelectionData,
                            guint aInfo,
                            guint aTime,
                            gpointer aData)
{
    nsRefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return;

    window->OnDragDataReceivedEvent(aWidget,
                                    aDragContext,
                                    aX, aY,
                                    aSelectionData,
                                    aInfo, aTime, aData);
}

static nsresult
initialize_prefs(void)
{
    gRaiseWindows =
        Preferences::GetBool("mozilla.widget.raise-on-setfocus", true);

    return NS_OK;
}

static GdkWindow *
get_inner_gdk_window (GdkWindow *aWindow,
                      gint x, gint y,
                      gint *retx, gint *rety)
{
    gint cx, cy, cw, ch;
    GList *children = gdk_window_peek_children(aWindow);
    for (GList *child = g_list_last(children);
         child;
         child = g_list_previous(child)) {
        GdkWindow *childWindow = (GdkWindow *) child->data;
        if (get_window_for_gdk_window(childWindow)) {
#if (MOZ_WIDGET_GTK == 2)
            gdk_window_get_geometry(childWindow, &cx, &cy, &cw, &ch, NULL);
#else
            gdk_window_get_geometry(childWindow, &cx, &cy, &cw, &ch);
#endif
            if ((cx < x) && (x < (cx + cw)) &&
                (cy < y) && (y < (cy + ch)) &&
                gdk_window_is_visible(childWindow)) {
                return get_inner_gdk_window(childWindow,
                                            x - cx, y - cy,
                                            retx, rety);
            }
        }
    }
    *retx = x;
    *rety = y;
    return aWindow;
}

static inline bool
is_context_menu_key(const WidgetKeyboardEvent& aKeyEvent)
{
    return ((aKeyEvent.keyCode == NS_VK_F10 && aKeyEvent.IsShift() &&
             !aKeyEvent.IsControl() && !aKeyEvent.IsMeta() &&
             !aKeyEvent.IsAlt()) ||
            (aKeyEvent.keyCode == NS_VK_CONTEXT_MENU && !aKeyEvent.IsShift() &&
             !aKeyEvent.IsControl() && !aKeyEvent.IsMeta() &&
             !aKeyEvent.IsAlt()));
}

static int
is_parent_ungrab_enter(GdkEventCrossing *aEvent)
{
    return (GDK_CROSSING_UNGRAB == aEvent->mode) &&
        ((GDK_NOTIFY_ANCESTOR == aEvent->detail) ||
         (GDK_NOTIFY_VIRTUAL == aEvent->detail));

}

static int
is_parent_grab_leave(GdkEventCrossing *aEvent)
{
    return (GDK_CROSSING_GRAB == aEvent->mode) &&
        ((GDK_NOTIFY_ANCESTOR == aEvent->detail) ||
            (GDK_NOTIFY_VIRTUAL == aEvent->detail));
}

#ifdef ACCESSIBILITY
void
nsWindow::CreateRootAccessible()
{
    if (mIsTopLevel && !mRootAccessible) {
        LOG(("nsWindow:: Create Toplevel Accessibility\n"));
        mRootAccessible = GetRootAccessible();
    }
}

void
nsWindow::DispatchEventToRootAccessible(uint32_t aEventType)
{
    if (!a11y::ShouldA11yBeEnabled()) {
        return;
    }

    nsCOMPtr<nsIAccessibilityService> accService =
        do_GetService("@mozilla.org/accessibilityService;1");
    if (!accService) {
        return;
    }

    // Get the root document accessible and fire event to it.
    a11y::Accessible* acc = GetRootAccessible();
    if (acc) {
        accService->FireAccessibleEvent(aEventType, acc);
    }
}

void
nsWindow::DispatchActivateEventAccessible(void)
{
    DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_ACTIVATE);
}

void
nsWindow::DispatchDeactivateEventAccessible(void)
{
    DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_DEACTIVATE);
}

void
nsWindow::DispatchMaximizeEventAccessible(void)
{
    DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_MAXIMIZE);
}

void
nsWindow::DispatchMinimizeEventAccessible(void)
{
    DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_MINIMIZE);
}

void
nsWindow::DispatchRestoreEventAccessible(void)
{
    DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_RESTORE);
}

#endif /* #ifdef ACCESSIBILITY */

// nsChildWindow class

nsChildWindow::nsChildWindow()
{
}

nsChildWindow::~nsChildWindow()
{
}

NS_IMETHODIMP
nsWindow::NotifyIME(NotificationToIME aNotification)
{
    if (MOZ_UNLIKELY(!mIMModule)) {
        switch (aNotification) {
            case NOTIFY_IME_OF_CURSOR_POS_CHANGED:
            case REQUEST_TO_COMMIT_COMPOSITION:
            case REQUEST_TO_CANCEL_COMPOSITION:
            case NOTIFY_IME_OF_FOCUS:
            case NOTIFY_IME_OF_BLUR:
              return NS_ERROR_NOT_AVAILABLE;
            default:
              break;
        }
    }
    switch (aNotification) {
        // TODO: We should replace NOTIFY_IME_OF_CURSOR_POS_CHANGED with
        //       NOTIFY_IME_OF_SELECTION_CHANGE.  The required behavior is
        //       really different from committing composition.
        case NOTIFY_IME_OF_CURSOR_POS_CHANGED:
        case REQUEST_TO_COMMIT_COMPOSITION:
            return mIMModule->CommitIMEComposition(this);
        case REQUEST_TO_CANCEL_COMPOSITION:
            return mIMModule->CancelIMEComposition(this);
        case NOTIFY_IME_OF_FOCUS:
            mIMModule->OnFocusChangeInGecko(true);
            return NS_OK;
        case NOTIFY_IME_OF_BLUR:
            mIMModule->OnFocusChangeInGecko(false);
            return NS_OK;
        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    if (!mIMModule) {
        return;
    }
    mIMModule->SetInputContext(this, &aContext, &aAction);
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
  InputContext context;
  if (!mIMModule) {
      context.mIMEState.mEnabled = IMEState::DISABLED;
      context.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
      // If IME context isn't available on this widget, we should set |this|
      // instead of nullptr since nullptr means that the platform has only one
      // context per process.
      context.mNativeIMEContext = this;
  } else {
      context = mIMModule->GetInputContext();
      context.mNativeIMEContext = mIMModule;
  }
  return context;
}

NS_IMETHODIMP
nsWindow::GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState)
{
    NS_ENSURE_ARG_POINTER(aLEDState);

    KeymapWrapper::Modifiers modifier;
    switch (aKeyCode) {
        case NS_VK_CAPS_LOCK:   modifier = KeymapWrapper::CAPS_LOCK;   break;
        case NS_VK_NUM_LOCK:    modifier = KeymapWrapper::NUM_LOCK;    break;
        case NS_VK_SCROLL_LOCK: modifier = KeymapWrapper::SCROLL_LOCK; break;
        default: return NS_ERROR_INVALID_ARG;
    }

    *aLEDState =
        KeymapWrapper::AreModifiersCurrentlyActive(modifier);
    return NS_OK;
}

#if defined(MOZ_X11) && (MOZ_WIDGET_GTK == 2)
/* static */ already_AddRefed<gfxASurface>
nsWindow::GetSurfaceForGdkDrawable(GdkDrawable* aDrawable,
                                   const nsIntSize& aSize)
{
    GdkVisual* visual = gdk_drawable_get_visual(aDrawable);
    Screen* xScreen =
        gdk_x11_screen_get_xscreen(gdk_drawable_get_screen(aDrawable));
    Display* xDisplay = DisplayOfScreen(xScreen);
    Drawable xDrawable = gdk_x11_drawable_get_xid(aDrawable);

    nsRefPtr<gfxASurface> result;

    if (visual) {
        Visual* xVisual = gdk_x11_visual_get_xvisual(visual);

        result = new gfxXlibSurface(xDisplay, xDrawable, xVisual,
                                    gfxIntSize(aSize.width, aSize.height));
    } else {
        // no visual? we must be using an xrender format.  Find a format
        // for this depth.
        XRenderPictFormat *pf = NULL;
        switch (gdk_drawable_get_depth(aDrawable)) {
            case 32:
                pf = XRenderFindStandardFormat(xDisplay, PictStandardARGB32);
                break;
            case 24:
                pf = XRenderFindStandardFormat(xDisplay, PictStandardRGB24);
                break;
            default:
                NS_ERROR("Don't know how to handle the given depth!");
                break;
        }

        result = new gfxXlibSurface(xScreen, xDrawable, pf,
                                    gfxIntSize(aSize.width, aSize.height));
    }

    return result.forget();
}
#endif

#if (MOZ_WIDGET_GTK == 2)
TemporaryRef<DrawTarget>
nsWindow::StartRemoteDrawing()
{
  gfxASurface *surf = GetThebesSurface();
  if (!surf) {
    return nullptr;
  }

  IntSize size(surf->GetSize().width, surf->GetSize().height);
  if (size.width <= 0 || size.height <= 0) {
    return nullptr;
  }

  return gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surf, size);
}
#endif

// return the gfxASurface for rendering to this widget
gfxASurface*
#if (MOZ_WIDGET_GTK == 2)
nsWindow::GetThebesSurface()
#else
nsWindow::GetThebesSurface(cairo_t *cr)
#endif
{
    if (!mGdkWindow)
        return nullptr;

#if (MOZ_WIDGET_GTK != 2)
    cairo_surface_t *surf = cairo_get_target(cr);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
      NS_NOTREACHED("Missing cairo target?");
      return NULL;
    }
#endif // MOZ_WIDGET_GTK2

#ifdef MOZ_X11
    gint width, height;

#if (MOZ_WIDGET_GTK == 2)
    gdk_drawable_get_size(GDK_DRAWABLE(mGdkWindow), &width, &height);
#else
    width = gdk_window_get_width(mGdkWindow);
    height = gdk_window_get_height(mGdkWindow);
#endif

    // Owen Taylor says this is the right thing to do!
    width = std::min(32767, width);
    height = std::min(32767, height);
    gfxIntSize size(width, height);

    GdkVisual *gdkVisual = gdk_window_get_visual(mGdkWindow);
    Visual* visual = gdk_x11_visual_get_xvisual(gdkVisual);

#  ifdef MOZ_HAVE_SHMIMAGE
    bool usingShm = false;
    if (nsShmImage::UseShm()) {
        // EnsureShmImage() is a dangerous interface, but we guarantee
        // that the thebes surface and the shmimage have the same
        // lifetime
        mThebesSurface =
            nsShmImage::EnsureShmImage(size,
                                       visual, gdk_visual_get_depth(gdkVisual),
                                       mShmImage);
        usingShm = mThebesSurface != nullptr;
    }
    if (!usingShm)
#  endif  // MOZ_HAVE_SHMIMAGE

#if (MOZ_WIDGET_GTK == 2)
    mThebesSurface = new gfxXlibSurface
        (GDK_WINDOW_XDISPLAY(mGdkWindow),
         gdk_x11_window_get_xid(mGdkWindow),
         visual,
         size);
#else
#if MOZ_TREE_CAIRO
#error "cairo-gtk3 target must be built with --enable-system-cairo"
#else
    mThebesSurface = gfxASurface::Wrap(surf);
#endif
#endif

#endif

    // if the surface creation is reporting an error, then
    // we don't have a surface to give back
    if (mThebesSurface && mThebesSurface->CairoStatus() != 0) {
        mThebesSurface = nullptr;
    }

    return mThebesSurface;
}

// Code shared begin BeginMoveDrag and BeginResizeDrag
bool
nsWindow::GetDragInfo(WidgetMouseEvent* aMouseEvent,
                      GdkWindow** aWindow, gint* aButton,
                      gint* aRootX, gint* aRootY)
{
    if (aMouseEvent->button != WidgetMouseEvent::eLeftButton) {
        // we can only begin a move drag with the left mouse button
        return false;
    }
    *aButton = 1;

    // get the gdk window for this widget
    GdkWindow* gdk_window = mGdkWindow;
    if (!gdk_window) {
        return false;
    }
    NS_ABORT_IF_FALSE(GDK_IS_WINDOW(gdk_window), "must really be window");

    // find the top-level window
    gdk_window = gdk_window_get_toplevel(gdk_window);
    NS_ABORT_IF_FALSE(gdk_window,
                      "gdk_window_get_toplevel should not return null");
    *aWindow = gdk_window;

    if (!aMouseEvent->widget) {
        return false;
    }

    // FIXME: It would be nice to have the widget position at the time
    // of the event, but it's relatively unlikely that the widget has
    // moved since the mousedown.  (On the other hand, it's quite likely
    // that the mouse has moved, which is why we use the mouse position
    // from the event.)
    nsIntPoint offset = aMouseEvent->widget->WidgetToScreenOffset();
    *aRootX = aMouseEvent->refPoint.x + offset.x;
    *aRootY = aMouseEvent->refPoint.y + offset.y;

    return true;
}

NS_IMETHODIMP
nsWindow::BeginMoveDrag(WidgetMouseEvent* aEvent)
{
    NS_ABORT_IF_FALSE(aEvent, "must have event");
    NS_ABORT_IF_FALSE(aEvent->eventStructType == NS_MOUSE_EVENT,
                      "event must have correct struct type");

    GdkWindow *gdk_window;
    gint button, screenX, screenY;
    if (!GetDragInfo(aEvent, &gdk_window, &button, &screenX, &screenY)) {
        return NS_ERROR_FAILURE;
    }

    // tell the window manager to start the move
    gdk_window_begin_move_drag(gdk_window, button, screenX, screenY,
                               aEvent->time);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::BeginResizeDrag(WidgetGUIEvent* aEvent,
                          int32_t aHorizontal,
                          int32_t aVertical)
{
    NS_ENSURE_ARG_POINTER(aEvent);

    if (aEvent->eventStructType != NS_MOUSE_EVENT) {
        // you can only begin a resize drag with a mouse event
        return NS_ERROR_INVALID_ARG;
    }

    WidgetMouseEvent* mouse_event = static_cast<WidgetMouseEvent*>(aEvent);

    GdkWindow *gdk_window;
    gint button, screenX, screenY;
    if (!GetDragInfo(mouse_event, &gdk_window, &button, &screenX, &screenY)) {
        return NS_ERROR_FAILURE;
    }

    // work out what GdkWindowEdge we're talking about
    GdkWindowEdge window_edge;
    if (aVertical < 0) {
        if (aHorizontal < 0) {
            window_edge = GDK_WINDOW_EDGE_NORTH_WEST;
        } else if (aHorizontal == 0) {
            window_edge = GDK_WINDOW_EDGE_NORTH;
        } else {
            window_edge = GDK_WINDOW_EDGE_NORTH_EAST;
        }
    } else if (aVertical == 0) {
        if (aHorizontal < 0) {
            window_edge = GDK_WINDOW_EDGE_WEST;
        } else if (aHorizontal == 0) {
            return NS_ERROR_INVALID_ARG;
        } else {
            window_edge = GDK_WINDOW_EDGE_EAST;
        }
    } else {
        if (aHorizontal < 0) {
            window_edge = GDK_WINDOW_EDGE_SOUTH_WEST;
        } else if (aHorizontal == 0) {
            window_edge = GDK_WINDOW_EDGE_SOUTH;
        } else {
            window_edge = GDK_WINDOW_EDGE_SOUTH_EAST;
        }
    }

    // tell the window manager to start the resize
    gdk_window_begin_resize_drag(gdk_window, window_edge, button,
                                 screenX, screenY, aEvent->time);

    return NS_OK;
}

nsIWidget::LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
    if (!mLayerManager && eTransparencyTransparent == GetTransparencyMode()) {
        mLayerManager = CreateBasicLayerManager();
    }

    return nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint,
                                         aPersistence, aAllowRetaining);
}

void
nsWindow::ClearCachedResources()
{
    if (mLayerManager &&
        mLayerManager->GetBackendType() == mozilla::layers::LAYERS_BASIC) {
        mLayerManager->ClearCachedResources();
    }

    GList* children = gdk_window_peek_children(mGdkWindow);
    for (GList* list = children; list; list = list->next) {
        nsWindow* window = get_window_for_gdk_window(GDK_WINDOW(list->data));
        if (window) {
            window->ClearCachedResources();
        }
    }
}

nsresult
nsWindow::SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                     uint32_t aNativeMessage,
                                     uint32_t aModifierFlags)
{
  if (!mGdkWindow) {
    return NS_OK;
  }

  GdkDisplay* display = gdk_window_get_display(mGdkWindow);

  // When a button-release event is requested, create it here and put it in the
  // event queue. This will not emit a motion event - this needs to be done
  // explicitly *before* requesting a button-release. You will also need to wait
  // for the motion event to be dispatched before requesting a button-release
  // event to maintain the desired event order.
  if (aNativeMessage == GDK_BUTTON_RELEASE) {
    GdkEvent event;
    memset(&event, 0, sizeof(GdkEvent));
    event.type = (GdkEventType)aNativeMessage;
    event.button.button = 1;
    event.button.window = mGdkWindow;
    event.button.time = GDK_CURRENT_TIME;

#if (MOZ_WIDGET_GTK == 3)
    // Get device for event source
    GdkDeviceManager *device_manager = gdk_display_get_device_manager(display);
    event.button.device = gdk_device_manager_get_client_pointer(device_manager);
#endif

    gdk_event_put(&event);
  } else {
    // We don't support specific events other than button-release. In case
    // aNativeMessage != GDK_BUTTON_RELEASE we'll synthesize a motion event
    // that will be emitted by gdk_display_warp_pointer().
    GdkScreen* screen = gdk_window_get_screen(mGdkWindow);
    gdk_display_warp_pointer(display, screen, aPoint.x, aPoint.y);
  }

  return NS_OK;
}
