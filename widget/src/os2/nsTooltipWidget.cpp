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
 */

#include "nsTooltipWidget.h"

// Tooltip class; just a custom canvas-like class to put things on.

NS_IMPL_ADDREF(nsTooltipWidget)
NS_IMPL_RELEASE(nsTooltipWidget)

nsresult nsTooltipWidget::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

   if( result == NS_NOINTERFACE && aIID.Equals( nsITooltipWidget::GetIID()))
   {
      *aInstancePtr = (void*) ((nsITooltipWidget*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
   }

   return result;
}

static BOOL bRegistered;
#define NSTOOLTIPCLASS (PCSZ)"WarpzillaTooltip"

PCSZ nsTooltipWidget::WindowClass()
{
   if( !bRegistered)
   {
      BOOL rc = WinRegisterClass( 0 /*hab*/, NSTOOLTIPCLASS,
                                  WinDefWindowProc, 0, 4);
      NS_ASSERTION( rc, "Couldn't register tooltip class");
      bRegistered = TRUE;
   }

   return NSTOOLTIPCLASS;
}

ULONG nsTooltipWidget::WindowStyle()
{
   return 0;
}

// background for tooltips
#define MK_RGB(r,g,b) ((r) * 65536) + ((g) * 256) + (b)


PRBool nsTooltipWidget::OnPaint()
{
   // Draw tooltip - black border, off-white interior
   RECTL rcl;
   HPS hps = WinBeginPaint( mWnd, 0, &rcl);
   WinQueryWindowRect( mWnd, &rcl);
   GpiCreateLogColorTable( hps, LCOL_PURECOLOR, LCOLF_RGB, 0, 0, 0);
   long lColor = 0; // black
   GpiSetAttrs( hps, PRIM_LINE, LBB_COLOR, 0, &lColor);
   lColor = MK_RGB(255,255,185); // off-white
   GpiSetAttrs( hps, PRIM_AREA, ABB_COLOR, 0, &lColor);
   rcl.yTop--;
   rcl.xRight--;
   PPOINTL pts = (PPOINTL) &rcl;
   GpiMove( hps, pts);
   GpiBox( hps, DRO_OUTLINEFILL, pts + 1, 0, 0);
   WinEndPaint( hps);
   return PR_TRUE;
}
