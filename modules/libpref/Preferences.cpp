/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "mozilla/ArenaAllocatorExtensions.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentPrefs.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Logging.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/Variant.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAutoPtr.h"
#include "nsCategoryManagerUtils.h"
#include "nsClassHashtable.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsDataHashtable.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsIConsoleService.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsIRelativeFilePref.h"
#include "nsISafeOutputStream.h"
#include "nsISimpleEnumerator.h"
#include "nsIStringBundle.h"
#include "nsIStringEnumerator.h"
#include "nsISupportsImpl.h"
#include "nsISupportsPrimitives.h"
#include "nsIZipReader.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsQuickSort.h"
#include "nsReadableUtils.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsUTF8Utils.h"
#include "nsWeakReference.h"
#include "nsXPCOMCID.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "nsZipArchive.h"
#include "plbase64.h"
#include "PLDHashTable.h"
#include "plstr.h"
#include "prlink.h"

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#endif

#ifdef XP_WIN
#include "windows.h"
#endif

using namespace mozilla;

#ifdef DEBUG

#define ENSURE_MAIN_PROCESS(func, pref)                                        \
  do {                                                                         \
    if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                \
      nsPrintfCString msg(                                                     \
        "ENSURE_MAIN_PROCESS: called %s on %s in a non-main process",          \
        func,                                                                  \
        pref);                                                                 \
      NS_ERROR(msg.get());                                                     \
      return NS_ERROR_NOT_AVAILABLE;                                           \
    }                                                                          \
  } while (0)

#define ENSURE_MAIN_PROCESS_WITH_WARNING(func, pref)                           \
  do {                                                                         \
    if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                \
      nsPrintfCString msg(                                                     \
        "ENSURE_MAIN_PROCESS: called %s on %s in a non-main process",          \
        func,                                                                  \
        pref);                                                                 \
      NS_WARNING(msg.get());                                                   \
      return NS_ERROR_NOT_AVAILABLE;                                           \
    }                                                                          \
  } while (0)

#else // DEBUG

#define ENSURE_MAIN_PROCESS(func, pref)                                        \
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                  \
    return NS_ERROR_NOT_AVAILABLE;                                             \
  }

#define ENSURE_MAIN_PROCESS_WITH_WARNING(func, pref)                           \
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                  \
    return NS_ERROR_NOT_AVAILABLE;                                             \
  }

#endif // DEBUG

//===========================================================================
// The old low-level prefs API
//===========================================================================

struct PrefHashEntry;

typedef nsTArray<nsCString> PrefSaveData;

static PrefHashEntry*
pref_HashTableLookup(const char* aKey);

// 1 MB should be enough for everyone.
static const uint32_t MAX_PREF_LENGTH = 1 * 1024 * 1024;
// Actually, 4kb should be enough for everyone.
static const uint32_t MAX_ADVISABLE_PREF_LENGTH = 4 * 1024;

union PrefValue {
  const char* mStringVal;
  int32_t mIntVal;
  bool mBoolVal;
};

// Preference flags, including the native type of the preference. Changing any
// of these values will require modifying the code inside of PrefTypeFlags
// class.
enum class PrefType
{
  Invalid = 0,
  String = 1,
  Int = 2,
  Bool = 3,
};

// Keep the type of the preference, as well as the flags guiding its behaviour.
class PrefTypeFlags
{
public:
  PrefTypeFlags()
    : mValue(AsInt(PrefType::Invalid))
  {
  }

  explicit PrefTypeFlags(PrefType aType)
    : mValue(AsInt(aType))
  {
  }

  PrefTypeFlags& Reset()
  {
    mValue = AsInt(PrefType::Invalid);
    return *this;
  }

  bool IsTypeValid() const { return !IsPrefType(PrefType::Invalid); }
  bool IsTypeString() const { return IsPrefType(PrefType::String); }
  bool IsTypeInt() const { return IsPrefType(PrefType::Int); }
  bool IsTypeBool() const { return IsPrefType(PrefType::Bool); }
  bool IsPrefType(PrefType type) const { return GetPrefType() == type; }

  void SetPrefType(PrefType aType)
  {
    mValue = mValue - AsInt(GetPrefType()) + AsInt(aType);
  }

  PrefType GetPrefType() const
  {
    return (PrefType)(mValue & (AsInt(PrefType::String) | AsInt(PrefType::Int) |
                                AsInt(PrefType::Bool)));
  }

  bool HasDefault() const { return mValue & PREF_FLAG_HAS_DEFAULT; }

  void SetHasDefault(bool aSetOrUnset)
  {
    SetFlag(PREF_FLAG_HAS_DEFAULT, aSetOrUnset);
  }

  bool HasStickyDefault() const { return mValue & PREF_FLAG_STICKY_DEFAULT; }

  void SetHasStickyDefault(bool aSetOrUnset)
  {
    SetFlag(PREF_FLAG_STICKY_DEFAULT, aSetOrUnset);
  }

  bool IsLocked() const { return mValue & PREF_FLAG_LOCKED; }

  void SetLocked(bool aSetOrUnset) { SetFlag(PREF_FLAG_LOCKED, aSetOrUnset); }

  bool HasUserValue() const { return mValue & PREF_FLAG_USERSET; }

  void SetHasUserValue(bool aSetOrUnset)
  {
    SetFlag(PREF_FLAG_USERSET, aSetOrUnset);
  }

private:
  static uint16_t AsInt(PrefType aType) { return (uint16_t)aType; }

  void SetFlag(uint16_t aFlag, bool aSetOrUnset)
  {
    mValue = aSetOrUnset ? mValue | aFlag : mValue & ~aFlag;
  }

  // We pack both the value of type (PrefType) and flags into the same int. The
  // flag enum starts at 4 so that the PrefType can occupy the bottom two bits.
  enum
  {
    PREF_FLAG_LOCKED = 4,
    PREF_FLAG_USERSET = 8,
    PREF_FLAG_HAS_DEFAULT = 16,
    PREF_FLAG_STICKY_DEFAULT = 32,
  };
  uint16_t mValue;
};

struct PrefHashEntry : PLDHashEntryHdr
{
  PrefTypeFlags mPrefFlags; // this field first to minimize 64-bit struct size
  const char* mKey;
  PrefValue mDefaultPref;
  PrefValue mUserPref;
};

static nsresult
PREF_ClearUserPref(const char* aPrefName);

static void
ClearPrefEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  auto pref = static_cast<PrefHashEntry*>(aEntry);
  if (pref->mPrefFlags.IsTypeString()) {
    free(const_cast<char*>(pref->mDefaultPref.mStringVal));
    free(const_cast<char*>(pref->mUserPref.mStringVal));
  }

  // Don't need to free this because it's allocated in memory owned by
  // gPrefNameArena.
  pref->mKey = nullptr;
  memset(aEntry, 0, aTable->EntrySize());
}

static bool
MatchPrefEntry(const PLDHashEntryHdr* aEntry, const void* aKey)
{
  auto prefEntry = static_cast<const PrefHashEntry*>(aEntry);

  if (prefEntry->mKey == aKey) {
    return true;
  }

  if (!prefEntry->mKey || !aKey) {
    return false;
  }

  auto otherKey = static_cast<const char*>(aKey);
  return (strcmp(prefEntry->mKey, otherKey) == 0);
}

struct CallbackNode
{
  const char* mDomain;

  // If someone attempts to remove the node from the callback list while
  // pref_DoCallback is running, |func| is set to nullptr. Such nodes will
  // be removed at the end of pref_DoCallback.
  PrefChangedFunc mFunc;
  void* mData;
  CallbackNode* mNext;
};

static PLDHashTable* gHashTable;

static ArenaAllocator<8192, 4> gPrefNameArena;

// The callback list contains all the priority callbacks followed by the
// non-priority callbacks. gLastPriorityNode records where the first part ends.
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

// The Init function initializes the preference context and creates the
// preference hashtable.
static void
PREF_Init()
{
  if (!gHashTable) {
    gHashTable = new PLDHashTable(
      &pref_HashTableOps, sizeof(PrefHashEntry), PREF_HASHTABLE_INITIAL_LENGTH);
  }
}

// Frees up all the objects except the callback list.
static void
PREF_CleanupPrefs()
{
  if (gHashTable) {
    delete gHashTable;
    gHashTable = nullptr;
    gPrefNameArena.Clear();
  }
}

// Frees the callback list. Should be called at program exit.
static void
PREF_Cleanup()
{
  NS_ASSERTION(!gCallbacksInProgress,
               "PREF_Cleanup was called while gCallbacksInProgress is true!");

  CallbackNode* node = gFirstCallback;
  CallbackNode* next_node;

  while (node) {
    next_node = node->mNext;
    free(const_cast<char*>(node->mDomain));
    free(node);
    node = next_node;
  }
  gLastPriorityNode = gFirstCallback = nullptr;

  PREF_CleanupPrefs();
}

// Assign to aResult a quoted, escaped copy of aOriginal.
static void
StrEscape(const char* aOriginal, nsCString& aResult)
{
  if (aOriginal == nullptr) {
    aResult.AssignLiteral("\"\"");
    return;
  }

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

  aResult.Assign('"');

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

  aResult.Append('"');
}

//
// External calls
//

// Set a char* pref. This function takes a dotted notation of the preference
// name (e.g. "browser.startup.homepage"). Note that this will cause the
// preference to be saved to the file if it is different from the default. In
// other words, this is used to set the _user_ preferences.
//
// If aSetDefault is set to true however, it sets the default value. This will
// only affect the program behavior if the user does not have a value saved
// over it for the particular preference. In addition, these will never be
// saved out to disk.
//
// Each set returns PREF_VALUECHANGED if the user value changed (triggering a
// callback), or PREF_NOERROR if the value was unchanged.
static nsresult
PREF_SetCStringPref(const char* aPrefName,
                    const nsACString& aValue,
                    bool aSetDefault)
{
  if (aValue.Length() > MAX_PREF_LENGTH) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // It's ok to stash a pointer to the temporary PromiseFlatCString's chars in
  // pref because pref_HashPref() duplicates those chars.
  PrefValue pref;
  const nsCString& flat = PromiseFlatCString(aValue);
  pref.mStringVal = flat.get();

  return pref_HashPref(
    aPrefName, pref, PrefType::String, aSetDefault ? kPrefSetDefault : 0);
}

// Like PREF_SetCStringPref(), but for integers.
static nsresult
PREF_SetIntPref(const char* aPrefName, int32_t aValue, bool aSetDefault)
{
  PrefValue pref;
  pref.mIntVal = aValue;

  return pref_HashPref(
    aPrefName, pref, PrefType::Int, aSetDefault ? kPrefSetDefault : 0);
}

// Like PREF_SetCStringPref(), but for booleans.
static nsresult
PREF_SetBoolPref(const char* aPrefName, bool aValue, bool aSetDefault)
{
  PrefValue pref;
  pref.mBoolVal = aValue;

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
      return PREF_SetCStringPref(aPrefName, aValue.get_nsCString(), setDefault);

    case dom::PrefValue::Tint32_t:
      return PREF_SetIntPref(aPrefName, aValue.get_int32_t(), setDefault);

    case dom::PrefValue::Tbool:
      return PREF_SetBoolPref(aPrefName, aValue.get_bool(), setDefault);

    default:
      MOZ_CRASH();
  }
}

static nsresult
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

static PrefSaveData
pref_savePrefs()
{
  PrefSaveData savedPrefs(gHashTable->EntryCount());

  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto pref = static_cast<PrefHashEntry*>(iter.Get());

    // where we're getting our pref from
    PrefValue* sourcePref;

    if (pref->mPrefFlags.HasUserValue() &&
        (pref_ValueChanged(pref->mDefaultPref,
                           pref->mUserPref,
                           pref->mPrefFlags.GetPrefType()) ||
         !pref->mPrefFlags.HasDefault() ||
         pref->mPrefFlags.HasStickyDefault())) {
      sourcePref = &pref->mUserPref;
    } else {
      // do not save default prefs that haven't changed
      continue;
    }

    nsAutoCString prefName;
    StrEscape(pref->mKey, prefName);

    nsAutoCString prefValue;
    if (pref->mPrefFlags.IsTypeString()) {
      StrEscape(sourcePref->mStringVal, prefValue);

    } else if (pref->mPrefFlags.IsTypeInt()) {
      prefValue.AppendInt(sourcePref->mIntVal);

    } else if (pref->mPrefFlags.IsTypeBool()) {
      prefValue = sourcePref->mBoolVal ? "true" : "false";
    }

    nsPrintfCString str("user_pref(%s, %s);", prefName.get(), prefValue.get());
    savedPrefs.AppendElement(str);
  }

  return savedPrefs;
}

static bool
pref_EntryHasAdvisablySizedValues(PrefHashEntry* aHashEntry)
{
  if (aHashEntry->mPrefFlags.GetPrefType() != PrefType::String) {
    return true;
  }

  const char* stringVal;
  if (aHashEntry->mPrefFlags.HasDefault()) {
    stringVal = aHashEntry->mDefaultPref.mStringVal;
    if (strlen(stringVal) > MAX_ADVISABLE_PREF_LENGTH) {
      return false;
    }
  }

  if (aHashEntry->mPrefFlags.HasUserValue()) {
    stringVal = aHashEntry->mUserPref.mStringVal;
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
    value = &aHashEntry->mUserPref;
    aPref->userValue() = dom::PrefValue();
    settingValue = &aPref->userValue().get_PrefValue();
  } else {
    value = &aHashEntry->mDefaultPref;
    aPref->defaultValue() = dom::PrefValue();
    settingValue = &aPref->defaultValue().get_PrefValue();
  }

  switch (aHashEntry->mPrefFlags.GetPrefType()) {
    case PrefType::String:
      *settingValue = nsDependentCString(value->mStringVal);
      return;
    case PrefType::Int:
      *settingValue = value->mIntVal;
      return;
    case PrefType::Bool:
      *settingValue = !!value->mBoolVal;
      return;
    default:
      MOZ_CRASH();
  }
}

static void
pref_GetPrefFromEntry(PrefHashEntry* aHashEntry, dom::PrefSetting* aPref)
{
  aPref->name() = aHashEntry->mKey;

  if (aHashEntry->mPrefFlags.HasDefault()) {
    GetPrefValueFromEntry(aHashEntry, aPref, DEFAULT_VALUE);
  } else {
    aPref->defaultValue() = null_t();
  }

  if (aHashEntry->mPrefFlags.HasUserValue()) {
    GetPrefValueFromEntry(aHashEntry, aPref, USER_VALUE);
  } else {
    aPref->userValue() = null_t();
  }

  MOZ_ASSERT(aPref->defaultValue().type() == dom::MaybePrefValue::Tnull_t ||
             aPref->userValue().type() == dom::MaybePrefValue::Tnull_t ||
             (aPref->defaultValue().get_PrefValue().type() ==
              aPref->userValue().get_PrefValue().type()));
}

static bool
PREF_HasUserPref(const char* aPrefName)
{
  if (!gHashTable) {
    return false;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  return pref && pref->mPrefFlags.HasUserValue();
}

static nsresult
PREF_GetCStringPref(const char* aPrefName,
                    nsACString& aValueOut,
                    bool aGetDefault)
{
  aValueOut.SetIsVoid(true);

  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  if (!pref || !pref->mPrefFlags.IsTypeString()) {
    return NS_ERROR_UNEXPECTED;
  }

  const char* stringVal = nullptr;
  if (aGetDefault || pref->mPrefFlags.IsLocked() ||
      !pref->mPrefFlags.HasUserValue()) {

    // Do we have a default?
    if (!pref->mPrefFlags.HasDefault()) {
      return NS_ERROR_UNEXPECTED;
    }
    stringVal = pref->mDefaultPref.mStringVal;
  } else {
    stringVal = pref->mUserPref.mStringVal;
  }

  if (!stringVal) {
    return NS_ERROR_UNEXPECTED;
  }

  aValueOut = stringVal;
  return NS_OK;
}

static nsresult
PREF_GetIntPref(const char* aPrefName, int32_t* aValueOut, bool aGetDefault)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  if (!pref || !pref->mPrefFlags.IsTypeInt()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aGetDefault || pref->mPrefFlags.IsLocked() ||
      !pref->mPrefFlags.HasUserValue()) {

    // Do we have a default?
    if (!pref->mPrefFlags.HasDefault()) {
      return NS_ERROR_UNEXPECTED;
    }
    *aValueOut = pref->mDefaultPref.mIntVal;
  } else {
    *aValueOut = pref->mUserPref.mIntVal;
  }

  return NS_OK;
}

static nsresult
PREF_GetBoolPref(const char* aPrefName, bool* aValueOut, bool aGetDefault)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  if (!pref || !pref->mPrefFlags.IsTypeBool()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aGetDefault || pref->mPrefFlags.IsLocked() ||
      !pref->mPrefFlags.HasUserValue()) {

    // Do we have a default?
    if (!pref->mPrefFlags.HasDefault()) {
      return NS_ERROR_UNEXPECTED;
    }
    *aValueOut = pref->mDefaultPref.mBoolVal;
  } else {
    *aValueOut = pref->mUserPref.mBoolVal;
  }

  return NS_OK;
}

// Clears the given pref (reverts it to its default value).
static nsresult
PREF_ClearUserPref(const char* aPrefName)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
  if (pref && pref->mPrefFlags.HasUserValue()) {
    pref->mPrefFlags.SetHasUserValue(false);

    if (!pref->mPrefFlags.HasDefault()) {
      gHashTable->RemoveEntry(pref);
    }

    pref_DoCallback(aPrefName);
    Preferences::HandleDirty();
  }
  return NS_OK;
}

