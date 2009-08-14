/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
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
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Ere Maijala <emaijala@kolumbus.fi>
 *   Mark Hammond <markh@activestate.com>
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Peter Bajusz <hyp-x@inf.bme.hu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Roy Yokoyama <yokoyama@netscape.com>
 *   Makoto Kato  <m_kato@ga2.so-net.ne.jp>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Dainis Jonitis <Dainis_Jonitis@swh-t.lv>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Ningjie Chen <chenn@email.uc.edu>
 *   Jim Mathies <jmathies@mozilla.com>
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

/*
 * nsWindow - Native window management and event handling.
 * 
 * nsWindow is organized into a set of major blocks and
 * block subsections. The layout is as follows:
 *
 *  Includes
 *  Variables
 *  nsIWidget impl.
 *     nsIWidget methods and utilities
 *  nsSwitchToUIThread impl.
 *     nsSwitchToUIThread methods and utilities
 *  Moz events
 *     Event initialization
 *     Event dispatching
 *  Native events
 *     Wndproc(s)
 *     Event processing
 *     OnEvent event handlers
 *  IME management and accessibility
 *  Transparency
 *  Popup hook handling
 *  Misc. utilities
 *  Child window impl.
 *
 * Search for "BLOCK:" to find major blocks.
 * Search for "SECTION:" to find specific sections.
 *
 * Blocks should be split out into separate files if they
 * become unmanageable.
 *
 * Related source:
 *
 *  nsWindowDefs.h     - Definitions, macros, structs, enums
 *                       and general setup.
 *  nsWindowDbg.h/.cpp - Debug related code and directives.
 *  nsWindowGfx.h/.cpp - Graphics and painting.
 *  nsWindowCE.h/.cpp  - WINCE specific code that can be
 *                       split out from nsWindow.
 *
 */

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Includes
 **
 ** Include headers.
 **
 **************************************************************
 **************************************************************/

#include "nsWindow.h"

#include <windows.h>
#include <process.h>
#include <commctrl.h>
#include <unknwn.h>

#include "prlog.h"
#include "prtime.h"
#include "prprf.h"
#include "prmem.h"

#include "nsIAppShell.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMNSUIEvent.h"
#include "nsITheme.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIScreenManager.h"
#include "imgIContainer.h"
#include "nsIFile.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIRegion.h"
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsIMM32Handler.h"
#include "nsILocalFile.h"
#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsIDeviceContext.h"

#include "nsGUIEvent.h"
#include "nsFont.h"
#include "nsRect.h"
#include "nsThreadUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsWidgetAtoms.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsWidgetsCID.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

#if defined(WINCE)
#include "nsWindowCE.h"
#endif

#include "nsWindowGfx.h"

#if !defined(WINCE)
#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#include "nsKeyboardLayout.h"
#include "nsNativeDragTarget.h"
#include <mmsystem.h> // needed for WIN32_LEAN_AND_MEAN
#include <zmouse.h>
#include <pbt.h>
#include <richedit.h>
#endif // !defined(WINCE)

#if defined(ACCESSIBILITY)
#include "oleidl.h"
#include <winuser.h>
#if !defined(WINABLEAPI)
#include <winable.h>
#endif // !defined(WINABLEAPI)
#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessNode.h"
#endif // defined(ACCESSIBILITY)

#if defined(NS_ENABLE_TSF)
#include "nsTextStore.h"
#endif // defined(NS_ENABLE_TSF)

#if defined(MOZ_SPLASHSCREEN)
#include "nsSplashScreen.h"
#endif // defined(MOZ_SPLASHSCREEN)

// Windowless plugin support
#include "nsplugindefs.h"

#include "nsWindowDefs.h"

// For scroll wheel calculations
#include "nsITimer.h"

/**************************************************************
 *
 * nsScrollPrefObserver Class for scroll acceleration prefs
 *
 **************************************************************/

class nsScrollPrefObserver : public nsIObserver
{
public:
  nsScrollPrefObserver();
  int GetScrollAccelerationStart();
  int GetScrollAccelerationFactor();
  int GetScrollNumLines();
  void RemoveObservers();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:
  nsCOMPtr<nsIPrefBranch2> mPrefBranch;
  int mScrollAccelerationStart;
  int mScrollAccelerationFactor;
  int mScrollNumLines;
};

NS_IMPL_ISUPPORTS1(nsScrollPrefObserver, nsScrollPrefObserver)

nsScrollPrefObserver::nsScrollPrefObserver()
{
  nsresult rv;
  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);

  rv = mPrefBranch->GetIntPref("mousewheel.acceleration.start",
                               &mScrollAccelerationStart);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), 
                    "Failed to get pref: mousewheel.acceleration.start");
  rv = mPrefBranch->AddObserver("mousewheel.acceleration.start", 
                                this, PR_FALSE);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), 
                    "Failed to add pref observer: mousewheel.acceleration.start");
                    
  rv = mPrefBranch->GetIntPref("mousewheel.acceleration.factor",
                               &mScrollAccelerationFactor);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), 
                    "Failed to get pref: mousewheel.acceleration.factor");
  rv = mPrefBranch->AddObserver("mousewheel.acceleration.factor", 
                                this, PR_FALSE);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), 
                    "Failed to add pref observer: mousewheel.acceleration.factor");
                    
  rv = mPrefBranch->GetIntPref("mousewheel.withnokey.numlines",
                               &mScrollNumLines);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), 
                    "Failed to get pref: mousewheel.withnokey.numlines");
  rv = mPrefBranch->AddObserver("mousewheel.withnokey.numlines", 
                                this, PR_FALSE);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), 
                    "Failed to add pref observer: mousewheel.withnokey.numlines");
}

int nsScrollPrefObserver::GetScrollAccelerationStart()
{
  return mScrollAccelerationStart;
}

int nsScrollPrefObserver::GetScrollAccelerationFactor()
{
  return mScrollAccelerationFactor;
}

int nsScrollPrefObserver::GetScrollNumLines()
{
  return mScrollNumLines;
}

void nsScrollPrefObserver::RemoveObservers()
{
  mPrefBranch->RemoveObserver("mousewheel.acceleration.start", this);
  mPrefBranch->RemoveObserver("mousewheel.acceleration.factor", this);
  mPrefBranch->RemoveObserver("mousewheel.withnokey.numlines", this);
}

NS_IMETHODIMP nsScrollPrefObserver::Observe(nsISupports *aSubject,
                                            const char *aTopic,
                                            const PRUnichar *aData)
{
  mPrefBranch->GetIntPref("mousewheel.acceleration.start",
                          &mScrollAccelerationStart);
  mPrefBranch->GetIntPref("mousewheel.acceleration.factor",
                          &mScrollAccelerationFactor);
  mPrefBranch->GetIntPref("mousewheel.withnokey.numlines",
                          &mScrollNumLines);

  return NS_OK;
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Variables
 **
 ** nsWindow Class static initializations and global variables. 
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: nsWindow statics
 *
 **************************************************************/

PRUint32        nsWindow::sInstanceCount          = 0;
PRBool          nsWindow::sSwitchKeyboardLayout   = PR_FALSE;
BOOL            nsWindow::sIsRegistered           = FALSE;
BOOL            nsWindow::sIsPopupClassRegistered = FALSE;
BOOL            nsWindow::sIsOleInitialized       = FALSE;
HCURSOR         nsWindow::sHCursor                = NULL;
imgIContainer*  nsWindow::sCursorImgContainer     = nsnull;
nsWindow*       nsWindow::sCurrentWindow          = nsnull;
PRBool          nsWindow::sJustGotDeactivate      = PR_FALSE;
PRBool          nsWindow::sJustGotActivate        = PR_FALSE;

// imported in nsWidgetFactory.cpp
TriStateBool    nsWindow::sCanQuit                = TRI_UNKNOWN;

// Hook Data Memebers for Dropdowns. sProcessHook Tells the
// hook methods whether they should be processing the hook
// messages.
HHOOK           nsWindow::sMsgFilterHook          = NULL;
HHOOK           nsWindow::sCallProcHook           = NULL;
HHOOK           nsWindow::sCallMouseHook          = NULL;
PRPackedBool    nsWindow::sProcessHook            = PR_FALSE;
UINT            nsWindow::sRollupMsgId            = 0;
HWND            nsWindow::sRollupMsgWnd           = NULL;
UINT            nsWindow::sHookTimerId            = 0;

// Rollup Listener
nsIRollupListener* nsWindow::sRollupListener      = nsnull;
nsIWidget*      nsWindow::sRollupWidget           = nsnull;
PRBool          nsWindow::sRollupConsumeEvent     = PR_FALSE;

// Mouse Clicks - static variable definitions for figuring
// out 1 - 3 Clicks.
POINT           nsWindow::sLastMousePoint         = {0};
POINT           nsWindow::sLastMouseMovePoint     = {0};
LONG            nsWindow::sLastMouseDownTime      = 0L;
LONG            nsWindow::sLastClickCount         = 0L;
BYTE            nsWindow::sLastMouseButton        = 0;

// Trim heap on minimize. (initialized, but still true.)
int             nsWindow::sTrimOnMinimize         = 2;

#ifdef ACCESSIBILITY
BOOL            nsWindow::sIsAccessibilityOn      = FALSE;
// Accessibility wm_getobject handler
HINSTANCE       nsWindow::sAccLib                 = 0;
LPFNLRESULTFROMOBJECT 
                nsWindow::sLresultFromObject      = 0;
#endif // ACCESSIBILITY

/**************************************************************
 *
 * SECTION: globals variables
 *
 **************************************************************/

static const char *sScreenManagerContractID       = "@mozilla.org/gfx/screenmanager;1";

#ifdef PR_LOGGING
PRLogModuleInfo* gWindowsLog                      = nsnull;
#endif

#ifndef WINCE
// Kbd layout. Used throughout character processing.
static KeyboardLayout gKbdLayout;
#endif

// The last user input event time in microseconds. If
// there are any pending native toolkit input events
// it returns the current time. The value is compatible
// with PR_IntervalToMicroseconds(PR_IntervalNow()).
#if !defined(WINCE)
static PRUint32 gLastInputEventTime               = 0;
#else
PRUint32        gLastInputEventTime               = 0;
#endif

// Global user preference for disabling native theme. Used
// in NativeWindowTheme.
PRBool          gDisableNativeTheme               = PR_FALSE;

// Global used in Show window enumerations.
static PRBool   gWindowsVisible                   = PR_FALSE;

static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

// Global scroll pref observer for scroll acceleration prefs
static nsScrollPrefObserver* gScrollPrefObserver  = nsnull;

/**************************************************************
 **************************************************************
 **
 ** BLOCK: nsIWidget impl.
 **
 ** nsIWidget interface implementation, broken down into
 ** sections.
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: nsWindow construction and destruction
 *
 **************************************************************/

nsWindow::nsWindow() : nsBaseWidget()
{
#ifdef PR_LOGGING
  if (!gWindowsLog)
    gWindowsLog = PR_NewLogModule("nsWindowsWidgets");
#endif

  mWnd                  = nsnull;
  mPaintDC              = nsnull;
  mPrevWndProc          = nsnull;
  mOldIMC               = nsnull;
  mNativeDragTarget     = nsnull;
  mInDtor               = PR_FALSE;
  mIsVisible            = PR_FALSE;
  mHas3DBorder          = PR_FALSE;
  mIsInMouseCapture     = PR_FALSE;
  mIsPluginWindow       = PR_FALSE;
  mIsTopWidgetWindow    = PR_FALSE;
  mInWheelProcessing    = PR_FALSE;
  mUnicodeWidget        = PR_TRUE;
  mWindowType           = eWindowType_child;
  mBorderStyle          = eBorderStyle_default;
  mPopupType            = ePopupTypeAny;
  mDisplayPanFeedback   = PR_FALSE;
  mLastPoint.x          = 0;
  mLastPoint.y          = 0;
  mLastSize.width       = 0;
  mLastSize.height      = 0;
  mOldStyle             = 0;
  mOldExStyle           = 0;
  mPainting             = 0;
  mLastKeyboardLayout   = 0;
  mBlurSuppressLevel    = 0;
  mIMEEnabled           = nsIWidget::IME_STATUS_ENABLED;
  mLeadByte             = '\0';
#ifdef MOZ_XUL
  mTransparentSurface   = nsnull;
  mMemoryDC             = nsnull;
  mTransparencyMode     = eTransparencyOpaque;
#endif
  mBackground           = ::GetSysColor(COLOR_BTNFACE);
  mBrush                = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
  mForeground           = ::GetSysColor(COLOR_WINDOWTEXT);

  // To be used for scroll acceleration
  mScrollSeriesCounter = 0;

  // Global initialization
  if (!sInstanceCount) {
#if !defined(WINCE)
  gKbdLayout.LoadLayout(::GetKeyboardLayout(0));
#endif

  // Init IME handler
  nsIMM32Handler::Initialize();

  // Init scroll pref observer for scroll acceleration
  NS_IF_ADDREF(gScrollPrefObserver = new nsScrollPrefObserver());

#ifdef NS_ENABLE_TSF
  nsTextStore::Initialize();
#endif

#if !defined(WINCE)
  if (SUCCEEDED(::OleInitialize(NULL)))
    sIsOleInitialized = TRUE;
  NS_ASSERTION(sIsOleInitialized, "***** OLE is not initialized!\n");
#endif

#if defined(HEAP_DUMP_EVENT)
  InitHeapDump();
#endif
  } // !sInstanceCount

  // Set gLastInputEventTime to some valid number
  gLastInputEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());

  sInstanceCount++;
}

nsWindow::~nsWindow()
{
  mInDtor = PR_TRUE;

  // If the widget was released without calling Destroy() then the native window still
  // exists, and we need to destroy it. This will also result in a call to OnDestroy.
  //
  // XXX How could this happen???
  if (NULL != mWnd)
    Destroy();

  sInstanceCount--;

  // Global shutdown
  if (sInstanceCount == 0) {
#ifdef NS_ENABLE_TSF
    nsTextStore::Terminate();
#endif

#if !defined(WINCE)
    NS_IF_RELEASE(sCursorImgContainer);
    if (sIsOleInitialized) {
      ::OleFlushClipboard();
      ::OleUninitialize();
      sIsOleInitialized = FALSE;
    }
    // delete any of the IME structures that we allocated
    nsIMM32Handler::Terminate();
#endif // !defined(WINCE)

    gScrollPrefObserver->RemoveObservers();
    NS_RELEASE(gScrollPrefObserver);
  }

#if !defined(WINCE)
  NS_IF_RELEASE(mNativeDragTarget);
#endif // !defined(WINCE)
}

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

/**************************************************************
 *
 * SECTION: nsIWidget::Create, nsIWidget::Destroy
 *
 * Creating and destroying windows for this widget.
 *
 **************************************************************/

// Create the proper widget
NS_METHOD nsWindow::Create(nsIWidget *aParent,
                           const nsIntRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  if (aInitData)
    mUnicodeWidget = aInitData->mUnicode;
  return(StandardWindowCreate(aParent, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              nsnull));
}


// Create with a native parent
NS_METHOD nsWindow::Create(nsNativeWidget aParent,
                           const nsIntRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  if (aInitData)
    mUnicodeWidget = aInitData->mUnicode;
  return(StandardWindowCreate(nsnull, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              aParent));
}

// Allow Derived classes to modify the height that is passed
// when the window is created or resized. Also add extra height
// if needed (on Windows CE)
PRInt32 nsWindow::GetHeight(PRInt32 aProposedHeight)
{
  PRInt32 extra = 0;

  #if defined(WINCE) && !defined(WINCE_WINDOWS_MOBILE)
  DWORD style = WindowStyle();
  if ((style & WS_SYSMENU) && (style & WS_POPUP)) {
    extra = GetSystemMetrics(SM_CYCAPTION);
  }
  #endif

  return aProposedHeight + extra;
}

