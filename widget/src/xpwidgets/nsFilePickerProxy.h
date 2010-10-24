/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   Steffen Imhof <steffen.imhof@googlemail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef NSFILEPICKERPROXY_H
#define NSFILEPICKERPROXY_H

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsCOMArray.h"

class nsIWidget;
class nsILocalFile;

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
    NS_IMETHODIMP Init(nsIDOMWindow* parent, const nsAString& title, PRInt16 mode);
    NS_IMETHODIMP AppendFilter(const nsAString& aTitle, const nsAString& aFilter);
    NS_IMETHODIMP GetDefaultString(nsAString& aDefaultString);
    NS_IMETHODIMP SetDefaultString(const nsAString& aDefaultString);
    NS_IMETHODIMP GetDefaultExtension(nsAString& aDefaultExtension);
    NS_IMETHODIMP SetDefaultExtension(const nsAString& aDefaultExtension);
    NS_IMETHODIMP GetFilterIndex(PRInt32* aFilterIndex);
    NS_IMETHODIMP SetFilterIndex(PRInt32 aFilterIndex);
    NS_IMETHODIMP GetFile(nsILocalFile** aFile);
    NS_IMETHODIMP GetFileURL(nsIURI** aFileURL);
    NS_IMETHODIMP GetFiles(nsISimpleEnumerator** aFiles);
    NS_IMETHODIMP Show(PRInt16* aReturn);

private:
    ~nsFilePickerProxy();
    void InitNative(nsIWidget*, const nsAString&, short int);

    nsCOMArray<nsILocalFile> mFiles;

    PRInt16   mMode;
    PRInt16   mSelectedType;
    nsString  mFile;
    nsString  mTitle;
    nsString  mDefault;
    nsString  mDefaultExtension;

    nsTArray<nsString> mFilters;
    nsTArray<nsString> mFilterNames;
};

#endif // NSFILEPICKERPROXY_H
