/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

 
/* nsDllStore
 *
 * Stores dll and their accociated info in a hash keyed on the system format
 * full dll path name e.g C:\Program Files\Netscape\Program\raptor.dll
 *
 * NOTE: dll names are considered to be case sensitive.
 */

#include "plhash.h"
#include "xcDll.h"

class nsDllStore
{
private:
	PLHashTable *m_dllHashTable;

public:
	// Constructor
	nsDllStore(void);
	~nsDllStore(void);

	// Caller is not expected to delete nsDll returned
	// The nsDll returned in NOT removed from the hash
	nsDll* Get(const char *filename);
	PRBool Put(const char *filename, nsDll *dllInfo);

	// The nsDll returned is removed from the hash
	// Caller is expected to delete the returned nsDll
	nsDll* Remove(const char *filename);
};
