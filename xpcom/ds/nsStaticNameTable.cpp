/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
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

/* Class to manage lookup of static names in a table. */

#include "nsCRT.h"

#include "nscore.h"
#include "nsString.h"
#include "nsReadableUtils.h"

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "nsStaticNameTable.h"

struct NameTableEntry : public PLDHashEntryHdr
{
    // no ownership here!
    const char *mKey;
    PRInt32 mIndex;
};

PR_STATIC_CALLBACK(PRBool)
matchNameKeysCaseInsensitive(PLDHashTable*, const PLDHashEntryHdr* aHdr,
                             const void* key)
{
    const NameTableEntry* entry =
        NS_STATIC_CAST(const NameTableEntry *, aHdr);
    const char *keyValue = NS_STATIC_CAST(const char*, key);

    return (nsCRT::strcasecmp(entry->mKey, keyValue)==0);
}

/*
 * caseInsensitiveHashKey is just like PL_DHashStringKey except it
 * uses (*s & ~0x20) instead of simply *s.  This means that "aFOO" and
 * "afoo" and "aFoo" will all hash to the same thing.  It also means
 * that some strings that aren't case-insensensitively equal will hash
 * to the same value, but it's just a hash function so it doesn't
 * matter.
 */
PR_STATIC_CALLBACK(PLDHashNumber)
caseInsensitiveStringHashKey(PLDHashTable *table, const void *key)
{
    PLDHashNumber h = 0;
    for (const unsigned char* s =
           NS_STATIC_CAST(const unsigned char*, key);
         *s != '\0';
         s++)
        h = (h >> (PL_DHASH_BITS - 4)) ^ (h << 4) ^ (*s & ~0x20);
    return h;
}

static const struct PLDHashTableOps nametable_CaseInsensitiveHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    caseInsensitiveStringHashKey,
    matchNameKeysCaseInsensitive,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull,
};

nsStaticCaseInsensitiveNameTable::nsStaticCaseInsensitiveNameTable()
  : mNameArray(nsnull), mNullStr("")
{
    MOZ_COUNT_CTOR(nsStaticCaseInsensitiveNameTable);
    mNameTable.ops = nsnull;
}

nsStaticCaseInsensitiveNameTable::~nsStaticCaseInsensitiveNameTable()
{
    if (mNameArray) {
        // manually call the destructor on placement-new'ed objects
        for (PRUint32 index = 0; index < mNameTable.entryCount; index++) {
            mNameArray[index].~nsDependentCString();
        }
        nsMemory::Free((void*)mNameArray);
    }
    if (mNameTable.ops)
        PL_DHashTableFinish(&mNameTable);
    MOZ_COUNT_DTOR(nsStaticCaseInsensitiveNameTable);
}

PRBool 
nsStaticCaseInsensitiveNameTable::Init(const char* const aNames[], PRInt32 Count)
{
    NS_ASSERTION(!mNameArray, "double Init");
    NS_ASSERTION(!mNameTable.ops, "double Init");
    NS_ASSERTION(aNames, "null name table");
    NS_ASSERTION(Count, "0 count");

    mNameArray = (nsDependentCString*)
                   nsMemory::Alloc(Count * sizeof(nsDependentCString));
    if (!mNameArray)
        return PR_FALSE;

    if (!PL_DHashTableInit(&mNameTable,
                           &nametable_CaseInsensitiveHashTableOps,
                           nsnull, sizeof(NameTableEntry), Count)) {
        mNameTable.ops = nsnull;
        return PR_FALSE;
    }

    for (PRInt32 index = 0; index < Count; ++index) {
        const char* raw = aNames[index];
#ifdef DEBUG
        {
            // verify invariants of contents
            nsCAutoString temp1(raw);
            nsDependentCString temp2(raw);
            ToLowerCase(temp1);
            NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
            NS_ASSERTION(nsCRT::IsAscii(raw),
                         "non-ascii string in table -- "
                         "case-insensitive matching won't work right");
        }
#endif
        // use placement-new to initialize the string object
        new (&mNameArray[index]) nsDependentCString(raw);

        NameTableEntry *entry =
          NS_STATIC_CAST(NameTableEntry*,
                         PL_DHashTableOperate(&mNameTable, raw, PL_DHASH_ADD));

        if (!entry) continue;

        NS_ASSERTION(entry->mKey == 0, "Entry already exists!");

        entry->mKey = raw;      // not owned!
        entry->mIndex = index;
    }
    return PR_TRUE;
}

inline PRInt32
LookupFlatKeyword(const nsAFlatCString& aKeyword,
                  PLDHashTable& aTable)
{
    NameTableEntry *entry =
      NS_STATIC_CAST(NameTableEntry*,
                     PL_DHashTableOperate(&aTable, aKeyword.get(), PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(entry))
        return nsStaticCaseInsensitiveNameTable::NOT_FOUND;

    return entry->mIndex;
}

PRInt32
nsStaticCaseInsensitiveNameTable::Lookup(const nsACString& aName)
{
    NS_ASSERTION(mNameArray, "not inited");
    NS_ASSERTION(mNameTable.ops, "not inited");

    return LookupFlatKeyword(PromiseFlatCString(aName), mNameTable);
}

PRInt32
nsStaticCaseInsensitiveNameTable::Lookup(const nsAString& aName)
{
    NS_ASSERTION(mNameArray, "not inited");
    NS_ASSERTION(mNameTable.ops, "not inited");

    nsCAutoString cstring;
    cstring.AssignWithConversion(aName);
    return LookupFlatKeyword(cstring, mNameTable);
}

const nsAFlatCString& 
nsStaticCaseInsensitiveNameTable::GetStringValue(PRInt32 index)
{
    NS_ASSERTION(mNameArray, "not inited");
    NS_ASSERTION(mNameTable.ops, "not inited");

    if ((NOT_FOUND < index) && ((PRUint32)index < mNameTable.entryCount)) {
        return mNameArray[index];
    }
    return mNullStr;
}
