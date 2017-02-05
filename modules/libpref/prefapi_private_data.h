/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Data shared between prefapi.c and nsPref.cpp */

#ifndef prefapi_private_data_h
#define prefapi_private_data_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"

extern PLDHashTable* gHashTable;

namespace mozilla {
namespace dom {
class PrefSetting;
} // namespace dom
} // namespace mozilla

mozilla::UniquePtr<char*[]>
pref_savePrefs(PLDHashTable* aTable, uint32_t* aPrefCount);

nsresult
pref_SetPref(const mozilla::dom::PrefSetting& aPref);

int pref_CompareStrings(const void *v1, const void *v2, void* unused);
PrefHashEntry* pref_HashTableLookup(const char *key);

bool
pref_EntryHasAdvisablySizedValues(PrefHashEntry* aHashEntry);

void pref_GetPrefFromEntry(PrefHashEntry *aHashEntry,
                           mozilla::dom::PrefSetting* aPref);

size_t
pref_SizeOfPrivateData(mozilla::MallocSizeOf aMallocSizeOf);

#endif
