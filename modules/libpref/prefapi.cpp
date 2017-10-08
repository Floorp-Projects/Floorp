/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "MainThreadUtils.h"
#include "mozilla/ArenaAllocatorExtensions.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/dom/ContentPrefs.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/Logging.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoStyleSet.h"
#include "nsCRT.h"
#include "nsPrintfCString.h"
#include "nsQuickSort.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "plbase64.h"
#include "PLDHashTable.h"
#include "plstr.h"
#include "prefapi.h"
#include "prefapi_private_data.h"
#include "prefread.h"
#include "prlink.h"

#ifdef _WIN32
#include "windows.h"
#endif

using namespace mozilla;

static void
ClearPrefEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  auto pref = static_cast<PrefHashEntry*>(aEntry);
  if (pref->prefFlags.IsTypeString()) {
    if (pref->defaultPref.stringVal) {
      PL_strfree(pref->defaultPref.stringVal);
    }
    if (pref->userPref.stringVal) {
      PL_strfree(pref->userPref.stringVal);
    }
  }

  // Don't need to free this because it's allocated in memory owned by
  // gPrefNameArena.
  pref->key = nullptr;
  memset(aEntry, 0, aTable->EntrySize());
}

static bool
MatchPrefEntry(const PLDHashEntryHdr* aEntry, const void* aKey)
{
  auto prefEntry = static_cast<const PrefHashEntry*>(aEntry);

  if (prefEntry->key == aKey) {
    return true;
  }

  if (!prefEntry->key || !aKey) {
    return false;
  }

  auto otherKey = static_cast<const char*>(aKey);
  return (strcmp(prefEntry->key, otherKey) == 0);
}

struct CallbackNode
{
  char* mDomain;

  // If someone attempts to remove the node from the callback list while
  // pref_DoCallback is running, |func| is set to nullptr. Such nodes will
  // be removed at the end of pref_DoCallback.
  PrefChangedFunc mFunc;
  void* mData;
  CallbackNode* mNext;
};

PLDHashTable* gHashTable;

static ArenaAllocator<8192, 4> gPrefNameArena;

static CallbackNode* gFirstCallback = nullptr;
static CallbackNode* gLastPriorityNode = nullptr;

static bool gIsAnyPrefLocked = false;

// These are only used during the call to pref_DoCallback.
static bool gCallbacksInProgress = false;
static bool gShouldCleanupDeadNodes = false;

static PLDHashTableOps pref_HashTableOps = {
  PLDHashTable::HashStringKey,
  MatchPrefEntry,
  PLDHashTable::MoveEntryStub,
  ClearPrefEntry,
  nullptr,
};

// PR_ALIGN_OF_WORD is only defined on some platforms. ALIGN_OF_WORD has
// already been defined to PR_ALIGN_OF_WORD everywhere.
#ifndef PR_ALIGN_OF_WORD
#define PR_ALIGN_OF_WORD PR_ALIGN_OF_POINTER
#endif

#define WORD_ALIGN_MASK (PR_ALIGN_OF_WORD - 1)

// Sanity checking.
#if (PR_ALIGN_OF_WORD & WORD_ALIGN_MASK) != 0
#error "PR_ALIGN_OF_WORD must be a power of 2!"
#endif

static PrefsDirtyFunc gDirtyCallback = nullptr;

inline void
MakeDirtyCallback()
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

void
PREF_SetDirtyCallback(PrefsDirtyFunc aFunc)
{
  gDirtyCallback = aFunc;
}

//---------------------------------------------------------------------------

static bool
pref_ValueChanged(PrefValue aOldValue, PrefValue aNewValue, PrefType aType);

static nsresult
pref_DoCallback(const char* aChangedPref);

enum
{
  kPrefSetDefault = 1,
  kPrefForceSet = 2,
  kPrefStickyDefault = 4,
};

static nsresult
pref_HashPref(const char* aKey,
              PrefValue aValue,
              PrefType aType,
              uint32_t aFlags);

#define PREF_HASHTABLE_INITIAL_LENGTH 1024

void
PREF_Init()
{
  if (!gHashTable) {
    gHashTable = new PLDHashTable(
      &pref_HashTableOps, sizeof(PrefHashEntry), PREF_HASHTABLE_INITIAL_LENGTH);
  }
}

