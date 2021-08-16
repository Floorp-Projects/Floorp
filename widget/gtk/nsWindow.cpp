/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindow.h"

#include <algorithm>
#include <dlfcn.h>
#include <gdk/gdkkeysyms.h>
#include <wchar.h>

#include "ClientLayerManager.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPlatformGtk.h"
#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "GtkCompositorWidget.h"
#include "gtkdrawing.h"
#include "imgIContainer.h"
#include "InputData.h"
#include "Layers.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Components.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WheelEventBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/Likely.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/WritingModes.h"
#include "mozilla/X11Util.h"
#include "mozilla/XREAppData.h"
#include "NativeKeyBindings.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsDragService.h"
#include "nsGTKToolkit.h"
#include "nsGtkKeyUtils.h"
#include "nsGtkCursors.h"
#include "nsGfxCIID.h"
#include "nsGtkUtils.h"
#include "nsIFile.h"
#include "nsIGSettingsService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsImageToPixbuf.h"
#include "nsINode.h"
#include "nsIRollupListener.h"
#include "nsIScreenManager.h"
#include "nsIUserIdleServiceInternal.h"
#include "nsIWidgetListener.h"
#include "nsLayoutUtils.h"
#include "nsMenuPopupFrame.h"
#include "nsPresContext.h"
#include "nsShmImage.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsViewManager.h"
#include "nsXPLookAndFeel.h"
#include "prlink.h"
#include "ScreenHelperGTK.h"
#include "SystemTimeConverter.h"
#include "WidgetUtilsGtk.h"
#include "mozilla/X11Util.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/LocalAccessible.h"
#  include "mozilla/a11y/Platform.h"
#  include "nsAccessibilityService.h"
#endif

#ifdef MOZ_X11
#  include <gdk/gdkkeysyms-compat.h>
#  include <X11/Xatom.h>
#  include <X11/extensions/XShm.h>
#  include <X11/extensions/shape.h>
#  include "gfxXlibSurface.h"
#  include "GLContextGLX.h"  // for GLContextGLX::FindVisual()
#  include "GLContextEGL.h"  // for GLContextEGL::FindVisual()
#  include "WindowSurfaceX11Image.h"
#  include "WindowSurfaceX11SHM.h"
#  include "WindowSurfaceXRender.h"
#endif
#ifdef MOZ_WAYLAND
#  include "nsIClipboard.h"
#  include "nsView.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using mozilla::gl::GLContextEGL;
using mozilla::gl::GLContextGLX;

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION 100

#if !GTK_CHECK_VERSION(3, 18, 0)

struct _GdkEventTouchpadPinch {
  GdkEventType type;
  GdkWindow* window;
  gint8 send_event;
  gint8 phase;
  gint8 n_fingers;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble dx;
  gdouble dy;
  gdouble angle_delta;
  gdouble scale;
  gdouble x_root, y_root;
  guint state;
};

typedef enum {
  GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
  GDK_TOUCHPAD_GESTURE_PHASE_UPDATE,
  GDK_TOUCHPAD_GESTURE_PHASE_END,
  GDK_TOUCHPAD_GESTURE_PHASE_CANCEL
} GdkTouchpadGesturePhase;

GdkEventMask GDK_TOUCHPAD_GESTURE_MASK = static_cast<GdkEventMask>(1 << 24);
GdkEventType GDK_TOUCHPAD_PINCH = static_cast<GdkEventType>(42);

#endif

const gint kEvents = GDK_TOUCHPAD_GESTURE_MASK | GDK_EXPOSURE_MASK |
                     GDK_STRUCTURE_MASK | GDK_VISIBILITY_NOTIFY_MASK |
                     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                     GDK_SMOOTH_SCROLL_MASK | GDK_TOUCH_MASK | GDK_SCROLL_MASK |
                     GDK_POINTER_MOTION_MASK | GDK_PROPERTY_CHANGE_MASK;

#if !GTK_CHECK_VERSION(3, 22, 0)
typedef enum {
  GDK_ANCHOR_FLIP_X = 1 << 0,
  GDK_ANCHOR_FLIP_Y = 1 << 1,
  GDK_ANCHOR_SLIDE_X = 1 << 2,
  GDK_ANCHOR_SLIDE_Y = 1 << 3,
  GDK_ANCHOR_RESIZE_X = 1 << 4,
  GDK_ANCHOR_RESIZE_Y = 1 << 5,
  GDK_ANCHOR_FLIP = GDK_ANCHOR_FLIP_X | GDK_ANCHOR_FLIP_Y,
  GDK_ANCHOR_SLIDE = GDK_ANCHOR_SLIDE_X | GDK_ANCHOR_SLIDE_Y,
  GDK_ANCHOR_RESIZE = GDK_ANCHOR_RESIZE_X | GDK_ANCHOR_RESIZE_Y
} GdkAnchorHints;
#endif

/* utility functions */
static bool is_mouse_in_window(GdkWindow* aWindow, gdouble aMouseX,
                               gdouble aMouseY);
static nsWindow* get_window_for_gtk_widget(GtkWidget* widget);
static nsWindow* get_window_for_gdk_window(GdkWindow* window);
static GtkWidget* get_gtk_widget_for_gdk_window(GdkWindow* window);
static GdkCursor* get_gtk_cursor(nsCursor aCursor);

static GdkWindow* get_inner_gdk_window(GdkWindow* aWindow, gint x, gint y,
                                       gint* retx, gint* rety);

static int is_parent_ungrab_enter(GdkEventCrossing* aEvent);
static int is_parent_grab_leave(GdkEventCrossing* aEvent);

/* callbacks from widgets */
static gboolean expose_event_cb(GtkWidget* widget, cairo_t* cr);
static gboolean configure_event_cb(GtkWidget* widget, GdkEventConfigure* event);
static void container_unrealize_cb(GtkWidget* widget);
static void size_allocate_cb(GtkWidget* widget, GtkAllocation* allocation);
static void toplevel_window_size_allocate_cb(GtkWidget* widget,
                                             GtkAllocation* allocation);
static gboolean delete_event_cb(GtkWidget* widget, GdkEventAny* event);
static gboolean enter_notify_event_cb(GtkWidget* widget,
                                      GdkEventCrossing* event);
static gboolean leave_notify_event_cb(GtkWidget* widget,
                                      GdkEventCrossing* event);
static gboolean motion_notify_event_cb(GtkWidget* widget,
                                       GdkEventMotion* event);
MOZ_CAN_RUN_SCRIPT static gboolean button_press_event_cb(GtkWidget* widget,
                                                         GdkEventButton* event);
static gboolean button_release_event_cb(GtkWidget* widget,
                                        GdkEventButton* event);
static gboolean focus_in_event_cb(GtkWidget* widget, GdkEventFocus* event);
static gboolean focus_out_event_cb(GtkWidget* widget, GdkEventFocus* event);
static gboolean key_press_event_cb(GtkWidget* widget, GdkEventKey* event);
static gboolean key_release_event_cb(GtkWidget* widget, GdkEventKey* event);
static gboolean property_notify_event_cb(GtkWidget* widget,
                                         GdkEventProperty* event);
static gboolean scroll_event_cb(GtkWidget* widget, GdkEventScroll* event);

static void hierarchy_changed_cb(GtkWidget* widget,
                                 GtkWidget* previous_toplevel);
static gboolean window_state_event_cb(GtkWidget* widget,
                                      GdkEventWindowState* event);
static void settings_xft_dpi_changed_cb(GtkSettings* settings,
                                        GParamSpec* pspec, nsWindow* data);
static void check_resize_cb(GtkContainer* container, gpointer user_data);
static void screen_composited_changed_cb(GdkScreen* screen, gpointer user_data);
static void widget_composited_changed_cb(GtkWidget* widget, gpointer user_data);

static void scale_changed_cb(GtkWidget* widget, GParamSpec* aPSpec,
                             gpointer aPointer);
static gboolean touch_event_cb(GtkWidget* aWidget, GdkEventTouch* aEvent);
static gboolean generic_event_cb(GtkWidget* widget, GdkEvent* aEvent);

static nsWindow* GetFirstNSWindowForGDKWindow(GdkWindow* aGdkWindow);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#ifdef MOZ_X11
static GdkFilterReturn popup_take_focus_filter(GdkXEvent* gdk_xevent,
                                               GdkEvent* event, gpointer data);
#endif /* MOZ_X11 */
#ifdef __cplusplus
}
#endif /* __cplusplus */

static gboolean drag_motion_event_cb(GtkWidget* aWidget,
                                     GdkDragContext* aDragContext, gint aX,
                                     gint aY, guint aTime, gpointer aData);
static void drag_leave_event_cb(GtkWidget* aWidget,
                                GdkDragContext* aDragContext, guint aTime,
                                gpointer aData);
static gboolean drag_drop_event_cb(GtkWidget* aWidget,
                                   GdkDragContext* aDragContext, gint aX,
                                   gint aY, guint aTime, gpointer aData);
static void drag_data_received_event_cb(GtkWidget* aWidget,
                                        GdkDragContext* aDragContext, gint aX,
                                        gint aY,
                                        GtkSelectionData* aSelectionData,
                                        guint aInfo, guint32 aTime,
                                        gpointer aData);

/* initialization static functions */
static nsresult initialize_prefs(void);

static guint32 sLastUserInputTime = GDK_CURRENT_TIME;
static guint32 sRetryGrabTime;

static SystemTimeConverter<guint32>& TimeConverter() {
  static SystemTimeConverter<guint32> sTimeConverterSingleton;
  return sTimeConverterSingleton;
}

nsWindow::GtkWindowDecoration nsWindow::sGtkWindowDecoration =
    GTK_DECORATION_UNKNOWN;
bool nsWindow::sTransparentMainWindow = false;

namespace mozilla {

class CurrentX11TimeGetter {
 public:
  explicit CurrentX11TimeGetter(GdkWindow* aWindow)
      : mWindow(aWindow), mAsyncUpdateStart() {}

  guint32 GetCurrentTime() const { return gdk_x11_get_server_time(mWindow); }

  void GetTimeAsyncForPossibleBackwardsSkew(const TimeStamp& aNow) {
    // Check for in-flight request
    if (!mAsyncUpdateStart.IsNull()) {
      return;
    }
    mAsyncUpdateStart = aNow;

    Display* xDisplay = GDK_WINDOW_XDISPLAY(mWindow);
    Window xWindow = GDK_WINDOW_XID(mWindow);
    unsigned char c = 'a';
    Atom timeStampPropAtom = TimeStampPropAtom();
    XChangeProperty(xDisplay, xWindow, timeStampPropAtom, timeStampPropAtom, 8,
                    PropModeReplace, &c, 1);
    XFlush(xDisplay);
  }

  gboolean PropertyNotifyHandler(GtkWidget* aWidget, GdkEventProperty* aEvent) {
    if (aEvent->atom != gdk_x11_xatom_to_atom(TimeStampPropAtom())) {
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
    return gdk_x11_get_xatom_by_name_for_display(gdk_display_get_default(),
                                                 "GDK_TIMESTAMP_PROP");
  }

  // This is safe because this class is stored as a member of mWindow and
  // won't outlive it.
  GdkWindow* mWindow;
  TimeStamp mAsyncUpdateStart;
};

}  // namespace mozilla

static NS_DEFINE_IID(kCDragServiceCID, NS_DRAGSERVICE_CID);

// The window from which the focus manager asks us to dispatch key events.
static nsWindow* gFocusWindow = nullptr;
static bool gBlockActivateEvent = false;
static bool gGlobalsInitialized = false;
static bool gRaiseWindows = true;
static bool gUseAspectRatio = true;
static uint32_t gLastTouchID = 0;

#define NS_WINDOW_TITLE_MAX_LENGTH 4095
#define kWindowPositionSlop 20

// cursor cache
static GdkCursor* gCursorCache[eCursorCount];

static GtkWidget* gInvisibleContainer = nullptr;

// Sometimes this actually also includes the state of the modifier keys, but
// only the button state bits are used.
static guint gButtonState;

static inline int32_t GetBitmapStride(int32_t width) {
#if defined(MOZ_X11)
  return (width + 7) / 8;
#else
  return cairo_format_stride_for_width(CAIRO_FORMAT_A1, width);
#endif
}

static inline bool TimestampIsNewerThan(guint32 a, guint32 b) {
  // Timestamps are just the least significant bits of a monotonically
  // increasing function, and so the use of unsigned overflow arithmetic.
  return a - b <= G_MAXUINT32 / 2;
}

static void UpdateLastInputEventTime(void* aGdkEvent) {
  nsCOMPtr<nsIUserIdleServiceInternal> idleService =
      do_GetService("@mozilla.org/widget/useridleservice;1");
  if (idleService) {
    idleService->ResetIdleTimeOut(0);
  }

  guint timestamp = gdk_event_get_time(static_cast<GdkEvent*>(aGdkEvent));
  if (timestamp == GDK_CURRENT_TIME) return;

  sLastUserInputTime = timestamp;
}

void GetWindowOrigin(GdkWindow* aWindow, int* aX, int* aY) {
  *aX = 0;
  *aY = 0;

  if (aWindow) {
    gdk_window_get_origin(aWindow, aX, aY);
  }
  // GetWindowOrigin / gdk_window_get_origin is very fast on Wayland as the
  // window position is cached by Gtk.

  // TODO(bug 1655924): gdk_window_get_origin is can block waiting for the X
  // server for a long time, we would like to use the implementation below
  // instead. However, removing the synchronous x server queries causes a race
  // condition to surface, causing issues such as bug 1652743 and bug 1653711.
#if 0
  *aX = 0;
  *aY = 0;
  if (!aWindow) {
    return;
  }

  GdkWindow* current = aWindow;
  while (GdkWindow* parent = gdk_window_get_parent(current)) {
    if (parent == current) {
      break;
    }

    int x = 0;
    int y = 0;
    gdk_window_get_position(current, &x, &y);
    *aX += x;
    *aY += y;

    current = parent;
  }
#endif
}

nsWindow::nsWindow()
    : mIsTopLevel(false),
      mIsDestroyed(false),
      mListenForResizes(false),
      mNeedsDispatchResized(false),
      mIsShown(false),
      mNeedsShow(false),
      mEnabled(true),
      mCreated(false),
      mHandleTouchEvent(false),
      mIsDragPopup(false),
      mPopupHint(),
      mWindowScaleFactorChanged(true),
      mWindowScaleFactor(1),
      mCompositedScreen(gdk_screen_is_composited(gdk_screen_get_default())),
      mIsAccelerated(false),
      mShell(nullptr),
      mContainer(nullptr),
      mGdkWindow(nullptr),
      mWindowShouldStartDragging(false),
      mCompositorWidgetDelegate(nullptr),
      mCompositorState(COMPOSITOR_ENABLED),
      mCompositorPauseTimeoutID(0),
      mHasMappedToplevel(false),
      mRetryPointerGrab(false),
      mSizeState(nsSizeMode_Normal),
      mAspectRatio(0.0f),
      mAspectRatioSaved(0.0f),
      mLastScrollEventTime(GDK_CURRENT_TIME),
      mPendingConfigures(0),
      mGtkWindowDecoration(GTK_DECORATION_NONE),
      mDrawToContainer(false),
      mDrawInTitlebar(false),
      mTitlebarBackdropState(false),
      mIsPIPWindow(false),
      mIsWaylandPanelWindow(false),
      mAlwaysOnTop(false),
      mIsTransparent(false),
      mTransparencyBitmap(nullptr),
      mTransparencyBitmapWidth(0),
      mTransparencyBitmapHeight(0),
      mTransparencyBitmapForTitlebar(false),
      mHasAlphaVisual(false),
      mLastMotionPressure(0),
      mLastSizeMode(nsSizeMode_Normal),
      mBoundsAreValid(true),
      mPopupTrackInHierarchy(false),
      mPopupTrackInHierarchyConfigured(false),
      mHiddenPopupPositioned(false),
      mPopupPosition(),
      mPopupAnchored(false),
      mPopupContextMenu(false),
      mRelativePopupPosition(),
      mRelativePopupOffset(),
      mPopupMatchesLayout(false),
      mPopupChanged(false),
      mPopupTemporaryHidden(false),
      mPopupClosed(false),
      mPopupLastAnchor(),
      mPreferredPopupRect(),
      mPreferredPopupRectFlushed(false),
      mWaitingForMoveToRectCallback(false),
      mNewSizeAfterMoveToRect(LayoutDeviceIntRect(0, 0, 0, 0))
#ifdef ACCESSIBILITY
      ,
      mRootAccessible(nullptr)
#endif
#ifdef MOZ_X11
      ,
      mXWindow(X11None),
      mXVisual(nullptr),
      mXDepth(0)
#endif
#ifdef MOZ_WAYLAND
      ,
      mNativePointerLockCenter(LayoutDeviceIntPoint()),
      mLockedPointer(nullptr),
      mRelativePointer(nullptr)
#endif
{
  mWindowType = eWindowType_child;
  mSizeConstraints.mMaxSize = GetSafeWindowSize(mSizeConstraints.mMaxSize);

  if (!gGlobalsInitialized) {
    gGlobalsInitialized = true;

    // It's OK if either of these fail, but it may not be one day.
    initialize_prefs();

#ifdef MOZ_WAYLAND
    // Wayland provides clipboard data to application on focus-in event
    // so we need to init our clipboard hooks before we create window
    // and get focus.
    if (GdkIsWaylandDisplay()) {
      nsCOMPtr<nsIClipboard> clipboard =
          do_GetService("@mozilla.org/widget/clipboard;1");
      NS_ASSERTION(clipboard, "Failed to init clipboard!");
    }
#endif
  }
}

nsWindow::~nsWindow() {
  LOG(("nsWindow::~nsWindow() [%p]\n", (void*)this));

  delete[] mTransparencyBitmap;
  mTransparencyBitmap = nullptr;

  Destroy();
}

/* static */
void nsWindow::ReleaseGlobals() {
  for (auto& cursor : gCursorCache) {
    if (cursor) {
      g_object_unref(cursor);
      cursor = nullptr;
    }
  }
}

void nsWindow::CommonCreate(nsIWidget* aParent, bool aListenForResizes) {
  mParent = aParent;
  mListenForResizes = aListenForResizes;
  mCreated = true;
}

void nsWindow::DispatchActivateEvent(void) {
  NS_ASSERTION(mContainer || mIsDestroyed,
               "DispatchActivateEvent only intended for container windows");

#ifdef ACCESSIBILITY
  DispatchActivateEventAccessible();
#endif  // ACCESSIBILITY

  if (mWidgetListener) mWidgetListener->WindowActivated();
}

void nsWindow::DispatchDeactivateEvent(void) {
  if (mWidgetListener) mWidgetListener->WindowDeactivated();

#ifdef ACCESSIBILITY
  DispatchDeactivateEventAccessible();
#endif  // ACCESSIBILITY
}

void nsWindow::DispatchResized() {
  LOG(("nsWindow::DispatchResized() [%p] size [%d, %d]", this,
       (int)(mBounds.width), (int)(mBounds.height)));

  mNeedsDispatchResized = false;
  if (mWidgetListener) {
    mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }
  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }
}

void nsWindow::MaybeDispatchResized() {
  if (mNeedsDispatchResized && !mIsDestroyed) {
    DispatchResized();
  }
}

nsIWidgetListener* nsWindow::GetListener() {
  return mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
}

nsresult nsWindow::DispatchEvent(WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) {
#ifdef DEBUG
  debug_DumpEvent(stdout, aEvent->mWidget, aEvent, "something", 0);
#endif
  aStatus = nsEventStatus_eIgnore;
  nsIWidgetListener* listener = GetListener();
  if (listener) {
    aStatus = listener->HandleEvent(aEvent, mUseAttachedEvents);
  }

  return NS_OK;
}

void nsWindow::OnDestroy(void) {
  if (mOnDestroyCalled) return;

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

bool nsWindow::AreBoundsSane() {
  return mBounds.width > 0 && mBounds.height > 0;
}

static GtkWidget* EnsureInvisibleContainer() {
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

static void CheckDestroyInvisibleContainer() {
  MOZ_ASSERT(gInvisibleContainer, "oh, no");
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
static void SetWidgetForHierarchy(GdkWindow* aWindow, GtkWidget* aOldWidget,
                                  GtkWidget* aNewWidget) {
  gpointer data;
  gdk_window_get_user_data(aWindow, &data);

  if (data != aOldWidget) {
    if (!GTK_IS_WIDGET(data)) return;

    auto* widget = static_cast<GtkWidget*>(data);
    if (gtk_widget_get_parent(widget) != aOldWidget) return;

    // This window belongs to a child widget, which will no longer be a
    // child of aOldWidget.
    gtk_widget_reparent(widget, aNewWidget);

    return;
  }

  GList* children = gdk_window_get_children(aWindow);
  for (GList* list = children; list; list = list->next) {
    SetWidgetForHierarchy(GDK_WINDOW(list->data), aOldWidget, aNewWidget);
  }
  g_list_free(children);

  gdk_window_set_user_data(aWindow, aNewWidget);
}

// Walk the list of child windows and call destroy on them.
void nsWindow::DestroyChildWindows() {
  if (!mGdkWindow) return;

  while (GList* children = gdk_window_peek_children(mGdkWindow)) {
    GdkWindow* child = GDK_WINDOW(children->data);
    nsWindow* kid = get_window_for_gdk_window(child);
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

void nsWindow::Destroy() {
  if (mIsDestroyed || !mCreated) return;

  LOG(("nsWindow::Destroy [%p]\n", (void*)this));
  mIsDestroyed = true;
  mCreated = false;

  /** Need to clean our LayerManager up while still alive */
  if (mWindowRenderer) {
    mWindowRenderer->Destroy();
  }
  mWindowRenderer = nullptr;

#ifdef MOZ_WAYLAND
  // Shut down our local vsync source
  if (mWaylandVsyncSource) {
    mWaylandVsyncSource->Shutdown();
    mWaylandVsyncSource = nullptr;
  }
#endif

  if (mCompositorPauseTimeoutID) {
    g_source_remove(mCompositorPauseTimeoutID);
    mCompositorPauseTimeoutID = 0;
  }

  // It is safe to call DestroyeCompositor several times (here and
  // in the parent class) since it will take effect only once.
  // The reason we call it here is because on gtk platforms we need
  // to destroy the compositor before we destroy the gdk window (which
  // destroys the the gl context attached to it).
  DestroyCompositor();

  // Ensure any resources assigned to the window get cleaned up first
  // to avoid double-freeing.
  mSurfaceProvider.CleanupResources();

  ClearCachedResources();

  g_signal_handlers_disconnect_by_data(gtk_settings_get_default(), this);

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  if (rollupListener) {
    nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
    if (static_cast<nsIWidget*>(this) == rollupWidget) {
      rollupListener->Rollup(0, false, nullptr, nullptr);
    }
  }

  // dragService will be null after shutdown of the service manager.
  RefPtr<nsDragService> dragService = nsDragService::GetInstance();
  if (dragService && this == dragService->GetMostRecentDestWindow()) {
    dragService->ScheduleLeaveEvent();
  }

  NativeShow(false);

  if (mIMContext) {
    mIMContext->OnDestroyWindow(this);
  }

  // make sure that we remove ourself as the focus window
  if (gFocusWindow == this) {
    LOG(("automatically losing focus...\n"));
    gFocusWindow = nullptr;
  }

  GtkWidget* owningWidget = GetMozContainerWidget();
  if (mShell) {
    gtk_widget_destroy(mShell);
    mShell = nullptr;
    mContainer = nullptr;
    MOZ_ASSERT(!mGdkWindow,
               "mGdkWindow should be NULL when mContainer is destroyed");
  } else if (mContainer) {
    gtk_widget_destroy(GTK_WIDGET(mContainer));
    mContainer = nullptr;
    MOZ_ASSERT(!mGdkWindow,
               "mGdkWindow should be NULL when mContainer is destroyed");
  } else if (mGdkWindow) {
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
}

nsIWidget* nsWindow::GetParent(void) { return mParent; }

float nsWindow::GetDPI() {
  float dpi = 96.0f;
  nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
  if (screen) {
    screen->GetDpi(&dpi);
  }
  return dpi;
}

double nsWindow::GetDefaultScaleInternal() {
  return FractionalScaleFactor() * gfxPlatformGtk::GetFontScaleFactor();
}

DesktopToLayoutDeviceScale nsWindow::GetDesktopToDeviceScale() {
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    return DesktopToLayoutDeviceScale(GdkCeiledScaleFactor());
  }
#endif

  // In Gtk/X11, we manage windows using device pixels.
  return DesktopToLayoutDeviceScale(1.0);
}

DesktopToLayoutDeviceScale nsWindow::GetDesktopToDeviceScaleByScreen() {
#ifdef MOZ_WAYLAND
  // In Wayland there's no way to get absolute position of the window and use it
  // to determine the screen factor of the monitor on which the window is
  // placed. The window is notified of the current scale factor but not at this
  // point, so the GdkScaleFactor can return wrong value which can lead to wrong
  // popup placement. We need to use parent's window scale factor for the new
  // one.
  if (GdkIsWaylandDisplay()) {
    nsView* view = nsView::GetViewFor(this);
    if (view) {
      nsView* parentView = view->GetParent();
      if (parentView) {
        nsIWidget* parentWidget = parentView->GetNearestWidget(nullptr);
        if (parentWidget) {
          return DesktopToLayoutDeviceScale(
              parentWidget->RoundsWidgetCoordinatesTo());
        }
        NS_WARNING("Widget has no parent");
      }
    } else {
      NS_WARNING("Cannot find widget view");
    }
  }
#endif
  return nsBaseWidget::GetDesktopToDeviceScale();
}

void nsWindow::SetParent(nsIWidget* aNewParent) {
  if (!mGdkWindow) {
    MOZ_ASSERT_UNREACHABLE("The native window has already been destroyed");
    return;
  }

  if (mContainer) {
    // FIXME bug 1469183
    NS_ERROR("nsWindow should not have a container here");
    return;
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
    return;
  }

  nsWindow* newParent = static_cast<nsWindow*>(aNewParent);
  GdkWindow* newParentWindow = nullptr;
  GtkWidget* newContainer = nullptr;
  if (aNewParent) {
    aNewParent->AddChild(this);
    newParentWindow = newParent->mGdkWindow;
    newContainer = newParent->GetMozContainerWidget();
  } else {
    // aNewParent is nullptr, but reparent to a hidden window to avoid
    // destroying the GdkWindow and its descendants.
    // An invisible container widget is needed to hold descendant
    // GtkWidgets.
    newContainer = EnsureInvisibleContainer();
    newParentWindow = gtk_widget_get_window(newContainer);
  }

  if (!newContainer) {
    // The new parent GdkWindow has been destroyed.
    MOZ_ASSERT(!newParentWindow || gdk_window_is_destroyed(newParentWindow),
               "live GdkWindow with no widget");
    Destroy();
  } else {
    if (newContainer != oldContainer) {
      MOZ_ASSERT(!gdk_window_is_destroyed(newParentWindow),
                 "destroyed GdkWindow with widget");
      SetWidgetForHierarchy(mGdkWindow, oldContainer, newContainer);

      if (oldContainer == gInvisibleContainer) {
        CheckDestroyInvisibleContainer();
      }
    }

    gdk_window_reparent(mGdkWindow, newParentWindow,
                        DevicePixelsToGdkCoordRoundDown(mBounds.x),
                        DevicePixelsToGdkCoordRoundDown(mBounds.y));
  }

  bool parentHasMappedToplevel = newParent && newParent->mHasMappedToplevel;
  if (mHasMappedToplevel != parentHasMappedToplevel) {
    SetHasMappedToplevel(parentHasMappedToplevel);
  }
}

bool nsWindow::WidgetTypeSupportsAcceleration() {
  if (IsSmallPopup()) {
    return false;
  }
  // Workaround for Bug 1479135
  // We draw transparent popups on non-compositing screens by SW as we don't
  // implement X shape masks in WebRender.
  if (mWindowType == eWindowType_popup) {
    return HasRemoteContent() && mCompositedScreen;
  }

  return true;
}

void nsWindow::ReparentNativeWidget(nsIWidget* aNewParent) {
  MOZ_ASSERT(aNewParent, "null widget");
  MOZ_ASSERT(!mIsDestroyed, "");
  MOZ_ASSERT(!static_cast<nsWindow*>(aNewParent)->mIsDestroyed, "");
  MOZ_ASSERT(!gdk_window_is_destroyed(mGdkWindow),
             "destroyed GdkWindow with widget");

  MOZ_ASSERT(
      !mParent,
      "nsWindow::ReparentNativeWidget() works on toplevel windows only.");

  auto* newParent = static_cast<nsWindow*>(aNewParent);
  GtkWindow* newParentWidget = GTK_WINDOW(newParent->GetGtkWidget());
  GtkWindow* shell = GTK_WINDOW(mShell);

  if (shell && gtk_window_get_transient_for(shell)) {
    gtk_window_set_transient_for(shell, newParentWidget);
  }
}

void nsWindow::SetModal(bool aModal) {
  LOG(("nsWindow::SetModal [%p] %d\n", (void*)this, aModal));
  if (mIsDestroyed) return;
  if (!mIsTopLevel || !mShell) return;
  gtk_window_set_modal(GTK_WINDOW(mShell), aModal ? TRUE : FALSE);
}

// nsIWidget method, which means IsShown.
bool nsWindow::IsVisible() const { return mIsShown; }

void nsWindow::RegisterTouchWindow() {
  mHandleTouchEvent = true;
  mTouches.Clear();
}

void nsWindow::ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) {
  if (!mIsTopLevel || !mShell || GdkIsWaylandDisplay()) {
    return;
  }

  double dpiScale = GetDefaultScale().scale;

  // we need to use the window size in logical screen pixels
  int32_t logWidth = std::max(NSToIntRound(mBounds.width / dpiScale), 1);
  int32_t logHeight = std::max(NSToIntRound(mBounds.height / dpiScale), 1);

  /* get our playing field. use the current screen, or failing that
    for any reason, use device caps for the default screen. */
  nsCOMPtr<nsIScreen> screen;
  nsCOMPtr<nsIScreenManager> screenmgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenmgr) {
    screenmgr->ScreenForRect(*aX, *aY, logWidth, logHeight,
                             getter_AddRefs(screen));
  }

  // We don't have any screen so leave the coordinates as is
  if (!screen) return;

  nsIntRect screenRect;
  if (mSizeMode != nsSizeMode_Fullscreen) {
    // For normalized windows, use the desktop work area.
    screen->GetAvailRectDisplayPix(&screenRect.x, &screenRect.y,
                                   &screenRect.width, &screenRect.height);
  } else {
    // For full screen windows, use the desktop.
    screen->GetRectDisplayPix(&screenRect.x, &screenRect.y, &screenRect.width,
                              &screenRect.height);
  }

  if (aAllowSlop) {
    if (*aX < screenRect.x - logWidth + kWindowPositionSlop) {
      *aX = screenRect.x - logWidth + kWindowPositionSlop;
    } else if (*aX >= screenRect.XMost() - kWindowPositionSlop) {
      *aX = screenRect.XMost() - kWindowPositionSlop;
    }

    if (*aY < screenRect.y - logHeight + kWindowPositionSlop) {
      *aY = screenRect.y - logHeight + kWindowPositionSlop;
    } else if (*aY >= screenRect.YMost() - kWindowPositionSlop) {
      *aY = screenRect.YMost() - kWindowPositionSlop;
    }
  } else {
    if (*aX < screenRect.x) {
      *aX = screenRect.x;
    } else if (*aX >= screenRect.XMost() - logWidth) {
      *aX = screenRect.XMost() - logWidth;
    }

    if (*aY < screenRect.y) {
      *aY = screenRect.y;
    } else if (*aY >= screenRect.YMost() - logHeight) {
      *aY = screenRect.YMost() - logHeight;
    }
  }
}

void nsWindow::SetSizeConstraints(const SizeConstraints& aConstraints) {
  mSizeConstraints.mMinSize = GetSafeWindowSize(aConstraints.mMinSize);
  mSizeConstraints.mMaxSize = GetSafeWindowSize(aConstraints.mMaxSize);

  ApplySizeConstraints();
}

void nsWindow::AddCSDDecorationSize(int* aWidth, int* aHeight) {
  if (mSizeState == nsSizeMode_Normal &&
      mGtkWindowDecoration == GTK_DECORATION_CLIENT && mDrawInTitlebar) {
    GtkBorder decorationSize = GetCSDDecorationSize(!mIsTopLevel);
    *aWidth += decorationSize.left + decorationSize.right;
    *aHeight += decorationSize.top + decorationSize.bottom;
  }
}

#ifdef MOZ_WAYLAND
bool nsWindow::GetCSDDecorationOffset(int* aDx, int* aDy) {
  if (mSizeState == nsSizeMode_Normal &&
      mGtkWindowDecoration == GTK_DECORATION_CLIENT && mDrawInTitlebar) {
    GtkBorder decorationSize = GetCSDDecorationSize(!mIsTopLevel);
    *aDx = decorationSize.left;
    *aDy = decorationSize.top;
    return true;
  }
  return false;
}
#endif

void nsWindow::ApplySizeConstraints(void) {
  if (mShell) {
    GdkGeometry geometry;
    geometry.min_width =
        DevicePixelsToGdkCoordRoundUp(mSizeConstraints.mMinSize.width);
    geometry.min_height =
        DevicePixelsToGdkCoordRoundUp(mSizeConstraints.mMinSize.height);
    geometry.max_width =
        DevicePixelsToGdkCoordRoundDown(mSizeConstraints.mMaxSize.width);
    geometry.max_height =
        DevicePixelsToGdkCoordRoundDown(mSizeConstraints.mMaxSize.height);

    uint32_t hints = 0;
    if (mSizeConstraints.mMinSize != LayoutDeviceIntSize(0, 0)) {
      if (GdkIsWaylandDisplay()) {
        gtk_widget_set_size_request(GTK_WIDGET(mContainer), geometry.min_width,
                                    geometry.min_height);
      }
      AddCSDDecorationSize(&geometry.min_width, &geometry.min_height);
      hints |= GDK_HINT_MIN_SIZE;
      LOG(("nsWindow::ApplySizeConstraints [%p] min size %d %d\n", (void*)this,
           geometry.min_width, geometry.min_height));
    }
    if (mSizeConstraints.mMaxSize !=
        LayoutDeviceIntSize(NS_MAXSIZE, NS_MAXSIZE)) {
      AddCSDDecorationSize(&geometry.max_width, &geometry.max_height);
      hints |= GDK_HINT_MAX_SIZE;
      LOG(("nsWindow::ApplySizeConstraints [%p] max size %d %d\n", (void*)this,
           geometry.max_width, geometry.max_height));
    }

    if (mAspectRatio != 0.0f) {
      geometry.min_aspect = mAspectRatio;
      geometry.max_aspect = mAspectRatio;
      hints |= GDK_HINT_ASPECT;
    }

    gtk_window_set_geometry_hints(GTK_WINDOW(mShell), nullptr, &geometry,
                                  GdkWindowHints(hints));
  }
}

void nsWindow::Show(bool aState) {
  if (aState == mIsShown) return;

  // Clear our cached resources when the window is hidden.
  if (mIsShown && !aState) {
    ClearCachedResources();
  }

  mIsShown = aState;

  LOG(("nsWindow::Show [%p] state %d\n", (void*)this, aState));

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
    return;
  }

  // If someone is hiding this widget, clear any needing show flag.
  if (!aState) mNeedsShow = false;

#ifdef ACCESSIBILITY
  if (aState && a11y::ShouldA11yBeEnabled()) CreateRootAccessible();
#endif

  NativeShow(aState);
}

void nsWindow::ResizeInt(int aX, int aY, int aWidth, int aHeight, bool aMove,
                         bool aRepaint) {
  LOG(("nsWindow::ResizeInt [%p] x:%d y:%d -> w:%d h:%d repaint %d aMove %d\n",
       (void*)this, aX, aY, aWidth, aHeight, aRepaint, aMove));

  ConstrainSize(&aWidth, &aHeight);

  LOG(("  ConstrainSize: w:%d h;%d\n", aWidth, aHeight));

  // If we used to have insane bounds, we may have skipped actually positioning
  // the widget in NativeMoveResizeWaylandPopup, in which case we need to
  // actually position it now as well.
  const bool hadInsaneWaylandPopupDimensions =
      !AreBoundsSane() && IsWaylandPopup();

  if (aMove) {
    mBounds.x = aX;
    mBounds.y = aY;
  }

  // For top-level windows, aWidth and aHeight should possibly be
  // interpreted as frame bounds, but NativeResize treats these as window
  // bounds (Bug 581866).
  mBounds.SizeTo(aWidth, aHeight);

  // We set correct mBounds in advance here. This can be invalided by state
  // event.
  mBoundsAreValid = true;

  // Recalculate aspect ratio when resized from DOM
  if (mAspectRatio != 0.0) {
    LockAspectRatio(true);
  }

  if (!mCreated) return;

  if (aMove || mPreferredPopupRectFlushed || hadInsaneWaylandPopupDimensions) {
    LOG_POPUP(("  Need also to move, flushed: %d, bounds were insane: %d\n",
               mPreferredPopupRectFlushed, hadInsaneWaylandPopupDimensions));
    NativeMoveResize();
  } else {
    NativeResize();
  }

  NotifyRollupGeometryChange();

  // send a resize notification if this is a toplevel
  if (mIsTopLevel || mListenForResizes) {
    DispatchResized();
  }
}

void nsWindow::Resize(double aWidth, double aHeight, bool aRepaint) {
  LOG(("nsWindow::Resize [%p] %f %f\n", (void*)this, aWidth, aHeight));

  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t width = NSToIntRound(scale * aWidth);
  int32_t height = NSToIntRound(scale * aHeight);

  ResizeInt(0, 0, width, height, /* aMove */ false, aRepaint);
}

void nsWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) {
  LOG(("nsWindow::Resize [%p] [%f,%f] -> [%f x %f] repaint %d\n", (void*)this,
       aX, aY, aWidth, aHeight, aRepaint));

  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t width = NSToIntRound(scale * aWidth);
  int32_t height = NSToIntRound(scale * aHeight);

  int32_t x = NSToIntRound(scale * aX);
  int32_t y = NSToIntRound(scale * aY);

  ResizeInt(x, y, width, height, /* aMove */ true, aRepaint);
}

void nsWindow::Enable(bool aState) { mEnabled = aState; }

bool nsWindow::IsEnabled() const { return mEnabled; }

void nsWindow::Move(double aX, double aY) {
  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t x = NSToIntRound(aX * scale);
  int32_t y = NSToIntRound(aY * scale);

  LOG(("nsWindow::Move [%p] to %d %d\n", (void*)this, x, y));

  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog) {
    SetSizeMode(nsSizeMode_Normal);
  }

  // Since a popup window's x/y coordinates are in relation to to
  // the parent, the parent might have moved so we always move a
  // popup window.
  LOG(("  bounds %d %d\n", mBounds.y, mBounds.y));
  if (x == mBounds.x && y == mBounds.y && mWindowType != eWindowType_popup) {
    LOG(("  position is the same, return\n"));
    return;
  }

  // XXX Should we do some AreBoundsSane check here?

  mBounds.x = x;
  mBounds.y = y;

  if (!mCreated) {
    LOG(("  is not created, return.\n"));
    return;
  }

  if (IsWaylandPopup()) {
    int32_t p2a = AppUnitsPerCSSPixel() / gfxPlatformGtk::GetFontScaleFactor();
    if (mPreferredPopupRect.x != mBounds.x * p2a &&
        mPreferredPopupRect.y != mBounds.y * p2a) {
      NativeMove();
      NotifyRollupGeometryChange();
    } else {
      LOG(("  mBounds same as mPreferredPopupRect, no need to move"));
    }
  } else {
    NativeMove();
    NotifyRollupGeometryChange();
  }
}

