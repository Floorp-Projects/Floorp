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

#include "nsRadioButton.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsStringUtil.h"

#include <windows.h>

//-------------------------------------------------------------------------
//
// nsRadioButton constructor
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton(nsISupports *aOuter) : nsWindow(aOuter)
{
  fRadioGroup = NULL;
}


//-------------------------------------------------------------------------
//
// nsRadioButton destructor
//
//-------------------------------------------------------------------------
nsRadioButton::~nsRadioButton()
{
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsRadioButton::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsRadioButtonIID, NS_IRADIOBUTTON_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsRadioButtonIID)) {
        *aInstancePtr = (void*) ((nsIRadioButton*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}

//-------------------------------------------------------------------------
//
// Sets the state of the nsRadioButton but it DOES notify its nsRadioGroup.
//
//-------------------------------------------------------------------------
void nsRadioButton::SetState(PRBool aState) 
{
    fState = aState;

    ::SendMessage(GetWindowHandle(), BM_SETCHECK, (WPARAM)(fState), 0L);
	if (nsnull != fRadioGroup)
      fRadioGroup->Clicked(this);
}

//-------------------------------------------------------------------------
//
// Sets the state of the nsRadioButton but it does not notify
// its nsRadioGroup.
//
//-------------------------------------------------------------------------
void nsRadioButton::SetStateNoNotify(PRBool aState) 
{
	fState = aState;
	::SendMessage(GetWindowHandle(), BM_SETCHECK, (WPARAM)(fState), 0L);
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
PRBool nsRadioButton::GetState()
{
  return fState;
}

//-------------------------------------------------------------------------
//
// Return the radio group this radio button belongs to
//
//-------------------------------------------------------------------------
nsIRadioGroup* nsRadioButton::GetRadioGroup()                      
{ 
    if (fRadioGroup) 
        fRadioGroup->AddRef();
    return fRadioGroup;   
}


//-------------------------------------------------------------------------
//
// Set the radio button group
//
//-------------------------------------------------------------------------
void nsRadioButton::SetRadioGroup(nsIRadioGroup* aGroup) 
{ 
    if (aGroup) 
        aGroup->AddRef();

    if (fRadioGroup)
        NS_RELEASE(fRadioGroup);

    fRadioGroup = aGroup; 
    fRadioGroup->Add(this);
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsRadioButton::SetLabel(const nsString& aText)
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
void nsRadioButton::GetLabel(nsString& aBuffer)
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
// Process all windows messages
//
//-------------------------------------------------------------------------
PRBool nsRadioButton::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue)
{            
    *aRetValue = 0;

    if (msg == WM_LBUTTONUP) {
      SetState(PR_TRUE);
      return PR_FALSE;
    } else {
      return nsWindow::ProcessMessage(msg, wParam, lParam, aRetValue);
    }
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsRadioButton::OnPaint()
{
    return PR_FALSE;
}

PRBool nsRadioButton::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsRadioButton::WindowClass()
{
    return "BUTTON";
}


//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsRadioButton::WindowStyle()
{
    return BS_RADIOBUTTON | WS_CHILD | WS_CLIPSIBLINGS; 
}


//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------
DWORD nsRadioButton::WindowExStyle()
{
    return 0;
}


