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

NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
NS_IMPL_ISUPPORTS(nsFileWidget, kIFileWidgetIID);

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget() : nsIFileWidget()
{
  NS_INIT_REFCNT();
  mWidget = NULL;
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
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
  // make things shorter
  GtkFileSelection *fs = GTK_FILE_SELECTION(mWidget);
  PRBool ret;

  gtk_window_set_modal(GTK_WINDOW(mWidget), PR_TRUE);
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
  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

NS_METHOD  nsFileWidget::GetFile(nsString& aFile)
{
  gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(mWidget));

  aFile.SetLength(0);
  aFile.Append(fn);
//  g_free(fn);
  return NS_OK;
}


NS_METHOD  nsFileWidget::GetFile(nsFileSpec& aFile)
{
  gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(mWidget));

  aFile = fn; // Put the filename into the nsFileSpec instance.

  return NS_OK;
}




//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDefaultString(nsString& aString)
{
  char *fn = aString.ToNewCString();
  g_print("%s\n",fn);
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(mWidget), fn);
  delete[] fn;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDisplayDirectory(nsString& aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::GetDisplayDirectory(nsString& aDirectory)
{
  aDirectory = mDisplayDirectory;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::Create(nsIWidget *aParent,
                           nsString& aTitle,
                           nsMode aMode,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           void *aInitData)
{
  mMode = aMode;
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  char *title = mTitle.ToNewCString();

  mWidget = gtk_file_selection_new(title);

  delete[] title;

  return NS_OK;
}

nsFileDlgResults nsFileWidget::GetFile(nsIWidget *aParent,
                                       nsString &promptString,
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
                                         nsString &promptString,
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
                                       nsString &promptString,
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
