/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Window_h__
#define Window_h__

/*
 * nsWindow - Native window management and event handling.
 */

#include "mozilla/RefPtr.h"
#include "nsBaseWidget.h"
#include "CompositorWidget.h"
#include "nsWindowBase.h"
#include "nsdefs.h"
#include "nsUserIdleService.h"
#include "nsToolkit.h"
#include "nsString.h"
#include "nsTArray.h"
#include "gfxWindowsPlatform.h"
#include "gfxWindowsSurface.h"
#include "nsWindowDbg.h"
#include "cairo.h"
#include "nsRegion.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/UniquePtr.h"
#include "nsMargin.h"
#include "nsRegionFwd.h"

#include "nsWinGesture.h"
#include "WinPointerEvents.h"
#include "WinUtils.h"
#include "WindowHook.h"
#include "TaskbarWindowPreview.h"

#ifdef ACCESSIBILITY
#  include "oleacc.h"
#  include "mozilla/a11y/LocalAccessible.h"
#endif

#include "nsUXThemeData.h"
#include "nsIUserIdleServiceInternal.h"

#include "IMMHandler.h"

/**
 * Forward class definitions
 */

class nsNativeDragTarget;
class nsIRollupListener;
class imgIContainer;

namespace mozilla {
namespace widget {
class NativeKey;
class InProcessWinCompositorWidget;
struct MSGResult;
class DirectManipulationOwner;
}  // namespace widget
}  // namespace mozilla

/**
 * Forward Windows-internal definitions of otherwise incomplete ones provided by
 * the SDK.
 */
const CLSID CLSID_ImmersiveShell = {
    0xC2F03A33,
    0x21F5,
    0x47FA,
    {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};

// Virtual Desktop.

EXTERN_C const IID IID_IVirtualDesktopManager;
MIDL_INTERFACE("a5cd92ff-29be-454c-8d04-d82879fb3f1b")
IVirtualDesktopManager : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(
      __RPC__in HWND topLevelWindow, __RPC__out GUID * desktopId) = 0;
  virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(
      __RPC__in HWND topLevelWindow, __RPC__in REFGUID desktopId) = 0;
};

/**
 * Native WIN32 window wrapper.
 */

class nsWindow final : public nsWindowBase {
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;
  typedef mozilla::widget::WindowHook WindowHook;
  typedef mozilla::widget::TaskbarWindowPreview TaskbarWindowPreview;
  typedef mozilla::widget::NativeKey NativeKey;
  typedef mozilla::widget::MSGResult MSGResult;
  typedef mozilla::widget::IMEContext IMEContext;
  typedef mozilla::widget::PlatformCompositorWidgetDelegate
      PlatformCompositorWidgetDelegate;

