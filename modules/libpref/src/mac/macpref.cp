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
#include "ufilemgr.h"
#include "uprefd.h"

#include <Types.h>
#include <Resources.h>
#include <Memory.h>

/*
 * Mac-specific libpref routines
 */

extern "C" {
JSBool pref_InitInitialObjects();
}
Boolean pref_FindAutoAdminLib(FSSpec& spec);

static JSBool pref_ReadResource(short id)
{
	JSBool ok = JS_FALSE;
	Handle data;
	UInt32 datasize;
	data = GetResource('TEXT', id);
	
	if (data) {
		DetachResource( data );
		HNoPurge( data );
		MoveHHi( data );
		datasize = GetHandleSize(data);

		HLock(data);
		ok = (JSBool) PREF_QuietEvaluateJSBuffer((char*) *data, datasize);
		HUnlock(data);
		DisposeHandle(data);
	}

	return ok;
}

/*
 * Initialize default preference JavaScript buffers from
 * appropriate TEXT resources
 */
JSBool pref_InitInitialObjects()
{
	JSBool ok = pref_ReadResource(3000);		// initprefs
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
	
	return ok;
}

/*
 * Convert between cross-platform file/folder pathname strings
 * and Mac aliases flattened into binary strings
 */
PR_IMPLEMENT(int)
PREF_CopyPathPref(const char *pref_name, char ** return_buffer)
{
	int dirSize, result;
	AliasHandle aliasH = NULL;
	char *dirAliasBuf = NULL;
	OSErr err;
	FSSpec fileSpec;
	Boolean changed;

	result = PREF_CopyBinaryPref(pref_name, &dirAliasBuf, &dirSize);
	if (result != PREF_NOERROR)
		return result;

	// Cast to an alias record and resolve.
	err = PtrToHand(dirAliasBuf, &(Handle) aliasH, dirSize);
	free(dirAliasBuf);
	if (err != noErr)
		return PREF_ERROR; // not enough memory?

	err = ::ResolveAlias(NULL, aliasH, &fileSpec, &changed);
	DisposeHandle((Handle) aliasH);
	if (err != noErr)
		return PREF_ERROR; // bad alias
		
	*return_buffer = CFileMgr::EncodedPathNameFromFSSpec(fileSpec, TRUE);
	
	return PREF_NOERROR;
}

PR_IMPLEMENT(int)
PREF_SetPathPref(const char *pref_name, const char *path, XP_Bool set_default)
{
	FSSpec fileSpec;
	AliasHandle	aliasH;
	OSErr err = CFileMgr::FSSpecFromLocalUnixPath(path, &fileSpec);
	if (err != noErr)
		return PREF_ERROR;

	err = NewAlias(nil, &fileSpec, &aliasH);
	if (err != noErr)
		return PREF_ERROR;

	int result;
	Size bytes = GetHandleSize((Handle) aliasH);
	HLock((Handle) aliasH);
	
	if (set_default)
		result = PREF_SetDefaultBinaryPref(pref_name, *aliasH, bytes);
	else
		result = PREF_SetBinaryPref(pref_name, *aliasH, bytes);
	DisposeHandle((Handle) aliasH);

	return result;
}

/* Looks for AutoAdminLib in Essential Files and returns FSSpec */
Boolean
pref_FindAutoAdminLib(FSSpec& spec)
{
	spec = CPrefs::GetFilePrototype(CPrefs::RequiredGutsFolder);
	LString::CopyPStr("\pAutoAdminLib", spec.name, 32);
	
	if (!CFileMgr::FileExists(spec))
		LString::CopyPStr("\pAutoAdminPPCLib", spec.name, 32);
	
	return CFileMgr::FileExists(spec);
}

PR_IMPLEMENT(XP_Bool)
PREF_IsAutoAdminEnabled()
{
	FSSpec spec;
	return (XP_Bool) pref_FindAutoAdminLib(spec);
}

