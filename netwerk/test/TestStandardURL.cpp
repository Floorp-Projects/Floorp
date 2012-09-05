#include <stdio.h>
#include <stdlib.h>

#include "TestCommon.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "prinrval.h"
#include "nsStringAPI.h"
#include "nsComponentManagerUtils.h"

static nsIURL     *test_url = 0;
static nsCString   test_param;

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
    nsAutoCString spec;
    test_url->GetSpec(spec);
}

static void resolve_test()
{
    nsAutoCString spec;
    test_url->Resolve(NS_LITERAL_CSTRING("foo.html?q=45"), spec);
}

static void set_scheme_test()
{
    test_url->SetScheme(NS_LITERAL_CSTRING("foo"));
}

static void get_scheme_test()
{
    nsAutoCString scheme;
    test_url->GetScheme(scheme);
}

static void host_test()
{
    nsAutoCString host;
    test_url->GetHost(host);
    test_url->SetHost(NS_LITERAL_CSTRING("www.yahoo.com"));
    test_url->SetHost(host);
}

static void set_path_test()
{
    test_url->SetPath(NS_LITERAL_CSTRING("/some-path/one-the-net/about.html?with-a-query#for-you"));
}

static void get_path_test()
{
    nsAutoCString path;
    test_url->GetPath(path);
}

static void query_test()
{
    nsAutoCString query;
    test_url->GetQuery(query);
    test_url->SetQuery(NS_LITERAL_CSTRING("a=b&d=c&what-ever-you-want-to-be-called=45"));
    test_url->SetQuery(query);
}

static void ref_test()
{
    nsAutoCString ref;
    test_url->GetRef(ref);
    test_url->SetRef(NS_LITERAL_CSTRING("#some-book-mark"));
    test_url->SetRef(ref);
}

int main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

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
