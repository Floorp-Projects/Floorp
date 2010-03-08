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
static PRBool gStaticAtomTableSealed = PR_FALSE;

// The |key| pointer in the various PLDHashTable callbacks we use is an
// AtomTableClearEntry*.  These pointers can come from two places: either a
// (probably stack-allocated) string key being passed to PL_DHashTableOperate,
// or an actual entry in the atom table. PLDHashTable reseves the keyHash
// values 0 and 1 for internal use, which means that the *PLDHashTable code*
// will never pass an entry whose keyhash is 0 or 1 to our hooks. That means we
// can use those values to tell whether an AtomTableEntry is a string key
// created by a PLDHashTable code caller or an actual live AtomTableEntry used
// by our PLDHashTable.
//
// Evil? Yes, but kinda neat too :-)
//
// An AtomTableEntry is a UTF-8 string key if keyHash is 0, in that
// case mBits points to a UTF-8 encoded char *. If keyHash is 1 the
// AtomTableEntry is a UTF-16 encoded string key and mBits points to a
// UTF-16 encoded PRUnichar *.
//
// If keyHash is any other value (> 1), the AtomTableEntry is an
// actual live entry in the table, and then mBits & ~0x1 in the
// AtomTableEntry points to an AtomImpl or a nsStaticAtomWrapper,
// indicated by the first bit of PtrBits.
// XXX This whole mess could be vastly simplified now that pldhash
// no longer has a getKey callback.
typedef PRUword PtrBits;

struct AtomTableEntry : public PLDHashEntryHdr {
  // If keyHash > 1, mBits & 0x1 means (mBits & ~0x1) points to an
  // nsStaticAtomWrapper else it points to an nsAtomImpl
  PtrBits mBits;
  PRUint32 mLength;

  inline AtomTableEntry(const char *aString, PRUint32 aLength)
    : mBits(PtrBits(aString)), mLength(aLength)
  {
    keyHash = 0;
  }

  inline AtomTableEntry(const PRUnichar *aString, PRUint32 aLength)
    : mBits(PtrBits(aString)), mLength(aLength)
  {
    keyHash = 1;
  }

  inline PRBool IsUTF8String() const {
    return keyHash == 0;
  }

  inline PRBool IsUTF16String() const {
    return keyHash == 1;
  }

  inline void SetAtomImpl(AtomImpl* aAtom) {
    NS_ASSERTION(keyHash > 1,
                 "SetAtomImpl() called on non-atom AtomTableEntry!");
    NS_ASSERTION(aAtom, "Setting null atom");
    mBits = PtrBits(aAtom);
    mLength = aAtom->mLength;
  }

  inline void ClearAtom() {
    mBits = nsnull;
  }

  inline PRBool HasValue() const {
    NS_ASSERTION(keyHash > 1,
                 "HasValue() called on non-atom AtomTableEntry!");
    return (mBits & ~0x1) != 0;
  }

  // these accessors assume that you already know the type
  inline AtomImpl *GetAtomImpl() const {
    NS_ASSERTION(keyHash > 1,
                 "GetAtomImpl() called on non-atom AtomTableEntry!");
    return (AtomImpl*) (mBits & ~0x1);
  }

  // type-agnostic accessors

  // get the string buffer
  inline const char* getAtomString() const {
    NS_ASSERTION(keyHash > 1,
                 "getAtomString() called on non-atom AtomTableEntry!");

    return GetAtomImpl()->mString;
  }

  // get the string buffer
  inline const char* getUTF8String() const {
    NS_ASSERTION(keyHash == 0,
                 "getUTF8String() called on non-UTF8 AtomTableEntry!");

    return (char *)mBits;
  }

  // get the string buffer
  inline const PRUnichar* getUTF16String() const {
    NS_ASSERTION(keyHash == 1,
                 "getUTF16String() called on non-UTF16 AtomTableEntry!");

    return (PRUnichar *)mBits;
  }

  // get the string length
  inline PRUint32 getLength() const {
    return mLength;
  }

  // get an addreffed nsIAtom - not using already_AddRef'ed atom
  // because the callers are not (and should not be) using nsCOMPtr
  inline nsIAtom* GetAtom() const {
    NS_ASSERTION(keyHash > 1,
                 "GetAtom() called on non-atom AtomTableEntry!");

    nsIAtom* result;
    
    NS_ADDREF(result = GetAtomImpl());
    
    return result;
  }
};

