/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <stdio.h>
#include <signal.h>

#ifdef WIN32
#include <windows.h>
#endif
#ifdef OS2
#include <os2.h>
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

#if defined(PR_LOGGING)
static PRLogModuleInfo *gTestSocketIOLog;
#define LOG(args) PR_LOG(gTestSocketIOLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

static PRTime gElapsedTime;
static int gKeepRunning = 1;
static nsIEventQueue* gEventQ = nsnull;

//
//----------------------------------------------------------------------------
// Test Listener
//----------------------------------------------------------------------------
//

class TestListener : public nsIStreamListener
{
public:
    TestListener() { NS_INIT_ISUPPORTS(); }
    virtual ~TestListener() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
};

NS_IMPL_ISUPPORTS2(TestListener,
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
                              PRUint32 aSourceOffset,
                              PRUint32 aLength)
{
    LOG(("TestListener::OnDataAvailable [offset=%u length=%u]\n",
        aSourceOffset, aLength));
    char buf[1025];
    PRUint32 amt;
    while (1) {
        aIStream->Read(buf, 1024, &amt);
        if (amt == 0)
            break;
        buf[amt] = '\0';
        //puts(buf);
        printf("read %d bytes\n", amt);
    }
    return NS_OK;
}

NS_IMETHODIMP
TestListener::OnStopRequest(nsIRequest* request, nsISupports* context,
                            nsresult aStatus)
{
    LOG(("TestListener::OnStopRequest [aStatus=%x]\n", aStatus));
    //gKeepRunning = 0;
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
    char    *mData;
    PRUint32 mOffset;
    PRUint32 mDataLen;   
    PRUint32 mRequestCount;
};

NS_IMPL_ISUPPORTS2(TestProvider,
                   nsIStreamProvider,
                   nsIRequestObserver)

TestProvider::TestProvider(char *data)
{
    NS_INIT_ISUPPORTS();
    mData = data;
    mDataLen = strlen(data);
    mOffset = 0;
    mRequestCount = 0;
    LOG(("Constructing TestProvider [this=%x]\n", this));
}

TestProvider::~TestProvider()
{
    LOG(("Destroying TestProvider [this=%x]\n", this));
}

NS_IMETHODIMP
TestProvider::OnStartRequest(nsIRequest* request, nsISupports* context)
{
    LOG(("TestProvider::OnStartRequest [this=%x]\n", this));
    return NS_OK;
}

NS_IMETHODIMP
TestProvider::OnStopRequest(nsIRequest* request, nsISupports* context,
                            nsresult aStatus)
{
    LOG(("TestProvider::OnStopRequest [status=%x]\n", aStatus));
    return NS_OK;
}

NS_IMETHODIMP
TestProvider::OnDataWritable(nsIRequest *request, nsISupports *context,
                             nsIOutputStream *output, PRUint32 offset, PRUint32 count)
{
    LOG(("TestProvider::OnDataWritable [offset=%u, count=%u]\n", offset, count));

    // Stop at 5 requests
    if (mRequestCount == 5)
        return NS_BASE_STREAM_CLOSED;

    PRUint32 writeCount, amount;
    amount = PR_MIN(count, mDataLen - mOffset);
    nsresult rv = output->Write(mData + mOffset, amount, &writeCount);
    if (NS_SUCCEEDED(rv)) {
        printf("wrote %u bytes\n", writeCount);
        mOffset += writeCount;
        if (mOffset == mDataLen) {
            printf("done sending packet %u\n", mRequestCount);
            mOffset = 0;
            mRequestCount++;
        }
    }
    return NS_OK;
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
    PRUint32 n;
    return os->Write(request, strlen(request), &n);
}

nsresult
ReadResponse(nsIInputStream *is)
{
    PRUint32 bytesRead;
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
    printf("usage: %s <host> <path>\n", argv[0]);
    exit(1);
}

int
main(int argc, char* argv[])
{
    nsresult rv;

#ifdef XP_OS2_VACPP
    signal(SIGSEGV, (_SigFunc)sighandler);
#else
    signal(SIGSEGV, sighandler);
#endif

#if defined(PR_LOGGING)
    gTestSocketIOLog = PR_NewLogModule("TestSocketIO");
#endif

    if (argc < 3)
        usage(argv);

    char *hostName = argv[1];
    char *fileName = argv[2];
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
    rv = sts->CreateTransport(hostName, port, nsnull, -1, 0, 0, getter_AddRefs(transport));
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to create: socket transport!");
        return rv;
    }

    gElapsedTime = PR_Now();

    nsCOMPtr<nsIRequest> writeRequest, readRequest;

    rv = transport->AsyncWrite(new TestProvider(buffer), nsnull, 0, 0, 0, getter_AddRefs(writeRequest));
    if (NS_FAILED(rv)) {
        NS_WARNING("failed calling: AsyncWrite!");
        return rv;
    }
    rv = transport->AsyncRead(new TestListener(), nsnull, 0, 0, 0, getter_AddRefs(readRequest));
    if (NS_FAILED(rv)) {
        NS_WARNING("failed calling: AsyncWrite!");
        return rv;
    }

    // Enter the message pump
    while ( gKeepRunning ) {
#ifdef WIN32
        MSG msg;
        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
            gKeepRunning = FALSE;
#elif XP_MAC
        /* Mac stuff is missing here! */
#elif XP_OS2
        QMSG qmsg;
        if (WinGetMsg(0, &qmsg, 0, 0, 0))
            WinDispatchMsg(0, &qmsg);
        else
            gKeepRunning = FALSE;
#else
        PLEvent *gEvent;
        rv = gEventQ->WaitForEvent(&gEvent);
        rv = gEventQ->HandleEvent(gEvent);
#endif
    }

    PRTime endTime; 
    endTime = PR_Now();
    LOG(("Elapsed time: %d\n", (PRInt32)(endTime/1000UL - gElapsedTime/1000UL)));

    sts->Shutdown();
    return 0;
}
