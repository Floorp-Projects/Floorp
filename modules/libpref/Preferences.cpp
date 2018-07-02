/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "SharedPrefMap.h"

#include "base/basictypes.h"
#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "mozilla/ArenaAllocatorExtensions.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
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
#include "mozilla/StaticPrefs.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/SystemGroup.h"
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
#include "nsHashKeys.h"
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

#ifdef MOZ_MEMORY
#include "mozmemory.h"
#endif

#ifdef XP_WIN
#include "windows.h"
#endif

using namespace mozilla;

using mozilla::ipc::FileDescriptor;

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

// This is used for pref names and string pref values. We encode the string
// length, then a '/', then the string chars. This encoding means there are no
// special chars that are forbidden or require escaping.
static void
SerializeAndAppendString(const char* aChars, nsCString& aStr)
{
  aStr.AppendInt(uint32_t(strlen(aChars)));
  aStr.Append('/');
  aStr.Append(aChars);
}

static char*
DeserializeString(char* aChars, nsCString& aStr)
{
  char* p = aChars;
  uint32_t length = strtol(p, &p, 10);
  MOZ_ASSERT(p[0] == '/');
  p++; // move past the '/'
  aStr.Assign(p, length);
  p += length; // move past the string itself
  return p;
}

// Keep this in sync with PrefValue in prefs_parser/src/lib.rs.
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

  template<typename T>
  T Get() const;

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
    }

    // Zero the entire value (regardless of type) via mStringVal.
    mStringVal = nullptr;
  }

  void Replace(bool aHasValue,
               PrefType aOldType,
               PrefType aNewType,
               PrefValue aNewValue)
  {
    if (aHasValue) {
      Clear(aOldType);
    }
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

  void SerializeAndAppend(PrefType aType, nsCString& aStr)
  {
    switch (aType) {
      case PrefType::Bool:
        aStr.Append(mBoolVal ? 'T' : 'F');
        break;

      case PrefType::Int:
        aStr.AppendInt(mIntVal);
        break;

      case PrefType::String: {
        SerializeAndAppendString(mStringVal, aStr);
        break;
      }

      case PrefType::None:
      default:
        MOZ_CRASH();
    }
  }

  static char* Deserialize(PrefType aType,
                           char* aStr,
                           dom::MaybePrefValue* aDomValue)
  {
    char* p = aStr;

    switch (aType) {
      case PrefType::Bool:
        if (*p == 'T') {
          *aDomValue = true;
        } else if (*p == 'F') {
          *aDomValue = false;
        } else {
          *aDomValue = false;
          NS_ERROR("bad bool pref value");
        }
        p++;
        return p;

      case PrefType::Int: {
        *aDomValue = int32_t(strtol(p, &p, 10));
        return p;
      }

      case PrefType::String: {
        nsCString str;
        p = DeserializeString(p, str);
        *aDomValue = str;
        return p;
      }

      default:
        MOZ_CRASH();
    }
  }
};

template<>
bool
PrefValue::Get() const
{
  return mBoolVal;
}

template<>
int32_t
PrefValue::Get() const
{
  return mIntVal;
}

template<>
nsDependentCString
PrefValue::Get() const
{
  return nsDependentCString(mStringVal);
}

