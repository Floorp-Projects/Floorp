/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2014 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "pkixcheck.h"
#include "pkixder.h"
#include "pkixgtest.h"
#include "pkixutil.h"

namespace mozilla { namespace pkix {

Result MatchPresentedDNSIDWithReferenceDNSID(Input presentedDNSID,
                                             Input referenceDNSID,
                                             /*out*/ bool& matches);

bool IsValidReferenceDNSID(Input hostname);
bool IsValidPresentedDNSID(Input hostname);
bool ParseIPv4Address(Input hostname, /*out*/ uint8_t (&out)[4]);
bool ParseIPv6Address(Input hostname, /*out*/ uint8_t (&out)[16]);

} } // namespace mozilla::pkix

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

struct PresentedMatchesReference
{
  ByteString presentedDNSID;
  ByteString referenceDNSID;
  Result expectedResult;
  bool expectedMatches; // only valid when expectedResult == Success
};

#define DNS_ID_MATCH(a, b) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(a), sizeof(a) - 1), \
    ByteString(reinterpret_cast<const uint8_t*>(b), sizeof(b) - 1), \
    Success, \
    true \
  }

#define DNS_ID_MISMATCH(a, b) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(a), sizeof(a) - 1), \
    ByteString(reinterpret_cast<const uint8_t*>(b), sizeof(b) - 1), \
    Success, \
    false \
  }

#define DNS_ID_BAD_DER(a, b) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(a), sizeof(a) - 1), \
    ByteString(reinterpret_cast<const uint8_t*>(b), sizeof(b) - 1), \
    Result::ERROR_BAD_DER, \
    false \
  }

