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

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#define NS_IMPL_IDS
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include <windows.h>
#include <SHLOBJ.H>

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
{
  NS_INIT_REFCNT();
  mWnd = NULL;
  mNumberOfFilters = 0;
  mUnicodeEncoder = nsnull;
  mUnicodeDecoder = nsnull;
}

//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
  NS_IF_RELEASE(mUnicodeEncoder);
  NS_IF_RELEASE(mUnicodeDecoder);
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *retval)
{
  NS_ENSURE_ARG_POINTER(retval);

  char fileBuffer[MAX_PATH+1] = "";
  char *converted = ConvertToFileSystemCharset(mDefault.GetUnicode());
  if (nsnull == converted) {
    mDefault.ToCString(fileBuffer,MAX_PATH);
  }
  else {
    PL_strcpy(fileBuffer, converted);
    delete [] converted;
  }

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);

  nsString filterList;
  GetFilterListArray(filterList);
  char *filterBuffer = filterList.ToNewCString();
  char *title = ConvertToFileSystemCharset(mTitle.GetUnicode());
  if (nsnull == title)
    title = mTitle.ToNewCString();
  char *initialDir;
  mDisplayDirectory->GetPath(&initialDir);
  if (initialDir && *initialDir) {
     ofn.lpstrInitialDir = initialDir;
  }

  ofn.lpstrTitle   = title;
  ofn.lpstrFilter  = filterBuffer;
  ofn.nFilterIndex = 1;
  ofn.hwndOwner    = mWnd;
  ofn.lpstrFile    = fileBuffer;
  ofn.nMaxFile     = MAX_PATH;

  // XXX use OFN_NOCHANGEDIR  for M5
  ofn.Flags = OFN_SHAREAWARE | OFN_LONGNAMES | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
  
  PRBool result;

  if (mMode == modeLoad) {
    result = ::GetOpenFileName(&ofn);
  }
  else if (mMode == modeSave) {
    result = ::GetSaveFileName(&ofn);
  }
  else {
    NS_ASSERTION(0, "Only load and save are supported modes"); 
  }
  
  // Remember what filter type the user selected
  mSelectedType = (PRInt16)ofn.nFilterIndex;

   // Store the current directory in mDisplayDirectory
  char* newCurrentDirectory = new char[MAX_PATH+1];
  VERIFY(::GetCurrentDirectory(MAX_PATH, newCurrentDirectory) > 0);
  mDisplayDirectory->InitWithPath(newCurrentDirectory);
  delete[] newCurrentDirectory;

   // Clean up filter buffers
  if (filterBuffer)
    delete[] filterBuffer;
  if (title)
    delete[] title;

   // Set user-selected location of file or directory
  mFile.SetLength(0);
  if (result == PR_TRUE) {
    // I think it also needs a conversion here (to unicode since appending to nsString) 
    // but doing that generates garbage file name, weird.
    mFile.Append(fileBuffer);
  }
  
  return((PRBool)result);
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsFilePicker::SetFilterList(PRInt32 aNumberOfFilters,
                                          const PRUnichar **aTitles,
                                          const PRUnichar **aFilters)
{
  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(*aFile);

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("component://mozilla/file/local"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(nsCAutoString(mFile));

  NS_ADDREF(*aFile = file);

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
  mDefault = aString;
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

NS_IMETHODIMP nsFilePicker::Create(nsIDOMWindow *aParent,
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
  mWnd = (HWND) ((aParent) ? aParent->GetNativeData(NS_NATIVE_WINDOW) : 0); 
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;
  return NS_OK;
}



//-------------------------------------------------------------------------
//
// Convert filter titles + filters into a Windows filter string
//
//-------------------------------------------------------------------------

void nsFilePicker::GetFilterListArray(nsString& aFilterList)
{
  aFilterList.SetLength(0);
  for (PRUint32 i = 0; i < mNumberOfFilters; i++) {
    const nsString& title = mTitles[i];
    const nsString& filter = nsAutoString(mFilters[i]);
    
    aFilterList.Append(title);
    aFilterList.Append('\0');
    aFilterList.Append(filter);
    aFilterList.Append('\0');
  }
  aFilterList.Append('\0'); 
}