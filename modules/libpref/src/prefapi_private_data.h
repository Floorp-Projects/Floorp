/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* Data shared between prefapi.c and nsPref.cpp */

extern JSTaskState *		gMochaTaskState;
extern JSContext *			gMochaContext;
extern JSObject *			gMochaPrefObject;
extern JSObject *			gGlobalConfigObject;
extern JSClass				global_class;
extern JSClass				autoconf_class;
extern JSPropertySpec		autoconf_props[];
extern JSFunctionSpec       autoconf_methods[];

#ifdef PREF_SUPPORT_OLD_PATH_STRINGS
extern char *				gFileName;
extern char *				gLIFileName;
#endif /*PREF_SUPPORT_OLD_PATH_STRINGS*/

extern struct CallbackNode*	gCallbacks;
extern PRBool				gErrorOpeningUserPrefs;
extern PRBool				gCallbacksEnabled;
extern PRBool				gIsAnyPrefLocked;
extern PLHashTable*			gHashTable;
extern char *               gSavedLine;       
extern PLHashAllocOps       pref_HashAllocOps;

NSPR_BEGIN_EXTERN_C
PR_EXTERN(JSBool) PR_CALLBACK pref_BranchCallback(JSContext *cx, JSScript *script);
PR_EXTERN(PrefResult) pref_savePref(PLHashEntry *he, int i, void *arg);
PR_EXTERN(PrefResult) pref_saveLIPref(PLHashEntry *he, int i, void *arg);
PR_EXTERN(PRBool) pref_VerifyLockFile(char* buf, long buflen);
PR_EXTERN(PrefResult) PREF_SetSpecialPrefsLocal(void);
PR_EXTERN(int) pref_CompareStrings(const void *v1, const void *v2);
NSPR_END_EXTERN_C

/* Possibly exportable */
#if defined(__cplusplus)
PR_EXTERN(PrefResult) PREF_SavePrefFileSpecWith(
	const nsFileSpec& fileSpec,
	PLHashEnumerator heSaveProc);
#endif /*PREF_SUPPORT_OLD_PATH_STRINGS*/

