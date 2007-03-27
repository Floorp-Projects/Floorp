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
#include "nsCRT.h"
#include "pldhash.h"
#include "prenv.h"

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

class nsStaticAtomWrapper;

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

// this is where we keep the nsStaticAtomWrapper objects

static PLArenaPool* gStaticAtomArena = 0;

class nsStaticAtomWrapper : public nsIAtom
{
public:
  nsStaticAtomWrapper(const nsStaticAtom* aAtom) :
    mStaticAtom(aAtom)
  {
    MOZ_COUNT_CTOR(nsStaticAtomWrapper);
  }
  ~nsStaticAtomWrapper() {   // no subclasses -> not virtual
    // this is arena allocated and won't be called except in debug
    // builds. If this function ever does anything non-debug, be sure
    // to get rid of the ifdefs in AtomTableClearEntry!
    MOZ_COUNT_DTOR(nsStaticAtomWrapper);
  }

  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_DECL_NSIATOM

  const nsStaticAtom* GetStaticAtom() {
    return mStaticAtom;
  }
private:
  const nsStaticAtom* mStaticAtom;
};

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

  inline AtomTableEntry(const char *aString)
    : mBits(PtrBits(aString))
  {
    keyHash = 0;
  }

  inline AtomTableEntry(const PRUnichar *aString)
    : mBits(PtrBits(aString))
  {
    keyHash = 1;
  }

  inline PRBool IsStaticAtom() const {
    NS_ASSERTION(keyHash > 1,
                 "IsStaticAtom() called on non-atom AtomTableEntry!");
    return (mBits & 0x1) != 0;
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
  }

  inline void SetStaticAtomWrapper(nsStaticAtomWrapper* aAtom) {
    NS_ASSERTION(keyHash > 1,
                 "SetStaticAtomWrapper() called on non-atom AtomTableEntry!");
    NS_ASSERTION(aAtom, "Setting null atom");
    NS_ASSERTION((PtrBits(aAtom) & ~0x1) == PtrBits(aAtom),
                 "Pointers must align or this is broken");

    mBits = PtrBits(aAtom) | 0x1;
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
    NS_ASSERTION(!IsStaticAtom(), "This is a static atom, not an AtomImpl");
    return (AtomImpl*) (mBits & ~0x1);
  }
  
  inline nsStaticAtomWrapper *GetStaticAtomWrapper() const {
    NS_ASSERTION(keyHash > 1,
                 "GetStaticAtomWrapper() called on non-atom AtomTableEntry!");
    NS_ASSERTION(IsStaticAtom(), "This is an AtomImpl, not a static atom");
    return (nsStaticAtomWrapper*) (mBits & ~0x1);
  }

  inline const nsStaticAtom* GetStaticAtom() const {
    NS_ASSERTION(keyHash > 1,
                 "GetStaticAtom() called on non-atom AtomTableEntry!");
    return GetStaticAtomWrapper()->GetStaticAtom();
  }

  // type-agnostic accessors

  // get the string buffer
  inline const char* getAtomString() const {
    NS_ASSERTION(keyHash > 1,
                 "getAtomString() called on non-atom AtomTableEntry!");

    return IsStaticAtom() ? GetStaticAtom()->mString : GetAtomImpl()->mString;
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

  // get an addreffed nsIAtom - not using already_AddRef'ed atom
  // because the callers are not (and should not be) using nsCOMPtr
  inline nsIAtom* GetAtom() const {
    NS_ASSERTION(keyHash > 1,
                 "GetAtom() called on non-atom AtomTableEntry!");

    nsIAtom* result;
    
    if (IsStaticAtom())
      result = GetStaticAtomWrapper();
    else {
      result = GetAtomImpl();
      NS_ADDREF(result);
    }
    
    return result;
  }
};

PR_STATIC_CALLBACK(PLDHashNumber)
AtomTableGetHash(PLDHashTable *table, const void *key)
{
  const AtomTableEntry *e = NS_STATIC_CAST(const AtomTableEntry*, key);

  if (e->IsUTF16String()) {
    return nsCRT::HashCodeAsUTF8(e->getUTF16String());
  }

  NS_ASSERTION(e->IsUTF8String(),
               "AtomTableGetHash() called on non-string-key AtomTableEntry!");

  return nsCRT::HashCode(e->getUTF8String());
}

