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
#include "nsdefs.h"
#include "nsIdleService.h"
#include "nsToolkit.h"
#include "nsString.h"
#include "nsTArray.h"
#include "gfxWindowsSurface.h"
#include "nsWindowDbg.h"
#include "cairo.h"
#include "nsITimer.h"
#include "mozilla/TimeStamp.h"

#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"
#endif

#include "nsWinGesture.h"

#include "WindowHook.h"
#include "TaskbarWindowPreview.h"

#ifdef ACCESSIBILITY
#include "OLEACC.H"
#include "mozilla/a11y/Accessible.h"
#endif

#include "nsUXThemeData.h"

#include "nsIDOMMouseEvent.h"

/**
 * Forward class definitions
 */

class nsNativeDragTarget;
class nsIRollupListener;
class nsIFile;
class imgIContainer;

namespace mozilla {
namespace widget {
class NativeKey;
} // namespace widget
} // namespacw mozilla;

/**
 * Native WIN32 window wrapper.
 */

class nsWindow : public nsBaseWidget
{
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;
  typedef mozilla::widget::WindowHook WindowHook;
  typedef mozilla::widget::TaskbarWindowPreview TaskbarWindowPreview;
  typedef mozilla::widget::NativeKey NativeKey;
public:
  nsWindow();
  virtual ~nsWindow();

  NS_DECL_ISUPPORTS_INHERITED

  friend class nsWindowGfx;

  /**
   * nsIWidget interface
   */
  NS_IMETHOD              Create(nsIWidget *aParent,
                                 nsNativeWidget aNativeParent,
                                 const nsIntRect &aRect,
                                 EVENT_CALLBACK aHandleEventFunction,
                                 nsDeviceContext *aContext,
                                 nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD              Destroy();
  NS_IMETHOD              SetParent(nsIWidget *aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual float           GetDPI();
  NS_IMETHOD              Show(bool bState);
  NS_IMETHOD              IsVisible(bool & aState);
  NS_IMETHOD              ConstrainPosition(bool aAllowSlop, PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD              Resize(PRInt32 aWidth, PRInt32 aHeight, bool aRepaint);
  NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, bool aRepaint);
  NS_IMETHOD              BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical);
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement, nsIWidget *aWidget, bool aActivate);
  NS_IMETHOD              SetSizeMode(PRInt32 aMode);
  NS_IMETHOD              Enable(bool aState);
  NS_IMETHOD              IsEnabled(bool *aState);
  NS_IMETHOD              SetFocus(bool aRaise);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);
  NS_IMETHOD              GetScreenBounds(nsIntRect &aRect);
  NS_IMETHOD              GetClientBounds(nsIntRect &aRect);
  virtual nsIntPoint      GetClientOffset();
  NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    PRUint32 aHotspotX, PRUint32 aHotspotY);
  NS_IMETHOD              SetCursor(nsCursor aCursor);
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  NS_IMETHOD              MakeFullScreen(bool aFullScreen);
  NS_IMETHOD              HideWindowChrome(bool aShouldHide);
  NS_IMETHOD              Invalidate(bool aEraseBackground = false,
                                     bool aUpdateNCArea = false,
                                     bool aIncludeChildren = false);
  NS_IMETHOD              Invalidate(const nsIntRect & aRect);
  virtual void*           GetNativeData(PRUint32 aDataType);
  virtual void            FreeNativeData(void * data, PRUint32 aDataType);
  NS_IMETHOD              SetTitle(const nsAString& aTitle);
  NS_IMETHOD              SetIcon(const nsAString& aIconSpec);
  virtual nsIntPoint      WidgetToScreenOffset();
  virtual nsIntSize       ClientToWindowSize(const nsIntSize& aClientSize);
  NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
  NS_IMETHOD              EnableDragDrop(bool aEnable);
  NS_IMETHOD              CaptureMouse(bool aCapture);
  NS_IMETHOD              CaptureRollupEvents(nsIRollupListener * aListener,
                                              bool aDoCapture, bool aConsumeRollupEvent);
  NS_IMETHOD              GetAttention(PRInt32 aCycleCount);
  virtual bool            HasPendingInputEvent();
  virtual LayerManager*   GetLayerManager(PLayersChild* aShadowManager = nsnull,
                                          LayersBackend aBackendHint = LayerManager::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nsnull);
  gfxASurface             *GetThebesSurface();
  NS_IMETHOD              OnDefaultButtonLoaded(const nsIntRect &aButtonRect);
  NS_IMETHOD              OverrideSystemMouseScrollSpeed(PRInt32 aOriginalDelta, bool aIsHorizontal, PRInt32 &aOverriddenDelta);

  virtual nsresult        SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                                   PRInt32 aNativeKeyCode,
                                                   PRUint32 aModifierFlags,
                                                   const nsAString& aCharacters,
                                                   const nsAString& aUnmodifiedCharacters);
  virtual nsresult        SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                     PRUint32 aNativeMessage,
                                                     PRUint32 aModifierFlags);

  virtual nsresult        SynthesizeNativeMouseMove(nsIntPoint aPoint)
                          { return SynthesizeNativeMouseEvent(aPoint, MOUSEEVENTF_MOVE, 0); }

  virtual nsresult        SynthesizeNativeMouseScrollEvent(nsIntPoint aPoint,
                                                           PRUint32 aNativeMessage,
                                                           double aDeltaX,
                                                           double aDeltaY,
                                                           double aDeltaZ,
                                                           PRUint32 aModifierFlags,
                                                           PRUint32 aAdditionalFlags);
  NS_IMETHOD              ResetInputState();
  NS_IMETHOD_(void)       SetInputContext(const InputContext& aContext,
                                          const InputContextAction& aAction);
  NS_IMETHOD_(InputContext) GetInputContext();
  NS_IMETHOD              CancelIMEComposition();
  NS_IMETHOD              GetToggledKeyState(PRUint32 aKeyCode, bool* aLEDState);
  NS_IMETHOD              RegisterTouchWindow();
  NS_IMETHOD              UnregisterTouchWindow();
