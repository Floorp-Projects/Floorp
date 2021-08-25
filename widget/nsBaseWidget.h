/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsBaseWidget_h__
#define nsBaseWidget_h__

#include "InputData.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/NativeLayer.h"
#include "mozilla/widget/ThemeChangeKind.h"
#include "nsRect.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIRollupListener.h"
#include "nsIObserver.h"
#include "nsIWidgetListener.h"
#include "nsPIDOMWindow.h"
#include "nsWeakReference.h"

#include <algorithm>

#if defined(XP_WIN)
// Scroll capture constants
const uint32_t kScrollCaptureFillColor = 0xFFa0a0a0;  // gray
const mozilla::gfx::SurfaceFormat kScrollCaptureFormat =
    mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32;
#endif

class nsIContent;
class gfxContext;

namespace mozilla {
class CompositorVsyncDispatcher;
class LiveResizeListener;
class FallbackRenderer;

#ifdef ACCESSIBILITY
namespace a11y {
class LocalAccessible;
}
#endif

namespace gfx {
class DrawTarget;
class SourceSurface;
}  // namespace gfx

namespace layers {
class BasicLayerManager;
class CompositorBridgeChild;
class CompositorBridgeParent;
class IAPZCTreeManager;
class GeckoContentController;
class APZEventState;
struct APZEventResult;
class CompositorSession;
class ImageContainer;
struct ScrollableLayerGuid;
class RemoteCompositorSession;
}  // namespace layers

namespace widget {
class CompositorWidgetDelegate;
class InProcessCompositorWidget;
class WidgetRenderingContext;
}  // namespace widget

class CompositorVsyncDispatcher;
}  // namespace mozilla

namespace base {
class Thread;
}  // namespace base

// Windows specific constant indicating the maximum number of touch points the
// inject api will allow. This also sets the maximum numerical value for touch
// ids we can use when injecting touch points on Windows.
#define TOUCH_INJECT_MAX_POINTS 256

class nsBaseWidget;

// Helper class used in shutting down gfx related code.
class WidgetShutdownObserver final : public nsIObserver {
  ~WidgetShutdownObserver();

 public:
  explicit WidgetShutdownObserver(nsBaseWidget* aWidget);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Register();
  void Unregister();

  nsBaseWidget* mWidget;
  bool mRegistered;
};

// Helper class used for observing locales change.
class LocalesChangedObserver final : public nsIObserver {
  ~LocalesChangedObserver();

 public:
  explicit LocalesChangedObserver(nsBaseWidget* aWidget);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Register();
  void Unregister();

  nsBaseWidget* mWidget;
  bool mRegistered;
};

/**
 * Common widget implementation used as base class for native
 * or crossplatform implementations of Widgets.
 * All cross-platform behavior that all widgets need to implement
 * should be placed in this class.
 * (Note: widget implementations are not required to use this
 * class, but it gives them a head start.)
 */

class nsBaseWidget : public nsIWidget, public nsSupportsWeakReference {
  template <class EventType, class InputType>
  friend class DispatchEventOnMainThread;
  friend class mozilla::widget::InProcessCompositorWidget;
  friend class mozilla::layers::RemoteCompositorSession;

 protected:
  typedef base::Thread Thread;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::layers::BasicLayerManager BasicLayerManager;
  typedef mozilla::layers::BufferMode BufferMode;
  typedef mozilla::layers::CompositorBridgeChild CompositorBridgeChild;
  typedef mozilla::layers::CompositorBridgeParent CompositorBridgeParent;
  typedef mozilla::layers::IAPZCTreeManager IAPZCTreeManager;
  typedef mozilla::layers::GeckoContentController GeckoContentController;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::APZEventState APZEventState;
  typedef mozilla::layers::SetAllowedTouchBehaviorCallback
      SetAllowedTouchBehaviorCallback;
  typedef mozilla::CSSIntRect CSSIntRect;
  typedef mozilla::CSSRect CSSRect;
  typedef mozilla::ScreenRotation ScreenRotation;
  typedef mozilla::widget::CompositorWidgetDelegate CompositorWidgetDelegate;
  typedef mozilla::layers::CompositorSession CompositorSession;
  typedef mozilla::layers::ImageContainer ImageContainer;

  virtual ~nsBaseWidget();

 public:
  nsBaseWidget();

