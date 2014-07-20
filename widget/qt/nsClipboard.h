/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIClipboardOwner.h"
#include "nsCOMPtr.h"

#include <qclipboard.h>

/* Native Qt Clipboard wrapper */
class nsClipboard : public nsIClipboard
{
public:
    nsClipboard();

    //nsISupports
    NS_DECL_ISUPPORTS

    // nsIClipboard
    NS_DECL_NSICLIPBOARD

protected:
    virtual ~nsClipboard();

    NS_IMETHOD SetNativeClipboardData(nsITransferable *aTransferable,
                                      QClipboard::Mode cbMode);
    NS_IMETHOD GetNativeClipboardData(nsITransferable *aTransferable,
                                      QClipboard::Mode cbMode);

    nsCOMPtr<nsIClipboardOwner> mSelectionOwner;
    nsCOMPtr<nsIClipboardOwner> mGlobalOwner;
    nsCOMPtr<nsITransferable>   mSelectionTransferable;
    nsCOMPtr<nsITransferable>   mGlobalTransferable;
};

#endif // nsClipboard_h__
