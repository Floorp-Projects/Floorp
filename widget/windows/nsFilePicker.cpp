/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePicker.h"

#include <cderr.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sysinfoapi.h>
#include <winerror.h>
#include <winuser.h>
#include <utility>

#include "ContentAnalysis.h"
#include "mozilla/Assertions.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/Components.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsCRT.h"
#include "nsEnumeratorUtils.h"
#include "nsIContentAnalysis.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsToolkit.h"
#include "nsWindow.h"
#include "WinUtils.h"

#include "mozilla/glean/GleanMetrics.h"

#include "mozilla/widget/filedialog/WinFileDialogCommands.h"
#include "mozilla/widget/filedialog/WinFileDialogParent.h"

using mozilla::LogLevel;
using mozilla::UniquePtr;

using namespace mozilla::widget;

template <typename Res>
using FDPromise = filedialog::Promise<Res>;

UniquePtr<char16_t[], nsFilePicker::FreeDeleter>
    nsFilePicker::sLastUsedUnicodeDirectory;

#define MAX_EXTENSION_LENGTH 10

///////////////////////////////////////////////////////////////////////////////
// Helper classes

// Manages matching PickerOpen/PickerClosed calls on the parent widget.
class AutoWidgetPickerState {
  static RefPtr<nsWindow> GetWindowForWidget(nsIWidget* aWidget) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!aWidget) {
      return nullptr;
    }
    HWND hwnd = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
    return RefPtr(WinUtils::GetNSWindowPtr(hwnd));
  }

 public:
  explicit AutoWidgetPickerState(nsIWidget* aWidget)
      : mWindow(GetWindowForWidget(aWidget)) {
    MOZ_ASSERT(mWindow);
    if (mWindow) mWindow->PickerOpen();
  }
  ~AutoWidgetPickerState() {
    // may be null if moved-from
    if (mWindow) mWindow->PickerClosed();
  }

  AutoWidgetPickerState(AutoWidgetPickerState const&) = delete;
  AutoWidgetPickerState(AutoWidgetPickerState&& that) noexcept = default;

 private:
  RefPtr<nsWindow> mWindow;
};

///////////////////////////////////////////////////////////////////////////////
// nsIFilePicker

nsFilePicker::nsFilePicker() : mSelectedType(1) {}

NS_IMPL_ISUPPORTS(nsFilePicker, nsIFilePicker)

NS_IMETHODIMP nsFilePicker::Init(
    mozilla::dom::BrowsingContext* aBrowsingContext, const nsAString& aTitle,
    nsIFilePicker::Mode aMode) {
  // Don't attempt to open a real file-picker in headless mode.
  if (gfxPlatform::IsHeadless()) {
    return nsresult::NS_ERROR_NOT_AVAILABLE;
  }

  return nsBaseFilePicker::Init(aBrowsingContext, aTitle, aMode);
}

