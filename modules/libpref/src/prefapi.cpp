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

#include "jsapi.h"
#include "xp_core.h"
#include "xp_mcom.h"
#include "xp_qsort.h"
#include <errno.h>

#include "prefldap.h"
#include "prefapi.h"

#if defined(XP_MAC) || defined(XP_UNIX)
#include "fe_proto.h"
#endif
#if defined(XP_WIN) || defined(XP_OS2)
#include "npapi.h"
#include "assert.h"
#define NOT_NULL(X)	X
#define XP_ASSERT(X) assert(X)
#define LINEBREAK "\n"
#endif
#include "sechash.h"
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif

#if defined(XP_MAC) && defined (__MWERKS__)
/* Can't get the xp people to fix warnings... */
#pragma require_prototypes off
#endif

JSTaskState *			m_mochaTaskState = NULL;
JSContext *				m_mochaContext = NULL;
JSObject *				m_mochaPrefObject = NULL;
JSObject *			    m_GlobalConfigObject = NULL;

static char *				m_filename = NULL;
static char *				m_lifilename = NULL;
static struct CallbackNode*	m_Callbacks = NULL;
static XP_Bool				m_ErrorOpeningUserPrefs = FALSE;
static XP_Bool				m_CallbacksEnabled = FALSE;
static XP_Bool				m_IsAnyPrefLocked = FALSE;
static PRHashTable*			m_HashTable = NULL;

/* LI_STUFF - PREF_LILOCAL here to flag prefs as transferable or not */
typedef enum { PREF_LOCKED = 1, PREF_USERSET = 2, PREF_CONFIG = 4,
			   PREF_STRING = 8, PREF_INT = 16, PREF_BOOL = 32, 
			   PREF_LILOCAL = 64 } PrefType;
/* LI_STUFF  PREF_SETLI here to flag prefs as transferable or not */
typedef enum { PREF_SETDEFAULT, PREF_SETUSER, 
			   PREF_LOCK, PREF_SETCONFIG, PREF_SETLI } PrefAction;

#define PREF_IS_LOCKED(pref)			((pref)->flags & PREF_LOCKED)
#define PREF_IS_CONFIG(pref)			((pref)->flags & PREF_CONFIG)
#define PREF_HAS_USER_VALUE(pref)		((pref)->flags & PREF_USERSET)
#define PREF_HAS_LI_VALUE(pref)			((pref)->flags & PREF_LILOCAL) /* LI_STUFF */

typedef union
{
	char*		stringVal;
	int32		intVal;
	XP_Bool		boolVal;
} PrefValue;
 
typedef struct 
{
	PrefValue	defaultPref;
	PrefValue	userPref;
	uint8		flags;
} PrefNode;

static JSBool pref_HashJSPref(unsigned int argc, jsval *argv, PrefAction action);

/* Hash table allocation */
PR_IMPLEMENT(void *)
pref_AllocTable(void *pool, size_t size)
{
    return malloc(size);
}

PR_IMPLEMENT(void) 
pref_FreeTable(void *pool, void *item)
{
    free(item);		/* free items? */
}

PR_IMPLEMENT(PRHashEntry *)
pref_AllocEntry(void *pool, const void *key)
{
    return malloc(sizeof(PRHashEntry));
}

PR_IMPLEMENT(void)
pref_FreeEntry(void *pool, PRHashEntry *he, uint flag)
{
	PrefNode *pref = (PrefNode *) he->value;
	if (pref) {
		if (pref->flags & PREF_STRING) {
			XP_FREEIF(pref->defaultPref.stringVal);
			XP_FREEIF(pref->userPref.stringVal);
		}
		XP_FREE(he->value);
	}

    if (flag == HT_FREE_ENTRY) {
		XP_FREEIF((void *)he->key);
        XP_FREE(he);
	}
}

static PRHashAllocOps pref_HashAllocOps = {
	pref_AllocTable, pref_FreeTable,
	pref_AllocEntry, pref_FreeEntry
};

#include "prlink.h"
extern PRLibrary *pref_LoadAutoAdminLib(void);
PRLibrary *m_AutoAdminLib = NULL;

/* -- Privates */
struct CallbackNode {
	char*					domain;
	PrefChangedFunc			func;
	void*					data;
	struct CallbackNode*	next;
};

/* -- Prototypes */
int pref_DoCallback(const char* changed_pref);
int pref_OpenFile(const char* filename, XP_Bool is_error_fatal, XP_Bool verifyHash, XP_Bool bGlobalContext);
XP_Bool pref_VerifyLockFile(char* buf, long buflen);

int pref_GetCharPref(const char *pref_name, char * return_buffer, int * length, XP_Bool get_default);
int pref_CopyCharPref(const char *pref_name, char ** return_buffer, XP_Bool get_default);
int pref_GetIntPref(const char *pref_name,int32 * return_int, XP_Bool get_default);
int pref_GetBoolPref(const char *pref_name, XP_Bool * return_value, XP_Bool get_default);

JSBool PR_CALLBACK pref_BranchCallback(JSContext *cx, JSScript *script);
void pref_ErrorReporter(JSContext *cx, const char *message,JSErrorReport *report);
void pref_Alert(char* msg);
int pref_HashPref(const char *key, PrefValue value, PrefType type, PrefAction action);

/* -- Platform specific function extern */
#if !defined(XP_OS2)
extern JSBool pref_InitInitialObjects(void);
#endif

