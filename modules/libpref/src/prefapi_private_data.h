/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Data shared between prefapi.c and nsPref.cpp */

extern PLDHashTable			gHashTable;
extern bool                 gDirty;

struct PrefTuple;

enum pref_SaveTypes { SAVE_NONSHARED, SAVE_SHARED, SAVE_ALL, SAVE_ALL_AND_DEFAULTS };

// Passed as the arg to pref_savePref
struct pref_saveArgs {
  char **prefArray;
  pref_SaveTypes saveTypes;
};

PLDHashOperator
pref_savePref(PLDHashTable *table, PLDHashEntryHdr *heh, PRUint32 i, void *arg);

PLDHashOperator
pref_MirrorPrefs(PLDHashTable *table, PLDHashEntryHdr *heh, PRUint32 i, void *arg);

nsresult
pref_SetPrefTuple(const PrefTuple &aPref,bool set_default = false);

int pref_CompareStrings(const void *v1, const void *v2, void* unused);
PrefHashEntry* pref_HashTableLookup(const void *key);

void pref_GetTupleFromEntry(PrefHashEntry *aHashEntry, PrefTuple *aTuple);
