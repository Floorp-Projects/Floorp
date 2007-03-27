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

#include "nsHTMLEntities.h"



#include "nsString.h"
#include "nsCRT.h"
#include "prtypes.h"
#include "pldhash.h"

struct EntityNode {
  const char* mStr; // never owns buffer
  PRInt32       mUnicode;
};

struct EntityNodeEntry : public PLDHashEntryHdr
{
  const EntityNode* node;
}; 

PR_STATIC_CALLBACK(PRBool)
  matchNodeString(PLDHashTable*, const PLDHashEntryHdr* aHdr,
                  const void* key)
{
  const EntityNodeEntry* entry = NS_STATIC_CAST(const EntityNodeEntry*, aHdr);
  const char* str = NS_STATIC_CAST(const char*, key);
  return (nsCRT::strcmp(entry->node->mStr, str) == 0);
}

PR_STATIC_CALLBACK(PRBool)
  matchNodeUnicode(PLDHashTable*, const PLDHashEntryHdr* aHdr,
                   const void* key)
{
  const EntityNodeEntry* entry = NS_STATIC_CAST(const EntityNodeEntry*, aHdr);
  const PRInt32 ucode = NS_PTR_TO_INT32(key);
  return (entry->node->mUnicode == ucode);
}

PR_STATIC_CALLBACK(PLDHashNumber)
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
  nsnull,
}; 

static const PLDHashTableOps UnicodeToEntityOps = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  hashUnicodeValue,
  matchNodeUnicode,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  nsnull,
};

static PLDHashTable gEntityToUnicode = { 0 };
static PLDHashTable gUnicodeToEntity = { 0 };
static nsrefcnt gTableRefCnt = 0;

#define HTML_ENTITY(_name, _value) { #_name, _value },
static const EntityNode gEntityArray[] = {
#include "nsHTMLEntityList.h"
};
#undef HTML_ENTITY

#define NS_HTML_ENTITY_COUNT ((PRInt32)NS_ARRAY_LENGTH(gEntityArray))

nsresult
nsHTMLEntities::AddRefTable(void) 
{
  if (!gTableRefCnt) {
    if (!PL_DHashTableInit(&gEntityToUnicode, &EntityToUnicodeOps,
                           nsnull, sizeof(EntityNodeEntry),
                           PRUint32(NS_HTML_ENTITY_COUNT / 0.75))) {
      gEntityToUnicode.ops = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!PL_DHashTableInit(&gUnicodeToEntity, &UnicodeToEntityOps,
                           nsnull, sizeof(EntityNodeEntry),
                           PRUint32(NS_HTML_ENTITY_COUNT / 0.75))) {
      PL_DHashTableFinish(&gEntityToUnicode);
      gEntityToUnicode.ops = gUnicodeToEntity.ops = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (const EntityNode *node = gEntityArray,
                 *node_end = gEntityArray + NS_ARRAY_LENGTH(gEntityArray);
         node < node_end; ++node) {

      // add to Entity->Unicode table
      EntityNodeEntry* entry =
        NS_STATIC_CAST(EntityNodeEntry*,
                       PL_DHashTableOperate(&gEntityToUnicode,
                                            node->mStr,
                                            PL_DHASH_ADD));
      NS_ASSERTION(entry, "Error adding an entry");
      // Prefer earlier entries when we have duplication.
      if (!entry->node)
        entry->node = node;

      // add to Unicode->Entity table
      entry = NS_STATIC_CAST(EntityNodeEntry*,
                             PL_DHashTableOperate(&gUnicodeToEntity,
                                                  NS_INT32_TO_PTR(node->mUnicode),
                                                  PL_DHASH_ADD));
      NS_ASSERTION(entry, "Error adding an entry");
      // Prefer earlier entries when we have duplication.
      if (!entry->node)
        entry->node = node;
    }
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
    gEntityToUnicode.ops = nsnull;
  }
  if (gUnicodeToEntity.ops) {
    PL_DHashTableFinish(&gUnicodeToEntity);
    gUnicodeToEntity.ops = nsnull;
  }

}

PRInt32 
nsHTMLEntities::EntityToUnicode(const nsCString& aEntity)
{
  NS_ASSERTION(gEntityToUnicode.ops, "no lookup table, needs addref");
  if (!gEntityToUnicode.ops)
    return -1;

    //this little piece of code exists because entities may or may not have the terminating ';'.
    //if we see it, strip if off for this test...

    if(';'==aEntity.Last()) {
      nsCAutoString temp(aEntity);
      temp.Truncate(aEntity.Length()-1);
      return EntityToUnicode(temp);
    }
      
  EntityNodeEntry* entry = 
    NS_STATIC_CAST(EntityNodeEntry*,
                   PL_DHashTableOperate(&gEntityToUnicode, aEntity.get(), PL_DHASH_LOOKUP));

  if (!entry || PL_DHASH_ENTRY_IS_FREE(entry))
  return -1;
        
  return entry->node->mUnicode;
}


PRInt32 
nsHTMLEntities::EntityToUnicode(const nsAString& aEntity) {
  nsCAutoString theEntity; theEntity.AssignWithConversion(aEntity);
  if(';'==theEntity.Last()) {
    theEntity.Truncate(theEntity.Length()-1);
  }

  return EntityToUnicode(theEntity);
}


const char*
nsHTMLEntities::UnicodeToEntity(PRInt32 aUnicode)
{
  NS_ASSERTION(gUnicodeToEntity.ops, "no lookup table, needs addref");
  EntityNodeEntry* entry =
    NS_STATIC_CAST(EntityNodeEntry*,
                   PL_DHashTableOperate(&gUnicodeToEntity, NS_INT32_TO_PTR(aUnicode), PL_DHASH_LOOKUP));
                   
  if (!entry || PL_DHASH_ENTRY_IS_FREE(entry))
  return nsnull;
    
  return entry->node->mStr;
}

#ifdef NS_DEBUG
#include <stdio.h>

class nsTestEntityTable {
public:
   nsTestEntityTable() {
     PRInt32 value;
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
     value = nsHTMLEntities::EntityToUnicode(nsCAutoString("@"));
     NS_ASSERTION(value == -1, "found @");
     value = nsHTMLEntities::EntityToUnicode(nsCAutoString("zzzzz"));
     NS_ASSERTION(value == -1, "found zzzzz");
     nsHTMLEntities::ReleaseTable();
   }
};
//nsTestEntityTable validateEntityTable;
#endif

