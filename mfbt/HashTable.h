/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A note on the differences between mozilla::HashTable and PLDHashTable (and
// its subclasses, such as nsTHashtable).
//
// - mozilla::HashTable is a lot faster, largely because it uses templates
//   throughout *and* inlines everything. PLDHashTable inlines operations much
//   less aggressively, and also uses "virtual ops" for operations like hashing
//   and matching entries that require function calls.
//
// - Correspondingly, mozilla::HashTable use is likely to increase executable
//   size much more than PLDHashTable.
//
// - mozilla::HashTable has a nicer API, with a proper HashSet vs. HashMap
//   distinction.
//
// - mozilla::HashTable requires more explicit OOM checking. Use
//   mozilla::InfallibleAllocPolicy to make allocations infallible; note that
//   return values of possibly-allocating methods such as add() will still need
//   checking in some fashion -- e.g. with MOZ_ALWAYS_TRUE() -- due to the use
//   of MOZ_MUST_USE.
//
// - mozilla::HashTable has a default capacity on creation of 32 and a minimum
//   capacity of 4. PLDHashTable has a default capacity on creation of 8 and a
//   minimum capacity of 8.
//
// - mozilla::HashTable allocates memory eagerly. PLDHashTable delays
//   allocating until the first element is inserted.

#ifndef mozilla_HashTable_h
#define mozilla_HashTable_h

#include "mozilla/AllocPolicy.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Casting.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/Opaque.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ReentrancyGuard.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

template<class>
struct DefaultHasher;

template<class, class>
class HashMapEntry;

namespace detail {

template<typename T>
class HashTableEntry;

template<class T, class HashPolicy, class AllocPolicy>
class HashTable;

} // namespace detail

/*****************************************************************************/

// The "generation" of a hash table is an opaque value indicating the state of
// modification of the hash table through its lifetime.  If the generation of
// a hash table compares equal at times T1 and T2, then lookups in the hash
// table, pointers to (or into) hash table entries, etc. at time T1 are valid
// at time T2.  If the generation compares unequal, these computations are all
// invalid and must be performed again to be used.
//
// Generations are meaningfully comparable only with respect to a single hash
// table.  It's always nonsensical to compare the generation of distinct hash
// tables H1 and H2.
using Generation = Opaque<uint64_t>;

// A performant, STL-like container providing a hash-based map from keys to
// values. In particular, HashMap calls constructors and destructors of all
// objects added so non-PODs may be used safely.
//
// Key/Value requirements:
//  - movable, destructible, assignable
// HashPolicy requirements:
//  - see Hash Policy section below
// AllocPolicy:
//  - see AllocPolicy.h
//
// Note:
// - HashMap is not reentrant: Key/Value/HashPolicy/AllocPolicy members
//   called by HashMap must not call back into the same HashMap object.
// - Due to the lack of exception handling, the user must call |init()|.
template<class Key,
         class Value,
         class HashPolicy = DefaultHasher<Key>,
         class AllocPolicy = MallocAllocPolicy>
class HashMap
{
  using TableEntry = HashMapEntry<Key, Value>;

  struct MapHashPolicy : HashPolicy
  {
    using Base = HashPolicy;
    using KeyType = Key;

    static const Key& getKey(TableEntry& aEntry) { return aEntry.key(); }

    static void setKey(TableEntry& aEntry, Key& aKey)
    {
      HashPolicy::rekey(aEntry.mutableKey(), aKey);
    }
  };

  using Impl = detail::HashTable<TableEntry, MapHashPolicy, AllocPolicy>;
  Impl mImpl;

public:
  using Lookup = typename HashPolicy::Lookup;
  using Entry = TableEntry;

  // HashMap construction is fallible (due to OOM); thus the user must call
  // init after constructing a HashMap and check the return value.
  explicit HashMap(AllocPolicy aPolicy = AllocPolicy())
    : mImpl(aPolicy)
  {
  }

  MOZ_MUST_USE bool init(uint32_t aLen = 16) { return mImpl.init(aLen); }

  bool initialized() const { return mImpl.initialized(); }

  // Return whether the given lookup value is present in the map. E.g.:
  //
  //   using HM = HashMap<int,char>;
  //   HM h;
  //   if (HM::Ptr p = h.lookup(3)) {
  //     const HM::Entry& e = *p; // p acts like a pointer to Entry
  //     assert(p->key == 3);     // Entry contains the key
  //     char val = p->value;     // and value
  //   }
  //
  // Also see the definition of Ptr in HashTable above (with T = Entry).
  using Ptr = typename Impl::Ptr;
  MOZ_ALWAYS_INLINE Ptr lookup(const Lookup& aLookup) const
  {
    return mImpl.lookup(aLookup);
  }

  // Like lookup, but does not assert if two threads call lookup at the same
  // time. Only use this method when none of the threads will modify the map.
  MOZ_ALWAYS_INLINE Ptr readonlyThreadsafeLookup(const Lookup& aLookup) const
  {
    return mImpl.readonlyThreadsafeLookup(aLookup);
  }

  // Assuming |p.found()|, remove |*p|.
  void remove(Ptr aPtr) { mImpl.remove(aPtr); }

  // Like |lookup(l)|, but on miss, |p = lookupForAdd(l)| allows efficient
  // insertion of Key |k| (where |HashPolicy::match(k,l) == true|) using
  // |add(p,k,v)|. After |add(p,k,v)|, |p| points to the new Entry. E.g.:
  //
  //   using HM = HashMap<int,char>;
  //   HM h;
  //   HM::AddPtr p = h.lookupForAdd(3);
  //   if (!p) {
  //     if (!h.add(p, 3, 'a')) {
  //       return false;
  //     }
  //   }
  //   const HM::Entry& e = *p;   // p acts like a pointer to Entry
  //   assert(p->key == 3);       // Entry contains the key
  //   char val = p->value;       // and value
  //
  // Also see the definition of AddPtr in HashTable above (with T = Entry).
  //
  // N.B. The caller must ensure that no mutating hash table operations
  // occur between a pair of |lookupForAdd| and |add| calls. To avoid
  // looking up the key a second time, the caller may use the more efficient
  // relookupOrAdd method. This method reuses part of the hashing computation
  // to more efficiently insert the key if it has not been added. For
  // example, a mutation-handling version of the previous example:
  //
  //    HM::AddPtr p = h.lookupForAdd(3);
  //    if (!p) {
  //      call_that_may_mutate_h();
  //      if (!h.relookupOrAdd(p, 3, 'a')) {
  //        return false;
  //      }
  //    }
  //    const HM::Entry& e = *p;
  //    assert(p->key == 3);
  //    char val = p->value;
  //
  using AddPtr = typename Impl::AddPtr;
  MOZ_ALWAYS_INLINE AddPtr lookupForAdd(const Lookup& aLookup) const
  {
    return mImpl.lookupForAdd(aLookup);
  }

  template<typename KeyInput, typename ValueInput>
  MOZ_MUST_USE bool add(AddPtr& aPtr, KeyInput&& aKey, ValueInput&& aValue)
  {
    return mImpl.add(
      aPtr, std::forward<KeyInput>(aKey), std::forward<ValueInput>(aValue));
  }

  template<typename KeyInput>
  MOZ_MUST_USE bool add(AddPtr& aPtr, KeyInput&& aKey)
  {
    return mImpl.add(aPtr, std::forward<KeyInput>(aKey), Value());
  }

  template<typename KeyInput, typename ValueInput>
  MOZ_MUST_USE bool relookupOrAdd(AddPtr& aPtr,
                                  KeyInput&& aKey,
                                  ValueInput&& aValue)
  {
    return mImpl.relookupOrAdd(aPtr,
                               aKey,
                               std::forward<KeyInput>(aKey),
                               std::forward<ValueInput>(aValue));
  }

  // |iter()| returns an Iterator:
  //
  //   HashMap<int, char> h;
  //   for (auto iter = h.iter(); !iter.done(); iter.next()) {
  //     char c = iter.get().value();
  //   }
  //
  // Also see the definition of Iterator in HashTable above (with T = Entry).
  using Iterator = typename Impl::Iterator;
  Iterator iter() const { return mImpl.iter(); }

  // |modIter()| returns a ModIterator:
  //
  //   HashMap<int, char> h;
  //   for (auto iter = h.modIter(); !iter.done(); iter.next()) {
  //     if (iter.get().value() == 'l') {
  //       iter.remove();
  //     }
  //   }
  //
  // Table resize may occur in ModIterator's destructor. Also see the
  // definition of ModIterator in HashTable above (with T = Entry).
  using ModIterator = typename Impl::ModIterator;
  ModIterator modIter() { return mImpl.modIter(); }

  // These are similar to Iterator/ModIterator/iter(), but use less common
  // terminology.
  using Range = typename Impl::Range;
  using Enum = typename Impl::Enum;
  Range all() const { return mImpl.all(); }

  // Remove all entries. This does not shrink the table. For that consider
  // using the finish() method.
  void clear() { mImpl.clear(); }

  // Remove all entries. Unlike clear() this method tries to shrink the table.
  // Unlike finish() it does not require the map to be initialized again.
  void clearAndShrink() { mImpl.clearAndShrink(); }

  // Remove all the entries and release all internal buffers. The map must
  // be initialized again before any use.
  void finish() { mImpl.finish(); }

  // Does the table contain any entries?
  bool empty() const { return mImpl.empty(); }

  // Number of live elements in the map.
  uint32_t count() const { return mImpl.count(); }

  // Total number of allocation in the dynamic table. Note: resize will
  // happen well before count() == capacity().
  size_t capacity() const { return mImpl.capacity(); }

  // Measure the size of the HashMap's entry storage. If the entries contain
  // pointers to other heap blocks, you must iterate over the table and measure
  // them separately; hence the "shallow" prefix.
  size_t shallowSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return mImpl.shallowSizeOfExcludingThis(aMallocSizeOf);
  }
  size_t shallowSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) +
           mImpl.shallowSizeOfExcludingThis(aMallocSizeOf);
  }

  Generation generation() const { return mImpl.generation(); }

  /************************************************** Shorthand operations */

  bool has(const Lookup& aLookup) const
  {
    return mImpl.lookup(aLookup).found();
  }

  // Overwrite existing value with aValue. Return false on oom.
  template<typename KeyInput, typename ValueInput>
  MOZ_MUST_USE bool put(KeyInput&& aKey, ValueInput&& aValue)
  {
    AddPtr p = lookupForAdd(aKey);
    if (p) {
      p->value() = std::forward<ValueInput>(aValue);
      return true;
    }
    return add(
      p, std::forward<KeyInput>(aKey), std::forward<ValueInput>(aValue));
  }

  // Like put, but assert that the given key is not already present.
  template<typename KeyInput, typename ValueInput>
  MOZ_MUST_USE bool putNew(KeyInput&& aKey, ValueInput&& aValue)
  {
    return mImpl.putNew(
      aKey, std::forward<KeyInput>(aKey), std::forward<ValueInput>(aValue));
  }

  // Only call this to populate an empty map after reserving space with init().
  template<typename KeyInput, typename ValueInput>
  void putNewInfallible(KeyInput&& aKey, ValueInput&& aValue)
  {
    mImpl.putNewInfallible(
      aKey, std::forward<KeyInput>(aKey), std::forward<ValueInput>(aValue));
  }

  // Add (aKey,aDefaultValue) if |aKey| is not found. Return a false-y Ptr on
  // oom.
  Ptr lookupWithDefault(const Key& aKey, const Value& aDefaultValue)
  {
    AddPtr p = lookupForAdd(aKey);
    if (p) {
      return p;
    }
    bool ok = add(p, aKey, aDefaultValue);
    MOZ_ASSERT_IF(!ok, !p); // p is left false-y on oom.
    (void)ok;
    return p;
  }

  // Remove if present.
  void remove(const Lookup& aLookup)
  {
    if (Ptr p = lookup(aLookup)) {
      remove(p);
    }
  }

  // Infallibly rekey one entry, if necessary.
  // Requires template parameters Key and HashPolicy::Lookup to be the same
  // type.
  void rekeyIfMoved(const Key& aOldKey, const Key& aNewKey)
  {
    if (aOldKey != aNewKey) {
      rekeyAs(aOldKey, aNewKey, aNewKey);
    }
  }

  // Infallibly rekey one entry if present, and return whether that happened.
  bool rekeyAs(const Lookup& aOldLookup,
               const Lookup& aNewLookup,
               const Key& aNewKey)
  {
    if (Ptr p = lookup(aOldLookup)) {
      mImpl.rekeyAndMaybeRehash(p, aNewLookup, aNewKey);
      return true;
    }
    return false;
  }

  // HashMap is movable
  HashMap(HashMap&& aRhs)
    : mImpl(std::move(aRhs.mImpl))
  {
  }
  void operator=(HashMap&& aRhs)
  {
    MOZ_ASSERT(this != &aRhs, "self-move assignment is prohibited");
    mImpl = std::move(aRhs.mImpl);
  }

