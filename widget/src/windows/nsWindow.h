/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Makoto Kato  <m_kato@ga2.so-net.ne.jp>
 *   Dainis Jonitis <Dainis_Jonitis@swh-t.lv>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Ningjie Chen <chenn@email.uc.edu>
 *   Jim Mathies <jmathies@mozilla.com>.
 *   Mats Palmgren <matspal@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"
#endif

#include "nsWinGesture.h"

#include "WindowHook.h"
#include "TaskbarWindowPreview.h"

#ifdef ACCESSIBILITY
#include "OLEACC.H"
#include "nsAccessible.h"
#endif

#include "nsUXThemeData.h"

/**
 * Forward class definitions
 */

class nsNativeDragTarget;
class nsIRollupListener;
class nsIFile;
class imgIContainer;

/**
 * Native WIN32 window wrapper.
 */

class nsWindow : public nsBaseWidget
{
  typedef mozilla::widget::WindowHook WindowHook;
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
  typedef mozilla::widget::TaskbarWindowPreview TaskbarWindowPreview;
#endif
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
                                 nsIAppShell *aAppShell = nsnull,
                                 nsIToolkit *aToolkit = nsnull,
                                 nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD              Destroy();
  NS_IMETHOD              SetParent(nsIWidget *aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual float           GetDPI();
  NS_IMETHOD              Show(PRBool bState);
  NS_IMETHOD              IsVisible(PRBool & aState);
  NS_IMETHOD              ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD              Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD              ResizeClient(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD              BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical);
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement, nsIWidget *aWidget, PRBool aActivate);
  NS_IMETHOD              SetSizeMode(PRInt32 aMode);
  NS_IMETHOD              Enable(PRBool aState);
  NS_IMETHOD              IsEnabled(PRBool *aState);
  NS_IMETHOD              SetFocus(PRBool aRaise);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);
  NS_IMETHOD              GetScreenBounds(nsIntRect &aRect);
  NS_IMETHOD              GetClientBounds(nsIntRect &aRect);
  virtual nsIntPoint      GetClientOffset();
  NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    PRUint32 aHotspotX, PRUint32 aHotspotY);
  NS_IMETHOD              SetCursor(nsCursor aCursor);
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  NS_IMETHOD              MakeFullScreen(PRBool aFullScreen);
  NS_IMETHOD              HideWindowChrome(PRBool aShouldHide);
  NS_IMETHOD              Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD              Invalidate(const nsIntRect & aRect, PRBool aIsSynchronous);
  NS_IMETHOD              Update();
  virtual void*           GetNativeData(PRUint32 aDataType);
  virtual void            FreeNativeData(void * data, PRUint32 aDataType);
  NS_IMETHOD              SetTitle(const nsAString& aTitle);
  NS_IMETHOD              SetIcon(const nsAString& aIconSpec);
  virtual nsIntPoint      WidgetToScreenOffset();
  virtual nsIntSize       ClientToWindowSize(const nsIntSize& aClientSize);
  NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
  NS_IMETHOD              EnableDragDrop(PRBool aEnable);
  NS_IMETHOD              CaptureMouse(PRBool aCapture);
  NS_IMETHOD              CaptureRollupEvents(nsIRollupListener * aListener, nsIMenuRollup * aMenuRollup,
                                              PRBool aDoCapture, PRBool aConsumeRollupEvent);
  NS_IMETHOD              GetAttention(PRInt32 aCycleCount);
  virtual PRBool          HasPendingInputEvent();
  virtual LayerManager*   GetLayerManager(LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT, bool* aAllowRetaining = nsnull);
  gfxASurface             *GetThebesSurface();
  NS_IMETHOD              OnDefaultButtonLoaded(const nsIntRect &aButtonRect);
  NS_IMETHOD              OverrideSystemMouseScrollSpeed(PRInt32 aOriginalDelta, PRBool aIsHorizontal, PRInt32 &aOverriddenDelta);

  virtual nsresult        SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                                   PRInt32 aNativeKeyCode,
                                                   PRUint32 aModifierFlags,
                                                   const nsAString& aCharacters,
                                                   const nsAString& aUnmodifiedCharacters);
  virtual nsresult        SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                     PRUint32 aNativeMessage,
                                                     PRUint32 aModifierFlags);
  NS_IMETHOD              ResetInputState();
  NS_IMETHOD              SetIMEOpenState(PRBool aState);
  NS_IMETHOD              GetIMEOpenState(PRBool* aState);
  NS_IMETHOD              SetInputMode(const IMEContext& aContext);
  NS_IMETHOD              GetInputMode(IMEContext& aContext);
  NS_IMETHOD              CancelIMEComposition();
  NS_IMETHOD              GetToggledKeyState(PRUint32 aKeyCode, PRBool* aLEDState);
  NS_IMETHOD              RegisterTouchWindow();
  NS_IMETHOD              UnregisterTouchWindow();
