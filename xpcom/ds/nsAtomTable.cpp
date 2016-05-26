/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/unused.h"

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

// There are two kinds of atoms handled by this module.
//
// - DynamicAtom: the atom itself is heap allocated, as is the nsStringBuffer it
//   points to. |gAtomTable| holds weak references to them DynamicAtoms, and
//   when they are destroyed (due to their refcount reaching zero) they remove
//   themselves from |gAtomTable|.
//
// - StaticAtom: the atom itself is heap allocated, but it points to a static
//   nsStringBuffer. |gAtomTable| effectively owns StaticAtoms, because such
//   atoms ignore all AddRef/Release calls, which ensures they stay alive until
//   |gAtomTable| itself is destroyed whereupon they are explicitly deleted.

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

class DynamicAtom final : public nsIAtom
{
public:
  DynamicAtom(const nsAString& aString, uint32_t aHash)
  {
    mLength = aString.Length();
    mIsStatic = false;
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

private:
  // We don't need a virtual destructor because we always delete via a
  // DynamicAtom* pointer (in Release(), defined via NS_IMPL_ISUPPORTS), not an
  // nsIAtom* pointer.
  ~DynamicAtom();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIATOM

  void TransmuteToStatic(nsStringBuffer* aStringBuffer);
};

NS_IMPL_ISUPPORTS(DynamicAtom, nsIAtom)

class StaticAtom final : public nsIAtom
{
  // This is the function that calls the private constructor.
  friend void DynamicAtom::TransmuteToStatic(nsStringBuffer*);

  // This constructor must only be used in conjunction with placement new on an
  // existing DynamicAtom (in DynamicAtom::TransmuteToStatic()) in order to
  // transmute that DynamicAtom into a StaticAtom. The constructor does four
  // notable things.
  // - Overwrites the vtable pointer (implicitly).
  // - Inverts mIsStatic.
  // - Zeroes the refcount (via the nsIAtom constructor). Having a zero refcount
  //   doesn't matter because StaticAtom's AddRef/Release methods don't consult
  //   the refcount.
  // - Releases the existing heap-allocated string buffer (explicitly),
  //   replacing it with the static string buffer (which must contain identical
  //   chars).
  explicit StaticAtom(nsStringBuffer* aStaticBuffer)
  {
    static_assert(sizeof(DynamicAtom) >= sizeof(StaticAtom),
                  "can't safely transmute a smaller object to a bigger one");

    // We must be transmuting an existing DynamicAtom.
    MOZ_ASSERT(!mIsStatic);
    mIsStatic = true;

    char16_t* staticString = static_cast<char16_t*>(aStaticBuffer->Data());
    MOZ_ASSERT(nsCRT::strcmp(staticString, mString) == 0);
    nsStringBuffer* dynamicBuffer = nsStringBuffer::FromData(mString);
    mString = staticString;
    dynamicBuffer->Release();
  }

public:
  // This is the normal constructor.
  StaticAtom(nsStringBuffer* aStringBuffer, uint32_t aLength, uint32_t aHash)
  {
    mLength = aLength;
    mIsStatic = true;
    mString = static_cast<char16_t*>(aStringBuffer->Data());
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

  // We don't need a virtual destructor because we always delete via a
  // StaticAtom* pointer (in AtomTableClearEntry()), not an nsIAtom* pointer.
  ~StaticAtom() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIATOM
};

NS_IMPL_QUERY_INTERFACE(StaticAtom, nsIAtom)

NS_IMETHODIMP_(MozExternalRefCountType)
StaticAtom::AddRef()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
StaticAtom::Release()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  return 1;
}

NS_IMETHODIMP
DynamicAtom::ScriptableToString(nsAString& aBuf)
{
  nsStringBuffer::FromData(mString)->ToString(mLength, aBuf);
  return NS_OK;
}

NS_IMETHODIMP
StaticAtom::ScriptableToString(nsAString& aBuf)
{
  nsStringBuffer::FromData(mString)->ToString(mLength, aBuf);
  return NS_OK;
}

NS_IMETHODIMP
DynamicAtom::ToUTF8String(nsACString& aBuf)
{
  CopyUTF16toUTF8(nsDependentString(mString, mLength), aBuf);
  return NS_OK;
}

NS_IMETHODIMP
StaticAtom::ToUTF8String(nsACString& aBuf)
{
  CopyUTF16toUTF8(nsDependentString(mString, mLength), aBuf);
  return NS_OK;
}

NS_IMETHODIMP
DynamicAtom::ScriptableEquals(const nsAString& aString, bool* aResult)
{
  *aResult = aString.Equals(nsDependentString(mString, mLength));
  return NS_OK;
}

NS_IMETHODIMP
StaticAtom::ScriptableEquals(const nsAString& aString, bool* aResult)
{
  *aResult = aString.Equals(nsDependentString(mString, mLength));
  return NS_OK;
}

NS_IMETHODIMP_(size_t)
DynamicAtom::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);
  n += nsStringBuffer::FromData(mString)->SizeOfIncludingThisIfUnshared(
         aMallocSizeOf);
  return n;
}

NS_IMETHODIMP_(size_t)
StaticAtom::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);
  // Don't measure the string buffer pointed to by the StaticAtom because it's
  // in static memory.
  return n;
}

// See the comment on the private StaticAtom constructor for details of how
// this works.
void
DynamicAtom::TransmuteToStatic(nsStringBuffer* aStringBuffer)
{
  new (this) StaticAtom(aStringBuffer);
}

