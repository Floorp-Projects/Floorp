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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
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

#include <string.h>

#include "nscore.h"
#include "plstr.h"
#include "prlink.h"

#include "nsSound.h"

#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"

#include <qapplication.h>
#include <stdio.h>
#include <unistd.h>

NS_IMPL_ISUPPORTS1(nsSound, nsISound)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
}

nsSound::~nsSound()
{
}

NS_IMETHODIMP
nsSound::Init()
{
    return NS_OK;
}

NS_METHOD nsSound::Beep()
{
    QApplication::beep();

    return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsSound::PlaySystemSound(const char *aSoundAlias)
{
    if (!aSoundAlias)
        return NS_ERROR_FAILURE;
    if (strcmp(aSoundAlias, "_moz_mailbeep") == 0) {
        QApplication::beep();
        return NS_OK;
    }

    return NS_ERROR_FAILURE;

//     nsresult rv;
//     nsCOMPtr <nsIURI> fileURI;

//     // create a nsILocalFile and then a nsIFileURL from that
//     nsCOMPtr <nsILocalFile> soundFile;
//     rv = NS_NewNativeLocalFile(nsDependentCString(aSoundAlias), PR_TRUE,
//                                getter_AddRefs(soundFile));
//     NS_ENSURE_SUCCESS(rv,rv);

//     rv = NS_NewFileURI(getter_AddRefs(fileURI), soundFile);
//     NS_ENSURE_SUCCESS(rv,rv);

//     nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI,&rv);
//     NS_ENSURE_SUCCESS(rv,rv);
//     rv = Play(fileURL);
//     return rv;
}
