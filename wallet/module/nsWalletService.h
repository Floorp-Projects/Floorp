/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsWalletService_h___
#define nsWalletService_h___

#include "nsIWalletService.h"

class nsWalletlibService : public nsIWalletService {

public:
    NS_DECL_ISUPPORTS
    nsWalletlibService();

    /* Implementation of the nsIWalletService interface */
    NS_IMETHOD WALLET_PreEdit(nsIURL* url);
    NS_IMETHOD WALLET_Prefill(nsIPresShell* shell, PRBool quick);
    NS_IMETHOD WALLET_Capture(nsIDocument* doc, nsString name, nsString value);
    NS_IMETHOD WALLET_OKToCapture(PRBool* result, PRInt32 count, char* URLName);
#ifdef junk
    NS_IMETHOD SI_DisplaySignonInfoAsHTML();
    NS_IMETHOD SI_RememberSignonData
        (char* URLName, LO_FormSubmitData *submit);
    NS_IMETHOD SI_RestoreSignonData
        (char* URLNAME, char* name, char** value);
#endif

protected:
    virtual ~nsWalletlibService();

private:
};


#endif /* nsWalletService_h___ */
