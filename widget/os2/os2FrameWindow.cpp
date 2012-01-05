/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rich Walsh <dragtext@e-vertise.com>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM are
 * Copyright (c) International Business Machines Corporation, 2000
 *
 */
//=============================================================================

#include "nsWindow.h"
#include "os2FrameWindow.h"
#include "nsIRollupListener.h"
#include "nsIScreenManager.h"
#include "nsOS2Uni.h"

//-----------------------------------------------------------------------------
// External Variables (in nsWindow.cpp)

extern nsIRollupListener*  gRollupListener;
extern nsIWidget*          gRollupWidget;
extern bool                gRollupConsumeRollupEvent;
extern PRUint32            gOS2Flags;

#ifdef DEBUG_FOCUS
  extern int currentWindowIdentifier;
#endif

//-----------------------------------------------------------------------------
// Debug

#ifdef DEBUG_FOCUS
  #define DEBUGFOCUS(what) fprintf(stderr, "[%8x]  %8lx  (%02d)  "#what"\n", \
                                   (int)this, mFrameWnd, mWindowIdentifier)
#else
  #define DEBUGFOCUS(what)
#endif

//=============================================================================
//  os2FrameWindow Setup
//=============================================================================

os2FrameWindow::os2FrameWindow(nsWindow* aOwner)
{
  mOwner            = aOwner;
  mFrameWnd         = 0;
  mTitleBar         = 0;
  mSysMenu          = 0;
  mMinMax           = 0;
  mSavedStyle       = 0;
  mFrameIcon        = 0;
  mChromeHidden     = false;
  mNeedActivation   = false;
  mPrevFrameProc    = 0;
  mFrameBounds      = nsIntRect(0, 0, 0, 0);
}

//-----------------------------------------------------------------------------

os2FrameWindow::~os2FrameWindow()
{
  if (mFrameIcon) {
    WinFreeFileIcon(mFrameIcon);
    mFrameIcon = 0;
  }
}

//-----------------------------------------------------------------------------
// Create frame & client windows for an os2FrameWindow object;  return the
// handle of the client window.  This is the only method that manipulates
// the client window - all other operations on it are handled by nsWindow.

HWND os2FrameWindow::CreateFrameWindow(nsWindow* aParent,
                                       HWND aParentWnd,
                                       const nsIntRect& aRect,
                                       nsWindowType aWindowType,
                                       nsBorderStyle aBorderStyle)
{
  // Create a frame window with a MozillaWindowClass window as its client.
  HWND hClient;
  PRUint32 fcfFlags = GetFCFlags(aWindowType, aBorderStyle);
  mFrameWnd = WinCreateStdWindow(HWND_DESKTOP,
                                 0,
                                 (ULONG*)&fcfFlags,
                                 kWindowClassName,
                                 "Title",
                                 WS_CLIPCHILDREN,
                                 NULLHANDLE,
                                 0,
                                 &hClient);
  if (!mFrameWnd) {
    return 0;
  }

  // Hide from the Window List until shown.
  SetWindowListVisibility(false);

  // This prevents a modal dialog from being covered by its disabled parent.
  if (aParentWnd != HWND_DESKTOP) {
    WinSetOwner(mFrameWnd, aParentWnd);
  }

  // Get the frame control HWNDs for use by fullscreen mode.
  mTitleBar = WinWindowFromID(mFrameWnd, FID_TITLEBAR);
  mSysMenu  = WinWindowFromID(mFrameWnd, FID_SYSMENU);
  mMinMax   = WinWindowFromID(mFrameWnd, FID_MINMAX);

  // Calc the size of a frame needed to contain a client area of the
  // specified width & height.  Without this, eWindowType_dialog windows
  // will be truncated (toplevel windows will still display correctly).
  RECTL rcl = {0, 0, aRect.width, aRect.height};
  WinCalcFrameRect(mFrameWnd, &rcl, FALSE);
  mFrameBounds = nsIntRect(aRect.x, aRect.y,
                           rcl.xRight-rcl.xLeft, rcl.yTop-rcl.yBottom);

  // Move & resize the frame.
  PRInt32 pmY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN)
                - mFrameBounds.y - mFrameBounds.height;
  WinSetWindowPos(mFrameWnd, 0, mFrameBounds.x, pmY,
                  mFrameBounds.width, mFrameBounds.height,
                  SWP_SIZE | SWP_MOVE);

  // Store the client's bounds.  For windows with resizable borders,
  // the width includes the width of the frame controls (minmax, etc.).
  SWP swp;
  WinQueryWindowPos(hClient, &swp);
  mOwner->SetBounds(nsIntRect(swp.x, mFrameBounds.height - swp.y - swp.cy,
                              swp.cx, swp.cy));

  // Subclass the frame.
  mPrevFrameProc = WinSubclassWindow(mFrameWnd, fnwpFrame);
  WinSetWindowPtr(mFrameWnd, QWL_USER, this);

  DEBUGFOCUS(Create os2FrameWindow);
  return hClient;
}

