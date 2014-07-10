/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsCheapSets_h__
#define __nsCheapSets_h__

#include "nsTHashtable.h"
#include <stdint.h>

/**
 * A set that takes up minimal size when there are 0 or 1 entries in the set.
 * Use for cases where sizes of 0 and 1 are even slightly common.
 */
template<typename EntryType>
class nsCheapSet
{
public:
  typedef typename EntryType::KeyType KeyType;
  typedef PLDHashOperator (*Enumerator)(EntryType* aEntry, void* userArg);

  nsCheapSet() : mState(ZERO) {}
  ~nsCheapSet() { Clear(); }

  /**
   * Remove all entries.
   */
  void Clear()
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
    mState = ZERO;
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

  uint32_t EnumerateEntries(Enumerator aEnumFunc, void* aUserArg)
  {
    switch (mState) {
      case ZERO:
        return 0;
      case ONE:
        if (aEnumFunc(GetSingleEntry(), aUserArg) == PL_DHASH_REMOVE) {
          GetSingleEntry()->~EntryType();
          mState = ZERO;
        }
        return 1;
      case MANY:
        return mUnion.table->EnumerateEntries(aEnumFunc, aUserArg);
      default:
        NS_NOTREACHED("bogus state");
        return 0;
    }
  }

private:
  EntryType* GetSingleEntry()
  {
    return reinterpret_cast<EntryType*>(&mUnion.singleEntry[0]);
  }

  enum SetState
  {
    ZERO,
    ONE,
    MANY
  };

  union
  {
    nsTHashtable<EntryType>* table;
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
    case ONE: {
      nsTHashtable<EntryType>* table = new nsTHashtable<EntryType>();
      EntryType* entry = GetSingleEntry();
      table->PutEntry(entry->GetKey());
      entry->~EntryType();
      mUnion.table = table;
      mState = MANY;
    }
    // Fall through.
    case MANY:
      mUnion.table->PutEntry(aVal);
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