namespace mozilla::detail {
using Error = mozilla::widget::filedialog::Error;

// Boilerplate for remotely showing a file dialog.
template <typename ActionType,
          typename ReturnType = typename decltype(std::declval<ActionType>()(
              nullptr))::element_type::ResolveValueType>
static auto ShowRemote(ActionType&& action) -> RefPtr<FDPromise<ReturnType>> {
  using RetPromise = FDPromise<ReturnType>;

// "function-local" #define
#define FAIL(where_, why_)                                             \
  return RetPromise::CreateAndReject(MOZ_FD_LOCAL_ERROR(where_, why_), \
                                     __PRETTY_FUNCTION__)

  auto mgr = mozilla::ipc::UtilityProcessManager::GetSingleton();
  if (!mgr) {
    MOZ_ASSERT(false);
    FAIL("ShowRemote: UtilityProcessManager::GetSingleton", E_POINTER);
  }

  auto wfda = mgr->CreateWinFileDialogActor();
  if (!wfda) {
    FAIL("ShowRemote: invocation of CreateWinFileDialogActor", E_POINTER);
  }

  using mozilla::widget::filedialog::sLogFileDialog;

  return wfda->Then(
      mozilla::GetMainThreadSerialEventTarget(),
      "nsFilePicker ShowRemote acquire",
      [action = std::forward<ActionType>(action)](
          filedialog::ProcessProxy p) -> RefPtr<RetPromise> {
        MOZ_LOG(sLogFileDialog, LogLevel::Info,
                ("nsFilePicker ShowRemote first callback: p = [%p]", p.get()));

        // false positive: not actually redundant
        // NOLINTNEXTLINE(readability-redundant-smartptr-get)
        auto promise = action(p.get());
        return promise->Map(
            mozilla::GetMainThreadSerialEventTarget(), __func__,
            [p = std::move(p)](typename RetPromise::ResolveValueType&& val) {
              // explicitly retain the ProcessProxy until at least this point
              return std::move(val);
            });
      },
      [](mozilla::ipc::LaunchError const& error) {
        MOZ_LOG(sLogFileDialog, LogLevel::Error,
                ("could not acquire WinFileDialog: %s:%zu",
                 error.FunctionName().get(), size_t(error.ErrorCode())));
        return RetPromise::CreateAndReject(Error::From(error),
                                           "nsFilePicker::ShowRemote");
      });

#undef FAIL
}

namespace {

static RefPtr<FDPromise<Maybe<filedialog::Results>>> ShowFilePickerRemote(
    HWND parent, filedialog::FileDialogType type,
    nsTArray<filedialog::Command> const& commands) {
  using mozilla::widget::filedialog::sLogFileDialog;
  return mozilla::detail::ShowRemote(
      [parent, type,
       commands = commands.Clone()](filedialog::WinFileDialogParent* p) {
        MOZ_LOG(sLogFileDialog, LogLevel::Info,
                ("%s: p = [%p]", __PRETTY_FUNCTION__, p));
        return p->ShowFileDialogImpl(parent, type, commands);
      });
}

static RefPtr<FDPromise<Maybe<nsString>>> ShowFolderPickerRemote(
    HWND parent, nsTArray<filedialog::Command> const& commands) {
  using mozilla::widget::filedialog::sLogFileDialog;
  return mozilla::detail::ShowRemote([parent, commands = commands.Clone()](
                                         filedialog::WinFileDialogParent* p) {
    MOZ_LOG(sLogFileDialog, LogLevel::Info,
            ("%s: p = [%p]", __PRETTY_FUNCTION__, p));
    return p->ShowFolderDialogImpl(parent, commands);
  });
}

static RefPtr<FDPromise<Maybe<filedialog::Results>>> ShowFilePickerLocal(
    HWND parent, filedialog::FileDialogType type,
    nsTArray<filedialog::Command> const& commands) {
  return filedialog::SpawnFilePicker(parent, type, commands.Clone());
}

static RefPtr<FDPromise<Maybe<nsString>>> ShowFolderPickerLocal(
    HWND parent, nsTArray<filedialog::Command> const& commands) {
  return filedialog::SpawnFolderPicker(parent, commands.Clone());
}

}  // namespace

// fd_async
//
// Wrapper-namespace for the AsyncExecute() and AsyncAll() functions.
namespace fd_async {

// Implementation details of, specifically, the AsyncExecute() and AsyncAll()
// functions.
namespace details {
// Helper for generically copying ordinary types and nsTArray (which lacks a
// copy constructor) in the same breath.
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

template <typename T>
class AsyncAllIterator final {
 public:
  NS_INLINE_DECL_REFCOUNTING(AsyncAllIterator)
  AsyncAllIterator(
      nsTArray<T> aItems,
      std::function<
          RefPtr<mozilla::MozPromise<bool, nsresult, true>>(const T& item)>
          aPredicate,
      RefPtr<mozilla::MozPromise<bool, nsresult, true>::Private> aPromise)
      : mItems(std::move(aItems)),
        mNextIndex(0),
        mPredicate(std::move(aPredicate)),
        mPromise(std::move(aPromise)) {}

  void StartIterating() { ContinueIterating(); }