PRIVATE JSClass global_class = {
    "global", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

JSBool PR_CALLBACK pref_NativeDefaultPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK pref_NativeUserPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK pref_NativeLockPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK pref_NativeUnlockPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK pref_NativeSetConfig(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK pref_NativeGetPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK pref_NativeGetLDAPAttr(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
/* LI_STUFF add nativelilocalpref */
JSBool PR_CALLBACK pref_NativeLILocalPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
/* LI_STUFF add NativeLIUserPref - does both lilocal and user at once */
JSBool PR_CALLBACK pref_NativeLIUserPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
JSBool PR_CALLBACK pref_NativeLIDefPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);


/* LI_STUFF added localPref	pref_NativeLILocalPref,	1 */
PRIVATE JSFunctionSpec autoconf_methods[] = {
    { "pref",				pref_NativeDefaultPref,	2 },
    { "defaultPref",		pref_NativeDefaultPref,	2 },
    { "user_pref",			pref_NativeUserPref,	2 },
    { "lockPref",			pref_NativeLockPref,	2 },
    { "unlockPref",			pref_NativeUnlockPref,	1 },
    { "config",				pref_NativeSetConfig,	2 },
    { "getPref",			pref_NativeGetPref,		1 },
    { "getLDAPAttributes",	pref_NativeGetLDAPAttr, 4 },
    { "localPref",			pref_NativeLILocalPref,	1 },
    { "localUserPref",		pref_NativeLIUserPref,	2 },
    { "localDefPref",		pref_NativeLIDefPref,	2 },
    { NULL,                 NULL,                   0 }
};

PRIVATE JSPropertySpec autoconf_props[] = {
    {0}
};

PRIVATE JSClass autoconf_class = {
    "PrefConfig", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

int pref_OpenFile(const char* filename, XP_Bool is_error_fatal, XP_Bool verifyHash, XP_Bool bGlobalContext)
{
	int ok = PREF_ERROR;
	XP_File fp;
	XP_StatStruct stats;
	long fileLength;

	stats.st_size = 0;
	if ( stat(filename, &stats) == -1 )
		return PREF_ERROR;

	fileLength = stats.st_size;
	if (fileLength <= 1)
		return PREF_ERROR;
	fp = fopen(filename, "r");

	if (fp) {	
		char* readBuf = (char *) malloc(fileLength * sizeof(char));
		if (readBuf) {
			fileLength = XP_FileRead(readBuf, fileLength, fp);

			if ( verifyHash && pref_VerifyLockFile(readBuf, fileLength) == FALSE )
			{
				ok = PREF_BAD_LOCKFILE;
			}
			else if ( PREF_EvaluateConfigScript(readBuf, fileLength,
						filename, bGlobalContext, FALSE ) == JS_TRUE )
			{
				ok = PREF_NOERROR;
			}
			free(readBuf);
		}
		XP_FileClose(fp);
		
		/* If the user prefs file exists but generates an error,
		   don't clobber the file when we try to save it. */
		if ((!readBuf || ok != PREF_NOERROR) && is_error_fatal)
			m_ErrorOpeningUserPrefs = TRUE;
#ifdef XP_WIN
		if (m_ErrorOpeningUserPrefs && is_error_fatal)
			MessageBox(NULL,"Error in preference file (prefs.js).  Default preferences will be used.","Netscape - Warning", MB_OK);
#endif
	}
	JS_GC(m_mochaContext);
	return (ok);
}

/* Computes the MD5 hash of the given buffer (not including the first line)
   and verifies the first line of the buffer expresses the correct hash in the form:
   // xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
   where each 'xx' is a hex value. */
XP_Bool pref_VerifyLockFile(char* buf, long buflen)
{
	XP_Bool success = FALSE;
	const int obscure_value = 7;
	const long hash_length = 51;		/* len = 48 chars of MD5 + // + EOL */
    unsigned char digest[16];
    char szHash[64];

	/* Unobscure file by subtracting some value from every char. */
	unsigned int i;
	for (i = 0; i < buflen; i++) {
		buf[i] -= obscure_value;
	}

    if (buflen >= hash_length) {
    	const unsigned char magic_key[] = "VonGloda5652TX75235ISBN";
	    unsigned char *pStart = (unsigned char*) buf + hash_length;
	    unsigned int len;
	    
	    MD5Context * md5_cxt = MD5_NewContext();
		MD5_Begin(md5_cxt);
		
		/* start with the magic key */
		MD5_Update(md5_cxt, magic_key, sizeof(magic_key));

		MD5_Update(md5_cxt, pStart, buflen - hash_length);
		
		MD5_End(md5_cxt, digest, &len, 16);
		
		MD5_DestroyContext(md5_cxt, PR_TRUE);
		
	    XP_SPRINTF(szHash, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
	        (int)digest[0],(int)digest[1],(int)digest[2],(int)digest[3],
	        (int)digest[4],(int)digest[5],(int)digest[6],(int)digest[7],
	        (int)digest[8],(int)digest[9],(int)digest[10],(int)digest[11],
	        (int)digest[12],(int)digest[13],(int)digest[14],(int)digest[15]);

		success = ( strncmp((const char*) buf + 3, szHash, hash_length - 4) == 0 );
	}
	
	/*
	 * Should return 'success', but since the MD5 code is stubbed out,
	 * just return 'TRUE' until we have a replacement.
	 */
	return TRUE;
}

PR_IMPLEMENT(int)
PREF_ReadLIJSFile(char *filename)
{
	int ok;

    if (filename) m_lifilename = strdup(filename);

	ok = pref_OpenFile(filename, FALSE, FALSE, FALSE);

	return ok;
}

PR_IMPLEMENT(int)
PREF_ReadUserJSFile(char *filename)
{
	int ok = pref_OpenFile(filename, FALSE, FALSE, TRUE);

	return ok;
}

PR_IMPLEMENT(int)
PREF_Init(char *filename)
{
    JSBool ok = JS_TRUE;

	/* --ML hash test */
	if (!m_HashTable)
	m_HashTable = PR_NewHashTable(2048, PR_HashString, PR_CompareStrings,
		PR_CompareValues, &pref_HashAllocOps, NULL);
	if (!m_HashTable)
		return 0;

    if (filename) m_filename = strdup(filename);

    if (!m_mochaTaskState)
		m_mochaTaskState = JS_Init((uint32) 0xffffffffL);

    if (!m_mochaContext) {
		m_mochaContext = JS_NewContext(m_mochaTaskState, 8192);  /* ???? What size? */
		if (!m_mochaContext) {
			return 0;
		}

		JS_SetVersion(m_mochaContext, JSVERSION_1_2);

		m_GlobalConfigObject = JS_NewObject(m_mochaContext, &global_class, NULL, NULL);
		if (!m_GlobalConfigObject) 
		    return 0;

		if (!JS_InitStandardClasses(m_mochaContext, m_GlobalConfigObject))
		    return 0;

		JS_SetBranchCallback(m_mochaContext, pref_BranchCallback);
		JS_SetErrorReporter(m_mochaContext, NULL);

		m_mochaPrefObject = JS_DefineObject(m_mochaContext, m_GlobalConfigObject, 
						    "PrefConfig",
						    &autoconf_class, 
						    NULL, 
						    JSPROP_ENUMERATE|JSPROP_READONLY);
		
		if (m_mochaPrefObject) {
		    if (!JS_DefineProperties(m_mochaContext,
					     m_mochaPrefObject,
					     autoconf_props)) {
			return 0;
		    }

		    if (!JS_DefineFunctions(m_mochaContext,
					    m_mochaPrefObject,
					    autoconf_methods)) {
			return 0;
		    }

		}

#if !defined(XP_WIN) && !defined(XP_OS2)
		ok = pref_InitInitialObjects();
#endif
	}

	if (ok && filename) {
	    ok = (JSBool) (pref_OpenFile(filename, TRUE, FALSE, FALSE) == PREF_NOERROR);
	}
	else if (!ok) {
		m_ErrorOpeningUserPrefs = TRUE;
	}
	return ok;
}

PR_IMPLEMENT(int)
PREF_GetConfigContext(JSContext **js_context)
{
	if (!js_context) return FALSE;

	*js_context = NULL;
    if (m_mochaContext)
		*js_context = m_mochaContext;

	return TRUE;
}

PR_IMPLEMENT(int)
PREF_GetGlobalConfigObject(JSObject **js_object)
{
	if (!js_object) return FALSE;

	*js_object = NULL;
    if (m_GlobalConfigObject)
		*js_object = m_GlobalConfigObject;

	return TRUE;
}

PR_IMPLEMENT(int)
PREF_GetPrefConfigObject(JSObject **js_object)
{
	if (!js_object) return FALSE;

	*js_object = NULL;
    if (m_mochaPrefObject)
		*js_object = m_mochaPrefObject;

	return TRUE;
}

/* Frees the callback list. */
PR_IMPLEMENT(void)
PREF_Cleanup()
{
	struct CallbackNode* node = m_Callbacks;
	struct CallbackNode* next_node;
	
	while (node) {
		next_node = node->next;
		XP_FREE(node->domain);
		XP_FREE(node);
		node = next_node;
	}
	if (m_mochaContext) JS_DestroyContext(m_mochaContext);
	if (m_mochaTaskState) JS_Finish(m_mochaTaskState);                      
	m_mochaContext = NULL;
	m_mochaTaskState = NULL;
	
	if (m_HashTable)
		PR_HashTableDestroy(m_HashTable);
}

PR_IMPLEMENT(int)
PREF_ReadLockFile(const char *filename)
{
/*
	return pref_OpenFile(filename, FALSE, FALSE, TRUE);

	Lock files are obscured, and the security code to read them has
	been removed from the free source.  So don't even try to read one.
	This is benign: no one listens closely to this error return,
	and no one mourns the missing lock file.
*/
	return PREF_ERROR;
}

/* This is more recent than the below 3 routines which should be obsoleted */
PR_IMPLEMENT(JSBool)
PREF_EvaluateConfigScript(const char * js_buffer, size_t length,
	const char* filename, XP_Bool bGlobalContext, XP_Bool bCallbacks)
{
	JSBool ok;
	jsval result;
	JSObject* scope;
	JSErrorReporter errReporter;
	
	if (bGlobalContext)
		scope = m_GlobalConfigObject;
	else
		scope = m_mochaPrefObject;
		
	if (!m_mochaContext || !scope)
		return JS_FALSE;

	errReporter = JS_SetErrorReporter(m_mochaContext, pref_ErrorReporter);
	m_CallbacksEnabled = bCallbacks;

	ok = JS_EvaluateScript(m_mochaContext, scope,
			js_buffer, length, filename, 0, &result);
	
	m_CallbacksEnabled = TRUE;		/* ?? want to enable after reading user/lock file */
	JS_SetErrorReporter(m_mochaContext, errReporter);
	
	return ok;
}

PR_IMPLEMENT(int)
PREF_EvaluateJSBuffer(const char * js_buffer, size_t length)
{
/* old routine that no longer triggers callbacks */
	int ret;

	ret = PREF_QuietEvaluateJSBuffer(js_buffer, length);
	
	return ret;
}

PR_IMPLEMENT(int)
PREF_QuietEvaluateJSBuffer(const char * js_buffer, size_t length)
{
	JSBool ok;
	jsval result;
	
	if (!m_mochaContext || !m_mochaPrefObject)
		return PREF_NOT_INITIALIZED;

	ok = JS_EvaluateScript(m_mochaContext, m_mochaPrefObject,
			js_buffer, length, NULL, 0, &result);
	
	/* Hey, this really returns a JSBool */
	return ok;
}

PR_IMPLEMENT(int)
PREF_QuietEvaluateJSBufferWithGlobalScope(const char * js_buffer, size_t length)
{
	JSBool ok;
	jsval result;
	
	if (!m_mochaContext || !m_GlobalConfigObject)
		return PREF_NOT_INITIALIZED;
	
	ok = JS_EvaluateScript(m_mochaContext, m_GlobalConfigObject,
			js_buffer, length, NULL, 0, &result);
	
	/* Hey, this really returns a JSBool */
	return ok;
}

static char * str_escape(const char * original) {
	const char *p;
	char * ret_str, *q;

	if (original == NULL)
		return NULL;
	
	ret_str = malloc(2*strlen(original) + 1);  /* Paranoid worse case all slashes will free quickly */
	for(p = original, q=ret_str ; *p; p++, q++)
        switch(*p) {
            case '\\':
                q[0] = '\\';
				q[1] = '\\';
				q++;
                break;
            case '\"':
                q[0] = '\\';
				q[1] = '\"';
				q++;
                break;
            default:
				*q = *p;
                break;
        }
	*q = 0;
	return ret_str;
}

/*
// External calls
 */
PR_IMPLEMENT(int)
PREF_SetCharPref(const char *pref_name, const char *value)
{
	PrefValue pref;
	pref.stringVal = (char*) value;
	
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

PR_IMPLEMENT(int)
PREF_SetIntPref(const char *pref_name, int32 value)
{
	PrefValue pref;
	pref.intVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_INT, PREF_SETUSER);
}

PR_IMPLEMENT(int)
PREF_SetBoolPref(const char *pref_name, XP_Bool value)
{
	PrefValue pref;
	pref.boolVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_BOOL, PREF_SETUSER);
}

#if !defined(XP_WIN) && !defined(XP_OS2)
extern char *EncodeBase64Buffer(char *subject, long size);
extern char *DecodeBase64Buffer(char *subject);
#else
/* temporary to make windows into a DLL...add a assert if used */
char *EncodeBase64Buffer(char *subject, long size) 
{
	assert(0);
}

char *DecodeBase64Buffer(char *subject)
{
	assert(0);
}
#endif

PR_IMPLEMENT(int)
PREF_SetBinaryPref(const char *pref_name, void * value, long size)
{
	char* buf = EncodeBase64Buffer(value, size);

	if (buf) {
		PrefValue pref;
		pref.stringVal = buf;
		return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
	}
	else
		return PREF_ERROR;
}

PR_IMPLEMENT(int)
PREF_SetColorPref(const char *pref_name, uint8 red, uint8 green, uint8 blue)
{
	char colstr[63];
	PrefValue pref;
	XP_SPRINTF( colstr, "#%02X%02X%02X", red, green, blue);

	pref.stringVal = colstr;
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

#define MYGetboolVal(rgb)   ((uint8) ((rgb) >> 16))
#define MYGetGValue(rgb)   ((uint8) (((uint16) (rgb)) >> 8)) 
#define MYGetRValue(rgb)   ((uint8) (rgb)) 

PR_IMPLEMENT(int)
PREF_SetColorPrefDWord(const char *pref_name, uint32 colorref)
{
	int red,green,blue;
	char colstr[63];
	PrefValue pref;

	red = MYGetRValue(colorref);
	green = MYGetGValue(colorref);
	blue = MYGetboolVal(colorref);
	XP_SPRINTF( colstr, "#%02X%02X%02X", red, green, blue);

	pref.stringVal = colstr;
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

PR_IMPLEMENT(int)
PREF_SetRectPref(const char *pref_name, int16 left, int16 top, int16 right, int16 bottom)
{
	char rectstr[63];
	PrefValue pref;
	XP_SPRINTF( rectstr, "%d,%d,%d,%d", left, top, right, bottom);

	pref.stringVal = rectstr;
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

/*
// DEFAULT VERSIONS:  Call internal with (set_default == TRUE)
 */
PR_IMPLEMENT(int)
PREF_SetDefaultCharPref(const char *pref_name,const char *value)
{
	PrefValue pref;
	pref.stringVal = (char*) value;
	
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETDEFAULT);
}


PR_IMPLEMENT(int)
PREF_SetDefaultIntPref(const char *pref_name,int32 value)
{
	PrefValue pref;
	pref.intVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_INT, PREF_SETDEFAULT);
}

PR_IMPLEMENT(int)
PREF_SetDefaultBoolPref(const char *pref_name,XP_Bool value)
{
	PrefValue pref;
	pref.boolVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_BOOL, PREF_SETDEFAULT);
}

PR_IMPLEMENT(int)
PREF_SetDefaultBinaryPref(const char *pref_name,void * value,long size)
{
	char* buf = EncodeBase64Buffer(value, size);
	if (buf) {
		PrefValue pref;
		pref.stringVal = buf;
		return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETDEFAULT);
	}
	else
		return PREF_ERROR;
}

PR_IMPLEMENT(int)
PREF_SetDefaultColorPref(const char *pref_name, uint8 red, uint8 green, uint8 blue)
{
	char colstr[63];
	XP_SPRINTF( colstr, "#%02X%02X%02X", red, green, blue);

	return PREF_SetDefaultCharPref(pref_name, colstr);
}

PR_IMPLEMENT(int)
PREF_SetDefaultRectPref(const char *pref_name, int16 left, int16 top, int16 right, int16 bottom)
{
	char rectstr[63];
	XP_SPRINTF( rectstr, "%d,%d,%d,%d", left, top, right, bottom);

	return PREF_SetDefaultCharPref(pref_name, rectstr);
}


/* LI_STUFF this does the same as savePref except it omits the lilocal prefs from the file. */
PR_IMPLEMENT(int)
pref_saveLIPref(PRHashEntry *he, int i, void *arg)
{
	char **prefArray = (char**) arg;
	PrefNode *pref = (PrefNode *) he->value;
	if (pref && PREF_HAS_USER_VALUE(pref) && !PREF_HAS_LI_VALUE(pref)) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->userPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				XP_FREE(tmp_str);
			}
		}
		else if (pref->flags & PREF_INT) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %ld);" LINEBREAK,
				(char*) he->key, (long) pref->userPref.intVal);
		}
		else if (pref->flags & PREF_BOOL) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %s);" LINEBREAK, (char*) he->key,
				(pref->userPref.boolVal) ? "true" : "false");
		}

		prefArray[i] = XP_STRDUP(buf);
	} else if (pref && PREF_IS_LOCKED(pref) && !PREF_HAS_LI_VALUE(pref)) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->defaultPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				XP_FREE(tmp_str);
			}
		}
		else if (pref->flags & PREF_INT) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %ld);" LINEBREAK,
				(char*) he->key, (long) pref->defaultPref.intVal);
		}
		else if (pref->flags & PREF_BOOL) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %s);" LINEBREAK, (char*) he->key,
				(pref->defaultPref.boolVal) ? "true" : "false");
		}

		prefArray[i] = XP_STRDUP(buf);
	}
	return 0;
}


