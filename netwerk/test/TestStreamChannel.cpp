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
#include "nsIStreamTransportService.h"
#include "nsIAsyncInputStream.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProxyObjectManager.h"
#include "nsIRequest.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsAutoLock.h"
#include "prlog.h"

////////////////////////////////////////////////////////////////////////////////

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nsnull;
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

PRBool gDone = PR_FALSE;
nsIEventQueue *gEventQ = nsnull;

////////////////////////////////////////////////////////////////////////////////

static void *PR_CALLBACK
DoneEvent_Handler(PLEvent *ev)
{
    gDone = PR_TRUE;
    return nsnull;
}

static void PR_CALLBACK
DoneEvent_Cleanup(PLEvent *ev)
{
    delete ev;
}

static void
PostDoneEvent()
{
    LOG(("PostDoneEvent\n"));

    PLEvent *ev = new PLEvent();

    PL_InitEvent(ev, nsnull, 
            DoneEvent_Handler,
            DoneEvent_Cleanup);

    gEventQ->PostEvent(ev);
}

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
                               PRUint32 offset, PRUint32 count)
    {
        LOG(("MyListener::OnDataAvailable [offset=%u count=%u]\n", offset, count));

        char buf[500];
        nsresult rv;

        while (count) {
            PRUint32 n, amt = PR_MIN(count, sizeof(buf));

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
        PostDoneEvent();
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS2(MyListener,
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
                        const PRUnichar *statusArg)
    {
        LOG(("MyCallbacks::OnStatus [status=%x]\n", status));
        return NS_OK;
    }

    NS_IMETHOD OnProgress(nsIRequest *req, nsISupports *ctx,
                          PRUint32 progress, PRUint32 progressMax)
    {
        LOG(("MyCallbacks::OnProgress [progress=%u/%u]\n", progress, progressMax));
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS2(MyCallbacks,
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

    const nsAFlatCString& empty = EmptyCString();

    nsCOMPtr<nsIChannel> chan;
    rv = NS_NewInputStreamChannel(getter_AddRefs(chan), uri, stream, empty,
				  empty);
    if (NS_FAILED(rv)) return rv;

    rv = chan->SetNotificationCallbacks(new MyCallbacks());
    if (NS_FAILED(rv)) return rv;

    rv = chan->AsyncOpen(new MyListener(), nsnull);
    if (NS_FAILED(rv)) return rv;

    gDone = PR_FALSE;
    while (!gDone) {
        PLEvent *event;
        rv = gEventQ->WaitForEvent(&event);
        if (NS_FAILED(rv)) return rv;
        rv = gEventQ->HandleEvent(event);
        if (NS_FAILED(rv)) return rv;
    }

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
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nsnull);

#if defined(PR_LOGGING)
        gTestLog = PR_NewLogModule("Test");
#endif

        nsCOMPtr<nsIEventQueueService> eventQService =
                 do_GetService(kEventQueueServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewNativeLocalFile(nsDependentCString(fileName), PR_FALSE, getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;

        rv = RunTest(file);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RunTest failed");

        NS_RELEASE(gEventQ);

        // give background threads a chance to finish whatever work they may
        // be doing.
        PR_Sleep(PR_SecondsToInterval(1));
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return NS_OK;
}