 public:
  explicit nsWindow(bool aIsChildWindow = false);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, nsWindowBase)

  friend class nsWindowGfx;

  void SendAnAPZEvent(mozilla::InputData& aEvent);

  // nsWindowBase
  virtual void InitEvent(mozilla::WidgetGUIEvent& aEvent,
                         LayoutDeviceIntPoint* aPoint = nullptr) override;
  virtual WidgetEventTime CurrentMessageWidgetEventTime() const override;
  virtual bool DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent) override;
  virtual bool DispatchKeyboardEvent(
      mozilla::WidgetKeyboardEvent* aEvent) override;
  virtual bool DispatchWheelEvent(mozilla::WidgetWheelEvent* aEvent) override;
  virtual bool DispatchContentCommandEvent(
      mozilla::WidgetContentCommandEvent* aEvent) override;
  virtual nsWindowBase* GetParentWindowBase(bool aIncludeOwner) override;
  virtual bool IsTopLevelWidget() override { return mIsTopWidgetWindow; }

  // nsIWidget interface
  using nsWindowBase::Create;  // for Create signature not overridden here
  [[nodiscard]] virtual nsresult Create(
      nsIWidget* aParent, nsNativeWidget aNativeParent,
      const LayoutDeviceIntRect& aRect,
      nsWidgetInitData* aInitData = nullptr) override;
  virtual void Destroy() override;
  virtual void SetParent(nsIWidget* aNewParent) override;
  virtual nsIWidget* GetParent(void) override;
  virtual float GetDPI() override;
  double GetDefaultScaleInternal() final;
  int32_t LogToPhys(double aValue) final;
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() final {
    if (mozilla::widget::WinUtils::IsPerMonitorDPIAware()) {
      return mozilla::DesktopToLayoutDeviceScale(1.0);
    } else {
      return mozilla::DesktopToLayoutDeviceScale(GetDefaultScaleInternal());
    }
  }

  virtual void Show(bool aState) override;
  virtual bool IsVisible() const override;
  virtual void ConstrainPosition(bool aAllowSlop, int32_t* aX,
                                 int32_t* aY) override;
  virtual void SetSizeConstraints(const SizeConstraints& aConstraints) override;
  virtual void LockAspectRatio(bool aShouldLock) override;
  virtual const SizeConstraints GetSizeConstraints() override;
  virtual void SetWindowMouseTransparent(bool aIsTransparent) override;
  virtual void Move(double aX, double aY) override;
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override;
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) override;
  virtual mozilla::Maybe<bool> IsResizingNativeWidget() override;
  [[nodiscard]] virtual nsresult BeginResizeDrag(
      mozilla::WidgetGUIEvent* aEvent, int32_t aHorizontal,
      int32_t aVertical) override;
  virtual void PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget* aWidget, bool aActivate) override;
  virtual void SetSizeMode(nsSizeMode aMode) override;
  virtual void GetWorkspaceID(nsAString& workspaceID) override;
  virtual void MoveToWorkspace(const nsAString& workspaceID) override;
  virtual void SuppressAnimation(bool aSuppress) override;
  virtual void Enable(bool aState) override;
  virtual bool IsEnabled() const override;
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  virtual LayoutDeviceIntRect GetBounds() override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  [[nodiscard]] virtual nsresult GetRestoredBounds(
      LayoutDeviceIntRect& aRect) override;
  virtual LayoutDeviceIntRect GetClientBounds() override;
  virtual LayoutDeviceIntPoint GetClientOffset() override;
  void SetBackgroundColor(const nscolor& aColor) override;
  virtual void SetCursor(const Cursor&) override;
  virtual nsresult ConfigureChildren(
      const nsTArray<Configuration>& aConfigurations) override;
  virtual bool PrepareForFullscreenTransition(nsISupports** aData) override;
  virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) override;
  virtual void CleanupFullscreenTransition() override;
  virtual nsresult MakeFullScreen(bool aFullScreen,
                                  nsIScreen* aScreen = nullptr) override;
  virtual void HideWindowChrome(bool aShouldHide) override;
  virtual void Invalidate(bool aEraseBackground = false,
                          bool aUpdateNCArea = false,
                          bool aIncludeChildren = false);
  virtual void Invalidate(const LayoutDeviceIntRect& aRect);
  virtual void* GetNativeData(uint32_t aDataType) override;
  void SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
  virtual void FreeNativeData(void* data, uint32_t aDataType) override;
  virtual nsresult SetTitle(const nsAString& aTitle) override;
  virtual void SetIcon(const nsAString& aIconSpec) override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual LayoutDeviceIntSize ClientToWindowSize(
      const LayoutDeviceIntSize& aClientSize) override;
  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;
  virtual void EnableDragDrop(bool aEnable) override;
  virtual void CaptureMouse(bool aCapture) override;
  virtual void CaptureRollupEvents(nsIRollupListener* aListener,
                                   bool aDoCapture) override;
  [[nodiscard]] virtual nsresult GetAttention(int32_t aCycleCount) override;
  virtual bool HasPendingInputEvent() override;
  virtual LayerManager* GetLayerManager(
      PLayerTransactionChild* aShadowManager = nullptr,
      LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
      LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;
  void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;
  [[nodiscard]] virtual nsresult OnDefaultButtonLoaded(
      const LayoutDeviceIntRect& aButtonRect) override;
  virtual nsresult SynthesizeNativeKeyEvent(
      int32_t aNativeKeyboardLayout, int32_t aNativeKeyCode,
      uint32_t aModifierFlags, const nsAString& aCharacters,
      const nsAString& aUnmodifiedCharacters, nsIObserver* aObserver) override;
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
  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;
  virtual TextEventDispatcherListener* GetNativeTextEventDispatcherListener()
      override;
#ifdef MOZ_XUL
  virtual void SetTransparencyMode(nsTransparencyMode aMode) override;
  virtual nsTransparencyMode GetTransparencyMode() override;
  virtual void UpdateOpaqueRegion(
      const LayoutDeviceIntRegion& aOpaqueRegion) override;
#endif  // MOZ_XUL
  virtual nsresult SetNonClientMargins(
      LayoutDeviceIntMargin& aMargins) override;
  void SetDrawsInTitlebar(bool aState) override;
  virtual void UpdateWindowDraggingRegion(
      const LayoutDeviceIntRegion& aRegion) override;

  virtual void UpdateThemeGeometries(
      const nsTArray<ThemeGeometry>& aThemeGeometries) override;
  virtual uint32_t GetMaxTouchPoints() const override;
  virtual void SetWindowClass(const nsAString& xulWinType) override;

  /**
   * Event helpers
   */
  virtual bool DispatchMouseEvent(mozilla::EventMessage aEventMessage,
                                  WPARAM wParam, LPARAM lParam,
                                  bool aIsContextMenuKey, int16_t aButton,
                                  uint16_t aInputSource,
                                  WinPointerInfo* aPointerInfo = nullptr);
  virtual bool DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent,
                                   nsEventStatus& aStatus);
  void DispatchPendingEvents();
  void DispatchCustomEvent(const nsString& eventName);

