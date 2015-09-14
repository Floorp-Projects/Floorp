/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTHashtable_h__
#define nsTHashtable_h__

#include "nscore.h"
#include "pldhash.h"
#include "nsDebug.h"
#include "mozilla/Assertions.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/fallible.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Attributes.h"

#include <new>

/**
 * a base class for templated hashtables.
 *
 * Clients will rarely need to use this class directly. Check the derived
 * classes first, to see if they will meet your needs.
 *
 * @param EntryType  the templated entry-type class that is managed by the
 *   hashtable. <code>EntryType</code> must extend the following declaration,
 *   and <strong>must not declare any virtual functions or derive from classes
 *   with virtual functions.</strong>  Any vtable pointer would break the
 *   PLDHashTable code.
 *<pre>   class EntryType : public PLDHashEntryHdr
 *   {
 *   public: or friend nsTHashtable<EntryType>;
 *     // KeyType is what we use when Get()ing or Put()ing this entry
 *     // this should either be a simple datatype (uint32_t, nsISupports*) or
 *     // a const reference (const nsAString&)
 *     typedef something KeyType;
 *     // KeyTypePointer is the pointer-version of KeyType, because pldhash.h
 *     // requires keys to cast to <code>const void*</code>
 *     typedef const something* KeyTypePointer;
 *
 *     EntryType(KeyTypePointer aKey);
 *
 *     // A copy or C++11 Move constructor must be defined, even if
 *     // AllowMemMove() == true, otherwise you will cause link errors.
 *     EntryType(const EntryType& aEnt);  // Either this...
 *     EntryType(EntryType&& aEnt);       // ...or this
 *
 *     // the destructor must be defined... or you will cause link errors!
 *     ~EntryType();
 *
 *     // KeyEquals(): does this entry match this key?
 *     bool KeyEquals(KeyTypePointer aKey) const;
 *
 *     // KeyToPointer(): Convert KeyType to KeyTypePointer
 *     static KeyTypePointer KeyToPointer(KeyType aKey);
 *
 *     // HashKey(): calculate the hash number
 *     static PLDHashNumber HashKey(KeyTypePointer aKey);
 *
 *     // ALLOW_MEMMOVE can we move this class with memmove(), or do we have
 *     // to use the copy constructor?
 *     enum { ALLOW_MEMMOVE = true/false };
 *   }</pre>
 *
 * @see nsInterfaceHashtable
 * @see nsDataHashtable
 * @see nsClassHashtable
 * @author "Benjamin Smedberg <bsmedberg@covad.net>"
 */

template<class EntryType>
class MOZ_NEEDS_NO_VTABLE_TYPE nsTHashtable
{
  typedef mozilla::fallible_t fallible_t;

public:
  // Separate constructors instead of default aInitLength parameter since
  // otherwise the default no-arg constructor isn't found.
  nsTHashtable()
    : mTable(Ops(), sizeof(EntryType), PLDHashTable::kDefaultInitialLength)
  {}
  explicit nsTHashtable(uint32_t aInitLength)
    : mTable(Ops(), sizeof(EntryType), aInitLength)
  {}

  /**
   * destructor, cleans up and deallocates
   */
  ~nsTHashtable();

  nsTHashtable(nsTHashtable<EntryType>&& aOther);

  /**
   * Return the generation number for the table. This increments whenever
   * the table data items are moved.
   */
  uint32_t GetGeneration() const { return mTable.Generation(); }

  /**
   * KeyType is typedef'ed for ease of use.
   */
  typedef typename EntryType::KeyType KeyType;

  /**
   * KeyTypePointer is typedef'ed for ease of use.
   */
  typedef typename EntryType::KeyTypePointer KeyTypePointer;

  /**
   * Return the number of entries in the table.
   * @return    number of entries
   */
  uint32_t Count() const { return mTable.EntryCount(); }

