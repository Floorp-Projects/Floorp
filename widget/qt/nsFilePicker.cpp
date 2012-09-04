/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QStringList>
#include <QGraphicsWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTemporaryFile>

#ifdef MOZ_ENABLE_CONTENTMANAGER
#include "nsMFilePicker.h"
#define MozFileDialog MeegoFileDialog
#else
#include <QFileDialog>
#define MozFileDialog QFileDialog
#endif


#include "nsFilePicker.h"
#include "nsNetUtil.h"
#include "nsIWidget.h"
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* sFilePickerLog = nullptr;
#endif

#include "nsDirectoryServiceDefs.h"

//-----------------------------

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

nsFilePicker::nsFilePicker()
    : mMode(nsIFilePicker::modeOpen)
{
#ifdef PR_LOGGING
    if (!sFilePickerLog)
        sFilePickerLog = PR_NewLogModule("nsQtFilePicker");
#endif
}

nsFilePicker::~nsFilePicker()
{
}

NS_IMETHODIMP
nsFilePicker::Init(nsIDOMWindow* parent, const nsAString& title, int16_t mode)
{
    return nsBaseFilePicker::Init(parent, title, mode);
}

/* void appendFilters (in long filterMask); */
NS_IMETHODIMP
nsFilePicker::AppendFilters(int32_t filterMask)
{
    return nsBaseFilePicker::AppendFilters(filterMask);
}

/* void appendFilter (in AString title, in AString filter); */
NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
    if (aFilter.Equals(NS_LITERAL_STRING("..apps"))) {
        // No platform specific thing we can do here, really....
        return NS_OK;
    }

    nsAutoCString filter, name;
    CopyUTF16toUTF8(aFilter, filter);
    CopyUTF16toUTF8(aTitle, name);

    mFilters.AppendElement(filter);
    mFilterNames.AppendElement(name);

    return NS_OK;
}

/* attribute AString defaultString; */
NS_IMETHODIMP
nsFilePicker::GetDefaultString(nsAString& aDefaultString)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultString(const nsAString& aDefaultString)
{
    mDefault = aDefaultString;

    return NS_OK;
}

/* attribute AString defaultExtension; */
NS_IMETHODIMP
nsFilePicker::GetDefaultExtension(nsAString& aDefaultExtension)
{
    aDefaultExtension = mDefaultExtension;

    return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultExtension(const nsAString& aDefaultExtension)
{
    mDefaultExtension = aDefaultExtension;

    return NS_OK;
}

/* attribute long filterIndex; */
NS_IMETHODIMP
nsFilePicker::GetFilterIndex(int32_t* aFilterIndex)
{
    *aFilterIndex = mSelectedType;

    return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetFilterIndex(int32_t aFilterIndex)
{
    mSelectedType = aFilterIndex;

    return NS_OK;
}

/* readonly attribute nsIFile file; */
NS_IMETHODIMP
nsFilePicker::GetFile(nsIFile* *aFile)
{
    NS_ENSURE_ARG_POINTER(aFile);

    *aFile = nullptr;
    if (mFile.IsEmpty()) {
        return NS_OK;
    }

    nsCOMPtr<nsIFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    file->InitWithNativePath(mFile);

    NS_ADDREF(*aFile = file);

    return NS_OK;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP
nsFilePicker::GetFileURL(nsIURI* *aFileURL)
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
nsFilePicker::GetFiles(nsISimpleEnumerator* *aFiles)
{
    NS_ENSURE_ARG_POINTER(aFiles);

    if (mMode == nsIFilePicker::modeOpenMultiple) {
        return NS_NewArrayEnumerator(aFiles, mFiles);
    }

    return NS_ERROR_FAILURE;
}

/* short show (); */
NS_IMETHODIMP
nsFilePicker::Show(int16_t* aReturn)
{
    nsAutoCString directory;
    if (mDisplayDirectory) {
        mDisplayDirectory->GetNativePath(directory);
    }

    QStringList filters;
    uint32_t count = mFilters.Length();
    for (uint32_t i = 0; i < count; ++i) {
        filters.append(mFilters[i].get());
    }

    QGraphicsWidget* parentWidget = 0;
    if (mParent) {
        parentWidget = static_cast<QGraphicsWidget*>
            (mParent->GetNativeData(NS_NATIVE_WIDGET));
    }

    QWidget* parentQWidget = 0;
    if (parentWidget && parentWidget->scene()) {
        if (parentWidget->scene()->views().size()>0) {
            parentQWidget = parentWidget->scene()->views()[0];
        }
    }

    QStringList files;
    QString selected;

    switch (mMode) {
    case nsIFilePicker::modeOpen:
        selected = MozFileDialog::getOpenFileName(parentQWidget, mCaption,
                                                  directory.get(), filters.join(";"));
        if (selected.isNull()) {
            *aReturn = nsIFilePicker::returnCancel;
            return NS_OK;
        }
        break;
    case nsIFilePicker::modeOpenMultiple:
        files = MozFileDialog::getOpenFileNames(parentQWidget, mCaption,
                                                directory.get(), filters.join(";"));
        if (files.empty()) {
            *aReturn = nsIFilePicker::returnCancel;
            return NS_OK;
        }
        selected = files[0];
        break;
    case nsIFilePicker::modeSave:
    {
        nsCOMPtr<nsIFile> targetFile;
        NS_GetSpecialDirectory(NS_UNIX_XDG_DOCUMENTS_DIR,
                               getter_AddRefs(targetFile));
        if (!targetFile) {
            // failed to get the XDG directory, using $HOME for now
            NS_GetSpecialDirectory(NS_UNIX_HOME_DIR,
                                   getter_AddRefs(targetFile));
        }

        if (targetFile) {
            targetFile->Append(mDefault);
            nsString targetPath;
            targetFile->GetPath(targetPath);

            bool exists = false;
            targetFile->Exists(&exists);
            if (exists) {
                // file exists already create temporary filename
                QTemporaryFile temp(QString::fromUtf16(targetPath.get()));
                temp.open();
                selected = temp.fileName();
                temp.close();
            } else {
                selected = QString::fromUtf16(targetPath.get());
            }
        }
    }
    break;
    case nsIFilePicker::modeGetFolder:
        selected = MozFileDialog::getExistingDirectory(parentQWidget, mCaption,
                                                       directory.get());
        if (selected.isNull()) {
            *aReturn = nsIFilePicker::returnCancel;
            return NS_OK;
        }
    break;
    default:
    break;
    }

    if (!selected.isEmpty()) {
        QString path = QFile::encodeName(selected);
        mFile.Assign(path.toUtf8().data());
    }

    *aReturn = nsIFilePicker::returnOK;
    if (mMode == modeSave) {
        nsCOMPtr<nsIFile> file;
        GetFile(getter_AddRefs(file));
        if (file) {
            bool exists = false;
            file->Exists(&exists);
            if (exists) {
                *aReturn = nsIFilePicker::returnReplace;
            }
        }
    }

    return NS_OK;
}

void nsFilePicker::InitNative(nsIWidget *aParent, const nsAString &aTitle, int16_t mode)
{
    PR_LOG(sFilePickerLog, PR_LOG_DEBUG, ("nsFilePicker::InitNative"));
    nsAutoString str(aTitle);
    mCaption = QString::fromUtf16(str.get());
    mParent = aParent;
    mMode = mode;
}
