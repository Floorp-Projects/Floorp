/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsIComponentRegistrar.h"
#include "nsPISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIRequest.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsStringAPI.h"
#include "nsIDNSService.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsAutoLock.h"
#include "mozilla/Logging.h"

////////////////////////////////////////////////////////////////////////////////

//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nullptr;
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

class MyHandler : public nsIOutputStreamCallback
                , public nsIInputStreamCallback
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    MyHandler(const char *path,
              nsIAsyncInputStream *in,
              nsIAsyncOutputStream *out)
        : mInput(in)
        , mOutput(out)
        , mWriteOffset(0)
        {
            mBuf.AssignLiteral("GET ");
            mBuf.Append(path);
            mBuf.AppendLiteral(" HTTP/1.0\r\n\r\n");
        }
    virtual ~MyHandler() {}

    // called on any thread
    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *out)
    {
        LOG(("OnOutputStreamReady\n"));

        nsresult rv;
        uint32_t n, count = mBuf.Length() - mWriteOffset;

        rv = out->Write(mBuf.get() + mWriteOffset, count, &n);

        LOG(("  write returned [rv=%x count=%u]\n", rv, n));

        if (NS_FAILED(rv) || (n == 0)) {
            if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
                LOG(("  done writing; starting to read\n"));
                mInput->AsyncWait(this, 0, 0, nullptr);
                return NS_OK;
            }
        }

        mWriteOffset += n;

        return out->AsyncWait(this, 0, 0, nullptr);
    }

    // called on any thread
    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *in)
    {
        LOG(("OnInputStreamReady\n"));

        nsresult rv;
        uint32_t n;
        char buf[500];

        rv = in->Read(buf, sizeof(buf), &n);

        LOG(("  read returned [rv=%x count=%u]\n", rv, n));

        if (NS_FAILED(rv) || (n == 0)) {
            if (rv != NS_BASE_STREAM_WOULD_BLOCK) {
                QuitPumpingEvents();
                return NS_OK;
            }
        }

        return in->AsyncWait(this, 0, 0, nullptr);
    }

private:
    nsCOMPtr<nsIAsyncInputStream>  mInput;
    nsCOMPtr<nsIAsyncOutputStream> mOutput;
    nsCString mBuf;
    uint32_t  mWriteOffset;
};

NS_IMPL_ISUPPORTS(MyHandler,
                  nsIOutputStreamCallback,
                  nsIInputStreamCallback)

////////////////////////////////////////////////////////////////////////////////

/**
 * create transport, open streams, and close
 */