// Utility methods for creating windows.
nsresult
nsWindow::StandardWindowCreate(nsIWidget *aParent,
                               const nsIntRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData,
                               nsNativeWidget aNativeParent)
{
  nsIWidget *baseParent = aInitData &&
                         (aInitData->mWindowType == eWindowType_dialog ||
                          aInitData->mWindowType == eWindowType_toplevel ||
                          aInitData->mWindowType == eWindowType_invisible) ?
                         nsnull : aParent;

  mIsTopWidgetWindow = (nsnull == baseParent);
  mBounds.width = aRect.width;
  mBounds.height = aRect.height;

  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

  HWND parent;
  if (nsnull != aParent) { // has a nsIWidget parent
    parent = ((aParent) ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
  } else { // has a nsNative parent
    parent = (HWND)aNativeParent;
  }

  if (nsnull != aInitData) {
    SetWindowType(aInitData->mWindowType);
    SetBorderStyle(aInitData->mBorderStyle);
    mPopupType = aInitData->mPopupHint;
  }

  mContentType = aInitData ? aInitData->mContentType : eContentTypeInherit;

  DWORD style = WindowStyle();
  DWORD extendedStyle = WindowExStyle();

  if (mWindowType == eWindowType_popup) {
    // if a parent was specified, don't use WS_EX_TOPMOST so that the popup
    // only appears above the parent, instead of all windows
    if (aParent)
      extendedStyle = WS_EX_TOOLWINDOW;
    else
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

  mWnd = ::CreateWindowExW(extendedStyle,
                           aInitData && aInitData->mDropShadow ?
                           WindowPopupClass() : WindowClass(),
                           L"",
                           style,
                           aRect.x,
                           aRect.y,
                           aRect.width,
                           GetHeight(aRect.height),
                           parent,
                           NULL,
                           nsToolkit::mDllInstance,
                           NULL);

  if (!mWnd)
    return NS_ERROR_FAILURE;

  // call the event callback to notify about creation

  DispatchStandardEvent(NS_CREATE);
  SubclassWindow(TRUE);

  if (sTrimOnMinimize == 2 && mWindowType == eWindowType_invisible) {
    /* The internal variable set by the config.trim_on_minimize pref
       has not yet been initialized, and this is the hidden window
       (conveniently created before any visible windows, and after
       the profile has been initialized).
       
       Default config.trim_on_minimize to false, to fix bug 76831
       for good.  If anyone complains about this new default, saying
       that a Mozilla app hogs too much memory while minimized, they
       will have that entire bug tattooed on their backside. */

    sTrimOnMinimize = 0;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      nsCOMPtr<nsIPrefBranch> prefBranch;
      prefs->GetBranch(0, getter_AddRefs(prefBranch));
      if (prefBranch) {

        PRBool temp;
        if (NS_SUCCEEDED(prefBranch->GetBoolPref("config.trim_on_minimize",
                                                 &temp))
            && temp)
          sTrimOnMinimize = 1;

        if (NS_SUCCEEDED(prefBranch->GetBoolPref("intl.keyboard.per_window_layout",
                                                 &temp)))
          sSwitchKeyboardLayout = temp;

        if (NS_SUCCEEDED(prefBranch->GetBoolPref("mozilla.widget.disable-native-theme",
                                                 &temp)))
          gDisableNativeTheme = temp;
      }
    }
  }
#if defined(WINCE_HAVE_SOFTKB)
  if (mWindowType == eWindowType_dialog || mWindowType == eWindowType_toplevel )
     nsWindowCE::CreateSoftKeyMenuBar(mWnd);
#endif

  return NS_OK;
}

// Close this nsWindow
NS_METHOD nsWindow::Destroy()
{
  // WM_DESTROY has already fired, we're done.
  if (nsnull == mWnd)
    return NS_OK;

  // During the destruction of all of our children, make sure we don't get deleted.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  // The DestroyWindow function destroys the specified window. The function sends WM_DESTROY
  // and WM_NCDESTROY messages to the window to deactivate it and remove the keyboard focus
  // from it. The function also destroys the window's menu, flushes the thread message queue,
  // destroys timers, removes clipboard ownership, and breaks the clipboard viewer chain (if
  // the window is at the top of the viewer chain).
  //
  // If the specified window is a parent or owner window, DestroyWindow automatically destroys
  // the associated child or owned windows when it destroys the parent or owner window. The
  // function first destroys child or owned windows, and then it destroys the parent or owner
  // window.
  VERIFY(::DestroyWindow(mWnd));
  
  // Our windows can be subclassed which may prevent us receiving WM_DESTROY. If OnDestroy()
  // didn't get called, call it now.
  if (PR_FALSE == mOnDestroyCalled)
    OnDestroy();

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: Window class utilities
 *
 * Utilities for calculating the proper window class name for
 * Create window.
 *
 **************************************************************/

// Return the proper window class for everything except popups.
LPCWSTR nsWindow::WindowClass()
{
  if (!nsWindow::sIsRegistered) {
    WNDCLASSW wc;

//    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = ::DefWindowProcW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = nsToolkit::mDllInstance;
    wc.hIcon         = ::LoadIconW(::GetModuleHandleW(NULL), (LPWSTR)IDI_APPLICATION);
    wc.hCursor       = NULL;
    wc.hbrBackground = mBrush;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = kClassNameHidden;

    BOOL succeeded = ::RegisterClassW(&wc) != 0 && 
      ERROR_CLASS_ALREADY_EXISTS != GetLastError();
    nsWindow::sIsRegistered = succeeded;

    wc.lpszClassName = kClassNameContentFrame;
    if (!::RegisterClassW(&wc) && 
      ERROR_CLASS_ALREADY_EXISTS != GetLastError()) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kClassNameContent;
    if (!::RegisterClassW(&wc) && 
      ERROR_CLASS_ALREADY_EXISTS != GetLastError()) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kClassNameUI;
    if (!::RegisterClassW(&wc) && 
      ERROR_CLASS_ALREADY_EXISTS != GetLastError()) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kClassNameGeneral;
    ATOM generalClassAtom = ::RegisterClassW(&wc);
    if (!generalClassAtom && 
      ERROR_CLASS_ALREADY_EXISTS != GetLastError()) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kClassNameDialog;
    wc.hIcon = 0;
    if (!::RegisterClassW(&wc) && 
      ERROR_CLASS_ALREADY_EXISTS != GetLastError()) {
      nsWindow::sIsRegistered = FALSE;
    }
  }

  if (mWindowType == eWindowType_invisible) {
    return kClassNameHidden;
  }
  if (mWindowType == eWindowType_dialog) {
    return kClassNameDialog;
  }
  if (mContentType == eContentTypeContent) {
    return kClassNameContent;
  }
  if (mContentType == eContentTypeContentFrame) {
    return kClassNameContentFrame;
  }
  if (mContentType == eContentTypeUI) {
    return kClassNameUI;
  }
  return kClassNameGeneral;
}

// Return the proper popup window class
LPCWSTR nsWindow::WindowPopupClass()
{
  if (!nsWindow::sIsPopupClassRegistered) {
    WNDCLASSW wc;

    wc.style = CS_DBLCLKS | CS_XP_DROPSHADOW;
    wc.lpfnWndProc   = ::DefWindowProcW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = nsToolkit::mDllInstance;
    wc.hIcon         = ::LoadIconW(::GetModuleHandleW(NULL), (LPWSTR)IDI_APPLICATION);
    wc.hCursor       = NULL;
    wc.hbrBackground = mBrush;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = kClassNameDropShadow;

    nsWindow::sIsPopupClassRegistered = ::RegisterClassW(&wc);
    if (!nsWindow::sIsPopupClassRegistered) {
      // For older versions of Win32 (i.e., not XP), the registration will
      // fail, so we have to re-register without the CS_XP_DROPSHADOW flag.
      wc.style = CS_DBLCLKS;
      nsWindow::sIsPopupClassRegistered = ::RegisterClassW(&wc);
    }
  }

  return kClassNameDropShadow;
}

/**************************************************************
 *
 * SECTION: Window styles utilities
 *
 * Return the proper windows styles and extended styles.
 *
 **************************************************************/

// Return nsWindow styles
#if !defined(WINCE) // implemented in nsWindowCE.cpp
DWORD nsWindow::WindowStyle()
{
  DWORD style;

  switch (mWindowType) {
    case eWindowType_child:
      style = WS_OVERLAPPED;
      break;

    case eWindowType_dialog:
      style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU | DS_3DLOOK | DS_MODALFRAME;
      if (mBorderStyle != eBorderStyle_default)
        style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      break;

    case eWindowType_popup:
      style = WS_POPUP;
      if (mTransparencyMode == eTransparencyGlass) {
        /* Glass seems to need WS_CAPTION or WS_THICKFRAME to work.
           WS_THICKFRAME has issues with autohiding popups but looks better */
        style |= WS_THICKFRAME;
      } else {
        style |= WS_OVERLAPPED;
      }
      break;

    default:
      NS_ASSERTION(0, "unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
              WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      break;
  }

  VERIFY_WINDOW_STYLE(style);
  return style;
}
#endif // !defined(WINCE)

// Return nsWindow extended styles
DWORD nsWindow::WindowExStyle()
{
  switch (mWindowType)
  {
    case eWindowType_child:
      return 0;

    case eWindowType_dialog:
      return WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME;

    case eWindowType_popup:
      return
#if defined(WINCE) && !defined(WINCE_WINDOWS_MOBILE)
        WS_EX_NOACTIVATE |
#endif
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    default:
      NS_ASSERTION(0, "unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      return WS_EX_WINDOWEDGE;
  }
}

/**************************************************************
 *
 * SECTION: Window subclassing utilities
 *
 * Set or clear window subclasses on native windows. Used in
 * Create and Destroy.
 *
 **************************************************************/

// Subclass (or remove the subclass from) this component's nsWindow
void nsWindow::SubclassWindow(BOOL bState)
{
  if (NULL != mWnd) {
    NS_PRECONDITION(::IsWindow(mWnd), "Invalid window handle");

    if (bState) {
      // change the nsWindow proc
      if (mUnicodeWidget)
        mPrevWndProc = (WNDPROC)::SetWindowLongPtrW(mWnd, GWLP_WNDPROC,
                                                (LONG_PTR)nsWindow::WindowProc);
      else
        mPrevWndProc = (WNDPROC)::SetWindowLongPtrA(mWnd, GWLP_WNDPROC,
                                                (LONG_PTR)nsWindow::WindowProc);
      NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
      // connect the this pointer to the nsWindow handle
      SetNSWindowPtr(mWnd, this);
    }
    else {
      if (mUnicodeWidget)
        ::SetWindowLongPtrW(mWnd, GWLP_WNDPROC, (LONG_PTR)mPrevWndProc);
      else
        ::SetWindowLongPtrA(mWnd, GWLP_WNDPROC, (LONG_PTR)mPrevWndProc);
      SetNSWindowPtr(mWnd, NULL);
      mPrevWndProc = NULL;
    }
  }
}

/**************************************************************
 *
 * SECTION: Window properties
 *
 * Set and clear native window properties.
 *
 **************************************************************/

static PRUnichar sPropName[40] = L"";
static PRUnichar* GetNSWindowPropName()
{
  if (!*sPropName)
  {
    _snwprintf(sPropName, 39, L"MozillansIWidgetPtr%p", GetCurrentProcessId());
    sPropName[39] = '\0';
  }
  return sPropName;
}

nsWindow * nsWindow::GetNSWindowPtr(HWND aWnd)
{
  return (nsWindow *) ::GetPropW(aWnd, GetNSWindowPropName());
}

BOOL nsWindow::SetNSWindowPtr(HWND aWnd, nsWindow * ptr)
{
  if (ptr == NULL) {
    ::RemovePropW(aWnd, GetNSWindowPropName());
    return TRUE;
  } else {
    return ::SetPropW(aWnd, GetNSWindowPropName(), (HANDLE)ptr);
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetParent, nsIWidget::GetParent
 *
 * Set or clear the parent widgets using window properties, and
 * handles calculating native parent handles.
 *
 **************************************************************/

// Get and set parent widgets
NS_IMETHODIMP nsWindow::SetParent(nsIWidget *aNewParent)
{
  if (aNewParent) {
    nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

    nsIWidget* parent = GetParent();
    if (parent) {
      parent->RemoveChild(this);
    }

    HWND newParent = (HWND)aNewParent->GetNativeData(NS_NATIVE_WINDOW);
    NS_ASSERTION(newParent, "Parent widget has a null native window handle");
    if (newParent && mWnd) {
      ::SetParent(mWnd, newParent);
    }

    aNewParent->AddChild(this);

    return NS_OK;
  }

  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  nsIWidget* parent = GetParent();

  if (parent) {
    parent->RemoveChild(this);
  }

  if (mWnd) {
    // If we have no parent, SetParent should return the desktop.
    VERIFY(::SetParent(mWnd, nsnull));
  }

  return NS_OK;
}

nsIWidget* nsWindow::GetParent(void)
{
  return GetParentWindow(PR_FALSE);
}

nsWindow* nsWindow::GetParentWindow(PRBool aIncludeOwner)
{
  if (mIsTopWidgetWindow) {
    // Must use a flag instead of mWindowType to tell if the window is the
    // owned by the topmost widget, because a child window can be embedded inside
    // a HWND which is not associated with a nsIWidget.
    return nsnull;
  }

  // If this widget has already been destroyed, pretend we have no parent.
  // This corresponds to code in Destroy which removes the destroyed
  // widget from its parent's child list.
  if (mInDtor || mOnDestroyCalled)
    return nsnull;


  // aIncludeOwner set to true implies walking the parent chain to retrieve the
  // root owner. aIncludeOwner set to false implies the search will stop at the
  // true parent (default).
  nsWindow* widget = nsnull;
  if (mWnd) {
#ifdef WINCE
    HWND parent = ::GetParent(mWnd);
#else
    HWND parent = nsnull;
    if (aIncludeOwner)
      parent = ::GetParent(mWnd);
    else
      parent = ::GetAncestor(mWnd, GA_PARENT);
#endif
    if (parent) {
      widget = GetNSWindowPtr(parent);
      if (widget) {
        // If the widget is in the process of being destroyed then
        // do NOT return it
        if (widget->mInDtor) {
          widget = nsnull;
        }
      }
    }
  }

  return widget;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Show
 *
 * Hide or show this component.
 *
 **************************************************************/

NS_METHOD nsWindow::Show(PRBool bState)
{
#if defined(MOZ_SPLASHSCREEN)
  // we're about to show the first toplevel window,
  // so kill off any splash screen if we had one
  nsSplashScreen *splash = nsSplashScreen::Get();
  if (splash && splash->IsOpen() && mWnd && bState &&
      (mWindowType == eWindowType_toplevel ||
       mWindowType == eWindowType_dialog ||
       mWindowType == eWindowType_popup))
  {
    splash->Close();
  }
#endif

  PRBool wasVisible = mIsVisible;
  // Set the status now so that anyone asking during ShowWindow or
  // SetWindowPos would get the correct answer.
  mIsVisible = bState;

  if (mWnd) {
    if (bState) {
      if (!wasVisible && mWindowType == eWindowType_toplevel) {
        switch (mSizeMode) {
#ifdef WINCE
          case nsSizeMode_Fullscreen:
            ::SetForegroundWindow(mWnd);
            ::ShowWindow(mWnd, SW_SHOWMAXIMIZED);
            MakeFullScreen(TRUE);
            break;

          case nsSizeMode_Maximized :
            ::SetForegroundWindow(mWnd);
            ::ShowWindow(mWnd, SW_SHOWMAXIMIZED);
            break;
          // use default for nsSizeMode_Minimized on Windows CE
#else
          case nsSizeMode_Fullscreen:
            ::ShowWindow(mWnd, SW_SHOWMAXIMIZED);
            break;

          case nsSizeMode_Maximized :
            ::ShowWindow(mWnd, SW_SHOWMAXIMIZED);
            break;
          case nsSizeMode_Minimized :
            ::ShowWindow(mWnd, SW_SHOWMINIMIZED);
            break;
#endif
          default:
            if (CanTakeFocus()) {
#ifdef WINCE
              ::SetForegroundWindow(mWnd);
#endif
              ::ShowWindow(mWnd, SW_SHOWNORMAL);
            } else {
              // Place the window behind the foreground window
              // (as long as it is not topmost)
              HWND wndAfter = ::GetForegroundWindow();
              if (!wndAfter)
                wndAfter = HWND_BOTTOM;
              else if (GetWindowLongPtrW(wndAfter, GWL_EXSTYLE) & WS_EX_TOPMOST)
                wndAfter = HWND_TOP;
              ::SetWindowPos(mWnd, wndAfter, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | 
                             SWP_NOMOVE | SWP_NOACTIVATE);
              GetAttention(2);
            }
            break;
        }
      } else {
        DWORD flags = SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW;
        if (wasVisible)
          flags |= SWP_NOZORDER;

        if (mWindowType == eWindowType_popup) {
#ifndef WINCE
          // ensure popups are the topmost of the TOPMOST
          // layer. Remember not to set the SWP_NOZORDER
          // flag as that might allow the taskbar to overlap
          // the popup.  However on windows ce, we need to
          // activate the popup or clicks will not be sent.
          flags |= SWP_NOACTIVATE;
#endif
          HWND owner = ::GetWindow(mWnd, GW_OWNER);
          ::SetWindowPos(mWnd, owner ? 0 : HWND_TOPMOST, 0, 0, 0, 0, flags);
        } else {
#ifndef WINCE
          if (mWindowType == eWindowType_dialog && !CanTakeFocus())
            flags |= SWP_NOACTIVATE;
#endif
          ::SetWindowPos(mWnd, HWND_TOP, 0, 0, 0, 0, flags);
        }
      }
    } else {
      if (mWindowType != eWindowType_dialog) {
        ::ShowWindow(mWnd, SW_HIDE);
      } else {
        ::SetWindowPos(mWnd, 0, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE |
                       SWP_NOZORDER | SWP_NOACTIVATE);
      }
    }
  }
  
#ifdef MOZ_XUL
  if (!wasVisible && bState)
    Invalidate(PR_FALSE);
#endif

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::IsVisible
 *
 * Returns the visibility state.
 *
 **************************************************************/

// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
  bState = mIsVisible;
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: Window clipping utilities
 *
 * Used in Size and Move operations for setting the proper
 * window clipping regions for window transparency.
 *
 **************************************************************/

// XP and Vista visual styles sometimes require window clipping regions to be applied for proper
// transparency. These routines are called on size and move operations.
void nsWindow::ClearThemeRegion()
{
#ifndef WINCE
  if (nsUXThemeData::sIsVistaOrLater && mTransparencyMode != eTransparencyGlass &&
      mWindowType == eWindowType_popup && (mPopupType == ePopupTypeTooltip || mPopupType == ePopupTypePanel)) {
    SetWindowRgn(mWnd, NULL, false);
  }
#endif
}

void nsWindow::SetThemeRegion()
{
#ifndef WINCE
  // Popup types that have a visual styles region applied (bug 376408). This can be expanded
  // for other window types as needed. The regions are applied generically to the base window
  // so default constants are used for part and state. At some point we might need part and
  // state values from nsNativeThemeWin's GetThemePartAndState, but currently windows that
  // change shape based on state haven't come up.
  if (nsUXThemeData::sIsVistaOrLater && mTransparencyMode != eTransparencyGlass &&
      mWindowType == eWindowType_popup && (mPopupType == ePopupTypeTooltip || mPopupType == ePopupTypePanel)) {
    HRGN hRgn = nsnull;
    RECT rect = {0,0,mBounds.width,mBounds.height};
    
    nsUXThemeData::getThemeBackgroundRegion(nsUXThemeData::GetTheme(eUXTooltip), GetDC(mWnd), TTP_STANDARD, TS_NORMAL, &rect, &hRgn);
    if (hRgn) {
      if (!SetWindowRgn(mWnd, hRgn, false)) // do not delete or alter hRgn if accepted.
        DeleteObject(hRgn);
    }
  }
#endif
}

/**************************************************************
 *
 * SECTION: nsIWidget::Move, nsIWidget::Resize, nsIWidget::Size
 *
 * Repositioning and sizing a window.
 *
 **************************************************************/

// Move this component
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  // Check to see if window needs to be moved first
  // to avoid a costly call to SetWindowPos. This check
  // can not be moved to the calling code in nsView, because
  // some platforms do not position child windows correctly

  // Only perform this check for non-popup windows, since the positioning can
  // in fact change even when the x/y do not.  We always need to perform the
  // check. See bug #97805 for details.
  if (mWindowType != eWindowType_popup && (mBounds.x == aX) && (mBounds.y == aY))
  {
    // Nothing to do, since it is already positioned correctly.
    return NS_OK;
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
      if (dc) {
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
    ClearThemeRegion();
    VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, 0, 0,
                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
    SetThemeRegion();
  }
  return NS_OK;
}

// Resize this component
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_ASSERTION((aWidth >=0 ) , "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((aHeight >=0 ), "Negative height passed to nsWindow::Resize");

#ifdef MOZ_XUL
  if (eTransparencyTransparent == mTransparencyMode)
    ResizeTranslucentWindow(aWidth, aHeight);
#endif

  // Set cached value for lightweight and printing
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if (mWnd) {
    UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE;

#ifndef WINCE
    if (!aRepaint) {
      flags |= SWP_NOREDRAW;
    }
#endif

    ClearThemeRegion();
    VERIFY(::SetWindowPos(mWnd, NULL, 0, 0, aWidth, GetHeight(aHeight), flags));
    SetThemeRegion();
  }

  if (aRepaint)
    Invalidate(PR_FALSE);

  return NS_OK;
}

// Resize this component
NS_METHOD nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_ASSERTION((aWidth >=0 ),  "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((aHeight >=0 ), "Negative height passed to nsWindow::Resize");

#ifdef MOZ_XUL
  if (eTransparencyTransparent == mTransparencyMode)
    ResizeTranslucentWindow(aWidth, aHeight);
#endif

  // Set cached value for lightweight and printing
  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if (mWnd) {
    UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE;
#ifndef WINCE
    if (!aRepaint) {
      flags |= SWP_NOREDRAW;
    }
#endif

    ClearThemeRegion();
    VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, aWidth, GetHeight(aHeight), flags));
    SetThemeRegion();
  }

  if (aRepaint)
    Invalidate(PR_FALSE);

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: Window Z-order and state.
 *
 * nsIWidget::PlaceBehind, nsIWidget::SetSizeMode,
 * nsIWidget::ConstrainPosition
 *
 * Z-order, positioning, restore, minimize, and maximize.
 *
 **************************************************************/

// Position the window behind the given window
NS_METHOD nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                nsIWidget *aWidget, PRBool aActivate)
{
  HWND behind = HWND_TOP;
  if (aPlacement == eZPlacementBottom)
    behind = HWND_BOTTOM;
  else if (aPlacement == eZPlacementBelow && aWidget)
    behind = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  UINT flags = SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE;
  if (!aActivate)
    flags |= SWP_NOACTIVATE;

  if (!CanTakeFocus() && behind == HWND_TOP)
  {
    // Can't place the window to top so place it behind the foreground window
    // (as long as it is not topmost)
    HWND wndAfter = ::GetForegroundWindow();
    if (!wndAfter)
      behind = HWND_BOTTOM;
    else if (!(GetWindowLongPtrW(wndAfter, GWL_EXSTYLE) & WS_EX_TOPMOST))
      behind = wndAfter;
    flags |= SWP_NOACTIVATE;
  }

  ::SetWindowPos(mWnd, behind, 0, 0, 0, 0, flags);
  return NS_OK;
}

// Maximize, minimize or restore the window.
#if !defined(WINCE) // implemented in nsWindowCE.cpp
NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode) {

  nsresult rv;

  // Let's not try and do anything if we're already in that state.
  // (This is needed to prevent problems when calling window.minimize(), which
  // calls us directly, and then the OS triggers another call to us.)
  if (aMode == mSizeMode)
    return NS_OK;

  // save the requested state
  rv = nsBaseWidget::SetSizeMode(aMode);
  if (NS_SUCCEEDED(rv) && mIsVisible) {
    int mode;

    switch (aMode) {
      case nsSizeMode_Fullscreen :
        mode = SW_MAXIMIZE;
        break;

      case nsSizeMode_Maximized :
        mode = SW_MAXIMIZE;
        break;
      case nsSizeMode_Minimized :
        mode = sTrimOnMinimize ? SW_MINIMIZE : SW_SHOWMINIMIZED;
        if (!sTrimOnMinimize) {
          // Find the next window that is enabled, visible, and not minimized.
          HWND hwndBelow = ::GetNextWindow(mWnd, GW_HWNDNEXT);
          while (hwndBelow && (!::IsWindowEnabled(hwndBelow) || !::IsWindowVisible(hwndBelow) ||
                               ::IsIconic(hwndBelow))) {
            hwndBelow = ::GetNextWindow(hwndBelow, GW_HWNDNEXT);
          }

          // Push ourselves to the bottom of the stack, then activate the
          // next window.
          ::SetWindowPos(mWnd, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
          if (hwndBelow)
            ::SetForegroundWindow(hwndBelow);

          // Play the minimize sound while we're here, since that is also
          // forgotten when we use SW_SHOWMINIMIZED.
          ::PlaySoundW(L"Minimize", nsnull, SND_ALIAS | SND_NODEFAULT | SND_ASYNC);
        }
        break;
      default :
        mode = SW_RESTORE;
    }
    ::ShowWindow(mWnd, mode);
    // we dispatch an activate event here to ensure that the right child window
    // is focused
    if (mode == SW_RESTORE || mode == SW_MAXIMIZE)
      DispatchFocusToTopLevelWindow(NS_ACTIVATE);
  }
  return rv;
}
#endif // !defined(WINCE)

// Constrain a potential move to fit onscreen
NS_METHOD nsWindow::ConstrainPosition(PRBool aAllowSlop,
                                      PRInt32 *aX, PRInt32 *aY)
{
  if (!mIsTopWidgetWindow) // only a problem for top-level windows
    return NS_OK;

  PRBool doConstrain = PR_FALSE; // whether we have enough info to do anything

  /* get our playing field. use the current screen, or failing that
    for any reason, use device caps for the default screen. */
  RECT screenRect;

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    PRInt32 left, top, width, height;

    // zero size rects confuse the screen manager
    width = mBounds.width > 0 ? mBounds.width : 1;
    height = mBounds.height > 0 ? mBounds.height : 1;
    screenmgr->ScreenForRect(*aX, *aY, width, height,
                             getter_AddRefs(screen));
    if (screen) {
      screen->GetAvailRect(&left, &top, &width, &height);
      screenRect.left = left;
      screenRect.right = left+width;
      screenRect.top = top;
      screenRect.bottom = top+height;
      doConstrain = PR_TRUE;
    }
  } else {
    if (mWnd) {
      HDC dc = ::GetDC(mWnd);
      if (dc) {
        if (::GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
          ::SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
          doConstrain = PR_TRUE;
        }
        ::ReleaseDC(mWnd, dc);
      }
    }
  }

  if (aAllowSlop) {
    if (*aX < screenRect.left - mBounds.width + kWindowPositionSlop)
      *aX = screenRect.left - mBounds.width + kWindowPositionSlop;
    else if (*aX >= screenRect.right - kWindowPositionSlop)
      *aX = screenRect.right - kWindowPositionSlop;

    if (*aY < screenRect.top - mBounds.height + kWindowPositionSlop)
      *aY = screenRect.top - mBounds.height + kWindowPositionSlop;
    else if (*aY >= screenRect.bottom - kWindowPositionSlop)
      *aY = screenRect.bottom - kWindowPositionSlop;

  } else {

    if (*aX < screenRect.left)
      *aX = screenRect.left;
    else if (*aX >= screenRect.right - mBounds.width)
      *aX = screenRect.right - mBounds.width;

    if (*aY < screenRect.top)
      *aY = screenRect.top;
    else if (*aY >= screenRect.bottom - mBounds.height)
      *aY = screenRect.bottom - mBounds.height;
  }

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Enable, nsIWidget::IsEnabled
 *
 * Enabling and disabling the widget.
 *
 **************************************************************/

// Enable/disable this component
NS_METHOD nsWindow::Enable(PRBool bState)
{
  if (mWnd) {
    ::EnableWindow(mWnd, bState);
  }
  return NS_OK;
}

// Return the current enable state
NS_METHOD nsWindow::IsEnabled(PRBool *aState)
{
  NS_ENSURE_ARG_POINTER(aState);

#ifndef WINCE
  *aState = !mWnd || (::IsWindowEnabled(mWnd) && ::IsWindowEnabled(::GetAncestor(mWnd, GA_ROOT)));
#else
  *aState = !mWnd || (::IsWindowEnabled(mWnd) && ::IsWindowEnabled(mWnd));
#endif

  return NS_OK;
}


/**************************************************************
 *
 * SECTION: nsIWidget::SetFocus
 *
 * Give the focus to this widget.
 *
 **************************************************************/

NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
  if (mWnd) {
    // Uniconify, if necessary
    HWND toplevelWnd = GetTopLevelHWND(mWnd);
    if (::IsIconic(toplevelWnd))
      ::ShowWindow(toplevelWnd, SW_RESTORE);
    ::SetFocus(mWnd);
  }
  return NS_OK;
}


/**************************************************************
 *
 * SECTION: Bounds
 *
 * nsIWidget::GetBounds, nsIWidget::GetScreenBounds,
 * nsIWidget::GetClientBounds
 *
 * Bound calculations.
 *
 **************************************************************/

// Get this component dimension
NS_METHOD nsWindow::GetBounds(nsIntRect &aRect)
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

// Get this component dimension
NS_METHOD nsWindow::GetClientBounds(nsIntRect &aRect)
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

// Get the bounds, but don't take into account the client size
void nsWindow::GetNonClientBounds(nsIntRect &aRect)
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

// Like GetBounds, but don't offset by the parent
NS_METHOD nsWindow::GetScreenBounds(nsIntRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetWindowRect(mWnd, &r));

    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;
    aRect.x = r.left;
    aRect.y = r.top;
  } else
    aRect = mBounds;

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetBackgroundColor
 *
 * Sets the window background paint color.
 *
 **************************************************************/

NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  nsBaseWidget::SetBackgroundColor(aColor);

  if (mBrush)
    ::DeleteObject(mBrush);

  mBrush = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
#ifndef WINCE
  if (mWnd != NULL) {
    ::SetClassLongPtrW(mWnd, GCLP_HBRBACKGROUND, (LONG_PTR)mBrush);
  }
#endif
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetCursor
 *
 * SetCursor and related utilities for manging cursor state.
 *
 **************************************************************/

// Set this component cursor
NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  // Only change cursor if it's changing

  //XXX mCursor isn't always right.  Scrollbars and others change it, too.
  //XXX If we want this optimization we need a better way to do it.
  //if (aCursor != mCursor) {
  HCURSOR newCursor = NULL;

  switch (aCursor) {
    case eCursor_select:
      newCursor = ::LoadCursor(NULL, IDC_IBEAM);
      break;

    case eCursor_wait:
      newCursor = ::LoadCursor(NULL, IDC_WAIT);
      break;

    case eCursor_hyperlink:
    {
      newCursor = ::LoadCursor(NULL, IDC_HAND);
      break;
    }

    case eCursor_standard:
      newCursor = ::LoadCursor(NULL, IDC_ARROW);
      break;

    case eCursor_n_resize:
    case eCursor_s_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENS);
      break;

    case eCursor_w_resize:
    case eCursor_e_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZEWE);
      break;

    case eCursor_nw_resize:
    case eCursor_se_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENWSE);
      break;

    case eCursor_ne_resize:
    case eCursor_sw_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENESW);
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
      // XXX this CSS3 cursor needs to be implemented
      break;

    case eCursor_zoom_in:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ZOOMIN));
      break;

    case eCursor_zoom_out:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ZOOMOUT));
      break;

    case eCursor_not_allowed:
    case eCursor_no_drop:
      newCursor = ::LoadCursor(NULL, IDC_NO);
      break;

    case eCursor_col_resize:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_COLRESIZE));
      break;

    case eCursor_row_resize:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ROWRESIZE));
      break;

    case eCursor_vertical_text:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_VERTICALTEXT));
      break;

    case eCursor_all_scroll:
      // XXX not 100% appropriate perhaps
      newCursor = ::LoadCursor(NULL, IDC_SIZEALL);
      break;

    case eCursor_nesw_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENESW);
      break;

    case eCursor_nwse_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENWSE);
      break;

    case eCursor_ns_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENS);
      break;

    case eCursor_ew_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZEWE);
      break;

    case eCursor_none:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_NONE));
      break;

    default:
      NS_ERROR("Invalid cursor type");
      break;
  }

  if (NULL != newCursor) {
    mCursor = aCursor;
    HCURSOR oldCursor = ::SetCursor(newCursor);
    
    if (sHCursor == oldCursor) {
      NS_IF_RELEASE(sCursorImgContainer);
      if (sHCursor != NULL)
        ::DestroyIcon(sHCursor);
      sHCursor = NULL;
    }
  }

  return NS_OK;
}

