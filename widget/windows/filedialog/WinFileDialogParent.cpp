/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/filedialog/WinFileDialogParent.h"

#include "mozilla/Logging.h"
#include "mozilla/Result.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsISupports.h"

mozilla::LazyLogModule sLogWFD("FileDialog");

namespace mozilla::widget::filedialog {

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

  return NS_OK;
}
}  // namespace mozilla::widget::filedialog
