/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIWidget_h__
#define nsIWidget_h__

#include <cmath>
#include <cstdint>
#include "ErrorList.h"
#include "Units.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/layers/ZoomConstraints.h"
#include "mozilla/widget/IMEData.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "nsDebug.h"
#include "nsID.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsITheme.h"
#include "nsITimer.h"
#include "nsIWidgetListener.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashMap.h"
#include "nsWidgetInitData.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_IS_GCC
#  include "VsyncSource.h"
#endif

// forward declarations
class nsIBidiKeyboard;
class nsIRollupListener;
class imgIContainer;
class nsIContent;
class ViewWrapper;
class nsIScreen;
class nsIRunnable;
class nsUint64HashKey;

namespace mozilla {
class NativeEventData;
class WidgetGUIEvent;
class WidgetInputEvent;
class WidgetKeyboardEvent;
struct FontRange;

enum class StyleWindowShadow : uint8_t;

#if defined(MOZ_WIDGET_ANDROID)
namespace ipc {
class Shmem;
}
#endif  // defined(MOZ_WIDGET_ANDROID)
namespace dom {
class BrowserChild;
enum class CallerType : uint32_t;
}  // namespace dom
namespace plugins {
class PluginWidgetChild;
}  // namespace plugins
namespace layers {
class AsyncDragMetrics;
class Compositor;
class CompositorBridgeChild;
struct FrameMetrics;
class LayerManager;
class LayerManagerComposite;
class PLayerTransactionChild;
class WebRenderBridgeChild;
}  // namespace layers
namespace gfx {
class VsyncSource;
}  // namespace gfx
namespace widget {
class TextEventDispatcher;
class TextEventDispatcherListener;
class CompositorWidget;
class CompositorWidgetInitData;
}  // namespace widget
namespace wr {
class DisplayListBuilder;
class IpcResourceUpdateQueue;
enum class RenderRoot : uint8_t;
}  // namespace wr
}  // namespace mozilla

/**
 * Callback function that processes events.
 *
 * The argument is actually a subtype (subclass) of WidgetEvent which carries
 * platform specific information about the event. Platform specific code
 * knows how to deal with it.
 *
 * The return value determines whether or not the default action should take
 * place.
 */
typedef nsEventStatus (*EVENT_CALLBACK)(mozilla::WidgetGUIEvent* aEvent);

// Hide the native window system's real window type so as to avoid
// including native window system types and APIs. This is necessary
// to ensure cross-platform code.
typedef void* nsNativeWidget;

/**
 * Flags for the GetNativeData and SetNativeData functions
 */
#define NS_NATIVE_WINDOW 0
#define NS_NATIVE_GRAPHIC 1
#define NS_NATIVE_TMP_WINDOW 2
#define NS_NATIVE_WIDGET 3
#define NS_NATIVE_DISPLAY 4
#define NS_NATIVE_REGION 5
#define NS_NATIVE_OFFSETX 6
#define NS_NATIVE_OFFSETY 7
#define NS_NATIVE_PLUGIN_PORT 8
#define NS_NATIVE_SCREEN 9
// The toplevel GtkWidget containing this nsIWidget:
#define NS_NATIVE_SHELLWIDGET 10
// Has to match to NPNVnetscapeWindow, and shareable across processes
// HWND on Windows and XID on X11
#define NS_NATIVE_SHAREABLE_WINDOW 11
#define NS_NATIVE_OPENGL_CONTEXT 12
// See RegisterPluginWindowForRemoteUpdates
#define NS_NATIVE_PLUGIN_ID 13
// This is available only with GetNativeData() in parent process.  Anybody
// shouldn't access this pointer as a valid pointer since the result may be
// special value like NS_ONLY_ONE_NATIVE_IME_CONTEXT.  So, the result is just
// an identifier of distinguishing a text composition is caused by which native
// IME context.  Note that the result is only valid in the process.  So,
// XP code should use nsIWidget::GetNativeIMEContext() instead of using this.
#define NS_RAW_NATIVE_IME_CONTEXT 14
#define NS_NATIVE_WINDOW_WEBRTC_DEVICE_ID 15
#ifdef XP_MACOSX
#  define NS_NATIVE_PLUGIN_PORT_QD 100
#  define NS_NATIVE_PLUGIN_PORT_CG 101
#endif
#ifdef XP_WIN
#  define NS_NATIVE_TSF_THREAD_MGR 100
#  define NS_NATIVE_TSF_CATEGORY_MGR 101
#  define NS_NATIVE_TSF_DISPLAY_ATTR_MGR 102
#  define NS_NATIVE_ICOREWINDOW 103  // winrt specific
#  define NS_NATIVE_CHILD_WINDOW 104
#  define NS_NATIVE_CHILD_OF_SHAREABLE_WINDOW 105
#endif
#if defined(MOZ_WIDGET_GTK)
// set/get nsPluginNativeWindowGtk, e10s specific
#  define NS_NATIVE_PLUGIN_OBJECT_PTR 104
#  define NS_NATIVE_EGL_WINDOW 106
#endif
#ifdef MOZ_WIDGET_ANDROID
#  define NS_JAVA_SURFACE 100
#  define NS_PRESENTATION_WINDOW 101
#  define NS_PRESENTATION_SURFACE 102
#endif

#define MOZ_WIDGET_MAX_SIZE 16384

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_IWIDGET_IID                               \
  {                                                  \
    0x06396bf6, 0x2dd8, 0x45e5, {                    \
      0xac, 0x45, 0x75, 0x26, 0x53, 0xb1, 0xc9, 0x80 \
    }                                                \
  }

/**
 * Transparency modes
 */

enum nsTransparencyMode {
  eTransparencyOpaque = 0,   // Fully opaque
  eTransparencyTransparent,  // Parts of the window may be transparent
  eTransparencyGlass,  // Transparent parts of the window have Vista AeroGlass
                       // effect applied
  eTransparencyBorderlessGlass  // As above, but without a border around the
                                // opaque areas when there would otherwise be
                                // one with eTransparencyGlass
  // If you add to the end here, you must update the serialization code in
  // WidgetMessageUtils.h
};

/**
 * Cursor types.
 */

enum nsCursor {  ///(normal cursor,       usually rendered as an arrow)
  eCursor_standard,
  ///(system is busy,      usually rendered as a hourglass or watch)
  eCursor_wait,
  ///(Selecting something, usually rendered as an IBeam)
  eCursor_select,
  ///(can hyper-link,      usually rendered as a human hand)
  eCursor_hyperlink,
  ///(north/south/west/east edge sizing)
  eCursor_n_resize,
  eCursor_s_resize,
  eCursor_w_resize,
  eCursor_e_resize,
  ///(corner sizing)
  eCursor_nw_resize,
  eCursor_se_resize,
  eCursor_ne_resize,
  eCursor_sw_resize,
  eCursor_crosshair,
  eCursor_move,
  eCursor_help,
  eCursor_copy,  // CSS3
  eCursor_alias,
  eCursor_context_menu,
  eCursor_cell,
  eCursor_grab,
  eCursor_grabbing,
  eCursor_spinning,
  eCursor_zoom_in,
  eCursor_zoom_out,
  eCursor_not_allowed,
  eCursor_col_resize,
  eCursor_row_resize,
  eCursor_no_drop,
  eCursor_vertical_text,
  eCursor_all_scroll,
  eCursor_nesw_resize,
  eCursor_nwse_resize,
  eCursor_ns_resize,
  eCursor_ew_resize,
  eCursor_none,
  // This one is used for array sizing, and so better be the last
  // one in this list...
  eCursorCount,

  // ...except for this one.
  eCursorInvalid = eCursorCount + 1
};

enum nsTopLevelWidgetZPlacement {  // for PlaceBehind()
  eZPlacementBottom = 0,           // bottom of the window stack
  eZPlacementBelow,                // just below another widget
  eZPlacementTop                   // top of the window stack
};

/**
 * Before the OS goes to sleep, this topic is notified.
 */
#define NS_WIDGET_SLEEP_OBSERVER_TOPIC "sleep_notification"

/**
 * After the OS wakes up, this topic is notified.
 */
#define NS_WIDGET_WAKE_OBSERVER_TOPIC "wake_notification"

/**
 * Before the OS suspends the current process, this topic is notified.  Some
 * OS will kill processes that are suspended instead of resuming them.
 * For that reason this topic may be useful to safely close down resources.
 */
#define NS_WIDGET_SUSPEND_PROCESS_OBSERVER_TOPIC "suspend_process_notification"

/**
 * After the current process resumes from being suspended, this topic is
 * notified.
 */
#define NS_WIDGET_RESUME_PROCESS_OBSERVER_TOPIC "resume_process_notification"

/**
 * When an app(-shell) is activated by the OS, this topic is notified.
 * Currently, this only happens on Mac OSX.
 */
#define NS_WIDGET_MAC_APP_ACTIVATE_OBSERVER_TOPIC "mac_app_activate"

namespace mozilla {
namespace widget {

/**
 * Size constraints for setting the minimum and maximum size of a widget.
 * Values are in device pixels.
 */
struct SizeConstraints {
  SizeConstraints() : mMaxSize(MOZ_WIDGET_MAX_SIZE, MOZ_WIDGET_MAX_SIZE) {}

