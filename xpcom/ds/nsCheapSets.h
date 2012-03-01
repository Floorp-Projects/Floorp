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
 * The Original Code is Mozilla Communicator client code.
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

#ifndef __nsCheapSets_h__
#define __nsCheapSets_h__

#include "nsTHashtable.h"
#include "mozilla/StandardInteger.h"

/**
 * A set that takes up minimal size when there are 0 or 1 entries in the set.
 * Use for cases where sizes of 0 and 1 are even slightly common.
 */
template<typename EntryType>
class nsCheapSet
{
public:
  typedef typename EntryType::KeyType KeyType;

  nsCheapSet() : mState(ZERO)
  {
  }
  ~nsCheapSet()
  {
    switch (mState) {
    case ZERO:
      break;
    case ONE:
      GetSingleEntry()->~EntryType();
      break;
    case MANY:
      delete mUnion.table;
      break;
    default:
      NS_NOTREACHED("bogus state");
      break;
    }
  }

  nsresult Put(const KeyType aVal);

  void Remove(const KeyType aVal);

  bool Contains(const KeyType aVal)
  {
    switch (mState) {
    case ZERO:
      return false;
    case ONE:
      return GetSingleEntry()->KeyEquals(EntryType::KeyToPointer(aVal));
    case MANY:
      return !!mUnion.table->GetEntry(aVal);
    default:
      NS_NOTREACHED("bogus state");
      return false;
    }
  }

private:
  EntryType* GetSingleEntry()
  {
    return reinterpret_cast<EntryType*>(&mUnion.singleEntry[0]);
  }

  enum SetState {
    ZERO,
    ONE,
    MANY
  };

  union {
    nsTHashtable<EntryType> *table;
    char singleEntry[sizeof(EntryType)];
  } mUnion;
  enum SetState mState;
};

template<typename EntryType>
nsresult
nsCheapSet<EntryType>::Put(const KeyType aVal)
{
  switch (mState) {
  case ZERO:
    new (GetSingleEntry()) EntryType(EntryType::KeyToPointer(aVal));
    mState = ONE;
    return NS_OK;
  case ONE:
    {
      nsTHashtable<EntryType> *table = new nsTHashtable<EntryType>();
      if (!table) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (!table->Init()) {
        return NS_ERROR_FAILURE;
      }
      EntryType *entry = GetSingleEntry();
      if (!table->PutEntry(entry->GetKey())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      entry->~EntryType();
      mUnion.table = table;
      mState = MANY;
    }
    // Fall through.
  case MANY:
    if (!mUnion.table->PutEntry(aVal)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  default:
    NS_NOTREACHED("bogus state");
    return NS_OK;
  }
}

template<typename EntryType>
void
nsCheapSet<EntryType>::Remove(const KeyType aVal)
{
  switch (mState) {
  case ZERO:
    break;
  case ONE:
    if (Contains(aVal)) {
      GetSingleEntry()->~EntryType();
      mState = ZERO;
    }
    break;
  case MANY:
    mUnion.table->RemoveEntry(aVal);
    break;
  default:
    NS_NOTREACHED("bogus state");
    break;
  }
}

#endif