bool nsWindow::IsPopup() {
  return mIsTopLevel && mWindowType == eWindowType_popup;
}

bool nsWindow::IsWaylandPopup() { return GdkIsWaylandDisplay() && IsPopup(); }

static nsMenuPopupFrame* GetMenuPopupFrame(nsIFrame* aFrame) {
  if (aFrame) {
    nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(aFrame);
    return menuPopupFrame;
  }
  return nullptr;
}

void nsWindow::AppendPopupToHierarchyList(nsWindow* aToplevelWindow) {
  mWaylandToplevel = aToplevelWindow;

  nsWindow* popup = aToplevelWindow;
  while (popup && popup->mWaylandPopupNext) {
    popup = popup->mWaylandPopupNext;
  }
  popup->mWaylandPopupNext = this;

  mWaylandPopupPrev = popup;
  mWaylandPopupNext = nullptr;
  mPopupChanged = true;
  mPopupClosed = false;
}

void nsWindow::RemovePopupFromHierarchyList() {
  // We're already removed from the popup hierarchy
  if (!IsInPopupHierarchy()) {
    return;
  }
  mWaylandPopupPrev->mWaylandPopupNext = mWaylandPopupNext;
  if (mWaylandPopupNext) {
    mWaylandPopupNext->mWaylandPopupPrev = mWaylandPopupPrev;
    mWaylandPopupNext->mPopupChanged = true;
  }
  mWaylandPopupNext = mWaylandPopupPrev = nullptr;
}

void nsWindow::HideWaylandWindow() {
  LOG(("nsWindow::HideWaylandWindow: [%p]\n", this));
  PauseCompositorHiddenWindow();
  gtk_widget_hide(mShell);
}

void nsWindow::WaylandPopupMarkAsClosed() {
  LOG_POPUP(("nsWindow::WaylandPopupMarkAsClosed: [%p]\n", this));
  mPopupClosed = true;
  // If we have any child popup window notify it about
  // parent switch.
  if (mWaylandPopupNext) {
    mWaylandPopupNext->mPopupChanged = true;
  }
}

nsWindow* nsWindow::WaylandPopupFindLast(nsWindow* aPopup) {
  while (aPopup && aPopup->mWaylandPopupNext) {
    aPopup = aPopup->mWaylandPopupNext;
  }
  return aPopup;
}

// Hide and potentially removes popup from popup hierarchy.
void nsWindow::HideWaylandPopupWindow(bool aTemporaryHide,
                                      bool aRemoveFromPopupList) {
  LOG_POPUP(("nsWindow::HideWaylandPopupWindow: [%p] remove from list %d\n",
             this, aRemoveFromPopupList));
  if (aRemoveFromPopupList) {
    RemovePopupFromHierarchyList();
  }

  if (!mPopupClosed) {
    mPopupClosed = !aTemporaryHide;
  }

  bool visible = gtk_widget_is_visible(mShell);
  LOG_POPUP(("  gtk_widget_is_visible() = %d\n", visible));

  // Restore only popups which are really visible
  mPopupTemporaryHidden = aTemporaryHide && visible;

  // Hide only visible popups or popups closed pernamently.
  if (visible) {
    HideWaylandWindow();
  }

  // Clear rendering transactions of closed window and disable rendering to it
  // (see https://bugzilla.mozilla.org/show_bug.cgi?id=1717451#c27
  // for details).
  if (mPopupClosed) {
    RevokeTransactionIdAllocator();
  }
}

void nsWindow::HideWaylandToplevelWindow() {
  LOG(("nsWindow::HideWaylandToplevelWindow: [%p]\n", this));
  if (mWaylandPopupNext) {
    nsWindow* popup = WaylandPopupFindLast(mWaylandPopupNext);
    while (popup->mWaylandToplevel != nullptr) {
      nsWindow* prev = popup->mWaylandPopupPrev;
      popup->HideWaylandPopupWindow(/* aTemporaryHide */ false,
                                    /* aRemoveFromPopupList */ true);
      popup = prev;
    }
  }
  HideWaylandWindow();
}

void nsWindow::WaylandPopupRemoveClosedPopups() {
  LOG(("nsWindow::WaylandPopupRemoveClosedPopups: [%p]\n", this));
  nsWindow* popup = this;
  while (popup) {
    nsWindow* next = popup->mWaylandPopupNext;
    if (popup->mPopupClosed) {
      popup->HideWaylandPopupWindow(/* aTemporaryHide */ false,
                                    /* aRemoveFromPopupList */ true);
    }
    popup = next;
  }
}

// Hide all tooltips except the latest one.
void nsWindow::WaylandPopupHideTooltips() {
  LOG_POPUP(("nsWindow::WaylandPopupHideTooltips"));
  MOZ_ASSERT(mWaylandToplevel == nullptr, "Should be called on toplevel only!");

  nsWindow* popup = mWaylandPopupNext;
  while (popup && popup->mWaylandPopupNext) {
    if (popup->mPopupType == ePopupTypeTooltip) {
      LOG_POPUP(("  hidding tooltip [%p]", popup));
      popup->WaylandPopupMarkAsClosed();
    }
    popup = popup->mWaylandPopupNext;
  }
}

// We can't show popups with remote content or overflow popups
// on top of regular ones.
// If there's any remote popup opened, close all parent popups of it.
void nsWindow::CloseAllPopupsBeforeRemotePopup() {
  LOG_POPUP(("nsWindow::CloseAllPopupsBeforeRemotePopup"));
  MOZ_ASSERT(mWaylandToplevel == nullptr, "Should be called on toplevel only!");

  // Don't waste time when there's only one popup opened.
  if (mWaylandPopupNext->mWaylandPopupNext == nullptr) {
    return;
  }

  // Find the first opened remote content popup
  nsWindow* remotePopup = mWaylandPopupNext;
  while (remotePopup) {
    if (remotePopup->HasRemoteContent() ||
        remotePopup->IsWidgetOverflowWindow()) {
      LOG_POPUP(("  remote popup [%p]", remotePopup));
      break;
    }
    remotePopup = remotePopup->mWaylandPopupNext;
  }

  if (!remotePopup) {
    return;
  }

  // ...hide opened popups before the remote one.
  nsWindow* popup = mWaylandPopupNext;
  while (popup && popup != remotePopup) {
    LOG_POPUP(("  hidding popup [%p]", popup));
    popup->WaylandPopupMarkAsClosed();
    popup = popup->mWaylandPopupNext;
  }
}

static void GetLayoutPopupWidgetChain(
    nsTArray<nsIWidget*>* aLayoutWidgetHierarchy) {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  pm->GetSubmenuWidgetChain(aLayoutWidgetHierarchy);
  aLayoutWidgetHierarchy->Reverse();
}

// Compare 'this' popup position in Wayland widget hierarchy
// (mWaylandPopupPrev/mWaylandPopupNext) with
// 'this' popup position in layout hierarchy.
//
// When aMustMatchParent is true we also request
// 'this' parents match, i.e. 'this' has the same parent in
// both layout and widget hierarchy.
bool nsWindow::IsPopupInLayoutPopupChain(
    nsTArray<nsIWidget*>* aLayoutWidgetHierarchy, bool aMustMatchParent) {
  int len = (int)aLayoutWidgetHierarchy->Length();
  for (int i = 0; i < len; i++) {
    if (this == (*aLayoutWidgetHierarchy)[i]) {
      if (!aMustMatchParent) {
        return true;
      }

      // Find correct parent popup for 'this' according to widget
      // hierarchy. That means we need to skip closed popups.
      nsWindow* parentPopup = nullptr;
      if (mWaylandPopupPrev != mWaylandToplevel) {
        parentPopup = mWaylandPopupPrev;
        while (parentPopup != mWaylandToplevel && parentPopup->mPopupClosed) {
          parentPopup = parentPopup->mWaylandPopupPrev;
        }
      }

      if (i == 0) {
        // We found 'this' popups as a first popup in layout hierarchy.
        // It matches layout hierarchy if it's first widget also in
        // wayland widget hierarchy (i.e. parent is null).
        return parentPopup == nullptr;
      }

      return parentPopup == (*aLayoutWidgetHierarchy)[i - 1];
    }
  }
  return false;
}

// Hide popups which are not in popup chain.
void nsWindow::WaylandPopupHierarchyHideByLayout(
    nsTArray<nsIWidget*>* aLayoutWidgetHierarchy) {
  LOG_POPUP(("nsWindow::WaylandPopupHierarchyHideByLayout"));
  MOZ_ASSERT(mWaylandToplevel == nullptr, "Should be called on toplevel only!");

  // Hide all popups which are not in layout popup chain
  nsWindow* popup = mWaylandPopupNext;
  while (popup) {
    // Tooltips are not tracked in layout chain
    if (!popup->mPopupClosed && popup->mPopupType != ePopupTypeTooltip) {
      if (!popup->IsPopupInLayoutPopupChain(aLayoutWidgetHierarchy,
                                            /* aMustMatchParent */ false)) {
        LOG_POPUP(("  hidding popup [%p]", popup));
        popup->WaylandPopupMarkAsClosed();
      }
    }
    popup = popup->mWaylandPopupNext;
  }
}

// Mark popups outside of layout hierarchy
void nsWindow::WaylandPopupHierarchyValidateByLayout(
    nsTArray<nsIWidget*>* aLayoutWidgetHierarchy) {
  LOG_POPUP(("nsWindow::WaylandPopupHierarchyValidateByLayout"));
  nsWindow* popup = mWaylandPopupNext;
  while (popup) {
    if (popup->mPopupType == ePopupTypeTooltip) {
      popup->mPopupMatchesLayout = true;
    } else if (!popup->mPopupClosed) {
      popup->mPopupMatchesLayout = popup->IsPopupInLayoutPopupChain(
          aLayoutWidgetHierarchy, /* aMustMatchParent */ true);
      LOG_POPUP(("  popup [%p] parent window [%p] matches layout %d\n",
                 (void*)popup, (void*)popup->mWaylandPopupPrev,
                 popup->mPopupMatchesLayout));
    }
    popup = popup->mWaylandPopupNext;
  }
}

void nsWindow::WaylandPopupHierarchyHideTemporary() {
  LOG_POPUP(("nsWindow::WaylandPopupHierarchyHideTemporary() [%p]", this));
  nsWindow* popup = WaylandPopupFindLast(this);
  while (popup) {
    LOG_POPUP(("  temporary hidding popup [%p]", popup));
    nsWindow* prev = popup->mWaylandPopupPrev;
    popup->HideWaylandPopupWindow(/* aTemporaryHide */ true,
                                  /* aRemoveFromPopupList */ false);
    if (popup == this) {
      break;
    }
    popup = prev;
  }
}

void nsWindow::WaylandPopupHierarchyShowTemporaryHidden() {
  LOG_POPUP(("nsWindow::WaylandPopupHierarchyShowTemporaryHidden()"));
  nsWindow* popup = this;
  while (popup) {
    if (popup->mPopupTemporaryHidden) {
      popup->mPopupTemporaryHidden = false;
      LOG_POPUP(("  showing temporary hidden popup [%p]", popup));
      gtk_widget_show(popup->mShell);
    }
    popup = popup->mWaylandPopupNext;
  }
}

void nsWindow::WaylandPopupHierarchyCalculatePositions() {
  LOG_POPUP(("nsWindow::WaylandPopupHierarchyCalculatePositions()"));

  // Set widget hierarchy in Gtk
  nsWindow* popup = mWaylandToplevel->mWaylandPopupNext;
  while (popup) {
    LOG_POPUP(("  popup [%p] set parent window [%p]", (void*)popup,
               (void*)popup->mWaylandPopupPrev));
    gtk_window_set_transient_for(GTK_WINDOW(popup->mShell),
                                 GTK_WINDOW(popup->mWaylandPopupPrev->mShell));
    popup = popup->mWaylandPopupNext;
  }

  popup = this;
  while (popup) {
    // Anchored window has mPopupPosition already calculated against
    // its parent, no need to recalculate.
    LOG_POPUP(("  popup [%p] bounds [%d, %d] -> [%d x %d]", popup,
               (int)(popup->mBounds.x / FractionalScaleFactor()),
               (int)(popup->mBounds.y / FractionalScaleFactor()),
               (int)(popup->mBounds.width / FractionalScaleFactor()),
               (int)(popup->mBounds.height / FractionalScaleFactor())));
#ifdef MOZ_LOGGING
    if (LOG_ENABLED()) {
      nsMenuPopupFrame* popupFrame = GetMenuPopupFrame(GetFrame());
      if (popupFrame) {
        auto pos = popupFrame->GetPosition();
        auto size = popupFrame->GetSize();
        int32_t p2a =
            AppUnitsPerCSSPixel() / gfxPlatformGtk::GetFontScaleFactor();
        LOG_POPUP(("  popup [%p] layout [%d, %d] -> [%d x %d]", popup,
                   pos.x / p2a, pos.y / p2a, size.width / p2a,
                   size.height / p2a));
      }
    }
#endif
    if (popup->mPopupContextMenu && !popup->mPopupAnchored) {
      LOG_POPUP(("  popup [%p] is first context menu", popup));
      static int menuOffsetX =
          LookAndFeel::GetInt(LookAndFeel::IntID::ContextMenuOffsetHorizontal);
      static int menuOffsetY =
          LookAndFeel::GetInt(LookAndFeel::IntID::ContextMenuOffsetVertical);
      popup->mRelativePopupPosition = popup->mPopupPosition;
      mRelativePopupOffset.x = menuOffsetX;
      mRelativePopupOffset.y = menuOffsetY;
    } else if (popup->mPopupAnchored) {
      LOG_POPUP(("  popup [%p] is anchored", popup));
      if (!popup->mPopupMatchesLayout) {
        NS_WARNING("Anchored popup does not match layout!");
      }
      popup->mRelativePopupPosition = popup->mPopupPosition;
    } else if (popup->mWaylandPopupPrev->mWaylandToplevel == nullptr) {
      LOG_POPUP(("  popup [%p] has toplevel as parent", popup));
      popup->mRelativePopupPosition = popup->mPopupPosition;
    } else {
      int parentX, parentY;
      GetParentPosition(&parentX, &parentY);

      LOG_POPUP(("  popup [%p] uses transformed coordinates\n", popup));
      LOG_POPUP(("    parent position [%d, %d]\n", parentX, parentY));
      LOG_POPUP(("    popup position [%d, %d]\n", popup->mPopupPosition.x,
                 popup->mPopupPosition.y));

      popup->mRelativePopupPosition.x = popup->mPopupPosition.x - parentX;
      popup->mRelativePopupPosition.y = popup->mPopupPosition.y - parentY;
    }
    LOG_POPUP(
        ("  popup [%p] transformed popup coordinates from [%d, %d] to [%d, %d]",
         popup, popup->mPopupPosition.x, popup->mPopupPosition.y,
         popup->mRelativePopupPosition.x, popup->mRelativePopupPosition.y));
    popup = popup->mWaylandPopupNext;
  }
}

// Gtk don't allow us to place tooltip popup on x < 0 & y < 0 coordinates
// see https://gitlab.gnome.org/GNOME/gtk/-/issues/4071
// so just remove such popups
//
// Returns:
//     false - positions are valid or we still need to reposition/show some
//             popups.
//     true  - last (and only changed) popup was removed, no need to do any
//             other actions.
bool nsWindow::IsTooltipWithNegativeRelativePositionRemoved() {
  LOG_POPUP(("nsWindow::IsTooltipWithNegativeRelativePositionRemoved()"));

  nsWindow* firstChangedPopup = this;
  nsWindow* popup = this;

  while (popup) {
    if (popup->mPopupType == ePopupTypeTooltip) {
      if (popup->mRelativePopupPosition.x < 0 &&
          popup->mRelativePopupPosition.y < 0) {
        LOG_POPUP(
            ("  removing tooltip with both negative coordinates [%p]", popup));
        popup->HideWaylandPopupWindow(/* aTemporaryHide */ false,
                                      /* aRemoveFromPopupList */ true);
        return firstChangedPopup == popup;
      }
      break;
    }
    popup = popup->mWaylandPopupNext;
  }

  return false;
}

// The MenuList popups are used as dropdown menus for example in WebRTC
// microphone/camera chooser or autocomplete widgets.
bool nsWindow::WaylandPopupIsMenu() {
  nsMenuPopupFrame* menuPopupFrame = GetMenuPopupFrame(GetFrame());
  if (menuPopupFrame) {
    return mPopupType == ePopupTypeMenu && !menuPopupFrame->IsMenuList();
  }
  return false;
}

bool nsWindow::WaylandPopupIsContextMenu() {
  nsMenuPopupFrame* popupFrame = GetMenuPopupFrame(GetFrame());
  if (!popupFrame) {
    return false;
  }
  return popupFrame->IsContextMenu();
}

bool nsWindow::WaylandPopupIsPermanent() {
  nsMenuPopupFrame* popupFrame = GetMenuPopupFrame(GetFrame());
  if (!popupFrame) {
    // We can always hide popups without frames.
    return false;
  }
  return popupFrame->IsNoAutoHide();
}

bool nsWindow::WaylandPopupIsAnchored() {
  nsMenuPopupFrame* popupFrame = GetMenuPopupFrame(GetFrame());
  if (!popupFrame) {
    // We can always hide popups without frames.
    return false;
  }
  return popupFrame->GetAnchor() != nullptr;
}

bool nsWindow::IsWidgetOverflowWindow() {
  if (this->GetFrame() && this->GetFrame()->GetContent()->GetID()) {
    nsCString nodeId;
    this->GetFrame()->GetContent()->GetID()->ToUTF8String(nodeId);
    return nodeId.Equals("widget-overflow");
  }
  return false;
}

void nsWindow::GetParentPosition(int* aX, int* aY) {
  *aX = *aY = 0;
  GtkWindow* parentGtkWindow = gtk_window_get_transient_for(GTK_WINDOW(mShell));
  if (!parentGtkWindow || !GTK_IS_WIDGET(parentGtkWindow)) {
    NS_WARNING("Popup has no parent!");
    return;
  }
  GetWindowOrigin(gtk_widget_get_window(GTK_WIDGET(parentGtkWindow)), aX, aY);
}

#ifdef MOZ_LOGGING
void nsWindow::LogPopupHierarchy() {
  if (!LOG_ENABLED()) {
    return;
  }

  LOG_POPUP(("Widget Popup Hierarchy:\n"));
  if (!mWaylandToplevel->mWaylandPopupNext) {
    LOG_POPUP(("    Empty\n"));
  } else {
    int indent = 4;
    nsWindow* popup = mWaylandToplevel->mWaylandPopupNext;
    while (popup) {
      nsPrintfCString indentString("%*s", indent, " ");
      LOG_POPUP(
          ("%s %s %s nsWindow [%p] Menu %d Permanent %d ContextMenu %d "
           "Anchored %d Visible %d\n",
           indentString.get(), popup->GetWindowNodeName().get(),
           popup->GetPopupTypeName().get(), popup, popup->WaylandPopupIsMenu(),
           popup->WaylandPopupIsPermanent(), popup->mPopupContextMenu,
           popup->mPopupAnchored, gtk_widget_is_visible(popup->mShell)));
      indent += 4;
      popup = popup->mWaylandPopupNext;
    }
  }

  LOG_POPUP(("Layout Popup Hierarchy:\n"));
  AutoTArray<nsIWidget*, 5> widgetChain;
  GetLayoutPopupWidgetChain(&widgetChain);
  if (widgetChain.Length() == 0) {
    LOG_POPUP(("    Empty\n"));
  } else {
    for (unsigned long i = 0; i < widgetChain.Length(); i++) {
      nsWindow* window = static_cast<nsWindow*>(widgetChain[i]);
      nsPrintfCString indentString("%*s", (int)(i + 1) * 4, " ");
      if (window) {
        LOG_POPUP(
            ("%s %s %s nsWindow [%p] Menu %d Permanent %d ContextMenu %d "
             "Anchored %d Visible %d\n",
             indentString.get(), window->GetWindowNodeName().get(),
             window->GetPopupTypeName().get(), window,
             window->WaylandPopupIsMenu(), window->WaylandPopupIsPermanent(),
             window->mPopupContextMenu, window->mPopupAnchored,
             gtk_widget_is_visible(window->mShell)));
      } else {
        LOG_POPUP(("%s null window\n", indentString.get()));
      }
    }
  }
}
#endif

nsWindow* nsWindow::WaylandPopupGetTopmostWindow() {
  nsView* view = nsView::GetViewFor(this);
  if (view) {
    nsView* parentView = view->GetParent();
    if (parentView) {
      nsIWidget* parentWidget = parentView->GetNearestWidget(nullptr);
      if (parentWidget) {
        nsWindow* parentnsWindow = static_cast<nsWindow*>(parentWidget);
        LOG_POPUP(("  Topmost window: %p [nsWindow]\n", parentnsWindow));
        return parentnsWindow;
      }
    }
  }
  return nullptr;
}

bool nsWindow::WaylandPopupNeedsTrackInHierarchy() {
  MOZ_RELEASE_ASSERT(!mIsDragPopup);

  if (mPopupTrackInHierarchyConfigured) {
    return mPopupTrackInHierarchy;
  }

  nsMenuPopupFrame* popupFrame = GetMenuPopupFrame(GetFrame());
  if (!popupFrame) {
    return false;
  }
  mPopupTrackInHierarchyConfigured = true;

  // We have nsMenuPopupFrame so we can configure the popup now.
  mPopupTrackInHierarchy = !WaylandPopupIsPermanent();
  mPopupAnchored = WaylandPopupIsAnchored();
  mPopupContextMenu = WaylandPopupIsContextMenu();

  // See gdkwindow-wayland.c and
  // should_map_as_popup()/should_map_as_subsurface()
  GdkWindowTypeHint gtkTypeHint;
  switch (mPopupHint) {
    case ePopupTypeMenu:
      // GDK_WINDOW_TYPE_HINT_POPUP_MENU is mapped as xdg_popup by default.
      // We use this type for all menu popups.
      gtkTypeHint = GDK_WINDOW_TYPE_HINT_POPUP_MENU;
      break;
    case ePopupTypeTooltip:
      gtkTypeHint = GDK_WINDOW_TYPE_HINT_TOOLTIP;
      break;
    default:  // popup panel type
      // GDK_WINDOW_TYPE_HINT_UTILITY is mapped as wl_subsurface
      // by default. It's used for panels attached to toplevel
      // window.
      gtkTypeHint = GDK_WINDOW_TYPE_HINT_UTILITY;
      break;
  }

  if (!mPopupTrackInHierarchy) {
    gtkTypeHint = GDK_WINDOW_TYPE_HINT_UTILITY;
  }
  LOG_POPUP(
      ("nsWindow::WaylandPopupNeedsTrackInHierarchy [%p] tracked %d anchored "
       "%d\n",
       (void*)this, mPopupTrackInHierarchy, mPopupAnchored));
  gtk_window_set_type_hint(GTK_WINDOW(mShell), gtkTypeHint);
  return mPopupTrackInHierarchy;
}

bool nsWindow::IsInPopupHierarchy() {
  return mPopupTrackInHierarchy && mWaylandToplevel && mWaylandPopupPrev;
}

void nsWindow::AddWindowToPopupHierarchy() {
  LOG_POPUP(("nsWindow::AddWindowToPopupHierarchy [%p]\n", (void*)this));
#if DEBUG
  if (this->GetFrame() && this->GetFrame()->GetContent()->GetID()) {
    nsCString nodeId;
    this->GetFrame()->GetContent()->GetID()->ToUTF8String(nodeId);
    LOG_POPUP(("  popup node id=%s\n", nodeId.get()));
  }
#endif
  if (!GetFrame()) {
    LOG_POPUP(("  Window without frame cannot be added as popup!\n"));
    return;
  }

  // Check if we're already in the hierarchy
  if (!IsInPopupHierarchy()) {
    mWaylandToplevel = WaylandPopupGetTopmostWindow();
    AppendPopupToHierarchyList(mWaylandToplevel);
  }
}

// Wayland keeps strong popup window hierarchy. We need to track active
// (visible) popup windows and make sure we hide popup on the same level
// before we open another one on that level. It means that every open
// popup needs to have an unique parent.
void nsWindow::UpdateWaylandPopupHierarchy() {
  LOG_POPUP(("nsWindow::UpdateWaylandPopupHierarchy [%p]\n", (void*)this));

  // This popup hasn't been added to popup hierarchy yet so no need to
  // do any configurations.
  if (!IsInPopupHierarchy()) {
    LOG_POPUP(("  popup [%p] isn't in hierarchy\n", (void*)this));
    return;
  }

#ifdef MOZ_LOGGING
  LogPopupHierarchy();
  auto printPopupHierarchy = MakeScopeExit([&] { LogPopupHierarchy(); });
#endif

  // Hide all tooltips without the last one. Tooltip can't be popup parent.
  mWaylandToplevel->WaylandPopupHideTooltips();

  // Check if we have any remote content / overflow window in hierarchy.
  // We can't attach such widget on top of other popup.
  mWaylandToplevel->CloseAllPopupsBeforeRemotePopup();

  // Check if your popup hierarchy matches layout hierarchy.
  // For instance we should not connect hamburger menu on top
  // of context menu.
  // Close all popups from different layout chains if possible.
  AutoTArray<nsIWidget*, 5> layoutPopupWidgetChain;
  GetLayoutPopupWidgetChain(&layoutPopupWidgetChain);

  mWaylandToplevel->WaylandPopupHierarchyHideByLayout(&layoutPopupWidgetChain);
  mWaylandToplevel->WaylandPopupHierarchyValidateByLayout(
      &layoutPopupWidgetChain);

  // Now we have Popup hierarchy complete.
  // Find first unchanged (and still open) popup to start with hierarchy
  // changes.
  nsWindow* changedPopup = mWaylandToplevel->mWaylandPopupNext;
  while (changedPopup) {
    // Stop when parent of this popup was changed and we need to recalc
    // popup position.
    if (changedPopup->mPopupChanged) {
      break;
    }
    // Stop when this popup is closed.
    if (changedPopup->mPopupClosed) {
      break;
    }
    changedPopup = changedPopup->mWaylandPopupNext;
  }

  // We don't need to recompute popup positions, quit now.
  if (!changedPopup) {
    LOG_POPUP(("  changed Popup is null, quit.\n"));
    return;
  }

  LOG_POPUP(("  first changed popup [%p]\n", (void*)changedPopup));

  // Hide parent popups if necessary (there are layout discontinuity)
  // reposition the popup and show them again.
  changedPopup->WaylandPopupHierarchyHideTemporary();

  nsWindow* parentOfchangedPopup = nullptr;
  if (changedPopup->mPopupClosed) {
    parentOfchangedPopup = changedPopup->mWaylandPopupPrev;
  }
  changedPopup->WaylandPopupRemoveClosedPopups();

  // It's possible that changedPopup was removed from widget hierarchy,
  // in such case use child popup of the removed one if there's any.
  if (!changedPopup->IsInPopupHierarchy()) {
    if (!parentOfchangedPopup || !parentOfchangedPopup->mWaylandPopupNext) {
      LOG_POPUP(("  last popup was removed, quit.\n"));
      return;
    }
    changedPopup = parentOfchangedPopup->mWaylandPopupNext;
  }

  GetLayoutPopupWidgetChain(&layoutPopupWidgetChain);
  mWaylandToplevel->WaylandPopupHierarchyValidateByLayout(
      &layoutPopupWidgetChain);

  changedPopup->WaylandPopupHierarchyCalculatePositions();
  if (changedPopup->IsTooltipWithNegativeRelativePositionRemoved()) {
    return;
  }

  nsWindow* popup = changedPopup;
  while (popup) {
    // We can use move_to_rect only when popups in popup hierarchy matches
    // layout hierarchy as move_to_rect request that parent/child
    // popups are adjacent.
    bool useMoveToRect = popup->mPopupMatchesLayout;
    if (useMoveToRect) {
      // We use move_to_rect when:
      // - Popup is anchored, i.e. it has an anchor defined by layout
      //   (mPopupAnchored).
      // - Popup isn't anchored but it has toplevel as parent, i.e.
      //   it's first popup.
      useMoveToRect = (popup->mPopupAnchored ||
                       (!popup->mPopupAnchored &&
                        popup->mWaylandPopupPrev->mWaylandToplevel == nullptr));
    }
    LOG_POPUP(
        ("  popup [%p] matches layout [%d] anchored [%d] first popup [%d] use "
         "move-to-rect %d\n",
         popup, popup->mPopupMatchesLayout, popup->mPopupAnchored,
         popup->mWaylandPopupPrev->mWaylandToplevel == nullptr, useMoveToRect));
    popup->WaylandPopupMove(useMoveToRect);
    popup->mPopupChanged = false;
    popup = popup->mWaylandPopupNext;
  }

  changedPopup->WaylandPopupHierarchyShowTemporaryHidden();
}

static void NativeMoveResizeCallback(GdkWindow* window,
                                     const GdkRectangle* flipped_rect,
                                     const GdkRectangle* final_rect,
                                     gboolean flipped_x, gboolean flipped_y,
                                     void* aWindow) {
  LOG_POPUP(("NativeMoveResizeCallback [%p] flipped_x %d flipped_y %d\n",
             aWindow, flipped_x, flipped_y));
  LOG_POPUP(("  new position [%d, %d] -> [%d x %d]", final_rect->x,
             final_rect->y, final_rect->width, final_rect->height));
  nsWindow* wnd = get_window_for_gdk_window(window);

  wnd->NativeMoveResizeWaylandPopupCallback(final_rect, flipped_x, flipped_y);
}

void nsWindow::NativeMoveResizeWaylandPopupCallback(
    const GdkRectangle* aFinalSize, bool aFlippedX, bool aFlippedY) {
  mWaitingForMoveToRectCallback = false;

  // We ignore the callback position data because the another resize has been
  // called before the callback have been triggered.
  if (mNewSizeAfterMoveToRect.height > 0 || mNewSizeAfterMoveToRect.width > 0) {
    LOG_POPUP(
        ("  Another resize called during waiting for callback, calling "
         "Resize(%d, %d)\n",
         mNewSizeAfterMoveToRect.width, mNewSizeAfterMoveToRect.height));
    // Set the preferred size to zero to avoid wrong size of popup because the
    // mPreferredPopupRect is used in nsMenuPopupFrame to set dimensions
    mPreferredPopupRect = nsRect(0, 0, 0, 0);

    // We need to schedule another resize because the window has been resized
    // again before callback was called.
    Resize(mNewSizeAfterMoveToRect.width, mNewSizeAfterMoveToRect.height, true);
    DispatchResized();
    mNewSizeAfterMoveToRect.width = mNewSizeAfterMoveToRect.height = 0;
    return;
  }

  int parentX, parentY;
  GetParentPosition(&parentX, &parentY);

  parentX = GdkCoordToDevicePixels(parentX);
  parentY = GdkCoordToDevicePixels(parentY);

  LOG_POPUP(("  orig mBounds [%d, %d] -> [%d x %d]\n", mBounds.x, mBounds.y,
             mBounds.width, mBounds.height));

  LayoutDeviceIntRect newBounds(0, 0, aFinalSize->width, aFinalSize->height);
  newBounds.x = GdkCoordToDevicePixels(aFinalSize->x) + parentX;
  newBounds.y = GdkCoordToDevicePixels(aFinalSize->y) + parentY;

  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t newWidth = NSToIntRound(scale * newBounds.width);
  int32_t newHeight = NSToIntRound(scale * newBounds.height);

  LOG_POPUP(("  new mBounds [%d, %d] -> [%d x %d]", newBounds.x, newBounds.y,
             newWidth, newHeight));

  bool needsPositionUpdate =
      (newBounds.x != mBounds.x || newBounds.y != mBounds.y);
  bool needsSizeUpdate =
      (newWidth != mBounds.width || newHeight != mBounds.height);
  // Update view
  if (needsSizeUpdate) {
    LOG_POPUP(("  needSizeUpdate\n"));
    // TODO: use correct monitor here?
    int32_t p2a = AppUnitsPerCSSPixel() / gfxPlatformGtk::GetFontScaleFactor();
    mPreferredPopupRect = nsRect(NSIntPixelsToAppUnits(newBounds.x, p2a),
                                 NSIntPixelsToAppUnits(newBounds.y, p2a),
                                 NSIntPixelsToAppUnits(newBounds.width, p2a),
                                 NSIntPixelsToAppUnits(newBounds.height, p2a));
    mPreferredPopupRectFlushed = false;
    Resize(newBounds.width, newBounds.height, true);
    DispatchResized();

    nsMenuPopupFrame* popupFrame = GetMenuPopupFrame(GetFrame());
    if (popupFrame) {
      RefPtr<PresShell> presShell = popupFrame->PresShell();
      presShell->FrameNeedsReflow(popupFrame, IntrinsicDirty::Resize,
                                  NS_FRAME_IS_DIRTY);
      // Force to trigger popup crop to fit the screen
      popupFrame->SetPopupPosition(nullptr, true, false);
    }
  }

  if (needsPositionUpdate) {
    LOG_POPUP(("  needPositionUpdate, new bounds [%d, %d]", newBounds.x,
               newBounds.y));
    mBounds.x = newBounds.x;
    mBounds.y = newBounds.y;
    NotifyWindowMoved(mBounds.x, mBounds.y);
  }
}

static GdkGravity PopupAlignmentToGdkGravity(int8_t aAlignment) {
  switch (aAlignment) {
    case POPUPALIGNMENT_NONE:
      return GDK_GRAVITY_NORTH_WEST;
      break;
    case POPUPALIGNMENT_TOPLEFT:
      return GDK_GRAVITY_NORTH_WEST;
      break;
    case POPUPALIGNMENT_TOPRIGHT:
      return GDK_GRAVITY_NORTH_EAST;
      break;
    case POPUPALIGNMENT_BOTTOMLEFT:
      return GDK_GRAVITY_SOUTH_WEST;
      break;
    case POPUPALIGNMENT_BOTTOMRIGHT:
      return GDK_GRAVITY_SOUTH_EAST;
      break;
    case POPUPALIGNMENT_LEFTCENTER:
      return GDK_GRAVITY_WEST;
      break;
    case POPUPALIGNMENT_RIGHTCENTER:
      return GDK_GRAVITY_EAST;
      break;
    case POPUPALIGNMENT_TOPCENTER:
      return GDK_GRAVITY_NORTH;
      break;
    case POPUPALIGNMENT_BOTTOMCENTER:
      return GDK_GRAVITY_SOUTH;
      break;
  }
  return GDK_GRAVITY_STATIC;
}

static GdkGravity PopupGetHorizontallyFlippedAnchor(GdkGravity anchor) {
  switch (anchor) {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_NORTH_EAST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_SOUTH_WEST;
  }
  return anchor;
}

void nsWindow::NativeMoveResizeWaylandPopup(GdkPoint* aPosition,
                                            GdkRectangle* aSize) {
  LOG_POPUP(("nsWindow::NativeMoveResizeWaylandPopup [%p] %d,%d -> %d x %d\n",
             (void*)this, aPosition->x, aPosition->y, aSize->width,
             aSize->height));

  // Compositor may be confused by windows with width/height = 0
  // and positioning such windows leads to Bug 1555866.
  if (!AreBoundsSane()) {
    LOG_POPUP(("  Bounds are not sane (width: %d height: %d)\n", mBounds.width,
               mBounds.height));
    return;
  }

  // Mark popup as changed as we're updating position/size.
  mPopupChanged = true;

  // Save popup position for former re-calculations when popup hierarchy
  // is changed.
  LOG_POPUP(("  saved popup position [%d, %d]\n", aPosition->x, aPosition->y));
  mPopupPosition = *aPosition;
  if (aSize) {
    LOG_POPUP(("  set size [%d, %d]\n", aSize->width, aSize->height));
    gtk_window_resize(GTK_WINDOW(mShell), aSize->width, aSize->height);
  }

  if (!WaylandPopupNeedsTrackInHierarchy()) {
    LOG_POPUP(("  not tracked, move popup to [%d, %d]\n", aPosition->x,
               aPosition->y));
    gtk_window_move(GTK_WINDOW(mShell), aPosition->x, aPosition->y);
    return;
  }

  UpdateWaylandPopupHierarchy();
}

