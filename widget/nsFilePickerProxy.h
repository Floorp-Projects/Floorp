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

class nsIWidget;
class nsIFile;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
class File;
}
}

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
    NS_IMETHODIMP Init(nsIDOMWindow* aParent, const nsAString& aTitle, int16_t aMode) override;
    NS_IMETHODIMP AppendFilter(const nsAString& aTitle, const nsAString& aFilter) override;
    NS_IMETHODIMP GetDefaultString(nsAString& aDefaultString) override;
    NS_IMETHODIMP SetDefaultString(const nsAString& aDefaultString) override;
    NS_IMETHODIMP GetDefaultExtension(nsAString& aDefaultExtension) override;
    NS_IMETHODIMP SetDefaultExtension(const nsAString& aDefaultExtension) override;
    NS_IMETHODIMP GetFilterIndex(int32_t* aFilterIndex) override;
    NS_IMETHODIMP SetFilterIndex(int32_t aFilterIndex) override;
    NS_IMETHODIMP GetFile(nsIFile** aFile) override;
    NS_IMETHODIMP GetFileURL(nsIURI** aFileURL) override;
    NS_IMETHODIMP GetFiles(nsISimpleEnumerator** aFiles) override;

    NS_IMETHODIMP GetDomfile(nsISupports** aFile) override;
    NS_IMETHODIMP GetDomfiles(nsISimpleEnumerator** aFiles) override;

    NS_IMETHODIMP Show(int16_t* aReturn) override;
    NS_IMETHODIMP Open(nsIFilePickerShownCallback* aCallback) override;

    // PFilePickerChild
    virtual bool
    Recv__delete__(const MaybeInputFiles& aFiles, const int16_t& aResult) override;

private:
    ~nsFilePickerProxy();
    void InitNative(nsIWidget*, const nsAString&) override;

    nsTArray<nsRefPtr<mozilla::dom::File>> mDomfiles;
    nsCOMPtr<nsIFilePickerShownCallback> mCallback;

    int16_t   mSelectedType;
    nsString  mFile;
    nsString  mDefault;
    nsString  mDefaultExtension;

    InfallibleTArray<nsString> mFilters;
    InfallibleTArray<nsString> mFilterNames;
};

#endif // NSFILEPICKERPROXY_H
