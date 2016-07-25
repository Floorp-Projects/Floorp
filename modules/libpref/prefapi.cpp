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
#include "PLDHashTable.h"
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
    if (pref->prefFlags.IsTypeString())
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
matchPrefEntry(const PLDHashEntryHdr* entry, const void* key)
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

static PrefsDirtyFunc gDirtyCallback = nullptr;

inline void MakeDirtyCallback()
{
    // Right now the callback function is always set, so we don't need
    // to complicate the code to cover the scenario where we set the callback
    // after we've already tried to make it dirty.  If this assert triggers
    // we will add that code.
    MOZ_ASSERT(gDirtyCallback);
    if (gDirtyCallback) {
        gDirtyCallback();
    }
}

void PREF_SetDirtyCallback(PrefsDirtyFunc aFunc)
{
    gDirtyCallback = aFunc;
}

/*---------------------------------------------------------------------------*/

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

    return pref_HashPref(pref_name, pref, PrefType::String, set_default ? kPrefSetDefault : 0);
}

nsresult
PREF_SetIntPref(const char *pref_name, int32_t value, bool set_default)
{
    PrefValue pref;
    pref.intVal = value;

    return pref_HashPref(pref_name, pref, PrefType::Int, set_default ? kPrefSetDefault : 0);
}

