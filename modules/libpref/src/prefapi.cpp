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

  /** USAGE NOTE: 
     <font color=red>

     This file (prefapi.c) is being obsoleted, and functions previously declared
     here are migrating to preffunc.cpp in this module.  If you make changes
     in this file, please be sure to check preffunc.cpp to ensure that similar
     changes are made in that file.
     
     Currently Windows uses preffunc.cpp and the other platforms use prefapi.c.

     </font>
  **/

#include "jsapi.h"
#if defined(XP_MAC)
#include <stat.h>
#else
#include <sys/stat.h>
#endif
#include <errno.h>
#ifdef _WIN32
#include "windows.h"
#endif 

#ifdef MOZ_ADMIN_LIB
#include "prefldap.h"
#endif
#include "prefapi.h"

#if defined(XP_MAC) || defined(XP_UNIX)
#include "fe_proto.h"
#endif
#if defined(XP_PC) || defined(XP_OS2)
#define LINEBREAK "\n"
#endif
#ifndef NO_SECURITY
#include "sechash.h"
#endif
#include "plstr.h"
#include "plhash.h"
#include "plbase64.h"
#include "prlog.h"
#include "prmem.h"
#include "prprf.h"
#include "xpassert.h"
#include "xp_str.h"

/* WHS TEMPORARY */
#define XP_QSORT qsort

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
static PRBool				m_ErrorOpeningUserPrefs = PR_FALSE;
static PRBool				m_CallbacksEnabled = PR_FALSE;
static PRBool				m_IsAnyPrefLocked = PR_FALSE;
static PRHashTable*			m_HashTable = NULL;
static char *               m_SavedLine = NULL;       

#define PREF_IS_LOCKED(pref)			((pref)->flags & PREF_LOCKED)
#define PREF_IS_CONFIG(pref)			((pref)->flags & PREF_CONFIG)
#define PREF_HAS_USER_VALUE(pref)		((pref)->flags & PREF_USERSET)
#define PREF_HAS_LI_VALUE(pref)			((pref)->flags & PREF_LILOCAL) /* LI_STUFF */
#define PREF_TYPE(pref)                 (PrefType)((pref)->flags & PREF_VALUETYPE_MASK)

typedef union
{
	char*		stringVal;
	int32		intVal;
	PRBool		boolVal;
} PrefValue;
 
typedef struct 
{
	PrefValue	defaultPref;
	PrefValue	userPref;
	uint8		flags;
} PrefNode;

