/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Peter Bajusz <hyp-x@inf.bme.hu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Mark Hammond <markh@activestate.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if defined(DEBUG_ftang)
//#define KE_DEBUG
//#define DEBUG_IME
//#define DEBUG_IME2
//#define DEBUG_KBSTATE
#endif

#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h" 
#include "nsIPref.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsStringUtil.h"
#include <windows.h>

//#include <winuser.h>
#include <zmouse.h>
//#include "sysmets.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include <commctrl.h>
#include "prtime.h"
#include "nsIRenderingContextWin.h"
#include "nsIImage.h"

#ifdef ACCESSIBILITY
#include "nsIAccessible.h"
#include "Accessible.h"
#endif

#include <imm.h>
#ifdef MOZ_AIMM
#include "aimm.h"
#endif // MOZ_AIMM

#include "nsNativeDragTarget.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIRegion.h"

//~~~ windowless plugin support
#include "nsplugindefs.h"

// For clipboard support
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsWidgetsCID.h"

#include "nsITimer.h"
#include "nsITimerQueue.h"

// For SetIcon
#include "nsSpecialSystemDirectory.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"

#define kWindowPositionSlop 20

#ifndef SPI_GETWHEELSCROLLLINES
#define SPI_GETWHEELSCROLLLINES 104
#endif

static NS_DEFINE_CID(kCClipboardCID,       NS_CLIPBOARD_CID);
static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_CID(kTimerManagerCID, NS_TIMERMANAGER_CID);

////////////////////////////////////////////////////
// Manager for Registering and unregistering OLE
// This is needed for drag & drop & Clipboard support
////////////////////////////////////////////////////
class OleRegisterMgr {
public:
  ~OleRegisterMgr();
protected:
  OleRegisterMgr();

  static OleRegisterMgr mSingleton;
};
OleRegisterMgr OleRegisterMgr::mSingleton;

OleRegisterMgr::OleRegisterMgr()
{
  //DWORD dwVer = ::OleBuildVersion();

  if (FAILED(::OleInitialize(NULL))) {
    NS_ASSERTION(0, "***** OLE has not been initialized!\n");
  } else {
#ifdef DEBUG
    //printf("***** OLE has been initialized!\n");
#endif
  }
}
 
OleRegisterMgr::~OleRegisterMgr()
{
#ifdef DEBUG
  //printf("***** OLE has been Uninitialized!\n");
#endif
  ::OleUninitialize();
}

////////////////////////////////////////////////////
// nsWindow Class static variable defintions
////////////////////////////////////////////////////
BOOL nsWindow::sIsRegistered       = FALSE;
UINT nsWindow::uMSH_MOUSEWHEEL     = 0;
UINT nsWindow::uWM_MSIME_RECONVERT = 0; // reconvert messge for MSIME
UINT nsWindow::uWM_MSIME_MOUSE     = 0; // mouse messge for MSIME
UINT nsWindow::uWM_ATOK_RECONVERT  = 0; // reconvert messge for ATOK
#ifdef ACCESSIBILITY
BOOL nsWindow::gIsAccessibilityOn = FALSE;
#endif
nsWindow* nsWindow::gCurrentWindow = nsnull;
////////////////////////////////////////////////////

////////////////////////////////////////////////////
// Rollup Listener - static variable defintions
////////////////////////////////////////////////////
static nsIRollupListener * gRollupListener           = nsnull;
static nsIWidget         * gRollupWidget             = nsnull;
static PRBool              gRollupConsumeRollupEvent = PR_FALSE;

// Hook Data Memebers for Dropdowns
//
// gProcessHook Tells the hook methods whether they should be 
//              processing the hook messages
//
static HHOOK        gMsgFilterHook = NULL;
static HHOOK        gCallProcHook  = NULL;
static HHOOK        gCallMouseHook = NULL;
static PRPackedBool gProcessHook   = PR_FALSE;
static UINT         gRollupMsgId   = 0;
static UINT         gHookTimerId   = 0;
////////////////////////////////////////////////////


////////////////////////////////////////////////////
// Mouse Clicks - static variable defintions 
// for figuring out 1 - 3 Clicks
////////////////////////////////////////////////////
static POINT gLastMousePoint;
static LONG  gLastMouseDownTime = 0L;
static LONG  gLastClickCount    = 0L;
////////////////////////////////////////////////////

#if 0
static PRBool is_vk_down(int vk)
{
   SHORT st = GetKeyState(vk);
#ifdef DEBUG
   printf("is_vk_down vk=%x st=%x\n",vk, st);
#endif
   return (st & 0x80) ? PR_TRUE : PR_FALSE;
}
#define IS_VK_DOWN is_vk_down
#else
#define IS_VK_DOWN(a) (PRBool)(((GetKeyState(a) & 0x80)) ? (PR_TRUE) : (PR_FALSE))
#endif


//
// input method offsets
//
#define IME_X_OFFSET	0
#define IME_Y_OFFSET	0



#define IS_IME_CODEPAGE(cp) ((932==(cp))||(936==(cp))||(949==(cp))||(950==(cp)))

//
// Macro for Active Input Method Manager (AIMM) support.
// Use AIMM method instead of Win32 Imm APIs.
//
#ifdef MOZ_AIMM

#define NS_IMM_GETCOMPOSITIONSTRING(hIMC, dwIndex, pBuf, dwBufLen, compStrLen) \
{ \
  compStrLen = 0; \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->GetCompositionStringA(hIMC, dwIndex, dwBufLen, &(compStrLen), pBuf); \
   else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      compStrLen = theIMM.GetCompositionStringA(hIMC, dwIndex, pBuf, dwBufLen); \
   } \
}

#define NS_IMM_GETCOMPOSITIONSTRINGW(hIMC, dwIndex, pBuf, dwBufLen, compStrLen) \
{ \
  compStrLen = 0; \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->GetCompositionStringW(hIMC, dwIndex, dwBufLen, &(compStrLen), pBuf); \
    else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      compStrLen = theIMM.GetCompositionStringW(hIMC, dwIndex, pBuf, dwBufLen); \
    } \
}

#define NS_IMM_GETCONTEXT(hWnd, hIMC) \
  { \
    hIMC = NULL; \
    if (nsToolkit::gAIMMApp) \
      nsToolkit::gAIMMApp->GetContext(hWnd, &(hIMC)); \
    else { \
      nsIMM& theIMM = nsIMM::LoadModule(); \
      hIMC = (HIMC)theIMM.GetContext(hWnd);  \
    } \
  }

#define NS_IMM_GETCONVERSIONSTATUS(hIMC, pfdwConversion, pfdwSentence, bRet) \
  { \
    bRet = TRUE; \
    if (nsToolkit::gAIMMApp) { \
      bRet = (nsToolkit::gAIMMApp->GetConversionStatus(hIMC, pfdwConversion, pfdwSentence) == S_OK); \
    } \
    else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      (theIMM.GetConversionStatus(hIMC, pfdwConversion, pfdwSentence)); \
    }\
  }

#define NS_IMM_RELEASECONTEXT(hWnd, hIMC) \
  { \
    if (nsToolkit::gAIMMApp) \
      nsToolkit::gAIMMApp->ReleaseContext(hWnd, hIMC); \
    else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      theIMM.ReleaseContext(hWnd, hIMC); \
    } \
  }

#define NS_IMM_NOTIFYIME(hIMC, dwAction, dwIndex, dwValue, bRtn) \
  { \
    bRtn = TRUE; \
    if (nsToolkit::gAIMMApp) { \
      bRtn = (nsToolkit::gAIMMApp->NotifyIME(hIMC, dwAction, dwIndex, dwValue) == S_OK); \
    }\
    else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      (theIMM.NotifyIME(hIMC, dwAction, dwIndex, dwValue)); \
    } \
  }

#define NS_IMM_SETCANDIDATEWINDOW(hIMC, candForm) \
  { \
    if (nsToolkit::gAIMMApp) \
      (nsToolkit::gAIMMApp->SetCandidateWindow(hIMC, candForm) == S_OK); \
    else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      theIMM.SetCandidateWindow(hIMC, candForm); \
    } \
  }

#define NS_IMM_GETCOMPOSITIONWINDOW(hIMC, compForm) \
  { \
    if (nsToolkit::gAIMMApp) \
      (nsToolkit::gAIMMApp->GetCompositionWindow(hIMC, compForm) == S_OK); \
    else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      theIMM.GetCompositionWindow(hIMC, compForm); \
    } \
  }

#define NS_IMM_SETCONVERSIONSTATUS(hIMC, pfdwConversion, pfdwSentence, bRet) \
  { \
    bRet = TRUE; \
    if (nsToolkit::gAIMMApp) \
      bRet = (nsToolkit::gAIMMApp->SetConversionStatus(hIMC, (pfdwConversion), (pfdwSentence)) == S_OK); \
    else { \
      nsIMM &theIMM = nsIMM::LoadModule(); \
      theIMM.SetConversionStatus(hIMC, (pfdwConversion), (pfdwSentence)); \
    } \
  } 

#else /* !MOZ_AIMM */

#define NS_IMM_GETCOMPOSITIONSTRING(hIMC, dwIndex, pBuf, dwBufLen, compStrLen) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    compStrLen = theIMM.GetCompositionStringA(hIMC, dwIndex, pBuf, dwBufLen); \
  }

#define NS_IMM_GETCOMPOSITIONSTRINGW(hIMC, dwIndex, pBuf, dwBufLen, compStrLen) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    compStrLen = theIMM.GetCompositionStringW(hIMC, dwIndex, pBuf, dwBufLen); \
  }

#define NS_IMM_GETCONTEXT(hWnd, hIMC) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    hIMC = theIMM.GetContext(hWnd); \
  }

#define NS_IMM_GETCONVERSIONSTATUS(hIMC, lpfdwConversion, lpfdwSentence) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.GetConversionStatus(hIMC, (lpfdwConversion), (lpfdwSentence)); \
  }

#define NS_IMM_RELEASECONTEXT(hWnd, hIMC) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.ReleaseContext(hWnd, hIMC); \
  }

#define NS_IMM_NOTIFYIME(hIMC, dwAction, dwIndex, dwValue) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.NotifyIME(hIMC, dwAction, dwIndex, dwValue); \
  }

#define NS_IMM_SETCANDIDATEWINDOW(hIMC, candForm) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.SetCandidateWindow(hIMC, candForm); \
  }

#define NS_IMM_SETCONVERSIONSTATUS(hIMC, lpfdwConversion, lpfdwSentence) \
  { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.SetConversionStatus(hIMC, (lpfdwConversion), (lpfdwSentence)); \
  }

#endif /* MOZ_AIMM */

//
// for reconversion define
//

// VC++5.0 header doesn't have reconvertion structure and message.
#ifndef WM_IME_REQUEST
typedef struct tagRECONVERTSTRING {
    DWORD dwSize;
    DWORD dwVersion;
    DWORD dwStrLen;
    DWORD dwStrOffset;
    DWORD dwCompStrLen;
    DWORD dwCompStrOffset;
    DWORD dwTargetStrLen;
    DWORD dwTargetStrOffset;
} RECONVERTSTRING, FAR * LPRECONVERTSTRING;

#define IMR_RECONVERTSTRING             0x0004
#define WM_IME_REQUEST                  0x0288
#endif

// from http://msdn.microsoft.com/library/specs/msime.h
#define RWM_RECONVERT       TEXT("MSIMEReconvert")
#define RWM_MOUSE           TEXT("MSIMEMouseOperation")

#define IMEMOUSE_NONE       0x00    // no mouse button was pushed
#define IMEMOUSE_LDOWN      0x01
#define IMEMOUSE_RDOWN      0x02
#define IMEMOUSE_MDOWN      0x04
#define IMEMOUSE_WUP        0x10    // wheel up
#define IMEMOUSE_WDOWN      0x20    // wheel down
  
// from http://www.justsystem.co.jp/tech/atok/api12_04.html#4_11
#define MSGNAME_ATOK_RECONVERT TEXT("Atok Message for ReconvertString")


static PRBool LangIDToCP(WORD aLangID, UINT& oCP)
{
	int localeid=MAKELCID(aLangID,SORT_DEFAULT);
	int numchar=GetLocaleInfo(localeid,LOCALE_IDEFAULTANSICODEPAGE,NULL,0);
        char cp_on_stack[32];

	char* cp_name;
        if(numchar > 32)
           cp_name  = new char[numchar];
        else 
           cp_name = cp_on_stack;
	if (cp_name) {
           GetLocaleInfo(localeid,LOCALE_IDEFAULTANSICODEPAGE,cp_name,numchar);
           oCP = atoi(cp_name);
           if(cp_name != cp_on_stack)
               delete [] cp_name;
           return PR_TRUE;
	} else {
           oCP = CP_ACP;
           return PR_FALSE;
        }
}

//-------------------------------------------------------------------------
//
// nsISupport stuff
//
//-------------------------------------------------------------------------
NS_IMPL_ADDREF(nsWindow)
NS_IMPL_RELEASE(nsWindow)
NS_IMETHODIMP nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    if (aIID.Equals(NS_GET_IID(nsIKBStateControl))) {
        *aInstancePtr = (void*) ((nsIKBStateControl*)this);
    	NS_ADDREF((nsBaseWidget*)this);
        // NS_ADDREF_THIS();
        return NS_OK;
    }

    return nsBaseWidget::QueryInterface(aIID,aInstancePtr);
}
//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
#ifdef ACCESSIBILITY
nsWindow::nsWindow() : nsBaseWidget(), mRootAccessible(NULL)
#else
nsWindow::nsWindow() : nsBaseWidget()
#endif
{
    NS_INIT_REFCNT();
    mWnd                = 0;
    mPrevWndProc        = NULL;
    mBackground         = ::GetSysColor(COLOR_BTNFACE);
    mBrush              = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
    mForeground         = ::GetSysColor(COLOR_WINDOWTEXT);
    mIsShiftDown        = PR_FALSE;
    mIsControlDown      = PR_FALSE;
    mIsAltDown          = PR_FALSE;
    mIsDestroying       = PR_FALSE;
    mOnDestroyCalled    = PR_FALSE;
    mDeferredPositioner = NULL;
    mLastPoint.x        = 0;
    mLastPoint.y        = 0;
    mPreferredWidth     = 0;
    mPreferredHeight    = 0;
    mFont               = nsnull;
    mIsVisible          = PR_FALSE;
    mHas3DBorder        = PR_FALSE;
    mWindowType         = eWindowType_child;
    mBorderStyle        = eBorderStyle_default;
    mBorderlessParent   = 0;
    mIsInMouseCapture   = PR_FALSE;
    mIsInMouseWheelProcessing = PR_FALSE;
    mLastSize.width = 0;
    mLastSize.height = 0;

	  mIMEProperty		= 0;
	  mIMEIsComposing		= PR_FALSE;
    mIMEIsStatusChanged = PR_FALSE;
	  mIMECompString = NULL;
	  mIMECompUnicode = NULL;
	  mIMEAttributeString = NULL;
	  mIMEAttributeStringSize = 0;
	  mIMEAttributeStringLength = 0;
	  mIMECompClauseString = NULL;
	  mIMECompClauseStringSize = 0;
	  mIMECompClauseStringLength = 0;
	  mIMEReconvertUnicode = NULL;

  static BOOL gbInitGlobalValue = FALSE;
  if(! gbInitGlobalValue) {
    gbInitGlobalValue = TRUE;
    gKeyboardLayout = GetKeyboardLayout(0);
    LangIDToCP((WORD)(0x0FFFFL & (DWORD)gKeyboardLayout), gCurrentKeyboardCP);

    //
    // Reconvert message for Windows 95 / NT 4.0
    //

    // MS-IME98/2000
    nsWindow::uWM_MSIME_RECONVERT = ::RegisterWindowMessage(RWM_RECONVERT);

    // ATOK12/13
    nsWindow::uWM_ATOK_RECONVERT  = ::RegisterWindowMessage(MSGNAME_ATOK_RECONVERT);

    // mouse message of MSIME98/2000
    nsWindow::uWM_MSIME_MOUSE     = ::RegisterWindowMessage(RWM_MOUSE);
  }

  mNativeDragTarget = nsnull;
  mIsTopWidgetWindow = PR_FALSE;
 
  if (!nsWindow::uMSH_MOUSEWHEEL)
    nsWindow::uMSH_MOUSEWHEEL = RegisterWindowMessage(MSH_MOUSEWHEEL);
}


HKL nsWindow::gKeyboardLayout = 0;
UINT nsWindow::gCurrentKeyboardCP = 0;

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
#ifdef ACCESSIBILITY
  if (mRootAccessible) {
    mRootAccessible->Release();
    mRootAccessible = nsnull;
  }
#endif

  mIsDestroying = PR_TRUE;
  if (gCurrentWindow == this) {
    gCurrentWindow = nsnull;
  }

  MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
  if (mouseTrailer->GetMouseTrailerWindow() == this) {
    mouseTrailer->DestroyTimer();
  } 

  // If the widget was released without calling Destroy() then the native
  // window still exists, and we need to destroy it
  if (NULL != mWnd) {
    Destroy();
  }

  //XXX Temporary: Should not be caching the font
  delete mFont;

  //
  // delete any of the IME structures that we allocated
  //
  if (mIMECompString!=NULL) 
	delete mIMECompString;
  if (mIMECompUnicode!=NULL) 
	delete mIMECompUnicode;
  if (mIMEAttributeString!=NULL) 
	delete [] mIMEAttributeString;
  if (mIMECompClauseString!=NULL) 
	delete [] mIMECompClauseString;
  if (mIMEReconvertUnicode)
    nsMemory::Free(mIMEReconvertUnicode);

  NS_IF_RELEASE(mNativeDragTarget);

}


NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
  if (PR_TRUE == aCapture) { 
    MouseTrailer::SetCaptureWindow(this);
    ::SetCapture(mWnd);
  } else {
    MouseTrailer::SetCaptureWindow(NULL);
    ::ReleaseCapture();
  }
  mIsInMouseCapture = aCapture;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Default for height modification is to do nothing
//
//-------------------------------------------------------------------------

PRInt32 nsWindow::GetHeight(PRInt32 aProposedHeight)
{
  return(aProposedHeight);
}

//-------------------------------------------------------------------------
//
// Deferred Window positioning
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::BeginResizingChildren(void)
{
  if (NULL == mDeferredPositioner)
    mDeferredPositioner = ::BeginDeferWindowPos(1);
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  if (NULL != mDeferredPositioner) {
    ::EndDeferWindowPos(mDeferredPositioner);
    mDeferredPositioner = NULL;
  }
  return NS_OK;
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  POINT point;
  point.x = aOldRect.x;
  point.y = aOldRect.y;
  ::ClientToScreen((HWND)GetNativeData(NS_NATIVE_WINDOW), &point);
  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  POINT point;
  point.x = aOldRect.x;
  point.y = aOldRect.y;
  ::ScreenToClient((HWND)GetNativeData(NS_NATIVE_WINDOW), &point);
  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
} 

//-------------------------------------------------------------------------
//
// Convert nsEventStatus value to a windows boolean
//
//-------------------------------------------------------------------------

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  switch(aStatus) {
  case nsEventStatus_eIgnore:
    return PR_FALSE;
  case nsEventStatus_eConsumeNoDefault:
    return PR_TRUE;
  case nsEventStatus_eConsumeDoDefault:
    return PR_FALSE;
  default:
    NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
    break;
  }
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------
void nsWindow::InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint)
{
    event.widget = this;
    NS_ADDREF(event.widget);

    if (nsnull == aPoint) {     // use the point from the event
      // get the message position in client coordinates and in twips
      DWORD pos = ::GetMessagePos();
      POINT cpos;

      cpos.x = (short)LOWORD(pos);
      cpos.y = (short)HIWORD(pos);

      if (mWnd != NULL) {
        ::ScreenToClient(mWnd, &cpos);
        event.point.x = cpos.x;
        event.point.y = cpos.y;
      } else {
        event.point.x = 0;
        event.point.y = 0;
      }
    }
    else {                      // use the point override if provided
      event.point.x = aPoint->x;
      event.point.y = aPoint->y;
    }

    event.time = ::GetMessageTime();
    event.message = aEventType;

    mLastPoint.x = event.point.x;
    mLastPoint.y = event.point.y;
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
#ifdef NS_DEBUG
  debug_DumpEvent(stdout,
                  event->widget,
                  event,
                  nsCAutoString("something"),
                  (PRInt32) mWnd);
#endif // NS_DEBUG

  aStatus = nsEventStatus_eIgnore;
 
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(event);
  }

    // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*event);
  }

  // the window can be destroyed during processing of seemingly innocuous events like, say,
  // mousedowns due to the magic of scripting. mousedowns will return nsEventStatus_eIgnore,
  // which causes problems with the deleted window. therefore:
  if (mOnDestroyCalled)
    aStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

PRBool nsWindow::DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus) {
  DispatchEvent(event, aStatus);
  return ConvertStatus(aStatus);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  InitEvent(event, aMsg);

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                            PRBool aDoCapture, 
                                            PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    /* we haven't bothered carrying a weak reference to gRollupWidget because
       we believe lifespan is properly scoped. this next assertion helps
       assure that remains true. */
    NS_ASSERTION(!gRollupWidget, "rollup widget reassigned before release");
    gRollupConsumeRollupEvent = aConsumeRollupEvent;
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);

    if (!gMsgFilterHook && !gCallProcHook && !gCallMouseHook) {
      RegisterSpecialDropdownHooks();
    }
    gProcessHook = PR_TRUE;

  } else {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);

    gProcessHook = PR_FALSE;
    UnregisterSpecialDropdownHooks();
  }

  return NS_OK;
}

PRBool 
nsWindow::EventIsInsideWindow(UINT Msg, nsWindow* aWindow) 
{
  RECT r;

  if (Msg == WM_ACTIVATE)
    // don't care about activation/deactivation
    return PR_FALSE;

  ::GetWindowRect(aWindow->mWnd, &r);
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = (short)LOWORD(pos);
  mp.y = (short)HIWORD(pos);

  // was the event inside this window?
  return (PRBool) PtInRect(&r, mp);
}

static LPCTSTR GetNSWindowPropName() {
  static ATOM atom = 0;

  // this is threadsafe, even without locking;
  // even if there's a race, GlobalAddAtom("MozillaWindowPtr")
  // will just return the same value
  if (!atom) {
    atom = ::GlobalAddAtom("MozillansIWidgetPtr");
  }
  return MAKEINTATOM(atom);
}

nsWindow * nsWindow::GetNSWindowPtr(HWND aWnd) {
  return (nsWindow *) ::GetProp(aWnd, GetNSWindowPropName());
}

BOOL nsWindow::SetNSWindowPtr(HWND aWnd, nsWindow * ptr) {
  if (ptr == NULL) {
    ::RemoveProp(aWnd, GetNSWindowPropName());
    return TRUE;
  } else {
    return ::SetProp(aWnd, GetNSWindowPropName(), (HANDLE)ptr);
  }
}

