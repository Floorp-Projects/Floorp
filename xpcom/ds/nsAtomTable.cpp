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
#include "nsVoidArray.h"

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

// the atomtableentry can contain either an AtomImpl or a
// nsStaticAtomWrapper, indicated by the first bit of PtrBits
typedef unsigned long PtrBits;

struct AtomTableEntry : public PLDHashEntryHdr {
  // mAtom & 0x1 means (mAtom & ~0x1) points to an nsStaticAtomWrapper
  // else it points to an nsAtomImpl
  PtrBits mAtom;

  inline PRBool IsStaticAtom() const {
    return (mAtom & 0x1) != 0;
  }
  
  inline void SetAtomImpl(AtomImpl* aAtom) {
    NS_ASSERTION(aAtom, "Setting null atom");
    mAtom = PtrBits(aAtom);
  }

  inline void SetStaticAtomWrapper(nsStaticAtomWrapper* aAtom) {
    NS_ASSERTION(aAtom, "Setting null atom");
    NS_ASSERTION((PtrBits(aAtom) & ~0x1) == PtrBits(aAtom),
                 "Pointers must align or this is broken");
    
    mAtom = PtrBits(aAtom) | 0x1;
  }
  
  inline void ClearAtom() {
    mAtom = nsnull;
  }

  inline PRBool HasValue() const {
    return (mAtom & ~0x1) != 0;
  }

  // these accessors assume that you already know the type
  inline AtomImpl *GetAtomImpl() const {
    NS_ASSERTION(!IsStaticAtom(), "This is a static atom, not an AtomImpl");
    return (AtomImpl*) (mAtom & ~0x1);
  }
  
  inline nsStaticAtomWrapper *GetStaticAtomWrapper() const {
    NS_ASSERTION(IsStaticAtom(), "This is an AtomImpl, not a static atom");
    return (nsStaticAtomWrapper*) (mAtom & ~0x1);
  }

  inline const nsStaticAtom* GetStaticAtom() const {
    return GetStaticAtomWrapper()->GetStaticAtom();
  }

  // type-agnostic accessors

  // get the string buffer
  inline const char* get() const {
    return IsStaticAtom() ? GetStaticAtom()->mString : GetAtomImpl()->mString;
  }

  // get an addreffed nsIAtom - not using already_AddRef'ed atom
  // because the callers are not (and should not be) using nsCOMPtr
  inline nsIAtom* GetAtom() const {
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

PR_STATIC_CALLBACK(const void *)
AtomTableGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  AtomTableEntry *he = NS_STATIC_CAST(AtomTableEntry*, entry);
  NS_ASSERTION(he->HasValue(), "Empty atom. how did that happen?");
  return he->get();
}

PR_STATIC_CALLBACK(PRBool)
AtomTableMatchKey(PLDHashTable *table,
                  const PLDHashEntryHdr *entry,
                  const void *key)
{
  const AtomTableEntry *he = NS_STATIC_CAST(const AtomTableEntry*, entry);
  const char* keyStr = NS_STATIC_CAST(const char*, key);
  return nsCRT::strcmp(keyStr, he->get()) == 0;
}

PR_STATIC_CALLBACK(void)
AtomTableClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  AtomTableEntry *he = NS_STATIC_CAST(AtomTableEntry*, entry);
  
  he->keyHash = 0;

  if (!he->IsStaticAtom()) {
    AtomImpl *atom = he->GetAtomImpl();
    // Normal |AtomImpl| atoms are deleted when their refcount hits 0, and
    // they then remove themselves from the table.  In other words, they
    // are owned by the callers who own references to them.
    // |PermanentAtomImpl| permanent atoms ignore their refcount and are
    // deleted when they are removed from the table at table destruction.
    // In other words, they are owned by the atom table.
    if (atom->IsPermanent())
      delete NS_STATIC_CAST(PermanentAtomImpl*, atom);
  }
  else {
    he->GetStaticAtomWrapper()->~nsStaticAtomWrapper();
  }
  
  he->ClearAtom();
}

static const PLDHashTableOps AtomTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  AtomTableGetKey,
  PL_DHashStringKey,
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

