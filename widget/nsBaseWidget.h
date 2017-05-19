/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsBaseWidget_h__
#define nsBaseWidget_h__

#include "InputData.h"
#include "mozilla/EventForwards.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/layers/APZCCallbackHelper.h"
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
const uint32_t kScrollCaptureFillColor = 0xFFa0a0a0; // gray
const mozilla::gfx::SurfaceFormat kScrollCaptureFormat =
  mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32;
#endif

class nsIContent;
class gfxContext;

namespace mozilla {
class CompositorVsyncDispatcher;
class LiveResizeListener;

#ifdef ACCESSIBILITY
namespace a11y {
class Accessible;
}
#endif

namespace gfx {
class DrawTarget;
class SourceSurface;
} // namespace gfx

namespace layers {
class BasicLayerManager;
class CompositorBridgeChild;
class CompositorBridgeParent;
class IAPZCTreeManager;
class GeckoContentController;
class APZEventState;
class CompositorSession;
class ImageContainer;
struct ScrollableLayerGuid;
class RemoteCompositorSession;
} // namespace layers

namespace widget {
class CompositorWidgetDelegate;
class InProcessCompositorWidget;
class WidgetRenderingContext;
} // namespace widget

class CompositorVsyncDispatcher;
} // namespace mozilla

namespace base {
class Thread;
} // namespace base

// Windows specific constant indicating the maximum number of touch points the
// inject api will allow. This also sets the maximum numerical value for touch
// ids we can use when injecting touch points on Windows.
#define TOUCH_INJECT_MAX_POINTS 256

class nsBaseWidget;

// Helper class used in shutting down gfx related code.
class WidgetShutdownObserver final : public nsIObserver
{
  ~WidgetShutdownObserver();

public:
  explicit WidgetShutdownObserver(nsBaseWidget* aWidget);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Register();
  void Unregister();

  nsBaseWidget *mWidget;
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

class nsBaseWidget : public nsIWidget, public nsSupportsWeakReference
{
  friend class DispatchWheelEventOnMainThread;
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
  typedef mozilla::layers::SetAllowedTouchBehaviorCallback SetAllowedTouchBehaviorCallback;
  typedef mozilla::CSSIntRect CSSIntRect;
  typedef mozilla::CSSRect CSSRect;
  typedef mozilla::ScreenRotation ScreenRotation;
  typedef mozilla::widget::CompositorWidgetDelegate CompositorWidgetDelegate;
  typedef mozilla::layers::CompositorSession CompositorSession;
  typedef mozilla::layers::ImageContainer ImageContainer;

  virtual ~nsBaseWidget();

public:
  nsBaseWidget();

  NS_DECL_ISUPPORTS

  // nsIWidget interface
  virtual void            CaptureMouse(bool aCapture) override {}
  virtual void            CaptureRollupEvents(nsIRollupListener* aListener,
                                              bool aDoCapture) override {}
  virtual nsIWidgetListener*  GetWidgetListener() override;
  virtual void            SetWidgetListener(nsIWidgetListener* alistener) override;
  virtual void            Destroy() override;
  virtual void            SetParent(nsIWidget* aNewParent) override {};
  virtual nsIWidget*      GetParent(void) override;
  virtual nsIWidget*      GetTopLevelWidget() override;
  virtual nsIWidget*      GetSheetWindowParent(void) override;
  virtual float           GetDPI() override;
  virtual void            AddChild(nsIWidget* aChild) override;
  virtual void            RemoveChild(nsIWidget* aChild) override;

  void                    SetZIndex(int32_t aZIndex) override;
  virtual void            PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                      nsIWidget *aWidget, bool aActivate)
                                      override {}

  virtual void            SetSizeMode(nsSizeMode aMode) override;
  virtual nsSizeMode      SizeMode() override
  {
    return mSizeMode;
  }

