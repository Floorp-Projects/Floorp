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

#include "nsComboBox.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsRepository.h"

#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
NS_IMPL_ADDREF(nsComboBox)
NS_IMPL_RELEASE(nsComboBox)

//-------------------------------------------------------------------------
//
// nsComboBox constructor
//
//-------------------------------------------------------------------------
nsComboBox::nsComboBox() : nsWindow(), nsIListWidget(), nsIComboBox()
{
  NS_INIT_REFCNT();
  mBackground = NS_RGB(124, 124, 124);
  mDropDownHeight = 60; // Default to 60 pixels for drop-down list height
}

//-------------------------------------------------------------------------
//
//  destructor
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);
  ::SendMessage(mWnd, CB_ADDSTRING, (int)aPosition, (LPARAM)(LPCTSTR)val); 
  NS_FREE_STR_BUF(val);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);
  int index = ::SendMessage(mWnd, CB_FINDSTRINGEXACT, (int)aStartPos, (LPARAM)(LPCTSTR)val); 
  NS_FREE_STR_BUF(val);

  return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::GetItemCount()
{
  return (PRInt32)::SendMessage(mWnd, CB_GETCOUNT, (int)0, (LPARAM)0); 
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsComboBox::RemoveItemAt(PRInt32 aPosition)
{
  int status = ::SendMessage(mWnd, CB_DELETESTRING, (int)aPosition, (LPARAM)(LPCTSTR)0); 
  return (status != CB_ERR?PR_TRUE:PR_FALSE);
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  PRBool result = PR_FALSE;
  int len = ::SendMessage(mWnd, CB_GETLBTEXTLEN, (int)aPosition, (LPARAM)0); 
  if (len != CB_ERR) {
    char * str = new char[len+1];
    anItem.SetLength(0);
    int status = ::SendMessage(mWnd, CB_GETLBTEXT, (int)aPosition, (LPARAM)(LPCTSTR)str); 
    if (status != CB_ERR) {
      anItem.Append(str);
      result = PR_TRUE;
    }
    delete str;
  }
  return result;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::GetSelectedItem(nsString& aItem)
{
  
  int index = ::SendMessage(mWnd, CB_GETCURSEL, (int)0, (LPARAM)0); 
  GetItemAt(aItem, index); 
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedIndex()
{  
  return ::SendMessage(mWnd, CB_GETCURSEL, (int)0, (LPARAM)0); 
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::SelectItem(PRInt32 aPosition)
{
  ::SendMessage(mWnd, CB_SETCURSEL, (int)aPosition, (LPARAM)0); 
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::Deselect()
{
  ::SendMessage(mWnd, CB_SETCURSEL, (WPARAM)-1, (LPARAM)0); 
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//  destructor
//
//-------------------------------------------------------------------------
nsComboBox::~nsComboBox()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsComboBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kInsComboBoxIID, NS_ICOMBOBOX_IID);
  static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);

  if (aIID.Equals(kInsComboBoxIID)) {
    *aInstancePtr = (void*) ((nsIComboBox*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kInsListWidgetIID)) {
    *aInstancePtr = (void*) ((nsIListWidget*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return nsWindow::QueryInterface(aIID,aInstancePtr);
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsComboBox::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsComboBox::OnPaint()
{
    return PR_FALSE;
}

PRBool nsComboBox::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsComboBox::WindowClass()
{
  return("COMBOBOX");
}

//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsComboBox::WindowStyle()
{
  return (CBS_DROPDOWNLIST | WS_BORDER | WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL);
}

//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------
DWORD nsComboBox::WindowExStyle()
{
    return WS_EX_CLIENTEDGE;
}

//-------------------------------------------------------------------------
//
// Cache the drop down list height in mDropDownHeight
//
//-------------------------------------------------------------------------

NS_METHOD nsComboBox::PreCreateWidget(nsWidgetInitData *aInitData)
{
  nsComboBoxInitData* comboData = (nsComboBoxInitData*)aInitData;
  mDropDownHeight = comboData->mDropDownHeight;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Modify the height passed to create and resize to be 
// the combo box drop down list height. (Note: Windows uses
// the height of the window to specify the drop-down list size,
// not the height of combobox text area.
//
//-------------------------------------------------------------------------

PRInt32 nsComboBox::GetHeight(PRInt32 aProposedHeight)
{
  return(mDropDownHeight);
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsComboBox::GetBounds(nsRect &aRect)
{
  nsWindow::GetNonClientBounds(aRect);
  return NS_OK;
}

/**
 * Renders the TextWidget for Printing
 *
 **/
NS_METHOD nsComboBox::Paint(nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect)
{
  nsBaseWidget::Paint(aRenderingContext, aDirtyRect);
  /*nsRect rect;
  GetBoundsAppUnits(rect, aTwipsConversion);
  aRenderingContext.SetColor(NS_RGB(0,0,0));

  nscolor bgColor  = NS_RGB(255,255,255);
  nscolor fgColor  = NS_RGB(0,0,0);
  nscolor hltColor = NS_RGB(240,240,240);
  nscolor sdwColor = NS_RGB(128,128,128);
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsRepository::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  bgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetForeground,  fgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DShadow,    sdwColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DHighlight, hltColor);
  }

  nsIDeviceContext * context;
  //nsDrawingSurface surface;
  aRenderingContext.GetDeviceContext(context);
  //context->GetDrawingSurface(aRenderingContext, surface);

  //HDC the_hdc = ((nsDrawingSurfaceWin *)&surface)->mDC;

  //::SendMessage(mWnd, WM_PAINT, (WPARAM)the_hdc, NULL);
  

  // shrink by one pixel
  
  nscoord onePixel  = nscoord((aTwipsConversion+0.6F));
  nscoord twoPixels = onePixel*2;

  nsString text("(ComboBox)");
  //GetSelectedItem(text);


  aRenderingContext.SetColor(bgColor);
  aRenderingContext.FillRect(rect);

  aRenderingContext.SetColor(NS_RGB(0,0,0));
  aRenderingContext.DrawRect(rect);


  aRenderingContext.SetFont(*mFont);

  nscoord textWidth;
  nscoord textHeight;
  aRenderingContext.GetWidth(text, textWidth);

  nsIFontMetrics* metrics;
  context->GetMetricsFor(*mFont, metrics);
  metrics->GetMaxAscent(textHeight);

  nscoord x = (twoPixels * 2)  + rect.x;
  nscoord y = ((rect.height - textHeight) / 2) + rect.y;
  //aRenderingContext.DrawString(text, x, y);
  */
  return NS_OK;
}



