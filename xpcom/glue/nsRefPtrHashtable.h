/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRefPtrHashtable_h__
#define nsRefPtrHashtable_h__

#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"

/**
 * templated hashtable class maps keys to reference pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param RefPtr the reference-type being wrapped
 * @see nsDataHashtable, nsClassHashtable
 */
template<class KeyClass, class RefPtr>
class nsRefPtrHashtable
  : public nsBaseHashtable<KeyClass, nsRefPtr<RefPtr>, RefPtr*>
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef RefPtr* UserDataType;
  typedef nsBaseHashtable<KeyClass, nsRefPtr<RefPtr>, RefPtr*> base_type;

  nsRefPtrHashtable() {}
  explicit nsRefPtrHashtable(uint32_t aInitLength)
    : nsBaseHashtable<KeyClass, nsRefPtr<RefPtr>, RefPtr*>(aInitLength)
  {
  }

  /**
   * @copydoc nsBaseHashtable::Get
   * @param aData This is an XPCOM getter, so aData is already_addrefed.
   *   If the key doesn't exist, aData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* aData) const;

  /**
   * Gets a weak reference to the hashtable entry.
   * @param aFound If not nullptr, will be set to true if the entry is found,
   *               to false otherwise.
   * @return The entry, or nullptr if not found. Do not release this pointer!
   */
  RefPtr* GetWeak(KeyType aKey, bool* aFound = nullptr) const;

  // Overload Put, rather than overriding it.
  using base_type::Put;

  void Put(KeyType aKey, already_AddRefed<RefPtr> aData);

  MOZ_WARN_UNUSED_RESULT bool Put(KeyType aKey, already_AddRefed<RefPtr> aData,
                                  const mozilla::fallible_t&);

  // Overload Remove, rather than overriding it.
  using base_type::Remove;

  /**
   * Remove the data for the associated key, swapping the current value into
   * pData, thereby avoiding calls to AddRef and Release.
   * @param aKey the key to remove from the hashtable
   * @param aData This is an XPCOM getter, so aData is already_addrefed.
   *   If the key doesn't exist, aData will be set to nullptr. Must be non-null.
   */
  bool Remove(KeyType aKey, UserDataType* aData);
};

template<typename K, typename T>
inline void
ImplCycleCollectionUnlink(nsRefPtrHashtable<K, T>& aField)
{
  aField.Clear();
}

template<typename K, typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsRefPtrHashtable<K, T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  nsBaseHashtableCCTraversalData userData(aCallback, aName, aFlags);

  aField.EnumerateRead(ImplCycleCollectionTraverse_EnumFunc<typename K::KeyType, T*>,
                       &userData);
}

//
// nsRefPtrHashtable definitions
//

template<class KeyClass, class RefPtr>
bool
nsRefPtrHashtable<KeyClass, RefPtr>::Get(KeyType aKey,
                                         UserDataType* aRefPtr) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aRefPtr) {
      *aRefPtr = ent->mData;

      NS_IF_ADDREF(*aRefPtr);
    }

    return true;
  }

  // if the key doesn't exist, set *aRefPtr to null
  // so that it is a valid XPCOM getter
  if (aRefPtr) {
    *aRefPtr = nullptr;
  }

  return false;
}

template<class KeyClass, class RefPtr>
RefPtr*
nsRefPtrHashtable<KeyClass, RefPtr>::GetWeak(KeyType aKey, bool* aFound) const
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

template<class KeyClass, class RefPtr>
void
nsRefPtrHashtable<KeyClass, RefPtr>::Put(KeyType aKey,
                                         already_AddRefed<RefPtr> aData)
{
  if (!Put(aKey, mozilla::Move(aData), mozilla::fallible_t())) {
    NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
  }
}

template<class KeyClass, class RefPtr>
bool
nsRefPtrHashtable<KeyClass, RefPtr>::Put(KeyType aKey,
                                         already_AddRefed<RefPtr> aData,
                                         const mozilla::fallible_t&)
{
  typename base_type::EntryType* ent = this->PutEntry(aKey);

  if (!ent) {
    return false;
  }

  ent->mData = aData;

  return true;
}

template<class KeyClass, class RefPtr>
bool
nsRefPtrHashtable<KeyClass, RefPtr>::Remove(KeyType aKey,
                                            UserDataType* aRefPtr)
{
  MOZ_ASSERT(aRefPtr);
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    ent->mData.forget(aRefPtr);
    this->Remove(aKey);
    return true;
  }

  // If the key doesn't exist, set *aRefPtr to null
  // so that it is a valid XPCOM getter.
  *aRefPtr = nullptr;
  return false;
}

#endif // nsRefPtrHashtable_h__
