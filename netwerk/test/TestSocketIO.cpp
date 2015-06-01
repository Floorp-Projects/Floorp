/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <signal.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "nspr.h"
#include "nscore.h"
#include "nsISocketTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsITransport.h"
#include "nsIRequest.h"
#include "nsIStreamProvider.h"
#include "nsIStreamListener.h"
#include "nsIPipe.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsIByteArrayInputStream.h"

static PRLogModuleInfo *gTestSocketIOLog;
#define LOG(args) MOZ_LOG(gTestSocketIOLog, mozilla::LogLevel::Debug, args)

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static PRTime gElapsedTime;
static int gKeepRunning = 1;
static nsIEventQueue* gEventQ = nullptr;

//
//----------------------------------------------------------------------------
// Test Listener
//----------------------------------------------------------------------------
//

class TestListener : public nsIStreamListener
{
public:
    TestListener() {}
    virtual ~TestListener() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
};

NS_IMPL_ISUPPORTS(TestListener,
                  nsIRequestObserver,
                  nsIStreamListener);

NS_IMETHODIMP
TestListener::OnStartRequest(nsIRequest* request, nsISupports* context)
{
    LOG(("TestListener::OnStartRequest\n"));
    return NS_OK;
}

NS_IMETHODIMP
TestListener::OnDataAvailable(nsIRequest* request,
                              nsISupports* context,
                              nsIInputStream *aIStream, 
                              uint32_t aSourceOffset,
                              uint32_t aLength)
{
    LOG(("TestListener::OnDataAvailable [offset=%u length=%u]\n",
        aSourceOffset, aLength));
    char buf[1025];
    uint32_t amt;
    while (1) {
        aIStream->Read(buf, 1024, &amt);
        if (amt == 0)
            break;
        buf[amt] = '\0';
        puts(buf);
    }
    return NS_OK;
}

NS_IMETHODIMP
TestListener::OnStopRequest(nsIRequest* request, nsISupports* context,
                            nsresult aStatus)
{
    LOG(("TestListener::OnStopRequest [aStatus=%x]\n", aStatus));
    gKeepRunning = 0;
    return NS_OK;
}

//
//----------------------------------------------------------------------------
// Test Provider
//----------------------------------------------------------------------------
//

class TestProvider : public nsIStreamProvider
{
public:
    TestProvider(char *data);
    virtual ~TestProvider();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMPROVIDER

protected:
    nsCOMPtr<nsIByteArrayInputStream> mData;
};

NS_IMPL_ISUPPORTS(TestProvider,
                  nsIStreamProvider,
                  nsIRequestObserver)

TestProvider::TestProvider(char *data)
{
    NS_NewByteArrayInputStream(getter_AddRefs(mData), data, strlen(data));
    LOG(("Constructing TestProvider [this=%p]\n", this));
}

TestProvider::~TestProvider()
{
    LOG(("Destroying TestProvider [this=%p]\n", this));
}

NS_IMETHODIMP
TestProvider::OnStartRequest(nsIRequest* request, nsISupports* context)
{
    LOG(("TestProvider::OnStartRequest [this=%p]\n", this));
    return NS_OK;
}

NS_IMETHODIMP
TestProvider::OnStopRequest(nsIRequest* request, nsISupports* context,
                            nsresult aStatus)
{
    LOG(("TestProvider::OnStopRequest [status=%x]\n", aStatus));

    nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(new TestListener());

    if (NS_SUCCEEDED(aStatus)) {
        nsCOMPtr<nsITransportRequest> treq = do_QueryInterface(request);
        nsCOMPtr<nsITransport> transport;
        treq->GetTransport(getter_AddRefs(transport));
        if (transport) {
            nsCOMPtr<nsIRequest> readRequest;
            transport->AsyncRead(listener, nullptr, 0, 0, 0, getter_AddRefs(readRequest));
        }
    } else
        gKeepRunning = 0;

    return NS_OK;
}

NS_IMETHODIMP
TestProvider::OnDataWritable(nsIRequest *request, nsISupports *context,
                             nsIOutputStream *output, uint32_t offset, uint32_t count)
{
    LOG(("TestProvider::OnDataWritable [offset=%u, count=%u]\n", offset, count));
    uint32_t writeCount;
    nsresult rv = output->WriteFrom(mData, count, &writeCount);
    // Zero bytes written on success indicates EOF
    if (NS_SUCCEEDED(rv) && (writeCount == 0))
        return NS_BASE_STREAM_CLOSED;
    return rv;
}

