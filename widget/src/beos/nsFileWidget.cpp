/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// Define so header files for openfilename are included
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "nsFileWidget.h"

//NS_IMPL_ISUPPORTS(nsFileWidget, NS_IFILEWIDGET_IID)
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
//  mWnd = NULL;
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------

PRBool nsFileWidget::Show()
{
printf("nsFileWidget::Show not implemented\n");
#if 0
  char fileBuffer[MAX_PATH+1] = "";
  mDefault.ToCString(fileBuffer,MAX_PATH);

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);

  nsString filterList;
  GetFilterListArray(filterList);
  char *filterBuffer = filterList.ToNewCString();
  char *title = mTitle.ToNewCString();
  char *initialDir = mDisplayDirectory.ToNewCString();
  if (mDisplayDirectory.Length() > 0) {
     ofn.lpstrInitialDir = initialDir;
  }

  ofn.lpstrTitle = title;
  ofn.lpstrFilter = filterBuffer;
  ofn.nFilterIndex = 1;
  ofn.hwndOwner = mWnd;
  ofn.lpstrFile = fileBuffer;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_SHAREAWARE | OFN_LONGNAMES | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
  
  PRBool result;

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

   // Store the current directory in mDisplayDirectory
  char* newCurrentDirectory = new char[MAX_PATH+1];
  VERIFY(::GetCurrentDirectory(MAX_PATH, newCurrentDirectory) > 0);
  mDisplayDirectory.SetLength(0);
  mDisplayDirectory.Append(newCurrentDirectory);
  delete newCurrentDirectory;


  VERIFY(::SetCurrentDirectory(currentDirectory));
  delete currentDirectory;
  
   // Clean up filter buffers
  delete filterBuffer;
  delete title;
  delete initialDir;

   // Set user-selected location of file or directory
  mFile.SetLength(0);
  if (result == PR_TRUE) {
    mFile.Append(fileBuffer);
  }
  
  return((PRBool)result);
#endif
	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// Convert filter titles + filters into a Windows filter string
//
//-------------------------------------------------------------------------

void nsFileWidget::GetFilterListArray(nsString& aFilterList)
{
  aFilterList.SetLength(0);
  for (PRUint32 i = 0; i < mNumberOfFilters; i++) {
    const nsString& title = mTitles[i];
    const nsString& filter = mFilters[i];
    
    aFilterList.Append(title);
    aFilterList.AppendWithConversion("\0");
    aFilterList.Append(filter);
    aFilterList.AppendWithConversion("\0");
  }
  aFilterList.AppendWithConversion("\0"); 
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_METHOD nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[])
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

NS_METHOD  nsFileWidget::GetFile(nsFileSpec& aFile)
{
  aFile = mFile;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDefaultString(const nsString& aString)
{
  mDefault = aString;
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
                       nsIDeviceContext *aContext = nsnull,
                       nsIAppShell *aAppShell = nsnull,
                       nsIToolkit *aToolkit = nsnull,
                       void *aInitData = nsnull)
{
//  mWnd = (HWND) ((aParent) ? aParent->GetNativeData(NS_NATIVE_WINDOW) : 0); 
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
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

NS_METHOD  nsFileWidget::GetSelectedType(PRInt16& theType)
{
  theType = mSelectedType;
  return NS_OK;
}