void nsWindow::WaylandPopupMove(bool aUseMoveToRect) {
  LOG_POPUP(("nsWindow::WaylandPopupMove [%p]\n", (void*)this));

  // Available as of GTK 3.24+
  static auto sGdkWindowMoveToRect = (void (*)(
      GdkWindow*, const GdkRectangle*, GdkGravity, GdkGravity, GdkAnchorHints,
      gint, gint))dlsym(RTLD_DEFAULT, "gdk_window_move_to_rect");

  GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(mShell));
  nsMenuPopupFrame* popupFrame = GetMenuPopupFrame(GetFrame());

  LOG_POPUP(("  original widget popup position [%d, %d]\n", mPopupPosition.x,
             mPopupPosition.y));
  LOG_POPUP(("  relative widget popup position [%d, %d]\n",
             mRelativePopupPosition.x, mRelativePopupPosition.y));
  LOG_POPUP(("  relative widget popup offset [%d, %d]\n",
             mRelativePopupOffset.x, mRelativePopupOffset.y));

  if (!sGdkWindowMoveToRect || !gdkWindow || !aUseMoveToRect || !popupFrame) {
    LOG_POPUP(("  use gtk_window_move(%d, %d)\n", mRelativePopupPosition.x,
               mRelativePopupPosition.y));
    gtk_window_move(GTK_WINDOW(mShell),
                    mRelativePopupPosition.x + mRelativePopupOffset.x,
                    mRelativePopupPosition.y + mRelativePopupOffset.y);
    if (mRelativePopupOffset.x || mRelativePopupOffset.y) {
      mBounds.x = (mRelativePopupPosition.x + mRelativePopupOffset.x) *
                  FractionalScaleFactor();
      mBounds.y = (mRelativePopupPosition.y + mRelativePopupOffset.y) *
                  FractionalScaleFactor();
      LOG_POPUP(("  setting new bounds [%d, %d]\n", mBounds.x, mBounds.y));
      NotifyWindowMoved(mBounds.x, mBounds.y);
    }
    return;
  }

  // Get anchor rectangle
  LayoutDeviceIntRect anchorRect(0, 0, 0, 0);

  int32_t p2a;
  double devPixelsPerCSSPixel = StaticPrefs::layout_css_devPixelsPerPx();
  if (devPixelsPerCSSPixel > 0.0) {
    p2a = AppUnitsPerCSSPixel() / devPixelsPerCSSPixel * GdkCeiledScaleFactor();
  } else {
    p2a = AppUnitsPerCSSPixel() / gfxPlatformGtk::GetFontScaleFactor();
  }

  nsRect anchorRectAppUnits = popupFrame->GetAnchorRect();
  anchorRect = LayoutDeviceIntRect::FromUnknownRect(
      anchorRectAppUnits.ToNearestPixels(p2a));

  // Anchor rect is in the toplevel coordinates but we need to transfer it to
  // the coordinates relative to the popup parent for the
  // gdk_window_move_to_rect
  LOG_POPUP(("  layout popup anchor [%d, %d] -> [%d, %d]\n", anchorRect.x,
             anchorRect.y, anchorRect.width, anchorRect.height));

  bool hasAnchorRect = true;
  if (mRelativePopupOffset.x || mRelativePopupOffset.y) {
    hasAnchorRect = false;
    anchorRect.SetRect(mRelativePopupPosition.x - mRelativePopupOffset.x,
                       mRelativePopupPosition.y - mRelativePopupOffset.y,
                       mRelativePopupOffset.x * 2, mRelativePopupOffset.y * 2);
    LOG_POPUP(("  Set anchor rect with offset [%d, %d] -> [%d x %d]",
               anchorRect.x, anchorRect.y, anchorRect.width,
               anchorRect.height));
  } else if (anchorRect.width == 0) {
    LOG_POPUP(("  No anchor rect given, use position for anchor [%d, %d]",
               mRelativePopupPosition.x, mRelativePopupPosition.y));
    hasAnchorRect = false;
    anchorRect.SetRect(mRelativePopupPosition.x, mRelativePopupPosition.y, 1,
                       1);
  } else {
    if (mWaylandPopupPrev->mWaylandToplevel != nullptr) {
      int parentX, parentY;
      GetParentPosition(&parentX, &parentY);
      LOG_POPUP(("  subtract parent position [%d, %d]\n", parentX, parentY));
      anchorRect.x -= parentX;
      anchorRect.y -= parentY;
    }
  }

  LOG_POPUP(("  final popup rect position [%d, %d] -> [%d x %d]\n",
             anchorRect.x, anchorRect.y, anchorRect.width, anchorRect.height));

  // Get gravity and flip type
  GdkGravity rectAnchor = GDK_GRAVITY_NORTH_WEST;
  GdkGravity menuAnchor = GDK_GRAVITY_NORTH_WEST;
  FlipType flipType = FlipType_Default;
  int8_t position = -1;
  if (mPopupContextMenu && !mPopupAnchored) {
    rectAnchor = GDK_GRAVITY_SOUTH_EAST;
    menuAnchor = GDK_GRAVITY_NORTH_WEST;
  } else {
    rectAnchor = PopupAlignmentToGdkGravity(popupFrame->GetPopupAnchor());
    menuAnchor = PopupAlignmentToGdkGravity(popupFrame->GetPopupAlignment());
    flipType = popupFrame->GetFlipType();
    position = popupFrame->GetAlignmentPosition();
  }

  if (popupFrame->IsDirectionRTL()) {
    rectAnchor = PopupGetHorizontallyFlippedAnchor(rectAnchor);
    menuAnchor = PopupGetHorizontallyFlippedAnchor(menuAnchor);
  }

  LOG_POPUP(("  parentRect gravity: %d anchor gravity: %d\n", rectAnchor,
             menuAnchor));

  // Gtk default is: GDK_ANCHOR_FLIP | GDK_ANCHOR_SLIDE | GDK_ANCHOR_RESIZE.
  // We want to SLIDE_X menu on the dual monitor setup rather than resize it
  // on the other monitor.
  GdkAnchorHints hints =
      GdkAnchorHints(GDK_ANCHOR_FLIP | GDK_ANCHOR_SLIDE_X | GDK_ANCHOR_RESIZE);

  // slideHorizontal from nsMenuPopupFrame::SetPopupPosition
  if (position >= POPUPPOSITION_BEFORESTART &&
      position <= POPUPPOSITION_AFTEREND) {
    hints = GdkAnchorHints(hints | GDK_ANCHOR_SLIDE_X);
  }
  // slideVertical from nsMenuPopupFrame::SetPopupPosition
  if (position >= POPUPPOSITION_STARTBEFORE &&
      position <= POPUPPOSITION_ENDAFTER) {
    hints = GdkAnchorHints(hints | GDK_ANCHOR_SLIDE_Y);
  }

  if (rectAnchor == GDK_GRAVITY_CENTER && menuAnchor == GDK_GRAVITY_CENTER) {
    // only slide
    hints = GdkAnchorHints(hints | GDK_ANCHOR_SLIDE);
  } else {
    switch (flipType) {
      case FlipType_Both:
        hints = GdkAnchorHints(hints | GDK_ANCHOR_FLIP);
        break;
      case FlipType_Slide:
        hints = GdkAnchorHints(hints | GDK_ANCHOR_SLIDE);
        break;
      case FlipType_Default:
        hints = GdkAnchorHints(hints | GDK_ANCHOR_FLIP);
        break;
      default:
        break;
    }
  }
  if (!WaylandPopupIsMenu()) {
    // we don't want to slide menus to fit the screen rather resize them
    hints = GdkAnchorHints(hints | GDK_ANCHOR_SLIDE);
  }
  // Inspired by nsMenuPopupFrame::AdjustPositionForAnchorAlign
  nsPoint cursorOffset(0, 0);
  // Offset is already computed to the tooltips
  if (hasAnchorRect && mPopupType != ePopupTypeTooltip) {
    nsMargin margin(0, 0, 0, 0);
    popupFrame->StyleMargin()->GetMargin(margin);
    int8_t popupAlign(popupFrame->GetPopupAlignment());
    if (popupFrame->IsDirectionRTL()) {
      popupAlign = -popupAlign;
    }
    switch (popupAlign) {
      case POPUPALIGNMENT_TOPRIGHT:
        cursorOffset.MoveBy(-margin.right, margin.top);
        break;
      case POPUPALIGNMENT_BOTTOMLEFT:
        cursorOffset.MoveBy(margin.left, -margin.bottom);
        break;
      case POPUPALIGNMENT_BOTTOMRIGHT:
        cursorOffset.MoveBy(-margin.right, -margin.bottom);
        break;
      case POPUPALIGNMENT_TOPLEFT:
      default:
        cursorOffset.MoveBy(margin.left, margin.top);
        break;
    }
  }

  if (!g_signal_handler_find(gdkWindow, G_SIGNAL_MATCH_FUNC, 0, 0, nullptr,
                             FuncToGpointer(NativeMoveResizeCallback), this)) {
    g_signal_connect(gdkWindow, "moved-to-rect",
                     G_CALLBACK(NativeMoveResizeCallback), this);
  }

  LOG_POPUP(("  popup window cursor offset x: %d y: %d\n", cursorOffset.x / p2a,
             cursorOffset.y / p2a));
  GdkRectangle rect = {anchorRect.x, anchorRect.y, anchorRect.width,
                       anchorRect.height};

  mWaitingForMoveToRectCallback = true;

  if (gtk_widget_is_visible(mShell)) {
    NS_WARNING(
        "Positioning visible popup under Wayland, position may be wrong!");
  }

  mPopupLastAnchor = anchorRect;
  sGdkWindowMoveToRect(gdkWindow, &rect, rectAnchor, menuAnchor, hints,
                       cursorOffset.x / p2a, cursorOffset.y / p2a);
}

void nsWindow::NativeMove() {
  GdkPoint point = DevicePixelsToGdkPointRoundDown(mBounds.TopLeft());

  LOG(("nsWindow::NativeMove [%p] %d %d\n", (void*)this, point.x, point.y));

  if (GdkIsX11Display() && IsPopup() &&
      !gtk_widget_get_visible(GTK_WIDGET(mShell))) {
    mHiddenPopupPositioned = true;
    mPopupPosition = point;
  }

  if (IsWaylandPopup()) {
    GdkRectangle size = DevicePixelsToGdkSizeRoundUp(mBounds.Size());
    NativeMoveResizeWaylandPopup(&point, &size);
  } else if (mIsTopLevel) {
    gtk_window_move(GTK_WINDOW(mShell), point.x, point.y);
  } else if (mGdkWindow) {
    gdk_window_move(mGdkWindow, point.x, point.y);
  }
}

void nsWindow::SetZIndex(int32_t aZIndex) {
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
    if (mGdkWindow) gdk_window_raise(mGdkWindow);
  } else {
    // All the siblings before us need to be below our widget.
    for (nsWindow* w = this; w;
         w = static_cast<nsWindow*>(w->GetPrevSibling())) {
      if (w->mGdkWindow) gdk_window_lower(w->mGdkWindow);
    }
  }
}

void nsWindow::SetSizeMode(nsSizeMode aMode) {
  LOG(("nsWindow::SetSizeMode [%p] %d\n", (void*)this, aMode));

  // Save the requested state.
  nsBaseWidget::SetSizeMode(aMode);

  // return if there's no shell or our current state is the same as
  // the mode we were just set to.
  if (!mShell || mSizeState == mSizeMode) {
    LOG(("    already set"));
    return;
  }

  switch (aMode) {
    case nsSizeMode_Maximized:
      LOG(("    set maximized"));
      gtk_window_maximize(GTK_WINDOW(mShell));
      break;
    case nsSizeMode_Minimized:
      LOG(("    set minimized"));
      gtk_window_iconify(GTK_WINDOW(mShell));
      break;
    case nsSizeMode_Fullscreen:
      LOG(("    set fullscreen"));
      MakeFullScreen(true);
      break;

    default:
      LOG(("    set normal"));
      // nsSizeMode_Normal, really.
      if (mSizeState == nsSizeMode_Minimized) {
        gtk_window_deiconify(GTK_WINDOW(mShell));
      } else if (mSizeState == nsSizeMode_Maximized) {
        gtk_window_unmaximize(GTK_WINDOW(mShell));
      }
      break;
  }

  // Request mBounds update from configure event as we may not get
  // OnSizeAllocate for size state changes (Bug 1489463).
  mBoundsAreValid = false;

  mSizeState = mSizeMode;
}

static bool GetWindowManagerName(GdkWindow* gdk_window, nsACString& wmName) {
  if (!GdkIsX11Display()) {
    return false;
  }

  Display* xdisplay = gdk_x11_get_default_xdisplay();
  GdkScreen* screen = gdk_window_get_screen(gdk_window);
  Window root_win = GDK_WINDOW_XID(gdk_screen_get_root_window(screen));

  int actual_format_return;
  Atom actual_type_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char* prop_return = nullptr;
  auto releaseXProperty = MakeScopeExit([&] {
    if (prop_return) {
      XFree(prop_return);
    }
  });

  Atom property = XInternAtom(xdisplay, "_NET_SUPPORTING_WM_CHECK", true);
  Atom req_type = XInternAtom(xdisplay, "WINDOW", true);
  if (!property || !req_type) {
    return false;
  }
  int result =
      XGetWindowProperty(xdisplay, root_win, property,
                         0L,                  // offset
                         sizeof(Window) / 4,  // length
                         false,               // delete
                         req_type, &actual_type_return, &actual_format_return,
                         &nitems_return, &bytes_after_return, &prop_return);

  if (result != Success || bytes_after_return != 0 || nitems_return != 1) {
    return false;
  }

  Window wmWindow = reinterpret_cast<Window*>(prop_return)[0];
  if (!wmWindow) {
    return false;
  }

  XFree(prop_return);
  prop_return = nullptr;

  property = XInternAtom(xdisplay, "_NET_WM_NAME", true);
  req_type = XInternAtom(xdisplay, "UTF8_STRING", true);
  if (!property || !req_type) {
    return false;
  }
  {
    // Suppress fatal errors for a missing window.
    ScopedXErrorHandler handler;
    result =
        XGetWindowProperty(xdisplay, wmWindow, property,
                           0L,         // offset
                           INT32_MAX,  // length
                           false,      // delete
                           req_type, &actual_type_return, &actual_format_return,
                           &nitems_return, &bytes_after_return, &prop_return);
  }

  if (result != Success || bytes_after_return != 0) {
    return false;
  }

  wmName = reinterpret_cast<const char*>(prop_return);
  return true;
}

#define kDesktopMutterSchema "org.gnome.mutter"
#define kDesktopDynamicWorkspacesKey "dynamic-workspaces"

static bool WorkspaceManagementDisabled(GdkWindow* gdk_window) {
  if (Preferences::GetBool("widget.disable-workspace-management", false)) {
    return true;
  }
  if (Preferences::HasUserValue("widget.workspace-management")) {
    return Preferences::GetBool("widget.workspace-management");
  }

  static const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
  if (currentDesktop && strstr(currentDesktop, "GNOME")) {
    // Gnome uses dynamic workspaces by default so disable workspace management
    // in that case.
    bool usesDynamicWorkspaces = true;
    nsCOMPtr<nsIGSettingsService> gsettings =
        do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
    if (gsettings) {
      nsCOMPtr<nsIGSettingsCollection> mutterSettings;
      gsettings->GetCollectionForSchema(nsLiteralCString(kDesktopMutterSchema),
                                        getter_AddRefs(mutterSettings));
      if (mutterSettings) {
        if (NS_SUCCEEDED(mutterSettings->GetBoolean(
                nsLiteralCString(kDesktopDynamicWorkspacesKey),
                &usesDynamicWorkspaces))) {
        }
      }
    }
    return usesDynamicWorkspaces;
  }

  // When XDG_CURRENT_DESKTOP is missing, try to get window manager name.
  if (!currentDesktop) {
    nsAutoCString wmName;
    if (GetWindowManagerName(gdk_window, wmName)) {
      if (wmName.EqualsLiteral("bspwm")) {
        return true;
      }
      if (wmName.EqualsLiteral("i3")) {
        return true;
      }
    }
  }

  return false;
}

void nsWindow::GetWorkspaceID(nsAString& workspaceID) {
  workspaceID.Truncate();

  if (!GdkIsX11Display() || !mShell) {
    return;
  }
  // Get the gdk window for this widget.
  GdkWindow* gdk_window = gtk_widget_get_window(mShell);
  if (!gdk_window) {
    return;
  }

  if (WorkspaceManagementDisabled(gdk_window)) {
    return;
  }

  GdkAtom cardinal_atom = gdk_x11_xatom_to_atom(XA_CARDINAL);
  GdkAtom type_returned;
  int format_returned;
  int length_returned;
  long* wm_desktop;

  if (!gdk_property_get(gdk_window, gdk_atom_intern("_NET_WM_DESKTOP", FALSE),
                        cardinal_atom,
                        0,          // offset
                        INT32_MAX,  // length
                        FALSE,      // delete
                        &type_returned, &format_returned, &length_returned,
                        (guchar**)&wm_desktop)) {
    return;
  }

  workspaceID.AppendInt((int32_t)wm_desktop[0]);
  g_free(wm_desktop);
}

void nsWindow::MoveToWorkspace(const nsAString& workspaceIDStr) {
  nsresult rv = NS_OK;
  int32_t workspaceID = workspaceIDStr.ToInteger(&rv);
  if (NS_FAILED(rv) || !workspaceID || !GdkIsX11Display() || !mShell) {
    return;
  }

  // Get the gdk window for this widget.
  GdkWindow* gdk_window = gtk_widget_get_window(mShell);
  if (!gdk_window) {
    return;
  }

  // This code is inspired by some found in the 'gxtuner' project.
  // https://github.com/brummer10/gxtuner/blob/792d453da0f3a599408008f0f1107823939d730d/deskpager.cpp#L50
  XEvent xevent;
  Display* xdisplay = gdk_x11_get_default_xdisplay();
  GdkScreen* screen = gdk_window_get_screen(gdk_window);
  Window root_win = GDK_WINDOW_XID(gdk_screen_get_root_window(screen));
  GdkDisplay* display = gdk_window_get_display(gdk_window);
  Atom type = gdk_x11_get_xatom_by_name_for_display(display, "_NET_WM_DESKTOP");

  xevent.type = ClientMessage;
  xevent.xclient.type = ClientMessage;
  xevent.xclient.serial = 0;
  xevent.xclient.send_event = TRUE;
  xevent.xclient.display = xdisplay;
  xevent.xclient.window = GDK_WINDOW_XID(gdk_window);
  xevent.xclient.message_type = type;
  xevent.xclient.format = 32;
  xevent.xclient.data.l[0] = workspaceID;
  xevent.xclient.data.l[1] = X11CurrentTime;
  xevent.xclient.data.l[2] = 0;
  xevent.xclient.data.l[3] = 0;
  xevent.xclient.data.l[4] = 0;

  XSendEvent(xdisplay, root_win, FALSE,
             SubstructureNotifyMask | SubstructureRedirectMask, &xevent);

  XFlush(xdisplay);
}

using SetUserTimeFunc = void (*)(GdkWindow*, guint32);

static void SetUserTimeAndStartupIDForActivatedWindow(GtkWidget* aWindow) {
  nsGTKToolkit* GTKToolkit = nsGTKToolkit::GetToolkit();
  if (!GTKToolkit) return;

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

  gtk_window_set_startup_id(GTK_WINDOW(aWindow), desktopStartupID.get());

  // If we used the startup ID, that already contains the focus timestamp;
  // we don't want to reuse the timestamp next time we raise the window
  GTKToolkit->SetFocusTimestamp(0);
  GTKToolkit->SetDesktopStartupID(""_ns);
}

/* static */
guint32 nsWindow::GetLastUserInputTime() {
  // gdk_x11_display_get_user_time/gtk_get_current_event_time tracks
  // button and key presses, DESKTOP_STARTUP_ID used to start the app,
  // drop events from external drags,
  // WM_DELETE_WINDOW delete events, but not usually mouse motion nor
  // button and key releases.  Therefore use the most recent of
  // gdk_x11_display_get_user_time and the last time that we have seen.
  GdkDisplay* gdkDisplay = gdk_display_get_default();
  guint32 timestamp = GdkIsX11Display(gdkDisplay)
                          ? gdk_x11_display_get_user_time(gdkDisplay)
                          : gtk_get_current_event_time();

  if (sLastUserInputTime != GDK_CURRENT_TIME &&
      TimestampIsNewerThan(sLastUserInputTime, timestamp)) {
    return sLastUserInputTime;
  }

  return timestamp;
}

void nsWindow::SetFocus(Raise aRaise, mozilla::dom::CallerType aCallerType) {
  // Make sure that our owning widget has focus.  If it doesn't try to
  // grab it.  Note that we don't set our focus flag in this case.

  LOG(("  SetFocus %d [%p]\n", aRaise == Raise::Yes, (void*)this));

  GtkWidget* owningWidget = GetMozContainerWidget();
  if (!owningWidget) return;

  // Raise the window if someone passed in true and the prefs are
  // set properly.
  GtkWidget* toplevelWidget = gtk_widget_get_toplevel(owningWidget);

  if (gRaiseWindows && aRaise == Raise::Yes && toplevelWidget &&
      !gtk_widget_has_focus(owningWidget) &&
      !gtk_widget_has_focus(toplevelWidget)) {
    GtkWidget* top_window = GetToplevelWidget();
    if (top_window && (gtk_widget_get_visible(top_window))) {
      gdk_window_show_unraised(gtk_widget_get_window(top_window));
      // Unset the urgency hint if possible.
      SetUrgencyHint(top_window, false);
    }
  }

  RefPtr<nsWindow> owningWindow = get_window_for_gtk_widget(owningWidget);
  if (!owningWindow) return;

  if (aRaise == Raise::Yes) {
    // means request toplevel activation.

    // This is asynchronous.
    // If and when the window manager accepts the request, then the focus
    // widget will get a focus-in-event signal.
    if (gRaiseWindows && owningWindow->mIsShown && owningWindow->mShell &&
        !gtk_window_is_active(GTK_WINDOW(owningWindow->mShell))) {
      if (GdkIsWaylandDisplay() &&
          Preferences::GetBool("widget.wayland.test-workarounds.enabled",
                               false)) {
        // Wayland does not support focus changes so we need to workaround it
        // by window hide/show sequence.
        owningWindow->NativeShow(false);
        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "nsWindow::NativeShow()",
            [self = RefPtr<nsWindow>(owningWindow)]() -> void {
              self->NativeShow(true);
            }));
        return;
      }

      uint32_t timestamp = GDK_CURRENT_TIME;

      nsGTKToolkit* GTKToolkit = nsGTKToolkit::GetToolkit();
      if (GTKToolkit) timestamp = GTKToolkit->GetFocusTimestamp();

      LOG(("  requesting toplevel activation [%p]\n", (void*)this));
      NS_ASSERTION(owningWindow->mWindowType != eWindowType_popup || mParent,
                   "Presenting an override-redirect window");
      gtk_window_present_with_time(GTK_WINDOW(owningWindow->mShell), timestamp);

      if (GTKToolkit) GTKToolkit->SetFocusTimestamp(0);
    }
    return;
  }

  // aRaise == No means that keyboard events should be dispatched from this
  // widget.

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
    LOG(("  already have focus [%p]\n", (void*)this));
    return;
  }

  // Set this window to be the focused child window
  gFocusWindow = this;

  if (mIMContext) {
    mIMContext->OnFocusWindow(this);
  }

  LOG(("  widget now has focus in SetFocus() [%p]\n", (void*)this));
}

LayoutDeviceIntRect nsWindow::GetScreenBounds() {
  LayoutDeviceIntRect rect;
  if (mIsTopLevel && mContainer) {
    // use the point including window decorations
    gint x, y;
    gdk_window_get_root_origin(gtk_widget_get_window(GTK_WIDGET(mContainer)),
                               &x, &y);
    rect.MoveTo(GdkPointToDevicePixels({x, y}));
  } else {
    rect.MoveTo(WidgetToScreenOffset());
  }
  // mBounds.Size() is the window bounds, not the window-manager frame
  // bounds (bug 581863).  gdk_window_get_frame_extents would give the
  // frame bounds, but mBounds.Size() is returned here for consistency
  // with Resize.
  rect.SizeTo(mBounds.Size());
#if MOZ_LOGGING
  gint scale = GdkCeiledScaleFactor();
  LOG(("GetScreenBounds [%p] %d,%d -> %d x %d, unscaled %d,%d -> %d x %d\n",
       this, rect.x, rect.y, rect.width, rect.height, rect.x / scale,
       rect.y / scale, rect.width / scale, rect.height / scale));
#endif
  return rect;
}

LayoutDeviceIntSize nsWindow::GetClientSize() {
  return LayoutDeviceIntSize(mBounds.width, mBounds.height);
}

LayoutDeviceIntRect nsWindow::GetClientBounds() {
  // GetBounds returns a rect whose top left represents the top left of the
  // outer bounds, but whose width/height represent the size of the inner
  // bounds (which is messed up).
  LayoutDeviceIntRect rect = GetBounds();
  rect.MoveBy(GetClientOffset());
  return rect;
}

void nsWindow::UpdateClientOffsetFromFrameExtents() {
  AUTO_PROFILER_LABEL("nsWindow::UpdateClientOffsetFromFrameExtents", OTHER);

  if (mGtkWindowDecoration == GTK_DECORATION_CLIENT && mDrawInTitlebar) {
    return;
  }

  if (!mIsTopLevel || !mShell ||
      gtk_window_get_window_type(GTK_WINDOW(mShell)) == GTK_WINDOW_POPUP) {
    mClientOffset = nsIntPoint(0, 0);
    return;
  }

  GdkAtom cardinal_atom = gdk_x11_xatom_to_atom(XA_CARDINAL);

  GdkAtom type_returned;
  int format_returned;
  int length_returned;
  long* frame_extents;

  if (!gdk_property_get(gtk_widget_get_window(mShell),
                        gdk_atom_intern("_NET_FRAME_EXTENTS", FALSE),
                        cardinal_atom,
                        0,      // offset
                        4 * 4,  // length
                        FALSE,  // delete
                        &type_returned, &format_returned, &length_returned,
                        (guchar**)&frame_extents) ||
      length_returned / sizeof(glong) != 4) {
    mClientOffset = nsIntPoint(0, 0);
  } else {
    // data returned is in the order left, right, top, bottom
    auto left = int32_t(frame_extents[0]);
    auto top = int32_t(frame_extents[2]);
    g_free(frame_extents);

    mClientOffset = nsIntPoint(left, top);
  }

  // Send a WindowMoved notification. This ensures that BrowserParent
  // picks up the new client offset and sends it to the child process
  // if appropriate.
  NotifyWindowMoved(mBounds.x, mBounds.y);

  LOG(("nsWindow::UpdateClientOffsetFromFrameExtents [%p] %d,%d\n", (void*)this,
       mClientOffset.x, mClientOffset.y));
}

LayoutDeviceIntPoint nsWindow::GetClientOffset() {
  return GdkIsX11Display()
             ? LayoutDeviceIntPoint::FromUnknownPoint(mClientOffset)
             : LayoutDeviceIntPoint(0, 0);
}

gboolean nsWindow::OnPropertyNotifyEvent(GtkWidget* aWidget,
                                         GdkEventProperty* aEvent) {
  if (aEvent->atom == gdk_atom_intern("_NET_FRAME_EXTENTS", FALSE)) {
    UpdateClientOffsetFromFrameExtents();
    return FALSE;
  }

  if (GetCurrentTimeGetter()->PropertyNotifyHandler(aWidget, aEvent)) {
    return TRUE;
  }

  return FALSE;
}

static GdkCursor* GetCursorForImage(const nsIWidget::Cursor& aCursor,
                                    int32_t aWidgetScaleFactor) {
  if (!aCursor.IsCustom()) {
    return nullptr;
  }
  nsIntSize size = nsIWidget::CustomCursorSize(aCursor);

  // NOTE: GTK only allows integer scale factors, so we ceil to the larger scale
  // factor and then tell gtk to scale it down. We ensure to scale at least to
  // the GDK scale factor, so that cursors aren't downsized in HiDPI on wayland,
  // see bug 1707533.
  int32_t gtkScale = std::max(
      aWidgetScaleFactor, int32_t(std::ceil(std::max(aCursor.mResolution.mX,
                                                     aCursor.mResolution.mY))));

  // Reject cursors greater than 128 pixels in some direction, to prevent
  // spoofing.
  // XXX ideally we should rescale. Also, we could modify the API to
  // allow trusted content to set larger cursors.
  //
  // TODO(emilio, bug 1445844): Unify the solution for this with other
  // platforms.
  if (size.width > 128 || size.height > 128) {
    return nullptr;
  }

  nsIntSize rasterSize = size * gtkScale;
  GdkPixbuf* pixbuf =
      nsImageToPixbuf::ImageToPixbuf(aCursor.mContainer, Some(rasterSize));
  if (!pixbuf) {
    return nullptr;
  }

  // Looks like all cursors need an alpha channel (tested on Gtk 2.4.4). This
  // is of course not documented anywhere...
  // So add one if there isn't one yet
  if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
    GdkPixbuf* alphaBuf = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
    g_object_unref(pixbuf);
    pixbuf = alphaBuf;
    if (!alphaBuf) {
      return nullptr;
    }
  }

  auto CleanupPixBuf =
      mozilla::MakeScopeExit([&]() { g_object_unref(pixbuf); });

  cairo_surface_t* surface =
      gdk_cairo_surface_create_from_pixbuf(pixbuf, gtkScale, nullptr);
  if (!surface) {
    return nullptr;
  }

  auto CleanupSurface =
      mozilla::MakeScopeExit([&]() { cairo_surface_destroy(surface); });

  return gdk_cursor_new_from_surface(gdk_display_get_default(), surface,
                                     aCursor.mHotspotX, aCursor.mHotspotY);
}

void nsWindow::SetCursor(const Cursor& aCursor) {
  // if we're not the toplevel window pass up the cursor request to
  // the toplevel window to handle it.
  if (!mContainer && mGdkWindow) {
    if (nsWindow* window = GetContainerWindow()) {
      window->SetCursor(aCursor);
    }
    return;
  }

  // Only change cursor if it's actually been changed
  if (!mUpdateCursor && mCursor == aCursor) {
    return;
  }

  mUpdateCursor = false;
  mCursor = aCursor;

  // Try to set the cursor image first, and fall back to the numeric cursor.
  bool fromImage = true;
  GdkCursor* newCursor = GetCursorForImage(aCursor, GdkCeiledScaleFactor());
  if (!newCursor) {
    fromImage = false;
    newCursor = get_gtk_cursor(aCursor.mDefaultCursor);
  }

  auto CleanupCursor = mozilla::MakeScopeExit([&]() {
    // get_gtk_cursor returns a weak reference, which we shouldn't unref.
    if (fromImage) {
      g_object_unref(newCursor);
    }
  });

  if (!newCursor || !mContainer) {
    return;
  }

  gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(mContainer)),
                        newCursor);
}

void nsWindow::Invalidate(const LayoutDeviceIntRect& aRect) {
  if (!mGdkWindow) return;

  GdkRectangle rect = DevicePixelsToGdkRectRoundOut(aRect);
  gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);

  LOG(("Invalidate (rect) [%p]: %d %d %d %d\n", (void*)this, rect.x, rect.y,
       rect.width, rect.height));
}

void* nsWindow::GetNativeData(uint32_t aDataType) {
  switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
      return mGdkWindow;
    }

    case NS_NATIVE_DISPLAY: {
#ifdef MOZ_X11
      GdkDisplay* gdkDisplay = gdk_display_get_default();
      if (GdkIsX11Display(gdkDisplay)) {
        return GDK_DISPLAY_XDISPLAY(gdkDisplay);
      }
#endif /* MOZ_X11 */
      // Don't bother to return native display on Wayland as it's for
      // X11 only NPAPI plugins.
      return nullptr;
    }
    case NS_NATIVE_SHELLWIDGET:
      return GetToplevelWidget();

    case NS_NATIVE_WINDOW_WEBRTC_DEVICE_ID:
      if (GdkIsX11Display()) {
        return (void*)GDK_WINDOW_XID(gdk_window_get_toplevel(mGdkWindow));
      }
      NS_WARNING(
          "nsWindow::GetNativeData(): NS_NATIVE_WINDOW_WEBRTC_DEVICE_ID is not "
          "handled on Wayland!");
      return nullptr;
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
    case NS_NATIVE_OPENGL_CONTEXT:
      return nullptr;
    case NS_NATIVE_EGL_WINDOW: {
      if (GdkIsX11Display()) {
        return mGdkWindow ? (void*)GDK_WINDOW_XID(mGdkWindow) : nullptr;
      }
#ifdef MOZ_WAYLAND
      if (mContainer) {
        return moz_container_wayland_get_egl_window(mContainer,
                                                    FractionalScaleFactor());
      }
#endif
      return nullptr;
    }
    default:
      NS_WARNING("nsWindow::GetNativeData called with bad value");
      return nullptr;
  }
}

nsresult nsWindow::SetTitle(const nsAString& aTitle) {
  if (!mShell) return NS_OK;

    // convert the string into utf8 and set the title.
#define UTF8_FOLLOWBYTE(ch) (((ch)&0xC0) == 0x80)
  NS_ConvertUTF16toUTF8 titleUTF8(aTitle);
  if (titleUTF8.Length() > NS_WINDOW_TITLE_MAX_LENGTH) {
    // Truncate overlong titles (bug 167315). Make sure we chop after a
    // complete sequence by making sure the next char isn't a follow-byte.
    uint32_t len = NS_WINDOW_TITLE_MAX_LENGTH;
    while (UTF8_FOLLOWBYTE(titleUTF8[len])) --len;
    titleUTF8.Truncate(len);
  }
  gtk_window_set_title(GTK_WINDOW(mShell), (const char*)titleUTF8.get());

  return NS_OK;
}

