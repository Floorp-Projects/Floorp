/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStoragePrivateHelpers_h
#define mozStoragePrivateHelpers_h

/**
 * This file contains convenience methods for mozStorage.
 */

#include "sqlite3.h"
#include "nsIVariant.h"
#include "nsError.h"
#include "js/TypeDecls.h"
#include "Variant.h"

class mozIStorageCompletionCallback;
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
void checkAndLogStatementPerformance(sqlite3_stmt* aStatement);

/**
 * Convert the provided JS::Value into a variant representation if possible.
 *
 * @param aCtx
 *        The JSContext the value is from.
 * @param aValue
 *        The JavaScript value to convert.  All primitive types are supported,
 *        but only Date objects are supported from the Date family.  Date
 *        objects are coerced to PRTime (nanoseconds since epoch) values.
 * @return the variant if conversion was successful, nullptr if conversion
 *         failed.  The caller is responsible for addref'ing if non-null.
 */
nsIVariant* convertJSValToVariant(JSContext* aCtx, const JS::Value& aValue);

/**
 * Convert a provided nsIVariant implementation to our own thread-safe
 * refcounting implementation, if needed.
 *
 * @param aValue
 *        The original nsIVariant to be converted.
 * @return a thread-safe refcounting nsIVariant implementation.
 */
Variant_base* convertVariantToStorageVariant(nsIVariant* aVariant);

/**
 * Obtains an event that will notify a completion callback about completion.
 *
 * @param aCallback
 *        The callback to be notified.
 * @return an nsIRunnable that can be dispatched to the calling thread.
 */
already_AddRefed<nsIRunnable> newCompletionEvent(
    mozIStorageCompletionCallback* aCallback);

/**
 * Utility method to get a Blob as a string value.  The string expects
 * the interface exposed by nsAString/nsACString/etc.
 */
template <class T, class V>
nsresult DoGetBlobAsString(T* aThis, uint32_t aIndex, V& aValue) {
  typedef typename V::char_type char_type;

  uint32_t size;
  char_type* blob;
  nsresult rv =
      aThis->GetBlob(aIndex, &size, reinterpret_cast<uint8_t**>(&blob));
  NS_ENSURE_SUCCESS(rv, rv);

  aValue.Assign(blob, size / sizeof(char_type));
  delete[] blob;
  return NS_OK;
}

/**
 * Utility method to bind a string value as a Blob.  The string expects
 * the interface exposed by nsAString/nsACString/etc.
 */
template <class T, class V>
nsresult DoBindStringAsBlobByName(T* aThis, const nsACString& aName,
                                  const V& aValue) {
  typedef typename V::char_type char_type;
  return aThis->BindBlobByName(
      aName, reinterpret_cast<const uint8_t*>(aValue.BeginReading()),
      aValue.Length() * sizeof(char_type));
}

/**
 * Utility method to bind a string value as a Blob.  The string expects
 * the interface exposed by nsAString/nsACString/etc.
 */
template <class T, class V>
nsresult DoBindStringAsBlobByIndex(T* aThis, uint32_t aIndex, const V& aValue) {
  typedef typename V::char_type char_type;
  return aThis->BindBlobByIndex(
      aIndex, reinterpret_cast<const uint8_t*>(aValue.BeginReading()),
      aValue.Length() * sizeof(char_type));
}

}  // namespace storage
}  // namespace mozilla

#endif  // mozStoragePrivateHelpers_h
