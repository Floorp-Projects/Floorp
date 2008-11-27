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

#include "nsBaseWidget.h"
#include "nsdefs.h"
#include "nsSwitchToUIThread.h"
#include "nsToolkit.h"

#include "nsIWidget.h"

#include "nsIEventListener.h"
#include "nsString.h"

#include "nsVoidArray.h"
#include "nsTArray.h"

class nsNativeDragTarget;
class nsIRollupListener;

class nsIFile;

class imgIContainer;

struct nsAlternativeCharCode;
struct nsFakeCharMessage;

#ifdef ACCESSIBILITY
#include "OLEACC.H"
#include "nsIAccessible.h"
#endif

#include "gfxWindowsSurface.h"

#define IME_MAX_CHAR_POS       64

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))
#define COLOREF_2_NSRGB(color) \
            NS_RGB(GetRValue(color), GetGValue(color), GetBValue(color))

#define WIN2K_VERSION   0x500
#define WINXP_VERSION   0x501
#define WIN2K3_VERSION  0x502
#define VISTA_VERSION   0x600

PRInt32 GetWindowsVersion();

/*
 * ::: IMPORTANT :::
 * External apps and drivers depend on window class names.
 * For example, changing the window classes
 * could break touchpad scrolling or screen readers.
 */
const PRUint32 kMaxClassNameLength    = 40;
const LPCWSTR kWClassNameHidden       = L"MozillaHiddenWindowClass";
const LPCWSTR kWClassNameUI           = L"MozillaUIWindowClass";
const LPCWSTR kWClassNameContent      = L"MozillaContentWindowClass";
const LPCWSTR kWClassNameContentFrame = L"MozillaContentFrameWindowClass";
const LPCWSTR kWClassNameGeneral      = L"MozillaWindowClass";
const LPCWSTR kWClassNameDialog       = L"MozillaDialogClass";
const LPCSTR kClassNameHidden         = "MozillaHiddenWindowClass";
const LPCSTR kClassNameUI             = "MozillaUIWindowClass";
const LPCSTR kClassNameContent        = "MozillaContentWindowClass";
const LPCSTR kClassNameContentFrame   = "MozillaContentFrameWindowClass";
const LPCSTR kClassNameGeneral        = "MozillaWindowClass";
const LPCSTR kClassNameDialog         = "MozillaDialogClass";

typedef enum
{
    TRI_UNKNOWN = -1,
    TRI_FALSE = 0,
    TRI_TRUE = 1
} TriStateBool;

/**
 * Native WIN32 window wrapper.
 */