PR_IMPLEMENT(int)
pref_savePref(PRHashEntry *he, int i, void *arg)
{
	char **prefArray = (char**) arg;
	PrefNode *pref = (PrefNode *) he->value;

	if (pref && PREF_HAS_USER_VALUE(pref)) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->userPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				XP_FREE(tmp_str);
			}
		}
		else if (pref->flags & PREF_INT) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %ld);" LINEBREAK,
				(char*) he->key, (long) pref->userPref.intVal);
		}
		else if (pref->flags & PREF_BOOL) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %s);" LINEBREAK, (char*) he->key,
				(pref->userPref.boolVal) ? "true" : "false");
		}

		prefArray[i] = XP_STRDUP(buf);
	} else if (pref && PREF_IS_LOCKED(pref)) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->defaultPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				XP_FREE(tmp_str);
			}
		}
		else if (pref->flags & PREF_INT) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %ld);" LINEBREAK,
				(char*) he->key, (long) pref->defaultPref.intVal);
		}
		else if (pref->flags & PREF_BOOL) {
			PR_snprintf(buf, 2048, "user_pref(\"%s\", %s);" LINEBREAK, (char*) he->key,
				(pref->defaultPref.boolVal) ? "true" : "false");
		}

		prefArray[i] = XP_STRDUP(buf);
	}
	/* LI_STUFF?? may need to write out the lilocal stuff here if it applies - probably won't support in 
		the prefs.js file. We won't need to worry about the user.js since it is read only.
	*/
	return 0;
}

