/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <ctype.h>

#include <mozilla/RefPtr.h>
#include "nsString.h"
#include "nsEscape.h"
#include "nsUrlClassifierUtils.h"
#include "stdlib.h"
#include "gtest/gtest.h"

static char int_to_hex_digit(int32_t i) {
  NS_ASSERTION((i >= 0) && (i <= 15), "int too big in int_to_hex_digit");
  return static_cast<char>(((i < 10) ? (i + '0') : ((i - 10) + 'A')));
}

static void CheckEquals(nsCString& expected, nsCString& actual)
{
  ASSERT_TRUE((expected).Equals((actual)));
}

void TestUnescapeHelper(const char* in, const char* expected)
{
  nsCString out, strIn(in), strExp(expected);

  NS_UnescapeURL(strIn.get(), strIn.Length(), esc_AlwaysCopy, out);
  CheckEquals(strExp, out);
}

// Make sure Unescape from nsEncode.h's unescape does what the server does.
TEST(UrlClassifierUtils, Unescape)
{
  // test empty string
  TestUnescapeHelper("\0", "\0");

  // Test docoding of all characters.
  nsCString allCharsEncoded, allCharsEncodedLowercase, allCharsAsString;
  for (int32_t i = 1; i < 256; ++i) {
    allCharsEncoded.Append('%');
    allCharsEncoded.Append(int_to_hex_digit(i / 16));
    allCharsEncoded.Append((int_to_hex_digit(i % 16)));

    allCharsEncodedLowercase.Append('%');
    allCharsEncodedLowercase.Append(tolower(int_to_hex_digit(i / 16)));
    allCharsEncodedLowercase.Append(tolower(int_to_hex_digit(i % 16)));

    allCharsAsString.Append(static_cast<char>(i));
  }

  nsCString out;
  NS_UnescapeURL(allCharsEncoded.get(),
                 allCharsEncoded.Length(),
                 esc_AlwaysCopy,
                 out);

  CheckEquals(allCharsAsString, out);

  out.Truncate();
  NS_UnescapeURL(allCharsEncodedLowercase.get(),
                 allCharsEncodedLowercase.Length(),
                 esc_AlwaysCopy,
                 out);
  CheckEquals(allCharsAsString, out);

  // Test %-related edge cases
  TestUnescapeHelper("%", "%");
  TestUnescapeHelper("%xx", "%xx");
  TestUnescapeHelper("%%", "%%");
  TestUnescapeHelper("%%%", "%%%");
  TestUnescapeHelper("%%%%", "%%%%");
  TestUnescapeHelper("%1", "%1");
  TestUnescapeHelper("%1z", "%1z");
  TestUnescapeHelper("a%1z", "a%1z");
  TestUnescapeHelper("abc%d%e%fg%hij%klmno%", "abc%d%e%fg%hij%klmno%");

  // A few more tests
  TestUnescapeHelper("%25", "%");
  TestUnescapeHelper("%25%32%35", "%25");
}

void TestEncodeHelper(const char* in, const char* expected)
{
  nsCString out, strIn(in), strExp(expected);
  RefPtr<nsUrlClassifierUtils> utils = new nsUrlClassifierUtils;
  utils->Init();

  utils->SpecialEncode(strIn, true, out);
  CheckEquals(strExp, out);
}

TEST(UrlClassifierUtils, Enc)
{
  // Test empty string
  TestEncodeHelper("", "");

  // Test that all characters we shouldn't encode ([33-36],[38,126]) are not.
  nsCString noenc;
  for (int32_t i = 33; i < 127; i++) {
    if (i != 37) {                      // skip %
      noenc.Append(static_cast<char>(i));
    }
  }
  RefPtr<nsUrlClassifierUtils> utils = new nsUrlClassifierUtils;
  utils->Init();
  nsCString out;
  utils->SpecialEncode(noenc, false, out);
  CheckEquals(noenc, out);

  // Test that all the chars that we should encode [0,32],37,[127,255] are
  nsCString yesAsString, yesExpectedString;
  for (int32_t i = 1; i < 256; i++) {
    if (i < 33 || i == 37 || i > 126) {
      yesAsString.Append(static_cast<char>(i));
      yesExpectedString.Append('%');
      yesExpectedString.Append(int_to_hex_digit(i / 16));
      yesExpectedString.Append(int_to_hex_digit(i % 16));
    }
  }

  out.Truncate();
  utils->SpecialEncode(yesAsString, false, out);
  CheckEquals(yesExpectedString, out);

  TestEncodeHelper("blah//blah", "blah/blah");
}

