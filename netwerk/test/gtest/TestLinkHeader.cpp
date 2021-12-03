/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest-param-test.h"
#include "gtest/gtest.h"

#include "nsNetUtil.h"

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

INSTANTIATE_TEST_CASE_P(TestLinkHeader, SimpleParseTest,
                        testing::ValuesIn(simple_parse_tests));
