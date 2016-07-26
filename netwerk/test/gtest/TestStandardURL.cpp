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
        url->SetSpec(NS_LITERAL_CSTRING("http://example.com"));
        url->GetSpec(out);
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
