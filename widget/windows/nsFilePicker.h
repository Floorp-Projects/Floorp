/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include <windows.h>

#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsCOMArray.h"
#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsdefs.h"
#include <commdlg.h>
#include <shobjidl.h>
#undef LogSeverity  // SetupAPI.h #defines this as DWORD

class nsILoadContext;

class nsBaseWinFilePicker : public nsBaseFilePicker {
 public:
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString) override;
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString) override;
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension) override;
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension) override;

 protected:
  nsString mDefaultFilePath;
  nsString mDefaultFilename;
  nsString mDefaultExtension;
};

/**
 * Native Windows FileSelector wrapper
 */

class nsFilePicker : public nsBaseWinFilePicker {
  virtual ~nsFilePicker() = default;

 public:
  nsFilePicker();

  NS_IMETHOD Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                  int16_t aMode) override;

  NS_DECL_ISUPPORTS

  // nsIFilePicker (less what's in nsBaseFilePicker and nsBaseWinFilePicker)
  NS_IMETHOD GetFilterIndex(int32_t* aFilterIndex) override;
  NS_IMETHOD SetFilterIndex(int32_t aFilterIndex) override;
  NS_IMETHOD GetFile(nsIFile** aFile) override;
  NS_IMETHOD GetFileURL(nsIURI** aFileURL) override;
  NS_IMETHOD GetFiles(nsISimpleEnumerator** aFiles) override;
  NS_IMETHOD AppendFilter(const nsAString& aTitle,
                          const nsAString& aFilter) override;

 protected:
  /* method from nsBaseFilePicker */
  virtual void InitNative(nsIWidget* aParent, const nsAString& aTitle) override;
  nsresult Show(int16_t* aReturnVal) override;
  nsresult ShowW(int16_t* aReturnVal);
  void GetFilterListArray(nsString& aFilterList);
  bool ShowFolderPicker(const nsString& aInitialDir);
  bool ShowFilePicker(const nsString& aInitialDir);
  void RememberLastUsedDirectory();
  bool IsPrivacyModeEnabled();
  bool IsDefaultPathLink();
  bool IsDefaultPathHtml();

  nsCOMPtr<nsILoadContext> mLoadContext;
  nsCOMPtr<nsIWidget> mParentWidget;
  nsString mTitle;
  nsCString mFile;
  nsString mFilterList;
  int16_t mSelectedType;
  nsCOMArray<nsIFile> mFiles;
  nsString mUnicodeFile;

  struct FreeDeleter {
    void operator()(void* aPtr) { ::free(aPtr); }
  };
  static mozilla::UniquePtr<char16_t[], FreeDeleter> sLastUsedUnicodeDirectory;

  class ComDlgFilterSpec {
   public:
    ComDlgFilterSpec() {}
    ~ComDlgFilterSpec() {}

    const uint32_t Length() { return mSpecList.Length(); }

    const bool IsEmpty() { return (mSpecList.Length() == 0); }

    const COMDLG_FILTERSPEC* get() { return mSpecList.Elements(); }

    void Append(const nsAString& aTitle, const nsAString& aFilter);

   private:
    AutoTArray<COMDLG_FILTERSPEC, 1> mSpecList;
    AutoTArray<nsString, 2> mStrings;
  };

  ComDlgFilterSpec mComFilterList;
};

#endif  // nsFilePicker_h__
