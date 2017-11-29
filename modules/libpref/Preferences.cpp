/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
#include "mozilla/Maybe.h"
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
#include "mozilla/Vector.h"
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

#ifdef XP_WIN
#include "windows.h"
#endif

using namespace mozilla;

#ifdef DEBUG

#define ENSURE_PARENT_PROCESS(func, pref)                                      \
  do {                                                                         \
    if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                \
      nsPrintfCString msg(                                                     \
        "ENSURE_PARENT_PROCESS: called %s on %s in a non-parent process",      \
        func,                                                                  \
        pref);                                                                 \
      NS_ERROR(msg.get());                                                     \
      return NS_ERROR_NOT_AVAILABLE;                                           \
    }                                                                          \
  } while (0)

#else // DEBUG

#define ENSURE_PARENT_PROCESS(func, pref)                                      \
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                  \
    return NS_ERROR_NOT_AVAILABLE;                                             \
  }

#endif // DEBUG

//===========================================================================
// Low-level types and operations
//===========================================================================

typedef nsTArray<nsCString> PrefSaveData;

// 1 MB should be enough for everyone.
static const uint32_t MAX_PREF_LENGTH = 1 * 1024 * 1024;
// Actually, 4kb should be enough for everyone.
static const uint32_t MAX_ADVISABLE_PREF_LENGTH = 4 * 1024;

enum class PrefType : uint8_t
{
  None = 0, // only used when neither the default nor user value is set
  String = 1,
  Int = 2,
  Bool = 3,
};

union PrefValue {
  const char* mStringVal;
  int32_t mIntVal;
  bool mBoolVal;

  bool Equals(PrefType aType, PrefValue aValue)
  {
    switch (aType) {
      case PrefType::String: {
        if (mStringVal && aValue.mStringVal) {
          return strcmp(mStringVal, aValue.mStringVal) == 0;
        }
        if (!mStringVal && !aValue.mStringVal) {
          return true;
        }
        return false;
      }

      case PrefType::Int:
        return mIntVal == aValue.mIntVal;

      case PrefType::Bool:
        return mBoolVal == aValue.mBoolVal;

      default:
        MOZ_CRASH("Unhandled enum value");
    }
  }

  void Init(PrefType aNewType, PrefValue aNewValue)
  {
    if (aNewType == PrefType::String) {
      MOZ_ASSERT(aNewValue.mStringVal);
      aNewValue.mStringVal = moz_xstrdup(aNewValue.mStringVal);
    }
    *this = aNewValue;
  }

  void Clear(PrefType aType)
  {
    if (aType == PrefType::String) {
      free(const_cast<char*>(mStringVal));
      mStringVal = nullptr;
    }
  }

  void Replace(PrefType aOldType, PrefType aNewType, PrefValue aNewValue)
  {
    Clear(aOldType);
    Init(aNewType, aNewValue);
  }

  void ToDomPrefValue(PrefType aType, dom::PrefValue* aDomValue)
  {
    switch (aType) {
      case PrefType::String:
        *aDomValue = nsDependentCString(mStringVal);
        return;

      case PrefType::Int:
        *aDomValue = mIntVal;
        return;

      case PrefType::Bool:
        *aDomValue = mBoolVal;
        return;

      default:
        MOZ_CRASH();
    }
  }

  PrefType FromDomPrefValue(const dom::PrefValue& aDomValue)
  {
    switch (aDomValue.type()) {
      case dom::PrefValue::TnsCString:
        mStringVal = aDomValue.get_nsCString().get();
        return PrefType::String;

      case dom::PrefValue::Tint32_t:
        mIntVal = aDomValue.get_int32_t();
        return PrefType::Int;

      case dom::PrefValue::Tbool:
        mBoolVal = aDomValue.get_bool();
        return PrefType::Bool;

      default:
        MOZ_CRASH();
    }
  }
};

#ifdef DEBUG
const char*
PrefTypeToString(PrefType aType)
{
  switch (aType) {
    case PrefType::String:
      return "string";
    case PrefType::Int:
      return "int";
    case PrefType::Bool:
      return "bool";
    default:
      MOZ_CRASH("Unhandled enum value");
  }
}
#endif

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

static ArenaAllocator<8192, 1> gPrefNameArena;

class Pref : public PLDHashEntryHdr
{
public:
  explicit Pref(const char* aName)
  {
    mName = ArenaStrdup(aName, gPrefNameArena);

    // We don't set the other fields because PLDHashTable always zeroes new
    // entries.
  }

  ~Pref()
  {
    // There's no need to free mName because it's allocated in memory owned by
    // gPrefNameArena.

    mDefaultValue.Clear(Type());
    mUserValue.Clear(Type());
  }

  const char* Name() { return mName; }

  // Types.

  PrefType Type() const { return static_cast<PrefType>(mType); }
  void SetType(PrefType aType) { mType = static_cast<uint32_t>(aType); }

  bool IsType(PrefType aType) const { return Type() == aType; }
  bool IsTypeString() const { return IsType(PrefType::String); }
  bool IsTypeInt() const { return IsType(PrefType::Int); }
  bool IsTypeBool() const { return IsType(PrefType::Bool); }

  // Other properties.

  bool IsLocked() const { return mIsLocked; }
  void SetIsLocked(bool aValue) { mIsLocked = aValue; }

  bool HasDefaultValue() const { return mHasDefaultValue; }
  bool HasUserValue() const { return mHasUserValue; }

  // Other operations.

  static bool MatchEntry(const PLDHashEntryHdr* aEntry, const void* aKey)
  {
    auto pref = static_cast<const Pref*>(aEntry);
    auto key = static_cast<const char*>(aKey);

    if (pref->mName == aKey) {
      return true;
    }

    if (!pref->mName || !aKey) {
      return false;
    }

    return strcmp(pref->mName, key) == 0;
  }

  static void ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
  {
    auto pref = static_cast<Pref*>(aEntry);
    pref->~Pref();
    memset(pref, 0, sizeof(Pref)); // zero just to be extra safe
  }

  nsresult GetBoolValue(PrefValueKind aKind, bool* aResult)
  {
    if (!IsTypeBool()) {
      return NS_ERROR_UNEXPECTED;
    }

    if (aKind == PrefValueKind::Default || IsLocked() || !mHasUserValue) {
      // Do we have a default?
      if (!mHasDefaultValue) {
        return NS_ERROR_UNEXPECTED;
      }
      *aResult = mDefaultValue.mBoolVal;
    } else {
      *aResult = mUserValue.mBoolVal;
    }

    return NS_OK;
  }

  nsresult GetIntValue(PrefValueKind aKind, int32_t* aResult)
  {
    if (!IsTypeInt()) {
      return NS_ERROR_UNEXPECTED;
    }

    if (aKind == PrefValueKind::Default || IsLocked() || !mHasUserValue) {
      // Do we have a default?
      if (!mHasDefaultValue) {
        return NS_ERROR_UNEXPECTED;
      }
      *aResult = mDefaultValue.mIntVal;
    } else {
      *aResult = mUserValue.mIntVal;
    }

    return NS_OK;
  }

  nsresult GetCStringValue(PrefValueKind aKind, nsACString& aResult)
  {
    if (!IsTypeString()) {
      return NS_ERROR_UNEXPECTED;
    }

    const char* stringVal = nullptr;
    if (aKind == PrefValueKind::Default || IsLocked() || !mHasUserValue) {
      // Do we have a default?
      if (!mHasDefaultValue) {
        return NS_ERROR_UNEXPECTED;
      }
      stringVal = mDefaultValue.mStringVal;
    } else {
      stringVal = mUserValue.mStringVal;
    }

    if (!stringVal) {
      return NS_ERROR_UNEXPECTED;
    }

    aResult = stringVal;
    return NS_OK;
  }

  void ToDomPref(dom::Pref* aDomPref)
  {
    aDomPref->name() = mName;

    aDomPref->isLocked() = mIsLocked;

    if (mHasDefaultValue) {
      aDomPref->defaultValue() = dom::PrefValue();
      mDefaultValue.ToDomPrefValue(Type(),
                                   &aDomPref->defaultValue().get_PrefValue());
    } else {
      aDomPref->defaultValue() = null_t();
    }

    if (mHasUserValue) {
      aDomPref->userValue() = dom::PrefValue();
      mUserValue.ToDomPrefValue(Type(), &aDomPref->userValue().get_PrefValue());
    } else {
      aDomPref->userValue() = null_t();
    }

    MOZ_ASSERT(aDomPref->defaultValue().type() ==
                 dom::MaybePrefValue::Tnull_t ||
               aDomPref->userValue().type() == dom::MaybePrefValue::Tnull_t ||
               (aDomPref->defaultValue().get_PrefValue().type() ==
                aDomPref->userValue().get_PrefValue().type()));
  }

