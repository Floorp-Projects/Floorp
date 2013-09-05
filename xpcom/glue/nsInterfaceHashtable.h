/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
template<class KeyClass,class Interface>
class nsInterfaceHashtable :
  public nsBaseHashtable< KeyClass, nsCOMPtr<Interface> , Interface* >
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef Interface* UserDataType;
  typedef nsBaseHashtable< KeyClass, nsCOMPtr<Interface> , Interface* >
          base_type;

  nsInterfaceHashtable()
  {
  }
  explicit nsInterfaceHashtable(uint32_t aInitSize)
    : nsBaseHashtable<KeyClass,nsCOMPtr<Interface>,Interface*>(aInitSize)
  {
  }

  /**
   * @copydoc nsBaseHashtable::Get
   * @param pData This is an XPCOM getter, so pData is already_addrefed.
   *   If the key doesn't exist, pData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* pData) const;

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
};

template <typename K, typename T>
inline void
ImplCycleCollectionUnlink(nsInterfaceHashtable<K, T>& aField)
{
  aField.Clear();
}

template <typename K, typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            const nsInterfaceHashtable<K, T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  nsBaseHashtableCCTraversalData userData(aCallback, aName, aFlags);

  aField.EnumerateRead(ImplCycleCollectionTraverse_EnumFunc<typename K::KeyType,T*>,
                       &userData);
}

//
// nsInterfaceHashtable definitions
//

template<class KeyClass,class Interface>
bool
nsInterfaceHashtable<KeyClass,Interface>::Get
  (KeyType aKey, UserDataType* pInterface) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent)
  {
    if (pInterface)
    {
      *pInterface = ent->mData;

      NS_IF_ADDREF(*pInterface);
    }

    return true;
  }

  // if the key doesn't exist, set *pInterface to null
  // so that it is a valid XPCOM getter
  if (pInterface)
    *pInterface = nullptr;

  return false;
}

template<class KeyClass, class Interface>
already_AddRefed<Interface>
nsInterfaceHashtable<KeyClass,Interface>::Get(KeyType aKey) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent)
    return nullptr;

  nsCOMPtr<Interface> copy = ent->mData;
  return copy.forget();
}

template<class KeyClass,class Interface>
Interface*
nsInterfaceHashtable<KeyClass,Interface>::GetWeak
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

#endif // nsInterfaceHashtable_h__
