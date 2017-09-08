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
// - Dynamic: the Atom itself is heap allocated, as is the nsStringBuffer it
//   points to. |gAtomTable| holds weak references to dynamic Atoms. When the
//   refcount of a dynamic Atom drops to zero, we increment a static counter.
//   When that counter reaches a certain threshold, we iterate over the Atom
//   table, removing and deleting dynamic Atoms with refcount zero. This allows
//   us to avoid acquiring the atom table lock during normal refcounting.
//
// - Static: the Atom itself is heap allocated, but it points to a static
//   nsStringBuffer. |gAtomTable| effectively owns static Atoms, because such
//   Atoms ignore all AddRef/Release calls, which ensures they stay alive until
//   |gAtomTable| itself is destroyed whereupon they are explicitly deleted.
//
// Note that gAtomTable is used on multiple threads, and callers must acquire
// gAtomTableLock before touching it.

using namespace mozilla;

//----------------------------------------------------------------------

class CheckStaticAtomSizes
{
  CheckStaticAtomSizes()
  {
    static_assert((sizeof(nsFakeStringBuffer<1>().mRefCnt) ==
                   sizeof(nsStringBuffer().mRefCount)) &&
                  (sizeof(nsFakeStringBuffer<1>().mSize) ==
                   sizeof(nsStringBuffer().mStorageSize)) &&
                  (offsetof(nsFakeStringBuffer<1>, mRefCnt) ==
                   offsetof(nsStringBuffer, mRefCount)) &&
                  (offsetof(nsFakeStringBuffer<1>, mSize) ==
                   offsetof(nsStringBuffer, mStorageSize)) &&
                  (offsetof(nsFakeStringBuffer<1>, mStringData) ==
                   sizeof(nsStringBuffer)),
                  "mocked-up strings' representations should be compatible");
  }
};

//----------------------------------------------------------------------

static Atomic<uint32_t, ReleaseAcquire> gUnusedAtomCount(0);

#if defined(NS_BUILD_REFCNT_LOGGING)
// nsFakeStringBuffers don't really use the refcounting system, but we
// have to give a coherent series of addrefs and releases to the
// refcount logging system, or we'll hit assertions when running with
// XPCOM_MEM_LOG_CLASSES=nsStringBuffer.
class FakeBufferRefcountHelper
{
public:
  explicit FakeBufferRefcountHelper(nsStringBuffer* aBuffer)
    : mBuffer(aBuffer)
  {
    // Account for the initial static refcount of 1, so that we don't
    // hit a refcount logging assertion when this object first appears
    // with a refcount of 2.
    NS_LOG_ADDREF(aBuffer, 1, "nsStringBuffer", sizeof(nsStringBuffer));
  }

  ~FakeBufferRefcountHelper()
  {
    // We told the refcount logging system in the ctor that this
    // object was created, so now we have to tell it that it was
    // destroyed, to avoid leak reports. This may cause odd the
    // refcount isn't actually 0.
    NS_LOG_RELEASE(mBuffer, 0, "nsStringBuffer");
  }

private:
  nsStringBuffer* mBuffer;
};

UniquePtr<nsTArray<FakeBufferRefcountHelper>> gFakeBuffers;
#endif

class Atom final : public nsIAtom
{
public:
  static already_AddRefed<Atom> CreateDynamic(const nsAString& aString,
                                              uint32_t aHash)
  {
    // The refcount is appropriately initialized in the constructor.
    return dont_AddRef(new Atom(aString, aHash));
  }

  static Atom* CreateStatic(nsStringBuffer* aStringBuffer, uint32_t aLength,
                            uint32_t aHash)
  {
    return new Atom(aStringBuffer, aLength, aHash);
  }

  static void GCAtomTable();

  enum class GCKind {
    RegularOperation,
    Shutdown,
  };

  static void GCAtomTableLocked(const MutexAutoLock& aProofOfLock,
                                GCKind aKind);

private:
  // This constructor is for dynamic Atoms.
  Atom(const nsAString& aString, uint32_t aHash)
    : mRefCnt(1)
  {
    mLength = aString.Length();
    SetKind(AtomKind::DynamicAtom);
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

    mHash = aHash;
    MOZ_ASSERT(mHash == HashString(mString, mLength));

    NS_ASSERTION(mString[mLength] == char16_t(0), "null terminated");
    NS_ASSERTION(buf && buf->StorageSize() >= (mLength + 1) * sizeof(char16_t),
                 "enough storage");
    NS_ASSERTION(Equals(aString), "correct data");

    // Take ownership of buffer
    mozilla::Unused << buf.forget();
  }