private:
  // HashMap is not copyable or assignable
  HashMap(const HashMap& hm) = delete;
  HashMap& operator=(const HashMap& hm) = delete;

  friend class Impl::Enum;
};

/*****************************************************************************/

// A performant, STL-like container providing a hash-based set of values. In
// particular, HashSet calls constructors and destructors of all objects added
// so non-PODs may be used safely.
//
// T requirements:
//  - movable, destructible, assignable
// HashPolicy requirements:
//  - see Hash Policy section below
// AllocPolicy:
//  - see AllocPolicy.h
//
// Note:
// - HashSet is not reentrant: T/HashPolicy/AllocPolicy members called by
//   HashSet must not call back into the same HashSet object.
// - Due to the lack of exception handling, the user must call |init()|.
template<class T,
         class HashPolicy = DefaultHasher<T>,
         class AllocPolicy = MallocAllocPolicy>
class HashSet
{
  struct SetOps : HashPolicy
  {
    using Base = HashPolicy;
    using KeyType = T;

    static const KeyType& getKey(const T& aT) { return aT; }
    static void setKey(T& aT, KeyType& aKey) { HashPolicy::rekey(aT, aKey); }
  };

  using Impl = detail::HashTable<const T, SetOps, AllocPolicy>;
  Impl mImpl;

public:
  using Lookup = typename HashPolicy::Lookup;
  using Entry = T;

  // HashSet construction is fallible (due to OOM); thus the user must call
  // init after constructing a HashSet and check the return value.
  explicit HashSet(AllocPolicy a = AllocPolicy())
    : mImpl(a)
  {
  }

  MOZ_MUST_USE bool init(uint32_t aLen = 16) { return mImpl.init(aLen); }

  bool initialized() const { return mImpl.initialized(); }

  // Return whether the given lookup value is present in the map. E.g.:
  //
  //   using HS = HashSet<int>;
  //   HS h;
  //   if (HS::Ptr p = h.lookup(3)) {
  //     assert(*p == 3);   // p acts like a pointer to int
  //   }
  //
  // Also see the definition of Ptr in HashTable above.
  using Ptr = typename Impl::Ptr;
  MOZ_ALWAYS_INLINE Ptr lookup(const Lookup& aLookup) const
  {
    return mImpl.lookup(aLookup);
  }

  // Like lookup, but does not assert if two threads call lookup at the same
  // time. Only use this method when none of the threads will modify the map.
  MOZ_ALWAYS_INLINE Ptr readonlyThreadsafeLookup(const Lookup& aLookup) const
  {
    return mImpl.readonlyThreadsafeLookup(aLookup);
  }

  // Assuming |aPtr.found()|, remove |*aPtr|.
  void remove(Ptr aPtr) { mImpl.remove(aPtr); }

  // Like |lookup(l)|, but on miss, |p = lookupForAdd(l)| allows efficient
  // insertion of T value |t| (where |HashPolicy::match(t,l) == true|) using
  // |add(p,t)|. After |add(p,t)|, |p| points to the new element. E.g.:
  //
  //   using HS = HashSet<int>;
  //   HS h;
  //   HS::AddPtr p = h.lookupForAdd(3);
  //   if (!p) {
  //     if (!h.add(p, 3)) {
  //       return false;
  //     }
  //   }
  //   assert(*p == 3);   // p acts like a pointer to int
  //
  // Also see the definition of AddPtr in HashTable above.
  //
  // N.B. The caller must ensure that no mutating hash table operations
  // occur between a pair of |lookupForAdd| and |add| calls. To avoid
  // looking up the key a second time, the caller may use the more efficient
  // relookupOrAdd method. This method reuses part of the hashing computation
  // to more efficiently insert the key if it has not been added. For
  // example, a mutation-handling version of the previous example:
  //
  //    HS::AddPtr p = h.lookupForAdd(3);
  //    if (!p) {
  //      call_that_may_mutate_h();
  //      if (!h.relookupOrAdd(p, 3, 3)) {
  //        return false;
  //      }
  //    }
  //    assert(*p == 3);
  //
  // Note that relookupOrAdd(p,l,t) performs Lookup using |l| and adds the
  // entry |t|, where the caller ensures match(l,t).
  using AddPtr = typename Impl::AddPtr;
  MOZ_ALWAYS_INLINE AddPtr lookupForAdd(const Lookup& aLookup) const
  {
    return mImpl.lookupForAdd(aLookup);
  }

