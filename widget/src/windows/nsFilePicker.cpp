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
 *   Jim Mathies <jmathies@mozilla.com>
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
#include <cderr.h>

#include "nsString.h"
#include "nsToolkit.h"

PRUnichar *nsFilePicker::mLastUsedUnicodeDirectory;
char nsFilePicker::mLastUsedDirectory[MAX_PATH+1] = { 0 };

#define MAX_EXTENSION_LENGTH 10

///////////////////////////////////////////////////////////////////////////////
// Helper classes

// Manages matching SuppressBlurEvents calls on the parent widget.
class AutoSuppressEvents
{
public:
  explicit AutoSuppressEvents(nsIWidget* aWidget) :
    mWindow(static_cast<nsWindow *>(aWidget)) {
    SuppressWidgetEvents(true);
  }

  ~AutoSuppressEvents() {
    SuppressWidgetEvents(false);
  }
private:
  void SuppressWidgetEvents(bool aFlag) {
    if (mWindow) {
      mWindow->SuppressBlurEvents(aFlag);
    }
  }
  nsRefPtr<nsWindow> mWindow;
};

// Manages the current working path.
class AutoRestoreWorkingPath
{
public:
  AutoRestoreWorkingPath() {
    DWORD bufferLength = GetCurrentDirectoryW(0, NULL);
    mWorkingPath = new PRUnichar[bufferLength];
    if (GetCurrentDirectoryW(bufferLength, mWorkingPath) == 0) {
      mWorkingPath = NULL;
    }
  }

  ~AutoRestoreWorkingPath() {
    if (HasWorkingPath()) {
      ::SetCurrentDirectoryW(mWorkingPath);
    }
  }

  inline bool HasWorkingPath() const {
    return mWorkingPath != NULL;
  }
private:
  nsAutoArrayPtr<PRUnichar> mWorkingPath;
};

// Manages NS_NATIVE_TMP_WINDOW child windows. NS_NATIVE_TMP_WINDOWs are
// temporary child windows of mParentWidget created to address RTL issues
// in picker dialogs. We are responsible for destroying these.
class AutoDestroyTmpWindow
{
public:
  explicit AutoDestroyTmpWindow(HWND aTmpWnd) :
    mWnd(aTmpWnd) {
  }

  ~AutoDestroyTmpWindow() {
    if (mWnd)
      DestroyWindow(mWnd);
  }
  
  inline HWND get() const { return mWnd; }
private:
  HWND mWnd;
};

// Manages matching PickerOpen/PickerClosed calls on the parent widget.
class AutoWidgetPickerState
{
public:
  explicit AutoWidgetPickerState(nsIWidget* aWidget) :
    mWindow(static_cast<nsWindow *>(aWidget)) {
    PickerState(true);
  }

  ~AutoWidgetPickerState() {
    PickerState(false);
  }
private:
  void PickerState(bool aFlag) {
    if (mWindow) {
      if (aFlag)
        mWindow->PickerOpen();
      else
        mWindow->PickerClosed();
    }
  }
  nsRefPtr<nsWindow> mWindow;
};

///////////////////////////////////////////////////////////////////////////////
// nsIFilePicker

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

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
int CALLBACK
BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
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

static void
EnsureWindowVisible(HWND hwnd) 
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

// Callback hook which will ensure that the window is visible. Currently
// only in use on os <= XP.
static UINT_PTR CALLBACK
FilePickerHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
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


// Callback hook which will dynamically allocate a buffer large enough
// for the file picker dialog.  Currently only in use on  os <= XP.
static UINT_PTR CALLBACK
MultiFilePickerHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

