/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIAtom.h"
#include "nsString.h"
#include "nsCRT.h"
#include "plhash.h"
#include "nsISizeOfHandler.h"

/**
 * The shared hash table for atom lookups.
 */
static nsrefcnt gAtoms;
static struct PLHashTable* gAtomHashTable;

class AtomImpl : public nsIAtom {
public:
  AtomImpl();
  virtual ~AtomImpl();

  NS_DECL_ISUPPORTS

  void* operator new(size_t size, const PRUnichar* us, PRInt32 uslen);

  void operator delete(void* ptr) {
    ::operator delete(ptr);
  }

  virtual void ToString(nsString& aBuf) const;

  virtual const PRUnichar* GetUnicode() const;

  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

  // Actually more; 0 terminated. This slot is reserved for the
  // terminating zero.
  PRUnichar mString[1];
};

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

static NS_DEFINE_IID(kIAtomIID, NS_IATOM_IID);
NS_IMPL_ISUPPORTS(AtomImpl, kIAtomIID);

void* AtomImpl::operator new(size_t size, const PRUnichar* us, PRInt32 uslen)
{
  size = size + uslen * sizeof(PRUnichar);
  AtomImpl* ii = (AtomImpl*) ::operator new(size);
  nsCRT::memcpy(ii->mString, us, uslen * sizeof(PRUnichar));
  ii->mString[uslen] = 0;
  return ii;
}

void AtomImpl::ToString(nsString& aBuf) const
{
  aBuf.SetLength(0);
  aBuf.Append(mString, nsCRT::strlen(mString));
}

const PRUnichar* AtomImpl::GetUnicode() const
{
  return mString;
}

NS_IMETHODIMP
AtomImpl::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this) + nsCRT::strlen(mString) * sizeof(PRUnichar));
  return NS_OK;
}

//----------------------------------------------------------------------

static PLHashNumber HashKey(const PRUnichar* k)
{
  return (PLHashNumber) nsCRT::HashValue(k);
}

static PRIntn CompareKeys(const PRUnichar* k1, const PRUnichar* k2)
{
  return nsCRT::strcmp(k1, k2) == 0;
}

NS_BASE nsIAtom* NS_NewAtom(const char* isolatin1)
{
  nsAutoString tmp(isolatin1);
  return NS_NewAtom(tmp.GetUnicode());
}

NS_BASE nsIAtom* NS_NewAtom(const nsString& aString)
{
  return NS_NewAtom(aString.GetUnicode());
}

NS_BASE nsIAtom* NS_NewAtom(const PRUnichar* us)
{
  if (nsnull == gAtomHashTable) {
    gAtomHashTable = PL_NewHashTable(8, (PLHashFunction) HashKey,
                                     (PLHashComparator) CompareKeys,
                                     (PLHashComparator) nsnull,
                                     nsnull, nsnull);
  }
  PRUint32 uslen;
  PRUint32 hashCode = nsCRT::HashValue(us, &uslen);
  PLHashEntry** hep = PL_HashTableRawLookup(gAtomHashTable, hashCode, us);
  PLHashEntry* he = *hep;
  if (nsnull != he) {
    nsIAtom* id = (nsIAtom*) he->value;
    NS_ADDREF(id);
    return id;
  }
  AtomImpl* id = new(us, uslen) AtomImpl();
  PL_HashTableRawAdd(gAtomHashTable, hep, hashCode, id->mString, id);
  NS_ADDREF(id);
  return id;
}

NS_BASE nsrefcnt NS_GetNumberOfAtoms(void)
{
  if (nsnull != gAtomHashTable) {
    NS_PRECONDITION(nsrefcnt(gAtomHashTable->nentries) == gAtoms, "bad atom table");
  }
  return gAtoms;
}
