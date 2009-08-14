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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * nsWindowCE - Windows CE specific code related to nsWindow. 
 */

#include "nsWindowCE.h"
#include "nsIObserverService.h"
#include "resource.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

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

#if defined(WINCE_HAVE_SOFTKB)
PRBool          nsWindow::sSoftKeyMenuBar         = PR_FALSE;
PRBool          nsWindow::sSoftKeyboardState      = PR_FALSE;
PRBool          nsWindowCE::sSIPInTransition      = PR_FALSE;
TriStateBool    nsWindowCE::sShowSIPButton        = TRI_UNKNOWN;
#endif

/**************************************************************
 **************************************************************
 **
 ** BLOCK: nsWindowCE helpers
 **
 ** Called by nsWindow for wince specific work.
 **
 **************************************************************
 **************************************************************/

#ifdef WINCE_HAVE_SOFTKB
void nsWindowCE::NotifySoftKbObservers(LPRECT visRect)
{
  if (!visRect) {
    SIPINFO sipInfo;
    memset(&sipInfo, 0, sizeof(SIPINFO));
    sipInfo.cbSize = sizeof(SIPINFO);
    if (SipGetInfo(&sipInfo)) 
      visRect = &(sipInfo.rcVisibleDesktop);
    else
      return;
  }
  
  
  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    wchar_t rectBuf[256];
    _snwprintf(rectBuf, 256, L"{\"left\": %d, \"top\": %d,"
               L" \"right\": %d, \"bottom\": %d}", 
               visRect->left, visRect->top, visRect->right, visRect->bottom);
    observerService->NotifyObservers(nsnull, "softkb-change", rectBuf);
  }
}

void nsWindowCE::ToggleSoftKB(PRBool show)
{
  sSIPInTransition = PR_TRUE;
  HWND hWndSIP = FindWindowW(L"SipWndClass", NULL );
  if (hWndSIP)
    ShowWindow(hWndSIP, show ? SW_SHOW: SW_HIDE);

  HWND hWndSIPB = FindWindowW(L"MS_SIPBUTTON", NULL ); 
  if (hWndSIPB)
      ShowWindow(hWndSIPB, show ? SW_SHOW: SW_HIDE);

  SipShowIM(show ? SIPF_ON : SIPF_OFF);

  if (sShowSIPButton == TRI_UNKNOWN) {
    PRBool tmpBool = PR_FALSE;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      nsCOMPtr<nsIPrefBranch> prefBranch;
      prefs->GetBranch(0, getter_AddRefs(prefBranch));
      if (prefBranch) {
        nsresult rv = prefBranch->GetBoolPref("ui.sip.showSIPButton", &tmpBool);
        if (NS_SUCCEEDED(rv))
          sShowSIPButton = tmpBool ? TRI_TRUE : TRI_FALSE;
      }
    }
  }
  if (sShowSIPButton != TRI_TRUE && hWndSIPB && hWndSIP) {
    ShowWindow(hWndSIPB, SW_HIDE);
    int sX = GetSystemMetrics(SM_CXSCREEN);
    int sY = GetSystemMetrics(SM_CYSCREEN);
    RECT sipRect;
    GetWindowRect(hWndSIP, &sipRect);
    int sipH = sipRect.bottom - sipRect.top;
    int sipW = sipRect.right - sipRect.left;
    sipRect.left = (sX - sipW)/2;
    sipRect.top =  sY - sipH;
    sipRect.bottom = sY;
    sipRect.right = sipRect.left + sipW;
    MoveWindow(hWndSIP, (sX - sipW)/2, sY - sipH, sipW, sipH, TRUE);
    SIPINFO sipInfo;
    RECT visRect;
    visRect.top = 0;
    visRect.left = 0;
    visRect.bottom = show ? sipRect.top : sY;
    visRect.right = sX;
    sipInfo.cbSize = sizeof(SIPINFO);
    sipInfo.fdwFlags = SIPF_DOCKED | SIPF_LOCKED | (show ? SIPF_ON : SIPF_OFF);
    sipInfo.rcSipRect = sipRect;
    sipInfo.rcVisibleDesktop = visRect;
    sipInfo.dwImDataSize = 0;
    sipInfo.pvImData = NULL;
    SipSetInfo(&sipInfo);
    NotifySoftKbObservers(&visRect);
  } else {
    NotifySoftKbObservers();
  }
  sSIPInTransition = PR_FALSE;
}