#ifdef MOZ_XUL
  virtual void            SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();
  virtual void            UpdateOpaqueRegion(const nsIntRegion& aOpaqueRegion);
#endif // MOZ_XUL
#ifdef NS_ENABLE_TSF
  NS_IMETHOD              OnIMEFocusChange(PRBool aFocus);
  NS_IMETHOD              OnIMETextChange(PRUint32 aStart, PRUint32 aOldEnd, PRUint32 aNewEnd);
  NS_IMETHOD              OnIMESelectionChange(void);
#endif // NS_ENABLE_TSF
  NS_IMETHOD              GetNonClientMargins(nsIntMargin &margins);
  NS_IMETHOD              SetNonClientMargins(nsIntMargin &margins);
  void                    SetDrawsInTitlebar(PRBool aState);

  /**
   * Statics used in other classes
   */
  static PRInt32          GetWindowsVersion();

  /**
   * Event helpers
   */
  void                    InitEvent(nsGUIEvent& event, nsIntPoint* aPoint = nsnull);
  virtual PRBool          DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam,
                                             LPARAM lParam,
                                             PRBool aIsContextMenuKey = PR_FALSE,
                                             PRInt16 aButton = nsMouseEvent::eLeftButton,
                                             PRUint16 aInputSource = nsIDOMNSMouseEvent::MOZ_SOURCE_MOUSE);
  virtual PRBool          DispatchWindowEvent(nsGUIEvent* event);
  virtual PRBool          DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus);
  virtual PRBool          DispatchKeyEvent(PRUint32 aEventType, WORD aCharCode,
                                           const nsTArray<nsAlternativeCharCode>* aAlternativeChars,
                                           UINT aVirtualCharCode, const MSG *aMsg,
                                           const nsModifierKeyState &aModKeyState,
                                           PRUint32 aFlags = 0);
  void                    DispatchPendingEvents();
  PRBool                  DispatchPluginEvent(UINT aMessage,
                                              WPARAM aWParam,
                                              LPARAM aLParam,
                                              PRBool aDispatchPendingEvents);

  void                    SuppressBlurEvents(PRBool aSuppress); // Called from nsFilePicker
  PRBool                  BlurEventsSuppressed();
#ifdef ACCESSIBILITY
  nsAccessible* DispatchAccessibleEvent(PRUint32 aEventType);
  nsAccessible* GetRootAccessible();
#endif // ACCESSIBILITY

  /**
   * Window utilities
   */
  static void             GlobalMsgWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  nsWindow*               GetTopLevelWindow(PRBool aStopOnDialogOrPopup);
  static HWND             GetTopLevelHWND(HWND aWnd, PRBool aStopOnDialogOrPopup = PR_FALSE);
  HWND                    GetWindowHandle() { return mWnd; }
  WNDPROC                 GetPrevWindowProc() { return mPrevWndProc; }
  static nsWindow*        GetNSWindowPtr(HWND aWnd);
  WindowHook&             GetWindowHook() { return mWindowHook; }
  nsWindow*               GetParentWindow(PRBool aIncludeOwner);
  // Get an array of all nsWindow*s on the main thread.
  typedef void            (WindowEnumCallback)(nsWindow*);
  static void             EnumAllWindows(WindowEnumCallback aCallback);

  /**
   * Misc.
   */
  virtual PRBool          AutoErase(HDC dc);
  nsIntPoint*             GetLastPoint() { return &mLastPoint; }
  // needed in nsIMM32Handler.cpp
  PRBool                  PluginHasFocus() { return mIMEContext.mStatus == nsIWidget::IME_STATUS_PLUGIN; }
  PRBool                  IsTopLevelWidget() { return mIsTopWidgetWindow; }
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
  PRBool                  AssociateDefaultIMC(PRBool aAssociate);

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
  PRBool HasTaskbarIconBeenCreated() { return mHasTaskbarIconBeenCreated; }
  // Called when either the nsWindow or an nsITaskbarTabPreview receives the noticiation that this window
  // has its icon placed on the taskbar.
  void SetHasTaskbarIconBeenCreated(PRBool created = PR_TRUE) { mHasTaskbarIconBeenCreated = created; }

  // Getter/setter for the nsITaskbarWindowPreview for this nsWindow
  already_AddRefed<nsITaskbarWindowPreview> GetTaskbarPreview() {
    nsCOMPtr<nsITaskbarWindowPreview> preview(do_QueryReferent(mTaskbarPreview));
    return preview.forget();
  }
  void SetTaskbarPreview(nsITaskbarWindowPreview *preview) { mTaskbarPreview = do_GetWeakReference(preview); }
