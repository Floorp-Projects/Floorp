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

#include "nsListBox.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>


NS_IMPL_ADDREF(nsListBox)
NS_IMPL_RELEASE(nsListBox)

//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::SetMultipleSelection(PRBool aMultipleSelections) 
{
  mMultiSelect = aMultipleSelections;
  return NS_OK;
}

NS_METHOD nsListBox::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsListBoxInitData* data = (nsListBoxInitData *) aInitData;
    mMultiSelect = data->mMultiSelect;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  destructor
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);
  SendMessage(mWnd, LB_INSERTSTRING, (int)aPosition, (LPARAM)(LPCTSTR)val); 
  NS_FREE_STR_BUF(val);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);
  int index = ::SendMessage(mWnd, LB_FINDSTRINGEXACT, (int)aStartPos, (LPARAM)(LPCTSTR)val); 
  NS_FREE_STR_BUF(val);

  return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
  return (PRInt32)::SendMessage(mWnd, LB_GETCOUNT, (int)0, (LPARAM)0); 
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
  int status = ::SendMessage(mWnd, LB_DELETESTRING, (int)aPosition, (LPARAM)(LPCTSTR)0); 
  return (status != LB_ERR?PR_TRUE:PR_FALSE);
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  PRBool result = PR_FALSE;
  int len = ::SendMessage(mWnd, LB_GETTEXTLEN, (int)aPosition, (LPARAM)0); 
  if (len != LB_ERR) {
    char * str = new char[len+1];
    anItem.SetLength(0);
    int status = ::SendMessage(mWnd, LB_GETTEXT, (int)aPosition, (LPARAM)(LPCTSTR)str); 
    if (status != LB_ERR) {
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
NS_METHOD nsListBox::GetSelectedItem(nsString& aItem)
{
  if (!mMultiSelect) { 
    int index = ::SendMessage(mWnd, LB_GETCURSEL, (int)0, (LPARAM)0); 
    GetItemAt(aItem, index); 
  } else {
    NS_ASSERTION(0, "Multi selection list box does not support GetSelectedItem()");
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{  
  if (!mMultiSelect) { 
    return ::SendMessage(mWnd, LB_GETCURSEL, (int)0, (LPARAM)0);
  } else {
    NS_ASSERTION(0, "Multi selection list box does not support GetSelectedIndex()");
  }
  return 0;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SelectItem(PRInt32 aPosition)
{
  if (!mMultiSelect) { 
    ::SendMessage(mWnd, LB_SETCURSEL, (int)aPosition, (LPARAM)0); 
  } else {
    ::SendMessage(mWnd, LB_SETSEL, (WPARAM) (BOOL)PR_TRUE, (LPARAM)(UINT)aPosition); 
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedCount()
{
  if (!mMultiSelect) { 
    PRInt32 inx = GetSelectedIndex();
    return (inx == -1? 0 : 1);
  } else {
    return ::SendMessage(mWnd, LB_GETSELCOUNT, (int)0, (LPARAM)0);
  }
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  ::SendMessage(mWnd, LB_GETSELITEMS, (int)aSize, (LPARAM)aIndices);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  SetSelectedIndices 
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  for (int i=0;i<aSize;i++) {
    SelectItem(aIndices[i]);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::Deselect()
{
  if (!mMultiSelect) { 
    ::SendMessage(mWnd, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0); 
  } else {
    ::SendMessage(mWnd, LB_SETSEL, (WPARAM) (BOOL)PR_FALSE, (LPARAM)(UINT)-1); 
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox() : nsWindow(), nsIListWidget(), nsIListBox()
{
  NS_INIT_REFCNT();
  mMultiSelect = PR_FALSE;
  mBackground  = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsListBox:: destructor
//
//-------------------------------------------------------------------------
nsListBox::~nsListBox()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsListBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsListBoxIID, NS_ILISTBOX_IID);
    static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);
    if (result == NS_NOINTERFACE) {
      if (aIID.Equals(kInsListBoxIID)) {
        *aInstancePtr = (void*) ((nsIListBox*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
      }
      else if (aIID.Equals(kInsListWidgetIID)) {
        *aInstancePtr = (void*) ((nsIListWidget*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
      }
    }

    return result;
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsListBox::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsListBox::OnPaint()
{
    return PR_FALSE;
}

PRBool nsListBox::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsListBox::WindowClass()
{
  return("LISTBOX");
}

//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsListBox::WindowStyle()
{
  DWORD style = (LBS_NOINTEGRALHEIGHT | WS_BORDER | WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL);
  if (mMultiSelect) 
    style = LBS_MULTIPLESEL | LBS_EXTENDEDSEL | style;

  return style;
}

//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------
DWORD nsListBox::WindowExStyle()
{
  return 0;
}

//-------------------------------------------------------------------------
//
// Clear window before paint
//
//-------------------------------------------------------------------------

PRBool nsListBox::AutoErase()
{
  return(PR_TRUE);
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::GetBounds(nsRect &aRect)
{
  nsWindow::GetNonClientBounds(aRect);
  return NS_OK;
}


