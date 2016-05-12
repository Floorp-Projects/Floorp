/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindow.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/EventForwards.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/UniquePtrExtensions.h"
#include <algorithm>

#include "GeckoProfiler.h"

#include "prlink.h"
#include "nsGTKToolkit.h"
#include "nsIRollupListener.h"
#include "nsIDOMNode.h"

#include "nsWidgetsCID.h"
#include "nsDragService.h"
#include "nsIWidgetListener.h"
#include "nsIScreenManager.h"
#include "SystemTimeConverter.h"

#include "nsGtkKeyUtils.h"
#include "nsGtkCursors.h"
#include "nsScreenGtk.h"

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

#include "mozilla/Assertions.h"
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
#include "gfx2DGlue.h"
#include "nsPluginNativeWindowGtk.h"

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
#include "ClientLayerManager.h"

#include "gfxPlatformGtk.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "Layers.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/CompositorBridgeParent.h"

#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif
  
#include "nsShmImage.h"

#include "nsIDOMWheelEvent.h"

#include "NativeKeyBindings.h"

#include <dlfcn.h>

#include "mozilla/layers/APZCTreeManager.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;
using namespace mozilla::layers;
using mozilla::gl::GLContext;

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION 100

const gint kEvents = GDK_EXPOSURE_MASK | GDK_STRUCTURE_MASK |
                     GDK_VISIBILITY_NOTIFY_MASK |
                     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
#if GTK_CHECK_VERSION(3,4,0)
                     GDK_SMOOTH_SCROLL_MASK |
                     GDK_TOUCH_MASK |
#endif
                     GDK_SCROLL_MASK |
                     GDK_POINTER_MOTION_MASK |
                     GDK_PROPERTY_CHANGE_MASK;

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
static gboolean property_notify_event_cb  (GtkWidget *widget,
                                           GdkEventProperty *event);
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
#if (MOZ_WIDGET_GTK == 3)
static void     scale_changed_cb          (GtkWidget* widget,
                                           GParamSpec* aPSpec,
                                           gpointer aPointer);
#endif
#if GTK_CHECK_VERSION(3,4,0)
static gboolean touch_event_cb            (GtkWidget* aWidget,
                                           GdkEventTouch* aEvent);
#endif
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

static SystemTimeConverter<guint32>&
TimeConverter() {
    static SystemTimeConverter<guint32> sTimeConverterSingleton;
    return sTimeConverterSingleton;
}

namespace mozilla {

class CurrentX11TimeGetter
{
public:
    explicit CurrentX11TimeGetter(GdkWindow* aWindow)
        : mWindow(aWindow)
        , mAsyncUpdateStart()
    {
    }

    guint32 GetCurrentTime() const
    {
        return gdk_x11_get_server_time(mWindow);
    }

    void GetTimeAsyncForPossibleBackwardsSkew(const TimeStamp& aNow)
    {
        // Check for in-flight request
        if (!mAsyncUpdateStart.IsNull()) {
            return;
        }
        mAsyncUpdateStart = aNow;

        Display* xDisplay = GDK_WINDOW_XDISPLAY(mWindow);
        Window xWindow = GDK_WINDOW_XID(mWindow);
        unsigned char c = 'a';
        Atom timeStampPropAtom = TimeStampPropAtom();
        XChangeProperty(xDisplay, xWindow, timeStampPropAtom,
                        timeStampPropAtom, 8, PropModeReplace, &c, 1);
        XFlush(xDisplay);
    }

    gboolean PropertyNotifyHandler(GtkWidget* aWidget,
                                   GdkEventProperty* aEvent)
    {
        if (aEvent->atom !=
            gdk_x11_xatom_to_atom(TimeStampPropAtom())) {
            return FALSE;
        }

        guint32 eventTime = aEvent->time;
        TimeStamp lowerBound = mAsyncUpdateStart;

        TimeConverter().CompensateForBackwardsSkew(eventTime, lowerBound);
        mAsyncUpdateStart = TimeStamp();
        return TRUE;
    }

private:
    static Atom TimeStampPropAtom() {
        return gdk_x11_get_xatom_by_name_for_display(
            gdk_display_get_default(), "GDK_TIMESTAMP_PROP");
    }

    // This is safe because this class is stored as a member of mWindow and
    // won't outlive it.
    GdkWindow* mWindow;
    TimeStamp  mAsyncUpdateStart;
};

} // namespace mozilla

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

// The window from which the focus manager asks us to dispatch key events.
static nsWindow         *gFocusWindow          = nullptr;
static bool              gBlockActivateEvent   = false;
static bool              gGlobalsInitialized   = false;
static bool              gRaiseWindows         = true;
static nsWindow         *gPluginFocusWindow    = nullptr;

#if GTK_CHECK_VERSION(3,4,0)
static uint32_t          gLastTouchID = 0;
#endif

#define NS_WINDOW_TITLE_MAX_LENGTH 4095

// If after selecting profile window, the startup fail, please refer to
// http://bugzilla.gnome.org/show_bug.cgi?id=88940

// needed for imgIContainer cursors
// GdkDisplay* was added in 2.2
typedef struct _GdkDisplay GdkDisplay;

#define kWindowPositionSlop 20

// cursor cache
static GdkCursor *gCursorCache[eCursorCount];

static GtkWidget *gInvisibleContainer = nullptr;

// Sometimes this actually also includes the state of the modifier keys, but
// only the button state bits are used.
static guint gButtonState;

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

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

nsWindow::nsWindow()
{
    mIsTopLevel          = false;
    mIsDestroyed         = false;
    mListenForResizes    = false;
    mNeedsDispatchResized = false;
    mIsShown             = false;
    mNeedsShow           = false;
    mEnabled             = true;
    mCreated             = false;
#if GTK_CHECK_VERSION(3,4,0)
    mHandleTouchEvent    = false;
#endif
    mIsDragPopup         = false;

    mContainer           = nullptr;
    mGdkWindow           = nullptr;
    mShell               = nullptr;
    mPluginNativeWindow  = nullptr;
    mHasMappedToplevel   = false;
    mIsFullyObscured     = false;
    mRetryPointerGrab    = false;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mLastSizeMode        = nsSizeMode_Normal;
    mSizeConstraints.mMaxSize = GetSafeWindowSize(mSizeConstraints.mMaxSize);

#ifdef MOZ_X11
    mOldFocusWindow      = 0;

    mXDisplay = nullptr;
    mXWindow  = None;
    mXVisual  = nullptr;
    mXDepth   = 0;
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

#if GTK_CHECK_VERSION(3,4,0)
    mLastScrollEventTime = GDK_CURRENT_TIME;
#endif
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
#if (MOZ_WIDGET_GTK == 3)
      g_object_unref(gCursorCache[i]);
#else
      gdk_cursor_unref(gCursorCache[i]);
#endif
      gCursorCache[i] = nullptr;
    }
  }
}

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
nsWindow::DispatchResized()
{
    mNeedsDispatchResized = false;
    if (mWidgetListener) {
        mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
    }
    if (mAttachedWidgetListener) {
        mAttachedWidgetListener->WindowResized(this,
                                               mBounds.width, mBounds.height);
    }
}

void
nsWindow::MaybeDispatchResized()
{
    if (mNeedsDispatchResized && !mIsDestroyed) {
        DispatchResized();
    }
}

nsIWidgetListener*
nsWindow::GetListener()
{
    return mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
}

nsresult
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
#ifdef DEBUG
    debug_DumpEvent(stdout, aEvent->mWidget, aEvent,
                    "something", 0);
