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
PRBool          nsWindow::sSoftKeyboardState      = PR_FALSE;
PRBool          nsWindowCE::sSIPInTransition      = PR_FALSE;
TriStateBool    nsWindowCE::sShowSIPButton        = TRI_UNKNOWN;
TriStateBool    nsWindowCE::sHardKBPresence       = TRI_UNKNOWN;
HWND            nsWindowCE::sSoftKeyMenuBarHandle = NULL;
RECT            nsWindowCE::sDefaultSIPRect       = {0, 0, 0, 0};
HWND            nsWindowCE::sMainWindowHandle     = NULL;
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
void nsWindowCE::OnSoftKbSettingsChange(HWND wnd, LPRECT visRect)
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

  if (wnd) {
    HWND wndMain = nsWindow::GetTopLevelHWND(wnd);
    RECT winRect;
    ::GetWindowRect(wndMain, &winRect);
    if (winRect.bottom != visRect->bottom) {
      if (winRect.bottom < visRect->bottom) {
        // Soft keyboard has been hidden, have to hide the SIP button as well
        if (sSoftKeyMenuBarHandle)
          ShowWindow(sSoftKeyMenuBarHandle, SW_HIDE);

        SHFullScreen(wndMain, SHFS_HIDESIPBUTTON);
      }

      winRect.bottom = visRect->bottom;
      MoveWindow(wndMain, winRect.left, winRect.top, winRect.right - winRect.left, winRect.bottom - winRect.top, TRUE);
    }
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

void nsWindowCE::ToggleSoftKB(HWND wnd, PRBool show)
{
  if (sHardKBPresence == TRI_UNKNOWN)
    CheckKeyboardStatus();

  if (sHardKBPresence == TRI_TRUE) {
    if (GetSliderStateOpen() != TRI_FALSE) {
      show = PR_FALSE;
      sShowSIPButton = TRI_FALSE;
    }
  }

  sSIPInTransition = PR_TRUE;

  SHSipPreference(wnd, show ? SIP_UP : SIP_DOWN);

  if (sShowSIPButton == TRI_UNKNOWN) {
    // Set it to a known value to avoid checking preferences every time
    // Note: ui.sip.showSIPButton preference change requires browser restart
    sShowSIPButton = TRI_TRUE;

    PRBool tmpBool = PR_FALSE;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      nsCOMPtr<nsIPrefBranch> prefBranch;
      prefs->GetBranch(0, getter_AddRefs(prefBranch));
      if (prefBranch) {
        nsresult rv = prefBranch->GetBoolPref("ui.sip.showSIPButton", &tmpBool);
        if (NS_SUCCEEDED(rv)) {
          sShowSIPButton = tmpBool ? TRI_TRUE : TRI_FALSE;

          if (sShowSIPButton == TRI_FALSE) {
            // Move the SIP rect to the bottom of the screen
            SIPINFO sipInfo;
            memset(&sipInfo, 0, sizeof(SIPINFO));
            sipInfo.cbSize = sizeof(SIPINFO);
            if (SipGetInfo(&sipInfo)) {
              // Store the original rect
              sDefaultSIPRect = sipInfo.rcSipRect;

              // Move the SIP to the bottom of the screen
              RECT sipRect = sipInfo.rcSipRect;
              int sipShift = GetSystemMetrics(SM_CYSCREEN) - sipRect.bottom;
              sipRect.top += sipShift;
              sipRect.bottom += sipShift;
              SipSetDefaultRect(&sipRect);

              // Re-select the IM to apply the change
              CLSID clsid;
              if (SipGetCurrentIM(&clsid))
                SipSetCurrentIM(&clsid);
            }
          }
        }
      }
    }
  }

  PRBool showSIPButton = show;
  if (sShowSIPButton == TRI_FALSE)
    showSIPButton = PR_FALSE;

  if (!showSIPButton) {
    HWND hWndSIPB = FindWindowW(L"MS_SIPBUTTON", NULL);
    if (hWndSIPB)
      ShowWindow(hWndSIPB, SW_HIDE);
  }

  if (sSoftKeyMenuBarHandle) {
    ShowWindow(sSoftKeyMenuBarHandle, showSIPButton ? SW_SHOW: SW_HIDE);
    if (showSIPButton)
      SetWindowPos(sSoftKeyMenuBarHandle, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
  }

  sSIPInTransition = PR_FALSE;
}

void nsWindowCE::CreateSoftKeyMenuBar(HWND wnd)
{
  if (!wnd)
    return;

  sMainWindowHandle = wnd;
  
  if (sSoftKeyMenuBarHandle != NULL)
    return;
  
  SHMENUBARINFO mbi;
  ZeroMemory(&mbi, sizeof(SHMENUBARINFO));
  mbi.cbSize = sizeof(SHMENUBARINFO);
  mbi.hwndParent = wnd;
  
  //  On windows ce smartphone, events never occur if the
  //  menubar is empty.  This doesn't work: 
  //  mbi.dwFlags = SHCMBF_EMPTYBAR;
  
  HMENU dummyMenu = CreateMenu();
  AppendMenu(dummyMenu, MF_STRING, 0, L"");

  mbi.nToolBarId = (UINT)dummyMenu;
  mbi.dwFlags = SHCMBF_HMENU | SHCMBF_HIDDEN;
  mbi.hInstRes = GetModuleHandle(NULL);
  
  if (!SHCreateMenuBar(&mbi))
    return;

  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TBACK,
              MAKELPARAM(SHMBOF_NODEFAULT | SHMBOF_NOTIFY,
                         SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
  
  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TSOFT1, 
              MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                          SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
  
  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TSOFT2, 
              MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                          SHMBOF_NODEFAULT | SHMBOF_NOTIFY));

  sSoftKeyMenuBarHandle = mbi.hwndMB;
}