PR_IMPLEMENT(int)
pref_CompareStrings (const void *v1, const void *v2)
{
	char *s1 = *(char**) v1;
	char *s2 = *(char**) v2;

	if (!s1)
	{
		if (!s2)
			return 0;
		else
			return -1;
	}
	else if (!s2)
		return 1;
	else
		return strcmp(s1, s2);
}

/* LI_STUFF  
this is new.  clients should use the old PREF_SavePrefFile or new PREF_SaveLIPrefFile.  
This is called by them and does the right thing.  
?? make this private to this file.
*/
PR_IMPLEMENT(int)
PREF_SavePrefFileWith(const char *filename, PRHashEnumerator heSaveProc) {
	int success = PREF_ERROR;
	FILE * fp;
	char **valueArray = NULL;
	int valueIdx;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	/* ?! Don't save (blank) user prefs if there was an error reading them */
#if defined(XP_WIN) || defined(XP_OS2)
	if (!filename)
#else
	if (!filename || m_ErrorOpeningUserPrefs)
#endif
		return PREF_NOERROR;

	valueArray = (char**) XP_CALLOC(sizeof(char*), m_HashTable->nentries);
	if (!valueArray)
		return PREF_OUT_OF_MEMORY;

	fp = fopen(filename, "w");
	if (fp) {
		XP_FileWrite("// Netscape User Preferences" LINEBREAK
			 "// This is a generated file!  Do not edit." LINEBREAK LINEBREAK,
			 -1, fp);
		
		/* LI_STUFF here we pass in the heSaveProc proc used so that li can do its own thing */
		PR_HashTableEnumerateEntries(m_HashTable, heSaveProc, valueArray);
		
		/* Sort the preferences to make a readable file on disk */
		XP_QSORT (valueArray, m_HashTable->nentries, sizeof(char*), pref_CompareStrings);
		for (valueIdx = 0; valueIdx < m_HashTable->nentries; valueIdx++)
		{
			if (valueArray[valueIdx])
			{
				XP_FileWrite(valueArray[valueIdx], -1, fp);
				XP_FREE(valueArray[valueIdx]);
			}
		}

		XP_FileClose(fp);
		success = PREF_NOERROR;
	}
	else 
		success = errno;

	XP_FREE(valueArray);

	return success;
}


PR_IMPLEMENT(int)
PREF_SavePrefFile()
{
	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;
	return (PREF_SavePrefFileWith(m_filename, pref_savePref));
}

/* LI_STUFF  Create a saveprefFile for LI for outsiders to call */
PR_IMPLEMENT(int)
PREF_SaveLIPrefFile(const char *filename)
{

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;
	return (PREF_SavePrefFileWith(((filename) ? filename : m_lifilename), pref_saveLIPref));
}

/* LI_STUFF  pass in the pref_savePref proc used instead of assuming it so that li can share that code too */
PR_IMPLEMENT(int)
PREF_SavePrefFileAs(const char *filename) {
	return PREF_SavePrefFileWith(filename, pref_savePref);
}

int pref_GetCharPref(const char *pref_name, char * return_buffer, int * length, XP_Bool get_default)
{
	int result = PREF_ERROR;
	char* stringVal;
	
	PrefNode* pref;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);

	if (pref) {
		if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
			stringVal = pref->defaultPref.stringVal;
		else
			stringVal = pref->userPref.stringVal;
		
		if (stringVal) {
			if (*length == 0) {
				*length = strlen(stringVal) + 1;
			}
			else {
				strncpy(return_buffer, stringVal, PR_MIN(*length - 1, strlen(stringVal) + 1));
				return_buffer[*length - 1] = '\0';
			}
			result = PREF_OK;
		}
	}
	return result;
}

int pref_CopyCharPref(const char *pref_name, char ** return_buffer, XP_Bool get_default)
{
	int result = PREF_ERROR;
	char* stringVal;	
	PrefNode* pref;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);

	if (pref && pref->flags & PREF_STRING) {
		if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
			stringVal = pref->defaultPref.stringVal;
		else
			stringVal = pref->userPref.stringVal;
		
		if (stringVal) {
			*return_buffer = XP_STRDUP(stringVal);
			result = PREF_OK;
		}
	}
	return result;
}

int pref_GetIntPref(const char *pref_name,int32 * return_int, XP_Bool get_default)
{
	int result = PREF_ERROR;	
	PrefNode* pref;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
	if (pref && pref->flags & PREF_INT) {
		if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
			*return_int = pref->defaultPref.intVal;
		else
			*return_int = pref->userPref.intVal;
		result = PREF_OK;
	}
	return result;
}

int pref_GetBoolPref(const char *pref_name, XP_Bool * return_value, XP_Bool get_default)
{
	int result = PREF_ERROR;
	PrefNode* pref;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);	
	if (pref && pref->flags & PREF_BOOL) {
		if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
			*return_value = pref->defaultPref.boolVal;
		else
			*return_value = pref->userPref.boolVal;
		result = PREF_OK;
	}
	return result;
}


PR_IMPLEMENT(int)
PREF_GetCharPref(const char *pref_name, char * return_buffer, int * length)
{
	return pref_GetCharPref(pref_name, return_buffer, length, FALSE);
}

PR_IMPLEMENT(int)
PREF_CopyCharPref(const char *pref_name, char ** return_buffer)
{
	return pref_CopyCharPref(pref_name, return_buffer, FALSE);
}

PR_IMPLEMENT(int)
PREF_GetIntPref(const char *pref_name,int32 * return_int)
{
	return pref_GetIntPref(pref_name, return_int, FALSE);
}

PR_IMPLEMENT(int)
PREF_GetBoolPref(const char *pref_name, XP_Bool * return_value)
{
	return pref_GetBoolPref(pref_name, return_value, FALSE);
}

PR_IMPLEMENT(int)
PREF_GetColorPref(const char *pref_name, uint8 *red, uint8 *green, uint8 *blue)
{
	char colstr[8];
	int iSize = 8;

	int result = PREF_GetCharPref(pref_name, colstr, &iSize);
	
	if (result == PREF_NOERROR) {
		int r, g, b;
		sscanf(colstr, "#%02X%02X%02X", &r, &g, &b);
		*red = r;
		*green = g;
		*blue = b;
	}
	
	return result;
}

#define MYRGB(r, g ,b)  ((uint32) (((uint8) (r) | ((uint16) (g) << 8)) | (((uint32) (uint8) (b)) << 16))) 

PR_IMPLEMENT(int)
PREF_GetColorPrefDWord(const char *pref_name, uint32 *colorref)
{
	char colstr[8];
	int iSize = 8;
	uint8 red, green, blue;

	int result = PREF_GetCharPref(pref_name, colstr, &iSize);
	
	if (result == 0) {
		int r, g, b;
		sscanf(colstr, "#%02X%02X%02X", &r, &g, &b);
		red = r;
		green = g;
		blue = b;
	}
	*colorref = MYRGB(red,green,blue);
	return result;
}

PR_IMPLEMENT(int)
PREF_GetRectPref(const char *pref_name, int16 *left, int16 *top, int16 *right, int16 *bottom)
{
	char rectstr[64];
	int iSize=64;
	int result = PREF_GetCharPref(pref_name, rectstr, &iSize);
	
	if (result == PREF_NOERROR) {
		int l, t, r, b;
		sscanf(rectstr, "%i,%i,%i,%i", &l, &t, &r, &b);
		*left = l;	*top = t;
		*right = r;	*bottom = b;
	}
	return result;
}

PR_IMPLEMENT(int)
PREF_GetBinaryPref(const char *pref_name, void * return_value, int *size)
{
	char* buf;
	int result;

	if (!m_mochaPrefObject || !return_value) return -1;

	result = PREF_CopyCharPref(pref_name, &buf);

	if (result == PREF_NOERROR) {
		char* debuf;
		if (strlen(buf) == 0) {		/* don't decode empty string ? */
			XP_FREE(buf);
			return -1;
		}
	
		debuf = DecodeBase64Buffer(buf);
		XP_MEMCPY(return_value, debuf, *size);
		
		XP_FREE(buf);
		XP_FREE(debuf);
	}
	return result;
}

