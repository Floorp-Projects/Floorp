/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prefapi.h"
#include "prefapi_private_data.h"
#include "prefread.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

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

#ifdef MOZ_PROFILESHARING
#include "nsSharedPrefHandler.h"
#endif

#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>
#endif

#ifdef XP_BEOS
#include "Alert.h"
#endif

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
    // gPrefNameArena
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

PLDHashTable        gHashTable = { nsnull };
static PLArenaPool  gPrefNameArena;
PRBool              gDirty = PR_FALSE;

static struct CallbackNode* gCallbacks = NULL;
static PRBool       gCallbacksEnabled = PR_TRUE;
static PRBool       gIsAnyPrefLocked = PR_FALSE;


static PLDHashTableOps     pref_HashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashStringKey,
    matchPrefEntry,
    PL_DHashMoveEntryStub,
    clearPrefEntry,
    PL_DHashFinalizeStub,
    nsnull,
};

// PR_ALIGN_OF_WORD is only defined on some platforms.  ALIGN_OF_WORD has
// already been defined to PR_ALIGN_OF_WORD everywhere
#ifndef PR_ALIGN_OF_WORD
#define PR_ALIGN_OF_WORD PR_ALIGN_OF_POINTER
#endif

// making PrefName arena 8k for nice allocation
#define PREFNAME_ARENA_SIZE 8192

#define WORD_ALIGN_MASK (PR_ALIGN_OF_WORD - 1)

// sanity checking
#if (PR_ALIGN_OF_WORD & WORD_ALIGN_MASK) != 0
#error "PR_ALIGN_OF_WORD must be a power of 2!"
#endif

// equivalent to strdup() - does no error checking,
// we're assuming we're only called with a valid pointer
static char *ArenaStrDup(const char* str, PLArenaPool* aArena)
{
    void* mem;
    PRUint32 len = strlen(str);
    PL_ARENA_ALLOCATE(mem, aArena, len+1);
    if (mem)
        memcpy(mem, str, len+1);
    return NS_STATIC_CAST(char*, mem);
}

/*---------------------------------------------------------------------------*/

#define PREF_IS_LOCKED(pref)            ((pref)->flags & PREF_LOCKED)
#define PREF_HAS_USER_VALUE(pref)       ((pref)->flags & PREF_USERSET)
#define PREF_TYPE(pref)                 (PrefType)((pref)->flags & PREF_VALUETYPE_MASK)

static PRBool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type);

/* -- Privates */
struct CallbackNode {
    char*                   domain;
    PrefChangedFunc         func;
    void*                   data;
    struct CallbackNode*    next;
};

/* -- Prototypes */
static nsresult pref_DoCallback(const char* changed_pref);


static nsresult pref_HashPref(const char *key, PrefValue value, PrefType type, PRBool defaultPref);
static inline PrefHashEntry* pref_HashTableLookup(const void *key);


nsresult PREF_Init()
{
    if (!gHashTable.ops) {
        if (!PL_DHashTableInit(&gHashTable, &pref_HashTableOps, nsnull,
                               sizeof(PrefHashEntry), 1024)) {
            gHashTable.ops = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        PL_INIT_ARENA_POOL(&gPrefNameArena, "PrefNameArena",
                           PREFNAME_ARENA_SIZE);
    }
    return NS_OK;
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
    if (gHashTable.ops) {
        PL_DHashTableFinish(&gHashTable);
        gHashTable.ops = nsnull;
        PL_FinishArenaPool(&gPrefNameArena);
    }
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
nsresult
PREF_SetCharPref(const char *pref_name, const char *value, PRBool set_default)
{
    PrefValue pref;
    pref.stringVal = (char*) value;

    return pref_HashPref(pref_name, pref, PREF_STRING, set_default);
}

nsresult
PREF_SetIntPref(const char *pref_name, PRInt32 value, PRBool set_default)
{
    PrefValue pref;
    pref.intVal = value;

    return pref_HashPref(pref_name, pref, PREF_INT, set_default);
}

nsresult
PREF_SetBoolPref(const char *pref_name, PRBool value, PRBool set_default)
{
    PrefValue pref;
    pref.boolVal = value ? PR_TRUE : PR_FALSE;

    return pref_HashPref(pref_name, pref, PREF_BOOL, set_default);
}


PLDHashOperator
pref_savePref(PLDHashTable *table, PLDHashEntryHdr *heh, PRUint32 i, void *arg)
{
    pref_saveArgs *argData = NS_STATIC_CAST(pref_saveArgs *, arg);
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

#if MOZ_PROFILESHARING
  if ((argData->saveTypes == SAVE_SHARED &&
      !gSharedPrefHandler->IsPrefShared(pref->key)) ||
      (argData->saveTypes == SAVE_NONSHARED &&
      gSharedPrefHandler->IsPrefShared(pref->key)))
    return PL_DHASH_NEXT;
#endif

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

    argData->prefArray[i] = ToNewCString(NS_LITERAL_CSTRING("user_pref(\"") +
                                prefName +
                                NS_LITERAL_CSTRING("\", ") +
                                prefValue +
                                NS_LITERAL_CSTRING(");"));
    return PL_DHASH_NEXT;
}

int PR_CALLBACK
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
    if (!gHashTable.ops)
        return PR_FALSE;

    PrefHashEntry *pref = pref_HashTableLookup(pref_name);
    if (!pref) return PR_FALSE;

    /* convert PREF_HAS_USER_VALUE to bool */
    return (PREF_HAS_USER_VALUE(pref) != 0);

}
nsresult PREF_GetCharPref(const char *pref_name, char * return_buffer, int * length, PRBool get_default)
{
    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    char* stringVal;

    PrefHashEntry* pref = pref_HashTableLookup(pref_name);

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
            rv = NS_OK;
        }
    }

    return rv;
}