void nsWindow::SetIcon(const nsAString& aIconSpec) {
  if (!mShell) return;

  nsAutoCString iconName;

  if (aIconSpec.EqualsLiteral("default")) {
    nsAutoString brandName;
    WidgetUtils::GetBrandShortName(brandName);
    if (brandName.IsEmpty()) {
      brandName.AssignLiteral(u"Mozilla");
    }
    AppendUTF16toUTF8(brandName, iconName);
    ToLowerCase(iconName);
  } else {
    AppendUTF16toUTF8(aIconSpec, iconName);
  }

  nsCOMPtr<nsIFile> iconFile;
  nsAutoCString path;

  gint* iconSizes = gtk_icon_theme_get_icon_sizes(gtk_icon_theme_get_default(),
                                                  iconName.get());
  bool foundIcon = (iconSizes[0] != 0);
  g_free(iconSizes);

  if (!foundIcon) {
    // Look for icons with the following suffixes appended to the base name
    // The last two entries (for the old XPM format) will be ignored unless
    // no icons are found using other suffixes. XPM icons are deprecated.

    const char16_t extensions[9][8] = {u".png",    u"16.png", u"32.png",
                                       u"48.png",  u"64.png", u"128.png",
                                       u"256.png", u".xpm",   u"16.xpm"};

    for (uint32_t i = 0; i < ArrayLength(extensions); i++) {
      // Don't bother looking for XPM versions if we found a PNG.
      if (i == ArrayLength(extensions) - 2 && foundIcon) break;

      ResolveIconName(aIconSpec, nsDependentString(extensions[i]),
                      getter_AddRefs(iconFile));
      if (iconFile) {
        iconFile->GetNativePath(path);
        GdkPixbuf* icon = gdk_pixbuf_new_from_file(path.get(), nullptr);
        if (icon) {
          gtk_icon_theme_add_builtin_icon(iconName.get(),
                                          gdk_pixbuf_get_height(icon), icon);
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
}

LayoutDeviceIntPoint nsWindow::WidgetToScreenOffset() {
  nsIntPoint origin;
  GetWindowOrigin(mGdkWindow, &origin.x, &origin.y);

  return GdkPointToDevicePixels({origin.x, origin.y});
}

void nsWindow::CaptureMouse(bool aCapture) {
  LOG(("nsWindow::CaptureMouse() [%p]\n", (void*)this));

  if (!mGdkWindow) return;

  if (!mContainer) return;

  if (aCapture) {
    gtk_grab_add(GTK_WIDGET(mContainer));
    GrabPointer(GetLastUserInputTime());
  } else {
    ReleaseGrabs();
    gtk_grab_remove(GTK_WIDGET(mContainer));
  }
}

void nsWindow::CaptureRollupEvents(nsIRollupListener* aListener,
                                   bool aDoCapture) {
  if (!mGdkWindow) return;

  if (!mContainer) return;

  LOG(("CaptureRollupEvents() [%p] %i\n", this, int(aDoCapture)));

  if (aDoCapture) {
    gRollupListener = aListener;
    // Don't add a grab if a drag is in progress, or if the widget is a drag
    // feedback popup. (panels with type="drag").
    if (!GdkIsWaylandDisplay() && !mIsDragPopup &&
        !nsWindow::DragInProgress()) {
      gtk_grab_add(GTK_WIDGET(mContainer));
      GrabPointer(GetLastUserInputTime());
    }
  } else {
    if (!nsWindow::DragInProgress()) {
      ReleaseGrabs();
    }
    // There may not have been a drag in process when aDoCapture was set,
    // so make sure to remove any added grab.  This is a no-op if the grab
    // was not added to this widget.
    LOG(("  remove mContainer grab [%p]\n", this));
    gtk_grab_remove(GTK_WIDGET(mContainer));
    gRollupListener = nullptr;
  }
}

nsresult nsWindow::GetAttention(int32_t aCycleCount) {
  LOG(("nsWindow::GetAttention [%p]\n", (void*)this));

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

bool nsWindow::HasPendingInputEvent() {
  // This sucks, but gtk/gdk has no way to answer the question we want while
  // excluding paint events, and there's no X API that will let us peek
  // without blocking or removing.  To prevent event reordering, peek
  // anything except expose events.  Reordering expose and others should be
  // ok, hopefully.
  bool haveEvent = false;
#ifdef MOZ_X11
  XEvent ev;
  if (GdkIsX11Display()) {
    Display* display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    haveEvent = XCheckMaskEvent(
        display,
        KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
            EnterWindowMask | LeaveWindowMask | PointerMotionMask |
            PointerMotionHintMask | Button1MotionMask | Button2MotionMask |
            Button3MotionMask | Button4MotionMask | Button5MotionMask |
            ButtonMotionMask | KeymapStateMask | VisibilityChangeMask |
            StructureNotifyMask | ResizeRedirectMask | SubstructureNotifyMask |
            SubstructureRedirectMask | FocusChangeMask | PropertyChangeMask |
            ColormapChangeMask | OwnerGrabButtonMask,
        &ev);
    if (haveEvent) {
      XPutBackEvent(display, &ev);
    }
  }
#endif
  return haveEvent;
}

#if 0
#  ifdef DEBUG
// Paint flashing code (disabled for cairo - see below)

#    define CAPS_LOCK_IS_ON \
      (KeymapWrapper::AreModifiersCurrentlyActive(KeymapWrapper::CAPS_LOCK))

#    define WANT_PAINT_FLASHING (debug_WantPaintFlashing() && CAPS_LOCK_IS_ON)

#    ifdef MOZ_X11
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

  gdk_window_get_geometry(aGdkWindow,nullptr,nullptr,&width,&height);

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
#    endif /* MOZ_X11 */
#  endif   // DEBUG
#endif

#ifdef cairo_copy_clip_rectangle_list
#  error "Looks like we're including Mozilla's cairo instead of system cairo"
#endif
static bool ExtractExposeRegion(LayoutDeviceIntRegion& aRegion, cairo_t* cr) {
  cairo_rectangle_list_t* rects = cairo_copy_clip_rectangle_list(cr);
  if (rects->status != CAIRO_STATUS_SUCCESS) {
    NS_WARNING("Failed to obtain cairo rectangle list.");
    return false;
  }

  for (int i = 0; i < rects->num_rectangles; i++) {
    const cairo_rectangle_t& r = rects->rectangles[i];
    aRegion.Or(aRegion,
               LayoutDeviceIntRect::Truncate((float)r.x, (float)r.y,
                                             (float)r.width, (float)r.height));
  }

  cairo_rectangle_list_destroy(rects);
  return true;
}

#ifdef MOZ_WAYLAND
void nsWindow::CreateCompositorVsyncDispatcher() {
  if (!mWaylandVsyncSource) {
    nsBaseWidget::CreateCompositorVsyncDispatcher();
    return;
  }

  if (XRE_IsParentProcess()) {
    if (!mCompositorVsyncDispatcherLock) {
      mCompositorVsyncDispatcherLock =
          MakeUnique<Mutex>("mCompositorVsyncDispatcherLock");
    }
    MutexAutoLock lock(*mCompositorVsyncDispatcherLock);
    if (!mCompositorVsyncDispatcher) {
      mCompositorVsyncDispatcher =
          new CompositorVsyncDispatcher(mWaylandVsyncSource);
    }
  }
}
#endif

gboolean nsWindow::OnExposeEvent(cairo_t* cr) {
  // Send any pending resize events so that layout can update.
  // May run event loop.
  MaybeDispatchResized();

  if (mIsDestroyed) {
    return FALSE;
  }

  // Windows that are not visible will be painted after they become visible.
  if (!mGdkWindow || !mHasMappedToplevel) {
    return FALSE;
  }
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay() && !moz_container_wayland_can_draw(mContainer)) {
    return FALSE;
  }
#endif

  nsIWidgetListener* listener = GetListener();
  if (!listener) return FALSE;

  LOG(("received expose event [%p] %p 0x%lx (rects follow):\n", this,
       mGdkWindow, GdkIsX11Display() ? gdk_x11_window_get_xid(mGdkWindow) : 0));
  LayoutDeviceIntRegion exposeRegion;
  if (!ExtractExposeRegion(exposeRegion, cr)) {
    return FALSE;
  }

  gint scale = GdkCeiledScaleFactor();
  LayoutDeviceIntRegion region = exposeRegion;
  region.ScaleRoundOut(scale, scale);

  WindowRenderer* renderer = GetWindowRenderer();
  LayerManager* layerManager = renderer->AsLayerManager();
  KnowsCompositor* knowsCompositor = renderer->AsKnowsCompositor();

  if (knowsCompositor && layerManager && mCompositorSession) {
    // We need to paint to the screen even if nothing changed, since if we
    // don't have a compositing window manager, our pixels could be stale.
    layerManager->SetNeedsComposite(true);
    layerManager->SendInvalidRegion(region.ToUnknownRegion());
  }

  RefPtr<nsWindow> strongThis(this);

  // Dispatch WillPaintWindow notification to allow scripts etc. to run
  // before we paint
  {
    listener->WillPaintWindow(this);

    // If the window has been destroyed during the will paint notification,
    // there is nothing left to do.
    if (!mGdkWindow) return TRUE;

    // Re-get the listener since the will paint notification might have
    // killed it.
    listener = GetListener();
    if (!listener) return FALSE;
  }

  if (knowsCompositor && layerManager && layerManager->NeedsComposite()) {
    layerManager->ScheduleComposite();
    layerManager->SetNeedsComposite(false);
  }

  // Our bounds may have changed after calling WillPaintWindow.  Clip
  // to the new bounds here.  The region is relative to this
  // window.
  region.And(region, LayoutDeviceIntRect(0, 0, mBounds.width, mBounds.height));

  bool shaped = false;
  if (eTransparencyTransparent == GetTransparencyMode()) {
    auto* window = static_cast<nsWindow*>(GetTopLevelWidget());
    if (mTransparencyBitmapForTitlebar) {
      if (mSizeState == nsSizeMode_Normal) {
        window->UpdateTitlebarTransparencyBitmap();
      } else {
        window->ClearTransparencyBitmap();
      }
    } else {
      if (mHasAlphaVisual) {
        // Remove possible shape mask from when window manger was not
        // previously compositing.
        window->ClearTransparencyBitmap();
      } else {
        shaped = true;
      }
    }
  }

  if (!shaped) {
    GList* children = gdk_window_peek_children(mGdkWindow);
    while (children) {
      GdkWindow* gdkWin = GDK_WINDOW(children->data);
      nsWindow* kid = get_window_for_gdk_window(gdkWin);
      if (kid && gdk_window_is_visible(gdkWin)) {
        AutoTArray<LayoutDeviceIntRect, 1> clipRects;
        kid->GetWindowClipRegion(&clipRects);
        LayoutDeviceIntRect bounds = kid->GetBounds();
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
  if (renderer->GetBackendType() == LayersBackend::LAYERS_CLIENT ||
      renderer->GetBackendType() == LayersBackend::LAYERS_WR) {
    listener->PaintWindow(this, region);

    // Re-get the listener since the will paint notification might have
    // killed it.
    listener = GetListener();
    if (!listener) return TRUE;

    listener->DidPaintWindow();
    return TRUE;
  }

  BufferMode layerBuffering = BufferMode::BUFFERED;
  RefPtr<DrawTarget> dt = StartRemoteDrawingInRegion(region, &layerBuffering);
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
    RefPtr<DrawTarget> destDT =
        dt->CreateSimilarDrawTarget(boundsRect.Size(), SurfaceFormat::B8G8R8A8);
    if (!destDT || !destDT->IsValid()) {
      return FALSE;
    }
    destDT->SetTransform(Matrix::Translation(-boundsRect.TopLeft()));
    ctx = gfxContext::CreatePreservingTransformOrNull(destDT);
  } else {
    gfxUtils::ClipToRegion(dt, region.ToUnknownRegion());
    ctx = gfxContext::CreatePreservingTransformOrNull(dt);
  }
  MOZ_ASSERT(ctx);  // checked both dt and destDT valid draw target above

#  if 0
    // NOTE: Paint flashing region would be wrong for cairo, since
    // cairo inflates the update region, etc.  So don't paint flash
    // for cairo.
#    ifdef DEBUG
    // XXX aEvent->region may refer to a newly-invalid area.  FIXME
    if (0 && WANT_PAINT_FLASHING && gtk_widget_get_window(aEvent))
        gdk_window_flash(mGdkWindow, 1, 100, aEvent->region);
#    endif
#  endif

#endif  // MOZ_X11

  bool painted = false;
  {
    if (renderer->GetBackendType() == LayersBackend::LAYERS_NONE ||
        renderer->GetBackendType() == LayersBackend::LAYERS_BASIC) {
      if (GetTransparencyMode() == eTransparencyTransparent &&
          layerBuffering == BufferMode::BUFFER_NONE && mHasAlphaVisual) {
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
      if (!listener) return TRUE;
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

        dt->DrawSurface(surf, Rect(boundsRect),
                        Rect(0, 0, boundsRect.width, boundsRect.height),
                        DrawSurfaceOptions(SamplingFilter::POINT),
                        DrawOptions(1.0f, CompositionOp::OP_SOURCE));
      }
    }
  }

  ctx = nullptr;
  dt->PopClip();

#endif  // MOZ_X11

  EndRemoteDrawingInRegion(dt, region);

  listener->DidPaintWindow();

  // Synchronously flush any new dirty areas
  cairo_region_t* dirtyArea = gdk_window_get_update_area(mGdkWindow);

  if (dirtyArea) {
    gdk_window_invalidate_region(mGdkWindow, dirtyArea, false);
    cairo_region_destroy(dirtyArea);
    gdk_window_process_updates(mGdkWindow, false);
  }

  // check the return value!
  return TRUE;
}

void nsWindow::UpdateAlpha(SourceSurface* aSourceSurface,
                           nsIntRect aBoundsRect) {
  // We need to create our own buffer to force the stride to match the
  // expected stride.
  int32_t stride =
      GetAlignedStride<4>(aBoundsRect.width, BytesPerPixel(SurfaceFormat::A8));
  if (stride == 0) {
    return;
  }
  int32_t bufferSize = stride * aBoundsRect.height;
  auto imageBuffer = MakeUniqueFallible<uint8_t[]>(bufferSize);
  {
    RefPtr<DrawTarget> drawTarget = gfxPlatform::CreateDrawTargetForData(
        imageBuffer.get(), aBoundsRect.Size(), stride, SurfaceFormat::A8);

    if (drawTarget) {
      drawTarget->DrawSurface(aSourceSurface,
                              Rect(0, 0, aBoundsRect.width, aBoundsRect.height),
                              Rect(0, 0, aSourceSurface->GetSize().width,
                                   aSourceSurface->GetSize().height),
                              DrawSurfaceOptions(SamplingFilter::POINT),
                              DrawOptions(1.0f, CompositionOp::OP_SOURCE));
    }
  }
  UpdateTranslucentWindowAlphaInternal(aBoundsRect, imageBuffer.get(), stride);
}

gboolean nsWindow::OnConfigureEvent(GtkWidget* aWidget,
                                    GdkEventConfigure* aEvent) {
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

  LOG(("configure event [%p] %d,%d -> %d x %d scale %d\n", (void*)this,
       aEvent->x, aEvent->y, aEvent->width, aEvent->height,
       gdk_window_get_scale_factor(mGdkWindow)));

  if (mPendingConfigures > 0) {
    mPendingConfigures--;
  }

  // Don't fire configure event for scale changes, we handle that
  // OnScaleChanged event. Skip that for toplevel windows only.
  if (mWindowType == eWindowType_toplevel) {
    if (mWindowScaleFactor != gdk_window_get_scale_factor(mGdkWindow)) {
      LOG(("  scale factor changed to %d,return early",
           gdk_window_get_scale_factor(mGdkWindow)));
      return FALSE;
    }
  }

  LayoutDeviceIntRect screenBounds = GetScreenBounds();

  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog) {
    // This check avoids unwanted rollup on spurious configure events from
    // Cygwin/X (bug 672103).
    if (mBounds.x != screenBounds.x || mBounds.y != screenBounds.y) {
      CheckForRollup(0, 0, false, true);
    }
  }

  NS_ASSERTION(GTK_IS_WINDOW(aWidget),
               "Configure event on widget that is not a GtkWindow");
  if (gtk_window_get_window_type(GTK_WINDOW(aWidget)) == GTK_WINDOW_POPUP) {
    // Override-redirect window
    //
    // These windows should not be moved by the window manager, and so any
    // change in position is a result of our direction.  mBounds has
    // already been set in std::move() or Resize(), and that is more
    // up-to-date than the position in the ConfigureNotify event if the
    // event is from an earlier window move.
    //
    // Skipping the WindowMoved call saves context menus from an infinite
    // loop when nsXULPopupManager::PopupMoved moves the window to the new
    // position and nsMenuPopupFrame::SetPopupPosition adds
    // offsetForContextMenu on each iteration.

    // Our back buffer might have been invalidated while we drew the last
    // frame, and its contents might be incorrect. See bug 1280653 comment 7
    // and comment 10. Specifically we must ensure we recomposite the frame
    // as soon as possible to avoid the corrupted frame being displayed.
    GetWindowRenderer()->FlushRendering();
    return FALSE;
  }

  mBounds.MoveTo(screenBounds.TopLeft());

  // XXX mozilla will invalidate the entire window after this move
  // complete.  wtf?
  NotifyWindowMoved(mBounds.x, mBounds.y);

  // A GTK app would usually update its client area size in response to
  // a "size-allocate" signal.
  // However, we need to set mBounds in advance at Resize()
  // as JS code expects immediate window size change.
  // If Gecko requests a resize from GTK, but subsequently,
  // before a corresponding "size-allocate" signal is emitted, the window is
  // resized to its former size via other means, such as maximizing,
  // then there is no "size-allocate" signal from which to update
  // the value of mBounds. Similarly, if Gecko's resize request is refused
  // by the window manager, then there will be no "size-allocate" signal.
  // In the refused request case, the window manager is required to dispatch
  // a ConfigureNotify event. mBounds can then be updated here.
  // This seems to also be sufficient to update mBounds when Gecko resizes
  // the window from maximized size and then immediately maximizes again.
  if (!mBoundsAreValid) {
    GtkAllocation allocation = {-1, -1, 0, 0};
    gtk_window_get_size(GTK_WINDOW(mShell), &allocation.width,
                        &allocation.height);
    OnSizeAllocate(&allocation);
  }

  return FALSE;
}

void nsWindow::OnContainerUnrealize() {
  // The GdkWindows are about to be destroyed (but not deleted), so remove
  // their references back to their container widget while the GdkWindow
  // hierarchy is still available.

  if (mGdkWindow) {
    DestroyChildWindows();

    g_object_set_data(G_OBJECT(mGdkWindow), "nsWindow", nullptr);
    mGdkWindow = nullptr;
  }
}

void nsWindow::OnSizeAllocate(GtkAllocation* aAllocation) {
  LOG(("nsWindow::OnSizeAllocate [%p] %d,%d -> %d x %d\n", (void*)this,
       aAllocation->x, aAllocation->y, aAllocation->width,
       aAllocation->height));

  // Client offset are updated by _NET_FRAME_EXTENTS on X11 when system titlebar
  // is enabled. In either cases (Wayland or system titlebar is off on X11)
  // we don't get _NET_FRAME_EXTENTS X11 property notification so we derive
  // it from mContainer position.
  if (mGtkWindowDecoration == GTK_DECORATION_CLIENT) {
    if (GdkIsWaylandDisplay() || (GdkIsX11Display() && mDrawInTitlebar)) {
      UpdateClientOffsetFromCSDWindow();
    }
  }

  mBoundsAreValid = true;

  LayoutDeviceIntSize size = GdkRectToDevicePixels(*aAllocation).Size();
  if (mBounds.Size() == size) {
    LOG(("  Already the same size"));
    // We were already resized at nsWindow::OnConfigureEvent() so skip it.
    return;
  }

  // Invalidate the new part of the window now for the pending paint to
  // minimize background flashes (GDK does not do this for external resizes
  // of toplevels.)
  if (mBounds.width < size.width) {
    GdkRectangle rect = DevicePixelsToGdkRectRoundOut(LayoutDeviceIntRect(
        mBounds.width, 0, size.width - mBounds.width, size.height));
    gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);
  }
  if (mBounds.height < size.height) {
    GdkRectangle rect = DevicePixelsToGdkRectRoundOut(LayoutDeviceIntRect(
        0, mBounds.height, size.width, size.height - mBounds.height));
    gdk_window_invalidate_rect(mGdkWindow, &rect, FALSE);
  }

  mBounds.SizeTo(size);

  // Notify the GtkCompositorWidget of a ClientSizeChange
  if (mCompositorWidgetDelegate) {
    mCompositorWidgetDelegate->NotifyClientSizeChanged(GetClientSize());
  }

  // Gecko permits running nested event loops during processing of events,
  // GtkWindow callers of gtk_widget_size_allocate expect the signal
  // handlers to return sometime in the near future.
  mNeedsDispatchResized = true;
  NS_DispatchToCurrentThread(NewRunnableMethod(
      "nsWindow::MaybeDispatchResized", this, &nsWindow::MaybeDispatchResized));
}

void nsWindow::OnDeleteEvent() {
  if (mWidgetListener) mWidgetListener->RequestWindowClose(this);
}

void nsWindow::OnEnterNotifyEvent(GdkEventCrossing* aEvent) {
  // This skips NotifyVirtual and NotifyNonlinearVirtual enter notify events
  // when the pointer enters a child window.  If the destination window is a
  // Gecko window then we'll catch the corresponding event on that window,
  // but we won't notice when the pointer directly enters a foreign (plugin)
  // child window without passing over a visible portion of a Gecko window.
  if (aEvent->subwindow != nullptr) return;

  // Check before is_parent_ungrab_enter() as the button state may have
  // changed while a non-Gecko ancestor window had a pointer grab.
  DispatchMissedButtonReleases(aEvent);

  if (is_parent_ungrab_enter(aEvent)) {
    return;
  }

  WidgetMouseEvent event(true, eMouseEnterIntoWidget, this,
                         WidgetMouseEvent::eReal);

  event.mRefPoint = GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
  event.AssignEventTime(GetWidgetEventTime(aEvent->time));

  LOG(("OnEnterNotify: %p\n", (void*)this));

  DispatchInputEvent(&event);
}

// XXX Is this the right test for embedding cases?
static bool is_top_level_mouse_exit(GdkWindow* aWindow,
                                    GdkEventCrossing* aEvent) {
  auto x = gint(aEvent->x_root);
  auto y = gint(aEvent->y_root);
  GdkDisplay* display = gdk_window_get_display(aWindow);
  GdkWindow* winAtPt = gdk_display_get_window_at_pointer(display, &x, &y);
  if (!winAtPt) return true;
  GdkWindow* topLevelAtPt = gdk_window_get_toplevel(winAtPt);
  GdkWindow* topLevelWidget = gdk_window_get_toplevel(aWindow);
  return topLevelAtPt != topLevelWidget;
}

void nsWindow::OnLeaveNotifyEvent(GdkEventCrossing* aEvent) {
  // This ignores NotifyVirtual and NotifyNonlinearVirtual leave notify
  // events when the pointer leaves a child window.  If the destination
  // window is a Gecko window then we'll catch the corresponding event on
  // that window.
  //
  // XXXkt However, we will miss toplevel exits when the pointer directly
  // leaves a foreign (plugin) child window without passing over a visible
  // portion of a Gecko window.
  if (aEvent->subwindow != nullptr) return;

  WidgetMouseEvent event(true, eMouseExitFromWidget, this,
                         WidgetMouseEvent::eReal);

  event.mRefPoint = GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
  event.AssignEventTime(GetWidgetEventTime(aEvent->time));

  event.mExitFrom = Some(is_top_level_mouse_exit(mGdkWindow, aEvent)
                             ? WidgetMouseEvent::ePlatformTopLevel
                             : WidgetMouseEvent::ePlatformChild);

  LOG(("OnLeaveNotify: %p\n", (void*)this));

  DispatchInputEvent(&event);
}

bool nsWindow::CheckResizerEdge(LayoutDeviceIntPoint aPoint,
                                GdkWindowEdge& aOutEdge) {
  // We only need to handle resizers for PIP window.
  if (!mIsPIPWindow) {
    return false;
  }

  // Don't allow resizing maximized windows.
  if (mSizeState != nsSizeMode_Normal) {
    return false;
  }

#define RESIZER_SIZE 15
  int resizerSize = RESIZER_SIZE * GdkCeiledScaleFactor();
  int topDist = aPoint.y;
  int leftDist = aPoint.x;
  int rightDist = mBounds.width - aPoint.x;
  int bottomDist = mBounds.height - aPoint.y;

  if (leftDist <= resizerSize && topDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_NORTH_WEST;
  } else if (rightDist <= resizerSize && topDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_NORTH_EAST;
  } else if (leftDist <= resizerSize && bottomDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_SOUTH_WEST;
  } else if (rightDist <= resizerSize && bottomDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_SOUTH_EAST;
  } else if (topDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_NORTH;
  } else if (leftDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_WEST;
  } else if (rightDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_EAST;
  } else if (bottomDist <= resizerSize) {
    aOutEdge = GDK_WINDOW_EDGE_SOUTH;
  } else {
    return false;
  }
  return true;
}

template <typename Event>
static LayoutDeviceIntPoint GetRefPoint(nsWindow* aWindow, Event* aEvent) {
  if (aEvent->window == aWindow->GetGdkWindow()) {
    // we are the window that the event happened on so no need for expensive
    // WidgetToScreenOffset
    return aWindow->GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
  }
  // XXX we're never quite sure which GdkWindow the event came from due to our
  // custom bubbling in scroll_event_cb(), so use ScreenToWidget to translate
  // the screen root coordinates into coordinates relative to this widget.
  return aWindow->GdkEventCoordsToDevicePixels(aEvent->x_root, aEvent->y_root) -
         aWindow->WidgetToScreenOffset();
}

void nsWindow::OnMotionNotifyEvent(GdkEventMotion* aEvent) {
  if (mWindowShouldStartDragging) {
    mWindowShouldStartDragging = false;
    // find the top-level window
    GdkWindow* gdk_window = gdk_window_get_toplevel(mGdkWindow);
    MOZ_ASSERT(gdk_window, "gdk_window_get_toplevel should not return null");

    bool canDrag = true;
    if (GdkIsX11Display()) {
      // Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=789054
      // To avoid crashes disable double-click on WM without _NET_WM_MOVERESIZE.
      // See _should_perform_ewmh_drag() at gdkwindow-x11.c
      GdkScreen* screen = gdk_window_get_screen(gdk_window);
      GdkAtom atom = gdk_atom_intern("_NET_WM_MOVERESIZE", FALSE);
      if (!gdk_x11_screen_supports_net_wm_hint(screen, atom)) {
        canDrag = false;
      }
    }

    if (canDrag) {
      gdk_window_begin_move_drag(gdk_window, 1, aEvent->x_root, aEvent->y_root,
                                 aEvent->time);
      return;
    }
  }

  GdkWindowEdge edge;
  if (CheckResizerEdge(GetRefPoint(this, aEvent), edge)) {
    nsCursor cursor = eCursor_none;
    switch (edge) {
      case GDK_WINDOW_EDGE_NORTH:
        cursor = eCursor_n_resize;
        break;
      case GDK_WINDOW_EDGE_NORTH_WEST:
        cursor = eCursor_nw_resize;
        break;
      case GDK_WINDOW_EDGE_NORTH_EAST:
        cursor = eCursor_ne_resize;
        break;
      case GDK_WINDOW_EDGE_WEST:
        cursor = eCursor_w_resize;
        break;
      case GDK_WINDOW_EDGE_EAST:
        cursor = eCursor_e_resize;
        break;
      case GDK_WINDOW_EDGE_SOUTH:
        cursor = eCursor_s_resize;
        break;
      case GDK_WINDOW_EDGE_SOUTH_WEST:
        cursor = eCursor_sw_resize;
        break;
      case GDK_WINDOW_EDGE_SOUTH_EAST:
        cursor = eCursor_se_resize;
        break;
    }
    SetCursor(Cursor{cursor});
    return;
  }

  WidgetMouseEvent event(true, eMouseMove, this, WidgetMouseEvent::eReal);

  gdouble pressure = 0;
  gdk_event_get_axis((GdkEvent*)aEvent, GDK_AXIS_PRESSURE, &pressure);
  // Sometime gdk generate 0 pressure value between normal values
  // We have to ignore that and use last valid value
  if (pressure) mLastMotionPressure = pressure;
  event.mPressure = mLastMotionPressure;
  event.mRefPoint = GetRefPoint(this, aEvent);
  event.AssignEventTime(GetWidgetEventTime(aEvent->time));

  KeymapWrapper::InitInputEvent(event, aEvent->state);

  DispatchInputEvent(&event);
}

// If the automatic pointer grab on ButtonPress has deactivated before
// ButtonRelease, and the mouse button is released while the pointer is not
// over any a Gecko window, then the ButtonRelease event will not be received.
// (A similar situation exists when the pointer is grabbed with owner_events
// True as the ButtonRelease may be received on a foreign [plugin] window).
// Use this method to check for released buttons when the pointer returns to a
// Gecko window.
void nsWindow::DispatchMissedButtonReleases(GdkEventCrossing* aGdkEvent) {
  guint changed = aGdkEvent->state ^ gButtonState;
  // Only consider button releases.
  // (Ignore button presses that occurred outside Gecko.)
  guint released = changed & gButtonState;
  gButtonState = aGdkEvent->state;

  // Loop over each button, excluding mouse wheel buttons 4 and 5 for which
  // GDK ignores releases.
  for (guint buttonMask = GDK_BUTTON1_MASK; buttonMask <= GDK_BUTTON3_MASK;
       buttonMask <<= 1) {
    if (released & buttonMask) {
      int16_t buttonType;
      switch (buttonMask) {
        case GDK_BUTTON1_MASK:
          buttonType = MouseButton::ePrimary;
          break;
        case GDK_BUTTON2_MASK:
          buttonType = MouseButton::eMiddle;
          break;
        default:
          NS_ASSERTION(buttonMask == GDK_BUTTON3_MASK,
                       "Unexpected button mask");
          buttonType = MouseButton::eSecondary;
      }

      LOG(("Synthesized button %u release on %p\n", guint(buttonType + 1),
           (void*)this));

      // Dispatch a synthesized button up event to tell Gecko about the
      // change in state.  This event is marked as synthesized so that
      // it is not dispatched as a DOM event, because we don't know the
      // position, widget, modifiers, or time/order.
      WidgetMouseEvent synthEvent(true, eMouseUp, this,
                                  WidgetMouseEvent::eSynthesized);
      synthEvent.mButton = buttonType;
      DispatchInputEvent(&synthEvent);
    }
  }
}

void nsWindow::InitButtonEvent(WidgetMouseEvent& aEvent,
                               GdkEventButton* aGdkEvent) {
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
      aEvent.mClickCount = 2;
      break;
    case GDK_3BUTTON_PRESS:
      aEvent.mClickCount = 3;
      break;
      // default is one click
    default:
      aEvent.mClickCount = 1;
  }
}

static guint ButtonMaskFromGDKButton(guint button) {
  return GDK_BUTTON1_MASK << (button - 1);
}

void nsWindow::DispatchContextMenuEventFromMouseEvent(uint16_t domButton,
                                                      GdkEventButton* aEvent) {
  if (domButton == MouseButton::eSecondary && MOZ_LIKELY(!mIsDestroyed)) {
    WidgetMouseEvent contextMenuEvent(true, eContextMenu, this,
                                      WidgetMouseEvent::eReal);
    InitButtonEvent(contextMenuEvent, aEvent);
    contextMenuEvent.mPressure = mLastMotionPressure;
    DispatchInputEvent(&contextMenuEvent);
  }
}

void nsWindow::OnButtonPressEvent(GdkEventButton* aEvent) {
  LOG(("Button %u press on %p\n", aEvent->button, (void*)this));

  // If you double click in GDK, it will actually generate a second
  // GDK_BUTTON_PRESS before sending the GDK_2BUTTON_PRESS, and this is
  // different than the DOM spec.  GDK puts this in the queue
  // programatically, so it's safe to assume that if there's a
  // double click in the queue, it was generated so we can just drop
  // this click.
  GdkEvent* peekedEvent = gdk_event_peek();
  if (peekedEvent) {
    GdkEventType type = peekedEvent->any.type;
    gdk_event_free(peekedEvent);
    if (type == GDK_2BUTTON_PRESS || type == GDK_3BUTTON_PRESS) return;
  }

  nsWindow* containerWindow = GetContainerWindow();
  if (!gFocusWindow && containerWindow) {
    containerWindow->DispatchActivateEvent();
  }

  // check to see if we should rollup
  if (CheckForRollup(aEvent->x_root, aEvent->y_root, false, false)) return;

  // Check to see if the event is within our window's resize region
  GdkWindowEdge edge;
  if (CheckResizerEdge(GetRefPoint(this, aEvent), edge)) {
    gdk_window_begin_resize_drag(gtk_widget_get_window(mShell), edge,
                                 aEvent->button, aEvent->x_root, aEvent->y_root,
                                 aEvent->time);
    return;
  }

  gdouble pressure = 0;
  gdk_event_get_axis((GdkEvent*)aEvent, GDK_AXIS_PRESSURE, &pressure);
  mLastMotionPressure = pressure;

  uint16_t domButton;
  switch (aEvent->button) {
    case 1:
      domButton = MouseButton::ePrimary;
      break;
    case 2:
      domButton = MouseButton::eMiddle;
      break;
    case 3:
      domButton = MouseButton::eSecondary;
      break;
    // These are mapped to horizontal scroll
    case 6:
    case 7:
      NS_WARNING("We're not supporting legacy horizontal scroll event");
      return;
    // Map buttons 8-9 to back/forward
    case 8:
      if (!Preferences::GetBool("mousebutton.4th.enabled", true)) {
        return;
      }
      DispatchCommandEvent(nsGkAtoms::Back);
      return;
    case 9:
      if (!Preferences::GetBool("mousebutton.5th.enabled", true)) {
        return;
      }
      DispatchCommandEvent(nsGkAtoms::Forward);
      return;
    default:
      return;
  }

  gButtonState |= ButtonMaskFromGDKButton(aEvent->button);

  WidgetMouseEvent event(true, eMouseDown, this, WidgetMouseEvent::eReal);
  event.mButton = domButton;
  InitButtonEvent(event, aEvent);
  event.mPressure = mLastMotionPressure;

  nsIWidget::ContentAndAPZEventStatus eventStatus = DispatchInputEvent(&event);

  LayoutDeviceIntPoint refPoint =
      GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
  if ((mIsWaylandPanelWindow ||
       mDraggableRegion.Contains(refPoint.x, refPoint.y)) &&
      domButton == MouseButton::ePrimary &&
      eventStatus.mContentStatus != nsEventStatus_eConsumeNoDefault) {
    mWindowShouldStartDragging = true;
  }

  // right menu click on linux should also pop up a context menu
  if (!StaticPrefs::ui_context_menus_after_mouseup() &&
      eventStatus.mApzStatus != nsEventStatus_eConsumeNoDefault) {
    DispatchContextMenuEventFromMouseEvent(domButton, aEvent);
  }
}

void nsWindow::OnButtonReleaseEvent(GdkEventButton* aEvent) {
  LOG(("Button %u release on %p\n", aEvent->button, (void*)this));

  if (mWindowShouldStartDragging) {
    mWindowShouldStartDragging = false;
  }

  uint16_t domButton;
  switch (aEvent->button) {
    case 1:
      domButton = MouseButton::ePrimary;
      break;
    case 2:
      domButton = MouseButton::eMiddle;
      break;
    case 3:
      domButton = MouseButton::eSecondary;
      break;
    default:
      return;
  }

  gButtonState &= ~ButtonMaskFromGDKButton(aEvent->button);

  WidgetMouseEvent event(true, eMouseUp, this, WidgetMouseEvent::eReal);
  event.mButton = domButton;
  InitButtonEvent(event, aEvent);
  gdouble pressure = 0;
  gdk_event_get_axis((GdkEvent*)aEvent, GDK_AXIS_PRESSURE, &pressure);
  event.mPressure = pressure ? (float)pressure : (float)mLastMotionPressure;

  // The mRefPoint is manipulated in DispatchInputEvent, we're saving it
  // to use it for the doubleclick position check.
  LayoutDeviceIntPoint pos = event.mRefPoint;

  nsIWidget::ContentAndAPZEventStatus eventStatus = DispatchInputEvent(&event);

  bool defaultPrevented =
      (eventStatus.mContentStatus == nsEventStatus_eConsumeNoDefault);
  // Check if mouse position in titlebar and doubleclick happened to
  // trigger restore/maximize.
  if (!defaultPrevented && mDrawInTitlebar &&
      event.mButton == MouseButton::ePrimary && event.mClickCount == 2 &&
      mDraggableRegion.Contains(pos.x, pos.y)) {
    if (mSizeState == nsSizeMode_Maximized) {
      SetSizeMode(nsSizeMode_Normal);
    } else {
      SetSizeMode(nsSizeMode_Maximized);
    }
  }
  mLastMotionPressure = pressure;

  // right menu click on linux should also pop up a context menu
  if (StaticPrefs::ui_context_menus_after_mouseup() &&
      eventStatus.mApzStatus != nsEventStatus_eConsumeNoDefault) {
    DispatchContextMenuEventFromMouseEvent(domButton, aEvent);
  }

  // Open window manager menu on PIP window to allow user
  // to place it on top / all workspaces.
  if (mIsPIPWindow && aEvent->button == 3) {
    static auto sGdkWindowShowWindowMenu =
        (gboolean(*)(GdkWindow * window, GdkEvent*))
            dlsym(RTLD_DEFAULT, "gdk_window_show_window_menu");
    if (sGdkWindowShowWindowMenu) {
      sGdkWindowShowWindowMenu(mGdkWindow, (GdkEvent*)aEvent);
    }
  }
}

void nsWindow::OnContainerFocusInEvent(GdkEventFocus* aEvent) {
  LOG(("OnContainerFocusInEvent [%p]\n", (void*)this));

  // Unset the urgency hint, if possible
  GtkWidget* top_window = GetToplevelWidget();
  if (top_window && (gtk_widget_get_visible(top_window))) {
    SetUrgencyHint(top_window, false);
  }

  // Return if being called within SetFocus because the focus manager
  // already knows that the window is active.
  if (gBlockActivateEvent) {
    LOG(("activated notification is blocked [%p]\n", (void*)this));
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

  LOG(("Events sent from focus in event [%p]\n", (void*)this));
}

void nsWindow::OnContainerFocusOutEvent(GdkEventFocus* aEvent) {
  LOG(("OnContainerFocusOutEvent [%p]\n", (void*)this));

  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog) {
    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
    nsCOMPtr<nsIDragSession> dragSession;
    dragService->GetCurrentSession(getter_AddRefs(dragSession));

    // Rollup popups when a window is focused out unless a drag is occurring.
    // This check is because drags grab the keyboard and cause a focus out on
    // versions of GTK before 2.18.
    bool shouldRollup = !dragSession;
    if (!shouldRollup) {
      // we also roll up when a drag is from a different application
      nsCOMPtr<nsINode> sourceNode;
      dragSession->GetSourceNode(getter_AddRefs(sourceNode));
      shouldRollup = (sourceNode == nullptr);
    }

    if (shouldRollup) {
      CheckForRollup(0, 0, false, true);
    }
  }

  if (gFocusWindow) {
    RefPtr<nsWindow> kungFuDeathGrip = gFocusWindow;
    if (gFocusWindow->mIMContext) {
      gFocusWindow->mIMContext->OnBlurWindow(gFocusWindow);
    }
    gFocusWindow = nullptr;
  }

  DispatchDeactivateEvent();

  if (IsChromeWindowTitlebar()) {
    // DispatchDeactivateEvent() ultimately results in a call to
    // BrowsingContext::SetIsActiveBrowserWindow(), which resets
    // the state.  We call UpdateMozWindowActive() to keep it in
    // sync with GDK_WINDOW_STATE_FOCUSED.
    UpdateMozWindowActive();
  }

  LOG(("Done with container focus out [%p]\n", (void*)this));
}

bool nsWindow::DispatchCommandEvent(nsAtom* aCommand) {
  nsEventStatus status;
  WidgetCommandEvent appCommandEvent(true, aCommand, this);
  DispatchEvent(&appCommandEvent, status);
  return TRUE;
}

bool nsWindow::DispatchContentCommandEvent(EventMessage aMsg) {
  nsEventStatus status;
  WidgetContentCommandEvent event(true, aMsg, this);
  DispatchEvent(&event, status);
  return TRUE;
}

WidgetEventTime nsWindow::GetWidgetEventTime(guint32 aEventTime) {
  return WidgetEventTime(aEventTime, GetEventTimeStamp(aEventTime));
}

TimeStamp nsWindow::GetEventTimeStamp(guint32 aEventTime) {
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

  TimeStamp eventTimeStamp;

  if (GdkIsWaylandDisplay()) {
    // Wayland compositors use monotonic time to set timestamps.
    int64_t timestampTime = g_get_monotonic_time() / 1000;
    guint32 refTimeTruncated = guint32(timestampTime);

    timestampTime -= refTimeTruncated - aEventTime;
    int64_t tick =
        BaseTimeDurationPlatformUtils::TicksFromMilliseconds(timestampTime);
    eventTimeStamp = TimeStamp::FromSystemTime(tick);
  } else {
    CurrentX11TimeGetter* getCurrentTime = GetCurrentTimeGetter();
    MOZ_ASSERT(getCurrentTime,
               "Null current time getter despite having a window");
    eventTimeStamp =
        TimeConverter().GetTimeStampFromSystemTime(aEventTime, *getCurrentTime);
  }
  return eventTimeStamp;
}

mozilla::CurrentX11TimeGetter* nsWindow::GetCurrentTimeGetter() {
  MOZ_ASSERT(mGdkWindow, "Expected mGdkWindow to be set");
  if (MOZ_UNLIKELY(!mCurrentTimeGetter)) {
    mCurrentTimeGetter = MakeUnique<CurrentX11TimeGetter>(mGdkWindow);
  }
  return mCurrentTimeGetter.get();
}

gboolean nsWindow::OnKeyPressEvent(GdkEventKey* aEvent) {
  LOG(("OnKeyPressEvent [%p]\n", (void*)this));

  RefPtr<nsWindow> self(this);
  KeymapWrapper::HandleKeyPressEvent(self, aEvent);
  return TRUE;
}

gboolean nsWindow::OnKeyReleaseEvent(GdkEventKey* aEvent) {
  LOG(("OnKeyReleaseEvent [%p]\n", (void*)this));

  RefPtr<nsWindow> self(this);
  if (NS_WARN_IF(!KeymapWrapper::HandleKeyReleaseEvent(self, aEvent))) {
    return FALSE;
  }
  return TRUE;
}

void nsWindow::OnScrollEvent(GdkEventScroll* aEvent) {
  // check to see if we should rollup
  if (CheckForRollup(aEvent->x_root, aEvent->y_root, true, false)) {
    return;
  }

  // check for duplicate legacy scroll event, see GNOME bug 726878
  if (aEvent->direction != GDK_SCROLL_SMOOTH &&
      mLastScrollEventTime == aEvent->time) {
    LOG(("[%d] duplicate legacy scroll event %d\n", aEvent->time,
         aEvent->direction));
    return;
  }
  WidgetWheelEvent wheelEvent(true, eWheel, this);
  wheelEvent.mDeltaMode = dom::WheelEvent_Binding::DOM_DELTA_LINE;
  switch (aEvent->direction) {
    case GDK_SCROLL_SMOOTH: {
      // As of GTK 3.4, all directional scroll events are provided by
      // the GDK_SCROLL_SMOOTH direction on XInput2 and Wayland devices.
      mLastScrollEventTime = aEvent->time;

      // Special handling for touchpads to support flings
      // (also known as kinetic/inertial/momentum scrolling)
      GdkDevice* device = gdk_event_get_source_device((GdkEvent*)aEvent);
      GdkInputSource source = gdk_device_get_source(device);
      if (source == GDK_SOURCE_TOUCHSCREEN || source == GDK_SOURCE_TOUCHPAD) {
        if (StaticPrefs::apz_gtk_kinetic_scroll_enabled() &&
            gtk_check_version(3, 20, 0) == nullptr) {
          static auto sGdkEventIsScrollStopEvent =
              (gboolean(*)(const GdkEvent*))dlsym(
                  RTLD_DEFAULT, "gdk_event_is_scroll_stop_event");

          LOG(("[%d] pan smooth event dx=%f dy=%f inprogress=%d\n",
               aEvent->time, aEvent->delta_x, aEvent->delta_y, mPanInProgress));
          PanGestureInput::PanGestureType eventType =
              PanGestureInput::PANGESTURE_PAN;
          if (sGdkEventIsScrollStopEvent((GdkEvent*)aEvent)) {
            eventType = PanGestureInput::PANGESTURE_END;
            mPanInProgress = false;
          } else if (!mPanInProgress) {
            eventType = PanGestureInput::PANGESTURE_START;
            mPanInProgress = true;
          }

          LayoutDeviceIntPoint touchPoint = GetRefPoint(this, aEvent);
          PanGestureInput panEvent(
              eventType, aEvent->time, GetEventTimeStamp(aEvent->time),
              ScreenPoint(touchPoint.x, touchPoint.y),
              ScreenPoint(aEvent->delta_x, aEvent->delta_y),
              KeymapWrapper::ComputeKeyModifiers(aEvent->state));
          panEvent.mDeltaType = PanGestureInput::PANDELTA_PAGE;
          panEvent.mSimulateMomentum = true;

          DispatchPanGestureInput(panEvent);

          return;
        }

        // Older GTK doesn't support stop events, so we can't support fling
        wheelEvent.mScrollType = WidgetWheelEvent::SCROLL_ASYNCHRONOUSELY;
      }

      // TODO - use a more appropriate scrolling unit than lines.
      // Multiply event deltas by 3 to emulate legacy behaviour.
      wheelEvent.mDeltaX = aEvent->delta_x * 3;
      wheelEvent.mDeltaY = aEvent->delta_y * 3;
      wheelEvent.mWheelTicksX = aEvent->delta_x;
      wheelEvent.mWheelTicksY = aEvent->delta_y;
      wheelEvent.mIsNoLineOrPageDelta = true;

      break;
    }
    case GDK_SCROLL_UP:
      wheelEvent.mDeltaY = wheelEvent.mLineOrPageDeltaY = -3;
      wheelEvent.mWheelTicksY = -1;
      break;
    case GDK_SCROLL_DOWN:
      wheelEvent.mDeltaY = wheelEvent.mLineOrPageDeltaY = 3;
      wheelEvent.mWheelTicksY = 1;
      break;
    case GDK_SCROLL_LEFT:
      wheelEvent.mDeltaX = wheelEvent.mLineOrPageDeltaX = -1;
      wheelEvent.mWheelTicksX = -1;
      break;
    case GDK_SCROLL_RIGHT:
      wheelEvent.mDeltaX = wheelEvent.mLineOrPageDeltaX = 1;
      wheelEvent.mWheelTicksX = 1;
      break;
  }

  wheelEvent.mRefPoint = GetRefPoint(this, aEvent);

  KeymapWrapper::InitInputEvent(wheelEvent, aEvent->state);

  wheelEvent.AssignEventTime(GetWidgetEventTime(aEvent->time));

  DispatchInputEvent(&wheelEvent);
}

void nsWindow::OnWindowStateEvent(GtkWidget* aWidget,
                                  GdkEventWindowState* aEvent) {
  LOG(
      ("nsWindow::OnWindowStateEvent [%p] for %p changed 0x%x new_window_state "
       "0x%x\n",
       (void*)this, aWidget, aEvent->changed_mask, aEvent->new_window_state));

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
    bool mapped = !(aEvent->new_window_state &
                    (GDK_WINDOW_STATE_ICONIFIED | GDK_WINDOW_STATE_WITHDRAWN));
    if (mHasMappedToplevel != mapped) {
      SetHasMappedToplevel(mapped);
    }
    LOG(("\tquick return because IS_MOZ_CONTAINER(aWidget) is true\n"));
    return;
  }
  // else the widget is a shell widget.

  // The block below is a bit evil.
  //
  // When a window is resized before it is shown, gtk_window_resize() delays
  // resizes until the window is shown.  If gtk_window_state_event() sees a
  // GDK_WINDOW_STATE_MAXIMIZED change [1] before the window is shown, then
  // gtk_window_compute_configure_request_size() ignores the values from the
  // resize [2].  See bug 1449166 for an example of how this could happen.
  //
  // [1] https://gitlab.gnome.org/GNOME/gtk/blob/3.22.30/gtk/gtkwindow.c#L7967
  // [2] https://gitlab.gnome.org/GNOME/gtk/blob/3.22.30/gtk/gtkwindow.c#L9377
  //
  // In order to provide a sensible size for the window when the user exits
  // maximized state, we hide the GDK_WINDOW_STATE_MAXIMIZED change from
  // gtk_window_state_event() so as to trick GTK into using the values from
  // gtk_window_resize() in its configure request.
  //
  // We instead notify gtk_window_state_event() of the maximized state change
  // once the window is shown.
  //
  // See https://gitlab.gnome.org/GNOME/gtk/issues/1044
  //
  // This may be fixed in Gtk 3.24+ but some DE still have this issue
  // (Bug 1624199) so let's remove it for Wayland only.
  if (GdkIsX11Display()) {
    if (!mIsShown) {
      aEvent->changed_mask = static_cast<GdkWindowState>(
          aEvent->changed_mask & ~GDK_WINDOW_STATE_MAXIMIZED);
    } else if (aEvent->changed_mask & GDK_WINDOW_STATE_WITHDRAWN &&
               aEvent->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
      aEvent->changed_mask = static_cast<GdkWindowState>(
          aEvent->changed_mask | GDK_WINDOW_STATE_MAXIMIZED);
    }
  }

  // This is a workaround for https://gitlab.gnome.org/GNOME/gtk/issues/1395
  // Gtk+ controls window active appearance by window-state-event signal.
  if (IsChromeWindowTitlebar() &&
      (aEvent->changed_mask & GDK_WINDOW_STATE_FOCUSED)) {
    // Emulate what Gtk+ does at gtk_window_state_event().
    // We can't check GTK_STATE_FLAG_BACKDROP directly as it's set by Gtk+
    // *after* this window-state-event handler.
    mTitlebarBackdropState =
        !(aEvent->new_window_state & GDK_WINDOW_STATE_FOCUSED);

    // keep IsActiveBrowserWindow in sync with GDK_WINDOW_STATE_FOCUSED
    UpdateMozWindowActive();

    ForceTitlebarRedraw();
  }

  // We don't care about anything but changes in the maximized/icon/fullscreen
  // states but we need a workaround for bug in Wayland:
  // https://gitlab.gnome.org/GNOME/gtk/issues/67
  // Under wayland the gtk_window_iconify implementation does NOT synthetize
  // window_state_event where the GDK_WINDOW_STATE_ICONIFIED is set.
  // During restore we  won't get aEvent->changed_mask with
  // the GDK_WINDOW_STATE_ICONIFIED so to detect that change we use the stored
  // mSizeState and obtaining a focus.
  bool waylandWasIconified =
      (GdkIsWaylandDisplay() &&
       aEvent->changed_mask & GDK_WINDOW_STATE_FOCUSED &&
       aEvent->new_window_state & GDK_WINDOW_STATE_FOCUSED &&
       mSizeState == nsSizeMode_Minimized);
  if (!waylandWasIconified &&
      (aEvent->changed_mask &
       (GDK_WINDOW_STATE_ICONIFIED | GDK_WINDOW_STATE_MAXIMIZED |
        GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_FULLSCREEN)) == 0) {
    LOG(("\tearly return because no interesting bits changed\n"));
    return;
  }

  if (aEvent->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
    LOG(("\tIconified\n"));
    mSizeState = nsSizeMode_Minimized;
#ifdef ACCESSIBILITY
    DispatchMinimizeEventAccessible();
#endif  // ACCESSIBILITY
  } else if (aEvent->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
    LOG(("\tFullscreen\n"));
    mSizeState = nsSizeMode_Fullscreen;
  } else if (aEvent->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
    LOG(("\tMaximized\n"));
    mSizeState = nsSizeMode_Maximized;
#ifdef ACCESSIBILITY
    DispatchMaximizeEventAccessible();
#endif  // ACCESSIBILITY
  } else {
    LOG(("\tNormal\n"));
    mSizeState = nsSizeMode_Normal;
#ifdef ACCESSIBILITY
    DispatchRestoreEventAccessible();
#endif  // ACCESSIBILITY
  }

  if (aEvent->new_window_state & GDK_WINDOW_STATE_TILED) {
    LOG(("\tTiled\n"));
    mIsTiled = true;
  } else {
    LOG(("\tNot tiled\n"));
    mIsTiled = false;
  }

  if (mWidgetListener) {
    mWidgetListener->SizeModeChanged(mSizeState);
    if (aEvent->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
      mWidgetListener->FullscreenChanged(aEvent->new_window_state &
                                         GDK_WINDOW_STATE_FULLSCREEN);
    }
  }

  if (mDrawInTitlebar && mTransparencyBitmapForTitlebar) {
    if (mSizeState == nsSizeMode_Normal && !mIsTiled) {
      UpdateTitlebarTransparencyBitmap();
    } else {
      ClearTransparencyBitmap();
    }
  }
}

void nsWindow::OnDPIChanged() {
  if (mWidgetListener) {
    if (PresShell* presShell = mWidgetListener->GetPresShell()) {
      presShell->BackingScaleFactorChanged();
      // Update menu's font size etc.
      // This affects style / layout because it affects system font sizes.
      presShell->ThemeChanged(ThemeChangeKind::StyleAndLayout);
    }
    mWidgetListener->UIResolutionChanged();
  }
}

void nsWindow::OnCheckResize() { mPendingConfigures++; }

void nsWindow::OnCompositedChanged() {
  // Update CSD after the change in alpha visibility. This only affects
  // system metrics, not other theme shenanigans.
  NotifyThemeChanged(ThemeChangeKind::MediaQueriesOnly);
  mCompositedScreen = gdk_screen_is_composited(gdk_screen_get_default());
}

void nsWindow::OnScaleChanged() {
  // Gtk supply us sometimes with doubled events so stay calm in such case.
  if (gdk_window_get_scale_factor(mGdkWindow) == mWindowScaleFactor) {
    return;
  }

  // We pause compositor to avoid rendering of obsoleted remote content which
  // produces flickering.
  // Re-enable compositor again when remote content is updated or
  // timeout happens.
  PauseCompositor();

  // Force scale factor recalculation
  mWindowScaleFactorChanged = true;

  GtkAllocation allocation;
  gtk_widget_get_allocation(GTK_WIDGET(mContainer), &allocation);
  LayoutDeviceIntSize size = GdkRectToDevicePixels(allocation).Size();
  mBoundsAreValid = true;
  mBounds.SizeTo(size);

  if (mWidgetListener) {
    if (PresShell* presShell = mWidgetListener->GetPresShell()) {
      presShell->BackingScaleFactorChanged();
      // This affects style / layout because it affects system font sizes.
      // Update menu's font size etc.
      presShell->ThemeChanged(ThemeChangeKind::StyleAndLayout);
    }
  }

  DispatchResized();

  if (mCompositorWidgetDelegate) {
    mCompositorWidgetDelegate->NotifyClientSizeChanged(GetClientSize());
  }

  if (mCursor.IsCustom()) {
    mUpdateCursor = true;
    SetCursor(Cursor{mCursor});
  }
}

void nsWindow::DispatchDragEvent(EventMessage aMsg,
                                 const LayoutDeviceIntPoint& aRefPoint,
                                 guint aTime) {
  WidgetDragEvent event(true, aMsg, this);

  InitDragEvent(event);

  event.mRefPoint = aRefPoint;
  event.AssignEventTime(GetWidgetEventTime(aTime));

  DispatchInputEvent(&event);
}

void nsWindow::OnDragDataReceivedEvent(GtkWidget* aWidget,
                                       GdkDragContext* aDragContext, gint aX,
                                       gint aY,
                                       GtkSelectionData* aSelectionData,
                                       guint aInfo, guint aTime,
                                       gpointer aData) {
  LOGDRAG(("nsWindow::OnDragDataReceived(%p)\n", (void*)this));

  RefPtr<nsDragService> dragService = nsDragService::GetInstance();
  dragService->TargetDataReceived(aWidget, aDragContext, aX, aY, aSelectionData,
                                  aInfo, aTime);
}

nsWindow* nsWindow::GetTransientForWindowIfPopup() {
  if (mWindowType != eWindowType_popup) {
    return nullptr;
  }
  GtkWindow* toplevel = gtk_window_get_transient_for(GTK_WINDOW(mShell));
  if (toplevel) {
    return get_window_for_gtk_widget(GTK_WIDGET(toplevel));
  }
  return nullptr;
}

bool nsWindow::IsHandlingTouchSequence(GdkEventSequence* aSequence) {
  return mHandleTouchEvent && mTouches.Contains(aSequence);
}

gboolean nsWindow::OnTouchpadPinchEvent(GdkEventTouchpadPinch* aEvent) {
  if (StaticPrefs::apz_gtk_touchpad_pinch_enabled()) {
    // Do not respond to pinch gestures involving more than two fingers
    // unless specifically preffed on. These are sometimes hooked up to other
    // actions at the desktop environment level and having the browser also
    // pinch can be undesirable.
    if (aEvent->n_fingers > 2 &&
        !StaticPrefs::apz_gtk_touchpad_pinch_three_fingers_enabled()) {
      return FALSE;
    }
    PinchGestureInput::PinchGestureType pinchGestureType =
        PinchGestureInput::PINCHGESTURE_SCALE;
    ScreenCoord CurrentSpan;
    ScreenCoord PreviousSpan;

    switch (aEvent->phase) {
      case GDK_TOUCHPAD_GESTURE_PHASE_BEGIN:
        pinchGestureType = PinchGestureInput::PINCHGESTURE_START;
        CurrentSpan = aEvent->scale;

        // Assign PreviousSpan --> 0.999 to make mDeltaY field of the
        // WidgetWheelEvent that this PinchGestureInput event will be converted
        // to not equal Zero as our discussion because we observed that the
        // scale of the PHASE_BEGIN event is 1.
        PreviousSpan = 0.999;
        break;

      case GDK_TOUCHPAD_GESTURE_PHASE_UPDATE:
        pinchGestureType = PinchGestureInput::PINCHGESTURE_SCALE;
        if (aEvent->scale == mLastPinchEventSpan) {
          return FALSE;
        }
        CurrentSpan = aEvent->scale;
        PreviousSpan = mLastPinchEventSpan;
        break;

      case GDK_TOUCHPAD_GESTURE_PHASE_END:
        pinchGestureType = PinchGestureInput::PINCHGESTURE_END;
        CurrentSpan = aEvent->scale;
        PreviousSpan = mLastPinchEventSpan;
        break;

      default:
        return FALSE;
    }

    LayoutDeviceIntPoint touchpadPoint = GetRefPoint(this, aEvent);
    PinchGestureInput event(
        pinchGestureType, PinchGestureInput::TRACKPAD, aEvent->time,
        GetEventTimeStamp(aEvent->time), ExternalPoint(0, 0),
        ScreenPoint(touchpadPoint.x, touchpadPoint.y),
        100.0 * ((aEvent->phase == GDK_TOUCHPAD_GESTURE_PHASE_END)
                     ? ScreenCoord(1.f)
                     : CurrentSpan),
        100.0 * ((aEvent->phase == GDK_TOUCHPAD_GESTURE_PHASE_END)
                     ? ScreenCoord(1.f)
                     : PreviousSpan),
        KeymapWrapper::ComputeKeyModifiers(aEvent->state));

    if (!event.SetLineOrPageDeltaY(this)) {
      return FALSE;
    }

    mLastPinchEventSpan = aEvent->scale;
    DispatchPinchGestureInput(event);
  }
  return TRUE;
}

gboolean nsWindow::OnTouchEvent(GdkEventTouch* aEvent) {
  LOG(("OnTouchEvent: x=%f y=%f type=%d\n", aEvent->x, aEvent->y,
       aEvent->type));
  if (!mHandleTouchEvent) {
    // If a popup window was spawned (e.g. as the result of a long-press)
    // and touch events got diverted to that window within a touch sequence,
    // ensure the touch event gets sent to the original window instead. We
    // keep the checks here very conservative so that we only redirect
    // events in this specific scenario.
    nsWindow* targetWindow = GetTransientForWindowIfPopup();
    if (targetWindow &&
        targetWindow->IsHandlingTouchSequence(aEvent->sequence)) {
      return targetWindow->OnTouchEvent(aEvent);
    }

    return FALSE;
  }

  EventMessage msg;
  switch (aEvent->type) {
    case GDK_TOUCH_BEGIN:
      // check to see if we should rollup
      if (CheckForRollup(aEvent->x_root, aEvent->y_root, false, false)) {
        return FALSE;
      }
      msg = eTouchStart;
      break;
    case GDK_TOUCH_UPDATE:
      msg = eTouchMove;
      // Start dragging when motion events happens in the dragging area
      if (mWindowShouldStartDragging) {
        mWindowShouldStartDragging = false;
        GdkWindow* gdk_window = gdk_window_get_toplevel(mGdkWindow);
        MOZ_ASSERT(gdk_window,
                   "gdk_window_get_toplevel should not return null");

        LOG(("  start window dragging window\n"));
        gdk_window_begin_move_drag(gdk_window, 1, aEvent->x_root,
                                   aEvent->y_root, aEvent->time);
        return TRUE;
      }
      break;
    case GDK_TOUCH_END:
      msg = eTouchEnd;
      if (mWindowShouldStartDragging) {
        LOG(("  end of window dragging window\n"));
        mWindowShouldStartDragging = false;
      }
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

  touch =
      new dom::Touch(id, touchPoint, LayoutDeviceIntPoint(1, 1), 0.0f, 0.0f);

  WidgetTouchEvent event(true, msg, this);
  KeymapWrapper::InitInputEvent(event, aEvent->state);
  event.mTime = aEvent->time;

  if (aEvent->type == GDK_TOUCH_BEGIN || aEvent->type == GDK_TOUCH_UPDATE) {
    mTouches.InsertOrUpdate(aEvent->sequence, std::move(touch));
    // add all touch points to event object
    for (const auto& data : mTouches.Values()) {
      event.mTouches.AppendElement(new dom::Touch(*data));
    }
  } else if (aEvent->type == GDK_TOUCH_END ||
             aEvent->type == GDK_TOUCH_CANCEL) {
    *event.mTouches.AppendElement() = std::move(touch);
  }

  nsIWidget::ContentAndAPZEventStatus eventStatus = DispatchInputEvent(&event);

  // There's a chance that we are in drag area and the event is not consumed
  // by something on it.
  LayoutDeviceIntPoint refPoint =
      GdkEventCoordsToDevicePixels(aEvent->x, aEvent->y);
  if (aEvent->type == GDK_TOUCH_BEGIN &&
      mDraggableRegion.Contains(refPoint.x, refPoint.y) &&
      eventStatus.mApzStatus != nsEventStatus_eConsumeNoDefault) {
    mWindowShouldStartDragging = true;
  }
  return TRUE;
}

// Return true if toplevel window is transparent.
// It's transparent when we're running on composited screens
// and we can draw main window without system titlebar.
bool nsWindow::IsToplevelWindowTransparent() {
  static bool transparencyConfigured = false;

  if (!transparencyConfigured) {
    if (gdk_screen_is_composited(gdk_screen_get_default())) {
      // Some Gtk+ themes use non-rectangular toplevel windows. To fully
      // support such themes we need to make toplevel window transparent
      // with ARGB visual.
      // It may cause performanance issue so make it configurable
      // and enable it by default for selected window managers.
      if (Preferences::HasUserValue("mozilla.widget.use-argb-visuals")) {
        // argb visual is explicitly required so use it
        sTransparentMainWindow =
            Preferences::GetBool("mozilla.widget.use-argb-visuals");
      } else {
        // Enable transparent toplevel window if we can draw main window
        // without system titlebar as Gtk+ themes use titlebar round corners.
        sTransparentMainWindow =
            GetSystemGtkWindowDecoration() != GTK_DECORATION_NONE;
      }
    }
    transparencyConfigured = true;
  }

  return sTransparentMainWindow;
}

static GdkWindow* CreateGdkWindow(GdkWindow* parent, GtkWidget* widget) {
  GdkWindowAttr attributes;
  gint attributes_mask = GDK_WA_VISUAL;

  attributes.event_mask = kEvents;

  attributes.width = 1;
  attributes.height = 1;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.window_type = GDK_WINDOW_CHILD;

  GdkWindow* window = gdk_window_new(parent, &attributes, attributes_mask);
  gdk_window_set_user_data(window, widget);

  return window;
}

// Configure GL visual on X11. We add alpha silently
// if we use WebRender to workaround NVIDIA specific Bug 1663273.
bool nsWindow::ConfigureX11GLVisual(bool aUseAlpha) {
  if (!GdkIsX11Display()) {
    return false;
  }

  // If using WebRender on X11, we need to select a visual with a depth
  // buffer, as well as an alpha channel if transparency is requested. This
  // must be done before the widget is realized.
  bool useWebRender = gfx::gfxVars::UseWebRender();
  auto* screen = gtk_widget_get_screen(mShell);
  int visualId = 0;
  bool haveVisual;

  nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
  nsString adapterDriverVendor;
  gfxInfo->GetAdapterDriverVendor(adapterDriverVendor);
  bool isMesa = adapterDriverVendor.Find("mesa") != -1;

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1663003
  // We need to use GLX to get visual even on EGL until
  // EGL can provide compositable visual:
  // https://gitlab.freedesktop.org/mesa/mesa/-/issues/149
  if (!gfxVars::UseEGL() || isMesa) {
    auto* display = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(mShell));
    int screenNumber = GDK_SCREEN_XNUMBER(screen);
    haveVisual = GLContextGLX::FindVisual(display, screenNumber, useWebRender,
                                          aUseAlpha || useWebRender, &visualId);
  } else {
    haveVisual = GLContextEGL::FindVisual(useWebRender,
                                          aUseAlpha || useWebRender, &visualId);
  }

  GdkVisual* gdkVisual = nullptr;
  if (haveVisual) {
    // If we're using CSD, rendering will go through mContainer, but
    // it will inherit this visual as it is a child of mShell.
    gdkVisual = gdk_x11_screen_lookup_visual(screen, visualId);
  }
  if (!gdkVisual) {
    NS_WARNING("We're missing X11 Visual!");
    if (aUseAlpha || useWebRender) {
      // We try to use a fallback alpha visual
      GdkScreen* screen = gtk_widget_get_screen(mShell);
      gdkVisual = gdk_screen_get_rgba_visual(screen);
    }
  }
  if (gdkVisual) {
    // TODO: We use alpha visual even on non-compositing screens (Bug 1479135).
    gtk_widget_set_visual(mShell, gdkVisual);
    mHasAlphaVisual = aUseAlpha;
  }

  return true;
}

nsCString nsWindow::GetWindowNodeName() {
  nsCString nodeName("Unknown");
  if (this->GetFrame() && this->GetFrame()->GetContent()) {
    nodeName =
        NS_ConvertUTF16toUTF8(this->GetFrame()->GetContent()->NodeName());
  }
  return nodeName;
}

nsCString nsWindow::GetPopupTypeName() {
  switch (mPopupHint) {
    case ePopupTypeMenu:
      return nsCString("Menu");
    case ePopupTypeTooltip:
      return nsCString("Tooltip");
    case ePopupTypePanel:
      return nsCString("Panel/Utility");
    default:
      return nsCString("Unknown");
  }
}

nsresult nsWindow::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData) {
  LOG(("nsWindow::Create: creating [%p]: nodename %s\n", this,
       GetWindowNodeName().get()));

  // only set the base parent if we're going to be a dialog or a
  // toplevel
  nsIWidget* baseParent =
      aInitData && (aInitData->mWindowType == eWindowType_dialog ||
                    aInitData->mWindowType == eWindowType_toplevel ||
                    aInitData->mWindowType == eWindowType_invisible)
          ? nullptr
          : aParent;

#ifdef ACCESSIBILITY
  // Send a DBus message to check whether a11y is enabled
  a11y::PreInit();
#endif

  // Ensure that the toolkit is created.
  nsGTKToolkit::GetToolkit();

  // initialize all the common bits of this class
  BaseCreate(baseParent, aInitData);

  // Do we need to listen for resizes?
  bool listenForResizes = false;

  if (aNativeParent || (aInitData && aInitData->mListenForResizes)) {
    listenForResizes = true;
  }

  // and do our common creation
  CommonCreate(aParent, listenForResizes);

  // save our bounds
  mBounds = aRect;
  LOG(("  mBounds: x:%d y:%d w:%d h:%d\n", mBounds.x, mBounds.y, mBounds.width,
       mBounds.height));

  mPreferredPopupRectFlushed = false;

  ConstrainSize(&mBounds.width, &mBounds.height);

  GtkWidget* eventWidget = nullptr;
  bool popupNeedsAlphaVisual = (mWindowType == eWindowType_popup &&
                                (aInitData && aInitData->mSupportTranslucency));

  // Figure out our parent window - only used for eWindowType_child
  GtkWidget* parentMozContainer = nullptr;
  GtkContainer* parentGtkContainer = nullptr;
  GdkWindow* parentGdkWindow = nullptr;
  nsWindow* parentnsWindow = nullptr;

  if (aParent) {
    parentnsWindow = static_cast<nsWindow*>(aParent);
    parentGdkWindow = parentnsWindow->mGdkWindow;
  } else if (aNativeParent && GDK_IS_WINDOW(aNativeParent)) {
    parentGdkWindow = GDK_WINDOW(aNativeParent);
    parentnsWindow = get_window_for_gdk_window(parentGdkWindow);
    if (!parentnsWindow) return NS_ERROR_FAILURE;

  } else if (aNativeParent && GTK_IS_CONTAINER(aNativeParent)) {
    parentGtkContainer = GTK_CONTAINER(aNativeParent);
  }

  if (parentGdkWindow) {
    // get the widget for the window - it should be a moz container
    parentMozContainer = parentnsWindow->GetMozContainerWidget();
    if (!parentMozContainer) return NS_ERROR_FAILURE;
  }
  // ^^ only used for eWindowType_child

  if (GdkIsWaylandDisplay()) {
    if (mWindowType == eWindowType_child) {
      // eWindowType_child is not supported on Wayland. Just switch to toplevel
      // as a workaround.
      mWindowType = eWindowType_toplevel;
    } else if (mWindowType == eWindowType_popup && !aNativeParent && !aParent) {
      // mIsWaylandPanelWindow is a special toplevel window on Wayland which
      // emulates X11 popup window without parent.
      mIsWaylandPanelWindow = true;
      mWindowType = eWindowType_toplevel;
    }
  }

  mAlwaysOnTop = aInitData && aInitData->mAlwaysOnTop;
  mIsPIPWindow = aInitData && aInitData->mPIPWindow;

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
      // window manager, as indicated by GTK_WINDOW_TOPLEVEL.
      // For Wayland we have to always use GTK_WINDOW_POPUP to control
      // popup window position.
      GtkWindowType type = GTK_WINDOW_TOPLEVEL;
      if (mWindowType == eWindowType_popup) {
        MOZ_ASSERT(aInitData);
        type = GTK_WINDOW_POPUP;
        if (GdkIsX11Display() && aInitData->mNoAutoHide) {
          type = GTK_WINDOW_TOPLEVEL;
        }
      }
      mShell = gtk_window_new(type);

      // Ensure gfxPlatform is initialized, since that is what initializes
      // gfxVars, used below.
      Unused << gfxPlatform::GetPlatform();

      if (mWindowType == eWindowType_toplevel ||
          mWindowType == eWindowType_dialog) {
        mGtkWindowDecoration = GetSystemGtkWindowDecoration();
      }

      // Don't use transparency for PictureInPicture windows.
      bool toplevelNeedsAlphaVisual = false;
      if (mWindowType == eWindowType_toplevel && !mIsPIPWindow) {
        toplevelNeedsAlphaVisual = IsToplevelWindowTransparent();
      }

      bool isGLVisualSet = false;
      mIsAccelerated = ComputeShouldAccelerate();
#ifdef MOZ_X11
      if (mIsAccelerated) {
        isGLVisualSet = ConfigureX11GLVisual(popupNeedsAlphaVisual ||
                                             toplevelNeedsAlphaVisual);
      }
#endif
      if (!isGLVisualSet &&
          (popupNeedsAlphaVisual || toplevelNeedsAlphaVisual)) {
        // We're running on composited screen so we can use alpha visual
        // for both toplevel and popups.
        if (mCompositedScreen) {
          GdkVisual* visual =
              gdk_screen_get_rgba_visual(gtk_widget_get_screen(mShell));
          if (visual) {
            gtk_widget_set_visual(mShell, visual);
            mHasAlphaVisual = true;
          }
        }
      }

      // Use X shape mask to draw round corners of Firefox titlebar.
      // We don't use shape masks any more as we switched to ARGB visual
      // by default and non-compositing screens use solid-csd decorations
      // without round corners.
      // Leave the shape mask code here as it can be used to draw round
      // corners on EGL (https://gitlab.freedesktop.org/mesa/mesa/-/issues/149)
      // or when custom titlebar theme is used.
      mTransparencyBitmapForTitlebar = TitlebarUseShapeMask();

      // We have a toplevel window with transparency.
      // Calls to UpdateTitlebarTransparencyBitmap() from OnExposeEvent()
      // occur before SetTransparencyMode() receives eTransparencyTransparent
      // from layout, so set mIsTransparent here.
      if (mWindowType == eWindowType_toplevel &&
          (mHasAlphaVisual || mTransparencyBitmapForTitlebar)) {
        mIsTransparent = true;
      }

      // We only move a general managed toplevel window if someone has
      // actually placed the window somewhere.  If no placement has taken
      // place, we just let the window manager Do The Right Thing.
      NativeResize();

      if (mWindowType == eWindowType_dialog) {
        mGtkWindowRoleName = "Dialog";

        SetDefaultIcon();
        gtk_window_set_type_hint(GTK_WINDOW(mShell),
                                 GDK_WINDOW_TYPE_HINT_DIALOG);
        LOG(("nsWindow::Create(): dialog [%p]\n", this));
        if (parentnsWindow) {
          gtk_window_set_transient_for(
              GTK_WINDOW(mShell), GTK_WINDOW(parentnsWindow->GetGtkWidget()));
          LOG(("    set parent window [%p]\n", parentnsWindow));
        }

      } else if (mWindowType == eWindowType_popup) {
        MOZ_ASSERT(aInitData);
        mGtkWindowRoleName = "Popup";
        mPopupHint = aInitData->mPopupHint;

        LOG(("nsWindow::Create() Popup [%p]\n", this));

        if (aInitData->mNoAutoHide) {
          // ... but the window manager does not decorate this window,
          // nor provide a separate taskbar icon.
          if (mBorderStyle == eBorderStyle_default) {
            gtk_window_set_decorated(GTK_WINDOW(mShell), FALSE);
          } else {
            bool decorate = mBorderStyle & eBorderStyle_title;
            gtk_window_set_decorated(GTK_WINDOW(mShell), decorate);
            if (decorate) {
              gtk_window_set_deletable(GTK_WINDOW(mShell),
                                       mBorderStyle & eBorderStyle_close);
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
          if (GdkIsX11Display()) {
            gtk_widget_realize(mShell);
            gdk_window_add_filter(gtk_widget_get_window(mShell),
                                  popup_take_focus_filter, nullptr);
          }
#endif
        }

        if (aInitData->mIsDragPopup) {
          gtk_window_set_type_hint(GTK_WINDOW(mShell),
                                   GDK_WINDOW_TYPE_HINT_DND);
          mIsDragPopup = true;
        } else if (GdkIsX11Display()) {
          // Set the window hints on X11 only. Wayland popups are configured
          // at WaylandPopupNeedsTrackInHierarchy().
          GdkWindowTypeHint gtkTypeHint;
          switch (mPopupHint) {
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
          gtk_window_set_type_hint(GTK_WINDOW(mShell), gtkTypeHint);
        }
        LOG_POPUP(("nsWindow::Create() popup [%p] type %s\n", this,
                   GetPopupTypeName().get()));
        if (parentnsWindow) {
          LOG_POPUP(("    set parent window [%p] %s\n", parentnsWindow,
                     parentnsWindow->mGtkWindowRoleName.get()));
          GtkWindow* parentWidget = GTK_WINDOW(parentnsWindow->GetGtkWidget());
          gtk_window_set_transient_for(GTK_WINDOW(mShell), parentWidget);
          if (GdkIsWaylandDisplay() && gtk_window_get_modal(parentWidget)) {
            gtk_window_set_modal(GTK_WINDOW(mShell), true);
          }
        }

        // We need realized mShell at NativeMove().
        gtk_widget_realize(mShell);

        // With popup windows, we want to control their position, so don't
        // wait for the window manager to place them (which wouldn't
        // happen with override-redirect windows anyway).
        NativeMove();
      } else {  // must be eWindowType_toplevel
        mGtkWindowRoleName = "Toplevel";
        SetDefaultIcon();

        LOG(("nsWindow::Create() Toplevel [%p]\n", this));

        if (mIsPIPWindow) {
          LOG(("    Is PIP Window\n"));
          gtk_window_set_type_hint(GTK_WINDOW(mShell),
                                   GDK_WINDOW_TYPE_HINT_UTILITY);
        }

        // each toplevel window gets its own window group
        GtkWindowGroup* group = gtk_window_group_new();
        gtk_window_group_add_window(group, GTK_WINDOW(mShell));
        g_object_unref(group);
      }

      if (mAlwaysOnTop) {
        gtk_window_set_keep_above(GTK_WINDOW(mShell), TRUE);
      }

      // Create a container to hold child windows and child GtkWidgets.
      GtkWidget* container = moz_container_new();
      mContainer = MOZ_CONTAINER(container);
#ifdef MOZ_WAYLAND
      if (GdkIsWaylandDisplay() && mIsAccelerated) {
        mCompositorState = COMPOSITOR_PAUSED_INITIALLY;
        RefPtr<nsWindow> self(this);
        moz_container_wayland_add_initial_draw_callback(
            mContainer, [self]() -> void {
              MOZ_LOG(self->IsPopup() ? gWidgetPopupLog : gWidgetLog,
                      mozilla::LogLevel::Debug,
                      ("moz_container_wayland initial create "
                       "ResumeCompositorHiddenWindow()"));
              self->mCompositorState = COMPOSITOR_PAUSED_MISSING_EGL_WINDOW;
              self->ResumeCompositorHiddenWindow();
            });
      }
#endif

      // "csd" style is set when widget is realized so we need to call
      // it explicitly now.
      gtk_widget_realize(mShell);

      /* There are several cases here:
       *
       * 1) We're running on Gtk+ without client side decorations.
       *    Content is rendered to mShell window and we listen
       *    to the Gtk+ events on mShell
       * 2) We're running on Gtk+ and client side decorations
       *    are drawn by Gtk+ to mShell. Content is rendered to mContainer
       *    and we listen to the Gtk+ events on mContainer.
       * 3) We're running on Wayland. All gecko content is rendered
       *    to mContainer and we listen to the Gtk+ events on mContainer.
       */
      GtkStyleContext* style = gtk_widget_get_style_context(mShell);
      mDrawToContainer = GdkIsWaylandDisplay() ||
                         (mGtkWindowDecoration == GTK_DECORATION_CLIENT) ||
                         gtk_style_context_has_class(style, "csd");
      eventWidget = (mDrawToContainer) ? container : mShell;

      // Prevent GtkWindow from painting a background to avoid flickering.
      gtk_widget_set_app_paintable(eventWidget, TRUE);

      gtk_widget_add_events(eventWidget, kEvents);
      if (mDrawToContainer) {
        gtk_widget_add_events(mShell, GDK_PROPERTY_CHANGE_MASK);
        gtk_widget_set_app_paintable(mShell, TRUE);
      }
      if (mTransparencyBitmapForTitlebar) {
        moz_container_force_default_visual(mContainer);
      }

      // If we draw to mContainer window then configure it now because
      // gtk_container_add() realizes the child widget.
      gtk_widget_set_has_window(container, mDrawToContainer);

      gtk_container_add(GTK_CONTAINER(mShell), container);

      // alwaysontop windows are generally used for peripheral indicators,
      // so we don't focus them by default.
      if (mAlwaysOnTop) {
        gtk_window_set_focus_on_map(GTK_WINDOW(mShell), FALSE);
      }

      gtk_widget_realize(container);

      // make sure this is the focus widget in the container
      gtk_widget_show(container);

      if (!mAlwaysOnTop) {
        gtk_widget_grab_focus(container);
      }

      // the drawing window
      mGdkWindow = gtk_widget_get_window(eventWidget);

      if (GdkIsX11Display() && gfx::gfxVars::UseEGL() && mIsAccelerated) {
        mCompositorState = COMPOSITOR_PAUSED_MISSING_EGL_WINDOW;
        ResumeCompositorHiddenWindow();
      }

      if (mIsWaylandPanelWindow) {
        gtk_window_set_decorated(GTK_WINDOW(mShell), false);
      }

      if (mWindowType == eWindowType_popup) {
        MOZ_ASSERT(aInitData);
        // gdk does not automatically set the cursor for "temporary"
        // windows, which are what gtk uses for popups.

        // force SetCursor to actually set the cursor, even though our internal
        // state indicates that we already have the standard cursor.
        mUpdateCursor = true;
        SetCursor(Cursor{eCursor_standard});

        if (aInitData->mNoAutoHide) {
          gint wmd = ConvertBorderStyles(mBorderStyle);
          if (wmd != -1) {
            gdk_window_set_decorations(mGdkWindow, (GdkWMDecoration)wmd);
          }
        }

        // If the popup ignores mouse events, set an empty input shape.
        SetWindowMouseTransparent(aInitData->mMouseTransparent);
      }
    } break;

    case eWindowType_plugin:
    case eWindowType_plugin_ipc_chrome:
    case eWindowType_plugin_ipc_content:
      MOZ_ASSERT_UNREACHABLE("Unexpected eWindowType_plugin*");
      return NS_ERROR_FAILURE;

    case eWindowType_child: {
      if (parentMozContainer) {
        mGdkWindow = CreateGdkWindow(parentGdkWindow, parentMozContainer);
        mHasMappedToplevel = parentnsWindow->mHasMappedToplevel;
      } else if (parentGtkContainer) {
        // This MozContainer has its own window for drawing and receives
        // events because there is no mShell widget (corresponding to this
        // nsWindow).
        GtkWidget* container = moz_container_new();
        mContainer = MOZ_CONTAINER(container);
        eventWidget = container;
        gtk_widget_add_events(eventWidget, kEvents);
        gtk_container_add(parentGtkContainer, container);
        gtk_widget_realize(container);

        mGdkWindow = gtk_widget_get_window(container);
      } else {
        NS_WARNING(
            "Warning: tried to create a new child widget with no parent!");
        return NS_ERROR_FAILURE;
      }
    } break;
    default:
      break;
  }

  // label the drawing window with this object so we can find our way home
  g_object_set_data(G_OBJECT(mGdkWindow), "nsWindow", this);
  if (mDrawToContainer) {
    // Also label mShell toplevel window,
    // property_notify_event_cb callback also needs to find its way home
    g_object_set_data(G_OBJECT(gtk_widget_get_window(mShell)), "nsWindow",
                      this);
  }

  if (mContainer) g_object_set_data(G_OBJECT(mContainer), "nsWindow", this);

  if (mShell) g_object_set_data(G_OBJECT(mShell), "nsWindow", this);

  // attach listeners for events
  if (mShell) {
    g_signal_connect(mShell, "configure_event", G_CALLBACK(configure_event_cb),
                     nullptr);
    g_signal_connect(mShell, "delete_event", G_CALLBACK(delete_event_cb),
                     nullptr);
    g_signal_connect(mShell, "window_state_event",
                     G_CALLBACK(window_state_event_cb), nullptr);
    g_signal_connect(mShell, "check-resize", G_CALLBACK(check_resize_cb),
                     nullptr);
    g_signal_connect(mShell, "composited-changed",
                     G_CALLBACK(widget_composited_changed_cb), nullptr);
    g_signal_connect(mShell, "property-notify-event",
                     G_CALLBACK(property_notify_event_cb), nullptr);

    if (mWindowType == eWindowType_toplevel) {
      g_signal_connect_after(mShell, "size_allocate",
                             G_CALLBACK(toplevel_window_size_allocate_cb),
                             nullptr);
    }

    GdkScreen* screen = gtk_widget_get_screen(mShell);
    if (!g_signal_handler_find(screen, G_SIGNAL_MATCH_FUNC, 0, 0, nullptr,
                               FuncToGpointer(screen_composited_changed_cb),
                               nullptr)) {
      g_signal_connect(screen, "composited-changed",
                       G_CALLBACK(screen_composited_changed_cb), nullptr);
    }

    GtkSettings* default_settings = gtk_settings_get_default();
    g_signal_connect_after(default_settings, "notify::gtk-xft-dpi",
                           G_CALLBACK(settings_xft_dpi_changed_cb), this);
  }

  if (mContainer) {
    // Widget signals
    g_signal_connect(mContainer, "unrealize",
                     G_CALLBACK(container_unrealize_cb), nullptr);
    g_signal_connect_after(mContainer, "size_allocate",
                           G_CALLBACK(size_allocate_cb), nullptr);
    g_signal_connect(mContainer, "hierarchy-changed",
                     G_CALLBACK(hierarchy_changed_cb), nullptr);
    g_signal_connect(mContainer, "notify::scale-factor",
                     G_CALLBACK(scale_changed_cb), nullptr);
    // Initialize mHasMappedToplevel.
    hierarchy_changed_cb(GTK_WIDGET(mContainer), nullptr);
    // Expose, focus, key, and drag events are sent even to GTK_NO_WINDOW
    // widgets.
    g_signal_connect(G_OBJECT(mContainer), "draw", G_CALLBACK(expose_event_cb),
                     nullptr);
    g_signal_connect(mContainer, "focus_in_event",
                     G_CALLBACK(focus_in_event_cb), nullptr);
    g_signal_connect(mContainer, "focus_out_event",
                     G_CALLBACK(focus_out_event_cb), nullptr);
    g_signal_connect(mContainer, "key_press_event",
                     G_CALLBACK(key_press_event_cb), nullptr);
    g_signal_connect(mContainer, "key_release_event",
                     G_CALLBACK(key_release_event_cb), nullptr);

    gtk_drag_dest_set((GtkWidget*)mContainer, (GtkDestDefaults)0, nullptr, 0,
                      (GdkDragAction)0);

    g_signal_connect(mContainer, "drag_motion",
                     G_CALLBACK(drag_motion_event_cb), nullptr);
    g_signal_connect(mContainer, "drag_leave", G_CALLBACK(drag_leave_event_cb),
                     nullptr);
    g_signal_connect(mContainer, "drag_drop", G_CALLBACK(drag_drop_event_cb),
                     nullptr);
    g_signal_connect(mContainer, "drag_data_received",
                     G_CALLBACK(drag_data_received_event_cb), nullptr);

#ifdef MOZ_X11
    if (GdkIsX11Display()) {
      GtkWidget* widgets[] = {GTK_WIDGET(mContainer),
                              !mDrawToContainer ? mShell : nullptr};
      for (size_t i = 0; i < ArrayLength(widgets) && widgets[i]; ++i) {
        // Double buffering is controlled by the window's owning
        // widget. Disable double buffering for painting directly to the
        // X Window.
        gtk_widget_set_double_buffered(widgets[i], FALSE);
      }
    }
#endif

    // We create input contexts for all containers, except for
    // toplevel popup windows
    if (mWindowType != eWindowType_popup) {
      mIMContext = new IMContextWrapper(this);
    }
  } else if (!mIMContext) {
    nsWindow* container = GetContainerWindow();
    if (container) {
      mIMContext = container->mIMContext;
    }
  }

  if (eventWidget) {
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
    g_signal_connect(eventWidget, "scroll-event", G_CALLBACK(scroll_event_cb),
                     nullptr);
    if (gtk_check_version(3, 18, 0) == nullptr) {
      g_signal_connect(eventWidget, "event", G_CALLBACK(generic_event_cb),
                       nullptr);
    }
    g_signal_connect(eventWidget, "touch-event", G_CALLBACK(touch_event_cb),
                     nullptr);
  }

  LOG(("nsWindow [%p] type %d %s\n", (void*)this, mWindowType,
       mIsPIPWindow ? "PIP window" : ""));
  if (mShell) {
    LOG(("\tmShell %p mContainer %p mGdkWindow %p XID 0x%lx\n", mShell,
         mContainer, mGdkWindow,
         GdkIsX11Display() ? gdk_x11_window_get_xid(mGdkWindow) : 0));
  } else if (mContainer) {
    LOG(("\tmContainer %p mGdkWindow %p\n", mContainer, mGdkWindow));
  } else if (mGdkWindow) {
    LOG(("\tmGdkWindow %p parent %p\n", mGdkWindow,
         gdk_window_get_parent(mGdkWindow)));
  }

  // resize so that everything is set to the right dimensions
  if (!mIsTopLevel) {
    ResizeInt(mBounds.x, mBounds.y, mBounds.width, mBounds.height,
              /* aMove */ false, /* aRepaint */ false);
  }

#ifdef MOZ_X11
  if (GdkIsX11Display() && mGdkWindow) {
    mXWindow = gdk_x11_window_get_xid(mGdkWindow);

    GdkVisual* gdkVisual = gdk_window_get_visual(mGdkWindow);
    mXVisual = gdk_x11_visual_get_xvisual(gdkVisual);
    mXDepth = gdk_visual_get_depth(gdkVisual);
    bool shaped = popupNeedsAlphaVisual && !mHasAlphaVisual;

    mSurfaceProvider.Initialize(mXWindow, mXVisual, mXDepth, shaped);

    if (mIsTopLevel) {
      // Set window manager hint to keep fullscreen windows composited.
      //
      // If the window were to get unredirected, there could be visible
      // tearing because Gecko does not align its framebuffer updates with
      // vblank.
      SetCompositorHint(GTK_WIDGET_COMPOSIDED_ENABLED);
    }
    // Dummy call to a function in mozgtk to prevent the linker from removing
    // the dependency with --as-needed.
    XShmQueryExtension(DefaultXDisplay());
  }
#endif
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    mSurfaceProvider.Initialize(this);
    WaylandStartVsync();
  }
#endif

  // Set default application name when it's empty.
  if (mGtkWindowAppName.IsEmpty()) {
    mGtkWindowAppName = gAppData->name;
  }
  RefreshWindowClass();

  return NS_OK;
}

void nsWindow::RefreshWindowClass(void) {
  GdkWindow* gdkWindow = gtk_widget_get_window(mShell);
  if (!gdkWindow) {
    return;
  }

  if (!mGtkWindowRoleName.IsEmpty()) {
    gdk_window_set_role(gdkWindow, mGtkWindowRoleName.get());
  }

#ifdef MOZ_X11
  if (!mGtkWindowAppName.IsEmpty() && GdkIsX11Display()) {
    XClassHint* class_hint = XAllocClassHint();
    if (!class_hint) {
      return;
    }
    const char* res_class = gdk_get_program_class();
    if (!res_class) return;

    class_hint->res_name = const_cast<char*>(mGtkWindowAppName.get());
    class_hint->res_class = const_cast<char*>(res_class);

    // Can't use gtk_window_set_wmclass() for this; it prints
    // a warning & refuses to make the change.
    GdkDisplay* display = gdk_display_get_default();
    XSetClassHint(GDK_DISPLAY_XDISPLAY(display),
                  gdk_x11_window_get_xid(gdkWindow), class_hint);
    XFree(class_hint);
  }
#endif /* MOZ_X11 */
}

void nsWindow::SetWindowClass(const nsAString& xulWinType) {
  if (!mShell) return;

  char* res_name = ToNewCString(xulWinType, mozilla::fallible);
  if (!res_name) return;

  const char* role = nullptr;

  // Parse res_name into a name and role. Characters other than
  // [A-Za-z0-9_-] are converted to '_'. Anything after the first
  // colon is assigned to role; if there's no colon, assign the
  // whole thing to both role and res_name.
  for (char* c = res_name; *c; c++) {
    if (':' == *c) {
      *c = 0;
      role = c + 1;
    } else if (!isascii(*c) || (!isalnum(*c) && ('_' != *c) && ('-' != *c))) {
      *c = '_';
    }
  }
  res_name[0] = (char)toupper(res_name[0]);
  if (!role) role = res_name;

  mGtkWindowAppName = res_name;
  mGtkWindowRoleName = role;
  free(res_name);

  RefreshWindowClass();
}

void nsWindow::NativeResize() {
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

  LOG(("nsWindow::NativeResize [%p] %d %d\n", (void*)this, size.width,
       size.height));

  if (mIsTopLevel) {
    MOZ_ASSERT(size.width > 0 && size.height > 0,
               "Can't resize window smaller than 1x1.");
    gtk_window_resize(GTK_WINDOW(mShell), size.width, size.height);
    if (mWaitingForMoveToRectCallback) {
      LOG_POPUP(("  waiting for move to rect, schedulling "));
      mNewSizeAfterMoveToRect = mBounds;
    }
  } else if (mContainer) {
    GtkWidget* widget = GTK_WIDGET(mContainer);
    GtkAllocation allocation, prev_allocation;
    gtk_widget_get_allocation(widget, &prev_allocation);
    allocation.x = prev_allocation.x;
    allocation.y = prev_allocation.y;
    allocation.width = size.width;
    allocation.height = size.height;
    gtk_widget_size_allocate(widget, &allocation);
  } else if (mGdkWindow) {
    gdk_window_resize(mGdkWindow, size.width, size.height);
  }

  // Notify the GtkCompositorWidget of a ClientSizeChange
  // This is different than OnSizeAllocate to catch initial sizing
  if (mCompositorWidgetDelegate) {
    mCompositorWidgetDelegate->NotifyClientSizeChanged(GetClientSize());
  }

  // Does it need to be shown because bounds were previously insane?
  if (mNeedsShow && mIsShown) {
    NativeShow(true);
  }
}

void nsWindow::NativeMoveResize() {
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

    return;
  }

  GdkRectangle size = DevicePixelsToGdkSizeRoundUp(mBounds.Size());
  GdkPoint topLeft = DevicePixelsToGdkPointRoundDown(mBounds.TopLeft());

  LOG(("nsWindow::NativeMoveResize [%p] %d,%d -> %d x %d\n", (void*)this,
       topLeft.x, topLeft.y, size.width, size.height));

  if (GdkIsX11Display() && IsPopup() &&
      !gtk_widget_get_visible(GTK_WIDGET(mShell))) {
    mHiddenPopupPositioned = true;
    mPopupPosition = topLeft;
  }

  if (IsWaylandPopup()) {
    NativeMoveResizeWaylandPopup(&topLeft, &size);
  } else {
    if (mIsTopLevel) {
      // x and y give the position of the window manager frame top-left.
      gtk_window_move(GTK_WINDOW(mShell), topLeft.x, topLeft.y);
      // This sets the client window size.
      MOZ_ASSERT(size.width > 0 && size.height > 0,
                 "Can't resize window smaller than 1x1.");
      gtk_window_resize(GTK_WINDOW(mShell), size.width, size.height);
    } else if (mContainer) {
      GtkAllocation allocation;
      allocation.x = topLeft.x;
      allocation.y = topLeft.y;
      allocation.width = size.width;
      allocation.height = size.height;
      gtk_widget_size_allocate(GTK_WIDGET(mContainer), &allocation);
    } else if (mGdkWindow) {
      gdk_window_move_resize(mGdkWindow, topLeft.x, topLeft.y, size.width,
                             size.height);
    }
  }

  // Notify the GtkCompositorWidget of a ClientSizeChange
  // This is different than OnSizeAllocate to catch initial sizing
  if (mCompositorWidgetDelegate) {
    mCompositorWidgetDelegate->NotifyClientSizeChanged(GetClientSize());
  }

  // Does it need to be shown because bounds were previously insane?
  if (mNeedsShow && mIsShown) {
    NativeShow(true);
  }
}