//
// DealWithPopups
//
// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
//
BOOL
nsWindow :: DealWithPopups ( UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult )
{
  if ( gRollupListener && gRollupWidget) {

    if (inMsg == WM_ACTIVATE || inMsg == WM_NCLBUTTONDOWN || inMsg == WM_LBUTTONDOWN ||
      inMsg == WM_RBUTTONDOWN || inMsg == WM_MBUTTONDOWN || 
      inMsg == WM_NCMBUTTONDOWN || inMsg == WM_NCRBUTTONDOWN || inMsg == WM_MOUSEACTIVATE ||
      inMsg == WM_MOUSEWHEEL || inMsg == uMSH_MOUSEWHEEL || inMsg == WM_ACTIVATEAPP ||
      inMsg == WM_MENUSELECT || inMsg == WM_MOVING || inMsg == WM_SIZING || inMsg == WM_GETMINMAXINFO)
    {
      // Rollup if the event is outside the popup.
      PRBool rollup = !nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)gRollupWidget);

      if (rollup && (inMsg == WM_MOUSEWHEEL || inMsg == uMSH_MOUSEWHEEL)) 
      {
        gRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);
        *outResult = PR_TRUE;
      }

      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      if (rollup) {
        nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
        if ( menuRollup ) {
          nsCOMPtr<nsISupportsArray> widgetChain;
          menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
          if ( widgetChain ) {
            PRUint32 count = 0;
            widgetChain->Count(&count);
            for ( PRUint32 i = 0; i < count; ++i ) {
              nsCOMPtr<nsISupports> genericWidget;
              widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
              nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
              if ( widget ) {
                nsIWidget* temp = widget.get();
                if ( nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)temp) ) {
                  rollup = PR_FALSE;
                  break;
                }
              }
            } // foreach parent menu widget
          }
        } // if rollup listener knows about menus
      }

      if (inMsg == WM_MOUSEACTIVATE) {
        // Prevent the click inside the popup from causing a change in window
        // activation. Since the popup is shown non-activated, we need to eat 
        // any requests to activate the window while it is displayed. Windows 
        // will automatically activate the popup on the mousedown otherwise.
        if (!rollup) {
          *outResult = MA_NOACTIVATE;
          return TRUE;
        }
        else
        {
          UINT uMsg = HIWORD(inLParam);
          if (uMsg == WM_MOUSEMOVE)
          {
            // WM_MOUSEACTIVATE cause by moving the mouse - X-mouse (eg. TweakUI)
            // must be enabled in Windows.
            // Assume that if the popup doesn't roll up on mouse wheel events,
            // then it shouldn't roll up on this event either.  This allows the
            // menu to stay open when it's activated by X-Mouse.
            gRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);
            if (!rollup)
            {
              *outResult = MA_NOACTIVATE;
              return true;
            }
          }
        }
      }

      // if we've still determined that we should still rollup everything, do it.
      else if ( rollup ) {
        gRollupListener->Rollup();

        // Tell hook to stop processing messages
        gProcessHook = PR_FALSE;
        gRollupMsgId = 0;

        // return TRUE tells Windows that the event is consumed, 
        // false allows the event to be dispatched
        //
        // So if we are NOT supposed to be consuming events, let it go through
        if (gRollupConsumeRollupEvent) {
          *outResult = TRUE;
          return TRUE;
        } 
      }
    } // if event that might trigger a popup to rollup    
  } // if rollup listeners registered

  return FALSE;

} // DealWithPopups


//-------------------------------------------------------------------------
//
// the nsWindow procedure for all nsWindows in this toolkit
//
//-------------------------------------------------------------------------
LRESULT CALLBACK nsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT popupHandlingResult;
    if ( DealWithPopups(msg, wParam, lParam, &popupHandlingResult) )
      return popupHandlingResult;

    // Get the window which caused the event and ask it to process the message
    nsWindow *someWindow = GetNSWindowPtr(hWnd);

    // XXX This fixes 50208 and we are leaving 51174 open to further investigate
    // why we are hitting this assert
    if (nsnull == someWindow) {
      NS_ASSERTION(someWindow, "someWindow is null, cannot call any CallWindowProc");      
      return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }

    // hold on to the window for the life of this method, in case it gets
    // deleted during processing. yes, it's a double hack, since someWindow
    // is not really an interface.
    nsCOMPtr<nsISupports> kungFuDeathGrip;
    if (!someWindow->mIsDestroying) // not if we're in the destructor!
      kungFuDeathGrip = do_QueryInterface((nsBaseWidget*)someWindow);

    // Re-direct a tab change message destined for its parent window to the
    // the actual window which generated the event.
    if (msg == WM_NOTIFY) {
      LPNMHDR pnmh = (LPNMHDR) lParam;
      if (pnmh->code == TCN_SELCHANGE) {
        someWindow = GetNSWindowPtr(pnmh->hwndFrom);
      }
    }

    if (nsnull != someWindow) {
        LRESULT retValue;
        if (PR_TRUE == someWindow->ProcessMessage(msg, wParam, lParam, &retValue)) {
            return retValue;
        }
    }

#if defined(STRICT)
    return ::CallWindowProc((WNDPROC)someWindow->GetPrevWindowProc(), hWnd, 
                            msg, wParam, lParam);
#else
    return ::CallWindowProc((FARPROC)someWindow->GetPrevWindowProc(), hWnd, 
                            msg, wParam, lParam);
#endif
}

#ifdef MOZ_AIMM
//
// Default Window proceduer for AIMM support.
//
LRESULT CALLBACK nsWindow::DefaultWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if(nsToolkit::gAIMMApp)
  {
    LRESULT lResult;
    if (nsToolkit::gAIMMApp->OnDefWindowProc(hWnd, msg, wParam, lParam, &lResult) == S_OK)
      return lResult;
  }
  return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
#endif

static BOOL CALLBACK DummyDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  return FALSE;
}

//WINOLEAPI oleStatus;
//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
nsresult nsWindow::StandardWindowCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent)
{
    nsIWidget *baseParent = aInitData &&
                 (aInitData->mWindowType == eWindowType_dialog ||
                  aInitData->mWindowType == eWindowType_toplevel) ?
                  nsnull : aParent;

    mIsTopWidgetWindow = (nsnull == baseParent);

    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext, 
       aAppShell, aToolkit, aInitData);

    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
  
    nsToolkit* toolkit = (nsToolkit *)mToolkit;
    if (toolkit) {
    if (!toolkit->IsGuiThread()) {
        DWORD args[7];
        args[0] = (DWORD)aParent;
        args[1] = (DWORD)&aRect;
        args[2] = (DWORD)aHandleEventFunction;
        args[3] = (DWORD)aContext;
        args[4] = (DWORD)aAppShell;
        args[5] = (DWORD)aToolkit;
        args[6] = (DWORD)aInitData;

        if (nsnull != aParent) {
           // nsIWidget parent dispatch
          MethodInfo info(this, nsWindow::CREATE, 7, args);
          toolkit->CallMethod(&info);
           return NS_OK;
        }
        else {
            // Native parent dispatch
          MethodInfo info(this, nsWindow::CREATE_NATIVE, 5, args);
          toolkit->CallMethod(&info);
          return NS_OK;
        }
    }
    }
 
    HWND parent;
    if (nsnull != aParent) { // has a nsIWidget parent
      parent = ((aParent) ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
    } else { // has a nsNative parent
       parent = (HWND)aNativeParent;
    }

    if (nsnull != aInitData) {
      SetWindowType(aInitData->mWindowType);
      SetBorderStyle(aInitData->mBorderStyle);
    }

    DWORD style = WindowStyle();
    DWORD extendedStyle = WindowExStyle();

    if (mWindowType == eWindowType_popup) {
      mBorderlessParent = parent;
      // Don't set the parent of a popup window. 
      parent = NULL;
    } else if (nsnull != aInitData) {
      // See if the caller wants to explictly set clip children and clip siblings
      if (aInitData->clipChildren) {
        style |= WS_CLIPCHILDREN;
      } else {
        style &= ~WS_CLIPCHILDREN;
      }
      if (aInitData->clipSiblings) {
        style |= WS_CLIPSIBLINGS;
      }
    }

    mHas3DBorder = (extendedStyle & WS_EX_CLIENTEDGE) > 0;

    if (mWindowType == eWindowType_dialog) {
      struct {
        DLGTEMPLATE t;
        short noMenu;
        short defaultClass;
        short title;
      } templ;
      LONG units = GetDialogBaseUnits();

      templ.t.style = style;
      templ.t.dwExtendedStyle = extendedStyle;
      templ.t.cdit = 0;
      templ.t.x = (aRect.x*4)/LOWORD(units);
      templ.t.y = (aRect.y*8)/HIWORD(units);
      templ.t.cx = (aRect.width*4 + LOWORD(units) - 1)/LOWORD(units);
      templ.t.cy = (GetHeight(aRect.height)*8 + HIWORD(units) - 1)/HIWORD(units);
      templ.noMenu = 0;
      templ.defaultClass = 0;
      templ.title = 0;

      mWnd = ::CreateDialogIndirectParam(nsToolkit::mDllInstance,
                                         &templ.t,
                                         parent,
                                         (DLGPROC)DummyDialogProc,
                                         NULL);
    } else {
      mWnd = ::CreateWindowEx(extendedStyle,
                              WindowClass(),
                              "",
                              style,
                              aRect.x,
                              aRect.y,
                              aRect.width,
                              GetHeight(aRect.height),
                              parent,
                              NULL,
                              nsToolkit::mDllInstance,
                              NULL);
    }
   
    VERIFY(mWnd);


   /*mNativeDragTarget = new nsNativeDragTarget(this);
   if (NULL != mNativeDragTarget) {
     mNativeDragTarget->AddRef();
     if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget,TRUE,FALSE)) {
       if (S_OK == ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget)) {
       }
     }
   }*/


    // call the event callback to notify about creation

    DispatchStandardEvent(NS_CREATE);
    SubclassWindow(TRUE);
    return(NS_OK);
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    return(StandardWindowCreate(aParent, aRect, aHandleEventFunction,
                         aContext, aAppShell, aToolkit, aInitData,
                         nsnull));
}


//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::Create(nsNativeWidget aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIAppShell *aAppShell,
                         nsIToolkit *aToolkit,
                         nsWidgetInitData *aInitData)
{
    return(StandardWindowCreate(nsnull, aRect, aHandleEventFunction,
                         aContext, aAppShell, aToolkit, aInitData,
                         aParent));
}


//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Destroy()
{
  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...
  nsToolkit* toolkit = (nsToolkit *)mToolkit;
  if (toolkit != nsnull && !toolkit->IsGuiThread()) {
    MethodInfo info(this, nsWindow::DESTROY);
    toolkit->CallMethod(&info);
    return NS_ERROR_FAILURE;
  }

  // disconnect from the parent
  if (!mIsDestroying) {
    nsBaseWidget::Destroy();
  }

  // just to be safe. If we're going away and for some reason we're still
  // the rollup widget, rollup and turn off capture.
  if ( this == gRollupWidget ) {
    if ( gRollupListener )
      gRollupListener->Rollup();
    CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
  }
  
  EnableDragDrop(PR_FALSE);

  // destroy the HWND
  if (mWnd) {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
    VERIFY(::DestroyWindow(mWnd));
    mWnd = NULL;
    //our windows can be subclassed by
    //others and these namless, faceless others
    //may not let us know about WM_DESTROY. so,
    //if OnDestroy() didn't get called, just call
    //it now. MMP
    if (PR_FALSE == mOnDestroyCalled)
      OnDestroy();
  }

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
    if (mIsTopWidgetWindow) {
       // Must use a flag instead of mWindowType to tell if the window is the 
       // owned by the topmost widget, because a child window can be embedded inside
       // a HWND which is not associated with a nsIWidget.
      return nsnull;
    }
    /* If this widget has already been destroyed, pretend we have no parent.
       This corresponds to code in Destroy which removes the destroyed
       widget from its parent's child list. */
    if (mIsDestroying || mOnDestroyCalled)
      return nsnull;


    nsWindow* widget = nsnull;
    if (mWnd) {
        HWND parent = ::GetParent(mWnd);
        if (parent) {
            widget = GetNSWindowPtr(parent);
            if (widget) {
              // If the widget is in the process of being destroyed then
              // do NOT return it
              if (widget->mIsDestroying) {
                widget = nsnull;
              } else {
                NS_ADDREF(widget);
              }
            }
        }
    }

    return (nsIWidget*)widget;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
  if (mWnd) {
    if (bState) {
      if (!mIsVisible && mWindowType == eWindowType_toplevel) {
        int mode;
        switch (mSizeMode) {
          case nsSizeMode_Maximized :
            mode = SW_SHOWMAXIMIZED;
            break;
          case nsSizeMode_Minimized :
            mode = SW_SHOWMINIMIZED;
            break;
          default :
            mode = SW_SHOWNORMAL;
        }
        ::ShowWindow(mWnd, mode);
      } else {
        DWORD flags = SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW;
        if (mIsVisible)
          flags |= SWP_NOZORDER;

        if (mWindowType == eWindowType_popup) {
          // ensure popups are the topmost of the TOPMOST layer. Remember
          // not to set the SWP_NOZORDER flag as that might allow the taskbar
          // to overlap the popup.
          flags |= SWP_NOACTIVATE;
          ::SetWindowPos(mWnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
        } else {
          ::SetWindowPos(mWnd, HWND_TOP, 0, 0, 0, 0, flags);
        }
      }
    } else
      ::ShowWindow(mWnd, SW_HIDE);
  }
  mIsVisible = bState;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
  bState = mIsVisible;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::PlaceBehind(nsIWidget *aWidget, PRBool aActivate)
{
  HWND behind = aWidget ? (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW) : HWND_TOP;
  UINT flags = SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE;
  if (!aActivate)
    flags |= SWP_NOACTIVATE;

  ::SetWindowPos(mWnd, behind, 0, 0, 0, 0, flags);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Maximize, minimize or restore the window.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode) {

  nsresult rv;

  // save the requested state
  rv = nsBaseWidget::SetSizeMode(aMode);
  if (NS_SUCCEEDED(rv) && mIsVisible) {
    int mode;

    switch (aMode) {
      case nsSizeMode_Maximized :
        mode = SW_MAXIMIZE;
        break;
      case nsSizeMode_Minimized :
        mode = SW_MINIMIZE;
        break;
      default :
        mode = SW_RESTORE;
    }
    ::ShowWindow(mWnd, mode);
  }
  return rv;
}

//-------------------------------------------------------------------------
// Return PR_TRUE in aForWindow if the given event should be processed
// assuming this is a modal window.
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                     PRBool *aForWindow)
{
  if (!aRealEvent) {
    *aForWindow = PR_FALSE;
    return NS_OK;
  }
#if 0
  // this version actually works, but turns out to be unnecessary
  // if we use the OS properly.
  MSG *msg = (MSG *) aEvent;

  switch (msg->message) {
     case WM_MOUSEMOVE:
     case WM_LBUTTONDOWN:
     case WM_LBUTTONUP:
     case WM_LBUTTONDBLCLK:
     case WM_MBUTTONDOWN:
     case WM_MBUTTONUP:
     case WM_MBUTTONDBLCLK:
     case WM_RBUTTONDOWN:
     case WM_RBUTTONUP:
     case WM_RBUTTONDBLCLK: {
         HWND   msgWindow, ourWindow, rollupWindow;
         PRBool acceptEvent;

         // is the event within our window?
         msgWindow = 0;
         rollupWindow = 0;
         ourWindow = msg->hwnd;
         while (ourWindow) {
           msgWindow = ourWindow;
           ourWindow = ::GetParent(ourWindow);
         }
         ourWindow = (HWND)GetNativeData(NS_NATIVE_WINDOW);
         if (gRollupWidget)
           rollupWindow = (HWND)gRollupWidget->GetNativeData(NS_NATIVE_WINDOW);
         acceptEvent = msgWindow && (msgWindow == ourWindow ||
                                     msgWindow == rollupWindow) ?
                       PR_TRUE : PR_FALSE;

         // if not, accept events for any window that hasn't been
         // disabled.
         if (!acceptEvent) {
           LONG proc = ::GetWindowLong(msgWindow, GWL_WNDPROC);
           if (proc == (LONG)&nsWindow::WindowProc) {
             nsWindow *msgWin = GetNSWindowPtr(msgWindow);
             msgWin->IsEnabled(&acceptEvent);
           }
         }
       }
       break;
     default:
       *aForWindow = PR_TRUE;
  }
#else
  *aForWindow = PR_TRUE;
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Constrain a potential move to fit onscreen
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  if (mWnd && mIsTopWidgetWindow) { // only a problem for top-level windows
    // XXX: Needs multiple monitor support
    HDC dc = ::GetDC(mWnd);
    if(dc) {
      if (::GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
        RECT workArea;
        ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        if (*aX < 0)
          *aX = 0;
        if (*aX >= workArea.right - kWindowPositionSlop)
          *aX = workArea.right - kWindowPositionSlop;
        if (*aY < 0)
          *aY = 0;
        if (*aY >= workArea.bottom - kWindowPositionSlop)
          *aY = workArea.bottom - kWindowPositionSlop;
      }
      ::ReleaseDC(mWnd, dc);
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
   // Check to see if window needs to be removed first
   // to avoid a costly call to SetWindowPos. This check
   // can not be moved to the calling code in nsView, because 
   // some platforms do not position child windows correctly

  nsRect currentRect;
  GetBounds(currentRect); 
  {
   if ((currentRect.x == aX) && (currentRect.y == aY))
   {
      // Nothing to do, since it is already positioned correctly.
     return NS_OK;    
   }
  }

   // When moving a borderless top-level window the window
   // must be placed relative to its parent. WIN32 wants to
   // place it relative to the screen, so we used the cached parent
   // to calculate the parent's location then add the x,y passed to
   // the move to get the screen coordinate for the borderless top-level
   // window.
  if (mWindowType == eWindowType_popup) {
    HWND parent = mBorderlessParent;
    if (parent) { 
      RECT pr; 
      VERIFY(::GetWindowRect(parent, &pr)); 
      aX += pr.left; 
      aY += pr.top;   
    } 
  } 

  mBounds.x = aX;
  mBounds.y = aY;

    if (mWnd) {
#ifdef DEBUG
      // complain if a window is moved offscreen (legal, but potentially worrisome)
      if (mIsTopWidgetWindow) { // only a problem for top-level windows
        // Make sure this window is actually on the screen before we move it
        // XXX: Needs multiple monitor support
        HDC dc = ::GetDC(mWnd);
        if(dc) {
          if (::GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
            RECT workArea;
            ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
            // no annoying assertions. just mention the issue.
            if (aX < 0 || aX >= workArea.right || aY < 0 || aY >= workArea.bottom)
              printf("window moved to offscreen position\n");
          }
        ::ReleaseDC(mWnd, dc);
        }
      }
#endif

        nsIWidget *par = GetParent();
        HDWP      deferrer = NULL;

        if (nsnull != par) {
          deferrer = ((nsWindow *)par)->mDeferredPositioner;
        }

        if (NULL != deferrer) {
            VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                                  mWnd, NULL, aX, aY, 0, 0,
                                  SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
        }
        else {
            VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, 0, 0, 
                                  SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
        }

        NS_IF_RELEASE(par);
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_ASSERTION((aWidth >=0 ) , "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((aHeight >=0 ), "Negative height passed to nsWindow::Resize");
  // Set cached value for lightweight and printing
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

    if (mWnd) {
        nsIWidget *par = GetParent();
        HDWP      deferrer = NULL;

        if (nsnull != par) {
          deferrer = ((nsWindow *)par)->mDeferredPositioner;
        }

        UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE;
        if (!aRepaint) {
          flags |= SWP_NOREDRAW;
        }

        if (NULL != deferrer) {
            VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                                    mWnd, NULL, 0, 0, aWidth, GetHeight(aHeight), flags));
        }
        else {
            VERIFY(::SetWindowPos(mWnd, NULL, 0, 0, aWidth, GetHeight(aHeight), 
                                  flags));
        }

        NS_IF_RELEASE(par);
    }
    
    if (aRepaint)
        Invalidate(PR_FALSE);
    
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aX,
                      PRInt32 aY,
                      PRInt32 aWidth,
                      PRInt32 aHeight,
                      PRBool   aRepaint)
{
  NS_ASSERTION((aWidth >=0 ),  "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((aHeight >=0 ), "Negative height passed to nsWindow::Resize");

  // Set cached value for lightweight and printing
  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

    if (mWnd) {
        nsIWidget *par = GetParent();
        HDWP      deferrer = NULL;

        if (nsnull != par) {
          deferrer = ((nsWindow *)par)->mDeferredPositioner;
        }

        UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE;
        if (!aRepaint) {
          flags |= SWP_NOREDRAW;
        }

        if (NULL != deferrer) {
            VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                                    mWnd, NULL, aX, aY, aWidth, GetHeight(aHeight), 
                                    flags));
        }
        else {
            VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, aWidth, GetHeight(aHeight), 
                                  flags));
        }

        NS_IF_RELEASE(par);
    }

    if (aRepaint)
        Invalidate(PR_FALSE);

    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool bState)
{
    if (mWnd) {
        ::EnableWindow(mWnd, bState);
    }
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    nsToolkit* toolkit = (nsToolkit *)mToolkit;
    NS_ASSERTION(toolkit != nsnull, "This should never be null!"); // Bug 57044
    if (toolkit != nsnull && !toolkit->IsGuiThread()) {
        MethodInfo info(this, nsWindow::SET_FOCUS);
        toolkit->CallMethod(&info);
        return NS_ERROR_FAILURE;
    }

    if (mWnd) {
        // Uniconify, if necessary
        HWND toplevelWnd = mWnd;
        while (::GetParent(toplevelWnd))
            toplevelWnd = ::GetParent(toplevelWnd);
        if (::IsIconic(toplevelWnd))
            ::OpenIcon(toplevelWnd);
        ::SetFocus(mWnd);
    }
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetWindowRect(mWnd, &r));

    // assign size
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

    // convert coordinates if parent exists
    HWND parent = ::GetParent(mWnd);
    if (parent) {
      RECT pr;
      VERIFY(::GetWindowRect(parent, &pr));
      r.left -= pr.left;
      r.top  -= pr.top;
    }
    aRect.x = r.left;
    aRect.y = r.top;
  } else {
    aRect = mBounds;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetClientBounds(nsRect &aRect)
{

  if (mWnd) {
    RECT r;
    VERIFY(::GetClientRect(mWnd, &r));

    // assign size
    aRect.x = 0;
    aRect.y = 0;
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

  } else {
    aRect.SetRect(0,0,0,0);
  }
  return NS_OK;
}

//get the bounds, but don't take into account the client size

void nsWindow::GetNonClientBounds(nsRect &aRect)
{
  if (mWnd) {
      RECT r;
      VERIFY(::GetWindowRect(mWnd, &r));

      // assign size
      aRect.width = r.right - r.left;
      aRect.height = r.bottom - r.top;

      // convert coordinates if parent exists
      HWND parent = ::GetParent(mWnd);
      if (parent) {
        RECT pr;
        VERIFY(::GetWindowRect(parent, &pr));
        r.left -= pr.left;
        r.top -= pr.top;
      }
      aRect.x = r.left;
      aRect.y = r.top;
  } else {
      aRect.SetRect(0,0,0,0);
  }
}

           
//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
    nsBaseWidget::SetBackgroundColor(aColor);
  
    if (mBrush)
      ::DeleteObject(mBrush);

    mBrush = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
    if (mWnd != NULL) {
      SetClassLong(mWnd, GCL_HBRBACKGROUND, (LONG)mBrush);
    }
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
    NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
    return NULL;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
  // Cache Font for owner draw
  if (mFont == nsnull) {
    mFont = new nsFont(aFont);
  } else {
    *mFont  = aFont;
  }
  
  // Bail out if there is no context
  if (nsnull == mContext) {
    return NS_ERROR_FAILURE;
  }

  nsIFontMetrics* metrics;
  mContext->GetMetricsFor(aFont, metrics);
  nsFontHandle  fontHandle;
  metrics->GetFontHandle(fontHandle);
  HFONT hfont = (HFONT)fontHandle;

    // Draw in the new font
  ::SendMessage(mWnd, WM_SETFONT, (WPARAM)hfont, (LPARAM)0); 
  NS_RELEASE(metrics);

  return NS_OK;
}

        
//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
 
  // Only change cursor if it's changing

  //XXX mCursor isn't always right.  Scrollbars and others change it, too.
  //XXX If we want this optimization we need a better way to do it.
  //if (aCursor != mCursor) {
    HCURSOR newCursor = NULL;

    switch(aCursor) {
    case eCursor_select:
      newCursor = ::LoadCursor(NULL, IDC_IBEAM);
      break;
      
    case eCursor_wait:
      newCursor = ::LoadCursor(NULL, IDC_WAIT);
      break;

    case eCursor_hyperlink: {
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_SELECTANCHOR));
      break;
    }

    case eCursor_standard:
      newCursor = ::LoadCursor(NULL, IDC_ARROW);
      break;

    case eCursor_sizeWE:
      newCursor = ::LoadCursor(NULL, IDC_SIZEWE);
      break;

    case eCursor_sizeNS:
      newCursor = ::LoadCursor(NULL, IDC_SIZENS);
      break;

    case eCursor_sizeNW:
    case eCursor_sizeSE:
      newCursor = ::LoadCursor(NULL, IDC_SIZENWSE);
      break;

    case eCursor_sizeNE:
    case eCursor_sizeSW:
      newCursor = ::LoadCursor(NULL, IDC_SIZENESW);
      break;

    case eCursor_arrow_north:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWNORTH));
      break;

    case eCursor_arrow_north_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWNORTHPLUS));
      break;

    case eCursor_arrow_south:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWSOUTH));
      break;

    case eCursor_arrow_south_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWSOUTHPLUS));
      break;

    case eCursor_arrow_east:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWEAST));
      break;

    case eCursor_arrow_east_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWEASTPLUS));
      break;

    case eCursor_arrow_west:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWWEST));
      break;

    case eCursor_arrow_west_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWWESTPLUS));
      break;

    case eCursor_crosshair:
      newCursor = ::LoadCursor(NULL, IDC_CROSS);
      break;
               
    case eCursor_move:
      newCursor = ::LoadCursor(NULL, IDC_SIZEALL);
      break;

    case eCursor_help:
      newCursor = ::LoadCursor(NULL, IDC_HELP);
      break;

    case eCursor_copy: // CSS3
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_COPY));
      break;

    case eCursor_alias:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ALIAS));
      break;

    case eCursor_cell:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_CELL));
      break;

    case eCursor_grab:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_GRAB));
      break;

    case eCursor_grabbing:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_GRABBING));
      break;

    case eCursor_spinning:
      newCursor = ::LoadCursor(NULL, IDC_APPSTARTING);
      break;

    case eCursor_context_menu:
    case eCursor_count_up:
    case eCursor_count_down:
    case eCursor_count_up_down:
      break;

    default:
      NS_ASSERTION(0, "Invalid cursor type");
      break;
    }

    if (NULL != newCursor) {
      mCursor = aCursor;
      HCURSOR oldCursor = ::SetCursor(newCursor);
    }
  //}
  return NS_OK;
}
    
