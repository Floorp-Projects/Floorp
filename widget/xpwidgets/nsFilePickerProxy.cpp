/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "nsFilePickerProxy.h"
#include "nsNetUtil.h"


NS_IMPL_ISUPPORTS1(nsFilePickerProxy, nsIFilePicker)

nsFilePickerProxy::nsFilePickerProxy()
{ 
}

nsFilePickerProxy::~nsFilePickerProxy()
{
}

NS_IMETHODIMP
nsFilePickerProxy::Init(nsIDOMWindow* /*aParent*/, const nsAString& aTitle,
                        int16_t aMode)
{
    mTitle = aTitle;
    mMode = aMode;

    return NS_OK;
}

void nsFilePickerProxy::InitNative(nsIWidget* aParent, const nsAString& aTitle)
{
}


NS_IMETHODIMP
nsFilePickerProxy::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
    mFilters.AppendElement(aFilter);
    mFilterNames.AppendElement(aTitle);  
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
    if (mFile.IsEmpty()) {
        return NS_OK;
    }

    nsCOMPtr<nsIFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    file->InitWithPath(mFile);

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

NS_IMETHODIMP nsFilePickerProxy::Show(int16_t* aReturn)
{
    mozilla::dom::ContentChild *cc = mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cc, "Content Protocol is NULL!");
    
    InfallibleTArray<nsString> filePaths;
    
    nsresult rv;
    cc->SendShowFilePicker(mMode, mSelectedType,
                           mAddToRecentDocs, mTitle,
                           mDefault, mDefaultExtension,
                           mFilters, mFilterNames,
                           &filePaths, aReturn, &rv);

    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t count = filePaths.Length();
    
    if (mMode == nsIFilePicker::modeOpenMultiple) {
        for (uint32_t i = 0; i < count; ++i) {
            nsCOMPtr<nsIFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
            NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

            file->InitWithPath(filePaths[i]);
            mFiles.AppendObject(file);
        }
        return NS_OK;
    }

    NS_ASSERTION(count == 1 || count == 0, "we should only have 1 or 0 files");

    if (count == 1)
        mFile = filePaths[0];
    
    return NS_OK;
}
