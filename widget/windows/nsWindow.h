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
#include "nsWindowBase.h"
#include "nsdefs.h"
#include "nsIdleService.h"
#include "nsToolkit.h"
#include "nsString.h"
#include "nsTArray.h"
#include "gfxWindowsPlatform.h"
#include "gfxWindowsSurface.h"
#include "nsWindowDbg.h"
#include "cairo.h"
#include "nsITimer.h"
#include "nsRegion.h"
#include "mozilla/EventForwards.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TimeStamp.h"
#include "nsMargin.h"
#include "nsRegionFwd.h"

#include "nsWinGesture.h"
#include "WinUtils.h"
#include "WindowHook.h"
#include "TaskbarWindowPreview.h"

#ifdef ACCESSIBILITY
#include "oleacc.h"
#include "mozilla/a11y/Accessible.h"
#endif

#include "nsUXThemeData.h"
#include "nsIDOMMouseEvent.h"
#include "nsIIdleServiceInternal.h"

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
class WinCompositorWidget;
struct MSGResult;
} // namespace widget
} // namespacw mozilla;

/**
 * Native WIN32 window wrapper.
 */

class nsWindow : public nsWindowBase
{
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;
  typedef mozilla::widget::WindowHook WindowHook;
  typedef mozilla::widget::TaskbarWindowPreview TaskbarWindowPreview;
  typedef mozilla::widget::NativeKey NativeKey;
  typedef mozilla::widget::MSGResult MSGResult;
  typedef mozilla::widget::IMEContext IMEContext;

public:
  nsWindow();

  NS_DECL_ISUPPORTS_INHERITED

  friend class nsWindowGfx;

