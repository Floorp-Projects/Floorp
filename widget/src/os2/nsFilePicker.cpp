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
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#define NS_IMPL_IDS
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsWidgetDefs.h"
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIFileChannel.h"
#include "nsIStringBundle.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

MRESULT EXPENTRY FileDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

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
  char fileBuffer[MAX_PATH+1] = "";
  char *converted = ConvertToFileSystemCharset(mDefault.get());
  if (nsnull == converted) {
    mDefault.ToCString(fileBuffer,MAX_PATH);
  }
  else {
    PL_strcpy(fileBuffer, converted);
    nsMemory::Free( converted );
  }

  char *title = ConvertToFileSystemCharset(mTitle.get());
  if (nsnull == title)
    title = ToNewCString(mTitle);
  char *initialDir;
  mDisplayDirectory->GetPath(&initialDir);

  mFile.SetLength(0);

  if (mMode == modeGetFolder) {
    FILEDLG filedlg;
 
    memset(&filedlg, 0, sizeof(FILEDLG));
    filedlg.cbSize = sizeof(FILEDLG);
    filedlg.fl = FDS_OPEN_DIALOG | FDS_CENTER;
    filedlg.pfnDlgProc = FileDialogProc;
    strcpy(filedlg.szFullFile, "^");
    WinFileDlg(HWND_DESKTOP, mWnd, &filedlg);
    char* tempptr = strstr(filedlg.szFullFile, "^");
    *tempptr = '\0';
    if (filedlg.lReturn == DID_OK) {
      result = PR_TRUE;
      mDisplayDirectory->InitWithPath(filedlg.szFullFile);
      mFile.Append(filedlg.szFullFile);
    }
  }
  else {

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);

    PRInt32 l = (mFilterList.Length()+2)*2;
    char *filterBuffer = (char*) nsMemory::Alloc(l);
    int len = gWidgetModuleData->WideCharToMultiByte(0,
                                          mFilterList.get(),
                                          mFilterList.Length(),
                                          filterBuffer,
                                          l);
    filterBuffer[len] = NULL;
    filterBuffer[len+1] = NULL;
                                  
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
    ofn.Flags = OFN_SHAREAWARE | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

    LONG rv;
    if (mMode == modeOpen) {
      // FILE MUST EXIST!
      ofn.Flags |= OFN_FILEMUSTEXIST;
    }
    if( DaxOpenSave( (mMode == modeSave), &rv, &ofn, NULL) )
      result = PR_TRUE;


    // Remember what filter type the user selected
    mSelectedType = (PRInt16)ofn.nFilterIndex;

    // Store the current directory in mDisplayDirectory
    char* newCurrentDirectory = NS_STATIC_CAST( char*, nsMemory::Alloc( MAX_PATH+1 ) );
    VERIFY(gWidgetModuleData->GetCurrentDirectory(MAX_PATH, newCurrentDirectory) > 0);
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

  if (title)
    nsMemory::Free( title );

  if (result) {
    PRInt16 returnOKorReplace = returnOK;

    if (mMode == modeSave) {
      // Windows does not return resultReplace,
      //   we must check if file already exists
      nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));

      NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

      file->InitWithPath(mFile.get());

      PRBool exists = PR_FALSE;
      file->Exists(&exists);
      if (exists)
        returnOKorReplace = returnReplace;
    }
    *retval = returnOKorReplace;
  }
  else {
    *retval = returnCancel;
  }
  return NS_OK;
}



NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  if (mFile.IsEmpty())
      return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(mFile.get());

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  file->InitWithPath(mFile.get());

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), file);
  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
  NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);
  
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
		  aCharset.AssignWithConversion("windows-1252");
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

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
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

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
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


NS_IMETHODIMP
nsFilePicker::AppendFilter(const PRUnichar *aTitle, const PRUnichar *aFilter)
{
  mFilterList.Append(aTitle);
  mFilterList.AppendWithConversion('\0');
  mFilterList.Append(aFilter);
  mFilterList.AppendWithConversion('\0');

  return NS_OK;
}

