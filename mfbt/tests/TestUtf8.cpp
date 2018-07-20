/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EnumSet.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/TextUtils.h"

using mozilla::ArrayLength;
using mozilla::DecodeOneUtf8CodePoint;
using mozilla::EnumSet;
using mozilla::IntegerRange;
using mozilla::IsAscii;
using mozilla::IsValidUtf8;
using mozilla::Utf8Unit;

static void
TestUtf8Unit()
{
  Utf8Unit c('A');
  MOZ_RELEASE_ASSERT(c.toChar() == 'A');
  MOZ_RELEASE_ASSERT(c == Utf8Unit('A'));
  MOZ_RELEASE_ASSERT(c != Utf8Unit('B'));
  MOZ_RELEASE_ASSERT(c.toUint8() == 0x41);

  unsigned char asUnsigned = 'A';
  MOZ_RELEASE_ASSERT(c.toUnsignedChar() == asUnsigned);
  MOZ_RELEASE_ASSERT(Utf8Unit('B').toUnsignedChar() != asUnsigned);

  Utf8Unit first('@');
  Utf8Unit second('#');

  MOZ_RELEASE_ASSERT(first != second);

  first = second;
  MOZ_RELEASE_ASSERT(first == second);
}

template<typename Char>
struct ToUtf8Units
{
public:
  explicit ToUtf8Units(const Char* aStart, const Char* aEnd)
    : lead(Utf8Unit(aStart[0]))
    , iter(aStart + 1)
    , end(aEnd)
  {
    MOZ_RELEASE_ASSERT(!IsAscii(aStart[0]));
  }

  const Utf8Unit lead;
  const Char* iter;
  const Char* const end;
};

class AssertIfCalled
{
public:
  template<typename... Args>
  void operator()(Args&&... aArgs) {
    MOZ_RELEASE_ASSERT(false, "AssertIfCalled instance was called");
  }
};

// NOTE: For simplicity in treating |aCharN| identically regardless whether it's
//       a string literal or a more-generalized array, we require |aCharN| be
//       null-terminated.

template<typename Char, size_t N>
static void
ExpectValidCodePoint(const Char (&aCharN)[N],
                     char32_t aExpectedCodePoint)
{
  MOZ_RELEASE_ASSERT(aCharN[N - 1] == 0,
                     "array must be null-terminated for |aCharN + N - 1| to "
                     "compute the value of |aIter| as altered by "
                     "DecodeOneUtf8CodePoint");

  ToUtf8Units<Char> simpleUnit(aCharN, aCharN + N - 1);
  auto simple =
    DecodeOneUtf8CodePoint(simpleUnit.lead, &simpleUnit.iter, simpleUnit.end);
  MOZ_RELEASE_ASSERT(simple.isSome());
  MOZ_RELEASE_ASSERT(*simple == aExpectedCodePoint);
  MOZ_RELEASE_ASSERT(simpleUnit.iter == simpleUnit.end);

  ToUtf8Units<Char> complexUnit(aCharN, aCharN + N - 1);
  auto complex =
    DecodeOneUtf8CodePoint(complexUnit.lead, &complexUnit.iter, complexUnit.end,
                           AssertIfCalled(),
                           AssertIfCalled(),
                           AssertIfCalled(),
                           AssertIfCalled(),
                           AssertIfCalled());
  MOZ_RELEASE_ASSERT(complex.isSome());
  MOZ_RELEASE_ASSERT(*complex == aExpectedCodePoint);
  MOZ_RELEASE_ASSERT(complexUnit.iter == complexUnit.end);
}

enum class InvalidUtf8Reason
{
  BadLeadUnit,
  NotEnoughUnits,
  BadTrailingUnit,
  BadCodePoint,
  NotShortestForm,
};

