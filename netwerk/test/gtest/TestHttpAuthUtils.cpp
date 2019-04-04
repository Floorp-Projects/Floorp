#include "gtest/gtest.h"

#include "mozilla/net/HttpAuthUtils.h"
#include "mozilla/Preferences.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

#define TEST_PREF "network.http_test.auth_utils"

TEST(TestHttpAuthUtils, Bug1351301)
{
  nsCOMPtr<nsIURI> url;
  nsAutoCString spec;

  ASSERT_EQ(Preferences::SetCString(TEST_PREF, "bar.com"), NS_OK);
  spec = "http://bar.com";
  ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
  ASSERT_EQ(auth::URIMatchesPrefPattern(url, TEST_PREF), true);

  spec = "http://foo.bar.com";
  ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
  ASSERT_EQ(auth::URIMatchesPrefPattern(url, TEST_PREF), true);

  spec = "http://foobar.com";
  ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
  ASSERT_EQ(auth::URIMatchesPrefPattern(url, TEST_PREF), false);

  ASSERT_EQ(Preferences::SetCString(TEST_PREF, ".bar.com"), NS_OK);
  spec = "http://foo.bar.com";
  ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
  ASSERT_EQ(auth::URIMatchesPrefPattern(url, TEST_PREF), true);

  spec = "http://bar.com";
  ASSERT_EQ(NS_NewURI(getter_AddRefs(url), spec), NS_OK);
  ASSERT_EQ(auth::URIMatchesPrefPattern(url, TEST_PREF), false);

  ASSERT_EQ(Preferences::ClearUser(TEST_PREF), NS_OK);
}

}  // namespace net
}  // namespace mozilla
