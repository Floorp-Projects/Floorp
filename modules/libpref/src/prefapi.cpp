/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "prefapi.h"
#include "jsapi.h"
#include "xp_core.h" /* Needed for XP_ defines */

#if defined(XP_MAC)
  #include <stat.h>
#else
  #ifdef XP_OS2_EMX
    #include <sys/types.h>
  #endif
#endif
#ifdef _WIN32
  #include "windows.h"
#endif /* _WIN32 */

#ifdef MOZ_ADMIN_LIB
#include "prefldap.h"
#endif

#ifdef MOZ_SECURITY
#include "sechash.h"
#endif
#include "plstr.h"
#include "plhash.h"
#include "plbase64.h"
#include "prlog.h"
#include "prmem.h"
#include "prprf.h"
#include "nsQuickSort.h"

#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>
#endif

#define BOGUS_DEFAULT_INT_PREF_VALUE (-5632)
#define BOGUS_DEFAULT_BOOL_PREF_VALUE (-2)

typedef union
{
    char*       stringVal;
    PRInt32     intVal;
    PRBool      boolVal;
} PrefValue;
 
typedef struct 
{
    PrefValue   defaultPref;
    PrefValue   userPref;
    PRUint8     flags;
} PrefNode;

/*-----------------------
** Hash table allocation
**----------------------*/

PR_STATIC_CALLBACK(void*) pref_AllocTable(void *pool, size_t size)
{
    return malloc(size);
}

PR_STATIC_CALLBACK(void) pref_FreeTable(void *pool, void *item)
{
    free(item);     /* free items? */
}

PR_STATIC_CALLBACK(PLHashEntry*) pref_AllocEntry(void *pool, const void *key)
{
    return malloc(sizeof(PLHashEntry));
}

PR_STATIC_CALLBACK(void) pref_FreeEntry(void *pool, PLHashEntry *he, PRUint32 flag)
{
    PrefNode *pref = (PrefNode *) he->value;
    if (pref)
    {
        if (pref->flags & PREF_STRING)
        {
            PR_FREEIF(pref->defaultPref.stringVal);
            PR_FREEIF(pref->userPref.stringVal);
        }
        PR_Free(he->value);
    }

    if (flag == HT_FREE_ENTRY)
    {
        PR_FREEIF(*(void**)&he->key);
        PR_Free(he);
    }
}

PR_STATIC_CALLBACK(JSBool) pref_NativeDefaultPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
PR_STATIC_CALLBACK(JSBool) pref_NativeUserPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
PR_STATIC_CALLBACK(JSBool) pref_NativeLockPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
PR_STATIC_CALLBACK(JSBool) pref_NativeUnlockPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
PR_STATIC_CALLBACK(JSBool) pref_NativeSetConfig(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
PR_STATIC_CALLBACK(JSBool) pref_NativeGetPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
PR_STATIC_CALLBACK(JSBool) pref_NativeGetLDAPAttr(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);

/*----------------------------------------------------------------------------------------*/
#include "prefapi_private_data.h"

static JSBool
global_enumerate(JSContext *cx, JSObject *obj)
{
    return JS_EnumerateStandardClasses(cx, obj);
}

static JSBool
global_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSBool resolved;

    return JS_ResolveStandardClass(cx, obj, id, &resolved);
}

JSRuntime *       gMochaTaskState = NULL;
JSContext *       gMochaContext = NULL;
JSObject *        gMochaPrefObject = NULL;
JSObject *        gGlobalConfigObject = NULL;
JSClass           global_class = {
                    "global", 0,
                    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
                    global_enumerate, global_resolve, JS_ConvertStub, JS_FinalizeStub,
                    JSCLASS_NO_OPTIONAL_MEMBERS
                    };
JSClass             autoconf_class = {
                    "PrefConfig", 0,
                    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
                    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
                    JSCLASS_NO_OPTIONAL_MEMBERS
                    };
JSPropertySpec      autoconf_props[] = {
                    {0,0,0,0,0}
                    };
JSFunctionSpec      autoconf_methods[] = {
                    { "pref",               pref_NativeDefaultPref, 2,0,0 },
                    { "defaultPref",        pref_NativeDefaultPref, 2,0,0 },
                    { "user_pref",          pref_NativeUserPref,    2,0,0 },
                    { "lockPref",           pref_NativeLockPref,    2,0,0 },
                    { "unlockPref",         pref_NativeUnlockPref,  1,0,0 },
                    { "config",             pref_NativeSetConfig,   2,0,0 },
                    { "getPref",            pref_NativeGetPref,     1,0,0 },
                    { "getLDAPAttributes",  pref_NativeGetLDAPAttr, 4,0,0 },
                    { "localPref",          pref_NativeDefaultPref, 1,0,0 },
                    { "localUserPref",      pref_NativeUserPref,    2,0,0 },
                    { "localDefPref",       pref_NativeDefaultPref, 2,0,0 },
                    { NULL,                 NULL,                   0,0,0 }
                    };

struct CallbackNode*    gCallbacks = NULL;
PRBool              gErrorOpeningUserPrefs = PR_FALSE;
PRBool              gCallbacksEnabled = PR_FALSE;
PRBool              gIsAnyPrefLocked = PR_FALSE;
PRBool              gLockInfoRead = PR_FALSE;
PLHashTable*        gHashTable = NULL;
char *              gSavedLine = NULL; 
char *              gLockFileName = NULL;
char *              gLockVendor = NULL;
      
PLHashAllocOps      pref_HashAllocOps = {
                        pref_AllocTable, pref_FreeTable,
                        pref_AllocEntry, pref_FreeEntry
                    };

/*----------------------------------------------------------------------------------------*/

#define PREF_IS_LOCKED(pref)            ((pref)->flags & PREF_LOCKED)
#define PREF_IS_CONFIG(pref)            ((pref)->flags & PREF_CONFIG)
#define PREF_HAS_USER_VALUE(pref)       ((pref)->flags & PREF_USERSET)
#define PREF_TYPE(pref)                 (PrefType)((pref)->flags & PREF_VALUETYPE_MASK)

static JSBool pref_HashJSPref(unsigned int argc, jsval *argv, PrefAction action);
static PRBool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type);

#include "prlink.h"
extern PRLibrary *pref_LoadAutoAdminLib(void);
PRLibrary *gAutoAdminLib = NULL;

/* -- Privates */
struct CallbackNode {
    char*                   domain;
    PrefChangedFunc         func;
    void*                   data;
    struct CallbackNode*    next;
};

/* -- Prototypes */
PrefResult pref_DoCallback(const char* changed_pref);
PRBool pref_VerifyLockFile(char* buf, long buflen);


JSBool PR_CALLBACK pref_BranchCallback(JSContext *cx, JSScript *script);
void pref_ErrorReporter(JSContext *cx, const char *message,JSErrorReport *report);
void pref_Alert(char* msg);
PrefResult pref_HashPref(const char *key, PrefValue value, PrefType type, PrefAction action);

/* Computes the MD5 hash of the given buffer (not including the first line)
   and verifies the first line of the buffer expresses the correct hash in the form:
   // xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
   where each 'xx' is a hex value. */
