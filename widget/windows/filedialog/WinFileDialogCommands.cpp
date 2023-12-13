/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/filedialog/WinFileDialogCommands.h"

#include <type_traits>
#include <shobjidl.h>
#include <shtypes.h>
#include <winerror.h>
#include "WinUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/mscom/ApartmentRegion.h"
#include "nsThreadUtils.h"

namespace mozilla::widget::filedialog {

// Visitor to apply commands to the dialog.
struct Applicator {
  IFileDialog* dialog = nullptr;

  HRESULT Visit(Command const& c) {
    switch (c.type()) {
      default:
      case Command::T__None:
        return E_INVALIDARG;

      case Command::TSetOptions:
        return Apply(c.get_SetOptions());
      case Command::TSetTitle:
        return Apply(c.get_SetTitle());
      case Command::TSetOkButtonLabel:
        return Apply(c.get_SetOkButtonLabel());
      case Command::TSetFolder:
        return Apply(c.get_SetFolder());
      case Command::TSetFileName:
        return Apply(c.get_SetFileName());
      case Command::TSetDefaultExtension:
        return Apply(c.get_SetDefaultExtension());
      case Command::TSetFileTypes:
        return Apply(c.get_SetFileTypes());
      case Command::TSetFileTypeIndex:
        return Apply(c.get_SetFileTypeIndex());
    }
  }

  HRESULT Apply(SetOptions const& c) { return dialog->SetOptions(c.options()); }
  HRESULT Apply(SetTitle const& c) { return dialog->SetTitle(c.title().get()); }
  HRESULT Apply(SetOkButtonLabel const& c) {
    return dialog->SetOkButtonLabel(c.label().get());
  }
  HRESULT Apply(SetFolder const& c) {
    RefPtr<IShellItem> folder;
    if (SUCCEEDED(SHCreateItemFromParsingName(
            c.path().get(), nullptr, IID_IShellItem, getter_AddRefs(folder)))) {
      return dialog->SetFolder(folder);
    }
    // graciously accept that the provided path may have been nonsense
    return S_OK;
  }
  HRESULT Apply(SetFileName const& c) {
    return dialog->SetFileName(c.filename().get());
  }
  HRESULT Apply(SetDefaultExtension const& c) {
    return dialog->SetDefaultExtension(c.extension().get());
  }
  HRESULT Apply(SetFileTypes const& c) {
    std::vector<COMDLG_FILTERSPEC> vec;
    for (auto const& filter : c.filterList()) {
      vec.push_back(
          {.pszName = filter.name().get(), .pszSpec = filter.spec().get()});
    }
    return dialog->SetFileTypes(vec.size(), vec.data());
  }
  HRESULT Apply(SetFileTypeIndex const& c) {
    return dialog->SetFileTypeIndex(c.index());
  }
};

namespace {
static HRESULT GetShellItemPath(IShellItem* aItem, nsString& aResultString) {
  NS_ENSURE_TRUE(aItem, E_INVALIDARG);

  mozilla::UniquePtr<wchar_t, CoTaskMemFreeDeleter> str;
  HRESULT const hr =
      aItem->GetDisplayName(SIGDN_FILESYSPATH, getter_Transfers(str));
  if (SUCCEEDED(hr)) {
    aResultString.Assign(str.get());
  }
  return hr;
}
}  // namespace

#define MOZ_ENSURE_HRESULT_OK(call_)            \
  do {                                          \
    HRESULT const _tmp_hr_ = (call_);           \
    if (FAILED(_tmp_hr_)) return Err(_tmp_hr_); \
  } while (0)

mozilla::Result<RefPtr<IFileDialog>, HRESULT> MakeFileDialog(
    FileDialogType type) {
  RefPtr<IFileDialog> dialog;

  CLSID const clsid = type == FileDialogType::Open ? CLSID_FileOpenDialog
                                                   : CLSID_FileSaveDialog;
  HRESULT const hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_IFileDialog, getter_AddRefs(dialog));
  MOZ_ENSURE_HRESULT_OK(hr);

  return std::move(dialog);
}

HRESULT ApplyCommands(::IFileDialog* dialog,
                      nsTArray<Command> const& commands) {
  Applicator applicator{.dialog = dialog};
  for (auto const& cmd : commands) {
    HRESULT const hr = applicator.Visit(cmd);
    if (FAILED(hr)) {
      return hr;
    }
  }
  return S_OK;
}

