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

#include <gtk/gtk.h>

#include "nsComboBox.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include "nsIDeviceContext.h"

#define DBG 0

#define INITIAL_MAX_ITEMS 128
#define ITEMS_GROWSIZE    128

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
  mMultiSelect = PR_FALSE;
  mBackground  = NS_RGB(124, 124, 124);

  mMaxNumItems = INITIAL_MAX_ITEMS;
#if 0
  mItems       = (GtkWidget *)new long[INITIAL_MAX_ITEMS];
#endif
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
// Set the foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::SetForegroundColor(const nscolor &aColor)
{
#if 0
  nsWindow::SetForegroundColor(aColor);
  PRUint32 pixel;
  mContext->ConvertPixel(aColor, pixel);
  XtVaSetValues(mOptionMenu, XtNforeground, pixel, nsnull);
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::SetBackgroundColor(const nscolor &aColor)
{
#if 0
  nsWindow::SetForegroundColor(aColor);
  PRUint32 pixel;
  mContext->ConvertPixel(aColor, pixel);
  XtVaSetValues(mOptionMenu, XtNbackground, pixel, nsnull);
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

NS_METHOD nsComboBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  mMultiSelect = aMultipleSelections;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

NS_METHOD nsComboBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
#if 0
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
  mItems[mNumItems++] = btn;

  NS_FREE_STR_BUF(val);
#endif
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
#else
  return -1;
#endif
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
#if 0
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
#endif
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
#if 0
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
#endif
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::GetSelectedItem(nsString& aItem)
{
#if 0
  Widget w;
  XtVaGetValues(mWidget, XmNmenuHistory, &w, NULL);
  int i;
  for (i=0;i<mNumItems;i++) {
    if (mItems[i] == w) {
      GetItemAt(aItem, i);
    }
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedIndex()
{
#if 0
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
    NS_ASSERTION(PR_FALSE, "Multi selection list box does not support GetSlectedIndex()");
  }
#endif
  return -1;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::SelectItem(PRInt32 aPosition)
{
#if 0
  if (!mMultiSelect) { 
    if (aPosition >= 0 && aPosition < mNumItems) {
      XtVaSetValues(mWidget,
		    XmNmenuHistory, mItems[aPosition],
		    NULL);
    }
  } else {
    // this is an error
    return NS_ERROR_FAILURE;
  }
#endif
  return NS_OK;
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
    return 0;
  }
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  // this is an error
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::Deselect()
{
  if (mMultiSelect) { 
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
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
    AddRef();
    return NS_OK;
  }
  else if (aIID.Equals(kInsListWidgetIID)) {
    *aInstancePtr = (void*) ((nsIListWidget*)this);
    AddRef();
    return NS_OK;
  }

  return nsWindow::QueryInterface(aIID,aInstancePtr);
}


//-------------------------------------------------------------------------
//
// nsComboBox Creator
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
#if 0
  aParent->AddChild(this);
  GtkWidget *parentWidget = nsnull;

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else if (aAppShell) {
    parentWidget = (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

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

  // save the event callback function
  mEventCallback = aHandleEventFunction;
#endif
  //InitCallbacks();
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsComboBox Creator
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return NS_ERROR_FAILURE;
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