PRBool pref_VerifyLockFile(char* buf, long buflen)
{
#ifdef MOZ_SECURITY
    PRBool success = PR_FALSE;
    const int obscure_value = 7;
    const long hash_length = 51;        /* len = 48 chars of MD5 + // + EOL */
    unsigned char digest[16];
    char szHash[64];

    /* Unobscure file by subtracting some value from every char. */
    long i;
    for (i = 0; i < buflen; i++)
        buf[i] -= obscure_value;

    if (buflen >= hash_length)
    {
        const unsigned char magic_key[] = "VonGloda5652TX75235ISBN";
        unsigned char *pStart = (unsigned char*) buf + hash_length;
        unsigned int len;
        
        MD5Context * md5_cxt = MD5_NewContext();
        MD5_Begin(md5_cxt);
        
        /* start with the magic key */
        MD5_Update(md5_cxt, magic_key, sizeof(magic_key));

        MD5_Update(md5_cxt, pStart, (unsigned int)(buflen - hash_length));
        
        MD5_End(md5_cxt, digest, &len, 16);
        
        MD5_DestroyContext(md5_cxt, PR_TRUE);
        
        PR_snprintf(szHash, 64, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            (int)digest[0],(int)digest[1],(int)digest[2],(int)digest[3],
            (int)digest[4],(int)digest[5],(int)digest[6],(int)digest[7],
            (int)digest[8],(int)digest[9],(int)digest[10],(int)digest[11],
            (int)digest[12],(int)digest[13],(int)digest[14],(int)digest[15]);

        success = ( PL_strncmp((const char*) buf + 3, szHash, (PRUint32)(hash_length - 4)) == 0 );
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
PRBool PREF_Init(const char *filename)
{
    PRBool ok = PR_TRUE, request = PR_FALSE;
    extern JSRuntime* PREF_GetJSRuntime(void);

    /* --ML hash test */
    if (!gHashTable)
    gHashTable = PR_NewHashTable(2048, PR_HashString, PR_CompareStrings,
            PR_CompareValues, &pref_HashAllocOps, NULL);
    if (!gHashTable)
        return PR_FALSE;

    if (!gMochaTaskState)
    {
        gMochaTaskState = PREF_GetJSRuntime();
        if (!gMochaTaskState)
            return PR_FALSE;
    }

    if (!gMochaContext)
    {
        ok = PR_FALSE;
        gMochaContext = JS_NewContext(gMochaTaskState, 8192);
        if (!gMochaContext)
            goto out;

        JS_BeginRequest(gMochaContext);
        request = PR_TRUE;

        gGlobalConfigObject = JS_NewObject(gMochaContext, &global_class, NULL,
                                           NULL);
        if (!gGlobalConfigObject)
            goto out;

        /* MLM - need a global object for set version call now. */
        JS_SetGlobalObject(gMochaContext, gGlobalConfigObject);

        JS_SetVersion(gMochaContext, JSVERSION_1_5);

        JS_SetBranchCallback(gMochaContext, pref_BranchCallback);
        JS_SetErrorReporter(gMochaContext, NULL);

        gMochaPrefObject = JS_DefineObject(gMochaContext, 
                                            gGlobalConfigObject, 
                                            "PrefConfig",
                                            &autoconf_class, 
                                            NULL, 
                                            JSPROP_ENUMERATE|JSPROP_READONLY);
        
        if (gMochaPrefObject)
        {
            if (!JS_DefineProperties(gMochaContext,
                                     gMochaPrefObject,
                                     autoconf_props))
            {
                goto out;
            }
            if (!JS_DefineFunctions(gMochaContext,
                                    gMochaPrefObject,
                                    autoconf_methods))
            {
                goto out;
            }
        }

        ok = pref_InitInitialObjects();
    }
 out:
    if (request)
        JS_EndRequest(gMochaContext);

    if (!ok)
        gErrorOpeningUserPrefs = PR_TRUE;

    return ok;
} /*PREF_Init*/

PrefResult
PREF_GetConfigContext(JSContext **js_context)
{
    if (!js_context) return PREF_ERROR;

    *js_context = gMochaContext;
    return PREF_NOERROR;
}

PrefResult
PREF_GetGlobalConfigObject(JSObject **js_object)
{
    if (!js_object) return PREF_ERROR;

    *js_object = NULL;
    if (gGlobalConfigObject)
        *js_object = gGlobalConfigObject;

    return PREF_NOERROR;
}

PrefResult
PREF_GetPrefConfigObject(JSObject **js_object)
{
    if (!js_object) return PREF_ERROR;

    *js_object = NULL;
    if (gMochaPrefObject)
        *js_object = gMochaPrefObject;

    return PREF_NOERROR;
}

/* Frees the callback list. */
void PREF_Cleanup()
{
    struct CallbackNode* node = gCallbacks;
    struct CallbackNode* next_node;
    
    while (node)
    {
        next_node = node->next;
        PR_Free(node->domain);
        PR_Free(node);
        node = next_node;
    }
    gCallbacks = NULL;

    PREF_CleanupPrefs();
}

/* Frees up all the objects except the callback list. */
void PREF_CleanupPrefs()
{
    gMochaTaskState = NULL; /* We -don't- destroy this. */

    if (gMochaContext) {
        extern JSRuntime* PREF_GetJSRuntime(void);
        JSRuntime *rt;
        gMochaPrefObject = NULL;

        if (gGlobalConfigObject) {
            JS_SetGlobalObject(gMochaContext, NULL);
            gGlobalConfigObject = NULL;
        }

        rt = PREF_GetJSRuntime();
        if (rt == JS_GetRuntime(gMochaContext)) {
            JS_DestroyContext(gMochaContext);
            gMochaContext = NULL;
        } else {
#ifdef DEBUG
            fputs("Runtime mismatch, so leaking context!\n", stderr);
#endif
        }
    }

    if (gHashTable)
        PR_HashTableDestroy(gHashTable);
    gHashTable = NULL;

    if (gSavedLine)
        free(gSavedLine);
    gSavedLine = NULL;
}

PrefResult
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
    gCallbacksEnabled = status;
}

/* This is more recent than the below 3 routines which should be obsoleted */
JSBool
PREF_EvaluateConfigScript(const char * js_buffer, size_t length,
    const char* filename, PRBool bGlobalContext, PRBool bCallbacks,
    PRBool skipFirstLine)
{
    JSBool ok;
    jsval result;
    JSObject* scope;
    JSErrorReporter errReporter;
    
    if (bGlobalContext)
        scope = gGlobalConfigObject;
    else
        scope = gMochaPrefObject;
        
    if (!gMochaContext || !scope)
        return JS_FALSE;

    errReporter = JS_SetErrorReporter(gMochaContext, pref_ErrorReporter);
    gCallbacksEnabled = bCallbacks;

    if (skipFirstLine)
    {
        /* In order to protect the privacy of the JavaScript preferences file 
         * from loading by the browser, we make the first line unparseable
         * by JavaScript. We must skip that line here before executing 
         * the JavaScript code.
         */
        unsigned int i=0;
        while (i < length)
        {
            char c = js_buffer[i++];
            if (c == '\r')
            {
                if (js_buffer[i] == '\n')
                    i++;
                break;
            }
            if (c == '\n')
                break;
        }

        /* Free up gSavedLine to avoid MLK. */
        if (gSavedLine) 
            free(gSavedLine);
        gSavedLine = malloc(i+1);
        if (!gSavedLine)
            return JS_FALSE;
        memcpy(gSavedLine, js_buffer, i);
        gSavedLine[i] = '\0';
        length -= i;
        js_buffer += i;
    }

    JS_BeginRequest(gMochaContext);
    ok = JS_EvaluateScript(gMochaContext, scope,
            js_buffer, length, filename, 0, &result);
    JS_EndRequest(gMochaContext);
    
    gCallbacksEnabled = PR_TRUE;        /* ?? want to enable after reading user/lock file */
    JS_SetErrorReporter(gMochaContext, errReporter);
    
    return ok;
}

static char * str_escape(const char * original)
{
    const char *p;
    char * ret_str, *q;

    if (original == NULL)
        return NULL;
    
    ret_str = malloc(2 * PL_strlen(original) + 1);
    /* Paranoid worst case all slashes will free quickly */
    p = original;
    q = ret_str;
    while (*p)
    {
        switch (*p)
        {
            case '\\':
            case '\"':
            case '\n':
                *q++ = '\\';
                break;
            default:
                break;
        }
        *q++ = *p++;
    }
    *q = 0;
    return ret_str;
}

/*
** External calls
*/
PrefResult
PREF_SetCharPref(const char *pref_name, const char *value)
{
    PrefValue pref;
    pref.stringVal = (char*) value;
    
    return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

PrefResult
PREF_SetIntPref(const char *pref_name, PRInt32 value)
{
    PrefValue pref;
    pref.intVal = value;
    
    return pref_HashPref(pref_name, pref, PREF_INT, PREF_SETUSER);
}

PrefResult
PREF_SetBoolPref(const char *pref_name, PRBool value)
{
    PrefValue pref;
    pref.boolVal = value;
    
    return pref_HashPref(pref_name, pref, PREF_BOOL, PREF_SETUSER);
}

PrefResult
PREF_SetBinaryPref(const char *pref_name, void * value, long size)
{
    char* buf = PL_Base64Encode(value, (PRUint32)size, NULL);

    if (buf) {
        PrefValue pref;
        pref.stringVal = buf;
        return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
    }
    else
        return PREF_ERROR;
}

PrefResult
PREF_SetColorPref(const char *pref_name, PRUint8 red, PRUint8 green, PRUint8 blue)
{
    char colstr[63];
    PrefValue pref;
    PR_snprintf( colstr, 63, "#%02X%02X%02X", red, green, blue);

    pref.stringVal = colstr;
    return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETUSER);
}

#define MYGetboolVal(rgb)   ((PRUint8) ((rgb) >> 16))
#define MYGetGValue(rgb)   ((PRUint8) (((PRUint16) (rgb)) >> 8)) 
#define MYGetRValue(rgb)   ((PRUint8) (rgb)) 

PrefResult
PREF_SetColorPrefDWord(const char *pref_name, PRUint32 colorref)
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

PrefResult
PREF_SetRectPref(const char *pref_name, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom)
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
PrefResult
PREF_SetDefaultCharPref(const char *pref_name,const char *value)
{
    PrefValue pref;
    pref.stringVal = (char*) value;
    
    return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETDEFAULT);
}


