/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"

#include "nsAtomTable.h"
#include "nsStaticAtom.h"
#include "nsString.h"
#include "nsCRT.h"
#include "PLDHashTable.h"
#include "prenv.h"
#include "nsThreadUtils.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"

// There are two kinds of atoms handled by this module.
//
// - Dynamic: the atom itself is heap allocated, as is the nsStringBuffer it
//   points to. |gAtomTable| holds weak references to dynamic atoms. When the
//   refcount of a dynamic atom drops to zero, we increment a static counter.
//   When that counter reaches a certain threshold, we iterate over the atom
//   table, removing and deleting dynamic atoms with refcount zero. This allows
//   us to avoid acquiring the atom table lock during normal refcounting.
//
// - Static: the atom itself is heap allocated, but it points to a static
//   nsStringBuffer. |gAtomTable| effectively owns static atoms, because such
//   atoms ignore all AddRef/Release calls, which ensures they stay alive until
//   |gAtomTable| itself is destroyed whereupon they are explicitly deleted.
//
// Note that gAtomTable is used on multiple threads, and callers must acquire
// gAtomTableLock before touching it.

using namespace mozilla;

//----------------------------------------------------------------------

enum class GCKind {
  RegularOperation,
  Shutdown,
};

// This class encapsulates the functions that need access to nsAtom's private
// members.
class nsAtomFriend
{
public:
  static void RegisterStaticAtoms(const nsStaticAtom* aAtoms,
                                  uint32_t aAtomCount);

  static void AtomTableClearEntry(PLDHashTable* aTable,
                                  PLDHashEntryHdr* aEntry);

  static void GCAtomTableLocked(const MutexAutoLock& aProofOfLock,
                                GCKind aKind);

  static already_AddRefed<nsAtom> Atomize(const nsACString& aUTF8String);
  static already_AddRefed<nsAtom> Atomize(const nsAString& aUTF16String);
  static already_AddRefed<nsAtom> AtomizeMainThread(const nsAString& aUTF16Str);
};

//----------------------------------------------------------------------

// gUnusedAtomCount is incremented when an atom loses its last reference
// (and thus turned into unused state), and decremented when an unused
// atom gets a reference again. The atom table relies on this value to
// schedule GC. This value can temporarily go below zero when multiple
// threads are operating the same atom, so it has to be signed so that
// we wouldn't use overflow value for comparison.
// See nsAtom::AddRef() and nsAtom::Release().
static Atomic<int32_t, ReleaseAcquire> gUnusedAtomCount(0);

// This constructor is for dynamic atoms and HTML5 atoms.
nsAtom::nsAtom(AtomKind aKind, const nsAString& aString, uint32_t aHash)
  : mRefCnt(1)
  , mLength(aString.Length())
  , mKind(static_cast<uint32_t>(aKind))
  , mHash(aHash)
{
  MOZ_ASSERT(aKind == AtomKind::DynamicAtom || aKind == AtomKind::HTML5Atom);
  RefPtr<nsStringBuffer> buf = nsStringBuffer::FromString(aString);
  if (buf) {
    mString = static_cast<char16_t*>(buf->Data());
  } else {
    const size_t size = (mLength + 1) * sizeof(char16_t);
    buf = nsStringBuffer::Alloc(size);
    if (MOZ_UNLIKELY(!buf)) {
      // We OOM because atom allocations should be small and it's hard to
      // handle them more gracefully in a constructor.
      NS_ABORT_OOM(size);
    }
    mString = static_cast<char16_t*>(buf->Data());
    CopyUnicodeTo(aString, 0, mString, mLength);
    mString[mLength] = char16_t(0);
  }

  MOZ_ASSERT_IF(IsDynamicAtom(), mHash == HashString(mString, mLength));

  MOZ_ASSERT(mString[mLength] == char16_t(0), "null terminated");
  MOZ_ASSERT(buf && buf->StorageSize() >= (mLength + 1) * sizeof(char16_t),
             "enough storage");
  MOZ_ASSERT(Equals(aString), "correct data");

  // Take ownership of buffer
  mozilla::Unused << buf.forget();
}

