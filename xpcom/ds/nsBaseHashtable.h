/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseHashtable_h__
#define nsBaseHashtable_h__

#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "nsTHashtable.h"
#include "nsDebug.h"

template<class KeyClass, class DataType, class UserDataType>
class nsBaseHashtable; // forward declaration

/**
 * the private nsTHashtable::EntryType class used by nsBaseHashtable
 * @see nsTHashtable for the specification of this class
 * @see nsBaseHashtable for template parameters
 */
template<class KeyClass, class DataType>
class nsBaseHashtableET : public KeyClass
{
public:
  DataType mData;
  friend class nsTHashtable<nsBaseHashtableET<KeyClass, DataType>>;

private:
  typedef typename KeyClass::KeyType KeyType;
  typedef typename KeyClass::KeyTypePointer KeyTypePointer;

  explicit nsBaseHashtableET(KeyTypePointer aKey);
  nsBaseHashtableET(nsBaseHashtableET<KeyClass, DataType>&& aToMove);
  ~nsBaseHashtableET();
};

/**
 * templated hashtable for simple data types
 * This class manages simple data types that do not need construction or
 * destruction.
 *
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the datatype stored in the hashtable,
 *   for example, uint32_t or nsCOMPtr.  If UserDataType is not the same,
 *   DataType must implicitly cast to UserDataType
 * @param UserDataType the user sees, for example uint32_t or nsISupports*
 */