static const PresentedMatchesReference DNSID_MATCH_PARAMS[] =
{
  DNS_ID_BAD_DER("", "a"),

  DNS_ID_MATCH("a", "a"),
  DNS_ID_MISMATCH("b", "a"),

  DNS_ID_MATCH("*.b.a", "c.b.a"),
  DNS_ID_MISMATCH("*.b.a", "b.a"),
  DNS_ID_MISMATCH("*.b.a", "b.a."),

  // We allow underscores for compatibility with existing practices.
  DNS_ID_MATCH("a_b", "a_b"),
  DNS_ID_MATCH("*.example.com", "uses_underscore.example.com"),
  DNS_ID_MATCH("*.uses_underscore.example.com", "a.uses_underscore.example.com"),

  // See bug 1139039
  DNS_ID_MATCH("_.example.com", "_.example.com"),
  DNS_ID_MATCH("*.example.com", "_.example.com"),
  DNS_ID_MATCH("_", "_"),
  DNS_ID_MATCH("___", "___"),
  DNS_ID_MATCH("example_", "example_"),
  DNS_ID_MATCH("_example", "_example"),
  DNS_ID_MATCH("*._._", "x._._"),

  // See bug 1139039
  // A DNS-ID must not end in an all-numeric label. We don't consider
  // underscores to be numeric.
  DNS_ID_MATCH("_1", "_1"),
  DNS_ID_MATCH("example._1", "example._1"),
  DNS_ID_MATCH("example.1_", "example.1_"),

  // Wildcard not in leftmost label
  DNS_ID_MATCH("d.c.b.a", "d.c.b.a"),
  DNS_ID_BAD_DER("d.*.b.a", "d.c.b.a"),
  DNS_ID_BAD_DER("d.c*.b.a", "d.c.b.a"),
  DNS_ID_BAD_DER("d.c*.b.a", "d.cc.b.a"),

  // case sensitivity
  DNS_ID_MATCH("abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
  DNS_ID_MATCH("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"),
  DNS_ID_MATCH("aBc", "Abc"),

  // digits
  DNS_ID_MATCH("a1", "a1"),

  // A trailing dot indicates an absolute name. Absolute presented names are
  // not allowed, but absolute reference names are allowed.
  DNS_ID_MATCH("example", "example"),
  DNS_ID_BAD_DER("example.", "example."),
  DNS_ID_MATCH("example", "example."),
  DNS_ID_BAD_DER("example.", "example"),
  DNS_ID_MATCH("example.com", "example.com"),
  DNS_ID_BAD_DER("example.com.", "example.com."),
  DNS_ID_MATCH("example.com", "example.com."),
  DNS_ID_BAD_DER("example.com.", "example.com"),
  DNS_ID_BAD_DER("example.com..", "example.com."),
  DNS_ID_BAD_DER("example.com..", "example.com"),
  DNS_ID_BAD_DER("example.com...", "example.com."),

  // xn-- IDN prefix
  DNS_ID_BAD_DER("x*.b.a", "xa.b.a"),
  DNS_ID_BAD_DER("x*.b.a", "xna.b.a"),
  DNS_ID_BAD_DER("x*.b.a", "xn-a.b.a"),
  DNS_ID_BAD_DER("x*.b.a", "xn--a.b.a"),
  DNS_ID_BAD_DER("xn*.b.a", "xn--a.b.a"),
  DNS_ID_BAD_DER("xn-*.b.a", "xn--a.b.a"),
  DNS_ID_BAD_DER("xn--*.b.a", "xn--a.b.a"),
  DNS_ID_BAD_DER("xn*.b.a", "xn--a.b.a"),
  DNS_ID_BAD_DER("xn-*.b.a", "xn--a.b.a"),
  DNS_ID_BAD_DER("xn--*.b.a", "xn--a.b.a"),
  DNS_ID_BAD_DER("xn---*.b.a", "xn--a.b.a"),

  // "*" cannot expand to nothing.
  DNS_ID_BAD_DER("c*.b.a", "c.b.a"),

  /////////////////////////////////////////////////////////////////////////////
  // These are test cases adapted from Chromium's x509_certificate_unittest.cc.
  // The parameter order is the opposite in Chromium's tests. Also, some tests
  // were modified to fit into this framework or due to intentional differences
  // between mozilla::pkix and Chromium.

  DNS_ID_MATCH("foo.com", "foo.com"),
  DNS_ID_MATCH("f", "f"),
  DNS_ID_MISMATCH("i", "h"),
  DNS_ID_MATCH("*.foo.com", "bar.foo.com"),
  DNS_ID_MATCH("*.test.fr", "www.test.fr"),
  DNS_ID_MATCH("*.test.FR", "wwW.tESt.fr"),
  DNS_ID_BAD_DER(".uk", "f.uk"),
  DNS_ID_BAD_DER("?.bar.foo.com", "w.bar.foo.com"),
  DNS_ID_BAD_DER("(www|ftp).foo.com", "www.foo.com"), // regex!
  DNS_ID_BAD_DER("www.foo.com\0", "www.foo.com"),
  DNS_ID_BAD_DER("www.foo.com\0*.foo.com", "www.foo.com"),
  DNS_ID_MISMATCH("ww.house.example", "www.house.example"),
  DNS_ID_MISMATCH("www.test.org", "test.org"),
  DNS_ID_MISMATCH("*.test.org", "test.org"),
  DNS_ID_BAD_DER("*.org", "test.org"),
  DNS_ID_BAD_DER("w*.bar.foo.com", "w.bar.foo.com"),
  DNS_ID_BAD_DER("ww*ww.bar.foo.com", "www.bar.foo.com"),
  DNS_ID_BAD_DER("ww*ww.bar.foo.com", "wwww.bar.foo.com"),

  // Different than Chromium, matches NSS.
  DNS_ID_BAD_DER("w*w.bar.foo.com", "wwww.bar.foo.com"),

  DNS_ID_BAD_DER("w*w.bar.foo.c0m", "wwww.bar.foo.com"),

  // '*' must be the only character in the wildcard label
  DNS_ID_BAD_DER("wa*.bar.foo.com", "WALLY.bar.foo.com"),

  // We require "*" to be the last character in a wildcard label, but
  // Chromium does not.
  DNS_ID_BAD_DER("*Ly.bar.foo.com", "wally.bar.foo.com"),

  // Chromium does URL decoding of the reference ID, but we don't, and we also
  // require that the reference ID is valid, so we can't test these two.
  // DNS_ID_MATCH("www.foo.com", "ww%57.foo.com"),
  // DNS_ID_MATCH("www&.foo.com", "www%26.foo.com"),

  DNS_ID_MISMATCH("*.test.de", "www.test.co.jp"),
  DNS_ID_BAD_DER("*.jp", "www.test.co.jp"),
  DNS_ID_MISMATCH("www.test.co.uk", "www.test.co.jp"),
  DNS_ID_BAD_DER("www.*.co.jp", "www.test.co.jp"),
  DNS_ID_MATCH("www.bar.foo.com", "www.bar.foo.com"),
  DNS_ID_MISMATCH("*.foo.com", "www.bar.foo.com"),
  DNS_ID_BAD_DER("*.*.foo.com", "www.bar.foo.com"),
  DNS_ID_BAD_DER("*.*.foo.com", "www.bar.foo.com"),

  // Our matcher requires the reference ID to be a valid DNS name, so we cannot
  // test this case.
  //DNS_ID_BAD_DER("*.*.bar.foo.com", "*..bar.foo.com"),

  DNS_ID_MATCH("www.bath.org", "www.bath.org"),

  // Our matcher requires the reference ID to be a valid DNS name, so we cannot
  // test these cases.
  // DNS_ID_BAD_DER("www.bath.org", ""),
  // DNS_ID_BAD_DER("www.bath.org", "20.30.40.50"),
  // DNS_ID_BAD_DER("www.bath.org", "66.77.88.99"),

  // IDN tests
  DNS_ID_MATCH("xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_MATCH("*.xn--poema-9qae5a.com.br", "www.xn--poema-9qae5a.com.br"),
  DNS_ID_MISMATCH("*.xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_BAD_DER("xn--poema-*.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_BAD_DER("xn--*-9qae5a.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_BAD_DER("*--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br"),

  // The following are adapted from the examples quoted from
  //   http://tools.ietf.org/html/rfc6125#section-6.4.3
  // (e.g., *.example.com would match foo.example.com but
  // not bar.foo.example.com or example.com).
  DNS_ID_MATCH("*.example.com", "foo.example.com"),
  DNS_ID_MISMATCH("*.example.com", "bar.foo.example.com"),
  DNS_ID_MISMATCH("*.example.com", "example.com"),
  // (e.g., baz*.example.net and *baz.example.net and b*z.example.net would
  // be taken to match baz1.example.net and foobaz.example.net and
  // buzz.example.net, respectively. However, we don't allow any characters
  // other than '*' in the wildcard label.
  DNS_ID_BAD_DER("baz*.example.net", "baz1.example.net"),

  // Both of these are different from Chromium, but match NSS, becaues the
  // wildcard character "*" is not the last character of the label.
  DNS_ID_BAD_DER("*baz.example.net", "foobaz.example.net"),
  DNS_ID_BAD_DER("b*z.example.net", "buzz.example.net"),

  // Wildcards should not be valid for public registry controlled domains,
  // and unknown/unrecognized domains, at least three domain components must
  // be present. For mozilla::pkix and NSS, there must always be at least two
  // labels after the wildcard label.
  DNS_ID_MATCH("*.test.example", "www.test.example"),
  DNS_ID_MATCH("*.example.co.uk", "test.example.co.uk"),
  DNS_ID_BAD_DER("*.exmaple", "test.example"),

  // The result is different than Chromium, because Chromium takes into account
  // the additional knowledge it has that "co.uk" is a TLD. mozilla::pkix does
  // not know that.
  DNS_ID_MATCH("*.co.uk", "example.co.uk"),

  DNS_ID_BAD_DER("*.com", "foo.com"),
  DNS_ID_BAD_DER("*.us", "foo.us"),
  DNS_ID_BAD_DER("*", "foo"),

  // IDN variants of wildcards and registry controlled domains.
  DNS_ID_MATCH("*.xn--poema-9qae5a.com.br", "www.xn--poema-9qae5a.com.br"),
  DNS_ID_MATCH("*.example.xn--mgbaam7a8h", "test.example.xn--mgbaam7a8h"),

  // RFC6126 allows this, and NSS accepts it, but Chromium disallows it.
  // TODO: File bug against Chromium.
  DNS_ID_MATCH("*.com.br", "xn--poema-9qae5a.com.br"),

  DNS_ID_BAD_DER("*.xn--mgbaam7a8h", "example.xn--mgbaam7a8h"),
  // Wildcards should be permissible for 'private' registry-controlled
  // domains. (In mozilla::pkix, we do not know if it is a private registry-
  // controlled domain or not.)
  DNS_ID_MATCH("*.appspot.com", "www.appspot.com"),
  DNS_ID_MATCH("*.s3.amazonaws.com", "foo.s3.amazonaws.com"),

  // Multiple wildcards are not valid.
  DNS_ID_BAD_DER("*.*.com", "foo.example.com"),
  DNS_ID_BAD_DER("*.bar.*.com", "foo.bar.example.com"),

  // Absolute vs relative DNS name tests. Although not explicitly specified
  // in RFC 6125, absolute reference names (those ending in a .) should
  // match either absolute or relative presented names. We don't allow
  // absolute presented names.
  // TODO: File errata against RFC 6125 about this.
  DNS_ID_BAD_DER("foo.com.", "foo.com"),
  DNS_ID_MATCH("foo.com", "foo.com."),
  DNS_ID_BAD_DER("foo.com.", "foo.com."),
  DNS_ID_BAD_DER("f.", "f"),
  DNS_ID_MATCH("f", "f."),
  DNS_ID_BAD_DER("f.", "f."),
  DNS_ID_BAD_DER("*.bar.foo.com.", "www-3.bar.foo.com"),
  DNS_ID_MATCH("*.bar.foo.com", "www-3.bar.foo.com."),
  DNS_ID_BAD_DER("*.bar.foo.com.", "www-3.bar.foo.com."),

  // We require the reference ID to be a valid DNS name, so we cannot test this
  // case.
  // DNS_ID_MISMATCH(".", "."),

  DNS_ID_BAD_DER("*.com.", "example.com"),
  DNS_ID_BAD_DER("*.com", "example.com."),
  DNS_ID_BAD_DER("*.com.", "example.com."),
  DNS_ID_BAD_DER("*.", "foo."),
  DNS_ID_BAD_DER("*.", "foo"),

  // The result is different than Chromium because we don't know that co.uk is
  // a TLD.
  DNS_ID_MATCH("*.co.uk", "foo.co.uk"),
  DNS_ID_MATCH("*.co.uk", "foo.co.uk."),
  DNS_ID_BAD_DER("*.co.uk.", "foo.co.uk"),
  DNS_ID_BAD_DER("*.co.uk.", "foo.co.uk."),

  DNS_ID_MISMATCH("*.example.com", "localhost"),
  DNS_ID_MISMATCH("*.example.com", "localhost."),
  // Note that we already have the testcase DNS_ID_BAD_DER("*", "foo") above
};

struct InputValidity
{
  ByteString input;
  bool isValidReferenceID;
  bool isValidPresentedID;
};

// str is null-terminated, which is why we subtract 1. str may contain embedded
// nulls (including at the end) preceding the null terminator though.
#define I(str, validReferenceID, validPresentedID) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(str), sizeof(str) - 1), \
    validReferenceID, \
    validPresentedID, \
  }

static const InputValidity DNSNAMES_VALIDITY[] =
{
  I("a", true, true),
  I("a.b", true, true),
  I("a.b.c", true, true),
  I("a.b.c.d", true, true),

  // empty labels
  I("", false, false),
  I(".", false, false),
  I("a", true, true),
  I(".a", false, false),
  I(".a.b", false, false),
  I("..a", false, false),
  I("a..b", false, false),
  I("a...b", false, false),
  I("a..b.c", false, false),
  I("a.b..c", false, false),
  I(".a.b.c.", false, false),

  // absolute names (only allowed for reference names)
  I("a.", true, false),
  I("a.b.", true, false),
  I("a.b.c.", true, false),

  // absolute names with empty label at end
  I("a..", false, false),
  I("a.b..", false, false),
  I("a.b.c..", false, false),
  I("a...", false, false),

  // Punycode
  I("xn--", false, false),
  I("xn--.", false, false),
  I("xn--.a", false, false),
  I("a.xn--", false, false),
  I("a.xn--.", false, false),
  I("a.xn--.b", false, false),
  I("a.xn--.b", false, false),
  I("a.xn--\0.b", false, false),
  I("a.xn--a.b", true, true),
  I("xn--a", true, true),
  I("a.xn--a", true, true),
  I("a.xn--a.a", true, true),
  I("\xc4\x95.com", false, false), // UTF-8 ĕ
  I("xn--jea.com", true, true), // punycode ĕ
  I("xn--\xc4\x95.com", false, false), // UTF-8 ĕ, malformed punycode + UTF-8 mashup

  // Surprising punycode
  I("xn--google.com", true, true), // 䕮䕵䕶䕱.com
  I("xn--citibank.com", true, true), // 岍岊岊岅岉岎.com
  I("xn--cnn.com", true, true), // 䁾.com
  I("a.xn--cnn", true, true), // a.䁾
  I("a.xn--cnn.com", true, true), // a.䁾.com

  I("1.2.3.4", false, false), // IPv4 address
  I("1::2", false, false), // IPV6 address

  // whitespace not allowed anywhere.
  I(" ", false, false),
  I(" a", false, false),
  I("a ", false, false),
  I("a b", false, false),
  I("a.b 1", false, false),
  I("a\t", false, false),

  // Nulls not allowed
  I("\0", false, false),
  I("a\0", false, false),
  I("example.org\0.example.com", false, false), // Hi Moxie!
  I("\0a", false, false),
  I("xn--\0", false, false),

  // Allowed character set
  I("a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z", true, true),
  I("A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.V.W.X.Y.Z", true, true),
  I("0.1.2.3.4.5.6.7.8.9.a", true, true), // "a" needed to avoid numeric last label
  I("a-b", true, true), // hyphen (a label cannot start or end with a hyphen)

  // Underscores
  I("a_b", true, true),
  // See bug 1139039
  I("_", true, true),
  I("a_", true, true),
  I("_a", true, true),
  I("_1", true, true),
  I("1_", true, true),
  I("___", true, true),

  // An invalid character in various positions
  I("!", false, false),
  I("!a", false, false),
  I("a!", false, false),
  I("a!b", false, false),
  I("a.!", false, false),
  I("a.a!", false, false),
  I("a.!a", false, false),
  I("a.a!a", false, false),
  I("a.!a.a", false, false),
  I("a.a!.a", false, false),
  I("a.a!a.a", false, false),

  // Various other invalid characters
  I("a!", false, false),
  I("a@", false, false),
  I("a#", false, false),
  I("a$", false, false),
  I("a%", false, false),
  I("a^", false, false),
  I("a&", false, false),
  I("a*", false, false),
  I("a(", false, false),
  I("a)", false, false),

  // last label can't be fully numeric
  I("1", false, false),
  I("a.1", false, false),

  // other labels can be fully numeric
  I("1.a", true, true),
  I("1.2.a", true, true),
  I("1.2.3.a", true, true),

  // last label can be *partly* numeric
  I("1a", true, true),
  I("1.1a", true, true),
  I("1-1", true, true),
  I("a.1-1", true, true),
  I("a.1-a", true, true),

  // labels cannot start with a hyphen
  I("-", false, false),
  I("-1", false, false),

  // labels cannot end with a hyphen
  I("1-", false, false),
  I("1-.a", false, false),
  I("a-", false, false),
  I("a-.a", false, false),
  I("a.1-.a", false, false),
  I("a.a-.a", false, false),

  // labels can contain a hyphen in the middle
  I("a-b", true, true),
  I("1-2", true, true),
  I("a.a-1", true, true),

  // multiple consecutive hyphens allowed
  I("a--1", true, true),
  I("1---a", true, true),
  I("a-----------------b", true, true),

  // Wildcard specifications are not valid reference names, but are valid
  // presented names if there are enough labels and if '*' is the only
  // character in the wildcard label.
  I("*.a", false, false),
  I("a*", false, false),
  I("a*.", false, false),
  I("a*.a", false, false),
  I("a*.a.", false, false),
  I("*.a.b", false, true),
  I("*.a.b.", false, false),
  I("a*.b.c", false, false),
  I("*.a.b.c", false, true),
  I("a*.b.c.d", false, false),

  // Multiple wildcards are not allowed.
  I("a**.b.c", false, false),
  I("a*b*.c.d", false, false),
  I("a*.b*.c", false, false),

  // Wildcards are only allowed in the first label.
  I("a.*", false, false),
  I("a.*.b", false, false),
  I("a.b.*", false, false),
  I("a.b*.c", false, false),
  I("*.b*.c", false, false),
  I(".*.a.b", false, false),
  I(".a*.b.c", false, false),

  // Wildcards must be at the *end* of the first label.
  I("*a.b.c", false, false),
  I("a*b.c.d", false, false),

  // Wildcards not allowed with IDNA prefix
  I("x*.a.b", false, false),
  I("xn*.a.b", false, false),
  I("xn-*.a.b", false, false),
  I("xn--*.a.b", false, false),
  I("xn--w*.a.b", false, false),

  // Redacted labels from RFC6962bis draft 4
  // https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-04#section-3.2.2
  I("(PRIVATE).foo", false, false),

  // maximum label length is 63 characters
  I("1234567890" "1234567890" "1234567890"
    "1234567890" "1234567890" "1234567890" "abc", true, true),
  I("1234567890" "1234567890" "1234567890"
    "1234567890" "1234567890" "1234567890" "abcd", false, false),

  // maximum total length is 253 characters
  I("1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "12345678" "a",
    true, true),
  I("1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "123456789" "a",
    false, false),
};

static const InputValidity DNSNAMES_VALIDITY_TURKISH_I[] =
{
  // http://en.wikipedia.org/wiki/Dotted_and_dotless_I#In_computing
  // IDN registration rules disallow "latin capital letter i with dot above,"
  // but our checks aren't intended to enforce those rules.
  I("I", true, true), // ASCII capital I
  I("i", true, true), // ASCII lowercase i
  I("\xC4\xB0", false, false), // latin capital letter i with dot above
  I("\xC4\xB1", false, false), // latin small letter dotless i
  I("xn--i-9bb", true, true), // latin capital letter i with dot above, in punycode
  I("xn--cfa", true, true), // latin small letter dotless i, in punycode
  I("xn--\xC4\xB0", false, false), // latin capital letter i with dot above, mashup
  I("xn--\xC4\xB1", false, false), // latin small letter dotless i, mashup
};

static const uint8_t LOWERCASE_I_VALUE[1] = { 'i' };
static const uint8_t UPPERCASE_I_VALUE[1] = { 'I' };
static const Input LOWERCASE_I(LOWERCASE_I_VALUE);
static const Input UPPERCASE_I(UPPERCASE_I_VALUE);

template <unsigned int L>
struct IPAddressParams
{
  ByteString input;
  bool isValid;
  uint8_t expectedValueIfValid[L];
};

#define IPV4_VALID(str, a, b, c, d) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(str), sizeof(str) - 1), \
    true, \
    { a, b, c, d } \
  }

// The value of expectedValueIfValid must be ignored for invalid IP addresses.
// The value { 73, 73, 73, 73 } is used because it is unlikely to result in an
// accidental match, unlike { 0, 0, 0, 0 }, which is a value we actually test.
#define IPV4_INVALID(str) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(str), sizeof(str) - 1), \
    false, \
    { 73, 73, 73, 73 } \
  }

static const IPAddressParams<4> IPV4_ADDRESSES[] =
{
  IPV4_INVALID(""),
  IPV4_INVALID("1"),
  IPV4_INVALID("1.2"),
  IPV4_INVALID("1.2.3"),
  IPV4_VALID("1.2.3.4", 1, 2, 3, 4),
  IPV4_INVALID("1.2.3.4.5"),

  IPV4_INVALID("1.2.3.4a"), // a DNSName!
  IPV4_INVALID("a.2.3.4"), // not even a DNSName!
  IPV4_INVALID("1::2"), // IPv6 address

  // Whitespace not allowed
  IPV4_INVALID(" 1.2.3.4"),
  IPV4_INVALID("1.2.3.4 "),
  IPV4_INVALID("1 .2.3.4"),
  IPV4_INVALID("\n1.2.3.4"),
  IPV4_INVALID("1.2.3.4\n"),

  // Nulls not allowed
  IPV4_INVALID("\0"),
  IPV4_INVALID("\0" "1.2.3.4"),
  IPV4_INVALID("1.2.3.4\0"),
  IPV4_INVALID("1.2.3.4\0.5"),

  // Range
  IPV4_VALID("0.0.0.0", 0, 0, 0, 0),
  IPV4_VALID("255.255.255.255", 255, 255, 255, 255),
  IPV4_INVALID("256.0.0.0"),
  IPV4_INVALID("0.256.0.0"),
  IPV4_INVALID("0.0.256.0"),
  IPV4_INVALID("0.0.0.256"),
  IPV4_INVALID("999.0.0.0"),
  IPV4_INVALID("9999999999999999999.0.0.0"),

  // All digits allowed
  IPV4_VALID("0.1.2.3", 0, 1, 2, 3),
  IPV4_VALID("4.5.6.7", 4, 5, 6, 7),
  IPV4_VALID("8.9.0.1", 8, 9, 0, 1),

  // Leading zeros not allowed
  IPV4_INVALID("01.2.3.4"),
  IPV4_INVALID("001.2.3.4"),
  IPV4_INVALID("00000000001.2.3.4"),
  IPV4_INVALID("010.2.3.4"),
  IPV4_INVALID("1.02.3.4"),
  IPV4_INVALID("1.2.03.4"),
  IPV4_INVALID("1.2.3.04"),

  // Empty components
  IPV4_INVALID(".2.3.4"),
  IPV4_INVALID("1..3.4"),
  IPV4_INVALID("1.2..4"),
  IPV4_INVALID("1.2.3."),

  // Too many components
  IPV4_INVALID("1.2.3.4.5"),
  IPV4_INVALID("1.2.3.4.5.6"),
  IPV4_INVALID("0.1.2.3.4"),
  IPV4_INVALID("1.2.3.4.0"),

  // Leading/trailing dot
  IPV4_INVALID(".1.2.3.4"),
  IPV4_INVALID("1.2.3.4."),

  // Other common forms of IPv4 address
  // http://en.wikipedia.org/wiki/IPv4#Address_representations
  IPV4_VALID("192.0.2.235", 192, 0, 2, 235), // dotted decimal (control value)
  IPV4_INVALID("0xC0.0x00.0x02.0xEB"), // dotted hex
  IPV4_INVALID("0301.0000.0002.0353"), // dotted octal
  IPV4_INVALID("0xC00002EB"), // non-dotted hex
  IPV4_INVALID("3221226219"), // non-dotted decimal
  IPV4_INVALID("030000001353"), // non-dotted octal
  IPV4_INVALID("192.0.0002.0xEB"), // mixed
};

#define IPV6_VALID(str, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(str), sizeof(str) - 1), \
    true, \
    { a, b, c, d, \
      e, f, g, h, \
      i, j, k, l, \
      m, n, o, p } \
  }

