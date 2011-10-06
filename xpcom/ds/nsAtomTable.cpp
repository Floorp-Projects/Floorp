/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

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

struct AtomTableEntry : public PLDHashEntryHdr {
  AtomImpl* mAtom;
};

struct AtomTableKey
{
  AtomTableKey(const PRUnichar* aUTF16String, PRUint32 aLength)
    : mUTF16String(aUTF16String),
      mUTF8String(nsnull),
      mLength(aLength)
  {
  }

  AtomTableKey(const char* aUTF8String, PRUint32 aLength)
    : mUTF16String(nsnull),
      mUTF8String(aUTF8String),
      mLength(aLength)
  {
  }

  const PRUnichar* mUTF16String;
  const char* mUTF8String;
  PRUint32 mLength;
};

static PLDHashNumber
AtomTableGetHash(PLDHashTable *table, const void *key)
{
  const AtomTableKey *k = static_cast<const AtomTableKey*>(key);

  if (k->mUTF8String) {
    bool err;
    PRUint32 hash = nsCRT::HashCodeAsUTF16(k->mUTF8String, k->mLength, &err);
    if (err) {
      AtomTableKey* mutableKey = const_cast<AtomTableKey*>(k);
      mutableKey->mUTF8String = nsnull;
      mutableKey->mLength = 0;
      hash = 0;
    }
    return hash;
  }

  return nsCRT::HashCode(k->mUTF16String, k->mLength);
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

  PRUint32 length = he->mAtom->GetLength();
  if (length != k->mLength) {
    return PR_FALSE;
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
  static_cast<AtomTableEntry*>(entry)->mAtom = nsnull;

  return PR_TRUE;
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
              PRUint32 index, void *arg)
{
  AtomTableEntry *entry = static_cast<AtomTableEntry*>(he);
  
  AtomImpl* atom = entry->mAtom;
  if (!atom->IsPermanent()) {
    ++*static_cast<PRUint32*>(arg);
    nsCAutoString str;
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
      PRUint32 leaked = 0;
      printf("*** %d atoms still exist (including permanent):\n",
             gAtomTable.entryCount);
      PL_DHashTableEnumerate(&gAtomTable, DumpAtomLeaks, &leaked);
      printf("*** %u non-permanent atoms leaked\n", leaked);
    }
#endif
    PL_DHashTableFinish(&gAtomTable);
    gAtomTable.entryCount = 0;
    gAtomTable.ops = nsnull;
  }
}

AtomImpl::AtomImpl(const nsAString& aString)
{
  mLength = aString.Length();
  nsStringBuffer* buf = nsStringBuffer::FromString(aString);
  if (buf) {
    buf->AddRef();
    mString = static_cast<PRUnichar*>(buf->Data());
  }
  else {
    buf = nsStringBuffer::Alloc((mLength + 1) * sizeof(PRUnichar));
    mString = static_cast<PRUnichar*>(buf->Data());
    CopyUnicodeTo(aString, 0, mString, mLength);
    mString[mLength] = PRUnichar(0);
  }

  NS_ASSERTION(mString[mLength] == PRUnichar(0), "null terminated");
  NS_ASSERTION(buf && buf->StorageSize() >= (mLength+1) * sizeof(PRUnichar),
               "enough storage");
  NS_ASSERTION(Equals(aString), "correct data");
}

AtomImpl::AtomImpl(nsStringBuffer* aStringBuffer, PRUint32 aLength)
{
  mLength = aLength;
  mString = static_cast<PRUnichar*>(aStringBuffer->Data());
  // Technically we could currently avoid doing this addref by instead making
  // the static atom buffers have an initial refcount of 2.
  aStringBuffer->AddRef();

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
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  return 2;
}

NS_IMETHODIMP_(nsrefcnt) PermanentAtomImpl::Release()
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  return 1;
}

/* virtual */ bool
AtomImpl::IsPermanent()
{
  return PR_FALSE;
}

/* virtual */ bool
PermanentAtomImpl::IsPermanent()
{
  return PR_TRUE;
}

void* PermanentAtomImpl::operator new ( size_t size, AtomImpl* aAtom ) CPP_THROW_NEW {
  NS_ASSERTION(!aAtom->IsPermanent(),
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

//----------------------------------------------------------------------

#define ATOM_HASHTABLE_INITIAL_SIZE  4096

static inline AtomTableEntry*
GetAtomHashEntry(const char* aString, PRUint32 aLength)
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  if (!gAtomTable.ops &&
      !PL_DHashTableInit(&gAtomTable, &AtomTableOps, 0,
                         sizeof(AtomTableEntry), ATOM_HASHTABLE_INITIAL_SIZE)) {
    gAtomTable.ops = nsnull;
    return nsnull;
  }

  AtomTableKey key(aString, aLength);
  return static_cast<AtomTableEntry*>
                    (PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_ADD));
}

