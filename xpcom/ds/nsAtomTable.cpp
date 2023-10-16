/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/MruCache.h"
#include "mozilla/Mutex.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Unused.h"

#include "nsAtom.h"
#include "nsAtomTable.h"
#include "nsCRT.h"
#include "nsGkAtoms.h"
#include "nsHashKeys.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"
#include "PLDHashTable.h"
#include "prenv.h"

// There are two kinds of atoms handled by this module.
//
// - Dynamic: the atom itself is heap allocated, as is the char buffer it
//   points to. |gAtomTable| holds weak references to dynamic atoms. When the
//   refcount of a dynamic atom drops to zero, we increment a static counter.
//   When that counter reaches a certain threshold, we iterate over the atom
//   table, removing and deleting dynamic atoms with refcount zero. This allows
//   us to avoid acquiring the atom table lock during normal refcounting.
//
// - Static: both the atom and its chars are statically allocated and
//   immutable, so it ignores all AddRef/Release calls.
//
// Note that gAtomTable is used on multiple threads, and has internal
// synchronization.

using namespace mozilla;

//----------------------------------------------------------------------

enum class GCKind {
  RegularOperation,
  Shutdown,
};

//----------------------------------------------------------------------

// gUnusedAtomCount is incremented when an atom loses its last reference
// (and thus turned into unused state), and decremented when an unused
// atom gets a reference again. The atom table relies on this value to
// schedule GC. This value can temporarily go below zero when multiple
// threads are operating the same atom, so it has to be signed so that
// we wouldn't use overflow value for comparison.
// See nsAtom::AddRef() and nsAtom::Release().
// This atomic can be accessed during the GC and other places where recorded
// events are not allowed, so its value is not preserved when recording or
// replaying.
Atomic<int32_t, ReleaseAcquire> nsDynamicAtom::gUnusedAtomCount;

nsDynamicAtom::nsDynamicAtom(const nsAString& aString, uint32_t aHash,
                             bool aIsAsciiLowercase)
    : nsAtom(aString, aHash, aIsAsciiLowercase), mRefCnt(1) {}

// Returns true if ToLowercaseASCII would return the string unchanged.
static bool IsAsciiLowercase(const char16_t* aString, const uint32_t aLength) {
  for (uint32_t i = 0; i < aLength; ++i) {
    if (IS_ASCII_UPPER(aString[i])) {
      return false;
    }
  }

  return true;
}

nsDynamicAtom* nsDynamicAtom::Create(const nsAString& aString, uint32_t aHash) {
  // We tack the chars onto the end of the nsDynamicAtom object.
  size_t numCharBytes = (aString.Length() + 1) * sizeof(char16_t);
  size_t numTotalBytes = sizeof(nsDynamicAtom) + numCharBytes;

  bool isAsciiLower = ::IsAsciiLowercase(aString.Data(), aString.Length());

  nsDynamicAtom* atom = (nsDynamicAtom*)moz_xmalloc(numTotalBytes);
  new (atom) nsDynamicAtom(aString, aHash, isAsciiLower);
  memcpy(const_cast<char16_t*>(atom->String()),
         PromiseFlatString(aString).get(), numCharBytes);

  MOZ_ASSERT(atom->String()[atom->GetLength()] == char16_t(0));
  MOZ_ASSERT(atom->Equals(aString));
  MOZ_ASSERT(atom->mHash == HashString(atom->String(), atom->GetLength()));
  MOZ_ASSERT(atom->mIsAsciiLowercase == isAsciiLower);

  return atom;
}

void nsDynamicAtom::Destroy(nsDynamicAtom* aAtom) {
  aAtom->~nsDynamicAtom();
  free(aAtom);
}

void nsAtom::ToString(nsAString& aString) const {
  // See the comment on |mString|'s declaration.
  if (IsStatic()) {
    // AssignLiteral() lets us assign without copying. This isn't a string
    // literal, but it's a static atom and thus has an unbounded lifetime,
    // which is what's important.
    aString.AssignLiteral(AsStatic()->String(), mLength);
  } else {
    aString.Assign(AsDynamic()->String(), mLength);
  }
}

void nsAtom::ToUTF8String(nsACString& aBuf) const {
  CopyUTF16toUTF8(nsDependentString(GetUTF16String(), mLength), aBuf);
}

void nsAtom::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                    AtomsSizes& aSizes) const {
  // Static atoms are in static memory, and so are not measured here.
  if (IsDynamic()) {
    aSizes.mDynamicAtoms += aMallocSizeOf(this);
  }
}