#define IPV6_INVALID(str) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(str), sizeof(str) - 1), \
    false, \
    { 73, 73, 73, 73, \
      73, 73, 73, 73, \
      73, 73, 73, 73, \
      73, 73, 73, 73 } \
  }

static const IPAddressParams<16> IPV6_ADDRESSES[] =
{
  IPV6_INVALID(""),
  IPV6_INVALID("1234"),
  IPV6_INVALID("1234:5678"),
  IPV6_INVALID("1234:5678:9abc"),
  IPV6_INVALID("1234:5678:9abc:def0"),
  IPV6_INVALID("1234:5678:9abc:def0:1234:"),
  IPV6_INVALID("1234:5678:9abc:def0:1234:5678:"),
  IPV6_INVALID("1234:5678:9abc:def0:1234:5678:9abc:"),
  IPV6_VALID("1234:5678:9abc:def0:1234:5678:9abc:def0",
             0x12, 0x34, 0x56, 0x78,
             0x9a, 0xbc, 0xde, 0xf0,
             0x12, 0x34, 0x56, 0x78,
             0x9a, 0xbc, 0xde, 0xf0),
  IPV6_INVALID("1234:5678:9abc:def0:1234:5678:9abc:def0:"),
  IPV6_INVALID(":1234:5678:9abc:def0:1234:5678:9abc:def0"),
  IPV6_INVALID("1234:5678:9abc:def0:1234:5678:9abc:def0:0000"),

  // Valid contractions
  IPV6_VALID("::1",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x01),
  IPV6_VALID("::1234",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x12, 0x34),
  IPV6_VALID("1234::",
             0x12, 0x34, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00),
  IPV6_VALID("1234::5678",
             0x12, 0x34, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x56, 0x78),
  IPV6_VALID("1234:5678::abcd",
             0x12, 0x34, 0x56, 0x78,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0xab, 0xcd),
  IPV6_VALID("1234:5678:9abc:def0:1234:5678:9abc::",
             0x12, 0x34, 0x56, 0x78,
             0x9a, 0xbc, 0xde, 0xf0,
             0x12, 0x34, 0x56, 0x78,
             0x9a, 0xbc, 0x00, 0x00),

  // Contraction in full IPv6 addresses not allowed
  IPV6_INVALID("::1234:5678:9abc:def0:1234:5678:9abc:def0"), // start
  IPV6_INVALID("1234:5678:9abc:def0:1234:5678:9abc:def0::"), // end
  IPV6_INVALID("1234:5678::9abc:def0:1234:5678:9abc:def0"), // interior

  // Multiple contractions not allowed
  IPV6_INVALID("::1::"),
  IPV6_INVALID("::1::2"),
  IPV6_INVALID("1::2::"),

  // Colon madness!
  IPV6_INVALID(":"),
  IPV6_INVALID("::"),
  IPV6_INVALID(":::"),
  IPV6_INVALID("::::"),
  IPV6_INVALID(":::1"),
  IPV6_INVALID("::::1"),
  IPV6_INVALID("1:::2"),
  IPV6_INVALID("1::::2"),
  IPV6_INVALID("1:2:::"),
  IPV6_INVALID("1:2::::"),
  IPV6_INVALID("::1234:"),
  IPV6_INVALID(":1234::"),

  IPV6_INVALID("01234::"), // too many digits, even if zero
  IPV6_INVALID("12345678::"), // too many digits or missing colon

  // uppercase
  IPV6_VALID("ABCD:EFAB::",
             0xab, 0xcd, 0xef, 0xab,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00),

  // miXeD CAse
  IPV6_VALID("aBcd:eFAb::",
             0xab, 0xcd, 0xef, 0xab,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00),

  // IPv4-style
  IPV6_VALID("::2.3.4.5",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x02, 0x03, 0x04, 0x05),
  IPV6_VALID("1234::2.3.4.5",
             0x12, 0x34, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x02, 0x03, 0x04, 0x05),
  IPV6_VALID("::abcd:2.3.4.5",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0xab, 0xcd,
             0x02, 0x03, 0x04, 0x05),
  IPV6_VALID("1234:5678:9abc:def0:1234:5678:252.253.254.255",
             0x12, 0x34, 0x56, 0x78,
             0x9a, 0xbc, 0xde, 0xf0,
             0x12, 0x34, 0x56, 0x78,
             252,  253,  254,  255),
  IPV6_VALID("1234:5678:9abc:def0:1234::252.253.254.255",
             0x12, 0x34, 0x56, 0x78,
             0x9a, 0xbc, 0xde, 0xf0,
             0x12, 0x34, 0x00, 0x00,
             252,  253,  254,  255),
  IPV6_INVALID("1234::252.253.254"),
  IPV6_INVALID("::252.253.254"),
  IPV6_INVALID("::252.253.254.300"),
  IPV6_INVALID("1234::252.253.254.255:"),
  IPV6_INVALID("1234::252.253.254.255:5678"),

  // Contractions that don't contract
  IPV6_INVALID("::1234:5678:9abc:def0:1234:5678:9abc:def0"),
  IPV6_INVALID("1234:5678:9abc:def0:1234:5678:9abc:def0::"),
  IPV6_INVALID("1234:5678:9abc:def0::1234:5678:9abc:def0"),
  IPV6_INVALID("1234:5678:9abc:def0:1234:5678::252.253.254.255"),

  // With and without leading zeros
  IPV6_VALID("::123",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x01, 0x23),
  IPV6_VALID("::0123",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x01, 0x23),
  IPV6_VALID("::012",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x12),
  IPV6_VALID("::0012",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x12),
  IPV6_VALID("::01",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x01),
  IPV6_VALID("::001",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x01),
  IPV6_VALID("::0001",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x01),
  IPV6_VALID("::0",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00),
  IPV6_VALID("::00",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00),
  IPV6_VALID("::000",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00),
  IPV6_VALID("::0000",
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00),
  IPV6_INVALID("::01234"),
  IPV6_INVALID("::00123"),
  IPV6_INVALID("::000123"),

  // Trailing zero
  IPV6_INVALID("::12340"),

  // Whitespace
  IPV6_INVALID(" 1234:5678:9abc:def0:1234:5678:9abc:def0"),
  IPV6_INVALID("\t1234:5678:9abc:def0:1234:5678:9abc:def0"),
  IPV6_INVALID("\t1234:5678:9abc:def0:1234:5678:9abc:def0\n"),
  IPV6_INVALID("1234 :5678:9abc:def0:1234:5678:9abc:def0"),
  IPV6_INVALID("1234: 5678:9abc:def0:1234:5678:9abc:def0"),
  IPV6_INVALID(":: 2.3.4.5"),
  IPV6_INVALID("1234::252.253.254.255 "),
  IPV6_INVALID("1234::252.253.254.255\n"),
  IPV6_INVALID("1234::252.253. 254.255"),

  // Nulls
  IPV6_INVALID("\0"),
  IPV6_INVALID("::1\0:2"),
  IPV6_INVALID("::1\0"),
  IPV6_INVALID("::1.2.3.4\0"),
  IPV6_INVALID("::1.2\02.3.4"),
};

class pkixnames_MatchPresentedDNSIDWithReferenceDNSID
  : public ::testing::Test
  , public ::testing::WithParamInterface<PresentedMatchesReference>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_MatchPresentedDNSIDWithReferenceDNSID,
       MatchPresentedDNSIDWithReferenceDNSID)
{
  const PresentedMatchesReference& param(GetParam());
  SCOPED_TRACE(param.presentedDNSID.c_str());
  SCOPED_TRACE(param.referenceDNSID.c_str());
  Input presented;
  ASSERT_EQ(Success, presented.Init(param.presentedDNSID.data(),
                                    param.presentedDNSID.length()));
  Input reference;
  ASSERT_EQ(Success, reference.Init(param.referenceDNSID.data(),
                                    param.referenceDNSID.length()));

  // sanity check that test makes sense
  ASSERT_TRUE(IsValidReferenceDNSID(reference));

  bool matches;
  ASSERT_EQ(param.expectedResult,
            MatchPresentedDNSIDWithReferenceDNSID(presented, reference,
                                                  matches));
  if (param.expectedResult == Success) {
    ASSERT_EQ(param.expectedMatches, matches);
  }
}

INSTANTIATE_TEST_CASE_P(pkixnames_MatchPresentedDNSIDWithReferenceDNSID,
                        pkixnames_MatchPresentedDNSIDWithReferenceDNSID,
                        testing::ValuesIn(DNSID_MATCH_PARAMS));

class pkixnames_Turkish_I_Comparison
  : public ::testing::Test
  , public ::testing::WithParamInterface<InputValidity>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_Turkish_I_Comparison, MatchPresentedDNSIDWithReferenceDNSID)
{
  // Make sure we don't have the similar problems that strcasecmp and others
  // have with the other kinds of "i" and "I" commonly used in Turkish locales.

  const InputValidity& inputValidity(GetParam());
  SCOPED_TRACE(inputValidity.input.c_str());
  Input input;
  ASSERT_EQ(Success, input.Init(inputValidity.input.data(),
                                inputValidity.input.length()));

  bool isASCII = InputsAreEqual(LOWERCASE_I, input) ||
                 InputsAreEqual(UPPERCASE_I, input);
  {
    bool matches;
    ASSERT_EQ(inputValidity.isValidPresentedID ? Success
                                               : Result::ERROR_BAD_DER,
              MatchPresentedDNSIDWithReferenceDNSID(input, LOWERCASE_I,
                                                    matches));
    if (inputValidity.isValidPresentedID) {
      ASSERT_EQ(isASCII, matches);
    }
  }
  {
    bool matches;
    ASSERT_EQ(inputValidity.isValidPresentedID ? Success
                                               : Result::ERROR_BAD_DER,
              MatchPresentedDNSIDWithReferenceDNSID(input, UPPERCASE_I,
                                                    matches));
    if (inputValidity.isValidPresentedID) {
      ASSERT_EQ(isASCII, matches);
    }
  }
}

INSTANTIATE_TEST_CASE_P(pkixnames_Turkish_I_Comparison,
                        pkixnames_Turkish_I_Comparison,
                        testing::ValuesIn(DNSNAMES_VALIDITY_TURKISH_I));

class pkixnames_IsValidReferenceDNSID
  : public ::testing::Test
  , public ::testing::WithParamInterface<InputValidity>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_IsValidReferenceDNSID, IsValidReferenceDNSID)
{
  const InputValidity& inputValidity(GetParam());
  SCOPED_TRACE(inputValidity.input.c_str());
  Input input;
  ASSERT_EQ(Success, input.Init(inputValidity.input.data(),
                                inputValidity.input.length()));
  ASSERT_EQ(inputValidity.isValidReferenceID, IsValidReferenceDNSID(input));
  ASSERT_EQ(inputValidity.isValidPresentedID, IsValidPresentedDNSID(input));
}

INSTANTIATE_TEST_CASE_P(pkixnames_IsValidReferenceDNSID,
                        pkixnames_IsValidReferenceDNSID,
                        testing::ValuesIn(DNSNAMES_VALIDITY));
INSTANTIATE_TEST_CASE_P(pkixnames_IsValidReferenceDNSID_Turkish_I,
                        pkixnames_IsValidReferenceDNSID,
                        testing::ValuesIn(DNSNAMES_VALIDITY_TURKISH_I));

class pkixnames_ParseIPv4Address
  : public ::testing::Test
  , public ::testing::WithParamInterface<IPAddressParams<4>>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_ParseIPv4Address, ParseIPv4Address)
{
  const IPAddressParams<4>& param(GetParam());
  SCOPED_TRACE(param.input.c_str());
  Input input;
  ASSERT_EQ(Success, input.Init(param.input.data(),
                                param.input.length()));
  uint8_t ipAddress[4];
  ASSERT_EQ(param.isValid, ParseIPv4Address(input, ipAddress));
  if (param.isValid) {
    for (size_t i = 0; i < sizeof(ipAddress); ++i) {
      ASSERT_EQ(param.expectedValueIfValid[i], ipAddress[i]);
    }
  }
}