// This constructor is for static atoms.
nsAtom::nsAtom(const char16_t* aString, uint32_t aLength, uint32_t aHash)
  : mLength(aLength)
  , mKind(static_cast<uint32_t>(AtomKind::StaticAtom))
  , mHash(aHash)
  , mString(const_cast<char16_t*>(aString))
{
  MOZ_ASSERT(mHash == HashString(mString, mLength));

  MOZ_ASSERT(mString[mLength] == char16_t(0), "null terminated");
  MOZ_ASSERT(NS_strlen(mString) == mLength, "correct storage");
}

nsAtom::~nsAtom()
{
  if (!IsStaticAtom()) {
    MOZ_ASSERT(IsDynamicAtom() || IsHTML5Atom());
    nsStringBuffer::FromData(mString)->Release();
  }
}

void
nsAtom::ToString(nsAString& aString) const
{
  // See the comment on |mString|'s declaration.
  if (IsStaticAtom()) {
    // AssignLiteral() lets us assign without copying. This isn't a string
    // literal, but it's a static atom and thus has an unbounded lifetime,
    // which is what's important.
    aString.AssignLiteral(mString, mLength);
  } else {
    nsStringBuffer::FromData(mString)->ToString(mLength, aString);
  }
}

void
nsAtom::ToUTF8String(nsACString& aBuf) const
{
  MOZ_ASSERT(!IsHTML5Atom(), "Called ToUTF8String() on an HTML5 atom");
  CopyUTF16toUTF8(nsDependentString(mString, mLength), aBuf);
}

size_t
nsAtom::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  MOZ_ASSERT(!IsHTML5Atom(), "Called SizeOfIncludingThis() on an HTML5 atom");
  size_t n = aMallocSizeOf(this);
  // String buffers pointed to by static atoms are in static memory, and so
  // are not measured here.
  if (IsDynamicAtom()) {
    n += nsStringBuffer::FromData(mString)->SizeOfIncludingThisIfUnshared(
           aMallocSizeOf);
  } else {
    MOZ_ASSERT(IsStaticAtom());
  }
  return n;
}

//----------------------------------------------------------------------

/**
 * The shared hash table for atom lookups.
 *
 * Callers must hold gAtomTableLock before manipulating the table.
 */
static PLDHashTable* gAtomTable;
static Mutex* gAtomTableLock;

struct AtomTableKey
{
  AtomTableKey(const char16_t* aUTF16String, uint32_t aLength, uint32_t aHash)
    : mUTF16String(aUTF16String)
    , mUTF8String(nullptr)
    , mLength(aLength)
    , mHash(aHash)
  {
    MOZ_ASSERT(mHash == HashString(mUTF16String, mLength));
  }

  AtomTableKey(const char* aUTF8String, uint32_t aLength, uint32_t aHash)
    : mUTF16String(nullptr)
    , mUTF8String(aUTF8String)
    , mLength(aLength)
    , mHash(aHash)
  {
    mozilla::DebugOnly<bool> err;
    MOZ_ASSERT(aHash == HashUTF8AsUTF16(mUTF8String, mLength, &err));
  }

  AtomTableKey(const char16_t* aUTF16String, uint32_t aLength,
               uint32_t* aHashOut)
    : mUTF16String(aUTF16String)
    , mUTF8String(nullptr)
    , mLength(aLength)
  {
    mHash = HashString(mUTF16String, mLength);
    *aHashOut = mHash;
  }

  AtomTableKey(const char* aUTF8String, uint32_t aLength, uint32_t* aHashOut)
    : mUTF16String(nullptr)
    , mUTF8String(aUTF8String)
    , mLength(aLength)
  {
    bool err;
    mHash = HashUTF8AsUTF16(mUTF8String, mLength, &err);
    if (err) {
      mUTF8String = nullptr;
      mLength = 0;
      mHash = 0;
    }
    *aHashOut = mHash;
  }

  const char16_t* mUTF16String;
  const char* mUTF8String;
  uint32_t mLength;
  uint32_t mHash;
};

struct AtomTableEntry : public PLDHashEntryHdr
{
  // These references are either to dynamic atoms, in which case they are
  // non-owning, or they are to static atoms, which aren't really refcounted.
  // See the comment at the top of this file for more details.
  nsAtom* MOZ_NON_OWNING_REF mAtom;
};

static PLDHashNumber
AtomTableGetHash(const void* aKey)
{
  const AtomTableKey* k = static_cast<const AtomTableKey*>(aKey);
  return k->mHash;
}

