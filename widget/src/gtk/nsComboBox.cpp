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
#include "nsGUIEvent.h"
#include "nsString.h"

#define DBG 0

#define INITIAL_MAX_ITEMS 128
#define ITEMS_GROWSIZE    128

NS_IMPL_ADDREF_INHERITED(nsComboBox, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsComboBox, nsWidget)
NS_IMPL_QUERY_INTERFACE3(nsComboBox, nsIComboBox, nsIListWidget, nsIWidget)

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
  gtk_widget_destroy(mCombo);
}

void nsComboBox::InitCallbacks(char * aName)
{
  InstallButtonPressSignal(mWidget);
  InstallButtonReleaseSignal(mWidget);

  InstallEnterNotifySignal(mWidget);
  InstallLeaveNotifySignal(mWidget);
   
  // These are needed so that the events will go to us and not our parent.
  AddToEventMask(mWidget,
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
  PRInt32 inx = -1;
  GList *items = g_list_nth(mItems, aStartPos);
  for(i=0; items != NULL; items = (GList *) g_list_next(items), i++ )
    {
      if(!strcmp(val, (gchar *) items->data))
        {
          inx = i;
          break;
        }
    }
  NS_FREE_STR_BUF(val);
  return inx;
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
// Create the native GtkCombo widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsComboBox::CreateNative(GtkWidget *parentWindow)
{
  mWidget = ::gtk_event_box_new();

  ::gtk_widget_set_name(mWidget, "nsComboBox");
  mCombo = ::gtk_combo_new();
  gtk_widget_show(mCombo);

  /* make the stuff uneditable */
  gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(mCombo)->entry), PR_FALSE);

  gtk_signal_connect(GTK_OBJECT(mCombo),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);
  gtk_signal_connect(GTK_OBJECT(GTK_COMBO(mCombo)->popwin),
                     "unmap",
                     GTK_SIGNAL_FUNC(UnmapSignal),
                     this);

  gtk_container_add(GTK_CONTAINER(mWidget), mCombo);

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

gint
nsComboBox::UnmapSignal(GtkWidget* aGtkWidget, nsComboBox* aCombo)
{
  if (!aCombo) return PR_FALSE;
  aCombo->OnUnmapSignal(aGtkWidget);
  return PR_TRUE;
}

void
nsComboBox::OnUnmapSignal(GtkWidget * aWidget)
{
  if (!aWidget) return;

  // Generate a NS_CONTROL_CHANGE event and send it to the frame
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  nsPoint point(0,0);
  InitEvent(event, NS_CONTROL_CHANGE, &point);
  DispatchWindowEvent(&event);
}

//-------------------------------------------------------------------------
//
// Get handle for style
//
//-------------------------------------------------------------------------
/*virtual*/
void nsComboBox::SetFontNative(GdkFont *aFont)
{
  GtkStyle *style = gtk_style_copy(GTK_WIDGET (g_list_nth_data(gtk_container_children(GTK_CONTAINER (mWidget)),0))->style);
  // gtk_style_copy ups the ref count of the font
  gdk_font_unref (style->font);
  
  style->font = aFont;
  gdk_font_ref(style->font);
  
  gtk_widget_set_style(GTK_BIN (mWidget)->child, style);
  
  gtk_style_unref(style);
}
