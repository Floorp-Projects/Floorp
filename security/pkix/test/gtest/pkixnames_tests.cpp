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
#include "pkix/pkix.h"
#include "pkixgtest.h"
#include "pkixtestutil.h"

namespace mozilla { namespace pkix {

bool PresentedDNSIDMatchesReferenceDNSID(Input presentedDNSID,
                                         Input referenceDNSID);

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
  bool matches;
};

#define DNS_ID_MATCH(a, b) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(a), sizeof(a) - 1), \
    ByteString(reinterpret_cast<const uint8_t*>(b), sizeof(b) - 1), \
    true \
  }

#define DNS_ID_MISMATCH(a, b) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(a), sizeof(a) - 1), \
    ByteString(reinterpret_cast<const uint8_t*>(b), sizeof(b) - 1), \
    false \
  }

static const PresentedMatchesReference DNSID_MATCH_PARAMS[] =
{
  DNS_ID_MISMATCH("", "a"),

  DNS_ID_MATCH("a", "a"),
  DNS_ID_MISMATCH("b", "a"),

  DNS_ID_MATCH("*.b.a", "c.b.a"),
  DNS_ID_MISMATCH("*.b.a", "b.a"),
  DNS_ID_MISMATCH("*.b.a", "b.a."),

  // Wildcard not in leftmost label
  DNS_ID_MATCH("d.c.b.a", "d.c.b.a"),
  DNS_ID_MISMATCH("d.*.b.a", "d.c.b.a"),
  DNS_ID_MISMATCH("d.c*.b.a", "d.c.b.a"),
  DNS_ID_MISMATCH("d.c*.b.a", "d.cc.b.a"),

  // case sensitivity
  DNS_ID_MATCH("abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
  DNS_ID_MATCH("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"),
  DNS_ID_MATCH("aBc", "Abc"),

  // digits
  DNS_ID_MATCH("a1", "a1"),

  // A trailing dot indicates an absolute name, and absolute names can match
  // relative names, and vice-versa.
  DNS_ID_MATCH("example", "example"),
  DNS_ID_MATCH("example.", "example."),
  DNS_ID_MATCH("example", "example."),
  DNS_ID_MATCH("example.", "example"),
  DNS_ID_MATCH("example.com", "example.com"),
  DNS_ID_MATCH("example.com.", "example.com."),
  DNS_ID_MATCH("example.com", "example.com."),
  DNS_ID_MATCH("example.com.", "example.com"),
  DNS_ID_MISMATCH("example.com..", "example.com."),
  DNS_ID_MISMATCH("example.com..", "example.com"),
  DNS_ID_MISMATCH("example.com...", "example.com."),

  // xn-- IDN prefix
  DNS_ID_MATCH("x*.b.a", "xa.b.a"),
  DNS_ID_MATCH("x*.b.a", "xna.b.a"),
  DNS_ID_MATCH("x*.b.a", "xn-a.b.a"),
  DNS_ID_MISMATCH("x*.b.a", "xn--a.b.a"),
  DNS_ID_MISMATCH("xn*.b.a", "xn--a.b.a"),
  DNS_ID_MISMATCH("xn-*.b.a", "xn--a.b.a"),
  DNS_ID_MISMATCH("xn--*.b.a", "xn--a.b.a"),
  DNS_ID_MISMATCH("xn*.b.a", "xn--a.b.a"),
  DNS_ID_MISMATCH("xn-*.b.a", "xn--a.b.a"),
  DNS_ID_MISMATCH("xn--*.b.a", "xn--a.b.a"),
  DNS_ID_MISMATCH("xn---*.b.a", "xn--a.b.a"),

  // "*" cannot expand to nothing.
  DNS_ID_MISMATCH("c*.b.a", "c.b.a"),

  // --------------------------------------------------------------------------
  // The rest of these are test cases adapted from Chromium's
  // x509_certificate_unittest.cc. The parameter order is the opposite in
  // Chromium's tests. Also, they some tests were modified to fit into this
  // framework or due to intentional differences between mozilla::pkix and
  // Chromium.

  DNS_ID_MATCH("foo.com", "foo.com"),
  DNS_ID_MATCH("f", "f"),
  DNS_ID_MISMATCH("i", "h"),
  DNS_ID_MATCH("*.foo.com", "bar.foo.com"),
  DNS_ID_MATCH("*.test.fr", "www.test.fr"),
  DNS_ID_MATCH("*.test.FR", "wwW.tESt.fr"),
  DNS_ID_MISMATCH(".uk", "f.uk"),
  DNS_ID_MISMATCH("?.bar.foo.com", "w.bar.foo.com"),
  DNS_ID_MISMATCH("(www|ftp).foo.com", "www.foo.com"), // regex!
  DNS_ID_MISMATCH("www.foo.com\0", "www.foo.com"),
  DNS_ID_MISMATCH("www.foo.com\0*.foo.com", "www.foo.com"),
  DNS_ID_MISMATCH("ww.house.example", "www.house.example"),
  DNS_ID_MISMATCH("www.test.org", "test.org"),
  DNS_ID_MISMATCH("*.test.org", "test.org"),
  DNS_ID_MISMATCH("*.org", "test.org"),
  DNS_ID_MISMATCH("w*.bar.foo.com", "w.bar.foo.com"),
  DNS_ID_MISMATCH("ww*ww.bar.foo.com", "www.bar.foo.com"),
  DNS_ID_MISMATCH("ww*ww.bar.foo.com", "wwww.bar.foo.com"),

  // Different than Chromium, matches NSS.
  DNS_ID_MISMATCH("w*w.bar.foo.com", "wwww.bar.foo.com"),

  DNS_ID_MISMATCH("w*w.bar.foo.c0m", "wwww.bar.foo.com"),

  DNS_ID_MATCH("wa*.bar.foo.com", "WALLY.bar.foo.com"),

  // We require "*" to be the last character in a wildcard label, but
  // Chromium does not.
  DNS_ID_MISMATCH("*Ly.bar.foo.com", "wally.bar.foo.com"),

  // Chromium does URL decoding of the reference ID, but we don't, and we also
  // require that the reference ID is valid, so we can't test these two.
  // DNS_ID_MATCH("www.foo.com", "ww%57.foo.com"),
  // DNS_ID_MATCH("www&.foo.com", "www%26.foo.com"),

  DNS_ID_MISMATCH("*.test.de", "www.test.co.jp"),
  DNS_ID_MISMATCH("*.jp", "www.test.co.jp"),
  DNS_ID_MISMATCH("www.test.co.uk", "www.test.co.jp"),
  DNS_ID_MISMATCH("www.*.co.jp", "www.test.co.jp"),
  DNS_ID_MATCH("www.bar.foo.com", "www.bar.foo.com"),
  DNS_ID_MISMATCH("*.foo.com", "www.bar.foo.com"),
  DNS_ID_MISMATCH("*.*.foo.com", "www.bar.foo.com"),
  DNS_ID_MISMATCH("*.*.foo.com", "www.bar.foo.com"),

  // Our matcher requires the reference ID to be a valid DNS name, so we cannot
  // test this case.
  // DNS_ID_MISMATCH("*.*.bar.foo.com", "*..bar.foo.com"),

  DNS_ID_MATCH("www.bath.org", "www.bath.org"),

  // Our matcher requires the reference ID to be a valid DNS name, so we cannot
  // test these cases.
  // DNS_ID_MISMATCH("www.bath.org", ""),
  // DNS_ID_MISMATCH("www.bath.org", "20.30.40.50"),
  // DNS_ID_MISMATCH("www.bath.org", "66.77.88.99"),

  // IDN tests
  DNS_ID_MATCH("xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_MATCH("*.xn--poema-9qae5a.com.br", "www.xn--poema-9qae5a.com.br"),
  DNS_ID_MISMATCH("*.xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_MISMATCH("xn--poema-*.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_MISMATCH("xn--*-9qae5a.com.br", "xn--poema-9qae5a.com.br"),
  DNS_ID_MISMATCH("*--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br"),

  // The following are adapted from the examples quoted from
  //   http://tools.ietf.org/html/rfc6125#section-6.4.3
  // (e.g., *.example.com would match foo.example.com but
  // not bar.foo.example.com or example.com).
  DNS_ID_MATCH("*.example.com", "foo.example.com"),
  DNS_ID_MISMATCH("*.example.com", "bar.foo.example.com"),
  DNS_ID_MISMATCH("*.example.com", "example.com"),
  // (e.g., baz*.example.net and *baz.example.net and b*z.example.net would
  // be taken to match baz1.example.net and foobaz.example.net and
  // buzz.example.net, respectively
  DNS_ID_MATCH("baz*.example.net", "baz1.example.net"),

  // Both of these are different from Chromium, but match NSS, becaues the
  // wildcard character "*" is not the last character of the label.
  DNS_ID_MISMATCH("*baz.example.net", "foobaz.example.net"),
  DNS_ID_MISMATCH("b*z.example.net", "buzz.example.net"),

  // Wildcards should not be valid for public registry controlled domains,
  // and unknown/unrecognized domains, at least three domain components must
  // be present. For mozilla::pkix and NSS, there must always be at least two
  // labels after the wildcard label.
  DNS_ID_MATCH("*.test.example", "www.test.example"),
  DNS_ID_MATCH("*.example.co.uk", "test.example.co.uk"),
  DNS_ID_MISMATCH("*.exmaple", "test.example"),

  // The result is different than Chromium, because Chromium takes into account
  // the additional knowledge it has that "co.uk" is a TLD. mozilla::pkix does
  // not know that.
  DNS_ID_MATCH("*.co.uk", "example.co.uk"),

  DNS_ID_MISMATCH("*.com", "foo.com"),
  DNS_ID_MISMATCH("*.us", "foo.us"),
  DNS_ID_MISMATCH("*", "foo"),

  // IDN variants of wildcards and registry controlled domains.
  DNS_ID_MATCH("*.xn--poema-9qae5a.com.br", "www.xn--poema-9qae5a.com.br"),
  DNS_ID_MATCH("*.example.xn--mgbaam7a8h", "test.example.xn--mgbaam7a8h"),

  // RFC6126 allows this, and NSS accepts it, but Chromium disallows it.
  // TODO: File bug against Chromium.
  DNS_ID_MATCH("*.com.br", "xn--poema-9qae5a.com.br"),

  DNS_ID_MISMATCH("*.xn--mgbaam7a8h", "example.xn--mgbaam7a8h"),
  // Wildcards should be permissible for 'private' registry-controlled
  // domains. (In mozilla::pkix, we do not know if it is a private registry-
  // controlled domain or not.)
  DNS_ID_MATCH("*.appspot.com", "www.appspot.com"),
  DNS_ID_MATCH("*.s3.amazonaws.com", "foo.s3.amazonaws.com"),

  // Multiple wildcards are not valid.
  DNS_ID_MISMATCH("*.*.com", "foo.example.com"),
  DNS_ID_MISMATCH("*.bar.*.com", "foo.bar.example.com"),

  // Absolute vs relative DNS name tests. Although not explicitly specified
  // in RFC 6125, absolute reference names (those ending in a .) should
  // match either absolute or relative presented names.
  // TODO: File errata against RFC 6125 about this.
  DNS_ID_MATCH("foo.com.", "foo.com"),
  DNS_ID_MATCH("foo.com", "foo.com."),
  DNS_ID_MATCH("foo.com.", "foo.com."),
  DNS_ID_MATCH("f.", "f"),
  DNS_ID_MATCH("f", "f."),
  DNS_ID_MATCH("f.", "f."),
  DNS_ID_MATCH("*.bar.foo.com.", "www-3.bar.foo.com"),
  DNS_ID_MATCH("*.bar.foo.com", "www-3.bar.foo.com."),
  DNS_ID_MATCH("*.bar.foo.com.", "www-3.bar.foo.com."),

  // We require the reference ID to be a valid DNS name, so we cannot test this
  // case.
  // DNS_ID_MISMATCH(".", "."),

  DNS_ID_MISMATCH("*.com.", "example.com"),
  DNS_ID_MISMATCH("*.com", "example.com."),
  DNS_ID_MISMATCH("*.com.", "example.com."),
  DNS_ID_MISMATCH("*.", "foo."),
  DNS_ID_MISMATCH("*.", "foo"),

  // The result is different than Chromium because we don't know that co.uk is
  // a TLD.
  DNS_ID_MATCH("*.co.uk.", "foo.co.uk"),
  DNS_ID_MATCH("*.co.uk.", "foo.co.uk."),
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
  I(".a", false, false), // PresentedDNSIDMatchesReferenceDNSID depends on this
  I(".a.b", false, false), // PresentedDNSIDMatchesReferenceDNSID depends on this
  I("..a", false, false),
  I("a..b", false, false),
  I("a...b", false, false),
  I("a..b.c", false, false),
  I("a.b..c", false, false),
  I(".a.b.c.", false, false),

  // absolute names
  I("a.", true, true),
  I("a.b.", true, true),
  I("a.b.c.", true, true),

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
  I("\0xc4\0x95.com", false, false), // UTF-8 ĕ
  I("xn--jea.com", true, true), // punycode ĕ
  I("xn--\0xc4\0x95.com", false, false), // UTF-8 ĕ, malformed punycode + UTF-8 mashup

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
  // presented names if there are enough labels
  I("*.a", false, false),
  I("a*", false, false),
  I("a*.", false, false),
  I("a*.a", false, false),
  I("a*.a.", false, false),
  I("*.a.b", false, true),
  I("*.a.b.", false, true),
  I("a*.b.c", false, true),
  I("*.a.b.c", false, true),
  I("a*.b.c.d", false, true),

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
  I("x*.a.b", false, true),
  I("xn*.a.b", false, true),
  I("xn-*.a.b", false, true),
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
  I("\0xC4\0xB0", false, false), // latin capital letter i with dot above
  I("\0xC4\0xB1", false, false), // latin small letter dotless i
  I("xn--i-9bb", true, true), // latin capital letter i with dot above, in punycode
  I("xn--cfa", true, true), // latin small letter dotless i, in punycode
  I("xn--\0xC4\0xB0", false, false), // latin capital letter i with dot above, mashup
  I("xn--\0xC4\0xB1", false, false), // latin small letter dotless i, mashup
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

class pkixnames_PresentedDNSIDMatchesReferenceDNSID
  : public ::testing::Test
  , public ::testing::WithParamInterface<PresentedMatchesReference>
{
};

TEST_P(pkixnames_PresentedDNSIDMatchesReferenceDNSID,
       PresentedDNSIDMatchesReferenceDNSID)
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

  ASSERT_EQ(param.matches,
            PresentedDNSIDMatchesReferenceDNSID(presented, reference));
}

INSTANTIATE_TEST_CASE_P(pkixnames_PresentedDNSIDMatchesReferenceDNSID,
                        pkixnames_PresentedDNSIDMatchesReferenceDNSID,
                        testing::ValuesIn(DNSID_MATCH_PARAMS));

class pkixnames_Turkish_I_Comparison
  : public ::testing::Test
  , public ::testing::WithParamInterface<InputValidity>
{
};

TEST_P(pkixnames_Turkish_I_Comparison, PresentedDNSIDMatchesReferenceDNSID)
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
  ASSERT_EQ(isASCII, PresentedDNSIDMatchesReferenceDNSID(input, LOWERCASE_I));
  ASSERT_EQ(isASCII, PresentedDNSIDMatchesReferenceDNSID(input, UPPERCASE_I));
}

INSTANTIATE_TEST_CASE_P(pkixnames_Turkish_I_Comparison,
                        pkixnames_Turkish_I_Comparison,
                        testing::ValuesIn(DNSNAMES_VALIDITY_TURKISH_I));

class pkixnames_IsValidReferenceDNSID
  : public ::testing::Test
  , public ::testing::WithParamInterface<InputValidity>
{
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
static const uint8_t ipv4_addr_bytes_as_str[] = "\0x01\x02\0x03\0x04";
static const uint8_t ipv4_addr_str[] = "1.2.3.4";

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
  "\0x11\0x22\0x33\0x44"
  "\0x55\0x66\0x77\0x88"
  "\0x99\0xaa\0xbb\0xcc"
  "\0xdd\0xee\0xff\0x11";

static const uint8_t ipv6_addr_str[] =
  "1122:3344:5566:7788:99aa:bbcc:ddee:ff11";

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
  // Match a DNSName SAN entry with a redundant (ignored) matching CN-ID.
  WITH_SAN("a", RDN(CN("a")), DNSName("a"), Success),
  // Match a DNSName SAN entry when there is an CN-ID that doesn't match.
  WITH_SAN("b", RDN(CN("a")), DNSName("b"), Success),
  // Do not match a CN-ID when there is a valid DNSName SAN Entry.
  WITH_SAN("a", RDN(CN("a")), DNSName("b"), Result::ERROR_BAD_CERT_DOMAIN),
  // Do not match a CN-ID when there is a malformed DNSName SAN Entry.
  WITH_SAN("a", RDN(CN("a")), DNSName("!"), Result::ERROR_BAD_CERT_DOMAIN),
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
  WITH_SAN("b", RDN(CN("foo")), DNSName("!") + DNSName("b"), Success),

  // http://tools.ietf.org/html/rfc5280#section-4.2.1.6: "If the subjectAltName
  // extension is present, the sequence MUST contain at least one entry."
  WITH_SAN("a", RDN(CN("a")), ByteString(), Result::ERROR_BAD_DER),

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

  // Do not match a DNSName that is encoded in a malformed IPAddress.
  WITH_SAN("example.com", RDN(CN("foo")), IPAddress(example_com),
           Result::ERROR_BAD_CERT_DOMAIN),

  // We skip over the malformed IPAddress and match the DNSName entry because
  // we've heard reports of real-world certificates that have malformed
  // IPAddress SANs.
  WITH_SAN("example.org", RDN(CN("foo")),
           IPAddress(example_com) + DNSName("example.org"), Success),

  // We skip over malformed DNSName entries too.
  WITH_SAN("example.com", RDN(CN("foo")),
           DNSName("!") + DNSName("example.com"), Success),

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
};

ByteString
CreateCert(const ByteString& subject, const ByteString& subjectAltName)
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

  ScopedTestKeyPair keyPair(CloneReusedKeyPair());
  return CreateEncodedCertificate(
                    v3, sha256WithRSAEncryption, serialNumber, issuerDER,
                    oneDayBeforeNow, oneDayAfterNow, Name(subject), *keyPair,
                    extensions, *keyPair, sha256WithRSAEncryption);
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

  ASSERT_EQ(param.result, CheckCertHostname(certInput, hostnameInput));
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
                       v3, sha256WithRSAEncryption, serialNumber,
                       Name(RDN(CN("issuer"))), oneDayBeforeNow, oneDayAfterNow,
                       Name(RDN(CN("a"))), *keyPair, extensions,
                       *keyPair, sha256WithRSAEncryption));
  ASSERT_FALSE(ENCODING_FAILED(certDER));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(certDER.data(), certDER.length()));

  static const uint8_t a[] = { 'a' };
  ASSERT_EQ(Result::ERROR_EXTENSION_VALUE_INVALID,
            CheckCertHostname(certInput, Input(a)));
}

