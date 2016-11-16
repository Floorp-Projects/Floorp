/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSFILEPICKERPROXY_H
#define NSFILEPICKERPROXY_H

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsCOMArray.h"

#include "mozilla/dom/PFilePickerChild.h"
#include "mozilla/dom/UnionTypes.h"

class nsIWidget;
class nsIFile;
class nsPIDOMWindowInner;

/**
  This class creates a proxy file picker to be used in content processes.
  The file picker just collects the initialization data and when Show() is
  called, remotes everything to the chrome process which in turn can show a
  platform specific file picker.
*/
class nsFilePickerProxy : public nsBaseFilePicker,
                          public mozilla::dom::PFilePickerChild
{
public:
    nsFilePickerProxy();

    NS_DECL_ISUPPORTS

    // nsIFilePicker (less what's in nsBaseFilePicker)
    NS_IMETHOD Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle, int16_t aMode) override;
    NS_IMETHOD AppendFilter(const nsAString& aTitle, const nsAString& aFilter) override;
    NS_IMETHOD GetDefaultString(nsAString& aDefaultString) override;
    NS_IMETHOD SetDefaultString(const nsAString& aDefaultString) override;
    NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension) override;
    NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension) override;
    NS_IMETHOD GetFilterIndex(int32_t* aFilterIndex) override;
    NS_IMETHOD SetFilterIndex(int32_t aFilterIndex) override;
    NS_IMETHOD GetFile(nsIFile** aFile) override;
    NS_IMETHOD GetFileURL(nsIURI** aFileURL) override;
    NS_IMETHOD GetFiles(nsISimpleEnumerator** aFiles) override;

    NS_IMETHOD GetDomFileOrDirectory(nsISupports** aValue) override;
    NS_IMETHOD GetDomFileOrDirectoryEnumerator(nsISimpleEnumerator** aValue) override;

    NS_IMETHOD Show(int16_t* aReturn) override;
    NS_IMETHOD Open(nsIFilePickerShownCallback* aCallback) override;

    // PFilePickerChild
    virtual mozilla::ipc::IPCResult
    Recv__delete__(const MaybeInputData& aData, const int16_t& aResult) override;

private:
    ~nsFilePickerProxy();
    void InitNative(nsIWidget*, const nsAString&) override;

    nsTArray<mozilla::dom::OwningFileOrDirectory> mFilesOrDirectories;
    nsCOMPtr<nsIFilePickerShownCallback> mCallback;

    int16_t   mSelectedType;
    nsString  mFile;
    nsString  mDefault;
    nsString  mDefaultExtension;

    InfallibleTArray<nsString> mFilters;
    InfallibleTArray<nsString> mFilterNames;
};

#endif // NSFILEPICKERPROXY_H
