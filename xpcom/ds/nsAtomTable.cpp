/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"

#include "nsAtomTable.h"
#include "nsStaticAtom.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUTF8Utils.h"
#include "nsCRT.h"
#include "pldhash.h"
#include "prenv.h"
#include "nsThreadUtils.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"
#include "nsUnicharUtils.h"

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

using namespace mozilla;

/**
 * The shared hash table for atom lookups.
 *
 * XXX This should be manipulated in a threadsafe way or we should make
 * sure it's only manipulated from the main thread.  Probably the latter
 * is better, since the former would hurt performance.
 *
 * If |gAtomTable.ops| is 0, then the table is uninitialized.
 */
static PLDHashTable gAtomTable;

/**
 * A hashtable of static atoms that existed at app startup. This hashtable helps 
 * nsHtml5AtomTable.
 */
static nsDataHashtable<nsStringHashKey, nsIAtom*>* gStaticAtomTable = 0;

/**
 * Whether it is still OK to add atoms to gStaticAtomTable.
 */
static bool gStaticAtomTableSealed = false;

//----------------------------------------------------------------------

/**
 * Note that AtomImpl objects are sometimes converted into PermanentAtomImpl
 * objects using placement new and just overwriting the vtable pointer.
 */

class AtomImpl : public nsIAtom {
public:
  AtomImpl(const nsAString& aString, PLDHashNumber aKeyHash);

  // This is currently only used during startup when creating a permanent atom
  // from NS_RegisterStaticAtoms
  AtomImpl(nsStringBuffer* aData, uint32_t aLength, PLDHashNumber aKeyHash);

protected:
  // This is only intended to be used when a normal atom is turned into a
  // permanent one.
  AtomImpl() {
    // We can't really assert that mString is a valid nsStringBuffer string,
    // so do the best we can do and check for some consistencies.
    NS_ASSERTION((mLength + 1) * sizeof(PRUnichar) <=
                 nsStringBuffer::FromData(mString)->StorageSize() &&
                 mString[mLength] == 0,
                 "Not initialized atom");
  }

  // We don't need a virtual destructor here because PermanentAtomImpl
  // deletions aren't handled through Release().
  ~AtomImpl();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIATOM

  enum { REFCNT_PERMANENT_SENTINEL = UINT32_MAX };

  virtual bool IsPermanent();

  // We can't use the virtual function in the base class destructor.
  bool IsPermanentInDestructor() {
    return mRefCnt == REFCNT_PERMANENT_SENTINEL;
  }

  // for |#ifdef NS_BUILD_REFCNT_LOGGING| access to reference count
  nsrefcnt GetRefCount() { return mRefCnt; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
};

/**
 * A non-refcounted implementation of nsIAtom.
 */

class PermanentAtomImpl MOZ_FINAL : public AtomImpl {
public:
  PermanentAtomImpl(const nsAString& aString, PLDHashNumber aKeyHash)
    : AtomImpl(aString, aKeyHash)
  {}
  PermanentAtomImpl(nsStringBuffer* aData, uint32_t aLength,
                    PLDHashNumber aKeyHash)
    : AtomImpl(aData, aLength, aKeyHash)
  {}
  PermanentAtomImpl()
  {}

  ~PermanentAtomImpl();
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual bool IsPermanent();

  // SizeOfIncludingThis() isn't needed -- the one inherited from AtomImpl is
  // good enough, because PermanentAtomImpl doesn't add any new data members.

  void* operator new(size_t size, AtomImpl* aAtom) CPP_THROW_NEW;
  void* operator new(size_t size) CPP_THROW_NEW
  {
    return ::operator new(size);
  }
};

//----------------------------------------------------------------------

struct AtomTableEntry : public PLDHashEntryHdr {
  AtomImpl* mAtom;
};

struct AtomTableKey
{
  AtomTableKey(const PRUnichar* aUTF16String, uint32_t aLength)
    : mUTF16String(aUTF16String),
      mUTF8String(nullptr),
      mLength(aLength)
  {
  }

  AtomTableKey(const char* aUTF8String, uint32_t aLength)
    : mUTF16String(nullptr),
      mUTF8String(aUTF8String),
      mLength(aLength)
  {
  }

