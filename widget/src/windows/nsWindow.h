/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Dean Tessman <dean_tessman@hotmail.com>
 */

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
#include "nsStringUtil.h"
#include "nsString.h"

#include "nsVoidArray.h"

class nsNativeDragTarget;
class nsIRollupListener;

class nsIMenuBar;

#ifdef ACCESSIBILITY
struct IAccessible;
#include "nsIAccessible.h"
#include "nsIAccessibleEventListener.h"
#endif

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))
#define COLOREF_2_NSRGB(color) \
            NS_RGB(GetRValue(color), GetGValue(color), GetBValue(color))

/** 
* Native IMM wrapper
*/
class nsIMM 
{
    //prototypes for DLL function calls...
    typedef LONG (CALLBACK *GetCompStrPtr)  (HIMC,DWORD,LPVOID,DWORD);
    typedef LONG (CALLBACK *GetContextPtr)  (HWND);
    typedef LONG (CALLBACK *RelContextPtr)  (HWND,HIMC);
    typedef LONG (CALLBACK *GetCStatusPtr)  (HIMC,LPDWORD,LPDWORD);
    typedef LONG (CALLBACK *SetCStatusPtr)  (HIMC,DWORD,DWORD);
    typedef LONG (CALLBACK *NotifyIMEPtr)   (HIMC,DWORD,DWORD,DWORD);
    typedef LONG (CALLBACK *SetCandWindowPtr)  (HIMC,LPCANDIDATEFORM);
    typedef LONG (CALLBACK *GetCompWindowPtr)  (HIMC,LPCOMPOSITIONFORM);

public:

    static nsIMM& LoadModule() {
      static nsIMM gIMM;
      return gIMM;
    }

    nsIMM(const char* aModuleName="IMM32.DLL") {
      mInstance=::LoadLibrary(aModuleName);
  	  NS_ASSERTION(mInstance!=NULL,"nsIMM.LoadLibrary failed.");

      mGetCompositionStringA=(mInstance) ? (GetCompStrPtr)GetProcAddress(mInstance,"ImmGetCompositionStringA") : 0;
      NS_ASSERTION(mGetCompositionStringA!=NULL,"nsIMM.ImmGetCompositionStringA failed.");

      mGetCompositionStringW=(mInstance) ? (GetCompStrPtr)GetProcAddress(mInstance,"ImmGetCompositionStringW") : 0;
	    NS_ASSERTION(mGetCompositionStringW!=NULL,"nsIMM.ImmGetCompositionStringW failed.");
      
	    mGetContext=(mInstance) ? (GetContextPtr)GetProcAddress(mInstance,"ImmGetContext") : 0;
	    NS_ASSERTION(mGetContext!=NULL,"nsIMM.ImmGetContext failed.");
      
	    mReleaseContext=(mInstance) ? (RelContextPtr)GetProcAddress(mInstance,"ImmReleaseContext") : 0;
	    NS_ASSERTION(mReleaseContext!=NULL,"nsIMM.ImmReleaseContext failed.");
      
	    mGetConversionStatus=(mInstance) ? (GetCStatusPtr)GetProcAddress(mInstance,"ImmGetConversionStatus") : 0;
	    NS_ASSERTION(mGetConversionStatus!=NULL,"nsIMM.ImmGetConversionStatus failed.");
      
	    mSetConversionStatus=(mInstance) ? (SetCStatusPtr)GetProcAddress(mInstance,"ImmSetConversionStatus") : 0;
	    NS_ASSERTION(mSetConversionStatus!=NULL,"nsIMM.ImmSetConversionStatus failed.");
      
	    mNotifyIME=(mInstance) ? (NotifyIMEPtr)GetProcAddress(mInstance,"ImmNotifyIME") : 0;
	    NS_ASSERTION(mNotifyIME!=NULL,"nsIMM.ImmNotifyIME failed.");
      
	    mSetCandiateWindow=(mInstance) ? (SetCandWindowPtr)GetProcAddress(mInstance,"ImmSetCandidateWindow") : 0;
	    NS_ASSERTION(mSetCandiateWindow!=NULL,"nsIMM.ImmSetCandidateWindow failed.");
      
	    mGetCompositionWindow=(mInstance) ? (GetCompWindowPtr)GetProcAddress(mInstance,"ImmGetCompositionWindow") : 0;
	    NS_ASSERTION(mGetCompositionWindow!=NULL,"nsIMM.ImmGetCompositionWindow failed.");
    }

