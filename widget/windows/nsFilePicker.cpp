/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePicker.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <cderr.h>
#include <winerror.h>
#include <winuser.h>
#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsCRT.h"
#include "nsEnumeratorUtils.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsToolkit.h"
#include "nsWindow.h"
#include "WinUtils.h"

#include "mozilla/widget/filedialog/WinFileDialogCommands.h"
#include "mozilla/widget/filedialog/WinFileDialogParent.h"

using mozilla::Maybe;
using mozilla::Result;
using mozilla::UniquePtr;

using namespace mozilla::widget;

UniquePtr<char16_t[], nsFilePicker::FreeDeleter>
    nsFilePicker::sLastUsedUnicodeDirectory;

using mozilla::LogLevel;

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

  AutoWidgetPickerState(AutoWidgetPickerState const&) = delete;
  AutoWidgetPickerState(AutoWidgetPickerState&& that) noexcept = default;

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
  // Don't attempt to open a real file-picker in headless mode.
  if (gfxPlatform::IsHeadless()) {
    return nsresult::NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aParent);
  nsIDocShell* docShell = window ? window->GetDocShell() : nullptr;
  mLoadContext = do_QueryInterface(docShell);

  return nsBaseFilePicker::Init(aParent, aTitle, aMode);
}

namespace mozilla::detail {
// Boilerplate for remotely showing a file dialog.
template <typename ActionType,
          typename ReturnType = typename decltype(std::declval<ActionType>()(
              nullptr))::element_type::ResolveValueType>
static auto ShowRemote(HWND parent, ActionType&& action)
    -> RefPtr<MozPromise<ReturnType, HRESULT, true>> {
  using RetPromise = MozPromise<ReturnType, HRESULT, true>;

  constexpr static const auto fail = []() {
    return RetPromise::CreateAndReject(E_FAIL, __PRETTY_FUNCTION__);
  };

  auto mgr = mozilla::ipc::UtilityProcessManager::GetSingleton();
  if (!mgr) {
    MOZ_ASSERT(false);
    return fail();
  }

  auto wfda = mgr->CreateWinFileDialogActor();
  if (!wfda) {
    return fail();
  }

  using mozilla::widget::filedialog::sLogFileDialog;

  // WORKAROUND FOR UNDOCUMENTED BEHAVIOR: `IFileDialog::Show` disables the
  // top-level ancestor of its provided owner-window. If the modal window's
  // container process crashes, it will never get a chance to undo that, so
  // we have to do it manually.
  struct WindowEnabler {
    HWND hwnd;
    BOOL enabled;
    explicit WindowEnabler(HWND hwnd)
        : hwnd(hwnd), enabled(::IsWindowEnabled(hwnd)) {
      MOZ_LOG(sLogFileDialog, LogLevel::Info,
              ("ShowRemote: HWND %08zX enabled=%u", uintptr_t(hwnd), enabled));
    }
    WindowEnabler(WindowEnabler const&) = default;

    void restore() const {
      MOZ_LOG(sLogFileDialog, LogLevel::Info,
              ("ShowRemote: HWND %08zX enabled=%u (restoring to %u)",
               uintptr_t(hwnd), ::IsWindowEnabled(hwnd), enabled));
      ::EnableWindow(hwnd, enabled);
    }
  };
  HWND const rootWindow = ::GetAncestor(parent, GA_ROOT);

  return wfda->Then(
      mozilla::GetMainThreadSerialEventTarget(),
      "nsFilePicker ShowRemote acquire",
      [action = std::forward<ActionType>(action),
       rootWindow](filedialog::ProcessProxy const& p) -> RefPtr<RetPromise> {
        MOZ_LOG(sLogFileDialog, LogLevel::Info,
                ("nsFilePicker ShowRemote first callback: p = [%p]", p.get()));
        WindowEnabler enabledState(rootWindow);

        // false positive: not actually redundant
        // NOLINTNEXTLINE(readability-redundant-smartptr-get)
        return action(p.get())
            ->Then(
                mozilla::GetMainThreadSerialEventTarget(),
                "nsFilePicker ShowRemote call",
                [p](ReturnType ret) {
                  return RetPromise::CreateAndResolve(std::move(ret),
                                                      __PRETTY_FUNCTION__);
                },
                [](mozilla::ipc::ResponseRejectReason error) {
                  MOZ_LOG(sLogFileDialog, LogLevel::Error,
                          ("IPC call rejected: %zu", size_t(error)));
                  return fail();
                })
            // Unconditionally restore the disabled state.
            ->Then(mozilla::GetMainThreadSerialEventTarget(),
                   "nsFilePicker ShowRemote cleanup",
                   [enabledState](
                       typename RetPromise::ResolveOrRejectValue const& val) {
                     enabledState.restore();
                     return RetPromise::CreateAndResolveOrReject(
                         val, "nsFilePicker ShowRemote cleanup fmap");
                   });
      },
      [](nsresult error) -> RefPtr<RetPromise> {
        MOZ_LOG(sLogFileDialog, LogLevel::Error,
                ("could not acquire WinFileDialog: %zu", size_t(error)));
        return fail();
      });
}

// LocalAndOrRemote
//
// Pseudo-namespace for the private implementation details of its AsyncExecute()
// function. Not intended to be instantiated.
struct LocalAndOrRemote {
  LocalAndOrRemote() = delete;

