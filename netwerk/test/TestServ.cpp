/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include <stdlib.h>
#include "nsIServiceManager.h"
#include "nsIServerSocket.h"
#include "nsISocketTransport.h"
#include "nsNetUtil.h"
#include "nsStringAPI.h"
#include "nsCOMPtr.h"
#include "mozilla/Logging.h"

//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nullptr;
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

class MySocketListener : public nsIServerSocketListener
{
protected:
    virtual ~MySocketListener() {}

public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISERVERSOCKETLISTENER

    MySocketListener() {}
};

NS_IMPL_ISUPPORTS(MySocketListener, nsIServerSocketListener)

NS_IMETHODIMP
MySocketListener::OnSocketAccepted(nsIServerSocket *serv,
                                   nsISocketTransport *trans)
{
    LOG(("MySocketListener::OnSocketAccepted [serv=%p trans=%p]\n", serv, trans));

    nsAutoCString host;
    int32_t port;

    trans->GetHost(host);
    trans->GetPort(&port);

    LOG(("  -> %s:%d\n", host.get(), port));

    nsCOMPtr<nsIInputStream> input;
    nsCOMPtr<nsIOutputStream> output;
    nsresult rv;

    rv = trans->OpenInputStream(nsITransport::OPEN_BLOCKING, 0, 0, getter_AddRefs(input));
    if (NS_FAILED(rv))
        return rv;

    rv = trans->OpenOutputStream(nsITransport::OPEN_BLOCKING, 0, 0, getter_AddRefs(output));
    if (NS_FAILED(rv))
        return rv;

    char buf[256];
    uint32_t n;

    rv = input->Read(buf, sizeof(buf), &n);
    if (NS_FAILED(rv))
        return rv;

    const char response[] = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nFooooopy!!\r\n";
    rv = output->Write(response, sizeof(response) - 1, &n);
    if (NS_FAILED(rv))
        return rv;

    input->Close();
    output->Close();
    return NS_OK;
}

NS_IMETHODIMP
MySocketListener::OnStopListening(nsIServerSocket *serv, nsresult status)
{
    LOG(("MySocketListener::OnStopListening [serv=%p status=%x]\n", serv, status));
    QuitPumpingEvents();
    return NS_OK;
}

static nsresult
MakeServer(int32_t port)
{
    nsresult rv;
    nsCOMPtr<nsIServerSocket> serv = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = serv->Init(port, true, 5);
    if (NS_FAILED(rv))
        return rv;

    rv = serv->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;
    LOG(("  listening on port %d\n", port));

    rv = serv->AsyncListen(new MySocketListener());
    return rv;
}

int
main(int argc, char* argv[])
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv= (nsresult)-1;
    if (argc < 2) {
        printf("usage: %s <port>\n", argv[0]);
        return -1;
    }

    gTestLog = PR_NewLogModule("Test");

    /* 
     * The following code only deals with XPCOM registration stuff. and setting
     * up the event queues. Copied from TestSocketIO.cpp
     */

    rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
    if (NS_FAILED(rv)) return -1;

    {
        rv = MakeServer(atoi(argv[1]));
        if (NS_FAILED(rv)) {
            LOG(("MakeServer failed [rv=%x]\n", rv));
            return -1;
        }

        // Enter the message pump to allow the URL load to proceed.
        PumpEvents();
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nullptr);
    return 0;
}
