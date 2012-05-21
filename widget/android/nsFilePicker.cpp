/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePicker.h"
#include "AndroidBridge.h"
#include "nsNetUtil.h"
#include "nsIURI.h"

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

NS_IMETHODIMP nsFilePicker::Init(nsIDOMWindow *parent, const nsAString& title, 
                                 PRInt16 mode)
{
    return (mode == nsIFilePicker::modeOpen ||
            mode == nsIFilePicker::modeOpenMultiple)
        ? NS_OK
        : NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFilePicker::AppendFilters(PRInt32 aFilterMask)
{
  if (aFilterMask == (filterAudio | filterAll)) {
    mMimeTypeFilter.AssignLiteral("audio/*");
    return NS_OK;
  }

  if (aFilterMask == (filterImages | filterAll)) {
    mMimeTypeFilter.AssignLiteral("image/*");
    return NS_OK;
  }

  if (aFilterMask == (filterVideo | filterAll)) {
    mMimeTypeFilter.AssignLiteral("video/*");
    return NS_OK;
  }

  if (aFilterMask & filterAll) {
    mMimeTypeFilter.AssignLiteral("*/*");
    return NS_OK;
  }

  return nsBaseFilePicker::AppendFilters(aFilterMask);
}

NS_IMETHODIMP nsFilePicker::AppendFilter(const nsAString& /*title*/,
                                         const nsAString& filter)
{
    if (!mExtensionsFilter.IsEmpty())
        mExtensionsFilter.AppendLiteral(", ");
    mExtensionsFilter.Append(filter);
    return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(nsAString & aDefaultString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsFilePicker::SetDefaultString(const nsAString & aDefaultString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFilePicker::GetDefaultExtension(nsAString & aDefaultExtension)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const nsAString & aDefaultExtension)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFilePicker::GetDisplayDirectory(nsILocalFile **aDisplayDirectory)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsFilePicker::SetDisplayDirectory(nsILocalFile *aDisplayDirectory)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
    NS_ENSURE_ARG_POINTER(aFile);

    *aFile = nsnull;
    if (mFilePath.IsEmpty()) {
        return NS_OK;
    }

    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    file->InitWithPath(mFilePath);

    NS_ADDREF(*aFile = file);

    nsCString path;
    (*aFile)->GetNativePath(path);
    return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFileURL(nsIURI **aFileURL)
{
    nsCOMPtr<nsILocalFile> file;
    GetFile(getter_AddRefs(file));

    nsCOMPtr<nsIURI> uri;
    NS_NewFileURI(getter_AddRefs(uri), file);
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

    return CallQueryInterface(uri, aFileURL);
}

NS_IMETHODIMP nsFilePicker::Show(PRInt16 *_retval NS_OUTPARAM)
{
    if (!mozilla::AndroidBridge::Bridge())
        return NS_ERROR_NOT_IMPLEMENTED;
    nsAutoString filePath;

    if (mExtensionsFilter.IsEmpty() == mMimeTypeFilter.IsEmpty()) {
      // Both filters or none of them are set. We want to show anything we can.
      mozilla::AndroidBridge::Bridge()->ShowFilePickerForMimeType(filePath, NS_LITERAL_STRING("*/*"));
    } else if (!mExtensionsFilter.IsEmpty()) {
      mozilla::AndroidBridge::Bridge()->ShowFilePickerForExtensions(filePath, mExtensionsFilter);
    } else {
      mozilla::AndroidBridge::Bridge()->ShowFilePickerForMimeType(filePath, mMimeTypeFilter);
    }

    *_retval = EmptyString().Equals(filePath) ? 
        nsIFilePicker::returnCancel : nsIFilePicker::returnOK;
    if (*_retval == nsIFilePicker::returnOK)
        mFilePath.Assign(filePath);
    return NS_OK;
}
