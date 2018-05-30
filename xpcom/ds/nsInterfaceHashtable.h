/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInterfaceHashtable_h__
#define nsInterfaceHashtable_h__

#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsCOMPtr.h"

/**
 * templated hashtable class maps keys to interface pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param Interface the interface-type being wrapped
 * @see nsDataHashtable, nsClassHashtable
 */
template<class KeyClass, class Interface>
class nsInterfaceHashtable
  : public nsBaseHashtable<KeyClass, nsCOMPtr<Interface>, Interface*>
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef Interface* UserDataType;
  typedef nsBaseHashtable<KeyClass, nsCOMPtr<Interface>, Interface*> base_type;

  nsInterfaceHashtable() {}
  explicit nsInterfaceHashtable(uint32_t aInitLength)
    : nsBaseHashtable<KeyClass, nsCOMPtr<Interface>, Interface*>(aInitLength)
  {
  }

  /**
   * @copydoc nsBaseHashtable::Get
   * @param aData This is an XPCOM getter, so aData is already_addrefed.
   *   If the key doesn't exist, aData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* aData) const;

  /**
   * @copydoc nsBaseHashtable::Get
   */
  already_AddRefed<Interface> Get(KeyType aKey) const;

  /**
   * Gets a weak reference to the hashtable entry.
   * @param aFound If not nullptr, will be set to true if the entry is found,
   *               to false otherwise.
   * @return The entry, or nullptr if not found. Do not release this pointer!
   */
  Interface* GetWeak(KeyType aKey, bool* aFound = nullptr) const;

  /**
   * Allows inserting a value into the hashtable, moving its owning reference
   * count into the hashtable, avoiding an AddRef.
   */
  void Put(KeyType aKey, already_AddRefed<Interface>&& aData)
  {
    if (!Put(aKey, std::move(aData), mozilla::fallible)) {
      NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
    }
  }

  MOZ_MUST_USE bool Put(KeyType aKey, already_AddRefed<Interface>&& aData,
                        const mozilla::fallible_t&);
  using base_type::Put;

  /**
   * Remove the entry associated with aKey (if any), optionally _moving_ its
   * current value into *aData, thereby avoiding calls to AddRef and Release.
   * Return true if found.
   * @param aKey the key to remove from the hashtable
   * @param aData where to move the value (if non-null).  If an entry is not
   *              found it will be set to nullptr.
   * @return true if an entry for aKey was found (and removed)
   */
  inline bool Remove(KeyType aKey, Interface** aData = nullptr);
};

template<typename K, typename T>
inline void
ImplCycleCollectionUnlink(nsInterfaceHashtable<K, T>& aField)
{
  aField.Clear();
}

template<typename K, typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            const nsInterfaceHashtable<K, T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  for (auto iter = aField.ConstIter(); !iter.Done(); iter.Next()) {
    CycleCollectionNoteChild(aCallback, iter.UserData(), aName, aFlags);
  }
}

//
// nsInterfaceHashtable definitions
//

template<class KeyClass, class Interface>
bool
nsInterfaceHashtable<KeyClass, Interface>::Get(KeyType aKey,
                                               UserDataType* aInterface) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aInterface) {
      *aInterface = ent->mData;

      NS_IF_ADDREF(*aInterface);
    }

    return true;
  }

  // if the key doesn't exist, set *aInterface to null
  // so that it is a valid XPCOM getter
  if (aInterface) {
    *aInterface = nullptr;
  }

  return false;
}

template<class KeyClass, class Interface>
already_AddRefed<Interface>
nsInterfaceHashtable<KeyClass, Interface>::Get(KeyType aKey) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return nullptr;
  }

  nsCOMPtr<Interface> copy = ent->mData;
  return copy.forget();
}

template<class KeyClass, class Interface>
Interface*
nsInterfaceHashtable<KeyClass, Interface>::GetWeak(KeyType aKey,
                                                   bool* aFound) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aFound) {
      *aFound = true;
    }

    return ent->mData;
  }

  // Key does not exist, return nullptr and set aFound to false
  if (aFound) {
    *aFound = false;
  }
  return nullptr;
}

template<class KeyClass, class Interface>
bool
nsInterfaceHashtable<KeyClass, Interface>::Put(KeyType aKey,
                                               already_AddRefed<Interface>&& aValue,
                                               const mozilla::fallible_t&)
{
  typename base_type::EntryType* ent = this->PutEntry(aKey);
  if (!ent) {
    return false;
  }

  ent->mData = aValue;
  return true;
}

template<class KeyClass, class Interface>
bool
nsInterfaceHashtable<KeyClass, Interface>::Remove(KeyType aKey,
                                                  Interface** aData)
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aData) {
      ent->mData.forget(aData);
    }
    this->RemoveEntry(ent);
    return true;
  }

  if (aData) {
    *aData = nullptr;
  }
  return false;
}

#endif // nsInterfaceHashtable_h__
