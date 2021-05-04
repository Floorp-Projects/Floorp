/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "CompositorWidget.h"
#include "MozContainer.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/widget/WindowSurface.h"
#include "mozilla/widget/WindowSurfaceProvider.h"
#include "nsBaseWidget.h"
#include "nsGkAtoms.h"
#include "nsIDragService.h"
#include "nsRefPtrHashtable.h"
#include "IMContextWrapper.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/LocalAccessible.h"
#endif

#ifdef MOZ_X11
#  include <gdk/gdkx.h>
#  include "X11UndefineNone.h"
#endif
#ifdef MOZ_WAYLAND
#  include <gdk/gdkwayland.h>
#  include "base/thread.h"
#  include "WaylandVsyncSource.h"
#endif

#undef LOG
#ifdef MOZ_LOGGING

#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"

extern mozilla::LazyLogModule gWidgetLog;
extern mozilla::LazyLogModule gWidgetDragLog;

#  define LOG(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#  define LOGDRAG(args) MOZ_LOG(gWidgetDragLog, mozilla::LogLevel::Debug, args)

#else

#  define LOG(args)
#  define LOGDRAG(args)

#endif /* MOZ_LOGGING */

#ifdef MOZ_WAYLAND
class nsWaylandDragContext;

gboolean WindowDragMotionHandler(GtkWidget* aWidget,
                                 GdkDragContext* aDragContext,
                                 nsWaylandDragContext* aWaylandDragContext,
                                 gint aX, gint aY, guint aTime);
gboolean WindowDragDropHandler(GtkWidget* aWidget, GdkDragContext* aDragContext,
                               nsWaylandDragContext* aWaylandDragContext,
                               gint aX, gint aY, guint aTime);
void WindowDragLeaveHandler(GtkWidget* aWidget);
#endif

class gfxPattern;
class nsIFrame;
#if !GTK_CHECK_VERSION(3, 18, 0)
struct _GdkEventTouchpadPinch;
typedef struct _GdkEventTouchpadPinch GdkEventTouchpadPinch;

#endif

namespace mozilla {
class TimeStamp;
class CurrentX11TimeGetter;

}  // namespace mozilla

class nsWindow final : public nsBaseWidget {
 public:
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::WidgetEventTime WidgetEventTime;
  typedef mozilla::WidgetKeyboardEvent WidgetKeyboardEvent;
  typedef mozilla::widget::PlatformCompositorWidgetDelegate
      PlatformCompositorWidgetDelegate;

  nsWindow();

