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
 * Contributor(s): Henry Sobotka <sobotka@axess.com> 01/2000 review and update
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/31/2000   IBM Corp.       @JSK32365 - Set WS_CLIPSIBLINGS on
 *                              nsCanvas objects to fix bleed-through
 * 04/31/2000   IBM Corp.       Change parms to DispatchMouseEvent to match Windows.
 *
 */

// nscanvas - basic window class that dispatches paint events.

#include "nscanvas.h"
#include "nsIDeviceContext.h"
#include <stdio.h>

nsCanvas::nsCanvas() : mIsTLB(FALSE)
{}


// Called in the PM thread.
void nsCanvas::RealDoCreate( HWND hwndP, nsWindow *aParent, const nsRect &aRect,
                             EVENT_CALLBACK aHandleEventFunction,
                             nsIDeviceContext *aContext, nsIAppShell *aAppShell,
                             nsWidgetInitData *aInitData, HWND hwndO)
{
   if( aInitData && aInitData->mBorderStyle == eBorderStyle_none)
   {
      // Untested hackery for (eg.) drop-downs on gfx-drawn comboboxes
      //
      // * Examine the window-type field in any init data.
      // * If this is set to 'borderless top-level' then
      //  * Parent window to desktop
      //  * Owner window to the frame of the xptoolkit parent
      //  * Record xptoolkit parent because coordinates are relative to it.
      //  * Pray that gecko will close the popup before trying to scroll content!

      mIsTLB = TRUE;

      // Find the frame.
      HWND hwndFrame = hwndP;
      while( !(FI_FRAME & (ULONG)WinSendMsg( hwndFrame, WM_QUERYFRAMEINFO, 0, 0)))
         hwndFrame = WinQueryWindow( hwndFrame, QW_PARENT);
   
      // Create the window.  Parent to desktop, owner to the frame (moving works),
      // maintain the xptoolkit parent, which handles coordinate resolution.
      nsWindow::RealDoCreate( HWND_DESKTOP, aParent, aRect,
                              aHandleEventFunction, aContext,
                              aAppShell, aInitData, hwndFrame);
   }
   else
   {
      // just pass thru'
      nsWindow::RealDoCreate( hwndP, aParent, aRect, aHandleEventFunction,
                              aContext, aAppShell, aInitData, hwndO);
   }
#if DEBUG_sobotka
   printf("\nIn nsCanvas::RealDoCreate aParent = 0x%lx\n", &aParent);
   printf("   hwndP = %lu\n", hwndP);
   printf("   hwnd0 = %lu\n", hwndO);
   printf("   aContext = 0x%lx\n", &aContext);
   printf("   aAppShell = 0x%lx\n", &aAppShell);
#endif
}

// Need to override this because the coords passed in are in OS/2 space but
// relative to the xptoolkit parent.

BOOL nsCanvas::SetWindowPos( HWND ib, long x, long y,
                             long cx, long cy, ULONG flags)
{
   POINTL ptl = { x, y };
   if( mIsTLB)
      WinMapWindowPoints( (HWND) mParent->GetNativeData(NS_NATIVE_WIDGET),
                           HWND_DESKTOP, &ptl, 1);
   return nsWindow::SetWindowPos( ib, ptl.x, ptl.y, cx, cy, flags);
}

PRBool nsCanvas::OnReposition( PSWP pSwp)
{
   PRBool rc = PR_FALSE;

   if( mIsTLB)
   {
      if( pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE))
         rc = OnMove( pSwp->x, gModuleData.szScreen.cy - pSwp->y - pSwp->cy);
      if( pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE))
         rc = OnResize( pSwp->cx, pSwp->cy);
   }
   else
      rc = nsWindow::OnReposition( pSwp);

   return rc;
}