void nsWindow::ResumeCompositorHiddenWindow() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mIsDestroyed || mCompositorState == COMPOSITOR_ENABLED ||
      mCompositorState == COMPOSITOR_PAUSED_INITIALLY) {
    return;
  }

  if (CompositorBridgeChild* remoteRenderer = GetRemoteRenderer()) {
    LOG(("nsWindow::ResumeCompositorHiddenWindow [%p]\n", (void*)this));
    MOZ_ASSERT(mCompositorWidgetDelegate);
    if (mCompositorWidgetDelegate) {
      mCompositorState = COMPOSITOR_ENABLED;
      remoteRenderer->SendResumeAsync();
    }
    remoteRenderer->SendForcePresent();
  }
}

// Because wl_egl_window is destroyed on moz_container_unmap(),
// the current compositor cannot use it anymore. To avoid crash,
// pause the compositor and destroy EGLSurface & resume the compositor
// and re-create EGLSurface on next expose event.
void nsWindow::PauseCompositorHiddenWindow() {
  // TODO: The compositor backend currently relies on the pause event to work
  // around a Gnome specific bug. Remove again once the fix is widely available.
  // See bug 1721298
  if ((!mIsAccelerated && !gfx::gfxVars::UseWebRenderCompositor()) ||
      mIsDestroyed || mCompositorState == COMPOSITOR_PAUSED_INITIALLY) {
    return;
  }

  LOG(("nsWindow::PauseCompositorHiddenWindow [%p]\n", (void*)this));

  mCompositorState = COMPOSITOR_PAUSED_MISSING_EGL_WINDOW;

  // Without remote widget / renderer we can't pause compositor.
  // So delete LayerManager to avoid EGLSurface access.
  CompositorBridgeChild* remoteRenderer = GetRemoteRenderer();
  if (!remoteRenderer || !mCompositorWidgetDelegate) {
    LOG(("  deleted layer manager"));
    DestroyLayerManager();
    return;
  }

  // XXX slow sync IPC
  LOG(("  paused compositor"));
  remoteRenderer->SendPause();
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    // Re-request initial draw callback
    RefPtr<nsWindow> self(this);
    moz_container_wayland_add_initial_draw_callback(
        mContainer, [self]() -> void {
          MOZ_LOG(self->IsPopup() ? gWidgetPopupLog : gWidgetLog,
                  mozilla::LogLevel::Debug,
                  ("moz_container_wayland resume callback "
                   "ResumeCompositorHiddenWindow()"));
          self->ResumeCompositorHiddenWindow();
        });
  }