template<class KeyClass, class DataType, class UserDataType>
class nsBaseHashtable
  : protected nsTHashtable<nsBaseHashtableET<KeyClass, DataType>>
{
  typedef mozilla::fallible_t fallible_t;

public:
  typedef typename KeyClass::KeyType KeyType;
  typedef nsBaseHashtableET<KeyClass, DataType> EntryType;

  using nsTHashtable<EntryType>::Contains;
  using nsTHashtable<EntryType>::GetGeneration;

  nsBaseHashtable() {}
  explicit nsBaseHashtable(uint32_t aInitLength)
    : nsTHashtable<EntryType>(aInitLength)
  {
  }

  /**
   * Return the number of entries in the table.
   * @return    number of entries
   */
  uint32_t Count() const { return nsTHashtable<EntryType>::Count(); }

  /**
   * retrieve the value for a key.
   * @param aKey the key to retreive
   * @param aData data associated with this key will be placed at this
   *   pointer.  If you only need to check if the key exists, aData
   *   may be null.
   * @return true if the key exists. If key does not exist, aData is not
   *   modified.
   */
  bool Get(KeyType aKey, UserDataType* aData) const
  {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return false;
    }

    if (aData) {
      *aData = ent->mData;
    }

    return true;
  }

  /**
   * Get the value, returning a zero-initialized POD or a default-initialized
   * object if the entry is not present in the table.
   *
   * @param aKey the key to retrieve
   * @return The found value, or UserDataType{} if no entry was found with the
   *         given key.
   * @note If zero/default-initialized values are stored in the table, it is
   *       not possible to distinguish between such a value and a missing entry.
   */
  UserDataType Get(KeyType aKey) const
  {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return UserDataType{};
    }

    return ent->mData;
  }

  /**
   * Add key to the table if not already present, and return a reference to its
   * value.  If key is not already in the table then the value is default
   * constructed.
   */
  DataType& GetOrInsert(const KeyType& aKey)
  {
    EntryType* ent = this->PutEntry(aKey);
    return ent->mData;
  }

  /**
   * put a new value for the associated key
   * @param aKey the key to put
   * @param aData the new data
   * @return always true, unless memory allocation failed
   */
  void Put(KeyType aKey, const UserDataType& aData)
  {
    if (!Put(aKey, aData, mozilla::fallible)) {
      NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
    }
  }

  MOZ_MUST_USE bool Put(KeyType aKey, const UserDataType& aData,
                        const fallible_t&)
  {
    EntryType* ent = this->PutEntry(aKey, mozilla::fallible);
    if (!ent) {
      return false;
    }

    ent->mData = aData;

    return true;
  }

  /**
   * remove the data for the associated key
   * @param aKey the key to remove from the hashtable
   */
  void Remove(KeyType aKey) { this->RemoveEntry(aKey); }

  /**
   * Looks up aKey in the hashtable and if found calls the given callback
   * aFunction with the value.  If the callback returns true then the entry
   * is removed.  If aKey doesn't exist nothing happens.
   * The hashtable must not be modified in the callback function.
   *
   * A typical usage of this API looks like this:
   *
   *   table.LookupRemoveIf(key, [](T* aValue) {
   *     // ... do stuff using aValue ...
   *     return aValue->IsEmpty(); // or some other condition to remove it
   *   });
   *
   * This is useful for cases where you want to lookup and possibly modify
   * the value and then maybe remove the entry but would like to avoid two
   * hashtable lookups.
   */
  template<class F>
  void LookupRemoveIf(KeyType aKey, F aFunction)
  {
#ifdef DEBUG
    auto tableGeneration = GetGeneration();
#endif
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return;
    }
    bool shouldRemove = aFunction(ent->mData);
    MOZ_ASSERT(tableGeneration == GetGeneration(),
               "hashtable was modified by the LookupRemoveIf callback!");
    if (shouldRemove) {
      this->RemoveEntry(ent);
    }
  }

  // This is an iterator that also allows entry removal. Example usage:
  //
  //   for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
  //     const KeyType key = iter.Key();
  //     const UserDataType data = iter.UserData();
  //     // or
  //     const DataType& data = iter.Data();
  //     // ... do stuff with |key| and/or |data| ...
  //     // ... possibly call iter.Remove() once ...
  //   }
  //
  class Iterator : public PLDHashTable::Iterator
  {
  public:
    typedef PLDHashTable::Iterator Base;

    explicit Iterator(nsBaseHashtable* aTable) : Base(&aTable->mTable) {}
    Iterator(Iterator&& aOther) : Base(aOther.mTable) {}
    ~Iterator() {}

    KeyType Key() const { return static_cast<EntryType*>(Get())->GetKey(); }
    UserDataType UserData() const
    {
      return static_cast<EntryType*>(Get())->mData;
    }
    DataType& Data() const { return static_cast<EntryType*>(Get())->mData; }

  private:
    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;
  };

  Iterator Iter() { return Iterator(this); }

  Iterator ConstIter() const
  {
    return Iterator(const_cast<nsBaseHashtable*>(this));
  }

  /**
   * reset the hashtable, removing all entries
   */
  void Clear() { nsTHashtable<EntryType>::Clear(); }

  /**
   * Measure the size of the table's entry storage. The size of things pointed
   * to by entries must be measured separately; hence the "Shallow" prefix.
   *
   * @param   aMallocSizeOf the function used to measure heap-allocated blocks
   * @return  the summed size of the table's storage
   */
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return this->mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Like ShallowSizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Swap the elements in this hashtable with the elements in aOther.
   */
  void SwapElements(nsBaseHashtable& aOther)
  {
    nsTHashtable<EntryType>::SwapElements(aOther);
  }


#ifdef DEBUG
  using nsTHashtable<EntryType>::MarkImmutable;
#endif
};

//
// nsBaseHashtableET definitions
//

template<class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::nsBaseHashtableET(KeyTypePointer aKey)
  : KeyClass(aKey)
  , mData()
{
}

template<class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::nsBaseHashtableET(
      nsBaseHashtableET<KeyClass, DataType>&& aToMove)
  : KeyClass(mozilla::Move(aToMove))
  , mData(mozilla::Move(aToMove.mData))
{
}

template<class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::~nsBaseHashtableET()
{
}

#endif // nsBaseHashtable_h__
