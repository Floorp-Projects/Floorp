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
#include "nsIKBStateControl.h"

#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsString.h"

#include "nsVoidArray.h"

#include <imm.h>

class nsNativeDragTarget;
class nsIRollupListener;

class nsIMenuBar;
class nsIFile;

#ifdef ACCESSIBILITY
#include "OLEACC.H"
#include "nsIAccessible.h"
#endif

#define IME_MAX_CHAR_POS       64

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))
#define COLOREF_2_NSRGB(color) \
            NS_RGB(GetRValue(color), GetGValue(color), GetBValue(color))

/*
 * ::: IMPORTANT :::
 * External apps and drivers depend on window class names.
 * For example, changing the window classes
 * could break touchpad scrolling or screen readers.
 */
const PRUint32 kMaxClassNameLength  = 40;
const LPCWSTR kWClassNameHidden     = L"MozillaHiddenWindowClass";
const LPCWSTR kWClassNameUI         = L"MozillaUIWindowClass";
const LPCWSTR kWClassNameContent    = L"MozillaContentWindowClass";
const LPCWSTR kWClassNameGeneral    = L"MozillaWindowClass";
const LPCSTR kClassNameHidden       = "MozillaHiddenWindowClass";
const LPCSTR kClassNameUI           = "MozillaUIWindowClass";
const LPCSTR kClassNameContent      = "MozillaContentWindowClass";
const LPCSTR kClassNameGeneral      = "MozillaWindowClass";

/**
* Native IMM wrapper
*/
class nsIMM
{
  //prototypes for DLL function calls...
  typedef LONG (CALLBACK *GetCompStrPtr)  (HIMC, DWORD, LPVOID, DWORD);
  typedef LONG (CALLBACK *GetContextPtr)  (HWND);
  typedef LONG (CALLBACK *RelContextPtr)  (HWND, HIMC);
  typedef LONG (CALLBACK *NotifyIMEPtr)   (HIMC, DWORD, DWORD, DWORD);
  typedef LONG (CALLBACK *SetCandWindowPtr)(HIMC, LPCANDIDATEFORM);
  typedef LONG (CALLBACK *SetCompWindowPtr)(HIMC, LPCOMPOSITIONFORM);
  typedef LONG (CALLBACK *GetCompWindowPtr)(HIMC, LPCOMPOSITIONFORM);
  typedef LONG (CALLBACK *GetPropertyPtr)  (HKL, DWORD);
  typedef LONG (CALLBACK *GetDefaultIMEWndPtr) (HWND);
public:

  static nsIMM& LoadModule() {
    static nsIMM gIMM;
    return gIMM;
  }

  nsIMM(const char* aModuleName="IMM32.DLL") {
    mInstance=::LoadLibrary(aModuleName);
    NS_ASSERTION(mInstance!=NULL, "nsIMM.LoadLibrary failed.");

    mGetCompositionStringA=(mInstance) ? (GetCompStrPtr)GetProcAddress(mInstance, "ImmGetCompositionStringA") : 0;
    NS_ASSERTION(mGetCompositionStringA!=NULL, "nsIMM.ImmGetCompositionStringA failed.");

    mGetCompositionStringW=(mInstance) ? (GetCompStrPtr)GetProcAddress(mInstance, "ImmGetCompositionStringW") : 0;
    NS_ASSERTION(mGetCompositionStringW!=NULL, "nsIMM.ImmGetCompositionStringW failed.");

    mGetContext=(mInstance) ? (GetContextPtr)GetProcAddress(mInstance, "ImmGetContext") : 0;
    NS_ASSERTION(mGetContext!=NULL, "nsIMM.ImmGetContext failed.");

    mReleaseContext=(mInstance) ? (RelContextPtr)GetProcAddress(mInstance, "ImmReleaseContext") : 0;
    NS_ASSERTION(mReleaseContext!=NULL, "nsIMM.ImmReleaseContext failed.");

    mNotifyIME=(mInstance) ? (NotifyIMEPtr)GetProcAddress(mInstance, "ImmNotifyIME") : 0;
    NS_ASSERTION(mNotifyIME!=NULL, "nsIMM.ImmNotifyIME failed.");

    mSetCandiateWindow=(mInstance) ? (SetCandWindowPtr)GetProcAddress(mInstance, "ImmSetCandidateWindow") : 0;
    NS_ASSERTION(mSetCandiateWindow!=NULL, "nsIMM.ImmSetCandidateWindow failed.");

    mGetCompositionWindow=(mInstance) ? (GetCompWindowPtr)GetProcAddress(mInstance, "ImmGetCompositionWindow") : 0;
    NS_ASSERTION(mGetCompositionWindow!=NULL, "nsIMM.ImmGetCompositionWindow failed.");

    mSetCompositionWindow=(mInstance) ? (SetCompWindowPtr)GetProcAddress(mInstance, "ImmSetCompositionWindow") : 0;
    NS_ASSERTION(mSetCompositionWindow!=NULL, "nsIMM.ImmSetCompositionWindow failed.");

    mGetProperty=(mInstance) ? (GetPropertyPtr)GetProcAddress(mInstance, "ImmGetProperty") : 0;
    NS_ASSERTION(mGetProperty!=NULL, "nsIMM.ImmGetProperty failed.");

    mGetDefaultIMEWnd=(mInstance) ? (GetDefaultIMEWndPtr)GetProcAddress(mInstance, "ImmGetDefaultIMEWnd") : 0;
    NS_ASSERTION(mGetDefaultIMEWnd!=NULL, "nsIMM.ImmGetDefaultIMEWnd failed.");
  }

