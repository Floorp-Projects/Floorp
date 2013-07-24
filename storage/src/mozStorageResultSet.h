/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageResultSet_h
#define mozStorageResultSet_h

#include "mozIStorageResultSet.h"
#include "nsCOMArray.h"
#include "mozilla/Attributes.h"
class mozIStorageRow;

namespace mozilla {
namespace storage {

class ResultSet MOZ_FINAL : public mozIStorageResultSet
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGERESULTSET

  ResultSet();
  ~ResultSet();

  /**
   * Adds a tuple to this result set.
   */
  nsresult add(mozIStorageRow *aTuple);

  /**
   * @returns the number of rows this result set holds.
   */
  int32_t rows() const { return mData.Count(); }

private:
  /**
   * Stores the current index of the active result set.
   */
  int32_t mCurrentIndex;

  /**
   * Stores the tuples.
   */
  nsCOMArray<mozIStorageRow> mData;
};

} // namespace storage
} // namespace mozilla

#endif // mozStorageResultSet_h