  virtual nsCursor        GetCursor() override;
  virtual void            SetCursor(nsCursor aCursor) override;
  virtual nsresult        SetCursor(imgIContainer* aCursor,
                                    uint32_t aHotspotX, uint32_t aHotspotY) override;
  virtual void            ClearCachedCursor() override { mUpdateCursor = true; }
  virtual void            SetTransparencyMode(nsTransparencyMode aMode) override;
  virtual nsTransparencyMode GetTransparencyMode() override;
  virtual void            GetWindowClipRegion(nsTArray<LayoutDeviceIntRect>* aRects) override;
  virtual void            SetWindowShadowStyle(int32_t aStyle) override {}
  virtual void            SetShowsToolbarButton(bool aShow) override {}
  virtual void            SetShowsFullScreenButton(bool aShow) override {}
  virtual void            SetWindowAnimationType(WindowAnimationType aType) override {}
  virtual void            HideWindowChrome(bool aShouldHide) override {}
  virtual bool PrepareForFullscreenTransition(nsISupports** aData) override { return false; }
  virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) override;
  virtual already_AddRefed<nsIScreen> GetWidgetScreen() override;
  virtual nsresult        MakeFullScreen(bool aFullScreen,
                                         nsIScreen* aScreen = nullptr) override;
  void                    InfallibleMakeFullScreen(bool aFullScreen,
                                                   nsIScreen* aScreen = nullptr);

  virtual LayerManager*   GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  // A remote compositor session tied to this window has been lost and IPC
  // messages will no longer work. The widget must clean up any lingering
  // resources and possibly schedule another paint.
  //
  // A reference to the session object is held until this function has
  // returned.
  void NotifyRemoteCompositorSessionLost(mozilla::layers::CompositorSession* aSession);

  mozilla::CompositorVsyncDispatcher* GetCompositorVsyncDispatcher();
  void            CreateCompositorVsyncDispatcher();
  virtual void            CreateCompositor();
  virtual void            CreateCompositor(int aWidth, int aHeight);
  virtual void            PrepareWindowEffects() override {}
  virtual void            UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) override {}
  virtual void            SetModal(bool aModal) override {}
  virtual uint32_t        GetMaxTouchPoints() const override;
  virtual void            SetWindowClass(const nsAString& xulWinType)
                            override {}
  virtual nsresult        SetWindowClipRegion(const nsTArray<LayoutDeviceIntRect>& aRects, bool aIntersectWithExisting) override;
  // Return whether this widget interprets parameters to Move and Resize APIs
  // as "desktop pixels" rather than "device pixels", and therefore
  // applies its GetDefaultScale() value to them before using them as mBounds
  // etc (which are always stored in device pixels).
  // Note that APIs that -get- the widget's position/size/bounds, rather than
  // -setting- them (i.e. moving or resizing the widget) will always return
  // values in the widget's device pixels.
  bool                    BoundsUseDesktopPixels() const {
    return mWindowType <= eWindowType_popup;
  }
  // Default implementation, to be overridden by platforms where desktop coords
  // are virtualized and may not correspond to device pixels on the screen.
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() override {
    return mozilla::DesktopToLayoutDeviceScale(1.0);
  }
  virtual void            ConstrainPosition(bool aAllowSlop,
                                            int32_t *aX,
                                            int32_t *aY) override {}
  virtual void            MoveClient(double aX, double aY) override;
  virtual void            ResizeClient(double aWidth, double aHeight, bool aRepaint) override;
  virtual void            ResizeClient(double aX, double aY, double aWidth, double aHeight, bool aRepaint) override;
  virtual LayoutDeviceIntRect GetBounds() override;
  virtual LayoutDeviceIntRect GetClientBounds() override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  virtual MOZ_MUST_USE nsresult GetRestoredBounds(LayoutDeviceIntRect& aRect) override;
  virtual nsresult        SetNonClientMargins(LayoutDeviceIntMargin& aMargins) override;
  virtual LayoutDeviceIntPoint GetClientOffset() override;
  virtual void            EnableDragDrop(bool aEnable) override {};
  virtual MOZ_MUST_USE nsresult
                          GetAttention(int32_t aCycleCount) override
                          { return NS_OK; }
  virtual bool            HasPendingInputEvent() override;
  virtual void            SetIcon(const nsAString &aIconSpec) override {}
  virtual void            SetWindowTitlebarColor(nscolor aColor, bool aActive)
                            override {}
  virtual void            SetDrawsInTitlebar(bool aState) override {}
  virtual bool            ShowsResizeIndicator(LayoutDeviceIntRect* aResizerRect) override;
  virtual void            FreeNativeData(void * data, uint32_t aDataType) override {}
  virtual MOZ_MUST_USE nsresult
                          BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                          int32_t aHorizontal,
                                          int32_t aVertical) override
                          { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual MOZ_MUST_USE nsresult
                          BeginMoveDrag(mozilla::WidgetMouseEvent* aEvent) override
                          { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult        ActivateNativeMenuItemAt(const nsAString& indexString) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult        ForceUpdateNativeMenuAt(const nsAString& indexString) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult        NotifyIME(const IMENotification& aIMENotification)
                            override final;
  virtual MOZ_MUST_USE nsresult
                          StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                                         int32_t aPanelX, int32_t aPanelY,
                                         nsString& aCommitted) override
                          { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual void            SetPluginFocused(bool& aFocused) override {}
  virtual void            SetCandidateWindowForPlugin(
                            const mozilla::widget::CandidateWindowPosition&
                              aPosition) override
                          { }
  virtual void            DefaultProcOfPluginEvent(
                            const mozilla::WidgetPluginEvent& aEvent) override
                          { }
  virtual MOZ_MUST_USE nsresult AttachNativeKeyEvent(mozilla::WidgetKeyboardEvent& aEvent) override { return NS_ERROR_NOT_IMPLEMENTED; }
  bool                    ComputeShouldAccelerate();
  virtual bool            WidgetTypeSupportsAcceleration() { return true; }
  virtual MOZ_MUST_USE nsresult OnDefaultButtonLoaded(const LayoutDeviceIntRect& aButtonRect) override { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual already_AddRefed<nsIWidget>
  CreateChild(const LayoutDeviceIntRect& aRect,
              nsWidgetInitData* aInitData = nullptr,
              bool aForceUseIWidgetParent = false) override;
  virtual void            AttachViewToTopLevel(bool aUseAttachedEvents) override;
  virtual nsIWidgetListener* GetAttachedWidgetListener() override;
  virtual void               SetAttachedWidgetListener(nsIWidgetListener* aListener) override;
  virtual nsIWidgetListener* GetPreviouslyAttachedWidgetListener() override;
  virtual void               SetPreviouslyAttachedWidgetListener(nsIWidgetListener* aListener) override;
  virtual TextEventDispatcher* GetTextEventDispatcher() override final;
  virtual TextEventDispatcherListener*
    GetNativeTextEventDispatcherListener() override;
  virtual void ZoomToRect(const uint32_t& aPresShellId,
                          const FrameMetrics::ViewID& aViewId,
                          const CSSRect& aRect,
                          const uint32_t& aFlags) override;
  // Dispatch an event that must be first be routed through APZ.
  nsEventStatus DispatchInputEvent(mozilla::WidgetInputEvent* aEvent) override;
  void DispatchEventToAPZOnly(mozilla::WidgetInputEvent* aEvent) override;

  void SetConfirmedTargetAPZC(uint64_t aInputBlockId,
                              const nsTArray<ScrollableLayerGuid>& aTargets) const override;

  void UpdateZoomConstraints(const uint32_t& aPresShellId,
                             const FrameMetrics::ViewID& aViewId,
                             const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

  bool AsyncPanZoomEnabled() const override;

  void NotifyWindowDestroyed();
  void NotifySizeMoveDone();
  void NotifyWindowMoved(int32_t aX, int32_t aY);

  // Register plugin windows for remote updates from the compositor
  virtual void RegisterPluginWindowForRemoteUpdates() override;
  virtual void UnregisterPluginWindowForRemoteUpdates() override;

  virtual void SetNativeData(uint32_t aDataType, uintptr_t aVal) override {};

  // Should be called by derived implementations to notify on system color and
  // theme changes.
  void NotifySysColorChanged();
  void NotifyThemeChanged();
  void NotifyUIStateChanged(UIStateChangeType aShowAccelerators,
                            UIStateChangeType aShowFocusRings);

#ifdef ACCESSIBILITY
  // Get the accessible for the window.
  mozilla::a11y::Accessible* GetRootAccessible();
#endif

  // Return true if this is a simple widget (that is typically not worth
  // accelerating)
  bool IsSmallPopup() const;

  nsPopupLevel PopupLevel() { return mPopupLevel; }

  virtual LayoutDeviceIntSize
  ClientToWindowSize(const LayoutDeviceIntSize& aClientSize) override
  {
    return aClientSize;
  }

  // return true if this is a popup widget with a native titlebar
  bool IsPopupWithTitleBar() const
  {
    return (mWindowType == eWindowType_popup &&
            mBorderStyle != eBorderStyle_default &&
            mBorderStyle & eBorderStyle_title);
  }

  virtual void ReparentNativeWidget(nsIWidget* aNewParent) override {}

  virtual const SizeConstraints GetSizeConstraints() override;
  virtual void SetSizeConstraints(const SizeConstraints& aConstraints) override;

  virtual void StartAsyncScrollbarDrag(const AsyncDragMetrics& aDragMetrics) override;

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
                          BufferMode aDoubleBuffering,
                          ScreenRotation aRotation = mozilla::ROTATION_0);
    ~AutoLayerManagerSetup();
  private:
    nsBaseWidget* mWidget;
    RefPtr<BasicLayerManager> mLayerManager;
  };
  friend class AutoLayerManagerSetup;

  virtual bool            ShouldUseOffMainThreadCompositing();

  static nsIRollupListener* GetActiveRollupListener();

  void Shutdown();

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
  void RecvToolbarAnimatorMessageFromCompositor(int32_t) override {};
  void UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset, const CSSToScreenScale& aZoom, const CSSRect& aPage) override {};
  void RecvScreenPixels(mozilla::ipc::Shmem&& aMem, const ScreenIntSize& aSize) override {};
#endif

protected:
  // These are methods for CompositorWidgetWrapper, and should only be
  // accessed from that class. Derived widgets can choose which methods to
  // implement, or none if supporting out-of-process compositing.
  virtual bool PreRender(mozilla::widget::WidgetRenderingContext* aContext) {
    return true;
  }
  virtual void PostRender(mozilla::widget::WidgetRenderingContext* aContext)
  {}
  virtual void DrawWindowUnderlay(mozilla::widget::WidgetRenderingContext* aContext,
                                  LayoutDeviceIntRect aRect)
  {}
  virtual void DrawWindowOverlay(mozilla::widget::WidgetRenderingContext* aContext,
                                 LayoutDeviceIntRect aRect)
  {}
  virtual already_AddRefed<DrawTarget> StartRemoteDrawing();
  virtual already_AddRefed<DrawTarget>
  StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion, BufferMode* aBufferMode)
  {
    return StartRemoteDrawing();
  }
  virtual void EndRemoteDrawing()
  {}
  virtual void EndRemoteDrawingInRegion(DrawTarget* aDrawTarget,
                                        LayoutDeviceIntRegion& aInvalidRegion)
  {
    EndRemoteDrawing();
  }
  virtual void CleanupRemoteDrawing()
  {}
  virtual void CleanupWindowEffects()
  {}
  virtual bool InitCompositor(mozilla::layers::Compositor* aCompositor) {
    return true;
  }
  virtual uint32_t GetGLFrameBufferFormat();

