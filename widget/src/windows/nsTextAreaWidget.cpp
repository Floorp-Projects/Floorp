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
#include "nsTextAreaWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include <windows.h>

void nsTextAreaWidget::SelectAll() 
{
  nsTextHelper::SelectAll();
}

void nsTextAreaWidget::SetMaxTextLength(PRUint32 aChars) 
{
  nsTextHelper::SetMaxTextLength(aChars);
}

PRUint32  nsTextAreaWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) 
{
  return(nsTextHelper::GetText(aTextBuffer, aBufferSize));
}

PRUint32  nsTextAreaWidget::SetText(const nsString &aText) 
{ 
  return(nsTextHelper::SetText(aText));
}

PRUint32  nsTextAreaWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos) 
{ 
  return(nsTextHelper::InsertText(aText, aStartPos, aEndPos));
}

void  nsTextAreaWidget::RemoveText() 
{
  nsTextHelper::RemoveText();
}

void  nsTextAreaWidget::SetPassword(PRBool aIsPassword)
{
  nsTextHelper::SetPassword(aIsPassword);
}

PRBool  nsTextAreaWidget::SetReadOnly(PRBool aReadOnlyFlag) 
{
  return(nsTextHelper::SetReadOnly(aReadOnlyFlag));
}

void  nsTextAreaWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
}

void  nsTextAreaWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
}

void  nsTextAreaWidget::SetCaretPosition(PRUint32 aPosition)
{
}

PRUint32  nsTextAreaWidget::GetCaretPosition()
{
  return(0);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget(nsISupports *aOuter) : nsTextHelper(aOuter)
{
  nsTextHelper::mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget destructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::~nsTextAreaWidget()
{
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextAreaWidget::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTextAreaWidgetIID)) {
        *aInstancePtr = (void*) ((nsITextAreaWidget*)this);
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
PRBool nsTextAreaWidget::OnPaint()
{
    return PR_FALSE;
}

PRBool nsTextAreaWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsTextAreaWidget::WindowClass()
{
  return(nsTextHelper::WindowClass());
}


//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsTextAreaWidget::WindowStyle()
{
  DWORD style = nsTextHelper::WindowStyle();
    //style = style | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL;
    style = style | ES_MULTILINE | ES_WANTRETURN | WS_HSCROLL | WS_VSCROLL;
    return style; 
}


//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------

DWORD nsTextAreaWidget::WindowExStyle()
{
    return WS_EX_CLIENTEDGE;
}