#endif
}

static int WindowResumeCompositor(void* data) {
  nsWindow* window = static_cast<nsWindow*>(data);
  window->ResumeCompositor();
  return true;
}

// We pause compositor to avoid rendering of obsoleted remote content which
// produces flickering.
// Re-enable compositor again when remote content is updated or
// timeout happens.

// Define maximal compositor pause when it's paused to avoid flickering,
// in milliseconds.
#define COMPOSITOR_PAUSE_TIMEOUT (1000)

void nsWindow::PauseCompositor() {
  bool pauseCompositor = (mWindowType == eWindowType_toplevel) &&
                         mCompositorState == COMPOSITOR_ENABLED &&
                         mIsAccelerated && mCompositorWidgetDelegate &&
                         !mIsDestroyed;
  if (!pauseCompositor) {
    return;
  }

  LOG(("nsWindow::PauseCompositor() [%p]\n", (void*)this));

  if (mCompositorPauseTimeoutID) {
    g_source_remove(mCompositorPauseTimeoutID);
    mCompositorPauseTimeoutID = 0;
  }

  CompositorBridgeChild* remoteRenderer = GetRemoteRenderer();
  if (remoteRenderer) {
    remoteRenderer->SendPause();
    mCompositorState = COMPOSITOR_PAUSED_FLICKERING;
    mCompositorPauseTimeoutID = (int)g_timeout_add(
        COMPOSITOR_PAUSE_TIMEOUT, &WindowResumeCompositor, this);
  }
}

bool nsWindow::IsWaitingForCompositorResume() {
  return !mIsDestroyed && mCompositorState == COMPOSITOR_PAUSED_FLICKERING;
}

void nsWindow::ResumeCompositor() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!IsWaitingForCompositorResume()) {
    return;
  }

  LOG(("nsWindow::ResumeCompositor() [%p]\n", (void*)this));

  if (mCompositorPauseTimeoutID) {
    g_source_remove(mCompositorPauseTimeoutID);
    mCompositorPauseTimeoutID = 0;
  }

  // We're expected to have mCompositorWidgetDelegate present
  // as we don't delete LayerManager (in PauseCompositor())
  // to avoid flickering.
  MOZ_RELEASE_ASSERT(mCompositorWidgetDelegate);

  CompositorBridgeChild* remoteRenderer = GetRemoteRenderer();
  if (remoteRenderer) {
    mCompositorState = COMPOSITOR_ENABLED;
    remoteRenderer->SendResumeAsync();
    remoteRenderer->SendForcePresent();
  }
}

void nsWindow::ResumeCompositorFromCompositorThread() {
  nsCOMPtr<nsIRunnable> event = NewRunnableMethod(
      "nsWindow::ResumeCompositor", this, &nsWindow::ResumeCompositor);
  NS_DispatchToMainThread(event.forget());
}

void nsWindow::WaylandStartVsync() {
#ifdef MOZ_WAYLAND
  // only use for toplevel windows for now - see bug 1619246
  if (!GdkIsWaylandDisplay() ||
      !StaticPrefs::widget_wayland_vsync_enabled_AtStartup() ||
      mWindowType != eWindowType_toplevel) {
    return;
  }

  if (!mWaylandVsyncSource) {
    mWaylandVsyncSource = new WaylandVsyncSource();
  }

  WaylandVsyncSource::WaylandDisplay& display =
      static_cast<WaylandVsyncSource::WaylandDisplay&>(
          mWaylandVsyncSource->GetGlobalDisplay());

  if (mCompositorWidgetDelegate) {
    if (RefPtr<layers::NativeLayerRoot> nativeLayerRoot =
            mCompositorWidgetDelegate->AsGtkCompositorWidget()
                ->GetNativeLayerRoot()) {
      display.MaybeUpdateSource(nativeLayerRoot->AsNativeLayerRootWayland());
    } else {
      display.MaybeUpdateSource(mContainer);
    }
  }
  display.EnableMonitor();
#endif
}

void nsWindow::WaylandStopVsync() {
#ifdef MOZ_WAYLAND
  if (mWaylandVsyncSource) {
    // The widget is going to be hidden, so clear the surface of our
    // vsync source.
    WaylandVsyncSource::WaylandDisplay& display =
        static_cast<WaylandVsyncSource::WaylandDisplay&>(
            mWaylandVsyncSource->GetGlobalDisplay());
    display.DisableMonitor();
    display.MaybeUpdateSource(nullptr);
  }
#endif
}

void nsWindow::NativeShow(bool aAction) {
  if (aAction) {
    // unset our flag now that our window has been shown
    mNeedsShow = false;
    LOG(("nsWindow::NativeShow show [%p]\n", this));

    if (mIsTopLevel) {
      if (IsWaylandPopup()) {
        mPopupClosed = false;
        if (WaylandPopupNeedsTrackInHierarchy()) {
          AddWindowToPopupHierarchy();
          UpdateWaylandPopupHierarchy();
          if (mPopupClosed) {
            return;
          }
        }
      }
      // Set up usertime/startupID metadata for the created window.
      if (mWindowType != eWindowType_invisible) {
        SetUserTimeAndStartupIDForActivatedWindow(mShell);
      }
      LOG(("  calling gtk_widget_show(mShell) [%p]\n", this));
      gtk_widget_show(mShell);
      if (GdkIsWaylandDisplay()) {
        WaylandStartVsync();
      }
    } else if (mContainer) {
      LOG(("  calling gtk_widget_show(mContainer)\n"));
      gtk_widget_show(GTK_WIDGET(mContainer));
    } else if (mGdkWindow) {
      LOG(("  calling gdk_window_show_unraised\n"));
      gdk_window_show_unraised(mGdkWindow);
    }

    if (mHiddenPopupPositioned && IsPopup() && mIsTopLevel) {
      gtk_window_move(GTK_WINDOW(mShell), mPopupPosition.x, mPopupPosition.y);
      mHiddenPopupPositioned = false;
    }
  } else {
    // There's a chance that when the popup will be shown again it might be
    // resized because parent could be moved meanwhile.
    mPreferredPopupRect = nsRect(0, 0, 0, 0);
    mPreferredPopupRectFlushed = false;
    LOG(("nsWindow::NativeShow hide [%p]\n", this));
    if (GdkIsWaylandDisplay()) {
      WaylandStopVsync();
      if (IsWaylandPopup()) {
        // We can't close tracked popups directly as they may have visible
        // child popups. Just mark is as closed and let
        // UpdateWaylandPopupHierarchy() do the job.
        if (IsInPopupHierarchy()) {
          WaylandPopupMarkAsClosed();
          UpdateWaylandPopupHierarchy();
        } else {
          // Close untracked popups directly.
          HideWaylandPopupWindow(/* aTemporaryHide */ false,
                                 /* aRemoveFromPopupList */ true);
        }
      } else {
        HideWaylandToplevelWindow();
      }
    } else if (mIsTopLevel) {
      // Workaround window freezes on GTK versions before 3.21.2 by
      // ensuring that configure events get dispatched to windows before
      // they are unmapped. See bug 1225044.
      if (gtk_check_version(3, 21, 2) != nullptr && mPendingConfigures > 0) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(mShell), &allocation);

        GdkEventConfigure event;
        PodZero(&event);
        event.type = GDK_CONFIGURE;
        event.window = mGdkWindow;
        event.send_event = TRUE;
        event.x = allocation.x;
        event.y = allocation.y;
        event.width = allocation.width;
        event.height = allocation.height;

        auto* shellClass = GTK_WIDGET_GET_CLASS(mShell);
        for (unsigned int i = 0; i < mPendingConfigures; i++) {
          Unused << shellClass->configure_event(mShell, &event);
        }
        mPendingConfigures = 0;
      }
      gtk_widget_hide(mShell);

      ClearTransparencyBitmap();  // Release some resources
    } else if (mContainer) {
      gtk_widget_hide(GTK_WIDGET(mContainer));
    } else if (mGdkWindow) {
      gdk_window_hide(mGdkWindow);
    }
  }
}

void nsWindow::SetHasMappedToplevel(bool aState) {
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
  if (!mIsShown || !mGdkWindow) return;

  if (aState && !oldState) {
    // Check that a grab didn't fail due to the window not being
    // viewable.
    EnsureGrabs();
  }

  for (GList* children = gdk_window_peek_children(mGdkWindow); children;
       children = children->next) {
    GdkWindow* gdkWin = GDK_WINDOW(children->data);
    nsWindow* child = get_window_for_gdk_window(gdkWin);

    if (child && child->mHasMappedToplevel != aState) {
      child->SetHasMappedToplevel(aState);
    }
  }
}

LayoutDeviceIntSize nsWindow::GetSafeWindowSize(LayoutDeviceIntSize aSize) {
  // The X protocol uses CARD32 for window sizes, but the server (1.11.3)
  // reads it as CARD16.  Sizes of pixmaps, used for drawing, are (unsigned)
  // CARD16 in the protocol, but the server's ProcCreatePixmap returns
  // BadAlloc if dimensions cannot be represented by signed shorts.
  // Because we are creating Cairo surfaces to represent window buffers,
  // we also must ensure that the window can fit in a Cairo surface.
  LayoutDeviceIntSize result = aSize;
  int32_t maxSize = 32767;
  if (mWindowRenderer && mWindowRenderer->AsKnowsCompositor()) {
    maxSize = std::min(
        maxSize, mWindowRenderer->AsKnowsCompositor()->GetMaxTextureSize());
  }
  if (result.width > maxSize) {
    result.width = maxSize;
  }
  if (result.height > maxSize) {
    result.height = maxSize;
  }
  return result;
}

void nsWindow::EnsureGrabs(void) {
  if (mRetryPointerGrab) GrabPointer(sRetryGrabTime);
}

void nsWindow::CleanLayerManagerRecursive(void) {
  if (mWindowRenderer) {
    mWindowRenderer->Destroy();
    mWindowRenderer = nullptr;
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

void nsWindow::SetTransparencyMode(nsTransparencyMode aMode) {
  if (!mShell) {
    // Pass the request to the toplevel window
    GtkWidget* topWidget = GetToplevelWidget();
    if (!topWidget) return;

    nsWindow* topWindow = get_window_for_gtk_widget(topWidget);
    if (!topWindow) return;

    topWindow->SetTransparencyMode(aMode);
    return;
  }

  bool isTransparent = aMode == eTransparencyTransparent;

  if (mIsTransparent == isTransparent) {
    return;
  }

  if (mWindowType != eWindowType_popup) {
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1344839 reported
    // problems cleaning the layer manager for toplevel windows.
    // Ignore the request so as to workaround that.
    // mIsTransparent is set in Create() if transparency may be required.
    if (isTransparent) {
      NS_WARNING("Transparent mode not supported on non-popup windows.");
    }
    return;
  }

  if (!isTransparent) {
    ClearTransparencyBitmap();
  }  // else the new default alpha values are "all 1", so we don't
  // need to change anything yet

  mIsTransparent = isTransparent;

  if (!mHasAlphaVisual) {
    // The choice of layer manager depends on
    // GtkCompositorWidgetInitData::Shaped(), which will need to change, so
    // clean out the old layer manager.
    CleanLayerManagerRecursive();
  }
}

nsTransparencyMode nsWindow::GetTransparencyMode() {
  if (!mShell) {
    // Pass the request to the toplevel window
    GtkWidget* topWidget = GetToplevelWidget();
    if (!topWidget) {
      return eTransparencyOpaque;
    }

    nsWindow* topWindow = get_window_for_gtk_widget(topWidget);
    if (!topWindow) {
      return eTransparencyOpaque;
    }

    return topWindow->GetTransparencyMode();
  }

  return mIsTransparent ? eTransparencyTransparent : eTransparencyOpaque;
}

void nsWindow::SetWindowMouseTransparent(bool aIsTransparent) {
  if (!mGdkWindow) {
    return;
  }

  cairo_rectangle_int_t emptyRect = {0, 0, 0, 0};
  cairo_region_t* region =
      aIsTransparent ? cairo_region_create_rectangle(&emptyRect) : nullptr;

  gdk_window_input_shape_combine_region(mGdkWindow, region, 0, 0);
  if (region) {
    cairo_region_destroy(region);
  }
}

// For setting the draggable titlebar region from CSS
// with -moz-window-dragging: drag.
void nsWindow::UpdateWindowDraggingRegion(
    const LayoutDeviceIntRegion& aRegion) {
  if (mDraggableRegion != aRegion) {
    mDraggableRegion = aRegion;
  }
}

// See subtract_corners_from_region() at gtk/gtkwindow.c
// We need to subtract corners from toplevel window opaque region
// to draw transparent corners of default Gtk titlebar.
// Both implementations (cairo_region_t and wl_region) needs to be synced.
static void SubtractTitlebarCorners(cairo_region_t* aRegion, int aX, int aY,
                                    int aWindowWidth) {
  cairo_rectangle_int_t rect = {aX, aY, TITLEBAR_SHAPE_MASK_HEIGHT,
                                TITLEBAR_SHAPE_MASK_HEIGHT};
  cairo_region_subtract_rectangle(aRegion, &rect);
  rect = {
      aX + aWindowWidth - TITLEBAR_SHAPE_MASK_HEIGHT,
      aY,
      TITLEBAR_SHAPE_MASK_HEIGHT,
      TITLEBAR_SHAPE_MASK_HEIGHT,
  };
  cairo_region_subtract_rectangle(aRegion, &rect);
}

void nsWindow::UpdateTopLevelOpaqueRegion(void) {
  if (!mCompositedScreen) {
    return;
  }

  GdkWindow* window =
      (mDrawToContainer) ? gtk_widget_get_window(mShell) : mGdkWindow;
  if (!window) {
    return;
  }
  MOZ_ASSERT(gdk_window_get_window_type(window) == GDK_WINDOW_TOPLEVEL);

  int x = 0;
  int y = 0;

  if (mDrawToContainer) {
    gdk_window_get_position(mGdkWindow, &x, &y);
  }

  int width = DevicePixelsToGdkCoordRoundDown(mBounds.width);
  int height = DevicePixelsToGdkCoordRoundDown(mBounds.height);

  cairo_region_t* region = cairo_region_create();
  cairo_rectangle_int_t rect = {x, y, width, height};
  cairo_region_union_rectangle(region, &rect);

  bool subtractCorners = DoDrawTilebarCorners();
  if (subtractCorners) {
    SubtractTitlebarCorners(region, x, y, width);
  }

  gdk_window_set_opaque_region(window, region);

  cairo_region_destroy(region);

#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    moz_container_wayland_update_opaque_region(mContainer, subtractCorners);
  }
#endif
}

bool nsWindow::IsChromeWindowTitlebar() {
  return mDrawInTitlebar && !mIsPIPWindow &&
         mWindowType == eWindowType_toplevel;
}

bool nsWindow::DoDrawTilebarCorners() {
  return IsChromeWindowTitlebar() && mSizeState == nsSizeMode_Normal &&
         !mIsTiled;
}

nsresult nsWindow::ConfigureChildren(
    const nsTArray<Configuration>& aConfigurations) {
  // If this is a remotely updated widget we receive clipping, position, and
  // size information from a source other than our owner. Don't let our parent
  // update this information.
  if (mWindowType == eWindowType_plugin_ipc_chrome) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    auto* w = static_cast<nsWindow*>(configuration.mChild.get());
    NS_ASSERTION(w->GetParent() == this, "Configured widget is not a child");
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

nsresult nsWindow::SetWindowClipRegion(
    const nsTArray<LayoutDeviceIntRect>& aRects, bool aIntersectWithExisting) {
  const nsTArray<LayoutDeviceIntRect>* newRects = &aRects;

  AutoTArray<LayoutDeviceIntRect, 1> intersectRects;
  if (aIntersectWithExisting) {
    AutoTArray<LayoutDeviceIntRect, 1> existingRects;
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

  if (IsWindowClipRegionEqual(*newRects)) return NS_OK;

  StoreWindowClipRegion(*newRects);

  if (!mGdkWindow) return NS_OK;

  cairo_region_t* region = cairo_region_create();
  for (uint32_t i = 0; i < newRects->Length(); ++i) {
    const LayoutDeviceIntRect& r = newRects->ElementAt(i);
    cairo_rectangle_int_t rect = {r.x, r.y, r.width, r.height};
    cairo_region_union_rectangle(region, &rect);
  }

  gdk_window_shape_combine_region(mGdkWindow, region, 0, 0);
  cairo_region_destroy(region);

  return NS_OK;
}

void nsWindow::ResizeTransparencyBitmap() {
  if (!mTransparencyBitmap) return;

  if (mBounds.width == mTransparencyBitmapWidth &&
      mBounds.height == mTransparencyBitmapHeight) {
    return;
  }

  int32_t newRowBytes = GetBitmapStride(mBounds.width);
  int32_t newSize = newRowBytes * mBounds.height;
  auto* newBits = new gchar[newSize];
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

static bool ChangedMaskBits(gchar* aMaskBits, int32_t aMaskWidth,
                            int32_t aMaskHeight, const nsIntRect& aRect,
                            uint8_t* aAlphas, int32_t aStride) {
  int32_t x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
  int32_t maskBytesPerRow = GetBitmapStride(aMaskWidth);
  for (y = aRect.y; y < yMax; y++) {
    gchar* maskBytes = aMaskBits + y * maskBytesPerRow;
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

static void UpdateMaskBits(gchar* aMaskBits, int32_t aMaskWidth,
                           int32_t aMaskHeight, const nsIntRect& aRect,
                           uint8_t* aAlphas, int32_t aStride) {
  int32_t x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
  int32_t maskBytesPerRow = GetBitmapStride(aMaskWidth);
  for (y = aRect.y; y < yMax; y++) {
    gchar* maskBytes = aMaskBits + y * maskBytesPerRow;
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

void nsWindow::ApplyTransparencyBitmap() {
#ifdef MOZ_X11
  // We use X11 calls where possible, because GDK handles expose events
  // for shaped windows in a way that's incompatible with us (Bug 635903).
  // It doesn't occur when the shapes are set through X.
  Display* xDisplay = GDK_WINDOW_XDISPLAY(mGdkWindow);
  Window xDrawable = GDK_WINDOW_XID(mGdkWindow);
  Pixmap maskPixmap = XCreateBitmapFromData(
      xDisplay, xDrawable, mTransparencyBitmap, mTransparencyBitmapWidth,
      mTransparencyBitmapHeight);
  XShapeCombineMask(xDisplay, xDrawable, ShapeBounding, 0, 0, maskPixmap,
                    ShapeSet);
  XFreePixmap(xDisplay, maskPixmap);
#else
  cairo_surface_t* maskBitmap;
  maskBitmap = cairo_image_surface_create_for_data(
      (unsigned char*)mTransparencyBitmap, CAIRO_FORMAT_A1,
      mTransparencyBitmapWidth, mTransparencyBitmapHeight,
      GetBitmapStride(mTransparencyBitmapWidth));
  if (!maskBitmap) return;

  cairo_region_t* maskRegion = gdk_cairo_region_create_from_surface(maskBitmap);
  gtk_widget_shape_combine_region(mShell, maskRegion);
  cairo_region_destroy(maskRegion);
  cairo_surface_destroy(maskBitmap);
#endif  // MOZ_X11
}

void nsWindow::ClearTransparencyBitmap() {
  if (!mTransparencyBitmap) return;

  delete[] mTransparencyBitmap;
  mTransparencyBitmap = nullptr;
  mTransparencyBitmapWidth = 0;
  mTransparencyBitmapHeight = 0;

  if (!mShell) return;

#ifdef MOZ_X11
  if (!mGdkWindow) return;

  Display* xDisplay = GDK_WINDOW_XDISPLAY(mGdkWindow);
  Window xWindow = gdk_x11_window_get_xid(mGdkWindow);

  XShapeCombineMask(xDisplay, xWindow, ShapeBounding, 0, 0, X11None, ShapeSet);
#endif
}

nsresult nsWindow::UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                                        uint8_t* aAlphas,
                                                        int32_t aStride) {
  if (!mShell) {
    // Pass the request to the toplevel window
    GtkWidget* topWidget = GetToplevelWidget();
    if (!topWidget) return NS_ERROR_FAILURE;

    nsWindow* topWindow = get_window_for_gtk_widget(topWidget);
    if (!topWindow) return NS_ERROR_FAILURE;

    return topWindow->UpdateTranslucentWindowAlphaInternal(aRect, aAlphas,
                                                           aStride);
  }

  NS_ASSERTION(mIsTransparent, "Window is not transparent");
  NS_ASSERTION(!mTransparencyBitmapForTitlebar,
               "Transparency bitmap is already used for titlebar rendering");

  if (mTransparencyBitmap == nullptr) {
    int32_t size = GetBitmapStride(mBounds.width) * mBounds.height;
    mTransparencyBitmap = new gchar[size];
    memset(mTransparencyBitmap, 255, size);
    mTransparencyBitmapWidth = mBounds.width;
    mTransparencyBitmapHeight = mBounds.height;
  } else {
    ResizeTransparencyBitmap();
  }

  nsIntRect rect;
  rect.IntersectRect(aRect, nsIntRect(0, 0, mBounds.width, mBounds.height));

  if (!ChangedMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height, rect,
                       aAlphas, aStride)) {
    // skip the expensive stuff if the mask bits haven't changed; hopefully
    // this is the common case
    return NS_OK;
  }

  UpdateMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height, rect,
                 aAlphas, aStride);

  if (!mNeedsShow) {
    ApplyTransparencyBitmap();
  }
  return NS_OK;
}

LayoutDeviceIntRect nsWindow::GetTitlebarRect() {
  if (!mGdkWindow || !mDrawInTitlebar) {
    return LayoutDeviceIntRect();
  }

  return LayoutDeviceIntRect(0, 0, mBounds.width, TITLEBAR_SHAPE_MASK_HEIGHT);
}

void nsWindow::UpdateTitlebarTransparencyBitmap() {
  NS_ASSERTION(mTransparencyBitmapForTitlebar,
               "Transparency bitmap is already used to draw window shape");

  if (!mGdkWindow || !mDrawInTitlebar ||
      (mBounds.width == mTransparencyBitmapWidth &&
       mBounds.height == mTransparencyBitmapHeight)) {
    return;
  }

  bool maskCreate =
      !mTransparencyBitmap || mBounds.width > mTransparencyBitmapWidth;

  bool maskUpdate =
      !mTransparencyBitmap || mBounds.width != mTransparencyBitmapWidth;

  if (maskCreate) {
    delete[] mTransparencyBitmap;
    int32_t size = GetBitmapStride(mBounds.width) * TITLEBAR_SHAPE_MASK_HEIGHT;
    mTransparencyBitmap = new gchar[size];
    mTransparencyBitmapWidth = mBounds.width;
  } else {
    mTransparencyBitmapWidth = mBounds.width;
  }
  mTransparencyBitmapHeight = mBounds.height;

  if (maskUpdate) {
    cairo_surface_t* surface = cairo_image_surface_create(
        CAIRO_FORMAT_A8, mTransparencyBitmapWidth, TITLEBAR_SHAPE_MASK_HEIGHT);
    if (!surface) return;

    cairo_t* cr = cairo_create(surface);

    GtkWidgetState state;
    memset((void*)&state, 0, sizeof(state));
    GdkRectangle rect = {0, 0, mTransparencyBitmapWidth,
                         TITLEBAR_SHAPE_MASK_HEIGHT};

    moz_gtk_widget_paint(MOZ_GTK_HEADER_BAR, cr, &rect, &state, 0,
                         GTK_TEXT_DIR_NONE);

    cairo_destroy(cr);
    cairo_surface_mark_dirty(surface);
    cairo_surface_flush(surface);

    UpdateMaskBits(
        mTransparencyBitmap, mTransparencyBitmapWidth,
        TITLEBAR_SHAPE_MASK_HEIGHT,
        nsIntRect(0, 0, mTransparencyBitmapWidth, TITLEBAR_SHAPE_MASK_HEIGHT),
        cairo_image_surface_get_data(surface),
        cairo_format_stride_for_width(CAIRO_FORMAT_A8,
                                      mTransparencyBitmapWidth));

    cairo_surface_destroy(surface);
  }

  if (!mNeedsShow) {
    Display* xDisplay = GDK_WINDOW_XDISPLAY(mGdkWindow);
    Window xDrawable = GDK_WINDOW_XID(mGdkWindow);

    Pixmap maskPixmap = XCreateBitmapFromData(
        xDisplay, xDrawable, mTransparencyBitmap, mTransparencyBitmapWidth,
        TITLEBAR_SHAPE_MASK_HEIGHT);

    XShapeCombineMask(xDisplay, xDrawable, ShapeBounding, 0, 0, maskPixmap,
                      ShapeSet);

    if (mTransparencyBitmapHeight > TITLEBAR_SHAPE_MASK_HEIGHT) {
      XRectangle rect = {0, 0, (unsigned short)mTransparencyBitmapWidth,
                         (unsigned short)(mTransparencyBitmapHeight -
                                          TITLEBAR_SHAPE_MASK_HEIGHT)};
      XShapeCombineRectangles(xDisplay, xDrawable, ShapeBounding, 0,
                              TITLEBAR_SHAPE_MASK_HEIGHT, &rect, 1, ShapeUnion,
                              0);
    }

    XFreePixmap(xDisplay, maskPixmap);
  }
}

void nsWindow::GrabPointer(guint32 aTime) {
  LOG(("GrabPointer time=0x%08x retry=%d\n", (unsigned int)aTime,
       mRetryPointerGrab));

  mRetryPointerGrab = false;
  sRetryGrabTime = aTime;

  // If the window isn't visible, just set the flag to retry the
  // grab.  When this window becomes visible, the grab will be
  // retried.
  if (!mHasMappedToplevel) {
    LOG(("  window not visible\n"));
    mRetryPointerGrab = true;
    return;
  }

  if (!mGdkWindow) {
    return;
  }

  if (GdkIsWaylandDisplay()) {
    // Don't to the grab on Wayland as it causes a regression
    // from Bug 1377084.
    return;
  }

  gint retval;
  // Note that we need GDK_TOUCH_MASK below to work around a GDK/X11 bug that
  // causes touch events that would normally be received by this client on
  // other windows to be discarded during the grab.
  retval = gdk_pointer_grab(
      mGdkWindow, TRUE,
      (GdkEventMask)(GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                     GDK_POINTER_MOTION_MASK | GDK_TOUCH_MASK),
      (GdkWindow*)nullptr, nullptr, aTime);

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
        NewRunnableMethod("nsWindow::CheckForRollupDuringGrab", this,
                          &nsWindow::CheckForRollupDuringGrab);
    NS_DispatchToCurrentThread(event.forget());
  }
}

void nsWindow::ReleaseGrabs(void) {
  LOG(("ReleaseGrabs\n"));

  mRetryPointerGrab = false;

  if (GdkIsWaylandDisplay()) {
    // Don't to the ungrab on Wayland as it causes a regression
    // from Bug 1377084.
    return;
  }

  gdk_pointer_ungrab(GDK_CURRENT_TIME);
}

GtkWidget* nsWindow::GetToplevelWidget() {
  if (mShell) {
    return mShell;
  }

  GtkWidget* widget = GetMozContainerWidget();
  if (!widget) return nullptr;

  return gtk_widget_get_toplevel(widget);
}

GtkWidget* nsWindow::GetMozContainerWidget() {
  if (!mGdkWindow) return nullptr;

  if (mContainer) return GTK_WIDGET(mContainer);

  GtkWidget* owningWidget = get_gtk_widget_for_gdk_window(mGdkWindow);
  return owningWidget;
}

nsWindow* nsWindow::GetContainerWindow() {
  GtkWidget* owningWidget = GetMozContainerWidget();
  if (!owningWidget) return nullptr;

  nsWindow* window = get_window_for_gtk_widget(owningWidget);
  NS_ASSERTION(window, "No nsWindow for container widget");
  return window;
}

void nsWindow::SetUrgencyHint(GtkWidget* top_window, bool state) {
  if (!top_window) return;

  gdk_window_set_urgency_hint(gtk_widget_get_window(top_window), state);
}

void nsWindow::SetDefaultIcon(void) { SetIcon(u"default"_ns); }

gint nsWindow::ConvertBorderStyles(nsBorderStyle aStyle) {
  gint w = 0;

  if (aStyle == eBorderStyle_default) return -1;

  // note that we don't handle eBorderStyle_close yet
  if (aStyle & eBorderStyle_all) w |= GDK_DECOR_ALL;
  if (aStyle & eBorderStyle_border) w |= GDK_DECOR_BORDER;
  if (aStyle & eBorderStyle_resizeh) w |= GDK_DECOR_RESIZEH;
  if (aStyle & eBorderStyle_title) w |= GDK_DECOR_TITLE;
  if (aStyle & eBorderStyle_menu) w |= GDK_DECOR_MENU;
  if (aStyle & eBorderStyle_minimize) w |= GDK_DECOR_MINIMIZE;
  if (aStyle & eBorderStyle_maximize) w |= GDK_DECOR_MAXIMIZE;

  return w;
}

class FullscreenTransitionWindow final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  explicit FullscreenTransitionWindow(GtkWidget* aWidget);

  GtkWidget* mWindow;

 private:
  ~FullscreenTransitionWindow();
};

NS_IMPL_ISUPPORTS0(FullscreenTransitionWindow)

FullscreenTransitionWindow::FullscreenTransitionWindow(GtkWidget* aWidget) {
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
  MOZ_ASSERT(monitorRect.width > 0 && monitorRect.height > 0,
             "Can't resize window smaller than 1x1.");
  gtk_window_resize(gtkWin, monitorRect.width, monitorRect.height);

  GdkColor bgColor;
  bgColor.red = bgColor.green = bgColor.blue = 0;
  gtk_widget_modify_bg(mWindow, GTK_STATE_NORMAL, &bgColor);

  gtk_window_set_opacity(gtkWin, 0.0);
  gtk_widget_show(mWindow);
}

FullscreenTransitionWindow::~FullscreenTransitionWindow() {
  gtk_widget_destroy(mWindow);
}

class FullscreenTransitionData {
 public:
  FullscreenTransitionData(nsIWidget::FullscreenTransitionStage aStage,
                           uint16_t aDuration, nsIRunnable* aCallback,
                           FullscreenTransitionWindow* aWindow)
      : mStage(aStage),
        mStartTime(TimeStamp::Now()),
        mDuration(TimeDuration::FromMilliseconds(aDuration)),
        mCallback(aCallback),
        mWindow(aWindow) {}

  static const guint sInterval = 1000 / 30;  // 30fps
  static gboolean TimeoutCallback(gpointer aData);

 private:
  nsIWidget::FullscreenTransitionStage mStage;
  TimeStamp mStartTime;
  TimeDuration mDuration;
  nsCOMPtr<nsIRunnable> mCallback;
  RefPtr<FullscreenTransitionWindow> mWindow;
};

/* static */
gboolean FullscreenTransitionData::TimeoutCallback(gpointer aData) {
  bool finishing = false;
  auto* data = static_cast<FullscreenTransitionData*>(aData);
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

/* virtual */
bool nsWindow::PrepareForFullscreenTransition(nsISupports** aData) {
  if (!mCompositedScreen) {
    return false;
  }
  *aData = do_AddRef(new FullscreenTransitionWindow(mShell)).take();
  return true;
}

/* virtual */
void nsWindow::PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) {
  auto* data = static_cast<FullscreenTransitionWindow*>(aData);
  // This will be released at the end of the last timeout callback for it.
  auto* transitionData =
      new FullscreenTransitionData(aStage, aDuration, aCallback, data);
  g_timeout_add_full(G_PRIORITY_HIGH, FullscreenTransitionData::sInterval,
                     FullscreenTransitionData::TimeoutCallback, transitionData,
                     nullptr);
}

already_AddRefed<nsIScreen> nsWindow::GetWidgetScreen() {
  LOG(("nsWindow::GetWidgetScreen() [%p]", this));
  // Wayland can read screen directly
  if (GdkIsWaylandDisplay()) {
    RefPtr<nsIScreen> screen = ScreenHelperGTK::GetScreenForWindow(this);
    if (screen) {
      return screen.forget();
    }
  }

  LOG(("  fallback to Gtk code"));
  nsCOMPtr<nsIScreenManager> screenManager;
  screenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (!screenManager) {
    return nullptr;
  }

  // GetScreenBounds() is slow for the GTK port so we override and use
  // mBounds directly.
  LayoutDeviceIntRect bounds = mBounds;
  if (!mIsTopLevel) {
    bounds.MoveTo(WidgetToScreenOffset());
  }

  DesktopIntRect deskBounds = RoundedToInt(bounds / GetDesktopToDeviceScale());
  nsCOMPtr<nsIScreen> screen;
  screenManager->ScreenForRect(deskBounds.x, deskBounds.y, deskBounds.width,
                               deskBounds.height, getter_AddRefs(screen));
  return screen.forget();
}

RefPtr<VsyncSource> nsWindow::GetVsyncSource() {
#ifdef MOZ_WAYLAND
  if (mWaylandVsyncSource) {
    return mWaylandVsyncSource;
  }
#endif

  return nullptr;
}

static bool IsFullscreenSupported(GtkWidget* aShell) {
#ifdef MOZ_X11
  GdkScreen* screen = gtk_widget_get_screen(aShell);
  GdkAtom atom = gdk_atom_intern("_NET_WM_STATE_FULLSCREEN", FALSE);
  return gdk_x11_screen_supports_net_wm_hint(screen, atom);
#elif
  return true;
#endif
}

nsresult nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen) {
  LOG(("nsWindow::MakeFullScreen [%p] aFullScreen %d\n", (void*)this,
       aFullScreen));

  if (GdkIsX11Display() && !IsFullscreenSupported(mShell)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool wasFullscreen = mSizeState == nsSizeMode_Fullscreen;
  if (aFullScreen != wasFullscreen && mWidgetListener) {
    mWidgetListener->FullscreenWillChange(aFullScreen);
  }

  if (aFullScreen) {
    if (mSizeMode != nsSizeMode_Fullscreen) mLastSizeMode = mSizeMode;

    mSizeMode = nsSizeMode_Fullscreen;

    if (mIsPIPWindow) {
      gtk_window_set_type_hint(GTK_WINDOW(mShell), GDK_WINDOW_TYPE_HINT_NORMAL);
      if (gUseAspectRatio) {
        mAspectRatioSaved = mAspectRatio;
        mAspectRatio = 0.0f;
        ApplySizeConstraints();
      }
    }

    gtk_window_fullscreen(GTK_WINDOW(mShell));
  } else {
    mSizeMode = mLastSizeMode;
    gtk_window_unfullscreen(GTK_WINDOW(mShell));

    if (mIsPIPWindow) {
      gtk_window_set_type_hint(GTK_WINDOW(mShell),
                               GDK_WINDOW_TYPE_HINT_UTILITY);
      if (gUseAspectRatio) {
        mAspectRatio = mAspectRatioSaved;
        // ApplySizeConstraints();
      }
    }
  }

  NS_ASSERTION(mLastSizeMode != nsSizeMode_Fullscreen,
               "mLastSizeMode should never be fullscreen");
  return NS_OK;
}

void nsWindow::SetWindowDecoration(nsBorderStyle aStyle) {
  LOG(("nsWindow::SetWindowDecoration() [%p] Border style %x\n", (void*)this,
       aStyle));

  if (!mShell) {
    // Pass the request to the toplevel window
    GtkWidget* topWidget = GetToplevelWidget();
    if (!topWidget) return;

    nsWindow* topWindow = get_window_for_gtk_widget(topWidget);
    if (!topWindow) return;

    topWindow->SetWindowDecoration(aStyle);
    return;
  }

  // We can't use mGdkWindow directly here as it can be
  // derived from mContainer which is not a top-level GdkWindow.
  GdkWindow* window = gtk_widget_get_window(mShell);

  // Sawfish, metacity, and presumably other window managers get
  // confused if we change the window decorations while the window
  // is visible.
  bool wasVisible = false;
  if (gdk_window_is_visible(window)) {
    gdk_window_hide(window);
    wasVisible = true;
  }

  gint wmd = ConvertBorderStyles(aStyle);
  if (wmd != -1) gdk_window_set_decorations(window, (GdkWMDecoration)wmd);

  if (wasVisible) gdk_window_show(window);

    // For some window managers, adding or removing window decorations
    // requires unmapping and remapping our toplevel window.  Go ahead
    // and flush the queue here so that we don't end up with a BadWindow
    // error later when this happens (when the persistence timer fires
    // and GetWindowPos is called)
#ifdef MOZ_X11
  if (GdkIsX11Display()) {
    XSync(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), X11False);
  } else
#endif /* MOZ_X11 */
  {
    gdk_flush();
  }
}

void nsWindow::HideWindowChrome(bool aShouldHide) {
  SetWindowDecoration(aShouldHide ? eBorderStyle_none : mBorderStyle);
}

bool nsWindow::CheckForRollup(gdouble aMouseX, gdouble aMouseY, bool aIsWheel,
                              bool aAlwaysRollup) {
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
  auto* currentPopup =
      (GdkWindow*)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);
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
      uint32_t sameTypeCount =
          rollupListener->GetSubmenuWidgetChain(&widgetChain);
      for (unsigned long i = 0; i < widgetChain.Length(); ++i) {
        nsIWidget* widget = widgetChain[i];
        auto* currWindow = (GdkWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
        if (is_mouse_in_window(currWindow, aMouseX, aMouseY)) {
          // don't roll up if the mouse event occurred within a
          // menu of the same type. If the mouse event occurred
          // in a menu higher than that, roll up, but pass the
          // number of popups to Rollup so that only those of the
          // same type close up.
          if (i < sameTypeCount) {
            rollup = false;
          } else {
            popupsToRollup = sameTypeCount;
          }
          break;
        }
      }  // foreach parent menu widget
    }    // if rollup listener knows about menus

    // if we've determined that we should still rollup, do it.
    bool usePoint = !aIsWheel && !aAlwaysRollup;
    LayoutDeviceIntPoint point;
    if (usePoint) {
      point = GdkEventCoordsToDevicePixels(aMouseX, aMouseY);
    }
    if (rollup &&
        rollupListener->Rollup(popupsToRollup, true,
                               usePoint ? &point : nullptr, nullptr)) {
      retVal = true;
    }
  }
  return retVal;
}