class nsWindow : public nsSwitchToUIThread,
                 public nsBaseWidget
{
public:
  nsWindow();
  virtual ~nsWindow();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIWidget interface
  NS_IMETHOD              Create(nsIWidget *aParent,
                                 const nsRect &aRect,
                                 EVENT_CALLBACK aHandleEventFunction,
                                 nsIDeviceContext *aContext,
                                 nsIAppShell *aAppShell = nsnull,
                                 nsIToolkit *aToolkit = nsnull,
                                 nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD              Create(nsNativeWidget aParent,
                                 const nsRect &aRect,
                                 EVENT_CALLBACK aHandleEventFunction,
                                 nsIDeviceContext *aContext,
                                 nsIAppShell *aAppShell = nsnull,
                                 nsIToolkit *aToolkit = nsnull,
                                 nsWidgetInitData *aInitData = nsnull);

  // Utility method for implementing both Create(nsIWidget ...) and
  // Create(nsNativeWidget...)

  virtual nsresult        StandardWindowCreate(nsIWidget *aParent,
                                               const nsRect &aRect,
                                               EVENT_CALLBACK aHandleEventFunction,
                                               nsIDeviceContext *aContext,
                                               nsIAppShell *aAppShell,
                                               nsIToolkit *aToolkit,
                                               nsWidgetInitData *aInitData,
                                               nsNativeWidget aNativeParent = nsnull);

  NS_IMETHOD              Destroy();
  NS_IMETHOD              SetParent(nsIWidget *aNewParent);
  virtual nsIWidget*      GetParent(void);
  NS_IMETHOD              Show(PRBool bState);
  NS_IMETHOD              IsVisible(PRBool & aState);
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement, nsIWidget *aWidget, PRBool aActivate);
  NS_IMETHOD              SetSizeMode(PRInt32 aMode);
  NS_IMETHOD              ModalEventFilter(PRBool aRealEvent, void *aEvent, PRBool *aForWindow);
  NS_IMETHOD              CaptureMouse(PRBool aCapture);
  NS_IMETHOD              ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD              Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD              Enable(PRBool aState);
  NS_IMETHOD              IsEnabled(PRBool *aState);
  NS_IMETHOD              SetFocus(PRBool aRaise);
  NS_IMETHOD              GetBounds(nsRect &aRect);
  NS_IMETHOD              GetClientBounds(nsRect &aRect);
  NS_IMETHOD              GetScreenBounds(nsRect &aRect);
  NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
  NS_IMETHOD              SetCursor(nsCursor aCursor);
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    PRUint32 aHotspotX, PRUint32 aHotspotY);
  NS_IMETHOD              HideWindowChrome(PRBool aShouldHide);
  NS_IMETHOD              Validate();
  NS_IMETHOD              Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD              Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
  NS_IMETHOD              InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);
  NS_IMETHOD              Update();
  virtual void*           GetNativeData(PRUint32 aDataType);
  virtual void            FreeNativeData(void * data, PRUint32 aDataType);//~~~
  NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
  //XXX-Scroll is obsolete it is going away soon
  NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD              ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD              ScrollRect(nsRect &aRect, PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD              SetTitle(const nsAString& aTitle);
  NS_IMETHOD              SetIcon(const nsAString& aIconSpec);
  NS_IMETHOD              SetMenuBar(void * aMenuBar) { return NS_ERROR_FAILURE; }
  NS_IMETHOD              ShowMenuBar(PRBool aShow)         { return NS_ERROR_FAILURE; }
  NS_IMETHOD              WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
  NS_IMETHOD              ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
  NS_IMETHOD              BeginResizingChildren(void);
  NS_IMETHOD              EndResizingChildren(void);
  NS_IMETHOD              GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
  NS_IMETHOD              SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
  NS_IMETHOD              EnableDragDrop(PRBool aEnable);

  virtual void            SetUpForPaint(HDC aHDC);
  virtual void            ConvertToDeviceCoordinates(nscoord& aX,nscoord& aY) {}

  NS_IMETHOD              CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);

  NS_IMETHOD              GetAttention(PRInt32 aCycleCount);
  NS_IMETHOD              GetLastInputEventTime(PRUint32& aTime);

  // Note that the result of GetTopLevelWindow method can be different from the
  // result of GetTopLevelHWND method.  The result can be non-floating window.
  // Because our top level window may be contained in another window which is
  // not managed by us.
  nsWindow*               GetTopLevelWindow(PRBool aStopOnDialogOrPopup);

  gfxASurface             *GetThebesSurface();

#ifdef MOZ_XUL
  virtual void            SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();
private:
  void                    SetWindowTranslucencyInner(nsTransparencyMode aMode);
  nsTransparencyMode      GetWindowTranslucencyInner() const { return mTransparencyMode; }
  void                    ResizeTranslucentWindow(PRInt32 aNewWidth, PRInt32 aNewHeight, PRBool force = PR_FALSE);
  nsresult                UpdateTranslucentWindow();
  void                    SetupTranslucentWindowMemoryBitmap(nsTransparencyMode aMode);
