#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h" // For MOZ_GTEST_BENCH

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsComponentManagerUtils.h"


// In nsStandardURL.cpp
extern nsresult Test_NormalizeIPv4(const nsCSubstring& host, nsCString& result);


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

TEST(TestStandardURL, NormalizeGood)
{
    nsCString result;
    const char* manual[] = {"0.0.0.0", "0.0.0.0",
                            "0", "0.0.0.0",
                            "000", "0.0.0.0",
                            "0x00", "0.0.0.0",
                            "10.20.100.200", "10.20.100.200",
                            "255.255.255.255", "255.255.255.255",
                            "0XFF.0xFF.0xff.0xFf", "255.255.255.255",
                            "0x000ff.0X00FF.0x0ff.0xff", "255.255.255.255",
                            "0x000fA.0X00FB.0x0fC.0xfD", "250.251.252.253",
                            "0x000fE.0X00FF.0x0fC.0xfD", "254.255.252.253",
                            "0x000fa.0x00fb.0x0fc.0xfd", "250.251.252.253",
                            "0x000fe.0x00ff.0x0fc.0xfd", "254.255.252.253",
                            "0377.0377.0377.0377", "255.255.255.255",
                            "0000377.000377.00377.0377", "255.255.255.255",
                            "65535", "0.0.255.255",
                            "0xfFFf", "0.0.255.255",
                            "0x00000ffff", "0.0.255.255",
                            "0177777", "0.0.255.255",
                            "000177777", "0.0.255.255",
                            "0.13.65535", "0.13.255.255",
                            "0.22.0xffff", "0.22.255.255",
                            "0.123.0177777", "0.123.255.255",
                            "65536", "0.1.0.0",
                            "0200000", "0.1.0.0",
                            "0x10000", "0.1.0.0"};
    for (uint32_t i = 0; i < sizeof(manual)/sizeof(manual[0]); i += 2) {
        nsCString encHost(manual[i + 0]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
        ASSERT_TRUE(result.Equals(manual[i + 1]));
    }

    // Make sure we're getting the numbers correctly interpreted:
    for (int i = 0; i < 256; i++) {
        nsCString encHost = nsPrintfCString("0x%x", i);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
        ASSERT_TRUE(result.Equals(nsPrintfCString("0.0.0.%d", i)));

        encHost = nsPrintfCString("0%o", i);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
        ASSERT_TRUE(result.Equals(nsPrintfCString("0.0.0.%d", i)));
    }

    // Some random numbers in the range, mixing hex, decimal, octal
    for (int i = 0; i < 8; i++) {
        int val[4] = {i * 11 + 13, i * 18 + 22, i * 4 + 28, i * 15 + 2};

        nsCString encHost = nsPrintfCString("%d.%d.%d.%d", val[0], val[1], val[2], val[3]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
        ASSERT_TRUE(result.Equals(encHost));

        nsCString encHostM = nsPrintfCString("0x%x.0x%x.0x%x.0x%x", val[0], val[1], val[2], val[3]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
        ASSERT_TRUE(result.Equals(encHost));

        encHostM = nsPrintfCString("0%o.0%o.0%o.0%o", val[0], val[1], val[2], val[3]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
        ASSERT_TRUE(result.Equals(encHost));

        encHostM = nsPrintfCString("0x%x.%d.0%o.%d", val[0], val[1], val[2], val[3]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
        ASSERT_TRUE(result.Equals(encHost));

        encHostM = nsPrintfCString("%d.0%o.0%o.0x%x", val[0], val[1], val[2], val[3]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
        ASSERT_TRUE(result.Equals(encHost));

        encHostM = nsPrintfCString("0%o.0%o.0x%x.0x%x", val[0], val[1], val[2], val[3]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
        ASSERT_TRUE(result.Equals(encHost));
    }
}

TEST(TestStandardURL, NormalizeBad)
{
  nsAutoCString result;
  const char* manual[] = { "x22.232.12.32", "122..12.32", "122.12.32.12.32", "122.12.32..",
                           "122.12.xx.22", "122.12.0xx.22", "0xx.12.01.22", "0x.12.01.22",
                           "12.12.02x.22", "1q.12.2.22", "122.01f.02.22", "12a.01.02.22",
                           "12.01.02.20x1", "10x2.01.02.20", "0xx.01.02.20", "10.x.02.20",
                           "10.00x2.02.20", "10.13.02x2.20", "10.x13.02.20", "10.0x134def.02.20",
                           "\0.2.2.2", "256.2.2.2", "2.256.2.2", "2.2.256.2",
                           "2.2.2.256", "2.2.-2.3", "+2.2.2.3", "13.0x2x2.2.3",
                           "0x2x2.13.2.3"};

  for (uint32_t i = 0; i < sizeof(manual)/sizeof(manual[0]); i ++) {
    nsCString encHost(manual[i]);
    ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost, result));
  }
}

TEST(TestStandardURL, From_test_standardurldotjs)
{
    // These are test (success and failure) cases from test_standardurl.js
    nsAutoCString result;

    const char* localIPv4s[] = {"127.0.0.1", "127.0.1", "127.1", "2130706433",
                                "0177.00.00.01", "0177.00.01", "0177.01",
                                "00000000000000000000000000177.0000000.0000000.0001",
                                "000000177.0000001", "017700000001",
                                "0x7f.0x00.0x00.0x01", "0x7f.0x01",
                                "0x7f000001", "0x007f.0x0000.0x0000.0x0001",
                                "000177.0.00000.0x0001", "127.0.0.1.",

                                "0X7F.0X00.0X00.0X01", "0X7F.0X01",
                                "0X7F000001", "0X007F.0X0000.0X0000.0X0001",
                                "000177.0.00000.0X0001"};
    for (uint32_t i = 0; i < sizeof(localIPv4s)/sizeof(localIPv4s[0]); i ++) {
        nsCString encHost(localIPv4s[i]);
        ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
        ASSERT_TRUE(result.Equals("127.0.0.1"));
    }

    const char* nonIPv4s[] = {"0xfffffffff", "0x100000000", "4294967296", "1.2.0x10000",
                              "1.0x1000000", "256.0.0.1", "1.256.1", "-1.0.0.0",
                              "1.2.3.4.5", "010000000000000000", "2+3", "0.0.0.-1",
                              "1.2.3.4..", "1..2", ".1.2.3.4"};
    for (uint32_t i = 0; i < sizeof(nonIPv4s)/sizeof(nonIPv4s[0]); i ++) {
        nsCString encHost(nonIPv4s[i]);
        ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost, result));
    }
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

// Note the five calls in the loop, so divide by 100k
MOZ_GTEST_BENCH(TestStandardURL, NormalizePerf, [] {
    nsAutoCString result;
    for (int i = 0; i < 20000; i++) {
      nsAutoCString encHost("123.232.12.32");
      ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
      nsAutoCString encHost2("83.62.12.92");
      ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost2, result));
      nsAutoCString encHost3("8.7.6.5");
      ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost3, result));
      nsAutoCString encHost4("111.159.123.220");
      ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost4, result));
      nsAutoCString encHost5("1.160.204.200");
      ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost5, result));
    }
});

// Note the five calls in the loop, so divide by 100k
MOZ_GTEST_BENCH(TestStandardURL, NormalizePerfFails, [] {
    nsAutoCString result;
    for (int i = 0; i < 20000; i++) {
      nsAutoCString encHost("123.292.12.32");
      ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost, result));
      nsAutoCString encHost2("83.62.12.0x13292");
      ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost2, result));
      nsAutoCString encHost3("8.7.6.0xhello");
      ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost3, result));
      nsAutoCString encHost4("111.159.notonmywatch.220");
      ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost4, result));
      nsAutoCString encHost5("1.160.204.20f");
      ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost5, result));
    }
});
