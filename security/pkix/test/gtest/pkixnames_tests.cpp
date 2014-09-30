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

bool IsValidDNSName(Input hostname);

} } // namespace mozilla::pkix

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

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
  I(".a", false),
  I(".a.b", false),
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