  SizeConstraints(mozilla::LayoutDeviceIntSize aMinSize,
                  mozilla::LayoutDeviceIntSize aMaxSize)
      : mMinSize(aMinSize), mMaxSize(aMaxSize) {
    if (mMaxSize.width > MOZ_WIDGET_MAX_SIZE) {
      mMaxSize.width = MOZ_WIDGET_MAX_SIZE;
    }
    if (mMaxSize.height > MOZ_WIDGET_MAX_SIZE) {
      mMaxSize.height = MOZ_WIDGET_MAX_SIZE;
    }
  }

  mozilla::LayoutDeviceIntSize mMinSize;
  mozilla::LayoutDeviceIntSize mMaxSize;
};

struct AutoObserverNotifier {
  AutoObserverNotifier(nsIObserver* aObserver, const char* aTopic)
      : mObserver(aObserver), mTopic(aTopic) {}

  void SkipNotification() { mObserver = nullptr; }

  uint64_t SaveObserver() {
    if (!mObserver) {
      return 0;
    }
    uint64_t observerId = ++sObserverId;
    sSavedObservers.InsertOrUpdate(observerId, mObserver);
    SkipNotification();
    return observerId;
  }

  ~AutoObserverNotifier() {
    if (mObserver) {
      mObserver->Observe(nullptr, mTopic, nullptr);
    }
  }

  static void NotifySavedObserver(const uint64_t& aObserverId,
                                  const char* aTopic) {
    nsCOMPtr<nsIObserver> observer = sSavedObservers.Get(aObserverId);
    if (!observer) {
      MOZ_ASSERT(aObserverId == 0,
                 "We should always find a saved observer for nonzero IDs");
      return;
    }

    sSavedObservers.Remove(aObserverId);
    observer->Observe(nullptr, aTopic, nullptr);
  }

 private:
  nsCOMPtr<nsIObserver> mObserver;
  const char* mTopic;

 private:
  static uint64_t sObserverId;
  static nsTHashMap<uint64_t, nsCOMPtr<nsIObserver>> sSavedObservers;
};

}  // namespace widget
}  // namespace mozilla

/**
 * The base class for all the widgets. It provides the interface for
 * all basic and necessary functionality.
 */
class nsIWidget : public nsISupports {
 protected:
  typedef mozilla::dom::BrowserChild BrowserChild;

 public:
  typedef mozilla::layers::CompositorBridgeChild CompositorBridgeChild;
  typedef mozilla::layers::AsyncDragMetrics AsyncDragMetrics;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::LayerManagerComposite LayerManagerComposite;
  typedef mozilla::layers::LayersBackend LayersBackend;
  typedef mozilla::layers::PLayerTransactionChild PLayerTransactionChild;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::ZoomConstraints ZoomConstraints;
  typedef mozilla::widget::IMEEnabled IMEEnabled;
  typedef mozilla::widget::IMEMessage IMEMessage;
  typedef mozilla::widget::IMENotification IMENotification;
  typedef mozilla::widget::IMENotificationRequests IMENotificationRequests;
  typedef mozilla::widget::IMEState IMEState;
  typedef mozilla::widget::InputContext InputContext;
  typedef mozilla::widget::InputContextAction InputContextAction;
  typedef mozilla::widget::NativeIMEContext NativeIMEContext;
  typedef mozilla::widget::SizeConstraints SizeConstraints;
  typedef mozilla::widget::TextEventDispatcher TextEventDispatcher;
  typedef mozilla::widget::TextEventDispatcherListener
      TextEventDispatcherListener;
  typedef mozilla::LayoutDeviceIntMargin LayoutDeviceIntMargin;
  typedef mozilla::LayoutDeviceIntPoint LayoutDeviceIntPoint;
  typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;
  typedef mozilla::LayoutDeviceIntRegion LayoutDeviceIntRegion;
  typedef mozilla::LayoutDeviceIntSize LayoutDeviceIntSize;
  typedef mozilla::ScreenIntPoint ScreenIntPoint;
  typedef mozilla::ScreenIntMargin ScreenIntMargin;
  typedef mozilla::ScreenIntSize ScreenIntSize;
  typedef mozilla::ScreenPoint ScreenPoint;
  typedef mozilla::CSSToScreenScale CSSToScreenScale;
  typedef mozilla::DesktopIntRect DesktopIntRect;
  typedef mozilla::DesktopPoint DesktopPoint;
  typedef mozilla::DesktopRect DesktopRect;
  typedef mozilla::DesktopSize DesktopSize;
  typedef mozilla::CSSPoint CSSPoint;
  typedef mozilla::CSSRect CSSRect;

  // Used in UpdateThemeGeometries.
  struct ThemeGeometry {
    // The ThemeGeometryType value for the themed widget, see
    // nsITheme::ThemeGeometryTypeForWidget.
    nsITheme::ThemeGeometryType mType;
    // The device-pixel rect within the window for the themed widget
    LayoutDeviceIntRect mRect;