static PLDHashNumber
AtomTableGetHash(PLDHashTable *table, const void *key)
{
  const AtomTableEntry *e = static_cast<const AtomTableEntry*>(key);

  if (e->IsUTF16String()) {
    return nsCRT::HashCodeAsUTF8(e->getUTF16String(), e->getLength());;
  }

  NS_ASSERTION(e->IsUTF8String(),
               "AtomTableGetHash() called on non-string-key AtomTableEntry!");

  return nsCRT::HashCode(e->getUTF8String(), e->getLength());
}

static PRBool
AtomTableMatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                  const void *key)
{
  const AtomTableEntry *he = static_cast<const AtomTableEntry*>(entry);
  const AtomTableEntry *strKey = static_cast<const AtomTableEntry*>(key);

  const char *atomString = he->getAtomString();

  if (strKey->IsUTF16String()) {
    return
      CompareUTF8toUTF16(nsDependentCSubstring(atomString, atomString + he->getLength()),
                         nsDependentSubstring(strKey->getUTF16String(),
                                              strKey->getUTF16String() + strKey->getLength())) == 0;
  }

  PRUint32 length = he->getLength();
  if (length != strKey->getLength()) {
    return PR_FALSE;
  }

  const char *str;

  if (strKey->IsUTF8String()) {
    str = strKey->getUTF8String();
  } else {
    str = strKey->getAtomString();
  }

  return memcmp(atomString, str, length * sizeof(char)) == 0;
}

static void
AtomTableClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  AtomTableEntry *he = static_cast<AtomTableEntry*>(entry);
  
  AtomImpl *atom = he->GetAtomImpl();
  // Normal |AtomImpl| atoms are deleted when their refcount hits 0, and
  // they then remove themselves from the table.  In other words, they
  // are owned by the callers who own references to them.
  // |PermanentAtomImpl| permanent atoms ignore their refcount and are
  // deleted when they are removed from the table at table destruction.
  // In other words, they are owned by the atom table.
  if (atom->IsPermanent()) {
    he->keyHash = 0;

    delete static_cast<PermanentAtomImpl*>(atom);
  }
  
  he->ClearAtom();
}

