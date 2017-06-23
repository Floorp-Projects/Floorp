/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class to manage lookup of static names in a table. */

#include "nsCRT.h"

#include "nscore.h"
#include "mozilla/HashFunctions.h"
#include "nsISupportsImpl.h"

#include "nsStaticNameTable.h"

using namespace mozilla;

struct NameTableKey
{
  NameTableKey(const nsDependentCString aNameArray[],
               const nsCString* aKeyStr)
    : mNameArray(aNameArray)
    , mIsUnichar(false)
  {
    mKeyStr.m1b = aKeyStr;
  }

  NameTableKey(const nsDependentCString aNameArray[],
               const nsString* aKeyStr)
    : mNameArray(aNameArray)
    , mIsUnichar(true)
  {
    mKeyStr.m2b = aKeyStr;
  }

  const nsDependentCString* mNameArray;
  union
  {
    const nsCString* m1b;
    const nsString* m2b;
  } mKeyStr;
  bool mIsUnichar;
};

struct NameTableEntry : public PLDHashEntryHdr
{
  int32_t mIndex;
};

static bool
matchNameKeysCaseInsensitive(const PLDHashEntryHdr* aHdr, const void* aVoidKey)
{
  auto entry = static_cast<const NameTableEntry*>(aHdr);
  auto key = static_cast<const NameTableKey*>(aVoidKey);
  const nsDependentCString* name = &key->mNameArray[entry->mIndex];

  return key->mIsUnichar
       ? key->mKeyStr.m2b->LowerCaseEqualsASCII(name->get(), name->Length())
       : key->mKeyStr.m1b->LowerCaseEqualsASCII(name->get(), name->Length());
}

/*
 * caseInsensitiveHashKey is just like PLDHashTable::HashStringKey except it
 * uses (*s & ~0x20) instead of simply *s.  This means that "aFOO" and
 * "afoo" and "aFoo" will all hash to the same thing.  It also means
 * that some strings that aren't case-insensensitively equal will hash
 * to the same value, but it's just a hash function so it doesn't
 * matter.
 */
static PLDHashNumber
caseInsensitiveStringHashKey(const void* aKey)
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
  caseInsensitiveStringHashKey,
  matchNameKeysCaseInsensitive,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  nullptr,
};

nsStaticCaseInsensitiveNameTable::nsStaticCaseInsensitiveNameTable(
    const char* const aNames[], int32_t aLength)
  : mNameArray(nullptr)
  , mNameTable(&nametable_CaseInsensitiveHashTableOps,
               sizeof(NameTableEntry), aLength)
  , mNullStr("")
{
  MOZ_COUNT_CTOR(nsStaticCaseInsensitiveNameTable);

  MOZ_ASSERT(aNames, "null name table");
  MOZ_ASSERT(aLength, "0 length");

  mNameArray = (nsDependentCString*)
    moz_xmalloc(aLength * sizeof(nsDependentCString));

  for (int32_t index = 0; index < aLength; ++index) {
    const char* raw = aNames[index];
#ifdef DEBUG
    {
      // verify invariants of contents
      nsAutoCString temp1(raw);
      nsDependentCString temp2(raw);
      ToLowerCase(temp1);
      MOZ_ASSERT(temp1.Equals(temp2), "upper case char in table");
      MOZ_ASSERT(nsCRT::IsAscii(raw),
                 "non-ascii string in table -- "
                 "case-insensitive matching won't work right");
    }
#endif
    // use placement-new to initialize the string object
    nsDependentCString* strPtr = &mNameArray[index];
    new (strPtr) nsDependentCString(raw);

    NameTableKey key(mNameArray, strPtr);

    auto entry = static_cast<NameTableEntry*>(mNameTable.Add(&key, fallible));
    if (!entry) {
      continue;
    }

    // If the entry already exists it's unlikely but possible that its index is
    // zero, in which case this assertion won't fail. But if the index is
    // non-zero (highly likely) then it will fail. In other words, this
    // assertion is likely but not guaranteed to detect if an entry is already
    // used.
    MOZ_ASSERT(entry->mIndex == 0, "Entry already exists!");

    entry->mIndex = index;
  }
#ifdef DEBUG
  mNameTable.MarkImmutable();
#endif
}

nsStaticCaseInsensitiveNameTable::~nsStaticCaseInsensitiveNameTable()
{
  // manually call the destructor on placement-new'ed objects
  for (uint32_t index = 0; index < mNameTable.EntryCount(); index++) {
    mNameArray[index].~nsDependentCString();
  }
  free((void*)mNameArray);
  MOZ_COUNT_DTOR(nsStaticCaseInsensitiveNameTable);
}

int32_t
nsStaticCaseInsensitiveNameTable::Lookup(const nsACString& aName)
{
  NS_ASSERTION(mNameArray, "not inited");

  const nsCString& str = PromiseFlatCString(aName);

  NameTableKey key(mNameArray, &str);
  auto entry = static_cast<NameTableEntry*>(mNameTable.Search(&key));

  return entry ? entry->mIndex : nsStaticCaseInsensitiveNameTable::NOT_FOUND;
}

int32_t
nsStaticCaseInsensitiveNameTable::Lookup(const nsAString& aName)
{
  NS_ASSERTION(mNameArray, "not inited");

  const nsString& str = PromiseFlatString(aName);

  NameTableKey key(mNameArray, &str);
  auto entry = static_cast<NameTableEntry*>(mNameTable.Search(&key));

  return entry ? entry->mIndex : nsStaticCaseInsensitiveNameTable::NOT_FOUND;
}

const nsCString&
nsStaticCaseInsensitiveNameTable::GetStringValue(int32_t aIndex)
{
  NS_ASSERTION(mNameArray, "not inited");

  if ((NOT_FOUND < aIndex) && ((uint32_t)aIndex < mNameTable.EntryCount())) {
    return mNameArray[aIndex];
  }
  return mNullStr;
}
