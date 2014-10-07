/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePickerProxy.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsDOMFile.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/BlobChild.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsFilePickerProxy, nsIFilePicker)

nsFilePickerProxy::nsFilePickerProxy()
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

/* readonly attribute nsIFile file; */
NS_IMETHODIMP
nsFilePickerProxy::GetFile(nsIFile** aFile)
{
  MOZ_ASSERT(false, "GetFile is unimplemented; use GetDomfile");
  return NS_ERROR_FAILURE;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP
nsFilePickerProxy::GetFileURL(nsIURI** aFileURL)
{
  MOZ_ASSERT(false, "GetFileURL is unimplemented; use GetDomfile");
  return NS_ERROR_FAILURE;
}

/* readonly attribute nsISimpleEnumerator files; */
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

  SendOpen(mSelectedType, mAddToRecentDocs, mDefault,
           mDefaultExtension, mFilters, mFilterNames);

  return NS_OK;
}

bool
nsFilePickerProxy::Recv__delete__(const MaybeInputFiles& aFiles,
                                  const int16_t& aResult)
{
  if (aFiles.type() == MaybeInputFiles::TInputFiles) {
    const InfallibleTArray<PBlobChild*>& files = aFiles.get_InputFiles().filesChild();
    for (uint32_t i = 0; i < files.Length(); ++i) {
      BlobChild* actor = static_cast<BlobChild*>(files[i]);
      nsCOMPtr<nsIDOMBlob> blob = actor->GetBlob();
      nsCOMPtr<nsIDOMFile> file(do_QueryInterface(blob));
      NS_ENSURE_TRUE(file, true);
      mDomfiles.AppendObject(file);
    }
  }

  if (mCallback) {
    mCallback->Done(aResult);
    mCallback = nullptr;
  }

  return true;
}

NS_IMETHODIMP
nsFilePickerProxy::GetDomfile(nsIDOMFile** aDomfile)
{
  *aDomfile = nullptr;
  if (mDomfiles.IsEmpty()) {
    return NS_OK;
  }

  MOZ_ASSERT(mDomfiles.Length() == 1);
  nsCOMPtr<nsIDOMFile> domfile = mDomfiles[0];
  domfile.forget(aDomfile);
  return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::GetDomfiles(nsISimpleEnumerator** aDomfiles)
{
  return NS_NewArrayEnumerator(aDomfiles, mDomfiles);
}
