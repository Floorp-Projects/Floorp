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

#include "nsFrameWindow.h"
#include "nsIRollupListener.h"

//-----------------------------------------------------------------------------

extern nsIRollupListener*  gRollupListener;
extern nsIWidget*          gRollupWidget;
extern PRBool              gRollupConsumeRollupEvent;
extern PRUint32            gOS2Flags;

#ifdef DEBUG_FOCUS
  extern int currentWindowIdentifier;
#endif

//=============================================================================
//  nsFrameWindow Setup
//=============================================================================

nsFrameWindow::nsFrameWindow() : nsWindow()
{
  mPrevFrameProc = 0;
  mWindowType  = eWindowType_toplevel;
  mNeedActivation = PR_FALSE;
}

nsFrameWindow::~nsFrameWindow()
{
}

//-----------------------------------------------------------------------------
// Create frame & client windows for an nsFrameWindow object

nsresult nsFrameWindow::CreateWindow(nsWindow* aParent,
                                     HWND aParentWnd,
                                     const nsIntRect& aRect,
                                     PRUint32 aStyle)
{
  // top-level widget windows (i.e nsFrameWindow) are created for
  // windows of types eWindowType_toplevel, *_dialog, & *_invisible
  mIsTopWidgetWindow = PR_TRUE;

  // Create a frame window with a MozillaWindowClass window as its client.
  PRUint32 fcfFlags = GetFCFlags();
  mFrameWnd = WinCreateStdWindow(HWND_DESKTOP,
                                 0,
                                 (ULONG*)&fcfFlags,
                                 kWindowClassName,
                                 "Title",
                                 aStyle,
                                 NULLHANDLE,
                                 0,
                                 &mWnd);
  NS_ENSURE_TRUE(mFrameWnd, NS_ERROR_FAILURE);

  // Hide from the Window List until shown.
  SetWindowListVisibility(PR_FALSE);

  // This prevents a modal dialog from being covered by its disabled parent.
  if (aParentWnd != HWND_DESKTOP) {
    WinSetOwner(mFrameWnd, aParentWnd);
  }

  // Set the frame control HWNDs as properties for use by fullscreen mode.
  HWND hTemp = WinWindowFromID(mFrameWnd, FID_TITLEBAR);
  WinSetProperty(mFrameWnd, "hwndTitleBar", (PVOID)hTemp, 0);
  hTemp = WinWindowFromID(mFrameWnd, FID_SYSMENU);
  WinSetProperty(mFrameWnd, "hwndSysMenu", (PVOID)hTemp, 0);
  hTemp = WinWindowFromID(mFrameWnd, FID_MINMAX);
  WinSetProperty(mFrameWnd, "hwndMinMax", (PVOID)hTemp, 0);

  // Establish a minimum height or else some dialogs will be truncated.
  PRInt32 minHeight;
  if (fcfFlags & FCF_SIZEBORDER) {
    minHeight = 2 * WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);
  } else if (fcfFlags & FCF_DLGBORDER) {
    minHeight = 2 * WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  } else {
    minHeight = 2 * WinQuerySysValue(HWND_DESKTOP, SV_CYBORDER);
  }
  if (fcfFlags & FCF_TITLEBAR) {
    minHeight += WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);
  }

  // Store the frame's adjusted dimensions, move & resize accordingly,
  // then calculate the client's dimensions.
  mBounds = aRect;
  if (mBounds.height < minHeight) {
    mBounds.height = minHeight;
  }
  PRInt32 pmY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN)
                - mBounds.y - mBounds.height;
  WinSetWindowPos(mFrameWnd, 0, mBounds.x, pmY,
                  mBounds.width, mBounds.height, SWP_SIZE | SWP_MOVE);
  UpdateClientSize();

  // Subclass the frame;  the client will be subclassed by Create().
  mPrevFrameProc = WinSubclassWindow(mFrameWnd, fnwpFrame);
  WinSetWindowPtr(mFrameWnd, QWL_USER, this);

  DEBUGFOCUS(Create nsFrameWindow);
  return NS_OK;
}

