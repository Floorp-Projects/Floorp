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
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/23/2000       IBM Corp.      Fix missing title bars on profile wizard windows.
 * 04/11/2000       IBM Corp.      Remove assertion.
 * 05/10/2000       IBM Corp.      Correct initial position of frame w/titlebar
 * 06/21/2000       IBM Corp.      Use rollup listener from nsWindow
 */

// Frame window - produced when NS_WINDOW_CID is required.

#include "nsFrameWindow.h"
#include "nsIRollupListener.h"
#include "nsIDeviceContext.h"
#include "nsIComponentManager.h"
#include "nsGfxCIID.h"

extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;
extern PRBool              gRollupConsumeRollupEvent;

#ifdef DEBUG_FOCUS
  extern int currentWindowIdentifier;
#endif

nsFrameWindow::nsFrameWindow() : nsWindow()
{
   fnwpDefFrame = 0;
   mWindowType  = eWindowType_toplevel;
   mNeedActivation = PR_FALSE;
}

nsFrameWindow::~nsFrameWindow()
{
}

void nsFrameWindow::SetWindowListVisibility( PRBool bState)
{
   HSWITCH hswitch;
   SWCNTRL swctl;

   hswitch = WinQuerySwitchHandle(mFrameWnd, 0);
   if( hswitch)
   {
      WinQuerySwitchEntry( hswitch, &swctl);
      swctl.uchVisibility = bState ? SWL_VISIBLE : SWL_INVISIBLE;
      swctl.fbJump        = bState ? SWL_JUMPABLE : SWL_NOTJUMPABLE;
      WinChangeSwitchEntry( hswitch, &swctl);
   }
}

