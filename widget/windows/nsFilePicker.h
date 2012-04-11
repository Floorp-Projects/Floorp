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

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include <windows.h>

// For Vista IFileDialog interfaces which aren't exposed
// unless _WIN32_WINNT >= _WIN32_WINNT_LONGHORN.
#if _WIN32_WINNT < _WIN32_WINNT_LONGHORN
#define _WIN32_WINNT_bak _WIN32_WINNT
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_LONGHORN
#define _WIN32_IE_bak _WIN32_IE
#undef _WIN32_IE
#define _WIN32_IE _WIN32_IE_IE70
#endif

#include "nsILocalFile.h"
#include "nsITimer.h"
#include "nsISimpleEnumerator.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"
#include "nsICharsetConverterManager.h"
#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsdefs.h"
#include <commdlg.h>
#include <shobjidl.h>

class nsILoadContext;

/**
 * Native Windows FileSelector wrapper
 */

class nsFilePicker :
  public nsBaseFilePicker,
  public IFileDialogEvents
{
public:
  nsFilePicker(); 
  virtual ~nsFilePicker();

  NS_IMETHOD Init(nsIDOMWindow *aParent, const nsAString& aTitle, PRInt16 aMode);
                  
  NS_DECL_ISUPPORTS
  
  // IUnknown's QueryInterface
  STDMETHODIMP QueryInterface(REFIID refiid, void** ppvResult);

  // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString);
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString);
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension);
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension);
  NS_IMETHOD GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHOD SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHOD GetFile(nsILocalFile * *aFile);
  NS_IMETHOD GetFileURL(nsIURI * *aFileURL);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHOD Show(PRInt16 *aReturnVal); 
  NS_IMETHOD ShowW(PRInt16 *aReturnVal); 
  NS_IMETHOD AppendFilter(const nsAString& aTitle, const nsAString& aFilter);

  // IFileDialogEvents
  HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog *pfd);
  HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog *pfd, IShellItem *psiFolder);
  HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog *pfd);
  HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog *pfd);
  HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog *pfd, IShellItem *psi, FDE_SHAREVIOLATION_RESPONSE *pResponse);
  HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog *pfd);
  HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog *pfd, IShellItem *psi, FDE_OVERWRITE_RESPONSE *pResponse);

protected:
  enum PickerType {
    PICKER_TYPE_OPEN,
    PICKER_TYPE_SAVE,
  };

  /* method from nsBaseFilePicker */
  virtual void InitNative(nsIWidget *aParent,
                          const nsAString& aTitle,
                          PRInt16 aMode);
  static void GetQualifiedPath(const PRUnichar *aInPath, nsString &aOutPath);
  void GetFilterListArray(nsString& aFilterList);
  bool FilePickerWrapper(OPENFILENAMEW* ofn, PickerType aType);
  bool ShowXPFolderPicker(const nsString& aInitialDir);
  bool ShowXPFilePicker(const nsString& aInitialDir);
  bool ShowFolderPicker(const nsString& aInitialDir);
  bool ShowFilePicker(const nsString& aInitialDir);
  void AppendXPFilter(const nsAString& aTitle, const nsAString& aFilter);
  void RememberLastUsedDirectory();
  bool IsPrivacyModeEnabled();
  bool IsDefaultPathLink();
  bool IsDefaultPathHtml();
  void SetDialogHandle(HWND aWnd);
  bool ClosePickerIfNeeded(bool aIsXPDialog);
  static void PickerCallbackTimerFunc(nsITimer *aTimer, void *aPicker);
  static UINT_PTR CALLBACK MultiFilePickerHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static UINT_PTR CALLBACK FilePickerHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  nsCOMPtr<nsILoadContext> mLoadContext;
  nsCOMPtr<nsIWidget>    mParentWidget;
  nsString               mTitle;
  PRInt16                mMode;
  nsCString              mFile;
  nsString               mDefaultFilePath;
  nsString               mDefaultFilename;
  nsString               mDefaultExtension;
  nsString               mFilterList;
  PRInt16                mSelectedType;
  nsCOMArray<nsILocalFile> mFiles;
  static char            mLastUsedDirectory[];
  nsString               mUnicodeFile;
  static PRUnichar      *mLastUsedUnicodeDirectory;
  HWND                   mDlgWnd;

  class ComDlgFilterSpec
  {
  public:
    ComDlgFilterSpec() {}
    ~ComDlgFilterSpec() {}
    
    const PRUint32 Length() {
      return mSpecList.Length();
    }

    const bool IsEmpty() {
      return (mSpecList.Length() == 0);
    }

    const COMDLG_FILTERSPEC* get() {
      return mSpecList.Elements();
    }
    
    void Append(const nsAString& aTitle, const nsAString& aFilter);
  private:
    nsAutoTArray<COMDLG_FILTERSPEC, 1> mSpecList;
    nsAutoTArray<nsString, 2> mStrings;
  };

  ComDlgFilterSpec       mComFilterList;
  DWORD                  mFDECookie;
};

#if defined(_WIN32_WINNT_bak)
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_bak
#undef _WIN32_IE
#define _WIN32_IE _WIN32_IE_bak
#endif

#endif // nsFilePicker_h__