 private:
  ~AsyncAllIterator() = default;
  void ContinueIterating() {
    if (mNextIndex >= mItems.Length()) {
      mPromise->Resolve(true, __func__);
      return;
    }
    mPredicate(mItems.ElementAt(mNextIndex))
        ->Then(
            mozilla::GetMainThreadSerialEventTarget(), __func__,
            [self = RefPtr{this}](bool aResult) {
              if (!aResult) {
                self->mPromise->Resolve(false, __func__);
                return;
              }
              ++self->mNextIndex;
              self->ContinueIterating();
            },
            [self = RefPtr{this}](nsresult aError) {
              self->mPromise->Reject(aError, __func__);
            });
  }
  nsTArray<T> mItems;
  uint32_t mNextIndex;
  std::function<RefPtr<mozilla::MozPromise<bool, nsresult, true>>(
      const T& item)>
      mPredicate;
  RefPtr<mozilla::MozPromise<bool, nsresult, true>::Private> mPromise;
};

namespace telemetry {
static uint32_t Delta(uint64_t tb, uint64_t ta) {
  // FILETIMEs are 100ns intervals; we reduce that to 1ms.
  // (`u32::max()` milliseconds is roughly 47.91 days.)
  return uint32_t((tb - ta) / 10'000);
};
static nsCString HexString(uint32_t val) {
  return nsPrintfCString("%08" PRIX32, val);
};

static void RecordSuccess(uint64_t (&&time)[2]) {
  auto [t0, t1] = time;

  namespace glean_fd = mozilla::glean::file_dialog;
  glean_fd::FallbackV2Extra extra{
      .succeeded = Some(true),
      .timeLocal = Nothing(),
      .timeRemote = Some(Delta(t1, t0)),
      .whereLocal = Nothing(),
      .whereRemote = Nothing(),
      .whyLocal = Nothing(),
      .whyRemote = Nothing(),
  };
  glean_fd::fallback_v2.Record(Some(extra));
}

// for use when a local fallback is not attempted
static void RecordFailure(uint64_t (&&time)[2], Error const& remote) {
  auto [t0, t1] = time;

  namespace glean_fd = mozilla::glean::file_dialog;
  glean_fd::FallbackV2Extra extra{
      .succeeded = Some(false),
      .timeLocal = Nothing(),
      .timeRemote = Some(Delta(t1, t0)),
      .whereLocal = Nothing(),
      .whereRemote = Some(remote.where.c_str()),
      .whyLocal = Nothing(),
      .whyRemote = Some(HexString(remote.why)),
  };
  glean_fd::fallback_v2.Record(Some(extra));
}

// for use when a local fallback is attempted
static void RecordFailure(uint64_t (&&time)[3], Error const& remote,
                          Maybe<Error> const& local) {
  auto [t0, t1, t2] = time;

  namespace glean_fd = mozilla::glean::file_dialog;
  glean_fd::FallbackV2Extra extra{
      .succeeded = Some(false),
      .timeLocal = Some(Delta(t2, t1)),
      .timeRemote = Some(Delta(t1, t0)),
      .whereLocal = local.map([](auto const& e) { return e.where.c_str(); }),
      .whereRemote = Some(remote.where.c_str()),
      .whyLocal = local.map([](auto const& e) { return HexString(e.why); }),
      .whyRemote = Some(HexString(remote.why)),
  };
  glean_fd::fallback_v2.Record(Some(extra));
}

}  // namespace telemetry

/* N.B.: L and R stand for Local and Remote, not just Left and Right */
template <typename FnL, typename FnR, typename... Args>
struct AsyncExecuteInfo {
  template <typename T>
  using DestructurePromise = widget::filedialog::detail::DestructurePromise<T>;

  using Unit = ::mozilla::Ok;

  using RetL = std::invoke_result_t<FnL, Args...>;
  using RetR = std::invoke_result_t<FnR, Args...>;

  using InfoL = DestructurePromise<RetL>;
  using InfoR = DestructurePromise<RetR>;

  MOZ_ASSERT_SAME_TYPE(
      typename InfoL::ResolveT, typename InfoR::ResolveT,
      "local and remote promises must have identical resolve-types");

