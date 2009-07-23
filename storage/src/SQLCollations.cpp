/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Storage code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "SQLCollations.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Local Helper Functions

namespace {

/**
 * Helper function for the UTF-8 locale collations.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
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
 * @param  aComparisonStrength
 *         The sorting strength, one of the nsICollation constants.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
int
localeCollationHelper8(void *aService,
                       int aLen1,
                       const void *aStr1,
                       int aLen2,
                       const void *aStr2,
                       PRInt32 aComparisonStrength)
{
  NS_ConvertUTF8toUTF16 str1(static_cast<const char *>(aStr1), aLen1);
  NS_ConvertUTF8toUTF16 str2(static_cast<const char *>(aStr2), aLen2);
  Service *serv = static_cast<Service *>(aService);
  return serv->localeCompareStrings(str1, str2, aComparisonStrength);
}

/**
 * Helper function for the UTF-16 locale collations.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes (not characters) in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2 as provided by SQLite.  It
 *         must be a non-null-terminated PRUnichar* buffer.
 * @param  aLen2
 *         The number of bytes (not characters) in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1 as provided by SQLite.  It
 *         must be a non-null-terminated PRUnichar* buffer.
 * @param  aComparisonStrength
 *         The sorting strength, one of the nsICollation constants.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
int
localeCollationHelper16(void *aService,
                        int aLen1,
                        const void *aStr1,
                        int aLen2,
                        const void *aStr2,
                        PRInt32 aComparisonStrength)
{
  const PRUnichar *buf1 = static_cast<const PRUnichar *>(aStr1);
  const PRUnichar *buf2 = static_cast<const PRUnichar *>(aStr2);

  // The second argument to the nsDependentSubstring constructor is exclusive:
  // It points to the PRUnichar immediately following the last one in the target
  // substring.  Since aLen1 and aLen2 are in bytes, divide by sizeof(PRUnichar)
  // so that the pointer arithmetic is correct.
  nsDependentSubstring str1(buf1, buf1 + (aLen1 / sizeof(PRUnichar)));
  nsDependentSubstring str2(buf2, buf2 + (aLen2 / sizeof(PRUnichar)));
  Service *serv = static_cast<Service *>(aService);
  return serv->localeCompareStrings(str1, str2, aComparisonStrength);
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// Exposed Functions

int
registerCollations(sqlite3 *aDB,
                   Service *aService)
{
  struct Collations {
    const char *zName;
    int enc;
    int(*xCompare)(void*, int, const void*, int, const void*);
  } collations[] = {
    {"locale",
     SQLITE_UTF8,
     localeCollation8},
    {"locale_case_sensitive",
     SQLITE_UTF8,
     localeCollationCaseSensitive8},
    {"locale_accent_sensitive",
     SQLITE_UTF8,
     localeCollationAccentSensitive8},
    {"locale_case_accent_sensitive",
     SQLITE_UTF8,
     localeCollationCaseAccentSensitive8},
    {"locale",
     SQLITE_UTF16,
     localeCollation16},
    {"locale_case_sensitive",
     SQLITE_UTF16,
     localeCollationCaseSensitive16},
    {"locale_accent_sensitive",
     SQLITE_UTF16,
     localeCollationAccentSensitive16},
    {"locale_case_accent_sensitive",
     SQLITE_UTF16,
     localeCollationCaseAccentSensitive16},
  };

  int rv = SQLITE_OK;
  for (size_t i = 0; SQLITE_OK == rv && i < NS_ARRAY_LENGTH(collations); ++i) {
    struct Collations *p = &collations[i];
    rv = ::sqlite3_create_collation(aDB, p->zName, p->enc, aService,
                                    p->xCompare);
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
//// SQL Collations

int
localeCollation8(void *aService,
                 int aLen1,
                 const void *aStr1,
                 int aLen2,
                 const void *aStr2)
{
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                nsICollation::kCollationCaseInSensitive);
}

int
localeCollationCaseSensitive8(void *aService,
                              int aLen1,
                              const void *aStr1,
                              int aLen2,
                              const void *aStr2)
{
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                nsICollation::kCollationAccentInsenstive);
}

int
localeCollationAccentSensitive8(void *aService,
                                int aLen1,
                                const void *aStr1,
                                int aLen2,
                                const void *aStr2)
{
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                nsICollation::kCollationCaseInsensitiveAscii);
}

int
localeCollationCaseAccentSensitive8(void *aService,
                                    int aLen1,
                                    const void *aStr1,
                                    int aLen2,
                                    const void *aStr2)
{
  return localeCollationHelper8(aService, aLen1, aStr1, aLen2, aStr2,
                                nsICollation::kCollationCaseSensitive);
}

int
localeCollation16(void *aService,
                  int aLen1,
                  const void *aStr1,
                  int aLen2,
                  const void *aStr2)
{
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 nsICollation::kCollationCaseInSensitive);
}

int
localeCollationCaseSensitive16(void *aService,
                               int aLen1,
                               const void *aStr1,
                               int aLen2,
                               const void *aStr2)
{
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 nsICollation::kCollationAccentInsenstive);
}

int
localeCollationAccentSensitive16(void *aService,
                                 int aLen1,
                                 const void *aStr1,
                                 int aLen2,
                                 const void *aStr2)
{
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 nsICollation::kCollationCaseInsensitiveAscii);
}

int
localeCollationCaseAccentSensitive16(void *aService,
                                     int aLen1,
                                     const void *aStr1,
                                     int aLen2,
                                     const void *aStr2)
{
  return localeCollationHelper16(aService, aLen1, aStr1, aLen2, aStr2,
                                 nsICollation::kCollationCaseSensitive);
}

} // namespace storage
} // namespace mozilla