bool
nsFilePicker::ShowFolderPicker(const nsString& aInitialDir)
{
  bool result = false;

  nsAutoArrayPtr<PRUnichar> dirBuffer(new PRUnichar[FILE_BUFFER_SIZE]);
  wcsncpy(dirBuffer, aInitialDir.get(), FILE_BUFFER_SIZE);
  dirBuffer[FILE_BUFFER_SIZE-1] = '\0';

  AutoDestroyTmpWindow adtw((HWND)(mParentWidget.get() ?
    mParentWidget->GetNativeData(NS_NATIVE_TMP_WINDOW) : NULL));

  BROWSEINFOW browserInfo = {0};
  browserInfo.pidlRoot       = nsnull;
  browserInfo.pszDisplayName = (LPWSTR)dirBuffer;
  browserInfo.lpszTitle      = mTitle.get();
  browserInfo.ulFlags        = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
  browserInfo.hwndOwner      = adtw.get(); 
  browserInfo.iImage         = nsnull;

  if (!aInitialDir.IsEmpty()) {
    // the dialog is modal so that |initialDir.get()| will be valid in 
    // BrowserCallbackProc. Thus, we don't need to clone it.
    browserInfo.lParam = (LPARAM) aInitialDir.get();
    browserInfo.lpfn   = &BrowseCallbackProc;
  } else {
    browserInfo.lParam = nsnull;
    browserInfo.lpfn   = nsnull;
  }

  LPITEMIDLIST list = ::SHBrowseForFolderW(&browserInfo);
  if (list) {
    result = ::SHGetPathFromIDListW(list, (LPWSTR)dirBuffer);
    if (result)
      mUnicodeFile.Assign(dirBuffer);
    // free PIDL
    CoTaskMemFree(list);
  }

  return result;
}

bool
nsFilePicker::FilePickerWrapper(OPENFILENAMEW* ofn, PickerType aType)
{
  if (!ofn)
    return false;

  bool result = false;
  AutoWidgetPickerState awps(mParentWidget);
  MOZ_SEH_TRY {
    if (aType == PICKER_TYPE_OPEN) 
      result = ::GetOpenFileNameW(ofn);
    else if (aType == PICKER_TYPE_SAVE)
      result = ::GetSaveFileNameW(ofn);
  } MOZ_SEH_EXCEPT(true) {
    NS_ERROR("nsFilePicker GetFileName win32 call generated an exception! This is bad!");
  }
  return result;
}