  // nsWindowBase
  virtual void InitEvent(mozilla::WidgetGUIEvent& aEvent,
                         LayoutDeviceIntPoint* aPoint = nullptr) override;
  virtual WidgetEventTime CurrentMessageWidgetEventTime() const override;
  virtual bool DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent) override;
  virtual bool DispatchKeyboardEvent(mozilla::WidgetKeyboardEvent* aEvent) override;
  virtual bool DispatchWheelEvent(mozilla::WidgetWheelEvent* aEvent) override;
  virtual bool DispatchContentCommandEvent(mozilla::WidgetContentCommandEvent* aEvent) override;
  virtual nsWindowBase* GetParentWindowBase(bool aIncludeOwner) override;
  virtual bool IsTopLevelWidget() override { return mIsTopWidgetWindow; }

  using nsWindowBase::DispatchPluginEvent;

  // nsIWidget interface
  using nsWindowBase::Create; // for Create signature not overridden here
  NS_IMETHOD              Create(nsIWidget* aParent,
                                 nsNativeWidget aNativeParent,
                                 const LayoutDeviceIntRect& aRect,
                                 nsWidgetInitData* aInitData = nullptr) override;
  NS_IMETHOD              Destroy() override;
  NS_IMETHOD              SetParent(nsIWidget *aNewParent) override;
  virtual nsIWidget*      GetParent(void) override;
  virtual float           GetDPI() override;
  double                  GetDefaultScaleInternal() final;
  int32_t                 LogToPhys(double aValue) final;
  mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() final
  {
    if (mozilla::widget::WinUtils::IsPerMonitorDPIAware()) {
      return mozilla::DesktopToLayoutDeviceScale(1.0);
    } else {
      return mozilla::DesktopToLayoutDeviceScale(GetDefaultScaleInternal());
    }
  }

  NS_IMETHOD              Show(bool bState) override;
  virtual bool            IsVisible() const override;
  NS_IMETHOD              ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY) override;
  virtual void            SetSizeConstraints(const SizeConstraints& aConstraints) override;
  virtual const SizeConstraints GetSizeConstraints() override;
  NS_IMETHOD              Move(double aX, double aY) override;
  NS_IMETHOD              Resize(double aWidth, double aHeight, bool aRepaint) override;
  NS_IMETHOD              Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) override;
  NS_IMETHOD              BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                          int32_t aHorizontal,
                                          int32_t aVertical) override;
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement, nsIWidget *aWidget, bool aActivate) override;
  NS_IMETHOD              SetSizeMode(nsSizeMode aMode) override;
  NS_IMETHOD              Enable(bool aState) override;
  virtual bool            IsEnabled() const override;
  NS_IMETHOD              SetFocus(bool aRaise) override;
  NS_IMETHOD              GetBounds(LayoutDeviceIntRect& aRect) override;
  NS_IMETHOD              GetScreenBounds(LayoutDeviceIntRect& aRect) override;
  NS_IMETHOD              GetRestoredBounds(LayoutDeviceIntRect& aRect) override;
  NS_IMETHOD              GetClientBounds(LayoutDeviceIntRect& aRect) override;
  virtual LayoutDeviceIntPoint GetClientOffset() override;
  void                    SetBackgroundColor(const nscolor& aColor) override;
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    uint32_t aHotspotX, uint32_t aHotspotY) override;
  NS_IMETHOD              SetCursor(nsCursor aCursor) override;
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override;
  virtual bool PrepareForFullscreenTransition(nsISupports** aData) override;
  virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) override;
  NS_IMETHOD              MakeFullScreen(bool aFullScreen, nsIScreen* aScreen = nullptr) override;
  NS_IMETHOD              HideWindowChrome(bool aShouldHide) override;
  NS_IMETHOD              Invalidate(bool aEraseBackground = false,
                                     bool aUpdateNCArea = false,
                                     bool aIncludeChildren = false);
  NS_IMETHOD              Invalidate(const LayoutDeviceIntRect& aRect);
  virtual void*           GetNativeData(uint32_t aDataType) override;
  void                    SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
  virtual void            FreeNativeData(void * data, uint32_t aDataType) override;
  NS_IMETHOD              SetTitle(const nsAString& aTitle) override;
  NS_IMETHOD              SetIcon(const nsAString& aIconSpec) override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual LayoutDeviceIntSize ClientToWindowSize(const LayoutDeviceIntSize& aClientSize) override;
  NS_IMETHOD              DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                        nsEventStatus& aStatus) override;
  NS_IMETHOD              EnableDragDrop(bool aEnable) override;
  NS_IMETHOD              CaptureMouse(bool aCapture) override;
  NS_IMETHOD              CaptureRollupEvents(nsIRollupListener * aListener,
                                              bool aDoCapture) override;
  NS_IMETHOD              GetAttention(int32_t aCycleCount) override;
  virtual bool            HasPendingInputEvent() override;
  virtual LayerManager*   GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;
  NS_IMETHOD              OnDefaultButtonLoaded(const LayoutDeviceIntRect& aButtonRect) override;
  virtual nsresult        SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                                   int32_t aNativeKeyCode,
                                                   uint32_t aModifierFlags,
                                                   const nsAString& aCharacters,
                                                   const nsAString& aUnmodifiedCharacters,
                                                   nsIObserver* aObserver) override;
  virtual nsresult        SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                                     uint32_t aNativeMessage,
                                                     uint32_t aModifierFlags,
                                                     nsIObserver* aObserver) override;

  virtual nsresult        SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                                    nsIObserver* aObserver) override
                          { return SynthesizeNativeMouseEvent(aPoint, MOUSEEVENTF_MOVE, 0, aObserver); }

  virtual nsresult        SynthesizeNativeMouseScrollEvent(LayoutDeviceIntPoint aPoint,
                                                           uint32_t aNativeMessage,
                                                           double aDeltaX,
                                                           double aDeltaY,
                                                           double aDeltaZ,
                                                           uint32_t aModifierFlags,
                                                           uint32_t aAdditionalFlags,
                                                           nsIObserver* aObserver) override;
  NS_IMETHOD_(void)       SetInputContext(const InputContext& aContext,
                                          const InputContextAction& aAction) override;
  NS_IMETHOD_(InputContext) GetInputContext() override;
  NS_IMETHOD_(TextEventDispatcherListener*)
    GetNativeTextEventDispatcherListener() override;
#ifdef MOZ_XUL
  virtual void            SetTransparencyMode(nsTransparencyMode aMode) override;
  virtual nsTransparencyMode GetTransparencyMode() override;
  virtual void            UpdateOpaqueRegion(const LayoutDeviceIntRegion& aOpaqueRegion) override;