  void FromDomPref(const dom::Pref& aDomPref, bool* aValueChanged)
  {
    MOZ_ASSERT(strcmp(mName, aDomPref.name().get()) == 0);

    mIsLocked = aDomPref.isLocked();

    const dom::MaybePrefValue& defaultValue = aDomPref.defaultValue();
    bool defaultValueChanged = false;
    if (defaultValue.type() == dom::MaybePrefValue::TPrefValue) {
      PrefValue value;
      PrefType type = value.FromDomPrefValue(defaultValue.get_PrefValue());
      if (!ValueMatches(PrefValueKind::Default, type, value)) {
        // Type() is PrefType::None if it's a newly added pref. This is ok.
        mDefaultValue.Replace(Type(), type, value);
        SetType(type);
        mHasDefaultValue = true;
        defaultValueChanged = true;
      }
    }
    // Note: we never clear a default value.

    const dom::MaybePrefValue& userValue = aDomPref.userValue();
    bool userValueChanged = false;
    if (userValue.type() == dom::MaybePrefValue::TPrefValue) {
      PrefValue value;
      PrefType type = value.FromDomPrefValue(userValue.get_PrefValue());
      if (!ValueMatches(PrefValueKind::User, type, value)) {
        // Type() is PrefType::None if it's a newly added pref. This is ok.
        mUserValue.Replace(Type(), type, value);
        SetType(type);
        mHasUserValue = true;
        userValueChanged = true;
      }
    } else if (mHasUserValue) {
      ClearUserValue();
      userValueChanged = true;
    }

    if (userValueChanged || (defaultValueChanged && !mHasUserValue)) {
      *aValueChanged = true;
    }
  }

  bool HasAdvisablySizedValues()
  {
    if (!IsTypeString()) {
      return true;
    }

    const char* stringVal;
    if (mHasDefaultValue) {
      stringVal = mDefaultValue.mStringVal;
      if (strlen(stringVal) > MAX_ADVISABLE_PREF_LENGTH) {
        return false;
      }
    }

    if (mHasUserValue) {
      stringVal = mUserValue.mStringVal;
      if (strlen(stringVal) > MAX_ADVISABLE_PREF_LENGTH) {
        return false;
      }
    }

    return true;
  }

private:
  bool ValueMatches(PrefValueKind aKind, PrefType aType, PrefValue aValue)
  {
    return IsType(aType) &&
           (aKind == PrefValueKind::Default
              ? mHasDefaultValue && mDefaultValue.Equals(aType, aValue)
              : mHasUserValue && mUserValue.Equals(aType, aValue));
  }

public:
  void ClearUserValue()
  {
    if (Type() == PrefType::String) {
      free(const_cast<char*>(mUserValue.mStringVal));
      mUserValue.mStringVal = nullptr;
    }

    mHasUserValue = false;
  }

  nsresult SetValue(PrefType aType,
                    PrefValueKind aKind,
                    PrefValue aValue,
                    bool aIsSticky,
                    bool aForceSet,
                    bool* aValueChanged,
                    bool* aDirty)
  {
    if (aKind == PrefValueKind::Default) {
      // Types must always match when setting the default value.
      if (!IsType(aType)) {
        return NS_ERROR_UNEXPECTED;
      }

      // Should we set the default value? Only if the pref is not locked, and
      // doing so would change the default value.
      if (!IsLocked() && !ValueMatches(PrefValueKind::Default, aType, aValue)) {
        mDefaultValue.Replace(Type(), aType, aValue);
        mHasDefaultValue = true;
        if (aIsSticky) {
          mIsSticky = true;
        }
        if (!mHasUserValue) {
          *aValueChanged = true;
        }
        // What if we change the default to be the same as the user value?
        // Should we clear the user value? Currently we don't.
      }
    } else {
      // If we have a default value, types must match when setting the user
      // value.
      if (mHasDefaultValue && !IsType(aType)) {
        return NS_ERROR_UNEXPECTED;
      }

      // Should we clear the user value, if present? Only if the new user value
      // matches the default value, and the pref isn't sticky, and we aren't
      // force-setting it.
      if (ValueMatches(PrefValueKind::Default, aType, aValue) && !mIsSticky &&
          !aForceSet) {
        if (mHasUserValue) {
          ClearUserValue();
          if (!IsLocked()) {
            *aDirty = true;
            *aValueChanged = true;
          }
        }

        // Otherwise, should we set the user value? Only if doing so would
        // change the user value.
      } else if (!ValueMatches(PrefValueKind::User, aType, aValue)) {
        mUserValue.Replace(Type(), aType, aValue);
        SetType(aType);   // needed because we may have changed the type
        mHasUserValue = true;
        if (!IsLocked()) {
          *aDirty = true;
          *aValueChanged = true;
        }
      }
    }
    return NS_OK;
  }

  // Returns false if this pref doesn't have a user value worth saving.
  bool UserValueToStringForSaving(nsCString& aStr)
  {
    // Should we save the user value, if present? Only if it does not match the
    // default value, or it is sticky.
    if (mHasUserValue &&
        (!ValueMatches(PrefValueKind::Default, Type(), mUserValue) ||
         mIsSticky)) {
      if (IsTypeString()) {
        StrEscape(mUserValue.mStringVal, aStr);

      } else if (IsTypeInt()) {
        aStr.AppendInt(mUserValue.mIntVal);

      } else if (IsTypeBool()) {
        aStr = mUserValue.mBoolVal ? "true" : "false";
      }
      return true;
    }

    // Do not save default prefs that haven't changed.
    return false;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf)
  {
    // Note: mName is allocated in gPrefNameArena, measured elsewhere.
    size_t n = 0;
    if (IsTypeString()) {
      if (mHasDefaultValue) {
        n += aMallocSizeOf(mDefaultValue.mStringVal);
      }
      if (mHasUserValue) {
        n += aMallocSizeOf(mUserValue.mStringVal);
      }
    }
    return n;
  }

private:
  // These fields go first to minimize this struct's size on 64-bit. (Because
  // this a subclass of PLDHashEntryHdr, there's a preceding 32-bit mKeyHash
  // field.)
  uint32_t mType : 2;
  uint32_t mIsSticky : 1;
  uint32_t mIsLocked : 1;
  uint32_t mHasUserValue : 1;
  uint32_t mHasDefaultValue : 1;

  const char* mName; // allocated in gPrefNameArena

  PrefValue mDefaultValue;
  PrefValue mUserValue;
};

struct CallbackNode
{
  CallbackNode(const char* aDomain,
               PrefChangedFunc aFunc,
               void* aData,
               Preferences::MatchKind aMatchKind)
    : mDomain(moz_xstrdup(aDomain))
    , mFunc(aFunc)
    , mData(aData)
    , mMatchKind(aMatchKind)
    , mNext(nullptr)
  {
  }

  UniqueFreePtr<const char> mDomain;

  // If someone attempts to remove the node from the callback list while
  // NotifyCallbacks() is running, |func| is set to nullptr. Such nodes will
  // be removed at the end of NotifyCallbacks().
  PrefChangedFunc mFunc;
  void* mData;
  Preferences::MatchKind mMatchKind;
  CallbackNode* mNext;
};

static PLDHashTable* gHashTable;

// The callback list contains all the priority callbacks followed by the
// non-priority callbacks. gLastPriorityNode records where the first part ends.
static CallbackNode* gFirstCallback = nullptr;
static CallbackNode* gLastPriorityNode = nullptr;

static bool gIsAnyPrefLocked = false;

// These are only used during the call to NotifyCallbacks().
static bool gCallbacksInProgress = false;
static bool gShouldCleanupDeadNodes = false;

static PLDHashTableOps pref_HashTableOps = {
  PLDHashTable::HashStringKey,
  Pref::MatchEntry,
  PLDHashTable::MoveEntryStub,
  Pref::ClearEntry,
  nullptr,
};

static Pref*
pref_HashTableLookup(const char* aPrefName);

static void
NotifyCallbacks(const char* aPrefName);

#define PREF_HASHTABLE_INITIAL_LENGTH 1024

static PrefSaveData
pref_savePrefs()
{
  PrefSaveData savedPrefs(gHashTable->EntryCount());

  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto pref = static_cast<Pref*>(iter.Get());

    nsAutoCString prefValueStr;
    if (!pref->UserValueToStringForSaving(prefValueStr)) {
      continue;
    }

    nsAutoCString prefNameStr;
    StrEscape(pref->Name(), prefNameStr);

    nsPrintfCString str(
      "user_pref(%s, %s);", prefNameStr.get(), prefValueStr.get());

    savedPrefs.AppendElement(str);
  }

  return savedPrefs;
}

#ifdef DEBUG

static pref_initPhase gPhase = START;

struct StringComparator
{
  const char* mPrefName;

  explicit StringComparator(const char* aPrefName)
    : mPrefName(aPrefName)
  {
  }

  int operator()(const char* aPrefName) const
  {
    return strcmp(mPrefName, aPrefName);
  }
};

static bool
InInitArray(const char* aPrefName)
{
  size_t prefsLen;
  size_t found;
  const char** list = mozilla::dom::ContentPrefs::GetContentPrefs(&prefsLen);
  return BinarySearchIf(list, 0, prefsLen, StringComparator(aPrefName), &found);
}

static bool gInstallingCallback = false;

class AutoInstallingCallback
{
public:
  AutoInstallingCallback() { gInstallingCallback = true; }
  ~AutoInstallingCallback() { gInstallingCallback = false; }
};

#define AUTO_INSTALLING_CALLBACK() AutoInstallingCallback installingRAII

#else // DEBUG

#define AUTO_INSTALLING_CALLBACK()

#endif // DEBUG