  /**
   * Get the entry associated with a key.
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class, if the key exists; nullptr if the
   *            key doesn't exist
   */
  EntryType* GetEntry(KeyType aKey) const
  {
    return static_cast<EntryType*>(
      const_cast<PLDHashTable*>(&mTable)->Search(EntryType::KeyToPointer(aKey)));
  }

  /**
   * Return true if an entry for the given key exists, false otherwise.
   * @param     aKey the key to retrieve
   * @return    true if the key exists, false if the key doesn't exist
   */
  bool Contains(KeyType aKey) const { return !!GetEntry(aKey); }

  /**
   * Get the entry associated with a key, or create a new entry,
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class retreived; nullptr only if memory
                can't be allocated
   */
  EntryType* PutEntry(KeyType aKey)
  {
    // infallible add
    return static_cast<EntryType*>(mTable.Add(EntryType::KeyToPointer(aKey)));
  }

  MOZ_WARN_UNUSED_RESULT
  EntryType* PutEntry(KeyType aKey, const fallible_t&)
  {
    return static_cast<EntryType*>(mTable.Add(EntryType::KeyToPointer(aKey),
                                              mozilla::fallible));
  }

  /**
   * Remove the entry associated with a key.
   * @param     aKey of the entry to remove
   */
  void RemoveEntry(KeyType aKey)
  {
    mTable.Remove(EntryType::KeyToPointer(aKey));
  }

  /**
   * Remove the entry associated with a key, but don't resize the hashtable.
   * This is a low-level method, and is not recommended unless you know what
   * you're doing and you need the extra performance. This method can be used
   * during enumeration, while RemoveEntry() cannot.
   * @param aEntry   the entry-pointer to remove (obtained from GetEntry or
   *                 the enumerator
   */
  void RawRemoveEntry(EntryType* aEntry)
  {
    mTable.RawRemove(aEntry);
  }

  // This is an iterator that also allows entry removal. Example usage:
  //
  //   for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
  //     Entry* entry = iter.Get();
  //     // ... do stuff with |entry| ...
  //     // ... possibly call iter.Remove() once ...
  //   }
  //
  class Iterator : public PLDHashTable::Iterator
  {
  public:
    typedef PLDHashTable::Iterator Base;

    explicit Iterator(nsTHashtable* aTable) : Base(&aTable->mTable) {}
    Iterator(Iterator&& aOther) : Base(aOther.mTable) {}
    ~Iterator() {}

    EntryType* Get() const { return static_cast<EntryType*>(Base::Get()); }

  private:
    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;
  };

  Iterator Iter() { return Iterator(this); }

  Iterator ConstIter() const
  {
    return Iterator(const_cast<nsTHashtable*>(this));
  }

  /**
   * Remove all entries, return hashtable to "pristine" state. It's
   * conceptually the same as calling the destructor and then re-calling the
   * constructor.
   */
  void Clear()
  {
    mTable.Clear();
  }

  /**
   * Measure the size of the table's entry storage. Does *not* measure anything
   * hanging off table entries; hence the "Shallow" prefix. To measure that,
   * either use SizeOfExcludingThis() or iterate manually over the entries,
   * calling SizeOfExcludingThis() on each one.
   *
   * @param     aMallocSizeOf the function used to measure heap-allocated blocks
   * @return    the measured shallow size of the table
   */
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Like ShallowSizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * This is a "deep" measurement of the table. To use it, |EntryType| must
   * define SizeOfExcludingThis, and that method will be called on all live
   * entries.
   */
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    size_t n = ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = ConstIter(); !iter.Done(); iter.Next()) {
      n += (*iter.Get()).SizeOfExcludingThis(aMallocSizeOf);
    }
    return n;
  }

  /**
   * Like SizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Swap the elements in this hashtable with the elements in aOther.
   */
  void SwapElements(nsTHashtable<EntryType>& aOther)
  {
    MOZ_ASSERT_IF(this->mTable.Ops() && aOther.mTable.Ops(),
                  this->mTable.Ops() == aOther.mTable.Ops());
    mozilla::Swap(this->mTable, aOther.mTable);
  }