void nsWindowCE::CreateSoftKeyMenuBar(HWND wnd)
{
  if (!wnd)
    return;
  
  static HWND sSoftKeyMenuBar = nsnull;
  
  if (sSoftKeyMenuBar != nsnull)
    return;
  
  SHMENUBARINFO mbi;
  ZeroMemory(&mbi, sizeof(SHMENUBARINFO));
  mbi.cbSize = sizeof(SHMENUBARINFO);
  mbi.hwndParent = wnd;
  
  //  On windows ce smartphone, events never occur if the
  //  menubar is empty.  This doesn't work: 
  //  mbi.dwFlags = SHCMBF_EMPTYBAR;
  
  mbi.nToolBarId = IDC_DUMMY_CE_MENUBAR;
  mbi.hInstRes   = GetModuleHandle(NULL);
  
  if (!SHCreateMenuBar(&mbi))
    return;
  
  SetWindowPos(mbi.hwndMB, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE);
  
  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TBACK,
              MAKELPARAM(SHMBOF_NODEFAULT | SHMBOF_NOTIFY,
                         SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
  
  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TSOFT1, 
              MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                          SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
  
  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TSOFT2, 
              MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                          SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
  
  sSoftKeyMenuBar = mbi.hwndMB;
}
#endif  //defined(WINCE_HAVE_SOFTKB)

typedef struct ECWWindows
{
  LPARAM      params;
  WNDENUMPROC func;
  HWND        parent;
} ECWWindows;

static BOOL CALLBACK MyEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
  ECWWindows *myParams = (ECWWindows*) lParam;
  
  if (IsChild(myParams->parent, hwnd))
  {
    return myParams->func(hwnd, myParams->params);
  }
  return TRUE;
}

BOOL nsWindowCE::EnumChildWindows(HWND inParent, WNDENUMPROC inFunc, LPARAM inParam)
{
  ECWWindows myParams;
  myParams.params = inParam;
  myParams.func   = inFunc;
  myParams.parent = inParent;
  
  return EnumWindows(MyEnumWindowsProc, (LPARAM) &myParams);
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: nsIWidget impl.
 **
 ** CE specifric nsIWidget impl. and associated helper methods
 ** used by nsWindow.
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: Window styles utilities
 *
 * Return the proper windows styles and extended styles.
 *
 **************************************************************/

// Return nsWindow styles
DWORD nsWindow::WindowStyle()
{
  DWORD style;

  /* On Windows Mobile, we want very simple window styles; this is
   * just optimizing for full-screen apps that don't want any
   * titlebar/etc.  UI.  We should probably allow apps some
   * finer-grained control over these types at some point, but for now
   * this will work fine.  If we're on Windows CE, we probably have a
   * full window manager, so we make dialog/toplevel windows be real
   * windows.  In addition, we do the post-processing on the style
   * (e.g. disabling the thick resize window if we don't have resize
   * handles specified in the style).
   */

  /* Note: On Windows CE (and presumably Mobile), WS_OVERLAPPED provides
   * space for a menu bar in the window, which we don't want; it shouldn't
   * be used. */

  /* on CE, WS_OVERLAPPED == WS_BORDER | WS_CAPTION, so don't use OVERLAPPED, just set the
   * separate bits directly for clarity */
  switch (mWindowType) {
    case eWindowType_child:
      style = WS_CHILD;
      break;

    case eWindowType_dialog:
      style = WS_BORDER | WS_POPUP;
#if !defined(WINCE_WINDOWS_MOBILE)
      style |= WS_SYSMENU;
      if (mBorderStyle != eBorderStyle_default)
        style |= WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
#endif
      break;

    case eWindowType_popup:
      style = WS_POPUP;
      break;

    default:
      NS_ERROR("unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      style = WS_BORDER;
#if !defined(WINCE_WINDOWS_MOBILE)
      style |= WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
#endif
      break;
  }

#ifndef WINCE_WINDOWS_MOBILE
  if (mBorderStyle != eBorderStyle_default && mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_border))
      style &= ~WS_BORDER;

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_title)) {
      style &= ~WS_DLGFRAME;
      style |= WS_POPUP;
      style &= ~WS_CHILD;
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
#endif // #ifndef WINCE_WINDOWS_MOBILE

  VERIFY_WINDOW_STYLE(style);
  return style;
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetSizeMode
 *
 * Z-order, positioning, restore, minimize, and maximize.
 *
 **************************************************************/

// Maximize, minimize or restore the window.
NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode)
{
#if defined(WINCE_HAVE_SOFTKB)
  if (aMode == 0 && mSizeMode == 3  && nsWindowCE::sSIPInTransition) {
      // ignore the size mode being set to normal by the SIP resizing us
    return NS_OK;
  }
#endif
  nsresult rv;

  // Let's not try and do anything if we're already in that state.
  // (This is needed to prevent problems when calling window.minimize(), which
  // calls us directly, and then the OS triggers another call to us.)
  if (aMode == mSizeMode)
    return NS_OK;

#ifdef WINCE_WINDOWS_MOBILE
  // on windows mobile, dialogs and top level windows are full screen
  // This is partly due to the lack of a GetWindowPlacement.
  if (mWindowType == eWindowType_dialog || mWindowType == eWindowType_toplevel) {
    if (aMode == nsSizeMode_Normal)
      aMode = nsSizeMode_Maximized;
  }

  // also on windows mobile, we never minimize.
  if (aMode == nsSizeMode_Minimized)
    return NS_OK;
#endif

  // save the requested state
  rv = nsBaseWidget::SetSizeMode(aMode);
  if (NS_SUCCEEDED(rv) && mIsVisible) {
    int mode;

    switch (aMode) {
      case nsSizeMode_Fullscreen :
      case nsSizeMode_Maximized :
        mode = SW_MAXIMIZE;
        break;
      default :
        mode = SW_RESTORE;
    }
    ::ShowWindow(mWnd, mode);
  }
  return rv;
}

