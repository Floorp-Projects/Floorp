#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h" // For MOZ_GTEST_BENCH

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsComponentManagerUtils.h"

TEST(TestStandardURL, Simple) {
    nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
    ASSERT_TRUE(url);
    ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://example.com")), NS_OK);

    nsAutoCString out;

    ASSERT_EQ(url->GetSpec(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://example.com/"));

    ASSERT_EQ(url->Resolve(NS_LITERAL_CSTRING("foo.html?q=45"), out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://example.com/foo.html?q=45"));

    ASSERT_EQ(url->SetScheme(NS_LITERAL_CSTRING("foo")), NS_OK);

    ASSERT_EQ(url->GetScheme(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("foo"));

    ASSERT_EQ(url->GetHost(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("example.com"));
    ASSERT_EQ(url->SetHost(NS_LITERAL_CSTRING("www.yahoo.com")), NS_OK);
    ASSERT_EQ(url->GetHost(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("www.yahoo.com"));

    ASSERT_EQ(url->SetPath(NS_LITERAL_CSTRING("/some-path/one-the-net/about.html?with-a-query#for-you")), NS_OK);
    ASSERT_EQ(url->GetPath(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("/some-path/one-the-net/about.html?with-a-query#for-you"));

    ASSERT_EQ(url->SetQuery(NS_LITERAL_CSTRING("a=b&d=c&what-ever-you-want-to-be-called=45")), NS_OK);
    ASSERT_EQ(url->GetQuery(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("a=b&d=c&what-ever-you-want-to-be-called=45"));

    ASSERT_EQ(url->SetRef(NS_LITERAL_CSTRING("#some-book-mark")), NS_OK);
    ASSERT_EQ(url->GetRef(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("some-book-mark"));
}

#define COUNT 10000

MOZ_GTEST_BENCH(TestStandardURL, Perf, [] {
    nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
    ASSERT_TRUE(url);
    nsAutoCString out;

    for (int i = COUNT; i; --i) {
        ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://example.com")), NS_OK);
        ASSERT_EQ(url->GetSpec(out), NS_OK);
        url->Resolve(NS_LITERAL_CSTRING("foo.html?q=45"), out);
        url->SetScheme(NS_LITERAL_CSTRING("foo"));
        url->GetScheme(out);
        url->SetHost(NS_LITERAL_CSTRING("www.yahoo.com"));
        url->GetHost(out);
        url->SetPath(NS_LITERAL_CSTRING("/some-path/one-the-net/about.html?with-a-query#for-you"));
        url->GetPath(out);
        url->SetQuery(NS_LITERAL_CSTRING("a=b&d=c&what-ever-you-want-to-be-called=45"));
        url->GetQuery(out);
        url->SetRef(NS_LITERAL_CSTRING("#some-book-mark"));
        url->GetRef(out);
    }
});

TEST(TestStandardURL, IPv4) {
    nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
    ASSERT_TRUE(url);
    ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://other.com/")), NS_OK);

    nsAutoCString out;

    ASSERT_EQ(url->GetSpec(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://other.com/"));

    ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://%30%78%63%30%2e%30%32%35%30.01%2e")), NS_OK);
    ASSERT_EQ(url->GetSpec(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://192.168.0.1/"));

    ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://0x7f000001")), NS_OK);
    ASSERT_EQ(url->GetSpec(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://127.0.0.1/"));

    ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://00000000000000000000000000177.0000000.0000000.0001")), NS_OK);
    ASSERT_EQ(url->GetSpec(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://127.0.0.1/"));

    ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://0x7f.0x00.0x00.0x01")), NS_OK);
    ASSERT_EQ(url->GetSpec(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://127.0.0.1/"));
}