char16ptr_t nsAtom::GetUTF16String() const {
  return IsStatic() ? AsStatic()->String() : AsDynamic()->String();
}

//----------------------------------------------------------------------

struct AtomTableKey {
  explicit AtomTableKey(const nsStaticAtom* aAtom)
      : mUTF16String(aAtom->String()),
        mUTF8String(nullptr),
        mLength(aAtom->GetLength()),
        mHash(aAtom->hash()) {
    MOZ_ASSERT(HashString(mUTF16String, mLength) == mHash);
  }

  AtomTableKey(const char16_t* aUTF16String, uint32_t aLength)
      : mUTF16String(aUTF16String), mUTF8String(nullptr), mLength(aLength) {
    mHash = HashString(mUTF16String, mLength);
  }

  AtomTableKey(const char* aUTF8String, uint32_t aLength, bool* aErr)
      : mUTF16String(nullptr), mUTF8String(aUTF8String), mLength(aLength) {
    mHash = HashUTF8AsUTF16(mUTF8String, mLength, aErr);
  }

  const char16_t* mUTF16String;
  const char* mUTF8String;
  uint32_t mLength;
  uint32_t mHash;
};

struct AtomTableEntry : public PLDHashEntryHdr {
  // These references are either to dynamic atoms, in which case they are
  // non-owning, or they are to static atoms, which aren't really refcounted.
  // See the comment at the top of this file for more details.
  nsAtom* MOZ_NON_OWNING_REF mAtom;
};

struct AtomCache : public MruCache<AtomTableKey, nsAtom*, AtomCache> {
  static HashNumber Hash(const AtomTableKey& aKey) { return aKey.mHash; }
  static bool Match(const AtomTableKey& aKey, const nsAtom* aVal) {
    MOZ_ASSERT(aKey.mUTF16String);
    return aVal->Equals(aKey.mUTF16String, aKey.mLength);
  }
};

static AtomCache sRecentlyUsedMainThreadAtoms;

// In order to reduce locking contention for concurrent atomization, we segment
// the atom table into N subtables, each with a separate lock. If the hash
// values we use to select the subtable are evenly distributed, this reduces the
// probability of contention by a factor of N. See bug 1440824.
//
// NB: This is somewhat similar to the technique used by Java's
// ConcurrentHashTable.
class nsAtomSubTable {
  friend class nsAtomTable;
  Mutex mLock;
  PLDHashTable mTable;
  nsAtomSubTable();
  void GCLocked(GCKind aKind) MOZ_REQUIRES(mLock);
  void AddSizeOfExcludingThisLocked(MallocSizeOf aMallocSizeOf,
                                    AtomsSizes& aSizes) MOZ_REQUIRES(mLock);

  AtomTableEntry* Search(AtomTableKey& aKey) const MOZ_REQUIRES(mLock) {
    mLock.AssertCurrentThreadOwns();
    return static_cast<AtomTableEntry*>(mTable.Search(&aKey));
  }

  AtomTableEntry* Add(AtomTableKey& aKey) MOZ_REQUIRES(mLock) {
    mLock.AssertCurrentThreadOwns();
    return static_cast<AtomTableEntry*>(mTable.Add(&aKey));  // Infallible
  }
};

