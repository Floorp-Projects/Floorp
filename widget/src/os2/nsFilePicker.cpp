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
#include "nsIPlatformCharset.h"
#include "nsWidgetDefs.h"
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsIStringBundle.h"
#include "nsEnumeratorUtils.h"
#include "nsModule.h"
#include "nsCRT.h"

#include "nsOS2Uni.h"

/* Item structure */
typedef struct _MyData
{
   PAPSZ    papszIFilterList;
   ULONG    ulCurExt;
   ULONG    ulNumFilters;
}MYDATA, *PMYDATA;

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

char nsFilePicker::mLastUsedDirectory[MAX_PATH+1] = { 0 };

MRESULT EXPENTRY DirDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY FileDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
{
  mWnd = NULL;
  mUnicodeEncoder = nsnull;
  mUnicodeDecoder = nsnull;
  mSelectedType   = 0;
  mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
  pszFDFileExists[0] = '\0';
}

//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
  mFilters.Clear();
  mTitles.Clear();

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
    PL_strncpyz(fileBuffer, converted, MAX_PATH+1);
    nsMemory::Free( converted );
  }

  char *title = ConvertToFileSystemCharset(mTitle.get());
  if (nsnull == title)
    title = ToNewCString(mTitle);
  nsCAutoString initialDir;
  mDisplayDirectory->GetNativePath(initialDir);
  // If no display directory, re-use the last one.
  if(initialDir.IsEmpty())
    initialDir = mLastUsedDirectory;

  mFile.SetLength(0);

  FILEDLG filedlg;
  memset(&filedlg, 0, sizeof(FILEDLG));
  filedlg.cbSize = sizeof(FILEDLG);
  filedlg.pszTitle = title;

  if (mMode == modeGetFolder) {
    PL_strncat(filedlg.szFullFile, initialDir.get(), MAX_PATH);
    PL_strncat(filedlg.szFullFile, "\\", 1);
    PL_strncat(filedlg.szFullFile, "^", 1);
    filedlg.fl = FDS_OPEN_DIALOG | FDS_CENTER;
    filedlg.pfnDlgProc = DirDialogProc;
    DosError(FERR_DISABLEHARDERR);
    WinFileDlg(HWND_DESKTOP, mWnd, &filedlg);
    DosError(FERR_ENABLEHARDERR);
    char* tempptr = strstr(filedlg.szFullFile, "^");
    *tempptr = '\0';
    if (filedlg.lReturn == DID_OK) {
      result = PR_TRUE;
      mDisplayDirectory->InitWithNativePath(nsDependentCString(filedlg.szFullFile));
      mFile.Append(filedlg.szFullFile);
    }
  }
  else {
    PL_strncpy(filedlg.szFullFile, initialDir.get(), MAX_PATH);
    PL_strncat(filedlg.szFullFile, "\\", 1);
    PL_strncat(filedlg.szFullFile, fileBuffer, MAX_PATH);
    filedlg.fl = FDS_CENTER;
    if (mMode == modeSave) {
       filedlg.fl |= FDS_SAVEAS_DIALOG | FDS_ENABLEFILELB;
    } else if (mMode == modeOpenMultiple) {
       filedlg.fl |= FDS_MULTIPLESEL | FDS_OPEN_DIALOG;
    } else {
       filedlg.fl |= FDS_OPEN_DIALOG;
    }
    PMYDATA pmydata;
    pmydata = (PMYDATA)calloc(1, sizeof(MYDATA));
    filedlg.ulUser = (ULONG)pmydata;
    filedlg.pfnDlgProc = FileDialogProc;

    int i;

    PSZ *apszTypeList;
    apszTypeList = (PSZ *)malloc(mTitles.Count()*sizeof(PSZ)+1);
    for (i = 0; i < mTitles.Count(); i++)
    {
      const nsString& typeWide = *mTitles[i];
      PRInt32 l = (typeWide.Length()+2)*2;
      char *filterBuffer = (char*) nsMemory::Alloc(l);
      int len = WideCharToMultiByte(0,
                                    typeWide.get(),
                                    typeWide.Length(),
                                    filterBuffer,
                                    l);
      filterBuffer[len] = NULL;
      filterBuffer[len+1] = NULL;
      apszTypeList[i] = filterBuffer;
    }
    apszTypeList[i] = 0;
    filedlg.papszITypeList = (PAPSZ)apszTypeList;

    PSZ *apszFilterList;
    apszFilterList = (PSZ *)malloc(mFilters.Count()*sizeof(PSZ)+1);
    for (i = 0; i < mFilters.Count(); i++)
    {
      const nsString& filterWide = *mFilters[i];
      apszFilterList[i] = ToNewCString(filterWide);
    }
    apszFilterList[i] = 0;
    pmydata->papszIFilterList = (PAPSZ)apszFilterList;

    pmydata->ulCurExt = mSelectedType;

    PRBool fileExists;
    do {
      DosError(FERR_DISABLEHARDERR);
      WinFileDlg(HWND_DESKTOP, mWnd, &filedlg);
      DosError(FERR_ENABLEHARDERR);
      if ((filedlg.lReturn == DID_OK) && (mMode == modeSave)) {
         PRFileInfo64 fileinfo64;
         PRStatus status = PR_GetFileInfo64(filedlg.szFullFile, &fileinfo64);
         if (status == PR_SUCCESS) {
            fileExists = PR_TRUE;
         } else {
            fileExists = PR_FALSE;
         }
         if (fileExists) {
            if (!pszFDFileExists[0]) {
              HMODULE hmod;
              char LoadError[CCHMAXPATH];
              DosLoadModule(LoadError, CCHMAXPATH, "PMSDMRI", &hmod);
              WinLoadString((HAB)0, hmod, 1110, 256, pszFDSaveCaption);
              WinLoadString((HAB)0, hmod, 1135, 256, pszFDFileExists);
              int i;
              for (i=0;i<256 && pszFDFileExists[i];i++ ) {
                if (pszFDFileExists[i] == '%') {
                  pszFDFileExists[i+1] = 's';
                  break;
                }
              }
              DosFreeModule(hmod);

            }
            char pszFullText[256+CCHMAXPATH];
            sprintf(pszFullText, pszFDFileExists, filedlg.szFullFile);
            ULONG ulResponse = WinMessageBox(HWND_DESKTOP, mWnd, pszFullText,
                                             pszFDSaveCaption, 0,
                                             MB_YESNO | MB_MOVEABLE | MB_WARNING);
            if (ulResponse == MBID_YES) {
               fileExists = PR_FALSE;
            }
         }
      }
    } while (mMode == modeSave && fileExists && filedlg.lReturn == DID_OK);

    if (filedlg.lReturn == DID_OK) {
      result = PR_TRUE;
      if (mMode == modeOpenMultiple) {
        nsresult rv = NS_NewISupportsArray(getter_AddRefs(mFiles));
        NS_ENSURE_SUCCESS(rv,rv);

        if (filedlg.papszFQFilename) {
          for (int i=0;i<filedlg.ulFQFCount;i++) {
            nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
            NS_ENSURE_SUCCESS(rv,rv);

            rv = file->InitWithNativePath(nsDependentCString(*(filedlg.papszFQFilename)[i]));
            NS_ENSURE_SUCCESS(rv,rv);

            rv = mFiles->AppendElement(file);
            NS_ENSURE_SUCCESS(rv,rv);
          }
          WinFreeFileDlgList(filedlg.papszFQFilename);
        } else {
          nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
          NS_ENSURE_SUCCESS(rv,rv);

          rv = file->InitWithNativePath(nsDependentCString(filedlg.szFullFile));
          NS_ENSURE_SUCCESS(rv,rv);

          rv = mFiles->AppendElement(file);
          NS_ENSURE_SUCCESS(rv,rv);
        }
      } else {
        mFile.Append(filedlg.szFullFile);
      }
      mSelectedType = (PRInt16)pmydata->ulCurExt;
    }

    for (i = 0; i < mTitles.Count(); i++)
    {
      nsMemory::Free(*(filedlg.papszITypeList[i]));
    }
    free(filedlg.papszITypeList);

    for (i = 0; i < mFilters.Count(); i++)
    {
      nsMemory::Free(*(pmydata->papszIFilterList[i]));
    }
    free(pmydata->papszIFilterList);
    free(pmydata);
  }

  if (title)
    nsMemory::Free( title );

  if (result) {
    PRInt16 returnOKorReplace = returnOK;

    // Remember last used directory.
    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    file->InitWithNativePath(mFile);
    nsCOMPtr<nsIFile> dir;
    if (NS_SUCCEEDED(file->GetParent(getter_AddRefs(dir)))) {
      nsCOMPtr<nsILocalFile> localDir(do_QueryInterface(dir));
      if (localDir) {
        nsCAutoString newDir;
        localDir->GetNativePath(newDir);
        if(!newDir.IsEmpty())
          PL_strncpyz(mLastUsedDirectory, newDir.get(), MAX_PATH+1);
        // Update mDisplayDirectory with this directory, also.
        // Some callers rely on this.
        mDisplayDirectory->InitWithNativePath( nsDependentCString(mLastUsedDirectory) );
      }
    }

    if (mMode == modeSave) {
      // Windows does not return resultReplace,
      //   we must check if file already exists
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

  file->InitWithNativePath(mFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  file->InitWithNativePath(mFile);

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), file);
  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
  NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);
  
  NS_ADDREF(*aFileURL = fileURL);

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  NS_ENSURE_ARG_POINTER(aFiles);
  return NS_NewArrayEnumerator(aFiles, mFiles);
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const PRUnichar *aString)
{
  mDefault = aString;

  //First, make sure the file name is not too long!
  PRInt32 nameLength;
  PRInt32 nameIndex = mDefault.RFind("\\");
  if (nameIndex == kNotFound)
    nameIndex = 0;
  else
    nameIndex ++;
  nameLength = mDefault.Length() - nameIndex;
  
  if (nameLength > CCHMAXPATH) {
    PRInt32 extIndex = mDefault.RFind(".");
    if (extIndex == kNotFound)
      extIndex = mDefault.Length();

    //Let's try to shave the needed characters from the name part
    PRInt32 charsToRemove = nameLength - CCHMAXPATH;
    if (extIndex - nameIndex >= charsToRemove) {
      mDefault.Cut(extIndex - charsToRemove, charsToRemove);
    }
  }

  //Then, we need to replace illegal characters.
  //Windows has the following statement:
  //At this stage, we cannot replace the backslash as the string might represent a file path.
  //But it is not correct - Windows assumes this is not a path as well,
  //as one of the FILE_ILLEGAL_CHARACTERS is a colon (:)

  mDefault.ReplaceChar("\"", '\'');
  mDefault.ReplaceChar("\<", '(');
  mDefault.ReplaceChar("\>", ')');

  mDefault.ReplaceChar(FILE_ILLEGAL_CHARACTERS, '_');

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(PRUnichar **aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// The default extension to use for files
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(PRUnichar **aExtension)
{
  *aExtension = ToNewUnicode(mDefaultExtension);
  if (!*aExtension)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const PRUnichar *aExtension)
{
  mDefaultExtension = aExtension;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the filter index
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  *aFilterIndex = mSelectedType;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  mSelectedType = aFilterIndex;
  return NS_OK;
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
		  aCharset.Assign(NS_LITERAL_STRING("windows-1252"));
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
    PRInt32 inLength = strlen(inString);
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
  mFilters.AppendString(nsDependentString(aFilter));
  mTitles.AppendString(nsDependentString(aTitle));
  
  return NS_OK;
}

MRESULT EXPENTRY DirDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
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

MRESULT EXPENTRY FileDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  MRESULT mr;
  PFILEDLG pfiledlg;
  HWND hwndTypeCombo;
  HWND hwndFileEF;
  PSZ pszTemp;
  INT i;
  SWP swp;
  PMYDATA pmydata;

  switch ( msg ) {
    case WM_INITDLG:
       /* Create another dropdown that we manage */
       mr = WinDefFileDlgProc(hwndDlg, msg, mp1, mp2);
       hwndTypeCombo = WinWindowFromID(hwndDlg, DID_FILTER_CB);
       WinQueryWindowPos(hwndTypeCombo, &swp);
       WinSetWindowPos(hwndTypeCombo, NULLHANDLE, 0, 0, 0, 0, SWP_HIDE);
       hwndTypeCombo = WinCreateWindow( hwndDlg, WC_COMBOBOX, "",
                                        WS_VISIBLE | WS_PARENTCLIP | WS_SYNCPAINT | WS_TABSTOP | CBS_DROPDOWNLIST,
                                        swp.x, swp.y,
                                        swp.cx, swp.cy, hwndDlg, swp.hwndInsertBehind, 290,
                                        NULL, NULL );
       WinSendMsg( hwndTypeCombo, LM_DELETEALL, (MPARAM)0, (MPARAM)0 );
       pfiledlg = (PFILEDLG)WinQueryWindowULong( hwndDlg, QWL_USER );
       pmydata = (PMYDATA)pfiledlg->ulUser;
       i = 0;
       while (*(pfiledlg->papszITypeList[i]) != NULL) {
           WinSendMsg( hwndTypeCombo, LM_INSERTITEM, (MPARAM)LIT_END, (MPARAM)*(pfiledlg->papszITypeList[i]) );
           i++;
       }
       WinSendMsg( hwndTypeCombo, LM_SELECTITEM, (MPARAM)pmydata->ulCurExt, (MPARAM)TRUE );

       return mr;
    case WM_CONTROL:
       {
         if ((SHORT1FROMMP(mp1) == 290) &&
           (SHORT2FROMMP(mp1) == CBN_LBSELECT)) {
           hwndTypeCombo = WinWindowFromID(hwndDlg, 290);
           pfiledlg = (PFILEDLG)WinQueryWindowULong( hwndDlg, QWL_USER );
           pmydata = (PMYDATA)pfiledlg->ulUser;
           pmydata->ulCurExt = (ULONG)WinSendMsg( hwndTypeCombo, LM_QUERYSELECTION, (MPARAM)LIT_FIRST, (MPARAM)0 );
           if (pfiledlg->fl & FDS_OPEN_DIALOG) {
             WinSetWindowText(WinWindowFromID(hwndDlg,DID_FILENAME_ED), *(pmydata->papszIFilterList[pmydata->ulCurExt]));
             WinSendMsg(WinWindowFromID(hwndDlg,DID_FILENAME_ED), EM_SETSEL, MPFROM2SHORT(0, 32000), (MPARAM)0 );
             WinSendMsg(hwndDlg, WM_CONTROL, MPFROM2SHORT(DID_FILTER_CB, CBN_LBSELECT), (MPARAM)0 );
           }
           return (MRESULT)TRUE;
         }
       }
       break;
  }      
  return WinDefFileDlgProc(hwndDlg, msg, mp1, mp2);
}