// ------------------------------------------------------------------------
//
// Validate a visible area of a widget.
//
// ------------------------------------------------------------------------

NS_METHOD nsWindow::Validate()
{
  if (mWnd)
    VERIFY(::ValidateRect(mWnd, NULL));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
    if (mWnd) 
    {
#ifdef NS_DEBUG
      debug_DumpInvalidate(stdout,
                           this,
                           nsnull,
                           aIsSynchronous,
                           nsCAutoString("noname"),
                           (PRInt32) mWnd);
#endif // NS_DEBUG
      
      VERIFY(::InvalidateRect(mWnd, NULL, TRUE));
      if (aIsSynchronous) {
          VERIFY(::UpdateWindow(mWnd));
      }
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  RECT rect;

  if (mWnd) 
  {
    rect.left   = aRect.x;
    rect.top    = aRect.y;
    rect.right  = aRect.x + aRect.width;
    rect.bottom = aRect.y  + aRect.height;

#ifdef NS_DEBUG
    debug_DumpInvalidate(stdout,
                         this,
                         &aRect,
                         aIsSynchronous,
                         nsCAutoString("noname"),
                         (PRInt32) mWnd);
#endif // NS_DEBUG

    VERIFY(::InvalidateRect(mWnd, &rect, TRUE));
    if (aIsSynchronous) {
      VERIFY(::UpdateWindow(mWnd));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)

{
  nsresult rv = NS_OK;
  if (mWnd) {
    HRGN nativeRegion;
    rv = aRegion->GetNativeRegion((void *&)nativeRegion);
    if (nativeRegion) {
      if (NS_SUCCEEDED(rv)) {
        VERIFY(::InvalidateRgn(mWnd, nativeRegion, TRUE));

        if (aIsSynchronous) {
          VERIFY(::UpdateWindow(mWnd));
        }
      }
    } else {
      rv = NS_ERROR_FAILURE;
    }
  }
  return rv;  
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
  // updates can come through for windows no longer holding an mWnd during
  // deletes triggered by JavaScript in buttons with mouse feedback
  if (mWnd)
    VERIFY(::UpdateWindow(mWnd));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {
        case NS_NATIVE_WIDGET:
        case NS_NATIVE_WINDOW:
        case NS_NATIVE_PLUGIN_PORT:
            return (void*)mWnd;
        case NS_NATIVE_GRAPHIC:
            // XXX:  This is sleezy!!  Remember to Release the DC after using it!
            return (void*)::GetDC(mWnd);
        case NS_NATIVE_COLORMAP:
        default:
            break;
    }

    return NULL;
}

//~~~
void nsWindow::FreeNativeData(void * data, PRUint32 aDataType)
{
  switch(aDataType) 
  {
    case NS_NATIVE_GRAPHIC:
    ::ReleaseDC(mWnd, (HDC)data);
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_COLORMAP:
      break;
    default:
      break;
  }
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
#if 0
    if (mPalette != NULL) {
        ::DeleteObject(mPalette);
    }

    PRUint8 *map = aColorMap->Index;
    LPLOGPALETTE pLogPal = (LPLOGPALETTE) new char[2 * sizeof(WORD) +
                                              aColorMap->NumColors * sizeof(PALETTEENTRY)];
    pLogPal->palVersion = 0x300;
    pLogPal->palNumEntries = aColorMap->NumColors;
    for(int i = 0; i < aColorMap->NumColors; i++) 
    {
    pLogPal->palPalEntry[i].peRed = *map++;
    pLogPal->palPalEntry[i].peGreen = *map++;
    pLogPal->palPalEntry[i].peBlue = *map++;
    pLogPal->palPalEntry[i].peFlags = 0;
    }
    mPalette = ::CreatePalette(pLogPal);
    delete pLogPal;

    NS_ASSERTION(mPalette != NULL, "Null palette");
    if (mPalette != NULL) {
        HDC hDC = ::GetDC(mWnd);
        HPALETTE hOldPalette = ::SelectPalette(hDC, mPalette, TRUE);
        ::RealizePalette(hDC);
        ::SelectPalette(hDC, hOldPalette, TRUE);
        ::ReleaseDC(mWnd, hDC);
    }
#endif
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
//XXX Scroll is obsolete and should go away soon
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  RECT  trect;

  if (nsnull != aClipRect)
  {
    trect.left = aClipRect->x;
    trect.top = aClipRect->y;
    trect.right = aClipRect->XMost();
    trect.bottom = aClipRect->YMost();
  }

  ::ScrollWindowEx(mWnd, aDx, aDy, (nsnull != aClipRect) ? &trect : NULL, NULL,
                   NULL, NULL, SW_INVALIDATE | SW_SCROLLCHILDREN);
  ::UpdateWindow(mWnd);
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
    // Scroll the entire contents of the window + change the offset of any child windows
  ::ScrollWindowEx(mWnd, aDx, aDy, NULL, NULL, NULL, 
     NULL, SW_INVALIDATE | SW_SCROLLCHILDREN);
  ::UpdateWindow(mWnd); // Force synchronous generation of NS_PAINT
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aRect, PRInt32 aDx, PRInt32 aDy)
{
  RECT  trect;

  trect.left = aRect.x;
  trect.top = aRect.y;
  trect.right = aRect.XMost();
  trect.bottom = aRect.YMost();

    // Scroll the bits in the window defined by trect. 
    // Child windows are not scrolled.
  ::ScrollWindowEx(mWnd, aDx, aDy, &trect, NULL, NULL, 
    NULL, SW_INVALIDATE);
  ::UpdateWindow(mWnd); // Force synchronous generation of NS_PAINT
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Every function that needs a thread switch goes through this function
// by calling SendMessage (..WM_CALLMETHOD..) in nsToolkit::CallMethod.
//
//-------------------------------------------------------------------------
BOOL nsWindow::CallMethod(MethodInfo *info)
{
    BOOL bRet = TRUE;

    switch (info->methodId) {
        case nsWindow::CREATE:
            NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
            Create((nsIWidget*)(info->args[0]), 
                        (nsRect&)*(nsRect*)(info->args[1]), 
                        (EVENT_CALLBACK)(info->args[2]), 
                        (nsIDeviceContext*)(info->args[3]),
                        (nsIAppShell *)(info->args[4]),
                        (nsIToolkit*)(info->args[5]),
                        (nsWidgetInitData*)(info->args[6]));
            break;

        case nsWindow::CREATE_NATIVE:
            NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
            Create((nsNativeWidget)(info->args[0]), 
                        (nsRect&)*(nsRect*)(info->args[1]), 
                        (EVENT_CALLBACK)(info->args[2]), 
                        (nsIDeviceContext*)(info->args[3]),
                        (nsIAppShell *)(info->args[4]),
                        (nsIToolkit*)(info->args[5]),
                        (nsWidgetInitData*)(info->args[6]));
            return TRUE;

        case nsWindow::DESTROY:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
            Destroy();
            break;

        case nsWindow::SET_FOCUS:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
            SetFocus(PR_FALSE);
            break;

        default:
            bRet = FALSE;
            break;
    }

    return bRet;
}

//-------------------------------------------------------------------------
void nsWindow::SetUpForPaint(HDC aHDC) 
{
  ::SetBkColor (aHDC, NSRGB_2_COLOREF(mBackground));
  ::SetTextColor(aHDC, NSRGB_2_COLOREF(mForeground));
  ::SetBkMode (aHDC, TRANSPARENT);
}

//---------------------------------------------------------
NS_METHOD nsWindow::EnableDragDrop(PRBool aEnable)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (aEnable) {
    if (nsnull == mNativeDragTarget) {
       mNativeDragTarget = new nsNativeDragTarget(this);
       if (NULL != mNativeDragTarget) {
         mNativeDragTarget->AddRef();
         if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget,TRUE,FALSE)) {
           if (S_OK == ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget)) {
             rv = NS_OK;
           }
         }
       }
    }
  } else {
    if (nsnull != mWnd && NULL != mNativeDragTarget) {
      ::RevokeDragDrop(mWnd);
      if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget, FALSE, TRUE)) {
        rv = NS_OK;
      }
      NS_RELEASE(mNativeDragTarget);
    }
  }

  return rv;
}

//-------------------------------------------------------------------------
UINT nsWindow::MapFromNativeToDOM(UINT aNativeKeyCode)
{
  switch (aNativeKeyCode) {
    case 0xBA: return NS_VK_SEMICOLON;
    case 0xBB: return NS_VK_EQUALS;
    case 0xBD: return NS_VK_SUBTRACT;
  }

  return aNativeKeyCode;
}

//-------------------------------------------------------------------------
//
// OnKey
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchKeyEvent(PRUint32 aEventType, WORD aCharCode, UINT aVirtualCharCode)
{
  nsKeyEvent event;
  nsPoint point;

  point.x = 0;
  point.y = 0;

  InitEvent(event, aEventType, &point); // this add ref's event.widget

  event.charCode = aCharCode;
  event.keyCode  = aVirtualCharCode;

#ifdef KE_DEBUG
  static cnt=0;
  printf("%d DispatchKE Type: %s charCode %d  keyCode %d ", cnt++,  
        (NS_KEY_PRESS == aEventType)?"PRESS":(aEventType == NS_KEY_UP?"Up":"Down"), 
         event.charCode, event.keyCode);
  printf("Shift: %s Control %s Alt: %s \n",  (mIsShiftDown?"D":"U"), (mIsControlDown?"D":"U"), (mIsAltDown?"D":"U"));
  printf("[%c][%c][%c] <==   [%c][%c][%c][ space bar ][%c][%c][%c]\n", 
             IS_VK_DOWN(NS_VK_SHIFT) ? 'S' : ' ',
             IS_VK_DOWN(NS_VK_CONTROL) ? 'C' : ' ',
             IS_VK_DOWN(NS_VK_ALT) ? 'A' : ' ',
             IS_VK_DOWN(VK_LSHIFT) ? 'S' : ' ',
             IS_VK_DOWN(VK_LCONTROL) ? 'C' : ' ',
             IS_VK_DOWN(VK_LMENU) ? 'A' : ' ',
             IS_VK_DOWN(VK_RMENU) ? 'A' : ' ',
             IS_VK_DOWN(VK_RCONTROL) ? 'C' : ' ',
             IS_VK_DOWN(VK_RSHIFT) ? 'S' : ' '

  );
#endif

  event.isShift   = mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isMeta   =  PR_FALSE;
  event.isAlt     = mIsAltDown;
  event.eventStructType = NS_KEY_EVENT;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  return result;
}



//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
#define WM_CHAR_LATER(vk) ( ((vk)<= VK_SPACE) || \
                            (('0'<=(vk))&&((vk)<='9')) || \
                            (('A'<=(vk))&&((vk)<='Z')) || \
                            ((VK_NUMPAD0 <=(vk))&&((vk)<=VK_DIVIDE)) || \
                            (((vk)==NS_VK_BACK_QUOTE)) || \
                            ((NS_VK_OPEN_BRACKET <=(vk))&&((vk)<=NS_VK_QUOTE)) \
                            )
#define NO_WM_CHAR_LATER(vk) (! WM_CHAR_LATER(vk))

