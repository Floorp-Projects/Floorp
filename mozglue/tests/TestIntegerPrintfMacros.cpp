/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Sprintf.h"

#include <string.h>
#include <inttypes.h>

/* Output array and poisoning method shared by all tests. */
static char gOutput[32];

static void PoisonOutput() { memset(gOutput, 0xDA, sizeof(gOutput)); }

/*
 * The fprintf macros for signed integers are:
 *
 *   PRIdN   PRIdLEASTN   PRIdFASTN   PRIdMAX   PRIdPTR
 *   PRIiN   PRIiLEASTN   PRIiFASTN   PRIiMAX   PRIiPTR
 *
 * In these names N is the width of the type as described in C99 7.18.1.
 */

static void TestPrintSigned8() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRId8, int8_t(-17));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-17"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIi8, int8_t(42));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "42"));
}

static void TestPrintSigned16() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRId16, int16_t(-289));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-289"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIi16, int16_t(728));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "728"));
}

static void TestPrintSigned32() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRId32, int32_t(-342178));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-342178"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIi32, int32_t(5719283));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "5719283"));
}

static void TestPrintSigned64() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRId64, int64_t(-INT64_C(432157943248732)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-432157943248732"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIi64, int64_t(INT64_C(325719232983)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "325719232983"));
}

static void TestPrintSignedN() {
  TestPrintSigned8();
  TestPrintSigned16();
  TestPrintSigned32();
  TestPrintSigned64();
}

static void TestPrintSignedLeast8() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdLEAST8, int_least8_t(-17));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-17"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiLEAST8, int_least8_t(42));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "42"));
}

static void TestPrintSignedLeast16() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdLEAST16, int_least16_t(-289));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-289"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiLEAST16, int_least16_t(728));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "728"));
}

static void TestPrintSignedLeast32() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdLEAST32, int_least32_t(-342178));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-342178"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiLEAST32, int_least32_t(5719283));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "5719283"));
}

static void TestPrintSignedLeast64() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdLEAST64,
                 int_least64_t(-INT64_C(432157943248732)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-432157943248732"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiLEAST64,
                 int_least64_t(INT64_C(325719232983)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "325719232983"));
}

static void TestPrintSignedLeastN() {
  TestPrintSignedLeast8();
  TestPrintSignedLeast16();
  TestPrintSignedLeast32();
  TestPrintSignedLeast64();
}

static void TestPrintSignedFast8() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdFAST8, int_fast8_t(-17));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-17"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiFAST8, int_fast8_t(42));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "42"));
}

static void TestPrintSignedFast16() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdFAST16, int_fast16_t(-289));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-289"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiFAST16, int_fast16_t(728));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "728"));
}

static void TestPrintSignedFast32() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdFAST32, int_fast32_t(-342178));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-342178"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiFAST32, int_fast32_t(5719283));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "5719283"));
}

static void TestPrintSignedFast64() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdFAST64,
                 int_fast64_t(-INT64_C(432157943248732)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-432157943248732"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiFAST64, int_fast64_t(INT64_C(325719232983)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "325719232983"));
}

static void TestPrintSignedFastN() {
  TestPrintSignedFast8();
  TestPrintSignedFast16();
  TestPrintSignedFast32();
  TestPrintSignedFast64();
}

static void TestPrintSignedMax() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdMAX, intmax_t(-INTMAX_C(432157943248732)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "-432157943248732"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiMAX, intmax_t(INTMAX_C(325719232983)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "325719232983"));
}

static void TestPrintSignedPtr() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIdPTR,
                 intptr_t(reinterpret_cast<void*>(12345678)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "12345678"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIiPTR,
                 intptr_t(reinterpret_cast<void*>(87654321)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "87654321"));
}

static void TestPrintSigned() {
  TestPrintSignedN();
  TestPrintSignedLeastN();
  TestPrintSignedFastN();
  TestPrintSignedMax();
  TestPrintSignedPtr();
}

/*
 * The fprintf macros for unsigned integers are:
 *
 *   PRIoN   PRIoLEASTN   PRIoFASTN   PRIoMAX   PRIoPTR
 *   PRIuN   PRIuLEASTN   PRIuFASTN   PRIuMAX   PRIuPTR
 *   PRIxN   PRIxLEASTN   PRIxFASTN   PRIxMAX   PRIxPTR
 *   PRIXN   PRIXLEASTN   PRIXFASTN   PRIXMAX   PRIXPTR
 *
 * In these names N is the width of the type as described in C99 7.18.1.
 */

