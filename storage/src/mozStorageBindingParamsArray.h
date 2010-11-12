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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#ifndef mozStorageBindingParamsArray_h
#define mozStorageBindingParamsArray_h

#include "nsAutoPtr.h"
#include "nsTArray.h"

#include "mozIStorageBindingParamsArray.h"

namespace mozilla {
namespace storage {

class StorageBaseStatementInternal;

class BindingParamsArray : public mozIStorageBindingParamsArray
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
             PRUint32 aIndex)
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
    PRUint32 mIndex;
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