BOOL nsWindow::OnKeyDown( UINT aVirtualKeyCode, UINT aScanCode)
{
  WORD asciiKey;

  asciiKey = 0;

  aVirtualKeyCode = !mIMEIsComposing?MapFromNativeToDOM(aVirtualKeyCode):aVirtualKeyCode;

#ifdef DEBUG
  //printf("In OnKeyDown ascii %d  virt: %d  scan: %d\n", asciiKey, aVirtualKeyCode, aScanCode);
#endif

  BOOL result = DispatchKeyEvent(NS_KEY_DOWN, asciiKey, aVirtualKeyCode);

  // XXX: this is a special case hack, should probably use IsSpecialChar and
  //      do the right thing for all SPECIAL_KEY codes
  // "SPECIAL_KEY" keys don't generate a WM_CHAR, so don't generate an NS_KEY_PRESS
  // this is a special case for the delete key
  if (aVirtualKeyCode==VK_DELETE) 
  {
    DispatchKeyEvent(NS_KEY_PRESS, 0, aVirtualKeyCode);
  } 
  else if (mIsControlDown && aVirtualKeyCode == NS_VK_SUBTRACT) {
    DispatchKeyEvent(NS_KEY_PRESS, aVirtualKeyCode-64, 0);
  }
  else if (mIsControlDown && 
           ((( NS_VK_0 <= aVirtualKeyCode) && (aVirtualKeyCode <= NS_VK_9)) ||
            (aVirtualKeyCode == NS_VK_SEMICOLON) ||
            (aVirtualKeyCode == NS_VK_EQUALS)    ||
            (aVirtualKeyCode == NS_VK_COMMA)     ||
            (aVirtualKeyCode == NS_VK_PERIOD)    ||
            (aVirtualKeyCode == NS_VK_SLASH)     
           )
          )
  {
    // put the 0 - 9 in charcode instead of keycode.
    DispatchKeyEvent(NS_KEY_PRESS, aVirtualKeyCode, 0);
  }
  else if (NO_WM_CHAR_LATER(aVirtualKeyCode) &&  
            (aVirtualKeyCode != NS_VK_SEMICOLON)  &&
            (aVirtualKeyCode != NS_VK_EQUALS)  &&
            (aVirtualKeyCode != NS_VK_COMMA)  &&
            (aVirtualKeyCode != NS_VK_PERIOD)  &&
            (aVirtualKeyCode != NS_VK_SLASH))
  {
    DispatchKeyEvent(NS_KEY_PRESS, 0, aVirtualKeyCode);
  } 

  return result;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
BOOL nsWindow::OnKeyUp( UINT aVirtualKeyCode, UINT aScanCode)
{
  aVirtualKeyCode = !mIMEIsComposing?MapFromNativeToDOM(aVirtualKeyCode):aVirtualKeyCode;
  BOOL result = DispatchKeyEvent(NS_KEY_UP, 0, aVirtualKeyCode);
  return result;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
BOOL nsWindow::OnChar( UINT mbcsCharCode, UINT virtualKeyCode, bool isMultiByte )
{
  wchar_t uniChar;
  char    charToConvert[2];
  size_t  length;

  if (mIMEIsComposing)  {
    HandleEndComposition();
  }

  {
    charToConvert[0] = LOBYTE(mbcsCharCode);
    length=1;
  }

  
  if(mIsControlDown && (virtualKeyCode <= 0x1A)) // Ctrl+A Ctrl+Z, see Programming Windows 3.1 page 110 for details  
  { 
    // need to account for shift here.  bug 16486 
    if ( mIsShiftDown ) 
      uniChar = virtualKeyCode - 1 + 'A' ; 
    else 
      uniChar = virtualKeyCode - 1 + 'a' ; 
    virtualKeyCode = 0;
  } 
  else 
  { // 0x20 - SPACE, 0x3D - EQUALS
    if(virtualKeyCode < 0x20 || (virtualKeyCode == 0x3D && mIsControlDown)) 
    {
      uniChar = 0;
    } 
    else 
    {
      ::MultiByteToWideChar(gCurrentKeyboardCP,MB_PRECOMPOSED,charToConvert,length,
	    &uniChar,sizeof(uniChar));
      virtualKeyCode = 0;
      mIsShiftDown = PR_FALSE;
    }
  }
  return DispatchKeyEvent(NS_KEY_PRESS, uniChar, virtualKeyCode);

  //return FALSE;
}


void nsWindow::ConstrainZLevel(HWND *aAfter) {

  nsZLevelEvent  event;
  nsWindow      *aboveWindow = 0;

  event.eventStructType = NS_ZLEVEL_EVENT;
  InitEvent(event, NS_SETZLEVEL);

  if (*aAfter == HWND_BOTTOM)
    event.mPlacement = nsWindowZBottom;
  else if (*aAfter == HWND_TOP || *aAfter == HWND_TOPMOST || *aAfter == HWND_NOTOPMOST)
    event.mPlacement = nsWindowZTop;
  else {
    event.mPlacement = nsWindowZRelative;
    aboveWindow = GetNSWindowPtr(*aAfter);
  }
  event.mReqBelow = aboveWindow;

  event.mImmediate = PR_FALSE;
  event.mAdjusted = PR_FALSE;
  DispatchWindowEvent(&event);

  if (event.mAdjusted) {
    if (event.mPlacement == nsWindowZBottom)
      *aAfter = HWND_BOTTOM;
    else if (event.mPlacement == nsWindowZTop)
      *aAfter = HWND_TOP;
    else {
      *aAfter = (HWND)event.mActualBelow->GetNativeData(NS_NATIVE_WINDOW);
      NS_IF_RELEASE(event.mActualBelow);
    }
  }

  NS_RELEASE(event.widget);
}

//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
static PRBool gJustGotDeactivate = PR_FALSE;
static PRBool gJustGotActivate = PR_FALSE;

#ifdef NS_DEBUG

typedef struct {
  char * mStr;
  long   mId;
} EventMsgInfo;

EventMsgInfo gAllEvents[] = {
    {"WM_NULL           ", 0x0000},
    {"WM_CREATE         ", 0x0001},
    {"WM_DESTROY        ", 0x0002},
    {"WM_MOVE           ", 0x0003},
    {"WM_SIZE           ", 0x0005},
    {"WM_ACTIVATE       ", 0x0006},
    {"WM_SETFOCUS       ", 0x0007},
    {"WM_KILLFOCUS      ", 0x0008},
    {"WM_ENABLE         ", 0x000A},
    {"WM_SETREDRAW      ", 0x000B},
    {"WM_SETTEXT        ", 0x000C},
    {"WM_GETTEXT        ", 0x000D},
    {"WM_GETTEXTLENGTH  ", 0x000E},
    {"WM_PAINT          ", 0x000F},
    {"WM_CLOSE          ", 0x0010},
    {"WM_QUERYENDSESSION", 0x0011},
    {"WM_QUIT           ", 0x0012},
    {"WM_QUERYOPEN      ", 0x0013},
    {"WM_ERASEBKGND     ", 0x0014},
    {"WM_SYSCOLORCHANGE ", 0x0015},
    {"WM_ENDSESSION     ", 0x0016},
    {"WM_SHOWWINDOW     ", 0x0018},
    {"WM_SETTINGCHANGE  ", 0x001A},
    {"WM_DEVMODECHANGE  ", 0x001B},
    {"WM_ACTIVATEAPP    ", 0x001C},
    {"WM_FONTCHANGE     ", 0x001D},
    {"WM_TIMECHANGE     ", 0x001E},
    {"WM_CANCELMODE     ", 0x001F},
    {"WM_SETCURSOR      ", 0x0020},
    {"WM_MOUSEACTIVATE  ", 0x0021},
    {"WM_CHILDACTIVATE  ", 0x0022},
    {"WM_QUEUESYNC      ", 0x0023},
    {"WM_GETMINMAXINFO  ", 0x0024},
    {"WM_PAINTICON      ", 0x0026},
    {"WM_ICONERASEBKGND ", 0x0027},
    {"WM_NEXTDLGCTL     ", 0x0028},
    {"WM_SPOOLERSTATUS  ", 0x002A},
    {"WM_DRAWITEM       ", 0x002B},
    {"WM_MEASUREITEM    ", 0x002C},
    {"WM_DELETEITEM     ", 0x002D},
    {"WM_VKEYTOITEM     ", 0x002E},
    {"WM_CHARTOITEM     ", 0x002F},
    {"WM_SETFONT        ", 0x0030},
    {"WM_GETFONT        ", 0x0031},
    {"WM_SETHOTKEY      ", 0x0032},
    {"WM_GETHOTKEY      ", 0x0033},
    {"WM_QUERYDRAGICON  ", 0x0037},
    {"WM_COMPAREITEM    ", 0x0039},
    {"WM_GETOBJECT      ", 0x003D},
    {"WM_COMPACTING     ", 0x0041},
    {"WM_COMMNOTIFY     ", 0x0044}, 
    {"WM_WINDOWPOSCHANGING", 0x0046},
    {"WM_WINDOWPOSCHANGED ", 0x0047},
    {"WM_POWER          ", 0x0048},
    {"WM_COPYDATA       ", 0x004A},
    {"WM_CANCELJOURNAL  ", 0x004B},
    {"WM_NOTIFY         ", 0x004E},
    {"WM_INPUTLANGCHANGEREQUEST ", 0x0050},
    {"WM_INPUTLANGCHANGE", 0x0051},
    {"WM_TCARD          ", 0x0052},
    {"WM_HELP           ", 0x0053},
    {"WM_USERCHANGED    ", 0x0054},
    {"WM_NOTIFYFORMAT   ", 0x0055},
    {"WM_CONTEXTMENU    ", 0x007B},
    {"WM_STYLECHANGING  ", 0x007C},
    {"WM_STYLECHANGED   ", 0x007D},
    {"WM_DISPLAYCHANGE  ", 0x007E},
    {"WM_GETICON        ", 0x007F},
    {"WM_SETICON        ", 0x0080},
    {"WM_NCCREATE       ", 0x0081},
    {"WM_NCDESTROY      ", 0x0082},
    {"WM_NCCALCSIZE     ", 0x0083},
    {"WM_NCHITTEST      ", 0x0084},
    {"WM_NCPAINT        ", 0x0085},
    {"WM_NCACTIVATE     ", 0x0086},
    {"WM_GETDLGCODE     ", 0x0087},
    {"WM_SYNCPAINT      ", 0x0088},
    {"WM_NCMOUSEMOVE    ", 0x00A0},
    {"WM_NCLBUTTONDOWN  ", 0x00A1},
    {"WM_NCLBUTTONUP    ", 0x00A2},
    {"WM_NCLBUTTONDBLCLK", 0x00A3},
    {"WM_NCRBUTTONDOWN  ", 0x00A4},
    {"WM_NCRBUTTONUP    ", 0x00A5},
    {"WM_NCRBUTTONDBLCLK", 0x00A6},
    {"WM_NCMBUTTONDOWN  ", 0x00A7},
    {"WM_NCMBUTTONUP    ", 0x00A8},
    {"WM_NCMBUTTONDBLCLK", 0x00A9},
    {"EM_GETSEL             ", 0x00B0},
    {"EM_SETSEL             ", 0x00B1},
    {"EM_GETRECT            ", 0x00B2},
    {"EM_SETRECT            ", 0x00B3},
    {"EM_SETRECTNP          ", 0x00B4},
    {"EM_SCROLL             ", 0x00B5},
    {"EM_LINESCROLL         ", 0x00B6},
    {"EM_SCROLLCARET        ", 0x00B7},
    {"EM_GETMODIFY          ", 0x00B8},
    {"EM_SETMODIFY          ", 0x00B9},
    {"EM_GETLINECOUNT       ", 0x00BA},
    {"EM_LINEINDEX          ", 0x00BB},
    {"EM_SETHANDLE          ", 0x00BC},
    {"EM_GETHANDLE          ", 0x00BD},
    {"EM_GETTHUMB           ", 0x00BE},
    {"EM_LINELENGTH         ", 0x00C1},
    {"EM_REPLACESEL         ", 0x00C2},
    {"EM_GETLINE            ", 0x00C4},
    {"EM_LIMITTEXT          ", 0x00C5},
    {"EM_CANUNDO            ", 0x00C6},
    {"EM_UNDO               ", 0x00C7},
    {"EM_FMTLINES           ", 0x00C8},
    {"EM_LINEFROMCHAR       ", 0x00C9},
    {"EM_SETTABSTOPS        ", 0x00CB},
    {"EM_SETPASSWORDCHAR    ", 0x00CC},
    {"EM_EMPTYUNDOBUFFER    ", 0x00CD},
    {"EM_GETFIRSTVISIBLELINE", 0x00CE},
    {"EM_SETREADONLY        ", 0x00CF},
    {"EM_SETWORDBREAKPROC   ", 0x00D0},
    {"EM_GETWORDBREAKPROC   ", 0x00D1},
    {"EM_GETPASSWORDCHAR    ", 0x00D2},
    {"EM_SETMARGINS         ", 0x00D3},
    {"EM_GETMARGINS         ", 0x00D4},
    {"EM_GETLIMITTEXT       ", 0x00D5},
    {"EM_POSFROMCHAR        ", 0x00D6},
    {"EM_CHARFROMPOS        ", 0x00D7},
    {"EM_SETIMESTATUS       ", 0x00D8},
    {"EM_GETIMESTATUS       ", 0x00D9},
    {"SBM_SETPOS        ", 0x00E0},
    {"SBM_GETPOS        ", 0x00E1},
    {"SBM_SETRANGE      ", 0x00E2},
    {"SBM_SETRANGEREDRAW", 0x00E6},
    {"SBM_GETRANGE      ", 0x00E3},
    {"SBM_ENABLE_ARROWS ", 0x00E4},
    {"SBM_SETSCROLLINFO ", 0x00E9},
    {"SBM_GETSCROLLINFO ", 0x00EA},
    {"WM_KEYDOWN        ", 0x0100},
    {"WM_KEYUP          ", 0x0101},
    {"WM_CHAR           ", 0x0102},
    {"WM_DEADCHAR       ", 0x0103},
    {"WM_SYSKEYDOWN     ", 0x0104},
    {"WM_SYSKEYUP       ", 0x0105},
    {"WM_SYSCHAR        ", 0x0106},
    {"WM_SYSDEADCHAR    ", 0x0107},
    {"WM_KEYLAST        ", 0x0108},
    {"WM_IME_STARTCOMPOSITION ", 0x010D},
    {"WM_IME_ENDCOMPOSITION ", 0x010E},
    {"WM_IME_COMPOSITION", 0x010F},
    {"WM_INITDIALOG     ", 0x0110},
    {"WM_COMMAND        ", 0x0111},
    {"WM_SYSCOMMAND     ", 0x0112},
    {"WM_TIMER          ", 0x0113},
    {"WM_HSCROLL        ", 0x0114},
    {"WM_VSCROLL        ", 0x0115},
    {"WM_INITMENU       ", 0x0116},
    {"WM_INITMENUPOPUP  ", 0x0117},
    {"WM_MENUSELECT     ", 0x011F},
    {"WM_MENUCHAR       ", 0x0120},
    {"WM_ENTERIDLE      ", 0x0121},
    {"WM_MENURBUTTONUP  ", 0x0122},
    {"WM_MENUDRAG       ", 0x0123},
    {"WM_MENUGETOBJECT  ", 0x0124},
    {"WM_UNINITMENUPOPUP", 0x0125},
    {"WM_MENUCOMMAND    ", 0x0126},
    {"WM_CTLCOLORMSGBOX ", 0x0132},
    {"WM_CTLCOLOREDIT   ", 0x0133},
    {"WM_CTLCOLORLISTBOX", 0x0134},
    {"WM_CTLCOLORBTN    ", 0x0135},
    {"WM_CTLCOLORDLG    ", 0x0136},
    {"WM_CTLCOLORSCROLLBAR", 0x0137},
    {"WM_CTLCOLORSTATIC ", 0x0138},
    {"CB_GETEDITSEL           ", 0x0140},
    {"CB_LIMITTEXT            ", 0x0141},
    {"CB_SETEDITSEL           ", 0x0142},
    {"CB_ADDSTRING            ", 0x0143},
    {"CB_DELETESTRING         ", 0x0144},
    {"CB_DIR                  ", 0x0145},
    {"CB_GETCOUNT             ", 0x0146},
    {"CB_GETCURSEL            ", 0x0147},
    {"CB_GETLBTEXT            ", 0x0148},
    {"CB_GETLBTEXTLEN         ", 0x0149},
    {"CB_INSERTSTRING         ", 0x014A},
    {"CB_RESETCONTENT         ", 0x014B},
    {"CB_FINDSTRING           ", 0x014C},
    {"CB_SELECTSTRING         ", 0x014D},
    {"CB_SETCURSEL            ", 0x014E},
    {"CB_SHOWDROPDOWN         ", 0x014F},
    {"CB_GETITEMDATA          ", 0x0150},
    {"CB_SETITEMDATA          ", 0x0151},
    {"CB_GETDROPPEDCONTROLRECT", 0x0152},
    {"CB_SETITEMHEIGHT        ", 0x0153},
    {"CB_GETITEMHEIGHT        ", 0x0154},
    {"CB_SETEXTENDEDUI        ", 0x0155},
    {"CB_GETEXTENDEDUI        ", 0x0156},
    {"CB_GETDROPPEDSTATE      ", 0x0157},
    {"CB_FINDSTRINGEXACT      ", 0x0158},
    {"CB_SETLOCALE            ", 0x0159},
    {"CB_GETLOCALE            ", 0x015A},
    {"CB_GETTOPINDEX          ", 0x015b},
    {"CB_SETTOPINDEX          ", 0x015c},
    {"CB_GETHORIZONTALEXTENT  ", 0x015d},
    {"CB_SETHORIZONTALEXTENT  ", 0x015e},
    {"CB_GETDROPPEDWIDTH      ", 0x015f},
    {"CB_SETDROPPEDWIDTH      ", 0x0160},
    {"CB_INITSTORAGE          ", 0x0161},
    {"CB_MSGMAX               ", 0x0162},
    {"LB_ADDSTRING          ", 0x0180},
    {"LB_INSERTSTRING       ", 0x0181},
    {"LB_DELETESTRING       ", 0x0182},
    {"LB_SELITEMRANGEEX     ", 0x0183},
    {"LB_RESETCONTENT       ", 0x0184},
    {"LB_SETSEL             ", 0x0185},
    {"LB_SETCURSEL          ", 0x0186},
    {"LB_GETSEL             ", 0x0187},
    {"LB_GETCURSEL          ", 0x0188},
    {"LB_GETTEXT            ", 0x0189},
    {"LB_GETTEXTLEN         ", 0x018A},
    {"LB_GETCOUNT           ", 0x018B},
    {"LB_SELECTSTRING       ", 0x018C},
    {"LB_DIR                ", 0x018D},
    {"LB_GETTOPINDEX        ", 0x018E},
    {"LB_FINDSTRING         ", 0x018F},
    {"LB_GETSELCOUNT        ", 0x0190},
    {"LB_GETSELITEMS        ", 0x0191},
    {"LB_SETTABSTOPS        ", 0x0192},
    {"LB_GETHORIZONTALEXTENT", 0x0193},
    {"LB_SETHORIZONTALEXTENT", 0x0194},
    {"LB_SETCOLUMNWIDTH     ", 0x0195},
    {"LB_ADDFILE            ", 0x0196},
    {"LB_SETTOPINDEX        ", 0x0197},
    {"LB_GETITEMRECT        ", 0x0198},
    {"LB_GETITEMDATA        ", 0x0199},
    {"LB_SETITEMDATA        ", 0x019A},
    {"LB_SELITEMRANGE       ", 0x019B},
    {"LB_SETANCHORINDEX     ", 0x019C},
    {"LB_GETANCHORINDEX     ", 0x019D},
    {"LB_SETCARETINDEX      ", 0x019E},
    {"LB_GETCARETINDEX      ", 0x019F},
    {"LB_SETITEMHEIGHT      ", 0x01A0},
    {"LB_GETITEMHEIGHT      ", 0x01A1},
    {"LB_FINDSTRINGEXACT    ", 0x01A2},
    {"LB_SETLOCALE          ", 0x01A5},
    {"LB_GETLOCALE          ", 0x01A6},
    {"LB_SETCOUNT           ", 0x01A7},
    {"LB_INITSTORAGE        ", 0x01A8},
    {"LB_ITEMFROMPOINT      ", 0x01A9},
    {"LB_MSGMAX             ", 0x01B0},
    {"WM_MOUSEFIRST     ", 0x0200},
    {"WM_MOUSEMOVE      ", 0x0200},
    {"WM_LBUTTONDOWN    ", 0x0201},
    {"WM_LBUTTONUP      ", 0x0202},
    {"WM_LBUTTONDBLCLK  ", 0x0203},
    {"WM_RBUTTONDOWN    ", 0x0204},
    {"WM_RBUTTONUP      ", 0x0205},
    {"WM_RBUTTONDBLCLK  ", 0x0206},
    {"WM_MBUTTONDOWN    ", 0x0207},
    {"WM_MBUTTONUP      ", 0x0208},
    {"WM_MBUTTONDBLCLK  ", 0x0209},
    {"WM_MOUSEWHEEL     ", 0x020A},
    {"WM_MOUSELAST      ", 0x020A},
    {"WM_MOUSELAST      ", 0x0209},
    {"WM_PARENTNOTIFY     ", 0x0210},
    {"WM_ENTERMENULOOP    ", 0x0211},
    {"WM_EXITMENULOOP     ", 0x0212},
    {"WM_NEXTMENU         ", 0x0213},
    {"WM_SIZING           ", 0x0214},
    {"WM_CAPTURECHANGED   ", 0x0215},
    {"WM_MOVING           ", 0x0216},
    {"WM_POWERBROADCAST   ", 0x0218},
    {"WM_DEVICECHANGE     ", 0x0219},
    {"WM_MDICREATE        ", 0x0220},
    {"WM_MDIDESTROY       ", 0x0221},
    {"WM_MDIACTIVATE      ", 0x0222},
    {"WM_MDIRESTORE       ", 0x0223},
    {"WM_MDINEXT          ", 0x0224},
    {"WM_MDIMAXIMIZE      ", 0x0225},
    {"WM_MDITILE          ", 0x0226},
    {"WM_MDICASCADE       ", 0x0227},
    {"WM_MDIICONARRANGE   ", 0x0228},
    {"WM_MDIGETACTIVE     ", 0x0229},
    {"WM_MDISETMENU       ", 0x0230},
    {"WM_ENTERSIZEMOVE    ", 0x0231},
    {"WM_EXITSIZEMOVE     ", 0x0232},
    {"WM_DROPFILES        ", 0x0233},
    {"WM_MDIREFRESHMENU   ", 0x0234},
    {"WM_IME_SETCONTEXT   ", 0x0281},
    {"WM_IME_NOTIFY       ", 0x0282},
    {"WM_IME_CONTROL      ", 0x0283},
    {"WM_IME_COMPOSITIONFULL", 0x0284},
    {"WM_IME_SELECT       ", 0x0285},
    {"WM_IME_CHAR         ", 0x0286},
    {"WM_IME_REQUEST      ", 0x0288},
    {"WM_IME_KEYDOWN      ", 0x0290},
    {"WM_IME_KEYUP        ", 0x0291},
    {"WM_MOUSEHOVER       ", 0x02A1},
    {"WM_MOUSELEAVE       ", 0x02A3},
    {"WM_CUT              ", 0x0300},
    {"WM_COPY             ", 0x0301},
    {"WM_PASTE            ", 0x0302},
    {"WM_CLEAR            ", 0x0303},
    {"WM_UNDO             ", 0x0304},
    {"WM_RENDERFORMAT     ", 0x0305},
    {"WM_RENDERALLFORMATS ", 0x0306},
    {"WM_DESTROYCLIPBOARD ", 0x0307},
    {"WM_DRAWCLIPBOARD    ", 0x0308},
    {"WM_PAINTCLIPBOARD   ", 0x0309},
    {"WM_VSCROLLCLIPBOARD ", 0x030A},
    {"WM_SIZECLIPBOARD    ", 0x030B},
    {"WM_ASKCBFORMATNAME  ", 0x030C},
    {"WM_CHANGECBCHAIN    ", 0x030D},
    {"WM_HSCROLLCLIPBOARD ", 0x030E},
    {"WM_QUERYNEWPALETTE  ", 0x030F},
    {"WM_PALETTEISCHANGING", 0x0310},
    {"WM_PALETTECHANGED   ", 0x0311},
    {"WM_HOTKEY           ", 0x0312},
    {"WM_PRINT            ", 0x0317},
    {"WM_PRINTCLIENT      ", 0x0318},
    {"WM_HANDHELDFIRST    ", 0x0358},
    {"WM_HANDHELDLAST     ", 0x035F},
    {"WM_AFXFIRST         ", 0x0360},
    {"WM_AFXLAST          ", 0x037F},
    {"WM_PENWINFIRST      ", 0x0380},
    {"WM_PENWINLAST       ", 0x038F},
    {"WM_APP              ", 0x8000},
    {NULL, 0x0}
    };


static long gEventCounter = 0;
static long gLastEventMsg = 0;

void PrintEvent(UINT msg, PRBool aShowAllEvents, PRBool aShowMouseMoves)
{
  int inx = 0;
  while (gAllEvents[inx].mId != (long)msg && gAllEvents[inx].mStr != NULL) {
    inx++;
  }
  if (aShowAllEvents || (!aShowAllEvents && gLastEventMsg != (long)msg)) {
    if (aShowMouseMoves || (!aShowMouseMoves && msg != 0x0020 && msg != 0x0200 && msg != 0x0084)) {
      printf("%6d - 0x%04X %s\n", gEventCounter++, msg, gAllEvents[inx].mStr?gAllEvents[inx].mStr:"Unknown");
      gLastEventMsg = msg;
    }
  }
}

#endif


PRBool nsWindow::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue)
{
    static UINT vkKeyCached = 0;              // caches VK code fon WM_KEYDOWN
    PRBool        result = PR_FALSE; // call the default nsWindow proc
    static PRBool getWheelInfo = PR_TRUE;
    *aRetValue = 0;
    PRBool isMozWindowTakingFocus = PR_TRUE;

    // Uncomment this to see all windows messages
    // first param showss all events 
    // second param indicates whether to show mouse move events
    //PrintEvent(msg, PR_FALSE, PR_FALSE);

    switch (msg) {
        case WM_COMMAND: {
          WORD wNotifyCode = HIWORD(wParam); // notification code 
          if ((CBN_SELENDOK == wNotifyCode) || (CBN_SELENDCANCEL == wNotifyCode)) { // Combo box change
            nsGUIEvent event;
            event.eventStructType = NS_GUI_EVENT;
            nsPoint point(0,0);
            InitEvent(event, NS_CONTROL_CHANGE, &point); // this add ref's event.widget
            result = DispatchWindowEvent(&event);
            NS_RELEASE(event.widget);
          } else if (wNotifyCode == 0) { // Menu selection
            nsMenuEvent event;
            event.mCommand = LOWORD(wParam);
            event.eventStructType = NS_MENU_EVENT;
            InitEvent(event, NS_MENU_SELECTED);
            result = DispatchWindowEvent(&event);
            NS_RELEASE(event.widget);
          }
        }
        break;

        case WM_DISPLAYCHANGE:
          DispatchStandardEvent(NS_DISPLAYCHANGED);
        break;

        case WM_ENTERIDLE:
          {
          nsresult rv;
          nsCOMPtr<nsITimerQueue> queue = 
                   do_GetService(kTimerManagerCID, &rv);
          if (!NS_FAILED(rv)) {

            if (queue->HasReadyTimers(NS_PRIORITY_LOWEST)) {

              MSG wmsg;
              do {
#ifdef DEBUG
                //printf("fire\n");
#endif
                queue->FireNextReadyTimer(NS_PRIORITY_LOWEST);
              } while (queue->HasReadyTimers(NS_PRIORITY_LOWEST) && 
                  !::PeekMessage(&wmsg, NULL, 0, 0, PM_NOREMOVE));
            }
          }
          }
          break;
        
        case WM_NOTIFY:
            // TAB change
          {
            LPNMHDR pnmh = (LPNMHDR) lParam;

            switch (pnmh->code) {
              case TCN_SELCHANGE: {
                DispatchStandardEvent(NS_TABCHANGE);
                result = PR_TRUE;
              }
              break;

            }
          }
          break;

        case WM_FONTCHANGE: 
          {
            nsresult rv;
            PRBool didChange = PR_FALSE;

            // update the global font list
            nsCOMPtr<nsIFontEnumerator> fontEnum = do_GetService("@mozilla.org/gfx/fontenumerator;1", &rv);
            if (NS_SUCCEEDED(rv)) {
              fontEnum->UpdateFontList(&didChange);
              //didChange is TRUE only if new font langGroup is added to the list.
              if (didChange)  {
                // update device context font cache
                // Dirty but easiest way: 
                // Changing nsIPref entry which triggers callbacks
                // and flows into calling mDeviceContext->FlushFontCache()
                // to update the font cache in all the instance of Browsers
                nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv); 
                if (NS_SUCCEEDED(rv)) { 
                  PRBool fontInternalChange = PR_FALSE;  
                  pPrefs->GetBoolPref("font.internaluseonly.changed", &fontInternalChange);
                  pPrefs->SetBoolPref("font.internaluseonly.changed", !fontInternalChange);
                }
              }
            } //if (NS_SUCCEEDED(rv))
          }
          break;

        case WM_MOVE: // Window moved 
          {
            PRInt32 x = (PRInt32)LOWORD(lParam); // horizontal position in screen coordinates 
            PRInt32 y = (PRInt32)HIWORD(lParam); // vertical position in screen coordinates 
            result = OnMove(x, y); 
          }
          break;

        case WM_CLOSE: // close request
          DispatchStandardEvent(NS_XUL_CLOSE);
          result = PR_TRUE; // abort window closure
          break;

        case WM_DESTROY:
            // clean up.
            OnDestroy();
            result = PR_TRUE;
            break;

        case WM_PAINT:
            result = OnPaint();
            break;
	case WM_SYSCHAR:
	case WM_CHAR: 
        {
#ifdef KE_DEBUG
            printf("%s\tchar=%c\twp=%4x\tlp=%8x\n", (msg == WM_SYSCHAR) ? "WM_SYSCHAR" : "WM_CHAR" , wParam, wParam, lParam);
#endif

            mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
            if(WM_SYSCHAR==msg)
            {
                mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
                mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
            } else { // WM_KEYUP
                // If the Context Code bit is down and we got a WM_KEY
                // it is a key press for character, not accelerator
                // see p246 of Programming Windows 95 [Charles Petzold] for details
                mIsControlDown = (0 == (KF_ALTDOWN & HIWORD(lParam)))&& IS_VK_DOWN(NS_VK_CONTROL);
                mIsAltDown     = (0 == (KF_ALTDOWN & HIWORD(lParam)))&& IS_VK_DOWN(NS_VK_ALT);
            }

            // ignore [shift+]alt+space so the OS can handle it 
            if (mIsAltDown && !mIsControlDown && IS_VK_DOWN(NS_VK_SPACE)) {
                result = PR_FALSE;
                break;
            }

            unsigned char    ch = (unsigned char)wParam;
            UINT            char_result;
  
            //
            // check first for backspace or return, handle them specially 
            //
            if ((wParam <= 0xff) && (ch==0x0d || ch==0x08)) {

                result = OnChar(ch,ch==0x0d ? VK_RETURN : VK_BACK,true);
                break;
            }
  
            {
                char_result = ch;
                result = OnChar(char_result,ch,false);
            }
  
            break;
        }
        case WM_SYSKEYUP:
        case WM_KEYUP: 
#ifdef KE_DEBUG
            printf("%s\t\twp=%x\tlp=%x\n",  
                   (WM_KEYUP==msg)?"WM_KEYUP":"WM_SYSKEYUP" , wParam, lParam);
#endif
            mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
            if(WM_SYSKEYUP==msg)
            {
                mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
                mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
            } else { // WM_KEYUP
                // If the Context Code bit is down and we got a WM_KEY
                // it is a key press for character, not accelerator
                // see p246 of Programming Windows 95 [Charles Petzold] for details
                mIsControlDown = (0 == (KF_ALTDOWN & HIWORD(lParam)))&& IS_VK_DOWN(NS_VK_CONTROL);
                mIsAltDown     = (0 == (KF_ALTDOWN & HIWORD(lParam)))&& IS_VK_DOWN(NS_VK_ALT);
            }

            // Note- The origional code pass (HIWORD(lParam)) to OnKeyUp as 
            // scan code. Howerver, this break Alt+Num pad input.
            // http://msdn.microsoft.com/library/psdk/winui/keybinpt_8qp5.htm
            // state the following-
            //  Typically, ToAscii performs the translation based on the 
            //  virtual-key code. In some cases, however, bit 15 of the
            //  uScanCode parameter may be used to distinguish between a key 
            //  press and a key release. The scan code is used for
            //  translating ALT+number key combinations.

            if (!mIMEIsComposing)
              result = OnKeyUp(wParam, (HIWORD(lParam) ));
			      else
				      result = PR_FALSE;
            break;

        // Let ths fall through if it isn't a key pad
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
#ifdef KE_DEBUG
            printf("%s\t\twp=%4x\tlp=%8x\n",  
                   (WM_KEYDOWN==msg)?"WM_KEYDOWN":"WM_SYSKEYDOWN" , wParam, lParam);
#endif
            mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
            if(WM_SYSKEYDOWN==msg)
            {
                mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
                mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
            } else { // WM_KEYUP
                // If the Context Code bit is down and we got a WM_KEY
                // it is a key press for character, not accelerator
                // see p246 of Programming Windows 95 [Charles Petzold] for details
                mIsControlDown = (0 == (KF_ALTDOWN & HIWORD(lParam)))&& IS_VK_DOWN(NS_VK_CONTROL);
                mIsAltDown     = (0 == (KF_ALTDOWN & HIWORD(lParam)))&& IS_VK_DOWN(NS_VK_ALT);
            }
            // Note- The origional code pass (HIWORD(lParam)) to OnKeyDown as 
            // scan code. Howerver, this break Alt+Num pad input.
            // http://msdn.microsoft.com/library/psdk/winui/keybinpt_8qp5.htm
            // state the following-
            //  Typically, ToAscii performs the translation based on the 
            //  virtual-key code. In some cases, however, bit 15 of the
            //  uScanCode parameter may be used to distinguish between a key 
            //  press and a key release. The scan code is used for
            //  translating ALT+number key combinations.

            if (mIsAltDown && mIMEIsStatusChanged) {
               mIMEIsStatusChanged = FALSE;
	             result = PR_FALSE;
            }
            else if (!mIMEIsComposing)
               result = OnKeyDown(wParam, (HIWORD(lParam)));
	          else
	             result = PR_FALSE;
            }

            if (wParam == VK_MENU || (wParam == VK_F10 && !mIsShiftDown)) {
              // This is required to prevent Windows
              // default menu processing getting in the
              // way of XP menus and key handling.
              // Without this we call DefWindowProc which will 
              // send us WM_COMMAND and/or WM_SYSCOMMAND messages.
              // Do not remove!  Talk to me if you have
              // questions. - hyatt@netscape.com
              result = PR_TRUE;
              *aRetValue = 0;           
            }
            break;

        // say we've dealt with erase background if widget does
        // not need auto-erasing
        case WM_ERASEBKGND: 
            if (! AutoErase()) {
              *aRetValue = 1;
              result = PR_TRUE;
            } 
            break;

        case WM_GETDLGCODE:
            *aRetValue = DLGC_WANTALLKEYS;
            result = PR_TRUE;
            break;

        case WM_MOUSEMOVE:
            //RelayMouseEvent(msg,wParam, lParam); 
            result = DispatchMouseEvent(NS_MOUSE_MOVE);
            break;

        case WM_LBUTTONDOWN:
            //SetFocus(); // this is bad
            //RelayMouseEvent(msg,wParam, lParam); 
            {
#if IME_MOUSE_EVENT_SUPPORT
            // check whether IME window do mouse operation
            if (mIMEIsComposing && nsWindow::uWM_MSIME_MOUSE) {
							POINT ptPos;
							ptPos.x = (short)LOWORD(lParam);
							ptPos.y = (short)HIWORD(lParam);
							if (IMECompositionHitTest(NS_MOUSE_LEFT_BUTTON_DOWN, &ptPos))
								if (HandleMouseActionOfIME(IMEMOUSE_LDOWN))
									break;
            }
#endif
            result = DispatchMouseEvent(NS_MOUSE_LEFT_BUTTON_DOWN);
            } break;

        case WM_LBUTTONUP:
            //RelayMouseEvent(msg,wParam, lParam); 
            result = DispatchMouseEvent(NS_MOUSE_LEFT_BUTTON_UP);
            break;

        case WM_CONTEXTMENU:
        {
            // if the context menu is brought up from the keyboard, |lParam|
            // will be maxlong. Send a different event msg instead.
            PRUint32 msg = (lParam == 0xFFFFFFFF) ? NS_CONTEXTMENU_KEY : NS_CONTEXTMENU;
            result = DispatchMouseEvent(msg);
        }
            break;
            
        case WM_LBUTTONDBLCLK:
            result = DispatchMouseEvent(NS_MOUSE_LEFT_DOUBLECLICK);
            break;

        case WM_MBUTTONDOWN:
            { 
#if IME_MOUSE_EVENT_SUPPORT
            // check whether IME window do mouse operation
            if (mIMEIsComposing && nsWindow::uWM_MSIME_MOUSE) {
							POINT ptPos;
							ptPos.x = (short)LOWORD(lParam);
							ptPos.y = (short)HIWORD(lParam);
							if (IMECompositionHitTest(NS_MOUSE_MIDDLE_BUTTON_DOWN, &ptPos))
	              if (HandleMouseActionOfIME(IMEMOUSE_MDOWN))
		              break;
            }
#endif
            result = DispatchMouseEvent(NS_MOUSE_MIDDLE_BUTTON_DOWN);
            } break;

        case WM_MBUTTONUP:
            result = DispatchMouseEvent(NS_MOUSE_MIDDLE_BUTTON_UP);
            break;

        case WM_MBUTTONDBLCLK:
            result = DispatchMouseEvent(NS_MOUSE_MIDDLE_BUTTON_DOWN);           
            break;

        case WM_RBUTTONDOWN:
            {
#if IME_MOUSE_EVENT_SUPPORT
            // check whether IME window do mouse operation
            if (mIMEIsComposing && nsWindow::uWM_MSIME_MOUSE) {
							POINT ptPos;
							ptPos.x = (short)LOWORD(lParam);
							ptPos.y = (short)HIWORD(lParam);
							if (IMECompositionHitTest(NS_MOUSE_RIGHT_BUTTON_DOWN, &ptPos))
	              if (HandleMouseActionOfIME(IMEMOUSE_RDOWN))
                break;
            }
#endif
            result = DispatchMouseEvent(NS_MOUSE_RIGHT_BUTTON_DOWN);            
            } break;

        case WM_RBUTTONUP:
            result = DispatchMouseEvent(NS_MOUSE_RIGHT_BUTTON_UP);
            break;

        case WM_RBUTTONDBLCLK:
            result = DispatchMouseEvent(NS_MOUSE_RIGHT_DOUBLECLICK);                      
            break;

        case WM_HSCROLL:
        case WM_VSCROLL: 
	          // check for the incoming nsWindow handle to be null in which case
	          // we assume the message is coming from a horizontal scrollbar inside
	          // a listbox and we don't bother processing it (well, we don't have to)
	          if (lParam) {
                nsWindow* scrollbar = GetNSWindowPtr((HWND)lParam);

		            if (scrollbar) {
		                result = scrollbar->OnScroll(LOWORD(wParam), (short)HIWORD(wParam));
		            }
	          }
            break;

        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:
        //case WM_CTLCOLORSCROLLBAR: //XXX causes a the scrollbar to be drawn incorrectly
        case WM_CTLCOLORSTATIC:
	          if (lParam) {
              nsWindow* control = GetNSWindowPtr((HWND)lParam);
		          if (control) {
                control->SetUpForPaint((HDC)wParam);
		            *aRetValue = (LPARAM)control->OnControlColor();
              }
	          }
    
            result = PR_TRUE;
            break;

        case WM_ACTIVATE:
          if (mEventCallback) {
            PRInt32 fActive = LOWORD(wParam);

            if(WA_INACTIVE == fActive) {
              gJustGotDeactivate = PR_TRUE;
            } else {
              gJustGotActivate = PR_TRUE;
              nsMouseEvent event;
              event.eventStructType = NS_GUI_EVENT;
              InitEvent(event, NS_MOUSE_ACTIVATE);

              event.acceptActivation = PR_TRUE;

              PRBool result = DispatchWindowEvent(&event);
              NS_RELEASE(event.widget);

              if(event.acceptActivation)
                *aRetValue = MA_ACTIVATE;
              else
                *aRetValue = MA_NOACTIVATE; 
            }				
          }
          break;

        case WM_MOUSEACTIVATE:
        {
          // This seems to be the only way we're
          // notified when a child window that doesn't have this handler proc
          // (read as: windows created by plugins like Adobe Acrobat)
          // has been activated via clicking.
          DispatchFocus(NS_PLUGIN_ACTIVATE, isMozWindowTakingFocus);
            break;
        }

        case WM_WINDOWPOSCHANGING: {
          LPWINDOWPOS info = (LPWINDOWPOS) lParam;
          if (!(info->flags & SWP_NOZORDER))
            ConstrainZLevel(&info->hwndInsertAfter);
          break;
        }

      case WM_SETFOCUS:
        result = DispatchFocus(NS_GOTFOCUS, isMozWindowTakingFocus);
        if(gJustGotActivate) {
          gJustGotActivate = PR_FALSE;
          result = DispatchFocus(NS_ACTIVATE, isMozWindowTakingFocus);
        }
#ifdef ACCESSIBILITY
        if (nsWindow::gIsAccessibilityOn && !mRootAccessible && mIsTopWidgetWindow) 
          CreateRootAccessible();
#endif

        break;

      case WM_KILLFOCUS:
        char className[19];
        ::GetClassName((HWND)wParam, className, 19);
        if(strcmp(className, WindowClass()))
          isMozWindowTakingFocus = PR_FALSE;

        if(gJustGotDeactivate) {
          gJustGotDeactivate = PR_FALSE;
          result = DispatchFocus(NS_DEACTIVATE, isMozWindowTakingFocus);
        } 
        result = DispatchFocus(NS_LOSTFOCUS, isMozWindowTakingFocus);
        break;

      case WM_WINDOWPOSCHANGED: 
        {
            WINDOWPOS *wp = (LPWINDOWPOS)lParam;

            // We only care about a resize, so filter out things like z-order
            // changes. Note: there's a WM_MOVE handler above which is why we're
            // not handling them here...
            if (0 == (wp->flags & SWP_NOSIZE)) {
              // XXX Why are we using the client size area? If the size notification
              // is for the client area then the origin should be (0,0) and not
              // the window origin in screen coordinates...
              RECT r;
              ::GetWindowRect(mWnd, &r);
              PRInt32 newWidth, newHeight;
              newWidth = PRInt32(r.right - r.left);
              newHeight = PRInt32(r.bottom - r.top);
              nsRect rect(wp->x, wp->y, newWidth, newHeight);
              if (newWidth != mLastSize.width)
              {
                RECT drect;

                //getting wider

                drect.left = wp->x + mLastSize.width;
                drect.top = wp->y; 
                drect.right = drect.left + (newWidth - mLastSize.width);
                drect.bottom = drect.top + newHeight;

                ::RedrawWindow(mWnd, &drect, NULL,
                               RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT | RDW_ERASENOW | RDW_ALLCHILDREN);
              }
              if (newHeight != mLastSize.height)
              {
                RECT drect;

                //getting taller

                drect.left = wp->x;
                drect.top = wp->y + mLastSize.height;
                drect.right = drect.left + newWidth;
                drect.bottom = drect.top + (newHeight - mLastSize.height);

                ::RedrawWindow(mWnd, &drect, NULL,
                               RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT | RDW_ERASENOW | RDW_ALLCHILDREN);
              }
              mBounds.width  = newWidth;
              mBounds.height = newHeight;
              mLastSize.width = newWidth;
              mLastSize.height = newHeight;
              ///nsRect rect(wp->x, wp->y, wp->cx, wp->cy);

              // If we're being minimized, don't send the resize event to Gecko because
              // it will cause the scrollbar in the content area to go away and we'll
              // forget the scroll position of the page.
              if ( !newWidth && !newHeight ) {
                result = PR_FALSE;
                break;
              }

              // recalculate the width and height
              // this time based on the client area
              if (::GetClientRect(mWnd, &r)) {
                rect.width  = PRInt32(r.right - r.left);
                rect.height = PRInt32(r.bottom - r.top);
              }
              result = OnResize(rect);
            }

            /* handle size mode changes
               (the framechanged message seems a handy place to hook in,
               because it happens early enough (WM_SIZE is too late) and
               because in testing it seems an accurate harbinger of
               an impending min/max/restore change (WM_NCCALCSIZE would
               also work, but it's also sent when merely resizing.)) */
            if (wp->flags & SWP_FRAMECHANGED) {
              WINDOWPLACEMENT pl;
              pl.length = sizeof(pl);
              ::GetWindowPlacement(mWnd, &pl);

              nsSizeModeEvent event;
              event.eventStructType = NS_SIZEMODE_EVENT;
              if (pl.showCmd == SW_SHOWMAXIMIZED)
                event.mSizeMode = nsSizeMode_Maximized;
              else if (pl.showCmd == SW_SHOWMINIMIZED)
                event.mSizeMode = nsSizeMode_Minimized;
              else
                event.mSizeMode = nsSizeMode_Normal;
              InitEvent(event, NS_SIZEMODE);

              result = DispatchWindowEvent(&event);
              NS_RELEASE(event.widget);
            }
            break;
        }

        case WM_SETTINGCHANGE:
          getWheelInfo = PR_TRUE;
        break;

        case WM_PALETTECHANGED:
            if ((HWND)wParam == mWnd) {
                // We caused the WM_PALETTECHANGED message so avoid realizing
                // another foreground palette
                result = PR_TRUE;
                break;
            }
            // fall thru...

        case WM_QUERYNEWPALETTE:      // this window is about to become active
#if 0
            mContext->GetPaletteInfo(palInfo);
            if (palInfo.isPaletteDevice && palInfo.palette) {
                HDC hDC = ::GetDC(mWnd);
                HPALETTE hOldPal = ::SelectPalette(hDC, (HPALETTE)palInfo.palette, FALSE);
                
                // Realize the drawing palette
                int i = ::RealizePalette(hDC);

#ifdef DEBUG
                //printf("number of colors that changed=%d\n",i);
#endif
                // we should always invalidate.. because the lookup may have changed
                ::InvalidateRect(mWnd, (LPRECT)NULL, TRUE);

                ::ReleaseDC(mWnd, hDC);
                *aRetValue = TRUE;
            }
            result = PR_TRUE;
#endif
            break;

				case WM_INPUTLANGCHANGEREQUEST:
					*aRetValue = TRUE;
					result = PR_FALSE;
					break;

				case WM_INPUTLANGCHANGE: 
					result = OnInputLangChange((HKL)lParam, 
								aRetValue);
					break;

				case WM_IME_STARTCOMPOSITION: 
					result = OnIMEStartComposition();
					break;

				case WM_IME_COMPOSITION: 
					result = OnIMEComposition(lParam);
					break;

				case WM_IME_ENDCOMPOSITION: 
					result = OnIMEEndComposition();
					break;

				case WM_IME_CHAR: 
					result = OnIMEChar((BYTE)(wParam>>8), 
						(BYTE) (wParam & 0x00FF), 
						lParam);
					break;

				case WM_IME_NOTIFY: 
					result = OnIMENotify(wParam, lParam, aRetValue);
					break;

				// This is a Window 98/2000 only message
				case WM_IME_REQUEST: 
					result = OnIMERequest(wParam, lParam, aRetValue);
					break;

				case WM_IME_SELECT: 
					result = OnIMESelect(wParam, (WORD)(lParam & 0x0FFFF));
					break;

				case WM_IME_SETCONTEXT: 
					result = OnIMESetContext(wParam, lParam);
					break;

        case WM_DROPFILES: {
#if 0
	        HDROP hDropInfo = (HDROP) wParam;
	        UINT nFiles = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);

	        for (UINT iFile = 0; iFile < nFiles; iFile++) {
		        TCHAR szFileName[_MAX_PATH];
		        ::DragQueryFile(hDropInfo, iFile, szFileName, _MAX_PATH);
#ifdef DEBUG
            printf("szFileName [%s]\n", szFileName);
#endif
            nsAutoString fileStr(szFileName);
            nsEventStatus status;
            nsDragDropEvent event;
            InitEvent(event, NS_DRAGDROP_EVENT);
            event.mType      = nsDragDropEventStatus_eDrop;
            event.mIsFileURL = PR_FALSE;
            event.mURL       = (PRUnichar *)fileStr.get();
            DispatchEvent(&event, status);
	        }
#endif
        } break;

      case WM_DESTROYCLIPBOARD: {
        nsIClipboard* clipboard;
        nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                                   NS_GET_IID(nsIClipboard),
                                                   (nsISupports **)&clipboard);
        clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);
        nsServiceManager::ReleaseService(kCClipboardCID, clipboard);
      } break;

