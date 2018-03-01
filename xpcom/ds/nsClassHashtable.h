/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClassHashtable_h__
#define nsClassHashtable_h__

#include "mozilla/Move.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"

/**
 * templated hashtable class maps keys to C++ object pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param Class the class-type being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template<class KeyClass, class T>
class nsClassHashtable
  : public nsBaseHashtable<KeyClass, nsAutoPtr<T>, T*>
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef T* UserDataType;
  typedef nsBaseHashtable<KeyClass, nsAutoPtr<T>, T*> base_type;

  using base_type::IsEmpty;
  using base_type::Remove;

  nsClassHashtable() {}
  explicit nsClassHashtable(uint32_t aInitLength)
    : nsBaseHashtable<KeyClass, nsAutoPtr<T>, T*>(aInitLength)
  {
  }

  /**
   * Looks up aKey in the hash table. If it doesn't exist a new object of
   * KeyClass will be created (using the arguments provided) and then returned.
   */
  template<typename... Args>
  UserDataType LookupOrAdd(KeyType aKey, Args&&... aConstructionArgs);

  /**
   * @copydoc nsBaseHashtable::Get
   * @param aData if the key doesn't exist, pData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* aData) const;

  /**
   * @copydoc nsBaseHashtable::Get
   * @returns nullptr if the key is not present.
   */
  UserDataType Get(KeyType aKey) const;
};

//
// nsClassHashtable definitions
//

template<class KeyClass, class T>
template<typename... Args>
T*
nsClassHashtable<KeyClass, T>::LookupOrAdd(KeyType aKey,
                                           Args&&... aConstructionArgs)
{
  auto count = this->Count();
  typename base_type::EntryType* ent = this->PutEntry(aKey);
  if (count != this->Count()) {
    ent->mData = new T(mozilla::Forward<Args>(aConstructionArgs)...);
  }
  return ent->mData;
}

template<class KeyClass, class T>
bool
nsClassHashtable<KeyClass, T>::Get(KeyType aKey, T** aRetVal) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aRetVal) {
      *aRetVal = ent->mData;
    }

    return true;
  }

  if (aRetVal) {
    *aRetVal = nullptr;
  }

  return false;
}

template<class KeyClass, class T>
T*
nsClassHashtable<KeyClass, T>::Get(KeyType aKey) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return nullptr;
  }

  return ent->mData;
}

#endif // nsClassHashtable_h__