typedef int (*CharPrefReadFunc)(const char*, char**);

static int
ReadCharPrefUsing(const char *pref_name, void** return_value, int *size, CharPrefReadFunc inFunc)
{
	char* buf;
	int result;

	if (!m_mochaPrefObject || !return_value)
		return -1;
	*return_value = NULL;

	result = inFunc(pref_name, &buf);

	if (result == PREF_NOERROR) {
		if (strlen(buf) == 0) {		/* do not decode empty string? */
			XP_FREE(buf);
			return -1;
		}
	
		*return_value = DecodeBase64Buffer(buf);
		*size = strlen(buf);
		
		XP_FREE(buf);
	}
	return result;
}

PR_IMPLEMENT(int)
PREF_CopyBinaryPref(const char *pref_name, void  ** return_value, int *size)
{
	return ReadCharPrefUsing(pref_name, return_value, size, PREF_CopyCharPref);
}

PR_IMPLEMENT(int)
PREF_CopyDefaultBinaryPref(const char *pref_name, void  ** return_value, int *size)
{
	return ReadCharPrefUsing(pref_name, return_value, size, PREF_CopyDefaultCharPref);
}

#ifndef XP_MAC
PR_IMPLEMENT(int)
PREF_CopyPathPref(const char *pref_name, char ** return_buffer)
{
	return PREF_CopyCharPref(pref_name, return_buffer);
}

PR_IMPLEMENT(int)
PREF_SetPathPref(const char *pref_name, const char *path, XP_Bool set_default)
{
	PrefAction action = set_default ? PREF_SETDEFAULT : PREF_SETUSER;
	PrefValue pref;
	pref.stringVal = (char*) path;
	
	return pref_HashPref(pref_name, pref, PREF_STRING, action);
}
#endif /* XP_MAC */


PR_IMPLEMENT(int)
PREF_GetDefaultCharPref(const char *pref_name, char * return_buffer, int * length)
{
	return pref_GetCharPref(pref_name, return_buffer, length, TRUE);
}

PR_IMPLEMENT(int)
PREF_CopyDefaultCharPref(const char *pref_name, char  ** return_buffer)
{
	return pref_CopyCharPref(pref_name, return_buffer, TRUE);
}

PR_IMPLEMENT(int)
PREF_GetDefaultIntPref(const char *pref_name, int32 * return_int)
{
	return pref_GetIntPref(pref_name, return_int, TRUE);
}

PR_IMPLEMENT(int)
PREF_GetDefaultBoolPref(const char *pref_name, XP_Bool * return_value)
{
	return pref_GetBoolPref(pref_name, return_value, TRUE);
}

PR_IMPLEMENT(int)
PREF_GetDefaultBinaryPref(const char *pref_name, void * return_value, int * length)
{
#ifdef XP_WIN
	assert( FALSE );
#else
	XP_ASSERT( FALSE );
#endif
	return TRUE;
}

PR_IMPLEMENT(int)
PREF_GetDefaultColorPref(const char *pref_name, uint8 *red, uint8 *green, uint8 *blue)
{
	char colstr[8];
	int iSize = 8;

	int result = PREF_GetDefaultCharPref(pref_name, colstr, &iSize);
	
	if (result == PREF_NOERROR) {
		int r, g, b;
		sscanf(colstr, "#%02X%02X%02X", &r, &g, &b);
		*red = r;
		*green = g;
		*blue = b;
	}

	return result;
}

PR_IMPLEMENT(int)
PREF_GetDefaultColorPrefDWord(const char *pref_name, uint32 * colorref)
{
	char colstr[8];
	int iSize = 8;
	uint8 red, green, blue;

	int result = PREF_GetDefaultCharPref(pref_name, colstr, &iSize);
	
	if (result == PREF_NOERROR) {
		int r, g, b;
		sscanf(colstr, "#%02X%02X%02X", &r, &g, &b);
		red = r;
		green = g;
		blue = b;
	}
	*colorref = MYRGB(red,green,blue);
	return result;
}

PR_IMPLEMENT(int)
PREF_GetDefaultRectPref(const char *pref_name, int16 *left, int16 *top, int16 *right, int16 *bottom)
{
	char rectstr[256];
	int iLen = 256;
	int result = PREF_GetDefaultCharPref(pref_name, (char *)&rectstr, &iLen);
	
	if (result == PREF_NOERROR) {
		sscanf(rectstr, "%d,%d,%d,%d", left, top, right, bottom);
	}
	return result;
}

/* Delete a branch. Used for deleting mime types */
PR_IMPLEMENT(int)
pref_DeleteItem(PRHashEntry *he, int i, void *arg)
{
	const char *to_delete = (const char *) arg;
	int len = strlen(to_delete);
	
	/* note if we're deleting "ldap" then we want to delete "ldap.xxx"
		and "ldap" (if such a leaf node exists) but not "ldap_1.xxx" */
	if (to_delete && (XP_STRNCMP(he->key, to_delete, len) == 0 ||
		(len-1 == strlen(he->key) && XP_STRNCMP(he->key, to_delete, len-1) == 0)))
		return HT_ENUMERATE_REMOVE;
	else
		return HT_ENUMERATE_NEXT;
}

PR_IMPLEMENT(int)
PREF_DeleteBranch(const char *branch_name)
{
	char* branch_dot = PR_smprintf("%s.", branch_name);
	if (!branch_dot)
		return PREF_OUT_OF_MEMORY;		

	PR_HashTableEnumerateEntries(m_HashTable, pref_DeleteItem, (void*) branch_dot);
	
	XP_FREE(branch_dot);
	return 0;
}

/* LI_STUFF  add a function to clear the li pref 
   does anyone use this??
*/
PR_IMPLEMENT(int)
PREF_ClearLIPref(const char *pref_name)
{
	int success = PREF_ERROR;
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
	if (pref && PREF_HAS_LI_VALUE(pref)) {
		pref->flags &= ~PREF_LILOCAL;
		if (m_CallbacksEnabled)
    		pref_DoCallback(pref_name);
		success = PREF_OK;
	}
    return success;
}



PR_IMPLEMENT(int)
PREF_ClearUserPref(const char *pref_name)
{
	int success = PREF_ERROR;
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
	if (pref && PREF_HAS_USER_VALUE(pref)) {
		pref->flags &= ~PREF_USERSET;
		if (m_CallbacksEnabled)
    		pref_DoCallback(pref_name);
		success = PREF_OK;
	}
    return success;
}

/* Prototype Admin Kit support */
PR_IMPLEMENT(int)
PREF_GetConfigString(const char *obj_name, char * return_buffer, int size,
	int index, const char *field)
{
#ifdef XP_WIN
	assert( FALSE );
#else
	XP_ASSERT( FALSE );
#endif
	return -1;
}

/*
 * Administration Kit support 
 */
PR_IMPLEMENT(int)
PREF_CopyConfigString(const char *obj_name, char **return_buffer)
{
	int success = PREF_ERROR;
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, obj_name);
	
	if (pref && pref->flags & PREF_STRING) {
    	if (return_buffer)
    		*return_buffer = XP_STRDUP(pref->defaultPref.stringVal);
    	success = PREF_NOERROR;
    }
    return success;
}

PR_IMPLEMENT(int)
PREF_CopyIndexConfigString(const char *obj_name,
	int index, const char *field, char **return_buffer)
{
	int success = PREF_ERROR;
	PrefNode* pref;
	char* setup_buf = PR_smprintf("%s_%d.%s", obj_name, index, field);

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, setup_buf);
	
	if (pref && pref->flags & PREF_STRING) {
    	if (return_buffer)
    		*return_buffer = XP_STRDUP(pref->defaultPref.stringVal);
    	success = PREF_NOERROR;
    }
    XP_FREEIF(setup_buf);
    return success;
}

PR_IMPLEMENT(int)
PREF_GetConfigInt(const char *obj_name, int32 *return_int)
{
	int success = PREF_ERROR;
	
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, obj_name);
	
	if (pref && pref->flags & PREF_INT) {
		*return_int = pref->defaultPref.intVal;
		success = PREF_NOERROR;
	}

	return success;
}