// Frees the callback list.
void
PREF_Cleanup()
{
  NS_ASSERTION(!gCallbacksInProgress,
               "PREF_Cleanup was called while gCallbacksInProgress is true!");

  CallbackNode* node = gFirstCallback;
  CallbackNode* next_node;

  while (node) {
    next_node = node->mNext;
    PL_strfree(node->mDomain);
    free(node);
    node = next_node;
  }
  gLastPriorityNode = gFirstCallback = nullptr;

  PREF_CleanupPrefs();
}

// Frees up all the objects except the callback list.
void
PREF_CleanupPrefs()
{
  if (gHashTable) {
    delete gHashTable;
    gHashTable = nullptr;
    gPrefNameArena.Clear();
  }
}

// Note that this appends to aResult, and does not assign!
static void
StrEscape(const char* aOriginal, nsCString& aResult)
{
  // JavaScript does not allow quotes, slashes, or line terminators inside
  // strings so we must escape them. ECMAScript defines four line terminators,
  // but we're only worrying about \r and \n here.  We currently feed our pref
  // script to the JS interpreter as Latin-1 so  we won't encounter \u2028
  // (line separator) or \u2029 (paragraph separator).
  //
  // WARNING: There are hints that we may be moving to storing prefs as utf8.
  // If we ever feed them to the JS compiler as UTF8 then we'll have to worry
  // about the multibyte sequences that would be interpreted as \u2028 and
  // \u2029.
  const char* p;

  if (aOriginal == nullptr) {
    return;
  }

  // Paranoid worst case all slashes will free quickly.
  for (p = aOriginal; *p; ++p) {
    switch (*p) {
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

//
// External calls
//

nsresult
PREF_SetCharPref(const char* aPrefName, const char* aValue, bool aSetDefault)
{
  if (strlen(aValue) > MAX_PREF_LENGTH) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PrefValue pref;
  pref.stringVal = const_cast<char*>(aValue);

  return pref_HashPref(
    aPrefName, pref, PrefType::String, aSetDefault ? kPrefSetDefault : 0);
}

nsresult
PREF_SetIntPref(const char* aPrefName, int32_t aValue, bool aSetDefault)
{
  PrefValue pref;
  pref.intVal = aValue;

  return pref_HashPref(
    aPrefName, pref, PrefType::Int, aSetDefault ? kPrefSetDefault : 0);
}

nsresult
PREF_SetBoolPref(const char* aPrefName, bool aValue, bool aSetDefault)
{
  PrefValue pref;
  pref.boolVal = aValue;

  return pref_HashPref(
    aPrefName, pref, PrefType::Bool, aSetDefault ? kPrefSetDefault : 0);
}

enum WhichValue
{
  DEFAULT_VALUE,
  USER_VALUE
};

static nsresult
SetPrefValue(const char* aPrefName,
             const dom::PrefValue& aValue,
             WhichValue aWhich)
{
  bool setDefault = (aWhich == DEFAULT_VALUE);

  switch (aValue.type()) {
    case dom::PrefValue::TnsCString:
      return PREF_SetCharPref(
        aPrefName, aValue.get_nsCString().get(), setDefault);

    case dom::PrefValue::Tint32_t:
      return PREF_SetIntPref(aPrefName, aValue.get_int32_t(), setDefault);

    case dom::PrefValue::Tbool:
      return PREF_SetBoolPref(aPrefName, aValue.get_bool(), setDefault);

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

PrefSaveData
pref_savePrefs(PLDHashTable* aTable)
{
  PrefSaveData savedPrefs(aTable->EntryCount());

  for (auto iter = aTable->Iter(); !iter.Done(); iter.Next()) {
    auto pref = static_cast<PrefHashEntry*>(iter.Get());

    nsAutoCString prefValue;
    nsAutoCString prefPrefix;
    prefPrefix.AssignLiteral("user_pref(\"");

    // where we're getting our pref from
    PrefValue* sourcePref;

    if (pref->prefFlags.HasUserValue() &&
        (pref_ValueChanged(
           pref->defaultPref, pref->userPref, pref->prefFlags.GetPrefType()) ||
         !pref->prefFlags.HasDefault() || pref->prefFlags.HasStickyDefault())) {
      sourcePref = &pref->userPref;
    } else {
      // do not save default prefs that haven't changed
      continue;
    }

    // strings are in quotes!
    if (pref->prefFlags.IsTypeString()) {
      prefValue = '\"';
      StrEscape(sourcePref->stringVal, prefValue);
      prefValue += '\"';

    } else if (pref->prefFlags.IsTypeInt()) {
      prefValue.AppendInt(sourcePref->intVal);

    } else if (pref->prefFlags.IsTypeBool()) {
      prefValue = (sourcePref->boolVal) ? "true" : "false";
    }

    nsAutoCString prefName;
    StrEscape(pref->key, prefName);

    savedPrefs.AppendElement()->reset(
      ToNewCString(prefPrefix + prefName + NS_LITERAL_CSTRING("\", ") +
                   prefValue + NS_LITERAL_CSTRING(");")));
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
GetPrefValueFromEntry(PrefHashEntry* aHashEntry,
                      dom::PrefSetting* aPref,
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
pref_GetPrefFromEntry(PrefHashEntry* aHashEntry, dom::PrefSetting* aPref)
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

bool
PREF_HasUserPref(const char* aPrefName)
{
  if (!gHashTable) {
    return false;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  return pref && pref->prefFlags.HasUserValue();
}

nsresult
PREF_CopyCharPref(const char* aPrefName, char** aValueOut, bool aGetDefault)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = NS_ERROR_UNEXPECTED;
  char* stringVal;
  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);

  if (pref && pref->prefFlags.IsTypeString()) {
    if (aGetDefault || pref->prefFlags.IsLocked() ||
        !pref->prefFlags.HasUserValue()) {
      stringVal = pref->defaultPref.stringVal;
    } else {
      stringVal = pref->userPref.stringVal;
    }

    if (stringVal) {
      *aValueOut = NS_strdup(stringVal);
      rv = NS_OK;
    }
  }

  return rv;
}

nsresult
PREF_GetIntPref(const char* aPrefName, int32_t* aValueOut, bool aGetDefault)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = NS_ERROR_UNEXPECTED;
  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  if (pref && pref->prefFlags.IsTypeInt()) {
    if (aGetDefault || pref->prefFlags.IsLocked() ||
        !pref->prefFlags.HasUserValue()) {
      int32_t tempInt = pref->defaultPref.intVal;

      // Check to see if we even had a default.
      if (!pref->prefFlags.HasDefault()) {
        return NS_ERROR_UNEXPECTED;
      }
      *aValueOut = tempInt;
    } else {
      *aValueOut = pref->userPref.intVal;
    }
    rv = NS_OK;
  }

  return rv;
}

nsresult
PREF_GetBoolPref(const char* aPrefName, bool* aValueOut, bool aGetDefault)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = NS_ERROR_UNEXPECTED;
  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  //NS_ASSERTION(pref, aPrefName);
  if (pref && pref->prefFlags.IsTypeBool()) {
    if (aGetDefault || pref->prefFlags.IsLocked() ||
        !pref->prefFlags.HasUserValue()) {
      bool tempBool = pref->defaultPref.boolVal;

      // Check to see if we even had a default.
      if (pref->prefFlags.HasDefault()) {
        *aValueOut = tempBool;
        rv = NS_OK;
      }
    } else {
      *aValueOut = pref->userPref.boolVal;
      rv = NS_OK;
    }
  }

  return rv;
}

nsresult
PREF_DeleteBranch(const char* aBranchName)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t len = strlen(aBranchName);

  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // The following check insures that if the branch name already has a "." at
  // the end, we don't end up with a "..". This fixes an incompatibility
  // between nsIPref, which needs the period added, and nsIPrefBranch which
  // does not. When nsIPref goes away this function should be fixed to never
  // add the period at all.
  nsAutoCString branch_dot(aBranchName);
  if (len > 1 && aBranchName[len - 1] != '.') {
    branch_dot += '.';
  }

  // Delete a branch. Used for deleting mime types.
  const char* to_delete = branch_dot.get();
  MOZ_ASSERT(to_delete);
  len = strlen(to_delete);
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PrefHashEntry*>(iter.Get());

    // Note: if we're deleting "ldap" then we want to delete "ldap.xxx" and
    // "ldap" (if such a leaf node exists) but not "ldap_1.xxx".
    if (PL_strncmp(entry->key, to_delete, len) == 0 ||
        (len - 1 == strlen(entry->key) &&
         PL_strncmp(entry->key, to_delete, len - 1) == 0)) {
      iter.Remove();
    }
  }

  MakeDirtyCallback();
  return NS_OK;
}

nsresult
PREF_ClearUserPref(const char* aPrefName)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  if (pref && pref->prefFlags.HasUserValue()) {
    pref->prefFlags.SetHasUserValue(false);

    if (!pref->prefFlags.HasDefault()) {
      gHashTable->RemoveEntry(pref);
    }

    pref_DoCallback(aPrefName);
    MakeDirtyCallback();
  }
  return NS_OK;
}

nsresult
PREF_ClearAllUserPrefs()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

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

nsresult
PREF_LockPref(const char* aKey, bool aLockIt)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aKey);
  if (!pref) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aLockIt) {
    if (!pref->prefFlags.IsLocked()) {
      pref->prefFlags.SetLocked(true);
      gIsAnyPrefLocked = true;
      pref_DoCallback(aKey);
    }
  } else if (pref->prefFlags.IsLocked()) {
    pref->prefFlags.SetLocked(false);
    pref_DoCallback(aKey);
  }

  return NS_OK;
}

