/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsComboBox.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nslog.h"

NS_IMPL_LOG(nsComboBoxLog)
#define PRINTF NS_LOG_PRINTF(nsComboBoxLog)
#define FLUSH  NS_LOG_FLUSH(nsComboBoxLog)

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
	if(mMenuField && mMenuField->LockLooper())
	{
		NS_ALLOC_STR_BUF(val, aItem, 256);
		mMenuField->Menu()->AddItem(new BMenuItem(val, 0), aPosition);
		NS_FREE_STR_BUF(val);
		mMenuField->UnlockLooper();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
#if 0
  NS_ALLOC_STR_BUF(val, aItem, 256);
  int index = ::SendMessage(mWnd, CB_FINDSTRINGEXACT, (int)aStartPos, (LPARAM)(LPCTSTR)val); 
  NS_FREE_STR_BUF(val);

  return index;
#endif
	PRINTF("nsListBox::FindItem not implemented\n");
	return -1;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::GetItemCount()
{
	PRInt32	result = 0;
	if(mMenuField && mMenuField->LockLooper())
	{
		result = mMenuField->Menu()->CountItems();
		mMenuField->UnlockLooper();
	}
	return result;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsComboBox::RemoveItemAt(PRInt32 aPosition)
{
	if(mMenuField && mMenuField->LockLooper())
	{
		BMenuItem *it = mMenuField->Menu()->RemoveItem(aPosition);
		delete it;
		mMenuField->UnlockLooper();
		return it ? PR_TRUE : PR_FALSE;
	}
	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
	PRBool result = PR_FALSE;
	anItem.SetLength(0);
	if(mMenuField && mMenuField->LockLooper())
	{
		BMenuItem	*it = mMenuField->Menu()->ItemAt(aPosition);
		anItem.AppendWithConversion(it->Label());
		mMenuField->UnlockLooper();
		result = PR_TRUE;
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
    GetItemAt(aItem, GetSelectedIndex()); 
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedIndex()
{  
	PRInt32	index = -1;
	if(mMenuField && mMenuField->LockLooper())
	{
		BMenuItem	*it = mMenuField->Menu()->FindMarked();
		index = it ? mMenuField->Menu()->IndexOf(it) : -1;
		mMenuField->UnlockLooper();
	}
	return index;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::SelectItem(PRInt32 aPosition)
{
	if(mMenuField && mMenuField->LockLooper())
	{
		BMenuItem *it = mMenuField->Menu()->ItemAt(aPosition);
		if(it)
			it->SetMarked(true);
		mMenuField->UnlockLooper();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::Deselect()
{
	if(mMenuField && mMenuField->LockLooper())
	{
		BMenuItem	*it = mMenuField->Menu()->FindMarked();
		if(it)
			it->SetMarked(false);
		mMenuField->UnlockLooper();
	}
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

PRBool nsComboBox::OnPaint(nsRect &r)
{
    return PR_FALSE;
}

PRBool nsComboBox::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
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
#if 0
  nsWindow::GetNonClientBounds(aRect);
#endif
  PRINTF("nsListBox::GetBounds not wrong\n");	// the following is just a placeholder
  nsWindow::GetClientBounds(aRect);
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
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
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


BView *nsComboBox::CreateBeOSView()
{
	return mMenuField = new nsMenuFieldBeOS((nsIWidget *)this, BRect(0, 0, 0, 0), "");
}

//-------------------------------------------------------------------------
// Sub-class of BeOS MenuField
//-------------------------------------------------------------------------
nsMenuFieldBeOS::nsMenuFieldBeOS( nsIWidget *aWidgetWindow, BRect aFrame, 
    const char *aName, uint32 aResizingMode, uint32 aFlags )
  : BMenuField( aFrame, aName, "", new BPopUpMenu(""), aResizingMode, aFlags ),
    nsIWidgetStore( aWidgetWindow )
{
	SetDivider(0);
}
