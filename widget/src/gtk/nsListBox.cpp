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
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsListBox, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsListBox, nsWidget)
NS_IMPL_QUERY_INTERFACE3(nsListBox, nsIListBox, nsIListWidget, nsIWidget)

//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox() : nsWidget(), nsIListWidget(), nsIListBox()
{
  NS_INIT_REFCNT();
  mMultiSelect = PR_FALSE;
  mCList = nsnull;
}

//-------------------------------------------------------------------------
//
// nsListBox:: destructor
//
//-------------------------------------------------------------------------
nsListBox::~nsListBox()
{
}

void nsListBox::InitCallbacks(char * aName)
{
  InstallButtonPressSignal(mCList);
  InstallButtonReleaseSignal(mCList);

  InstallEnterNotifySignal(mCList);
  InstallLeaveNotifySignal(mCList);

  // These are needed so that the events will go to us and not our parent.
  AddToEventMask(mCList,
                 GDK_BUTTON_PRESS_MASK |
                 GDK_BUTTON_RELEASE_MASK |
                 GDK_ENTER_NOTIFY_MASK |
                 GDK_EXPOSURE_MASK |
                 GDK_FOCUS_CHANGE_MASK |
                 GDK_KEY_PRESS_MASK |
                 GDK_KEY_RELEASE_MASK |
                 GDK_LEAVE_NOTIFY_MASK |
                 GDK_POINTER_MOTION_MASK);
}

//-------------------------------------------------------------------------
//
//  initializer
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsListBox::SetMultipleSelection(PRBool aMultipleSelections)
{
  mMultiSelect = aMultipleSelections;
  if (mCList) {
    if (mMultiSelect)
      gtk_clist_set_selection_mode(GTK_CLIST(mCList), GTK_SELECTION_MULTIPLE);
    else
      gtk_clist_set_selection_mode(GTK_CLIST(mCList), GTK_SELECTION_BROWSE);
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  if (mCList) {
    gchar *text[2];
    const nsAutoCString tempStr(aItem);
    text[0] = (gchar*)(const char *)tempStr;
    text[1] = (gchar*)NULL;
    gtk_clist_insert(GTK_CLIST(mCList), (int)aPosition, text);

    // XXX Im not sure using the string address is the right thing to 
    // store in the row data.
    gtk_clist_set_row_data(GTK_CLIST(mCList), aPosition, (gpointer)&aItem);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  int i = -1;
  if (mCList) {
    i = gtk_clist_find_row_from_data(GTK_CLIST(mCList), (gpointer)&aItem);
    if (i < aStartPos) {
      i = -1;
    }
  }
  return i;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
  if (mCList) {
    return GTK_CLIST(mCList)->rows;
  }
  else {
    return 0;
  }
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
  if (mCList) {
    gtk_clist_remove(GTK_CLIST(mCList), aPosition);
  }
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
  anItem.Truncate();
  if (mCList) {
    char *text = nsnull;
    gtk_clist_get_text(GTK_CLIST(mCList),aPosition,0,&text);
    if (text) {
      anItem.Append(text);
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
NS_IMETHODIMP nsListBox::GetSelectedItem(nsString& aItem)
{
  aItem.Truncate();
  if (mCList) {
    PRInt32 i=0, idx=-1;
    GtkCList *clist = GTK_CLIST(mCList);
    GList *list = clist->row_list;

    for (i=0; i < clist->rows && idx == -1; i++, list = list->next) {
      if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED) {
        char *text = nsnull;
        gtk_clist_get_text(GTK_CLIST(mCList),i,0,&text);
        if (text) {
          aItem.Append(text);
        }
        return NS_OK;
      }
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
  PRInt32 i=0, idx=-1;
  if (mCList) {
    if (!mMultiSelect) {
      GtkCList *clist = GTK_CLIST(mCList);
      GList *list = clist->row_list;

      for (i=0; i < clist->rows && idx == -1; i++, list = list->next) {
        if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED) {
          idx = i;
        }
      }
    } else {
      NS_ASSERTION(PR_FALSE, "Multi selection list box does not support GetSelectedIndex()");
    }
  }
  return idx;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsListBox::SelectItem(PRInt32 aPosition)
{
  if (mCList) {
    gtk_clist_select_row(GTK_CLIST(mCList), aPosition, 0);
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
  if (mCList) {
    if (!GTK_CLIST(mCList)->selection)
      return 0;
    else
      return (PRInt32)g_list_length(GTK_CLIST(mCList)->selection);
  }
  else {
    return 0;
  }
}

//-------------------------------------------------------------------------
//
//  GetSelectedIndices
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  if (mCList) {
    PRInt32 i=0, num = 0;
    GtkCList *clist = GTK_CLIST(mCList);
    GList *list = clist->row_list;

    for (i=0; i < clist->rows && num < aSize; i++, list = list->next) {
      if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED) {
        aIndices[num] = i;
        num++;
      }
    }
  }
  else {
    PRInt32 i = 0;
    for (i = 0; i < aSize; i++) aIndices[i] = 0;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  SetSelectedIndices
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{
  if (mCList) {
    gtk_clist_unselect_all(GTK_CLIST(mCList));
    int i;
    for (i=0;i<aSize;i++) {
      SelectItem(aIndices[i]);
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsListBox::Deselect()
{
  if (mCList) {
    gtk_clist_unselect_all(GTK_CLIST(mCList));
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set initial parameters
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsListBox::PreCreateWidget(nsWidgetInitData *aInitData)
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
NS_IMETHODIMP nsListBox::CreateNative(GtkWidget *parentWindow)
{
  // to handle scrolling
  mWidget = gtk_scrolled_window_new (nsnull, nsnull);
  gtk_widget_set_name(mWidget, "nsListBox");
  gtk_container_set_border_width(GTK_CONTAINER(mWidget), 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mWidget),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  mCList = ::gtk_clist_new(1);
  gtk_clist_column_titles_hide(GTK_CLIST(mCList));
  // Default (it may be changed)
  gtk_clist_set_selection_mode(GTK_CLIST(mCList), GTK_SELECTION_BROWSE);
  SetMultipleSelection(mMultiSelect);
  gtk_widget_show(mCList);
  gtk_signal_connect(GTK_OBJECT(mCList),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  gtk_container_add (GTK_CONTAINER (mWidget), mCList);

  return NS_OK;
}

void
nsListBox::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mCList) {
    mCList = nsnull;
  }
  else {
    nsWidget::OnDestroySignal(aGtkWidget);
  }
}

//-------------------------------------------------------------------------
//
// set font for listbox
//
//-------------------------------------------------------------------------
/*virtual*/
void nsListBox::SetFontNative(GdkFont *aFont)
{
  GtkStyle *style = gtk_style_copy(GTK_WIDGET (g_list_nth_data(gtk_container_children(GTK_CONTAINER (mWidget)),0))->style);
  // gtk_style_copy ups the ref count of the font
  gdk_font_unref (style->font);
  
  style->font = aFont;
  gdk_font_ref(style->font);
  
  gtk_widget_set_style(GTK_WIDGET (g_list_nth_data(gtk_container_children(GTK_CONTAINER (mWidget)),0)), style);
  
  gtk_style_unref(style);
}
