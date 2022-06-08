/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest-param-test.h"
#include "gtest/gtest.h"

#include "nsNetUtil.h"

using namespace mozilla::net;

LinkHeader LinkHeaderSetAll(nsAString const& v) {
  LinkHeader l;
  l.mHref = v;
  l.mRel = v;
  l.mTitle = v;
  l.mIntegrity = v;
  l.mSrcset = v;
  l.mSizes = v;
  l.mType = v;
  l.mMedia = v;
  l.mAnchor = v;
  l.mCrossOrigin = v;
  l.mReferrerPolicy = v;
  l.mAs = v;
  return l;
}

LinkHeader LinkHeaderSetTitle(nsAString const& v) {
  LinkHeader l;
  l.mHref = v;
  l.mRel = v;
  l.mTitle = v;
  return l;
}

LinkHeader LinkHeaderSetMinimum(nsAString const& v) {
  LinkHeader l;
  l.mHref = v;
  l.mRel = v;
  return l;
}

TEST(TestLinkHeader, MultipleLinkHeaders)
{
  nsString link =
      u"<a>; rel=a; title=a; integrity=a; imagesrcset=a; imagesizes=a; type=a; media=a; anchor=a; crossorigin=a; referrerpolicy=a; as=a,"_ns
      u"<b>; rel=b; title=b; integrity=b; imagesrcset=b; imagesizes=b; type=b; media=b; anchor=b; crossorigin=b; referrerpolicy=b; as=b,"_ns
      u"<c>; rel=c"_ns;

  nsTArray<LinkHeader> linkHeaders = ParseLinkHeader(link);

  nsTArray<LinkHeader> expected;
  expected.AppendElement(LinkHeaderSetAll(u"a"_ns));
  expected.AppendElement(LinkHeaderSetAll(u"b"_ns));
  expected.AppendElement(LinkHeaderSetMinimum(u"c"_ns));

  ASSERT_EQ(linkHeaders, expected);
}

// title* has to be tested separately
TEST(TestLinkHeader, MultipleLinkHeadersTitleStar)
{
  nsString link =
      u"<d>; rel=d; title*=UTF-8'de'd,"_ns
      u"<e>; rel=e; title*=UTF-8'de'e; title=g,"_ns
      u"<f>; rel=f"_ns;

  nsTArray<LinkHeader> linkHeaders = ParseLinkHeader(link);

  nsTArray<LinkHeader> expected;
  expected.AppendElement(LinkHeaderSetTitle(u"d"_ns));
  expected.AppendElement(LinkHeaderSetTitle(u"e"_ns));
  expected.AppendElement(LinkHeaderSetMinimum(u"f"_ns));

  ASSERT_EQ(linkHeaders, expected);
}

struct SimpleParseTestData {
  nsString link;
  bool valid;
  nsString url;
  nsString rel;
  nsString as;
};

class SimpleParseTest : public ::testing::TestWithParam<SimpleParseTestData> {};

TEST_P(SimpleParseTest, Simple) {
  const SimpleParseTestData test = GetParam();

  nsTArray<LinkHeader> linkHeaders = ParseLinkHeader(test.link);

  EXPECT_EQ(test.valid, !linkHeaders.IsEmpty());
  if (test.valid) {
    ASSERT_EQ(linkHeaders.Length(), (nsTArray<LinkHeader>::size_type)1);
    EXPECT_EQ(test.url, linkHeaders[0].mHref);
    EXPECT_EQ(test.rel, linkHeaders[0].mRel);
    EXPECT_EQ(test.as, linkHeaders[0].mAs);
  }
}