//
// Hash table functions
//

static bool
pref_ValueChanged(PrefValue aOldValue, PrefValue aNewValue, PrefType aType)
{
  bool changed = true;
  switch (aType) {
    case PrefType::String:
      if (aOldValue.stringVal && aNewValue.stringVal) {
        changed = (strcmp(aOldValue.stringVal, aNewValue.stringVal) != 0);
      }
      break;

    case PrefType::Int:
      changed = aOldValue.intVal != aNewValue.intVal;
      break;

    case PrefType::Bool:
      changed = aOldValue.boolVal != aNewValue.boolVal;
      break;

    case PrefType::Invalid:
    default:
      changed = false;
      break;
  }

  return changed;
}

// Overwrite the type and value of an existing preference. Caller must ensure
// that they are not changing the type of a preference that has a default
// value.
static PrefTypeFlags
pref_SetValue(PrefValue* aExistingValue,
              PrefTypeFlags aFlags,
              PrefValue aNewValue,
              PrefType aNewType)
{
  if (aFlags.IsTypeString() && aExistingValue->stringVal) {
    PL_strfree(aExistingValue->stringVal);
  }

  aFlags.SetPrefType(aNewType);
  if (aFlags.IsTypeString()) {
    MOZ_ASSERT(aNewValue.stringVal);
    aExistingValue->stringVal =
      aNewValue.stringVal ? PL_strdup(aNewValue.stringVal) : nullptr;
  } else {
    *aExistingValue = aNewValue;
  }

  return aFlags;
}