template<typename Char, size_t N>
static void
ExpectInvalidCodePointHelper(const Char (&aCharN)[N],
                             InvalidUtf8Reason aExpectedReason,
                             uint8_t aExpectedUnitsAvailable,
                             uint8_t aExpectedUnitsNeeded,
                             char32_t aExpectedBadCodePoint,
                             uint8_t aExpectedUnitsObserved)
{
  MOZ_RELEASE_ASSERT(aCharN[N - 1] == 0,
                     "array must be null-terminated for |aCharN + N - 1| to "
                     "compute the value of |aIter| as altered by "
                     "DecodeOneUtf8CodePoint");

  ToUtf8Units<Char> simpleUnit(aCharN, aCharN + N - 1);
  auto simple =
    DecodeOneUtf8CodePoint(simpleUnit.lead, &simpleUnit.iter, simpleUnit.end);
  MOZ_RELEASE_ASSERT(simple.isNothing());
  MOZ_RELEASE_ASSERT(static_cast<const void*>(simpleUnit.iter) == aCharN);

  EnumSet<InvalidUtf8Reason> reasons;
  uint8_t unitsAvailable;
  uint8_t unitsNeeded;
  char32_t badCodePoint;
  uint8_t unitsObserved;

  struct OnNotShortestForm
  {
    EnumSet<InvalidUtf8Reason>& reasons;
    char32_t& badCodePoint;
    uint8_t& unitsObserved;

    void operator()(char32_t aBadCodePoint, uint8_t aUnitsObserved) {
      reasons += InvalidUtf8Reason::NotShortestForm;
      badCodePoint = aBadCodePoint;
      unitsObserved = aUnitsObserved;
    }
  };

  ToUtf8Units<Char> complexUnit(aCharN, aCharN + N - 1);
  auto complex =
    DecodeOneUtf8CodePoint(complexUnit.lead, &complexUnit.iter, complexUnit.end,
                           [&reasons]() {
                             reasons += InvalidUtf8Reason::BadLeadUnit;
                           },
                           [&reasons, &unitsAvailable, &unitsNeeded](uint8_t aUnitsAvailable,
                                                                     uint8_t aUnitsNeeded)
                           {
                             reasons += InvalidUtf8Reason::NotEnoughUnits;
                             unitsAvailable = aUnitsAvailable;
                             unitsNeeded = aUnitsNeeded;
                           },
                           [&reasons, &unitsObserved](uint8_t aUnitsObserved)
                           {
                             reasons += InvalidUtf8Reason::BadTrailingUnit;
                             unitsObserved = aUnitsObserved;
                           },
                           [&reasons, &badCodePoint, &unitsObserved](char32_t aBadCodePoint,
                                                                     uint8_t aUnitsObserved)
                           {
                             reasons += InvalidUtf8Reason::BadCodePoint;
                             badCodePoint = aBadCodePoint;
                             unitsObserved = aUnitsObserved;
                           },
                           [&reasons, &badCodePoint, &unitsObserved](char32_t aBadCodePoint,
                                                                     uint8_t aUnitsObserved)
                           {
                             reasons += InvalidUtf8Reason::NotShortestForm;
                             badCodePoint = aBadCodePoint;
                             unitsObserved = aUnitsObserved;
                           });
  MOZ_RELEASE_ASSERT(complex.isNothing());
  MOZ_RELEASE_ASSERT(static_cast<const void*>(complexUnit.iter) == aCharN);

  bool alreadyIterated = false;
  for (InvalidUtf8Reason reason : reasons) {
    MOZ_RELEASE_ASSERT(!alreadyIterated);
    alreadyIterated = true;

    switch (reason) {
    case InvalidUtf8Reason::BadLeadUnit:
      break;

    case InvalidUtf8Reason::NotEnoughUnits:
      MOZ_RELEASE_ASSERT(unitsAvailable == aExpectedUnitsAvailable);
      MOZ_RELEASE_ASSERT(unitsNeeded == aExpectedUnitsNeeded);
      break;

    case InvalidUtf8Reason::BadTrailingUnit:
      MOZ_RELEASE_ASSERT(unitsObserved == aExpectedUnitsObserved);
      break;

    case InvalidUtf8Reason::BadCodePoint:
      MOZ_RELEASE_ASSERT(badCodePoint == aExpectedBadCodePoint);
      MOZ_RELEASE_ASSERT(unitsObserved == aExpectedUnitsObserved);
      break;

    case InvalidUtf8Reason::NotShortestForm:
      MOZ_RELEASE_ASSERT(badCodePoint == aExpectedBadCodePoint);
      MOZ_RELEASE_ASSERT(unitsObserved == aExpectedUnitsObserved);
      break;
    }
  }
}

// NOTE: For simplicity in treating |aCharN| identically regardless whether it's
//       a string literal or a more-generalized array, we require |aCharN| be
//       null-terminated in all these functions.

