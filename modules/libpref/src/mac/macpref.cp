/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "xp_core.h"
#include "prefapi.h"
#include "jsapi.h"
#include "prlink.h"

#include "MacPrefUtils.h"

#ifndef __MEMORY__
#include <Memory.h>
#endif
#ifndef __ALIASES__
#include <Aliases.h>
#endif
#ifndef __CODEFRAGMENTS__
#include <CodeFragments.h>
#endif
#ifndef __RESOURCES__
#include <Resources.h>
#endif


/*
 * Mac-specific libpref routines
 */


//----------------------------------------------------------------------------------------
PR_IMPLEMENT(PrefResult)
PREF_CopyPathPref(const char *pref_name, char ** return_buffer, PRBool isDefault)
// Convert between cross-platform file/folder pathname strings
// and Mac aliases flattened into binary strings
//----------------------------------------------------------------------------------------
{
	int dirSize;
	char *dirAliasBuf = NULL;
	PrefResult result = PREF_CopyBinaryPref(pref_name, &dirAliasBuf, &dirSize, isDefault);
	if (result != PREF_NOERROR)
		return result;

	// Cast to an alias record and resolve.
	AliasHandle aliasH = NULL;
	OSErr err = PtrToHand(dirAliasBuf, &(Handle) aliasH, dirSize);
	free(dirAliasBuf);
	if (err != noErr)
		return PREF_ERROR; // not enough memory?

	FSSpec fileSpec;
	Boolean changed;
	err = ::ResolveAlias(NULL, aliasH, &fileSpec, &changed);
	DisposeHandle((Handle) aliasH);
	if (err != noErr)
		return PREF_ERROR; // bad alias
		
	*return_buffer = EncodedPathNameFromFSSpec(fileSpec, TRUE);
	
	return PREF_NOERROR;
}

//----------------------------------------------------------------------------------------
PR_IMPLEMENT(PrefResult)
PREF_SetPathPref(const char *pref_name, const char *path, PRBool set_default)
//----------------------------------------------------------------------------------------
{
	FSSpec fileSpec;
	AliasHandle	aliasH;
	OSErr err = FSSpecFromLocalUnixPath(path, &fileSpec);
	if (err != noErr)
		return PREF_ERROR;

	err = NewAlias(nil, &fileSpec, &aliasH);
	if (err != noErr)
		return PREF_ERROR;

	PrefResult result;
	Size bytes = GetHandleSize((Handle) aliasH);
	HLock((Handle) aliasH);
	
	if (set_default)
		result = PREF_SetDefaultBinaryPref(pref_name, *aliasH, bytes);
	else
		result = PREF_SetBinaryPref(pref_name, *aliasH, bytes);
	DisposeHandle((Handle) aliasH);

	return result;
}

//----------------------------------------------------------------------------------------
PR_IMPLEMENT(PRBool) PREF_IsAutoAdminEnabled()
//----------------------------------------------------------------------------------------
{
	return PR_FALSE;
}
