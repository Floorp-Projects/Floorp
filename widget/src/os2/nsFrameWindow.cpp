/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
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

static PRBool haveHiddenWindow = PR_FALSE;

extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;
extern PRBool              gRollupConsumeRollupEvent;

extern PRBool gJustGotDeactivate;
extern HWND   gHwndBeingDestroyed;


nsFrameWindow::nsFrameWindow() : nsWindow()
{
   hwndFrame    = 0;
   fnwpDefFrame = 0;
   mWindowType  = eWindowType_toplevel;
}

nsFrameWindow::~nsFrameWindow()
{
}

void nsFrameWindow::SetWindowListVisibility( PRBool bState)
{
   HSWITCH hswitch;
   SWCNTRL swctl;

   hswitch = WinQuerySwitchHandle(hwndFrame, 0);
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
                                  const nsRect &aRect,
                                  EVENT_CALLBACK aHandleEventFunction,
                                  nsIDeviceContext *aContext,
                                  nsIAppShell *aAppShell,
                                  nsWidgetInitData *aInitData, HWND hwndO)
{
   nsRect rect;
   if( aParent)  // Offset rect by position of owner
   {
      nsRect clientRect;
      aParent->GetBounds(rect);
      aParent->GetClientBounds(clientRect);
      rect.x += aRect.x + clientRect.x;
      rect.y += aRect.y + clientRect.y;
      rect.width = aRect.width;
      rect.height = aRect.height;
      hwndP = aParent->GetMainWindow();
   }
   else          // Use original rect, no owner window
   {
      rect = aRect;
      hwndP = NULLHANDLE;
   }

#if DEBUG_sobotka
   printf("\nIn nsFrameWindow::RealDoCreate:\n");
   printf("   hwndP = %lu\n", hwndP);
   printf("   aParent = 0x%lx\n", &aParent);
   printf("   aRect = %ld, %ld, %ld, %ld\n", aRect.x, aRect.y, aRect.height, aRect.width);
#endif

   // Create the frame window.
   FRAMECDATA fcd = { sizeof( FRAMECDATA), 0, 0, 0 };

   // Set flags only if not first hidden window created by nsAppShellService

   // This line is being commented out to address the problem of the profile manager's windows
   // being created w/out title-bars
   // OS2TODO -- insure this does not affect any other portion of the application 
   // if (haveHiddenWindow)
     fcd.flCreateFlags = GetFCFlags();

   // Assume frames are toplevel.  Breaks if anyone tries to do MDI, which
   // is an extra bonus feature :-)
   hwndFrame = WinCreateWindow( HWND_DESKTOP,
                                WC_FRAME,
                                0, 0,                  // text, style
                                0, 0, 0, 0,            // position
                                hwndP,
                                HWND_TOP,
                                0,                     // ID
                                &fcd, 0);

   if (hwndFrame && !haveHiddenWindow)
     haveHiddenWindow = PR_TRUE;

   SetWindowListVisibility( PR_FALSE);  // Hide from Window List until shown

   // This is a bit weird; without an icon, we get WM_PAINT messages
   // when minimized.  They don't stop, giving maxed cpu.  Go figure.

   NS_ASSERTION( hwndFrame, "Couldn't create frame");

   // Frames have a minimum height based on the pieces they are created with,
   // such as titlebar, menubar, frame borders, etc.  We need this minimum
   // height so we can correctly set the frame position (coordinate flipping).
   nsRect frameRect = rect;
   long minheight; 

   if ( fcd.flCreateFlags & FCF_SIZEBORDER) {
      minheight = 2 * WinQuerySysValue( HWND_DESKTOP, SV_CYSIZEBORDER);
   }
   else if ( fcd.flCreateFlags & FCF_DLGBORDER) {
      minheight = 2 * WinQuerySysValue( HWND_DESKTOP, SV_CYDLGFRAME);
   }
   else {
      minheight = 2 * WinQuerySysValue( HWND_DESKTOP, SV_CYBORDER);
   }
   if ( fcd.flCreateFlags & FCF_TITLEBAR) {
      minheight += WinQuerySysValue( HWND_DESKTOP, SV_CYTITLEBAR);
   }
   if ( fcd.flCreateFlags & FCF_MENU) {
      minheight += WinQuerySysValue( HWND_DESKTOP, SV_CYMENU);
   }
   if ( frameRect.height < minheight) {
      frameRect.height = minheight;
   }

   // Now create the client as a child of us, triggers resize and sets
   // up the client size (with any luck...)
   nsWindow::RealDoCreate( hwndFrame, nsnull, frameRect, aHandleEventFunction,
                           aContext, aAppShell, aInitData, hwndO);

   // Subclass frame
   fnwpDefFrame = WinSubclassWindow( hwndFrame, fnwpFrame);
   WinSetWindowPtr( hwndFrame, QWL_USER, this);
   BOOL brc = (BOOL) WinSendMsg( hwndFrame, WM_SETICON,
                                 MPFROMLONG( gWidgetModuleData->GetFrameIcon()), 0);

   // make the client the client.
   WinSetWindowUShort( mWnd, QWS_ID, FID_CLIENT);
   WinSendMsg( hwndFrame, WM_UPDATEFRAME, 0, 0); // possibly superfluous

   // Set up initial client size
   UpdateClientSize();

   // Record frame hwnd somewhere that the window object can see during dtor
   mHackDestroyWnd = hwndFrame;
}