PR_STATIC_CALLBACK(PRBool)
AtomTableMatchKey(PLDHashTable *table, const PLDHashEntryHdr *entry,
                  const void *key)
{
  const AtomTableEntry *he = NS_STATIC_CAST(const AtomTableEntry*, entry);
  const AtomTableEntry *strKey = NS_STATIC_CAST(const AtomTableEntry*, key);

  const char *atomString = he->getAtomString();

  if (strKey->IsUTF16String()) {
    return
      CompareUTF8toUTF16(nsDependentCString(atomString),
                         nsDependentString(strKey->getUTF16String())) == 0;
  }

  if (strKey->IsUTF8String()) {
    return strcmp(atomString, strKey->getUTF8String()) == 0;
  }

  return strcmp(atomString, strKey->getAtomString()) == 0;
}

PR_STATIC_CALLBACK(void)
AtomTableClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  AtomTableEntry *he = NS_STATIC_CAST(AtomTableEntry*, entry);
  
  if (!he->IsStaticAtom()) {
    AtomImpl *atom = he->GetAtomImpl();
    // Normal |AtomImpl| atoms are deleted when their refcount hits 0, and
    // they then remove themselves from the table.  In other words, they
    // are owned by the callers who own references to them.
    // |PermanentAtomImpl| permanent atoms ignore their refcount and are
    // deleted when they are removed from the table at table destruction.
    // In other words, they are owned by the atom table.
    if (atom->IsPermanent()) {
      he->keyHash = 0;

      delete NS_STATIC_CAST(PermanentAtomImpl*, atom);
    }
  }
  else {
    he->GetStaticAtomWrapper()->~nsStaticAtomWrapper();
  }
  
  he->ClearAtom();
}

static const PLDHashTableOps AtomTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  AtomTableGetHash,
  AtomTableMatchKey,
  PL_DHashMoveEntryStub,
  AtomTableClearEntry,
  PL_DHashFinalizeStub,
  NULL
};


#ifdef DEBUG

