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
#include "prefapi_private_data.h"
#include "nsReadableUtils.h"
#include "jsapi.h"
#include "nsCRT.h"

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

#ifdef MOZ_SECURITY
#include "sechash.h"
#endif
#include "plstr.h"
#include "pldhash.h"
#include "plbase64.h"
#include "prlog.h"
#include "prmem.h"
#include "prprf.h"
#include "nsQuickSort.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "prlink.h"

#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>
#endif

extern JSRuntime* PREF_GetJSRuntime();

#define BOGUS_DEFAULT_INT_PREF_VALUE (-5632)
#define BOGUS_DEFAULT_BOOL_PREF_VALUE (-2)

PR_STATIC_CALLBACK(void)
clearPrefEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    PrefHashEntry *pref = NS_STATIC_CAST(PrefHashEntry *, entry);
    if (pref->flags & PREF_STRING)
    {
        PR_FREEIF(pref->defaultPref.stringVal);
        PR_FREEIF(pref->userPref.stringVal);
    }
    // don't need to free this as it's allocated in memory owned by
    // the global PrefNameBuffer
    pref->key = nsnull;
    memset(entry, 0, table->entrySize);
}

PR_STATIC_CALLBACK(PRBool)
matchPrefEntry(PLDHashTable*, const PLDHashEntryHdr* entry,
               const void* key)
{
    const PrefHashEntry *prefEntry =
        NS_STATIC_CAST(const PrefHashEntry*,entry);
    
    if (prefEntry->key == key) return PR_TRUE;
    
    if (!prefEntry->key || !key) return PR_FALSE;

    const char *otherKey = NS_REINTERPRET_CAST(const char*, key);
    return (strcmp(prefEntry->key, otherKey) == 0);
}

PR_STATIC_CALLBACK(JSBool) pref_NativeDefaultPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
PR_STATIC_CALLBACK(JSBool) pref_NativeUserPref(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);
/*----------------------------------------------------------------------------------------*/

JS_STATIC_DLL_CALLBACK(JSBool)
global_enumerate(JSContext *cx, JSObject *obj)
{
    return JS_EnumerateStandardClasses(cx, obj);
}

JS_STATIC_DLL_CALLBACK(JSBool)
global_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSBool resolved;

    return JS_ResolveStandardClass(cx, obj, id, &resolved);
}

JSContext *       gMochaContext = NULL;
PRBool              gErrorOpeningUserPrefs = PR_FALSE;
PLDHashTable        gHashTable = { nsnull };
PRBool              gDirty = PR_FALSE;

static JSRuntime *       gMochaTaskState = NULL;
static JSObject *        gMochaPrefObject = NULL;
static JSObject *        gGlobalConfigObject = NULL;
static JSClass      global_class = {
                    "global", 0,
                    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
                    global_enumerate, global_resolve, JS_ConvertStub, JS_FinalizeStub,
                    JSCLASS_NO_OPTIONAL_MEMBERS
                    };
static JSClass      autoconf_class = {
                    "PrefConfig", 0,
                    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
                    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
                    JSCLASS_NO_OPTIONAL_MEMBERS
                    };
static JSPropertySpec autoconf_props[] = {
                    {0,0,0,0,0}
                    };
static JSFunctionSpec autoconf_methods[] = {
                    { "pref",               pref_NativeDefaultPref, 2,0,0 },
                    { "user_pref",          pref_NativeUserPref,    2,0,0 },
                    { NULL,                 NULL,                   0,0,0 }
                    };

static struct CallbackNode* gCallbacks = NULL;
static PRBool       gCallbacksEnabled = PR_FALSE;
static PRBool       gIsAnyPrefLocked = PR_FALSE;
static char *       gSavedLine = NULL; 


static PLDHashTableOps     pref_HashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashStringKey,
    matchPrefEntry,
    PL_DHashMoveEntryStub,
    clearPrefEntry,
    PL_DHashFinalizeStub,
    nsnull,
};
    
class PrefNameBuffer;

