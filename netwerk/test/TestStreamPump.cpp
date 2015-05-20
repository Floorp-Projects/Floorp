/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsIComponentRegistrar.h"
#include "nsIStreamTransportService.h"
#include "nsIAsyncInputStream.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIRequest.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsStringAPI.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsAutoLock.h"
#include "mozilla/Logging.h"
#include "prprf.h"
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////

//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nullptr;
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

////////////////////////////////////////////////////////////////////////////////

class MyListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS

    MyListener() {}
    virtual ~MyListener() {}

    NS_IMETHOD OnStartRequest(nsIRequest *req, nsISupports *ctx)
    {
        LOG(("MyListener::OnStartRequest\n"));
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                               nsIInputStream *stream,
                               uint64_t offset, uint32_t count)
    {
        LOG(("MyListener::OnDataAvailable [offset=%llu count=%u]\n", offset, count));

        char buf[500];
        nsresult rv;

        while (count) {
            uint32_t n, amt = std::min<uint32_t>(count, sizeof(buf));

            rv = stream->Read(buf, amt, &n);
            if (NS_FAILED(rv)) {
                LOG(("  read returned 0x%08x\n", rv));
                return rv;
            }

            fwrite(buf, n, 1, stdout);
            printf("\n");

            LOG(("  read %u bytes\n", n));
            count -= n;
        }

        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
    {
        LOG(("MyListener::OnStopRequest [status=%x]\n", status));
        QuitPumpingEvents();
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(MyListener,
                  nsIRequestObserver,
                  nsIStreamListener)

////////////////////////////////////////////////////////////////////////////////

/**
 * asynchronously copy file.
 */
static nsresult
RunTest(nsIFile *file, int64_t offset, int64_t length)
{
    nsresult rv;

    LOG(("RunTest\n"));

    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStreamPump> pump;
    rv = NS_NewInputStreamPump(getter_AddRefs(pump), stream, offset, length);
    if (NS_FAILED(rv)) return rv;

    rv = pump->AsyncRead(new MyListener(), nullptr);
    if (NS_FAILED(rv)) return rv;

    PumpEvents();
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
        printf("usage: %s <file-to-read> <start-offset> <read-length>\n", argv[0]);
        return -1;
    }
    char* fileName = argv[1];
    int64_t offset, length;
    int err = PR_sscanf(argv[2], "%lld", &offset);
    if (err == -1) {
      printf("Start offset must be an integer!\n");
      return 1;
    }
    err = PR_sscanf(argv[3], "%lld", &length);
    if (err == -1) {
      printf("Length must be an integer!\n");
      return 1;
    }

    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

        gTestLog = PR_NewLogModule("Test");

        nsCOMPtr<nsIFile> file;
        rv = NS_NewNativeLocalFile(nsDependentCString(fileName), false, getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;

        rv = RunTest(file, offset, length);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunTest failed");

        // give background threads a chance to finish whatever work they may
        // be doing.
        PR_Sleep(PR_SecondsToInterval(1));
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return NS_OK;
}
