/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Window_h__
#define Window_h__

/*
 * nsWindow - Native window management and event handling.
 */

#include "nsAutoPtr.h"
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

#include "nsWinGesture.h"

#include "WindowHook.h"
#include "TaskbarWindowPreview.h"

#ifdef ACCESSIBILITY
#include "oleacc.h"
#include "mozilla/a11y/Accessible.h"
#endif

#include "nsUXThemeData.h"
#include "nsIDOMMouseEvent.h"
#include "nsIIdleServiceInternal.h"

/**
 * Forward class definitions
 */

class nsNativeDragTarget;
class nsIRollupListener;
class nsIntRegion;
class imgIContainer;

namespace mozilla {
namespace widget {
class NativeKey;
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

public:
  nsWindow();

  NS_DECL_ISUPPORTS_INHERITED

  friend class nsWindowGfx;

  // nsWindowBase
  virtual void InitEvent(mozilla::WidgetGUIEvent& aEvent,
                         nsIntPoint* aPoint = nullptr) override;
  virtual bool DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent) override;
  virtual bool DispatchKeyboardEvent(mozilla::WidgetKeyboardEvent* aEvent) override;
  virtual bool DispatchWheelEvent(mozilla::WidgetWheelEvent* aEvent) override;
  virtual bool DispatchContentCommandEvent(mozilla::WidgetContentCommandEvent* aEvent) override;
  virtual nsWindowBase* GetParentWindowBase(bool aIncludeOwner) override;
  virtual bool IsTopLevelWidget() override { return mIsTopWidgetWindow; }

  using nsWindowBase::DispatchPluginEvent;

  // nsIWidget interface
  NS_IMETHOD              Create(nsIWidget *aParent,
                                 nsNativeWidget aNativeParent,
                                 const nsIntRect &aRect,
                                 nsWidgetInitData *aInitData = nullptr);
  NS_IMETHOD              Destroy();
  NS_IMETHOD              SetParent(nsIWidget *aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual float           GetDPI();
  virtual double          GetDefaultScaleInternal();
  NS_IMETHOD              Show(bool bState);
  virtual bool            IsVisible() const;
  NS_IMETHOD              ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY);
  virtual void            SetSizeConstraints(const SizeConstraints& aConstraints);
  NS_IMETHOD              Move(double aX, double aY);
  NS_IMETHOD              Resize(double aWidth, double aHeight, bool aRepaint);
  NS_IMETHOD              Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint);
  NS_IMETHOD              BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                          int32_t aHorizontal,
                                          int32_t aVertical);
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement, nsIWidget *aWidget, bool aActivate);
  NS_IMETHOD              SetSizeMode(int32_t aMode);
  NS_IMETHOD              Enable(bool aState);
  virtual bool            IsEnabled() const;
  NS_IMETHOD              SetFocus(bool aRaise);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);
  NS_IMETHOD              GetScreenBounds(nsIntRect &aRect);
  NS_IMETHOD              GetRestoredBounds(nsIntRect &aRect) override;
  NS_IMETHOD              GetClientBounds(nsIntRect &aRect);
  virtual nsIntPoint      GetClientOffset();
  void                    SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    uint32_t aHotspotX, uint32_t aHotspotY);
  NS_IMETHOD              SetCursor(nsCursor aCursor);
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  NS_IMETHOD              MakeFullScreen(bool aFullScreen, nsIScreen* aScreen = nullptr);
  NS_IMETHOD              HideWindowChrome(bool aShouldHide);
  NS_IMETHOD              Invalidate(bool aEraseBackground = false,
                                     bool aUpdateNCArea = false,
                                     bool aIncludeChildren = false);
  NS_IMETHOD              Invalidate(const nsIntRect & aRect);
  virtual void*           GetNativeData(uint32_t aDataType);
  virtual void            FreeNativeData(void * data, uint32_t aDataType);
  NS_IMETHOD              SetTitle(const nsAString& aTitle);
  NS_IMETHOD              SetIcon(const nsAString& aIconSpec);
  virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset();
  virtual mozilla::LayoutDeviceIntSize ClientToWindowSize(const mozilla::LayoutDeviceIntSize& aClientSize) override;
  NS_IMETHOD              DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                        nsEventStatus& aStatus);
  NS_IMETHOD              EnableDragDrop(bool aEnable);
  NS_IMETHOD              CaptureMouse(bool aCapture);
  NS_IMETHOD              CaptureRollupEvents(nsIRollupListener * aListener,
                                              bool aDoCapture);
  NS_IMETHOD              GetAttention(int32_t aCycleCount);
  virtual bool            HasPendingInputEvent();
  virtual LayerManager*   GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nullptr);
  NS_IMETHOD              OnDefaultButtonLoaded(const nsIntRect &aButtonRect);
  NS_IMETHOD              OverrideSystemMouseScrollSpeed(double aOriginalDeltaX,
                                                         double aOriginalDeltaY,
                                                         double& aOverriddenDeltaX,
                                                         double& aOverriddenDeltaY);

  virtual nsresult        SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                                   int32_t aNativeKeyCode,
                                                   uint32_t aModifierFlags,
                                                   const nsAString& aCharacters,
                                                   const nsAString& aUnmodifiedCharacters,
                                                   nsIObserver* aObserver) override;
  virtual nsresult        SynthesizeNativeMouseEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                     uint32_t aNativeMessage,
                                                     uint32_t aModifierFlags,
                                                     nsIObserver* aObserver) override;

  virtual nsresult        SynthesizeNativeMouseMove(mozilla::LayoutDeviceIntPoint aPoint,
                                                    nsIObserver* aObserver) override
                          { return SynthesizeNativeMouseEvent(aPoint, MOUSEEVENTF_MOVE, 0, aObserver); }

  virtual nsresult        SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                           uint32_t aNativeMessage,
                                                           double aDeltaX,
                                                           double aDeltaY,
                                                           double aDeltaZ,
                                                           uint32_t aModifierFlags,
                                                           uint32_t aAdditionalFlags,
                                                           nsIObserver* aObserver) override;
  NS_IMETHOD_(void)       SetInputContext(const InputContext& aContext,
                                          const InputContextAction& aAction);
  NS_IMETHOD_(InputContext) GetInputContext();
  NS_IMETHOD              GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState);
