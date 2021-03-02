/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClassHashtable_h__
#define nsClassHashtable_h__

#include <utility>

#include "mozilla/UniquePtr.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"

/**
 * Helper class that provides methods to wrap and unwrap the UserDataType.
 */
template <class T>
class nsUniquePtrConverter {
 public:
  using UserDataType = T*;
  using DataType = mozilla::UniquePtr<T>;

  static UserDataType Unwrap(DataType& src) { return src.get(); }
  static DataType Wrap(UserDataType&& src) { return DataType(std::move(src)); }
  static DataType Wrap(const UserDataType& src) { return DataType(src); }
};

/**
 * templated hashtable class maps keys to C++ object pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param Class the class-type being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template <class KeyClass, class T>
class nsClassHashtable : public nsBaseHashtable<KeyClass, mozilla::UniquePtr<T>,
                                                T*, nsUniquePtrConverter<T>> {
 public:
  typedef typename KeyClass::KeyType KeyType;
  typedef T* UserDataType;
  typedef nsBaseHashtable<KeyClass, mozilla::UniquePtr<T>, T*,
                          nsUniquePtrConverter<T>>
      base_type;

  using base_type::IsEmpty;
  using base_type::Remove;

  nsClassHashtable() = default;
  explicit nsClassHashtable(uint32_t aInitLength) : base_type(aInitLength) {}

  /**
   * @copydoc nsBaseHashtable::Get
   * @param aData if the key doesn't exist, pData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* aData) const;

  /**
   * @copydoc nsBaseHashtable::Get
   * @returns nullptr if the key is not present.
   */
  [[nodiscard]] UserDataType Get(KeyType aKey) const;
};

template <typename K, typename T>
inline void ImplCycleCollectionUnlink(nsClassHashtable<K, T>& aField) {
  aField.Clear();
}

template <typename K, typename T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    const nsClassHashtable<K, T>& aField, const char* aName,
    uint32_t aFlags = 0) {
  for (auto iter = aField.ConstIter(); !iter.Done(); iter.Next()) {
    ImplCycleCollectionTraverse(aCallback, *iter.UserData(), aName, aFlags);
  }
}

//
// nsClassHashtable definitions
//

template <class KeyClass, class T>
bool nsClassHashtable<KeyClass, T>::Get(KeyType aKey, T** aRetVal) const {
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aRetVal) {
      *aRetVal = ent->GetData().get();
    }

    return true;
  }

  if (aRetVal) {
    *aRetVal = nullptr;
  }

  return false;
}

template <class KeyClass, class T>
T* nsClassHashtable<KeyClass, T>::Get(KeyType aKey) const {
  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return nullptr;
  }

  return ent->GetData().get();
}

#endif  // nsClassHashtable_h__
