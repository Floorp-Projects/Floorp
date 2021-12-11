/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageRow_h
#define mozStorageRow_h

#include "mozIStorageRow.h"
#include "nsCOMArray.h"
#include "nsTHashMap.h"
#include "mozilla/Attributes.h"
class nsIVariant;
struct sqlite3_stmt;

namespace mozilla {
namespace storage {

class Row final : public mozIStorageRow {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEROW
  NS_DECL_MOZISTORAGEVALUEARRAY

  Row() : mNumCols(0) {}

  /**
   * Initializes the object with the given statement.  Copies the values from
   * the statement.
   *
   * @param aStatement
   *        The sqlite statement to pull results from.
   */
  nsresult initialize(sqlite3_stmt* aStatement);

 private:
  ~Row() {}

  /**
   * The number of columns in this tuple.
   */
  uint32_t mNumCols;

  /**
   * Stores the data in the tuple.
   */
  nsCOMArray<nsIVariant> mData;

  /**
   * Maps a given name to a column index.
   */
  nsTHashMap<nsCStringHashKey, uint32_t> mNameHashtable;
};

}  // namespace storage
}  // namespace mozilla

#endif  // mozStorageRow_h