#endif
    aStatus = nsEventStatus_eIgnore;
    nsIWidgetListener* listener = GetListener();
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
        gInvisibleContainer = nullptr;
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
        mLayerManager->Destroy();
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
            rollupListener->Rollup(0, false, nullptr, nullptr);
        }
    }

    // dragService will be null after shutdown of the service manager.
    nsDragService *dragService = nsDragService::GetInstance();
    if (dragService && this == dragService->GetMostRecentDestWindow()) {
        dragService->ScheduleLeaveEvent();
    }

    NativeShow(false);

    if (mIMContext) {
        mIMContext->OnDestroyWindow(this);
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
#endif /* MOZ_X11 && MOZ_WIDGET_GTK == 2 && defined(MOZ_X11) */

    GtkWidget *owningWidget = GetMozContainerWidget();
    if (mShell) {
        gtk_widget_destroy(mShell);
        mShell = nullptr;
        mContainer = nullptr;
        MOZ_ASSERT(!mGdkWindow,
                   "mGdkWindow should be NULL when mContainer is destroyed");
    }
    else if (mContainer) {
        gtk_widget_destroy(GTK_WIDGET(mContainer));
        mContainer = nullptr;
        MOZ_ASSERT(!mGdkWindow,
                   "mGdkWindow should be NULL when mContainer is destroyed");
    }
    else if (mGdkWindow) {
        // Destroy child windows to ensure that their mThebesSurfaces are
        // released and to remove references from GdkWindows back to their
        // container widget.  (OnContainerUnrealize() does this when the
        // MozContainer widget is destroyed.)
        DestroyChildWindows();

        gdk_window_set_user_data(mGdkWindow, nullptr);
        g_object_set_data(G_OBJECT(mGdkWindow), "nsWindow", nullptr);
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
    GdkScreen *screen = gdk_display_get_default_screen(gdk_display_get_default());
    double heightInches = gdk_screen_get_height_mm(screen)/MM_PER_INCH_FLOAT;
    if (heightInches < 0.25) {
        // Something's broken, but we'd better not crash.
        return 96.0f;
    }
    return float(gdk_screen_get_height(screen)/heightInches);
}

double
nsWindow::GetDefaultScaleInternal()
{
    return GdkScaleFactor() * gfxPlatformGtk::GetDPIScale();
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
        MOZ_ASSERT(gdk_window_is_destroyed(mGdkWindow),
                   "live GdkWindow with no widget");
        return NS_OK;
    }

    if (aNewParent) {
        aNewParent->AddChild(this);
        ReparentNativeWidget(aNewParent);
    } else {
        // aNewParent is nullptr, but reparent to a hidden window to avoid
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
        MOZ_ASSERT(gdk_window_is_destroyed(mGdkWindow),
                   "live GdkWindow with no widget");
        return NS_OK;
    }
    MOZ_ASSERT(!gdk_window_is_destroyed(mGdkWindow),
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
        MOZ_ASSERT(!aNewParentWindow ||
                   gdk_window_is_destroyed(aNewParentWindow),
                   "live GdkWindow with no widget");
        Destroy();
    } else {
        if (aNewContainer != aOldContainer) {
            MOZ_ASSERT(!gdk_window_is_destroyed(aNewParentWindow),
                       "destroyed GdkWindow with widget");
            SetWidgetForHierarchy(mGdkWindow, aOldContainer, aNewContainer);

            if (aOldContainer == gInvisibleContainer) {
                CheckDestroyInvisibleContainer();
            }
        }

        if (!mIsTopLevel) {
            gdk_window_reparent(mGdkWindow, aNewParentWindow,
                                DevicePixelsToGdkCoordRoundDown(mBounds.x),
                                DevicePixelsToGdkCoordRoundDown(mBounds.y));
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

void
nsWindow::RegisterTouchWindow()
{
#if GTK_CHECK_VERSION(3,4,0)
    mHandleTouchEvent = true;
    mTouches.Clear();
#endif
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY)
{
    if (!mIsTopLevel || !mShell)  
      return NS_OK;

    double dpiScale = GetDefaultScale().scale;

    // we need to use the window size in logical screen pixels
    int32_t logWidth = std::max(NSToIntRound(mBounds.width / dpiScale), 1);
    int32_t logHeight = std::max(NSToIntRound(mBounds.height / dpiScale), 1);  

    /* get our playing field. use the current screen, or failing that
      for any reason, use device caps for the default screen. */
    nsCOMPtr<nsIScreen> screen;
    nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
    if (screenmgr) {
      screenmgr->ScreenForRect(*aX, *aY, logWidth, logHeight,
                               getter_AddRefs(screen));
    }

    // We don't have any screen so leave the coordinates as is
    if (!screen)
      return NS_OK;

    nsIntRect screenRect;
    if (mSizeMode != nsSizeMode_Fullscreen) {
      // For normalized windows, use the desktop work area.
      screen->GetAvailRectDisplayPix(&screenRect.x, &screenRect.y,
                                     &screenRect.width, &screenRect.height);
    } else {
      // For full screen windows, use the desktop.
      screen->GetRectDisplayPix(&screenRect.x, &screenRect.y,
                                &screenRect.width, &screenRect.height);
    }

    if (aAllowSlop) {
      if (*aX < screenRect.x - logWidth + kWindowPositionSlop)
          *aX = screenRect.x - logWidth + kWindowPositionSlop;
      else if (*aX >= screenRect.XMost() - kWindowPositionSlop)
          *aX = screenRect.XMost() - kWindowPositionSlop;

      if (*aY < screenRect.y - logHeight + kWindowPositionSlop)
          *aY = screenRect.y - logHeight + kWindowPositionSlop;
      else if (*aY >= screenRect.YMost() - kWindowPositionSlop)
          *aY = screenRect.YMost() - kWindowPositionSlop;
    } else {  
      if (*aX < screenRect.x)
          *aX = screenRect.x;
      else if (*aX >= screenRect.XMost() - logWidth)
          *aX = screenRect.XMost() - logWidth;

      if (*aY < screenRect.y)
          *aY = screenRect.y;
      else if (*aY >= screenRect.YMost() - logHeight)
          *aY = screenRect.YMost() - logHeight;
    }

    return NS_OK;
}

void nsWindow::SetSizeConstraints(const SizeConstraints& aConstraints)
{
    mSizeConstraints.mMinSize = GetSafeWindowSize(aConstraints.mMinSize);
    mSizeConstraints.mMaxSize = GetSafeWindowSize(aConstraints.mMaxSize);

    if (mShell) {
        GdkGeometry geometry;
        geometry.min_width = DevicePixelsToGdkCoordRoundUp(
                             mSizeConstraints.mMinSize.width);
        geometry.min_height = DevicePixelsToGdkCoordRoundUp(
                              mSizeConstraints.mMinSize.height);
        geometry.max_width = DevicePixelsToGdkCoordRoundDown(
                             mSizeConstraints.mMaxSize.width);
        geometry.max_height = DevicePixelsToGdkCoordRoundDown(
                              mSizeConstraints.mMaxSize.height);

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
    double scale = BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
    int32_t width = NSToIntRound(scale * aWidth);
    int32_t height = NSToIntRound(scale * aHeight);
    ConstrainSize(&width, &height);

    // For top-level windows, aWidth and aHeight should possibly be
    // interpreted as frame bounds, but NativeResize treats these as window
    // bounds (Bug 581866).

    mBounds.SizeTo(width, height);

    if (!mCreated)
        return NS_OK;

    NativeResize();

    NotifyRollupGeometryChange();
    ResizePluginSocketWidget();

    // send a resize notification if this is a toplevel
    if (mIsTopLevel || mListenForResizes) {
        DispatchResized();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                 bool aRepaint)
{
    double scale = BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
    int32_t width = NSToIntRound(scale * aWidth);
    int32_t height = NSToIntRound(scale * aHeight);
    ConstrainSize(&width, &height);

    int32_t x = NSToIntRound(scale * aX);
    int32_t y = NSToIntRound(scale * aY);
    mBounds.x = x;
    mBounds.y = y;
    mBounds.SizeTo(width, height);

    if (!mCreated)
        return NS_OK;

    NativeMoveResize();

    NotifyRollupGeometryChange();
    ResizePluginSocketWidget();

    if (mIsTopLevel || mListenForResizes) {
        DispatchResized();
    }

    return NS_OK;
}

void
nsWindow::ResizePluginSocketWidget()
{
    // e10s specific, a eWindowType_plugin_ipc_chrome holds its own
    // nsPluginNativeWindowGtk wrapper. We are responsible for resizing
    // the embedded socket widget.
    if (mWindowType == eWindowType_plugin_ipc_chrome) {
        nsPluginNativeWindowGtk* wrapper = (nsPluginNativeWindowGtk*)
          GetNativeData(NS_NATIVE_PLUGIN_OBJECT_PTR);
        if (wrapper) {
            wrapper->width = mBounds.width;
            wrapper->height = mBounds.height;
            wrapper->SetAllocation();
        }
    }
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

    double scale = BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
    int32_t x = NSToIntRound(aX * scale);
    int32_t y = NSToIntRound(aY * scale);

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

    NativeMove();

    NotifyRollupGeometryChange();
    return NS_OK;
}


void
nsWindow::NativeMove()
{
    GdkPoint point = DevicePixelsToGdkPointRoundDown(mBounds.TopLeft());

    if (mIsTopLevel) {
        gtk_window_move(GTK_WINDOW(mShell), point.x, point.y);
    }
    else if (mGdkWindow) {
        gdk_window_move(mGdkWindow, point.x, point.y);
    }
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                      nsIWidget                  *aWidget,
                      bool                        aActivate)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsWindow::SetZIndex(int32_t aZIndex)
{
    nsIWidget* oldPrev = GetPrevSibling();

    nsBaseWidget::SetZIndex(aZIndex);

    if (GetPrevSibling() == oldPrev) {
        return;
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
}

NS_IMETHODIMP
nsWindow::SetSizeMode(nsSizeMode aMode)
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
        gdk_x11_window_set_user_time(gdkWindow, sn_launchee_context_get_timestamp(ctx));
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

    RefPtr<nsWindow> owningWindow = get_window_for_gtk_widget(owningWidget);
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

    if (mIMContext) {
        mIMContext->OnFocusWindow(this);
    }

    LOGFOCUS(("  widget now has focus in SetFocus() [%p]\n",
              (void *)this));

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(LayoutDeviceIntRect& aRect)
{
    if (mIsTopLevel && mContainer) {
        // use the point including window decorations
        gint x, y;
        gdk_window_get_root_origin(gtk_widget_get_window(GTK_WIDGET(mContainer)), &x, &y);
        aRect.MoveTo(GdkPointToDevicePixels({ x, y }));
    } else {
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

LayoutDeviceIntSize
nsWindow::GetClientSize()
{
  return LayoutDeviceIntSize(mBounds.width, mBounds.height);
}

NS_IMETHODIMP
nsWindow::GetClientBounds(LayoutDeviceIntRect& aRect)
{
    // GetBounds returns a rect whose top left represents the top left of the
    // outer bounds, but whose width/height represent the size of the inner
    // bounds (which is messed up).
    GetBounds(aRect);
    aRect.MoveBy(GetClientOffset());

    return NS_OK;
}

void
nsWindow::UpdateClientOffset()
{
    PROFILER_LABEL("nsWindow", "UpdateClientOffset", js::ProfileEntry::Category::GRAPHICS);

    if (!mIsTopLevel || !mShell || !mGdkWindow ||
        gtk_window_get_window_type(GTK_WINDOW(mShell)) == GTK_WINDOW_POPUP) {
        mClientOffset = nsIntPoint(0, 0);
        return;
    }

    GdkAtom cardinal_atom = gdk_x11_xatom_to_atom(XA_CARDINAL);

    GdkAtom type_returned;
    int format_returned;
    int length_returned;
    long *frame_extents;

    if (!gdk_property_get(mGdkWindow,
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
        mClientOffset = nsIntPoint(0, 0);
        return;
    }

    // data returned is in the order left, right, top, bottom
    int32_t left = int32_t(frame_extents[0]);
    int32_t top = int32_t(frame_extents[2]);

    g_free(frame_extents);

    mClientOffset = nsIntPoint(left, top);
}

LayoutDeviceIntPoint
nsWindow::GetClientOffset()
{
    return LayoutDeviceIntPoint::FromUnknownPoint(mClientOffset);
}

gboolean
nsWindow::OnPropertyNotifyEvent(GtkWidget* aWidget, GdkEventProperty* aEvent)

{
  if (aEvent->atom == gdk_atom_intern("_NET_FRAME_EXTENTS", FALSE)) {
    UpdateClientOffset();
    return FALSE;
  }

  if (GetCurrentTimeGetter()->PropertyNotifyHandler(aWidget, aEvent)) {
    return TRUE;
  }

  return FALSE;
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
    if (aCursor != mCursor || mUpdateCursor) {
        GdkCursor *newCursor = nullptr;
        mUpdateCursor = false;

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
#if (MOZ_WIDGET_GTK == 3)
        g_object_unref(cursor);
#else
        gdk_cursor_unref(cursor);
#endif
    }

    return rv;
}

NS_IMETHODIMP
nsWindow::Invalidate(const LayoutDeviceIntRect& aRect)
{
    if (!mGdkWindow)
        return NS_OK;

    GdkRectangle rect = DevicePixelsToGdkRectRoundOut(aRect);
    gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);

    LOGDRAW(("Invalidate (rect) [%p]: %d %d %d %d\n", (void *)this,
             rect.x, rect.y, rect.width, rect.height));

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
    }

    case NS_NATIVE_PLUGIN_PORT:
        return SetupPluginPort();

    case NS_NATIVE_PLUGIN_ID:
        if (!mPluginNativeWindow) {
          NS_WARNING("no native plugin instance!");
          return nullptr;
        }
        // Return the socket widget XID
        return (void*)mPluginNativeWindow->window;

    case NS_NATIVE_DISPLAY: {
#ifdef MOZ_X11
        GdkDisplay* gdkDisplay = gdk_display_get_default();
        if (GDK_IS_X11_DISPLAY(gdkDisplay)) {
          return GDK_DISPLAY_XDISPLAY(gdkDisplay);
        }
#endif /* MOZ_X11 */
        return nullptr;
    }
    case NS_NATIVE_SHELLWIDGET:
        return GetToplevelWidget();

    case NS_NATIVE_SHAREABLE_WINDOW:
        return (void *) GDK_WINDOW_XID(gdk_window_get_toplevel(mGdkWindow));
    case NS_NATIVE_PLUGIN_OBJECT_PTR:
        return (void *) mPluginNativeWindow;
    case NS_RAW_NATIVE_IME_CONTEXT: {
        void* pseudoIMEContext = GetPseudoIMEContext();
        if (pseudoIMEContext) {
            return pseudoIMEContext;
        }
        // If IME context isn't available on this widget, we should set |this|
        // instead of nullptr.
        if (!mIMContext) {
            return this;
        }
        return mIMContext.get();
    }
    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nullptr;
    }
}

void
nsWindow::SetNativeData(uint32_t aDataType, uintptr_t aVal)
{
    if (aDataType != NS_NATIVE_PLUGIN_OBJECT_PTR) {
        NS_WARNING("nsWindow::SetNativeData called with bad value");
        return;
    }
    mPluginNativeWindow = (nsPluginNativeWindowGtk*)aVal;
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
                GdkPixbuf *icon = gdk_pixbuf_new_from_file(path.get(), nullptr);
                if (icon) {
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


LayoutDeviceIntPoint
nsWindow::WidgetToScreenOffset()
{
    gint x = 0, y = 0;

    if (mGdkWindow) {
        gdk_window_get_origin(mGdkWindow, &x, &y);
    }

    return GdkPointToDevicePixels({ x, y });
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

    if (!mContainer)
        return NS_ERROR_FAILURE;

    if (aCapture) {
        gtk_grab_add(GTK_WIDGET(mContainer));
        GrabPointer(GetLastUserInputTime());
    }
    else {
        ReleaseGrabs();
        gtk_grab_remove(GTK_WIDGET(mContainer));
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
                              bool               aDoCapture)
{
    if (!mGdkWindow)
        return NS_OK;

    if (!mContainer)
        return NS_ERROR_FAILURE;

    LOG(("CaptureRollupEvents %p %i\n", this, int(aDoCapture)));

    if (aDoCapture) {
        gRollupListener = aListener;
        // Don't add a grab if a drag is in progress, or if the widget is a drag
        // feedback popup. (panels with type="drag").
        if (!mIsDragPopup && !nsWindow::DragInProgress()) {
            gtk_grab_add(GTK_WIDGET(mContainer));
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
        gtk_grab_remove(GTK_WIDGET(mContainer));
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
    bool haveEvent = false;
#ifdef MOZ_X11
    XEvent ev;
    GdkDisplay* gdkDisplay = gdk_display_get_default();
    if (GDK_IS_X11_DISPLAY(gdkDisplay)) {
        Display *display = GDK_DISPLAY_XDISPLAY(gdkDisplay);
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
    }
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
  gdk_window_get_geometry(aGdkWindow,nullptr,nullptr,&width,&height,nullptr);
#else
  gdk_window_get_geometry(aGdkWindow,nullptr,nullptr,&width,&height);
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
static bool
ExtractExposeRegion(LayoutDeviceIntRegion& aRegion, GdkEventExpose* aEvent)
{
  GdkRectangle* rects;
  gint nrects;
  gdk_region_get_rectangles(aEvent->region, &rects, &nrects);

  if (nrects > MAX_RECTS_IN_REGION) {
      // Just use the bounding box
      rects[0] = aEvent->area;
      nrects = 1;
  }

  for (GdkRectangle* r = rects; r < rects + nrects; r++) {
      aRegion.Or(aRegion, LayoutDeviceIntRect(r->x, r->y, r->width, r->height));
      LOGDRAW(("\t%d %d %d %d\n", r->x, r->y, r->width, r->height));
  }

  g_free(rects);
  return true;
}

#else
# ifdef cairo_copy_clip_rectangle_list
#  error "Looks like we're including Mozilla's cairo instead of system cairo"
# endif
static bool
ExtractExposeRegion(LayoutDeviceIntRegion& aRegion, cairo_t* cr)
{
  cairo_rectangle_list_t* rects = cairo_copy_clip_rectangle_list(cr);
  if (rects->status != CAIRO_STATUS_SUCCESS) {
      NS_WARNING("Failed to obtain cairo rectangle list.");
      return false;
  }

  for (int i = 0; i < rects->num_rectangles; i++)  {
      const cairo_rectangle_t& r = rects->rectangles[i];
      aRegion.Or(aRegion, LayoutDeviceIntRect(r.x, r.y, r.width, r.height));
      LOGDRAW(("\t%d %d %d %d\n", r.x, r.y, r.width, r.height));
  }

  cairo_rectangle_list_destroy(rects);
  return true;
}
#endif

#if (MOZ_WIDGET_GTK == 2)
gboolean
nsWindow::OnExposeEvent(GdkEventExpose *aEvent)
#else
gboolean
nsWindow::OnExposeEvent(cairo_t *cr)
#endif
{
    // Send any pending resize events so that layout can update.
    // May run event loop.
    MaybeDispatchResized();

    if (mIsDestroyed) {
        return FALSE;
    }

    // Windows that are not visible will be painted after they become visible.
    if (!mGdkWindow || mIsFullyObscured || !mHasMappedToplevel)
        return FALSE;

    nsIWidgetListener *listener = GetListener();
    if (!listener)
        return FALSE;

    LayoutDeviceIntRegion exposeRegion;
#if (MOZ_WIDGET_GTK == 2)
    if (!ExtractExposeRegion(exposeRegion, aEvent)) {
#else
    if (!ExtractExposeRegion(exposeRegion, cr)) {
#endif
        return FALSE;
    }

    gint scale = GdkScaleFactor();
    LayoutDeviceIntRegion region = exposeRegion;
    region.ScaleRoundOut(scale, scale);

    ClientLayerManager *clientLayers =
        (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT)
        ? static_cast<ClientLayerManager*>(GetLayerManager())
        : nullptr;

    if (clientLayers && mCompositorBridgeParent) {
        // We need to paint to the screen even if nothing changed, since if we
        // don't have a compositing window manager, our pixels could be stale.
        clientLayers->SetNeedsComposite(true);
        clientLayers->SendInvalidRegion(region.ToUnknownRegion());
    }

    RefPtr<nsWindow> strongThis(this);

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
        listener = GetListener();
        if (!listener)
            return FALSE;
    }

    if (clientLayers && mCompositorBridgeParent && clientLayers->NeedsComposite()) {
        mCompositorBridgeParent->ScheduleRenderOnCompositorThread();
        clientLayers->SetNeedsComposite(false);
    }

    LOGDRAW(("sending expose event [%p] %p 0x%lx (rects follow):\n",
             (void *)this, (void *)mGdkWindow,
             gdk_x11_window_get_xid(mGdkWindow)));

    // Our bounds may have changed after calling WillPaintWindow.  Clip
    // to the new bounds here.  The region is relative to this
    // window.
    region.And(region, LayoutDeviceIntRect(0, 0, mBounds.width, mBounds.height));

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
                AutoTArray<LayoutDeviceIntRect,1> clipRects;
                kid->GetWindowClipRegion(&clipRects);
                LayoutDeviceIntRect bounds;
                kid->GetBounds(bounds);
                for (uint32_t i = 0; i < clipRects.Length(); ++i) {
                    LayoutDeviceIntRect r = clipRects[i] + bounds.TopLeft();
                    region.Sub(region, r);
                }
            }
            children = children->next;
        }
    }

    if (region.IsEmpty()) {
        return TRUE;
    }

    // If this widget uses OMTC...
    if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
        listener->PaintWindow(this, region);

        // Re-get the listener since the will paint notification might have
        // killed it.
        listener = GetListener();
        if (!listener)
            return TRUE;

        listener->DidPaintWindow();
        return TRUE;
    }

    BufferMode layerBuffering = BufferMode::BUFFERED;
    RefPtr<DrawTarget> dt = GetDrawTarget(region, &layerBuffering);
    if (!dt || !dt->IsValid()) {
        return FALSE;
    }
    RefPtr<gfxContext> ctx;
    IntRect boundsRect = region.GetBounds().ToUnknownRect();
    IntPoint offset(0, 0);
    if (dt->GetSize() == boundsRect.Size()) {
      offset = boundsRect.TopLeft();
      dt->SetTransform(Matrix::Translation(-offset));
    }

#ifdef MOZ_X11
    if (shaped) {
        // Collapse update area to the bounding box. This is so we only have to
        // call UpdateTranslucentWindowAlpha once. After we have dropped
        // support for non-Thebes graphics, UpdateTranslucentWindowAlpha will be
        // our private interface so we can rework things to avoid this.
        dt->PushClipRect(Rect(boundsRect));

        // The double buffering is done here to extract the shape mask.
        // (The shape mask won't be necessary when a visual with an alpha
        // channel is used on compositing window managers.)
        layerBuffering = BufferMode::BUFFER_NONE;
        RefPtr<DrawTarget> destDT = dt->CreateSimilarDrawTarget(boundsRect.Size(), SurfaceFormat::B8G8R8A8);
        if (!destDT || !destDT->IsValid()) {
            return FALSE;
        }
        ctx = gfxContext::ForDrawTarget(destDT, boundsRect.TopLeft());
    } else {
        gfxUtils::ClipToRegion(dt, region.ToUnknownRegion());

        ctx = gfxContext::ForDrawTarget(dt, offset);
    }
    MOZ_ASSERT(ctx); // checked both dt and destDT valid draw target above

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
      if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC) {
        GdkScreen *screen = gdk_window_get_screen(mGdkWindow);
        if (GetTransparencyMode() == eTransparencyTransparent &&
            layerBuffering == BufferMode::BUFFER_NONE &&
            gdk_screen_is_composited(screen) &&
            gdk_window_get_visual(mGdkWindow) ==
            gdk_screen_get_rgba_visual(screen)) {
          // If our draw target is unbuffered and we use an alpha channel,
          // clear the image beforehand to ensure we don't get artifacts from a
          // reused SHM image. See bug 1258086.
          dt->ClearRect(Rect(boundsRect));
        }
        AutoLayerManagerSetup setupLayerManager(this, ctx, layerBuffering);
        painted = listener->PaintWindow(this, region);

        // Re-get the listener since the will paint notification might have
        // killed it.
        listener = GetListener();
        if (!listener)
            return TRUE;

      }
    }

#ifdef MOZ_X11
    // PaintWindow can Destroy us (bug 378273), avoid doing any paint
    // operations below if that happened - it will lead to XError and exit().
    if (shaped) {
        if (MOZ_LIKELY(!mIsDestroyed)) {
            if (painted) {
                RefPtr<SourceSurface> surf = ctx->GetDrawTarget()->Snapshot();

                UpdateAlpha(surf, boundsRect);

                dt->DrawSurface(surf, Rect(boundsRect), Rect(0, 0, boundsRect.width, boundsRect.height),
                                DrawSurfaceOptions(Filter::POINT), DrawOptions(1.0f, CompositionOp::OP_SOURCE));
            }
        }
    }

    ctx = nullptr;
    dt->PopClip();

#  ifdef MOZ_HAVE_SHMIMAGE
    if (mBackShmImage && MOZ_LIKELY(!mIsDestroyed)) {
      mBackShmImage->Put(region);
    }
#  endif  // MOZ_HAVE_SHMIMAGE
#endif // MOZ_X11

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
nsWindow::UpdateAlpha(SourceSurface* aSourceSurface, nsIntRect aBoundsRect)
{
    // We need to create our own buffer to force the stride to match the
    // expected stride.
    int32_t stride = GetAlignedStride<4>(BytesPerPixel(SurfaceFormat::A8) *
                                         aBoundsRect.width);
    int32_t bufferSize = stride * aBoundsRect.height;
    auto imageBuffer = MakeUniqueFallible<uint8_t[]>(bufferSize);
    {
        RefPtr<DrawTarget> drawTarget = gfxPlatform::GetPlatform()->
            CreateDrawTargetForData(imageBuffer.get(), aBoundsRect.Size(),
                                    stride, SurfaceFormat::A8);

        if (drawTarget) {
            drawTarget->DrawSurface(aSourceSurface, Rect(0, 0, aBoundsRect.width, aBoundsRect.height),
                                    Rect(0, 0, aSourceSurface->GetSize().width, aSourceSurface->GetSize().height),
                                    DrawSurfaceOptions(Filter::POINT), DrawOptions(1.0f, CompositionOp::OP_SOURCE));
        }
    }
    UpdateTranslucentWindowAlphaInternal(aBoundsRect, imageBuffer.get(), stride);
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

    LayoutDeviceIntRect screenBounds;
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

    NS_ASSERTION(GTK_IS_WINDOW(aWidget),
                 "Configure event on widget that is not a GtkWindow");
    if (gtk_window_get_window_type(GTK_WINDOW(aWidget)) == GTK_WINDOW_POPUP) {
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
    NotifyWindowMoved(mBounds.x, mBounds.y);

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

        g_object_set_data(G_OBJECT(mGdkWindow), "nsWindow", nullptr);
        mGdkWindow = nullptr;
    }
}

void
nsWindow::OnSizeAllocate(GtkAllocation *aAllocation)
{
    LOG(("size_allocate [%p] %d %d %d %d\n",
         (void *)this, aAllocation->x, aAllocation->y,
         aAllocation->width, aAllocation->height));

    LayoutDeviceIntSize size = GdkRectToDevicePixels(*aAllocation).Size();

    if (mBounds.Size() == size)
        return;

    // Invalidate the new part of the window now for the pending paint to
    // minimize background flashes (GDK does not do this for external resizes
    // of toplevels.)
    if (mBounds.width < size.width) {
        GdkRectangle rect = DevicePixelsToGdkRectRoundOut(
            LayoutDeviceIntRect(mBounds.width, 0,
                                size.width - mBounds.width, size.height));
        gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);
    }
    if (mBounds.height < size.height) {
        GdkRectangle rect = DevicePixelsToGdkRectRoundOut(
            LayoutDeviceIntRect(0, mBounds.height,
                                size.width, size.height - mBounds.height));
        gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);
    }

    mBounds.SizeTo(size);

    // Gecko permits running nested event loops during processing of events,
    // GtkWindow callers of gtk_widget_size_allocate expect the signal
    // handlers to return sometime in the near future.
    mNeedsDispatchResized = true;
    NS_DispatchToCurrentThread(NewRunnableMethod(this, &nsWindow::MaybeDispatchResized));
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
    if (aEvent->subwindow != nullptr)
        return;

    // Check before is_parent_ungrab_enter() as the button state may have
    // changed while a non-Gecko ancestor window had a pointer grab.
    DispatchMissedButtonReleases(aEvent);

    if (is_parent_ungrab_enter(aEvent))
        return;

    WidgetMouseEvent event(true, eMouseEnterIntoWidget, this,
                           WidgetMouseEvent::eReal);

    event.mRefPoint = GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
    event.AssignEventTime(GetWidgetEventTime(aEvent->time));

    LOG(("OnEnterNotify: %p\n", (void *)this));

    DispatchInputEvent(&event);
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
    if (aEvent->subwindow != nullptr)
        return;

    WidgetMouseEvent event(true, eMouseExitFromWidget, this,
                           WidgetMouseEvent::eReal);

    event.mRefPoint = GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
    event.AssignEventTime(GetWidgetEventTime(aEvent->time));

    event.mExitFrom = is_top_level_mouse_exit(mGdkWindow, aEvent)
        ? WidgetMouseEvent::eTopLevel : WidgetMouseEvent::eChild;

    LOG(("OnLeaveNotify: %p\n", (void *)this));

    DispatchInputEvent(&event);
}

template <typename Event> static LayoutDeviceIntPoint
GetRefPoint(nsWindow* aWindow, Event* aEvent)
{
    if (aEvent->window == aWindow->GetGdkWindow()) {
        // we are the window that the event happened on so no need for expensive WidgetToScreenOffset
        return aWindow->GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
    }
    // XXX we're never quite sure which GdkWindow the event came from due to our custom bubbling
    // in scroll_event_cb(), so use ScreenToWidget to translate the screen root coordinates into
    // coordinates relative to this widget.
    return aWindow->GdkEventCoordsToDevicePixels(
        aEvent->x_root, aEvent->y_root) - aWindow->WidgetToScreenOffset();
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

    bool isX11Display = GDK_IS_X11_DISPLAY(gdk_display_get_default());
    if (isX11Display) {
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
            RefPtr<nsWindow> kungFuDeathGrip = gPluginFocusWindow;
            gPluginFocusWindow->LoseNonXEmbedPluginFocus();
        }
#endif /* MOZ_WIDGET_GTK == 2 */
    }
#endif /* MOZ_X11 */

    WidgetMouseEvent event(true, eMouseMove, this, WidgetMouseEvent::eReal);

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
        event.mRefPoint.x = nscoord(xevent.xmotion.x);
        event.mRefPoint.y = nscoord(xevent.xmotion.y);

        modifierState = xevent.xmotion.state;

        event.AssignEventTime(GetWidgetEventTime(xevent.xmotion.time));
#else
        event.mRefPoint = GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);

        modifierState = aEvent->state;

        event.AssignEventTime(GetWidgetEventTime(aEvent->time));
#endif /* MOZ_X11 */
    } else {
        event.mRefPoint = GetRefPoint(this, aEvent);

        modifierState = aEvent->state;

        event.AssignEventTime(GetWidgetEventTime(aEvent->time));
    }

    KeymapWrapper::InitInputEvent(event, modifierState);

    DispatchInputEvent(&event);
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
            WidgetMouseEvent synthEvent(true, eMouseUp, this,
                                        WidgetMouseEvent::eSynthesized);
            synthEvent.button = buttonType;
            DispatchInputEvent(&synthEvent);
        }
    }
}

