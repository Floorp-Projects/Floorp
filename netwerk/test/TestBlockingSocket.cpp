/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsIComponentRegistrar.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsStringAPI.h"
#include "nsIFile.h"
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
static PRLogModuleInfo *gTestLog = nullptr;
#endif
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

static nsresult
RunBlockingTest(const nsACString &host, int32_t port, nsIFile *file)
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
    rv = sts->CreateTransport(nullptr, 0, host, port, nullptr, getter_AddRefs(trans));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> output;
    rv = trans->OpenOutputStream(nsITransport::OPEN_BLOCKING, 100, 10, getter_AddRefs(output));
    if (NS_FAILED(rv)) return rv;

    char buf[120];
    uint32_t nr, nw;
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
    int32_t port = atoi(argv[2]);
    char* fileName = argv[3];
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);

#if defined(PR_LOGGING)
        gTestLog = PR_NewLogModule("Test");
#endif

        nsCOMPtr<nsIFile> file;
        rv = NS_NewNativeLocalFile(nsDependentCString(fileName), false, getter_AddRefs(file));
        if (NS_FAILED(rv)) return -1;

        rv = RunBlockingTest(nsDependentCString(hostName), port, file);
#if defined(PR_LOGGING)
        if (NS_FAILED(rv))
            LOG(("RunBlockingTest failed [rv=%x]\n", rv));
#endif

        // give background threads a chance to finish whatever work they may
        // be doing.
        LOG(("sleeping for 5 seconds...\n"));
        PR_Sleep(PR_SecondsToInterval(5));
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}