static void TestPrintUnsigned8() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIo8, uint8_t(042));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "42"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIu8, uint8_t(17));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "17"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIx8, uint8_t(0x2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIX8, uint8_t(0xCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CD"));
}

static void TestPrintUnsigned16() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIo16, uint16_t(04242));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "4242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIu16, uint16_t(1717));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "1717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIx16, uint16_t(0x2a2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIX16, uint16_t(0xCDCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCD"));
}

static void TestPrintUnsigned32() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIo32, uint32_t(0424242));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "424242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIu32, uint32_t(171717));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "171717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIx32, uint32_t(0x2a2a2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIX32, uint32_t(0xCDCDCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCDCD"));
}

static void TestPrintUnsigned64() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIo64, uint64_t(UINT64_C(0424242424242)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "424242424242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIu64, uint64_t(UINT64_C(17171717171717171717)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "17171717171717171717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIx64, uint64_t(UINT64_C(0x2a2a2a2a2a2a2a)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a2a2a2a2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIX64, uint64_t(UINT64_C(0xCDCDCDCDCDCD)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCDCDCDCDCD"));
}

static void TestPrintUnsignedN() {
  TestPrintUnsigned8();
  TestPrintUnsigned16();
  TestPrintUnsigned32();
  TestPrintUnsigned64();
}

static void TestPrintUnsignedLeast8() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoLEAST8, uint_least8_t(042));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "42"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuLEAST8, uint_least8_t(17));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "17"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxLEAST8, uint_least8_t(0x2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXLEAST8, uint_least8_t(0xCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CD"));
}

static void TestPrintUnsignedLeast16() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoLEAST16, uint_least16_t(04242));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "4242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuLEAST16, uint_least16_t(1717));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "1717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxLEAST16, uint_least16_t(0x2a2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXLEAST16, uint_least16_t(0xCDCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCD"));
}

static void TestPrintUnsignedLeast32() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoLEAST32, uint_least32_t(0424242));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "424242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuLEAST32, uint_least32_t(171717));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "171717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxLEAST32, uint_least32_t(0x2a2a2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXLEAST32, uint_least32_t(0xCDCDCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCDCD"));
}

static void TestPrintUnsignedLeast64() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoLEAST64,
                 uint_least64_t(UINT64_C(0424242424242)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "424242424242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuLEAST64,
                 uint_least64_t(UINT64_C(17171717171717171717)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "17171717171717171717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxLEAST64,
                 uint_least64_t(UINT64_C(0x2a2a2a2a2a2a2a)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a2a2a2a2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXLEAST64,
                 uint_least64_t(UINT64_C(0xCDCDCDCDCDCD)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCDCDCDCDCD"));
}

static void TestPrintUnsignedLeastN() {
  TestPrintUnsignedLeast8();
  TestPrintUnsignedLeast16();
  TestPrintUnsignedLeast32();
  TestPrintUnsignedLeast64();
}

static void TestPrintUnsignedFast8() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoFAST8, uint_fast8_t(042));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "42"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuFAST8, uint_fast8_t(17));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "17"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxFAST8, uint_fast8_t(0x2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXFAST8, uint_fast8_t(0xCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CD"));
}

static void TestPrintUnsignedFast16() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoFAST16, uint_fast16_t(04242));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "4242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuFAST16, uint_fast16_t(1717));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "1717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxFAST16, uint_fast16_t(0x2a2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXFAST16, uint_fast16_t(0xCDCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCD"));
}

static void TestPrintUnsignedFast32() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoFAST32, uint_fast32_t(0424242));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "424242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuFAST32, uint_fast32_t(171717));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "171717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxFAST32, uint_fast32_t(0x2a2a2a));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXFAST32, uint_fast32_t(0xCDCDCD));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCDCD"));
}