//-----------------------------------------------------------------------------

PRUint32 os2FrameWindow::GetFCFlags(nsWindowType aWindowType,
                                    nsBorderStyle aBorderStyle)
{
  PRUint32 style = FCF_TITLEBAR | FCF_SYSMENU | FCF_TASKLIST |
                   FCF_CLOSEBUTTON | FCF_NOBYTEALIGN | FCF_AUTOICON;

  if (aWindowType == eWindowType_dialog) {
    if (aBorderStyle == eBorderStyle_default) {
      style |= FCF_DLGBORDER;
    } else {
      style |= FCF_SIZEBORDER | FCF_MINMAX;
    }
  }
  else {
    style |= FCF_SIZEBORDER | FCF_MINMAX;
  }

  if (gOS2Flags & kIsDBCS) {
    style |= FCF_DBE_APPSTAT;
  }
  if (aWindowType == eWindowType_invisible) {
    style &= ~FCF_TASKLIST;
  }

  if (aBorderStyle != eBorderStyle_default &&
      aBorderStyle != eBorderStyle_all) {
    if (aBorderStyle == eBorderStyle_none ||
        !(aBorderStyle & eBorderStyle_resizeh)) {
      style &= ~FCF_SIZEBORDER;
      style |= FCF_DLGBORDER;
    }
    if (aBorderStyle == eBorderStyle_none ||
        !(aBorderStyle & eBorderStyle_border)) {
      style &= ~(FCF_DLGBORDER | FCF_SIZEBORDER);
    }
    if (aBorderStyle == eBorderStyle_none ||
        !(aBorderStyle & eBorderStyle_title)) {
      style &= ~(FCF_TITLEBAR | FCF_TASKLIST);
    }
    if (aBorderStyle == eBorderStyle_none ||
        !(aBorderStyle & eBorderStyle_close)) {
      style &= ~FCF_CLOSEBUTTON;
    }
    if (aBorderStyle == eBorderStyle_none ||
      !(aBorderStyle & (eBorderStyle_menu | eBorderStyle_close))) {
      style &= ~FCF_SYSMENU;
    }

    // Looks like getting rid of the system menu also does away
    // with the close box. So, we only get rid of the system menu
    // if you want neither it nor the close box.

    if (aBorderStyle == eBorderStyle_none ||
        !(aBorderStyle & eBorderStyle_minimize)) {
      style &= ~FCF_MINBUTTON;
    }
    if (aBorderStyle == eBorderStyle_none ||
        !(aBorderStyle & eBorderStyle_maximize)) {
      style &= ~FCF_MAXBUTTON;
    }
  }

  return style;
}

//=============================================================================
//  Window Operations
//=============================================================================

// For frame windows, 'Show' is equivalent to 'Show & Activate'.