  // This constructor is for static Atoms.
  Atom(nsStringBuffer* aStringBuffer, uint32_t aLength, uint32_t aHash)
  {
    mLength = aLength;
    SetKind(AtomKind::StaticAtom);
    mString = static_cast<char16_t*>(aStringBuffer->Data());

#if defined(NS_BUILD_REFCNT_LOGGING)
    MOZ_ASSERT(NS_IsMainThread());
    if (!gFakeBuffers) {
      gFakeBuffers = MakeUnique<nsTArray<FakeBufferRefcountHelper>>();
    }
    gFakeBuffers->AppendElement(aStringBuffer);
#endif

    // Technically we could currently avoid doing this addref by instead making
    // the static atom buffers have an initial refcount of 2.
    aStringBuffer->AddRef();

    mHash = aHash;
    MOZ_ASSERT(mHash == HashString(mString, mLength));

    MOZ_ASSERT(mString[mLength] == char16_t(0), "null terminated");
    MOZ_ASSERT(aStringBuffer &&
               aStringBuffer->StorageSize() == (mLength + 1) * sizeof(char16_t),
               "correct storage");
  }

public:
  // We don't need a virtual destructor because we always delete via an Atom*
  // pointer (in AtomTableClearEntry() for static Atoms, and in
  // GCAtomTableLocked() for dynamic Atoms), not an nsIAtom* pointer.
  ~Atom() {
    if (IsDynamicAtom()) {
      nsStringBuffer::FromData(mString)->Release();
    } else {
      MOZ_ASSERT(IsStaticAtom());
    }
  }

  NS_DECL_NSIATOM
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) final;
  typedef mozilla::TrueType HasThreadSafeRefCnt;

  MozExternalRefCountType DynamicAddRef();
  MozExternalRefCountType DynamicRelease();

protected:
  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

NS_IMPL_QUERY_INTERFACE(Atom, nsIAtom);

NS_IMETHODIMP
Atom::ToUTF8String(nsACString& aBuf)
{
  CopyUTF16toUTF8(nsDependentString(mString, mLength), aBuf);
  return NS_OK;
}

NS_IMETHODIMP_(size_t)
Atom::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
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