  template<typename U>
  MOZ_MUST_USE bool add(AddPtr& aPtr, U&& aU)
  {
    return mImpl.add(aPtr, std::forward<U>(aU));
  }

  template<typename U>
  MOZ_MUST_USE bool relookupOrAdd(AddPtr& aPtr, const Lookup& aLookup, U&& aU)
  {
    return mImpl.relookupOrAdd(aPtr, aLookup, std::forward<U>(aU));
  }

  // |iter()| returns an Iterator:
  //
  //   HashSet<int> h;
  //   for (auto iter = h.iter(); !iter.done(); iter.next()) {
  //     int i = iter.get();
  //   }
  //
  // Also see the definition of Iterator in HashTable above.
  typedef typename Impl::Iterator Iterator;
  Iterator iter() const { return mImpl.iter(); }

  // |modIter()| returns a ModIterator:
  //
  //   HashSet<int> h;
  //   for (auto iter = h.modIter(); !iter.done(); iter.next()) {
  //     if (iter.get() == 42) {
  //       iter.remove();
  //     }
  //   }
  //
  // Table resize may occur in ModIterator's destructor. Also see the
  // definition of ModIterator in HashTable above.
  typedef typename Impl::ModIterator ModIterator;
  ModIterator modIter() { return mImpl.modIter(); }

  // These are similar to Iterator/ModIterator/iter(), but use different
  // terminology.
  using Range = typename Impl::Range;
  using Enum = typename Impl::Enum;
  Range all() const { return mImpl.all(); }

  // Remove all entries. This does not shrink the table. For that consider
  // using the finish() method.
  void clear() { mImpl.clear(); }

  // Remove all entries. Unlike clear() this method tries to shrink the table.
  // Unlike finish() it does not require the set to be initialized again.
  void clearAndShrink() { mImpl.clearAndShrink(); }

  // Remove all the entries and release all internal buffers. The set must
  // be initialized again before any use.
  void finish() { mImpl.finish(); }

  // Does the table contain any entries?
  bool empty() const { return mImpl.empty(); }

  // Number of live elements in the map.
  uint32_t count() const { return mImpl.count(); }

  // Total number of allocation in the dynamic table. Note: resize will
  // happen well before count() == capacity().
  size_t capacity() const { return mImpl.capacity(); }

  // Measure the size of the HashSet's entry storage. If the entries contain
  // pointers to other heap blocks, you must iterate over the table and measure
  // them separately; hence the "shallow" prefix.
  size_t shallowSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return mImpl.shallowSizeOfExcludingThis(aMallocSizeOf);
  }
  size_t shallowSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) +
           mImpl.shallowSizeOfExcludingThis(aMallocSizeOf);
  }

  Generation generation() const { return mImpl.generation(); }

  /************************************************** Shorthand operations */

  bool has(const Lookup& aLookup) const
  {
    return mImpl.lookup(aLookup).found();
  }

  // Add |aU| if it is not present already. Return false on oom.
  template<typename U>
  MOZ_MUST_USE bool put(U&& aU)
  {
    AddPtr p = lookupForAdd(aU);
    return p ? true : add(p, std::forward<U>(aU));
  }

  // Like put, but assert that the given key is not already present.
  template<typename U>
  MOZ_MUST_USE bool putNew(U&& aU)
  {
    return mImpl.putNew(aU, std::forward<U>(aU));
  }

  template<typename U>
  MOZ_MUST_USE bool putNew(const Lookup& aLookup, U&& aU)
  {
    return mImpl.putNew(aLookup, std::forward<U>(aU));
  }

  // Only call this to populate an empty set after reserving space with init().
  template<typename U>
  void putNewInfallible(const Lookup& aLookup, U&& aU)
  {
    mImpl.putNewInfallible(aLookup, std::forward<U>(aU));
  }

  void remove(const Lookup& aLookup)
  {
    if (Ptr p = lookup(aLookup)) {
      remove(p);
    }
  }

  // Infallibly rekey one entry, if present.
  // Requires template parameters T and HashPolicy::Lookup to be the same type.
  void rekeyIfMoved(const Lookup& aOldValue, const T& aNewValue)
  {
    if (aOldValue != aNewValue) {
      rekeyAs(aOldValue, aNewValue, aNewValue);
    }
  }

  // Infallibly rekey one entry if present, and return whether that happened.
  bool rekeyAs(const Lookup& aOldLookup,
               const Lookup& aNewLookup,
               const T& aNewValue)
  {
    if (Ptr p = lookup(aOldLookup)) {
      mImpl.rekeyAndMaybeRehash(p, aNewLookup, aNewValue);
      return true;
    }
    return false;
  }

  // Infallibly replace the current key at |p| with an equivalent key.
  // Specifically, both HashPolicy::hash and HashPolicy::match must return
  // identical results for the new and old key when applied against all
  // possible matching values.
  void replaceKey(Ptr aPtr, const T& aNewValue)
  {
    MOZ_ASSERT(aPtr.found());
    MOZ_ASSERT(*aPtr != aNewValue);
    MOZ_ASSERT(HashPolicy::hash(*aPtr) == HashPolicy::hash(aNewValue));
    MOZ_ASSERT(HashPolicy::match(*aPtr, aNewValue));
    const_cast<T&>(*aPtr) = aNewValue;
  }

  // HashSet is movable
  HashSet(HashSet&& aRhs)
    : mImpl(std::move(aRhs.mImpl))
  {
  }
  void operator=(HashSet&& aRhs)
  {
    MOZ_ASSERT(this != &aRhs, "self-move assignment is prohibited");
    mImpl = std::move(aRhs.mImpl);
  }

private:
  // HashSet is not copyable or assignable.
  HashSet(const HashSet& hs) = delete;
  HashSet& operator=(const HashSet& hs) = delete;

  friend class Impl::Enum;
};

/*****************************************************************************/

// Hash Policy
//
// A hash policy P for a hash table with key-type Key must provide:
//  - a type |P::Lookup| to use to lookup table entries;
//  - a static member function |P::hash| with signature
//
//      static mozilla::HashNumber hash(Lookup)
//
//    to use to hash the lookup type; and
//  - a static member function |P::match| with signature
//
//      static bool match(Key, Lookup)
//
//    to use to test equality of key and lookup values.
//
// Normally, Lookup = Key. In general, though, different values and types of
// values can be used to lookup and store. If a Lookup value |l| is != to the
// added Key value |k|, the user must ensure that |P::match(k,l)|. E.g.:
//
//   mozilla::HashSet<Key, P>::AddPtr p = h.lookup(l);
//   if (!p) {
//     assert(P::match(k, l));  // must hold
//     h.add(p, k);
//   }

// Pointer hashing policy that uses HashGeneric() to create good hashes for
// pointers.  Note that we don't shift out the lowest k bits to generate a
// good distribution for arena allocated pointers.
template<typename Key>
struct PointerHasher
{
  using Lookup = Key;

  static HashNumber hash(const Lookup& aLookup)
  {
    size_t word = reinterpret_cast<size_t>(aLookup);
    return HashGeneric(word);
  }

  static bool match(const Key& aKey, const Lookup& aLookup)
  {
    return aKey == aLookup;
  }

  static void rekey(Key& aKey, const Key& aNewKey) { aKey = aNewKey; }
};

// Default hash policy: just use the 'lookup' value. This of course only
// works if the lookup value is integral. HashTable applies ScrambleHashCode to
// the result of the 'hash' which means that it is 'ok' if the lookup value is
// not well distributed over the HashNumber domain.
template<class Key>
struct DefaultHasher
{
  using Lookup = Key;

  static HashNumber hash(const Lookup& aLookup)
  {
    // Hash if can implicitly cast to hash number type.
    return aLookup;
  }

  static bool match(const Key& aKey, const Lookup& aLookup)
  {
    // Use builtin or overloaded operator==.
    return aKey == aLookup;
  }

  static void rekey(Key& aKey, const Key& aNewKey) { aKey = aNewKey; }
};

// Specialize hashing policy for pointer types. It assumes that the type is
// at least word-aligned. For types with smaller size use PointerHasher.
template<class T>
struct DefaultHasher<T*> : PointerHasher<T*>
{
};

// Specialize hashing policy for mozilla::UniquePtr to proxy the UniquePtr's
// raw pointer to PointerHasher.
template<class T, class D>
struct DefaultHasher<UniquePtr<T, D>>
{
  using Lookup = UniquePtr<T, D>;
  using PtrHasher = PointerHasher<T*>;

