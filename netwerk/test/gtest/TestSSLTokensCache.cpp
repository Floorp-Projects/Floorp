#include "gtest/gtest.h"

#include <numeric>
#include "mozilla/Preferences.h"
#include "nsITransportSecurityInfo.h"
#include "nsSerializationHelper.h"
#include "SSLTokensCache.h"

static already_AddRefed<nsITransportSecurityInfo> createDummySecInfo() {
  // clang-format off
  nsCString base64Serialization(
  "FnhllAKWRHGAlo+ESXykKAAAAAAAAAAAwAAAAAAAAEaphjojH6pBabDSgSnsfLHeAAQAAgAAAAAAAAAAAAAAAAAAAAA"
  "B4vFIJp5wRkeyPxAQ9RJGKPqbqVvKO0mKuIl8ec8o/uhmCjImkVxP+7sgiYWmMt8F+O2DZM7ZTG6GukivU8OT5gAAAAIAAAWpMII"
  "FpTCCBI2gAwIBAgIQD4svsaKEC+QtqtsU2TF8ITANBgkqhkiG9w0BAQsFADBwMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUN"
  "lcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMS8wLQYDVQQDEyZEaWdpQ2VydCBTSEEyIEhpZ2ggQXNzdXJhbmNlIFN"
  "lcnZlciBDQTAeFw0xNTAyMjMwMDAwMDBaFw0xNjAzMDIxMjAwMDBaMGoxCzAJBgNVBAYTAlVTMRYwFAYDVQQHEw1TYW4gRnJhbmN"
  "pc2NvMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRUwEwYDVQQKEwxGYXN0bHksIEluYy4xFzAVBgNVBAMTDnd3dy5naXRodWIuY29tMII"
  "BIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA+9WUCgrgUNwP/JC3cUefLAXeDpq8Ko/U8p8IRvny0Ri0I6Uq0t+RP/nF0LJ"
  "Avda8QHYujdgeDTePepBX7+OiwBFhA0YO+rM3C2Z8IRaN/i9eLln+Yyc68+1z+E10s1EXdZrtDGvN6MHqygGsdfkXKfBLUJ1BZEh"
  "s9sBnfcjq3kh5gZdBArdG9l5NpdmQhtceaFGsPiWuJxGxRzS4i95veUHWkhMpEYDEEBdcDGxqArvQCvzSlngdttQCfx8OUkBTb3B"
  "A2okpTwwJfqPsxVetA6qR7UNc+fVb6KHwvm0bzi2rQ3xw3D/syRHwdMkpoVDQPCk43H9WufgfBKRen87dFwIDAQABo4ICPzCCAjs"
  "wHwYDVR0jBBgwFoAUUWj/kK8CB3U8zNllZGKiErhZcjswHQYDVR0OBBYEFGS/RLNGCZvPWh1xSaIEcouINIQjMHsGA1UdEQR0MHK"
  "CDnd3dy5naXRodWIuY29tggpnaXRodWIuY29tggwqLmdpdGh1Yi5jb22CCyouZ2l0aHViLmlvgglnaXRodWIuaW+CFyouZ2l0aHV"
  "idXNlcmNvbnRlbnQuY29tghVnaXRodWJ1c2VyY29udGVudC5jb20wDgYDVR0PAQH/BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwM"
  "BBggrBgEFBQcDAjB1BgNVHR8EbjBsMDSgMqAwhi5odHRwOi8vY3JsMy5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzMuY3J"
  "sMDSgMqAwhi5odHRwOi8vY3JsNC5kaWdpY2VydC5jb20vc2hhMi1oYS1zZXJ2ZXItZzMuY3JsMEIGA1UdIAQ7MDkwNwYJYIZIAYb"
  "9bAEBMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwgYMGCCsGAQUFBwEBBHcwdTAkBggrBgEFBQc"
  "wAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tME0GCCsGAQUFBzAChkFodHRwOi8vY2FjZXJ0cy5kaWdpY2VydC5jb20vRGlnaUN"
  "lcnRTSEEySGlnaEFzc3VyYW5jZVNlcnZlckNBLmNydDAMBgNVHRMBAf8EAjAAMA0GCSqGSIb3DQEBCwUAA4IBAQAc4dbVmuKvyI7"
  "KZ4Txk+ZqcAYToJGKUIVaPL94e5SZGweUisjaCbplAOihnf6Mxt8n6vnuH2IsCaz2NRHqhdcosjT3CwAiJpJNkXPKWVL/txgdSTV"
  "2cqB1GG4esFOalvI52dzn+J4fTIYZvNF+AtGyHSLm2XRXYZCw455laUKf6Sk9RDShDgUvzhOKL4GXfTwKXv12MyMknJybH8UCpjC"
  "HZmFBVHMcUN/87HsQo20PdOekeEvkjrrMIxW+gxw22Yb67yF/qKgwrWr+43bLN709iyw+LWiU7sQcHL2xk9SYiWQDj2tYz2soObV"
  "QYTJm0VUZMEVFhtALq46cx92Zu4vFwC8AAwAAAAABAQAA");
  // clang-format on
  nsCOMPtr<nsISupports> secInfo;
  NS_DeserializeObject(base64Serialization, getter_AddRefs(secInfo));

  nsCOMPtr<nsITransportSecurityInfo> securityInfo = do_QueryInterface(secInfo);
  return securityInfo.forget();
}

static auto MakeTestData(const size_t aDataSize) {
  auto data = nsTArray<uint8_t>();
  data.SetLength(aDataSize);
  std::iota(data.begin(), data.end(), 0);
  return data;
}

static void putToken(const nsACString& aKey, uint32_t aSize) {
  nsCOMPtr<nsITransportSecurityInfo> secInfo = createDummySecInfo();
  nsTArray<uint8_t> token = MakeTestData(aSize);
  nsresult rv = mozilla::net::SSLTokensCache::Put(aKey, token.Elements(), aSize,
                                                  secInfo, aSize);
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
  mozilla::Preferences::SetBool("network.ssl_tokens_cache_use_only_once",
                                false);

  putToken("anon:www.example.com:443"_ns, 100);
  nsTArray<uint8_t> result;
  mozilla::net::SessionCacheInfo unused;
  uint64_t id = 0;
  nsresult rv = mozilla::net::SSLTokensCache::Get("anon:www.example.com:443"_ns,
                                                  result, unused, &id);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(result.Length(), (size_t)100);
  ASSERT_EQ(id, (uint64_t)1);
  rv = mozilla::net::SSLTokensCache::Get("anon:www.example.com:443"_ns, result,
                                         unused, &id);
  ASSERT_EQ(rv, NS_OK);

  mozilla::Preferences::SetBool("network.ssl_tokens_cache_use_only_once", true);
  // network.ssl_tokens_cache_use_only_once is true, so the record will be
  // removed after SSLTokensCache::Get below.
  rv = mozilla::net::SSLTokensCache::Get("anon:www.example.com:443"_ns, result,
                                         unused);
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