#ifdef DEBUG
const char*
PrefTypeToString(PrefType aType)
{
  switch (aType) {
    case PrefType::None:
      return "none";
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

namespace mozilla {
struct PrefsSizes
{
  PrefsSizes()
    : mHashTable(0)
    , mPrefValues(0)
    , mStringValues(0)
    , mCacheData(0)
    , mRootBranches(0)
    , mPrefNameArena(0)
    , mCallbacksObjects(0)
    , mCallbacksDomains(0)
    , mMisc(0)
  {
  }

  size_t mHashTable;
  size_t mPrefValues;
  size_t mStringValues;
  size_t mCacheData;
  size_t mRootBranches;
  size_t mPrefNameArena;
  size_t mCallbacksObjects;
  size_t mCallbacksDomains;
  size_t mMisc;
};
}

static ArenaAllocator<8192, 1> gPrefNameArena;

class Pref
{
public:
  explicit Pref(const char* aName)
    : mName(ArenaStrdup(aName, gPrefNameArena))
    , mType(static_cast<uint32_t>(PrefType::None))
    , mIsSticky(false)
    , mIsLocked(false)
    , mHasDefaultValue(false)
    , mHasUserValue(false)
    , mHasChangedSinceInit(false)
    , mDefaultValue()
    , mUserValue()
  {
  }

  ~Pref()
  {
    // There's no need to free mName because it's allocated in memory owned by
    // gPrefNameArena.

    mDefaultValue.Clear(Type());
    mUserValue.Clear(Type());
  }

  const char* Name() const { return mName; }
  nsDependentCString NameString() const { return nsDependentCString(mName); }

  // Types.

  PrefType Type() const { return static_cast<PrefType>(mType); }
  void SetType(PrefType aType) { mType = static_cast<uint32_t>(aType); }

  bool IsType(PrefType aType) const { return Type() == aType; }
  bool IsTypeNone() const { return IsType(PrefType::None); }
  bool IsTypeString() const { return IsType(PrefType::String); }
  bool IsTypeInt() const { return IsType(PrefType::Int); }
  bool IsTypeBool() const { return IsType(PrefType::Bool); }

  // Other properties.

  bool IsLocked() const { return mIsLocked; }
  void SetIsLocked(bool aValue)
  {
    mIsLocked = aValue;
    mHasChangedSinceInit = true;
  }

  bool IsSticky() const { return mIsSticky; }

  bool HasDefaultValue() const { return mHasDefaultValue; }
  bool HasUserValue() const { return mHasUserValue; }

  template<typename T>
  void AddToMap(SharedPrefMapBuilder& aMap)
  {
    aMap.Add(Name(),
             { HasDefaultValue(), HasUserValue(), IsSticky(), IsLocked() },
             HasDefaultValue() ? mDefaultValue.Get<T>() : T(),
             HasUserValue() ? mUserValue.Get<T>() : T());
  }

  void AddToMap(SharedPrefMapBuilder& aMap)
  {
    if (IsTypeBool()) {
      AddToMap<bool>(aMap);
    } else if (IsTypeInt()) {
      AddToMap<int32_t>(aMap);
    } else if (IsTypeString()) {
      AddToMap<nsDependentCString>(aMap);
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected preference type");
    }
  }

  // When a content process is created we could tell it about every pref. But
  // the content process also initializes prefs from file, so we save a lot of
  // IPC if we only tell it about prefs that have changed since initialization.
  //
  // Specifically, we send a pref if any of the following conditions are met.
  //
  // - If the pref has changed in any way (default value, user value, or other
  //   attribute, such as whether it is locked) since being initialized from
  //   file.
  //
  // - If the pref has a user value. (User values are more complicated than
  //   default values, because they can be loaded from file after
  //   initialization with Preferences::ReadUserPrefsFromFile(), so we are
  //   conservative with them.)
  //
  // In other words, prefs that only have a default value and haven't changed
  // need not be sent. One could do better with effort, but it's ok to be
  // conservative and this still greatly reduces the number of prefs sent.
  //
  // Note: This function is only useful in the parent process.
  bool MustSendToContentProcesses() const
  {
    MOZ_ASSERT(XRE_IsParentProcess());
    return mHasUserValue || mHasChangedSinceInit;
  }

  // Other operations.

  bool MatchEntry(const char* aPrefName)
  {
    if (!mName || !aPrefName) {
      return false;
    }

    return strcmp(mName, aPrefName) == 0;
  }

  bool GetBoolValue(PrefValueKind aKind = PrefValueKind::User) const
  {
    MOZ_ASSERT(IsTypeBool());
    MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                               : HasUserValue());

    return aKind == PrefValueKind::Default ? mDefaultValue.mBoolVal
                                           : mUserValue.mBoolVal;
  }

  int32_t GetIntValue(PrefValueKind aKind = PrefValueKind::User) const
  {
    MOZ_ASSERT(IsTypeInt());
    MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                               : HasUserValue());

    return aKind == PrefValueKind::Default ? mDefaultValue.mIntVal
                                           : mUserValue.mIntVal;
  }

  const char* GetBareStringValue(
    PrefValueKind aKind = PrefValueKind::User) const
  {
    MOZ_ASSERT(IsTypeString());
    MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                               : HasUserValue());

    return aKind == PrefValueKind::Default ? mDefaultValue.mStringVal
                                           : mUserValue.mStringVal;
  }

  nsDependentCString GetStringValue(
    PrefValueKind aKind = PrefValueKind::User) const
  {
    return nsDependentCString(GetBareStringValue(aKind));
  }

  void ToDomPref(dom::Pref* aDomPref)
  {
    MOZ_ASSERT(XRE_IsParentProcess());

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
    MOZ_ASSERT(!XRE_IsParentProcess());
    MOZ_ASSERT(strcmp(mName, aDomPref.name().get()) == 0);

    mIsLocked = aDomPref.isLocked();

    const dom::MaybePrefValue& defaultValue = aDomPref.defaultValue();
    bool defaultValueChanged = false;
    if (defaultValue.type() == dom::MaybePrefValue::TPrefValue) {
      PrefValue value;
      PrefType type = value.FromDomPrefValue(defaultValue.get_PrefValue());
      if (!ValueMatches(PrefValueKind::Default, type, value)) {
        // Type() is PrefType::None if it's a newly added pref. This is ok.
        mDefaultValue.Replace(mHasDefaultValue, Type(), type, value);
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
        mUserValue.Replace(mHasUserValue, Type(), type, value);
        SetType(type);
        mHasUserValue = true;
        userValueChanged = true;
      }
    } else if (mHasUserValue) {
      ClearUserValue();
      userValueChanged = true;
    }

    mHasChangedSinceInit = true;

    if (userValueChanged || (defaultValueChanged && !mHasUserValue)) {
      *aValueChanged = true;
    }
  }

  bool HasAdvisablySizedValues()
  {
    MOZ_ASSERT(XRE_IsParentProcess());

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
    mUserValue.Clear(Type());
    mHasUserValue = false;
    mHasChangedSinceInit = true;
  }

  nsresult SetDefaultValue(PrefType aType,
                           PrefValue aValue,
                           bool aIsSticky,
                           bool aIsLocked,
                           bool aFromInit,
                           bool* aValueChanged)
  {
    // Types must always match when setting the default value.
    if (!IsType(aType)) {
      return NS_ERROR_UNEXPECTED;
    }

    // Should we set the default value? Only if the pref is not locked, and
    // doing so would change the default value.
    if (!IsLocked()) {
      if (aIsLocked) {
        SetIsLocked(true);
      }
      if (!ValueMatches(PrefValueKind::Default, aType, aValue)) {
        mDefaultValue.Replace(mHasDefaultValue, Type(), aType, aValue);
        mHasDefaultValue = true;
        if (!aFromInit) {
          mHasChangedSinceInit = true;
        }
        if (aIsSticky) {
          mIsSticky = true;
        }
        if (!mHasUserValue) {
          *aValueChanged = true;
        }
        // What if we change the default to be the same as the user value?
        // Should we clear the user value? Currently we don't.
      }
    }
    return NS_OK;
  }

  nsresult SetUserValue(PrefType aType,
                        PrefValue aValue,
                        bool aFromInit,
                        bool* aValueChanged)
  {
    // If we have a default value, types must match when setting the user
    // value.
    if (mHasDefaultValue && !IsType(aType)) {
      return NS_ERROR_UNEXPECTED;
    }

    // Should we clear the user value, if present? Only if the new user value
    // matches the default value, and the pref isn't sticky, and we aren't
    // force-setting it during initialization.
    if (ValueMatches(PrefValueKind::Default, aType, aValue) && !mIsSticky &&
        !aFromInit) {
      if (mHasUserValue) {
        ClearUserValue();
        if (!IsLocked()) {
          *aValueChanged = true;
        }
      }

      // Otherwise, should we set the user value? Only if doing so would
      // change the user value.
    } else if (!ValueMatches(PrefValueKind::User, aType, aValue)) {
      mUserValue.Replace(mHasUserValue, Type(), aType, aValue);
      SetType(aType); // needed because we may have changed the type
      mHasUserValue = true;
      if (!aFromInit) {
        mHasChangedSinceInit = true;
      }
      if (!IsLocked()) {
        *aValueChanged = true;
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

  // Prefs are serialized in a manner that mirrors dom::Pref. The two should be
  // kept in sync. E.g. if something is added to one it should also be added to
  // the other. (It would be nice to be able to use the code generated from
  // IPDL for serializing dom::Pref here instead of writing by hand this
  // serialization/deserialization. Unfortunately, that generated code is
  // difficult to use directly, outside of the IPDL IPC code.)
  //
  // The grammar for the serialized prefs has the following form.
  //
  // <pref>         = <type> <locked> ':' <name> ':' <value>? ':' <value>? '\n'
  // <type>         = 'B' | 'I' | 'S'
  // <locked>       = 'L' | '-'
  // <name>         = <string-value>
  // <value>        = <bool-value> | <int-value> | <string-value>
  // <bool-value>   = 'T' | 'F'
  // <int-value>    = an integer literal accepted by strtol()
  // <string-value> = <int-value> '/' <chars>
  // <chars>        = any char sequence of length dictated by the preceding
  //                  <int-value>.
  //
  // No whitespace is tolerated between tokens. <type> must match the types of
  // the values.
  //
  // The serialization is text-based, rather than binary, for the following
  // reasons.
  //
  // - The size difference wouldn't be much different between text-based and
  //   binary. Most of the space is for strings (pref names and string pref
  //   values), which would be the same in both styles. And other differences
  //   would be minimal, e.g. small integers are shorter in text but long
  //   integers are longer in text.
  //
  // - Likewise, speed differences should be negligible.
  //
  // - It's much easier to debug a text-based serialization. E.g. you can
  //   print it and inspect it easily in a debugger.
  //
  // Examples of unlocked boolean prefs:
  // - "B-:8/my.bool1:F:T\n"
  // - "B-:8/my.bool2:F:\n"
  // - "B-:8/my.bool3::T\n"
  //
  // Examples of locked integer prefs:
  // - "IL:7/my.int1:0:1\n"
  // - "IL:7/my.int2:123:\n"
  // - "IL:7/my.int3::-99\n"
  //
  // Examples of unlocked string prefs:
  // - "S-:10/my.string1:3/abc:4/wxyz\n"
  // - "S-:10/my.string2:5/1.234:\n"
  // - "S-:10/my.string3::7/string!\n"

  void SerializeAndAppend(nsCString& aStr)
  {
    switch (Type()) {
      case PrefType::Bool:
        aStr.Append('B');
        break;

      case PrefType::Int:
        aStr.Append('I');
        break;

      case PrefType::String: {
        aStr.Append('S');
        break;
      }

      case PrefType::None:
      default:
        MOZ_CRASH();
    }

    aStr.Append(mIsLocked ? 'L' : '-');
    aStr.Append(':');

    SerializeAndAppendString(mName, aStr);
    aStr.Append(':');

    if (mHasDefaultValue) {
      mDefaultValue.SerializeAndAppend(Type(), aStr);
    }
    aStr.Append(':');

    if (mHasUserValue) {
      mUserValue.SerializeAndAppend(Type(), aStr);
    }
    aStr.Append('\n');
  }

  static char* Deserialize(char* aStr, dom::Pref* aDomPref)
  {
    char* p = aStr;

    // The type.
    PrefType type;
    if (*p == 'B') {
      type = PrefType::Bool;
    } else if (*p == 'I') {
      type = PrefType::Int;
    } else if (*p == 'S') {
      type = PrefType::String;
    } else {
      NS_ERROR("bad pref type");
      type = PrefType::None;
    }
    p++; // move past the type char

    // Locked?
    bool isLocked;
    if (*p == 'L') {
      isLocked = true;
    } else if (*p == '-') {
      isLocked = false;
    } else {
      NS_ERROR("bad pref locked status");
      isLocked = false;
    }
    p++; // move past the isLocked char

    MOZ_ASSERT(*p == ':');
    p++; // move past the ':'

    // The pref name.
    nsCString name;
    p = DeserializeString(p, name);

    MOZ_ASSERT(*p == ':');
    p++; // move past the ':' preceding the default value

    dom::MaybePrefValue maybeDefaultValue;
    if (*p != ':') {
      dom::PrefValue defaultValue;
      p = PrefValue::Deserialize(type, p, &maybeDefaultValue);
    }

    MOZ_ASSERT(*p == ':');
    p++; // move past the ':' between the default and user values

    dom::MaybePrefValue maybeUserValue;
    if (*p != '\n') {
      dom::PrefValue userValue;
      p = PrefValue::Deserialize(type, p, &maybeUserValue);
    }

    MOZ_ASSERT(*p == '\n');
    p++; // move past the '\n' following the user value

    *aDomPref = dom::Pref(name, isLocked, maybeDefaultValue, maybeUserValue);

    return p;
  }

  void AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf, PrefsSizes& aSizes)
  {
    // Note: mName is allocated in gPrefNameArena, measured elsewhere.
    aSizes.mPrefValues += aMallocSizeOf(this);
    if (IsTypeString()) {
      if (mHasDefaultValue) {
        aSizes.mStringValues += aMallocSizeOf(mDefaultValue.mStringVal);
      }
      if (mHasUserValue) {
        aSizes.mStringValues += aMallocSizeOf(mUserValue.mStringVal);
      }
    }
  }

private:
  const char* mName; // allocated in gPrefNameArena

  uint32_t mType : 2;
  uint32_t mIsSticky : 1;
  uint32_t mIsLocked : 1;
  uint32_t mHasDefaultValue : 1;
  uint32_t mHasUserValue : 1;
  uint32_t mHasChangedSinceInit : 1;

  PrefValue mDefaultValue;
  PrefValue mUserValue;
};

class PrefEntry : public PLDHashEntryHdr
{
public:
  Pref* mPref; // Note: this is never null in a live entry.

  static bool MatchEntry(const PLDHashEntryHdr* aEntry, const void* aKey)
  {
    auto entry = static_cast<const PrefEntry*>(aEntry);
    auto prefName = static_cast<const char*>(aKey);

    return entry->mPref->MatchEntry(prefName);
  }

  static void InitEntry(PLDHashEntryHdr* aEntry, const void* aKey)
  {
    auto entry = static_cast<PrefEntry*>(aEntry);
    auto prefName = static_cast<const char*>(aKey);

    entry->mPref = new Pref(prefName);
  }

  static void ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
  {
    auto entry = static_cast<PrefEntry*>(aEntry);

    delete entry->mPref;
    entry->mPref = nullptr;
  }
};

using PrefWrapperBase = Variant<Pref*, SharedPrefMap::Pref>;
class MOZ_STACK_CLASS PrefWrapper : public PrefWrapperBase
{
  using SharedPref = const SharedPrefMap::Pref;

public:
  MOZ_IMPLICIT PrefWrapper(Pref* aPref)
    : PrefWrapperBase(AsVariant(aPref))
  {
  }

  MOZ_IMPLICIT PrefWrapper(const SharedPrefMap::Pref& aPref)
    : PrefWrapperBase(AsVariant(aPref))
  {
  }

  // Types.

  bool IsType(PrefType aType) const { return Type() == aType; }
  bool IsTypeNone() const { return IsType(PrefType::None); }
  bool IsTypeString() const { return IsType(PrefType::String); }
  bool IsTypeInt() const { return IsType(PrefType::Int); }
  bool IsTypeBool() const { return IsType(PrefType::Bool); }

#define FORWARD(retType, method)                                               \
  retType method() const                                                       \
  {                                                                            \
    struct Matcher                                                             \
    {                                                                          \
      retType match(const Pref* aPref) { return aPref->method(); }             \
      retType match(SharedPref& aPref) { return aPref.method(); }              \
    };                                                                         \
    return match(Matcher());                                                   \
  }

  FORWARD(bool, IsLocked)
  FORWARD(bool, IsSticky)
  FORWARD(bool, HasDefaultValue)
  FORWARD(bool, HasUserValue)
  FORWARD(const char*, Name)
  FORWARD(nsCString, NameString)
  FORWARD(PrefType, Type)
#undef FORWARD

#define FORWARD(retType, method)                                               \
  retType method(PrefValueKind aKind = PrefValueKind::User) const              \
  {                                                                            \
    struct Matcher                                                             \
    {                                                                          \
      PrefValueKind mKind;                                                     \
                                                                               \
      retType match(const Pref* aPref) { return aPref->method(mKind); }        \
      retType match(SharedPref& aPref) { return aPref.method(mKind); }         \
    };                                                                         \
    return match(Matcher{ aKind });                                            \
  }

  FORWARD(bool, GetBoolValue)
  FORWARD(int32_t, GetIntValue)
  FORWARD(nsCString, GetStringValue)
  FORWARD(const char*, GetBareStringValue)
#undef FORWARD

  Result<PrefValueKind, nsresult> WantValueKind(PrefType aType,
                                                PrefValueKind aKind) const
  {
    if (Type() != aType) {
      return Err(NS_ERROR_UNEXPECTED);
    }

    if (aKind == PrefValueKind::Default || IsLocked() || !HasUserValue()) {
      if (!HasDefaultValue()) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      return PrefValueKind::Default;
    }
    return PrefValueKind::User;
  }

  nsresult GetBoolValue(PrefValueKind aKind, bool* aResult) const
  {
    PrefValueKind kind;
    MOZ_TRY_VAR(kind, WantValueKind(PrefType::Bool, aKind));

    *aResult = GetBoolValue(kind);
    return NS_OK;
  }

  nsresult GetIntValue(PrefValueKind aKind, int32_t* aResult) const
  {
    PrefValueKind kind;
    MOZ_TRY_VAR(kind, WantValueKind(PrefType::Int, aKind));

    *aResult = GetIntValue(kind);
    return NS_OK;
  }

  nsresult GetCStringValue(PrefValueKind aKind, nsACString& aResult) const
  {
    PrefValueKind kind;
    MOZ_TRY_VAR(kind, WantValueKind(PrefType::String, aKind));

    aResult = GetStringValue(kind);
    return NS_OK;
  }
};

class CallbackNode
{
public:
  CallbackNode(const nsACString& aDomain,
               PrefChangedFunc aFunc,
               void* aData,
               Preferences::MatchKind aMatchKind)
    : mDomain(aDomain)
    , mFunc(aFunc)
    , mData(aData)
    , mNextAndMatchKind(aMatchKind)
  {
  }

  // mDomain is a UniquePtr<>, so any uses of Domain() should only be temporary
  // borrows.
  const nsCString& Domain() const { return mDomain; }

  PrefChangedFunc Func() const { return mFunc; }
  void ClearFunc() { mFunc = nullptr; }

  void* Data() const { return mData; }

  Preferences::MatchKind MatchKind() const
  {
    return static_cast<Preferences::MatchKind>(mNextAndMatchKind &
                                               kMatchKindMask);
  }

  CallbackNode* Next() const
  {
    return reinterpret_cast<CallbackNode*>(mNextAndMatchKind & kNextMask);
  }

  void SetNext(CallbackNode* aNext)
  {
    uintptr_t matchKind = mNextAndMatchKind & kMatchKindMask;
    mNextAndMatchKind = reinterpret_cast<uintptr_t>(aNext);
    MOZ_ASSERT((mNextAndMatchKind & kMatchKindMask) == 0);
    mNextAndMatchKind |= matchKind;
  }

  void AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf, PrefsSizes& aSizes)
  {
    aSizes.mCallbacksObjects += aMallocSizeOf(this);
    aSizes.mCallbacksDomains +=
      mDomain.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

private:
  static const uintptr_t kMatchKindMask = uintptr_t(0x1);
  static const uintptr_t kNextMask = ~kMatchKindMask;

  nsCString mDomain;

  // If someone attempts to remove the node from the callback list while
  // NotifyCallbacks() is running, |func| is set to nullptr. Such nodes will
  // be removed at the end of NotifyCallbacks().
  PrefChangedFunc mFunc;
  void* mData;

  // Conceptually this is two fields:
  // - CallbackNode* mNext;
  // - Preferences::MatchKind mMatchKind;
  // They are combined into a tagged pointer to save memory.
  uintptr_t mNextAndMatchKind;
};

static PLDHashTable* gHashTable;

static StaticRefPtr<SharedPrefMap> gSharedMap;

// The callback list contains all the priority callbacks followed by the
// non-priority callbacks. gLastPriorityNode records where the first part ends.
static CallbackNode* gFirstCallback = nullptr;
static CallbackNode* gLastPriorityNode = nullptr;

#ifdef DEBUG
#define ACCESS_COUNTS
#endif

#ifdef ACCESS_COUNTS
using AccessCountsHashTable = nsDataHashtable<nsCStringHashKey, uint32_t>;
static AccessCountsHashTable* gAccessCounts;

static void
AddAccessCount(const nsACString& aPrefName)
{
  // FIXME: Servo reads preferences from background threads in unsafe ways (bug
  // 1474789), and triggers assertions here if we try to add usage count entries
  // from background threads.
  if (NS_IsMainThread()) {
    uint32_t& count = gAccessCounts->GetOrInsert(aPrefName);
    count++;
  }
}

static void
AddAccessCount(const char* aPrefName)
{
  AddAccessCount(nsDependentCString(aPrefName));
}
#else
static void MOZ_MAYBE_UNUSED
AddAccessCount(const nsACString& aPrefName)
{
}

static void
AddAccessCount(const char* aPrefName)
{
}
#endif

// These are only used during the call to NotifyCallbacks().
static bool gCallbacksInProgress = false;
static bool gShouldCleanupDeadNodes = false;

static PLDHashTableOps pref_HashTableOps = {
  PLDHashTable::HashStringKey, PrefEntry::MatchEntry,
  PLDHashTable::MoveEntryStub, PrefEntry::ClearEntry,
  PrefEntry::InitEntry,
};

static Pref*
pref_HashTableLookup(const char* aPrefName);

static void
NotifyCallbacks(const char* aPrefName);

#define PREF_HASHTABLE_INITIAL_LENGTH 1024

static PrefSaveData
pref_savePrefs()
{
  MOZ_ASSERT(NS_IsMainThread());

  PrefSaveData savedPrefs(gHashTable->EntryCount());

  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    Pref* pref = static_cast<PrefEntry*>(iter.Get())->mPref;

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

// Note that this never changes in the parent process, and is only read in
// content processes.
static bool gContentProcessPrefsAreInited = false;

#endif // DEBUG

static PrefEntry*
pref_HashTableLookupInner(const char* aPrefName)
{
  MOZ_ASSERT(NS_IsMainThread() || mozilla::ServoStyleSet::IsInServoTraversal());

  MOZ_ASSERT_IF(!XRE_IsParentProcess(), gContentProcessPrefsAreInited);

  return static_cast<PrefEntry*>(gHashTable->Search(aPrefName));
}

static Pref*
pref_HashTableLookup(const char* aPrefName)
{
  PrefEntry* entry = pref_HashTableLookupInner(aPrefName);
  if (!entry) {
    return nullptr;
  }

  return entry->mPref;
}

Maybe<PrefWrapper>
pref_Lookup(const char* aPrefName)
{
  Maybe<PrefWrapper> result;

  AddAccessCount(aPrefName);

  if (Pref* pref = pref_HashTableLookup(aPrefName)) {
    result.emplace(pref);
  }

  return result;
}

static nsresult
pref_SetPref(const char* aPrefName,
             PrefType aType,
             PrefValueKind aKind,
             PrefValue aValue,
             bool aIsSticky,
             bool aIsLocked,
             bool aFromInit)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gHashTable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  auto entry = static_cast<PrefEntry*>(gHashTable->Add(aPrefName, fallible));
  if (!entry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  Pref* pref = entry->mPref;
  if (pref->IsTypeNone()) {
    // New entry. Set the type.
    pref->SetType(aType);
  }

  bool valueChanged = false;
  nsresult rv;
  if (aKind == PrefValueKind::Default) {
    rv = pref->SetDefaultValue(
      aType, aValue, aIsSticky, aIsLocked, aFromInit, &valueChanged);
  } else {
    MOZ_ASSERT(!aIsLocked); // `locked` is disallowed in user pref files
    rv = pref->SetUserValue(aType, aValue, aFromInit, &valueChanged);
  }
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

  if (valueChanged) {
    if (aKind == PrefValueKind::User && XRE_IsParentProcess()) {
      Preferences::HandleDirty();
    }
    NotifyCallbacks(aPrefName);
  }

  return NS_OK;
}

// Removes |node| from callback list. Returns the node after the deleted one.
static CallbackNode*
pref_RemoveCallbackNode(CallbackNode* aNode, CallbackNode* aPrevNode)
{
  MOZ_ASSERT(!aPrevNode || aPrevNode->Next() == aNode);
  MOZ_ASSERT(aPrevNode || gFirstCallback == aNode);
  MOZ_ASSERT(!gCallbacksInProgress);

  CallbackNode* next_node = aNode->Next();
  if (aPrevNode) {
    aPrevNode->SetNext(next_node);
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

  nsDependentCString prefName(aPrefName);

  for (CallbackNode* node = gFirstCallback; node; node = node->Next()) {
    if (node->Func()) {
      bool matches = node->MatchKind() == Preferences::ExactMatch
                       ? node->Domain() == prefName
                       : StringBeginsWith(prefName, node->Domain());
      if (matches) {
        (node->Func())(aPrefName, node->Data());
      }
    }
  }

  gCallbacksInProgress = reentered;

  if (gShouldCleanupDeadNodes && !gCallbacksInProgress) {
    CallbackNode* prev_node = nullptr;
    CallbackNode* node = gFirstCallback;

    while (node) {
      if (!node->Func()) {
        node = pref_RemoveCallbackNode(node, prev_node);
      } else {
        prev_node = node;
        node = node->Next();
      }
    }
    gShouldCleanupDeadNodes = false;
  }
}

//===========================================================================
// Prefs parsing
//===========================================================================

struct TelemetryLoadData
{
  uint32_t mFileLoadSize_B;
  uint32_t mFileLoadNumPrefs;
  uint32_t mFileLoadTime_us;
};

static nsDataHashtable<nsCStringHashKey, TelemetryLoadData>* gTelemetryLoadData;

extern "C" {

// Keep this in sync with PrefFn in prefs_parser/src/lib.rs.
typedef void (*PrefsParserPrefFn)(const char* aPrefName,
                                  PrefType aType,
                                  PrefValueKind aKind,
                                  PrefValue aValue,
                                  bool aIsSticky,
                                  bool aIsLocked);

// Keep this in sync with ErrorFn in prefs_parser/src/lib.rs.
//
// `aMsg` is just a borrow of the string, and must be copied if it is used
// outside the lifetime of the prefs_parser_parse() call.
typedef void (*PrefsParserErrorFn)(const char* aMsg);

// Keep this in sync with prefs_parser_parse() in prefs_parser/src/lib.rs.
bool
prefs_parser_parse(const char* aPath,
                   PrefValueKind aKind,
                   const char* aBuf,
                   size_t aLen,
                   PrefsParserPrefFn aPrefFn,
                   PrefsParserErrorFn aErrorFn);
}

class Parser
{
public:
  Parser() = default;
  ~Parser() = default;

  bool Parse(const nsCString& aName,
             PrefValueKind aKind,
             const char* aPath,
             const TimeStamp& aStartTime,
             const nsCString& aBuf)
  {
    sNumPrefs = 0;
    bool ok = prefs_parser_parse(
      aPath, aKind, aBuf.get(), aBuf.Length(), HandlePref, HandleError);
    if (!ok) {
      return false;
    }

    uint32_t loadTime_us = (TimeStamp::Now() - aStartTime).ToMicroseconds();

    // Most prefs files are read before telemetry initializes, so we have to
    // save these measurements now and send them to telemetry later.
    TelemetryLoadData loadData = { uint32_t(aBuf.Length()),
                                   sNumPrefs,
                                   loadTime_us };
    gTelemetryLoadData->Put(aName, loadData);

    return true;
  }

private:
  static void HandlePref(const char* aPrefName,
                         PrefType aType,
                         PrefValueKind aKind,
                         PrefValue aValue,
                         bool aIsSticky,
                         bool aIsLocked)
  {
    sNumPrefs++;
    pref_SetPref(aPrefName,
                 aType,
                 aKind,
                 aValue,
                 aIsSticky,
                 aIsLocked,
                 /* fromInit */ true);
  }

  static void HandleError(const char* aMsg)
  {
    nsresult rv;
    nsCOMPtr<nsIConsoleService> console =
      do_GetService("@mozilla.org/consoleservice;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      console->LogStringMessage(NS_ConvertUTF8toUTF16(aMsg).get());
    }
#ifdef DEBUG
    NS_ERROR(aMsg);
#else
    printf_stderr("%s\n", aMsg);
#endif
  }

  // This is static so that HandlePref() can increment it easily. This is ok
  // because prefs files are read one at a time.
  static uint32_t sNumPrefs;
};

uint32_t Parser::sNumPrefs = 0;

// The following code is test code for the gtest.

static void
TestParseErrorHandlePref(const char* aPrefName,
                         PrefType aType,
                         PrefValueKind aKind,
                         PrefValue aValue,
                         bool aIsSticky,
                         bool aIsLocked)
{
}

static nsCString gTestParseErrorMsgs;

static void
TestParseErrorHandleError(const char* aMsg)
{
  gTestParseErrorMsgs.Append(aMsg);
  gTestParseErrorMsgs.Append('\n');
}

// Keep this in sync with the declaration in test/gtest/Parser.cpp.
void
TestParseError(PrefValueKind aKind, const char* aText, nsCString& aErrorMsg)
{
  prefs_parser_parse("test",
                     aKind,
                     aText,
                     strlen(aText),
                     TestParseErrorHandlePref,
                     TestParseErrorHandleError);

  // Copy the error messages into the outparam, then clear them from
  // gTestParseErrorMsgs.
  aErrorMsg.Assign(gTestParseErrorMsgs);
  gTestParseErrorMsgs.Truncate();
}

void
SendTelemetryLoadData()
{
  for (auto iter = gTelemetryLoadData->Iter(); !iter.Done(); iter.Next()) {
    const nsCString& filename = PromiseFlatCString(iter.Key());
    const TelemetryLoadData& data = iter.Data();
    Telemetry::Accumulate(
      Telemetry::PREFERENCES_FILE_LOAD_SIZE_B, filename, data.mFileLoadSize_B);
    Telemetry::Accumulate(Telemetry::PREFERENCES_FILE_LOAD_NUM_PREFS,
                          filename,
                          data.mFileLoadNumPrefs);
    Telemetry::Accumulate(Telemetry::PREFERENCES_FILE_LOAD_TIME_US,
                          filename,
                          data.mFileLoadTime_us);
  }

  gTelemetryLoadData->Clear();
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
  PrefCallback(const nsACString& aDomain,
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
  PrefCallback(const nsACString& aDomain,
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

    struct CStringMatcher
    {
      // Note: This is a reference, not an instance. It's used to pass our outer
      // method argument through to our matcher methods.
      nsACString& mStr;

      void match(const char* aVal) { mStr.Assign(aVal); }
      void match(const nsCString& aVal) { mStr.Assign(aVal); }
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

    void get(nsACString& aStr) const { match(CStringMatcher{ aStr }); }

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

  PrefName GetPrefName(const char* aPrefName) const
  {
    return GetPrefName(nsDependentCString(aPrefName));
  }

  PrefName GetPrefName(const nsACString& aPrefName) const;

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

NS_IMPL_ISUPPORTS(nsPrefBranch,
                  nsIPrefBranch,
                  nsIObserver,
                  nsISupportsWeakReference)

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
  *aRetVal = Preferences::GetType(prefName.get());
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
    Pref* pref = static_cast<PrefEntry*>(iter.Get())->mPref;

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

  MOZ_ASSERT(NS_IsMainThread());

  *aChildArray = nullptr;
  *aCount = 0;

  // This will contain a list of all the pref name strings. Allocated on the
  // stack for speed.

  const PrefName& parent = GetPrefName(aStartingAt);
  size_t parentLen = parent.Length();
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    Pref* pref = static_cast<PrefEntry*>(iter.Get())->mPref;
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
nsPrefBranch::AddObserverImpl(const nsACString& aDomain,
                              nsIObserver* aObserver,
                              bool aHoldWeak)
{
  PrefCallback* pCallback;

  NS_ENSURE_ARG(aObserver);

  nsCString prefName;
  GetPrefName(aDomain).get(prefName);

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
    pCallback = new PrefCallback(prefName, weakRefFactory, this);

  } else {
    // Construct a PrefCallback with a strong reference to the observer.
    pCallback = new PrefCallback(prefName, aObserver, this);
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
  Preferences::RegisterCallback(NotifyObserver,
                                prefName,
                                pCallback,
                                Preferences::PrefixMatch,
                                /* isPriority */ false);

  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::RemoveObserverImpl(const nsACString& aDomain,
                                 nsIObserver* aObserver)
{
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
  nsCString prefName;
  GetPrefName(aDomain).get(prefName);
  PrefCallback key(prefName, aObserver, this);
  nsAutoPtr<PrefCallback> pCallback;
  mObservers.Remove(&key, &pCallback);
  if (pCallback) {
    rv = Preferences::UnregisterCallback(
      NotifyObserver, prefName, pCallback, Preferences::PrefixMatch);
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
  nsDependentCString suffix(aNewPref + len);

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
    Preferences::UnregisterCallback(nsPrefBranch::NotifyObserver,
                                    callback->GetDomain(),
                                    callback,
                                    Preferences::PrefixMatch);
    iter.Remove();
  }
  mFreeingObserverList = false;
}

void
nsPrefBranch::RemoveExpiredCallback(PrefCallback* aCallback)
{
  MOZ_ASSERT(aCallback->IsExpired());
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
nsPrefBranch::GetPrefName(const nsACString& aPrefName) const
{
  if (mPrefRoot.IsEmpty()) {
    return PrefName(PromiseFlatCString(aPrefName));
  }

  return PrefName(mPrefRoot + aPrefName);
}

//----------------------------------------------------------------------------
// nsPrefLocalizedString
//----------------------------------------------------------------------------

nsPrefLocalizedString::nsPrefLocalizedString() = default;

nsPrefLocalizedString::~nsPrefLocalizedString() = default;

NS_IMPL_ISUPPORTS(nsPrefLocalizedString,
                  nsIPrefLocalizedString,
                  nsISupportsString)

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
openPrefFile(nsIFile* aFile, PrefValueKind aKind);

// clang-format off
static const char kPrefFileHeader[] =
  "// Mozilla User Preferences"
  NS_LINEBREAK
  NS_LINEBREAK
  "// DO NOT EDIT THIS FILE."
  NS_LINEBREAK
  "//"
  NS_LINEBREAK
  "// If you make changes to this file while the application is running,"
  NS_LINEBREAK
  "// the changes will be overwritten when the application exits."
  NS_LINEBREAK
  "//"
  NS_LINEBREAK
  "// To change a preference value, you can either:"
  NS_LINEBREAK
  "// - modify it via the UI (e.g. via about:config in the browser); or"
  NS_LINEBREAK
  "// - set it within a user.js file in your profile."
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
    MOZ_ASSERT(safeStream, "expected a safe output stream!");
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
#endif

static void
AssertNotAlreadyCached(const char* aPrefType, const char* aPref, void* aPtr)
{
#ifdef DEBUG
  MOZ_ASSERT(aPtr);
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
#endif
}

static void
AssertNotAlreadyCached(const char* aPrefType,
                       const nsACString& aPref,
                       void* aPtr)
{
  AssertNotAlreadyCached(aPrefType, PromiseFlatCString(aPref).get(), aPtr);
}

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
      Pref* pref = static_cast<PrefEntry*>(iter.Get())->mPref;
      pref->AddSizeOfIncludingThis(mallocSizeOf, sizes);
    }
  }

  if (gCacheData) {
    sizes.mCacheData += gCacheData->ShallowSizeOfIncludingThis(mallocSizeOf);
    for (uint32_t i = 0, count = gCacheData->Length(); i < count; ++i) {
      sizes.mCacheData += mallocSizeOf((*gCacheData)[i]);
    }
  }

  sizes.mPrefNameArena += gPrefNameArena.SizeOfExcludingThis(mallocSizeOf);

  for (CallbackNode* node = gFirstCallback; node; node = node->Next()) {
    node->AddSizeOfIncludingThis(mallocSizeOf, sizes);
  }

  if (gSharedMap) {
    sizes.mMisc += mallocSizeOf(gSharedMap);
  }

  MOZ_COLLECT_REPORT("explicit/preferences/hash-table",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mHashTable,
                     "Memory used by libpref's hash table.");

  MOZ_COLLECT_REPORT("explicit/preferences/pref-values",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mPrefValues,
                     "Memory used by PrefValues hanging off the hash table.");

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

  MOZ_COLLECT_REPORT("explicit/preferences/callbacks/objects",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mCallbacksObjects,
                     "Memory used by pref callback objects.");

  MOZ_COLLECT_REPORT("explicit/preferences/callbacks/domains",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mCallbacksDomains,
                     "Memory used by pref callback domains (pref names and "
                     "prefixes).");

  MOZ_COLLECT_REPORT("explicit/preferences/misc",
                     KIND_HEAP,
                     UNITS_BYTES,
                     sizes.mMisc,
                     "Miscellaneous memory used by libpref.");

  if (gSharedMap) {
    if (XRE_IsParentProcess()) {
      MOZ_COLLECT_REPORT("explicit/preferences/shared-memory-map",
                         KIND_NONHEAP,
                         UNITS_BYTES,
                         gSharedMap->MapSize(),
                         "The shared memory mapping used to share a "
                         "snapshot of preference values across processes.");
    }
  }

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

    uint32_t oldCount = 0;
    prefCounter.Get(callback->GetDomain(), &oldCount);
    uint32_t currentCount = oldCount + 1;
    prefCounter.Put(callback->GetDomain(), currentCount);

    // Keep track of preferences that have a suspiciously large number of
    // referents (a symptom of a leak).
    if (currentCount == kSuspectReferentCount) {
      suspectPreferences.AppendElement(callback->GetDomain());
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

// A list of changed prefs sent from the parent via shared memory.
static InfallibleTArray<dom::Pref>* gChangedDomPrefs;

static const char kTelemetryPref[] = "toolkit.telemetry.enabled";
static const char kChannelPref[] = "app.update.channel";

#ifdef MOZ_WIDGET_ANDROID

static Maybe<bool>
TelemetryPrefValue()
{
  // Leave it unchanged if it's already set.
  // XXX: how could it already be set?
  if (Preferences::GetType(kTelemetryPref) != nsIPrefBranch::PREF_INVALID) {
    return Nothing();
  }

    // Determine the correct default for toolkit.telemetry.enabled. If this
    // build has MOZ_TELEMETRY_ON_BY_DEFAULT *or* we're on the beta channel,
    // telemetry is on by default, otherwise not. This is necessary so that
    // beta users who are testing final release builds don't flipflop defaults.
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
  return Some(true);
#else
  nsAutoCString channelPrefValue;
  Unused << Preferences::GetCString(
    kChannelPref, channelPrefValue, PrefValueKind::Default);
  return Some(channelPrefValue.EqualsLiteral("beta"));
#endif
}

/* static */ void
Preferences::SetupTelemetryPref()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  Maybe<bool> telemetryPrefValue = TelemetryPrefValue();
  if (telemetryPrefValue.isSome()) {
    Preferences::SetBool(
      kTelemetryPref, *telemetryPrefValue, PrefValueKind::Default);
  }
}

#else // !MOZ_WIDGET_ANDROID

static bool
TelemetryPrefValue()
{
  // For platforms with Unified Telemetry (here meaning not-Android),
  // toolkit.telemetry.enabled determines whether we send "extended" data.
  // We only want extended data from pre-release channels due to size.

  NS_NAMED_LITERAL_CSTRING(channel, NS_STRINGIFY(MOZ_UPDATE_CHANNEL));

  // Easy cases: Nightly, Aurora, Beta.
  if (channel.EqualsLiteral("nightly") || channel.EqualsLiteral("aurora") ||
      channel.EqualsLiteral("beta")) {
    return true;
  }

#ifndef MOZILLA_OFFICIAL
  // Local developer builds: non-official builds on the "default" channel.
  if (channel.EqualsLiteral("default")) {
    return true;
  }
#endif

  // Release Candidate builds: builds that think they are release builds, but
  // are shipped to beta users.
  if (channel.EqualsLiteral("release")) {
    nsAutoCString channelPrefValue;
    Unused << Preferences::GetCString(
      kChannelPref, channelPrefValue, PrefValueKind::Default);
    if (channelPrefValue.EqualsLiteral("beta")) {
      return true;
    }
  }

  return false;
}

/* static */ void
Preferences::SetupTelemetryPref()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  Preferences::SetBool(
    kTelemetryPref, TelemetryPrefValue(), PrefValueKind::Default);
  Preferences::Lock(kTelemetryPref);
}

static void
CheckTelemetryPref()
{
  MOZ_ASSERT(!XRE_IsParentProcess());

  // Make sure the children got passed the right telemetry pref details.
  DebugOnly<bool> value;
  MOZ_ASSERT(NS_SUCCEEDED(Preferences::GetBool(kTelemetryPref, &value)) &&
             value == TelemetryPrefValue());
  MOZ_ASSERT(Preferences::IsLocked(kTelemetryPref));
}

#endif // MOZ_WIDGET_ANDROID

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
    &pref_HashTableOps, sizeof(PrefEntry), PREF_HASHTABLE_INITIAL_LENGTH);

  gTelemetryLoadData =
    new nsDataHashtable<nsCStringHashKey, TelemetryLoadData>();

#ifdef ACCESS_COUNTS
  MOZ_ASSERT(!gAccessCounts);
  gAccessCounts = new AccessCountsHashTable();
#endif

  gCacheData = new nsTArray<nsAutoPtr<CacheData>>();
  gCacheDataDesc = "set by GetInstanceForService() (1)";

  Result<Ok, const char*> res = InitInitialObjects(/* isStartup */ true);
  if (res.isErr()) {
    sPreferences = nullptr;
    gCacheDataDesc = res.unwrapErr();
    return nullptr;
  }

  if (!XRE_IsParentProcess()) {
    MOZ_ASSERT(gChangedDomPrefs);
    for (unsigned int i = 0; i < gChangedDomPrefs->Length(); i++) {
      Preferences::SetPreference(gChangedDomPrefs->ElementAt(i));
    }
    delete gChangedDomPrefs;
    gChangedDomPrefs = nullptr;

#ifndef MOZ_WIDGET_ANDROID
    CheckTelemetryPref();
#endif

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

  gCacheDataDesc = "set by GetInstanceForService() (2)";

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

  MOZ_ASSERT(!gCallbacksInProgress);

  CallbackNode* node = gFirstCallback;
  while (node) {
    CallbackNode* next_node = node->Next();
    delete node;
    node = next_node;
  }
  gLastPriorityNode = gFirstCallback = nullptr;

  delete gHashTable;
  gHashTable = nullptr;

  delete gTelemetryLoadData;
  gTelemetryLoadData = nullptr;

#ifdef ACCESS_COUNTS
  delete gAccessCounts;
#endif

  gSharedMap = nullptr;

  gPrefNameArena.Clear();
}

NS_IMPL_ISUPPORTS(Preferences,
                  nsIPrefService,
                  nsIObserver,
                  nsIPrefBranch,
                  nsISupportsWeakReference)

/* static */ void
Preferences::SerializePreferences(nsCString& aStr)
{
  MOZ_RELEASE_ASSERT(InitStaticMembers());

  aStr.Truncate();

  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    Pref* pref = static_cast<PrefEntry*>(iter.Get())->mPref;
    if (pref->MustSendToContentProcesses() && pref->HasAdvisablySizedValues()) {
      pref->SerializeAndAppend(aStr);
    }
  }

  aStr.Append('\0');
}

/* static */ void
Preferences::DeserializePreferences(char* aStr, size_t aPrefsLen)
{
  MOZ_ASSERT(!XRE_IsParentProcess());

  MOZ_ASSERT(!gChangedDomPrefs);
  gChangedDomPrefs = new InfallibleTArray<dom::Pref>();

  char* p = aStr;
  while (*p != '\0') {
    dom::Pref pref;
    p = Pref::Deserialize(p, &pref);
    gChangedDomPrefs->AppendElement(pref);
  }

  // We finished parsing on a '\0'. That should be the last char in the shared
  // memory. (aPrefsLen includes the '\0'.)
  MOZ_ASSERT(p == aStr + aPrefsLen - 1);

#ifdef DEBUG
  MOZ_ASSERT(!gContentProcessPrefsAreInited);
  gContentProcessPrefsAreInited = true;
#endif
}

/* static */ FileDescriptor
Preferences::EnsureSnapshot(size_t* aSize)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!gSharedMap) {
    SharedPrefMapBuilder builder;

    for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
      Pref* pref = static_cast<PrefEntry*>(iter.Get())->mPref;

      pref->AddToMap(builder);
    }

    gSharedMap = new SharedPrefMap(std::move(builder));
  }

  *aSize = gSharedMap->MapSize();
  return gSharedMap->CloneFileDescriptor();
}

/* static */ void
Preferences::InitSnapshot(const FileDescriptor& aHandle, size_t aSize)
{
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(!gSharedMap);

  gSharedMap = new SharedPrefMap(aHandle, aSize);
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

  // At this point all the prefs files have been read and telemetry has been
  // initialized. Send all the file load measurements to telemetry.
  SendTelemetryLoadData();
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
    Unused << InitInitialObjects(/* isStartup */ false);

  } else if (!nsCRT::strcmp(aTopic, "suspend_process_notification")) {
    // Our process is being suspended. The OS may wake our process later,
    // or it may kill the process. In case our process is going to be killed
    // from the suspended state, we save preferences before suspending.
    rv = SavePrefFileBlocking();
  }

  return rv;
}