  static HashNumber hash(const Lookup& aLookup)
  {
    return PtrHasher::hash(aLookup.get());
  }

  static bool match(const UniquePtr<T, D>& aKey, const Lookup& aLookup)
  {
    return PtrHasher::match(aKey.get(), aLookup.get());
  }

  static void rekey(UniquePtr<T, D>& aKey, UniquePtr<T, D>&& aNewKey)
  {
    aKey = std::move(aNewKey);
  }
};

// For doubles, we can xor the two uint32s.
template<>
struct DefaultHasher<double>
{
  using Lookup = double;

  static HashNumber hash(double aVal)
  {
    static_assert(sizeof(HashNumber) == 4,
                  "subsequent code assumes a four-byte hash");
    uint64_t u = BitwiseCast<uint64_t>(aVal);
    return HashNumber(u ^ (u >> 32));
  }

  static bool match(double aLhs, double aRhs)
  {
    return BitwiseCast<uint64_t>(aLhs) == BitwiseCast<uint64_t>(aRhs);
  }
};

template<>
struct DefaultHasher<float>
{
  using Lookup = float;

  static HashNumber hash(float aVal)
  {
    static_assert(sizeof(HashNumber) == 4,
                  "subsequent code assumes a four-byte hash");
    return HashNumber(BitwiseCast<uint32_t>(aVal));
  }

  static bool match(float aLhs, float aRhs)
  {
    return BitwiseCast<uint32_t>(aLhs) == BitwiseCast<uint32_t>(aRhs);
  }
};

// A hash policy that compares C strings.
struct CStringHasher
{
  using Lookup = const char*;

  static HashNumber hash(Lookup aLookup) { return HashString(aLookup); }

  static bool match(const char* key, Lookup lookup)
  {
    return strcmp(key, lookup) == 0;
  }
};

// Fallible hashing interface.
//
// Most of the time generating a hash code is infallible so this class provides
// default methods that always succeed.  Specialize this class for your own hash
// policy to provide fallible hashing.
//
// This is used by MovableCellHasher to handle the fact that generating a unique
// ID for cell pointer may fail due to OOM.
template<typename HashPolicy>
struct FallibleHashMethods
{
  // Return true if a hashcode is already available for its argument.  Once
  // this returns true for a specific argument it must continue to do so.
  template<typename Lookup>
  static bool hasHash(Lookup&& aLookup)
  {
    return true;
  }

  // Fallible method to ensure a hashcode exists for its argument and create
  // one if not.  Returns false on error, e.g. out of memory.
  template<typename Lookup>
  static bool ensureHash(Lookup&& aLookup)
  {
    return true;
  }
};

template<typename HashPolicy, typename Lookup>
static bool
HasHash(Lookup&& aLookup)
{
  return FallibleHashMethods<typename HashPolicy::Base>::hasHash(
    std::forward<Lookup>(aLookup));
}

template<typename HashPolicy, typename Lookup>
static bool
EnsureHash(Lookup&& aLookup)
{
  return FallibleHashMethods<typename HashPolicy::Base>::ensureHash(
    std::forward<Lookup>(aLookup));
}

/*****************************************************************************/

// Both HashMap and HashSet are implemented by a single HashTable that is even
// more heavily parameterized than the other two. This leaves HashTable gnarly
// and extremely coupled to HashMap and HashSet; thus code should not use
// HashTable directly.

template<class Key, class Value>
class HashMapEntry
{
  Key key_;
  Value value_;

  template<class, class, class>
  friend class detail::HashTable;
  template<class>
  friend class detail::HashTableEntry;
  template<class, class, class, class>
  friend class HashMap;

public:
  template<typename KeyInput, typename ValueInput>
  HashMapEntry(KeyInput&& aKey, ValueInput&& aValue)
    : key_(std::forward<KeyInput>(aKey))
    , value_(std::forward<ValueInput>(aValue))
  {
  }

  HashMapEntry(HashMapEntry&& aRhs)
    : key_(std::move(aRhs.key_))
    , value_(std::move(aRhs.value_))
  {
  }

  void operator=(HashMapEntry&& aRhs)
  {
    key_ = std::move(aRhs.key_);
    value_ = std::move(aRhs.value_);
  }

  using KeyType = Key;
  using ValueType = Value;

  const Key& key() const { return key_; }
  Key& mutableKey() { return key_; }

  const Value& value() const { return value_; }
  Value& value() { return value_; }

private:
  HashMapEntry(const HashMapEntry&) = delete;
  void operator=(const HashMapEntry&) = delete;
};

template<typename K, typename V>
struct IsPod<HashMapEntry<K, V>>
  : IntegralConstant<bool, IsPod<K>::value && IsPod<V>::value>
{
};

namespace detail {

template<class T, class HashPolicy, class AllocPolicy>
class HashTable;

template<typename T>
class HashTableEntry
{
private:
  using NonConstT = typename RemoveConst<T>::Type;

  static const HashNumber sFreeKey = 0;
  static const HashNumber sRemovedKey = 1;
  static const HashNumber sCollisionBit = 1;

  HashNumber mKeyHash = sFreeKey;
  alignas(NonConstT) unsigned char mValueData[sizeof(NonConstT)];

private:
  template<class, class, class>
  friend class HashTable;

  // Some versions of GCC treat it as a -Wstrict-aliasing violation (ergo a
  // -Werror compile error) to reinterpret_cast<> |mValueData| to |T*|, even
  // through |void*|.  Placing the latter cast in these separate functions
  // breaks the chain such that affected GCC versions no longer warn/error.
  void* rawValuePtr() { return mValueData; }

  static bool isLiveHash(HashNumber hash) { return hash > sRemovedKey; }

  HashTableEntry(const HashTableEntry&) = delete;
  void operator=(const HashTableEntry&) = delete;

  NonConstT* valuePtr() { return reinterpret_cast<NonConstT*>(rawValuePtr()); }

  void destroyStoredT()
  {
    NonConstT* ptr = valuePtr();
    ptr->~T();
    MOZ_MAKE_MEM_UNDEFINED(ptr, sizeof(*ptr));
  }

public:
  HashTableEntry() = default;

  ~HashTableEntry()
  {
    if (isLive()) {
      destroyStoredT();
    }

    MOZ_MAKE_MEM_UNDEFINED(this, sizeof(*this));
  }

  void destroy()
  {
    MOZ_ASSERT(isLive());
    destroyStoredT();
  }

  void swap(HashTableEntry* aOther)
  {
    if (this == aOther) {
      return;
    }
    MOZ_ASSERT(isLive());
    if (aOther->isLive()) {
      Swap(*valuePtr(), *aOther->valuePtr());
    } else {
      *aOther->valuePtr() = std::move(*valuePtr());
      destroy();
    }
    Swap(mKeyHash, aOther->mKeyHash);
  }

  T& get()
  {
    MOZ_ASSERT(isLive());
    return *valuePtr();
  }

  NonConstT& getMutable()
  {
    MOZ_ASSERT(isLive());
    return *valuePtr();
  }

  bool isFree() const { return mKeyHash == sFreeKey; }

  void clearLive()
  {
    MOZ_ASSERT(isLive());
    mKeyHash = sFreeKey;
    destroyStoredT();
  }

  void clear()
  {
    if (isLive()) {
      destroyStoredT();
    }
    MOZ_MAKE_MEM_UNDEFINED(this, sizeof(*this));
    mKeyHash = sFreeKey;
  }

  bool isRemoved() const { return mKeyHash == sRemovedKey; }

  void removeLive()
  {
    MOZ_ASSERT(isLive());
    mKeyHash = sRemovedKey;
    destroyStoredT();
  }

  bool isLive() const { return isLiveHash(mKeyHash); }

  void setCollision()
  {
    MOZ_ASSERT(isLive());
    mKeyHash |= sCollisionBit;
  }

  void unsetCollision() { mKeyHash &= ~sCollisionBit; }

  bool hasCollision() const { return mKeyHash & sCollisionBit; }

  bool matchHash(HashNumber hn) { return (mKeyHash & ~sCollisionBit) == hn; }

  HashNumber getKeyHash() const { return mKeyHash & ~sCollisionBit; }

  template<typename... Args>
  void setLive(HashNumber aHashNumber, Args&&... aArgs)
  {
    MOZ_ASSERT(!isLive());
    mKeyHash = aHashNumber;
    new (valuePtr()) T(std::forward<Args>(aArgs)...);
    MOZ_ASSERT(isLive());
  }
};

template<class T, class HashPolicy, class AllocPolicy>
class HashTable : private AllocPolicy
{
  friend class mozilla::ReentrancyGuard;

