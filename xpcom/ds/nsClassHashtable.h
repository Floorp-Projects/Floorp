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

  struct EntryPtr {
  private:
    typename base_type::EntryType& mEntry;
    // For debugging purposes
#ifdef DEBUG
    base_type& mTable;
    uint32_t mTableGeneration;
#endif

  public:
    EntryPtr(base_type& aTable, typename base_type::EntryType* aEntry)
      : mEntry(*aEntry)
#ifdef DEBUG
      , mTable(aTable)
      , mTableGeneration(aTable.GetGeneration())
#endif
    {
    }
    ~EntryPtr()
    {
      MOZ_ASSERT(mEntry.mData, "Entry should have been added by now");
    }

    explicit operator bool() const
    {
      // Is there something stored in the table already?
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      return !!mEntry.mData;
    }

    T& operator*()
    {
      MOZ_ASSERT(mEntry.mData);
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      return *mEntry.mData;
    }

    void TakeOwnership(T* aPtr)
    {
      MOZ_ASSERT(!mEntry.mData);
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      mEntry.mData = aPtr;
    }
  };

  /**
   * Looks up aKey in the hash table and returns an object that allows you to
   * insert a new entry into the hashtable for that key if an existing entry
   * isn't found for it.
   *
   * A typical usage of this API looks like this:
   *
   *   auto p = table.LookupForAdd(key);
   *   if (p) {
   *     // The entry already existed in the table.
   *     Use(*p);
   *   } else {
   *     // An existing entry wasn't found, store a new entry in the hashtable.
   *     table.Insert(p, newValue);
   *   }
   *
   * We ensure that the hashtable isn't modified before Insert() is called.
   * This is useful for cases where you want to insert a new entry into the
   * hashtable if one doesn't exist before but would like to avoid two hashtable
   * lookups.
   */
  MOZ_MUST_USE EntryPtr LookupForAdd(KeyType aKey);

  void Insert(EntryPtr& aEntryPtr, T* aPtr);
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
  typename base_type::EntryType* ent = this->PutEntry(aKey);
  if (!ent->mData) {
    ent->mData = new T(mozilla::Forward<Args>(aConstructionArgs)...);
  }
  return ent->mData;
}

template<class KeyClass, class T>
typename nsClassHashtable<KeyClass, T>::EntryPtr
nsClassHashtable<KeyClass, T>::LookupForAdd(KeyType aKey)
{
  typename base_type::EntryType* ent = this->PutEntry(aKey);
  return EntryPtr(*this, ent);
}

template<class KeyClass, class T>
void
nsClassHashtable<KeyClass, T>::Insert(typename nsClassHashtable<KeyClass, T>::EntryPtr& aEntryPtr,
                                      T* aPtr)
{
  aEntryPtr.TakeOwnership(aPtr);
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

template<class KeyClass, class T>
void
nsClassHashtable<KeyClass, T>::RemoveAndForget(KeyType aKey, nsAutoPtr<T>& aOut)
{
  aOut = nullptr;

  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return;
  }

  // Transfer ownership from ent->mData into aOut.
  aOut = mozilla::Move(ent->mData);

  this->Remove(aKey);
}

#endif // nsClassHashtable_h__