PrefResult
PREF_SetDefaultIntPref(const char *pref_name,PRInt32 value)
{
    PrefValue pref;
    pref.intVal = value;
    
    return pref_HashPref(pref_name, pref, PREF_INT, PREF_SETDEFAULT);
}

PrefResult
PREF_SetDefaultBoolPref(const char *pref_name,PRBool value)
{
    PrefValue pref;
    pref.boolVal = value;
    
    return pref_HashPref(pref_name, pref, PREF_BOOL, PREF_SETDEFAULT);
}

PrefResult
PREF_SetDefaultBinaryPref(const char *pref_name,void * value,long size)
{
    char* buf = PL_Base64Encode(value, (PRUint32)size, NULL);
    if (buf) {
        PrefValue pref;
        pref.stringVal = buf;
        return pref_HashPref(pref_name, pref, PREF_STRING, PREF_SETDEFAULT);
    }
    else
        return PREF_ERROR;
}

PrefResult
PREF_SetDefaultColorPref(const char *pref_name, PRUint8 red, PRUint8 green, PRUint8 blue)
{
    char colstr[63];
    PR_snprintf( colstr, 63, "#%02X%02X%02X", red, green, blue);

    return PREF_SetDefaultCharPref(pref_name, colstr);
}

PrefResult
PREF_SetDefaultRectPref(const char *pref_name, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom)
{
    char rectstr[63];
    PR_snprintf( rectstr, 63, "%d,%d,%d,%d", left, top, right, bottom);

    return PREF_SetDefaultCharPref(pref_name, rectstr);
}


PrefResult
pref_savePref(PLHashEntry *he, int i, void *arg)
{
    char **prefArray = (char**) arg;
    PrefNode *pref = (PrefNode *) he->value;

    PR_ASSERT(pref);
    if (!pref)
        return PREF_NOERROR;

    if (PREF_HAS_USER_VALUE(pref) && 
        pref_ValueChanged(pref->defaultPref, 
                          pref->userPref, 
                          (PrefType) PREF_TYPE(pref)))
    {
        char *prefEntry = nsnull;

        if (pref->flags & PREF_STRING)
        {
            char *tmp_str = str_escape(pref->userPref.stringVal);

            if (tmp_str)
            {
                prefEntry = PR_smprintf("user_pref(\"%s\", \"%s\");",
                                (char*)he->key, tmp_str);
                PR_Free(tmp_str);
            }
        }
        else if (pref->flags & PREF_INT)
        {
            prefEntry = PR_smprintf("user_pref(\"%s\", %ld);", (char*)he->key,
                            (long) pref->userPref.intVal);
        }
        else if (pref->flags & PREF_BOOL)
        {
            prefEntry = PR_smprintf("user_pref(\"%s\", %s);", (char*)he->key,
                            (pref->userPref.boolVal) ? "true" : "false");
        }

        prefArray[i] = prefEntry;
    }
    else if (pref && PREF_IS_LOCKED(pref))
    {
        char *prefEntry = nsnull;

        if (pref->flags & PREF_STRING)
        {
            char *tmp_str = str_escape(pref->defaultPref.stringVal);
            if (tmp_str) {
                prefEntry = PR_smprintf("user_pref(\"%s\", \"%s\");",
                                (char*)he->key, tmp_str);
                PR_Free(tmp_str);
            }
        }
        else if (pref->flags & PREF_INT)
        {
            prefEntry = PR_smprintf("user_pref(\"%s\", %ld);", (char*)he->key,
                            (long) pref->defaultPref.intVal);
        }
        else if (pref->flags & PREF_BOOL)
        {
            prefEntry = PR_smprintf("user_pref(\"%s\", %s);", (char*)he->key,
                (pref->defaultPref.boolVal) ? "true" : "false");
        }
        prefArray[i] = prefEntry;
    }
    return PREF_NOERROR;
}

int
#ifdef XP_OS2_VACPP
_Optlink
#endif
pref_CompareStrings(const void *v1, const void *v2, void *unused)
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


PRBool
pref_useDefaultPrefFile(void)
{
#ifdef PREF_BACKOUT

#if defined(XP_UNIX) || defined(XP_BEOS)
  return PREF_Init("preferences.js");
#elif defined(XP_MAC)
  return PREF_Init("Netscape Preferences");
#else /* XP_WIN */
  return PREF_Init("prefs.js");
#endif

#else /* !PREF_BACKOUT */

  return PR_FALSE;

#endif /* PREF_BACKOUT */
}