// Setting the actual cursor
NS_IMETHODIMP nsWindow::SetCursor(imgIContainer* aCursor,
                                  PRUint32 aHotspotX, PRUint32 aHotspotY)
{
  if (sCursorImgContainer == aCursor && sHCursor) {
    ::SetCursor(sHCursor);
    return NS_OK;
  }

  PRInt32 width;
  PRInt32 height;

  nsresult rv;
  rv = aCursor->GetWidth(&width);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCursor->GetHeight(&height);
  NS_ENSURE_SUCCESS(rv, rv);

  // Reject cursors greater than 128 pixels in either direction, to prevent
  // spoofing.
  // XXX ideally we should rescale. Also, we could modify the API to
  // allow trusted content to set larger cursors.
  if (width > 128 || height > 128)
    return NS_ERROR_NOT_AVAILABLE;

  HCURSOR cursor;
  rv = nsWindowGfx::CreateIcon(aCursor, PR_TRUE, aHotspotX, aHotspotY, &cursor);
  NS_ENSURE_SUCCESS(rv, rv);

  mCursor = nsCursor(-1);
  ::SetCursor(cursor);

  NS_IF_RELEASE(sCursorImgContainer);
  sCursorImgContainer = aCursor;
  NS_ADDREF(sCursorImgContainer);

  if (sHCursor != NULL)
    ::DestroyIcon(sHCursor);
  sHCursor = cursor;

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Get/SetTransparencyMode
 *
 * Manage the transparency mode of the top-level window
 * containing this widget.
 *
 **************************************************************/

#ifdef MOZ_XUL
nsTransparencyMode nsWindow::GetTransparencyMode()
{
  return GetTopLevelWindow(PR_TRUE)->GetWindowTranslucencyInner();
}

void nsWindow::SetTransparencyMode(nsTransparencyMode aMode)
{
  GetTopLevelWindow(PR_TRUE)->SetWindowTranslucencyInner(aMode);
}
#endif

/**************************************************************
 *
 * SECTION: nsIWidget::HideWindowChrome
 *
 * Show or hide window chrome.
 *
 **************************************************************/

NS_IMETHODIMP nsWindow::HideWindowChrome(PRBool aShouldHide)
{
  HWND hwnd = GetTopLevelHWND(mWnd, PR_TRUE);
  if (!GetNSWindowPtr(hwnd))
  {
    NS_WARNING("Trying to hide window decorations in an embedded context");
    return NS_ERROR_FAILURE;
  }

  DWORD_PTR style, exStyle;
  if (aShouldHide) {
    DWORD_PTR tempStyle = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD_PTR tempExStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    style = tempStyle & ~(WS_CAPTION | WS_THICKFRAME);
    exStyle = tempExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                              WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

    mOldStyle = tempStyle;
    mOldExStyle = tempExStyle;
  }
  else {
    if (!mOldStyle || !mOldExStyle) {
      mOldStyle = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
      mOldExStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    }

    style = mOldStyle;
    exStyle = mOldExStyle;
  }

  VERIFY_WINDOW_STYLE(style);
  ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
  ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Validate, nsIWidget::Invalidate
 *
 * Validate or invalidate an area of the client for painting.
 *
 **************************************************************/

// Validate a visible area of a widget.
NS_METHOD nsWindow::Validate()
{
  if (mWnd)
    VERIFY(::ValidateRect(mWnd, NULL));
  return NS_OK;
}

// Invalidate this component visible area
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
  if (mWnd)
  {
#ifdef WIDGET_DEBUG_OUTPUT
    debug_DumpInvalidate(stdout,
                         this,
                         nsnull,
                         aIsSynchronous,
                         nsCAutoString("noname"),
                         (PRInt32) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

    VERIFY(::InvalidateRect(mWnd, NULL, FALSE));

    if (aIsSynchronous) {
      VERIFY(::UpdateWindow(mWnd));
    }
  }
  return NS_OK;
}

// Invalidate this component visible area
NS_METHOD nsWindow::Invalidate(const nsIntRect & aRect, PRBool aIsSynchronous)
{
  if (mWnd)
  {
#ifdef WIDGET_DEBUG_OUTPUT
    debug_DumpInvalidate(stdout,
                         this,
                         &aRect,
                         aIsSynchronous,
                         nsCAutoString("noname"),
                         (PRInt32) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

    RECT rect;

    rect.left   = aRect.x;
    rect.top    = aRect.y;
    rect.right  = aRect.x + aRect.width;
    rect.bottom = aRect.y + aRect.height;

    VERIFY(::InvalidateRect(mWnd, &rect, FALSE));

    if (aIsSynchronous) {
      VERIFY(::UpdateWindow(mWnd));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(PRBool aFullScreen)
{
#if WINCE_WINDOWS_MOBILE
  RECT rc;
  if (aFullScreen) {
    SetForegroundWindow(mWnd);
    SHFullScreen(mWnd, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON);
    SetRect(&rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
  }
  else {
    SHFullScreen(mWnd, SHFS_SHOWTASKBAR | SHFS_SHOWSTARTICON);
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, FALSE);
  }
  MoveWindow(mWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);

  if (aFullScreen)
    mSizeMode = nsSizeMode_Fullscreen;
#endif

  return nsBaseWidget::MakeFullScreen(aFullScreen);
}

/**************************************************************
 *
 * SECTION: nsIWidget::Update
 *
 * Force a synchronous repaint of the window.
 *
 **************************************************************/

NS_IMETHODIMP nsWindow::Update()
{
  nsresult rv = NS_OK;

  // updates can come through for windows no longer holding an mWnd during
  // deletes triggered by JavaScript in buttons with mouse feedback
  if (mWnd)
    VERIFY(::UpdateWindow(mWnd));

  return rv;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Scroll
 *
 * Scroll this widget.
 *
 **************************************************************/

static PRBool
ClipRegionContainedInRect(const nsTArray<nsIntRect>& aClipRects,
                          const nsIntRect& aRect)
{
  for (PRUint32 i = 0; i < aClipRects.Length(); ++i) {
    if (!aRect.Contains(aClipRects[i]))
      return PR_FALSE;
  }
  return PR_TRUE;
}

void
nsWindow::Scroll(const nsIntPoint& aDelta,
                 const nsTArray<nsIntRect>& aDestRects,
                 const nsTArray<Configuration>& aConfigurations)
{
  // We use SW_SCROLLCHILDREN if all the windows that intersect the
  // affected area are moving by the scroll amount.
  // First, build the set of widgets that are to be moved by the scroll
  // amount.
  // At the same time, set the clip region of all changed windows to the
  // intersection of the current and new regions.
  nsTHashtable<nsPtrHashKey<nsWindow> > scrolledWidgets;
  scrolledWidgets.Init();
  for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
    NS_ASSERTION(w->GetParent() == this,
                 "Configured widget is not a child");
    if (configuration.mBounds == w->mBounds + aDelta) {
      scrolledWidgets.PutEntry(w);
    }
    w->SetWindowClipRegion(configuration.mClipRegion, PR_TRUE);
  }

  for (PRUint32 i = 0; i < aDestRects.Length(); ++i) {
    nsIntRect affectedRect;
    affectedRect.UnionRect(aDestRects[i], aDestRects[i] - aDelta);
    // We pass SW_INVALIDATE because areas that get scrolled into view
    // from offscreen (but inside the scroll area) need to be repainted.
    UINT flags = SW_SCROLLCHILDREN | SW_INVALIDATE;
    // Now check if any of our children would be affected by
    // SW_SCROLLCHILDREN but not supposed to scroll.
    for (nsWindow* w = static_cast<nsWindow*>(GetFirstChild()); w;
         w = static_cast<nsWindow*>(w->GetNextSibling())) {
      if (w->mBounds.Intersects(affectedRect)) {
        // This child will be affected
        nsPtrHashKey<nsWindow>* entry = scrolledWidgets.GetEntry(w);
        if (entry) {
          // It's supposed to be scrolled, so we can still use
          // SW_SCROLLCHILDREN. But don't allow SW_SCROLLCHILDREN to be
          // used on it again by a later rectangle in aDestRects, we
          // don't want it to move twice!
          scrolledWidgets.RawRemoveEntry(entry);
        } else {
          flags &= ~SW_SCROLLCHILDREN;
          // We may have removed some children from scrolledWidgets even
          // though we decide here to not use SW_SCROLLCHILDREN. That's OK,
          // it just means that we might not use SW_SCROLLCHILDREN
          // for a later rectangle when we could have.
          break;
        }
      }
    }

    if (flags & SW_SCROLLCHILDREN) {
      for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
        const Configuration& configuration = aConfigurations[i];
        nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
        // Widgets that will be scrolled by SW_SCROLLCHILDREN but which
        // will be partly visible outside the scroll area after scrolling
        // must be invalidated, because SW_SCROLLCHILDREN doesn't
        // update parts of widgets outside the area it scrolled, even
        // if it moved them.
        if (w->mBounds.Intersects(affectedRect) &&
            !ClipRegionContainedInRect(configuration.mClipRegion,
                                       affectedRect - (w->mBounds.TopLeft() + aDelta))) {
          w->Invalidate(PR_FALSE);
        }
      }
    }

    // Note that when SW_SCROLLCHILDREN is used, WM_MOVE messages are sent
    // which will update the mBounds of the children.
    RECT clip = { affectedRect.x, affectedRect.y, affectedRect.XMost(), affectedRect.YMost() };
    ::ScrollWindowEx(mWnd, aDelta.x, aDelta.y, &clip, &clip, NULL, NULL, flags);
  }

  // Now make sure all children actually get positioned, sized and clipped
  // correctly. If SW_SCROLLCHILDREN already moved widgets to their correct
  // locations, then the SetWindowPos calls this triggers will just be
  // no-ops.
  ConfigureChildren(aConfigurations);
}

/**************************************************************
 *
 * SECTION: Native data storage
 *
 * nsIWidget::GetNativeData
 * nsIWidget::FreeNativeData
 *
 * Set or clear native data based on a constant.
 *
 **************************************************************/

// Return some native data according to aDataType
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
  switch (aDataType) {
    case NS_NATIVE_PLUGIN_PORT:
      mIsPluginWindow = 1;
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
      return (void*)mWnd;
    case NS_NATIVE_GRAPHIC:
      // XXX:  This is sleezy!!  Remember to Release the DC after using it!
#ifdef MOZ_XUL
      return (void*)(eTransparencyTransparent == mTransparencyMode) ?
        mMemoryDC : ::GetDC(mWnd);
#else
      return (void*)::GetDC(mWnd);
#endif

#ifdef NS_ENABLE_TSF
    case NS_NATIVE_TSF_THREAD_MGR:
      return nsTextStore::GetThreadMgr();
    case NS_NATIVE_TSF_CATEGORY_MGR:
      return nsTextStore::GetCategoryMgr();
    case NS_NATIVE_TSF_DISPLAY_ATTR_MGR:
      return nsTextStore::GetDisplayAttrMgr();
#endif //NS_ENABLE_TSF

    default:
      break;
  }

  return NULL;
}

// Free some native data according to aDataType
void nsWindow::FreeNativeData(void * data, PRUint32 aDataType)
{
  switch (aDataType)
  {
    case NS_NATIVE_GRAPHIC:
#ifdef MOZ_XUL
      if (eTransparencyTransparent != mTransparencyMode)
        ::ReleaseDC(mWnd, (HDC)data);
#else
      ::ReleaseDC(mWnd, (HDC)data);
#endif
      break;
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
      break;
    default:
      break;
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetTitle
 *
 * Set the main windows title text.
 *
 **************************************************************/

NS_METHOD nsWindow::SetTitle(const nsAString& aTitle)
{
  const nsString& strTitle = PromiseFlatString(aTitle);
  ::SendMessageW(mWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCWSTR)strTitle.get());
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetIcon
 *
 * Set the main windows icon.
 *
 **************************************************************/

NS_METHOD nsWindow::SetIcon(const nsAString& aIconSpec) 
{
#ifndef WINCE
  // Assume the given string is a local identifier for an icon file.

  nsCOMPtr<nsILocalFile> iconFile;
  ResolveIconName(aIconSpec, NS_LITERAL_STRING(".ico"),
                  getter_AddRefs(iconFile));
  if (!iconFile)
    return NS_OK; // not an error if icon is not found

  nsAutoString iconPath;
  iconFile->GetPath(iconPath);

  // XXX this should use MZLU (see bug 239279)

  ::SetLastError(0);

  HICON bigIcon = (HICON)::LoadImageW(NULL,
                                      (LPCWSTR)iconPath.get(),
                                      IMAGE_ICON,
                                      ::GetSystemMetrics(SM_CXICON),
                                      ::GetSystemMetrics(SM_CYICON),
                                      LR_LOADFROMFILE );
  HICON smallIcon = (HICON)::LoadImageW(NULL,
                                        (LPCWSTR)iconPath.get(),
                                        IMAGE_ICON,
                                        ::GetSystemMetrics(SM_CXSMICON),
                                        ::GetSystemMetrics(SM_CYSMICON),
                                        LR_LOADFROMFILE );

  if (bigIcon) {
    HICON icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)bigIcon);
    if (icon)
      ::DestroyIcon(icon);
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    printf( "\nIcon load error; icon=%s, rc=0x%08X\n\n", cPath.get(), ::GetLastError() );
  }
#endif
  if (smallIcon) {
    HICON icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)smallIcon);
    if (icon)
      ::DestroyIcon(icon);
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    printf( "\nSmall icon load error; icon=%s, rc=0x%08X\n\n", cPath.get(), ::GetLastError() );
  }
#endif
#endif // WINCE
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::WidgetToScreenOffset
 *
 * Return this widget's origin in screen coordinates.
 *
 **************************************************************/

nsIntPoint nsWindow::WidgetToScreenOffset()
{
  POINT point;
  point.x = 0;
  point.y = 0;
  ::ClientToScreen(mWnd, &point);
  return nsIntPoint(point.x, point.y);
}

/**************************************************************
 *
 * SECTION: nsIWidget::EnableDragDrop
 *
 * Enables/Disables drag and drop of files on this widget.
 *
 **************************************************************/

#if !defined(WINCE) // implemented in nsWindowCE.cpp
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
      mNativeDragTarget->mDragCancelled = PR_TRUE;
      NS_RELEASE(mNativeDragTarget);
    }
  }
  return rv;
}
#endif

/**************************************************************
 *
 * SECTION: nsIWidget::CaptureMouse
 *
 * Enables/Disables system mouse capture.
 *
 **************************************************************/

NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
  if (!nsToolkit::gMouseTrailer) {
    NS_ERROR("nsWindow::CaptureMouse called after nsToolkit destroyed");
    return NS_OK;
  }

  if (aCapture) {
    nsToolkit::gMouseTrailer->SetCaptureWindow(mWnd);
    ::SetCapture(mWnd);
  } else {
    nsToolkit::gMouseTrailer->SetCaptureWindow(NULL);
    ::ReleaseCapture();
  }
  mIsInMouseCapture = aCapture;
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::CaptureRollupEvents
 *
 * Dealing with event rollup on destroy for popups. Enables &
 * Disables system capture of any and all events that would
 * cause a dropdown to be rolled up.
 *
 **************************************************************/

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener,
                                            PRBool aDoCapture,
                                            PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    /* we haven't bothered carrying a weak reference to sRollupWidget because
       we believe lifespan is properly scoped. this next assertion helps
       assure that remains true. */
    NS_ASSERTION(!sRollupWidget, "rollup widget reassigned before release");
    sRollupConsumeEvent = aConsumeRollupEvent;
    NS_IF_RELEASE(sRollupListener);
    NS_IF_RELEASE(sRollupWidget);
    sRollupListener = aListener;
    NS_ADDREF(aListener);
    sRollupWidget = this;
    NS_ADDREF(this);

#ifndef WINCE
    if (!sMsgFilterHook && !sCallProcHook && !sCallMouseHook) {
      RegisterSpecialDropdownHooks();
    }
    sProcessHook = PR_TRUE;
#endif
    
  } else {
    NS_IF_RELEASE(sRollupListener);
    NS_IF_RELEASE(sRollupWidget);
    
#ifndef WINCE
    sProcessHook = PR_FALSE;
    UnregisterSpecialDropdownHooks();
#endif
  }

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::GetAttention
 *
 * Bring this window to the user's attention.
 *
 **************************************************************/

// Draw user's attention to this window until it comes to foreground.
NS_IMETHODIMP
nsWindow::GetAttention(PRInt32 aCycleCount)
{
#ifndef WINCE
  // Got window?
  if (!mWnd)
    return NS_ERROR_NOT_INITIALIZED;

  // Don't flash if the flash count is 0 or if the
  // top level window is already active.
  HWND fgWnd = ::GetForegroundWindow();
  if (aCycleCount == 0 || fgWnd == GetTopLevelHWND(mWnd))
    return NS_OK;

  HWND flashWnd = mWnd;
  while (HWND ownerWnd = ::GetWindow(flashWnd, GW_OWNER)) {
    flashWnd = ownerWnd;
  }

  // Don't flash if the owner window is active either.
  if (fgWnd == flashWnd)
    return NS_OK;

  DWORD defaultCycleCount = 0;
  ::SystemParametersInfo(SPI_GETFOREGROUNDFLASHCOUNT, 0, &defaultCycleCount, 0);

  FLASHWINFO flashInfo = { sizeof(FLASHWINFO), flashWnd,
    FLASHW_ALL, aCycleCount > 0 ? aCycleCount : defaultCycleCount, 0 };
  ::FlashWindowEx(&flashInfo);
#endif
  return NS_OK;
}

void nsWindow::StopFlashing()
{
#ifndef WINCE
  HWND flashWnd = mWnd;
  while (HWND ownerWnd = ::GetWindow(flashWnd, GW_OWNER)) {
    flashWnd = ownerWnd;
  }

  FLASHWINFO flashInfo = { sizeof(FLASHWINFO), flashWnd,
    FLASHW_STOP, 0, 0 };
  ::FlashWindowEx(&flashInfo);
#endif
}

/**************************************************************
 *
 * SECTION: nsIWidget::HasPendingInputEvent
 *
 * Ask whether there user input events pending.  All input events are
 * included, including those not targeted at this nsIwidget instance.
 *
 **************************************************************/

PRBool
nsWindow::HasPendingInputEvent()
{
  // If there is pending input or the user is currently
  // moving the window then return true.
  // Note: When the user is moving the window WIN32 spins
  // a separate event loop and input events are not
  // reported to the application.
  if (HIWORD(GetQueueStatus(QS_INPUT)))
    return PR_TRUE;
#ifdef WINCE
  return PR_FALSE;
#else
  GUITHREADINFO guiInfo;
  guiInfo.cbSize = sizeof(GUITHREADINFO);
  if (!GetGUIThreadInfo(GetCurrentThreadId(), &guiInfo))
    return PR_FALSE;
  return GUI_INMOVESIZE == (guiInfo.flags & GUI_INMOVESIZE);
#endif
}

/**************************************************************
 *
 * SECTION: nsIWidget::GetThebesSurface
 *
 * Get the Thebes surface associated with this widget.
 *
 **************************************************************/

gfxASurface *nsWindow::GetThebesSurface()
{
  if (mPaintDC)
    return (new gfxWindowsSurface(mPaintDC));

  return (new gfxWindowsSurface(mWnd));
}

/**************************************************************
 *
 * SECTION: nsIWidget::OnDefaultButtonLoaded
 *
 * Called after the dialog is loaded and it has a default button.
 *
 **************************************************************/
 
NS_IMETHODIMP
nsWindow::OnDefaultButtonLoaded(const nsIntRect &aButtonRect)
{
#ifdef WINCE
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  if (aButtonRect.IsEmpty())
    return NS_OK;

  // Don't snap when we are not active.
  HWND activeWnd = ::GetActiveWindow();
  if (activeWnd != ::GetForegroundWindow() ||
      GetTopLevelHWND(mWnd, PR_TRUE) != GetTopLevelHWND(activeWnd, PR_TRUE)) {
    return NS_OK;
  }

  PRBool isAlwaysSnapCursor = PR_FALSE;
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
    if (prefBranch) {
      prefBranch->GetBoolPref("ui.cursor_snapping.always_enabled",
                              &isAlwaysSnapCursor);
    }
  }

  if (!isAlwaysSnapCursor) {
    BOOL snapDefaultButton;
    if (!::SystemParametersInfo(SPI_GETSNAPTODEFBUTTON, 0,
                                &snapDefaultButton, 0) || !snapDefaultButton)
      return NS_OK;
  }

  nsIntRect widgetRect;
  nsresult rv = GetScreenBounds(widgetRect);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIntRect buttonRect(aButtonRect + widgetRect.TopLeft());

  nsIntPoint centerOfButton(buttonRect.x + buttonRect.width / 2,
                            buttonRect.y + buttonRect.height / 2);
  // The center of the button can be outside of the widget.
  // E.g., it could be hidden by scrolling.
  if (!widgetRect.Contains(centerOfButton)) {
    return NS_OK;
  }

  if (!::SetCursorPos(centerOfButton.x, centerOfButton.y)) {
    NS_ERROR("SetCursorPos failed");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
#endif
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Moz Events
 **
 ** Moz GUI event management. 
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: Mozilla event initialization
 *
 * Helpers for initializing moz events.
 *
 **************************************************************/

// Event intialization
MSG nsWindow::InitMSG(UINT aMessage, WPARAM wParam, LPARAM lParam)
{
  MSG msg;
  msg.message = aMessage;
  msg.wParam  = wParam;
  msg.lParam  = lParam;
  return msg;
}

void nsWindow::InitEvent(nsGUIEvent& event, nsIntPoint* aPoint)
{
  if (nsnull == aPoint) {     // use the point from the event
    // get the message position in client coordinates
    if (mWnd != NULL) {

      DWORD pos = ::GetMessagePos();
      POINT cpos;
      
      cpos.x = GET_X_LPARAM(pos);
      cpos.y = GET_Y_LPARAM(pos);

      ::ScreenToClient(mWnd, &cpos);
      event.refPoint.x = cpos.x;
      event.refPoint.y = cpos.y;
    } else {
      event.refPoint.x = 0;
      event.refPoint.y = 0;
    }
  }
  else {  
    // use the point override if provided
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }

#ifndef WINCE
  event.time = ::GetMessageTime();
#else
  event.time = PR_Now() / 1000;
#endif

  mLastPoint = event.refPoint;
}

/**************************************************************
 *
 * SECTION: Moz event dispatch helpers
 *
 * Helpers for dispatching different types of moz events.
 *
 **************************************************************/

// Main event dispatch. Invokes callback and ProcessEvent method on
// Event Listener object. Part of nsIWidget.
NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
#ifdef WIDGET_DEBUG_OUTPUT
  debug_DumpEvent(stdout,
                  event->widget,
                  event,
                  nsCAutoString("something"),
                  (PRInt32) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

  aStatus = nsEventStatus_eIgnore;

  // skip processing of suppressed blur events
  if (event->message == NS_DEACTIVATE && BlurEventsSuppressed())
    return NS_OK;

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

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event(PR_TRUE, aMsg, this);
  InitEvent(event);

  PRBool result = DispatchWindowEvent(&event);
  return result;
}

PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event, nsEventStatus &aStatus) {
  DispatchEvent(event, aStatus);
  return ConvertStatus(aStatus);
}

