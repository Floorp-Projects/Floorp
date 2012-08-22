/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageBindingParamsArray_h
#define mozStorageBindingParamsArray_h

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

#include "mozIStorageBindingParamsArray.h"

namespace mozilla {
namespace storage {

class StorageBaseStatementInternal;

class BindingParamsArray MOZ_FINAL : public mozIStorageBindingParamsArray
{
  typedef nsTArray< nsCOMPtr<mozIStorageBindingParams> > array_type;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEBINDINGPARAMSARRAY

  BindingParamsArray(StorageBaseStatementInternal *aOwningStatement);

  typedef array_type::size_type size_type;

  /**
   * Locks the array and prevents further modification to it (such as adding
   * more elements to it).
   */
  void lock();

  /**
   * @return the pointer to the owning BindingParamsArray.
   */
  const StorageBaseStatementInternal *getOwner() const;

  /**
   * @return the number of elemets the array contains.
   */
  const size_type length() const { return mArray.Length(); }

  class iterator {
  public:
    iterator(BindingParamsArray *aArray,
             uint32_t aIndex)
    : mArray(aArray)
    , mIndex(aIndex)
    {
    }

    iterator &operator++(int)
    {
      mIndex++;
      return *this;
    }

    bool operator==(const iterator &aOther) const
    {
      return mIndex == aOther.mIndex;
    }
    bool operator!=(const iterator &aOther) const
    {
      return !(*this == aOther);
    }
    mozIStorageBindingParams *operator*()
    {
      NS_ASSERTION(mIndex < mArray->length(),
                   "Dereferenceing an invalid value!");
      return mArray->mArray[mIndex].get();
    }
  private:
    void operator--() { }
    BindingParamsArray *mArray;
    uint32_t mIndex;
  };

  /**
   * Obtains an iterator pointing to the beginning of the array.
   */
  inline iterator begin()
  {
    NS_ASSERTION(length() != 0,
                 "Obtaining an iterator to the beginning with no elements!");
    return iterator(this, 0);
  }

  /**
   * Obtains an iterator pointing to the end of the array.
   */
  inline iterator end()
  {
    NS_ASSERTION(mLocked,
                 "Obtaining an iterator to the end when we are not locked!");
    return iterator(this, length());
  }
private:
  nsCOMPtr<StorageBaseStatementInternal> mOwningStatement;
  array_type mArray;
  bool mLocked;

  friend class iterator;
};

} // namespace storage
} // namespace mozilla

#endif // mozStorageBindingParamsArray_h