#endif

  NS_IMETHOD              ReparentNativeWidget(nsIWidget* aNewParent);
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
  static BOOL CALLBACK    FindOurWindowAtPointCallback(HWND aHWND, LPARAM aLPARAM);

  /**
   * Window utilities
   */
  static BOOL             SetNSWindowPtr(HWND aWnd, nsWindow * ptr);
  static PRInt32          GetMonitorCount();
  LPARAM                  lParamToScreen(LPARAM lParam);
  LPARAM                  lParamToClient(LPARAM lParam);
  virtual void            SubclassWindow(BOOL bState);
  PRBool                  CanTakeFocus();
  PRBool                  UpdateNonClientMargins(PRInt32 aSizeMode = -1, PRBool aReflowWindow = PR_TRUE);
  void                    UpdateGetWindowInfoCaptionStatus(PRBool aActiveCaption);
  void                    ResetLayout();
  void                    InvalidateNonClientRegion();
  HRGN                    ExcludeNonClientFromPaintRegion(HRGN aRegion);
  static void             InitInputWorkaroundPrefDefaults();
  static PRBool           GetInputWorkaroundPref(const char* aPrefName, PRBool aValueIfAutomatic);
  static PRBool           UseTrackPointHack();
  static void             PerformElantechSwipeGestureHack(UINT& aVirtualKeyCode, nsModifierKeyState& aModKeyState);
  static void             GetMainWindowClass(nsAString& aClass);
  PRBool                  HasGlass() const {
    return mTransparencyMode == eTransparencyGlass ||
           mTransparencyMode == eTransparencyBorderlessGlass;
  }
  static PRBool           IsOurProcessWindow(HWND aHWND);
  static HWND             FindOurProcessWindow(HWND aHWND);
  static HWND             FindOurWindowAtPoint(const POINT& aPoint);

  /**
   * Event processing helpers
   */
  PRBool                  DispatchPluginEvent(const MSG &aMsg);
  PRBool                  DispatchFocusToTopLevelWindow(PRUint32 aEventType);
  PRBool                  DispatchFocus(PRUint32 aEventType);
  PRBool                  DispatchStandardEvent(PRUint32 aMsg);
  PRBool                  DispatchCommandEvent(PRUint32 aEventCommand);
  void                    RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);
  static void             RemoveNextCharMessage(HWND aWnd);
  void                    RemoveMessageAndDispatchPluginEvent(UINT aFirstMsg,
                            UINT aLastMsg,
                            nsFakeCharMessage* aFakeCharMessage = nsnull);
  static MSG              InitMSG(UINT aMessage, WPARAM wParam, LPARAM lParam);
  virtual PRBool          ProcessMessage(UINT msg, WPARAM &wParam,
                                         LPARAM &lParam, LRESULT *aRetValue);
  PRBool                  ProcessMessageForPlugin(const MSG &aMsg,
                                                  LRESULT *aRetValue, PRBool &aCallDefWndProc);
  LRESULT                 ProcessCharMessage(const MSG &aMsg,
                                             PRBool *aEventDispatched);
  LRESULT                 ProcessKeyUpMessage(const MSG &aMsg,
                                              PRBool *aEventDispatched);
  LRESULT                 ProcessKeyDownMessage(const MSG &aMsg,
                                                PRBool *aEventDispatched);
  static PRBool           EventIsInsideWindow(UINT Msg, nsWindow* aWindow);
  // Convert nsEventStatus value to a windows boolean
  static PRBool           ConvertStatus(nsEventStatus aStatus);
  static void             PostSleepWakeNotification(const char* aNotification);
  PRInt32                 ClientMarginHitTestPoint(PRInt32 mx, PRInt32 my);
  static WORD             GetScanCode(LPARAM aLParam)
  {
    return (aLParam >> 16) & 0xFF;
  }
  static PRBool           IsExtendedScanCode(LPARAM aLParam)
  {
    return (aLParam & 0x1000000) != 0;
  }
  static PRBool           IsRedirectedKeyDownMessage(const MSG &aMsg);
  static void             ForgetRedirectedKeyDownMessage()
  {
    sRedirectedKeyDown.message = WM_NULL;
  }

  /**
   * Event handlers
   */
  virtual void            OnDestroy();
  virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool          OnResize(nsIntRect &aWindowRect);
  LRESULT                 OnChar(const MSG &aMsg,
                                 nsModifierKeyState &aModKeyState,
                                 PRBool *aEventDispatched,
                                 PRUint32 aFlags = 0);
  LRESULT                 OnKeyDown(const MSG &aMsg,
                                    nsModifierKeyState &aModKeyState,
                                    PRBool *aEventDispatched,
                                    nsFakeCharMessage* aFakeCharMessage);
  LRESULT                 OnKeyUp(const MSG &aMsg,
                                  nsModifierKeyState &aModKeyState,
                                  PRBool *aEventDispatched);
  LRESULT                 OnCharRaw(UINT charCode, UINT aScanCode,
                                    nsModifierKeyState &aModKeyState,
                                    PRUint32 aFlags = 0,
                                    const MSG *aMsg = nsnull,
                                    PRBool *aEventDispatched = nsnull);
  PRBool                  OnScroll(UINT aMsg, WPARAM aWParam, LPARAM aLParam);
  void                    OnScrollInternal(UINT aMsg, WPARAM aWParam,
                                           LPARAM aLParam);
  PRBool                  OnGesture(WPARAM wParam, LPARAM lParam);
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
  PRBool                  OnTouch(WPARAM wParam, LPARAM lParam);