INSTANTIATE_TEST_CASE_P(pkixnames_ParseIPv4Address,
                        pkixnames_ParseIPv4Address,
                        testing::ValuesIn(IPV4_ADDRESSES));

class pkixnames_ParseIPv6Address
  : public ::testing::Test
  , public ::testing::WithParamInterface<IPAddressParams<16>>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_ParseIPv6Address, ParseIPv6Address)
{
  const IPAddressParams<16>& param(GetParam());
  SCOPED_TRACE(param.input.c_str());
  Input input;
  ASSERT_EQ(Success, input.Init(param.input.data(),
                                param.input.length()));
  uint8_t ipAddress[16];
  ASSERT_EQ(param.isValid, ParseIPv6Address(input, ipAddress));
  if (param.isValid) {
    for (size_t i = 0; i < sizeof(ipAddress); ++i) {
      ASSERT_EQ(param.expectedValueIfValid[i], ipAddress[i]);
    }
  }
}

INSTANTIATE_TEST_CASE_P(pkixnames_ParseIPv6Address,
                        pkixnames_ParseIPv6Address,
                        testing::ValuesIn(IPV6_ADDRESSES));

// This is an arbitrary string that is used to indicate that no SAN extension
// should be put into the generated certificate. It needs to be different from
// "" or any other subjectAltName value that we actually want to test, but its
// actual value does not matter. Note that this isn't a correctly-encoded SAN
// extension value!
static const ByteString
  NO_SAN(reinterpret_cast<const uint8_t*>("I'm a bad, bad, certificate"));

struct CheckCertHostnameParams
{
  ByteString hostname;
  ByteString subject;
  ByteString subjectAltName;
  Result result;
};

class pkixnames_CheckCertHostname
  : public ::testing::Test
  , public ::testing::WithParamInterface<CheckCertHostnameParams>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

#define WITH_SAN(r, ps, psan, result) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(r), sizeof(r) - 1), \
    ps, \
    psan, \
    result \
  }

#define WITHOUT_SAN(r, ps, result) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(r), sizeof(r) - 1), \
    ps, \
    NO_SAN, \
    result \
  }

static const uint8_t example_com[] = {
  'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'
};

// Note that We avoid zero-valued bytes in these IP addresses so that we don't
// get false negatives from anti-NULL-byte defenses in dNSName decoding.
static const uint8_t ipv4_addr_bytes[] = {
  1, 2, 3, 4
};
static const uint8_t ipv4_addr_bytes_as_str[] = "\x01\x02\x03\x04";
static const uint8_t ipv4_addr_str[] = "1.2.3.4";
static const uint8_t ipv4_addr_bytes_FFFFFFFF[8] = {
  1, 2, 3, 4, 0xff, 0xff, 0xff, 0xff
};

static const uint8_t ipv4_compatible_ipv6_addr_bytes[] = {
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
  1, 2, 3, 4
};
static const uint8_t ipv4_compatible_ipv6_addr_str[] = "::1.2.3.4";

static const uint8_t ipv4_mapped_ipv6_addr_bytes[] = {
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0xFF, 0xFF,
  1, 2, 3, 4
};
static const uint8_t ipv4_mapped_ipv6_addr_str[] = "::FFFF:1.2.3.4";

static const uint8_t ipv6_addr_bytes[] = {
  0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88,
  0x99, 0xaa, 0xbb, 0xcc,
  0xdd, 0xee, 0xff, 0x11
};
static const uint8_t ipv6_addr_bytes_as_str[] =
  "\x11\x22\x33\x44"
  "\x55\x66\x77\x88"
  "\x99\xaa\xbb\xcc"
  "\xdd\xee\xff\x11";

static const uint8_t ipv6_addr_str[] =
  "1122:3344:5566:7788:99aa:bbcc:ddee:ff11";

static const uint8_t ipv6_other_addr_bytes[] = {
  0xff, 0xee, 0xdd, 0xcc,
  0xbb, 0xaa, 0x99, 0x88,
  0x77, 0x66, 0x55, 0x44,
  0x33, 0x22, 0x11, 0x00,
};

static const uint8_t ipv4_other_addr_bytes[] = {
  5, 6, 7, 8
};
static const uint8_t ipv4_other_addr_bytes_FFFFFFFF[] = {
  5, 6, 7, 8, 0xff, 0xff, 0xff, 0xff
};

static const uint8_t ipv4_addr_00000000_bytes[] = {
  0, 0, 0, 0
};
static const uint8_t ipv4_addr_FFFFFFFF_bytes[] = {
  0, 0, 0, 0
};

static const uint8_t ipv4_constraint_all_zeros_bytes[] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

static const uint8_t ipv6_addr_all_zeros_bytes[] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t ipv6_constraint_all_zeros_bytes[] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};

static const uint8_t ipv4_constraint_CIDR_16_bytes[] = {
  1, 2, 0, 0, 0xff, 0xff, 0, 0
};
static const uint8_t ipv4_constraint_CIDR_17_bytes[] = {
  1, 2, 0, 0, 0xff, 0xff, 0x80, 0
};

// The subnet is 1.2.0.0/16 but it is specified as 1.2.3.0/16
static const uint8_t ipv4_constraint_CIDR_16_bad_addr_bytes[] = {
  1, 2, 3, 0, 0xff, 0xff, 0, 0
};

// Masks are supposed to be of the form <ones><zeros>, but this one is of the
// form <ones><zeros><ones><zeros>.
static const uint8_t ipv4_constraint_bad_mask_bytes[] = {
  1, 2, 3, 0, 0xff, 0, 0xff, 0
};

static const uint8_t ipv6_constraint_CIDR_16_bytes[] = {
  0x11, 0x22, 0, 0, 0, 0, 0, 0,
     0,    0, 0, 0, 0, 0, 0, 0,
  0xff, 0xff, 0, 0, 0, 0, 0, 0,
     0,    0, 0, 0, 0, 0, 0, 0
};

// The subnet is 1122::/16 but it is specified as 1122:3344::/16
static const uint8_t ipv6_constraint_CIDR_16_bad_addr_bytes[] = {
  0x11, 0x22, 0x33, 0x44, 0, 0, 0, 0,
     0,    0,    0,    0, 0, 0, 0, 0,
  0xff, 0xff,    0,    0, 0, 0, 0, 0,
     0,    0,    0,    0, 0, 0, 0, 0
};

// Masks are supposed to be of the form <ones><zeros>, but this one is of the
// form <ones><zeros><ones><zeros>.
static const uint8_t ipv6_constraint_bad_mask_bytes[] = {
  0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0, 0,
     0,    0,    0,    0,    0,    0, 0, 0,
  0xff, 0xff,    0,    0, 0xff, 0xff, 0, 0,
     0,    0,    0,    0,    0,    0, 0, 0,
};

static const uint8_t ipv4_addr_truncated_bytes[] = {
  1, 2, 3
};
static const uint8_t ipv4_addr_overlong_bytes[] = {
  1, 2, 3, 4, 5
};
static const uint8_t ipv4_constraint_truncated_bytes[] = {
  0, 0, 0, 0,
  0, 0, 0,
};
static const uint8_t ipv4_constraint_overlong_bytes[] = {
  0, 0, 0, 0,
  0, 0, 0, 0, 0
};

static const uint8_t ipv6_addr_truncated_bytes[] = {
  0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88,
  0x99, 0xaa, 0xbb, 0xcc,
  0xdd, 0xee, 0xff
};
static const uint8_t ipv6_addr_overlong_bytes[] = {
  0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88,
  0x99, 0xaa, 0xbb, 0xcc,
  0xdd, 0xee, 0xff, 0x11, 0x00
};
static const uint8_t ipv6_constraint_truncated_bytes[] = {
  0x11, 0x22, 0, 0, 0, 0, 0, 0,
     0,    0, 0, 0, 0, 0, 0, 0,
  0xff, 0xff, 0, 0, 0, 0, 0, 0,
     0,    0, 0, 0, 0, 0, 0
};
static const uint8_t ipv6_constraint_overlong_bytes[] = {
  0x11, 0x22, 0, 0, 0, 0, 0, 0,
     0,    0, 0, 0, 0, 0, 0, 0,
  0xff, 0xff, 0, 0, 0, 0, 0, 0,
     0,    0, 0, 0, 0, 0, 0, 0, 0
};