PR_IMPLEMENT(int)
PREF_GetConfigBool(const char *obj_name, XP_Bool *return_bool)
{
	int success = PREF_ERROR;
	
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, obj_name);
	
	if (pref && pref->flags & PREF_BOOL) {
		*return_bool = pref->defaultPref.boolVal;
		success = PREF_NOERROR;
	}

	return success;
}

/*
 * Hash table functions
 */
static XP_Bool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type)
{
	XP_Bool changed = TRUE;
	switch (type) {
		case PREF_STRING:
			if (oldValue.stringVal && newValue.stringVal)
				changed = (strcmp(oldValue.stringVal, newValue.stringVal) != 0);
			break;
		
		case PREF_INT:
			changed = oldValue.intVal != newValue.intVal;
			break;
			
		case PREF_BOOL:
			changed = oldValue.boolVal != newValue.boolVal;
			break;
	}
	return changed;
}

static void pref_SetValue(PrefValue* oldValue, PrefValue newValue, PrefType type)
{
	switch (type) {
		case PREF_STRING:
			XP_ASSERT(newValue.stringVal);
			XP_FREEIF(oldValue->stringVal);
			oldValue->stringVal = newValue.stringVal ? XP_STRDUP(newValue.stringVal) : NULL;
			break;
		
		default:
			*oldValue = newValue;
	}
}

int pref_HashPref(const char *key, PrefValue value, PrefType type, PrefAction action)
{
	PrefNode* pref;
	int result = PREF_OK;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
	if (!pref) {
    	pref = (PrefNode*) calloc(sizeof(PrefNode), 1);
    	if (!pref)	
    		return PREF_OUT_OF_MEMORY;
    	pref->flags = type;
		if (pref->flags & PREF_BOOL)
			pref->defaultPref.boolVal = (XP_Bool) -2;
		/* ugly hack -- define it to a default that no pref will ever default to
		   this should really get fixed right by some out of band data */
		if (pref->flags & PREF_INT)
			pref->defaultPref.intVal = (int32) -5632;
    	PR_HashTableAdd(m_HashTable, XP_STRDUP(key), pref);
    }
    else if (!(pref->flags & type)) {
		XP_ASSERT(0);			/* this shouldn't happen */
		return PREF_TYPE_CHANGE_ERR;
    }
    
    switch (action) {
    	case PREF_SETDEFAULT:
    	case PREF_SETCONFIG:
    		if (!PREF_IS_LOCKED(pref)) {		/* ?? change of semantics? */
				if (pref_ValueChanged(pref->defaultPref, value, type)) {
					pref_SetValue(&pref->defaultPref, value, type);
					if (!PREF_HAS_USER_VALUE(pref))
				    	result = PREF_VALUECHANGED;
			    }
			}
			if (action == PREF_SETCONFIG)
				pref->flags |= PREF_CONFIG;
			break;

		/* LI_STUFF  turn the li stuff on */
		case PREF_SETLI:
			if ( !PREF_HAS_LI_VALUE(pref) ||
					pref_ValueChanged(pref->userPref, value, type) ) {    	
				pref_SetValue(&pref->userPref, value, type);
				pref->flags |= PREF_LILOCAL;
				if (!PREF_IS_LOCKED(pref))
			    	result = PREF_VALUECHANGED;
		    }
			break;
			
		case PREF_SETUSER:
			/* If setting to the default value, then un-set the user value.
			   Otherwise, set the user value only if it has changed */
			if ( !pref_ValueChanged(pref->defaultPref, value, type) ) {
				if (PREF_HAS_USER_VALUE(pref)) {
					pref->flags &= ~PREF_USERSET;
					if (!PREF_IS_LOCKED(pref))
						result = PREF_VALUECHANGED;
				}
			}
			else if ( !PREF_HAS_USER_VALUE(pref) ||
					   pref_ValueChanged(pref->userPref, value, type) ) {    	
				pref_SetValue(&pref->userPref, value, type);
				pref->flags |= PREF_USERSET;
				if (!PREF_IS_LOCKED(pref))
			    	result = PREF_VALUECHANGED;
		    }
			break;
			
		case PREF_LOCK:
			if (pref_ValueChanged(pref->defaultPref, value, type)) {
				pref_SetValue(&pref->defaultPref, value, type);
		    	result = PREF_VALUECHANGED;
		    }
		    else if (!PREF_IS_LOCKED(pref)) {
		    	result = PREF_VALUECHANGED;
		    }
		    pref->flags |= PREF_LOCKED;
		    m_IsAnyPrefLocked = TRUE;
		    break;
	}

    if (result == PREF_VALUECHANGED && m_CallbacksEnabled) {
    	int result2 = pref_DoCallback(key);
    	if (result2 < 0)
    		result = result2;
    }
    return result;
}

PR_IMPLEMENT(int)
PREF_GetPrefType(const char *pref_name)
{
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
	if (pref) {
		if (pref->flags & PREF_STRING)
			return PREF_STRING;
		else if (pref->flags & PREF_INT)
			return PREF_INT;
		else if (pref->flags & PREF_BOOL)
			return PREF_BOOL;
	}
	return PREF_ERROR;
}

JSBool PR_CALLBACK pref_NativeDefaultPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
	return pref_HashJSPref(argc, argv, PREF_SETDEFAULT);
}

/* LI_STUFF    here is the hookup with js prefs calls */
JSBool PR_CALLBACK pref_NativeLILocalPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
    if (argc >= 1 && JSVAL_IS_STRING(argv[0])) {
    	const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
		
		if (pref && !PREF_HAS_LI_VALUE(pref)) {
			pref->flags |= PREF_LILOCAL;
			if (m_CallbacksEnabled) {
				pref_DoCallback(key);
			}
		}
	}
	return JS_TRUE;
}

/* combo li and user pref - save some time */
JSBool PR_CALLBACK pref_NativeLIUserPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
	return (JSBool)(pref_HashJSPref(argc, argv, PREF_SETUSER) && pref_HashJSPref(argc, argv, PREF_SETLI));
}

/* combo li and user pref - save some time */
JSBool PR_CALLBACK pref_NativeLIDefPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
	return (JSBool)(pref_HashJSPref(argc, argv, PREF_SETDEFAULT) && pref_HashJSPref(argc, argv, PREF_SETLI));
}

JSBool PR_CALLBACK pref_NativeUserPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
	return pref_HashJSPref(argc, argv, PREF_SETUSER);
}

JSBool PR_CALLBACK pref_NativeLockPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
	return pref_HashJSPref(argc, argv, PREF_LOCK);
}

JSBool PR_CALLBACK pref_NativeUnlockPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
    if (argc >= 1 && JSVAL_IS_STRING(argv[0])) {
    	const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
		
		if (pref && PREF_IS_LOCKED(pref)) {
			pref->flags &= ~PREF_LOCKED;
			if (m_CallbacksEnabled) {
				pref_DoCallback(key);
			}
		}
	}
	return JS_TRUE;
}

JSBool PR_CALLBACK pref_NativeSetConfig
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
	return pref_HashJSPref(argc, argv, PREF_SETCONFIG);
}

JSBool PR_CALLBACK pref_NativeGetPref
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
	void* value = NULL;
	PrefNode* pref;
	XP_Bool prefExists = TRUE;
	
    if (argc >= 1 && JSVAL_IS_STRING(argv[0]))
    {
    	const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
    	
    	if (pref) {
    		XP_Bool use_default = (PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref));
    			
    		if (pref->flags & PREF_STRING) {
    			char* str = use_default ? pref->defaultPref.stringVal : pref->userPref.stringVal;
    			JSString* jsstr = JS_NewStringCopyZ(cx, str);
    			*rval = STRING_TO_JSVAL(jsstr);
    		}
    		else if (pref->flags & PREF_INT) {
    			*rval = INT_TO_JSVAL(use_default ? pref->defaultPref.intVal : pref->userPref.intVal);
    		}
    		else if (pref->flags & PREF_BOOL) {
    			*rval = BOOLEAN_TO_JSVAL(use_default ? pref->defaultPref.boolVal : pref->userPref.boolVal);
    		}
    	}
    }
	return JS_TRUE;
}
/* -- */

PR_IMPLEMENT(XP_Bool)
PREF_PrefIsLocked(const char *pref_name)
{
	XP_Bool result = FALSE;
	if (m_IsAnyPrefLocked) {
		PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
		if (pref && PREF_IS_LOCKED(pref))
			result = TRUE;
	}
	
	return result;
}

