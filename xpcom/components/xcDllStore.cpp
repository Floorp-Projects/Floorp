/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* THIS FILE IS OBSOLETE */
 
/* nsDllStore
 *
 * Stores dll and their accociated info in a hash keyed on the system format
 * full dll path name e.g C:\Program Files\Netscape\Program\raptor.dll
 *
 * NOTE: dll names are considered to be case sensitive.
 */

#include "xcDllStore.h"

static PR_CALLBACK PRIntn _deleteDllInfo(PLHashEntry *he, PRIntn i, void *arg)
{
	delete (nsDll *)he->value;
	return (HT_ENUMERATE_NEXT);
}

nsDllStore::nsDllStore(void) : m_dllHashTable(NULL)
{
	PRUint32 initSize = 128;

	m_dllHashTable = PL_NewHashTable(initSize, PL_HashString,
		PL_CompareStrings, PL_CompareValues, NULL, NULL);
}


nsDllStore::~nsDllStore(void)
{
	if (m_dllHashTable)
	{
		// Delete each of the nsDll stored before deleting the Hash Table
		PL_HashTableEnumerateEntries(m_dllHashTable, _deleteDllInfo, NULL);
		PL_HashTableDestroy(m_dllHashTable);
	}
	m_dllHashTable = NULL;
}


nsDll* nsDllStore::Get(const char *dll)
{
	nsDll *dllInfo = NULL;
	if (m_dllHashTable)
	{
		dllInfo = (nsDll *)PL_HashTableLookup(m_dllHashTable, dll);
	}
	return (dllInfo);
}


nsDll* nsDllStore::Remove(const char *dll)
{
	if (m_dllHashTable == NULL)
	{
		return (NULL);
	}
	nsDll *dllInfo = Get(dll);
	PL_HashTableRemove(m_dllHashTable, dll);
	return (dllInfo);
}

PRBool nsDllStore::Put(const char *dll, nsDll *dllInfo)
{
	if (m_dllHashTable == NULL)
		return(PR_FALSE);

	PLHashEntry *entry =
		PL_HashTableAdd(m_dllHashTable, (void *)dll, (void *)dllInfo);
	return ((entry != NULL) ? PR_TRUE : PR_FALSE);
}
