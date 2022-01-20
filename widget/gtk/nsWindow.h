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
#  include "nsClipboardWayland.h"
#endif

#ifdef MOZ_LOGGING

#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"

extern mozilla::LazyLogModule gWidgetLog;
extern mozilla::LazyLogModule gWidgetDragLog;
extern mozilla::LazyLogModule gWidgetPopupLog;

#  define LOG(str, ...)                               \
    MOZ_LOG(IsPopup() ? gWidgetPopupLog : gWidgetLog, \
            mozilla::LogLevel::Debug,                 \
            ("%s: " str, GetDebugTag().get(), ##__VA_ARGS__))
#  define LOGW(...) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define LOGDRAG(...) \
    MOZ_LOG(gWidgetDragLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define LOG_POPUP(...) \
    MOZ_LOG(gWidgetPopupLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define LOG_ENABLED()                                         \
    (MOZ_LOG_TEST(gWidgetPopupLog, mozilla::LogLevel::Debug) || \
     MOZ_LOG_TEST(gWidgetLog, mozilla::LogLevel::Debug))

#else

#  define LOG(...)
#  define LOGW(...)
#  define LOGDRAG(...)
#  define LOG_POPUP(...)
#  define LOG_ENABLED() false

#endif /* MOZ_LOGGING */

#ifdef MOZ_WAYLAND
gboolean WindowDragMotionHandler(GtkWidget* aWidget,
                                 GdkDragContext* aDragContext,
                                 RefPtr<DataOffer> aDataOffer, gint aX, gint aY,
                                 guint aTime);
gboolean WindowDragDropHandler(GtkWidget* aWidget, GdkDragContext* aDragContext,
                               RefPtr<DataOffer> aDataOffer, gint aX, gint aY,
                               guint aTime);
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
  void OnMap();
  void OnUnrealize();
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
  bool SynchronouslyRepaintOnResize() override;

  void OnDPIChanged(void);
  void OnCheckResize(void);
  void OnCompositedChanged(void);
  void OnScaleChanged();
  void DispatchResized();

  static guint32 sLastButtonPressTime;

  [[nodiscard]] virtual nsresult BeginResizeDrag(
      mozilla::WidgetGUIEvent* aEvent, int32_t aHorizontal,
      int32_t aVertical) override;

  MozContainer* GetMozContainer() { return mContainer; }
  LayoutDeviceIntSize GetMozContainerSize();
  // GetMozContainerWidget returns the MozContainer even for undestroyed
  // descendant windows
  GtkWidget* GetMozContainerWidget();
  GdkWindow* GetGdkWindow() { return mGdkWindow; };
  GdkWindow* GetToplevelGdkWindow();
  GtkWidget* GetGtkWidget() { return mShell; }
  nsIFrame* GetFrame() const;
  bool IsDestroyed() const { return mIsDestroyed; }
  bool IsPopup() const;
  bool IsWaylandPopup() const;
  bool IsPIPWindow() const { return mIsPIPWindow; };
  bool IsDragPopup() { return mIsDragPopup; };

  nsAutoCString GetDebugTag() const;

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
  nsresult UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                                uint8_t* aAlphas,
                                                int32_t aStride);
  virtual void ReparentNativeWidget(nsIWidget* aNewParent) override;

  void UpdateTitlebarTransparencyBitmap();

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
      TouchpadGesturePhase aEventPhase, float aScale,
      LayoutDeviceIntPoint aPoint, int32_t aModifierFlags) override;

  virtual void GetCompositorWidgetInitData(
      mozilla::widget::CompositorWidgetInitData* aInitData) override;

  virtual nsresult SetNonClientMargins(
      LayoutDeviceIntMargin& aMargins) override;
  void SetDrawsInTitlebar(bool aState) override;
  mozilla::LayoutDeviceIntCoord GetTitlebarRadius();
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
  } GtkWindowDecoration;
  /**
   * Get the support of Client Side Decoration by checking
   * the XDG_CURRENT_DESKTOP environment variable.
   */
  static GtkWindowDecoration GetSystemGtkWindowDecoration();

  static bool GetTopLevelWindowActiveState(nsIFrame* aFrame);
  static bool TitlebarUseShapeMask();
  bool IsRemoteContent() { return HasRemoteContent(); }
  void NativeMoveResizeWaylandPopupCallback(const GdkRectangle* aFinalSize,
                                            bool aFlippedX, bool aFlippedY);
  static bool IsToplevelWindowTransparent();

  static nsWindow* GetFocusedWindow();

#ifdef MOZ_WAYLAND
  // Use xdg-activation protocol to transfer focus from gFocusWindow to aWindow.
  // RequestFocusWaylandWindow needs to be called on focused window only.
  void RequestFocusWaylandWindow(RefPtr<nsWindow> aWindow);
  void FocusWaylandWindow(const char* aTokenID);

  bool GetCSDDecorationOffset(int* aDx, int* aDy);
  void SetEGLNativeWindowSize(const LayoutDeviceIntSize& aEGLWindowSize);
  void WaylandDragWorkaround(GdkEventButton* aEvent);

  wl_display* GetWaylandDisplay();
  virtual void CreateCompositorVsyncDispatcher() override;
  LayoutDeviceIntPoint GetNativePointerLockCenter() {
    return mNativePointerLockCenter;
  }
  virtual void SetNativePointerLockCenter(
      const LayoutDeviceIntPoint& aLockCenter) override;
  virtual void LockNativePointer() override;
  virtual void UnlockNativePointer() override;
  virtual nsRect GetPreferredPopupRect() override {
    return mPreferredPopupRect;
  };
  virtual void FlushPreferredPopupRect() override {
    mPreferredPopupRect = nsRect(0, 0, 0, 0);
    mPreferredPopupRectFlushed = true;
  };
#endif

  typedef enum {
    // WebRender compositor is enabled
    COMPOSITOR_ENABLED,
    // WebRender compositor is paused after window creation.
    COMPOSITOR_PAUSED_INITIALLY,
    // WebRender compositor is paused because GtkWindow is hidden,
    // we can't draw into GL context.
    COMPOSITOR_PAUSED_MISSING_WINDOW,
    // WebRender compositor is paused as we're repainting whole window and
    // we're waiting for content process to update page content.
    COMPOSITOR_PAUSED_FLICKERING
  } WindowCompositorState;

  // Pause compositor to avoid rendering artifacts from content process.
  void ResumeCompositor();
  void ResumeCompositorFromCompositorThread();
  void PauseCompositor();
  bool IsWaitingForCompositorResume();

 protected:
  virtual ~nsWindow();

  // event handling code
  void DispatchActivateEvent(void);
  void DispatchDeactivateEvent(void);
  void MaybeDispatchResized();

  virtual void RegisterTouchWindow() override;
  virtual bool CompositorInitiallyPaused() override {
    return mCompositorState == COMPOSITOR_PAUSED_INITIALLY;
  }
  nsCOMPtr<nsIWidget> mParent;
  // Has this widget been destroyed yet?
  bool mIsDestroyed;
  // Does WindowResized need to be called on listeners?
  bool mNeedsDispatchResized;
  // mIsShown tracks requested visible status from browser perspective, i.e.
  // if the window should be visible or now.
  bool mIsShown;
  // mNeedsShow is set when browser requested to show this window but we failed
  // to do so for some reason (wrong window size for instance).
  // In such case we set mIsShown = true and mNeedsShow = true to indicate
  // that the window is not actually visible but we report to browser that
  // it is visible (mIsShown == true).
  bool mNeedsShow;
  // This track real window visibility from OS perspective.
  // It's set by OnMap/OnUnrealize which is based on Gtk events.
  bool mIsMapped;
  // is this widget enabled?
  bool mEnabled;
  // has the native window for this been created yet?
  bool mCreated;
  // whether we handle touch event
  bool mHandleTouchEvent;
  // true if this is a drag and drop feedback popup
  bool mIsDragPopup;
  nsPopupType mPopupHint;
  bool mWindowScaleFactorChanged;
  int mWindowScaleFactor;
  bool mCompositedScreen;
  bool mIsAccelerated;

 private:
  void UpdateAlpha(mozilla::gfx::SourceSurface* aSourceSurface,
                   nsIntRect aBoundsRect);

  void NativeMoveResize(bool aMoved, bool aResized);

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

  void EnableRenderingToWindow();
  void DisableRenderingToWindow();
  void ResumeCompositorHiddenWindow();
  void PauseCompositorHiddenWindow();
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
  nsIWidgetListener* GetListener();

  nsWindow* GetTransientForWindowIfPopup();
  bool IsHandlingTouchSequence(GdkEventSequence* aSequence);

  void ResizeInt(int aX, int aY, int aWidth, int aHeight, bool aMove,
                 bool aRepaint);
  void NativeMoveResizeWaylandPopup(bool aMove, bool aResize);

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
  mozilla::Atomic<WindowCompositorState, mozilla::Relaxed> mCompositorState;
  // This is used in COMPOSITOR_PAUSED_FLICKERING mode only to resume compositor
  // in some reasonable time when page content is not updated.
  int mCompositorPauseTimeoutID;

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
  // It's undecorated popup utility window, without resizers/titlebar,
  // movable by mouse. Used on Wayland for popups without
  // parent (for instance WebRTC sharing indicator, notifications).
  bool mIsWaylandPanelWindow;
  // It's child window, i.e. window which is nested in parent window.
  // This is obsoleted and should not be used.
  // We use GdkWindow hierarchy for such windows.
  bool mIsChildWindow;
  bool mAlwaysOnTop;
  bool mNoAutoHide;
  bool mMouseTransparent;

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

  // When window widget gets mapped/unmapped we need to configure
  // underlying GdkWindow properly. Otherwise we'll end up with
  // rendering to released window.
  void ConfigureGdkWindow();
  void ReleaseGdkWindow();

  // nsBaseWidget
  virtual WindowRenderer* GetWindowRenderer() override;

  void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;

  void CleanLayerManagerRecursive();

  virtual int32_t RoundsWidgetCoordinatesTo() override;

  void UpdateMozWindowActive();

  void ForceTitlebarRedraw();
  bool DoDrawTilebarCorners();
  bool IsChromeWindowTitlebar();

  void SetPopupWindowDecoration(bool aShowOnTaskbar);

  void ApplySizeConstraints(void);

  void GetParentPosition(int* aX, int* aY);

  // Wayland Popup section
  bool WaylandPopupNeedsTrackInHierarchy();
  bool WaylandPopupIsAnchored();
  bool WaylandPopupIsMenu();
  bool WaylandPopupIsContextMenu();
  bool WaylandPopupIsPermanent();
  bool IsWidgetOverflowWindow();
  void RemovePopupFromHierarchyList();
  void ShowWaylandWindow();
  void HideWaylandWindow();
  void HideWaylandPopupWindow(bool aTemporaryHidden, bool aRemoveFromPopupList);
  void HideWaylandToplevelWindow();
  void WaylandPopupHideTooltips();
  void AppendPopupToHierarchyList(nsWindow* aToplevelWindow);
  void WaylandPopupHierarchyHideTemporary();
  void WaylandPopupHierarchyShowTemporaryHidden();
  void WaylandPopupHierarchyCalculatePositions();
  bool IsInPopupHierarchy();
  void AddWindowToPopupHierarchy();
  void UpdateWaylandPopupHierarchy();
  void WaylandPopupHierarchyHideByLayout(
      nsTArray<nsIWidget*>* aLayoutWidgetHierarchy);
  void WaylandPopupHierarchyValidateByLayout(
      nsTArray<nsIWidget*>* aLayoutWidgetHierarchy);
  void CloseAllPopupsBeforeRemotePopup();
  void WaylandPopupHideClosedPopups();
  void WaylandPopupMove();
  bool WaylandPopupRemoveNegativePosition(int* aX = nullptr, int* aY = nullptr);
  nsWindow* WaylandPopupGetTopmostWindow();
  bool IsPopupInLayoutPopupChain(nsTArray<nsIWidget*>* aLayoutWidgetHierarchy,
                                 bool aMustMatchParent);
  void WaylandPopupMarkAsClosed();
  void WaylandPopupRemoveClosedPopups();
  void WaylandPopupSetDirectPosition();
  bool WaylandPopupFitsParentWindow(GdkRectangle* aSize);
  nsWindow* WaylandPopupFindLast(nsWindow* aPopup);
  GtkWindow* GetCurrentTopmostWindow();
  nsAutoCString GetFrameTag() const;
  nsCString GetPopupTypeName();
  bool IsPopupDirectionRTL();

#ifdef MOZ_LOGGING
  void LogPopupHierarchy();
#endif

  /*  Gkt creates popup in two incarnations - wl_subsurface and xdg_popup.
   *  Kind of popup is choosen before GdkWindow is mapped so we can change
   *  it only when GdkWindow is hidden.
   *
   *  Relevant Gtk code is at gdkwindow-wayland.c
   *  in should_map_as_popup() and should_map_as_subsurface()
   *
   *  wl_subsurface:
   *    - can't be positioned by move-to-rect
   *    - can stand outside popup widget hierarchy (has toplevel as parent)
   *    - don't have child popup widgets
   *
   *  xdg_popup:
   *    - can be positioned by move-to-rect
   *    - aligned in popup widget hierarchy, first one is attached to toplevel
   *    - has child (popup) widgets
   *
   *  Thus we need to map Firefox popup type to desired Gtk one:
   *
   *  wl_subsurface:
   *    - pernament panels
   *
   *  xdg_popup:
   *    - menus
   *    - autohide popups (hamburger menu)
   *    - extension popups
   *    - tooltips
   *
   *  We set mPopupTrackInHierarchy = false for pernament panels which
   *  are always mapped to toplevel and painted as wl_surfaces.
   */
  bool mPopupTrackInHierarchy;
  bool mPopupTrackInHierarchyConfigured;

  /* On X11 Gtk tends to ignore window position requests when gtk_window
   * is hidden. Save the position requests at mPopupPosition and apply
   * when the widget is shown.
   */
  bool mHiddenPopupPositioned;

  /*  mPopupPosition is the original popup position from layout,
   *  set by nsWindow::Move() or nsWindow::Resize().
   */
  GdkPoint mPopupPosition;

  /*  When popup is anchored, mPopupPosition is relative to its parent popup.
   */
  bool mPopupAnchored;

  /*  When popup is context menu.
   */
  bool mPopupContextMenu;

  /*  mRelativePopupPosition is popup position calculated against parent window.
   */
  GdkPoint mRelativePopupPosition;

  /* mRelativePopupOffset is used by context menus.
   */
  GdkPoint mRelativePopupOffset;

  /*  Indicates that this popup matches layout setup so we can use
   *  parent popup coordinates reliably.
   */
  bool mPopupMatchesLayout;

  /*  Indicates that popup setup was changed and
   *  we need to recalculate popup coordinates.
   */
  bool mPopupChanged;

  /*  Popup is hidden only as a part of hierarchy tree update.
   */
  bool mPopupTemporaryHidden;

  /*  Popup is going to be closed and removed.
   */
  bool mPopupClosed;

  /* Popup is positioned by gdk_window_move_to_rect()
   */
  bool mPopupUseMoveToRect;

  /* Last used anchor for move-to-rect.
   */
  LayoutDeviceIntRect mPopupLastAnchor;

  /* Toplevel window (first element) of linked list of wayland popups.
   * It's nullptr if we're the toplevel.
   */
  RefPtr<nsWindow> mWaylandToplevel;

  /* Next/Previous popups in Wayland popup hieararchy.
   */
  RefPtr<nsWindow> mWaylandPopupNext;
  RefPtr<nsWindow> mWaylandPopupPrev;

  /* Used by WaylandPopupMove() to track popup movement.
   *
   */
  nsRect mPreferredPopupRect;
  bool mPreferredPopupRectFlushed;

  /* mWaitingForMoveToRectCallback is set when move-to-rect is called
   * and we're waiting for move-to-rect callback.
   *
   * If another position/resize request comes between move-to-rect call and
   * move-to-rect callback we set mNewBoundsAfterMoveToRect.
   */
  bool mWaitingForMoveToRectCallback;
  LayoutDeviceIntRect mNewBoundsAfterMoveToRect;

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
  bool ConfigureX11GLVisual();

  Window mXWindow;
  Visual* mXVisual;
  int mXDepth;
  bool mIsShaped;
#endif
#ifdef MOZ_WAYLAND
  RefPtr<mozilla::gfx::VsyncSource> mWaylandVsyncSource;
  LayoutDeviceIntPoint mNativePointerLockCenter;
  zwp_locked_pointer_v1* mLockedPointer;
  zwp_relative_pointer_v1* mRelativePointer;
  xdg_activation_token_v1* mXdgToken;
#endif
  mozilla::widget::WindowSurfaceProvider mSurfaceProvider;
};

#endif /* __nsWindow_h__ */
