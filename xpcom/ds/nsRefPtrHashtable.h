/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRefPtrHashtable_h__
#define nsRefPtrHashtable_h__

#include "nsBaseHashtable.h"
#include "nsHashKeys.h"

/**
 * templated hashtable class maps keys to reference pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param PtrType the reference-type being wrapped
 * @see nsDataHashtable, nsClassHashtable
 */
template <class KeyClass, class PtrType>
class nsRefPtrHashtable
    : public nsBaseHashtable<KeyClass, RefPtr<PtrType>, PtrType*> {
 public:
  typedef typename KeyClass::KeyType KeyType;
  typedef PtrType* UserDataType;
  typedef nsBaseHashtable<KeyClass, RefPtr<PtrType>, PtrType*> base_type;

  nsRefPtrHashtable() = default;
  explicit nsRefPtrHashtable(uint32_t aInitLength)
      : nsBaseHashtable<KeyClass, RefPtr<PtrType>, PtrType*>(aInitLength) {}

  /**
   * @copydoc nsBaseHashtable::Get
   * @param aData This is an XPCOM getter, so aData is already_addrefed.
   *   If the key doesn't exist, aData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* aData) const;

  /**
   * @copydoc nsBaseHashtable::Get
   */
  already_AddRefed<PtrType> Get(KeyType aKey) const;

  /**
   * Gets a weak reference to the hashtable entry.
   * @param aFound If not nullptr, will be set to true if the entry is found,
   *               to false otherwise.
   * @return The entry, or nullptr if not found. Do not release this pointer!
   */
  PtrType* GetWeak(KeyType aKey, bool* aFound = nullptr) const;

  // Hide base class' Put overloads intentionally, to make any necessary
  // refcounting explicit when calling Put.

  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<PtrType, U>>>
  void Put(KeyType aKey, RefPtr<U>&& aData);

  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<PtrType, U>>>
  [[nodiscard]] bool Put(KeyType aKey, RefPtr<U>&& aData,
                         const mozilla::fallible_t&);

  /**
   * Remove the entry associated with aKey (if any), optionally _moving_ its
   * current value into *aData, thereby avoiding calls to AddRef and Release.
   * Return true if found.
   * @param aKey the key to remove from the hashtable
   * @param aData where to move the value (if non-null).  If an entry is not
   *              found it will be set to nullptr.
   * @return true if an entry for aKey was found (and removed)
   */
  inline bool Remove(KeyType aKey, UserDataType* aData = nullptr);
};

template <typename K, typename T>
inline void ImplCycleCollectionUnlink(nsRefPtrHashtable<K, T>& aField) {
  aField.Clear();
}

template <typename K, typename T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsRefPtrHashtable<K, T>& aField, const char* aName, uint32_t aFlags = 0) {
  for (auto iter = aField.ConstIter(); !iter.Done(); iter.Next()) {
    CycleCollectionNoteChild(aCallback, iter.UserData(), aName, aFlags);
  }
}

//
// nsRefPtrHashtable definitions
//

template <class KeyClass, class PtrType>
bool nsRefPtrHashtable<KeyClass, PtrType>::Get(KeyType aKey,
                                               UserDataType* aRefPtr) const {
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aRefPtr) {
      *aRefPtr = ent->GetData();

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

template <class KeyClass, class PtrType>
already_AddRefed<PtrType> nsRefPtrHashtable<KeyClass, PtrType>::Get(
    KeyType aKey) const {
  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return nullptr;
  }

  RefPtr<PtrType> copy = ent->GetData();
  return copy.forget();
}

template <class KeyClass, class PtrType>
PtrType* nsRefPtrHashtable<KeyClass, PtrType>::GetWeak(KeyType aKey,
                                                       bool* aFound) const {
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aFound) {
      *aFound = true;
    }

    return ent->GetData();
  }

  // Key does not exist, return nullptr and set aFound to false
  if (aFound) {
    *aFound = false;
  }

  return nullptr;
}

template <class KeyClass, class PtrType>
template <typename U, typename>
void nsRefPtrHashtable<KeyClass, PtrType>::Put(KeyType aKey,
                                               RefPtr<U>&& aData) {
  if (!Put(aKey, std::move(aData), mozilla::fallible)) {
    NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
  }
}

template <class KeyClass, class PtrType>
template <typename U, typename>
bool nsRefPtrHashtable<KeyClass, PtrType>::Put(KeyType aKey, RefPtr<U>&& aData,
                                               const mozilla::fallible_t&) {
  typename base_type::EntryType* ent = this->PutEntry(aKey, mozilla::fallible);

  if (!ent) {
    return false;
  }

  ent->SetData(std::move(aData));

  return true;
}

template <class KeyClass, class PtrType>
bool nsRefPtrHashtable<KeyClass, PtrType>::Remove(KeyType aKey,
                                                  UserDataType* aRefPtr) {
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aRefPtr) {
      ent->GetModifiableData()->forget(aRefPtr);
    }
    this->RemoveEntry(ent);
    return true;
  }

  if (aRefPtr) {
    *aRefPtr = nullptr;
  }
  return false;
}

#endif  // nsRefPtrHashtable_h__