PR_STATIC_CALLBACK(PLDHashOperator)
DumpAtomLeaks(PLDHashTable *table, PLDHashEntryHdr *he,
              PRUint32 index, void *arg)
{
  AtomTableEntry *entry = NS_STATIC_CAST(AtomTableEntry*, he);
  
  if (entry->IsStaticAtom())
    return PL_DHASH_NEXT;
  
  AtomImpl* atom = entry->GetAtomImpl();
  if (!atom->IsPermanent()) {
    ++*NS_STATIC_CAST(PRUint32*, arg);
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
  if (gAtomTable.ops) {
#ifdef DEBUG
    if (PR_GetEnv("MOZ_DUMP_ATOM_LEAKS")) {
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

    if (gStaticAtomArena) {
      PL_FinishArenaPool(gStaticAtomArena);
      delete gStaticAtomArena;
      gStaticAtomArena = nsnull;
    }
  }
}

AtomImpl::AtomImpl()
{
}

AtomImpl::~AtomImpl()
{
  NS_PRECONDITION(gAtomTable.ops, "uninitialized atom hashtable");
  // Permanent atoms are removed from the hashtable at shutdown, and we
  // don't want to remove them twice.  See comment above in
  // |AtomTableClearEntry|.
  if (!IsPermanentInDestructor()) {
    AtomTableEntry key(mString);
    PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_REMOVE);
    if (gAtomTable.entryCount == 0) {
      PL_DHashTableFinish(&gAtomTable);
      NS_ASSERTION(gAtomTable.entryCount == 0,
                   "PL_DHashTableFinish changed the entry count");
    }
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(AtomImpl, nsIAtom)

PermanentAtomImpl::PermanentAtomImpl()
  : AtomImpl()
{
}

PermanentAtomImpl::~PermanentAtomImpl()
{
  // So we can tell if we were permanent while running the base class dtor.
  mRefCnt = REFCNT_PERMANENT_SENTINEL;
}

NS_IMETHODIMP_(nsrefcnt) PermanentAtomImpl::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt) PermanentAtomImpl::Release()
{
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

void* AtomImpl::operator new ( size_t size, const nsACString& aString ) CPP_THROW_NEW
{
    /*
      Note: since the |size| will initially also include the |PRUnichar| member
        |mString|, our size calculation will give us one character too many.
        We use that extra character for a zero-terminator.

      Note: this construction is not guaranteed to be possible by the C++
        compiler.  A more reliable scheme is used by |nsShared[C]String|s, see
        http://lxr.mozilla.org/seamonkey/source/xpcom/ds/nsSharedString.h#174
     */
  size += aString.Length() * sizeof(char);
  AtomImpl* ii = NS_STATIC_CAST(AtomImpl*, ::operator new(size));
  NS_ENSURE_TRUE(ii, nsnull);

  char* toBegin = &ii->mString[0];
  nsACString::const_iterator fromBegin, fromEnd;
  *copy_string(aString.BeginReading(fromBegin), aString.EndReading(fromEnd), toBegin) = '\0';
  return ii;
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
  CopyUTF8toUTF16(nsDependentCString(mString), aBuf);
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::ToUTF8String(nsACString& aBuf)
{
  aBuf.Assign(mString);
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
  *aResult = aString.Equals(mString);
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::Equals(const nsAString& aString, PRBool* aResult)
{
  *aResult = CompareUTF8toUTF16(nsDependentCString(mString),
                                PromiseFlatString(aString)) == 0;
  return NS_OK;
}

//----------------------------------------------------------------------

// wrapper class for the nsStaticAtom struct

NS_IMETHODIMP_(nsrefcnt)
nsStaticAtomWrapper::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt)
nsStaticAtomWrapper::Release()
{
  return 1;
}

NS_IMPL_QUERY_INTERFACE1(nsStaticAtomWrapper, nsIAtom)

NS_IMETHODIMP
nsStaticAtomWrapper::GetUTF8String(const char** aResult)
{
  *aResult = mStaticAtom->mString;
  return NS_OK;
}

NS_IMETHODIMP
nsStaticAtomWrapper::ToString(nsAString& aBuf)
{
  // static should always be always ASCII, to allow tools like gperf
  // to generate the tables, and to avoid unnecessary conversion
  NS_ASSERTION(nsCRT::IsAscii(mStaticAtom->mString),
               "Data loss - atom should be ASCII");
  CopyASCIItoUTF16(nsDependentCString(mStaticAtom->mString), aBuf);
  return NS_OK;
}

NS_IMETHODIMP
nsStaticAtomWrapper::ToUTF8String(nsACString& aBuf)
{
  aBuf.Assign(mStaticAtom->mString);
  return NS_OK;
}

NS_IMETHODIMP
nsStaticAtomWrapper::EqualsUTF8(const nsACString& aString, PRBool* aResult)
{
  *aResult = aString.Equals(mStaticAtom->mString);
  return NS_OK;
}

NS_IMETHODIMP
nsStaticAtomWrapper::Equals(const nsAString& aString, PRBool* aResult)
{
  *aResult = CompareUTF8toUTF16(nsDependentCString(mStaticAtom->mString),
                                PromiseFlatString(aString)) == 0;
  return NS_OK;
}
//----------------------------------------------------------------------

static nsStaticAtomWrapper*
WrapStaticAtom(const nsStaticAtom* aAtom)
{
  if (!gStaticAtomArena) {
    gStaticAtomArena = new PLArenaPool;
    if (!gStaticAtomArena)
      return nsnull;
    
    PL_INIT_ARENA_POOL(gStaticAtomArena, "nsStaticAtomArena", 4096);
  }
  
  void* mem;
  PL_ARENA_ALLOCATE(mem, gStaticAtomArena, sizeof(nsStaticAtom));
  
  nsStaticAtomWrapper* wrapper =
    new (mem) nsStaticAtomWrapper(aAtom);
  
  return wrapper;
}

static inline AtomTableEntry*
GetAtomHashEntry(const char* aString)
{
  if (!gAtomTable.ops &&
      !PL_DHashTableInit(&gAtomTable, &AtomTableOps, 0,
                         sizeof(AtomTableEntry), 2048)) {
    gAtomTable.ops = nsnull;
    return nsnull;
  }

  AtomTableEntry key(aString);
  return NS_STATIC_CAST(AtomTableEntry*,
                        PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_ADD));
}

static inline AtomTableEntry*
GetAtomHashEntry(const PRUnichar* aString)
{
  if (!gAtomTable.ops &&
      !PL_DHashTableInit(&gAtomTable, &AtomTableOps, 0,
                         sizeof(AtomTableEntry), 2048)) {
    gAtomTable.ops = nsnull;
    return nsnull;
  }

  AtomTableEntry key(aString);
  return NS_STATIC_CAST(AtomTableEntry*,
                        PL_DHashTableOperate(&gAtomTable, &key, PL_DHASH_ADD));
}

NS_COM nsresult
NS_RegisterStaticAtoms(const nsStaticAtom* aAtoms, PRUint32 aAtomCount)
{
  // this does two things:
  // 1) wraps each static atom in a wrapper, if necessary
  // 2) initializes the address pointed to by each mBits slot
  
  for (PRUint32 i=0; i<aAtomCount; i++) {
    NS_ASSERTION(nsCRT::IsAscii(aAtoms[i].mString),
                 "Static atoms must be ASCII!");
    AtomTableEntry *he =
      GetAtomHashEntry(aAtoms[i].mString);
    
    if (he->HasValue() && aAtoms[i].mAtom) {
      // there already is an atom with this name in the table.. but we
      // still have to update mBits
      if (!he->IsStaticAtom() && !he->GetAtomImpl()->IsPermanent()) {
        // since we wanted to create a static atom but there is
        // already one there, we convert it to a non-refcounting
        // permanent atom
        PromoteToPermanent(he->GetAtomImpl());
      }
      
      // and now, if the consumer wants to remember this value in a
      // slot, we do so
      if (aAtoms[i].mAtom)
        *aAtoms[i].mAtom = he->GetAtom();
    }
    
    else {
      nsStaticAtomWrapper* atom = WrapStaticAtom(&aAtoms[i]);
      NS_ASSERTION(atom, "Failed to wrap static atom");

      // but even if atom is null, no real difference in code..
      he->SetStaticAtomWrapper(atom);
      if (aAtoms[i].mAtom)
        *aAtoms[i].mAtom = atom;
    }
  }
  return NS_OK;
}

NS_COM nsIAtom*
NS_NewAtom(const char* aUTF8String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF8String);

  if (!he) {
    return nsnull;
  }

  NS_ASSERTION(!he->IsUTF8String() && !he->IsUTF16String(),
               "Atom hash entry is string?  Should be atom!");

  if (he->HasValue())
    return he->GetAtom();

  // MSVC.NET doesn't like passing a temporary nsDependentCString() to
  // operator new, so declare one as a local instead.
  nsDependentCString str(aUTF8String);
  AtomImpl* atom = new (str) AtomImpl();
  he->SetAtomImpl(atom);
  if (!atom) {
    PL_DHashTableRawRemove(&gAtomTable, he);
    return nsnull;
  }

  NS_ADDREF(atom);
  return atom;
}

NS_COM nsIAtom*
NS_NewAtom(const nsACString& aUTF8String)
{
  return NS_NewAtom(PromiseFlatCString(aUTF8String).get());
}

NS_COM nsIAtom*
NS_NewAtom(const PRUnichar* aUTF16String)
{
  AtomTableEntry *he = GetAtomHashEntry(aUTF16String);

  if (he->HasValue())
    return he->GetAtom();

  // MSVC.NET doesn't like passing a temporary NS_ConvertUTF16toUTF8() to
  // operator new, so declare one as a local instead.
  NS_ConvertUTF16toUTF8 str(aUTF16String);
  AtomImpl* atom = new (str) AtomImpl();
  he->SetAtomImpl(atom);
  if (!atom) {
    PL_DHashTableRawRemove(&gAtomTable, he);
    return nsnull;
  }

  NS_ADDREF(atom);
  return atom;
}

NS_COM nsIAtom*
NS_NewAtom(const nsAString& aUTF16String)
{
  return NS_NewAtom(PromiseFlatString(aUTF16String).get());
}

NS_COM nsIAtom*
NS_NewPermanentAtom(const char* aUTF8String)
{
  return NS_NewPermanentAtom(nsDependentCString(aUTF8String));
}

NS_COM nsIAtom*
NS_NewPermanentAtom(const nsACString& aUTF8String)
{
  AtomTableEntry *he = GetAtomHashEntry(PromiseFlatCString(aUTF8String).get());

  if (he->HasValue() && he->IsStaticAtom())
    return he->GetStaticAtomWrapper();
  
  // either there is no atom and we'll create an AtomImpl,
  // or there is an existing AtomImpl
  AtomImpl* atom = he->GetAtomImpl();
  
  if (atom) {
    // ensure that it's permanent
    if (!atom->IsPermanent()) {
      PromoteToPermanent(atom);
    }
  } else {
    // otherwise, make a new atom
    atom = new (aUTF8String) PermanentAtomImpl();
    he->SetAtomImpl(atom);
    if ( !atom ) {
      PL_DHashTableRawRemove(&gAtomTable, he);
      return nsnull;
    }
  }

  NS_ADDREF(atom);
  return atom;
}

NS_COM nsIAtom*
NS_NewPermanentAtom(const nsAString& aUTF16String)
{
  return NS_NewPermanentAtom(NS_ConvertUTF16toUTF8(aUTF16String));
}

NS_COM nsIAtom*
NS_NewPermanentAtom(const PRUnichar* aUTF16String)
{
  return NS_NewPermanentAtom(NS_ConvertUTF16toUTF8(aUTF16String));
}

NS_COM nsrefcnt
NS_GetNumberOfAtoms(void)
{
  return gAtomTable.entryCount;
}

