/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsCheapSets_h__
#define __nsCheapSets_h__

#include "nsTHashtable.h"
#include <stdint.h>

enum nsCheapSetOperator
{
  OpNext = 0,   // enumerator says continue
  OpRemove = 1, // enumerator says remove and continue
};

/**
 * A set that takes up minimal size when there are 0 or 1 entries in the set.
 * Use for cases where sizes of 0 and 1 are even slightly common.
 */
template<typename EntryType>
class nsCheapSet
{
public:
  typedef typename EntryType::KeyType KeyType;
  typedef nsCheapSetOperator (*Enumerator)(EntryType* aEntry, void* userArg);

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

  void Put(const KeyType aVal);

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
        if (aEnumFunc(GetSingleEntry(), aUserArg) == OpRemove) {
          GetSingleEntry()->~EntryType();
          mState = ZERO;
        }
        return 1;
      case MANY: {
        uint32_t n = mUnion.table->Count();
        for (auto iter = mUnion.table->Iter(); !iter.Done(); iter.Next()) {
          auto entry = static_cast<EntryType*>(iter.Get());
          if (aEnumFunc(entry, aUserArg) == OpRemove) {
            iter.Remove();
          }
        }
        return n;
      }
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
void
nsCheapSet<EntryType>::Put(const KeyType aVal)
{
  switch (mState) {
    case ZERO:
      new (GetSingleEntry()) EntryType(EntryType::KeyToPointer(aVal));
      mState = ONE;
      return;
    case ONE: {
      nsTHashtable<EntryType>* table = new nsTHashtable<EntryType>();
      EntryType* entry = GetSingleEntry();
      table->PutEntry(entry->GetKey());
      entry->~EntryType();
      mUnion.table = table;
      mState = MANY;
    }
    MOZ_FALLTHROUGH;

    case MANY:
      mUnion.table->PutEntry(aVal);
      return;
    default:
      NS_NOTREACHED("bogus state");
      return;
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
