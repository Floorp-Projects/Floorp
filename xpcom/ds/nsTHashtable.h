/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// See the comment at the top of mfbt/HashTable.h for a comparison between
// PLDHashTable and mozilla::HashTable.

#ifndef nsTHashtable_h__
#define nsTHashtable_h__

#include <new>
#include <type_traits>
#include <utility>

#include "PLDHashTable.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/fallible.h"
#include "nsPointerHashKeys.h"

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
 *     // KeyTypePointer is the pointer-version of KeyType, because
 *     // PLDHashTable.h requires keys to cast to <code>const void*</code>
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

template <class EntryType>
class MOZ_NEEDS_NO_VTABLE_TYPE nsTHashtable {
  typedef mozilla::fallible_t fallible_t;
  static_assert(std::is_pointer_v<typename EntryType::KeyTypePointer>,
                "KeyTypePointer should be a pointer");

 public:
  // Separate constructors instead of default aInitLength parameter since
  // otherwise the default no-arg constructor isn't found.
  nsTHashtable()
      : mTable(Ops(), sizeof(EntryType), PLDHashTable::kDefaultInitialLength) {}
  explicit nsTHashtable(uint32_t aInitLength)
      : mTable(Ops(), sizeof(EntryType), aInitLength) {}

  /**
   * destructor, cleans up and deallocates
   */
  ~nsTHashtable() = default;

  nsTHashtable(nsTHashtable<EntryType>&& aOther);
  nsTHashtable<EntryType>& operator=(nsTHashtable<EntryType>&& aOther);

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
   * Return true if the hashtable is empty.
   */
  bool IsEmpty() const { return Count() == 0; }

  /**
   * Get the entry associated with a key.
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class, if the key exists; nullptr if the
   *            key doesn't exist
   */
  EntryType* GetEntry(KeyType aKey) const {
    return static_cast<EntryType*>(
        mTable.Search(EntryType::KeyToPointer(aKey)));
  }

  /**
   * Return true if an entry for the given key exists, false otherwise.
   * @param     aKey the key to retrieve
   * @return    true if the key exists, false if the key doesn't exist
   */
  bool Contains(KeyType aKey) const { return !!GetEntry(aKey); }

  /**
   * Infallibly get the entry associated with a key, or create a new entry,
   * @param     aKey the key to retrieve
   * @return    pointer to the entry retrieved; never nullptr
   */
  EntryType* PutEntry(KeyType aKey) {
    // infallible add
    return static_cast<EntryType*>(mTable.Add(EntryType::KeyToPointer(aKey)));
  }

  /**
   * Fallibly get the entry associated with a key, or create a new entry,
   * @param     aKey the key to retrieve
   * @return    pointer to the entry retrieved; nullptr only if memory can't
   *            be allocated
   */
  [[nodiscard]] EntryType* PutEntry(KeyType aKey, const fallible_t&) {
    return static_cast<EntryType*>(
        mTable.Add(EntryType::KeyToPointer(aKey), mozilla::fallible));
  }

  /**
   * Get the entry associated with a key, or create a new entry using infallible
   * allocation and insert that.
   * @param     aKey the key to retrieve
   * @param     aEntry will be assigned (if non-null) to the entry that was
   * found or created
   * @return    true if a new entry was created, or false if an existing entry
   *            was found
   */
  [[nodiscard]] bool EnsureInserted(KeyType aKey,
                                    EntryType** aEntry = nullptr) {
    auto oldCount = Count();
    EntryType* entry = PutEntry(aKey);
    if (aEntry) {
      *aEntry = entry;
    }
    return oldCount != Count();
  }

  /**
   * Remove the entry associated with a key.
   * @param     aKey of the entry to remove
   */
  void RemoveEntry(KeyType aKey) {
    mTable.Remove(EntryType::KeyToPointer(aKey));
  }

