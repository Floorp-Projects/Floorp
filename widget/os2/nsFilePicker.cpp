/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include "nsEnumeratorUtils.h"
#include "nsCRT.h"
#include "nsIWidget.h"
#include "nsOS2Uni.h"
#include "nsFilePicker.h"

#ifndef MAX_PATH
#define MAX_PATH CCHMAXPATH
#endif

/* Item structure */
typedef struct _MyData
{
   PAPSZ    papszIFilterList;
   ULONG    ulCurExt;
   ULONG    ulNumFilters;
}MYDATA, *PMYDATA;

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

char nsFilePicker::mLastUsedDirectory[MAX_PATH+1] = { 0 };


static char* gpszFDSaveCaption = 0;
static char* gpszFDFileExists = 0;
static char* gpszFDFileReadOnly = 0;

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

/* static */ void
nsFilePicker::ReleaseGlobals()
{
  if (gpszFDSaveCaption) {
     free(gpszFDSaveCaption);
     free(gpszFDFileExists);
     free(gpszFDFileReadOnly);
  }
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *retval)
{
  NS_ENSURE_ARG_POINTER(retval);

  bool result = false;
  nsCAutoString fileBuffer;
  char *converted = ConvertToFileSystemCharset(mDefault);
  if (nsnull == converted) {
    LossyCopyUTF16toASCII(mDefault, fileBuffer);
  }
  else {
    fileBuffer.Assign(converted);
    nsMemory::Free( converted );
  }

  char *title = ConvertToFileSystemCharset(mTitle);
  if (nsnull == title)
    title = ToNewCString(mTitle);
  nsCAutoString initialDir;
  if (mDisplayDirectory)
    mDisplayDirectory->GetNativePath(initialDir);
  // If no display directory, re-use the last one.
  if(initialDir.IsEmpty())
    initialDir = mLastUsedDirectory;

  mFile.Truncate();

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
    char* tempptr = strchr(filedlg.szFullFile, '^');
    if (tempptr)
      *tempptr = '\0';
    if (filedlg.lReturn == DID_OK) {
      result = true;
      if (!mDisplayDirectory)
        mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
      if (mDisplayDirectory)
        mDisplayDirectory->InitWithNativePath(nsDependentCString(filedlg.szFullFile));
      mFile.Assign(filedlg.szFullFile);
    }
  }
  else {
    PL_strncpy(filedlg.szFullFile, initialDir.get(), MAX_PATH);
    PL_strncat(filedlg.szFullFile, "\\", 1);
    PL_strncat(filedlg.szFullFile, fileBuffer.get(), MAX_PATH);
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

    PRUint32 i;

    PSZ *apszTypeList;
    apszTypeList = (PSZ *)malloc(mTitles.Length()*sizeof(PSZ)+1);
    for (i = 0; i < mTitles.Length(); i++)
    {
      const nsString& typeWide = mTitles[i];
      nsAutoCharBuffer buffer;
      PRInt32 bufLength;
      WideCharToMultiByte(0, typeWide.get(), typeWide.Length(),
                          buffer, bufLength);
      apszTypeList[i] = ToNewCString(nsDependentCString(buffer.Elements()));
    }
    apszTypeList[i] = 0;
    filedlg.papszITypeList = (PAPSZ)apszTypeList;

    PSZ *apszFilterList;
    apszFilterList = (PSZ *)malloc(mFilters.Length()*sizeof(PSZ)+1);
    for (i = 0; i < mFilters.Length(); i++)
    {
      const nsString& filterWide = mFilters[i];
      apszFilterList[i] = ToNewCString(filterWide);
    }
    apszFilterList[i] = 0;
    pmydata->papszIFilterList = (PAPSZ)apszFilterList;

    pmydata->ulCurExt = mSelectedType;

    bool fileExists = true;
    do {
      DosError(FERR_DISABLEHARDERR);
      WinFileDlg(HWND_DESKTOP, mWnd, &filedlg);
      DosError(FERR_ENABLEHARDERR);
      if ((filedlg.lReturn == DID_OK) && (mMode == modeSave)) {
         PRFileInfo64 fileinfo64;
         PRStatus status = PR_GetFileInfo64(filedlg.szFullFile, &fileinfo64);
         if (status == PR_SUCCESS) {
            fileExists = true;
         } else {
            fileExists = false;
         }
         if (fileExists) {
            if (!gpszFDSaveCaption) {
              HMODULE hmod;
              char LoadError[CCHMAXPATH];
              char loadedString[256];
              int length;
              DosLoadModule(LoadError, CCHMAXPATH, "PMSDMRI", &hmod);
              length = WinLoadString((HAB)0, hmod, 1110, 256, loadedString);
              gpszFDSaveCaption = (char*)malloc(length+1);
              strcpy(gpszFDSaveCaption, loadedString);
              length = WinLoadString((HAB)0, hmod, 1135, 256, loadedString);
              gpszFDFileExists = (char*)malloc(length+1);
              strcpy(gpszFDFileExists, loadedString);
              length = WinLoadString((HAB)0, hmod, 1136, 256, loadedString);
              gpszFDFileReadOnly = (char*)malloc(length+1);
              strcpy(gpszFDFileReadOnly, loadedString);
              int i;
              for (i=0;i<256 && gpszFDFileExists[i];i++ ) {
                if (gpszFDFileExists[i] == '%') {
                  gpszFDFileExists[i+1] = 's';
                  break;
                }
              }
              for (i=0;i<256 && gpszFDFileReadOnly[i];i++ ) {
                if (gpszFDFileReadOnly[i] == '%') {
                  gpszFDFileReadOnly[i+1] = 's';
                  break;
                }
              }
              DosFreeModule(hmod);

            }
            char pszFullText[256+CCHMAXPATH];
            FILESTATUS3 fsts3;
            ULONG ulResponse;
            DosQueryPathInfo( filedlg.szFullFile, FIL_STANDARD, &fsts3, sizeof(FILESTATUS3));
            if (fsts3.attrFile & FILE_READONLY) {
              sprintf(pszFullText, gpszFDFileReadOnly, filedlg.szFullFile);
              ulResponse = WinMessageBox(HWND_DESKTOP, mWnd, pszFullText,
                                               gpszFDSaveCaption, 0,
                                               MB_OK | MB_MOVEABLE | MB_WARNING);
            } else {
              sprintf(pszFullText, gpszFDFileExists, filedlg.szFullFile);
              ulResponse = WinMessageBox(HWND_DESKTOP, mWnd, pszFullText,
                                               gpszFDSaveCaption, 0,
                                               MB_YESNO | MB_MOVEABLE | MB_WARNING);
            }

            if (ulResponse == MBID_YES) {
               fileExists = false;
            }
         }
      }
    } while (mMode == modeSave && fileExists && filedlg.lReturn == DID_OK);

    if (filedlg.lReturn == DID_OK) {
      result = true;
      if (mMode == modeOpenMultiple) {
        nsresult rv;

        if (filedlg.papszFQFilename) {
          for (ULONG i=0;i<filedlg.ulFQFCount;i++) {
            nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
            NS_ENSURE_SUCCESS(rv,rv);

            rv = file->InitWithNativePath(nsDependentCString(*(filedlg.papszFQFilename)[i]));
            NS_ENSURE_SUCCESS(rv,rv);

            rv = mFiles.AppendObject(file);
            NS_ENSURE_SUCCESS(rv,rv);
          }
          WinFreeFileDlgList(filedlg.papszFQFilename);
        } else {
          nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
          NS_ENSURE_SUCCESS(rv,rv);

          rv = file->InitWithNativePath(nsDependentCString(filedlg.szFullFile));
          NS_ENSURE_SUCCESS(rv,rv);

          rv = mFiles.AppendObject(file);
          NS_ENSURE_SUCCESS(rv,rv);
        }
      } else {
        mFile.Assign(filedlg.szFullFile);
      }
      mSelectedType = (PRInt16)pmydata->ulCurExt;
    }

    for (i = 0; i < mTitles.Length(); i++)
    {
      nsMemory::Free(*(filedlg.papszITypeList[i]));
    }
    free(filedlg.papszITypeList);

    for (i = 0; i < mFilters.Length(); i++)
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

    nsresult rv;
    // Remember last used directory.
    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);

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
        if (!mDisplayDirectory)
           mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
        if (mDisplayDirectory)
           mDisplayDirectory->InitWithNativePath( nsDependentCString(mLastUsedDirectory) );
      }
    }

    if (mMode == modeSave) {
      // Windows does not return resultReplace,
      //   we must check if file already exists
      bool exists = false;
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
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIURI **aFileURL)
{
  *aFileURL = nsnull;
  nsCOMPtr<nsILocalFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (!file)
    return rv;

  return NS_NewFileURI(aFileURL, file);
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
NS_IMETHODIMP nsFilePicker::SetDefaultString(const nsAString& aString)
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
  mDefault.ReplaceChar("<", '(');
  mDefault.ReplaceChar(">", ')');

  mDefault.ReplaceChar(FILE_ILLEGAL_CHARACTERS, '_');

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(nsAString& aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// The default extension to use for files
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
  aExtension = mDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
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
void nsFilePicker::InitNative(nsIWidget *aParent,
                              const nsAString& aTitle,
                              PRInt16 aMode)
{
  mWnd = (HWND) ((aParent) ? aParent->GetNativeData(NS_NATIVE_WINDOW) : 0); 
  mTitle.Assign(aTitle);
  mMode = aMode;
}


//-------------------------------------------------------------------------
void nsFilePicker::GetFileSystemCharset(nsCString & fileSystemCharset)
{
  static nsCAutoString aCharset;
  nsresult rv;

  if (aCharset.Length() < 1) {
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
	  if (NS_SUCCEEDED(rv)) 
		  rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");
	  if (NS_FAILED(rv)) 
		  aCharset.AssignLiteral("IBM850");
  }
  fileSystemCharset = aCharset;
}

//-------------------------------------------------------------------------
char * nsFilePicker::ConvertToFileSystemCharset(const nsAString& inString)
{
  char *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (nsnull == mUnicodeEncoder) {
    nsCAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeEncoderRaw(fileSystemCharset.get(), &mUnicodeEncoder);
    }
  }

  // converts from unicode to the file system charset
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = inString.Length();

    const nsAFlatString& flatInString = PromiseFlatString(inString);

    PRInt32 outLength;
    rv = mUnicodeEncoder->GetMaxLength(flatInString.get(), inLength,
                                       &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = static_cast<char*>(nsMemory::Alloc( outLength+1 ));
      if (nsnull == outString) {
        return nsnull;
      }
      rv = mUnicodeEncoder->Convert(flatInString.get(), &inLength, outString,
                                    &outLength);
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
    nsCAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeDecoderRaw(fileSystemCharset.get(), &mUnicodeDecoder);
    }
  }

  // converts from the file system charset to unicode
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeDecoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = static_cast<PRUnichar*>(nsMemory::Alloc( (outLength+1) * sizeof( PRUnichar ) ));
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
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  if (aFilter.EqualsLiteral("..apps"))
    mFilters.AppendElement(NS_LITERAL_STRING("*.exe;*.cmd;*.com;*.bat"));
  else
    mFilters.AppendElement(aFilter);
  mTitles.AppendElement(aTitle);

  return NS_OK;
}