static void TestPrintUnsignedFast64() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoFAST64,
                 uint_fast64_t(UINT64_C(0424242424242)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "424242424242"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuFAST64,
                 uint_fast64_t(UINT64_C(17171717171717171717)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "17171717171717171717"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxFAST64,
                 uint_fast64_t(UINT64_C(0x2a2a2a2a2a2a2a)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "2a2a2a2a2a2a2a"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXFAST64,
                 uint_fast64_t(UINT64_C(0xCDCDCDCDCDCD)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "CDCDCDCDCDCD"));
}

static void TestPrintUnsignedFastN() {
  TestPrintUnsignedFast8();
  TestPrintUnsignedFast16();
  TestPrintUnsignedFast32();
  TestPrintUnsignedFast64();
}

static void TestPrintUnsignedMax() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoMAX, uintmax_t(UINTMAX_C(432157943248732)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "14220563454333534"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuMAX, uintmax_t(UINTMAX_C(325719232983)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "325719232983"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxMAX, uintmax_t(UINTMAX_C(327281321873)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "4c337ca791"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXMAX, uintmax_t(UINTMAX_C(912389523743523)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "33DD03D75A323"));
}

static void TestPrintUnsignedPtr() {
  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIoPTR,
                 uintptr_t(reinterpret_cast<void*>(12345678)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "57060516"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIuPTR,
                 uintptr_t(reinterpret_cast<void*>(87654321)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "87654321"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIxPTR,
                 uintptr_t(reinterpret_cast<void*>(0x4c3a791)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "4c3a791"));

  PoisonOutput();
  SprintfLiteral(gOutput, "%" PRIXPTR,
                 uintptr_t(reinterpret_cast<void*>(0xF328DB)));
  MOZ_RELEASE_ASSERT(!strcmp(gOutput, "F328DB"));
}

static void TestPrintUnsigned() {
  TestPrintUnsignedN();
  TestPrintUnsignedLeastN();
  TestPrintUnsignedFastN();
  TestPrintUnsignedMax();
  TestPrintUnsignedPtr();
}

static void TestPrint() {
  TestPrintSigned();
  TestPrintUnsigned();
}

/*
 * The fscanf macros for signed integers are:
 *
 *   SCNdN   SCNdLEASTN   SCNdFASTN   SCNdMAX   SCNdPTR
 *   SCNiN   SCNiLEASTN   SCNiFASTN   SCNiMAX   SCNiPTR
 *
 * In these names N is the width of the type as described in C99 7.18.1.
 */

/*
 * MSVC's scanf is insufficiently powerful to implement all the SCN* macros.
 * Rather than support some subset of them, we instead support none of them.
 * See the comment at the top of IntegerPrintfMacros.h.  But in case we ever do
 * support them, the following tests should adequately test implementation
 * correctness.  (Indeed, these tests *revealed* MSVC's limitations.)
 *
 * That said, even if MSVC ever picks up complete support, we still probably
 * don't want to support these, because of the undefined-behavior issue noted
 * further down in the comment atop IntegerPrintfMacros.h.
 */
#define SHOULD_TEST_SCANF_MACROS 0

#if SHOULD_TEST_SCANF_MACROS

/*
 * glibc header definitions for SCN{d,i,o,u,x}{,LEAST,FAST}8 use the "hh" length
 * modifier, which is new in C99 (and C++11, by reference).  We compile this
 * file as C++11, so if "hh" is used in these macros, it's standard.  But some
 * versions of gcc wrongly think it isn't and warn about a "non-standard"
 * modifier.  And since these tests mostly exist to verify format-macro/type
 * consistency (particularly through compiler warnings about incorrect formats),
 * these warnings are unacceptable.  So for now, compile tests for those macros
 * only if we aren't compiling with gcc.
 */
#  define SHOULD_TEST_8BIT_FORMAT_MACROS (!(MOZ_IS_GCC))

template <typename T>
union Input {
  T mI;
  unsigned char mPun[16];
};

template <typename T>
static void PoisonInput(Input<T>& aInput) {
  memset(aInput.mPun, 0xDA, sizeof(aInput.mPun));
}

template <typename T>
static bool ExtraBitsUntouched(const Input<T>& aInput) {
  for (size_t i = sizeof(aInput.mI); i < sizeof(aInput); i++) {
    if (aInput.mPun[i] != 0xDA) {
      return false;
    }
  }

  return true;
}

static void TestScanSigned8() {
#  if SHOULD_TEST_8BIT_FORMAT_MACROS
  Input<int8_t> u;

  PoisonInput(u);
  sscanf("-17", "%" SCNd8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -17);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("042", "%" SCNi8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 042);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
#  endif
}

static void TestScanSigned16() {
  Input<int16_t> u;

  PoisonInput(u);
  sscanf("-1742", "%" SCNd16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -1742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("04217", "%" SCNi16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 04217);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSigned32() {
  Input<int32_t> u;

  PoisonInput(u);
  sscanf("-174257", "%" SCNd32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -174257);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("0423571", "%" SCNi32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0423571);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSigned64() {
  Input<int64_t> u;

  PoisonInput(u);
  sscanf("-17425238927232", "%" SCNd64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -INT64_C(17425238927232));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("042333576571", "%" SCNi64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == INT64_C(042333576571));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedN() {
  TestScanSigned8();
  TestScanSigned16();
  TestScanSigned32();
  TestScanSigned64();
}

static void TestScanSignedLeast8() {
#  if SHOULD_TEST_8BIT_FORMAT_MACROS
  Input<int_least8_t> u;

  PoisonInput(u);
  sscanf("-17", "%" SCNdLEAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -17);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("042", "%" SCNiLEAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 042);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
#  endif
}

static void TestScanSignedLeast16() {
  Input<int_least16_t> u;

  PoisonInput(u);
  sscanf("-1742", "%" SCNdLEAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -1742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("04217", "%" SCNiLEAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 04217);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedLeast32() {
  Input<int_least32_t> u;

  PoisonInput(u);
  sscanf("-174257", "%" SCNdLEAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -174257);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("0423571", "%" SCNiLEAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0423571);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedLeast64() {
  Input<int_least64_t> u;

  PoisonInput(u);
  sscanf("-17425238927232", "%" SCNdLEAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -INT64_C(17425238927232));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("042333576571", "%" SCNiLEAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == INT64_C(042333576571));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedLeastN() {
  TestScanSignedLeast8();
  TestScanSignedLeast16();
  TestScanSignedLeast32();
  TestScanSignedLeast64();
}

static void TestScanSignedFast8() {
#  if SHOULD_TEST_8BIT_FORMAT_MACROS
  Input<int_fast8_t> u;

  PoisonInput(u);
  sscanf("-17", "%" SCNdFAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -17);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("042", "%" SCNiFAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 042);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
#  endif
}

static void TestScanSignedFast16() {
  Input<int_fast16_t> u;

  PoisonInput(u);
  sscanf("-1742", "%" SCNdFAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -1742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("04217", "%" SCNiFAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 04217);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedFast32() {
  Input<int_fast32_t> u;

  PoisonInput(u);
  sscanf("-174257", "%" SCNdFAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -174257);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("0423571", "%" SCNiFAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0423571);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedFast64() {
  Input<int_fast64_t> u;

  PoisonInput(u);
  sscanf("-17425238927232", "%" SCNdFAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -INT64_C(17425238927232));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("042333576571", "%" SCNiFAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == INT64_C(042333576571));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedFastN() {
  TestScanSignedFast8();
  TestScanSignedFast16();
  TestScanSignedFast32();
  TestScanSignedFast64();
}

static void TestScanSignedMax() {
  Input<intmax_t> u;

  PoisonInput(u);
  sscanf("-432157943248732", "%" SCNdMAX, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == -INTMAX_C(432157943248732));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("04233357236571", "%" SCNiMAX, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == INTMAX_C(04233357236571));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSignedPtr() {
  Input<intptr_t> u;

  PoisonInput(u);
  sscanf("12345678", "%" SCNdPTR, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == intptr_t(reinterpret_cast<void*>(12345678)));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("04233357236", "%" SCNiPTR, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == intptr_t(reinterpret_cast<void*>(04233357236)));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanSigned() {
  TestScanSignedN();
  TestScanSignedLeastN();
  TestScanSignedFastN();
  TestScanSignedMax();
  TestScanSignedPtr();
}

/*
 * The fscanf macros for unsigned integers are:
 *
 *   SCNoN   SCNoLEASTN   SCNoFASTN   SCNoMAX   SCNoPTR
 *   SCNuN   SCNuLEASTN   SCNuFASTN   SCNuMAX   SCNuPTR
 *   SCNxN   SCNxLEASTN   SCNxFASTN   SCNxMAX   SCNxPTR
 *
 * In these names N is the width of the type as described in C99 7.18.1.
 */

static void TestScanUnsigned8() {
#  if SHOULD_TEST_8BIT_FORMAT_MACROS
  Input<uint8_t> u;

  PoisonInput(u);
  sscanf("17", "%" SCNo8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 017);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("42", "%" SCNu8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 42);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2A", "%" SCNx8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2A);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
#  endif
}

static void TestScanUnsigned16() {
  Input<uint16_t> u;

  PoisonInput(u);
  sscanf("1742", "%" SCNo16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 01742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4217", "%" SCNu16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 4217);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2ABC", "%" SCNx16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2ABC);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsigned32() {
  Input<uint32_t> u;

  PoisonInput(u);
  sscanf("17421742", "%" SCNo32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 017421742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4217867", "%" SCNu32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 4217867);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2ABCBEEF", "%" SCNx32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2ABCBEEF);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsigned64() {
  Input<uint64_t> u;

  PoisonInput(u);
  sscanf("17421742173", "%" SCNo64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(017421742173));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("421786713579", "%" SCNu64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(421786713579));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("DEADBEEF7457E", "%" SCNx64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(0xDEADBEEF7457E));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedN() {
  TestScanUnsigned8();
  TestScanUnsigned16();
  TestScanUnsigned32();
  TestScanUnsigned64();
}

static void TestScanUnsignedLeast8() {
#  if SHOULD_TEST_8BIT_FORMAT_MACROS
  Input<uint_least8_t> u;

  PoisonInput(u);
  sscanf("17", "%" SCNoLEAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 017);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("42", "%" SCNuLEAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 42);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2A", "%" SCNxLEAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2A);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
#  endif
}

static void TestScanUnsignedLeast16() {
  Input<uint_least16_t> u;

  PoisonInput(u);
  sscanf("1742", "%" SCNoLEAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 01742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4217", "%" SCNuLEAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 4217);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2ABC", "%" SCNxLEAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2ABC);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedLeast32() {
  Input<uint_least32_t> u;

  PoisonInput(u);
  sscanf("17421742", "%" SCNoLEAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 017421742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4217867", "%" SCNuLEAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 4217867);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2ABCBEEF", "%" SCNxLEAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2ABCBEEF);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedLeast64() {
  Input<uint_least64_t> u;

  PoisonInput(u);
  sscanf("17421742173", "%" SCNoLEAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(017421742173));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("421786713579", "%" SCNuLEAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(421786713579));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("DEADBEEF7457E", "%" SCNxLEAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(0xDEADBEEF7457E));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedLeastN() {
  TestScanUnsignedLeast8();
  TestScanUnsignedLeast16();
  TestScanUnsignedLeast32();
  TestScanUnsignedLeast64();
}

static void TestScanUnsignedFast8() {
#  if SHOULD_TEST_8BIT_FORMAT_MACROS
  Input<uint_fast8_t> u;

  PoisonInput(u);
  sscanf("17", "%" SCNoFAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 017);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("42", "%" SCNuFAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 42);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2A", "%" SCNxFAST8, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2A);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
#  endif
}

static void TestScanUnsignedFast16() {
  Input<uint_fast16_t> u;

  PoisonInput(u);
  sscanf("1742", "%" SCNoFAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 01742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4217", "%" SCNuFAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 4217);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2ABC", "%" SCNxFAST16, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2ABC);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedFast32() {
  Input<uint_fast32_t> u;

  PoisonInput(u);
  sscanf("17421742", "%" SCNoFAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 017421742);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4217867", "%" SCNuFAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 4217867);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("2ABCBEEF", "%" SCNxFAST32, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == 0x2ABCBEEF);
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedFast64() {
  Input<uint_fast64_t> u;

  PoisonInput(u);
  sscanf("17421742173", "%" SCNoFAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(017421742173));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("421786713579", "%" SCNuFAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(421786713579));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("DEADBEEF7457E", "%" SCNxFAST64, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINT64_C(0xDEADBEEF7457E));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedFastN() {
  TestScanUnsignedFast8();
  TestScanUnsignedFast16();
  TestScanUnsignedFast32();
  TestScanUnsignedFast64();
}

static void TestScanUnsignedMax() {
  Input<uintmax_t> u;

  PoisonInput(u);
  sscanf("14220563454333534", "%" SCNoMAX, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINTMAX_C(432157943248732));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("432157943248732", "%" SCNuMAX, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINTMAX_C(432157943248732));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4c337ca791", "%" SCNxMAX, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == UINTMAX_C(327281321873));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsignedPtr() {
  Input<uintptr_t> u;

  PoisonInput(u);
  sscanf("57060516", "%" SCNoPTR, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == uintptr_t(reinterpret_cast<void*>(12345678)));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("87654321", "%" SCNuPTR, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == uintptr_t(reinterpret_cast<void*>(87654321)));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));

  PoisonInput(u);
  sscanf("4c3a791", "%" SCNxPTR, &u.mI);
  MOZ_RELEASE_ASSERT(u.mI == uintptr_t(reinterpret_cast<void*>(0x4c3a791)));
  MOZ_RELEASE_ASSERT(ExtraBitsUntouched(u));
}

static void TestScanUnsigned() {
  TestScanUnsignedN();
  TestScanUnsignedLeastN();
  TestScanUnsignedFastN();
  TestScanUnsignedMax();
  TestScanUnsignedPtr();
}

static void TestScan() {
  TestScanSigned();
  TestScanUnsigned();
}

#endif /* SHOULD_TEST_SCANF_MACROS */

#if defined(XP_WIN)
int wmain()
#else
int main()
#endif  // defined(XP_WIN)
{
  TestPrint();
#if SHOULD_TEST_SCANF_MACROS
  TestScan();
#endif
  return 0;
}
