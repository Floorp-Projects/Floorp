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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#ifndef mozStoragePrivateHelpers_h
#define mozStoragePrivateHelpers_h

/**
 * This file contains convenience methods for mozStorage.
 */

#include "sqlite3.h"
#include "nsIVariant.h"
#include "mozStorage.h"
#include "jsapi.h"
#include "nsAutoPtr.h"

class mozIStorageCompletionCallback;
class mozIStorageBaseStatement;
class mozIStorageBindingParams;
class nsIRunnable;

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Macros

#define ENSURE_INDEX_VALUE(aIndex, aCount) \
  NS_ENSURE_TRUE(aIndex < aCount, NS_ERROR_INVALID_ARG)

////////////////////////////////////////////////////////////////////////////////
//// Functions

/**
 * Converts a SQLite return code to an nsresult return code.
 *
 * @param aSQLiteResultCode
 *        The SQLite return code to convert.
 * @returns the corresponding nsresult code for aSQLiteResultCode.
 */
nsresult convertResultCode(int aSQLiteResultCode);

/**
 * Checks the performance of a SQLite statement and logs a warning with
 * NS_WARNING.  Currently this only checks the number of sort operations done
 * on a statement, and if more than zero have been done, the statement can be
 * made faster with the careful use of an index.
 *
 * @param aStatement
 *        The sqlite3_stmt object to check.
 */
void checkAndLogStatementPerformance(sqlite3_stmt *aStatement);

/**
 * Convert the provided jsval into a variant representation if possible.
 *
 * @param aCtx
 *        The JSContext the value is from.
 * @param aValue
 *        The JavaScript value to convert.  All primitive types are supported,
 *        but only Date objects are supported from the Date family.  Date
 *        objects are coerced to PRTime (nanoseconds since epoch) values.
 * @return the variant if conversion was successful, nsnull if conversion
 *         failed.  The caller is responsible for addref'ing if non-null.
 */
nsIVariant *convertJSValToVariant(JSContext *aCtx, jsval aValue);

/**
 * Obtains an event that will notify a completion callback about completion.
 *
 * @param aCallback
 *        The callback to be notified.
 * @return an nsIRunnable that can be dispatched to the calling thread.
 */
already_AddRefed<nsIRunnable> newCompletionEvent(
  mozIStorageCompletionCallback *aCallback
);

} // namespace storage
} // namespace mozilla

#endif // mozStoragePrivateHelpers_h
