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
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsStringAPI.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsAutoLock.h"
#include "prlog.h"
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nullptr;
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

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
                               uint32_t offset, uint32_t count)
    {
        LOG(("MyListener::OnDataAvailable [offset=%u count=%u]\n", offset, count));

        char buf[500];
        nsresult rv;

        while (count) {
            uint32_t n, amt = std::min<uint32_t>(count, sizeof(buf));

            rv = stream->Read(buf, amt, &n);
            if (NS_FAILED(rv)) {
                LOG(("  read returned 0x%08x\n", rv));
                return rv;
            }

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

class MyCallbacks : public nsIInterfaceRequestor
                  , public nsIProgressEventSink
{
public:
    NS_DECL_ISUPPORTS

    MyCallbacks() {}
    virtual ~MyCallbacks() {}

    NS_IMETHOD GetInterface(const nsID &iid, void **result)
    {
        return QueryInterface(iid, result);
    }

    NS_IMETHOD OnStatus(nsIRequest *req, nsISupports *ctx, nsresult status,
                        const char16_t *statusArg)
    {
        LOG(("MyCallbacks::OnStatus [status=%x]\n", status));
        return NS_OK;
    }

    NS_IMETHOD OnProgress(nsIRequest *req, nsISupports *ctx,
                          uint64_t progress, uint64_t progressMax)
    {
        LOG(("MyCallbacks::OnProgress [progress=%llu/%llu]\n", progress, progressMax));
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(MyCallbacks,
                  nsIInterfaceRequestor,
                  nsIProgressEventSink)

////////////////////////////////////////////////////////////////////////////////

/**
 * asynchronously copy file.
 */
static nsresult
RunTest(nsIFile *file)
{
    nsresult rv;

    LOG(("RunTest\n"));

    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> uri = do_CreateInstance(kSimpleURICID);
    if (uri)
        uri->SetSpec(NS_LITERAL_CSTRING("foo://bar"));

    nsCOMPtr<nsIChannel> chan;
    rv = NS_NewInputStreamChannel(getter_AddRefs(chan), uri, stream);
    if (NS_FAILED(rv)) return rv;

    rv = chan->SetNotificationCallbacks(new MyCallbacks());
    if (NS_FAILED(rv)) return rv;

    rv = chan->AsyncOpen(new MyListener(), nullptr);
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

    if (argc < 2) {
        printf("usage: %s <file-to-read>\n", argv[0]);
        return -1;
    }
    char* fileName = argv[1];
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

#if defined(PR_LOGGING)
        gTestLog = PR_NewLogModule("Test");
#endif

        nsCOMPtr<nsIFile> file;
        rv = NS_NewNativeLocalFile(nsDependentCString(fileName), false, getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;

        rv = RunTest(file);
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