mozilla::Result<Results, HRESULT> GetFileResults(::IFileDialog* dialog) {
  FILEOPENDIALOGOPTIONS fos;
  MOZ_ENSURE_HRESULT_OK(dialog->GetOptions(&fos));

  using widget::WinUtils;

  // Extract which filter type the user selected
  UINT index;
  MOZ_ENSURE_HRESULT_OK(dialog->GetFileTypeIndex(&index));

  // single selection
  if ((fos & FOS_ALLOWMULTISELECT) == 0) {
    RefPtr<IShellItem> item;
    MOZ_ENSURE_HRESULT_OK(dialog->GetResult(getter_AddRefs(item)));
    if (!item) {
      return Err(E_FAIL);
    }

    nsAutoString path;
    MOZ_ENSURE_HRESULT_OK(GetShellItemPath(item, path));

    return Results({path}, index);
  }

  // multiple selection
  RefPtr<IFileOpenDialog> openDlg;
  dialog->QueryInterface(IID_IFileOpenDialog, getter_AddRefs(openDlg));
  if (!openDlg) {
    MOZ_ASSERT(false, "a file-save dialog was given FOS_ALLOWMULTISELECT?");
    return Err(E_UNEXPECTED);
  }

  RefPtr<IShellItemArray> items;
  MOZ_ENSURE_HRESULT_OK(openDlg->GetResults(getter_AddRefs(items)));
  if (!items) {
    return Err(E_FAIL);
  }

  nsTArray<nsString> paths;

  DWORD count = 0;
  MOZ_ENSURE_HRESULT_OK(items->GetCount(&count));
  for (DWORD idx = 0; idx < count; idx++) {
    RefPtr<IShellItem> item;
    MOZ_ENSURE_HRESULT_OK(items->GetItemAt(idx, getter_AddRefs(item)));

    nsAutoString str;
    MOZ_ENSURE_HRESULT_OK(GetShellItemPath(item, str));

    paths.EmplaceBack(str);
  }

  return Results(std::move(paths), std::move(index));
}

mozilla::Result<nsString, HRESULT> GetFolderResults(::IFileDialog* dialog) {
  RefPtr<IShellItem> item;
  MOZ_ENSURE_HRESULT_OK(dialog->GetResult(getter_AddRefs(item)));
  if (!item) {
    // shouldn't happen -- probably a precondition failure on our part, but
    // might be due to misbehaving shell extensions?
    MOZ_ASSERT(false,
               "unexpected lack of item: was `Show`'s return value checked?");
    return Err(E_FAIL);
  }

  // If the user chose a Win7 Library, resolve to the library's
  // default save folder.
  RefPtr<IShellLibrary> shellLib;
  RefPtr<IShellItem> folderPath;
  MOZ_ENSURE_HRESULT_OK(
      CoCreateInstance(CLSID_ShellLibrary, nullptr, CLSCTX_INPROC_SERVER,
                       IID_IShellLibrary, getter_AddRefs(shellLib)));

  if (shellLib && SUCCEEDED(shellLib->LoadLibraryFromItem(item, STGM_READ)) &&
      SUCCEEDED(shellLib->GetDefaultSaveFolder(DSFT_DETECT, IID_IShellItem,
                                               getter_AddRefs(folderPath)))) {
    item.swap(folderPath);
  }

  // get the folder's file system path
  nsAutoString str;
  MOZ_ENSURE_HRESULT_OK(GetShellItemPath(item, str));
  return str;
}

#undef MOZ_ENSURE_HRESULT_OK