PRBool PREF_HasUserPref(const char *pref_name)
{
    PrefNode *pref;
    
    if (!gHashTable && !pref_useDefaultPrefFile())
        return PR_FALSE;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);

    if (!pref) return PR_FALSE;
    
    /* convert PREF_HAS_USER_VALUE to bool */
    return (PREF_HAS_USER_VALUE(pref) != 0);

}
PrefResult PREF_GetCharPref(const char *pref_name, char * return_buffer, int * length, PRBool get_default)
{
    PrefResult result = PREF_ERROR;
    char* stringVal;
    
    PrefNode* pref;

    if (!gHashTable && !pref_useDefaultPrefFile())
        return PREF_NOT_INITIALIZED;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);

    if (pref)
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
            stringVal = pref->defaultPref.stringVal;
        else
            stringVal = pref->userPref.stringVal;
        
        if (stringVal)
        {
            if (*length <= 0)
                *length = PL_strlen(stringVal) + 1;
            else
            {
                PL_strncpy(return_buffer, stringVal, PR_MIN((size_t)*length - 1, PL_strlen(stringVal) + 1));
                return_buffer[*length - 1] = '\0';
            }
            result = PREF_OK;
        }
    }
    return result;
}

PrefResult
PREF_CopyCharPref(const char *pref_name, char ** return_buffer, PRBool get_default)
{
    PrefResult result = PREF_ERROR;
    char* stringVal;    
    PrefNode* pref;

    if (!gHashTable && !pref_useDefaultPrefFile())
        return PREF_NOT_INITIALIZED;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);

    if (pref && (pref->flags & PREF_STRING))
    {
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

PrefResult PREF_GetIntPref(const char *pref_name,PRInt32 * return_int, PRBool get_default)
{
    PrefResult result = PREF_ERROR; 
    PrefNode* pref;

    if (!gHashTable && !pref_useDefaultPrefFile())
        return PREF_NOT_INITIALIZED;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);
    if (pref && (pref->flags & PREF_INT))
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
        {
            PRInt32 tempInt = pref->defaultPref.intVal;
            /* check to see if we even had a default */
            if (tempInt == ((PRInt32) BOGUS_DEFAULT_INT_PREF_VALUE))
                return PREF_DEFAULT_VALUE_NOT_INITIALIZED;
            *return_int = tempInt;
        }
        else
            *return_int = pref->userPref.intVal;
        result = PREF_OK;
    }
    return result;
}

PrefResult PREF_GetBoolPref(const char *pref_name, PRBool * return_value, PRBool get_default)
{
    PrefResult result = PREF_ERROR;
    PrefNode* pref;

    if (!gHashTable && !pref_useDefaultPrefFile())
        return PREF_NOT_INITIALIZED;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);   
    if (pref && (pref->flags & PREF_BOOL))
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
        {
            PRBool tempBool = pref->defaultPref.boolVal;
            /* check to see if we even had a default */
            if (tempBool == ((PRBool) BOGUS_DEFAULT_BOOL_PREF_VALUE))
                return PREF_DEFAULT_VALUE_NOT_INITIALIZED;
            *return_value = tempBool;
        }
        else
            *return_value = pref->userPref.boolVal;
        result = PREF_OK;
    }
    return result;
}



PrefResult
PREF_GetColorPref(const char *pref_name, PRUint8 *red, PRUint8 *green, PRUint8 *blue, PRBool isDefault)
{
    char colstr[8];
    int iSize = 8;

    PrefResult result = PREF_GetCharPref(pref_name, colstr, &iSize, isDefault);
    
    if (result == PREF_NOERROR)
    {
        unsigned int r, g, b;
        sscanf(colstr, "#%02x%02x%02x", &r, &g, &b);
        *red = r;
        *green = g;
        *blue = b;
    }   
    return result;
}

#define MYRGB(r, g ,b)  ((PRUint32) (((PRUint8) (r) | ((PRUint16) (g) << 8)) | (((PRUint32) (PRUint8) (b)) << 16))) 

PrefResult
PREF_GetColorPrefDWord(const char *pref_name, PRUint32 *colorref, PRBool isDefault)
{
    PRUint8 red, green, blue;
    PrefResult   result;
    PR_ASSERT(colorref);
    result = PREF_GetColorPref(pref_name, &red, &green, &blue, isDefault);
    if (result == PREF_NOERROR)
        *colorref = MYRGB(red,green,blue);
    return result;
}


PrefResult
PREF_GetBinaryPref(const char *pref_name, void * return_value, int *size, PRBool isDefault)
{
    char* buf;
    PrefResult result;

    if (!gMochaPrefObject || !return_value) return PREF_ERROR;

    result = PREF_CopyCharPref(pref_name, &buf, isDefault);

    if (result == PREF_NOERROR)
    {
        if (PL_strlen(buf) == 0)
        {       /* don't decode empty string ? */
            PR_Free(buf);
            return PREF_ERROR;
        }
    
        PL_Base64Decode(buf, (PRUint32)(*size), return_value);
        
        PR_Free(buf);
    }
    return result;
}

typedef PrefResult (*CharPrefReadFunc)(const char*, char**, PRBool);

static PrefResult
ReadCharPrefUsing(const char *pref_name, void** return_value, int *size, CharPrefReadFunc inFunc, PRBool isDefault)
{
    char* buf;
    PrefResult result;

    if (!gMochaPrefObject || !return_value)
        return PREF_ERROR;
    *return_value = NULL;

    result = inFunc(pref_name, &buf, isDefault);

    if (result == PREF_NOERROR)
    {
        if (PL_strlen(buf) == 0)
        {       /* do not decode empty string? */
            PR_Free(buf);
            return PREF_ERROR;
        }
    
        *return_value = PL_Base64Decode(buf, 0, NULL);
        *size = PL_strlen(buf);
        
        PR_Free(buf);
    }
    return result;
}

PrefResult
PREF_CopyBinaryPref(const char *pref_name, void  ** return_value, int *size, PRBool isDefault)
{
    return ReadCharPrefUsing(pref_name, return_value, size, PREF_CopyCharPref, isDefault);
}

#ifndef XP_MAC
PrefResult
PREF_CopyPathPref(const char *pref_name, char ** return_buffer, PRBool isDefault)
{
    return PREF_CopyCharPref(pref_name, return_buffer, isDefault);
}

PrefResult
PREF_SetPathPref(const char *pref_name, const char *path, PRBool set_default)
{
    PrefAction action = set_default ? PREF_SETDEFAULT : PREF_SETUSER;
    PrefValue pref;
    pref.stringVal = (char*) path;
    
    return pref_HashPref(pref_name, pref, PREF_STRING, action);
}
#endif /* XP_MAC */

/* Delete a branch. Used for deleting mime types */
int
pref_DeleteItem(PLHashEntry *he, int i, void *arg)
{
    const char *to_delete = (const char *) arg;
    int len = PL_strlen(to_delete);
    
    /* note if we're deleting "ldap" then we want to delete "ldap.xxx"
        and "ldap" (if such a leaf node exists) but not "ldap_1.xxx" */
    if (to_delete && (PL_strncmp(he->key, to_delete, (PRUint32) len) == 0 ||
        (len-1 == (int)PL_strlen(he->key) && PL_strncmp(he->key, to_delete, (PRUint32)(len-1)) == 0)))
        return HT_ENUMERATE_REMOVE;
    else
        return HT_ENUMERATE_NEXT;
}

