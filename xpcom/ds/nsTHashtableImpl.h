/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is C++ hashtable templates.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsTHashtableImpl_h__
#define nsTHashtableImpl_h__

#ifndef nsTHashtable_h__
#include "nsTHashtable.h"
#endif

#include "nsDebug.h"

template<class EntryType>
nsTHashtable<EntryType>::nsTHashtable()
{
  // entrySize is our "I'm initialized" indicator
  mTable.entrySize = 0;
}

template<class EntryType>
nsTHashtable<EntryType>::~nsTHashtable()
{
  if (mTable.entrySize)
    PL_DHashTableFinish(&mTable);
}

template<class EntryType>
PRBool
nsTHashtable<EntryType>::Init(PRUint32 initSize)
{
  if (mTable.entrySize)
  {
    NS_ERROR("nsTHashtable::Init() should not be called twice.");
    return PR_FALSE;
  }
  
  if (!PL_DHashTableInit(&mTable, &sOps, nsnull, sizeof(EntryType), initSize))
  {
    // if failed, reset "flag"
    mTable.entrySize = 0;
    return PR_FALSE;
  }

  return PR_TRUE;
}

template<class EntryType>
EntryType*
nsTHashtable<EntryType>::GetEntry(KeyTypePointer aKey) const
{
  NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");
  
  return NS_REINTERPRET_CAST(EntryType*,
                             PL_DHashTableOperate(
                               NS_CONST_CAST(PLDHashTable*,&mTable),
                               aKey,
                               PL_DHASH_LOOKUP));
}

template<class EntryType>
EntryType*
nsTHashtable<EntryType>::PutEntry(KeyTypePointer aKey)
{
  NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

  return (EntryType*) PL_DHashTableOperate(&mTable, aKey, PL_DHASH_ADD);
}

template<class EntryType>
void
nsTHashtable<EntryType>::RemoveEntry(KeyTypePointer aKey)
{
  NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

  PL_DHashTableOperate(&mTable, aKey, PL_DHASH_REMOVE);

  return;
}

template<class EntryType>
PRUint32
nsTHashtable<EntryType>::EnumerateEntries(Enumerator enumFunc, void* userArg)
{
  NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

  s_EnumArgs args = { enumFunc, userArg };
  
  return PL_DHashTableEnumerate(&mTable, s_EnumStub, &args);
}

template<class EntryType>
void
nsTHashtable<EntryType>::Clear()
{
  NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

  PL_DHashTableEnumerate(&mTable, PL_DHashStubEnumRemove, nsnull);
}

// static definitions

template<class EntryType>
PLDHashTableOps
nsTHashtable<EntryType>::sOps =
{
  ::PL_DHashAllocTable,
  ::PL_DHashFreeTable,
  s_GetKey,
  s_HashKey,
  s_MatchEntry,
  EntryType::AllowMemMove() ? ::PL_DHashMoveEntryStub : s_CopyEntry,
  s_ClearEntry,
  ::PL_DHashFinalizeStub,
  s_InitEntry
};

template<class EntryType>
const void*
nsTHashtable<EntryType>::s_GetKey(PLDHashTable    *table,
                                  PLDHashEntryHdr *entry)
{
  return ((EntryType*) entry)->GetKeyPointer();
}

template<class EntryType>
PLDHashNumber
nsTHashtable<EntryType>::s_HashKey(PLDHashTable  *table,
                                   const void    *key)
{
  return EntryType::HashKey(NS_REINTERPRET_CAST(const KeyTypePointer, key));
}

template<class EntryType>
PRBool
nsTHashtable<EntryType>::s_MatchEntry(PLDHashTable          *table,
                                      const PLDHashEntryHdr *entry,
                                      const void            *key)
{
  return ((const EntryType*) entry)->KeyEquals(
    NS_REINTERPRET_CAST(const KeyTypePointer, key));
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_CopyEntry(PLDHashTable          *table,
                                     const PLDHashEntryHdr *from,
                                     PLDHashEntryHdr       *to)
{
  new(to) EntryType(* NS_REINTERPRET_CAST(const EntryType*,from));

  NS_CONST_CAST(EntryType*,NS_REINTERPRET_CAST(const EntryType*,from))->~EntryType();
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_ClearEntry(PLDHashTable    *table,
                                      PLDHashEntryHdr *entry)
{
  NS_REINTERPRET_CAST(EntryType*,entry)->~EntryType();
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_InitEntry(PLDHashTable    *table,
                                     PLDHashEntryHdr *entry,
                                     const void      *key)
{
  new(entry) EntryType(NS_REINTERPRET_CAST(KeyTypePointer,key));
}

template<class EntryType>
PLDHashOperator
nsTHashtable<EntryType>::s_EnumStub(PLDHashTable    *table,
                                    PLDHashEntryHdr *entry,
                                    PRUint32         number,
                                    void            *arg)
{
  // dereferences the function-pointer to the user's enumeration function
  return (* NS_REINTERPRET_CAST(s_EnumArgs*,arg)->userFunc)(
    NS_REINTERPRET_CAST(EntryType*,entry),
    NS_REINTERPRET_CAST(s_EnumArgs*,arg)->userArg);
}

#endif // nsTHashtableImpl_h__