//
//----------------------------------------------------------------------------
// Synchronous IO
//----------------------------------------------------------------------------
//
nsresult
WriteRequest(nsIOutputStream *os, const char *request)
{
    LOG(("WriteRequest [request=%s]\n", request));
    uint32_t n;
    return os->Write(request, strlen(request), &n);
}

nsresult
ReadResponse(nsIInputStream *is)
{
    uint32_t bytesRead;
    char buf[2048];
    do {
        is->Read(buf, sizeof(buf), &bytesRead);
        if (bytesRead > 0)
            fwrite(buf, 1, bytesRead, stdout);
    } while (bytesRead > 0);
    return NS_OK;
}

//
//----------------------------------------------------------------------------
// Startup...
//----------------------------------------------------------------------------
//

void
sighandler(int sig)
{
    LOG(("got signal: %d\n", sig));
    NS_BREAK();
}

void
usage(char **argv)
{
    printf("usage: %s [-sync] <host> <path>\n", argv[0]);
    exit(1);
}

int
main(int argc, char* argv[])
{
    nsresult rv;

    signal(SIGSEGV, sighandler);

    gTestSocketIOLog = PR_NewLogModule("TestSocketIO");

    if (argc < 3)
        usage(argv);

    int i=0;
    bool sync = false;
    if (nsCRT::strcasecmp(argv[1], "-sync") == 0) {
        if (argc < 4)
            usage(argv);
        sync = true;
        i = 1;
    }

    char *hostName = argv[1+i];
    char *fileName = argv[2+i];
    int port = 80;

    // Create the Event Queue for this thread...
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to create: event queue service!");
        return rv;
    }

    rv = eventQService->CreateMonitoredThreadEventQueue();
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to create: thread event queue!");
        return rv;
    }

    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);

    // Create the Socket transport service...
    nsCOMPtr<nsISocketTransportService> sts = 
             do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to create: socket transport service!");
        return rv;
    }

    char *buffer = PR_smprintf("GET %s HTTP/1.1" CRLF
                               "host: %s" CRLF
                               "user-agent: Mozilla/5.0 (X11; N; Linux 2.2.16-22smp i686; en-US; m18) Gecko/20001220" CRLF
                               "accept: */*" CRLF
                               "accept-language: en" CRLF
                               "accept-encoding: gzip,deflate,compress,identity" CRLF
                               "keep-alive: 300" CRLF
                               "connection: keep-alive" CRLF
                                CRLF,
                                fileName, hostName);
    LOG(("Request [\n%s]\n", buffer));

    // Create the socket transport...
    nsCOMPtr<nsITransport> transport;
    rv = sts->CreateTransport(hostName, port, nullptr, 0, 0, getter_AddRefs(transport));
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to create: socket transport!");
        return rv;
    }

    gElapsedTime = PR_Now();

    if (!sync) {
        nsCOMPtr<nsIRequest> request;
        rv = transport->AsyncWrite(new TestProvider(buffer), nullptr, 0, 0, 0, getter_AddRefs(request));
        if (NS_FAILED(rv)) {
            NS_WARNING("failed calling: AsyncWrite!");
            return rv;
        }

        // Enter the message pump to allow the URL load to proceed.
        while ( gKeepRunning ) {
            PLEvent *gEvent;
            gEventQ->WaitForEvent(&gEvent);
            gEventQ->HandleEvent(gEvent);
        }
    }
    else {
        // synchronous write
        {
            nsCOMPtr<nsIOutputStream> os;
            rv = transport->OpenOutputStream(0, 0, 0, getter_AddRefs(os));
            if (NS_FAILED(rv)) {
                LOG(("OpenOutputStream failed [rv=%x]\n", rv));
                return rv;
            }
            rv = WriteRequest(os, buffer);
            if (NS_FAILED(rv)) {
                LOG(("WriteRequest failed [rv=%x]\n", rv));
                return rv;
            }
        }
        // synchronous read
        {
            nsCOMPtr<nsIInputStream> is;
            rv = transport->OpenInputStream(0, 0, 0, getter_AddRefs(is));
            if (NS_FAILED(rv)) {
                LOG(("OpenInputStream failed [rv=%x]\n", rv));
                return rv;
            }
            rv = ReadResponse(is);
            if (NS_FAILED(rv)) {
                LOG(("ReadResponse failed [rv=%x]\n", rv));
                return rv;
            }
        }
    }

    PRTime endTime; 
    endTime = PR_Now();
    LOG(("Elapsed time: %d\n", (int32_t)(endTime/1000UL - gElapsedTime/1000UL)));

    sts->Shutdown();
    return 0;
}