#ifdef MOZ_XUL
  virtual void            SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();
  virtual void            UpdateOpaqueRegion(const nsIntRegion& aOpaqueRegion);
#endif // MOZ_XUL
#ifdef NS_ENABLE_TSF
  NS_IMETHOD              OnIMEFocusChange(bool aFocus);
  NS_IMETHOD              OnIMETextChange(PRUint32 aStart, PRUint32 aOldEnd, PRUint32 aNewEnd);
  NS_IMETHOD              OnIMESelectionChange(void);
#endif // NS_ENABLE_TSF
  NS_IMETHOD              GetNonClientMargins(nsIntMargin &margins);
  NS_IMETHOD              SetNonClientMargins(nsIntMargin &margins);
  void                    SetDrawsInTitlebar(bool aState);

  /**
   * Event helpers
   */
  void                    InitEvent(nsGUIEvent& event, nsIntPoint* aPoint = nsnull);
  virtual bool            DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam,
                                             LPARAM lParam,
                                             bool aIsContextMenuKey = false,
                                             PRInt16 aButton = nsMouseEvent::eLeftButton,
                                             PRUint16 aInputSource = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE);
  virtual bool            DispatchWindowEvent(nsGUIEvent* event);
  virtual bool            DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus);
  void                    InitKeyEvent(nsKeyEvent& aKeyEvent,
                                       const NativeKey& aNativeKey,
                                       const nsModifierKeyState &aModKeyState);
  virtual bool            DispatchKeyEvent(nsKeyEvent& aKeyEvent,
                                           const MSG *aMsgSentToPlugin);
  void                    DispatchPendingEvents();
  bool                    DispatchPluginEvent(UINT aMessage,
                                              WPARAM aWParam,
                                              LPARAM aLParam,
                                              bool aDispatchPendingEvents);

  void                    SuppressBlurEvents(bool aSuppress); // Called from nsFilePicker
  bool                    BlurEventsSuppressed();
#ifdef ACCESSIBILITY
  Accessible* DispatchAccessibleEvent(PRUint32 aEventType);
  Accessible* GetRootAccessible();
#endif // ACCESSIBILITY

  /**
   * Window utilities
   */
  nsWindow*               GetTopLevelWindow(bool aStopOnDialogOrPopup);
  HWND                    GetWindowHandle() { return mWnd; }
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
  nsIntPoint*             GetLastPoint() { return &mLastPoint; }
  // needed in nsIMM32Handler.cpp
  bool                    PluginHasFocus()
  {
    return (mInputContext.mIMEState.mEnabled == IMEState::PLUGIN);
  }
  bool                    IsTopLevelWidget() { return mIsTopWidgetWindow; }
  /**
   * Start allowing Direct3D9 to be used by widgets when GetLayerManager is
   * called.
   *
   * @param aReinitialize Call GetLayerManager on widgets to ensure D3D9 is
   *                      initialized, this is usually called when this function
   *                      is triggered by timeout and not user/web interaction.
   */
  static void             StartAllowingD3D9(bool aReinitialize);

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

  static void             SetupKeyModifiersSequence(nsTArray<KeyPair>* aArray, PRUint32 aModifiers);
