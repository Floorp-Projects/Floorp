/* ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   Denis Issoupov <denis@macadamian.com>
 *   John C. Griggs <johng@corel.com>
 *   Dan Rosen <dr@netscape.com>
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

#include <QApplication>
#include <QMimeData>
#include <QString>
#include <QStringList>

#include "nsClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard)

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsIClipboard(),
                             mSelectionOwner(nsnull),
                             mGlobalOwner(nsnull),
                             mSelectionTransferable(nsnull),
                             mGlobalTransferable(nsnull)
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


// nsClipboard::SetNativeClipboardData ie. Copy

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData( nsITransferable *aTransferable,
                                     QClipboard::Mode clipboardMode )
{
    if (nsnull == aTransferable)
    {
        qDebug("nsClipboard::SetNativeClipboardData(): no transferable!");
        return NS_ERROR_FAILURE;
    }

    // get flavor list that includes all flavors that can be written (including
    // ones obtained through conversion)
    nsCOMPtr<nsISupportsArray> flavorList;
    nsresult errCode = aTransferable->FlavorsTransferableCanExport( getter_AddRefs(flavorList) );

    if (NS_FAILED(errCode))
    {
        qDebug("nsClipboard::SetNativeClipboardData(): no FlavorsTransferable !");
        return NS_ERROR_FAILURE;
    }

    QClipboard *cb = QApplication::clipboard();
    PRUint32 flavorCount = 0;
    flavorList->Count(&flavorCount);

    for (PRUint32 i = 0; i < flavorCount; ++i)
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
            PRUint32 len;

            // Unicode text?
            if (!strcmp(flavorStr.get(), kUnicodeMime))
            {
                aTransferable->GetTransferData(flavorStr,getter_AddRefs(clip),&len);
                nsCOMPtr<nsISupportsString> wideString;
                wideString = do_QueryInterface(clip);
                if (!wideString)
                {
                    return NS_ERROR_FAILURE;
                }

                nsAutoString utf16string;
                wideString->GetData(utf16string);
                QString str = QString::fromUtf16(utf16string.get());

                // Set the date to the clipboard
                cb->setText(str, clipboardMode);
                break;
            }

            if ( !strcmp(flavorStr.get(), kNativeImageMime)
              || !strcmp(flavorStr.get(), kPNGImageMime)
              || !strcmp(flavorStr.get(), kJPEGImageMime)
              || !strcmp(flavorStr.get(), kGIFImageMime))
            {
                qDebug("nsClipboard::SetNativeClipboardData(): Copying image data not implemented!");
            }
        }
    }

    return NS_OK;
}

// nsClipboard::GetNativeClipboardData ie. Paste
//
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable *aTransferable,
                                    QClipboard::Mode clipboardMode)
{
    if (nsnull == aTransferable)
    {
        qDebug("  GetNativeClipboardData: Transferable is null!");
        return NS_ERROR_FAILURE;
    }

    // get flavor list that includes all acceptable flavors (including
    // ones obtained through conversion)
    nsCOMPtr<nsISupportsArray> flavorList;
    nsresult errCode = aTransferable->FlavorsTransferableCanImport(
        getter_AddRefs(flavorList));

    if (NS_FAILED(errCode))
    {
        qDebug("nsClipboard::GetNativeClipboardData(): no FlavorsTransferable %i !",
               errCode);
        return NS_ERROR_FAILURE;
    }

    QClipboard *cb = QApplication::clipboard();
    const QMimeData *mimeData = cb->mimeData(clipboardMode);

    // Walk through flavors and see which flavor matches the one being pasted
    PRUint32 flavorCount;
    flavorList->Count(&flavorCount);
    nsCAutoString foundFlavor;

    for (PRUint32 i = 0; i < flavorCount; ++i)
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
            if (!strcmp(flavorStr.get(), kUnicodeMime)) 
            {
                if (mimeData->hasText())
                {
                    // Clipboard has text and flavor accepts text, so lets
                    // handle the data as text
                    foundFlavor = nsCAutoString(flavorStr);

                    // Get the text data from clipboard
                    QString text = mimeData->text();
                    const QChar *unicode = text.unicode();
                    // Is there a more correct way to get the size in UTF16?
                    PRUint32 len = (PRUint32) 2*text.size();

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
            }

            // Image?
            if (!strcmp(flavorStr.get(), kJPEGImageMime)
             || !strcmp(flavorStr.get(), kPNGImageMime)
             || !strcmp(flavorStr.get(), kGIFImageMime))
            {
                qDebug("nsClipboard::GetNativeClipboardData(): Pasting image data not implemented!");
                break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char** aFlavorList, PRUint32 aLength,
                                    PRInt32 aWhichClipboard, PRBool *_retval)
{
    *_retval = PR_FALSE;
    if (aWhichClipboard != kGlobalClipboard)
        return NS_OK;

    // Which kind of data in the clipboard
    QClipboard *cb = QApplication::clipboard();
    const QMimeData *mimeData = cb->mimeData();
    const char *flavor=NULL;
    QStringList formats = mimeData->formats();
    // Temp QString for comparison
    QString utf8text("text/plain;charset=utf-8");
    // And is there matching flavor?
    for (PRUint32 i = 0; i < aLength; ++i)
    {
        flavor = aFlavorList[i];
        if (flavor)
        {
            QString qflavor(flavor);

            if (strcmp(flavor,kTextMime) == 0)
            {
                NS_WARNING("DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD");
            }

            // QClipboard says it has text/plain;charset=utf-8 data, mozilla wants to
            // know if the data is text/unicode -> interpret text/plain;charset=utf-8 to text/unicode
            if (formats.contains(qflavor) ||
                ((strcmp(flavor, kUnicodeMime) == 0) && formats.contains(utf8text)))
            {
                // A match has been found, return'
                *_retval = PR_TRUE;
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
                     PRInt32 aWhichClipboard)
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
nsClipboard::GetData(nsITransferable *aTransferable, PRInt32 aWhichClipboard)
{
    if (nsnull != aTransferable)
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
        qDebug("  nsClipboard::GetData(), aTransferable is NULL.");
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(PRInt32 aWhichClipboard)
{
    if (aWhichClipboard == kSelectionClipboard)
    {
        if (mSelectionOwner)
        {
            mSelectionOwner->LosingOwnership(mSelectionTransferable);
            mSelectionOwner = nsnull;
        }
        mSelectionTransferable = nsnull;
    }
    else
    {
        if (mGlobalOwner)
        {
            mGlobalOwner->LosingOwnership(mGlobalTransferable);
            mGlobalOwner = nsnull;
        }
        mGlobalTransferable = nsnull;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    QClipboard *cb = QApplication::clipboard();
    if (cb->supportsSelection())
    {
        *_retval = PR_TRUE; // we support the selection clipboard 
    }
    else
    {
        *_retval = PR_FALSE;
    }

    return NS_OK;
}
