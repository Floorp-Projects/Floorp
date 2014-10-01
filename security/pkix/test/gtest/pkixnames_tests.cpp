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

#include <cstring>

#include "pkix/Input.h"
#include "pkixgtest.h"
#include "pkixtestutil.h"

namespace mozilla { namespace pkix {

bool PresentedDNSIDMatchesReferenceDNSID(Input presentedDNSID,
                                         Input referenceDNSID,
                                         bool referenceDNSIDWasVerifiedAsValid);

bool IsValidDNSName(Input hostname);
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
  bool isValid;
};

// str is null-terminated, which is why we subtract 1. str may contain embedded
// nulls (including at the end) preceding the null terminator though.
#define I(str, valid) \
  { \
    ByteString(reinterpret_cast<const uint8_t*>(str), sizeof(str) - 1), valid \
  }

static const InputValidity DNSNAMES_VALIDITY[] =
{
  I("a", true),
  I("a.b", true),
  I("a.b.c", true),
  I("a.b.c.d", true),

  // empty labels
  I("", false),
  I(".", false),
  I("a", true),
  I(".a", false), // PresentedDNSIDMatchesReferenceDNSID depends on this
  I(".a.b", false), // PresentedDNSIDMatchesReferenceDNSID depends on this
  I("..a", false),
  I("a..b", false),
  I("a...b", false),
  I("a..b.c", false),
  I("a.b..c", false),
  I(".a.b.c.", false),

  // absolute names
  I("a.", true),
  I("a.b.", true),
  I("a.b.c.", true),

  // absolute names with empty label at end
  I("a..", false),
  I("a.b..", false),
  I("a.b.c..", false),
  I("a...", false),

  // Punycode
  I("xn--", false),
  I("xn--.", false),
  I("xn--.a", false),
  I("a.xn--", false),
  I("a.xn--.", false),
  I("a.xn--.b", false),
  I("a.xn--.b", false),
  I("a.xn--\0.b", false),
  I("a.xn--a.b", true),
  I("xn--a", true),
  I("a.xn--a", true),
  I("a.xn--a.a", true),
  I("\0xc4\0x95.com", false), // UTF-8 ĕ
  I("xn--jea.com", true), // punycode ĕ
  I("xn--\0xc4\0x95.com", false), // UTF-8 ĕ, malformed punycode + UTF-8 mashup

  // Surprising punycode
  I("xn--google.com", true), // 䕮䕵䕶䕱.com
  I("xn--citibank.com", true), // 岍岊岊岅岉岎.com
  I("xn--cnn.com", true), // 䁾.com
  I("a.xn--cnn", true), // a.䁾
  I("a.xn--cnn.com", true), // a.䁾.com

  I("1.2.3.4", false), // IPv4 address
  I("1::2", false), // IPV6 address

  // whitespace not allowed anywhere.
  I(" ", false),
  I(" a", false),
  I("a ", false),
  I("a b", false),
  I("a.b 1", false),
  I("a\t", false),

  // Nulls not allowed
  I("\0", false),
  I("a\0", false),
  I("example.org\0.example.com", false), // Hi Moxie!
  I("\0a", false),
  I("xn--\0", false),

  // Allowed character set
  I("a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z", true),
  I("A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.V.W.X.Y.Z", true),
  I("0.1.2.3.4.5.6.7.8.9.a", true), // "a" needed to avoid numeric last label
  I("a-b", true), // hyphen (a label cannot start or end with a hyphen)

  // An invalid character in various positions
  I("!", false),
  I("!a", false),
  I("a!", false),
  I("a!b", false),
  I("a.!", false),
  I("a.a!", false),
  I("a.!a", false),
  I("a.a!a", false),
  I("a.!a.a", false),
  I("a.a!.a", false),
  I("a.a!a.a", false),

  // Various other invalid characters
  I("a!", false),
  I("a@", false),
  I("a#", false),
  I("a$", false),
  I("a%", false),
  I("a^", false),
  I("a&", false),
  I("a*", false),
  I("a(", false),
  I("a)", false),

  // last label can't be fully numeric
  I("1", false),
  I("a.1", false),

  // other labels can be fully numeric
  I("1.a", true),
  I("1.2.a", true),
  I("1.2.3.a", true),

  // last label can be *partly* numeric
  I("1a", true),
  I("1.1a", true),
  I("1-1", true),
  I("a.1-1", true),
  I("a.1-a", true),

  // labels cannot start with a hyphen
  I("-", false),
  I("-1", false),

  // labels cannot end with a hyphen
  I("1-", false),
  I("1-.a", false),
  I("a-", false),
  I("a-.a", false),
  I("a.1-.a", false),
  I("a.a-.a", false),

  // labels can contain a hyphen in the middle
  I("a-b", true),
  I("1-2", true),
  I("a.a-1", true),

  // multiple consecutive hyphens allowed
  I("a--1", true),
  I("1---a", true),
  I("a-----------------b", true),

  // Wildcard specifications are not valid DNS names
  I("*.a", false),
  I("a*", false),
  I("a*.a", false),

  // Redacted labels from RFC6962bis draft 4
  // https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-04#section-3.2.2
  I("(PRIVATE).foo", false),

  // maximum label length is 63 characters
  I("1234567890" "1234567890" "1234567890"
    "1234567890" "1234567890" "1234567890" "abc", true),
  I("1234567890" "1234567890" "1234567890"
    "1234567890" "1234567890" "1234567890" "abcd", false),

  // maximum total length is 253 characters
  I("1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "12345678" "a",
    true),
  I("1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "1234567890" "."
    "1234567890" "1234567890" "1234567890" "1234567890" "123456789" "a",
    false),
};

static const InputValidity DNSNAMES_VALIDITY_TURKISH_I[] =
{
  // http://en.wikipedia.org/wiki/Dotted_and_dotless_I#In_computing
  // IDN registration rules disallow "latin capital letter i with dot above,"
  // but our checks aren't intended to enforce those rules.
  I("I", true), // ASCII capital I
  I("i", true), // ASCII lowercase i
  I("\0xC4\0xB0", false), // latin capital letter i with dot above
  I("\0xC4\0xB1", false), // latin small letter dotless i
  I("xn--i-9bb", true), // latin capital letter i with dot above, in punycode
  I("xn--cfa", true), // latin small letter dotless i, in punycode
  I("xn--\0xC4\0xB0", false), // latin capital letter i with dot above, mashup
  I("xn--\0xC4\0xB1", false), // latin small letter dotless i, mashup
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
  bool referenceIsValidDNSName = IsValidDNSName(reference);
  ASSERT_TRUE(referenceIsValidDNSName); // sanity check that test makes sense
  ASSERT_EQ(param.matches,
            PresentedDNSIDMatchesReferenceDNSID(presented, reference,
                                                referenceIsValidDNSName));
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
  ASSERT_EQ(isASCII, PresentedDNSIDMatchesReferenceDNSID(input, LOWERCASE_I,
                                                         true));
  ASSERT_EQ(isASCII, PresentedDNSIDMatchesReferenceDNSID(input, UPPERCASE_I,
                                                         true));
}

INSTANTIATE_TEST_CASE_P(pkixnames_Turkish_I_Comparison,
                        pkixnames_Turkish_I_Comparison,
                        testing::ValuesIn(DNSNAMES_VALIDITY_TURKISH_I));

class pkixnames_IsValidDNSName
  : public ::testing::Test
  , public ::testing::WithParamInterface<InputValidity>
{
};

TEST_P(pkixnames_IsValidDNSName, IsValidDNSName)
{
  const InputValidity& inputValidity(GetParam());
  SCOPED_TRACE(inputValidity.input.c_str());
  Input input;
  ASSERT_EQ(Success, input.Init(inputValidity.input.data(),
                                inputValidity.input.length()));
  ASSERT_EQ(inputValidity.isValid, IsValidDNSName(input));
}

INSTANTIATE_TEST_CASE_P(pkixnames_IsValidDNSName,
                        pkixnames_IsValidDNSName,
                        testing::ValuesIn(DNSNAMES_VALIDITY));
INSTANTIATE_TEST_CASE_P(pkixnames_IsValidDNSName_Turkish_I,
                        pkixnames_IsValidDNSName,
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
