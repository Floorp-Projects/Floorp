/* vim:set ts=4 sw=4 et cindent: */
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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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
#include <stdlib.h>
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIServerSocket.h"
#include "nsISocketTransport.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "prlog.h"

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nsnull;
#endif
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

static PRBool gKeepRunning = PR_TRUE;
static nsIEventQueue* gEventQ = nsnull;

class MySocketListener : public nsIServerSocketListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISERVERSOCKETLISTENER

    MySocketListener() {}
    virtual ~MySocketListener() {}
};

NS_IMPL_THREADSAFE_ISUPPORTS1(MySocketListener, nsIServerSocketListener)

NS_IMETHODIMP
MySocketListener::OnSocketAccepted(nsIServerSocket *serv,
                                   nsISocketTransport *trans)
{
    LOG(("MySocketListener::OnSocketAccepted [serv=%p trans=%p]\n", serv, trans));

    nsCAutoString host;
    PRInt32 port;

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
    PRUint32 n;

    rv = input->Read(buf, sizeof(buf), &n);
    if (NS_FAILED(rv))
        return rv;

    const char response[] = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nFooooopy!!\r\n";
    rv = output->Write(response, sizeof(response), &n);
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
    gKeepRunning = PR_FALSE;
    return NS_OK;
}

static nsresult
MakeServer(PRInt32 port)
{
    nsresult rv;
    nsCOMPtr<nsIServerSocket> serv = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = serv->Init(port, PR_TRUE, 5);
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

#if defined(PR_LOGGING)
    gTestLog = PR_NewLogModule("Test");
#endif

    /* 
     * The following code only deals with XPCOM registration stuff. and setting
     * up the event queues. Copied from TestSocketIO.cpp
     */

    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;

    {
        // Create the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> eqs =
                 do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;

        eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);

        rv = MakeServer(atoi(argv[1]));
        if (NS_FAILED(rv)) {
            LOG(("MakeServer failed [rv=%x]\n", rv));
            return -1;
        }

        // Enter the message pump to allow the URL load to proceed.
        while (gKeepRunning) {
            PLEvent *event;
            gEventQ->WaitForEvent(&event);
            gEventQ->HandleEvent(event);
        }
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nsnull);
    return rv;
}