  NS_DECL_THREADSAFE_ISUPPORTS

  // nsIWidget interface
  void CaptureMouse(bool aCapture) override {}
  void CaptureRollupEvents(nsIRollupListener* aListener,
                           bool aDoCapture) override {}
  nsIWidgetListener* GetWidgetListener() override;
  void SetWidgetListener(nsIWidgetListener* alistener) override;
  void Destroy() override;
  void SetParent(nsIWidget* aNewParent) override{};
  nsIWidget* GetParent() override;
  nsIWidget* GetTopLevelWidget() override;
  nsIWidget* GetSheetWindowParent(void) override;
  float GetDPI() override;
  void AddChild(nsIWidget* aChild) override;
  void RemoveChild(nsIWidget* aChild) override;

  void SetZIndex(int32_t aZIndex) override;
  void PlaceBehind(nsTopLevelWidgetZPlacement aPlacement, nsIWidget* aWidget,
                   bool aActivate) override {}

  void SetSizeMode(nsSizeMode aMode) override;
  nsSizeMode SizeMode() override { return mSizeMode; }
  void GetWorkspaceID(nsAString& workspaceID) override;
  void MoveToWorkspace(const nsAString& workspaceID) override;
  bool IsTiled() const override { return mIsTiled; }

  bool IsFullyOccluded() const override { return mIsFullyOccluded; }