void NS_PurgeAtomTable()
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
  if (!IsPermanent()) {
    PL_DHashTableOperate(&gAtomTable, mString, PL_DHASH_REMOVE);
    if (gAtomTable.entryCount == 0) {
      PL_DHashTableFinish(&gAtomTable);
      NS_ASSERTION(gAtomTable.entryCount == 0,
                   "PL_DHashTableFinish changed the entry count");
    }
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(AtomImpl, nsIAtom)

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
  *aResult = NS_ConvertUTF16toUTF8(aString).Equals(mString);
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
  CopyASCIItoUCS2(nsDependentCString(mStaticAtom->mString), aBuf);
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
  *aResult = NS_ConvertUCS2toUTF8(aString).Equals(mStaticAtom->mString);
  return NS_OK;
}
//----------------------------------------------------------------------

NS_COM nsIAtom* NS_NewAtom(const char* isolatin1)
{
  return NS_NewAtom(nsDependentCString(isolatin1));
}

NS_COM nsIAtom* NS_NewPermanentAtom(const char* isolatin1)
{
  return NS_NewPermanentAtom(NS_ConvertASCIItoUCS2(isolatin1));
}

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

static AtomTableEntry* GetAtomHashEntry(const char* aString)
{
  if (!gAtomTable.ops &&
      !PL_DHashTableInit(&gAtomTable, &AtomTableOps, 0,
                         sizeof(AtomTableEntry), 2048)) {
    gAtomTable.ops = nsnull;
    return nsnull;
  }
  return NS_STATIC_CAST(AtomTableEntry*,
                        PL_DHashTableOperate(&gAtomTable,
                                             aString,
                                             PL_DHASH_ADD));
}

NS_COM nsresult
NS_RegisterStaticAtoms(const nsStaticAtom* aAtoms, PRUint32 aAtomCount)
{
  // this does two things:
  // 1) wraps each static atom in a wrapper, if necessary
  // 2) initializes the address pointed to by each mAtom slot
  
  for (PRUint32 i=0; i<aAtomCount; i++) {
    NS_ASSERTION(nsCRT::IsAscii(aAtoms[i].mString),
                 "Static atoms must be ASCII!");
    AtomTableEntry *he =
      GetAtomHashEntry(aAtoms[i].mString);
    
    if (he->HasValue() && aAtoms[i].mAtom) {
      // there already is an atom with this name in the table.. but we
      // still have to update mAtom
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

NS_COM nsIAtom* NS_NewAtom( const nsAString& aString )
{
  NS_ConvertUCS2toUTF8 utf8String(aString);

  return NS_NewAtom(utf8String);
}

NS_COM
nsIAtom*
NS_NewAtom( const nsACString& aString )
{
  AtomTableEntry *he = GetAtomHashEntry(PromiseFlatCString(aString).get());

  if (he->HasValue())
    return he->GetAtom();

  AtomImpl* atom = new (aString) AtomImpl();
  he->SetAtomImpl(atom);
  if (!atom) {
    PL_DHashTableRawRemove(&gAtomTable, he);
    return nsnull;
  }

  NS_ADDREF(atom);
  return atom;
}

NS_COM nsIAtom* NS_NewPermanentAtom( const nsAString& aString )
{
  return NS_NewPermanentAtom(NS_ConvertUCS2toUTF8(aString));
}

NS_COM
nsIAtom* NS_NewPermanentAtom( const nsACString& aString )
{
  AtomTableEntry *he = GetAtomHashEntry(PromiseFlatCString(aString).get());

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
    atom = new (aString) PermanentAtomImpl();
    he->SetAtomImpl(atom);
    if ( !atom ) {
      PL_DHashTableRawRemove(&gAtomTable, he);
      return nsnull;
    }
  }

  NS_ADDREF(atom);
  return atom;
}

NS_COM nsIAtom* NS_NewAtom( const PRUnichar* str )
{
  return NS_NewAtom(NS_ConvertUCS2toUTF8(str));
}

NS_COM nsIAtom* NS_NewPermanentAtom( const PRUnichar* str )
{
  return NS_NewPermanentAtom(nsDependentString(str));
}

NS_COM nsrefcnt NS_GetNumberOfAtoms(void)
{
  return gAtomTable.entryCount;
}

