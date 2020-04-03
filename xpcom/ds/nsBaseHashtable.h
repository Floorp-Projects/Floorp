/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseHashtable_h__
#define nsBaseHashtable_h__

#include <utility>

#include "mozilla/MemoryReporting.h"
#include "nsDebug.h"
#include "nsTHashtable.h"

template <class KeyClass, class DataType, class UserDataType, class Converter>
class nsBaseHashtable;  // forward declaration

/**
 * Data type conversion helper that is used to wrap and unwrap the specified
 * DataType.
 */
template <class DataType, class UserDataType>
class nsDefaultConverter {
 public:
  /**
   * Maps the storage DataType to the exposed UserDataType.
   */
  static UserDataType Unwrap(DataType& src) { return UserDataType(src); }

  /**
   * Const ref variant used for example with nsCOMPtr wrappers.
   */
  static DataType Wrap(const UserDataType& src) { return DataType(src); }

  /**
   * Generic conversion, this is useful for things like already_AddRefed.
   */
  template <typename U>
  static DataType Wrap(U&& src) {
    return std::move(src);
  }
};

/**
 * the private nsTHashtable::EntryType class used by nsBaseHashtable
 * @see nsTHashtable for the specification of this class
 * @see nsBaseHashtable for template parameters
 */
template <class KeyClass, class DataType>
class nsBaseHashtableET : public KeyClass {
 public:
  const DataType& GetData() const { return mData; }
  DataType* GetModifiableData() { return &mData; }
  template <typename U>
  void SetData(U&& aData) {
    mData = std::forward<U>(aData);
  }

 private:
  DataType mData;
  friend class nsTHashtable<nsBaseHashtableET<KeyClass, DataType>>;
  template <typename KeyClassX, typename DataTypeX, typename UserDataTypeX,
            typename ConverterX>
  friend class nsBaseHashtable;

  typedef typename KeyClass::KeyType KeyType;
  typedef typename KeyClass::KeyTypePointer KeyTypePointer;

  explicit nsBaseHashtableET(KeyTypePointer aKey);
  nsBaseHashtableET(nsBaseHashtableET<KeyClass, DataType>&& aToMove);
  ~nsBaseHashtableET() = default;
};

/**
 * templated hashtable for simple data types
 * This class manages simple data types that do not need construction or
 * destruction.
 *
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the datatype stored in the hashtable,
 *   for example, uint32_t or nsCOMPtr.
 * @param UserDataType the user sees, for example uint32_t or nsISupports*
 * @param Converter that can be used to map from DataType to UserDataType. A
 *   default converter is provided that assumes implicit conversion is an
 *   option.
 */
template <class KeyClass, class DataType, class UserDataType,
          class Converter = nsDefaultConverter<DataType, UserDataType>>
