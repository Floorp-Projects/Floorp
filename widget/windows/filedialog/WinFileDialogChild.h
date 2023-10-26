/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_filedialog_WinFileDialogChild_h__
#define widget_windows_filedialog_WinFileDialogChild_h__

#include "mozilla/widget/filedialog/PWinFileDialogChild.h"

// forward declaration of native Windows interface-struct
struct IFileDialog;

namespace mozilla::widget::filedialog {

class WinFileDialogChild : public PWinFileDialogChild {
 public:
  using Command = mozilla::widget::filedialog::Command;
  using IPCResult = mozilla::ipc::IPCResult;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WinFileDialogChild, override);

  WinFileDialogChild();

 public:
  using FileResolver = PWinFileDialogChild::ShowFileDialogResolver;
  IPCResult RecvShowFileDialog(uintptr_t parentHwnd, FileDialogType,
                               nsTArray<Command>, FileResolver&&);

  using FolderResolver = PWinFileDialogChild::ShowFolderDialogResolver;
  IPCResult RecvShowFolderDialog(uintptr_t parentHwnd, nsTArray<Command>,
                                 FolderResolver&&);

 private:
  ~WinFileDialogChild();

  void ProcessingError(Result aCode, const char* aReason) override;

  // Defined and used only in WinFileDialogChild.cpp.
  template <size_t N>
  IPCResult MakeIpcFailure(HRESULT hr, const char (&what)[N]);

  // This flag properly _should_ be static (_i.e._, per-process) rather than
  // per-instance; but we can't presently instantiate two separate utility
  // processes with the same sandbox type, so we have to reuse the existing
  // utility process if there is one.
  bool mUsed = false;
};

}  // namespace mozilla::widget::filedialog

#endif  // widget_windows_filedialog_WinFileDialogChild_h__
