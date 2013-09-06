/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class nsRefPtrHashtable :
  public nsBaseHashtable< KeyClass, nsRefPtr<RefPtr> , RefPtr* >
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef RefPtr* UserDataType;
  typedef nsBaseHashtable< KeyClass, nsRefPtr<RefPtr> , RefPtr* > base_type;

  nsRefPtrHashtable()
  {
  }
  explicit nsRefPtrHashtable(uint32_t aInitSize)
    : nsBaseHashtable<KeyClass,nsRefPtr<RefPtr>,RefPtr*>(aInitSize)
  {
  }

  /**
   * @copydoc nsBaseHashtable::Get
   * @param pData This is an XPCOM getter, so pData is already_addrefed.
   *   If the key doesn't exist, pData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* pData) const;

  /**
   * Gets a weak reference to the hashtable entry.
   * @param aFound If not nullptr, will be set to true if the entry is found,
   *               to false otherwise.
   * @return The entry, or nullptr if not found. Do not release this pointer!
   */
  RefPtr* GetWeak(KeyType aKey, bool* aFound = nullptr) const;
};

template <typename K, typename T>
inline void
ImplCycleCollectionUnlink(nsRefPtrHashtable<K, T>& aField)
{
  aField.Clear();
}

template <typename K, typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsRefPtrHashtable<K, T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  nsBaseHashtableCCTraversalData userData(aCallback, aName, aFlags);

  aField.EnumerateRead(ImplCycleCollectionTraverse_EnumFunc<typename K::KeyType,T*>,
                       &userData);
}

//
// nsRefPtrHashtable definitions
//

template<class KeyClass, class RefPtr>
bool
nsRefPtrHashtable<KeyClass,RefPtr>::Get
  (KeyType aKey, UserDataType* pRefPtr) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent)
  {
    if (pRefPtr)
    {
      *pRefPtr = ent->mData;

      NS_IF_ADDREF(*pRefPtr);
    }

    return true;
  }

  // if the key doesn't exist, set *pRefPtr to null
  // so that it is a valid XPCOM getter
  if (pRefPtr)
    *pRefPtr = nullptr;

  return false;
}

template<class KeyClass, class RefPtr>
RefPtr*
nsRefPtrHashtable<KeyClass,RefPtr>::GetWeak
  (KeyType aKey, bool* aFound) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent)
  {
    if (aFound)
      *aFound = true;

    return ent->mData;
  }

  // Key does not exist, return nullptr and set aFound to false
  if (aFound)
    *aFound = false;
  return nullptr;
}

#endif // nsRefPtrHashtable_h__