namespace detail {
void LogProcessingError(LogModule* aModule, ipc::IProtocol* aCaller,
                        ipc::HasResultCodes::Result aCode,
                        const char* aReason) {
  LogLevel const level = [&]() {
    switch (aCode) {
      case ipc::HasResultCodes::MsgProcessed:
        // Normal operation. (We probably never actually get this code.)
        return LogLevel::Verbose;

      case ipc::HasResultCodes::MsgDropped:
        return LogLevel::Verbose;

      default:
        return LogLevel::Error;
    }
  }();

  // Processing errors are sometimes unhelpfully formatted. We can't fix that
  // directly because the unhelpful formatting has made its way to telemetry
  // (table `telemetry.socorro_crash`, column `ipc_channel_error`) and is being
  // aggregated on. :(
  nsCString reason(aReason);
  if (reason.Last() == '\n') {
    reason.Truncate(reason.Length() - 1);
  }

  if (MOZ_LOG_TEST(aModule, level)) {
    const char* const side = [&]() {
      switch (aCaller->GetSide()) {
        case ipc::ParentSide:
          return "parent";
        case ipc::ChildSide:
          return "child";
        case ipc::UnknownSide:
          return "unknown side";
        default:
          return "<illegal value>";
      }
    }();

    const char* const errorStr = [&]() {
      switch (aCode) {
        case ipc::HasResultCodes::MsgProcessed:
          return "Processed";
        case ipc::HasResultCodes::MsgDropped:
          return "Dropped";
        case ipc::HasResultCodes::MsgNotKnown:
          return "NotKnown";
        case ipc::HasResultCodes::MsgNotAllowed:
          return "NotAllowed";
        case ipc::HasResultCodes::MsgPayloadError:
          return "PayloadError";
        case ipc::HasResultCodes::MsgProcessingError:
          return "ProcessingError";
        case ipc::HasResultCodes::MsgRouteError:
          return "RouteError";
        case ipc::HasResultCodes::MsgValueError:
          return "ValueError";
        default:
          return "<illegal error type>";
      }
    }();

    MOZ_LOG(aModule, level,
            ("%s [%s]: IPC error (%s): %s", aCaller->GetProtocolName(), side,
             errorStr, reason.get()));
  }

  if (level == LogLevel::Error) {
    // kill the child process...
    if (aCaller->GetSide() == ipc::ParentSide) {
      // ... which isn't us
      ipc::UtilityProcessManager::GetSingleton()->CleanShutdown(
          ipc::SandboxingKind::WINDOWS_FILE_DIALOG);
    } else {
      // ... which (presumably) is us
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::ipc_channel_error, reason);

      MOZ_CRASH("IPC error");
    }
  }
}

// Given a (synchronous) Action returning a Result<T, HRESULT>, perform that
// action on a new single-purpose "File Dialog" thread, with COM initialized as
// STA. (The thread will be destroyed afterwards.)
//
// Returns a Promise which will resolve to T (if the action returns Ok) or
// reject with an HRESULT (if the action either returns Err or couldn't be
// performed).
template <typename Res, typename Action, size_t N>
RefPtr<Promise<Res>> SpawnFileDialogThread(const char (&where)[N],
                                           Action action) {
  RefPtr<nsIThread> thread;
  {
    nsresult rv = NS_NewNamedThread("File Dialog", getter_AddRefs(thread),
                                    nullptr, {.isUiThread = true});
    if (NS_FAILED(rv)) {
      return Promise<Res>::CreateAndReject((HRESULT)rv, where);
    }
  }
  // `thread` is single-purpose, and should not perform any additional work
  // after `action`. Shut it down after we've dispatched that.
  auto close_thread_ = MakeScopeExit([&]() {
    auto const res = thread->AsyncShutdown();
    static_assert(
        std::is_same_v<uint32_t, std::underlying_type_t<decltype(res)>>);
    if (NS_FAILED(res)) {
      MOZ_LOG(sLogFileDialog, LogLevel::Warning,
              ("thread->AsyncShutdown() failed: res=0x%08" PRIX32,
               static_cast<uint32_t>(res)));
    }
  });

  // our eventual return value
  RefPtr promise = MakeRefPtr<typename Promise<Res>::Private>(where);

  // alias to reduce indentation depth
  auto const dispatch = [&](auto closure) {
    return thread->DispatchToQueue(
        NS_NewRunnableFunction(where, std::move(closure)),
        mozilla::EventQueuePriority::Normal);
  };

  dispatch([thread, promise, where, action = std::move(action)]() {
    // Like essentially all COM UI components, the file dialog is STA: it must
    // be associated with a specific thread to create its HWNDs and receive
    // messages for them. If it's launched from a thread in the multithreaded
    // apartment (including via implicit MTA), COM will proxy out to the
    // process's main STA thread, and the file-dialog's modal loop will run
    // there.
    //
    // This of course would completely negate any point in using a separate
    // thread, since behind the scenes the dialog would still be running on the
    // process's main thread. In particular, under that arrangement, file
    // dialogs (and other nested modal loops, like those performed by
    // `SpinEventLoopUntil`) will resolve in strictly LIFO order, effectively
    // remaining suspended until all later modal loops resolve.
    //
    // To avoid this, we initialize COM as STA, so that it (rather than the main
    // STA thread) is the file dialog's "home" thread and the IFileDialog's home
    // apartment.

    mozilla::mscom::STARegion staRegion;
    if (!staRegion) {
      MOZ_LOG(sLogFileDialog, LogLevel::Error,
              ("COM init failed on file dialog thread: hr = %08lx",
               staRegion.GetHResult()));

      APTTYPE at;
      APTTYPEQUALIFIER atq;
      HRESULT const hr = ::CoGetApartmentType(&at, &atq);
      MOZ_LOG(sLogFileDialog, LogLevel::Error,
              ("  current COM apartment state: hr = %08lX, APTTYPE = "
               "%08X, APTTYPEQUALIFIER = %08X",
               hr, at, atq));

      // If this happens in the utility process, crash so we learn about it.
      // (TODO: replace this with a telemetry ping.)
      if (!XRE_IsParentProcess()) {
        // Preserve relevant data on the stack for later analysis.
        std::tuple volatile info{staRegion.GetHResult(), hr, at, atq};
        MOZ_CRASH("Could not initialize COM STA in utility process");
      }

      // If this happens in the parent process, don't crash; just fall back to a
      // nested modal loop. This isn't ideal, but it will probably still work
      // well enough for the common case, wherein no other modal loops are
      // active.
      //
      // (TODO: replace this with a telemetry ping, too.)
    }

    // Actually invoke the action and report the result.
    Result<Res, HRESULT> val = action();
    if (val.isErr()) {
      promise->Reject(val.unwrapErr(), where);
    } else {
      promise->Resolve(val.unwrap(), where);
    }
  });

  return promise;
}

