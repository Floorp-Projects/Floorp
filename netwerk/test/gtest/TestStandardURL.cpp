#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsComponentManagerUtils.h"
#include "nsIURIMutator.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Unused.h"
#include "nsSerializationHelper.h"
#include "mozilla/Base64.h"
#include "nsEscape.h"

using namespace mozilla;

// In nsStandardURL.cpp
extern nsresult Test_NormalizeIPv4(const nsACString& host, nsCString& result);
extern nsresult Test_ParseIPv4Number(const nsACString& input, int32_t base,
                                     uint32_t& number, uint32_t maxNumber);
extern int32_t Test_ValidateIPv4Number(const nsACString& host, int32_t bases[4],
                                       int32_t dotIndex[3], bool& onlyBase10,
                                       int32_t length);
TEST(TestStandardURL, Simple)
{
  nsCOMPtr<nsIURI> url;
  ASSERT_EQ(NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                .SetSpec("http://example.com"_ns)
                .Finalize(url),
            NS_OK);
  ASSERT_TRUE(url);

  ASSERT_EQ(NS_MutateURI(url).SetSpec("http://example.com"_ns).Finalize(url),
            NS_OK);

  nsAutoCString out;

  ASSERT_EQ(url->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "http://example.com/"_ns);

  ASSERT_EQ(url->Resolve("foo.html?q=45"_ns, out), NS_OK);
  ASSERT_TRUE(out == "http://example.com/foo.html?q=45"_ns);

  ASSERT_EQ(NS_MutateURI(url).SetScheme("foo"_ns).Finalize(url), NS_OK);

  ASSERT_EQ(url->GetScheme(out), NS_OK);
  ASSERT_TRUE(out == "foo"_ns);

  ASSERT_EQ(url->GetHost(out), NS_OK);
  ASSERT_TRUE(out == "example.com"_ns);
  ASSERT_EQ(NS_MutateURI(url).SetHost("www.yahoo.com"_ns).Finalize(url), NS_OK);
  ASSERT_EQ(url->GetHost(out), NS_OK);
  ASSERT_TRUE(out == "www.yahoo.com"_ns);

  ASSERT_EQ(NS_MutateURI(url)
                .SetPathQueryRef(nsLiteralCString(
                    "/some-path/one-the-net/about.html?with-a-query#for-you"))
                .Finalize(url),
            NS_OK);
  ASSERT_EQ(url->GetPathQueryRef(out), NS_OK);
  ASSERT_TRUE(out ==
              nsLiteralCString(
                  "/some-path/one-the-net/about.html?with-a-query#for-you"));

  ASSERT_EQ(NS_MutateURI(url)
                .SetQuery(nsLiteralCString(
                    "a=b&d=c&what-ever-you-want-to-be-called=45"))
                .Finalize(url),
            NS_OK);
  ASSERT_EQ(url->GetQuery(out), NS_OK);
  ASSERT_TRUE(out == "a=b&d=c&what-ever-you-want-to-be-called=45"_ns);

  ASSERT_EQ(NS_MutateURI(url).SetRef("#some-book-mark"_ns).Finalize(url),
            NS_OK);
  ASSERT_EQ(url->GetRef(out), NS_OK);
  ASSERT_TRUE(out == "some-book-mark"_ns);
}

TEST(TestStandardURL, NormalizeGood)
{
  nsCString result;
  const char* manual[] = {"0.0.0.0",
                          "0.0.0.0",
                          "0",
                          "0.0.0.0",
                          "000",
                          "0.0.0.0",
                          "0x00",
                          "0.0.0.0",
                          "10.20.100.200",
                          "10.20.100.200",
                          "255.255.255.255",
                          "255.255.255.255",
                          "0XFF.0xFF.0xff.0xFf",
                          "255.255.255.255",
                          "0x000ff.0X00FF.0x0ff.0xff",
                          "255.255.255.255",
                          "0x000fA.0X00FB.0x0fC.0xfD",
                          "250.251.252.253",
                          "0x000fE.0X00FF.0x0fC.0xfD",
                          "254.255.252.253",
                          "0x000fa.0x00fb.0x0fc.0xfd",
                          "250.251.252.253",
                          "0x000fe.0x00ff.0x0fc.0xfd",
                          "254.255.252.253",
                          "0377.0377.0377.0377",
                          "255.255.255.255",
                          "0000377.000377.00377.0377",
                          "255.255.255.255",
                          "65535",
                          "0.0.255.255",
                          "0xfFFf",
                          "0.0.255.255",
                          "0x00000ffff",
                          "0.0.255.255",
                          "0177777",
                          "0.0.255.255",
                          "000177777",
                          "0.0.255.255",
                          "0.13.65535",
                          "0.13.255.255",
                          "0.22.0xffff",
                          "0.22.255.255",
                          "0.123.0177777",
                          "0.123.255.255",
                          "65536",
                          "0.1.0.0",
                          "0200000",
                          "0.1.0.0",
                          "0x10000",
                          "0.1.0.0"};
  for (uint32_t i = 0; i < sizeof(manual) / sizeof(manual[0]); i += 2) {
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

    nsCString encHost =
        nsPrintfCString("%d.%d.%d.%d", val[0], val[1], val[2], val[3]);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
    ASSERT_TRUE(result.Equals(encHost));

    nsCString encHostM =
        nsPrintfCString("0x%x.0x%x.0x%x.0x%x", val[0], val[1], val[2], val[3]);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
    ASSERT_TRUE(result.Equals(encHost));

    encHostM =
        nsPrintfCString("0%o.0%o.0%o.0%o", val[0], val[1], val[2], val[3]);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
    ASSERT_TRUE(result.Equals(encHost));

    encHostM =
        nsPrintfCString("0x%x.%d.0%o.%d", val[0], val[1], val[2], val[3]);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
    ASSERT_TRUE(result.Equals(encHost));

    encHostM =
        nsPrintfCString("%d.0%o.0%o.0x%x", val[0], val[1], val[2], val[3]);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
    ASSERT_TRUE(result.Equals(encHost));

    encHostM =
        nsPrintfCString("0%o.0%o.0x%x.0x%x", val[0], val[1], val[2], val[3]);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHostM, result));
    ASSERT_TRUE(result.Equals(encHost));
  }
}