    ThemeGeometry(nsITheme::ThemeGeometryType aType,
                  const LayoutDeviceIntRect& aRect)
        : mType(aType), mRect(aRect) {}
  };

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWIDGET_IID)

  nsIWidget()
      : mLastChild(nullptr),
        mPrevSibling(nullptr),
        mOnDestroyCalled(false),
        mWindowType(eWindowType_child),
        mZIndex(0)

  {
    ClearNativeTouchSequence(nullptr);
  }

  /**
   * Create and initialize a widget.
   *
   * All the arguments can be null in which case a top level window
   * with size 0 is created. The event callback function has to be
   * provided only if the caller wants to deal with the events this
   * widget receives.  The event callback is basically a preprocess
   * hook called synchronously. The return value determines whether
   * the event goes to the default window procedure or it is hidden
   * to the os. The assumption is that if the event handler returns
   * false the widget does not see the event. The widget should not
   * automatically clear the window to the background color. The
   * calling code must handle paint messages and clear the background
   * itself.
   *
   * In practice at least one of aParent and aNativeParent will be null. If
   * both are null the widget isn't parented (e.g. context menus or
   * independent top level windows).
   *
   * The dimensions given in aRect are specified in the parent's
   * device coordinate system.
   * This must not be called for parentless widgets such as top-level
   * windows, which use the desktop pixel coordinate system; a separate
   * method is provided for these.
   *
   * @param     aParent       parent nsIWidget
   * @param     aNativeParent native parent widget
   * @param     aRect         the widget dimension
   * @param     aInitData     data that is used for widget initialization
   *
   */
  [[nodiscard]] virtual nsresult Create(
      nsIWidget* aParent, nsNativeWidget aNativeParent,
      const LayoutDeviceIntRect& aRect,
      nsWidgetInitData* aInitData = nullptr) = 0;

  /*
   * As above, but with aRect specified in DesktopPixel units (for top-level
   * widgets).
   * Default implementation just converts aRect to device pixels and calls
   * through to device-pixel Create, but platforms may override this if the
   * mapping is not straightforward or the native platform needs to use the
   * desktop pixel values directly.
   */
  [[nodiscard]] virtual nsresult Create(nsIWidget* aParent,
                                        nsNativeWidget aNativeParent,
                                        const DesktopIntRect& aRect,
                                        nsWidgetInitData* aInitData = nullptr) {
    LayoutDeviceIntRect devPixRect =
        RoundedToInt(aRect * GetDesktopToDeviceScale());
    return Create(aParent, aNativeParent, devPixRect, aInitData);
  }

  /**
   * Allocate, initialize, and return a widget that is a child of
   * |this|.  The returned widget (if nonnull) has gone through the
   * equivalent of CreateInstance(widgetCID) + Create(...).
   *
   * |CreateChild()| lets widget backends decide whether to parent
   * the new child widget to this, nonnatively parent it, or both.
   * This interface exists to support the PuppetWidget backend,
   * which is entirely non-native.  All other params are the same as
   * for |Create()|.
   *
   * |aForceUseIWidgetParent| forces |CreateChild()| to only use the
   * |nsIWidget*| this, not its native widget (if it exists), when
   * calling |Create()|.  This is a timid hack around poorly
   * understood code, and shouldn't be used in new code.
   */
  virtual already_AddRefed<nsIWidget> CreateChild(
      const LayoutDeviceIntRect& aRect, nsWidgetInitData* aInitData = nullptr,
      bool aForceUseIWidgetParent = false) = 0;

  /**
   * Attach to a top level widget.
   *
   * In cases where a top level chrome widget is being used as a content
   * container, attach a secondary listener and update the device
   * context. The primary widget listener will continue to be called for
   * notifications relating to the top-level window, whereas other
   * notifications such as painting and events will instead be called via
   * the attached listener. SetAttachedWidgetListener should be used to
   * assign the attached listener.
   *
   * aUseAttachedEvents if true, events are sent to the attached listener
   * instead of the normal listener.
   */
  virtual void AttachViewToTopLevel(bool aUseAttachedEvents) = 0;

  /**
   * Accessor functions to get and set the attached listener. Used by
   * nsView in connection with AttachViewToTopLevel above.
   */
  virtual void SetAttachedWidgetListener(nsIWidgetListener* aListener) = 0;
  virtual nsIWidgetListener* GetAttachedWidgetListener() = 0;
  virtual void SetPreviouslyAttachedWidgetListener(
      nsIWidgetListener* aListener) = 0;
  virtual nsIWidgetListener* GetPreviouslyAttachedWidgetListener() = 0;

  /**
   * Accessor functions to get and set the listener which handles various
   * actions for the widget.
   */
  //@{
  virtual nsIWidgetListener* GetWidgetListener() = 0;
  virtual void SetWidgetListener(nsIWidgetListener* alistener) = 0;
  //@}

  /**
   * Close and destroy the internal native window.
   * This method does not delete the widget.
   */

  virtual void Destroy() = 0;

  /**
   * Destroyed() returns true if Destroy() has been called already.
   * Otherwise, false.
   */
  bool Destroyed() const { return mOnDestroyCalled; }

  /**
   * Reparent a widget
   *
   * Change the widget's parent. Null parents are allowed.
   *
   * @param     aNewParent   new parent
   */
  virtual void SetParent(nsIWidget* aNewParent) = 0;

  /**
   * Return the parent Widget of this Widget or nullptr if this is a
   * top level window
   *
   * @return the parent widget or nullptr if it does not have a parent
   *
   */
  virtual nsIWidget* GetParent(void) = 0;

  /**
   * Return the top level Widget of this Widget
   *
   * @return the top level widget
   */
  virtual nsIWidget* GetTopLevelWidget() = 0;

  /**
   * Return the top (non-sheet) parent of this Widget if it's a sheet,
   * or nullptr if this isn't a sheet (or some other error occurred).
   * Sheets are only supported on some platforms (currently only OS X).
   *
   * @return the top (non-sheet) parent widget or nullptr
   *
   */
  virtual nsIWidget* GetSheetWindowParent(void) = 0;

  /**
   * Return the physical DPI of the screen containing the window ...
   * the number of device pixels per inch.
   */
  virtual float GetDPI() = 0;

  /**
   * Return the scaling factor between device pixels and the platform-
   * dependent "desktop pixels" used to manage window positions on a
   * potentially multi-screen, mixed-resolution desktop.
   */
  virtual mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() = 0;

  /**
   * Return the scaling factor between device pixels and the platform-
   * dependent "desktop pixels" by looking up the screen by the position
   * of the widget.
   */
  virtual mozilla::DesktopToLayoutDeviceScale
  GetDesktopToDeviceScaleByScreen() = 0;

  /**
   * Return the default scale factor for the window. This is the
   * default number of device pixels per CSS pixel to use. This should
   * depend on OS/platform settings such as the Mac's "UI scale factor"
   * or Windows' "font DPI". This will take into account Gecko preferences
   * overriding the system setting.
   */
  mozilla::CSSToLayoutDeviceScale GetDefaultScale();

  /**
   * Return the first child of this widget.  Will return null if
   * there are no children.
   */
  nsIWidget* GetFirstChild() const { return mFirstChild; }

  /**
   * Return the last child of this widget.  Will return null if
   * there are no children.
   */
  nsIWidget* GetLastChild() const { return mLastChild; }

  /**
   * Return the next sibling of this widget
   */
  nsIWidget* GetNextSibling() const { return mNextSibling; }

  /**
   * Set the next sibling of this widget
   */
  void SetNextSibling(nsIWidget* aSibling) { mNextSibling = aSibling; }

  /**
   * Return the previous sibling of this widget
   */
  nsIWidget* GetPrevSibling() const { return mPrevSibling; }

  /**
   * Set the previous sibling of this widget
   */
  void SetPrevSibling(nsIWidget* aSibling) { mPrevSibling = aSibling; }

  /**
   * Show or hide this widget
   *
   * @param aState true to show the Widget, false to hide it
   *
   */
  virtual void Show(bool aState) = 0;

  /**
   * Whether or not a widget must be recreated after being hidden to show
   * again properly.
   */
  virtual bool NeedsRecreateToReshow() { return false; }

  /**
   * Make the window modal.
   */
  virtual void SetModal(bool aModal) = 0;

  /**
   * Make the non-modal window opened by modal window fake-modal, that will
   * call SetFakeModal(false) on destroy on Cocoa.
   */
  virtual void SetFakeModal(bool aModal) { SetModal(aModal); }

  /**
   * Are we app modal. Currently only implemented on Cocoa.
   */
  virtual bool IsRunningAppModal() { return false; }

  /**
   * The maximum number of simultaneous touch contacts supported by the device.
   * In the case of devices with multiple digitizers (e.g. multiple touch
   * screens), the value will be the maximum of the set of maximum supported
   * contacts by each individual digitizer.
   */
  virtual uint32_t GetMaxTouchPoints() const = 0;

  /**
   * Returns whether the window is visible
   *
   */
  virtual bool IsVisible() const = 0;

  /**
   * Perform platform-dependent sanity check on a potential window position.
   * This is guaranteed to work only for top-level windows.
   *
   * @param aAllowSlop: if true, allow the window to slop offscreen;
   *                    the window should be partially visible. if false,
   *                    force the entire window onscreen (or at least
   *                    the upper-left corner, if it's too large).
   * @param aX in: an x position expressed in screen coordinates.
   *           out: the x position constrained to fit on the screen(s).
   * @param aY in: an y position expressed in screen coordinates.
   *           out: the y position constrained to fit on the screen(s).
   *
   **/
  virtual void ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) = 0;

  /**
   * NOTE:
   *
   * For a top-level window widget, the "parent's coordinate system" is the
   * "global" display pixel coordinate space, *not* device pixels (which
   * may be inconsistent between multiple screens, at least in the Mac OS
   * case with mixed hi-dpi and lo-dpi displays). This applies to all the
   * following Move and Resize widget APIs.
   *
   * The display-/device-pixel distinction becomes important for (at least)
   * Mac OS X with Hi-DPI (retina) displays, and Windows when the UI scale
   * factor is set to other than 100%.
   *
   * The Move and Resize methods take floating-point parameters, rather than
   * integer ones. This is important when manipulating top-level widgets,
   * where the coordinate system may not be an integral multiple of the
   * device-pixel space.
   **/

  /**
   * Move this widget.
   *
   * Coordinates refer to the top-left of the widget.  For toplevel windows
   * with decorations, this is the top-left of the titlebar and frame .
   *
   * @param aX the new x position expressed in the parent's coordinate system
   * @param aY the new y position expressed in the parent's coordinate system
   *
   **/
  virtual void Move(double aX, double aY) = 0;

  /**
   * Reposition this widget so that the client area has the given offset.
   *
   * @param aOffset  the new offset of the client area expressed as an
   *                 offset from the origin of the client area of the parent
   *                 widget (for root widgets and popup widgets it is in
   *                 screen coordinates)
   **/
  virtual void MoveClient(const DesktopPoint& aOffset) = 0;

  /**
   * Resize this widget. Any size constraints set for the window by a
   * previous call to SetSizeConstraints will be applied.
   *
   * @param aWidth  the new width expressed in the parent's coordinate system
   * @param aHeight the new height expressed in the parent's coordinate
   *                system
   * @param aRepaint whether the widget should be repainted
   */
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) = 0;

  /**
   * Lock the aspect ratio of a Window
   *
   * @param aShouldLock bool
   *
   */
  virtual void LockAspectRatio(bool aShouldLock){};

  /**
   * Move or resize this widget. Any size constraints set for the window by
   * a previous call to SetSizeConstraints will be applied.
   *
   * @param aX       the new x position expressed in the parent's coordinate
   *                 system
   * @param aY       the new y position expressed in the parent's coordinate
   *                 system
   * @param aWidth   the new width expressed in the parent's coordinate system
   * @param aHeight  the new height expressed in the parent's coordinate
   *                 system
   * @param aRepaint whether the widget should be repainted if the size
   *                 changes
   *
   */
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) = 0;

  virtual mozilla::Maybe<bool> IsResizingNativeWidget() {
    return mozilla::Nothing();
  }

  /**
   * Resize the widget so that the inner client area has the given size.
   *
   * @param aSize    the new size of the client area.
   * @param aRepaint whether the widget should be repainted
   */
  virtual void ResizeClient(const DesktopSize& aSize, bool aRepaint) = 0;

  /**
   * Resize and reposition the widget so tht inner client area has the given
   * offset and size.
   *
   * @param aRect    the new offset and size of the client area. The offset is
   *                 expressed as an offset from the origin of the client area
   *                 of the parent widget (for root widgets and popup widgets it
   *                 is in screen coordinates).
   * @param aRepaint whether the widget should be repainted
   */
  virtual void ResizeClient(const DesktopRect& aRect, bool aRepaint) = 0;

  /**
   * Sets the widget's z-index.
   */
  virtual void SetZIndex(int32_t aZIndex) = 0;

  /**
   * Gets the widget's z-index.
   */
  int32_t GetZIndex() { return mZIndex; }

  /**
   * Position this widget just behind the given widget. (Used to
   * control z-order for top-level widgets. Get/SetZIndex by contrast
   * control z-order for child widgets of other widgets.)
   * @param aPlacement top, bottom, or below a widget
   *                   (if top or bottom, param aWidget is ignored)
   * @param aWidget    widget to place this widget behind
   *                   (only if aPlacement is eZPlacementBelow).
   *                   null is equivalent to aPlacement of eZPlacementTop
   * @param aActivate  true to activate the widget after placing it
   */
  virtual void PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget* aWidget, bool aActivate) = 0;

  /**
   * Minimize, maximize or normalize the window size.
   * Takes a value from nsSizeMode (see nsIWidgetListener.h)
   */
  virtual void SetSizeMode(nsSizeMode aMode) = 0;

  virtual void GetWorkspaceID(nsAString& workspaceID) = 0;

  virtual void MoveToWorkspace(const nsAString& workspaceID) = 0;

  /**
   * Suppress animations that are applied to a window by OS.
   */
  virtual void SuppressAnimation(bool aSuppress) {}

  /**
   * Return size mode (minimized, maximized, normalized).
   * Returns a value from nsSizeMode (see nsIWidgetListener.h)
   */
  virtual nsSizeMode SizeMode() = 0;

  /**
   * Ask whether the window is tiled.
   */
  virtual bool IsTiled() const = 0;

  /**
   * Ask wether the widget is fully occluded
   */
  virtual bool IsFullyOccluded() const = 0;

  /**
   * Enable or disable this Widget
   *
   * @param aState true to enable the Widget, false to disable it.
   */
  virtual void Enable(bool aState) = 0;

  /**
   * Ask whether the widget is enabled
   */
  virtual bool IsEnabled() const = 0;

  /*
   * Whether we should request activation of this widget's toplevel window.
   */
  enum class Raise {
    No,
    Yes,
  };

  /**
   * Request activation of this window or give focus to this widget.
   */
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) = 0;

  /**
   * Get this widget's outside dimensions relative to its parent widget. For
   * popup widgets the returned rect is in screen coordinates and not
   * relative to its parent widget.
   *
   * @return the x, y, width and height of this widget.
   */
  virtual LayoutDeviceIntRect GetBounds() = 0;

  /**
   * Get this widget's outside dimensions in device coordinates. This
   * includes any title bar on the window.
   *
   * @return the x, y, width and height of this widget.
   */
  virtual LayoutDeviceIntRect GetScreenBounds() = 0;

  /**
   * Similar to GetScreenBounds except that this function will always
   * get the size when the widget is in the nsSizeMode_Normal size mode
   * even if the current size mode is not nsSizeMode_Normal.
   * This method will fail if the size mode is not nsSizeMode_Normal and
   * the platform doesn't have the ability.
   * This method will always succeed if the current size mode is
   * nsSizeMode_Normal.
   *
   * @param aRect   On return it holds the  x, y, width and height of
   *                this widget.
   */
  [[nodiscard]] virtual nsresult GetRestoredBounds(
      LayoutDeviceIntRect& aRect) = 0;

  /**
   * Get this widget's client area bounds, if the window has a 3D border
   * appearance this returns the area inside the border. The position is the
   * position of the client area relative to the client area of the parent
   * widget (for root widgets and popup widgets it is in screen coordinates).
   *
   * @return the x, y, width and height of the client area of this widget.
   */
  virtual LayoutDeviceIntRect GetClientBounds() = 0;

  /**
   * Sets the non-client area dimensions of the window. Pass -1 to restore
   * the system default frame size for that border. Pass zero to remove
   * a border, or pass a specific value adjust a border. Units are in
   * pixels. (DPI dependent)
   *
   * Platform notes:
   *  Windows: shrinking top non-client height will remove application
   *  icon and window title text. Glass desktops will refuse to set
   *  dimensions between zero and size < system default.
   */
  virtual nsresult SetNonClientMargins(LayoutDeviceIntMargin& aMargins) = 0;

  /**
   * Get the client offset from the window origin.
   *
   * @return the x and y of the offset.
   */
  virtual LayoutDeviceIntPoint GetClientOffset() = 0;

  /**
   * Equivalent to GetClientBounds but only returns the size.
   */
  virtual LayoutDeviceIntSize GetClientSize() {
    // Depending on the backend, overloading this method may be useful if
    // requesting the client offset is expensive.
    return GetClientBounds().Size();
  }

  /**
   * Set the background color for this widget
   *
   * @param aColor the new background color
   *
   */

  virtual void SetBackgroundColor(const nscolor& aColor) {}

  /**
   * If a cursor type is currently cached locally for this widget, clear the
   * cached cursor to force an update on the next SetCursor call.
   */

  virtual void ClearCachedCursor() = 0;

  /**
   * Sets the cursor cursor for this widget.
   *
   * @param aDefaultCursor the default cursor to be set
   * @param aCursorImage a custom cursor, maybe null.
   * @param aX the X coordinate of the hotspot for aCursorImage (from left).
   * @param aY the Y coordinate of the hotspot for aCursorImage (from top).
   */
  virtual void SetCursor(nsCursor aDefaultCursor, imgIContainer* aCursorImage,
                         uint32_t aHotspotX, uint32_t aHotspotY) = 0;

  /**
   * Get the window type of this widget.
   */
  nsWindowType WindowType() { return mWindowType; }

  /**
   * Determines if this widget is one of the three types of plugin widgets.
   */
  bool IsPlugin() {
    return mWindowType == eWindowType_plugin ||
           mWindowType == eWindowType_plugin_ipc_chrome ||
           mWindowType == eWindowType_plugin_ipc_content;
  }

  /**
   * Set the transparency mode of the top-level window containing this widget.
   * So, e.g., if you call this on the widget for an IFRAME, the top level
   * browser window containing the IFRAME actually gets set. Be careful.
   *
   * This can fail if the platform doesn't support
   * transparency/glass. By default widgets are not
   * transparent.  This will also fail if the toplevel window is not
   * a Mozilla window, e.g., if the widget is in an embedded
   * context.
   *
   * After transparency/glass has been enabled, the initial alpha channel
   * value for all pixels is 1, i.e., opaque.
   * If the window is resized then the alpha channel values for
   * all pixels are reset to 1.
   * Pixel RGB color values are already premultiplied with alpha channel values.
   */
  virtual void SetTransparencyMode(nsTransparencyMode aMode) = 0;

  /**
   * Get the transparency mode of the top-level window that contains this
   * widget.
   */
  virtual nsTransparencyMode GetTransparencyMode() = 0;

  /**
   * This represents a command to set the bounds and clip region of
   * a child widget.
   */
  struct Configuration {
    nsCOMPtr<nsIWidget> mChild;
    uintptr_t mWindowID;  // e10s specific, the unique plugin port id
    bool mVisible;        // e10s specific, widget visibility
    LayoutDeviceIntRect mBounds;
    CopyableTArray<LayoutDeviceIntRect> mClipRegion;
  };

  /**
   * Sets the clip region of each mChild (which must actually be a child
   * of this widget) to the union of the pixel rects given in
   * mClipRegion, all relative to the top-left of the child
   * widget. Clip regions are not implemented on all platforms and only
   * need to actually work for children that are plugins.
   *
   * Also sets the bounds of each child to mBounds.
   *
   * This will invalidate areas of the children that have changed, but
   * does not need to invalidate any part of this widget.
   *
   * Children should be moved in the order given; the array is
   * sorted so to minimize unnecessary invalidation if children are
   * moved in that order.
   */
  virtual nsresult ConfigureChildren(
      const nsTArray<Configuration>& aConfigurations) = 0;
  virtual nsresult SetWindowClipRegion(
      const nsTArray<LayoutDeviceIntRect>& aRects,
      bool aIntersectWithExisting) = 0;

  /**
   * Appends to aRects the rectangles constituting this widget's clip
   * region. If this widget is not clipped, appends a single rectangle
   * (0, 0, bounds.width, bounds.height).
   */
  virtual void GetWindowClipRegion(nsTArray<LayoutDeviceIntRect>* aRects) = 0;

  /**
   * Register or unregister native plugin widgets which receive Configuration
   * data from the content process via the compositor.
   *
   * Lookups are used by the main thread via the compositor to lookup widgets
   * based on a unique window id. On Windows and Linux this is the
   * NS_NATIVE_PLUGIN_PORT (hwnd/XID). This tracking maintains a reference to
   * widgets held. Consumers are responsible for removing widgets from this
   * list.
   */
  virtual void RegisterPluginWindowForRemoteUpdates() = 0;
  virtual void UnregisterPluginWindowForRemoteUpdates() = 0;
  static nsIWidget* LookupRegisteredPluginWindow(uintptr_t aWindowID);

  /**
   * Iterates across the list of registered plugin widgets and updates thier
   * visibility based on which plugins are included in the 'visible' list.
   *
   * The compositor knows little about tabs, but it does know which plugin
   * widgets are currently included in the visible layer tree. It calls this
   * helper to hide widgets it knows nothing about.
   */
  static void UpdateRegisteredPluginWindowVisibility(
      uintptr_t aOwnerWidget, nsTArray<uintptr_t>& aPluginIds);