  /**
   * Lookup the entry associated with aKey and remove it if found, otherwise
   * do nothing.
   * @param     aKey of the entry to remove
   * @return    true if an entry was found and removed, or false if no entry
   *            was found for aKey
   */
  bool EnsureRemoved(KeyType aKey) {
    auto* entry = GetEntry(aKey);
    if (entry) {
      RemoveEntry(entry);
      return true;
    }
    return false;
  }

  /**
   * Remove the entry associated with a key.
   * @param aEntry   the entry-pointer to remove (obtained from GetEntry)
   */
  void RemoveEntry(EntryType* aEntry) { mTable.RemoveEntry(aEntry); }

  /**
   * Remove the entry associated with a key, but don't resize the hashtable.
   * This is a low-level method, and is not recommended unless you know what
   * you're doing. If you use it, please add a comment explaining why you
   * didn't use RemoveEntry().
   * @param aEntry   the entry-pointer to remove (obtained from GetEntry)
   */
  void RawRemoveEntry(EntryType* aEntry) { mTable.RawRemove(aEntry); }

  // This is an iterator that also allows entry removal. Example usage:
  //
  //   for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
  //     Entry* entry = iter.Get();
  //     // ... do stuff with |entry| ...
  //     // ... possibly call iter.Remove() once ...
  //   }
  //
  class Iterator : public PLDHashTable::Iterator {
   public:
    typedef PLDHashTable::Iterator Base;

    explicit Iterator(nsTHashtable* aTable) : Base(&aTable->mTable) {}
    Iterator(Iterator&& aOther) : Base(aOther.mTable) {}
    ~Iterator() = default;

    EntryType* Get() const { return static_cast<EntryType*>(Base::Get()); }

   private:
    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;
  };

  Iterator Iter() { return Iterator(this); }

  Iterator ConstIter() const {
    return Iterator(const_cast<nsTHashtable*>(this));
  }

  /**
   * Remove all entries, return hashtable to "pristine" state. It's
   * conceptually the same as calling the destructor and then re-calling the
   * constructor.
   */
  void Clear() { mTable.Clear(); }

  /**
   * Measure the size of the table's entry storage. Does *not* measure anything
   * hanging off table entries; hence the "Shallow" prefix. To measure that,
   * either use SizeOfExcludingThis() or iterate manually over the entries,
   * calling SizeOfExcludingThis() on each one.
   *
   * @param     aMallocSizeOf the function used to measure heap-allocated blocks
   * @return    the measured shallow size of the table
   */
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Like ShallowSizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * This is a "deep" measurement of the table. To use it, |EntryType| must
   * define SizeOfExcludingThis, and that method will be called on all live
   * entries.
   */
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t n = ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = ConstIter(); !iter.Done(); iter.Next()) {
      n += (*iter.Get()).SizeOfExcludingThis(aMallocSizeOf);
    }
    return n;
  }

  /**
   * Like SizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Swap the elements in this hashtable with the elements in aOther.
   */
  void SwapElements(nsTHashtable<EntryType>& aOther) {
    MOZ_ASSERT_IF(this->mTable.Ops() && aOther.mTable.Ops(),
                  this->mTable.Ops() == aOther.mTable.Ops());
    std::swap(this->mTable, aOther.mTable);
  }

  /**
   * Mark the table as constant after initialization.
   *
   * This will prevent assertions when a read-only hash is accessed on multiple
   * threads without synchronization.
   */
  void MarkImmutable() { mTable.MarkImmutable(); }

 protected:
  PLDHashTable mTable;

  static PLDHashNumber s_HashKey(const void* aKey);

  static bool s_MatchEntry(const PLDHashEntryHdr* aEntry, const void* aKey);

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
  nsTHashtable<EntryType>& operator=(nsTHashtable<EntryType>& aToEqual) =
      delete;
};