static bool
AtomTableMatchKey(const PLDHashEntryHdr* aEntry, const void* aKey)
{
  const AtomTableEntry* he = static_cast<const AtomTableEntry*>(aEntry);
  const AtomTableKey* k = static_cast<const AtomTableKey*>(aKey);

  if (k->mUTF8String) {
    return
      CompareUTF8toUTF16(nsDependentCSubstring(k->mUTF8String,
                                               k->mUTF8String + k->mLength),
                         nsDependentAtomString(he->mAtom)) == 0;
  }

  return he->mAtom->Equals(k->mUTF16String, k->mLength);
}

void
nsAtomFriend::AtomTableClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  auto entry = static_cast<AtomTableEntry*>(aEntry);
  nsAtom* atom = entry->mAtom;
  if (atom->IsStaticAtom()) {
    // This case -- when the entry being cleared holds a static atom -- only
    // occurs when gAtomTable is destroyed, whereupon all static atoms within it
    // must be explicitly deleted.
    delete atom;
  }
}

static void
AtomTableInitEntry(PLDHashEntryHdr* aEntry, const void* aKey)
{
  static_cast<AtomTableEntry*>(aEntry)->mAtom = nullptr;
}

static const PLDHashTableOps AtomTableOps = {
  AtomTableGetHash,
  AtomTableMatchKey,
  PLDHashTable::MoveEntryStub,
  nsAtomFriend::AtomTableClearEntry,
  AtomTableInitEntry
};

//----------------------------------------------------------------------

#define RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE 31
static nsAtom*
  sRecentlyUsedMainThreadAtoms[RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE] = {};

