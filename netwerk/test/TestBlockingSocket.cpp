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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include "TestCommon.h"
#include "nsIComponentRegistrar.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "prenv.h"
#include "prthread.h"
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nsnull;
#endif
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

static nsresult
RunBlockingTest(const nsACString &host, PRInt32 port, nsIFile *file)
{
    nsresult rv;

    LOG(("RunBlockingTest\n"));

    nsCOMPtr<nsISocketTransportService> sts =
        do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> input;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(input), file);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISocketTransport> trans;
    rv = sts->CreateTransport(nsnull, 0, host, port, nsnull, getter_AddRefs(trans));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> output;
    rv = trans->OpenOutputStream(nsITransport::OPEN_BLOCKING, 100, 10, getter_AddRefs(output));
    if (NS_FAILED(rv)) return rv;

    char buf[120];
    PRUint32 nr, nw;
    for (;;) {
        rv = input->Read(buf, sizeof(buf), &nr);
        if (NS_FAILED(rv) || (nr == 0)) return rv;

/*
        const char *p = buf;
        while (nr) {
            rv = output->Write(p, nr, &nw);
            if (NS_FAILED(rv)) return rv;

            nr -= nw;
            p  += nw;
        }
*/
        
        rv = output->Write(buf, nr, &nw);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(nr == nw, "not all written");
    }

    LOG(("  done copying data.\n"));
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

int
main(int argc, char* argv[])
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv;

    if (argc < 4) {
        printf("usage: %s <host> <port> <file-to-read>\n", argv[0]);
        return -1;
    }
    char* hostName = argv[1];
    PRInt32 port = atoi(argv[2]);
    char* fileName = argv[3];
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nsnull);

#if defined(PR_LOGGING)
        gTestLog = PR_NewLogModule("Test");
#endif

        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewNativeLocalFile(nsDependentCString(fileName), PR_FALSE, getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;

        rv = RunBlockingTest(nsDependentCString(hostName), port, file);
        if (NS_FAILED(rv))
            LOG(("RunBlockingTest failed [rv=%x]\n", rv));

        // give background threads a chance to finish whatever work they may
        // be doing.
        LOG(("sleeping for 5 seconds...\n"));
        PR_Sleep(PR_SecondsToInterval(5));
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return NS_OK;
}