#ifdef MOZ_XUL
  virtual void            SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();
  virtual void            UpdateOpaqueRegion(const nsIntRegion& aOpaqueRegion);
#endif // MOZ_XUL
  virtual nsIMEUpdatePreference GetIMEUpdatePreference();
  NS_IMETHOD              GetNonClientMargins(nsIntMargin &margins);
  NS_IMETHOD              SetNonClientMargins(nsIntMargin &margins);
  void                    SetDrawsInTitlebar(bool aState);
  mozilla::TemporaryRef<mozilla::gfx::DrawTarget> StartRemoteDrawing() override;
  virtual void            EndRemoteDrawing() override;
  virtual void UpdateWindowDraggingRegion(const nsIntRegion& aRegion) override;

  virtual void            UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) override;
  virtual uint32_t        GetMaxTouchPoints() const override;

  /**
   * Event helpers
   */
  virtual bool            DispatchMouseEvent(uint32_t aEventType, WPARAM wParam,
                                             LPARAM lParam,
                                             bool aIsContextMenuKey = false,
                                             int16_t aButton = mozilla::WidgetMouseEvent::eLeftButton,
                                             uint16_t aInputSource = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE);
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

  static void             ClearCompositor(nsWindow* aWindow);

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

  NS_IMETHOD              ReparentNativeWidget(nsIWidget* aNewParent);

  // Open file picker tracking
  void                    PickerOpen();
  void                    PickerClosed();

  bool                    const DestroyCalled() { return mDestroyCalled; }

  virtual void GetPreferredCompositorBackends(nsTArray<mozilla::layers::LayersBackend>& aHints);

  bool IsPopup();
  virtual bool ShouldUseOffMainThreadCompositing();