static Pref*
pref_HashTableLookup(const char* aKey)
{
  MOZ_ASSERT(NS_IsMainThread() || mozilla::ServoStyleSet::IsInServoTraversal());
  MOZ_ASSERT((XRE_IsParentProcess() || gPhase != START),
             "pref access before commandline prefs set");

  // If you're hitting this assertion, you've added a pref access to start up.
  // Consider moving it later or add it to the whitelist in ContentPrefs.cpp
  // and get review from a DOM peer.
  //
  // Note that accesses of non-whitelisted prefs that happen while installing a
  // callback (e.g. VarCache) are considered acceptable. These accesses will
  // fail, but once the proper pref value is set the callback will be
  // immediately called, so things should work out.
#ifdef DEBUG
  if (!XRE_IsParentProcess() && gPhase <= END_INIT_PREFS &&
      !gInstallingCallback && !InInitArray(aKey)) {
    MOZ_CRASH_UNSAFE_PRINTF(
      "accessing non-init pref %s before the rest of the prefs are sent", aKey);
  }
#endif

  return static_cast<Pref*>(gHashTable->Search(aKey));
}

static nsresult
pref_SetPref(const char* aPrefName,
             PrefType aType,
             PrefValueKind aKind,
             PrefValue aValue,
             bool aIsSticky,
             bool aForceSet)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gHashTable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  auto pref = static_cast<Pref*>(gHashTable->Add(aPrefName, fallible));
  if (!pref) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!pref->Name()) {
    // New (zeroed) entry. Partially initialize it.
    new (pref) Pref(aPrefName);
    pref->SetType(aType);
  }

  bool valueChanged = false, handleDirty = false;
  nsresult rv = pref->SetValue(
    aType, aKind, aValue, aIsSticky, aForceSet, &valueChanged, &handleDirty);
  if (NS_FAILED(rv)) {
    NS_WARNING(
      nsPrintfCString(
        "Rejected attempt to change type of pref %s's %s value from %s to %s",
        aPrefName,
        (aKind == PrefValueKind::Default) ? "default" : "user",
        PrefTypeToString(pref->Type()),
        PrefTypeToString(aType))
        .get());

    return rv;
  }

  if (handleDirty && XRE_IsParentProcess()) {
    Preferences::HandleDirty();
  }
  if (valueChanged) {
    NotifyCallbacks(aPrefName);
  }

  return NS_OK;
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
  delete aNode;
  return next_node;
}

static void
NotifyCallbacks(const char* aPrefName)
{
  bool reentered = gCallbacksInProgress;

  // Nodes must not be deleted while gCallbacksInProgress is true.
  // Nodes that need to be deleted are marked for deletion by nulling
  // out the |func| pointer. We release them at the end of this function
  // if we haven't reentered.
  gCallbacksInProgress = true;

  for (CallbackNode* node = gFirstCallback; node; node = node->mNext) {
    if (node->mFunc) {
      bool matches = node->mMatchKind == Preferences::ExactMatch
                       ? strcmp(node->mDomain.get(), aPrefName) == 0
                       : strncmp(node->mDomain.get(),
                                 aPrefName,
                                 strlen(node->mDomain.get())) == 0;
      if (matches) {
        (node->mFunc)(aPrefName, node->mData);
      }
    }
  }

  gCallbacksInProgress = reentered;

  if (gShouldCleanupDeadNodes && !gCallbacksInProgress) {
    CallbackNode* prev_node = nullptr;
    CallbackNode* node = gFirstCallback;

    while (node) {
      if (!node->mFunc) {
        node = pref_RemoveCallbackNode(node, prev_node);
      } else {
        prev_node = node;
        node = node->mNext;
      }
    }
    gShouldCleanupDeadNodes = false;
  }
}

//===========================================================================
// Prefs parsing
//===========================================================================

class Parser
{
public:
  Parser()
    : mState()
    , mNextState()
    , mStrMatch()
    , mStrIndex()
    , mUtf16()
    , mEscLen()
    , mEscTmp()
    , mQuoteChar()
    , mLb()
    , mLbCur()
    , mLbEnd()
    , mVb()
    , mVtype()
    , mIsDefault()
    , mIsSticky()
  {
  }

  ~Parser() { free(mLb); }

  bool Parse(const char* aBuf, int aBufLen);

  bool GrowBuf();

  void HandleValue(const char* aPrefName,
                   PrefType aType,
                   PrefValue aValue,
                   bool aIsDefault,
                   bool aIsSticky);

  void ReportProblem(const char* aMessage, int aLine, bool aError);

private:
  // Pref parser states.
  enum class State
  {
    eInit,
    eMatchString,
    eUntilName,
    eQuotedString,
    eUntilComma,
    eUntilValue,
    eIntValue,
    eCommentMaybeStart,
    eCommentBlock,
    eCommentBlockMaybeEnd,
    eEscapeSequence,
    eHexEscape,
    eUTF16LowSurrogate,
    eUntilOpenParen,
    eUntilCloseParen,
    eUntilSemicolon,
    eUntilEOL
  };

  static const int kUTF16EscapeNumDigits = 4;
  static const int kHexEscapeNumDigits = 2;
  static const int KBitsPerHexDigit = 4;

  static constexpr const char* kUserPref = "user_pref";
  static constexpr const char* kPref = "pref";
  static constexpr const char* kStickyPref = "sticky_pref";
  static constexpr const char* kTrue = "true";
  static constexpr const char* kFalse = "false";

  State mState;           // current parse state
  State mNextState;       // sometimes used...
  const char* mStrMatch;  // string to match
  int mStrIndex;          // next char of smatch to check;
                          // also, counter in \u parsing
  char16_t mUtf16[2];     // parsing UTF16 (\u) escape
  int mEscLen;            // length in mEscTmp
  char mEscTmp[6];        // raw escape to put back if err
  char mQuoteChar;        // char delimiter for quotations
  char* mLb;              // line buffer (only allocation)
  char* mLbCur;           // line buffer cursor
  char* mLbEnd;           // line buffer end
  char* mVb;              // value buffer (ptr into mLb)
  Maybe<PrefType> mVtype; // pref value type
  bool mIsDefault;        // true if (default) pref
  bool mIsSticky;         // true if (sticky) pref
};

// This function will increase the size of the buffer owned by the given pref
// parse state. We currently use a simple doubling algorithm, but the only hard
// requirement is that it increase the buffer by at least the size of the
// mEscTmp buffer used for escape processing (currently 6 bytes).
//
// The buffer is used to store partial pref lines. It is freed when the parse
// state is destroyed.
//
// This function updates all pointers that reference an address within mLb
// since realloc may relocate the buffer.
//
// Returns false on failure.
bool
Parser::GrowBuf()
{
  int bufLen, curPos, valPos;

  bufLen = mLbEnd - mLb;
  curPos = mLbCur - mLb;
  valPos = mVb - mLb;

  if (bufLen == 0) {
    bufLen = 128; // default buffer size
  } else {
    bufLen <<= 1; // double buffer size
  }

  mLb = (char*)realloc(mLb, bufLen);
  if (!mLb) {
    return false;
  }

  mLbCur = mLb + curPos;
  mLbEnd = mLb + bufLen;
  mVb = mLb + valPos;

  return true;
}

void
Parser::HandleValue(const char* aPrefName,
                    PrefType aType,
                    PrefValue aValue,
                    bool aIsDefault,
                    bool aIsSticky)
{
  PrefValueKind kind;
  bool forceSet;
  if (aIsDefault) {
    kind = PrefValueKind::Default;
    forceSet = false;
  } else {
    kind = PrefValueKind::User;
    forceSet = true;
  }
  pref_SetPref(aPrefName, aType, kind, aValue, aIsSticky, forceSet);
}

// Report an error or a warning. If not specified, just dump to stderr.
void
Parser::ReportProblem(const char* aMessage, int aLine, bool aError)
{
  nsPrintfCString message("** Preference parsing %s (line %d) = %s **\n",
                          (aError ? "error" : "warning"),
                          aLine,
                          aMessage);
  nsresult rv;
  nsCOMPtr<nsIConsoleService> console =
    do_GetService("@mozilla.org/consoleservice;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    console->LogStringMessage(NS_ConvertUTF8toUTF16(message).get());
  } else {
    printf_stderr("%s", message.get());
  }
}