#endif // MOZ_XUL
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() override;
  NS_IMETHOD              GetNonClientMargins(LayoutDeviceIntMargin& aMargins) override;
  NS_IMETHOD              SetNonClientMargins(LayoutDeviceIntMargin& aMargins) override;
  void                    SetDrawsInTitlebar(bool aState) override;
  virtual void            UpdateWindowDraggingRegion(const LayoutDeviceIntRegion& aRegion) override;

  virtual void            UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) override;
  virtual uint32_t        GetMaxTouchPoints() const override;

  /**
   * Event helpers
   */
  virtual bool            DispatchMouseEvent(
                            mozilla::EventMessage aEventMessage,
                            WPARAM wParam,
                            LPARAM lParam,
                            bool aIsContextMenuKey = false,
                            int16_t aButton =
                              mozilla::WidgetMouseEvent::eLeftButton,
                            uint16_t aInputSource =
                              nsIDOMMouseEvent::MOZ_SOURCE_MOUSE);
  virtual bool            DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent,
                                              nsEventStatus& aStatus);
  void                    DispatchPendingEvents();
  bool                    DispatchPluginEvent(UINT aMessage,
                                              WPARAM aWParam,
                                              LPARAM aLParam,
                                              bool aDispatchPendingEvents);

  void                    SuppressBlurEvents(bool aSuppress); // Called from nsFilePicker
  bool                    BlurEventsSuppressed();
#ifdef ACCESSIBILITY
  /**
   * Return an accessible associated with the window.
   */
  mozilla::a11y::Accessible* GetAccessible();
#endif // ACCESSIBILITY

  /**
   * Window utilities
   */
  nsWindow*               GetTopLevelWindow(bool aStopOnDialogOrPopup);
  WNDPROC                 GetPrevWindowProc() { return mPrevWndProc; }
  WindowHook&             GetWindowHook() { return mWindowHook; }
  nsWindow*               GetParentWindow(bool aIncludeOwner);
  // Get an array of all nsWindow*s on the main thread.
  typedef void            (WindowEnumCallback)(nsWindow*);
  static void             EnumAllWindows(WindowEnumCallback aCallback);

  /**
   * Misc.
   */
  virtual bool            AutoErase(HDC dc);
  bool ComputeShouldAccelerate() override;

  void                    ForcePresent();

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
  bool                    AssociateDefaultIMC(bool aAssociate);

  bool HasTaskbarIconBeenCreated() { return mHasTaskbarIconBeenCreated; }
  // Called when either the nsWindow or an nsITaskbarTabPreview receives the noticiation that this window
  // has its icon placed on the taskbar.
  void SetHasTaskbarIconBeenCreated(bool created = true) { mHasTaskbarIconBeenCreated = created; }

  // Getter/setter for the nsITaskbarWindowPreview for this nsWindow
  already_AddRefed<nsITaskbarWindowPreview> GetTaskbarPreview() {
    nsCOMPtr<nsITaskbarWindowPreview> preview(do_QueryReferent(mTaskbarPreview));
    return preview.forget();
  }
  void SetTaskbarPreview(nsITaskbarWindowPreview *preview) { mTaskbarPreview = do_GetWeakReference(preview); }

  NS_IMETHOD              ReparentNativeWidget(nsIWidget* aNewParent) override;

  // Open file picker tracking
  void                    PickerOpen();
  void                    PickerClosed();

  bool                    const DestroyCalled() { return mDestroyCalled; }

  bool IsPopup();
  virtual bool ShouldUseOffMainThreadCompositing() override;

  const IMEContext& DefaultIMC() const { return mDefaultIMC; }

  virtual void SetCandidateWindowForPlugin(
                 const mozilla::widget::CandidateWindowPosition&
                   aPosition) override;
  virtual void DefaultProcOfPluginEvent(
                 const mozilla::WidgetPluginEvent& aEvent) override;
  virtual nsresult OnWindowedPluginKeyEvent(
                     const mozilla::NativeEventData& aKeyEventData,
                     nsIKeyEventInPluginCallback* aCallback) override;

  void GetCompositorWidgetInitData(mozilla::widget::CompositorWidgetInitData* aInitData) override;