protected:

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
  static void             AllowD3D9Callback(nsWindow *aWindow);
  static void             AllowD3D9WithReinitializeCallback(nsWindow *aWindow);

  /**
   * Window utilities
   */
  LPARAM                  lParamToScreen(LPARAM lParam);
  LPARAM                  lParamToClient(LPARAM lParam);
  virtual void            SubclassWindow(BOOL bState);
  bool                    CanTakeFocus();
  bool                    UpdateNonClientMargins(PRInt32 aSizeMode = -1, bool aReflowWindow = true);
  void                    UpdateGetWindowInfoCaptionStatus(bool aActiveCaption);
  void                    ResetLayout();
  void                    InvalidateNonClientRegion();
  HRGN                    ExcludeNonClientFromPaintRegion(HRGN aRegion);
  static void             GetMainWindowClass(nsAString& aClass);
  bool                    HasGlass() const {
    return mTransparencyMode == eTransparencyGlass ||
           mTransparencyMode == eTransparencyBorderlessGlass;
  }

  /**
   * Event processing helpers
   */
  bool                    DispatchPluginEvent(const MSG &aMsg);
  bool                    DispatchFocusToTopLevelWindow(PRUint32 aEventType);
  bool                    DispatchFocus(PRUint32 aEventType);
  bool                    DispatchStandardEvent(PRUint32 aMsg);
  bool                    DispatchCommandEvent(PRUint32 aEventCommand);
  void                    RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);
  static void             RemoveNextCharMessage(HWND aWnd);
  void                    RemoveMessageAndDispatchPluginEvent(UINT aFirstMsg,
                            UINT aLastMsg,
                            nsFakeCharMessage* aFakeCharMessage = nsnull);
  virtual bool            ProcessMessage(UINT msg, WPARAM &wParam,
                                         LPARAM &lParam, LRESULT *aRetValue);
  bool                    ProcessMessageForPlugin(const MSG &aMsg,
                                                  LRESULT *aRetValue, bool &aCallDefWndProc);
  LRESULT                 ProcessCharMessage(const MSG &aMsg,
                                             bool *aEventDispatched);
  LRESULT                 ProcessKeyUpMessage(const MSG &aMsg,
                                              bool *aEventDispatched);
  LRESULT                 ProcessKeyDownMessage(const MSG &aMsg,
                                                bool *aEventDispatched);
  static bool             EventIsInsideWindow(UINT Msg, nsWindow* aWindow);
  // Convert nsEventStatus value to a windows boolean
  static bool             ConvertStatus(nsEventStatus aStatus);
  static void             PostSleepWakeNotification(const bool aIsSleepMode);
  PRInt32                 ClientMarginHitTestPoint(PRInt32 mx, PRInt32 my);
  static bool             IsRedirectedKeyDownMessage(const MSG &aMsg);
  static void             ForgetRedirectedKeyDownMessage()
  {
    sRedirectedKeyDown.message = WM_NULL;
  }

  /**
   * Event handlers
   */
  virtual void            OnDestroy();
  virtual bool            OnMove(PRInt32 aX, PRInt32 aY);
  virtual bool            OnResize(nsIntRect &aWindowRect);
  /**
   * @param aVirtualKeyCode     If caller knows which key exactly caused the
   *                            aMsg, set the virtual key code.
   *                            Otherwise, 0.
   * @param aScanCode           If aVirutalKeyCode isn't 0, set the scan code.
   */
  LRESULT                 OnChar(const MSG &aMsg,
                                 const NativeKey& aNativeKey,
                                 nsModifierKeyState &aModKeyState,
                                 bool *aEventDispatched,
                                 PRUint32 aFlags = 0);
  LRESULT                 OnKeyDown(const MSG &aMsg,
                                    nsModifierKeyState &aModKeyState,
                                    bool *aEventDispatched,
                                    nsFakeCharMessage* aFakeCharMessage);
  LRESULT                 OnKeyUp(const MSG &aMsg,
                                  nsModifierKeyState &aModKeyState,
                                  bool *aEventDispatched);
  bool                    OnGesture(WPARAM wParam, LPARAM lParam);
  bool                    OnTouch(WPARAM wParam, LPARAM lParam);
  bool                    OnHotKey(WPARAM wParam, LPARAM lParam);
  BOOL                    OnInputLangChange(HKL aHKL);
  bool                    OnPaint(HDC aDC, PRUint32 aNestingLevel);
  void                    OnWindowPosChanged(WINDOWPOS *wp, bool& aResult);
  void                    OnWindowPosChanging(LPWINDOWPOS& info);
  void                    OnSysColorChanged();

  /**
   * Function that registers when the user has been active (used for detecting
   * when the user is idle).
   */
  void                    UserActivity();

  PRInt32                 GetHeight(PRInt32 aProposedHeight);
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
  static BOOL             DealWithPopups(HWND inWnd, UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult);

  /**
   * Window transparency helpers
   */