static inline AtomTableEntry*
GetAtomHashEntry(const PRUnichar* aString, PRUint32 aLength)
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  if (!gAtomTable.ops &&
      !PL_DHashTableInit(&gAtomTable, &AtomTableOps, 0,
                         sizeof(AtomTableEntry), ATOM_HASHTABLE_INITIAL_SIZE)) {
    gAtomTable.ops = nsnull;
    return nsnull;
  }

  AtomTableKey key(aString, aLength);
  return static_cast<AtomTableEntry*>
                    (PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_ADD));
}

class CheckStaticAtomSizes
{
  CheckStaticAtomSizes() {
    PR_STATIC_ASSERT((sizeof(nsFakeStringBuffer<1>().mRefCnt) ==
                      sizeof(nsStringBuffer().mRefCount)) &&
                     (sizeof(nsFakeStringBuffer<1>().mSize) ==
                      sizeof(nsStringBuffer().mStorageSize)) &&
                     (offsetof(nsFakeStringBuffer<1>, mRefCnt) ==
                      offsetof(nsStringBuffer, mRefCount)) &&
                     (offsetof(nsFakeStringBuffer<1>, mSize) ==
                      offsetof(nsStringBuffer, mStorageSize)) &&
                     (offsetof(nsFakeStringBuffer<1>, mStringData) ==
                      sizeof(nsStringBuffer)));
  }
};

nsresult
NS_RegisterStaticAtoms(const nsStaticAtom* aAtoms, PRUint32 aAtomCount)
{
  // this does three things:
  // 1) wraps each static atom in a wrapper, if necessary
  // 2) initializes the address pointed to by each mBits slot
  // 3) puts the atom into the static atom table as well
  
  if (!gStaticAtomTable && !gStaticAtomTableSealed) {
    gStaticAtomTable = new nsDataHashtable<nsStringHashKey, nsIAtom*>();
    if (!gStaticAtomTable || !gStaticAtomTable->Init()) {
      delete gStaticAtomTable;
      gStaticAtomTable = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  
  for (PRUint32 i=0; i<aAtomCount; i++) {
#ifdef NS_STATIC_ATOM_USE_WIDE_STRINGS
    NS_ASSERTION(nsCRT::IsAscii((PRUnichar*)aAtoms[i].mStringBuffer->Data()),
                 "Static atoms must be ASCII!");

    PRUint32 stringLen =
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
                                             stringLen);
      he->mAtom = atom;
      *aAtoms[i].mAtom = atom;

      if (!gStaticAtomTableSealed) {
        gStaticAtomTable->Put(nsAtomString(atom), atom);
      }
    }
#else // NS_STATIC_ATOM_USE_WIDE_STRINGS
    NS_ASSERTION(nsCRT::IsAscii((char*)aAtoms[i].mStringBuffer->Data()),
                 "Static atoms must be ASCII!");

    PRUint32 stringLen = aAtoms[i].mStringBuffer->StorageSize() - 1;

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

nsIAtom*
NS_NewAtom(const char* aUTF8String)
{
  return NS_NewAtom(nsDependentCString(aUTF8String));
}

nsIAtom*
NS_NewAtom(const nsACString& aUTF8String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF8String.Data(),
                                        aUTF8String.Length());

  if (he->mAtom) {
    nsIAtom* atom;
    NS_ADDREF(atom = he->mAtom);

    return atom;
  }

  // This results in an extra addref/release of the nsStringBuffer.
  // Unfortunately there doesn't seem to be any APIs to avoid that.
  nsString str;
  CopyUTF8toUTF16(aUTF8String, str);
  AtomImpl* atom = new AtomImpl(str);

  he->mAtom = atom;
  NS_ADDREF(atom);

  return atom;
}

nsIAtom*
NS_NewAtom(const PRUnichar* aUTF16String)
{
  return NS_NewAtom(nsDependentString(aUTF16String));
}

nsIAtom*
NS_NewAtom(const nsAString& aUTF16String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF16String.Data(),
                                        aUTF16String.Length());

  if (he->mAtom) {
    nsIAtom* atom;
    NS_ADDREF(atom = he->mAtom);

    return atom;
  }

  AtomImpl* atom = new AtomImpl(aUTF16String);
  he->mAtom = atom;
  NS_ADDREF(atom);

  return atom;
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
    atom = new PermanentAtomImpl(aUTF16String);
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
    atom = nsnull;
  }
  return atom;
}

void
NS_SealStaticAtomTable()
{
  gStaticAtomTableSealed = PR_TRUE;
}
