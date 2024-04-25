/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/filedialog/WinFileDialogParent.h"

#include "mozilla/Logging.h"
#include "mozilla/Result.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "nsISupports.h"

namespace mozilla::widget::filedialog {

// Count of currently-open file dialogs (not just open-file dialogs).
static size_t sOpenDialogActors = 0;

WinFileDialogParent::WinFileDialogParent() {
  MOZ_LOG(sLogFileDialog, LogLevel::Debug,
          ("%s %p", __PRETTY_FUNCTION__, this));
}

WinFileDialogParent::~WinFileDialogParent() {
  MOZ_LOG(sLogFileDialog, LogLevel::Debug,
          ("%s %p", __PRETTY_FUNCTION__, this));
}

PWinFileDialogParent::nsresult WinFileDialogParent::BindToUtilityProcess(
    mozilla::ipc::UtilityProcessParent* aUtilityParent) {
  Endpoint<PWinFileDialogParent> parentEnd;
  Endpoint<PWinFileDialogChild> childEnd;
  nsresult rv = PWinFileDialog::CreateEndpoints(base::GetCurrentProcId(),
                                                aUtilityParent->OtherPid(),
                                                &parentEnd, &childEnd);

  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "Protocol endpoints failure");
    return NS_ERROR_FAILURE;
  }

  if (!aUtilityParent->SendStartWinFileDialogService(std::move(childEnd))) {
    MOZ_ASSERT(false, "SendStartWinFileDialogService failed");
    return NS_ERROR_FAILURE;
  }

  if (!parentEnd.Bind(this)) {
    MOZ_ASSERT(false, "parentEnd.Bind failed");
    return NS_ERROR_FAILURE;
  }

  sOpenDialogActors++;
  return NS_OK;
}

// Convert the raw IPC promise-type to a filedialog::Promise.
template <typename T, size_t N>
static auto ConvertToFDPromise(
    const char (&aMethod)[N],  // __func__
    RefPtr<MozPromise<T, mozilla::ipc::ResponseRejectReason, true>> aSrcPromise)
    -> RefPtr<MozPromise<T, Error, true>> {
  using DstPromiseT = MozPromise<T, Error, true>;
  using SrcPromiseT = MozPromise<T, mozilla::ipc::ResponseRejectReason, true>;

  // a note to the reader:
  static_assert(std::is_same_v<DstPromiseT, Promise<T>>);

  return aSrcPromise->MapErr(mozilla::GetCurrentSerialEventTarget(), aMethod,
                             [=](typename SrcPromiseT::RejectValueType&& val) {
                               return Error{.kind = Error::IPCError,
                                            .where = "IPC"_ns,
                                            .why = (uint32_t)val};
                             });
}

[[nodiscard]] RefPtr<WinFileDialogParent::ShowFileDialogPromise>
WinFileDialogParent::ShowFileDialogImpl(HWND parent, const FileDialogType& type,
                                        mozilla::Span<Command const> commands) {
  auto inner_promise = PWinFileDialogParent::SendShowFileDialog(
      reinterpret_cast<WindowsHandle>(parent), type, std::move(commands));

  return ConvertToFDPromise(__func__, std::move(inner_promise));
}

[[nodiscard]] RefPtr<WinFileDialogParent::ShowFolderDialogPromise>
WinFileDialogParent::ShowFolderDialogImpl(
    HWND parent, mozilla::Span<Command const> commands) {
  auto inner_promise = PWinFileDialogParent::SendShowFolderDialog(
      reinterpret_cast<WindowsHandle>(parent), std::move(commands));

  return ConvertToFDPromise(__func__, std::move(inner_promise));
}

void WinFileDialogParent::ProcessingError(Result aCode, const char* aReason) {
  detail::LogProcessingError(sLogFileDialog, this, aCode, aReason);
}

ProcessProxy::ProcessProxy(RefPtr<WFDP>&& obj)
    : data(MakeRefPtr<Contents>(std::move(obj))) {}

ProcessProxy::Contents::Contents(RefPtr<WFDP>&& obj) : ptr(std::move(obj)) {}

ProcessProxy::Contents::~Contents() {
  AssertIsOnMainThread();

  // destroy the actor...
  ptr->Close();

  // ... and possibly the process
  if (!--sOpenDialogActors) {
    StopProcess();
  }
}

void ProcessProxy::Contents::StopProcess() {
  auto const upm = ipc::UtilityProcessManager::GetSingleton();
  if (!upm) {
    // This is only possible when the UtilityProcessManager has shut down -- in
    // which case the file-dialog process has also already been directed to shut
    // down, and there's nothing we need to do here.
    return;
  }

  MOZ_LOG(sLogFileDialog, LogLevel::Debug,
          ("%s: killing the WINDOWS_FILE_DIALOG process (no more live "
           "actors)",
           __PRETTY_FUNCTION__));
  upm->CleanShutdown(ipc::SandboxingKind::WINDOWS_FILE_DIALOG);
}

}  // namespace mozilla::widget::filedialog