#ifdef ACCESSIBILITY
      case WM_GETOBJECT: 
      {
        nsWindow::gIsAccessibilityOn = TRUE;
        LRESULT lAcc = 0;
        if (mIsTopWidgetWindow && !mRootAccessible) 
          CreateRootAccessible();
        if (lParam == OBJID_CLIENT && mRootAccessible)   // oleacc.dll will be loaded dynamically
          lAcc = Accessible::LresultFromObject(IID_IAccessible, wParam, mRootAccessible); // ref 1

        return (*aRetValue = lAcc) != 0;

      } 
#endif
      default: {
        // Handle both flavors of mouse wheel events.
        if ((msg == WM_MOUSEWHEEL) || (msg == uMSH_MOUSEWHEEL)) {
          static int iDeltaPerLine;
          static ULONG ulScrollLines;
          
          // Get mouse wheel metrics (but only once).
          if (getWheelInfo) {
            getWheelInfo = PR_FALSE;
            
            // This needs to be done differently for Win95 than Win98/NT
            // Taken from sample code in MS Intellimouse SDK
            // http://www.microsoft.com/Mouse/intellimouse/sdk/sdkmessaging.htm
            
            OSVERSIONINFO osversion;
            memset(&osversion, 0, sizeof(OSVERSIONINFO));
            osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            GetVersionEx(&osversion);
            
            if ((osversion.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) &&
                (osversion.dwMajorVersion == 4) &&
                (osversion.dwMinorVersion == 0))
              {
                // This is the Windows 95 case
                HWND hdlMsWheel = FindWindow(MSH_WHEELMODULE_CLASS,
                                             MSH_WHEELMODULE_TITLE);
                if (hdlMsWheel) {
                  UINT uiMsh_MsgScrollLines = RegisterWindowMessage(MSH_SCROLL_LINES);
                  if (uiMsh_MsgScrollLines) {
                    ulScrollLines = (int) SendMessage(hdlMsWheel,
                                                      uiMsh_MsgScrollLines, 0,
                                                      0);
                  }
                }
              }
            else if (osversion.dwMajorVersion >= 4) {
              // This is the Win98/NT4/Win2K case
              SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0,
                                    &ulScrollLines, 0);
            }
            
            // ulScrollLines usually equals 3 or 0 (for no scrolling)
            // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40.

            // However, if ulScrollLines > WHEEL_DELTA, we assume that
            // the mouse driver wants a page scroll.  The docs state that
            // ulScrollLines should explicitly equal WHEEL_PAGESCROLL, but
            // since some mouse drivers use an arbitrary large number instead,
            // we have to handle that as well.

            iDeltaPerLine = 0;
            if (ulScrollLines) {
              if (ulScrollLines <= WHEEL_DELTA) {
                iDeltaPerLine = WHEEL_DELTA / ulScrollLines;
              } else {
                ulScrollLines = WHEEL_PAGESCROLL;
              }
            }
          }
          
          if ((ulScrollLines != WHEEL_PAGESCROLL) && (!iDeltaPerLine))
            return 0;
          
          // The mousewheel event will be dispatched to the toplevel
          // window.  We need to give it to the child window
          
          POINT point;
          point.x = (short) LOWORD(lParam);
          point.y = (short) HIWORD(lParam);
          HWND destWnd = ::WindowFromPoint(point);
          
          // Since we receive mousewheel events for as long as
          // we are focused, it's entirely possible that there
          // is another app's window or no window under the
          // pointer.
          
          if (!destWnd) {
            // No window is under the pointer
            break;
          }
          
          LONG proc = ::GetWindowLong(destWnd, GWL_WNDPROC);
          if (proc != (LONG)&nsWindow::WindowProc)  {
            // Some other app, or a plugin window.
            // Windows directs WM_MOUSEWHEEL to the focused window.
            // However, Mozilla does not like plugins having focus, so a 
            // Mozilla window (ie, the plugin's parent (us!) has focus.)
            // Therefore, plugins etc _should_ get first grab at the 
            // message, but this focus vaguary means the plugin misses 
            // out. If the window is a child of ours, forward it on.
            // Determine if a child by walking the parent list until
            // we find a parent matching our wndproc.
            HWND parentWnd = ::GetParent(destWnd);
            while (parentWnd) {
              LONG parentWndProc = ::GetClassLong(parentWnd, GCL_WNDPROC);
              if (parentWndProc == (LONG)&nsWindow::DefaultWindowProc || parentWndProc == (LONG)&nsWindow::WindowProc) {
                // We have a child window - quite possibly a plugin window.
                // However, not all plugins are created equal - some will handle this message themselves,
                // some will forward directly back to us, while others will call DefWndProc, which
                // itself still forwards back to us.
                // So if we have sent it once, we need to handle it ourself.
                if (mIsInMouseWheelProcessing) {
                    destWnd = parentWnd;
                } else {
                    // First time we have seen this message.
                    // Call the child - either it will consume it, or
                    // it will wind it's way back to us, triggering the destWnd case above.
                    // either way, when the call returns, we are all done with the message,
                    mIsInMouseWheelProcessing = PR_TRUE;
                    if (0==SendMessage(destWnd, msg, wParam, lParam)) {
                        result = PR_TRUE; // consumed - don't call DefWndProc
                    }
                    destWnd = nsnull;
                    mIsInMouseWheelProcessing = PR_FALSE;
                }
                break; // stop parent search
              }
              parentWnd = ::GetParent(parentWnd);
            } // while parentWnd
          }
          if (destWnd == nsnull)
              break; // done with this message.
          if (destWnd != mWnd) {
            nsWindow* destWindow = GetNSWindowPtr(destWnd);
            if (destWindow) {
              return destWindow->ProcessMessage(msg, wParam, lParam,
                                                aRetValue);
            }
#ifdef DEBUG
            else
              printf("WARNING: couldn't get child window for MW event\n");
#endif
          }
          
          nsMouseScrollEvent scrollEvent;
          scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;
          if (ulScrollLines == WHEEL_PAGESCROLL) {
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            if (msg == WM_MOUSEWHEEL)
              scrollEvent.delta = (((short) HIWORD (wParam)) > 0) ? -1 : 1;
            else
              scrollEvent.delta = ((int) wParam > 0) ? -1 : 1;
          } else {
            if (msg == WM_MOUSEWHEEL)
              scrollEvent.delta = -((short) HIWORD (wParam) / iDeltaPerLine);
            else
              scrollEvent.delta = -((int) wParam / iDeltaPerLine);
          }
          
          scrollEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;
          scrollEvent.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
          scrollEvent.isControl = IS_VK_DOWN(NS_VK_CONTROL);
          scrollEvent.isMeta    = PR_FALSE;
          scrollEvent.isAlt     = IS_VK_DOWN(NS_VK_ALT);
          InitEvent(scrollEvent, NS_MOUSE_SCROLL);
          if (nsnull != mEventCallback) {
            result = DispatchWindowEvent(&scrollEvent);
          }
          NS_RELEASE(scrollEvent.widget);
        } // WM_MOUSEWHEEL || uMSH_MOUSEWHEEL
        
        //
        // reconvertion meesage for Windows 95 / NT 4.0
        //
        // See the following URL
        //  http://msdn.microsoft.com/library/specs/msimeif_perimeinterfaces.htm#WM_MSIME_RECONVERT
        //  http://www.justsystem.co.jp/tech/atok/api12_04.html#4_11
        
        else if ((msg == nsWindow::uWM_ATOK_RECONVERT) || (msg == nsWindow::uWM_MSIME_RECONVERT)) {
          result = OnIMERequest(wParam, lParam, aRetValue, PR_TRUE);
        }

      } break;
    }

    //*aRetValue = result;
    if (mWnd) {
      return result;
    }
    else {
      //Events which caused mWnd destruction and aren't consumed
      //will crash during the Windows default processing.
      return PR_TRUE;
    }
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsWindow::WindowClass()
{
    const LPCTSTR className = "MozillaWindowClass";

    if (!nsWindow::sIsRegistered) {
        WNDCLASS wc;

//        wc.style            = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.style            = CS_DBLCLKS;
#ifdef MOZ_AIMM
        wc.lpfnWndProc      = nsWindow::DefaultWindowProc;
#else
        wc.lpfnWndProc      = ::DefWindowProc;
#endif
        wc.cbClsExtra       = 0;
        wc.cbWndExtra       = 0;
        wc.hInstance        = nsToolkit::mDllInstance;
        wc.hIcon            = ::LoadIcon(::GetModuleHandle(NULL), IDI_APPLICATION);
        wc.hCursor          = NULL;
        wc.hbrBackground    = mBrush;
        wc.lpszMenuName     = NULL;
        wc.lpszClassName    = className;
    
        nsWindow::sIsRegistered = ::RegisterClass(&wc);
#ifdef MOZ_AIMM
        // Call FilterClientWindows method since it enables ActiveIME on CJK Windows
        if(nsToolkit::gAIMMApp)
          nsToolkit::gAIMMApp->FilterClientWindows((ATOM*)&nsWindow::sIsRegistered,1);
#endif // MOZ_AIMM
    }

    return className;
}