ULONG nsFrameWindow::GetFCFlags()
{
  ULONG style = FCF_TITLEBAR | FCF_SYSMENU | FCF_TASKLIST |
                FCF_CLOSEBUTTON | FCF_NOBYTEALIGN |
                (gWidgetModuleData->bIsDBCS ? FCF_DBE_APPSTAT : 0);

  if (mWindowType == eWindowType_dialog) {
    style |= FCF_DIALOGBOX;
    if (mBorderStyle == eBorderStyle_default) {
      style |= FCF_DLGBORDER;
    } else {
      style |= FCF_SIZEBORDER | FCF_MINMAX;
    }
  }
  else {
    style |= FCF_SIZEBORDER | FCF_MINMAX;
  }


  if (mBorderStyle != eBorderStyle_default && mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_resizeh)) {
      style &= ~FCF_SIZEBORDER;
      style |= FCF_DLGBORDER;
    }
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_border))
      style &= ~(FCF_DLGBORDER | FCF_SIZEBORDER);
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_title)) {
      style &= ~(FCF_TITLEBAR | FCF_TASKLIST);
    }

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_close))
      style &= ~FCF_CLOSEBUTTON;

    if (mBorderStyle == eBorderStyle_none ||
      !(mBorderStyle & (eBorderStyle_menu | eBorderStyle_close)))
      style &= ~FCF_SYSMENU;
    // Looks like getting rid of the system menu also does away with the
    // close box. So, we only get rid of the system menu if you want neither it
    // nor the close box. How does the Windows "Dialog" window class get just
    // closebox and no sysmenu? Who knows.
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_minimize))
      style &= ~FCF_MINBUTTON;
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_maximize))
      style &= ~FCF_MAXBUTTON;
  }

  return style;
}

void nsFrameWindow::UpdateClientSize()
{
   RECTL rcl = { 0, 0, mBounds.width, mBounds.height };
   WinCalcFrameRect( hwndFrame, &rcl, TRUE); // provided == frame rect
   mSizeClient.width = rcl.xRight - rcl.xLeft;
   mSizeClient.height = rcl.yTop - rcl.yBottom;
   mSizeBorder.width = (mBounds.width - mSizeClient.width) / 2;
   mSizeBorder.height = (mBounds.height - mSizeClient.height) / 2;
}

nsresult nsFrameWindow::GetClientBounds( nsRect &aRect)
{
   RECTL rcl = { 0, 0, mBounds.width, mBounds.height };
   WinCalcFrameRect( hwndFrame, &rcl, TRUE); // provided == frame rect
   aRect.x = rcl.xLeft;
   aRect.y = mBounds.height - rcl.yTop;
   aRect.width = mSizeClient.width;
   aRect.height = mSizeClient.height;
   return NS_OK;
}