void TestCanonicalizeHelper(const char* in, const char* expected)
{
  nsCString out, strIn(in), strExp(expected);
  RefPtr<nsUrlClassifierUtils> utils = new nsUrlClassifierUtils;
  utils->Init();

  utils->CanonicalizePath(strIn, out);
  CheckEquals(strExp, out);
}

TEST(UrlClassifierUtils, Canonicalize)
{
  // Test repeated %-decoding. Note: %25 --> %, %32 --> 2, %35 --> 5
  TestCanonicalizeHelper("%25", "%25");
  TestCanonicalizeHelper("%25%32%35", "%25");
  TestCanonicalizeHelper("asdf%25%32%35asd", "asdf%25asd");
  TestCanonicalizeHelper("%%%25%32%35asd%%", "%25%25%25asd%25%25");
  TestCanonicalizeHelper("%25%32%35%25%32%35%25%32%35", "%25%25%25");
  TestCanonicalizeHelper("%25", "%25");
  TestCanonicalizeHelper("%257Ea%2521b%2540c%2523d%2524e%25f%255E00%252611%252A22%252833%252944_55%252B",
      "~a!b@c#d$e%25f^00&11*22(33)44_55+");

  TestCanonicalizeHelper("", "");
  TestCanonicalizeHelper("%31%36%38%2e%31%38%38%2e%39%39%2e%32%36/%2E%73%65%63%75%72%65/%77%77%77%2E%65%62%61%79%2E%63%6F%6D/",
                         "168.188.99.26/.secure/www.ebay.com/");
  TestCanonicalizeHelper("195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/",
                         "195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/");
  // Added in bug 489455.  %00 should no longer be changed to %01.
  TestCanonicalizeHelper("%00", "%00");
}

void TestParseIPAddressHelper(const char *in, const char *expected)
{
  nsCString out, strIn(in), strExp(expected);
  RefPtr<nsUrlClassifierUtils> utils = new nsUrlClassifierUtils;
  utils->Init();

  utils->ParseIPAddress(strIn, out);
  CheckEquals(strExp, out);
}

TEST(UrlClassifierUtils, ParseIPAddress)
{
  TestParseIPAddressHelper("123.123.0.0.1", "");
  TestParseIPAddressHelper("255.0.0.1", "255.0.0.1");
  TestParseIPAddressHelper("12.0x12.01234", "12.18.2.156");
  TestParseIPAddressHelper("276.2.3", "20.2.0.3");
  TestParseIPAddressHelper("012.034.01.055", "10.28.1.45");
  TestParseIPAddressHelper("0x12.0x43.0x44.0x01", "18.67.68.1");
  TestParseIPAddressHelper("167838211", "10.1.2.3");
  TestParseIPAddressHelper("3279880203", "195.127.0.11");
  TestParseIPAddressHelper("0x12434401", "18.67.68.1");
  TestParseIPAddressHelper("413960661", "24.172.137.213");
  TestParseIPAddressHelper("03053104725", "24.172.137.213");
  TestParseIPAddressHelper("030.0254.0x89d5", "24.172.137.213");
  TestParseIPAddressHelper("1.234.4.0377", "1.234.4.255");
  TestParseIPAddressHelper("1.2.3.00x0", "");
  TestParseIPAddressHelper("10.192.95.89 xy", "10.192.95.89");
  TestParseIPAddressHelper("10.192.95.89 xyz", "");
  TestParseIPAddressHelper("1.2.3.0x0", "1.2.3.0");
  TestParseIPAddressHelper("1.2.3.4", "1.2.3.4");
}