PRBool nsWindow::DispatchKeyEvent(PRUint32 aEventType, WORD aCharCode,
                   const nsTArray<nsAlternativeCharCode>* aAlternativeCharCodes,
                   UINT aVirtualCharCode, const MSG *aMsg,
                   const nsModifierKeyState &aModKeyState,
                   PRUint32 aFlags)
{
  nsKeyEvent event(PR_TRUE, aEventType, this);
  nsIntPoint point(0, 0);

  InitEvent(event, &point); // this add ref's event.widget

  event.flags |= aFlags;
  event.charCode = aCharCode;
  if (aAlternativeCharCodes)
    event.alternativeCharCodes.AppendElements(*aAlternativeCharCodes);
  event.keyCode  = aVirtualCharCode;

#ifdef KE_DEBUG
  static cnt=0;
  printf("%d DispatchKE Type: %s charCode %d  keyCode %d ", cnt++,
        (NS_KEY_PRESS == aEventType) ? "PRESS" : (aEventType == NS_KEY_UP ? "Up" : "Down"),
         event.charCode, event.keyCode);
  printf("Shift: %s Control %s Alt: %s \n", 
         (mIsShiftDown ? "D" : "U"), (mIsControlDown ? "D" : "U"), (mIsAltDown ? "D" : "U"));
  printf("[%c][%c][%c] <==   [%c][%c][%c][ space bar ][%c][%c][%c]\n",
         IS_VK_DOWN(NS_VK_SHIFT) ? 'S' : ' ',
         IS_VK_DOWN(NS_VK_CONTROL) ? 'C' : ' ',
         IS_VK_DOWN(NS_VK_ALT) ? 'A' : ' ',
         IS_VK_DOWN(VK_LSHIFT) ? 'S' : ' ',
         IS_VK_DOWN(VK_LCONTROL) ? 'C' : ' ',
         IS_VK_DOWN(VK_LMENU) ? 'A' : ' ',
         IS_VK_DOWN(VK_RMENU) ? 'A' : ' ',
         IS_VK_DOWN(VK_RCONTROL) ? 'C' : ' ',
         IS_VK_DOWN(VK_RSHIFT) ? 'S' : ' ');
#endif

  event.isShift   = aModKeyState.mIsShiftDown;
  event.isControl = aModKeyState.mIsControlDown;
  event.isMeta    = PR_FALSE;
  event.isAlt     = aModKeyState.mIsAltDown;

  nsPluginEvent pluginEvent;
  if (aMsg && PluginHasFocus()) {
    pluginEvent.event = aMsg->message;
    pluginEvent.wParam = aMsg->wParam;
    pluginEvent.lParam = aMsg->lParam;
    event.nativeMsg = (void *)&pluginEvent;
  }

  PRBool result = DispatchWindowEvent(&event);

  return result;
}

PRBool nsWindow::DispatchCommandEvent(PRUint32 aEventCommand)
{
  nsCOMPtr<nsIAtom> command;
  switch (aEventCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      command = nsWidgetAtoms::Back;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      command = nsWidgetAtoms::Forward;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      command = nsWidgetAtoms::Reload;
      break;
    case APPCOMMAND_BROWSER_STOP:
      command = nsWidgetAtoms::Stop;
      break;
    case APPCOMMAND_BROWSER_SEARCH:
      command = nsWidgetAtoms::Search;
      break;
    case APPCOMMAND_BROWSER_FAVORITES:
      command = nsWidgetAtoms::Bookmarks;
      break;
    case APPCOMMAND_BROWSER_HOME:
      command = nsWidgetAtoms::Home;
      break;
    default:
      return PR_FALSE;
  }
  nsCommandEvent event(PR_TRUE, nsWidgetAtoms::onAppCommand, command, this);

  InitEvent(event);
  DispatchWindowEvent(&event);

  return PR_TRUE;
}

// Recursively dispatch synchronous paints for nsIWidget
// descendants with invalidated rectangles.
BOOL CALLBACK nsWindow::DispatchStarvedPaints(HWND aWnd, LPARAM aMsg)
{
  LONG_PTR proc = ::GetWindowLongPtrW(aWnd, GWLP_WNDPROC);
  if (proc == (LONG_PTR)&nsWindow::WindowProc) {
    // its one of our windows so check to see if it has a
    // invalidated rect. If it does. Dispatch a synchronous
    // paint.
    if (GetUpdateRect(aWnd, NULL, FALSE)) {
      VERIFY(::UpdateWindow(aWnd));
    }
  }
  return TRUE;
}

// Check for pending paints and dispatch any pending paint
// messages for any nsIWidget which is a descendant of the
// top-level window that *this* window is embedded within.
//
// Note: We do not dispatch pending paint messages for non
// nsIWidget managed windows.
void nsWindow::DispatchPendingEvents()
{
  gLastInputEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());

  // We need to ensure that reflow events do not get starved.
  // At the same time, we don't want to recurse through here
  // as that would prevent us from dispatching starved paints.
  static int recursionBlocker = 0;
  if (recursionBlocker++ == 0) {
    NS_ProcessPendingEvents(nsnull, PR_MillisecondsToInterval(100));
    --recursionBlocker;
  }

  // Quickly check to see if there are any
  // paint events pending.
  if (::GetQueueStatus(QS_PAINT)) {
    // Find the top level window.
    HWND topWnd = GetTopLevelHWND(mWnd);

    // Dispatch pending paints for all topWnd's descendant windows.
    // Note: EnumChildWindows enumerates all descendant windows not just
    // it's children.
#if !defined(WINCE)
    ::EnumChildWindows(topWnd, nsWindow::DispatchStarvedPaints, NULL);
#else
    nsWindowCE::EnumChildWindows(topWnd, nsWindow::DispatchStarvedPaints, NULL);
#endif
  }
}

// Deal with plugin events
PRBool nsWindow::DispatchPluginEvent(const MSG &aMsg)
{
  if (!PluginHasFocus())
    return PR_FALSE;

  nsGUIEvent event(PR_TRUE, NS_PLUGIN_EVENT, this);
  nsIntPoint point(0, 0);
  InitEvent(event, &point);
  nsPluginEvent pluginEvent;
  pluginEvent.event = aMsg.message;
  pluginEvent.wParam = aMsg.wParam;
  pluginEvent.lParam = aMsg.lParam;
  event.nativeMsg = (void *)&pluginEvent;
  return DispatchWindowEvent(&event);
}

void nsWindow::RemoveMessageAndDispatchPluginEvent(UINT aFirstMsg,
                                                   UINT aLastMsg)
{
  MSG msg;
  ::GetMessageW(&msg, mWnd, aFirstMsg, aLastMsg);
  DispatchPluginEvent(msg);
}

// Deal with all sort of mouse event
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam,
                                    LPARAM lParam, PRBool aIsContextMenuKey,
                                    PRInt16 aButton)
{
  PRBool result = PR_FALSE;

  if (!mEventCallback) {
    return result;
  }

  nsIntPoint eventPoint;
  eventPoint.x = GET_X_LPARAM(lParam);
  eventPoint.y = GET_Y_LPARAM(lParam);

  nsMouseEvent event(PR_TRUE, aEventType, this, nsMouseEvent::eReal,
                     aIsContextMenuKey
                     ? nsMouseEvent::eContextMenuKey
                     : nsMouseEvent::eNormal);
  if (aEventType == NS_CONTEXTMENU && aIsContextMenuKey) {
    nsIntPoint zero(0, 0);
    InitEvent(event, &zero);
  } else {
    InitEvent(event, &eventPoint);
  }

  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.button    = aButton;

  nsIntPoint mpScreen = eventPoint + WidgetToScreenOffset();

  // Suppress mouse moves caused by widget creation
  if (aEventType == NS_MOUSE_MOVE) 
  {
    if ((sLastMouseMovePoint.x == mpScreen.x) && (sLastMouseMovePoint.y == mpScreen.y))
      return result;
    sLastMouseMovePoint.x = mpScreen.x;
    sLastMouseMovePoint.y = mpScreen.y;
  }

  PRBool insideMovementThreshold = (abs(sLastMousePoint.x - eventPoint.x) < (short)::GetSystemMetrics(SM_CXDOUBLECLK)) &&
                                   (abs(sLastMousePoint.y - eventPoint.y) < (short)::GetSystemMetrics(SM_CYDOUBLECLK));

  BYTE eventButton;
  switch (aButton) {
    case nsMouseEvent::eLeftButton:
      eventButton = VK_LBUTTON;
      break;
    case nsMouseEvent::eMiddleButton:
      eventButton = VK_MBUTTON;
      break;
    case nsMouseEvent::eRightButton:
      eventButton = VK_RBUTTON;
      break;
    default:
      eventButton = 0;
      break;
  }

  // Doubleclicks are used to set the click count, then changed to mousedowns
  // We're going to time double-clicks from mouse *up* to next mouse *down*
#ifndef WINCE
  LONG curMsgTime = ::GetMessageTime();
#else
  LONG curMsgTime = PR_Now() / 1000;
#endif

  if (aEventType == NS_MOUSE_DOUBLECLICK) {
    event.message = NS_MOUSE_BUTTON_DOWN;
    event.button = aButton;
    sLastClickCount = 2;
  }
  else if (aEventType == NS_MOUSE_BUTTON_UP) {
    // remember when this happened for the next mouse down
    sLastMousePoint.x = eventPoint.x;
    sLastMousePoint.y = eventPoint.y;
    sLastMouseButton = eventButton;
  }
  else if (aEventType == NS_MOUSE_BUTTON_DOWN) {
    // now look to see if we want to convert this to a double- or triple-click
    if (((curMsgTime - sLastMouseDownTime) < (LONG)::GetDoubleClickTime()) && insideMovementThreshold &&
        eventButton == sLastMouseButton) {
      sLastClickCount ++;
    } else {
      // reset the click count, to count *this* click
      sLastClickCount = 1;
    }
    // Set last Click time on MouseDown only
    sLastMouseDownTime = curMsgTime;
  }
  else if (aEventType == NS_MOUSE_MOVE && !insideMovementThreshold) {
    sLastClickCount = 0;
  }
  else if (aEventType == NS_MOUSE_EXIT) {
    event.exit = IsTopLevelMouseExit(mWnd) ? nsMouseEvent::eTopLevel : nsMouseEvent::eChild;
  }
  event.clickCount = sLastClickCount;

#ifdef NS_DEBUG_XX
  printf("Msg Time: %d Click Count: %d\n", curMsgTime, event.clickCount);
#endif

  nsPluginEvent pluginEvent;

  switch (aEventType)
  {
    case NS_MOUSE_BUTTON_DOWN:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONDOWN;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONDOWN;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONDOWN;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_BUTTON_UP:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONUP;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONUP;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONUP;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_DOUBLECLICK:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONDBLCLK;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONDBLCLK;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONDBLCLK;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_MOVE:
      pluginEvent.event = WM_MOUSEMOVE;
      break;
    default:
      pluginEvent.event = WM_NULL;
      break;
  }

  pluginEvent.wParam = wParam;     // plugins NEED raw OS event flags!
  pluginEvent.lParam = lParam;

  event.nativeMsg = (void *)&pluginEvent;

  // call the event callback
  if (nsnull != mEventCallback) {
    if (nsToolkit::gMouseTrailer)
      nsToolkit::gMouseTrailer->Disable();
    if (aEventType == NS_MOUSE_MOVE) {
      if (nsToolkit::gMouseTrailer && !mIsInMouseCapture) {
        nsToolkit::gMouseTrailer->SetMouseTrailerWindow(mWnd);
      }
      nsIntRect rect;
      GetBounds(rect);
      rect.x = 0;
      rect.y = 0;

      if (rect.Contains(event.refPoint)) {
        if (sCurrentWindow == NULL || sCurrentWindow != this) {
          if ((nsnull != sCurrentWindow) && (!sCurrentWindow->mInDtor)) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT, wParam, pos);
          }
          sCurrentWindow = this;
          if (!mInDtor) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER, wParam, pos);
          }
        }
      }
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (sCurrentWindow == this) {
        sCurrentWindow = nsnull;
      }
    }

    result = DispatchWindowEvent(&event);

    if (nsToolkit::gMouseTrailer)
      nsToolkit::gMouseTrailer->Enable();

    // Release the widget with NS_IF_RELEASE() just in case
    // the context menu key code in nsEventListenerManager::HandleEvent()
    // released it already.
    return result;
  }

  return result;
}

// Deal with accessibile event
#ifdef ACCESSIBILITY
PRBool nsWindow::DispatchAccessibleEvent(PRUint32 aEventType, nsIAccessible** aAcc, nsIntPoint* aPoint)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback) {
    return result;
  }

  *aAcc = nsnull;

  nsAccessibleEvent event(PR_TRUE, aEventType, this);
  InitEvent(event, aPoint);

  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.accessible = nsnull;

  result = DispatchWindowEvent(&event);

  // if the event returned an accesssible get it.
  if (event.accessible)
    *aAcc = event.accessible;

  return result;
}
#endif

PRBool nsWindow::DispatchFocusToTopLevelWindow(PRUint32 aEventType)
{
  if (aEventType == NS_ACTIVATE)
    sJustGotActivate = PR_FALSE;
  sJustGotDeactivate = PR_FALSE;

  // clear the flags, and retrieve the toplevel window. This way, it doesn't
  // mattter what child widget received the focus event and it will always be
  // fired at the toplevel window.
  HWND toplevelWnd = GetTopLevelHWND(mWnd);
  if (toplevelWnd) {
    nsWindow *win = GetNSWindowPtr(toplevelWnd);
    if (win)
      return win->DispatchFocus(aEventType);
  }

  return PR_FALSE;
}

// Deal with focus messages
PRBool nsWindow::DispatchFocus(PRUint32 aEventType)
{
  // call the event callback
  if (mEventCallback) {
    nsGUIEvent event(PR_TRUE, aEventType, this);
    InitEvent(event);

    //focus and blur event should go to their base widget loc, not current mouse pos
    event.refPoint.x = 0;
    event.refPoint.y = 0;

    nsPluginEvent pluginEvent;

    switch (aEventType)
    {
      case NS_ACTIVATE:
        pluginEvent.event = WM_SETFOCUS;
        break;
      case NS_DEACTIVATE:
        pluginEvent.event = WM_KILLFOCUS;
        break;
      case NS_PLUGIN_ACTIVATE:
        pluginEvent.event = WM_KILLFOCUS;
        break;
      default:
        break;
    }

    event.nativeMsg = (void *)&pluginEvent;

    return DispatchWindowEvent(&event);
  }
  return PR_FALSE;
}

PRBool nsWindow::IsTopLevelMouseExit(HWND aWnd)
{
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = GET_X_LPARAM(pos);
  mp.y = GET_Y_LPARAM(pos);
  HWND mouseWnd = ::WindowFromPoint(mp);

  // GetTopLevelHWND will return a HWND for the window frame (which includes
  // the non-client area).  If the mouse has moved into the non-client area,
  // we should treat it as a top-level exit.
  HWND mouseTopLevel = nsWindow::GetTopLevelHWND(mouseWnd);
  if (mouseWnd == mouseTopLevel)
    return PR_TRUE;

  return nsWindow::GetTopLevelHWND(aWnd) != mouseTopLevel;
}

PRBool nsWindow::BlurEventsSuppressed()
{
  // are they suppressed in this window?
  if (mBlurSuppressLevel > 0)
    return PR_TRUE;

  // are they suppressed by any container widget?
  HWND parentWnd = ::GetParent(mWnd);
  if (parentWnd) {
    nsWindow *parent = GetNSWindowPtr(parentWnd);
    if (parent)
      return parent->BlurEventsSuppressed();
  }
  return PR_FALSE;
}

// In some circumstances (opening dependent windows) it makes more sense
// (and fixes a crash bug) to not blur the parent window. Called from
// nsFilePicker.
void nsWindow::SuppressBlurEvents(PRBool aSuppress)
{
  if (aSuppress)
    ++mBlurSuppressLevel; // for this widget
  else {
    NS_ASSERTION(mBlurSuppressLevel > 0, "unbalanced blur event suppression");
    if (mBlurSuppressLevel > 0)
      --mBlurSuppressLevel;
  }
}

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  return aStatus == nsEventStatus_eConsumeNoDefault;
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Native events
 **
 ** Main Windows message handlers and OnXXX handlers for
 ** Windows event handling.
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: Wind proc.
 *
 * The main Windows event procedures and associated
 * message processing methods.
 *
 **************************************************************/

// The WndProc procedure for all nsWindows in this toolkit
LRESULT CALLBACK nsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  // create this here so that we store the last rolled up popup until after
  // the event has been processed.
  nsAutoRollup autoRollup;

  LRESULT popupHandlingResult;
  if ( DealWithPopups(hWnd, msg, wParam, lParam, &popupHandlingResult) )
    return popupHandlingResult;

  // Get the window which caused the event and ask it to process the message
  nsWindow *someWindow = GetNSWindowPtr(hWnd);

  // XXX This fixes 50208 and we are leaving 51174 open to further investigate
  // why we are hitting this assert
  if (nsnull == someWindow) {
    NS_ASSERTION(someWindow, "someWindow is null, cannot call any CallWindowProc");
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
  }

  // hold on to the window for the life of this method, in case it gets
  // deleted during processing. yes, it's a double hack, since someWindow
  // is not really an interface.
  nsCOMPtr<nsISupports> kungFuDeathGrip;
  if (!someWindow->mInDtor) // not if we're in the destructor!
    kungFuDeathGrip = do_QueryInterface((nsBaseWidget*)someWindow);

  // Re-direct a tab change message destined for its parent window to the
  // the actual window which generated the event.
  if (msg == WM_NOTIFY) {
    LPNMHDR pnmh = (LPNMHDR) lParam;
    if (pnmh->code == TCN_SELCHANGE) {
      someWindow = GetNSWindowPtr(pnmh->hwndFrom);
    }
  }

  // Call ProcessMessage
  if (nsnull != someWindow) {
    LRESULT retValue;
    if (PR_TRUE == someWindow->ProcessMessage(msg, wParam, lParam, &retValue)) {
      return retValue;
    }
  }

  return ::CallWindowProcW(someWindow->GetPrevWindowProc(),
                           hWnd, msg, wParam, lParam);
}

// The main windows message processing method for plugins.
// The result means whether this method processed the native
// event for plugin. If false, the native event should be
// processed by the caller self.
PRBool
nsWindow::ProcessMessageForPlugin(const MSG &aMsg,
                                  LRESULT *aResult,
                                  PRBool &aCallDefWndProc)
{
  NS_PRECONDITION(aResult, "aResult must be non-null.");
  *aResult = 0;

  aCallDefWndProc = PR_FALSE;
  PRBool fallBackToNonPluginProcess = PR_FALSE;
  PRBool eventDispatched = PR_FALSE;
  PRBool dispatchPendingEvents = PR_TRUE;
  switch (aMsg.message) {
    case WM_INPUTLANGCHANGEREQUEST:
    case WM_INPUTLANGCHANGE:
      DispatchPluginEvent(aMsg);
      return PR_FALSE; // go to non-plug-ins processing

    case WM_CHAR:
    case WM_SYSCHAR:
      *aResult = ProcessCharMessage(aMsg, &eventDispatched);
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      *aResult = ProcessKeyUpMessage(aMsg, &eventDispatched);
      break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      *aResult = ProcessKeyDownMessage(aMsg, &eventDispatched);
      break;

    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
    case WM_CONTEXTMENU:

    case WM_CUT:
    case WM_COPY:
    case WM_PASTE:
    case WM_CLEAR:
    case WM_UNDO:

    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_CHAR:
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_CONTROL:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
    case WM_IME_NOTIFY:
    case WM_IME_REQUEST:
    case WM_IME_SELECT:
      break;

    case WM_IME_SETCONTEXT:
      // Don't synchronously dispatch when we receive WM_IME_SETCONTEXT
      // because we get it during plugin destruction. (bug 491848)
      dispatchPendingEvents = PR_FALSE;
      break;

    default:
      return PR_FALSE;
  }

  if (!eventDispatched)
    aCallDefWndProc = !DispatchPluginEvent(aMsg);
  if (dispatchPendingEvents)
    DispatchPendingEvents();
  return PR_TRUE;
}

// The main windows message processing method.
PRBool nsWindow::ProcessMessage(UINT msg, WPARAM &wParam, LPARAM &lParam,
                                LRESULT *aRetValue)
{
  // (Large blocks of code should be broken out into OnEvent handlers.)
  if (mWindowHook.Notify(mWnd, msg, wParam, lParam, aRetValue))
    return PR_TRUE;
  
  PRBool eatMessage;
  if (nsIMM32Handler::ProcessMessage(this, msg, wParam, lParam, aRetValue,
                                     eatMessage)) {
    return mWnd ? eatMessage : PR_TRUE;
  }

  if (PluginHasFocus()) {
    PRBool callDefaultWndProc;
    MSG nativeMsg = InitMSG(msg, wParam, lParam);
    if (ProcessMessageForPlugin(nativeMsg, aRetValue, callDefaultWndProc)) {
      return mWnd ? !callDefaultWndProc : PR_TRUE;
    }
  }

  static UINT vkKeyCached = 0; // caches VK code fon WM_KEYDOWN
  PRBool result = PR_FALSE;    // call the default nsWindow proc
  *aRetValue = 0;

#if !defined (WINCE_WINDOWS_MOBILE)
  static PRBool getWheelInfo = PR_TRUE;
#endif

#if defined(EVENT_DEBUG_OUTPUT)
  // First param shows all events, second param indicates whether
  // to show mouse move events. See nsWindowDbg for details.
  PrintEvent(msg, SHOW_REPEAT_EVENTS, SHOW_MOUSEMOVE_EVENTS);
#endif

  switch (msg) {
    case WM_COMMAND:
    {
      WORD wNotifyCode = HIWORD(wParam); // notification code
      if ((CBN_SELENDOK == wNotifyCode) || (CBN_SELENDCANCEL == wNotifyCode)) { // Combo box change
        nsGUIEvent event(PR_TRUE, NS_CONTROL_CHANGE, this);
        nsIntPoint point(0,0);
        InitEvent(event, &point); // this add ref's event.widget
        result = DispatchWindowEvent(&event);
      } else if (wNotifyCode == 0) { // Menu selection
        nsMenuEvent event(PR_TRUE, NS_MENU_SELECTED, this);
        event.mCommand = LOWORD(wParam);
        InitEvent(event);
        result = DispatchWindowEvent(&event);
      }
    }
    break;

#ifndef WINCE
    // WM_QUERYENDSESSION must be handled by all windows.
    // Otherwise Windows thinks the window can just be killed at will.
    case WM_QUERYENDSESSION:
      if (sCanQuit == TRI_UNKNOWN)
      {
        // Ask if it's ok to quit, and store the answer until we
        // get WM_ENDSESSION signaling the round is complete.
        nsCOMPtr<nsIObserverService> obsServ =
          do_GetService("@mozilla.org/observer-service;1");
        nsCOMPtr<nsISupportsPRBool> cancelQuit =
          do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);
        cancelQuit->SetData(PR_FALSE);
        obsServ->NotifyObservers(cancelQuit, "quit-application-requested", nsnull);

        PRBool abortQuit;
        cancelQuit->GetData(&abortQuit);
        sCanQuit = abortQuit ? TRI_FALSE : TRI_TRUE;
      }
      *aRetValue = sCanQuit ? TRUE : FALSE;
      result = PR_TRUE;
      break;

    case WM_ENDSESSION:
      if (wParam == TRUE && sCanQuit == TRI_TRUE)
      {
        // Let's fake a shutdown sequence without actually closing windows etc.
        // to avoid Windows killing us in the middle. A proper shutdown would
        // require having a chance to pump some messages. Unfortunately
        // Windows won't let us do that. Bug 212316.
        nsCOMPtr<nsIObserverService> obsServ =
          do_GetService("@mozilla.org/observer-service;1");
        NS_NAMED_LITERAL_STRING(context, "shutdown-persist");
        obsServ->NotifyObservers(nsnull, "quit-application-granted", nsnull);
        obsServ->NotifyObservers(nsnull, "quit-application-forced", nsnull);
        obsServ->NotifyObservers(nsnull, "quit-application", nsnull);
        obsServ->NotifyObservers(nsnull, "profile-change-net-teardown", context.get());
        obsServ->NotifyObservers(nsnull, "profile-change-teardown", context.get());
        obsServ->NotifyObservers(nsnull, "profile-before-change", context.get());
        // Then a controlled but very quick exit.
        _exit(0);
      }
      sCanQuit = TRI_UNKNOWN;
      result = PR_TRUE;
      break;

    case WM_DISPLAYCHANGE:
      DispatchStandardEvent(NS_DISPLAYCHANGED);
      break;
#endif

    case WM_SYSCOLORCHANGE:
      // Note: This is sent for child windows as well as top-level windows.
      // The Win32 toolkit normally only sends these events to top-level windows.
      // But we cycle through all of the childwindows and send it to them as well
      // so all presentations get notified properly.
      // See nsWindow::GlobalMsgWindowProc.
      DispatchStandardEvent(NS_SYSCOLORCHANGED);
      break;

    case WM_NOTIFY:
      // TAB change
    {
      LPNMHDR pnmh = (LPNMHDR) lParam;

        switch (pnmh->code) {
          case TCN_SELCHANGE:
          {
            DispatchStandardEvent(NS_TABCHANGE);
            result = PR_TRUE;
          }
          break;
        }
    }
    break;

    case WM_XP_THEMECHANGED:
    {
      DispatchStandardEvent(NS_THEMECHANGED);

      // Invalidate the window so that the repaint will
      // pick up the new theme.
      Invalidate(PR_FALSE);
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
          // Changing nsIPrefBranch entry which triggers callbacks
          // and flows into calling mDeviceContext->FlushFontCache()
          // to update the font cache in all the instance of Browsers
          nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
          if (prefs) {
            nsCOMPtr<nsIPrefBranch> fiPrefs;
            prefs->GetBranch("font.internaluseonly.", getter_AddRefs(fiPrefs));
            if (fiPrefs) {
              PRBool fontInternalChange = PR_FALSE;
              fiPrefs->GetBoolPref("changed", &fontInternalChange);
              fiPrefs->SetBoolPref("changed", !fontInternalChange);
            }
          }
        }
      } //if (NS_SUCCEEDED(rv))
    }
    break;

