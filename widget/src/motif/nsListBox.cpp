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
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include <Xm/List.h>

#define DBG 0

//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

void nsListBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  mMultiSelect = aMultipleSelections;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

void nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);

  XmString str;

  str = XmStringCreateLocalized(val);

  printf("String being added [%s] %d\n", val, aPosition);
  XmListAddItem(mWidget, str, (int)aPosition);
  NS_FREE_STR_BUF(val);
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);
  int index = 0;//::SendMessage(mWnd, LB_FINDSTRINGEXACT, (int)aStartPos, (LPARAM)(LPCTSTR)val); 
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
  return (PRInt32)0;//::SendMessage(mWnd, LB_GETCOUNT, (int)0, (LPARAM)0); 
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
  int status = 0;//::SendMessage(mWnd, LB_DELETESTRING, (int)aPosition, (LPARAM)(LPCTSTR)0); 
  return 0;//(status != LB_ERR?PR_TRUE:PR_FALSE);
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  PRBool result = PR_FALSE;
  /*int len = ::SendMessage(mWnd, LB_GETTEXTLEN, (int)aPosition, (LPARAM)0); 
  if (len != LB_ERR) {
    char * str = new char[len+1];
    anItem.SetLength(0);
    int status = ::SendMessage(mWnd, LB_GETTEXT, (int)aPosition, (LPARAM)(LPCTSTR)str); 
    if (status != LB_ERR) {
      anItem.Append(str);
      result = PR_TRUE;
    }
    delete str;
  }*/
  return result;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
void nsListBox::GetSelectedItem(nsString& aItem)
{
  int index = 0;//::SendMessage(mWnd, LB_GETCURSEL, (int)0, (LPARAM)0); 
  GetItemAt(aItem, index); 
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{  
  if (!mMultiSelect) { 
    return 0;//::SendMessage(mWnd, LB_GETCURSEL, (int)0, (LPARAM)0);
  } else {
    NS_ASSERTION(0, "Multi selection list box does not support GetSlectedIndex()");
  }
  return 0;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
void nsListBox::SelectItem(PRInt32 aPosition)
{
  if (!mMultiSelect) { 
    //::SendMessage(mWnd, LB_SETCURSEL, (int)aPosition, (LPARAM)0); 
  } else {
    //::SendMessage(mWnd, LB_SETSEL, (WPARAM) (BOOL)PR_TRUE, (LPARAM)(UINT)aPosition); 
  }
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
    return 0;//::SendMessage(mWnd, LB_GETSELCOUNT, (int)0, (LPARAM)0);
  }
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
void nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  //::SendMessage(mWnd, LB_GETSELITEMS, (int)aSize, (LPARAM)aIndices);
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
void nsListBox::Deselect()
{
  if (!mMultiSelect) { 
    //::SendMessage(mWnd, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0); 
  } else {
    //::SendMessage(mWnd, LB_SETSEL, (WPARAM) (BOOL)PR_FALSE, (LPARAM)(UINT)-1); 
  }

}


//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox(nsISupports *aOuter) : nsWindow(aOuter)
{
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
nsresult nsListBox::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsListBoxIID, NS_ILISTBOX_IID);
    static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);
    if (result == NS_NOINTERFACE) {
      if (aIID.Equals(kInsListBoxIID)) {
        *aInstancePtr = (void*) ((nsIListBox*)this);
        AddRef();
        result = NS_OK;
      }
      else if (aIID.Equals(kInsListWidgetIID)) {
        *aInstancePtr = (void*) ((nsIListWidget*)this);
        AddRef();
        result = NS_OK;
      }
    }

    return result;
}

//-------------------------------------------------------------------------
//
// nsListBox Creator
//
//-------------------------------------------------------------------------
void nsListBox::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);

  mWidget = ::XtVaCreateManagedWidget("",
                                    xmListWidgetClass,
                                    parentWidget,
                                    XmNitemCount, 0,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNx, aRect.x,
                                    XmNy, aRect.y,
                                    nsnull);

  if (DBG) fprintf(stderr, "Button 0x%x  this 0x%x\n", mWidget, this);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  //InitCallbacks();

}

//-------------------------------------------------------------------------
//
// nsListBox Creator
//
//-------------------------------------------------------------------------
void nsListBox::Create(nsNativeWindow aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
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

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsListBox::OnPaint(nsPaintEvent &aEvent)
{
  return PR_FALSE;
}

PRBool nsListBox::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
#define GET_OUTER() ((nsListBox*) ((char*)this - nsListBox::GetOuterOffset()))


//-------------------------------------------------------------------------
//
//  SetMultipleSelection
//
//-------------------------------------------------------------------------

void nsListBox::AggListBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  GET_OUTER()->SetMultipleSelection(aMultipleSelections);
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

void nsListBox::AggListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  GET_OUTER()->AddItemAt(aItem, aPosition);
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::AggListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  return  GET_OUTER()->FindItem(aItem, aStartPos);
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::AggListBox::GetItemCount()
{
  return GET_OUTER()->GetItemCount();
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::AggListBox::RemoveItemAt(PRInt32 aPosition)
{
  return GET_OUTER()->RemoveItemAt(aPosition);
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsListBox::AggListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  return  GET_OUTER()->GetItemAt(anItem, aPosition);
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
void nsListBox::AggListBox::GetSelectedItem(nsString& aItem)
{
  GET_OUTER()->GetSelectedItem(aItem);
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::AggListBox::GetSelectedIndex()
{  
  return GET_OUTER()->GetSelectedIndex();
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
void nsListBox::AggListBox::SelectItem(PRInt32 aPosition)
{
  GET_OUTER()->SelectItem(aPosition);
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::AggListBox::GetSelectedCount()
{
  return  GET_OUTER()->GetSelectedCount();
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
void nsListBox::AggListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  GET_OUTER()->GetSelectedIndices(aIndices, aSize);
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
void nsListBox::AggListBox::Deselect()
{
  GET_OUTER()->Deselect();
}


//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsListBox, AggListBox);


