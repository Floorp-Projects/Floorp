#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH

#include "nsCOMPtr.h"
#include "mozilla/net/MozURL.h"

using namespace mozilla::net;

TEST(TestMozURL, Getters) {
  nsAutoCString href("http://user:pass@example.com/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  ASSERT_TRUE(url->Scheme().EqualsLiteral("http"));

  ASSERT_TRUE(url->Spec() == href);

  ASSERT_TRUE(url->Username().EqualsLiteral("user"));

  ASSERT_TRUE(url->Password().EqualsLiteral("pass"));

  ASSERT_TRUE(url->Host().EqualsLiteral("example.com"));

  ASSERT_TRUE(url->FilePath().EqualsLiteral("/path"));

  ASSERT_TRUE(url->Query().EqualsLiteral("query"));

  ASSERT_TRUE(url->Ref().EqualsLiteral("ref"));

  url = nullptr;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), NS_LITERAL_CSTRING("")),
            NS_ERROR_MALFORMED_URI);
  ASSERT_EQ(url, nullptr);
}

TEST(TestMozURL, MutatorChain) {
  nsAutoCString href("http://user:pass@example.com/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);
  nsAutoCString out;

  RefPtr<MozURL> url2;
  ASSERT_EQ(url->Mutate()
                .SetScheme(NS_LITERAL_CSTRING("https"))
                .SetUsername(NS_LITERAL_CSTRING("newuser"))
                .SetPassword(NS_LITERAL_CSTRING("newpass"))
                .SetHostname(NS_LITERAL_CSTRING("test"))
                .SetFilePath(NS_LITERAL_CSTRING("new/file/path"))
                .SetQuery(NS_LITERAL_CSTRING("bla"))
                .SetRef(NS_LITERAL_CSTRING("huh"))
                .Finalize(getter_AddRefs(url2)),
            NS_OK);

  ASSERT_TRUE(url2->Spec().EqualsLiteral(
      "https://newuser:newpass@test/new/file/path?bla#huh"));
}

TEST(TestMozURL, MutatorFinalizeTwice) {
  nsAutoCString href("http://user:pass@example.com/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);
  nsAutoCString out;

  RefPtr<MozURL> url2;
  MozURL::Mutator mut = url->Mutate();
  mut.SetScheme(NS_LITERAL_CSTRING("https"));  // Change the scheme to https
  ASSERT_EQ(mut.Finalize(getter_AddRefs(url2)), NS_OK);
  ASSERT_TRUE(url2->Spec().EqualsLiteral(
      "https://user:pass@example.com/path?query#ref"));

  // Test that a second call to Finalize will result in an error code
  url2 = nullptr;
  ASSERT_EQ(mut.Finalize(getter_AddRefs(url2)), NS_ERROR_NOT_AVAILABLE);
  ASSERT_EQ(url2, nullptr);
}

TEST(TestMozURL, MutatorErrorStatus) {
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

TEST(TestMozURL, InitWithBase) {
  nsAutoCString href("https://example.net/a/b.html");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  ASSERT_TRUE(url->Spec().EqualsLiteral("https://example.net/a/b.html"));

  RefPtr<MozURL> url2;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url2), NS_LITERAL_CSTRING("c.png"), url),
      NS_OK);

  ASSERT_TRUE(url2->Spec().EqualsLiteral("https://example.net/a/c.png"));
}

TEST(TestMozURL, Path) {
  nsAutoCString href("about:blank");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  ASSERT_TRUE(url->Spec().EqualsLiteral("about:blank"));

  ASSERT_TRUE(url->Scheme().EqualsLiteral("about"));

  ASSERT_TRUE(url->FilePath().EqualsLiteral("blank"));
}

TEST(TestMozURL, HostPort) {
  nsAutoCString href("https://user:pass@example.net:1234/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  ASSERT_TRUE(url->HostPort().EqualsLiteral("example.net:1234"));

  RefPtr<MozURL> url2;
  url->Mutate()
      .SetHostPort(NS_LITERAL_CSTRING("test:321"))
      .Finalize(getter_AddRefs(url2));

  ASSERT_TRUE(url2->HostPort().EqualsLiteral("test:321"));
  ASSERT_TRUE(
      url2->Spec().EqualsLiteral("https://user:pass@test:321/path?query#ref"));

  href.Assign("https://user:pass@example.net:443/path?query#ref");
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);
  ASSERT_TRUE(url->HostPort().EqualsLiteral("example.net"));
  ASSERT_EQ(url->Port(), -1);
}

TEST(TestMozURL, Origin) {
  nsAutoCString href("https://user:pass@example.net:1234/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  nsAutoCString out;
  url->Origin(out);
  ASSERT_TRUE(out.EqualsLiteral("https://example.net:1234"));

  RefPtr<MozURL> url2;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url2), NS_LITERAL_CSTRING("file:///tmp/foo")),
      NS_OK);
  url2->Origin(out);
  ASSERT_TRUE(out.EqualsLiteral("null"));
}