namespace mozilla {
namespace detail {

// Like PLDHashTable::MoveEntryStub, but specialized for fixed N (i.e. the size
// of the entries in the hashtable).  Saves a memory read to figure out the size
// from the table and gives the compiler the opportunity to inline the memcpy.
//
// We define this outside of nsTHashtable so only one copy exists for every N,
// rather than separate copies for every EntryType used with nsTHashtable.
template <size_t N>
static void FixedSizeEntryMover(PLDHashTable*, const PLDHashEntryHdr* aFrom,
                                PLDHashEntryHdr* aTo) {
  memcpy(aTo, aFrom, N);
}

}  // namespace detail
}  // namespace mozilla

//
// template definitions
//

template <class EntryType>
nsTHashtable<EntryType>::nsTHashtable(nsTHashtable<EntryType>&& aOther)
    : mTable(std::move(aOther.mTable)) {}

template <class EntryType>
nsTHashtable<EntryType>& nsTHashtable<EntryType>::operator=(
    nsTHashtable<EntryType>&& aOther) {
  mTable = std::move(aOther.mTable);
  return *this;
}

template <class EntryType>
/* static */ const PLDHashTableOps* nsTHashtable<EntryType>::Ops() {
  // If this variable is a global variable, we get strange start-up failures on
  // WindowsCrtPatch.h (see bug 1166598 comment 20). But putting it inside a
  // function avoids that problem.
  static const PLDHashTableOps sOps = {
      s_HashKey, s_MatchEntry,
      EntryType::ALLOW_MEMMOVE
          ? mozilla::detail::FixedSizeEntryMover<sizeof(EntryType)>
          : s_CopyEntry,
      s_ClearEntry, s_InitEntry};
  return &sOps;
}

// static definitions

template <class EntryType>
PLDHashNumber nsTHashtable<EntryType>::s_HashKey(const void* aKey) {
  return EntryType::HashKey(static_cast<KeyTypePointer>(aKey));
}

template <class EntryType>
bool nsTHashtable<EntryType>::s_MatchEntry(const PLDHashEntryHdr* aEntry,
                                           const void* aKey) {
  return (static_cast<const EntryType*>(aEntry))
      ->KeyEquals(static_cast<KeyTypePointer>(aKey));
}

template <class EntryType>
void nsTHashtable<EntryType>::s_CopyEntry(PLDHashTable* aTable,
                                          const PLDHashEntryHdr* aFrom,
                                          PLDHashEntryHdr* aTo) {
  EntryType* fromEntry =
      const_cast<EntryType*>(static_cast<const EntryType*>(aFrom));

  new (mozilla::KnownNotNull, aTo) EntryType(std::move(*fromEntry));

  fromEntry->~EntryType();
}

template <class EntryType>
void nsTHashtable<EntryType>::s_ClearEntry(PLDHashTable* aTable,
                                           PLDHashEntryHdr* aEntry) {
  static_cast<EntryType*>(aEntry)->~EntryType();
}

template <class EntryType>
void nsTHashtable<EntryType>::s_InitEntry(PLDHashEntryHdr* aEntry,
                                          const void* aKey) {
  new (mozilla::KnownNotNull, aEntry)
      EntryType(static_cast<KeyTypePointer>(aKey));
}

class nsCycleCollectionTraversalCallback;

template <class EntryType>
inline void ImplCycleCollectionUnlink(nsTHashtable<EntryType>& aField) {
  aField.Clear();
}

template <class EntryType>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsTHashtable<EntryType>& aField, const char* aName, uint32_t aFlags = 0) {
  for (auto iter = aField.Iter(); !iter.Done(); iter.Next()) {
    EntryType* entry = iter.Get();
    ImplCycleCollectionTraverse(aCallback, *entry, aName, aFlags);
  }
}

/**
 * For nsTHashtable with pointer entries, we can have a template specialization
 * that layers a typed T* interface on top of a common implementation that
 * works internally with void pointers.  This arrangement saves code size and
 * might slightly improve performance as well.
 */