#if defined(XP_WIN)
  /**
   * Iterates over the list of registered plugins and for any that are owned
   * by aOwnerWidget and visible it takes a snapshot.
   *
   * @param aOwnerWidget only captures visible widgets owned by this
   */
  static void CaptureRegisteredPlugins(uintptr_t aOwnerWidget);

  /**
   * Take a scroll capture for this widget if possible.
   */
  virtual void UpdateScrollCapture() = 0;

  /**
   * Creates an async ImageContainer to hold scroll capture images that can be
   * used if the plugin is hidden during scroll.
   * @return the async container ID of the created ImageContainer.
   */
  virtual uint64_t CreateScrollCaptureContainer() = 0;
#endif

  /**
   * Set the shadow style of the window.
   *
   * Ignored on child widgets and on non-Mac platforms.
   */
  virtual void SetWindowShadowStyle(mozilla::StyleWindowShadow aStyle) = 0;

  /**
   * Set the opacity of the window.
   * Values need to be between 0.0f (invisible) and 1.0f (fully opaque).
   *
   * Ignored on child widgets and on non-Mac platforms.
   */
  virtual void SetWindowOpacity(float aOpacity) {}

  /**
   * Set the transform of the window. Values are in device pixels,
   * the origin is the top left corner of the window.
   *
   * Ignored on child widgets and on non-Mac platforms.
   */
  virtual void SetWindowTransform(const mozilla::gfx::Matrix& aTransform) {}

  /**
   * Set whether the window should ignore mouse events or not.
   *
   * This is only used on popup windows.
   */
  virtual void SetWindowMouseTransparent(bool aIsTransparent) {}

  /*
   * On Mac OS X, this method shows or hides the pill button in the titlebar
   * that's used to collapse the toolbar.
   *
   * Ignored on child widgets and on non-Mac platforms.
   */
  virtual void SetShowsToolbarButton(bool aShow) = 0;

  /*
   * On macOS, this method determines whether we tell cocoa that the window
   * supports native full screen. If we do so, and another window is in
   * native full screen, this window will also appear in native full screen.
   *
   * We generally only want to do this for primary application windows.
   *
   * Ignored on child widgets and on non-Mac platforms.
   */
  virtual void SetSupportsNativeFullscreen(bool aSupportsNativeFullscreen) = 0;

  enum WindowAnimationType {
    eGenericWindowAnimation,
    eDocumentWindowAnimation
  };

  /**
   * Sets the kind of top-level window animation this widget should have.  On
   * Mac OS X, this causes a particular kind of animation to be shown when the
   * window is first made visible.
   *
   * Ignored on child widgets and on non-Mac platforms.
   */
  virtual void SetWindowAnimationType(WindowAnimationType aType) = 0;

  /**
   * Specifies whether the window title should be drawn even if the window
   * contents extend into the titlebar. Ignored on windows that don't draw
   * in the titlebar. Only implemented on OS X.
   */
  virtual void SetDrawsTitle(bool aDrawTitle) {}

  /**
   * Hide window chrome (borders, buttons) for this widget.
   *
   */
  virtual void HideWindowChrome(bool aShouldHide) = 0;

  enum FullscreenTransitionStage {
    eBeforeFullscreenToggle,
    eAfterFullscreenToggle
  };

  /**
   * Prepares for fullscreen transition and returns whether the widget
   * supports fullscreen transition. If this method returns false,
   * PerformFullscreenTransition() must never be called. Otherwise,
   * caller should call that method twice with "before" and "after"
   * stages respectively in order. In the latter case, this method may
   * return some data via aData pointer. Caller must pass that data to
   * PerformFullscreenTransition() if any, and caller is responsible
   * for releasing that data.
   */
  virtual bool PrepareForFullscreenTransition(nsISupports** aData) = 0;

  /**
   * Performs fullscreen transition. This method returns immediately,
   * and will post aCallback to the main thread when the transition
   * finishes.
   */
  virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) = 0;

  /**
   * Perform any actions needed after the fullscreen transition has ended.
   */
  virtual void CleanupFullscreenTransition() = 0;

  /**
   * Return the screen the widget is in, or null if we don't know.
   */
  virtual already_AddRefed<nsIScreen> GetWidgetScreen() = 0;

  /**
   * Put the toplevel window into or out of fullscreen mode.
   * If aTargetScreen is given, attempt to go fullscreen on that screen,
   * if possible.  (If not, it behaves as if aTargetScreen is null.)
   * If !aFullScreen, aTargetScreen is ignored.
   * aTargetScreen support is currently only implemented on Windows.
   *
   * @return NS_OK if the widget is setup properly for fullscreen and
   * FullscreenChanged callback has been or will be called. If other
   * value is returned, the caller should continue the change itself.
   */
  virtual nsresult MakeFullScreen(bool aFullScreen,
                                  nsIScreen* aTargetScreen = nullptr) = 0;

  /**
   * Same as MakeFullScreen, except that, on systems which natively
   * support fullscreen transition, calling this method explicitly
   * requests that behavior.
   * It is currently only supported on OS X 10.7+.
   */
  virtual nsresult MakeFullScreenWithNativeTransition(
      bool aFullScreen, nsIScreen* aTargetScreen = nullptr) {
    return MakeFullScreen(aFullScreen, aTargetScreen);
  }

  /**
   * Invalidate a specified rect for a widget so that it will be repainted
   * later.
   */
  virtual void Invalidate(const LayoutDeviceIntRect& aRect) = 0;

  enum LayerManagerPersistence {
    LAYER_MANAGER_CURRENT = 0,
    LAYER_MANAGER_PERSISTENT
  };

  /**
   * Return the widget's LayerManager. The layer tree for that
   * LayerManager is what gets rendered to the widget.
   */
  inline LayerManager* GetLayerManager() {
    return GetLayerManager(nullptr, mozilla::layers::LayersBackend::LAYERS_NONE,
                           LAYER_MANAGER_CURRENT);
  }

  inline LayerManager* GetLayerManager(LayerManagerPersistence aPersistence) {
    return GetLayerManager(nullptr, mozilla::layers::LayersBackend::LAYERS_NONE,
                           aPersistence);
  }

  /**
   * Like GetLayerManager(), but prefers creating a layer manager of
   * type |aBackendHint| instead of what would normally be created.
   * LayersBackend::LAYERS_NONE means "no hint".
   */
  virtual LayerManager* GetLayerManager(
      PLayerTransactionChild* aShadowManager, LayersBackend aBackendHint,
      LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) = 0;

  /**
   * Called before each layer manager transaction to allow any preparation
   * for DrawWindowUnderlay/Overlay that needs to be on the main thread.
   *
   * Always called on the main thread.
   */
  virtual void PrepareWindowEffects() = 0;

  /**
   * Called on the main thread at the end of WebRender display list building.
   */
  virtual void AddWindowOverlayWebRenderCommands(
      mozilla::layers::WebRenderBridgeChild* aWrBridge,
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources) {}

  /**
   * Called when Gecko knows which themed widgets exist in this window.
   * The passed array contains an entry for every themed widget of the right
   * type (currently only StyleAppearance::Toolbar) within the window, except
   * for themed widgets which are transformed or have effects applied to them
   * (e.g. CSS opacity or filters).
   * This could sometimes be called during display list construction
   * outside of painting.
   * If called during painting, it will be called before we actually
   * paint anything.
   */
  virtual void UpdateThemeGeometries(
      const nsTArray<ThemeGeometry>& aThemeGeometries) = 0;

  /**
   * Informs the widget about the region of the window that is opaque.
   *
   * @param aOpaqueRegion the region of the window that is opaque.
   */
  virtual void UpdateOpaqueRegion(const LayoutDeviceIntRegion& aOpaqueRegion) {}

  /**
   * Informs the widget about the region of the window that is draggable.
   */
  virtual void UpdateWindowDraggingRegion(
      const LayoutDeviceIntRegion& aRegion) {}

  /**
   * Tells the widget whether the given input block results in a swipe.
   * Should be called in response to a WidgetWheelEvent that has
   * mFlags.mCanTriggerSwipe set on it.
   */
  virtual void ReportSwipeStarted(uint64_t aInputBlockId, bool aStartSwipe) {}

  /**
   * Internal methods
   */

  //@{
  virtual void AddChild(nsIWidget* aChild) = 0;
  virtual void RemoveChild(nsIWidget* aChild) = 0;
  virtual void* GetNativeData(uint32_t aDataType) = 0;
  virtual void SetNativeData(uint32_t aDataType, uintptr_t aVal) = 0;
  virtual void FreeNativeData(void* data, uint32_t aDataType) = 0;  //~~~

  //@}

  /**
   * Set the widget's title.
   * Must be called after Create.
   *
   * @param aTitle string displayed as the title of the widget
   */
  virtual nsresult SetTitle(const nsAString& aTitle) = 0;

  /**
   * Set the widget's icon.
   * Must be called after Create.
   *
   * @param aIconSpec string specifying the icon to use; convention is to
   *                  pass a resource: URL from which a platform-dependent
   *                  resource file name will be constructed
   */
  virtual void SetIcon(const nsAString& aIconSpec) = 0;

  /**
   * Return this widget's origin in screen coordinates.
   *
   * @return screen coordinates stored in the x,y members
   */
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() = 0;

  /**
   * The same as WidgetToScreenOffset(), except in the case of
   * PuppetWidget where this method omits the chrome offset.
   */
  virtual LayoutDeviceIntPoint TopLevelWidgetToScreenOffset() {
    return WidgetToScreenOffset();
  }

  /**
   * For a PuppetWidget, returns the transform from the coordinate
   * space of the PuppetWidget to the coordinate space of the
   * top-level native widget.
   *
   * Identity transform in other cases.
   */
  virtual mozilla::LayoutDeviceToLayoutDeviceMatrix4x4
  WidgetToTopLevelWidgetTransform() {
    return mozilla::LayoutDeviceToLayoutDeviceMatrix4x4();
  }

  /**
   * Given the specified client size, return the corresponding window size,
   * which includes the area for the borders and titlebar. This method
   * should work even when the window is not yet visible.
   */
  virtual LayoutDeviceIntSize ClientToWindowSize(
      const LayoutDeviceIntSize& aClientSize) = 0;

  /**
   * Dispatches an event to the widget
   */
  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* event,
                                 nsEventStatus& aStatus) = 0;

  /**
   * Dispatches an event to APZ only.
   * No-op in the child process.
   */
  virtual void DispatchEventToAPZOnly(mozilla::WidgetInputEvent* aEvent) = 0;

  /**
   * Dispatches an event that must be handled by APZ first, when APZ is
   * enabled. If invoked in the child process, it is forwarded to the
   * parent process synchronously.
   */
  virtual nsEventStatus DispatchInputEvent(
      mozilla::WidgetInputEvent* aEvent) = 0;

  /**
   * Confirm an APZ-aware event target. This should be used when APZ will
   * not need a layers update to process the event.
   */
  virtual void SetConfirmedTargetAPZC(
      uint64_t aInputBlockId,
      const nsTArray<ScrollableLayerGuid>& aTargets) const = 0;

  /**
   * Returns true if APZ is in use, false otherwise.
   */
  virtual bool AsyncPanZoomEnabled() const = 0;

  /**
   * Enables the dropping of files to a widget.
   */
  virtual void EnableDragDrop(bool aEnable) = 0;
  virtual nsresult AsyncEnableDragDrop(bool aEnable) = 0;

  /**
   * Enables/Disables system mouse capture.
   * @param aCapture true enables mouse capture, false disables mouse capture
   *
   */
  virtual void CaptureMouse(bool aCapture) = 0;

  /**
   * Classify the window for the window manager. Mostly for X11.
   */
  virtual void SetWindowClass(const nsAString& xulWinType) = 0;

  /**
   * Enables/Disables system capture of any and all events that would cause a
   * popup to be rolled up. aListener should be set to a non-null value for
   * any popups that are not managed by the popup manager.
   * @param aDoCapture true enables capture, false disables capture
   *
   */
  virtual void CaptureRollupEvents(nsIRollupListener* aListener,
                                   bool aDoCapture) = 0;

  /**
   * Bring this window to the user's attention.  This is intended to be a more
   * gentle notification than popping the window to the top or putting up an
   * alert.  See, for example, Win32 FlashWindow or the NotificationManager on
   * the Mac.  The notification should be suppressed if the window is already
   * in the foreground and should be dismissed when the user brings this window
   * to the foreground.
   * @param aCycleCount Maximum number of times to animate the window per system
   *                    conventions. If set to -1, cycles indefinitely until
   *                    window is brought into the foreground.
   */
  [[nodiscard]] virtual nsresult GetAttention(int32_t aCycleCount) = 0;

  /**
   * Ask whether there user input events pending.  All input events are
   * included, including those not targeted at this nsIwidget instance.
   */
  virtual bool HasPendingInputEvent() = 0;

  /**
   * If set to true, the window will draw its contents into the titlebar
   * instead of below it.
   *
   * Ignored on any platform that does not support it. Ignored by widgets that
   * do not represent windows.
   * May result in a resize event, so should only be called from places where
   * reflow and painting is allowed.
   *
   * @param aState Whether drawing into the titlebar should be activated.
   */
  virtual void SetDrawsInTitlebar(bool aState) = 0;

  /*
   * Determine whether the widget shows a resize widget. If it does,
   * aResizerRect returns the resizer's rect.
   *
   * Returns false on any platform that does not support it.
   *
   * @param aResizerRect The resizer's rect in device pixels.
   * @return Whether a resize widget is shown.
   */
  virtual bool ShowsResizeIndicator(LayoutDeviceIntRect* aResizerRect) = 0;

  /**
   * Begin a window resizing drag, based on the event passed in.
   */
  [[nodiscard]] virtual nsresult BeginResizeDrag(
      mozilla::WidgetGUIEvent* aEvent, int32_t aHorizontal,
      int32_t aVertical) = 0;

  // TODO: Make this an enum class with MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS or
  //       EnumSet class.
  enum Modifiers : uint32_t {
    NO_MODIFIERS = 0x00000000,
    CAPS_LOCK = 0x00000001,  // when CapsLock is active
    NUM_LOCK = 0x00000002,   // when NumLock is active
    SHIFT_L = 0x00000100,
    SHIFT_R = 0x00000200,
    CTRL_L = 0x00000400,
    CTRL_R = 0x00000800,
    ALT_L = 0x00001000,  // includes Option
    ALT_R = 0x00002000,
    COMMAND_L = 0x00004000,
    COMMAND_R = 0x00008000,
    HELP = 0x00010000,
    ALTGRAPH = 0x00020000,  // AltGr key on Windows.  This emulates
                            // AltRight key behavior of keyboard
                            // layouts which maps AltGr to AltRight
                            // key.
    FUNCTION = 0x00100000,
    NUMERIC_KEY_PAD = 0x01000000  // when the key is coming from the keypad
  };
  /**
   * Utility method intended for testing. Dispatches native key events
   * to this widget to simulate the press and release of a key.
   * @param aNativeKeyboardLayout a *platform-specific* constant.
   * On Mac, this is the resource ID for a 'uchr' or 'kchr' resource.
   * On Windows, it is converted to a hex string and passed to
   * LoadKeyboardLayout, see
   * http://msdn.microsoft.com/en-us/library/ms646305(VS.85).aspx
   * @param aNativeKeyCode a *platform-specific* keycode.
   * On Windows, this is the virtual key code.
   * @param aModifiers some combination of the above 'Modifiers' flags;
   * not all flags will apply to all platforms. Mac ignores the _R
   * modifiers. Windows ignores COMMAND, NUMERIC_KEY_PAD, HELP and
   * FUNCTION.
   * @param aCharacters characters that the OS would decide to generate
   * from the event. On Windows, this is the charCode passed by
   * WM_CHAR.
   * @param aUnmodifiedCharacters characters that the OS would decide
   * to generate from the event if modifier keys (other than shift)
   * were assumed inactive. Needed on Mac, ignored on Windows.
   * @param aObserver the observer that will get notified once the events
   * have been dispatched.
   * @return NS_ERROR_NOT_AVAILABLE to indicate that the keyboard
   * layout is not supported and the event was not fired
   */
  virtual nsresult SynthesizeNativeKeyEvent(
      int32_t aNativeKeyboardLayout, int32_t aNativeKeyCode,
      uint32_t aModifierFlags, const nsAString& aCharacters,
      const nsAString& aUnmodifiedCharacters, nsIObserver* aObserver) = 0;

  /**
   * Utility method intended for testing. Dispatches native mouse events
   * may even move the mouse cursor. On Mac the events are guaranteed to
   * be sent to the window containing this widget, but on Windows they'll go
   * to whatever's topmost on the screen at that position, so for
   * cross-platform testing ensure that your window is at the top of the
   * z-order.
   * @param aPoint screen location of the mouse, in device
   * pixels, with origin at the top left
   * @param aNativeMessage abstract native message.
   * @param aButton Mouse button defined by DOM UI Events.
   * @param aModifierFlags Some values of nsIWidget::Modifiers.
   *                       FYI: On Windows, Android and Headless widget on all
   *                       platroms, this hasn't been handled yet.
   * @param aObserver the observer that will get notified once the events
   * have been dispatched.
   */
  enum class NativeMouseMessage : uint32_t {
    ButtonDown,   // button down
    ButtonUp,     // button up
    Move,         // mouse cursor move
    EnterWindow,  // mouse cursor comes into a window
    LeaveWindow,  // mouse cursor leaves from a window
  };
  virtual nsresult SynthesizeNativeMouseEvent(
      LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
      mozilla::MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
      nsIObserver* aObserver) = 0;

  /**
   * A shortcut to SynthesizeNativeMouseEvent, abstracting away the native
   * message. aPoint is location in device pixels to which the mouse pointer
   * moves to.
   * @param aObserver the observer that will get notified once the events
   * have been dispatched.
   */
  virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) = 0;

  /**
   * Utility method intended for testing. Dispatching native mouse scroll
   * events may move the mouse cursor.
   *
   * @param aPoint            Mouse cursor position in screen coordinates.
   *                          In device pixels, the origin at the top left of
   *                          the primary display.
   * @param aNativeMessage    Platform native message.
   * @param aDeltaX           The delta value for X direction.  If the native
   *                          message doesn't indicate X direction scrolling,
   *                          this may be ignored.
   * @param aDeltaY           The delta value for Y direction.  If the native
   *                          message doesn't indicate Y direction scrolling,
   *                          this may be ignored.
   * @param aDeltaZ           The delta value for Z direction.  If the native
   *                          message doesn't indicate Z direction scrolling,
   *                          this may be ignored.
   * @param aModifierFlags    Must be values of Modifiers, or zero.
   * @param aAdditionalFlags  See nsIDOMWidnowUtils' consts and their
   *                          document.
   * @param aObserver         The observer that will get notified once the
   *                          events have been dispatched.
   */
  virtual nsresult SynthesizeNativeMouseScrollEvent(
      LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
      double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
      uint32_t aAdditionalFlags, nsIObserver* aObserver) = 0;

  /*
   * TouchPointerState states for SynthesizeNativeTouchPoint. Match
   * touch states in nsIDOMWindowUtils.idl.
   */
  enum TouchPointerState {
    // The pointer is in a hover state above the digitizer
    TOUCH_HOVER = (1 << 0),
    // The pointer is in contact with the digitizer
    TOUCH_CONTACT = (1 << 1),
    // The pointer has been removed from the digitizer detection area
    TOUCH_REMOVE = (1 << 2),
    // The pointer has been canceled. Will cancel any pending os level
    // gestures that would triggered as a result of completion of the
    // input sequence. This may not cancel moz platform related events
    // that might get tirggered by input already delivered.
    TOUCH_CANCEL = (1 << 3),

    // ALL_BITS used for validity checking during IPC serialization
    ALL_BITS = (1 << 4) - 1
  };
  /*
   * TouchpadPinchPhase states for SynthesizeNativeTouchPadPinch. Match
   * Phase states in nsIDOMWindowUtils.idl.
   */
  enum TouchpadPinchPhase { PHASE_BEGIN = 0, PHASE_UPDATE = 1, PHASE_END = 2 };
  /*
   * Create a new or update an existing touch pointer on the digitizer.
   * To trigger os level gestures, individual touch points should
   * transition through a complete set of touch states which should be
   * sent as individual messages.
   *
   * @param aPointerId The touch point id to create or update.
   * @param aPointerState one or more of the touch states listed above
   * @param aPoint coords of this event
   * @param aPressure 0.0 -> 1.0 float val indicating pressure
   * @param aOrientation 0 -> 359 degree value indicating the
   * orientation of the pointer. Use 90 for normal taps.
   * @param aObserver The observer that will get notified once the events
   * have been dispatched.
   */
  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) = 0;
  /*
   * See nsIDOMWindowUtils.sendNativeTouchpadPinch().
   */
  virtual nsresult SynthesizeNativeTouchPadPinch(TouchpadPinchPhase aEventPhase,
                                                 float aScale,
                                                 LayoutDeviceIntPoint aPoint,
                                                 int32_t aModifierFlags) = 0;

  /*
   * Helper for simulating a simple tap event with one touch point. When
   * aLongTap is true, simulates a native long tap with a duration equal to
   * ui.click_hold_context_menus.delay. This pref is compatible with the
   * apzc long tap duration. Defaults to 1.5 seconds.
   * @param aObserver The observer that will get notified once the events
   * have been dispatched.
   */
  virtual nsresult SynthesizeNativeTouchTap(LayoutDeviceIntPoint aPoint,
                                            bool aLongTap,
                                            nsIObserver* aObserver);

  virtual nsresult SynthesizeNativePenInput(
      uint32_t aPointerId, TouchPointerState aPointerState,
      LayoutDeviceIntPoint aPoint, double aPressure, uint32_t aRotation,
      int32_t aTiltX, int32_t aTiltY, nsIObserver* aObserver) = 0;

  /*
   * Cancels all active simulated touch input points and pending long taps.
   * Native widgets should track existing points such that they can clear the
   * digitizer state when this call is made.
   * @param aObserver The observer that will get notified once the touch
   * sequence has been cleared.
   */
  virtual nsresult ClearNativeTouchSequence(nsIObserver* aObserver);

  /*
   * Send a native event as if the user double tapped the touchpad with two
   * fingers.
   */
  virtual nsresult SynthesizeNativeTouchpadDoubleTap(
      LayoutDeviceIntPoint aPoint, uint32_t aModifierFlags) = 0;

  virtual void StartAsyncScrollbarDrag(
      const AsyncDragMetrics& aDragMetrics) = 0;

  /**
   * Notify APZ to start autoscrolling.
   * @param aAnchorLocation the location of the autoscroll anchor
   * @param aGuid identifies the scroll frame to be autoscrolled
   * @return true if APZ has been successfully notified
   */
  virtual bool StartAsyncAutoscroll(const ScreenPoint& aAnchorLocation,
                                    const ScrollableLayerGuid& aGuid) = 0;

  /**
   * Notify APZ to stop autoscrolling.
   * @param aGuid identifies the scroll frame which is being autoscrolled.
   */
  virtual void StopAsyncAutoscroll(const ScrollableLayerGuid& aGuid) = 0;

  // If this widget supports out-of-process compositing, it can override
  // this method to provide additional information to the compositor.
  virtual void GetCompositorWidgetInitData(
      mozilla::widget::CompositorWidgetInitData* aInitData) {}

  /**
   * Setter/Getter of the system font setting for testing.
   */
  virtual nsresult SetSystemFont(const nsCString& aFontName) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  virtual nsresult GetSystemFont(nsCString& aFontName) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Get rectangle of the screen where the window is placed.
  // It's used to detect popup overflow under Wayland because
  // Screenmanager does not work under it.