public:
#endif

  NS_IMETHOD ResetInputState();
  NS_IMETHOD SetIMEOpenState(PRBool aState);
  NS_IMETHOD GetIMEOpenState(PRBool* aState);
  NS_IMETHOD SetIMEEnabled(PRUint32 aState);
  NS_IMETHOD GetIMEEnabled(PRUint32* aState);
  NS_IMETHOD CancelIMEComposition();
  NS_IMETHOD GetToggledKeyState(PRUint32 aKeyCode, PRBool* aLEDState);

  PRBool IMEMouseHandling(PRInt32 aAction, LPARAM lParam);
  PRBool IMECompositionHitTest(POINT * ptPos);
  PRBool HandleMouseActionOfIME(PRInt32 aAction, POINT* ptPos);
  void GetCompositionWindowPos(HIMC hIMC, PRUint32 aEventType, COMPOSITIONFORM *cpForm);

  // nsSwitchToUIThread interface
  virtual BOOL            CallMethod(MethodInfo *info);

  HWND                    GetWindowHandle() { return mWnd; }
  WNDPROC                 GetPrevWindowProc() { return mPrevWndProc; }

  virtual PRBool          DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam,
                                             LPARAM lParam,
                                             PRBool aIsContextMenuKey = PR_FALSE,
                                             PRInt16 aButton = nsMouseEvent::eLeftButton);
#ifdef ACCESSIBILITY
  virtual PRBool          DispatchAccessibleEvent(PRUint32 aEventType, nsIAccessible** aAccessible, nsPoint* aPoint = nsnull);
  already_AddRefed<nsIAccessible> GetRootAccessible();
#endif
  virtual PRBool          AutoErase();
  nsPoint*                GetLastPoint() { return &mLastPoint; }

  PRInt32                 GetNewCmdMenuId() { mMenuCmdId++; return mMenuCmdId; }

  void                    InitEvent(nsGUIEvent& event, nsPoint* aPoint = nsnull);

  void                    SuppressBlurEvents(PRBool aSuppress);
  PRBool                  BlurEventsSuppressed();

protected:

#ifndef WINCE

  // special callback hook methods for pop ups
  static LRESULT CALLBACK MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam);
  static VOID    CALLBACK HookTimerForPopups( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime );
  static void             ScheduleHookTimer(HWND aWnd, UINT aMsgId);

  static void             RegisterSpecialDropdownHooks();
  static void             UnregisterSpecialDropdownHooks();

  static void             PostSleepWakeNotification(const char* aNotification);