template<typename Char, size_t N>
static void
ExpectBadLeadUnit(const Char (&aCharN)[N])
{
  ExpectInvalidCodePointHelper(aCharN,
                               InvalidUtf8Reason::BadLeadUnit,
                               0xFF, 0xFF, 0xFFFFFFFF, 0xFF);
}

template<typename Char, size_t N>
static void
ExpectNotEnoughUnits(const Char (&aCharN)[N],
                     uint8_t aExpectedUnitsAvailable,
                     uint8_t aExpectedUnitsNeeded)
{
  ExpectInvalidCodePointHelper(aCharN,
                               InvalidUtf8Reason::NotEnoughUnits,
                               aExpectedUnitsAvailable, aExpectedUnitsNeeded,
                               0xFFFFFFFF, 0xFF);
}

template<typename Char, size_t N>
static void
ExpectBadTrailingUnit(const Char (&aCharN)[N],
                      uint8_t aExpectedUnitsObserved)
{
  ExpectInvalidCodePointHelper(aCharN,
                               InvalidUtf8Reason::BadTrailingUnit,
                               0xFF, 0xFF, 0xFFFFFFFF,
                               aExpectedUnitsObserved);
}

template<typename Char, size_t N>
static void
ExpectNotShortestForm(const Char (&aCharN)[N],
                      char32_t aExpectedBadCodePoint,
                      uint8_t aExpectedUnitsObserved)
{
  ExpectInvalidCodePointHelper(aCharN,
                               InvalidUtf8Reason::NotShortestForm,
                               0xFF, 0xFF,
                               aExpectedBadCodePoint,
                               aExpectedUnitsObserved);
}

template<typename Char, size_t N>
static void
ExpectBadCodePoint(const Char (&aCharN)[N],
                   char32_t aExpectedBadCodePoint,
                   uint8_t aExpectedUnitsObserved)
{
  ExpectInvalidCodePointHelper(aCharN,
                               InvalidUtf8Reason::BadCodePoint,
                               0xFF, 0xFF,
                               aExpectedBadCodePoint,
                               aExpectedUnitsObserved);
}