#ifdef DEBUG
static pref_initPhase gPhase = START;

static bool gWatchingPref = false;

void
pref_SetInitPhase(pref_initPhase aPhase)
{
  gPhase = aPhase;
}

pref_initPhase
pref_GetInitPhase()
{
  return gPhase;
}

void
pref_SetWatchingPref(bool watching)
{
  gWatchingPref = watching;
}

struct StringComparator
{
  const char* mKey;
  explicit StringComparator(const char* aKey)
    : mKey(aKey)
  {
  }
  int operator()(const char* aString) const { return strcmp(mKey, aString); }
};

bool
InInitArray(const char* aKey)
{
  size_t prefsLen;
  size_t found;
  const char** list = mozilla::dom::ContentPrefs::GetContentPrefs(&prefsLen);
  return BinarySearchIf(list, 0, prefsLen, StringComparator(aKey), &found);
}
#endif

PrefHashEntry*
pref_HashTableLookup(const char* aKey)
{
  MOZ_ASSERT(NS_IsMainThread() || mozilla::ServoStyleSet::IsInServoTraversal());
  MOZ_ASSERT((!XRE_IsContentProcess() || gPhase != START),
             "pref access before commandline prefs set");

  // If you're hitting this assertion, you've added a pref access to start up.
  // Consider moving it later or add it to the whitelist in ContentPrefs.cpp
  // and get review from a DOM peer
#ifdef DEBUG
  if (XRE_IsContentProcess() && gPhase <= END_INIT_PREFS && !gWatchingPref &&
      !InInitArray(aKey)) {
    MOZ_CRASH_UNSAFE_PRINTF(
      "accessing non-init pref %s before the rest of the prefs are sent", aKey);
  }
#endif

  return static_cast<PrefHashEntry*>(gHashTable->Search(aKey));
}