#endif

  static BOOL             DealWithPopups (HWND inWnd, UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult);

  static PRBool           EventIsInsideWindow(UINT Msg, nsWindow* aWindow);

  static nsWindow*        GetNSWindowPtr(HWND aWnd);
  static BOOL             SetNSWindowPtr(HWND aWnd, nsWindow * ptr);
  nsWindow*               GetParentWindow();

  void                    DispatchPendingEvents();
  virtual PRBool          ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue);
  virtual PRBool          DispatchWindowEvent(nsGUIEvent* event);
  virtual PRBool          DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus);

   // Allow Derived classes to modify the height that is passed
   // when the window is created or resized.
  virtual PRInt32         GetHeight(PRInt32 aProposedHeight);
  virtual LPCWSTR         WindowClassW();
  virtual LPCWSTR         WindowPopupClassW();
  virtual LPCTSTR         WindowClass();
  virtual LPCTSTR         WindowPopupClass();
  virtual DWORD           WindowStyle();
  virtual DWORD           WindowExStyle();

  virtual void            SubclassWindow(BOOL bState);

  virtual void            OnDestroy();
  virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool          OnPaint(HDC aDC = nsnull);
  virtual PRBool          OnResize(nsRect &aWindowRect);
  
  void                    SetupModKeyState();
  BOOL                    OnChar(UINT charCode, UINT aScanCode, PRUint32 aFlags = 0);
  BOOL                    OnKeyDown( UINT aVirtualKeyCode, LPARAM aKeyCode,
                                     nsFakeCharMessage* aFakeCharMessage);
  BOOL                    OnKeyUp( UINT aVirtualKeyCode, LPARAM aKeyCode);
  UINT                    MapFromNativeToDOM(UINT aNativeKeyCode);


  BOOL                    OnInputLangChange(HKL aHKL, LRESULT *oResult);
  BOOL                    OnIMEChar(BYTE aByte1, BYTE aByte2, LPARAM aKeyState);
  BOOL                    OnIMEComposition(LPARAM  aGCS);
  BOOL                    OnIMECompositionFull();
  BOOL                    OnIMEEndComposition();
  BOOL                    OnIMENotify(WPARAM  aIMN, LPARAM aData, LRESULT *oResult);
  BOOL                    OnIMERequest(WPARAM  aIMR, LPARAM aData, LRESULT *oResult);
  BOOL                    OnIMESelect(BOOL  aSelected, WORD aLangID);
  BOOL                    OnIMESetContext(BOOL aActive, LPARAM& aISC);
  BOOL                    OnIMEStartComposition();
  BOOL                    OnIMEReconvert(LPARAM aData, LRESULT *oResult);
  BOOL                    OnIMEQueryCharPosition(LPARAM aData, LRESULT *oResult);

  void                    GetCompositionString(HIMC aHIMC, DWORD aIndex);

  /**
   *  ResolveIMECaretPos
   *  Convert the caret rect of a composition event to another widget's
   *  coordinate system.
   *
   *  @param aReferenceWidget The origin widget of aCursorRect.
   *                          Typically, this is mReferenceWidget of the
   *                          composing events. If the aCursorRect is in screen
   *                          coordinates, set nsnull.
   *  @param aCursorRect      The cursor rect.
   *  @param aNewOriginWidget aOutRect will be in this widget's coordinates. If
   *                          this is nsnull, aOutRect will be in screen
   *                          coordinates.
   *  @param aOutRect         The converted cursor rect.
   */
  void                    ResolveIMECaretPos(nsIWidget* aReferenceWidget,
                                             nsRect&    aCursorRect,
                                             nsIWidget* aNewOriginWidget,
                                             nsRect&    aOutRect);

  virtual PRBool          DispatchKeyEvent(PRUint32 aEventType, WORD aCharCode,
                            const nsTArray<nsAlternativeCharCode>* aAlternativeChars,
                            UINT aVirtualCharCode, LPARAM aKeyCode,
                            PRUint32 aFlags = 0);

  virtual PRBool          DispatchFocus(PRUint32 aEventType, PRBool isMozWindowTakingFocus);
  virtual PRBool          OnScroll(UINT scrollCode, int cPos);
  virtual HBRUSH          OnControlColor();

  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK DefaultWindowProc(HWND hWns, UINT msg, WPARAM wParam, LPARAM lParam);

  // Convert nsEventStatus value to a windows boolean
  static PRBool ConvertStatus(nsEventStatus aStatus)
                       { return aStatus == nsEventStatus_eConsumeNoDefault; }

  PRBool DispatchStandardEvent(PRUint32 aMsg);
  PRBool DispatchCommandEvent(PRUint32 aEventCommand);
  void RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);

  void GetNonClientBounds(nsRect &aRect);
  void HandleTextEvent(HIMC hIMEContext, PRBool aCheckAttr = PR_TRUE);
  BOOL HandleStartComposition(HIMC hIMEContext);
  void HandleEndComposition(void);
  void GetTextRangeList(PRUint32* textRangeListLengthResult, nsTextRangeArray* textRangeListResult);

  void ConstrainZLevel(HWND *aAfter);

  LPARAM lParamToScreen(LPARAM lParam);
  LPARAM lParamToClient(LPARAM lParam);

  PRBool CanTakeFocus();

  virtual nsresult SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                            PRInt32 aNativeKeyCode,
                                            PRUint32 aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters);

private:


#ifdef DEBUG
  void DebugPrintEvent(nsGUIEvent& aEvent, HWND aWnd);
#endif

protected:
  // Count of nsWindow instances. Used to manage IME buffers
  static PRUint32   sInstanceCount;

  // For Input Method Support
  // Only one set of IME buffers is needed for a process.
  static PRBool     sIMEIsComposing;
  static PRBool     sIMEIsStatusChanged;

  static DWORD      sIMEProperty;
  static nsString*  sIMECompUnicode;
  static PRUint8*   sIMEAttributeArray;
  static PRInt32    sIMEAttributeArrayLength;
  static PRInt32    sIMEAttributeArraySize;
  static PRUint32*  sIMECompClauseArray;
  static PRInt32    sIMECompClauseArrayLength;
  static PRInt32    sIMECompClauseArraySize;
  static long       sIMECursorPosition;

  // For describing composing frame
  static RECT*      sIMECompCharPos;

  static TriStateBool sCanQuit;

  nsSize        mLastSize;
  static        nsWindow* gCurrentWindow;
  nsPoint       mLastPoint;
  HWND          mWnd;
  HDC           mPaintDC; // only set during painting