// Test copied and adapted from
// https://source.chromium.org/chromium/chromium/src/+/main:components/link_header_util/link_header_util_unittest.cc
// the different behavior of the parser is commented above each test case.
const SimpleParseTestData simple_parse_tests[] = {
    {u"</images/cat.jpg>; rel=prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>;rel=prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>   ;rel=prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>   ;   rel=prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"< /images/cat.jpg>   ;   rel=prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg >   ;   rel=prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    // TODO(1744051): don't ignore spaces in href
    // {u"</images/cat.jpg wutwut>   ;   rel=prefetch"_ns, true,
    //  u"/images/cat.jpg wutwut"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg wutwut>   ;   rel=prefetch"_ns, true,
     u"/images/cat.jpgwutwut"_ns, u"prefetch"_ns, u""_ns},
    // TODO(1744051): don't ignore spaces in href
    // {u"</images/cat.jpg wutwut  \t >   ;   rel=prefetch"_ns, true,
    //  u"/images/cat.jpg wutwut"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg wutwut  \t >   ;   rel=prefetch"_ns, true,
     u"/images/cat.jpgwutwut"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; rel=prefetch   "_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; Rel=prefetch   "_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; Rel=PReFetCh   "_ns, true, u"/images/cat.jpg"_ns,
     u"PReFetCh"_ns, u""_ns},
    {u"</images/cat.jpg>; rel=prefetch; rel=somethingelse"_ns, true,
     u"/images/cat.jpg"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>\t\t ; \trel=prefetch \t  "_ns, true,
     u"/images/cat.jpg"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; rel= prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"<../images/cat.jpg?dog>; rel= prefetch"_ns, true,
     u"../images/cat.jpg?dog"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; rel =prefetch"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; rel pel=prefetch"_ns, false},
    // different from chromium test case, because we already check for
    // existence of "rel" parameter
    {u"< /images/cat.jpg>"_ns, false},
    {u"</images/cat.jpg>; wut=sup; rel =prefetch"_ns, true,
     u"/images/cat.jpg"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; wut=sup ; rel =prefetch"_ns, true,
     u"/images/cat.jpg"_ns, u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; wut=sup ; rel =prefetch  \t  ;"_ns, true,
     u"/images/cat.jpg"_ns, u"prefetch"_ns, u""_ns},
    // TODO(1744051): forbid non-whitespace characters between '>' and the first
    // semicolon making it conform RFC 8288 Sec 3
    // {u"</images/cat.jpg> wut=sup ; rel =prefetch  \t  ;"_ns, false},
    {u"</images/cat.jpg> wut=sup ; rel =prefetch  \t  ;"_ns, true,
     u"/images/cat.jpg"_ns, u"prefetch"_ns, u""_ns},
    {u"<   /images/cat.jpg"_ns, false},
    // TODO(1744051): don't ignore spaces in href
    // {u"<   http://wut.com/  sdfsdf ?sd>; rel=dns-prefetch"_ns, true,
    //  u"http://wut.com/  sdfsdf ?sd"_ns, u"dns-prefetch"_ns, u""_ns},
    {u"<   http://wut.com/  sdfsdf ?sd>; rel=dns-prefetch"_ns, true,
     u"http://wut.com/sdfsdf?sd"_ns, u"dns-prefetch"_ns, u""_ns},
    {u"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=dns-prefetch"_ns, true,
     u"http://wut.com/%20%20%3dsdfsdf?sd"_ns, u"dns-prefetch"_ns, u""_ns},
    {u"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>; rel=prefetch"_ns, true,
     u"http://wut.com/dfsdf?sdf=ghj&wer=rty"_ns, u"prefetch"_ns, u""_ns},
    {u"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>;;;;; rel=prefetch"_ns, true,
     u"http://wut.com/dfsdf?sdf=ghj&wer=rty"_ns, u"prefetch"_ns, u""_ns},
    {u"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=image"_ns, true,
     u"http://wut.com/%20%20%3dsdfsdf?sd"_ns, u"preload"_ns, u"image"_ns},
    {u"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=whatever"_ns,
     true, u"http://wut.com/%20%20%3dsdfsdf?sd"_ns, u"preload"_ns,
     u"whatever"_ns},
    {u"</images/cat.jpg>; rel=prefetch;"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/cat.jpg>; rel=prefetch    ;"_ns, true, u"/images/cat.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"</images/ca,t.jpg>; rel=prefetch    ;"_ns, true, u"/images/ca,t.jpg"_ns,
     u"prefetch"_ns, u""_ns},
    {u"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE and "
     "backslash\""_ns,
     true, u"simple.css"_ns, u"stylesheet"_ns, u""_ns},
    // TODO(1744051): forbid missing end quote
    // {u"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE \\\" and "
    //  "backslash: \\\""_ns, false},
    {u"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE \\\" and backslash: \\\""_ns,
     true, u"simple.css"_ns, u"stylesheet"_ns, u""_ns},
    {u"<simple.css>; title=\"title with a DQUOTE \\\" and backslash: \"; "
     "rel=stylesheet; "_ns,
     true, u"simple.css"_ns, u"stylesheet"_ns, u""_ns},
    {u"<simple.css>; title=\'title with a DQUOTE \\\' and backslash: \'; "
     "rel=stylesheet; "_ns,
     true, u"simple.css"_ns, u"stylesheet"_ns, u""_ns},
    {u"<simple.css>; title=\"title with a DQUOTE \\\" and ;backslash,: \"; "
     "rel=stylesheet; "_ns,
     true, u"simple.css"_ns, u"stylesheet"_ns, u""_ns},
    {u"<simple.css>; title=\"title with a DQUOTE \' and ;backslash,: \"; "
     "rel=stylesheet; "_ns,
     true, u"simple.css"_ns, u"stylesheet"_ns, u""_ns},
    {u"<simple.css>; title=\"\"; rel=stylesheet; "_ns, true, u"simple.css"_ns,
     u"stylesheet"_ns, u""_ns},
    {u"<simple.css>; title=\"\"; rel=\"stylesheet\"; "_ns, true,
     u"simple.css"_ns, u"stylesheet"_ns, u""_ns},
    // TODO(1744051): forbid missing end quote
    // {u"<simple.css>; rel=stylesheet; title=\""_ns, false},
    {u"<simple.css>; rel=stylesheet; title=\""_ns, true, u"simple.css"_ns,
     u"stylesheet"_ns, u""_ns},
    {u"<simple.css>; rel=stylesheet; title=\"\""_ns, true, u"simple.css"_ns,
     u"stylesheet"_ns, u""_ns},
    // TODO(1744051): forbid missing end quote
    // {u"<simple.css>; rel=\"stylesheet\"; title=\""_ns, false},
    {u"<simple.css>; rel=\"stylesheet\"; title=\""_ns, true, u"simple.css"_ns,
     u"stylesheet"_ns, u""_ns},
    // TODO(1744051): forbid missing end quote
    // {u"<simple.css>; rel=\";style,sheet\"; title=\""_ns, false},
    {u"<simple.css>; rel=\";style,sheet\"; title=\""_ns, true, u"simple.css"_ns,
     u";style,sheet"_ns, u""_ns},
    // TODO(1744051): forbid missing end quote
    // {u"<simple.css>; rel=\"bla'sdf\"; title=\""_ns, false}
    {u"<simple.css>; rel=\"bla'sdf\"; title=\""_ns, true, u"simple.css"_ns,
     u"bla'sdf"_ns, u""_ns},
    // TODO(1744051): allow explicit empty rel
    // {u"<simple.css>; rel=\"\"; title=\"\""_ns, true, u"simple.css"_ns,
    //  u""_ns, u""_ns}
    {u"<simple.css>; rel=\"\"; title=\"\""_ns, false},
    {u"<simple.css>; rel=''; title=\"\""_ns, true, u"simple.css"_ns, u"''"_ns,
     u""_ns},
    {u"<simple.css>; rel=''; bla"_ns, true, u"simple.css"_ns, u"''"_ns, u""_ns},
    {u"<simple.css>; rel='prefetch"_ns, true, u"simple.css"_ns, u"'prefetch"_ns,
     u""_ns},
    // TODO(1744051): forbid missing end quote
    // {u"<simple.css>; rel=\"prefetch"_ns, false},
    {u"<simple.css>; rel=\"prefetch"_ns, true, u"simple.css"_ns,
     u"\"prefetch"_ns, u""_ns},
    {u"<simple.css>; rel=\""_ns, false},
    {u"simple.css; rel=prefetch"_ns, false},
    {u"<simple.css>; rel=prefetch; rel=foobar"_ns, true, u"simple.css"_ns,
     u"prefetch"_ns, u""_ns},
};

