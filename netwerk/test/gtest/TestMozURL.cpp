#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH

#include <regex>
#include "json/json.h"
#include "json/reader.h"
#include "mozilla/TextUtils.h"
#include "mozilla/net/MozURL.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsStreamUtils.h"

using namespace mozilla;
using namespace mozilla::net;

TEST(TestMozURL, Getters)
{
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

TEST(TestMozURL, MutatorChain)
{
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

TEST(TestMozURL, MutatorFinalizeTwice)
{
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

  ASSERT_TRUE(url->Spec().EqualsLiteral("https://example.net/a/b.html"));

  RefPtr<MozURL> url2;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url2), NS_LITERAL_CSTRING("c.png"), url),
      NS_OK);

  ASSERT_TRUE(url2->Spec().EqualsLiteral("https://example.net/a/c.png"));
}

TEST(TestMozURL, Path)
{
  nsAutoCString href("about:blank");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  ASSERT_TRUE(url->Spec().EqualsLiteral("about:blank"));

  ASSERT_TRUE(url->Scheme().EqualsLiteral("about"));

  ASSERT_TRUE(url->FilePath().EqualsLiteral("blank"));
}

TEST(TestMozURL, HostPort)
{
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

TEST(TestMozURL, Origin)
{
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
  ASSERT_TRUE(out.EqualsLiteral("file:///tmp/foo"));

  RefPtr<MozURL> url3;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url3),
                   NS_LITERAL_CSTRING(
                       "moz-extension://53711a8f-65ed-e742-9671-1f02e267c0bc/"
                       "foo/bar.html")),
      NS_OK);
  url3->Origin(out);
  ASSERT_TRUE(out.EqualsLiteral(
      "moz-extension://53711a8f-65ed-e742-9671-1f02e267c0bc"));

  RefPtr<MozURL> url4;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url4),
                         NS_LITERAL_CSTRING("resource://foo/bar.html")),
            NS_OK);
  url4->Origin(out);
  ASSERT_TRUE(out.EqualsLiteral("resource://foo"));

  RefPtr<MozURL> url5;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url5), NS_LITERAL_CSTRING("about:home")),
      NS_OK);
  url5->Origin(out);
  ASSERT_TRUE(out.EqualsLiteral("about:home"));
}

TEST(TestMozURL, BaseDomain)
{
  nsAutoCString href("https://user:pass@example.net:1234/path?query#ref");
  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), href), NS_OK);

  nsAutoCString out;
  ASSERT_EQ(url->BaseDomain(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("example.net"));

  RefPtr<MozURL> url2;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url2), NS_LITERAL_CSTRING("file:///tmp/foo")),
      NS_OK);
  ASSERT_EQ(url2->BaseDomain(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("/tmp/foo"));

  RefPtr<MozURL> url3;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url3),
                   NS_LITERAL_CSTRING(
                       "moz-extension://53711a8f-65ed-e742-9671-1f02e267c0bc/"
                       "foo/bar.html")),
      NS_OK);
  ASSERT_EQ(url3->BaseDomain(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("53711a8f-65ed-e742-9671-1f02e267c0bc"));

  RefPtr<MozURL> url4;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url4),
                         NS_LITERAL_CSTRING("resource://foo/bar.html")),
            NS_OK);
  ASSERT_EQ(url4->BaseDomain(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("foo"));

  RefPtr<MozURL> url5;
  ASSERT_EQ(
      MozURL::Init(getter_AddRefs(url5), NS_LITERAL_CSTRING("about:home")),
      NS_OK);
  ASSERT_EQ(url5->BaseDomain(out), NS_OK);
  ASSERT_TRUE(out.EqualsLiteral("about:home"));
}