#endif
  PRBool                  OnHotKey(WPARAM wParam, LPARAM lParam);
  BOOL                    OnInputLangChange(HKL aHKL);
  PRBool                  OnPaint(HDC aDC, PRUint32 aNestingLevel);
  void                    OnWindowPosChanged(WINDOWPOS *wp, PRBool& aResult);
  static UINT             GetInternalMessage(UINT aNativeMessage);
  static UINT             GetNativeMessage(UINT aInternalMessage);
  void                    OnMouseWheel(UINT aMsg, WPARAM aWParam,
                                       LPARAM aLParam, LRESULT *aRetValue);
  void                    OnMouseWheelInternal(UINT aMessage, WPARAM aWParam,
                                               LPARAM aLParam,
                                               LRESULT *aRetValue);
  void                    OnWindowPosChanging(LPWINDOWPOS& info);

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
  void                    ResizeTranslucentWindow(PRInt32 aNewWidth, PRInt32 aNewHeight, PRBool force = PR_FALSE);
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
  UINT                    MapFromNativeToDOM(UINT aNativeKeyCode);
  void                    StopFlashing();
  static PRBool           IsTopLevelMouseExit(HWND aWnd);
  static void             SetupKeyModifiersSequence(nsTArray<KeyPair>* aArray, PRUint32 aModifiers);
  nsresult                SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                              PRBool aIntersectWithExisting);
  nsIntRegion             GetRegionToPaint(PRBool aForceFullRepaint, 
                                           PAINTSTRUCT ps, HDC aDC);
  static void             ActivateOtherWindowHelper(HWND aWnd);
  static PRUint16         GetMouseInputSource();
#ifdef ACCESSIBILITY
  static STDMETHODIMP_(LRESULT) LresultFromObject(REFIID riid, WPARAM wParam, LPUNKNOWN pAcc);
#endif // ACCESSIBILITY
  void                    ClearCachedResources();

  nsPopupType PopupType() { return mPopupType; }

