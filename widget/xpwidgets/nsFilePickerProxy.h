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

class nsIWidget;
class nsIFile;

/**
  This class creates a proxy file picker to be used in content processes.
  The file picker just collects the initialization data and when Show() is
  called, remotes everything to the chrome process which in turn can show a
  platform specific file picker.
*/
class nsFilePickerProxy : public nsBaseFilePicker
{
public:
    nsFilePickerProxy();

    NS_DECL_ISUPPORTS

    // nsIFilePicker (less what's in nsBaseFilePicker)
    NS_IMETHODIMP Init(nsIDOMWindow* parent, const nsAString& title, int16_t mode);
    NS_IMETHODIMP AppendFilter(const nsAString& aTitle, const nsAString& aFilter);
    NS_IMETHODIMP GetDefaultString(nsAString& aDefaultString);
    NS_IMETHODIMP SetDefaultString(const nsAString& aDefaultString);
    NS_IMETHODIMP GetDefaultExtension(nsAString& aDefaultExtension);
    NS_IMETHODIMP SetDefaultExtension(const nsAString& aDefaultExtension);
    NS_IMETHODIMP GetFilterIndex(int32_t* aFilterIndex);
    NS_IMETHODIMP SetFilterIndex(int32_t aFilterIndex);
    NS_IMETHODIMP GetFile(nsIFile** aFile);
    NS_IMETHODIMP GetFileURL(nsIURI** aFileURL);
    NS_IMETHODIMP GetFiles(nsISimpleEnumerator** aFiles);
    NS_IMETHODIMP Show(int16_t* aReturn);

private:
    ~nsFilePickerProxy();
    void InitNative(nsIWidget*, const nsAString&);

    nsCOMArray<nsIFile> mFiles;

    int16_t   mSelectedType;
    nsString  mFile;
    nsString  mTitle;
    nsString  mDefault;
    nsString  mDefaultExtension;

    InfallibleTArray<nsString> mFilters;
    InfallibleTArray<nsString> mFilterNames;
};

#endif // NSFILEPICKERPROXY_H