class nsBaseHashtable
    : protected nsTHashtable<nsBaseHashtableET<KeyClass, DataType>> {
  typedef mozilla::fallible_t fallible_t;

 public:
  typedef typename KeyClass::KeyType KeyType;
  typedef nsBaseHashtableET<KeyClass, DataType> EntryType;

  using nsTHashtable<EntryType>::Contains;
  using nsTHashtable<EntryType>::GetGeneration;
  using nsTHashtable<EntryType>::SizeOfExcludingThis;
  using nsTHashtable<EntryType>::SizeOfIncludingThis;

  nsBaseHashtable() = default;
  explicit nsBaseHashtable(uint32_t aInitLength)
      : nsTHashtable<EntryType>(aInitLength) {}

  /**
   * Return the number of entries in the table.
   * @return    number of entries
   */
  uint32_t Count() const { return nsTHashtable<EntryType>::Count(); }

  /**
   * Return whether the table is empty.
   * @return    whether empty
   */
  bool IsEmpty() const { return nsTHashtable<EntryType>::IsEmpty(); }

  /**
   * retrieve the value for a key.
   * @param aKey the key to retreive
   * @param aData data associated with this key will be placed at this
   *   pointer.  If you only need to check if the key exists, aData
   *   may be null.
   * @return true if the key exists. If key does not exist, aData is not
   *   modified.
   */
  bool Get(KeyType aKey, UserDataType* aData) const {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return false;
    }

    if (aData) {
      *aData = Converter::Unwrap(ent->mData);
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
  UserDataType Get(KeyType aKey) const {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return UserDataType{};
    }

    return Converter::Unwrap(ent->mData);
  }

  /**
   * Add key to the table if not already present, and return a reference to its
   * value.  If key is not already in the table then the value is default
   * constructed.
   */
  DataType& GetOrInsert(const KeyType& aKey) {
    EntryType* ent = this->PutEntry(aKey);
    return ent->mData;
  }

  /**
   * Put a new value for the associated key
   * @param aKey the key to put
   * @param aData the new data
   */
  void Put(KeyType aKey, const UserDataType& aData) {
    if (!Put(aKey, aData, mozilla::fallible)) {
      NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
    }
  }

  [[nodiscard]] bool Put(KeyType aKey, const UserDataType& aData,
                         const fallible_t&) {
    EntryType* ent = this->PutEntry(aKey, mozilla::fallible);
    if (!ent) {
      return false;
    }

    ent->mData = Converter::Wrap(aData);

    return true;
  }

  /**
   * Put a new value for the associated key
   * @param aKey the key to put
   * @param aData the new data
   */
  void Put(KeyType aKey, UserDataType&& aData) {
    if (!Put(aKey, std::move(aData), mozilla::fallible)) {
      NS_ABORT_OOM(this->mTable.EntrySize() * this->mTable.EntryCount());
    }
  }

  [[nodiscard]] bool Put(KeyType aKey, UserDataType&& aData,
                         const fallible_t&) {
    EntryType* ent = this->PutEntry(aKey, mozilla::fallible);
    if (!ent) {
      return false;
    }

    ent->mData = Converter::Wrap(std::move(aData));

    return true;
  }

  /**
   * Remove the entry associated with aKey (if any), optionally _moving_ its
   * current value into *aData.  Return true if found.
   * @param aKey the key to remove from the hashtable
   * @param aData where to move the value (if non-null).  If an entry is not
   *              found, *aData will be assigned a default-constructed value
   *              (i.e. reset to zero or nullptr for primitive types).
   * @return true if an entry for aKey was found (and removed)
   */
  bool Remove(KeyType aKey, DataType* aData = nullptr) {
    if (auto* ent = this->GetEntry(aKey)) {
      if (aData) {
        *aData = std::move(ent->mData);
      }
      this->RemoveEntry(ent);
      return true;
    }
    if (aData) {
      *aData = std::move(DataType());
    }
    return false;
  }

  struct LookupResult {
   private:
    EntryType* mEntry;
    nsBaseHashtable& mTable;
#ifdef DEBUG
    uint32_t mTableGeneration;
#endif

   public:
    LookupResult(EntryType* aEntry, nsBaseHashtable& aTable)
        : mEntry(aEntry),
          mTable(aTable)
#ifdef DEBUG
          ,
          mTableGeneration(aTable.GetGeneration())
#endif
    {
    }

    // Is there something stored in the table?
    explicit operator bool() const {
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      return mEntry;
    }

    void Remove() {
      if (!*this) {
        return;
      }
      mTable.RemoveEntry(mEntry);
      mEntry = nullptr;
    }

    [[nodiscard]] DataType& Data() {
      MOZ_ASSERT(!!*this, "must have an entry to access its value");
      return mEntry->mData;
    }
  };

  /**
   * Removes all entries matching a predicate.
   *
   * The predicate must be compatible with signature bool (const Iterator &).
   */
  template <typename Pred>
  void RemoveIf(Pred&& aPred) {
    for (auto iter = Iter(); !iter.Done(); iter.Next()) {
      if (aPred(const_cast<std::add_const_t<decltype(iter)>&>(iter))) {
        iter.Remove();
      }
    }
  }

  /**
   * Looks up aKey in the hashtable and returns an object that allows you to
   * read/modify the value of the entry, or remove the entry (if found).
   *
   * A typical usage of this API looks like this:
   *
   *   if (auto entry = hashtable.Lookup(key)) {
   *     DoSomething(entry.Data());
   *     if (entry.Data() > 42) {
   *       entry.Remove();
   *     }
   *   } // else - an entry with the given key doesn't exist
   *
   * This is useful for cases where you want to read/write the value of an entry
   * and (optionally) remove the entry without having to do multiple hashtable
   * lookups.  If you want to insert a new entry if one does not exist, then use
   * LookupForAdd instead, see below.
   */
  [[nodiscard]] LookupResult Lookup(KeyType aKey) {
    return LookupResult(this->GetEntry(aKey), *this);
  }

  struct EntryPtr {
   private:
    EntryType* mEntry;
    bool mExistingEntry;
    nsBaseHashtable& mTable;
    // For debugging purposes
#ifdef DEBUG
    uint32_t mTableGeneration;
    bool mDidInitNewEntry;
#endif

   public:
    EntryPtr(nsBaseHashtable& aTable, EntryType* aEntry, bool aExistingEntry)
        : mEntry(aEntry),
          mExistingEntry(aExistingEntry),
          mTable(aTable)
#ifdef DEBUG
          ,
          mTableGeneration(aTable.GetGeneration()),
          mDidInitNewEntry(false)
#endif
    {
    }
    ~EntryPtr() {
      MOZ_ASSERT(mExistingEntry || mDidInitNewEntry || !mEntry,
                 "Forgot to call OrInsert() or OrRemove() on a new entry");
    }

    // Is there something stored in the table already?
    explicit operator bool() const {
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      return mExistingEntry;
    }

    template <class F>
    DataType& OrInsert(F func) {
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      MOZ_ASSERT(mEntry);
      if (!mExistingEntry) {
        mEntry->mData = Converter::Wrap(func());
#ifdef DEBUG
        mDidInitNewEntry = true;
#endif
      }
      return mEntry->mData;
    }

    void OrRemove() {
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      MOZ_ASSERT(mEntry);
      mTable.RemoveEntry(mEntry);
      mEntry = nullptr;
    }

    [[nodiscard]] DataType& Data() {
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      MOZ_ASSERT(mEntry);
      return mEntry->mData;
    }
  };

  /**
   * Looks up aKey in the hashtable and returns an object that allows you to
   * insert a new entry into the hashtable for that key if an existing entry
   * isn't found for it.
   *
   * A typical usage of this API looks like this:
   *
   *   auto insertedValue = table.LookupForAdd(key).OrInsert([]() {
   *     return newValue;
   *   });
   *
   *   auto p = table.LookupForAdd(key);
   *   if (p) {
   *     // The entry already existed in the table.
   *     DoSomething(p.Data());
   *   } else {
   *     // An existing entry wasn't found, store a new entry in the hashtable.
   *     p.OrInsert([]() { return newValue; });
   *   }
   *
   * We ensure that the hashtable isn't modified before EntryPtr method calls.
   * This is useful for cases where you want to insert a new entry into the
   * hashtable if one doesn't exist before but would like to avoid two hashtable
   * lookups.
   */
  [[nodiscard]] EntryPtr LookupForAdd(KeyType aKey) {
    auto count = Count();
    EntryType* ent = this->PutEntry(aKey);
    return EntryPtr(*this, ent, count == Count());
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
  class Iterator : public PLDHashTable::Iterator {
   public:
    typedef PLDHashTable::Iterator Base;

    explicit Iterator(nsBaseHashtable* aTable) : Base(&aTable->mTable) {}
    Iterator(Iterator&& aOther) : Base(aOther.mTable) {}
    ~Iterator() = default;

    KeyType Key() const { return static_cast<EntryType*>(Get())->GetKey(); }
    UserDataType UserData() const {
      return Converter::Unwrap(static_cast<EntryType*>(Get())->mData);
    }
    DataType& Data() const { return static_cast<EntryType*>(Get())->mData; }

   private:
    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;
  };

  Iterator Iter() { return Iterator(this); }

  Iterator ConstIter() const {
    return Iterator(const_cast<nsBaseHashtable*>(this));
  }

  using typename nsTHashtable<EntryType>::iterator;
  using typename nsTHashtable<EntryType>::const_iterator;

  using nsTHashtable<EntryType>::begin;
  using nsTHashtable<EntryType>::end;
  using nsTHashtable<EntryType>::cbegin;
  using nsTHashtable<EntryType>::cend;

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
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return this->mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Like ShallowSizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Swap the elements in this hashtable with the elements in aOther.
   */
  void SwapElements(nsBaseHashtable& aOther) {
    nsTHashtable<EntryType>::SwapElements(aOther);
  }

  using nsTHashtable<EntryType>::MarkImmutable;
};

//
// nsBaseHashtableET definitions
//

template <class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::nsBaseHashtableET(KeyTypePointer aKey)
    : KeyClass(aKey), mData() {}

template <class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::nsBaseHashtableET(
    nsBaseHashtableET<KeyClass, DataType>&& aToMove)
    : KeyClass(std::move(aToMove)), mData(std::move(aToMove.mData)) {}

#endif  // nsBaseHashtable_h__