PrefResult
PREF_DeleteBranch(const char *branch_name)
{
    char* branch_dot;
    int len = (int)PL_strlen(branch_name);

    if (!gHashTable)
        return PREF_NOT_INITIALIZED;

    /* The following check insures that if the branch name already has a "."
     * at the end, we don't end up with a "..". This fixes an incompatibility
     * between nsIPref, which needs the period added, and nsIPrefBranch which
     * does not. When nsIPref goes away this function should be fixed to 
     * never add the period at all.
     */
    if ((len > 1) && (branch_name[len - 1] == '.'))
        branch_dot = (char *)branch_name;
    else
    {
        branch_dot = PR_smprintf("%s.", branch_name);
        if (!branch_dot)
            return PREF_OUT_OF_MEMORY;
    }

    PR_HashTableEnumerateEntries(gHashTable, pref_DeleteItem, (void*) branch_dot);
    
    if (branch_dot != branch_name)
        PR_Free(branch_dot);
    return PREF_NOERROR;
}


PrefResult
PREF_ClearUserPref(const char *pref_name)
{
    PrefResult success = PREF_ERROR;
    PrefNode*       pref;

    if (!gHashTable)
        return PREF_NOT_INITIALIZED;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);
    if (pref && PREF_HAS_USER_VALUE(pref))
    {
        pref->flags &= ~PREF_USERSET;
        if (gCallbacksEnabled)
            pref_DoCallback(pref_name);
        success = PREF_OK;
    }
    return success;
}

PR_STATIC_CALLBACK(PRIntn)
pref_ClearUserPref(PLHashEntry *he, int i, void *arg)
{
    PrefNode *pref = (PrefNode *) he->value;
    PR_ASSERT(pref);

    if (pref && PREF_HAS_USER_VALUE(pref))
    {
        pref->flags &= ~PREF_USERSET;
        if (gCallbacksEnabled)
            pref_DoCallback(he->key);
    }
    return HT_ENUMERATE_NEXT;
}

PrefResult
PREF_ClearAllUserPrefs()
{
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;
    
    PL_HashTableEnumerateEntries(gHashTable, pref_ClearUserPref, nsnull);
    return PREF_OK;
}

/* Prototype Admin Kit support */
PrefResult
PREF_GetConfigString(const char *obj_name, char * return_buffer, int size,
    int indx, const char *field)
{
    PR_ASSERT( PR_FALSE );
    return PREF_ERROR;
}

/*
 * Administration Kit support 
 */
PrefResult
PREF_CopyConfigString(const char *obj_name, char **return_buffer)
{
    PrefResult success = PREF_ERROR;
    PrefNode* pref = (PrefNode*) PR_HashTableLookup(gHashTable, obj_name);
    
    if (pref && (pref->flags & PREF_STRING))
    {
        if (return_buffer)
            *return_buffer = PL_strdup(pref->defaultPref.stringVal);
        success = PREF_NOERROR;
    }
    return success;
}

PrefResult
PREF_CopyIndexConfigString(const char *obj_name,
    int indx, const char *field, char **return_buffer)
{
    PrefResult success = PREF_ERROR;
    PrefNode* pref;
    char* setup_buf = PR_smprintf("%s_%d.%s", obj_name, indx, field);

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, setup_buf);
    
    if (pref && (pref->flags & PREF_STRING))
    {
        if (return_buffer)
            *return_buffer = PL_strdup(pref->defaultPref.stringVal);
        success = PREF_NOERROR;
    }
    PR_FREEIF(setup_buf);
    return success;
}

PrefResult
PREF_GetConfigInt(const char *obj_name, PRInt32 *return_int)
{
    PrefResult success = PREF_ERROR;
    PrefNode* pref = (PrefNode*) PR_HashTableLookup(gHashTable, obj_name);
    if (pref && (pref->flags & PREF_INT))
    {
        *return_int = pref->defaultPref.intVal;
        success = PREF_NOERROR;
    }

    return success;
}

PrefResult
PREF_GetConfigBool(const char *obj_name, PRBool *return_bool)
{
    PrefNode* pref = (PrefNode*) PR_HashTableLookup(gHashTable, obj_name);  
    if (pref && (pref->flags & PREF_BOOL))
    {
        *return_bool = pref->defaultPref.boolVal;
        return PREF_NOERROR;
    }
    return PREF_ERROR;
}

PrefResult pref_UnlockPref(const char *key)
{
    PrefNode* pref;
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, key);
    if (!pref)
        return PREF_DOES_NOT_EXIST;

    if (PREF_IS_LOCKED(pref))
    {
        pref->flags &= ~PREF_LOCKED;
        if (gCallbacksEnabled)
            pref_DoCallback(key);
    }
    return PREF_OK;
}

PrefResult pref_LockPref(const char *key)
{
    PrefNode* pref;
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;

    pref = (PrefNode*) PR_HashTableLookup(gHashTable, key);
    if (!pref)
        return PREF_DOES_NOT_EXIST;
   
    return pref_HashPref(key, pref->defaultPref, (PrefType)pref->flags, PREF_LOCK);
}

PrefResult
PREF_LockPref(const char *key)
{
    return pref_LockPref(key);
}

/*
 * Hash table functions
 */
static PRBool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type)
{
    PRBool changed = PR_TRUE;
    if (type & PREF_STRING)
    {
        if (oldValue.stringVal && newValue.stringVal)
            changed = (strcmp(oldValue.stringVal, newValue.stringVal) != 0);
    }
    else if (type & PREF_INT)
        changed = oldValue.intVal != newValue.intVal;
    else if (type & PREF_BOOL)
        changed = oldValue.boolVal != newValue.boolVal;
    return changed;
}