MRESULT EXPENTRY DirDialogProc( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   switch ( msg ) {
      case WM_INITDLG:
         {
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
         HENUM henum;
         HWND hwndNext;
         ULONG ulCurY, ulCurX;
         LONG lScreenX, lScreenY, lDlgFrameX, lDlgFrameY, lTitleBarY;

         lScreenX = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
         lScreenY = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
         lDlgFrameX = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
         lDlgFrameY = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
         lTitleBarY = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

         hwndFileST = WinWindowFromID(hwndDlg, DID_FILENAME_TXT);
         hwndDirST = WinWindowFromID(hwndDlg, DID_DIRECTORY_TXT);
         hwndDirLB = WinWindowFromID(hwndDlg, DID_DIRECTORY_LB);
         hwndDriveST = WinWindowFromID(hwndDlg, DID_DRIVE_TXT);
         hwndDriveCB = WinWindowFromID(hwndDlg, DID_DRIVE_CB);
         hwndOK = WinWindowFromID(hwndDlg, DID_OK);
         hwndCancel = WinWindowFromID(hwndDlg, DID_CANCEL);
         
#define SPACING 10
         // Reposition drives combobox
         ulCurY = SPACING;
         ulCurX = SPACING + lDlgFrameX;
         WinQueryWindowPos(hwndOK, &swpOK);
         WinSetWindowPos(hwndOK, 0, ulCurX, ulCurY, 0, 0, SWP_MOVE);
         ulCurY += swpOK.cy + SPACING;
         WinQueryWindowPos(hwndCancel, &swpCancel);
         WinSetWindowPos(hwndCancel, 0, ulCurX+swpOK.cx+10, SPACING, 0, 0, SWP_MOVE);
         WinQueryWindowPos(hwndDirLB, &swpDirLB);
         WinSetWindowPos(hwndDirLB, 0, ulCurX, ulCurY, swpDirLB.cx, swpDirLB.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpDirLB.cy + SPACING;
         WinQueryWindowPos(hwndDirST, &swpDirST);
         WinSetWindowPos(hwndDirST, 0, ulCurX, ulCurY, swpDirST.cx, swpDirST.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpDirST.cy + SPACING;
         WinQueryWindowPos(hwndDriveCB, &swpDriveCB);
         WinQueryWindowPos(WinWindowFromID(hwndDriveCB, CBID_EDIT), &swpDriveCBEF);
         WinSetWindowPos(hwndDriveCB, 0, ulCurX, ulCurY-(swpDriveCB.cy-swpDriveCBEF.cy)+5,
                                         swpDirLB.cx,
                                         swpDriveCB.cy,
                                         SWP_SIZE | SWP_MOVE);
         ulCurY += swpDriveCBEF.cy + SPACING;
         WinQueryWindowPos(hwndDriveST, &swpDriveST);
         WinSetWindowPos(hwndDriveST, 0, ulCurX, ulCurY, swpDriveST.cx, swpDriveST.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpDriveST.cy + SPACING;
         WinQueryWindowPos(hwndFileST, &swpFileST);
         WinSetWindowPos(hwndFileST, 0, ulCurX, ulCurY, swpFileST.cx, swpFileST.cy, SWP_MOVE | SWP_SIZE);
         ulCurY += swpFileST.cy + SPACING;

         // Hide unused stuff
         henum = WinBeginEnumWindows(hwndDlg);
         while ((hwndNext = WinGetNextWindow(henum)) != NULLHANDLE)
         {
           USHORT usID = WinQueryWindowUShort(hwndNext, QWS_ID);
           if (usID != DID_FILENAME_TXT &&
               usID != DID_DIRECTORY_TXT &&
               usID != DID_DIRECTORY_LB &&
               usID != DID_DRIVE_TXT &&
               usID != DID_DRIVE_CB &&
               usID != DID_OK &&
               usID != DID_CANCEL &&
               usID != FID_TITLEBAR &&
               usID != FID_SYSMENU &&
               usID != FID_MINMAX) 
           {
             WinShowWindow(hwndNext, FALSE);
           }
         }

         WinSetWindowPos(hwndDlg,
                         HWND_TOP,
                         (lScreenX/2)-((swpDirLB.cx+2*SPACING+2*lDlgFrameX)/2),
                         (lScreenY/2)-((ulCurY+2*lDlgFrameY+lTitleBarY)/2),
                         swpDirLB.cx+2*SPACING+2*lDlgFrameX,
                         ulCurY+2*lDlgFrameY+lTitleBarY,
                         SWP_MOVE | SWP_SIZE);
         }
         break;
      case WM_CONTROL:
         {
         PFILEDLG pfiledlg;
         pfiledlg = (PFILEDLG)WinQueryWindowPtr(hwndDlg, QWL_USER);

         HPS           hps;
         SWP           swp;
         HWND          hwndST;
         RECTL         rectlString = {0,0,1000,1000};
         char          *ptr = NULL;
         int           iHalfLen;
         int           iLength;
         CHAR          szString[CCHMAXPATH];

         hwndST = WinWindowFromID(hwndDlg, DID_FILENAME_TXT);
       
         strcpy(szString, pfiledlg->szFullFile);
         iLength = strlen(pfiledlg->szFullFile);
         /* If we are not just a drive */
         if (iLength > 3) {
           if (szString[iLength-1] == '\\') {
             szString[iLength-1] = '\0';
             iLength--;
           }
         }
       
         hps = WinGetPS(hwndST);
         WinQueryWindowPos(hwndST, &swp);
       
         WinDrawText(hps, iLength, szString,
                          &rectlString, 0, 0, 
                          DT_BOTTOM | DT_QUERYEXTENT | DT_TEXTATTRS);
         while(rectlString.xRight > swp.cx)
         {
           iHalfLen = iLength / 2;
           if(iHalfLen == 2)
             break;
       
           ptr = szString + iHalfLen;
           memmove(ptr - 1, ptr, strlen(ptr) + 1);
           szString[iHalfLen - 2] = '.';
           szString[iHalfLen - 1] = '.';
           szString[iHalfLen]     = '.';
           iLength = strlen(szString);
           rectlString.xLeft = rectlString.yBottom = 0;
           rectlString.xRight = rectlString.yTop = 1000;
           WinDrawText(hps, iLength, szString,
                       &rectlString, 0, 0, 
                       DT_BOTTOM | DT_QUERYEXTENT | DT_TEXTATTRS);
         }
       
         WinReleasePS(hps);
         WinSetWindowText(hwndST, szString);
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