//----------------------------------------------------------------------

/**
 * The shared hash table for atom lookups.
 *
 * XXX This should be manipulated in a threadsafe way or we should make
 * sure it's only manipulated from the main thread.  Probably the latter
 * is better, since the former would hurt performance.
 */
static PLDHashTable* gAtomTable;

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
  // These references are either to DynamicAtoms, in which case they are
  // non-owning, or they are to StaticAtoms, which aren't really refcounted.
  // See the comment at the top of this file for more details.
  nsIAtom* MOZ_NON_OWNING_REF mAtom;
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

  uint32_t length = he->mAtom->GetLength();
  if (length != k->mLength) {
    return false;
  }

  return memcmp(he->mAtom->GetUTF16String(),
                k->mUTF16String, length * sizeof(char16_t)) == 0;
}

static void
AtomTableClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  auto entry = static_cast<AtomTableEntry*>(aEntry);
  nsIAtom* atom = entry->mAtom;
  if (atom->IsStaticAtom()) {
    // This case -- when the entry being cleared holds a StaticAtom -- only
    // occurs when gAtomTable is destroyed, whereupon all StaticAtoms within it
    // must be explicitly deleted. The cast is required because StaticAtom
    // doesn't have a virtual destructor.
    delete static_cast<StaticAtom*>(atom);
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

static inline void
EnsureTableExists()
{
  if (!gAtomTable) {
    gAtomTable = new PLDHashTable(&AtomTableOps, sizeof(AtomTableEntry),
                                  ATOM_HASHTABLE_INITIAL_LENGTH);
  }
}

//----------------------------------------------------------------------

DynamicAtom::~DynamicAtom()
{
  MOZ_ASSERT(gAtomTable, "uninitialized atom hashtable");

  // DynamicAtoms must be removed from gAtomTable when their refcount reaches
  // zero and they are released.
  AtomTableKey key(mString, mLength, mHash);
  gAtomTable->Remove(&key);

  nsStringBuffer::FromData(mString)->Release();
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

  // StaticAtoms aren't really refcounted. Because these entries live in a
  // global hashtable, this reference is essentially owning.
  StaticAtom* MOZ_OWNING_REF mAtom;
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

//----------------------------------------------------------------------

void
NS_PurgeAtomTable()
{
  delete gStaticAtomTable;
  gStaticAtomTable = nullptr;

  if (gAtomTable) {
    // XXXbholley: it would be good to assert gAtomTable->EntryCount() == 0
    // here, but that currently fails. Probably just a few things that need
    // to be fixed up.
    delete gAtomTable;
    gAtomTable = nullptr;
  }
}

void
NS_SizeOfAtomTablesIncludingThis(MallocSizeOf aMallocSizeOf,
                                 size_t* aMain, size_t* aStatic)
{
  *aMain = 0;
  if (gAtomTable) {
    *aMain += gAtomTable->ShallowSizeOfIncludingThis(aMallocSizeOf);
    for (auto iter = gAtomTable->Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<AtomTableEntry*>(iter.Get());
      *aMain += entry->mAtom->SizeOfIncludingThis(aMallocSizeOf);
    }
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
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  EnsureTableExists();
  AtomTableKey key(aString, aLength, aHashOut);
  // This is an infallible add.
  return static_cast<AtomTableEntry*>(gAtomTable->Add(&key));
}

static inline AtomTableEntry*
GetAtomHashEntry(const char16_t* aString, uint32_t aLength, uint32_t* aHashOut)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  EnsureTableExists();
  AtomTableKey key(aString, aLength, aHashOut);
  // This is an infallible add.
  return static_cast<AtomTableEntry*>(gAtomTable->Add(&key));
}

void
RegisterStaticAtoms(const nsStaticAtom* aAtoms, uint32_t aAtomCount)
{
  if (!gStaticAtomTable && !gStaticAtomTableSealed) {
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

    nsIAtom* atom = he->mAtom;
    if (atom) {
      if (!atom->IsStaticAtom()) {
        // A rare case: we're creating a StaticAtom but there is already a
        // DynamicAtom of the same name. Transmute the DynamicAtom into a
        // StaticAtom.
        static_cast<DynamicAtom*>(atom)->TransmuteToStatic(stringBuffer);
      }
    } else {
      atom = new StaticAtom(stringBuffer, stringLen, hash);
      he->mAtom = atom;
    }
    *atomp = atom;

    if (!gStaticAtomTableSealed) {
      StaticAtomEntry* entry =
        gStaticAtomTable->PutEntry(nsDependentAtomString(atom));
      MOZ_ASSERT(atom->IsStaticAtom());
      entry->mAtom = static_cast<StaticAtom*>(atom);
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
  RefPtr<DynamicAtom> atom = new DynamicAtom(str, hash);

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
  uint32_t hash;
  AtomTableEntry* he = GetAtomHashEntry(aUTF16String.Data(),
                                        aUTF16String.Length(),
                                        &hash);

  if (he->mAtom) {
    nsCOMPtr<nsIAtom> atom = he->mAtom;

    return atom.forget();
  }

  RefPtr<DynamicAtom> atom = new DynamicAtom(aUTF16String, hash);
  he->mAtom = atom;

  return atom.forget();
}

nsrefcnt
NS_GetNumberOfAtoms(void)
{
  MOZ_ASSERT(gAtomTable);
  return gAtomTable->EntryCount();
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
