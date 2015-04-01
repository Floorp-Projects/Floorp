/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIURI.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

nsresult ServiceMakeAbsolute(nsIURI *baseURI, char *relativeInfo, char **_retval) {
    nsresult rv;
    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
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

    uint32_t cycles = atoi(argv[1]);
    char *base = argv[2];
    char *rel  = argv[3];
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

        nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> uri;
        rv = serv->NewURI(base, nullptr, getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        char *absURLString;
        uint32_t i = 0;
        while (i++ < cycles) {
            rv = ServiceMakeAbsolute(uri, rel, &absURLString);
            if (NS_FAILED(rv)) return rv;
            free(absURLString);

            rv = URLMakeAbsolute(uri, rel, &absURLString);
            free(absURLString);
        }
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nullptr);
    return rv;
}
