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
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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
#include "nsFilePicker.h"

#include "nsILocalFile.h"
#include "nsIFileURL.h"
#include "nsIURI.h"
#include "nsISupportsArray.h"
#include "nsMemory.h"
#include "nsEnumeratorUtils.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"

#include <qfiledialog.h>
#include <qfile.h>
#include <qstringlist.h>

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

nsFilePicker::nsFilePicker()
    : mDialog(0),
      mMode(nsIFilePicker::modeOpen)
{
}

nsFilePicker::~nsFilePicker()
{
    delete mDialog;
}

NS_IMETHODIMP
nsFilePicker::Init(nsIDOMWindow *parent, const nsAString & title, PRInt16 mode)
{
    return nsBaseFilePicker::Init(parent, title, mode);
}

/* void appendFilters (in long filterMask); */
NS_IMETHODIMP
nsFilePicker::AppendFilters(PRInt32 filterMask)
{
    return nsBaseFilePicker::AppendFilters(filterMask);
}

/* void appendFilter (in AString title, in AString filter); */
NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString & aTitle, const nsAString & aFilter)
{
    if (aFilter.Equals(NS_LITERAL_STRING("..apps"))) {
        // No platform specific thing we can do here, really....
        return NS_OK;
    }

    nsCAutoString filter, name;
    CopyUTF16toUTF8(aFilter, filter);
    CopyUTF16toUTF8(aTitle, name);

    mFilters.AppendCString(filter);
    mFilterNames.AppendCString(name);

    return NS_OK;
}

/* attribute AString defaultString; */
NS_IMETHODIMP
nsFilePicker::GetDefaultString(nsAString & aDefaultString)
{
    mDefault = aDefaultString;

    return NS_OK;
}
NS_IMETHODIMP
nsFilePicker::SetDefaultString(const nsAString & aDefaultString)
{
    return NS_ERROR_FAILURE;
}

/* attribute AString defaultExtension; */
NS_IMETHODIMP
nsFilePicker::GetDefaultExtension(nsAString & aDefaultExtension)
{
    aDefaultExtension = mDefaultExtension;

    return NS_OK;
}
NS_IMETHODIMP
nsFilePicker::SetDefaultExtension(const nsAString & aDefaultExtension)
{
    mDefaultExtension = aDefaultExtension;

    return NS_OK;
}

/* attribute long filterIndex; */
NS_IMETHODIMP
nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
    *aFilterIndex = mSelectedType;

    return NS_OK;
}
NS_IMETHODIMP
nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
    mSelectedType = aFilterIndex;

    return NS_OK;
}

/* attribute nsILocalFile displayDirectory; */
NS_IMETHODIMP
nsFilePicker::GetDisplayDirectory(nsILocalFile * *aDisplayDirectory)
{
    NS_IF_ADDREF(*aDisplayDirectory = mDisplayDirectory);

    return NS_OK;
}
NS_IMETHODIMP
nsFilePicker::SetDisplayDirectory(nsILocalFile * aDisplayDirectory)
{
    mDisplayDirectory = aDisplayDirectory;
    return NS_OK;
}

/* readonly attribute nsILocalFile file; */
NS_IMETHODIMP
nsFilePicker::GetFile(nsILocalFile * *aFile)
{
    NS_ENSURE_ARG_POINTER(aFile);

    *aFile = nsnull;
    if (mFile.IsEmpty()) {
        return NS_OK;
    }

    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    file->InitWithNativePath(mFile);

    NS_ADDREF(*aFile = file);

    return NS_OK;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP
nsFilePicker::GetFileURL(nsIFileURL * *aFileURL)
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
nsFilePicker::GetFiles(nsISimpleEnumerator * *aFiles)
{
    NS_ENSURE_ARG_POINTER(aFiles);

    if (mMode == nsIFilePicker::modeOpenMultiple) {
        return NS_NewArrayEnumerator(aFiles, mFiles);
    }

    return NS_ERROR_FAILURE;
}

/* short show (); */
NS_IMETHODIMP
nsFilePicker::Show(PRInt16 *aReturn)
{
    nsCAutoString directory;
    if (mDisplayDirectory) {
        mDisplayDirectory->GetNativePath(directory);
    }

    switch (mMode) {
    case nsIFilePicker::modeOpen:
        break;
    case nsIFilePicker::modeOpenMultiple:
        mDialog->setMode(QFileDialog::ExistingFiles);
        break;
    case nsIFilePicker::modeSave:
        mDialog->setMode(QFileDialog::AnyFile);
        break;
    case nsIFilePicker::modeGetFolder:
        mDialog->setMode(QFileDialog::DirectoryOnly);
        break;
    default:
        break;
    }

    mDialog->setSelection(QString::fromUcs2(mDefault.get()));

    mDialog->setDir(directory.get());

    QStringList filters;
    PRInt32 count = mFilters.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        filters.append( mFilters[i]->get() );
    }
    mDialog->setFilters(filters);

    switch (mDialog->exec()) {
    case QDialog::Accepted: {
        QCString path = QFile::encodeName(mDialog->selectedFile());
        qDebug("path is '%s'", path.data());
        mFile.Assign(path);
        *aReturn = nsIFilePicker::returnOK;
        if (mMode == modeSave) {
            nsCOMPtr<nsILocalFile> file;
            GetFile(getter_AddRefs(file));
            if (file) {
                PRBool exists = PR_FALSE;
                file->Exists(&exists);
                if (exists) {
                    *aReturn = nsIFilePicker::returnReplace;
                }
            }
        }
    }
        break;
    case QDialog::Rejected: {
        *aReturn = nsIFilePicker::returnCancel;
    }
        break;
    default:
        *aReturn = nsIFilePicker::returnCancel;
        break;
    }


    return NS_OK;
}

void nsFilePicker::InitNative(nsIWidget *parent, const nsAString &title, PRInt16 mode)
{
    QWidget *parentWidget = (parent)? (QWidget*)parent->GetNativeData(NS_NATIVE_WIDGET):0;

    nsAutoString str(title);
    mDialog = new QFileDialog(parentWidget);
    mDialog->setCaption(QString::fromUcs2(str.get()));
    mMode = mode;
}