// Note that, for DNSNames, these test cases in CHECK_CERT_HOSTNAME_PARAMS are
// mostly about testing different scenerios regarding the structure of entries
// in the subjectAltName and subject of the certificate, than about the how
// specific presented identifier values are matched against the reference
// identifier values. This is because we also use the test cases in
// DNSNAMES_VALIDITY to test CheckCertHostname. Consequently, tests about
// whether specific presented DNSNames (including wildcards, in particular) are
// matched against a reference DNSName only need to be added to
// DNSNAMES_VALIDITY, and not here.
static const CheckCertHostnameParams CHECK_CERT_HOSTNAME_PARAMS[] =
{
  // This is technically illegal. PrintableString is defined in such a way that
  // '*' is not an allowed character, but there are many real-world certificates
  // that are encoded this way.
  WITHOUT_SAN("foo.example.com", RDN(CN("*.example.com", der::PrintableString)),
              Success),
  WITHOUT_SAN("foo.example.com", RDN(CN("*.example.com", der::UTF8String)),
              Success),

  // Many certificates use TeletexString when encoding wildcards in CN-IDs
  // because PrintableString is defined as not allowing '*' and UTF8String was,
  // at one point in history, considered too new to depend on for compatibility.
  // We accept TeletexString-encoded CN-IDs when they don't contain any escape
  // sequences. The reference I used for the escape codes was
  // https://tools.ietf.org/html/rfc1468. The escaping mechanism is actually
  // pretty complex and these tests don't even come close to testing all the
  // possibilities.
  WITHOUT_SAN("foo.example.com", RDN(CN("*.example.com", der::TeletexString)),
              Success),
  // "ESC ( B" ({0x1B,0x50,0x42}) is the escape code to switch to ASCII, which
  // is redundant because it already the default.
  WITHOUT_SAN("foo.example.com",
              RDN(CN("\x1B(B*.example.com", der::TeletexString)),
              Result::ERROR_BAD_CERT_DOMAIN),
  WITHOUT_SAN("foo.example.com",
              RDN(CN("*.example\x1B(B.com", der::TeletexString)),
              Result::ERROR_BAD_CERT_DOMAIN),
  WITHOUT_SAN("foo.example.com",
              RDN(CN("*.example.com\x1B(B", der::TeletexString)),
              Result::ERROR_BAD_CERT_DOMAIN),
  // "ESC $ B" ({0x1B,0x24,0x42}) is the escape code to switch to
  // JIS X 0208-1983 (a Japanese character set).
  WITHOUT_SAN("foo.example.com",
              RDN(CN("\x1B$B*.example.com", der::TeletexString)),
              Result::ERROR_BAD_CERT_DOMAIN),
  WITHOUT_SAN("foo.example.com",
              RDN(CN("*.example.com\x1B$B", der::TeletexString)),
              Result::ERROR_BAD_CERT_DOMAIN),

  // Match a DNSName SAN entry with a redundant (ignored) matching CN-ID.
  WITH_SAN("a", RDN(CN("a")), DNSName("a"), Success),
  // Match a DNSName SAN entry when there is an CN-ID that doesn't match.
  WITH_SAN("b", RDN(CN("a")), DNSName("b"), Success),
  // Do not match a CN-ID when there is a valid DNSName SAN Entry.
  WITH_SAN("a", RDN(CN("a")), DNSName("b"), Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match a CN-ID when there is a malformed DNSName SAN Entry.
  WITH_SAN("a", RDN(CN("a")), DNSName("!"), Result::ERROR_BAD_DER),
  // Do not match a matching CN-ID when there is a valid IPAddress SAN entry.
  WITH_SAN("a", RDN(CN("a")), IPAddress(ipv4_addr_bytes),
           Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match a matching CN-ID when there is a malformed IPAddress SAN entry.
  WITH_SAN("a", RDN(CN("a")), IPAddress(example_com),
           Result::ERROR_BAD_CERT_DOMAIN),
  // Match a DNSName against a matching CN-ID when there is a SAN, but the SAN
  // does not contain an DNSName or IPAddress entry.
  WITH_SAN("a", RDN(CN("a")), RFC822Name("foo@example.com"), Success),
  // Match a matching CN-ID when there is no SAN.
  WITHOUT_SAN("a", RDN(CN("a")), Success),
  // Do not match a mismatching CN-ID when there is no SAN.
  WITHOUT_SAN("a", RDN(CN("b")), Result::ERROR_BAD_CERT_DOMAIN),

  // The first DNSName matches.
  WITH_SAN("a", RDN(CN("foo")), DNSName("a") + DNSName("b"), Success),
  // The last DNSName matches.
  WITH_SAN("b", RDN(CN("foo")), DNSName("a") + DNSName("b"), Success),
  // The middle DNSName matches.
  WITH_SAN("b", RDN(CN("foo")),
           DNSName("a") + DNSName("b") + DNSName("c"), Success),
  // After an IP address.
  WITH_SAN("b", RDN(CN("foo")),
           IPAddress(ipv4_addr_bytes) + DNSName("b"), Success),
  // Before an IP address.
  WITH_SAN("a", RDN(CN("foo")),
           DNSName("a") + IPAddress(ipv4_addr_bytes), Success),
  // Between an RFC822Name and an IP address.
  WITH_SAN("b", RDN(CN("foo")),
           RFC822Name("foo@example.com") + DNSName("b") +
                                           IPAddress(ipv4_addr_bytes),
           Success),
  // Duplicate DNSName.
  WITH_SAN("a", RDN(CN("foo")), DNSName("a") + DNSName("a"), Success),
  // After an invalid DNSName.
  WITH_SAN("b", RDN(CN("foo")), DNSName("!") + DNSName("b"),
           Result::ERROR_BAD_DER),

  // http://tools.ietf.org/html/rfc5280#section-4.2.1.6: "If the subjectAltName
  // extension is present, the sequence MUST contain at least one entry."
  // However, for compatibility reasons, this is not enforced. See bug 1143085.
  // This case is treated as if the extension is not present (i.e. name
  // matching falls back to the subject CN).
  WITH_SAN("a", RDN(CN("a")), ByteString(), Success),
  WITH_SAN("a", RDN(CN("b")), ByteString(), Result::ERROR_BAD_CERT_DOMAIN),

  // http://tools.ietf.org/html/rfc5280#section-4.1.2.6 says "If subject naming
  // information is present only in the subjectAltName extension (e.g., a key
  // bound only to an email address or URI), then the subject name MUST be an
  // empty sequence and the subjectAltName extension MUST be critical." So, we
  // have to support an empty subject. We don't enforce that the SAN must be
  // critical or even that there is a SAN when the subject is empty, though.
  WITH_SAN("a", ByteString(), DNSName("a"), Success),
  // Make sure we return ERROR_BAD_CERT_DOMAIN and not ERROR_BAD_DER.
  WITHOUT_SAN("a", ByteString(), Result::ERROR_BAD_CERT_DOMAIN),

  // Two CNs in the same RDN, both match.
  WITHOUT_SAN("a", RDN(CN("a") + CN("a")), Success),
  // Two CNs in the same RDN, both DNSNames, first one matches.
  WITHOUT_SAN("a", RDN(CN("a") + CN("b")),
              Result::ERROR_BAD_CERT_DOMAIN),
  // Two CNs in the same RDN, both DNSNames, last one matches.
  WITHOUT_SAN("b", RDN(CN("a") + CN("b")), Success),
  // Two CNs in the same RDN, first one matches, second isn't a DNSName.
  WITHOUT_SAN("a", RDN(CN("a") + CN("Not a DNSName")),
              Result::ERROR_BAD_CERT_DOMAIN),
  // Two CNs in the same RDN, first one not a DNSName, second matches.
  WITHOUT_SAN("b", RDN(CN("Not a DNSName") + CN("b")), Success),

  // Two CNs in separate RDNs, both match.
  WITHOUT_SAN("a", RDN(CN("a")) + RDN(CN("a")), Success),
  // Two CNs in separate RDNs, both DNSNames, first one matches.
  WITHOUT_SAN("a", RDN(CN("a")) + RDN(CN("b")),
              Result::ERROR_BAD_CERT_DOMAIN),
  // Two CNs in separate RDNs, both DNSNames, last one matches.
  WITHOUT_SAN("b", RDN(CN("a")) + RDN(CN("b")), Success),
  // Two CNs in separate RDNs, first one matches, second isn't a DNSName.
  WITHOUT_SAN("a", RDN(CN("a")) + RDN(CN("Not a DNSName")),
              Result::ERROR_BAD_CERT_DOMAIN),
  // Two CNs in separate RDNs, first one not a DNSName, second matches.
  WITHOUT_SAN("b", RDN(CN("Not a DNSName")) + RDN(CN("b")), Success),

  // One CN, one RDN, CN is the first AVA in the RDN, CN matches.
  WITHOUT_SAN("a", RDN(CN("a") + OU("b")), Success),
  // One CN, one RDN, CN is the first AVA in the RDN, CN does not match.
  WITHOUT_SAN("b", RDN(CN("a") + OU("b")),
              Result::ERROR_BAD_CERT_DOMAIN),
  // One CN, one RDN, CN is not the first AVA in the RDN, CN matches.
  WITHOUT_SAN("b", RDN(OU("a") + CN("b")), Success),
  // One CN, one RDN, CN is not the first AVA in the RDN, CN does not match.
  WITHOUT_SAN("a", RDN(OU("a") + CN("b")),
              Result::ERROR_BAD_CERT_DOMAIN),

  // One CN, multiple RDNs, CN is in the first RDN, CN matches.
  WITHOUT_SAN("a", RDN(CN("a")) + RDN(OU("b")), Success),
  // One CN, multiple RDNs, CN is in the first RDN, CN does not match.
  WITHOUT_SAN("b", RDN(CN("a")) + RDN(OU("b")), Result::ERROR_BAD_CERT_DOMAIN),
  // One CN, multiple RDNs, CN is not in the first RDN, CN matches.
  WITHOUT_SAN("b", RDN(OU("a")) + RDN(CN("b")), Success),
  // One CN, multiple RDNs, CN is not in the first RDN, CN does not match.
  WITHOUT_SAN("a", RDN(OU("a")) + RDN(CN("b")), Result::ERROR_BAD_CERT_DOMAIN),

  // One CN, one RDN, CN is not in the first or last AVA, CN matches.
  WITHOUT_SAN("b", RDN(OU("a") + CN("b") + OU("c")), Success),
  // One CN, multiple RDNs, CN is not in the first or last RDN, CN matches.
  WITHOUT_SAN("b", RDN(OU("a")) + RDN(CN("b")) + RDN(OU("c")), Success),

  // Empty CN does not match.
  WITHOUT_SAN("example.com", RDN(CN("")), Result::ERROR_BAD_CERT_DOMAIN),

  WITHOUT_SAN("uses_underscore.example.com", RDN(CN("*.example.com")), Success),
  WITHOUT_SAN("a.uses_underscore.example.com",
              RDN(CN("*.uses_underscore.example.com")), Success),
  WITH_SAN("uses_underscore.example.com", RDN(CN("foo")),
           DNSName("*.example.com"), Success),
  WITH_SAN("a.uses_underscore.example.com", RDN(CN("foo")),
           DNSName("*.uses_underscore.example.com"), Success),

  // Do not match a DNSName that is encoded in a malformed IPAddress.
  WITH_SAN("example.com", RDN(CN("foo")), IPAddress(example_com),
           Result::ERROR_BAD_CERT_DOMAIN),

  // We skip over the malformed IPAddress and match the DNSName entry because
  // we've heard reports of real-world certificates that have malformed
  // IPAddress SANs.
  WITH_SAN("example.org", RDN(CN("foo")),
           IPAddress(example_com) + DNSName("example.org"), Success),

  WITH_SAN("example.com", RDN(CN("foo")),
           DNSName("!") + DNSName("example.com"), Result::ERROR_BAD_DER),

  // Match a matching IPv4 address SAN entry.
  WITH_SAN(ipv4_addr_str, RDN(CN("foo")), IPAddress(ipv4_addr_bytes),
           Success),
  // Match a matching IPv4 addresses in the CN when there is no SAN
  WITHOUT_SAN(ipv4_addr_str, RDN(CN(ipv4_addr_str)), Success),
  // Do not match a matching IPv4 address in the CN when there is a SAN with
  // a DNSName entry.
  WITH_SAN(ipv4_addr_str, RDN(CN(ipv4_addr_str)),
           DNSName("example.com"), Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match a matching IPv4 address in the CN when there is a SAN with
  // a non-matching IPAddress entry.
  WITH_SAN(ipv4_addr_str, RDN(CN(ipv4_addr_str)),
           IPAddress(ipv6_addr_bytes), Result::ERROR_BAD_CERT_DOMAIN),
  // Match a matching IPv4 address in the CN when there is a SAN with a
  // non-IPAddress, non-DNSName entry.
  WITH_SAN(ipv4_addr_str, RDN(CN(ipv4_addr_str)),
           RFC822Name("foo@example.com"), Success),
  // Do not match a matching IPv4 address in the CN when there is a SAN with a
  // malformed IPAddress entry.
  WITH_SAN(ipv4_addr_str, RDN(CN(ipv4_addr_str)),
           IPAddress(example_com), Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match a matching IPv4 address in the CN when there is a SAN with a
  // malformed DNSName entry.
  WITH_SAN(ipv4_addr_str, RDN(CN(ipv4_addr_str)),
           DNSName("!"), Result::ERROR_BAD_CERT_DOMAIN),

  // We don't match IPv6 addresses in the CN, regardless of whether there is
  // a SAN.
  WITHOUT_SAN(ipv6_addr_str, RDN(CN(ipv6_addr_str)),
              Result::ERROR_BAD_CERT_DOMAIN),
  WITH_SAN(ipv6_addr_str, RDN(CN(ipv6_addr_str)),
           DNSName("example.com"), Result::ERROR_BAD_CERT_DOMAIN),
  WITH_SAN(ipv6_addr_str, RDN(CN(ipv6_addr_str)),
                          IPAddress(ipv6_addr_bytes), Success),
  WITH_SAN(ipv6_addr_str, RDN(CN("foo")), IPAddress(ipv6_addr_bytes),
           Success),

  // We don't match the binary encoding of the bytes of IP addresses in the
  // CN.
  WITHOUT_SAN(ipv4_addr_str, RDN(CN(ipv4_addr_bytes_as_str)),
              Result::ERROR_BAD_CERT_DOMAIN),
  WITHOUT_SAN(ipv6_addr_str, RDN(CN(ipv6_addr_bytes_as_str)),
              Result::ERROR_BAD_CERT_DOMAIN),

  // We don't match IP addresses with DNSName SANs.
  WITH_SAN(ipv4_addr_str, RDN(CN("foo")),
           DNSName(ipv4_addr_bytes_as_str), Result::ERROR_BAD_CERT_DOMAIN),
  WITH_SAN(ipv4_addr_str, RDN(CN("foo")), DNSName(ipv4_addr_str),
           Result::ERROR_BAD_CERT_DOMAIN),
  WITH_SAN(ipv6_addr_str, RDN(CN("foo")),
           DNSName(ipv6_addr_bytes_as_str), Result::ERROR_BAD_CERT_DOMAIN),
  WITH_SAN(ipv6_addr_str, RDN(CN("foo")), DNSName(ipv6_addr_str),
           Result::ERROR_BAD_CERT_DOMAIN),

  // Do not match an IPv4 reference ID against the equivalent IPv4-compatible
  // IPv6 SAN entry.
  WITH_SAN(ipv4_addr_str, RDN(CN("foo")),
           IPAddress(ipv4_compatible_ipv6_addr_bytes),
           Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match an IPv4 reference ID against the equivalent IPv4-mapped IPv6
  // SAN entry.
  WITH_SAN(ipv4_addr_str, RDN(CN("foo")),
           IPAddress(ipv4_mapped_ipv6_addr_bytes),
           Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match an IPv4-compatible IPv6 reference ID against the equivalent
  // IPv4 SAN entry.
  WITH_SAN(ipv4_compatible_ipv6_addr_str, RDN(CN("foo")),
           IPAddress(ipv4_addr_bytes), Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match an IPv4 reference ID against the equivalent IPv4-mapped IPv6
  // SAN entry.
  WITH_SAN(ipv4_mapped_ipv6_addr_str, RDN(CN("foo")),
           IPAddress(ipv4_addr_bytes),
           Result::ERROR_BAD_CERT_DOMAIN),

  // Test that the presence of an otherName entry is handled appropriately.
  // (The actual value of the otherName entry isn't important - that's not what
  // we're testing here.)
  WITH_SAN("example.com", ByteString(),
           // The tag for otherName is CONTEXT_SPECIFIC | CONSTRUCTED | 0
           TLV((2 << 6) | (1 << 5) | 0, ByteString()) + DNSName("example.com"),
           Success),
  WITH_SAN("example.com", ByteString(),
           TLV((2 << 6) | (1 << 5) | 0, ByteString()),
           Result::ERROR_BAD_CERT_DOMAIN),
};

ByteString
CreateCert(const ByteString& subject, const ByteString& subjectAltName,
           EndEntityOrCA endEntityOrCA = EndEntityOrCA::MustBeEndEntity)
{
  ByteString serialNumber(CreateEncodedSerialNumber(1));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));

  ByteString issuerDER(Name(RDN(CN("issuer"))));
  EXPECT_FALSE(ENCODING_FAILED(issuerDER));

  ByteString extensions[2];
  if (subjectAltName != NO_SAN) {
    extensions[0] = CreateEncodedSubjectAltName(subjectAltName);
    EXPECT_FALSE(ENCODING_FAILED(extensions[0]));
  }
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    // Currently, these tests assume that if we're creating a CA certificate, it
    // will not have a subjectAlternativeName extension. If that assumption
    // changes, this code will have to be updated. Ideally this would be
    // ASSERT_EQ, but that inserts a 'return;', which doesn't match this
    // function's return type.
    EXPECT_EQ(subjectAltName, NO_SAN);
    extensions[0] = CreateEncodedBasicConstraints(true, nullptr,
                                                  Critical::Yes);
    EXPECT_FALSE(ENCODING_FAILED(extensions[0]));
  }

  ScopedTestKeyPair keyPair(CloneReusedKeyPair());
  return CreateEncodedCertificate(
                    v3, sha256WithRSAEncryption(), serialNumber, issuerDER,
                    oneDayBeforeNow, oneDayAfterNow, Name(subject), *keyPair,
                    extensions, *keyPair, sha256WithRSAEncryption());
}

TEST_P(pkixnames_CheckCertHostname, CheckCertHostname)
{
  const CheckCertHostnameParams& param(GetParam());

  ByteString cert(CreateCert(param.subject, param.subjectAltName));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));

  Input hostnameInput;
  ASSERT_EQ(Success, hostnameInput.Init(param.hostname.data(),
                                        param.hostname.length()));

  ASSERT_EQ(param.result, CheckCertHostname(certInput, hostnameInput,
                                            mNameMatchingPolicy));
}

INSTANTIATE_TEST_CASE_P(pkixnames_CheckCertHostname,
                        pkixnames_CheckCertHostname,
                        testing::ValuesIn(CHECK_CERT_HOSTNAME_PARAMS));

TEST_F(pkixnames_CheckCertHostname, SANWithoutSequence)
{
  // A certificate with a truly empty SAN extension (one that doesn't even
  // contain a SEQUENCE at all) is malformed. If we didn't treat this as
  // malformed then we'd have to treat it like the CN_EmptySAN cases.

  ByteString serialNumber(CreateEncodedSerialNumber(1));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));

  ByteString extensions[2];
  extensions[0] = CreateEncodedEmptySubjectAltName();
  ASSERT_FALSE(ENCODING_FAILED(extensions[0]));

  ScopedTestKeyPair keyPair(CloneReusedKeyPair());
  ByteString certDER(CreateEncodedCertificate(
                       v3, sha256WithRSAEncryption(), serialNumber,
                       Name(RDN(CN("issuer"))), oneDayBeforeNow, oneDayAfterNow,
                       Name(RDN(CN("a"))), *keyPair, extensions,
                       *keyPair, sha256WithRSAEncryption()));
  ASSERT_FALSE(ENCODING_FAILED(certDER));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(certDER.data(), certDER.length()));

  static const uint8_t a[] = { 'a' };
  ASSERT_EQ(Result::ERROR_EXTENSION_VALUE_INVALID,
            CheckCertHostname(certInput, Input(a), mNameMatchingPolicy));
}