INSTANTIATE_TEST_SUITE_P(TestLinkHeader, SimpleParseTest,
                         testing::ValuesIn(simple_parse_tests));

// Test anchor

struct AnchorTestData {
  nsString baseURI;
  // building the new anchor in combination with the baseURI
  nsString anchor;
  nsString href;
  const char* resolved;
};

class AnchorTest : public ::testing::TestWithParam<AnchorTestData> {};

const AnchorTestData anchor_tests[] = {
    {u"http://example.com/path/to/index.html"_ns, u""_ns, u"page.html"_ns,
     "http://example.com/path/to/page.html"},
    {u"http://example.com/path/to/index.html"_ns,
     u"http://example.com/path/"_ns, u"page.html"_ns,
     "http://example.com/path/page.html"},
    {u"http://example.com/path/to/index.html"_ns,
     u"http://example.com/path/"_ns, u"/page.html"_ns,
     "http://example.com/page.html"},
    {u"http://example.com/path/to/index.html"_ns, u".."_ns, u"page.html"_ns,
     "http://example.com/path/page.html"},
    {u"http://example.com/path/to/index.html"_ns, u".."_ns,
     u"from/page.html"_ns, "http://example.com/path/from/page.html"},
    {u"http://example.com/path/to/index.html"_ns, u"/hello/"_ns,
     u"page.html"_ns, "http://example.com/hello/page.html"},
    {u"http://example.com/path/to/index.html"_ns, u"/hello"_ns, u"page.html"_ns,
     "http://example.com/page.html"},
    {u"http://example.com/path/to/index.html"_ns, u"#necko"_ns, u"page.html"_ns,
     "http://example.com/path/to/page.html"},
    {u"http://example.com/path/to/index.html"_ns, u"https://example.net/"_ns,
     u"to/page.html"_ns, "https://example.net/to/page.html"},
};

LinkHeader LinkHeaderFromHrefAndAnchor(nsAString const& aHref,
                                       nsAString const& aAnchor) {
  LinkHeader l;
  l.mHref = aHref;
  l.mAnchor = aAnchor;
  return l;
}

TEST_P(AnchorTest, Anchor) {
  const AnchorTestData test = GetParam();

  LinkHeader linkHeader = LinkHeaderFromHrefAndAnchor(test.href, test.anchor);

  nsCOMPtr<nsIURI> baseURI;
  ASSERT_TRUE(NS_SUCCEEDED(NS_NewURI(getter_AddRefs(baseURI), test.baseURI)));

  nsCOMPtr<nsIURI> resolved;
  ASSERT_TRUE(NS_SUCCEEDED(
      linkHeader.NewResolveHref(getter_AddRefs(resolved), baseURI)));

  ASSERT_STREQ(resolved->GetSpecOrDefault().get(), test.resolved);
}

INSTANTIATE_TEST_SUITE_P(TestLinkHeader, AnchorTest,
                         testing::ValuesIn(anchor_tests));