protected:
  void            ResolveIconName(const nsAString &aIconName,
                                  const nsAString &aIconSuffix,
                                  nsIFile **aResult);
  virtual void    OnDestroy();
  void            BaseCreate(nsIWidget *aParent,
                             nsWidgetInitData* aInitData);

  virtual void ConfigureAPZCTreeManager();
  virtual void ConfigureAPZControllerThread();
  virtual already_AddRefed<GeckoContentController> CreateRootContentController();

  // Dispatch an event that has already been routed through APZ.
  nsEventStatus ProcessUntransformedAPZEvent(mozilla::WidgetInputEvent* aEvent,
                                             const ScrollableLayerGuid& aGuid,
                                             uint64_t aInputBlockId,
                                             nsEventStatus aApzResponse);

  const LayoutDeviceIntRegion RegionFromArray(const nsTArray<LayoutDeviceIntRect>& aRects);
  void ArrayFromRegion(const LayoutDeviceIntRegion& aRegion,
                       nsTArray<LayoutDeviceIntRect>& aRects);

  virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                            int32_t aNativeKeyCode,
                                            uint32_t aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters,
                                            nsIObserver* aObserver) override
  {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "keyevent");
    return NS_ERROR_UNEXPECTED;
  }

  virtual nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags,
                                              nsIObserver* aObserver) override
  {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");
    return NS_ERROR_UNEXPECTED;
  }

  virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) override
  {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");
    return NS_ERROR_UNEXPECTED;
  }

  virtual nsresult SynthesizeNativeMouseScrollEvent(LayoutDeviceIntPoint aPoint,
                                                    uint32_t aNativeMessage,
                                                    double aDeltaX,
                                                    double aDeltaY,
                                                    double aDeltaZ,
                                                    uint32_t aModifierFlags,
                                                    uint32_t aAdditionalFlags,
                                                    nsIObserver* aObserver) override
  {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mousescrollevent");
    return NS_ERROR_UNEXPECTED;
  }

  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) override
  {
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "touchpoint");
    return NS_ERROR_UNEXPECTED;
  }

  virtual nsresult NotifyIMEInternal(const IMENotification& aIMENotification)
  { return NS_ERROR_NOT_IMPLEMENTED; }

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

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget()
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_CHILD_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

  LayerManager* CreateBasicLayerManager();

  nsPopupType PopupType() const { return mPopupType; }

  bool HasRemoteContent() const { return mHasRemoteContent; }

  void NotifyRollupGeometryChange()
  {
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
  void ConstrainSize(int32_t* aWidth, int32_t* aHeight)
  {
    SizeConstraints c = GetSizeConstraints();
    *aWidth = std::max(c.mMinSize.width,
                       std::min(c.mMaxSize.width, *aWidth));
    *aHeight = std::max(c.mMinSize.height,
                        std::min(c.mMaxSize.height, *aHeight));
  }

  virtual CompositorBridgeChild* GetRemoteRenderer() override;

  /**
   * Notify the widget that this window is being used with OMTC.
   */
  virtual void WindowUsesOMTC() {}
  virtual void RegisterTouchWindow() {}

  nsIDocument* GetDocument() const;

  void EnsureTextEventDispatcher();

  // Notify the compositor that a device reset has occurred.
  void OnRenderingDeviceReset();

  bool UseAPZ();

  /**
   * For widgets that support synthesizing native touch events, this function
   * can be used to manage the current state of synthetic pointers. Each widget
   * must maintain its own MultiTouchInput instance and pass it in as the state,
   * along with the desired parameters for the changes. This function returns
   * a new MultiTouchInput object that is ready to be dispatched.
   */
  mozilla::MultiTouchInput
  UpdateSynthesizedTouchState(mozilla::MultiTouchInput* aState,
                              uint32_t aTime,
                              mozilla::TimeStamp aTimeStamp,
                              uint32_t aPointerId,
                              TouchPointerState aPointerState,
                              LayoutDeviceIntPoint aPoint,
                              double aPointerPressure,
                              uint32_t aPointerOrientation);

  /**
   * Dispatch the given MultiTouchInput through APZ to Gecko (if APZ is enabled)
   * or directly to gecko (if APZ is not enabled). This function must only
   * be called from the main thread, and if APZ is enabled, that must also be
   * the APZ controller thread.
   */
  void DispatchTouchInput(mozilla::MultiTouchInput& aInput);

#if defined(XP_WIN)
  void UpdateScrollCapture() override;

  /**
   * To be overridden by derived classes to return a snapshot that can be used
   * during scrolling. Returning null means we won't update the container.
   * @return an already AddRefed SourceSurface containing the snapshot
   */
  virtual already_AddRefed<SourceSurface> CreateScrollSnapshot()
  {
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
  virtual bool UseExternalCompositingSurface() const {
    return false;
  }

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

  nsIWidgetListener* mWidgetListener;
  nsIWidgetListener* mAttachedWidgetListener;
  nsIWidgetListener* mPreviouslyAttachedWidgetListener;
  RefPtr<LayerManager> mLayerManager;
  RefPtr<CompositorSession> mCompositorSession;
  RefPtr<CompositorBridgeChild> mCompositorBridgeChild;
  RefPtr<mozilla::CompositorVsyncDispatcher> mCompositorVsyncDispatcher;
  RefPtr<IAPZCTreeManager> mAPZC;
  RefPtr<GeckoContentController> mRootContentController;
  RefPtr<APZEventState> mAPZEventState;
  SetAllowedTouchBehaviorCallback mSetAllowedTouchBehaviorCallback;
  RefPtr<WidgetShutdownObserver> mShutdownObserver;
  RefPtr<TextEventDispatcher> mTextEventDispatcher;
  nsCursor          mCursor;
  nsBorderStyle     mBorderStyle;
  LayoutDeviceIntRect mBounds;
  LayoutDeviceIntRect* mOriginalBounds;
  // When this pointer is null, the widget is not clipped
  mozilla::UniquePtr<LayoutDeviceIntRect[]> mClipRects;
  uint32_t          mClipRectCount;
  nsSizeMode        mSizeMode;
  nsPopupLevel      mPopupLevel;
  nsPopupType       mPopupType;
  SizeConstraints   mSizeConstraints;
  bool              mHasRemoteContent;

  CompositorWidgetDelegate* mCompositorWidgetDelegate;

  bool              mUpdateCursor;
  bool              mUseAttachedEvents;
  bool              mIMEHasFocus;
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_GTK)
  bool              mAccessibilityInUseFlag;
#endif
  static nsIRollupListener* gRollupListener;

  struct InitialZoomConstraints {
    InitialZoomConstraints(const uint32_t& aPresShellID,
                           const FrameMetrics::ViewID& aViewID,
                           const ZoomConstraints& aConstraints)
      : mPresShellID(aPresShellID), mViewID(aViewID), mConstraints(aConstraints)
    {
    }

    uint32_t mPresShellID;
    FrameMetrics::ViewID mViewID;
    ZoomConstraints mConstraints;
  };

  mozilla::Maybe<InitialZoomConstraints> mInitialZoomConstraints;

  // This points to the resize listeners who have been notified that a live
  // resize is in progress. This should always be empty when a live-resize is
  // not in progress.
  nsTArray<RefPtr<mozilla::LiveResizeListener>> mLiveResizeListeners;

#ifdef DEBUG
protected:
  static nsAutoString debug_GuiEventToString(mozilla::WidgetGUIEvent* aGuiEvent);
  static bool debug_WantPaintFlashing();

  static void debug_DumpInvalidate(FILE* aFileOut,
                                   nsIWidget* aWidget,
                                   const LayoutDeviceIntRect* aRect,
                                   const char* aWidgetName,
                                   int32_t aWindowID);

  static void debug_DumpEvent(FILE* aFileOut,
                              nsIWidget* aWidget,
                              mozilla::WidgetGUIEvent* aGuiEvent,
                              const char* aWidgetName,
                              int32_t aWindowID);

  static void debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRegion &   aPaintEvent,
                                   const char *          aWidgetName,
                                   int32_t               aWindowID);

  static bool debug_GetCachedBoolPref(const char* aPrefName);
#endif
};

#endif // nsBaseWidget_h__