//-----------------------------------------------------------------------------

PRUint32 nsFrameWindow::GetFCFlags()
{
  PRUint32 style = FCF_TITLEBAR | FCF_SYSMENU | FCF_TASKLIST |
                   FCF_CLOSEBUTTON | FCF_NOBYTEALIGN | FCF_AUTOICON;

  if (mWindowType == eWindowType_dialog) {
    if (mBorderStyle == eBorderStyle_default) {
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
  if (mWindowType == eWindowType_invisible) {
    style &= ~FCF_TASKLIST;
  }

  if (mBorderStyle != eBorderStyle_default &&
      mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_resizeh)) {
      style &= ~FCF_SIZEBORDER;
      style |= FCF_DLGBORDER;
    }
    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_border)) {
      style &= ~(FCF_DLGBORDER | FCF_SIZEBORDER);
    }
    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_title)) {
      style &= ~(FCF_TITLEBAR | FCF_TASKLIST);
    }
    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_close)) {
      style &= ~FCF_CLOSEBUTTON;
    }
    if (mBorderStyle == eBorderStyle_none ||
      !(mBorderStyle & (eBorderStyle_menu | eBorderStyle_close))) {
      style &= ~FCF_SYSMENU;
    }

    // Looks like getting rid of the system menu also does away
    // with the close box. So, we only get rid of the system menu
    // if you want neither it nor the close box.

    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_minimize)) {
      style &= ~FCF_MINBUTTON;
    }
    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_maximize)) {
      style &= ~FCF_MAXBUTTON;
    }
  }

  return style;
}

//=============================================================================
//  Window Operations
//=============================================================================

// For frame windows, 'Show' is equivalent to 'Show & Activate'

NS_METHOD nsFrameWindow::Show(PRBool aState)
{
  if (!mWnd) {
    return NS_OK;
  }

  PRUint32 ulFlags;
  if (!aState) {
    ulFlags = SWP_HIDE | SWP_DEACTIVATE;
  } else {
    ulFlags = SWP_SHOW;

    // Don't activate the window unless the parent is visible
    if (WinIsWindowVisible(WinQueryWindow(mFrameWnd, QW_PARENT))) {
      ulFlags |= SWP_ACTIVATE;
    }

    PRUint32 ulStyle = WinQueryWindowULong(mFrameWnd, QWL_STYLE);
    if (!(ulStyle & WS_VISIBLE)) {
      PRInt32 sizeMode;
      GetSizeMode(&sizeMode);
      if (sizeMode == nsSizeMode_Maximized) {
        ulFlags |= SWP_MAXIMIZE;
      } else if (sizeMode == nsSizeMode_Minimized) {
        ulFlags |= SWP_MINIMIZE;
      } else {
        ulFlags |= SWP_RESTORE;
      }
    }
    if (ulStyle & WS_MINIMIZED) {
      ulFlags |= (SWP_RESTORE | SWP_MAXIMIZE);
    }
  }

  WinSetWindowPos(mFrameWnd, 0, 0, 0, 0, 0, ulFlags);
  SetWindowListVisibility(aState);

  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD nsFrameWindow::GetClientBounds(nsIntRect& aRect)
{
  RECTL rcl = { 0, 0, mBounds.width, mBounds.height };
  WinCalcFrameRect(mFrameWnd, &rcl, TRUE); // provided == frame rect
  aRect.x = rcl.xLeft;
  aRect.y = mBounds.height - rcl.yTop;
  aRect.width = mSizeClient.width;
  aRect.height = mSizeClient.height;
  return NS_OK;
}

//-----------------------------------------------------------------------------

void nsFrameWindow::UpdateClientSize()
{
  RECTL rcl = { 0, 0, mBounds.width, mBounds.height };
  WinCalcFrameRect(mFrameWnd, &rcl, TRUE); // provided == frame rect
  mSizeClient.width  = rcl.xRight - rcl.xLeft;
  mSizeClient.height = rcl.yTop - rcl.yBottom;
}

//-----------------------------------------------------------------------------
// When WM_ACTIVATE is received with the "gaining activation" flag set,
// the frame's wndproc sets mNeedActivation.  Later, when an nsWindow
// gets a WM_FOCUSCHANGED msg with the "gaining focus" flag set, it
// invokes this method on nsFrameWindow to fire an NS_ACTIVATE event.

void nsFrameWindow::ActivateTopLevelWidget()
{
  // Don't fire event if we're minimized or else the window will
  // be restored as soon as the user clicks on it.  When the user
  // explicitly restores it, SetSizeMode() will call this method.

  if (mNeedActivation) {
    PRInt32 sizeMode;
    GetSizeMode(&sizeMode);
    if (sizeMode != nsSizeMode_Minimized) {
      mNeedActivation = PR_FALSE;
      DEBUGFOCUS(NS_ACTIVATE);
      DispatchActivationEvent(NS_ACTIVATE);
    }
  }
  return;
}

//-----------------------------------------------------------------------------
// Just ignore this callback; the correct stuff is done in the frame wp.

PRBool nsFrameWindow::OnReposition(PSWP pSwp)
{
  return PR_TRUE;
}

//-----------------------------------------------------------------------------

void nsFrameWindow::SetWindowListVisibility(PRBool aState)
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
//  nsFrameWindow's Window Procedure
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
        gRollupListener->Rollup(PR_UINT32_MAX, nsnull);
      }
    }
  }

  nsFrameWindow* pFrame = (nsFrameWindow*)WinQueryWindowPtr(hwnd, QWL_USER);
  return pFrame->FrameMessage(msg, mp1, mp2);
}

