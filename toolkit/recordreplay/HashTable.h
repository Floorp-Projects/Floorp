/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_HashTable_h
#define mozilla_recordreplay_HashTable_h

#include "plhash.h"

namespace mozilla {
namespace recordreplay {

// Routines for creating specialized callbacks for PLHashTables that preserve
// iteration order, similar to those for PLDHashTables in RecordReplay.h.
void GeneratePLHashTableCallbacks(PLHashFunction* aKeyHash,
				  PLHashComparator* aKeyCompare,
				  PLHashComparator* aValueCompare,
				  const PLHashAllocOps** aAllocOps,
				  void** aAllocPrivate);
void DestroyPLHashTableCallbacks(void* aAllocPrivate);

} // recordreplay
} // mozilla

#endif // mozilla_recordreplay_HashTable_h