#ifdef DEBUG
  /**
   * Mark the table as constant after initialization.
   *
   * This will prevent assertions when a read-only hash is accessed on multiple
   * threads without synchronization.
   */
  void MarkImmutable()
  {
    PL_DHashMarkTableImmutable(&mTable);
  }
#endif

protected:
  PLDHashTable mTable;

  static const void* s_GetKey(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

  static PLDHashNumber s_HashKey(PLDHashTable* aTable, const void* aKey);

  static bool s_MatchEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aEntry,
                           const void* aKey);

  static void s_CopyEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aFrom,
                          PLDHashEntryHdr* aTo);

  static void s_ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

  static void s_InitEntry(PLDHashEntryHdr* aEntry, const void* aKey);

private:
  // copy constructor, not implemented
  nsTHashtable(nsTHashtable<EntryType>& aToCopy) = delete;

  /**
   * Gets the table's ops.
   */
  static const PLDHashTableOps* Ops();

  // assignment operator, not implemented
  nsTHashtable<EntryType>& operator=(nsTHashtable<EntryType>& aToEqual) = delete;
};

//
// template definitions
//

template<class EntryType>
nsTHashtable<EntryType>::nsTHashtable(nsTHashtable<EntryType>&& aOther)
  : mTable(mozilla::Move(aOther.mTable))
{
  // aOther shouldn't touch mTable after this, because we've stolen the table's
  // pointers but not overwitten them.
  MOZ_MAKE_MEM_UNDEFINED(&aOther.mTable, sizeof(aOther.mTable));
}

template<class EntryType>
nsTHashtable<EntryType>::~nsTHashtable()
{
}

template<class EntryType>
/* static */ const PLDHashTableOps*
nsTHashtable<EntryType>::Ops()
{
  // If this variable is a global variable, we get strange start-up failures on
  // WindowsCrtPatch.h (see bug 1166598 comment 20). But putting it inside a
  // function avoids that problem.
  static const PLDHashTableOps sOps =
  {
    s_HashKey,
    s_MatchEntry,
    EntryType::ALLOW_MEMMOVE ? ::PL_DHashMoveEntryStub : s_CopyEntry,
    s_ClearEntry,
    s_InitEntry
  };
  return &sOps;
}

// static definitions

template<class EntryType>
PLDHashNumber
nsTHashtable<EntryType>::s_HashKey(PLDHashTable* aTable, const void* aKey)
{
  return EntryType::HashKey(reinterpret_cast<const KeyTypePointer>(aKey));
}

template<class EntryType>
bool
nsTHashtable<EntryType>::s_MatchEntry(PLDHashTable* aTable,
                                      const PLDHashEntryHdr* aEntry,
                                      const void* aKey)
{
  return ((const EntryType*)aEntry)->KeyEquals(
    reinterpret_cast<const KeyTypePointer>(aKey));
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_CopyEntry(PLDHashTable* aTable,
                                     const PLDHashEntryHdr* aFrom,
                                     PLDHashEntryHdr* aTo)
{
  EntryType* fromEntry =
    const_cast<EntryType*>(reinterpret_cast<const EntryType*>(aFrom));

  new (aTo) EntryType(mozilla::Move(*fromEntry));

  fromEntry->~EntryType();
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_ClearEntry(PLDHashTable* aTable,
                                      PLDHashEntryHdr* aEntry)
{
  static_cast<EntryType*>(aEntry)->~EntryType();
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_InitEntry(PLDHashEntryHdr* aEntry,
                                     const void* aKey)
{
  new (aEntry) EntryType(reinterpret_cast<KeyTypePointer>(aKey));
}

class nsCycleCollectionTraversalCallback;

template<class EntryType>
inline void
ImplCycleCollectionUnlink(nsTHashtable<EntryType>& aField)
{
  aField.Clear();
}

template<class EntryType>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsTHashtable<EntryType>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    EntryType* entry = iter.Get();
    ImplCycleCollectionTraverse(aCallback, *entry, aName, aFlags);
  }
}

#endif // nsTHashtable_h__
