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
//  mBackground  = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsListBox:: destructor
//
//-------------------------------------------------------------------------
nsListBox::~nsListBox()
{
  if (mCList)
  {
    ::gtk_widget_destroy(mCList);
    mCList = nsnull;
  }
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
//  cout << "nsListBox::AddItemAt(" << aItem << "," << aPosition << ")" << endl;

  char *buf = aItem.ToNewCString();
  gchar *text[2];
  text[0] = buf;
  text[1] = (char *)NULL;
  gtk_clist_insert(GTK_CLIST(mCList), (int)aPosition, text);

  // XXX Im not sure using the string address is the right thing to 
  // store in the row data.
  gtk_clist_set_row_data(GTK_CLIST(mCList), (int)aPosition, (gpointer)&aItem);

  delete[] buf;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
//  cout << "nsListBox::FindItem(" << aItem << "," << aStartPos << ")" << endl;

  int index = gtk_clist_find_row_from_data(GTK_CLIST(mCList), (gpointer)&aItem);

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
  char *text = nsnull;

  gtk_clist_get_text(GTK_CLIST(mCList),aPosition,0,&text);
  if (text) {
    anItem.SetLength(0);
    anItem.Append(text);
    result = PR_TRUE;
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
  PRInt32 i=0, index=-1;
  GtkCList *clist = GTK_CLIST(mCList);
  GList *list = clist->row_list;

  for (i=0; i < clist->rows && index == -1; i++, list = list->next) {
    if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED) {
      char *text = nsnull;
      gtk_clist_get_text(GTK_CLIST(mCList),i,0,&text);
      if (text) {
        aItem.SetLength(0);
        aItem.Append(text);
      }
      return NS_OK;
    }
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
  PRInt32 i=0, index=-1;
  if (!mMultiSelect) {
    GtkCList *clist = GTK_CLIST(mCList);
    GList *list = clist->row_list;

    for (i=0; i < clist->rows && index == -1; i++, list = list->next) {
      if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED) {
        index = i;
      }
    }
  } else {
    NS_ASSERTION(PR_FALSE, "Multi selection list box does not support GetSelectedIndex()");
  }

  return index;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SelectItem(PRInt32 aPosition)
{
  gtk_clist_select_row(GTK_CLIST(mCList), aPosition, 0);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  GetSelectedCount
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedCount()
{
  if (!GTK_CLIST(mCList)->selection)
    return 0;
  else
    return (PRInt32)g_list_length(GTK_CLIST(mCList)->selection);
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  PRInt32 i=0, num = 0;
  GtkCList *clist = GTK_CLIST(mCList);
  GList *list = clist->row_list;

  for (i=0; i < clist->rows && num < aSize; i++, list = list->next) {
    if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED) {
      aIndices[num] = i;
      num++;
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  SetSelectedIndices
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  gtk_clist_unselect_all(GTK_CLIST(mCList));
  int i;
  for (i=0;i<aSize;i++) {
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
  gtk_clist_unselect_all(GTK_CLIST(mCList));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set initial parameters
//
//-------------------------------------------------------------------------
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
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::CreateNative(GtkWidget *parentWindow)
{
  // to handle scrolling
  mWidget = gtk_scrolled_window_new (nsnull, nsnull);
  gtk_widget_set_name(mWidget, "nsListBox");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mWidget),
                                  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);

  mCList = ::gtk_clist_new(1);
  gtk_clist_column_titles_hide(GTK_CLIST(mCList));
  // Default (it may be changed)
  gtk_clist_set_selection_mode(GTK_CLIST(mCList), GTK_SELECTION_BROWSE);
  SetMultipleSelection(mMultiSelect);
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
