/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "xp_core.h"
#include "prefapi.h"
#include "jsapi.h"
#include "prlink.h"

#if 0
// beard these are old world APIs that must be excised in the new world.
#include "LString.h"
#include "ufilemgr.h"
#include "uprefd.h"
#endif

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

#if 0
//----------------------------------------------------------------------------------------
static JSBool pref_ReadResource(short id)
//----------------------------------------------------------------------------------------
{
	Handle data = GetResource('TEXT', id);	
	if (!data)
		return JS_FALSE;

	HLock(data);
	UInt32 datasize = GetHandleSize(data);

//	JSBool ok = (JSBool) PREF_QuietEvaluateJSBuffer((char*)*data, datasize);
	JSBool ok = PREF_EvaluateConfigScript(
		(char*)*data, datasize,
		NULL, // No file
		PR_FALSE, // Don't global context
		PR_FALSE, // No callbacks
		PR_FALSE // Don't skip first line.
		);
	ReleaseResource(data);
	return ok;
}
#endif // 0

#if 0
//----------------------------------------------------------------------------------------
extern "C" JSBool pref_InitInitialObjects()
// Initialize default preference JavaScript buffers from
// appropriate TEXT resources
//----------------------------------------------------------------------------------------
{
	if (gPrefResources <= 0)
	    return JS_FALSE;
	    
	short savedResFile = ::CurResFile();
	::UseResFile(gPrefResources);
	JSBool ok = pref_ReadResource(3000);		// initpref.js
	if (ok)
		ok = pref_ReadResource(3010);			// all.js
	if (ok)
		ok = pref_ReadResource(3016);			// mailnews.js
	if (ok)
		ok = pref_ReadResource(3017);			// editor.js
	if (ok)
		ok = pref_ReadResource(3018);			// security.js
	if (ok)
		ok = pref_ReadResource(3011);			// config.js
	if (ok)
		ok = pref_ReadResource(3015);			// macprefs.js
	
	::CloseResFile(gPrefResources);
	if (savedResFile != gPrefResources)
		::UseResFile(savedResFile);
	gPrefResources = -1;
	return ok;
}
#endif // 0

//----------------------------------------------------------------------------------------
PR_IMPLEMENT(PrefResult)
PREF_CopyPathPref(const char *pref_name, char ** return_buffer)
// Convert between cross-platform file/folder pathname strings
// and Mac aliases flattened into binary strings
//----------------------------------------------------------------------------------------
{
	int dirSize;
	char *dirAliasBuf = NULL;
	PrefResult result = PREF_CopyBinaryPref(pref_name, &dirAliasBuf, &dirSize);
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
#if 0
	FSSpec spec;
	return (XP_Bool) pref_FindAutoAdminLib(spec);
#endif
	return PR_FALSE;
}
