/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePicker.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <cderr.h>

#include "mozilla/Assertions.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsWindow.h"
#include "nsEnumeratorUtils.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsToolkit.h"
#include "WinUtils.h"
#include "nsPIDOMWindow.h"

#include "mozilla/widget/filedialog/WinFileDialogCommands.h"

using mozilla::Maybe;
using mozilla::Result;
using mozilla::UniquePtr;

using namespace mozilla::widget;

UniquePtr<char16_t[], nsFilePicker::FreeDeleter>
    nsFilePicker::sLastUsedUnicodeDirectory;

#define MAX_EXTENSION_LENGTH 10

///////////////////////////////////////////////////////////////////////////////
// Helper classes

// Manages matching PickerOpen/PickerClosed calls on the parent widget.
class AutoWidgetPickerState {
 public:
  explicit AutoWidgetPickerState(nsIWidget* aWidget)
      : mWindow(static_cast<nsWindow*>(aWidget)) {
    PickerState(true);
  }

  ~AutoWidgetPickerState() { PickerState(false); }

 private:
  void PickerState(bool aFlag) {
    if (mWindow) {
      if (aFlag)
        mWindow->PickerOpen();
      else
        mWindow->PickerClosed();
    }
  }
  RefPtr<nsWindow> mWindow;
};

///////////////////////////////////////////////////////////////////////////////
// nsIFilePicker

nsFilePicker::nsFilePicker() : mSelectedType(1) {}

NS_IMPL_ISUPPORTS(nsFilePicker, nsIFilePicker)

NS_IMETHODIMP nsFilePicker::Init(mozIDOMWindowProxy* aParent,
                                 const nsAString& aTitle,
                                 nsIFilePicker::Mode aMode) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aParent);
  nsIDocShell* docShell = window ? window->GetDocShell() : nullptr;
  mLoadContext = do_QueryInterface(docShell);

  return nsBaseFilePicker::Init(aParent, aTitle, aMode);
}

/* static */
Result<Maybe<filedialog::Results>, HRESULT> nsFilePicker::ShowFilePickerLocal(
    HWND parent, filedialog::FileDialogType type,
    nsTArray<filedialog::Command> const& commands) {
  using mozilla::Err;
  using mozilla::Nothing;
  using mozilla::Some;

  namespace fd = filedialog;

  RefPtr<IFileDialog> dialog;
  MOZ_TRY_VAR(dialog, fd::MakeFileDialog(type));

  if (auto const res = fd::ApplyCommands(dialog.get(), commands); FAILED(res)) {
    return Err(res);
  }

  // synchronously show the dialog
  auto const ret = dialog->Show(parent);
  if (ret == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return Maybe<fd::Results>(Nothing());
  }
  if (FAILED(ret)) {
    return Err(ret);
  }

  return fd::GetFileResults(dialog.get()).map(mozilla::Some<fd::Results>);
}

/* static */
Result<Maybe<nsString>, HRESULT> nsFilePicker::ShowFolderPickerLocal(
    HWND parent, nsTArray<filedialog::Command> const& commands) {
  using mozilla::Err;
  using mozilla::Nothing;
  using mozilla::Some;
  namespace fd = filedialog;

  RefPtr<IFileDialog> dialog;
  MOZ_TRY_VAR(dialog, fd::MakeFileDialog(fd::FileDialogType::Open));

  if (auto const res = fd::ApplyCommands(dialog.get(), commands); FAILED(res)) {
    return Err(res);
  }

  // synchronously show the dialog
  auto const ret = dialog->Show(parent);
  if (ret == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return Maybe<nsString>(Nothing());
  }
  if (FAILED(ret)) {
    return Err(ret);
  }

  return fd::GetFolderResults(dialog.get()).map(mozilla::Some<nsString>);
}

/*
 * Folder picker invocation
 */

/*
 * Show a folder picker.
 *
 * @param aInitialDir   The initial directory, the last used directory will be
 *                      used if left blank.
 * @return true if a file was selected successfully.
 */