  const PRUnichar* mUTF16String;
  const char* mUTF8String;
  uint32_t mLength;
};

static PLDHashNumber
AtomTableGetHash(PLDHashTable *table, const void *key)
{
  const AtomTableKey *k = static_cast<const AtomTableKey*>(key);

  if (k->mUTF8String) {
    bool err;
    uint32_t hash = HashUTF8AsUTF16(k->mUTF8String, k->mLength, &err);
    if (err) {
      AtomTableKey* mutableKey = const_cast<AtomTableKey*>(k);
      mutableKey->mUTF8String = nullptr;
      mutableKey->mLength = 0;
      hash = 0;
    }
    return hash;
  }

  return HashString(k->mUTF16String, k->mLength);
}

static bool
AtomTableMatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                  const void *key)
{
  const AtomTableEntry *he = static_cast<const AtomTableEntry*>(entry);
  const AtomTableKey *k = static_cast<const AtomTableKey*>(key);

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
                k->mUTF16String, length * sizeof(PRUnichar)) == 0;
}

static void
AtomTableClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  // Normal |AtomImpl| atoms are deleted when their refcount hits 0, and
  // they then remove themselves from the table.  In other words, they
  // are owned by the callers who own references to them.
  // |PermanentAtomImpl| permanent atoms ignore their refcount and are
  // deleted when they are removed from the table at table destruction.
  // In other words, they are owned by the atom table.

  AtomImpl *atom = static_cast<AtomTableEntry*>(entry)->mAtom;
  if (atom->IsPermanent()) {
    // Note that the cast here is important since AtomImpls doesn't have a
    // virtual dtor.
    delete static_cast<PermanentAtomImpl*>(atom);
  }
}

static bool
AtomTableInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                   const void *key)
{
  static_cast<AtomTableEntry*>(entry)->mAtom = nullptr;

  return true;
}


static const PLDHashTableOps AtomTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  AtomTableGetHash,
  AtomTableMatchKey,
  PL_DHashMoveEntryStub,
  AtomTableClearEntry,
  PL_DHashFinalizeStub,
  AtomTableInitEntry
};


#ifdef DEBUG
static PLDHashOperator
DumpAtomLeaks(PLDHashTable *table, PLDHashEntryHdr *he,
              uint32_t index, void *arg)
{
  AtomTableEntry *entry = static_cast<AtomTableEntry*>(he);
  
  AtomImpl* atom = entry->mAtom;
  if (!atom->IsPermanent()) {
    ++*static_cast<uint32_t*>(arg);
    nsAutoCString str;
    atom->ToUTF8String(str);
    fputs(str.get(), stdout);
    fputs("\n", stdout);
  }
  return PL_DHASH_NEXT;
}
#endif

static inline
void PromoteToPermanent(AtomImpl* aAtom)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  {
    nsrefcnt refcount = aAtom->GetRefCount();
    do {
      NS_LOG_RELEASE(aAtom, --refcount, "AtomImpl");
    } while (refcount);
  }
#endif
  aAtom = new (aAtom) PermanentAtomImpl();
}

void
NS_PurgeAtomTable()
{
  delete gStaticAtomTable;

  if (gAtomTable.ops) {
#ifdef DEBUG
    const char *dumpAtomLeaks = PR_GetEnv("MOZ_DUMP_ATOM_LEAKS");
    if (dumpAtomLeaks && *dumpAtomLeaks) {
      uint32_t leaked = 0;
      printf("*** %d atoms still exist (including permanent):\n",
             gAtomTable.entryCount);
      PL_DHashTableEnumerate(&gAtomTable, DumpAtomLeaks, &leaked);
      printf("*** %u non-permanent atoms leaked\n", leaked);
    }
#endif
    PL_DHashTableFinish(&gAtomTable);
    gAtomTable.entryCount = 0;
    gAtomTable.ops = nullptr;
  }
}

