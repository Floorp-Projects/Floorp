/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsID.h"

#include <limits.h>

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

  if (!NSS_IsInitialized()) {
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

static const char gIDFormat[] =
    "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";

/*
 * Returns an allocated string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
 * format. The string is allocated with moz_xmalloc and should be freed by
 * the caller.
 */

char* nsID::ToString() const {
  char* res = (char*)moz_xmalloc(NSID_LENGTH);
  snprintf(res, NSID_LENGTH, gIDFormat, m0, (uint32_t)m1, (uint32_t)m2,
           (uint32_t)m3[0], (uint32_t)m3[1], (uint32_t)m3[2], (uint32_t)m3[3],
           (uint32_t)m3[4], (uint32_t)m3[5], (uint32_t)m3[6], (uint32_t)m3[7]);
  return res;
}

void nsID::ToProvidedString(char (&aDest)[NSID_LENGTH]) const {
  SprintfLiteral(aDest, gIDFormat, m0, (uint32_t)m1, (uint32_t)m2,
                 (uint32_t)m3[0], (uint32_t)m3[1], (uint32_t)m3[2],
                 (uint32_t)m3[3], (uint32_t)m3[4], (uint32_t)m3[5],
                 (uint32_t)m3[6], (uint32_t)m3[7]);
}

#endif  // XPCOM_GLUE_AVOID_NSPR

nsID* nsID::Clone() const {
  auto id = static_cast<nsID*>(moz_xmalloc(sizeof(nsID)));
  *id = *this;
  return id;
}
