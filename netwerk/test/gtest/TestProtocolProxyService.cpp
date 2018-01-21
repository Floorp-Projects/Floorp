#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "nsComponentManagerUtils.h"
#include "../../base/nsProtocolProxyService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Preferences.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

TEST(TestProtocolProxyService, LoadHostFilters) {
  nsCOMPtr<nsIProtocolProxyService2> ps = do_GetService(NS_PROTOCOLPROXYSERVICE_CID);
  ASSERT_TRUE(ps);
  mozilla::net::nsProtocolProxyService* pps = static_cast<mozilla::net::nsProtocolProxyService*>(ps.get());

  nsCOMPtr<nsIURI> url;
  nsAutoCString spec;

  auto CheckLoopbackURLs = [&](bool expected)
  {
    // loopback IPs are always filtered
    spec = "http://127.0.0.1";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);
    spec = "http://[::1]";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);
  };

  auto CheckURLs = [&](bool expected)
  {
    spec = "http://example.com";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);

    spec = "https://10.2.3.4";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 443), expected);

    spec = "http://1.2.3.4";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);

    spec = "http://1.2.3.4:8080";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);

    spec = "http://[2001::1]";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);

    spec = "http://2.3.4.5:7777";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);

    spec = "http://[abcd::2]:123";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);

    spec = "http://bla.test.com";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);
  };

  auto CheckPortDomain = [&](bool expected)
  {
    spec = "http://blabla.com:10";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);
  };

  auto CheckLocalDomain = [&](bool expected)
  {
    spec = "http://test";
    ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
    ASSERT_EQ(pps->CanUseProxy(url, 80), expected);
  };

  // --------------------------------------------------------------------------

  nsAutoCString filter;

  // Anything is allowed when there are no filters set
  printf("Testing empty filter: %s\n", filter.get());
  pps->LoadHostFilters(filter);

  CheckLoopbackURLs(true); // only time when loopbacks can be proxied. bug?
  CheckLocalDomain(true);
  CheckURLs(true);
  CheckPortDomain(true);

  // --------------------------------------------------------------------------

  filter = "example.com, 1.2.3.4/16, [2001::1], 10.0.0.0/8, 2.3.0.0/16:7777, [abcd::1]/64:123, *.test.com";
  printf("Testing filter: %s\n", filter.get());
  pps->LoadHostFilters(filter);
  // Check URLs can no longer use filtered proxy
  CheckURLs(false);
  CheckLoopbackURLs(false);
  CheckLocalDomain(true);
  CheckPortDomain(true);

  // --------------------------------------------------------------------------

  // This is space separated. See bug 1346711 comment 4. We check this to keep
  // backwards compatibility.
  filter = "<local> blabla.com:10";
  printf("Testing filter: %s\n", filter.get());
  pps->LoadHostFilters(filter);
  CheckURLs(true);
  CheckLoopbackURLs(false);
  CheckLocalDomain(false);
  CheckPortDomain(false);

  // Check that we don't crash on weird input
  filter = "a b c abc:1x2, ,, * ** *.* *:10 :20 :40/12 */12:90";
  printf("Testing filter: %s\n", filter.get());
  pps->LoadHostFilters(filter);

  // Check that filtering works properly when the filter is set to "<local>"
  filter = "<local>";
  printf("Testing filter: %s\n", filter.get());
  pps->LoadHostFilters(filter);
  CheckURLs(true);
  CheckLoopbackURLs(false);
  CheckLocalDomain(false);
  CheckPortDomain(true);
}

} // namespace net
} // namespace mozila
