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

  if (mMultiSelect)
    gtk_clist_set_selection_mode(GTK_CLIST(mCList), GTK_SELECTION_MULTIPLE);
  else
    gtk_clist_set_selection_mode(GTK_CLIST(mCList), GTK_SELECTION_BROWSE);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

NS_METHOD nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  NS_ALLOC_STR_BUF(textc, aItem, 256);
//  str = XmStringCreateLocalized(val);
  gchar *text[2];
  text[0] = textc;
  gtk_clist_insert(GTK_CLIST(mCList), (int)aPosition, text);
  gtk_clist_set_row_data(GTK_CLIST(mCList), (int)aPosition, aItem);

  NS_FREE_STR_BUF(textc);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  int index = gtk_clist_find_row_from_data(GTK_CLIST(mCList), aItem);

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
  return GTK_CLIST(mCList)->rows;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
  gtk_clist_remove(GTK_CLIST(mCList), aPosition);
/*
  int count = 0;
  XtVaGetValues(mCList, XmNitemCount, &count, nsnull);
  if (aPosition >= 0 && aPosition < count) {
    XmListDeletePos(mCList, aPosition+1);
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

  gtk_clist_get_text(GTK_CLIST(mCList),aPosition,0,&text);
  if (text) {
    anItem.SetLength(0);
    anItem.Append(text);
  }
  return PR_TRUE;

#if 0
  XmStringTable list;

  int count = 0;
  XtVaGetValues(mCList,  XmNitems, &list, XmNitemCount, &count, nsnull);

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

  if (XmListGetSelectedPos(mCList, &list, &count)) {
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

    if (XmListGetSelectedPos(mCList, &list, &count)) {
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
  gtk_clist_select_row(GTK_CLIST(mCList), aPosition, 0);
/*
  int count = 0;
  XtVaGetValues(mCList,  XmNitemCount, &count, nsnull);
  if (aPosition >= 0 && aPosition < count) {
    XmListSelectPos(mCList, aPosition+1, FALSE);
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
  return (PRInt32)g_list_length(GTK_CLIST(mCList)->selection);
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

  if (XmListGetSelectedPos(mCList, &list, &count)) {
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
    XtVaSetValues(mCList, XmNselectedItemCount, 0, NULL);
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
  gtk_clist_unselect_all(GTK_CLIST(mCList));
//  XtVaSetValues(mCList, XmNselectedItemCount, 0, NULL);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::CreateNative(GtkWidget *parentWindow)
{
  // to handle scrolling
  mWidget = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_name(mWidget, "nsListBox");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mWidget),
                                  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_ALWAYS);

  mCList = ::gtk_clist_new(1);
  gtk_clist_column_titles_hide(GTK_CLIST(mCList));
  // Default (it may be changed)
  gtk_clist_set_selection_mode(GTK_CLIST(mCList), GTK_SELECTION_BROWSE);
  gtk_widget_show(mCList);

  gtk_container_add (GTK_CONTAINER (mWidget), mCList);

  return NS_OK;
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
