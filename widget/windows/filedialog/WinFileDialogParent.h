/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_filedialog_WinFileDialogParent_h__
#define widget_windows_filedialog_WinFileDialogParent_h__

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ProcInfo.h"
#include "mozilla/Result.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/widget/filedialog/PWinFileDialogParent.h"
#include "nsISupports.h"
#include "nsStringFwd.h"

namespace mozilla::widget::filedialog {

class WinFileDialogParent : public PWinFileDialogParent {
 public:
  using UtilityActorName = ::mozilla::UtilityActorName;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WinFileDialogParent, override);

 public:
  WinFileDialogParent();
  nsresult BindToUtilityProcess(
      mozilla::ipc::UtilityProcessParent* aUtilityParent);

  UtilityActorName GetActorName() {
    return UtilityActorName::WindowsFileDialog;
  }

 private:
  ~WinFileDialogParent();
};
}  // namespace mozilla::widget::filedialog

#endif  // widget_windows_filedialog_WinFileDialogParent_h__
