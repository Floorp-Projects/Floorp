/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAtomTable.h"
#include "nsString.h"
#include "nsCRT.h"
#include "pldhash.h"
#include "prenv.h"
#include "nsISizeOfHandler.h"

/**
 * The shared hash table for atom lookups.
 *
 * XXX This should be manipulated in a threadsafe way or we should make
 * sure it's only manipulated from the main thread.  Probably the latter
 * is better, since the former would hurt performance.
 *
 * If |gAtomTable.entryCount| is 0, then the table is uninitialized.
 */
static PLDHashTable gAtomTable;

struct AtomTableEntry : public PLDHashEntryHdr {
  AtomImpl *mAtom;
};

PR_STATIC_CALLBACK(const void *)
AtomTableGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  AtomTableEntry *he = NS_STATIC_CAST(AtomTableEntry*, entry);
  return he->mAtom->mString;
}

PR_STATIC_CALLBACK(PLDHashNumber)
AtomTableHashKey(PLDHashTable *table, const void *key)
{
  return nsCRT::HashCode(NS_STATIC_CAST(const PRUnichar*,key));
}

PR_STATIC_CALLBACK(PRBool)
AtomTableMatchKey(PLDHashTable *table,
                  const PLDHashEntryHdr *entry,
                  const void *key)
{
  const AtomTableEntry *he = NS_STATIC_CAST(const AtomTableEntry*, entry);
  const PRUnichar* keyStr = NS_STATIC_CAST(const PRUnichar*, key);
  return nsCRT::strcmp(keyStr, he->mAtom->mString) == 0;
}

PR_STATIC_CALLBACK(void)
AtomTableClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  AtomTableEntry *he = NS_STATIC_CAST(AtomTableEntry*, entry);
  AtomImpl *atom = he->mAtom;
  he->mAtom = 0;
  he->keyHash = 0;

  // Normal |AtomImpl| atoms are deleted when their refcount hits 0, and
  // they then remove themselves from the table.  In other words, they
  // are owned by the callers who own references to them.
  // |PermanentAtomImpl| permanent atoms ignore their refcount and are
  // deleted when they are removed from the table at table destruction.
  // In other words, they are owned by the atom table.
  if (atom->IsPermanent())
    delete atom;
}

static PLDHashTableOps AtomTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  AtomTableGetKey,
  AtomTableHashKey,
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
  AtomImpl* atom = entry->mAtom;
  if (!atom->IsPermanent()) {
    ++*NS_STATIC_CAST(PRUint32*, arg);
    const PRUnichar *str;
    atom->GetUnicode(&str);
    fputs(NS_LossyConvertUCS2toASCII(str).get(), stdout);
    fputs("\n", stdout);
  }
  return PL_DHASH_NEXT;
}

#endif

void NS_PurgeAtomTable()
{
  if (gAtomTable.entryCount) {
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
  }
}

AtomImpl::AtomImpl()
{
  NS_INIT_ISUPPORTS();
}

AtomImpl::~AtomImpl()
{
  NS_PRECONDITION(gAtomTable.entryCount, "uninitialized atom hashtable");
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

void* AtomImpl::operator new ( size_t size, const nsAReadableString& aString )
{
    /*
      Note: since the |size| will initially also include the |PRUnichar| member
        |mString|, our size calculation will give us one character too many.
        We use that extra character for a zero-terminator.

      Note: this construction is not guaranteed to be possible by the C++
        compiler.  A more reliable scheme is used by |nsShared[C]String|s, see
        http://lxr.mozilla.org/seamonkey/source/xpcom/ds/nsSharedString.h#174
     */
  size += aString.Length() * sizeof(PRUnichar);
  AtomImpl* ii = NS_STATIC_CAST(AtomImpl*, ::operator new(size));

  PRUnichar* toBegin = &ii->mString[0];
  nsReadingIterator<PRUnichar> fromBegin, fromEnd;
  *copy_string(aString.BeginReading(fromBegin), aString.EndReading(fromEnd), toBegin) = PRUnichar(0);
  return ii;
}

void* PermanentAtomImpl::operator new ( size_t size, AtomImpl* aAtom ) {
  NS_ASSERTION(!aAtom->IsPermanent(),
               "converting atom that's already permanent");

  // Just let the constructor overwrite the vtable pointer.
  return aAtom;
}

NS_IMETHODIMP 
AtomImpl::ToString(nsAWritableString& aBuf) /*FIX: const */
{
  aBuf.Assign(mString);
  return NS_OK;
}

NS_IMETHODIMP 
AtomImpl::GetUnicode(const PRUnichar **aResult) /*FIX: const */
{
  NS_PRECONDITION(aResult, "null out param");
  *aResult = mString;
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::SizeOf(nsISizeOfHandler* aHandler, PRUint32* _retval) /*FIX: const */
{
#ifdef DEBUG
  NS_PRECONDITION(_retval, "null out param");
  *_retval = sizeof(*this) + nsCRT::strlen(mString) * sizeof(PRUnichar);
#endif
  return NS_OK;
}

//----------------------------------------------------------------------

NS_COM nsIAtom* NS_NewAtom(const char* isolatin1)
{
  return NS_NewAtom(NS_ConvertASCIItoUCS2(isolatin1));
}

NS_COM nsIAtom* NS_NewPermanentAtom(const char* isolatin1)
{
  return NS_NewPermanentAtom(NS_ConvertASCIItoUCS2(isolatin1));
}

static AtomTableEntry* GetAtomHashEntry(const nsAString& aString)
{
  if ( !gAtomTable.entryCount )
    PL_DHashTableInit(&gAtomTable, &AtomTableOps, 0,
                      sizeof(AtomTableEntry), 2048);

  return NS_STATIC_CAST(AtomTableEntry*,
                        PL_DHashTableOperate(&gAtomTable,
                                             PromiseFlatString(aString).get(),
                                             PL_DHASH_ADD));
}

NS_COM nsIAtom* NS_NewAtom( const nsAString& aString )
{
  AtomTableEntry *he = GetAtomHashEntry(aString);
  AtomImpl* atom = he->mAtom;

  if (!atom) {
    atom = new (aString) AtomImpl();
    he->mAtom = atom;
    if (!atom) {
      PL_DHashTableRawRemove(&gAtomTable, he);
      return nsnull;
    }
  }

  NS_ADDREF(atom);
  return atom;
}

NS_COM nsIAtom* NS_NewPermanentAtom( const nsAString& aString )
{
  AtomTableEntry *he = GetAtomHashEntry(aString);
  AtomImpl* atom = he->mAtom;

  if (atom) {
    // ensure that it's permanent
    if (!atom->IsPermanent()) {
#ifdef NS_BUILD_REFCNT_LOGGING
      {
        nsrefcnt refcount = atom->GetRefCount();
        do {
          NS_LOG_RELEASE(atom, --refcount, "AtomImpl");
        } while (refcount);
      }
#endif
      atom = new (atom) PermanentAtomImpl();
    }
  } else {
    // otherwise, make a new atom
    atom = new (aString) PermanentAtomImpl();
    he->mAtom = atom;
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
  return NS_NewAtom(nsDependentString(str));
}

NS_COM nsIAtom* NS_NewPermanentAtom( const PRUnichar* str )
{
  return NS_NewPermanentAtom(nsDependentString(str));
}

NS_COM nsrefcnt NS_GetNumberOfAtoms(void)
{
  return gAtomTable.entryCount;
}
