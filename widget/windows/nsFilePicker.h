/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include <windows.h>

#include "mozilla/MozPromise.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "nsIContentAnalysis.h"
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

namespace mozilla::widget::filedialog {
class Command;
class Results;
enum class FileDialogType : uint8_t;
}  // namespace mozilla::widget::filedialog

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

class nsFilePicker final : public nsBaseWinFilePicker {
  virtual ~nsFilePicker() = default;

  template <typename T>
  using Maybe = mozilla::Maybe<T>;
  template <typename T>
  using Result = mozilla::Result<T, HRESULT>;
  template <typename Res>
  using FPPromise = RefPtr<mozilla::MozPromise<Maybe<Res>, HRESULT, true>>;

  using Command = mozilla::widget::filedialog::Command;
  using Results = mozilla::widget::filedialog::Results;
  using FileDialogType = mozilla::widget::filedialog::FileDialogType;

 public:
  nsFilePicker();

  NS_IMETHOD Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                  nsIFilePicker::Mode aMode,
                  mozilla::dom::BrowsingContext* aBrowsingContext) override;

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
  nsresult Show(nsIFilePicker::ResultCode* aReturnVal) override;
  void GetFilterListArray(nsString& aFilterList);

  NS_IMETHOD Open(nsIFilePickerShownCallback* aCallback) override;

 private:
  RefPtr<mozilla::MozPromise<bool, HRESULT, true>> ShowFolderPicker(
      const nsString& aInitialDir);
  RefPtr<mozilla::MozPromise<bool, HRESULT, true>> ShowFilePicker(
      const nsString& aInitialDir);

  // Show the dialog out-of-process.
  static FPPromise<Results> ShowFilePickerRemote(
      HWND aParent, FileDialogType type, nsTArray<Command> const& commands);
  static FPPromise<nsString> ShowFolderPickerRemote(
      HWND aParent, nsTArray<Command> const& commands);

  // Show the dialog in-process.
  static FPPromise<Results> ShowFilePickerLocal(
      HWND aParent, FileDialogType type, nsTArray<Command> const& commands);
  static FPPromise<nsString> ShowFolderPickerLocal(
      HWND aParent, nsTArray<Command> const& commands);

  void ClearFiles();
  using ContentAnalysisResponse = mozilla::MozPromise<bool, nsresult, true>;
  RefPtr<ContentAnalysisResponse> CheckContentAnalysisService();

 protected:
  void RememberLastUsedDirectory();
  bool IsPrivacyModeEnabled();
  bool IsDefaultPathLink();
  bool IsDefaultPathHtml();

  nsCOMPtr<nsILoadContext> mLoadContext;
  nsCOMPtr<nsIWidget> mParentWidget;
  nsString mTitle;
  nsCString mFile;
  int32_t mSelectedType;
  nsCOMArray<nsIFile> mFiles;
  nsString mUnicodeFile;

  struct FreeDeleter {
    void operator()(void* aPtr) { ::free(aPtr); }
  };
  static mozilla::UniquePtr<char16_t[], FreeDeleter> sLastUsedUnicodeDirectory;

  struct Filter {
    nsString title;
    nsString filter;
  };
  AutoTArray<Filter, 1> mFilterList;
};

#endif  // nsFilePicker_h__