//-----------------------------------------------------------------------------
// Process messages from the frame

MRESULT nsFrameWindow::FrameMessage(ULONG msg, MPARAM mp1, MPARAM mp2)
{
  MRESULT mresult = 0;
  PRBool  bDone = PR_FALSE;

  switch (msg) {
    case WM_WINDOWPOSCHANGED: {
      PSWP pSwp = (PSWP)mp1;

      // Note that client windows never get 'move' messages
      if (pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE)) {
        POINTL ptl = { pSwp->x, pSwp->y + pSwp->cy };
        ptl.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - ptl.y;
        mBounds.x = ptl.x;
        mBounds.y = ptl.y;
        DispatchMoveEvent(ptl.x, ptl.y);
      }

      // When the frame is sized, do stuff to recalculate client size.
      if (pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE)) {
        mresult = (*mPrevFrameProc)(mFrameWnd, msg, mp1, mp2);
        bDone = PR_TRUE;
        mBounds.width = pSwp->cx;
        mBounds.height = pSwp->cy;
        UpdateClientSize();
        DispatchResizeEvent(mSizeClient.width, mSizeClient.height);
      }

      if (pSwp->fl & (SWP_MAXIMIZE | SWP_MINIMIZE | SWP_RESTORE)) {
        nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);
        if (pSwp->fl & SWP_MAXIMIZE) {
          event.mSizeMode = nsSizeMode_Maximized;
        } else if (pSwp->fl & SWP_MINIMIZE) {
          event.mSizeMode = nsSizeMode_Minimized;
        } else {
          event.mSizeMode = nsSizeMode_Normal;
        }
        InitEvent(event);
        DispatchWindowEvent(&event);
      }

      break;
    }

     // a frame window in kiosk/fullscreen mode must have its frame
     // controls reattached before it's minimized & detached after it's
     // restored;  if this doesn't happen at the correct times, clicking
     // on the icon won't restore it, the sysmenu will have the wrong
     // items, and/or the minmax button will have the wrong buttons

    case WM_ADJUSTWINDOWPOS: {
      if (mChromeHidden && ((PSWP)mp1)->fl & SWP_MINIMIZE) {
        HWND hTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndMinMax");
        if (hTemp) {
          WinSetParent(hTemp, mFrameWnd, TRUE);
        }
        hTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndTitleBar");
        if (hTemp) {
          WinSetParent(hTemp, mFrameWnd, TRUE);
        }
        hTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndSysMenu");
        if (hTemp) {
          WinSetParent(hTemp, mFrameWnd, TRUE);
        }
      }
      break;
    }

    case WM_ADJUSTFRAMEPOS: {
      if (mChromeHidden && ((PSWP)mp1)->fl & SWP_RESTORE) {
        HWND hTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndSysMenu");
        if (hTemp) {
          WinSetParent(hTemp, HWND_OBJECT, TRUE);
        }
        hTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndTitleBar");
        if (hTemp) {
          WinSetParent(hTemp, HWND_OBJECT, TRUE);
        }
        hTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndMinMax");
        if (hTemp) {
          WinSetParent(hTemp, HWND_OBJECT, TRUE);
        }
      }
      break;
    }

    case WM_DESTROY:
      DEBUGFOCUS(frame WM_DESTROY);
      WinSubclassWindow(mFrameWnd, mPrevFrameProc);
      WinSetWindowPtr(mFrameWnd, QWL_USER, 0);
      WinRemoveProperty(mFrameWnd, "hwndTitleBar");
      WinRemoveProperty(mFrameWnd, "hwndSysMenu");
      WinRemoveProperty(mFrameWnd, "hwndMinMax");
      WinRemoveProperty(mFrameWnd, "ulStyle");
      break;

    case WM_INITMENU:
      // If we are in fullscreen/kiosk mode, disable maximize menu item
      if (mChromeHidden &&
          SHORT1FROMMP(mp1) == SC_SYSMENU &&
          WinQueryWindowULong(mFrameWnd, QWL_STYLE) & WS_MINIMIZED) {
        MENUITEM menuitem;
        WinSendMsg(WinWindowFromID(mFrameWnd, FID_SYSMENU), MM_QUERYITEM,
                   MPFROM2SHORT(SC_SYSMENU, FALSE), MPARAM(&menuitem));
        mresult = (*mPrevFrameProc)(mFrameWnd, msg, mp1, mp2);
        WinEnableMenuItem(menuitem.hwndSubMenu, SC_MAXIMIZE, FALSE);
        bDone = PR_TRUE;
      }
      break;

    case WM_SYSCOMMAND:
      // If we are in fullscreen/kiosk mode, don't honor maximize requests
      if (mChromeHidden &&
          SHORT1FROMMP(mp1) == SC_MAXIMIZE &&
          WinQueryWindowULong(mFrameWnd, QWL_STYLE) & WS_MINIMIZED) {
        bDone = PR_TRUE;
      }
      break;

    // When the frame is activated, set a flag to be acted on after
    // PM has finished changing focus.  When deactivated, dispatch
    // the event immediately because it doesn't affect the focus.
    case WM_ACTIVATE:
      DEBUGFOCUS(WM_ACTIVATE);
      if (mp1) {
        mNeedActivation = PR_TRUE;
      } else {
        mNeedActivation = PR_FALSE;
        DEBUGFOCUS(NS_DEACTIVATE);
        DispatchActivationEvent(NS_DEACTIVATE);
        // Prevent the frame from automatically focusing any window
        // when it's reactivated.  Let moz set the focus to avoid
        // having non-widget children of plugins focused in error.
        WinSetWindowULong(mFrameWnd, QWL_HWNDFOCUSSAVE, 0);
      }
      break;
  }

  if (!bDone) {
    mresult = (*mPrevFrameProc)(mFrameWnd, msg, mp1, mp2);
  }

  return mresult;
}

//=============================================================================

