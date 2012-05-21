/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"

// Gee this seems simple! It's for testing for memory leaks with Purify.

void main(int argc, char* argv[])
{
    nsresult rv;
    nsIServiceManager* servMgr;
    rv = NS_InitXPCOM2(&servMgr, NULL, NULL);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");

    // try loading a component and releasing it to see if it leaks
    if (argc > 1 && argv[1] != nsnull) {
        char* cidStr = argv[1];
        nsISupports* obj = nsnull;
        if (cidStr[0] == '{') {
            nsCID cid;
            cid.Parse(cidStr);
            rv = CallCreateInstance(cid, &obj);
        }
        else {
            // contractID case:
            rv = CallCreateInstance(cidStr, &obj);
        }
        if (NS_SUCCEEDED(rv)) {
            printf("Successfully created %s\n", cidStr);
            NS_RELEASE(obj);
        }
        else {
            printf("Failed to create %s (%x)\n", cidStr, rv);
        }
    }

    rv = NS_ShutdownXPCOM(servMgr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
}