    ~nsIMM() {
      if(mInstance) {
        ::FreeLibrary(mInstance);
      }
      mGetCompositionStringA= 0;
      mGetCompositionStringW= 0;
      mGetContext=0;
      mReleaseContext=0;
      mGetConversionStatus=0;
      mSetConversionStatus=0;
      mNotifyIME=0;
      mSetCandiateWindow=0;
      mGetCompositionWindow=0;
    }

    LONG GetCompositionStringA(HIMC h,DWORD d1,LPVOID v,DWORD d2) {
      return (mGetCompositionStringA) ? mGetCompositionStringA(h,d1,v,d2) : 0L;
    }

    LONG GetCompositionStringW(HIMC h,DWORD d1,LPVOID v,DWORD d2) {
      return (mGetCompositionStringW) ? mGetCompositionStringW(h,d1,v,d2) : 0L;
    }

    LONG GetContext(HWND anHWND) {
      return (mGetContext) ? mGetContext(anHWND) : 0L;
    }

    LONG ReleaseContext(HWND anHWND,HIMC anIMC) {
      return (mReleaseContext) ? mReleaseContext(anHWND,anIMC) : 0L;
    }

    LONG GetConversionStatus(HIMC h, LPDWORD w1, LPDWORD w2) {
      return (mGetConversionStatus) ? mGetConversionStatus(h,w1,w2) : 0L;
    }

    LONG SetConversionStatus(HIMC h,DWORD d1,DWORD d2) {
      return (mSetConversionStatus) ? mSetConversionStatus(h,d1,d2) : 0L;
    }

    LONG NotifyIME(HIMC h,DWORD d1,DWORD d2,DWORD d3) {
      return (mNotifyIME) ? mNotifyIME(h,d1,d2,d3) : 0L;
    }

    LONG SetCandidateWindow(HIMC h,LPCANDIDATEFORM l) {
      return (mSetCandiateWindow) ? mSetCandiateWindow(h,l) : 0L;
    }

    LONG GetCompositionWindow(HIMC h,LPCOMPOSITIONFORM l) {
      return (mGetCompositionWindow) ? mGetCompositionWindow(h,l) : 0L;
    }

private:

    HINSTANCE mInstance;
    GetCompStrPtr  mGetCompositionStringA;
    GetCompStrPtr  mGetCompositionStringW;
    GetContextPtr  mGetContext;
    RelContextPtr  mReleaseContext;
    GetCStatusPtr  mGetConversionStatus;
    SetCStatusPtr  mSetConversionStatus;
    NotifyIMEPtr   mNotifyIME;  
    SetCandWindowPtr  mSetCandiateWindow;   
    GetCompWindowPtr  mGetCompositionWindow;   
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
    NS_IMETHOD            Create(nsIWidget *aParent,
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
    virtual nsIWidget*      GetParent(void);
    NS_IMETHOD              Show(PRBool bState);
    NS_IMETHOD              IsVisible(PRBool & aState);
    NS_IMETHOD              PlaceBehind(nsIWidget *aWidget, PRBool aActivate);
    NS_IMETHOD              SetSizeMode(PRInt32 aMode);

    NS_IMETHOD              ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                             PRBool *aForWindow);

