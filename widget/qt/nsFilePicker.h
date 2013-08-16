/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSFILEPICKER_H
#define NSFILEPICKER_H

#include <QPointer>
#include "nsBaseFilePicker.h"
#include "nsCOMArray.h"
#include "nsString.h"

class QFileDialog;

class nsFilePicker : public nsBaseFilePicker
{
public:
    nsFilePicker();

    NS_DECL_ISUPPORTS

    // nsIFilePicker (less what's in nsBaseFilePicker)
    NS_IMETHODIMP Init(nsIDOMWindow* parent, const nsAString& title, int16_t mode);
    NS_IMETHODIMP AppendFilters(int32_t filterMask);
    NS_IMETHODIMP AppendFilter(const nsAString& aTitle, const nsAString& aFilter);
    NS_IMETHODIMP GetDefaultString(nsAString& aDefaultString);
    NS_IMETHODIMP SetDefaultString(const nsAString& aDefaultString);
    NS_IMETHODIMP GetDefaultExtension(nsAString& aDefaultExtension);
    NS_IMETHODIMP SetDefaultExtension(const nsAString& aDefaultExtension);
    NS_IMETHODIMP GetFilterIndex(int32_t* aFilterIndex);
    NS_IMETHODIMP SetFilterIndex(int32_t aFilterIndex);
    NS_IMETHODIMP GetFile(nsIFile* *aFile);
    NS_IMETHODIMP GetFileURL(nsIURI* *aFileURL);
    NS_IMETHODIMP GetFiles(nsISimpleEnumerator* *aFiles);
    NS_IMETHODIMP Show(int16_t* aReturn);

private:
    ~nsFilePicker();
    void InitNative(nsIWidget*, const nsAString&);

protected:
    QPointer<QFileDialog> mDialog;
    nsCOMArray<nsIFile> mFiles;

    int16_t   mSelectedType;
    nsCString mFile;
    nsString  mTitle;
    nsString  mDefault;
    nsString  mDefaultExtension;

    nsTArray<nsCString> mFilters;
    nsTArray<nsCString> mFilterNames;

    QString mCaption;
    nsIWidget* mParent;
};

#endif // NSFILEPICKER_H