#ifndef WINCE
    case WM_POWERBROADCAST:
      // only hidden window handle this
      // to prevent duplicate notification
      if (mWindowType == eWindowType_invisible) {
        switch (wParam)
        {
          case PBT_APMSUSPEND:
            PostSleepWakeNotification("sleep_notification");
            break;
          case PBT_APMRESUMEAUTOMATIC:
          case PBT_APMRESUMECRITICAL:
          case PBT_APMRESUMESUSPEND:
            PostSleepWakeNotification("wake_notification");
            break;
        }
      }
      break;
#endif

    case WM_MOVE: // Window moved
    {
      PRInt32 x = GET_X_LPARAM(lParam); // horizontal position in screen coordinates
      PRInt32 y = GET_Y_LPARAM(lParam); // vertical position in screen coordinates
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
      *aRetValue = (int) OnPaint();
      result = PR_TRUE;
      break;

#ifndef WINCE
    case WM_PRINTCLIENT:
      result = OnPaint((HDC) wParam);
      break;
#endif

    case WM_HOTKEY:
      result = OnHotKey(wParam, lParam);
      break;

    case WM_SYSCHAR:
    case WM_CHAR:
    {
      MSG nativeMsg = InitMSG(msg, wParam, lParam);
      result = ProcessCharMessage(nativeMsg, nsnull);
      DispatchPendingEvents();
    }
    break;

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
      MSG nativeMsg = InitMSG(msg, wParam, lParam);
      result = ProcessKeyUpMessage(nativeMsg, nsnull);
      DispatchPendingEvents();
    }
    break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
      MSG nativeMsg = InitMSG(msg, wParam, lParam);
      result = ProcessKeyDownMessage(nativeMsg, nsnull);
      DispatchPendingEvents();
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
    {
      // Suppress dispatch of pending events
      // when mouse moves are generated by widget
      // creation instead of user input.
      LPARAM lParamScreen = lParamToScreen(lParam);
      POINT mp;
      mp.x      = GET_X_LPARAM(lParamScreen);
      mp.y      = GET_Y_LPARAM(lParamScreen);
      PRBool userMovedMouse = PR_FALSE;
      if ((sLastMouseMovePoint.x != mp.x) || (sLastMouseMovePoint.y != mp.y)) {
        userMovedMouse = PR_TRUE;
      }

      result = DispatchMouseEvent(NS_MOUSE_MOVE, wParam, lParam);
      if (userMovedMouse) {
        DispatchPendingEvents();
      }
    }
    break;

    case WM_LBUTTONDOWN:
    {
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam,
                                  PR_FALSE, nsMouseEvent::eLeftButton);
      DispatchPendingEvents();
    }
    break;

    case WM_LBUTTONUP:
    {
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam,
                                  PR_FALSE, nsMouseEvent::eLeftButton);
      DispatchPendingEvents();
    }
    break;

#ifndef WINCE
    case WM_MOUSELEAVE:
    {
      // We need to check mouse button states and put them in for
      // wParam.
      WPARAM mouseState = (GetKeyState(VK_LBUTTON) ? MK_LBUTTON : 0)
        | (GetKeyState(VK_MBUTTON) ? MK_MBUTTON : 0)
        | (GetKeyState(VK_RBUTTON) ? MK_RBUTTON : 0);
      // Synthesize an event position because we don't get one from
      // WM_MOUSELEAVE.
      LPARAM pos = lParamToClient(::GetMessagePos());
      DispatchMouseEvent(NS_MOUSE_EXIT, mouseState, pos);
    }
    break;
#endif

    case WM_CONTEXTMENU:
    {
      // if the context menu is brought up from the keyboard, |lParam|
      // will be maxlong.
      LPARAM pos;
      PRBool contextMenukey = PR_FALSE;
      if (lParam == 0xFFFFFFFF)
      {
        contextMenukey = PR_TRUE;
        pos = lParamToClient(GetMessagePos());
      }
      else
      {
        pos = lParamToClient(lParam);
      }
      result = DispatchMouseEvent(NS_CONTEXTMENU, wParam, pos, contextMenukey,
                                  contextMenukey ?
                                    nsMouseEvent::eLeftButton :
                                    nsMouseEvent::eRightButton);
    }
    break;

    case WM_LBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eLeftButton);
      break;

    case WM_MBUTTONDOWN:
    {
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eMiddleButton);
      DispatchPendingEvents();
    }
    break;

    case WM_MBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eMiddleButton);
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eMiddleButton);
      break;

    case WM_RBUTTONDOWN:
    {
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eRightButton);
      DispatchPendingEvents();
    }
    break;

    case WM_RBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eRightButton);
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eRightButton);
      break;

    case WM_APPCOMMAND:
    {
      PRUint32 appCommand = GET_APPCOMMAND_LPARAM(lParam);

      switch (appCommand)
      {
        case APPCOMMAND_BROWSER_BACKWARD:
        case APPCOMMAND_BROWSER_FORWARD:
        case APPCOMMAND_BROWSER_REFRESH:
        case APPCOMMAND_BROWSER_STOP:
        case APPCOMMAND_BROWSER_SEARCH:
        case APPCOMMAND_BROWSER_FAVORITES:
        case APPCOMMAND_BROWSER_HOME:
          DispatchCommandEvent(appCommand);
          // tell the driver that we handled the event
          *aRetValue = 1;
          result = PR_TRUE;
          break;
      }
      // default = PR_FALSE - tell the driver that the event was not handled
    }
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
    //case WM_CTLCOLORSCROLLBAR: //XXX causes the scrollbar to be drawn incorrectly
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

    // The WM_ACTIVATE event is fired when a window is raised or lowered,
    // and the loword of wParam specifies which. But we don't want to tell
    // the focus system about this until the WM_SETFOCUS or WM_KILLFOCUS
    // events are fired. Instead, set either the sJustGotActivate or
    // gJustGotDeativate flags and fire the NS_ACTIVATE or NS_DEACTIVATE
    // events once the focus events arrive.
    case WM_ACTIVATE:
      if (mEventCallback) {
        PRInt32 fActive = LOWORD(wParam);

#if defined(WINCE_HAVE_SOFTKB)
        if (mIsTopWidgetWindow && sSoftKeyboardState)
          nsWindowCE::ToggleSoftKB(fActive);
        if (nsWindowCE::sShowSIPButton != TRI_TRUE && WA_INACTIVE != fActive) {
          HWND hWndSIPB = FindWindowW(L"MS_SIPBUTTON", NULL ); 
          if (hWndSIPB)
            ShowWindow(hWndSIPB, SW_HIDE);
        }

#endif

        if (WA_INACTIVE == fActive) {
          // when minimizing a window, the deactivation and focus events will
          // be fired in the reverse order. Instead, just dispatch
          // NS_DEACTIVATE right away.
          if (HIWORD(wParam))
            result = DispatchFocusToTopLevelWindow(NS_DEACTIVATE);
          else
            sJustGotDeactivate = PR_TRUE;
#ifndef WINCE
          if (mIsTopWidgetWindow)
            mLastKeyboardLayout = gKbdLayout.GetLayout();
#endif

        } else {
          StopFlashing();

          sJustGotActivate = PR_TRUE;
          nsMouseEvent event(PR_TRUE, NS_MOUSE_ACTIVATE, this,
                             nsMouseEvent::eReal);
          InitEvent(event);

          event.acceptActivation = PR_TRUE;
  
          PRBool result = DispatchWindowEvent(&event);
#ifndef WINCE
          if (event.acceptActivation)
            *aRetValue = MA_ACTIVATE;
          else
            *aRetValue = MA_NOACTIVATE;

          if (sSwitchKeyboardLayout && mLastKeyboardLayout)
            ActivateKeyboardLayout(mLastKeyboardLayout, 0);
#else
          *aRetValue = 0;
#endif
          if (mSizeMode == nsSizeMode_Fullscreen)
            MakeFullScreen(TRUE);
        }
      }
      break;

#ifndef WINCE
    case WM_MOUSEACTIVATE:
      if (mWindowType == eWindowType_popup) {
        // a popup with a parent owner should not be activated when clicked
        // but should still allow the mouse event to be fired, so the return
        // value is set to MA_NOACTIVATE. But if the owner isn't the frontmost
        // window, just use default processing so that the window is activated.
        HWND owner = ::GetWindow(mWnd, GW_OWNER);
        if (owner && owner == ::GetForegroundWindow()) {
          *aRetValue = MA_NOACTIVATE;
          result = PR_TRUE;
        }
      }
      break;

    case WM_WINDOWPOSCHANGING:
    {
      LPWINDOWPOS info = (LPWINDOWPOS) lParam;
      OnWindowPosChanging(info);
    }
    break;
#endif

    case WM_SETFOCUS:
      if (sJustGotActivate) {
        result = DispatchFocusToTopLevelWindow(NS_ACTIVATE);
      }

#ifdef ACCESSIBILITY
      if (nsWindow::sIsAccessibilityOn) {
        // Create it for the first time so that it can start firing events
        nsCOMPtr<nsIAccessible> rootAccessible = GetRootAccessible();
      }
#endif

#if defined(WINCE_HAVE_SOFTKB)
      {
        // On Windows CE, we have a window that overlaps
        // the ISP button.  In this case, we should always
        // try to hide it when we are activated
      
        nsIMEContext IMEContext(mWnd);
        // Open the IME 
        ImmSetOpenStatus(IMEContext.get(), TRUE);
      }
#endif
      break;

    case WM_KILLFOCUS:
#if defined(WINCE_HAVE_SOFTKB)
      {
        nsIMEContext IMEContext(mWnd);
        ImmSetOpenStatus(IMEContext.get(), FALSE);
      }
#endif
      if (sJustGotDeactivate) {
        result = DispatchFocusToTopLevelWindow(NS_DEACTIVATE);
      }
      break;

    case WM_WINDOWPOSCHANGED:
    {
      WINDOWPOS *wp = (LPWINDOWPOS)lParam;
      OnWindowPosChanged(wp, result);
    }
    break;

    case WM_SETTINGCHANGE:
#if !defined (WINCE_WINDOWS_MOBILE)
      getWheelInfo = PR_TRUE;
#endif
      OnSettingsChange(wParam, lParam);
      break;

#ifndef WINCE
    case WM_INPUTLANGCHANGEREQUEST:
      *aRetValue = TRUE;
      result = PR_FALSE;
      break;

    case WM_INPUTLANGCHANGE:
      result = OnInputLangChange((HKL)lParam);
      break;
#endif // WINCE

    case WM_DESTROYCLIPBOARD:
    {
      nsIClipboard* clipboard;
      nsresult rv = CallGetService(kCClipboardCID, &clipboard);
      clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);
      NS_RELEASE(clipboard);
    }
    break;

#ifdef ACCESSIBILITY
    case WM_GETOBJECT:
    {
      *aRetValue = 0;
      if (lParam == OBJID_CLIENT) { // oleacc.dll will be loaded dynamically
        nsCOMPtr<nsIAccessible> rootAccessible = GetRootAccessible(); // Held by a11y cache
        if (rootAccessible) {
          IAccessible *msaaAccessible = NULL;
          rootAccessible->GetNativeInterface((void**)&msaaAccessible); // does an addref
          if (msaaAccessible) {
            *aRetValue = LresultFromObject(IID_IAccessible, wParam, msaaAccessible); // does an addref
            msaaAccessible->Release(); // release extra addref
            result = PR_TRUE;  // We handled the WM_GETOBJECT message
          }
        }
      }
    }
#endif

#ifndef WINCE
    case WM_SYSCOMMAND:
      // prevent Windows from trimming the working set. bug 76831
      if (!sTrimOnMinimize && wParam == SC_MINIMIZE) {
        ::ShowWindow(mWnd, SW_SHOWMINIMIZED);
        result = PR_TRUE;
      }
      break;
#endif


#ifdef WINCE
  case WM_HIBERNATE:        
    nsMemory::HeapMinimize(PR_TRUE);
    break;
#endif

#if !defined (WINCE_WINDOWS_MOBILE)
  case WM_MOUSEWHEEL:
  case WM_MOUSEHWHEEL:
    {
      // If OnMouseWheel returns true, the event was forwarded directly to another
      // mozilla window message handler (ProcessMessage). In this case the return
      // value of the forwarded event is in 'result' which we should return immediately.
      // If OnMouseWheel returns false, OnMouseWheel processed the event internally.
      // 'result' and 'aRetValue' will be set based on what we did with the event, so
      // we should fall through.
      if (OnMouseWheel(msg, wParam, lParam, getWheelInfo, result, aRetValue))
        return result;
    }
    break;
#endif

#ifndef WINCE
  case WM_DWMCOMPOSITIONCHANGED:
    BroadcastMsg(mWnd, WM_DWMCOMPOSITIONCHANGED);
    DispatchStandardEvent(NS_THEMECHANGED);
    if (nsUXThemeData::CheckForCompositor() && mTransparencyMode == eTransparencyGlass) {
      MARGINS margins = { -1, -1, -1, -1 };
      nsUXThemeData::dwmExtendFrameIntoClientAreaPtr(mWnd, &margins);
    }
    Invalidate(PR_FALSE);
    break;
#endif

#if !defined(WINCE)
  /* Gesture support events */
  case WM_TABLET_QUERYSYSTEMGESTURESTATUS:
    // According to MS samples, this must be handled to enable
    // rotational support in multi-touch drivers.
    result = PR_TRUE;
    *aRetValue = TABLET_ROTATE_GESTURE_ENABLE;
    break;
    
  case WM_GESTURE:
    result = OnGesture(wParam, lParam);
    break;

  case WM_GESTURENOTIFY:
    {
      if (mWindowType != eWindowType_invisible &&
          mWindowType != eWindowType_plugin &&
          mWindowType != eWindowType_java &&
          mWindowType != eWindowType_toplevel) {
        // eWindowType_toplevel is the top level main frame window. Gesture support
        // there prevents the user from interacting with the title bar or nc
        // areas using a single finger. Java and plugin windows can make their
        // own calls.
        GESTURENOTIFYSTRUCT * gestureinfo = (GESTURENOTIFYSTRUCT*)lParam;
        nsPointWin touchPoint;
        touchPoint = gestureinfo->ptsLocation;
        touchPoint.ScreenToClient(mWnd);
        nsGestureNotifyEvent gestureNotifyEvent(PR_TRUE, NS_GESTURENOTIFY_EVENT_START, this);
        gestureNotifyEvent.refPoint = touchPoint;
        nsEventStatus status;
        DispatchEvent(&gestureNotifyEvent, status);
        mDisplayPanFeedback = gestureNotifyEvent.displayPanFeedback;
        mGesture.SetWinGestureSupport(mWnd, gestureNotifyEvent.panDirection);
      }
      result = PR_FALSE; //should always bubble to DefWindowProc
    }
    break;
#endif // !defined(WINCE)

    case WM_CLEAR:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_DELETE, this);
      DispatchWindowEvent(&command);
      result = PR_TRUE;
    }
    break;

    case WM_CUT:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_CUT, this);
      DispatchWindowEvent(&command);
      result = PR_TRUE;
    }
    break;

    case WM_COPY:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_COPY, this);
      DispatchWindowEvent(&command);
      result = PR_TRUE;
    }
    break;

    case WM_PASTE:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_PASTE, this);
      DispatchWindowEvent(&command);
      result = PR_TRUE;
    }
    break;

#ifndef WINCE
    case EM_UNDO:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_UNDO, this);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = PR_TRUE;
    }
    break;

    case EM_REDO:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_REDO, this);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = PR_TRUE;
    }
    break;

    case EM_CANPASTE:
    {
      // Support EM_CANPASTE message only when wParam isn't specified or
      // is plain text format.
      if (wParam == 0 || wParam == CF_TEXT || wParam == CF_UNICODETEXT) {
        nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_PASTE,
                                      this, PR_TRUE);
        DispatchWindowEvent(&command);
        *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
        result = PR_TRUE;
      }
    }
    break;

    case EM_CANUNDO:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_UNDO,
                                    this, PR_TRUE);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = PR_TRUE;
    }
    break;

    case EM_CANREDO:
    {
      nsContentCommandEvent command(PR_TRUE, NS_CONTENT_COMMAND_REDO,
                                    this, PR_TRUE);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = PR_TRUE;
    }
    break;
#endif

    default:
    {
#ifdef NS_ENABLE_TSF
      if (msg == WM_USER_TSF_TEXTCHANGE) {
        nsTextStore::OnTextChangeMsg();
      }
#endif //NS_ENABLE_TSF
#if defined(HEAP_DUMP_EVENT)
      if (msg == GetHeapMsg()) {
        HeapDump(msg, wParam, lParam);
        result = PR_TRUE;
      }
#endif
    }
    break;
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

/**************************************************************
 *
 * SECTION: Broadcast messaging
 *
 * Broadcast messages to all windows.
 *
 **************************************************************/

// Enumerate all child windows sending aMsg to each of them
BOOL CALLBACK nsWindow::BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg)
{
  WNDPROC winProc = (WNDPROC)::GetWindowLongPtrW(aWnd, GWLP_WNDPROC);
  if (winProc == &nsWindow::WindowProc) {
    // it's one of our windows so go ahead and send a message to it
    ::CallWindowProcW(winProc, aWnd, aMsg, 0, 0);
  }
  return TRUE;
}

// Enumerate all top level windows specifying that the children of each
// top level window should be enumerated. Do *not* send the message to
// each top level window since it is assumed that the toolkit will send
// aMsg to them directly.
BOOL CALLBACK nsWindow::BroadcastMsg(HWND aTopWindow, LPARAM aMsg)
{
  // Iterate each of aTopWindows child windows sending the aMsg
  // to each of them.
#if !defined(WINCE)
  ::EnumChildWindows(aTopWindow, nsWindow::BroadcastMsgToChildren, aMsg);
#else
  nsWindowCE::EnumChildWindows(aTopWindow, nsWindow::BroadcastMsgToChildren, aMsg);
#endif
  return TRUE;
}

// This method is called from nsToolkit::WindowProc to forward global
// messages which need to be dispatched to all child windows.
void nsWindow::GlobalMsgWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    case WM_SYSCOLORCHANGE:
      // Code to dispatch WM_SYSCOLORCHANGE message to all child windows.
      // WM_SYSCOLORCHANGE is only sent to top-level windows, but the
      // cross platform API requires that NS_SYSCOLORCHANGE message be sent to
      // all child windows as well. When running in an embedded application
      // we may not receive a WM_SYSCOLORCHANGE message because the top
      // level window is owned by the embeddor.
      // System color changes are posted to top-level windows only.
      // The NS_SYSCOLORCHANGE must be dispatched to all child
      // windows as well.
#if !defined(WINCE)
     ::EnumThreadWindows(GetCurrentThreadId(), nsWindow::BroadcastMsg, msg);
#endif
    break;
  }
}

/**************************************************************
 *
 * SECTION: Event processing helpers
 *
 * Special processing for certain event types and 
 * synthesized events.
 *
 **************************************************************/

#ifndef WINCE
void nsWindow::PostSleepWakeNotification(const char* aNotification)
{
  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1");
  if (observerService)
  {
    observerService->NotifyObservers(nsnull, aNotification, nsnull);
  }
}
#endif

LRESULT nsWindow::ProcessCharMessage(const MSG &aMsg, PRBool *aEventDispatched)
{
  NS_PRECONDITION(aMsg.message == WM_CHAR || aMsg.message == WM_SYSCHAR,
                  "message is not keydown event");
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("%s charCode=%d scanCode=%d\n",
         aMsg.message == WM_SYSCHAR ? "WM_SYSCHAR" : "WM_CHAR",
         aMsg.wParam, HIWORD(aMsg.lParam) & 0xFF));

  // These must be checked here too as a lone WM_CHAR could be received
  // if a child window didn't handle it (for example Alt+Space in a content window)
  nsModifierKeyState modKeyState;
  return OnChar(aMsg, modKeyState, aEventDispatched);
}

LRESULT nsWindow::ProcessKeyUpMessage(const MSG &aMsg, PRBool *aEventDispatched)
{
  NS_PRECONDITION(aMsg.message == WM_KEYUP || aMsg.message == WM_SYSKEYUP,
                  "message is not keydown event");
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("%s VK=%d\n", aMsg.message == WM_SYSKEYDOWN ?
                          "WM_SYSKEYUP" : "WM_KEYUP", aMsg.wParam));

  nsModifierKeyState modKeyState;

  // Note: the original code passed (HIWORD(lParam)) to OnKeyUp as
  // scan code. However, this breaks Alt+Num pad input.
  // MSDN states the following:
  //  Typically, ToAscii performs the translation based on the
  //  virtual-key code. In some cases, however, bit 15 of the
  //  uScanCode parameter may be used to distinguish between a key
  //  press and a key release. The scan code is used for
  //  translating ALT+number key combinations.

  // ignore [shift+]alt+space so the OS can handle it
  if (modKeyState.mIsAltDown && !modKeyState.mIsControlDown &&
      IS_VK_DOWN(NS_VK_SPACE)) {
    return FALSE;
  }

  if (!nsIMM32Handler::IsComposing(this) &&
      (aMsg.message != WM_KEYUP || aMsg.message != VK_MENU)) {
    // Ignore VK_MENU if it's not a system key release, so that the menu bar does not trigger
    // This helps avoid triggering the menu bar for ALT key accelerators used in
    // assistive technologies such as Window-Eyes and ZoomText, and when using Alt+Tab
    // to switch back to Mozilla in Windows 95 and Windows 98
    return OnKeyUp(aMsg, modKeyState, aEventDispatched);
  }

  return 0;
}

LRESULT nsWindow::ProcessKeyDownMessage(const MSG &aMsg,
                                        PRBool *aEventDispatched)
{
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("%s VK=%d\n", aMsg.message == WM_SYSKEYDOWN ?
                          "WM_SYSKEYDOWN" : "WM_KEYDOWN", aMsg.wParam));
  NS_PRECONDITION(aMsg.message == WM_KEYDOWN || aMsg.message == WM_SYSKEYDOWN,
                  "message is not keydown event");

  nsModifierKeyState modKeyState;

  // Note: the original code passed (HIWORD(lParam)) to OnKeyDown as
  // scan code. However, this breaks Alt+Num pad input.
  // MSDN states the following:
  //  Typically, ToAscii performs the translation based on the
  //  virtual-key code. In some cases, however, bit 15 of the
  //  uScanCode parameter may be used to distinguish between a key
  //  press and a key release. The scan code is used for
  //  translating ALT+number key combinations.

  // ignore [shift+]alt+space so the OS can handle it
  if (modKeyState.mIsAltDown && !modKeyState.mIsControlDown &&
      IS_VK_DOWN(NS_VK_SPACE))
    return FALSE;

  LRESULT result = 0;
  if (modKeyState.mIsAltDown && nsIMM32Handler::IsStatusChanged()) {
    nsIMM32Handler::NotifyEndStatusChange();
  } else if (!nsIMM32Handler::IsComposing(this)) {
    result = OnKeyDown(aMsg, modKeyState, aEventDispatched, nsnull);
  }

