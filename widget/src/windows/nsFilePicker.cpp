/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsWindow.h"
#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIPrivateBrowsingService.h"
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include "nsEnumeratorUtils.h"
#include "nsCRT.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

// commdlg.h and cderr.h are needed to build with WIN32_LEAN_AND_MEAN
#include <commdlg.h>
#include <cderr.h>

#include "nsString.h"
#include "nsToolkit.h"

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

PRUnichar *nsFilePicker::mLastUsedUnicodeDirectory;
char nsFilePicker::mLastUsedDirectory[MAX_PATH+1] = { 0 };

#define MAX_EXTENSION_LENGTH 10

nsFilePicker::nsFilePicker()
{
  mSelectedType   = 1;
}

nsFilePicker::~nsFilePicker()
{
  if (mLastUsedUnicodeDirectory) {
    NS_Free(mLastUsedUnicodeDirectory);
    mLastUsedUnicodeDirectory = nsnull;
  }
}

// Show - Display the file dialog
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
  if (uMsg == BFFM_INITIALIZED)
  {
    PRUnichar * filePath = (PRUnichar *) lpData;
    if (filePath)
      ::SendMessageW(hwnd, BFFM_SETSELECTIONW,
                     TRUE /* true because lpData is a path string */,
                     lpData);
  }
  return 0;
}

static void EnsureWindowVisible(HWND hwnd) 
{
  // Obtain the monitor which has the largest area of intersection 
  // with the window, or NULL if there is no intersection.
  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
  if (!monitor) {
    // The window is not visible, we should reposition it to the same place as its parent
    HWND parentHwnd = GetParent(hwnd);
    RECT parentRect;
    GetWindowRect(parentHwnd, &parentRect);
    BOOL b = SetWindowPos(hwnd, NULL, 
                          parentRect.left, 
                          parentRect.top, 0, 0, 
                          SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
  }
}

// Callback hook which will ensure that the window is visible
static UINT_PTR CALLBACK FilePickerHook(HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam) 
{
  if (msg == WM_NOTIFY) {
    LPOFNOTIFYW lpofn = (LPOFNOTIFYW) lParam;
    if (!lpofn || !lpofn->lpOFN) {
      return 0;
    }
    
    if (CDN_INITDONE == lpofn->hdr.code) {
      // The Window will be automatically moved to the last position after
      // CDN_INITDONE.  We post a message to ensure the window will be visible
      // so it will be done after the automatic last position window move.
      PostMessage(hwnd, MOZ_WM_ENSUREVISIBLE, 0, 0);
    }
  } else if (msg == MOZ_WM_ENSUREVISIBLE) {
    EnsureWindowVisible(GetParent(hwnd));
  }
  return 0;
}


// Callback hook which will dynamically allocate a buffer large
// enough for the file picker dialog.
static UINT_PTR CALLBACK MultiFilePickerHook(HWND hwnd, UINT msg,
                                             WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    case WM_INITDIALOG:
      {
        // Finds the child drop down of a File Picker dialog and sets the 
        // maximum amount of text it can hold when typed in manually.
        // A wParam of 0 mean 0x7FFFFFFE characters.
        HWND comboBox = FindWindowEx(GetParent(hwnd), NULL, 
                                     L"ComboBoxEx32", NULL );
        if(comboBox)
          SendMessage(comboBox, CB_LIMITTEXT, 0, 0);
      }
      break;
    case WM_NOTIFY:
      {
        LPOFNOTIFYW lpofn = (LPOFNOTIFYW) lParam;
        if (!lpofn || !lpofn->lpOFN) {
          return 0;
        }
        // CDN_SELCHANGE is sent when the selection in the list box of the file
        // selection dialog changes
        if (lpofn->hdr.code == CDN_SELCHANGE) {
          HWND parentHWND = GetParent(hwnd);

          // Get the required size for the selected files buffer
          UINT newBufLength = 0; 
          int requiredBufLength = CommDlg_OpenSave_GetSpecW(parentHWND, 
                                                            NULL, 0);
          if(requiredBufLength >= 0)
            newBufLength += requiredBufLength;
          else
            newBufLength += MAX_PATH;

          // If the user selects multiple files, the buffer contains the 
          // current directory followed by the file names of the selected 
          // files. So make room for the directory path.  If the user
          // selects a single file, it is no harm to add extra space.
          requiredBufLength = CommDlg_OpenSave_GetFolderPathW(parentHWND, 
                                                              NULL, 0);
          if(requiredBufLength >= 0)
            newBufLength += requiredBufLength;
          else
            newBufLength += MAX_PATH;

          // Check if lpstrFile and nMaxFile are large enough
          if (newBufLength > lpofn->lpOFN->nMaxFile) {
            if (lpofn->lpOFN->lpstrFile)
              delete[] lpofn->lpOFN->lpstrFile;

            // We allocate FILE_BUFFER_SIZE more bytes than is needed so that
            // if the user selects a file and holds down shift and down to 
            // select  additional items, we will not continuously reallocate
            newBufLength += FILE_BUFFER_SIZE;

            PRUnichar* filesBuffer = new PRUnichar[newBufLength];
            ZeroMemory(filesBuffer, newBufLength * sizeof(PRUnichar));

            lpofn->lpOFN->lpstrFile = filesBuffer;
            lpofn->lpOFN->nMaxFile  = newBufLength;
          }
        }
      }
      break;
  }

  return FilePickerHook(hwnd, msg, wParam, lParam);
}