namespace {

bool OriginMatchesExpectedOrigin(const nsACString& aOrigin,
                                 const nsACString& aExpectedOrigin) {
  if (aExpectedOrigin.Equals("null") &&
      StringBeginsWith(aOrigin, NS_LITERAL_CSTRING("moz-nullprincipal"))) {
    return true;
  }
  return aOrigin == aExpectedOrigin;
}

bool IsUUID(const nsACString& aString) {
  if (!IsAscii(aString)) {
    return false;
  }

  std::regex pattern(
      "^\\{[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-4[0-9A-Fa-f]{3}-[89ABab"
      "][0-9A-Fa-f]{3}-[0-9A-Fa-f]{12}\\}$");
  return regex_match(nsCString(aString).get(), pattern);
}

bool BaseDomainsEqual(const nsACString& aBaseDomain1,
                      const nsACString& aBaseDomain2) {
  if (IsUUID(aBaseDomain1) && IsUUID(aBaseDomain2)) {
    return true;
  }
  return aBaseDomain1 == aBaseDomain2;
}

void CheckOrigin(const nsACString& aSpec, const nsACString& aBase,
                 const nsACString& aOrigin) {
  nsCOMPtr<nsIURI> baseUri;
  nsresult rv = NS_NewURI(getter_AddRefs(baseUri), aBase);
  ASSERT_EQ(rv, NS_OK);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aSpec, nullptr, baseUri);
  ASSERT_EQ(rv, NS_OK);

  OriginAttributes attrs;

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(principal);

  nsCString origin;
  rv = principal->GetOriginNoSuffix(origin);
  ASSERT_EQ(rv, NS_OK);

  EXPECT_TRUE(OriginMatchesExpectedOrigin(origin, aOrigin));

  nsCString baseDomain;
  rv = principal->GetBaseDomain(baseDomain);

  bool baseDomainSucceeded = NS_SUCCEEDED(rv);

  RefPtr<MozURL> baseUrl;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(baseUrl), aBase), NS_OK);

  RefPtr<MozURL> url;
  ASSERT_EQ(MozURL::Init(getter_AddRefs(url), aSpec, baseUrl), NS_OK);

  url->Origin(origin);

  EXPECT_TRUE(OriginMatchesExpectedOrigin(origin, aOrigin));

  nsCString baseDomain2;
  rv = url->BaseDomain(baseDomain2);

  bool baseDomain2Succeeded = NS_SUCCEEDED(rv);

  EXPECT_TRUE(baseDomainSucceeded == baseDomain2Succeeded);

  if (baseDomainSucceeded) {
    EXPECT_TRUE(BaseDomainsEqual(baseDomain, baseDomain2));
  }
}

}  // namespace

TEST(TestMozURL, UrlTestData)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(file));
  ASSERT_EQ(rv, NS_OK);

  rv = file->Append(NS_LITERAL_STRING("urltestdata.json"));
  ASSERT_EQ(rv, NS_OK);

  bool exists;
  rv = file->Exists(&exists);
  ASSERT_EQ(rv, NS_OK);

  ASSERT_TRUE(exists);

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
  ASSERT_EQ(rv, NS_OK);

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                 stream.forget(), 4096);
  ASSERT_EQ(rv, NS_OK);

  nsCString data;
  rv = NS_ConsumeStream(bufferedStream, UINT32_MAX, data);
  ASSERT_EQ(rv, NS_OK);

  Json::Reader reader;
  Json::Value root;
  ASSERT_TRUE(reader.parse(data.BeginReading(), data.EndReading(), root));
  ASSERT_TRUE(root.isArray());

  for (uint32_t index = 0; index < root.size(); index++) {
    const Json::Value& item = root[index];

    if (!item.isObject()) {
      continue;
    }

    const Json::Value& skip = item["skip"];
    ASSERT_TRUE(skip.isNull() || skip.isBool());
    if (skip.isBool() && skip.asBool()) {
      continue;
    }

    const Json::Value& failure = item["failure"];
    ASSERT_TRUE(failure.isNull() || failure.isBool());
    if (failure.isBool() && failure.asBool()) {
      continue;
    }

    const Json::Value& origin = item["origin"];
    ASSERT_TRUE(origin.isNull() || origin.isString());
    if (origin.isNull()) {
      continue;
    }
    const char* originBegin;
    const char* originEnd;
    origin.getString(&originBegin, &originEnd);

    const Json::Value& base = item["base"];
    ASSERT_TRUE(base.isString());
    const char* baseBegin;
    const char* baseEnd;
    base.getString(&baseBegin, &baseEnd);

    const Json::Value& input = item["input"];
    ASSERT_TRUE(input.isString());
    const char* inputBegin;
    const char* inputEnd;
    input.getString(&inputBegin, &inputEnd);

    CheckOrigin(nsDependentCString(inputBegin, inputEnd),
                nsDependentCString(baseBegin, baseEnd),
                nsDependentCString(originBegin, originEnd));
  }
}
