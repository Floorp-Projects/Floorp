/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePickerProxy.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "mozilla/dom/TabChild.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS1(nsFilePickerProxy, nsIFilePicker)

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
  NS_ENSURE_ARG_POINTER(aFile);

  *aFile = nullptr;
  if (mFiles.IsEmpty()) {
      return NS_OK;
  }

  nsCOMPtr<nsIFile> file = mFiles[0];
  file.forget(aFile);
  return NS_OK;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP
nsFilePickerProxy::GetFileURL(nsIURI** aFileURL)
{
  nsCOMPtr<nsIFile> file;
  GetFile(getter_AddRefs(file));

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), file);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  return CallQueryInterface(uri, aFileURL);
}

/* readonly attribute nsISimpleEnumerator files; */
NS_IMETHODIMP
nsFilePickerProxy::GetFiles(nsISimpleEnumerator** aFiles)
{
  NS_ENSURE_ARG_POINTER(aFiles);

  if (mMode == nsIFilePicker::modeOpenMultiple) {
    return NS_NewArrayEnumerator(aFiles, mFiles);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePickerProxy::Show(int16_t* aReturn)
{
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
    const InfallibleTArray<nsString>& files = aFiles.get_InputFiles().files();
    for (uint32_t i = 0; i < files.Length(); ++i) {
      nsCOMPtr<nsIFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
      NS_ENSURE_TRUE(file, true);
      file->InitWithPath(files[i]);
      mFiles.AppendObject(file);
    }
  }

  if (mCallback) {
    mCallback->Done(aResult);
    mCallback = nullptr;
  }

  return true;
}