static JSBool pref_HashJSPref(unsigned int argc, jsval *argv, PrefAction action);
static XP_Bool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type);

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
			PR_FREEIF(pref->defaultPref.stringVal);
			PR_FREEIF(pref->userPref.stringVal);
		}
		PR_Free(he->value);
	}

    if (flag == HT_FREE_ENTRY) {
		PR_FREEIF(he->key);
        PR_Free(he);
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
PrefResult pref_DoCallback(const char* changed_pref);
PrefResult pref_OpenFile(
    const char* filename,
    PRBool is_error_fatal,
    PRBool verifyHash,
    PRBool bGlobalContext,
    PRBool skipFirstLine);
PRBool pref_VerifyLockFile(char* buf, long buflen);

PrefResult pref_GetCharPref(const char *pref_name, char * return_buffer, int * length, PRBool get_default);
PrefResult pref_CopyCharPref(const char *pref_name, char ** return_buffer, PRBool get_default);
PrefResult pref_GetIntPref(const char *pref_name,int32 * return_int, PRBool get_default);
PrefResult pref_GetBoolPref(const char *pref_name, XP_Bool * return_value, PRBool get_default);

JSBool PR_CALLBACK pref_BranchCallback(JSContext *cx, JSScript *script);
void pref_ErrorReporter(JSContext *cx, const char *message,JSErrorReport *report);
void pref_Alert(char* msg);
PrefResult pref_HashPref(const char *key, PrefValue value, PrefType type, PrefAction action);

/* -- Platform specific function extern */
#if !defined(XP_WIN) && !defined(XP_OS2)
extern JSBool pref_InitInitialObjects(void);
#endif

static JSClass global_class = {
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
static JSFunctionSpec autoconf_methods[] = {
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

static JSPropertySpec autoconf_props[] = {
    {0}
};

static JSClass autoconf_class = {
    "PrefConfig", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

PrefResult pref_OpenFile(
    const char* filename,
    PRBool is_error_fatal,
    PRBool verifyHash,
    PRBool bGlobalContext,
    PRBool skipFirstLine)
{
	PrefResult ok = PREF_ERROR;
	FILE *fp;
	struct stat stats;
	size_t fileLength;

	stats.st_size = 0;
	if ( stat(filename, (struct stat *) &stats) == -1 )
		return PREF_ERROR;

	fileLength = stats.st_size;
	if (fileLength <= 1)
		return PREF_ERROR;
	fp = fopen(filename, "r");

	if (fp) {	
		char* readBuf = (char *) malloc(fileLength * sizeof(char));
		if (readBuf) {
			fileLength = fread(readBuf, sizeof(char), fileLength, fp);

			if ( verifyHash && pref_VerifyLockFile(readBuf, fileLength) == PR_FALSE )
			{
				ok = PREF_BAD_LOCKFILE;
			}
			else if ( PREF_EvaluateConfigScript(readBuf, fileLength,
						filename, bGlobalContext, PR_FALSE, skipFirstLine ) == JS_TRUE )
			{
				ok = PREF_NOERROR;
			}
			free(readBuf);
		}
		fclose(fp);
		
		/* If the user prefs file exists but generates an error,
		   don't clobber the file when we try to save it. */
		if ((!readBuf || ok != PREF_NOERROR) && is_error_fatal)
			m_ErrorOpeningUserPrefs = PR_TRUE;
#ifdef XP_PC
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
PRBool pref_VerifyLockFile(char* buf, long buflen)
{
#ifndef NO_SECURITY
	PRBool success = PR_FALSE;
	const int obscure_value = 7;
	const long hash_length = 51;		/* len = 48 chars of MD5 + // + EOL */
    unsigned char digest[16];
    char szHash[64];

	/* Unobscure file by subtracting some value from every char. */
	long i;
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
		
	    PR_snprintf(szHash, 64, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
	        (int)digest[0],(int)digest[1],(int)digest[2],(int)digest[3],
	        (int)digest[4],(int)digest[5],(int)digest[6],(int)digest[7],
	        (int)digest[8],(int)digest[9],(int)digest[10],(int)digest[11],
	        (int)digest[12],(int)digest[13],(int)digest[14],(int)digest[15]);

		success = ( PL_strncmp((const char*) buf + 3, szHash, hash_length - 4) == 0 );
	}
    return success;
#else	
	/*
	 * Should return 'success', but since the MD5 code is stubbed out,
	 * just return 'PR_TRUE' until we have a replacement.
	 */
	return PR_TRUE;
#endif
}

PR_IMPLEMENT(PrefResult)
PREF_ReadLIJSFile(char *filename)
{
	PrefResult ok;

    if (filename) m_lifilename = strdup(filename);

	ok = pref_OpenFile(filename, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE);

	return ok;
}

PR_IMPLEMENT(PrefResult)
PREF_ReadUserJSFile(char *filename)
{
	PrefResult ok = pref_OpenFile(filename, PR_FALSE, PR_FALSE, PR_TRUE, PR_FALSE);

	return ok;
}

PR_IMPLEMENT(PRBool)
PREF_Init(char *filename)
{
    PRBool ok = PR_TRUE;

	/* --ML hash test */
	if (!m_HashTable)
	m_HashTable = PR_NewHashTable(2048, PR_HashString, PR_CompareStrings,
		PR_CompareValues, &pref_HashAllocOps, NULL);
	if (!m_HashTable)
		return PR_FALSE;

    if (filename) {
        if (m_filename) /* happens if PREF_Init is called twice (it is) */
            free(m_filename);
        m_filename = strdup(filename);
    }

    if (!m_mochaTaskState)
		m_mochaTaskState = JS_Init((uint32) 0xffffffffL);

    if (!m_mochaContext) {
		m_mochaContext = JS_NewContext(m_mochaTaskState, 8192);  /* ???? What size? */
		if (!m_mochaContext) {
			return PR_FALSE;
		}

		JS_BeginRequest(m_mochaContext);

		m_GlobalConfigObject = JS_NewObject(m_mochaContext, &global_class, NULL, NULL);
		if (!m_GlobalConfigObject)  {
		    JS_EndRequest(m_mochaContext);
		    return PR_FALSE;
                }

                /* MLM - need a global object for set version call now. */
                JS_SetGlobalObject(m_mochaContext, m_GlobalConfigObject);

		JS_SetVersion(m_mochaContext, JSVERSION_1_2);

		if (!JS_InitStandardClasses(m_mochaContext, 
					    m_GlobalConfigObject))  {
		    JS_EndRequest(m_mochaContext);
		    return PR_FALSE;
	 	}

		JS_SetBranchCallback(m_mochaContext, pref_BranchCallback);
		JS_SetErrorReporter(m_mochaContext, NULL);

		m_mochaPrefObject = JS_DefineObject(m_mochaContext, 
						    m_GlobalConfigObject, 
						    "PrefConfig",
						    &autoconf_class, 
						    NULL, 
						    JSPROP_ENUMERATE|JSPROP_READONLY);
		
		if (m_mochaPrefObject) {
		    if (!JS_DefineProperties(m_mochaContext,
					     m_mochaPrefObject,
					     autoconf_props)) {
		        JS_EndRequest(m_mochaContext);
			return PR_FALSE;
		    }

		    if (!JS_DefineFunctions(m_mochaContext,
					    m_mochaPrefObject,
					    autoconf_methods)) {
		        JS_EndRequest(m_mochaContext);
			return PR_FALSE;
		    }

		}

#if !defined(XP_PC) && !defined(XP_OS2) && !defined(XP_MAC)
		ok = pref_InitInitialObjects();
#endif
	}  else  {
	    JS_BeginRequest(m_mochaContext);
	}

	if (ok && filename) {
	    ok = (JSBool) (pref_OpenFile(filename, PR_TRUE, PR_FALSE, PR_FALSE, PR_TRUE) == PREF_NOERROR);
	}
	else if (!ok) {
		m_ErrorOpeningUserPrefs = PR_TRUE;
	}
	JS_EndRequest(m_mochaContext);
	return ok;
}

PR_IMPLEMENT(PrefResult)
PREF_GetConfigContext(JSContext **js_context)
{
	if (!js_context) return PREF_ERROR;

	*js_context = NULL;
    if (m_mochaContext)  {
		*js_context = m_mochaContext;
		JS_SetContextThread(m_mochaContext);
    }

	return PREF_NOERROR;
}

PR_IMPLEMENT(PrefResult)
PREF_GetGlobalConfigObject(JSObject **js_object)
{
	if (!js_object) return PREF_ERROR;

	*js_object = NULL;
    if (m_GlobalConfigObject)
		*js_object = m_GlobalConfigObject;

	return PREF_NOERROR;
}

PR_IMPLEMENT(PrefResult)
PREF_GetPrefConfigObject(JSObject **js_object)
{
	if (!js_object) return PREF_ERROR;

	*js_object = NULL;
    if (m_mochaPrefObject)
		*js_object = m_mochaPrefObject;

	return PREF_NOERROR;
}

/* Frees the callback list. */
PR_IMPLEMENT(void)
PREF_Cleanup()
{
	struct CallbackNode* node = m_Callbacks;
	struct CallbackNode* next_node;
	
	while (node) {
		next_node = node->next;
		PR_Free(node->domain);
		PR_Free(node);
		node = next_node;
	}
	if (m_mochaContext) JS_DestroyContext(m_mochaContext);
	if (m_mochaTaskState) JS_Finish(m_mochaTaskState);                      
	m_mochaContext = NULL;
	m_mochaTaskState = NULL;
	
	if (m_HashTable)
		PR_HashTableDestroy(m_HashTable);
}

PR_IMPLEMENT(PrefResult)
PREF_ReadLockFile(const char *filename)
{
/*
	return pref_OpenFile(filename, PR_FALSE, PR_FALSE, PR_TRUE);

	Lock files are obscured, and the security code to read them has
	been removed from the free source.  So don't even try to read one.
	This is benign: no one listens closely to this error return,
	and no one mourns the missing lock file.
*/
	return PREF_ERROR;
}

void PREF_SetCallbacksStatus( PRBool status )
{
	m_CallbacksEnabled = status;
}

/* This is more recent than the below 3 routines which should be obsoleted */
PR_IMPLEMENT(JSBool)
PREF_EvaluateConfigScript(const char * js_buffer, size_t length,
	const char* filename, PRBool bGlobalContext, PRBool bCallbacks,
	PRBool skipFirstLine)
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

	JS_BeginRequest(m_mochaContext);
	errReporter = JS_SetErrorReporter(m_mochaContext, pref_ErrorReporter);
	m_CallbacksEnabled = bCallbacks;

    if (skipFirstLine) {
        /* In order to protect the privacy of the JavaScript preferences file 
         * from loading by the browser, we make the first line unparseable
         * by JavaScript. We must skip that line here before executing 
         * the JavaScript code.
         */
        unsigned int i=0;
        while (i < length) {
            char c = js_buffer[i++];
            if (c == '\r') {
                if (js_buffer[i] == '\n')
                    i++;
                break;
            }
            if (c == '\n')
                break;
        }
        m_SavedLine = malloc(i+1);
        if (!m_SavedLine)
            return JS_FALSE;
        memcpy(m_SavedLine, js_buffer, i);
        m_SavedLine[i] = '\0';
        length -= i;
        js_buffer += i;
    }

	ok = JS_EvaluateScript(m_mochaContext, scope,
			js_buffer, length, filename, 0, &result);
	
	m_CallbacksEnabled = PR_TRUE;		/* ?? want to enable after reading user/lock file */
	JS_SetErrorReporter(m_mochaContext, errReporter);
	
	JS_EndRequest(m_mochaContext);
	return ok;
}

PR_IMPLEMENT(JSBool)
PREF_EvaluateJSBuffer(const char * js_buffer, size_t length)
{
/* old routine that no longer triggers callbacks */
	JSBool ret;

	ret = PREF_QuietEvaluateJSBuffer(js_buffer, length);
	
	return ret;
}

PR_IMPLEMENT(JSBool)
PREF_QuietEvaluateJSBuffer(const char * js_buffer, size_t length)
{
	JSBool ok;
	jsval result;
	
	if (!m_mochaContext || !m_mochaPrefObject)
		return PREF_NOT_INITIALIZED;

	JS_BeginRequest(m_mochaContext);
	ok = JS_EvaluateScript(m_mochaContext, m_mochaPrefObject,
			js_buffer, length, NULL, 0, &result);
	
	JS_EndRequest(m_mochaContext);
	/* Hey, this really returns a JSBool */
	return ok;
}

PR_IMPLEMENT(JSBool)
PREF_QuietEvaluateJSBufferWithGlobalScope(const char * js_buffer, size_t length)
{
	JSBool ok;
	jsval result;
	
	if (!m_mochaContext || !m_GlobalConfigObject)
		return PREF_NOT_INITIALIZED;
	
	JS_BeginRequest(m_mochaContext);
	ok = JS_EvaluateScript(m_mochaContext, m_GlobalConfigObject,
			js_buffer, length, NULL, 0, &result);
	JS_EndRequest(m_mochaContext);
	
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
            case '\n':
				q[0] = '\\';
				q[1] = '\n';
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
** External calls
*/
PR_IMPLEMENT(PrefResult)
PREF_SetCharPref(const char *pref_name, const char *value)
{
	PrefValue pref;
	pref.stringVal = (char*) value;
	
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

PR_IMPLEMENT(PrefResult)
PREF_SetIntPref(const char *pref_name, int32 value)
{
	PrefValue pref;
	pref.intVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_INT, PREF_SETUSER);
}

PR_IMPLEMENT(PrefResult)
PREF_SetBoolPref(const char *pref_name, PRBool value)
{
	PrefValue pref;
	pref.boolVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_BOOL, PREF_SETUSER);
}

PR_IMPLEMENT(PrefResult)
PREF_SetBinaryPref(const char *pref_name, void * value, long size)
{
	char* buf = PL_Base64Encode(value, size, NULL);

	if (buf) {
		PrefValue pref;
		pref.stringVal = buf;
		return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
	}
	else
		return PREF_ERROR;
}

PR_IMPLEMENT(PrefResult)
PREF_SetColorPref(const char *pref_name, uint8 red, uint8 green, uint8 blue)
{
	char colstr[63];
	PrefValue pref;
	PR_snprintf( colstr, 63, "#%02X%02X%02X", red, green, blue);

	pref.stringVal = colstr;
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

#define MYGetboolVal(rgb)   ((uint8) ((rgb) >> 16))
#define MYGetGValue(rgb)   ((uint8) (((uint16) (rgb)) >> 8)) 
#define MYGetRValue(rgb)   ((uint8) (rgb)) 

PR_IMPLEMENT(PrefResult)
PREF_SetColorPrefDWord(const char *pref_name, uint32 colorref)
{
	int red,green,blue;
	char colstr[63];
	PrefValue pref;

	red = MYGetRValue(colorref);
	green = MYGetGValue(colorref);
	blue = MYGetboolVal(colorref);
	PR_snprintf( colstr, 63, "#%02X%02X%02X", red, green, blue);

	pref.stringVal = colstr;
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

PR_IMPLEMENT(PrefResult)
PREF_SetRectPref(const char *pref_name, int16 left, int16 top, int16 right, int16 bottom)
{
	char rectstr[63];
	PrefValue pref;
	PR_snprintf( rectstr, 63, "%d,%d,%d,%d", left, top, right, bottom);

	pref.stringVal = rectstr;
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

/*
** DEFAULT VERSIONS:  Call internal with (set_default == PR_TRUE)
*/
PR_IMPLEMENT(PrefResult)
PREF_SetDefaultCharPref(const char *pref_name,const char *value)
{
	PrefValue pref;
	pref.stringVal = (char*) value;
	
	return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETDEFAULT);
}


PR_IMPLEMENT(PrefResult)
PREF_SetDefaultIntPref(const char *pref_name,int32 value)
{
	PrefValue pref;
	pref.intVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_INT, PREF_SETDEFAULT);
}

PR_IMPLEMENT(PrefResult)
PREF_SetDefaultBoolPref(const char *pref_name,PRBool value)
{
	PrefValue pref;
	pref.boolVal = value;
	
	return pref_HashPref(pref_name, pref, PREF_BOOL, PREF_SETDEFAULT);
}

PR_IMPLEMENT(PrefResult)
PREF_SetDefaultBinaryPref(const char *pref_name,void * value,long size)
{
	char* buf = PL_Base64Encode(value, size, NULL);
	if (buf) {
		PrefValue pref;
		pref.stringVal = buf;
		return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETDEFAULT);
	}
	else
		return PREF_ERROR;
}

PR_IMPLEMENT(PrefResult)
PREF_SetDefaultColorPref(const char *pref_name, uint8 red, uint8 green, uint8 blue)
{
	char colstr[63];
	PR_snprintf( colstr, 63, "#%02X%02X%02X", red, green, blue);

	return PREF_SetDefaultCharPref(pref_name, colstr);
}

PR_IMPLEMENT(PrefResult)
PREF_SetDefaultRectPref(const char *pref_name, int16 left, int16 top, int16 right, int16 bottom)
{
	char rectstr[63];
	PR_snprintf( rectstr, 63, "%d,%d,%d,%d", left, top, right, bottom);

	return PREF_SetDefaultCharPref(pref_name, rectstr);
}


/* LI_STUFF this does the same as savePref except it omits the lilocal prefs from the file. */
PR_IMPLEMENT(int)
pref_saveLIPref(PRHashEntry *he, int i, void *arg)
{
	char **prefArray = (char**) arg;
	PrefNode *pref = (PrefNode *) he->value;
	if (pref && PREF_HAS_USER_VALUE(pref) && !PREF_HAS_LI_VALUE(pref) && 
		pref_ValueChanged(pref->defaultPref, 
						  pref->userPref, 
						  (PrefType) PREF_TYPE(pref))) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->userPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				PR_Free(tmp_str);
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

		prefArray[i] = PL_strdup(buf);
	} else if (pref && PREF_IS_LOCKED(pref) && !PREF_HAS_LI_VALUE(pref)) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->defaultPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				PR_Free(tmp_str);
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

		prefArray[i] = PL_strdup(buf);
	}
	return 0;
}


PR_IMPLEMENT(int)
pref_savePref(PRHashEntry *he, int i, void *arg)
{
	char **prefArray = (char**) arg;
	PrefNode *pref = (PrefNode *) he->value;

	PR_ASSERT(pref);
	if (!pref)
		return 0;

	if (PREF_HAS_USER_VALUE(pref) && 
		pref_ValueChanged(pref->defaultPref, 
						  pref->userPref, 
						  (PrefType) PREF_TYPE(pref))) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->userPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				PR_Free(tmp_str);
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

		prefArray[i] = PL_strdup(buf);
	} else if (pref && PREF_IS_LOCKED(pref)) {
		char buf[2048];

		if (pref->flags & PREF_STRING) {
			char *tmp_str = str_escape(pref->defaultPref.stringVal);
			if (tmp_str) {
				PR_snprintf(buf, 2048, "user_pref(\"%s\", \"%s\");" LINEBREAK,
					(char*) he->key, tmp_str);
				PR_Free(tmp_str);
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

		prefArray[i] = PL_strdup(buf);
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


PR_IMPLEMENT(PRBool)
pref_useDefaultPrefFile(void)
{
#if defined(XP_UNIX)
  return PREF_Init("preferences.js");
#elif defined(XP_MAC)
  return PREF_Init("Netscape Preferences");
#else /* XP_WIN */
  return PREF_Init("prefs.js");
#endif
}


/* LI_STUFF  
this is new.  clients should use the old PREF_SavePrefFile or new PREF_SaveLIPrefFile.  
This is called by them and does the right thing.  
?? make this private to this file.
*/

#define PREF_FILE_BANNER "// Netscape User Preferences" LINEBREAK \
			 "// This is a generated file!  Do not edit." LINEBREAK LINEBREAK

PR_IMPLEMENT(PrefResult)
PREF_SavePrefFileWith(const char *filename, PRHashEnumerator heSaveProc)
{
	PrefResult success = PREF_ERROR;
	FILE * fp;
	char **valueArray = NULL;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	/* ?! Don't save (blank) user prefs if there was an error reading them */
#if defined(XP_PC) || defined(XP_OS2)
	if (!filename)
#else
	if (!filename || m_ErrorOpeningUserPrefs)
#endif
		return PREF_NOERROR;

	valueArray = (char**) PR_Calloc(sizeof(char*), m_HashTable->nentries);
	if (!valueArray)
		return PREF_OUT_OF_MEMORY;

	fp = fopen(filename, "w");
	if (fp)
	{
		PRUint32 valueIdx;
		fwrite(PREF_FILE_BANNER, sizeof(char), PL_strlen(PREF_FILE_BANNER), fp);
		
		/* LI_STUFF here we pass in the heSaveProc proc used so that li can do its own thing */
		PR_HashTableEnumerateEntries(m_HashTable, heSaveProc, valueArray);
		
		/* Sort the preferences to make a readable file on disk */
		XP_QSORT(valueArray, m_HashTable->nentries, sizeof(char*), pref_CompareStrings);
		for (valueIdx = 0; valueIdx < m_HashTable->nentries; valueIdx++)
		{
			if (valueArray[valueIdx])
			{
				fwrite(valueArray[valueIdx], sizeof(char),
                       PL_strlen(valueArray[valueIdx]), fp);
				PR_Free(valueArray[valueIdx]);
			}
		}

		fclose(fp);
		success = PREF_NOERROR;
	}
	else 
		success = (PrefResult)errno; /* Really? */

	PR_Free(valueArray);

	return success;
}


PR_IMPLEMENT(PrefResult)
PREF_SavePrefFile()
{
#if 0 /* defined(XP_MAC) || defined(XP_PC) */
    return (PrefResult)pref_SaveProfile();
#else
    if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

    return (PREF_SavePrefFileWith(m_filename, pref_savePref));
#endif
}

/* 
 *  We need to flag a bunch of prefs as local that aren't initialized via all.js.
 *  This seems the safest way to do this.
 */
PR_IMPLEMENT(PrefResult)
PREF_SetSpecialPrefsLocal(void)
{
	static char *prefName[] = {
	"ab_dir.outliner_geometry",
	"addressbook.outliner_geometry",
	"bookmarks.outliner_geometry",
	"browser.bookmark_columns_win",
	"browser.history_file",
	"browser.sarcache.directory",
	"browser.user_history_file",
	"browser.win_height",
	"browser.win_width",
	"category.thread_columns_win",
	"comppicker.outliner_geometry",
	"editor.html_editor",
	"editor.template_history_0",
	"editor.template_last_loc",
	"editor.win_height",
	"editor.win_width",
	"helpers.global_mailcap_file",
	"helpers.global_mime_types_file",
	"helpers.private_mailcap_file",
	"helpers.private_mime_types_file",
	"history.outliner_geometry",
	"intl.font_charset",
	"intl.font_spec_list",
	"mail.addr_book.sliderwidth",
	"mail.addr_picker.sliderwidth",
	"mail.compose.win_height",
	"mail.compose.win_width",
	"mail.composition.addresspane.outliner_geometry",
	"mail.folder.win_height",
	"mail.folder.win_width",
	"mail.folderpane.outliner_geometry",
	"mail.imap.root_dir",
	"mail.mailfilter.outliner_geometry",
	"mail.msg.win_height",
	"mail.msg.win_width",
	"mail.subscribepane.all_groups.outliner_geometry",
	"mail.subscribepane.new_groups.outliner_geometry",
	"mail.subscribepane.search_groups.outliner_geometry",
	"mail.thread.win_height",
	"mail.thread.win_width",
	"mail.thread_columns_win",
	"mail.threadpane.messagepane_height",
	"mail.threadpane.outliner_geometry",
	"mailnews.3Pane_folder_columns_win",
	"mailnews.3pane_folder_width",
	"mailnews.3pane_thread_height",
	"mailnews.abook_columns_win",
	"mailnews.folder_columns_win",
	"mailnews.ldapsearch_columns_win",
	"news.thread_columns_win",
	"preferences.lang.outliner_geometry",
	"profile.name",
	"profile.numprofiles",
	"taskbar.x",
	"taskbar.y",
	"profile.directory"
	};
    PrefNode* pref;
	int       i;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	for (i = 0; i < (sizeof(prefName)/sizeof(prefName[0])); i++) {
		pref = (PrefNode*) PR_HashTableLookup(m_HashTable, prefName[i]);
		if (pref) 
			pref->flags |= PREF_LILOCAL;
	}
	return PREF_OK;
}

PR_IMPLEMENT(PrefResult)
PREF_SaveLIPrefFile(const char *filename)
{

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;
	PREF_SetSpecialPrefsLocal();
	return (PREF_SavePrefFileWith(((filename) ? filename : m_lifilename), pref_saveLIPref));
}

PR_IMPLEMENT(PrefResult)
PREF_SavePrefFileAs(const char *filename) {
	return PREF_SavePrefFileWith(filename, pref_savePref);
}

PrefResult pref_GetCharPref(const char *pref_name, char * return_buffer, int * length, PRBool get_default)
{
	PrefResult result = PREF_ERROR;
	char* stringVal;
	
	PrefNode* pref;

	if (!m_HashTable && !pref_useDefaultPrefFile())
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);

	if (pref)
	{
		if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
			stringVal = pref->defaultPref.stringVal;
		else
			stringVal = pref->userPref.stringVal;
		
		if (stringVal)
		{
			if (*length == 0)
				*length = strlen(stringVal) + 1;
			else
			{
				PL_strncpy(return_buffer, stringVal, PR_MIN(*length - 1, strlen(stringVal) + 1));
				return_buffer[*length - 1] = '\0';
			}
			result = PREF_OK;
		}
	}
	return result;
}

PrefResult pref_CopyCharPref(const char *pref_name, char ** return_buffer, PRBool get_default)
{
	PrefResult result = PREF_ERROR;
	char* stringVal;	
	PrefNode* pref;

	if (!m_HashTable && !pref_useDefaultPrefFile())
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);

	if (pref && pref->flags & PREF_STRING) {
		if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
			stringVal = pref->defaultPref.stringVal;
		else
			stringVal = pref->userPref.stringVal;
		
		if (stringVal) {
			*return_buffer = PL_strdup(stringVal);
			result = PREF_OK;
		}
	}
	return result;
}

PrefResult pref_GetIntPref(const char *pref_name,int32 * return_int, PRBool get_default)
{
	PrefResult result = PREF_ERROR;	
	PrefNode* pref;

	if (!m_HashTable && !pref_useDefaultPrefFile())
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

PrefResult pref_GetBoolPref(const char *pref_name, XP_Bool * return_value, PRBool get_default)
{
	PrefResult result = PREF_ERROR;
	PrefNode* pref;

	if (!m_HashTable && !pref_useDefaultPrefFile())
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


PR_IMPLEMENT(PrefResult)
PREF_GetCharPref(const char *pref_name, char * return_buffer, int * length)
{
	return pref_GetCharPref(pref_name, return_buffer, length, PR_FALSE);
}

PR_IMPLEMENT(PrefResult)
PREF_CopyCharPref(const char *pref_name, char ** return_buffer)
{
	return pref_CopyCharPref(pref_name, return_buffer, PR_FALSE);
}

PR_IMPLEMENT(PrefResult)
PREF_GetIntPref(const char *pref_name,int32 * return_int)
{
	return pref_GetIntPref(pref_name, return_int, PR_FALSE);
}

PR_IMPLEMENT(PrefResult)
PREF_GetBoolPref(const char *pref_name, XP_Bool * return_value)
{
	return pref_GetBoolPref(pref_name, return_value, PR_FALSE);
}

PR_IMPLEMENT(PrefResult)
PREF_GetColorPref(const char *pref_name, uint8 *red, uint8 *green, uint8 *blue)
{
	char colstr[8];
	int iSize = 8;

	PrefResult result = PREF_GetCharPref(pref_name, colstr, &iSize);
	
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

PR_IMPLEMENT(PrefResult)
PREF_GetColorPrefDWord(const char *pref_name, uint32 *colorref)
{
    uint8 red, green, blue;
    PrefResult   result;

    PR_ASSERT(colorref);

    result = PREF_GetColorPref(pref_name, &red, &green, &blue);

    if (result == PREF_NOERROR) {
    	*colorref = MYRGB(red,green,blue);
    }

	return result;
}

PR_IMPLEMENT(PrefResult)
PREF_GetRectPref(const char *pref_name, int16 *left, int16 *top, int16 *right, int16 *bottom)
{
	char rectstr[64];
	int iSize=64;
	PrefResult result = PREF_GetCharPref(pref_name, rectstr, &iSize);
	
	if (result == PREF_NOERROR) {
		int l, t, r, b;
		sscanf(rectstr, "%i,%i,%i,%i", &l, &t, &r, &b);
		*left = l;	*top = t;
		*right = r;	*bottom = b;
	}
	return result;
}

PR_IMPLEMENT(PrefResult)
PREF_GetBinaryPref(const char *pref_name, void * return_value, int *size)
{
	char* buf;
	PrefResult result;

	if (!m_mochaPrefObject || !return_value) return PREF_ERROR;

	result = PREF_CopyCharPref(pref_name, &buf);

	if (result == PREF_NOERROR) {
		if (strlen(buf) == 0) {		/* don't decode empty string ? */
			PR_Free(buf);
			return PREF_ERROR;
		}
	
		PL_Base64Decode(buf, *size, return_value);
		
		PR_Free(buf);
	}
	return result;
}

typedef PrefResult (*CharPrefReadFunc)(const char*, char**);

static PrefResult
ReadCharPrefUsing(const char *pref_name, void** return_value, int *size, CharPrefReadFunc inFunc)
{
	char* buf;
	PrefResult result;

	if (!m_mochaPrefObject || !return_value)
		return PREF_ERROR;
	*return_value = NULL;

	result = inFunc(pref_name, &buf);

	if (result == PREF_NOERROR) {
		if (strlen(buf) == 0) {		/* do not decode empty string? */
			PR_Free(buf);
			return PREF_ERROR;
		}
	
		*return_value = PL_Base64Decode(buf, 0, NULL);
		*size = strlen(buf);
		
		PR_Free(buf);
	}
	return result;
}

PR_IMPLEMENT(PrefResult)
PREF_CopyBinaryPref(const char *pref_name, void  ** return_value, int *size)
{
	return ReadCharPrefUsing(pref_name, return_value, size, PREF_CopyCharPref);
}

PR_IMPLEMENT(PrefResult)
PREF_CopyDefaultBinaryPref(const char *pref_name, void  ** return_value, int *size)
{
	return ReadCharPrefUsing(pref_name, return_value, size, PREF_CopyDefaultCharPref);
}

#ifndef XP_MAC
PR_IMPLEMENT(PrefResult)
PREF_CopyPathPref(const char *pref_name, char ** return_buffer)
{
	return PREF_CopyCharPref(pref_name, return_buffer);
}

PR_IMPLEMENT(PrefResult)
PREF_SetPathPref(const char *pref_name, const char *path, PRBool set_default)
{
	PrefAction action = set_default ? PREF_SETDEFAULT : PREF_SETUSER;
	PrefValue pref;
	pref.stringVal = (char*) path;
	
	return pref_HashPref(pref_name, pref, PREF_STRING, action);
}
#endif /* XP_MAC */


PR_IMPLEMENT(PrefResult)
PREF_GetDefaultCharPref(const char *pref_name, char * return_buffer, int * length)
{
	return pref_GetCharPref(pref_name, return_buffer, length, PR_TRUE);
}

PR_IMPLEMENT(PrefResult)
PREF_CopyDefaultCharPref(const char *pref_name, char  ** return_buffer)
{
	return pref_CopyCharPref(pref_name, return_buffer, PR_TRUE);
}

PR_IMPLEMENT(PrefResult)
PREF_GetDefaultIntPref(const char *pref_name, int32 * return_int)
{
	return pref_GetIntPref(pref_name, return_int, PR_TRUE);
}

PR_IMPLEMENT(PrefResult)
PREF_GetDefaultBoolPref(const char *pref_name, XP_Bool * return_value)
{
	return pref_GetBoolPref(pref_name, return_value, PR_TRUE);
}

PR_IMPLEMENT(PrefResult)
PREF_GetDefaultBinaryPref(const char *pref_name, void * return_value, int * length)
{
	PR_ASSERT( PR_FALSE );
	return PREF_ERROR;
}

PR_IMPLEMENT(PrefResult)
PREF_GetDefaultColorPref(const char *pref_name, uint8 *red, uint8 *green, uint8 *blue)
{
	char colstr[8];
	int iSize = 8;

	PrefResult result = PREF_GetDefaultCharPref(pref_name, colstr, &iSize);
	
	if (result == PREF_NOERROR) {
		int r, g, b;
		sscanf(colstr, "#%02X%02X%02X", &r, &g, &b);
		*red = r;
		*green = g;
		*blue = b;
	}

	return result;
}

PR_IMPLEMENT(PrefResult)
PREF_GetDefaultColorPrefDWord(const char *pref_name, uint32 * colorref)
{
    uint8 red, green, blue;
    PrefResult   result;

    PR_ASSERT(colorref);

    result = PREF_GetDefaultColorPref(pref_name, &red, &green, &blue);

    if (result == PREF_NOERROR) {
    	*colorref = MYRGB(red,green,blue);
    }

	return result;
}

PR_IMPLEMENT(PrefResult)
PREF_GetDefaultRectPref(const char *pref_name, int16 *left, int16 *top, int16 *right, int16 *bottom)
{
	char rectstr[256];
	int iLen = 256;
	PrefResult result = PREF_GetDefaultCharPref(pref_name, (char *)&rectstr, &iLen);
	
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
	if (to_delete && (PL_strncmp(he->key, to_delete, len) == 0 ||
		(len-1 == strlen(he->key) && PL_strncmp(he->key, to_delete, len-1) == 0)))
		return HT_ENUMERATE_REMOVE;
	else
		return HT_ENUMERATE_NEXT;
}

PR_IMPLEMENT(PrefResult)
PREF_DeleteBranch(const char *branch_name)
{
	char* branch_dot = PR_smprintf("%s.", branch_name);
	if (!branch_dot)
		return PREF_OUT_OF_MEMORY;		

	PR_HashTableEnumerateEntries(m_HashTable, pref_DeleteItem, (void*) branch_dot);
	
	PR_Free(branch_dot);
	return PREF_NOERROR;
}

/* LI_STUFF  add a function to clear the li pref 
   does anyone use this??
*/
PR_IMPLEMENT(PrefResult)
PREF_ClearLIPref(const char *pref_name)
{
	PrefResult success = PREF_ERROR;
    PrefNode*       pref;

    if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
	if (pref && PREF_HAS_LI_VALUE(pref)) {
		pref->flags &= ~PREF_LILOCAL;
		if (m_CallbacksEnabled)
    		pref_DoCallback(pref_name);
		success = PREF_OK;
	}
    return success;
}



PR_IMPLEMENT(PrefResult)
PREF_ClearUserPref(const char *pref_name)
{
	PrefResult success = PREF_ERROR;
    PrefNode*       pref;

    if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
	if (pref && PREF_HAS_USER_VALUE(pref)) {
		pref->flags &= ~PREF_USERSET;
		if (m_CallbacksEnabled)
    		pref_DoCallback(pref_name);
		success = PREF_OK;
	}
    return success;
}

/* Prototype Admin Kit support */
PR_IMPLEMENT(PrefResult)
PREF_GetConfigString(const char *obj_name, char * return_buffer, int size,
	int index, const char *field)
{
	PR_ASSERT( PR_FALSE );
	return PREF_ERROR;
}

/*
 * Administration Kit support 
 */
PR_IMPLEMENT(PrefResult)
PREF_CopyConfigString(const char *obj_name, char **return_buffer)
{
	PrefResult success = PREF_ERROR;
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, obj_name);
	
	if (pref && pref->flags & PREF_STRING) {
    	if (return_buffer)
    		*return_buffer = PL_strdup(pref->defaultPref.stringVal);
    	success = PREF_NOERROR;
    }
    return success;
}

PR_IMPLEMENT(PrefResult)
PREF_CopyIndexConfigString(const char *obj_name,
	int index, const char *field, char **return_buffer)
{
	PrefResult success = PREF_ERROR;
	PrefNode* pref;
	char* setup_buf = PR_smprintf("%s_%d.%s", obj_name, index, field);

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, setup_buf);
	
	if (pref && pref->flags & PREF_STRING) {
    	if (return_buffer)
    		*return_buffer = PL_strdup(pref->defaultPref.stringVal);
    	success = PREF_NOERROR;
    }
    PR_FREEIF(setup_buf);
    return success;
}

PR_IMPLEMENT(PrefResult)
PREF_GetConfigInt(const char *obj_name, int32 *return_int)
{
	PrefResult success = PREF_ERROR;
	
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, obj_name);
	
	if (pref && pref->flags & PREF_INT) {
		*return_int = pref->defaultPref.intVal;
		success = PREF_NOERROR;
	}

	return success;
}

PR_IMPLEMENT(PrefResult)
PREF_GetConfigBool(const char *obj_name, XP_Bool *return_bool)
{
	PrefResult success = PREF_ERROR;
	
	PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, obj_name);
	
	if (pref && pref->flags & PREF_BOOL) {
		*return_bool = pref->defaultPref.boolVal;
		success = PREF_NOERROR;
	}

	return success;
}

PrefResult pref_UnlockPref(const char *key)
{
	PrefNode* pref;
	PrefResult result = PREF_OK;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
	if (!pref) {
        return PREF_DOES_NOT_EXIST;
    }

	if (PREF_IS_LOCKED(pref)) {
		pref->flags &= ~PREF_LOCKED;
		if (m_CallbacksEnabled) {
			pref_DoCallback(key);
		}
    }

    return PREF_OK;
}

PrefResult pref_LockPref(const char *key)
{
	PrefNode* pref;
	PrefResult result = PREF_OK;

	if (!m_HashTable)
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
	if (!pref) {
        return PREF_DOES_NOT_EXIST;
    }

   
    return pref_HashPref(key, pref->defaultPref, (PrefType)pref->flags, PREF_LOCK);
}

PR_IMPLEMENT(PrefResult)
PREF_LockPref(const char *key)
{
	return pref_LockPref(key);
}

/*
 * Hash table functions
 */
static XP_Bool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type)
{
	PRBool changed = PR_TRUE;
	if (type & PREF_STRING) {
		if (oldValue.stringVal && newValue.stringVal)
			changed = (strcmp(oldValue.stringVal, newValue.stringVal) != 0);
    } else if (type & PREF_INT) {		
		changed = oldValue.intVal != newValue.intVal;
    } else if (type & PREF_BOOL) {
		changed = oldValue.boolVal != newValue.boolVal;
    }
	return changed;
}

static void pref_SetValue(PrefValue* oldValue, PrefValue newValue, PrefType type)
{
	switch (type & PREF_VALUETYPE_MASK) {
		case PREF_STRING:
			PR_ASSERT(newValue.stringVal);
			PR_FREEIF(oldValue->stringVal);
			oldValue->stringVal = newValue.stringVal ? PL_strdup(newValue.stringVal) : NULL;
			break;
		
		default:
			*oldValue = newValue;
	}
}

PrefResult pref_HashPref(const char *key, PrefValue value, PrefType type, PrefAction action)
{
	PrefNode* pref;
	PrefResult result = PREF_OK;

	if (!m_HashTable && !pref_useDefaultPrefFile())
		return PREF_NOT_INITIALIZED;

	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
	if (!pref) {
    	pref = (PrefNode*) calloc(sizeof(PrefNode), 1);
    	if (!pref)	
    		return PREF_OUT_OF_MEMORY;
    	pref->flags = type;
		if (pref->flags & PREF_BOOL)
			pref->defaultPref.boolVal = (PRBool) -2;
		/* ugly hack -- define it to a default that no pref will ever default to
		   this should really get fixed right by some out of band data */
		if (pref->flags & PREF_INT)
			pref->defaultPref.intVal = (int32) -5632;
    	PR_HashTableAdd(m_HashTable, PL_strdup(key), pref);
    }
    else if ((pref->flags & PREF_VALUETYPE_MASK) != (type & PREF_VALUETYPE_MASK)) {
		PR_ASSERT(0);			/* this shouldn't happen */
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
		    m_IsAnyPrefLocked = PR_TRUE;
		    break;
	}

    if (result == PREF_VALUECHANGED && m_CallbacksEnabled) {
    	PrefResult result2 = pref_DoCallback(key);
    	if (result2 < 0)
    		result = result2;
    }
    return result;
}

PR_IMPLEMENT(PrefType)
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
	return PREF_INVALID;
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
	PRBool prefExists = PR_TRUE;
	
    if (argc >= 1 && JSVAL_IS_STRING(argv[0]))
    {
    	const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    	pref = (PrefNode*) PR_HashTableLookup(m_HashTable, key);
    	
    	if (pref) {
    		PRBool use_default = (PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref));
    			
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

PR_IMPLEMENT(PRBool)
PREF_PrefIsLocked(const char *pref_name)
{
	PRBool result = PR_FALSE;
	if (m_IsAnyPrefLocked) {
		PrefNode* pref = (PrefNode*) PR_HashTableLookup(m_HashTable, pref_name);
		if (pref && PREF_IS_LOCKED(pref))
			result = PR_TRUE;
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
	if ( PL_strncmp(he->key, pcs->parent, strlen(pcs->parent)) == 0 ) {
		char buf[512];
		char* nextdelim;
		int parentlen = strlen(pcs->parent);
		char* substring;
		PRBool substringBordersSeparator = PR_FALSE;

		strncpy(buf, he->key, PR_MIN(512, strlen(he->key) + 1));
		nextdelim = buf + parentlen;
		if (parentlen < PL_strlen(buf)) {
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
			PR_ASSERT(substring[buflen] > 0);
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
			PL_strcat(pcs->childList, buf);
			PL_strcat(pcs->childList, ";");
		}
	}
	return 0;
}

PR_IMPLEMENT(PrefResult)
PREF_CreateChildList(const char* parent_node, char **child_list)
{
	PrefChildIter pcs;

#ifdef XP_WIN16
	pcs.bufsize = 20480;
#else
	pcs.bufsize = 2048;
#endif
	pcs.childList = (char*) malloc(sizeof(char) * pcs.bufsize);
	if (*parent_node > 0)
		pcs.parent = PR_smprintf("%s.", parent_node);
	else
		pcs.parent = PL_strdup("");
	if (!pcs.parent || !pcs.childList)
		return PREF_OUT_OF_MEMORY;
	pcs.childList[0] = '\0';

	PR_HashTableEnumerateEntries(m_HashTable, pref_addChild, &pcs);

	*child_list = pcs.childList;
	PR_Free(pcs.parent);
	
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

/*----------------------------------------------------------------------------------------
*	pref_copyTree
*
*	A recursive function that copies all the prefs in some subtree to
*	another subtree. Either srcPrefix or dstPrefix can be empty strings,
*	but not NULL pointers. Preferences in the destination are created if 
*	they do not already exist; otherwise the old values are replaced.
*
*	Example calls:
*
*		Copy all the prefs to another tree:			pref_copyTree("", "temp", "")
*
*		Copy all the prefs under mail. to newmail.:	pref_copyTree("mail", "newmail", "mail")
*
--------------------------------------------------------------------------------------*/ 
PrefResult pref_copyTree(const char *srcPrefix, const char *destPrefix, const char *curSrcBranch)
{
	PrefResult		result = PREF_NOERROR;

	char* 	children = NULL;
	
	if ( PREF_CreateChildList(curSrcBranch, &children) == PREF_NOERROR )
	{	
		int 	index = 0;
		int		srcPrefixLen = PL_strlen(srcPrefix);
		char* 	child = NULL;
		
		while ( (child = PREF_NextChild(children, &index)) != NULL)
		{
			PrefType prefType;
			char	*destPrefName = NULL;
			char	*childStart = (srcPrefixLen > 0) ? (child + srcPrefixLen + 1) : child;
			
			PR_ASSERT( PL_strncmp(child, curSrcBranch, srcPrefixLen) == 0 );
							
			if (*destPrefix > 0)
				destPrefName = PR_smprintf("%s.%s", destPrefix, childStart);
			else
				destPrefName = PR_smprintf("%s", childStart);
			
			if (!destPrefName)
			{
				result = PREF_OUT_OF_MEMORY;
				break;
			}
			
			if ( ! PREF_PrefIsLocked(destPrefName) )		/* returns true if the prefs exists, and is locked */
			{
				/*	PREF_GetPrefType masks out the other bits of the pref flag, so we only
					ever get the values in the switch.
				*/
				prefType = PREF_GetPrefType(child);
				
				switch (prefType)
				{
					case PREF_STRING:
						{
							char	*prefVal = NULL;
							
							result = PREF_CopyCharPref(child, &prefVal);
							if (result == PREF_NOERROR)
								result = PREF_SetCharPref(destPrefName, prefVal);
								
							PR_FREEIF(prefVal);
						}
						break;
					
					case PREF_INT:
							{
							int32 	prefValInt;
							
							result = PREF_GetIntPref(child, &prefValInt);
							if (result == PREF_NOERROR)
								result = PREF_SetIntPref(destPrefName, prefValInt);
						}
						break;
						
					case PREF_BOOL:
						{
							XP_Bool	prefBool;
							
							result = PREF_GetBoolPref(child, &prefBool);
							if (result == PREF_NOERROR)
								result = PREF_SetBoolPref(destPrefName, prefBool);
						}
						break;
					
					case PREF_ERROR:
						/*	this is probably just a branch. Since we can have both
							 a.b and a.b.c as valid prefs, this is OK.
						*/
						break;
						
					default:
						/* we should never get here */
						PR_ASSERT(PR_FALSE);
						break;
				}
				
			}	/* is not locked */
			
			PR_FREEIF(destPrefName);
			
			/* Recurse */
			if (result == PREF_NOERROR || result == PREF_VALUECHANGED)
				result = pref_copyTree(srcPrefix, destPrefix, child);
		}
		
		PR_Free(children);
	}
	
	return result;
}

PR_IMPLEMENT(PrefResult)
PREF_CopyPrefsTree(const char *srcRoot, const char *destRoot)
{
	PR_ASSERT(srcRoot != NULL);
	PR_ASSERT(destRoot != NULL);
	
	return pref_copyTree(srcRoot, destRoot, srcRoot);
}


/* Adds a node to the beginning of the callback list. */
PR_IMPLEMENT(void)
PREF_RegisterCallback(const char *pref_node,
					   PrefChangedFunc callback,
					   void * instance_data)
{
	struct CallbackNode* node = (struct CallbackNode*) malloc(sizeof(struct CallbackNode));
	if (node) {
		node->domain = PL_strdup(pref_node);
		node->func = callback;
		node->data = instance_data;
		node->next = m_Callbacks;
		m_Callbacks = node;
	}
	return;
}

/* Deletes a node from the callback list. */
PR_IMPLEMENT(PrefResult)
PREF_UnregisterCallback(const char *pref_node,
						 PrefChangedFunc callback,
						 void * instance_data)
{
	PrefResult result = PREF_ERROR;
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
			PR_Free(node->domain);
			PR_Free(node);
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

PrefResult pref_DoCallback(const char* changed_pref)
{
	PrefResult result = PREF_OK;
	struct CallbackNode* node;
	for (node = m_Callbacks; node != NULL; node = node->next)
	{
		if ( PL_strncmp(changed_pref, node->domain, strlen(node->domain)) == 0 ) {
			int result2 = (*node->func) (changed_pref, node->data);
			if (result2 != 0)
				result = (PrefResult)result2;
		}
	}
	return result;
}

/* !! Front ends need to implement */
#ifndef XP_MAC /* see macpref.cp */
PR_IMPLEMENT(PRBool)
PREF_IsAutoAdminEnabled()
{
	return PR_TRUE;
}
#endif /* XP_MAC */

/* Called from JavaScript */
typedef char* (*ldap_func)(char*, char*, char*, char*, char**); 

JSBool PR_CALLBACK pref_NativeGetLDAPAttr
	(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
#ifdef MOZ_ADMIN_LIB
	ldap_func get_ldap_attributes = NULL;
#if (defined (XP_MAC) && defined(powerc)) || defined (XP_PC) || defined(XP_UNIX)
	if (m_AutoAdminLib == NULL) {
		m_AutoAdminLib = pref_LoadAutoAdminLib();
	}
		
	if (m_AutoAdminLib) {
		get_ldap_attributes = (ldap_func)
			PR_FindSymbol(
			 m_AutoAdminLib,
#ifndef XP_WIN16
			"pref_get_ldap_attributes"
#else
			MAKEINTRESOURCE(1)
#endif
			);
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
			PR_Free(value);
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

/* Dump debugging info in response to about:config.
 */
PR_IMPLEMENT(int)
pref_printDebugInfo(PRHashEntry *he, int i, void *arg)
{
	char *buf1=NULL, *buf2=NULL;
	PrefValue val;
	PrefChildIter* pcs = (PrefChildIter*) arg;
	PrefNode *pref = (PrefNode *) he->value;
	
	PR_ASSERT(pref);
	if (!pref)
		return PREF_NOERROR;

	if (PREF_HAS_USER_VALUE(pref) && 
		pref_ValueChanged(pref->defaultPref, 
						  pref->userPref, 
						  (PrefType) PREF_TYPE(pref)) && 
		!PREF_IS_LOCKED(pref))
	{
		buf1 = PR_smprintf("<font color=\"blue\">%s = ", (char*) he->key);
		val = pref->userPref;
	}
	else
	{
		buf1 = PR_smprintf("<font color=\"%s\">%s = ",
			PREF_IS_LOCKED(pref) ? "red" : (PREF_IS_CONFIG(pref) ? "black" : "green"),
			(char*) he->key);
		val = pref->defaultPref;
	}
	
	if (pref->flags & PREF_STRING)
		buf2 = PR_smprintf("%s %s</font><br>", buf1, val.stringVal);
	else if (pref->flags & PREF_INT)
		buf2 = PR_smprintf("%s %d</font><br>", buf1, val.intVal);
	else if (pref->flags & PREF_BOOL)
		buf2 = PR_smprintf("%s %s</font><br>", buf1, val.boolVal ? "true" : "false");
	
	if ((PL_strlen(buf2) + PL_strlen(pcs->childList) + 1) > pcs->bufsize)
	{
		pcs->bufsize *= 3;
		pcs->childList = (char*) realloc(pcs->childList, sizeof(char) * pcs->bufsize);
		if (!pcs->childList)
			return HT_ENUMERATE_STOP;
	}
	PL_strcat(pcs->childList, buf2);
	PR_Free(buf1);
	PR_Free(buf2);
	return 0;
}

PR_IMPLEMENT(char *)
PREF_AboutConfig()
{
	PrefChildIter pcs;
	pcs.bufsize = 8192;
	pcs.childList = (char*) malloc(sizeof(char) * pcs.bufsize);
	pcs.childList[0] = '\0';
	PL_strcat(pcs.childList, "<HTML>");	

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
	    PR_Free(message);
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
		PR_Free(last);
	}
}

#if defined(XP_MAC)

#include <Dialogs.h>
#include <Memory.h>

void pref_Alert(char* msg)
{
	Str255 pmsg;
	SInt16 itemHit;
	pmsg[0] = strlen(msg);
	BlockMoveData(msg, pmsg + 1, pmsg[0]);
	StandardAlert(kAlertPlainAlert, "\pNetscape -- JS Preference Warning", pmsg, NULL, &itemHit);
}

#else

/* Platform specific alert messages */
void pref_Alert(char* msg)
{
#if defined(XP_UNIX) || defined(XP_OS2)
#if defined(XP_UNIX)
    if ( getenv("NO_PREF_SPAM") == NULL )
#endif
      /* FE_Alert will eventually become something else */
#if 0
	FE_Alert(NULL, msg);
#else
    fputs(stderr, msg);
#endif
#endif
#if defined (XP_PC)
		MessageBox (NULL, msg, "Netscape -- JS Preference Warning", MB_OK);
#endif
}

#endif

#ifdef XP_WIN16
#define ADMNLIBNAME "adm1640.dll"
#elif defined XP_PC || defined XP_OS2
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

#if defined(XP_UNIX) && !defined(B1M)
	{
		char aalib[MAXPATHLEN];

		if (getenv("NS_ADMIN_LIB"))
		{
			lib = PR_LoadLibrary(getenv("NS_ADMIN_LIB"));
		}
		else
		{
			if (getenv("MOZILLA_FIVE_HOME"))
			{
				PL_strcpy(aalib, getenv("MOZILLA_FIVE_HOME"));
				lib = PR_LoadLibrary(PL_strcat(aalib, ADMNLIBNAME));
			}
			if (lib == NULL)
			{
				fe_GetProgramDirectory(aalib, sizeof(aalib)-1);
				lib = PR_LoadLibrary(PL_strcat(aalib, ADMNLIBNAME));
			}
			if (lib == NULL)
			{
				(void) PL_strcpy(aalib, "/usr/local/netscape/");
				lib = PR_LoadLibrary(PL_strcat(aalib, ADMNLIBNAME));
			}
		}
	}
	/* Make sure it's really libAutoAdmin.so */
    
	if ( lib && PR_FindSymbol(lib, "_POLARIS_SplashPro") == NULL ) return NULL;
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
	PRBool bIsBool, bIsInt, bIsString;

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

/*
 * pref_CountListMembers
 */
static int
pref_CountListMembers(char* list)
{
	int members = 0;
	char* p = list = PL_strdup(list);

	for ( p = strtok(p, ","); p != NULL; p = strtok(NULL, ",") ) {
		members++;
	}

	PR_FREEIF(list);

	return members;
}


/*
 * PREF_GetListPref
 * Splits a comma separated strings into an array of strings.
 * The array of strings is actually just an array of pointers into a copy
 * of the value returned by PREF_CopyCharPref().  So, we don't have to
 * allocate each string separately.
 */
PR_IMPLEMENT(PrefResult)
PREF_GetListPref(const char* pref, char*** list)
{
	char* value;
	char** p;
	int num_members;

	*list = NULL;

	if ( PREF_CopyCharPref(pref, &value) != PREF_OK || value == NULL ) {
		return PREF_ERROR;
	}

	num_members = pref_CountListMembers(value);

	p = *list = (char**) PR_MALLOC((num_members+1) * sizeof(char**));
	if ( *list == NULL ) return PREF_ERROR;

	for ( *p = strtok(value, ","); 
          *p != NULL; 
          *(++p) = strtok(NULL, ",") ) /* Empty body */ ;

	/* Copy each entry so that users can free them. */
	for ( p = *list; *p != NULL; p++ ) {
		*p = PL_strdup(*p);
	}

	PR_Free(value);

	return PREF_OK;
}


/*
 * PREF_SetListPref
 * TODO: Call Javascript callback to make sure user is allowed to make this
 * change.
 */
PR_IMPLEMENT(PrefResult)
PREF_SetListPref(const char* pref, char** list)
{
	PrefResult status;
	int len;
	char** p;
	char* value = NULL;

	if ( pref == NULL || list == NULL ) return PREF_ERROR;

	for ( len = 0, p = list; p != NULL && *p != NULL; p++ ) {
		len+= (PL_strlen(*p)+1); /* The '+1' is for a comma or '\0' */
	}

	if ( len <= 0 || (value = PR_MALLOC(len)) == NULL ) {
		return PREF_ERROR;
	}

	(void) PL_strcpy(value, *list);
	for ( p = list+1; p != NULL && *p != NULL; p++ ) {
		(void) PL_strcat(value, ",");
		(void) PL_strcat(value, *p);
	}

	status = PREF_SetCharPref(pref, value);

	PR_FREEIF(value);

	return status;	
}


PR_IMPLEMENT(PrefResult)
PREF_AppendListPref(const char* pref, const char* value)
{
	char *pListPref = NULL, *pNewList = NULL;
    int nPrefLen = 0;

	PREF_CopyCharPref(pref, &pListPref);

	if (pListPref)
	{
        nPrefLen = PL_strlen(pListPref);
    }

    if (nPrefLen == 0) {
		PREF_SetCharPref(pref, value);
    } else {
        pNewList = (char *) PR_MALLOC((nPrefLen + PL_strlen(value) + 2));
        if (pNewList) {
            PL_strcpy(pNewList, pListPref);
			PL_strcat(pNewList, ",");
			PL_strcat(pNewList, value);
    		PREF_SetCharPref(pref, pNewList);
            PR_Free(pNewList);
        }

    }

    PR_FREEIF(pListPref);

    return PREF_NOERROR;
}

/*
 * PREF_FreeListPref
 * Free each element in the list, then free the list, then NULL the
 * list out.
 */
PR_IMPLEMENT(PrefResult)
PREF_FreeListPref(char*** list)
{
	char** p;

	if ( list == NULL ) return PREF_ERROR;

	for ( p = *list; *p != NULL; p++ ) {
		PR_Free(*p);
	}

	PR_FREEIF(*list);

	*list = NULL;

	return PREF_OK;
}