  ~nsIMM() {
    if(mInstance) {
      ::FreeLibrary(mInstance);
    }
    mGetCompositionStringA=0;
    mGetCompositionStringW=0;
    mGetContext=0;
    mReleaseContext=0;
    mNotifyIME=0;
    mSetCandiateWindow=0;
    mGetCompositionWindow=0;
    mSetCompositionWindow=0;
    mGetProperty=0;
    mGetDefaultIMEWnd=0;
  }

  LONG GetCompositionStringA(HIMC h, DWORD d1, LPVOID v, DWORD d2) {
    return (mGetCompositionStringA) ? mGetCompositionStringA(h, d1, v, d2) : 0L;
  }

  LONG GetCompositionStringW(HIMC h, DWORD d1, LPVOID v, DWORD d2) {
    return (mGetCompositionStringW) ? mGetCompositionStringW(h, d1, v, d2) : 0L;
  }

  LONG GetContext(HWND anHWND) {
    return (mGetContext) ? mGetContext(anHWND) : 0L;
  }

  LONG ReleaseContext(HWND anHWND, HIMC anIMC) {
    return (mReleaseContext) ? mReleaseContext(anHWND, anIMC) : 0L;
  }

  LONG NotifyIME(HIMC h, DWORD d1, DWORD d2, DWORD d3) {
    return (mNotifyIME) ? mNotifyIME(h, d1, d2, d3) : 0L;
  }

  LONG SetCandidateWindow(HIMC h, LPCANDIDATEFORM l) {
    return (mSetCandiateWindow) ? mSetCandiateWindow(h, l) : 0L;
  }

  LONG SetCompositionWindow(HIMC h, LPCOMPOSITIONFORM l) {
    return (mSetCompositionWindow) ? mSetCompositionWindow(h, l) : 0L;
  }

  LONG GetCompositionWindow(HIMC h,LPCOMPOSITIONFORM l) {
    return (mGetCompositionWindow) ? mGetCompositionWindow(h, l) : 0L;
  }

  LONG GetProperty(HKL hKL, DWORD dwIndex) {
    return (mGetProperty) ? mGetProperty(hKL, dwIndex) : 0L;
  }

  LONG GetDefaultIMEWnd(HWND hWnd) {
    return (mGetDefaultIMEWnd) ? mGetDefaultIMEWnd(hWnd) : 0L;
  }

private:

  HINSTANCE           mInstance;
  GetCompStrPtr       mGetCompositionStringA;
  GetCompStrPtr       mGetCompositionStringW;
  GetContextPtr       mGetContext;
  RelContextPtr       mReleaseContext;
  NotifyIMEPtr        mNotifyIME;
  SetCandWindowPtr    mSetCandiateWindow;
  SetCompWindowPtr    mSetCompositionWindow;
  GetCompWindowPtr    mGetCompositionWindow;
  GetPropertyPtr      mGetProperty;
  GetDefaultIMEWndPtr mGetDefaultIMEWnd;
};

/**
 * Native WIN32 window wrapper.
 */