nsresult os2FrameWindow::Show(bool aState)
{
  PRUint32 ulFlags;
  if (!aState) {
    ulFlags = SWP_HIDE | SWP_DEACTIVATE;
  } else {
    ulFlags = SWP_SHOW | SWP_ACTIVATE;

    PRUint32 ulStyle = WinQueryWindowULong(mFrameWnd, QWL_STYLE);
    PRInt32 sizeMode;
    mOwner->GetSizeMode(&sizeMode);
    if (!(ulStyle & WS_VISIBLE)) {
      if (sizeMode == nsSizeMode_Maximized) {
        ulFlags |= SWP_MAXIMIZE;
      } else if (sizeMode == nsSizeMode_Minimized) {
        ulFlags |= SWP_MINIMIZE;
      } else {
        ulFlags |= SWP_RESTORE;
      }
    } else if (ulStyle & WS_MINIMIZED) {
      if (sizeMode == nsSizeMode_Maximized) {
        ulFlags |= SWP_MAXIMIZE;
      } else {
        ulFlags |= SWP_RESTORE;
      }
    }
  }

  WinSetWindowPos(mFrameWnd, 0, 0, 0, 0, 0, ulFlags);
  SetWindowListVisibility(aState);

  return NS_OK;
}

//-----------------------------------------------------------------------------

void os2FrameWindow::SetWindowListVisibility(bool aState)
{
  HSWITCH hswitch = WinQuerySwitchHandle(mFrameWnd, 0);
  if (hswitch) {
    SWCNTRL swctl;
    WinQuerySwitchEntry(hswitch, &swctl);
    swctl.uchVisibility = aState ? SWL_VISIBLE : SWL_INVISIBLE;
    swctl.fbJump        = aState ? SWL_JUMPABLE : SWL_NOTJUMPABLE;
    WinChangeSwitchEntry(hswitch, &swctl);
  }
}

//=============================================================================
//  Window Positioning
//=============================================================================