class pkixnames_CheckCertHostname_PresentedMatchesReference
  : public ::testing::Test
  , public ::testing::WithParamInterface<PresentedMatchesReference>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_CheckCertHostname_PresentedMatchesReference, CN_NoSAN)
{
  // Since there is no SAN, a valid presented DNS ID in the subject CN field
  // should result in a match.

  const PresentedMatchesReference& param(GetParam());

  ByteString cert(CreateCert(RDN(CN(param.presentedDNSID)), NO_SAN));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));

  Input hostnameInput;
  ASSERT_EQ(Success, hostnameInput.Init(param.referenceDNSID.data(),
                                        param.referenceDNSID.length()));

  ASSERT_EQ(param.expectedMatches ? Success : Result::ERROR_BAD_CERT_DOMAIN,
            CheckCertHostname(certInput, hostnameInput, mNameMatchingPolicy));
}

TEST_P(pkixnames_CheckCertHostname_PresentedMatchesReference,
       SubjectAltName_CNNotDNSName)
{
  // A DNSName SAN entry should match, regardless of the contents of the
  // subject CN.

  const PresentedMatchesReference& param(GetParam());

  ByteString cert(CreateCert(RDN(CN("Common Name")),
                             DNSName(param.presentedDNSID)));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));

  Input hostnameInput;
  ASSERT_EQ(Success, hostnameInput.Init(param.referenceDNSID.data(),
                                        param.referenceDNSID.length()));
  Result expectedResult
    = param.expectedResult != Success ? param.expectedResult
    : param.expectedMatches ? Success
    : Result::ERROR_BAD_CERT_DOMAIN;
  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, hostnameInput,
                                              mNameMatchingPolicy));
}

INSTANTIATE_TEST_CASE_P(pkixnames_CheckCertHostname_DNSID_MATCH_PARAMS,
                        pkixnames_CheckCertHostname_PresentedMatchesReference,
                        testing::ValuesIn(DNSID_MATCH_PARAMS));

TEST_P(pkixnames_Turkish_I_Comparison, CheckCertHostname_CN_NoSAN)
{
  // Make sure we don't have the similar problems that strcasecmp and others
  // have with the other kinds of "i" and "I" commonly used in Turkish locales,
  // when we're matching a CN due to lack of subjectAltName.

  const InputValidity& param(GetParam());
  SCOPED_TRACE(param.input.c_str());

  Input input;
  ASSERT_EQ(Success, input.Init(param.input.data(), param.input.length()));

  ByteString cert(CreateCert(RDN(CN(param.input)), NO_SAN));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));

  Result expectedResult = (InputsAreEqual(LOWERCASE_I, input) ||
                           InputsAreEqual(UPPERCASE_I, input))
                        ? Success
                        : Result::ERROR_BAD_CERT_DOMAIN;

  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, UPPERCASE_I,
                                              mNameMatchingPolicy));
  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, LOWERCASE_I,
                                              mNameMatchingPolicy));
}

TEST_P(pkixnames_Turkish_I_Comparison, CheckCertHostname_SAN)
{
  // Make sure we don't have the similar problems that strcasecmp and others
  // have with the other kinds of "i" and "I" commonly used in Turkish locales,
  // when we're matching a dNSName in the SAN.

  const InputValidity& param(GetParam());
  SCOPED_TRACE(param.input.c_str());

  Input input;
  ASSERT_EQ(Success, input.Init(param.input.data(), param.input.length()));

  ByteString cert(CreateCert(RDN(CN("Common Name")), DNSName(param.input)));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));

  Result expectedResult
    = (!param.isValidPresentedID) ? Result::ERROR_BAD_DER
    : (InputsAreEqual(LOWERCASE_I, input) ||
       InputsAreEqual(UPPERCASE_I, input)) ? Success
    : Result::ERROR_BAD_CERT_DOMAIN;

  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, UPPERCASE_I,
                                              mNameMatchingPolicy));
  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, LOWERCASE_I,
                                              mNameMatchingPolicy));
}

class pkixnames_CheckCertHostname_IPV4_Addresses
  : public ::testing::Test
  , public ::testing::WithParamInterface<IPAddressParams<4>>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_CheckCertHostname_IPV4_Addresses,
       ValidIPv4AddressInIPAddressSAN)
{
  // When the reference hostname is a valid IPv4 address, a correctly-formed
  // IPv4 Address SAN matches it.

  const IPAddressParams<4>& param(GetParam());

  ByteString cert(CreateCert(RDN(CN("Common Name")),
                             IPAddress(param.expectedValueIfValid)));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));

  Input hostnameInput;
  ASSERT_EQ(Success, hostnameInput.Init(param.input.data(),
                                        param.input.length()));

  ASSERT_EQ(param.isValid ? Success : Result::ERROR_BAD_CERT_DOMAIN,
            CheckCertHostname(certInput, hostnameInput, mNameMatchingPolicy));
}

TEST_P(pkixnames_CheckCertHostname_IPV4_Addresses,
       ValidIPv4AddressInCN_NoSAN)
{
  // When the reference hostname is a valid IPv4 address, a correctly-formed
  // IPv4 Address in the CN matches it when there is no SAN.

  const IPAddressParams<4>& param(GetParam());

  SCOPED_TRACE(param.input.c_str());

  ByteString cert(CreateCert(RDN(CN(param.input)), NO_SAN));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));

  Input hostnameInput;
  ASSERT_EQ(Success, hostnameInput.Init(param.input.data(),
                                        param.input.length()));

  // Some of the invalid IPv4 addresses are valid DNS names!
  Result expectedResult = (param.isValid || IsValidReferenceDNSID(hostnameInput))
                        ? Success
                        : Result::ERROR_BAD_CERT_DOMAIN;

  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, hostnameInput,
                                              mNameMatchingPolicy));
}

INSTANTIATE_TEST_CASE_P(pkixnames_CheckCertHostname_IPV4_ADDRESSES,
                        pkixnames_CheckCertHostname_IPV4_Addresses,
                        testing::ValuesIn(IPV4_ADDRESSES));

struct NameConstraintParams
{
  ByteString subject;
  ByteString subjectAltName;
  ByteString subtrees;
  Result expectedPermittedSubtreesResult;
  Result expectedExcludedSubtreesResult;
};

static ByteString
PermittedSubtrees(const ByteString& generalSubtrees)
{
  return TLV(der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
             generalSubtrees);
}

static ByteString
ExcludedSubtrees(const ByteString& generalSubtrees)
{
  return TLV(der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
             generalSubtrees);
}

// Does not encode min or max.
static ByteString
GeneralSubtree(const ByteString& base)
{
  return TLV(der::SEQUENCE, base);
}