TEST(TestStandardURL, NormalizeBad)
{
  nsAutoCString result;
  const char* manual[] = {
      "x22.232.12.32", "122..12.32",    "122.12.32.12.32",   "122.12.32..",
      "122.12.xx.22",  "122.12.0xx.22", "0xx.12.01.22",      "12.12.02x.22",
      "1q.12.2.22",    "122.01f.02.22", "12a.01.02.22",      "12.01.02.20x1",
      "10x2.01.02.20", "0xx.01.02.20",  "10.x.02.20",        "10.00x2.02.20",
      "10.13.02x2.20", "10.x13.02.20",  "10.0x134def.02.20", "\0.2.2.2",
      "256.2.2.2",     "2.256.2.2",     "2.2.256.2",         "2.2.2.256",
      "2.2.-2.3",      "+2.2.2.3",      "13.0x2x2.2.3",      "0x2x2.13.2.3"};

  for (auto& i : manual) {
    nsCString encHost(i);
    ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost, result));
  }
}

TEST(TestStandardURL, From_test_standardurldotjs)
{
  // These are test (success and failure) cases from test_standardurl.js
  nsAutoCString result;

  const char* localIPv4s[] = {
      "127.0.0.1",
      "127.0.1",
      "127.1",
      "2130706433",
      "0177.00.00.01",
      "0177.00.01",
      "0177.01",
      "00000000000000000000000000177.0000000.0000000.0001",
      "000000177.0000001",
      "017700000001",
      "0x7f.0x00.0x00.0x01",
      "0x7f.0x01",
      "0x7f000001",
      "0x007f.0x0000.0x0000.0x0001",
      "000177.0.00000.0x0001",
      "127.0.0.1.",

      "0X7F.0X00.0X00.0X01",
      "0X7F.0X01",
      "0X7F000001",
      "0X007F.0X0000.0X0000.0X0001",
      "000177.0.00000.0X0001"};
  for (auto& localIPv4 : localIPv4s) {
    nsCString encHost(localIPv4);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
    ASSERT_TRUE(result.EqualsLiteral("127.0.0.1"));
  }

  const char* nonIPv4s[] = {"0xfffffffff", "0x100000000",
                            "4294967296",  "1.2.0x10000",
                            "1.0x1000000", "256.0.0.1",
                            "1.256.1",     "-1.0.0.0",
                            "1.2.3.4.5",   "010000000000000000",
                            "2+3",         "0.0.0.-1",
                            "1.2.3.4..",   "1..2",
                            ".1.2.3.4",    ".127"};
  for (auto& nonIPv4 : nonIPv4s) {
    nsCString encHost(nonIPv4);
    ASSERT_EQ(NS_ERROR_FAILURE, Test_NormalizeIPv4(encHost, result));
  }

  const char* oneOrNoDotsIPv4s[] = {"127", "127."};
  for (auto& localIPv4 : oneOrNoDotsIPv4s) {
    nsCString encHost(localIPv4);
    ASSERT_EQ(NS_OK, Test_NormalizeIPv4(encHost, result));
    ASSERT_TRUE(result.EqualsLiteral("0.0.0.127"));
  }
}

#define TEST_COUNT 10000