protected:
  virtual ~nsWindow();

  virtual void WindowUsesOMTC() override;
  virtual void ConfigureAPZCTreeManager() override;
  void RegisterTouchWindow();

  virtual nsresult NotifyIMEInternal(
                     const IMENotification& aIMENotification) override;

  // A magic number to identify the FAKETRACKPOINTSCROLLABLE window created
  // when the trackpoint hack is enabled.
  enum { eFakeTrackPointScrollableID = 0x46545053 };

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
  static void             GetMainWindowClass(nsAString& aClass);
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
  bool                    DispatchStandardEvent(uint32_t aMsg);
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
  static TimeStamp        GetMessageTimeStamp(LONG aEventTime);
  static void             UpdateFirstEventTime(DWORD aEventTime);

  /**
   * Event handlers
   */
  virtual void            OnDestroy();
  virtual bool            OnResize(nsIntRect &aWindowRect);
  bool                    OnGesture(WPARAM wParam, LPARAM lParam);
  bool                    OnTouch(WPARAM wParam, LPARAM lParam);
  bool                    OnHotKey(WPARAM wParam, LPARAM lParam);
  bool                    OnPaint(HDC aDC, uint32_t aNestingLevel);
  void                    OnWindowPosChanged(WINDOWPOS* wp);
  void                    OnWindowPosChanging(LPWINDOWPOS& info);
  void                    OnSysColorChanged();

  /**
   * Function that registers when the user has been active (used for detecting
   * when the user is idle).
   */
  void                    UserActivity();

  int32_t                 GetHeight(int32_t aProposedHeight);
  void                    GetWindowClass(nsString& aWindowClass);
  void                    GetWindowPopupClass(nsString& aWindowClass);
  virtual DWORD           WindowStyle();
  DWORD                   WindowExStyle();

  void                    RegisterWindowClass(const nsString& aClassName,
                                              UINT aExtraStyle,
                                              LPWSTR aIconID);

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
  void                    ResizeTranslucentWindow(int32_t aNewWidth, int32_t aNewHeight, bool force = false);
  nsresult                UpdateTranslucentWindow();
  void                    ClearTranslucentWindow();
  void                    SetupTranslucentWindowMemoryBitmap(nsTransparencyMode aMode);
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
  virtual nsresult        SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                              bool aIntersectWithExisting) override;
  nsIntRegion             GetRegionToPaint(bool aForceFullRepaint, 
                                           PAINTSTRUCT ps, HDC aDC);
  static void             ActivateOtherWindowHelper(HWND aWnd);
  void                    ClearCachedResources();
  nsIWidgetListener*      GetPaintListener();
  static bool             IsRenderMode(gfxWindowsPlatform::RenderMode aMode);

protected:
  nsCOMPtr<nsIWidget>   mParent;
  nsIntSize             mLastSize;
  nsIntPoint            mLastPoint;
  HWND                  mWnd;
  WNDPROC               mPrevWndProc;
  HBRUSH                mBrush;
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
  static const char*    sDefaultMainWindowClass;

  // Always use the helper method to read this property.  See bug 603793.
  static TriStateBool   sHasBogusPopupsDropShadowOnMultiMonitor;
  static bool           HasBogusPopupsDropShadowOnMultiMonitor();

  // Non-client margin settings
  // Pre-calculated outward offset applied to default frames
  nsIntMargin           mNonClientOffset;
  // Margins set by the owner
  nsIntMargin           mNonClientMargins;
  // Margins we'd like to set once chrome is reshown:
  nsIntMargin           mFutureMarginsOnceChromeShows;
  // Indicates we need to apply margins once toggling chrome into showing:
  bool                  mFutureMarginsToUse;

  // Indicates custom frames are enabled
  bool                  mCustomNonClient;
  // Cached copy of L&F's resize border  
  int32_t               mHorResizeMargin;
  int32_t               mVertResizeMargin;
  // Height of the caption plus border
  int32_t               mCaptionHeight;

  nsCOMPtr<nsIIdleServiceInternal> mIdleService;

  // Draggable titlebar region maintained by UpdateWindowDraggingRegion
  nsIntRegion mDraggableRegion;

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
  HDC                   mCompositeDC; // only set during StartRemoteDrawing

  nsIntRect             mLastPaintBounds;

  // Transparency
#ifdef MOZ_XUL
  // Use layered windows to support full 256 level alpha translucency
  nsRefPtr<gfxASurface> mTransparentSurface;
  HDC                   mMemoryDC;
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

  // The point in time at which the last paint completed. We use this to avoid
  //  painting too rapidly in response to frequent input events.
  TimeStamp mLastPaintEndTime;

  // Caching for hit test results
  POINT mCachedHitTestPoint;
  TimeStamp mCachedHitTestTime;
  int32_t mCachedHitTestResult;

  // For converting native event times to timestamps we record the time of the
  // first received event in each time scale.
  static DWORD     sFirstEventTime;
  static TimeStamp sFirstEventTimeStamp;

  static bool sNeedsToInitMouseWheelSettings;
  static void InitMouseWheelScrollData();
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
