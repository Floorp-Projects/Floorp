/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsFilePicker.h"
#include "nsILocalFile.h"

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
{
  NS_INIT_REFCNT();
  mWidget = nsnull;
  mDisplayDirectory = nsnull;
  mFilterMenu = nsnull;
  mOptionMenu = nsnull;
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
  if (mFilterMenu)
  {
    GtkWidget *menu_item;
    GList *list = g_list_first(GTK_MENU_SHELL(mFilterMenu)->children);

    for (;list; list = list->next)
    {
      menu_item = GTK_WIDGET(list->data);
      gchar *data = (gchar*)gtk_object_get_data(GTK_OBJECT(menu_item), "filters");
      
      if (data)
        nsCRT::free(data);
    }
  }

  gtk_widget_destroy(mWidget);
}


static void file_ok_clicked(GtkWidget *w, PRBool *ret)
{
#ifdef DEBUG
  g_print("user hit ok\n");
#endif
  *ret = PR_TRUE;
  gtk_main_quit();
}

static void file_cancel_clicked(GtkWidget *w, PRBool *ret)
{
#ifdef DEBUG
  g_print("user hit cancel\n");
#endif
  *ret = PR_FALSE;
  gtk_main_quit();
}

#ifdef SET_FILTER_LIST_IS_WORKING
static void filter_item_activated(GtkWidget *w, gpointer data)
{
#ifdef DEBUG
  //  nsFilePicker *f = (nsFilePicker*)data;
  gchar *foo = (gchar*)gtk_object_get_data(GTK_OBJECT(w), "filters");
  g_print("filter_item_activated(): %s\n", foo);
#endif
}
#endif /* SET_FILTER_LIST_IS_WORKING */

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *retval)
{
  NS_ENSURE_ARG_POINTER(retval);

  PRBool ret;
  if (mWidget) {
    // make things shorter
    GtkFileSelection *fs = GTK_FILE_SELECTION(mWidget);

    if (mNumberOfFilters != 0)
    {
      gtk_option_menu_set_menu(GTK_OPTION_MENU(mOptionMenu), mFilterMenu);
    }
    else
      gtk_widget_hide(mOptionMenu);

#if 0
    if (mDisplayDirectory)
      gtk_file_selection_complete(fs, "/");
#endif

    //    gtk_window_set_modal(GTK_WINDOW(mWidget), PR_TRUE);
    gtk_widget_show(mWidget);

    // handle close, destroy, etc on the dialog
    gtk_signal_connect(GTK_OBJECT(fs->ok_button), "clicked",
                       GTK_SIGNAL_FUNC(file_ok_clicked),
                       &ret);
    gtk_signal_connect(GTK_OBJECT(fs->cancel_button), "clicked",
                       GTK_SIGNAL_FUNC(file_cancel_clicked),
                       &ret);
    // start new loop.   ret is set in the above callbacks.
    gtk_main();
  }
  else {
    ret = PR_FALSE;
  }

  if (ret)
    *retval = returnOK;
  else
    *retval = returnCancel;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsFilePicker::SetFilterList(PRUint32 aNumberOfFilters,
                                          const PRUnichar **aTitles,
                                          const PRUnichar **filters)
{
#ifdef SET_FILTER_LIST_IS_WORKING
  GtkWidget *menu_item;

  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;

  mFilterMenu = gtk_menu_new();

  for(unsigned int i=0; i < aNumberOfFilters; i++)
  {
    // we need *.{htm, html, xul, etc}

    char *filters = ToNewCString(aFilters[i]);
#ifdef DEBUG
    char *foo = ToNewCString(aTitles[i]);
    printf("%20s %s\n", foo, filters);
    nsCRT::free(foo);
#endif

    menu_item = gtk_menu_item_new_with_label(nsCAutoString(aTitles[i]));

    gtk_object_set_data(GTK_OBJECT(menu_item), "filters", filters);

    gtk_signal_connect(GTK_OBJECT(menu_item),
                       "activate",
                       GTK_SIGNAL_FUNC(filter_item_activated),
                       this);

    gtk_menu_append(GTK_MENU(mFilterMenu), menu_item);
    gtk_widget_show(menu_item);

    nsCRT::free(filters);
  }
#endif /* SET_FILTER_LIST_IS_WORKING */
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(*aFile);
  if (mWidget) {
    gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(mWidget));

    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    file->InitWithPath(fn);

    file->QueryInterface(NS_GET_IID(nsILocalFile), (void**)aFile);
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetSelectedFilter(PRInt32 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mSelectedType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const PRUnichar *aString)
{
  if (mWidget) {
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(mWidget),
                                    (const gchar*)nsCAutoString(aString));
  }
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(PRUnichar **aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDisplayDirectory(nsILocalFile *aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDisplayDirectory(nsILocalFile **aDirectory)
{
  *aDirectory = mDisplayDirectory;
  NS_IF_ADDREF(*aDirectory);
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::Create(nsIDOMWindowInternal *aParent,
                                   const PRUnichar *aTitle,
                                   PRInt16 aMode)
{
  return nsBaseFilePicker::Create(aParent, aTitle, aMode);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::CreateNative(nsIWidget *aParent,
                                         const PRUnichar *aTitle,
                                         PRInt16 aMode)
{
  mWidget = gtk_file_selection_new((const gchar *)nsCAutoString(aTitle));
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  gtk_button_box_set_layout(GTK_BUTTON_BOX(GTK_FILE_SELECTION(mWidget)->button_area), GTK_BUTTONBOX_SPREAD);

  mOptionMenu = gtk_option_menu_new();

  gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(mWidget)->main_vbox), mOptionMenu, PR_FALSE, PR_FALSE, 0);
  gtk_widget_show(mOptionMenu);

  // Hide the file column for the folder case.
  if (aMode == nsIFilePicker::modeGetFolder) {
    gtk_widget_hide((GTK_FILE_SELECTION(mWidget)->file_list)->parent);
  }

  return NS_OK;
}

gint
nsFilePicker::DestroySignal(GtkWidget *  aGtkWidget,
                            nsFilePicker* aWidget)
{
  aWidget->OnDestroySignal(aGtkWidget);
  return TRUE;
}

void
nsFilePicker::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mWidget) {
    mWidget = nsnull;
  }
}