    NS_IMETHOD              CaptureMouse(PRBool aCapture);
    NS_IMETHOD              ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD              Resize(PRInt32 aWidth,
                                   PRInt32 aHeight,
                                   PRBool   aRepaint);
    NS_IMETHOD              Resize(PRInt32 aX,
                                   PRInt32 aY,
                                   PRInt32 aWidth,
                                   PRInt32 aHeight,
                                   PRBool   aRepaint);
    NS_IMETHOD              Enable(PRBool bState);
    NS_IMETHOD              SetFocus(PRBool aRaise);
    NS_IMETHOD              GetBounds(nsRect &aRect);
    NS_IMETHOD              GetClientBounds(nsRect &aRect);
    NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD              SetFont(const nsFont &aFont);
    NS_IMETHOD              SetCursor(nsCursor aCursor);
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
    NS_IMETHOD              SetTitle(const nsString& aTitle); 
    NS_IMETHOD              SetIcon(const nsAReadableString& aIconSpec); 
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
   	virtual void            ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY) {}

    NS_IMETHOD              CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);

    NS_IMETHOD              GetAttention();

    // nsIKBStateControl interface 

    NS_IMETHOD ResetInputState();

    HWND                    mBorderlessParent;

    // nsSwitchToUIThread interface
    virtual BOOL            CallMethod(MethodInfo *info);

            HWND            GetWindowHandle() { return mWnd; }
            WNDPROC         GetPrevWindowProc() { return mPrevWndProc; }

    virtual PRBool          DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint = nsnull);
#ifdef ACCESSIBILITY
    virtual PRBool          DispatchAccessibleEvent(PRUint32 aEventType, nsIAccessible** aAccessible, nsPoint* aPoint = nsnull);
    void                    CreateRootAccessible();
#endif
    virtual PRBool          AutoErase();
    nsPoint*                GetLastPoint() { return &mLastPoint; }

    PRInt32                 GetNewCmdMenuId() { mMenuCmdId++; return mMenuCmdId;}

    void InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint = nsnull);

protected:
    // special callback hook methods for pop ups
    static LRESULT CALLBACK MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam);
    static VOID    CALLBACK HookTimerForPopups( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime );
    static void             ScheduleHookTimer(HWND aWnd, UINT aMsgId);

    static void             RegisterSpecialDropdownHooks();
    static void             UnregisterSpecialDropdownHooks();

    static  BOOL            DealWithPopups ( UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult ) ;

    static  PRBool          EventIsInsideWindow(UINT Msg, nsWindow* aWindow); 

    static  nsWindow *      GetNSWindowPtr(HWND aWnd);
    static  BOOL            SetNSWindowPtr(HWND aWnd, nsWindow * ptr);

    virtual PRBool          ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue);
    virtual PRBool          DispatchWindowEvent(nsGUIEvent* event);
    virtual PRBool          DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus);

     // Allow Derived classes to modify the height that is passed
     // when the window is created or resized.
    virtual PRInt32         GetHeight(PRInt32 aProposedHeight);
    virtual LPCTSTR         WindowClass();
    virtual DWORD           WindowStyle();
    virtual DWORD           WindowExStyle();

    virtual void            SubclassWindow(BOOL bState);

    virtual void            OnDestroy();
    virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool          OnPaint();
    virtual PRBool          OnResize(nsRect &aWindowRect);

    BOOL                    OnChar(UINT mbcsCharCode, UINT virtualKeyCode, bool isMultibyte);

    BOOL                    OnKeyDown( UINT aVirtualKeyCode, UINT aScanCode);
    BOOL                    OnKeyUp( UINT aVirtualKeyCode, UINT aScanCode);
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

    ULONG                   IsSpecialChar(UINT aVirtualKeyCode, WORD *aAsciiKey);
    virtual PRBool          DispatchKeyEvent(PRUint32 aEventType, WORD aCharCode, UINT aVirtualCharCode);

    virtual PRBool          DispatchFocus(PRUint32 aEventType, PRBool isMozWindowTakingFocus);
    virtual PRBool          OnScroll(UINT scrollCode, int cPos);
    virtual HBRUSH          OnControlColor();

    static LRESULT CALLBACK WindowProc(HWND hWnd,
                                        UINT msg,
                                        WPARAM wParam,
                                        LPARAM lParam);
