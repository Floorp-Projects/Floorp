#include "TestCommon.h"
#include <stdio.h>
#include "nsCRT.h" /* should be "plstr.h"? */
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsISupportsArray.h"
#include "nsContentUtils.h"
#include <algorithm>

namespace TestPerf {

static nsIIOService *gIOService = nullptr;

//-----------------------------------------------------------------------------

static bool
load_sync_1(nsISupports *element, void *data)
{
    nsCOMPtr<nsIInputStream> stream;
    nsCOMPtr<nsIURI> uri( do_QueryInterface(element) );
    nsAutoCString spec;
    nsresult rv;

    rv = NS_OpenURI(getter_AddRefs(stream),
                    uri,
                    nsContentUtils::GetSystemPrincipal(),
                    nsILoadInfo::SEC_NORMAL,
                    nsIContentPolicy::TYPE_OTHER,
                    nullptr, // aLoadGroup
                    nullptr, // aCallbacks
                    LOAD_NORMAL,
                    gIOService);

    if (NS_FAILED(rv)) {
        uri->GetAsciiSpec(spec);
        fprintf(stderr, "*** failed opening %s [rv=%x]\n", spec.get(), rv);
        return true;
    }

    char buf[4096];
    uint32_t bytesRead;

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

    return true;
}

static nsresult
load_sync(nsISupportsArray *urls)
{
    urls->EnumerateForwards(load_sync_1, nullptr);
    return NS_OK;
}

//-----------------------------------------------------------------------------

static int gRequestCount = 0;

class MyListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    MyListener() { }
    virtual ~MyListener() {}
};

NS_IMPL_ISUPPORTS(MyListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
MyListener::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                            nsIInputStream *stream,
                            uint64_t offset, uint32_t count)
{
    nsresult rv;
    char buf[4096];
    uint32_t n, bytesRead;
    while (count) {
        n = std::min<uint32_t>(count, sizeof(buf));
        rv = stream->Read(buf, n, &bytesRead);
        if (NS_FAILED(rv))
            break;
        count -= bytesRead;
        if (bytesRead == 0)
            break;
    }
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    if (NS_FAILED(status)) {
        nsAutoCString spec;
        req->GetName(spec);
        fprintf(stderr, "*** failed loading %s [reason=%x]\n", spec.get(), status);
    }
    if (--gRequestCount == 0) {
        // post shutdown event
        QuitPumpingEvents();
    }
    return NS_OK;
}

static bool
load_async_1(nsISupports *element, void *data)
{
    nsCOMPtr<nsIURI> uri( do_QueryInterface(element) );
    if (!uri)
        return true;

    MyListener *listener = new MyListener();
    if (!listener)
        return true;
    NS_ADDREF(listener);

    nsresult rv = NS_OpenURI(listener,
                             nullptr,   // aContext
                             uri,
                             nsContentUtils::GetSystemPrincipal(),
                             nsILoadInfo::SEC_NORMAL,
                             nsIContentPolicy::TYPE_OTHER,
                             nullptr,   // aLoadGroup
                             nullptr,   // aCallbacks
                             gIOService);

    NS_RELEASE(listener);
    if (NS_SUCCEEDED(rv))
        gRequestCount++;
    else 
        printf(">> NS_OpenURI failed [rv=%x]\n", rv);
    return true;
}

static nsresult
load_async(nsISupportsArray *urls)
{
    urls->EnumerateForwards(load_async_1, nullptr);

    PumpEvents();
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
        rv = NS_NewURI(getter_AddRefs(uri), buf, nullptr, gIOService); 
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

} // namespace

using namespace TestPerf;

int
main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv;
    bool sync;

    if (argc < 3) {
        print_usage();
        return -1;
    }

    if (PL_strcasecmp(argv[1], "-sync") == 0)
        sync = true;
    else if (PL_strcasecmp(argv[1], "-async") == 0)
        sync = false;
    else {
        print_usage();
        return -1;
    }

    nsCOMPtr<nsIServiceManager> servMan;
    NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    registrar->AutoRegister(nullptr);

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

    uint32_t urlCount;
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