#if 0
  HPALETTE      mPalette;
#endif
  WNDPROC       mPrevWndProc;
  HBRUSH        mBrush;

#ifdef MOZ_XUL
  // use layered windows to support full 256 level alpha translucency
  nsRefPtr<gfxWindowsSurface> mTransparentSurface;

  HDC           mMemoryDC;
  nsTransparencyMode mTransparencyMode;
#endif
  PRPackedBool  mIsTopWidgetWindow;
  PRPackedBool  mHas3DBorder;
  PRPackedBool  mIsShiftDown;
  PRPackedBool  mIsControlDown;
  PRPackedBool  mIsAltDown;
  PRPackedBool  mIsDestroying;
  PRPackedBool  mOnDestroyCalled;
  PRPackedBool  mIsVisible;
  PRPackedBool  mIsInMouseCapture;
  PRPackedBool  mIsInMouseWheelProcessing;
  PRPackedBool  mUnicodeWidget;
  PRPackedBool  mIsPluginWindow;

  PRPackedBool  mPainting;
  char          mLeadByte;
  PRUint32      mBlurEventSuppressionLevel;
  nsContentType mContentType;

  PRInt32       mPreferredWidth;
  PRInt32       mPreferredHeight;

  PRInt32       mMenuCmdId;

  // Window styles used by this window before chrome was hidden
  DWORD         mOldStyle;
  DWORD         mOldExStyle;

  // To enable/disable IME
  HIMC          mOldIMC;
  PRUint32      mIMEEnabled;

  static HKL    gKeyboardLayout;
  static PRBool gSwitchKeyboardLayout;

  HKL           mLastKeyboardLayout;

  // Drag & Drop
  nsNativeDragTarget * mNativeDragTarget;

  // Enumeration of the methods which are accessible on the "main GUI thread"
  // via the CallMethod(...) mechanism...
  // see nsSwitchToUIThread
  enum {
    CREATE = 0x0101,
    CREATE_NATIVE,
    DESTROY,
    SET_FOCUS,
    SET_CURSOR,
    CREATE_HACK
  };

  static BOOL   sIsRegistered;
  static BOOL   sIsPopupClassRegistered;
  static BOOL   sIsOleInitialized; // OLE is needed for clipboard and drag & drop support

  HDWP mDeferredPositioner;
  static UINT   uWM_MSIME_MOUSE;     // mouse message for MSIME

  // Heap dump
  static UINT   uWM_HEAP_DUMP;       // Dump heap to a file

  // Cursor caching
  static HCURSOR        gHCursor;
  static imgIContainer* gCursorImgContainer;

#ifdef ACCESSIBILITY
  static BOOL gIsAccessibilityOn;
  static HINSTANCE gmAccLib;
  static LPFNLRESULTFROMOBJECT gmLresultFromObject;
  static STDMETHODIMP_(LRESULT) LresultFromObject(REFIID riid, WPARAM wParam, LPUNKNOWN pAcc);
#endif

  static BOOL CALLBACK BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg);
  static BOOL CALLBACK BroadcastMsg(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK DispatchStarvedPaints(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK InvalidateForeignChildWindows(HWND aWnd, LPARAM aMsg);

public:
  static void GlobalMsgWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  // Note that the result of GetTopLevelHWND can be different from the result
  // of GetTopLevelWindow method.  Because this is checking whether the window
  // is top level only in Win32 window system.  Therefore, the result window
  // may not be managed by us.
  static HWND GetTopLevelHWND(HWND aWnd,
                              PRBool aStopOnDialogOrPopup = PR_FALSE);
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {

public:
  ChildWindow() {}
  PRBool DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam, LPARAM lParam,
                            PRBool aIsContextMenuKey = PR_FALSE,
                            PRInt16 aButton = nsMouseEvent::eLeftButton);

protected:
  virtual DWORD WindowStyle();
};


#endif // Window_h__
