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

mozilla::LazyLogModule sLogWFD("FileDialog");

namespace mozilla::widget::filedialog {

static size_t sOpenDialogActors = 0;

WinFileDialogParent::WinFileDialogParent() {
  MOZ_LOG(sLogWFD, LogLevel::Info, ("%s %p", __PRETTY_FUNCTION__, this));
}

WinFileDialogParent::~WinFileDialogParent() {
  MOZ_LOG(sLogWFD, LogLevel::Info, ("%s %p", __PRETTY_FUNCTION__, this));
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

void WinFileDialogParent::ProcessingError(Result aCode, const char* aReason) {
  detail::LogProcessingError(sLogWFD, this, aCode, aReason);
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
    MOZ_LOG(
        sLogWFD, LogLevel::Info,
        ("%s: killing the WINDOWS_FILE_DIALOG process (no more live actors)",
         __PRETTY_FUNCTION__));
    ipc::UtilityProcessManager::GetSingleton()->CleanShutdown(
        ipc::SandboxingKind::WINDOWS_FILE_DIALOG);
  }
}
}  // namespace mozilla::widget::filedialog