static void pref_SetValue(PrefValue* oldValue, PrefValue newValue, PrefType type)
{
    switch (type & PREF_VALUETYPE_MASK)
    {
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

    if (!gHashTable && !pref_useDefaultPrefFile())
        return PREF_NOT_INITIALIZED;
    
    pref = (PrefNode*) PR_HashTableLookup(gHashTable, key);
    if (!pref)
    {
        pref = (PrefNode*) calloc(sizeof(PrefNode), 1);
        if (!pref)  
            return PREF_OUT_OF_MEMORY;
        pref->flags = type;
        if (pref->flags & PREF_BOOL)
            pref->defaultPref.boolVal = (PRBool) BOGUS_DEFAULT_BOOL_PREF_VALUE;
        /* ugly hack -- define it to a default that no pref will ever default to
           this should really get fixed right by some out of band data */
        if (pref->flags & PREF_INT)
            pref->defaultPref.intVal = (PRInt32) BOGUS_DEFAULT_INT_PREF_VALUE;
        PR_HashTableAdd(gHashTable, PL_strdup(key), pref);
    }
    else if ((((PrefType)(pref->flags)) & PREF_VALUETYPE_MASK) != (type & PREF_VALUETYPE_MASK))
    {
      /*PR_ASSERT(0);*/         /* this shouldn't happen */
      /* NS_ASSERTION(0, "Trying to set pref to with the wrong type!"); */
        return PREF_TYPE_CHANGE_ERR;
    }

    switch (action)
    {
        case PREF_SETDEFAULT:
        case PREF_SETCONFIG:
            if (!PREF_IS_LOCKED(pref))
            {       /* ?? change of semantics? */
                if (pref_ValueChanged(pref->defaultPref, value, type))
                {
                    pref_SetValue(&pref->defaultPref, value, type);
                    if (!PREF_HAS_USER_VALUE(pref))
                        result = PREF_VALUECHANGED;
                }
            }
            if (action == PREF_SETCONFIG)
                pref->flags |= PREF_CONFIG;
            break;
  
        case PREF_SETUSER:
            /* If setting to the default value, then un-set the user value.
               Otherwise, set the user value only if it has changed */
            if ( !pref_ValueChanged(pref->defaultPref, value, type) )
            {
                if (PREF_HAS_USER_VALUE(pref))
                {
                    pref->flags &= ~PREF_USERSET;
                    if (!PREF_IS_LOCKED(pref))
                        result = PREF_VALUECHANGED;
                }
            }
            else if ( !PREF_HAS_USER_VALUE(pref) ||
                       pref_ValueChanged(pref->userPref, value, type) )
            {       
                pref_SetValue(&pref->userPref, value, type);
                pref->flags |= PREF_USERSET;
                if (!PREF_IS_LOCKED(pref))
                    result = PREF_VALUECHANGED;
            }
            break;
            
        case PREF_LOCK:
            if (pref_ValueChanged(pref->defaultPref, value, type))
            {
                pref_SetValue(&pref->defaultPref, value, type);
                result = PREF_VALUECHANGED;
            }
            else if (!PREF_IS_LOCKED(pref))
            {
                result = PREF_VALUECHANGED;
            }
            pref->flags |= PREF_LOCKED;
            gIsAnyPrefLocked = PR_TRUE;
            break;
    }

    if (result == PREF_VALUECHANGED && gCallbacksEnabled)
    {
        PrefResult result2 = pref_DoCallback(key);
        if (result2 < 0)
            result = result2;
    }
    return result;
}

PrefType
PREF_GetPrefType(const char *pref_name)
{
    if (gHashTable)
    {
        PrefNode* pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);
        if (pref)
        {
            if (pref->flags & PREF_STRING)
                return PREF_STRING;
            else if (pref->flags & PREF_INT)
                return PREF_INT;
            else if (pref->flags & PREF_BOOL)
                return PREF_BOOL;
        }
    }
    return PREF_INVALID;
}

