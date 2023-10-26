/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/filedialog/WinFileDialogCommands.h"

#include <shobjidl.h>
#include <shtypes.h>
#include <winerror.h>
#include "WinUtils.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WinHeaderOnlyUtils.h"

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

}  // namespace mozilla::widget::filedialog