protected:
  nsCOMPtr<nsIWidget>   mParent;
  nsIntSize             mLastSize;
  nsIntPoint            mLastPoint;
  HWND                  mWnd;
  WNDPROC               mPrevWndProc;
  HBRUSH                mBrush;
  PRPackedBool          mIsTopWidgetWindow;
  PRPackedBool          mInDtor;
  PRPackedBool          mIsVisible;
  PRPackedBool          mUnicodeWidget;
  PRPackedBool          mPainting;
  PRPackedBool          mTouchWindow;
  PRPackedBool          mDisplayPanFeedback;
  PRPackedBool          mHideChrome;
  PRPackedBool          mIsRTL;
  PRPackedBool          mFullscreenMode;
  PRPackedBool          mMousePresent;
  PRUint32              mBlurSuppressLevel;
  DWORD_PTR             mOldStyle;
  DWORD_PTR             mOldExStyle;
  IMEContext            mIMEContext;
  nsNativeDragTarget*   mNativeDragTarget;
  HKL                   mLastKeyboardLayout;
  nsPopupType           mPopupType;
  nsSizeMode            mOldSizeMode;
  WindowHook            mWindowHook;
  DWORD                 mAssumeWheelIsZoomUntil;
  static PRBool         sDropShadowEnabled;
  static PRUint32       sInstanceCount;
  static TriStateBool   sCanQuit;
  static nsWindow*      sCurrentWindow;
  static BOOL           sIsOleInitialized;
  static HCURSOR        sHCursor;
  static imgIContainer* sCursorImgContainer;
  static PRBool         sSwitchKeyboardLayout;
  static PRBool         sJustGotDeactivate;
  static PRBool         sJustGotActivate;
  static PRBool         sIsInMouseCapture;
  static int            sTrimOnMinimize;
  static PRBool         sDefaultTrackPointHack;
  static const char*    sDefaultMainWindowClass;
  static PRBool         sUseElantechSwipeHack;
  static PRBool         sUseElantechPinchHack;
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
  PRPackedBool          mCustomNonClient;
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
  static PRPackedBool   sProcessHook;
  static UINT           sRollupMsgId;
  static HWND           sRollupMsgWnd;
  static UINT           sHookTimerId;

  // Rollup Listener
  static nsIWidget*     sRollupWidget;
  static PRBool         sRollupConsumeEvent;
  static nsIRollupListener* sRollupListener;
  static nsIMenuRollup* sMenuRollup;

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
#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  nsIntRegion           mPossiblyTransparentRegion;
  MARGINS               mGlassMargins;
#endif // #if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
#endif // MOZ_XUL

  // Win7 Gesture processing and management
  nsWinGesture          mGesture;

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_WIN7
  // Weak ref to the nsITaskbarWindowPreview associated with this window
  nsWeakPtr             mTaskbarPreview;
  // True if the taskbar (possibly through the tab preview) tells us that the
  // icon has been created on the taskbar.
  PRBool                mHasTaskbarIconBeenCreated;
#endif

#ifdef ACCESSIBILITY
  static BOOL           sIsAccessibilityOn;
  static HINSTANCE      sAccLib;
  static LPFNLRESULTFROMOBJECT sLresultFromObject;
#endif // ACCESSIBILITY

  // sRedirectedKeyDown is WM_KEYDOWN message or WM_SYSKEYDOWN message which
  // was reirected to SendInput() API by OnKeyDown().
  static MSG            sRedirectedKeyDown;

  static PRBool sEnablePixelScrolling;
  static PRBool sNeedsToInitMouseWheelSettings;
  static ULONG sMouseWheelScrollLines;
  static ULONG sMouseWheelScrollChars;
  static void InitMouseWheelScrollData();

  static HWND sLastMouseWheelWnd;
  static PRInt32 sRemainingDeltaForScroll;
  static PRInt32 sRemainingDeltaForPixel;
  static PRBool sLastMouseWheelDeltaIsPositive;
  static PRBool sLastMouseWheelOrientationIsVertical;
  static PRBool sLastMouseWheelUnitIsPage;
  static PRUint32 sLastMouseWheelTime; // in milliseconds
  static void ResetRemainingWheelDelta();

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

    PRBool mCancel;
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
