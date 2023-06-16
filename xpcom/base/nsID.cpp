/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsID.h"

#include <limits.h>

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/RandomNum.h"
#include "mozilla/Sprintf.h"
#include "nss.h"
#include "ScopedNSSTypes.h"

[[nodiscard]] static bool GenerateRandomBytesFromNSS(void* aBuffer,
                                                     size_t aLength) {
  MOZ_ASSERT(aBuffer);

  // Bounds check that we can safely truncate size_t `aLength` to an int.
  if (aLength == 0 || aLength > INT_MAX) {
    MOZ_ASSERT_UNREACHABLE("Bad aLength");
    return false;
  }
  int len = static_cast<int>(aLength);

  // Only try to use NSS on the main thread.
  if (!NS_IsMainThread() || !NSS_IsInitialized()) {
    return false;
  }

  mozilla::UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    MOZ_ASSERT_UNREACHABLE("Null slot");
    return false;
  }

  SECStatus srv = PK11_GenerateRandomOnSlot(
      slot.get(), static_cast<unsigned char*>(aBuffer), len);
  MOZ_ASSERT(srv == SECSuccess);
  return (srv == SECSuccess);
}

nsresult nsID::GenerateUUIDInPlace(nsID& aId) {
  // Firefox needs to generate some UUIDs before NSS has been initialized. We
  // prefer NSS's RNG, but if NSS is not available yet or returns an error, fall
  // back to MFBT's GenerateRandomBytes().
  if (!GenerateRandomBytesFromNSS(&aId, sizeof(nsID)) &&
      !mozilla::GenerateRandomBytesFromOS(&aId, sizeof(nsID))) {
    MOZ_ASSERT_UNREACHABLE("GenerateRandomBytesFromOS() failed");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Put in the version
  aId.m2 &= 0x0fff;
  aId.m2 |= 0x4000;

  // Put in the variant
  aId.m3[0] &= 0x3f;
  aId.m3[0] |= 0x80;

  return NS_OK;
}

nsID nsID::GenerateUUID() {
  nsID uuid;
  nsresult rv = GenerateUUIDInPlace(uuid);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  return uuid;
}

void nsID::Clear() {
  m0 = 0;
  m1 = 0;
  m2 = 0;
  memset(m3, 0, sizeof(m3));
}

/**
 * Multiplies the_int_var with 16 (0x10) and adds the value of the
 * hexadecimal digit the_char. If it fails it returns false from
 * the function it's used in.
 */

#define ADD_HEX_CHAR_TO_INT_OR_RETURN_FALSE(the_char, the_int_var) \
  the_int_var = (the_int_var << 4) + the_char;                     \
  if (the_char >= '0' && the_char <= '9')                          \
    the_int_var -= '0';                                            \
  else if (the_char >= 'a' && the_char <= 'f')                     \
    the_int_var -= 'a' - 10;                                       \
  else if (the_char >= 'A' && the_char <= 'F')                     \
    the_int_var -= 'A' - 10;                                       \
  else                                                             \
    return false

/**
 * Parses number_of_chars characters from the char_pointer pointer and
 * puts the number in the dest_variable. The pointer is moved to point
 * at the first character after the parsed ones. If it fails it returns
 * false from the function the macro is used in.
 */

#define PARSE_CHARS_TO_NUM(char_pointer, dest_variable, number_of_chars) \
  do {                                                                   \
    int32_t _i = number_of_chars;                                        \
    dest_variable = 0;                                                   \
    while (_i) {                                                         \
      ADD_HEX_CHAR_TO_INT_OR_RETURN_FALSE(*char_pointer, dest_variable); \
      char_pointer++;                                                    \
      _i--;                                                              \
    }                                                                    \
  } while (0)

/**
 * Parses a hyphen from the char_pointer string. If there is no hyphen there
 * the function returns false from the function it's used in. The
 * char_pointer is advanced one step.
 */

#define PARSE_HYPHEN(char_pointer) \
  if (*(char_pointer++) != '-') return false

/*
 * Turns a {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} string into
 * an nsID. It can also handle the old format without the { and }.
 */

bool nsID::Parse(const char* aIDStr) {
  /* Optimized for speed */
  if (!aIDStr) {
    return false;
  }

  bool expectFormat1 = (aIDStr[0] == '{');
  if (expectFormat1) {
    ++aIDStr;
  }

  PARSE_CHARS_TO_NUM(aIDStr, m0, 8);
  PARSE_HYPHEN(aIDStr);
  PARSE_CHARS_TO_NUM(aIDStr, m1, 4);
  PARSE_HYPHEN(aIDStr);
  PARSE_CHARS_TO_NUM(aIDStr, m2, 4);
  PARSE_HYPHEN(aIDStr);
  int i;
  for (i = 0; i < 2; ++i) {
    PARSE_CHARS_TO_NUM(aIDStr, m3[i], 2);
  }
  PARSE_HYPHEN(aIDStr);
  while (i < 8) {
    PARSE_CHARS_TO_NUM(aIDStr, m3[i], 2);
    i++;
  }

  return expectFormat1 ? *aIDStr == '}' : true;
}

#ifndef XPCOM_GLUE_AVOID_NSPR

/*
 * Returns a managed string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
 * format.
 */
nsIDToCString nsID::ToString() const { return nsIDToCString(*this); }

static const char sHexChars[256 * 2 + 1] =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f"
    "303132333435363738393a3b3c3d3e3f"
    "404142434445464748494a4b4c4d4e4f"
    "505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f"
    "707172737475767778797a7b7c7d7e7f"
    "808182838485868788898a8b8c8d8e8f"
    "909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
    "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
    "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

// Writes a zero-padded 8-bit integer as lowercase hex into aDest[0..2]
static void ToHex8Bit(uint8_t aValue, char* aDest) {
  aDest[0] = sHexChars[2 * aValue];
  aDest[1] = sHexChars[2 * aValue + 1];
}

// Writes a zero-padded 16-bit integer as lowercase hex into aDest[0..4]
static void ToHex16Bit(uint16_t aValue, char* aDest) {
  const uint8_t hi = (aValue >> 8);
  const uint8_t lo = aValue;
  ToHex8Bit(hi, &aDest[0]);
  ToHex8Bit(lo, &aDest[2]);
}

// Writes a zero-padded 32-bit integer as lowercase hex into aDest[0..8]
static void ToHex32Bit(uint32_t aValue, char* aDest) {
  const uint16_t hi = (aValue >> 16);
  const uint16_t lo = aValue;
  ToHex16Bit(hi, &aDest[0]);
  ToHex16Bit(lo, &aDest[4]);
}

void nsID::ToProvidedString(char (&aDest)[NSID_LENGTH]) const {
  // Stringify manually, for best performance.
  //
  // "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"
  // "{ecc35fd2-9029-4287-a826-b0a241d48d31}"
  aDest[0] = '{';
  ToHex32Bit(m0, &aDest[1]);
  aDest[9] = '-';
  ToHex16Bit(m1, &aDest[10]);
  aDest[14] = '-';
  ToHex16Bit(m2, &aDest[15]);
  aDest[19] = '-';
  ToHex8Bit(m3[0], &aDest[20]);
  ToHex8Bit(m3[1], &aDest[22]);
  aDest[24] = '-';
  ToHex8Bit(m3[2], &aDest[25]);
  ToHex8Bit(m3[3], &aDest[27]);
  ToHex8Bit(m3[4], &aDest[29]);
  ToHex8Bit(m3[5], &aDest[31]);
  ToHex8Bit(m3[6], &aDest[33]);
  ToHex8Bit(m3[7], &aDest[35]);
  aDest[37] = '}';
  aDest[38] = '\0';
}

#endif  // XPCOM_GLUE_AVOID_NSPR

nsID* nsID::Clone() const {
  auto id = static_cast<nsID*>(moz_xmalloc(sizeof(nsID)));
  *id = *this;
  return id;
}