void
nsWindow::InitButtonEvent(WidgetMouseEvent& aEvent,
                          GdkEventButton* aGdkEvent)
{
    aEvent.mRefPoint = GetRefPoint(this, aGdkEvent);

    guint modifierState = aGdkEvent->state;
    // aEvent's state includes the button state from immediately before this
    // event.  If aEvent is a mousedown or mouseup event, we need to update
    // the button state.
    guint buttonMask = 0;
    switch (aGdkEvent->button) {
        case 1:
            buttonMask = GDK_BUTTON1_MASK;
            break;
        case 2:
            buttonMask = GDK_BUTTON2_MASK;
            break;
        case 3:
            buttonMask = GDK_BUTTON3_MASK;
            break;
    }
    if (aGdkEvent->type == GDK_BUTTON_RELEASE) {
        modifierState &= ~buttonMask;
    } else {
        modifierState |= buttonMask;
    }

    KeymapWrapper::InitInputEvent(aEvent, modifierState);

    aEvent.AssignEventTime(GetWidgetEventTime(aGdkEvent->time));

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

    WidgetMouseEvent event(true, eMouseDown, this, WidgetMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent);
    event.pressure = mLastMotionPressure;

    DispatchInputEvent(&event);

    // right menu click on linux should also pop up a context menu
    if (domButton == WidgetMouseEvent::eRightButton &&
        MOZ_LIKELY(!mIsDestroyed)) {
        WidgetMouseEvent contextMenuEvent(true, eContextMenu, this,
                                          WidgetMouseEvent::eReal);
        InitButtonEvent(contextMenuEvent, aEvent);
        contextMenuEvent.pressure = mLastMotionPressure;
        DispatchInputEvent(&contextMenuEvent);
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

    WidgetMouseEvent event(true, eMouseUp, this,
                           WidgetMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent);
    gdouble pressure = 0;
    gdk_event_get_axis ((GdkEvent*)aEvent, GDK_AXIS_PRESSURE, &pressure);
    event.pressure = pressure ? pressure : mLastMotionPressure;

    DispatchInputEvent(&event);
    mLastMotionPressure = pressure;
}

void
nsWindow::OnContainerFocusInEvent(GdkEventFocus *aEvent)
{
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
        // setting a non-nullptr value here prevents OnButtonPressEvent() from
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
        RefPtr<nsWindow> kungFuDeathGrip = gPluginFocusWindow;
        gPluginFocusWindow->LoseNonXEmbedPluginFocus();
    }
#endif /* MOZ_X11 && MOZ_WIDGET_GTK == 2 */

    if (gFocusWindow) {
        RefPtr<nsWindow> kungFuDeathGrip = gFocusWindow;
        if (gFocusWindow->mIMContext) {
            gFocusWindow->mIMContext->OnBlurWindow(gFocusWindow);
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
nsWindow::DispatchContentCommandEvent(EventMessage aMsg)
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

    RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
    nsresult rv = dispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return FALSE;
    }

    WidgetKeyboardEvent keydownEvent(true, eKeyDown, this);
    KeymapWrapper::InitKeyEvent(keydownEvent, aEvent);
    nsEventStatus status = nsEventStatus_eIgnore;
    bool dispatched =
        dispatcher->DispatchKeyboardEvent(eKeyDown, keydownEvent,
                                          status, aEvent);
    *aCancelled = (status == nsEventStatus_eConsumeNoDefault);
    return dispatched ? TRUE : FALSE;
}

WidgetEventTime
nsWindow::GetWidgetEventTime(guint32 aEventTime)
{
  return WidgetEventTime(aEventTime, GetEventTimeStamp(aEventTime));
}

TimeStamp
nsWindow::GetEventTimeStamp(guint32 aEventTime)
{
    if (MOZ_UNLIKELY(!mGdkWindow)) {
        // nsWindow has been Destroy()ed.
        return TimeStamp::Now();
    }
    if (aEventTime == 0) {
        // Some X11 and GDK events may be received with a time of 0 to indicate
        // that they are synthetic events. Some input method editors do this.
        // In this case too, just return the current timestamp.
        return TimeStamp::Now();
    }
    CurrentX11TimeGetter* getCurrentTime = GetCurrentTimeGetter();
    MOZ_ASSERT(getCurrentTime,
               "Null current time getter despite having a window");
    return TimeConverter().GetTimeStampFromSystemTime(aEventTime,
                                                      *getCurrentTime);
}

mozilla::CurrentX11TimeGetter*
nsWindow::GetCurrentTimeGetter() {
    MOZ_ASSERT(mGdkWindow, "Expected mGdkWindow to be set");
    if (MOZ_UNLIKELY(!mCurrentTimeGetter)) {
        mCurrentTimeGetter = MakeUnique<CurrentX11TimeGetter>(mGdkWindow);
    }
    return mCurrentTimeGetter.get();
}

gboolean
nsWindow::OnKeyPressEvent(GdkEventKey *aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));

    // if we are in the middle of composing text, XIM gets to see it
    // before mozilla does.
    // FYI: Don't dispatch keydown event before notifying IME of the event
    //      because IME may send a key event synchronously and consume the
    //      original event.
    bool IMEWasEnabled = false;
    if (mIMContext) {
        IMEWasEnabled = mIMContext->IsEnabled();
        if (mIMContext->OnKeyEvent(this, aEvent)) {
            return TRUE;
        }
    }

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
    if (!IMEWasEnabled && mIMContext && mIMContext->IsEnabled()) {
        // Notice our keydown event was already dispatched.  This prevents
        // unnecessary DOM keydown event in the editor.
        if (mIMContext->OnKeyEvent(this, aEvent, true)) {
            return TRUE;
        }
    }

    // Look for specialized app-command keys
    switch (aEvent->keyval) {
        case GDK_Back:
            return DispatchCommandEvent(nsGkAtoms::Back);
        case GDK_Forward:
            return DispatchCommandEvent(nsGkAtoms::Forward);
        case GDK_Refresh:
            return DispatchCommandEvent(nsGkAtoms::Reload);
        case GDK_Stop:
            return DispatchCommandEvent(nsGkAtoms::Stop);
        case GDK_Search:
            return DispatchCommandEvent(nsGkAtoms::Search);
        case GDK_Favorites:
            return DispatchCommandEvent(nsGkAtoms::Bookmarks);
        case GDK_HomePage:
            return DispatchCommandEvent(nsGkAtoms::Home);
        case GDK_Copy:
        case GDK_F16:  // F16, F20, F18, F14 are old keysyms for Copy Cut Paste Undo
            return DispatchContentCommandEvent(eContentCommandCopy);
        case GDK_Cut:
        case GDK_F20:
            return DispatchContentCommandEvent(eContentCommandCut);
        case GDK_Paste:
        case GDK_F18:
            return DispatchContentCommandEvent(eContentCommandPaste);
        case GDK_Redo:
            return DispatchContentCommandEvent(eContentCommandRedo);
        case GDK_Undo:
        case GDK_F14:
            return DispatchContentCommandEvent(eContentCommandUndo);
    }

    WidgetKeyboardEvent keypressEvent(true, eKeyPress, this);
    KeymapWrapper::InitKeyEvent(keypressEvent, aEvent);

    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (is_context_menu_key(keypressEvent)) {
        WidgetMouseEvent contextMenuEvent(true, eContextMenu, this,
                                          WidgetMouseEvent::eReal,
                                          WidgetMouseEvent::eContextMenuKey);

        contextMenuEvent.mRefPoint = LayoutDeviceIntPoint(0, 0);
        contextMenuEvent.AssignEventTime(GetWidgetEventTime(aEvent->time));
        contextMenuEvent.clickCount = 1;
        KeymapWrapper::InitInputEvent(contextMenuEvent, aEvent->state);
        DispatchInputEvent(&contextMenuEvent);
    } else {
        RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
        nsresult rv = dispatcher->BeginNativeInputTransaction();
        if (NS_WARN_IF(NS_FAILED(rv))) {
            return TRUE;
        }

        // If the character code is in the BMP, send the key press event.
        // Otherwise, send a compositionchange event with the equivalent UTF-16
        // string.
        // TODO: Investigate other browser's behavior in this case because
        //       this hack is odd for UI Events.
        nsEventStatus status = nsEventStatus_eIgnore;
        if (keypressEvent.mKeyNameIndex != KEY_NAME_INDEX_USE_STRING ||
            keypressEvent.mKeyValue.Length() == 1) {
            dispatcher->MaybeDispatchKeypressEvents(keypressEvent,
                                                    status, aEvent);
        } else {
            WidgetEventTime eventTime = GetWidgetEventTime(aEvent->time);
            dispatcher->CommitComposition(status, &keypressEvent.mKeyValue,
                                          &eventTime);
        }
    }

    return TRUE;
}

