/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_WINDOWS_NSWINDOW_H_
#define WIDGET_WINDOWS_NSWINDOW_H_

/*
 * nsWindow - Native window management and event handling.
 */

#include "mozilla/RefPtr.h"
#include "nsBaseWidget.h"
#include "CompositorWidget.h"
#include "mozilla/EventForwards.h"
#include "nsClassHashtable.h"
#include <windows.h>
#include "touchinjection_sdk80.h"
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
#include "mozilla/EnumeratedArray.h"
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
#include "CheckInvariantWrapper.h"

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
  virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(
      __RPC__in HWND topLevelWindow, __RPC__out BOOL * onCurrentDesktop) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(
      __RPC__in HWND topLevelWindow, __RPC__out GUID * desktopId) = 0;
  virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(
      __RPC__in HWND topLevelWindow, __RPC__in REFGUID desktopId) = 0;
};

/**
 * Native WIN32 window wrapper.
 */

class nsWindow final : public nsBaseWidget {
 public:
  using WindowHook = mozilla::widget::WindowHook;
  using IMEContext = mozilla::widget::IMEContext;
  using WidgetEventTime = mozilla::WidgetEventTime;

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, nsBaseWidget)

  explicit nsWindow(bool aIsChildWindow = false);

  void SendAnAPZEvent(mozilla::InputData& aEvent);

  /*
   * Init a standard gecko event for this widget.
   * @param aEvent the event to initialize.
   * @param aPoint message position in physical coordinates.
   */
  void InitEvent(mozilla::WidgetGUIEvent& aEvent,
                 LayoutDeviceIntPoint* aPoint = nullptr);

  /*
   * Returns WidgetEventTime instance which is initialized with current message
   * time.
   */
  WidgetEventTime CurrentMessageWidgetEventTime() const;

  /*
   * Dispatch a gecko keyboard event for this widget. This
   * is called by KeyboardLayout to dispatch gecko events.
   * Returns true if it's consumed.  Otherwise, false.
   */
  bool DispatchKeyboardEvent(mozilla::WidgetKeyboardEvent* aEvent);

  /*
   * Dispatch a gecko wheel event for this widget. This
   * is called by ScrollHandler to dispatch gecko events.
   * Returns true if it's consumed.  Otherwise, false.
   */
  bool DispatchWheelEvent(mozilla::WidgetWheelEvent* aEvent);

  /*
   * Dispatch a gecko content command event for this widget. This
   * is called by ScrollHandler to dispatch gecko events.
   * Returns true if it's consumed.  Otherwise, false.
   */
  bool DispatchContentCommandEvent(mozilla::WidgetContentCommandEvent* aEvent);

  /*
   * Return the parent window, if it exists.
   */
  nsWindow* GetParentWindowBase(bool aIncludeOwner);

  /*
   * Return true if this is a top level widget.
   */
  bool IsTopLevelWidget() { return mIsTopWidgetWindow; }

  // nsIWidget interface
  using nsBaseWidget::Create;  // for Create signature not overridden here
  [[nodiscard]] nsresult Create(nsIWidget* aParent,
                                nsNativeWidget aNativeParent,
                                const LayoutDeviceIntRect& aRect,
                                nsWidgetInitData* aInitData = nullptr) override;
  void Destroy() override;
  void SetParent(nsIWidget* aNewParent) override;
  nsIWidget* GetParent(void) override;
  float GetDPI() override;
  double GetDefaultScaleInternal() override;
  int32_t LogToPhys(double aValue);
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() override {
    if (mozilla::widget::WinUtils::IsPerMonitorDPIAware()) {
      return mozilla::DesktopToLayoutDeviceScale(1.0);
    } else {
      return mozilla::DesktopToLayoutDeviceScale(GetDefaultScaleInternal());
    }
  }

  void Show(bool aState) override;
  bool IsVisible() const override;
  void ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) override;
  void SetSizeConstraints(const SizeConstraints& aConstraints) override;
  void LockAspectRatio(bool aShouldLock) override;
  const SizeConstraints GetSizeConstraints() override;
  void SetWindowMouseTransparent(bool aIsTransparent) override;
  void Move(double aX, double aY) override;
  void Resize(double aWidth, double aHeight, bool aRepaint) override;
  void Resize(double aX, double aY, double aWidth, double aHeight,
              bool aRepaint) override;
  mozilla::Maybe<bool> IsResizingNativeWidget() override;
  [[nodiscard]] nsresult BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                         int32_t aHorizontal,
                                         int32_t aVertical) override;
  void PlaceBehind(nsTopLevelWidgetZPlacement aPlacement, nsIWidget* aWidget,
                   bool aActivate) override;
  void SetSizeMode(nsSizeMode aMode) override;
  nsSizeMode SizeMode() override;
  void GetWorkspaceID(nsAString& workspaceID) override;
  void MoveToWorkspace(const nsAString& workspaceID) override;
  void SuppressAnimation(bool aSuppress) override;
  void Enable(bool aState) override;
  bool IsEnabled() const override;
  void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  LayoutDeviceIntRect GetBounds() override;
  LayoutDeviceIntRect GetScreenBounds() override;
  [[nodiscard]] nsresult GetRestoredBounds(LayoutDeviceIntRect& aRect) override;
  LayoutDeviceIntRect GetClientBounds() override;
  LayoutDeviceIntPoint GetClientOffset() override;
  void SetBackgroundColor(const nscolor& aColor) override;
  void SetCursor(const Cursor&) override;
  bool PrepareForFullscreenTransition(nsISupports** aData) override;
  void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                   uint16_t aDuration, nsISupports* aData,
                                   nsIRunnable* aCallback) override;
  void CleanupFullscreenTransition() override;
  nsresult MakeFullScreen(bool aFullScreen) override;
  void HideWindowChrome(bool aShouldHide) override;
  void Invalidate(bool aEraseBackground = false, bool aUpdateNCArea = false,
                  bool aIncludeChildren = false);
  void Invalidate(const LayoutDeviceIntRect& aRect) override;
  void* GetNativeData(uint32_t aDataType) override;
  void SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
  void FreeNativeData(void* data, uint32_t aDataType) override;
  nsresult SetTitle(const nsAString& aTitle) override;
  void SetIcon(const nsAString& aIconSpec) override;
  LayoutDeviceIntPoint WidgetToScreenOffset() override;
  LayoutDeviceIntSize ClientToWindowSize(
      const LayoutDeviceIntSize& aClientSize) override;
  nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus& aStatus) override;
  void EnableDragDrop(bool aEnable) override;
  void CaptureMouse(bool aCapture) override;
  void CaptureRollupEvents(nsIRollupListener* aListener,
                           bool aDoCapture) override;
  [[nodiscard]] nsresult GetAttention(int32_t aCycleCount) override;
  bool HasPendingInputEvent() override;
  WindowRenderer* GetWindowRenderer() override;
  void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;
  [[nodiscard]] nsresult OnDefaultButtonLoaded(
      const LayoutDeviceIntRect& aButtonRect) override;
  nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                    int32_t aNativeKeyCode,
                                    uint32_t aModifierFlags,
                                    const nsAString& aCharacters,
                                    const nsAString& aUnmodifiedCharacters,
                                    nsIObserver* aObserver) override;
  nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                      NativeMouseMessage aNativeMessage,
                                      mozilla::MouseButton aButton,
                                      nsIWidget::Modifiers aModifierFlags,
                                      nsIObserver* aObserver) override;

  nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                     nsIObserver* aObserver) override {
    return SynthesizeNativeMouseEvent(
        aPoint, NativeMouseMessage::Move, mozilla::MouseButton::eNotPressed,
        nsIWidget::Modifiers::NO_MODIFIERS, aObserver);
  }

  nsresult SynthesizeNativeMouseScrollEvent(
      LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
      double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
      uint32_t aAdditionalFlags, nsIObserver* aObserver) override;

  nsresult SynthesizeNativeTouchpadPan(TouchpadGesturePhase aEventPhase,
                                       LayoutDeviceIntPoint aPoint,
                                       double aDeltaX, double aDeltaY,
                                       int32_t aModifierFlagsn,
                                       nsIObserver* aObserver) override;

  void SetInputContext(const InputContext& aContext,
                       const InputContextAction& aAction) override;
  InputContext GetInputContext() override;
  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
  void SetTransparencyMode(nsTransparencyMode aMode) override;
  nsTransparencyMode GetTransparencyMode() override;
  void UpdateOpaqueRegion(const LayoutDeviceIntRegion& aOpaqueRegion) override;
  nsresult SetNonClientMargins(LayoutDeviceIntMargin& aMargins) override;
  void SetResizeMargin(mozilla::LayoutDeviceIntCoord aResizeMargin) override;
  void SetDrawsInTitlebar(bool aState) override;
  void UpdateWindowDraggingRegion(
      const LayoutDeviceIntRegion& aRegion) override;

  void UpdateThemeGeometries(
      const nsTArray<ThemeGeometry>& aThemeGeometries) override;
  uint32_t GetMaxTouchPoints() const override;
  void SetWindowClass(const nsAString& xulWinType) override;

  /**
   * Event helpers
   */
  bool DispatchMouseEvent(mozilla::EventMessage aEventMessage, WPARAM wParam,
                          LPARAM lParam, bool aIsContextMenuKey,
                          int16_t aButton, uint16_t aInputSource,
                          WinPointerInfo* aPointerInfo = nullptr,
                          bool aIgnoreAPZ = false);
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
  bool AutoErase(HDC dc);
  bool WidgetTypeSupportsAcceleration() override;

  void ForcePresent();
  bool TouchEventShouldStartDrag(mozilla::EventMessage aEventMessage,
                                 LayoutDeviceIntPoint aEventPoint);

  void SetSmallIcon(HICON aIcon);
  void SetBigIcon(HICON aIcon);
  void SetSmallIconNoData();
  void SetBigIconNoData();

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

  void ReparentNativeWidget(nsIWidget* aNewParent) override;

  // Open file picker tracking
  void PickerOpen();
  void PickerClosed();

  bool DestroyCalled() { return mDestroyCalled; }

  bool IsPopup();
  bool ShouldUseOffMainThreadCompositing() override;

  const IMEContext& DefaultIMC() const { return mDefaultIMC; }

  void GetCompositorWidgetInitData(
      mozilla::widget::CompositorWidgetInitData* aInitData) override;
  bool IsTouchWindow() const { return mTouchWindow; }
  bool SynchronouslyRepaintOnResize() override;
  void MaybeDispatchInitialFocusEvent() override;

  void LocalesChanged() override;

  void NotifyOcclusionState(mozilla::widget::OcclusionState aState) override;
  void MaybeEnableWindowOcclusion(bool aEnable);

  /*
   * Return the HWND or null for this widget.
   */
  HWND GetWindowHandle() {
    return static_cast<HWND>(GetNativeData(NS_NATIVE_WINDOW));
  }

  /*
   * Touch input injection apis
   */
  nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                      TouchPointerState aPointerState,
                                      LayoutDeviceIntPoint aPoint,
                                      double aPointerPressure,
                                      uint32_t aPointerOrientation,
                                      nsIObserver* aObserver) override;
  nsresult ClearNativeTouchSequence(nsIObserver* aObserver) override;

  nsresult SynthesizeNativePenInput(uint32_t aPointerId,
                                    TouchPointerState aPointerState,
                                    LayoutDeviceIntPoint aPoint,
                                    double aPressure, uint32_t aRotation,
                                    int32_t aTiltX, int32_t aTiltY,
                                    int32_t aButton,
                                    nsIObserver* aObserver) override;

  /*
   * WM_APPCOMMAND common handler.
   * Sends events via NativeKey::HandleAppCommandMessage().
   */
  bool HandleAppCommandMsg(const MSG& aAppCommandMsg, LRESULT* aRetValue);

  const InputContext& InputContextRef() const { return mInputContext; }

 private:
  using TimeStamp = mozilla::TimeStamp;
  using TimeDuration = mozilla::TimeDuration;
  using TaskbarWindowPreview = mozilla::widget::TaskbarWindowPreview;
  using NativeKey = mozilla::widget::NativeKey;
  using MSGResult = mozilla::widget::MSGResult;
  using PlatformCompositorWidgetDelegate =
      mozilla::widget::PlatformCompositorWidgetDelegate;

  struct Desktop {
    // Cached GUID of the virtual desktop this window should be on.
    // This value may be stale.
    nsString mID;
    bool mUpdateIsQueued = false;
  };

  class PointerInfo {
   public:
    enum class PointerType : uint8_t {
      TOUCH,
      PEN,
    };

    PointerInfo(int32_t aPointerId, LayoutDeviceIntPoint& aPoint,
                PointerType aType)
        : mPointerId(aPointerId), mPosition(aPoint), mType(aType) {}

    int32_t mPointerId;
    LayoutDeviceIntPoint mPosition;
    PointerType mType;
  };

  class FrameState {
   public:
    explicit FrameState(nsWindow* aWindow);

    void ConsumePreXULSkeletonState(bool aWasMaximized);
    void EnsureSizeMode(nsSizeMode aMode);
    void EnsureFullscreenMode(bool aFullScreen);
    void OnFrameChanging();
    void OnFrameChanged();

    nsSizeMode GetSizeMode() const;

    void CheckInvariant() const;

   private:
    void SetSizeModeInternal(nsSizeMode aMode);

    nsSizeMode mSizeMode = nsSizeMode_Normal;
    nsSizeMode mLastSizeMode = nsSizeMode_Normal;
    nsSizeMode mOldSizeMode = nsSizeMode_Normal;
    bool mFullscreenMode = false;
    nsWindow* mWindow;
  };

  // A magic number to identify the FAKETRACKPOINTSCROLLABLE window created
  // when the trackpoint hack is enabled.
  enum { eFakeTrackPointScrollableID = 0x46545053 };

  // Used for displayport suppression during window resize
  enum ResizeState { NOT_RESIZING, IN_SIZEMOVE, RESIZING, MOVING };

  ~nsWindow() override;

  void WindowUsesOMTC() override;
  void RegisterTouchWindow() override;

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
  static BOOL CALLBACK EnumAllChildWindProc(HWND aWnd, LPARAM aParam);
  static BOOL CALLBACK EnumAllThreadWindowProc(HWND aWnd, LPARAM aParam);

  /**
   * Window utilities
   */
  LPARAM lParamToScreen(LPARAM lParam);
  LPARAM lParamToClient(LPARAM lParam);

  WPARAM wParamFromGlobalMouseState();

  void SubclassWindow(BOOL bState);
  bool CanTakeFocus();
  bool UpdateNonClientMargins(int32_t aSizeMode = -1,
                              bool aReflowWindow = true);
  void UpdateDarkModeToolbar();
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
  bool ProcessMessage(UINT msg, WPARAM& wParam, LPARAM& lParam,
                      LRESULT* aRetValue);
  // We wrap this in ProcessMessage so we can log the return value
  bool ProcessMessageInternal(UINT msg, WPARAM& wParam, LPARAM& lParam,
                              LRESULT* aRetValue);
  bool ExternalHandlerProcessMessage(UINT aMessage, WPARAM& aWParam,
                                     LPARAM& aLParam, MSGResult& aResult);
  LRESULT ProcessCharMessage(const MSG& aMsg, bool* aEventDispatched);
  LRESULT ProcessKeyUpMessage(const MSG& aMsg, bool* aEventDispatched);
  LRESULT ProcessKeyDownMessage(const MSG& aMsg, bool* aEventDispatched);
  static bool EventIsInsideWindow(
      nsWindow* aWindow,
      mozilla::Maybe<POINT> aEventPoint = mozilla::Nothing());
  static void PostSleepWakeNotification(const bool aIsSleepMode);
  int32_t ClientMarginHitTestPoint(int32_t mx, int32_t my);
  void SetWindowButtonRect(WindowButtonType aButtonType,
                           const LayoutDeviceIntRect& aClientRect) override {
    mWindowBtnRect[aButtonType] = aClientRect;
  }
  TimeStamp GetMessageTimeStamp(LONG aEventTime) const;
  static void UpdateFirstEventTime(DWORD aEventTime);
  void FinishLiveResizing(ResizeState aNewState);
  mozilla::Maybe<mozilla::PanGestureInput> ConvertTouchToPanGesture(
      const mozilla::MultiTouchInput& aTouchInput, PTOUCHINPUT aOriginalEvent);
  void DispatchTouchOrPanGestureInput(mozilla::MultiTouchInput& aTouchInput,
                                      PTOUCHINPUT aOSEvent);

  /**
   * Event handlers
   */
  void OnDestroy() override;
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

  DWORD WindowStyle();
  DWORD WindowExStyle();

  static const wchar_t* ChooseWindowClass(nsWindowType, bool aForMenupopupFrame);
  // This method registers the given window class, and returns the class name.
  static const wchar_t* RegisterWindowClass(const wchar_t* aClassName,
                                            UINT aExtraStyle, LPWSTR aIconID);

  /**
   * XP and Vista theming support for windows with rounded edges
   */
  void ClearThemeRegion();

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
  void SetWindowTranslucencyInner(nsTransparencyMode aMode);
  nsTransparencyMode GetWindowTranslucencyInner() const {
    return mTransparencyMode;
  }
  void UpdateGlass();
  bool IsSimulatedClientArea(int32_t clientX, int32_t clientY);
  bool IsWindowButton(int32_t hitTestResult);

  bool DispatchTouchEventFromWMPointer(UINT msg, LPARAM aLParam,
                                       const WinPointerInfo& aPointerInfo,
                                       mozilla::MouseButton aButton);

  static bool IsAsyncResponseEvent(UINT aMsg, LRESULT& aResult);
  void IPCWindowProcHandler(UINT& msg, WPARAM& wParam, LPARAM& lParam);

  /**
   * Misc.
   */
  void StopFlashing();
  static HWND WindowAtMouse();
  static bool IsTopLevelMouseExit(HWND aWnd);
  LayoutDeviceIntRegion GetRegionToPaint(bool aForceFullRepaint, PAINTSTRUCT ps,
                                         HDC aDC);
  nsIWidgetListener* GetPaintListener();

  void AddWindowOverlayWebRenderCommands(
      mozilla::layers::WebRenderBridgeChild* aWrBridge,
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResourceUpdates) override;

  void CreateCompositor() override;
  void DestroyCompositor() override;
  void RequestFxrOutput() override;

  void RecreateDirectManipulationIfNeeded();
  void ResizeDirectManipulationViewport();
  void DestroyDirectManipulation();

  bool NeedsToTrackWindowOcclusionState();

  void AsyncUpdateWorkspaceID(Desktop& aDesktop);

  static bool HasBogusPopupsDropShadowOnMultiMonitor();

  static void InitMouseWheelScrollData();

  void ChangedDPI();

  static bool InitTouchInjection();

  bool InjectTouchPoint(uint32_t aId, LayoutDeviceIntPoint& aPoint,
                        POINTER_FLAGS aFlags, uint32_t aPressure = 1024,
                        uint32_t aOrientation = 90);

  void OnFullscreenWillChange(bool aFullScreen);
  void OnFullscreenChanged(bool aFullScreen);

  static bool sTouchInjectInitialized;
  static InjectTouchInputPtr sInjectTouchFuncPtr;
  static uint32_t sInstanceCount;
  static TriStateBool sCanQuit;
  static nsWindow* sCurrentWindow;
  static BOOL sIsOleInitialized;
  static Cursor sCurrentCursor;
  static bool sSwitchKeyboardLayout;
  static bool sJustGotDeactivate;
  static bool sJustGotActivate;
  static bool sIsInMouseCapture;
  static bool sHaveInitializedPrefs;
  static bool sIsRestoringSession;
  static bool sFirstTopLevelWindowCreated;

  // Always use the helper method to read this property.  See bug 603793.
  static TriStateBool sHasBogusPopupsDropShadowOnMultiMonitor;

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

  static bool sNeedsToInitMouseWheelSettings;

  nsClassHashtable<nsUint32HashKey, PointerInfo> mActivePointers;

  // This is used by SynthesizeNativeTouchPoint to maintain state between
  // multiple synthesized points, in the case where we can't call InjectTouch
  // directly.
  mozilla::UniquePtr<mozilla::MultiTouchInput> mSynthesizedTouchInput;

  InputContext mInputContext;

  nsCOMPtr<nsIWidget> mParent;
  nsIntSize mLastSize = nsIntSize(0, 0);
  nsIntPoint mLastPoint;
  HWND mWnd = nullptr;
  HWND mTransitionWnd = nullptr;
  WNDPROC mPrevWndProc = nullptr;
  HBRUSH mBrush;
  IMEContext mDefaultIMC;
  HDEVNOTIFY mDeviceNotifyHandle = nullptr;
  bool mIsTopWidgetWindow = false;
  bool mInDtor = false;
  bool mIsVisible = false;
  bool mPainting = false;
  bool mTouchWindow = false;
  bool mDisplayPanFeedback = false;
  bool mHideChrome = false;
  bool mIsRTL;
  bool mMousePresent = false;
  bool mSimulatedClientArea = false;
  bool mDestroyCalled = false;
  bool mOpeningAnimationSuppressed;
  bool mAlwaysOnTop;
  bool mIsEarlyBlankWindow = false;
  bool mIsShowingPreXULSkeletonUI = false;
  bool mResizable = false;
  bool mForMenupopupFrame = false;
  DWORD_PTR mOldStyle = 0;
  DWORD_PTR mOldExStyle = 0;
  nsNativeDragTarget* mNativeDragTarget = nullptr;
  HKL mLastKeyboardLayout = 0;
  mozilla::CheckInvariantWrapper<FrameState> mFrameState;
  WindowHook mWindowHook;
  uint32_t mPickerDisplayCount = 0;
  HICON mIconSmall = nullptr;
  HICON mIconBig = nullptr;
  HWND mLastKillFocusWindow = nullptr;
  PlatformCompositorWidgetDelegate* mCompositorWidgetDelegate = nullptr;

  // Non-client margin settings
  // Pre-calculated outward offset applied to default frames
  LayoutDeviceIntMargin mNonClientOffset;
  // Margins set by the owner
  LayoutDeviceIntMargin mNonClientMargins;
  // Margins we'd like to set once chrome is reshown:
  LayoutDeviceIntMargin mFutureMarginsOnceChromeShows;
  // Indicates we need to apply margins once toggling chrome into showing:
  bool mFutureMarginsToUse = false;

  // Indicates custom frames are enabled
  bool mCustomNonClient = false;
  // Indicates custom resize margins are in effect
  bool mUseResizeMarginOverrides = false;
  // Width of the left and right portions of the resize region
  int32_t mHorResizeMargin;
  // Height of the top and bottom portions of the resize region
  int32_t mVertResizeMargin;
  // Height of the caption plus border
  int32_t mCaptionHeight;

  // not yet set, will be calculated on first use
  double mDefaultScale = -1.0;

  // not yet set, will be calculated on first use
  float mAspectRatio = 0.0;

  nsCOMPtr<nsIUserIdleServiceInternal> mIdleService;

  // Draggable titlebar region maintained by UpdateWindowDraggingRegion
  LayoutDeviceIntRegion mDraggableRegion;

  // Graphics
  HDC mPaintDC = nullptr;  // only set during painting

  LayoutDeviceIntRect mLastPaintBounds;

  ResizeState mResizeState = NOT_RESIZING;

  // Transparency
  nsTransparencyMode mTransparencyMode = eTransparencyOpaque;
  nsIntRegion mPossiblyTransparentRegion;
  MARGINS mGlassMargins = {0, 0, 0, 0};

  // Win7 Gesture processing and management
  nsWinGesture mGesture;

  // Weak ref to the nsITaskbarWindowPreview associated with this window
  nsWeakPtr mTaskbarPreview = nullptr;
  // True if the taskbar (possibly through the tab preview) tells us that the
  // icon has been created on the taskbar.
  bool mHasTaskbarIconBeenCreated = false;

  // Indicates that mouse events should be ignored and pass through to the
  // window below. This is currently only used for popups.
  bool mMouseTransparent = false;

  // Whether we're in the process of sending a WM_SETTEXT ourselves
  bool mSendingSetText = false;

  // Whether we we're created as a child window (aka ChildWindow) or not.
  bool mIsChildWindow : 1;

  int32_t mCachedHitTestResult = 0;

  // The point in time at which the last paint completed. We use this to avoid
  //  painting too rapidly in response to frequent input events.
  TimeStamp mLastPaintEndTime;

  // The location of the window buttons in the window.
  mozilla::Maybe<LayoutDeviceIntRect> mWindowButtonsRect;

  // Caching for hit test results
  POINT mCachedHitTestPoint = {0, 0};
  TimeStamp mCachedHitTestTime;

  RefPtr<mozilla::widget::InProcessWinCompositorWidget> mBasicLayersSurface;

  double mSizeConstraintsScale;  // scale in effect when setting constraints

  // Will be calculated when layer manager is created.
  int32_t mMaxTextureSize = -1;

  // Pointer events processing and management
  WinPointerEvents mPointerEvents;

  ScreenPoint mLastPanGestureFocus;

  // When true, used to indicate an async call to RequestFxrOutput to the GPU
  // process after the Compositor is created
  bool mRequestFxrOutputPending = false;

  mozilla::UniquePtr<mozilla::widget::DirectManipulationOwner> mDmOwner;

  // Client rect for minimize, maximize and close buttons.
  mozilla::EnumeratedArray<WindowButtonType, WindowButtonType::Count,
                           LayoutDeviceIntRect>
      mWindowBtnRect;

  mozilla::DataMutex<Desktop> mDesktopId;

  friend class nsWindowGfx;
};

#endif  // WIDGET_WINDOWS_NSWINDOW_H_
