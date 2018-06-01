/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDataHashtable_h__
#define nsDataHashtable_h__

#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "mozilla/Maybe.h"

/**
 * templated hashtable class maps keys to simple datatypes.
 * See nsBaseHashtable for complete declaration
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the simple datatype being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template<class KeyClass, class DataType>
class nsDataHashtable
  : public nsBaseHashtable<KeyClass, DataType, DataType>
{
private:
  typedef nsBaseHashtable<KeyClass, DataType, DataType> BaseClass;

public:
  using typename BaseClass::KeyType;
  using typename BaseClass::EntryType;

  nsDataHashtable() {}
  explicit nsDataHashtable(uint32_t aInitLength)
    : BaseClass(aInitLength)
  {
  }

  /**
   * Retrieve a reference to the value for a key.
   *
   * @param aKey the key to retrieve.
   * @return a reference to the found value, or nullptr if no entry was found
   * with the given key.
   */
  DataType* GetValue(KeyType aKey)
  {
    if (EntryType* ent = this->GetEntry(aKey)) {
      return &ent->mData;
    }
    return nullptr;
  }

  /**
   * Retrieve the value for a key and remove the corresponding entry at
   * the same time.
   *
   * @param aKey the key to retrieve and remove
   * @return the found value, or Nothing if no entry was found with the
   *   given key.
   */
  mozilla::Maybe<DataType> GetAndRemove(KeyType aKey)
  {
    mozilla::Maybe<DataType> value;
    if (EntryType* ent = this->GetEntry(aKey)) {
      value.emplace(std::move(ent->mData));
      this->RemoveEntry(ent);
    }
    return value;
  }
};

#endif // nsDataHashtable_h__