gboolean
nsWindow::OnKeyReleaseEvent(GdkEventKey *aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));

    if (mIMContext && mIMContext->OnKeyEvent(this, aEvent)) {
        return TRUE;
    }

    RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
    nsresult rv = dispatcher->BeginNativeInputTransaction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
    }

    WidgetKeyboardEvent keyupEvent(true, eKeyUp, this);
    KeymapWrapper::InitKeyEvent(keyupEvent, aEvent);
    nsEventStatus status = nsEventStatus_eIgnore;
    dispatcher->DispatchKeyboardEvent(eKeyUp, keyupEvent, status, aEvent);

    return TRUE;
}

void
nsWindow::OnScrollEvent(GdkEventScroll *aEvent)
{
    // check to see if we should rollup
    if (CheckForRollup(aEvent->x_root, aEvent->y_root, true, false))
        return;
#if GTK_CHECK_VERSION(3,4,0)
    // check for duplicate legacy scroll event, see GNOME bug 726878
    if (aEvent->direction != GDK_SCROLL_SMOOTH &&
        mLastScrollEventTime == aEvent->time)
        return; 
#endif
    WidgetWheelEvent wheelEvent(true, eWheel, this);
    wheelEvent.mDeltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;
    switch (aEvent->direction) {
#if GTK_CHECK_VERSION(3,4,0)
    case GDK_SCROLL_SMOOTH:
    {
        // As of GTK 3.4, all directional scroll events are provided by
        // the GDK_SCROLL_SMOOTH direction on XInput2 devices.
        mLastScrollEventTime = aEvent->time;
        // TODO - use a more appropriate scrolling unit than lines.
        // Multiply event deltas by 3 to emulate legacy behaviour.
        wheelEvent.mDeltaX = aEvent->delta_x * 3;
        wheelEvent.mDeltaY = aEvent->delta_y * 3;
        wheelEvent.mIsNoLineOrPageDelta = true;
        // This next step manually unsets smooth scrolling for touch devices 
        // that trigger GDK_SCROLL_SMOOTH. We use the slave device, which 
        // represents the actual input.
        GdkDevice *device = gdk_event_get_source_device((GdkEvent*)aEvent);
        GdkInputSource source = gdk_device_get_source(device);
        if (source == GDK_SOURCE_TOUCHSCREEN ||
            source == GDK_SOURCE_TOUCHPAD) {
            wheelEvent.mScrollType = WidgetWheelEvent::SCROLL_ASYNCHRONOUSELY;
        }
        break;
    }
#endif
    case GDK_SCROLL_UP:
        wheelEvent.mDeltaY = wheelEvent.mLineOrPageDeltaY = -3;
        break;
    case GDK_SCROLL_DOWN:
        wheelEvent.mDeltaY = wheelEvent.mLineOrPageDeltaY = 3;
        break;
    case GDK_SCROLL_LEFT:
        wheelEvent.mDeltaX = wheelEvent.mLineOrPageDeltaX = -1;
        break;
    case GDK_SCROLL_RIGHT:
        wheelEvent.mDeltaX = wheelEvent.mLineOrPageDeltaX = 1;
        break;
    }

    wheelEvent.mRefPoint = GetRefPoint(this, aEvent);

    KeymapWrapper::InitInputEvent(wheelEvent, aEvent->state);

    wheelEvent.AssignEventTime(GetWidgetEventTime(aEvent->time));

    DispatchInputEvent(&wheelEvent);
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
            gdk_window_invalidate_rect(mGdkWindow, nullptr, FALSE);
        }

        mIsFullyObscured = false;

        // if we have to retry the grab, retry it.
        EnsureGrabs();
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

    if (mWidgetListener) {
      mWidgetListener->SizeModeChanged(mSizeState);
      if (aEvent->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        mWidgetListener->FullscreenChanged(
          aEvent->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);
      }
    }
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
            RefPtr<nsWindow> kungFuDeathGrip = win;
            win->ThemeChanged();
        }

        children = children->next;
    }
}

void
nsWindow::OnDPIChanged()
{
  if (mWidgetListener) {
    nsIPresShell* presShell = mWidgetListener->GetPresShell();
    if (presShell) {
      presShell->BackingScaleFactorChanged();
      // Update menu's font size etc
      presShell->ThemeChanged();
    }
  }
}

