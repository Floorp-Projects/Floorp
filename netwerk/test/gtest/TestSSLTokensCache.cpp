#include <numeric>

#include "CertVerifier.h"
#include "CommonSocketControl.h"
#include "SSLTokensCache.h"
#include "TransportSecurityInfo.h"
#include "gtest/gtest.h"
#include "mozilla/Preferences.h"
#include "nsITransportSecurityInfo.h"
#include "nsIWebProgressListener.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsServiceManagerUtils.h"
#include "sslproto.h"

static already_AddRefed<CommonSocketControl> createDummySocketControl() {
  nsCOMPtr<nsIX509CertDB> certDB(do_GetService(NS_X509CERTDB_CONTRACTID));
  EXPECT_TRUE(certDB);
  nsLiteralCString base64(
      "MIIBbjCCARWgAwIBAgIUOyCxVVqw03yUxKSfSojsMF8K/"
      "ikwCgYIKoZIzj0EAwIwHTEbMBkGA1UEAwwScm9vdF9zZWNwMjU2azFfMjU2MCIYDzIwMjAxM"
      "TI3MDAwMDAwWhgPMjAyMzAyMDUwMDAwMDBaMC8xLTArBgNVBAMMJGludF9zZWNwMjU2cjFfM"
      "jU2LXJvb3Rfc2VjcDI1NmsxXzI1NjBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABE+/"
      "u7th4Pj5saYKWayHBOLsBQtCPjz3LpI/"
      "LE95S0VcKmnSM0VsNsQRnQcG4A7tyNGTkNeZG3stB6ME6qBKpsCjHTAbMAwGA1UdEwQFMAMB"
      "Af8wCwYDVR0PBAQDAgEGMAoGCCqGSM49BAMCA0cAMEQCIFuwodUwyOUnIR4KN5ZCSrU7y4iz"
      "4/1EWRdHm5kWKi8dAiB6Ixn9sw3uBVbyxnQKYqGnOwM+qLOkJK0W8XkIE3n5sg==");
  nsCOMPtr<nsIX509Cert> cert;
  EXPECT_TRUE(NS_SUCCEEDED(
      certDB->ConstructX509FromBase64(base64, getter_AddRefs(cert))));
  EXPECT_TRUE(cert);
  nsTArray<nsTArray<uint8_t>> succeededCertChain;
  for (size_t i = 0; i < 3; i++) {
    nsTArray<uint8_t> certDER;
    EXPECT_TRUE(NS_SUCCEEDED(cert->GetRawDER(certDER)));
    succeededCertChain.AppendElement(std::move(certDER));
  }
  RefPtr<CommonSocketControl> socketControl(
      new CommonSocketControl(nsLiteralCString("example.com"), 433, 0));
  socketControl->SetServerCert(cert, mozilla::psm::EVStatus::NotEV);
  socketControl->SetSucceededCertChain(std::move(succeededCertChain));
  return socketControl.forget();
}

static auto MakeTestData(const size_t aDataSize) {
  auto data = nsTArray<uint8_t>();
  data.SetLength(aDataSize);
  std::iota(data.begin(), data.end(), 0);
  return data;
}

static void putToken(const nsACString& aKey, uint32_t aSize) {
  RefPtr<CommonSocketControl> socketControl = createDummySocketControl();
  nsTArray<uint8_t> token = MakeTestData(aSize);
  nsresult rv = mozilla::net::SSLTokensCache::Put(aKey, token.Elements(), aSize,
                                                  socketControl, aSize);
  ASSERT_EQ(rv, NS_OK);
}

static void getAndCheckResult(const nsACString& aKey, uint32_t aExpectedSize) {
  nsTArray<uint8_t> result;
  mozilla::net::SessionCacheInfo unused;
  nsresult rv = mozilla::net::SSLTokensCache::Get(aKey, result, unused);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(result.Length(), (size_t)aExpectedSize);
}