static nsresult
RunCloseTest(nsISocketTransportService *sts,
             const char *host, int port,
             uint32_t inFlags, uint32_t outFlags)
{
    nsresult rv;

    LOG(("RunCloseTest\n"));

    nsCOMPtr<nsISocketTransport> transport;
    rv = sts->CreateTransport(nullptr, 0,
                              nsDependentCString(host), port, nullptr,
                              getter_AddRefs(transport));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> in;
    rv = transport->OpenInputStream(inFlags, 0, 0, getter_AddRefs(in));
    nsCOMPtr<nsIAsyncInputStream> asyncIn = do_QueryInterface(in, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> out;
    rv = transport->OpenOutputStream(outFlags, 0, 0, getter_AddRefs(out));
    nsCOMPtr<nsIAsyncOutputStream> asyncOut = do_QueryInterface(out, &rv);
    if (NS_FAILED(rv)) return rv;

    LOG(("waiting 1 second before closing transport and streams...\n"));
    PR_Sleep(PR_SecondsToInterval(1));
    
    // let nsCOMPtr destructors close everything...
    return NS_OK;
}


/**
 * asynchronously read socket stream
 */
static nsresult
RunTest(nsISocketTransportService *sts,
        const char *host, int port, const char *path,
        uint32_t inFlags, uint32_t outFlags)
{
    nsresult rv;

    LOG(("RunTest\n"));

    nsCOMPtr<nsISocketTransport> transport;
    rv = sts->CreateTransport(nullptr, 0,
                              nsDependentCString(host), port, nullptr,
                              getter_AddRefs(transport));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> in;
    rv = transport->OpenInputStream(inFlags, 0, 0, getter_AddRefs(in));
    nsCOMPtr<nsIAsyncInputStream> asyncIn = do_QueryInterface(in, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> out;
    rv = transport->OpenOutputStream(outFlags, 0, 0, getter_AddRefs(out));
    nsCOMPtr<nsIAsyncOutputStream> asyncOut = do_QueryInterface(out, &rv);
    if (NS_FAILED(rv)) return rv;

    MyHandler *handler = new MyHandler(path, asyncIn, asyncOut);
    if (handler == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(handler);

    rv = asyncOut->AsyncWait(handler, 0, 0, nullptr);

    if (NS_SUCCEEDED(rv))
        PumpEvents();

    NS_RELEASE(handler);

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
        printf("usage: TestSocketTransport <host> <port> <path>\n");
        return -1;
    }

    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

        gTestLog = PR_NewLogModule("Test");

        // Make sure the DNS service is initialized on the main thread
        nsCOMPtr<nsIDNSService> dns =
                 do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsPISocketTransportService> sts =
            do_GetService(kSocketTransportServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        LOG(("phase 1 tests...\n"));

        LOG(("flags = { OPEN_UNBUFFERED, OPEN_UNBUFFERED }\n"));
        rv = RunCloseTest(sts, argv[1], atoi(argv[2]),
                          nsITransport::OPEN_UNBUFFERED,
                          nsITransport::OPEN_UNBUFFERED);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunCloseTest failed");

        LOG(("flags = { OPEN_BUFFERED, OPEN_UNBUFFERED }\n"));
        rv = RunCloseTest(sts, argv[1], atoi(argv[2]),
                          0 /* nsITransport::OPEN_BUFFERED */,
                          nsITransport::OPEN_UNBUFFERED);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunCloseTest failed");

        LOG(("flags = { OPEN_UNBUFFERED, OPEN_BUFFERED }\n"));
        rv = RunCloseTest(sts, argv[1], atoi(argv[2]),
                          nsITransport::OPEN_UNBUFFERED,
                          0 /*nsITransport::OPEN_BUFFERED */);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunCloseTest failed");

        LOG(("flags = { OPEN_BUFFERED, OPEN_BUFFERED }\n"));
        rv = RunCloseTest(sts, argv[1], atoi(argv[2]),
                          0 /*nsITransport::OPEN_BUFFERED */,
                          0 /*nsITransport::OPEN_BUFFERED */);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunCloseTest failed");

        LOG(("calling Shutdown on socket transport service:\n"));
        sts->Shutdown();

        LOG(("calling Init on socket transport service:\n"));
        sts->Init();

        LOG(("phase 2 tests...\n"));

        LOG(("flags = { OPEN_UNBUFFERED, OPEN_UNBUFFERED }\n"));
        rv = RunTest(sts, argv[1], atoi(argv[2]), argv[3],
                     nsITransport::OPEN_UNBUFFERED,
                     nsITransport::OPEN_UNBUFFERED);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunTest failed");

        LOG(("flags = { OPEN_BUFFERED, OPEN_UNBUFFERED }\n"));
        rv = RunTest(sts, argv[1], atoi(argv[2]), argv[3],
                     0 /* nsITransport::OPEN_BUFFERED */,
                     nsITransport::OPEN_UNBUFFERED);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunTest failed");

        LOG(("flags = { OPEN_UNBUFFERED, OPEN_BUFFERED }\n"));
        rv = RunTest(sts, argv[1], atoi(argv[2]), argv[3],
                     nsITransport::OPEN_UNBUFFERED,
                     0 /*nsITransport::OPEN_BUFFERED */);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunTest failed");

        LOG(("flags = { OPEN_BUFFERED, OPEN_BUFFERED }\n"));
        rv = RunTest(sts, argv[1], atoi(argv[2]), argv[3],
                     0 /*nsITransport::OPEN_BUFFERED */,
                     0 /*nsITransport::OPEN_BUFFERED */);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunTest failed");

        LOG(("waiting 1 second before calling Shutdown...\n"));
        PR_Sleep(PR_SecondsToInterval(1));

        LOG(("calling Shutdown on socket transport service:\n"));
        sts->Shutdown();

        // give background threads a chance to finish whatever work they may
        // be doing.
        LOG(("waiting 1 second before exiting...\n"));
        PR_Sleep(PR_SecondsToInterval(1));
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}
