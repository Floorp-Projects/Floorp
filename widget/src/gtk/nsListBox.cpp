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
#include "gtklayout.h"

#include "nsListBox.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

NS_IMPL_ADDREF(nsListBox)
NS_IMPL_RELEASE(nsListBox)

//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox() : nsWidget(), nsIListWidget(), nsIListBox()
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
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

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
//  initializer
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  mMultiSelect = aMultipleSelections;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  NS_ALLOC_STR_BUF(text, aItem, 256);
//  str = XmStringCreateLocalized(val);

  gtk_clist_insert(GTK_CLIST(mWidget), (int)aPosition+1, NULL);
  gtk_clist_set_text(GTK_CLIST(mWidget), (int)aPosition+1, 0, text);
  gtk_clist_set_row_data(GTK_CLIST(mWidget), (int)aPosition+1, aItem);

  NS_FREE_STR_BUF(text);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  int index = gtk_clist_find_row_from_data(GTK_CLIST(mWidget), aItem)-1;

  if (index < aStartPos) {
    index = -1;
  }

  return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
  return GTK_CLIST(mWidget)->rows;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
  gtk_clist_remove(GTK_CLIST(mWidget), aPosition+1);
/*
  int count = 0;
  XtVaGetValues(mWidget, XmNitemCount, &count, nsnull);
  if (aPosition >= 0 && aPosition < count) {
    XmListDeletePos(mWidget, aPosition+1);
    return PR_TRUE;
  }
  return PR_FALSE;
*/
  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  PRBool result = PR_FALSE;
  char *text = NULL;

  gtk_clist_get_text(GTK_CLIST(mWidget),aPosition,0,&text);
  if (text) {
    anItem.SetLength(0);
    anItem.Append(text);
  }
  return PR_TRUE;

#if 0
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
#endif
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedItem(nsString& aItem)
{
#if 0
  int * list;
  int   count;

  if (XmListGetSelectedPos(mWidget, &list, &count)) {
    GetItemAt(aItem, list[0]-1);
    XtFree((char *)list);
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{
#if 0
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
    NS_ASSERTION(PR_FALSE, "Multi selection list box does not support GetSelectedIndex()");
  }
#endif
  return -1;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SelectItem(PRInt32 aPosition)
{
  gtk_clist_select_row(GTK_CLIST(mWidget), aPosition+1, 0);
/*
  int count = 0;
  XtVaGetValues(mWidget,  XmNitemCount, &count, nsnull);
  if (aPosition >= 0 && aPosition < count) {
    XmListSelectPos(mWidget, aPosition+1, FALSE);
  }
*/
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedCount()
{
  return (PRInt32)g_list_length(GTK_CLIST(mWidget)->selection);
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
#if 0
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
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  SetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
#if 0
  if (GetSelectedCount() > 0) {
    XtVaSetValues(mWidget, XmNselectedItemCount, 0, NULL);
  }
  int i;
  for (i=0;i<aSize;i++) {
    SelectItem(aIndices[i]);
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::Deselect()
{
#if 0
  XtVaSetValues(mWidget, XmNselectedItemCount, 0, NULL);
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsListBox Creator
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  aParent->AddChild(this);
  GtkWidget *parentWidget = nsnull;

  if (aParent) {
    parentWidget = (GtkWidget*) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (GtkWidget*) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  GtkSelectionMode selectionPolicy;

  mWidget = gtk_clist_new(1);

  if (mMultiSelect) {
    //selectionPolicy = XmEXTENDED_SELECT;
    gtk_clist_set_selection_mode(GTK_CLIST(mWidget), GTK_SELECTION_MULTIPLE);
  } else {
    gtk_clist_set_selection_mode(GTK_CLIST(mWidget), GTK_SELECTION_BROWSE);
  }

  gtk_clist_column_titles_hide(GTK_CLIST(mWidget));

  gtk_layout_put(GTK_LAYOUT(aParent), mWidget, aRect.x, aRect.y);
/*
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
*/
  // save the event callback function
  mEventCallback = aHandleEventFunction;

  //InitCallbacks();
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsListBox Creator
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::Create(nsNativeWidget aParent,
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
