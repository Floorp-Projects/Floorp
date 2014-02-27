/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsHTMLEntities.h"



#include "nsString.h"
#include "nsCRT.h"
#include "pldhash.h"

using namespace mozilla;

struct EntityNode {
  const char* mStr; // never owns buffer
  int32_t       mUnicode;
};

struct EntityNodeEntry : public PLDHashEntryHdr
{
  const EntityNode* node;
}; 

static bool
  matchNodeString(PLDHashTable*, const PLDHashEntryHdr* aHdr,
                  const void* key)
{
  const EntityNodeEntry* entry = static_cast<const EntityNodeEntry*>(aHdr);
  const char* str = static_cast<const char*>(key);
  return (nsCRT::strcmp(entry->node->mStr, str) == 0);
}

static bool
  matchNodeUnicode(PLDHashTable*, const PLDHashEntryHdr* aHdr,
                   const void* key)
{
  const EntityNodeEntry* entry = static_cast<const EntityNodeEntry*>(aHdr);
  const int32_t ucode = NS_PTR_TO_INT32(key);
  return (entry->node->mUnicode == ucode);
}

static PLDHashNumber
  hashUnicodeValue(PLDHashTable*, const void* key)
{
  // key is actually the unicode value
  return PLDHashNumber(NS_PTR_TO_INT32(key));
  }


static const PLDHashTableOps EntityToUnicodeOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashStringKey,
  matchNodeString,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  nullptr,
}; 

static const PLDHashTableOps UnicodeToEntityOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  hashUnicodeValue,
  matchNodeUnicode,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  nullptr,
};

static PLDHashTable gEntityToUnicode = { 0 };
static PLDHashTable gUnicodeToEntity = { 0 };
static nsrefcnt gTableRefCnt = 0;

#define HTML_ENTITY(_name, _value) { #_name, _value },
static const EntityNode gEntityArray[] = {
#include "nsHTMLEntityList.h"
};
#undef HTML_ENTITY

#define NS_HTML_ENTITY_COUNT ((int32_t)ArrayLength(gEntityArray))

nsresult
nsHTMLEntities::AddRefTable(void) 
{
  if (!gTableRefCnt) {
    if (!PL_DHashTableInit(&gEntityToUnicode, &EntityToUnicodeOps,
                           nullptr, sizeof(EntityNodeEntry),
                           uint32_t(NS_HTML_ENTITY_COUNT / 0.75),
                           fallible_t())) {
      gEntityToUnicode.ops = nullptr;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!PL_DHashTableInit(&gUnicodeToEntity, &UnicodeToEntityOps,
                           nullptr, sizeof(EntityNodeEntry),
                           uint32_t(NS_HTML_ENTITY_COUNT / 0.75),
                           fallible_t())) {
      PL_DHashTableFinish(&gEntityToUnicode);
      gEntityToUnicode.ops = gUnicodeToEntity.ops = nullptr;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (const EntityNode *node = gEntityArray,
                 *node_end = ArrayEnd(gEntityArray);
         node < node_end; ++node) {

      // add to Entity->Unicode table
      EntityNodeEntry* entry =
        static_cast<EntityNodeEntry*>
                   (PL_DHashTableOperate(&gEntityToUnicode,
                                            node->mStr,
                                            PL_DHASH_ADD));
      NS_ASSERTION(entry, "Error adding an entry");
      // Prefer earlier entries when we have duplication.
      if (!entry->node)
        entry->node = node;

      // add to Unicode->Entity table
      entry = static_cast<EntityNodeEntry*>
                         (PL_DHashTableOperate(&gUnicodeToEntity,
                                                  NS_INT32_TO_PTR(node->mUnicode),
                                                  PL_DHASH_ADD));
      NS_ASSERTION(entry, "Error adding an entry");
      // Prefer earlier entries when we have duplication.
      if (!entry->node)
        entry->node = node;
    }
#ifdef DEBUG
    PL_DHashMarkTableImmutable(&gUnicodeToEntity);
    PL_DHashMarkTableImmutable(&gEntityToUnicode);
#endif
  }
  ++gTableRefCnt;
  return NS_OK;
}

void
nsHTMLEntities::ReleaseTable(void) 
{
  if (--gTableRefCnt != 0)
    return;

  if (gEntityToUnicode.ops) {
    PL_DHashTableFinish(&gEntityToUnicode);
    gEntityToUnicode.ops = nullptr;
  }
  if (gUnicodeToEntity.ops) {
    PL_DHashTableFinish(&gUnicodeToEntity);
    gUnicodeToEntity.ops = nullptr;
  }

}

int32_t 
nsHTMLEntities::EntityToUnicode(const nsCString& aEntity)
{
  NS_ASSERTION(gEntityToUnicode.ops, "no lookup table, needs addref");
  if (!gEntityToUnicode.ops)
    return -1;

    //this little piece of code exists because entities may or may not have the terminating ';'.
    //if we see it, strip if off for this test...

    if(';'==aEntity.Last()) {
      nsAutoCString temp(aEntity);
      temp.Truncate(aEntity.Length()-1);
      return EntityToUnicode(temp);
    }
      
  EntityNodeEntry* entry = 
    static_cast<EntityNodeEntry*>
               (PL_DHashTableOperate(&gEntityToUnicode, aEntity.get(), PL_DHASH_LOOKUP));

  if (!entry || PL_DHASH_ENTRY_IS_FREE(entry))
  return -1;
        
  return entry->node->mUnicode;
}


int32_t 
nsHTMLEntities::EntityToUnicode(const nsAString& aEntity) {
  nsAutoCString theEntity; theEntity.AssignWithConversion(aEntity);
  if(';'==theEntity.Last()) {
    theEntity.Truncate(theEntity.Length()-1);
  }

  return EntityToUnicode(theEntity);
}


const char*
nsHTMLEntities::UnicodeToEntity(int32_t aUnicode)
{
  NS_ASSERTION(gUnicodeToEntity.ops, "no lookup table, needs addref");
  EntityNodeEntry* entry =
    static_cast<EntityNodeEntry*>
               (PL_DHashTableOperate(&gUnicodeToEntity, NS_INT32_TO_PTR(aUnicode), PL_DHASH_LOOKUP));
                   
  if (!entry || PL_DHASH_ENTRY_IS_FREE(entry))
  return nullptr;
    
  return entry->node->mStr;
}

#ifdef DEBUG
#include <stdio.h>

class nsTestEntityTable {
public:
   nsTestEntityTable() {
     int32_t value;
     nsHTMLEntities::AddRefTable();

     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_ENTITY_COUNT; ++i) {
       nsAutoString entity; entity.AssignWithConversion(gEntityArray[i].mStr);

       value = nsHTMLEntities::EntityToUnicode(entity);
       NS_ASSERTION(value != -1, "can't find entity");
       NS_ASSERTION(value == gEntityArray[i].mUnicode, "bad unicode value");

       entity.AssignWithConversion(nsHTMLEntities::UnicodeToEntity(value));
       NS_ASSERTION(entity.EqualsASCII(gEntityArray[i].mStr), "bad entity name");
     }

     // Make sure we don't find things that aren't there
     value = nsHTMLEntities::EntityToUnicode(nsAutoCString("@"));
     NS_ASSERTION(value == -1, "found @");
     value = nsHTMLEntities::EntityToUnicode(nsAutoCString("zzzzz"));
     NS_ASSERTION(value == -1, "found zzzzz");
     nsHTMLEntities::ReleaseTable();
   }
};
//nsTestEntityTable validateEntityTable;
#endif