nsresult
PREF_CopyCharPref(const char *pref_name, char ** return_buffer, PRBool get_default)
{
    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    char* stringVal;
    PrefHashEntry* pref = pref_HashTableLookup(pref_name);

    if (pref && (pref->flags & PREF_STRING))
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
            stringVal = pref->defaultPref.stringVal;
        else
            stringVal = pref->userPref.stringVal;

        if (stringVal) {
            *return_buffer = PL_strdup(stringVal);
            rv = NS_OK;
        }
    }
    return rv;
}

nsresult PREF_GetIntPref(const char *pref_name,PRInt32 * return_int, PRBool get_default)
{
    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    PrefHashEntry* pref = pref_HashTableLookup(pref_name);
    if (pref && (pref->flags & PREF_INT))
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
        {
            PRInt32 tempInt = pref->defaultPref.intVal;
            /* check to see if we even had a default */
            if (tempInt == ((PRInt32) BOGUS_DEFAULT_INT_PREF_VALUE))
                return NS_ERROR_UNEXPECTED;
            *return_int = tempInt;
        }
        else
            *return_int = pref->userPref.intVal;
        rv = NS_OK;
    }
    return rv;
}

nsresult PREF_GetBoolPref(const char *pref_name, PRBool * return_value, PRBool get_default)
{
    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    PrefHashEntry* pref = pref_HashTableLookup(pref_name);
    //NS_ASSERTION(pref, pref_name);
    if (pref && (pref->flags & PREF_BOOL))
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
        {
            PRBool tempBool = pref->defaultPref.boolVal;
            /* check to see if we even had a default */
            if (tempBool != ((PRBool) BOGUS_DEFAULT_BOOL_PREF_VALUE)) {
                *return_value = tempBool;
                rv = NS_OK;
            }
        }
        else {
            *return_value = pref->userPref.boolVal;
            rv = NS_OK;
        }
    }
    return rv;
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

nsresult
PREF_DeleteBranch(const char *branch_name)
{
    int len = (int)PL_strlen(branch_name);

    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

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
    return NS_OK;
}


nsresult
PREF_ClearUserPref(const char *pref_name)
{
    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    PrefHashEntry* pref = pref_HashTableLookup(pref_name);
    if (pref && PREF_HAS_USER_VALUE(pref))
    {
        pref->flags &= ~PREF_USERSET;

        if ((pref->flags & PREF_INT && 
             pref->defaultPref.intVal == ((PRInt32) BOGUS_DEFAULT_INT_PREF_VALUE)) ||
            (pref->flags & PREF_BOOL && 
             pref->defaultPref.boolVal == ((PRBool) BOGUS_DEFAULT_BOOL_PREF_VALUE)) ||
            (pref->flags & PREF_STRING && !pref->defaultPref.stringVal)) {
            PL_DHashTableOperate(&gHashTable, pref_name, PL_DHASH_REMOVE);
        }

        if (gCallbacksEnabled)
            pref_DoCallback(pref_name);
        gDirty = PR_TRUE;
        rv = NS_OK;
    }
    return rv;
}

PR_STATIC_CALLBACK(PLDHashOperator)
pref_ClearUserPref(PLDHashTable *table, PLDHashEntryHdr *he, PRUint32,
                   void *arg)
{
    PrefHashEntry *pref = NS_STATIC_CAST(PrefHashEntry*,  he);

    PLDHashOperator nextOp = PL_DHASH_NEXT;

    if (PREF_HAS_USER_VALUE(pref))
    {
        pref->flags &= ~PREF_USERSET;

        if ((pref->flags & PREF_INT && 
             pref->defaultPref.intVal == ((PRInt32) BOGUS_DEFAULT_INT_PREF_VALUE)) ||
            (pref->flags & PREF_BOOL && 
             pref->defaultPref.boolVal == ((PRBool) BOGUS_DEFAULT_BOOL_PREF_VALUE)) ||
            (pref->flags & PREF_STRING && !pref->defaultPref.stringVal)) {
            nextOp = PL_DHASH_REMOVE;
        }

        if (gCallbacksEnabled)
            pref_DoCallback(pref->key);
    }
    return nextOp;
}