// The outer atom table, which coordinates access to the inner array of
// subtables.
class nsAtomTable {
 public:
  nsAtomSubTable& SelectSubTable(AtomTableKey& aKey);
  void AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf, AtomsSizes& aSizes);
  void GC(GCKind aKind);
  already_AddRefed<nsAtom> Atomize(const nsAString& aUTF16String);
  already_AddRefed<nsAtom> Atomize(const nsACString& aUTF8String);
  already_AddRefed<nsAtom> AtomizeMainThread(const nsAString& aUTF16String);
  nsStaticAtom* GetStaticAtom(const nsAString& aUTF16String);
  void RegisterStaticAtoms(const nsStaticAtom* aAtoms, size_t aAtomsLen);

  // The result of this function may be imprecise if other threads are operating
  // on atoms concurrently. It's also slow, since it triggers a GC before
  // counting.
  size_t RacySlowCount();

  // This hash table op is a static member of this class so that it can take
  // advantage of |friend| declarations.
  static void AtomTableClearEntry(PLDHashTable* aTable,
                                  PLDHashEntryHdr* aEntry);

  // We achieve measurable reduction in locking contention in parallel CSS
  // parsing by increasing the number of subtables up to 128. This has been
  // measured to have neglible impact on the performance of initialization, GC,
  // and shutdown.
  //
  // Another important consideration is memory, since we're adding fixed
  // overhead per content process, which we try to avoid. Measuring a
  // mostly-empty page [1] with various numbers of subtables, we get the
  // following deep sizes for the atom table:
  //       1 subtable:  278K
  //       8 subtables: 279K
  //      16 subtables: 282K
  //      64 subtables: 286K
  //     128 subtables: 290K
  //
  // So 128 subtables costs us 12K relative to a single table, and 4K relative
  // to 64 subtables. Conversely, measuring parallel (6 thread) CSS parsing on
  // tp6-facebook, a single table provides ~150ms of locking overhead per
  // thread, 64 subtables provides ~2-3ms of overhead, and 128 subtables
  // provides <1ms. And so while either 64 or 128 subtables would probably be
  // acceptable, achieving a measurable reduction in contention for 4k of fixed
  // memory overhead is probably worth it.
  //
  // [1] The numbers will look different for content processes with complex
  // pages loaded, but in those cases the actual atoms will dominate memory
  // usage and the overhead of extra tables will be negligible. We're mostly
  // interested in the fixed cost for nearly-empty content processes.
  const static size_t kNumSubTables = 128;  // Must be power of two.

 private:
  nsAtomSubTable mSubTables[kNumSubTables];
};

// Static singleton instance for the atom table.
static nsAtomTable* gAtomTable;

static PLDHashNumber AtomTableGetHash(const void* aKey) {
  const AtomTableKey* k = static_cast<const AtomTableKey*>(aKey);
  return k->mHash;
}

static bool AtomTableMatchKey(const PLDHashEntryHdr* aEntry, const void* aKey) {
  const AtomTableEntry* he = static_cast<const AtomTableEntry*>(aEntry);
  const AtomTableKey* k = static_cast<const AtomTableKey*>(aKey);

  if (k->mUTF8String) {
    bool err = false;
    return (CompareUTF8toUTF16(nsDependentCSubstring(
                                   k->mUTF8String, k->mUTF8String + k->mLength),
                               nsDependentAtomString(he->mAtom), &err) == 0) &&
           !err;
  }

  return he->mAtom->Equals(k->mUTF16String, k->mLength);
}

void nsAtomTable::AtomTableClearEntry(PLDHashTable* aTable,
                                      PLDHashEntryHdr* aEntry) {
  auto entry = static_cast<AtomTableEntry*>(aEntry);
  entry->mAtom = nullptr;
}

static void AtomTableInitEntry(PLDHashEntryHdr* aEntry, const void* aKey) {
  static_cast<AtomTableEntry*>(aEntry)->mAtom = nullptr;
}

static const PLDHashTableOps AtomTableOps = {
    AtomTableGetHash, AtomTableMatchKey, PLDHashTable::MoveEntryStub,
    nsAtomTable::AtomTableClearEntry, AtomTableInitEntry};

// The atom table very quickly gets 10,000+ entries in it (or even 100,000+).
// But choosing the best initial subtable length has some subtleties: we add
// ~2700 static atoms at start-up, and then we start adding and removing
// dynamic atoms. If we make the tables too big to start with, when the first
// dynamic atom gets removed from a given table the load factor will be < 25%
// and we will shrink it.
//
// So we first make the simplifying assumption that the atoms are more or less
// evenly-distributed across the subtables (which is the case empirically).
// Then, we take the total atom count when the first dynamic atom is removed
// (~2700), divide that across the N subtables, and the largest capacity that
// will allow each subtable to be > 25% full with that count.
//
// So want an initial subtable capacity less than (2700 / N) * 4 = 10800 / N.
// Rounding down to the nearest power of two gives us 8192 / N. Since the
// capacity is double the initial length, we end up with (4096 / N) per
// subtable.
#define INITIAL_SUBTABLE_LENGTH (4096 / nsAtomTable::kNumSubTables)