NS_IMETHODIMP_(MozExternalRefCountType)
nsIAtom::AddRef()
{
  MOZ_ASSERT(!IsHTML5Atom(), "Attempt to AddRef an nsHtml5Atom");
  if (!IsDynamicAtom()) {
    MOZ_ASSERT(IsStaticAtom());
    return 2;
  }
  return static_cast<Atom*>(this)->DynamicAddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsIAtom::Release()
{
  MOZ_ASSERT(!IsHTML5Atom(), "Attempt to Release an nsHtml5Atom");
  if (!IsDynamicAtom()) {
    MOZ_ASSERT(IsStaticAtom());
    return 1;
  }
  return static_cast<Atom*>(this)->DynamicRelease();
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
  // These references are either to dynamic Atoms, in which case they are
  // non-owning, or they are to static Atoms, which aren't really refcounted.
  // See the comment at the top of this file for more details.
  Atom* MOZ_NON_OWNING_REF mAtom;
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

static void
AtomTableClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  auto entry = static_cast<AtomTableEntry*>(aEntry);
  Atom* atom = entry->mAtom;
  if (atom->IsStaticAtom()) {
    // This case -- when the entry being cleared holds a static Atom -- only
    // occurs when gAtomTable is destroyed, whereupon all static Atoms within it
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
  AtomTableClearEntry,
  AtomTableInitEntry
};

//----------------------------------------------------------------------

#define RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE 31
static Atom*
  sRecentlyUsedMainThreadAtoms[RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE] = {};

void
Atom::GCAtomTable()
{
  if (NS_IsMainThread()) {
    MutexAutoLock lock(*gAtomTableLock);
    GCAtomTableLocked(lock, GCKind::RegularOperation);
  }
}

void
Atom::GCAtomTableLocked(const MutexAutoLock& aProofOfLock, GCKind aKind)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE; ++i) {
    sRecentlyUsedMainThreadAtoms[i] = nullptr;
  }

  uint32_t removedCount = 0; // Use a non-atomic temporary for cheaper increments.
  nsAutoCString nonZeroRefcountAtoms;
  uint32_t nonZeroRefcountAtomsCount = 0;
  for (auto i = gAtomTable->Iter(); !i.Done(); i.Next()) {
    auto entry = static_cast<AtomTableEntry*>(i.Get());
    if (entry->mAtom->IsStaticAtom()) {
      continue;
    }

    Atom* atom = entry->mAtom;
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

MozExternalRefCountType
Atom::DynamicAddRef()
{
  MOZ_ASSERT(IsDynamicAtom());
  nsrefcnt count = ++mRefCnt;
  if (count == 1) {
    gUnusedAtomCount--;
  }
  return count;
}

#ifdef DEBUG
// We set a lower GC threshold for atoms in debug builds so that we exercise
// the GC machinery more often.
static const uint32_t kAtomGCThreshold = 20;
#else
static const uint32_t kAtomGCThreshold = 10000;
#endif

MozExternalRefCountType
Atom::DynamicRelease()
{
  MOZ_ASSERT(IsDynamicAtom());
  MOZ_ASSERT(mRefCnt > 0);
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

  // Static Atoms aren't really refcounted. Because these entries live in a
  // global hashtable, this reference is essentially owning.
  Atom* MOZ_OWNING_REF mAtom;
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
  static nsIAtom* empty_atom = nullptr;
  static const nsStaticAtom default_atoms[] = {
    NS_STATIC_ATOM(empty, &empty_atom)
  };
  NS_RegisterStaticAtoms(default_atoms);
}

void
NS_ShutdownAtomTable()
{
#if defined(NS_BUILD_REFCNT_LOGGING)
  gFakeBuffers = nullptr;
#endif

  delete gStaticAtomTable;
  gStaticAtomTable = nullptr;

#ifdef NS_FREE_PERMANENT_DATA
  // Do a final GC to satisfy leak checking. We skip this step in release
  // builds.
  {
    MutexAutoLock lock(*gAtomTableLock);
    Atom::GCAtomTableLocked(lock, Atom::GCKind::Shutdown);
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
RegisterStaticAtoms(const nsStaticAtom* aAtoms, uint32_t aAtomCount)
{
  MutexAutoLock lock(*gAtomTableLock);

  MOZ_RELEASE_ASSERT(!gStaticAtomTableSealed,
                     "Atom table has already been sealed!");

  if (!gStaticAtomTable) {
    gStaticAtomTable = new StaticAtomTable();
  }

  for (uint32_t i = 0; i < aAtomCount; ++i) {
    nsStringBuffer* stringBuffer = aAtoms[i].mStringBuffer;
    nsIAtom** atomp = aAtoms[i].mAtom;

    MOZ_ASSERT(nsCRT::IsAscii(static_cast<char16_t*>(stringBuffer->Data())));

    uint32_t stringLen = stringBuffer->StorageSize() / sizeof(char16_t) - 1;

    uint32_t hash;
    AtomTableEntry* he =
      GetAtomHashEntry(static_cast<char16_t*>(stringBuffer->Data()),
                       stringLen, &hash);

    Atom* atom = he->mAtom;
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
      atom = Atom::CreateStatic(stringBuffer, stringLen, hash);
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

already_AddRefed<nsIAtom>
NS_Atomize(const char* aUTF8String)
{
  return NS_Atomize(nsDependentCString(aUTF8String));
}

already_AddRefed<nsIAtom>
NS_Atomize(const nsACString& aUTF8String)
{
  MutexAutoLock lock(*gAtomTableLock);
  uint32_t hash;
  AtomTableEntry* he = GetAtomHashEntry(aUTF8String.Data(),
                                        aUTF8String.Length(),
                                        &hash);

  if (he->mAtom) {
    nsCOMPtr<nsIAtom> atom = he->mAtom;

    return atom.forget();
  }

  // This results in an extra addref/release of the nsStringBuffer.
  // Unfortunately there doesn't seem to be any APIs to avoid that.
  // Actually, now there is, sort of: ForgetSharedBuffer.
  nsString str;
  CopyUTF8toUTF16(aUTF8String, str);
  RefPtr<Atom> atom = Atom::CreateDynamic(str, hash);

  he->mAtom = atom;

  return atom.forget();
}

already_AddRefed<nsIAtom>
NS_Atomize(const char16_t* aUTF16String)
{
  return NS_Atomize(nsDependentString(aUTF16String));
}

already_AddRefed<nsIAtom>
NS_Atomize(const nsAString& aUTF16String)
{
  MutexAutoLock lock(*gAtomTableLock);
  uint32_t hash;
  AtomTableEntry* he = GetAtomHashEntry(aUTF16String.Data(),
                                        aUTF16String.Length(),
                                        &hash);

  if (he->mAtom) {
    nsCOMPtr<nsIAtom> atom = he->mAtom;

    return atom.forget();
  }

  RefPtr<Atom> atom = Atom::CreateDynamic(aUTF16String, hash);
  he->mAtom = atom;

  return atom.forget();
}

already_AddRefed<nsIAtom>
NS_AtomizeMainThread(const nsAString& aUTF16String)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIAtom> retVal;
  uint32_t hash;
  AtomTableKey key(aUTF16String.Data(), aUTF16String.Length(), &hash);
  uint32_t index = hash % RECENTLY_USED_MAIN_THREAD_ATOM_CACHE_SIZE;
  Atom* atom = sRecentlyUsedMainThreadAtoms[index];
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
    RefPtr<Atom> newAtom = Atom::CreateDynamic(aUTF16String, hash);
    he->mAtom = newAtom;
    retVal = newAtom.forget();
  }

  sRecentlyUsedMainThreadAtoms[index] = he->mAtom;
  return retVal.forget();
}

nsrefcnt
NS_GetNumberOfAtoms(void)
{
  Atom::GCAtomTable(); // Trigger a GC so that we return a deterministic result.
  MutexAutoLock lock(*gAtomTableLock);
  return gAtomTable->EntryCount();
}

uint32_t
NS_GetUnusedAtomCount(void)
{
  return gUnusedAtomCount;
}

nsIAtom*
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
