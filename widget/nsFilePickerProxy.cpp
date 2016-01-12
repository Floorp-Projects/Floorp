/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePickerProxy.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/BlobChild.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsFilePickerProxy, nsIFilePicker)

nsFilePickerProxy::nsFilePickerProxy()
  : mSelectedType(0)
{
}

nsFilePickerProxy::~nsFilePickerProxy()
{
}

NS_IMETHODIMP
nsFilePickerProxy::Init(nsIDOMWindow* aParent, const nsAString& aTitle,
                        int16_t aMode)
{
  TabChild* tabChild = TabChild::GetFrom(aParent);
  if (!tabChild) {
    return NS_ERROR_FAILURE;
  }

  mParent = do_QueryInterface(aParent);
  if (!mParent->IsInnerWindow()) {
    mParent = mParent->GetCurrentInnerWindow();
  }

  mMode = aMode;

  NS_ADDREF_THIS();
  tabChild->SendPFilePickerConstructor(this, nsString(aTitle), aMode);
  return NS_OK;
}

void
nsFilePickerProxy::InitNative(nsIWidget* aParent, const nsAString& aTitle)
{
}

NS_IMETHODIMP
nsFilePickerProxy::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  mFilterNames.AppendElement(aTitle);
  mFilters.AppendElement(aFilter);
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetDefaultString(nsAString& aDefaultString)
{
  aDefaultString = mDefault;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetDefaultString(const nsAString& aDefaultString)
{
  mDefault = aDefaultString;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetDefaultExtension(nsAString& aDefaultExtension)
{
  aDefaultExtension = mDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetDefaultExtension(const nsAString& aDefaultExtension)
{
  mDefaultExtension = aDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFilterIndex(int32_t* aFilterIndex)
{
  *aFilterIndex = mSelectedType;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetFilterIndex(int32_t aFilterIndex)
{
  mSelectedType = aFilterIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFile(nsIFile** aFile)
{
  MOZ_ASSERT(false, "GetFile is unimplemented; use GetDomfile");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFileURL(nsIURI** aFileURL)
{
  MOZ_ASSERT(false, "GetFileURL is unimplemented; use GetDomfile");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePickerProxy::GetFiles(nsISimpleEnumerator** aFiles)
{
  MOZ_ASSERT(false, "GetFiles is unimplemented; use GetDomfiles");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePickerProxy::Show(int16_t* aReturn)
{
  MOZ_ASSERT(false, "Show is unimplemented; use Open");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFilePickerProxy::Open(nsIFilePickerShownCallback* aCallback)
{
  mCallback = aCallback;

  nsString displayDirectory;
  if (mDisplayDirectory) {
    mDisplayDirectory->GetPath(displayDirectory);
  }

  SendOpen(mSelectedType, mAddToRecentDocs, mDefault, mDefaultExtension,
           mFilters, mFilterNames, displayDirectory);

  return NS_OK;
}

bool
nsFilePickerProxy::Recv__delete__(const MaybeInputFiles& aFiles,
                                  const int16_t& aResult)
{
  if (aFiles.type() == MaybeInputFiles::TInputFiles) {
    const InfallibleTArray<PBlobChild*>& blobs = aFiles.get_InputFiles().blobsChild();
    for (uint32_t i = 0; i < blobs.Length(); ++i) {
      BlobChild* actor = static_cast<BlobChild*>(blobs[i]);
      RefPtr<BlobImpl> blobImpl = actor->GetBlobImpl();
      NS_ENSURE_TRUE(blobImpl, true);

      if (!blobImpl->IsFile()) {
        return true;
      }

      RefPtr<File> file = File::Create(mParent, blobImpl);
      MOZ_ASSERT(file);

      mDomfiles.AppendElement(file);
    }
  }

  if (mCallback) {
    mCallback->Done(aResult);
    mCallback = nullptr;
  }

  return true;
}

NS_IMETHODIMP
nsFilePickerProxy::GetDomfile(nsISupports** aDomfile)
{
  *aDomfile = nullptr;
  if (mDomfiles.IsEmpty()) {
    return NS_OK;
  }

  MOZ_ASSERT(mDomfiles.Length() == 1);
  nsCOMPtr<nsIDOMBlob> blob = mDomfiles[0].get();
  blob.forget(aDomfile);
  return NS_OK;
}

namespace {

class SimpleEnumerator final : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS

  explicit SimpleEnumerator(const nsTArray<RefPtr<File>>& aFiles)
    : mFiles(aFiles)
    , mIndex(0)
  {}

  NS_IMETHOD
  HasMoreElements(bool* aRetvalue) override
  {
    MOZ_ASSERT(aRetvalue);
    *aRetvalue = mIndex < mFiles.Length();
    return NS_OK;
  }

  NS_IMETHOD
  GetNext(nsISupports** aSupports) override
  {
    NS_ENSURE_TRUE(mIndex < mFiles.Length(), NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMBlob> blob = mFiles[mIndex++].get();
    blob.forget(aSupports);
    return NS_OK;
  }

private:
  ~SimpleEnumerator()
  {}

  nsTArray<RefPtr<File>> mFiles;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS(SimpleEnumerator, nsISimpleEnumerator)

} // namespace

NS_IMETHODIMP
nsFilePickerProxy::GetDomfiles(nsISimpleEnumerator** aDomfiles)
{
  RefPtr<SimpleEnumerator> enumerator = new SimpleEnumerator(mDomfiles);
  enumerator.forget(aDomfiles);
  return NS_OK;
}