void TestCanonicalNumHelper(const char *in, uint32_t bytes,
                            bool allowOctal, const char *expected)
{
  nsCString out, strIn(in), strExp(expected);
  RefPtr<nsUrlClassifierUtils> utils = new nsUrlClassifierUtils;
  utils->Init();

  utils->CanonicalNum(strIn, bytes, allowOctal, out);
  CheckEquals(strExp, out);
}

TEST(UrlClassifierUtils, CanonicalNum)
{
  TestCanonicalNumHelper("", 1, true, "");
  TestCanonicalNumHelper("10", 0, true, "");
  TestCanonicalNumHelper("45", 1, true, "45");
  TestCanonicalNumHelper("0x10", 1, true, "16");
  TestCanonicalNumHelper("367", 2, true, "1.111");
  TestCanonicalNumHelper("012345", 3, true, "0.20.229");
  TestCanonicalNumHelper("0173", 1, true, "123");
  TestCanonicalNumHelper("09", 1, false, "9");
  TestCanonicalNumHelper("0x120x34", 2, true, "");
  TestCanonicalNumHelper("0x12fc", 2, true, "18.252");
  TestCanonicalNumHelper("3279880203", 4, true, "195.127.0.11");
  TestCanonicalNumHelper("0x0000059", 1, true, "89");
  TestCanonicalNumHelper("0x00000059", 1, true, "89");
  TestCanonicalNumHelper("0x0000067", 1, true, "103");
}

void TestHostnameHelper(const char *in, const char *expected)
{
  nsCString out, strIn(in), strExp(expected);
  RefPtr<nsUrlClassifierUtils> utils = new nsUrlClassifierUtils;
  utils->Init();

  utils->CanonicalizeHostname(strIn, out);
  CheckEquals(strExp, out);
}

TEST(UrlClassifierUtils, Hostname)
{
  TestHostnameHelper("abcd123;[]", "abcd123;[]");
  TestHostnameHelper("abc.123", "abc.123");
  TestHostnameHelper("abc..123", "abc.123");
  TestHostnameHelper("trailing.", "trailing");
  TestHostnameHelper("i love trailing dots....", "i%20love%20trailing%20dots");
  TestHostnameHelper(".leading", "leading");
  TestHostnameHelper("..leading", "leading");
  TestHostnameHelper(".dots.", "dots");
  TestHostnameHelper(".both.", "both");
  TestHostnameHelper(".both..", "both");
  TestHostnameHelper("..both.", "both");
  TestHostnameHelper("..both..", "both");
  TestHostnameHelper("..a.b.c.d..", "a.b.c.d");
  TestHostnameHelper("..127.0.0.1..", "127.0.0.1");
  TestHostnameHelper("asdf!@#$a", "asdf!@#$a");
  TestHostnameHelper("AB CD 12354", "ab%20cd%2012354");
  TestHostnameHelper("\1\2\3\4\112\177", "%01%02%03%04j%7F");
  TestHostnameHelper("<>.AS/-+", "<>.as/-+");
  // Added in bug 489455.  %00 should no longer be changed to %01.
  TestHostnameHelper("%00", "%00");
}

TEST(UrlClassifierUtils, LongHostname)
{
  static const int kTestSize = 1024 * 150;
  char *str = static_cast<char*>(malloc(kTestSize + 1));
  memset(str, 'x', kTestSize);
  str[kTestSize] = '\0';

  RefPtr<nsUrlClassifierUtils> utils = new nsUrlClassifierUtils;
  utils->Init();

  nsAutoCString out;
  nsDependentCString in(str);
  PRIntervalTime clockStart = PR_IntervalNow();
  utils->CanonicalizeHostname(in, out);
  PRIntervalTime clockEnd = PR_IntervalNow();

  CheckEquals(in, out);

  printf("CanonicalizeHostname on long string (%dms)\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart));
}