// Called in the PM thread.
void nsFrameWindow::RealDoCreate( HWND hwndP, nsWindow *aParent,
                                  const nsIntRect &aRect,
                                  EVENT_CALLBACK aHandleEventFunction,
                                  nsIDeviceContext *aContext,
                                  nsIAppShell *aAppShell,
                                  nsWidgetInitData *aInitData, HWND hwndO)
{
   nsIntRect rect = aRect;
   if( aParent)  // Offset rect by position of owner
   {
      nsIntRect clientRect;
      aParent->GetBounds(rect);
      aParent->GetClientBounds(clientRect);
      rect.x += aRect.x + clientRect.x;
      rect.y += aRect.y + clientRect.y;
      rect.width = aRect.width;
      rect.height = aRect.height;
      hwndP = aParent->GetMainWindow();
      rect.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - (rect.y + rect.height);
   }
   else          // Use original rect, no owner window
   {
      rect = aRect;
      rect.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - (aRect.y + aRect.height);
   }

#if DEBUG_sobotka
   printf("\nIn nsFrameWindow::RealDoCreate:\n");
   printf("   hwndP = %lu\n", hwndP);
   printf("   aParent = 0x%lx\n", &aParent);
   printf("   aRect = %ld, %ld, %ld, %ld\n", aRect.x, aRect.y, aRect.height, aRect.width);
#endif

   ULONG fcfFlags = GetFCFlags();

   ULONG style = WindowStyle();
   if( aInitData)
   {
      if( aInitData->clipChildren)
         style |= WS_CLIPCHILDREN;
#if 0
      //
      // Windows has a slightly different idea of what the implications are
      // of a window having or not having the CLIPSIBLINGS style.
      // All 'canvas' components we create must have clipsiblings, or
      // strange things happen & performance actually degrades.
      //
      else
        style &= ~WS_CLIPCHILDREN;
#endif

      if( aInitData->clipSiblings)
         style |= WS_CLIPSIBLINGS;
      else
         style &= ~WS_CLIPSIBLINGS;
   }

#ifdef DEBUG_FOCUS
   mWindowIdentifier = currentWindowIdentifier;
   currentWindowIdentifier++;
   if (aInitData && (aInitData->mWindowType == eWindowType_toplevel))
     DEBUGFOCUS(Create Frame Window);
   else
     DEBUGFOCUS(Create Window);
#endif

   mFrameWnd = WinCreateStdWindow( HWND_DESKTOP,
                                   0,
                                   &fcfFlags,
                                   WindowClass(),
                                   "Title",
                                   style,
                                   NULLHANDLE,
                                   0,
                                   &mWnd);

  
   /* Because WinCreateStdWindow doesn't take an owner, we have to set it */
   if (hwndP)
     WinSetOwner(mFrameWnd, hwndP);


   /* Set some HWNDs and style into properties for fullscreen mode */
   HWND hwndTitleBar = WinWindowFromID(mFrameWnd, FID_TITLEBAR);
   WinSetProperty(mFrameWnd, "hwndTitleBar", (PVOID)hwndTitleBar, 0);
   HWND hwndSysMenu = WinWindowFromID(mFrameWnd, FID_SYSMENU);
   WinSetProperty(mFrameWnd, "hwndSysMenu", (PVOID)hwndSysMenu, 0);
   HWND hwndMinMax = WinWindowFromID(mFrameWnd, FID_MINMAX);
   WinSetProperty(mFrameWnd, "hwndMinMax", (PVOID)hwndMinMax, 0);


   SetWindowListVisibility( PR_FALSE);  // Hide from Window List until shown

   NS_ASSERTION( mFrameWnd, "Couldn't create frame");

   // Frames have a minimum height based on the pieces they are created with,
   // such as titlebar, menubar, frame borders, etc.  We need this minimum
   // height so we can correctly set the frame position (coordinate flipping).
   nsIntRect frameRect = rect;
   long minheight; 

   if ( fcfFlags & FCF_SIZEBORDER) {
      minheight = 2 * WinQuerySysValue( HWND_DESKTOP, SV_CYSIZEBORDER);
   }
   else if ( fcfFlags & FCF_DLGBORDER) {
      minheight = 2 * WinQuerySysValue( HWND_DESKTOP, SV_CYDLGFRAME);
   }
   else {
      minheight = 2 * WinQuerySysValue( HWND_DESKTOP, SV_CYBORDER);
   }
   if ( fcfFlags & FCF_TITLEBAR) {
      minheight += WinQuerySysValue( HWND_DESKTOP, SV_CYTITLEBAR);
   }
   if ( frameRect.height < minheight) {
      frameRect.height = minheight;
   }

   // Set up parent data - don't addref to avoid circularity
   mParent = nsnull;

   // Make sure we have a device context from somewhere
   if( aContext)
   {
      mContext = aContext;
      NS_ADDREF(mContext);
   }
   else
   {
      nsresult rc;
      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

      rc = CallCreateInstance(kDeviceContextCID, &mContext);
      if( NS_SUCCEEDED(rc))
         mContext->Init(this);
#ifdef DEBUG
      else
         printf( "Couldn't find DC instance for nsWindow\n");
#endif
   }

   // Record bounds.  This is XP, the rect of the entire main window in
   // parent space.  Returned by GetBounds().
   // NB: We haven't subclassed yet, so callbacks to change mBounds won't
   //     have happened!
   mBounds = frameRect;
   mBounds.height = frameRect.height;

   // Record passed in things
   mEventCallback = aHandleEventFunction;

   if( mParent)
      mParent->AddChild( this);

   // call the event callback to notify about creation

   DispatchStandardEvent( NS_CREATE );
   SubclassWindow(TRUE);
   PostCreateWidget();

   // Subclass frame
   fnwpDefFrame = WinSubclassWindow( mFrameWnd, fnwpFrame);
   WinSetWindowPtr( mFrameWnd, QWL_USER, this);


   WinSetWindowPos(mFrameWnd, 0, frameRect.x, frameRect.y, frameRect.width, frameRect.height, SWP_SIZE | SWP_MOVE);
}


void nsFrameWindow::UpdateClientSize()
{
   RECTL rcl = { 0, 0, mBounds.width, mBounds.height };
   WinCalcFrameRect( mFrameWnd, &rcl, TRUE); // provided == frame rect
   mSizeClient.width = rcl.xRight - rcl.xLeft;
   mSizeClient.height = rcl.yTop - rcl.yBottom;
   mSizeBorder.width = (mBounds.width - mSizeClient.width) / 2;
   mSizeBorder.height = (mBounds.height - mSizeClient.height) / 2;
}

