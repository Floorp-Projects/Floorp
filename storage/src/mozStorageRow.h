/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageRow_h
#define mozStorageRow_h

#include "mozIStorageRow.h"
#include "nsCOMArray.h"
#include "nsDataHashtable.h"
#include "mozilla/Attributes.h"
class nsIVariant;
struct sqlite3_stmt;

namespace mozilla {
namespace storage {

class Row MOZ_FINAL : public mozIStorageRow
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEROW
  NS_DECL_MOZISTORAGEVALUEARRAY

  /**
   * Initializes the object with the given statement.  Copies the values from
   * the statement.
   *
   * @param aStatement
   *        The sqlite statement to pull results from.
   */
  nsresult initialize(sqlite3_stmt *aStatement);

private:
  /**
   * The number of columns in this tuple.
   */
  PRUint32 mNumCols;

  /**
   * Stores the data in the tuple.
   */
  nsCOMArray<nsIVariant> mData;

  /**
   * Maps a given name to a column index.
   */
  nsDataHashtable<nsCStringHashKey, PRUint32> mNameHashtable;
};

} // namespace storage
} // namespace mozilla

#endif // mozStorageRow_h
