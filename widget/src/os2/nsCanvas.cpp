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

#include "nsCanvas.h"
#include <stdio.h>

extern BOOL g_bHandlingMouseClick;

PRBool nsCanvas::DispatchMouseEvent( PRUint32 aEventType, MPARAM mp1, MPARAM mp2)
{
   PRBool rc = PR_FALSE;

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


ULONG nsCanvas::WindowStyle()
{
   return BASE_CONTROL_STYLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
}
