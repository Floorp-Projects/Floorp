/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsHTMLEntities.h"

#include "nsString.h"
#include "nsCRT.h"
#include "PLDHashTable.h"

using namespace mozilla;

struct EntityNode {
  const char* mStr; // never owns buffer
  int32_t       mUnicode;
};

struct EntityNodeEntry : public PLDHashEntryHdr
{
  const EntityNode* node;
};

static bool matchNodeUnicode(const PLDHashEntryHdr* aHdr, const void* key)
{
  const EntityNodeEntry* entry = static_cast<const EntityNodeEntry*>(aHdr);
  const int32_t ucode = NS_PTR_TO_INT32(key);
  return (entry->node->mUnicode == ucode);
}

static PLDHashNumber hashUnicodeValue(const void* key)
{
  // key is actually the unicode value
  return HashGeneric(key);
}

static const PLDHashTableOps UnicodeToEntityOps = {
  hashUnicodeValue,
  matchNodeUnicode,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  nullptr,
};

static PLDHashTable* gUnicodeToEntity;
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
    gUnicodeToEntity = new PLDHashTable(&UnicodeToEntityOps,
                                        sizeof(EntityNodeEntry),
                                        NS_HTML_ENTITY_COUNT);
    for (const EntityNode *node = gEntityArray,
                 *node_end = ArrayEnd(gEntityArray);
         node < node_end; ++node) {

      // add to Unicode->Entity table
      auto entry =
        static_cast<EntityNodeEntry*>
          (gUnicodeToEntity->Add(NS_INT32_TO_PTR(node->mUnicode), fallible));
      NS_ASSERTION(entry, "Error adding an entry");
      // Prefer earlier entries when we have duplication.
      if (!entry->node)
        entry->node = node;
    }
#ifdef DEBUG
    gUnicodeToEntity->MarkImmutable();
#endif
  }
  ++gTableRefCnt;
  return NS_OK;
}

void
nsHTMLEntities::ReleaseTable(void)
{
  if (--gTableRefCnt != 0) {
    return;
  }

  delete gUnicodeToEntity;
  gUnicodeToEntity = nullptr;
}

const char*
nsHTMLEntities::UnicodeToEntity(int32_t aUnicode)
{
  NS_ASSERTION(gUnicodeToEntity, "no lookup table, needs addref");
  auto entry =
    static_cast<EntityNodeEntry*>
               (gUnicodeToEntity->Search(NS_INT32_TO_PTR(aUnicode)));

  return entry ? entry->node->mStr : nullptr;
}