void
nsAtomFriend::GCAtomTableLocked(const MutexAutoLock& aProofOfLock, GCKind aKind)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE; ++i) {
    sRecentlyUsedMainThreadAtoms[i] = nullptr;
  }

  int32_t removedCount = 0; // Use a non-atomic temporary for cheaper increments.
  nsAutoCString nonZeroRefcountAtoms;
  uint32_t nonZeroRefcountAtomsCount = 0;
  for (auto i = gAtomTable->Iter(); !i.Done(); i.Next()) {
    auto entry = static_cast<AtomTableEntry*>(i.Get());
    if (entry->mAtom->IsStaticAtom()) {
      continue;
    }

    nsAtom* atom = entry->mAtom;
    if (atom->mRefCnt == 0) {
      i.Remove();
      delete atom;
      ++removedCount;
    }
#ifdef NS_FREE_PERMANENT_DATA
    else if (aKind == GCKind::Shutdown && PR_GetEnv("XPCOM_MEM_BLOAT_LOG")) {
      // Only report leaking atoms in leak-checking builds in a run
      // where we are checking for leaks, during shutdown. If
      // something is anomalous, then we'll assert later in this
      // function.
      nsAutoCString name;
      atom->ToUTF8String(name);
      if (nonZeroRefcountAtomsCount == 0) {
        nonZeroRefcountAtoms = name;
      } else if (nonZeroRefcountAtomsCount < 20) {
        nonZeroRefcountAtoms += NS_LITERAL_CSTRING(",") + name;
      } else if (nonZeroRefcountAtomsCount == 20) {
        nonZeroRefcountAtoms += NS_LITERAL_CSTRING(",...");
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

  // We would like to assert that gUnusedAtomCount matches the number of atoms
  // we found in the table which we removed. During the course of this function,
  // the atom table is locked, but this lock is not acquired for AddRef() and
  // Release() calls. This means we might see a gUnusedAtomCount value in
  // between, say, AddRef() incrementing mRefCnt and it decrementing
  // gUnusedAtomCount. So, we don't bother asserting that there are no unused
  // atoms at the end of a regular GC. But we can (and do) assert thist just
  // after the last GC at shutdown.
  //
  // Note that, barring refcounting bugs, an atom can only go from a zero
  // refcount to a non-zero refcount while the atom table lock is held, so
  // so we won't try to resurrect a zero refcount atom while trying to delete
  // it.

  MOZ_ASSERT_IF(aKind == GCKind::Shutdown, removedCount == gUnusedAtomCount);

  gUnusedAtomCount -= removedCount;
}

static void
GCAtomTable()
{
  if (NS_IsMainThread()) {
    MutexAutoLock lock(*gAtomTableLock);
    nsAtomFriend::GCAtomTableLocked(lock, GCKind::RegularOperation);
  }
}

MozExternalRefCountType
nsAtom::AddRef()
{
  MOZ_ASSERT(!IsHTML5Atom(), "Attempt to AddRef an HTML5 atom");
  if (!IsDynamicAtom()) {
    MOZ_ASSERT(IsStaticAtom());
    return 2;
  }

  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count = ++mRefCnt;
  if (count == 1) {
    gUnusedAtomCount--;
  }
  return count;
}

MozExternalRefCountType
nsAtom::Release()
{
  MOZ_ASSERT(!IsHTML5Atom(), "Attempt to Release an HTML5 atom");
  if (!IsDynamicAtom()) {
    MOZ_ASSERT(IsStaticAtom());
    return 1;
  }

  #ifdef DEBUG
  // We set a lower GC threshold for atoms in debug builds so that we exercise
  // the GC machinery more often.
  static const int32_t kAtomGCThreshold = 20;
  #else
  static const int32_t kAtomGCThreshold = 10000;
  #endif

  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;
  if (count == 0) {
    if (++gUnusedAtomCount >= kAtomGCThreshold) {
      GCAtomTable();
    }
  }

  return count;
}

//----------------------------------------------------------------------

class StaticAtomEntry : public PLDHashEntryHdr
{
public:
  typedef const nsAString& KeyType;
  typedef const nsAString* KeyTypePointer;

  explicit StaticAtomEntry(KeyTypePointer aKey) {}
  StaticAtomEntry(const StaticAtomEntry& aOther) : mAtom(aOther.mAtom) {}

  // We do not delete the atom because that's done when gAtomTable is
  // destroyed -- which happens immediately after gStaticAtomTable is destroyed
  // -- in NS_PurgeAtomTable().
  ~StaticAtomEntry() {}

  bool KeyEquals(KeyTypePointer aKey) const
  {
    return mAtom->Equals(*aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return HashString(*aKey);
  }

  enum { ALLOW_MEMMOVE = true };

  // Static atoms aren't really refcounted. Because these entries live in a
  // global hashtable, this reference is essentially owning.
  nsAtom* MOZ_OWNING_REF mAtom;
};

/**
 * A hashtable of static atoms that existed at app startup. This hashtable
 * helps nsHtml5AtomTable.
 */
typedef nsTHashtable<StaticAtomEntry> StaticAtomTable;
static StaticAtomTable* gStaticAtomTable = nullptr;

/**
 * Whether it is still OK to add atoms to gStaticAtomTable.
 */
static bool gStaticAtomTableSealed = false;

// The atom table very quickly gets 10,000+ entries in it (or even 100,000+).
// But choosing the best initial length has some subtleties: we add ~2700
// static atoms to the table at start-up, and then we start adding and removing
// dynamic atoms. If we make the table too big to start with, when the first
// dynamic atom gets removed the load factor will be < 25% and so we will
// shrink it to 4096 entries.
//
// By choosing an initial length of 4096, we get an initial capacity of 8192.
// That's the biggest initial capacity that will let us be > 25% full when the
// first dynamic atom is removed (when the count is ~2700), thus avoiding any
// shrinking.
#define ATOM_HASHTABLE_INITIAL_LENGTH  4096

void
NS_InitAtomTable()
{
  MOZ_ASSERT(!gAtomTable);
  gAtomTable = new PLDHashTable(&AtomTableOps, sizeof(AtomTableEntry),
                                ATOM_HASHTABLE_INITIAL_LENGTH);
  gAtomTableLock = new Mutex("Atom Table Lock");

  // Bug 1340710 has caused us to generate an empty atom at arbitrary times
  // after startup.  If we end up creating one before nsGkAtoms::_empty is
  // registered, we get an assertion about transmuting a dynamic atom into a
  // static atom.  In order to avoid that, we register an empty string static
  // atom as soon as we initialize the atom table to guarantee that the empty
  // string atom will always be static.
  NS_STATIC_ATOM_BUFFER(empty, "");
  static nsAtom* empty_atom = nullptr;
  static const nsStaticAtom default_atoms[] = {
    NS_STATIC_ATOM(empty, &empty_atom)
  };
  NS_RegisterStaticAtoms(default_atoms);
}

void
NS_ShutdownAtomTable()
{
  delete gStaticAtomTable;
  gStaticAtomTable = nullptr;

#ifdef NS_FREE_PERMANENT_DATA
  // Do a final GC to satisfy leak checking. We skip this step in release
  // builds.
  {
    MutexAutoLock lock(*gAtomTableLock);
    nsAtomFriend::GCAtomTableLocked(lock, GCKind::Shutdown);
  }
#endif

  delete gAtomTable;
  gAtomTable = nullptr;
  delete gAtomTableLock;
  gAtomTableLock = nullptr;
}

void
NS_SizeOfAtomTablesIncludingThis(MallocSizeOf aMallocSizeOf,
                                 size_t* aMain, size_t* aStatic)
{
  MutexAutoLock lock(*gAtomTableLock);
  *aMain = gAtomTable->ShallowSizeOfIncludingThis(aMallocSizeOf);
  for (auto iter = gAtomTable->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<AtomTableEntry*>(iter.Get());
    *aMain += entry->mAtom->SizeOfIncludingThis(aMallocSizeOf);
  }

  // The atoms pointed to by gStaticAtomTable are also pointed to by gAtomTable,
  // and they're measured by the loop above. So no need to measure them here.
  *aStatic = gStaticAtomTable
           ? gStaticAtomTable->ShallowSizeOfIncludingThis(aMallocSizeOf)
           : 0;
}

static inline AtomTableEntry*
GetAtomHashEntry(const char* aString, uint32_t aLength, uint32_t* aHashOut)
{
  gAtomTableLock->AssertCurrentThreadOwns();
  AtomTableKey key(aString, aLength, aHashOut);
  // This is an infallible add.
  return static_cast<AtomTableEntry*>(gAtomTable->Add(&key));
}

static inline AtomTableEntry*
GetAtomHashEntry(const char16_t* aString, uint32_t aLength, uint32_t* aHashOut)
{
  gAtomTableLock->AssertCurrentThreadOwns();
  AtomTableKey key(aString, aLength, aHashOut);
  // This is an infallible add.
  return static_cast<AtomTableEntry*>(gAtomTable->Add(&key));
}

void
nsAtomFriend::RegisterStaticAtoms(const nsStaticAtom* aAtoms,
                                  uint32_t aAtomCount)
{
  MutexAutoLock lock(*gAtomTableLock);

  MOZ_RELEASE_ASSERT(!gStaticAtomTableSealed,
                     "Atom table has already been sealed!");

  if (!gStaticAtomTable) {
    gStaticAtomTable = new StaticAtomTable();
  }

  for (uint32_t i = 0; i < aAtomCount; ++i) {
    const char16_t* string = aAtoms[i].mString;
    nsAtom** atomp = aAtoms[i].mAtom;

    MOZ_ASSERT(nsCRT::IsAscii(string));

    uint32_t stringLen = NS_strlen(string);

    uint32_t hash;
    AtomTableEntry* he = GetAtomHashEntry(string, stringLen, &hash);

    nsAtom* atom = he->mAtom;
    if (atom) {
      // Disallow creating a dynamic atom, and then later, while the
      // dynamic atom is still alive, registering that same atom as a
      // static atom.  It causes subtle bugs, and we're programming in
      // C++ here, not Smalltalk.
      if (!atom->IsStaticAtom()) {
        nsAutoCString name;
        atom->ToUTF8String(name);
        MOZ_CRASH_UNSAFE_PRINTF(
          "Static atom registration for %s should be pushed back", name.get());
      }
    } else {
      atom = new nsAtom(string, stringLen, hash);
      he->mAtom = atom;
    }
    *atomp = atom;

    if (!gStaticAtomTableSealed) {
      StaticAtomEntry* entry =
        gStaticAtomTable->PutEntry(nsDependentAtomString(atom));
      MOZ_ASSERT(atom->IsStaticAtom());
      entry->mAtom = atom;
    }
  }
}

void
RegisterStaticAtoms(const nsStaticAtom* aAtoms, uint32_t aAtomCount)
{
  nsAtomFriend::RegisterStaticAtoms(aAtoms, aAtomCount);
}

already_AddRefed<nsAtom>
NS_Atomize(const char* aUTF8String)
{
  return nsAtomFriend::Atomize(nsDependentCString(aUTF8String));
}

already_AddRefed<nsAtom>
nsAtomFriend::Atomize(const nsACString& aUTF8String)
{
  MutexAutoLock lock(*gAtomTableLock);
  uint32_t hash;
  AtomTableEntry* he = GetAtomHashEntry(aUTF8String.Data(),
                                        aUTF8String.Length(),
                                        &hash);

  if (he->mAtom) {
    RefPtr<nsAtom> atom = he->mAtom;

    return atom.forget();
  }

  // This results in an extra addref/release of the nsStringBuffer.
  // Unfortunately there doesn't seem to be any APIs to avoid that.
  // Actually, now there is, sort of: ForgetSharedBuffer.
  nsString str;
  CopyUTF8toUTF16(aUTF8String, str);
  RefPtr<nsAtom> atom =
    dont_AddRef(new nsAtom(nsAtom::AtomKind::DynamicAtom, str, hash));

  he->mAtom = atom;

  return atom.forget();
}

already_AddRefed<nsAtom>
NS_Atomize(const nsACString& aUTF8String)
{
  return nsAtomFriend::Atomize(aUTF8String);
}

already_AddRefed<nsAtom>
NS_Atomize(const char16_t* aUTF16String)
{
  return nsAtomFriend::Atomize(nsDependentString(aUTF16String));
}

already_AddRefed<nsAtom>
nsAtomFriend::Atomize(const nsAString& aUTF16String)
{
  MutexAutoLock lock(*gAtomTableLock);
  uint32_t hash;
  AtomTableEntry* he = GetAtomHashEntry(aUTF16String.Data(),
                                        aUTF16String.Length(),
                                        &hash);

  if (he->mAtom) {
    RefPtr<nsAtom> atom = he->mAtom;

    return atom.forget();
  }

  RefPtr<nsAtom> atom =
    dont_AddRef(new nsAtom(nsAtom::AtomKind::DynamicAtom, aUTF16String, hash));
  he->mAtom = atom;

  return atom.forget();
}

already_AddRefed<nsAtom>
NS_Atomize(const nsAString& aUTF16String)
{
  return nsAtomFriend::Atomize(aUTF16String);
}

already_AddRefed<nsAtom>
nsAtomFriend::AtomizeMainThread(const nsAString& aUTF16String)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsAtom> retVal;
  uint32_t hash;
  AtomTableKey key(aUTF16String.Data(), aUTF16String.Length(), &hash);
  uint32_t index = hash % RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE;
  nsAtom* atom = sRecentlyUsedMainThreadAtoms[index];
  if (atom) {
    uint32_t length = atom->GetLength();
    if (length == key.mLength &&
        (memcmp(atom->GetUTF16String(),
                key.mUTF16String, length * sizeof(char16_t)) == 0)) {
      retVal = atom;
      return retVal.forget();
    }
  }

  MutexAutoLock lock(*gAtomTableLock);
  AtomTableEntry* he = static_cast<AtomTableEntry*>(gAtomTable->Add(&key));

  if (he->mAtom) {
    retVal = he->mAtom;
  } else {
    RefPtr<nsAtom> newAtom = dont_AddRef(
      new nsAtom(nsAtom::AtomKind::DynamicAtom, aUTF16String, hash));
    he->mAtom = newAtom;
    retVal = newAtom.forget();
  }

  sRecentlyUsedMainThreadAtoms[index] = he->mAtom;
  return retVal.forget();
}

already_AddRefed<nsAtom>
NS_AtomizeMainThread(const nsAString& aUTF16String)
{
  return nsAtomFriend::AtomizeMainThread(aUTF16String);
}

nsrefcnt
NS_GetNumberOfAtoms(void)
{
  GCAtomTable(); // Trigger a GC so we return a deterministic result.
  MutexAutoLock lock(*gAtomTableLock);
  return gAtomTable->EntryCount();
}

int32_t
NS_GetUnusedAtomCount(void)
{
  return gUnusedAtomCount;
}

nsAtom*
NS_GetStaticAtom(const nsAString& aUTF16String)
{
  NS_PRECONDITION(gStaticAtomTable, "Static atom table not created yet.");
  NS_PRECONDITION(gStaticAtomTableSealed, "Static atom table not sealed yet.");
  StaticAtomEntry* entry = gStaticAtomTable->GetEntry(aUTF16String);
  return entry ? entry->mAtom : nullptr;
}

void
NS_SealStaticAtomTable()
{
  gStaticAtomTableSealed = true;
}
