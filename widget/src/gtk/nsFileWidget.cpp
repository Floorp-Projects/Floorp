/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsFileWidget.h"
#include "nsIToolkit.h"

NS_IMPL_ISUPPORTS(nsFileWidget, nsIFileWidget::GetIID());

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget() : nsIFileWidget()
{
  NS_INIT_REFCNT();
  mWidget = nsnull;
  mFilterMenu = nsnull;
  mOptionMenu = nsnull;
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
  GtkWidget *menu_item;
  GList *list = g_list_first(GTK_MENU_SHELL(mFilterMenu)->children);

  for (;list; list = list->next)
  {
    menu_item = GTK_WIDGET(list->data);
    gchar *data = (gchar*)gtk_object_get_data(GTK_OBJECT(menu_item), "filters");

    if (data)
      delete[] data;
  }

  gtk_widget_destroy(mWidget);
}


static void file_ok_clicked(GtkWidget *w, PRBool *ret)
{
  g_print("user hit ok\n");
  *ret = PR_TRUE;
  gtk_main_quit();
}

static void file_cancel_clicked(GtkWidget *w, PRBool *ret)
{
  g_print("user hit cancel\n");
  *ret = PR_FALSE;
  gtk_main_quit();
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
PRBool nsFileWidget::Show()
{
  PRBool ret;
  if (mWidget) {
    // make things shorter
    GtkFileSelection *fs = GTK_FILE_SELECTION(mWidget);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(mOptionMenu), mFilterMenu);

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
  return ret;
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_METHOD nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,
				      const nsString aTitles[],
				      const nsString aFilters[])
{
  GtkWidget *menu_item;

  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;

  mFilterMenu = gtk_menu_new();

  for(unsigned int i=0; i < aNumberOfFilters; i++)
  {
    // we need *.{htm, html, xul, etc}
    char *foo = aTitles[i].ToNewCString();
    char *filters = aFilters[i].ToNewCString();
    printf("%20s %s\n", foo, filters);

    menu_item = gtk_menu_item_new_with_label(nsAutoCString(aTitles[i]));

    gtk_object_set_data(GTK_OBJECT(menu_item), "filters", filters);

    gtk_menu_append(GTK_MENU(mFilterMenu), menu_item);
    gtk_widget_show(menu_item);

    delete[] foo;
  }

  return NS_OK;
}

NS_METHOD  nsFileWidget::GetFile(nsFileSpec& aFile)
{
  if (mWidget) {
    gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(mWidget));
    aFile = fn; // Put the filename into the nsFileSpec instance.
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::GetSelectedType(PRInt16& theType)
{
  theType = mSelectedType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDefaultString(const nsString& aString)
{
  if (mWidget) {
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(mWidget),
                                    (const gchar*)nsAutoCString(aString));
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDisplayDirectory(const nsFileSpec& aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
  aDirectory = mDisplayDirectory;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::Create(nsIWidget *aParent,
                               const nsString& aTitle,
                               nsFileDlgMode aMode,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               void *aInitData)
{
  mMode = aMode;
  mTitle.SetLength(0);
  mTitle.Append(aTitle);

  mWidget = gtk_file_selection_new((const gchar *)nsAutoCString(aTitle));
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  gtk_button_box_set_layout(GTK_BUTTON_BOX(GTK_FILE_SELECTION(mWidget)->button_area), GTK_BUTTONBOX_SPREAD);

  mOptionMenu = gtk_option_menu_new();

  gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(mWidget)->main_vbox), mOptionMenu, PR_FALSE, PR_FALSE, 0);
  gtk_widget_show(mOptionMenu);


  // Hide the file column for the folder case.
  if (aMode == eMode_getfolder) {
    gtk_widget_hide((GTK_FILE_SELECTION(mWidget)->file_list)->parent);
  }

  return NS_OK;
}

gint
nsFileWidget::DestroySignal(GtkWidget *  aGtkWidget,
                            nsFileWidget* aWidget)
{
  aWidget->OnDestroySignal(aGtkWidget);
  return TRUE;
}

void
nsFileWidget::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mWidget) {
    mWidget = nsnull;
  }
}

nsFileDlgResults nsFileWidget::GetFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{
	Create(aParent, promptString, eMode_load, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

  return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::GetFolder(nsIWidget *aParent,
                                         const nsString &promptString,
                                         nsFileSpec &theFileSpec)
{
	Create(aParent, promptString, eMode_getfolder, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

  return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::PutFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{ 
	Create(aParent, promptString, eMode_save, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}
  return nsFileDlgResults_Cancel; 
}