#ifndef WINCE
  if (aMsg.wParam == VK_MENU ||
      (aMsg.wParam == VK_F10 && !modKeyState.mIsShiftDown)) {
    // We need to let Windows handle this keypress,
    // by returning PR_FALSE, if there's a native menu
    // bar somewhere in our containing window hierarchy.
    // Otherwise we handle the keypress and don't pass
    // it on to Windows, by returning PR_TRUE.
    PRBool hasNativeMenu = PR_FALSE;
    HWND hWnd = mWnd;
    while (hWnd) {
      if (::GetMenu(hWnd)) {
        hasNativeMenu = PR_TRUE;
        break;
      }
      hWnd = ::GetParent(hWnd);
    }
    result = !hasNativeMenu;
  }
#endif

  return result;
}

nsresult
nsWindow::SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                   PRInt32 aNativeKeyCode,
                                   PRUint32 aModifierFlags,
                                   const nsAString& aCharacters,
                                   const nsAString& aUnmodifiedCharacters)
{
#ifndef WINCE  //Win CE doesn't support many of the calls used in this method, perhaps theres another way
  nsPrintfCString layoutName("%08x", aNativeKeyboardLayout);
  HKL loadedLayout = LoadKeyboardLayoutA(layoutName.get(), KLF_NOTELLSHELL);
  if (loadedLayout == NULL)
    return NS_ERROR_NOT_AVAILABLE;

  // Setup clean key state and load desired layout
  BYTE originalKbdState[256];
  ::GetKeyboardState(originalKbdState);
  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));
  // This changes the state of the keyboard for the current thread only,
  // and we'll restore it soon, so this should be OK.
  ::SetKeyboardState(kbdState);
  HKL oldLayout = gKbdLayout.GetLayout();
  gKbdLayout.LoadLayout(loadedLayout);

  nsAutoTArray<KeyPair,10> keySequence;
  SetupKeyModifiersSequence(&keySequence, aModifierFlags);
  NS_ASSERTION(aNativeKeyCode >= 0 && aNativeKeyCode < 256,
               "Native VK key code out of range");
  keySequence.AppendElement(KeyPair(aNativeKeyCode, 0));

  // Simulate the pressing of each modifier key and then the real key
  for (PRUint32 i = 0; i < keySequence.Length(); ++i) {
    PRUint8 key = keySequence[i].mGeneral;
    PRUint8 keySpecific = keySequence[i].mSpecific;
    kbdState[key] = 0x81; // key is down and toggled on if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0x81;
    }
    ::SetKeyboardState(kbdState);
    nsModifierKeyState modKeyState;
    MSG msg = InitMSG(WM_KEYDOWN, key, 0);
    if (i == keySequence.Length() - 1 && aCharacters.Length() > 0) {
      UINT scanCode = ::MapVirtualKeyEx(aNativeKeyCode, MAPVK_VK_TO_VSC,
                                        gKbdLayout.GetLayout());
      nsFakeCharMessage fakeMsg = { aCharacters.CharAt(0), scanCode };
      OnKeyDown(msg, modKeyState, nsnull, &fakeMsg);
    } else {
      OnKeyDown(msg, modKeyState, nsnull, nsnull);
    }
  }
  for (PRUint32 i = keySequence.Length(); i > 0; --i) {
    PRUint8 key = keySequence[i - 1].mGeneral;
    PRUint8 keySpecific = keySequence[i - 1].mSpecific;
    kbdState[key] = 0; // key is up and toggled off if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0;
    }
    ::SetKeyboardState(kbdState);
    nsModifierKeyState modKeyState;
    MSG msg = InitMSG(WM_KEYUP, key, 0);
    OnKeyUp(msg, modKeyState, nsnull);
  }

  // Restore old key state and layout
  ::SetKeyboardState(originalKbdState);
  gKbdLayout.LoadLayout(oldLayout);

  UnloadKeyboardLayout(loadedLayout);
  return NS_OK;
#else  //XXX: is there another way to do this?
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

/**************************************************************
 *
 * SECTION: OnXXX message handlers
 *
 * For message handlers that need to be broken out or
 * implemented in specific platform code.
 *
 **************************************************************/

BOOL nsWindow::OnInputLangChange(HKL aHKL)
{
#ifdef KE_DEBUG
  printf("OnInputLanguageChange\n");
#endif

#ifndef WINCE
  gKbdLayout.LoadLayout(aHKL);
#endif

  return PR_FALSE;   // always pass to child window
}

void nsWindow::OnWindowPosChanged(WINDOWPOS *wp, PRBool& result)
{
  if (wp == nsnull)
    return;

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
    nsIntRect rect(wp->x, wp->y, newWidth, newHeight);

#ifdef MOZ_XUL
    if (eTransparencyTransparent == mTransparencyMode)
      ResizeTranslucentWindow(newWidth, newHeight);
#endif

    if (newWidth > mLastSize.width)
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
    if (newHeight > mLastSize.height)
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

    // If we're being minimized, don't send the resize event to Gecko because
    // it will cause the scrollbar in the content area to go away and we'll
    // forget the scroll position of the page.  Note that we need to check the
    // toplevel window, because child windows seem to go to 0x0 on minimize.
    HWND toplevelWnd = GetTopLevelHWND(mWnd);
    if (mWnd == toplevelWnd && IsIconic(toplevelWnd)) {
      result = PR_FALSE;
      return;
    }

    // recalculate the width and height
    // this time based on the client area
    if (::GetClientRect(mWnd, &r)) {
      rect.width  = PRInt32(r.right - r.left);
      rect.height = PRInt32(r.bottom - r.top);
    }
    result = OnResize(rect);
  }

  // handle size mode changes - (the framechanged message seems a handy
  // place to hook in, because it happens early enough (WM_SIZE is too
  // late) and because in testing it seems an accurate harbinger of an
  // impending min/max/restore change (WM_NCCALCSIZE would also work,
  // but it's also sent when merely resizing.))
  if (wp->flags & SWP_FRAMECHANGED && ::IsWindowVisible(mWnd)) {
    nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);
#ifndef WINCE
    WINDOWPLACEMENT pl;
    pl.length = sizeof(pl);
    ::GetWindowPlacement(mWnd, &pl);

    if (pl.showCmd == SW_SHOWMAXIMIZED)
      event.mSizeMode = nsSizeMode_Maximized;
    else if (pl.showCmd == SW_SHOWMINIMIZED)
      event.mSizeMode = nsSizeMode_Minimized;
    else
      event.mSizeMode = nsSizeMode_Normal;
#else
    event.mSizeMode = mSizeMode;
#endif
    // Windows has just changed the size mode of this window. The following
    // NS_SIZEMODE event will trigger a call into SetSizeMode where we will
    // set the min/max window state again or for nsSizeMode_Normal, call
    // SetWindow with a parameter of SW_RESTORE. There's no need however as
    // this window's mode has already changed. Updating mSizeMode here
    // insures the SetSizeMode call is a no-op. Addresses a bug on Win7 related
    // to window docking. (bug 489258)
    mSizeMode = event.mSizeMode;

    InitEvent(event);

    result = DispatchWindowEvent(&event);
  }
}

#if !defined(WINCE)
void nsWindow::OnWindowPosChanging(LPWINDOWPOS& info)
{
  // enforce local z-order rules
  if (!(info->flags & SWP_NOZORDER)) {
    HWND hwndAfter = info->hwndInsertAfter;
    
    nsZLevelEvent event(PR_TRUE, NS_SETZLEVEL, this);
    nsWindow *aboveWindow = 0;

    InitEvent(event);

    if (hwndAfter == HWND_BOTTOM)
      event.mPlacement = nsWindowZBottom;
    else if (hwndAfter == HWND_TOP || hwndAfter == HWND_TOPMOST || hwndAfter == HWND_NOTOPMOST)
      event.mPlacement = nsWindowZTop;
    else {
      event.mPlacement = nsWindowZRelative;
      aboveWindow = GetNSWindowPtr(hwndAfter);
    }
    event.mReqBelow = aboveWindow;
    event.mActualBelow = nsnull;

    event.mImmediate = PR_FALSE;
    event.mAdjusted = PR_FALSE;
    DispatchWindowEvent(&event);

    if (event.mAdjusted) {
      if (event.mPlacement == nsWindowZBottom)
        info->hwndInsertAfter = HWND_BOTTOM;
      else if (event.mPlacement == nsWindowZTop)
        info->hwndInsertAfter = HWND_TOP;
      else {
        info->hwndInsertAfter = (HWND)event.mActualBelow->GetNativeData(NS_NATIVE_WINDOW);
      }
    }
    NS_IF_RELEASE(event.mActualBelow);
  }
  // prevent rude external programs from making hidden window visible
  if (mWindowType == eWindowType_invisible)
    info->flags &= ~SWP_SHOWWINDOW;
}
#endif

// Gesture event processing. Handles WM_GESTURE events.
#if !defined(WINCE)
PRBool nsWindow::OnGesture(WPARAM wParam, LPARAM lParam)
{
  // Treatment for pan events which translate into scroll events:
  if (mGesture.IsPanEvent(lParam)) {
    nsMouseScrollEvent event(PR_TRUE, NS_MOUSE_PIXEL_SCROLL, this);

    if ( !mGesture.ProcessPanMessage(mWnd, wParam, lParam) )
      return PR_FALSE; // ignore

    nsEventStatus status;

    event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
    event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
    event.isMeta    = PR_FALSE;
    event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
    event.button    = 0;
    event.time      = ::GetMessageTime();

    PRBool endFeedback = PR_TRUE;

    PRInt32 scrollOverflowX = 0;
    PRInt32 scrollOverflowY = 0;

    if (mGesture.PanDeltaToPixelScrollX(event)) {
      DispatchEvent(&event, status);
      scrollOverflowX = event.scrollOverflow;
    }

    if (mGesture.PanDeltaToPixelScrollY(event)) {
      DispatchEvent(&event, status);
      scrollOverflowY = event.scrollOverflow;
    }

    if (mDisplayPanFeedback) {
      mGesture.UpdatePanFeedbackX(mWnd, scrollOverflowX, endFeedback);
      mGesture.UpdatePanFeedbackY(mWnd, scrollOverflowY, endFeedback);
      mGesture.PanFeedbackFinalize(mWnd, endFeedback);
    }

    mGesture.CloseGestureInfoHandle((HGESTUREINFO)lParam);

    return PR_TRUE;
  }

  // Other gestures translate into simple gesture events:
  nsSimpleGestureEvent event(PR_TRUE, 0, this, 0, 0.0);
  if ( !mGesture.ProcessGestureMessage(mWnd, wParam, lParam, event) ) {
    return PR_FALSE; // fall through to DefWndProc
  }
  
  // Polish up and send off the new event
  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.button    = 0;
  event.time      = ::GetMessageTime();

  nsEventStatus status;
  DispatchEvent(&event, status);
  if (status == nsEventStatus_eIgnore) {
    return PR_FALSE; // Ignored, fall through
  }

  // Only close this if we process and return true.
  mGesture.CloseGestureInfoHandle((HGESTUREINFO)lParam);

  return PR_TRUE; // Handled
}
#endif // !defined(WINCE)

/*
 * OnMouseWheel - mouse wheele event processing. This was originally embedded
 * within the message case block. If returning true result should be returned
 * immediately (no more processing).
 */
#if !defined (WINCE_WINDOWS_MOBILE)
PRBool nsWindow::OnMouseWheel(UINT msg, WPARAM wParam, LPARAM lParam, PRBool& getWheelInfo, PRBool& result, LRESULT *aRetValue)
{
  // Handle both flavors of mouse wheel events.
  static int iDeltaPerLine, iDeltaPerChar;
  static ULONG ulScrollLines, ulScrollChars = 1;
  static int currentVDelta, currentHDelta;
  static HWND currentWindow = 0;

  PRBool isVertical = msg == WM_MOUSEWHEEL;

  // Get mouse wheel metrics (but only once).
  if (getWheelInfo) {
    getWheelInfo = PR_FALSE;

    SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);

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

    if (!SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0,
                              &ulScrollChars, 0)) {
      // Note that we may always fail to get the value before Win Vista.
      ulScrollChars = 1;
    }

    iDeltaPerChar = 0;
    if (ulScrollChars) {
      if (ulScrollChars <= WHEEL_DELTA) {
        iDeltaPerChar = WHEEL_DELTA / ulScrollChars;
      } else {
        ulScrollChars = WHEEL_PAGESCROLL;
      }
    }
  }

  if ((isVertical  && ulScrollLines != WHEEL_PAGESCROLL && !iDeltaPerLine) ||
      (!isVertical && ulScrollChars != WHEEL_PAGESCROLL && !iDeltaPerChar))
    return PR_FALSE; // break

  // The mousewheel event will be dispatched to the toplevel
  // window.  We need to give it to the child window

  POINT point;
  point.x = GET_X_LPARAM(lParam);
  point.y = GET_Y_LPARAM(lParam);
  HWND destWnd = ::WindowFromPoint(point);

  // Since we receive mousewheel events for as long as
  // we are focused, it's entirely possible that there
  // is another app's window or no window under the
  // pointer.

  if (!destWnd) {
    // No window is under the pointer
    return PR_FALSE; // break
  }

  // We don't care about windows belonging to other processes.
  DWORD processId = 0;
  GetWindowThreadProcessId(destWnd, &processId);
  if (processId != GetCurrentProcessId())
  {
    // Somebody elses window
    return PR_FALSE; // break
  }

  nsWindow* destWindow = GetNSWindowPtr(destWnd);
  if (!destWindow || destWindow->mIsPluginWindow) {
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
      nsWindow* parentWindow = GetNSWindowPtr(parentWnd);
      if (parentWindow) {
        // We have a child window - quite possibly a plugin window.
        // However, not all plugins are created equal - some will handle this message themselves,
        // some will forward directly back to us, while others will call DefWndProc, which
        // itself still forwards back to us.
        // So if we have sent it once, we need to handle it ourself.
        if (mInWheelProcessing) {
          destWnd = parentWnd;
          destWindow = parentWindow;
        } else {
          // First time we have seen this message.
          // Call the child - either it will consume it, or
          // it will wind it's way back to us, triggering the destWnd case above.
          // either way, when the call returns, we are all done with the message,
          mInWheelProcessing = PR_TRUE;
          if (0 == ::SendMessageW(destWnd, msg, wParam, lParam)) {
            result = PR_TRUE; // consumed - don't call DefWndProc
          }
          destWnd = nsnull;
          mInWheelProcessing = PR_FALSE;
        }
        return PR_FALSE; // break; // stop parent search
      }
      parentWnd = ::GetParent(parentWnd);
    } // while parentWnd
  }
  if (destWnd == nsnull)
    return PR_FALSE;
  if (destWnd != mWnd) {
    if (destWindow) {
      result = destWindow->ProcessMessage(msg, wParam, lParam, aRetValue);
      return PR_TRUE; // return result immediately
    }
  #ifdef DEBUG
    else
      printf("WARNING: couldn't get child window for MW event\n");
  #endif
  }

  // We should cancel the surplus delta if the current window is not
  // same as previous.
  if (currentWindow != mWnd) {
    currentVDelta = 0;
    currentHDelta = 0;
    currentWindow = mWnd;
  }

  // Keep track of whether or not the scroll notification is part of a series
  // in order to calculate appropriate acceleration effect
  UpdateMouseWheelSeriesCounter();

  nsMouseScrollEvent scrollEvent(PR_TRUE, NS_MOUSE_SCROLL, this);
  scrollEvent.delta = 0;
  if (isVertical) {
    scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;
    if (ulScrollLines == WHEEL_PAGESCROLL) {
      scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
      scrollEvent.delta = (((short) HIWORD (wParam)) > 0) ? -1 : 1;
    } else {
      currentVDelta -= (short) HIWORD (wParam);
      if (PR_ABS(currentVDelta) >= iDeltaPerLine) {
        // Compute delta to create acceleration effect
        scrollEvent.delta = ComputeMouseWheelDelta(currentVDelta, 
                                                   iDeltaPerLine, 
                                                   ulScrollLines);
        currentVDelta %= iDeltaPerLine;
      }
    }
  } else {
    scrollEvent.scrollFlags = nsMouseScrollEvent::kIsHorizontal;
    if (ulScrollChars == WHEEL_PAGESCROLL) {
      scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
      scrollEvent.delta = (((short) HIWORD (wParam)) > 0) ? 1 : -1;
    } else {
      currentHDelta += (short) HIWORD (wParam);
      if (PR_ABS(currentHDelta) >= iDeltaPerChar) {
        scrollEvent.delta = currentHDelta / iDeltaPerChar;
        currentHDelta %= iDeltaPerChar;
      }
    }
  }

  if (!scrollEvent.delta)
    return PR_FALSE; // break

  scrollEvent.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  scrollEvent.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  scrollEvent.isMeta    = PR_FALSE;
  scrollEvent.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  InitEvent(scrollEvent);
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(&scrollEvent);
  }
  // Note that we should return zero if we process WM_MOUSEWHEEL.
  // But if we process WM_MOUSEHWHEEL, we should return non-zero.

  if (result)
    *aRetValue = isVertical ? 0 : TRUE;
  
  return PR_FALSE; // break;
} 

// Reset scrollSeriesCounter when timer finishes (scroll series has ended)
void nsWindow::OnMouseWheelTimeout(nsITimer* aTimer, void* aClosure) 
{
  nsWindow* window = (nsWindow*) aClosure;
  window->mScrollSeriesCounter = 0;
}

// Increment scrollSeriesCount and reset timer to keep count going
void nsWindow::UpdateMouseWheelSeriesCounter() 
{
  mScrollSeriesCounter++;

  int scrollSeriesTimeout = 80;
  static nsITimer* scrollTimer;
  if (!scrollTimer) {
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!timer)
      return;
    timer.swap(scrollTimer);
  }

  scrollTimer->Cancel();
  nsresult rv = 
    scrollTimer->InitWithFuncCallback(OnMouseWheelTimeout, this,
                                      scrollSeriesTimeout,
                                      nsITimer::TYPE_ONE_SHOT);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "nsITimer::InitWithFuncCallback failed");
}

// If the scroll notification is part of a series of notifications we should
// increase scollEvent.delta to create an acceleration effect
int nsWindow::ComputeMouseWheelDelta(int currentVDelta,
                                     int iDeltaPerLine,
                                     ULONG ulScrollLines)
{
  // scrollAccelerationStart: click number at which acceleration starts
  int scrollAccelerationStart = gScrollPrefObserver->GetScrollAccelerationStart();
  // scrollNumlines: number of lines per scroll before acceleration
  int scrollNumLines = gScrollPrefObserver->GetScrollNumLines();
  // scrollAccelerationFactor: factor muliplied for constant acceleration
  int scrollAccelerationFactor = gScrollPrefObserver->GetScrollAccelerationFactor();

  // compute delta that obeys numlines pref
  int ulScrollLinesInt = static_cast<int>(ulScrollLines);
  // currentVDelta is a multiple of (iDeltaPerLine * ulScrollLinesInt)
  int delta = scrollNumLines * currentVDelta / (iDeltaPerLine * ulScrollLinesInt);

  // mScrollSeriesCounter: the index of the scroll notification in a series
  if (mScrollSeriesCounter < scrollAccelerationStart ||
      scrollAccelerationStart < 0 ||
      scrollAccelerationFactor < 0)
    return delta;
  else
    return int(0.5 + delta * mScrollSeriesCounter *
           (double) scrollAccelerationFactor / 10);
}

#endif // !defined(WINCE_WINDOWS_MOBILE)

static PRBool
StringCaseInsensitiveEquals(const PRUnichar* aChars1, const PRUint32 aNumChars1,
                            const PRUnichar* aChars2, const PRUint32 aNumChars2)
{
  if (aNumChars1 != aNumChars2)
    return PR_FALSE;

  nsCaseInsensitiveStringComparator comp;
  return comp(aChars1, aChars2, aNumChars1) == 0;
}

UINT nsWindow::MapFromNativeToDOM(UINT aNativeKeyCode)
{
#ifndef WINCE
  switch (aNativeKeyCode) {
    case VK_OEM_1:     return NS_VK_SEMICOLON;     // 0xBA, For the US standard keyboard, the ';:' key
    case VK_OEM_PLUS:  return NS_VK_ADD;           // 0xBB, For any country/region, the '+' key
    case VK_OEM_MINUS: return NS_VK_SUBTRACT;      // 0xBD, For any country/region, the '-' key
  }
#endif

  return aNativeKeyCode;
}

/**
 * nsWindow::OnKeyDown peeks into the message queue and pulls out
 * WM_CHAR messages for processing. During testing we don't want to
 * mess with the real message queue. Instead we pass a
 * pseudo-WM_CHAR-message using this structure, and OnKeyDown will use
 * that as if it was in the message queue, and refrain from actually
 * looking at or touching the message queue.
 */
LRESULT nsWindow::OnKeyDown(const MSG &aMsg,
                            nsModifierKeyState &aModKeyState,
                            PRBool *aEventDispatched,
                            nsFakeCharMessage* aFakeCharMessage)
{
  UINT virtualKeyCode = aMsg.wParam;

#ifndef WINCE
  gKbdLayout.OnKeyDown (virtualKeyCode);
#endif

  // Use only DOMKeyCode for XP processing.
  // Use aVirtualKeyCode for gKbdLayout and native processing.
  UINT DOMKeyCode = nsIMM32Handler::IsComposing(this) ?
                      virtualKeyCode : MapFromNativeToDOM(virtualKeyCode);

#ifdef DEBUG
  //printf("In OnKeyDown virt: %d\n", DOMKeyCode);
#endif

  PRBool noDefault =
    DispatchKeyEvent(NS_KEY_DOWN, 0, nsnull, DOMKeyCode, &aMsg, aModKeyState);
  if (aEventDispatched)
    *aEventDispatched = PR_TRUE;

  // If we won't be getting a WM_CHAR, WM_SYSCHAR or WM_DEADCHAR, synthesize a keypress
  // for almost all keys
  switch (DOMKeyCode) {
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
    case NS_VK_CAPS_LOCK:
    case NS_VK_NUM_LOCK:
    case NS_VK_SCROLL_LOCK: return noDefault;
  }

  PRUint32 extraFlags = (noDefault ? NS_EVENT_FLAG_NO_DEFAULT : 0);
  MSG msg;
  BOOL gotMsg = aFakeCharMessage ||
    ::PeekMessageW(&msg, mWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD);
  // Enter and backspace are always handled here to avoid for example the
  // confusion between ctrl-enter and ctrl-J.
  if (DOMKeyCode == NS_VK_RETURN || DOMKeyCode == NS_VK_BACK ||
      ((aModKeyState.mIsControlDown || aModKeyState.mIsAltDown)
#ifdef WINCE
       ))
#else
       && !gKbdLayout.IsDeadKey() && KeyboardLayout::IsPrintableCharKey(virtualKeyCode)))
#endif
  {
    // Remove a possible WM_CHAR or WM_SYSCHAR messages from the message queue.
    // They can be more than one because of:
    //  * Dead-keys not pairing with base character
    //  * Some keyboard layouts may map up to 4 characters to the single key
    PRBool anyCharMessagesRemoved = PR_FALSE;

    if (aFakeCharMessage) {
      anyCharMessagesRemoved = PR_TRUE;
    } else {
      while (gotMsg && (msg.message == WM_CHAR || msg.message == WM_SYSCHAR))
      {
        PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
               ("%s charCode=%d scanCode=%d\n", msg.message == WM_SYSCHAR ? "WM_SYSCHAR" : "WM_CHAR",
                msg.wParam, HIWORD(msg.lParam) & 0xFF));
        RemoveMessageAndDispatchPluginEvent(WM_KEYFIRST, WM_KEYLAST);
        anyCharMessagesRemoved = PR_TRUE;

        gotMsg = ::PeekMessageW (&msg, mWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD);
      }
    }

    if (!anyCharMessagesRemoved && DOMKeyCode == NS_VK_BACK &&
        nsIMM32Handler::IsDoingKakuteiUndo(mWnd)) {
      NS_ASSERTION(!aFakeCharMessage,
                   "We shouldn't be touching the real msg queue");
      RemoveMessageAndDispatchPluginEvent(WM_CHAR, WM_CHAR);
    }
  }
  else if (gotMsg &&
           (aFakeCharMessage ||
            msg.message == WM_CHAR || msg.message == WM_SYSCHAR || msg.message == WM_DEADCHAR)) {
    if (aFakeCharMessage)
      return OnCharRaw(aFakeCharMessage->mCharCode,
                       aFakeCharMessage->mScanCode, aModKeyState, extraFlags);

    // If prevent default set for keydown, do same for keypress
    ::GetMessageW(&msg, mWnd, msg.message, msg.message);

    if (msg.message == WM_DEADCHAR) {
      if (!PluginHasFocus())
        return PR_FALSE;

      // We need to send the removed message to focused plug-in.
      DispatchPluginEvent(msg);
      return noDefault;
    }

    PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
           ("%s charCode=%d scanCode=%d\n",
            msg.message == WM_SYSCHAR ? "WM_SYSCHAR" : "WM_CHAR",
            msg.wParam, HIWORD(msg.lParam) & 0xFF));

    BOOL result = OnChar(msg, aModKeyState, nsnull, extraFlags);
    // If a syschar keypress wasn't processed, Windows may want to
    // handle it to activate a native menu.
    if (!result && msg.message == WM_SYSCHAR)
      ::DefWindowProcW(mWnd, msg.message, msg.wParam, msg.lParam);
    return result;
  }