  using NonConstT = typename RemoveConst<T>::Type;
  using Key = typename HashPolicy::KeyType;
  using Lookup = typename HashPolicy::Lookup;

public:
  using Entry = HashTableEntry<T>;

  // A nullable pointer to a hash table element. A Ptr |p| can be tested
  // either explicitly |if (p.found()) p->...| or using boolean conversion
  // |if (p) p->...|. Ptr objects must not be used after any mutating hash
  // table operations unless |generation()| is tested.
  class Ptr
  {
    friend class HashTable;

    Entry* mEntry;
#ifdef DEBUG
    const HashTable* mTable;
    Generation mGeneration;
#endif

  protected:
    Ptr(Entry& aEntry, const HashTable& aTable)
      : mEntry(&aEntry)
#ifdef DEBUG
      , mTable(&aTable)
      , mGeneration(aTable.generation())
#endif
    {
    }

  public:
    Ptr()
      : mEntry(nullptr)
#ifdef DEBUG
      , mTable(nullptr)
      , mGeneration(0)
#endif
    {
    }

    bool isValid() const { return !!mEntry; }

    bool found() const
    {
      if (!isValid()) {
        return false;
      }
#ifdef DEBUG
      MOZ_ASSERT(mGeneration == mTable->generation());
#endif
      return mEntry->isLive();
    }

    explicit operator bool() const { return found(); }

    bool operator==(const Ptr& aRhs) const
    {
      MOZ_ASSERT(found() && aRhs.found());
      return mEntry == aRhs.mEntry;
    }

    bool operator!=(const Ptr& aRhs) const
    {
#ifdef DEBUG
      MOZ_ASSERT(mGeneration == mTable->generation());
#endif
      return !(*this == aRhs);
    }

    T& operator*() const
    {
#ifdef DEBUG
      MOZ_ASSERT(found());
      MOZ_ASSERT(mGeneration == mTable->generation());
#endif
      return mEntry->get();
    }

    T* operator->() const
    {
#ifdef DEBUG
      MOZ_ASSERT(found());
      MOZ_ASSERT(mGeneration == mTable->generation());
#endif
      return &mEntry->get();
    }
  };

  // A Ptr that can be used to add a key after a failed lookup.
  class AddPtr : public Ptr
  {
    friend class HashTable;

    HashNumber mKeyHash;
#ifdef DEBUG
    uint64_t mMutationCount;
#endif

    AddPtr(Entry& aEntry, const HashTable& aTable, HashNumber aHashNumber)
      : Ptr(aEntry, aTable)
      , mKeyHash(aHashNumber)
#ifdef DEBUG
      , mMutationCount(aTable.mMutationCount)
#endif
    {
    }

  public:
    AddPtr()
      : mKeyHash(0)
    {
    }
  };

  // A hash table iterator that (mostly) doesn't allow table modifications.
  // As with Ptr/AddPtr, Iterator objects must not be used after any mutating
  // hash table operation unless the |generation()| is tested.
  class Iterator
  {
  protected:
    friend class HashTable;

    explicit Iterator(const HashTable& aTable)
      : mCur(aTable.mTable)
      , mEnd(aTable.mTable + aTable.capacity())
#ifdef DEBUG
      , mTable(aTable)
      , mMutationCount(aTable.mMutationCount)
      , mGeneration(aTable.generation())
      , mValidEntry(true)
#endif
    {
      while (mCur < mEnd && !mCur->isLive()) {
        ++mCur;
      }
    }

    Entry* mCur;
    Entry* mEnd;
#ifdef DEBUG
    const HashTable& mTable;
    uint64_t mMutationCount;
    Generation mGeneration;
    bool mValidEntry;
#endif

  public:
    bool done() const
    {
#ifdef DEBUG
      MOZ_ASSERT(mGeneration == mTable.generation());
      MOZ_ASSERT(mMutationCount == mTable.mMutationCount);
#endif
      return mCur == mEnd;
    }

    T& get() const
    {
      MOZ_ASSERT(!done());
#ifdef DEBUG
      MOZ_ASSERT(mValidEntry);
      MOZ_ASSERT(mGeneration == mTable.generation());
      MOZ_ASSERT(mMutationCount == mTable.mMutationCount);
#endif
      return mCur->get();
    }

    void next()
    {
      MOZ_ASSERT(!done());
#ifdef DEBUG
      MOZ_ASSERT(mGeneration == mTable.generation());
      MOZ_ASSERT(mMutationCount == mTable.mMutationCount);
#endif
      while (++mCur < mEnd && !mCur->isLive()) {
        continue;
      }
#ifdef DEBUG
      mValidEntry = true;
#endif
    }
  };

  // A hash table iterator that permits modification, removal and rekeying.
  // Since rehashing when elements were removed during enumeration would be
  // bad, it is postponed until the ModIterator is destructed. Since the
  // ModIterator's destructor touches the hash table, the user must ensure
  // that the hash table is still alive when the destructor runs.
  class ModIterator : public Iterator
  {
    friend class HashTable;

    HashTable& mTable;
    bool mRekeyed;
    bool mRemoved;

    // ModIterator is movable but not copyable.
    ModIterator(const ModIterator&) = delete;
    void operator=(const ModIterator&) = delete;

  protected:
    explicit ModIterator(HashTable& aTable)
      : Iterator(aTable)
      , mTable(aTable)
      , mRekeyed(false)
      , mRemoved(false)
    {
    }

  public:
    MOZ_IMPLICIT ModIterator(ModIterator&& aOther)
      : Iterator(aOther)
      , mTable(aOther.mTable)
      , mRekeyed(aOther.mRekeyed)
      , mRemoved(aOther.mRemoved)
    {
      aOther.mRekeyed = false;
      aOther.mRemoved = false;
    }

    // Removes the current element from the table, leaving |get()|
    // invalid until the next call to |next()|.
    void remove()
    {
      mTable.remove(*this->mCur);
      mRemoved = true;
#ifdef DEBUG
      this->mValidEntry = false;
      this->mMutationCount = mTable.mMutationCount;
#endif
    }

    NonConstT& getMutable()
    {
      MOZ_ASSERT(!this->done());
#ifdef DEBUG
      MOZ_ASSERT(this->mValidEntry);
      MOZ_ASSERT(this->mGeneration == this->Iterator::mTable.generation());
      MOZ_ASSERT(this->mMutationCount == this->Iterator::mTable.mMutationCount);
#endif
      return this->mCur->getMutable();
    }

    // Removes the current element and re-inserts it into the table with
    // a new key at the new Lookup position.  |get()| is invalid after
    // this operation until the next call to |next()|.
    void rekey(const Lookup& l, const Key& k)
    {
      MOZ_ASSERT(&k != &HashPolicy::getKey(this->mCur->get()));
      Ptr p(*this->mCur, mTable);
      mTable.rekeyWithoutRehash(p, l, k);
      mRekeyed = true;
#ifdef DEBUG
      this->mValidEntry = false;
      this->mMutationCount = mTable.mMutationCount;
#endif
    }

    void rekey(const Key& k) { rekey(k, k); }

    // Potentially rehashes the table.
    ~ModIterator()
    {
      if (mRekeyed) {
        mTable.mGen++;
        mTable.checkOverRemoved();
      }

      if (mRemoved) {
        mTable.compactIfUnderloaded();
      }
    }
  };

  // Range is similar to Iterator, but uses different terminology.
  class Range
  {
    friend class HashTable;

    Iterator mIter;

  protected:
    explicit Range(const HashTable& table)
      : mIter(table)
    {
    }

  public:
    bool empty() const { return mIter.done(); }

    T& front() const { return mIter.get(); }

    void popFront() { return mIter.next(); }
  };

  // Enum is similar to ModIterator, but uses different terminology.
  class Enum
  {
    ModIterator mIter;

    // Enum is movable but not copyable.
    Enum(const Enum&) = delete;
    void operator=(const Enum&) = delete;

  public:
    template<class Map>
    explicit Enum(Map& map)
      : mIter(map.mImpl)
    {
    }

    MOZ_IMPLICIT Enum(Enum&& other)
      : mIter(std::move(other.mIter))
    {
    }

    bool empty() const { return mIter.done(); }

    T& front() const { return mIter.get(); }

    void popFront() { return mIter.next(); }

    void removeFront() { mIter.remove(); }

    NonConstT& mutableFront() { return mIter.getMutable(); }

    void rekeyFront(const Lookup& aLookup, const Key& aKey)
    {
      mIter.rekey(aLookup, aKey);
    }

