/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_DS_NSREFCOUNTEDHASHTABLE_H_
#define XPCOM_DS_NSREFCOUNTEDHASHTABLE_H_

#include "nsBaseHashtable.h"
#include "nsHashKeys.h"

/**
 * templated hashtable class maps keys to reference pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param PtrType the reference-type being wrapped
 * @see nsClassHashtable, nsTHashMap
 */
template <class KeyClass, class PtrType>
class nsRefCountedHashtable
    : public nsBaseHashtable<
          KeyClass, PtrType,
          typename mozilla::detail::SmartPtrTraits<PtrType>::RawPointerType> {
 public:
  using KeyType = typename KeyClass::KeyType;
  using SmartPtrTraits = mozilla::detail::SmartPtrTraits<PtrType>;
  using PointeeType = typename SmartPtrTraits::PointeeType;
  using RawPointerType = typename SmartPtrTraits::RawPointerType;
  using base_type = nsBaseHashtable<KeyClass, PtrType, RawPointerType>;

  using base_type::base_type;

  static_assert(SmartPtrTraits::IsRefCounted);

  /**
   * @copydoc nsBaseHashtable::Get
   * @param aData This is an XPCOM getter, so aData is already_addrefed.
   *   If the key doesn't exist, *aData will be set to nullptr.
   */
  bool Get(KeyType aKey, RawPointerType* aData) const;

  /**
   * @copydoc nsBaseHashtable::Get
   */
  [[nodiscard]] already_AddRefed<PointeeType> Get(KeyType aKey) const;

  /**
   * Gets a weak reference to the hashtable entry.
   * @param aFound If not nullptr, will be set to true if the entry is found,
   *               to false otherwise.
   * @return The entry, or nullptr if not found. Do not release this pointer!
   */
  [[nodiscard]] RawPointerType GetWeak(KeyType aKey,
                                       bool* aFound = nullptr) const;

  using base_type::InsertOrUpdate;

  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<PointeeType, U>>>
  void InsertOrUpdate(
      KeyType aKey,
      typename SmartPtrTraits::template OtherSmartPtrType<U>&& aData);

  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<PointeeType, U>>>
  [[nodiscard]] bool InsertOrUpdate(
      KeyType aKey,
      typename SmartPtrTraits::template OtherSmartPtrType<U>&& aData,
      const mozilla::fallible_t&);

  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<PointeeType, U>>>
  void InsertOrUpdate(KeyType aKey, already_AddRefed<U>&& aData);

  template <typename U,
            typename = std::enable_if_t<std::is_base_of_v<PointeeType, U>>>
  [[nodiscard]] bool InsertOrUpdate(KeyType aKey, already_AddRefed<U>&& aData,
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
  inline bool Remove(KeyType aKey, RawPointerType* aData = nullptr);
};

template <typename K, typename T>
inline void ImplCycleCollectionUnlink(nsRefCountedHashtable<K, T>& aField) {
  aField.Clear();
}

template <typename K, typename T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsRefCountedHashtable<K, T>& aField, const char* aName,
    uint32_t aFlags = 0) {
  for (auto iter = aField.ConstIter(); !iter.Done(); iter.Next()) {
    CycleCollectionNoteChild(aCallback, iter.UserData(), aName, aFlags);
  }
}

//
// nsRefCountedHashtable definitions
//

template <class KeyClass, class PtrType>
bool nsRefCountedHashtable<KeyClass, PtrType>::Get(
    KeyType aKey, RawPointerType* aRefPtr) const {
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
already_AddRefed<typename nsRefCountedHashtable<KeyClass, PtrType>::PointeeType>
nsRefCountedHashtable<KeyClass, PtrType>::Get(KeyType aKey) const {
  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return nullptr;
  }

  PtrType copy = ent->GetData();
  return copy.forget();
}

template <class KeyClass, class PtrType>
typename nsRefCountedHashtable<KeyClass, PtrType>::RawPointerType
nsRefCountedHashtable<KeyClass, PtrType>::GetWeak(KeyType aKey,
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
void nsRefCountedHashtable<KeyClass, PtrType>::InsertOrUpdate(
    KeyType aKey,
    typename SmartPtrTraits::template OtherSmartPtrType<U>&& aData) {
  if (!InsertOrUpdate(aKey, std::move(aData), mozilla::fallible)) {
    NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
  }
}

template <class KeyClass, class PtrType>
template <typename U, typename>
bool nsRefCountedHashtable<KeyClass, PtrType>::InsertOrUpdate(
    KeyType aKey,
    typename SmartPtrTraits::template OtherSmartPtrType<U>&& aData,
    const mozilla::fallible_t&) {
  typename base_type::EntryType* ent = this->PutEntry(aKey, mozilla::fallible);

  if (!ent) {
    return false;
  }

  ent->SetData(std::move(aData));

  return true;
}

template <class KeyClass, class PtrType>
template <typename U, typename>
void nsRefCountedHashtable<KeyClass, PtrType>::InsertOrUpdate(
    KeyType aKey, already_AddRefed<U>&& aData) {
  if (!InsertOrUpdate(aKey, std::move(aData), mozilla::fallible)) {
    NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
  }
}

template <class KeyClass, class PtrType>
template <typename U, typename>
bool nsRefCountedHashtable<KeyClass, PtrType>::InsertOrUpdate(
    KeyType aKey, already_AddRefed<U>&& aData, const mozilla::fallible_t&) {
  typename base_type::EntryType* ent = this->PutEntry(aKey, mozilla::fallible);

  if (!ent) {
    return false;
  }

  ent->SetData(std::move(aData));

  return true;
}

template <class KeyClass, class PtrType>
bool nsRefCountedHashtable<KeyClass, PtrType>::Remove(KeyType aKey,
                                                      RawPointerType* aRefPtr) {
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

#endif  // XPCOM_DS_NSREFCOUNTEDHASHTABLE_H_