void
nsWindow::DispatchDragEvent(EventMessage aMsg, const LayoutDeviceIntPoint& aRefPoint,
                            guint aTime)
{
    WidgetDragEvent event(true, aMsg, this);

    if (aMsg == eDragOver) {
        InitDragEvent(event);
    }

    event.mRefPoint = aRefPoint;
    event.AssignEventTime(GetWidgetEventTime(aTime));

    DispatchInputEvent(&event);
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

#if GTK_CHECK_VERSION(3,4,0)
gboolean
nsWindow::OnTouchEvent(GdkEventTouch* aEvent)
{
    if (!mHandleTouchEvent) {
        return FALSE;
    }

    EventMessage msg;
    switch (aEvent->type) {
    case GDK_TOUCH_BEGIN:
        msg = eTouchStart;
        break;
    case GDK_TOUCH_UPDATE:
        msg = eTouchMove;
        break;
    case GDK_TOUCH_END:
        msg = eTouchEnd;
        break;
    case GDK_TOUCH_CANCEL:
        msg = eTouchCancel;
        break;
    default:
        return FALSE;
    }

    LayoutDeviceIntPoint touchPoint = GetRefPoint(this, aEvent);

    int32_t id;
    RefPtr<dom::Touch> touch;
    if (mTouches.Remove(aEvent->sequence, getter_AddRefs(touch))) {
        id = touch->mIdentifier;
    } else {
        id = ++gLastTouchID & 0x7FFFFFFF;
    }

    touch = new dom::Touch(id, touchPoint, LayoutDeviceIntPoint(1, 1),
                           0.0f, 0.0f);

    WidgetTouchEvent event(true, msg, this);
    KeymapWrapper::InitInputEvent(event, aEvent->state);
    event.mTime = aEvent->time;

    if (aEvent->type == GDK_TOUCH_BEGIN || aEvent->type == GDK_TOUCH_UPDATE) {
        mTouches.Put(aEvent->sequence, touch.forget());
        // add all touch points to event object
        for (auto iter = mTouches.Iter(); !iter.Done(); iter.Next()) {
            event.mTouches.AppendElement(new dom::Touch(*iter.UserData()));
        }
    } else if (aEvent->type == GDK_TOUCH_END ||
               aEvent->type == GDK_TOUCH_CANCEL) {
        *event.mTouches.AppendElement() = touch.forget();
    }

    DispatchInputEvent(&event);
    return TRUE;
}
#endif

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
            MOZ_UTF16("brandShortName"),
            getter_Copies(brandName));

    if (brandName.IsEmpty())
        brandName.AssignLiteral(MOZ_UTF16("Mozilla"));
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
    gdk_window_set_back_pixmap(window, nullptr, FALSE);
#endif

    return window;
}

nsresult
nsWindow::Create(nsIWidget* aParent,
                 nsNativeWidget aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
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
    BaseCreate(baseParent, aInitData);

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
    bool            shellHasCSD = false;

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

        // Popups that are not noautohide are only temporary. The are used
        // for menus and the like and disappear when another window is used.
        // For most popups, use the standard GtkWindowType GTK_WINDOW_POPUP,
        // which will use a Window with the override-redirect attribute
        // (for temporary windows).
        // For long-lived windows, their stacking order is managed by the
        // window manager, as indicated by GTK_WINDOW_TOPLEVEL ...
        GtkWindowType type =
            mWindowType != eWindowType_popup || aInitData->mNoAutoHide ?
              GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP;
        mShell = gtk_window_new(type);

        // We only move a general managed toplevel window if someone has
        // actually placed the window somewhere.  If no placement has taken
        // place, we just let the window manager Do The Right Thing.
        NativeResize();

        if (mWindowType == eWindowType_dialog) {
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
            NativeMove();

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
                                      popup_take_focus_filter, nullptr); 
#endif
            }

            GdkWindowTypeHint gtkTypeHint;
            if (aInitData->mIsDragPopup) {
                gtkTypeHint = GDK_WINDOW_TYPE_HINT_DND;
                mIsDragPopup = true;
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
            SetDefaultIcon();
            gtk_window_set_wmclass(GTK_WINDOW(mShell), "Toplevel", 
                                   gdk_get_program_class());

            // each toplevel window gets its own window group
            GtkWindowGroup *group = gtk_window_group_new();
            gtk_window_group_add_window(group, GTK_WINDOW(mShell));
            g_object_unref(group);
        }

        // Create a container to hold child windows and child GtkWidgets.
        GtkWidget *container = moz_container_new();
        mContainer = MOZ_CONTAINER(container);

#if (MOZ_WIDGET_GTK == 3)
        // "csd" style is set when widget is realized so we need to call
        // it explicitly now.
        gtk_widget_realize(mShell);

        // We can't draw directly to top-level window when client side
        // decorations are enabled. We use container with GdkWindow instead.
        GtkStyleContext* style = gtk_widget_get_style_context(mShell);
        shellHasCSD = gtk_style_context_has_class(style, "csd");
#endif
        if (!shellHasCSD) {
            // Use mShell's window for drawing and events.
            gtk_widget_set_has_window(container, FALSE);
            // Prevent GtkWindow from painting a background to flicker.
            gtk_widget_set_app_paintable(mShell, TRUE);
        }
        // Set up event widget
        eventWidget = shellHasCSD ? container : mShell;
        gtk_widget_add_events(eventWidget, kEvents);

        gtk_container_add(GTK_CONTAINER(mShell), container);
        gtk_widget_realize(container);

        // make sure this is the focus widget in the container
        gtk_widget_show(container);
        gtk_widget_grab_focus(container);

        // the drawing window
        mGdkWindow = gtk_widget_get_window(eventWidget);

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
                  gdk_window_set_decorations(mGdkWindow, (GdkWMDecoration) wmd);
            }

            // If the popup ignores mouse events, set an empty input shape.
            if (aInitData->mMouseTransparent) {
#if (MOZ_WIDGET_GTK == 2)
              GdkRectangle rect = { 0, 0, 0, 0 };
              GdkRegion *region = gdk_region_rectangle(&rect);

              gdk_window_input_shape_combine_region(mGdkWindow, region, 0, 0);
              gdk_region_destroy(region);
#else
              cairo_rectangle_int_t rect = { 0, 0, 0, 0 };
              cairo_region_t *region = cairo_region_create_rectangle(&rect);

              gdk_window_input_shape_combine_region(mGdkWindow, region, 0, 0);
              cairo_region_destroy(region);
#endif
            }
        }
    }
        break;
    case eWindowType_plugin:
    case eWindowType_plugin_ipc_chrome:
    case eWindowType_plugin_ipc_content:
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
                         G_CALLBACK(configure_event_cb), nullptr);
        g_signal_connect(mShell, "delete_event",
                         G_CALLBACK(delete_event_cb), nullptr);
        g_signal_connect(mShell, "window_state_event",
                         G_CALLBACK(window_state_event_cb), nullptr);

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
                         G_CALLBACK(container_unrealize_cb), nullptr);
        g_signal_connect_after(mContainer, "size_allocate",
                               G_CALLBACK(size_allocate_cb), nullptr);
        g_signal_connect(mContainer, "hierarchy-changed",
                         G_CALLBACK(hierarchy_changed_cb), nullptr);
#if (MOZ_WIDGET_GTK == 3)
        g_signal_connect(mContainer, "notify::scale-factor",
                         G_CALLBACK(scale_changed_cb), nullptr);
#endif
        // Initialize mHasMappedToplevel.
        hierarchy_changed_cb(GTK_WIDGET(mContainer), nullptr);
        // Expose, focus, key, and drag events are sent even to GTK_NO_WINDOW
        // widgets.
#if (MOZ_WIDGET_GTK == 2)
        g_signal_connect(mContainer, "expose_event",
                         G_CALLBACK(expose_event_cb), nullptr);
#else
        g_signal_connect(G_OBJECT(mContainer), "draw",
                         G_CALLBACK(expose_event_cb), nullptr);
#endif
        g_signal_connect(mContainer, "focus_in_event",
                         G_CALLBACK(focus_in_event_cb), nullptr);
        g_signal_connect(mContainer, "focus_out_event",
                         G_CALLBACK(focus_out_event_cb), nullptr);
        g_signal_connect(mContainer, "key_press_event",
                         G_CALLBACK(key_press_event_cb), nullptr);
        g_signal_connect(mContainer, "key_release_event",
                         G_CALLBACK(key_release_event_cb), nullptr);

        gtk_drag_dest_set((GtkWidget *)mContainer,
                          (GtkDestDefaults)0,
                          nullptr,
                          0,
                          (GdkDragAction)0);

        g_signal_connect(mContainer, "drag_motion",
                         G_CALLBACK(drag_motion_event_cb), nullptr);
        g_signal_connect(mContainer, "drag_leave",
                         G_CALLBACK(drag_leave_event_cb), nullptr);
        g_signal_connect(mContainer, "drag_drop",
                         G_CALLBACK(drag_drop_event_cb), nullptr);
        g_signal_connect(mContainer, "drag_data_received",
                         G_CALLBACK(drag_data_received_event_cb), nullptr);

        GtkWidget *widgets[] = { GTK_WIDGET(mContainer),
                                 !shellHasCSD ? mShell : nullptr };
        for (size_t i = 0; i < ArrayLength(widgets) && widgets[i]; ++i) {
            // Visibility events are sent to the owning widget of the relevant
            // window but do not propagate to parent widgets so connect on
            // mShell (if it exists) as well as mContainer.
            g_signal_connect(widgets[i], "visibility-notify-event",
                             G_CALLBACK(visibility_notify_event_cb), nullptr);
            // Similarly double buffering is controlled by the window's owning
            // widget.  Disable double buffering for painting directly to the
            // X Window.
            gtk_widget_set_double_buffered(widgets[i], FALSE);
        }

        // We create input contexts for all containers, except for
        // toplevel popup windows
        if (mWindowType != eWindowType_popup) {
            mIMContext = new IMContextWrapper(this);
        }
    } else if (!mIMContext) {
        nsWindow *container = GetContainerWindow();
        if (container) {
            mIMContext = container->mIMContext;
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
                         G_CALLBACK(enter_notify_event_cb), nullptr);
        g_signal_connect(eventWidget, "leave-notify-event",
                         G_CALLBACK(leave_notify_event_cb), nullptr);
        g_signal_connect(eventWidget, "motion-notify-event",
                         G_CALLBACK(motion_notify_event_cb), nullptr);
        g_signal_connect(eventWidget, "button-press-event",
                         G_CALLBACK(button_press_event_cb), nullptr);
        g_signal_connect(eventWidget, "button-release-event",
                         G_CALLBACK(button_release_event_cb), nullptr);
        g_signal_connect(eventWidget, "property-notify-event",
                         G_CALLBACK(property_notify_event_cb), nullptr);
        g_signal_connect(eventWidget, "scroll-event",
                         G_CALLBACK(scroll_event_cb), nullptr);
#if GTK_CHECK_VERSION(3,4,0)
        g_signal_connect(eventWidget, "touch-event",
                         G_CALLBACK(touch_event_cb), nullptr);
#endif
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

#ifdef MOZ_X11
    if (mGdkWindow) {
      mXDisplay = GDK_WINDOW_XDISPLAY(mGdkWindow);
      mXWindow = gdk_x11_window_get_xid(mGdkWindow);

      GdkVisual* gdkVisual = gdk_window_get_visual(mGdkWindow);
      mXVisual = gdk_x11_visual_get_xvisual(gdkVisual);
      mXDepth = gdk_visual_get_depth(gdkVisual);
    }
#endif

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

  const char *role = nullptr;

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

  gdk_window_set_role(mGdkWindow, role);

#ifdef MOZ_X11
  GdkDisplay *display = gdk_display_get_default();
  if (GDK_IS_X11_DISPLAY(display)) {
      XClassHint *class_hint = XAllocClassHint();
      if (!class_hint) {
        free(res_name);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      class_hint->res_name = res_name;
      class_hint->res_class = const_cast<char*>(res_class);

      // Can't use gtk_window_set_wmclass() for this; it prints
      // a warning & refuses to make the change.
      XSetClassHint(GDK_DISPLAY_XDISPLAY(display),
                    gdk_x11_window_get_xid(mGdkWindow),
                    class_hint);
      XFree(class_hint);
  }
#endif /* MOZ_X11 */

  free(res_name);

  return NS_OK;
}

void
nsWindow::NativeResize()
{
    if (!AreBoundsSane()) {
        // If someone has set this so that the needs show flag is false
        // and it needs to be hidden, update the flag and hide the
        // window.  This flag will be cleared the next time someone
        // hides the window or shows it.  It also prevents us from
        // calling NativeShow(false) excessively on the window which
        // causes unneeded X traffic.
        if (!mNeedsShow && mIsShown) {
            mNeedsShow = true;
            NativeShow(false);
        }
        return;
    }

    GdkRectangle size = DevicePixelsToGdkSizeRoundUp(mBounds.Size());

    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         size.width, size.height));

    if (mIsTopLevel) {
        gtk_window_resize(GTK_WINDOW(mShell), size.width, size.height);
    }
    else if (mContainer) {
        GtkWidget *widget = GTK_WIDGET(mContainer);
        GtkAllocation allocation, prev_allocation;
        gtk_widget_get_allocation(widget, &prev_allocation);
        allocation.x = prev_allocation.x;
        allocation.y = prev_allocation.y;
        allocation.width = size.width;
        allocation.height = size.height;
        gtk_widget_size_allocate(widget, &allocation);
    }
    else if (mGdkWindow) {
        gdk_window_resize(mGdkWindow, size.width, size.height);
    }

    // Does it need to be shown because bounds were previously insane?
    if (mNeedsShow && mIsShown) {
        NativeShow(true);
    }
}