protected:
  virtual ~nsWindow();

  virtual void WindowUsesOMTC() override;
  virtual void RegisterTouchWindow() override;

  // A magic number to identify the FAKETRACKPOINTSCROLLABLE window created
  // when the trackpoint hack is enabled.
  enum { eFakeTrackPointScrollableID = 0x46545053 };

  // Used for displayport suppression during window resize
  enum ResizeState {
    NOT_RESIZING,
    IN_SIZEMOVE,
    RESIZING,
    MOVING
  };

  /**
   * Callbacks
   */
  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK WindowProcInternal(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

  static BOOL CALLBACK    BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg);
  static BOOL CALLBACK    BroadcastMsg(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK    DispatchStarvedPaints(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK    RegisterTouchForDescendants(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK    UnregisterTouchForDescendants(HWND aTopWindow, LPARAM aMsg);
  static LRESULT CALLBACK MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam);
  static VOID    CALLBACK HookTimerForPopups( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime );
  static BOOL    CALLBACK ClearResourcesCallback(HWND aChild, LPARAM aParam);
  static BOOL    CALLBACK EnumAllChildWindProc(HWND aWnd, LPARAM aParam);
  static BOOL    CALLBACK EnumAllThreadWindowProc(HWND aWnd, LPARAM aParam);

  /**
   * Window utilities
   */
  LPARAM                  lParamToScreen(LPARAM lParam);
  LPARAM                  lParamToClient(LPARAM lParam);
  virtual void            SubclassWindow(BOOL bState);
  bool                    CanTakeFocus();
  bool                    UpdateNonClientMargins(int32_t aSizeMode = -1, bool aReflowWindow = true);
  void                    UpdateGetWindowInfoCaptionStatus(bool aActiveCaption);
  void                    ResetLayout();
  void                    InvalidateNonClientRegion();
  HRGN                    ExcludeNonClientFromPaintRegion(HRGN aRegion);
  static const wchar_t*   GetMainWindowClass();
  bool                    HasGlass() const {
    return mTransparencyMode == eTransparencyGlass ||
           mTransparencyMode == eTransparencyBorderlessGlass;
  }
  HWND                    GetOwnerWnd() const
  {
    return ::GetWindow(mWnd, GW_OWNER);
  }
  bool                    IsOwnerForegroundWindow() const
  {
    HWND owner = GetOwnerWnd();
    return owner && owner == ::GetForegroundWindow();
  }
  bool                    IsPopup() const
  {
    return mWindowType == eWindowType_popup;
  }


  /**
   * Event processing helpers
   */
  void                    DispatchFocusToTopLevelWindow(bool aIsActivate);
  bool                    DispatchStandardEvent(mozilla::EventMessage aMsg);
  void                    RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);
  virtual bool            ProcessMessage(UINT msg, WPARAM &wParam,
                                         LPARAM &lParam, LRESULT *aRetValue);
  bool                    ExternalHandlerProcessMessage(
                                         UINT aMessage, WPARAM& aWParam,
                                         LPARAM& aLParam, MSGResult& aResult);
  bool                    ProcessMessageForPlugin(const MSG &aMsg,
                                                  MSGResult& aResult);
  LRESULT                 ProcessCharMessage(const MSG &aMsg,
                                             bool *aEventDispatched);
  LRESULT                 ProcessKeyUpMessage(const MSG &aMsg,
                                              bool *aEventDispatched);
  LRESULT                 ProcessKeyDownMessage(const MSG &aMsg,
                                                bool *aEventDispatched);
  static bool             EventIsInsideWindow(nsWindow* aWindow);
  // Convert nsEventStatus value to a windows boolean
  static bool             ConvertStatus(nsEventStatus aStatus);
  static void             PostSleepWakeNotification(const bool aIsSleepMode);
  int32_t                 ClientMarginHitTestPoint(int32_t mx, int32_t my);
  TimeStamp               GetMessageTimeStamp(LONG aEventTime) const;
  static void             UpdateFirstEventTime(DWORD aEventTime);
  void                    FinishLiveResizing(ResizeState aNewState);

  /**
   * Event handlers
   */
  virtual void            OnDestroy() override;
  virtual bool            OnResize(nsIntRect &aWindowRect);
  bool                    OnGesture(WPARAM wParam, LPARAM lParam);
  bool                    OnTouch(WPARAM wParam, LPARAM lParam);
  bool                    OnHotKey(WPARAM wParam, LPARAM lParam);
  bool                    OnPaint(HDC aDC, uint32_t aNestingLevel);
  void                    OnWindowPosChanged(WINDOWPOS* wp);
  void                    OnWindowPosChanging(LPWINDOWPOS& info);
  void                    OnSysColorChanged();
  void                    OnDPIChanged(int32_t x, int32_t y,
                                       int32_t width, int32_t height);

  /**
   * Function that registers when the user has been active (used for detecting
   * when the user is idle).
   */
  void                    UserActivity();

  int32_t                 GetHeight(int32_t aProposedHeight);
  const wchar_t*          GetWindowClass() const;
  const wchar_t*          GetWindowPopupClass() const;
  virtual DWORD           WindowStyle();
  DWORD                   WindowExStyle();

  // This method registers the given window class, and returns the class name.
  const wchar_t*          RegisterWindowClass(const wchar_t* aClassName,
                                              UINT aExtraStyle,
                                              LPWSTR aIconID) const;

  /**
   * XP and Vista theming support for windows with rounded edges
   */
  void                    ClearThemeRegion();
  void                    SetThemeRegion();

  /**
   * Popup hooks
   */
  static void             ScheduleHookTimer(HWND aWnd, UINT aMsgId);
  static void             RegisterSpecialDropdownHooks();
  static void             UnregisterSpecialDropdownHooks();
  static bool             GetPopupsToRollup(nsIRollupListener* aRollupListener,
                                            uint32_t* aPopupsToRollup);
  static bool             NeedsToHandleNCActivateDelayed(HWND aWnd);
  static bool             DealWithPopups(HWND inWnd, UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult);

  /**
   * Window transparency helpers
   */
#ifdef MOZ_XUL
private:
  void                    SetWindowTranslucencyInner(nsTransparencyMode aMode);
  nsTransparencyMode      GetWindowTranslucencyInner() const { return mTransparencyMode; }
  void                    UpdateGlass();
protected:
#endif // MOZ_XUL

  static bool             IsAsyncResponseEvent(UINT aMsg, LRESULT& aResult);
  void                    IPCWindowProcHandler(UINT& msg, WPARAM& wParam, LPARAM& lParam);

  /**
   * Misc.
   */
  void                    StopFlashing();
  static bool             IsTopLevelMouseExit(HWND aWnd);
  virtual nsresult        SetWindowClipRegion(const nsTArray<LayoutDeviceIntRect>& aRects,
                                              bool aIntersectWithExisting) override;
  nsIntRegion             GetRegionToPaint(bool aForceFullRepaint, 
                                           PAINTSTRUCT ps, HDC aDC);
  static void             ActivateOtherWindowHelper(HWND aWnd);
  void                    ClearCachedResources();
  nsIWidgetListener*      GetPaintListener();

  already_AddRefed<SourceSurface> CreateScrollSnapshot() override;

  struct ScrollSnapshot
  {
    RefPtr<gfxWindowsSurface> surface;
    bool surfaceHasSnapshot = false;
    RECT clip;
  };

  ScrollSnapshot* EnsureSnapshotSurface(ScrollSnapshot& aSnapshotData,
                                        const mozilla::gfx::IntSize& aSize);

  ScrollSnapshot mFullSnapshot;
  ScrollSnapshot mPartialSnapshot;
  ScrollSnapshot* mCurrentSnapshot = nullptr;

  already_AddRefed<SourceSurface>
    GetFallbackScrollSnapshot(const RECT& aRequiredClip);

protected:
  nsCOMPtr<nsIWidget>   mParent;
  nsIntSize             mLastSize;
  nsIntPoint            mLastPoint;
  HWND                  mWnd;
  HWND                  mTransitionWnd;
  WNDPROC               mPrevWndProc;
  HBRUSH                mBrush;
  IMEContext            mDefaultIMC;
  bool                  mIsTopWidgetWindow;
  bool                  mInDtor;
  bool                  mIsVisible;
  bool                  mUnicodeWidget;
  bool                  mPainting;
  bool                  mTouchWindow;
  bool                  mDisplayPanFeedback;
  bool                  mHideChrome;
  bool                  mIsRTL;
  bool                  mFullscreenMode;
  bool                  mMousePresent;
  bool                  mDestroyCalled;
  uint32_t              mBlurSuppressLevel;
  DWORD_PTR             mOldStyle;
  DWORD_PTR             mOldExStyle;
  nsNativeDragTarget*   mNativeDragTarget;
  HKL                   mLastKeyboardLayout;
  nsSizeMode            mOldSizeMode;
  nsSizeMode            mLastSizeMode;
  WindowHook            mWindowHook;
  uint32_t              mPickerDisplayCount;
  HICON                 mIconSmall;
  HICON                 mIconBig;
  static bool           sDropShadowEnabled;
  static uint32_t       sInstanceCount;
  static TriStateBool   sCanQuit;
  static nsWindow*      sCurrentWindow;
  static BOOL           sIsOleInitialized;
  static HCURSOR        sHCursor;
  static imgIContainer* sCursorImgContainer;
  static bool           sSwitchKeyboardLayout;
  static bool           sJustGotDeactivate;
  static bool           sJustGotActivate;
  static bool           sIsInMouseCapture;
  static int            sTrimOnMinimize;

  // Always use the helper method to read this property.  See bug 603793.
  static TriStateBool   sHasBogusPopupsDropShadowOnMultiMonitor;
  static bool           HasBogusPopupsDropShadowOnMultiMonitor();

  // Non-client margin settings
  // Pre-calculated outward offset applied to default frames
  LayoutDeviceIntMargin mNonClientOffset;
  // Margins set by the owner
  LayoutDeviceIntMargin mNonClientMargins;
  // Margins we'd like to set once chrome is reshown:
  LayoutDeviceIntMargin mFutureMarginsOnceChromeShows;
  // Indicates we need to apply margins once toggling chrome into showing:
  bool                  mFutureMarginsToUse;

  // Indicates custom frames are enabled
  bool                  mCustomNonClient;
  // Cached copy of L&F's resize border  
  int32_t               mHorResizeMargin;
  int32_t               mVertResizeMargin;
  // Height of the caption plus border
  int32_t               mCaptionHeight;

  double                mDefaultScale;

  nsCOMPtr<nsIIdleServiceInternal> mIdleService;

  // Draggable titlebar region maintained by UpdateWindowDraggingRegion
  LayoutDeviceIntRegion mDraggableRegion;

  // Hook Data Memebers for Dropdowns. sProcessHook Tells the
  // hook methods whether they should be processing the hook
  // messages.
  static HHOOK          sMsgFilterHook;
  static HHOOK          sCallProcHook;
  static HHOOK          sCallMouseHook;
  static bool           sProcessHook;
  static UINT           sRollupMsgId;
  static HWND           sRollupMsgWnd;
  static UINT           sHookTimerId;

  // Mouse Clicks - static variable definitions for figuring
  // out 1 - 3 Clicks.
  static POINT          sLastMousePoint;
  static POINT          sLastMouseMovePoint;
  static LONG           sLastMouseDownTime;
  static LONG           sLastClickCount;
  static BYTE           sLastMouseButton;

  // Graphics
  HDC                   mPaintDC; // only set during painting

  LayoutDeviceIntRect   mLastPaintBounds;

  ResizeState mResizeState;

  // Transparency
#ifdef MOZ_XUL
  nsTransparencyMode    mTransparencyMode;
  nsIntRegion           mPossiblyTransparentRegion;
  MARGINS               mGlassMargins;
#endif // MOZ_XUL

  // Win7 Gesture processing and management
  nsWinGesture          mGesture;

  // Weak ref to the nsITaskbarWindowPreview associated with this window
  nsWeakPtr             mTaskbarPreview;
  // True if the taskbar (possibly through the tab preview) tells us that the
  // icon has been created on the taskbar.
  bool                  mHasTaskbarIconBeenCreated;

  // Indicates that mouse events should be ignored and pass through to the
  // window below. This is currently only used for popups.
  bool                  mMouseTransparent;

  // Whether we're in the process of sending a WM_SETTEXT ourselves
  bool                  mSendingSetText;

  // The point in time at which the last paint completed. We use this to avoid
  //  painting too rapidly in response to frequent input events.
  TimeStamp mLastPaintEndTime;

  // Caching for hit test results
  POINT mCachedHitTestPoint;
  TimeStamp mCachedHitTestTime;
  int32_t mCachedHitTestResult;

  RefPtr<mozilla::widget::WinCompositorWidget> mBasicLayersSurface;

  static bool sNeedsToInitMouseWheelSettings;
  static void InitMouseWheelScrollData();

  double mSizeConstraintsScale; // scale in effect when setting constraints
};

/**
 * A child window is a window with different style.
 */
class ChildWindow : public nsWindow {

public:
  ChildWindow() {}

protected:
  virtual DWORD WindowStyle();
};

#endif // Window_h__