//-------------------------------------------------------------------------
//
// return nsWindow styles
//
//-------------------------------------------------------------------------
DWORD nsWindow::WindowStyle()
{
  DWORD style;
   
  switch(mWindowType) {

    case eWindowType_child:
      style = WS_OVERLAPPED;
      break;

    case eWindowType_dialog:
      if (mBorderStyle == eBorderStyle_default) {
        style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
                DS_3DLOOK | DS_MODALFRAME;
      } else {
        style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
                DS_3DLOOK | DS_MODALFRAME |
                WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      }
      break;

    case eWindowType_popup:
      style = WS_OVERLAPPED | WS_POPUP;
      break;

    default:
      NS_ASSERTION(0, "unknown border style");
      // fall through

    case eWindowType_toplevel:
      style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
              WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      break;
  }

  if (mBorderStyle != eBorderStyle_default && mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_border))
      style &= ~WS_BORDER;
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_title)) {
      style &= ~WS_DLGFRAME;
      style |= WS_POPUP;
    }

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_close))
      style &= ~0;
    // XXX The close box can only be removed by changing the window class,
    // as far as I know   --- roc+moz@cs.cmu.edu

    if (mBorderStyle == eBorderStyle_none ||
      !(mBorderStyle & (eBorderStyle_menu | eBorderStyle_close)))
      style &= ~WS_SYSMENU;
    // Looks like getting rid of the system menu also does away with the
    // close box. So, we only get rid of the system menu if you want neither it
    // nor the close box. How does the Windows "Dialog" window class get just
    // closebox and no sysmenu? Who knows.
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_resizeh))
      style &= ~WS_THICKFRAME;
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_minimize))
      style &= ~WS_MINIMIZEBOX;
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_maximize))
      style &= ~WS_MAXIMIZEBOX;
  }

  return style;
}


//-------------------------------------------------------------------------
//
// return nsWindow extended styles
//
//-------------------------------------------------------------------------
DWORD nsWindow::WindowExStyle()
{
  switch(mWindowType)
  {
    case eWindowType_child:
      return 0;

    case eWindowType_dialog:
      return WS_EX_WINDOWEDGE;

    case eWindowType_popup:
      return WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    default:
      NS_ASSERTION(0, "unknown border style");
      // fall through

    case eWindowType_toplevel:
      return WS_EX_WINDOWEDGE;
  }
}


// -----------------------------------------------------------------------
//
// Subclass (or remove the subclass from) this component's nsWindow
//
// -----------------------------------------------------------------------
void nsWindow::SubclassWindow(BOOL bState)
{
  if (NULL != mWnd) {
    NS_PRECONDITION(::IsWindow(mWnd), "Invalid window handle");
    
    if (bState) {
        // change the nsWindow proc
        mPrevWndProc = (WNDPROC)::SetWindowLong(mWnd, GWL_WNDPROC, 
                                                 (LONG)nsWindow::WindowProc);
        NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
        // connect the this pointer to the nsWindow handle
        SetNSWindowPtr(mWnd, this);
    } 
    else {
        ::SetWindowLong(mWnd, GWL_WNDPROC, (LONG)mPrevWndProc);
        SetNSWindowPtr(mWnd, NULL);
        mPrevWndProc = NULL;
    }
  }
}


//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
    mOnDestroyCalled = PR_TRUE;

    SubclassWindow(FALSE);
    mWnd = NULL;

    // free GDI objects
    if (mBrush) {
      VERIFY(::DeleteObject(mBrush));
      mBrush = NULL;
    }

#if 0
    if (mPalette) {
      VERIFY(::DeleteObject(mPalette));
      mPalette = NULL;
    }
#endif

    // if we were in the middle of deferred window positioning then
    // free the memory for the multiple-window position structure
    if (mDeferredPositioner) {
      VERIFY(::EndDeferWindowPos(mDeferredPositioner));
      mDeferredPositioner = NULL;
    }

    // release references to children, device context, toolkit, and app shell
    nsBaseWidget::OnDestroy();
 
    // dispatch the event
    if (!mIsDestroying) {
      // dispatching of the event may cause the reference count to drop to 0
      // and result in this object being destroyed. To avoid that, add a reference
      // and then release it after dispatching the event
      AddRef();
      DispatchStandardEvent(NS_DESTROY);
      Release();
    }
}

//-------------------------------------------------------------------------
//
// Move
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnMove(PRInt32 aX, PRInt32 aY)
{            
  nsGUIEvent event;
  InitEvent(event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Paint
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnPaint()
{
    nsRect    bounds;
    PRBool result = PR_TRUE;
    PAINTSTRUCT ps;
    nsEventStatus eventStatus = nsEventStatus_eIgnore;

#ifdef NS_DEBUG
    HRGN debugPaintFlashRegion = NULL;
    HDC debugPaintFlashDC = NULL;

    if (debug_WantPaintFlashing())
    {
      debugPaintFlashRegion = ::CreateRectRgn(0, 0, 0, 0);
      ::GetUpdateRgn(mWnd, debugPaintFlashRegion, TRUE);
      debugPaintFlashDC = ::GetDC(mWnd);
    }
#endif // NS_DEBUG

    HDC hDC = ::BeginPaint(mWnd, &ps);

    // XXX What is this check doing? If it's trying to check for an empty
    // paint rect then use the IsRectEmpty() function...
    if (ps.rcPaint.left || ps.rcPaint.right || ps.rcPaint.top || ps.rcPaint.bottom) {
        // call the event callback 
        if (mEventCallback) 
        {

            nsPaintEvent event;

            InitEvent(event, NS_PAINT);

            nsRect rect(ps.rcPaint.left, 
                        ps.rcPaint.top, 
                        ps.rcPaint.right - ps.rcPaint.left, 
                        ps.rcPaint.bottom - ps.rcPaint.top);
            event.rect = &rect;
            event.eventStructType = NS_PAINT_EVENT;

#ifdef NS_DEBUG
          debug_DumpPaintEvent(stdout,
                               this,
                               &event,
                               nsCAutoString("noname"),
                               (PRInt32) mWnd);
#endif // NS_DEBUG

            if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, 
                                                            nsnull, 
                                                            NS_GET_IID(nsIRenderingContext), 
                                                            (void **)&event.renderingContext))
            {
              nsIRenderingContextWin *winrc;

              if (NS_OK == event.renderingContext->QueryInterface(NS_GET_IID(nsIRenderingContextWin), (void **)&winrc))
              {
                nsDrawingSurface surf;

                //i know all of this seems a little backwards. i'll fix it, i swear. MMP

                if (NS_OK == winrc->CreateDrawingSurface(hDC, surf))
                {
                  event.renderingContext->Init(mContext, surf);
                  result = DispatchWindowEvent(&event, eventStatus);
                  event.renderingContext->DestroyDrawingSurface(surf);
                }

                NS_RELEASE(winrc);
              }

              NS_RELEASE(event.renderingContext);
            }
            else
              result = PR_FALSE;

            NS_RELEASE(event.widget);
        }
    }

    ::EndPaint(mWnd, &ps);

#ifdef NS_DEBUG
    if (debug_WantPaintFlashing())
    {
         // Only flash paint events which have not ignored the paint message.
        // Those that ignore the paint message aren't painting anything so there
        // is only the overhead of the dispatching the paint event.
      if (nsEventStatus_eIgnore != eventStatus) {
        ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
        int x;
        for (x = 0; x < 1000000; x++);
        ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
        for (x = 0; x < 1000000; x++);
      }
      ::ReleaseDC(mWnd, debugPaintFlashDC);
      ::DeleteObject(debugPaintFlashRegion);
    }
#endif // NS_DEBUG

    return result;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(nsRect &aWindowRect)
{
  // call the event callback 
  if (mEventCallback) {
    nsSizeEvent event;
    InitEvent(event, NS_SIZE);
    event.windowSize = &aWindowRect;
    event.eventStructType = NS_SIZE_EVENT;
    RECT r;
    if (::GetWindowRect(mWnd, &r)) {
      event.mWinWidth  = PRInt32(r.right - r.left);
      event.mWinHeight = PRInt32(r.bottom - r.top);
    } else {
      event.mWinWidth  = 0;
      event.mWinHeight = 0;
    }
    PRBool result = DispatchWindowEvent(&event);
    NS_RELEASE(event.widget);
    return result;
  }

  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  nsMouseEvent event;
  InitEvent(event, aEventType, aPoint);

  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.eventStructType = NS_MOUSE_EVENT;

  //Dblclicks are used to set the click count, then changed to mousedowns
  LONG curMsgTime = ::GetMessageTime();
  POINT mp;
  DWORD pos = ::GetMessagePos();
  mp.x      = (short)LOWORD(pos);
  mp.y      = (short)HIWORD(pos);
  PRBool insideMovementThreshold = (abs(gLastMousePoint.x - mp.x) < (short)::GetSystemMetrics(SM_CXDOUBLECLK)) &&
                                   (abs(gLastMousePoint.y - mp.y) < (short)::GetSystemMetrics(SM_CYDOUBLECLK));

  // we're going to time double-clicks from mouse *up* to next mouse *down*
  if (aEventType == NS_MOUSE_LEFT_DOUBLECLICK) {
    event.message = NS_MOUSE_LEFT_BUTTON_DOWN;
    gLastClickCount = 2;
  }
  else if (aEventType == NS_MOUSE_MIDDLE_DOUBLECLICK) {
    event.message = NS_MOUSE_MIDDLE_BUTTON_DOWN;
    gLastClickCount = 2;
  }
  else if (aEventType == NS_MOUSE_RIGHT_DOUBLECLICK) {
    event.message = NS_MOUSE_RIGHT_BUTTON_DOWN;
    gLastClickCount = 2;
  }
  else if (aEventType == NS_MOUSE_LEFT_BUTTON_UP || aEventType == NS_MOUSE_MIDDLE_BUTTON_UP || aEventType == NS_MOUSE_RIGHT_BUTTON_UP) {
    // remember when this happened for the next mouse down
    DWORD pos = ::GetMessagePos();
    gLastMousePoint.x = (short)LOWORD(pos);
    gLastMousePoint.y = (short)HIWORD(pos);
  }
  else if (aEventType == NS_MOUSE_LEFT_BUTTON_DOWN || aEventType == NS_MOUSE_MIDDLE_BUTTON_DOWN || aEventType == NS_MOUSE_RIGHT_BUTTON_DOWN) {
    // now look to see if we want to convert this to a double- or triple-click
   
#ifdef NS_DEBUG_XX
    printf("Msg: %d Last: %d Dif: %d Max %d\n", curMsgTime, gLastMouseDownTime, curMsgTime-gLastMouseDownTime, ::GetDoubleClickTime());
    printf("Mouse %d %d\n", abs(gLastMousePoint.x - mp.x), abs(gLastMousePoint.y - mp.y));
#endif
    if (((curMsgTime - gLastMouseDownTime) < (LONG)::GetDoubleClickTime()) && insideMovementThreshold) {
      gLastClickCount ++;
    } else {
      // reset the click count, to count *this* click
      gLastClickCount = 1;
    }
    // Set last Click time on MouseDown only
    gLastMouseDownTime = curMsgTime;
  }
  else if (aEventType == NS_MOUSE_MOVE && !insideMovementThreshold) {
    gLastClickCount = 0;
  }
  event.clickCount = gLastClickCount;

#ifdef NS_DEBUG_XX
  printf("Msg Time: %d Click Count: %d\n", curMsgTime, event.clickCount);
#endif

  nsPluginEvent pluginEvent;

  switch (aEventType)//~~~
  {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      pluginEvent.event = WM_LBUTTONDOWN;
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
      pluginEvent.event = WM_LBUTTONUP;
      break;
    case NS_MOUSE_LEFT_DOUBLECLICK:
      pluginEvent.event = WM_LBUTTONDBLCLK;
      break;
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      pluginEvent.event = WM_RBUTTONDOWN;
      break;
    case NS_MOUSE_RIGHT_BUTTON_UP:
      pluginEvent.event = WM_RBUTTONUP;
      break;
    case NS_MOUSE_RIGHT_DOUBLECLICK:
      pluginEvent.event = WM_RBUTTONDBLCLK;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      pluginEvent.event = WM_MBUTTONDOWN;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_UP:
      pluginEvent.event = WM_MBUTTONUP;
      break;
    case NS_MOUSE_MIDDLE_DOUBLECLICK:
      pluginEvent.event = WM_MBUTTONDBLCLK;
      break;
    case NS_MOUSE_MOVE:
      pluginEvent.event = WM_MOUSEMOVE;
      break;
    default:
      break;
  }

  pluginEvent.wParam = 0;
  pluginEvent.wParam |= (event.isShift) ? MK_SHIFT : 0;
  pluginEvent.wParam |= (event.isControl) ? MK_CONTROL : 0;
  pluginEvent.lParam = MAKELONG(event.point.x, event.point.y);

  event.nativeMsg = (void *)&pluginEvent;

  // call the event callback 
  if (nsnull != mEventCallback) {

    result = DispatchWindowEvent(&event);

    if (aEventType == NS_MOUSE_MOVE) {

      // if we are not in mouse cpature mode (mouse down and hold)
      // then use "this" window
      // if we are in mouse capture, then all events are being directed
      // back to the nsWindow doing the capture. So therefore, the detection
      // of whether we are in a new nsWindow is wrong. Meaning this MOUSE_MOVE
      // event hold the captured windows pointer not the one the mouse is over.
      //
      // So we use "WindowFromPoint" to find what window we are over and 
      // set that window into the mouse trailer timer.
      if (!mIsInMouseCapture) {
        MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
        MouseTrailer::SetMouseTrailerWindow(this);
        mouseTrailer->CreateTimer();
      } else {
        POINT mp;
        DWORD pos = ::GetMessagePos();
        mp.x      = (short)LOWORD(pos);
        mp.y      = (short)HIWORD(pos);

        // OK, now find out if we are still inside
        // the captured native window
        POINT cpos;
        cpos.x = (short)LOWORD(pos);
        cpos.y = (short)HIWORD(pos);

        nsWindow * someWindow = NULL;
        HWND hWnd = ::WindowFromPoint(mp);
        if (hWnd != NULL) {
          ::ScreenToClient(hWnd, &cpos);
          RECT r;
          VERIFY(::GetWindowRect(hWnd, &r));
          if (cpos.x >= r.left && cpos.x <= r.right &&
              cpos.y >= r.top && cpos.y <= r.bottom) {
            // yes we are so we should be able to get a valid window
            // although, strangley enough when we are on the frame part of the
            // window we get right here when in capture mode
            // but this window won't match the capture mode window so
            // we are ok
            someWindow = GetNSWindowPtr(hWnd);
          } 
        }
        // only set the window into the mouse trailer if we have a good window
        if (nsnull != someWindow)  {
          MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
          MouseTrailer::SetMouseTrailerWindow(someWindow);
          mouseTrailer->CreateTimer();
        }
      }

      nsRect rect;
      GetBounds(rect);
      rect.x = 0;
      rect.y = 0;

      if (rect.Contains(event.point.x, event.point.y)) {
        if (gCurrentWindow == NULL || gCurrentWindow != this) {
          if ((nsnull != gCurrentWindow) && (!gCurrentWindow->mIsDestroying)) {
            MouseTrailer::IgnoreNextCycle();
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT, gCurrentWindow->GetLastPoint());
          }
          gCurrentWindow = this;
          if (!mIsDestroying) {
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER);
          }
        }
      } 
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (gCurrentWindow == this) {
        gCurrentWindow = nsnull;
      }
    }
    NS_IF_RELEASE(event.widget);
    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEventType) {
      case NS_MOUSE_MOVE: {
        result = ConvertStatus(mMouseListener->MouseMoved(event));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(event.point.x, event.point.y)) {
          if (gCurrentWindow == NULL || gCurrentWindow != this) {
            gCurrentWindow = this;
          }
        } else {
#ifdef DEBUG
          //printf("Mouse exit");
#endif
        }

      } break;

      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(event));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(event));
        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;
    } // switch
  } 
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Deal with accessibile event
//
//-------------------------------------------------------------------------
#ifdef ACCESSIBILITY
PRBool nsWindow::DispatchAccessibleEvent(PRUint32 aEventType, nsIAccessible** aAcc, nsPoint* aPoint)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback) {
    return result;
  }

  *aAcc = nsnull;

  nsAccessibleEvent event;
  InitEvent(event, aEventType, aPoint);

  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.eventStructType = NS_ACCESSIBLE_EVENT;
  event.accessible = nsnull;

  result = DispatchWindowEvent(&event);

  // if the event returned an accesssible get it.
  if (event.accessible)
    *aAcc = event.accessible;

  NS_RELEASE(event.widget);

  return result;
}
#endif

//-------------------------------------------------------------------------
//
// Deal with focus messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchFocus(PRUint32 aEventType, PRBool isMozWindowTakingFocus)
{
  // call the event callback 
  if (mEventCallback) {
    nsFocusEvent event;
    event.eventStructType = NS_FOCUS_EVENT;
    InitEvent(event, aEventType);

    //focus and blur event should go to their base widget loc, not current mouse pos
    event.point.x = 0;
    event.point.y = 0;

    event.isMozWindowTakingFocus = isMozWindowTakingFocus;

    nsPluginEvent pluginEvent;

    switch (aEventType)//~~~
    {
      case NS_GOTFOCUS:
        pluginEvent.event = WM_SETFOCUS;
        break;
      case NS_LOSTFOCUS:
        pluginEvent.event = WM_KILLFOCUS;
        break;
      case NS_PLUGIN_ACTIVATE:
        pluginEvent.event = WM_KILLFOCUS;
        break;
      default:
        break;
    }

    event.nativeMsg = (void *)&pluginEvent;

    PRBool result = DispatchWindowEvent(&event);
    NS_RELEASE(event.widget);
    return result;
  }
  return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll(UINT scrollCode, int cPos)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Return the brush used to paint the background of this control 
//
//-------------------------------------------------------------------------
HBRUSH nsWindow::OnControlColor()
{
    return mBrush;
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool ChildWindow::DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  switch (aEventType) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
        CaptureMouse(PR_TRUE);
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_UP:
        CaptureMouse(PR_FALSE);
      break;

    default:
      break;

  } // switch

  return nsWindow::DispatchMouseEvent(aEventType, aPoint);
}

//-------------------------------------------------------------------------
//
// return the style for a child nsWindow
//
//-------------------------------------------------------------------------
DWORD ChildWindow::WindowStyle()
{
  return WS_CHILD | WS_CLIPCHILDREN | nsWindow::WindowStyle();
}


static char* GetACPString(const nsString& aStr)
{
   int acplen = aStr.Length() * 2 + 1;
   char * acp = new char[acplen];
   if(acp)
   {
      int outlen = ::WideCharToMultiByte( CP_ACP, 0, 
                      aStr.get(), aStr.Length(),
                      acp, acplen, NULL, NULL);
      if ( outlen >= 0)
         acp[outlen] = '\0';  // null terminate
   }
   return acp;
}

NS_METHOD nsWindow::SetTitle(const nsString& aTitle) 
{
  char* title = GetACPString(aTitle);
  if (title) {
    ::SendMessage(mWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)title);
    delete [] title;
  }
  return NS_OK;
} 