#ifdef MOZ_XUL
private:
  void                    SetWindowTranslucencyInner(nsTransparencyMode aMode);
  nsTransparencyMode      GetWindowTranslucencyInner() const { return mTransparencyMode; }
  void                    ResizeTranslucentWindow(PRInt32 aNewWidth, PRInt32 aNewHeight, bool force = false);
  nsresult                UpdateTranslucentWindow();
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
  nsresult                SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                              bool aIntersectWithExisting);
  nsIntRegion             GetRegionToPaint(bool aForceFullRepaint, 
                                           PAINTSTRUCT ps, HDC aDC);
  static void             ActivateOtherWindowHelper(HWND aWnd);
  void                    ClearCachedResources();

  nsPopupType PopupType() { return mPopupType; }

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
  PRUint32              mBlurSuppressLevel;
  DWORD_PTR             mOldStyle;
  DWORD_PTR             mOldExStyle;
  InputContext mInputContext;
  nsNativeDragTarget*   mNativeDragTarget;
  HKL                   mLastKeyboardLayout;
  nsPopupType           mPopupType;
  nsSizeMode            mOldSizeMode;
  nsSizeMode            mLastSizeMode;
  WindowHook            mWindowHook;
  DWORD                 mAssumeWheelIsZoomUntil;
  PRUint32              mPickerDisplayCount;
  static bool           sDropShadowEnabled;
  static PRUint32       sInstanceCount;
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
  static bool           sAllowD3D9;

  // Always use the helper method to read this property.  See bug 603793.
  static TriStateBool   sHasBogusPopupsDropShadowOnMultiMonitor;
  static bool           HasBogusPopupsDropShadowOnMultiMonitor();

  static PRUint32       sOOPPPluginFocusEvent;

  // Non-client margin settings
  // Pre-calculated outward offset applied to default frames
  nsIntMargin           mNonClientOffset;
  // Margins set by the owner
  nsIntMargin           mNonClientMargins;

  // Indicates custom frames are enabled
  bool                  mCustomNonClient;
  // Cached copy of L&F's resize border  
  PRInt32               mHorResizeMargin;
  PRInt32               mVertResizeMargin;
  // Height of the caption plus border
  PRInt32               mCaptionHeight;

  nsCOMPtr<nsIdleService> mIdleService;

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

  // Rollup Listener
  static nsIWidget*     sRollupWidget;
  static bool           sRollupConsumeEvent;
  static nsIRollupListener* sRollupListener;

  // Mouse Clicks - static variable definitions for figuring
  // out 1 - 3 Clicks.
  static POINT          sLastMousePoint;
  static POINT          sLastMouseMovePoint;
  static LONG           sLastMouseDownTime;
  static LONG           sLastClickCount;
  static BYTE           sLastMouseButton;

  // Graphics
  HDC                   mPaintDC; // only set during painting

#ifdef CAIRO_HAS_D2D_SURFACE
  nsRefPtr<gfxD2DSurface>    mD2DWindowSurface; // Surface for this window.
#endif

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

  // The point in time at which the last paint completed. We use this to avoid
  //  painting too rapidly in response to frequent input events.
  TimeStamp mLastPaintEndTime;

  // sRedirectedKeyDown is WM_KEYDOWN message or WM_SYSKEYDOWN message which
  // was reirected to SendInput() API by OnKeyDown().
  static MSG            sRedirectedKeyDown;

  static bool sNeedsToInitMouseWheelSettings;
  static void InitMouseWheelScrollData();

  // If a window receives WM_KEYDOWN message or WM_SYSKEYDOWM message which is
  // redirected message, OnKeyDowm() prevents to dispatch NS_KEY_DOWN event
  // because it has been dispatched before the message was redirected.
  // However, in some cases, ProcessKeyDownMessage() doesn't call OnKeyDown().
  // Then, ProcessKeyDownMessage() needs to forget the redirected message and
  // remove WM_CHAR message or WM_SYSCHAR message for the redirected keydown
  // message.  AutoForgetRedirectedKeyDownMessage struct is a helper struct
  // for doing that.  This must be created in stack.
  struct AutoForgetRedirectedKeyDownMessage
  {
    AutoForgetRedirectedKeyDownMessage(nsWindow* aWindow, const MSG &aMsg) :
      mCancel(!nsWindow::IsRedirectedKeyDownMessage(aMsg)),
      mWindow(aWindow), mMsg(aMsg)
    {
    }

    ~AutoForgetRedirectedKeyDownMessage()
    {
      if (mCancel) {
        return;
      }
      // Prevent unnecessary keypress event
      if (!mWindow->mOnDestroyCalled) {
        nsWindow::RemoveNextCharMessage(mWindow->mWnd);
      }
      // Foreget the redirected message
      nsWindow::ForgetRedirectedKeyDownMessage();
    }

    bool mCancel;
    nsRefPtr<nsWindow> mWindow;
    const MSG &mMsg;
  };
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