class pkixnames_CheckCertHostname_PresentedMatchesReference
  : public ::testing::Test
  , public ::testing::WithParamInterface<PresentedMatchesReference>
{
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

  ASSERT_EQ(param.matches ? Success : Result::ERROR_BAD_CERT_DOMAIN,
            CheckCertHostname(certInput, hostnameInput));
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

  ASSERT_EQ(param.matches ? Success : Result::ERROR_BAD_CERT_DOMAIN,
            CheckCertHostname(certInput, hostnameInput));
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

  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, UPPERCASE_I));
  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, LOWERCASE_I));
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

  Result expectedResult = (InputsAreEqual(LOWERCASE_I, input) ||
                           InputsAreEqual(UPPERCASE_I, input))
                        ? Success
                        : Result::ERROR_BAD_CERT_DOMAIN;

  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, UPPERCASE_I));
  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, LOWERCASE_I));
}

class pkixnames_CheckCertHostname_IPV4_Addresses
  : public ::testing::Test
  , public ::testing::WithParamInterface<IPAddressParams<4>>
{
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
            CheckCertHostname(certInput, hostnameInput));
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

  ASSERT_EQ(expectedResult, CheckCertHostname(certInput, hostnameInput));
}

INSTANTIATE_TEST_CASE_P(pkixnames_CheckCertHostname_IPV4_ADDRESSES,
                        pkixnames_CheckCertHostname_IPV4_Addresses,
                        testing::ValuesIn(IPV4_ADDRESSES));

