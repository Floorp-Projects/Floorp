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

  XmListAddItem(mWidget, str, (int)aPosition+1);
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

  XmString str = XmStringCreate(val, XmFONTLIST_DEFAULT_TAG);

  int index = XmListItemPos(mWidget, str)-1;
  if (index < aStartPos) {
    index = -1;
  }
  NS_FREE_STR_BUF(val);
  XmStringFree(str);

  return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
  int count = 0;
  XtVaGetValues(mWidget, XmNitemCount, &count, nsnull);

  return (PRInt32)count;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
  int count = 0;
  XtVaGetValues(mWidget, XmNitemCount, &count, nsnull);
  if (aPosition >= 0 && aPosition < count) {
    XmListDeletePos(mWidget, aPosition+1);
    return PR_TRUE;
  }
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//  
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  PRBool result = PR_FALSE;
  XmStringTable list;

  int count = 0;
  XtVaGetValues(mWidget,  XmNitems, &list, XmNitemCount, &count, nsnull);

  if (aPosition >= 0 && aPosition < count) {
    char * text;
    if (XmStringGetLtoR(list[aPosition], XmFONTLIST_DEFAULT_TAG, &text)) {
      anItem.SetLength(0);
      anItem.Append(text);
      XtFree(text);
      result = PR_TRUE;
    }
  }

  return result;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
void nsListBox::GetSelectedItem(nsString& aItem)
{
  int * list;
  int   count;

  if (XmListGetSelectedPos(mWidget, &list, &count)) {
    GetItemAt(aItem, list[0]-1); 
    XtFree((char *)list);
  }
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{  
  if (!mMultiSelect) { 
    int * list;
    int   count;

    if (XmListGetSelectedPos(mWidget, &list, &count)) {
      int index = -1;
      if (count > 0) {
        index = list[0]-1;
      }
      XtFree((char *)list);
      return index;
    }
  } else {
    NS_ASSERTION(0, "Multi selection list box does not support GetSlectedIndex()");
  }
  return -1;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
void nsListBox::SelectItem(PRInt32 aPosition)
{
  int count = 0;
  XtVaGetValues(mWidget,  XmNitemCount, &count, nsnull);
  if (aPosition >= 0 && aPosition < count) {
    XmListSelectPos(mWidget, aPosition+1, FALSE);
  }
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedCount()
{
  int count = 0;
  XtVaGetValues(mWidget,  XmNselectedItemCount, &count, nsnull);
  return (PRInt32)count;
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
void nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  int * list;
  int   count;

  if (XmListGetSelectedPos(mWidget, &list, &count)) {
    int num = aSize > count?count:aSize;
    int i;
    for (i=0;i<num;i++) {
      aIndices[i] = (PRInt32)list[i]-1;
    }
    XtFree((char *)list);
  }
}

//-------------------------------------------------------------------------
//
//  SetSelectedIndices
//
//-------------------------------------------------------------------------
void nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  if (GetSelectedCount() > 0) {
    XtVaSetValues(mWidget, XmNselectedItemCount, 0, NULL);
  }
  int i;
  for (i=0;i<aSize;i++) {
    SelectItem(aIndices[i]);
  }
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
void nsListBox::Deselect()
{
  XtVaSetValues(mWidget, XmNselectedItemCount, 0, NULL);
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
        *aInstancePtr = (void*) ((nsIListBox*)&mAggWidget);
        AddRef();
        result = NS_OK;
      }
      else if (aIID.Equals(kInsListWidgetIID)) {
        *aInstancePtr = (void*) ((nsIListWidget*)&mAggWidget);
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

  unsigned char selectionPolicy;
  
  Boolean autoSelection;
  if (mMultiSelect) {
    //selectionPolicy = XmEXTENDED_SELECT;
    selectionPolicy = XmMULTIPLE_SELECT;
    autoSelection   = TRUE;
  } else {
    selectionPolicy = XmBROWSE_SELECT;
    autoSelection   = FALSE;
  }

  mWidget = ::XtVaCreateManagedWidget("",
                                    xmListWidgetClass,
                                    parentWidget,
                                    XmNitemCount, 0,
                                    XmNx, aRect.x,
                                    XmNy, aRect.y,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNselectionPolicy, selectionPolicy,
                                    XmNautomaticSelection, autoSelection,
                                    XmNscrollBarDisplayPolicy, XmAS_NEEDED,
                                    XmNlistSizePolicy, XmCONSTANT,
                                    XmNmarginTop, 0,
                                    XmNmarginBottom, 0,
                                    XmNmarginLeft, 0,
                                    XmNmarginRight, 0,
                                    XmNmarginHeight, 0,
                                    XmNmarginWidth, 0,
                                    XmNlistMarginHeight, 0,
                                    XmNlistMarginWidth, 0,
                                    XmNscrolledWindowMarginWidth, 0,
                                    XmNscrolledWindowMarginHeight, 0,
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
void nsListBox::Create(nsNativeWidget aParent,
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
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
void nsListBox::AggListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  GET_OUTER()->SetSelectedIndices(aIndices, aSize);
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