  static void ReleaseGlobals();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, nsBaseWidget)

  void CommonCreate(nsIWidget* aParent, bool aListenForResizes);

  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;

  // called when we are destroyed
  virtual void OnDestroy(void) override;

  // called to check and see if a widget's dimensions are sane
  bool AreBoundsSane(void);

  // nsIWidget
  using nsBaseWidget::Create;  // for Create signature not overridden here
  [[nodiscard]] virtual nsresult Create(nsIWidget* aParent,
                                        nsNativeWidget aNativeParent,
                                        const LayoutDeviceIntRect& aRect,
                                        nsWidgetInitData* aInitData) override;
  virtual void Destroy() override;
  virtual nsIWidget* GetParent() override;
  virtual float GetDPI() override;
  virtual double GetDefaultScaleInternal() override;
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() override;
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScaleByScreen()
      override;
  virtual void SetParent(nsIWidget* aNewParent) override;
  virtual void SetModal(bool aModal) override;
  virtual bool IsVisible() const override;
  virtual void ConstrainPosition(bool aAllowSlop, int32_t* aX,
                                 int32_t* aY) override;
  virtual void SetSizeConstraints(const SizeConstraints& aConstraints) override;
  virtual void LockAspectRatio(bool aShouldLock) override;
  virtual void Move(double aX, double aY) override;
  virtual void Show(bool aState) override;
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override;
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) override;
  virtual bool IsEnabled() const override;

  void SetZIndex(int32_t aZIndex) override;
  virtual void SetSizeMode(nsSizeMode aMode) override;
  virtual void GetWorkspaceID(nsAString& workspaceID) override;
  virtual void MoveToWorkspace(const nsAString& workspaceID) override;
  virtual void Enable(bool aState) override;
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  virtual LayoutDeviceIntRect GetClientBounds() override;
  virtual LayoutDeviceIntSize GetClientSize() override;
  virtual LayoutDeviceIntPoint GetClientOffset() override;
  virtual void SetCursor(const Cursor&) override;
  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;
  virtual void* GetNativeData(uint32_t aDataType) override;
  virtual nsresult SetTitle(const nsAString& aTitle) override;
  virtual void SetIcon(const nsAString& aIconSpec) override;
  virtual void SetWindowClass(const nsAString& xulWinType) override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual void CaptureMouse(bool aCapture) override;
  virtual void CaptureRollupEvents(nsIRollupListener* aListener,
                                   bool aDoCapture) override;
  [[nodiscard]] virtual nsresult GetAttention(int32_t aCycleCount) override;
  virtual nsresult SetWindowClipRegion(
      const nsTArray<LayoutDeviceIntRect>& aRects,
      bool aIntersectWithExisting) override;
  virtual bool HasPendingInputEvent() override;

  virtual bool PrepareForFullscreenTransition(nsISupports** aData) override;
  virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) override;
  virtual already_AddRefed<nsIScreen> GetWidgetScreen() override;
  virtual nsresult MakeFullScreen(bool aFullScreen,
                                  nsIScreen* aTargetScreen = nullptr) override;
  virtual void HideWindowChrome(bool aShouldHide) override;

  /**
   * GetLastUserInputTime returns a timestamp for the most recent user input
   * event.  This is intended for pointer grab requests (including drags).
   */
  static guint32 GetLastUserInputTime();

  // utility method, -1 if no change should be made, otherwise returns a
  // value that can be passed to gdk_window_set_decorations
  gint ConvertBorderStyles(nsBorderStyle aStyle);

  GdkRectangle DevicePixelsToGdkRectRoundOut(LayoutDeviceIntRect aRect);

  mozilla::widget::IMContextWrapper* GetIMContext() const { return mIMContext; }

  bool DispatchCommandEvent(nsAtom* aCommand);
  bool DispatchContentCommandEvent(mozilla::EventMessage aMsg);

  // event callbacks
  gboolean OnExposeEvent(cairo_t* cr);
  gboolean OnConfigureEvent(GtkWidget* aWidget, GdkEventConfigure* aEvent);
  void OnContainerUnrealize();
  void OnSizeAllocate(GtkAllocation* aAllocation);
  void OnDeleteEvent();
  void OnEnterNotifyEvent(GdkEventCrossing* aEvent);
  void OnLeaveNotifyEvent(GdkEventCrossing* aEvent);
  void OnMotionNotifyEvent(GdkEventMotion* aEvent);
  void OnButtonPressEvent(GdkEventButton* aEvent);
  void OnButtonReleaseEvent(GdkEventButton* aEvent);
  void OnContainerFocusInEvent(GdkEventFocus* aEvent);
  void OnContainerFocusOutEvent(GdkEventFocus* aEvent);
  gboolean OnKeyPressEvent(GdkEventKey* aEvent);
  gboolean OnKeyReleaseEvent(GdkEventKey* aEvent);

  void OnScrollEvent(GdkEventScroll* aEvent);

  void OnWindowStateEvent(GtkWidget* aWidget, GdkEventWindowState* aEvent);
  void OnDragDataReceivedEvent(GtkWidget* aWidget, GdkDragContext* aDragContext,
                               gint aX, gint aY,
                               GtkSelectionData* aSelectionData, guint aInfo,
                               guint aTime, gpointer aData);
  gboolean OnPropertyNotifyEvent(GtkWidget* aWidget, GdkEventProperty* aEvent);
  gboolean OnTouchEvent(GdkEventTouch* aEvent);
  gboolean OnTouchpadPinchEvent(GdkEventTouchpadPinch* aEvent);

  void UpdateTopLevelOpaqueRegion();

  virtual already_AddRefed<mozilla::gfx::DrawTarget> StartRemoteDrawingInRegion(
      const LayoutDeviceIntRegion& aInvalidRegion,
      mozilla::layers::BufferMode* aBufferMode) override;
  virtual void EndRemoteDrawingInRegion(
      mozilla::gfx::DrawTarget* aDrawTarget,
      const LayoutDeviceIntRegion& aInvalidRegion) override;

  void SetProgress(unsigned long progressPercent);

  RefPtr<mozilla::gfx::VsyncSource> GetVsyncSource() override;

  static void WithSettingsChangesIgnored(const std::function<void()>& aFn);

  void ThemeChanged(void);
  void OnDPIChanged(void);
  void OnCheckResize(void);
  void OnCompositedChanged(void);
  void OnScaleChanged(GtkAllocation* aAllocation);
  void DispatchResized();

  static guint32 sLastButtonPressTime;

  [[nodiscard]] virtual nsresult BeginResizeDrag(
      mozilla::WidgetGUIEvent* aEvent, int32_t aHorizontal,
      int32_t aVertical) override;

  MozContainer* GetMozContainer() { return mContainer; }
  LayoutDeviceIntRect GetMozContainerSize();
  // GetMozContainerWidget returns the MozContainer even for undestroyed
  // descendant windows
  GtkWidget* GetMozContainerWidget();
  GdkWindow* GetGdkWindow() { return mGdkWindow; }
  GtkWidget* GetGtkWidget() { return mShell; }
  nsIFrame* GetFrame();
  bool IsDestroyed() { return mIsDestroyed; }
  bool IsPopup();
  bool IsWaylandPopup();
  bool IsPIPWindow() { return mIsPIPWindow; };

  void DispatchDragEvent(mozilla::EventMessage aMsg,
                         const LayoutDeviceIntPoint& aRefPoint, guint aTime);
  static void UpdateDragStatus(GdkDragContext* aDragContext,
                               nsIDragService* aDragService);

  WidgetEventTime GetWidgetEventTime(guint32 aEventTime);
  mozilla::TimeStamp GetEventTimeStamp(guint32 aEventTime);
  mozilla::CurrentX11TimeGetter* GetCurrentTimeGetter();

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;
  virtual TextEventDispatcherListener* GetNativeTextEventDispatcherListener()
      override;
  MOZ_CAN_RUN_SCRIPT virtual bool GetEditCommands(
      NativeKeyBindingsType aType, const mozilla::WidgetKeyboardEvent& aEvent,
      nsTArray<mozilla::CommandInt>& aCommands) override;

  // These methods are for toplevel windows only.
  void ResizeTransparencyBitmap();
  void ApplyTransparencyBitmap();
  void ClearTransparencyBitmap();

  virtual void SetTransparencyMode(nsTransparencyMode aMode) override;
  virtual nsTransparencyMode GetTransparencyMode() override;
  virtual void SetWindowMouseTransparent(bool aIsTransparent) override;
  virtual nsresult ConfigureChildren(
      const nsTArray<Configuration>& aConfigurations) override;
  nsresult UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                                uint8_t* aAlphas,
                                                int32_t aStride);
  void UpdateTitlebarTransparencyBitmap();

  virtual void ReparentNativeWidget(nsIWidget* aNewParent) override;

  virtual nsresult SynthesizeNativeMouseEvent(
      LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
      mozilla::MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
      nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) override {
    return SynthesizeNativeMouseEvent(
        aPoint, NativeMouseMessage::Move, mozilla::MouseButton::eNotPressed,
        nsIWidget::Modifiers::NO_MODIFIERS, aObserver);
  }

  virtual nsresult SynthesizeNativeMouseScrollEvent(
      LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
      double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
      uint32_t aAdditionalFlags, nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeTouchPadPinch(
      TouchpadPinchPhase aEventPhase, float aScale, LayoutDeviceIntPoint aPoint,
      int32_t aModifierFlags) override;

  virtual void GetCompositorWidgetInitData(
      mozilla::widget::CompositorWidgetInitData* aInitData) override;

  virtual nsresult SetNonClientMargins(
      LayoutDeviceIntMargin& aMargins) override;
  void SetDrawsInTitlebar(bool aState) override;
  LayoutDeviceIntRect GetTitlebarRect();
  virtual void UpdateWindowDraggingRegion(
      const LayoutDeviceIntRegion& aRegion) override;

  // HiDPI scale conversion
  gint GdkCeiledScaleFactor();
  bool UseFractionalScale();
  double FractionalScaleFactor();

  // To GDK
  gint DevicePixelsToGdkCoordRoundUp(int pixels);
  gint DevicePixelsToGdkCoordRoundDown(int pixels);
  GdkPoint DevicePixelsToGdkPointRoundDown(LayoutDeviceIntPoint point);
  GdkRectangle DevicePixelsToGdkSizeRoundUp(LayoutDeviceIntSize pixelSize);

  // From GDK
  int GdkCoordToDevicePixels(gint coord);
  LayoutDeviceIntPoint GdkPointToDevicePixels(GdkPoint point);
  LayoutDeviceIntPoint GdkEventCoordsToDevicePixels(gdouble x, gdouble y);
  LayoutDeviceIntRect GdkRectToDevicePixels(GdkRectangle rect);

  virtual bool WidgetTypeSupportsAcceleration() override;

  nsresult SetSystemFont(const nsCString& aFontName) override;
  nsresult GetSystemFont(nsCString& aFontName) override;

  typedef enum {
    GTK_DECORATION_SYSTEM,  // CSD including shadows
    GTK_DECORATION_CLIENT,  // CSD without shadows
    GTK_DECORATION_NONE,    // WM does not support CSD at all
    GTK_DECORATION_UNKNOWN
  } GtkWindowDecoration;
  /**
   * Get the support of Client Side Decoration by checking
   * the XDG_CURRENT_DESKTOP environment variable.
   */
  static GtkWindowDecoration GetSystemGtkWindowDecoration();

  static bool HideTitlebarByDefault();
  static bool GetTopLevelWindowActiveState(nsIFrame* aFrame);
  static bool TitlebarUseShapeMask();
  bool IsRemoteContent() { return HasRemoteContent(); }
  static void HideWaylandOpenedPopups();
  void NativeMoveResizeWaylandPopupCB(const GdkRectangle* aFinalSize,
                                      bool aFlippedX, bool aFlippedY);
  static bool IsToplevelWindowTransparent();

#ifdef MOZ_WAYLAND
  bool GetCSDDecorationOffset(int* aDx, int* aDy);
  void SetEGLNativeWindowSize(const LayoutDeviceIntSize& aEGLWindowSize);
  static nsWindow* GetFocusedWindow();
  void WaylandDragWorkaround(GdkEventButton* aEvent);

  wl_display* GetWaylandDisplay();
  bool WaylandSurfaceNeedsClear();
  virtual void CreateCompositorVsyncDispatcher() override;
  LayoutDeviceIntPoint GetNativePointerLockCenter() {
    return mNativePointerLockCenter;
  }
  virtual void SetNativePointerLockCenter(
      const LayoutDeviceIntPoint& aLockCenter) override;
  virtual void LockNativePointer() override;
  virtual void UnlockNativePointer() override;
  virtual nsresult GetScreenRect(LayoutDeviceIntRect* aRect) override;
  virtual nsRect GetPreferredPopupRect() override {
    return mPreferredPopupRect;
  };
  virtual void FlushPreferredPopupRect() override {
    mPreferredPopupRect = nsRect(0, 0, 0, 0);
    mPreferredPopupRectFlushed = true;
  };
#endif

 protected:
  virtual ~nsWindow();

  // event handling code
  void DispatchActivateEvent(void);
  void DispatchDeactivateEvent(void);
  void MaybeDispatchResized();

  virtual void RegisterTouchWindow() override;
  virtual bool CompositorInitiallyPaused() override {
#ifdef MOZ_WAYLAND
    return mCompositorInitiallyPaused;
#else
    return false;
#endif
  }
  nsCOMPtr<nsIWidget> mParent;
  // Is this a toplevel window?
  bool mIsTopLevel;
  // Has this widget been destroyed yet?
  bool mIsDestroyed;

  // Should we send resize events on all resizes?
  bool mListenForResizes;
  // Does WindowResized need to be called on listeners?
  bool mNeedsDispatchResized;
  // This flag tracks if we're hidden or shown.
  bool mIsShown;
  bool mNeedsShow;
  // is this widget enabled?
  bool mEnabled;
  // has the native window for this been created yet?
  bool mCreated;
  // whether we handle touch event
  bool mHandleTouchEvent;
  // true if this is a drag and drop feedback popup
  bool mIsDragPopup;
  bool mWindowScaleFactorChanged;
  int mWindowScaleFactor;
  bool mCompositedScreen;

#ifdef MOZ_WAYLAND
  bool mNeedsCompositorResume;
  bool mCompositorInitiallyPaused;
  LayoutDeviceIntPoint mNativePointerLockCenter;
#endif

 private:
  void UpdateAlpha(mozilla::gfx::SourceSurface* aSourceSurface,
                   nsIntRect aBoundsRect);

  void NativeMove();
  void NativeResize();
  void NativeMoveResize();

  void NativeShow(bool aAction);
  void SetHasMappedToplevel(bool aState);
  LayoutDeviceIntSize GetSafeWindowSize(LayoutDeviceIntSize aSize);

  void EnsureGrabs(void);
  void GrabPointer(guint32 aTime);
  void ReleaseGrabs(void);

  void UpdateClientOffsetFromFrameExtents();
  void UpdateClientOffsetFromCSDWindow();

  void DispatchContextMenuEventFromMouseEvent(uint16_t domButton,
                                              GdkEventButton* aEvent);

  void WaylandStartVsync();
  void WaylandStopVsync();
  void DestroyChildWindows();
  GtkWidget* GetToplevelWidget();
  nsWindow* GetContainerWindow();
  void SetUrgencyHint(GtkWidget* top_window, bool state);
  void SetDefaultIcon(void);
  void SetWindowDecoration(nsBorderStyle aStyle);
  void InitButtonEvent(mozilla::WidgetMouseEvent& aEvent,
                       GdkEventButton* aGdkEvent);
  bool CheckForRollup(gdouble aMouseX, gdouble aMouseY, bool aIsWheel,
                      bool aAlwaysRollup);
  void CheckForRollupDuringGrab() { CheckForRollup(0, 0, false, true); }

  bool GetDragInfo(mozilla::WidgetMouseEvent* aMouseEvent, GdkWindow** aWindow,
                   gint* aButton, gint* aRootX, gint* aRootY);
  void ClearCachedResources();
  nsIWidgetListener* GetListener();

  nsWindow* GetTransientForWindowIfPopup();
  bool IsHandlingTouchSequence(GdkEventSequence* aSequence);

  void ResizeInt(int aX, int aY, int aWidth, int aHeight, bool aMove,
                 bool aRepaint);
  void NativeMoveResizeWaylandPopup(GdkPoint* aPosition, GdkRectangle* aSize);

  // Returns true if the given point (in device pixels) is within a resizer
  // region of the window. Only used when drawing decorations client side.
  bool CheckResizerEdge(LayoutDeviceIntPoint aPoint, GdkWindowEdge& aOutEdge);

  GtkTextDirection GetTextDirection();

  void AddCSDDecorationSize(int* aWidth, int* aHeight);

  nsCString mGtkWindowAppName;
  nsCString mGtkWindowRoleName;
  void RefreshWindowClass();

  GtkWidget* mShell;
  MozContainer* mContainer;
  GdkWindow* mGdkWindow;
  bool mWindowShouldStartDragging;
  PlatformCompositorWidgetDelegate* mCompositorWidgetDelegate;

  uint32_t mHasMappedToplevel : 1, mRetryPointerGrab : 1;
  nsSizeMode mSizeState;
  float mAspectRatio;
  float mAspectRatioSaved;
  nsIntPoint mClientOffset;

  // This field omits duplicate scroll events caused by GNOME bug 726878.
  guint32 mLastScrollEventTime;
  mozilla::ScreenCoord mLastPinchEventSpan;
  bool mPanInProgress = false;

  // for touch event handling
  nsRefPtrHashtable<nsPtrHashKey<GdkEventSequence>, mozilla::dom::Touch>
      mTouches;

  // Upper bound on pending ConfigureNotify events to be dispatched to the
  // window. See bug 1225044.
  unsigned int mPendingConfigures;

  // Window titlebar rendering mode, GTK_DECORATION_NONE if it's disabled
  // for this window.
  GtkWindowDecoration mGtkWindowDecoration;
  // Use dedicated GdkWindow for mContainer
  bool mDrawToContainer;
  // If true, draw our own window titlebar.
  bool mDrawInTitlebar;
  // Draw titlebar with :backdrop css state (inactive/unfocused).
  bool mTitlebarBackdropState;
  // Draggable titlebar region maintained by UpdateWindowDraggingRegion
  LayoutDeviceIntRegion mDraggableRegion;
  // It's PictureInPicture window.
  bool mIsPIPWindow;
  bool mAlwaysOnTop;

  // The cursor cache
  static GdkCursor* gsGtkCursorCache[eCursorCount];

  // Transparency
  bool mIsTransparent;
  // This bitmap tracks which pixels are transparent. We don't support
  // full translucency at this time; each pixel is either fully opaque
  // or fully transparent.
  gchar* mTransparencyBitmap;
  int32_t mTransparencyBitmapWidth;
  int32_t mTransparencyBitmapHeight;
  // The transparency bitmap is used instead of ARGB visual for toplevel
  // window to draw titlebar.
  bool mTransparencyBitmapForTitlebar;

  // True when we're on compositing window manager and this
  // window is using visual with alpha channel.
  bool mHasAlphaVisual;

  // all of our DND stuff
  void InitDragEvent(mozilla::WidgetDragEvent& aEvent);

  float mLastMotionPressure;

  // Remember the last sizemode so that we can restore it when
  // leaving fullscreen
  nsSizeMode mLastSizeMode;
  // We can't detect size state changes correctly so set this flag
  // to force update mBounds after a size state change from a configure
  // event.
  bool mBoundsAreValid;

  static bool DragInProgress(void);

  void DispatchMissedButtonReleases(GdkEventCrossing* aGdkEvent);

  // nsBaseWidget
  virtual LayerManager* GetLayerManager(
      PLayerTransactionChild* aShadowManager = nullptr,
      LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
      LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;

  void CleanLayerManagerRecursive();

  virtual int32_t RoundsWidgetCoordinatesTo() override;

  void UpdateMozWindowActive();

  void ForceTitlebarRedraw();
  bool DoDrawTilebarCorners();
  bool IsChromeWindowTitlebar();

  void SetPopupWindowDecoration(bool aShowOnTaskbar);

  void ApplySizeConstraints(void);

  bool IsMainMenuWindow();
  GtkWidget* ConfigureWaylandPopupWindows();
  void PauseRemoteRenderer();
  void HideWaylandWindow();
  void HideWaylandTooltips();
  void HideWaylandPopupAndAllChildren();
  void CleanupWaylandPopups();
  GtkWindow* GetCurrentTopmostWindow();
  GtkWindow* GetCurrentWindow();
  GtkWindow* GetTopmostWindow();
  bool IsWidgetOverflowWindow();
  nsRect mPreferredPopupRect;
  bool mPreferredPopupRectFlushed;
  bool mWaitingForMoveToRectCB;
  LayoutDeviceIntRect mPendingSizeRect;

  /**
   * |mIMContext| takes all IME related stuff.
   *
   * This is owned by the top-level nsWindow or the topmost child
   * nsWindow embedded in a non-Gecko widget.
   *
   * The instance is created when the top level widget is created.  And when
   * the widget is destroyed, it's released.  All child windows refer its
   * ancestor widget's instance.  So, one set of IM contexts is created for
   * all windows in a hierarchy.  If the children are released after the top
   * level window is released, the children still have a valid pointer,
   * however, IME doesn't work at that time.
   */
  RefPtr<mozilla::widget::IMContextWrapper> mIMContext;

  mozilla::UniquePtr<mozilla::CurrentX11TimeGetter> mCurrentTimeGetter;
  static GtkWindowDecoration sGtkWindowDecoration;

  static bool sTransparentMainWindow;

#ifdef ACCESSIBILITY
  RefPtr<mozilla::a11y::LocalAccessible> mRootAccessible;

  /**
   * Request to create the accessible for this window if it is top level.
   */
  void CreateRootAccessible();

  /**
   * Dispatch accessible event for the top level window accessible.
   *
   * @param  aEventType  [in] the accessible event type to dispatch
   */
  void DispatchEventToRootAccessible(uint32_t aEventType);

  /**
   * Dispatch accessible window activate event for the top level window
   * accessible.
   */
  void DispatchActivateEventAccessible();

  /**
   * Dispatch accessible window deactivate event for the top level window
   * accessible.
   */
  void DispatchDeactivateEventAccessible();

  /**
   * Dispatch accessible window maximize event for the top level window
   * accessible.
   */
  void DispatchMaximizeEventAccessible();

  /**
   * Dispatch accessible window minize event for the top level window
   * accessible.
   */
  void DispatchMinimizeEventAccessible();

  /**
   * Dispatch accessible window restore event for the top level window
   * accessible.
   */
  void DispatchRestoreEventAccessible();
#endif

#ifdef MOZ_X11
  typedef enum {GTK_WIDGET_COMPOSIDED_DEFAULT = 0,
                GTK_WIDGET_COMPOSIDED_DISABLED = 1,
                GTK_WIDGET_COMPOSIDED_ENABLED = 2} WindowComposeRequest;
  void SetCompositorHint(WindowComposeRequest aState);

  Window mXWindow;
  Visual* mXVisual;
  int mXDepth;
  mozilla::widget::WindowSurfaceProvider mSurfaceProvider;

  bool ConfigureX11GLVisual(bool aUseAlpha);
#endif
#ifdef MOZ_WAYLAND
  void MaybeResumeCompositor();

  RefPtr<mozilla::gfx::VsyncSource> mWaylandVsyncSource;
  zwp_locked_pointer_v1* mLockedPointer;
  zwp_relative_pointer_v1* mRelativePointer;
#endif
};

#endif /* __nsWindow_h__ */
