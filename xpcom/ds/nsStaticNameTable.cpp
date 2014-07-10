/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class to manage lookup of static names in a table. */

#include "nsCRT.h"

#include "nscore.h"
#include "mozilla/HashFunctions.h"
#include "nsISupportsImpl.h"

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "nsStaticNameTable.h"

using namespace mozilla;

struct NameTableKey
{
  NameTableKey(const nsAFlatCString* aKeyStr)
    : mIsUnichar(false)
  {
    mKeyStr.m1b = aKeyStr;
  }

  NameTableKey(const nsAFlatString* aKeyStr)
    : mIsUnichar(true)
  {
    mKeyStr.m2b = aKeyStr;
  }

  bool mIsUnichar;
  union
  {
    const nsAFlatCString* m1b;
    const nsAFlatString* m2b;
  } mKeyStr;
};

struct NameTableEntry : public PLDHashEntryHdr
{
  // no ownership here!
  const nsAFlatCString* mString;
  int32_t mIndex;
};

static bool
matchNameKeysCaseInsensitive(PLDHashTable*, const PLDHashEntryHdr* aHdr,
                             const void* aKey)
{
  const NameTableEntry* entry = static_cast<const NameTableEntry*>(aHdr);
  const NameTableKey* keyValue = static_cast<const NameTableKey*>(aKey);
  const nsAFlatCString* entryKey = entry->mString;

  if (keyValue->mIsUnichar) {
    return keyValue->mKeyStr.m2b->LowerCaseEqualsASCII(entryKey->get(),
                                                       entryKey->Length());
  }

  return keyValue->mKeyStr.m1b->LowerCaseEqualsASCII(entryKey->get(),
                                                     entryKey->Length());
}

/*
 * caseInsensitiveHashKey is just like PL_DHashStringKey except it
 * uses (*s & ~0x20) instead of simply *s.  This means that "aFOO" and
 * "afoo" and "aFoo" will all hash to the same thing.  It also means
 * that some strings that aren't case-insensensitively equal will hash
 * to the same value, but it's just a hash function so it doesn't
 * matter.
 */
static PLDHashNumber
caseInsensitiveStringHashKey(PLDHashTable* aTable, const void* aKey)
{
  PLDHashNumber h = 0;
  const NameTableKey* tableKey = static_cast<const NameTableKey*>(aKey);
  if (tableKey->mIsUnichar) {
    for (const char16_t* s = tableKey->mKeyStr.m2b->get();
         *s != '\0';
         s++) {
      h = AddToHash(h, *s & ~0x20);
    }
  } else {
    for (const unsigned char* s = reinterpret_cast<const unsigned char*>(
           tableKey->mKeyStr.m1b->get());
         *s != '\0';
         s++) {
      h = AddToHash(h, *s & ~0x20);
    }
  }
  return h;
}

static const struct PLDHashTableOps nametable_CaseInsensitiveHashTableOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  caseInsensitiveStringHashKey,
  matchNameKeysCaseInsensitive,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  nullptr,
};

nsStaticCaseInsensitiveNameTable::nsStaticCaseInsensitiveNameTable()
  : mNameArray(nullptr)
  , mNullStr("")
{
  MOZ_COUNT_CTOR(nsStaticCaseInsensitiveNameTable);
  mNameTable.ops = nullptr;
}

nsStaticCaseInsensitiveNameTable::~nsStaticCaseInsensitiveNameTable()
{
  if (mNameArray) {
    // manually call the destructor on placement-new'ed objects
    for (uint32_t index = 0; index < mNameTable.entryCount; index++) {
      mNameArray[index].~nsDependentCString();
    }
    nsMemory::Free((void*)mNameArray);
  }
  if (mNameTable.ops) {
    PL_DHashTableFinish(&mNameTable);
  }
  MOZ_COUNT_DTOR(nsStaticCaseInsensitiveNameTable);
}

bool
nsStaticCaseInsensitiveNameTable::Init(const char* const aNames[],
                                       int32_t aCount)
{
  NS_ASSERTION(!mNameArray, "double Init");
  NS_ASSERTION(!mNameTable.ops, "double Init");
  NS_ASSERTION(aNames, "null name table");
  NS_ASSERTION(aCount, "0 count");

  mNameArray = (nsDependentCString*)
    nsMemory::Alloc(aCount * sizeof(nsDependentCString));
  if (!mNameArray) {
    return false;
  }

  if (!PL_DHashTableInit(&mNameTable, &nametable_CaseInsensitiveHashTableOps,
                         nullptr, sizeof(NameTableEntry), aCount,
                         fallible_t())) {
    mNameTable.ops = nullptr;
    return false;
  }

  for (int32_t index = 0; index < aCount; ++index) {
    const char* raw = aNames[index];
#ifdef DEBUG
    {
      // verify invariants of contents
      nsAutoCString temp1(raw);
      nsDependentCString temp2(raw);
      ToLowerCase(temp1);
      NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
      NS_ASSERTION(nsCRT::IsAscii(raw),
                   "non-ascii string in table -- "
                   "case-insensitive matching won't work right");
    }
#endif
    // use placement-new to initialize the string object
    nsDependentCString* strPtr = &mNameArray[index];
    new (strPtr) nsDependentCString(raw);

    NameTableKey key(strPtr);

    NameTableEntry* entry =
      static_cast<NameTableEntry*>(PL_DHashTableOperate(&mNameTable, &key,
                                                        PL_DHASH_ADD));
    if (!entry) {
      continue;
    }

    NS_ASSERTION(entry->mString == 0, "Entry already exists!");

    entry->mString = strPtr;      // not owned!
    entry->mIndex = index;
  }
#ifdef DEBUG
  PL_DHashMarkTableImmutable(&mNameTable);
#endif
  return true;
}

int32_t
nsStaticCaseInsensitiveNameTable::Lookup(const nsACString& aName)
{
  NS_ASSERTION(mNameArray, "not inited");
  NS_ASSERTION(mNameTable.ops, "not inited");

  const nsAFlatCString& str = PromiseFlatCString(aName);

  NameTableKey key(&str);
  NameTableEntry* entry =
    static_cast<NameTableEntry*>(PL_DHashTableOperate(&mNameTable, &key,
                                                      PL_DHASH_LOOKUP));
  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    return nsStaticCaseInsensitiveNameTable::NOT_FOUND;
  }

  return entry->mIndex;
}

int32_t
nsStaticCaseInsensitiveNameTable::Lookup(const nsAString& aName)
{
  NS_ASSERTION(mNameArray, "not inited");
  NS_ASSERTION(mNameTable.ops, "not inited");

  const nsAFlatString& str = PromiseFlatString(aName);

  NameTableKey key(&str);
  NameTableEntry* entry =
    static_cast<NameTableEntry*>(PL_DHashTableOperate(&mNameTable, &key,
                                                      PL_DHASH_LOOKUP));
  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    return nsStaticCaseInsensitiveNameTable::NOT_FOUND;
  }

  return entry->mIndex;
}

const nsAFlatCString&
nsStaticCaseInsensitiveNameTable::GetStringValue(int32_t aIndex)
{
  NS_ASSERTION(mNameArray, "not inited");
  NS_ASSERTION(mNameTable.ops, "not inited");

  if ((NOT_FOUND < aIndex) && ((uint32_t)aIndex < mNameTable.entryCount)) {
    return mNameArray[aIndex];
  }
  return mNullStr;
}