/**
 * We need a separate entry type class for the inheritance structure of the
 * nsTHashtable specialization below; nsVoidPtrHashKey is simply typedefed to a
 * specialization of nsPtrHashKey, and the formulation:
 *
 * class nsTHashtable<nsPtrHashKey<T>> :
 *   protected nsTHashtable<nsPtrHashKey<const void>
 *
 * is not going to turn out very well, since we'd wind up with an nsTHashtable
 * instantiation that is its own base class.
 */
namespace detail {

class VoidPtrHashKey : public nsPtrHashKey<const void> {
  typedef nsPtrHashKey<const void> Base;

 public:
  explicit VoidPtrHashKey(const void* aKey) : Base(aKey) {}
};

}  // namespace detail

/**
 * See the main nsTHashtable documentation for descriptions of this class's
 * methods.
 */
template <typename T>
class nsTHashtable<nsPtrHashKey<T>>
    : protected nsTHashtable<::detail::VoidPtrHashKey> {
  typedef nsTHashtable<::detail::VoidPtrHashKey> Base;
  typedef nsPtrHashKey<T> EntryType;

  // We play games with reinterpret_cast'ing between these two classes, so
  // try to ensure that playing said games is reasonable.
  static_assert(sizeof(nsPtrHashKey<T>) == sizeof(::detail::VoidPtrHashKey),
                "hash keys must be the same size");

  nsTHashtable(const nsTHashtable& aOther) = delete;
  nsTHashtable& operator=(const nsTHashtable& aOther) = delete;

 public:
  nsTHashtable() = default;
  explicit nsTHashtable(uint32_t aInitLength) : Base(aInitLength) {}

  ~nsTHashtable() = default;

  nsTHashtable(nsTHashtable&&) = default;

  using Base::Clear;
  using Base::Count;
  using Base::GetGeneration;
  using Base::IsEmpty;

  using Base::MarkImmutable;
  using Base::ShallowSizeOfExcludingThis;
  using Base::ShallowSizeOfIncludingThis;

  /* Wrapper functions */
  EntryType* GetEntry(T* aKey) const {
    return reinterpret_cast<EntryType*>(Base::GetEntry(aKey));
  }

  bool Contains(T* aKey) const { return Base::Contains(aKey); }

  EntryType* PutEntry(T* aKey) {
    return reinterpret_cast<EntryType*>(Base::PutEntry(aKey));
  }

  [[nodiscard]] EntryType* PutEntry(T* aKey, const mozilla::fallible_t&) {
    return reinterpret_cast<EntryType*>(
        Base::PutEntry(aKey, mozilla::fallible));
  }

  [[nodiscard]] bool EnsureInserted(T* aKey, EntryType** aEntry = nullptr) {
    return Base::EnsureInserted(
        aKey, reinterpret_cast<::detail::VoidPtrHashKey**>(aEntry));
  }

  void RemoveEntry(T* aKey) { Base::RemoveEntry(aKey); }

  bool EnsureRemoved(T* aKey) { return Base::EnsureRemoved(aKey); }

  void RemoveEntry(EntryType* aEntry) {
    Base::RemoveEntry(reinterpret_cast<::detail::VoidPtrHashKey*>(aEntry));
  }

  void RawRemoveEntry(EntryType* aEntry) {
    Base::RawRemoveEntry(reinterpret_cast<::detail::VoidPtrHashKey*>(aEntry));
  }

  class Iterator : public Base::Iterator {
   public:
    typedef nsTHashtable::Base::Iterator Base;

    explicit Iterator(nsTHashtable* aTable) : Base(aTable) {}
    Iterator(Iterator&& aOther) : Base(std::move(aOther)) {}
    ~Iterator() = default;

    EntryType* Get() const { return reinterpret_cast<EntryType*>(Base::Get()); }

   private:
    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(Iterator&&) = delete;
  };

  Iterator Iter() { return Iterator(this); }

  Iterator ConstIter() const {
    return Iterator(const_cast<nsTHashtable*>(this));
  }

  void SwapElements(nsTHashtable& aOther) { Base::SwapElements(aOther); }
};

#endif  // nsTHashtable_h__