nsresult
PREF_SetBoolPref(const char *pref_name, bool value, bool set_default)
{
    PrefValue pref;
    pref.boolVal = value;

    return pref_HashPref(pref_name, pref, PrefType::Bool, set_default ? kPrefSetDefault : 0);
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

UniquePtr<char*[]>
pref_savePrefs(PLDHashTable* aTable)
{
    auto savedPrefs = MakeUnique<char*[]>(aTable->EntryCount());
    memset(savedPrefs.get(), 0, aTable->EntryCount() * sizeof(char*));

    int32_t j = 0;
    for (auto iter = aTable->Iter(); !iter.Done(); iter.Next()) {
        auto pref = static_cast<PrefHashEntry*>(iter.Get());

        nsAutoCString prefValue;
        nsAutoCString prefPrefix;
        prefPrefix.AssignLiteral("user_pref(\"");

        // where we're getting our pref from
        PrefValue* sourcePref;

        if (pref->prefFlags.HasUserValue() &&
            (pref_ValueChanged(pref->defaultPref,
                               pref->userPref,
                               pref->prefFlags.GetPrefType()) ||
             !(pref->prefFlags.HasDefault()) ||
             pref->prefFlags.HasStickyDefault())) {
            sourcePref = &pref->userPref;
        } else {
            // do not save default prefs that haven't changed
            continue;
        }

        // strings are in quotes!
        if (pref->prefFlags.IsTypeString()) {
            prefValue = '\"';
            str_escape(sourcePref->stringVal, prefValue);
            prefValue += '\"';

        } else if (pref->prefFlags.IsTypeInt()) {
            prefValue.AppendInt(sourcePref->intVal);

        } else if (pref->prefFlags.IsTypeBool()) {
            prefValue = (sourcePref->boolVal) ? "true" : "false";
        }

        nsAutoCString prefName;
        str_escape(pref->key, prefName);

        savedPrefs[j++] = ToNewCString(prefPrefix +
                                       prefName +
                                       NS_LITERAL_CSTRING("\", ") +
                                       prefValue +
                                       NS_LITERAL_CSTRING(");"));
    }

    return savedPrefs;
}

bool
pref_EntryHasAdvisablySizedValues(PrefHashEntry* aHashEntry)
{
    if (aHashEntry->prefFlags.GetPrefType() != PrefType::String) {
        return true;
    }

    char* stringVal;
    if (aHashEntry->prefFlags.HasDefault()) {
        stringVal = aHashEntry->defaultPref.stringVal;
        if (strlen(stringVal) > MAX_ADVISABLE_PREF_LENGTH) {
            return false;
        }
    }

    if (aHashEntry->prefFlags.HasUserValue()) {
        stringVal = aHashEntry->userPref.stringVal;
        if (strlen(stringVal) > MAX_ADVISABLE_PREF_LENGTH) {
            return false;
        }
    }

    return true;
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

    switch (aHashEntry->prefFlags.GetPrefType()) {
      case PrefType::String:
        *settingValue = nsDependentCString(value->stringVal);
        return;
      case PrefType::Int:
        *settingValue = value->intVal;
        return;
      case PrefType::Bool:
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
    if (aHashEntry->prefFlags.HasDefault()) {
        GetPrefValueFromEntry(aHashEntry, aPref, DEFAULT_VALUE);
    } else {
        aPref->defaultValue() = null_t();
    }
    if (aHashEntry->prefFlags.HasUserValue()) {
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
    return pref && pref->prefFlags.HasUserValue();
}

nsresult
PREF_CopyCharPref(const char *pref_name, char ** return_buffer, bool get_default)
{
    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_ERROR_UNEXPECTED;
    char* stringVal;
    PrefHashEntry* pref = pref_HashTableLookup(pref_name);

    if (pref && (pref->prefFlags.IsTypeString())) {
        if (get_default || pref->prefFlags.IsLocked() || !pref->prefFlags.HasUserValue()) {
            stringVal = pref->defaultPref.stringVal;
        } else {
            stringVal = pref->userPref.stringVal;
        }

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
    if (pref && (pref->prefFlags.IsTypeInt())) {
        if (get_default || pref->prefFlags.IsLocked() || !pref->prefFlags.HasUserValue()) {
            int32_t tempInt = pref->defaultPref.intVal;
            /* check to see if we even had a default */
            if (!pref->prefFlags.HasDefault()) {
                return NS_ERROR_UNEXPECTED;
            }
            *return_int = tempInt;
        } else {
            *return_int = pref->userPref.intVal;
        }
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
    if (pref && (pref->prefFlags.IsTypeBool())) {
        if (get_default || pref->prefFlags.IsLocked() || !pref->prefFlags.HasUserValue()) {
            bool tempBool = pref->defaultPref.boolVal;
            /* check to see if we even had a default */
            if (pref->prefFlags.HasDefault()) {
                *return_value = tempBool;
                rv = NS_OK;
            }
        } else {
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

    MakeDirtyCallback();
    return NS_OK;
}


nsresult
PREF_ClearUserPref(const char *pref_name)
{
    if (!gHashTable)
        return NS_ERROR_NOT_INITIALIZED;

    PrefHashEntry* pref = pref_HashTableLookup(pref_name);
    if (pref && pref->prefFlags.HasUserValue()) {
        pref->prefFlags.SetHasUserValue(false);

        if (!pref->prefFlags.HasDefault()) {
            gHashTable->RemoveEntry(pref);
        }

        pref_DoCallback(pref_name);
        MakeDirtyCallback();
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

        if (pref->prefFlags.HasUserValue()) {
            prefStrings.push_back(std::string(pref->key));

            pref->prefFlags.SetHasUserValue(false);
            if (!pref->prefFlags.HasDefault()) {
                iter.Remove();
            }
        }
    }

    for (std::string& prefString : prefStrings) {
        pref_DoCallback(prefString.c_str());
    }

    MakeDirtyCallback();
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
        if (!pref->prefFlags.IsLocked()) {
            pref->prefFlags.SetLocked(true);
            gIsAnyPrefLocked = true;
            pref_DoCallback(key);
        }
    } else {
        if (pref->prefFlags.IsLocked()) {
            pref->prefFlags.SetLocked(false);
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
    switch(type) {
      case PrefType::String:
        if (oldValue.stringVal && newValue.stringVal) {
            changed = (strcmp(oldValue.stringVal, newValue.stringVal) != 0);
        }
        break;
      case PrefType::Int:
        changed = oldValue.intVal != newValue.intVal;
        break;
      case PrefType::Bool:
        changed = oldValue.boolVal != newValue.boolVal;
        break;
      case PrefType::Invalid:
      default:
        changed = false;
        break;
    }
    return changed;
}

/*
 * Overwrite the type and value of an existing preference. Caller must
 * ensure that they are not changing the type of a preference that has
 * a default value.
 */
static PrefTypeFlags pref_SetValue(PrefValue* existingValue, PrefTypeFlags flags,
                                   PrefValue newValue, PrefType newType)
{
    if (flags.IsTypeString() && existingValue->stringVal) {
        PL_strfree(existingValue->stringVal);
    }
    flags.SetPrefType(newType);
    if (flags.IsTypeString()) {
        MOZ_ASSERT(newValue.stringVal);
        existingValue->stringVal = newValue.stringVal ? PL_strdup(newValue.stringVal) : nullptr;
    }
    else {
        *existingValue = newValue;
    }
    return flags;
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
        pref->prefFlags.Reset().SetPrefType(type);
        pref->key = ArenaStrDup(key, &gPrefNameArena);
        memset(&pref->defaultPref, 0, sizeof(pref->defaultPref));
        memset(&pref->userPref, 0, sizeof(pref->userPref));
    } else if (pref->prefFlags.HasDefault() && !pref->prefFlags.IsPrefType(type)) {
        NS_WARNING(nsPrintfCString("Trying to overwrite value of default pref %s with the wrong type!", key).get());
        return NS_ERROR_UNEXPECTED;
    }

    bool valueChanged = false;
    if (flags & kPrefSetDefault) {
        if (!pref->prefFlags.IsLocked()) {
            /* ?? change of semantics? */
            if (pref_ValueChanged(pref->defaultPref, value, type) ||
                !pref->prefFlags.HasDefault()) {
                pref->prefFlags = pref_SetValue(&pref->defaultPref, pref->prefFlags, value, type).SetHasDefault(true);
                if (flags & kPrefStickyDefault) {
                    pref->prefFlags.SetHasStickyDefault(true);
                }
                if (!pref->prefFlags.HasUserValue()) {
                    valueChanged = true;
                }
            }
            // What if we change the default to be the same as the user value?
            // Should we clear the user value?
        }
    } else {
        /* If new value is same as the default value and it's not a "sticky"
           pref, then un-set the user value.
           Otherwise, set the user value only if it has changed */
        if ((pref->prefFlags.HasDefault()) &&
            !(pref->prefFlags.HasStickyDefault()) &&
            !pref_ValueChanged(pref->defaultPref, value, type) &&
            !(flags & kPrefForceSet)) {
            if (pref->prefFlags.HasUserValue()) {
                /* XXX should we free a user-set string value if there is one? */
                pref->prefFlags.SetHasUserValue(false);
                if (!pref->prefFlags.IsLocked()) {
                    MakeDirtyCallback();
                    valueChanged = true;
                }
            }
        } else if (!pref->prefFlags.HasUserValue() ||
                 !pref->prefFlags.IsPrefType(type) ||
                 pref_ValueChanged(pref->userPref, value, type) ) {
            pref->prefFlags = pref_SetValue(&pref->userPref, pref->prefFlags, value, type).SetHasUserValue(true);
            if (!pref->prefFlags.IsLocked()) {
                MakeDirtyCallback();
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
        if (pref) {
            return pref->prefFlags.GetPrefType();
        }
    }
    return PrefType::Invalid;
}

/* -- */

bool
PREF_PrefIsLocked(const char *pref_name)
{
    bool result = false;
    if (gIsAnyPrefLocked && gHashTable) {
        PrefHashEntry* pref = pref_HashTableLookup(pref_name);
        if (pref && pref->prefFlags.IsLocked()) {
            result = true;
        }
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
    uint32_t flags = 0;
    if (isDefault) {
        flags |= kPrefSetDefault;
        if (isStickyDefault) {
            flags |= kPrefStickyDefault;
        }
    } else {
        flags |= kPrefForceSet;
    }
    pref_HashPref(pref, value, type, flags);
}