  // At present, the local and remote promises have the same type, but this
  // isn't logically necessary. (In particular, a future refactor may remove the
  // redundant `.kind` from the local promises' return types.)
  MOZ_ASSERT_SAME_TYPE(typename InfoL::RejectT, filedialog::Error,
                       "local promise must reject with a filedialog::Error");

  MOZ_ASSERT_SAME_TYPE(typename InfoR::RejectT, filedialog::Error,
                       "remote promise must reject with a filedialog::Error");

  using ResolveT = typename InfoL::ResolveT;
  using PromiseT = MozPromise<ResolveT, Unit, true>;

  using RetT = RefPtr<PromiseT>;
};

}  // namespace details

// Invoke either or both of a promise-returning "do locally" and "do remotely"
// function with the provided arguments, depending on the relevant preference's
// value and on whether or not the remote version fails (returns a rejection-
// promise).
//
// Both provided functions must return a `RefPtr<filedialog::MozPromise<T>>`. As
// `AsyncExecute` reports failures itself, its rejection-type is `()`.
template <typename Fn1, typename Fn2, typename... Args>
static auto AsyncExecute(Fn1 local, Fn2 remote, Args const&... args) ->
    typename details::AsyncExecuteInfo<Fn1, Fn2, Args...>::RetT {
  using namespace details;
  using Info = AsyncExecuteInfo<Fn1, Fn2, Args...>;

  using ResolveT = typename Info::ResolveT;
  using PromiseT = typename Info::PromiseT;
  using LPromiseT = typename Info::InfoL::Promise;
  using RPromiseT = typename Info::InfoR::Promise;

  constexpr static char kFunctionName[] = "LocalAndOrRemote::AsyncExecute";

  // capture time for telemetry
  constexpr static const auto GetTime = []() -> uint64_t {
    FILETIME t;
    ::GetSystemTimeAsFileTime(&t);
    return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
  };
  uint64_t const t0 = GetTime();

  switch (GetStrategy()) {
    case Local: {
      return local(args...)->MapErr(
          NS_GetCurrentThread(), __func__, [](Error const& err) {
            MOZ_ASSERT(err.kind == Error::LocalError);
            MOZ_LOG(filedialog::sLogFileDialog, LogLevel::Info,
                    ("local file-dialog failed: where=%s, why=%08" PRIX32,
                     err.where.c_str(), err.why));
            return Ok();
          });
    }

    case Remote:
      return remote(args...)->Then(
          NS_GetCurrentThread(), __func__,
          [t0](
              typename RPromiseT::ResolveValueType result) -> RefPtr<PromiseT> {
            // success; stop here
            auto const t1 = GetTime();
            // record success
            telemetry::RecordSuccess({t0, t1});
            return PromiseT::CreateAndResolve(std::move(result), kFunctionName);
          },
          [t0](Error const& err) {
            auto const t1 = GetTime();
            telemetry::RecordFailure({t0, t1}, err);

            MOZ_LOG(filedialog::sLogFileDialog, LogLevel::Info,
                    ("remote file-dialog failed: kind=%s, where=%s, "
                     "why=%08" PRIX32,
                     Error::KindName(err.kind), err.where.c_str(), err.why));
            return PromiseT::CreateAndReject(Ok(), kFunctionName);
          });

    case RemoteWithFallback:
      // more complicated; continue below
      break;
  }

  return remote(args...)->Then(
      NS_GetCurrentThread(), kFunctionName,
      [t0](typename RPromiseT::ResolveValueType result) -> RefPtr<PromiseT> {
        // success; stop here
        auto const t1 = GetTime();
        // record success
        telemetry::RecordSuccess({t0, t1});
        return PromiseT::CreateAndResolve(std::move(result), kFunctionName);
      },
      // initialized lambda pack captures are C++20 (clang 9, gcc 9);
      // `make_tuple` is just a C++17 workaround
      [=, tuple = std::make_tuple(Copy(args)...)](
          typename RPromiseT::RejectValueType err) mutable -> RefPtr<PromiseT> {
        // failure; record time
        auto const t1 = GetTime();

        // retry locally...
        auto p0 = std::apply(local, std::move(tuple));
        // ...then record the telemetry event
        return p0->Then(
            NS_GetCurrentThread(), kFunctionName,
            [t0, t1, errRemote = std::move(err)](
                typename LPromiseT::ResolveOrRejectValue&& val)
                -> RefPtr<PromiseT> {
              auto const t2 = GetTime();
              Maybe<Error> const errLocal =
                  val.IsReject() ? Some(val.RejectValue()) : Nothing();
              telemetry::RecordFailure({t0, t1, t2}, errRemote, errLocal);

              using V = typename PromiseT::ResolveOrRejectValue;
              return PromiseT::CreateAndResolveOrReject(
                  val.IsResolve()
                      ? V::MakeResolve(std::move(val).ResolveValue())
                      : V::MakeReject(Ok{}),
                  kFunctionName);
            });
      });
}

// Asynchronously invokes `aPredicate` on each member of `aItems`.
// Yields `false` (and stops immediately) if any invocation of
// `predicate` yielded `false`; otherwise yields `true`.
template <typename T>
static RefPtr<mozilla::MozPromise<bool, nsresult, true>> AsyncAll(
    nsTArray<T> aItems,
    std::function<
        RefPtr<mozilla::MozPromise<bool, nsresult, true>>(const T& item)>
        aPredicate) {
  auto promise =
      mozilla::MakeRefPtr<mozilla::MozPromise<bool, nsresult, true>::Private>(
          __func__);
  auto iterator = mozilla::MakeRefPtr<details::AsyncAllIterator<T>>(
      std::move(aItems), aPredicate, promise);
  iterator->StartIterating();
  return promise;
}
}  // namespace fd_async

using fd_async::AsyncAll;
using fd_async::AsyncExecute;

}  // namespace mozilla::detail

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
RefPtr<mozilla::MozPromise<bool, nsFilePicker::Unit, true>>
nsFilePicker::ShowFolderPicker(const nsString& aInitialDir) {
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

  return mozilla::detail::AsyncExecute(&mozilla::detail::ShowFolderPickerLocal,
                                       &mozilla::detail::ShowFolderPickerRemote,
                                       shim.get(), commands)
      ->Map(NS_GetCurrentThread(), __PRETTY_FUNCTION__,
            [self = RefPtr(this), shim = std::move(shim),
             awps = std::move(awps)](Maybe<nsString> val) {
              if (val) {
                self->mUnicodeFile = val.extract();
                return true;
              }
              return false;
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
RefPtr<mozilla::MozPromise<bool, nsFilePicker::Unit, true>>
nsFilePicker::ShowFilePicker(const nsString& aInitialDir) {
  AUTO_PROFILER_LABEL("nsFilePicker::ShowFilePicker", OTHER);

  using Promise = mozilla::MozPromise<bool, Unit, true>;
  constexpr static auto Ok = [](bool val) {
    return Promise::CreateAndResolve(val, "nsFilePicker::ShowFilePicker");
  };
  constexpr static auto NotOk = []() {
    return Promise::CreateAndReject(Unit(), "nsFilePicker::ShowFilePicker");
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
        return NotOk();
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

  auto promise = mozilla::detail::AsyncExecute(
      &mozilla::detail::ShowFilePickerLocal,
      &mozilla::detail::ShowFilePickerRemote, shim.get(), type, commands);

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
      [](Unit err) {
        NS_WARNING("ShowFilePicker failed");
        return NotOk();
      });
}

void nsFilePicker::ClearFiles() {
  mUnicodeFile.Truncate();
  mFiles.Clear();
}

namespace {
class GetFilesInDirectoryCallback final
    : public mozilla::dom::GetFilesCallback {
 public:
  explicit GetFilesInDirectoryCallback(
      RefPtr<mozilla::MozPromise<nsTArray<mozilla::PathString>, nsresult,
                                 true>::Private>
          aPromise)
      : mPromise(std::move(aPromise)) {}
  void Callback(
      nsresult aStatus,
      const FallibleTArray<RefPtr<mozilla::dom::BlobImpl>>& aBlobImpls) {
    if (NS_FAILED(aStatus)) {
      mPromise->Reject(aStatus, __func__);
      return;
    }
    nsTArray<mozilla::PathString> filePaths;
    filePaths.SetCapacity(aBlobImpls.Length());
    for (const auto& blob : aBlobImpls) {
      if (blob->IsFile()) {
        mozilla::PathString pathString;
        mozilla::ErrorResult error;
        blob->GetMozFullPathInternal(pathString, error);
        nsresult rv = error.StealNSResult();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          mPromise->Reject(rv, __func__);
          return;
        }
        filePaths.AppendElement(pathString);
      } else {
        NS_WARNING("Got a non-file blob, can't do content analysis on it");
      }
    }
    mPromise->Resolve(std::move(filePaths), __func__);
  }

 private:
  RefPtr<mozilla::MozPromise<nsTArray<mozilla::PathString>, nsresult,
                             true>::Private>
      mPromise;
};
}  // anonymous namespace

RefPtr<nsFilePicker::ContentAnalysisResponse>
nsFilePicker::CheckContentAnalysisService() {
  nsresult rv;
  nsCOMPtr<nsIContentAnalysis> contentAnalysis =
      mozilla::components::nsIContentAnalysis::Service(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsFilePicker::ContentAnalysisResponse::CreateAndReject(rv, __func__);
  }
  bool contentAnalysisIsActive = false;
  rv = contentAnalysis->GetIsActive(&contentAnalysisIsActive);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsFilePicker::ContentAnalysisResponse::CreateAndReject(rv, __func__);
  }
  if (!contentAnalysisIsActive) {
    return nsFilePicker::ContentAnalysisResponse::CreateAndResolve(true,
                                                                   __func__);
  }

  nsCOMPtr<nsIURI> uri = mBrowsingContext->Canonical()->GetCurrentURI();

  auto processOneItem = [self = RefPtr{this},
                         contentAnalysis = std::move(contentAnalysis),
                         uri =
                             std::move(uri)](const mozilla::PathString& aItem) {
    nsCString emptyDigestString;
    auto* windowGlobal =
        self->mBrowsingContext->Canonical()->GetCurrentWindowGlobal();
    nsCOMPtr<nsIContentAnalysisRequest> contentAnalysisRequest(
        new mozilla::contentanalysis::ContentAnalysisRequest(
            nsIContentAnalysisRequest::AnalysisType::eFileAttached, aItem, true,
            std::move(emptyDigestString), uri,
            nsIContentAnalysisRequest::OperationType::eCustomDisplayString,
            windowGlobal));

    auto promise =
        mozilla::MakeRefPtr<nsFilePicker::ContentAnalysisResponse::Private>(
            __func__);
    auto contentAnalysisCallback =
        mozilla::MakeRefPtr<mozilla::contentanalysis::ContentAnalysisCallback>(
            [promise](nsIContentAnalysisResponse* aResponse) {
              bool shouldAllow = false;
              mozilla::DebugOnly<nsresult> rv =
                  aResponse->GetShouldAllowContent(&shouldAllow);
              MOZ_ASSERT(NS_SUCCEEDED(rv));
              promise->Resolve(shouldAllow, __func__);
            },
            [promise](nsresult aError) { promise->Reject(aError, __func__); });

    nsresult rv = contentAnalysis->AnalyzeContentRequestCallback(
        contentAnalysisRequest, /* aAutoAcknowledge */ true,
        contentAnalysisCallback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      promise->Reject(rv, __func__);
    }
    return promise;
  };

  // Since getting the files to analyze might be asynchronous, use a MozPromise
  // to unify the logic below.
  RefPtr<mozilla::MozPromise<nsTArray<mozilla::PathString>, nsresult,
                             true>::Private>
      getFilesToAnalyzePromise = nullptr;
  if (mMode == modeGetFolder) {
    nsCOMPtr<nsIFile> file;
    nsresult rv = GetFile(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv) || !file)) {
      return nsFilePicker::ContentAnalysisResponse::CreateAndReject(rv,
                                                                    __func__);
    }
    // We just need to iterate over the directory, so use the junk scope
    RefPtr<mozilla::dom::Directory> directory = mozilla::dom::Directory::Create(
        xpc::NativeGlobal(xpc::PrivilegedJunkScope()), file);
    if (!directory) {
      // The directory may have been deleted
      return nsFilePicker::ContentAnalysisResponse::CreateAndReject(rv,
                                                                    __func__);
    }
    mozilla::dom::OwningFileOrDirectory owningDirectory;
    owningDirectory.SetAsDirectory() = directory;
    nsTArray<mozilla::dom::OwningFileOrDirectory> directoryArray{
        std::move(owningDirectory)};

    mozilla::ErrorResult error;
    RefPtr<mozilla::dom::GetFilesHelper> helper =
        mozilla::dom::GetFilesHelper::Create(directoryArray, true, error);
    rv = error.StealNSResult();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nsFilePicker::ContentAnalysisResponse::CreateAndReject(rv,
                                                                    __func__);
    }

