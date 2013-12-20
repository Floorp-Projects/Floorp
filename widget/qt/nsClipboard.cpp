/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QApplication>
#include <QMimeData>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QImage>
#include <QImageWriter>
#include <QBuffer>

#include "mozilla/ArrayUtils.h"

#include "nsClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsIInputStream.h"
#include "nsReadableUtils.h"
#include "nsStringStream.h"
#include "nsComponentManagerUtils.h"

#include "imgIContainer.h"
#include "gfxImageSurface.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard)

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsIClipboard(),
                             mSelectionOwner(nullptr),
                             mGlobalOwner(nullptr),
                             mSelectionTransferable(nullptr),
                             mGlobalTransferable(nullptr)
{
    // No implementation needed
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
}

static inline QImage::Format
_gfximage_to_qformat(gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxImageFormatARGB32:
        return QImage::Format_ARGB32_Premultiplied;
    case gfxImageFormatRGB24:
        return QImage::Format_ARGB32;
    case gfxImageFormatRGB16_565:
        return QImage::Format_RGB16;
    default:
        return QImage::Format_Invalid;
    }
}

// nsClipboard::SetNativeClipboardData ie. Copy

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData( nsITransferable *aTransferable,
                                     QClipboard::Mode clipboardMode )
{
    if (nullptr == aTransferable)
    {
        NS_WARNING("nsClipboard::SetNativeClipboardData(): no transferable!");
        return NS_ERROR_FAILURE;
    }

    // get flavor list that includes all flavors that can be written (including
    // ones obtained through conversion)
    nsCOMPtr<nsISupportsArray> flavorList;
    nsresult rv = aTransferable->FlavorsTransferableCanExport( getter_AddRefs(flavorList) );

    if (NS_FAILED(rv))
    {
        NS_WARNING("nsClipboard::SetNativeClipboardData(): no FlavorsTransferable !");
        return NS_ERROR_FAILURE;
    }

    QClipboard *cb = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData;

    uint32_t flavorCount = 0;
    flavorList->Count(&flavorCount);
    bool imageAdded = false;

    for (uint32_t i = 0; i < flavorCount; ++i)
    {
        nsCOMPtr<nsISupports> genericFlavor;
        flavorList->GetElementAt(i,getter_AddRefs(genericFlavor));
        nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));
        
        if (currentFlavor)
        {
            // flavorStr is the mime type
            nsXPIDLCString flavorStr;
            currentFlavor->ToString(getter_Copies(flavorStr));

            // Clip is the data which will be sent to the clipboard
            nsCOMPtr<nsISupports> clip;
            // len is the length of the data
            uint32_t len;

            // Unicode text?
            if (!strcmp(flavorStr.get(), kUnicodeMime))
            {
                rv = aTransferable->GetTransferData(flavorStr,getter_AddRefs(clip),&len);
                nsCOMPtr<nsISupportsString> wideString;
                wideString = do_QueryInterface(clip);
                if (!wideString || NS_FAILED(rv))
                    continue;

                nsAutoString utf16string;
                wideString->GetData(utf16string);
                QString str = QString::fromUtf16(utf16string.get());

                // Add text to the mimeData
                mimeData->setText(str);
            }

            // html?
            else if (!strcmp(flavorStr.get(), kHTMLMime))
            {
                rv = aTransferable->GetTransferData(flavorStr,getter_AddRefs(clip),&len);
                nsCOMPtr<nsISupportsString> wideString;
                wideString = do_QueryInterface(clip);
                if (!wideString || NS_FAILED(rv))
                    continue;

                nsAutoString utf16string;
                wideString->GetData(utf16string);
                QString str = QString::fromUtf16(utf16string.get());

                // Add html to the mimeData
                mimeData->setHtml(str);
            }

            // image?
            else if (!imageAdded // image is added only once to the clipboard
                     && (!strcmp(flavorStr.get(), kNativeImageMime)
                     ||  !strcmp(flavorStr.get(), kPNGImageMime)
                     ||  !strcmp(flavorStr.get(), kJPEGImageMime)
                     ||  !strcmp(flavorStr.get(), kJPGImageMime)
                     ||  !strcmp(flavorStr.get(), kGIFImageMime))
                    )
            {
                // Look through our transfer data for the image
                static const char* const imageMimeTypes[] = {
                    kNativeImageMime, kPNGImageMime, kJPEGImageMime, kJPGImageMime, kGIFImageMime };
                nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive;
                for (uint32_t i = 0; !ptrPrimitive && i < ArrayLength(imageMimeTypes); i++)
                {
                    aTransferable->GetTransferData(imageMimeTypes[i], getter_AddRefs(clip), &len);
                    ptrPrimitive = do_QueryInterface(clip);
                }

                if (!ptrPrimitive)
                    continue;

                nsCOMPtr<nsISupports> primitiveData;
                ptrPrimitive->GetData(getter_AddRefs(primitiveData));
                nsCOMPtr<imgIContainer> image(do_QueryInterface(primitiveData));
                if (!image)  // Not getting an image for an image mime type!?
                   continue;

                nsRefPtr<gfxASurface> surface =
                  image->GetFrame(imgIContainer::FRAME_CURRENT,
                                  imgIContainer::FLAG_SYNC_DECODE);
                if (!surface)
                  continue;

                nsRefPtr<gfxImageSurface> frame(surface->GetAsReadableARGB32ImageSurface());
                if (!frame)
                  continue;

                QImage qImage(frame->Data(),
                              frame->Width(),
                              frame->Height(),
                              frame->Stride(),
                              _gfximage_to_qformat(frame->Format()));

                // Add image to the mimeData
                mimeData->setImageData(qImage);
                imageAdded = true;
            }

            // Other flavors, adding data to clipboard "as is"
            else
            {
                rv = aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(clip), &len);
                // nothing found?
                if (!clip || NS_FAILED(rv))
                    continue;

                void *primitive_data = nullptr;
                nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr.get(), clip,
                                                            &primitive_data, len);

                if (primitive_data)
                {
                    QByteArray data ((const char *)primitive_data, len);
                    // Add data to the mimeData
                    mimeData->setData(flavorStr.get(), data);
                    nsMemory::Free(primitive_data);
                }
            }
        }
    }

    // If we have some mime data, add it to the clipboard
    if(!mimeData->formats().isEmpty())
        cb->setMimeData(mimeData, clipboardMode);
    else
        delete mimeData;

    return NS_OK;
}

