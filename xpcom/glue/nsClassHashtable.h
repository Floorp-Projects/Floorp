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

  nsClassHashtable() {}
  explicit nsClassHashtable(uint32_t aInitSize)
    : nsBaseHashtable<KeyClass, nsAutoPtr<T>, T*>(aInitSize)
  {
  }

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

  /**
   * Remove the entry for the given key from the hashtable and return it in
   * aOut.  If the key is not in the hashtable, aOut's pointer is set to
   * nullptr.
   *
   * Normally, an entry is deleted when it's removed from an nsClassHashtable,
   * but this function transfers ownership of the entry back to the caller
   * through aOut -- the entry will be deleted when aOut goes out of scope.
   *
   * @param aKey the key to get and remove from the hashtable
   */
  void RemoveAndForget(KeyType aKey, nsAutoPtr<T>& aOut);
};

//
// nsClassHashtable definitions
//

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

template<class KeyClass, class T>
void
nsClassHashtable<KeyClass, T>::RemoveAndForget(KeyType aKey, nsAutoPtr<T>& aOut)
{
  aOut = nullptr;
  nsAutoPtr<T> ptr;

  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return;
  }

  // Transfer ownership from ent->mData into aOut.
  aOut = mozilla::Move(ent->mData);

  this->Remove(aKey);
}

#endif // nsClassHashtable_h__