// For F returning `Result<T, E>`, yields the type `T`.
template <typename F, typename... Args>
using inner_result_of =
    typename std::remove_reference_t<decltype(std::declval<F>()(
        std::declval<Args>()...))>::ok_type;

template <typename ExtractorF,
          typename RetT = inner_result_of<ExtractorF, IFileDialog*>>
auto SpawnPickerT(HWND parent, FileDialogType type, ExtractorF&& extractor,
                  nsTArray<Command> commands) -> RefPtr<Promise<Maybe<RetT>>> {
  return detail::SpawnFileDialogThread<Maybe<RetT>>(
      __PRETTY_FUNCTION__,
      [=, commands = std::move(commands)]() -> Result<Maybe<RetT>, HRESULT> {
        // On Win10, the picker doesn't support per-monitor DPI, so we create it
        // with our context set temporarily to system-dpi-aware.
        WinUtils::AutoSystemDpiAware dpiAwareness;

        RefPtr<IFileDialog> dialog;
        MOZ_TRY_VAR(dialog, MakeFileDialog(type));

        if (HRESULT const rv = ApplyCommands(dialog, commands); FAILED(rv)) {
          return mozilla::Err(rv);
        }

        if (HRESULT const rv = dialog->Show(parent); FAILED(rv)) {
          if (rv == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            return Result<Maybe<RetT>, HRESULT>(Nothing());
          }
          return mozilla::Err(rv);
        }

        RetT res;
        MOZ_TRY_VAR(res, extractor(dialog.get()));

        return Some(res);
      });
}

}  // namespace detail

RefPtr<Promise<Maybe<Results>>> SpawnFilePicker(HWND parent,
                                                FileDialogType type,
                                                nsTArray<Command> commands) {
  return detail::SpawnPickerT(parent, type, GetFileResults,
                              std::move(commands));
}

RefPtr<Promise<Maybe<nsString>>> SpawnFolderPicker(HWND parent,
                                                   nsTArray<Command> commands) {
  return detail::SpawnPickerT(parent, FileDialogType::Open, GetFolderResults,
                              std::move(commands));
}

}  // namespace mozilla::widget::filedialog
