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
 *   Henry Sobotka <sobotka@axess.com>: OS/2 adaptation
 */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#define NS_IMPL_IDS
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include "nsDirPicker.h"

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
  mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
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

  PRBool result = PR_FALSE;
  char fileBuffer[CCHMAXPATH+1] = "";
  char *converted = ConvertToFileSystemCharset(mDefault.GetUnicode());
  if (nsnull == converted) {
    mDefault.ToCString(fileBuffer,CCHMAXPATH);
  }
  else {
    PL_strcpy(fileBuffer, converted);
    nsMemory::Free( converted );
  }

  char *title = ConvertToFileSystemCharset(mTitle.GetUnicode());
  if (nsnull == title)
    title = mTitle.ToNewCString();
  char *initialDir;
  mDisplayDirectory->GetPath(&initialDir);

  mFile.SetLength(0);

#ifdef XP_OS2

  if (mMode == modeGetFolder) {

    DIRPICKER dp = { { 0 }, 0, TRUE, 0 }; // modal dialog

    HWND ret = FS_PickDirectory(HWND_DESKTOP, mWnd,
                                gModuleData.hModResources, &dp);

    if (ret && dp.lReturn == DID_OK) {
      result = PR_TRUE;
      mDisplayDirectory->InitWithPath(dp.szFullFile);
      mFile.Append(dp.szFullFile);
    }
  }

  else {

    FILEDLG fdlg;
    memset(&fdlg, 0, sizeof(FILEDLG));

    fdlg.cbSize = sizeof(FILEDLG);
    fdlg.fl = FDS_CENTER;
    fdlg.pszTitle = title;

    //  XXX Unused because presently "All Files"
    //    char *filterBuffer = mFilterList.ToNewCString();
    //    strcpy(fdlg.szFullFile, filterBuffer);

    if (mMode == modeOpen)
      fdlg.fl |= FDS_OPEN_DIALOG;

    else if (mMode == modeSave) {
      fdlg.fl |= FDS_SAVEAS_DIALOG | FDS_ENABLEFILELB;

      // OS2TODO:
      // get URL leaf (if path ends in '/', use "index.html")
      // and display in filename field
    }

    else
      NS_ASSERTION(0, "Only open and save modes supported");

    WinFileDlg( HWND_DESKTOP, mWnd, &fdlg);

    if (fdlg.lReturn == DID_OK) {
      result = PR_TRUE;
      mDisplayDirectory->InitWithPath(fdlg.szFullFile);
      mFile.Append(fdlg.szFullFile);
    }

    // XXX For when filters work
    //    if (filterBuffer)
    //      nsMemory::Free(filterBuffer);
  }

#else      // Windows version for reference

  if (mMode == modeGetFolder) {

    BROWSEINFO browserInfo;
    browserInfo.hwndOwner      = mWnd;
    browserInfo.pidlRoot       = nsnull;
    browserInfo.pszDisplayName = (LPSTR)initialDir;
    browserInfo.lpszTitle      = title;
    browserInfo.ulFlags        = BIF_RETURNONLYFSDIRS;//BIF_STATUSTEXT | BIF_RETURNONLYFSDIRS;
    browserInfo.lpfn           = nsnull;
    browserInfo.lParam         = nsnull;
    browserInfo.iImage         = nsnull;

    // XXX UNICODE support is needed here --> DONE
    LPITEMIDLIST list = ::SHBrowseForFolder(&browserInfo);
    if (list != NULL) {
      result = ::SHGetPathFromIDList(list, (LPSTR)fileBuffer);
      if (result) {

        // XXXX ???? nothing done with pathStr - Henry

        nsAutoString pathStr;
        PRUnichar *unichar = ConvertFromFileSystemCharset(fileBuffer);
        if (nsnull == unichar)
          pathStr.AssignWithConversion(fileBuffer);
        else {
          pathStr.Assign(unichar);
          nsMemory::Free( unichar );
        }
          
        if (result == PR_TRUE) {
          // I think it also needs a conversion here (to unicode since appending to nsString) 
          // but doing that generates garbage file name, weird.
          mFile.Append(fileBuffer);
        }
      }
    }
  }
  else {

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);

    char *filterBuffer = mFilterList.ToNewCString();
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

    if (mMode == modeOpen) {
      // FILE MUST EXIST!
      ofn.Flags |= OFN_FILEMUSTEXIST;
      result = ::GetOpenFileName(&ofn);
    }
    else if (mMode == modeSave) {
      result = ::GetSaveFileName(&ofn);
    }
    else {
      NS_ASSERTION(0, "Only load, save and getFolder are supported modes"); 
    }
  
    // Remember what filter type the user selected
    mSelectedType = (PRInt16)ofn.nFilterIndex;

    // Store the current directory in mDisplayDirectory
    char* newCurrentDirectory = NS_STATIC_CAST( char*, nsMemory::Alloc( MAX_PATH+1 ) );

    VERIFY(::GetCurrentDirectory(MAX_PATH, newCurrentDirectory) > 0);

    mDisplayDirectory->InitWithPath(newCurrentDirectory);
    nsMemory::Free( newCurrentDirectory );

    // Clean up filter buffers
    if (filterBuffer)
      nsMemory::Free( filterBuffer );

    // Set user-selected location of file or directory
    if (result == PR_TRUE) {
      // I think it also needs a conversion here (to unicode since appending to nsString) 
      // but doing that generates garbage file name, weird.
      mFile.Append(fileBuffer);
    }

  }
#endif

  if (title)
    nsMemory::Free(title);

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

NS_IMETHODIMP nsFilePicker::AppendFilters(PRInt32 aFilterMask)
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

  if (aFilterMask & filterAll) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("allTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("allFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterHTML) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("htmlTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("htmlFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterText) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("textTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("textFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterImages) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("imageTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("imageFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXML) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xmlTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xmlFilter").GetUnicode(), &filter);
    AppendFilter(title,filter);
  }
  if (aFilterMask & filterXUL) {
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xulTitle").GetUnicode(), &title);
    stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("xulFilter").GetUnicode(), &filter);
    AppendFilter(title, filter);
  }

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::AppendFilter(const PRUnichar *aTitle,
                                         const PRUnichar *aFilter)
{
  mFilterList.Append(aTitle);
  mFilterList.AppendWithConversion('\0');
  mFilterList.Append(aFilter);
  mFilterList.AppendWithConversion('\0');

  return NS_OK;
}


NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  if (mFile.IsEmpty())
      return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(mFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  file->InitWithPath(mFile);

  nsCOMPtr<nsIFileURL> fileURL(do_CreateInstance("@mozilla.org/network/standard-url;1"));
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

NS_IMETHODIMP nsFilePicker::Init(nsIDOMWindowInternal *aParent,
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
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
	  if (NS_SUCCEEDED(rv)) 
		  rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");

	  if (NS_FAILED(rv)) 
		  aCharset.AssignWithConversion("ISO-8859-1");
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
      outString = NS_STATIC_CAST( char*, nsMemory::Alloc( outLength+1 ) );
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
      outString = NS_STATIC_CAST( PRUnichar*, nsMemory::Alloc( (outLength+1) * sizeof( PRUnichar ) ) );
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
