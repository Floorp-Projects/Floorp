/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_filedialog_WinFileDialogCommands_h__
#define widget_windows_filedialog_WinFileDialogCommands_h__

#include "ipc/EnumSerializer.h"
#include "mozilla/ipc/MessageLink.h"
#include "mozilla/widget/filedialog/WinFileDialogCommandsDefn.h"

// Windows interface types, defined in <shobjidl.h>
struct IFileDialog;
struct IFileOpenDialog;

namespace mozilla::widget::filedialog {

enum class FileDialogType : uint8_t { Open, Save };

// Create a file-dialog of the relevant type. Requires MSCOM to be initialized.
mozilla::Result<RefPtr<IFileDialog>, HRESULT> MakeFileDialog(FileDialogType);

// Apply the selected commands to the IFileDialog, in preparation for showing
// it. (The actual showing step is left to the caller.)
[[nodiscard]] HRESULT ApplyCommands(::IFileDialog*,
                                    nsTArray<Command> const& commands);

// Extract one or more results from the file-picker dialog.
//
// Requires that Show() has been called and has returned S_OK.
mozilla::Result<Results, HRESULT> GetFileResults(::IFileDialog*);

// Extract the chosen folder from the folder-picker dialog.
//
// Requires that Show() has been called and has returned S_OK.
mozilla::Result<nsString, HRESULT> GetFolderResults(::IFileDialog*);

namespace detail {
// Log the error. If it's a notable error, kill the child process.
void LogProcessingError(LogModule* aModule, ipc::IProtocol* aCaller,
                        ipc::HasResultCodes::Result aCode, const char* aReason);
}  // namespace detail

}  // namespace mozilla::widget::filedialog

namespace IPC {
template <>
struct ParamTraits<mozilla::widget::filedialog::FileDialogType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::widget::filedialog::FileDialogType,
          mozilla::widget::filedialog::FileDialogType::Open,
          mozilla::widget::filedialog::FileDialogType::Save> {};
}  // namespace IPC

#endif  // widget_windows_filedialog_WinFileDialogCommands_h__