class nsWindow : public nsSwitchToUIThread,
                 public nsBaseWidget,
                 public nsIKBStateControl
{
public:
  nsWindow();
  virtual ~nsWindow();

  // nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

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
  virtual nsIFontMetrics* GetFont(void);
  NS_IMETHOD              SetFont(const nsFont &aFont);
  NS_IMETHOD              SetCursor(nsCursor aCursor);
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
  NS_IMETHOD              SetMenuBar(nsIMenuBar * aMenuBar) { return NS_ERROR_FAILURE; }
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

#ifdef MOZ_XUL
  NS_IMETHOD              SetWindowTranslucency(PRBool aTransparent);
  NS_IMETHOD              GetWindowTranslucency(PRBool& aTransparent);
  NS_IMETHOD              UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas);
private:
  nsresult                SetWindowTranslucencyInner(PRBool aTransparent);
  PRBool                  GetWindowTranslucencyInner() { return mIsTranslucent; }
  void                    UpdateTranslucentWindowAlphaInner(const nsRect& aRect, PRUint8* aAlphas);
  nsIWidget*              GetTopLevelWidget();
  void                    ResizeTranslucentWindow(PRInt32 aNewWidth, PRInt32 aNewHeight);
  nsresult                UpdateTranslucentWindow();
public:
#endif

  // nsIKBStateControl interface

  NS_IMETHOD ResetInputState();

  PRBool IMEMouseHandling(PRUint32 aEventType, PRInt32 aAction, LPARAM lParam);
  PRBool IMECompositionHitTest(PRUint32 aEventType, POINT * ptPos);
  PRBool HandleMouseActionOfIME(PRInt32 aAction, POINT* ptPos);
  void GetCompositionWindowPos(HIMC hIMC, PRUint32 aEventType, COMPOSITIONFORM *cpForm);

  // nsSwitchToUIThread interface
  virtual BOOL            CallMethod(MethodInfo *info);

  HWND                    GetWindowHandle() { return mWnd; }
  WNDPROC                 GetPrevWindowProc() { return mPrevWndProc; }

  virtual PRBool          DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam = NULL, nsPoint* aPoint = nsnull);
#ifdef ACCESSIBILITY
  virtual PRBool          DispatchAccessibleEvent(PRUint32 aEventType, nsIAccessible** aAccessible, nsPoint* aPoint = nsnull);
  void                    CreateRootAccessible();
  void                    ClearRootAccessible();
#endif
  virtual PRBool          AutoErase();
  nsPoint*                GetLastPoint() { return &mLastPoint; }

  PRInt32                 GetNewCmdMenuId() { mMenuCmdId++; return mMenuCmdId; }

  void                    InitEvent(nsGUIEvent& event, nsPoint* aPoint = nsnull);

  void                    SuppressBlurEvents(PRBool aSuppress);
  PRBool                  BlurEventsSuppressed();

