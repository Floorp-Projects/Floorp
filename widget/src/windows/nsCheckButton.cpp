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

#include "nsCheckButton.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>

//-------------------------------------------------------------------------
//
// nsCheckButton constructor
//
//-------------------------------------------------------------------------
nsCheckButton::nsCheckButton(nsISupports *aOuter) : nsWindow(aOuter)
{
}


//-------------------------------------------------------------------------
//
// nsCheckButton destructor
//
//-------------------------------------------------------------------------
nsCheckButton::~nsCheckButton()
{
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsCheckButton::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsCheckButtonIID, NS_ICHECKBUTTON_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsCheckButtonIID)) {
        *aInstancePtr = (void*) ((nsICheckButton*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsCheckButton::SetState(PRBool aState) 
{
    BOOL state;
    if (aState) 
        state = BST_CHECKED;
    else
        state = BST_UNCHECKED;
    ::SendMessage(mWnd, BM_SETCHECK, (WPARAM)state, (LPARAM)0);
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
PRBool nsCheckButton::GetState()
{
    if (::SendMessage(mWnd, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED)
        return PR_TRUE;
    else
        return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsCheckButton::SetLabel(const nsString& aText)
{
    char label[256];
    aText.ToCString(label, 256);
    label[255] = '\0';
    VERIFY(::SetWindowText(mWnd, label));
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
void nsCheckButton::GetLabel(nsString& aBuffer)
{
  int actualSize = ::GetWindowTextLength(mWnd)+1;
  NS_ALLOC_CHAR_BUF(label, 256, actualSize);
  ::GetWindowText(mWnd, label, actualSize);
  aBuffer.SetLength(0);
  aBuffer.Append(label);
  NS_FREE_CHAR_BUF(label);
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsCheckButton::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsCheckButton::OnPaint()
{
    return PR_FALSE;
}

PRBool nsCheckButton::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsCheckButton::WindowClass()
{
    return "BUTTON";
}


//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsCheckButton::WindowStyle()
{
    return BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS; 
}


//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------
DWORD nsCheckButton::WindowExStyle()
{
    return 0;
}