NS_METHOD nsWindow::SetIcon(const nsAReadableString& anIconSpec) 
{
  // Start at app chrome directory.
  nsCOMPtr<nsIFile> chromeDir;
  if ( NS_FAILED( NS_GetSpecialDirectory( NS_APP_CHROME_DIR,
                                          getter_AddRefs( chromeDir ) ) ) ) {
      return NS_ERROR_FAILURE;
  }
  // Get native file name of that directory.
  nsXPIDLString rootPath;
  chromeDir->GetUnicodePath( getter_Copies( rootPath ) );

  // Start there.
  nsAutoString iconPath( rootPath );

  // Now take input path...
  nsAutoString iconSpec( anIconSpec );
  // ...append ".ico" to that.
  iconSpec.Append( NS_LITERAL_STRING(".ico") );
  // ...and figure out where /chrome/... is within that
  // (and skip the "resource:///chrome" part).
  nsAutoString key(NS_LITERAL_STRING("/chrome/"));
  PRInt32 n = iconSpec.Find( key ) + key.Length();
  // Convert / to \.
  nsAutoString slash(NS_LITERAL_STRING("/"));
  nsAutoString bslash(NS_LITERAL_STRING("\\"));
  iconSpec.ReplaceChar( *(slash.get()), *(bslash.get()) );

  // Append that to icon resource path.
  iconPath.Append( iconSpec.get() + n - 1 );

  ::SetLastError( 0 ); 
  HICON bigIcon = (HICON)::LoadImageW( NULL,
                                       (LPCWSTR)iconPath.get(),
                                       IMAGE_ICON,
                                       ::GetSystemMetrics(SM_CXICON),
                                       ::GetSystemMetrics(SM_CYICON),
                                       LR_LOADFROMFILE | LR_SHARED );
  HICON smallIcon = (HICON)::LoadImageW( NULL,
                                         (LPCWSTR)iconPath.get(),
                                         IMAGE_ICON,
                                         ::GetSystemMetrics(SM_CXSMICON),
                                         ::GetSystemMetrics(SM_CYSMICON),
                                         LR_LOADFROMFILE | LR_SHARED );

  // See if unicode API not implemented and if not, try ascii version
  if ( ::GetLastError() == ERROR_CALL_NOT_IMPLEMENTED ) {
      nsCOMPtr<nsILocalFile> pathConverter;
      if ( NS_SUCCEEDED( NS_NewUnicodeLocalFile( iconPath.get(),
                                                 PR_FALSE,
                                                 getter_AddRefs( pathConverter ) ) ) ) {
          // Now try the char* path.
          nsXPIDLCString aPath;
          pathConverter->GetPath( getter_Copies( aPath ) );
          bigIcon = (HICON)::LoadImage( NULL,
                                        (const char*)aPath,
                                        IMAGE_ICON,
                                        ::GetSystemMetrics(SM_CXICON),
                                        ::GetSystemMetrics(SM_CYICON),
                                        LR_LOADFROMFILE | LR_SHARED );
          smallIcon = (HICON)::LoadImage( NULL,
                                          (const char*)aPath,
                                          IMAGE_ICON,
                                          ::GetSystemMetrics(SM_CXSMICON),
                                          ::GetSystemMetrics(SM_CYSMICON),
                                          LR_LOADFROMFILE | LR_SHARED );
      }
  }

  if ( bigIcon ) {
      LRESULT rv = 0;
      rv = ::SendMessage(mWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)bigIcon);
  }
#ifdef DEBUG_law
  else {
    nsCAutoString cPath; cPath.AssignWithConversion(iconPath);
    printf( "\nIcon load error; icon=%s, rc=0x%08X\n\n", (const char*)cPath, ::GetLastError() );
  }
#endif
  if ( smallIcon ) {
      LRESULT rv = 0;
      rv = ::SendMessage(mWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)smallIcon);
  }
#ifdef DEBUG_law
  else {
    nsCAutoString cPath; cPath.AssignWithConversion(iconPath);
    printf( "\nSmall icon load error; icon=%s, rc=0x%08X\n\n", (const char*)cPath, ::GetLastError() );
  }
#endif
  return NS_OK;
} 


PRBool nsWindow::AutoErase()
{
  return(PR_FALSE);
}

NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

void
nsWindow::HandleTextEvent(HIMC hIMEContext,PRBool aCheckAttr)
{
  NS_ASSERTION( mIMECompString, "mIMECompString is null");
  NS_ASSERTION( mIMECompUnicode, "mIMECompUnicode is null");
  NS_ASSERTION( mIMEIsComposing, "conflict state");
  if((nsnull == mIMECompString) || (nsnull == mIMECompUnicode))
	return;

  nsTextEvent		event;
  nsPoint			point;
  size_t			unicharSize;
  CANDIDATEFORM		candForm;
  point.x = 0;
  point.y = 0;

  InitEvent(event, NS_TEXT_EVENT, &point);
 
  //
  // convert the composition string text into unicode before it is sent to xp-land
  // but, on Windows NT / 2000, need not convert.
  //
  if (!nsToolkit::mIsNT) {
    unicharSize = ::MultiByteToWideChar(gCurrentKeyboardCP,MB_PRECOMPOSED,
      mIMECompString->get(),
      mIMECompString->Length(),
      NULL,0);

    mIMECompUnicode->SetCapacity(unicharSize+1);

    unicharSize = ::MultiByteToWideChar(gCurrentKeyboardCP,MB_PRECOMPOSED,
      mIMECompString->get(),
      mIMECompString->Length(),
      (PRUnichar*)mIMECompUnicode->get(),
      mIMECompUnicode->mCapacity);
    ((PRUnichar*)mIMECompUnicode->get())[unicharSize] = (PRUnichar) 0;
    mIMECompUnicode->mLength = unicharSize;
  }

  //
  // we need to convert the attribute array, which is alligned with the mutibyte text into an array of offsets
  // mapped to the unicode text
  //
  
  if(aCheckAttr) {
     MapDBCSAtrributeArrayToUnicodeOffsets(&(event.rangeCount),&(event.rangeArray));
  } else {
     event.rangeCount = 0;
     event.rangeArray = nsnull;
  }

  event.theText = (PRUnichar*)mIMECompUnicode->get();
  event.isShift	= mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isMeta	= PR_FALSE;
  event.isAlt = mIsAltDown;
  event.eventStructType = NS_TEXT_EVENT;

  (void)DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  if(event.rangeArray)
     delete [] event.rangeArray;

  //
  // Post process event
  //
  candForm.dwIndex = 0;
  candForm.dwStyle = CFS_EXCLUDE;
  candForm.ptCurrentPos.x = event.theReply.mCursorPosition.x;
  candForm.ptCurrentPos.y = event.theReply.mCursorPosition.y;
  candForm.rcArea.right = candForm.rcArea.left = candForm.ptCurrentPos.x;
  candForm.rcArea.top = candForm.ptCurrentPos.y;
  candForm.rcArea.bottom = candForm.ptCurrentPos.y+event.theReply.mCursorPosition.height;
 
  NS_IMM_SETCANDIDATEWINDOW(hIMEContext,&candForm);

}

void
nsWindow::HandleStartComposition(HIMC hIMEContext)
{
	NS_ASSERTION( !mIMEIsComposing, "conflict state");
	nsCompositionEvent	event;
	nsPoint				point;
	CANDIDATEFORM		candForm;

	point.x	= 0;
	point.y = 0;

	InitEvent(event,NS_COMPOSITION_START,&point);
	event.eventStructType = NS_COMPOSITION_START;
	event.compositionMessage = NS_COMPOSITION_START;
	(void)DispatchWindowEvent(&event);

	//
	// Post process event
	//
	candForm.dwIndex = 0;
	candForm.dwStyle = CFS_CANDIDATEPOS;
	candForm.ptCurrentPos.x = event.theReply.mCursorPosition.x + IME_X_OFFSET;
	candForm.ptCurrentPos.y = event.theReply.mCursorPosition.y + IME_Y_OFFSET
	                        + event.theReply.mCursorPosition.height;
#ifdef DEBUG_IME2
	printf("Candidate window position: x=%d, y=%d\n",candForm.ptCurrentPos.x,candForm.ptCurrentPos.y);
#endif
	NS_IMM_SETCANDIDATEWINDOW(hIMEContext, &candForm);
	NS_RELEASE(event.widget);

	if(nsnull == mIMECompString)
		mIMECompString = new nsCAutoString();
	if(nsnull == mIMECompUnicode)
		mIMECompUnicode = new nsAutoString();
	mIMEIsComposing = PR_TRUE;

}

void
nsWindow::HandleEndComposition(void)
{
	NS_ASSERTION(mIMEIsComposing, "conflict state");
	nsCompositionEvent	event;
	nsPoint				point;

	point.x	= 0;
	point.y = 0;

	InitEvent(event,NS_COMPOSITION_END,&point);
	event.eventStructType = NS_COMPOSITION_END;
	event.compositionMessage = NS_COMPOSITION_END;
	(void)DispatchWindowEvent(&event);
	NS_RELEASE(event.widget);
	mIMEIsComposing = PR_FALSE;
}

static PRUint32 PlatformToNSAttr(PRUint8 aAttr)
{
 switch(aAttr)
 {
    case ATTR_INPUT_ERROR:
    // case ATTR_FIXEDCONVERTED:
    case ATTR_INPUT:
         return NS_TEXTRANGE_RAWINPUT;
    case ATTR_CONVERTED:
         return NS_TEXTRANGE_CONVERTEDTEXT;
    case ATTR_TARGET_NOTCONVERTED:
         return NS_TEXTRANGE_SELECTEDRAWTEXT;
    case ATTR_TARGET_CONVERTED:
         return NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
    default:
         NS_ASSERTION(PR_FALSE, "unknown attribute");
         return NS_TEXTRANGE_CARETPOSITION;
 }
}
//
// This function converters the composition string (CGS_COMPSTR) into Unicode while mapping the 
//  attribute (GCS_ATTR) string t
void 
nsWindow::MapDBCSAtrributeArrayToUnicodeOffsets(PRUint32* textRangeListLengthResult,nsTextRangeArray* textRangeListResult)
{
  NS_ASSERTION( mIMECompString, "mIMECompString is null");
  NS_ASSERTION( mIMECompUnicode, "mIMECompUnicode is null");
  if((nsnull == mIMECompString) || (nsnull == mIMECompUnicode))
	return;
	PRInt32	rangePointer;
	size_t		lastUnicodeOffset, substringLength, lastMBCSOffset;

  //
  // On Windows NT / 2000, it doesn't use mIMECompString.
  // IME strings already store to mIMECompUnicode.
  //

  long maxlen = nsToolkit::mIsNT ? mIMECompUnicode->Length() : mIMECompString->Length();
  long cursor = mIMECursorPosition;
  NS_ASSERTION(cursor <= maxlen, "wrong cursor positoin");
  if(cursor > maxlen)
    cursor = maxlen;

	//
	// figure out the ranges from the compclause string
	//
	if (mIMECompClauseStringLength==0) {
		*textRangeListLengthResult = 2;
		*textRangeListResult = new nsTextRange[2];
		(*textRangeListResult)[0].mStartOffset=0;
		substringLength = nsToolkit::mIsNT ? mIMECompUnicode->Length() :
      ::MultiByteToWideChar(gCurrentKeyboardCP,MB_PRECOMPOSED,
				mIMECompString->get(), maxlen,NULL,0);
		(*textRangeListResult)[0].mEndOffset = substringLength;
		(*textRangeListResult)[0].mRangeType = NS_TEXTRANGE_RAWINPUT;
		substringLength = nsToolkit::mIsNT ? cursor :
      ::MultiByteToWideChar(gCurrentKeyboardCP,MB_PRECOMPOSED,mIMECompString->get(),
								cursor,NULL,0);
		(*textRangeListResult)[1].mStartOffset=substringLength;
		(*textRangeListResult)[1].mEndOffset = substringLength;
		(*textRangeListResult)[1].mRangeType = NS_TEXTRANGE_CARETPOSITION;

		
	} else {
		
		*textRangeListLengthResult =  mIMECompClauseStringLength;

		//
		//  allocate the offset array
		//
		*textRangeListResult = new nsTextRange[*textRangeListLengthResult];

		//
		// figure out the cursor position
		//
		
		substringLength =  nsToolkit::mIsNT ? cursor :
      ::MultiByteToWideChar(gCurrentKeyboardCP,
          MB_PRECOMPOSED,mIMECompString->get(),cursor,NULL,0);
		(*textRangeListResult)[0].mStartOffset=substringLength;
		(*textRangeListResult)[0].mEndOffset = substringLength;
		(*textRangeListResult)[0].mRangeType = NS_TEXTRANGE_CARETPOSITION;

	
		//
		// iterate over the attributes and convert them into unicode 
		for(rangePointer=1, lastUnicodeOffset= lastMBCSOffset = 0;
			rangePointer<mIMECompClauseStringLength;
				rangePointer++) 
		{
      long current = mIMECompClauseString[rangePointer];
  		NS_ASSERTION(current <= maxlen, "wrong offset");
  		if(current > maxlen)
				current = maxlen;

			(*textRangeListResult)[rangePointer].mRangeType = 
        PlatformToNSAttr(mIMEAttributeString[lastMBCSOffset]);
			(*textRangeListResult)[rangePointer].mStartOffset = lastUnicodeOffset;

			lastUnicodeOffset += nsToolkit::mIsNT ? current - lastMBCSOffset :
        ::MultiByteToWideChar(gCurrentKeyboardCP,
          MB_PRECOMPOSED,mIMECompString->get()+lastMBCSOffset,
				current-lastMBCSOffset,NULL,0);

			(*textRangeListResult)[rangePointer].mEndOffset = lastUnicodeOffset;

			lastMBCSOffset = current;
		} // for
	} // if else


}




//==========================================================================
BOOL nsWindow::OnInputLangChange(HKL aHKL, LRESULT *oRetValue)			
{
#ifdef KE_DEBUG
	printf("OnInputLanguageChange\n");
#endif

  if(gKeyboardLayout != aHKL) 
  {
    gKeyboardLayout = aHKL;
    *oRetValue = LangIDToCP((WORD)((DWORD)gKeyboardLayout & 0x0FFFF),
                    gCurrentKeyboardCP);
  }

  ResetInputState();

  if (mIMEIsComposing)  {
    HandleEndComposition();
  }

  return PR_FALSE;   // always pass to child window
}
//==========================================================================
BOOL nsWindow::OnIMEChar(BYTE aByte1, BYTE aByte2, LPARAM aKeyState)
{
#ifdef DEBUG_IME
	printf("OnIMEChar\n");
#endif
	NS_ASSERTION(PR_TRUE, "should not got an WM_IME_CHAR");

	// not implement yet
	return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMEComposition(LPARAM  aGCS)			
{
#ifdef DEBUG_IME
	printf("OnIMEComposition\n");
#endif
  // for bug #60050
  // MS-IME 95/97/98/2000 may send WM_IME_COMPOSITION with non-conversion
  // mode before it send WM_IME_STARTCOMPOSITION.

  if (mIMEIsComposing != PR_TRUE)
  {
    if(!mIMECompString)
      mIMECompString = new nsCAutoString();

    if(!mIMECompUnicode)
      mIMECompUnicode = new nsAutoString();
  }

  NS_ASSERTION( mIMECompString, "mIMECompString is null");
  NS_ASSERTION( mIMECompUnicode, "mIMECompUnicode is null");
  if((nsnull == mIMECompString) || (nsnull == mIMECompUnicode))
	return PR_TRUE;

	HIMC hIMEContext;

	BOOL result = PR_FALSE;					// will change this if an IME message we handle
	NS_IMM_GETCONTEXT(mWnd, hIMEContext);
	if (hIMEContext==NULL) 
		return PR_TRUE;

	//
	// This catches a fixed result
	//
	if (aGCS & GCS_RESULTSTR) {
#ifdef DEBUG_IME
		fprintf(stderr,"Handling GCS_RESULTSTR\n");
#endif
		if(! mIMEIsComposing) 
			HandleStartComposition(hIMEContext);

    //
    // On Windows 2000, ImmGetCompositionStringA() don't work well using IME of
    // different code page.  (See BUG # 29606)
    // And ImmGetCompositionStringW() don't work on Windows 9x.
    //

		long compStrLen;
    if (nsToolkit::mIsNT) {
      // Imm* Unicode API works on Windows NT / 2000.
      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext, GCS_RESULTSTR, NULL, 0, compStrLen);

      mIMECompUnicode->SetCapacity((compStrLen / sizeof(WCHAR))+1);

      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext, GCS_RESULTSTR,
        (LPVOID)mIMECompUnicode->get(), (mIMECompString->mCapacity * sizeof(WCHAR)), compStrLen);
      compStrLen = compStrLen / sizeof(WCHAR);
      ((PRUnichar*)mIMECompUnicode->get())[compStrLen] = '\0';
      mIMECompUnicode->mLength = compStrLen;
    } else {
      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext, GCS_RESULTSTR, NULL, 0, compStrLen);

      mIMECompString->SetCapacity(compStrLen+1);

      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext, GCS_RESULTSTR,
        (LPVOID)mIMECompString->get(), mIMECompString->mCapacity, compStrLen);
      ((char*)mIMECompString->get())[compStrLen] = '\0';
      mIMECompString->mLength = compStrLen;
    }
#ifdef DEBUG_IME
		fprintf(stderr,"GCS_RESULTSTR compStrLen = %d\n", compStrLen);
#endif
		result = PR_TRUE;
		HandleTextEvent(hIMEContext, PR_FALSE);
		HandleEndComposition();
	}


	//
	// This provides us with a composition string
	//
	if (aGCS & 
			(GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS ))
	{
#ifdef DEBUG_IME
		fprintf(stderr,"Handling GCS_COMPSTR\n");
#endif

		if(! mIMEIsComposing) 
			HandleStartComposition(hIMEContext);

		//--------------------------------------------------------
		// 1. Get GCS_COMPATTR
		//--------------------------------------------------------
		// This provides us with the attribute string necessary 
		// for doing hiliting
		long attrStrLen;
    if (nsToolkit::mIsNT) {
      // Imm* Unicode API works on Windows NT / 2000.
      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext, GCS_COMPATTR, NULL, 0, attrStrLen);
      if (attrStrLen > mIMEAttributeStringSize) {
        if (mIMEAttributeString != NULL) 
          delete [] mIMEAttributeString;
        mIMEAttributeString = new PRUint8[attrStrLen+32];
        mIMEAttributeStringSize = attrStrLen+32;
      }

      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext, GCS_COMPATTR, mIMEAttributeString, mIMEAttributeStringSize, attrStrLen);
    } else {
      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext, GCS_COMPATTR, NULL, 0, attrStrLen);
      if (attrStrLen>mIMEAttributeStringSize) {
        if (mIMEAttributeString!=NULL) 
          delete [] mIMEAttributeString;
        mIMEAttributeString = new PRUint8[attrStrLen+32];
        mIMEAttributeStringSize = attrStrLen+32;
      }

      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext, GCS_COMPATTR, mIMEAttributeString, mIMEAttributeStringSize, attrStrLen);
    }

		mIMEAttributeStringLength = attrStrLen;

		//--------------------------------------------------------
		// 2. Get GCS_COMPCLAUSE
		//--------------------------------------------------------
		long compClauseLen;
    long compClauseLen2;
    if (nsToolkit::mIsNT) {
      // Imm* Unicode API works on Windows NT / 2000.
      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext,
        GCS_COMPCLAUSE, NULL, 0, compClauseLen);
      compClauseLen = compClauseLen / sizeof(PRUint32);

      if (compClauseLen > mIMECompClauseStringSize) {
        if (mIMECompClauseString != NULL) 
          delete [] mIMECompClauseString;
        mIMECompClauseString = new PRUint32 [compClauseLen+32];
        mIMECompClauseStringSize = compClauseLen+32;
      }

      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext,
        GCS_COMPCLAUSE,
        mIMECompClauseString,
        mIMECompClauseStringSize * sizeof(PRUint32),
        compClauseLen2);
    } else {
      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext,
        GCS_COMPCLAUSE, NULL, 0, compClauseLen);
      compClauseLen = compClauseLen / sizeof(PRUint32);

      if (compClauseLen>mIMECompClauseStringSize) {
        if (mIMECompClauseString!=NULL) 
          delete [] mIMECompClauseString;
        mIMECompClauseString = new PRUint32 [compClauseLen+32];
        mIMECompClauseStringSize = compClauseLen+32;
      }

      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext,
        GCS_COMPCLAUSE,
        mIMECompClauseString,
        mIMECompClauseStringSize * sizeof(PRUint32),
        compClauseLen2);
    }
		compClauseLen2 = compClauseLen2 / sizeof(PRUint32);
                NS_ASSERTION(compClauseLen2 == compClauseLen, "strange result");
                if(compClauseLen > compClauseLen2)
                  compClauseLen = compClauseLen2;
		mIMECompClauseStringLength = compClauseLen;
 

		//--------------------------------------------------------
		// 3. Get GCS_CURSOPOS
		//--------------------------------------------------------
    if (nsToolkit::mIsNT) {
      // Imm* Unicode API works on Windows NT / 2000.
      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext, GCS_CURSORPOS, NULL, 0, mIMECursorPosition);
    } else {
      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext, GCS_CURSORPOS, NULL, 0, mIMECursorPosition);
    }

		//--------------------------------------------------------
		// 4. Get GCS_COMPSTR
		//--------------------------------------------------------
		long compStrLen;
    if (nsToolkit::mIsNT) {
      // Imm* Unicode API works on Windows NT / 2000.
      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext, GCS_COMPSTR, NULL, 0, compStrLen);

      mIMECompUnicode->SetCapacity((compStrLen / sizeof(WCHAR)) + 1);

      NS_IMM_GETCOMPOSITIONSTRINGW(hIMEContext,
        GCS_COMPSTR,
        (LPVOID)mIMECompUnicode->get(),
        (mIMECompUnicode->mCapacity * sizeof(WCHAR)), compStrLen);
      compStrLen = compStrLen / sizeof(WCHAR);
      ((PRUnichar*)mIMECompUnicode->get())[compStrLen] = '\0';
      mIMECompUnicode->mLength = compStrLen;
    } else {
      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext, GCS_COMPSTR, NULL, 0, compStrLen);
      mIMECompString->SetCapacity(compStrLen+1);

      NS_IMM_GETCOMPOSITIONSTRING(hIMEContext,
        GCS_COMPSTR,
        (char*)mIMECompString->get(),
        mIMECompString->mCapacity, compStrLen);
      ((char*)mIMECompString->get())[compStrLen] = '\0';
      mIMECompString->mLength = compStrLen;
    }
#ifdef DEBUG_IME
		fprintf(stderr,"GCS_COMPSTR compStrLen = %d\n", compStrLen);
#endif
#ifdef DEBUG
                for(int kk=0;kk<mIMECompClauseStringLength;kk++)
                {
                  NS_ASSERTION(mIMECompClauseString[kk] <= (nsToolkit::mIsNT ? mIMECompUnicode->mLength : mIMECompString->mLength), "illegal pos");
                }
