/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
