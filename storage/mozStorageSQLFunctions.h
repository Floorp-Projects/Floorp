/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageSQLFunctions_h
#define mozStorageSQLFunctions_h

#include "sqlite3.h"
#include "nscore.h"

namespace mozilla {
namespace storage {

/**
 * Registers the functions declared here with the specified database.
 *
 * @param aDB
 *        The database we'll be registering the functions with.
 * @return the SQLite status code indicating success or failure.
 */
int registerFunctions(sqlite3* aDB);

////////////////////////////////////////////////////////////////////////////////
//// Predefined Functions

/**
 * Overridden function to perform the SQL functions UPPER and LOWER.  These
 * support unicode, which the default implementations do not do.
 *
 * @param aCtx
 *        The sqlite_context that this function is being called on.
 * @param aArgc
 *        The number of arguments the function is being called with.
 * @param aArgv
 *        An array of the arguments the functions is being called with.
 */
void caseFunction(sqlite3_context* aCtx, int aArgc, sqlite3_value** aArgv);

/**
 * Overridden function to perform the SQL function LIKE.  This supports unicode,
 * which the default implementation does not do.
 *
 * @param aCtx
 *        The sqlite_context that this function is being called on.
 * @param aArgc
 *        The number of arguments the function is being called with.
 * @param aArgv
 *        An array of the arguments the functions is being called with.
 */
void likeFunction(sqlite3_context* aCtx, int aArgc, sqlite3_value** aArgv);

/**
 * An implementation of the Levenshtein Edit Distance algorithm for use in
 * Sqlite queries.
 *
 * @param aCtx
 *        The sqlite_context that this function is being called on.
 * @param aArgc
 *        The number of arguments the function is being called with.
 * @param aArgv
 *        An array of the arguments the functions is being called with.
 */
void levenshteinDistanceFunction(sqlite3_context* aCtx, int aArgc,
                                 sqlite3_value** aArgv);

/**
 * An alternative string length function that uses XPCOM string classes for
 * string length calculation.
 *
 * @param aCtx
 *        The sqlite_context that this function is being called on.
 * @param aArgc
 *        The number of arguments the function is being called with.
 * @param aArgv
 *        An array of the arguments the functions is being called with.
 */
void utf16LengthFunction(sqlite3_context* aCtx, int aArgc,
                         sqlite3_value** aArgv);

}  // namespace storage
}  // namespace mozilla

#endif  // mozStorageSQLFunctions_h
