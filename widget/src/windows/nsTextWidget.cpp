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

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include <windows.h>


//-------------------------------------------------------------------------
//
// nsTextWidget constructor
//
//-------------------------------------------------------------------------
nsTextWidget::nsTextWidget(nsISupports *aOuter) : nsTextHelper(aOuter)
{
  mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextWidget destructor
//
//-------------------------------------------------------------------------
nsTextWidget::~nsTextWidget()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextWidget::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTextWidgetIID, NS_ITEXTWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTextWidgetIID)) {
        *aInstancePtr = (void*) ((nsITextWidget*)this);
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
PRBool nsTextWidget::OnPaint()
{
    return PR_FALSE;
}


PRBool nsTextWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsTextWidget::WindowClass()
{
  return(nsTextHelper::WindowClass());
}

//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsTextWidget::WindowStyle()
{
  return(nsTextHelper::WindowStyle());
}


//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------

DWORD nsTextWidget::WindowExStyle()
{
    return WS_EX_CLIENTEDGE;
}


