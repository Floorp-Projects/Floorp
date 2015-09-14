/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <vector>

#include "base/basictypes.h"

#include "prefapi.h"
#include "prefapi_private_data.h"
#include "prefread.h"
#include "MainThreadUtils.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

#ifdef _WIN32
  #include "windows.h"
#endif /* _WIN32 */

#include "plstr.h"
#include "pldhash.h"
#include "plbase64.h"
#include "mozilla/Logging.h"
#include "prprf.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/PContent.h"
#include "nsQuickSort.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "prlink.h"

using namespace mozilla;

static void
clearPrefEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    PrefHashEntry *pref = static_cast<PrefHashEntry *>(entry);
    if (pref->flags & PREF_STRING)
    {
        if (pref->defaultPref.stringVal)
            PL_strfree(pref->defaultPref.stringVal);
        if (pref->userPref.stringVal)
            PL_strfree(pref->userPref.stringVal);
    }
    // don't need to free this as it's allocated in memory owned by
    // gPrefNameArena
    pref->key = nullptr;
    memset(entry, 0, table->EntrySize());
}

static bool
matchPrefEntry(PLDHashTable*, const PLDHashEntryHdr* entry,
               const void* key)
{
    const PrefHashEntry *prefEntry =
        static_cast<const PrefHashEntry*>(entry);

    if (prefEntry->key == key) return true;

    if (!prefEntry->key || !key) return false;

    const char *otherKey = reinterpret_cast<const char*>(key);
    return (strcmp(prefEntry->key, otherKey) == 0);
}

PLDHashTable*       gHashTable;
static PLArenaPool  gPrefNameArena;
bool                gDirty = false;

static struct CallbackNode* gCallbacks = nullptr;
static bool         gIsAnyPrefLocked = false;
// These are only used during the call to pref_DoCallback
static bool         gCallbacksInProgress = false;
static bool         gShouldCleanupDeadNodes = false;


static PLDHashTableOps     pref_HashTableOps = {
    PLDHashTable::HashStringKey,
    matchPrefEntry,
    PLDHashTable::MoveEntryStub,
    clearPrefEntry,
    nullptr,
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
    uint32_t len = strlen(str);
    PL_ARENA_ALLOCATE(mem, aArena, len+1);
    if (mem)
        memcpy(mem, str, len+1);
    return static_cast<char*>(mem);
}

/*---------------------------------------------------------------------------*/

#define PREF_IS_LOCKED(pref)            ((pref)->flags & PREF_LOCKED)
#define PREF_HAS_DEFAULT_VALUE(pref)    ((pref)->flags & PREF_HAS_DEFAULT)
#define PREF_HAS_USER_VALUE(pref)       ((pref)->flags & PREF_USERSET)
#define PREF_TYPE(pref)                 (PrefType)((pref)->flags & PREF_VALUETYPE_MASK)

static bool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type);

/* -- Privates */
struct CallbackNode {
    char*                   domain;
    // If someone attempts to remove the node from the callback list while
    // pref_DoCallback is running, |func| is set to nullptr. Such nodes will
    // be removed at the end of pref_DoCallback.
    PrefChangedFunc         func;
    void*                   data;
    struct CallbackNode*    next;
};

/* -- Prototypes */
static nsresult pref_DoCallback(const char* changed_pref);

enum {
    kPrefSetDefault = 1,
    kPrefForceSet = 2,
    kPrefStickyDefault = 4,
};
static nsresult pref_HashPref(const char *key, PrefValue value, PrefType type, uint32_t flags);

#define PREF_HASHTABLE_INITIAL_LENGTH   1024

void PREF_Init()
{
    if (!gHashTable) {
        gHashTable = new PLDHashTable(&pref_HashTableOps,
                                      sizeof(PrefHashEntry),
                                      PREF_HASHTABLE_INITIAL_LENGTH);

        PL_INIT_ARENA_POOL(&gPrefNameArena, "PrefNameArena",
                           PREFNAME_ARENA_SIZE);
    }
}