nsAtomSubTable& nsAtomTable::SelectSubTable(AtomTableKey& aKey) {
  // There are a few considerations around how we select subtables.
  //
  // First, we want entries to be evenly distributed across the subtables. This
  // can be achieved by using any bits in the hash key, assuming the key itself
  // is evenly-distributed. Empirical measurements indicate that this method
  // produces a roughly-even distribution across subtables.
  //
  // Second, we want to use the hash bits that are least likely to influence an
  // entry's position within the subtable. If we used the exact same bits used
  // by the subtables, then each subtable would compute the same position for
  // every entry it observes, leading to pessimal performance. In this case,
  // we're using PLDHashTable, whose primary hash function uses the N leftmost
  // bits of the hash value (where N is the log2 capacity of the table). This
  // means we should prefer the rightmost bits here.
  //
  // Note that the below is equivalent to mHash % kNumSubTables, a replacement
  // which an optimizing compiler should make, but let's avoid any doubt.
  static_assert((kNumSubTables & (kNumSubTables - 1)) == 0,
                "must be power of two");
  return mSubTables[aKey.mHash & (kNumSubTables - 1)];
}

void nsAtomTable::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                         AtomsSizes& aSizes) {
  MOZ_ASSERT(NS_IsMainThread());
  aSizes.mTable += aMallocSizeOf(this);
  for (auto& table : mSubTables) {
    MutexAutoLock lock(table.mLock);
    table.AddSizeOfExcludingThisLocked(aMallocSizeOf, aSizes);
  }
}

void nsAtomTable::GC(GCKind aKind) {
  MOZ_ASSERT(NS_IsMainThread());
  sRecentlyUsedMainThreadAtoms.Clear();

  // Note that this is effectively an incremental GC, since only one subtable
  // is locked at a time.
  for (auto& table : mSubTables) {
    MutexAutoLock lock(table.mLock);
    table.GCLocked(aKind);
  }

  // We would like to assert that gUnusedAtomCount matches the number of atoms
  // we found in the table which we removed. However, there are two problems
  // with this:
  // * We have multiple subtables, each with their own lock. For optimal
  //   performance we only want to hold one lock at a time, but this means
  //   that atoms can be added and removed between GC slices.
  // * Even if we held all the locks and performed all GC slices atomically,
  //   the locks are not acquired for AddRef() and Release() calls. This means
  //   we might see a gUnusedAtomCount value in between, say, AddRef()
  //   incrementing mRefCnt and it decrementing gUnusedAtomCount.
  //
  // So, we don't bother asserting that there are no unused atoms at the end of
  // a regular GC. But we can (and do) assert this just after the last GC at
  // shutdown.
  //
  // Note that, barring refcounting bugs, an atom can only go from a zero
  // refcount to a non-zero refcount while the atom table lock is held, so
  // so we won't try to resurrect a zero refcount atom while trying to delete
  // it.

  MOZ_ASSERT_IF(aKind == GCKind::Shutdown,
                nsDynamicAtom::gUnusedAtomCount == 0);
}

size_t nsAtomTable::RacySlowCount() {
  // Trigger a GC so that the result is deterministic modulo other threads.
  GC(GCKind::RegularOperation);
  size_t count = 0;
  for (auto& table : mSubTables) {
    MutexAutoLock lock(table.mLock);
    count += table.mTable.EntryCount();
  }

  return count;
}

nsAtomSubTable::nsAtomSubTable()
    : mLock("Atom Sub-Table Lock"),
      mTable(&AtomTableOps, sizeof(AtomTableEntry), INITIAL_SUBTABLE_LENGTH) {}

void nsAtomSubTable::GCLocked(GCKind aKind) {
  MOZ_ASSERT(NS_IsMainThread());
  mLock.AssertCurrentThreadOwns();

  int32_t removedCount = 0;  // A non-atomic temporary for cheaper increments.
  nsAutoCString nonZeroRefcountAtoms;
  uint32_t nonZeroRefcountAtomsCount = 0;
  for (auto i = mTable.Iter(); !i.Done(); i.Next()) {
    auto entry = static_cast<AtomTableEntry*>(i.Get());
    if (entry->mAtom->IsStatic()) {
      continue;
    }

    nsAtom* atom = entry->mAtom;
    if (atom->IsDynamic() && atom->AsDynamic()->mRefCnt == 0) {
      i.Remove();
      nsDynamicAtom::Destroy(atom->AsDynamic());
      ++removedCount;
    }
#ifdef NS_FREE_PERMANENT_DATA
    else if (aKind == GCKind::Shutdown && PR_GetEnv("XPCOM_MEM_BLOAT_LOG")) {
      // Only report leaking atoms in leak-checking builds in a run where we
      // are checking for leaks, during shutdown. If something is anomalous,
      // then we'll assert later in this function.
      nsAutoCString name;
      atom->ToUTF8String(name);
      if (nonZeroRefcountAtomsCount == 0) {
        nonZeroRefcountAtoms = name;
      } else if (nonZeroRefcountAtomsCount < 20) {
        nonZeroRefcountAtoms += ","_ns + name;
      } else if (nonZeroRefcountAtomsCount == 20) {
        nonZeroRefcountAtoms += ",..."_ns;
      }
      nonZeroRefcountAtomsCount++;
    }
#endif
  }
  if (nonZeroRefcountAtomsCount) {
    nsPrintfCString msg("%d dynamic atom(s) with non-zero refcount: %s",
                        nonZeroRefcountAtomsCount, nonZeroRefcountAtoms.get());
    NS_ASSERTION(nonZeroRefcountAtomsCount == 0, msg.get());
  }

  nsDynamicAtom::gUnusedAtomCount -= removedCount;
}