AtomImpl::AtomImpl(const nsAString& aString, PLDHashNumber aKeyHash)
{
  mLength = aString.Length();
  nsRefPtr<nsStringBuffer> buf = nsStringBuffer::FromString(aString);
  if (buf) {
    mString = static_cast<PRUnichar*>(buf->Data());
  } else {
    buf = nsStringBuffer::Alloc((mLength + 1) * sizeof(PRUnichar));
    mString = static_cast<PRUnichar*>(buf->Data());
    CopyUnicodeTo(aString, 0, mString, mLength);
    mString[mLength] = PRUnichar(0);
  }

  // The low bit of aKeyHash is generally useless, so shift it out
  MOZ_ASSERT(sizeof(mHash) == sizeof(PLDHashNumber));
  mHash = aKeyHash >> 1;

  NS_ASSERTION(mString[mLength] == PRUnichar(0), "null terminated");
  NS_ASSERTION(buf && buf->StorageSize() >= (mLength+1) * sizeof(PRUnichar),
               "enough storage");
  NS_ASSERTION(Equals(aString), "correct data");

  // Take ownership of buffer
  buf.forget();
}

AtomImpl::AtomImpl(nsStringBuffer* aStringBuffer, uint32_t aLength,
                   PLDHashNumber aKeyHash)
{
  mLength = aLength;
  mString = static_cast<PRUnichar*>(aStringBuffer->Data());
  // Technically we could currently avoid doing this addref by instead making
  // the static atom buffers have an initial refcount of 2.
  aStringBuffer->AddRef();

  // The low bit of aKeyHash is generally useless, so shift it out
  MOZ_ASSERT(sizeof(mHash) == sizeof(PLDHashNumber));
  mHash = aKeyHash >> 1;

  NS_ASSERTION(mString[mLength] == PRUnichar(0), "null terminated");
  NS_ASSERTION(aStringBuffer &&
               aStringBuffer->StorageSize() == (mLength+1) * sizeof(PRUnichar),
               "correct storage");
}

AtomImpl::~AtomImpl()
{
  NS_PRECONDITION(gAtomTable.ops, "uninitialized atom hashtable");
  // Permanent atoms are removed from the hashtable at shutdown, and we
  // don't want to remove them twice.  See comment above in
  // |AtomTableClearEntry|.
  if (!IsPermanentInDestructor()) {
    AtomTableKey key(mString, mLength);
    PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_REMOVE);
    if (gAtomTable.entryCount == 0) {
      PL_DHashTableFinish(&gAtomTable);
      NS_ASSERTION(gAtomTable.entryCount == 0,
                   "PL_DHashTableFinish changed the entry count");
    }
  }

  nsStringBuffer::FromData(mString)->Release();
}

NS_IMPL_ISUPPORTS1(AtomImpl, nsIAtom)

PermanentAtomImpl::~PermanentAtomImpl()
{
  // So we can tell if we were permanent while running the base class dtor.
  mRefCnt = REFCNT_PERMANENT_SENTINEL;
}

NS_IMETHODIMP_(nsrefcnt) PermanentAtomImpl::AddRef()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  return 2;
}

NS_IMETHODIMP_(nsrefcnt) PermanentAtomImpl::Release()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  return 1;
}

/* virtual */ bool
AtomImpl::IsPermanent()
{
  return false;
}

/* virtual */ bool
PermanentAtomImpl::IsPermanent()
{
  return true;
}

void* PermanentAtomImpl::operator new ( size_t size, AtomImpl* aAtom ) CPP_THROW_NEW {
  MOZ_ASSERT(!aAtom->IsPermanent(),
             "converting atom that's already permanent");

  // Just let the constructor overwrite the vtable pointer.
  return aAtom;
}

NS_IMETHODIMP 
AtomImpl::ScriptableToString(nsAString& aBuf)
{
  nsStringBuffer::FromData(mString)->ToString(mLength, aBuf);
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::ToUTF8String(nsACString& aBuf)
{
  CopyUTF16toUTF8(nsDependentString(mString, mLength), aBuf);
  return NS_OK;
}

NS_IMETHODIMP_(bool)
AtomImpl::EqualsUTF8(const nsACString& aString)
{
  return CompareUTF8toUTF16(aString,
                            nsDependentString(mString, mLength)) == 0;
}

NS_IMETHODIMP
AtomImpl::ScriptableEquals(const nsAString& aString, bool* aResult)
{
  *aResult = aString.Equals(nsDependentString(mString, mLength));
  return NS_OK;
}

NS_IMETHODIMP_(bool)
AtomImpl::IsStaticAtom()
{
  return IsPermanent();
}

size_t
AtomImpl::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) +
         nsStringBuffer::FromData(mString)->
           SizeOfIncludingThisIfUnshared(aMallocSizeOf);
}