void
nsWindow::NativeMoveResize()
{
    if (!AreBoundsSane()) {
        // If someone has set this so that the needs show flag is false
        // and it needs to be hidden, update the flag and hide the
        // window.  This flag will be cleared the next time someone
        // hides the window or shows it.  It also prevents us from
        // calling NativeShow(false) excessively on the window which
        // causes unneeded X traffic.
        if (!mNeedsShow && mIsShown) {
            mNeedsShow = true;
            NativeShow(false);
        }
        NativeMove();
    }

    GdkRectangle size = DevicePixelsToGdkSizeRoundUp(mBounds.Size());
    GdkPoint topLeft = DevicePixelsToGdkPointRoundDown(mBounds.TopLeft());

    LOG(("nsWindow::NativeMoveResize [%p] %d %d %d %d\n", (void *)this,
         topLeft.x, topLeft.y, size.width, size.height));

    if (mIsTopLevel) {
        // x and y give the position of the window manager frame top-left.
        gtk_window_move(GTK_WINDOW(mShell), topLeft.x, topLeft.y);
        // This sets the client window size.
        gtk_window_resize(GTK_WINDOW(mShell), size.width, size.height);
    }
    else if (mContainer) {
        GtkAllocation allocation;
        allocation.x = topLeft.x;
        allocation.y = topLeft.y;
        allocation.width = size.width;
        allocation.height = size.height;
        gtk_widget_size_allocate(GTK_WIDGET(mContainer), &allocation);
    }
    else if (mGdkWindow) {
        gdk_window_move_resize(mGdkWindow,
                               topLeft.x, topLeft.y, size.width, size.height);
    }

    // Does it need to be shown because bounds were previously insane?
    if (mNeedsShow && mIsShown) {
        NativeShow(true);
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
        gdk_window_invalidate_rect(mGdkWindow, nullptr, FALSE);

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

LayoutDeviceIntSize
nsWindow::GetSafeWindowSize(LayoutDeviceIntSize aSize)
{
    // The X protocol uses CARD32 for window sizes, but the server (1.11.3)
    // reads it as CARD16.  Sizes of pixmaps, used for drawing, are (unsigned)
    // CARD16 in the protocol, but the server's ProcCreatePixmap returns
    // BadAlloc if dimensions cannot be represented by signed shorts.
    LayoutDeviceIntSize result = aSize;
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
    // If this is a remotely updated widget we receive clipping, position, and
    // size information from a source other than our owner. Don't let our parent
    // update this information.
    if (mWindowType == eWindowType_plugin_ipc_chrome) {
      return NS_OK;
    }

    for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
        const Configuration& configuration = aConfigurations[i];
        nsWindow* w = static_cast<nsWindow*>(configuration.mChild.get());
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

nsresult
nsWindow::SetWindowClipRegion(const nsTArray<LayoutDeviceIntRect>& aRects,
                              bool aIntersectWithExisting)
{
    const nsTArray<LayoutDeviceIntRect>* newRects = &aRects;

    AutoTArray<LayoutDeviceIntRect,1> intersectRects;
    if (aIntersectWithExisting) {
        AutoTArray<LayoutDeviceIntRect,1> existingRects;
        GetWindowClipRegion(&existingRects);

        LayoutDeviceIntRegion existingRegion = RegionFromArray(existingRects);
        LayoutDeviceIntRegion newRegion = RegionFromArray(aRects);
        LayoutDeviceIntRegion intersectRegion;
        intersectRegion.And(newRegion, existingRegion);

        // If mClipRects is null we haven't set a clip rect yet, so we
        // need to set the clip even if it is equal.
        if (mClipRects && intersectRegion.IsEqual(existingRegion)) {
            return NS_OK;
        }

        if (!intersectRegion.IsEqual(newRegion)) {
            ArrayFromRegion(intersectRegion, intersectRects);
            newRects = &intersectRects;
        }
    }

    if (IsWindowClipRegionEqual(*newRects))
        return NS_OK;

    StoreWindowClipRegion(*newRects);

    if (!mGdkWindow)
        return NS_OK;

#if (MOZ_WIDGET_GTK == 2)
    GdkRegion *region = gdk_region_new(); // aborts on OOM
    for (uint32_t i = 0; i < newRects->Length(); ++i) {
        const LayoutDeviceIntRect& r = newRects->ElementAt(i);
        GdkRectangle rect = { r.x, r.y, r.width, r.height };
        gdk_region_union_with_rect(region, &rect);
    }

    gdk_window_shape_combine_region(mGdkWindow, region, 0, 0);
    gdk_region_destroy(region);
#else
    cairo_region_t *region = cairo_region_create();
    for (uint32_t i = 0; i < newRects->Length(); ++i) {
        const LayoutDeviceIntRect& r = newRects->ElementAt(i);
        cairo_rectangle_int_t rect = { r.x, r.y, r.width, r.height };
        cairo_region_union_rectangle(region, &rect);
    }

    gdk_window_shape_combine_region(mGdkWindow, region, 0, 0);
    cairo_region_destroy(region);
#endif

    return NS_OK;
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
    Display* xDisplay = GDK_WINDOW_XDISPLAY(mGdkWindow);
    Window xDrawable = GDK_WINDOW_XID(mGdkWindow);
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
    GdkBitmap* maskBitmap = gdk_bitmap_create_from_data(mGdkWindow,
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
#endif // MOZ_WIDGET_GTK == 2
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
    if (!mGdkWindow)
        return;

    Display* xDisplay = GDK_WINDOW_XDISPLAY(mGdkWindow);
    Window xWindow = gdk_x11_window_get_xid(mGdkWindow);

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
                              (GdkWindow *)nullptr, nullptr, aTime);

    if (retval == GDK_GRAB_NOT_VIEWABLE) {
        LOG(("GrabPointer: window not viewable; will retry\n"));
        mRetryPointerGrab = true;
    } else if (retval != GDK_GRAB_SUCCESS) {
        LOG(("GrabPointer: pointer grab failed: %i\n", retval));
        // A failed grab indicates that another app has grabbed the pointer.
        // Check for rollup now, because, without the grab, we likely won't
        // get subsequent button press events. Do this with an event so that
        // popups don't rollup while potentially adjusting the grab for
        // this popup.
        nsCOMPtr<nsIRunnable> event =
            NewRunnableMethod(this, &nsWindow::CheckForRollupDuringGrab);
        NS_DispatchToCurrentThread(event.forget());
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
        return nullptr;

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
        RefPtr<nsWindow> kungFuDeathGrip = gPluginFocusWindow;
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
#if (MOZ_WIDGET_GTK == 3)
    gdk_error_trap_pop_ignored();
#else
    gdk_error_trap_pop();
#endif
    gPluginFocusWindow = this;
    gdk_window_add_filter(nullptr, plugin_client_message_filter, this);

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
#if (MOZ_WIDGET_GTK == 3)
        gdk_error_trap_pop_ignored();
#else
        gdk_error_trap_pop();
#endif
    }
    gPluginFocusWindow = nullptr;
    mOldFocusWindow = 0;
    gdk_window_remove_filter(nullptr, plugin_client_message_filter, this);

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

class FullscreenTransitionWindow final : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    explicit FullscreenTransitionWindow(GtkWidget* aWidget);

    GtkWidget* mWindow;

private:
    ~FullscreenTransitionWindow();
};

NS_IMPL_ISUPPORTS0(FullscreenTransitionWindow)

FullscreenTransitionWindow::FullscreenTransitionWindow(GtkWidget* aWidget)
{
    mWindow = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWindow* gtkWin = GTK_WINDOW(mWindow);

    gtk_window_set_type_hint(gtkWin, GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
    gtk_window_set_transient_for(gtkWin, GTK_WINDOW(aWidget));
    gtk_window_set_decorated(gtkWin, false);

    GdkWindow* gdkWin = gtk_widget_get_window(aWidget);
    GdkScreen* screen = gtk_widget_get_screen(aWidget);
    gint monitorNum = gdk_screen_get_monitor_at_window(screen, gdkWin);
    GdkRectangle monitorRect;
    gdk_screen_get_monitor_geometry(screen, monitorNum, &monitorRect);
    gtk_window_set_screen(gtkWin, screen);
    gtk_window_move(gtkWin, monitorRect.x, monitorRect.y);
    gtk_window_resize(gtkWin, monitorRect.width, monitorRect.height);

    GdkColor bgColor;
    bgColor.red = bgColor.green = bgColor.blue = 0;
    gtk_widget_modify_bg(mWindow, GTK_STATE_NORMAL, &bgColor);

    gtk_window_set_opacity(gtkWin, 0.0);
    gtk_widget_show(mWindow);
}

FullscreenTransitionWindow::~FullscreenTransitionWindow()
{
    gtk_widget_destroy(mWindow);
}

class FullscreenTransitionData
{
public:
    FullscreenTransitionData(nsIWidget::FullscreenTransitionStage aStage,
                             uint16_t aDuration, nsIRunnable* aCallback,
                             FullscreenTransitionWindow* aWindow)
        : mStage(aStage)
        , mStartTime(TimeStamp::Now())
        , mDuration(TimeDuration::FromMilliseconds(aDuration))
        , mCallback(aCallback)
        , mWindow(aWindow) { }

    static const guint sInterval = 1000 / 30; // 30fps
    static gboolean TimeoutCallback(gpointer aData);

private:
    nsIWidget::FullscreenTransitionStage mStage;
    TimeStamp mStartTime;
    TimeDuration mDuration;
    nsCOMPtr<nsIRunnable> mCallback;
    RefPtr<FullscreenTransitionWindow> mWindow;
};

/* static */ gboolean
FullscreenTransitionData::TimeoutCallback(gpointer aData)
{
    bool finishing = false;
    auto data = static_cast<FullscreenTransitionData*>(aData);
    gdouble opacity = (TimeStamp::Now() - data->mStartTime) / data->mDuration;
    if (opacity >= 1.0) {
        opacity = 1.0;
        finishing = true;
    }
    if (data->mStage == nsIWidget::eAfterFullscreenToggle) {
        opacity = 1.0 - opacity;
    }
    gtk_window_set_opacity(GTK_WINDOW(data->mWindow->mWindow), opacity);

    if (!finishing) {
        return TRUE;
    }
    NS_DispatchToMainThread(data->mCallback.forget());
    delete data;
    return FALSE;
}

/* virtual */ bool
nsWindow::PrepareForFullscreenTransition(nsISupports** aData)
{
    GdkScreen* screen = gtk_widget_get_screen(mShell);
    if (!gdk_screen_is_composited(screen)) {
        return false;
    }
    *aData = do_AddRef(new FullscreenTransitionWindow(mShell)).take();
    return true;
}

/* virtual */ void
nsWindow::PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                      uint16_t aDuration, nsISupports* aData,
                                      nsIRunnable* aCallback)
{
    auto data = static_cast<FullscreenTransitionWindow*>(aData);
    // This will be released at the end of the last timeout callback for it.
    auto transitionData = new FullscreenTransitionData(aStage, aDuration,
                                                       aCallback, data);
    g_timeout_add_full(G_PRIORITY_HIGH,
                       FullscreenTransitionData::sInterval,
                       FullscreenTransitionData::TimeoutCallback,
                       transitionData, nullptr);
}

static bool
IsFullscreenSupported(GtkWidget* aShell)
{
#ifdef MOZ_X11
    GdkScreen* screen = gtk_widget_get_screen(aShell);
    GdkAtom atom = gdk_atom_intern("_NET_WM_STATE_FULLSCREEN", FALSE);
    if (!gdk_x11_screen_supports_net_wm_hint(screen, atom)) {
        return false;
    }
#endif
    return true;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen)
{
    LOG(("nsWindow::MakeFullScreen [%p] aFullScreen %d\n",
         (void *)this, aFullScreen));

    if (!IsFullscreenSupported(mShell)) {
        return NS_ERROR_NOT_AVAILABLE;
    }

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
    if (gdk_window_is_visible(mGdkWindow)) {
        gdk_window_hide(mGdkWindow);
        wasVisible = true;
    }

    gint wmd;
    if (aShouldHide)
        wmd = 0;
    else
        wmd = ConvertBorderStyles(mBorderStyle);

    if (wmd != -1)
      gdk_window_set_decorations(mGdkWindow, (GdkWMDecoration) wmd);

    if (wasVisible)
        gdk_window_show(mGdkWindow);

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
            retVal = rollupListener->ShouldConsumeOnMouseWheelEvent();
        }
        // if we're dealing with menus, we probably have submenus and
        // we don't want to rollup if the click is in a parent menu of
        // the current submenu
        uint32_t popupsToRollup = UINT32_MAX;
        if (!aAlwaysRollup) {
            AutoTArray<nsIWidget*, 5> widgetChain;
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
        bool usePoint = !aIsWheel && !aAlwaysRollup;
        nsIntPoint point(aMouseX, aMouseY);
        if (rollup && rollupListener->Rollup(popupsToRollup, true, usePoint ? &point : nullptr, nullptr)) {
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
    gpointer user_data = nullptr;
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
    
    GdkDisplay *defaultDisplay = gdk_display_get_default();

    // The strategy here is to use standard GDK cursors, and, if not available,
    // load by standard name with gdk_cursor_new_from_name.
    // Spec is here: http://www.freedesktop.org/wiki/Specifications/cursor-spec/
    switch (aCursor) {
    case eCursor_standard:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_LEFT_PTR);
        break;
    case eCursor_wait:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_WATCH);
        break;
    case eCursor_select:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_XTERM);
        break;
    case eCursor_hyperlink:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_HAND2);
        break;
    case eCursor_n_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_TOP_SIDE);
        break;
    case eCursor_s_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_BOTTOM_SIDE);
        break;
    case eCursor_w_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_LEFT_SIDE);
        break;
    case eCursor_e_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_RIGHT_SIDE);
        break;
    case eCursor_nw_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                               GDK_TOP_LEFT_CORNER);
        break;
    case eCursor_se_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                               GDK_BOTTOM_RIGHT_CORNER);
        break;
    case eCursor_ne_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                               GDK_TOP_RIGHT_CORNER);
        break;
    case eCursor_sw_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                               GDK_BOTTOM_LEFT_CORNER);
        break;
    case eCursor_crosshair:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_CROSSHAIR);
        break;
    case eCursor_move:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_FLEUR);
        break;
    case eCursor_help:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                               GDK_QUESTION_ARROW);
        break;
    case eCursor_copy: // CSS3
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "copy");
        if (!gdkcursor)
            newType = MOZ_CURSOR_COPY;
        break;
    case eCursor_alias:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "alias");
        if (!gdkcursor)
            newType = MOZ_CURSOR_ALIAS;
        break;
    case eCursor_context_menu:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "context-menu");
        if (!gdkcursor)
            newType = MOZ_CURSOR_CONTEXT_MENU;
        break;
    case eCursor_cell:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_PLUS);
        break;
    // Those two aren’t standardized. Trying both KDE’s and GNOME’s names
    case eCursor_grab:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "openhand");
        if (!gdkcursor)
            newType = MOZ_CURSOR_HAND_GRAB;
        break;
    case eCursor_grabbing:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "closedhand");
        if (!gdkcursor)
            gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "grabbing");
        if (!gdkcursor)
            newType = MOZ_CURSOR_HAND_GRABBING;
        break;
    case eCursor_spinning:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "progress");
        if (!gdkcursor)
            newType = MOZ_CURSOR_SPINNING;
        break;
    case eCursor_zoom_in:
        newType = MOZ_CURSOR_ZOOM_IN;
        break;
    case eCursor_zoom_out:
        newType = MOZ_CURSOR_ZOOM_OUT;
        break;
    case eCursor_not_allowed:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "not-allowed");
        if (!gdkcursor) // nonstandard, yet common
            gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "crossed_circle");
        if (!gdkcursor)
            newType = MOZ_CURSOR_NOT_ALLOWED;
        break;
    case eCursor_no_drop:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "no-drop");
        if (!gdkcursor) // this nonstandard sequence makes it work on KDE and GNOME
            gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "forbidden");
        if (!gdkcursor)
            gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "circle");
        if (!gdkcursor)
            newType = MOZ_CURSOR_NOT_ALLOWED;
        break;
    case eCursor_vertical_text:
        newType = MOZ_CURSOR_VERTICAL_TEXT;
        break;
    case eCursor_all_scroll:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_FLEUR);
        break;
    case eCursor_nesw_resize:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "size_bdiag");
        if (!gdkcursor)
            newType = MOZ_CURSOR_NESW_RESIZE;
        break;
    case eCursor_nwse_resize:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "size_fdiag");
        if (!gdkcursor)
            newType = MOZ_CURSOR_NWSE_RESIZE;
        break;
    case eCursor_ns_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                               GDK_SB_V_DOUBLE_ARROW);
        break;
    case eCursor_ew_resize:
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                               GDK_SB_H_DOUBLE_ARROW);
        break;
    // Here, two better fitting cursors exist in some cursor themes. Try those first
    case eCursor_row_resize:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "split_v");
        if (!gdkcursor)
            gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                                   GDK_SB_V_DOUBLE_ARROW);
        break;
    case eCursor_col_resize:
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "split_h");
        if (!gdkcursor)
            gdkcursor = gdk_cursor_new_for_display(defaultDisplay,
                                                   GDK_SB_H_DOUBLE_ARROW);
        break;
    case eCursor_none:
        newType = MOZ_CURSOR_NONE;
        break;
    default:
        NS_ASSERTION(aCursor, "Invalid cursor type");
        gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_LEFT_PTR);
        break;
    }

    // If by now we don't have a xcursor, this means we have to make a custom
    // one. First, we try creating a named cursor based on the hash of our
    // custom bitmap, as libXcursor has some magic to convert bitmapped cursors
    // to themed cursors
    if (newType != 0xFF && GtkCursors[newType].hash) {
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, GtkCursors[newType].hash);
    }

    // If we still don't have a xcursor, we now really create a bitmap cursor
    if (newType != 0xff && !gdkcursor) {
        GdkPixbuf * cursor_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
        if (!cursor_pixbuf)
            return nullptr;
      
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
    RefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
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
        RefPtr<nsWindow> window = get_window_for_gdk_window(aWindow);
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

    // A strong reference is already held during "draw" signal emission,
    // but GTK+ 3.4 wants the object to live a little longer than that
    // (bug 1225970).
    g_object_ref(widget);
    g_idle_add(
        [](gpointer data) -> gboolean {
            g_object_unref(data);
            return G_SOURCE_REMOVE;
        },
        widget);

    return FALSE;
}
#endif //MOZ_WIDGET_GTK == 2