void nsDynamicAtom::GCAtomTable() {
  MOZ_ASSERT(gAtomTable);
  if (NS_IsMainThread()) {
    gAtomTable->GC(GCKind::RegularOperation);
  }
}

//----------------------------------------------------------------------

// Have the static atoms been inserted into the table?
static bool gStaticAtomsDone = false;

void NS_InitAtomTable() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gAtomTable);

  // We register static atoms immediately so they're available for use as early
  // as possible.
  gAtomTable = new nsAtomTable();
  gAtomTable->RegisterStaticAtoms(nsGkAtoms::sAtoms, nsGkAtoms::sAtomsLen);
  gStaticAtomsDone = true;
}

void NS_ShutdownAtomTable() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gAtomTable);

#ifdef NS_FREE_PERMANENT_DATA
  // Do a final GC to satisfy leak checking. We skip this step in release
  // builds.
  gAtomTable->GC(GCKind::Shutdown);
#endif

  delete gAtomTable;
  gAtomTable = nullptr;
}

void NS_AddSizeOfAtoms(MallocSizeOf aMallocSizeOf, AtomsSizes& aSizes) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
}

void nsAtomSubTable::AddSizeOfExcludingThisLocked(MallocSizeOf aMallocSizeOf,
                                                  AtomsSizes& aSizes) {
  mLock.AssertCurrentThreadOwns();
  aSizes.mTable += mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mTable.Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<AtomTableEntry*>(iter.Get());
    entry->mAtom->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
  }
}

void nsAtomTable::RegisterStaticAtoms(const nsStaticAtom* aAtoms,
                                      size_t aAtomsLen) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!gStaticAtomsDone, "Static atom insertion is finished!");

  for (uint32_t i = 0; i < aAtomsLen; ++i) {
    const nsStaticAtom* atom = &aAtoms[i];
    MOZ_ASSERT(IsAsciiNullTerminated(atom->String()));
    MOZ_ASSERT(NS_strlen(atom->String()) == atom->GetLength());
    MOZ_ASSERT(atom->IsAsciiLowercase() ==
               ::IsAsciiLowercase(atom->String(), atom->GetLength()));

    // This assertion ensures the static atom's precomputed hash value matches
    // what would be computed by mozilla::HashString(aStr), which is what we use
    // when atomizing strings. We compute this hash in Atom.py.
    MOZ_ASSERT(HashString(atom->String()) == atom->hash());

    AtomTableKey key(atom);
    nsAtomSubTable& table = SelectSubTable(key);
    MutexAutoLock lock(table.mLock);
    AtomTableEntry* he = table.Add(key);

    if (he->mAtom) {
      // There are two ways we could get here.
      // - Register two static atoms with the same string.
      // - Create a dynamic atom and then register a static atom with the same
      //   string while the dynamic atom is alive.
      // Both cases can cause subtle bugs, and are disallowed. We're
      // programming in C++ here, not Smalltalk.
      nsAutoCString name;
      he->mAtom->ToUTF8String(name);
      MOZ_CRASH_UNSAFE_PRINTF("Atom for '%s' already exists", name.get());
    }
    he->mAtom = const_cast<nsStaticAtom*>(atom);
  }
}

already_AddRefed<nsAtom> NS_Atomize(const char* aUTF8String) {
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->Atomize(nsDependentCString(aUTF8String));
}