nsresult nsFrameWindow::GetClientBounds( nsIntRect &aRect)
{
   RECTL rcl = { 0, 0, mBounds.width, mBounds.height };
   WinCalcFrameRect( mFrameWnd, &rcl, TRUE); // provided == frame rect
   aRect.x = rcl.xLeft;
   aRect.y = mBounds.height - rcl.yTop;
   aRect.width = mSizeClient.width;
   aRect.height = mSizeClient.height;
   return NS_OK;
}

// Just ignore this callback; the correct stuff is done in the frame wp.
PRBool nsFrameWindow::OnReposition( PSWP pSwp)
{
   return PR_TRUE;
}

// For frame windows, 'Show' is equivalent to 'Show & Activate'
nsresult nsFrameWindow::Show( PRBool bState)
{
   if( mWnd)
   {
      ULONG ulFlags;
      if( bState) {
         ULONG ulStyle = WinQueryWindowULong( GetMainWindow(), QWL_STYLE);
         ulFlags = SWP_SHOW;
         /* Don't activate the window unless the parent is visible */
         if (WinIsWindowVisible(WinQueryWindow(GetMainWindow(), QW_PARENT)))
           ulFlags |= SWP_ACTIVATE;
         if (!( ulStyle & WS_VISIBLE)) {
            PRInt32 sizeMode;
            GetSizeMode( &sizeMode);
            if ( sizeMode == nsSizeMode_Maximized) {
               ulFlags |= SWP_MAXIMIZE;
            } else if ( sizeMode == nsSizeMode_Minimized) {
               ulFlags |= SWP_MINIMIZE;
            } else {
               ulFlags |= SWP_RESTORE;
            }
         }
         if( ulStyle & WS_MINIMIZED)
            ulFlags |= (SWP_RESTORE | SWP_MAXIMIZE);
      }
      else
         ulFlags = SWP_HIDE | SWP_DEACTIVATE;
      WinSetWindowPos( GetMainWindow(), NULLHANDLE, 0L, 0L, 0L, 0L, ulFlags);
      SetWindowListVisibility( bState);
   }

   return NS_OK;
}

// When WM_ACTIVATE is received with the "gaining activation" flag set,
// the frame's wndproc sets mNeedActivation.  Later, when an nsWindow
// gets a WM_FOCUSCHANGED msg with the "gaining focus" flag set, it
// invokes this method on nsFrameWindow to fire an NS_ACTIVATE event.

void    nsFrameWindow::ActivateTopLevelWidget()
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
      DispatchFocus(NS_ACTIVATE);
    }
  }
  return;
}

// Subclass for frame window
MRESULT EXPENTRY fnwpFrame( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   // check to see if we have a rollup listener registered
   if (nsnull != gRollupListener && nsnull != gRollupWidget) {
      if (msg == WM_TRACKFRAME || msg == WM_MINMAXFRAME ||
          msg == WM_BUTTON1DOWN || msg == WM_BUTTON2DOWN || msg == WM_BUTTON3DOWN) {
         // Rollup if the event is outside the popup
         if (PR_FALSE == nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget)) {
            gRollupListener->Rollup(PR_UINT32_MAX, nsnull);

            // if we are supposed to be consuming events and it is
            // a Mouse Button down, let it go through
//            if (gRollupConsumeRollupEvent && msg != WM_BUTTON1DOWN) {
//               return FALSE;
//            }
         } 
      }
   }

   nsFrameWindow *pFrame = (nsFrameWindow*) WinQueryWindowPtr( hwnd, QWL_USER);
   return pFrame->FrameMessage( msg, mp1, mp2);
}

