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

NS_IMPL_ADDREF_INHERITED(nsComboBox, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsComboBox, nsWidget)

//-------------------------------------------------------------------------
//
// nsComboBox constructor
//
//-------------------------------------------------------------------------
nsComboBox::nsComboBox() : nsWidget(), nsIListWidget(), nsIComboBox()
{
  NS_INIT_REFCNT();
  mMultiSelect = PR_FALSE;
  mItems = nsnull;
  mNumItems = 0;
}

//-------------------------------------------------------------------------
//
// nsComboBox:: destructor
//
//-------------------------------------------------------------------------
nsComboBox::~nsComboBox()
{
  if (mItems) {
    for (GList *items = mItems; items; items = (GList*) g_list_next(items)){
      g_free(items->data);
    }
    g_list_free(mItems);
  }
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
  NS_ALLOC_STR_BUF(val, aItem, 256);
  mItems = g_list_insert( mItems, g_strdup(val), aPosition );
  mNumItems++;
  if (mCombo) {
    gtk_combo_set_popdown_strings( GTK_COMBO( mCombo ), mItems );
  }
  NS_FREE_STR_BUF(val);
  return NS_OK;
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
  GList *items = g_list_nth(mItems, aStartPos);
  for(i=0; items != NULL; items = (GList *) g_list_next(items), i++ )
    {
      if(!strcmp(val, (gchar *) items->data))
        {
          index = i;
          break;
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

    g_free(g_list_nth(mItems, aPosition)->data);
    mItems = g_list_remove_link(mItems, g_list_nth(mItems, aPosition));
    mNumItems--;
    if (mCombo) {
      gtk_combo_set_popdown_strings(GTK_COMBO( mCombo ), mItems);
    }
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  if (aPosition >= 0 && aPosition < mNumItems) {
    anItem = (gchar *) g_list_nth(mItems, aPosition)->data;
    return PR_TRUE;
  }
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::GetSelectedItem(nsString& aItem)
{
  aItem.Truncate();
  if (mCombo) {
    aItem = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO(mCombo)->entry));
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedIndex()
{
  nsString nsstring;
  GetSelectedItem(nsstring);
  return FindItem(nsstring, 0);
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
NS_METHOD nsComboBox::SelectItem(PRInt32 aPosition)
{
  GList *pos;
  if (!mItems)
    return NS_ERROR_FAILURE;

  pos = g_list_nth(mItems, aPosition);
  if (!pos)
    return NS_ERROR_FAILURE;

  if (mCombo) {
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(mCombo)->entry),
                       (gchar *) pos->data);
  }

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

  return nsWidget::QueryInterface(aIID,aInstancePtr);
}


//-------------------------------------------------------------------------
//
// Create the native GtkCombo widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsComboBox::CreateNative(GtkWidget *parentWindow)
{
/* there is a bug in gtkcombo
   add it inside an alignment  set the usize on it..
   (set xscale yscale for the alignment to 1.0)
*/
  mWidget = ::gtk_alignment_new(1.0,1.0,1.0,1.0);
  ::gtk_widget_set_name(mWidget, "nsComboBox");
  mCombo = ::gtk_combo_new();
  gtk_widget_show(mCombo);
  /* make the stuff uneditable */
  gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(mCombo)->entry), PR_FALSE);
  gtk_container_add(GTK_CONTAINER(mWidget), mCombo);
  gtk_signal_connect(GTK_OBJECT(mCombo),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  return NS_OK;
}

void
nsComboBox::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mCombo) {
    mCombo = nsnull;
  }
  else {
    nsWidget::OnDestroySignal(aGtkWidget);
  }
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