    void rekeyFront(const Key& aKey) { mIter.rekey(aKey); }
  };

  // HashTable is movable
  HashTable(HashTable&& aRhs)
    : AllocPolicy(aRhs)
  {
    PodAssign(this, &aRhs);
    aRhs.mTable = nullptr;
  }
  void operator=(HashTable&& aRhs)
  {
    MOZ_ASSERT(this != &aRhs, "self-move assignment is prohibited");
    if (mTable) {
      destroyTable(*this, mTable, capacity());
    }
    PodAssign(this, &aRhs);
    aRhs.mTable = nullptr;
  }

private:
  // HashTable is not copyable or assignable
  HashTable(const HashTable&) = delete;
  void operator=(const HashTable&) = delete;

  static const size_t CAP_BITS = 30;

public:
  uint64_t mGen : 56;      // entry storage generation number
  uint64_t mHashShift : 8; // multiplicative hash shift
  Entry* mTable;           // entry storage
  uint32_t mEntryCount;    // number of entries in mTable
  uint32_t mRemovedCount;  // removed entry sentinels in mTable

#ifdef DEBUG
  uint64_t mMutationCount;
  mutable bool mEntered;

  // Note that some updates to these stats are not thread-safe. See the
  // comment on the three-argument overloading of HashTable::lookup().
  mutable struct Stats
  {
    uint32_t mSearches;       // total number of table searches
    uint32_t mSteps;          // hash chain links traversed
    uint32_t mHits;           // searches that found key
    uint32_t mMisses;         // searches that didn't find key
    uint32_t mAddOverRemoved; // adds that recycled a removed entry
    uint32_t mRemoves;        // calls to remove
    uint32_t mRemoveFrees;    // calls to remove that freed the entry
    uint32_t mGrows;          // table expansions
    uint32_t mShrinks;        // table contractions
    uint32_t mCompresses;     // table compressions
    uint32_t mRehashes;       // tombstone decontaminations
  } mStats;
#define METER(x) x
#else
#define METER(x)
#endif

  // The default initial capacity is 32 (enough to hold 16 elements), but it
  // can be as low as 4.
  static const uint32_t sMinCapacity = 4;
  static const uint32_t sMaxInit = 1u << (CAP_BITS - 1);
  static const uint32_t sMaxCapacity = 1u << CAP_BITS;

  // Hash-table alpha is conceptually a fraction, but to avoid floating-point
  // math we implement it as a ratio of integers.
  static const uint8_t sAlphaDenominator = 4;
  static const uint8_t sMinAlphaNumerator = 1; // min alpha: 1/4
  static const uint8_t sMaxAlphaNumerator = 3; // max alpha: 3/4

  static const HashNumber sFreeKey = Entry::sFreeKey;
  static const HashNumber sRemovedKey = Entry::sRemovedKey;
  static const HashNumber sCollisionBit = Entry::sCollisionBit;

  void setTableSizeLog2(uint32_t aSizeLog2)
  {
    mHashShift = kHashNumberBits - aSizeLog2;
  }

  static bool isLiveHash(HashNumber aHash) { return Entry::isLiveHash(aHash); }

  static HashNumber prepareHash(const Lookup& aLookup)
  {
    HashNumber keyHash = ScrambleHashCode(HashPolicy::hash(aLookup));

    // Avoid reserved hash codes.
    if (!isLiveHash(keyHash)) {
      keyHash -= (sRemovedKey + 1);
    }
    return keyHash & ~sCollisionBit;
  }

  enum FailureBehavior
  {
    DontReportFailure = false,
    ReportFailure = true
  };

  static Entry* createTable(AllocPolicy& aAllocPolicy,
                            uint32_t aCapacity,
                            FailureBehavior aReportFailure = ReportFailure)
  {
    Entry* table = aReportFailure
                     ? aAllocPolicy.template pod_malloc<Entry>(aCapacity)
                     : aAllocPolicy.template maybe_pod_malloc<Entry>(aCapacity);
    if (table) {
      for (uint32_t i = 0; i < aCapacity; i++) {
        new (&table[i]) Entry();
      }
    }
    return table;
  }

  static Entry* maybeCreateTable(AllocPolicy& aAllocPolicy, uint32_t aCapacity)
  {
    Entry* table = aAllocPolicy.template maybe_pod_malloc<Entry>(aCapacity);
    if (table) {
      for (uint32_t i = 0; i < aCapacity; i++) {
        new (&table[i]) Entry();
      }
    }
    return table;
  }

  static void destroyTable(AllocPolicy& aAllocPolicy,
                           Entry* aOldTable,
                           uint32_t aCapacity)
  {
    Entry* end = aOldTable + aCapacity;
    for (Entry* e = aOldTable; e < end; ++e) {
      e->~Entry();
    }
    aAllocPolicy.free_(aOldTable, aCapacity);
  }

public:
  explicit HashTable(AllocPolicy aAllocPolicy)
    : AllocPolicy(aAllocPolicy)
    , mGen(0)
    , mHashShift(kHashNumberBits)
    , mTable(nullptr)
    , mEntryCount(0)
    , mRemovedCount(0)
#ifdef DEBUG
    , mMutationCount(0)
    , mEntered(false)
#endif
  {
  }

  MOZ_MUST_USE bool init(uint32_t aLen)
  {
    MOZ_ASSERT(!initialized());

    // Reject all lengths whose initial computed capacity would exceed
    // sMaxCapacity. Round that maximum aLen down to the nearest power of two
    // for speedier code.
    if (MOZ_UNLIKELY(aLen > sMaxInit)) {
      this->reportAllocOverflow();
      return false;
    }

    static_assert((sMaxInit * sAlphaDenominator) / sAlphaDenominator ==
                    sMaxInit,
                  "multiplication in numerator below could overflow");
    static_assert(sMaxInit * sAlphaDenominator <=
                    UINT32_MAX - sMaxAlphaNumerator,
                  "numerator calculation below could potentially overflow");

    // Compute the smallest capacity allowing |aLen| elements to be
    // inserted without rehashing: ceil(aLen / max-alpha).  (Ceiling
    // integral division: <http://stackoverflow.com/a/2745086>.)
    uint32_t newCapacity =
      (aLen * sAlphaDenominator + sMaxAlphaNumerator - 1) / sMaxAlphaNumerator;
    if (newCapacity < sMinCapacity) {
      newCapacity = sMinCapacity;
    }

    // Round up capacity to next power-of-two.
    uint32_t log2 = mozilla::CeilingLog2(newCapacity);
    newCapacity = 1u << log2;

    MOZ_ASSERT(newCapacity >= aLen);
    MOZ_ASSERT(newCapacity <= sMaxCapacity);

    mTable = createTable(*this, newCapacity);
    if (!mTable) {
      return false;
    }
    setTableSizeLog2(log2);
    METER(memset(&mStats, 0, sizeof(mStats)));
    return true;
  }

  bool initialized() const { return !!mTable; }

  ~HashTable()
  {
    if (mTable) {
      destroyTable(*this, mTable, capacity());
    }
  }

private:
  HashNumber hash1(HashNumber aHash0) const { return aHash0 >> mHashShift; }

  struct DoubleHash
  {
    HashNumber mHash2;
    HashNumber mSizeMask;
  };

  DoubleHash hash2(HashNumber aCurKeyHash) const
  {
    uint32_t sizeLog2 = kHashNumberBits - mHashShift;
    DoubleHash dh = { ((aCurKeyHash << sizeLog2) >> mHashShift) | 1,
                      (HashNumber(1) << sizeLog2) - 1 };
    return dh;
  }

  static HashNumber applyDoubleHash(HashNumber aHash1,
                                    const DoubleHash& aDoubleHash)
  {
    return (aHash1 - aDoubleHash.mHash2) & aDoubleHash.mSizeMask;
  }

  bool overloaded()
  {
    static_assert(sMaxCapacity <= UINT32_MAX / sMaxAlphaNumerator,
                  "multiplication below could overflow");
    return mEntryCount + mRemovedCount >=
           capacity() * sMaxAlphaNumerator / sAlphaDenominator;
  }

  // Would the table be underloaded if it had the given capacity and entryCount?
  static bool wouldBeUnderloaded(uint32_t aCapacity, uint32_t aEntryCount)
  {
    static_assert(sMaxCapacity <= UINT32_MAX / sMinAlphaNumerator,
                  "multiplication below could overflow");
    return aCapacity > sMinCapacity &&
           aEntryCount <= aCapacity * sMinAlphaNumerator / sAlphaDenominator;
  }

  bool underloaded() { return wouldBeUnderloaded(capacity(), mEntryCount); }

