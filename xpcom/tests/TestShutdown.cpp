/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsIServiceManager.h"

// Gee this seems simple! It's for testing for memory leaks with Purify.

void main(int argc, char* argv[])
{
    nsresult rv;
    nsIServiceManager* servMgr;
    rv = NS_InitXPCOM(&servMgr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");

    // try loading a component and releasing it to see if it leaks
    if (argc > 1 && argv[1] != nsnull) {
        nsISupports* obj = nsnull;
        rv = nsComponentManager::CreateInstance(argv[1], nsnull,
                                                nsCOMTypeInfo<nsISupports>::GetIID(),
                                                (void**)&obj);
        if (NS_SUCCEEDED(rv)) {
            printf("Successfully created %s\n", argv[1]);
            NS_RELEASE(obj);
        }
        else {
            printf("Failed to create %s (%x)\n", argv[1], rv);
        }
    }

    rv = NS_ShutdownXPCOM(servMgr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    NS_RELEASE(servMgr);
}