bool nsFilePicker::ShowFolderPicker(const nsString& aInitialDir) {
  namespace fd = ::mozilla::widget::filedialog;
  nsTArray<fd::Command> commands = {
      fd::SetOptions(FOS_PICKFOLDERS),
      fd::SetTitle(mTitle),
  };

  if (!mOkButtonLabel.IsEmpty()) {
    commands.AppendElement(fd::SetOkButtonLabel(mOkButtonLabel));
  }

  if (!aInitialDir.IsEmpty()) {
    commands.AppendElement(fd::SetFolder(aInitialDir));
  }

  nsString result;
  {
    ScopedRtlShimWindow shim(mParentWidget.get());
    AutoWidgetPickerState awps(mParentWidget);

    mozilla::BackgroundHangMonitor().NotifyWait();
    auto res = ShowFolderPickerLocal(shim.get(), commands);
    if (res.isErr()) {
      NS_WARNING("ShowFolderPickerImpl failed");
      return false;
    }

    auto optResults = res.unwrap();
    if (!optResults) {
      // cancellation, not error
      return false;
    }

    result = optResults.extract();
  }

  mUnicodeFile = result;
  return true;
}

/*
 * File open and save picker invocation
 */

/*
 * Show a file picker.
 *
 * @param aInitialDir   The initial directory, the last used directory will be
 *                      used if left blank.
 * @return true if a file was selected successfully.
 */