/* Frees the callback list. */
void PREF_Cleanup()
{
    NS_ASSERTION(!gCallbacksInProgress,
        "PREF_Cleanup was called while gCallbacksInProgress is true!");
    struct CallbackNode* node = gCallbacks;
    struct CallbackNode* next_node;

    while (node)
    {
        next_node = node->next;
        PL_strfree(node->domain);
        free(node);
        node = next_node;
    }
    gCallbacks = nullptr;

    PREF_CleanupPrefs();
}

/* Frees up all the objects except the callback list. */
void PREF_CleanupPrefs()
{
    if (gHashTable) {
        delete gHashTable;
        gHashTable = nullptr;
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

    if (original == nullptr)
        return;

    /* Paranoid worst case all slashes will free quickly */
    for  (p=original; *p; ++p)
    {
        switch (*p)
        {
            case '\n':
                aResult.AppendLiteral("\\n");
                break;

            case '\r':
                aResult.AppendLiteral("\\r");
                break;

            case '\\':
                aResult.AppendLiteral("\\\\");
                break;

            case '\"':
                aResult.AppendLiteral("\\\"");
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
PREF_SetCharPref(const char *pref_name, const char *value, bool set_default)
{
    if ((uint32_t)strlen(value) > MAX_PREF_LENGTH) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    PrefValue pref;
    pref.stringVal = (char*)value;

    return pref_HashPref(pref_name, pref, PREF_STRING, set_default ? kPrefSetDefault : 0);
}

nsresult
PREF_SetIntPref(const char *pref_name, int32_t value, bool set_default)
{
    PrefValue pref;
    pref.intVal = value;

    return pref_HashPref(pref_name, pref, PREF_INT, set_default ? kPrefSetDefault : 0);
}

nsresult
PREF_SetBoolPref(const char *pref_name, bool value, bool set_default)
{
    PrefValue pref;
    pref.boolVal = value;

    return pref_HashPref(pref_name, pref, PREF_BOOL, set_default ? kPrefSetDefault : 0);
}

enum WhichValue { DEFAULT_VALUE, USER_VALUE };
static nsresult
SetPrefValue(const char* aPrefName, const dom::PrefValue& aValue,
             WhichValue aWhich)
{
    bool setDefault = (aWhich == DEFAULT_VALUE);
    switch (aValue.type()) {
    case dom::PrefValue::TnsCString:
        return PREF_SetCharPref(aPrefName, aValue.get_nsCString().get(),
                                setDefault);
    case dom::PrefValue::Tint32_t:
        return PREF_SetIntPref(aPrefName, aValue.get_int32_t(),
                               setDefault);
    case dom::PrefValue::Tbool:
        return PREF_SetBoolPref(aPrefName, aValue.get_bool(),
                                setDefault);
    default:
        MOZ_CRASH();
    }
}

nsresult
pref_SetPref(const dom::PrefSetting& aPref)
{
    const char* prefName = aPref.name().get();
    const dom::MaybePrefValue& defaultValue = aPref.defaultValue();
    const dom::MaybePrefValue& userValue = aPref.userValue();

    nsresult rv;
    if (defaultValue.type() == dom::MaybePrefValue::TPrefValue) {
        rv = SetPrefValue(prefName, defaultValue.get_PrefValue(), DEFAULT_VALUE);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    if (userValue.type() == dom::MaybePrefValue::TPrefValue) {
        rv = SetPrefValue(prefName, userValue.get_PrefValue(), USER_VALUE);
    } else {
        rv = PREF_ClearUserPref(prefName);
    }

    // NB: we should never try to clear a default value, that doesn't
    // make sense

    return rv;
}

void
pref_savePrefs(PLDHashTable* aTable, char** aPrefArray)
{
    int32_t j = 0;
    for (auto iter = aTable->Iter(); !iter.Done(); iter.Next()) {
        auto pref = static_cast<PrefHashEntry*>(iter.Get());

        nsAutoCString prefValue;
        nsAutoCString prefPrefix;
        prefPrefix.AssignLiteral("user_pref(\"");

        // where we're getting our pref from
        PrefValue* sourcePref;

        if (PREF_HAS_USER_VALUE(pref) &&
            (pref_ValueChanged(pref->defaultPref,
                               pref->userPref,
                               (PrefType) PREF_TYPE(pref)) ||
             !(pref->flags & PREF_HAS_DEFAULT) ||
             pref->flags & PREF_STICKY_DEFAULT)) {
            sourcePref = &pref->userPref;
        } else {
            // do not save default prefs that haven't changed
            continue;
        }

        // strings are in quotes!
        if (pref->flags & PREF_STRING) {
            prefValue = '\"';
            str_escape(sourcePref->stringVal, prefValue);
            prefValue += '\"';

        } else if (pref->flags & PREF_INT) {
            prefValue.AppendInt(sourcePref->intVal);

        } else if (pref->flags & PREF_BOOL) {
            prefValue = (sourcePref->boolVal) ? "true" : "false";
        }

        nsAutoCString prefName;
        str_escape(pref->key, prefName);

        aPrefArray[j++] = ToNewCString(prefPrefix +
                                       prefName +
                                       NS_LITERAL_CSTRING("\", ") +
                                       prefValue +
                                       NS_LITERAL_CSTRING(");"));
    }
}

static void
GetPrefValueFromEntry(PrefHashEntry *aHashEntry, dom::PrefSetting* aPref,
                      WhichValue aWhich)
{
    PrefValue* value;
    dom::PrefValue* settingValue;
    if (aWhich == USER_VALUE) {
        value = &aHashEntry->userPref;
        aPref->userValue() = dom::PrefValue();
        settingValue = &aPref->userValue().get_PrefValue();
    } else {
        value = &aHashEntry->defaultPref;
        aPref->defaultValue() = dom::PrefValue();
        settingValue = &aPref->defaultValue().get_PrefValue();
    }

    switch (aHashEntry->flags & PREF_VALUETYPE_MASK) {
    case PREF_STRING:
        *settingValue = nsDependentCString(value->stringVal);
        return;
    case PREF_INT:
        *settingValue = value->intVal;
        return;
    case PREF_BOOL:
        *settingValue = !!value->boolVal;
        return;
    default:
        MOZ_CRASH();
    }
}

void
pref_GetPrefFromEntry(PrefHashEntry *aHashEntry, dom::PrefSetting* aPref)
{
    aPref->name() = aHashEntry->key;
    if (PREF_HAS_DEFAULT_VALUE(aHashEntry)) {
        GetPrefValueFromEntry(aHashEntry, aPref, DEFAULT_VALUE);
    } else {
        aPref->defaultValue() = null_t();
    }
    if (PREF_HAS_USER_VALUE(aHashEntry)) {
        GetPrefValueFromEntry(aHashEntry, aPref, USER_VALUE);
    } else {
        aPref->userValue() = null_t();
    }

    MOZ_ASSERT(aPref->defaultValue().type() == dom::MaybePrefValue::Tnull_t ||
               aPref->userValue().type() == dom::MaybePrefValue::Tnull_t ||
               (aPref->defaultValue().get_PrefValue().type() ==
                aPref->userValue().get_PrefValue().type()));
}


int
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

bool PREF_HasUserPref(const char *pref_name)
{
    if (!gHashTable)
        return false;

    PrefHashEntry *pref = pref_HashTableLookup(pref_name);
    if (!pref) return false;

    /* convert PREF_HAS_USER_VALUE to bool */
    return (PREF_HAS_USER_VALUE(pref) != 0);

}

nsresult
PREF_CopyCharPref(const char *pref_name, char ** return_buffer, bool get_default)
{
    if (!gHashTable)
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
            *return_buffer = NS_strdup(stringVal);
            rv = NS_OK;
        }
    }
    return rv;
}

nsresult PREF_GetIntPref(const char *pref_name,int32_t * return_int, bool get_default)
{
    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    PrefHashEntry* pref = pref_HashTableLookup(pref_name);
    if (pref && (pref->flags & PREF_INT))
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
        {
            int32_t tempInt = pref->defaultPref.intVal;
            /* check to see if we even had a default */
            if (!(pref->flags & PREF_HAS_DEFAULT))
                return NS_ERROR_UNEXPECTED;
            *return_int = tempInt;
        }
        else
            *return_int = pref->userPref.intVal;
        rv = NS_OK;
    }
    return rv;
}

nsresult PREF_GetBoolPref(const char *pref_name, bool * return_value, bool get_default)
{
    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    PrefHashEntry* pref = pref_HashTableLookup(pref_name);
    //NS_ASSERTION(pref, pref_name);
    if (pref && (pref->flags & PREF_BOOL))
    {
        if (get_default || PREF_IS_LOCKED(pref) || !PREF_HAS_USER_VALUE(pref))
        {
            bool tempBool = pref->defaultPref.boolVal;
            /* check to see if we even had a default */
            if (pref->flags & PREF_HAS_DEFAULT) {
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

nsresult
PREF_DeleteBranch(const char *branch_name)
{
#ifndef MOZ_B2G
    MOZ_ASSERT(NS_IsMainThread());
#endif

    int len = (int)strlen(branch_name);

    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    /* The following check insures that if the branch name already has a "."
     * at the end, we don't end up with a "..". This fixes an incompatibility
     * between nsIPref, which needs the period added, and nsIPrefBranch which
     * does not. When nsIPref goes away this function should be fixed to
     * never add the period at all.
     */
    nsAutoCString branch_dot(branch_name);
    if ((len > 1) && branch_name[len - 1] != '.')
        branch_dot += '.';

    /* Delete a branch. Used for deleting mime types */
    const char *to_delete = branch_dot.get();
    MOZ_ASSERT(to_delete);
    len = strlen(to_delete);
    for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<PrefHashEntry*>(iter.Get());

        /* note if we're deleting "ldap" then we want to delete "ldap.xxx"
            and "ldap" (if such a leaf node exists) but not "ldap_1.xxx" */
        if (PL_strncmp(entry->key, to_delete, (uint32_t) len) == 0 ||
            (len-1 == (int)strlen(entry->key) &&
             PL_strncmp(entry->key, to_delete, (uint32_t)(len-1)) == 0)) {
            iter.Remove();
        }
    }

    gDirty = true;
    return NS_OK;
}


nsresult
PREF_ClearUserPref(const char *pref_name)
{
    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    PrefHashEntry* pref = pref_HashTableLookup(pref_name);
    if (pref && PREF_HAS_USER_VALUE(pref))
    {
        pref->flags &= ~PREF_USERSET;

        if (!(pref->flags & PREF_HAS_DEFAULT)) {
            gHashTable->RemoveEntry(pref);
        }

        pref_DoCallback(pref_name);
        gDirty = true;
    }
    return NS_OK;
}

nsresult
PREF_ClearAllUserPrefs()
{
#ifndef MOZ_B2G
    MOZ_ASSERT(NS_IsMainThread());
#endif

    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    std::vector<std::string> prefStrings;
    for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
        auto pref = static_cast<PrefHashEntry*>(iter.Get());

        if (PREF_HAS_USER_VALUE(pref)) {
            prefStrings.push_back(std::string(pref->key));

            pref->flags &= ~PREF_USERSET;
            if (!(pref->flags & PREF_HAS_DEFAULT)) {
                iter.Remove();
            }
        }
    }

    for (std::string& prefString : prefStrings) {
        pref_DoCallback(prefString.c_str());
    }

    gDirty = true;
    return NS_OK;
}

nsresult PREF_LockPref(const char *key, bool lockit)
{
    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    PrefHashEntry* pref = pref_HashTableLookup(key);
    if (!pref)
        return NS_ERROR_UNEXPECTED;

    if (lockit) {
        if (!PREF_IS_LOCKED(pref))
        {
            pref->flags |= PREF_LOCKED;
            gIsAnyPrefLocked = true;
            pref_DoCallback(key);
        }
    }
    else
    {
        if (PREF_IS_LOCKED(pref))
        {
            pref->flags &= ~PREF_LOCKED;
            pref_DoCallback(key);
        }
    }
    return NS_OK;
}

/*
 * Hash table functions
 */
static bool pref_ValueChanged(PrefValue oldValue, PrefValue newValue, PrefType type)
{
    bool changed = true;
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

/*
 * Overwrite the type and value of an existing preference. Caller must
 * ensure that they are not changing the type of a preference that has
 * a default value.
 */
static void pref_SetValue(PrefValue* existingValue, uint16_t *existingFlags,
                          PrefValue newValue, PrefType newType)
{
    if ((*existingFlags & PREF_STRING) && existingValue->stringVal) {
        PL_strfree(existingValue->stringVal);
    }
    *existingFlags = (*existingFlags & ~PREF_VALUETYPE_MASK) | newType;
    if (newType & PREF_STRING) {
        PR_ASSERT(newValue.stringVal);
        existingValue->stringVal = newValue.stringVal ? PL_strdup(newValue.stringVal) : nullptr;
    }
    else {
        *existingValue = newValue;
    }
}

PrefHashEntry* pref_HashTableLookup(const char *key)
{
#ifndef MOZ_B2G
    MOZ_ASSERT(NS_IsMainThread());
#endif

    return static_cast<PrefHashEntry*>(gHashTable->Search(key));
}

nsresult pref_HashPref(const char *key, PrefValue value, PrefType type, uint32_t flags)
{
#ifndef MOZ_B2G
    MOZ_ASSERT(NS_IsMainThread());
#endif

    if (!gHashTable)
        return NS_ERROR_OUT_OF_MEMORY;

    auto pref = static_cast<PrefHashEntry*>(gHashTable->Add(key, fallible));
    if (!pref)
        return NS_ERROR_OUT_OF_MEMORY;

    // new entry, better initialize
    if (!pref->key) {

        // initialize the pref entry
        pref->flags = type;
        pref->key = ArenaStrDup(key, &gPrefNameArena);
        memset(&pref->defaultPref, 0, sizeof(pref->defaultPref));
        memset(&pref->userPref, 0, sizeof(pref->userPref));
    }
    else if ((pref->flags & PREF_HAS_DEFAULT) && PREF_TYPE(pref) != type)
    {
        NS_WARNING(nsPrintfCString("Trying to overwrite value of default pref %s with the wrong type!", key).get());
        return NS_ERROR_UNEXPECTED;
    }

    bool valueChanged = false;
    if (flags & kPrefSetDefault)
    {
        if (!PREF_IS_LOCKED(pref))
        {       /* ?? change of semantics? */
            if (pref_ValueChanged(pref->defaultPref, value, type) ||
                !(pref->flags & PREF_HAS_DEFAULT))
            {
                pref_SetValue(&pref->defaultPref, &pref->flags, value, type);
                pref->flags |= PREF_HAS_DEFAULT;
                if (flags & kPrefStickyDefault)
                    pref->flags |= PREF_STICKY_DEFAULT;
                if (!PREF_HAS_USER_VALUE(pref))
                    valueChanged = true;
            }
            // What if we change the default to be the same as the user value?
            // Should we clear the user value?
        }
    }
    else
    {
        /* If new value is same as the default value and it's not a "sticky"
           pref, then un-set the user value.
           Otherwise, set the user value only if it has changed */
        if ((pref->flags & PREF_HAS_DEFAULT) &&
            !(pref->flags & PREF_STICKY_DEFAULT) &&
            !pref_ValueChanged(pref->defaultPref, value, type) &&
            !(flags & kPrefForceSet))
        {
            if (PREF_HAS_USER_VALUE(pref))
            {
                /* XXX should we free a user-set string value if there is one? */
                pref->flags &= ~PREF_USERSET;
                if (!PREF_IS_LOCKED(pref)) {
                    gDirty = true;
                    valueChanged = true;
                }
            }
        }
        else if (!PREF_HAS_USER_VALUE(pref) ||
                 PREF_TYPE(pref) != type ||
                 pref_ValueChanged(pref->userPref, value, type) )
        {
            pref_SetValue(&pref->userPref, &pref->flags, value, type);
            pref->flags |= PREF_USERSET;
            if (!PREF_IS_LOCKED(pref)) {
                gDirty = true;
                valueChanged = true;
            }
        }
    }

    if (valueChanged) {
        return pref_DoCallback(key);
    }
    return NS_OK;
}

size_t
pref_SizeOfPrivateData(MallocSizeOf aMallocSizeOf)
{
    size_t n = PL_SizeOfArenaPoolExcludingPool(&gPrefNameArena, aMallocSizeOf);
    for (struct CallbackNode* node = gCallbacks; node; node = node->next) {
        n += aMallocSizeOf(node);
        n += aMallocSizeOf(node->domain);
    }
    return n;
}

PrefType
PREF_GetPrefType(const char *pref_name)
{
    if (gHashTable) {
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

bool
PREF_PrefIsLocked(const char *pref_name)
{
    bool result = false;
    if (gIsAnyPrefLocked && gHashTable) {
        PrefHashEntry* pref = pref_HashTableLookup(pref_name);
        if (pref && PREF_IS_LOCKED(pref))
            result = true;
    }

    return result;
}

/* Adds a node to the beginning of the callback list. */
void
PREF_RegisterCallback(const char *pref_node,
                       PrefChangedFunc callback,
                       void * instance_data)
{
    NS_PRECONDITION(pref_node, "pref_node must not be nullptr");
    NS_PRECONDITION(callback, "callback must not be nullptr");

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

/* Removes |node| from gCallbacks list.
   Returns the node after the deleted one. */
struct CallbackNode*
pref_RemoveCallbackNode(struct CallbackNode* node,
                        struct CallbackNode* prev_node)
{
    NS_PRECONDITION(!prev_node || prev_node->next == node, "invalid params");
    NS_PRECONDITION(prev_node || gCallbacks == node, "invalid params");

    NS_ASSERTION(!gCallbacksInProgress,
        "modifying the callback list while gCallbacksInProgress is true");

    struct CallbackNode* next_node = node->next;
    if (prev_node)
        prev_node->next = next_node;
    else
        gCallbacks = next_node;
    PL_strfree(node->domain);
    free(node);
    return next_node;
}

/* Deletes a node from the callback list or marks it for deletion. */
nsresult
PREF_UnregisterCallback(const char *pref_node,
                         PrefChangedFunc callback,
                         void * instance_data)
{
    nsresult rv = NS_ERROR_FAILURE;
    struct CallbackNode* node = gCallbacks;
    struct CallbackNode* prev_node = nullptr;

    while (node != nullptr)
    {
        if ( node->func == callback &&
             node->data == instance_data &&
             strcmp(node->domain, pref_node) == 0)
        {
            if (gCallbacksInProgress)
            {
                // postpone the node removal until after
                // gCallbacks enumeration is finished.
                node->func = nullptr;
                gShouldCleanupDeadNodes = true;
                prev_node = node;
                node = node->next;
            }
            else
            {
                node = pref_RemoveCallbackNode(node, prev_node);
            }
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

    bool reentered = gCallbacksInProgress;
    gCallbacksInProgress = true;
    // Nodes must not be deleted while gCallbacksInProgress is true.
    // Nodes that need to be deleted are marked for deletion by nulling
    // out the |func| pointer. We release them at the end of this function
    // if we haven't reentered.

    for (node = gCallbacks; node != nullptr; node = node->next)
    {
        if ( node->func &&
             PL_strncmp(changed_pref,
                        node->domain,
                        strlen(node->domain)) == 0 )
        {
            (*node->func) (changed_pref, node->data);
        }
    }

    gCallbacksInProgress = reentered;

    if (gShouldCleanupDeadNodes && !gCallbacksInProgress)
    {
        struct CallbackNode* prev_node = nullptr;
        node = gCallbacks;

        while (node != nullptr)
        {
            if (!node->func)
            {
                node = pref_RemoveCallbackNode(node, prev_node);
            }
            else
            {
                prev_node = node;
                node = node->next;
            }
        }
        gShouldCleanupDeadNodes = false;
    }

    return rv;
}

void PREF_ReaderCallback(void       *closure,
                         const char *pref,
                         PrefValue   value,
                         PrefType    type,
                         bool        isDefault,
                         bool        isStickyDefault)
{
    uint32_t flags = isDefault ? kPrefSetDefault : kPrefForceSet;
    if (isDefault && isStickyDefault) {
        flags |= kPrefStickyDefault;
    }
    pref_HashPref(pref, value, type, flags);
}
