/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSFILEPICKER_H
#define NSFILEPICKER_H

#include "nsBaseFilePicker.h"
#include "nsString.h"

class nsFilePicker : public nsBaseFilePicker
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHODIMP Init(nsIDOMWindow *parent, const nsAString& title,
                       int16_t mode);
    NS_IMETHOD AppendFilters(int32_t aFilterMask);
    NS_IMETHOD AppendFilter(const nsAString & aTitle,
                            const nsAString & aFilter);
    NS_IMETHOD GetDefaultString(nsAString & aDefaultString);
    NS_IMETHOD SetDefaultString(const nsAString & aDefaultString);
    NS_IMETHOD GetDefaultExtension(nsAString & aDefaultExtension);
    NS_IMETHOD SetDefaultExtension(const nsAString & aDefaultExtension);
    NS_IMETHOD GetFile(nsIFile * *aFile);
    NS_IMETHOD GetFileURL(nsIURI * *aFileURL);
    NS_IMETHOD SetDisplayDirectory(nsIFile *aDisplayDirectory);
    NS_IMETHOD GetDisplayDirectory(nsIFile **aDisplayDirectory);
    NS_IMETHOD Show(int16_t *aReturn);
private:
    void InitNative(nsIWidget*, const nsAString&, int16_t) {};
    nsString mFilePath;
    nsString mExtensionsFilter;
    nsString mMimeTypeFilter;
};
#endif