PRBool nsCanvas::OnPaint()
{
   PRBool rc = PR_FALSE;

   if( mContext && (mEventCallback || mEventListener))
   {
      // Get rect to redraw and validate window
#if 0
      RECTL rcl;
      WinQueryUpdateRect( mWnd, &rcl);
      WinValidateRect( mWnd, &rcl, TRUE);

      if( rcl.xLeft || rcl.xRight || rcl.yBottom || rcl.yTop)
      {
         // build XP rect from in-ex window rect
         nsRect rect;
         rect.x = rcl.xLeft;
         rect.y = GetClientHeight() - rcl.yTop;
         rect.width = rcl.xRight - rcl.xLeft;
         rect.height = rcl.yTop - rcl.yBottom;
   
         // build & dispatch paint event
         nsPaintEvent event;
         InitEvent( event, NS_PAINT);
         event.rect = &rect;
         event.eventStructType = NS_PAINT_EVENT;
         event.renderingContext = GetRenderingContext();
         rc = DispatchWindowEvent( &event);
   
         NS_RELEASE( event.renderingContext);
      }
#else
      RECTL rcl = { 0 };
      HPS   thePS = (HPS) GetNativeData( NS_NATIVE_GRAPHIC);
      thePS = WinBeginPaint( mWnd, thePS, &rcl);

      if( rcl.xLeft || rcl.xRight || rcl.yBottom || rcl.yTop)
      {
         // build XP rect from in-ex window rect
         nsRect rect;
         rect.x = rcl.xLeft;
         rect.y = GetClientHeight() - rcl.yTop;
         rect.width = rcl.xRight - rcl.xLeft;
         rect.height = rcl.yTop - rcl.yBottom;
   
         // build & dispatch paint event
         nsPaintEvent event;
         InitEvent( event, NS_PAINT);
         event.rect = &rect;
         event.eventStructType = NS_PAINT_EVENT;
         event.renderingContext = GetRenderingContext();
         rc = DispatchWindowEvent( &event);
   
         NS_RELEASE( event.renderingContext);
      }

      WinEndPaint( thePS);
      FreeNativeData( (void*)thePS, NS_NATIVE_GRAPHIC);
#endif
   }

   return rc;
}

// Realize-palette.  I reckon only top-level windows get the message, so
// there's code in frame to broadcast it to children.
PRBool nsCanvas::OnRealizePalette()
{
   PRBool rc = PR_FALSE;

   // Get palette info from device 
   nsPaletteInfo palInfo;
   mContext->GetPaletteInfo( palInfo);

   if( mPS && palInfo.isPaletteDevice && palInfo.palette)
   {
      // An onscreen nsDrawingSurface has been created for the window,
      // and we have a palette.  So realize it.
      ULONG cclr;
      long palrc = WinRealizePalette( mWnd, mPS, &cclr);
      if( palrc && palrc != PAL_ERROR)
         // Colours have changed, redraw.
         WinInvalidateRect( mWnd, 0, FALSE);

      rc = PR_TRUE;
   }

   return rc;
}

PRBool nsCanvas::OnKey( MPARAM mp1, MPARAM mp2)
{
   nsWindow::OnKey( mp1, mp2);
   return PR_TRUE; // Gecko doesn't expect unhandled events to propagate
}

extern BOOL g_bHandlingMouseClick;

PRBool nsCanvas::DispatchMouseEvent( PRUint32 aEventType, MPARAM mp1, MPARAM mp2)
{
   PRBool rc = PR_FALSE;

   // Stop multiple messages for the same PM action
   if( g_bHandlingMouseClick)
      return rc;

   // Don't capture mb2 so that drag'n'drop works.
 
   if( mEventCallback || mMouseListener)
   {
      switch( aEventType)
      {
         case NS_MOUSE_LEFT_BUTTON_DOWN:
         case NS_MOUSE_MIDDLE_BUTTON_DOWN:
            WinSetCapture( HWND_DESKTOP, mWnd);
            break;
    
         case NS_MOUSE_LEFT_BUTTON_UP:
         case NS_MOUSE_MIDDLE_BUTTON_UP:
            WinSetCapture( HWND_DESKTOP, 0); // release
            break;
    
         default:
            break;
      }
      rc = nsWindow::DispatchMouseEvent( aEventType, mp1, mp2);
   }

// Mousemove messages mustn't propagate to get cursors working.  Try commenting
// this block out & then move the mouse over a link.

   if( aEventType == NS_MOUSE_MOVE)
    rc = PR_TRUE;

   return rc;
}

// Creation hooks
static BOOL bRegistered;
PCSZ nsCanvas::WindowClass()
{
   if( !bRegistered)
   {
      BOOL rc = WinRegisterClass( 0 /*hab*/, NSCANVASCLASS,
                                  WinDefWindowProc, 0, 4);
      NS_ASSERTION(rc, "Couldn't register canvas class");
      bRegistered = TRUE;
   }

   return (PCSZ) NSCANVASCLASS;
}

ULONG nsCanvas::WindowStyle()
{
   return BASE_CONTROL_STYLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
}
