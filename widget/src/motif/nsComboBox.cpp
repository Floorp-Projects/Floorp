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
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include <Xm/RowColumn.h>
#include <Xm/PushB.h>

#define DBG 0

#define INITIAL_MAX_ITEMS 128
#define ITEMS_GROWSIZE    128


//-------------------------------------------------------------------------
//
// nsComboBox constructor
//
//-------------------------------------------------------------------------
nsComboBox::nsComboBox(nsISupports *aOuter) : nsWindow(aOuter)
{
  mMultiSelect = PR_FALSE;
  mBackground  = NS_RGB(124, 124, 124);

  mMaxNumItems = INITIAL_MAX_ITEMS;
  mItems       = (Widget *)new long[INITIAL_MAX_ITEMS];
  mNumItems    = 0;
}

//-------------------------------------------------------------------------
//
// nsComboBox:: destructor
//
//-------------------------------------------------------------------------
nsComboBox::~nsComboBox()
{
  if (mItems != nsnull) {
    delete[] mItems;
  }
}

//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

void nsComboBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  mMultiSelect = aMultipleSelections;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

void nsComboBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);

  XmString str;

  Arg	args[30];
  int	argc = 0;

  str = XmStringCreateLocalized(val);
  XtSetArg(args[argc], XmNlabelString, str); argc++;

  Widget btn = XmCreatePushButton(mPullDownMenu, val, args, argc);
  XtManageChild(btn);

  if (mNumItems == mMaxNumItems) {
    // [TODO] Grow array here by ITEMS_GROWSIZE
  }
  mItems[mNumItems++] = (long)btn;

  NS_FREE_STR_BUF(val);
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  NS_ALLOC_STR_BUF(val, aItem, 256);

  int i;
  PRInt32 index = -1;
  for (i=0;i<mNumItems && index == -1;i++) {
    XmString str;
    XtVaGetValues(mItems[i], XmNlabelString, &str, nsnull);
    char * text;
    if (XmStringGetLtoR(str, XmFONTLIST_DEFAULT_TAG, &text)) {
      if (!strcmp(text, val)) {
        index = i;
      }
      XtFree(text);
    }
  }
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
  return (PRInt32)mNumItems;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsComboBox::RemoveItemAt(PRInt32 aPosition)
{

  if (aPosition >= 0 && aPosition < mNumItems) {
    XtUnmanageChild(mItems[aPosition]);
    XtDestroyWidget(mItems[aPosition]);
    int i;
    for (i=aPosition ; i < mNumItems-1; i++) {
      mItems[i] = mItems[i+1];
    }
    mItems[mNumItems-1] = NULL;
    mNumItems--;
    return PR_TRUE;
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

  if (aPosition < 0 || aPosition >= mNumItems) {
    return result;
  }

  XmString str;
  XtVaGetValues(mItems[aPosition], XmNlabelString, &str, nsnull);
  char * text;
  if (XmStringGetLtoR(str, XmFONTLIST_DEFAULT_TAG, &text)) {
    anItem = text;
    XtFree(text);
  }

  return result;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
void nsComboBox::GetSelectedItem(nsString& aItem)
{
  Widget w;
  XtVaGetValues(mWidget, XmNmenuHistory, &w, NULL);
  int i;
  for (i=0;i<mNumItems;i++) {
    if (mItems[i] == w) {
      GetItemAt(aItem, i);
    }
  }
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedIndex()
{  
  if (!mMultiSelect) { 
    Widget w;
    XtVaGetValues(mWidget, XmNmenuHistory, &w, NULL);
    int i;
    for (i=0;i<mNumItems;i++) {
      if (mItems[i] == w) {
        return (PRInt32)i;
      }
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
void nsComboBox::SelectItem(PRInt32 aPosition)
{
  if (!mMultiSelect) { 
    if (aPosition >= 0 && aPosition < mNumItems) {
      XtVaSetValues(mWidget,
		    XmNmenuHistory, mItems[aPosition],
		    NULL);
    }
  } else {
    // this is an error
  }
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedCount()
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
void nsComboBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  // this is an error
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
void nsComboBox::Deselect()
{
  if (!mMultiSelect) { 
    //::SendMessage(mWnd, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0); 
  } else {
    // this is an error
  }

}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsComboBox::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kIComboBoxIID, NS_ICOMBOBOX_IID);
    static NS_DEFINE_IID(kIListWidgetIID, NS_ILISTWIDGET_IID);
    if (result == NS_NOINTERFACE) {
      if (aIID.Equals(kIComboBoxIID)) {
        *aInstancePtr = (void*) ((nsIComboBox*)&mAggWidget);
        AddRef();
        result = NS_OK;
      }
      else if (aIID.Equals(kIListWidgetIID)) {
        *aInstancePtr = (void*) ((nsIListWidget*)&mAggWidget);
        AddRef();
        result = NS_OK;
      }
    }

    return result;
}

//-------------------------------------------------------------------------
//
// nsComboBox Creator
//
//-------------------------------------------------------------------------
void nsComboBox::Create(nsIWidget *aParent,
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

  /*mWidget = ::XtVaCreateManagedWidget("",
                                    xmListWidgetClass,
                                    parentWidget,
                                    XmNitemCount, 0,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNx, aRect.x,
                                    XmNy, aRect.y,
                                    nsnull);
  */
    Arg	args[30];
    int	argc;

    argc = 0;
    XtSetArg(args[argc], XmNx, 0); argc++;
    XtSetArg(args[argc], XmNy, 0); argc++;
    mPullDownMenu = XmCreatePulldownMenu(parentWidget, "pulldown", args, argc);

    argc = 0;
    XtSetArg(args[argc], XmNmarginHeight, 0); argc++;
    XtSetArg(args[argc], XmNmarginWidth, 0); argc++;
    XtSetArg(args[argc], XmNrecomputeSize, False); argc++;
    XtSetArg(args[argc], XmNresizeHeight, False); argc++;
    XtSetArg(args[argc], XmNresizeWidth, False); argc++;
    XtSetArg(args[argc], XmNspacing, False); argc++;
    XtSetArg(args[argc], XmNborderWidth, 0); argc++;
    XtSetArg(args[argc], XmNnavigationType, XmTAB_GROUP); argc++;
    XtSetArg(args[argc], XmNtraversalOn, True); argc++;
    XtSetArg(args[argc], XmNorientation, XmVERTICAL); argc++;
    XtSetArg(args[argc], XmNadjustMargin, False); argc++;
    XtSetArg(args[argc], XmNsubMenuId, mPullDownMenu); argc++;
    XtSetArg(args[argc], XmNuserData, (XtPointer)this); argc++;
    XtSetArg(args[argc], XmNx, aRect.x); argc++;
    XtSetArg(args[argc], XmNy, aRect.y); argc++;
    XtSetArg(args[argc], XmNwidth, aRect.width); argc++;
    XtSetArg(args[argc], XmNheight, aRect.height); argc++;
    mWidget = XmCreateOptionMenu(parentWidget, "", args, argc);

    mOptionMenu = XmOptionLabelGadget(mWidget);
    XtUnmanageChild(mOptionMenu);

    /*XtVaSetValues(mWidget, 
                  XmNx, aRect.x, 
                  XmNy, aRect.y, 
                  XmNwidth, aRect.width, 
                  XmNheight, aRect.height, 
                  nsnull);*/
    //XtManageChild(mPullDownMenu);
    //XtManageChild(mOptionMenu);

    //XtSetMappedWhenManaged(mOptionMenu, False);
    //XtManageChild(mOptionMenu);


  //if (DBG) 

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  //InitCallbacks();

}

//-------------------------------------------------------------------------
//
// nsComboBox Creator
//
//-------------------------------------------------------------------------
void nsComboBox::Create(nsNativeWidget aParent,
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
PRBool nsComboBox::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsComboBox::OnPaint(nsPaintEvent &aEvent)
{
  return PR_FALSE;
}

PRBool nsComboBox::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
#define GET_OUTER() ((nsComboBox*) ((char*)this - nsComboBox::GetOuterOffset()))


//-------------------------------------------------------------------------
//
//  SetMultipleSelection
//
//-------------------------------------------------------------------------

void nsComboBox::AggComboBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  GET_OUTER()->SetMultipleSelection(aMultipleSelections);
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

void nsComboBox::AggComboBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  GET_OUTER()->AddItemAt(aItem, aPosition);
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::AggComboBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  return  GET_OUTER()->FindItem(aItem, aStartPos);
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::AggComboBox::GetItemCount()
{
  return GET_OUTER()->GetItemCount();
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsComboBox::AggComboBox::RemoveItemAt(PRInt32 aPosition)
{
  return GET_OUTER()->RemoveItemAt(aPosition);
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::AggComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  return  GET_OUTER()->GetItemAt(anItem, aPosition);
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
void nsComboBox::AggComboBox::GetSelectedItem(nsString& aItem)
{
  GET_OUTER()->GetSelectedItem(aItem);
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::AggComboBox::GetSelectedIndex()
{  
  return GET_OUTER()->GetSelectedIndex();
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
void nsComboBox::AggComboBox::SelectItem(PRInt32 aPosition)
{
  GET_OUTER()->SelectItem(aPosition);
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::AggComboBox::GetSelectedCount()
{
  return  GET_OUTER()->GetSelectedCount();
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
void nsComboBox::AggComboBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  GET_OUTER()->GetSelectedIndices(aIndices, aSize);
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
void nsComboBox::AggComboBox::Deselect()
{
  GET_OUTER()->Deselect();
}


//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsComboBox, AggComboBox);