  static MOZ_ALWAYS_INLINE bool match(Entry& aEntry, const Lookup& aLookup)
  {
    return HashPolicy::match(HashPolicy::getKey(aEntry.get()), aLookup);
  }

  // Warning: in order for readonlyThreadsafeLookup() to be safe this
  // function must not modify the table in any way when |collisionBit| is 0.
  // (The use of the METER() macro to increment stats violates this
  // restriction but we will live with that for now because it's enabled so
  // rarely.)
  MOZ_ALWAYS_INLINE Entry& lookup(const Lookup& aLookup,
                                  HashNumber aKeyHash,
                                  uint32_t aCollisionBit) const
  {
    MOZ_ASSERT(isLiveHash(aKeyHash));
    MOZ_ASSERT(!(aKeyHash & sCollisionBit));
    MOZ_ASSERT(aCollisionBit == 0 || aCollisionBit == sCollisionBit);
    MOZ_ASSERT(mTable);
    METER(mStats.mSearches++);

    // Compute the primary hash address.
    HashNumber h1 = hash1(aKeyHash);
    Entry* entry = &mTable[h1];

    // Miss: return space for a new entry.
    if (entry->isFree()) {
      METER(mStats.mMisses++);
      return *entry;
    }

    // Hit: return entry.
    if (entry->matchHash(aKeyHash) && match(*entry, aLookup)) {
      METER(mStats.mHits++);
      return *entry;
    }

    // Collision: double hash.
    DoubleHash dh = hash2(aKeyHash);

    // Save the first removed entry pointer so we can recycle later.
    Entry* firstRemoved = nullptr;

    while (true) {
      if (MOZ_UNLIKELY(entry->isRemoved())) {
        if (!firstRemoved) {
          firstRemoved = entry;
        }
      } else {
        if (aCollisionBit == sCollisionBit) {
          entry->setCollision();
        }
      }

      METER(mStats.mSteps++);
      h1 = applyDoubleHash(h1, dh);

      entry = &mTable[h1];
      if (entry->isFree()) {
        METER(mStats.mMisses++);
        return firstRemoved ? *firstRemoved : *entry;
      }

      if (entry->matchHash(aKeyHash) && match(*entry, aLookup)) {
        METER(mStats.mHits++);
        return *entry;
      }
    }
  }

  // This is a copy of lookup hardcoded to the assumptions:
  //   1. the lookup is a lookupForAdd
  //   2. the key, whose |keyHash| has been passed is not in the table,
  //   3. no entries have been removed from the table.
  // This specialized search avoids the need for recovering lookup values
  // from entries, which allows more flexible Lookup/Key types.
  Entry& findFreeEntry(HashNumber aKeyHash)
  {
    MOZ_ASSERT(!(aKeyHash & sCollisionBit));
    MOZ_ASSERT(mTable);
    METER(mStats.mSearches++);

    // We assume 'aKeyHash' has already been distributed.

    // Compute the primary hash address.
    HashNumber h1 = hash1(aKeyHash);
    Entry* entry = &mTable[h1];

    // Miss: return space for a new entry.
    if (!entry->isLive()) {
      METER(mStats.mMisses++);
      return *entry;
    }

    // Collision: double hash.
    DoubleHash dh = hash2(aKeyHash);

    while (true) {
      MOZ_ASSERT(!entry->isRemoved());
      entry->setCollision();

      METER(mStats.mSteps++);
      h1 = applyDoubleHash(h1, dh);

      entry = &mTable[h1];
      if (!entry->isLive()) {
        METER(mStats.mMisses++);
        return *entry;
      }
    }
  }

  enum RebuildStatus
  {
    NotOverloaded,
    Rehashed,
    RehashFailed
  };

  RebuildStatus changeTableSize(int aDeltaLog2,
                                FailureBehavior aReportFailure = ReportFailure)
  {
    // Look, but don't touch, until we succeed in getting new entry store.
    Entry* oldTable = mTable;
    uint32_t oldCap = capacity();
    uint32_t newLog2 = kHashNumberBits - mHashShift + aDeltaLog2;
    uint32_t newCapacity = 1u << newLog2;
    if (MOZ_UNLIKELY(newCapacity > sMaxCapacity)) {
      if (aReportFailure) {
        this->reportAllocOverflow();
      }
      return RehashFailed;
    }

    Entry* newTable = createTable(*this, newCapacity, aReportFailure);
    if (!newTable) {
      return RehashFailed;
    }

    // We can't fail from here on, so update table parameters.
    setTableSizeLog2(newLog2);
    mRemovedCount = 0;
    mGen++;
    mTable = newTable;

    // Copy only live entries, leaving removed ones behind.
    Entry* end = oldTable + oldCap;
    for (Entry* src = oldTable; src < end; ++src) {
      if (src->isLive()) {
        HashNumber hn = src->getKeyHash();
        findFreeEntry(hn).setLive(
          hn, std::move(const_cast<typename Entry::NonConstT&>(src->get())));
      }

      src->~Entry();
    }

    // All entries have been destroyed, no need to destroyTable.
    this->free_(oldTable, oldCap);
    return Rehashed;
  }

  bool shouldCompressTable()
  {
    // Compress if a quarter or more of all entries are removed.
    return mRemovedCount >= (capacity() >> 2);
  }

  RebuildStatus checkOverloaded(FailureBehavior aReportFailure = ReportFailure)
  {
    if (!overloaded()) {
      return NotOverloaded;
    }

    int deltaLog2;
    if (shouldCompressTable()) {
      METER(mStats.mCompresses++);
      deltaLog2 = 0;
    } else {
      METER(mStats.mGrows++);
      deltaLog2 = 1;
    }

    return changeTableSize(deltaLog2, aReportFailure);
  }

  // Infallibly rehash the table if we are overloaded with removals.
  void checkOverRemoved()
  {
    if (overloaded()) {
      if (checkOverloaded(DontReportFailure) == RehashFailed) {
        rehashTableInPlace();
      }
    }
  }

  void remove(Entry& aEntry)
  {
    MOZ_ASSERT(mTable);
    METER(mStats.mRemoves++);

    if (aEntry.hasCollision()) {
      aEntry.removeLive();
      mRemovedCount++;
    } else {
      METER(mStats.mRemoveFrees++);
      aEntry.clearLive();
    }
    mEntryCount--;
#ifdef DEBUG
    mMutationCount++;
#endif
  }

  void checkUnderloaded()
  {
    if (underloaded()) {
      METER(mStats.mShrinks++);
      (void)changeTableSize(-1, DontReportFailure);
    }
  }

  // Resize the table down to the largest capacity which doesn't underload the
  // table.  Since we call checkUnderloaded() on every remove, you only need
  // to call this after a bulk removal of items done without calling remove().
  void compactIfUnderloaded()
  {
    int32_t resizeLog2 = 0;
    uint32_t newCapacity = capacity();
    while (wouldBeUnderloaded(newCapacity, mEntryCount)) {
      newCapacity = newCapacity >> 1;
      resizeLog2--;
    }

    if (resizeLog2 != 0) {
      (void)changeTableSize(resizeLog2, DontReportFailure);
    }
  }

  // This is identical to changeTableSize(currentSize), but without requiring
  // a second table.  We do this by recycling the collision bits to tell us if
  // the element is already inserted or still waiting to be inserted.  Since
  // already-inserted elements win any conflicts, we get the same table as we
  // would have gotten through random insertion order.
  void rehashTableInPlace()
  {
    METER(mStats.mRehashes++);
    mRemovedCount = 0;
    mGen++;
    for (size_t i = 0; i < capacity(); ++i) {
      mTable[i].unsetCollision();
    }
    for (size_t i = 0; i < capacity();) {
      Entry* src = &mTable[i];

      if (!src->isLive() || src->hasCollision()) {
        ++i;
        continue;
      }

      HashNumber keyHash = src->getKeyHash();
      HashNumber h1 = hash1(keyHash);
      DoubleHash dh = hash2(keyHash);
      Entry* tgt = &mTable[h1];
      while (true) {
        if (!tgt->hasCollision()) {
          src->swap(tgt);
          tgt->setCollision();
          break;
        }

        h1 = applyDoubleHash(h1, dh);
        tgt = &mTable[h1];
      }
    }

    // TODO: this algorithm leaves collision bits on *all* elements, even if
    // they are on no collision path. We have the option of setting the
    // collision bits correctly on a subsequent pass or skipping the rehash
    // unless we are totally filled with tombstones: benchmark to find out
    // which approach is best.
  }