NS_IMETHODIMP nsFilePicker::ShowW(PRInt16 *aReturnVal)
{
  NS_ENSURE_ARG_POINTER(aReturnVal);

  // suppress blur events
  if (mParentWidget) {
    nsIWidget *tmp = mParentWidget;
    nsWindow *parent = static_cast<nsWindow *>(tmp);
    parent->SuppressBlurEvents(PR_TRUE);
  }

  bool result = false;
  nsAutoArrayPtr<PRUnichar> fileBuffer(new PRUnichar[FILE_BUFFER_SIZE+1]);
            
  wcsncpy(fileBuffer,  mDefault.get(), FILE_BUFFER_SIZE);
  fileBuffer[FILE_BUFFER_SIZE] = '\0'; // null terminate in case copy truncated

  NS_NAMED_LITERAL_STRING(htmExt, "html");
  nsAutoString initialDir;
  if (mDisplayDirectory)
    mDisplayDirectory->GetPath(initialDir);

  // If no display directory, re-use the last one.
  if(initialDir.IsEmpty()) {
    // Allocate copy of last used dir.
    initialDir = mLastUsedUnicodeDirectory;
  }

  mUnicodeFile.Truncate();

  if (mMode == modeGetFolder) {
    PRUnichar dirBuffer[MAX_PATH+1];
    wcsncpy(dirBuffer, initialDir.get(), MAX_PATH);

    BROWSEINFOW browserInfo;
    browserInfo.hwndOwner      = (HWND)
      (mParentWidget.get() ? mParentWidget->GetNativeData(NS_NATIVE_TMP_WINDOW) : 0); 
    browserInfo.pidlRoot       = nsnull;
    browserInfo.pszDisplayName = (LPWSTR)dirBuffer;
    browserInfo.lpszTitle      = mTitle.get();
    browserInfo.ulFlags        = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
    if (initialDir.Length()) {
      // the dialog is modal so that |initialDir.get()| will be valid in 
      // BrowserCallbackProc. Thus, we don't need to clone it.
      browserInfo.lParam       = (LPARAM) initialDir.get();
      browserInfo.lpfn         = &BrowseCallbackProc;
    } else {
    browserInfo.lParam         = nsnull;
      browserInfo.lpfn         = nsnull;
    }
    browserInfo.iImage         = nsnull;

    LPITEMIDLIST list = ::SHBrowseForFolderW(&browserInfo);
    if (list != NULL) {
      result = ::SHGetPathFromIDListW(list, (LPWSTR)fileBuffer);
      if (result) {
          mUnicodeFile.Assign(fileBuffer);
      }
  
      // free PIDL
      CoTaskMemFree(list);
    }
  } else {

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    nsString filterBuffer = mFilterList;
                                  
    if (!initialDir.IsEmpty()) {
      ofn.lpstrInitialDir = initialDir.get();
    }
    
    ofn.lpstrTitle   = (LPCWSTR)mTitle.get();
    ofn.lpstrFilter  = (LPCWSTR)filterBuffer.get();
    ofn.nFilterIndex = mSelectedType;
    ofn.hwndOwner    = (HWND) (mParentWidget.get() ? mParentWidget->GetNativeData(NS_NATIVE_TMP_WINDOW) : 0); 
    ofn.lpstrFile    = fileBuffer;
    ofn.nMaxFile     = FILE_BUFFER_SIZE;
    ofn.Flags = OFN_SHAREAWARE | OFN_LONGNAMES | OFN_OVERWRITEPROMPT |
                OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_ENABLESIZING | 
                OFN_EXPLORER;

    // Windows Vista and up won't allow you to use the new looking dialogs with
    // a hook procedure.  The hook procedure fixes a problem on XP dialogs for
    // file picker visibility.  Vista and up automatically ensures the file 
    // picker is always visible.
    if (nsWindow::GetWindowsVersion() < VISTA_VERSION) {
      ofn.lpfnHook = FilePickerHook;
      ofn.Flags |= OFN_ENABLEHOOK;
    }

    // Handle add to recent docs settings
    nsCOMPtr<nsIPrivateBrowsingService> pbs =
      do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    bool privacyModeEnabled = false;
    if (pbs) {
      pbs->GetPrivateBrowsingEnabled(&privacyModeEnabled);
    }
    if (privacyModeEnabled || !mAddToRecentDocs) {
      ofn.Flags |= OFN_DONTADDTORECENT;
    }

    if (!mDefaultExtension.IsEmpty()) {
      ofn.lpstrDefExt = mDefaultExtension.get();
    }
    else {
      // Get file extension from suggested filename
      //  to detect if we are saving an html file
      //XXX: nsIFile SHOULD HAVE A GetExtension() METHOD!
      PRInt32 extIndex = mDefault.RFind(".");
      if ( extIndex >= 0) {
        nsAutoString ext;
        mDefault.Right(ext, mDefault.Length() - extIndex);
        // Should we test for ".cgi", ".asp", ".jsp" and other
        // "generated" html pages?

        if ( ext.LowerCaseEqualsLiteral(".htm")  ||
             ext.LowerCaseEqualsLiteral(".html") ||
             ext.LowerCaseEqualsLiteral(".shtml") ) {
          // This is supposed to append ".htm" if user doesn't supply an extension
          //XXX Actually, behavior is sort of weird:
          //    often appends ".html" even if you have an extension
          //    It obeys your extension if you put quotes around name
          ofn.lpstrDefExt = htmExt.get();
        }
      }
    }

    // When possible, instead of using OFN_NOCHANGEDIR to ensure the current
    // working directory will not change from this call, we will retrieve the
    // current working directory before the call and restore it after the 
    // call.  This flag causes problems on Windows XP for paths that are
    // selected like  C:test.txt where the user is currently at C:\somepath
    // In which case expected result should be C:\somepath\test.txt
    AutoRestoreWorkingPath restoreWorkingPath;
    // If we can't get the current working directory, the best case is to
    // use the OFN_NOCHANGEDIR flag
    if (!restoreWorkingPath.HasWorkingPath()) {
      ofn.Flags |= OFN_NOCHANGEDIR;
    }
    
    MOZ_SEH_TRY {
      if (mMode == modeOpen) {
        // FILE MUST EXIST!
        ofn.Flags |= OFN_FILEMUSTEXIST;
        result = ::GetOpenFileNameW(&ofn);
      }
      else if (mMode == modeOpenMultiple) {
        ofn.Flags |= OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

        // The hook set here ensures that the buffer returned will always be
        // large enough to hold all selected files.  The hook may modify the
        // value of ofn.lpstrFile and deallocate the old buffer that it pointed
        // to (fileBuffer). The hook assumes that the passed in value is heap 
        // allocated and that the returned value should be freed by the caller.
        // If the hook changes the buffer, it will deallocate the old buffer.
        // This fix would be nice to have in Vista and up, but it would force
        // the file picker to use the old style dialogs because hooks are not
        // allowed in the new file picker UI.  We need to eventually move to
        // the new Common File Dialogs for Vista and up.
        if (nsWindow::GetWindowsVersion() < VISTA_VERSION) {
          ofn.lpfnHook = MultiFilePickerHook;
          fileBuffer.forget();
          result = ::GetOpenFileNameW(&ofn);
          fileBuffer = ofn.lpstrFile;
        }
        else {
          result = ::GetOpenFileNameW(&ofn);
        }
      }
      else if (mMode == modeSave) {
        ofn.Flags |= OFN_NOREADONLYRETURN;

        // Don't follow shortcuts when saving a shortcut, this can be used
        // to trick users (bug 271732)
        NS_ConvertUTF16toUTF8 ext(mDefault);
        ext.Trim(" .", PR_FALSE, PR_TRUE); // watch out for trailing space and dots
        ToLowerCase(ext);
        if (StringEndsWith(ext, NS_LITERAL_CSTRING(".lnk")) ||
            StringEndsWith(ext, NS_LITERAL_CSTRING(".pif")) ||
            StringEndsWith(ext, NS_LITERAL_CSTRING(".url")))
          ofn.Flags |= OFN_NODEREFERENCELINKS;

        result = ::GetSaveFileNameW(&ofn);
        if (!result) {
          // Error, find out what kind.
          if (::GetLastError() == ERROR_INVALID_PARAMETER 
              || ::CommDlgExtendedError() == FNERR_INVALIDFILENAME
              ) {
            // probably the default file name is too long or contains illegal characters!
            // Try again, without a starting file name.
            ofn.lpstrFile[0] = 0;
            result = ::GetSaveFileNameW(&ofn);
          }
        }
      } 
      else {
        NS_ERROR("unsupported mode"); 
      }
    }
    MOZ_SEH_EXCEPT(PR_TRUE) {
      MessageBoxW(ofn.hwndOwner,
                  0,
                  L"The filepicker was unexpectedly closed by Windows.",
                  MB_ICONERROR);
      result = PR_FALSE;
    }

    if (result) {
      // Remember what filter type the user selected
      mSelectedType = (PRInt16)ofn.nFilterIndex;

      // Clear out any files from previous Show calls
      mFiles.Clear();

      // Set user-selected location of file or directory
      if (mMode == modeOpenMultiple) {
        
        // from msdn.microsoft.com, "Open and Save As Dialog Boxes" section:
        // If you specify OFN_EXPLORER,
        // The directory and file name strings are NULL separated, 
        // with an extra NULL character after the last file name. 
        // This format enables the Explorer-style dialog boxes
        // to return long file names that include spaces. 
        PRUnichar *current = fileBuffer;
        
        nsAutoString dirName(current);
        // sometimes dirName contains a trailing slash
        // and sometimes it doesn't.
        if (current[dirName.Length() - 1] != '\\')
          dirName.Append((PRUnichar)'\\');
        
        nsresult rv;
        while (current && *current && *(current + nsCRT::strlen(current) + 1)) {
          current = current + nsCRT::strlen(current) + 1;
          
          nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
          NS_ENSURE_SUCCESS(rv,rv);

          // Only prepend the directory if the path specified is a relative path
          nsAutoString path;
          if (PathIsRelativeW(current)) {
            path = dirName + nsDependentString(current);
          } else {
            path = current;
          }

          nsAutoString canonicalizedPath;
          GetQualifiedPath(path.get(), canonicalizedPath);
          
          rv = file->InitWithPath(canonicalizedPath);
          NS_ENSURE_SUCCESS(rv,rv);
          
          rv = mFiles.AppendObject(file);
          NS_ENSURE_SUCCESS(rv,rv);
        }
        
        // handle the case where the user selected just one
        // file.  according to msdn.microsoft.com:
        // If you specify OFN_ALLOWMULTISELECT and the user selects 
        // only one file, the lpstrFile string does not have 
        // a separator between the path and file name.
        if (current && *current && (current == fileBuffer)) {
          nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
          NS_ENSURE_SUCCESS(rv,rv);
          
          nsAutoString canonicalizedPath;
          GetQualifiedPath(current, canonicalizedPath);
          rv = file->InitWithPath(canonicalizedPath);
          NS_ENSURE_SUCCESS(rv,rv);
          
          rv = mFiles.AppendObject(file);
          NS_ENSURE_SUCCESS(rv,rv);
        }
      } else {
        GetQualifiedPath(fileBuffer, mUnicodeFile);
      }
    }
    if (ofn.hwndOwner) {
      ::DestroyWindow(ofn.hwndOwner);
    }
  }

  if (result) {
    PRInt16 returnOKorReplace = returnOK;

    // Remember last used directory.
    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    // XXX  InitWithPath() will convert UCS2 to FS path !!!  corrupts unicode 
    file->InitWithPath(mUnicodeFile);
    nsCOMPtr<nsIFile> dir;
    if (NS_SUCCEEDED(file->GetParent(getter_AddRefs(dir)))) {
      mDisplayDirectory = do_QueryInterface(dir);
      if (mDisplayDirectory) {
        if (mLastUsedUnicodeDirectory) {
          NS_Free(mLastUsedUnicodeDirectory);
          mLastUsedUnicodeDirectory = nsnull;
        }

        nsAutoString newDir;
        mDisplayDirectory->GetPath(newDir);
        if(!newDir.IsEmpty())
          mLastUsedUnicodeDirectory = ToNewUnicode(newDir);
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
    *aReturnVal = returnOKorReplace;
  }
  else {
    *aReturnVal = returnCancel;
  }
  if (mParentWidget) {
    nsIWidget *tmp = mParentWidget;
    nsWindow *parent = static_cast<nsWindow *>(tmp);
    parent->SuppressBlurEvents(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::Show(PRInt16 *aReturnVal)
{
  return ShowW(aReturnVal);
}

NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nsnull;

  if (mUnicodeFile.IsEmpty())
      return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(mUnicodeFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

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

// Get the file + path
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
  
  if (nameLength > MAX_PATH) {
    PRInt32 extIndex = mDefault.RFind(".");
    if (extIndex == kNotFound)
      extIndex = mDefault.Length();

    //Let's try to shave the needed characters from the name part
    PRInt32 charsToRemove = nameLength - MAX_PATH;
    if (extIndex - nameIndex >= charsToRemove) {
      mDefault.Cut(extIndex - charsToRemove, charsToRemove);
    }
  }

  //Then, we need to replace illegal characters.
  //At this stage, we cannot replace the backslash as the string might represent a file path.
  mDefault.ReplaceChar(FILE_ILLEGAL_CHARACTERS, '-');

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(nsAString& aString)
{
  return NS_ERROR_FAILURE;
}

// The default extension to use for files
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

// Set the filter index
NS_IMETHODIMP nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  // Windows' filter index is 1-based, we use a 0-based system.
  *aFilterIndex = mSelectedType - 1;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  // Windows' filter index is 1-based, we use a 0-based system.
  mSelectedType = aFilterIndex + 1;
  return NS_OK;
}

void nsFilePicker::InitNative(nsIWidget *aParent,
                              const nsAString& aTitle,
                              PRInt16 aMode)
{
  mParentWidget = aParent;
  mTitle.Assign(aTitle);
  mMode = aMode;
}

void 
nsFilePicker::GetQualifiedPath(const PRUnichar *aInPath, nsString &aOutPath)
{
  // Prefer a qualified path over a non qualified path.
  // Things like c:file.txt would be accepted in Win XP but would later
  // fail to open from the download manager.
  PRUnichar qualifiedFileBuffer[MAX_PATH];
  if (PathSearchAndQualifyW(aInPath, qualifiedFileBuffer, MAX_PATH)) {
    aOutPath.Assign(qualifiedFileBuffer);
  } else {
    aOutPath.Assign(aInPath);
  }
}

NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  mFilterList.Append(aTitle);
  mFilterList.Append(PRUnichar('\0'));

  if (aFilter.EqualsLiteral("..apps"))
    mFilterList.AppendLiteral("*.exe;*.com");
  else
  {
    nsAutoString filter(aFilter);
    filter.StripWhitespace();
    if (filter.EqualsLiteral("*"))
      filter.AppendLiteral(".*");
    mFilterList.Append(filter);
  }

  mFilterList.Append(PRUnichar('\0'));

  return NS_OK;
}

AutoRestoreWorkingPath::AutoRestoreWorkingPath() 
{
  DWORD bufferLength = GetCurrentDirectoryW(0, NULL);
  mWorkingPath = new PRUnichar[bufferLength];
  if (GetCurrentDirectoryW(bufferLength, mWorkingPath) == 0) {
    mWorkingPath = NULL;
  }
}

AutoRestoreWorkingPath::~AutoRestoreWorkingPath()
{
  if (HasWorkingPath()) {
    ::SetCurrentDirectoryW(mWorkingPath);
  }
}