MRESULT EXPENTRY FileDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   switch ( msg ) {
      case WM_INITDLG:
         {
         SWP swpDlgOrig;
         SWP swpDlgNew;
         SWP swpFileST;
         SWP swpDirST;
         SWP swpDirLB;
         SWP swpDriveST;
         SWP swpDriveCB;
         SWP swpDriveCBEF;
         SWP swpOK;
         SWP swpCancel;
         HWND hwndFileST;
         HWND hwndDirST;
         HWND hwndDirLB;
         HWND hwndDriveST;
         HWND hwndDriveCB;
         HWND hwndOK;
         HWND hwndCancel;

         hwndFileST = WinWindowFromID(hwndDlg, DID_FILENAME_TXT);
         hwndDirST = WinWindowFromID(hwndDlg, DID_DIRECTORY_TXT);
         hwndDirLB = WinWindowFromID(hwndDlg, DID_DIRECTORY_LB);
         hwndDriveST = WinWindowFromID(hwndDlg, DID_DRIVE_TXT);
         hwndDriveCB = WinWindowFromID(hwndDlg, DID_DRIVE_CB);
         hwndOK = WinWindowFromID(hwndDlg, DID_OK);
         hwndCancel = WinWindowFromID(hwndDlg, DID_CANCEL);
         
         // Reposition drives combobox
         WinQueryWindowPos(hwndOK, &swpOK);
         WinQueryWindowPos(hwndCancel, &swpCancel);
         WinQueryWindowPos(hwndDriveCB, &swpDriveCB);
         WinQueryWindowPos(WinWindowFromID(hwndDriveCB, CBID_EDIT), &swpDriveCBEF);
         WinSetWindowPos(hwndDriveCB, 0, swpDriveCB.x,
                                         swpOK.y+swpOK.cy+swpDriveCBEF.cy-swpDriveCB.cy+20,
                                         swpCancel.x+swpCancel.cx-swpOK.x, swpDriveCB.cy, SWP_MOVE | SWP_SIZE);

         // Reposition drives text
         WinQueryWindowPos(hwndDriveCB, &swpDriveCB);
         WinQueryWindowPos(hwndDriveST, &swpDriveST);
         WinSetWindowPos(hwndDriveST, 0, swpDriveST.x, swpDriveCB.y+swpDriveCB.cy+5,
                                         swpDriveST.cx*2, swpDriveST.cy, SWP_MOVE | SWP_SIZE);

         // Reposition directories listbox 
         WinQueryWindowPos(hwndDriveST, &swpDriveST);
         WinQueryWindowPos(hwndDirLB, &swpDirLB);
         WinSetWindowPos(hwndDirLB, 0, swpDirLB.x, swpDriveST.y+swpDriveST.cy+10,
                                       swpDirLB.cx*2, swpDirLB.cy, SWP_MOVE | SWP_SIZE);

         // Reposition file text
         WinQueryWindowPos(hwndDirLB, &swpDirLB);
         WinQueryWindowPos(hwndFileST, &swpFileST);
         WinSetWindowPos(hwndFileST, 0, swpFileST.x, swpDirLB.y+swpDirLB.cy+5,
                                        swpFileST.cx*2, swpFileST.cy, SWP_MOVE | SWP_SIZE);

         // Begin repositioning dialog
         WinQueryWindowPos(hwndDlg, &swpDlgOrig);
         swpDlgNew = swpDlgOrig;
         swpDlgNew.cy -= swpFileST.y;

         // Reposition directory text
         WinQueryWindowPos(hwndFileST, &swpFileST);
         WinQueryWindowPos(hwndDirST, &swpDirST);
         WinSetWindowPos(hwndDirST, 0, swpDirST.x, swpFileST.y+swpFileST.cy+5,
                                       swpDirST.cx*2, swpDirST.cy, SWP_MOVE | SWP_SIZE);

         // Size main dialog
         swpDlgNew.cy += (swpFileST.y+swpFileST.cy+5);
         swpDlgNew.cx = (swpDirLB.x*2)+swpDirLB.cx;
         swpDlgNew.x += (swpDlgOrig.cx-swpDlgNew.cx)/2;
         swpDlgNew.y += (swpDlgOrig.cy-swpDlgNew.cy)/2;
         WinSetWindowPos(hwndDlg, 0, swpDlgNew.x, swpDlgNew.y,
                                     swpDlgNew.cx, swpDlgNew.cy,
                                     SWP_SIZE | SWP_MOVE);

         // Hide unused stuff
         WinShowWindow(WinWindowFromID(hwndDlg,DID_FILES_LB), FALSE);
         WinShowWindow(WinWindowFromID(hwndDlg,DID_FILES_TXT), FALSE);
         WinShowWindow(WinWindowFromID(hwndDlg,DID_FILTER_CB), FALSE);
         WinShowWindow(WinWindowFromID(hwndDlg,DID_FILTER_TXT), FALSE);
         WinShowWindow(WinWindowFromID(hwndDlg, DID_FILENAME_ED), FALSE);
         WinShowWindow(WinWindowFromID(hwndDlg, 0x503D), FALSE);
         // Set Titlebar to nothing
         WinSetWindowText(hwndDlg, "");
         }
         break;
      case WM_CONTROL:
         {
         PFILEDLG pfiledlg;

         pfiledlg = (PFILEDLG)WinQueryWindowPtr(hwndDlg, QWL_USER);
         WinSetWindowText(WinWindowFromID(hwndDlg,DID_FILENAME_TXT), pfiledlg->szFullFile);
         }
         break;
   }      
   return WinDefFileDlgProc(hwndDlg, msg, mp1, mp2);
}