// Process messages from the frame
MRESULT nsFrameWindow::FrameMessage( ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mresult = 0;
   BOOL    bDone = FALSE;

   switch (msg)
   {
      case WM_WINDOWPOSCHANGED:
      {
         PSWP pSwp = (PSWP) mp1;

         // Note that client windows never get 'move' messages (well, they won't here anyway)
         if( pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE))
         {
            // These commented-out `-1's cancel each other out.
            POINTL ptl = { pSwp->x, pSwp->y + pSwp->cy /* - 1 */ };
            ptl.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - ptl.y /* - 1*/ ;
            mBounds.x = ptl.x;
            mBounds.y = ptl.y;
            OnMove( ptl.x, ptl.y);
         }

         // When the frame is sized, do stuff to recalculate client size.
         if( pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE))
         {
            mresult = (*fnwpDefFrame)( mFrameWnd, msg, mp1, mp2);
            bDone = TRUE;

            mBounds.width = pSwp->cx;
            mBounds.height = pSwp->cy;

            UpdateClientSize();
            DispatchResizeEvent( mSizeClient.width, mSizeClient.height);
         }
 
         if (pSwp->fl & (SWP_MAXIMIZE | SWP_MINIMIZE | SWP_RESTORE)) {
           nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);
            if (pSwp->fl & SWP_MAXIMIZE)
              event.mSizeMode = nsSizeMode_Maximized;
            else if (pSwp->fl & SWP_MINIMIZE)
              event.mSizeMode = nsSizeMode_Minimized;
            else
              event.mSizeMode = nsSizeMode_Normal;
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

      case WM_ADJUSTWINDOWPOS:
      {
        if (mChromeHidden && ((PSWP)mp1)->fl & SWP_MINIMIZE) {
          HWND hwndTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndMinMax");
          if (hwndTemp)
            WinSetParent(hwndTemp, mFrameWnd, TRUE);
          hwndTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndTitleBar");
          if (hwndTemp)
            WinSetParent(hwndTemp, mFrameWnd, TRUE);
          hwndTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndSysMenu");
          if (hwndTemp)
            WinSetParent(hwndTemp, mFrameWnd, TRUE);
        }
        break;
      }
      case WM_ADJUSTFRAMEPOS:
      {
        if (mChromeHidden && ((PSWP)mp1)->fl & SWP_RESTORE) {
          HWND hwndTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndSysMenu");
          if (hwndTemp)
            WinSetParent(hwndTemp, HWND_OBJECT, TRUE);
          hwndTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndTitleBar");
          if (hwndTemp)
            WinSetParent(hwndTemp, HWND_OBJECT, TRUE);
          hwndTemp = (HWND)WinQueryProperty(mFrameWnd, "hwndMinMax");
          if (hwndTemp)
            WinSetParent(hwndTemp, HWND_OBJECT, TRUE);
        }
        break;
      }

      case WM_DESTROY:
         DEBUGFOCUS(frame WM_DESTROY);
         WinSubclassWindow( mFrameWnd, fnwpDefFrame);
         WinSetWindowPtr( mFrameWnd, QWL_USER, 0);
         WinRemoveProperty(mFrameWnd, "hwndTitleBar");
         WinRemoveProperty(mFrameWnd, "hwndSysMenu");
         WinRemoveProperty(mFrameWnd, "hwndMinMax");
         WinRemoveProperty(mFrameWnd, "ulStyle");
         break;
      case WM_INITMENU:
         /* If we are in fullscreen/kiosk mode, disable maximize menu item */
         if (mChromeHidden) {
            if (WinQueryWindowULong(mFrameWnd, QWL_STYLE) & WS_MINIMIZED) {
              if (SHORT1FROMMP(mp1) == SC_SYSMENU) {
                MENUITEM menuitem;
                WinSendMsg(WinWindowFromID(mFrameWnd, FID_SYSMENU), MM_QUERYITEM, MPFROM2SHORT(SC_SYSMENU, FALSE), MPARAM(&menuitem));
                mresult = (*fnwpDefFrame)( mFrameWnd, msg, mp1, mp2);
                WinEnableMenuItem(menuitem.hwndSubMenu, SC_MAXIMIZE, FALSE);
                bDone = TRUE;
              }
            }
         }
         break;
      case WM_SYSCOMMAND:
         /* If we are in fullscreen/kiosk mode, don't honor maximize requests */
         if (mChromeHidden) {
            if (WinQueryWindowULong(mFrameWnd, QWL_STYLE) & WS_MINIMIZED) {
              if ((SHORT1FROMMP(mp1) == SC_MAXIMIZE))
              {
                bDone = TRUE;
              }
            }
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
            DispatchFocus(NS_DEACTIVATE);
            // Prevent the frame from automatically focusing any window
            // when it's reactivated.  Let moz set the focus to avoid
            // having non-widget children of plugins focused in error.
            WinSetWindowULong(mFrameWnd, QWL_HWNDFOCUSSAVE, 0);
         }
         break;
   }

   if( !bDone)
      mresult = (*fnwpDefFrame)( mFrameWnd, msg, mp1, mp2);

   return mresult;
}