#endif
		//--------------------------------------------------------
		// 5. Sent the text event
		//--------------------------------------------------------
		HandleTextEvent(hIMEContext);
		result = PR_TRUE;
	}
	if(! result)
	{
#ifdef DEBUG_IME
		fprintf(stderr,"Haandle 0 length TextEvent. \n");
#endif
		if(! mIMEIsComposing) 
			HandleStartComposition(hIMEContext);

    if (nsToolkit::mIsNT) {
      ((PRUnichar*)mIMECompUnicode->get())[0] = '\0';
      mIMECompUnicode->mLength = 0;
    } else {
      ((char*)mIMECompString->get())[0] = '\0';
      mIMECompString->mLength = 0;
    }
		HandleTextEvent(hIMEContext,PR_FALSE);
		result = PR_TRUE;
	}

	NS_IMM_RELEASECONTEXT(mWnd, hIMEContext);
	return result;
}
//==========================================================================
BOOL nsWindow::OnIMECompositionFull()			
{
#ifdef DEBUG_IME2
	printf("OnIMECompositionFull\n");
#endif

	// not implement yet
	return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMEEndComposition()			
{
#ifdef DEBUG_IME
	printf("OnIMEEndComposition\n");
#endif
	if(mIMEIsComposing)
	{
		HIMC hIMEContext;

		if ((mIMEProperty & IME_PROP_SPECIAL_UI) || 
			  (mIMEProperty & IME_PROP_AT_CARET)) 
			return PR_FALSE;

		NS_IMM_GETCONTEXT(mWnd, hIMEContext);
		if (hIMEContext==NULL) 
			return PR_TRUE;

		// IME on Korean NT somehow send WM_IME_ENDCOMPOSITION
		// first when we hit space in composition mode
		// we need to clear out the current composition string 
		// in that case. 
    if (nsToolkit::mIsNT) {
      ((PRUnichar*)mIMECompUnicode->get())[0] = '\0';
      mIMECompUnicode->mLength = 0;
    } else {
      ((char*)mIMECompString->get())[0] = '\0';
      mIMECompString->mLength = 0;
    }
		HandleTextEvent(hIMEContext, PR_FALSE);

		HandleEndComposition();
		NS_IMM_RELEASECONTEXT(mWnd, hIMEContext);
	}
	return PR_TRUE;
}
//==========================================================================
BOOL nsWindow::OnIMENotify(WPARAM  aIMN, LPARAM aData, LRESULT *oResult)	
{
#ifdef DEBUG_IME2
	printf("OnIMENotify ");
	switch(aIMN) {
		case IMN_CHANGECANDIDATE:
			printf("IMN_CHANGECANDIDATE %x\n", aData);
		break;
		case IMN_CLOSECANDIDATE:
			printf("IMN_CLOSECANDIDATE %x\n", aData);
		break;
		case IMN_CLOSESTATUSWINDOW:
			printf("IMN_CLOSESTATUSWINDOW\n");
		break;
		case IMN_GUIDELINE:
			printf("IMN_GUIDELINE\n");
		break;
		case IMN_OPENCANDIDATE:
			printf("IMN_OPENCANDIDATE %x\n", aData);
		break;
		case IMN_OPENSTATUSWINDOW:
			printf("IMN_OPENSTATUSWINDOW\n");
		break;
		case IMN_SETCANDIDATEPOS:
			printf("IMN_SETCANDIDATEPOS %x\n", aData);
		break;
		case IMN_SETCOMPOSITIONFONT:
			printf("IMN_SETCOMPOSITIONFONT\n");
		break;
		case IMN_SETCOMPOSITIONWINDOW:
			printf("IMN_SETCOMPOSITIONWINDOW\n");
		break;
		case IMN_SETCONVERSIONMODE:
			printf("IMN_SETCONVERSIONMODE\n");
		break;
		case IMN_SETOPENSTATUS:
			printf("IMN_SETOPENSTATUS\n");
		break;
		case IMN_SETSENTENCEMODE:
			printf("IMN_SETSENTENCEMODE\n");
		break;
		case IMN_SETSTATUSWINDOWPOS:
			printf("IMN_SETSTATUSWINDOWPOS\n");
		break;
		case IMN_PRIVATE:
			printf("IMN_PRIVATE\n");
		break;
	};
#endif

  // add hacky code here
  if(IS_VK_DOWN(NS_VK_ALT)) {
      mIsShiftDown = PR_FALSE;
      mIsControlDown = PR_FALSE;
      mIsAltDown = PR_TRUE;
      DispatchKeyEvent(NS_KEY_PRESS, 0, 192);// XXX hack hack hack
      if (aIMN == IMN_SETOPENSTATUS)
        mIMEIsStatusChanged = PR_TRUE;
  }
	// not implement yet 
	return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMERequest(WPARAM  aIMR, LPARAM aData, LRESULT *oResult, PRBool aUseUnicode)
{
#ifdef DEBUG_IME
	printf("OnIMERequest\n");
#endif

  PRBool result = PR_FALSE;
  
  switch(aIMR) {
    case IMR_RECONVERTSTRING:
      result = OnIMEReconvert(aData, oResult, aUseUnicode);
      break;
  }

  return result;
}

//==========================================================================
PRBool nsWindow::OnIMEReconvert(LPARAM aData, LRESULT *oResult, PRBool aUseUnicode)
{
#ifdef DEBUG_IME
  printf("OnIMEReconvert\n");
#endif

  PRBool           result  = PR_FALSE;
  RECONVERTSTRING* pReconv = (RECONVERTSTRING*) aData;
  int              len = 0;

  if(!pReconv) {

    //
    // When reconvert, it must return need size to reconvert.
    //
    if(mIMEReconvertUnicode) {
      nsMemory::Free(mIMEReconvertUnicode);
      mIMEReconvertUnicode = NULL;
    }

    // Get reconversion string
    nsReconversionEvent event;
    nsPoint point;

    point.x = 0;
    point.y = 0;
    InitEvent(event, NS_RECONVERSION_QUERY, &point);
    event.theReply.mReconversionString = NULL;
    DispatchWindowEvent(&event);

    mIMEReconvertUnicode = event.theReply.mReconversionString;
    NS_RELEASE(event.widget);

    // Return need size

    if(mIMEReconvertUnicode) {
      if (aUseUnicode) {
        len = nsCRT::strlen(mIMEReconvertUnicode);
        *oResult = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);
      } else {
        len = ::WideCharToMultiByte(gCurrentKeyboardCP, 0,
                                    mIMEReconvertUnicode,
                                    nsCRT::strlen(mIMEReconvertUnicode),
                                    NULL, 0, NULL, NULL);
        *oResult = sizeof(RECONVERTSTRING) + len;
      }

      result = PR_TRUE;
    }
  } else {

    //
    // Fill reconvert struct
    //

    if (aUseUnicode) {
      len = nsCRT::strlen(mIMEReconvertUnicode);
      *oResult = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);
    } else {
      len = ::WideCharToMultiByte(gCurrentKeyboardCP, 0,
                                  mIMEReconvertUnicode,
                                  nsCRT::strlen(mIMEReconvertUnicode),
                                  NULL, 0, NULL, NULL);
      *oResult = sizeof(RECONVERTSTRING) + len;
    }

    ::ZeroMemory(pReconv, sizeof(RECONVERTSTRING));
    pReconv->dwSize            = sizeof(RECONVERTSTRING);
    pReconv->dwVersion         = 0;
    pReconv->dwStrLen          = len;
    pReconv->dwStrOffset       = sizeof(RECONVERTSTRING);
    pReconv->dwCompStrLen      = len;
    pReconv->dwCompStrOffset   = 0;
    pReconv->dwTargetStrLen    = len;
    pReconv->dwTargetStrOffset = 0;

    if (aUseUnicode) {
      ::CopyMemory((LPVOID) (aData + sizeof(RECONVERTSTRING)),
                   mIMEReconvertUnicode, len * sizeof(WCHAR));
    } else {
      ::WideCharToMultiByte(gCurrentKeyboardCP, 0,
                            mIMEReconvertUnicode,
                            nsCRT::strlen(mIMEReconvertUnicode),
                            (LPSTR) (aData + sizeof(RECONVERTSTRING)),
                            len,
                            NULL, NULL);
    }

    result = PR_TRUE;
  }

  return result;
}

//==========================================================================
BOOL nsWindow::OnIMESelect(BOOL  aSelected, WORD aLangID)			
{
#ifdef DEBUG_IME2
	printf("OnIMESelect\n");
#endif

	// not implement yet
	return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMESetContext(BOOL aActive, LPARAM& aISC)			
{
#ifdef DEBUG_IME2
	printf("OnIMESetContext %x %s %s %s Candidate[%s%s%s%s]\n", this, 
		(aActive ? "Active" : "Deactiv"),
		((aISC & ISC_SHOWUICOMPOSITIONWINDOW) ? "[Comp]" : ""),
		((aISC & ISC_SHOWUIGUIDELINE) ? "[GUID]" : ""),
		((aISC & ISC_SHOWUICANDIDATEWINDOW) ? "0" : ""),
		((aISC & (ISC_SHOWUICANDIDATEWINDOW<<1)) ? "1" : ""),
		((aISC & (ISC_SHOWUICANDIDATEWINDOW<<2)) ? "2" : ""),
		((aISC & (ISC_SHOWUICANDIDATEWINDOW<<3)) ? "3" : "")
	);
#endif
	if(! aActive)
		ResetInputState();

	aISC &= ~ ISC_SHOWUICOMPOSITIONWINDOW;
	// We still return false here because we need to pass the 
	// aISC w/ ISC_SHOWUICOMPOSITIONWINDOW clear to the default
	// window proc so it will draw the candidcate window for us...
	return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMEStartComposition()
{
#ifdef DEBUG_IME
	printf("OnIMEStartComposition\n");
#endif
	HIMC hIMEContext;

	if ((mIMEProperty & IME_PROP_SPECIAL_UI) || 
      (mIMEProperty & IME_PROP_AT_CARET)) 
		return PR_FALSE;

	NS_IMM_GETCONTEXT(mWnd, hIMEContext);
	if (hIMEContext==NULL) 
		return PR_TRUE;

	HandleStartComposition(hIMEContext);
	NS_IMM_RELEASECONTEXT(mWnd, hIMEContext);
	return PR_TRUE;
}

//==========================================================================
NS_IMETHODIMP nsWindow::ResetInputState()
{
#ifdef DEBUG_KBSTATE
	printf("ResetInputState\n");
#endif 
	//if(mIMEIsComposing) {
		HIMC hIMC;
		NS_IMM_GETCONTEXT(mWnd, hIMC);
		if(hIMC) {
			BOOL ret = FALSE;
      NS_IMM_NOTIFYIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, NULL, ret);
      NS_IMM_NOTIFYIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, NULL, ret);
			//NS_ASSERTION(ret, "ImmNotify failed");
			NS_IMM_RELEASECONTEXT(mWnd, hIMC);
		}
	//}
	return NS_OK;
}


#if IME_MOUSE_EVENT_SUPPORT
// Mouse operation of IME
PRBool
nsWindow::HandleMouseActionOfIME(int aAction)
{
  PRBool IsHandle = PR_FALSE;

  if (mWnd) {
    HIMC hIMC = NULL;
    NS_IMM_GETCONTEXT(mWnd, hIMC);
    if (hIMC) {
      int positioning = 0;
      int offset = 0;

      // get location of each compositon charactors

      // calcurate positioning and offset

      // send MS_MSIME_MOUSE message to default IME window.
      if (::SendMessage(mWnd, nsWindow::uWM_MSIME_MOUSE, MAKELONG(MAKEWORD(aAction, positioning), offset), (LPARAM) hIMC) == 1)
        IsHandle = PR_TRUE;
      }
    NS_IMM_RELEASECONTEXT(mWnd, hIMC);
  }

  return IsHandle;
}

//The coordinate is relative to the upper-left corner of the client area. 
PRBool nsWindow::IMECompositionHitTest(PRUint32 aEventType, POINT * ptPos)
{
  PRBool IsHit = PR_FALSE;

	COMPOSITIONFORM cpForm;

  if (mWnd) {
    HIMC hIMC = NULL;
    NS_IMM_GETCONTEXT(mWnd, hIMC);
    if (hIMC) {
			GetCompositionWindowPos(hIMC, aEventType, &cpForm);
			if (PtInRect(&cpForm.rcArea, *ptPos))
				IsHit = PR_TRUE;
		}
    NS_IMM_RELEASECONTEXT(mWnd, hIMC);
	}
  return IsHit;
}

void nsWindow::GetCompositionWindowPos(HIMC hIMC, PRUint32 aEventType, COMPOSITIONFORM *cpForm)
{
  nsTextEvent		event;
  POINT point;
  point.x = 0;
  point.y = 0;
  DWORD pos = ::GetMessagePos();

  point.x = (short)LOWORD(pos);
  point.y = (short)HIWORD(pos);

  if (mWnd != NULL) {
    ::ScreenToClient(mWnd, &point);
    event.point.x = point.x;
    event.point.y = point.y;
  } else {
    event.point.x = 0;
    event.point.y = 0;
  }

  NS_IMM_GETCOMPOSITIONWINDOW(hIMC, cpForm);

  cpForm->ptCurrentPos.x = event.theReply.mCursorPosition.x + IME_X_OFFSET;
  cpForm->ptCurrentPos.y = event.theReply.mCursorPosition.y + IME_Y_OFFSET
                          + event.theReply.mCursorPosition.height ;
	cpForm->rcArea.left = cpForm->ptCurrentPos.x;
	cpForm->rcArea.top = cpForm->ptCurrentPos.y;
	cpForm->rcArea.right = cpForm->ptCurrentPos.x + event.theReply.mCursorPosition.width;
	cpForm->rcArea.bottom = cpForm->ptCurrentPos.y + event.theReply.mCursorPosition.height;
}

#endif

// Pick some random timer ID.  Is there a better way?
#define NS_FLASH_TIMER_ID 0x011231984

// This function is called on a timer to do the flashing.  It simply toggles the flash
// status until the window comes to the foreground.
static VOID CALLBACK nsGetAttentionTimerFunc( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime ) {
    // Flash the window until we're in the foreground.
    if ( GetForegroundWindow() != hwnd ) {
        // We're not in the foreground yet.
        FlashWindow( hwnd, TRUE );
    } else {
        FlashWindow( hwnd, FALSE );
        KillTimer( hwnd, idEvent );
    }
}

// Draw user's attention to this window until it comes to foreground.
NS_IMETHODIMP
nsWindow::GetAttention() {
    // Got window?
    if ( !mWnd ) {
        return NS_ERROR_NOT_INITIALIZED;
    }

    // If window is in foreground, no notification is necessary.
    if ( GetForegroundWindow() != mWnd ) {
        // Kick off timer that does single flash till window comes to foreground.
        SetTimer( mWnd, NS_FLASH_TIMER_ID, GetCaretBlinkTime(), (TIMERPROC)nsGetAttentionTimerFunc );
    }

    return NS_OK;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-- NOTE!!! These hook functions can be removed when we migrate to 
//-- XBL-Form Controls
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//#define DISPLAY_NOISY_MSGF_MSG

#ifdef DISPLAY_NOISY_MSGF_MSG
typedef struct {
  char * mStr;
  int    mId;
} MSGFEventMsgInfo;

MSGFEventMsgInfo gMSGFEvents[] = {
  "MSGF_DIALOGBOX",      0,
  "MSGF_MESSAGEBOX",     1,
  "MSGF_MENU",           2,
  "MSGF_SCROLLBAR",      5,
  "MSGF_NEXTWINDOW",     6,
  "MSGF_MAX",            8,
  "MSGF_USER",          4096,
  NULL, 0};

  void PrintEvent(UINT msg, PRBool aShowAllEvents, PRBool aShowMouseMoves);
  int gLastMsgCode = 0;

#define DISPLAY_NMM_PRT(_arg) printf((_arg));
#else
#define DISPLAY_NMM_PRT(_arg) 
#endif


//-------------------------------------------------------------------------
// Scedules a timer for a window, so we can rollup after processing the hook event
void nsWindow::ScheduleHookTimer(HWND aWnd, UINT aMsgId)
{
  // In some cases multiple hooks may be schedule
  // so ignore any other requests once one timer is scheduled
  if (gHookTimerId == 0) {
    // Remember the message ID to be used later
    gRollupMsgId = aMsgId;
    // Schedule native timer for doing the rollup after 
    // this event is done being processed
    gHookTimerId = ::SetTimer( NULL, 0, 0, (TIMERPROC)HookTimerForPopups );
    NS_ASSERTION(gHookTimerId, "Timer couldn't be created.");
  }
}

//-------------------------------------------------------------------------
// Process Menu messages 
// Rollup when when is clicked
LRESULT CALLBACK nsWindow::MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef DISPLAY_NOISY_MSGF_MSG
  if (gProcessHook) {
    MSG* pMsg = (MSG*)lParam;

    int inx = 0;
    while (gMSGFEvents[inx].mId != code && gMSGFEvents[inx].mStr != NULL) {
      inx++;
    }
    if (code != gLastMsgCode) {
      if (gMSGFEvents[inx].mId == code) {
#ifdef DEBUG
        printf("MozSpecialMessageProc - code: 0x%X  - %s  hw: %p\n", code, gMSGFEvents[inx].mStr, pMsg->hwnd);
#endif
      } else {
#ifdef DEBUG
        printf("MozSpecialMessageProc - code: 0x%X  - %d  hw: %p\n", code, gMSGFEvents[inx].mId, pMsg->hwnd);
#endif
      }
      gLastMsgCode = code;
    }
    PrintEvent(pMsg->message, FALSE, FALSE);
  }
#endif

  if (gProcessHook && code == MSGF_MENU) {
    MSG* pMsg = (MSG*)lParam;
    ScheduleHookTimer( pMsg->hwnd, pMsg->message);
  }

  return ::CallNextHookEx(gMsgFilterHook, code, wParam, lParam);
}

//-------------------------------------------------------------------------
// Process all mouse messages
// Roll up when a click is in a native window that doesn't have an nsIWidget
LRESULT CALLBACK nsWindow::MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (gProcessHook) {
    MOUSEHOOKSTRUCT* ms = (MOUSEHOOKSTRUCT*)lParam;
    if (wParam == WM_LBUTTONDOWN) {
      nsIWidget* mozWin = (nsIWidget*)GetNSWindowPtr(ms->hwnd);
      if (mozWin == NULL) {
        ScheduleHookTimer( ms->hwnd, (UINT)wParam);
      }
    }
  }
  return ::CallNextHookEx(gCallMouseHook, code, wParam, lParam);
}

//-------------------------------------------------------------------------
// Process all messages
// Roll up when the window is moving, or is resizing or when maximized or mininized
LRESULT CALLBACK nsWindow::MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef DISPLAY_NOISY_MSGF_MSG
    if (gProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    PrintEvent(cwpt->message, FALSE, FALSE);
  }
#endif

  if (gProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    if (cwpt->message == WM_MOVING || 
        cwpt->message == WM_SIZING || 
        cwpt->message == WM_GETMINMAXINFO) {
      ScheduleHookTimer( cwpt->hwnd, (UINT)cwpt->message);
    }
  }

  return ::CallNextHookEx(gCallProcHook, code, wParam, lParam);

}

//-------------------------------------------------------------------------
// Register the special "hooks" for dropdown processing
void nsWindow::RegisterSpecialDropdownHooks()
{
  NS_ASSERTION(!gMsgFilterHook, "gMsgFilterHook must be NULL!");
  NS_ASSERTION(!gCallProcHook,  "gCallProcHook must be NULL!");

  DISPLAY_NMM_PRT("***************** Installing Msg Hooks ***************\n");

  //HMODULE hMod = GetModuleHandle("gkwidget.dll");

  // Install msg hook for moving the window and resizing
  if (!gMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Hooking gMsgFilterHook!\n");
    gMsgFilterHook = SetWindowsHookEx(WH_MSGFILTER, MozSpecialMsgFilter, NULL, GetCurrentThreadId());
#ifdef DISPLAY_NOISY_MSGF_MSG
    if (!gMsgFilterHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_MSGFILTER!\n");
    }
#endif
  }

  // Install msg hook for menus
  if (!gCallProcHook) {
    DISPLAY_NMM_PRT("***** Hooking gCallProcHook!\n");
    gCallProcHook  = SetWindowsHookEx(WH_CALLWNDPROC, MozSpecialWndProc, NULL, GetCurrentThreadId());
#ifdef DISPLAY_NOISY_MSGF_MSG
    if (!gCallProcHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_CALLWNDPROC!\n");
    }
#endif
  }

  // Install msg hook for the mouse
  if (!gCallMouseHook) {
    DISPLAY_NMM_PRT("***** Hooking gCallMouseHook!\n");
    gCallMouseHook  = SetWindowsHookEx(WH_MOUSE, MozSpecialMouseProc, NULL, GetCurrentThreadId());
#ifdef DISPLAY_NOISY_MSGF_MSG
    if (!gCallMouseHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_MOUSE!\n");
    }
#endif
  }
}

//-------------------------------------------------------------------------
// Unhook special message hooks for dropdowns
void nsWindow::UnregisterSpecialDropdownHooks()
{

  DISPLAY_NMM_PRT("***************** De-installing Msg Hooks ***************\n");

  if (gCallProcHook) {
    DISPLAY_NMM_PRT("***** Unhooking gCallProcHook!\n");
    if (!::UnhookWindowsHookEx(gCallProcHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for gCallProcHook!\n");
    }
    gCallProcHook = NULL;
  }

  if (gMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Unhooking gMsgFilterHook!\n");
    if (!::UnhookWindowsHookEx(gMsgFilterHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for gMsgFilterHook!\n");
    }
    gMsgFilterHook = NULL;
  }

  if (gCallMouseHook) {
    DISPLAY_NMM_PRT("***** Unhooking gCallMouseHook!\n");
    if (!::UnhookWindowsHookEx(gCallMouseHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for gCallMouseHook!\n");
    }
    gCallMouseHook = NULL;
  }
}

//-------------------------------------------------------------------------
// This timer is designed to only fire one time at most each time a "hook" function
// is used to rollup the dropdown
// In some cases, the timer may be scheduled from the hook, but that hook event or 
// a subsequent event may roll up the dropdown before this timer function is executed.
//
// For example, if an MFC control takes focus, the combobox will lose focus and rollup
// before this function fires.
//
VOID CALLBACK nsWindow::HookTimerForPopups( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime ) 
{
  if (gHookTimerId != 0) {
    // if the window is NULL then we need to use the ID to kill the timer
    BOOL status = ::KillTimer(NULL, gHookTimerId);
    NS_ASSERTION(status, "Hook Timer was not killed.");
    gHookTimerId = 0;
  }

  if (gRollupMsgId != 0) {
    // Note: DealWithPopups does the check to make sure that 
    // gRollupListener and gRollupWidget are not NULL
    LRESULT popupHandlingResult;
    DealWithPopups(gRollupMsgId, 0, 0, &popupHandlingResult);
    gRollupMsgId = 0;
  }

}


#ifdef ACCESSIBILITY
void nsWindow::CreateRootAccessible()
{
  // Create this as early as possible in new window, if accessibility is turned on
  // We need it to be created early so it can generate accessibility events right away
  nsCOMPtr<nsIAccessible> acc;
  DispatchAccessibleEvent(NS_GETACCESSIBLE, getter_AddRefs(acc));
  // create the COM accessible object
  if (acc) {
     HWND wnd = GetWindowHandle();
     mRootAccessible = new RootAccessible(acc, wnd); // ref is 0       
     mRootAccessible->AddRef();
  }
}
#endif