nsresult
pref_HashPref(const char* aKey,
              PrefValue aValue,
              PrefType aType,
              uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gHashTable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  auto pref = static_cast<PrefHashEntry*>(gHashTable->Add(aKey, fallible));
  if (!pref) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // New entry, need to initialize.
  if (!pref->key) {
    // Initialize the pref entry.
    pref->prefFlags.Reset().SetPrefType(aType);
    pref->key = ArenaStrdup(aKey, gPrefNameArena);
    memset(&pref->defaultPref, 0, sizeof(pref->defaultPref));
    memset(&pref->userPref, 0, sizeof(pref->userPref));

  } else if (pref->prefFlags.HasDefault() &&
             !pref->prefFlags.IsPrefType(aType)) {
    NS_WARNING(
      nsPrintfCString(
        "Trying to overwrite value of default pref %s with the wrong type!",
        aKey)
        .get());

    return NS_ERROR_UNEXPECTED;
  }

  bool valueChanged = false;
  if (aFlags & kPrefSetDefault) {
    if (!pref->prefFlags.IsLocked()) {
      // ?? change of semantics?
      if (pref_ValueChanged(pref->defaultPref, aValue, aType) ||
          !pref->prefFlags.HasDefault()) {
        pref->prefFlags =
          pref_SetValue(&pref->defaultPref, pref->prefFlags, aValue, aType)
            .SetHasDefault(true);
        if (aFlags & kPrefStickyDefault) {
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
    // If new value is same as the default value and it's not a "sticky" pref,
    // then un-set the user value. Otherwise, set the user value only if it has
    // changed.
    if ((pref->prefFlags.HasDefault()) &&
        !(pref->prefFlags.HasStickyDefault()) &&
        !pref_ValueChanged(pref->defaultPref, aValue, aType) &&
        !(aFlags & kPrefForceSet)) {
      if (pref->prefFlags.HasUserValue()) {
        // XXX should we free a user-set string value if there is one?
        pref->prefFlags.SetHasUserValue(false);
        if (!pref->prefFlags.IsLocked()) {
          MakeDirtyCallback();
          valueChanged = true;
        }
      }
    } else if (!pref->prefFlags.HasUserValue() ||
               !pref->prefFlags.IsPrefType(aType) ||
               pref_ValueChanged(pref->userPref, aValue, aType)) {
      pref->prefFlags =
        pref_SetValue(&pref->userPref, pref->prefFlags, aValue, aType)
          .SetHasUserValue(true);
      if (!pref->prefFlags.IsLocked()) {
        MakeDirtyCallback();
        valueChanged = true;
      }
    }
  }

  if (valueChanged) {
    return pref_DoCallback(aKey);
  }

  return NS_OK;
}

size_t
pref_SizeOfPrivateData(MallocSizeOf aMallocSizeOf)
{
  size_t n = gPrefNameArena.SizeOfExcludingThis(aMallocSizeOf);
  for (CallbackNode* node = gFirstCallback; node; node = node->mNext) {
    n += aMallocSizeOf(node);
    n += aMallocSizeOf(node->mDomain);
  }
  return n;
}

PrefType
PREF_GetPrefType(const char* aPrefName)
{
  if (gHashTable) {
    PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
    if (pref) {
      return pref->prefFlags.GetPrefType();
    }
  }
  return PrefType::Invalid;
}

bool
PREF_PrefIsLocked(const char* aPrefName)
{
  bool result = false;
  if (gIsAnyPrefLocked && gHashTable) {
    PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
    if (pref && pref->prefFlags.IsLocked()) {
      result = true;
    }
  }

  return result;
}

// Adds a node to the beginning of the callback list.
void
PREF_RegisterPriorityCallback(const char* aPrefNode,
                              PrefChangedFunc aCallback,
                              void* aData)
{
  NS_PRECONDITION(aPrefNode, "aPrefNode must not be nullptr");
  NS_PRECONDITION(aCallback, "aCallback must not be nullptr");

  auto node = (CallbackNode*)malloc(sizeof(struct CallbackNode));
  if (node) {
    node->mDomain = PL_strdup(aPrefNode);
    node->mFunc = aCallback;
    node->mData = aData;
    node->mNext = gFirstCallback;
    gFirstCallback = node;
    if (!gLastPriorityNode) {
      gLastPriorityNode = node;
    }
  }
}

// Adds a node to the end of the callback list.
void
PREF_RegisterCallback(const char* aPrefNode,
                      PrefChangedFunc aCallback,
                      void* aData)
{
  NS_PRECONDITION(aPrefNode, "aPrefNode must not be nullptr");
  NS_PRECONDITION(aCallback, "aCallback must not be nullptr");

  auto node = (CallbackNode*)malloc(sizeof(struct CallbackNode));
  if (node) {
    node->mDomain = PL_strdup(aPrefNode);
    node->mFunc = aCallback;
    node->mData = aData;
    if (gLastPriorityNode) {
      node->mNext = gLastPriorityNode->mNext;
      gLastPriorityNode->mNext = node;
    } else {
      node->mNext = gFirstCallback;
      gFirstCallback = node;
    }
  }
}

// Removes |node| from callback list. Returns the node after the deleted one.
CallbackNode*
pref_RemoveCallbackNode(CallbackNode* aNode, CallbackNode* aPrevNode)
{
  NS_PRECONDITION(!aPrevNode || aPrevNode->mNext == aNode, "invalid params");
  NS_PRECONDITION(aPrevNode || gFirstCallback == aNode, "invalid params");

  NS_ASSERTION(
    !gCallbacksInProgress,
    "modifying the callback list while gCallbacksInProgress is true");

  CallbackNode* next_node = aNode->mNext;
  if (aPrevNode) {
    aPrevNode->mNext = next_node;
  } else {
    gFirstCallback = next_node;
  }
  if (gLastPriorityNode == aNode) {
    gLastPriorityNode = aPrevNode;
  }
  PL_strfree(aNode->mDomain);
  free(aNode);
  return next_node;
}

// Deletes a node from the callback list or marks it for deletion.
nsresult
PREF_UnregisterCallback(const char* aPrefNode,
                        PrefChangedFunc aCallback,
                        void* aData)
{
  nsresult rv = NS_ERROR_FAILURE;
  CallbackNode* node = gFirstCallback;
  CallbackNode* prev_node = nullptr;

  while (node != nullptr) {
    if (node->mFunc == aCallback && node->mData == aData &&
        strcmp(node->mDomain, aPrefNode) == 0) {
      if (gCallbacksInProgress) {
        // postpone the node removal until after
        // callbacks enumeration is finished.
        node->mFunc = nullptr;
        gShouldCleanupDeadNodes = true;
        prev_node = node;
        node = node->mNext;
      } else {
        node = pref_RemoveCallbackNode(node, prev_node);
      }
      rv = NS_OK;
    } else {
      prev_node = node;
      node = node->mNext;
    }
  }
  return rv;
}

static nsresult
pref_DoCallback(const char* aChangedPref)
{
  nsresult rv = NS_OK;
  CallbackNode* node;

  bool reentered = gCallbacksInProgress;

  // Nodes must not be deleted while gCallbacksInProgress is true.
  // Nodes that need to be deleted are marked for deletion by nulling
  // out the |func| pointer. We release them at the end of this function
  // if we haven't reentered.
  gCallbacksInProgress = true;

  for (node = gFirstCallback; node != nullptr; node = node->mNext) {
    if (node->mFunc &&
        PL_strncmp(aChangedPref, node->mDomain, strlen(node->mDomain)) == 0) {
      (*node->mFunc)(aChangedPref, node->mData);
    }
  }

  gCallbacksInProgress = reentered;

  if (gShouldCleanupDeadNodes && !gCallbacksInProgress) {
    CallbackNode* prev_node = nullptr;
    node = gFirstCallback;

    while (node != nullptr) {
      if (!node->mFunc) {
        node = pref_RemoveCallbackNode(node, prev_node);
      } else {
        prev_node = node;
        node = node->mNext;
      }
    }
    gShouldCleanupDeadNodes = false;
  }

  return rv;
}

void
PREF_ReaderCallback(void* aClosure,
                    const char* aPref,
                    PrefValue aValue,
                    PrefType aType,
                    bool aIsDefault,
                    bool aIsStickyDefault)
{
  uint32_t flags = 0;
  if (aIsDefault) {
    flags |= kPrefSetDefault;
    if (aIsStickyDefault) {
      flags |= kPrefStickyDefault;
    }
  } else {
    flags |= kPrefForceSet;
  }
  pref_HashPref(aPref, aValue, aType, flags);
}