/*
 * Creates an iterator over the children of a node.
 */
typedef struct 
{
	char*		childList;
	char*		parent;
	int			bufsize;
} PrefChildIter; 

/* if entry begins with the given string, i.e. if string is
  "a"
  and entry is
  "a.b.c" or "a.b"
  then add "a.b" to the list. */
PR_IMPLEMENT(int)
pref_addChild(PRHashEntry *he, int i, void *arg)
{
	PrefChildIter* pcs = (PrefChildIter*) arg;
	if ( XP_STRNCMP(he->key, pcs->parent, strlen(pcs->parent)) == 0 ) {
		char buf[512];
		char* nextdelim;
		int parentlen = strlen(pcs->parent);
		char* substring;
		XP_Bool substringBordersSeparator = FALSE;

		strncpy(buf, he->key, PR_MIN(512, strlen(he->key) + 1));
		nextdelim = buf + parentlen;
		if (parentlen < strlen(buf)) {
			/* Find the next delimiter if any and truncate the string there */
			nextdelim = strstr(nextdelim, ".");
			if (nextdelim) {
				*nextdelim = '\0';
			}
		}

		substring = strstr(pcs->childList, buf);
		if (substring)
		{
			int buflen = strlen(buf);
			XP_ASSERT(substring[buflen] > 0);
			substringBordersSeparator = (substring[buflen] == '\0' || substring[buflen] == ';');
		}

		if (!substring || !substringBordersSeparator) {
			int newsize = strlen(pcs->childList) + strlen(buf) + 2;
#ifdef XP_WIN16
			return HT_ENUMERATE_STOP;
#else
			if (newsize > pcs->bufsize) {
				pcs->bufsize *= 3;
				pcs->childList = (char*) realloc(pcs->childList, sizeof(char) * pcs->bufsize);
				if (!pcs->childList)
					return HT_ENUMERATE_STOP;
			}
#endif
			XP_STRCAT(pcs->childList, buf);
			XP_STRCAT(pcs->childList, ";");
		}
	}
	return 0;
}

PR_IMPLEMENT(int)
PREF_CreateChildList(const char* parent_node, char **child_list)
{
	PrefChildIter pcs;

#ifdef XP_WIN16
	pcs.bufsize = 20480;
#else
	pcs.bufsize = 2048;
#endif
	pcs.childList = (char*) malloc(sizeof(char) * pcs.bufsize);
	pcs.parent = PR_smprintf("%s.", parent_node);
	if (!pcs.parent || !pcs.childList)
		return PREF_OUT_OF_MEMORY;
	pcs.childList[0] = '\0';

	PR_HashTableEnumerateEntries(m_HashTable, pref_addChild, &pcs);

	*child_list = pcs.childList;
	XP_FREE(pcs.parent);
	
	return (pcs.childList == NULL) ? PREF_OUT_OF_MEMORY : PREF_OK;
}

PR_IMPLEMENT(char*)
PREF_NextChild(char *child_list, int *index)
{
	char* child = strtok(&child_list[*index], ";");
	if (child)
		*index += strlen(child) + 1;
	return child;
}

/* Adds a node to the beginning of the callback list. */
PR_IMPLEMENT(void)
PREF_RegisterCallback(const char *pref_node,
					   PrefChangedFunc callback,
					   void * instance_data)
{
	struct CallbackNode* node = (struct CallbackNode*) malloc(sizeof(struct CallbackNode));
	if (node) {
		node->domain = XP_STRDUP(pref_node);
		node->func = callback;
		node->data = instance_data;
		node->next = m_Callbacks;
		m_Callbacks = node;
	}
	return;
}

/* Deletes a node from the callback list. */
PR_IMPLEMENT(int)
PREF_UnregisterCallback(const char *pref_node,
						 PrefChangedFunc callback,
						 void * instance_data)
{
	int result = PREF_ERROR;
	struct CallbackNode* node = m_Callbacks;
	struct CallbackNode* prev_node = NULL;
	
	while (node != NULL)
	{
		if ( strcmp(node->domain, pref_node) == 0 &&
			 node->func == callback &&
			 node->data == instance_data )
		{
			struct CallbackNode* next_node = node->next;
			if (prev_node)
				prev_node->next = next_node;
			else
				m_Callbacks = next_node;
			XP_FREE(node->domain);
			XP_FREE(node);
			node = next_node;
			result = PREF_NOERROR;
		}
		else {
			prev_node = node;
			node = node->next;
		}
	}
	return result;
}

int pref_DoCallback(const char* changed_pref)
{
	int result = PREF_OK;
	struct CallbackNode* node;
	for (node = m_Callbacks; node != NULL; node = node->next)
	{
		if ( XP_STRNCMP(changed_pref, node->domain, strlen(node->domain)) == 0 ) {
			int result2 = (*node->func) (changed_pref, node->data);
			if (result2 != PREF_OK)
				result = result2;
		}
	}
	return result;
}

/* !! Front ends need to implement */
#ifndef XP_MAC
PR_IMPLEMENT(XP_Bool)
PREF_IsAutoAdminEnabled()
{
	if (m_AutoAdminLib == NULL)
		m_AutoAdminLib = pref_LoadAutoAdminLib();
	
	return (m_AutoAdminLib != NULL);
}
#endif

/* Called from JavaScript */
typedef char* (*ldap_func)(char*, char*, char*, char*, char**); 

JSBool PR_CALLBACK pref_NativeGetLDAPAttr
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
#ifdef MOZ_ADMIN_LIB
	ldap_func get_ldap_attributes = NULL;
#if (defined (XP_MAC) && defined(powerc)) || defined (XP_WIN) || defined(XP_UNIX)
	if (m_AutoAdminLib == NULL) {
		m_AutoAdminLib = pref_LoadAutoAdminLib();
	}
		
	if (m_AutoAdminLib) {
		get_ldap_attributes = (ldap_func)
#ifndef NSPR20
			PR_FindSymbol(
#ifndef XP_WIN16
			"pref_get_ldap_attributes"
#else
			MAKEINTRESOURCE(1)
#endif
			, m_AutoAdminLib);
#else /* NSPR20 */
			PR_FindSymbol(
			 m_AutoAdminLib,
#ifndef XP_WIN16
			"pref_get_ldap_attributes"
#else
			MAKEINTRESOURCE(1)
#endif
			);
#endif /* NSPR20 */
	}
	if (get_ldap_attributes == NULL) {
		/* This indicates the AutoAdmin dll was not found. */
		*rval = JSVAL_NULL;
		return JS_TRUE;
	}
#else
	get_ldap_attributes = pref_get_ldap_attributes;
#endif /* MOZ_ADMIN_LIB */

	if (argc >= 4 && JSVAL_IS_STRING(argv[0])
		&& JSVAL_IS_STRING(argv[1])
		&& JSVAL_IS_STRING(argv[2])
		&& JSVAL_IS_STRING(argv[3])) {
		char *return_error = NULL;
		char *value = get_ldap_attributes(
			JS_GetStringBytes(JSVAL_TO_STRING(argv[0])),
			JS_GetStringBytes(JSVAL_TO_STRING(argv[1])),
			JS_GetStringBytes(JSVAL_TO_STRING(argv[2])),
			JS_GetStringBytes(JSVAL_TO_STRING(argv[3])),
			&return_error );
		
		if (value) {
			JSString* str = JS_NewStringCopyZ(cx, value);
			XP_FREE(value);
			if (str) {
				*rval = STRING_TO_JSVAL(str);
				return JS_TRUE;
			}
		}
		if (return_error) {
			pref_Alert(return_error);
		}
	}
#endif
	
	*rval = JSVAL_NULL;
	return JS_TRUE;
}

/* LI_STUFF ?? add some debugging stuff here. */
/* Dump debugging info in response to about:config.
 */