#ifdef MOZ_WAYLAND
  virtual nsresult GetScreenRect(LayoutDeviceIntRect* aRect) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  virtual nsRect GetPreferredPopupRect() {
    NS_WARNING("GetPreferredPopupRect implemented only for wayland");
    return nsRect(0, 0, 0, 0);
  }
  virtual void FlushPreferredPopupRect() {
    NS_WARNING("FlushPreferredPopupRect implemented only for wayland");
    return;
  }

#endif

  /*
   * Get safe area insets except to cutout.
   * See https://drafts.csswg.org/css-env-1/#safe-area-insets.
   */
  virtual mozilla::ScreenIntMargin GetSafeAreaInsets() const {
    return mozilla::ScreenIntMargin();
  }

 private:
  class LongTapInfo {
   public:
    LongTapInfo(int32_t aPointerId, LayoutDeviceIntPoint& aPoint,
                mozilla::TimeDuration aDuration, nsIObserver* aObserver)
        : mPointerId(aPointerId),
          mPosition(aPoint),
          mDuration(aDuration),
          mObserver(aObserver),
          mStamp(mozilla::TimeStamp::Now()) {}

    int32_t mPointerId;
    LayoutDeviceIntPoint mPosition;
    mozilla::TimeDuration mDuration;
    nsCOMPtr<nsIObserver> mObserver;
    mozilla::TimeStamp mStamp;
  };

  static void OnLongTapTimerCallback(nsITimer* aTimer, void* aClosure);

  static already_AddRefed<nsIBidiKeyboard> CreateBidiKeyboardContentProcess();
  static already_AddRefed<nsIBidiKeyboard> CreateBidiKeyboardInner();

  mozilla::UniquePtr<LongTapInfo> mLongTapTouchPoint;
  nsCOMPtr<nsITimer> mLongTapTimer;
  static int32_t sPointerIdCounter;

 public:
  /**
   * If key events have not been handled by content or XBL handlers, they can
   * be offered to the system (for custom application shortcuts set in system
   * preferences, for example).
   */
  virtual void PostHandleKeyEvent(mozilla::WidgetKeyboardEvent* aEvent);

  /**
   * Activates a native menu item at the position specified by the index
   * string. The index string is a string of positive integers separated
   * by the "|" (pipe) character. The last integer in the string represents
   * the item index in a submenu located using the integers preceding it.
   *
   * Example: 1|0|4
   * In this string, the first integer represents the top-level submenu
   * in the native menu bar. Since the integer is 1, it is the second submeu
   * in the native menu bar. Within that, the first item (index 0) is a
   * submenu, and we want to activate the 5th item within that submenu.
   */
  virtual nsresult ActivateNativeMenuItemAt(const nsAString& indexString) = 0;

  /**
   * This is used for native menu system testing.
   *
   * Updates a native menu at the position specified by the index string.
   * The index string is a string of positive integers separated by the "|"
   * (pipe) character.
   *
   * Example: 1|0|4
   * In this string, the first integer represents the top-level submenu
   * in the native menu bar. Since the integer is 1, it is the second submeu
   * in the native menu bar. Within that, the first item (index 0) is a
   * submenu, and we want to update submenu at index 4 within that submenu.
   *
   * If this is called with an empty string it forces a full reload of the
   * menu system.
   */
  virtual nsresult ForceUpdateNativeMenuAt(const nsAString& indexString) = 0;

  /**
   * This is used for testing macOS service menu code.
   *
   * @param aResult - the current text selection. Is empty if no selection.
   * @return nsresult - whether or not aResult was assigned the selected text.
   */
  [[nodiscard]] virtual nsresult GetSelectionAsPlaintext(nsAString& aResult) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  /**
   * Notify IME of the specified notification.
   *
   * @return If the notification is mouse button event and it's consumed by
   *         IME, this returns NS_SUCCESS_EVENT_CONSUMED.
   */
  virtual nsresult NotifyIME(const IMENotification& aIMENotification) = 0;

  /**
   * MaybeDispatchInitialFocusEvent will dispatch a focus event after creation
   * of the widget, in the event that we were not able to observe and respond to
   * the initial focus event. This is necessary for the early skeleton UI
   * window, which is displayed and receives its initial focus event before we
   * can actually respond to it.
   */
  virtual void MaybeDispatchInitialFocusEvent() {}

  /*
   * Notifies the input context changes.
   */
  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) = 0;

  /*
   * Get current input context.
   */
  virtual InputContext GetInputContext() = 0;

  /**
   * Get native IME context.  This is different from GetNativeData() with
   * NS_RAW_NATIVE_IME_CONTEXT, the result is unique even if in a remote
   * process.
   */
  virtual NativeIMEContext GetNativeIMEContext() = 0;

  /*
   * Given a WidgetKeyboardEvent, this method synthesizes a corresponding
   * native (OS-level) event for it. This method allows tests to simulate
   * keystrokes that trigger native key bindings (which require a native
   * event).
   */
  [[nodiscard]] virtual nsresult AttachNativeKeyEvent(
      mozilla::WidgetKeyboardEvent& aEvent) = 0;

  /**
   * Retrieve edit commands when the key combination of aEvent is used
   * in platform native applications.
   */
  enum NativeKeyBindingsType : uint8_t {
    NativeKeyBindingsForSingleLineEditor,
    NativeKeyBindingsForMultiLineEditor,
    NativeKeyBindingsForRichTextEditor
  };
  MOZ_CAN_RUN_SCRIPT virtual bool GetEditCommands(
      NativeKeyBindingsType aType, const mozilla::WidgetKeyboardEvent& aEvent,
      nsTArray<mozilla::CommandInt>& aCommands);

  /*
   * Retrieves a reference to notification requests of IME.  Note that the
   * reference is valid while the nsIWidget instance is alive.  So, if you
   * need to store the reference for a long time, you need to grab the widget
   * instance too.
   */
  const IMENotificationRequests& IMENotificationRequestsRef();

  /*
   * Call this method when a dialog is opened which has a default button.
   * The button's rectangle should be supplied in aButtonRect.
   */
  [[nodiscard]] virtual nsresult OnDefaultButtonLoaded(
      const LayoutDeviceIntRect& aButtonRect) = 0;

  /**
   * Return true if this process shouldn't use platform widgets, and
   * so should use PuppetWidgets instead.  If this returns true, the
   * result of creating and using a platform widget is undefined,
   * and likely to end in crashes or other buggy behavior.
   */
  static bool UsePuppetWidgets() { return XRE_IsContentProcess(); }

  static already_AddRefed<nsIWidget> CreateTopLevelWindow();

  static already_AddRefed<nsIWidget> CreateChildWindow();

  /**
   * Allocate and return a "puppet widget" that doesn't directly
   * correlate to a platform widget; platform events and data must
   * be fed to it.  Currently used in content processes.  NULL is
   * returned if puppet widgets aren't supported in this build
   * config, on this platform, or for this process type.
   *
   * This function is called "Create" to match CreateInstance().
   * The returned widget must still be nsIWidget::Create()d.
   */
  static already_AddRefed<nsIWidget> CreatePuppetWidget(
      BrowserChild* aBrowserChild);

  static already_AddRefed<nsIWidget> CreateHeadlessWidget();

  /**
   * Allocate and return a "plugin proxy widget", a subclass of PuppetWidget
   * used in wrapping a PPluginWidget connection for remote widgets. Note
   * this call creates the base object, it does not create the widget. Use
   * nsIWidget's Create to do this.
   */
  static already_AddRefed<nsIWidget> CreatePluginProxyWidget(
      BrowserChild* aBrowserChild, mozilla::plugins::PluginWidgetChild* aActor);

  /**
   * Reparent this widget's native widget.
   * @param aNewParent the native widget of aNewParent is the new native
   *                   parent widget
   */
  virtual void ReparentNativeWidget(nsIWidget* aNewParent) = 0;

  /**
   * Return true if widget has it's own GL context
   */
  virtual bool HasGLContext() { return false; }

  /**
   * Returns true to indicate that this widget paints an opaque background
   * that we want to be visible under the page, so layout should not force
   * a default background.
   */
  virtual bool WidgetPaintsBackground() { return false; }

  virtual bool NeedsPaint() { return IsVisible() && !GetBounds().IsEmpty(); }

  /**
   * Get the natural bounds of this widget.  This method is only
   * meaningful for widgets for which Gecko implements screen
   * rotation natively.  When this is the case, GetBounds() returns
   * the widget bounds taking rotation into account, and
   * GetNaturalBounds() returns the bounds *not* taking rotation
   * into account.
   *
   * No code outside of the composition pipeline should know or care
   * about this.  If you're not an agent of the compositor, you
   * probably shouldn't call this method.
   */
  virtual LayoutDeviceIntRect GetNaturalBounds() { return GetBounds(); }

  /**
   * Set size constraints on the window size such that it is never less than
   * the specified minimum size and never larger than the specified maximum
   * size. The size constraints are sizes of the outer rectangle including
   * the window frame and title bar. Use 0 for an unconstrained minimum size
   * and NS_MAXSIZE for an unconstrained maximum size. Note that this method
   * does not necessarily change the size of a window to conform to this size,
   * thus Resize should be called afterwards.
   *
   * @param aConstraints: the size constraints in device pixels
   */
  virtual void SetSizeConstraints(const SizeConstraints& aConstraints) = 0;

  /**
   * Return the size constraints currently observed by the widget.
   *
   * @return the constraints in device pixels
   */
  virtual const SizeConstraints GetSizeConstraints() = 0;

  /**
   * If this is owned by a BrowserChild, return that.  Otherwise return
   * null.
   */
  virtual BrowserChild* GetOwningBrowserChild() { return nullptr; }

  /**
   * If this isn't directly compositing to its window surface,
   * return the compositor which is doing that on our behalf.
   */
  virtual CompositorBridgeChild* GetRemoteRenderer() { return nullptr; }

  /**
   * Clear WebRender resources
   */
  virtual void ClearCachedWebrenderResources() {}

  /**
   * If this widget has its own vsync source, return it, otherwise return
   * nullptr. An example of such local source would be Wayland frame callbacks.
   */
  virtual RefPtr<mozilla::gfx::VsyncSource> GetVsyncSource() { return nullptr; }

  /**
   * Returns true if the widget requires synchronous repaints on resize,
   * false otherwise.
   */
  virtual bool SynchronouslyRepaintOnResize() { return true; }

  /**
   * Some platforms (only cocoa right now) round widget coordinates to the
   * nearest even pixels (see bug 892994), this function allows us to
   * determine how widget coordinates will be rounded.
   */
  virtual int32_t RoundsWidgetCoordinatesTo() { return 1; }

  virtual void UpdateZoomConstraints(
      const uint32_t& aPresShellId, const ScrollableLayerGuid::ViewID& aViewId,
      const mozilla::Maybe<ZoomConstraints>& aConstraints){};

  /**
   * GetTextEventDispatcher() returns TextEventDispatcher belonging to the
   * widget.  Note that this never returns nullptr.
   */
  virtual TextEventDispatcher* GetTextEventDispatcher() = 0;

  /**
   * GetNativeTextEventDispatcherListener() returns a
   * TextEventDispatcherListener instance which is used when the widget
   * instance handles native IME and/or keyboard events.
   */
  virtual TextEventDispatcherListener*
  GetNativeTextEventDispatcherListener() = 0;

  virtual void ZoomToRect(const uint32_t& aPresShellId,
                          const ScrollableLayerGuid::ViewID& aViewId,
                          const CSSRect& aRect, const uint32_t& aFlags) = 0;

  /**
   * LookUpDictionary shows the dictionary for the word around current point.
   *
   * @param aText            the word to look up dictiorary.
   * @param aFontRangeArray  text decoration of aText
   * @param aIsVertical      true if the word is vertical layout
   * @param aPoint           top-left point of aText
   */
  virtual void LookUpDictionary(
      const nsAString& aText,
      const nsTArray<mozilla::FontRange>& aFontRangeArray,
      const bool aIsVertical, const LayoutDeviceIntPoint& aPoint) {}

  virtual void RequestFxrOutput() {
    MOZ_ASSERT(false, "This function should only execute in Windows");
  }