static void
TestIsValidUtf8()
{
  // Note we include the U+0000 NULL in this one -- and that's fine.
  static const char asciiBytes[] = u8"How about a nice game of chess?";
  MOZ_RELEASE_ASSERT(IsValidUtf8(asciiBytes, ArrayLength(asciiBytes)));

  static const char endNonAsciiBytes[] = u8"Life is like a 🌯";
  MOZ_RELEASE_ASSERT(IsValidUtf8(endNonAsciiBytes, ArrayLength(endNonAsciiBytes) - 1));

  static const unsigned char badLeading[] = { 0x80 };
  MOZ_RELEASE_ASSERT(!IsValidUtf8(badLeading, ArrayLength(badLeading)));

  // Byte-counts

  // 1
  static const char oneBytes[] = u8"A"; // U+0041 LATIN CAPITAL LETTER A
  constexpr size_t oneBytesLen = ArrayLength(oneBytes);
  static_assert(oneBytesLen == 2, "U+0041 plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(oneBytes, oneBytesLen));

  // 2
  static const char twoBytes[] = u8"؆"; // U+0606 ARABIC-INDIC CUBE ROOT
  constexpr size_t twoBytesLen = ArrayLength(twoBytes);
  static_assert(twoBytesLen == 3, "U+0606 in two bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(twoBytes, twoBytesLen));

  ExpectValidCodePoint(twoBytes, 0x0606);

  // 3
  static const char threeBytes[] = u8"᨞"; // U+1A1E BUGINESE PALLAWA
  constexpr size_t threeBytesLen = ArrayLength(threeBytes);
  static_assert(threeBytesLen == 4, "U+1A1E in three bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(threeBytes, threeBytesLen));

  ExpectValidCodePoint(threeBytes, 0x1A1E);

  // 4
  static const char fourBytes[] = u8"🁡"; // U+1F061 DOMINO TILE HORIZONTAL-06-06
  constexpr size_t fourBytesLen = ArrayLength(fourBytes);
  static_assert(fourBytesLen == 5, "U+1F061 in four bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(fourBytes, fourBytesLen));

  ExpectValidCodePoint(fourBytes, 0x1F061);

  // Max code point
  static const char maxCodePoint[] = u8"􏿿"; // U+10FFFF
  constexpr size_t maxCodePointLen = ArrayLength(maxCodePoint);
  static_assert(maxCodePointLen == 5, "U+10FFFF in four bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(maxCodePoint, maxCodePointLen));

  ExpectValidCodePoint(maxCodePoint, 0x10FFFF);

  // One past max code point
  static const unsigned char onePastMaxCodePoint[] = { 0xF4, 0x90, 0x80, 0x80, 0x0 };
  constexpr size_t onePastMaxCodePointLen = ArrayLength(onePastMaxCodePoint);
  MOZ_RELEASE_ASSERT(!IsValidUtf8(onePastMaxCodePoint, onePastMaxCodePointLen));

  ExpectBadCodePoint(onePastMaxCodePoint, 0x110000, 4);

  // Surrogate-related testing

  // (Note that the various code unit sequences here are null-terminated to
  // simplify life for ExpectValidCodePoint, which presumes null termination.)

  static const unsigned char justBeforeSurrogates[] = { 0xED, 0x9F, 0xBF, 0x0 };
  constexpr size_t justBeforeSurrogatesLen = ArrayLength(justBeforeSurrogates) - 1;
  MOZ_RELEASE_ASSERT(IsValidUtf8(justBeforeSurrogates, justBeforeSurrogatesLen));

  ExpectValidCodePoint(justBeforeSurrogates, 0xD7FF);

  static const unsigned char leastSurrogate[] = { 0xED, 0xA0, 0x80, 0x0 };
  constexpr size_t leastSurrogateLen = ArrayLength(leastSurrogate) - 1;
  MOZ_RELEASE_ASSERT(!IsValidUtf8(leastSurrogate, leastSurrogateLen));

  ExpectBadCodePoint(leastSurrogate, 0xD800, 3);

  static const unsigned char arbitraryHighSurrogate[] = { 0xED, 0xA2, 0x87, 0x0 };
  constexpr size_t arbitraryHighSurrogateLen = ArrayLength(arbitraryHighSurrogate) - 1;
  MOZ_RELEASE_ASSERT(!IsValidUtf8(arbitraryHighSurrogate, arbitraryHighSurrogateLen));

  ExpectBadCodePoint(arbitraryHighSurrogate, 0xD887, 3);

  static const unsigned char arbitraryLowSurrogate[] = { 0xED, 0xB7, 0xAF, 0x0 };
  constexpr size_t arbitraryLowSurrogateLen = ArrayLength(arbitraryLowSurrogate) - 1;
  MOZ_RELEASE_ASSERT(!IsValidUtf8(arbitraryLowSurrogate, arbitraryLowSurrogateLen));

  ExpectBadCodePoint(arbitraryLowSurrogate, 0xDDEF, 3);

  static const unsigned char greatestSurrogate[] = { 0xED, 0xBF, 0xBF, 0x0 };
  constexpr size_t greatestSurrogateLen = ArrayLength(greatestSurrogate) - 1;
  MOZ_RELEASE_ASSERT(!IsValidUtf8(greatestSurrogate, greatestSurrogateLen));

  ExpectBadCodePoint(greatestSurrogate, 0xDFFF, 3);

  static const unsigned char justAfterSurrogates[] = { 0xEE, 0x80, 0x80, 0x0 };
  constexpr size_t justAfterSurrogatesLen = ArrayLength(justAfterSurrogates) - 1;
  MOZ_RELEASE_ASSERT(IsValidUtf8(justAfterSurrogates, justAfterSurrogatesLen));

  ExpectValidCodePoint(justAfterSurrogates, 0xE000);
}

static void
TestDecodeOneValidUtf8CodePoint()
{
  // NOTE: DecodeOneUtf8CodePoint decodes only *non*-ASCII code points that
  //       consist of multiple code units, so there are no ASCII tests below.

  // Length two.

  ExpectValidCodePoint(u8"", 0x80); // <control>
  ExpectValidCodePoint(u8"©", 0xA9); // COPYRIGHT SIGN
  ExpectValidCodePoint(u8"¶", 0xB6); // PILCROW SIGN
  ExpectValidCodePoint(u8"¾", 0xBE); // VULGAR FRACTION THREE QUARTERS
  ExpectValidCodePoint(u8"÷", 0xF7); // DIVISION SIGN
  ExpectValidCodePoint(u8"ÿ", 0xFF); // LATIN SMALL LETTER Y WITH DIAERESIS
  ExpectValidCodePoint(u8"Ā", 0x100); // LATIN CAPITAL LETTER A WITH MACRON
  ExpectValidCodePoint(u8"Ĳ", 0x132); // LATIN CAPITAL LETTER LIGATURE IJ
  ExpectValidCodePoint(u8"ͼ", 0x37C); // GREEK SMALL DOTTED LUNATE SIGMA SYMBOL
  ExpectValidCodePoint(u8"Ӝ", 0x4DC); // CYRILLIC CAPITAL LETTER ZHE WITTH DIAERESIS
  ExpectValidCodePoint(u8"۩", 0x6E9); // ARABIC PLACE OF SAJDAH
  ExpectValidCodePoint(u8"߿", 0x7FF); // <not assigned>

  // Length three.

  ExpectValidCodePoint(u8"ࠀ", 0x800); // SAMARITAN LETTER ALAF
  ExpectValidCodePoint(u8"ࡁ", 0x841); // MANDAIC LETTER AB
  ExpectValidCodePoint(u8"ࣿ", 0x8FF); // ARABIC MARK SIDEWAYS NOON GHUNNA
  ExpectValidCodePoint(u8"ஆ", 0xB86); // TAMIL LETTER AA
  ExpectValidCodePoint(u8"༃", 0xF03); // TIBETAN MARK GTER YIG MGO -UM GTER TSHEG MA
  ExpectValidCodePoint(u8"࿉", 0xFC9); // TIBETAN SYMBOL NOR BU (but on my system it really looks like SOFT-SERVE ICE CREAM FROM ABOVE THE PLANE if you ask me)
  ExpectValidCodePoint(u8"ဪ", 0x102A); // MYANMAR LETTER AU
  ExpectValidCodePoint(u8"ᚏ", 0x168F); // OGHAM LETTER RUIS
  ExpectValidCodePoint("\xE2\x80\xA8", 0x2028); // (the hated) LINE SEPARATOR
  ExpectValidCodePoint("\xE2\x80\xA9", 0x2029); // (the hated) PARAGRAPH SEPARATOR
  ExpectValidCodePoint(u8"☬", 0x262C); // ADI SHAKTI
  ExpectValidCodePoint(u8"㊮", 0x32AE); // CIRCLED IDEOGRAPH RESOURCE
  ExpectValidCodePoint(u8"㏖", 0x33D6); // SQUARE MOL
  ExpectValidCodePoint(u8"ꔄ", 0xA504); // VAI SYLLABLE WEEN
  ExpectValidCodePoint(u8"ퟕ", 0xD7D5); // HANGUL JONGSEONG RIEUL-SSANGKIYEOK
  ExpectValidCodePoint(u8"퟿", 0xD7FF); // <not assigned>
  ExpectValidCodePoint(u8"", 0xE000); // <Private Use>
  ExpectValidCodePoint(u8"鱗", 0xF9F2); // CJK COMPATIBILITY IDEOGRAPH-F9F
  ExpectValidCodePoint(u8"﷽", 0xFDFD); // ARABIC LIGATURE BISMILLAH AR-RAHMAN AR-RAHHHEEEEM
  ExpectValidCodePoint(u8"￿", 0xFFFF); // <not assigned>

  // Length four.
  ExpectValidCodePoint(u8"𐀀", 0x10000); // LINEAR B SYLLABLE B008 A
  ExpectValidCodePoint(u8"𔑀", 0x14440); // ANATOLIAN HIEROGLYPH A058
  ExpectValidCodePoint(u8"𝛗", 0x1D6D7); // MATHEMATICAL BOLD SMALL PHI
  ExpectValidCodePoint(u8"💩", 0x1F4A9); // PILE OF POO
  ExpectValidCodePoint(u8"🔫", 0x1F52B); // PISTOL
  ExpectValidCodePoint(u8"🥌", 0x1F94C); // CURLING STONE
  ExpectValidCodePoint(u8"🥏", 0x1F94F); // FLYING DISC
  ExpectValidCodePoint(u8"𠍆", 0x20346); // CJK UNIFIED IDEOGRAPH-20346
  ExpectValidCodePoint(u8"𡠺", 0x2183A); // CJK UNIFIED IDEOGRAPH-2183A
  ExpectValidCodePoint(u8"񁟶", 0x417F6); // <not assigned>
  ExpectValidCodePoint(u8"񾠶", 0x7E836); // <not assigned>
  ExpectValidCodePoint(u8"󾽧", 0xFEF67); // <Plane 15 Private Use>
  ExpectValidCodePoint(u8"􏿿", 0x10FFFF); //
}

static void
TestDecodeBadLeadUnit()
{
  // These tests are actually exhaustive.

  unsigned char badLead[] = { '\0', '\0' };

  for (uint8_t lead : IntegerRange(0b1000'0000, 0b1100'0000)) {
    badLead[0] = lead;
    ExpectBadLeadUnit(badLead);
  }

  {
    uint8_t lead = 0b1111'1000;
    do {
      badLead[0] = lead;
      ExpectBadLeadUnit(badLead);
      if (lead == 0b1111'1111) {
        break;
      }

      lead++;
    } while (true);
  }
}

static void
TestTooFewOrBadTrailingUnits()
{
  // Lead unit indicates a two-byte code point.

  char truncatedTwo[] = { '\0', '\0' };
  char badTrailTwo[] = { '\0', '\0', '\0' };

  for (uint8_t lead : IntegerRange(0b1100'0000, 0b1110'0000)) {
    truncatedTwo[0] = lead;
    ExpectNotEnoughUnits(truncatedTwo, 1, 2);

    badTrailTwo[0] = lead;
    for (uint8_t trail : IntegerRange(0b0000'0000, 0b1000'0000)) {
      badTrailTwo[1] = trail;
      ExpectBadTrailingUnit(badTrailTwo, 2);
    }

    for (uint8_t trail : IntegerRange(0b1100'0000, 0b1111'1111)) {
      badTrailTwo[1] = trail;
      ExpectBadTrailingUnit(badTrailTwo, 2);
    }
  }

  // Lead unit indicates a three-byte code point.

  char truncatedThreeOne[] = { '\0', '\0' };
  char truncatedThreeTwo[] = { '\0', '\0', '\0' };
  unsigned char badTrailThree[] = { '\0', '\0', '\0', '\0' };

  for (uint8_t lead : IntegerRange(0b1110'0000, 0b1111'0000)) {
    truncatedThreeOne[0] = lead;
    ExpectNotEnoughUnits(truncatedThreeOne, 1, 3);

    truncatedThreeTwo[0] = lead;
    ExpectNotEnoughUnits(truncatedThreeTwo, 2, 3);

    badTrailThree[0] = lead;
    badTrailThree[2] = 0b1011'1111; // make valid to test overreads
    for (uint8_t mid : IntegerRange(0b0000'0000, 0b1000'0000)) {
      badTrailThree[1] = mid;
      ExpectBadTrailingUnit(badTrailThree, 2);
    }
    {
      uint8_t mid = 0b1100'0000;
      do {
        badTrailThree[1] = mid;
        ExpectBadTrailingUnit(badTrailThree, 2);
        if (mid == 0b1111'1111) {
          break;
        }

        mid++;
      } while (true);
    }

    badTrailThree[1] = 0b1011'1111;
    for (uint8_t last : IntegerRange(0b0000'0000, 0b1000'0000)) {
      badTrailThree[2] = last;
      ExpectBadTrailingUnit(badTrailThree, 3);
    }
    {
      uint8_t last = 0b1100'0000;
      do {
        badTrailThree[2] = last;
        ExpectBadTrailingUnit(badTrailThree, 3);
        if (last == 0b1111'1111) {
          break;
        }

        last++;
      } while (true);
    }
  }

  // Lead unit indicates a four-byte code point.

  char truncatedFourOne[] = { '\0', '\0' };
  char truncatedFourTwo[] = { '\0', '\0', '\0' };
  char truncatedFourThree[] = { '\0', '\0', '\0', '\0' };

  unsigned char badTrailFour[] = { '\0', '\0', '\0', '\0', '\0' };

  for (uint8_t lead : IntegerRange(0b1111'0000, 0b1111'1000)) {
    truncatedFourOne[0] = lead;
    ExpectNotEnoughUnits(truncatedFourOne, 1, 4);

    truncatedFourTwo[0] = lead;
    ExpectNotEnoughUnits(truncatedFourTwo, 2, 4);

    truncatedFourThree[0] = lead;
    ExpectNotEnoughUnits(truncatedFourThree, 3, 4);

    badTrailFour[0] = lead;
    badTrailFour[2] = badTrailFour[3] = 0b1011'1111; // test for overreads
    for (uint8_t second : IntegerRange(0b0000'0000, 0b1000'0000)) {
      badTrailFour[1] = second;
      ExpectBadTrailingUnit(badTrailFour, 2);
    }
    {
      uint8_t second = 0b1100'0000;
      do {
        badTrailFour[1] = second;
        ExpectBadTrailingUnit(badTrailFour, 2);
        if (second == 0b1111'1111) {
          break;
        }

        second++;
      } while (true);
    }

    badTrailFour[1] = badTrailFour[3] = 0b1011'1111; // test for overreads
    for (uint8_t third : IntegerRange(0b0000'0000, 0b1000'0000)) {
      badTrailFour[2] = third;
      ExpectBadTrailingUnit(badTrailFour, 3);
    }
    {
      uint8_t third = 0b1100'0000;
      do {
        badTrailFour[2] = third;
        ExpectBadTrailingUnit(badTrailFour, 3);
        if (third == 0b1111'1111) {
          break;
        }

        third++;
      } while (true);
    }

    badTrailFour[2] = 0b1011'1111;
    for (uint8_t fourth : IntegerRange(0b0000'0000, 0b1000'0000)) {
      badTrailFour[3] = fourth;
      ExpectBadTrailingUnit(badTrailFour, 4);
    }
    {
      uint8_t fourth = 0b1100'0000;
      do {
        badTrailFour[3] = fourth;
        ExpectBadTrailingUnit(badTrailFour, 4);
        if (fourth == 0b1111'1111) {
          break;
        }

        fourth++;
      } while (true);
    }
  }
}

static void
TestBadSurrogate()
{
  // These tests are actually exhaustive.

  ExpectValidCodePoint("\xED\x9F\xBF", 0xD7FF); // last before surrogates
  ExpectValidCodePoint("\xEE\x80\x80", 0xE000); // first after surrogates

  // First invalid surrogate encoding is { 0xED, 0xA0, 0x80 }.  Last invalid
  // surrogate encoding is { 0xED, 0xBF, 0xBF }.

  char badSurrogate[] = { '\xED', '\0', '\0', '\0' };

  for (char32_t c = 0xD800; c < 0xE000; c++) {
    badSurrogate[1] = 0b1000'0000 ^ ((c & 0b1111'1100'0000) >> 6);
    badSurrogate[2] = 0b1000'0000 ^ ((c & 0b0000'0011'1111));

    ExpectBadCodePoint(badSurrogate, c, 3);
  }
}

static void
TestBadTooBig()
{
  // These tests are actually exhaustive.

  ExpectValidCodePoint("\xF4\x8F\xBF\xBF", 0x10'FFFF); // last code point

  // Four-byte code points are
  //
  //   0b1111'0xxx 0b10xx'xxxx 0b10xx'xxxx 0b10xx'xxxx
  //
  // with 3 + 6 + 6 + 6 == 21 unconstrained bytes, so the structurally
  // representable limit (exclusive) is 2**21 - 1 == 2097152.

  char tooLargeCodePoint[] = { '\0', '\0', '\0', '\0', '\0' };

  for (char32_t c = 0x11'0000; c < (1 << 21); c++) {
    tooLargeCodePoint[0] = 0b1111'0000 ^ ((c & 0b1'1100'0000'0000'0000'0000) >> 18);
    tooLargeCodePoint[1] = 0b1000'0000 ^ ((c & 0b0'0011'1111'0000'0000'0000) >> 12);
    tooLargeCodePoint[2] = 0b1000'0000 ^ ((c & 0b0'0000'0000'1111'1100'0000) >> 6);
    tooLargeCodePoint[3] = 0b1000'0000 ^ ((c & 0b0'0000'0000'0000'0011'1111));

    ExpectBadCodePoint(tooLargeCodePoint, c, 4);
  }
}