#ifndef WINCE
  else if (!aModKeyState.mIsControlDown && !aModKeyState.mIsAltDown &&
             (KeyboardLayout::IsPrintableCharKey(virtualKeyCode) ||
              KeyboardLayout::IsNumpadKey(virtualKeyCode)))
  {
    // If this is simple KeyDown event but next message is not WM_CHAR,
    // this event may not input text, so we should ignore this event.
    // See bug 314130.
    return PluginHasFocus() && noDefault;
  }

  if (gKbdLayout.IsDeadKey ())
    return PluginHasFocus() && noDefault;

  PRUint8 shiftStates[5];
  PRUnichar uniChars[5];
  PRUnichar shiftedChars[5] = {0, 0, 0, 0, 0};
  PRUnichar unshiftedChars[5] = {0, 0, 0, 0, 0};
  PRUnichar shiftedLatinChar = 0;
  PRUnichar unshiftedLatinChar = 0;
  PRUint32 numOfUniChars = 0;
  PRUint32 numOfShiftedChars = 0;
  PRUint32 numOfUnshiftedChars = 0;
  PRUint32 numOfShiftStates = 0;

  switch (virtualKeyCode) {
    // keys to be sent as characters
    case VK_ADD:       uniChars [0] = '+';  numOfUniChars = 1;  break;
    case VK_SUBTRACT:  uniChars [0] = '-';  numOfUniChars = 1;  break;
    case VK_DIVIDE:    uniChars [0] = '/';  numOfUniChars = 1;  break;
    case VK_MULTIPLY:  uniChars [0] = '*';  numOfUniChars = 1;  break;
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
      uniChars [0] = virtualKeyCode - VK_NUMPAD0 + '0';
      numOfUniChars = 1;
      break;
    default:
      if (KeyboardLayout::IsPrintableCharKey(virtualKeyCode)) {
        numOfUniChars = numOfShiftStates =
          gKbdLayout.GetUniChars(uniChars, shiftStates,
                                 NS_ARRAY_LENGTH(uniChars));
      }

      if (aModKeyState.mIsControlDown ^ aModKeyState.mIsAltDown) {
        PRUint8 capsLockState = (::GetKeyState(VK_CAPITAL) & 1) ? eCapsLock : 0;
        numOfUnshiftedChars =
          gKbdLayout.GetUniCharsWithShiftState(virtualKeyCode, capsLockState,
                       unshiftedChars, NS_ARRAY_LENGTH(unshiftedChars));
        numOfShiftedChars =
          gKbdLayout.GetUniCharsWithShiftState(virtualKeyCode,
                       capsLockState | eShift,
                       shiftedChars, NS_ARRAY_LENGTH(shiftedChars));

        // The current keyboard cannot input alphabets or numerics,
        // we should append them for Shortcut/Access keys.
        // E.g., for Cyrillic keyboard layout.
        if (NS_VK_A <= DOMKeyCode && DOMKeyCode <= NS_VK_Z) {
          shiftedLatinChar = unshiftedLatinChar = DOMKeyCode;
          if (capsLockState)
            shiftedLatinChar += 0x20;
          else
            unshiftedLatinChar += 0x20;
          if (unshiftedLatinChar == unshiftedChars[0] &&
              shiftedLatinChar == shiftedChars[0]) {
              shiftedLatinChar = unshiftedLatinChar = 0;
          }
        } else {
          PRUint16 ch = 0;
          if (NS_VK_0 <= DOMKeyCode && DOMKeyCode <= NS_VK_9) {
            ch = DOMKeyCode;
          } else {
            switch (virtualKeyCode) {
              case VK_OEM_PLUS:   ch = '+'; break;
              case VK_OEM_MINUS:  ch = '-'; break;
            }
          }
          if (ch && unshiftedChars[0] != ch && shiftedChars[0] != ch) {
            // Windows has assigned a virtual key code to the key even though
            // the character can't be produced with this key.  That probably
            // means the character can't be produced with any key in the
            // current layout and so the assignment is based on a QWERTY
            // layout.  Append this code so that users can access the shortcut.
            unshiftedLatinChar = ch;
          }
        }

        // If the charCode is not ASCII character, we should replace the
        // charCode with ASCII character only when Ctrl is pressed.
        // But don't replace the charCode when the charCode is not same as
        // unmodified characters. In such case, Ctrl is sometimes used for a
        // part of character inputting key combination like Shift.
        if (aModKeyState.mIsControlDown) {
          PRUint8 currentState = eCtrl;
          if (aModKeyState.mIsShiftDown)
            currentState |= eShift;

          PRUint32 ch =
            aModKeyState.mIsShiftDown ? shiftedLatinChar : unshiftedLatinChar;
          if (ch &&
              (numOfUniChars == 0 ||
               StringCaseInsensitiveEquals(uniChars, numOfUniChars,
                 aModKeyState.mIsShiftDown ? shiftedChars : unshiftedChars,
                 aModKeyState.mIsShiftDown ? numOfShiftedChars :
                                             numOfUnshiftedChars))) {
            numOfUniChars = numOfShiftStates = 1;
            uniChars[0] = ch;
            shiftStates[0] = currentState;
          }
        }
      }
  }

  if (numOfUniChars > 0 || numOfShiftedChars > 0 || numOfUnshiftedChars > 0) {
    PRUint32 num = PR_MAX(numOfUniChars,
                          PR_MAX(numOfShiftedChars, numOfUnshiftedChars));
    PRUint32 skipUniChars = num - numOfUniChars;
    PRUint32 skipShiftedChars = num - numOfShiftedChars;
    PRUint32 skipUnshiftedChars = num - numOfUnshiftedChars;
    UINT keyCode = numOfUniChars == 0 ? DOMKeyCode : 0;
    for (PRUint32 cnt = 0; cnt < num; cnt++) {
      PRUint16 uniChar, shiftedChar, unshiftedChar;
      uniChar = shiftedChar = unshiftedChar = 0;
      if (skipUniChars <= cnt) {
        if (cnt - skipUniChars  < numOfShiftStates) {
          // If key in combination with Alt and/or Ctrl produces a different
          // character than without them then do not report these flags
          // because it is separate keyboard layout shift state. If dead-key
          // and base character does not produce a valid composite character
          // then both produced dead-key character and following base
          // character may have different modifier flags, too.
          aModKeyState.mIsShiftDown =
            (shiftStates[cnt - skipUniChars] & eShift) != 0;
          aModKeyState.mIsControlDown =
            (shiftStates[cnt - skipUniChars] & eCtrl) != 0;
          aModKeyState.mIsAltDown =
            (shiftStates[cnt - skipUniChars] & eAlt) != 0;
        }
        uniChar = uniChars[cnt - skipUniChars];
      }
      if (skipShiftedChars <= cnt)
        shiftedChar = shiftedChars[cnt - skipShiftedChars];
      if (skipUnshiftedChars <= cnt)
        unshiftedChar = unshiftedChars[cnt - skipUnshiftedChars];
      nsAutoTArray<nsAlternativeCharCode, 5> altArray;

      if (shiftedChar || unshiftedChar) {
        nsAlternativeCharCode chars(unshiftedChar, shiftedChar);
        altArray.AppendElement(chars);
      }
      if (cnt == num - 1 && (unshiftedLatinChar || shiftedLatinChar)) {
        nsAlternativeCharCode chars(unshiftedLatinChar, shiftedLatinChar);
        altArray.AppendElement(chars);
      }

      DispatchKeyEvent(NS_KEY_PRESS, uniChar, &altArray,
                       keyCode, nsnull, aModKeyState, extraFlags);
    }
  } else {
    DispatchKeyEvent(NS_KEY_PRESS, 0, nsnull, DOMKeyCode, nsnull, aModKeyState,
                     extraFlags);
  }
#else
  {
    UINT unichar = ::MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_CHAR);
    // Check for dead characters or no mapping
    if (unichar & 0x80) {
      return noDefault;
    }
    DispatchKeyEvent(NS_KEY_PRESS, unichar, nsnull, DOMKeyCode, nsnull, aModKeyState,
                     extraFlags);
  }
#endif

  return noDefault;
}

// OnKeyUp
LRESULT nsWindow::OnKeyUp(const MSG &aMsg,
                          nsModifierKeyState &aModKeyState,
                          PRBool *aEventDispatched)
{
  UINT virtualKeyCode = aMsg.wParam;

  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("nsWindow::OnKeyUp VK=%d\n", virtualKeyCode));

  if (!nsIMM32Handler::IsComposing(this)) {
    virtualKeyCode = MapFromNativeToDOM(virtualKeyCode);
  }

  if (aEventDispatched)
    *aEventDispatched = PR_TRUE;
  return DispatchKeyEvent(NS_KEY_UP, 0, nsnull, virtualKeyCode, &aMsg,
                          aModKeyState);
}

// OnChar
LRESULT nsWindow::OnChar(const MSG &aMsg, nsModifierKeyState &aModKeyState,
                         PRBool *aEventDispatched, PRUint32 aFlags)
{
  return OnCharRaw(aMsg.wParam, HIWORD(aMsg.lParam) & 0xFF, aModKeyState,
                   aFlags, &aMsg, aEventDispatched);
}

// OnCharRaw
LRESULT nsWindow::OnCharRaw(UINT charCode, UINT aScanCode,
                            nsModifierKeyState &aModKeyState, PRUint32 aFlags,
                            const MSG *aMsg, PRBool *aEventDispatched)
{
  // ignore [shift+]alt+space so the OS can handle it
  if (aModKeyState.mIsAltDown && !aModKeyState.mIsControlDown &&
      IS_VK_DOWN(NS_VK_SPACE)) {
    return FALSE;
  }
  
  // Ignore Ctrl+Enter (bug 318235)
  if (aModKeyState.mIsControlDown && charCode == 0xA) {
    return FALSE;
  }

  // WM_CHAR with Control and Alt (== AltGr) down really means a normal character
  PRBool saveIsAltDown = aModKeyState.mIsAltDown;
  PRBool saveIsControlDown = aModKeyState.mIsControlDown;
  if (aModKeyState.mIsAltDown && aModKeyState.mIsControlDown)
    aModKeyState.mIsAltDown = aModKeyState.mIsControlDown = PR_FALSE;

  wchar_t uniChar;

  if (nsIMM32Handler::IsComposing(this)) {
    ResetInputState();
  }

  if (aModKeyState.mIsControlDown && charCode <= 0x1A) { // Ctrl+A Ctrl+Z, see Programming Windows 3.1 page 110 for details
    // need to account for shift here.  bug 16486
    if (aModKeyState.mIsShiftDown)
      uniChar = charCode - 1 + 'A';
    else
      uniChar = charCode - 1 + 'a';
    charCode = 0;
  }
  else if (aModKeyState.mIsControlDown && charCode <= 0x1F) {
    // Fix for 50255 - <ctrl><[> and <ctrl><]> are not being processed.
    // also fixes ctrl+\ (x1c), ctrl+^ (x1e) and ctrl+_ (x1f)
    // for some reason the keypress handler need to have the uniChar code set
    // with the addition of a upper case A not the lower case.
    uniChar = charCode - 1 + 'A';
    charCode = 0;
  } else { // 0x20 - SPACE, 0x3D - EQUALS
    if (charCode < 0x20 || (charCode == 0x3D && aModKeyState.mIsControlDown)) {
      uniChar = 0;
    } else {
      uniChar = charCode;
      charCode = 0;
    }
  }

  // Keep the characters unshifted for shortcuts and accesskeys and make sure
  // that numbers are always passed as such (among others: bugs 50255 and 351310)
  if (uniChar && (aModKeyState.mIsControlDown || aModKeyState.mIsAltDown)) {
    UINT virtualKeyCode = ::MapVirtualKeyEx(aScanCode, MAPVK_VSC_TO_VK,
                                            gKbdLayout.GetLayout());
    UINT unshiftedCharCode =
      virtualKeyCode >= '0' && virtualKeyCode <= '9' ? virtualKeyCode :
        aModKeyState.mIsShiftDown ? ::MapVirtualKeyEx(virtualKeyCode,
                                        MAPVK_VK_TO_CHAR,
                                        gKbdLayout.GetLayout()) : 0;
    // ignore diacritics (top bit set) and key mapping errors (char code 0)
    if ((INT)unshiftedCharCode > 0)
      uniChar = unshiftedCharCode;
  }

  // Fix for bug 285161 (and 295095) which was caused by the initial fix for bug 178110.
  // When pressing (alt|ctrl)+char, the char must be lowercase unless shift is
  // pressed too.
  if (!aModKeyState.mIsShiftDown && (saveIsAltDown || saveIsControlDown)) {
    uniChar = towlower(uniChar);
  }

  PRBool result = DispatchKeyEvent(NS_KEY_PRESS, uniChar, nsnull,
                                   charCode, aMsg, aModKeyState, aFlags);
  if (aEventDispatched)
    *aEventDispatched = PR_TRUE;
  aModKeyState.mIsAltDown = saveIsAltDown;
  aModKeyState.mIsControlDown = saveIsControlDown;
  return result;
}

void
nsWindow::SetupKeyModifiersSequence(nsTArray<KeyPair>* aArray, PRUint32 aModifiers)
{
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(sModifierKeyMap); ++i) {
    const PRUint32* map = sModifierKeyMap[i];
    if (aModifiers & map[0]) {
      aArray->AppendElement(KeyPair(map[1], map[2]));
    }
  }
}

nsresult
nsWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  // XXXroc we could use BeginDeferWindowPos/DeferWindowPos/EndDeferWindowPos
  // here, if that helps in some situations. So far I haven't seen a
  // need.
  for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
    NS_ASSERTION(w->GetParent() == this,
                 "Configured widget is not a child");
#ifdef WINCE
    // MSDN says we should do on WinCE this before moving or resizing the window
    // See http://msdn.microsoft.com/en-us/library/aa930600.aspx
    // We put the region back just below, anyway.
    ::SetWindowRgn(w->mWnd, NULL, TRUE);
#endif
    nsIntRect bounds;
    w->GetBounds(bounds);
    if (bounds.Size() != configuration.mBounds.Size()) {
      w->Resize(configuration.mBounds.x, configuration.mBounds.y,
                configuration.mBounds.width, configuration.mBounds.height,
                PR_TRUE);
    } else if (bounds.TopLeft() != configuration.mBounds.TopLeft()) {
      w->Move(configuration.mBounds.x, configuration.mBounds.y);
    }
    nsresult rv = w->SetWindowClipRegion(configuration.mClipRegion, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

static HRGN
CreateHRGNFromArray(const nsTArray<nsIntRect>& aRects)
{
  PRInt32 size = sizeof(RGNDATAHEADER) + sizeof(RECT)*aRects.Length();
  nsAutoTArray<PRUint8,100> buf;
  if (!buf.SetLength(size))
    return NULL;
  RGNDATA* data = reinterpret_cast<RGNDATA*>(buf.Elements());
  RECT* rects = reinterpret_cast<RECT*>(data->Buffer);
  data->rdh.dwSize = sizeof(data->rdh);
  data->rdh.iType = RDH_RECTANGLES;
  data->rdh.nCount = aRects.Length();
  nsIntRect bounds;
  for (PRUint32 i = 0; i < aRects.Length(); ++i) {
    const nsIntRect& r = aRects[i];
    bounds.UnionRect(bounds, r);
    ::SetRect(&rects[i], r.x, r.y, r.XMost(), r.YMost());
  }
  ::SetRect(&data->rdh.rcBound, bounds.x, bounds.y, bounds.XMost(), bounds.YMost());
  return ::ExtCreateRegion(NULL, buf.Length(), data);
}

nsresult
nsWindow::SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                              PRBool aIntersectWithExisting)
{
  if (!aIntersectWithExisting) {
    if (!StoreWindowClipRegion(aRects))
      return NS_OK;
  }

  HRGN dest = CreateHRGNFromArray(aRects);
  if (!dest)
    return NS_ERROR_OUT_OF_MEMORY;

  if (aIntersectWithExisting) {
    HRGN current = ::CreateRectRgn(0, 0, 0, 0);
    if (current) {
      if (::GetWindowRgn(mWnd, current) != 0 /*ERROR*/) {
        ::CombineRgn(dest, dest, current, RGN_AND);
      }
      ::DeleteObject(current);
    }
  }

  if (!::SetWindowRgn(mWnd, dest, TRUE)) {
    ::DeleteObject(dest);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// WM_DESTROY event handler
void nsWindow::OnDestroy()
{
  mOnDestroyCalled = PR_TRUE;

  // Make sure we don't get destroyed in the process of tearing down.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  
  // Dispatch the NS_DESTROY event. Must be called before mEventCallback is cleared.
  if (!mInDtor)
    DispatchStandardEvent(NS_DESTROY);

  // Prevent the widget from sending additional events.
  mEventCallback = nsnull;

  // Free our subclass and clear |this| stored in the window props. We will no longer
  // receive events from Windows after this point.
  SubclassWindow(FALSE);

  // Once mEventCallback is cleared and the subclass is reset, sCurrentWindow can be
  // cleared. (It's used in tracking windows for mouse events.)
  if (sCurrentWindow == this)
    sCurrentWindow = nsnull;

  // Disconnects us from our parent, will call our GetParent().
  nsBaseWidget::Destroy();

  // Release references to children, device context, toolkit, and app shell.
  nsBaseWidget::OnDestroy();
  
  // Clear our native parent handle.
  // XXX Windows will take care of this in the proper order, and SetParent(nsnull)'s
  // remove child on the parent already took place in nsBaseWidget's Destroy call above.
  //SetParent(nsnull);

  // We have to destroy the native drag target before we null out our window pointer.
  EnableDragDrop(PR_FALSE);

  // If we're going away and for some reason we're still the rollup widget, rollup and
  // turn off capture.
  if ( this == sRollupWidget ) {
    if ( sRollupListener )
      sRollupListener->Rollup(nsnull, nsnull);
    CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
  }

  // If IME is disabled, restore it.
  if (mOldIMC) {
    mOldIMC = ::ImmAssociateContext(mWnd, mOldIMC);
    NS_ASSERTION(!mOldIMC, "Another IMC was associated");
  }

  // Turn off mouse trails if enabled.
  MouseTrailer* mtrailer = nsToolkit::gMouseTrailer;
  if (mtrailer) {
    if (mtrailer->GetMouseTrailerWindow() == mWnd)
      mtrailer->DestroyTimer();

    if (mtrailer->GetCaptureWindow() == mWnd)
      mtrailer->SetCaptureWindow(nsnull);
  }

  // Free GDI window class objects
  if (mBrush) {
    VERIFY(::DeleteObject(mBrush));
    mBrush = NULL;
  }

  // Free app icon resources.
  HICON icon;
  icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM) 0);
  if (icon)
    ::DestroyIcon(icon);

  icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM) 0);
  if (icon)
    ::DestroyIcon(icon);

  // Destroy any custom cursor resources.
  if (mCursor == -1)
    SetCursor(eCursor_standard);

#ifdef MOZ_XUL
  // Reset transparency
  if (eTransparencyTransparent == mTransparencyMode)
    SetupTranslucentWindowMemoryBitmap(eTransparencyOpaque);
#endif

  // Clear the main HWND.
  mWnd = NULL;
}

// OnMove
PRBool nsWindow::OnMove(PRInt32 aX, PRInt32 aY)
{
  mBounds.x = aX;
  mBounds.y = aY;

  nsGUIEvent event(PR_TRUE, NS_MOVE, this);
  InitEvent(event);
  event.refPoint.x = aX;
  event.refPoint.y = aY;

  return DispatchWindowEvent(&event);
}

// Send a resize message to the listener
PRBool nsWindow::OnResize(nsIntRect &aWindowRect)
{
  // call the event callback
  if (mEventCallback) {
    nsSizeEvent event(PR_TRUE, NS_SIZE, this);
    InitEvent(event);
    event.windowSize = &aWindowRect;
    RECT r;
    if (::GetWindowRect(mWnd, &r)) {
      event.mWinWidth  = PRInt32(r.right - r.left);
      event.mWinHeight = PRInt32(r.bottom - r.top);
    } else {
      event.mWinWidth  = 0;
      event.mWinHeight = 0;
    }
    return DispatchWindowEvent(&event);
  }

  return PR_FALSE;
}

#if !defined(WINCE) // implemented in nsWindowCE.cpp
PRBool nsWindow::OnHotKey(WPARAM wParam, LPARAM lParam)
{
  return PR_TRUE;
}
#endif // !defined(WINCE)

void nsWindow::OnSettingsChange(WPARAM wParam, LPARAM lParam)
{
  nsWindowGfx::OnSettingsChangeGfx(wParam);
}

// Deal with scrollbar messages (actually implemented only in nsScrollbar)
PRBool nsWindow::OnScroll(UINT scrollCode, int cPos)
{
  return PR_FALSE;
}

// Return the brush used to paint the background of this control
HBRUSH nsWindow::OnControlColor()
{
  return mBrush;
}