#if defined(MOZ_WIDGET_ANDROID)
  /**
   * RecvToolbarAnimatorMessageFromCompositor receive message from compositor
   * thread.
   *
   * @param aMessage message being sent to Android UI thread.
   */
  virtual void RecvToolbarAnimatorMessageFromCompositor(int32_t aMessage) = 0;

  /**
   * UpdateRootFrameMetrics steady state frame metrics send from compositor
   * thread
   *
   * @param aScrollOffset  page scroll offset value in screen pixels.
   * @param aZoom          current page zoom.
   */
  virtual void UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset,
                                      const CSSToScreenScale& aZoom) = 0;

  /**
   * RecvScreenPixels Buffer containing the pixel from the frame buffer. Used
   * for android robocop tests.
   *
   * @param aMem  shared memory containing the frame buffer pixels.
   * @param aSize size of the buffer in screen pixels.
   */
  virtual void RecvScreenPixels(mozilla::ipc::Shmem&& aMem,
                                const ScreenIntSize& aSize,
                                bool aNeedsYFlip) = 0;

  virtual void UpdateDynamicToolbarMaxHeight(mozilla::ScreenIntCoord aHeight) {}
  virtual mozilla::ScreenIntCoord GetDynamicToolbarMaxHeight() const {
    return 0;
  }
#endif

  static already_AddRefed<nsIBidiKeyboard> CreateBidiKeyboard();

  /**
   * Like GetDefaultScale, but taking into account only the system settings
   * and ignoring Gecko preferences.
   */
  virtual double GetDefaultScaleInternal() { return 1.0; }

 protected:
  // keep the list of children.  We also keep track of our siblings.
  // The ownership model is as follows: parent holds a strong ref to
  // the first element of the list, and each element holds a strong
  // ref to the next element in the list.  The prevsibling and
  // lastchild pointers are weak, which is fine as long as they are
  // maintained properly.
  nsCOMPtr<nsIWidget> mFirstChild;
  nsIWidget* MOZ_NON_OWNING_REF mLastChild;
  nsCOMPtr<nsIWidget> mNextSibling;
  nsIWidget* MOZ_NON_OWNING_REF mPrevSibling;
  // When Destroy() is called, the sub class should set this true.
  bool mOnDestroyCalled;
  nsWindowType mWindowType;
  int32_t mZIndex;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIWidget, NS_IWIDGET_IID)

#endif  // nsIWidget_h__