// PR_ALIGN_OF_WORD is only defined on some platforms.  ALIGN_OF_WORD has
// already been defined to PR_ALIGN_OF_WORD everywhere
#ifndef PR_ALIGN_OF_WORD
#define PR_ALIGN_OF_WORD PR_ALIGN_OF_POINTER
#endif

// making PrefNameBuffer exactly 8k for nice allocation
#define PREFNAME_BUFFER_LEN (8192 - (sizeof(PrefNameBuffer*) + sizeof(char*)))

#define WORD_ALIGN_MASK (PR_ALIGN_OF_WORD - 1)

// sanity checking
#if (PR_ALIGN_OF_WORD & WORD_ALIGN_MASK) != 0
#error "PR_ALIGN_OF_WORD must be a power of 2!"
#endif

class PrefNameBuffer {
 public:
    PrefNameBuffer(PrefNameBuffer* aNext) : mNext(aNext), mNextFree(0)
        {}
    static const char *StrDup(const char*);
    static void FreeAllBuffers();

 private:
    char *Alloc(PRInt32 len);

    static PrefNameBuffer *gRoot;

    // member variables
    class PrefNameBuffer* mNext;
    PRUint32 mNextFree;
    char mBuf[PREFNAME_BUFFER_LEN];
};

PrefNameBuffer *PrefNameBuffer::gRoot = nsnull;

char*
PrefNameBuffer::Alloc(PRInt32 len)
{
    NS_ASSERTION(this == gRoot, "Can only allocate on the root!\n");
    NS_ASSERTION(len < PREFNAME_BUFFER_LEN, "Can only allocate short strings\n");

    // check for space in the current buffer
    if ((mNextFree + len) > PREFNAME_BUFFER_LEN) {
        // allocate and update the root
        gRoot = new PrefNameBuffer(this);
        return gRoot->Alloc(len);
    }

    // ok, we have space.
    char *result = &mBuf[mNextFree];
    
    mNextFree += len;

    // now align the next free allocation
    mNextFree = (mNextFree + WORD_ALIGN_MASK) & ~WORD_ALIGN_MASK;

    return result;
}

// equivalent to strdup() - does no error checking,
// we're assuming we're only called with a valid pointer
const char *
PrefNameBuffer::StrDup(const char *str)
{
    if (!gRoot) {
        gRoot = new PrefNameBuffer(nsnull);
        
        // ack, no memory
        if (!gRoot)
            return nsnull;
    }

    // extra byte for null-termination
    PRInt32 len = strlen(str) + 1;
    
    char *buf = gRoot->Alloc(len);

    memcpy(buf, str, len);
    
    return buf;
}

void
PrefNameBuffer::FreeAllBuffers()
{
    PrefNameBuffer *curr = gRoot;
    PrefNameBuffer *next;
    while (curr) {
        next = curr->mNext;
        delete curr;
        curr = next;
    }
    gRoot = nsnull;
}

/*---------------------------------------------------------------------------*/

#define PREF_IS_LOCKED(pref)            ((pref)->flags & PREF_LOCKED)
#define PREF_IS_CONFIG(pref)            ((pref)->flags & PREF_CONFIG)
#define PREF_HAS_USER_VALUE(pref)       ((pref)->flags & PREF_USERSET)
#define PREF_TYPE(pref)                 (PrefType)((pref)->flags & PREF_VALUETYPE_MASK)

static JSBool pref_HashJSPref(unsigned int argc, jsval *argv, PrefAction action);
static PRBool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type);

/* -- Privates */
struct CallbackNode {
    char*                   domain;
    PrefChangedFunc         func;
    void*                   data;
    struct CallbackNode*    next;
};

/* -- Prototypes */
static PrefResult pref_DoCallback(const char* changed_pref);


PR_STATIC_CALLBACK(JSBool) pref_BranchCallback(JSContext *cx, JSScript *script);
PR_STATIC_CALLBACK(void) pref_ErrorReporter(JSContext *cx, const char *message,JSErrorReport *report);
static void pref_Alert(char* msg);
static PrefResult pref_HashPref(const char *key, PrefValue value, PrefType type, PrefAction action);
static inline PrefHashEntry* pref_HashTableLookup(const void *key);
  