#ifdef ACCESSIBILITY
  /**
   * Return an accessible associated with the window.
   */
  mozilla::a11y::LocalAccessible* GetAccessible();
#endif  // ACCESSIBILITY

  /**
   * Window utilities
   */
  nsWindow* GetTopLevelWindow(bool aStopOnDialogOrPopup);
  WNDPROC GetPrevWindowProc() { return mPrevWndProc; }
  WindowHook& GetWindowHook() { return mWindowHook; }
  nsWindow* GetParentWindow(bool aIncludeOwner);
  // Get an array of all nsWindow*s on the main thread.
  static nsTArray<nsWindow*> EnumAllWindows();

  /**
   * Misc.
   */
  virtual bool AutoErase(HDC dc);
  bool WidgetTypeSupportsAcceleration() override;

  void ForcePresent();
  bool TouchEventShouldStartDrag(mozilla::EventMessage aEventMessage,
                                 LayoutDeviceIntPoint aEventPoint);

  void SetSmallIcon(HICON aIcon);
  void SetBigIcon(HICON aIcon);

  static void SetIsRestoringSession(const bool aIsRestoringSession) {
    sIsRestoringSession = aIsRestoringSession;
  }

  /**
   * AssociateDefaultIMC() associates or disassociates the default IMC for
   * the window.
   *
   * @param aAssociate    TRUE, associates the default IMC with the window.
   *                      Otherwise, disassociates the default IMC from the
   *                      window.
   * @return              TRUE if this method associated the default IMC with
   *                      disassociated window or disassociated the default IMC
   *                      from associated window.
   *                      Otherwise, i.e., if this method did nothing actually,
   *                      FALSE.
   */
  bool AssociateDefaultIMC(bool aAssociate);

  bool HasTaskbarIconBeenCreated() { return mHasTaskbarIconBeenCreated; }
  // Called when either the nsWindow or an nsITaskbarTabPreview receives the
  // noticiation that this window has its icon placed on the taskbar.
  void SetHasTaskbarIconBeenCreated(bool created = true) {
    mHasTaskbarIconBeenCreated = created;
  }

  // Getter/setter for the nsITaskbarWindowPreview for this nsWindow
  already_AddRefed<nsITaskbarWindowPreview> GetTaskbarPreview() {
    nsCOMPtr<nsITaskbarWindowPreview> preview(
        do_QueryReferent(mTaskbarPreview));
    return preview.forget();
  }
  void SetTaskbarPreview(nsITaskbarWindowPreview* preview) {
    mTaskbarPreview = do_GetWeakReference(preview);
  }

  virtual void ReparentNativeWidget(nsIWidget* aNewParent) override;

  // Open file picker tracking
  void PickerOpen();
  void PickerClosed();

  bool const DestroyCalled() { return mDestroyCalled; }

  bool IsPopup();
  virtual bool ShouldUseOffMainThreadCompositing() override;

  const IMEContext& DefaultIMC() const { return mDefaultIMC; }

  void GetCompositorWidgetInitData(
      mozilla::widget::CompositorWidgetInitData* aInitData) override;
  bool IsTouchWindow() const { return mTouchWindow; }
  bool SynchronouslyRepaintOnResize() override;
  virtual void MaybeDispatchInitialFocusEvent() override;

 protected:
  virtual ~nsWindow();

  virtual void WindowUsesOMTC() override;
  virtual void RegisterTouchWindow() override;

  // A magic number to identify the FAKETRACKPOINTSCROLLABLE window created
  // when the trackpoint hack is enabled.
  enum { eFakeTrackPointScrollableID = 0x46545053 };

  // Used for displayport suppression during window resize
  enum ResizeState { NOT_RESIZING, IN_SIZEMOVE, RESIZING, MOVING };

  /**
   * Callbacks
   */
  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam);
  static LRESULT CALLBACK WindowProcInternal(HWND hWnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam);

  static BOOL CALLBACK BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg);
  static BOOL CALLBACK BroadcastMsg(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK DispatchStarvedPaints(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK RegisterTouchForDescendants(HWND aTopWindow,
                                                   LPARAM aMsg);
  static BOOL CALLBACK UnregisterTouchForDescendants(HWND aTopWindow,
                                                     LPARAM aMsg);
  static LRESULT CALLBACK MozSpecialMsgFilter(int code, WPARAM wParam,
                                              LPARAM lParam);
  static LRESULT CALLBACK MozSpecialWndProc(int code, WPARAM wParam,
                                            LPARAM lParam);
  static LRESULT CALLBACK MozSpecialMouseProc(int code, WPARAM wParam,
                                              LPARAM lParam);
  static VOID CALLBACK HookTimerForPopups(HWND hwnd, UINT uMsg, UINT idEvent,
                                          DWORD dwTime);
  static BOOL CALLBACK ClearResourcesCallback(HWND aChild, LPARAM aParam);
  static BOOL CALLBACK EnumAllChildWindProc(HWND aWnd, LPARAM aParam);
  static BOOL CALLBACK EnumAllThreadWindowProc(HWND aWnd, LPARAM aParam);

  /**
   * Window utilities
   */
  LPARAM lParamToScreen(LPARAM lParam);
  LPARAM lParamToClient(LPARAM lParam);
  virtual void SubclassWindow(BOOL bState);
  bool CanTakeFocus();
  bool UpdateNonClientMargins(int32_t aSizeMode = -1,
                              bool aReflowWindow = true);
  void UpdateGetWindowInfoCaptionStatus(bool aActiveCaption);
  void ResetLayout();
  void InvalidateNonClientRegion();
  HRGN ExcludeNonClientFromPaintRegion(HRGN aRegion);
  static const wchar_t* GetMainWindowClass();
  bool HasGlass() const {
    return mTransparencyMode == eTransparencyGlass ||
           mTransparencyMode == eTransparencyBorderlessGlass;
  }
  HWND GetOwnerWnd() const { return ::GetWindow(mWnd, GW_OWNER); }
  bool IsOwnerForegroundWindow() const {
    HWND owner = GetOwnerWnd();
    return owner && owner == ::GetForegroundWindow();
  }
  bool IsPopup() const { return mWindowType == eWindowType_popup; }

  /**
   * Event processing helpers
   */
  HWND GetTopLevelForFocus(HWND aCurWnd);
  void DispatchFocusToTopLevelWindow(bool aIsActivate);
  bool DispatchStandardEvent(mozilla::EventMessage aMsg);
  void RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);
  virtual bool ProcessMessage(UINT msg, WPARAM& wParam, LPARAM& lParam,
                              LRESULT* aRetValue);
  bool ExternalHandlerProcessMessage(UINT aMessage, WPARAM& aWParam,
                                     LPARAM& aLParam, MSGResult& aResult);
  LRESULT ProcessCharMessage(const MSG& aMsg, bool* aEventDispatched);
  LRESULT ProcessKeyUpMessage(const MSG& aMsg, bool* aEventDispatched);
  LRESULT ProcessKeyDownMessage(const MSG& aMsg, bool* aEventDispatched);
  static bool EventIsInsideWindow(
      nsWindow* aWindow,
      mozilla::Maybe<POINT> aEventPoint = mozilla::Nothing());
  // Convert nsEventStatus value to a windows boolean
  static bool ConvertStatus(nsEventStatus aStatus);
  static void PostSleepWakeNotification(const bool aIsSleepMode);
  int32_t ClientMarginHitTestPoint(int32_t mx, int32_t my);
  TimeStamp GetMessageTimeStamp(LONG aEventTime) const;
  static void UpdateFirstEventTime(DWORD aEventTime);
  void FinishLiveResizing(ResizeState aNewState);
  nsIntPoint GetTouchCoordinates(WPARAM wParam, LPARAM lParam);
  mozilla::Maybe<mozilla::PanGestureInput> ConvertTouchToPanGesture(
      const mozilla::MultiTouchInput& aTouchInput, PTOUCHINPUT aOriginalEvent);
  void DispatchTouchOrPanGestureInput(mozilla::MultiTouchInput& aTouchInput,
                                      PTOUCHINPUT aOSEvent);

  /**
   * Event handlers
   */
  virtual void OnDestroy() override;
  bool OnResize(const LayoutDeviceIntSize& aSize);
  void OnSizeModeChange(nsSizeMode aSizeMode);
  bool OnGesture(WPARAM wParam, LPARAM lParam);
  bool OnTouch(WPARAM wParam, LPARAM lParam);
  bool OnHotKey(WPARAM wParam, LPARAM lParam);
  bool OnPaint(HDC aDC, uint32_t aNestingLevel);
  void OnWindowPosChanged(WINDOWPOS* wp);
  void OnWindowPosChanging(LPWINDOWPOS& info);
  void OnSysColorChanged();
  void OnDPIChanged(int32_t x, int32_t y, int32_t width, int32_t height);
  bool OnPointerEvents(UINT msg, WPARAM wParam, LPARAM lParam);

  /**
   * Function that registers when the user has been active (used for detecting
   * when the user is idle).
   */
  void UserActivity();

  int32_t GetHeight(int32_t aProposedHeight);
  const wchar_t* GetWindowClass() const;
  const wchar_t* GetWindowPopupClass() const;
  virtual DWORD WindowStyle();
  DWORD WindowExStyle();

  // This method registers the given window class, and returns the class name.
  const wchar_t* RegisterWindowClass(const wchar_t* aClassName,
                                     UINT aExtraStyle, LPWSTR aIconID) const;

  /**
   * XP and Vista theming support for windows with rounded edges
   */
  void ClearThemeRegion();
  void SetThemeRegion();

  /**
   * Popup hooks
   */
  static void ScheduleHookTimer(HWND aWnd, UINT aMsgId);
  static void RegisterSpecialDropdownHooks();
  static void UnregisterSpecialDropdownHooks();
  static bool GetPopupsToRollup(
      nsIRollupListener* aRollupListener, uint32_t* aPopupsToRollup,
      mozilla::Maybe<POINT> aEventPoint = mozilla::Nothing());
  static bool NeedsToHandleNCActivateDelayed(HWND aWnd);
  static bool DealWithPopups(HWND inWnd, UINT inMsg, WPARAM inWParam,
                             LPARAM inLParam, LRESULT* outResult);

  /**
   * Window transparency helpers
   */
#ifdef MOZ_XUL
 private:
  void SetWindowTranslucencyInner(nsTransparencyMode aMode);
  nsTransparencyMode GetWindowTranslucencyInner() const {
    return mTransparencyMode;
  }
  void UpdateGlass();
  bool WithinDraggableRegion(int32_t clientX, int32_t clientY);

  bool DispatchTouchEventFromWMPointer(UINT msg, LPARAM aLParam,
                                       const WinPointerInfo& aPointerInfo);

 protected:
#endif  // MOZ_XUL

  static bool IsAsyncResponseEvent(UINT aMsg, LRESULT& aResult);
  void IPCWindowProcHandler(UINT& msg, WPARAM& wParam, LPARAM& lParam);

  /**
   * Misc.
   */
  void StopFlashing();
  static HWND WindowAtMouse();
  static bool IsTopLevelMouseExit(HWND aWnd);
  virtual nsresult SetWindowClipRegion(
      const nsTArray<LayoutDeviceIntRect>& aRects,
      bool aIntersectWithExisting) override;
  LayoutDeviceIntRegion GetRegionToPaint(bool aForceFullRepaint, PAINTSTRUCT ps,
                                         HDC aDC);
  void ClearCachedResources();
  nsIWidgetListener* GetPaintListener();

  virtual void AddWindowOverlayWebRenderCommands(
      mozilla::layers::WebRenderBridgeChild* aWrBridge,
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResourceUpdates) override;

  already_AddRefed<SourceSurface> CreateScrollSnapshot() override;

  struct ScrollSnapshot {
    RefPtr<gfxWindowsSurface> surface;
    bool surfaceHasSnapshot = false;
    RECT clip;
  };

  ScrollSnapshot* EnsureSnapshotSurface(ScrollSnapshot& aSnapshotData,
                                        const mozilla::gfx::IntSize& aSize);

  ScrollSnapshot mFullSnapshot;
  ScrollSnapshot mPartialSnapshot;
  ScrollSnapshot* mCurrentSnapshot = nullptr;

  already_AddRefed<SourceSurface> GetFallbackScrollSnapshot(
      const RECT& aRequiredClip);

  void CreateCompositor() override;
  void RequestFxrOutput();

  void RecreateDirectManipulationIfNeeded();
  void ResizeDirectManipulationViewport();
  void DestroyDirectManipulation();

 protected:
  nsCOMPtr<nsIWidget> mParent;
  nsIntSize mLastSize;
  nsIntPoint mLastPoint;
  HWND mWnd;
  HWND mTransitionWnd;
  WNDPROC mPrevWndProc;
  HBRUSH mBrush;
  IMEContext mDefaultIMC;
  HDEVNOTIFY mDeviceNotifyHandle;
  bool mIsTopWidgetWindow;
  bool mInDtor;
  bool mIsVisible;
  bool mPainting;
  bool mTouchWindow;
  bool mDisplayPanFeedback;
  bool mHideChrome;
  bool mIsRTL;
  bool mFullscreenMode;
  bool mMousePresent;
  bool mMouseInDraggableArea;
  bool mDestroyCalled;
  bool mOpeningAnimationSuppressed;
  bool mAlwaysOnTop;
  bool mIsEarlyBlankWindow;
  bool mIsShowingPreXULSkeletonUI;
  bool mResizable;
  DWORD_PTR mOldStyle;
  DWORD_PTR mOldExStyle;
  nsNativeDragTarget* mNativeDragTarget;
  HKL mLastKeyboardLayout;
  nsSizeMode mOldSizeMode;
  nsSizeMode mLastSizeMode;
  WindowHook mWindowHook;
  uint32_t mPickerDisplayCount;
  HICON mIconSmall;
  HICON mIconBig;
  HWND mLastKillFocusWindow;
  static bool sDropShadowEnabled;
  static uint32_t sInstanceCount;
  static TriStateBool sCanQuit;
  static nsWindow* sCurrentWindow;
  static BOOL sIsOleInitialized;
  static HCURSOR sHCursor;
  static Cursor sCurrentCursor;
  static bool sSwitchKeyboardLayout;
  static bool sJustGotDeactivate;
  static bool sJustGotActivate;
  static bool sIsInMouseCapture;
  static bool sHaveInitializedPrefs;
  static bool sIsRestoringSession;
  static bool sFirstTopLevelWindowCreated;

  PlatformCompositorWidgetDelegate* mCompositorWidgetDelegate;

  // Always use the helper method to read this property.  See bug 603793.
  static TriStateBool sHasBogusPopupsDropShadowOnMultiMonitor;
  static bool HasBogusPopupsDropShadowOnMultiMonitor();

  // Non-client margin settings
  // Pre-calculated outward offset applied to default frames
  LayoutDeviceIntMargin mNonClientOffset;
  // Margins set by the owner
  LayoutDeviceIntMargin mNonClientMargins;
  // Margins we'd like to set once chrome is reshown:
  LayoutDeviceIntMargin mFutureMarginsOnceChromeShows;
  // Indicates we need to apply margins once toggling chrome into showing:
  bool mFutureMarginsToUse;

  // Indicates custom frames are enabled
  bool mCustomNonClient;
  // Cached copy of L&F's resize border
  int32_t mHorResizeMargin;
  int32_t mVertResizeMargin;
  // Height of the caption plus border
  int32_t mCaptionHeight;

  double mDefaultScale;

  float mAspectRatio;

  nsCOMPtr<nsIUserIdleServiceInternal> mIdleService;

  // Draggable titlebar region maintained by UpdateWindowDraggingRegion
  LayoutDeviceIntRegion mDraggableRegion;

  // Hook Data Memebers for Dropdowns. sProcessHook Tells the
  // hook methods whether they should be processing the hook
  // messages.
  static HHOOK sMsgFilterHook;
  static HHOOK sCallProcHook;
  static HHOOK sCallMouseHook;
  static bool sProcessHook;
  static UINT sRollupMsgId;
  static HWND sRollupMsgWnd;
  static UINT sHookTimerId;

  // Mouse Clicks - static variable definitions for figuring
  // out 1 - 3 Clicks.
  static POINT sLastMousePoint;
  static POINT sLastMouseMovePoint;
  static LONG sLastMouseDownTime;
  static LONG sLastClickCount;
  static BYTE sLastMouseButton;

  // Graphics
  HDC mPaintDC;  // only set during painting

  LayoutDeviceIntRect mLastPaintBounds;

  ResizeState mResizeState;

  // Transparency
#ifdef MOZ_XUL
  nsTransparencyMode mTransparencyMode;
  nsIntRegion mPossiblyTransparentRegion;
  MARGINS mGlassMargins;
#endif  // MOZ_XUL

  // Win7 Gesture processing and management
  nsWinGesture mGesture;

  // Weak ref to the nsITaskbarWindowPreview associated with this window
  nsWeakPtr mTaskbarPreview;
  // True if the taskbar (possibly through the tab preview) tells us that the
  // icon has been created on the taskbar.
  bool mHasTaskbarIconBeenCreated;

  // Indicates that mouse events should be ignored and pass through to the
  // window below. This is currently only used for popups.
  bool mMouseTransparent;

  // Whether we're in the process of sending a WM_SETTEXT ourselves
  bool mSendingSetText;

  // Whether we we're created as a child window (aka ChildWindow) or not.
  bool mIsChildWindow : 1;

  int32_t mCachedHitTestResult;

  // The point in time at which the last paint completed. We use this to avoid
  //  painting too rapidly in response to frequent input events.
  TimeStamp mLastPaintEndTime;

  // The location of the window buttons in the window.
  mozilla::Maybe<LayoutDeviceIntRect> mWindowButtonsRect;

  // Caching for hit test results
  POINT mCachedHitTestPoint;
  TimeStamp mCachedHitTestTime;

  RefPtr<mozilla::widget::InProcessWinCompositorWidget> mBasicLayersSurface;

  static bool sNeedsToInitMouseWheelSettings;
  static void InitMouseWheelScrollData();

  double mSizeConstraintsScale;  // scale in effect when setting constraints
  int32_t mMaxTextureSize;

  // Pointer events processing and management
  WinPointerEvents mPointerEvents;

  ScreenPoint mLastPanGestureFocus;

  // When true, used to indicate an async call to RequestFxrOutput to the GPU
  // process after the Compositor is created
  bool mRequestFxrOutputPending;

  mozilla::UniquePtr<mozilla::widget::DirectManipulationOwner> mDmOwner;
};

#endif  // Window_h__