already_AddRefed<nsAtom> nsAtomTable::Atomize(const nsACString& aUTF8String) {
  bool err;
  AtomTableKey key(aUTF8String.Data(), aUTF8String.Length(), &err);
  if (MOZ_UNLIKELY(err)) {
    MOZ_ASSERT_UNREACHABLE("Tried to atomize invalid UTF-8.");
    // The input was invalid UTF-8. Let's replace the errors with U+FFFD
    // and atomize the result.
    nsString str;
    CopyUTF8toUTF16(aUTF8String, str);
    return Atomize(str);
  }
  nsAtomSubTable& table = SelectSubTable(key);
  MutexAutoLock lock(table.mLock);
  AtomTableEntry* he = table.Add(key);

  if (he->mAtom) {
    RefPtr<nsAtom> atom = he->mAtom;
    return atom.forget();
  }

  nsString str;
  CopyUTF8toUTF16(aUTF8String, str);
  RefPtr<nsAtom> atom = dont_AddRef(nsDynamicAtom::Create(str, key.mHash));

  he->mAtom = atom;

  return atom.forget();
}

already_AddRefed<nsAtom> NS_Atomize(const nsACString& aUTF8String) {
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->Atomize(aUTF8String);
}

already_AddRefed<nsAtom> NS_Atomize(const char16_t* aUTF16String) {
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->Atomize(nsDependentString(aUTF16String));
}

already_AddRefed<nsAtom> nsAtomTable::Atomize(const nsAString& aUTF16String) {
  AtomTableKey key(aUTF16String.Data(), aUTF16String.Length());
  nsAtomSubTable& table = SelectSubTable(key);
  MutexAutoLock lock(table.mLock);
  AtomTableEntry* he = table.Add(key);

  if (he->mAtom) {
    RefPtr<nsAtom> atom = he->mAtom;
    return atom.forget();
  }

  RefPtr<nsAtom> atom =
      dont_AddRef(nsDynamicAtom::Create(aUTF16String, key.mHash));
  he->mAtom = atom;

  return atom.forget();
}

already_AddRefed<nsAtom> NS_Atomize(const nsAString& aUTF16String) {
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->Atomize(aUTF16String);
}

already_AddRefed<nsAtom> nsAtomTable::AtomizeMainThread(
    const nsAString& aUTF16String) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsAtom> retVal;
  AtomTableKey key(aUTF16String.Data(), aUTF16String.Length());
  auto p = sRecentlyUsedMainThreadAtoms.Lookup(key);
  if (p) {
    retVal = p.Data();
    return retVal.forget();
  }

  nsAtomSubTable& table = SelectSubTable(key);
  MutexAutoLock lock(table.mLock);
  AtomTableEntry* he = table.Add(key);

  if (he->mAtom) {
    retVal = he->mAtom;
  } else {
    RefPtr<nsAtom> newAtom =
        dont_AddRef(nsDynamicAtom::Create(aUTF16String, key.mHash));
    he->mAtom = newAtom;
    retVal = std::move(newAtom);
  }

  p.Set(retVal);
  return retVal.forget();
}

already_AddRefed<nsAtom> NS_AtomizeMainThread(const nsAString& aUTF16String) {
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->AtomizeMainThread(aUTF16String);
}

nsrefcnt NS_GetNumberOfAtoms(void) {
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->RacySlowCount();
}

int32_t NS_GetUnusedAtomCount(void) { return nsDynamicAtom::gUnusedAtomCount; }

nsStaticAtom* NS_GetStaticAtom(const nsAString& aUTF16String) {
  MOZ_ASSERT(gStaticAtomsDone, "Static atom setup not yet done.");
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->GetStaticAtom(aUTF16String);
}

nsStaticAtom* nsAtomTable::GetStaticAtom(const nsAString& aUTF16String) {
  AtomTableKey key(aUTF16String.Data(), aUTF16String.Length());
  nsAtomSubTable& table = SelectSubTable(key);
  MutexAutoLock lock(table.mLock);
  AtomTableEntry* he = table.Search(key);
  return he && he->mAtom->IsStatic() ? static_cast<nsStaticAtom*>(he->mAtom)
                                     : nullptr;
}

void ToLowerCaseASCII(RefPtr<nsAtom>& aAtom) {
  // Assume the common case is that the atom is already ASCII lowercase.
  if (aAtom->IsAsciiLowercase()) {
    return;
  }

  nsAutoString lowercased;
  ToLowerCaseASCII(nsDependentAtomString(aAtom), lowercased);
  aAtom = NS_Atomize(lowercased);
}
