#include <stdlib.h>
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "prinrval.h"
#include "nsXPIDLString.h"

static nsIURL     *test_url = 0;
static const char *test_param = 0;

static void run_test(const char *testname, int count, void (* testfunc)())
{
    PRIntervalTime start, end;
    start = PR_IntervalNow();
    for (; count; --count)
        testfunc();
    end = PR_IntervalNow();
    printf("completed %s test in %u milliseconds\n", testname,
            PR_IntervalToMilliseconds(end - start));
}

static void set_spec_test()
{
    test_url->SetSpec(test_param);
}

static void get_spec_test()
{
    nsXPIDLCString spec;
    test_url->GetSpec(getter_Copies(spec));
}

static void resolve_test()
{
    nsXPIDLCString spec;
    test_url->Resolve("foo.html?q=45", getter_Copies(spec));
}

static void set_scheme_test()
{
    test_url->SetScheme("foo");
}

static void get_scheme_test()
{
    nsXPIDLCString scheme;
    test_url->GetScheme(getter_Copies(scheme));
}

static void host_test()
{
    nsXPIDLCString host;
    test_url->GetHost(getter_Copies(host));
    test_url->SetHost("www.yahoo.com");
    test_url->SetHost(host);
}

static void set_path_test()
{
    test_url->SetPath("/some-path/one-the-net/about.html?with-a-query#for-you");
}

static void get_path_test()
{
    nsXPIDLCString path;
    test_url->GetPath(getter_Copies(path));
}

static void query_test()
{
    nsXPIDLCString query;
    test_url->GetQuery(getter_Copies(query));
    test_url->SetQuery("a=b&d=c&what-ever-you-want-to-be-called=45");
    test_url->SetQuery(query);
}

static void ref_test()
{
    nsXPIDLCString ref;
    test_url->GetRef(getter_Copies(ref));
    test_url->SetRef("#some-book-mark");
    test_url->SetRef(ref);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: TestURL url [count]\n");
        return -1;
    }

    int count = 1000;
    if (argc == 3)
        count = atoi(argv[2]);
    else
        printf("using a default count of %d\n", count);

    nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
    if (!url) {
        printf("failed to instantiate component: %s\n", NS_STANDARDURL_CONTRACTID);
        return -1;
    }

    test_url = url;
    test_param = argv[1];

    run_test("SetSpec", count, set_spec_test);
    run_test("GetSpec", count, get_spec_test);
    run_test("Resolve", count, resolve_test);
    run_test("SetScheme", count, set_scheme_test);
    run_test("GetScheme", count, get_scheme_test);
    run_test("[GS]etHost", count, host_test);
    run_test("SetPath", count, set_path_test);
    run_test("GetPath", count, get_path_test);
    run_test("[GS]etQuery", count, query_test);
    run_test("[GS]etRef", count, ref_test);

    return 0;
}