nsresult nsFrameWindow::GetBorderSize( PRInt32 &aWidth, PRInt32 &aHeight)
{
   aWidth = mSizeBorder.width;
   aHeight = mSizeBorder.height;
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
         ulFlags = SWP_SHOW | SWP_ACTIVATE;
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

// Subclass for frame window
MRESULT EXPENTRY fnwpFrame( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   // check to see if we have a rollup listener registered
   if (nsnull != gRollupListener && nsnull != gRollupWidget) {
      if (msg == WM_ACTIVATE || msg == WM_TRACKFRAME || msg == WM_MINMAXFRAME ||
          msg == WM_BUTTON1DOWN || msg == WM_BUTTON2DOWN || msg == WM_BUTTON3DOWN) {
         // Rollup if the event is outside the popup
         if (PR_FALSE == nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget)) {
            gRollupListener->Rollup();

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
   MRESULT mRC = 0;
   BOOL    bDone = FALSE;

   switch( msg)
   {
      case WM_WINDOWPOSCHANGED:
      {
         PSWP pSwp = (PSWP) mp1;

         // Note that client windows never get 'move' messages (well, they won't here anyway)
         if( pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE))
         {
            // These commented-out `-1's cancel each other out.
            POINTL ptl = { pSwp->x, pSwp->y + pSwp->cy /* - 1 */ };
            ptl.y = gWidgetModuleData->szScreen.cy - ptl.y /* - 1*/ ;
            mBounds.x = ptl.x;
            mBounds.y = ptl.y;
            OnMove( ptl.x, ptl.y);
         }

         // When the frame is sized, do stuff to recalculate client size.
         if( pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE))
         {
            mRC = (*fnwpDefFrame)( hwndFrame, msg, mp1, mp2);
            bDone = TRUE;

            mBounds.width = pSwp->cx;
            mBounds.height = pSwp->cy;

            UpdateClientSize();
            DispatchResizeEvent( mSizeClient.width, mSizeClient.height);
         }
 
         if ( pSwp->fl & (SWP_MAXIMIZE | SWP_MINIMIZE | SWP_RESTORE)) {
            nsSizeModeEvent event;
            event.eventStructType = NS_SIZEMODE_EVENT;
            if ( pSwp->fl & SWP_MAXIMIZE)
              event.mSizeMode = nsSizeMode_Maximized;
            else if ( pSwp->fl & SWP_MINIMIZE)
              event.mSizeMode = nsSizeMode_Minimized;
            else
              event.mSizeMode = nsSizeMode_Normal;
            InitEvent(event, NS_SIZEMODE);
            DispatchWindowEvent(&event);
            NS_RELEASE(event.widget);
         }

         break;
      }

      case WM_DESTROY:
         WinSubclassWindow( hwndFrame, fnwpDefFrame);
         WinSetWindowPtr( hwndFrame, QWL_USER, 0);
         break;

      // adjust client size when menu appears or disappears.
      case WM_UPDATEFRAME:
         if( LONGFROMMP(mp1) & FCF_MENU)
            UpdateClientSize();
         break;

      case WM_ACTIVATE:
         if( !mp1)
         {
            /* we don't want to go through all of this deactivation stuff if
             * the window is just going away anyhow.  Causes problems to process
             * the deactivation event if we've hit some destructors or done
             * some destroy work.  Especially w.r.t the focus controller
             */
            if( (hwndFrame != gHwndBeingDestroyed) || 
                (!WinIsChild( hwndFrame, gHwndBeingDestroyed )) )
            {
               gJustGotDeactivate = PR_TRUE;
            }
         }
         break;
   }

   if( !bDone)
      mRC = (*fnwpDefFrame)( hwndFrame, msg, mp1, mp2);

   return mRC;
}

ULONG nsFrameWindow::WindowStyle()
{
   return BASE_CONTROL_STYLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
}