TEST(TestTokensCache, SinglePut)
{
  mozilla::net::SSLTokensCache::Clear();
  mozilla::Preferences::SetInt("network.ssl_tokens_cache_records_per_entry", 1);
  mozilla::Preferences::SetBool("network.ssl_tokens_cache_use_only_once", true);

  putToken("anon:www.example.com:443"_ns, 100);
  nsTArray<uint8_t> result;
  mozilla::net::SessionCacheInfo unused;
  nsresult rv = mozilla::net::SSLTokensCache::Get("anon:www.example.com:443"_ns,
                                                  result, unused);
  ASSERT_EQ(rv, NS_OK);
  rv = mozilla::net::SSLTokensCache::Get("anon:www.example.com:443"_ns, result,
                                         unused);
  ASSERT_EQ(rv, NS_ERROR_NOT_AVAILABLE);
}

TEST(TestTokensCache, MultiplePut)
{
  mozilla::net::SSLTokensCache::Clear();
  mozilla::Preferences::SetInt("network.ssl_tokens_cache_records_per_entry", 3);

  putToken("anon:www.example1.com:443"_ns, 300);
  // This record will be removed because
  // "network.ssl_tokens_cache_records_per_entry" is 3.
  putToken("anon:www.example1.com:443"_ns, 100);
  putToken("anon:www.example1.com:443"_ns, 200);
  putToken("anon:www.example1.com:443"_ns, 400);

  // Test if records are ordered by the expiration time
  getAndCheckResult("anon:www.example1.com:443"_ns, 200);
  getAndCheckResult("anon:www.example1.com:443"_ns, 300);
  getAndCheckResult("anon:www.example1.com:443"_ns, 400);
}

TEST(TestTokensCache, RemoveAll)
{
  mozilla::net::SSLTokensCache::Clear();
  mozilla::Preferences::SetInt("network.ssl_tokens_cache_records_per_entry", 3);

  putToken("anon:www.example1.com:443"_ns, 100);
  putToken("anon:www.example1.com:443"_ns, 200);
  putToken("anon:www.example1.com:443"_ns, 300);

  putToken("anon:www.example2.com:443"_ns, 100);
  putToken("anon:www.example2.com:443"_ns, 200);
  putToken("anon:www.example2.com:443"_ns, 300);

  nsTArray<uint8_t> result;
  mozilla::net::SessionCacheInfo unused;
  nsresult rv = mozilla::net::SSLTokensCache::Get(
      "anon:www.example1.com:443"_ns, result, unused);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(result.Length(), (size_t)100);

  rv = mozilla::net::SSLTokensCache::RemoveAll("anon:www.example1.com:443"_ns);
  ASSERT_EQ(rv, NS_OK);

  rv = mozilla::net::SSLTokensCache::Get("anon:www.example1.com:443"_ns, result,
                                         unused);
  ASSERT_EQ(rv, NS_ERROR_NOT_AVAILABLE);

  rv = mozilla::net::SSLTokensCache::Get("anon:www.example2.com:443"_ns, result,
                                         unused);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(result.Length(), (size_t)100);
}

TEST(TestTokensCache, Eviction)
{
  mozilla::net::SSLTokensCache::Clear();

  mozilla::Preferences::SetInt("network.ssl_tokens_cache_records_per_entry", 3);
  mozilla::Preferences::SetInt("network.ssl_tokens_cache_capacity", 8);

  putToken("anon:www.example2.com:443"_ns, 300);
  putToken("anon:www.example2.com:443"_ns, 400);
  putToken("anon:www.example2.com:443"_ns, 500);
  // The one has expiration time "300" will be removed because we only allow 3
  // records per entry.
  putToken("anon:www.example2.com:443"_ns, 600);

  putToken("anon:www.example3.com:443"_ns, 600);
  putToken("anon:www.example3.com:443"_ns, 500);
  // The one has expiration time "400" was evicted, so we get "500".
  getAndCheckResult("anon:www.example2.com:443"_ns, 500);
}