  void SetCursor(const Cursor&) override;
  void ClearCachedCursor() final {
    mCursor = {};
    mUpdateCursor = true;
  }
  void SetTransparencyMode(nsTransparencyMode aMode) override;
  nsTransparencyMode GetTransparencyMode() override;
  void GetWindowClipRegion(nsTArray<LayoutDeviceIntRect>* aRects) override;
  void SetWindowShadowStyle(mozilla::StyleWindowShadow aStyle) override {}
  void SetShowsToolbarButton(bool aShow) override {}
  void SetSupportsNativeFullscreen(bool aSupportsNativeFullscreen) override {}
  void SetWindowAnimationType(WindowAnimationType aType) override {}
  void HideWindowChrome(bool aShouldHide) override {}
  bool PrepareForFullscreenTransition(nsISupports** aData) override {
    return false;
  }
  void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                   uint16_t aDuration, nsISupports* aData,
                                   nsIRunnable* aCallback) override;
  void CleanupFullscreenTransition() override {}
  already_AddRefed<nsIScreen> GetWidgetScreen() override;
  nsresult MakeFullScreen(bool aFullScreen,
                          nsIScreen* aScreen = nullptr) override;
  void InfallibleMakeFullScreen(bool aFullScreen, nsIScreen* aScreen = nullptr);

  WindowRenderer* GetWindowRenderer() override;

  // A remote compositor session tied to this window has been lost and IPC
  // messages will no longer work. The widget must clean up any lingering
  // resources and possibly schedule another paint.
  //
  // A reference to the session object is held until this function has
  // returned.
  void NotifyCompositorSessionLost(
      mozilla::layers::CompositorSession* aSession);

  already_AddRefed<mozilla::CompositorVsyncDispatcher>
  GetCompositorVsyncDispatcher();
  virtual void CreateCompositorVsyncDispatcher();
  virtual void CreateCompositor();
  virtual void CreateCompositor(int aWidth, int aHeight);
  virtual void SetCompositorWidgetDelegate(CompositorWidgetDelegate*) {}
  void PrepareWindowEffects() override {}
  void UpdateThemeGeometries(
      const nsTArray<ThemeGeometry>& aThemeGeometries) override {}
  void SetModal(bool aModal) override {}
  uint32_t GetMaxTouchPoints() const override;
  void SetWindowClass(const nsAString& xulWinType) override {}
  nsresult SetWindowClipRegion(const nsTArray<LayoutDeviceIntRect>& aRects,
                               bool aIntersectWithExisting) override;
  // Return whether this widget interprets parameters to Move and Resize APIs
  // as "desktop pixels" rather than "device pixels", and therefore
  // applies its GetDefaultScale() value to them before using them as mBounds
  // etc (which are always stored in device pixels).
  // Note that APIs that -get- the widget's position/size/bounds, rather than
  // -setting- them (i.e. moving or resizing the widget) will always return
  // values in the widget's device pixels.
  bool BoundsUseDesktopPixels() const {
    return mWindowType <= eWindowType_popup;
  }
  // Default implementation, to be overridden by platforms where desktop coords
  // are virtualized and may not correspond to device pixels on the screen.
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() override {
    return mozilla::DesktopToLayoutDeviceScale(1.0);
  }
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScaleByScreen()
      override;

  void ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) override {}
  void MoveClient(const DesktopPoint& aOffset) override;
  void ResizeClient(const DesktopSize& aSize, bool aRepaint) override;
  void ResizeClient(const DesktopRect& aRect, bool aRepaint) override;
  LayoutDeviceIntRect GetBounds() override;
  LayoutDeviceIntRect GetClientBounds() override;
  LayoutDeviceIntRect GetScreenBounds() override;
  [[nodiscard]] nsresult GetRestoredBounds(LayoutDeviceIntRect& aRect) override;
  nsresult SetNonClientMargins(LayoutDeviceIntMargin& aMargins) override;
  LayoutDeviceIntPoint GetClientOffset() override;
  void EnableDragDrop(bool aEnable) override{};
  nsresult AsyncEnableDragDrop(bool aEnable) override;
  [[nodiscard]] nsresult GetAttention(int32_t aCycleCount) override {
    return NS_OK;
  }
  bool HasPendingInputEvent() override;
  void SetIcon(const nsAString& aIconSpec) override {}
  void SetDrawsInTitlebar(bool aState) override {}
  bool ShowsResizeIndicator(LayoutDeviceIntRect* aResizerRect) override;
  void FreeNativeData(void* data, uint32_t aDataType) override {}
  [[nodiscard]] nsresult BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                         int32_t aHorizontal,
                                         int32_t aVertical) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult ActivateNativeMenuItemAt(const nsAString& indexString) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult ForceUpdateNativeMenuAt(const nsAString& indexString) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult NotifyIME(const IMENotification& aIMENotification) final;
  [[nodiscard]] nsresult AttachNativeKeyEvent(
      mozilla::WidgetKeyboardEvent& aEvent) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  bool ComputeShouldAccelerate();
  virtual bool WidgetTypeSupportsAcceleration() { return true; }
  [[nodiscard]] nsresult OnDefaultButtonLoaded(
      const LayoutDeviceIntRect& aButtonRect) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  already_AddRefed<nsIWidget> CreateChild(
      const LayoutDeviceIntRect& aRect, nsWidgetInitData* aInitData = nullptr,
      bool aForceUseIWidgetParent = false) override;
  void AttachViewToTopLevel(bool aUseAttachedEvents) override;
  nsIWidgetListener* GetAttachedWidgetListener() override;
  void SetAttachedWidgetListener(nsIWidgetListener* aListener) override;
  nsIWidgetListener* GetPreviouslyAttachedWidgetListener() override;
  void SetPreviouslyAttachedWidgetListener(nsIWidgetListener*) override;
  NativeIMEContext GetNativeIMEContext() override;
  TextEventDispatcher* GetTextEventDispatcher() final;
  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
  void ZoomToRect(const uint32_t& aPresShellId,
                  const ScrollableLayerGuid::ViewID& aViewId,
                  const CSSRect& aRect, const uint32_t& aFlags) override;

  // Dispatch an event that must be first be routed through APZ.
  ContentAndAPZEventStatus DispatchInputEvent(
      mozilla::WidgetInputEvent* aEvent) override;
  void DispatchEventToAPZOnly(mozilla::WidgetInputEvent* aEvent) override;

  void SetConfirmedTargetAPZC(
      uint64_t aInputBlockId,
      const nsTArray<ScrollableLayerGuid>& aTargets) const override;

  void UpdateZoomConstraints(
      const uint32_t& aPresShellId, const ScrollableLayerGuid::ViewID& aViewId,
      const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

  bool AsyncPanZoomEnabled() const override;

  void NotifyWindowDestroyed();
  void NotifySizeMoveDone();
  void NotifyWindowMoved(int32_t aX, int32_t aY);

  // Register plugin windows for remote updates from the compositor
  void RegisterPluginWindowForRemoteUpdates() override;
  void UnregisterPluginWindowForRemoteUpdates() override;

  void SetNativeData(uint32_t aDataType, uintptr_t aVal) override {}

  // Should be called by derived implementations to notify on system color and
  // theme changes.
  void NotifyThemeChanged(mozilla::widget::ThemeChangeKind);
  void NotifyUIStateChanged(UIStateChangeType aShowFocusRings);

#ifdef ACCESSIBILITY
  // Get the accessible for the window.
  mozilla::a11y::LocalAccessible* GetRootAccessible();
#endif

  // Return true if this is a simple widget (that is typically not worth
  // accelerating)
  bool IsSmallPopup() const;

  nsPopupLevel PopupLevel() { return mPopupLevel; }

  LayoutDeviceIntSize ClientToWindowSize(
      const LayoutDeviceIntSize& aClientSize) override {
    return aClientSize;
  }

  // return true if this is a popup widget with a native titlebar
  bool IsPopupWithTitleBar() const {
    return (mWindowType == eWindowType_popup &&
            mBorderStyle != eBorderStyle_default &&
            mBorderStyle & eBorderStyle_title);
  }

  void ReparentNativeWidget(nsIWidget* aNewParent) override {}

  const SizeConstraints GetSizeConstraints() override;
  void SetSizeConstraints(const SizeConstraints& aConstraints) override;

  void StartAsyncScrollbarDrag(const AsyncDragMetrics& aDragMetrics) override;

  bool StartAsyncAutoscroll(const ScreenPoint& aAnchorLocation,
                            const ScrollableLayerGuid& aGuid) override;

  void StopAsyncAutoscroll(const ScrollableLayerGuid& aGuid) override;

  mozilla::layers::LayersId GetRootLayerTreeId() override;

  /**
   * Use this when GetLayerManager() returns a BasicLayerManager
   * (nsBaseWidget::GetLayerManager() does). This sets up the widget's
   * layer manager to temporarily render into aTarget.
   *
   * |aNaturalWidgetBounds| is the un-rotated bounds of |aWidget|.
   * |aRotation| is the "virtual rotation" to apply when rendering to
   * the target.  When |aRotation| is ROTATION_0,
   * |aNaturalWidgetBounds| is not used.
   */
  class AutoLayerManagerSetup {
   public:
    AutoLayerManagerSetup(nsBaseWidget* aWidget, gfxContext* aTarget,
                          BufferMode aDoubleBuffering);
    ~AutoLayerManagerSetup();

   private:
    nsBaseWidget* mWidget;
    RefPtr<BasicLayerManager> mLayerManager;
    mozilla::FallbackRenderer* mRenderer = nullptr;
  };
  friend class AutoLayerManagerSetup;

  virtual bool ShouldUseOffMainThreadCompositing();

  static nsIRollupListener* GetActiveRollupListener();

  void Shutdown();

  void QuitIME();

#if defined(XP_WIN)
  uint64_t CreateScrollCaptureContainer() override;
#endif

  // These functions should be called at the start and end of a "live" widget
  // resize (i.e. when the window contents are repainting during the resize,
  // such as when the user drags a window border). It will suppress the
  // displayport during the live resize to avoid unneccessary overpainting.
  void NotifyLiveResizeStarted();
  void NotifyLiveResizeStopped();

#if defined(MOZ_WIDGET_ANDROID)
  void RecvToolbarAnimatorMessageFromCompositor(int32_t) override{};
  void UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset,
                              const CSSToScreenScale& aZoom) override{};
  void RecvScreenPixels(mozilla::ipc::Shmem&& aMem, const ScreenIntSize& aSize,
                        bool aNeedsYFlip) override{};
