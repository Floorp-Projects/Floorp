#include "TestCommon.h"
#include <stdio.h>
#include "nsCRT.h" /* should be "plstr.h"? */
#include "nsNetUtil.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsISupportsArray.h"

static nsIIOService *gIOService = nsnull;

//-----------------------------------------------------------------------------

static PRBool
load_sync_1(nsISupports *element, void *data)
{
    nsCOMPtr<nsIInputStream> stream;
    nsCOMPtr<nsIURI> uri( do_QueryInterface(element) );
    nsCAutoString spec;
    nsresult rv;

    rv = NS_OpenURI(getter_AddRefs(stream), uri, gIOService);
    if (NS_FAILED(rv)) {
        uri->GetAsciiSpec(spec);
        fprintf(stderr, "*** failed opening %s [rv=%x]\n", spec.get(), rv);
        return PR_TRUE;
    }

    char buf[4096];
    PRUint32 bytesRead;

    while (1) {
        rv = stream->Read(buf, sizeof(buf), &bytesRead);
        if (NS_FAILED(rv) || bytesRead == 0) {
            if (NS_FAILED(rv)) {
                uri->GetAsciiSpec(spec);
                fprintf(stderr, "*** failed reading %s [rv=%x]\n", spec.get(), rv);
            }
            break;
        }
    }

    return PR_TRUE;
}

static nsresult
load_sync(nsISupportsArray *urls)
{
    urls->EnumerateForwards(load_sync_1, nsnull);
    return NS_OK;
}

//-----------------------------------------------------------------------------

static int gRequestCount = 0;
static nsIEventQueue *gEventQ = 0;


class MyListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    MyListener() { }
    virtual ~MyListener() {}
};

NS_IMPL_ISUPPORTS2(MyListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
MyListener::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                            nsIInputStream *stream,
                            PRUint32 offset, PRUint32 count)
{
    nsresult rv;
    char buf[4096];
    PRUint32 n, bytesRead;
    while (count) {
        n = PR_MIN(count, sizeof(buf));
        rv = stream->Read(buf, n, &bytesRead);
        if (NS_FAILED(rv))
            break;
        count -= bytesRead;
        if (bytesRead == 0)
            break;
    }
    return NS_OK;
}

static void *PR_CALLBACK
ShutdownEvent_Handler(PLEvent *ev)
{
    return nsnull;
}

static void PR_CALLBACK
ShutdownEvent_Cleanup(PLEvent *ev)
{
    delete ev;
}

NS_IMETHODIMP
MyListener::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    if (NS_FAILED(status)) {
        nsCAutoString spec;
        req->GetName(spec);
        fprintf(stderr, "*** failed loading %s [reason=%x]\n", spec.get(), status);
    }
    if (--gRequestCount == 0) {
        // post shutdown event
        PLEvent *ev = new PLEvent;
        PL_InitEvent(ev, nsnull,
                ShutdownEvent_Handler,
                ShutdownEvent_Cleanup);
        gEventQ->PostEvent(ev);
    }
    return NS_OK;
}

static PRBool
load_async_1(nsISupports *element, void *data)
{
    nsCOMPtr<nsIURI> uri( do_QueryInterface(element) );
    if (!uri)
        return PR_TRUE;

    MyListener *listener = new MyListener();
    if (!listener)
        return PR_TRUE;
    NS_ADDREF(listener);
    nsresult rv = NS_OpenURI(listener, nsnull, uri, gIOService);
    NS_RELEASE(listener);
    if (NS_SUCCEEDED(rv))
        gRequestCount++;
    else 
        printf(">> NS_OpenURI failed [rv=%x]\n", rv);
    return PR_TRUE;
}

static nsresult
load_async(nsISupportsArray *urls)
{
    nsresult rv;

    // Create the Event Queue for this thread...
    nsCOMPtr<nsIEventQueueService> eqs(
            do_GetService("@mozilla.org/event-queue-service;1", &rv) );
    if (NS_FAILED(rv)) return rv;

    rv = eqs->CreateMonitoredThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED(rv)) return rv;

    urls->EnumerateForwards(load_async_1, nsnull);

    while (1) {
        PLEvent *ev;
        gEventQ->WaitForEvent(&ev);
        if (gRequestCount == 0)
            break;
        gEventQ->HandleEvent(ev);
    }
    NS_RELEASE(gEventQ);
    return NS_OK;
}

//-----------------------------------------------------------------------------

static nsresult
read_file(const char *fname, nsISupportsArray *urls)
{
    FILE *fp = fopen(fname, "r");
    if (!fp) {
        printf("failed opening file: %s\n", fname);
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv;
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
        // remove trailing newline
        buf[strlen(buf) - 1] = 0;
        rv = NS_NewURI(getter_AddRefs(uri), buf, nsnull, gIOService); 
        if (NS_FAILED(rv))
            printf("*** ignoring malformed uri: %s\n", buf);
        else {
            //nsXPIDLCString spec;
            //uri->GetSpec(getter_Copies(spec));
            //printf("read url: %s\n", spec.get());
            urls->AppendElement(uri);
        }
    }

    fclose(fp);
    return NS_OK;
}

//-----------------------------------------------------------------------------

static void
print_usage()
{
    printf("usage: TestPerf [-sync|-async] <file-of-urls>\n");
}

int
main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv;
    PRBool sync;

    if (argc < 3) {
        print_usage();
        return -1;
    }

    if (PL_strcasecmp(argv[1], "-sync") == 0)
        sync = PR_TRUE;
    else if (PL_strcasecmp(argv[1], "-async") == 0)
        sync = PR_FALSE;
    else {
        print_usage();
        return -1;
    }

    nsCOMPtr<nsIServiceManager> servMan;
    NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    registrar->AutoRegister(nsnull);

    // cache the io service
    {
        nsCOMPtr<nsIIOService> ioserv( do_GetIOService() );
        NS_ADDREF(gIOService = ioserv);
    }

    nsCOMPtr<nsISupportsArray> urls;
    rv = NS_NewISupportsArray(getter_AddRefs(urls));
    if (NS_FAILED(rv)) return -1;

    rv = read_file(argv[2], urls);
    if (NS_FAILED(rv)) {
        printf("failed reading file-of-urls\n");
        return -1;
    }

    PRUint32 urlCount;
    urls->Count(&urlCount);

    PRIntervalTime start = PR_IntervalNow();

    if (sync)
        rv = load_sync(urls);
    else
        rv = load_async(urls);

    if (NS_FAILED(rv)) {
        printf("load failed\n");
        return -1;
    }

    PRIntervalTime end = PR_IntervalNow();
    fprintf(stderr, "read: %u urls; total time: %u milliseconds\n",
            urlCount,
            PR_IntervalToMilliseconds(end - start));

    NS_RELEASE(gIOService);
    return 0;
}
