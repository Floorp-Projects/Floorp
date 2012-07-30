#include "nsNetUtil.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"

#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(">>> %s failed: rv=%x\n", step, rv); \
        return rv;\
    } \
    PR_END_MACRO

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static nsIEventQueue* gEventQ = nullptr;
static bool gKeepRunning = true;

//-----------------------------------------------------------------------------
// nsIStreamListener implementation
//-----------------------------------------------------------------------------

class MyListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    MyListener() { }
    virtual ~MyListener() {}
};

NS_IMPL_ISUPPORTS2(MyListener,
                   nsIRequestObserver,
                   nsIStreamListener)

NS_IMETHODIMP
MyListener::OnStartRequest(nsIRequest *req, nsISupports *ctxt)
{
    printf(">>> OnStartRequest\n");
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnStopRequest(nsIRequest *req, nsISupports *ctxt, nsresult status)
{
    printf(">>> OnStopRequest status=%x\n", status);
    gKeepRunning = false;
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnDataAvailable(nsIRequest *req, nsISupports *ctxt,
                            nsIInputStream *stream,
                            PRUint32 offset, PRUint32 count)
{
    printf(">>> OnDataAvailable [count=%u]\n", count);

    char buf[256];
    nsresult rv;
    PRUint32 bytesRead=0;

    while (count) {
        PRUint32 amount = NS_MIN<PRUint32>(count, sizeof(buf));

        rv = stream->Read(buf, amount, &bytesRead);
        if (NS_FAILED(rv)) {
            printf(">>> stream->Read failed with rv=%x\n", rv);
            return rv;
        }

        fwrite(buf, 1, bytesRead, stdout);

        count -= bytesRead;
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// NotificationCallbacks implementation
//-----------------------------------------------------------------------------

class MyNotifications : public nsIInterfaceRequestor
                      , public nsIProgressEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK

    MyNotifications() { }
    virtual ~MyNotifications() {}
};

NS_IMPL_THREADSAFE_ISUPPORTS2(MyNotifications,
                              nsIInterfaceRequestor,
                              nsIProgressEventSink)

NS_IMETHODIMP
MyNotifications::GetInterface(const nsIID &iid, void **result)
{
    return QueryInterface(iid, result);
}

NS_IMETHODIMP
MyNotifications::OnStatus(nsIRequest *req, nsISupports *ctx,
                          nsresult status, const PRUnichar *statusText)
{
    printf("status: %x\n", status);
    return NS_OK;
}

NS_IMETHODIMP
MyNotifications::OnProgress(nsIRequest *req, nsISupports *ctx,
                            PRUint64 progress, PRUint64 progressMax)
{
    printf("progress: %llu/%llu\n", progress, progressMax);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// main, etc..
//-----------------------------------------------------------------------------


int main(int argc, char **argv)
{
    nsresult rv;

    if (argc == 1) {
        printf("usage: TestHttp <url>\n");
        return -1;
    }
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

        // Create the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> eqs =
                 do_GetService(kEventQueueServiceCID, &rv);
        RETURN_IF_FAILED(rv, "do_GetService(EventQueueService)");

        rv = eqs->CreateMonitoredThreadEventQueue();
        RETURN_IF_FAILED(rv, "CreateMonitoredThreadEventQueue");

        rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
        RETURN_IF_FAILED(rv, "GetThreadEventQueue");

        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> chan;
        nsCOMPtr<nsIStreamListener> listener = new MyListener();
        nsCOMPtr<nsIInterfaceRequestor> callbacks = new MyNotifications();

        rv = NS_NewURI(getter_AddRefs(uri), argv[1]);
        RETURN_IF_FAILED(rv, "NS_NewURI");

        rv = NS_NewChannel(getter_AddRefs(chan), uri, nullptr, nullptr, callbacks);
        RETURN_IF_FAILED(rv, "NS_OpenURI");

        rv = chan->AsyncOpen(listener, nullptr);
        RETURN_IF_FAILED(rv, "AsyncOpen");

        while (gKeepRunning)
            gEventQ->ProcessPendingEvents();

        printf(">>> done\n");
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}
