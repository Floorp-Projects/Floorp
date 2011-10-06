/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