nsresult
PREF_ClearAllUserPrefs()
{
    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

    PL_DHashTableEnumerate(&gHashTable, pref_ClearUserPref, nsnull);

    gDirty = PR_TRUE;
    return NS_OK;
}

nsresult PREF_LockPref(const char *key, PRBool lockit)
{
    if (!gHashTable.ops)
        return NS_ERROR_NOT_INITIALIZED;

    PrefHashEntry* pref = pref_HashTableLookup(key);
    if (!pref)
        return NS_ERROR_UNEXPECTED;

    if (lockit) {
        if (!PREF_IS_LOCKED(pref))
        {
            pref->flags |= PREF_LOCKED;
            gIsAnyPrefLocked = PR_TRUE;
            if (gCallbacksEnabled)
                pref_DoCallback(key);
        }
    }
    else
    {
        if (PREF_IS_LOCKED(pref))
        {
            pref->flags &= ~PREF_LOCKED;
            if (gCallbacksEnabled)
                pref_DoCallback(key);
        }
    }
    return NS_OK;
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

nsresult pref_HashPref(const char *key, PrefValue value, PrefType type, PRBool set_default)
{
    if (!gHashTable.ops)
        return NS_ERROR_OUT_OF_MEMORY;

    PrefHashEntry* pref = NS_STATIC_CAST(PrefHashEntry*, PL_DHashTableOperate(&gHashTable, key, PL_DHASH_ADD));

    if (!pref)
        return NS_ERROR_OUT_OF_MEMORY;

    // new entry, better intialize
    if (!pref->key) {

        // initialize the pref entry
        pref->flags = type;
        pref->key = ArenaStrDup(key, &gPrefNameArena);
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
        NS_WARNING(nsPrintfCString(192, "Trying to set pref %s to with the wrong type!", key).get());
        return NS_ERROR_UNEXPECTED;
    }

    PRBool valueChanged = PR_FALSE;
    if (set_default)
    {
        if (!PREF_IS_LOCKED(pref))
        {       /* ?? change of semantics? */
            if (pref_ValueChanged(pref->defaultPref, value, type))
            {
                pref_SetValue(&pref->defaultPref, value, type);
                if (!PREF_HAS_USER_VALUE(pref))
                    valueChanged = PR_TRUE;
            }
        }
    }
    else
    {
        /* If new value is same as the default value, then un-set the user value.
           Otherwise, set the user value only if it has changed */
        if ( !pref_ValueChanged(pref->defaultPref, value, type) )
        {
            if (PREF_HAS_USER_VALUE(pref))
            {
                pref->flags &= ~PREF_USERSET;
                if (!PREF_IS_LOCKED(pref))
                    valueChanged = PR_TRUE;
            }
        }
        else if ( !PREF_HAS_USER_VALUE(pref) ||
                   pref_ValueChanged(pref->userPref, value, type) )
        {
            pref_SetValue(&pref->userPref, value, type);
            pref->flags |= PREF_USERSET;
            if (!PREF_IS_LOCKED(pref))
                valueChanged = PR_TRUE;
        }
    }

    nsresult rv = NS_OK;
    if (valueChanged) {
        gDirty = PR_TRUE;

        if (gCallbacksEnabled) {
            nsresult rv2 = pref_DoCallback(key);
            if (NS_FAILED(rv2))
                rv = rv2;
        }
#ifdef MOZ_PROFILESHARING
        if (gSharedPrefHandler)
            gSharedPrefHandler->OnPrefChanged(set_default, pref, value);
#endif
    }
    return rv;
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
nsresult
PREF_UnregisterCallback(const char *pref_node,
                         PrefChangedFunc callback,
                         void * instance_data)
{
    nsresult rv = NS_ERROR_FAILURE;
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
            rv = NS_OK;
        }
        else
        {
            prev_node = node;
            node = node->next;
        }
    }
    return rv;
}

static nsresult pref_DoCallback(const char* changed_pref)
{
    nsresult rv = NS_OK;
    struct CallbackNode* node;
    for (node = gCallbacks; node != NULL; node = node->next)
    {
        if ( PL_strncmp(changed_pref, node->domain, PL_strlen(node->domain)) == 0 )
        {
            nsresult rv2 = (*node->func) (changed_pref, node->data);
            if (NS_FAILED(rv2))
                rv = rv2;
        }
    }
    return rv;
}

void PREF_ReaderCallback(void       *closure,
                         const char *pref,
                         PrefValue   value,
                         PrefType    type,
                         PRBool      isDefault)
{
    pref_HashPref(pref, value, type, isDefault);
}
