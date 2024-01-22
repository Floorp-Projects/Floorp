/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePickerProxy.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsSimpleEnumerator.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/IPCBlobUtils.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsFilePickerProxy, nsIFilePicker)

nsFilePickerProxy::nsFilePickerProxy()
    : mSelectedType(0), mCapture(captureNone), mIPCActive(false) {}

nsFilePickerProxy::~nsFilePickerProxy() = default;

NS_IMETHODIMP
nsFilePickerProxy::Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                        nsIFilePicker::Mode aMode,
                        BrowsingContext* aBrowsingContext) {
  BrowserChild* browserChild = BrowserChild::GetFrom(aParent);
  if (!browserChild) {
    return NS_ERROR_FAILURE;
  }

  mParent = nsPIDOMWindowOuter::From(aParent);

  mMode = aMode;

  browserChild->SendPFilePickerConstructor(this, aTitle, aMode);

  mIPCActive = true;
  return NS_OK;
}

void nsFilePickerProxy::InitNative(nsIWidget* aParent,
                                   const nsAString& aTitle) {}

NS_IMETHODIMP
nsFilePickerProxy::AppendFilter(const nsAString& aTitle,
                                const nsAString& aFilter) {
  mFilterNames.AppendElement(aTitle);
  mFilters.AppendElement(aFilter);
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetCapture(nsIFilePicker::CaptureTarget* aCapture) {
  *aCapture = mCapture;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetCapture(nsIFilePicker::CaptureTarget aCapture) {
  mCapture = aCapture;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetDefaultString(nsAString& aDefaultString) {
  aDefaultString = mDefault;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetDefaultString(const nsAString& aDefaultString) {
  mDefault = aDefaultString;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetDefaultExtension(nsAString& aDefaultExtension) {
  aDefaultExtension = mDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetDefaultExtension(const nsAString& aDefaultExtension) {
  mDefaultExtension = aDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFilterIndex(int32_t* aFilterIndex) {
  *aFilterIndex = mSelectedType;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetFilterIndex(int32_t aFilterIndex) {
  mSelectedType = aFilterIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFile(nsIFile** aFile) {
  MOZ_ASSERT(false, "GetFile is unimplemented; use GetDomFileOrDirectory");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFileURL(nsIURI** aFileURL) {
  MOZ_ASSERT(false, "GetFileURL is unimplemented; use GetDomFileOrDirectory");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFiles(nsISimpleEnumerator** aFiles) {
  MOZ_ASSERT(false,
             "GetFiles is unimplemented; use GetDomFileOrDirectoryEnumerator");
  return NS_ERROR_FAILURE;
}

nsresult nsFilePickerProxy::Show(nsIFilePicker::ResultCode* aReturn) {
  MOZ_ASSERT(false, "Show is unimplemented; use Open");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFilePickerProxy::Open(nsIFilePickerShownCallback* aCallback) {
  mCallback = aCallback;

  nsString displayDirectory;
  if (mDisplayDirectory) {
    mDisplayDirectory->GetPath(displayDirectory);
  }

  if (!mIPCActive) {
    return NS_ERROR_FAILURE;
  }

  SendOpen(mSelectedType, mAddToRecentDocs, mDefault, mDefaultExtension,
           mFilters, mFilterNames, mRawFilters, displayDirectory,
           mDisplaySpecialDirectory, mOkButtonLabel, mCapture);

  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::Close() {
  SendClose();

  return NS_OK;
}

mozilla::ipc::IPCResult nsFilePickerProxy::Recv__delete__(
    const MaybeInputData& aData, const nsIFilePicker::ResultCode& aResult) {
  nsPIDOMWindowInner* inner =
      mParent ? mParent->GetCurrentInnerWindow() : nullptr;

  if (NS_WARN_IF(!inner)) {
    return IPC_OK();
  }

  if (aData.type() == MaybeInputData::TInputBlobs) {
    const nsTArray<IPCBlob>& blobs = aData.get_InputBlobs().blobs();
    for (uint32_t i = 0; i < blobs.Length(); ++i) {
      RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(blobs[i]);
      NS_ENSURE_TRUE(blobImpl, IPC_OK());

      if (!blobImpl->IsFile()) {
        return IPC_OK();
      }

      RefPtr<File> file = File::Create(inner->AsGlobal(), blobImpl);
      if (NS_WARN_IF(!file)) {
        return IPC_OK();
      }

      OwningFileOrDirectory* element = mFilesOrDirectories.AppendElement();
      element->SetAsFile() = file;
    }
  } else if (aData.type() == MaybeInputData::TInputDirectory) {
    nsCOMPtr<nsIFile> file;
    const nsAString& path(aData.get_InputDirectory().directoryPath());
    nsresult rv = NS_NewLocalFile(path, true, getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return IPC_OK();
    }

    RefPtr<Directory> directory = Directory::Create(inner->AsGlobal(), file);
    MOZ_ASSERT(directory);

    OwningFileOrDirectory* element = mFilesOrDirectories.AppendElement();
    element->SetAsDirectory() = directory;
  }

  if (mCallback) {
    mCallback->Done(aResult);
    mCallback = nullptr;
  }

  return IPC_OK();
}

NS_IMETHODIMP
nsFilePickerProxy::GetDomFileOrDirectory(nsISupports** aValue) {
  *aValue = nullptr;
  if (mFilesOrDirectories.IsEmpty()) {
    return NS_OK;
  }

  MOZ_ASSERT(mFilesOrDirectories.Length() == 1);

  if (mFilesOrDirectories[0].IsFile()) {
    nsCOMPtr<nsISupports> blob = ToSupports(mFilesOrDirectories[0].GetAsFile());
    blob.forget(aValue);
    return NS_OK;
  }

  MOZ_ASSERT(mFilesOrDirectories[0].IsDirectory());
  RefPtr<Directory> directory = mFilesOrDirectories[0].GetAsDirectory();
  directory.forget(aValue);
  return NS_OK;
}

namespace {

class SimpleEnumerator final : public nsSimpleEnumerator {
 public:
  explicit SimpleEnumerator(
      const nsTArray<OwningFileOrDirectory>& aFilesOrDirectories)
      : mFilesOrDirectories(aFilesOrDirectories.Clone()), mIndex(0) {}

  NS_IMETHOD
  HasMoreElements(bool* aRetvalue) override {
    MOZ_ASSERT(aRetvalue);
    *aRetvalue = mIndex < mFilesOrDirectories.Length();
    return NS_OK;
  }

  NS_IMETHOD
  GetNext(nsISupports** aValue) override {
    NS_ENSURE_TRUE(mIndex < mFilesOrDirectories.Length(), NS_ERROR_FAILURE);

    uint32_t index = mIndex++;

    if (mFilesOrDirectories[index].IsFile()) {
      nsCOMPtr<nsISupports> blob =
          ToSupports(mFilesOrDirectories[index].GetAsFile());
      blob.forget(aValue);
      return NS_OK;
    }

    MOZ_ASSERT(mFilesOrDirectories[index].IsDirectory());
    RefPtr<Directory> directory = mFilesOrDirectories[index].GetAsDirectory();
    directory.forget(aValue);
    return NS_OK;
  }

 private:
  nsTArray<mozilla::dom::OwningFileOrDirectory> mFilesOrDirectories;
  uint32_t mIndex;
};

}  // namespace

NS_IMETHODIMP
nsFilePickerProxy::GetDomFileOrDirectoryEnumerator(
    nsISimpleEnumerator** aDomfiles) {
  RefPtr<SimpleEnumerator> enumerator =
      new SimpleEnumerator(mFilesOrDirectories);
  enumerator.forget(aDomfiles);
  return NS_OK;
}

void nsFilePickerProxy::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCActive = false;

  if (mCallback) {
    mCallback->Done(nsIFilePicker::returnCancel);
    mCallback = nullptr;
  }
}

nsresult nsFilePickerProxy::ResolveSpecialDirectory(
    const nsAString& aSpecialDirectory) {
  MOZ_ASSERT(XRE_IsContentProcess());
  // Resolving the special-directory name to a path in both the child and parent
  // processes is redundant -- and sandboxing may prevent us from doing so in
  // the child process, anyway. (See bugs 1357846 and 1838244.)
  //
  // Unfortunately we can't easily verify that `aSpecialDirectory` is usable or
  // even meaningful here, so we just accept anything.
  return NS_OK;
}