bool nsFilePicker::ShowFilePicker(const nsString& aInitialDir) {
  AUTO_PROFILER_LABEL("nsFilePicker::ShowFilePicker", OTHER);

  namespace fd = ::mozilla::widget::filedialog;
  nsTArray<fd::Command> commands;
  // options
  {
    FILEOPENDIALOGOPTIONS fos = 0;
    fos |= FOS_SHAREAWARE | FOS_OVERWRITEPROMPT | FOS_FORCEFILESYSTEM;

    // Handle add to recent docs settings
    if (IsPrivacyModeEnabled() || !mAddToRecentDocs) {
      fos |= FOS_DONTADDTORECENT;
    }

    // mode specific
    switch (mMode) {
      case modeOpen:
        fos |= FOS_FILEMUSTEXIST;
        break;

      case modeOpenMultiple:
        fos |= FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT;
        break;

      case modeSave:
        fos |= FOS_NOREADONLYRETURN;
        // Don't follow shortcuts when saving a shortcut, this can be used
        // to trick users (bug 271732)
        if (IsDefaultPathLink()) fos |= FOS_NODEREFERENCELINKS;
        break;

      case modeGetFolder:
        MOZ_ASSERT(false, "file-picker opened in directory-picker mode");
        return false;
    }

    commands.AppendElement(fd::SetOptions(fos));
  }

  // initial strings

  // title
  commands.AppendElement(fd::SetTitle(mTitle));

  // default filename
  if (!mDefaultFilename.IsEmpty()) {
    // Prevent the shell from expanding environment variables by removing
    // the % characters that are used to delimit them.
    nsAutoString sanitizedFilename(mDefaultFilename);
    sanitizedFilename.ReplaceChar('%', '_');

    commands.AppendElement(fd::SetFileName(sanitizedFilename));
  }

  // default extension to append to new files
  if (!mDefaultExtension.IsEmpty()) {
    // We don't want environment variables expanded in the extension either.
    nsAutoString sanitizedExtension(mDefaultExtension);
    sanitizedExtension.ReplaceChar('%', '_');

    commands.AppendElement(fd::SetDefaultExtension(sanitizedExtension));
  } else if (IsDefaultPathHtml()) {
    commands.AppendElement(fd::SetDefaultExtension(u"html"_ns));
  }

  // initial location
  if (!aInitialDir.IsEmpty()) {
    commands.AppendElement(fd::SetFolder(aInitialDir));
  }

  // filter types and the default index
  if (!mFilterList.IsEmpty()) {
    nsTArray<fd::ComDlgFilterSpec> fileTypes;
    for (auto const& filter : mFilterList) {
      fileTypes.EmplaceBack(filter.title, filter.filter);
    }
    commands.AppendElement(fd::SetFileTypes(std::move(fileTypes)));
    commands.AppendElement(fd::SetFileTypeIndex(mSelectedType));
  }

  // display
  fd::Results result;
  {
    ScopedRtlShimWindow shim(mParentWidget.get());
    AutoWidgetPickerState awps(mParentWidget);

    mozilla::BackgroundHangMonitor().NotifyWait();
    auto res = ShowFilePickerLocal(
        shim.get(),
        mMode == modeSave ? FileDialogType::Save : FileDialogType::Open,
        commands);

    if (res.isErr()) {
      NS_WARNING("ShowFilePickerImpl failed");
      return false;
    }

    auto optResults = res.unwrap();
    if (!optResults) {
      // cancellation, not error
      return false;
    }

    result = optResults.extract();
  }

  // Remember what filter type the user selected
  mSelectedType = int32_t(result.selectedFileTypeIndex());

  auto const& paths = result.paths();

  // single selection
  if (mMode != modeOpenMultiple) {
    if (!paths.IsEmpty()) {
      MOZ_ASSERT(paths.Length() == 1);
      mUnicodeFile = paths[0];
      return true;
    }
    return false;
  }

  // multiple selection
  for (auto const& str : paths) {
    nsCOMPtr<nsIFile> file;
    if (NS_SUCCEEDED(NS_NewLocalFile(str, false, getter_AddRefs(file)))) {
      mFiles.AppendObject(file);
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// nsIFilePicker impl.

nsresult nsFilePicker::ShowW(nsIFilePicker::ResultCode* aReturnVal) {
  NS_ENSURE_ARG_POINTER(aReturnVal);

  *aReturnVal = returnCancel;

  nsAutoString initialDir;
  if (mDisplayDirectory) mDisplayDirectory->GetPath(initialDir);

  // If no display directory, re-use the last one.
  if (initialDir.IsEmpty()) {
    // Allocate copy of last used dir.
    initialDir = sLastUsedUnicodeDirectory.get();
  }

  // Clear previous file selections
  mUnicodeFile.Truncate();
  mFiles.Clear();

  // On Win10, the picker doesn't support per-monitor DPI, so we open it
  // with our context set temporarily to system-dpi-aware
  WinUtils::AutoSystemDpiAware dpiAwareness;

  bool result = false;
  if (mMode == modeGetFolder) {
    result = ShowFolderPicker(initialDir);
  } else {
    result = ShowFilePicker(initialDir);
  }

  // exit, and return returnCancel in aReturnVal
  if (!result) return NS_OK;

  RememberLastUsedDirectory();

  nsIFilePicker::ResultCode retValue = returnOK;
  if (mMode == modeSave) {
    // Windows does not return resultReplace, we must check if file
    // already exists.
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_NewLocalFile(mUnicodeFile, false, getter_AddRefs(file));

    bool flag = false;
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(file->Exists(&flag)) && flag) {
      retValue = returnReplace;
    }
  }

  *aReturnVal = retValue;
  return NS_OK;
}

nsresult nsFilePicker::Show(nsIFilePicker::ResultCode* aReturnVal) {
  return ShowW(aReturnVal);
}

NS_IMETHODIMP
nsFilePicker::GetFile(nsIFile** aFile) {
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nullptr;

  if (mUnicodeFile.IsEmpty()) return NS_OK;

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(mUnicodeFile, false, getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return rv;
  }

  file.forget(aFile);
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFileURL(nsIURI** aFileURL) {
  *aFileURL = nullptr;
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (!file) return rv;

  return NS_NewFileURI(aFileURL, file);
}

NS_IMETHODIMP
nsFilePicker::GetFiles(nsISimpleEnumerator** aFiles) {
  NS_ENSURE_ARG_POINTER(aFiles);
  return NS_NewArrayEnumerator(aFiles, mFiles, NS_GET_IID(nsIFile));
}

// Get the file + path
NS_IMETHODIMP
nsBaseWinFilePicker::SetDefaultString(const nsAString& aString) {
  mDefaultFilePath = aString;

  // First, make sure the file name is not too long.
  int32_t nameLength;
  int32_t nameIndex = mDefaultFilePath.RFind(u"\\");
  if (nameIndex == kNotFound)
    nameIndex = 0;
  else
    nameIndex++;
  nameLength = mDefaultFilePath.Length() - nameIndex;
  mDefaultFilename.Assign(Substring(mDefaultFilePath, nameIndex));

  if (nameLength > MAX_PATH) {
    int32_t extIndex = mDefaultFilePath.RFind(u".");
    if (extIndex == kNotFound) extIndex = mDefaultFilePath.Length();

    // Let's try to shave the needed characters from the name part.
    int32_t charsToRemove = nameLength - MAX_PATH;
    if (extIndex - nameIndex >= charsToRemove) {
      mDefaultFilePath.Cut(extIndex - charsToRemove, charsToRemove);
    }
  }

  // Then, we need to replace illegal characters. At this stage, we cannot
  // replace the backslash as the string might represent a file path.
  mDefaultFilePath.ReplaceChar(u"" FILE_ILLEGAL_CHARACTERS, u'-');
  mDefaultFilename.ReplaceChar(u"" FILE_ILLEGAL_CHARACTERS, u'-');

  return NS_OK;
}

NS_IMETHODIMP
nsBaseWinFilePicker::GetDefaultString(nsAString& aString) {
  return NS_ERROR_FAILURE;
}

// The default extension to use for files
NS_IMETHODIMP
nsBaseWinFilePicker::GetDefaultExtension(nsAString& aExtension) {
  aExtension = mDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseWinFilePicker::SetDefaultExtension(const nsAString& aExtension) {
  mDefaultExtension = aExtension;
  return NS_OK;
}

// Set the filter index
NS_IMETHODIMP
nsFilePicker::GetFilterIndex(int32_t* aFilterIndex) {
  // Windows' filter index is 1-based, we use a 0-based system.
  *aFilterIndex = mSelectedType - 1;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetFilterIndex(int32_t aFilterIndex) {
  // Windows' filter index is 1-based, we use a 0-based system.
  mSelectedType = aFilterIndex + 1;
  return NS_OK;
}

void nsFilePicker::InitNative(nsIWidget* aParent, const nsAString& aTitle) {
  mParentWidget = aParent;
  mTitle.Assign(aTitle);
}

NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter) {
  nsString sanitizedFilter(aFilter);
  sanitizedFilter.ReplaceChar('%', '_');

  if (sanitizedFilter == u"..apps"_ns) {
    sanitizedFilter = u"*.exe;*.com"_ns;
  } else {
    sanitizedFilter.StripWhitespace();
    if (sanitizedFilter == u"*"_ns) {
      sanitizedFilter = u"*.*"_ns;
    }
  }
  mFilterList.AppendElement(
      Filter{.title = nsString(aTitle), .filter = std::move(sanitizedFilter)});
  return NS_OK;
}

void nsFilePicker::RememberLastUsedDirectory() {
  if (IsPrivacyModeEnabled()) {
    // Don't remember the directory if private browsing was in effect
    return;
  }

  nsCOMPtr<nsIFile> file;
  if (NS_FAILED(NS_NewLocalFile(mUnicodeFile, false, getter_AddRefs(file)))) {
    NS_WARNING("RememberLastUsedDirectory failed to init file path.");
    return;
  }

  nsCOMPtr<nsIFile> dir;
  nsAutoString newDir;
  if (NS_FAILED(file->GetParent(getter_AddRefs(dir))) ||
      !(mDisplayDirectory = dir) ||
      NS_FAILED(mDisplayDirectory->GetPath(newDir)) || newDir.IsEmpty()) {
    NS_WARNING("RememberLastUsedDirectory failed to get parent directory.");
    return;
  }

  sLastUsedUnicodeDirectory.reset(ToNewUnicode(newDir));
}

bool nsFilePicker::IsPrivacyModeEnabled() {
  return mLoadContext && mLoadContext->UsePrivateBrowsing();
}

bool nsFilePicker::IsDefaultPathLink() {
  NS_ConvertUTF16toUTF8 ext(mDefaultFilePath);
  ext.Trim(" .", false, true);  // watch out for trailing space and dots
  ToLowerCase(ext);
  return StringEndsWith(ext, ".lnk"_ns) || StringEndsWith(ext, ".pif"_ns) ||
         StringEndsWith(ext, ".url"_ns);
}

bool nsFilePicker::IsDefaultPathHtml() {
  int32_t extIndex = mDefaultFilePath.RFind(u".");
  if (extIndex >= 0) {
    nsAutoString ext;
    mDefaultFilePath.Right(ext, mDefaultFilePath.Length() - extIndex);
    if (ext.LowerCaseEqualsLiteral(".htm") ||
        ext.LowerCaseEqualsLiteral(".html") ||
        ext.LowerCaseEqualsLiteral(".shtml"))
      return true;
  }
  return false;
}
