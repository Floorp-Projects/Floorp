/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

    PRUint32 cycles = atoi(argv[1]);
    char *base = argv[2];
    char *rel  = argv[3];
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nsnull);

        nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> uri;
        rv = serv->NewURI(base, nsnull, getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        char *absURLString;
        PRUint32 i = 0;
        while (i++ < cycles) {
            rv = ServiceMakeAbsolute(uri, rel, &absURLString);
            if (NS_FAILED(rv)) return rv;
            nsMemory::Free(absURLString);

            rv = URLMakeAbsolute(uri, rel, &absURLString);
            nsMemory::Free(absURLString);
        }
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nsnull);
    return rv;
}