#endif

  virtual void LocalesChanged() {}

 protected:
  // These are methods for CompositorWidgetWrapper, and should only be
  // accessed from that class. Derived widgets can choose which methods to
  // implement, or none if supporting out-of-process compositing.
  virtual bool PreRender(mozilla::widget::WidgetRenderingContext* aContext) {
    return true;
  }
  virtual void PostRender(mozilla::widget::WidgetRenderingContext* aContext) {}
  virtual RefPtr<mozilla::layers::NativeLayerRoot> GetNativeLayerRoot() {
    return nullptr;
  }
  virtual already_AddRefed<DrawTarget> StartRemoteDrawing();
  virtual already_AddRefed<DrawTarget> StartRemoteDrawingInRegion(
      const LayoutDeviceIntRegion& aInvalidRegion, BufferMode* aBufferMode) {
    return StartRemoteDrawing();
  }
  virtual void EndRemoteDrawing() {}
  virtual void EndRemoteDrawingInRegion(
      DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
    EndRemoteDrawing();
  }
  virtual void CleanupRemoteDrawing() {}
  virtual void CleanupWindowEffects() {}
  virtual bool InitCompositor(mozilla::layers::Compositor* aCompositor) {
    return true;
  }
  virtual uint32_t GetGLFrameBufferFormat();
  virtual bool CompositorInitiallyPaused() { return false; }

 protected:
  void ResolveIconName(const nsAString& aIconName, const nsAString& aIconSuffix,
                       nsIFile** aResult);
  virtual void OnDestroy();
  void BaseCreate(nsIWidget* aParent, nsWidgetInitData* aInitData);

  virtual void ConfigureAPZCTreeManager();
  virtual void ConfigureAPZControllerThread();
  virtual already_AddRefed<GeckoContentController>
  CreateRootContentController();

  // Dispatch an event that has already been routed through APZ.
  nsEventStatus ProcessUntransformedAPZEvent(
      mozilla::WidgetInputEvent* aEvent,
      const mozilla::layers::APZEventResult& aApzResult);

  const LayoutDeviceIntRegion RegionFromArray(
      const nsTArray<LayoutDeviceIntRect>& aRects);
  void ArrayFromRegion(const LayoutDeviceIntRegion& aRegion,
                       nsTArray<LayoutDeviceIntRect>& aRects);

  nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                    int32_t aNativeKeyCode,
                                    uint32_t aModifierFlags,
                                    const nsAString& aCharacters,
                                    const nsAString& aUnmodifiedCharacters,
                                    nsIObserver* aObserver) override {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "keyevent");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                      NativeMouseMessage aNativeMessage,
                                      mozilla::MouseButton aButton,
                                      nsIWidget::Modifiers aModifierFlags,
                                      nsIObserver* aObserver) override {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                     nsIObserver* aObserver) override {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativeMouseScrollEvent(
      LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
      double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
      uint32_t aAdditionalFlags, nsIObserver* aObserver) override {
    mozilla::widget::AutoObserverNotifier notifier(aObserver,
                                                   "mousescrollevent");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                      TouchPointerState aPointerState,
                                      LayoutDeviceIntPoint aPoint,
                                      double aPointerPressure,
                                      uint32_t aPointerOrientation,
                                      nsIObserver* aObserver) override {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "touchpoint");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativeTouchPadPinch(TouchpadGesturePhase aEventPhase,
                                         float aScale,
                                         LayoutDeviceIntPoint aPoint,
                                         int32_t aModifierFlags) override {
    MOZ_RELEASE_ASSERT(
        false, "This method is not implemented on the current platform");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativePenInput(uint32_t aPointerId,
                                    TouchPointerState aPointerState,
                                    LayoutDeviceIntPoint aPoint,
                                    double aPressure, uint32_t aRotation,
                                    int32_t aTiltX, int32_t aTiltY,
                                    nsIObserver* aObserver) override {
    MOZ_RELEASE_ASSERT(
        false, "This method is not implemented on the current platform");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativeTouchpadDoubleTap(LayoutDeviceIntPoint aPoint,
                                             uint32_t aModifierFlags) override {
    MOZ_RELEASE_ASSERT(
        false, "This method is not implemented on the current platform");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult SynthesizeNativeTouchpadPan(TouchpadGesturePhase aEventPhase,
                                       LayoutDeviceIntPoint aPoint,
                                       double aDeltaX, double aDeltaY,
                                       int32_t aModifierFlags) override {
    MOZ_RELEASE_ASSERT(
        false, "This method is not implemented on the current platform");
    return NS_ERROR_UNEXPECTED;
  }

  /**
   * GetPseudoIMEContext() returns pseudo IME context when TextEventDispatcher
   * has non-native input transaction.  Otherwise, returns nullptr.
   */
  void* GetPseudoIMEContext();

 protected:
  // Utility to check if an array of clip rects is equal to our
  // internally stored clip rect array mClipRects.
  bool IsWindowClipRegionEqual(const nsTArray<LayoutDeviceIntRect>& aRects);

  // Stores the clip rectangles in aRects into mClipRects.
  void StoreWindowClipRegion(const nsTArray<LayoutDeviceIntRect>& aRects);

  virtual already_AddRefed<nsIWidget> AllocateChildPopupWidget() {
    return nsIWidget::CreateChildWindow();
  }

  WindowRenderer* CreateBasicLayerManager();

  nsPopupType PopupType() const { return mPopupType; }

  bool HasRemoteContent() const { return mHasRemoteContent; }

  void NotifyRollupGeometryChange() {
    // XULPopupManager isn't interested in this notification, so only
    // send it if gRollupListener is set.
    if (gRollupListener) {
      gRollupListener->NotifyGeometryChange();
    }
  }

  /**
   * Apply the current size constraints to the given size.
   *
   * @param aWidth width to constrain
   * @param aHeight height to constrain
   */
  void ConstrainSize(int32_t* aWidth, int32_t* aHeight) override {
    SizeConstraints c = GetSizeConstraints();
    *aWidth = std::max(c.mMinSize.width, std::min(c.mMaxSize.width, *aWidth));
    *aHeight =
        std::max(c.mMinSize.height, std::min(c.mMaxSize.height, *aHeight));
  }

  CompositorBridgeChild* GetRemoteRenderer() override;

  void ClearCachedWebrenderResources() override;

  /**
   * Notify the widget that this window is being used with OMTC.
   */
  virtual void WindowUsesOMTC() {}
  virtual void RegisterTouchWindow() {}

  mozilla::dom::Document* GetDocument() const;

  void EnsureTextEventDispatcher();

  // Notify the compositor that a device reset has occurred.
  void OnRenderingDeviceReset();

  bool UseAPZ();

  bool AllowWebRenderForThisWindow();

  /**
   * For widgets that support synthesizing native touch events, this function
   * can be used to manage the current state of synthetic pointers. Each widget
   * must maintain its own MultiTouchInput instance and pass it in as the state,
   * along with the desired parameters for the changes. This function returns
   * a new MultiTouchInput object that is ready to be dispatched.
   */
  mozilla::MultiTouchInput UpdateSynthesizedTouchState(
      mozilla::MultiTouchInput* aState, uint32_t aTime,
      mozilla::TimeStamp aTimeStamp, uint32_t aPointerId,
      TouchPointerState aPointerState, LayoutDeviceIntPoint aPoint,
      double aPointerPressure, uint32_t aPointerOrientation);

  /**
   * Dispatch the given MultiTouchInput through APZ to Gecko (if APZ is enabled)
   * or directly to gecko (if APZ is not enabled). This function must only
   * be called from the main thread, and if APZ is enabled, that must also be
   * the APZ controller thread.
   */
  void DispatchTouchInput(
      mozilla::MultiTouchInput& aInput,
      uint16_t aInputSource =
          mozilla::dom::MouseEvent_Binding::MOZ_SOURCE_TOUCH);

  /**
   * Dispatch the given PanGestureInput through APZ to Gecko (if APZ is enabled)
   * or directly to gecko (if APZ is not enabled). This function must only
   * be called from the main thread, and if APZ is enabled, that must also be
   * the APZ controller thread.
   */
  void DispatchPanGestureInput(mozilla::PanGestureInput& aInput);
  void DispatchPinchGestureInput(mozilla::PinchGestureInput& aInput);

#if defined(XP_WIN)
  void UpdateScrollCapture() override;

  /**
   * To be overridden by derived classes to return a snapshot that can be used
   * during scrolling. Returning null means we won't update the container.
   * @return an already AddRefed SourceSurface containing the snapshot
   */
  virtual already_AddRefed<SourceSurface> CreateScrollSnapshot() {
    return nullptr;
  };

  /**
   * Used by derived classes to create a fallback scroll image.
   * @param aSnapshotDrawTarget DrawTarget to fill with fallback image.
   */
  void DefaultFillScrollCapture(DrawTarget* aSnapshotDrawTarget);

  RefPtr<ImageContainer> mScrollCaptureContainer;
#endif

 protected:
  // Returns whether compositing should use an external surface size.
  virtual bool UseExternalCompositingSurface() const { return false; }

  /**
   * Starts the OMTC compositor destruction sequence.
   *
   * When this function returns, the compositor should not be
   * able to access the opengl context anymore.
   * It is safe to call it several times if platform implementations
   * require the compositor to be destroyed before ~nsBaseWidget is
   * reached (This is the case with gtk2 for instance).
   */
  virtual void DestroyCompositor();
  void DestroyLayerManager();
  void ReleaseContentController();
  void RevokeTransactionIdAllocator();

  void FreeShutdownObserver();
  void FreeLocalesChangedObserver();

  nsIWidgetListener* mWidgetListener;
  nsIWidgetListener* mAttachedWidgetListener;
  nsIWidgetListener* mPreviouslyAttachedWidgetListener;
  RefPtr<WindowRenderer> mWindowRenderer;
  RefPtr<CompositorSession> mCompositorSession;
  RefPtr<CompositorBridgeChild> mCompositorBridgeChild;

  mozilla::UniquePtr<mozilla::Mutex> mCompositorVsyncDispatcherLock;
  RefPtr<mozilla::CompositorVsyncDispatcher> mCompositorVsyncDispatcher;

  RefPtr<IAPZCTreeManager> mAPZC;
  RefPtr<GeckoContentController> mRootContentController;
  RefPtr<APZEventState> mAPZEventState;
  SetAllowedTouchBehaviorCallback mSetAllowedTouchBehaviorCallback;
  RefPtr<WidgetShutdownObserver> mShutdownObserver;
  RefPtr<LocalesChangedObserver> mLocalesChangedObserver;
  RefPtr<TextEventDispatcher> mTextEventDispatcher;
  Cursor mCursor;
  nsBorderStyle mBorderStyle;
  LayoutDeviceIntRect mBounds;
  LayoutDeviceIntRect* mOriginalBounds;
  // When this pointer is null, the widget is not clipped
  mozilla::UniquePtr<LayoutDeviceIntRect[]> mClipRects;
  uint32_t mClipRectCount;
  nsSizeMode mSizeMode;
  bool mIsTiled;
  nsPopupLevel mPopupLevel;
  nsPopupType mPopupType;
  SizeConstraints mSizeConstraints;
  bool mHasRemoteContent;
  bool mFissionWindow;

  bool mUpdateCursor;
  bool mUseAttachedEvents;
  bool mIMEHasFocus;
  bool mIMEHasQuit;
  bool mIsFullyOccluded;
  static nsIRollupListener* gRollupListener;

  struct InitialZoomConstraints {
    InitialZoomConstraints(const uint32_t& aPresShellID,
                           const ScrollableLayerGuid::ViewID& aViewID,
                           const ZoomConstraints& aConstraints)
        : mPresShellID(aPresShellID),
          mViewID(aViewID),
          mConstraints(aConstraints) {}

    uint32_t mPresShellID;
    ScrollableLayerGuid::ViewID mViewID;
    ZoomConstraints mConstraints;
  };

  mozilla::Maybe<InitialZoomConstraints> mInitialZoomConstraints;

  // This points to the resize listeners who have been notified that a live
  // resize is in progress. This should always be empty when a live-resize is
  // not in progress.
  nsTArray<RefPtr<mozilla::LiveResizeListener>> mLiveResizeListeners;

#ifdef DEBUG
 protected:
  static nsAutoString debug_GuiEventToString(
      mozilla::WidgetGUIEvent* aGuiEvent);
  static bool debug_WantPaintFlashing();

  static void debug_DumpInvalidate(FILE* aFileOut, nsIWidget* aWidget,
                                   const LayoutDeviceIntRect* aRect,
                                   const char* aWidgetName, int32_t aWindowID);

  static void debug_DumpEvent(FILE* aFileOut, nsIWidget* aWidget,
                              mozilla::WidgetGUIEvent* aGuiEvent,
                              const char* aWidgetName, int32_t aWindowID);

  static void debug_DumpPaintEvent(FILE* aFileOut, nsIWidget* aWidget,
                                   const nsIntRegion& aPaintEvent,
                                   const char* aWidgetName, int32_t aWindowID);

  static bool debug_GetCachedBoolPref(const char* aPrefName);
#endif

 private:
  already_AddRefed<LayerManager> CreateCompositorSession(
      int aWidth, int aHeight, mozilla::layers::CompositorOptions* aOptionsOut);
};

#endif  // nsBaseWidget_h__
