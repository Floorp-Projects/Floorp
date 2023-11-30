/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/filedialog/WinFileDialogChild.h"

#include <combaseapi.h>
#include <objbase.h>
#include <shobjidl.h>

#include "mozilla/Assertions.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/widget/filedialog/WinFileDialogCommands.h"
#include "nsPrintfCString.h"

static mozilla::LazyLogModule sLogFileDialog("FileDialog");

namespace mozilla::widget::filedialog {

WinFileDialogChild::WinFileDialogChild() {
  MOZ_LOG(sLogFileDialog, LogLevel::Info, ("%s %p", __PRETTY_FUNCTION__, this));
};

WinFileDialogChild::~WinFileDialogChild() {
  MOZ_LOG(sLogFileDialog, LogLevel::Info, ("%s %p", __PRETTY_FUNCTION__, this));
};

#define MOZ_ABORT_IF_ALREADY_USED()                                            \
  do {                                                                         \
    MOZ_RELEASE_ASSERT(                                                        \
        !mUsed, "called Show* twice on a single WinFileDialog instance");      \
    MOZ_LOG(                                                                   \
        sLogFileDialog, LogLevel::Info,                                        \
        ("%s %p: first call to a Show* function", __PRETTY_FUNCTION__, this)); \
    mUsed = true;                                                              \
  } while (0)

template <size_t N>
WinFileDialogChild::IPCResult WinFileDialogChild::MakeIpcFailure(
    HRESULT hr, const char (&what)[N]) {
  // The crash-report annotator stringifies integer values anyway. We do so
  // eagerly here to avoid questions about C int/long conversion semantics.
  nsPrintfCString data("%lu", hr);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::WindowsFileDialogErrorCode, data);

  return IPC_FAIL(this, what);
}

#define MOZ_IPC_ENSURE_HRESULT_OK(hr, what)             \
  do {                                                  \
    MOZ_LOG(sLogFileDialog, LogLevel::Verbose,          \
            ("checking HRESULT for %s", what));         \
    HRESULT const _hr_ = (hr);                          \
    if (FAILED(_hr_)) {                                 \
      MOZ_LOG(sLogFileDialog, LogLevel::Error,          \
              ("HRESULT %8lX while %s", (hr), (what))); \
      return MakeIpcFailure(_hr_, (what));              \
    }                                                   \
  } while (0)

WinFileDialogChild::IPCResult WinFileDialogChild::RecvShowFileDialog(
    uintptr_t parentHwnd, FileDialogType type, nsTArray<Command> commands,
    FileResolver&& resolver) {
  MOZ_ABORT_IF_ALREADY_USED();

  // create dialog of the relevant type
  RefPtr<IFileDialog> dialog;
  {
    auto res = MakeFileDialog(type);
    if (res.isErr()) {
      return MakeIpcFailure(res.unwrapErr(), "creating file dialog");
    }
    dialog = res.unwrap();
  }

  MOZ_IPC_ENSURE_HRESULT_OK(ApplyCommands(dialog.get(), commands),
                            "applying commands");

  auto const hr = dialog->Show((HWND)parentHwnd);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    resolver(Nothing());
    return IPC_OK();
  }
  MOZ_IPC_ENSURE_HRESULT_OK(hr, "showing file dialog");

  {
    auto res = GetFileResults(dialog.get());
    if (res.isErr()) {
      return MakeIpcFailure(res.unwrapErr(), "GetFileResults");
    }
    resolver(Some(res.unwrap()));
  }

  return IPC_OK();
}

WinFileDialogChild::IPCResult WinFileDialogChild::RecvShowFolderDialog(
    uintptr_t parentHwnd, nsTArray<Command> commands,
    FolderResolver&& resolver) {
  MOZ_ABORT_IF_ALREADY_USED();

  RefPtr<IFileDialog> dialog;
  {
    auto res = MakeFileDialog(FileDialogType::Open);
    if (res.isErr()) {
      return MakeIpcFailure(res.unwrapErr(), "creating file dialog");
    }
    dialog = res.unwrap();
  }

  MOZ_IPC_ENSURE_HRESULT_OK(ApplyCommands(dialog.get(), commands),
                            "applying commands");

  {
    auto const hr = dialog->Show((HWND)parentHwnd);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
      resolver(Nothing());
      return IPC_OK();
    }
    MOZ_IPC_ENSURE_HRESULT_OK(hr, "showing folder dialog");
  }

  {
    auto res = GetFolderResults(dialog.get());
    if (res.isErr()) {
      return MakeIpcFailure(res.unwrapErr(), "GetFolderResults");
    }
    resolver(Some(res.unwrap()));
  }

  return IPC_OK();
}

#undef MOZ_IPC_ENSURE_HRESULT_OK

void WinFileDialogChild::ProcessingError(Result aCode, const char* aReason) {
  detail::LogProcessingError(sLogFileDialog, this, aCode, aReason);
}

}  // namespace mozilla::widget::filedialog