 private:
  // Helper for generically copying ordinary types and nsTArray (which lacks a
  // copy constructor) in the same breath.
  //
  // N.B.: This function is ultimately the reason for LocalAndOrRemote existing
  // (rather than AsyncExecute() being a free function at namespace scope).
  // While this is notionally an implementation detail of AsyncExecute(), it
  // can't be confined to a local struct therein because local structs can't
  // have template member functions [temp.mem/2].
  template <typename T>
  static T Copy(T const& val) {
    return val;
  }
  template <typename T>
  static nsTArray<T> Copy(nsTArray<T> const& arr) {
    return arr.Clone();
  }

  // The possible execution strategies of AsyncExecute.
  enum Strategy { Local, Remote, RemoteWithFallback };

  // Decode the relevant preference to determine the desired execution-
  // strategy.
  static Strategy GetStrategy() {
    int32_t const pref =
        mozilla::StaticPrefs::widget_windows_utility_process_file_picker();
    switch (pref) {
      case -1:
        return Local;
      case 2:
        return Remote;
      case 1:
        return RemoteWithFallback;

      default:
#ifdef NIGHTLY_BUILD
        // on Nightly builds, fall back to local on failure
        return RemoteWithFallback;
#else
        // on release and beta, remain local-only for now
        return Local;
#endif
    }
  };

 public:
  // Invoke either or both of a "do locally" and "do remotely" function with the
  // provided arguments, depending on the relevant preference-value and whether
  // or not the remote version fails.
  //
  // Both functions must be asynchronous, returning a `RefPtr<MozPromise<...>>`.
  // "Failure" is defined as the promise being rejected.
  template <typename Fn1, typename Fn2, typename... Args>
  static auto AsyncExecute(Fn1 local, Fn2 remote, Args const&... args)
      -> std::invoke_result_t<Fn1, Args...> {
    static_assert(std::is_same_v<std::invoke_result_t<Fn1, Args...>,
                                 std::invoke_result_t<Fn2, Args...>>);
    using PromiseT = typename std::invoke_result_t<Fn1, Args...>::element_type;

    constexpr static char kFunctionName[] = "LocalAndOrRemote::AsyncExecute";

    switch (GetStrategy()) {
      case Local:
        return local(args...);

      case Remote:
        return remote(args...);

      case RemoteWithFallback:
        return remote(args...)->Then(
            NS_GetCurrentThread(), kFunctionName,
            [](typename PromiseT::ResolveValueType result) -> RefPtr<PromiseT> {
              // success; stop here
              return PromiseT::CreateAndResolve(result, kFunctionName);
            },
            // initialized lambda pack captures are C++20 (clang 9, gcc 9);
            // `make_tuple` is just a C++17 workaround
            [=, tuple = std::make_tuple(Copy(args)...)](
                typename PromiseT::RejectValueType _err) mutable
            -> RefPtr<PromiseT> {
              // failure; retry locally
              return std::apply(local, std::move(tuple));
            });
    }
  }
};

}  // namespace mozilla::detail