JSBool PR_CALLBACK pref_NativeDefaultPref
    (JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
    return pref_HashJSPref(argc, argv, PREF_SETDEFAULT);
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
    if (argc >= 1 && JSVAL_IS_STRING(argv[0]))
    {
        const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
        PrefNode* pref = (PrefNode*) PR_HashTableLookup(gHashTable, key);
        
        if (pref && PREF_IS_LOCKED(pref))
        {
            pref->flags &= ~PREF_LOCKED;
            if (gCallbacksEnabled)
                pref_DoCallback(key);
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
    /*void* value = NULL;*/
    PrefNode* pref;
    /*PRBool prefExists = PR_TRUE;*/
    
    if (argc >= 1 && JSVAL_IS_STRING(argv[0]))
    {
        const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
        pref = (PrefNode*) PR_HashTableLookup(gHashTable, key);
        
        if (pref)
        {
            PRBool use_default = (PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref));      
            if (pref->flags & PREF_STRING)
            {
                char* str = use_default ? pref->defaultPref.stringVal : pref->userPref.stringVal;
                JSString* jsstr = JS_NewStringCopyZ(cx, str);
                *rval = STRING_TO_JSVAL(jsstr);
            }
            else if (pref->flags & PREF_INT)
                *rval = INT_TO_JSVAL(use_default ?
                    pref->defaultPref.intVal : pref->userPref.intVal);
            else if (pref->flags & PREF_BOOL)
                *rval = BOOLEAN_TO_JSVAL(use_default ?
                    pref->defaultPref.boolVal : pref->userPref.boolVal);
        }
    }
    return JS_TRUE;
}
/* -- */

PRBool
PREF_PrefIsLocked(const char *pref_name)
{
    PRBool result = PR_FALSE;
    if (gIsAnyPrefLocked) {
        PrefNode* pref = (PrefNode*) PR_HashTableLookup(gHashTable, pref_name);
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
    char*       childList;
    char*       parent;
    int         bufsize;
} PrefChildIter; 

/* if entry begins with the given string, i.e. if string is
  "a"
  and entry is
  "a.b.c" or "a.b"
  then add "a.b" to the list. */
int
pref_addChild(PLHashEntry *he, int i, void *arg)
{
    PrefChildIter* pcs = (PrefChildIter*) arg;
    if ( PL_strncmp(he->key, pcs->parent, PL_strlen(pcs->parent)) == 0 )
    {
        char buf[512];
        char* nextdelim;
        PRUint32 parentlen = PL_strlen(pcs->parent);
        char* substring;

        strncpy(buf, he->key, PR_MIN(512, PL_strlen(he->key) + 2));
        nextdelim = buf + parentlen;
        if (parentlen < PL_strlen(buf))
        {
            /* Find the next delimiter if any and truncate the string there */
            nextdelim = strstr(nextdelim, ".");
            if (nextdelim)
            {
                *nextdelim = ';';
                *(nextdelim + 1) = '\0';
            } else {
                /* otherwise, ensure string always ends with a ';' character so strtok will be happy. */
                strcat(buf, ";");
            }
        }

        substring = strstr(pcs->childList, buf);

        if (!substring)
        {
            int newsize = PL_strlen(pcs->childList) + PL_strlen(buf) + 2;
            if (newsize > pcs->bufsize)
            {
                pcs->bufsize *= 3;
                pcs->childList = (char*) realloc(pcs->childList, sizeof(char) * pcs->bufsize);
                if (!pcs->childList)
                    return HT_ENUMERATE_STOP;
            }
            PL_strcat(pcs->childList, buf);
        }
    }
    return 0;
}

PrefResult
PREF_CreateChildList(const char* parent_node, char **child_list)
{
    PrefChildIter pcs;
    if (!gHashTable)
        return PREF_NOT_INITIALIZED;
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

    PR_HashTableEnumerateEntries(gHashTable, pref_addChild, &pcs);

    *child_list = pcs.childList;
    PR_Free(pcs.parent);
    
    return (pcs.childList == NULL) ? PREF_OUT_OF_MEMORY : PREF_OK;
}

char*
PREF_NextChild(char *child_list, int *indx)
{
#if HAVE_STRTOK_R
	char *nextstr;
    char* child = strtok_r(&child_list[*indx], ";", nextstr);
#else
    char* child = strtok(&child_list[*indx], ";");
#endif
    if (child)
        *indx += PL_strlen(child) + 1;
    return child;
}

/*----------------------------------------------------------------------------------------
*   pref_copyTree
*
*   A recursive function that copies all the prefs in some subtree to
*   another subtree. Either srcPrefix or dstPrefix can be empty strings,
*   but not NULL pointers. Preferences in the destination are created if 
*   they do not already exist; otherwise the old values are replaced.
*
*   Example calls:
*
*       Copy all the prefs to another tree:         pref_copyTree("", "temp", "")
*
*       Copy all the prefs under mail. to newmail.: pref_copyTree("mail", "newmail", "mail")
*
--------------------------------------------------------------------------------------*/ 
PrefResult pref_copyTree(const char *srcPrefix, const char *destPrefix, const char *curSrcBranch)
{
    PrefResult      result = PREF_NOERROR;

    char*   children = NULL;
    
    if ( PREF_CreateChildList(curSrcBranch, &children) == PREF_NOERROR )
    {   
        int     indx = 0;
        int     srcPrefixLen = PL_strlen(srcPrefix);
        char*   child = NULL;
        
        while ( (child = PREF_NextChild(children, &indx)) != NULL)
        {
            PrefType prefType;
            char    *destPrefName = NULL;
            char    *childStart = (srcPrefixLen > 0) ? (child + srcPrefixLen + 1) : child;
            
            /*NS_ASSERTION( PL_strncmp(child, curSrcBranch, (PRUint32)srcPrefixLen) == 0, "bad pref child in pref_copyTree" );*/
            
            if (*destPrefix > 0)
                destPrefName = PR_smprintf("%s.%s", destPrefix, childStart);
            else
                destPrefName = PR_smprintf("%s", childStart);
            
            if (!destPrefName)
            {
                result = PREF_OUT_OF_MEMORY;
                break;
            }
            
            if ( ! PREF_PrefIsLocked(destPrefName) )        /* returns true if the prefs exists, and is locked */
            {
                /*  PREF_GetPrefType masks out the other bits of the pref flag, so we only
                    ever get the values in the switch.
                */
                prefType = PREF_GetPrefType(child);
                
                switch (prefType)
                {
                    case PREF_STRING:
                        {
                            char    *prefVal = NULL;
                            
                            result = PREF_CopyCharPref(child, &prefVal, PR_FALSE);
                            if (result == PREF_NOERROR)
                                result = PREF_SetCharPref(destPrefName, prefVal);
                                
                            PR_FREEIF(prefVal);
                        }
                        break;
                    
                    case PREF_INT:
                            {
                            PRInt32     prefValInt;
                            
                            result = PREF_GetIntPref(child, &prefValInt, PR_FALSE);
                            if (result == PREF_NOERROR)
                                result = PREF_SetIntPref(destPrefName, prefValInt);
                        }
                        break;
                        
                    case PREF_BOOL:
                        {
                            PRBool  prefBool;
                            
                            result = PREF_GetBoolPref(child, &prefBool, PR_FALSE);
                            if (result == PREF_NOERROR)
                                result = PREF_SetBoolPref(destPrefName, prefBool);
                        }
                        break;
                    
                    case PREF_INVALID:
                        /*  this is probably just a branch. Since we can have both
                             a.b and a.b.c as valid prefs, this is OK.
                        */
                        break;
                        
                    default:
                        /* we should never get here */
                        PR_ASSERT(PR_FALSE);
                        break;
                }
                
            }   /* is not locked */
            
            PR_FREEIF(destPrefName);
            
            /* Recurse */
            if (result == PREF_NOERROR || result == PREF_VALUECHANGED)
                result = pref_copyTree(srcPrefix, destPrefix, child);
        }
        
        PR_Free(children);
    }
    
    return result;
}

PrefResult
PREF_CopyPrefsTree(const char *srcRoot, const char *destRoot)
{
    PR_ASSERT(srcRoot != NULL);
    PR_ASSERT(destRoot != NULL);
    
    return pref_copyTree(srcRoot, destRoot, srcRoot);
}

/* Adds a node to the beginning of the callback list. */
void
PREF_RegisterCallback(const char *pref_node,
                       PrefChangedFunc callback,
                       void * instance_data)
{
    struct CallbackNode* node = (struct CallbackNode*) malloc(sizeof(struct CallbackNode));
    if (node)
    {
        node->domain = PL_strdup(pref_node);
        node->func = callback;
        node->data = instance_data;
        node->next = gCallbacks;
        gCallbacks = node;
    }
    return;
}

/* Deletes a node from the callback list. */
PrefResult
PREF_UnregisterCallback(const char *pref_node,
                         PrefChangedFunc callback,
                         void * instance_data)
{
    PrefResult result = PREF_ERROR;
    struct CallbackNode* node = gCallbacks;
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
                gCallbacks = next_node;
            PR_Free(node->domain);
            PR_Free(node);
            node = next_node;
            result = PREF_NOERROR;
        }
        else
        {
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
    for (node = gCallbacks; node != NULL; node = node->next)
    {
        if ( PL_strncmp(changed_pref, node->domain, PL_strlen(node->domain)) == 0 )
        {
            int result2 = (*node->func) (changed_pref, node->data);
            if (result2 != 0)
                result = (PrefResult)result2;
        }
    }
    return result;
}

/* !! Front ends need to implement */
#ifndef XP_MAC /* see macpref.cp */
PRBool
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
#if (defined (XP_MAC) && defined(powerc)) || defined (XP_WIN) || defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
    if (!gAutoAdminLib)
        gAutoAdminLib = pref_LoadAutoAdminLib();
        
    if (gAutoAdminLib)
    {
        get_ldap_attributes = (ldap_func)
            PR_FindSymbol(
             gAutoAdminLib,
#ifndef XP_WIN16
            "pref_get_ldap_attributes"
#else
            MAKEINTRESOURCE(1)
#endif
            );
    }
    if (get_ldap_attributes == NULL)
    {
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
        && JSVAL_IS_STRING(argv[3]))
    {
        char *return_error = NULL;
        char *value = get_ldap_attributes(
            JS_GetStringBytes(JSVAL_TO_STRING(argv[0])),
            JS_GetStringBytes(JSVAL_TO_STRING(argv[1])),
            JS_GetStringBytes(JSVAL_TO_STRING(argv[2])),
            JS_GetStringBytes(JSVAL_TO_STRING(argv[3])),
            &return_error );
        
        if (value)
        {
            JSString* str = JS_NewStringCopyZ(cx, value);
            PR_Free(value);
            if (str)
            {
                *rval = STRING_TO_JSVAL(str);
                return JS_TRUE;
            }
        }
        if (return_error)
            pref_Alert(return_error);
    }
#endif
    
    *rval = JSVAL_NULL;
    return JS_TRUE;
}

/* Dump debugging info in response to about:config.
 */
int
pref_printDebugInfo(PLHashEntry *he, int i, void *arg)
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
    
    if ((PL_strlen(buf2) + PL_strlen(pcs->childList) + 1) > (PRUint32)pcs->bufsize)
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

char *
PREF_AboutConfig()
{
    PrefChildIter pcs;
    pcs.bufsize = 8192;
    pcs.childList = (char*) malloc(sizeof(char) * pcs.bufsize);
    pcs.childList[0] = '\0';
    PL_strcat(pcs.childList, "<HTML>"); 

    PR_HashTableEnumerateEntries(gHashTable, pref_printDebugInfo, &pcs);
    
    return pcs.childList;
}

#define MAYBE_GC_BRANCH_COUNT_MASK  4095

JSBool PR_CALLBACK
pref_BranchCallback(JSContext *cx, JSScript *script)
{ 
    static PRUint32 count = 0;
    
    /*
     * If we've been running for a long time, then try a GC to 
     * free up some memory.
     */ 
    if ( (++count & MAYBE_GC_BRANCH_COUNT_MASK) == 0 )
        JS_MaybeGC(cx); 

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

#if defined(XP_MAC)
    // StandardAlert doesn't handle linefeeds. Use spaces to avoid garbage characters.
    last = PR_sprintf_append(last, "  ");
#else
    last = PR_sprintf_append(last, LINEBREAK LINEBREAK);
#endif
    if (!report)
        last = PR_sprintf_append(last, "%s\n", message);
    else
    {
        if (report->filename)
            last = PR_sprintf_append(last, "%s, ",
                                     report->filename, report->filename);
        if (report->lineno)
            last = PR_sprintf_append(last, "line %u: ", report->lineno);
        last = PR_sprintf_append(last, "%s. ", message);
        if (report->linebuf)
        {
            for (s = t = report->linebuf; *s != '\0'; s = t)
            {
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

    if (last)
    {
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
    pmsg[0] = PL_strlen(msg);
    BlockMoveData(msg, pmsg + 1, pmsg[0]);
    StandardAlert(kAlertPlainAlert, "\pConfiguration Warning", pmsg, NULL, &itemHit);
}

#else

/* Platform specific alert messages */
void pref_Alert(char* msg)
{
#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#if defined(XP_UNIX) || defined(XP_OS2)
    if ( getenv("NO_PREF_SPAM") == NULL )
#endif
    fputs(msg, stderr);
#endif
#if defined(XP_WIN)
      MessageBox (NULL, msg, "Configuration Warning", MB_OK);
#elif defined(XP_OS2)
      WinMessageBox (HWND_DESKTOP, 0, msg, "Configuration Warning", 0, MB_WARNING | MB_OK | MB_APPLMODAL | MB_MOVEABLE);
#endif
}

#endif

#ifdef XP_WIN16
#define ADMNLIBNAME "adm1640.dll"
#elif defined XP_WIN || defined XP_OS2
#define ADMNLIBNAME "adm3240.dll"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#define ADMNLIBNAME "libAutoAdmin.so"
extern void fe_GetProgramDirectory(char *path, int len);
#else
#define ADMNLIBNAME "AutoAdmin" /* internal fragment name */
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

#if defined(XP_UNIX) && !defined(B_ONE_M)
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

/*--------------------------------------------------------------------------------------*/
static JSBool pref_HashJSPref(unsigned int argc, jsval *argv, PrefAction action)
/* Native implementations of JavaScript functions
    pref        -> pref_NativeDefaultPref
    defaultPref -> "
    userPref    -> pref_NativeUserPref
    lockPref    -> pref_NativeLockPref
    unlockPref  -> pref_NativeUnlockPref
    getPref     -> pref_NativeGetPref
    config      -> pref_NativeSetConfig
 *--------------------------------------------------------------------------------------*/
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

    if (bIsString)
    {
        value.stringVal = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
        pref_HashPref(key, value, PREF_STRING, action);
    }
    
#else
    if (argc >= 2 && JSVAL_IS_STRING(argv[0]))
    {
        PrefValue value;
        const char *key = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
        
        if (JSVAL_IS_STRING(argv[1]))
        {
            value.stringVal = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
            pref_HashPref(key, value, PREF_STRING, action);
        }
        else if (JSVAL_IS_INT(argv[1]))
        {
            value.intVal = JSVAL_TO_INT(argv[1]);
            pref_HashPref(key, value, PREF_INT, action);
        }
        else if (JSVAL_IS_BOOLEAN(argv[1]))
        {
            value.boolVal = JSVAL_TO_BOOLEAN(argv[1]);
            pref_HashPref(key, value, PREF_BOOL, action);
        }
    }
#endif

    return JS_TRUE;
}

/*--------------------------------------------------------------------------------------*/
static int pref_CountListMembers(char* list)
/*--------------------------------------------------------------------------------------*/
{
    int members = 0;
    char* p = list = PL_strdup(list);
#ifdef HAVE_STRTOK_R
    char* nextstr;
    for ( p = strtok_r(p, ",", &nextstr); p != NULL; p = strtok_r(nextstr, ",", &nextstr) )
#else
    for ( p = strtok(p, ","); p != NULL; p = strtok(NULL, ",") )
#endif
        members++;
    PR_FREEIF(list);
    return members;
}


/*--------------------------------------------------------------------------------------*/
PrefResult PREF_GetListPref(const char* pref, char*** list, PRBool isDefault)
/* Splits a comma separated string into an array of strings.
 * The array of strings is actually just an array of pointers into a copy
 * of the value returned by PREF_CopyCharPref().  So, we don't have to
 * allocate each string separately.
----------------------------------------------------------------------------------------*/
{
    char* value;
    char** p;
    int nugmembers;
#ifdef HAVE_STRTOK_R
    char* nextstr;
#endif

    *list = NULL;

    if ( PREF_CopyCharPref(pref, &value, isDefault) != PREF_OK || value == NULL )
        return PREF_ERROR;

    nugmembers = pref_CountListMembers(value);

    p = *list = (char**) PR_MALLOC((nugmembers+1) * sizeof(char**));
    if ( *list == NULL ) return PREF_ERROR;

#ifdef HAVE_STRTOK_R
    for ( *p = strtok_r(value, ",", &nextstr);
          *p != NULL;
          *(++p) = strtok_r(nextstr, ",", &nextstr) ) /* Empty body */ ;
#else
    for ( *p = strtok(value, ","); 
          *p != NULL; 
          *(++p) = strtok(NULL, ",") ) /* Empty body */ ;
#endif

    /* Copy each entry so that users can free them. */
    for ( p = *list; *p != NULL; p++ )
        *p = PL_strdup(*p);

    PR_Free(value);

    return PREF_OK;
}


/*--------------------------------------------------------------------------------------*/
PrefResult PREF_SetListPref(const char* pref, char** list)
/* TODO: Call Javascript callback to make sure user is allowed to make this
 * change.
----------------------------------------------------------------------------------------*/
{
    PrefResult status;
    int len;
    char** p;
    char* value = NULL;

    if ( pref == NULL || list == NULL ) return PREF_ERROR;

    for ( len = 0, p = list; p != NULL && *p != NULL; p++ )
        len+= (PL_strlen(*p)+1); /* The '+1' is for a comma or '\0' */

    if ( len <= 0 || (value = PR_MALLOC((PRUint32)len)) == NULL )
        return PREF_ERROR;

    (void) PL_strcpy(value, *list);
    for ( p = list+1; p != NULL && *p != NULL; p++ )
    {
        (void) PL_strcat(value, ",");
        (void) PL_strcat(value, *p);
    }

    status = PREF_SetCharPref(pref, value);

    PR_FREEIF(value);

    return status;  
}


/*--------------------------------------------------------------------------------------*/
PrefResult
PREF_AppendListPref(const char* pref, const char* value)
/*--------------------------------------------------------------------------------------*/
{
    char *pListPref = NULL, *pNewList = NULL;
    int nPrefLen = 0;

    PREF_CopyCharPref(pref, &pListPref, PR_FALSE);

    if (pListPref)
    {
        nPrefLen = PL_strlen(pListPref);
    }

    if (nPrefLen == 0)
        PREF_SetCharPref(pref, value);
    else
    {
        pNewList = (char *) PR_MALLOC((nPrefLen + PL_strlen(value) + 2));
        if (pNewList)
        {
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

/*--------------------------------------------------------------------------------------*/
PrefResult
PREF_FreeListPref(char*** list)
/* Free each element in the list, then free the list, then NULL the
 * list out.
 *--------------------------------------------------------------------------------------*/
{
    char** p;
    if (!list)
        return PREF_ERROR;

    for ( p = *list; *p != NULL; p++ )
        PR_Free(*p);

    PR_FREEIF(*list);
    return PREF_OK;
}
