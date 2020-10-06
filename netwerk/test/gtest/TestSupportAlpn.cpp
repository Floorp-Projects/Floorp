#include "gtest/gtest.h"

#include "mozilla/Preferences.h"
#include "nsIHttpProtocolHandler.h"
#include "nsHttp.h"

namespace mozilla {
namespace net {

TEST(TestSupportAlpn, testSvcParamKeyAlpn)
{
  Preferences::SetBool("network.http.http3.enabled", true);
  // initialize gHttphandler.
  nsCOMPtr<nsIHttpProtocolHandler> http =
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http");

  // h2 and h3 are enabled, so first appearance h3 alpn-id is returned.
  nsCString result =
      SelectAlpnFromAlpnList("h3-28,h3-27,h2,http/1.1"_ns, false, false);
  ASSERT_EQ(result, "h3-28"_ns);

  // h3 is disabled, so we will use h2.
  result = SelectAlpnFromAlpnList("h3-28,h3-27,h2,http/1.1"_ns, false, true);
  ASSERT_EQ(result, "h2"_ns);

  // h3 is disabled and h2 is not found, so we will select http/1.1.
  result = SelectAlpnFromAlpnList("h3-28,h3-27,http/1.1"_ns, false, true);
  ASSERT_EQ(result, "http/1.1"_ns);

  // h3 and h2 are disabled, so we will select http/1.1.
  result = SelectAlpnFromAlpnList("h3-28,h3-27,h2,http/1.1"_ns, true, true);
  ASSERT_EQ(result, "http/1.1"_ns);

  // h3 and h2 are disabled and http1.1 is not found, we return an empty string.
  result = SelectAlpnFromAlpnList("h3-28,h3-27,h2"_ns, true, true);
  ASSERT_EQ(result, ""_ns);

  // No supported alpn.
  result = SelectAlpnFromAlpnList("ftp,h2c"_ns, true, true);
  ASSERT_EQ(result, ""_ns);
}

}  // namespace net
}  // namespace mozilla