static void
TestBadCodePoint()
{
  TestBadSurrogate();
  TestBadTooBig();
}

static void
TestNotShortestForm()
{
  {
    // One-byte in two-byte.

    char oneInTwo[] = { '\0', '\0', '\0' };

    for (char32_t c = '\0'; c < 0x80; c++) {
      oneInTwo[0] = 0b1100'0000 ^ ((c & 0b0111'1100'0000) >> 6);
      oneInTwo[1] = 0b1000'0000 ^ ((c & 0b0000'0011'1111));

      ExpectNotShortestForm(oneInTwo, c, 2);
    }

    // One-byte in three-byte.

    char oneInThree[] = { '\0', '\0', '\0', '\0' };

    for (char32_t c = '\0'; c < 0x80; c++) {
      oneInThree[0] = 0b1110'0000 ^ ((c & 0b1111'0000'0000'0000) >> 12);
      oneInThree[1] = 0b1000'0000 ^ ((c & 0b0000'1111'1100'0000) >> 6);
      oneInThree[2] = 0b1000'0000 ^ ((c & 0b0000'0000'0011'1111));

      ExpectNotShortestForm(oneInThree, c, 3);
    }

    // One-byte in four-byte.

    char oneInFour[] = { '\0', '\0', '\0', '\0', '\0' };

    for (char32_t c = '\0'; c < 0x80; c++) {
      oneInFour[0] = 0b1111'0000 ^ ((c & 0b1'1100'0000'0000'0000'0000) >> 18);
      oneInFour[1] = 0b1000'0000 ^ ((c & 0b0'0011'1111'0000'0000'0000) >> 12);
      oneInFour[2] = 0b1000'0000 ^ ((c & 0b0'0000'0000'1111'1100'0000) >> 6);
      oneInFour[3] = 0b1000'0000 ^ ((c & 0b0'0000'0000'0000'0011'1111));

      ExpectNotShortestForm(oneInFour, c, 4);
    }
  }

  {
    // Two-byte in three-byte.

    char twoInThree[] = { '\0', '\0', '\0', '\0' };

    for (char32_t c = 0x80; c < 0x800; c++) {
      twoInThree[0] = 0b1110'0000 ^ ((c & 0b1111'0000'0000'0000) >> 12);
      twoInThree[1] = 0b1000'0000 ^ ((c & 0b0000'1111'1100'0000) >> 6);
      twoInThree[2] = 0b1000'0000 ^ ((c & 0b0000'0000'0011'1111));

      ExpectNotShortestForm(twoInThree, c, 3);
    }

    // Two-byte in four-byte.

    char twoInFour[] = { '\0', '\0', '\0', '\0', '\0' };

    for (char32_t c = 0x80; c < 0x800; c++) {
      twoInFour[0] = 0b1111'0000 ^ ((c & 0b1'1100'0000'0000'0000'0000) >> 18);
      twoInFour[1] = 0b1000'0000 ^ ((c & 0b0'0011'1111'0000'0000'0000) >> 12);
      twoInFour[2] = 0b1000'0000 ^ ((c & 0b0'0000'0000'1111'1100'0000) >> 6);
      twoInFour[3] = 0b1000'0000 ^ ((c & 0b0'0000'0000'0000'0011'1111));

      ExpectNotShortestForm(twoInFour, c, 4);
    }
  }

  {
    // Three-byte in four-byte.

    char threeInFour[] = { '\0', '\0', '\0', '\0', '\0' };

    for (char32_t c = 0x800; c < 0x1'0000; c++) {
      threeInFour[0] = 0b1111'0000 ^ ((c & 0b1'1100'0000'0000'0000'0000) >> 18);
      threeInFour[1] = 0b1000'0000 ^ ((c & 0b0'0011'1111'0000'0000'0000) >> 12);
      threeInFour[2] = 0b1000'0000 ^ ((c & 0b0'0000'0000'1111'1100'0000) >> 6);
      threeInFour[3] = 0b1000'0000 ^ ((c & 0b0'0000'0000'0000'0011'1111));

      ExpectNotShortestForm(threeInFour, c, 4);
    }
  }
}

static void
TestDecodeOneInvalidUtf8CodePoint()
{
  TestDecodeBadLeadUnit();
  TestTooFewOrBadTrailingUnits();
  TestBadCodePoint();
  TestNotShortestForm();
}

static void
TestDecodeOneUtf8CodePoint()
{
  TestDecodeOneValidUtf8CodePoint();
  TestDecodeOneInvalidUtf8CodePoint();
}

int
main()
{
  TestUtf8Unit();
  TestIsValidUtf8();
  TestDecodeOneUtf8CodePoint();
  return 0;
}