/* static */
bool nsWindow::DragInProgress(void) {
  nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
  if (!dragService) {
    return false;
  }

  nsCOMPtr<nsIDragSession> currentDragSession;
  dragService->GetCurrentSession(getter_AddRefs(currentDragSession));

  return currentDragSession != nullptr;
}

// This is an ugly workaround for
// https://bugzilla.mozilla.org/show_bug.cgi?id=1622107
// We try to detect when Wayland compositor / gtk fails to deliver
// info about finished D&D operations and cancel it on our own.
MOZ_CAN_RUN_SCRIPT static void WaylandDragWorkaround(GdkEventButton* aEvent) {
  static int buttonPressCountWithDrag = 0;

  // We track only left button state as Firefox performs D&D on left
  // button only.
  if (aEvent->button != 1 || aEvent->type != GDK_BUTTON_PRESS) {
    return;
  }

  nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
  if (!dragService) {
    return;
  }
  nsCOMPtr<nsIDragSession> currentDragSession;
  dragService->GetCurrentSession(getter_AddRefs(currentDragSession));

  if (currentDragSession != nullptr) {
    buttonPressCountWithDrag++;
    if (buttonPressCountWithDrag > 1) {
      NS_WARNING(
          "Quit unfinished Wayland Drag and Drop operation. Buggy Wayland "
          "compositor?");
      buttonPressCountWithDrag = 0;
      dragService->EndDragSession(false, 0);
    }
  }
}

static bool is_mouse_in_window(GdkWindow* aWindow, gdouble aMouseX,
                               gdouble aMouseY) {
  gint x = 0;
  gint y = 0;
  gint w, h;

  gint offsetX = 0;
  gint offsetY = 0;

  GdkWindow* window = aWindow;

  while (window) {
    gint tmpX = 0;
    gint tmpY = 0;

    gdk_window_get_position(window, &tmpX, &tmpY);
    GtkWidget* widget = get_gtk_widget_for_gdk_window(window);

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

  w = gdk_window_get_width(aWindow);
  h = gdk_window_get_height(aWindow);

  return (aMouseX > x && aMouseX < x + w && aMouseY > y && aMouseY < y + h);
}

static nsWindow* get_window_for_gtk_widget(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "nsWindow");

  return static_cast<nsWindow*>(user_data);
}

static nsWindow* get_window_for_gdk_window(GdkWindow* window) {
  gpointer user_data = g_object_get_data(G_OBJECT(window), "nsWindow");

  return static_cast<nsWindow*>(user_data);
}

static GtkWidget* get_gtk_widget_for_gdk_window(GdkWindow* window) {
  gpointer user_data = nullptr;
  gdk_window_get_user_data(window, &user_data);

  return GTK_WIDGET(user_data);
}

static GdkCursor* get_gtk_cursor(nsCursor aCursor) {
  GdkCursor* gdkcursor = nullptr;
  uint8_t newType = 0xff;

  if ((gdkcursor = gCursorCache[aCursor])) {
    return gdkcursor;
  }

  GdkDisplay* defaultDisplay = gdk_display_get_default();

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
      gdkcursor =
          gdk_cursor_new_for_display(defaultDisplay, GDK_TOP_LEFT_CORNER);
      break;
    case eCursor_se_resize:
      gdkcursor =
          gdk_cursor_new_for_display(defaultDisplay, GDK_BOTTOM_RIGHT_CORNER);
      break;
    case eCursor_ne_resize:
      gdkcursor =
          gdk_cursor_new_for_display(defaultDisplay, GDK_TOP_RIGHT_CORNER);
      break;
    case eCursor_sw_resize:
      gdkcursor =
          gdk_cursor_new_for_display(defaultDisplay, GDK_BOTTOM_LEFT_CORNER);
      break;
    case eCursor_crosshair:
      gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_CROSSHAIR);
      break;
    case eCursor_move:
      gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_FLEUR);
      break;
    case eCursor_help:
      gdkcursor =
          gdk_cursor_new_for_display(defaultDisplay, GDK_QUESTION_ARROW);
      break;
    case eCursor_copy:  // CSS3
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "copy");
      if (!gdkcursor) newType = MOZ_CURSOR_COPY;
      break;
    case eCursor_alias:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "alias");
      if (!gdkcursor) newType = MOZ_CURSOR_ALIAS;
      break;
    case eCursor_context_menu:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "context-menu");
      if (!gdkcursor) newType = MOZ_CURSOR_CONTEXT_MENU;
      break;
    case eCursor_cell:
      gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_PLUS);
      break;
    // Those two aren’t standardized. Trying both KDE’s and GNOME’s names
    case eCursor_grab:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "openhand");
      if (!gdkcursor) newType = MOZ_CURSOR_HAND_GRAB;
      break;
    case eCursor_grabbing:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "closedhand");
      if (!gdkcursor) {
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "grabbing");
      }
      if (!gdkcursor) newType = MOZ_CURSOR_HAND_GRABBING;
      break;
    case eCursor_spinning:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "progress");
      if (!gdkcursor) newType = MOZ_CURSOR_SPINNING;
      break;
    case eCursor_zoom_in:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "zoom-in");
      if (!gdkcursor) newType = MOZ_CURSOR_ZOOM_IN;
      break;
    case eCursor_zoom_out:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "zoom-out");
      if (!gdkcursor) newType = MOZ_CURSOR_ZOOM_OUT;
      break;
    case eCursor_not_allowed:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "not-allowed");
      if (!gdkcursor) {  // nonstandard, yet common
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "crossed_circle");
      }
      if (!gdkcursor) newType = MOZ_CURSOR_NOT_ALLOWED;
      break;
    case eCursor_no_drop:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "no-drop");
      if (!gdkcursor) {  // this nonstandard sequence makes it work on KDE and
                         // GNOME
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "forbidden");
      }
      if (!gdkcursor) {
        gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "circle");
      }
      if (!gdkcursor) newType = MOZ_CURSOR_NOT_ALLOWED;
      break;
    case eCursor_vertical_text:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "vertical-text");
      if (!gdkcursor) {
        newType = MOZ_CURSOR_VERTICAL_TEXT;
      }
      break;
    case eCursor_all_scroll:
      gdkcursor = gdk_cursor_new_for_display(defaultDisplay, GDK_FLEUR);
      break;
    case eCursor_nesw_resize:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "size_bdiag");
      if (!gdkcursor) newType = MOZ_CURSOR_NESW_RESIZE;
      break;
    case eCursor_nwse_resize:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "size_fdiag");
      if (!gdkcursor) newType = MOZ_CURSOR_NWSE_RESIZE;
      break;
    case eCursor_ns_resize:
      gdkcursor =
          gdk_cursor_new_for_display(defaultDisplay, GDK_SB_V_DOUBLE_ARROW);
      break;
    case eCursor_ew_resize:
      gdkcursor =
          gdk_cursor_new_for_display(defaultDisplay, GDK_SB_H_DOUBLE_ARROW);
      break;
    // Here, two better fitting cursors exist in some cursor themes. Try those
    // first
    case eCursor_row_resize:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "split_v");
      if (!gdkcursor) {
        gdkcursor =
            gdk_cursor_new_for_display(defaultDisplay, GDK_SB_V_DOUBLE_ARROW);
      }
      break;
    case eCursor_col_resize:
      gdkcursor = gdk_cursor_new_from_name(defaultDisplay, "split_h");
      if (!gdkcursor) {
        gdkcursor =
            gdk_cursor_new_for_display(defaultDisplay, GDK_SB_H_DOUBLE_ARROW);
      }
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
    gdkcursor =
        gdk_cursor_new_from_name(defaultDisplay, GtkCursors[newType].hash);
  }

  // If we still don't have a xcursor, we now really create a bitmap cursor
  if (newType != 0xff && !gdkcursor) {
    GdkPixbuf* cursor_pixbuf =
        gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
    if (!cursor_pixbuf) return nullptr;

    guchar* data = gdk_pixbuf_get_pixels(cursor_pixbuf);

    // Read data from GtkCursors and compose RGBA surface from 1bit bitmap and
    // mask GtkCursors bits and mask are 32x32 monochrome bitmaps (1 bit for
    // each pixel) so it's 128 byte array (4 bytes for are one bitmap row and
    // there are 32 rows here).
    const unsigned char* bits = GtkCursors[newType].bits;
    const unsigned char* mask_bits = GtkCursors[newType].mask_bits;

    for (int i = 0; i < 128; i++) {
      char bit = (char)*bits++;
      char mask = (char)*mask_bits++;
      for (int j = 0; j < 8; j++) {
        unsigned char pix = ~(((bit >> j) & 0x01) * 0xff);
        *data++ = pix;
        *data++ = pix;
        *data++ = pix;
        *data++ = (((mask >> j) & 0x01) * 0xff);
      }
    }

    gdkcursor = gdk_cursor_new_from_pixbuf(
        gdk_display_get_default(), cursor_pixbuf, GtkCursors[newType].hot_x,
        GtkCursors[newType].hot_y);

    g_object_unref(cursor_pixbuf);
  }

  gCursorCache[aCursor] = gdkcursor;

  return gdkcursor;
}

// gtk callbacks

void draw_window_of_widget(GtkWidget* widget, GdkWindow* aWindow, cairo_t* cr) {
  if (gtk_cairo_should_draw_window(cr, aWindow)) {
    RefPtr<nsWindow> window = get_window_for_gdk_window(aWindow);
    if (!window) {
      NS_WARNING("Cannot get nsWindow from GtkWidget");
    } else {
      cairo_save(cr);
      gtk_cairo_transform_to_window(cr, widget, aWindow);
      // TODO - window->OnExposeEvent() can destroy this or other windows,
      // do we need to handle it somehow?
      window->OnExposeEvent(cr);
      cairo_restore(cr);
    }
  }

  GList* children = gdk_window_get_children(aWindow);
  GList* child = children;
  while (child) {
    GdkWindow* window = GDK_WINDOW(child->data);
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
gboolean expose_event_cb(GtkWidget* widget, cairo_t* cr) {
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

static gboolean configure_event_cb(GtkWidget* widget,
                                   GdkEventConfigure* event) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) {
    return FALSE;
  }

  return window->OnConfigureEvent(widget, event);
}

static void container_unrealize_cb(GtkWidget* widget) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) {
    return;
  }

  window->OnContainerUnrealize();
}

static void size_allocate_cb(GtkWidget* widget, GtkAllocation* allocation) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) {
    return;
  }

  window->OnSizeAllocate(allocation);
}

static void toplevel_window_size_allocate_cb(GtkWidget* widget,
                                             GtkAllocation* allocation) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) {
    return;
  }

  window->UpdateTopLevelOpaqueRegion();
}

static gboolean delete_event_cb(GtkWidget* widget, GdkEventAny* event) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) {
    return FALSE;
  }

  window->OnDeleteEvent();

  return TRUE;
}

static gboolean enter_notify_event_cb(GtkWidget* widget,
                                      GdkEventCrossing* event) {
  RefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
  if (!window) {
    return TRUE;
  }

  window->OnEnterNotifyEvent(event);

  return TRUE;
}

static gboolean leave_notify_event_cb(GtkWidget* widget,
                                      GdkEventCrossing* event) {
  if (is_parent_grab_leave(event)) {
    return TRUE;
  }

  // bug 369599: Suppress LeaveNotify events caused by pointer grabs to
  // avoid generating spurious mouse exit events.
  auto x = gint(event->x_root);
  auto y = gint(event->y_root);
  GdkDisplay* display = gtk_widget_get_display(widget);
  GdkWindow* winAtPt = gdk_display_get_window_at_pointer(display, &x, &y);
  if (winAtPt == event->window) {
    return TRUE;
  }

  RefPtr<nsWindow> window = get_window_for_gdk_window(event->window);
  if (!window) return TRUE;

  window->OnLeaveNotifyEvent(event);

  return TRUE;
}

static nsWindow* GetFirstNSWindowForGDKWindow(GdkWindow* aGdkWindow) {
  nsWindow* window;
  while (!(window = get_window_for_gdk_window(aGdkWindow))) {
    // The event has bubbled to the moz_container widget as passed into each
    // caller's *widget parameter, but its corresponding nsWindow is an ancestor
    // of the window that we need.  Instead, look at event->window and find the
    // first ancestor nsWindow of it because event->window may be in a plugin.
    aGdkWindow = gdk_window_get_parent(aGdkWindow);
    if (!aGdkWindow) {
      window = nullptr;
      break;
    }
  }
  return window;
}

static gboolean motion_notify_event_cb(GtkWidget* widget,
                                       GdkEventMotion* event) {
  UpdateLastInputEventTime(event);

  nsWindow* window = GetFirstNSWindowForGDKWindow(event->window);
  if (!window) return FALSE;

  window->OnMotionNotifyEvent(event);

  return TRUE;
}

static gboolean button_press_event_cb(GtkWidget* widget,
                                      GdkEventButton* event) {
  UpdateLastInputEventTime(event);

  nsWindow* window = GetFirstNSWindowForGDKWindow(event->window);
  if (!window) return FALSE;

  window->OnButtonPressEvent(event);

  if (GdkIsWaylandDisplay()) {
    WaylandDragWorkaround(event);
  }

  return TRUE;
}

static gboolean button_release_event_cb(GtkWidget* widget,
                                        GdkEventButton* event) {
  UpdateLastInputEventTime(event);

  nsWindow* window = GetFirstNSWindowForGDKWindow(event->window);
  if (!window) return FALSE;

  window->OnButtonReleaseEvent(event);

  return TRUE;
}

static gboolean focus_in_event_cb(GtkWidget* widget, GdkEventFocus* event) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) return FALSE;

  window->OnContainerFocusInEvent(event);

  return FALSE;
}

static gboolean focus_out_event_cb(GtkWidget* widget, GdkEventFocus* event) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) return FALSE;

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

static GdkFilterReturn popup_take_focus_filter(GdkXEvent* gdk_xevent,
                                               GdkEvent* event, gpointer data) {
  auto* xevent = static_cast<XEvent*>(gdk_xevent);
  if (xevent->type != ClientMessage) return GDK_FILTER_CONTINUE;

  XClientMessageEvent& xclient = xevent->xclient;
  if (xclient.message_type != gdk_x11_get_xatom_by_name("WM_PROTOCOLS")) {
    return GDK_FILTER_CONTINUE;
  }

  Atom atom = xclient.data.l[0];
  if (atom != gdk_x11_get_xatom_by_name("WM_TAKE_FOCUS")) {
    return GDK_FILTER_CONTINUE;
  }

  guint32 timestamp = xclient.data.l[1];

  GtkWidget* widget = get_gtk_widget_for_gdk_window(event->any.window);
  if (!widget) return GDK_FILTER_CONTINUE;

  GtkWindow* parent = gtk_window_get_transient_for(GTK_WINDOW(widget));
  if (!parent) return GDK_FILTER_CONTINUE;

  if (gtk_window_is_active(parent)) {
    return GDK_FILTER_REMOVE;  // leave input focus on the parent
  }

  GdkWindow* parent_window = gtk_widget_get_window(GTK_WIDGET(parent));
  if (!parent_window) return GDK_FILTER_CONTINUE;

  // In case the parent has not been deconified.
  gdk_window_show_unraised(parent_window);

  // Request focus on the parent window.
  // Use gdk_window_focus rather than gtk_window_present to avoid
  // raising the parent window.
  gdk_window_focus(parent_window, timestamp);
  return GDK_FILTER_REMOVE;
}
#endif /* MOZ_X11 */

static gboolean key_press_event_cb(GtkWidget* widget, GdkEventKey* event) {
  LOGW(("key_press_event_cb\n"));

  UpdateLastInputEventTime(event);

  // find the window with focus and dispatch this event to that widget
  nsWindow* window = get_window_for_gtk_widget(widget);
  if (!window) return FALSE;

  RefPtr<nsWindow> focusWindow = gFocusWindow ? gFocusWindow : window;

#ifdef MOZ_X11
  // Keyboard repeat can cause key press events to queue up when there are
  // slow event handlers (bug 301029).  Throttle these events by removing
  // consecutive pending duplicate KeyPress events to the same window.
  // We use the event time of the last one.
  // Note: GDK calls XkbSetDetectableAutorepeat so that KeyRelease events
  // are generated only when the key is physically released.
#  define NS_GDKEVENT_MATCH_MASK 0x1FFF  // GDK_SHIFT_MASK .. GDK_BUTTON5_MASK
  // Our headers undefine X11 KeyPress - let's redefine it here.
#  ifndef KeyPress
#    define KeyPress 2
#  endif
  GdkDisplay* gdkDisplay = gtk_widget_get_display(widget);
  if (GdkIsX11Display(gdkDisplay)) {
    Display* dpy = GDK_DISPLAY_XDISPLAY(gdkDisplay);
    while (XPending(dpy)) {
      XEvent next_event;
      XPeekEvent(dpy, &next_event);
      GdkWindow* nextGdkWindow =
          gdk_x11_window_lookup_for_display(gdkDisplay, next_event.xany.window);
      if (nextGdkWindow != event->window || next_event.type != KeyPress ||
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

static gboolean key_release_event_cb(GtkWidget* widget, GdkEventKey* event) {
  LOGW(("key_release_event_cb\n"));

  UpdateLastInputEventTime(event);

  // find the window with focus and dispatch this event to that widget
  nsWindow* window = get_window_for_gtk_widget(widget);
  if (!window) return FALSE;

  RefPtr<nsWindow> focusWindow = gFocusWindow ? gFocusWindow : window;

  return focusWindow->OnKeyReleaseEvent(event);
}

static gboolean property_notify_event_cb(GtkWidget* aWidget,
                                         GdkEventProperty* aEvent) {
  RefPtr<nsWindow> window = get_window_for_gdk_window(aEvent->window);
  if (!window) return FALSE;

  return window->OnPropertyNotifyEvent(aWidget, aEvent);
}

static gboolean scroll_event_cb(GtkWidget* widget, GdkEventScroll* event) {
  nsWindow* window = GetFirstNSWindowForGDKWindow(event->window);
  if (!window) return FALSE;

  window->OnScrollEvent(event);

  return TRUE;
}

static void hierarchy_changed_cb(GtkWidget* widget,
                                 GtkWidget* previous_toplevel) {
  GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
  GdkWindowState old_window_state = GDK_WINDOW_STATE_WITHDRAWN;
  GdkEventWindowState event;

  event.new_window_state = GDK_WINDOW_STATE_WITHDRAWN;

  if (GTK_IS_WINDOW(previous_toplevel)) {
    g_signal_handlers_disconnect_by_func(
        previous_toplevel, FuncToGpointer(window_state_event_cb), widget);
    GdkWindow* win = gtk_widget_get_window(previous_toplevel);
    if (win) {
      old_window_state = gdk_window_get_state(win);
    }
  }

  if (GTK_IS_WINDOW(toplevel)) {
    g_signal_connect_swapped(toplevel, "window-state-event",
                             G_CALLBACK(window_state_event_cb), widget);
    GdkWindow* win = gtk_widget_get_window(toplevel);
    if (win) {
      event.new_window_state = gdk_window_get_state(win);
    }
  }

  event.changed_mask =
      static_cast<GdkWindowState>(old_window_state ^ event.new_window_state);

  if (event.changed_mask) {
    event.type = GDK_WINDOW_STATE;
    event.window = nullptr;
    event.send_event = TRUE;
    window_state_event_cb(widget, &event);
  }
}

static gboolean window_state_event_cb(GtkWidget* widget,
                                      GdkEventWindowState* event) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) return FALSE;

  window->OnWindowStateEvent(widget, event);

  return FALSE;
}

static void settings_xft_dpi_changed_cb(GtkSettings* gtk_settings,
                                        GParamSpec* pspec, nsWindow* data) {
  RefPtr<nsWindow> window = data;
  window->OnDPIChanged();
  // Even though the window size in screen pixels has not changed,
  // nsViewManager stores the dimensions in app units.
  // DispatchResized() updates those.
  window->DispatchResized();
}

static void check_resize_cb(GtkContainer* container, gpointer user_data) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(GTK_WIDGET(container));
  if (!window) {
    return;
  }
  window->OnCheckResize();
}

static void screen_composited_changed_cb(GdkScreen* screen,
                                         gpointer user_data) {
  // This callback can run before gfxPlatform::Init() in rare
  // cases involving the profile manager. When this happens,
  // we have no reason to reset any compositors as graphics
  // hasn't been initialized yet.
  if (GPUProcessManager::Get()) {
    GPUProcessManager::Get()->ResetCompositors();
  }
}

static void widget_composited_changed_cb(GtkWidget* widget,
                                         gpointer user_data) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) {
    return;
  }
  window->OnCompositedChanged();
}

static void scale_changed_cb(GtkWidget* widget, GParamSpec* aPSpec,
                             gpointer aPointer) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(widget);
  if (!window) {
    return;
  }

  window->OnScaleChanged();
}

static gboolean touch_event_cb(GtkWidget* aWidget, GdkEventTouch* aEvent) {
  UpdateLastInputEventTime(aEvent);

  nsWindow* window = GetFirstNSWindowForGDKWindow(aEvent->window);
  if (!window) {
    return FALSE;
  }

  return window->OnTouchEvent(aEvent);
}

// This function called generic because there is no signal specific to touchpad
// pinch events.
static gboolean generic_event_cb(GtkWidget* widget, GdkEvent* aEvent) {
  if (aEvent->type != GDK_TOUCHPAD_PINCH) {
    return FALSE;
  }
  // Using reinterpret_cast because the touchpad_pinch field of GdkEvent is not
  // available in GTK+ versions lower than v3.18
  GdkEventTouchpadPinch* event =
      reinterpret_cast<GdkEventTouchpadPinch*>(aEvent);

  nsWindow* window = GetFirstNSWindowForGDKWindow(event->window);

  if (!window) {
    return FALSE;
  }
  return window->OnTouchpadPinchEvent(event);
}

//////////////////////////////////////////////////////////////////////
// These are all of our drag and drop operations

void nsWindow::InitDragEvent(WidgetDragEvent& aEvent) {
  // set the keyboard modifiers
  guint modifierState = KeymapWrapper::GetCurrentModifierState();
  KeymapWrapper::InitInputEvent(aEvent, modifierState);
}

gboolean WindowDragMotionHandler(GtkWidget* aWidget,
                                 GdkDragContext* aDragContext,
                                 RefPtr<DataOffer> aDataOffer, gint aX, gint aY,
                                 guint aTime) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
  if (!window) {
    return FALSE;
  }

  // figure out which internal widget this drag motion actually happened on
  nscoord retx = 0;
  nscoord rety = 0;

  GdkWindow* innerWindow = get_inner_gdk_window(gtk_widget_get_window(aWidget),
                                                aX, aY, &retx, &rety);
  RefPtr<nsWindow> innerMostWindow = get_window_for_gdk_window(innerWindow);

  if (!innerMostWindow) {
    innerMostWindow = window;
  }

  LOGDRAG(("WindowDragMotionHandler nsWindow %p\n", (void*)innerMostWindow));

  LayoutDeviceIntPoint point = window->GdkPointToDevicePixels({retx, rety});

  RefPtr<nsDragService> dragService = nsDragService::GetInstance();
  return dragService->ScheduleMotionEvent(innerMostWindow, aDragContext,
                                          aDataOffer, point, aTime);
}

static gboolean drag_motion_event_cb(GtkWidget* aWidget,
                                     GdkDragContext* aDragContext, gint aX,
                                     gint aY, guint aTime, gpointer aData) {
  return WindowDragMotionHandler(aWidget, aDragContext, nullptr, aX, aY, aTime);
}

void WindowDragLeaveHandler(GtkWidget* aWidget) {
  LOGDRAG(("WindowDragLeaveHandler()\n"));

  RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
  if (!window) {
    LOGDRAG(("    Failed - can't find nsWindow!\n"));
    return;
  }

  RefPtr<nsDragService> dragService = nsDragService::GetInstance();

  nsWindow* mostRecentDragWindow = dragService->GetMostRecentDestWindow();
  if (!mostRecentDragWindow) {
    // This can happen when the target will not accept a drop.  A GTK drag
    // source sends the leave message to the destination before the
    // drag-failed signal on the source widget, but the leave message goes
    // via the X server, and so doesn't get processed at least until the
    // event loop runs again.
    LOGDRAG(("    Failed - GetMostRecentDestWindow()!\n"));
    return;
  }

  GtkWidget* mozContainer = mostRecentDragWindow->GetMozContainerWidget();
  if (aWidget != mozContainer) {
    // When the drag moves between widgets, GTK can send leave signal for
    // the old widget after the motion or drop signal for the new widget.
    // We'll send the leave event when the motion or drop event is run.
    LOGDRAG(("    Failed - GetMozContainerWidget()!\n"));
    return;
  }

  LOGDRAG(
      ("WindowDragLeaveHandler nsWindow %p\n", (void*)mostRecentDragWindow));
  dragService->ScheduleLeaveEvent();
}

static void drag_leave_event_cb(GtkWidget* aWidget,
                                GdkDragContext* aDragContext, guint aTime,
                                gpointer aData) {
  WindowDragLeaveHandler(aWidget);
}

gboolean WindowDragDropHandler(GtkWidget* aWidget, GdkDragContext* aDragContext,
                               RefPtr<DataOffer> aDataOffer, gint aX, gint aY,
                               guint aTime) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
  if (!window) return FALSE;

  // figure out which internal widget this drag motion actually happened on
  nscoord retx = 0;
  nscoord rety = 0;

  GdkWindow* innerWindow = get_inner_gdk_window(gtk_widget_get_window(aWidget),
                                                aX, aY, &retx, &rety);
  RefPtr<nsWindow> innerMostWindow = get_window_for_gdk_window(innerWindow);

  if (!innerMostWindow) {
    innerMostWindow = window;
  }

  LOGDRAG(("WindowDragDropHandler nsWindow %p\n", (void*)innerMostWindow));

  LayoutDeviceIntPoint point = window->GdkPointToDevicePixels({retx, rety});

  RefPtr<nsDragService> dragService = nsDragService::GetInstance();
  return dragService->ScheduleDropEvent(innerMostWindow, aDragContext,
                                        aDataOffer, point, aTime);
}

static gboolean drag_drop_event_cb(GtkWidget* aWidget,
                                   GdkDragContext* aDragContext, gint aX,
                                   gint aY, guint aTime, gpointer aData) {
  return WindowDragDropHandler(aWidget, aDragContext, nullptr, aX, aY, aTime);
}

static void drag_data_received_event_cb(GtkWidget* aWidget,
                                        GdkDragContext* aDragContext, gint aX,
                                        gint aY,
                                        GtkSelectionData* aSelectionData,
                                        guint aInfo, guint aTime,
                                        gpointer aData) {
  RefPtr<nsWindow> window = get_window_for_gtk_widget(aWidget);
  if (!window) return;

  window->OnDragDataReceivedEvent(aWidget, aDragContext, aX, aY, aSelectionData,
                                  aInfo, aTime, aData);
}

static nsresult initialize_prefs(void) {
  gRaiseWindows =
      Preferences::GetBool("mozilla.widget.raise-on-setfocus", true);

  if (Preferences::HasUserValue("widget.use-aspect-ratio")) {
    gUseAspectRatio = Preferences::GetBool("widget.use-aspect-ratio", true);
  } else {
    static const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
    gUseAspectRatio =
        currentDesktop ? (strstr(currentDesktop, "GNOME") != nullptr) : false;
  }

  return NS_OK;
}

static GdkWindow* get_inner_gdk_window(GdkWindow* aWindow, gint x, gint y,
                                       gint* retx, gint* rety) {
  gint cx, cy, cw, ch;
  GList* children = gdk_window_peek_children(aWindow);
  for (GList* child = g_list_last(children); child;
       child = g_list_previous(child)) {
    auto* childWindow = (GdkWindow*)child->data;
    if (get_window_for_gdk_window(childWindow)) {
      gdk_window_get_geometry(childWindow, &cx, &cy, &cw, &ch);
      if ((cx < x) && (x < (cx + cw)) && (cy < y) && (y < (cy + ch)) &&
          gdk_window_is_visible(childWindow)) {
        return get_inner_gdk_window(childWindow, x - cx, y - cy, retx, rety);
      }
    }
  }
  *retx = x;
  *rety = y;
  return aWindow;
}

static int is_parent_ungrab_enter(GdkEventCrossing* aEvent) {
  return (GDK_CROSSING_UNGRAB == aEvent->mode) &&
         ((GDK_NOTIFY_ANCESTOR == aEvent->detail) ||
          (GDK_NOTIFY_VIRTUAL == aEvent->detail));
}