//----------------------------------------------------------------------

static size_t
SizeOfAtomTableEntryExcludingThis(PLDHashEntryHdr *aHdr,
                                  MallocSizeOf aMallocSizeOf,
                                  void *aArg)
{
  AtomTableEntry* entry = static_cast<AtomTableEntry*>(aHdr);
  return entry->mAtom->SizeOfIncludingThis(aMallocSizeOf);
}

static size_t
SizeOfStaticAtomTableEntryExcludingThis(const nsAString& aKey,
                                        nsIAtom* const& aData,
                                        MallocSizeOf aMallocSizeOf,
                                        void* aArg)
{
  return aKey.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
NS_SizeOfAtomTablesIncludingThis(MallocSizeOf aMallocSizeOf) {
  size_t n = 0;
  if (gAtomTable.ops) {
      n += PL_DHashTableSizeOfExcludingThis(&gAtomTable,
                                            SizeOfAtomTableEntryExcludingThis,
                                            aMallocSizeOf);
  }
  if (gStaticAtomTable) {
    n += gStaticAtomTable->SizeOfIncludingThis(SizeOfStaticAtomTableEntryExcludingThis,
                                               aMallocSizeOf);
  }
  return n;
}

#define ATOM_HASHTABLE_INITIAL_SIZE  4096

static void HandleOOM()
{
  fputs("Out of memory allocating atom hashtable.\n", stderr);
  MOZ_CRASH();
}

static inline void
EnsureTableExists()
{
  if (!gAtomTable.ops &&
      !PL_DHashTableInit(&gAtomTable, &AtomTableOps, 0,
                         sizeof(AtomTableEntry), ATOM_HASHTABLE_INITIAL_SIZE)) {
    // Initialization failed.
    HandleOOM();
  }
}

static inline AtomTableEntry*
GetAtomHashEntry(const char* aString, uint32_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  EnsureTableExists();
  AtomTableKey key(aString, aLength);
  AtomTableEntry* e =
    static_cast<AtomTableEntry*>
               (PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_ADD));
  if (!e) {
    HandleOOM();
  }
  return e;
}

static inline AtomTableEntry*
GetAtomHashEntry(const PRUnichar* aString, uint32_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  EnsureTableExists();
  AtomTableKey key(aString, aLength);
  AtomTableEntry* e =
    static_cast<AtomTableEntry*>
               (PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_ADD));
  if (!e) {
    HandleOOM();
  }
  return e;
}