    getFilesToAnalyzePromise = mozilla::MakeRefPtr<mozilla::MozPromise<
        nsTArray<mozilla::PathString>, nsresult, true>::Private>(__func__);
    auto getFilesCallback = mozilla::MakeRefPtr<GetFilesInDirectoryCallback>(
        getFilesToAnalyzePromise);
    helper->AddCallback(getFilesCallback);
  } else {
    nsCOMArray<nsIFile> files;
    if (!mUnicodeFile.IsEmpty()) {
      nsCOMPtr<nsIFile> file;
      rv = GetFile(getter_AddRefs(file));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nsFilePicker::ContentAnalysisResponse::CreateAndReject(rv,
                                                                      __func__);
      }
      files.AppendElement(file);
    } else {
      files.AppendElements(mFiles);
    }
    nsTArray<mozilla::PathString> paths(files.Length());
    std::transform(files.begin(), files.end(), MakeBackInserter(paths),
                   [](auto* entry) { return entry->NativePath(); });
    getFilesToAnalyzePromise = mozilla::MakeRefPtr<mozilla::MozPromise<
        nsTArray<mozilla::PathString>, nsresult, true>::Private>(__func__);
    getFilesToAnalyzePromise->Resolve(std::move(paths), __func__);
  }

  MOZ_ASSERT(getFilesToAnalyzePromise);
  return getFilesToAnalyzePromise->Then(
      mozilla::GetMainThreadSerialEventTarget(), __func__,
      [processOneItem](nsTArray<mozilla::PathString> aPaths) mutable {
        return mozilla::detail::AsyncAll<mozilla::PathString>(std::move(aPaths),
                                                              processOneItem);
      },
      [](nsresult aError) {
        return nsFilePicker::ContentAnalysisResponse::CreateAndReject(aError,
                                                                      __func__);
      });
};