PR_IMPLEMENT(int)
pref_printDebugInfo(PRHashEntry *he, int i, void *arg)
{
	char *buf1, *buf2;
	PrefValue val;
	PrefChildIter* pcs = (PrefChildIter*) arg;
	PrefNode *pref = (PrefNode *) he->value;
	
	if (PREF_HAS_USER_VALUE(pref) && !PREF_IS_LOCKED(pref)) {
		buf1 = PR_smprintf("<font color=\"blue\">%s = ", (char*) he->key);
		val = pref->userPref;
	}
	else {
		buf1 = PR_smprintf("<font color=\"%s\">%s = ",
			PREF_IS_LOCKED(pref) ? "red" : (PREF_IS_CONFIG(pref) ? "black" : "green"),
			(char*) he->key);
		val = pref->defaultPref;
	}
	
	if (pref->flags & PREF_STRING) {
		buf2 = PR_smprintf("%s %s</font><br>", buf1, val.stringVal);
	}
	else if (pref->flags & PREF_INT) {
		buf2 = PR_smprintf("%s %d</font><br>", buf1, val.intVal);
	}
	else if (pref->flags & PREF_BOOL) {
		buf2 = PR_smprintf("%s %s</font><br>", buf1, val.boolVal ? "true" : "false");
	}
	
	if ((strlen(buf2) + strlen(pcs->childList) + 1) > pcs->bufsize) {
		pcs->bufsize *= 3;
		pcs->childList = (char*) realloc(pcs->childList, sizeof(char) * pcs->bufsize);
		if (!pcs->childList)
			return HT_ENUMERATE_STOP;
	}
	XP_STRCAT(pcs->childList, buf2);
	XP_FREE(buf1);
	XP_FREE(buf2);
	return 0;
}

PR_IMPLEMENT(char *)
PREF_AboutConfig()
{
	PrefChildIter pcs;
	pcs.bufsize = 8192;
	pcs.childList = (char*) malloc(sizeof(char) * pcs.bufsize);
	pcs.childList[0] = '\0';
	XP_STRCAT(pcs.childList, "<HTML>");	

	PR_HashTableEnumerateEntries(m_HashTable, pref_printDebugInfo, &pcs);
	
	return pcs.childList;
}

#define MAYBE_GC_BRANCH_COUNT_MASK	4095

JSBool PR_CALLBACK
pref_BranchCallback(JSContext *cx, JSScript *script)
{ 
	static uint32	count = 0;
	
	/*
	 * If we've been running for a long time, then try a GC to 
	 * free up some memory.
	 */	
	if ( (++count & MAYBE_GC_BRANCH_COUNT_MASK) == 0 )
		JS_MaybeGC(cx); 

#ifdef LATER
    JSDecoder *decoder;
    char *message;
    JSBool ok = JS_TRUE;

    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    if (decoder->window_context && ++decoder->branch_count == 1000000) {
	decoder->branch_count = 0;
	message = PR_smprintf("Lengthy %s still running.  Continue?",
			      lm_language_name);
	if (message) {
	    ok = FE_Confirm(decoder->window_context, message);
	    XP_FREE(message);
	}
    }
#endif
    return JS_TRUE;
}

/* copied from libmocha */
void
pref_ErrorReporter(JSContext *cx, const char *message,
				 JSErrorReport *report)
{
	char *last;

	int i, j, k, n;
	const char *s, *t;

	last = PR_sprintf_append(0, "An error occurred reading the startup configuration file.  "
		"Please contact your administrator.");

	last = PR_sprintf_append(last, LINEBREAK LINEBREAK);
	if (!report) {
		last = PR_sprintf_append(last, "%s\n", message);
	} else {
		if (report->filename)
			last = PR_sprintf_append(last, "%s, ",
									 report->filename, report->filename);
		if (report->lineno)
			last = PR_sprintf_append(last, "line %u: ", report->lineno);
		last = PR_sprintf_append(last, "%s. ", message);
		if (report->linebuf) {
			for (s = t = report->linebuf; *s != '\0'; s = t) {
				for (; t != report->tokenptr && *t != '<' && *t != '\0'; t++)
					;
				last = PR_sprintf_append(last, "%.*s", t - s, s);
				if (*t == '\0')
					break;
				last = PR_sprintf_append(last, (*t == '<') ? "" : "%c", *t);
				t++;
			}
		}
	}

	if (last) {
		pref_Alert(last);
		XP_FREE(last);
	}
}

/* Platform specific alert messages */
void pref_Alert(char* msg)
{
#if defined(XP_MAC) || defined(XP_UNIX) || defined(XP_OS2)
#if defined(XP_UNIX)
    if ( getenv("NO_PREF_SPAM") == NULL )
#endif
	FE_Alert(NULL, msg);
#endif
#if defined (XP_WIN)
		MessageBox (NULL, msg, "Netscape -- JS Preference Warning", MB_OK);
#endif
}


#ifdef XP_WIN16
#define ADMNLIBNAME "adm1640.dll"
#elif defined XP_WIN32 || defined XP_OS2
#define ADMNLIBNAME "adm3240.dll"
#elif defined XP_UNIX
#define ADMNLIBNAME "libAutoAdmin.so"
extern void fe_GetProgramDirectory(char *path, int len);
#else
#define ADMNLIBNAME "AutoAdmin"	/* internal fragment name */
#endif

/* Try to load AutoAdminLib */
PRLibrary *
pref_LoadAutoAdminLib()
{
	PRLibrary *lib = NULL;

#ifdef XP_MAC
	const char *oldpath = PR_GetLibraryPath();
	PR_SetLibraryPath( "/usr/local/netscape/" );
#endif

#ifdef XP_UNIX
	{
		char aalib[MAXPATHLEN];

		if (getenv("NS_ADMIN_LIB"))
		{
			lib = PR_LoadLibrary(getenv("NS_ADMIN_LIB"));
		}
		else
		{
			if (getenv("MOZILLA_HOME"))
			{
				strcpy(aalib, getenv("MOZILLA_HOME"));
				lib = PR_LoadLibrary(strcat(aalib, ADMNLIBNAME));
			}
			if (lib == NULL)
			{
				fe_GetProgramDirectory(aalib, sizeof(aalib)-1);
				lib = PR_LoadLibrary(strcat(aalib, ADMNLIBNAME));
			}
			if (lib == NULL)
			{
				(void) strcpy(aalib, "/usr/local/netscape/");
				lib = PR_LoadLibrary(strcat(aalib, ADMNLIBNAME));
			}
		}
	}
	/* Make sure it's really libAutoAdmin.so */
    
#ifndef NSPR20
	if ( lib && PR_FindSymbol("_POLARIS_SplashPro", lib) == NULL ) return NULL;
#else 
	if ( lib && PR_FindSymbol(lib, "_POLARIS_SplashPro") == NULL ) return NULL;
#endif
#else
	lib = PR_LoadLibrary( ADMNLIBNAME );
#endif

#ifdef XP_MAC
	PR_SetLibraryPath(oldpath);
#endif

	return lib;
}

/*
 * Native implementations of JavaScript functions
	pref		-> pref_NativeDefaultPref
	defaultPref -> "
	userPref	-> pref_NativeUserPref
	lockPref	-> pref_NativeLockPref
	unlockPref	-> pref_NativeUnlockPref
	getPref		-> pref_NativeGetPref
	config		-> pref_NativeSetConfig
 */
static JSBool pref_HashJSPref(unsigned int argc, jsval *argv, PrefAction action)
{	
#ifdef NOPE1987
	/* this is somehow fixing an internal compiler error for win16 */
	PrefValue value;
    const char *key;
	XP_Bool bIsBool, bIsInt, bIsString;

	;
	if (argc < 2)
		return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0]))
		return JS_FALSE;

	bIsBool = JSVAL_IS_BOOLEAN(argv[1]);
	bIsInt = JSVAL_IS_INT(argv[1]);
	bIsString = JSVAL_IS_STRING(argv[1]);

    key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    if (bIsString) {
    	value.stringVal = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
    	pref_HashPref(key, value, PREF_STRING, action);
    }
    
#else
    if (argc >= 2 && JSVAL_IS_STRING(argv[0])) {
		PrefValue value;
    	const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    	
    	if (JSVAL_IS_STRING(argv[1])) {
    		value.stringVal = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
    		pref_HashPref(key, value, PREF_STRING, action);
    	}
    	else if (JSVAL_IS_INT(argv[1])) {
    		value.intVal = JSVAL_TO_INT(argv[1]);
    		pref_HashPref(key, value, PREF_INT, action);
    	}
    	else if (JSVAL_IS_BOOLEAN(argv[1])) {
    		value.boolVal = JSVAL_TO_BOOLEAN(argv[1]);
    		pref_HashPref(key, value, PREF_BOOL, action);
    	}
    }
#endif

	return JS_TRUE;
}