  // Note: |aLookup| may be a reference to a piece of |u|, so this function
  // must take care not to use |aLookup| after moving |u|.
  //
  // Prefer to use putNewInfallible; this function does not check
  // invariants.
  template<typename... Args>
  void putNewInfallibleInternal(const Lookup& aLookup, Args&&... aArgs)
  {
    MOZ_ASSERT(mTable);

    HashNumber keyHash = prepareHash(aLookup);
    Entry* entry = &findFreeEntry(keyHash);
    MOZ_ASSERT(entry);

    if (entry->isRemoved()) {
      METER(mStats.mAddOverRemoved++);
      mRemovedCount--;
      keyHash |= sCollisionBit;
    }

    entry->setLive(keyHash, std::forward<Args>(aArgs)...);
    mEntryCount++;
#ifdef DEBUG
    mMutationCount++;
#endif
  }

public:
  void clear()
  {
    Entry* end = mTable + capacity();
    for (Entry* e = mTable; e < end; ++e) {
      e->clear();
    }
    mRemovedCount = 0;
    mEntryCount = 0;
#ifdef DEBUG
    mMutationCount++;
#endif
  }

  void clearAndShrink()
  {
    clear();
    compactIfUnderloaded();
  }

  void finish()
  {
#ifdef DEBUG
    MOZ_ASSERT(!mEntered);
#endif
    if (!mTable) {
      return;
    }

    destroyTable(*this, mTable, capacity());
    mTable = nullptr;
    mGen++;
    mEntryCount = 0;
    mRemovedCount = 0;
#ifdef DEBUG
    mMutationCount++;
#endif
  }

  Iterator iter() const
  {
    MOZ_ASSERT(mTable);
    return Iterator(*this);
  }

  ModIterator modIter()
  {
    MOZ_ASSERT(mTable);
    return ModIterator(*this);
  }

  Range all() const
  {
    MOZ_ASSERT(mTable);
    return Range(*this);
  }

  bool empty() const
  {
    MOZ_ASSERT(mTable);
    return !mEntryCount;
  }

  uint32_t count() const
  {
    MOZ_ASSERT(mTable);
    return mEntryCount;
  }

  uint32_t capacity() const
  {
    MOZ_ASSERT(mTable);
    return 1u << (kHashNumberBits - mHashShift);
  }

  Generation generation() const
  {
    MOZ_ASSERT(mTable);
    return Generation(mGen);
  }

  size_t shallowSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(mTable);
  }

  size_t shallowSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + shallowSizeOfExcludingThis(aMallocSizeOf);
  }

  MOZ_ALWAYS_INLINE Ptr lookup(const Lookup& aLookup) const
  {
    ReentrancyGuard g(*this);
    if (!HasHash<HashPolicy>(aLookup)) {
      return Ptr();
    }
    HashNumber keyHash = prepareHash(aLookup);
    return Ptr(lookup(aLookup, keyHash, 0), *this);
  }

  MOZ_ALWAYS_INLINE Ptr readonlyThreadsafeLookup(const Lookup& aLookup) const
  {
    if (!HasHash<HashPolicy>(aLookup)) {
      return Ptr();
    }
    HashNumber keyHash = prepareHash(aLookup);
    return Ptr(lookup(aLookup, keyHash, 0), *this);
  }

  MOZ_ALWAYS_INLINE AddPtr lookupForAdd(const Lookup& aLookup) const
  {
    ReentrancyGuard g(*this);
    if (!EnsureHash<HashPolicy>(aLookup)) {
      return AddPtr();
    }
    HashNumber keyHash = prepareHash(aLookup);
    // Directly call the constructor in the return statement to avoid
    // excess copying when building with Visual Studio 2017.
    // See bug 1385181.
    return AddPtr(lookup(aLookup, keyHash, sCollisionBit), *this, keyHash);
  }

  template<typename... Args>
  MOZ_MUST_USE bool add(AddPtr& aPtr, Args&&... aArgs)
  {
    ReentrancyGuard g(*this);
    MOZ_ASSERT(mTable);
    MOZ_ASSERT_IF(aPtr.isValid(), aPtr.mTable == this);
    MOZ_ASSERT(!aPtr.found());
    MOZ_ASSERT(!(aPtr.mKeyHash & sCollisionBit));

    // Check for error from ensureHash() here.
    if (!aPtr.isValid()) {
      return false;
    }

    MOZ_ASSERT(aPtr.mGeneration == generation());
#ifdef DEBUG
    MOZ_ASSERT(aPtr.mMutationCount == mMutationCount);
#endif

    // Changing an entry from removed to live does not affect whether we
    // are overloaded and can be handled separately.
    if (aPtr.mEntry->isRemoved()) {
      if (!this->checkSimulatedOOM()) {
        return false;
      }
      METER(mStats.mAddOverRemoved++);
      mRemovedCount--;
      aPtr.mKeyHash |= sCollisionBit;
    } else {
      // Preserve the validity of |aPtr.mEntry|.
      RebuildStatus status = checkOverloaded();
      if (status == RehashFailed) {
        return false;
      }
      if (status == NotOverloaded && !this->checkSimulatedOOM()) {
        return false;
      }
      if (status == Rehashed) {
        aPtr.mEntry = &findFreeEntry(aPtr.mKeyHash);
      }
    }

    aPtr.mEntry->setLive(aPtr.mKeyHash, std::forward<Args>(aArgs)...);
    mEntryCount++;
#ifdef DEBUG
    mMutationCount++;
    aPtr.mGeneration = generation();
    aPtr.mMutationCount = mMutationCount;
#endif
    return true;
  }

  // Note: |aLookup| may be a reference to a piece of |u|, so this function
  // must take care not to use |aLookup| after moving |u|.
  template<typename... Args>
  void putNewInfallible(const Lookup& aLookup, Args&&... aArgs)
  {
    MOZ_ASSERT(!lookup(aLookup).found());
    ReentrancyGuard g(*this);
    putNewInfallibleInternal(aLookup, std::forward<Args>(aArgs)...);
  }

  // Note: |aLookup| may be alias arguments in |aArgs|, so this function must
  // take care not to use |aLookup| after moving |aArgs|.
  template<typename... Args>
  MOZ_MUST_USE bool putNew(const Lookup& aLookup, Args&&... aArgs)
  {
    if (!this->checkSimulatedOOM()) {
      return false;
    }
    if (!EnsureHash<HashPolicy>(aLookup)) {
      return false;
    }
    if (checkOverloaded() == RehashFailed) {
      return false;
    }
    putNewInfallible(aLookup, std::forward<Args>(aArgs)...);
    return true;
  }

  // Note: |aLookup| may be a reference to a piece of |u|, so this function
  // must take care not to use |aLookup| after moving |u|.
  template<typename... Args>
  MOZ_MUST_USE bool relookupOrAdd(AddPtr& aPtr,
                                  const Lookup& aLookup,
                                  Args&&... aArgs)
  {
    // Check for error from ensureHash() here.
    if (!aPtr.isValid()) {
      return false;
    }
#ifdef DEBUG
    aPtr.mGeneration = generation();
    aPtr.mMutationCount = mMutationCount;
#endif
    {
      ReentrancyGuard g(*this);
      // Check that aLookup has not been destroyed.
      MOZ_ASSERT(prepareHash(aLookup) == aPtr.mKeyHash);
      aPtr.mEntry = &lookup(aLookup, aPtr.mKeyHash, sCollisionBit);
    }
    return aPtr.found() || add(aPtr, std::forward<Args>(aArgs)...);
  }

  void remove(Ptr aPtr)
  {
    MOZ_ASSERT(mTable);
    ReentrancyGuard g(*this);
    MOZ_ASSERT(aPtr.found());
    MOZ_ASSERT(aPtr.mGeneration == generation());
    remove(*aPtr.mEntry);
    checkUnderloaded();
  }

  void rekeyWithoutRehash(Ptr aPtr, const Lookup& aLookup, const Key& aKey)
  {
    MOZ_ASSERT(mTable);
    ReentrancyGuard g(*this);
    MOZ_ASSERT(aPtr.found());
    MOZ_ASSERT(aPtr.mGeneration == generation());
    typename HashTableEntry<T>::NonConstT t(std::move(*aPtr));
    HashPolicy::setKey(t, const_cast<Key&>(aKey));
    remove(*aPtr.mEntry);
    putNewInfallibleInternal(aLookup, std::move(t));
  }

  void rekeyAndMaybeRehash(Ptr aPtr, const Lookup& aLookup, const Key& aKey)
  {
    rekeyWithoutRehash(aPtr, aLookup, aKey);
    checkOverRemoved();
  }

#undef METER
};

} // namespace detail
} // namespace mozilla

#endif /* mozilla_HashTable_h */