class CheckStaticAtomSizes
{
  CheckStaticAtomSizes() {
    MOZ_STATIC_ASSERT((sizeof(nsFakeStringBuffer<1>().mRefCnt) ==
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

nsresult
RegisterStaticAtoms(const nsStaticAtom* aAtoms, uint32_t aAtomCount)
{
  // this does three things:
  // 1) wraps each static atom in a wrapper, if necessary
  // 2) initializes the address pointed to by each mBits slot
  // 3) puts the atom into the static atom table as well
  
  if (!gStaticAtomTable && !gStaticAtomTableSealed) {
    gStaticAtomTable = new nsDataHashtable<nsStringHashKey, nsIAtom*>();
    if (!gStaticAtomTable) {
      delete gStaticAtomTable;
      gStaticAtomTable = nullptr;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    gStaticAtomTable->Init();
  }
  
  for (uint32_t i=0; i<aAtomCount; i++) {
#ifdef NS_STATIC_ATOM_USE_WIDE_STRINGS
    NS_ASSERTION(nsCRT::IsAscii((PRUnichar*)aAtoms[i].mStringBuffer->Data()),
                 "Static atoms must be ASCII!");

    uint32_t stringLen =
      aAtoms[i].mStringBuffer->StorageSize() / sizeof(PRUnichar) - 1;

    AtomTableEntry *he =
      GetAtomHashEntry((PRUnichar*)aAtoms[i].mStringBuffer->Data(),
                       stringLen);

    if (he->mAtom) {
      // there already is an atom with this name in the table.. but we
      // still have to update mBits
      if (!he->mAtom->IsPermanent()) {
        // since we wanted to create a static atom but there is
        // already one there, we convert it to a non-refcounting
        // permanent atom
        PromoteToPermanent(he->mAtom);
      }
      
      *aAtoms[i].mAtom = he->mAtom;
    }
    else {
      AtomImpl* atom = new PermanentAtomImpl(aAtoms[i].mStringBuffer,
                                             stringLen,
                                             he->keyHash);
      he->mAtom = atom;
      *aAtoms[i].mAtom = atom;

      if (!gStaticAtomTableSealed) {
        gStaticAtomTable->Put(nsAtomString(atom), atom);
      }
    }
#else // NS_STATIC_ATOM_USE_WIDE_STRINGS
    NS_ASSERTION(nsCRT::IsAscii((char*)aAtoms[i].mStringBuffer->Data()),
                 "Static atoms must be ASCII!");

    uint32_t stringLen = aAtoms[i].mStringBuffer->StorageSize() - 1;

    NS_ConvertASCIItoUTF16 str((char*)aAtoms[i].mStringBuffer->Data(),
                               stringLen);
    nsIAtom* atom = NS_NewPermanentAtom(str);
    *aAtoms[i].mAtom = atom;

    if (!gStaticAtomTableSealed) {
      gStaticAtomTable->Put(str, atom);
    }
#endif

  }
  return NS_OK;
}

already_AddRefed<nsIAtom>
NS_NewAtom(const char* aUTF8String)
{
  return NS_NewAtom(nsDependentCString(aUTF8String));
}

already_AddRefed<nsIAtom>
NS_NewAtom(const nsACString& aUTF8String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF8String.Data(),
                                        aUTF8String.Length());

  if (he->mAtom) {
    nsCOMPtr<nsIAtom> atom = he->mAtom;

    return atom.forget();
  }

  // This results in an extra addref/release of the nsStringBuffer.
  // Unfortunately there doesn't seem to be any APIs to avoid that.
  // Actually, now there is, sort of: ForgetSharedBuffer.
  nsString str;
  CopyUTF8toUTF16(aUTF8String, str);
  nsRefPtr<AtomImpl> atom = new AtomImpl(str, he->keyHash);

  he->mAtom = atom;

  return atom.forget();
}

already_AddRefed<nsIAtom>
NS_NewAtom(const PRUnichar* aUTF16String)
{
  return NS_NewAtom(nsDependentString(aUTF16String));
}

already_AddRefed<nsIAtom>
NS_NewAtom(const nsAString& aUTF16String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF16String.Data(),
                                        aUTF16String.Length());

  if (he->mAtom) {
    nsCOMPtr<nsIAtom> atom = he->mAtom;

    return atom.forget();
  }

  nsRefPtr<AtomImpl> atom = new AtomImpl(aUTF16String, he->keyHash);
  he->mAtom = atom;

  return atom.forget();
}

nsIAtom*
NS_NewPermanentAtom(const nsAString& aUTF16String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF16String.Data(),
                                        aUTF16String.Length());

  AtomImpl* atom = he->mAtom;
  if (atom) {
    if (!atom->IsPermanent()) {
      PromoteToPermanent(atom);
    }
  }
  else {
    atom = new PermanentAtomImpl(aUTF16String, he->keyHash);
    he->mAtom = atom;
  }

  // No need to addref since permanent atoms aren't refcounted anyway
  return atom;
}

nsrefcnt
NS_GetNumberOfAtoms(void)
{
  return gAtomTable.entryCount;
}

nsIAtom*
NS_GetStaticAtom(const nsAString& aUTF16String)
{
  NS_PRECONDITION(gStaticAtomTable, "Static atom table not created yet.");
  NS_PRECONDITION(gStaticAtomTableSealed, "Static atom table not sealed yet.");
  nsIAtom* atom;
  if (!gStaticAtomTable->Get(aUTF16String, &atom)) {
    atom = nullptr;
  }
  return atom;
}

void
NS_SealStaticAtomTable()
{
  gStaticAtomTableSealed = true;
}
