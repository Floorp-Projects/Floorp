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

#ifndef nsBaseHashtableImpl_h__
#define nsBaseHashtableImpl_h__

#ifndef nsBaseHashtable_h__
#include "nsBaseHashtable.h"
#endif

#ifndef nsTHashtableImpl_h__
#include "nsTHashtableImpl.h"
#endif

#include "prrwlock.h"
#include "nsDebug.h"

/* nsBaseHashtableET definitions */

template<class KeyClass,class DataType>
nsBaseHashtableET<KeyClass,DataType>::nsBaseHashtableET(KeyTypePointer aKey) :
  KeyClass(aKey)
{ }

template<class KeyClass,class DataType>
nsBaseHashtableET<KeyClass,DataType>::nsBaseHashtableET
  (const nsBaseHashtableET<KeyClass,DataType>& toCopy) :
  KeyClass(toCopy),
  mData(toCopy.mData)
{ }

template<class KeyClass,class DataType>
nsBaseHashtableET<KeyClass,DataType>::~nsBaseHashtableET()
{ }

/* nsBaseHashtable definitions */

template<class KeyClass,class DataType,class UserDataType>
nsBaseHashtable<KeyClass,DataType,UserDataType>::nsBaseHashtable()
{ }

template<class KeyClass,class DataType,class UserDataType>
nsBaseHashtable<KeyClass,DataType,UserDataType>::~nsBaseHashtable()
{
  if (mLock)
    PR_DestroyRWLock(mLock);
}

template<class KeyClass,class DataType,class UserDataType>
PRBool
nsBaseHashtable<KeyClass,DataType,UserDataType>::Init(PRUint32 initSize,
                                                      PRBool   threadSafe)
{
  if (!nsTHashtable<EntryType>::Init(initSize))
    return PR_FALSE;

  if (!threadSafe)
  {
    mLock = nsnull;
    return PR_TRUE;
  }

  mLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "nsBaseHashtable");

  NS_WARN_IF_FALSE(mLock, "Error creating lock during nsBaseHashtable::Init()");

  if (!mLock)
    return PR_FALSE;

  return PR_TRUE;
}

template<class KeyClass,class DataType,class UserDataType>
PRBool
nsBaseHashtable<KeyClass,DataType,UserDataType>::Get(KeyType       aKey,
                                                     UserDataType* pData) const
{
  if (mLock)
    PR_RWLock_Rlock(mLock);

  EntryType* ent = GetEntry(KeyClass::KeyToPointer(aKey));

  if (ent)
  {
    if (pData)
      *pData = ent->mData;

    if (mLock)
      PR_RWLock_Unlock(mLock);

    return PR_TRUE;
  }

  if (mLock)
    PR_RWLock_Unlock(mLock);

  return PR_FALSE;
}

template<class KeyClass,class DataType,class UserDataType>
PRBool
nsBaseHashtable<KeyClass,DataType,UserDataType>::Put(KeyType      aKey,
                                                     UserDataType aData)
{
  if (mLock)
    PR_RWLock_Wlock(mLock);

  EntryType* ent = PutEntry(KeyClass::KeyToPointer(aKey));

  if (!ent)
  {
    if (mLock)
      PR_RWLock_Unlock(mLock);

    return PR_FALSE;
  }

  ent->mData = aData;

  if (mLock)
    PR_RWLock_Unlock(mLock);

  return PR_TRUE;
}

template<class KeyClass,class DataType,class UserDataType>
void
nsBaseHashtable<KeyClass,DataType,UserDataType>::Remove(KeyType aKey)
{
  if (mLock)
    PR_RWLock_Wlock(mLock);

  RemoveEntry(KeyClass::KeyToPointer(aKey));

  if (mLock)
    PR_RWLock_Unlock(mLock);
}

template<class KeyClass,class DataType,class UserDataType>
void
nsBaseHashtable<KeyClass,DataType,UserDataType>::EnumerateRead
  (EnumReadFunction fEnumCall, void* userArg) const
{
  NS_ASSERTION(mTable.entrySize,
               "nsBaseHashtable was not initialized properly.");

  if (mLock)
    PR_RWLock_Rlock(mLock);

  s_EnumReadArgs enumData = { fEnumCall, userArg };
  PL_DHashTableEnumerate(NS_CONST_CAST(PLDHashTable*,&mTable),
                         s_EnumStub,
                         &enumData);

  if (mLock)
    PR_RWLock_Unlock(mLock);
}

template<class KeyClass,class DataType,class UserDataType>
void nsBaseHashtable<KeyClass,DataType,UserDataType>::Clear()
{
  if (mLock)
    PR_RWLock_Wlock(mLock);

  nsTHashtable<EntryType>::Clear();

  if (mLock)
    PR_RWLock_Unlock(mLock);
}

template<class KeyClass,class DataType,class UserDataType>
PLDHashOperator
nsBaseHashtable<KeyClass,DataType,UserDataType>::s_EnumReadStub
  (PLDHashTable *table, PLDHashEntryHdr *hdr, PRUint32 number, void* arg)
{
  EntryType* ent = (EntryType*) hdr;
  s_EnumReadArgs* eargs = (s_EnumReadArgs*) arg;

  PLDHashOperator res = (eargs->func)(ent->GetKey(), ent->mData, eargs->userArg);

  NS_ASSERTION( !(res & PL_DHASH_REMOVE ),
                "PL_DHASH_REMOVE return during const enumeration; ignoring.");

  if (res & PL_DHASH_STOP)
    return PL_DHASH_STOP;

  return PL_DHASH_NEXT;
}

#endif // nsBaseHashtableImpl_h__