// nsClipboard::GetNativeClipboardData ie. Paste
//
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable *aTransferable,
                                    QClipboard::Mode clipboardMode)
{
    if (nullptr == aTransferable)
    {
        NS_WARNING("GetNativeClipboardData: Transferable is null!");
        return NS_ERROR_FAILURE;
    }

    // get flavor list that includes all acceptable flavors (including
    // ones obtained through conversion)
    nsCOMPtr<nsISupportsArray> flavorList;
    nsresult errCode = aTransferable->FlavorsTransferableCanImport(
        getter_AddRefs(flavorList));

    if (NS_FAILED(errCode))
    {
        NS_WARNING("nsClipboard::GetNativeClipboardData(): no FlavorsTransferable!");
        return NS_ERROR_FAILURE;
    }

    QClipboard *cb = QApplication::clipboard();
    const QMimeData *mimeData = cb->mimeData(clipboardMode);

    // Walk through flavors and see which flavor matches the one being pasted
    uint32_t flavorCount;
    flavorList->Count(&flavorCount);
    nsAutoCString foundFlavor;

    for (uint32_t i = 0; i < flavorCount; ++i)
    {
        nsCOMPtr<nsISupports> genericFlavor;
        flavorList->GetElementAt(i,getter_AddRefs(genericFlavor));
        nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface( genericFlavor) );

        if (currentFlavor)
        {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString(getter_Copies(flavorStr));

            // Ok, so which flavor the data being pasted could be?
            // Text?
            if (!strcmp(flavorStr.get(), kUnicodeMime) && mimeData->hasText())
            {
                // Clipboard has text and flavor accepts text, so lets
                // handle the data as text
                foundFlavor = nsAutoCString(flavorStr);

                // Get the text data from clipboard
                QString text = mimeData->text();
                const QChar *unicode = text.unicode();
                // Is there a more correct way to get the size in UTF16?
                uint32_t len = (uint32_t) 2*text.size();

                // And then to genericDataWrapper
                nsCOMPtr<nsISupports> genericDataWrapper;
                nsPrimitiveHelpers::CreatePrimitiveForData(
                        foundFlavor.get(),
                        (void*)unicode,
                        len,
                        getter_AddRefs(genericDataWrapper));
                // Data is good, set it to the transferable
                aTransferable->SetTransferData(foundFlavor.get(),
                                               genericDataWrapper,len);
                // And thats all
                break;
            }

            // html?
            if (!strcmp(flavorStr.get(), kHTMLMime) && mimeData->hasHtml())
            {
                // Clipboard has text/html and flavor accepts text/html, so lets
                // handle the data as text/html
                foundFlavor = nsAutoCString(flavorStr);

                // Get the text data from clipboard
                QString html = mimeData->html();
                const QChar *unicode = html.unicode();
                // Is there a more correct way to get the size in UTF16?
                uint32_t len = (uint32_t) 2*html.size();

                // And then to genericDataWrapper
                nsCOMPtr<nsISupports> genericDataWrapper;
                nsPrimitiveHelpers::CreatePrimitiveForData(
                        foundFlavor.get(),
                        (void*)unicode,
                        len,
                        getter_AddRefs(genericDataWrapper));
                // Data is good, set it to the transferable
                aTransferable->SetTransferData(foundFlavor.get(),
                                               genericDataWrapper,len);
                // And thats all
                break;
            }

            // Image?
            if ((  !strcmp(flavorStr.get(), kJPEGImageMime)
                || !strcmp(flavorStr.get(), kJPGImageMime)
                || !strcmp(flavorStr.get(), kPNGImageMime)
                || !strcmp(flavorStr.get(), kGIFImageMime))
                && mimeData->hasImage())
            {
                // Try to retrieve an image from clipboard
                QImage image = cb->image();
                if(image.isNull())
                    continue;

                // Lets set the image format
                QByteArray imageFormat;
                if (!strcmp(flavorStr.get(), kJPEGImageMime) || !strcmp(flavorStr.get(), kJPGImageMime))
                    imageFormat = "jpeg";
                else if (!strcmp(flavorStr.get(), kPNGImageMime))
                    imageFormat = "png";
                else if (!strcmp(flavorStr.get(), kGIFImageMime))
                    imageFormat = "gif";
                else
                    continue;

                // Write image from clippboard to a QByteArrayBuffer
                QByteArray imageData;
                QBuffer imageBuffer(&imageData);
                QImageWriter imageWriter(&imageBuffer, imageFormat);
                if(!imageWriter.write(image))
                    continue;

                // Add the data to inputstream
                nsCOMPtr<nsIInputStream> byteStream;
                NS_NewByteInputStream(getter_AddRefs(byteStream), imageData.constData(),
                                      imageData.size(), NS_ASSIGNMENT_COPY);
                // Data is good, set it to the transferable
                aTransferable->SetTransferData(flavorStr, byteStream, sizeof(nsIInputStream*));

                imageBuffer.close();

                // And thats all
                break;
            }

            // Other mimetype?
            // Trying to forward the data "as is"
            if(mimeData->hasFormat(flavorStr.get()))
            {
                // get the data from the clipboard
                QByteArray clipboardData = mimeData->data(flavorStr.get());
                // And add it to genericDataWrapper
                nsCOMPtr<nsISupports> genericDataWrapper;
                nsPrimitiveHelpers::CreatePrimitiveForData(
                        foundFlavor.get(),
                        (void*) clipboardData.data(),
                        clipboardData.size(),
                        getter_AddRefs(genericDataWrapper));

                // Data is good, set it to the transferable
                aTransferable->SetTransferData(foundFlavor.get(),
                                               genericDataWrapper,clipboardData.size());
                // And thats all
                break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char** aFlavorList, uint32_t aLength,
                                    int32_t aWhichClipboard, bool *_retval)
{
    *_retval = false;
    if (aWhichClipboard != kGlobalClipboard)
        return NS_OK;

    // Which kind of data in the clipboard
    QClipboard *cb = QApplication::clipboard();
    const QMimeData *mimeData = cb->mimeData();
    const char *flavor=nullptr;
    QStringList formats = mimeData->formats();
    for (uint32_t i = 0; i < aLength; ++i)
    {
        flavor = aFlavorList[i];
        if (flavor)
        {
            QString qflavor(flavor);

            if (strcmp(flavor,kTextMime) == 0)
            {
                NS_WARNING("DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD");
            }

            // QClipboard says it has text/plain, mozilla wants to
            // know if the data is text/unicode -> interpret text/plain to text/unicode
            if (formats.contains(qflavor) ||
                strcmp(flavor, kUnicodeMime) == 0)
            {
                // A match has been found, return'
                *_retval = true;
                break;
            }
        }
    }
    return NS_OK;
}

/**
 * Sets the transferable object
 */
NS_IMETHODIMP
nsClipboard::SetData(nsITransferable *aTransferable,
                     nsIClipboardOwner *aOwner,
                     int32_t aWhichClipboard)
{
    // See if we can short cut
    if (
        (aWhichClipboard == kGlobalClipboard
           && aTransferable == mGlobalTransferable.get()
           && aOwner == mGlobalOwner.get()
        )
       ||
        (aWhichClipboard == kSelectionClipboard
         && aTransferable == mSelectionTransferable.get()
         && aOwner == mSelectionOwner.get()
        )
       )
    {
        return NS_OK;
    }

    nsresult rv;
    if (!mPrivacyHandler) {
      rv = NS_NewClipboardPrivacyHandler(getter_AddRefs(mPrivacyHandler));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = mPrivacyHandler->PrepareDataForClipboard(aTransferable);
    NS_ENSURE_SUCCESS(rv, rv);

    EmptyClipboard(aWhichClipboard);

    QClipboard::Mode mode;

    if (kGlobalClipboard == aWhichClipboard)
    {
        mGlobalOwner = aOwner;
        mGlobalTransferable = aTransferable;

        mode = QClipboard::Clipboard;
    }
    else
    {
        mSelectionOwner = aOwner;
        mSelectionTransferable = aTransferable;

        mode = QClipboard::Selection;
    }
    return SetNativeClipboardData( aTransferable, mode );
}

/**
 * Gets the transferable object
 */
NS_IMETHODIMP
nsClipboard::GetData(nsITransferable *aTransferable, int32_t aWhichClipboard)
{
    if (nullptr != aTransferable)
    {
        QClipboard::Mode mode;
        if (kGlobalClipboard == aWhichClipboard)
        {
            mode = QClipboard::Clipboard;
        }
        else
        {
            mode = QClipboard::Selection;
        }
        return GetNativeClipboardData(aTransferable, mode);
    }
    else
    {
        NS_WARNING("nsClipboard::GetData(), aTransferable is NULL.");
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
    if (aWhichClipboard == kSelectionClipboard)
    {
        if (mSelectionOwner)
        {
            mSelectionOwner->LosingOwnership(mSelectionTransferable);
            mSelectionOwner = nullptr;
        }
        mSelectionTransferable = nullptr;
    }
    else
    {
        if (mGlobalOwner)
        {
            mGlobalOwner->LosingOwnership(mGlobalTransferable);
            mGlobalOwner = nullptr;
        }
        mGlobalTransferable = nullptr;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    QClipboard *cb = QApplication::clipboard();
    if (cb->supportsSelection())
    {
        *_retval = true; // we support the selection clipboard 
    }
    else
    {
        *_retval = false;
    }

    return NS_OK;
}