static gboolean
configure_event_cb(GtkWidget *widget,
                   GdkEventConfigure *event)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    return window->OnConfigureEvent(widget, event);
}

static void
container_unrealize_cb (GtkWidget *widget)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return;

    window->OnContainerUnrealize();
}

static void
size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return;

    window->OnSizeAllocate(allocation);
}

static gboolean
delete_event_cb(GtkWidget *widget, GdkEventAny *event)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnDeleteEvent();

    return TRUE;
}

static gboolean
enter_notify_event_cb(GtkWidget *widget,
                      GdkEventCrossing *event)
{
    RefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
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

    RefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
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
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnContainerFocusInEvent(event);

    return FALSE;
}

static gboolean
focus_out_event_cb(GtkWidget *widget, GdkEventFocus *event)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
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

    RefPtr<nsWindow> nswindow = (nsWindow*)data;
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

    RefPtr<nsWindow> focusWindow = gFocusWindow ? gFocusWindow : window;

#ifdef MOZ_X11
    // Keyboard repeat can cause key press events to queue up when there are
    // slow event handlers (bug 301029).  Throttle these events by removing
    // consecutive pending duplicate KeyPress events to the same window.
    // We use the event time of the last one.
    // Note: GDK calls XkbSetDetectableAutorepeat so that KeyRelease events
    // are generated only when the key is physically released.
#define NS_GDKEVENT_MATCH_MASK 0x1FFF /* GDK_SHIFT_MASK .. GDK_BUTTON5_MASK */
    GdkDisplay* gdkDisplay = gtk_widget_get_display(widget);
    if (GDK_IS_X11_DISPLAY(gdkDisplay)) {
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

    RefPtr<nsWindow> focusWindow = gFocusWindow ? gFocusWindow : window;

    return focusWindow->OnKeyReleaseEvent(event);
}

static gboolean
property_notify_event_cb(GtkWidget* aWidget, GdkEventProperty* aEvent)
{
    RefPtr<nsWindow> window = get_window_for_gdk_window(aEvent->window);
    if (!window)
        return FALSE;

    return window->OnPropertyNotifyEvent(aWidget, aEvent);
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
    RefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
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
        event.window = nullptr;
        event.send_event = TRUE;
        window_state_event_cb(widget, &event);
    }
}

static gboolean
window_state_event_cb (GtkWidget *widget, GdkEventWindowState *event)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window)
        return FALSE;

    window->OnWindowStateEvent(widget, event);

    return FALSE;
}

static void
theme_changed_cb (GtkSettings *settings, GParamSpec *pspec, nsWindow *data)
{
    RefPtr<nsWindow> window = data;
    window->ThemeChanged();
}

#if (MOZ_WIDGET_GTK == 3)
static void
scale_changed_cb (GtkWidget* widget, GParamSpec* aPSpec, gpointer aPointer)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
    if (!window) {
      return;
    }
    window->OnDPIChanged();

    // configure_event is already fired before scale-factor signal,
    // but size-allocate isn't fired by changing scale
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    window->OnSizeAllocate(&allocation);
}
#endif

#if GTK_CHECK_VERSION(3,4,0)
static gboolean
touch_event_cb(GtkWidget* aWidget, GdkEventTouch* aEvent)
{
    UpdateLastInputEventTime(aEvent);

    nsWindow* window = GetFirstNSWindowForGDKWindow(aEvent->window);
    if (!window) {
        return FALSE;
    }

    return window->OnTouchEvent(aEvent);
}
#endif

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
    RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return FALSE;

    // figure out which internal widget this drag motion actually happened on
    nscoord retx = 0;
    nscoord rety = 0;

    GdkWindow *innerWindow =
        get_inner_gdk_window(gtk_widget_get_window(aWidget), aX, aY,
                             &retx, &rety);
    RefPtr<nsWindow> innerMostWindow = get_window_for_gdk_window(innerWindow);

    if (!innerMostWindow) {
        innerMostWindow = window;
    }

    LOGDRAG(("nsWindow drag-motion signal for %p\n", (void*)innerMostWindow));

    LayoutDeviceIntPoint point = window->GdkPointToDevicePixels({ retx, rety });

    return nsDragService::GetInstance()->
        ScheduleMotionEvent(innerMostWindow, aDragContext,
                            point, aTime);
}

static void
drag_leave_event_cb(GtkWidget *aWidget,
                    GdkDragContext *aDragContext,
                    guint aTime,
                    gpointer aData)
{
    RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
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
    RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
    if (!window)
        return FALSE;

    // figure out which internal widget this drag motion actually happened on
    nscoord retx = 0;
    nscoord rety = 0;

    GdkWindow *innerWindow =
        get_inner_gdk_window(gtk_widget_get_window(aWidget), aX, aY,
                             &retx, &rety);
    RefPtr<nsWindow> innerMostWindow = get_window_for_gdk_window(innerWindow);

    if (!innerMostWindow) {
        innerMostWindow = window;
    }

    LOGDRAG(("nsWindow drag-drop signal for %p\n", (void*)innerMostWindow));

    LayoutDeviceIntPoint point = window->GdkPointToDevicePixels({ retx, rety });

    return nsDragService::GetInstance()->
        ScheduleDropEvent(innerMostWindow, aDragContext,
                          point, aTime);
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
    RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
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
            gdk_window_get_geometry(childWindow, &cx, &cy, &cw, &ch, nullptr);
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
        services::GetAccessibilityService();
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

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    if (!mIMContext) {
        return;
    }
    mIMContext->SetInputContext(this, &aContext, &aAction);
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
  InputContext context;
  if (!mIMContext) {
      context.mIMEState.mEnabled = IMEState::DISABLED;
      context.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  } else {
      context = mIMContext->GetInputContext();
  }
  return context;
}

nsIMEUpdatePreference
nsWindow::GetIMEUpdatePreference()
{
    if (!mIMContext) {
        return nsIMEUpdatePreference();
    }
    return mIMContext->GetIMEUpdatePreference();
}

NS_IMETHODIMP_(TextEventDispatcherListener*)
nsWindow::GetNativeTextEventDispatcherListener()
{
    if (NS_WARN_IF(!mIMContext)) {
        return nullptr;
    }
    return mIMContext;
}

bool
nsWindow::ExecuteNativeKeyBindingRemapped(NativeKeyBindingsType aType,
                                          const WidgetKeyboardEvent& aEvent,
                                          DoCommandCallback aCallback,
                                          void* aCallbackData,
                                          uint32_t aGeckoKeyCode,
                                          uint32_t aNativeKeyCode)
{
    WidgetKeyboardEvent modifiedEvent(aEvent);
    modifiedEvent.keyCode = aGeckoKeyCode;
    static_cast<GdkEventKey*>(modifiedEvent.mNativeKeyEvent)->keyval =
        aNativeKeyCode;

    NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
    return keyBindings->Execute(modifiedEvent, aCallback, aCallbackData);
}

NS_IMETHODIMP_(bool)
nsWindow::ExecuteNativeKeyBinding(NativeKeyBindingsType aType,
                                  const WidgetKeyboardEvent& aEvent,
                                  DoCommandCallback aCallback,
                                  void* aCallbackData)
{
    if (aEvent.keyCode >= nsIDOMKeyEvent::DOM_VK_LEFT &&
        aEvent.keyCode <= nsIDOMKeyEvent::DOM_VK_DOWN) {

        // Check if we're targeting content with vertical writing mode,
        // and if so remap the arrow keys.
        WidgetQueryContentEvent query(true, eQuerySelectedText, this);
        nsEventStatus status;
        DispatchEvent(&query, status);

        if (query.mSucceeded && query.mReply.mWritingMode.IsVertical()) {
            uint32_t geckoCode = 0;
            uint32_t gdkCode = 0;
            switch (aEvent.keyCode) {
            case nsIDOMKeyEvent::DOM_VK_LEFT:
                if (query.mReply.mWritingMode.IsVerticalLR()) {
                    geckoCode = nsIDOMKeyEvent::DOM_VK_UP;
                    gdkCode = GDK_Up;
                } else {
                    geckoCode = nsIDOMKeyEvent::DOM_VK_DOWN;
                    gdkCode = GDK_Down;
                }
                break;

            case nsIDOMKeyEvent::DOM_VK_RIGHT:
                if (query.mReply.mWritingMode.IsVerticalLR()) {
                    geckoCode = nsIDOMKeyEvent::DOM_VK_DOWN;
                    gdkCode = GDK_Down;
                } else {
                    geckoCode = nsIDOMKeyEvent::DOM_VK_UP;
                    gdkCode = GDK_Up;
                }
                break;

            case nsIDOMKeyEvent::DOM_VK_UP:
                geckoCode = nsIDOMKeyEvent::DOM_VK_LEFT;
                gdkCode = GDK_Left;
                break;

            case nsIDOMKeyEvent::DOM_VK_DOWN:
                geckoCode = nsIDOMKeyEvent::DOM_VK_RIGHT;
                gdkCode = GDK_Right;
                break;
            }

            return ExecuteNativeKeyBindingRemapped(aType, aEvent, aCallback,
                                                   aCallbackData,
                                                   geckoCode, gdkCode);
        }
    }

    NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
    return keyBindings->Execute(aEvent, aCallback, aCallbackData);
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

    RefPtr<gfxASurface> result;

    if (visual) {
        Visual* xVisual = gdk_x11_visual_get_xvisual(visual);

        result = new gfxXlibSurface(xDisplay, xDrawable, xVisual,
                                    IntSize(aSize.width, aSize.height));
    } else {
        // no visual? we must be using an xrender format.  Find a format
        // for this depth.
        XRenderPictFormat *pf = nullptr;
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
                                    IntSize(aSize.width, aSize.height));
    }

    return result.forget();
}
#endif