// Can be overriden. Controls auot-erase of background.
PRBool nsWindow::AutoErase()
{
  return PR_FALSE;
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: IME management and accessibility
 **
 ** Handles managing IME input and accessibility.
 **
 **************************************************************
 **************************************************************/

NS_IMETHODIMP nsWindow::ResetInputState()
{
#ifdef DEBUG_KBSTATE
  printf("ResetInputState\n");
#endif

#ifdef NS_ENABLE_TSF
  nsTextStore::CommitComposition(PR_FALSE);
#endif //NS_ENABLE_TSF

  nsIMEContext IMEContext(mWnd);
  if (IMEContext.IsValid()) {
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_COMPLETE, NULL);
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_CANCEL, NULL);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetIMEOpenState(PRBool aState)
{
#ifdef DEBUG_KBSTATE
  printf("SetIMEOpenState %s\n", (aState ? "Open" : "Close"));
#endif 

#ifdef NS_ENABLE_TSF
  nsTextStore::SetIMEOpenState(aState);
#endif //NS_ENABLE_TSF

  nsIMEContext IMEContext(mWnd);
  if (IMEContext.IsValid()) {
    ::ImmSetOpenStatus(IMEContext.get(), aState ? TRUE : FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::GetIMEOpenState(PRBool* aState)
{
  nsIMEContext IMEContext(mWnd);
  if (IMEContext.IsValid()) {
    BOOL isOpen = ::ImmGetOpenStatus(IMEContext.get());
    *aState = isOpen ? PR_TRUE : PR_FALSE;
  } else 
    *aState = PR_FALSE;

#ifdef NS_ENABLE_TSF
  *aState |= nsTextStore::GetIMEOpenState();
#endif //NS_ENABLE_TSF

  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetIMEEnabled(PRUint32 aState)
{
#ifdef NS_ENABLE_TSF
  nsTextStore::SetIMEEnabled(aState);
#endif //NS_ENABLE_TSF
#ifdef DEBUG_KBSTATE
  printf("SetIMEEnabled: %s\n", (aState == nsIWidget::IME_STATUS_ENABLED ||
                                 aState == nsIWidget::IME_STATUS_PLUGIN)? 
                                "Enabled": "Disabled");
#endif 
  if (nsIMM32Handler::IsComposing(this))
    ResetInputState();
  mIMEEnabled = aState;
  PRBool enable = (aState == nsIWidget::IME_STATUS_ENABLED ||
                   aState == nsIWidget::IME_STATUS_PLUGIN);

#if defined(WINCE_HAVE_SOFTKB)
  sSoftKeyboardState = (aState != nsIWidget::IME_STATUS_DISABLED);
  nsWindowCE::ToggleSoftKB(sSoftKeyboardState);
#endif

  if (!enable != !mOldIMC)
    return NS_OK;
  mOldIMC = ::ImmAssociateContext(mWnd, enable ? mOldIMC : NULL);
  NS_ASSERTION(!enable || !mOldIMC, "Another IMC was associated");

  return NS_OK;
}

NS_IMETHODIMP nsWindow::GetIMEEnabled(PRUint32* aState)
{
#ifdef DEBUG_KBSTATE
  printf("GetIMEEnabled: %s\n", mIMEEnabled? "Enabled": "Disabled");
#endif 
  *aState = mIMEEnabled;
  return NS_OK;
}

NS_IMETHODIMP nsWindow::CancelIMEComposition()
{
#ifdef DEBUG_KBSTATE
  printf("CancelIMEComposition\n");
#endif 

#ifdef NS_ENABLE_TSF
  nsTextStore::CommitComposition(PR_TRUE);
#endif //NS_ENABLE_TSF

  nsIMEContext IMEContext(mWnd);
  if (IMEContext.IsValid()) {
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_CANCEL, NULL);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetToggledKeyState(PRUint32 aKeyCode, PRBool* aLEDState)
{
#ifdef DEBUG_KBSTATE
  printf("GetToggledKeyState\n");
#endif 
  NS_ENSURE_ARG_POINTER(aLEDState);
  *aLEDState = (::GetKeyState(aKeyCode) & 1) != 0;
  return NS_OK;
}

#ifdef NS_ENABLE_TSF
NS_IMETHODIMP
nsWindow::OnIMEFocusChange(PRBool aFocus)
{
  nsresult rv = nsTextStore::OnFocusChange(aFocus, this, mIMEEnabled);
  if (rv == NS_ERROR_NOT_AVAILABLE)
    rv = NS_OK; // TSF is not enabled, maybe.
  return rv;
}

NS_IMETHODIMP
nsWindow::OnIMETextChange(PRUint32 aStart,
                          PRUint32 aOldEnd,
                          PRUint32 aNewEnd)
{
  return nsTextStore::OnTextChange(aStart, aOldEnd, aNewEnd);
}

NS_IMETHODIMP
nsWindow::OnIMESelectionChange(void)
{
  return nsTextStore::OnSelectionChange();
}
#endif //NS_ENABLE_TSF

#ifdef ACCESSIBILITY
already_AddRefed<nsIAccessible> nsWindow::GetRootAccessible()
{
  nsWindow::sIsAccessibilityOn = TRUE;

  if (mInDtor || mOnDestroyCalled || mWindowType == eWindowType_invisible) {
    return nsnull;
  }

  nsIAccessible *rootAccessible = nsnull;

  // If accessibility is turned on, we create this even before it is requested
  // when the window gets focused. We need it to be created early so it can 
  // generate accessibility events right away
  nsWindow* accessibleWindow = nsnull;
  if (mContentType != eContentTypeInherit) {
    // We're on a MozillaContentWindowClass or MozillaUIWindowClass window.
    // Search for the correct visible child window to get an accessible 
    // document from. Make sure to use an active child window
    HWND accessibleWnd = ::GetTopWindow(mWnd);
    while (accessibleWnd) {
      // Loop through windows and find the first one with accessibility info
      accessibleWindow = GetNSWindowPtr(accessibleWnd);
      if (accessibleWindow) {
        accessibleWindow->DispatchAccessibleEvent(NS_GETACCESSIBLE, &rootAccessible);
        if (rootAccessible) {
          break;  // Success, one of the child windows was active
        }
      }
      accessibleWnd = ::GetNextWindow(accessibleWnd, GW_HWNDNEXT);
    }
  }
  else {
    DispatchAccessibleEvent(NS_GETACCESSIBLE, &rootAccessible);
  }
  return rootAccessible;
}

STDMETHODIMP_(LRESULT)
nsWindow::LresultFromObject(REFIID riid, WPARAM wParam, LPUNKNOWN pAcc)
{
  // open the dll dynamically
  if (!sAccLib)
    sAccLib =::LoadLibraryW(L"OLEACC.DLL");

  if (sAccLib) {
    if (!sLresultFromObject)
      sLresultFromObject = (LPFNLRESULTFROMOBJECT)GetProcAddress(sAccLib,"LresultFromObject");

    if (sLresultFromObject)
      return sLresultFromObject(riid,wParam,pAcc);
  }

  return 0;
}
#endif

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Transparency
 **
 ** Window transparency helpers.
 **
 **************************************************************
 **************************************************************/

#ifdef MOZ_XUL

void nsWindow::ResizeTranslucentWindow(PRInt32 aNewWidth, PRInt32 aNewHeight, PRBool force)
{
  if (!force && aNewWidth == mBounds.width && aNewHeight == mBounds.height)
    return;

  mTransparentSurface = new gfxWindowsSurface(gfxIntSize(aNewWidth, aNewHeight), gfxASurface::ImageFormatARGB32);
  mMemoryDC = mTransparentSurface->GetDC();
}

void nsWindow::SetWindowTranslucencyInner(nsTransparencyMode aMode)
{
#ifndef WINCE

  if (aMode == mTransparencyMode)
    return;

  HWND hWnd = GetTopLevelHWND(mWnd, PR_TRUE);
  nsWindow* topWindow = GetNSWindowPtr(hWnd);

  if (!topWindow)
  {
    NS_WARNING("Trying to use transparent chrome in an embedded context");
    return;
  }

  LONG_PTR style = 0, exStyle = 0;
  switch(aMode) {
    case eTransparencyTransparent:
      exStyle |= WS_EX_LAYERED;
    case eTransparencyOpaque:
    case eTransparencyGlass:
      topWindow->mTransparencyMode = aMode;
      break;
  }

  style |= topWindow->WindowStyle();
  exStyle |= topWindow->WindowExStyle();

  if (aMode == eTransparencyTransparent) {
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
  }

  VERIFY_WINDOW_STYLE(style);
  ::SetWindowLongPtrW(hWnd, GWL_STYLE, style);
  ::SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);

  mTransparencyMode = aMode;

  SetupTranslucentWindowMemoryBitmap(aMode);
  MARGINS margins = { 0, 0, 0, 0 };
  if(eTransparencyGlass == aMode)
    margins.cxLeftWidth = -1;
  if(nsUXThemeData::sHaveCompositor)
    nsUXThemeData::dwmExtendFrameIntoClientAreaPtr(hWnd, &margins);
#endif
}

void nsWindow::SetupTranslucentWindowMemoryBitmap(nsTransparencyMode aMode)
{
  if (eTransparencyTransparent == aMode) {
    ResizeTranslucentWindow(mBounds.width, mBounds.height, PR_TRUE);
  } else {
    mTransparentSurface = nsnull;
    mMemoryDC = NULL;
  }
}

nsresult nsWindow::UpdateTranslucentWindow()
{
#ifndef WINCE
  if (mBounds.IsEmpty())
    return NS_OK;

  ::GdiFlush();

  BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
  SIZE winSize = { mBounds.width, mBounds.height };
  POINT srcPos = { 0, 0 };
  HWND hWnd = GetTopLevelHWND(mWnd, PR_TRUE);
  RECT winRect;
  ::GetWindowRect(hWnd, &winRect);

  // perform the alpha blend
  if (!::UpdateLayeredWindow(hWnd, NULL, (POINT*)&winRect, &winSize, mMemoryDC, &srcPos, 0, &bf, ULW_ALPHA))
    return NS_ERROR_FAILURE;
#endif

  return NS_OK;
}

#endif //MOZ_XUL

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Popup rollup hooks
 **
 ** Deals with CaptureRollup on popup windows.
 **
 **************************************************************
 **************************************************************/

#ifndef WINCE
// Schedules a timer for a window, so we can rollup after processing the hook event
void nsWindow::ScheduleHookTimer(HWND aWnd, UINT aMsgId)
{
  // In some cases multiple hooks may be scheduled
  // so ignore any other requests once one timer is scheduled
  if (sHookTimerId == 0) {
    // Remember the window handle and the message ID to be used later
    sRollupMsgId = aMsgId;
    sRollupMsgWnd = aWnd;
    // Schedule native timer for doing the rollup after
    // this event is done being processed
    sHookTimerId = ::SetTimer(NULL, 0, 0, (TIMERPROC)HookTimerForPopups);
    NS_ASSERTION(sHookTimerId, "Timer couldn't be created.");
  }
}

#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
int gLastMsgCode = 0;
extern MSGFEventMsgInfo gMSGFEvents[];
#endif

// Process Menu messages, rollup when popup is clicked.
LRESULT CALLBACK nsWindow::MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
  if (sProcessHook) {
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
#endif // #ifdef POPUP_ROLLUP_DEBUG_OUTPUT

  if (sProcessHook && code == MSGF_MENU) {
    MSG* pMsg = (MSG*)lParam;
    ScheduleHookTimer( pMsg->hwnd, pMsg->message);
  }

  return ::CallNextHookEx(sMsgFilterHook, code, wParam, lParam);
}

// Process all mouse messages. Roll up when a click is in a native window
// that doesn't have an nsIWidget.
LRESULT CALLBACK nsWindow::MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (sProcessHook) {
    switch (wParam) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      {
        MOUSEHOOKSTRUCT* ms = (MOUSEHOOKSTRUCT*)lParam;
        nsIWidget* mozWin = (nsIWidget*)GetNSWindowPtr(ms->hwnd);
        if (mozWin) {
          // If this window is windowed plugin window, the mouse events are not
          // sent to us.
          if (static_cast<nsWindow*>(mozWin)->mIsPluginWindow)
            ScheduleHookTimer(ms->hwnd, (UINT)wParam);
        } else {
          ScheduleHookTimer(ms->hwnd, (UINT)wParam);
        }
        break;
      }
    }
  }
  return ::CallNextHookEx(sCallMouseHook, code, wParam, lParam);
}

// Process all messages. Roll up when the window is moving, or
// is resizing or when maximized or mininized.
LRESULT CALLBACK nsWindow::MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
  if (sProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    PrintEvent(cwpt->message, FALSE, FALSE);
  }
#endif

  if (sProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    if (cwpt->message == WM_MOVING ||
        cwpt->message == WM_SIZING ||
        cwpt->message == WM_GETMINMAXINFO) {
      ScheduleHookTimer(cwpt->hwnd, (UINT)cwpt->message);
    }
  }

  return ::CallNextHookEx(sCallProcHook, code, wParam, lParam);
}

// Register the special "hooks" for dropdown processing.
void nsWindow::RegisterSpecialDropdownHooks()
{
  NS_ASSERTION(!sMsgFilterHook, "sMsgFilterHook must be NULL!");
  NS_ASSERTION(!sCallProcHook,  "sCallProcHook must be NULL!");

  DISPLAY_NMM_PRT("***************** Installing Msg Hooks ***************\n");

  //HMODULE hMod = GetModuleHandle("gkwidget.dll");

  // Install msg hook for moving the window and resizing
  if (!sMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Hooking sMsgFilterHook!\n");
    sMsgFilterHook = SetWindowsHookEx(WH_MSGFILTER, MozSpecialMsgFilter, NULL, GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sMsgFilterHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_MSGFILTER!\n");
    }
#endif
  }

  // Install msg hook for menus
  if (!sCallProcHook) {
    DISPLAY_NMM_PRT("***** Hooking sCallProcHook!\n");
    sCallProcHook  = SetWindowsHookEx(WH_CALLWNDPROC, MozSpecialWndProc, NULL, GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sCallProcHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_CALLWNDPROC!\n");
    }
#endif
  }

  // Install msg hook for the mouse
  if (!sCallMouseHook) {
    DISPLAY_NMM_PRT("***** Hooking sCallMouseHook!\n");
    sCallMouseHook  = SetWindowsHookEx(WH_MOUSE, MozSpecialMouseProc, NULL, GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sCallMouseHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_MOUSE!\n");
    }
#endif
  }
}

// Unhook special message hooks for dropdowns.
void nsWindow::UnregisterSpecialDropdownHooks()
{
  DISPLAY_NMM_PRT("***************** De-installing Msg Hooks ***************\n");

  if (sCallProcHook) {
    DISPLAY_NMM_PRT("***** Unhooking sCallProcHook!\n");
    if (!::UnhookWindowsHookEx(sCallProcHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sCallProcHook!\n");
    }
    sCallProcHook = NULL;
  }

  if (sMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Unhooking sMsgFilterHook!\n");
    if (!::UnhookWindowsHookEx(sMsgFilterHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sMsgFilterHook!\n");
    }
    sMsgFilterHook = NULL;
  }

  if (sCallMouseHook) {
    DISPLAY_NMM_PRT("***** Unhooking sCallMouseHook!\n");
    if (!::UnhookWindowsHookEx(sCallMouseHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sCallMouseHook!\n");
    }
    sCallMouseHook = NULL;
  }
}

// This timer is designed to only fire one time at most each time a "hook" function
// is used to rollup the dropdown. In some cases, the timer may be scheduled from the
// hook, but that hook event or a subsequent event may roll up the dropdown before
// this timer function is executed.
//
// For example, if an MFC control takes focus, the combobox will lose focus and rollup
// before this function fires.
VOID CALLBACK nsWindow::HookTimerForPopups(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
  if (sHookTimerId != 0) {
    // if the window is NULL then we need to use the ID to kill the timer
    BOOL status = ::KillTimer(NULL, sHookTimerId);
    NS_ASSERTION(status, "Hook Timer was not killed.");
    sHookTimerId = 0;
  }

  if (sRollupMsgId != 0) {
    // Note: DealWithPopups does the check to make sure that
    // sRollupListener and sRollupWidget are not NULL
    LRESULT popupHandlingResult;
    nsAutoRollup autoRollup;
    DealWithPopups(sRollupMsgWnd, sRollupMsgId, 0, 0, &popupHandlingResult);
    sRollupMsgId = 0;
    sRollupMsgWnd = NULL;
  }
}
#endif // WinCE

static PRBool IsDifferentThreadWindow(HWND aWnd)
{
  return ::GetCurrentThreadId() != ::GetWindowThreadProcessId(aWnd, NULL);
}

PRBool
nsWindow::EventIsInsideWindow(UINT Msg, nsWindow* aWindow)
{
  RECT r;

#ifndef WINCE
  if (Msg == WM_ACTIVATEAPP)
    // don't care about activation/deactivation
    return PR_FALSE;
#else
  if (Msg == WM_ACTIVATE)
    // but on Windows CE we do care about
    // activation/deactivation because there doesn't exist
    // cancelable Mouse Activation events
    return PR_TRUE;
#endif

  ::GetWindowRect(aWindow->mWnd, &r);
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = GET_X_LPARAM(pos);
  mp.y = GET_Y_LPARAM(pos);

  // was the event inside this window?
  return (PRBool) PtInRect(&r, mp);
}

// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
BOOL
nsWindow::DealWithPopups(HWND inWnd, UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult)
{
  if (sRollupListener && sRollupWidget && ::IsWindowVisible(inWnd)) {

    if (inMsg == WM_LBUTTONDOWN || inMsg == WM_RBUTTONDOWN || inMsg == WM_MBUTTONDOWN ||
        inMsg == WM_MOUSEWHEEL || inMsg == WM_MOUSEHWHEEL || inMsg == WM_ACTIVATE ||
        (inMsg == WM_KILLFOCUS && IsDifferentThreadWindow((HWND)inWParam))
#ifndef WINCE
        ||
        inMsg == WM_NCRBUTTONDOWN ||
        inMsg == WM_MOVING ||
        inMsg == WM_SIZING ||
        inMsg == WM_NCLBUTTONDOWN ||
        inMsg == WM_NCMBUTTONDOWN ||
        inMsg == WM_MOUSEACTIVATE ||
        inMsg == WM_ACTIVATEAPP ||
        inMsg == WM_MENUSELECT
#endif
        )
    {
      // Rollup if the event is outside the popup.
      PRBool rollup = !nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)sRollupWidget);

      if (rollup && (inMsg == WM_MOUSEWHEEL || inMsg == WM_MOUSEHWHEEL))
      {
        sRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);
        *outResult = PR_TRUE;
      }

      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      PRUint32 popupsToRollup = PR_UINT32_MAX;
      if (rollup) {
        nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(sRollupListener) );
        if ( menuRollup ) {
          nsAutoTArray<nsIWidget*, 5> widgetChain;
          PRUint32 sameTypeCount = menuRollup->GetSubmenuWidgetChain(&widgetChain);
          for ( PRUint32 i = 0; i < widgetChain.Length(); ++i ) {
            nsIWidget* widget = widgetChain[i];
            if ( nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)widget) ) {
              // don't roll up if the mouse event occured within a menu of the
              // same type. If the mouse event occured in a menu higher than
              // that, roll up, but pass the number of popups to Rollup so
              // that only those of the same type close up.
              if (i < sameTypeCount) {
                rollup = PR_FALSE;
              }
              else {
                popupsToRollup = sameTypeCount;
              }
              break;
            }
          } // foreach parent menu widget
        } // if rollup listener knows about menus
      }

#ifndef WINCE
      if (inMsg == WM_MOUSEACTIVATE && popupsToRollup == PR_UINT32_MAX) {
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
            sRollupListener->ShouldRollupOnMouseActivate(&rollup);
            if (!rollup)
            {
              *outResult = MA_NOACTIVATE;
              return true;
            }
          }
        }
      }
      // if we've still determined that we should still rollup everything, do it.
      else
#endif
      if ( rollup ) {
        // sRollupConsumeEvent may be modified by
        // nsIRollupListener::Rollup.
        PRBool consumeRollupEvent = sRollupConsumeEvent;
        // only need to deal with the last rollup for left mouse down events.
        sRollupListener->Rollup(popupsToRollup, inMsg == WM_LBUTTONDOWN ? &mLastRollup : nsnull);

        // Tell hook to stop processing messages
        sProcessHook = PR_FALSE;
        sRollupMsgId = 0;
        sRollupMsgWnd = NULL;

        // return TRUE tells Windows that the event is consumed,
        // false allows the event to be dispatched
        //
        // So if we are NOT supposed to be consuming events, let it go through
        if (consumeRollupEvent && inMsg != WM_RBUTTONDOWN) {
          *outResult = TRUE;
          return TRUE;
        }
#ifndef WINCE
        // if we are only rolling up some popups, don't activate and don't let
        // the event go through. This prevents clicks menus higher in the
        // chain from opening when a context menu is open
        if (popupsToRollup != PR_UINT32_MAX && inMsg == WM_MOUSEACTIVATE) {
          *outResult = MA_NOACTIVATEANDEAT;
          return TRUE;
        }
#endif
      }
    } // if event that might trigger a popup to rollup
  } // if rollup listeners registered

  return FALSE;
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Misc. utility methods and functions.
 **
 ** General use.
 **
 **************************************************************
 **************************************************************/

// nsModifierKeyState used in various character processing. 
nsModifierKeyState::nsModifierKeyState()
{
  mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
  mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
  mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
}


PRInt32 nsWindow::GetWindowsVersion()
{
#ifdef WINCE
  return 0x500;
#else
  static PRInt32 version = 0;
  static PRBool didCheck = PR_FALSE;

  if (!didCheck)
  {
    didCheck = PR_TRUE;
    OSVERSIONINFOEX osInfo;
    osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    // This cast is safe and supposed to be here, don't worry
    ::GetVersionEx((OSVERSIONINFO*)&osInfo);
    version = (osInfo.dwMajorVersion & 0xff) << 8 | (osInfo.dwMinorVersion & 0xff);
  }
  return version;
#endif
}

// Note that the result of GetTopLevelWindow method can be different from the
// result of GetTopLevelHWND method.  The result can be non-floating window.
// Because our top level window may be contained in another window which is
// not managed by us.
nsWindow* nsWindow::GetTopLevelWindow(PRBool aStopOnDialogOrPopup)
{
  nsWindow* curWindow = this;

  while (PR_TRUE) {
    if (aStopOnDialogOrPopup) {
      switch (curWindow->mWindowType) {
        case eWindowType_dialog:
        case eWindowType_popup:
          return curWindow;
      }
    }

    // Retrieve the top level parent or owner window
    nsWindow* parentWindow = curWindow->GetParentWindow(PR_TRUE);

    if (!parentWindow)
      return curWindow;

    curWindow = parentWindow;
  }
}

// Note that the result of GetTopLevelHWND can be different from the result
// of GetTopLevelWindow method.  Because this is checking whether the window
// is top level only in Win32 window system.  Therefore, the result window
// may not be managed by us.
HWND nsWindow::GetTopLevelHWND(HWND aWnd, PRBool aStopOnDialogOrPopup)
{
  HWND curWnd = aWnd;
  HWND topWnd = NULL;
  HWND upWnd = NULL;

  while (curWnd) {
    topWnd = curWnd;

    if (aStopOnDialogOrPopup) {
      DWORD_PTR style = ::GetWindowLongPtrW(curWnd, GWL_STYLE);

      VERIFY_WINDOW_STYLE(style);

      if (!(style & WS_CHILD)) // first top-level window
        break;
    }

    upWnd = ::GetParent(curWnd); // Parent or owner (if has no parent)

#ifdef WINCE
    // For dialog windows, we want just the parent, not the owner.
    // For other/popup windows, we want to find the first owner/parent
    // that's a dialog and/or has an owner.
    if (upWnd && ::GetWindow(curWnd, GW_OWNER) == upWnd) {
      DWORD_PTR style = ::GetWindowLongPtrW(curWnd, GWL_STYLE);
      if ((style & WS_DLGFRAME) != 0)
        break;
    }
#endif

    curWnd = upWnd;
  }

  return topWnd;
}

static BOOL CALLBACK gEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
  DWORD pid;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (pid == GetCurrentProcessId() && ::IsWindowVisible(hwnd))
  {
    gWindowsVisible = PR_TRUE;
    return FALSE;
  }
  return TRUE;
}

PRBool nsWindow::CanTakeFocus()
{
  gWindowsVisible = PR_FALSE;
  EnumWindows(gEnumWindowsProc, 0);
  if (!gWindowsVisible) {
    return PR_TRUE;
  } else {
    HWND fgWnd = ::GetForegroundWindow();
    if (!fgWnd) {
      return PR_TRUE;
    }
    DWORD pid;
    GetWindowThreadProcessId(fgWnd, &pid);
    if (pid == GetCurrentProcessId()) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

LPARAM nsWindow::lParamToScreen(LPARAM lParam)
{
  POINT pt;
  pt.x = GET_X_LPARAM(lParam);
  pt.y = GET_Y_LPARAM(lParam);
  ::ClientToScreen(mWnd, &pt);
  return MAKELPARAM(pt.x, pt.y);
}

LPARAM nsWindow::lParamToClient(LPARAM lParam)
{
  POINT pt;
  pt.x = GET_X_LPARAM(lParam);
  pt.y = GET_Y_LPARAM(lParam);
  ::ScreenToClient(mWnd, &pt);
  return MAKELPARAM(pt.x, pt.y);
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: ChildWindow impl.
 **
 ** Child window overrides.
 **
 **************************************************************
 **************************************************************/

// Deal with all sort of mouse event
PRBool ChildWindow::DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam, LPARAM lParam,
                                       PRBool aIsContextMenuKey, PRInt16 aButton)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback) {
    return result;
  }

  switch (aEventType) {
    case NS_MOUSE_BUTTON_DOWN:
      CaptureMouse(PR_TRUE);
      break;

    // NS_MOUSE_MOVE and NS_MOUSE_EXIT are here because we need to make sure capture flag
    // isn't left on after a drag where we wouldn't see a button up message (see bug 324131).
    case NS_MOUSE_BUTTON_UP:
    case NS_MOUSE_MOVE:
    case NS_MOUSE_EXIT:
      if (!(wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) && mIsInMouseCapture)
        CaptureMouse(PR_FALSE);
      break;

    default:
      break;

  } // switch

  return nsWindow::DispatchMouseEvent(aEventType, wParam, lParam,
                                      aIsContextMenuKey, aButton);
}

// return the style for a child nsWindow
DWORD ChildWindow::WindowStyle()
{
  DWORD style = WS_CLIPCHILDREN | nsWindow::WindowStyle();
  if (!(style & WS_POPUP))
    style |= WS_CHILD; // WS_POPUP and WS_CHILD are mutually exclusive.
  VERIFY_WINDOW_STYLE(style);
  return style;
}