#ifdef MOZ_AIMM
    static LRESULT CALLBACK DefaultWindowProc(HWND hWns, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

    static PRBool ConvertStatus(nsEventStatus aStatus);

    PRBool DispatchStandardEvent(PRUint32 aMsg);
    void RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);

    void GetNonClientBounds(nsRect &aRect);
    void HandleTextEvent(HIMC hIMEContext, PRBool aCheckAttr=PR_TRUE);
    void HandleStartComposition(HIMC hIMEContext);
    void HandleEndComposition(void);
    void MapDBCSAtrributeArrayToUnicodeOffsets(PRUint32* textRangeListLengthResult, nsTextRangeArray* textRangeListResult);

    void ConstrainZLevel(HWND *aAfter);

private:


#ifdef DEBUG
  void DebugPrintEvent(nsGUIEvent &   aEvent,
                       HWND           aWnd);
#endif

protected:
    nsSize      mLastSize;
    static      nsWindow* gCurrentWindow;
    PRBool      mIsTopWidgetWindow;
    nsPoint     mLastPoint;
    HWND        mWnd;
#if 0
    HPALETTE    mPalette;
#endif
    WNDPROC     mPrevWndProc;

    PRBool      mHas3DBorder;
    HBRUSH      mBrush;
    PRBool      mIsShiftDown;
    PRBool      mIsControlDown;
    PRBool      mIsAltDown;
    PRBool      mIsDestroying;
    PRBool      mOnDestroyCalled;
    PRBool      mIsVisible;
    // XXX Temporary, should not be caching the font
    nsFont *    mFont;

    PRInt32     mPreferredWidth;
    PRInt32     mPreferredHeight;

    PRInt32       mMenuCmdId;

	// For Input Method Support
	DWORD		mIMEProperty;
	PRBool		mIMEIsComposing;
  PRBool    mIMEIsStatusChanged;
	nsCString*	mIMECompString;
	nsString*	mIMECompUnicode;
	PRUint8*	mIMEAttributeString;
	PRInt32		mIMEAttributeStringLength;
	PRInt32		mIMEAttributeStringSize;
	PRUint32*	mIMECompClauseString;
	PRInt32		mIMECompClauseStringLength;
	PRInt32		mIMECompClauseStringSize;
	long		mIMECursorPosition;

    PRUnichar*  mIMEReconvertUnicode; // reconvert string

	static UINT		gCurrentKeyboardCP;
	static HKL		gKeyboardLayout;

  PRBool  mIsInMouseCapture;


    // Drag & Drop
    nsNativeDragTarget * mNativeDragTarget;

    // Enumeration of the methods which are accessable on the "main GUI thread"
    // via the CallMethod(...) mechanism...
    // see nsSwitchToUIThread
    enum {
        CREATE       = 0x0101,
        CREATE_NATIVE,
        DESTROY, 
        SET_FOCUS,
        SET_CURSOR,
        CREATE_HACK
    };

    static BOOL sIsRegistered;

    HDWP mDeferredPositioner;
    static UINT uMSH_MOUSEWHEEL;

    // IME special message
    static UINT uWM_MSIME_RECONVERT; // reconvert messge for MSIME
    static UINT uWM_MSIME_MOUSE;     // mouse messge for MSIME
    static UINT uWM_ATOK_RECONVERT;  // reconvert messge for ATOK

#ifdef ACCESSIBILITY
    IAccessible* mRootAccessible;
    static BOOL gIsAccessibilityOn;
#endif
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {

public:
                            ChildWindow(){}
    PRBool          DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint = nsnull);

protected:
    virtual DWORD           WindowStyle();
};


#endif // Window_h__