MOZ_GTEST_BENCH(TestStandardURL, DISABLED_Perf, [] {
  nsCOMPtr<nsIURI> url;
  ASSERT_EQ(NS_OK, NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                       .SetSpec("http://example.com"_ns)
                       .Finalize(url));

  nsAutoCString out;
  for (int i = TEST_COUNT; i; --i) {
    ASSERT_EQ(NS_MutateURI(url).SetSpec("http://example.com"_ns).Finalize(url),
              NS_OK);
    ASSERT_EQ(url->GetSpec(out), NS_OK);
    url->Resolve("foo.html?q=45"_ns, out);
    mozilla::Unused << NS_MutateURI(url).SetScheme("foo"_ns).Finalize(url);
    url->GetScheme(out);
    mozilla::Unused
        << NS_MutateURI(url).SetHost("www.yahoo.com"_ns).Finalize(url);
    url->GetHost(out);
    mozilla::Unused
        << NS_MutateURI(url)
               .SetPathQueryRef(nsLiteralCString(
                   "/some-path/one-the-net/about.html?with-a-query#for-you"))
               .Finalize(url);
    url->GetPathQueryRef(out);
    mozilla::Unused << NS_MutateURI(url)
                           .SetQuery(nsLiteralCString(
                               "a=b&d=c&what-ever-you-want-to-be-called=45"))
                           .Finalize(url);
    url->GetQuery(out);
    mozilla::Unused
        << NS_MutateURI(url).SetRef("#some-book-mark"_ns).Finalize(url);
    url->GetRef(out);
  }
});

// Note the five calls in the loop, so divide by 100k
MOZ_GTEST_BENCH(TestStandardURL, DISABLED_NormalizePerf, [] {
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

// Bug 1394785 - ignore unstable test on OSX
#ifndef XP_MACOSX
// Note the five calls in the loop, so divide by 100k
MOZ_GTEST_BENCH(TestStandardURL, DISABLED_NormalizePerfFails, [] {
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
#endif

TEST(TestStandardURL, Mutator)
{
  nsAutoCString out;
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                    .SetSpec("http://example.com"_ns)
                    .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);

  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "http://example.com/"_ns);

  rv = NS_MutateURI(uri)
           .SetScheme("ftp"_ns)
           .SetHost("mozilla.org"_ns)
           .SetPathQueryRef("/path?query#ref"_ns)
           .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "ftp://mozilla.org/path?query#ref"_ns);

  nsCOMPtr<nsIURL> url;
  rv = NS_MutateURI(uri).SetScheme("https"_ns).Finalize(url);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(url->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "https://mozilla.org/path?query#ref"_ns);
}

TEST(TestStandardURL, Deserialize_Bug1392739)
{
  mozilla::ipc::StandardURLParams standard_params;
  standard_params.urlType() = nsIStandardURL::URLTYPE_STANDARD;
  standard_params.spec().Truncate();
  standard_params.host() = mozilla::ipc::StandardURLSegment(4294967295, 1);

  mozilla::ipc::URIParams params(standard_params);

  nsCOMPtr<nsIURIMutator> mutator =
      do_CreateInstance(NS_STANDARDURLMUTATOR_CID);
  ASSERT_EQ(mutator->Deserialize(params), NS_ERROR_FAILURE);
}

TEST(TestStandardURL, CorruptSerialization)
{
  auto spec = "http://user:pass@example.com/path/to/file.ext?query#hash"_ns;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                    .SetSpec(spec)
                    .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);

  nsAutoCString serialization;
  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(uri);
  ASSERT_TRUE(serializable);

  // Check that the URL is normally serializable.
  ASSERT_EQ(NS_OK, NS_SerializeToString(serializable, serialization));
  nsCOMPtr<nsISupports> deserializedObject;
  ASSERT_EQ(NS_OK, NS_DeserializeObject(serialization,
                                        getter_AddRefs(deserializedObject)));

  nsAutoCString canonicalBin;
  Unused << Base64Decode(serialization, canonicalBin);

// The spec serialization begins at byte 49
// If the implementation of nsStandardURL::Write changes, this test will need
// to be adjusted.
#define SPEC_OFFSET 49

  ASSERT_EQ(Substring(canonicalBin, SPEC_OFFSET, 7), "http://"_ns);

  nsAutoCString corruptedBin = canonicalBin;
  // change mScheme.mPos
  corruptedBin.BeginWriting()[SPEC_OFFSET + spec.Length()] = 1;
  Unused << Base64Encode(corruptedBin, serialization);
  ASSERT_EQ(
      NS_ERROR_MALFORMED_URI,
      NS_DeserializeObject(serialization, getter_AddRefs(deserializedObject)));

  corruptedBin = canonicalBin;
  // change mScheme.mLen
  corruptedBin.BeginWriting()[SPEC_OFFSET + spec.Length() + 4] = 127;
  Unused << Base64Encode(corruptedBin, serialization);
  ASSERT_EQ(
      NS_ERROR_MALFORMED_URI,
      NS_DeserializeObject(serialization, getter_AddRefs(deserializedObject)));
}

TEST(TestStandardURL, ParseIPv4Num)
{
  auto host = "0x.0x.0"_ns;

  int32_t bases[4] = {10, 10, 10, 10};
  bool onlyBase10 = true;  // Track this as a special case
  int32_t dotIndex[3];     // The positions of the dots in the string
  int32_t length = static_cast<int32_t>(host.Length());

  ASSERT_EQ(2,
            Test_ValidateIPv4Number(host, bases, dotIndex, onlyBase10, length));

  nsCString result;
  ASSERT_EQ(NS_OK, Test_NormalizeIPv4("0x.0x.0"_ns, result));

  uint32_t number;
  Test_ParseIPv4Number("0x10"_ns, 16, number, 255);
  ASSERT_EQ(number, (uint32_t)16);
}
