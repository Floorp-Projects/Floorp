#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h" // For MOZ_GTEST_BENCH

#include "nsCOMPtr.h"
#include "../../base/MozURL.h"

using namespace mozilla::net;

TEST(TestMozURL, Getters)
{
  nsAutoCString href("http://user:pass@example.com/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  nsAutoCString out;

  ASSERT_EQ(url->GetScheme(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("http"));

  ASSERT_EQ(url->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == href);

  ASSERT_EQ(url->GetUsername(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("user"));

  ASSERT_EQ(url->GetPassword(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("pass"));

  ASSERT_EQ(url->GetHostname(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("example.com"));

  ASSERT_EQ(url->GetFilePath(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("/path"));

  ASSERT_EQ(url->GetQuery(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("query"));

  ASSERT_EQ(url->GetRef(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("ref"));

  url = nullptr;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), NS_LITERAL_CSTRING("")),
            NS_ERROR_FAILURE);
  ASSERT_EQ(url, nullptr);
}

TEST(TestMozURL, MutatorChain)
{
  nsAutoCString href("http://user:pass@example.com/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);
  nsAutoCString out;

  RefPtr<MozURL> url2;
  ASSERT_EQ(url->Mutate().SetScheme(NS_LITERAL_CSTRING("https"))
                         .SetUsername(NS_LITERAL_CSTRING("newuser"))
                         .SetPassword(NS_LITERAL_CSTRING("newpass"))
                         .SetHostname(NS_LITERAL_CSTRING("test"))
                         .SetFilePath(NS_LITERAL_CSTRING("new/file/path"))
                         .SetQuery(NS_LITERAL_CSTRING("bla"))
                         .SetRef(NS_LITERAL_CSTRING("huh"))
                         .Finalize(getter_AddRefs(url2)), NS_OK);

  ASSERT_EQ(url2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("https://newuser:newpass@test/new/file/path?bla#huh"));
}

TEST(TestMozURL, MutatorFinalizeTwice)
{
  nsAutoCString href("http://user:pass@example.com/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);
  nsAutoCString out;

  RefPtr<MozURL> url2;
  MozURL::Mutator mut = url->Mutate();
  mut.SetScheme(NS_LITERAL_CSTRING("https")); // Change the scheme to https
  ASSERT_EQ(mut.Finalize(getter_AddRefs(url2)), NS_OK);
  ASSERT_EQ(url2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("https://user:pass@example.com/path?query#ref"));

  // Test that a second call to Finalize will result in an error code
  url2 = nullptr;
  ASSERT_EQ(mut.Finalize(getter_AddRefs(url2)), NS_ERROR_NOT_AVAILABLE);
  ASSERT_EQ(url2, nullptr);
}

TEST(TestMozURL, MutatorErrorStatus)
{
  nsAutoCString href("http://user:pass@example.com/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);
  nsAutoCString out;

  // Test that trying to set the scheme to a bad value will get you an error
  MozURL::Mutator mut = url->Mutate();
  mut.SetScheme(NS_LITERAL_CSTRING("!@#$%^&*("));
  ASSERT_EQ(mut.GetStatus(), NS_ERROR_MALFORMED_URI);

  // Test that the mutator will not work after one faulty operation
  mut.SetScheme(NS_LITERAL_CSTRING("test"));
  ASSERT_EQ(mut.GetStatus(), NS_ERROR_MALFORMED_URI);
}

TEST(TestMozURL, InitWithBase)
{
  nsAutoCString href("https://example.net/a/b.html");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);
  nsAutoCString out;

  ASSERT_EQ(url->GetSpec(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("https://example.net/a/b.html"));

  RefPtr<MozURL> url2;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url2), NS_LITERAL_CSTRING("c.png"),
                         url), NS_OK);

  ASSERT_EQ(url2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("https://example.net/a/c.png"));
}