/* static */
nsFilePicker::FPPromise<filedialog::Results> nsFilePicker::ShowFilePickerRemote(
    HWND parent, filedialog::FileDialogType type,
    nsTArray<filedialog::Command> const& commands) {
  using mozilla::widget::filedialog::sLogFileDialog;
  return mozilla::detail::ShowRemote(
      parent, [parent, type, commands = commands.Clone()](
                  filedialog::WinFileDialogParent* p) {
        MOZ_LOG(sLogFileDialog, LogLevel::Info,
                ("%s: p = [%p]", __PRETTY_FUNCTION__, p));
        return p->SendShowFileDialog((uintptr_t)parent, type, commands);
      });
}

/* static */
nsFilePicker::FPPromise<nsString> nsFilePicker::ShowFolderPickerRemote(
    HWND parent, nsTArray<filedialog::Command> const& commands) {
  using mozilla::widget::filedialog::sLogFileDialog;
  return mozilla::detail::ShowRemote(
      parent, [parent, commands = commands.Clone()](
                  filedialog::WinFileDialogParent* p) {
        MOZ_LOG(sLogFileDialog, LogLevel::Info,
                ("%s: p = [%p]", __PRETTY_FUNCTION__, p));
        return p->SendShowFolderDialog((uintptr_t)parent, commands);
      });
}

/* static */
nsFilePicker::FPPromise<filedialog::Results> nsFilePicker::ShowFilePickerLocal(
    HWND parent, filedialog::FileDialogType type,
    nsTArray<filedialog::Command> const& commands) {
  return filedialog::SpawnFilePicker(parent, type, commands.Clone());
}

/* static */
nsFilePicker::FPPromise<nsString> nsFilePicker::ShowFolderPickerLocal(
    HWND parent, nsTArray<filedialog::Command> const& commands) {
  return filedialog::SpawnFolderPicker(parent, commands.Clone());
}

/*
 * Folder picker invocation
 */

/*
 * Show a folder picker.
 *
 * @param aInitialDir   The initial directory. The last-used directory will be
 *                      used if left blank.
 * @return  A promise which:
 *          - resolves to true if a file was selected successfully (in which
 *            case mUnicodeFile will be updated);
 *          - resolves to false if the dialog was cancelled by the user;
 *          - is rejected with the associated HRESULT if some error occurred.
 */
RefPtr<mozilla::MozPromise<bool, HRESULT, true>> nsFilePicker::ShowFolderPicker(
    const nsString& aInitialDir) {
  using Promise = mozilla::MozPromise<bool, HRESULT, true>;
  constexpr static auto Ok = [](bool val) {
    return Promise::CreateAndResolve(val, "nsFilePicker::ShowFolderPicker");
  };
  constexpr static auto NotOk = [](HRESULT val = E_FAIL) {
    return Promise::CreateAndReject(val, "nsFilePicker::ShowFolderPicker");
  };

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

  ScopedRtlShimWindow shim(mParentWidget.get());
  AutoWidgetPickerState awps(mParentWidget);

  return mozilla::detail::LocalAndOrRemote::AsyncExecute(
             &ShowFolderPickerLocal, &ShowFolderPickerRemote, shim.get(),
             commands)
      ->Then(
          NS_GetCurrentThread(), __PRETTY_FUNCTION__,
          [self = RefPtr(this), shim = std::move(shim),
           awps = std::move(awps)](Maybe<nsString> val) {
            if (val) {
              self->mUnicodeFile = val.extract();
              return Ok(true);
            }
            return Ok(false);
          },
          [](HRESULT err) {
            NS_WARNING("ShowFolderPicker failed");
            return NotOk(err);
          });
}

/*
 * File open and save picker invocation
 */

/*
 * Show a file picker.
 *
 * @param aInitialDir   The initial directory. The last-used directory will be
 *                      used if left blank.
 * @return  A promise which:
 *          - resolves to true if one or more files were selected successfully
 *            (in which case mUnicodeFile and/or mFiles will be updated);
 *          - resolves to false if the dialog was cancelled by the user;
 *          - is rejected with the associated HRESULT if some error occurred.
 */
