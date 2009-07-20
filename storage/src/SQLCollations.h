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

#ifndef _mozilla_storage_SQLCollations_h_
#define _mozilla_storage_SQLCollations_h_

#include "mozStorageService.h"
#include "nscore.h"
#include "nsString.h"

#include "sqlite3.h"

namespace mozilla {
namespace storage {

/**
 * Registers the collating sequences declared here with the specified
 * database and Service.
 *
 * @param  aDB
 *         The database we'll be registering the collations with.
 * @param  aService
 *         The Service that owns the nsICollation used by our collations.
 * @return the SQLite status code indicating success or failure.
 */
NS_HIDDEN_(int) registerCollations(sqlite3 *aDB, Service *aService);

////////////////////////////////////////////////////////////////////////////////
//// Predefined Functions

/**
 * Custom UTF-8 collating sequence that respects the application's locale.
 * Comparison is case- and accent-insensitive.  This is called by SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @param  aLen2
 *         The number of bytes in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollation8(void *aService,
                                 int aLen1,
                                 const void *aStr1,
                                 int aLen2,
                                 const void *aStr2);

/**
 * Custom UTF-8 collating sequence that respects the application's locale.
 * Comparison is case-sensitive and accent-insensitive.  This is called by
 * SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @param  aLen2
 *         The number of bytes in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollationCaseSensitive8(void *aService,
                                              int aLen1,
                                              const void *aStr1,
                                              int aLen2,
                                              const void *aStr2);

/**
 * Custom UTF-8 collating sequence that respects the application's locale.
 * Comparison is case-insensitive and accent-sensitive.  This is called by
 * SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @param  aLen2
 *         The number of bytes in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollationAccentSensitive8(void *aService,
                                                int aLen1,
                                                const void *aStr1,
                                                int aLen2,
                                                const void *aStr2);

/**
 * Custom UTF-8 collating sequence that respects the application's locale.
 * Comparison is case- and accent-sensitive.  This is called by SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @param  aLen2
 *         The number of bytes in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated char* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollationCaseAccentSensitive8(void *aService,
                                                    int aLen1,
                                                    const void *aStr1,
                                                    int aLen2,
                                                    const void *aStr2);

/**
 * Custom UTF-16 collating sequence that respects the application's locale.
 * Comparison is case- and accent-insensitive.  This is called by SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes (not characters) in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @param  aLen2
 *         The number of bytes (not characters) in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollation16(void *aService,
                                  int aLen1,
                                  const void *aStr1,
                                  int aLen2,
                                  const void *aStr2);

/**
 * Custom UTF-16 collating sequence that respects the application's locale.
 * Comparison is case-sensitive and accent-insensitive.  This is called by
 * SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes (not characters) in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @param  aLen2
 *         The number of bytes (not characters) in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollationCaseSensitive16(void *aService,
                                               int aLen1,
                                               const void *aStr1,
                                               int aLen2,
                                               const void *aStr2);

/**
 * Custom UTF-16 collating sequence that respects the application's locale.
 * Comparison is case-insensitive and accent-sensitive.  This is called by
 * SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes (not characters) in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @param  aLen2
 *         The number of bytes (not characters) in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollationAccentSensitive16(void *aService,
                                                 int aLen1,
                                                 const void *aStr1,
                                                 int aLen2,
                                                 const void *aStr2);

/**
 * Custom UTF-16 collating sequence that respects the application's locale.
 * Comparison is case- and accent-sensitive.  This is called by SQLite.
 *
 * @param  aService
 *         The Service that owns the nsICollation used by this collation.
 * @param  aLen1
 *         The number of bytes (not characters) in aStr1.
 * @param  aStr1
 *         The string to be compared against aStr2.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @param  aLen2
 *         The number of bytes (not characters) in aStr2.
 * @param  aStr2
 *         The string to be compared against aStr1.  It will be passed in by
 *         SQLite as a non-null-terminated PRUnichar* buffer.
 * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative number.
 *         If aStr1 > aStr2, returns a positive number.  If aStr1 == aStr2,
 *         returns 0.
 */
NS_HIDDEN_(int) localeCollationCaseAccentSensitive16(void *aService,
                                                     int aLen1,
                                                     const void *aStr1,
                                                     int aLen2,
                                                     const void *aStr2);

} // namespace storage
} // namespace mozilla

#endif // _mozilla_storage_SQLCollations_h_