static PRBool
AtomTableInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                   const void *key)
{
  AtomTableEntry *he = static_cast<AtomTableEntry*>(entry);
  const AtomTableEntry *strKey = static_cast<const AtomTableEntry*>(key);

  he->mLength = strKey->getLength();

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
  
  AtomImpl* atom = entry->GetAtomImpl();
  if (!atom->IsPermanent()) {
    ++*static_cast<PRUint32*>(arg);
    const char *str;
    atom->GetUTF8String(&str);
    fputs(str, stdout);
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

AtomImpl::AtomImpl(const nsACString& aString)
  : mLength(aString.Length())
{
  nsStringBuffer* buf = nsStringBuffer::FromString(aString);
  if (buf) {
    buf->AddRef();
    mString = static_cast<char*>(buf->Data());
  }
  else {
    buf = nsStringBuffer::Alloc((mLength + 1) * sizeof(char));
    mString = static_cast<char*>(buf->Data());
    memcpy(mString, aString.BeginReading(), mLength * sizeof(char));
    mString[mLength] = PRUnichar(0);
  }
}

AtomImpl::AtomImpl(nsStringBuffer* aStringBuffer, PRUint32 aLength)
  : mLength(aLength),
    mString(static_cast<char*>(aStringBuffer->Data()))
{
  // Technically we could currently avoid doing this addref by instead making
  // the static atom buffers have an initial refcount of 2.
  aStringBuffer->AddRef();
}

AtomImpl::~AtomImpl()
{
  NS_PRECONDITION(gAtomTable.ops, "uninitialized atom hashtable");
  // Permanent atoms are removed from the hashtable at shutdown, and we
  // don't want to remove them twice.  See comment above in
  // |AtomTableClearEntry|.
  if (!IsPermanentInDestructor()) {
    AtomTableEntry key(mString, mLength);
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

/* virtual */ PRBool
AtomImpl::IsPermanent()
{
  return PR_FALSE;
}

/* virtual */ PRBool
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
AtomImpl::ToString(nsAString& aBuf)
{
  CopyUTF8toUTF16(nsDependentCString(mString, mLength), aBuf);
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::ToUTF8String(nsACString& aBuf)
{
  nsStringBuffer::FromData(mString)->ToString(mLength, aBuf);
  return NS_OK;
}

NS_IMETHODIMP 
AtomImpl::GetUTF8String(const char **aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  *aResult = mString;
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::EqualsUTF8(const nsACString& aString, PRBool* aResult)
{
  *aResult = aString.Equals(nsDependentCString(mString, mLength));
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::Equals(const nsAString& aString, PRBool* aResult)
{
  *aResult = CompareUTF8toUTF16(nsDependentCString(mString, mLength),
                                aString) == 0;
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
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

  AtomTableEntry key(aString, aLength);
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

  AtomTableEntry key(aString, aLength);
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

NS_COM nsresult
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
    NS_ASSERTION(nsCRT::IsAscii((char*)aAtoms[i].mStringBuffer->Data()),
                 "Static atoms must be ASCII!");

    PRUint32 stringLen =
      aAtoms[i].mStringBuffer->StorageSize() / sizeof(char) - 1;

    AtomTableEntry *he =
      GetAtomHashEntry((char*)aAtoms[i].mStringBuffer->Data(),
                       stringLen);

    if (he->HasValue()) {
      // there already is an atom with this name in the table.. but we
      // still have to update mBits
      if (!he->GetAtomImpl()->IsPermanent()) {
        // since we wanted to create a static atom but there is
        // already one there, we convert it to a non-refcounting
        // permanent atom
        PromoteToPermanent(he->GetAtomImpl());
      }
      
      *aAtoms[i].mAtom = he->GetAtom();
    }
    else {
      AtomImpl* atom = new PermanentAtomImpl(aAtoms[i].mStringBuffer,
                                             stringLen);
      he->SetAtomImpl(atom);
      *aAtoms[i].mAtom = atom;

      if (!gStaticAtomTableSealed) {
        nsAutoString key;
        atom->ToString(key);
        gStaticAtomTable->Put(key, atom);
      }
    }
  }
  return NS_OK;
}

NS_COM nsIAtom*
NS_NewAtom(const char* aUTF8String)
{
  return NS_NewAtom(nsDependentCString(aUTF8String));
}

NS_COM nsIAtom*
NS_NewAtom(const nsACString& aUTF8String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF8String.Data(),
                                        aUTF8String.Length());

  if (!he) {
    return nsnull;
  }

  NS_ASSERTION(!he->IsUTF8String() && !he->IsUTF16String(),
               "Atom hash entry is string?  Should be atom!");

  if (he->HasValue())
    return he->GetAtom();

  AtomImpl* atom = new AtomImpl(aUTF8String);
  he->SetAtomImpl(atom);
  NS_ADDREF(atom);

  return atom;
}

NS_COM nsIAtom*
NS_NewAtom(const PRUnichar* aUTF16String)
{
  return NS_NewAtom(nsDependentString(aUTF16String));
}

NS_COM nsIAtom*
NS_NewAtom(const nsAString& aUTF16String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF16String.Data(),
                                        aUTF16String.Length());

  if (he->HasValue())
    return he->GetAtom();

  // This results in an extra addref/release of the nsStringBuffer.
  // Unfortunately there doesn't seem to be any APIs to avoid that.
  nsCString str;
  CopyUTF16toUTF8(aUTF16String, str);
  AtomImpl* atom = new AtomImpl(str);

  he->SetAtomImpl(atom);
  NS_ADDREF(atom);

  return atom;
}

NS_COM nsIAtom*
NS_NewPermanentAtom(const nsACString& aUTF8String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF8String.Data(),
                                        aUTF8String.Length());

  if (he->HasValue()) {
    AtomImpl* atom = he->GetAtomImpl();
    if (!atom->IsPermanent()) {
      PromoteToPermanent(atom);
    }
    return atom;
  }
  
  AtomImpl* atom = new PermanentAtomImpl(aUTF8String);
  he->SetAtomImpl(atom);

  // No need to addref since permanent atoms aren't refcounted anyway

  return atom;
}

NS_COM nsrefcnt
NS_GetNumberOfAtoms(void)
{
  return gAtomTable.entryCount;
}

NS_COM nsIAtom*
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

NS_COM void
NS_SealStaticAtomTable()
{
  gStaticAtomTableSealed = PR_TRUE;
}
