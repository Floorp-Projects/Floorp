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

// Define so header files for openfilename are included
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
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include <windows.h>
#include <SHLOBJ.H>

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

#define FILEPICKER_PROPERTIES "chrome://global/locale/filepicker.properties"

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
  mUnicodeEncoder = nsnull;
  mUnicodeDecoder = nsnull;
  mDisplayDirectory = do_CreateInstance("component://mozilla/file/local");

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

  char *filterBuffer = mFilterList.ToNewCString();
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

  if (mMode == modeOpen) {
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
    mFile.Append(fileBuffer);
  }
  
  if (result)
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

NS_IMETHODIMP nsFilePicker::SetFilters(PRInt32 aFilterMask)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
  nsCOMPtr<nsIStringBundle> stringBundle;
  nsILocale   *locale = nsnull;

  rv = stringService->CreateBundle(FILEPICKER_PROPERTIES, locale, getter_AddRefs(stringBundle));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  PRUnichar *title;
  PRUnichar *filter;

  mFilterList.SetLength(0);
  if (aFilterMask & filterAll) {
    stringBundle->GetStringFromName(nsAutoString("allTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(nsAutoString("allFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterHTML) {
    stringBundle->GetStringFromName(nsAutoString("htmlTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(nsAutoString("htmlFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterText) {
    stringBundle->GetStringFromName(nsAutoString("textTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(nsAutoString("textFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterImages) {
    stringBundle->GetStringFromName(nsAutoString("imageTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(nsAutoString("imageFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXML) {
    stringBundle->GetStringFromName(nsAutoString("xmlTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(nsAutoString("xmlFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXUL) {
    stringBundle->GetStringFromName(nsAutoString("xulTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(nsAutoString("xulFilter").GetUnicode(), &filter);
    AppendFilter(title, filter);
  }

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::AppendFilter(const PRUnichar *aTitle,
                                         const PRUnichar *aFilter)
{
  mFilterList.Append(aTitle);
  mFilterList.Append('\0');
  mFilterList.Append(aFilter);
  mFilterList.Append('\0');

  return NS_OK;
}


NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  if (mFile.IsEmpty())
      return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("component://mozilla/file/local"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(mFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("component://mozilla/file/local"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  file->InitWithPath(mFile);

  nsCOMPtr<nsIFileURL> fileURL(do_CreateInstance("component://netscape/network/standard-url"));
  NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);
  fileURL->SetFile(file);

  NS_ADDREF(*aFileURL = fileURL);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const char *aString)
{
  mDefault = aString;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(char **aString)
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

NS_IMETHODIMP nsFilePicker::Init(nsIDOMWindow *aParent,
                                 const PRUnichar *aTitle,
                                 PRInt16 aMode)
{
  return nsBaseFilePicker::Init(aParent, aTitle, aMode);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::InitNative(nsIWidget *aParent,
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
void nsFilePicker::GetFileSystemCharset(nsString & fileSystemCharset)
{
  static nsAutoString aCharset;
  nsresult rv;

  if (aCharset.Length() < 1) {
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_PROGID, &rv);
	  if (NS_SUCCEEDED(rv)) 
		  rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");
	  if (NS_FAILED(rv)) 
		  aCharset.Assign("windows-1252");
  }
  fileSystemCharset = aCharset;
}

//-------------------------------------------------------------------------
char * nsFilePicker::ConvertToFileSystemCharset(const PRUnichar *inString)
{
  char *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (nsnull == mUnicodeEncoder) {
    nsAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeEncoder(&fileSystemCharset, &mUnicodeEncoder);
    }
  }

  // converts from unicode to the file system charset
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = nsCRT::strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeEncoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = new char[outLength+1];
      if (nsnull == outString) {
        return nsnull;
      }
      rv = mUnicodeEncoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = '\0';
      }
    }
  }
  
  return NS_SUCCEEDED(rv) ? outString : nsnull;
}

//-------------------------------------------------------------------------
PRUnichar * nsFilePicker::ConvertFromFileSystemCharset(const char *inString)
{
  PRUnichar *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (nsnull == mUnicodeDecoder) {
    nsAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeDecoder(&fileSystemCharset, &mUnicodeDecoder);
    }
  }

  // converts from the file system charset to unicode
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = nsCRT::strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeDecoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = new PRUnichar[outLength+1];
      if (nsnull == outString) {
        return nsnull;
      }
      rv = mUnicodeDecoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = 0;
      }
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "error charset conversion");
  return NS_SUCCEEDED(rv) ? outString : nsnull;
}