///////////////////////////////////////////////////////////////////////////////
// nsIFilePicker impl.

nsresult nsFilePicker::Open(nsIFilePickerShownCallback* aCallback) {
  NS_ENSURE_ARG_POINTER(aCallback);

  if (MaybeBlockFilePicker(aCallback)) {
    return NS_OK;
  }

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
  ClearFiles();

  auto promise = mMode == modeGetFolder ? ShowFolderPicker(initialDir)
                                        : ShowFilePicker(initialDir);

  promise->Then(
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

        if (self->mBrowsingContext && !self->mBrowsingContext->IsChrome() &&
            self->mMode != modeSave && retValue != ResultCode::returnCancel) {
          self->CheckContentAnalysisService()->Then(
              mozilla::GetMainThreadSerialEventTarget(), __func__,
              [retValue, callback, self = RefPtr{self}](bool aAllowContent) {
                if (aAllowContent) {
                  callback->Done(retValue);
                } else {
                  self->ClearFiles();
                  callback->Done(ResultCode::returnCancel);
                }
              },
              [callback, self = RefPtr{self}](nsresult aError) {
                self->ClearFiles();
                callback->Done(ResultCode::returnCancel);
              });
          return;
        }

        callback->Done(retValue);
      },
      [callback = RefPtr(aCallback)](Unit _) {
        // logging already handled
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
  return mBrowsingContext && mBrowsingContext->UsePrivateBrowsing();
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
