/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *      Denis Issoupov <denis@macadamian.com>
 *
 */
#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIClipboardOwner.h"
#include "nsCOMPtr.h"

#include <qlist.h> 
#include <qcstring.h> 
#include <qmime.h> 

/* Native QT Clipboard wrapper */
class nsClipboard : public nsIClipboard
{
public:
    nsClipboard();
    virtual ~nsClipboard();

    //nsISupports
    NS_DECL_ISUPPORTS

    // nsIClipboard  
    NS_DECL_NSICLIPBOARD

protected:
    NS_IMETHOD SetNativeClipboardData(PRInt32 aWhichClipboard);
    NS_IMETHOD GetNativeClipboardData(nsITransferable *aTransferable,
                                      PRInt32 aWhichClipboard);

    inline nsITransferable *GetTransferable(PRInt32 aWhichClipboard);

    PRBool              mIgnoreEmptyNotification;

    nsCOMPtr<nsIClipboardOwner> mSelectionOwner;
    nsCOMPtr<nsIClipboardOwner> mGlobalOwner;
    nsCOMPtr<nsITransferable>   mSelectionTransferable;
    nsCOMPtr<nsITransferable>   mGlobalTransferable;
};

#endif // nsClipboard_h__
