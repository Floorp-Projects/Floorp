/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/intl/Collator.h"

#include "SQLCollations.h"

using mozilla::intl::Collator;

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Local Helper Functions

namespace {

/**
 * Helper function for the UTF-8 locale collations.
 *
 * @param  aService
 *         The Service that owns the collator used by this collation.
 * @param  aLen1
 *         The number of bytes in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2 as provided by SQLite.  It
 *         must be a non-null-terminated char* buffer.
 * @param  aLen2
 *         The number of bytes in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1 as provided by SQLite.  It
 *         must be a non-null-terminated char* buffer.
 * @param  aSensitivity
 *         The sorting sensitivity.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
int localeCollationHelper8(void* aService, int aLen1, const void* aStr1,
                           int aLen2, const void* aStr2,
                           Collator::Sensitivity aSensitivity) {
  NS_ConvertUTF8toUTF16 str1(static_cast<const char*>(aStr1), aLen1);
  NS_ConvertUTF8toUTF16 str2(static_cast<const char*>(aStr2), aLen2);
  Service* serv = static_cast<Service*>(aService);
  return serv->localeCompareStrings(str1, str2, aSensitivity);
}

/**
 * Helper function for the UTF-16 locale collations.
 *
 * @param  aService
 *         The Service that owns the collator used by this collation.
 * @param  aLen1
 *         The number of bytes (not characters) in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2 as provided by SQLite.  It
 *         must be a non-null-terminated char16_t* buffer.
 * @param  aLen2
 *         The number of bytes (not characters) in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1 as provided by SQLite.  It
 *         must be a non-null-terminated char16_t* buffer.
 * @param  aSensitivity
 *         The sorting sensitivity.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
int localeCollationHelper16(void* aService, int aLen1, const void* aStr1,
                            int aLen2, const void* aStr2,
                            Collator::Sensitivity aSensitivity) {
  const char16_t* buf1 = static_cast<const char16_t*>(aStr1);
  const char16_t* buf2 = static_cast<const char16_t*>(aStr2);

  // The second argument to the nsDependentSubstring constructor is exclusive:
  // It points to the char16_t immediately following the last one in the target
  // substring.  Since aLen1 and aLen2 are in bytes, divide by sizeof(char16_t)
  // so that the pointer arithmetic is correct.
  nsDependentSubstring str1(buf1, buf1 + (aLen1 / sizeof(char16_t)));
  nsDependentSubstring str2(buf2, buf2 + (aLen2 / sizeof(char16_t)));
  Service* serv = static_cast<Service*>(aService);
  return serv->localeCompareStrings(str1, str2, aSensitivity);
}

// This struct is used only by registerCollations below, but ISO C++98 forbids
// instantiating a template dependent on a locally-defined type.  Boo-urns!
struct Collations {
  const char* zName;
  int enc;
  int (*xCompare)(void*, int, const void*, int, const void*);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//// Exposed Functions

int registerCollations(sqlite3* aDB, Service* aService) {
  Collations collations[] = {
      {"locale", SQLITE_UTF8, localeCollation8},
      {"locale_case_sensitive", SQLITE_UTF8, localeCollationCaseSensitive8},
      {"locale_accent_sensitive", SQLITE_UTF8, localeCollationAccentSensitive8},
      {"locale_case_accent_sensitive", SQLITE_UTF8,
       localeCollationCaseAccentSensitive8},
      {"locale", SQLITE_UTF16, localeCollation16},
      {"locale_case_sensitive", SQLITE_UTF16, localeCollationCaseSensitive16},
      {"locale_accent_sensitive", SQLITE_UTF16,
       localeCollationAccentSensitive16},
      {"locale_case_accent_sensitive", SQLITE_UTF16,
       localeCollationCaseAccentSensitive16},
  };

  int rv = SQLITE_OK;
  for (size_t i = 0; SQLITE_OK == rv && i < ArrayLength(collations); ++i) {
    struct Collations* p = &collations[i];
    rv = ::sqlite3_create_collation(aDB, p->zName, p->enc, aService,
                                    p->xCompare);
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
//// SQL Collations

int localeCollation8(void* aService, int aLen1, const void* aStr1, int aLen2,
                     const void* aStr2) {
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                Collator::Sensitivity::Base);
}

int localeCollationCaseSensitive8(void* aService, int aLen1, const void* aStr1,
                                  int aLen2, const void* aStr2) {
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                Collator::Sensitivity::Case);
}

int localeCollationAccentSensitive8(void* aService, int aLen1,
                                    const void* aStr1, int aLen2,
                                    const void* aStr2) {
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                Collator::Sensitivity::Accent);
}

int localeCollationCaseAccentSensitive8(void* aService, int aLen1,
                                        const void* aStr1, int aLen2,
                                        const void* aStr2) {
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                Collator::Sensitivity::Variant);
}

int localeCollation16(void* aService, int aLen1, const void* aStr1, int aLen2,
                      const void* aStr2) {
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 Collator::Sensitivity::Base);
}

int localeCollationCaseSensitive16(void* aService, int aLen1, const void* aStr1,
                                   int aLen2, const void* aStr2) {
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 Collator::Sensitivity::Case);
}

int localeCollationAccentSensitive16(void* aService, int aLen1,
                                     const void* aStr1, int aLen2,
                                     const void* aStr2) {
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 Collator::Sensitivity::Accent);
}

int localeCollationCaseAccentSensitive16(void* aService, int aLen1,
                                         const void* aStr1, int aLen2,
                                         const void* aStr2) {
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 Collator::Sensitivity::Variant);
}

}  // namespace storage
}  // namespace mozilla
