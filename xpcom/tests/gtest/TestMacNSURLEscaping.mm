#include "nsCocoaUtils.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "gtest/gtest.h"

#include <CoreFoundation/CoreFoundation.h>

//
// For the macOS File->Share menu, we must create an NSURL. However, NSURL is
// more strict than the browser about the encoding of URLs it accepts.
// Therefore additional encoding must be done on a URL before it is used to
// create an NSURL object. These tests aim to exercise the code used to
// perform additional encoding on a URL used to create NSURL objects.
//

// Ensure nsCocoaUtils::ToNSURL() didn't change the URL.
// Create an NSURL with the provided string and then read the URL out of
// the NSURL and test it matches the provided string.
void ExpectUnchangedByNSURL(nsCString& aEncoded) {
  NSURL* macURL = nsCocoaUtils::ToNSURL(NS_ConvertUTF8toUTF16(aEncoded));
  NSString* macURLString = [macURL absoluteString];

  nsString geckoURLString;
  nsCocoaUtils::GetStringForNSString(macURLString, geckoURLString);
  EXPECT_STREQ(aEncoded.BeginReading(),
               NS_ConvertUTF16toUTF8(geckoURLString).get());
}

// Test escaping of URLs to ensure that
// 1) We escape URLs in such a way that macOS's NSURL code accepts the URL as
//    valid.
// 2) The encoding encoded the expected characters only.
// 2) NSURL not modify the URL. Check this by reading the URL back out of the
//    NSURL object and comparing it. If the URL is changed by creating an
//    NSURL, it may indicate the encoding is incorrect.
//
// It is not a requirement that NSURL not change the URL, but we don't
// expect that for these test cases.
TEST(NSURLEscaping, NSURLEscapingTests)
{
  // Per RFC2396, URI "unreserved" characters. These are allowed in URIs and
  // can be escaped without changing the semantics of the URI, but the escaping
  // should only be done if the URI is used in a context that requires it.
  //
  //   "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
  //
  // These are the URI general "reserved" characters. Their reserved purpose
  // is as delimters so they don't need to be escaped unless used in a URI
  // component in a way that conflicts with the reserved purpose. i.e.,
  // whether or not they must be encoded depends on the component.
  //
  //   ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
  //
  // Characters considered excluded from URI use and should be escaped.
  // "#" when not used to delimit the start of the fragment identifier.
  // "%" when not used to escape a character.
  //
  //   "<" | ">" | "#" | "%" | <"> | "{" | "}" | "|" | "\" | "^" | "[" | "]" |
  //   "`"

  // Pairs of URLs of the form (un-encoded, expected encoded result) to verify.
  nsTArray<std::pair<nsCString, nsCString>> pairs{
      {// '#' in ref
       "https://chat.mozilla.org/#/room/#macdev:mozilla.org"_ns,
       "https://chat.mozilla.org/#/room/%23macdev:mozilla.org"_ns},
      {
          // '"' in ref
          "https://example.com/path#ref_with_#_and\""_ns,
          "https://example.com/path#ref_with_%23_and%22"_ns,
      },
      {
          // '[]{}|' in ref
          "https://example.com/path#ref_with_[foo]_{and}_|"_ns,
          "https://example.com/path#ref_with_%5Bfoo%5D_%7Band%7D_%7C"_ns,
      },
      {
          // Unreserved characters in path, query, and ref
          "https://example.com/path-_.!~&'(x)?x=y&x=z-_.!~&'(x)#ref-_.!~&'(x)"_ns,
          "https://example.com/path-_.!~&'(x)?x=y&x=z-_.!~&%27(x)#ref-_.!~&'(x)"_ns,
      },
      {
          // All excluded characters in the ref.
          "https://example.com/path#ref \"#<>[]\\^`{|}ref"_ns,
          "https://example.com/path#ref%20%22%23%3C%3E%5B%5D%5C%5E%60%7B%7C%7Dref"_ns,
      },
      /*
       * Bug 1739533:
       * This test fails because the '%' character needs to be escaped before
       * the URI is passed to [NSURL URLWithString]. '<' brackets are
       * already escaped by the browser to their "%xx" form so the encoding must
       * be added in a way that ignores characters already % encoded "%xx".
      {
        // Unreserved characters in path, query, and ref
        // https://example.com/path/with<more>/%/and"#frag_with_#_and"
        "https://example.com/path/with<more>/%/and\"#frag_with_#_and\""_ns,
        "https://example.com/path/with%3Cmore%3E/%25/and%22#frag_with_%23_and%22"_ns,
      },
      */
  };

  for (std::pair<nsCString, nsCString>& pair : pairs) {
    nsCString escaped;
    nsresult rv = NS_GetSpecWithNSURLEncoding(escaped, pair.first);
    EXPECT_EQ(rv, NS_OK);
    EXPECT_STREQ(pair.second.BeginReading(), escaped.BeginReading());
    ExpectUnchangedByNSURL(escaped);
  }

  // A list of URLs that should not be changed by encoding.
  nsTArray<nsCString> unchangedURLs{
      // '=' In the query
      "https://bugzilla.mozilla.org/show_bug.cgi?id=1737854"_ns,
      "https://bugzilla.mozilla.org/show_bug.cgi?id=1737854#ref"_ns,
      "https://bugzilla.mozilla.org/allinref#show_bug.cgi?id=1737854ref"_ns,
      "https://example.com/script?foo=bar#this_ref"_ns,
      // Escaped character in the ref
      "https://html.spec.whatwg.org/multipage/dom.html#the-document%27s-address"_ns,
      // Misc query
      "https://www.google.com/search?q=firefox+web+browser&client=firefox-b-1-d&ei=abc&ved=abc&abc=5&oq=firefox+web+browser&gs_lcp=abc&sclient=gws-wiz"_ns,
      // Check for double encoding. % encoded octals should not be re-encoded.
      "https://chat.mozilla.org/#/room/%23macdev%3Amozilla.org"_ns,
      "https://searchfox.org/mozilla-central/search?q=symbol%3AE_%3CT_mozilla%3A%3AWebGLExtensionID%3E_EXT_color_buffer_half_float&path="_ns,
      // Unreserved and reserved that don't need encoding in ref.
      "https://example.com/path#ref!$&'(foo),:;=?@~"_ns,
      // Unreserved and reserved that don't need encoding in path.
      "https://example.com/path-_.!~&'(x)#ref"_ns,
      // Unreserved and reserved that don't need encoding in path and ref.
      "https://example.com/path-_.!~&'(x)#ref-_.!~&'(x)"_ns,
      // Reserved in query.
      "https://example.com/path?a=b&;=/&/=?&@=a+b,$"_ns,
  };

  for (nsCString& toEscape : unchangedURLs) {
    nsCString escaped;
    nsresult rv = NS_GetSpecWithNSURLEncoding(escaped, toEscape);
    EXPECT_EQ(rv, NS_OK);
    EXPECT_STREQ(toEscape.BeginReading(), escaped.BeginReading());
    ExpectUnchangedByNSURL(escaped);
  }
}