static int is_parent_grab_leave(GdkEventCrossing* aEvent) {
  return (GDK_CROSSING_GRAB == aEvent->mode) &&
         ((GDK_NOTIFY_ANCESTOR == aEvent->detail) ||
          (GDK_NOTIFY_VIRTUAL == aEvent->detail));
}

#ifdef ACCESSIBILITY
void nsWindow::CreateRootAccessible() {
  if (mIsTopLevel && !mRootAccessible) {
    LOG(("nsWindow:: Create Toplevel Accessibility\n"));
    mRootAccessible = GetRootAccessible();
  }
}

void nsWindow::DispatchEventToRootAccessible(uint32_t aEventType) {
  if (!a11y::ShouldA11yBeEnabled()) {
    return;
  }

  nsAccessibilityService* accService = GetOrCreateAccService();
  if (!accService) {
    return;
  }

  // Get the root document accessible and fire event to it.
  a11y::LocalAccessible* acc = GetRootAccessible();
  if (acc) {
    accService->FireAccessibleEvent(aEventType, acc);
  }
}

void nsWindow::DispatchActivateEventAccessible(void) {
  DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_ACTIVATE);
}

void nsWindow::DispatchDeactivateEventAccessible(void) {
  DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_DEACTIVATE);
}

void nsWindow::DispatchMaximizeEventAccessible(void) {
  DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_MAXIMIZE);
}

void nsWindow::DispatchMinimizeEventAccessible(void) {
  DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_MINIMIZE);
}

void nsWindow::DispatchRestoreEventAccessible(void) {
  DispatchEventToRootAccessible(nsIAccessibleEvent::EVENT_WINDOW_RESTORE);
}

#endif /* #ifdef ACCESSIBILITY */

void nsWindow::SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) {
  if (!mIMContext) {
    return;
  }
  mIMContext->SetInputContext(this, &aContext, &aAction);
}

InputContext nsWindow::GetInputContext() {
  InputContext context;
  if (!mIMContext) {
    context.mIMEState.mEnabled = IMEEnabled::Disabled;
    context.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  } else {
    context = mIMContext->GetInputContext();
  }
  return context;
}

TextEventDispatcherListener* nsWindow::GetNativeTextEventDispatcherListener() {
  if (NS_WARN_IF(!mIMContext)) {
    return nullptr;
  }
  return mIMContext;
}

bool nsWindow::GetEditCommands(NativeKeyBindingsType aType,
                               const WidgetKeyboardEvent& aEvent,
                               nsTArray<CommandInt>& aCommands) {
  // Validate the arguments.
  if (NS_WARN_IF(!nsIWidget::GetEditCommands(aType, aEvent, aCommands))) {
    return false;
  }

  Maybe<WritingMode> writingMode;
  if (aEvent.NeedsToRemapNavigationKey()) {
    if (RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher()) {
      writingMode = dispatcher->MaybeWritingModeAtSelection();
    }
  }

  NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
  keyBindings->GetEditCommands(aEvent, writingMode, aCommands);
  return true;
}

already_AddRefed<DrawTarget> nsWindow::StartRemoteDrawingInRegion(
    const LayoutDeviceIntRegion& aInvalidRegion, BufferMode* aBufferMode) {
  return mSurfaceProvider.StartRemoteDrawingInRegion(aInvalidRegion,
                                                     aBufferMode);
}

void nsWindow::EndRemoteDrawingInRegion(
    DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
  mSurfaceProvider.EndRemoteDrawingInRegion(aDrawTarget, aInvalidRegion);
}

// Code shared begin BeginMoveDrag and BeginResizeDrag
bool nsWindow::GetDragInfo(WidgetMouseEvent* aMouseEvent, GdkWindow** aWindow,
                           gint* aButton, gint* aRootX, gint* aRootY) {
  if (aMouseEvent->mButton != MouseButton::ePrimary) {
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
  MOZ_ASSERT(gdk_window, "gdk_window_get_toplevel should not return null");
  *aWindow = gdk_window;

  if (!aMouseEvent->mWidget) {
    return false;
  }

  if (GdkIsX11Display()) {
    // Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=789054
    // To avoid crashes disable double-click on WM without _NET_WM_MOVERESIZE.
    // See _should_perform_ewmh_drag() at gdkwindow-x11.c
    GdkScreen* screen = gdk_window_get_screen(gdk_window);
    GdkAtom atom = gdk_atom_intern("_NET_WM_MOVERESIZE", FALSE);
    if (!gdk_x11_screen_supports_net_wm_hint(screen, atom)) {
      static unsigned int lastTimeStamp = 0;
      if (lastTimeStamp != aMouseEvent->mTime) {
        lastTimeStamp = aMouseEvent->mTime;
      } else {
        return false;
      }
    }
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

nsresult nsWindow::BeginResizeDrag(WidgetGUIEvent* aEvent, int32_t aHorizontal,
                                   int32_t aVertical) {
  NS_ENSURE_ARG_POINTER(aEvent);

  if (aEvent->mClass != eMouseEventClass) {
    // you can only begin a resize drag with a mouse event
    return NS_ERROR_INVALID_ARG;
  }

  GdkWindow* gdk_window;
  gint button, screenX, screenY;
  if (!GetDragInfo(aEvent->AsMouseEvent(), &gdk_window, &button, &screenX,
                   &screenY)) {
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
  gdk_window_begin_resize_drag(gdk_window, window_edge, button, screenX,
                               screenY, aEvent->mTime);

  return NS_OK;
}

nsIWidget::WindowRenderer* nsWindow::GetWindowRenderer() {
  if (mIsDestroyed) {
    // Prevent external code from triggering the re-creation of the
    // LayerManager/Compositor during shutdown. Just return what we currently
    // have, which is most likely null.
    return mWindowRenderer;
  }

  return nsBaseWidget::GetWindowRenderer();
}

void nsWindow::SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) {
  LOG(("nsWindow::SetCompositorWidgetDelegate [%p] %p\n", (void*)this,
       delegate));

  if (delegate) {
    mCompositorWidgetDelegate = delegate->AsPlatformSpecificDelegate();
    MOZ_ASSERT(mCompositorWidgetDelegate,
               "nsWindow::SetCompositorWidgetDelegate called with a "
               "non-PlatformCompositorWidgetDelegate");
    ResumeCompositorHiddenWindow();
    WaylandStartVsync();
  } else {
    WaylandStopVsync();
    mCompositorWidgetDelegate = nullptr;
  }
}

void nsWindow::ClearCachedResources() {
  if (mWindowRenderer && mWindowRenderer->GetBackendType() ==
                             mozilla::layers::LayersBackend::LAYERS_BASIC) {
    mWindowRenderer->AsLayerManager()->ClearCachedResources();
  }

  GList* children = gdk_window_peek_children(mGdkWindow);
  for (GList* list = children; list; list = list->next) {
    nsWindow* window = get_window_for_gdk_window(GDK_WINDOW(list->data));
    if (window) {
      window->ClearCachedResources();
    }
  }
}

/* nsWindow::UpdateClientOffsetFromCSDWindow() is designed to be called from
 * nsWindow::OnConfigureEvent() when mContainer window is already positioned.
 *
 * It works only for CSD decorated GtkWindow.
 */
void nsWindow::UpdateClientOffsetFromCSDWindow() {
  int x, y;
  gdk_window_get_position(mGdkWindow, &x, &y);

  x = GdkCoordToDevicePixels(x);
  y = GdkCoordToDevicePixels(y);

  if (mClientOffset.x != x || mClientOffset.y != y) {
    mClientOffset = nsIntPoint(x, y);

    LOG(("nsWindow::UpdateClientOffsetFromCSDWindow [%p] %d, %d\n", (void*)this,
         mClientOffset.x, mClientOffset.y));

    // Send a WindowMoved notification. This ensures that BrowserParent
    // picks up the new client offset and sends it to the child process
    // if appropriate.
    NotifyWindowMoved(mBounds.x, mBounds.y);
  }
}

nsresult nsWindow::SetNonClientMargins(LayoutDeviceIntMargin& aMargins) {
  SetDrawsInTitlebar(aMargins.top == 0);
  return NS_OK;
}

void nsWindow::SetDrawsInTitlebar(bool aState) {
  LOG(("nsWindow::SetDrawsInTitlebar() [%p] State %d mGtkWindowDecoration %d\n",
       (void*)this, aState, (int)mGtkWindowDecoration));

  if (mIsPIPWindow && aState == mDrawInTitlebar) {
    gtk_window_set_decorated(GTK_WINDOW(mShell), !aState);
    return;
  }

  if (!mShell || mGtkWindowDecoration == GTK_DECORATION_NONE ||
      aState == mDrawInTitlebar) {
    return;
  }

  if (mGtkWindowDecoration == GTK_DECORATION_SYSTEM) {
    SetWindowDecoration(aState ? eBorderStyle_border : mBorderStyle);
  } else if (mGtkWindowDecoration == GTK_DECORATION_CLIENT) {
    LOG(("    Using CSD mode\n"));

    /* Window manager does not support GDK_DECOR_BORDER,
     * emulate it by CSD.
     *
     * gtk_window_set_titlebar() works on unrealized widgets only,
     * we need to handle mShell carefully here.
     * When CSD is enabled mGdkWindow is owned by mContainer which is good
     * as we can't delete our mGdkWindow. To make mShell unrealized while
     * mContainer is preserved we temporary reparent mContainer to an
     * invisible GtkWindow.
     */
    NativeShow(false);

    // Using GTK_WINDOW_POPUP rather than
    // GTK_WINDOW_TOPLEVEL in the hope that POPUP results in less
    // initialization and window manager interaction.
    GtkWidget* tmpWindow = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_realize(tmpWindow);

    gtk_widget_reparent(GTK_WIDGET(mContainer), tmpWindow);
    gtk_widget_unrealize(GTK_WIDGET(mShell));

    if (aState) {
      // Add a hidden titlebar widget to trigger CSD, but disable the default
      // titlebar.  GtkFixed is a somewhat random choice for a simple unused
      // widget. gtk_window_set_titlebar() takes ownership of the titlebar
      // widget.
      gtk_window_set_titlebar(GTK_WINDOW(mShell), gtk_fixed_new());
    } else {
      gtk_window_set_titlebar(GTK_WINDOW(mShell), nullptr);
    }

    /* A workaround for https://bugzilla.gnome.org/show_bug.cgi?id=791081
     * gtk_widget_realize() throws:
     * "In pixman_region32_init_rect: Invalid rectangle passed"
     * when mShell has default 1x1 size.
     */
    GtkAllocation allocation = {0, 0, 0, 0};
    gtk_widget_get_preferred_width(GTK_WIDGET(mShell), nullptr,
                                   &allocation.width);
    gtk_widget_get_preferred_height(GTK_WIDGET(mShell), nullptr,
                                    &allocation.height);
    gtk_widget_size_allocate(GTK_WIDGET(mShell), &allocation);

    gtk_widget_realize(GTK_WIDGET(mShell));
    gtk_widget_reparent(GTK_WIDGET(mContainer), GTK_WIDGET(mShell));
    mNeedsShow = true;
    NativeResize();

    // Label mShell toplevel window so property_notify_event_cb callback
    // can find its way home.
    g_object_set_data(G_OBJECT(gtk_widget_get_window(mShell)), "nsWindow",
                      this);
#ifdef MOZ_X11
    SetCompositorHint(GTK_WIDGET_COMPOSIDED_ENABLED);
#endif
    RefreshWindowClass();

    gtk_widget_destroy(tmpWindow);
  }

  mDrawInTitlebar = aState;

  if (mTransparencyBitmapForTitlebar) {
    if (mDrawInTitlebar && mSizeState == nsSizeMode_Normal && !mIsTiled) {
      UpdateTitlebarTransparencyBitmap();
    } else {
      ClearTransparencyBitmap();
    }
  }
}

GtkWindow* nsWindow::GetCurrentTopmostWindow() {
  GtkWindow* parentWindow = GTK_WINDOW(GetGtkWidget());
  GtkWindow* topmostParentWindow = nullptr;
  while (parentWindow) {
    topmostParentWindow = parentWindow;
    parentWindow = gtk_window_get_transient_for(parentWindow);
  }
  return topmostParentWindow;
}

gint nsWindow::GdkCeiledScaleFactor() {
  // We depend on notify::scale-factor callback which is reliable for toplevel
  // windows only, so don't use scale cache for popup windows.
  if (mWindowType == eWindowType_toplevel && !mWindowScaleFactorChanged) {
    return mWindowScaleFactor;
  }

  GdkWindow* scaledGdkWindow = mGdkWindow;
  if (GdkIsWaylandDisplay()) {
    // For popup windows/dialogs with parent window we need to get scale factor
    // of the topmost window. Otherwise the scale factor of the popup is
    // not updated during it's hidden.
    if (mWindowType == eWindowType_popup || mWindowType == eWindowType_dialog) {
      // Get toplevel window for scale factor:
      GtkWindow* topmostParentWindow = GetCurrentTopmostWindow();
      if (topmostParentWindow) {
        scaledGdkWindow =
            gtk_widget_get_window(GTK_WIDGET(topmostParentWindow));
      } else {
        NS_WARNING("Popup/Dialog has no parent.");
      }
      // Fallback for windows which parent has been unrealized.
      if (!scaledGdkWindow) {
        scaledGdkWindow = mGdkWindow;
      }
    }
  }

  if (scaledGdkWindow) {
    mWindowScaleFactor = gdk_window_get_scale_factor(scaledGdkWindow);
    mWindowScaleFactorChanged = false;
  } else {
    mWindowScaleFactor = ScreenHelperGTK::GetGTKMonitorScaleFactor();
  }

  return mWindowScaleFactor;
}

bool nsWindow::UseFractionalScale() {
#ifdef MOZ_WAYLAND
  return (GdkIsWaylandDisplay() &&
          StaticPrefs::widget_wayland_fractional_buffer_scale_AtStartup() > 0 &&
          WaylandDisplayGet()->GetViewporter());
#else
  return false;
#endif
}

double nsWindow::FractionalScaleFactor() {
#ifdef MOZ_WAYLAND
  if (UseFractionalScale()) {
    double scale =
        StaticPrefs::widget_wayland_fractional_buffer_scale_AtStartup();
    scale = std::max(scale, 0.5);
    scale = std::min(scale, 8.0);
    return scale;
  }
#endif
  return GdkCeiledScaleFactor();
}

gint nsWindow::DevicePixelsToGdkCoordRoundUp(int pixels) {
  double scale = FractionalScaleFactor();
  return ceil(pixels / scale);
}

gint nsWindow::DevicePixelsToGdkCoordRoundDown(int pixels) {
  double scale = FractionalScaleFactor();
  return floor(pixels / scale);
}

GdkPoint nsWindow::DevicePixelsToGdkPointRoundDown(LayoutDeviceIntPoint point) {
  double scale = FractionalScaleFactor();
  return {int(point.x / scale), int(point.y / scale)};
}

GdkRectangle nsWindow::DevicePixelsToGdkRectRoundOut(LayoutDeviceIntRect rect) {
  double scale = FractionalScaleFactor();
  int x = floor(rect.x / scale);
  int y = floor(rect.y / scale);
  int right = ceil((rect.x + rect.width) / scale);
  int bottom = ceil((rect.y + rect.height) / scale);
  return {x, y, right - x, bottom - y};
}

GdkRectangle nsWindow::DevicePixelsToGdkSizeRoundUp(
    LayoutDeviceIntSize pixelSize) {
  double scale = FractionalScaleFactor();
  gint width = ceil(pixelSize.width / scale);
  gint height = ceil(pixelSize.height / scale);
  return {0, 0, width, height};
}

int nsWindow::GdkCoordToDevicePixels(gint coord) {
  return (int)(coord * FractionalScaleFactor());
}

LayoutDeviceIntPoint nsWindow::GdkEventCoordsToDevicePixels(gdouble x,
                                                            gdouble y) {
  double scale = FractionalScaleFactor();
  return LayoutDeviceIntPoint::Floor((float)(x * scale), (float)(y * scale));
}

LayoutDeviceIntPoint nsWindow::GdkPointToDevicePixels(GdkPoint point) {
  double scale = FractionalScaleFactor();
  return LayoutDeviceIntPoint::Floor((float)(point.x * scale),
                                     (float)(point.y * scale));
}

LayoutDeviceIntRect nsWindow::GdkRectToDevicePixels(GdkRectangle rect) {
  double scale = FractionalScaleFactor();
  return LayoutDeviceIntRect::RoundIn(
      (float)(rect.x * scale), (float)(rect.y * scale),
      (float)(rect.width * scale), (float)(rect.height * scale));
}

nsresult nsWindow::SynthesizeNativeMouseEvent(
    LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
    MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
    nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mouseevent");

  if (!mGdkWindow) {
    return NS_OK;
  }

  GdkDisplay* display = gdk_window_get_display(mGdkWindow);

  // When a button-press/release event is requested, create it here and put it
  // in the event queue. This will not emit a motion event - this needs to be
  // done explicitly *before* requesting a button-press/release. You will also
  // need to wait for the motion event to be dispatched before requesting a
  // button-press/release event to maintain the desired event order.
  switch (aNativeMessage) {
    case NativeMouseMessage::ButtonDown:
    case NativeMouseMessage::ButtonUp: {
      GdkEvent event;
      memset(&event, 0, sizeof(GdkEvent));
      event.type = aNativeMessage == NativeMouseMessage::ButtonDown
                       ? GDK_BUTTON_PRESS
                       : GDK_BUTTON_RELEASE;
      switch (aButton) {
        case MouseButton::ePrimary:
        case MouseButton::eMiddle:
        case MouseButton::eSecondary:
        case MouseButton::eX1:
        case MouseButton::eX2:
          event.button.button = aButton + 1;
          break;
        default:
          return NS_ERROR_INVALID_ARG;
      }
      event.button.state =
          KeymapWrapper::ConvertWidgetModifierToGdkState(aModifierFlags);
      event.button.window = mGdkWindow;
      event.button.time = GDK_CURRENT_TIME;

      // Get device for event source
      GdkDeviceManager* device_manager =
          gdk_display_get_device_manager(display);
      event.button.device =
          gdk_device_manager_get_client_pointer(device_manager);

      event.button.x_root = DevicePixelsToGdkCoordRoundDown(aPoint.x);
      event.button.y_root = DevicePixelsToGdkCoordRoundDown(aPoint.y);

      LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
      event.button.x = DevicePixelsToGdkCoordRoundDown(pointInWindow.x);
      event.button.y = DevicePixelsToGdkCoordRoundDown(pointInWindow.y);

      gdk_event_put(&event);
      return NS_OK;
    }
    case NativeMouseMessage::Move: {
      // We don't support specific events other than button-press/release. In
      // all other cases we'll synthesize a motion event that will be emitted by
      // gdk_display_warp_pointer().
      // XXX How to activate native modifier for the other events?
#ifdef MOZ_WAYLAND
      // Impossible to warp the pointer on Wayland.
      // For pointer lock, pointer-constraints and relative-pointer are used.
      if (GdkIsWaylandDisplay()) {
        return NS_OK;
      }
#endif
      GdkScreen* screen = gdk_window_get_screen(mGdkWindow);
      GdkPoint point = DevicePixelsToGdkPointRoundDown(aPoint);
      gdk_display_warp_pointer(display, screen, point.x, point.y);
      return NS_OK;
    }
    case NativeMouseMessage::EnterWindow:
    case NativeMouseMessage::LeaveWindow:
      MOZ_ASSERT_UNREACHABLE("Non supported mouse event on Linux");
      return NS_ERROR_INVALID_ARG;
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult nsWindow::SynthesizeNativeMouseScrollEvent(
    mozilla::LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage,
    double aDeltaX, double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
    uint32_t aAdditionalFlags, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");

  if (!mGdkWindow) {
    return NS_OK;
  }

  GdkEvent event;
  memset(&event, 0, sizeof(GdkEvent));
  event.type = GDK_SCROLL;
  event.scroll.window = mGdkWindow;
  event.scroll.time = GDK_CURRENT_TIME;
  // Get device for event source
  GdkDisplay* display = gdk_window_get_display(mGdkWindow);
  GdkDeviceManager* device_manager = gdk_display_get_device_manager(display);
  event.scroll.device = gdk_device_manager_get_client_pointer(device_manager);
  event.scroll.x_root = DevicePixelsToGdkCoordRoundDown(aPoint.x);
  event.scroll.y_root = DevicePixelsToGdkCoordRoundDown(aPoint.y);

  LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
  event.scroll.x = DevicePixelsToGdkCoordRoundDown(pointInWindow.x);
  event.scroll.y = DevicePixelsToGdkCoordRoundDown(pointInWindow.y);

  // The delta values are backwards on Linux compared to Windows and Cocoa,
  // hence the negation.
  event.scroll.direction = GDK_SCROLL_SMOOTH;
  event.scroll.delta_x = -aDeltaX;
  event.scroll.delta_y = -aDeltaY;

  gdk_event_put(&event);

  return NS_OK;
}

nsresult nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) {
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

nsresult nsWindow::SynthesizeNativeTouchPadPinch(
    TouchpadGesturePhase aEventPhase, float aScale, LayoutDeviceIntPoint aPoint,
    int32_t aModifierFlags) {
  if (!mGdkWindow) {
    return NS_OK;
  }
  GdkEvent event;
  memset(&event, 0, sizeof(GdkEvent));

  GdkEventTouchpadPinch* touchpad_event =
      reinterpret_cast<GdkEventTouchpadPinch*>(&event);
  touchpad_event->type = GDK_TOUCHPAD_PINCH;

  switch (aEventPhase) {
    case PHASE_BEGIN:
      touchpad_event->phase = GDK_TOUCHPAD_GESTURE_PHASE_BEGIN;
      break;
    case PHASE_UPDATE:
      touchpad_event->phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
      break;
    case PHASE_END:
      touchpad_event->phase = GDK_TOUCHPAD_GESTURE_PHASE_END;
      break;

    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  touchpad_event->window = mGdkWindow;
  // We only set the fields of GdkEventTouchpadPinch which are
  // actually used in OnTouchpadPinchEvent().
  // GdkEventTouchpadPinch has additional fields (for example, `dx` and `dy`).
  // If OnTouchpadPinchEvent() is changed to use other fields, this function
  // will need to change to set them as well.
  touchpad_event->time = GDK_CURRENT_TIME;
  touchpad_event->scale = aScale;
  touchpad_event->x_root = DevicePixelsToGdkCoordRoundDown(aPoint.x);
  touchpad_event->y_root = DevicePixelsToGdkCoordRoundDown(aPoint.y);

  LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
  touchpad_event->x = DevicePixelsToGdkCoordRoundDown(pointInWindow.x);
  touchpad_event->y = DevicePixelsToGdkCoordRoundDown(pointInWindow.y);
  touchpad_event->state = aModifierFlags;

  gdk_event_put(&event);

  return NS_OK;
}

nsWindow::GtkWindowDecoration nsWindow::GetSystemGtkWindowDecoration() {
  if (sGtkWindowDecoration != GTK_DECORATION_UNKNOWN) {
    return sGtkWindowDecoration;
  }

  // Allow MOZ_GTK_TITLEBAR_DECORATION to override our heuristics
  const char* decorationOverride = getenv("MOZ_GTK_TITLEBAR_DECORATION");
  if (decorationOverride) {
    if (strcmp(decorationOverride, "none") == 0) {
      sGtkWindowDecoration = GTK_DECORATION_NONE;
    } else if (strcmp(decorationOverride, "client") == 0) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else if (strcmp(decorationOverride, "system") == 0) {
      sGtkWindowDecoration = GTK_DECORATION_SYSTEM;
    }
    return sGtkWindowDecoration;
  }

  // nsWindow::GetSystemGtkWindowDecoration can be called from various threads
  // so we can't use gfxPlatformGtk here.
  if (GdkIsWaylandDisplay()) {
    sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    return sGtkWindowDecoration;
  }

  // GTK_CSD forces CSD mode - use also CSD because window manager
  // decorations does not work with CSD.
  // We check GTK_CSD as well as gtk_window_should_use_csd() does.
  const char* csdOverride = getenv("GTK_CSD");
  if (csdOverride && *csdOverride == '1') {
    sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    return sGtkWindowDecoration;
  }

  const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
  if (currentDesktop) {
    // GNOME Flashback (fallback)
    if (strstr(currentDesktop, "GNOME-Flashback:GNOME") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_SYSTEM;
      // Pop Linux Bug 1629198
    } else if (strstr(currentDesktop, "pop:GNOME") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
      // gnome-shell
    } else if (strstr(currentDesktop, "GNOME") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_SYSTEM;
    } else if (strstr(currentDesktop, "XFCE") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else if (strstr(currentDesktop, "X-Cinnamon") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_SYSTEM;
      // KDE Plasma
    } else if (strstr(currentDesktop, "KDE") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else if (strstr(currentDesktop, "Enlightenment") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else if (strstr(currentDesktop, "LXDE") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else if (strstr(currentDesktop, "openbox") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else if (strstr(currentDesktop, "i3") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_NONE;
    } else if (strstr(currentDesktop, "MATE") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
      // Ubuntu Unity
    } else if (strstr(currentDesktop, "Unity") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_SYSTEM;
      // Elementary OS
    } else if (strstr(currentDesktop, "Pantheon") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else if (strstr(currentDesktop, "LXQt") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_SYSTEM;
    } else if (strstr(currentDesktop, "Deepin") != nullptr) {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    } else {
      sGtkWindowDecoration = GTK_DECORATION_CLIENT;
    }
  } else {
    sGtkWindowDecoration = GTK_DECORATION_NONE;
  }
  return sGtkWindowDecoration;
}

bool nsWindow::TitlebarUseShapeMask() {
  static int useShapeMask = []() {
    // Don't use titlebar shape mask on Wayland
    if (!GdkIsX11Display()) {
      return false;
    }

    // We can'y use shape masks on Mutter/X.org as we can't resize Firefox
    // window there (Bug 1530252).
    const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
    if (currentDesktop) {
      if (strstr(currentDesktop, "GNOME") != nullptr) {
        const char* sessionType = getenv("XDG_SESSION_TYPE");
        if (sessionType && strstr(sessionType, "x11") != nullptr) {
          return false;
        }
      }
    }

    return Preferences::GetBool("widget.titlebar-x11-use-shape-mask", false);
  }();
  return useShapeMask;
}

bool nsWindow::HideTitlebarByDefault() {
  static int hideTitlebar = []() {
    // When user defined widget.default-hidden-titlebar don't do any
    // heuristics and just follow it.
    if (Preferences::HasUserValue("widget.default-hidden-titlebar")) {
      return Preferences::GetBool("widget.default-hidden-titlebar", false);
    }

    // Don't hide titlebar when it's disabled on current desktop.
    const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
    if (!currentDesktop ||
        GetSystemGtkWindowDecoration() == GTK_DECORATION_NONE) {
      return false;
    }

    // We hide system titlebar on Gnome/ElementaryOS without any restriction.
    return ((strstr(currentDesktop, "GNOME-Flashback:GNOME") != nullptr ||
             strstr(currentDesktop, "GNOME") != nullptr ||
             strstr(currentDesktop, "Pantheon") != nullptr));
  }();
  return hideTitlebar;
}

int32_t nsWindow::RoundsWidgetCoordinatesTo() { return GdkCeiledScaleFactor(); }

void nsWindow::GetCompositorWidgetInitData(
    mozilla::widget::CompositorWidgetInitData* aInitData) {
  nsCString displayName;

  if (GdkIsX11Display() && mXWindow != X11None) {
    // Make sure the window XID is propagated to X server, we can fail otherwise
    // in GPU process (Bug 1401634).
    Display* display = DefaultXDisplay();
    XFlush(display);
    displayName = nsCString(XDisplayString(display));
  }

  bool isShaped =
      mIsTransparent && !mHasAlphaVisual && !mTransparencyBitmapForTitlebar;
  *aInitData = mozilla::widget::GtkCompositorWidgetInitData(
      (mXWindow != X11None) ? mXWindow : (uintptr_t) nullptr, displayName,
      isShaped, GdkIsX11Display(), GetClientSize());
}

#ifdef MOZ_WAYLAND
bool nsWindow::WaylandSurfaceNeedsClear() {
  if (mContainer) {
    return moz_container_wayland_surface_needs_clear(MOZ_CONTAINER(mContainer));
  }
  return false;
}
#endif

#ifdef MOZ_X11
/* XApp progress support currently works by setting a property
 * on a window with this Atom name.  A supporting window manager
 * will notice this and pass it along to whatever handling has
 * been implemented on that end (e.g. passing it on to a taskbar
 * widget.)  There is no issue if WM support is lacking, this is
 * simply ignored in that case.
 *
 * See https://github.com/linuxmint/xapps/blob/master/libxapp/xapp-gtk-window.c
 * for further details.
 */

#  define PROGRESS_HINT "_NET_WM_XAPP_PROGRESS"

static void set_window_hint_cardinal(Window xid, const gchar* atom_name,
                                     gulong cardinal) {
  GdkDisplay* display;

  display = gdk_display_get_default();

  if (cardinal > 0) {
    XChangeProperty(GDK_DISPLAY_XDISPLAY(display), xid,
                    gdk_x11_get_xatom_by_name_for_display(display, atom_name),
                    XA_CARDINAL, 32, PropModeReplace, (guchar*)&cardinal, 1);
  } else {
    XDeleteProperty(GDK_DISPLAY_XDISPLAY(display), xid,
                    gdk_x11_get_xatom_by_name_for_display(display, atom_name));
  }
}
#endif  // MOZ_X11

void nsWindow::SetProgress(unsigned long progressPercent) {
#ifdef MOZ_X11

  if (!GdkIsX11Display()) {
    return;
  }

  if (!mShell) {
    return;
  }

  progressPercent = MIN(progressPercent, 100);

  set_window_hint_cardinal(GDK_WINDOW_XID(gtk_widget_get_window(mShell)),
                           PROGRESS_HINT, progressPercent);
#endif  // MOZ_X11
}

#ifdef MOZ_X11
void nsWindow::SetCompositorHint(WindowComposeRequest aState) {
  if (!GdkIsX11Display()) {
    return;
  }

  gulong value = aState;
  GdkAtom cardinal_atom = gdk_x11_xatom_to_atom(XA_CARDINAL);
  gdk_property_change(gtk_widget_get_window(mShell),
                      gdk_atom_intern("_NET_WM_BYPASS_COMPOSITOR", FALSE),
                      cardinal_atom,
                      32,  // format
                      GDK_PROP_MODE_REPLACE, (guchar*)&value, 1);
}
#endif

nsresult nsWindow::SetSystemFont(const nsCString& aFontName) {
  GtkSettings* settings = gtk_settings_get_default();
  g_object_set(settings, "gtk-font-name", aFontName.get(), nullptr);
  return NS_OK;
}

nsresult nsWindow::GetSystemFont(nsCString& aFontName) {
  GtkSettings* settings = gtk_settings_get_default();
  gchar* fontName = nullptr;
  g_object_get(settings, "gtk-font-name", &fontName, nullptr);
  if (fontName) {
    aFontName.Assign(fontName);
    g_free(fontName);
  }
  return NS_OK;
}

already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow();
  return window.forget();
}

already_AddRefed<nsIWidget> nsIWidget::CreateChildWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow();
  return window.forget();
}

#ifdef MOZ_WAYLAND
static void relative_pointer_handle_relative_motion(
    void* data, struct zwp_relative_pointer_v1* pointer, uint32_t time_hi,
    uint32_t time_lo, wl_fixed_t dx_w, wl_fixed_t dy_w, wl_fixed_t dx_unaccel_w,
    wl_fixed_t dy_unaccel_w) {
  RefPtr<nsWindow> window(reinterpret_cast<nsWindow*>(data));

  WidgetMouseEvent event(true, eMouseMove, window, WidgetMouseEvent::eReal);

  event.mRefPoint = window->GetNativePointerLockCenter();
  event.mRefPoint.x += wl_fixed_to_double(dx_unaccel_w);
  event.mRefPoint.y += wl_fixed_to_double(dy_unaccel_w);

  event.AssignEventTime(window->GetWidgetEventTime(time_lo));
  window->DispatchInputEvent(&event);
}

static const struct zwp_relative_pointer_v1_listener relative_pointer_listener =
    {
        relative_pointer_handle_relative_motion,
};

void nsWindow::SetNativePointerLockCenter(
    const LayoutDeviceIntPoint& aLockCenter) {
  mNativePointerLockCenter = aLockCenter;
}

void nsWindow::LockNativePointer() {
  if (!GdkIsWaylandDisplay()) {
    return;
  }

  auto waylandDisplay = WaylandDisplayGet();

  auto* pointerConstraints = waylandDisplay->GetPointerConstraints();
  if (!pointerConstraints) {
    return;
  }

  auto* relativePointerMgr = waylandDisplay->GetRelativePointerManager();
  if (!relativePointerMgr) {
    return;
  }

  GdkDisplay* display = gdk_display_get_default();

  GdkDeviceManager* manager = gdk_display_get_device_manager(display);
  MOZ_ASSERT(manager);

  GdkDevice* device = gdk_device_manager_get_client_pointer(manager);
  if (!device) {
    NS_WARNING("Could not find Wayland pointer to lock");
    return;
  }
  wl_pointer* pointer = gdk_wayland_device_get_wl_pointer(device);
  MOZ_ASSERT(pointer);

  wl_surface* surface =
      gdk_wayland_window_get_wl_surface(gtk_widget_get_window(GetGtkWidget()));
  if (!surface) {
    /* Can be null when the window is hidden.
     * Though it's unlikely that a lock request comes in that case, be
     * defensive. */
    return;
  }

  mLockedPointer = zwp_pointer_constraints_v1_lock_pointer(
      pointerConstraints, surface, pointer, nullptr,
      ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
  if (!mLockedPointer) {
    NS_WARNING("Could not lock Wayland pointer");
    return;
  }

  mRelativePointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
      relativePointerMgr, pointer);
  if (!mRelativePointer) {
    NS_WARNING("Could not create relative Wayland pointer");
    zwp_locked_pointer_v1_destroy(mLockedPointer);
    mLockedPointer = nullptr;
    return;
  }

  zwp_relative_pointer_v1_add_listener(mRelativePointer,
                                       &relative_pointer_listener, this);
}

void nsWindow::UnlockNativePointer() {
  if (!GdkIsWaylandDisplay()) {
    return;
  }
  if (mRelativePointer) {
    zwp_relative_pointer_v1_destroy(mRelativePointer);
    mRelativePointer = nullptr;
  }
  if (mLockedPointer) {
    zwp_locked_pointer_v1_destroy(mLockedPointer);
    mLockedPointer = nullptr;
  }
}
#endif

bool nsWindow::GetTopLevelWindowActiveState(nsIFrame* aFrame) {
  // Used by window frame and button box rendering. We can end up in here in
  // the content process when rendering one of these moz styles freely in a
  // page. Fail in this case, there is no applicable window focus state.
  if (!XRE_IsParentProcess()) {
    return false;
  }
  // All headless windows are considered active so they are painted.
  if (gfxPlatform::IsHeadless()) {
    return true;
  }
  // Get the widget. nsIFrame's GetNearestWidget walks up the view chain
  // until it finds a real window.
  nsWindow* window = static_cast<nsWindow*>(aFrame->GetNearestWidget());
  if (!window) {
    return false;
  }

  // Get our toplevel nsWindow.
  if (!window->mIsTopLevel) {
    GtkWidget* widget = window->GetMozContainerWidget();
    if (!widget) {
      return false;
    }

    GtkWidget* toplevelWidget = gtk_widget_get_toplevel(widget);
    window = get_window_for_gtk_widget(toplevelWidget);
    if (!window) {
      return false;
    }
  }

  return !window->mTitlebarBackdropState;
}

static nsIFrame* FindTitlebarFrame(nsIFrame* aFrame) {
  for (nsIFrame* childFrame : aFrame->PrincipalChildList()) {
    StyleAppearance appearance =
        childFrame->StyleDisplay()->EffectiveAppearance();
    if (appearance == StyleAppearance::MozWindowTitlebar ||
        appearance == StyleAppearance::MozWindowTitlebarMaximized) {
      return childFrame;
    }

    if (nsIFrame* foundFrame = FindTitlebarFrame(childFrame)) {
      return foundFrame;
    }
  }
  return nullptr;
}

nsIFrame* nsWindow::GetFrame(void) {
  nsView* view = nsView::GetViewFor(this);
  if (!view) {
    return nullptr;
  }
  return view->GetFrame();
}

void nsWindow::UpdateMozWindowActive() {
  // Update activation state for the :-moz-window-inactive pseudoclass.
  // Normally, this follows focus; we override it here to follow
  // GDK_WINDOW_STATE_FOCUSED.
  if (mozilla::dom::Document* document = GetDocument()) {
    if (nsPIDOMWindowOuter* window = document->GetWindow()) {
      if (RefPtr<mozilla::dom::BrowsingContext> bc =
              window->GetBrowsingContext()) {
        bc->SetIsActiveBrowserWindow(!mTitlebarBackdropState);
      }
    }
  }
}

void nsWindow::ForceTitlebarRedraw(void) {
  MOZ_ASSERT(mDrawInTitlebar, "We should not redraw invisible titlebar.");

  if (!mWidgetListener || !mWidgetListener->GetPresShell()) {
    return;
  }

  nsIFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  frame = FindTitlebarFrame(frame);
  if (frame) {
    nsIContent* content = frame->GetContent();
    if (content) {
      nsLayoutUtils::PostRestyleEvent(content->AsElement(), RestyleHint{0},
                                      nsChangeHint_RepaintFrame);
    }
  }
}

void nsWindow::LockAspectRatio(bool aShouldLock) {
  if (!gUseAspectRatio) {
    return;
  }

  if (aShouldLock) {
    int decWidth = 0, decHeight = 0;
    AddCSDDecorationSize(&decWidth, &decHeight);

    float width =
        (float)DevicePixelsToGdkCoordRoundDown(mBounds.width) + (float)decWidth;
    float height = (float)DevicePixelsToGdkCoordRoundDown(mBounds.height) +
                   (float)decHeight;

    mAspectRatio = width / height;
    LOG(("nsWindow::LockAspectRatio() [%p] width %f height %f aspect %f\n",
         (void*)this, width, height, mAspectRatio));
  } else {
    mAspectRatio = 0.0;
    LOG(("nsWindow::LockAspectRatio() [%p] removed aspect ratio\n",
         (void*)this));
  }

  ApplySizeConstraints();
}

nsWindow* nsWindow::GetFocusedWindow() { return gFocusWindow; }

#ifdef MOZ_WAYLAND
void nsWindow::SetEGLNativeWindowSize(
    const LayoutDeviceIntSize& aEGLWindowSize) {
  if (!mContainer || !GdkIsWaylandDisplay()) {
    return;
  }
  moz_container_wayland_egl_window_set_size(mContainer, aEGLWindowSize.width,
                                            aEGLWindowSize.height);
  moz_container_wayland_set_scale_factor(mContainer);
}
#endif

LayoutDeviceIntSize nsWindow::GetMozContainerSize() {
  LayoutDeviceIntSize size(0, 0);
  if (mContainer) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(mContainer), &allocation);
    double scale = FractionalScaleFactor();
    size.width = round(allocation.width * scale);
    size.height = round(allocation.height * scale);
  }
  return size;
}
