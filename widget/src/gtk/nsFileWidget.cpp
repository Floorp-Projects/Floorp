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
//  gtk_widget_destroy(mWidget);
}



//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------

PRBool nsFileWidget::Show()
{
  gtk_widget_show(mWidget);
/*
  char fileBuffer[MAX_PATH+1] = "";
  mDefault.ToCString(fileBuffer,MAX_PATH);

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);

  nsString filterList;
  GetFilterListArray(filterList);
  char *filterBuffer = filterList.ToNewCString();
  char *title = mTitle.ToNewCString();
  ofn.lpstrTitle = title;
  ofn.lpstrFilter = filterBuffer;
  ofn.nFilterIndex = 1;
  ofn.hwndOwner = mWnd;
  ofn.lpstrFile = fileBuffer;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_SHAREAWARE | OFN_NOCHANGEDIR | OFN_LONGNAMES | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
  
  BOOL result;

    // Save current directory, so we can reset if it changes.
  char* currentDirectory = new char[MAX_PATH+1];
  VERIFY(::GetCurrentDirectory(MAX_PATH, currentDirectory) > 0);

  if (mMode == eMode_load) {
    result = GetOpenFileName(&ofn);
  }
  else if (mMode == eMode_save) {
    result = GetSaveFileName(&ofn);
  }
  else {
    NS_ASSERTION(0, "Only load and save are supported modes"); 
  }

  VERIFY(::SetCurrentDirectory(currentDirectory));
  
   // Clean up filter buffers
  delete filterBuffer;
  delete title;

   // Set user-selected location of file or directory
  mFile.SetLength(0);
  if (result==PR_TRUE) {
    mFile.Append(fileBuffer);
  }
  
  return((PRBool)result);
  */
  // returning true causes the browser to try and display the dir structure...
//  return PR_TRUE;
  return PR_FALSE;
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
  delete fn;
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
  delete fn;
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

//  mWnd = (HWND) ((aParent) ? aParent->GetNativeData(NS_NATIVE_WINDOW) : 0); 
//  mTitle.SetLength(0);
//  mTitle.Append(aTitle);
  return NS_OK;
}

