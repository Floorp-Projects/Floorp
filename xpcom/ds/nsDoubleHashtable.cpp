/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDoubleHashtable.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsReadableUtils.h"

//
// KEY String
//
class PLDHashStringEntry : public PLDHashEntryHdr
{
public:
  PLDHashStringEntry(const nsAString& aString) :
    mKey(aString) { }

  nsString mKey;
};

PR_STATIC_CALLBACK(const void *)
PLDHashStringGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  PLDHashStringEntry *e = NS_STATIC_CAST(PLDHashStringEntry *, entry);

  return NS_STATIC_CAST(const nsAString *, &e->mKey);
}

PR_STATIC_CALLBACK(PLDHashNumber)
PLDHashStringHashKey(PLDHashTable *table, const void *key)
{
  const nsAString *str = NS_STATIC_CAST(const nsAString *, key);

  return HashString(*str);
}

PR_STATIC_CALLBACK(PRBool)
PLDHashStringMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *entry,
                        const void *key)
{
  const PLDHashStringEntry *e =
    NS_STATIC_CAST(const PLDHashStringEntry *, entry);
  const nsAString *str = NS_STATIC_CAST(const nsAString *, key);

  return str->Equals(e->mKey);
}

//
// ENTRY StringSupports
//
class PLDHashStringSupportsEntry : public PLDHashStringEntry
{
public:
  PLDHashStringSupportsEntry(const nsAString& aString) :
    PLDHashStringEntry(aString)
  {
  }
  ~PLDHashStringSupportsEntry() {
  }
  nsCOMPtr<nsISupports> mVal;
};

PR_STATIC_CALLBACK(void)
PLDHashStringSupportsClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  PLDHashStringSupportsEntry *e = NS_STATIC_CAST(PLDHashStringSupportsEntry *,
                                                 entry);
  // An entry is being cleared, let the entry down its own cleanup.
  e->~PLDHashStringSupportsEntry();
}

PR_STATIC_CALLBACK(void)
PLDHashStringSupportsInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                           const void *key)
{
  const nsAString *keyStr = NS_STATIC_CAST(const nsAString *, key);
  // Initialize the entry with placement new
  new (entry) PLDHashStringSupportsEntry(*keyStr);
}


#define INIT_STRING_HASH(_HASHNAME_) \
static PLDHashTableOps hash_table_ops = \
{ \
  PL_DHashAllocTable, \
  PL_DHashFreeTable, \
  PLDHashStringGetKey, \
  PLDHashStringHashKey, \
  PLDHashStringMatchEntry, \
  PL_DHashMoveEntryStub, \
  _HASHNAME_##ClearEntry, \
  PL_DHashFinalizeStub, \
  _HASHNAME_##InitEntry \
}; \
if (!mHashTable.ops) { \
  PRBool isLive = PL_DHashTableInit(&mHashTable, \
                                    &hash_table_ops, nsnull, \
                                    sizeof(_HASHNAME_##Entry), 16); \
  if (!isLive) { \
    mHashTable.ops = nsnull; \
    return NS_ERROR_OUT_OF_MEMORY; \
  } \
}


//
// HASH StringSupports
//
nsDoubleHashtableStringSupports::nsDoubleHashtableStringSupports()
{
  // MUST init mHashTable.ops to nsnull so that we can detect that the
  // hashtable has not been initialized.
  mHashTable.ops = nsnull;
};

nsDoubleHashtableStringSupports::~nsDoubleHashtableStringSupports()
{
  if (mHashTable.ops) {
    PL_DHashTableFinish(&mHashTable);
  }
}

nsresult
nsDoubleHashtableStringSupports::Init()
{
  INIT_STRING_HASH(PLDHashStringSupports)
  return NS_OK;
}

already_AddRefed<nsISupports>
nsDoubleHashtableStringSupports::Get(const nsAString& aKey)
{
  PLDHashStringSupportsEntry * entry =
    NS_STATIC_CAST(PLDHashStringSupportsEntry *,
                   PL_DHashTableOperate(&mHashTable, &aKey,
                                        PL_DHASH_LOOKUP));
  if (!PL_DHASH_ENTRY_IS_LIVE(entry)) {
    return nsnull;
  }

  nsISupports* supports = entry->mVal;
  NS_IF_ADDREF(supports);
  return supports;
}

nsresult
nsDoubleHashtableStringSupports::Put(const nsAString& aKey,
                                     nsISupports* aResult)
{
  PLDHashStringSupportsEntry * entry =
    NS_STATIC_CAST(PLDHashStringSupportsEntry *,
                   PL_DHashTableOperate(&mHashTable, &aKey,
                                        PL_DHASH_ADD));

  NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

  //
  // Add the entry
  //
  entry->mVal = aResult;

  return NS_OK;
}

nsresult
nsDoubleHashtableStringSupports::Remove(nsAString& aKey)
{
  PL_DHashTableOperate(&mHashTable, &aKey, PL_DHASH_REMOVE);
  return NS_OK;
}