protected:
  // special callback hook methods for pop ups
  static LRESULT CALLBACK MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam);
  static VOID    CALLBACK HookTimerForPopups( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime );
  static void             ScheduleHookTimer(HWND aWnd, UINT aMsgId);

  static void             RegisterSpecialDropdownHooks();
  static void             UnregisterSpecialDropdownHooks();

  static BOOL             DealWithPopups (UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult);

  static PRBool           EventIsInsideWindow(UINT Msg, nsWindow* aWindow);

  static nsWindow *       GetNSWindowPtr(HWND aWnd);
  static BOOL             SetNSWindowPtr(HWND aWnd, nsWindow * ptr);

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

  BOOL                    OnChar(UINT charCode);

  BOOL                    OnKeyDown( UINT aVirtualKeyCode, UINT aScanCode, LPARAM aKeyCode);
  BOOL                    OnKeyUp( UINT aVirtualKeyCode, UINT aScanCode, LPARAM aKeyCode);
  UINT                    MapFromNativeToDOM(UINT aNativeKeyCode);


  BOOL                    OnInputLangChange(HKL aHKL, LRESULT *oResult);
  BOOL                    OnIMEChar(BYTE aByte1, BYTE aByte2, LPARAM aKeyState);
  BOOL                    OnIMEComposition(LPARAM  aGCS);
  BOOL                    OnIMECompositionFull();
  BOOL                    OnIMEEndComposition();
  BOOL                    OnIMENotify(WPARAM  aIMN, LPARAM aData, LRESULT *oResult);
  BOOL                    OnIMERequest(WPARAM  aIMR, LPARAM aData, LRESULT *oResult, PRBool aUseUnicode = PR_FALSE);
  BOOL                    OnIMESelect(BOOL  aSelected, WORD aLangID);
  BOOL                    OnIMESetContext(BOOL aActive, LPARAM& aISC);
  BOOL                    OnIMEStartComposition();
  BOOL                    OnIMEReconvert(LPARAM aData, LRESULT *oResult, PRBool aUseUnicode);

  void                    GetCompositionString(HIMC aHIMC, DWORD aIndex, nsString* aStrUnicode, nsCString* aStrAnsi);

  virtual PRBool          DispatchKeyEvent(PRUint32 aEventType, WORD aCharCode, UINT aVirtualCharCode, LPARAM aKeyCode);

  virtual PRBool          DispatchFocus(PRUint32 aEventType, PRBool isMozWindowTakingFocus);
  virtual PRBool          OnScroll(UINT scrollCode, int cPos);
  virtual HBRUSH          OnControlColor();

  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK DefaultWindowProc(HWND hWns, UINT msg, WPARAM wParam, LPARAM lParam);

  static PRBool ConvertStatus(nsEventStatus aStatus);

  PRBool DispatchStandardEvent(PRUint32 aMsg);
  PRBool DispatchAppCommandEvent(PRUint32 aEventCommand);
  void RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);

  void GetNonClientBounds(nsRect &aRect);
  void HandleTextEvent(HIMC hIMEContext, PRBool aCheckAttr=PR_TRUE);
  BOOL HandleStartComposition(HIMC hIMEContext);
  void HandleEndComposition(void);
  void GetTextRangeList(PRUint32* textRangeListLengthResult, nsTextRangeArray* textRangeListResult);

  void ConstrainZLevel(HWND *aAfter);

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
  static PRUnichar* sIMEReconvertUnicode; // reconvert string

  // For describing composing frame
  static RECT*      sIMECompCharPos;

  nsSize        mLastSize;
  static        nsWindow* gCurrentWindow;
  nsPoint       mLastPoint;
  HWND          mWnd;
  HWND          mBorderlessParent;
#if 0
  HPALETTE      mPalette;
#endif
  WNDPROC       mPrevWndProc;
  HBRUSH        mBrush;

#ifdef MOZ_XUL
  HBITMAP       mMemoryBitmap;
  HDC           mMemoryDC;
  PRUint8*      mAlphaMask;
  PRPackedBool  mIsTranslucent;
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

  char          mLeadByte;
  PRUint32      mBlurEventSuppressionLevel;
  nsContentType mContentType;

  // XXX Temporary, should not be caching the font
  nsFont *      mFont;

  PRInt32       mPreferredWidth;
  PRInt32       mPreferredHeight;

  PRInt32       mMenuCmdId;

    // Window styles used by this window before chrome was hidden
  DWORD         mOldStyle;
  DWORD         mOldExStyle;

  static UINT   gCurrentKeyboardCP;
  static HKL    gKeyboardLayout;

  // Drag & Drop
  nsNativeDragTarget * mNativeDragTarget;

  // Enumeration of the methods which are accessable on the "main GUI thread"
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

  HDWP mDeferredPositioner;
  static UINT   uMSH_MOUSEWHEEL;

  // IME special message
  static UINT   uWM_MSIME_RECONVERT; // reconvert message for MSIME
  static UINT   uWM_MSIME_MOUSE;     // mouse message for MSIME
  static UINT   uWM_ATOK_RECONVERT;  // reconvert message for ATOK

  // Heap dump
  static UINT   uWM_HEAP_DUMP;       // Dump heap to a file

#ifdef ACCESSIBILITY
  nsIAccessible* mRootAccessible;
  static BOOL gIsAccessibilityOn;
  static HINSTANCE gmAccLib;
  static LPFNLRESULTFROMOBJECT gmLresultFromObject;
  static STDMETHODIMP_(LRESULT) LresultFromObject(REFIID riid, WPARAM wParam, LPUNKNOWN pAcc);
#endif

  static BOOL CALLBACK BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg);
  static BOOL CALLBACK BroadcastMsg(HWND aTopWindow, LPARAM aMsg);
  static BOOL CALLBACK DispatchStarvedPaints(HWND aTopWindow, LPARAM aMsg);

public:
  static void GlobalMsgWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {

public:
  ChildWindow() {}
  PRBool DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam = NULL, nsPoint* aPoint = nsnull);

protected:
  virtual DWORD WindowStyle();
};


#endif // Window_h__