RefPtr<mozilla::MozPromise<bool, HRESULT, true>> nsFilePicker::ShowFilePicker(
    const nsString& aInitialDir) {
  AUTO_PROFILER_LABEL("nsFilePicker::ShowFilePicker", OTHER);

  using Promise = mozilla::MozPromise<bool, HRESULT, true>;
  constexpr static auto Ok = [](bool val) {
    return Promise::CreateAndResolve(val, "nsFilePicker::ShowFilePicker");
  };
  constexpr static auto NotOk = [](HRESULT val = E_FAIL) {
    return Promise::CreateAndReject(val, "nsFilePicker::ShowFilePicker");
  };

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
        return NotOk(E_FAIL);
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

  ScopedRtlShimWindow shim(mParentWidget.get());
  AutoWidgetPickerState awps(mParentWidget);

  mozilla::BackgroundHangMonitor().NotifyWait();
  auto type = mMode == modeSave ? FileDialogType::Save : FileDialogType::Open;

  auto promise = mozilla::detail::LocalAndOrRemote::AsyncExecute(
      &ShowFilePickerLocal, &ShowFilePickerRemote, shim.get(), type, commands);

  return promise->Then(
      mozilla::GetMainThreadSerialEventTarget(), __PRETTY_FUNCTION__,
      [self = RefPtr(this), mode = mMode, shim = std::move(shim),
       awps = std::move(awps)](Maybe<Results> res_opt) {
        if (!res_opt) {
          return Ok(false);
        }
        auto result = res_opt.extract();

        // Remember what filter type the user selected
        self->mSelectedType = int32_t(result.selectedFileTypeIndex());

        auto const& paths = result.paths();

        // single selection
        if (mode != modeOpenMultiple) {
          if (!paths.IsEmpty()) {
            MOZ_ASSERT(paths.Length() == 1);
            self->mUnicodeFile = paths[0];
            return Ok(true);
          }
          return Ok(false);
        }

        // multiple selection
        for (auto const& str : paths) {
          nsCOMPtr<nsIFile> file;
          if (NS_SUCCEEDED(NS_NewLocalFile(str, false, getter_AddRefs(file)))) {
            self->mFiles.AppendObject(file);
          }
        }

        return Ok(true);
      },
      [](HRESULT err) {
        NS_WARNING("ShowFilePicker failed");
        return NotOk(err);
      });
}

///////////////////////////////////////////////////////////////////////////////
// nsIFilePicker impl.

nsresult nsFilePicker::Open(nsIFilePickerShownCallback* aCallback) {
  NS_ENSURE_ARG_POINTER(aCallback);

  // Don't attempt to open a real file-picker in headless mode.
  if (gfxPlatform::IsHeadless()) {
    return nsresult::NS_ERROR_NOT_AVAILABLE;
  }

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

  auto promise = mMode == modeGetFolder ? ShowFolderPicker(initialDir)
                                        : ShowFilePicker(initialDir);

  auto p2 = promise->Then(
      mozilla::GetMainThreadSerialEventTarget(), __PRETTY_FUNCTION__,
      [self = RefPtr(this),
       callback = RefPtr(aCallback)](bool selectionMade) -> void {
        if (!selectionMade) {
          callback->Done(ResultCode::returnCancel);
          return;
        }

        self->RememberLastUsedDirectory();

        nsIFilePicker::ResultCode retValue = ResultCode::returnOK;

        if (self->mMode == modeSave) {
          // Windows does not return resultReplace; we must check whether the
          // file already exists.
          nsCOMPtr<nsIFile> file;
          nsresult rv =
              NS_NewLocalFile(self->mUnicodeFile, false, getter_AddRefs(file));

          bool flag = false;
          if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(file->Exists(&flag)) && flag) {
            retValue = ResultCode::returnReplace;
          }
        }

        callback->Done(retValue);
      },
      [callback = RefPtr(aCallback)](HRESULT err) {
        using mozilla::widget::filedialog::sLogFileDialog;
        MOZ_LOG(sLogFileDialog, LogLevel::Error,
                ("nsFilePicker: Show failed with hr=0x%08lX", err));
        callback->Done(ResultCode::returnCancel);
      });

  return NS_OK;
}

nsresult nsFilePicker::Show(nsIFilePicker::ResultCode* aReturnVal) {
  return NS_ERROR_NOT_IMPLEMENTED;
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