/**************************************************************
 *
 * SECTION: nsIWidget::EnableDragDrop
 *
 * Enables/Disables drag and drop of files on this widget.
 *
 **************************************************************/

NS_METHOD nsWindow::EnableDragDrop(PRBool aEnable)
{
  return NS_ERROR_FAILURE;
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Native message handling
 **
 ** Main Windows message handlers and OnXXX handlers for
 ** windowing events.
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: OnXXX message handlers
 *
 * For message handlers that need to be broken out or
 * implemented in specific win platform code.
 *
 **************************************************************/

// This needs to move into nsIDOMKeyEvent.idl && nsGUIEvent.h
PRBool nsWindow::OnHotKey(WPARAM wParam, LPARAM lParam)
{
  // SmartPhones has a one or two menu buttons at the
  // bottom of the screen.  They are dispatched via a
  // menu resource, rather then a hotkey.  To make
  // this look consistent, we have mapped this menu to
  // fire hotkey events.  See
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/win_ce/html/pwc_TheBackButtonandOtherInterestingButtons.asp
  
  if (VK_TSOFT1 == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam))))
  {
    keybd_event(VK_F19, 0, 0, 0);
    keybd_event(VK_F19, 0, KEYEVENTF_KEYUP, 0);
    return PR_FALSE;
  }
  
  if (VK_TSOFT2 == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam))))
  {
    keybd_event(VK_F20, 0, 0, 0);
    keybd_event(VK_F20, 0, KEYEVENTF_KEYUP, 0);
    return PR_FALSE;
  }
  
  if (VK_TBACK == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam))))
  {
    keybd_event(VK_BACK, 0, 0, 0);
    keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
    return PR_FALSE;
  }

  switch (wParam) 
  {
    case VK_APP1:
      keybd_event(VK_F1, 0, 0, 0);
      keybd_event(VK_F1, 0, KEYEVENTF_KEYUP, 0);
      break;

    case VK_APP2:
      keybd_event(VK_F2, 0, 0, 0);
      keybd_event(VK_F2, 0, KEYEVENTF_KEYUP, 0);
      break;

    case VK_APP3:
      keybd_event(VK_F3, 0, 0, 0);
      keybd_event(VK_F3, 0, KEYEVENTF_KEYUP, 0);
      break;

    case VK_APP4:
      keybd_event(VK_F4, 0, 0, 0);
      keybd_event(VK_F4, 0, KEYEVENTF_KEYUP, 0);
      break;

    case VK_APP5:
      keybd_event(VK_F5, 0, 0, 0);
      keybd_event(VK_F5, 0, KEYEVENTF_KEYUP, 0);
      break;

    case VK_APP6:
      keybd_event(VK_F6, 0, 0, 0);
      keybd_event(VK_F6, 0, KEYEVENTF_KEYUP, 0);
      break;
  }
  return PR_FALSE;
}
