/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsTooltipWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <commctrl.h>
#include <windows.h>

BOOL nsTooltipWidget::sTooltipWidgetIsRegistered = FALSE;

//-------------------------------------------------------------------------
//
// nsTooltipWidget constructor
//
//-------------------------------------------------------------------------
nsTooltipWidget::nsTooltipWidget(nsISupports *aOuter) : nsWindow(aOuter)
{
}

//-------------------------------------------------------------------------
//
// nsTooltipWidget destructor
//
//-------------------------------------------------------------------------
nsTooltipWidget::~nsTooltipWidget()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTooltipWidget::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

  static NS_DEFINE_IID(kInsTooltipWidgetIID, NS_ITOOLTIPWIDGET_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsTooltipWidgetIID)) {
      *aInstancePtr = (void*) ((nsITooltipWidget*)this);
      AddRef();
      result = NS_OK;
  }

  return result;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsTooltipWidget::OnPaint()
{
  return PR_FALSE;
}

PRBool nsTooltipWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------

LPCTSTR nsTooltipWidget::WindowClass()
{
  const LPCTSTR className = "NetscapeTooltipWidgetClass";

  if (!nsTooltipWidget::sTooltipWidgetIsRegistered) {
      WNDCLASS wc;

      wc.style            = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
      wc.lpfnWndProc      = ::DefWindowProc;
      wc.cbClsExtra       = 0;
      wc.cbWndExtra       = 0;
      wc.hInstance        = nsToolkit::mDllInstance;
      wc.hIcon            = 0;
      wc.hCursor          = NULL;
      wc.hbrBackground    = ::CreateSolidBrush(NSRGB_2_COLOREF(NS_RGB(255,255,225))); 
      wc.lpszMenuName     = NULL;
      wc.lpszClassName    = className;
  
      nsTooltipWidget::sTooltipWidgetIsRegistered = ::RegisterClass(&wc);
  }

  return className;
}

//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsTooltipWidget::WindowStyle()
{ 
  return WS_POPUP; 
}

//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------
DWORD nsTooltipWidget::WindowExStyle()
{
  return 0; //WS_EX_TOPMOST;
}

//-------------------------------------------------------------------------
//
// Clear window before paint
//
//-------------------------------------------------------------------------

PRBool nsTooltipWidget::AutoErase()
{
  return(PR_TRUE);
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

void nsTooltipWidget::GetBounds(nsRect &aRect)
{
    nsWindow::GetBounds(aRect);
}