PRBool PREF_Init(const char *filename)
{
    PRBool ok = PR_TRUE, request = PR_FALSE;

    if (!gHashTable.ops) {
        if (!PL_DHashTableInit(&gHashTable, &pref_HashTableOps, nsnull,
                               sizeof(PrefHashEntry), 1024))
            gHashTable.ops = nsnull;
    }
        
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

    if (gHashTable.ops) {
        PL_DHashTableFinish(&gHashTable);
        gHashTable.ops = nsnull;
    }

    PrefNameBuffer::FreeAllBuffers();
    
    if (gSavedLine)
        free(gSavedLine);
    gSavedLine = NULL;
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
        gSavedLine = (char *)malloc(i + 1);
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

// note that this appends to aResult, and does not assign!
static void str_escape(const char * original, nsAFlatCString& aResult)
{
    /* JavaScript does not allow quotes, slashes, or line terminators inside
     * strings so we must escape them. ECMAScript defines four line
     * terminators, but we're only worrying about \r and \n here.  We currently
     * feed our pref script to the JS interpreter as Latin-1 so  we won't
     * encounter \u2028 (line separator) or \u2029 (paragraph separator).
     *
     * WARNING: There are hints that we may be moving to storing prefs
     * as utf8. If we ever feed them to the JS compiler as UTF8 then
     * we'll have to worry about the multibyte sequences that would be
     * interpreted as \u2028 and \u2029
     */
    const char *p;

    if (original == NULL)
        return;

    /* Paranoid worst case all slashes will free quickly */
    for  (p=original; *p; ++p)
    {
        switch (*p)
        {
            case '\n':
                aResult.Append("\\n");
                break;

            case '\r':
                aResult.Append("\\r");
                break;

            case '\\':
                aResult.Append("\\\\");
                break;

            case '\"':
                aResult.Append("\\\"");
                break;

            default:
                aResult.Append(*p);
                break;
        }
    }
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

PLDHashOperator
pref_savePref(PLDHashTable *table, PLDHashEntryHdr *heh, PRUint32 i, void *arg)
{
    char **prefArray = (char**) arg;
    PrefHashEntry *pref = NS_STATIC_CAST(PrefHashEntry *, heh);

    PR_ASSERT(pref);
    if (!pref)
        return PL_DHASH_NEXT;

    nsCAutoString prefValue;

    // where we're getting our pref from
    PrefValue* sourcePref;

    if (PREF_HAS_USER_VALUE(pref) && 
        pref_ValueChanged(pref->defaultPref, 
                          pref->userPref, 
                          (PrefType) PREF_TYPE(pref)))
        sourcePref = &pref->userPref;
    else if (PREF_IS_LOCKED(pref))
        sourcePref = &pref->defaultPref;
    else
        // do not save default prefs that haven't changed
        return PL_DHASH_NEXT;

    // strings are in quotes!
    if (pref->flags & PREF_STRING) {
        prefValue = '\"';
        str_escape(sourcePref->stringVal, prefValue);
        prefValue += '\"';
    }

    else if (pref->flags & PREF_INT)
        prefValue.AppendInt(sourcePref->intVal);

    else if (pref->flags & PREF_BOOL)
        prefValue = (sourcePref->boolVal) ? "true" : "false";

    nsCAutoString prefName;
    str_escape(pref->key, prefName);

    prefArray[i] = ToNewCString(NS_LITERAL_CSTRING("user_pref(\"") +
                                prefName +
                                NS_LITERAL_CSTRING("\", ") +
                                prefValue +
                                NS_LITERAL_CSTRING(");"));
    return PL_DHASH_NEXT;
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


PRBool PREF_HasUserPref(const char *pref_name)
{
    PrefHashEntry *pref;
    
    if (!gHashTable.ops)
        return PR_FALSE;

    pref = pref_HashTableLookup(pref_name);

    if (!pref) return PR_FALSE;
    
    /* convert PREF_HAS_USER_VALUE to bool */
    return (PREF_HAS_USER_VALUE(pref) != 0);

}
PrefResult PREF_GetCharPref(const char *pref_name, char * return_buffer, int * length, PRBool get_default)
{
    PrefResult result = PREF_ERROR;
    char* stringVal;
    
    PrefHashEntry* pref;

    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    pref = pref_HashTableLookup(pref_name);

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
    PrefHashEntry* pref;

    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    pref = pref_HashTableLookup(pref_name);

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
    PrefHashEntry* pref;

    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    pref = pref_HashTableLookup(pref_name);
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
    PrefHashEntry* pref;

    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    pref = pref_HashTableLookup(pref_name);
    //NS_ASSERTION(pref, pref_name);
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

/* Delete a branch. Used for deleting mime types */
PR_STATIC_CALLBACK(PLDHashOperator)
pref_DeleteItem(PLDHashTable *table, PLDHashEntryHdr *heh, PRUint32 i, void *arg)
{
    PrefHashEntry* he = NS_STATIC_CAST(PrefHashEntry*,heh);
    const char *to_delete = (const char *) arg;
    int len = PL_strlen(to_delete);
    
    /* note if we're deleting "ldap" then we want to delete "ldap.xxx"
        and "ldap" (if such a leaf node exists) but not "ldap_1.xxx" */
    if (to_delete && (PL_strncmp(he->key, to_delete, (PRUint32) len) == 0 ||
        (len-1 == (int)PL_strlen(he->key) && PL_strncmp(he->key, to_delete, (PRUint32)(len-1)) == 0)))
        return PL_DHASH_REMOVE;
    
    return PL_DHASH_NEXT;
}

PrefResult
PREF_DeleteBranch(const char *branch_name)
{
    int len = (int)PL_strlen(branch_name);

    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    /* The following check insures that if the branch name already has a "."
     * at the end, we don't end up with a "..". This fixes an incompatibility
     * between nsIPref, which needs the period added, and nsIPrefBranch which
     * does not. When nsIPref goes away this function should be fixed to 
     * never add the period at all.
     */
    nsCAutoString branch_dot(branch_name);
    if ((len > 1) && branch_name[len - 1] != '.')
        branch_dot += '.';

    PL_DHashTableEnumerate(&gHashTable, pref_DeleteItem,
                           (void*) branch_dot.get());
    gDirty = PR_TRUE;
    return PREF_NOERROR;
}


PrefResult
PREF_ClearUserPref(const char *pref_name)
{
    PrefResult success = PREF_ERROR;
    PrefHashEntry*       pref;

    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    pref = pref_HashTableLookup(pref_name);
    if (pref && PREF_HAS_USER_VALUE(pref))
    {
        pref->flags &= ~PREF_USERSET;
        if (gCallbacksEnabled)
            pref_DoCallback(pref_name);
        success = PREF_OK;
        gDirty = PR_TRUE;
    }
    return success;
}

PR_STATIC_CALLBACK(PLDHashOperator)
pref_ClearUserPref(PLDHashTable *table, PLDHashEntryHdr *he, PRUint32,
                   void *arg)
{
    PrefHashEntry *pref = NS_STATIC_CAST(PrefHashEntry*,  he);

    if (PREF_HAS_USER_VALUE(pref))
    {
        // Note that we're not unhashing the pref. A pref which has both
        // a user value and a default value needs to remain. Currently,
        // there isn't a way to determine that a pref has a default value,
        // other than comparing its value to BOGUS_DEFAULT_XXX_PREF_VALUE.
        // This needs to be fixed. If we could positively identify a pref
        // as not having a set default value here, we could unhash it.

        pref->flags &= ~PREF_USERSET;
        if (gCallbacksEnabled)
            pref_DoCallback(pref->key);
    }
    return PL_DHASH_NEXT;
}

PrefResult
PREF_ClearAllUserPrefs()
{
    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;
    
    PL_DHashTableEnumerate(&gHashTable, pref_ClearUserPref, nsnull);

    gDirty = PR_TRUE;
    return PREF_OK;
}


PrefResult pref_UnlockPref(const char *key)
{
    PrefHashEntry* pref;
    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    pref = pref_HashTableLookup(key);
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

PrefResult PREF_LockPref(const char *key)
{
    PrefHashEntry* pref;
    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;

    pref = pref_HashTableLookup(key);
    if (!pref)
        return PREF_DOES_NOT_EXIST;
   
    return pref_HashPref(key, pref->defaultPref, (PrefType)pref->flags, PREF_LOCK);
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
    gDirty = PR_TRUE;
}

static inline PrefHashEntry* pref_HashTableLookup(const void *key)
{
    PrefHashEntry* result =
        NS_STATIC_CAST(PrefHashEntry*, PL_DHashTableOperate(&gHashTable, key, PL_DHASH_LOOKUP));
    
    if (PL_DHASH_ENTRY_IS_FREE(result))
        return nsnull;
    
    return result;
}

PrefResult pref_HashPref(const char *key, PrefValue value, PrefType type, PrefAction action)
{
    PrefHashEntry* pref;
    PrefResult result = PREF_OK;

    if (!gHashTable.ops)
        return PREF_NOT_INITIALIZED;
    
    pref = NS_STATIC_CAST(PrefHashEntry*, PL_DHashTableOperate(&gHashTable, key, PL_DHASH_ADD));

    if (!pref)
        return PREF_OUT_OF_MEMORY;

    // new entry, better intialize
    if (!pref->key) {
        
        // initialize the pref entry
        pref->flags = type;
        pref->key = PrefNameBuffer::StrDup(key);
        pref->defaultPref.intVal = 0;
        pref->userPref.intVal = 0;
        
        /* ugly hack -- define it to a default that no pref will ever
           default to this should really get fixed right by some out
           of band data
        */
        if (pref->flags & PREF_BOOL)
            pref->defaultPref.boolVal = (PRBool) BOGUS_DEFAULT_BOOL_PREF_VALUE;
        if (pref->flags & PREF_INT)
            pref->defaultPref.intVal = (PRInt32) BOGUS_DEFAULT_INT_PREF_VALUE;
    }
    else if ((((PrefType)(pref->flags)) & PREF_VALUETYPE_MASK) !=
                 (type & PREF_VALUETYPE_MASK))
    {
        NS_WARNING(nsPrintfCString("Trying to set pref %s to with the wrong type!", key).get());
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

    if (result == PREF_VALUECHANGED) {
        gDirty = PR_TRUE;
        
        if (gCallbacksEnabled) {
            PrefResult result2 = pref_DoCallback(key);
            if (result2 < 0)
                result = result2;
        }
    }
    return result;
}

PrefType
PREF_GetPrefType(const char *pref_name)
{
    if (gHashTable.ops)
    {
        PrefHashEntry* pref = pref_HashTableLookup(pref_name);
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

PR_STATIC_CALLBACK(JSBool) pref_NativeDefaultPref
    (JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
    return pref_HashJSPref(argc, argv, PREF_SETDEFAULT);
}

PR_STATIC_CALLBACK(JSBool) pref_NativeUserPref
    (JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
    return pref_HashJSPref(argc, argv, PREF_SETUSER);
}

/* -- */

PRBool
PREF_PrefIsLocked(const char *pref_name)
{
    PRBool result = PR_FALSE;
    if (gIsAnyPrefLocked) {
        PrefHashEntry* pref = pref_HashTableLookup(pref_name);
        if (pref && PREF_IS_LOCKED(pref))
            result = PR_TRUE;
    }
    
    return result;
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

static PrefResult pref_DoCallback(const char* changed_pref)
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

#define MAYBE_GC_BRANCH_COUNT_MASK  4095

PR_STATIC_CALLBACK(JSBool)
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
    /* StandardAlert doesn't handle linefeeds. Use spaces to avoid garbage characters. */
    last = PR_sprintf_append(last, "  ");
#else
    last = PR_sprintf_append(last, NS_LINEBREAK NS_LINEBREAK);
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


/*--------------------------------------------------------------------------------------*/
static JSBool pref_HashJSPref(unsigned int argc, jsval *argv, PrefAction action)
/* Native implementations of JavaScript functions
    pref        -> pref_NativeDefaultPref
    userPref    -> pref_NativeUserPref
 *--------------------------------------------------------------------------------------*/
{   
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

    return JS_TRUE;
}

