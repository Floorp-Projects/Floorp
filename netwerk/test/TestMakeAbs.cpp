/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nspr.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

nsresult ServiceMakeAbsolute(nsIURI *baseURI, char *relativeInfo, char **_retval) {
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    return serv->MakeAbsolute(relativeInfo, baseURI, _retval);
}

nsresult URLMakeAbsolute(nsIURI *baseURI, char *relativeInfo, char **_retval) {
    return baseURI->MakeAbsolute(relativeInfo, _retval);
}

int
main(int argc, char* argv[])
{
    nsresult rv = NS_OK;
    if (argc < 4) {
        printf("usage: %s int (loop count) baseURL relativeSpec\n", argv[0]);
        return -1;
    }

    PRUint32 cycles = atoi(argv[1]);
    char *base = argv[2];
    char *rel  = argv[3];

    rv = NS_AutoregisterComponents();
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> uri;
    rv = serv->NewURI(base, nsnull, getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    char *absURLString;
    PRUint32 i = 0;
    while (i++ < cycles) {
        rv = ServiceMakeAbsolute(uri, rel, &absURLString);
        if (NS_FAILED(rv)) return rv;
        nsAllocator::Free(absURLString);

        rv = URLMakeAbsolute(uri, rel, &absURLString);
        nsAllocator::Free(absURLString);
    }

    return rv;
}
