/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AboutThirdParty.h"

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace {

class FileDialogEventsForTesting final : public IFileDialogEvents {
  mozilla::Atomic<uint32_t> mRefCnt;
  const nsString mTargetName;
  RefPtr<IShellItem> mTargetDir;

  ~FileDialogEventsForTesting() = default;

 public:
  FileDialogEventsForTesting(const nsAString& aTargetName,
                             IShellItem* aTargetDir)
      : mRefCnt(0),
        mTargetName(PromiseFlatString(aTargetName)),
        mTargetDir(aTargetDir) {}

  // IUnknown

  STDMETHODIMP QueryInterface(REFIID aRefIID, void** aResult) {
    if (!aResult) {
      return E_INVALIDARG;
    }

    if (aRefIID == IID_IFileDialogEvents) {
      RefPtr ref(static_cast<IFileDialogEvents*>(this));
      ref.forget(aResult);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() { return ++mRefCnt; }

  STDMETHODIMP_(ULONG) Release() {
    ULONG result = --mRefCnt;
    if (!result) {
      delete this;
    }
    return result;
  }

  // IFileDialogEvents

  STDMETHODIMP OnFileOk(IFileDialog*) { return E_NOTIMPL; }
  STDMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) { return E_NOTIMPL; }
  STDMETHODIMP OnShareViolation(IFileDialog*, IShellItem*,
                                FDE_SHAREVIOLATION_RESPONSE*) {
    return E_NOTIMPL;
  }
  STDMETHODIMP OnTypeChange(IFileDialog*) { return E_NOTIMPL; }
  STDMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) {
    return E_NOTIMPL;
  }
  STDMETHODIMP OnFolderChange(IFileDialog*) { return E_NOTIMPL; }
  STDMETHODIMP OnSelectionChange(IFileDialog* aDialog) {
    if (::GetModuleHandleW(mTargetName.get())) {
      aDialog->Close(S_OK);
    } else {
      // This sends a notification which is processed asynchronously.  Calling
      // SetFolder from OnSelectionChange gives the main thread some cycles to
      // process other window messages, while calling SetFolder from
      // OnFolderChange causes freeze.  Thus we can safely wait until a common
      // dialog loads a shell extension without blocking UI.
      aDialog->SetFolder(mTargetDir);
    }

    return E_NOTIMPL;
  }
};

}  // anonymous namespace

namespace mozilla {

NS_IMETHODIMP AboutThirdParty::OpenAndCloseFileDialogForTesting(
    const nsAString& aModuleName, const nsAString& aInitialDir,
    const nsAString& aFilter) {
  // Notify the shell of a new icon handler which should have been registered
  // by the test script.
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  RefPtr<IFileOpenDialog> dialog;
  if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                CLSCTX_INPROC_SERVER, IID_IFileOpenDialog,
                                getter_AddRefs(dialog)))) {
    return NS_ERROR_UNEXPECTED;
  }

  const nsString& filter = PromiseFlatString(aFilter);
  COMDLG_FILTERSPEC fileFilter = {L"Test Target", filter.get()};
  if (FAILED(dialog->SetFileTypes(1, &fileFilter))) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<IShellItem> folder;
  if (FAILED(::SHCreateItemFromParsingName(PromiseFlatString(aInitialDir).get(),
                                           nullptr, IID_IShellItem,
                                           getter_AddRefs(folder)))) {
    return NS_ERROR_UNEXPECTED;
  }

  // Need to send a first notification outside FileDialogEventsForTesting.
  if (FAILED(dialog->SetFolder(folder))) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr events(new FileDialogEventsForTesting(aModuleName, folder));

  DWORD cookie;
  if (FAILED(dialog->Advise(events, &cookie))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (FAILED(dialog->Show(nullptr))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (FAILED(dialog->Unadvise(cookie))) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

}  // namespace mozilla