bool
nsFilePicker::ShowFilePicker(const nsString& aInitialDir)
{
  OPENFILENAMEW ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  nsString filterBuffer = mFilterList;
                                
  nsAutoArrayPtr<PRUnichar> fileBuffer(new PRUnichar[FILE_BUFFER_SIZE]);
  wcsncpy(fileBuffer,  mDefault.get(), FILE_BUFFER_SIZE);
  fileBuffer[FILE_BUFFER_SIZE-1] = '\0'; // null terminate in case copy truncated

  if (!aInitialDir.IsEmpty()) {
    ofn.lpstrInitialDir = aInitialDir.get();
  }

  AutoDestroyTmpWindow adtw((HWND) (mParentWidget.get() ?
    mParentWidget->GetNativeData(NS_NATIVE_TMP_WINDOW) : NULL));

  ofn.lpstrTitle   = (LPCWSTR)mTitle.get();
  ofn.lpstrFilter  = (LPCWSTR)filterBuffer.get();
  ofn.nFilterIndex = mSelectedType;
  ofn.lpstrFile    = fileBuffer;
  ofn.nMaxFile     = FILE_BUFFER_SIZE;
  ofn.hwndOwner    = adtw.get();
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
  if (IsPrivacyModeEnabled() || !mAddToRecentDocs) {
    ofn.Flags |= OFN_DONTADDTORECENT;
  }

  NS_NAMED_LITERAL_STRING(htmExt, "html");

  if (!mDefaultExtension.IsEmpty()) {
    ofn.lpstrDefExt = mDefaultExtension.get();
  } else {
    // Get file extension from suggested filename to detect if we are
    // saving an html file.
    if (IsDefaultPathHtml()) {
      // This is supposed to append ".htm" if user doesn't supply an
      // extension but the behavior is sort of weird:
      // - Often appends ".html" even if you have an extension
      // - It obeys your extension if you put quotes around name
      ofn.lpstrDefExt = htmExt.get();
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

  bool result = false;

  switch(mMode) {
    case modeOpen:
      // FILE MUST EXIST!
      ofn.Flags |= OFN_FILEMUSTEXIST;
      result = FilePickerWrapper(&ofn, PICKER_TYPE_OPEN);
      break;

    case modeOpenMultiple:
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
        result = FilePickerWrapper(&ofn, PICKER_TYPE_OPEN);
        fileBuffer = ofn.lpstrFile;
      } else {
        result = FilePickerWrapper(&ofn, PICKER_TYPE_OPEN);
      }
      break;

    case modeSave:
      {
        ofn.Flags |= OFN_NOREADONLYRETURN;

        // Don't follow shortcuts when saving a shortcut, this can be used
        // to trick users (bug 271732)
        if (IsDefaultPathLink())
          ofn.Flags |= OFN_NODEREFERENCELINKS;

        result = FilePickerWrapper(&ofn, PICKER_TYPE_SAVE);
        if (!result) {
          // Error, find out what kind.
          if (GetLastError() == ERROR_INVALID_PARAMETER ||
              CommDlgExtendedError() == FNERR_INVALIDFILENAME) {
            // Probably the default file name is too long or contains illegal
            // characters. Try again, without a starting file name.
            ofn.lpstrFile[0] = nsnull;
            result = FilePickerWrapper(&ofn, PICKER_TYPE_SAVE);
          }
        }
      }
      break;

    default:
      NS_ERROR("unsupported file picker mode");
      return false;
  }

  if (!result)
    return false;

  // Remember what filter type the user selected
  mSelectedType = (PRInt16)ofn.nFilterIndex;

  // Single file selection, we're done
  if (mMode != modeOpenMultiple) {
    GetQualifiedPath(fileBuffer, mUnicodeFile);
    return true;
  }

  // Clear previous file selection list
  mFiles.Clear();

  // Set user-selected location of file or directory.  From msdn's "Open and
  // Save As Dialog Boxes" section:
  // If you specify OFN_EXPLORER, the directory and file name strings are NULL
  // separated, with an extra NULL character after the last file name. This
  // format enables the Explorer-style dialog boxes to return long file names
  // that include spaces. 
  PRUnichar *current = fileBuffer;
  
  nsAutoString dirName(current);
  // Sometimes dirName contains a trailing slash and sometimes it doesn't:
  if (current[dirName.Length() - 1] != '\\')
    dirName.Append((PRUnichar)'\\');
  
  while (current && *current && *(current + nsCRT::strlen(current) + 1)) {
    current = current + nsCRT::strlen(current) + 1;
    
    nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1");
    NS_ENSURE_TRUE(file, false);

    // Only prepend the directory if the path specified is a relative path
    nsAutoString path;
    if (PathIsRelativeW(current)) {
      path = dirName + nsDependentString(current);
    } else {
      path = current;
    }

    nsAutoString canonicalizedPath;
    GetQualifiedPath(path.get(), canonicalizedPath);
    if (NS_FAILED(file->InitWithPath(canonicalizedPath)) ||
        !mFiles.AppendObject(file))
      return false;
  }
  
  // Handle the case where the user selected just one file. From msdn: If you
  // specify OFN_ALLOWMULTISELECT and the user selects only one file the
  // lpstrFile string does not have a separator between the path and file name.
  if (current && *current && (current == fileBuffer)) {
    nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1");
    NS_ENSURE_TRUE(file, false);
    
    nsAutoString canonicalizedPath;
    GetQualifiedPath(current, canonicalizedPath);
    if (NS_FAILED(file->InitWithPath(canonicalizedPath)) ||
        !mFiles.AppendObject(file))
      return false;
  }

  return true;
}

NS_IMETHODIMP
nsFilePicker::ShowW(PRInt16 *aReturnVal)
{
  NS_ENSURE_ARG_POINTER(aReturnVal);

  *aReturnVal = returnCancel;

  AutoSuppressEvents supress(mParentWidget);

  nsAutoString initialDir;
  if (mDisplayDirectory)
    mDisplayDirectory->GetPath(initialDir);

  // If no display directory, re-use the last one.
  if(initialDir.IsEmpty()) {
    // Allocate copy of last used dir.
    initialDir = mLastUsedUnicodeDirectory;
  }

  mUnicodeFile.Truncate();

  bool result = false;
  if (mMode == modeGetFolder) {
    result = ShowFolderPicker(initialDir);
  } else {
    result = ShowFilePicker(initialDir);
  }

  // exit, and return returnCancel in aReturnVal
  if (!result)
    return NS_OK;

  RememberLastUsedDirectory();

  PRInt16 retValue = returnOK;
  if (mMode == modeSave) {
    // Windows does not return resultReplace, we must check if file
    // already exists.
    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    bool flag = false;
    if (file && NS_SUCCEEDED(file->InitWithPath(mUnicodeFile)) &&
        NS_SUCCEEDED(file->Exists(&flag)) && flag) {
      retValue = returnReplace;
    }
  }

  *aReturnVal = retValue;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::Show(PRInt16 *aReturnVal)
{
  return ShowW(aReturnVal);
}

NS_IMETHODIMP
nsFilePicker::GetFile(nsILocalFile **aFile)
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

NS_IMETHODIMP
nsFilePicker::GetFileURL(nsIURI **aFileURL)
{
  *aFileURL = nsnull;
  nsCOMPtr<nsILocalFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (!file)
    return rv;

  return NS_NewFileURI(aFileURL, file);
}

NS_IMETHODIMP
nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  NS_ENSURE_ARG_POINTER(aFiles);
  return NS_NewArrayEnumerator(aFiles, mFiles);
}

// Get the file + path
NS_IMETHODIMP
nsFilePicker::SetDefaultString(const nsAString& aString)
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

NS_IMETHODIMP
nsFilePicker::GetDefaultString(nsAString& aString)
{
  return NS_ERROR_FAILURE;
}

// The default extension to use for files
NS_IMETHODIMP
nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
  aExtension = mDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
{
  mDefaultExtension = aExtension;
  return NS_OK;
}

// Set the filter index
NS_IMETHODIMP
nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  // Windows' filter index is 1-based, we use a 0-based system.
  *aFilterIndex = mSelectedType - 1;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  // Windows' filter index is 1-based, we use a 0-based system.
  mSelectedType = aFilterIndex + 1;
  return NS_OK;
}

