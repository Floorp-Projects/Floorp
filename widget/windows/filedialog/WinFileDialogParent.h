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

  void ProcessingError(Result aCode, const char* aReason) override;
};

// Proxy for the WinFileDialog process and actor.
//
// The IPC subsystem holds a strong reference to all IPC actors, so releasing
// the last RefPtr for such an actor does not actually cause the actor to be
// destroyed. Similarly, the UtilityProcessManager owns the host process for an
// actor, and merely destroying all actors within that host process will not
// cause it to be reaped.
//
// This object, then, acts as a proxy for those objects' lifetimes: when the
// last reference to `Contents` is released, the necessary explicit cleanup of
// the actor (and, if possible, the host process) will be performed.
class ProcessProxy {
 public:
  using WFDP = WinFileDialogParent;

  explicit ProcessProxy(RefPtr<WFDP>&& obj);
  ~ProcessProxy() = default;

  explicit operator bool() const { return data->ptr && data->ptr->CanSend(); }
  bool operator!() const { return !bool(*this); }

  WFDP& operator*() const { return *data->ptr; }
  WFDP* operator->() const { return data->ptr; }
  WFDP* get() const { return data->ptr; }

  ProcessProxy(ProcessProxy const& that) = default;
  ProcessProxy(ProcessProxy&&) = default;

 private:
  struct Contents {
    NS_INLINE_DECL_REFCOUNTING(Contents);

   public:
    explicit Contents(RefPtr<WFDP>&& obj);
    RefPtr<WFDP> const ptr;

   private:
    ~Contents();
  };
  // guaranteed nonnull
  RefPtr<Contents> data;
};

}  // namespace mozilla::widget::filedialog

#endif  // widget_windows_filedialog_WinFileDialogParent_h__