static const NameConstraintParams NAME_CONSTRAINT_PARAMS[] =
{
  /////////////////////////////////////////////////////////////////////////////
  // XXX: Malformed name constraints for supported types of names are ignored
  // when there are no names of that type to constrain.
  { ByteString(), NO_SAN,
    GeneralSubtree(DNSName("!")),
    Success, Success
  },
  { // DirectoryName constraints are an exception, because *every* certificate
    // has at least one DirectoryName (tbsCertificate.subject).
    ByteString(), NO_SAN,
    GeneralSubtree(Name(ByteString(reinterpret_cast<const uint8_t*>("!"), 1))),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { ByteString(), NO_SAN,
    GeneralSubtree(IPAddress(ipv4_constraint_truncated_bytes)),
    Success, Success
  },
  { ByteString(), NO_SAN,
    GeneralSubtree(IPAddress(ipv4_constraint_overlong_bytes)),
    Success, Success
  },
  { ByteString(), NO_SAN,
  GeneralSubtree(IPAddress(ipv6_constraint_truncated_bytes)),
  Success, Success
  },
  { ByteString(), NO_SAN,
  GeneralSubtree(IPAddress(ipv6_constraint_overlong_bytes)),
  Success, Success
  },
  { ByteString(), NO_SAN,
    GeneralSubtree(RFC822Name("!")),
    Success, Success
  },

  /////////////////////////////////////////////////////////////////////////////
  // Edge cases of name constraint absolute vs. relative and subdomain matching
  // that are not clearly explained in RFC 5280. (See the long comment above
  // MatchPresentedDNSIDWithReferenceDNSID.)

  // Q: Does a presented identifier equal (case insensitive) to the name
  //    constraint match the constraint? For example, does the presented
  //    ID "host.example.com" match a "host.example.com" constraint?
  { ByteString(), DNSName("host.example.com"),
    GeneralSubtree(DNSName("host.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // This test case is an example from RFC 5280.
    ByteString(), DNSName("host1.example.com"),
    GeneralSubtree(DNSName("host.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { ByteString(), RFC822Name("a@host.example.com"),
    GeneralSubtree(RFC822Name("host.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // This test case is an example from RFC 5280.
    ByteString(), RFC822Name("a@host1.example.com"),
    GeneralSubtree(RFC822Name("host.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },

  // Q: When the name constraint does not start with ".", do subdomain
  //    presented identifiers match it? For example, does the presented
  //    ID "www.host.example.com" match a "host.example.com" constraint?
  { // This test case is an example from RFC 5280.
    ByteString(),  DNSName("www.host.example.com"),
    GeneralSubtree(DNSName(    "host.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // The subdomain matching rule for host names that do not start with "." is
    // different for RFC822Names than for DNSNames!
    ByteString(),  RFC822Name("a@www.host.example.com"),
    GeneralSubtree(RFC822Name(      "host.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE,
    Success
  },

  // Q: When the name constraint does not start with ".", does a
  //    non-subdomain prefix match it? For example, does "bigfoo.bar.com"
  //    match "foo.bar.com"?
  { ByteString(), DNSName("bigfoo.bar.com"),
    GeneralSubtree(DNSName(  "foo.bar.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { ByteString(), RFC822Name("a@bigfoo.bar.com"),
    GeneralSubtree(RFC822Name(    "foo.bar.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },

  // Q: Is a name constraint that starts with "." valid, and if so, what
  //    semantics does it have? For example, does a presented ID of
  //    "www.example.com" match a constraint of ".example.com"? Does a
  //    presented ID of "example.com" match a constraint of ".example.com"?
  { ByteString(), DNSName("www.example.com"),
    GeneralSubtree(DNSName(  ".example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // When there is no Local-part, an RFC822 name constraint's domain may
    // start with '.', and the semantics are the same as for DNSNames.
    ByteString(), RFC822Name("a@www.example.com"),
    GeneralSubtree(RFC822Name(    ".example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // When there is a Local-part, an RFC822 name constraint's domain must not
    // start with '.'.
    ByteString(), RFC822Name("a@www.example.com"),
    GeneralSubtree(RFC822Name(  "a@.example.com")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // Check that we only allow subdomains to match.
    ByteString(), DNSName(  "example.com"),
    GeneralSubtree(DNSName(".example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // Check that we only allow subdomains to match.
    ByteString(), RFC822Name("a@example.com"),
    GeneralSubtree(RFC822Name(".example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // Check that we don't get confused and consider "b" == "."
    ByteString(), DNSName("bexample.com"),
    GeneralSubtree(DNSName(".example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // Check that we don't get confused and consider "b" == "."
    ByteString(), RFC822Name("a@bexample.com"),
    GeneralSubtree(RFC822Name( ".example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },

  // Q: Is there a way to prevent subdomain matches?
  // (This is tested in a different set of tests because it requires a
  // combination of permittedSubtrees and excludedSubtrees.)

  // Q: Are name constraints allowed to be specified as absolute names?
  //    For example, does a presented ID of "example.com" match a name
  //    constraint of "example.com." and vice versa?
  //
  { // The DNSName in the constraint is not valid because constraint DNS IDs
    // are not allowed to be absolute.
    ByteString(), DNSName("example.com"),
    GeneralSubtree(DNSName("example.com.")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { ByteString(), RFC822Name("a@example.com"),
    GeneralSubtree(RFC822Name( "example.com.")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { // The DNSName in the SAN is not valid because presented DNS IDs are not
    // allowed to be absolute.
    ByteString(), DNSName("example.com."),
    GeneralSubtree(DNSName("example.com")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { ByteString(), RFC822Name("a@example.com."),
    GeneralSubtree(RFC822Name( "example.com")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { // The presented DNSName is the same length as the constraint, because the
    // subdomain is only one character long and because the constraint both
    // begins and ends with ".". But, it doesn't matter because absolute names
    // are not allowed for DNSName constraints.
    ByteString(), DNSName("p.example.com"),
    GeneralSubtree(DNSName(".example.com.")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { // The presented DNSName is the same length as the constraint, because the
    // subdomain is only one character long and because the constraint both
    // begins and ends with ".".
    ByteString(), RFC822Name("a@p.example.com"),
    GeneralSubtree(RFC822Name(  ".example.com.")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { // Same as previous test case, but using a wildcard presented ID.
    ByteString(), DNSName("*.example.com"),
    GeneralSubtree(DNSName(".example.com.")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // Same as previous test case, but using a wildcard presented ID, which is
    // invalid in an RFC822Name.
    ByteString(), RFC822Name("a@*.example.com"),
    GeneralSubtree(RFC822Name(  ".example.com.")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },

  // Q: Are "" and "." valid DNSName constraints? If so, what do they mean?
  { ByteString(), DNSName("example.com"),
    GeneralSubtree(DNSName("")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), RFC822Name("a@example.com"),
    GeneralSubtree(RFC822Name("")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // The malformed (absolute) presented ID does not match.
    ByteString(), DNSName("example.com."),
    GeneralSubtree(DNSName("")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("a@example.com."),
    GeneralSubtree(RFC822Name("")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // Invalid syntax in name constraint
    ByteString(), DNSName("example.com"),
    GeneralSubtree(DNSName(".")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { // Invalid syntax in name constraint
    ByteString(), RFC822Name("a@example.com"),
    GeneralSubtree(RFC822Name(".")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER,
  },
  { ByteString(), DNSName("example.com."),
    GeneralSubtree(DNSName(".")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("a@example.com."),
    GeneralSubtree(RFC822Name(".")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },

  /////////////////////////////////////////////////////////////////////////////
  // Basic IP Address constraints (non-CN-ID)

  // The Mozilla CA Policy says this means "no IPv4 addresses allowed."
  { ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_all_zeros_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), IPAddress(ipv4_addr_00000000_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_all_zeros_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), IPAddress(ipv4_addr_FFFFFFFF_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_all_zeros_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },

  // The Mozilla CA Policy says this means "no IPv6 addresses allowed."
  { ByteString(), IPAddress(ipv6_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_all_zeros_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), IPAddress(ipv6_addr_all_zeros_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_all_zeros_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },

  // RFC 5280 doesn't partition IP address constraints into separate IPv4 and
  // IPv6 categories, so a IPv4 permittedSubtrees constraint excludes all IPv6
  // addresses, and vice versa.
  { ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_all_zeros_bytes)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { ByteString(), IPAddress(ipv6_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_all_zeros_bytes)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },

  // IPv4 Subnets
  { ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_CIDR_16_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_CIDR_17_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), IPAddress(ipv4_other_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_CIDR_16_bytes)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // XXX(bug 1089430): We don't reject this even though it is weird.
    ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_CIDR_16_bad_addr_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // XXX(bug 1089430): We don't reject this even though it is weird.
    ByteString(), IPAddress(ipv4_other_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_bad_mask_bytes)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },

  // IPv6 Subnets
  { ByteString(), IPAddress(ipv6_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_CIDR_16_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), IPAddress(ipv6_other_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_CIDR_16_bytes)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // XXX(bug 1089430): We don't reject this even though it is weird.
    ByteString(), IPAddress(ipv6_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_CIDR_16_bad_addr_bytes)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // XXX(bug 1089430): We don't reject this even though it is weird.
    ByteString(), IPAddress(ipv6_other_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_bad_mask_bytes)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },

  // Malformed presented IP addresses and constraints

  { // The presented IPv4 address is empty
    ByteString(), IPAddress(),
    GeneralSubtree(IPAddress(ipv4_constraint_all_zeros_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv4 address is truncated
    ByteString(), IPAddress(ipv4_addr_truncated_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_all_zeros_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv4 address is too long
    ByteString(), IPAddress(ipv4_addr_overlong_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_all_zeros_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv4 constraint is empty
    ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress()),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv4 constraint is truncated
    ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_truncated_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv4 constraint is too long
    ByteString(), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_constraint_overlong_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv6 address is empty
    ByteString(), IPAddress(),
    GeneralSubtree(IPAddress(ipv6_constraint_all_zeros_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv6 address is truncated
    ByteString(), IPAddress(ipv6_addr_truncated_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_all_zeros_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv6 address is too long
    ByteString(), IPAddress(ipv6_addr_overlong_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_all_zeros_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv6 constraint is empty
    ByteString(), IPAddress(ipv6_addr_bytes),
    GeneralSubtree(IPAddress()),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv6 constraint is truncated
    ByteString(), IPAddress(ipv6_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_truncated_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  { // The presented IPv6 constraint is too long
    ByteString(), IPAddress(ipv6_addr_bytes),
    GeneralSubtree(IPAddress(ipv6_constraint_overlong_bytes)),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },

  /////////////////////////////////////////////////////////////////////////////
  // XXX: We don't reject malformed name constraints when there are no names of
  // that type.
  { ByteString(), NO_SAN, GeneralSubtree(DNSName("!")),
    Success, Success
  },
  { ByteString(), NO_SAN, GeneralSubtree(IPAddress(ipv4_addr_overlong_bytes)),
    Success, Success
  },
  { ByteString(), NO_SAN, GeneralSubtree(IPAddress(ipv6_addr_overlong_bytes)),
    Success, Success
  },
  { ByteString(), NO_SAN, GeneralSubtree(RFC822Name("\0")),
    Success, Success
  },

  /////////////////////////////////////////////////////////////////////////////
  // Basic CN-ID DNSName constraint tests.

  { // Empty Name is ignored for DNSName constraints.
    ByteString(), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { // Empty CN is ignored for DNSName constraints because it isn't a
    // syntactically-valid DNSName.
    //
    // NSS gives different results.
    RDN(CN("")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { // IP Address is ignored for DNSName constraints.
    //
    // NSS gives different results.
    RDN(CN("1.2.3.4")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { // OU has something that looks like a dNSName that matches.
    RDN(OU("a.example.com")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { // OU has something that looks like a dNSName that does not match.
    RDN(OU("b.example.com")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { // NSS gives different results.
    RDN(CN("Not a DNSName")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { RDN(CN("a.example.com")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { RDN(CN("b.example.com")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // DNSName CN-ID match is detected when there is a SAN w/o any DNSName or
    // IPAddress
    RDN(CN("a.example.com")), RFC822Name("foo@example.com"),
    GeneralSubtree(DNSName("a.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // DNSName CN-ID mismatch is detected when there is a SAN w/o any DNSName
    // or IPAddress
    RDN(CN("a.example.com")), RFC822Name("foo@example.com"),
    GeneralSubtree(DNSName("b.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // DNSName CN-ID match not reported when there is a DNSName SAN
    RDN(CN("a.example.com")), DNSName("b.example.com"),
    GeneralSubtree(DNSName("a.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // DNSName CN-ID mismatch not reported when there is a DNSName SAN
    RDN(CN("a.example.com")), DNSName("b.example.com"),
    GeneralSubtree(DNSName("b.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE,
  },
  { // DNSName CN-ID match not reported when there is an IPAddress SAN
    RDN(CN("a.example.com")), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { // DNSName CN-ID mismatch not reported when there is an IPAddress SAN
    RDN(CN("a.example.com")), IPAddress(ipv4_addr_bytes),
    GeneralSubtree(DNSName("b.example.com")),
    Success, Success
  },

  { // IPAddress CN-ID match is detected when there is a SAN w/o any DNSName or
    // IPAddress
    RDN(CN(ipv4_addr_str)), RFC822Name("foo@example.com"),
    GeneralSubtree(IPAddress(ipv4_addr_bytes_FFFFFFFF)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // IPAddress CN-ID mismatch is detected when there is a SAN w/o any DNSName
    // or IPAddress
    RDN(CN(ipv4_addr_str)), RFC822Name("foo@example.com"),
    GeneralSubtree(IPAddress(ipv4_other_addr_bytes_FFFFFFFF)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // IPAddress CN-ID match not reported when there is a DNSName SAN
    RDN(CN(ipv4_addr_str)), DNSName("b.example.com"),
    GeneralSubtree(IPAddress(ipv4_addr_bytes_FFFFFFFF)),
    Success, Success
  },
  { // IPAddress CN-ID mismatch not reported when there is a DNSName SAN
    RDN(CN(ipv4_addr_str)), DNSName("b.example.com"),
    GeneralSubtree(IPAddress(ipv4_addr_bytes_FFFFFFFF)),
    Success, Success
  },
  { // IPAddress CN-ID match not reported when there is an IPAddress SAN
    RDN(CN(ipv4_addr_str)), IPAddress(ipv4_other_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_addr_bytes_FFFFFFFF)),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // IPAddress CN-ID mismatch not reported when there is an IPAddress SAN
    RDN(CN(ipv4_addr_str)), IPAddress(ipv4_other_addr_bytes),
    GeneralSubtree(IPAddress(ipv4_other_addr_bytes_FFFFFFFF)),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },

  /////////////////////////////////////////////////////////////////////////////
  // Test that constraints are applied to the most specific (last) CN, and only
  // that CN-ID.

  { // Name constraint only matches a.example.com, but the most specific CN
    // (i.e. the CN-ID) is b.example.com. (Two CNs in one RDN.)
    RDN(CN("a.example.com") + CN("b.example.com")), NO_SAN,
    GeneralSubtree(DNSName("a.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // Name constraint only matches a.example.com, but the most specific CN
    // (i.e. the CN-ID) is b.example.com. (Two CNs in separate RDNs.)
    RDN(CN("a.example.com")) + RDN(CN("b.example.com")), NO_SAN,
    GeneralSubtree(DNSName("a.example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Success
  },
  { // Name constraint only permits b.example.com, and the most specific CN
    // (i.e. the CN-ID) is b.example.com. (Two CNs in one RDN.)
    RDN(CN("a.example.com") + CN("b.example.com")), NO_SAN,
    GeneralSubtree(DNSName("b.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // Name constraint only permits b.example.com, and the most specific CN
    // (i.e. the CN-ID) is b.example.com. (Two CNs in separate RDNs.)
    RDN(CN("a.example.com")) + RDN(CN("b.example.com")), NO_SAN,
    GeneralSubtree(DNSName("b.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },

  /////////////////////////////////////////////////////////////////////////////
  // Additional RFC822 name constraint tests. There are more tests regarding
  // the DNSName part of the constraint mixed into the DNSName constraint
  // tests.

  { ByteString(), RFC822Name("a@example.com"),
    GeneralSubtree(RFC822Name("a@example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },

  // Bug 1056773: name constraints that omit Local-part but include '@' are
  // invalid.
  { ByteString(), RFC822Name("a@example.com"),
    GeneralSubtree(RFC822Name("@example.com")),
    Result::ERROR_BAD_DER,
    Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("@example.com"),
    GeneralSubtree(RFC822Name("@example.com")),
    Result::ERROR_BAD_DER,
    Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("example.com"),
    GeneralSubtree(RFC822Name("@example.com")),
    Result::ERROR_BAD_DER,
    Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("a@mail.example.com"),
    GeneralSubtree(RFC822Name("a@*.example.com")),
    Result::ERROR_BAD_DER,
    Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("a@*.example.com"),
    GeneralSubtree(RFC822Name(".example.com")),
    Result::ERROR_BAD_DER,
    Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("@example.com"),
    GeneralSubtree(RFC822Name(".example.com")),
    Result::ERROR_BAD_DER,
    Result::ERROR_BAD_DER
  },
  { ByteString(), RFC822Name("@a.example.com"),
    GeneralSubtree(RFC822Name(".example.com")),
    Result::ERROR_BAD_DER,
    Result::ERROR_BAD_DER
  },

  /////////////////////////////////////////////////////////////////////////////
  // Test name constraints with underscores.
  //
  { ByteString(), DNSName("uses_underscore.example.com"),
    GeneralSubtree(DNSName("uses_underscore.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), DNSName("uses_underscore.example.com"),
    GeneralSubtree(DNSName("example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), DNSName("a.uses_underscore.example.com"),
    GeneralSubtree(DNSName("uses_underscore.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), RFC822Name("a@uses_underscore.example.com"),
    GeneralSubtree(RFC822Name("uses_underscore.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), RFC822Name("uses_underscore@example.com"),
    GeneralSubtree(RFC822Name("example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), RFC822Name("a@a.uses_underscore.example.com"),
    GeneralSubtree(RFC822Name(".uses_underscore.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },

  /////////////////////////////////////////////////////////////////////////////
  // Name constraint tests that relate to having an empty SAN. According to RFC
  // 5280 this isn't valid, but we allow it for compatibility reasons (see bug
  // 1143085).
  { // For DNSNames, we fall back to the subject CN.
    RDN(CN("a.example.com")), ByteString(),
    GeneralSubtree(DNSName("a.example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // For RFC822Names, we do not fall back to the subject emailAddress.
    // This new implementation seems to conform better to the standards for
    // RFC822 name constraints, by only applying the name constraints to
    // emailAddress names in the certificate subject if there is no
    // subjectAltName extension in the cert.
    // In this case, the presence of the (empty) SAN extension means that RFC822
    // name constraints are not enforced on the emailAddress attributes of the
    // subject.
    RDN(emailAddress("a@example.com")), ByteString(),
    GeneralSubtree(RFC822Name("a@example.com")),
    Success, Success
  },
  { // Compare this to the case where there is no SAN (i.e. the name
    // constraints are enforced, because the extension is not present at all).
    RDN(emailAddress("a@example.com")), NO_SAN,
    GeneralSubtree(RFC822Name("a@example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },

  /////////////////////////////////////////////////////////////////////////////
  // DirectoryName name constraint tests

  { // One AVA per RDN
    RDN(OU("Example Organization")) + RDN(CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization")) +
                                      RDN(CN("example.com"))))),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // RDNs can have multiple AVAs.
    RDN(OU("Example Organization") + CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization") +
                                          CN("example.com"))))),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // The constraint is a prefix of the subject DN.
    RDN(OU("Example Organization")) + RDN(CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization"))))),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // The name constraint is not a prefix of the subject DN.
    // Note that for excludedSubtrees, we simply prohibit any non-empty
    // directoryName constraint to ensure we are not being too lenient.
    RDN(OU("Other Example Organization")) + RDN(CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization")) +
                                      RDN(CN("example.com"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // Same as the previous one, but one RDN with multiple AVAs.
    RDN(OU("Other Example Organization") + CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization") +
                                          CN("example.com"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // With multiple AVAs per RDN in the subject DN, the constraint is not a
    // prefix of the subject DN.
    RDN(OU("Example Organization") + CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // The subject DN RDN has multiple AVAs, but the name constraint has only
    // one AVA per RDN.
    RDN(OU("Example Organization") + CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization")) +
                                      RDN(CN("example.com"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // The name constraint RDN has multiple AVAs, but the subject DN has only
    // one AVA per RDN.
    RDN(OU("Example Organization")) + RDN(CN("example.com")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization") +
                                          CN("example.com"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // In this case, the constraint uses a different encoding from the subject.
    // We consider them to match because we allow UTF8String and
    // PrintableString to compare equal when their contents are equal.
    RDN(OU("Example Organization", der::UTF8String)) + RDN(CN("example.com")),
    NO_SAN, GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization",
                                                     der::PrintableString)) +
                                              RDN(CN("example.com"))))),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // Same as above, but with UTF8String/PrintableString switched.
    RDN(OU("Example Organization", der::PrintableString)) + RDN(CN("example.com")),
    NO_SAN, GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization",
                                                     der::UTF8String)) +
                                              RDN(CN("example.com"))))),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // If the contents aren't the same, then they shouldn't match.
    RDN(OU("Other Example Organization", der::UTF8String)) + RDN(CN("example.com")),
    NO_SAN, GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization",
                                                     der::PrintableString)) +
                                              RDN(CN("example.com"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { // Only UTF8String and PrintableString are considered equivalent.
    RDN(OU("Example Organization", der::PrintableString)) + RDN(CN("example.com")),
    NO_SAN, GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization",
                                                     der::TeletexString)) +
                                              RDN(CN("example.com"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  // Some additional tests for completeness:
  // Ensure that wildcards are handled:
  { RDN(CN("*.example.com")), NO_SAN, GeneralSubtree(DNSName("example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), DNSName("*.example.com"),
    GeneralSubtree(DNSName("example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), DNSName("www.example.com"),
    GeneralSubtree(DNSName("*.example.com")),
    Result::ERROR_BAD_DER, Result::ERROR_BAD_DER
  },
  // Handle multiple name constraint entries:
  { RDN(CN("example.com")), NO_SAN,
    GeneralSubtree(DNSName("example.org")) +
      GeneralSubtree(DNSName("example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { ByteString(), DNSName("example.com"),
    GeneralSubtree(DNSName("example.org")) +
      GeneralSubtree(DNSName("example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  // Handle multiple names in subject alternative name extension:
  { ByteString(), DNSName("example.com") + DNSName("example.org"),
    GeneralSubtree(DNSName("example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  // Handle a mix of DNSName and DirectoryName:
  { RDN(OU("Example Organization")), DNSName("example.com"),
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization"))))) +
      GeneralSubtree(DNSName("example.com")),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { RDN(OU("Other Example Organization")), DNSName("example.com"),
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization"))))) +
      GeneralSubtree(DNSName("example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  { RDN(OU("Example Organization")), DNSName("example.org"),
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization"))))) +
      GeneralSubtree(DNSName("example.com")),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  // Handle a certificate with no DirectoryName:
  { ByteString(), DNSName("example.com"),
    GeneralSubtree(DirectoryName(Name(RDN(OU("Example Organization"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
};

class pkixnames_CheckNameConstraints
  : public ::testing::Test
  , public ::testing::WithParamInterface<NameConstraintParams>
{
public:
  DefaultNameMatchingPolicy mNameMatchingPolicy;
};

TEST_P(pkixnames_CheckNameConstraints,
       NameConstraintsEnforcedForDirectlyIssuedEndEntity)
{
  // Test that name constraints are enforced on a certificate directly issued by
  // a certificate with the given name constraints.

  const NameConstraintParams& param(GetParam());

  ByteString certDER(CreateCert(param.subject, param.subjectAltName));
  ASSERT_FALSE(ENCODING_FAILED(certDER));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(certDER.data(), certDER.length()));
  BackCert cert(certInput, EndEntityOrCA::MustBeEndEntity, nullptr);
  ASSERT_EQ(Success, cert.Init());

  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      PermittedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedPermittedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_serverAuth));
  }
  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      ExcludedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedExcludedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_serverAuth));
  }
  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      PermittedSubtrees(param.subtrees) +
                                      ExcludedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ((param.expectedPermittedSubtreesResult ==
               param.expectedExcludedSubtreesResult)
                ? param.expectedExcludedSubtreesResult
                : Result::ERROR_CERT_NOT_IN_NAME_SPACE,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_serverAuth));
  }
}

INSTANTIATE_TEST_CASE_P(pkixnames_CheckNameConstraints,
                        pkixnames_CheckNameConstraints,
                        testing::ValuesIn(NAME_CONSTRAINT_PARAMS));

// The |subjectAltName| param is not used for these test cases (hence the use of
// "NO_SAN").
static const NameConstraintParams NO_FALLBACK_NAME_CONSTRAINT_PARAMS[] =
{
  // The only difference between end-entities being verified for serverAuth and
  // intermediates or end-entities being verified for other uses is that for
  // the latter cases, there is no fallback matching of DNSName entries to the
  // subject common name.
  { RDN(CN("Not a DNSName")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { RDN(CN("a.example.com")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  { RDN(CN("b.example.com")), NO_SAN, GeneralSubtree(DNSName("a.example.com")),
    Success, Success
  },
  // Sanity-check that name constraints are in fact enforced in these cases.
  { RDN(CN("Example Name")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(CN("Example Name"))))),
    Success, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
  // (In this implementation, if a DirectoryName is in excludedSubtrees, nothing
  // is considered to be in the name space.)
  { RDN(CN("Other Example Name")), NO_SAN,
    GeneralSubtree(DirectoryName(Name(RDN(CN("Example Name"))))),
    Result::ERROR_CERT_NOT_IN_NAME_SPACE, Result::ERROR_CERT_NOT_IN_NAME_SPACE
  },
};

class pkixnames_CheckNameConstraintsOnIntermediate
  : public ::testing::Test
  , public ::testing::WithParamInterface<NameConstraintParams>
{
};

TEST_P(pkixnames_CheckNameConstraintsOnIntermediate,
       NameConstraintsEnforcedOnIntermediate)
{
  // Test that name constraints are enforced on an intermediate certificate
  // directly issued by a certificate with the given name constraints.

  const NameConstraintParams& param(GetParam());

  ByteString certDER(CreateCert(param.subject, NO_SAN,
                                EndEntityOrCA::MustBeCA));
  ASSERT_FALSE(ENCODING_FAILED(certDER));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(certDER.data(), certDER.length()));
  BackCert cert(certInput, EndEntityOrCA::MustBeCA, nullptr);
  ASSERT_EQ(Success, cert.Init());

  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      PermittedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedPermittedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_serverAuth));
  }
  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      ExcludedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedExcludedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_serverAuth));
  }
  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      PermittedSubtrees(param.subtrees) +
                                      ExcludedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedExcludedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_serverAuth));
  }
}

INSTANTIATE_TEST_CASE_P(pkixnames_CheckNameConstraintsOnIntermediate,
                        pkixnames_CheckNameConstraintsOnIntermediate,
                        testing::ValuesIn(NO_FALLBACK_NAME_CONSTRAINT_PARAMS));

class pkixnames_CheckNameConstraintsForNonServerAuthUsage
  : public ::testing::Test
  , public ::testing::WithParamInterface<NameConstraintParams>
{
};

TEST_P(pkixnames_CheckNameConstraintsForNonServerAuthUsage,
       NameConstraintsEnforcedForNonServerAuthUsage)
{
  // Test that for key purposes other than serverAuth, fallback to the subject
  // common name does not occur.

  const NameConstraintParams& param(GetParam());

  ByteString certDER(CreateCert(param.subject, NO_SAN));
  ASSERT_FALSE(ENCODING_FAILED(certDER));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(certDER.data(), certDER.length()));
  BackCert cert(certInput, EndEntityOrCA::MustBeEndEntity, nullptr);
  ASSERT_EQ(Success, cert.Init());

  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      PermittedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedPermittedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_clientAuth));
  }
  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      ExcludedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedExcludedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_clientAuth));
  }
  {
    ByteString nameConstraintsDER(TLV(der::SEQUENCE,
                                      PermittedSubtrees(param.subtrees) +
                                      ExcludedSubtrees(param.subtrees)));
    Input nameConstraints;
    ASSERT_EQ(Success,
              nameConstraints.Init(nameConstraintsDER.data(),
                                   nameConstraintsDER.length()));
    ASSERT_EQ(param.expectedExcludedSubtreesResult,
              CheckNameConstraints(nameConstraints, cert,
                                   KeyPurposeId::id_kp_clientAuth));
  }
}

INSTANTIATE_TEST_CASE_P(pkixnames_CheckNameConstraintsForNonServerAuthUsage,
                        pkixnames_CheckNameConstraintsForNonServerAuthUsage,
                        testing::ValuesIn(NO_FALLBACK_NAME_CONSTRAINT_PARAMS));
