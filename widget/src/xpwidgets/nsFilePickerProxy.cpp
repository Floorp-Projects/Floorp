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
 *  The Mozilla Foundation
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

#include "mozilla/dom/ContentChild.h"
#include "nsFilePickerProxy.h"


NS_IMPL_ISUPPORTS1(nsFilePickerProxy, nsIFilePicker)

nsFilePickerProxy::nsFilePickerProxy()
{ 
}

nsFilePickerProxy::~nsFilePickerProxy()
{
}

NS_IMETHODIMP
nsFilePickerProxy::Init(nsIDOMWindow* /*aParent*/, const nsAString& aTitle,
                        PRInt16 aMode)
{
    mTitle = aTitle;
    mMode = aMode;

    return NS_OK;
}

void nsFilePickerProxy::InitNative(nsIWidget* aParent, const nsAString& aTitle,
                              PRInt16 aMode)
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
nsFilePickerProxy::GetFilterIndex(PRInt32* aFilterIndex)
{
    *aFilterIndex = mSelectedType;
    return NS_OK;
}

NS_IMETHODIMP
nsFilePickerProxy::SetFilterIndex(PRInt32 aFilterIndex)
{
    mSelectedType = aFilterIndex;
    return NS_OK;
}

/* readonly attribute nsILocalFile file; */
NS_IMETHODIMP
nsFilePickerProxy::GetFile(nsILocalFile** aFile)
{
    NS_ENSURE_ARG_POINTER(aFile);

    *aFile = nsnull;
    if (mFile.IsEmpty()) {
        return NS_OK;
    }

    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    file->InitWithPath(mFile);

    file.forget(aFile);

    return NS_OK;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP
nsFilePickerProxy::GetFileURL(nsIURI** aFileURL)
{
    nsCOMPtr<nsILocalFile> file;
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

NS_IMETHODIMP nsFilePickerProxy::Show(PRInt16* aReturn)
{
    mozilla::dom::ContentChild *cc = mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cc, "Content Protocol is NULL!");
    
    nsTArray<nsString> filePaths;
    
    nsresult rv;
    cc->SendShowFilePicker(mMode, mSelectedType, mTitle,
                           mDefault, mDefaultExtension,
                           mFilters, mFilterNames,
                           &filePaths, aReturn, &rv);

    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 count = filePaths.Length();
    
    if (mMode == nsIFilePicker::modeOpenMultiple) {
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
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