already_AddRefed<DrawTarget>
nsWindow::GetDrawTarget(const LayoutDeviceIntRegion& aRegion, BufferMode* aBufferMode)
{
  if (!mGdkWindow || aRegion.IsEmpty()) {
    return nullptr;
  }

  RefPtr<DrawTarget> dt;

#ifdef MOZ_X11
#  ifdef MOZ_HAVE_SHMIMAGE
#    ifdef MOZ_WIDGET_GTK
  if (!gfxPlatformGtk::GetPlatform()->UseXRender())
#    endif
  if (nsShmImage::UseShm()) {
    mBackShmImage.swap(mFrontShmImage);
    if (!mBackShmImage) {
      mBackShmImage = new nsShmImage(mXDisplay, mXWindow, mXVisual, mXDepth);
    }
    dt = mBackShmImage->CreateDrawTarget(aRegion);
    *aBufferMode = BufferMode::BUFFER_NONE;
    if (!dt) {
      mBackShmImage = nullptr;
    }
  }
#  endif  // MOZ_HAVE_SHMIMAGE
  if (!dt) {
    LayoutDeviceIntRect bounds = aRegion.GetBounds();
    LayoutDeviceIntSize size(bounds.XMost(), bounds.YMost());
    RefPtr<gfxXlibSurface> surf = new gfxXlibSurface(mXDisplay, mXWindow, mXVisual, size.ToUnknownSize());
    if (!surf->CairoStatus()) {
      dt = gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surf.get(), surf->GetSize());
      *aBufferMode = BufferMode::BUFFERED;
    }
  }
#endif // MOZ_X11

  return dt.forget();
}

already_AddRefed<DrawTarget>
nsWindow::StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion, BufferMode* aBufferMode)
{
  return GetDrawTarget(aInvalidRegion, aBufferMode);
}

void
nsWindow::EndRemoteDrawingInRegion(DrawTarget* aDrawTarget,
                                   LayoutDeviceIntRegion& aInvalidRegion)
{
#ifdef MOZ_X11
#  ifdef MOZ_HAVE_SHMIMAGE
  if (!mGdkWindow || !mBackShmImage) {
    return;
  }

  mBackShmImage->Put(aInvalidRegion);
#  endif // MOZ_HAVE_SHMIMAGE
#endif // MOZ_X11
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
#ifdef DEBUG
    // GDK_IS_WINDOW(...) expands to a statement-expression, and
    // statement-expressions are not allowed in template-argument lists. So we
    // have to make the MOZ_ASSERT condition indirect.
    if (!GDK_IS_WINDOW(gdk_window)) {
        MOZ_ASSERT(false, "must really be window");
    }
#endif

    // find the top-level window
    gdk_window = gdk_window_get_toplevel(gdk_window);
    MOZ_ASSERT(gdk_window,
               "gdk_window_get_toplevel should not return null");
    *aWindow = gdk_window;

    if (!aMouseEvent->mWidget) {
        return false;
    }

    // FIXME: It would be nice to have the widget position at the time
    // of the event, but it's relatively unlikely that the widget has
    // moved since the mousedown.  (On the other hand, it's quite likely
    // that the mouse has moved, which is why we use the mouse position
    // from the event.)
    LayoutDeviceIntPoint offset = aMouseEvent->mWidget->WidgetToScreenOffset();
    *aRootX = aMouseEvent->mRefPoint.x + offset.x;
    *aRootY = aMouseEvent->mRefPoint.y + offset.y;

    return true;
}

NS_IMETHODIMP
nsWindow::BeginMoveDrag(WidgetMouseEvent* aEvent)
{
    MOZ_ASSERT(aEvent, "must have event");
    MOZ_ASSERT(aEvent->mClass == eMouseEventClass,
               "event must have correct struct type");

    GdkWindow *gdk_window;
    gint button, screenX, screenY;
    if (!GetDragInfo(aEvent, &gdk_window, &button, &screenX, &screenY)) {
        return NS_ERROR_FAILURE;
    }

    // tell the window manager to start the move
    screenX = DevicePixelsToGdkCoordRoundDown(screenX);
    screenY = DevicePixelsToGdkCoordRoundDown(screenY);
    gdk_window_begin_move_drag(gdk_window, button, screenX, screenY,
                               aEvent->mTime);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::BeginResizeDrag(WidgetGUIEvent* aEvent,
                          int32_t aHorizontal,
                          int32_t aVertical)
{
    NS_ENSURE_ARG_POINTER(aEvent);

    if (aEvent->mClass != eMouseEventClass) {
        // you can only begin a resize drag with a mouse event
        return NS_ERROR_INVALID_ARG;
    }

    GdkWindow *gdk_window;
    gint button, screenX, screenY;
    if (!GetDragInfo(aEvent->AsMouseEvent(), &gdk_window, &button,
                     &screenX, &screenY)) {
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
                                 screenX, screenY, aEvent->mTime);

    return NS_OK;
}

nsIWidget::LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
    if (mIsDestroyed) {
      // Prevent external code from triggering the re-creation of the LayerManager/Compositor
      // during shutdown. Just return what we currently have, which is most likely null.
      return mLayerManager;
    }
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
        mLayerManager->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_BASIC) {
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

gint
nsWindow::GdkScaleFactor()
{
#if (MOZ_WIDGET_GTK >= 3)
    // Available as of GTK 3.10+
    static auto sGdkWindowGetScaleFactorPtr = (gint (*)(GdkWindow*))
        dlsym(RTLD_DEFAULT, "gdk_window_get_scale_factor");
    if (sGdkWindowGetScaleFactorPtr && mGdkWindow)
        return (*sGdkWindowGetScaleFactorPtr)(mGdkWindow);
#endif
    return nsScreenGtk::GetGtkMonitorScaleFactor();
}


gint
nsWindow::DevicePixelsToGdkCoordRoundUp(int pixels) {
    gint scale = GdkScaleFactor();
    return (pixels + scale - 1) / scale;
}

gint
nsWindow::DevicePixelsToGdkCoordRoundDown(int pixels) {
    gint scale = GdkScaleFactor();
    return pixels / scale;
}

GdkPoint
nsWindow::DevicePixelsToGdkPointRoundDown(LayoutDeviceIntPoint point) {
    gint scale = GdkScaleFactor();
    return { point.x / scale, point.y / scale };
}

GdkRectangle
nsWindow::DevicePixelsToGdkRectRoundOut(LayoutDeviceIntRect rect) {
    gint scale = GdkScaleFactor();
    int x = rect.x / scale;
    int y = rect.y / scale;
    int right = (rect.x + rect.width + scale - 1) / scale;
    int bottom = (rect.y + rect.height + scale - 1) / scale;
    return { x, y, right - x, bottom - y };
}

GdkRectangle
nsWindow::DevicePixelsToGdkSizeRoundUp(LayoutDeviceIntSize pixelSize) {
    gint scale = GdkScaleFactor();
    gint width = (pixelSize.width + scale - 1) / scale;
    gint height = (pixelSize.height + scale - 1) / scale;
    return { 0, 0, width, height };
}

int
nsWindow::GdkCoordToDevicePixels(gint coord) {
    return coord * GdkScaleFactor();
}

LayoutDeviceIntPoint
nsWindow::GdkEventCoordsToDevicePixels(gdouble x, gdouble y)
{
    gint scale = GdkScaleFactor();
    return LayoutDeviceIntPoint(floor(x * scale + 0.5), floor(y * scale + 0.5));
}

LayoutDeviceIntPoint
nsWindow::GdkPointToDevicePixels(GdkPoint point) {
    gint scale = GdkScaleFactor();
    return LayoutDeviceIntPoint(point.x * scale,
                                point.y * scale);
}

LayoutDeviceIntRect
nsWindow::GdkRectToDevicePixels(GdkRectangle rect) {
    gint scale = GdkScaleFactor();
    return LayoutDeviceIntRect(rect.x * scale,
                               rect.y * scale,
                               rect.width * scale,
                               rect.height * scale);
}

nsresult
nsWindow::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                     uint32_t aNativeMessage,
                                     uint32_t aModifierFlags,
                                     nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mouseevent");

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
    GdkPoint point = DevicePixelsToGdkPointRoundDown(aPoint);
    gdk_display_warp_pointer(display, screen, point.x, point.y);
  }

  return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                           uint32_t aNativeMessage,
                                           double aDeltaX,
                                           double aDeltaY,
                                           double aDeltaZ,
                                           uint32_t aModifierFlags,
                                           uint32_t aAdditionalFlags,
                                           nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");

  if (!mGdkWindow) {
    return NS_OK;
  }

  GdkEvent event;
  memset(&event, 0, sizeof(GdkEvent));
  event.type = GDK_SCROLL;
  event.scroll.window = mGdkWindow;
  event.scroll.time = GDK_CURRENT_TIME;
#if (MOZ_WIDGET_GTK == 3)
  // Get device for event source
  GdkDisplay* display = gdk_window_get_display(mGdkWindow);
  GdkDeviceManager *device_manager = gdk_display_get_device_manager(display);
  event.scroll.device = gdk_device_manager_get_client_pointer(device_manager);
#endif
  event.scroll.x_root = aPoint.x;
  event.scroll.y_root = aPoint.y;

  LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
  event.scroll.x = pointInWindow.x;
  event.scroll.y = pointInWindow.y;

  // The delta values are backwards on Linux compared to Windows and Cocoa,
  // hence the negation.
#if GTK_CHECK_VERSION(3,4,0)
  // TODO: is this correct? I don't have GTK 3.4+ so I can't check
  event.scroll.direction = GDK_SCROLL_SMOOTH;
  event.scroll.delta_x = -aDeltaX;
  event.scroll.delta_y = -aDeltaY;
#else
  if (aDeltaX < 0) {
    event.scroll.direction = GDK_SCROLL_RIGHT;
  } else if (aDeltaX > 0) {
    event.scroll.direction = GDK_SCROLL_LEFT;
  } else if (aDeltaY < 0) {
    event.scroll.direction = GDK_SCROLL_DOWN;
  } else if (aDeltaY > 0) {
    event.scroll.direction = GDK_SCROLL_UP;
  } else {
    return NS_OK;
  }
#endif

  gdk_event_put(&event);

  return NS_OK;
}

#if GTK_CHECK_VERSION(3,4,0)
nsresult
nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                     TouchPointerState aPointerState,
                                     LayoutDeviceIntPoint aPoint,
                                     double aPointerPressure,
                                     uint32_t aPointerOrientation,
                                     nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "touchpoint");

  if (!mGdkWindow) {
    return NS_OK;
  }

  GdkEvent event;
  memset(&event, 0, sizeof(GdkEvent));

  static std::map<uint32_t, GdkEventSequence*> sKnownPointers;

  auto result = sKnownPointers.find(aPointerId);
  switch (aPointerState) {
  case TOUCH_CONTACT:
    if (result == sKnownPointers.end()) {
      // GdkEventSequence isn't a thing we can instantiate, and never gets
      // dereferenced in the gtk code. It's an opaque pointer, the only
      // requirement is that it be distinct from other instances of
      // GdkEventSequence*.
      event.touch.sequence = (GdkEventSequence*)((uintptr_t)aPointerId);
      sKnownPointers[aPointerId] = event.touch.sequence;
      event.type = GDK_TOUCH_BEGIN;
    } else {
      event.touch.sequence = result->second;
      event.type = GDK_TOUCH_UPDATE;
    }
    break;
  case TOUCH_REMOVE:
    event.type = GDK_TOUCH_END;
    if (result == sKnownPointers.end()) {
      NS_WARNING("Tried to synthesize touch-end for unknown pointer!");
      return NS_ERROR_UNEXPECTED;
    }
    event.touch.sequence = result->second;
    sKnownPointers.erase(result);
    break;
  case TOUCH_CANCEL:
    event.type = GDK_TOUCH_CANCEL;
    if (result == sKnownPointers.end()) {
      NS_WARNING("Tried to synthesize touch-cancel for unknown pointer!");
      return NS_ERROR_UNEXPECTED;
    }
    event.touch.sequence = result->second;
    sKnownPointers.erase(result);
    break;
  case TOUCH_HOVER:
  default:
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  event.touch.window = mGdkWindow;
  event.touch.time = GDK_CURRENT_TIME;

  GdkDisplay* display = gdk_window_get_display(mGdkWindow);
  GdkDeviceManager* device_manager = gdk_display_get_device_manager(display);
  event.touch.device = gdk_device_manager_get_client_pointer(device_manager);

  event.touch.x_root = DevicePixelsToGdkCoordRoundDown(aPoint.x);
  event.touch.y_root = DevicePixelsToGdkCoordRoundDown(aPoint.y);

  LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
  event.touch.x = DevicePixelsToGdkCoordRoundDown(pointInWindow.x);
  event.touch.y = DevicePixelsToGdkCoordRoundDown(pointInWindow.y);

  gdk_event_put(&event);

  return NS_OK;
}
#endif

int32_t
nsWindow::RoundsWidgetCoordinatesTo()
{
    return GdkScaleFactor();
}