// Parse a buffer containing some portion of a preference file. This function
// may be called repeatedly as new data is made available. The PrefReader
// callback function passed to Parser's constructor will be called as preference
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
bool
Parser::Parse(const char* aBuf, int aBufLen)
{
  // The line number is currently only used for the error/warning reporting.
  int lineNum = 0;

  State state = mState;
  for (const char* end = aBuf + aBufLen; aBuf != end; ++aBuf) {
    char c = *aBuf;
    if (c == '\r' || c == '\n' || c == 0x1A) {
      lineNum++;
    }

    switch (state) {
      // initial state
      case State::eInit:
        if (mLbCur != mLb) { // reset state
          mLbCur = mLb;
          mVb = nullptr;
          mVtype = Nothing();
          mIsDefault = false;
          mIsSticky = false;
        }
        switch (c) {
          case '/': // begin comment block or line?
            state = State::eCommentMaybeStart;
            break;
          case '#': // accept shell style comments
            state = State::eUntilEOL;
            break;
          case 'u': // indicating user_pref
          case 's': // indicating sticky_pref
          case 'p': // indicating pref
            if (c == 'u') {
              mStrMatch = kUserPref;
            } else if (c == 's') {
              mStrMatch = kStickyPref;
            } else {
              mStrMatch = kPref;
            }
            mStrIndex = 1;
            mNextState = State::eUntilOpenParen;
            state = State::eMatchString;
            break;
            // else skip char
        }
        break;

      // string matching
      case State::eMatchString:
        if (c == mStrMatch[mStrIndex++]) {
          // If we've matched all characters, then move to next state.
          if (mStrMatch[mStrIndex] == '\0') {
            state = mNextState;
            mNextState = State::eInit; // reset next state
          }
          // else wait for next char
        } else {
          ReportProblem("non-matching string", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // quoted string parsing
      case State::eQuotedString:
        // we assume that the initial quote has already been consumed
        if (mLbCur == mLbEnd && !GrowBuf()) {
          return false; // out of memory
        }
        if (c == '\\') {
          state = State::eEscapeSequence;
        } else if (c == mQuoteChar) {
          *mLbCur++ = '\0';
          state = mNextState;
          mNextState = State::eInit; // reset next state
        } else {
          *mLbCur++ = c;
        }
        break;

      // name parsing
      case State::eUntilName:
        if (c == '\"' || c == '\'') {
          mIsDefault = (mStrMatch == kPref || mStrMatch == kStickyPref);
          mIsSticky = (mStrMatch == kStickyPref);
          mQuoteChar = c;
          mNextState = State::eUntilComma; // return here when done
          state = State::eQuotedString;
        } else if (c == '/') { // allow embedded comment
          mNextState = state;  // return here when done with comment
          state = State::eCommentMaybeStart;
        } else if (!isspace(c)) {
          ReportProblem("need space, comment or quote", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // parse until we find a comma separating name and value
      case State::eUntilComma:
        if (c == ',') {
          mVb = mLbCur;
          state = State::eUntilValue;
        } else if (c == '/') { // allow embedded comment
          mNextState = state;  // return here when done with comment
          state = State::eCommentMaybeStart;
        } else if (!isspace(c)) {
          ReportProblem("need space, comment or comma", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // value parsing
      case State::eUntilValue:
        // The pref value type is unknown. So, we scan for the first character
        // of the value, and determine the type from that.
        if (c == '\"' || c == '\'') {
          mVtype = Some(PrefType::String);
          mQuoteChar = c;
          mNextState = State::eUntilCloseParen;
          state = State::eQuotedString;
        } else if (c == 't' || c == 'f') {
          mVb = (char*)(c == 't' ? kTrue : kFalse);
          mVtype = Some(PrefType::Bool);
          mStrMatch = mVb;
          mStrIndex = 1;
          mNextState = State::eUntilCloseParen;
          state = State::eMatchString;
        } else if (isdigit(c) || (c == '-') || (c == '+')) {
          mVtype = Some(PrefType::Int);
          // write c to line buffer...
          if (mLbCur == mLbEnd && !GrowBuf()) {
            return false; // out of memory
          }
          *mLbCur++ = c;
          state = State::eIntValue;
        } else if (c == '/') { // allow embedded comment
          mNextState = state;  // return here when done with comment
          state = State::eCommentMaybeStart;
        } else if (!isspace(c)) {
          ReportProblem("need value, comment or space", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      case State::eIntValue:
        // grow line buffer if necessary...
        if (mLbCur == mLbEnd && !GrowBuf()) {
          return false; // out of memory
        }
        if (isdigit(c)) {
          *mLbCur++ = c;
        } else {
          *mLbCur++ = '\0'; // stomp null terminator; we are done.
          if (c == ')') {
            state = State::eUntilSemicolon;
          } else if (c == '/') { // allow embedded comment
            mNextState = State::eUntilCloseParen;
            state = State::eCommentMaybeStart;
          } else if (isspace(c)) {
            state = State::eUntilCloseParen;
          } else {
            ReportProblem("while parsing integer", lineNum, true);
            NS_WARNING("malformed pref file");
            return false;
          }
        }
        break;

      // comment parsing
      case State::eCommentMaybeStart:
        switch (c) {
          case '*': // comment block
            state = State::eCommentBlock;
            break;
          case '/': // comment line
            state = State::eUntilEOL;
            break;
          default:
            // pref file is malformed
            ReportProblem("while parsing comment", lineNum, true);
            NS_WARNING("malformed pref file");
            return false;
        }
        break;

      case State::eCommentBlock:
        if (c == '*') {
          state = State::eCommentBlockMaybeEnd;
        }
        break;

      case State::eCommentBlockMaybeEnd:
        switch (c) {
          case '/':
            state = mNextState;
            mNextState = State::eInit;
            break;
          case '*': // stay in this state
            break;
          default:
            state = State::eCommentBlock;
            break;
        }
        break;

      // string escape sequence parsing
      case State::eEscapeSequence:
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
            mEscTmp[0] = c;
            mEscLen = 1;
            mUtf16[0] = mUtf16[1] = 0;
            mStrIndex =
              (c == 'x') ? kHexEscapeNumDigits : kUTF16EscapeNumDigits;
            state = State::eHexEscape;
            continue;
          default:
            ReportProblem(
              "preserving unexpected JS escape sequence", lineNum, false);
            NS_WARNING("preserving unexpected JS escape sequence");
            // Invalid escape sequence so we do have to write more than one
            // character. Grow line buffer if necessary...
            if ((mLbCur + 1) == mLbEnd && !GrowBuf()) {
              return false; // out of memory
            }
            *mLbCur++ = '\\'; // preserve the escape sequence
            break;
        }
        *mLbCur++ = c;
        state = State::eQuotedString;
        break;

      // parsing a hex (\xHH) or mUtf16 escape (\uHHHH)
      case State::eHexEscape: {
        char udigit;
        if (c >= '0' && c <= '9') {
          udigit = (c - '0');
        } else if (c >= 'A' && c <= 'F') {
          udigit = (c - 'A') + 10;
        } else if (c >= 'a' && c <= 'f') {
          udigit = (c - 'a') + 10;
        } else {
          // bad escape sequence found, write out broken escape as-is
          ReportProblem(
            "preserving invalid or incomplete hex escape", lineNum, false);
          NS_WARNING("preserving invalid or incomplete hex escape");
          *mLbCur++ = '\\'; // original escape slash
          if ((mLbCur + mEscLen) >= mLbEnd && !GrowBuf()) {
            return false;
          }
          for (int i = 0; i < mEscLen; ++i) {
            *mLbCur++ = mEscTmp[i];
          }

          // Push the non-hex character back for re-parsing. (++aBuf at the top
          // of the loop keeps this safe.)
          --aBuf;
          state = State::eQuotedString;
          continue;
        }

        // have a digit
        mEscTmp[mEscLen++] = c; // preserve it
        mUtf16[1] <<= KBitsPerHexDigit;
        mUtf16[1] |= udigit;
        mStrIndex--;
        if (mStrIndex == 0) {
          // we have the full escape, convert to UTF8
          int utf16len = 0;
          if (mUtf16[0]) {
            // already have a high surrogate, this is a two char seq
            utf16len = 2;
          } else if (0xD800 == (0xFC00 & mUtf16[1])) {
            // a high surrogate, can't convert until we have the low
            mUtf16[0] = mUtf16[1];
            mUtf16[1] = 0;
            state = State::eUTF16LowSurrogate;
            break;
          } else {
            // a single mUtf16 character
            mUtf16[0] = mUtf16[1];
            utf16len = 1;
          }

          // The actual conversion.
          // Make sure there's room, 6 bytes is max utf8 len (in theory; 4
          // bytes covers the actual mUtf16 range).
          if (mLbCur + 6 >= mLbEnd && !GrowBuf()) {
            return false;
          }

          ConvertUTF16toUTF8 converter(mLbCur);
          converter.write(mUtf16, utf16len);
          mLbCur += converter.Size();
          state = State::eQuotedString;
        }
        break;
      }

      // looking for beginning of mUtf16 low surrogate
      case State::eUTF16LowSurrogate:
        if (mStrIndex == 0 && c == '\\') {
          ++mStrIndex;
        } else if (mStrIndex == 1 && c == 'u') {
          // escape sequence is correct, now parse hex
          mStrIndex = kUTF16EscapeNumDigits;
          mEscTmp[0] = 'u';
          mEscLen = 1;
          state = State::eHexEscape;
        } else {
          // Didn't find expected low surrogate. Ignore high surrogate (it
          // would just get converted to nothing anyway) and start over with
          // this character.
          --aBuf;
          if (mStrIndex == 1) {
            state = State::eEscapeSequence;
          } else {
            state = State::eQuotedString;
          }
          continue;
        }
        break;

      // function open and close parsing
      case State::eUntilOpenParen:
        // tolerate only whitespace and embedded comments
        if (c == '(') {
          state = State::eUntilName;
        } else if (c == '/') {
          mNextState = state; // return here when done with comment
          state = State::eCommentMaybeStart;
        } else if (!isspace(c)) {
          ReportProblem(
            "need space, comment or open parentheses", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      case State::eUntilCloseParen:
        // tolerate only whitespace and embedded comments
        if (c == ')') {
          state = State::eUntilSemicolon;
        } else if (c == '/') {
          mNextState = state; // return here when done with comment
          state = State::eCommentMaybeStart;
        } else if (!isspace(c)) {
          ReportProblem(
            "need space, comment or closing parentheses", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // function terminator ';' parsing
      case State::eUntilSemicolon:
        // tolerate only whitespace and embedded comments
        if (c == ';') {

          PrefValue value;

          switch (*mVtype) {
            case PrefType::String:
              value.mStringVal = mVb;
              break;

            case PrefType::Int:
              if ((mVb[0] == '-' || mVb[0] == '+') && mVb[1] == '\0') {
                ReportProblem("invalid integer value", 0, true);
                NS_WARNING("malformed integer value");
                return false;
              }
              value.mIntVal = atoi(mVb);
              break;

            case PrefType::Bool:
              value.mBoolVal = (mVb == kTrue);
              break;

            default:
              MOZ_CRASH();
          }

          // We've extracted a complete name/value pair.
          HandleValue(mLb, *mVtype, value, mIsDefault, mIsSticky);

          state = State::eInit;
        } else if (c == '/') {
          mNextState = state; // return here when done with comment
          state = State::eCommentMaybeStart;
        } else if (!isspace(c)) {
          ReportProblem("need space, comment or semicolon", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // eol parsing
      case State::eUntilEOL:
        // Need to handle mac, unix, or dos line endings. State::eInit will
        // eat the next \n in case we have \r\n.
        if (c == '\r' || c == '\n' || c == 0x1A) {
          state = mNextState;
          mNextState = State::eInit; // reset next state
        }
        break;
    }
  }
  mState = state;
  return true;
}

//===========================================================================
// nsPrefBranch et al.
//===========================================================================

namespace mozilla {
class PreferenceServiceReporter;
} // namespace mozilla

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

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t n = aMallocSizeOf(this);
    n += mDomain.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    // All the other fields are non-owning pointers, so we don't measure them.

    return n;
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

  nsPrefBranch(const char* aPrefRoot, PrefValueKind aKind);
  nsPrefBranch() = delete;

  static void NotifyObserver(const char* aNewpref, void* aData);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

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

  int32_t GetRootLength() const { return mPrefRoot.Length(); }

  nsresult GetDefaultFromPropertiesFile(const char* aPrefName,
                                        nsAString& aReturn);

  // As SetCharPref, but without any check on the length of |aValue|.
  nsresult SetCharPrefNoLengthCheck(const char* aPrefName,
                                    const nsACString& aValue);

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
  PrefValueKind mKind;

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

nsPrefBranch::nsPrefBranch(const char* aPrefRoot, PrefValueKind aKind)
  : mPrefRoot(aPrefRoot)
  , mKind(aKind)
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

  const PrefName& prefName = GetPrefName(aPrefName);
  Pref* pref;
  if (gHashTable && (pref = pref_HashTableLookup(prefName.get()))) {
    switch (pref->Type()) {
      case PrefType::String:
        *aRetVal = PREF_STRING;
        break;

      case PrefType::Int:
        *aRetVal = PREF_INT;
        break;

      case PrefType::Bool:
        *aRetVal = PREF_BOOL;
        break;

      default:
        MOZ_CRASH();
    }
  } else {
    *aRetVal = PREF_INVALID;
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
  return Preferences::GetBool(pref.get(), aRetVal, mKind);
}

NS_IMETHODIMP
nsPrefBranch::SetBoolPref(const char* aPrefName, bool aValue)
{
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::SetBool(pref.get(), aValue, mKind);
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
  return Preferences::GetCString(pref.get(), aRetVal, mKind);
}

NS_IMETHODIMP
nsPrefBranch::SetCharPref(const char* aPrefName, const nsACString& aValue)
{
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return SetCharPrefNoLengthCheck(aPrefName, aValue);
}

nsresult
nsPrefBranch::SetCharPrefNoLengthCheck(const char* aPrefName,
                                       const nsACString& aValue)
{
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::SetCString(pref.get(), aValue, mKind);
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

  return SetCharPrefNoLengthCheck(aPrefName, aValue);
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
  return Preferences::GetInt(pref.get(), aRetVal, mKind);
}

NS_IMETHODIMP
nsPrefBranch::SetIntPref(const char* aPrefName, int32_t aValue)
{
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::SetInt(pref.get(), aValue, mKind);
}

NS_IMETHODIMP
nsPrefBranch::GetComplexValue(const char* aPrefName,
                              const nsIID& aType,
                              void** aRetVal)
{
  NS_ENSURE_ARG(aPrefName);

  nsresult rv;
  nsAutoCString utf8String;

  // We have to do this one first because it's different to all the rest.
  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString(
      do_CreateInstance(NS_PREFLOCALIZEDSTRING_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }

    const PrefName& pref = GetPrefName(aPrefName);
    bool bNeedDefault = false;

    if (mKind == PrefValueKind::Default) {
      bNeedDefault = true;
    } else {
      // if there is no user (or locked) value
      if (!Preferences::HasUserValue(pref.get()) &&
          !Preferences::IsLocked(pref.get())) {
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
    ENSURE_PARENT_PROCESS("GetComplexValue(nsIFile)", aPrefName);

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
    ENSURE_PARENT_PROCESS("GetComplexValue(nsIRelativeFilePref)", aPrefName);

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

NS_IMETHODIMP
nsPrefBranch::SetComplexValue(const char* aPrefName,
                              const nsIID& aType,
                              nsISupports* aValue)
{
  ENSURE_PARENT_PROCESS("SetComplexValue", aPrefName);
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
      rv = SetCharPrefNoLengthCheck(aPrefName, descriptorString);
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
    return SetCharPrefNoLengthCheck(aPrefName, descriptorString);
  }

  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
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
        rv = SetCharPrefNoLengthCheck(aPrefName,
                                      NS_ConvertUTF16toUTF8(wideString));
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
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::ClearUser(pref.get());
}

NS_IMETHODIMP
nsPrefBranch::PrefHasUserValue(const char* aPrefName, bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  *aRetVal = Preferences::HasUserValue(pref.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::LockPref(const char* aPrefName)
{
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::Lock(pref.get());
}

NS_IMETHODIMP
nsPrefBranch::PrefIsLocked(const char* aPrefName, bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  *aRetVal = Preferences::IsLocked(pref.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::UnlockPref(const char* aPrefName)
{
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::Unlock(pref.get());
}

NS_IMETHODIMP
nsPrefBranch::ResetBranch(const char* aStartingAt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefBranch::DeleteBranch(const char* aStartingAt)
{
  ENSURE_PARENT_PROCESS("DeleteBranch", aStartingAt);
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
    auto pref = static_cast<Pref*>(iter.Get());

    // The first disjunct matches branches: e.g. a branch name "foo.bar."
    // matches a name "foo.bar.baz" (but it won't match "foo.barrel.baz").
    // The second disjunct matches leaf nodes: e.g. a branch name "foo.bar."
    // matches a name "foo.bar" (by ignoring the trailing '.').
    nsDependentCString name(pref->Name());
    if (StringBeginsWith(name, branchName) || name.Equals(branchNameNoDot)) {
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
    auto pref = static_cast<Pref*>(iter.Get());
    if (strncmp(pref->Name(), parent.get(), parentLen) == 0) {
      prefArray.AppendElement(pref->Name());
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
  Preferences::RegisterCallback(NotifyObserver,
                                pref.get(),
                                pCallback,
                                Preferences::PrefixMatch,
                                /* isPriority */ false);

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
    rv = Preferences::UnregisterCallback(
      NotifyObserver, pref.get(), pCallback, Preferences::PrefixMatch);
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
nsPrefBranch::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  n += mPrefRoot.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

  n += mObservers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mObservers.ConstIter(); !iter.Done(); iter.Next()) {
    const PrefCallback* data = iter.UserData();
    n += data->SizeOfIncludingThis(aMallocSizeOf);
  }

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
    Preferences::UnregisterCallback(nsPrefBranch::NotifyObserver,
                                    pref.get(),
                                    callback,
                                    Preferences::PrefixMatch);
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
  nsresult rv =
    Preferences::GetCString(aPrefName, propertyFileURL, PrefValueKind::Default);
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
// class Preferences and related things
//===========================================================================

namespace mozilla {

#define INITIAL_PREF_FILES 10

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

void
Preferences::HandleDirty()
{
  MOZ_ASSERT(XRE_IsParentProcess());

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
                                   sPreferences.get(),
                                   &Preferences::SavePrefFileAsynchronous),
        PREF_DELAY_MS);
    }
  }
}

static nsresult
openPrefFile(nsIFile* aFile);

static nsresult
pref_LoadPrefsInDirList(const char* aListId);

static const char kTelemetryPref[] = "toolkit.telemetry.enabled";
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

// Note: if sShutdown is true, sPreferences will be nullptr.
StaticRefPtr<Preferences> Preferences::sPreferences;
bool Preferences::sShutdown = false;

// This globally enables or disables OMT pref writing, both sync and async.
static int32_t sAllowOMTPrefWrite = -1;

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

// gCacheData holds the CacheData objects used for VarCache prefs. It owns
// those objects, and also is used to detect if multiple VarCaches get tied to
// a single global variable.
static nsTArray<nsAutoPtr<CacheData>>* gCacheData = nullptr;

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

struct PrefsSizes
{
  PrefsSizes()
    : mHashTable(0)
    , mStringValues(0)
    , mCacheData(0)
    , mRootBranches(0)
    , mPrefNameArena(0)
    , mCallbacks(0)
    , mMisc(0)
  {
  }

  size_t mHashTable;
  size_t mStringValues;
  size_t mCacheData;
  size_t mRootBranches;
  size_t mPrefNameArena;
  size_t mCallbacks;
  size_t mMisc;
};

// Although this is a member of Preferences, it measures sPreferences and
// several other global structures.
/* static */ void
Preferences::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                    PrefsSizes& aSizes)
{
  if (!sPreferences) {
    return;
  }

  aSizes.mMisc += aMallocSizeOf(sPreferences.get());

  aSizes.mRootBranches +=
    static_cast<nsPrefBranch*>(sPreferences->mRootBranch.get())
      ->SizeOfIncludingThis(aMallocSizeOf) +
    static_cast<nsPrefBranch*>(sPreferences->mDefaultRootBranch.get())
      ->SizeOfIncludingThis(aMallocSizeOf);
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
  MOZ_ASSERT(NS_IsMainThread());

  MallocSizeOf mallocSizeOf = PreferenceServiceMallocSizeOf;
  PrefsSizes sizes;

  Preferences::AddSizeOfIncludingThis(mallocSizeOf, sizes);

  if (gHashTable) {
    sizes.mHashTable += gHashTable->ShallowSizeOfIncludingThis(mallocSizeOf);
    for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
      auto pref = static_cast<Pref*>(iter.Get());
      sizes.mStringValues += pref->SizeOfExcludingThis(mallocSizeOf);
    }
  }

  if (gCacheData) {
    sizes.mCacheData += gCacheData->ShallowSizeOfIncludingThis(mallocSizeOf);
    for (uint32_t i = 0, count = gCacheData->Length(); i < count; ++i) {
      sizes.mCacheData += mallocSizeOf((*gCacheData)[i]);
    }
  }

  sizes.mPrefNameArena += gPrefNameArena.SizeOfExcludingThis(mallocSizeOf);

  for (CallbackNode* node = gFirstCallback; node; node = node->mNext) {
    sizes.mCallbacks += mallocSizeOf(node);
    sizes.mCallbacks += mallocSizeOf(node->mDomain.get());
  }

  MOZ_COLLECT_REPORT("explicit/preferences/hash-table",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mHashTable,
                     "Memory used by libpref's hash table.");

  MOZ_COLLECT_REPORT("explicit/preferences/string-values",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mStringValues,
                     "Memory used by libpref's string pref values.");

  MOZ_COLLECT_REPORT("explicit/preferences/cache-data",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mCacheData,
                     "Memory used by libpref's VarCaches.");

  MOZ_COLLECT_REPORT("explicit/preferences/root-branches",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mRootBranches,
                     "Memory used by libpref's root branches.");

  MOZ_COLLECT_REPORT("explicit/preferences/pref-name-arena",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mPrefNameArena,
                     "Memory used by libpref's arena for pref names.");

  MOZ_COLLECT_REPORT("explicit/preferences/callbacks",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mCallbacks,
                     "Memory used by libpref's callbacks list, including "
                     "pref names and prefixes.");

  MOZ_COLLECT_REPORT("explicit/preferences/misc",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mMisc,
                     "Miscellaneous memory used by libpref.");

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

static InfallibleTArray<dom::Pref>* gInitDomPrefs;

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

  sPreferences = new Preferences();

  MOZ_ASSERT(!gHashTable);
  gHashTable = new PLDHashTable(
    &pref_HashTableOps, sizeof(Pref), PREF_HASHTABLE_INITIAL_LENGTH);

  Result<Ok, const char*> res = InitInitialObjects();
  if (res.isErr()) {
    sPreferences = nullptr;
    gCacheDataDesc = res.unwrapErr();
    return nullptr;
  }

  if (!XRE_IsParentProcess()) {
    MOZ_ASSERT(gInitDomPrefs);
    for (unsigned int i = 0; i < gInitDomPrefs->Length(); i++) {
      Preferences::SetPreference(gInitDomPrefs->ElementAt(i));
    }
    delete gInitDomPrefs;
    gInitDomPrefs = nullptr;

  } else {
    // Check if there is a deployment configuration file. If so, set up the
    // pref config machinery, which will actually read the file.
    nsAutoCString lockFileName;
    nsresult rv = Preferences::GetCString(
      "general.config.filename", lockFileName, PrefValueKind::User);
    if (NS_SUCCEEDED(rv)) {
      NS_CreateServicesFromCategory(
        "pref-config-startup",
        static_cast<nsISupports*>(static_cast<void*>(sPreferences)),
        "pref-config-startup");
    }

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService) {
      sPreferences = nullptr;
      gCacheDataDesc = "GetObserverService() failed (1)";
      return nullptr;
    }

    observerService->AddObserver(
      sPreferences, "profile-before-change-telemetry", true);
    rv =
      observerService->AddObserver(sPreferences, "profile-before-change", true);

    observerService->AddObserver(
      sPreferences, "suspend_process_notification", true);

    if (NS_FAILED(rv)) {
      sPreferences = nullptr;
      gCacheDataDesc = "AddObserver(\"profile-before-change\") failed";
      return nullptr;
    }
  }

  gCacheData = new nsTArray<nsAutoPtr<CacheData>>();
  gCacheDataDesc = "set by GetInstanceForService()";

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

  if (MOZ_LIKELY(sPreferences)) {
    return true;
  }

  if (!sShutdown) {
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
    sPreferences = nullptr;
  }
}

Preferences::Preferences()
  : mRootBranch(new nsPrefBranch("", PrefValueKind::User))
  , mDefaultRootBranch(new nsPrefBranch("", PrefValueKind::Default))
{
}

Preferences::~Preferences()
{
  MOZ_ASSERT(!sPreferences);

  delete gCacheData;
  gCacheData = nullptr;

  NS_ASSERTION(!gCallbacksInProgress,
               "~Preferences was called while gCallbacksInProgress is true!");

  CallbackNode* node = gFirstCallback;
  while (node) {
    CallbackNode* next_node = node->mNext;
    delete node;
    node = next_node;
  }
  gLastPriorityNode = gFirstCallback = nullptr;

  delete gHashTable;
  gHashTable = nullptr;
  gPrefNameArena.Clear();
}

NS_IMPL_ADDREF(Preferences)
NS_IMPL_RELEASE(Preferences)

NS_INTERFACE_MAP_BEGIN(Preferences)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrefService)
  NS_INTERFACE_MAP_ENTRY(nsIPrefService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIPrefBranch)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

/* static */ void
Preferences::SetInitPreferences(nsTArray<dom::Pref>* aDomPrefs)
{
  gInitDomPrefs = new InfallibleTArray<dom::Pref>(mozilla::Move(*aDomPrefs));
}

/* static */ void
Preferences::InitializeUserPrefs()
{
  MOZ_ASSERT(XRE_IsParentProcess());
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

  } else if (!nsCRT::strcmp(aTopic, "reload-default-prefs")) {
    // Reload the default prefs from file.
    Unused << InitInitialObjects();

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
  ENSURE_PARENT_PROCESS("Preferences::ReadUserPrefsFromFile", "all prefs");

  if (!aFile) {
    NS_ERROR("ReadUserPrefsFromFile requires a parameter");
    return NS_ERROR_INVALID_ARG;
  }

  return openPrefFile(aFile);
}

NS_IMETHODIMP
Preferences::ResetPrefs()
{
  ENSURE_PARENT_PROCESS("Preferences::ResetPrefs", "all prefs");

  NotifyServiceObservers(NS_PREFSERVICE_RESET_TOPIC_ID);

  gHashTable->ClearAndPrepareForLength(PREF_HASHTABLE_INITIAL_LENGTH);
  gPrefNameArena.Clear();

  return InitInitialObjects().isOk() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Preferences::ResetUserPrefs()
{
  ENSURE_PARENT_PROCESS("Preferences::ResetUserPrefs", "all prefs");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  MOZ_ASSERT(NS_IsMainThread());

  Vector<const char*> prefNames;
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto pref = static_cast<Pref*>(iter.Get());

    if (pref->HasUserValue()) {
      if (!prefNames.append(pref->Name())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      pref->ClearUserValue();
      if (!pref->HasDefaultValue()) {
        iter.Remove();
      }
    }
  }

  for (const char* prefName : prefNames) {
    NotifyCallbacks(prefName);
  }

  Preferences::HandleDirty();
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

/* static */ void
Preferences::SetPreference(const dom::Pref& aDomPref)
{
  MOZ_ASSERT(!XRE_IsParentProcess());
  NS_ENSURE_TRUE(InitStaticMembers(), (void)0);

  const char* prefName = aDomPref.name().get();

  auto pref = static_cast<Pref*>(gHashTable->Add(prefName, fallible));
  if (!pref) {
    return;
  }

  if (!pref->Name()) {
    // New (zeroed) entry. Partially initialize it.
    new (pref) Pref(prefName);
  }

  bool valueChanged = false;
  pref->FromDomPref(aDomPref, &valueChanged);

  // When the parent process clears a pref's user value we get a DomPref here
  // with no default value and no user value. There are two possibilities.
  //
  // - There was an existing pref with only a user value. FromDomPref() will
  //   have just cleared that user value, so the pref can be removed.
  //
  // - There was no existing pref. FromDomPref() will have done nothing, and
  //   `pref` will be valueless. We will end up adding and removing the value
  //   needlessly, but that's ok because this case is rare.
  //
  if (!pref->HasDefaultValue() && !pref->HasUserValue()) {
    gHashTable->RemoveEntry(pref);
  }

  // Note: we don't have to worry about HandleDirty() because we are setting
  // prefs in the content process that have come from the parent process.

  if (valueChanged) {
    NotifyCallbacks(prefName);
  }
}

/* static */ void
Preferences::GetPreference(dom::Pref* aDomPref)
{
  Pref* pref = pref_HashTableLookup(aDomPref->name().get());
  if (pref && pref->HasAdvisablySizedValues()) {
    pref->ToDomPref(aDomPref);
  }
}

void
Preferences::GetPreferences(InfallibleTArray<dom::Pref>* aDomPrefs)
{
  aDomPrefs->SetCapacity(gHashTable->EntryCount());
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    auto pref = static_cast<Pref*>(iter.Get());

    if (pref->HasAdvisablySizedValues()) {
      dom::Pref* setting = aDomPrefs->AppendElement();
      pref->ToDomPref(setting);
    }
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
    RefPtr<nsPrefBranch> prefBranch =
      new nsPrefBranch(aPrefRoot, PrefValueKind::User);
    prefBranch.forget(aRetVal);
  } else {
    // Special case: caching the default root.
    nsCOMPtr<nsIPrefBranch> root(sPreferences->mRootBranch);
    root.forget(aRetVal);
  }

  return NS_OK;
}

NS_IMETHODIMP
Preferences::GetDefaultBranch(const char* aPrefRoot, nsIPrefBranch** aRetVal)
{
  if (!aPrefRoot || !aPrefRoot[0]) {
    nsCOMPtr<nsIPrefBranch> root(sPreferences->mDefaultRootBranch);
    root.forget(aRetVal);
    return NS_OK;
  }

  // TODO: Cache this stuff and allow consumers to share branches (hold weak
  // references, I think).
  RefPtr<nsPrefBranch> prefBranch =
    new nsPrefBranch(aPrefRoot, PrefValueKind::Default);
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
  ENSURE_PARENT_PROCESS("Preferences::SavePrefFileInternal", "all prefs");

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
  MOZ_ASSERT(XRE_IsParentProcess());

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
  nsCString data;
  MOZ_TRY_VAR(data, URLPreloader::ReadFile(aFile));

  Parser parser;
  if (!parser.Parse(data.get(), data.Length())) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

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

    // Do we care if a file provided by this process fails to load?
    pref_LoadPrefsInDir(path, nullptr, 0);
  }

  return NS_OK;
}

static nsresult
pref_ReadPrefFromJar(nsZipArchive* aJarReader, const char* aName)
{
  nsCString manifest;
  MOZ_TRY_VAR(manifest,
              URLPreloader::ReadZip(aJarReader, nsDependentCString(aName)));

  Parser parser;
  parser.Parse(manifest.get(), manifest.Length());

  return NS_OK;
}

// Initialize default preference JavaScript buffers from appropriate TEXT
// resources.
/* static */ Result<Ok, const char*>
Preferences::InitInitialObjects()
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
  if (Preferences::GetType(kTelemetryPref, PrefValueKind::Default) ==
      nsIPrefBranch::PREF_INVALID) {
    bool prerelease = false;
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
    prerelease = true;
#else
    nsAutoCString prefValue;
    Preferences::GetCString(kChannelPref, prefValue, PrefValueKind::Default);
    if (prefValue.EqualsLiteral("beta")) {
      prerelease = true;
    }
#endif
    Preferences::SetBoolInAnyProcess(
      kTelemetryPref, prerelease, PrefValueKind::Default);
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
      !strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "beta") || developerBuild) {
    Preferences::SetBoolInAnyProcess(
      kTelemetryPref, true, PrefValueKind::Default);
  } else {
    Preferences::SetBoolInAnyProcess(
      kTelemetryPref, false, PrefValueKind::Default);
  }
  Preferences::LockInAnyProcess(kTelemetryPref);
#endif // MOZ_WIDGET_ANDROID

  NS_CreateServicesFromCategory(NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID,
                                nullptr,
                                NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  NS_ENSURE_SUCCESS(rv, Err("GetObserverService() failed (2)"));

  observerService->NotifyObservers(
    nullptr, NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID, nullptr);

  return Ok();
}

/* static */ nsresult
Preferences::GetBool(const char* aPrefName, bool* aResult, PrefValueKind aKind)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref = pref_HashTableLookup(aPrefName);
  return pref ? pref->GetBoolValue(aKind, aResult) : NS_ERROR_UNEXPECTED;
}

/* static */ nsresult
Preferences::GetInt(const char* aPrefName,
                    int32_t* aResult,
                    PrefValueKind aKind)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref = pref_HashTableLookup(aPrefName);
  return pref ? pref->GetIntValue(aKind, aResult) : NS_ERROR_UNEXPECTED;
}

/* static */ nsresult
Preferences::GetFloat(const char* aPrefName,
                      float* aResult,
                      PrefValueKind aKind)
{
  NS_PRECONDITION(aResult, "aResult must not be NULL");

  nsAutoCString result;
  nsresult rv = Preferences::GetCString(aPrefName, result, aKind);
  if (NS_SUCCEEDED(rv)) {
    *aResult = result.ToFloat(&rv);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetCString(const char* aPrefName,
                        nsACString& aResult,
                        PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  aResult.SetIsVoid(true);

  Pref* pref = pref_HashTableLookup(aPrefName);
  return pref ? pref->GetCStringValue(aKind, aResult) : NS_ERROR_UNEXPECTED;
}

/* static */ nsresult
Preferences::GetString(const char* aPrefName,
                       nsAString& aResult,
                       PrefValueKind aKind)
{
  nsAutoCString result;
  nsresult rv = Preferences::GetCString(aPrefName, result, aKind);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(result, aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetLocalizedCString(const char* aPrefName,
                                 nsACString& aResult,
                                 PrefValueKind aKind)
{
  nsAutoString result;
  nsresult rv = GetLocalizedString(aPrefName, result);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF16toUTF8(result, aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetLocalizedString(const char* aPrefName,
                                nsAString& aResult,
                                PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
  nsresult rv =
    GetRootBranch(aKind)->GetComplexValue(aPrefName,
                                          NS_GET_IID(nsIPrefLocalizedString),
                                          getter_AddRefs(prefLocalString));
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(prefLocalString, "Succeeded but the result is NULL");
    prefLocalString->GetData(aResult);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetComplex(const char* aPrefName,
                        const nsIID& aType,
                        void** aResult,
                        PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return GetRootBranch(aKind)->GetComplexValue(aPrefName, aType, aResult);
}

/* static */ nsresult
Preferences::SetCStringInAnyProcess(const char* aPrefName,
                                    const nsACString& aValue,
                                    PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  if (aValue.Length() > MAX_PREF_LENGTH) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // It's ok to stash a pointer to the temporary PromiseFlatCString's chars in
  // pref because pref_SetPref() duplicates those chars.
  PrefValue prefValue;
  const nsCString& flat = PromiseFlatCString(aValue);
  prefValue.mStringVal = flat.get();
  return pref_SetPref(aPrefName,
                      PrefType::String,
                      aKind,
                      prefValue,
                      /* isSticky */ false,
                      /* forceSet */ false);
}

/* static */ nsresult
Preferences::SetCString(const char* aPrefName,
                        const nsACString& aValue,
                        PrefValueKind aKind)
{
  ENSURE_PARENT_PROCESS("SetCString", aPrefName);
  return SetCStringInAnyProcess(aPrefName, aValue, aKind);
}

/* static */ nsresult
Preferences::SetBoolInAnyProcess(const char* aPrefName,
                                 bool aValue,
                                 PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  PrefValue prefValue;
  prefValue.mBoolVal = aValue;
  return pref_SetPref(aPrefName,
                      PrefType::Bool,
                      aKind,
                      prefValue,
                      /* isSticky */ false,
                      /* forceSet */ false);
}

/* static */ nsresult
Preferences::SetBool(const char* aPrefName, bool aValue, PrefValueKind aKind)
{
  ENSURE_PARENT_PROCESS("SetBool", aPrefName);
  return SetBoolInAnyProcess(aPrefName, aValue, aKind);
}

/* static */ nsresult
Preferences::SetIntInAnyProcess(const char* aPrefName,
                                int32_t aValue,
                                PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  PrefValue prefValue;
  prefValue.mIntVal = aValue;
  return pref_SetPref(aPrefName,
                      PrefType::Int,
                      aKind,
                      prefValue,
                      /* isSticky */ false,
                      /* forceSet */ false);
}

/* static */ nsresult
Preferences::SetInt(const char* aPrefName, int32_t aValue, PrefValueKind aKind)
{
  ENSURE_PARENT_PROCESS("SetInt", aPrefName);
  return SetIntInAnyProcess(aPrefName, aValue, aKind);
}

/* static */ nsresult
Preferences::SetComplex(const char* aPrefName,
                        const nsIID& aType,
                        nsISupports* aValue,
                        PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return GetRootBranch(aKind)->SetComplexValue(aPrefName, aType, aValue);
}

/* static */ nsresult
Preferences::LockInAnyProcess(const char* aPrefName)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref = pref_HashTableLookup(aPrefName);
  if (!pref) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!pref->IsLocked()) {
    pref->SetIsLocked(true);
    gIsAnyPrefLocked = true;
    NotifyCallbacks(aPrefName);
  }

  return NS_OK;
}

/* static */ nsresult
Preferences::Lock(const char* aPrefName)
{
  ENSURE_PARENT_PROCESS("Lock", aPrefName);
  return Preferences::LockInAnyProcess(aPrefName);
}

/* static */ nsresult
Preferences::Unlock(const char* aPrefName)
{
  ENSURE_PARENT_PROCESS("Unlock", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref = pref_HashTableLookup(aPrefName);
  if (!pref) {
    return NS_ERROR_UNEXPECTED;
  }

  if (pref->IsLocked()) {
    pref->SetIsLocked(false);
    NotifyCallbacks(aPrefName);
  }

  return NS_OK;
}

/* static */ bool
Preferences::IsLocked(const char* aPrefName)
{
  NS_ENSURE_TRUE(InitStaticMembers(), false);

  if (gIsAnyPrefLocked) {
    Pref* pref = pref_HashTableLookup(aPrefName);
    if (pref && pref->IsLocked()) {
      return true;
    }
  }

  return false;
}

/* static */ nsresult
Preferences::ClearUserInAnyProcess(const char* aPrefName)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref = pref_HashTableLookup(aPrefName);
  if (pref && pref->HasUserValue()) {
    pref->ClearUserValue();

    if (!pref->HasDefaultValue()) {
      gHashTable->RemoveEntry(pref);
    }

    NotifyCallbacks(aPrefName);
    Preferences::HandleDirty();
  }
  return NS_OK;
}

/* static */ nsresult
Preferences::ClearUser(const char* aPrefName)
{
  ENSURE_PARENT_PROCESS("ClearUser", aPrefName);
  return ClearUserInAnyProcess(aPrefName);
}

/* static */ bool
Preferences::HasUserValue(const char* aPrefName)
{
  NS_ENSURE_TRUE(InitStaticMembers(), false);

  Pref* pref = pref_HashTableLookup(aPrefName);
  return pref && pref->HasUserValue();
}

/* static */ int32_t
Preferences::GetType(const char* aPrefName, PrefValueKind aKind)
{
  NS_ENSURE_TRUE(InitStaticMembers(), nsIPrefBranch::PREF_INVALID);
  int32_t result;
  return NS_SUCCEEDED(GetRootBranch(aKind)->GetPrefType(aPrefName, &result))
           ? result
           : nsIPrefBranch::PREF_INVALID;
}

/* static */ nsresult
Preferences::AddStrongObserver(nsIObserver* aObserver, const char* aPref)
{
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->AddObserver(aPref, aObserver, false);
}

/* static */ nsresult
Preferences::AddWeakObserver(nsIObserver* aObserver, const char* aPref)
{
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->AddObserver(aPref, aObserver, true);
}

/* static */ nsresult
Preferences::RemoveObserver(nsIObserver* aObserver, const char* aPref)
{
  MOZ_ASSERT(aObserver);
  if (sShutdown) {
    MOZ_ASSERT(!sPreferences);
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->RemoveObserver(aPref, aObserver);
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
  if (sShutdown) {
    MOZ_ASSERT(!sPreferences);
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = RemoveObserver(aObserver, aPrefs[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/* static */ nsresult
Preferences::RegisterCallback(PrefChangedFunc aCallback,
                              const char* aPrefNode,
                              void* aData,
                              MatchKind aMatchKind,
                              bool aIsPriority)
{
  NS_ENSURE_ARG(aPrefNode);
  NS_ENSURE_ARG(aCallback);

  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  auto node = new CallbackNode(aPrefNode, aCallback, aData, aMatchKind);

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

  return NS_OK;
}

/* static */ nsresult
Preferences::RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                     const char* aPref,
                                     void* aClosure,
                                     MatchKind aMatchKind)
{
  MOZ_ASSERT(aCallback);
  nsresult rv = RegisterCallback(aCallback, aPref, aClosure, aMatchKind);
  if (NS_SUCCEEDED(rv)) {
    AUTO_INSTALLING_CALLBACK();
    (*aCallback)(aPref, aClosure);
  }
  return rv;
}

/* static */ nsresult
Preferences::UnregisterCallback(PrefChangedFunc aCallback,
                                const char* aPrefNode,
                                void* aData,
                                MatchKind aMatchKind)
{
  MOZ_ASSERT(aCallback);
  if (sShutdown) {
    MOZ_ASSERT(!sPreferences);
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  nsresult rv = NS_ERROR_FAILURE;
  CallbackNode* node = gFirstCallback;
  CallbackNode* prev_node = nullptr;

  while (node) {
    if (node->mFunc == aCallback && node->mData == aData &&
        node->mMatchKind == aMatchKind &&
        strcmp(node->mDomain.get(), aPrefNode) == 0) {
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

static void
CacheDataAppendElement(CacheData* aData)
{
  if (!gCacheData) {
    MOZ_CRASH_UNSAFE_PRINTF("!gCacheData: %s", gCacheDataDesc);
  }
  gCacheData->AppendElement(aData);
}

static void
BoolVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<bool*>(cache->mCacheLocation) =
    Preferences::GetBool(aPref, cache->mDefaultValueBool);
}

/* static */ nsresult
Preferences::AddBoolVarCache(bool* aCache, const char* aPref, bool aDefault)
{
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("bool", aPref, aCache);
#endif
  {
    AUTO_INSTALLING_CALLBACK();
    *aCache = GetBool(aPref, aDefault);
  }
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueBool = aDefault;
  CacheDataAppendElement(data);
  Preferences::RegisterCallback(BoolVarChanged,
                                aPref,
                                data,
                                Preferences::ExactMatch,
                                /* isPriority */ true);
  return NS_OK;
}

template<MemoryOrdering Order>
static void
AtomicBoolVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<Atomic<bool, Order>*>(cache->mCacheLocation) =
    Preferences::GetBool(aPref, cache->mDefaultValueBool);
}

template<MemoryOrdering Order>
/* static */ nsresult
Preferences::AddAtomicBoolVarCache(Atomic<bool, Order>* aCache,
                                   const char* aPref,
                                   bool aDefault)
{
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("bool", aPref, aCache);
#endif
  {
    AUTO_INSTALLING_CALLBACK();
    *aCache = Preferences::GetBool(aPref, aDefault);
  }
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueBool = aDefault;
  CacheDataAppendElement(data);
  Preferences::RegisterCallback(AtomicBoolVarChanged<Order>,
                                aPref,
                                data,
                                Preferences::ExactMatch,
                                /* isPriority */ true);
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
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("int", aPref, aCache);
#endif
  {
    AUTO_INSTALLING_CALLBACK();
    *aCache = Preferences::GetInt(aPref, aDefault);
  }
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueInt = aDefault;
  CacheDataAppendElement(data);
  Preferences::RegisterCallback(
    IntVarChanged, aPref, data, Preferences::ExactMatch, /* isPriority */ true);
  return NS_OK;
}

template<MemoryOrdering Order>
static void
AtomicIntVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<Atomic<int32_t, Order>*>(cache->mCacheLocation) =
    Preferences::GetInt(aPref, cache->mDefaultValueUint);
}

template<MemoryOrdering Order>
/* static */ nsresult
Preferences::AddAtomicIntVarCache(Atomic<int32_t, Order>* aCache,
                                  const char* aPref,
                                  int32_t aDefault)
{
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("int", aPref, aCache);
#endif
  {
    AUTO_INSTALLING_CALLBACK();
    *aCache = Preferences::GetInt(aPref, aDefault);
  }
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueUint = aDefault;
  CacheDataAppendElement(data);
  Preferences::RegisterCallback(AtomicIntVarChanged<Order>,
                                aPref,
                                data,
                                Preferences::ExactMatch,
                                /* isPriority */ true);
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
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("uint", aPref, aCache);
#endif
  {
    AUTO_INSTALLING_CALLBACK();
    *aCache = Preferences::GetUint(aPref, aDefault);
  }
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueUint = aDefault;
  CacheDataAppendElement(data);
  Preferences::RegisterCallback(UintVarChanged,
                                aPref,
                                data,
                                Preferences::ExactMatch,
                                /* isPriority */ true);
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
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("uint", aPref, aCache);
#endif
  {
    AUTO_INSTALLING_CALLBACK();
    *aCache = Preferences::GetUint(aPref, aDefault);
  }
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueUint = aDefault;
  CacheDataAppendElement(data);
  Preferences::RegisterCallback(AtomicUintVarChanged<Order>,
                                aPref,
                                data,
                                Preferences::ExactMatch,
                                /* isPriority */ true);
  return NS_OK;
}

// Since the definition of template functions is not in a header file, we
// need to explicitly specify the instantiations that are required. Currently
// only the order=Relaxed variant is needed.
template nsresult
Preferences::AddAtomicBoolVarCache(Atomic<bool, Relaxed>*, const char*, bool);

template nsresult
Preferences::AddAtomicIntVarCache(Atomic<int32_t, Relaxed>*,
                                  const char*,
                                  int32_t);

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
  NS_ASSERTION(aCache, "aCache must not be NULL");
#ifdef DEBUG
  AssertNotAlreadyCached("float", aPref, aCache);
#endif
  {
    AUTO_INSTALLING_CALLBACK();
    *aCache = Preferences::GetFloat(aPref, aDefault);
  }
  CacheData* data = new CacheData();
  data->mCacheLocation = aCache;
  data->mDefaultValueFloat = aDefault;
  CacheDataAppendElement(data);
  Preferences::RegisterCallback(FloatVarChanged,
                                aPref,
                                data,
                                Preferences::ExactMatch,
                                /* isPriority */ true);
  return NS_OK;
}

} // namespace mozilla

#undef ENSURE_PARENT_PROCESS

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
