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

NS_IMPL_ADDREF(nsTextAreaWidget)
//NS_IMPL_RELEASE(nsTextAreaWidget)
nsrefcnt nsTextAreaWidget::Release(void)                         
{             
//  NS_PRECONDITION(0 != cnt, "dup release");        
  if (--mRefCnt == 0) {                                
    delete this;                                       
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}


//-------------------------------------------------------------------------
void nsTextAreaWidget::SetUpForPaint(HDC aHDC) 
{
  NS_INIT_REFCNT();
  ::SetBkColor (aHDC, NSRGB_2_COLOREF(mBackground));
  ::SetTextColor(aHDC, NSRGB_2_COLOREF(mForeground));
  //::SetBkMode (aHDC, TRANSPARENT); // don't do this
}


//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget()
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
nsresult nsTextAreaWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);


  if (aIID.Equals(kITextAreaWidgetIID)) {
      nsITextAreaWidget* textArea = this;
      *aInstancePtr = (void*) (textArea);
      NS_ADDREF_THIS();
      return NS_OK;
  } 
  else if (aIID.Equals(kIWidgetIID))
  {
      nsIWidget* widget = this;
      *aInstancePtr = (void*) (widget);
      NS_ADDREF_THIS();
      return NS_OK;
  }

  return nsWindow::QueryInterface(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextAreaWidget::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

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


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsTextAreaWidget::GetBounds(nsRect &aRect)
{
    nsWindow::GetNonClientBounds(aRect);
    return NS_OK;
}