void
nsFilePicker::InitNative(nsIWidget *aParent,
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

void
nsFilePicker::RememberLastUsedDirectory()
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  if (!file || NS_FAILED(file->InitWithPath(mUnicodeFile))) {
    NS_WARNING("RememberLastUsedDirectory failed to init file path.");
    return;
  }

  nsCOMPtr<nsIFile> dir;
  nsAutoString newDir;
  if (NS_FAILED(file->GetParent(getter_AddRefs(dir))) ||
      !(mDisplayDirectory = do_QueryInterface(dir)) ||
      NS_FAILED(mDisplayDirectory->GetPath(newDir)) ||
      newDir.IsEmpty()) {
    NS_WARNING("RememberLastUsedDirectory failed to get parent directory.");
    return;
  }

  if (mLastUsedUnicodeDirectory) {
    NS_Free(mLastUsedUnicodeDirectory);
    mLastUsedUnicodeDirectory = nsnull;
  }
  mLastUsedUnicodeDirectory = ToNewUnicode(newDir);
}

bool
nsFilePicker::IsPrivacyModeEnabled()
{
  // Handle add to recent docs settings
  nsCOMPtr<nsIPrivateBrowsingService> pbs =
    do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
  bool privacyModeEnabled = false;
  if (pbs) {
    pbs->GetPrivateBrowsingEnabled(&privacyModeEnabled);
  }
  return privacyModeEnabled;
}

bool
nsFilePicker::IsDefaultPathLink()
{
  NS_ConvertUTF16toUTF8 ext(mDefault);
  ext.Trim(" .", false, true); // watch out for trailing space and dots
  ToLowerCase(ext);
  if (StringEndsWith(ext, NS_LITERAL_CSTRING(".lnk")) ||
      StringEndsWith(ext, NS_LITERAL_CSTRING(".pif")) ||
      StringEndsWith(ext, NS_LITERAL_CSTRING(".url")))
    return true;
  return false;
}

bool
nsFilePicker::IsDefaultPathHtml()
{
  PRInt32 extIndex = mDefault.RFind(".");
  if (extIndex >= 0) {
    nsAutoString ext;
    mDefault.Right(ext, mDefault.Length() - extIndex);
    if (ext.LowerCaseEqualsLiteral(".htm")  ||
        ext.LowerCaseEqualsLiteral(".html") ||
        ext.LowerCaseEqualsLiteral(".shtml"))
      return true;
  }
  return false;
}