NS_IMETHODIMP
Preferences::ReadDefaultPrefsFromFile(nsIFile* aFile)
{
  ENSURE_PARENT_PROCESS("Preferences::ReadDefaultPrefsFromFile", "all prefs");

  if (!aFile) {
    NS_ERROR("ReadDefaultPrefsFromFile requires a parameter");
    return NS_ERROR_INVALID_ARG;
  }

  return openPrefFile(aFile, PrefValueKind::Default);
}

NS_IMETHODIMP
Preferences::ReadUserPrefsFromFile(nsIFile* aFile)
{
  ENSURE_PARENT_PROCESS("Preferences::ReadUserPrefsFromFile", "all prefs");

  if (!aFile) {
    NS_ERROR("ReadUserPrefsFromFile requires a parameter");
    return NS_ERROR_INVALID_ARG;
  }

  return openPrefFile(aFile, PrefValueKind::User);
}

NS_IMETHODIMP
Preferences::ResetPrefs()
{
  ENSURE_PARENT_PROCESS("Preferences::ResetPrefs", "all prefs");

  gHashTable->ClearAndPrepareForLength(PREF_HASHTABLE_INITIAL_LENGTH);
  gPrefNameArena.Clear();

  return InitInitialObjects(/* isStartup */ false).isOk() ? NS_OK
                                                          : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Preferences::ResetUserPrefs()
{
  ENSURE_PARENT_PROCESS("Preferences::ResetUserPrefs", "all prefs");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  MOZ_ASSERT(NS_IsMainThread());

  Vector<const char*> prefNames;
  for (auto iter = gHashTable->Iter(); !iter.Done(); iter.Next()) {
    Pref* pref = static_cast<PrefEntry*>(iter.Get())->mPref;

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

  auto entry = static_cast<PrefEntry*>(gHashTable->Add(prefName, fallible));
  if (!entry) {
    return;
  }

  Pref* pref = entry->mPref;

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
    gHashTable->RemoveEntry(entry);
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
  MOZ_ASSERT(XRE_IsParentProcess());

  Pref* pref = pref_HashTableLookup(aDomPref->name().get());
  if (pref && pref->HasAdvisablySizedValues()) {
    pref->ToDomPref(aDomPref);
  }
}

#ifdef DEBUG
bool
Preferences::ArePrefsInitedInContentProcess()
{
  MOZ_ASSERT(!XRE_IsParentProcess());
  return gContentProcessPrefsAreInited;
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
Preferences::ReadStats(nsIPrefStatsCallback* aCallback)
{
#ifdef ACCESS_COUNTS
  for (auto iter = gAccessCounts->Iter(); !iter.Done(); iter.Next()) {
    aCallback->Visit(iter.Key(), iter.Data());
  }

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
Preferences::ResetStats()
{
#ifdef ACCESS_COUNTS
  gAccessCounts->Clear();
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
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

  rv = openPrefFile(file, PrefValueKind::User);
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
  rv = openPrefFile(aFile, PrefValueKind::User);
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
openPrefFile(nsIFile* aFile, PrefValueKind aKind)
{
  TimeStamp startTime = TimeStamp::Now();

  nsCString data;
  MOZ_TRY_VAR(data, URLPreloader::ReadFile(aFile));

  nsAutoString filenameUtf16;
  aFile->GetLeafName(filenameUtf16);
  NS_ConvertUTF16toUTF8 filename(filenameUtf16);

  nsAutoString path;
  aFile->GetPath(path);

  Parser parser;
  if (!parser.Parse(
        filename, aKind, NS_ConvertUTF16toUTF8(path).get(), startTime, data)) {
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

  nsCOMPtr<nsIDirectoryEnumerator> dirIterator;

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

  nsCOMArray<nsIFile> prefFiles(INITIAL_PREF_FILES);
  nsCOMArray<nsIFile> specialFiles(aSpecialFilesCount);
  nsCOMPtr<nsIFile> prefFile;

  while (NS_SUCCEEDED(dirIterator->GetNextFile(getter_AddRefs(prefFile))) &&
         prefFile) {
    nsAutoCString leafName;
    prefFile->GetNativeLeafName(leafName);
    MOZ_ASSERT(
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
    rv2 = openPrefFile(prefFiles[i], PrefValueKind::Default);
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
      rv2 = openPrefFile(file, PrefValueKind::Default);
      if (NS_FAILED(rv2)) {
        NS_ERROR("Special default pref file not parsed successfully.");
        rv = rv2;
      }
    }
  }

  return rv;
}

static nsresult
pref_ReadPrefFromJar(nsZipArchive* aJarReader, const char* aName)
{
  TimeStamp startTime = TimeStamp::Now();

  nsCString manifest;
  MOZ_TRY_VAR(manifest,
              URLPreloader::ReadZip(aJarReader, nsDependentCString(aName)));

  Parser parser;
  if (!parser.Parse(nsDependentCString(aName),
                    PrefValueKind::Default,
                    aName,
                    startTime,
                    manifest)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

// Initialize default preference JavaScript buffers from appropriate TEXT
// resources.
/* static */ Result<Ok, const char*>
Preferences::InitInitialObjects(bool aIsStartup)
{
  // Initialize static prefs before prefs from data files so that the latter
  // will override the former.
  StaticPrefs::InitAll(aIsStartup);

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

    rv = openPrefFile(greprefsFile, PrefValueKind::Default);
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

  nsCOMPtr<nsIProperties> dirSvc(
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(
    rv, Err("do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID) failed"));

  nsCOMPtr<nsISimpleEnumerator> list;
  dirSvc->Get(NS_APP_PREFS_DEFAULTS_DIR_LIST,
              NS_GET_IID(nsISimpleEnumerator),
              getter_AddRefs(list));
  if (list) {
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
  }

  if (XRE_IsParentProcess()) {
    SetupTelemetryPref();
  }

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
  MOZ_ASSERT(aResult);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  return pref.isSome() ? pref->GetBoolValue(aKind, aResult)
                       : NS_ERROR_UNEXPECTED;
}

/* static */ nsresult
Preferences::GetInt(const char* aPrefName,
                    int32_t* aResult,
                    PrefValueKind aKind)
{
  MOZ_ASSERT(aResult);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  return pref.isSome() ? pref->GetIntValue(aKind, aResult)
                       : NS_ERROR_UNEXPECTED;
}

/* static */ nsresult
Preferences::GetFloat(const char* aPrefName,
                      float* aResult,
                      PrefValueKind aKind)
{
  MOZ_ASSERT(aResult);

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

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  return pref.isSome() ? pref->GetCStringValue(aKind, aResult)
                       : NS_ERROR_UNEXPECTED;
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
  nsresult rv = GetLocalizedString(aPrefName, result, aKind);
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
    MOZ_ASSERT(prefLocalString, "Succeeded but the result is NULL");
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
Preferences::SetCString(const char* aPrefName,
                        const nsACString& aValue,
                        PrefValueKind aKind)
{
  ENSURE_PARENT_PROCESS("SetCString", aPrefName);
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
                      /* isLocked */ false,
                      /* fromInit */ false);
}

/* static */ nsresult
Preferences::SetBool(const char* aPrefName, bool aValue, PrefValueKind aKind)
{
  ENSURE_PARENT_PROCESS("SetBool", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  PrefValue prefValue;
  prefValue.mBoolVal = aValue;
  return pref_SetPref(aPrefName,
                      PrefType::Bool,
                      aKind,
                      prefValue,
                      /* isSticky */ false,
                      /* isLocked */ false,
                      /* fromInit */ false);
}

/* static */ nsresult
Preferences::SetInt(const char* aPrefName, int32_t aValue, PrefValueKind aKind)
{
  ENSURE_PARENT_PROCESS("SetInt", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  PrefValue prefValue;
  prefValue.mIntVal = aValue;
  return pref_SetPref(aPrefName,
                      PrefType::Int,
                      aKind,
                      prefValue,
                      /* isSticky */ false,
                      /* isLocked */ false,
                      /* fromInit */ false);
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
Preferences::Lock(const char* aPrefName)
{
  ENSURE_PARENT_PROCESS("Lock", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref = pref_HashTableLookup(aPrefName);
  if (!pref) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!pref->IsLocked()) {
    pref->SetIsLocked(true);
    NotifyCallbacks(aPrefName);
  }

  return NS_OK;
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

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  return pref.isSome() && pref->IsLocked();
}

/* static */ nsresult
Preferences::ClearUser(const char* aPrefName)
{
  ENSURE_PARENT_PROCESS("ClearUser", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  PrefEntry* entry = pref_HashTableLookupInner(aPrefName);
  Pref* pref;
  if (entry && (pref = entry->mPref) && pref->HasUserValue()) {
    pref->ClearUserValue();

    if (!pref->HasDefaultValue()) {
      gHashTable->RemoveEntry(entry);
    }

    NotifyCallbacks(aPrefName);
    Preferences::HandleDirty();
  }
  return NS_OK;
}

/* static */ bool
Preferences::HasUserValue(const char* aPrefName)
{
  NS_ENSURE_TRUE(InitStaticMembers(), false);

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  return pref.isSome() && pref->HasUserValue();
}

/* static */ int32_t
Preferences::GetType(const char* aPrefName)
{
  NS_ENSURE_TRUE(InitStaticMembers(), nsIPrefBranch::PREF_INVALID);

  if (!gHashTable) {
    return PREF_INVALID;
  }

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  if (!pref.isSome()) {
    return PREF_INVALID;
  }

  switch (pref->Type()) {
    case PrefType::String:
      return PREF_STRING;

    case PrefType::Int:
      return PREF_INT;

    case PrefType::Bool:
      return PREF_BOOL;

    default:
      MOZ_CRASH();
  }
}

/* static */ nsresult
Preferences::AddStrongObserver(nsIObserver* aObserver, const nsACString& aPref)
{
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->AddObserver(aPref, aObserver, false);
}

/* static */ nsresult
Preferences::AddWeakObserver(nsIObserver* aObserver, const nsACString& aPref)
{
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->AddObserver(aPref, aObserver, true);
}

/* static */ nsresult
Preferences::RemoveObserver(nsIObserver* aObserver, const nsACString& aPref)
{
  MOZ_ASSERT(aObserver);
  if (sShutdown) {
    MOZ_ASSERT(!sPreferences);
    return NS_OK; // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->RemoveObserver(aPref, aObserver);
}

template<typename T>
static void
AssertNotMallocAllocated(T* aPtr)
{
#if defined(DEBUG) && defined(MOZ_MEMORY)
  jemalloc_ptr_info_t info;
  jemalloc_ptr_info((void*)aPtr, &info);
  MOZ_ASSERT(info.tag == TagUnknown);
#endif
}

/* static */ nsresult
Preferences::AddStrongObservers(nsIObserver* aObserver, const char** aPrefs)
{
  MOZ_ASSERT(aObserver);
  for (uint32_t i = 0; aPrefs[i]; i++) {
    AssertNotMallocAllocated(aPrefs[i]);

    nsCString pref;
    pref.AssignLiteral(aPrefs[i], strlen(aPrefs[i]));
    nsresult rv = AddStrongObserver(aObserver, pref);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/* static */ nsresult
Preferences::AddWeakObservers(nsIObserver* aObserver, const char** aPrefs)
{
  MOZ_ASSERT(aObserver);
  for (uint32_t i = 0; aPrefs[i]; i++) {
    AssertNotMallocAllocated(aPrefs[i]);

    nsCString pref;
    pref.AssignLiteral(aPrefs[i], strlen(aPrefs[i]));
    nsresult rv = AddWeakObserver(aObserver, pref);
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
    nsresult rv = RemoveObserver(aObserver, nsDependentCString(aPrefs[i]));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/* static */ nsresult
Preferences::RegisterCallback(PrefChangedFunc aCallback,
                              const nsACString& aPrefNode,
                              void* aData,
                              MatchKind aMatchKind,
                              bool aIsPriority)
{
  NS_ENSURE_ARG(aCallback);

  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  auto node = new CallbackNode(aPrefNode, aCallback, aData, aMatchKind);

  if (aIsPriority) {
    // Add to the start of the list.
    node->SetNext(gFirstCallback);
    gFirstCallback = node;
    if (!gLastPriorityNode) {
      gLastPriorityNode = node;
    }
  } else {
    // Add to the start of the non-priority part of the list.
    if (gLastPriorityNode) {
      node->SetNext(gLastPriorityNode->Next());
      gLastPriorityNode->SetNext(node);
    } else {
      node->SetNext(gFirstCallback);
      gFirstCallback = node;
    }
  }

  return NS_OK;
}

/* static */ nsresult
Preferences::RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                     const nsACString& aPref,
                                     void* aClosure,
                                     MatchKind aMatchKind)
{
  MOZ_ASSERT(aCallback);
  nsresult rv = RegisterCallback(aCallback, aPref, aClosure, aMatchKind);
  if (NS_SUCCEEDED(rv)) {
    (*aCallback)(PromiseFlatCString(aPref).get(), aClosure);
  }
  return rv;
}

/* static */ nsresult
Preferences::UnregisterCallback(PrefChangedFunc aCallback,
                                const nsACString& aPrefNode,
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
    if (node->Func() == aCallback && node->Data() == aData &&
        node->MatchKind() == aMatchKind && node->Domain() == aPrefNode) {
      if (gCallbacksInProgress) {
        // Postpone the node removal until after callbacks enumeration is
        // finished.
        node->ClearFunc();
        gShouldCleanupDeadNodes = true;
        prev_node = node;
        node = node->Next();
      } else {
        node = pref_RemoveCallbackNode(node, prev_node);
      }
      rv = NS_OK;
    } else {
      prev_node = node;
      node = node->Next();
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
Preferences::AddBoolVarCache(bool* aCache,
                             const nsACString& aPref,
                             bool aDefault,
                             bool aSkipAssignment)
{
  AssertNotAlreadyCached("bool", aPref, aCache);
  if (!aSkipAssignment) {
    *aCache = GetBool(PromiseFlatCString(aPref).get(), aDefault);
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
                                   const nsACString& aPref,
                                   bool aDefault,
                                   bool aSkipAssignment)
{
  AssertNotAlreadyCached("bool", aPref, aCache);
  if (!aSkipAssignment) {
    *aCache = GetBool(PromiseFlatCString(aPref).get(), aDefault);
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
                            const nsACString& aPref,
                            int32_t aDefault,
                            bool aSkipAssignment)
{
  AssertNotAlreadyCached("int", aPref, aCache);
  if (!aSkipAssignment) {
    *aCache = GetInt(PromiseFlatCString(aPref).get(), aDefault);
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
                                  const nsACString& aPref,
                                  int32_t aDefault,
                                  bool aSkipAssignment)
{
  AssertNotAlreadyCached("int", aPref, aCache);
  if (!aSkipAssignment) {
    *aCache = GetInt(PromiseFlatCString(aPref).get(), aDefault);
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
                             const nsACString& aPref,
                             uint32_t aDefault,
                             bool aSkipAssignment)
{
  AssertNotAlreadyCached("uint", aPref, aCache);
  if (!aSkipAssignment) {
    *aCache = GetUint(PromiseFlatCString(aPref).get(), aDefault);
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
                                   const nsACString& aPref,
                                   uint32_t aDefault,
                                   bool aSkipAssignment)
{
  AssertNotAlreadyCached("uint", aPref, aCache);
  if (!aSkipAssignment) {
    *aCache = GetUint(PromiseFlatCString(aPref).get(), aDefault);
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
// limited orders are needed and therefore implemented.
template nsresult
Preferences::AddAtomicBoolVarCache(Atomic<bool, Relaxed>*,
                                   const nsACString&,
                                   bool,
                                   bool);

template nsresult
Preferences::AddAtomicBoolVarCache(Atomic<bool, ReleaseAcquire>*,
                                   const nsACString&,
                                   bool,
                                   bool);

template nsresult
Preferences::AddAtomicBoolVarCache(Atomic<bool, SequentiallyConsistent>*,
                                   const nsACString&,
                                   bool,
                                   bool);

template nsresult
Preferences::AddAtomicIntVarCache(Atomic<int32_t, Relaxed>*,
                                  const nsACString&,
                                  int32_t,
                                  bool);

template nsresult
Preferences::AddAtomicUintVarCache(Atomic<uint32_t, Relaxed>*,
                                   const nsACString&,
                                   uint32_t,
                                   bool);

template nsresult
Preferences::AddAtomicUintVarCache(Atomic<uint32_t, ReleaseAcquire>*,
                                   const nsACString&,
                                   uint32_t,
                                   bool);

template nsresult
Preferences::AddAtomicUintVarCache(Atomic<uint32_t, SequentiallyConsistent>*,
                                   const nsACString&,
                                   uint32_t,
                                   bool);

static void
FloatVarChanged(const char* aPref, void* aClosure)
{
  CacheData* cache = static_cast<CacheData*>(aClosure);
  *static_cast<float*>(cache->mCacheLocation) =
    Preferences::GetFloat(aPref, cache->mDefaultValueFloat);
}

/* static */ nsresult
Preferences::AddFloatVarCache(float* aCache,
                              const nsACString& aPref,
                              float aDefault,
                              bool aSkipAssignment)
{
  AssertNotAlreadyCached("float", aPref, aCache);
  if (!aSkipAssignment) {
    *aCache = GetFloat(PromiseFlatCString(aPref).get(), aDefault);
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

// For a VarCache pref like this:
//
//   VARCACHE_PREF("my.varcache", my_varcache, int32_t, 99)
//
// we generate a static variable definition:
//
//   int32_t StaticPrefs::sVarCache_my_varcache(99);
//
#define PREF(name, cpp_type, value)
#define VARCACHE_PREF(name, id, cpp_type, value)                               \
  cpp_type StaticPrefs::sVarCache_##id(value);
#include "mozilla/StaticPrefList.h"
#undef PREF
#undef VARCACHE_PREF

// The SetPref_*() functions below end in a `_<type>` suffix because they are
// used by the PREF macro definition in InitAll() below.

static void
SetPref_bool(const char* aName, bool aDefaultValue)
{
  PrefValue value;
  value.mBoolVal = aDefaultValue;
  pref_SetPref(aName,
               PrefType::Bool,
               PrefValueKind::Default,
               value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

static void
SetPref_int32_t(const char* aName, int32_t aDefaultValue)
{
  PrefValue value;
  value.mIntVal = aDefaultValue;
  pref_SetPref(aName,
               PrefType::Int,
               PrefValueKind::Default,
               value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

static void
SetPref_float(const char* aName, float aDefaultValue)
{
  PrefValue value;
  nsPrintfCString defaultValue("%f", aDefaultValue);
  value.mStringVal = defaultValue.get();
  pref_SetPref(aName,
               PrefType::String,
               PrefValueKind::Default,
               value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

// XXX: this will eventually become used
MOZ_MAYBE_UNUSED static void
SetPref_String(const char* aName, const char* aDefaultValue)
{
  PrefValue value;
  value.mStringVal = aDefaultValue;
  pref_SetPref(aName,
               PrefType::String,
               PrefValueKind::Default,
               value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

static void
InitVarCachePref(const nsACString& aName,
                 bool* aCache,
                 bool aDefaultValue,
                 bool aIsStartup)
{
  SetPref_bool(PromiseFlatCString(aName).get(), aDefaultValue);
  *aCache = aDefaultValue;
  if (aIsStartup) {
    Preferences::AddBoolVarCache(aCache, aName, aDefaultValue, true);
  }
}

template<MemoryOrdering Order>
static void
InitVarCachePref(const nsACString& aName,
                 Atomic<bool, Order>* aCache,
                 bool aDefaultValue,
                 bool aIsStartup)
{
  SetPref_bool(PromiseFlatCString(aName).get(), aDefaultValue);
  *aCache = aDefaultValue;
  if (aIsStartup) {
    Preferences::AddAtomicBoolVarCache(aCache, aName, aDefaultValue, true);
  }
}

// XXX: this will eventually become used
MOZ_MAYBE_UNUSED static void
InitVarCachePref(const nsACString& aName,
                 int32_t* aCache,
                 int32_t aDefaultValue,
                 bool aIsStartup)
{
  SetPref_int32_t(PromiseFlatCString(aName).get(), aDefaultValue);
  *aCache = aDefaultValue;
  if (aIsStartup) {
    Preferences::AddIntVarCache(aCache, aName, aDefaultValue, true);
  }
}

template<MemoryOrdering Order>
static void
InitVarCachePref(const nsACString& aName,
                 Atomic<int32_t, Order>* aCache,
                 int32_t aDefaultValue,
                 bool aIsStartup)
{
  SetPref_int32_t(PromiseFlatCString(aName).get(), aDefaultValue);
  *aCache = aDefaultValue;
  if (aIsStartup) {
    Preferences::AddAtomicIntVarCache(aCache, aName, aDefaultValue, true);
  }
}

static void
InitVarCachePref(const nsACString& aName,
                 uint32_t* aCache,
                 uint32_t aDefaultValue,
                 bool aIsStartup)
{
  SetPref_int32_t(PromiseFlatCString(aName).get(),
                  static_cast<int32_t>(aDefaultValue));
  *aCache = aDefaultValue;
  if (aIsStartup) {
    Preferences::AddUintVarCache(aCache, aName, aDefaultValue, true);
  }
}

template<MemoryOrdering Order>
static void
InitVarCachePref(const nsACString& aName,
                 Atomic<uint32_t, Order>* aCache,
                 uint32_t aDefaultValue,
                 bool aIsStartup)
{
  SetPref_int32_t(PromiseFlatCString(aName).get(),
                  static_cast<int32_t>(aDefaultValue));
  *aCache = aDefaultValue;
  if (aIsStartup) {
    Preferences::AddAtomicUintVarCache(aCache, aName, aDefaultValue, true);
  }
}

// XXX: this will eventually become used
MOZ_MAYBE_UNUSED static void
InitVarCachePref(const nsACString& aName,
                 float* aCache,
                 float aDefaultValue,
                 bool aIsStartup)
{
  SetPref_float(PromiseFlatCString(aName).get(), aDefaultValue);
  *aCache = aDefaultValue;
  if (aIsStartup) {
    Preferences::AddFloatVarCache(aCache, aName, aDefaultValue, true);
  }
}

/* static */ void
StaticPrefs::InitAll(bool aIsStartup)
{
// For prefs like these:
//
//   PREF("foo.bar.baz", bool, true)
//   VARCACHE_PREF("my.varcache", my_varcache, int32_t, 99)
//
// we generate registration calls:
//
//   SetPref_bool("foo.bar.baz", true);
//   InitVarCachePref("my.varcache", &StaticPrefs::sVarCache_my_varcache, 99,
//                    aIsStartup);
//
// The SetPref_*() functions have a type suffix to avoid ambiguity between
// prefs having int32_t and float default values. That suffix is not needed for
// the InitVarCachePref() functions because they take a pointer parameter,
// which prevents automatic int-to-float coercion.
#define PREF(name, cpp_type, value) SetPref_##cpp_type(name, value);
#define VARCACHE_PREF(name, id, cpp_type, value)                               \
  InitVarCachePref(NS_LITERAL_CSTRING(name),                                   \
                   &StaticPrefs::sVarCache_##id,                               \
                   value,                                                      \
                   aIsStartup);
#include "mozilla/StaticPrefList.h"
#undef PREF
#undef VARCACHE_PREF
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