nsresult os2FrameWindow::GetBounds(nsIntRect& aRect)
{
  aRect = mFrameBounds;
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult os2FrameWindow::Move(PRInt32 aX, PRInt32 aY)
{
  aY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - mFrameBounds.height - aY;
  WinSetWindowPos(mFrameWnd, 0, aX, aY, 0, 0, SWP_MOVE);
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult os2FrameWindow::Resize(PRInt32 aWidth, PRInt32 aHeight,
                                bool aRepaint)
{
  // When resizing, the coordinates of the window's bottom-left corner have to
  // be adjusted to ensure the position of the top-left corner doesn't change.
  Resize(mFrameBounds.x, mFrameBounds.y, aWidth, aHeight, aRepaint);

  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult os2FrameWindow::Resize(PRInt32 aX, PRInt32 aY,
                                PRInt32 aWidth, PRInt32 aHeight,
                                bool aRepaint)
{
  aY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - aY - aHeight;
  WinSetWindowPos(mFrameWnd, 0, aX, aY, aWidth, aHeight, SWP_MOVE | SWP_SIZE);
  return NS_OK;
}

//=============================================================================
//  Top-level (frame window) Operations
//=============================================================================

// When WM_ACTIVATE is received with the "gaining activation" flag set,
// the frame's wndproc sets mNeedActivation.  Later, when an nsWindow
// gets a WM_FOCUSCHANGED msg with the "gaining focus" flag set, it
// invokes this method on os2FrameWindow to fire an NS_ACTIVATE event.

void os2FrameWindow::ActivateTopLevelWidget()
{
  // Don't fire event if we're minimized or else the window will
  // be restored as soon as the user clicks on it.  When the user
  // explicitly restores it, SetSizeMode() will call this method.
  if (mNeedActivation) {
    PRInt32 sizeMode;
    mOwner->GetSizeMode(&sizeMode);
    if (sizeMode != nsSizeMode_Minimized) {
      mNeedActivation = false;
      DEBUGFOCUS(NS_ACTIVATE);
      mOwner->DispatchActivationEvent(NS_ACTIVATE);
    }
  }
  return;
}

//-----------------------------------------------------------------------------
// Maximize, minimize or restore the window.  When the frame has its
// controls, this method is usually advisory because min/max/restore has
// already occurred.  It only performs these actions when the frame is in
// fullscreen mode or saved window positions are being restored at startup.

nsresult os2FrameWindow::SetSizeMode(PRInt32 aMode)
{
  PRInt32 previousMode;
  mOwner->GetSizeMode(&previousMode);

  // save the new state
  nsresult rv = mOwner->nsBaseWidget::SetSizeMode(aMode);
  if (!NS_SUCCEEDED(rv)) {
    return rv;
  }

  // Minimized windows would get restored involuntarily if we fired an
  // NS_ACTIVATE when the user clicks on them.  Instead, we defer the
  // activation event until the window has explicitly been restored.
  if (previousMode == nsSizeMode_Minimized && previousMode != aMode) {
    DEBUGFOCUS(deferred NS_ACTIVATE);
    ActivateTopLevelWidget();
  }

  ULONG ulStyle = WinQueryWindowULong(mFrameWnd, QWL_STYLE);

  switch (aMode) {
    case nsSizeMode_Normal:
      if (ulStyle & (WS_MAXIMIZED | WS_MINIMIZED)) {
        WinSetWindowPos(mFrameWnd, 0, 0, 0, 0, 0, SWP_RESTORE);
      }
      break;

    case nsSizeMode_Minimized:
      if (!(ulStyle & WS_MINIMIZED)) {
        WinSetWindowPos(mFrameWnd, HWND_BOTTOM, 0, 0, 0, 0,
                        SWP_MINIMIZE | SWP_ZORDER | SWP_DEACTIVATE);
      }
      break;

    case nsSizeMode_Maximized:
      // Don't permit the window to be maximized when in
      // fullscreen mode because it won't be restored correctly.
      if (!(ulStyle & WS_MAXIMIZED) && !mChromeHidden) {
        WinSetWindowPos(mFrameWnd, HWND_TOP, 0, 0, 0, 0,
                        SWP_MAXIMIZE | SWP_ZORDER);
      }
      break;

    // 'nsSizeMode_Fullscreen' is defined but isn't used (as of v1.9.3)
    case nsSizeMode_Fullscreen:
    default:
      break;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// Hide or show the frame & its controls (titlebar, minmax, etc.).

nsresult os2FrameWindow::HideWindowChrome(bool aShouldHide)
{
  // Putting a maximized window into fullscreen mode causes multiple
  // problems if it's later minimized & then restored.  To avoid them,
  // restore maximized windows before putting them in fullscreen mode.
  if (WinQueryWindowULong(mFrameWnd, QWL_STYLE) & WS_MAXIMIZED) {
    WinSetWindowPos(mFrameWnd, 0, 0, 0, 0, 0, SWP_RESTORE | SWP_NOREDRAW);
  }

  HWND hParent;
  if (aShouldHide) {
    hParent = HWND_OBJECT;
    mChromeHidden = true;
  } else {
    hParent = mFrameWnd;
    mChromeHidden = false;
  }

  // Hide or show the frame controls.
  WinSetParent(mTitleBar, hParent, FALSE);
  WinSetParent(mSysMenu, hParent, FALSE);
  WinSetParent(mMinMax, hParent, FALSE);

  // Modify the frame style, then advise it of the changes.
  if (aShouldHide) {
    mSavedStyle = WinQueryWindowULong(mFrameWnd, QWL_STYLE);
    WinSetWindowULong(mFrameWnd, QWL_STYLE, mSavedStyle & ~FS_SIZEBORDER);
    WinSendMsg(mFrameWnd, WM_UPDATEFRAME, 0, 0);
  } else {
    WinSetWindowULong(mFrameWnd, QWL_STYLE, mSavedStyle);
    WinSendMsg(mFrameWnd, WM_UPDATEFRAME,
               MPFROMLONG(FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX), 0);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// On OS/2, if you pass a titlebar > 512 characters, it doesn't display at all.
// We are going to limit our titlebars to 256 just to be on the safe side.

#define MAX_TITLEBAR_LENGTH 256

nsresult os2FrameWindow::SetTitle(const nsAString& aTitle)
{
  PRUnichar* uchtemp = ToNewUnicode(aTitle);
  for (PRUint32 i = 0; i < aTitle.Length(); i++) {
    switch (uchtemp[i]) {
      case 0x2018:
      case 0x2019:
        uchtemp[i] = 0x0027;
        break;
      case 0x201C:
      case 0x201D:
        uchtemp[i] = 0x0022;
        break;
      case 0x2014:
        uchtemp[i] = 0x002D;
        break;
    }
  }

  nsAutoCharBuffer title;
  PRInt32 titleLength;
  WideCharToMultiByte(0, uchtemp, aTitle.Length(), title, titleLength);
  if (titleLength > MAX_TITLEBAR_LENGTH) {
    title[MAX_TITLEBAR_LENGTH] = '\0';
  }
  WinSetWindowText(mFrameWnd, title.Elements());

  // If the chrome is hidden, set the text of the titlebar directly
  if (mChromeHidden) {
    WinSetWindowText(mTitleBar, title.Elements());
  }

  nsMemory::Free(uchtemp);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// This implementation guarantees that sysmenus & minimized windows
// will always have some icon other than the sysmenu default.

nsresult os2FrameWindow::SetIcon(const nsAString& aIconSpec)
{
  static HPOINTER hDefaultIcon = 0;
  HPOINTER        hWorkingIcon = 0;

  // Assume the given string is a local identifier for an icon file.
  nsCOMPtr<nsILocalFile> iconFile;
  mOwner->ResolveIconName(aIconSpec, NS_LITERAL_STRING(".ico"),
                          getter_AddRefs(iconFile));

  // if the file was found, try to use it
  if (iconFile) {
    nsCAutoString path;
    iconFile->GetNativePath(path);

    if (mFrameIcon) {
      WinFreeFileIcon(mFrameIcon);
    }
    mFrameIcon = WinLoadFileIcon(path.get(), FALSE);
    hWorkingIcon = mFrameIcon;
  }

  // if that doesn't work, use the app's icon (let's hope it can be
  // loaded because nobody should have to look at SPTR_APPICON)
  if (!hWorkingIcon) {
    if (!hDefaultIcon) {
      hDefaultIcon = WinLoadPointer(HWND_DESKTOP, 0, 1);
      if (!hDefaultIcon) {
        hDefaultIcon = WinQuerySysPointer(HWND_DESKTOP, SPTR_APPICON, FALSE);
      }
    }
    hWorkingIcon = hDefaultIcon;
  }

  WinSendMsg(mFrameWnd, WM_SETICON, (MPARAM)hWorkingIcon, (MPARAM)0);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Constrain a potential move to fit onscreen.

nsresult os2FrameWindow::ConstrainPosition(bool aAllowSlop,
                                      PRInt32* aX, PRInt32* aY)
{
  // do we have enough info to do anything
  bool doConstrain = false;

  // get our playing field. use the current screen, or failing
  // that for any reason, use device caps for the default screen.
  RECTL screenRect;

  nsCOMPtr<nsIScreenManager> screenmgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    PRInt32 left, top, width, height;

    // zero size rects confuse the screen manager
    width = mFrameBounds.width > 0 ? mFrameBounds.width : 1;
    height = mFrameBounds.height > 0 ? mFrameBounds.height : 1;
    screenmgr->ScreenForRect(*aX, *aY, width, height,
                             getter_AddRefs(screen));
    if (screen) {
      screen->GetAvailRect(&left, &top, &width, &height);
      screenRect.xLeft = left;
      screenRect.xRight = left+width;
      screenRect.yTop = top;
      screenRect.yBottom = top+height;
      doConstrain = true;
    }
  }

#define kWindowPositionSlop 100

  if (doConstrain) {
    if (aAllowSlop) {
      if (*aX < screenRect.xLeft - mFrameBounds.width + kWindowPositionSlop) {
        *aX = screenRect.xLeft - mFrameBounds.width + kWindowPositionSlop;
      } else
      if (*aX >= screenRect.xRight - kWindowPositionSlop) {
        *aX = screenRect.xRight - kWindowPositionSlop;
      }

      if (*aY < screenRect.yTop) {
        *aY = screenRect.yTop;
      } else
      if (*aY >= screenRect.yBottom - kWindowPositionSlop) {
        *aY = screenRect.yBottom - kWindowPositionSlop;
      }
    } else {
      if (*aX < screenRect.xLeft) {
        *aX = screenRect.xLeft;
      } else
      if (*aX >= screenRect.xRight - mFrameBounds.width) {
        *aX = screenRect.xRight - mFrameBounds.width;
      }

      if (*aY < screenRect.yTop) {
        *aY = screenRect.yTop;
      } else
      if (*aY >= screenRect.yBottom - mFrameBounds.height) {
        *aY = screenRect.yBottom - mFrameBounds.height;
      }
    }
  }

  return NS_OK;
}

//=============================================================================
//  os2FrameWindow's Window Procedure
//=============================================================================

// Subclass for frame window

MRESULT EXPENTRY fnwpFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  // check to see if we have a rollup listener registered
  if (gRollupListener && gRollupWidget) {
    if (msg == WM_TRACKFRAME || msg == WM_MINMAXFRAME ||
        msg == WM_BUTTON1DOWN || msg == WM_BUTTON2DOWN ||
        msg == WM_BUTTON3DOWN) {
      // Rollup if the event is outside the popup
      if (!nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget)) {
        gRollupListener->Rollup(PR_UINT32_MAX);
      }
    }
  }

  os2FrameWindow* pFrame = (os2FrameWindow*)WinQueryWindowPtr(hwnd, QWL_USER);
  return pFrame->ProcessFrameMessage(msg, mp1, mp2);
}

//-----------------------------------------------------------------------------
// Process messages from the frame

MRESULT os2FrameWindow::ProcessFrameMessage(ULONG msg, MPARAM mp1, MPARAM mp2)
{
  MRESULT mresult = 0;
  bool    isDone = false;

  switch (msg) {
    case WM_WINDOWPOSCHANGED: {
      PSWP pSwp = (PSWP)mp1;

      // Don't save the new position or size of a minimized
      // window, or else it won't be restored correctly.
      if (pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE)) {
        POINTL ptl = { pSwp->x, pSwp->y + pSwp->cy };
        ptl.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - ptl.y;
        mFrameBounds.x = ptl.x;
        mFrameBounds.y = ptl.y;
        mOwner->DispatchMoveEvent(ptl.x, ptl.y);
      }

      // Save the frame's bounds, then call the default wndproc
      // so the client can handle its WM_WINDOWPOSCHANGED msg now.
      if (pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE)) {
        mFrameBounds.width = pSwp->cx;
        mFrameBounds.height = pSwp->cy;
        mresult = (*mPrevFrameProc)(mFrameWnd, msg, mp1, mp2);
        isDone = true;
      }

      if (pSwp->fl & (SWP_MAXIMIZE | SWP_MINIMIZE | SWP_RESTORE)) {
        nsSizeModeEvent event(true, NS_SIZEMODE, mOwner);
        if (pSwp->fl & SWP_MAXIMIZE) {
          event.mSizeMode = nsSizeMode_Maximized;
        } else if (pSwp->fl & SWP_MINIMIZE) {
          event.mSizeMode = nsSizeMode_Minimized;
        } else {
          event.mSizeMode = nsSizeMode_Normal;
        }
        mOwner->InitEvent(event);
        mOwner->DispatchWindowEvent(&event);
      }
      break;
    }

     // A frame window in kiosk/fullscreen mode must have its frame
     // controls reattached before it's minimized & detached after it's
     // restored.  If this doesn't happen at the correct times, clicking
     // on the icon won't restore it, the sysmenu will have the wrong
     // items, and/or the minmax button will have the wrong buttons.

    case WM_ADJUSTWINDOWPOS:
      if (mChromeHidden && ((PSWP)mp1)->fl & SWP_MINIMIZE) {
        WinSetParent(mTitleBar, mFrameWnd, TRUE);
        WinSetParent(mSysMenu, mFrameWnd, TRUE);
        WinSetParent(mMinMax, mFrameWnd, TRUE);
      }
      break;

    case WM_ADJUSTFRAMEPOS:
      if (mChromeHidden && ((PSWP)mp1)->fl & SWP_RESTORE) {
        WinSetParent(mTitleBar, HWND_OBJECT, TRUE);
        WinSetParent(mSysMenu, HWND_OBJECT, TRUE);
        WinSetParent(mMinMax, HWND_OBJECT, TRUE);
      }
      break;

    case WM_DESTROY:
      DEBUGFOCUS(frame WM_DESTROY);
      WinSubclassWindow(mFrameWnd, mPrevFrameProc);
      WinSetWindowPtr(mFrameWnd, QWL_USER, 0);
      break;

    case WM_INITMENU:
      // If we are in fullscreen/kiosk mode, disable maximize menu item.
      if (mChromeHidden &&
          SHORT1FROMMP(mp1) == SC_SYSMENU &&
          WinQueryWindowULong(mFrameWnd, QWL_STYLE) & WS_MINIMIZED) {
        MENUITEM menuitem;
        WinSendMsg(WinWindowFromID(mFrameWnd, FID_SYSMENU), MM_QUERYITEM,
                   MPFROM2SHORT(SC_SYSMENU, FALSE), MPARAM(&menuitem));
        mresult = (*mPrevFrameProc)(mFrameWnd, msg, mp1, mp2);
        WinEnableMenuItem(menuitem.hwndSubMenu, SC_MAXIMIZE, FALSE);
        isDone = true;
      }
      break;

    case WM_SYSCOMMAND:
      // If we are in fullscreen/kiosk mode, don't honor maximize requests.
      if (mChromeHidden &&
          SHORT1FROMMP(mp1) == SC_MAXIMIZE &&
          WinQueryWindowULong(mFrameWnd, QWL_STYLE) & WS_MINIMIZED) {
        isDone = true;
      }
      break;

    // When the frame is activated, set a flag to be acted on after
    // PM has finished changing focus.  When deactivated, dispatch
    // the event immediately because it doesn't affect the focus.
    case WM_ACTIVATE:
      DEBUGFOCUS(WM_ACTIVATE);
      if (mp1) {
        mNeedActivation = true;
      } else {
        mNeedActivation = false;
        DEBUGFOCUS(NS_DEACTIVATE);
        mOwner->DispatchActivationEvent(NS_DEACTIVATE);
        // Prevent the frame from automatically focusing any window
        // when it's reactivated.  Let moz set the focus to avoid
        // having non-widget children of plugins focused in error.
        WinSetWindowULong(mFrameWnd, QWL_HWNDFOCUSSAVE, 0);
      }
      break;
  }

  if (!isDone) {
    mresult = (*mPrevFrameProc)(mFrameWnd, msg, mp1, mp2);
  }

  return mresult;
}

//=============================================================================

