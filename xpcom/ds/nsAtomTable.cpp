/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   IBM Corp.
 */

#include "nsAtomTable.h"
#include "nsString.h"
#include "nsCRT.h"
#include "plhash.h"
#include "nsISizeOfHandler.h"
#include "nslog.h"

NS_IMPL_LOG(nsAtomTableLog)
#define PRINTF NS_LOG_PRINTF(nsAtomTableLog)
#define FLUSH  NS_LOG_FLUSH(nsAtomTableLog)

/**
 * The shared hash table for atom lookups.
 */
static nsrefcnt gAtoms;
static struct PLHashTable* gAtomHashTable;

#if defined(DEBUG) && (defined(XP_UNIX) || defined(XP_PC))
static PRIntn PR_CALLBACK
DumpAtomLeaks(PLHashEntry *he, PRIntn index, void *arg)
{
  AtomImpl* atom = (AtomImpl*) he->value;
  if (atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, stdout);
    fputs("\n", stdout);
  }
  return HT_ENUMERATE_NEXT;
}
#endif

NS_COM void NS_PurgeAtomTable(void)
{
  if (gAtomHashTable) {
#if defined(DEBUG) && (defined(XP_UNIX) || defined(XP_PC))
    if (gAtoms) {
      if (NS_LOG_ENABLED(nsAtomTableLog)) {
        PRINTF("*** leaking %d atoms\n", gAtoms);
        PL_HashTableEnumerateEntries(gAtomHashTable, DumpAtomLeaks, 0);
      }
    }
#endif
    PL_HashTableDestroy(gAtomHashTable);
    gAtomHashTable = nsnull;
  }
}

AtomImpl::AtomImpl()
{
  NS_INIT_REFCNT();
  // Every live atom holds a reference on the atom hashtable
  gAtoms++;
}

AtomImpl::~AtomImpl()
{
  NS_PRECONDITION(nsnull != gAtomHashTable, "null atom hashtable");
  if (nsnull != gAtomHashTable) {

    PL_HashTableRemove(gAtomHashTable, mString);
    nsrefcnt cnt = --gAtoms;
    if (0 == cnt) {
      // When the last atom is destroyed, the atom arena is destroyed
      NS_ASSERTION(0 == gAtomHashTable->nentries, "bad atom table");
      PL_HashTableDestroy(gAtomHashTable);
      gAtomHashTable = nsnull;
    }
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(AtomImpl, nsIAtom)

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
  AtomImpl* ii = (AtomImpl*) ::operator new(size);

  PRUnichar* toBegin = &ii->mString[0];
  nsReadingIterator<PRUnichar> fromBegin, fromEnd;
  *copy_string(aString.BeginReading(fromBegin), aString.EndReading(fromEnd), toBegin) = PRUnichar(0);
  return ii;
}

NS_IMETHODIMP 
AtomImpl::ToString(nsAWritableString& aBuf) /*FIX: const */
{
  aBuf.SetLength(0);
  aBuf.Append(mString, nsCRT::strlen(mString));
  return NS_OK;
}

NS_IMETHODIMP 
AtomImpl::GetUnicode(const PRUnichar **aResult) /*FIX: const */
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mString;
  return NS_OK;
}

NS_IMETHODIMP
AtomImpl::SizeOf(nsISizeOfHandler* aHandler, PRUint32* _retval) /*FIX: const */
{
  NS_ENSURE_ARG_POINTER(_retval);
  PRUint32 sum = sizeof(*this) + nsCRT::strlen(mString) * sizeof(PRUnichar);
  *_retval = sum;
  return NS_OK;
}

//----------------------------------------------------------------------

static PLHashNumber HashKey(const PRUnichar* k)
{
  PRUint32 hashResult = 0; 
  PRUint32 N = nsCRT::strlen(k);

  for ( PRUint32 n=0; n<N; ++n )
    hashResult = (hashResult<<5) + (hashResult<<2) + hashResult + *k++;  // mHash = mHash*37 + *s

  return hashResult;
}

static PRIntn CompareKeys( const PRUnichar* k1, const PRUnichar* k2 )
{
  return nsCRT::strcmp(k1, k2) == 0;
}

NS_COM nsIAtom* NS_NewAtom(const char* isolatin1)
{
  return NS_NewAtom(NS_ConvertASCIItoUCS2(isolatin1));
}

NS_COM nsIAtom* NS_NewAtom( const nsAReadableString& aString )
{
  if ( !gAtomHashTable )
    gAtomHashTable = PL_NewHashTable(2048, (PLHashFunction)HashKey,
                                     (PLHashComparator)CompareKeys,
                                     (PLHashComparator)0, 0, 0);

  const PRUnichar *str = nsPromiseFlatString(aString).get();

  PRUint32 hashCode = HashKey(str);

  PLHashEntry** hep = PL_HashTableRawLookup(gAtomHashTable, hashCode, str);

  PLHashEntry*  he  = *hep;

  AtomImpl* id;

  if ( he ) {
      // if we found one, great
    id = NS_STATIC_CAST(AtomImpl*, he->value);
  } else {
      // otherwise, we'll make a new atom
    id = new (aString) AtomImpl();
    if ( id ) {
      PL_HashTableRawAdd(gAtomHashTable, hep, hashCode, id->mString, id);
    }
  }

  NS_IF_ADDREF(id);
  return id;
}

NS_COM nsIAtom* NS_NewAtom( const PRUnichar* str )
{
  return NS_NewAtom(nsLiteralString(str));
}

NS_COM nsrefcnt NS_GetNumberOfAtoms(void)
{
  if (nsnull != gAtomHashTable) {
    NS_PRECONDITION(nsrefcnt(gAtomHashTable->nentries) == gAtoms, "bad atom table");
  }
  return gAtoms;
}
