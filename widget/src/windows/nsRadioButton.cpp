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

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#include "nsIDeviceContext.h"

NS_IMPL_ADDREF(nsRadioButton)
NS_IMPL_RELEASE(nsRadioButton)

//-------------------------------------------------------------------------
//
// nsRadioButton constructor
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton() : nsWindow(), nsIRadioButton()
{
  NS_INIT_REFCNT();
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
nsresult nsRadioButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kIRadioButtonIID)) {
      *aInstancePtr = (void*) ((nsIRadioButton*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
  }
  return result;
  }

//-------------------------------------------------------------------------
//
// Sets the state of the nsRadioButton 
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetState(const PRBool aState) 
{
  fState = aState;
  ::SendMessage(GetWindowHandle(), BM_SETCHECK, (WPARAM)(fState), 0L);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetState(PRBool& aState)
{
  aState = fState;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
{
  char label[256];
  aText.ToCString(label, 256);
  label[255] = '\0';
  VERIFY(::SetWindowText(mWnd, label));
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
  int actualSize = ::GetWindowTextLength(mWnd)+1;
  NS_ALLOC_CHAR_BUF(label, 256, actualSize);
  ::GetWindowText(mWnd, label, actualSize);
  aBuffer.SetLength(0);
  aBuffer.Append(label);
  NS_FREE_CHAR_BUF(label);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsRadioButton::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

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

/**
 * Renders the RadioButton for Printing
 *
 **/
NS_METHOD nsRadioButton::Paint(nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect)
{
  float  appUnits;
  float  scale;
  nsIDeviceContext * context;
  aRenderingContext.GetDeviceContext(context);

  context->GetCanonicalPixelScale(scale);
  context->GetDevUnitsToAppUnits(appUnits);
  nsRect rect;
  GetBounds(rect);

  rect.x++;
  rect.y++;
  rect.width  -= 2;
  rect.height -= 2;
  aRenderingContext.SetColor(NS_RGB(0,0,0));

  nscoord one = nscoord(PRFloat64(rect.width) * 1.0/12.0);

  rect.x      = nscoord((PRFloat64)rect.x * appUnits);
  rect.y      = nscoord((PRFloat64)rect.y * appUnits);
  rect.width  = nscoord((PRFloat64)rect.width * appUnits); 
  rect.height = nscoord((PRFloat64)rect.height * appUnits); 
  rect.x      += one;
  rect.width  = nscoord(PRFloat64(rect.width)  * 11.0/12.0);
  rect.height = nscoord(PRFloat64(rect.height) * 11.0/12.0);

  for (nscoord i=0;i<nscoord(scale*1.25);i++) {
    aRenderingContext.DrawArc(rect, 0, 180);
    aRenderingContext.DrawArc(rect, 180, 360);
    rect.x++;
    rect.y++;
    rect.width  -= 2;
    rect.height -= 2;
  }

  if (fState) {
    GetBounds(rect);
    nscoord xHalf = rect.width / 4;
    nscoord yHalf = rect.height / 4;
    rect.x      += xHalf;
    rect.y      += yHalf;
    rect.width  -= xHalf*2;
    rect.height -= yHalf*2;
    aRenderingContext.SetColor(NS_RGB(0,0,0));

    nscoord one    = nscoord(PRFloat64(rect.width) * 1.0/12.0);

    rect.x      = nscoord((PRFloat64)rect.x * appUnits);
    rect.y      = nscoord((PRFloat64)rect.y * appUnits);
    rect.width  = nscoord((PRFloat64)rect.width * appUnits); 
    rect.height = nscoord((PRFloat64)rect.height * appUnits); 
    rect.x      += one;
    rect.width  = nscoord(PRFloat64(rect.width)  * 11.0/12.0);
    rect.height = nscoord(PRFloat64(rect.height) * 11.0/12.0);

    aRenderingContext.FillArc(rect, 0, 180);
    aRenderingContext.FillArc(rect, 180, 360);

  }

  NS_RELEASE(context);
  return NS_OK;
}