// Clears all user prefs.
static nsresult
PREF_ClearAllUserPrefs()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  std::vector<std::string> prefStrings;
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto pref = static_cast<PrefHashEntry*>(iter.Get());

    if (pref->mPrefFlags.HasUserValue()) {
      prefStrings.push_back(std::string(pref->mKey));

      pref->mPrefFlags.SetHasUserValue(false);
      if (!pref->mPrefFlags.HasDefault()) {
        iter.Remove();
      }
    }
  }

  for (std::string& prefString : prefStrings) {
    pref_DoCallback(prefString.c_str());
  }

  Preferences::HandleDirty();
  return NS_OK;
}

// Function that sets whether or not the preference is locked and therefore
// cannot be changed.
static nsresult
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
    if (!pref->mPrefFlags.IsLocked()) {
      pref->mPrefFlags.SetLocked(true);
      gIsAnyPrefLocked = true;
      pref_DoCallback(aKey);
    }
  } else if (pref->mPrefFlags.IsLocked()) {
    pref->mPrefFlags.SetLocked(false);
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
      if (aOldValue.mStringVal && aNewValue.mStringVal) {
        changed = (strcmp(aOldValue.mStringVal, aNewValue.mStringVal) != 0);
      }
      break;

    case PrefType::Int:
      changed = aOldValue.mIntVal != aNewValue.mIntVal;
      break;

    case PrefType::Bool:
      changed = aOldValue.mBoolVal != aNewValue.mBoolVal;
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
static void
pref_SetValue(PrefValue* aExistingValue,
              PrefType aExistingType,
              PrefValue aNewValue,
              PrefType aNewType)
{
  if (aExistingType == PrefType::String) {
    free(const_cast<char*>(aExistingValue->mStringVal));
  }

  if (aNewType == PrefType::String) {
    MOZ_ASSERT(aNewValue.mStringVal);
    aExistingValue->mStringVal =
      aNewValue.mStringVal ? moz_xstrdup(aNewValue.mStringVal) : nullptr;
  } else {
    *aExistingValue = aNewValue;
  }
}

#ifdef DEBUG

static pref_initPhase gPhase = START;

struct StringComparator
{
  const char* mKey;
  explicit StringComparator(const char* aKey)
    : mKey(aKey)
  {
  }
  int operator()(const char* aString) const { return strcmp(mKey, aString); }
};

static bool
InInitArray(const char* aKey)
{
  size_t prefsLen;
  size_t found;
  const char** list = mozilla::dom::ContentPrefs::GetContentPrefs(&prefsLen);
  return BinarySearchIf(list, 0, prefsLen, StringComparator(aKey), &found);
}

static bool gWatchingPref = false;

class WatchingPrefRAII
{
public:
  WatchingPrefRAII() { gWatchingPref = true; }
  ~WatchingPrefRAII() { gWatchingPref = false; }
};

#define WATCHING_PREF_RAII() WatchingPrefRAII watchingPrefRAII

#else // DEBUG

#define WATCHING_PREF_RAII()

#endif // DEBUG

static PrefHashEntry*
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

static nsresult
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
  if (!pref->mKey) {
    // Initialize the pref entry.
    pref->mPrefFlags.Reset().SetPrefType(aType);
    pref->mKey = ArenaStrdup(aKey, gPrefNameArena);
    memset(&pref->mDefaultPref, 0, sizeof(pref->mDefaultPref));
    memset(&pref->mUserPref, 0, sizeof(pref->mUserPref));

  } else if (pref->mPrefFlags.HasDefault() &&
             !pref->mPrefFlags.IsPrefType(aType)) {
    NS_WARNING(
      nsPrintfCString(
        "Trying to overwrite value of default pref %s with the wrong type!",
        aKey)
        .get());

    return NS_ERROR_UNEXPECTED;
  }

  bool valueChanged = false;
  if (aFlags & kPrefSetDefault) {
    if (!pref->mPrefFlags.IsLocked()) {
      // ?? change of semantics?
      if (pref_ValueChanged(pref->mDefaultPref, aValue, aType) ||
          !pref->mPrefFlags.HasDefault()) {
        pref_SetValue(
          &pref->mDefaultPref, pref->mPrefFlags.GetPrefType(), aValue, aType);
        pref->mPrefFlags.SetPrefType(aType);
        pref->mPrefFlags.SetHasDefault(true);
        if (aFlags & kPrefStickyDefault) {
          pref->mPrefFlags.SetHasStickyDefault(true);
        }
        if (!pref->mPrefFlags.HasUserValue()) {
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
    if ((pref->mPrefFlags.HasDefault()) &&
        !(pref->mPrefFlags.HasStickyDefault()) &&
        !pref_ValueChanged(pref->mDefaultPref, aValue, aType) &&
        !(aFlags & kPrefForceSet)) {
      if (pref->mPrefFlags.HasUserValue()) {
        // XXX should we free a user-set string value if there is one?
        pref->mPrefFlags.SetHasUserValue(false);
        if (!pref->mPrefFlags.IsLocked()) {
          Preferences::HandleDirty();
          valueChanged = true;
        }
      }
    } else if (!pref->mPrefFlags.HasUserValue() ||
               !pref->mPrefFlags.IsPrefType(aType) ||
               pref_ValueChanged(pref->mUserPref, aValue, aType)) {
      pref_SetValue(
        &pref->mUserPref, pref->mPrefFlags.GetPrefType(), aValue, aType);
      pref->mPrefFlags.SetPrefType(aType);
      pref->mPrefFlags.SetHasUserValue(true);
      if (!pref->mPrefFlags.IsLocked()) {
        Preferences::HandleDirty();
        valueChanged = true;
      }
    }
  }

  if (valueChanged) {
    return pref_DoCallback(aKey);
  }

  return NS_OK;
}

static size_t
pref_SizeOfPrivateData(MallocSizeOf aMallocSizeOf)
{
  size_t n = gPrefNameArena.SizeOfExcludingThis(aMallocSizeOf);
  for (CallbackNode* node = gFirstCallback; node; node = node->mNext) {
    n += aMallocSizeOf(node);
    n += aMallocSizeOf(node->mDomain);
  }
  return n;
}

// Bool function that returns whether or not the preference is locked and
// therefore cannot be changed.
static bool
PREF_PrefIsLocked(const char* aPrefName)
{
  bool result = false;
  if (gIsAnyPrefLocked && gHashTable) {
    PrefHashEntry* pref = pref_HashTableLookup(aPrefName);
    if (pref && pref->mPrefFlags.IsLocked()) {
      result = true;
    }
  }

  return result;
}

// Adds a node to the callback list; the position depends on aIsPriority. The
// callback function will be called if anything below that node is modified.
static void
PREF_RegisterCallback(const char* aPrefNode,
                      PrefChangedFunc aCallback,
                      void* aData,
                      bool aIsPriority)
{
  NS_PRECONDITION(aPrefNode, "aPrefNode must not be nullptr");
  NS_PRECONDITION(aCallback, "aCallback must not be nullptr");

  auto node = (CallbackNode*)moz_xmalloc(sizeof(CallbackNode));
  node->mDomain = moz_xstrdup(aPrefNode);
  node->mFunc = aCallback;
  node->mData = aData;

  if (aIsPriority) {
    // Add to the start of the list.
    node->mNext = gFirstCallback;
    gFirstCallback = node;
    if (!gLastPriorityNode) {
      gLastPriorityNode = node;
    }
  } else {
    // Add to the start of the non-priority part of the list.
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
static CallbackNode*
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
  free(const_cast<char*>(aNode->mDomain));
  free(aNode);
  return next_node;
}

// Deletes a node from the callback list or marks it for deletion. Succeeds if
// a callback was found that matched all the parameters.
static nsresult
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

// The callback function of the prefs parser.
static void
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

//===========================================================================
// Prefs parsing
//===========================================================================

// Callback function used to notify consumer of preference name value pairs.
// The pref name and value must be copied by the implementor of the callback
// if they are needed beyond the scope of the callback function.
//
// |aClosure| is user data passed to PREF_InitParseState.
// |aPref| is the preference name.
// |aValue| is the preference value.
// |aType| is the preference type (PREF_STRING, PREF_INT, or PREF_BOOL).
// |aIsDefault| indicates if it's a default preference.
// |aIsStickyDefault| indicates if it's a sticky default preference.
typedef void (*PrefReader)(void* aClosure,
                           const char* aPref,
                           PrefValue aValue,
                           PrefType aType,
                           bool aIsDefault,
                           bool aIsStickyDefault);

// Report any errors or warnings we encounter during parsing.
typedef void (*PrefParseErrorReporter)(const char* aMessage,
                                       int aLine,
                                       bool aError);

struct PrefParseState
{
  PrefReader mReader;
  PrefParseErrorReporter mReporter;
  void* mClosure;
  int mState;            // PREF_PARSE_...
  int mNextState;        // sometimes used...
  const char* mStrMatch; // string to match
  int mStrIndex;         // next char of smatch to check;
                         // also, counter in \u parsing
  char16_t mUtf16[2];    // parsing UTF16 (\u) escape
  int mEscLen;           // length in mEscTmp
  char mEscTmp[6];       // raw escape to put back if err
  char mQuoteChar;       // char delimiter for quotations
  char* mLb;             // line buffer (only allocation)
  char* mLbCur;          // line buffer cursor
  char* mLbEnd;          // line buffer end
  char* mVb;             // value buffer (ptr into mLb)
  PrefType mVtype;       // PREF_{STRING,INT,BOOL}
  bool mIsDefault;       // true if (default) pref
  bool mIsStickyDefault; // true if (sticky) pref
};

// Pref parser states.
enum
{
  PREF_PARSE_INIT,
  PREF_PARSE_MATCH_STRING,
  PREF_PARSE_UNTIL_NAME,
  PREF_PARSE_QUOTED_STRING,
  PREF_PARSE_UNTIL_COMMA,
  PREF_PARSE_UNTIL_VALUE,
  PREF_PARSE_INT_VALUE,
  PREF_PARSE_COMMENT_MAYBE_START,
  PREF_PARSE_COMMENT_BLOCK,
  PREF_PARSE_COMMENT_BLOCK_MAYBE_END,
  PREF_PARSE_ESC_SEQUENCE,
  PREF_PARSE_HEX_ESCAPE,
  PREF_PARSE_UTF16_LOW_SURROGATE,
  PREF_PARSE_UNTIL_OPEN_PAREN,
  PREF_PARSE_UNTIL_CLOSE_PAREN,
  PREF_PARSE_UNTIL_SEMICOLON,
  PREF_PARSE_UNTIL_EOL
};

#define UTF16_ESC_NUM_DIGITS 4
#define HEX_ESC_NUM_DIGITS 2
#define BITS_PER_HEX_DIGIT 4

static const char kUserPref[] = "user_pref";
static const char kPref[] = "pref";
static const char kPrefSticky[] = "sticky_pref";
static const char kTrue[] = "true";
static const char kFalse[] = "false";

// This function will increase the size of the buffer owned by the given pref
// parse state. We currently use a simple doubling algorithm, but the only hard
// requirement is that it increase the buffer by at least the size of the
// aPS->mEscTmp buffer used for escape processing (currently 6 bytes).
//
// The buffer is used to store partial pref lines. It is freed when the parse
// state is destroyed.
//
// @param aPS
//        parse state instance
//
// This function updates all pointers that reference an address within mLb
// since realloc may relocate the buffer.
//
// @return false if insufficient memory.
static bool
pref_GrowBuf(PrefParseState* aPS)
{
  int bufLen, curPos, valPos;

  bufLen = aPS->mLbEnd - aPS->mLb;
  curPos = aPS->mLbCur - aPS->mLb;
  valPos = aPS->mVb - aPS->mLb;

  if (bufLen == 0) {
    bufLen = 128; // default buffer size
  } else {
    bufLen <<= 1; // double buffer size
  }

  aPS->mLb = (char*)realloc(aPS->mLb, bufLen);
  if (!aPS->mLb) {
    return false;
  }

  aPS->mLbCur = aPS->mLb + curPos;
  aPS->mLbEnd = aPS->mLb + bufLen;
  aPS->mVb = aPS->mLb + valPos;

  return true;
}

// Report an error or a warning. If not specified, just dump to stderr.
static void
pref_ReportParseProblem(PrefParseState& aPS,
                        const char* aMessage,
                        int aLine,
                        bool aError)
{
  if (aPS.mReporter) {
    aPS.mReporter(aMessage, aLine, aError);
  } else {
    printf_stderr("**** Preference parsing %s (line %d) = %s **\n",
                  (aError ? "error" : "warning"),
                  aLine,
                  aMessage);
  }
}

// Initialize a PrefParseState instance.
//
// |aPS| is the PrefParseState instance.
// |aReader| is the PrefReader callback function, which will be called once for
// each preference name value pair extracted.
// |aReporter| is the PrefParseErrorReporter callback function, which will be
// called if we encounter any errors (stop) or warnings (continue) during
// parsing.
// |aClosure| is extra data passed to |aReader|.
static void
PREF_InitParseState(PrefParseState* aPS,
                    PrefReader aReader,
                    PrefParseErrorReporter aReporter,
                    void* aClosure)
{
  memset(aPS, 0, sizeof(*aPS));
  aPS->mReader = aReader;
  aPS->mClosure = aClosure;
  aPS->mReporter = aReporter;
}

// Release any memory in use by the PrefParseState instance.
static void
PREF_FinalizeParseState(PrefParseState* aPS)
{
  if (aPS->mLb) {
    free(aPS->mLb);
  }
}

// Parse a buffer containing some portion of a preference file. This function
// may be called repeatedly as new data is made available. The PrefReader
// callback function passed PREF_InitParseState will be called as preference
// name value pairs are extracted from the data. Returns false if buffer
// contains malformed content.
//
// Pseudo-BNF
// ----------
// function      = LJUNK function-name JUNK function-args
// function-name = "user_pref" | "pref" | "sticky_pref"
// function-args = "(" JUNK pref-name JUNK "," JUNK pref-value JUNK ")" JUNK ";"
// pref-name     = quoted-string
// pref-value    = quoted-string | "true" | "false" | integer-value
// JUNK          = *(WS | comment-block | comment-line)
// LJUNK         = *(WS | comment-block | comment-line | bcomment-line)
// WS            = SP | HT | LF | VT | FF | CR
// SP            = <US-ASCII SP, space (32)>
// HT            = <US-ASCII HT, horizontal-tab (9)>
// LF            = <US-ASCII LF, linefeed (10)>
// VT            = <US-ASCII HT, vertical-tab (11)>
// FF            = <US-ASCII FF, form-feed (12)>
// CR            = <US-ASCII CR, carriage return (13)>
// comment-block = <C/C++ style comment block>
// comment-line  = <C++ style comment line>
// bcomment-line = <bourne-shell style comment line>
//
static bool
PREF_ParseBuf(PrefParseState* aPS, const char* aBuf, int aBufLen)
{
  const char* end;
  char c;
  char udigit;
  int state;

  // The line number is currently only used for the error/warning reporting.
  int lineNum = 0;

  state = aPS->mState;
  for (end = aBuf + aBufLen; aBuf != end; ++aBuf) {
    c = *aBuf;
    if (c == '\r' || c == '\n' || c == 0x1A) {
      lineNum++;
    }

    switch (state) {
      // initial state
      case PREF_PARSE_INIT:
        if (aPS->mLbCur != aPS->mLb) { // reset state
          aPS->mLbCur = aPS->mLb;
          aPS->mVb = nullptr;
          aPS->mVtype = PrefType::Invalid;
          aPS->mIsDefault = false;
          aPS->mIsStickyDefault = false;
        }
        switch (c) {
          case '/': // begin comment block or line?
            state = PREF_PARSE_COMMENT_MAYBE_START;
            break;
          case '#': // accept shell style comments
            state = PREF_PARSE_UNTIL_EOL;
            break;
          case 'u': // indicating user_pref
          case 's': // indicating sticky_pref
          case 'p': // indicating pref
            if (c == 'u') {
              aPS->mStrMatch = kUserPref;
            } else if (c == 's') {
              aPS->mStrMatch = kPrefSticky;
            } else {
              aPS->mStrMatch = kPref;
            }
            aPS->mStrIndex = 1;
            aPS->mNextState = PREF_PARSE_UNTIL_OPEN_PAREN;
            state = PREF_PARSE_MATCH_STRING;
            break;
            // else skip char
        }
        break;

      // string matching
      case PREF_PARSE_MATCH_STRING:
        if (c == aPS->mStrMatch[aPS->mStrIndex++]) {
          // If we've matched all characters, then move to next state.
          if (aPS->mStrMatch[aPS->mStrIndex] == '\0') {
            state = aPS->mNextState;
            aPS->mNextState = PREF_PARSE_INIT; // reset next state
          }
          // else wait for next char
        } else {
          pref_ReportParseProblem(*aPS, "non-matching string", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // quoted string parsing
      case PREF_PARSE_QUOTED_STRING:
        // we assume that the initial quote has already been consumed
        if (aPS->mLbCur == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
          return false; // out of memory
        }
        if (c == '\\') {
          state = PREF_PARSE_ESC_SEQUENCE;
        } else if (c == aPS->mQuoteChar) {
          *aPS->mLbCur++ = '\0';
          state = aPS->mNextState;
          aPS->mNextState = PREF_PARSE_INIT; // reset next state
        } else {
          *aPS->mLbCur++ = c;
        }
        break;

      // name parsing
      case PREF_PARSE_UNTIL_NAME:
        if (c == '\"' || c == '\'') {
          aPS->mIsDefault =
            (aPS->mStrMatch == kPref || aPS->mStrMatch == kPrefSticky);
          aPS->mIsStickyDefault = (aPS->mStrMatch == kPrefSticky);
          aPS->mQuoteChar = c;
          aPS->mNextState = PREF_PARSE_UNTIL_COMMA; // return here when done
          state = PREF_PARSE_QUOTED_STRING;
        } else if (c == '/') {     // allow embedded comment
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or quote", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // parse until we find a comma separating name and value
      case PREF_PARSE_UNTIL_COMMA:
        if (c == ',') {
          aPS->mVb = aPS->mLbCur;
          state = PREF_PARSE_UNTIL_VALUE;
        } else if (c == '/') {     // allow embedded comment
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or comma", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // value parsing
      case PREF_PARSE_UNTIL_VALUE:
        // The pref value type is unknown. So, we scan for the first character
        // of the value, and determine the type from that.
        if (c == '\"' || c == '\'') {
          aPS->mVtype = PrefType::String;
          aPS->mQuoteChar = c;
          aPS->mNextState = PREF_PARSE_UNTIL_CLOSE_PAREN;
          state = PREF_PARSE_QUOTED_STRING;
        } else if (c == 't' || c == 'f') {
          aPS->mVb = (char*)(c == 't' ? kTrue : kFalse);
          aPS->mVtype = PrefType::Bool;
          aPS->mStrMatch = aPS->mVb;
          aPS->mStrIndex = 1;
          aPS->mNextState = PREF_PARSE_UNTIL_CLOSE_PAREN;
          state = PREF_PARSE_MATCH_STRING;
        } else if (isdigit(c) || (c == '-') || (c == '+')) {
          aPS->mVtype = PrefType::Int;
          // write c to line buffer...
          if (aPS->mLbCur == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
            return false; // out of memory
          }
          *aPS->mLbCur++ = c;
          state = PREF_PARSE_INT_VALUE;
        } else if (c == '/') {     // allow embedded comment
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need value, comment or space", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      case PREF_PARSE_INT_VALUE:
        // grow line buffer if necessary...
        if (aPS->mLbCur == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
          return false; // out of memory
        }
        if (isdigit(c)) {
          *aPS->mLbCur++ = c;
        } else {
          *aPS->mLbCur++ = '\0'; // stomp null terminator; we are done.
          if (c == ')') {
            state = PREF_PARSE_UNTIL_SEMICOLON;
          } else if (c == '/') { // allow embedded comment
            aPS->mNextState = PREF_PARSE_UNTIL_CLOSE_PAREN;
            state = PREF_PARSE_COMMENT_MAYBE_START;
          } else if (isspace(c)) {
            state = PREF_PARSE_UNTIL_CLOSE_PAREN;
          } else {
            pref_ReportParseProblem(
              *aPS, "while parsing integer", lineNum, true);
            NS_WARNING("malformed pref file");
            return false;
          }
        }
        break;

      // comment parsing
      case PREF_PARSE_COMMENT_MAYBE_START:
        switch (c) {
          case '*': // comment block
            state = PREF_PARSE_COMMENT_BLOCK;
            break;
          case '/': // comment line
            state = PREF_PARSE_UNTIL_EOL;
            break;
          default:
            // pref file is malformed
            pref_ReportParseProblem(
              *aPS, "while parsing comment", lineNum, true);
            NS_WARNING("malformed pref file");
            return false;
        }
        break;

      case PREF_PARSE_COMMENT_BLOCK:
        if (c == '*') {
          state = PREF_PARSE_COMMENT_BLOCK_MAYBE_END;
        }
        break;

      case PREF_PARSE_COMMENT_BLOCK_MAYBE_END:
        switch (c) {
          case '/':
            state = aPS->mNextState;
            aPS->mNextState = PREF_PARSE_INIT;
            break;
          case '*': // stay in this state
            break;
          default:
            state = PREF_PARSE_COMMENT_BLOCK;
            break;
        }
        break;

      // string escape sequence parsing
      case PREF_PARSE_ESC_SEQUENCE:
        // It's not necessary to resize the buffer here since we should be
        // writing only one character and the resize check would have been done
        // for us in the previous state.
        switch (c) {
          case '\"':
          case '\'':
          case '\\':
            break;
          case 'r':
            c = '\r';
            break;
          case 'n':
            c = '\n';
            break;
          case 'x': // hex escape -- always interpreted as Latin-1
          case 'u': // UTF16 escape
            aPS->mEscTmp[0] = c;
            aPS->mEscLen = 1;
            aPS->mUtf16[0] = aPS->mUtf16[1] = 0;
            aPS->mStrIndex =
              (c == 'x') ? HEX_ESC_NUM_DIGITS : UTF16_ESC_NUM_DIGITS;
            state = PREF_PARSE_HEX_ESCAPE;
            continue;
          default:
            pref_ReportParseProblem(
              *aPS, "preserving unexpected JS escape sequence", lineNum, false);
            NS_WARNING("preserving unexpected JS escape sequence");
            // Invalid escape sequence so we do have to write more than one
            // character. Grow line buffer if necessary...
            if ((aPS->mLbCur + 1) == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
              return false; // out of memory
            }
            *aPS->mLbCur++ = '\\'; // preserve the escape sequence
            break;
        }
        *aPS->mLbCur++ = c;
        state = PREF_PARSE_QUOTED_STRING;
        break;

      // parsing a hex (\xHH) or mUtf16 escape (\uHHHH)
      case PREF_PARSE_HEX_ESCAPE:
        if (c >= '0' && c <= '9') {
          udigit = (c - '0');
        } else if (c >= 'A' && c <= 'F') {
          udigit = (c - 'A') + 10;
        } else if (c >= 'a' && c <= 'f') {
          udigit = (c - 'a') + 10;
        } else {
          // bad escape sequence found, write out broken escape as-is
          pref_ReportParseProblem(*aPS,
                                  "preserving invalid or incomplete hex escape",
                                  lineNum,
                                  false);
          NS_WARNING("preserving invalid or incomplete hex escape");
          *aPS->mLbCur++ = '\\'; // original escape slash
          if ((aPS->mLbCur + aPS->mEscLen) >= aPS->mLbEnd &&
              !pref_GrowBuf(aPS)) {
            return false;
          }
          for (int i = 0; i < aPS->mEscLen; ++i) {
            *aPS->mLbCur++ = aPS->mEscTmp[i];
          }

          // Push the non-hex character back for re-parsing. (++aBuf at the top
          // of the loop keeps this safe.)
          --aBuf;
          state = PREF_PARSE_QUOTED_STRING;
          continue;
        }

        // have a digit
        aPS->mEscTmp[aPS->mEscLen++] = c; // preserve it
        aPS->mUtf16[1] <<= BITS_PER_HEX_DIGIT;
        aPS->mUtf16[1] |= udigit;
        aPS->mStrIndex--;
        if (aPS->mStrIndex == 0) {
          // we have the full escape, convert to UTF8
          int utf16len = 0;
          if (aPS->mUtf16[0]) {
            // already have a high surrogate, this is a two char seq
            utf16len = 2;
          } else if (0xD800 == (0xFC00 & aPS->mUtf16[1])) {
            // a high surrogate, can't convert until we have the low
            aPS->mUtf16[0] = aPS->mUtf16[1];
            aPS->mUtf16[1] = 0;
            state = PREF_PARSE_UTF16_LOW_SURROGATE;
            break;
          } else {
            // a single mUtf16 character
            aPS->mUtf16[0] = aPS->mUtf16[1];
            utf16len = 1;
          }

          // The actual conversion.
          // Make sure there's room, 6 bytes is max utf8 len (in theory; 4
          // bytes covers the actual mUtf16 range).
          if (aPS->mLbCur + 6 >= aPS->mLbEnd && !pref_GrowBuf(aPS)) {
            return false;
          }

          ConvertUTF16toUTF8 converter(aPS->mLbCur);
          converter.write(aPS->mUtf16, utf16len);
          aPS->mLbCur += converter.Size();
          state = PREF_PARSE_QUOTED_STRING;
        }
        break;

      // looking for beginning of mUtf16 low surrogate
      case PREF_PARSE_UTF16_LOW_SURROGATE:
        if (aPS->mStrIndex == 0 && c == '\\') {
          ++aPS->mStrIndex;
        } else if (aPS->mStrIndex == 1 && c == 'u') {
          // escape sequence is correct, now parse hex
          aPS->mStrIndex = UTF16_ESC_NUM_DIGITS;
          aPS->mEscTmp[0] = 'u';
          aPS->mEscLen = 1;
          state = PREF_PARSE_HEX_ESCAPE;
        } else {
          // Didn't find expected low surrogate. Ignore high surrogate (it
          // would just get converted to nothing anyway) and start over with
          // this character.
          --aBuf;
          if (aPS->mStrIndex == 1) {
            state = PREF_PARSE_ESC_SEQUENCE;
          } else {
            state = PREF_PARSE_QUOTED_STRING;
          }
          continue;
        }
        break;

      // function open and close parsing
      case PREF_PARSE_UNTIL_OPEN_PAREN:
        // tolerate only whitespace and embedded comments
        if (c == '(') {
          state = PREF_PARSE_UNTIL_NAME;
        } else if (c == '/') {
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or open parentheses", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      case PREF_PARSE_UNTIL_CLOSE_PAREN:
        // tolerate only whitespace and embedded comments
        if (c == ')') {
          state = PREF_PARSE_UNTIL_SEMICOLON;
        } else if (c == '/') {
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or closing parentheses", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // function terminator ';' parsing
      case PREF_PARSE_UNTIL_SEMICOLON:
        // tolerate only whitespace and embedded comments
        if (c == ';') {

          PrefValue value;

          switch (aPS->mVtype) {
            case PrefType::String:
              value.mStringVal = aPS->mVb;
              break;

            case PrefType::Int:
              if ((aPS->mVb[0] == '-' || aPS->mVb[0] == '+') &&
                  aPS->mVb[1] == '\0') {
                pref_ReportParseProblem(*aPS, "invalid integer value", 0, true);
                NS_WARNING("malformed integer value");
                return false;
              }
              value.mIntVal = atoi(aPS->mVb);
              break;

            case PrefType::Bool:
              value.mBoolVal = (aPS->mVb == kTrue);
              break;

            default:
              break;
          }

          // We've extracted a complete name/value pair.
          aPS->mReader(aPS->mClosure,
                       aPS->mLb,
                       value,
                       aPS->mVtype,
                       aPS->mIsDefault,
                       aPS->mIsStickyDefault);

          state = PREF_PARSE_INIT;
        } else if (c == '/') {
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or semicolon", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // eol parsing
      case PREF_PARSE_UNTIL_EOL:
        // Need to handle mac, unix, or dos line endings. PREF_PARSE_INIT will
        // eat the next \n in case we have \r\n.
        if (c == '\r' || c == '\n' || c == 0x1A) {
          state = aPS->mNextState;
          aPS->mNextState = PREF_PARSE_INIT; // reset next state
        }
        break;
    }
  }
  aPS->mState = state;
  return true;
}

//===========================================================================
// nsPrefBranch et al.
//===========================================================================

namespace mozilla {
class PreferenceServiceReporter;
} // namespace mozilla

class nsPrefBranch;

class PrefCallback : public PLDHashEntryHdr
{
  friend class mozilla::PreferenceServiceReporter;

public:
  typedef PrefCallback* KeyType;
  typedef const PrefCallback* KeyTypePointer;

  static const PrefCallback* KeyToPointer(PrefCallback* aKey) { return aKey; }

  static PLDHashNumber HashKey(const PrefCallback* aKey)
  {
    uint32_t hash = mozilla::HashString(aKey->mDomain);
    return mozilla::AddToHash(hash, aKey->mCanonical);
  }

public:
  // Create a PrefCallback with a strong reference to its observer.
  PrefCallback(const char* aDomain,
               nsIObserver* aObserver,
               nsPrefBranch* aBranch)
    : mDomain(aDomain)
    , mBranch(aBranch)
    , mWeakRef(nullptr)
    , mStrongRef(aObserver)
  {
    MOZ_COUNT_CTOR(PrefCallback);
    nsCOMPtr<nsISupports> canonical = do_QueryInterface(aObserver);
    mCanonical = canonical;
  }

  // Create a PrefCallback with a weak reference to its observer.
  PrefCallback(const char* aDomain,
               nsISupportsWeakReference* aObserver,
               nsPrefBranch* aBranch)
    : mDomain(aDomain)
    , mBranch(aBranch)
    , mWeakRef(do_GetWeakReference(aObserver))
    , mStrongRef(nullptr)
  {
    MOZ_COUNT_CTOR(PrefCallback);
    nsCOMPtr<nsISupports> canonical = do_QueryInterface(aObserver);
    mCanonical = canonical;
  }

  // Copy constructor needs to be explicit or the linker complains.
  explicit PrefCallback(const PrefCallback*& aCopy)
    : mDomain(aCopy->mDomain)
    , mBranch(aCopy->mBranch)
    , mWeakRef(aCopy->mWeakRef)
    , mStrongRef(aCopy->mStrongRef)
    , mCanonical(aCopy->mCanonical)
  {
    MOZ_COUNT_CTOR(PrefCallback);
  }

  ~PrefCallback() { MOZ_COUNT_DTOR(PrefCallback); }

  bool KeyEquals(const PrefCallback* aKey) const
  {
    // We want to be able to look up a weakly-referencing PrefCallback after
    // its observer has died so we can remove it from the table. Once the
    // callback's observer dies, its canonical pointer is stale -- in
    // particular, we may have allocated a new observer in the same spot in
    // memory! So we can't just compare canonical pointers to determine whether
    // aKey refers to the same observer as this.
    //
    // Our workaround is based on the way we use this hashtable: When we ask
    // the hashtable to remove a PrefCallback whose weak reference has expired,
    // we use as the key for removal the same object as was inserted into the
    // hashtable. Thus we can say that if one of the keys' weak references has
    // expired, the two keys are equal iff they're the same object.

    if (IsExpired() || aKey->IsExpired()) {
      return this == aKey;
    }

    if (mCanonical != aKey->mCanonical) {
      return false;
    }

    return mDomain.Equals(aKey->mDomain);
  }

  PrefCallback* GetKey() const { return const_cast<PrefCallback*>(this); }

  // Get a reference to the callback's observer, or null if the observer was
  // weakly referenced and has been destroyed.
  already_AddRefed<nsIObserver> GetObserver() const
  {
    if (!IsWeak()) {
      nsCOMPtr<nsIObserver> copy = mStrongRef;
      return copy.forget();
    }

    nsCOMPtr<nsIObserver> observer = do_QueryReferent(mWeakRef);
    return observer.forget();
  }

  const nsCString& GetDomain() const { return mDomain; }

  nsPrefBranch* GetPrefBranch() const { return mBranch; }

  // Has this callback's weak reference died?
  bool IsExpired() const
  {
    if (!IsWeak())
      return false;

    nsCOMPtr<nsIObserver> observer(do_QueryReferent(mWeakRef));
    return !observer;
  }

  enum
  {
    ALLOW_MEMMOVE = true
  };

private:
  nsCString mDomain;
  nsPrefBranch* mBranch;

  // Exactly one of mWeakRef and mStrongRef should be non-null.
  nsWeakPtr mWeakRef;
  nsCOMPtr<nsIObserver> mStrongRef;

  // We need a canonical nsISupports pointer, per bug 578392.
  nsISupports* mCanonical;

  bool IsWeak() const { return !!mWeakRef; }
};

class nsPrefBranch final
  : public nsIPrefBranch
  , public nsIObserver
  , public nsSupportsWeakReference
{
  friend class mozilla::PreferenceServiceReporter;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPREFBRANCH
  NS_DECL_NSIOBSERVER

  nsPrefBranch(const char* aPrefRoot, bool aDefaultBranch);
  nsPrefBranch() = delete;

  int32_t GetRootLength() const { return mPrefRoot.Length(); }

  static void NotifyObserver(const char* aNewpref, void* aData);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  static void ReportToConsole(const nsAString& aMessage);

private:
  // Helper class for either returning a raw cstring or nsCString.
  typedef mozilla::Variant<const char*, const nsCString> PrefNameBase;
  class PrefName : public PrefNameBase
  {
  public:
    explicit PrefName(const char* aName)
      : PrefNameBase(aName)
    {
    }
    explicit PrefName(const nsCString& aName)
      : PrefNameBase(aName)
    {
    }

    // Use default move constructors, disallow copy constructors.
    PrefName(PrefName&& aOther) = default;
    PrefName& operator=(PrefName&& aOther) = default;
    PrefName(const PrefName&) = delete;
    PrefName& operator=(const PrefName&) = delete;

    struct PtrMatcher
    {
      static const char* match(const char* aVal) { return aVal; }
      static const char* match(const nsCString& aVal) { return aVal.get(); }
    };

    struct LenMatcher
    {
      static size_t match(const char* aVal) { return strlen(aVal); }
      static size_t match(const nsCString& aVal) { return aVal.Length(); }
    };

    const char* get() const
    {
      static PtrMatcher m;
      return match(m);
    }

    size_t Length() const
    {
      static LenMatcher m;
      return match(m);
    }
  };

  virtual ~nsPrefBranch();

  nsresult GetDefaultFromPropertiesFile(const char* aPrefName,
                                        nsAString& aReturn);

  // As SetCharPref, but without any check on the length of |aValue|.
  nsresult SetCharPrefInternal(const char* aPrefName, const nsACString& aValue);

  // Reject strings that are more than 1Mb, warn if strings are more than 16kb.
  nsresult CheckSanityOfStringLength(const char* aPrefName,
                                     const nsAString& aValue);
  nsresult CheckSanityOfStringLength(const char* aPrefName,
                                     const nsACString& aValue);
  nsresult CheckSanityOfStringLength(const char* aPrefName,
                                     const uint32_t aLength);

  void RemoveExpiredCallback(PrefCallback* aCallback);

  PrefName GetPrefName(const char* aPrefName) const;

  void FreeObserverList(void);

  const nsCString mPrefRoot;
  bool mIsDefault;

  bool mFreeingObserverList;
  nsClassHashtable<PrefCallback, PrefCallback> mObservers;
};

class nsPrefLocalizedString final : public nsIPrefLocalizedString
{
public:
  nsPrefLocalizedString();

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSISUPPORTSPRIMITIVE(mUnicodeString->)
  NS_FORWARD_NSISUPPORTSSTRING(mUnicodeString->)

  nsresult Init();

private:
  virtual ~nsPrefLocalizedString();

  nsCOMPtr<nsISupportsString> mUnicodeString;
};

class nsRelativeFilePref : public nsIRelativeFilePref
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRELATIVEFILEPREF

  nsRelativeFilePref();

private:
  virtual ~nsRelativeFilePref();

  nsCOMPtr<nsIFile> mFile;
  nsCString mRelativeToKey;
};

//----------------------------------------------------------------------------
// nsPrefBranch
//----------------------------------------------------------------------------

nsPrefBranch::nsPrefBranch(const char* aPrefRoot, bool aDefaultBranch)
  : mPrefRoot(aPrefRoot)
  , mIsDefault(aDefaultBranch)
  , mFreeingObserverList(false)
  , mObservers()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    ++mRefCnt; // must be > 0 when we call this, or we'll get deleted!

    // Add weakly so we don't have to clean up at shutdown.
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    --mRefCnt;
  }
}

nsPrefBranch::~nsPrefBranch()
{
  FreeObserverList();

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
}

NS_IMPL_ADDREF(nsPrefBranch)
NS_IMPL_RELEASE(nsPrefBranch)

NS_INTERFACE_MAP_BEGIN(nsPrefBranch)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsPrefBranch::GetRoot(nsACString& aRoot)
{
  aRoot = mPrefRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetPrefType(const char* aPrefName, int32_t* aRetVal)
{
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  PrefType type = PrefType::Invalid;
  if (gHashTable) {
    PrefHashEntry* entry = pref_HashTableLookup(pref.get());
    if (entry) {
      type = entry->mPrefFlags.GetPrefType();
    }
  }

  switch (type) {
    case PrefType::String:
      *aRetVal = PREF_STRING;
      break;

    case PrefType::Int:
      *aRetVal = PREF_INT;
      break;

    case PrefType::Bool:
      *aRetVal = PREF_BOOL;
      break;

    case PrefType::Invalid:
    default:
      *aRetVal = PREF_INVALID;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetBoolPrefWithDefault(const char* aPrefName,
                                     bool aDefaultValue,
                                     uint8_t aArgc,
                                     bool* aRetVal)
{
  nsresult rv = GetBoolPref(aPrefName, aRetVal);
  if (NS_FAILED(rv) && aArgc == 1) {
    *aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetBoolPref(const char* aPrefName, bool* aRetVal)
{
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_GetBoolPref(pref.get(), aRetVal, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::SetBoolPref(const char* aPrefName, bool aValue)
{
  ENSURE_MAIN_PROCESS("SetBoolPref", aPrefName);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_SetBoolPref(pref.get(), aValue, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::GetFloatPrefWithDefault(const char* aPrefName,
                                      float aDefaultValue,
                                      uint8_t aArgc,
                                      float* aRetVal)
{
  nsresult rv = GetFloatPref(aPrefName, aRetVal);

  if (NS_FAILED(rv) && aArgc == 1) {
    *aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetFloatPref(const char* aPrefName, float* aRetVal)
{
  NS_ENSURE_ARG(aPrefName);

  nsAutoCString stringVal;
  nsresult rv = GetCharPref(aPrefName, stringVal);
  if (NS_SUCCEEDED(rv)) {
    *aRetVal = stringVal.ToFloat(&rv);
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetCharPrefWithDefault(const char* aPrefName,
                                     const nsACString& aDefaultValue,
                                     uint8_t aArgc,
                                     nsACString& aRetVal)
{
  nsresult rv = GetCharPref(aPrefName, aRetVal);

  if (NS_FAILED(rv) && aArgc == 1) {
    aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetCharPref(const char* aPrefName, nsACString& aRetVal)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_GetCStringPref(pref.get(), aRetVal, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::SetCharPref(const char* aPrefName, const nsACString& aValue)
{
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return SetCharPrefInternal(aPrefName, aValue);
}

nsresult
nsPrefBranch::SetCharPrefInternal(const char* aPrefName,
                                  const nsACString& aValue)
{
  ENSURE_MAIN_PROCESS("SetCharPref", aPrefName);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_SetCStringPref(pref.get(), aValue, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::GetStringPref(const char* aPrefName,
                            const nsACString& aDefaultValue,
                            uint8_t aArgc,
                            nsACString& aRetVal)
{
  nsCString utf8String;
  nsresult rv = GetCharPref(aPrefName, utf8String);
  if (NS_SUCCEEDED(rv)) {
    aRetVal = utf8String;
    return rv;
  }

  if (aArgc == 1) {
    aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::SetStringPref(const char* aPrefName, const nsACString& aValue)
{
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return SetCharPrefInternal(aPrefName, aValue);
}

NS_IMETHODIMP
nsPrefBranch::GetIntPrefWithDefault(const char* aPrefName,
                                    int32_t aDefaultValue,
                                    uint8_t aArgc,
                                    int32_t* aRetVal)
{
  nsresult rv = GetIntPref(aPrefName, aRetVal);

  if (NS_FAILED(rv) && aArgc == 1) {
    *aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetIntPref(const char* aPrefName, int32_t* aRetVal)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_GetIntPref(pref.get(), aRetVal, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::SetIntPref(const char* aPrefName, int32_t aValue)
{
  ENSURE_MAIN_PROCESS("SetIntPref", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_SetIntPref(pref.get(), aValue, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::GetComplexValue(const char* aPrefName,
                              const nsIID& aType,
                              void** aRetVal)
{
  NS_ENSURE_ARG(aPrefName);

  nsresult rv;
  nsAutoCString utf8String;

  // we have to do this one first because it's different than all the rest
  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString(
      do_CreateInstance(NS_PREFLOCALIZEDSTRING_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }

    const PrefName& pref = GetPrefName(aPrefName);
    bool bNeedDefault = false;

    if (mIsDefault) {
      bNeedDefault = true;
    } else {
      // if there is no user (or locked) value
      if (!PREF_HasUserPref(pref.get()) && !PREF_PrefIsLocked(pref.get())) {
        bNeedDefault = true;
      }
    }

    // if we need to fetch the default value, do that instead, otherwise use the
    // value we pulled in at the top of this function
    if (bNeedDefault) {
      nsAutoString utf16String;
      rv = GetDefaultFromPropertiesFile(pref.get(), utf16String);
      if (NS_SUCCEEDED(rv)) {
        theString->SetData(utf16String);
      }
    } else {
      rv = GetCharPref(aPrefName, utf8String);
      if (NS_SUCCEEDED(rv)) {
        theString->SetData(NS_ConvertUTF8toUTF16(utf8String));
      }
    }

    if (NS_SUCCEEDED(rv)) {
      theString.forget(reinterpret_cast<nsIPrefLocalizedString**>(aRetVal));
    }

    return rv;
  }

  // if we can't get the pref, there's no point in being here
  rv = GetCharPref(aPrefName, utf8String);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIFile))) {
    if (XRE_IsContentProcess()) {
      NS_ERROR("cannot get nsIFile pref from content process");
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      rv = file->SetPersistentDescriptor(utf8String);
      if (NS_SUCCEEDED(rv)) {
        file.forget(reinterpret_cast<nsIFile**>(aRetVal));
        return NS_OK;
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    if (XRE_IsContentProcess()) {
      NS_ERROR("cannot get nsIRelativeFilePref from content process");
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsACString::const_iterator keyBegin, strEnd;
    utf8String.BeginReading(keyBegin);
    utf8String.EndReading(strEnd);

    // The pref has the format: [fromKey]a/b/c
    if (*keyBegin++ != '[') {
      return NS_ERROR_FAILURE;
    }

    nsACString::const_iterator keyEnd(keyBegin);
    if (!FindCharInReadable(']', keyEnd, strEnd)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString key(Substring(keyBegin, keyEnd));

    nsCOMPtr<nsIFile> fromFile;
    nsCOMPtr<nsIProperties> directoryService(
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = directoryService->Get(
      key.get(), NS_GET_IID(nsIFile), getter_AddRefs(fromFile));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIFile> theFile;
    rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(theFile));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = theFile->SetRelativeDescriptor(fromFile, Substring(++keyEnd, strEnd));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIRelativeFilePref> relativePref;
    rv = NS_NewRelativeFilePref(theFile, key, getter_AddRefs(relativePref));
    if (NS_FAILED(rv)) {
      return rv;
    }

    relativePref.forget(reinterpret_cast<nsIRelativeFilePref**>(aRetVal));
    return NS_OK;
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
    nsCOMPtr<nsISupportsString> theString(
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));

    if (NS_SUCCEEDED(rv)) {
      // Debugging to see why we end up with very long strings here with
      // some addons, see bug 836263.
      nsAutoString wdata;
      if (!AppendUTF8toUTF16(utf8String, wdata, mozilla::fallible)) {
#ifdef MOZ_CRASHREPORTER
        nsCOMPtr<nsICrashReporter> cr =
          do_GetService("@mozilla.org/toolkit/crash-reporter;1");
        if (cr) {
          cr->AnnotateCrashReport(NS_LITERAL_CSTRING("bug836263-size"),
                                  nsPrintfCString("%x", utf8String.Length()));
          cr->RegisterAppMemory(uint64_t(utf8String.BeginReading()),
                                std::min(0x1000U, utf8String.Length()));
        }
#endif
        MOZ_CRASH("bug836263");
      }
      theString->SetData(wdata);
      theString.forget(reinterpret_cast<nsISupportsString**>(aRetVal));
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::GetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

nsresult
nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName,
                                        const nsAString& aValue)
{
  return CheckSanityOfStringLength(aPrefName, aValue.Length());
}

nsresult
nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName,
                                        const nsACString& aValue)
{
  return CheckSanityOfStringLength(aPrefName, aValue.Length());
}

nsresult
nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName,
                                        const uint32_t aLength)
{
  if (aLength > MAX_PREF_LENGTH) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (aLength <= MAX_ADVISABLE_PREF_LENGTH) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIConsoleService> console =
    do_GetService("@mozilla.org/consoleservice;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString message(nsPrintfCString(
    "Warning: attempting to write %d bytes to preference %s. This is bad "
    "for general performance and memory usage. Such an amount of data "
    "should rather be written to an external file. This preference will "
    "not be sent to any content processes.",
    aLength,
    GetPrefName(aPrefName).get()));

  rv = console->LogStringMessage(NS_ConvertUTF8toUTF16(message).get());
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

/* static */ void
nsPrefBranch::ReportToConsole(const nsAString& aMessage)
{
  nsresult rv;
  nsCOMPtr<nsIConsoleService> console =
    do_GetService("@mozilla.org/consoleservice;1", &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoString message(aMessage);
  console->LogStringMessage(message.get());
}

NS_IMETHODIMP
nsPrefBranch::SetComplexValue(const char* aPrefName,
                              const nsIID& aType,
                              nsISupports* aValue)
{
  ENSURE_MAIN_PROCESS("SetComplexValue", aPrefName);
  NS_ENSURE_ARG(aPrefName);

  nsresult rv = NS_NOINTERFACE;

  if (aType.Equals(NS_GET_IID(nsIFile))) {
    nsCOMPtr<nsIFile> file = do_QueryInterface(aValue);
    if (!file) {
      return NS_NOINTERFACE;
    }

    nsAutoCString descriptorString;
    rv = file->GetPersistentDescriptor(descriptorString);
    if (NS_SUCCEEDED(rv)) {
      rv = SetCharPrefInternal(aPrefName, descriptorString);
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIRelativeFilePref))) {
    nsCOMPtr<nsIRelativeFilePref> relFilePref = do_QueryInterface(aValue);
    if (!relFilePref) {
      return NS_NOINTERFACE;
    }

    nsCOMPtr<nsIFile> file;
    relFilePref->GetFile(getter_AddRefs(file));
    if (!file) {
      return NS_NOINTERFACE;
    }

    nsAutoCString relativeToKey;
    (void)relFilePref->GetRelativeToKey(relativeToKey);

    nsCOMPtr<nsIFile> relativeToFile;
    nsCOMPtr<nsIProperties> directoryService(
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = directoryService->Get(
      relativeToKey.get(), NS_GET_IID(nsIFile), getter_AddRefs(relativeToFile));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsAutoCString relDescriptor;
    rv = file->GetRelativeDescriptor(relativeToFile, relDescriptor);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsAutoCString descriptorString;
    descriptorString.Append('[');
    descriptorString.Append(relativeToKey);
    descriptorString.Append(']');
    descriptorString.Append(relDescriptor);
    return SetCharPrefInternal(aPrefName, descriptorString);
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString)) ||
      aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsISupportsString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsString wideString;

      rv = theString->GetData(wideString);
      if (NS_SUCCEEDED(rv)) {
        // Check sanity of string length before any lengthy conversion
        rv = CheckSanityOfStringLength(aPrefName, wideString);
        if (NS_FAILED(rv)) {
          return rv;
        }
        rv = SetCharPrefInternal(aPrefName, NS_ConvertUTF16toUTF8(wideString));
      }
    }
    return rv;
  }

  NS_WARNING("nsPrefBranch::SetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsPrefBranch::ClearUserPref(const char* aPrefName)
{
  ENSURE_MAIN_PROCESS("ClearUserPref", aPrefName);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_ClearUserPref(pref.get());
}

NS_IMETHODIMP
nsPrefBranch::PrefHasUserValue(const char* aPrefName, bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  *aRetVal = PREF_HasUserPref(pref.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::LockPref(const char* aPrefName)
{
  ENSURE_MAIN_PROCESS("LockPref", aPrefName);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_LockPref(pref.get(), true);
}

NS_IMETHODIMP
nsPrefBranch::PrefIsLocked(const char* aPrefName, bool* aRetVal)
{
  ENSURE_MAIN_PROCESS("PrefIsLocked", aPrefName);
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  *aRetVal = PREF_PrefIsLocked(pref.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::UnlockPref(const char* aPrefName)
{
  ENSURE_MAIN_PROCESS("UnlockPref", aPrefName);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_LockPref(pref.get(), false);
}

NS_IMETHODIMP
nsPrefBranch::ResetBranch(const char* aStartingAt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefBranch::DeleteBranch(const char* aStartingAt)
{
  ENSURE_MAIN_PROCESS("DeleteBranch", aStartingAt);
  NS_ENSURE_ARG(aStartingAt);

  MOZ_ASSERT(NS_IsMainThread());

  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const PrefName& pref = GetPrefName(aStartingAt);
  nsAutoCString branchName(pref.get());

  // Add a trailing '.' if it doesn't already have one.
  if (branchName.Length() > 1 &&
      !StringEndsWith(branchName, NS_LITERAL_CSTRING("."))) {
    branchName += '.';
  }

  const nsACString& branchNameNoDot =
    Substring(branchName, 0, branchName.Length() - 1);

  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PrefHashEntry*>(iter.Get());

    // The first disjunct matches branches: e.g. a branch name "foo.bar."
    // matches an mKey "foo.bar.baz" (but it won't match "foo.barrel.baz").
    // The second disjunct matches leaf nodes: e.g. a branch name "foo.bar."
    // matches an mKey "foo.bar" (by ignoring the trailing '.').
    nsDependentCString key(entry->mKey);
    if (StringBeginsWith(key, branchName) || key.Equals(branchNameNoDot)) {
      iter.Remove();
    }
  }

  Preferences::HandleDirty();
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetChildList(const char* aStartingAt,
                           uint32_t* aCount,
                           char*** aChildArray)
{
  char** outArray;
  int32_t numPrefs;
  int32_t dwIndex;
  AutoTArray<nsCString, 32> prefArray;

  NS_ENSURE_ARG(aStartingAt);
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aChildArray);

  *aChildArray = nullptr;
  *aCount = 0;

  // This will contain a list of all the pref name strings. Allocated on the
  // stack for speed.

  const PrefName& parent = GetPrefName(aStartingAt);
  size_t parentLen = parent.Length();
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PrefHashEntry*>(iter.Get());
    if (strncmp(entry->mKey, parent.get(), parentLen) == 0) {
      prefArray.AppendElement(entry->mKey);
    }
  }

  // Now that we've built up the list, run the callback on all the matching
  // elements.
  numPrefs = prefArray.Length();

  if (numPrefs) {
    outArray = (char**)moz_xmalloc(numPrefs * sizeof(char*));

    for (dwIndex = 0; dwIndex < numPrefs; ++dwIndex) {
      // we need to lop off mPrefRoot in case the user is planning to pass this
      // back to us because if they do we are going to add mPrefRoot again.
      const nsCString& element = prefArray[dwIndex];
      outArray[dwIndex] =
        (char*)nsMemory::Clone(element.get() + mPrefRoot.Length(),
                               element.Length() - mPrefRoot.Length() + 1);

      if (!outArray[dwIndex]) {
        // We ran out of memory... this is annoying.
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(dwIndex, outArray);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    *aChildArray = outArray;
  }
  *aCount = numPrefs;

  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::AddObserver(const char* aDomain,
                          nsIObserver* aObserver,
                          bool aHoldWeak)
{
  PrefCallback* pCallback;

  NS_ENSURE_ARG(aDomain);
  NS_ENSURE_ARG(aObserver);

  // Hold a weak reference to the observer if so requested.
  if (aHoldWeak) {
    nsCOMPtr<nsISupportsWeakReference> weakRefFactory =
      do_QueryInterface(aObserver);
    if (!weakRefFactory) {
      // The caller didn't give us a object that supports weak reference...
      // tell them.
      return NS_ERROR_INVALID_ARG;
    }

    // Construct a PrefCallback with a weak reference to the observer.
    pCallback = new PrefCallback(aDomain, weakRefFactory, this);

  } else {
    // Construct a PrefCallback with a strong reference to the observer.
    pCallback = new PrefCallback(aDomain, aObserver, this);
  }

  auto p = mObservers.LookupForAdd(pCallback);
  if (p) {
    NS_WARNING("Ignoring duplicate observer.");
    delete pCallback;
    return NS_OK;
  }

  p.OrInsert([&pCallback]() { return pCallback; });

  // We must pass a fully qualified preference name to the callback
  // aDomain == nullptr is the only possible failure, and we trapped it with
  // NS_ENSURE_ARG above.
  const PrefName& pref = GetPrefName(aDomain);
  PREF_RegisterCallback(
    pref.get(), NotifyObserver, pCallback, /* isPriority */ false);
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::RemoveObserver(const char* aDomain, nsIObserver* aObserver)
{
  NS_ENSURE_ARG(aDomain);
  NS_ENSURE_ARG(aObserver);

  nsresult rv = NS_OK;

  // If we're in the middle of a call to FreeObserverList, don't process this
  // RemoveObserver call -- the observer in question will be removed soon, if
  // it hasn't been already.
  //
  // It's important that we don't touch mObservers in any way -- even a Get()
  // which returns null might cause the hashtable to resize itself, which will
  // break the iteration in FreeObserverList.
  if (mFreeingObserverList) {
    return NS_OK;
  }

  // Remove the relevant PrefCallback from mObservers and get an owning pointer
  // to it. Unregister the callback first, and then let the owning pointer go
  // out of scope and destroy the callback.
  PrefCallback key(aDomain, aObserver, this);
  nsAutoPtr<PrefCallback> pCallback;
  mObservers.Remove(&key, &pCallback);
  if (pCallback) {
    // aDomain == nullptr is the only possible failure, trapped above.
    const PrefName& pref = GetPrefName(aDomain);
    rv = PREF_UnregisterCallback(pref.get(), NotifyObserver, pCallback);
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::Observe(nsISupports* aSubject,
                      const char* aTopic,
                      const char16_t* aData)
{
  // Watch for xpcom shutdown and free our observers to eliminate any cyclic
  // references.
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    FreeObserverList();
  }
  return NS_OK;
}

/* static */ void
nsPrefBranch::NotifyObserver(const char* aNewPref, void* aData)
{
  PrefCallback* pCallback = (PrefCallback*)aData;

  nsCOMPtr<nsIObserver> observer = pCallback->GetObserver();
  if (!observer) {
    // The observer has expired.  Let's remove this callback.
    pCallback->GetPrefBranch()->RemoveExpiredCallback(pCallback);
    return;
  }

  // Remove any root this string may contain so as to not confuse the observer
  // by passing them something other than what they passed us as a topic.
  uint32_t len = pCallback->GetPrefBranch()->GetRootLength();
  nsAutoCString suffix(aNewPref + len);

  observer->Observe(static_cast<nsIPrefBranch*>(pCallback->GetPrefBranch()),
                    NS_PREFBRANCH_PREFCHANGE_TOPIC_ID,
                    NS_ConvertASCIItoUTF16(suffix).get());
}

size_t
nsPrefBranch::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);
  n += mPrefRoot.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += mObservers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

void
nsPrefBranch::FreeObserverList()
{
  // We need to prevent anyone from modifying mObservers while we're iterating
  // over it. In particular, some clients will call RemoveObserver() when
  // they're removed and destructed via the iterator; we set
  // mFreeingObserverList to keep those calls from touching mObservers.
  mFreeingObserverList = true;
  for (auto iter = mObservers.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<PrefCallback>& callback = iter.Data();
    nsPrefBranch* prefBranch = callback->GetPrefBranch();
    const PrefName& pref = prefBranch->GetPrefName(callback->GetDomain().get());
    PREF_UnregisterCallback(pref.get(), nsPrefBranch::NotifyObserver, callback);
    iter.Remove();
  }
  mFreeingObserverList = false;
}

void
nsPrefBranch::RemoveExpiredCallback(PrefCallback* aCallback)
{
  NS_PRECONDITION(aCallback->IsExpired(), "Callback should be expired.");
  mObservers.Remove(aCallback);
}

nsresult
nsPrefBranch::GetDefaultFromPropertiesFile(const char* aPrefName,
                                           nsAString& aReturn)
{
  // The default value contains a URL to a .properties file.

  nsAutoCString propertyFileURL;
  nsresult rv = PREF_GetCStringPref(aPrefName, propertyFileURL, true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (!bundleService) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIStringBundle> bundle;
  rv =
    bundleService->CreateBundle(propertyFileURL.get(), getter_AddRefs(bundle));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return bundle->GetStringFromName(aPrefName, aReturn);
}

nsPrefBranch::PrefName
nsPrefBranch::GetPrefName(const char* aPrefName) const
{
  NS_ASSERTION(aPrefName, "null pref name!");

  // For speed, avoid strcpy if we can.
  if (mPrefRoot.IsEmpty()) {
    return PrefName(aPrefName);
  }

  return PrefName(mPrefRoot + nsDependentCString(aPrefName));
}

//----------------------------------------------------------------------------
// nsPrefLocalizedString
//----------------------------------------------------------------------------

nsPrefLocalizedString::nsPrefLocalizedString() = default;

nsPrefLocalizedString::~nsPrefLocalizedString() = default;

//
// nsISupports Implementation
//

NS_IMPL_ADDREF(nsPrefLocalizedString)
NS_IMPL_RELEASE(nsPrefLocalizedString)

NS_INTERFACE_MAP_BEGIN(nsPrefLocalizedString)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefLocalizedString)
  NS_INTERFACE_MAP_ENTRY(nsIPrefLocalizedString)
  NS_INTERFACE_MAP_ENTRY(nsISupportsString)
NS_INTERFACE_MAP_END

nsresult
nsPrefLocalizedString::Init()
{
  nsresult rv;
  mUnicodeString = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);

  return rv;
}

//----------------------------------------------------------------------------
// nsRelativeFilePref
//----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsRelativeFilePref, nsIRelativeFilePref)

nsRelativeFilePref::nsRelativeFilePref() = default;

nsRelativeFilePref::~nsRelativeFilePref() = default;

NS_IMETHODIMP
nsRelativeFilePref::GetFile(nsIFile** aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = mFile;
  NS_IF_ADDREF(*aFile);
  return NS_OK;
}

NS_IMETHODIMP
nsRelativeFilePref::SetFile(nsIFile* aFile)
{
  mFile = aFile;
  return NS_OK;
}

NS_IMETHODIMP
nsRelativeFilePref::GetRelativeToKey(nsACString& aRelativeToKey)
{
  aRelativeToKey.Assign(mRelativeToKey);
  return NS_OK;
}

NS_IMETHODIMP
nsRelativeFilePref::SetRelativeToKey(const nsACString& aRelativeToKey)
{
  mRelativeToKey.Assign(aRelativeToKey);
  return NS_OK;
}

//===========================================================================
// Core prefs code
//===========================================================================

namespace mozilla {

#define INITIAL_PREF_FILES 10

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

void
Preferences::HandleDirty()
{
  if (!XRE_IsParentProcess()) {
    // TODO: this should really assert because you can't set prefs in a
    // content process. But so much code currently does this that we just
    // ignore it for now.
    return;
  }

  if (!gHashTable || !sPreferences) {
    return;
  }

  if (sPreferences->mProfileShutdown) {
    NS_WARNING("Setting user pref after profile shutdown.");
    return;
  }

  if (!sPreferences->mDirty) {
    sPreferences->mDirty = true;

    if (sPreferences->mCurrentFile && sPreferences->AllowOffMainThreadSave() &&
        !sPreferences->mSavePending) {
      sPreferences->mSavePending = true;
      static const int PREF_DELAY_MS = 500;
      NS_DelayedDispatchToCurrentThread(
        mozilla::NewRunnableMethod("Preferences::SavePrefFileAsynchronous",
                                   sPreferences,
                                   &Preferences::SavePrefFileAsynchronous),
        PREF_DELAY_MS);
    }
  }
}

static nsresult
openPrefFile(nsIFile* aFile);

static Result<Ok, const char*>
pref_InitInitialObjects();

static nsresult
pref_LoadPrefsInDirList(const char* aListId);

static const char kTelemetryPref[] = "toolkit.telemetry.enabled";
static const char kOldTelemetryPref[] = "toolkit.telemetry.enabledPreRelease";
static const char kChannelPref[] = "app.update.channel";

// clang-format off
static const char kPrefFileHeader[] =
  "# Mozilla User Preferences"
  NS_LINEBREAK
  NS_LINEBREAK
  "/* Do not edit this file."
  NS_LINEBREAK
  " *"
  NS_LINEBREAK
  " * If you make changes to this file while the application is running,"
  NS_LINEBREAK
  " * the changes will be overwritten when the application exits."
  NS_LINEBREAK
  " *"
  NS_LINEBREAK
  " * To make a manual change to preferences, you can visit the URL "
  "about:config"
  NS_LINEBREAK
  " */"
  NS_LINEBREAK
  NS_LINEBREAK;
// clang-format on

Preferences* Preferences::sPreferences = nullptr;
nsIPrefBranch* Preferences::sRootBranch = nullptr;
nsIPrefBranch* Preferences::sDefaultRootBranch = nullptr;
bool Preferences::sShutdown = false;

// This globally enables or disables OMT pref writing, both sync and async.
static int32_t sAllowOMTPrefWrite = -1;

class ValueObserverHashKey : public PLDHashEntryHdr
{
public:
  typedef ValueObserverHashKey* KeyType;
  typedef const ValueObserverHashKey* KeyTypePointer;

  static const ValueObserverHashKey* KeyToPointer(ValueObserverHashKey* aKey)
  {
    return aKey;
  }

  static PLDHashNumber HashKey(const ValueObserverHashKey* aKey)
  {
    PLDHashNumber hash = HashString(aKey->mPrefName);
    hash = AddToHash(hash, aKey->mMatchKind);
    return AddToHash(hash, aKey->mCallback);
  }

  ValueObserverHashKey(const char* aPref,
                       PrefChangedFunc aCallback,
                       Preferences::MatchKind aMatchKind)
    : mPrefName(aPref)
    , mCallback(aCallback)
    , mMatchKind(aMatchKind)
  {
  }

  explicit ValueObserverHashKey(const ValueObserverHashKey* aOther)
    : mPrefName(aOther->mPrefName)
    , mCallback(aOther->mCallback)
    , mMatchKind(aOther->mMatchKind)
  {
  }

  bool KeyEquals(const ValueObserverHashKey* aOther) const
  {
    return mCallback == aOther->mCallback && mPrefName == aOther->mPrefName &&
           mMatchKind == aOther->mMatchKind;
  }

  ValueObserverHashKey* GetKey() const
  {
    return const_cast<ValueObserverHashKey*>(this);
  }

  enum
  {
    ALLOW_MEMMOVE = true
  };

  nsCString mPrefName;
  PrefChangedFunc mCallback;
  Preferences::MatchKind mMatchKind;
};

class ValueObserver final
  : public nsIObserver
  , public ValueObserverHashKey
{
  ~ValueObserver() { Preferences::RemoveObserver(this, mPrefName.get()); }

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ValueObserver(const char* aPref,
                PrefChangedFunc aCallback,
                Preferences::MatchKind aMatchKind)
    : ValueObserverHashKey(aPref, aCallback, aMatchKind)
  {
  }

  void AppendClosure(void* aClosure) { mClosures.AppendElement(aClosure); }

  void RemoveClosure(void* aClosure) { mClosures.RemoveElement(aClosure); }

  bool HasNoClosures() { return mClosures.Length() == 0; }

  nsTArray<void*> mClosures;
};

NS_IMPL_ISUPPORTS(ValueObserver, nsIObserver)

NS_IMETHODIMP
ValueObserver::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const char16_t* aData)
{
  NS_ASSERTION(!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID),
               "invalid topic");

  NS_ConvertUTF16toUTF8 data(aData);
  if (mMatchKind == Preferences::ExactMatch &&
      !mPrefName.EqualsASCII(data.get())) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < mClosures.Length(); i++) {
    mCallback(data.get(), mClosures.ElementAt(i));
  }

  return NS_OK;
}

// Write the preference data to a file.
class PreferencesWriter final
{
public:
  PreferencesWriter() = default;

  static nsresult Write(nsIFile* aFile, PrefSaveData& aPrefs)
  {
    nsCOMPtr<nsIOutputStream> outStreamSink;
    nsCOMPtr<nsIOutputStream> outStream;
    uint32_t writeAmount;
    nsresult rv;

    // Execute a "safe" save by saving through a tempfile.
    rv = NS_NewSafeLocalFileOutputStream(
      getter_AddRefs(outStreamSink), aFile, -1, 0600);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = NS_NewBufferedOutputStream(
      getter_AddRefs(outStream), outStreamSink.forget(), 4096);
    if (NS_FAILED(rv)) {
      return rv;
    }

    struct CharComparator
    {
      bool LessThan(const nsCString& aA, const nsCString& aB) const
      {
        return aA < aB;
      }

      bool Equals(const nsCString& aA, const nsCString& aB) const
      {
        return aA == aB;
      }
    };

    // Sort the preferences to make a readable file on disk.
    aPrefs.Sort(CharComparator());

    // Write out the file header.
    outStream->Write(
      kPrefFileHeader, sizeof(kPrefFileHeader) - 1, &writeAmount);

    for (nsCString& pref : aPrefs) {
      outStream->Write(pref.get(), pref.Length(), &writeAmount);
      outStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &writeAmount);
    }

    // Tell the safe output stream to overwrite the real prefs file.
    // (It'll abort if there were any errors during writing.)
    nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(outStream);
    NS_ASSERTION(safeStream, "expected a safe output stream!");
    if (safeStream) {
      rv = safeStream->Finish();
    }

#ifdef DEBUG
    if (NS_FAILED(rv)) {
      NS_WARNING("failed to save prefs file! possible data loss");
    }
#endif

    return rv;
  }

  static void Flush()
  {
    // This can be further optimized; instead of waiting for all of the writer
    // thread to be available, we just have to wait for all the pending writes
    // to be done.
    if (!sPendingWriteData.compareExchange(nullptr, nullptr)) {
      nsresult rv = NS_OK;
      nsCOMPtr<nsIEventTarget> target =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv)) {
        target->Dispatch(NS_NewRunnableFunction("Preferences_dummy", [] {}),
                         nsIEventTarget::DISPATCH_SYNC);
      }
    }
  }

  // This is the data that all of the runnables (see below) will attempt
  // to write.  It will always have the most up to date version, or be
  // null, if the up to date information has already been written out.
  static Atomic<PrefSaveData*> sPendingWriteData;
};

Atomic<PrefSaveData*> PreferencesWriter::sPendingWriteData(nullptr);

class PWRunnable : public Runnable
{
public:
  explicit PWRunnable(nsIFile* aFile)
    : Runnable("PWRunnable")
    , mFile(aFile)
  {
  }

  NS_IMETHOD Run() override
  {
    // If we get a nullptr on the exchange, it means that somebody
    // else has already processed the request, and we can just return.
    mozilla::UniquePtr<PrefSaveData> prefs(
      PreferencesWriter::sPendingWriteData.exchange(nullptr));
    nsresult rv = NS_OK;
    if (prefs) {
      rv = PreferencesWriter::Write(mFile, *prefs);

      // Make a copy of these so we can have them in runnable lambda.
      // nsIFile is only there so that we would never release the
      // ref counted pointer off main thread.
      nsresult rvCopy = rv;
      nsCOMPtr<nsIFile> fileCopy(mFile);
      SystemGroup::Dispatch(
        TaskCategory::Other,
        NS_NewRunnableFunction("Preferences::WriterRunnable",
                               [fileCopy, rvCopy] {
                                 MOZ_RELEASE_ASSERT(NS_IsMainThread());
                                 if (NS_FAILED(rvCopy)) {
                                   Preferences::HandleDirty();
                                 }
                               }));
    }
    return rv;
  }

protected:
  nsCOMPtr<nsIFile> mFile;
};

struct CacheData
{
  void* mCacheLocation;
  union {
    bool mDefaultValueBool;
    int32_t mDefaultValueInt;
    uint32_t mDefaultValueUint;
    float mDefaultValueFloat;
  };
};

// gCacheDataDesc holds information about prefs startup. It's being used for
// diagnosing prefs startup problems in bug 1276488.
static const char* gCacheDataDesc = "untouched";
static nsTArray<nsAutoPtr<CacheData>>* gCacheData = nullptr;
static nsRefPtrHashtable<ValueObserverHashKey, ValueObserver>* gObserverTable =
  nullptr;

#ifdef DEBUG
static bool
HaveExistingCacheFor(void* aPtr)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (gCacheData) {
    for (size_t i = 0, count = gCacheData->Length(); i < count; ++i) {
      if ((*gCacheData)[i]->mCacheLocation == aPtr) {
        return true;
      }
    }
  }
  return false;
}

static void
AssertNotAlreadyCached(const char* aPrefType, const char* aPref, void* aPtr)
{
  if (HaveExistingCacheFor(aPtr)) {
    fprintf_stderr(
      stderr,
      "Attempt to add a %s pref cache for preference '%s' at address '%p'"
      "was made. However, a pref was already cached at this address.\n",
      aPrefType,
      aPref,
      aPtr);
    MOZ_ASSERT(false,
               "Should not have an existing pref cache for this address");
  }
}
#endif

static void
ReportToConsole(const char* aMessage, int aLine, bool aError)
{
  nsPrintfCString message("** Preference parsing %s (line %d) = %s **\n",
                          (aError ? "error" : "warning"),
                          aLine,
                          aMessage);
  nsPrefBranch::ReportToConsole(NS_ConvertUTF8toUTF16(message.get()));
}

// Although this is a member of Preferences, it measures sPreferences and
// several other global structures.
/* static */ int64_t
Preferences::SizeOfIncludingThisAndOtherStuff(
  mozilla::MallocSizeOf aMallocSizeOf)
{
  NS_ENSURE_TRUE(InitStaticMembers(), 0);

  size_t n = aMallocSizeOf(sPreferences);
  if (gHashTable) {
    // Pref keys are allocated in a private arena, which we count elsewhere.
    // Pref stringvals are allocated out of the same private arena.
    n += gHashTable->ShallowSizeOfIncludingThis(aMallocSizeOf);
  }

  if (gCacheData) {
    n += gCacheData->ShallowSizeOfIncludingThis(aMallocSizeOf);
    for (uint32_t i = 0, count = gCacheData->Length(); i < count; ++i) {
      n += aMallocSizeOf((*gCacheData)[i]);
    }
  }

  if (gObserverTable) {
    n += gObserverTable->ShallowSizeOfIncludingThis(aMallocSizeOf);
    for (auto iter = gObserverTable->Iter(); !iter.Done(); iter.Next()) {
      n += iter.Key()->mPrefName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
      n += iter.Data()->mClosures.ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
  }

  if (sRootBranch) {
    n += reinterpret_cast<nsPrefBranch*>(sRootBranch)
           ->SizeOfIncludingThis(aMallocSizeOf);
  }

  if (sDefaultRootBranch) {
    n += reinterpret_cast<nsPrefBranch*>(sDefaultRootBranch)
           ->SizeOfIncludingThis(aMallocSizeOf);
  }

  n += pref_SizeOfPrivateData(aMallocSizeOf);

  return n;
}

class PreferenceServiceReporter final : public nsIMemoryReporter
{
  ~PreferenceServiceReporter() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

protected:
  static const uint32_t kSuspectReferentCount = 1000;
};

NS_IMPL_ISUPPORTS(PreferenceServiceReporter, nsIMemoryReporter)

MOZ_DEFINE_MALLOC_SIZE_OF(PreferenceServiceMallocSizeOf)

NS_IMETHODIMP
PreferenceServiceReporter::CollectReports(
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aData,
  bool aAnonymize)
{
  MOZ_COLLECT_REPORT("explicit/preferences",
                     KIND_HEAP,
                     UNITS_BYTES,
                     Preferences::SizeOfIncludingThisAndOtherStuff(
                       PreferenceServiceMallocSizeOf),
                     "Memory used by the preferences system.");

  nsPrefBranch* rootBranch =
    static_cast<nsPrefBranch*>(Preferences::GetRootBranch());
  if (!rootBranch) {
    return NS_OK;
  }

  size_t numStrong = 0;
  size_t numWeakAlive = 0;
  size_t numWeakDead = 0;
  nsTArray<nsCString> suspectPreferences;
  // Count of the number of referents for each preference.
  nsDataHashtable<nsCStringHashKey, uint32_t> prefCounter;

  for (auto iter = rootBranch->mObservers.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<PrefCallback>& callback = iter.Data();
    nsPrefBranch* prefBranch = callback->GetPrefBranch();
    const auto& pref = prefBranch->GetPrefName(callback->GetDomain().get());

    if (callback->IsWeak()) {
      nsCOMPtr<nsIObserver> callbackRef = do_QueryReferent(callback->mWeakRef);
      if (callbackRef) {
        numWeakAlive++;
      } else {
        numWeakDead++;
      }
    } else {
      numStrong++;
    }

    nsDependentCString prefString(pref.get());
    uint32_t oldCount = 0;
    prefCounter.Get(prefString, &oldCount);
    uint32_t currentCount = oldCount + 1;
    prefCounter.Put(prefString, currentCount);

    // Keep track of preferences that have a suspiciously large number of
    // referents (a symptom of a leak).
    if (currentCount == kSuspectReferentCount) {
      suspectPreferences.AppendElement(prefString);
    }
  }

  for (uint32_t i = 0; i < suspectPreferences.Length(); i++) {
    nsCString& suspect = suspectPreferences[i];
    uint32_t totalReferentCount = 0;
    prefCounter.Get(suspect, &totalReferentCount);

    nsPrintfCString suspectPath("preference-service-suspect/"
                                "referent(pref=%s)",
                                suspect.get());

    aHandleReport->Callback(
      /* process = */ EmptyCString(),
      suspectPath,
      KIND_OTHER,
      UNITS_COUNT,
      totalReferentCount,
      NS_LITERAL_CSTRING(
        "A preference with a suspiciously large number referents (symptom of a "
        "leak)."),
      aData);
  }

  MOZ_COLLECT_REPORT(
    "preference-service/referent/strong",
    KIND_OTHER,
    UNITS_COUNT,
    numStrong,
    "The number of strong referents held by the preference service.");

  MOZ_COLLECT_REPORT(
    "preference-service/referent/weak/alive",
    KIND_OTHER,
    UNITS_COUNT,
    numWeakAlive,
    "The number of weak referents held by the preference service that are "
    "still alive.");

  MOZ_COLLECT_REPORT(
    "preference-service/referent/weak/dead",
    KIND_OTHER,
    UNITS_COUNT,
    numWeakDead,
    "The number of weak referents held by the preference service that are "
    "dead.");

  return NS_OK;
}

namespace {

class AddPreferencesMemoryReporterRunnable : public Runnable
{
public:
  AddPreferencesMemoryReporterRunnable()
    : Runnable("AddPreferencesMemoryReporterRunnable")
  {
  }

  NS_IMETHOD Run() override
  {
    return RegisterStrongMemoryReporter(new PreferenceServiceReporter());
  }
};

} // namespace

/* static */ already_AddRefed<Preferences>
Preferences::GetInstanceForService()
{
  if (sPreferences) {
    return do_AddRef(sPreferences);
  }

  if (sShutdown) {
    gCacheDataDesc = "shutting down in GetInstanceForService()";
    return nullptr;
  }

  sRootBranch = new nsPrefBranch("", false);
  NS_ADDREF(sRootBranch);
  sDefaultRootBranch = new nsPrefBranch("", true);
  NS_ADDREF(sDefaultRootBranch);

  sPreferences = new Preferences();
  NS_ADDREF(sPreferences);

  Result<Ok, const char*> res = sPreferences->Init();
  if (res.isErr()) {
    // The singleton instance will delete sRootBranch and sDefaultRootBranch.
    gCacheDataDesc = res.unwrapErr();
    NS_RELEASE(sPreferences);
    return nullptr;
  }

  gCacheData = new nsTArray<nsAutoPtr<CacheData>>();
  gCacheDataDesc = "set by GetInstanceForService()";

  gObserverTable = new nsRefPtrHashtable<ValueObserverHashKey, ValueObserver>();

  // Preferences::GetInstanceForService() can be called from GetService(), and
  // RegisterStrongMemoryReporter calls GetService(nsIMemoryReporter).  To
  // avoid a potential recursive GetService() call, we can't register the
  // memory reporter here; instead, do it off a runnable.
  RefPtr<AddPreferencesMemoryReporterRunnable> runnable =
    new AddPreferencesMemoryReporterRunnable();
  NS_DispatchToMainThread(runnable);

  return do_AddRef(sPreferences);
}

/* static */ bool
Preferences::IsServiceAvailable()
{
  return !!sPreferences;
}

/* static */ bool
Preferences::InitStaticMembers()
{
  MOZ_ASSERT(NS_IsMainThread() || mozilla::ServoStyleSet::IsInServoTraversal());

  if (!sShutdown && !sPreferences) {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIPrefService> prefService =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
  }

  return sPreferences != nullptr;
}

/* static */ void
Preferences::Shutdown()
{
  if (!sShutdown) {
    sShutdown = true; // Don't create the singleton instance after here.

    // Don't set sPreferences to nullptr here. The instance may be grabbed by
    // other modules. The utility methods of Preferences should be available
    // until the singleton instance actually released.
    if (sPreferences) {
      sPreferences->Release();
    }
  }
}

//-----------------------------------------------------------------------------

//
// Constructor/Destructor
//

Preferences::Preferences() = default;

Preferences::~Preferences()
{
  NS_ASSERTION(sPreferences == this, "Isn't this the singleton instance?");

  delete gObserverTable;
  gObserverTable = nullptr;

  delete gCacheData;
  gCacheData = nullptr;

  NS_RELEASE(sRootBranch);
  NS_RELEASE(sDefaultRootBranch);

  sPreferences = nullptr;

  PREF_Cleanup();
}

//
// nsISupports Implementation
//

NS_IMPL_ADDREF(Preferences)
NS_IMPL_RELEASE(Preferences)

NS_INTERFACE_MAP_BEGIN(Preferences)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefService)
  NS_INTERFACE_MAP_ENTRY(nsIPrefService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//
// nsIPrefService Implementation
//

static InfallibleTArray<Preferences::PrefSetting>* gInitPrefs;

/* static */ void
Preferences::SetInitPreferences(nsTArray<PrefSetting>* aPrefs)
{
  gInitPrefs = new InfallibleTArray<PrefSetting>(mozilla::Move(*aPrefs));
}

Result<Ok, const char*>
Preferences::Init()
{
  PREF_Init();

  MOZ_TRY(pref_InitInitialObjects());

  if (XRE_IsContentProcess()) {
    MOZ_ASSERT(gInitPrefs);
    for (unsigned int i = 0; i < gInitPrefs->Length(); i++) {
      Preferences::SetPreference(gInitPrefs->ElementAt(i));
    }
    delete gInitPrefs;
    gInitPrefs = nullptr;
    return Ok();
  }

  nsAutoCString lockFileName;

  // The following is a small hack which will allow us to only load the library
  // which supports the netscape.cfg file if the preference is defined. We
  // test for the existence of the pref, set in the all.js (mozilla) or
  // all-ns.js (netscape 6), and if it exists we startup the pref config
  // category which will do the rest.

  nsresult rv =
    PREF_GetCStringPref("general.config.filename", lockFileName, false);
  if (NS_SUCCEEDED(rv)) {
    NS_CreateServicesFromCategory(
      "pref-config-startup",
      static_cast<nsISupports*>(static_cast<void*>(this)),
      "pref-config-startup");
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return Err("GetObserverService() failed (1)");
  }

  observerService->AddObserver(this, "profile-before-change-telemetry", true);
  rv = observerService->AddObserver(this, "profile-before-change", true);

  observerService->AddObserver(this, "load-extension-defaults", true);
  observerService->AddObserver(this, "suspend_process_notification", true);

  if (NS_FAILED(rv)) {
    return Err("AddObserver(\"profile-before-change\") failed");
  }

  return Ok();
}

/* static */ void
Preferences::InitializeUserPrefs()
{
  MOZ_ASSERT(!sPreferences->mCurrentFile, "Should only initialize prefs once");

  // Prefs which are set before we initialize the profile are silently
  // discarded. This is stupid, but there are various tests which depend on
  // this behavior.
  sPreferences->ResetUserPrefs();

  nsCOMPtr<nsIFile> prefsFile = sPreferences->ReadSavedPrefs();
  sPreferences->ReadUserOverridePrefs();

  sPreferences->mDirty = false;

  // Don't set mCurrentFile until we're done so that dirty flags work properly.
  sPreferences->mCurrentFile = prefsFile.forget();

  // Migrate the old prerelease telemetry pref.
  if (!Preferences::GetBool(kOldTelemetryPref, true)) {
    Preferences::SetBool(kTelemetryPref, false);
    Preferences::ClearUser(kOldTelemetryPref);
  }

  sPreferences->NotifyServiceObservers(NS_PREFSERVICE_READ_TOPIC_ID);
}

NS_IMETHODIMP
Preferences::Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* someData)
{
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // Normally prefs aren't written after this point, and so we kick off
    // an asynchronous pref save so that I/O can be done in parallel with
    // other shutdown.
    if (AllowOffMainThreadSave()) {
      SavePrefFile(nullptr);
    }

  } else if (!nsCRT::strcmp(aTopic, "profile-before-change-telemetry")) {
    // It's possible that a profile-before-change observer after ours
    // set a pref. A blocking save here re-saves if necessary and also waits
    // for any pending saves to complete.
    SavePrefFileBlocking();
    MOZ_ASSERT(!mDirty, "Preferences should not be dirty");
    mProfileShutdown = true;

  } else if (!strcmp(aTopic, "load-extension-defaults")) {
    pref_LoadPrefsInDirList(NS_EXT_PREFS_DEFAULTS_DIR_LIST);

  } else if (!nsCRT::strcmp(aTopic, "reload-default-prefs")) {
    // Reload the default prefs from file.
    Unused << pref_InitInitialObjects();

  } else if (!nsCRT::strcmp(aTopic, "suspend_process_notification")) {
    // Our process is being suspended. The OS may wake our process later,
    // or it may kill the process. In case our process is going to be killed
    // from the suspended state, we save preferences before suspending.
    rv = SavePrefFileBlocking();
  }

  return rv;
}

NS_IMETHODIMP
Preferences::ReadUserPrefsFromFile(nsIFile* aFile)
{
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {
    NS_ERROR("must load prefs from parent process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!aFile) {
    NS_ERROR("ReadUserPrefsFromFile requires a parameter");
    return NS_ERROR_INVALID_ARG;
  }

  return openPrefFile(aFile);
}

NS_IMETHODIMP
Preferences::ResetPrefs()
{
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {
    NS_ERROR("must reset prefs from parent process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NotifyServiceObservers(NS_PREFSERVICE_RESET_TOPIC_ID);
  PREF_CleanupPrefs();

  PREF_Init();

  return pref_InitInitialObjects().isOk() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Preferences::ResetUserPrefs()
{
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {
    NS_ERROR("must reset user prefs from parent process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  PREF_ClearAllUserPrefs();
  return NS_OK;
}

bool
Preferences::AllowOffMainThreadSave()
{
  // Put in a preference that allows us to disable off main thread preference
  // file save.
  if (sAllowOMTPrefWrite < 0) {
    bool value = false;
    Preferences::GetBool("preferences.allow.omt-write", &value);
    sAllowOMTPrefWrite = value ? 1 : 0;
  }

  return !!sAllowOMTPrefWrite;
}

nsresult
Preferences::SavePrefFileBlocking()
{
  if (mDirty) {
    return SavePrefFileInternal(nullptr, SaveMethod::Blocking);
  }

  // If we weren't dirty to start, SavePrefFileInternal will early exit so
  // there is no guarantee that we don't have oustanding async saves in the
  // pipe. Since the contract of SavePrefFileOnMainThread is that the file on
  // disk matches the preferences, we have to make sure those requests are
  // completed.

  if (AllowOffMainThreadSave()) {
    PreferencesWriter::Flush();
  }

  return NS_OK;
}

nsresult
Preferences::SavePrefFileAsynchronous()
{
  return SavePrefFileInternal(nullptr, SaveMethod::Asynchronous);
}

NS_IMETHODIMP
Preferences::SavePrefFile(nsIFile* aFile)
{
  // This is the method accessible from service API. Make it off main thread.
  return SavePrefFileInternal(aFile, SaveMethod::Asynchronous);
}

static nsresult
ReadExtensionPrefs(nsIFile* aFile)
{
  nsresult rv;
  nsCOMPtr<nsIZipReader> reader = do_CreateInstance(kZipReaderCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = reader->Open(aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUTF8StringEnumerator> files;
  rv = reader->FindEntries(
    nsDependentCString("defaults/preferences/*.(J|j)(S|s)$"),
    getter_AddRefs(files));
  NS_ENSURE_SUCCESS(rv, rv);

  char buffer[4096];

  bool more;
  while (NS_SUCCEEDED(rv = files->HasMore(&more)) && more) {
    nsAutoCString entry;
    rv = files->GetNext(entry);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInputStream> stream;
    rv = reader->GetInputStream(entry, getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, rv);

    uint64_t avail;
    uint32_t read;

    PrefParseState ps;
    PREF_InitParseState(&ps, PREF_ReaderCallback, ReportToConsole, nullptr);
    while (NS_SUCCEEDED(rv = stream->Available(&avail)) && avail) {
      rv = stream->Read(buffer, 4096, &read);
      if (NS_FAILED(rv)) {
        NS_WARNING("Pref stream read failed");
        break;
      }

      PREF_ParseBuf(&ps, buffer, read);
    }
    PREF_FinalizeParseState(&ps);
  }

  return rv;
}

void
Preferences::SetPreference(const PrefSetting& aPref)
{
  pref_SetPref(aPref);
}

void
Preferences::GetPreference(PrefSetting* aPref)
{
  PrefHashEntry* entry = pref_HashTableLookup(aPref->name().get());
  if (!entry) {
    return;
  }

  if (pref_EntryHasAdvisablySizedValues(entry)) {
    pref_GetPrefFromEntry(entry, aPref);
  }
}

void
Preferences::GetPreferences(InfallibleTArray<PrefSetting>* aPrefs)
{
  aPrefs->SetCapacity(gHashTable->Capacity());
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PrefHashEntry*>(iter.Get());

    if (!pref_EntryHasAdvisablySizedValues(entry)) {
      continue;
    }

    dom::PrefSetting* pref = aPrefs->AppendElement();
    pref_GetPrefFromEntry(entry, pref);
  }
}

#ifdef DEBUG
void
Preferences::SetInitPhase(pref_initPhase aPhase)
{
  gPhase = aPhase;
}

pref_initPhase
Preferences::InitPhase()
{
  return gPhase;
}
#endif

NS_IMETHODIMP
Preferences::GetBranch(const char* aPrefRoot, nsIPrefBranch** aRetVal)
{
  if ((nullptr != aPrefRoot) && (*aPrefRoot != '\0')) {
    // TODO: Cache this stuff and allow consumers to share branches (hold weak
    // references, I think).
    RefPtr<nsPrefBranch> prefBranch = new nsPrefBranch(aPrefRoot, false);
    prefBranch.forget(aRetVal);
  } else {
    // Special case: caching the default root.
    nsCOMPtr<nsIPrefBranch> root(sRootBranch);
    root.forget(aRetVal);
  }

  return NS_OK;
}

NS_IMETHODIMP
Preferences::GetDefaultBranch(const char* aPrefRoot, nsIPrefBranch** aRetVal)
{
  if (!aPrefRoot || !aPrefRoot[0]) {
    nsCOMPtr<nsIPrefBranch> root(sDefaultRootBranch);
    root.forget(aRetVal);
    return NS_OK;
  }

  // TODO: Cache this stuff and allow consumers to share branches (hold weak
  // references, I think).
  RefPtr<nsPrefBranch> prefBranch = new nsPrefBranch(aPrefRoot, true);
  if (!prefBranch) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  prefBranch.forget(aRetVal);
  return NS_OK;
}

NS_IMETHODIMP
Preferences::GetDirty(bool* aRetVal)
{
  *aRetVal = mDirty;
  return NS_OK;
}

nsresult
Preferences::NotifyServiceObservers(const char* aTopic)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  auto subject = static_cast<nsIPrefService*>(this);
  observerService->NotifyObservers(subject, aTopic, nullptr);

  return NS_OK;
}

already_AddRefed<nsIFile>
Preferences::ReadSavedPrefs()
{
  nsCOMPtr<nsIFile> file;
  nsresult rv =
    NS_GetSpecialDirectory(NS_APP_PREFS_50_FILE, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  rv = openPrefFile(file);
  if (rv == NS_ERROR_FILE_NOT_FOUND) {
    // This is a normal case for new users.
    Telemetry::ScalarSet(
      Telemetry::ScalarID::PREFERENCES_CREATED_NEW_USER_PREFS_FILE, true);
    rv = NS_OK;
  } else if (NS_FAILED(rv)) {
    // Save a backup copy of the current (invalid) prefs file, since all prefs
    // from the error line to the end of the file will be lost (bug 361102).
    // TODO we should notify the user about it (bug 523725).
    Telemetry::ScalarSet(
      Telemetry::ScalarID::PREFERENCES_PREFS_FILE_WAS_INVALID, true);
    MakeBackupPrefFile(file);
  }

  return file.forget();
}

void
Preferences::ReadUserOverridePrefs()
{
  nsCOMPtr<nsIFile> aFile;
  nsresult rv =
    NS_GetSpecialDirectory(NS_APP_PREFS_50_DIR, getter_AddRefs(aFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  aFile->AppendNative(NS_LITERAL_CSTRING("user.js"));
  rv = openPrefFile(aFile);
  if (rv != NS_ERROR_FILE_NOT_FOUND) {
    // If the file exists and was at least partially read, record that in
    // telemetry as it may be a sign of pref injection.
    Telemetry::ScalarSet(Telemetry::ScalarID::PREFERENCES_READ_USER_JS, true);
  }
}

nsresult
Preferences::MakeBackupPrefFile(nsIFile* aFile)
{
  // Example: this copies "prefs.js" to "Invalidprefs.js" in the same directory.
  // "Invalidprefs.js" is removed if it exists, prior to making the copy.
  nsAutoString newFilename;
  nsresult rv = aFile->GetLeafName(newFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  newFilename.InsertLiteral(u"Invalid", 0);
  nsCOMPtr<nsIFile> newFile;
  rv = aFile->GetParent(getter_AddRefs(newFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = newFile->Append(newFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists = false;
  newFile->Exists(&exists);
  if (exists) {
    rv = newFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aFile->CopyTo(nullptr, newFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
Preferences::SavePrefFileInternal(nsIFile* aFile, SaveMethod aSaveMethod)
{
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {
    NS_ERROR("must save pref file from parent process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We allow different behavior here when aFile argument is not null, but it
  // happens to be the same as the current file.  It is not clear that we
  // should, but it does give us a "force" save on the unmodified pref file
  // (see the original bug 160377 when we added this.)

  if (nullptr == aFile) {
    mSavePending = false;

    // Off main thread writing only if allowed.
    if (!AllowOffMainThreadSave()) {
      aSaveMethod = SaveMethod::Blocking;
    }

    // The mDirty flag tells us if we should write to mCurrentFile. We only
    // check this flag when the caller wants to write to the default.
    if (!mDirty) {
      return NS_OK;
    }

    // Check for profile shutdown after mDirty because the runnables from
    // HandleDirty() can still be pending.
    if (mProfileShutdown) {
      NS_WARNING("Cannot save pref file after profile shutdown.");
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }

    // It's possible that we never got a prefs file.
    nsresult rv = NS_OK;
    if (mCurrentFile) {
      rv = WritePrefFile(mCurrentFile, aSaveMethod);
    }

    // If we succeeded writing to mCurrentFile, reset the dirty flag.
    if (NS_SUCCEEDED(rv)) {
      mDirty = false;
    }
    return rv;

  } else {
    // We only allow off main thread writes on mCurrentFile.
    return WritePrefFile(aFile, SaveMethod::Blocking);
  }
}

nsresult
Preferences::WritePrefFile(nsIFile* aFile, SaveMethod aSaveMethod)
{
  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AUTO_PROFILER_LABEL("Preferences::WritePrefFile", OTHER);

  if (AllowOffMainThreadSave()) {

    nsresult rv = NS_OK;
    mozilla::UniquePtr<PrefSaveData> prefs =
      MakeUnique<PrefSaveData>(pref_savePrefs());

    // Put the newly constructed preference data into sPendingWriteData
    // for the next request to pick up
    prefs.reset(PreferencesWriter::sPendingWriteData.exchange(prefs.release()));
    if (prefs) {
      // There was a previous request that hasn't been processed,
      // and this is the data it had.
      return rv;
    }

    // There were no previous requests. Dispatch one since sPendingWriteData has
    // the up to date information.
    nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      bool async = aSaveMethod == SaveMethod::Asynchronous;
      if (async) {
        rv = target->Dispatch(new PWRunnable(aFile),
                              nsIEventTarget::DISPATCH_NORMAL);
      } else {
        // Note that we don't get the nsresult return value here.
        SyncRunnable::DispatchToThread(target, new PWRunnable(aFile), true);
      }
      return rv;
    }

    // If we can't get the thread for writing, for whatever reason, do the main
    // thread write after making some noise.
    MOZ_ASSERT(false, "failed to get the target thread for OMT pref write");
  }

  // This will do a main thread write. It is safe to do it this way because
  // AllowOffMainThreadSave() returns a consistent value for the lifetime of
  // the parent process.
  PrefSaveData prefsData = pref_savePrefs();
  return PreferencesWriter::Write(aFile, prefsData);
}

static nsresult
openPrefFile(nsIFile* aFile)
{
  PrefParseState ps;
  PREF_InitParseState(&ps, PREF_ReaderCallback, ReportToConsole, nullptr);
  auto cleanup = MakeScopeExit([&]() { PREF_FinalizeParseState(&ps); });

  nsCString data;
  MOZ_TRY_VAR(data, URLPreloader::ReadFile(aFile));
  if (!PREF_ParseBuf(&ps, data.get(), data.Length())) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

//
// Some stuff that gets called from Pref_Init()
//

static int
pref_CompareFileNames(nsIFile* aFile1, nsIFile* aFile2, void* /* unused */)
{
  nsAutoCString filename1, filename2;
  aFile1->GetNativeLeafName(filename1);
  aFile2->GetNativeLeafName(filename2);

  return Compare(filename2, filename1);
}

// Load default pref files from a directory. The files in the directory are
// sorted reverse-alphabetically; a set of "special file names" may be
// specified which are loaded after all the others.
static nsresult
pref_LoadPrefsInDir(nsIFile* aDir,
                    char const* const* aSpecialFiles,
                    uint32_t aSpecialFilesCount)
{
  nsresult rv, rv2;
  bool hasMoreElements;

  nsCOMPtr<nsISimpleEnumerator> dirIterator;

  // This may fail in some normal cases, such as embedders who do not use a
  // GRE.
  rv = aDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
  if (NS_FAILED(rv)) {
    // If the directory doesn't exist, then we have no reason to complain. We
    // loaded everything (and nothing) successfully.
    if (rv == NS_ERROR_FILE_NOT_FOUND ||
        rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      rv = NS_OK;
    }
    return rv;
  }

  rv = dirIterator->HasMoreElements(&hasMoreElements);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsIFile> prefFiles(INITIAL_PREF_FILES);
  nsCOMArray<nsIFile> specialFiles(aSpecialFilesCount);
  nsCOMPtr<nsIFile> prefFile;

  while (hasMoreElements && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISupports> supports;
    rv = dirIterator->GetNext(getter_AddRefs(supports));
    prefFile = do_QueryInterface(supports);
    if (NS_FAILED(rv)) {
      break;
    }

    nsAutoCString leafName;
    prefFile->GetNativeLeafName(leafName);
    NS_ASSERTION(
      !leafName.IsEmpty(),
      "Failure in default prefs: directory enumerator returned empty file?");

    // Skip non-js files.
    if (StringEndsWith(leafName,
                       NS_LITERAL_CSTRING(".js"),
                       nsCaseInsensitiveCStringComparator())) {
      bool shouldParse = true;

      // Separate out special files.
      for (uint32_t i = 0; i < aSpecialFilesCount; ++i) {
        if (leafName.Equals(nsDependentCString(aSpecialFiles[i]))) {
          shouldParse = false;
          // Special files should be processed in order. We put them into the
          // array by index, which can make the array sparse.
          specialFiles.ReplaceObjectAt(prefFile, i);
        }
      }

      if (shouldParse) {
        prefFiles.AppendObject(prefFile);
      }
    }

    rv = dirIterator->HasMoreElements(&hasMoreElements);
  }

  if (prefFiles.Count() + specialFiles.Count() == 0) {
    NS_WARNING("No default pref files found.");
    if (NS_SUCCEEDED(rv)) {
      rv = NS_SUCCESS_FILE_DIRECTORY_EMPTY;
    }
    return rv;
  }

  prefFiles.Sort(pref_CompareFileNames, nullptr);

  uint32_t arrayCount = prefFiles.Count();
  uint32_t i;
  for (i = 0; i < arrayCount; ++i) {
    rv2 = openPrefFile(prefFiles[i]);
    if (NS_FAILED(rv2)) {
      NS_ERROR("Default pref file not parsed successfully.");
      rv = rv2;
    }
  }

  arrayCount = specialFiles.Count();
  for (i = 0; i < arrayCount; ++i) {
    // This may be a sparse array; test before parsing.
    nsIFile* file = specialFiles[i];
    if (file) {
      rv2 = openPrefFile(file);
      if (NS_FAILED(rv2)) {
        NS_ERROR("Special default pref file not parsed successfully.");
        rv = rv2;
      }
    }
  }

  return rv;
}

static nsresult
pref_LoadPrefsInDirList(const char* aListId)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> dirSvc(
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISimpleEnumerator> list;
  dirSvc->Get(aListId, NS_GET_IID(nsISimpleEnumerator), getter_AddRefs(list));
  if (!list) {
    return NS_OK;
  }

  bool hasMore;
  while (NS_SUCCEEDED(list->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    list->GetNext(getter_AddRefs(elem));
    if (!elem) {
      continue;
    }

    nsCOMPtr<nsIFile> path = do_QueryInterface(elem);
    if (!path) {
      continue;
    }

    nsAutoCString leaf;
    path->GetNativeLeafName(leaf);

    // Do we care if a file provided by this process fails to load?
    if (Substring(leaf, leaf.Length() - 4).EqualsLiteral(".xpi")) {
      ReadExtensionPrefs(path);
    } else {
      pref_LoadPrefsInDir(path, nullptr, 0);
    }
  }

  return NS_OK;
}

static nsresult
pref_ReadPrefFromJar(nsZipArchive* aJarReader, const char* aName)
{
  nsCString manifest;
  MOZ_TRY_VAR(manifest,
              URLPreloader::ReadZip(aJarReader, nsDependentCString(aName)));

  PrefParseState ps;
  PREF_InitParseState(&ps, PREF_ReaderCallback, ReportToConsole, nullptr);
  PREF_ParseBuf(&ps, manifest.get(), manifest.Length());
  PREF_FinalizeParseState(&ps);

  return NS_OK;
}

// Initialize default preference JavaScript buffers from appropriate TEXT
// resources.
static Result<Ok, const char*>
pref_InitInitialObjects()
{
  // In the omni.jar case, we load the following prefs:
  // - jar:$gre/omni.jar!/greprefs.js
  // - jar:$gre/omni.jar!/defaults/pref/*.js
  //
  // In the non-omni.jar case, we load:
  // - $gre/greprefs.js
  //
  // In both cases, we also load:
  // - $gre/defaults/pref/*.js
  //
  // This is kept for bug 591866 (channel-prefs.js should not be in omni.jar)
  // in the `$app == $gre` case; we load all files instead of channel-prefs.js
  // only to have the same behaviour as `$app != $gre`, where this is required
  // as a supported location for GRE preferences.
  //
  // When `$app != $gre`, we additionally load, in the omni.jar case:
  // - jar:$app/omni.jar!/defaults/preferences/*.js
  // - $app/defaults/preferences/*.js
  //
  // and in the non-omni.jar case:
  // - $app/defaults/preferences/*.js
  //
  // When `$app == $gre`, we additionally load, in the omni.jar case:
  // - jar:$gre/omni.jar!/defaults/preferences/*.js
  //
  // Thus, in the omni.jar case, we always load app-specific default
  // preferences from omni.jar, whether or not `$app == $gre`.

  nsresult rv;
  nsZipFind* findPtr;
  nsAutoPtr<nsZipFind> find;
  nsTArray<nsCString> prefEntries;
  const char* entryName;
  uint16_t entryNameLen;

  RefPtr<nsZipArchive> jarReader =
    mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
  if (jarReader) {
    // Load jar:$gre/omni.jar!/greprefs.js.
    rv = pref_ReadPrefFromJar(jarReader, "greprefs.js");
    NS_ENSURE_SUCCESS(rv, Err("pref_ReadPrefFromJar() failed"));

    // Load jar:$gre/omni.jar!/defaults/pref/*.js.
    rv = jarReader->FindInit("defaults/pref/*.js$", &findPtr);
    NS_ENSURE_SUCCESS(rv, Err("jarReader->FindInit() failed"));

    find = findPtr;
    while (NS_SUCCEEDED(find->FindNext(&entryName, &entryNameLen))) {
      prefEntries.AppendElement(Substring(entryName, entryNameLen));
    }

    prefEntries.Sort();
    for (uint32_t i = prefEntries.Length(); i--;) {
      rv = pref_ReadPrefFromJar(jarReader, prefEntries[i].get());
      if (NS_FAILED(rv)) {
        NS_WARNING("Error parsing preferences.");
      }
    }

  } else {
    // Load $gre/greprefs.js.
    nsCOMPtr<nsIFile> greprefsFile;
    rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(greprefsFile));
    NS_ENSURE_SUCCESS(rv, Err("NS_GetSpecialDirectory(NS_GRE_DIR) failed"));

    rv = greprefsFile->AppendNative(NS_LITERAL_CSTRING("greprefs.js"));
    NS_ENSURE_SUCCESS(rv, Err("greprefsFile->AppendNative() failed"));

    rv = openPrefFile(greprefsFile);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error parsing GRE default preferences. Is this an old-style "
                 "embedding app?");
    }
  }

  // Load $gre/defaults/pref/*.js.
  nsCOMPtr<nsIFile> defaultPrefDir;
  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR,
                              getter_AddRefs(defaultPrefDir));
  NS_ENSURE_SUCCESS(
    rv, Err("NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR) failed"));

  // These pref file names should not be used: we process them after all other
  // application pref files for backwards compatibility.
  static const char* specialFiles[] = {
#if defined(XP_MACOSX)
    "macprefs.js"
#elif defined(XP_WIN)
    "winpref.js"
#elif defined(XP_UNIX)
    "unix.js"
#if defined(_AIX)
    ,
    "aix.js"
#endif
#elif defined(XP_BEOS)
    "beos.js"
#endif
  };

  rv = pref_LoadPrefsInDir(
    defaultPrefDir, specialFiles, ArrayLength(specialFiles));
  if (NS_FAILED(rv)) {
    NS_WARNING("Error parsing application default preferences.");
  }

  // Load jar:$app/omni.jar!/defaults/preferences/*.js
  // or jar:$gre/omni.jar!/defaults/preferences/*.js.
  RefPtr<nsZipArchive> appJarReader =
    mozilla::Omnijar::GetReader(mozilla::Omnijar::APP);

  // GetReader(mozilla::Omnijar::APP) returns null when `$app == $gre`, in
  // which case we look for app-specific default preferences in $gre.
  if (!appJarReader) {
    appJarReader = mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
  }

  if (appJarReader) {
    rv = appJarReader->FindInit("defaults/preferences/*.js$", &findPtr);
    NS_ENSURE_SUCCESS(rv, Err("appJarReader->FindInit() failed"));
    find = findPtr;
    prefEntries.Clear();
    while (NS_SUCCEEDED(find->FindNext(&entryName, &entryNameLen))) {
      prefEntries.AppendElement(Substring(entryName, entryNameLen));
    }
    prefEntries.Sort();
    for (uint32_t i = prefEntries.Length(); i--;) {
      rv = pref_ReadPrefFromJar(appJarReader, prefEntries[i].get());
      if (NS_FAILED(rv)) {
        NS_WARNING("Error parsing preferences.");
      }
    }
  }

  rv = pref_LoadPrefsInDirList(NS_APP_PREFS_DEFAULTS_DIR_LIST);
  NS_ENSURE_SUCCESS(
    rv, Err("pref_LoadPrefsInDirList(NS_APP_PREFS_DEFAULTS_DIR_LIST) failed"));

#ifdef MOZ_WIDGET_ANDROID
  // Set up the correct default for toolkit.telemetry.enabled. If this build
  // has MOZ_TELEMETRY_ON_BY_DEFAULT *or* we're on the beta channel, telemetry
  // is on by default, otherwise not. This is necessary so that beta users who
  // are testing final release builds don't flipflop defaults.
  if (Preferences::GetDefaultType(kTelemetryPref) ==
      nsIPrefBranch::PREF_INVALID) {
    bool prerelease = false;
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
    prerelease = true;
#else
    nsAutoCString prefValue;
    Preferences::GetDefaultCString(kChannelPref, prefValue);
    if (prefValue.EqualsLiteral("beta")) {
      prerelease = true;
    }
#endif
    PREF_SetBoolPref(kTelemetryPref, prerelease, true);
  }
#else
  // For platforms with Unified Telemetry (here meaning not-Android),
  // toolkit.telemetry.enabled determines whether we send "extended" data.
  // We only want extended data from pre-release channels due to size. We
  // also want it to be recorded for local developer builds (non-official builds
  // on the "default" channel).
  bool developerBuild = false;
#ifndef MOZILLA_OFFICIAL
  developerBuild = !strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "default");
#endif

  if (!strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "nightly") ||
      !strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "aurora") ||
      !strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "beta") ||
      developerBuild) {
    PREF_SetBoolPref(kTelemetryPref, true, true);
  } else {
    PREF_SetBoolPref(kTelemetryPref, false, true);
  }
  PREF_LockPref(kTelemetryPref, true);
#endif // MOZ_WIDGET_ANDROID

  NS_CreateServicesFromCategory(NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID,
                                nullptr,
                                NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  NS_ENSURE_SUCCESS(rv, Err("GetObserverService() failed (2)"));

  observerService->NotifyObservers(
    nullptr, NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID, nullptr);

  rv = pref_LoadPrefsInDirList(NS_EXT_PREFS_DEFAULTS_DIR_LIST);
  NS_ENSURE_SUCCESS(
    rv, Err("pref_LoadPrefsInDirList(NS_EXT_PREFS_DEFAULTS_DIR_LIST) failed"));

  return Ok();
}

//----------------------------------------------------------------------------
// Static utilities
//----------------------------------------------------------------------------

/* static */ nsresult
Preferences::GetBool(const char* aPref, bool* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetBoolPref(aPref, aResult, false);
}

/* static */ nsresult
Preferences::GetInt(const char* aPref, int32_t* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetIntPref(aPref, aResult, false);
}

/* static */ nsresult
Preferences::GetFloat(const char* aPref, float* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_GetCStringPref(aPref, result, false);
  if (NS_SUCCEEDED(rv)) {
    *aResult = result.ToFloat(&rv);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetCString(const char* aPref, nsACString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetCStringPref(aPref, aResult, false);
}

/* static */ nsresult
Preferences::GetString(const char* aPref, nsAString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_GetCStringPref(aPref, result, false);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(result, aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetLocalizedCString(const char* aPref, nsACString& aResult)
{
  nsAutoString result;
  nsresult rv = GetLocalizedString(aPref, result);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF16toUTF8(result, aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetLocalizedString(const char* aPref, nsAString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
  nsresult rv = sRootBranch->GetComplexValue(
    aPref, NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(prefLocalString));
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(prefLocalString, "Succeeded but the result is NULL");
    prefLocalString->GetData(aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetComplex(const char* aPref, const nsIID& aType, void** aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->GetComplexValue(aPref, aType, aResult);
}

/* static */ nsresult
Preferences::SetCString(const char* aPref, const char* aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetCString", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCStringPref(aPref, nsDependentCString(aValue), false);
}

/* static */ nsresult
Preferences::SetCString(const char* aPref, const nsACString& aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetCString", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCStringPref(aPref, aValue, false);
}

/* static */ nsresult
Preferences::SetString(const char* aPref, const char16ptr_t aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetString", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCStringPref(aPref, NS_ConvertUTF16toUTF8(aValue), false);
}

/* static */ nsresult
Preferences::SetString(const char* aPref, const nsAString& aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetString", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCStringPref(aPref, NS_ConvertUTF16toUTF8(aValue), false);
}

/* static */ nsresult
Preferences::SetBool(const char* aPref, bool aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetBool", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetBoolPref(aPref, aValue, false);
}

/* static */ nsresult
Preferences::SetInt(const char* aPref, int32_t aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetInt", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetIntPref(aPref, aValue, false);
}

/* static */ nsresult
Preferences::SetFloat(const char* aPref, float aValue)
{
  return SetCString(aPref, nsPrintfCString("%f", aValue).get());
}

/* static */ nsresult
Preferences::SetComplex(const char* aPref,
                        const nsIID& aType,
                        nsISupports* aValue)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->SetComplexValue(aPref, aType, aValue);
}

/* static */ nsresult
Preferences::ClearUser(const char* aPref)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("ClearUser", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_ClearUserPref(aPref);
}

/* static */ bool
Preferences::HasUserValue(const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), false);
  return PREF_HasUserPref(aPref);
}

/* static */ int32_t
Preferences::GetType(const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), nsIPrefBranch::PREF_INVALID);
  int32_t result;
  return NS_SUCCEEDED(sRootBranch->GetPrefType(aPref, &result))
           ? result
           : nsIPrefBranch::PREF_INVALID;
}

/* static */ nsresult
Preferences::AddStrongObserver(nsIObserver* aObserver, const char* aPref)
{
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->AddObserver(aPref, aObserver, false);
}

/* static */ nsresult
Preferences::AddWeakObserver(nsIObserver* aObserver, const char* aPref)
{
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->AddObserver(aPref, aObserver, true);
}

/* static */ nsresult
Preferences::RemoveObserver(nsIObserver* aObserver, const char* aPref)
{
  MOZ_ASSERT(aObserver);
  if (!sPreferences && sShutdown) {
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);
  return sRootBranch->RemoveObserver(aPref, aObserver);
}

/* static */ nsresult
Preferences::AddStrongObservers(nsIObserver* aObserver, const char** aPrefs)
{
  MOZ_ASSERT(aObserver);
  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = AddStrongObserver(aObserver, aPrefs[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/* static */ nsresult
Preferences::AddWeakObservers(nsIObserver* aObserver, const char** aPrefs)
{
  MOZ_ASSERT(aObserver);
  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = AddWeakObserver(aObserver, aPrefs[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/* static */ nsresult
Preferences::RemoveObservers(nsIObserver* aObserver, const char** aPrefs)
{
  MOZ_ASSERT(aObserver);
  if (!sPreferences && sShutdown) {
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = RemoveObserver(aObserver, aPrefs[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

static void
NotifyObserver(const char* aPref, void* aClosure)
{
  nsCOMPtr<nsIObserver> observer = static_cast<nsIObserver*>(aClosure);
  observer->Observe(nullptr,
                    NS_PREFBRANCH_PREFCHANGE_TOPIC_ID,
                    NS_ConvertASCIItoUTF16(aPref).get());
}

static void
RegisterPriorityCallback(PrefChangedFunc aCallback,
                         const char* aPref,
                         void* aClosure)
{
  MOZ_ASSERT(Preferences::IsServiceAvailable());

  ValueObserverHashKey hashKey(aPref, aCallback, Preferences::ExactMatch);
  RefPtr<ValueObserver> observer;
  gObserverTable->Get(&hashKey, getter_AddRefs(observer));
  if (observer) {
    observer->AppendClosure(aClosure);
    return;
  }

  observer = new ValueObserver(aPref, aCallback, Preferences::ExactMatch);
  observer->AppendClosure(aClosure);
  PREF_RegisterCallback(aPref,
                        NotifyObserver,
                        static_cast<nsIObserver*>(observer),
                        /* isPriority */ true);
  gObserverTable->Put(observer, observer);
}

/* static */ nsresult
Preferences::RegisterCallback(PrefChangedFunc aCallback,
                              const char* aPref,
                              void* aClosure,
                              MatchKind aMatchKind)
{
  MOZ_ASSERT(aCallback);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  ValueObserverHashKey hashKey(aPref, aCallback, aMatchKind);
  RefPtr<ValueObserver> observer;
  gObserverTable->Get(&hashKey, getter_AddRefs(observer));
  if (observer) {
    observer->AppendClosure(aClosure);
    return NS_OK;
  }

  observer = new ValueObserver(aPref, aCallback, aMatchKind);
  observer->AppendClosure(aClosure);
  nsresult rv = AddStrongObserver(observer, aPref);
  NS_ENSURE_SUCCESS(rv, rv);

  gObserverTable->Put(observer, observer);
  return NS_OK;
}

/* static */ nsresult
Preferences::RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                     const char* aPref,
                                     void* aClosure,
                                     MatchKind aMatchKind)
{
  MOZ_ASSERT(aCallback);
  WATCHING_PREF_RAII();
  nsresult rv = RegisterCallback(aCallback, aPref, aClosure, aMatchKind);
  if (NS_SUCCEEDED(rv)) {
    (*aCallback)(aPref, aClosure);
  }
  return rv;
}

/* static */ nsresult
Preferences::UnregisterCallback(PrefChangedFunc aCallback,
                                const char* aPref,
                                void* aClosure,
                                MatchKind aMatchKind)
{
  MOZ_ASSERT(aCallback);
  if (!sPreferences && sShutdown) {
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  ValueObserverHashKey hashKey(aPref, aCallback, aMatchKind);
  RefPtr<ValueObserver> observer;
  gObserverTable->Get(&hashKey, getter_AddRefs(observer));
  if (!observer) {
    return NS_OK;
  }

  observer->RemoveClosure(aClosure);
  if (observer->HasNoClosures()) {
    // Delete the callback since its list of closures is empty.
    gObserverTable->Remove(observer);
  }
  return NS_OK;
}

// We insert cache observers using RegisterPriorityCallback to ensure they are
// called prior to ordinary pref observers. Doing this ensures that ordinary
// observers will never get stale values from cache variables.

static void
BoolVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<bool*>(cache->mCacheLocation) =
    Preferences::GetBool(aPref, cache->mDefaultValueBool);
}

static void
CacheDataAppendElement(CacheData* aData)
{
  if (!gCacheData) {
    MOZ_CRASH_UNSAFE_PRINTF("!gCacheData: %s", gCacheDataDesc);
  }
  gCacheData->AppendElement(aData);
}

/* static */ nsresult
Preferences::AddBoolVarCache(bool* aCache, const char* aPref, bool aDefault)
{
  WATCHING_PREF_RAII();
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("bool", aPref, aCache);
#endif
  *aCache = GetBool(aPref, aDefault);
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueBool = aDefault;
  CacheDataAppendElement(data);
  RegisterPriorityCallback(BoolVarChanged, aPref, data);
  return NS_OK;
}

static void
IntVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<int32_t*>(cache->mCacheLocation) =
    Preferences::GetInt(aPref, cache->mDefaultValueInt);
}

/* static */ nsresult
Preferences::AddIntVarCache(int32_t* aCache,
                            const char* aPref,
                            int32_t aDefault)
{
  WATCHING_PREF_RAII();
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("int", aPref, aCache);
#endif
  *aCache = Preferences::GetInt(aPref, aDefault);
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueInt = aDefault;
  CacheDataAppendElement(data);
  RegisterPriorityCallback(IntVarChanged, aPref, data);
  return NS_OK;
}

static void
UintVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<uint32_t*>(cache->mCacheLocation) =
    Preferences::GetUint(aPref, cache->mDefaultValueUint);
}

/* static */ nsresult
Preferences::AddUintVarCache(uint32_t* aCache,
                             const char* aPref,
                             uint32_t aDefault)
{
  WATCHING_PREF_RAII();
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("uint", aPref, aCache);
#endif
  *aCache = Preferences::GetUint(aPref, aDefault);
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueUint = aDefault;
  CacheDataAppendElement(data);
  RegisterPriorityCallback(UintVarChanged, aPref, data);
  return NS_OK;
}

template<MemoryOrdering Order>
static void
AtomicUintVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<Atomic<uint32_t, Order>*>(cache->mCacheLocation) =
    Preferences::GetUint(aPref, cache->mDefaultValueUint);
}

template<MemoryOrdering Order>
/* static */ nsresult
Preferences::AddAtomicUintVarCache(Atomic<uint32_t, Order>* aCache,
                                   const char* aPref,
                                   uint32_t aDefault)
{
  WATCHING_PREF_RAII();
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("uint", aPref, aCache);
#endif
  *aCache = Preferences::GetUint(aPref, aDefault);
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueUint = aDefault;
  CacheDataAppendElement(data);
  RegisterPriorityCallback(AtomicUintVarChanged<Order>, aPref, data);
  return NS_OK;
}

// Since the definition of this template function is not in a header file, we
// need to explicitly specify the instantiations that are required. Currently
// only the order=Relaxed variant is needed.
template nsresult
Preferences::AddAtomicUintVarCache(Atomic<uint32_t, Relaxed>*,
                                   const char*,
                                   uint32_t);

static void
FloatVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<float*>(cache->mCacheLocation) =
    Preferences::GetFloat(aPref, cache->mDefaultValueFloat);
}

/* static */ nsresult
Preferences::AddFloatVarCache(float* aCache, const char* aPref, float aDefault)
{
  WATCHING_PREF_RAII();
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("float", aPref, aCache);
#endif
  *aCache = Preferences::GetFloat(aPref, aDefault);
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueFloat = aDefault;
  CacheDataAppendElement(data);
  RegisterPriorityCallback(FloatVarChanged, aPref, data);
  return NS_OK;
}

/* static */ nsresult
Preferences::GetDefaultBool(const char* aPref, bool* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetBoolPref(aPref, aResult, true);
}

/* static */ nsresult
Preferences::GetDefaultInt(const char* aPref, int32_t* aResult)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetIntPref(aPref, aResult, true);
}

/* static */ nsresult
Preferences::GetDefaultCString(const char* aPref, nsACString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_GetCStringPref(aPref, aResult, true);
}

/* static */ nsresult
Preferences::GetDefaultString(const char* aPref, nsAString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_GetCStringPref(aPref, result, true);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(result, aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetDefaultLocalizedCString(const char* aPref, nsACString& aResult)
{
  nsAutoString result;
  nsresult rv = GetDefaultLocalizedString(aPref, result);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF16toUTF8(result, aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetDefaultLocalizedString(const char* aPref, nsAString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
  nsresult rv = sDefaultRootBranch->GetComplexValue(
    aPref, NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(prefLocalString));
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(prefLocalString, "Succeeded but the result is NULL");
    prefLocalString->GetData(aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetDefaultComplex(const char* aPref,
                               const nsIID& aType,
                               void** aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sDefaultRootBranch->GetComplexValue(aPref, aType, aResult);
}

/* static */ int32_t
Preferences::GetDefaultType(const char* aPref)
{
  NS_ENSURE_TRUE(InitStaticMembers(), nsIPrefBranch::PREF_INVALID);
  int32_t result;
  return NS_SUCCEEDED(sDefaultRootBranch->GetPrefType(aPref, &result))
           ? result
           : nsIPrefBranch::PREF_INVALID;
}

} // namespace mozilla

#undef ENSURE_MAIN_PROCESS
#undef ENSURE_MAIN_PROCESS_WITH_WARNING

//===========================================================================
// Module and factory stuff
//===========================================================================

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(Preferences,
                                         Preferences::GetInstanceForService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrefLocalizedString, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRelativeFilePref)

static NS_DEFINE_CID(kPrefServiceCID, NS_PREFSERVICE_CID);
static NS_DEFINE_CID(kPrefLocalizedStringCID, NS_PREFLOCALIZEDSTRING_CID);
static NS_DEFINE_CID(kRelativeFilePrefCID, NS_RELATIVEFILEPREF_CID);

static mozilla::Module::CIDEntry kPrefCIDs[] = {
  { &kPrefServiceCID, true, nullptr, PreferencesConstructor },
  { &kPrefLocalizedStringCID,
    false,
    nullptr,
    nsPrefLocalizedStringConstructor },
  { &kRelativeFilePrefCID, false, nullptr, nsRelativeFilePrefConstructor },
  { nullptr }
};

static mozilla::Module::ContractIDEntry kPrefContracts[] = {
  { NS_PREFSERVICE_CONTRACTID, &kPrefServiceCID },
  { NS_PREFLOCALIZEDSTRING_CONTRACTID, &kPrefLocalizedStringCID },
  { NS_RELATIVEFILEPREF_CONTRACTID, &kRelativeFilePrefCID },
  // compatibility for extension that uses old service
  { "@mozilla.org/preferences;1", &kPrefServiceCID },
  { nullptr }
};

static void
UnloadPrefsModule()
{
  Preferences::Shutdown();
}

static const mozilla::Module kPrefModule = { mozilla::Module::kVersion,
                                             kPrefCIDs,
                                             kPrefContracts,
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             UnloadPrefsModule };

NSMODULE_DEFN(nsPrefModule) = &kPrefModule;