void nsWindowCE::CheckKeyboardStatus()
{
  HKEY  hKey = 0;
  LONG  result = 0;
  DWORD entryType = 0;
  DWORD hwkbd = 0;
  DWORD paramSize = sizeof(DWORD);
 
  result = RegOpenKeyExW(HKEY_CURRENT_USER, 
                           L"SOFTWARE\\Microsoft\\Shell", 
                           0, 
                           KEY_READ,
                           &hKey); 

  if (result != ERROR_SUCCESS)
  {
    sHardKBPresence = TRI_FALSE;
    return;
  }

  result = RegQueryValueEx(hKey,
                             L"HasKeyboard",
                             NULL, 
                             &entryType, 
                             (LPBYTE)&hwkbd, 
                             &paramSize);

  if (result != ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    sHardKBPresence = TRI_FALSE;
    return;
  }
    
  sHardKBPresence = hwkbd ? TRI_TRUE : TRI_FALSE;
}

TriStateBool nsWindowCE::GetSliderStateOpen()
{

  HKEY  hKey = 0;
  LONG  result = 0;
  DWORD entryType = 0;
  DWORD sliderStateOpen = 0;
  DWORD paramSize = sizeof(DWORD);

  result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                           L"System\\GDI\\Rotation", 
                           0, 
                           KEY_READ,
                           &hKey); 

  if (result != ERROR_SUCCESS)
    return TRI_UNKNOWN;

  result = RegQueryValueEx(hKey,
                             L"Slidekey",
                             NULL, 
                             &entryType, 
                             (LPBYTE)&sliderStateOpen, 
                             &paramSize);

  if (result != ERROR_SUCCESS)
  {
     RegCloseKey(hKey);
     return TRI_UNKNOWN;
  }
    
  return  sliderStateOpen ? TRI_TRUE : TRI_FALSE;
}


void nsWindowCE::ResetSoftKB(HWND wnd)
{
  if (!wnd || wnd != sMainWindowHandle)
    return;

  // Main window is being destroyed - reset all the soft-keyboard settings
  sMainWindowHandle = NULL;
  sSoftKeyMenuBarHandle = NULL;

  if (sDefaultSIPRect.top > 0) {
    SipSetDefaultRect(&sDefaultSIPRect);
    // Re-select the IM to apply the change
    CLSID clsid;
    if (SipGetCurrentIM(&clsid))
      SipSetCurrentIM(&clsid);
    ZeroMemory(&sDefaultSIPRect, sizeof(sDefaultSIPRect));
  }

  // This will make it re-read the pref next time
  sShowSIPButton = TRI_UNKNOWN;
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
    case eWindowType_plugin:
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

void nsWindow::OnWindowPosChanged(WINDOWPOS *wp, PRBool& result)
{
  if (wp == nsnull)
    return;

  // We only care about a resize, so filter out things like z-order
  // changes. Note: there's a WM_MOVE handler above which is why we're
  // not handling them here...
  if (0 == (wp->flags & SWP_NOSIZE)) {
    RECT r;
    PRInt32 newWidth, newHeight;

    ::GetWindowRect(mWnd, &r);

    newWidth  = r.right - r.left;
    newHeight = r.bottom - r.top;
    nsIntRect rect(wp->x, wp->y, newWidth, newHeight);

    if (newWidth > mLastSize.width)
    {
      RECT drect;

      // getting wider
      drect.left   = wp->x + mLastSize.width;
      drect.top    = wp->y;
      drect.right  = drect.left + (newWidth - mLastSize.width);
      drect.bottom = drect.top + newHeight;

      ::RedrawWindow(mWnd, &drect, NULL,
                     RDW_INVALIDATE |
                     RDW_NOERASE |
                     RDW_NOINTERNALPAINT |
                     RDW_ERASENOW |
                     RDW_ALLCHILDREN);
    }
    if (newHeight > mLastSize.height)
    {
      RECT drect;

      // getting taller
      drect.left   = wp->x;
      drect.top    = wp->y + mLastSize.height;
      drect.right  = drect.left + newWidth;
      drect.bottom = drect.top + (newHeight - mLastSize.height);

      ::RedrawWindow(mWnd, &drect, NULL,
                     RDW_INVALIDATE |
                     RDW_NOERASE |
                     RDW_NOINTERNALPAINT |
                     RDW_ERASENOW |
                     RDW_ALLCHILDREN);
    }

    mBounds.width    = newWidth;
    mBounds.height   = newHeight;
    mLastSize.width  = newWidth;
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
    event.mSizeMode = mSizeMode;
    InitEvent(event);
    result = DispatchWindowEvent(&event);
  }
}
