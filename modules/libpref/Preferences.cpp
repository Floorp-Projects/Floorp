/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Documentation for libpref is in modules/libpref/docs/index.rst.

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
#include "mozilla/Components.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefsAll.h"
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
#include "nsRelativeFilePref.h"
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
#include "xpcpublic.h"

#ifdef DEBUG
#  include <map>
#endif

#ifdef MOZ_MEMORY
#  include "mozmemory.h"
#endif

#ifdef XP_WIN
#  include "windows.h"
#endif

using namespace mozilla;

using ipc::FileDescriptor;

#ifdef DEBUG

#  define ENSURE_PARENT_PROCESS(func, pref)                                   \
    do {                                                                      \
      if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                             \
        nsPrintfCString msg(                                                  \
            "ENSURE_PARENT_PROCESS: called %s on %s in a non-parent process", \
            func, pref);                                                      \
        NS_ERROR(msg.get());                                                  \
        return NS_ERROR_NOT_AVAILABLE;                                        \
      }                                                                       \
    } while (0)

#else  // DEBUG

#  define ENSURE_PARENT_PROCESS(func, pref)     \
    if (MOZ_UNLIKELY(!XRE_IsParentProcess())) { \
      return NS_ERROR_NOT_AVAILABLE;            \
    }

#endif  // DEBUG

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
static void SerializeAndAppendString(const char* aChars, nsCString& aStr) {
  aStr.AppendInt(uint32_t(strlen(aChars)));
  aStr.Append('/');
  aStr.Append(aChars);
}

static char* DeserializeString(char* aChars, nsCString& aStr) {
  char* p = aChars;
  uint32_t length = strtol(p, &p, 10);
  MOZ_ASSERT(p[0] == '/');
  p++;  // move past the '/'
  aStr.Assign(p, length);
  p += length;  // move past the string itself
  return p;
}

// Keep this in sync with PrefValue in prefs_parser/src/lib.rs.
union PrefValue {
  const char* mStringVal;
  int32_t mIntVal;
  bool mBoolVal;

  PrefValue() = default;

  explicit PrefValue(bool aVal) : mBoolVal(aVal) {}

  explicit PrefValue(int32_t aVal) : mIntVal(aVal) {}

  explicit PrefValue(const char* aVal) : mStringVal(aVal) {}

  bool Equals(PrefType aType, PrefValue aValue) {
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

  template <typename T>
  T Get() const;

  void Init(PrefType aNewType, PrefValue aNewValue) {
    if (aNewType == PrefType::String) {
      MOZ_ASSERT(aNewValue.mStringVal);
      aNewValue.mStringVal = moz_xstrdup(aNewValue.mStringVal);
    }
    *this = aNewValue;
  }

  void Clear(PrefType aType) {
    if (aType == PrefType::String) {
      free(const_cast<char*>(mStringVal));
    }

    // Zero the entire value (regardless of type) via mStringVal.
    mStringVal = nullptr;
  }

  void Replace(bool aHasValue, PrefType aOldType, PrefType aNewType,
               PrefValue aNewValue) {
    if (aHasValue) {
      Clear(aOldType);
    }
    Init(aNewType, aNewValue);
  }

  void ToDomPrefValue(PrefType aType, dom::PrefValue* aDomValue) {
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

  PrefType FromDomPrefValue(const dom::PrefValue& aDomValue) {
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

  void SerializeAndAppend(PrefType aType, nsCString& aStr) {
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

  static char* Deserialize(PrefType aType, char* aStr,
                           Maybe<dom::PrefValue>* aDomValue) {
    char* p = aStr;

    switch (aType) {
      case PrefType::Bool:
        if (*p == 'T') {
          *aDomValue = Some(true);
        } else if (*p == 'F') {
          *aDomValue = Some(false);
        } else {
          *aDomValue = Some(false);
          NS_ERROR("bad bool pref value");
        }
        p++;
        return p;

      case PrefType::Int: {
        *aDomValue = Some(int32_t(strtol(p, &p, 10)));
        return p;
      }

      case PrefType::String: {
        nsCString str;
        p = DeserializeString(p, str);
        *aDomValue = Some(str);
        return p;
      }

      default:
        MOZ_CRASH();
    }
  }
};

template <>
bool PrefValue::Get() const {
  return mBoolVal;
}

template <>
int32_t PrefValue::Get() const {
  return mIntVal;
}

template <>
nsDependentCString PrefValue::Get() const {
  return nsDependentCString(mStringVal);
}

#ifdef DEBUG
const char* PrefTypeToString(PrefType aType) {
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
static void StrEscape(const char* aOriginal, nsCString& aResult) {
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
struct PrefsSizes {
  PrefsSizes()
      : mHashTable(0),
        mPrefValues(0),
        mStringValues(0),
        mRootBranches(0),
        mPrefNameArena(0),
        mCallbacksObjects(0),
        mCallbacksDomains(0),
        mMisc(0) {}

  size_t mHashTable;
  size_t mPrefValues;
  size_t mStringValues;
  size_t mRootBranches;
  size_t mPrefNameArena;
  size_t mCallbacksObjects;
  size_t mCallbacksDomains;
  size_t mMisc;
};
}  // namespace mozilla

static StaticRefPtr<SharedPrefMap> gSharedMap;

static ArenaAllocator<4096, 1> gPrefNameArena;

class PrefWrapper;

class Pref {
 public:
  explicit Pref(const char* aName)
      : mName(ArenaStrdup(aName, gPrefNameArena)),
        mType(static_cast<uint32_t>(PrefType::None)),
        mIsSticky(false),
        mIsLocked(false),
        mDefaultChanged(false),
        mHasDefaultValue(false),
        mHasUserValue(false),
        mIsSkippedByIteration(false),
        mDefaultValue(),
        mUserValue() {}

  ~Pref() {
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
  void SetIsLocked(bool aValue) { mIsLocked = aValue; }
  bool IsSkippedByIteration() const { return mIsSkippedByIteration; }
  void SetIsSkippedByIteration(bool aValue) { mIsSkippedByIteration = aValue; }

  bool DefaultChanged() const { return mDefaultChanged; }

  bool IsSticky() const { return mIsSticky; }

  bool HasDefaultValue() const { return mHasDefaultValue; }
  bool HasUserValue() const { return mHasUserValue; }

  template <typename T>
  void AddToMap(SharedPrefMapBuilder& aMap) {
    aMap.Add(Name(),
             {HasDefaultValue(), HasUserValue(), IsSticky(), IsLocked(),
              DefaultChanged(), IsSkippedByIteration()},
             HasDefaultValue() ? mDefaultValue.Get<T>() : T(),
             HasUserValue() ? mUserValue.Get<T>() : T());
  }

  void AddToMap(SharedPrefMapBuilder& aMap) {
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

  // Other operations.

  bool GetBoolValue(PrefValueKind aKind = PrefValueKind::User) const {
    MOZ_ASSERT(IsTypeBool());
    MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                               : HasUserValue());

    return aKind == PrefValueKind::Default ? mDefaultValue.mBoolVal
                                           : mUserValue.mBoolVal;
  }

  int32_t GetIntValue(PrefValueKind aKind = PrefValueKind::User) const {
    MOZ_ASSERT(IsTypeInt());
    MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                               : HasUserValue());

    return aKind == PrefValueKind::Default ? mDefaultValue.mIntVal
                                           : mUserValue.mIntVal;
  }

  const char* GetBareStringValue(
      PrefValueKind aKind = PrefValueKind::User) const {
    MOZ_ASSERT(IsTypeString());
    MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                               : HasUserValue());

    return aKind == PrefValueKind::Default ? mDefaultValue.mStringVal
                                           : mUserValue.mStringVal;
  }

  nsDependentCString GetStringValue(
      PrefValueKind aKind = PrefValueKind::User) const {
    return nsDependentCString(GetBareStringValue(aKind));
  }

  void ToDomPref(dom::Pref* aDomPref) {
    MOZ_ASSERT(XRE_IsParentProcess());

    aDomPref->name() = mName;

    aDomPref->isLocked() = mIsLocked;

    if (mHasDefaultValue) {
      aDomPref->defaultValue() = Some(dom::PrefValue());
      mDefaultValue.ToDomPrefValue(Type(), &aDomPref->defaultValue().ref());
    } else {
      aDomPref->defaultValue() = Nothing();
    }

    if (mHasUserValue) {
      aDomPref->userValue() = Some(dom::PrefValue());
      mUserValue.ToDomPrefValue(Type(), &aDomPref->userValue().ref());
    } else {
      aDomPref->userValue() = Nothing();
    }

    MOZ_ASSERT(aDomPref->defaultValue().isNothing() ||
               aDomPref->userValue().isNothing() ||
               (aDomPref->defaultValue().ref().type() ==
                aDomPref->userValue().ref().type()));
  }

  void FromDomPref(const dom::Pref& aDomPref, bool* aValueChanged) {
    MOZ_ASSERT(!XRE_IsParentProcess());
    MOZ_ASSERT(strcmp(mName, aDomPref.name().get()) == 0);

    mIsLocked = aDomPref.isLocked();

    const Maybe<dom::PrefValue>& defaultValue = aDomPref.defaultValue();
    bool defaultValueChanged = false;
    if (defaultValue.isSome()) {
      PrefValue value;
      PrefType type = value.FromDomPrefValue(defaultValue.ref());
      if (!ValueMatches(PrefValueKind::Default, type, value)) {
        // Type() is PrefType::None if it's a newly added pref. This is ok.
        mDefaultValue.Replace(mHasDefaultValue, Type(), type, value);
        SetType(type);
        mHasDefaultValue = true;
        defaultValueChanged = true;
      }
    }
    // Note: we never clear a default value.

    const Maybe<dom::PrefValue>& userValue = aDomPref.userValue();
    bool userValueChanged = false;
    if (userValue.isSome()) {
      PrefValue value;
      PrefType type = value.FromDomPrefValue(userValue.ref());
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

    if (userValueChanged || (defaultValueChanged && !mHasUserValue)) {
      *aValueChanged = true;
    }
  }

  void FromWrapper(PrefWrapper& aWrapper);

  bool HasAdvisablySizedValues() {
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
  bool ValueMatches(PrefValueKind aKind, PrefType aType, PrefValue aValue) {
    return IsType(aType) &&
           (aKind == PrefValueKind::Default
                ? mHasDefaultValue && mDefaultValue.Equals(aType, aValue)
                : mHasUserValue && mUserValue.Equals(aType, aValue));
  }

 public:
  void ClearUserValue() {
    mUserValue.Clear(Type());
    mHasUserValue = false;
  }

  nsresult SetDefaultValue(PrefType aType, PrefValue aValue, bool aIsSticky,
                           bool aIsLocked, bool* aValueChanged) {
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
        if (mHasDefaultValue) {
          mDefaultChanged = true;
        }
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
    }
    return NS_OK;
  }

  nsresult SetUserValue(PrefType aType, PrefValue aValue, bool aFromInit,
                        bool* aValueChanged) {
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
      SetType(aType);  // needed because we may have changed the type
      mHasUserValue = true;
      if (!IsLocked()) {
        *aValueChanged = true;
      }
    }
    return NS_OK;
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

  void SerializeAndAppend(nsCString& aStr) {
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

  static char* Deserialize(char* aStr, dom::Pref* aDomPref) {
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
    p++;  // move past the type char

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
    p++;  // move past the isLocked char

    MOZ_ASSERT(*p == ':');
    p++;  // move past the ':'

    // The pref name.
    nsCString name;
    p = DeserializeString(p, name);

    MOZ_ASSERT(*p == ':');
    p++;  // move past the ':' preceding the default value

    Maybe<dom::PrefValue> maybeDefaultValue;
    if (*p != ':') {
      dom::PrefValue defaultValue;
      p = PrefValue::Deserialize(type, p, &maybeDefaultValue);
    }

    MOZ_ASSERT(*p == ':');
    p++;  // move past the ':' between the default and user values

    Maybe<dom::PrefValue> maybeUserValue;
    if (*p != '\n') {
      dom::PrefValue userValue;
      p = PrefValue::Deserialize(type, p, &maybeUserValue);
    }

    MOZ_ASSERT(*p == '\n');
    p++;  // move past the '\n' following the user value

    *aDomPref = dom::Pref(name, isLocked, maybeDefaultValue, maybeUserValue);

    return p;
  }

  void AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf, PrefsSizes& aSizes) {
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
  const char* mName;  // allocated in gPrefNameArena

  uint32_t mType : 2;
  uint32_t mIsSticky : 1;
  uint32_t mIsLocked : 1;
  uint32_t mDefaultChanged : 1;
  uint32_t mHasDefaultValue : 1;
  uint32_t mHasUserValue : 1;
  uint32_t mIsSkippedByIteration : 1;

  PrefValue mDefaultValue;
  PrefValue mUserValue;
};

struct PrefHasher {
  using Key = UniquePtr<Pref>;
  using Lookup = const char*;

  static HashNumber hash(const Lookup& aLookup) { return HashString(aLookup); }

  static bool match(const Key& aKey, const Lookup& aLookup) {
    if (!aLookup || !aKey->Name()) {
      return false;
    }

    return strcmp(aLookup, aKey->Name()) == 0;
  }
};

using PrefWrapperBase = Variant<Pref*, SharedPrefMap::Pref>;
class MOZ_STACK_CLASS PrefWrapper : public PrefWrapperBase {
  using SharedPref = const SharedPrefMap::Pref;

 public:
  MOZ_IMPLICIT PrefWrapper(Pref* aPref) : PrefWrapperBase(AsVariant(aPref)) {}

  MOZ_IMPLICIT PrefWrapper(const SharedPrefMap::Pref& aPref)
      : PrefWrapperBase(AsVariant(aPref)) {}

  // Types.

  bool IsType(PrefType aType) const { return Type() == aType; }
  bool IsTypeNone() const { return IsType(PrefType::None); }
  bool IsTypeString() const { return IsType(PrefType::String); }
  bool IsTypeInt() const { return IsType(PrefType::Int); }
  bool IsTypeBool() const { return IsType(PrefType::Bool); }

#define FORWARD(retType, method)                                        \
  retType method() const {                                              \
    struct Matcher {                                                    \
      retType operator()(const Pref* aPref) { return aPref->method(); } \
      retType operator()(SharedPref& aPref) { return aPref.method(); }  \
    };                                                                  \
    return match(Matcher());                                            \
  }

  FORWARD(bool, DefaultChanged)
  FORWARD(bool, IsLocked)
  FORWARD(bool, IsSticky)
  FORWARD(bool, HasDefaultValue)
  FORWARD(bool, HasUserValue)
  FORWARD(const char*, Name)
  FORWARD(nsCString, NameString)
  FORWARD(PrefType, Type)
#undef FORWARD

#define FORWARD(retType, method)                                             \
  retType method(PrefValueKind aKind = PrefValueKind::User) const {          \
    struct Matcher {                                                         \
      PrefValueKind mKind;                                                   \
                                                                             \
      retType operator()(const Pref* aPref) { return aPref->method(mKind); } \
      retType operator()(SharedPref& aPref) { return aPref.method(mKind); }  \
    };                                                                       \
    return match(Matcher{aKind});                                            \
  }

  FORWARD(bool, GetBoolValue)
  FORWARD(int32_t, GetIntValue)
  FORWARD(nsCString, GetStringValue)
  FORWARD(const char*, GetBareStringValue)
#undef FORWARD

  PrefValue GetValue(PrefValueKind aKind = PrefValueKind::User) const {
    switch (Type()) {
      case PrefType::Bool:
        return PrefValue{GetBoolValue(aKind)};
      case PrefType::Int:
        return PrefValue{GetIntValue(aKind)};
      case PrefType::String:
        return PrefValue{GetBareStringValue(aKind)};
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected pref type");
        return PrefValue{};
    }
  }

  Result<PrefValueKind, nsresult> WantValueKind(PrefType aType,
                                                PrefValueKind aKind) const {
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

  nsresult GetValue(PrefValueKind aKind, bool* aResult) const {
    PrefValueKind kind;
    MOZ_TRY_VAR(kind, WantValueKind(PrefType::Bool, aKind));

    *aResult = GetBoolValue(kind);
    return NS_OK;
  }

  nsresult GetValue(PrefValueKind aKind, int32_t* aResult) const {
    PrefValueKind kind;
    MOZ_TRY_VAR(kind, WantValueKind(PrefType::Int, aKind));

    *aResult = GetIntValue(kind);
    return NS_OK;
  }

  nsresult GetValue(PrefValueKind aKind, uint32_t* aResult) const {
    return GetValue(aKind, reinterpret_cast<int32_t*>(aResult));
  }

  nsresult GetValue(PrefValueKind aKind, float* aResult) const {
    nsAutoCString result;
    nsresult rv = GetValue(aKind, result);
    if (NS_SUCCEEDED(rv)) {
      // ToFloat() does a locale-independent conversion.
      *aResult = result.ToFloat(&rv);
    }
    return rv;
  }

  nsresult GetValue(PrefValueKind aKind, nsACString& aResult) const {
    PrefValueKind kind;
    MOZ_TRY_VAR(kind, WantValueKind(PrefType::String, aKind));

    aResult = GetStringValue(kind);
    return NS_OK;
  }

  // Returns false if this pref doesn't have a user value worth saving.
  bool UserValueToStringForSaving(nsCString& aStr) {
    // Should we save the user value, if present? Only if it does not match the
    // default value, or it is sticky.
    if (HasUserValue() &&
        (!ValueMatches(PrefValueKind::Default, Type(), GetValue()) ||
         IsSticky())) {
      if (IsTypeString()) {
        StrEscape(GetStringValue().get(), aStr);

      } else if (IsTypeInt()) {
        aStr.AppendInt(GetIntValue());

      } else if (IsTypeBool()) {
        aStr = GetBoolValue() ? "true" : "false";
      }
      return true;
    }

    // Do not save default prefs that haven't changed.
    return false;
  }

  bool Matches(PrefType aType, PrefValueKind aKind, PrefValue& aValue,
               bool aIsSticky, bool aIsLocked) const {
    return (ValueMatches(aKind, aType, aValue) && aIsSticky == IsSticky() &&
            aIsLocked == IsLocked());
  }

  bool ValueMatches(PrefValueKind aKind, PrefType aType,
                    const PrefValue& aValue) const {
    if (!IsType(aType)) {
      return false;
    }
    if (!(aKind == PrefValueKind::Default ? HasDefaultValue()
                                          : HasUserValue())) {
      return false;
    }
    switch (aType) {
      case PrefType::Bool:
        return GetBoolValue(aKind) == aValue.mBoolVal;
      case PrefType::Int:
        return GetIntValue(aKind) == aValue.mIntVal;
      case PrefType::String:
        return strcmp(GetBareStringValue(aKind), aValue.mStringVal) == 0;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected preference type");
        return false;
    }
  }
};

void Pref::FromWrapper(PrefWrapper& aWrapper) {
  MOZ_ASSERT(aWrapper.is<SharedPrefMap::Pref>());
  auto pref = aWrapper.as<SharedPrefMap::Pref>();

  MOZ_ASSERT(IsTypeNone());
  MOZ_ASSERT(strcmp(mName, pref.Name()) == 0);

  mType = uint32_t(pref.Type());

  mIsLocked = pref.IsLocked();
  mIsSticky = pref.IsSticky();

  mHasDefaultValue = pref.HasDefaultValue();
  mHasUserValue = pref.HasUserValue();

  if (mHasDefaultValue) {
    mDefaultValue.Init(Type(), aWrapper.GetValue(PrefValueKind::Default));
  }
  if (mHasUserValue) {
    mUserValue.Init(Type(), aWrapper.GetValue(PrefValueKind::User));
  }
}

class CallbackNode {
 public:
  CallbackNode(const nsACString& aDomain, PrefChangedFunc aFunc, void* aData,
               Preferences::MatchKind aMatchKind)
      : mDomain(AsVariant(nsCString(aDomain))),
        mFunc(aFunc),
        mData(aData),
        mNextAndMatchKind(aMatchKind) {}

  CallbackNode(const char** aDomains, PrefChangedFunc aFunc, void* aData,
               Preferences::MatchKind aMatchKind)
      : mDomain(AsVariant(aDomains)),
        mFunc(aFunc),
        mData(aData),
        mNextAndMatchKind(aMatchKind) {}

  // mDomain is a UniquePtr<>, so any uses of Domain() should only be temporary
  // borrows.
  const Variant<nsCString, const char**>& Domain() const { return mDomain; }

  PrefChangedFunc Func() const { return mFunc; }
  void ClearFunc() { mFunc = nullptr; }

  void* Data() const { return mData; }

  Preferences::MatchKind MatchKind() const {
    return static_cast<Preferences::MatchKind>(mNextAndMatchKind &
                                               kMatchKindMask);
  }

  bool DomainIs(const nsACString& aDomain) const {
    return mDomain.is<nsCString>() && mDomain.as<nsCString>() == aDomain;
  }

  bool DomainIs(const char** aPrefs) const {
    return mDomain == AsVariant(aPrefs);
  }

  bool Matches(const nsACString& aPrefName) const {
    auto match = [&](const nsACString& aStr) {
      return MatchKind() == Preferences::ExactMatch
                 ? aPrefName == aStr
                 : StringBeginsWith(aPrefName, aStr);
    };

    if (mDomain.is<nsCString>()) {
      return match(mDomain.as<nsCString>());
    }
    for (const char** ptr = mDomain.as<const char**>(); *ptr; ptr++) {
      if (match(nsDependentCString(*ptr))) {
        return true;
      }
    }
    return false;
  }

  CallbackNode* Next() const {
    return reinterpret_cast<CallbackNode*>(mNextAndMatchKind & kNextMask);
  }

  void SetNext(CallbackNode* aNext) {
    uintptr_t matchKind = mNextAndMatchKind & kMatchKindMask;
    mNextAndMatchKind = reinterpret_cast<uintptr_t>(aNext);
    MOZ_ASSERT((mNextAndMatchKind & kMatchKindMask) == 0);
    mNextAndMatchKind |= matchKind;
  }

  void AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf, PrefsSizes& aSizes) {
    aSizes.mCallbacksObjects += aMallocSizeOf(this);
    if (mDomain.is<nsCString>()) {
      aSizes.mCallbacksDomains +=
          mDomain.as<nsCString>().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
  }

 private:
  static const uintptr_t kMatchKindMask = uintptr_t(0x1);
  static const uintptr_t kNextMask = ~kMatchKindMask;

  Variant<nsCString, const char**> mDomain;

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

using PrefsHashTable = HashSet<UniquePtr<Pref>, PrefHasher>;

static PrefsHashTable* gHashTable;

#ifdef DEBUG
// This defines the type used to store our `once` mirrors checker. We can't use
// HashMap for now due to alignment restrictions when dealing with
// std::function<void()> (see bug 1557617).
typedef std::function<void()> AntiFootgunCallback;
struct CompareStr {
  bool operator()(char const* a, char const* b) const {
    return std::strcmp(a, b) < 0;
  }
};
typedef std::map<const char*, AntiFootgunCallback, CompareStr> AntiFootgunMap;
static AntiFootgunMap* gOnceStaticPrefsAntiFootgun;
#endif

// The callback list contains all the priority callbacks followed by the
// non-priority callbacks. gLastPriorityNode records where the first part ends.
static CallbackNode* gFirstCallback = nullptr;
static CallbackNode* gLastPriorityNode = nullptr;

#ifdef DEBUG
#  define ACCESS_COUNTS
#endif

#ifdef ACCESS_COUNTS
using AccessCountsHashTable = nsDataHashtable<nsCStringHashKey, uint32_t>;
static AccessCountsHashTable* gAccessCounts;

static void AddAccessCount(const nsACString& aPrefName) {
  // FIXME: Servo reads preferences from background threads in unsafe ways (bug
  // 1474789), and triggers assertions here if we try to add usage count entries
  // from background threads.
  if (NS_IsMainThread()) {
    uint32_t& count = gAccessCounts->GetOrInsert(aPrefName);
    count++;
  }
}

static void AddAccessCount(const char* aPrefName) {
  AddAccessCount(nsDependentCString(aPrefName));
}
#else
static void MOZ_MAYBE_UNUSED AddAccessCount(const nsACString& aPrefName) {}

static void AddAccessCount(const char* aPrefName) {}
#endif

// These are only used during the call to NotifyCallbacks().
static bool gCallbacksInProgress = false;
static bool gShouldCleanupDeadNodes = false;

class PrefsHashIter {
  using Iterator = decltype(gHashTable->modIter());
  using ElemType = Pref*;

  Iterator mIter;

 public:
  explicit PrefsHashIter(PrefsHashTable* aTable) : mIter(aTable->modIter()) {}

  class Elem {
    friend class PrefsHashIter;

    PrefsHashIter& mParent;
    bool mDone;

    Elem(PrefsHashIter& aIter, bool aDone) : mParent(aIter), mDone(aDone) {}

    Iterator& Iter() { return mParent.mIter; }

   public:
    Elem& operator*() { return *this; }

    ElemType get() {
      if (mDone) {
        return nullptr;
      }
      return Iter().get().get();
    }
    ElemType get() const { return const_cast<Elem*>(this)->get(); }

    ElemType operator->() { return get(); }
    ElemType operator->() const { return get(); }

    operator ElemType() { return get(); }

    void Remove() { Iter().remove(); }

    Elem& operator++() {
      MOZ_ASSERT(!mDone);
      Iter().next();
      mDone = Iter().done();
      return *this;
    }

    bool operator!=(Elem& other) {
      return mDone != other.mDone || this->get() != other.get();
    }
  };

  Elem begin() { return Elem(*this, mIter.done()); }

  Elem end() { return Elem(*this, true); }
};

class PrefsIter {
  using Iterator = decltype(gHashTable->iter());
  using ElemType = PrefWrapper;

  using HashElem = PrefsHashIter::Elem;
  using SharedElem = SharedPrefMap::Pref;

  using ElemTypeVariant = Variant<HashElem, SharedElem>;

  SharedPrefMap* mSharedMap;
  PrefsHashTable* mHashTable;
  PrefsHashIter mIter;

  ElemTypeVariant mPos;
  ElemTypeVariant mEnd;

  Maybe<PrefWrapper> mEntry;

 public:
  PrefsIter(PrefsHashTable* aHashTable, SharedPrefMap* aSharedMap)
      : mSharedMap(aSharedMap),
        mHashTable(aHashTable),
        mIter(aHashTable),
        mPos(AsVariant(mIter.begin())),
        mEnd(AsVariant(mIter.end())) {
    if (Done()) {
      NextIterator();
    }
  }

 private:
#define MATCH(type, ...)                                                \
  do {                                                                  \
    struct Matcher {                                                    \
      PrefsIter& mIter;                                                 \
      type operator()(HashElem& pos) {                                  \
        HashElem& end MOZ_MAYBE_UNUSED = mIter.mEnd.as<HashElem>();     \
        __VA_ARGS__;                                                    \
      }                                                                 \
      type operator()(SharedElem& pos) {                                \
        SharedElem& end MOZ_MAYBE_UNUSED = mIter.mEnd.as<SharedElem>(); \
        __VA_ARGS__;                                                    \
      }                                                                 \
    };                                                                  \
    return mPos.match(Matcher{*this});                                  \
  } while (0);

  bool Done() { MATCH(bool, return pos == end); }

  PrefWrapper MakeEntry() { MATCH(PrefWrapper, return PrefWrapper(pos)); }

  void NextEntry() {
    mEntry.reset();
    MATCH(void, ++pos);
  }
#undef MATCH

  bool Next() {
    NextEntry();
    return !Done() || NextIterator();
  }

  bool NextIterator() {
    if (mPos.is<HashElem>() && mSharedMap) {
      mPos = AsVariant(mSharedMap->begin());
      mEnd = AsVariant(mSharedMap->end());
      return !Done();
    }
    return false;
  }

  bool IteratingBase() { return mPos.is<SharedElem>(); }

  PrefWrapper& Entry() {
    MOZ_ASSERT(!Done());

    if (!mEntry.isSome()) {
      mEntry.emplace(MakeEntry());
    }
    return mEntry.ref();
  }

 public:
  class Elem {
    friend class PrefsIter;

    PrefsIter& mParent;
    bool mDone;

    Elem(PrefsIter& aIter, bool aDone) : mParent(aIter), mDone(aDone) {
      SkipDuplicates();
    }

    void Next() { mDone = !mParent.Next(); }

    void SkipDuplicates() {
      while (!mDone &&
             (mParent.IteratingBase() ? mParent.mHashTable->has(ref().Name())
                                      : ref().IsTypeNone())) {
        Next();
      }
    }

   public:
    Elem& operator*() { return *this; }

    ElemType& ref() { return mParent.Entry(); }
    const ElemType& ref() const { return const_cast<Elem*>(this)->ref(); }

    ElemType* operator->() { return &ref(); }
    const ElemType* operator->() const { return &ref(); }

    operator ElemType() { return ref(); }

    Elem& operator++() {
      MOZ_ASSERT(!mDone);
      Next();
      SkipDuplicates();
      return *this;
    }

    bool operator!=(Elem& other) {
      if (mDone != other.mDone) {
        return true;
      }
      if (mDone) {
        return false;
      }
      return &this->ref() != &other.ref();
    }
  };

  Elem begin() { return {*this, Done()}; }

  Elem end() { return {*this, true}; }
};

static Pref* pref_HashTableLookup(const char* aPrefName);

static void NotifyCallbacks(const char* aPrefName,
                            const PrefWrapper* aPref = nullptr);

static void NotifyCallbacks(const char* aPrefName, const PrefWrapper& aPref) {
  NotifyCallbacks(aPrefName, &aPref);
}

// The approximate number of preferences in the dynamic hashtable for the parent
// and content processes, respectively. These numbers are used to determine the
// initial size of the dynamic preference hashtables, and should be chosen to
// avoid rehashing during normal usage. The actual number of preferences will,
// or course, change over time, but these numbers only need to be within a
// binary order of magnitude of the actual values to remain effective.
//
// The number for the parent process should reflect the total number of
// preferences in the database, since the parent process needs to initially
// build a dynamic hashtable of the entire preference database. The number for
// the child process should reflect the number of preferences which are likely
// to change after the startup of the first content process, since content
// processes only store changed preferences on top of a snapshot of the database
// created at startup.
//
// Note: The capacity of a hashtable doubles when its length reaches an exact
// power of two. A table with an initial length of 64 is twice as large as one
// with an initial length of 63. This is important in content processes, where
// lookup speed is less critical and we pay the price of the additional overhead
// for each content process. So the initial content length should generally be
// *under* the next power-of-two larger than its expected length.
constexpr size_t kHashTableInitialLengthParent = 3000;
constexpr size_t kHashTableInitialLengthContent = 64;

static PrefSaveData pref_savePrefs() {
  MOZ_ASSERT(NS_IsMainThread());

  PrefSaveData savedPrefs(gHashTable->count());

  for (auto& pref : PrefsIter(gHashTable, gSharedMap)) {
    nsAutoCString prefValueStr;
    if (!pref->UserValueToStringForSaving(prefValueStr)) {
      continue;
    }

    nsAutoCString prefNameStr;
    StrEscape(pref->Name(), prefNameStr);

    nsPrintfCString str("user_pref(%s, %s);", prefNameStr.get(),
                        prefValueStr.get());

    savedPrefs.AppendElement(str);
  }

  return savedPrefs;
}

#ifdef DEBUG

// Note that this never changes in the parent process, and is only read in
// content processes.
static bool gContentProcessPrefsAreInited = false;

#endif  // DEBUG

static Pref* pref_HashTableLookup(const char* aPrefName) {
  MOZ_ASSERT(NS_IsMainThread() || ServoStyleSet::IsInServoTraversal());

  MOZ_ASSERT_IF(!XRE_IsParentProcess(), gContentProcessPrefsAreInited);

  // We use readonlyThreadsafeLookup() because we often have concurrent lookups
  // from multiple Stylo threads. This is safe because those threads cannot
  // modify gHashTable, and the main thread is blocked while Stylo threads are
  // doing these lookups.
  auto p = gHashTable->readonlyThreadsafeLookup(aPrefName);
  return p ? p->get() : nullptr;
}

// While notifying preference callbacks, this holds the wrapper for the
// preference being notified, in order to optimize lookups.
//
// Note: Callbacks and lookups only happen on the main thread, so this is safe
// to use without locking.
static const PrefWrapper* gCallbackPref;

Maybe<PrefWrapper> pref_SharedLookup(const char* aPrefName) {
  MOZ_DIAGNOSTIC_ASSERT(gSharedMap, "gSharedMap must be initialized");
  if (Maybe<SharedPrefMap::Pref> pref = gSharedMap->Get(aPrefName)) {
    return Some(*pref);
  }
  return Nothing();
}

Maybe<PrefWrapper> pref_Lookup(const char* aPrefName,
                               bool aIncludeTypeNone = false) {
  MOZ_ASSERT(NS_IsMainThread() || ServoStyleSet::IsInServoTraversal());

  AddAccessCount(aPrefName);

  if (gCallbackPref && strcmp(aPrefName, gCallbackPref->Name()) == 0) {
    return Some(*gCallbackPref);
  }
  if (Pref* pref = pref_HashTableLookup(aPrefName)) {
    if (aIncludeTypeNone || !pref->IsTypeNone()) {
      return Some(pref);
    }
  } else if (gSharedMap) {
    return pref_SharedLookup(aPrefName);
  }

  return Nothing();
}

static Result<Pref*, nsresult> pref_LookupForModify(
    const char* aPrefName,
    const std::function<bool(const PrefWrapper&)>& aCheckFn) {
  Maybe<PrefWrapper> wrapper =
      pref_Lookup(aPrefName, /* includeTypeNone */ true);
  if (wrapper.isNothing()) {
    return Err(NS_ERROR_INVALID_ARG);
  }
  if (!aCheckFn(*wrapper)) {
    return nullptr;
  }
  if (wrapper->is<Pref*>()) {
    return wrapper->as<Pref*>();
  }

  Pref* pref = new Pref(aPrefName);
  if (!gHashTable->putNew(aPrefName, pref)) {
    delete pref;
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }
  pref->FromWrapper(*wrapper);
  return pref;
}

static nsresult pref_SetPref(const char* aPrefName, PrefType aType,
                             PrefValueKind aKind, PrefValue aValue,
                             bool aIsSticky, bool aIsLocked, bool aFromInit) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gHashTable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  Pref* pref = nullptr;
  if (gSharedMap) {
    auto result =
        pref_LookupForModify(aPrefName, [&](const PrefWrapper& aWrapper) {
          return !aWrapper.Matches(aType, aKind, aValue, aIsSticky, aIsLocked);
        });
    if (result.isOk() && !(pref = result.unwrap())) {
      // No changes required.
      return NS_OK;
    }
  }

  if (!pref) {
    auto p = gHashTable->lookupForAdd(aPrefName);
    if (!p) {
      pref = new Pref(aPrefName);
      pref->SetType(aType);
      if (!gHashTable->add(p, pref)) {
        delete pref;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    } else {
      pref = p->get();
    }
  }

  bool valueChanged = false;
  nsresult rv;
  if (aKind == PrefValueKind::Default) {
    rv = pref->SetDefaultValue(aType, aValue, aIsSticky, aIsLocked,
                               &valueChanged);
  } else {
    MOZ_ASSERT(!aIsLocked);  // `locked` is disallowed in user pref files
    rv = pref->SetUserValue(aType, aValue, aFromInit, &valueChanged);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING(
        nsPrintfCString("Rejected attempt to change type of pref %s's %s value "
                        "from %s to %s",
                        aPrefName,
                        (aKind == PrefValueKind::Default) ? "default" : "user",
                        PrefTypeToString(pref->Type()), PrefTypeToString(aType))
            .get());

    return rv;
  }

  if (valueChanged) {
    if (aKind == PrefValueKind::User) {
      Preferences::HandleDirty();
    }
    NotifyCallbacks(aPrefName, PrefWrapper(pref));
  }

  return NS_OK;
}

// Removes |node| from callback list. Returns the node after the deleted one.
static CallbackNode* pref_RemoveCallbackNode(CallbackNode* aNode,
                                             CallbackNode* aPrevNode) {
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

static void NotifyCallbacks(const char* aPrefName, const PrefWrapper* aPref) {
  bool reentered = gCallbacksInProgress;

  gCallbackPref = aPref;
  auto cleanup = MakeScopeExit([]() { gCallbackPref = nullptr; });

  // Nodes must not be deleted while gCallbacksInProgress is true.
  // Nodes that need to be deleted are marked for deletion by nulling
  // out the |func| pointer. We release them at the end of this function
  // if we haven't reentered.
  gCallbacksInProgress = true;

  nsDependentCString prefName(aPrefName);

  for (CallbackNode* node = gFirstCallback; node; node = node->Next()) {
    if (node->Func()) {
      if (node->Matches(prefName)) {
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

#ifdef DEBUG
  if (XRE_IsParentProcess() &&
      !StaticPrefs::preferences_force_disable_check_once_policy() &&
      (StaticPrefs::preferences_check_once_policy() || xpc::IsInAutomation())) {
    // Check that we aren't modifying a `once`-mirrored pref using that pref
    // name. We have about 100 `once`-mirrored prefs. std::map performs a
    // search in O(log n), so this is fast enough.
    MOZ_ASSERT(gOnceStaticPrefsAntiFootgun);
    auto search = gOnceStaticPrefsAntiFootgun->find(aPrefName);
    if (search != gOnceStaticPrefsAntiFootgun->end()) {
      // Run the callback.
      (search->second)();
    }
  }
#endif
}

//===========================================================================
// Prefs parsing
//===========================================================================

struct TelemetryLoadData {
  uint32_t mFileLoadSize_B;
  uint32_t mFileLoadNumPrefs;
  uint32_t mFileLoadTime_us;
};

static nsDataHashtable<nsCStringHashKey, TelemetryLoadData>* gTelemetryLoadData;

extern "C" {

// Keep this in sync with PrefFn in prefs_parser/src/lib.rs.
typedef void (*PrefsParserPrefFn)(const char* aPrefName, PrefType aType,
                                  PrefValueKind aKind, PrefValue aValue,
                                  bool aIsSticky, bool aIsLocked);

// Keep this in sync with ErrorFn in prefs_parser/src/lib.rs.
//
// `aMsg` is just a borrow of the string, and must be copied if it is used
// outside the lifetime of the prefs_parser_parse() call.
typedef void (*PrefsParserErrorFn)(const char* aMsg);

// Keep this in sync with prefs_parser_parse() in prefs_parser/src/lib.rs.
bool prefs_parser_parse(const char* aPath, PrefValueKind aKind,
                        const char* aBuf, size_t aLen,
                        PrefsParserPrefFn aPrefFn, PrefsParserErrorFn aErrorFn);
}

class Parser {
 public:
  Parser() = default;
  ~Parser() = default;

  bool Parse(const nsCString& aName, PrefValueKind aKind, const char* aPath,
             const TimeStamp& aStartTime, const nsCString& aBuf) {
    MOZ_ASSERT(XRE_IsParentProcess());
    sNumPrefs = 0;
    bool ok = prefs_parser_parse(aPath, aKind, aBuf.get(), aBuf.Length(),
                                 HandlePref, HandleError);
    if (!ok) {
      return false;
    }

    uint32_t loadTime_us = (TimeStamp::Now() - aStartTime).ToMicroseconds();

    // Most prefs files are read before telemetry initializes, so we have to
    // save these measurements now and send them to telemetry later.
    TelemetryLoadData loadData = {uint32_t(aBuf.Length()), sNumPrefs,
                                  loadTime_us};
    gTelemetryLoadData->Put(aName, loadData);

    return true;
  }

 private:
  static void HandlePref(const char* aPrefName, PrefType aType,
                         PrefValueKind aKind, PrefValue aValue, bool aIsSticky,
                         bool aIsLocked) {
    MOZ_ASSERT(XRE_IsParentProcess());
    sNumPrefs++;
    pref_SetPref(aPrefName, aType, aKind, aValue, aIsSticky, aIsLocked,
                 /* fromInit */ true);
  }

  static void HandleError(const char* aMsg) {
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

static void TestParseErrorHandlePref(const char* aPrefName, PrefType aType,
                                     PrefValueKind aKind, PrefValue aValue,
                                     bool aIsSticky, bool aIsLocked) {}

static nsCString gTestParseErrorMsgs;

static void TestParseErrorHandleError(const char* aMsg) {
  gTestParseErrorMsgs.Append(aMsg);
  gTestParseErrorMsgs.Append('\n');
}

// Keep this in sync with the declaration in test/gtest/Parser.cpp.
void TestParseError(PrefValueKind aKind, const char* aText,
                    nsCString& aErrorMsg) {
  prefs_parser_parse("test", aKind, aText, strlen(aText),
                     TestParseErrorHandlePref, TestParseErrorHandleError);

  // Copy the error messages into the outparam, then clear them from
  // gTestParseErrorMsgs.
  aErrorMsg.Assign(gTestParseErrorMsgs);
  gTestParseErrorMsgs.Truncate();
}

void SendTelemetryLoadData() {
  for (auto iter = gTelemetryLoadData->Iter(); !iter.Done(); iter.Next()) {
    const nsCString& filename = PromiseFlatCString(iter.Key());
    const TelemetryLoadData& data = iter.Data();
    Telemetry::Accumulate(Telemetry::PREFERENCES_FILE_LOAD_SIZE_B, filename,
                          data.mFileLoadSize_B);
    Telemetry::Accumulate(Telemetry::PREFERENCES_FILE_LOAD_NUM_PREFS, filename,
                          data.mFileLoadNumPrefs);
    Telemetry::Accumulate(Telemetry::PREFERENCES_FILE_LOAD_TIME_US, filename,
                          data.mFileLoadTime_us);
  }

  gTelemetryLoadData->Clear();
}

//===========================================================================
// nsPrefBranch et al.
//===========================================================================

namespace mozilla {
class PreferenceServiceReporter;
}  // namespace mozilla

class PrefCallback : public PLDHashEntryHdr {
  friend class mozilla::PreferenceServiceReporter;

 public:
  typedef PrefCallback* KeyType;
  typedef const PrefCallback* KeyTypePointer;

  static const PrefCallback* KeyToPointer(PrefCallback* aKey) { return aKey; }

  static PLDHashNumber HashKey(const PrefCallback* aKey) {
    uint32_t hash = HashString(aKey->mDomain);
    return AddToHash(hash, aKey->mCanonical);
  }

 public:
  // Create a PrefCallback with a strong reference to its observer.
  PrefCallback(const nsACString& aDomain, nsIObserver* aObserver,
               nsPrefBranch* aBranch)
      : mDomain(aDomain),
        mBranch(aBranch),
        mWeakRef(nullptr),
        mStrongRef(aObserver) {
    MOZ_COUNT_CTOR(PrefCallback);
    nsCOMPtr<nsISupports> canonical = do_QueryInterface(aObserver);
    mCanonical = canonical;
  }

  // Create a PrefCallback with a weak reference to its observer.
  PrefCallback(const nsACString& aDomain, nsISupportsWeakReference* aObserver,
               nsPrefBranch* aBranch)
      : mDomain(aDomain),
        mBranch(aBranch),
        mWeakRef(do_GetWeakReference(aObserver)),
        mStrongRef(nullptr) {
    MOZ_COUNT_CTOR(PrefCallback);
    nsCOMPtr<nsISupports> canonical = do_QueryInterface(aObserver);
    mCanonical = canonical;
  }

  // This is explicitly not a copy constructor.
  explicit PrefCallback(const PrefCallback*& aCopy)
      : mDomain(aCopy->mDomain),
        mBranch(aCopy->mBranch),
        mWeakRef(aCopy->mWeakRef),
        mStrongRef(aCopy->mStrongRef),
        mCanonical(aCopy->mCanonical) {
    MOZ_COUNT_CTOR(PrefCallback);
  }

  PrefCallback(const PrefCallback&) = delete;
  PrefCallback(PrefCallback&&) = default;

  ~PrefCallback() { MOZ_COUNT_DTOR(PrefCallback); }

  bool KeyEquals(const PrefCallback* aKey) const {
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
  already_AddRefed<nsIObserver> GetObserver() const {
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
  bool IsExpired() const {
    if (!IsWeak()) return false;

    nsCOMPtr<nsIObserver> observer(do_QueryReferent(mWeakRef));
    return !observer;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t n = aMallocSizeOf(this);
    n += mDomain.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    // All the other fields are non-owning pointers, so we don't measure them.

    return n;
  }

  enum { ALLOW_MEMMOVE = true };

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

class nsPrefBranch final : public nsIPrefBranch,
                           public nsIObserver,
                           public nsSupportsWeakReference {
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
  typedef Variant<const char*, const nsCString> PrefNameBase;
  class PrefName : public PrefNameBase {
   public:
    explicit PrefName(const char* aName) : PrefNameBase(aName) {}
    explicit PrefName(const nsCString& aName) : PrefNameBase(aName) {}

    // Use default move constructors, disallow copy constructors.
    PrefName(PrefName&& aOther) = default;
    PrefName& operator=(PrefName&& aOther) = default;
    PrefName(const PrefName&) = delete;
    PrefName& operator=(const PrefName&) = delete;

    struct PtrMatcher {
      const char* operator()(const char* aVal) { return aVal; }
      const char* operator()(const nsCString& aVal) { return aVal.get(); }
    };

    struct CStringMatcher {
      // Note: This is a reference, not an instance. It's used to pass our outer
      // method argument through to our matcher methods.
      nsACString& mStr;

      void operator()(const char* aVal) { mStr.Assign(aVal); }
      void operator()(const nsCString& aVal) { mStr.Assign(aVal); }
    };

    struct LenMatcher {
      size_t operator()(const char* aVal) { return strlen(aVal); }
      size_t operator()(const nsCString& aVal) { return aVal.Length(); }
    };

    const char* get() const { return match(PtrMatcher{}); }

    void get(nsACString& aStr) const { match(CStringMatcher{aStr}); }

    size_t Length() const { return match(LenMatcher{}); }
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

  PrefName GetPrefName(const char* aPrefName) const {
    return GetPrefName(nsDependentCString(aPrefName));
  }

  PrefName GetPrefName(const nsACString& aPrefName) const;

  void FreeObserverList(void);

  const nsCString mPrefRoot;
  PrefValueKind mKind;

  bool mFreeingObserverList;
  nsClassHashtable<PrefCallback, PrefCallback> mObservers;
};

class nsPrefLocalizedString final : public nsIPrefLocalizedString {
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

//----------------------------------------------------------------------------
// nsPrefBranch
//----------------------------------------------------------------------------

nsPrefBranch::nsPrefBranch(const char* aPrefRoot, PrefValueKind aKind)
    : mPrefRoot(aPrefRoot),
      mKind(aKind),
      mFreeingObserverList(false),
      mObservers() {
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    ++mRefCnt;  // must be > 0 when we call this, or we'll get deleted!

    // Add weakly so we don't have to clean up at shutdown.
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    --mRefCnt;
  }
}

nsPrefBranch::~nsPrefBranch() { FreeObserverList(); }

NS_IMPL_ISUPPORTS(nsPrefBranch, nsIPrefBranch, nsIObserver,
                  nsISupportsWeakReference)

NS_IMETHODIMP
nsPrefBranch::GetRoot(nsACString& aRoot) {
  aRoot = mPrefRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetPrefType(const char* aPrefName, int32_t* aRetVal) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& prefName = GetPrefName(aPrefName);
  *aRetVal = Preferences::GetType(prefName.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetBoolPrefWithDefault(const char* aPrefName, bool aDefaultValue,
                                     uint8_t aArgc, bool* aRetVal) {
  nsresult rv = GetBoolPref(aPrefName, aRetVal);
  if (NS_FAILED(rv) && aArgc == 1) {
    *aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetBoolPref(const char* aPrefName, bool* aRetVal) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::GetBool(pref.get(), aRetVal, mKind);
}

NS_IMETHODIMP
nsPrefBranch::SetBoolPref(const char* aPrefName, bool aValue) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::SetBool(pref.get(), aValue, mKind);
}

NS_IMETHODIMP
nsPrefBranch::GetFloatPrefWithDefault(const char* aPrefName,
                                      float aDefaultValue, uint8_t aArgc,
                                      float* aRetVal) {
  nsresult rv = GetFloatPref(aPrefName, aRetVal);

  if (NS_FAILED(rv) && aArgc == 1) {
    *aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetFloatPref(const char* aPrefName, float* aRetVal) {
  NS_ENSURE_ARG(aPrefName);

  nsAutoCString stringVal;
  nsresult rv = GetCharPref(aPrefName, stringVal);
  if (NS_SUCCEEDED(rv)) {
    // ToFloat() does a locale-independent conversion.
    *aRetVal = stringVal.ToFloat(&rv);
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetCharPrefWithDefault(const char* aPrefName,
                                     const nsACString& aDefaultValue,
                                     uint8_t aArgc, nsACString& aRetVal) {
  nsresult rv = GetCharPref(aPrefName, aRetVal);

  if (NS_FAILED(rv) && aArgc == 1) {
    aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetCharPref(const char* aPrefName, nsACString& aRetVal) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::GetCString(pref.get(), aRetVal, mKind);
}

NS_IMETHODIMP
nsPrefBranch::SetCharPref(const char* aPrefName, const nsACString& aValue) {
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return SetCharPrefNoLengthCheck(aPrefName, aValue);
}

nsresult nsPrefBranch::SetCharPrefNoLengthCheck(const char* aPrefName,
                                                const nsACString& aValue) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::SetCString(pref.get(), aValue, mKind);
}

NS_IMETHODIMP
nsPrefBranch::GetStringPref(const char* aPrefName,
                            const nsACString& aDefaultValue, uint8_t aArgc,
                            nsACString& aRetVal) {
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
nsPrefBranch::SetStringPref(const char* aPrefName, const nsACString& aValue) {
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return SetCharPrefNoLengthCheck(aPrefName, aValue);
}

NS_IMETHODIMP
nsPrefBranch::GetIntPrefWithDefault(const char* aPrefName,
                                    int32_t aDefaultValue, uint8_t aArgc,
                                    int32_t* aRetVal) {
  nsresult rv = GetIntPref(aPrefName, aRetVal);

  if (NS_FAILED(rv) && aArgc == 1) {
    *aRetVal = aDefaultValue;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetIntPref(const char* aPrefName, int32_t* aRetVal) {
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::GetInt(pref.get(), aRetVal, mKind);
}

NS_IMETHODIMP
nsPrefBranch::SetIntPref(const char* aPrefName, int32_t aValue) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::SetInt(pref.get(), aValue, mKind);
}

NS_IMETHODIMP
nsPrefBranch::GetComplexValue(const char* aPrefName, const nsIID& aType,
                              void** aRetVal) {
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

    rv = directoryService->Get(key.get(), NS_GET_IID(nsIFile),
                               getter_AddRefs(fromFile));
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

    nsCOMPtr<nsIRelativeFilePref> relativePref = new nsRelativeFilePref();
    Unused << relativePref->SetFile(theFile);
    Unused << relativePref->SetRelativeToKey(key);

    relativePref.forget(reinterpret_cast<nsIRelativeFilePref**>(aRetVal));
    return NS_OK;
  }

  NS_WARNING("nsPrefBranch::GetComplexValue - Unsupported interface type");
  return NS_NOINTERFACE;
}

nsresult nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName,
                                                 const nsAString& aValue) {
  return CheckSanityOfStringLength(aPrefName, aValue.Length());
}

nsresult nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName,
                                                 const nsACString& aValue) {
  return CheckSanityOfStringLength(aPrefName, aValue.Length());
}

nsresult nsPrefBranch::CheckSanityOfStringLength(const char* aPrefName,
                                                 const uint32_t aLength) {
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
      aLength, GetPrefName(aPrefName).get()));

  rv = console->LogStringMessage(NS_ConvertUTF8toUTF16(message).get());
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::SetComplexValue(const char* aPrefName, const nsIID& aType,
                              nsISupports* aValue) {
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

    rv = directoryService->Get(relativeToKey.get(), NS_GET_IID(nsIFile),
                               getter_AddRefs(relativeToFile));
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
nsPrefBranch::ClearUserPref(const char* aPrefName) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::ClearUser(pref.get());
}

NS_IMETHODIMP
nsPrefBranch::PrefHasUserValue(const char* aPrefName, bool* aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  *aRetVal = Preferences::HasUserValue(pref.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::LockPref(const char* aPrefName) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::Lock(pref.get());
}

NS_IMETHODIMP
nsPrefBranch::PrefIsLocked(const char* aPrefName, bool* aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  *aRetVal = Preferences::IsLocked(pref.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::UnlockPref(const char* aPrefName) {
  NS_ENSURE_ARG(aPrefName);

  const PrefName& pref = GetPrefName(aPrefName);
  return Preferences::Unlock(pref.get());
}

NS_IMETHODIMP
nsPrefBranch::ResetBranch(const char* aStartingAt) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefBranch::DeleteBranch(const char* aStartingAt) {
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

  for (auto iter = gHashTable->modIter(); !iter.done(); iter.next()) {
    // The first disjunct matches branches: e.g. a branch name "foo.bar."
    // matches a name "foo.bar.baz" (but it won't match "foo.barrel.baz").
    // The second disjunct matches leaf nodes: e.g. a branch name "foo.bar."
    // matches a name "foo.bar" (by ignoring the trailing '.').
    nsDependentCString name(iter.get()->Name());
    if (StringBeginsWith(name, branchName) || name.Equals(branchNameNoDot)) {
      iter.remove();
      // The saved callback pref may be invalid now.
      gCallbackPref = nullptr;
    }
  }

  Preferences::HandleDirty();
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetChildList(const char* aStartingAt,
                           nsTArray<nsCString>& aChildArray) {
  NS_ENSURE_ARG(aStartingAt);

  MOZ_ASSERT(NS_IsMainThread());

  // This will contain a list of all the pref name strings. Allocated on the
  // stack for speed.
  AutoTArray<nsCString, 32> prefArray;

  const PrefName& parent = GetPrefName(aStartingAt);
  size_t parentLen = parent.Length();
  for (auto& pref : PrefsIter(gHashTable, gSharedMap)) {
    if (strncmp(pref->Name(), parent.get(), parentLen) == 0) {
      prefArray.AppendElement(pref->NameString());
    }
  }

  // Now that we've built up the list, run the callback on all the matching
  // elements.
  aChildArray.SetCapacity(prefArray.Length());
  for (auto& element : prefArray) {
    // we need to lop off mPrefRoot in case the user is planning to pass this
    // back to us because if they do we are going to add mPrefRoot again.
    aChildArray.AppendElement(Substring(element, mPrefRoot.Length()));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::AddObserverImpl(const nsACString& aDomain, nsIObserver* aObserver,
                              bool aHoldWeak) {
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
  Preferences::RegisterCallback(NotifyObserver, prefName, pCallback,
                                Preferences::PrefixMatch,
                                /* isPriority */ false);

  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::RemoveObserverImpl(const nsACString& aDomain,
                                 nsIObserver* aObserver) {
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
    rv = Preferences::UnregisterCallback(NotifyObserver, prefName, pCallback,
                                         Preferences::PrefixMatch);
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  // Watch for xpcom shutdown and free our observers to eliminate any cyclic
  // references.
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    FreeObserverList();
  }
  return NS_OK;
}

/* static */
void nsPrefBranch::NotifyObserver(const char* aNewPref, void* aData) {
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

size_t nsPrefBranch::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  n += mPrefRoot.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

  n += mObservers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mObservers.ConstIter(); !iter.Done(); iter.Next()) {
    const PrefCallback* data = iter.UserData();
    n += data->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

void nsPrefBranch::FreeObserverList() {
  // We need to prevent anyone from modifying mObservers while we're iterating
  // over it. In particular, some clients will call RemoveObserver() when
  // they're removed and destructed via the iterator; we set
  // mFreeingObserverList to keep those calls from touching mObservers.
  mFreeingObserverList = true;
  for (auto iter = mObservers.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<PrefCallback>& callback = iter.Data();
    Preferences::UnregisterCallback(nsPrefBranch::NotifyObserver,
                                    callback->GetDomain(), callback,
                                    Preferences::PrefixMatch);
    iter.Remove();
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  mFreeingObserverList = false;
}

void nsPrefBranch::RemoveExpiredCallback(PrefCallback* aCallback) {
  MOZ_ASSERT(aCallback->IsExpired());
  mObservers.Remove(aCallback);
}

nsresult nsPrefBranch::GetDefaultFromPropertiesFile(const char* aPrefName,
                                                    nsAString& aReturn) {
  // The default value contains a URL to a .properties file.

  nsAutoCString propertyFileURL;
  nsresult rv = Preferences::GetCString(aPrefName, propertyFileURL,
                                        PrefValueKind::Default);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIStringBundleService> bundleService =
      services::GetStringBundleService();
  if (!bundleService) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(propertyFileURL.get(),
                                   getter_AddRefs(bundle));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return bundle->GetStringFromName(aPrefName, aReturn);
}

nsPrefBranch::PrefName nsPrefBranch::GetPrefName(
    const nsACString& aPrefName) const {
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

NS_IMPL_ISUPPORTS(nsPrefLocalizedString, nsIPrefLocalizedString,
                  nsISupportsString)

nsresult nsPrefLocalizedString::Init() {
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
nsRelativeFilePref::GetFile(nsIFile** aFile) {
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = mFile;
  NS_IF_ADDREF(*aFile);
  return NS_OK;
}

NS_IMETHODIMP
nsRelativeFilePref::SetFile(nsIFile* aFile) {
  mFile = aFile;
  return NS_OK;
}

NS_IMETHODIMP
nsRelativeFilePref::GetRelativeToKey(nsACString& aRelativeToKey) {
  aRelativeToKey.Assign(mRelativeToKey);
  return NS_OK;
}

NS_IMETHODIMP
nsRelativeFilePref::SetRelativeToKey(const nsACString& aRelativeToKey) {
  mRelativeToKey.Assign(aRelativeToKey);
  return NS_OK;
}

//===========================================================================
// class Preferences and related things
//===========================================================================

namespace mozilla {

#define INITIAL_PREF_FILES 10

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

void Preferences::HandleDirty() {
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
          NewRunnableMethod("Preferences::SavePrefFileAsynchronous",
                            sPreferences.get(),
                            &Preferences::SavePrefFileAsynchronous),
          PREF_DELAY_MS);
    }
  }
}

static nsresult openPrefFile(nsIFile* aFile, PrefValueKind aKind);

static nsresult parsePrefData(const nsCString& aData, PrefValueKind aKind);

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
class PreferencesWriter final {
 public:
  PreferencesWriter() = default;

  static nsresult Write(nsIFile* aFile, PrefSaveData& aPrefs) {
    nsCOMPtr<nsIOutputStream> outStreamSink;
    nsCOMPtr<nsIOutputStream> outStream;
    uint32_t writeAmount;
    nsresult rv;

    // Execute a "safe" save by saving through a tempfile.
    rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(outStreamSink), aFile,
                                         -1, 0600);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = NS_NewBufferedOutputStream(getter_AddRefs(outStream),
                                    outStreamSink.forget(), 4096);
    if (NS_FAILED(rv)) {
      return rv;
    }

    struct CharComparator {
      bool LessThan(const nsCString& aA, const nsCString& aB) const {
        return aA < aB;
      }

      bool Equals(const nsCString& aA, const nsCString& aB) const {
        return aA == aB;
      }
    };

    // Sort the preferences to make a readable file on disk.
    aPrefs.Sort(CharComparator());

    // Write out the file header.
    outStream->Write(kPrefFileHeader, sizeof(kPrefFileHeader) - 1,
                     &writeAmount);

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

  static void Flush() {
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

class PWRunnable : public Runnable {
 public:
  explicit PWRunnable(nsIFile* aFile) : Runnable("PWRunnable"), mFile(aFile) {}

  NS_IMETHOD Run() override {
    // If we get a nullptr on the exchange, it means that somebody
    // else has already processed the request, and we can just return.
    UniquePtr<PrefSaveData> prefs(
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

// Although this is a member of Preferences, it measures sPreferences and
// several other global structures.
/* static */
void Preferences::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                         PrefsSizes& aSizes) {
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

class PreferenceServiceReporter final : public nsIMemoryReporter {
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
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    bool aAnonymize) {
  MOZ_ASSERT(NS_IsMainThread());

  MallocSizeOf mallocSizeOf = PreferenceServiceMallocSizeOf;
  PrefsSizes sizes;

  Preferences::AddSizeOfIncludingThis(mallocSizeOf, sizes);

  if (gHashTable) {
    sizes.mHashTable += gHashTable->shallowSizeOfIncludingThis(mallocSizeOf);
    for (auto iter = gHashTable->iter(); !iter.done(); iter.next()) {
      iter.get()->AddSizeOfIncludingThis(mallocSizeOf, sizes);
    }
  }

  sizes.mPrefNameArena += gPrefNameArena.SizeOfExcludingThis(mallocSizeOf);

  for (CallbackNode* node = gFirstCallback; node; node = node->Next()) {
    node->AddSizeOfIncludingThis(mallocSizeOf, sizes);
  }

  if (gSharedMap) {
    sizes.mMisc += mallocSizeOf(gSharedMap);
  }

  MOZ_COLLECT_REPORT("explicit/preferences/hash-table", KIND_HEAP, UNITS_BYTES,
                     sizes.mHashTable, "Memory used by libpref's hash table.");

  MOZ_COLLECT_REPORT("explicit/preferences/pref-values", KIND_HEAP, UNITS_BYTES,
                     sizes.mPrefValues,
                     "Memory used by PrefValues hanging off the hash table.");

  MOZ_COLLECT_REPORT("explicit/preferences/string-values", KIND_HEAP,
                     UNITS_BYTES, sizes.mStringValues,
                     "Memory used by libpref's string pref values.");

  MOZ_COLLECT_REPORT("explicit/preferences/root-branches", KIND_HEAP,
                     UNITS_BYTES, sizes.mRootBranches,
                     "Memory used by libpref's root branches.");

  MOZ_COLLECT_REPORT("explicit/preferences/pref-name-arena", KIND_HEAP,
                     UNITS_BYTES, sizes.mPrefNameArena,
                     "Memory used by libpref's arena for pref names.");

  MOZ_COLLECT_REPORT("explicit/preferences/callbacks/objects", KIND_HEAP,
                     UNITS_BYTES, sizes.mCallbacksObjects,
                     "Memory used by pref callback objects.");

  MOZ_COLLECT_REPORT("explicit/preferences/callbacks/domains", KIND_HEAP,
                     UNITS_BYTES, sizes.mCallbacksDomains,
                     "Memory used by pref callback domains (pref names and "
                     "prefixes).");

  MOZ_COLLECT_REPORT("explicit/preferences/misc", KIND_HEAP, UNITS_BYTES,
                     sizes.mMisc, "Miscellaneous memory used by libpref.");

  if (gSharedMap) {
    if (XRE_IsParentProcess()) {
      MOZ_COLLECT_REPORT("explicit/preferences/shared-memory-map", KIND_NONHEAP,
                         UNITS_BYTES, gSharedMap->MapSize(),
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

    nsPrintfCString suspectPath(
        "preference-service-suspect/"
        "referent(pref=%s)",
        suspect.get());

    aHandleReport->Callback(
        /* process = */ EmptyCString(), suspectPath, KIND_OTHER, UNITS_COUNT,
        totalReferentCount,
        NS_LITERAL_CSTRING("A preference with a suspiciously large number "
                           "referents (symptom of a "
                           "leak)."),
        aData);
  }

  MOZ_COLLECT_REPORT(
      "preference-service/referent/strong", KIND_OTHER, UNITS_COUNT, numStrong,
      "The number of strong referents held by the preference service.");

  MOZ_COLLECT_REPORT(
      "preference-service/referent/weak/alive", KIND_OTHER, UNITS_COUNT,
      numWeakAlive,
      "The number of weak referents held by the preference service that are "
      "still alive.");

  MOZ_COLLECT_REPORT(
      "preference-service/referent/weak/dead", KIND_OTHER, UNITS_COUNT,
      numWeakDead,
      "The number of weak referents held by the preference service that are "
      "dead.");

  return NS_OK;
}

namespace {

class AddPreferencesMemoryReporterRunnable : public Runnable {
 public:
  AddPreferencesMemoryReporterRunnable()
      : Runnable("AddPreferencesMemoryReporterRunnable") {}

  NS_IMETHOD Run() override {
    return RegisterStrongMemoryReporter(new PreferenceServiceReporter());
  }
};

}  // namespace

// A list of changed prefs sent from the parent via shared memory.
static nsTArray<dom::Pref>* gChangedDomPrefs;

static const char kTelemetryPref[] = "toolkit.telemetry.enabled";
static const char kChannelPref[] = "app.update.channel";

#ifdef MOZ_WIDGET_ANDROID

static Maybe<bool> TelemetryPrefValue() {
  // Leave it unchanged if it's already set.
  // XXX: how could it already be set?
  if (Preferences::GetType(kTelemetryPref) != nsIPrefBranch::PREF_INVALID) {
    return Nothing();
  }

  // Determine the correct default for toolkit.telemetry.enabled. If this
  // build has MOZ_TELEMETRY_ON_BY_DEFAULT *or* we're on the beta channel,
  // telemetry is on by default, otherwise not. This is necessary so that
  // beta users who are testing final release builds don't flipflop defaults.
#  ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
  return Some(true);
#  else
  nsAutoCString channelPrefValue;
  Unused << Preferences::GetCString(kChannelPref, channelPrefValue,
                                    PrefValueKind::Default);
  return Some(channelPrefValue.EqualsLiteral("beta"));
#  endif
}

/* static */
void Preferences::SetupTelemetryPref() {
  MOZ_ASSERT(XRE_IsParentProcess());

  Maybe<bool> telemetryPrefValue = TelemetryPrefValue();
  if (telemetryPrefValue.isSome()) {
    Preferences::SetBool(kTelemetryPref, *telemetryPrefValue,
                         PrefValueKind::Default);
  }
}

#else  // !MOZ_WIDGET_ANDROID

static bool TelemetryPrefValue() {
  // For platforms with Unified Telemetry (here meaning not-Android),
  // toolkit.telemetry.enabled determines whether we send "extended" data.
  // We only want extended data from pre-release channels due to size.

  NS_NAMED_LITERAL_CSTRING(channel, NS_STRINGIFY(MOZ_UPDATE_CHANNEL));

  // Easy cases: Nightly, Aurora, Beta.
  if (channel.EqualsLiteral("nightly") || channel.EqualsLiteral("aurora") ||
      channel.EqualsLiteral("beta")) {
    return true;
  }

#  ifndef MOZILLA_OFFICIAL
  // Local developer builds: non-official builds on the "default" channel.
  if (channel.EqualsLiteral("default")) {
    return true;
  }
#  endif

  // Release Candidate builds: builds that think they are release builds, but
  // are shipped to beta users.
  if (channel.EqualsLiteral("release")) {
    nsAutoCString channelPrefValue;
    Unused << Preferences::GetCString(kChannelPref, channelPrefValue,
                                      PrefValueKind::Default);
    if (channelPrefValue.EqualsLiteral("beta")) {
      return true;
    }
  }

  return false;
}

/* static */
void Preferences::SetupTelemetryPref() {
  MOZ_ASSERT(XRE_IsParentProcess());

  Preferences::SetBool(kTelemetryPref, TelemetryPrefValue(),
                       PrefValueKind::Default);
  Preferences::Lock(kTelemetryPref);
}

static void CheckTelemetryPref() {
  MOZ_ASSERT(!XRE_IsParentProcess());

  // Make sure the children got passed the right telemetry pref details.
  DebugOnly<bool> value;
  MOZ_ASSERT(NS_SUCCEEDED(Preferences::GetBool(kTelemetryPref, &value)) &&
             value == TelemetryPrefValue());
  MOZ_ASSERT(Preferences::IsLocked(kTelemetryPref));
}

#endif  // MOZ_WIDGET_ANDROID

/* static */
already_AddRefed<Preferences> Preferences::GetInstanceForService() {
  if (sPreferences) {
    return do_AddRef(sPreferences);
  }

  if (sShutdown) {
    return nullptr;
  }

  sPreferences = new Preferences();

  MOZ_ASSERT(!gHashTable);
  gHashTable = new PrefsHashTable(XRE_IsParentProcess()
                                      ? kHashTableInitialLengthParent
                                      : kHashTableInitialLengthContent);

  gTelemetryLoadData =
      new nsDataHashtable<nsCStringHashKey, TelemetryLoadData>();

#ifdef DEBUG
  gOnceStaticPrefsAntiFootgun = new AntiFootgunMap();
#endif

#ifdef ACCESS_COUNTS
  MOZ_ASSERT(!gAccessCounts);
  gAccessCounts = new AccessCountsHashTable();
#endif

  nsresult rv = InitInitialObjects(/* isStartup */ true);
  if (NS_FAILED(rv)) {
    sPreferences = nullptr;
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
    nsresult rv = Preferences::GetCString("general.config.filename",
                                          lockFileName, PrefValueKind::User);
    if (NS_SUCCEEDED(rv)) {
      NS_CreateServicesFromCategory(
          "pref-config-startup",
          static_cast<nsISupports*>(static_cast<void*>(sPreferences)),
          "pref-config-startup");
    }

    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (!observerService) {
      sPreferences = nullptr;
      return nullptr;
    }

    observerService->AddObserver(sPreferences,
                                 "profile-before-change-telemetry", true);
    rv = observerService->AddObserver(sPreferences, "profile-before-change",
                                      true);

    observerService->AddObserver(sPreferences, "suspend_process_notification",
                                 true);

    if (NS_FAILED(rv)) {
      sPreferences = nullptr;
      return nullptr;
    }
  }

  const char* defaultPrefs = getenv("MOZ_DEFAULT_PREFS");
  if (defaultPrefs) {
    parsePrefData(nsCString(defaultPrefs), PrefValueKind::Default);
  }

  // Preferences::GetInstanceForService() can be called from GetService(), and
  // RegisterStrongMemoryReporter calls GetService(nsIMemoryReporter).  To
  // avoid a potential recursive GetService() call, we can't register the
  // memory reporter here; instead, do it off a runnable.
  RefPtr<AddPreferencesMemoryReporterRunnable> runnable =
      new AddPreferencesMemoryReporterRunnable();
  NS_DispatchToMainThread(runnable);

  return do_AddRef(sPreferences);
}

/* static */
bool Preferences::IsServiceAvailable() { return !!sPreferences; }

/* static */
bool Preferences::InitStaticMembers() {
  MOZ_ASSERT(NS_IsMainThread() || ServoStyleSet::IsInServoTraversal());

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

/* static */
void Preferences::Shutdown() {
  if (!sShutdown) {
    sShutdown = true;  // Don't create the singleton instance after here.
    sPreferences = nullptr;
  }
}

Preferences::Preferences()
    : mRootBranch(new nsPrefBranch("", PrefValueKind::User)),
      mDefaultRootBranch(new nsPrefBranch("", PrefValueKind::Default)) {}

Preferences::~Preferences() {
  MOZ_ASSERT(!sPreferences);

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

#ifdef DEBUG
  delete gOnceStaticPrefsAntiFootgun;
  gOnceStaticPrefsAntiFootgun = nullptr;
#endif

#ifdef ACCESS_COUNTS
  delete gAccessCounts;
#endif

  gSharedMap = nullptr;

  gPrefNameArena.Clear();
}

NS_IMPL_ISUPPORTS(Preferences, nsIPrefService, nsIObserver, nsIPrefBranch,
                  nsISupportsWeakReference)

/* static */
void Preferences::SerializePreferences(nsCString& aStr) {
  MOZ_RELEASE_ASSERT(InitStaticMembers());

  aStr.Truncate();

  for (auto iter = gHashTable->iter(); !iter.done(); iter.next()) {
    Pref* pref = iter.get().get();
    if (!pref->IsTypeNone() && pref->HasAdvisablySizedValues()) {
      pref->SerializeAndAppend(aStr);
    }
  }

  aStr.Append('\0');
}

/* static */
void Preferences::DeserializePreferences(char* aStr, size_t aPrefsLen) {
  MOZ_ASSERT(!XRE_IsParentProcess());

  MOZ_ASSERT(!gChangedDomPrefs);
  gChangedDomPrefs = new nsTArray<dom::Pref>();

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

// Forward declarations.
namespace StaticPrefs {

static void InitAll(bool aIsStartup);
static void InitOncePrefs();
static void InitStaticPrefsFromShared();
static void RegisterOncePrefs(SharedPrefMapBuilder& aBuilder);

}  // namespace StaticPrefs

/* static */
FileDescriptor Preferences::EnsureSnapshot(size_t* aSize) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!gSharedMap) {
    SharedPrefMapBuilder builder;

    for (auto iter = gHashTable->iter(); !iter.done(); iter.next()) {
      iter.get()->AddToMap(builder);
    }

    // Store the current value of `once`-mirrored prefs. After this point they
    // will be immutable.
    StaticPrefs::RegisterOncePrefs(builder);

    gSharedMap = new SharedPrefMap(std::move(builder));

    // Once we've built a snapshot of the database, there's no need to continue
    // storing dynamic copies of the preferences it contains. Once we reset the
    // hashtable, preference lookups will fall back to the snapshot for any
    // preferences not in the dynamic hashtable.
    //
    // And since the majority of the database is now contained in the snapshot,
    // we can initialize the hashtable with the expected number of per-session
    // changed preferences, rather than the expected total number of
    // preferences.
    gHashTable->clearAndCompact();
    Unused << gHashTable->reserve(kHashTableInitialLengthContent);

    gPrefNameArena.Clear();
  }

  *aSize = gSharedMap->MapSize();
  return gSharedMap->CloneFileDescriptor();
}

/* static */
void Preferences::InitSnapshot(const FileDescriptor& aHandle, size_t aSize) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(!gSharedMap);

  gSharedMap = new SharedPrefMap(aHandle, aSize);

  StaticPrefs::InitStaticPrefsFromShared();
}

/* static */
void Preferences::InitializeUserPrefs() {
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
Preferences::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* someData) {
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
Preferences::ReadDefaultPrefsFromFile(nsIFile* aFile) {
  ENSURE_PARENT_PROCESS("Preferences::ReadDefaultPrefsFromFile", "all prefs");

  if (!aFile) {
    NS_ERROR("ReadDefaultPrefsFromFile requires a parameter");
    return NS_ERROR_INVALID_ARG;
  }

  return openPrefFile(aFile, PrefValueKind::Default);
}

NS_IMETHODIMP
Preferences::ReadUserPrefsFromFile(nsIFile* aFile) {
  ENSURE_PARENT_PROCESS("Preferences::ReadUserPrefsFromFile", "all prefs");

  if (!aFile) {
    NS_ERROR("ReadUserPrefsFromFile requires a parameter");
    return NS_ERROR_INVALID_ARG;
  }

  return openPrefFile(aFile, PrefValueKind::User);
}

NS_IMETHODIMP
Preferences::ResetPrefs() {
  ENSURE_PARENT_PROCESS("Preferences::ResetPrefs", "all prefs");

  if (gSharedMap) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  gHashTable->clearAndCompact();
  Unused << gHashTable->reserve(kHashTableInitialLengthParent);

  gPrefNameArena.Clear();

  return InitInitialObjects(/* isStartup */ false);
}

NS_IMETHODIMP
Preferences::ResetUserPrefs() {
  ENSURE_PARENT_PROCESS("Preferences::ResetUserPrefs", "all prefs");
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  MOZ_ASSERT(NS_IsMainThread());

  Vector<const char*> prefNames;
  for (auto iter = gHashTable->modIter(); !iter.done(); iter.next()) {
    Pref* pref = iter.get().get();

    if (pref->HasUserValue()) {
      if (!prefNames.append(pref->Name())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      pref->ClearUserValue();
      if (!pref->HasDefaultValue()) {
        iter.remove();
      }
    }
  }

  for (const char* prefName : prefNames) {
    NotifyCallbacks(prefName);
  }

  Preferences::HandleDirty();
  return NS_OK;
}

bool Preferences::AllowOffMainThreadSave() {
  // Put in a preference that allows us to disable off main thread preference
  // file save.
  if (sAllowOMTPrefWrite < 0) {
    bool value = false;
    Preferences::GetBool("preferences.allow.omt-write", &value);
    sAllowOMTPrefWrite = value ? 1 : 0;
  }

  return !!sAllowOMTPrefWrite;
}

nsresult Preferences::SavePrefFileBlocking() {
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

nsresult Preferences::SavePrefFileAsynchronous() {
  return SavePrefFileInternal(nullptr, SaveMethod::Asynchronous);
}

NS_IMETHODIMP
Preferences::SavePrefFile(nsIFile* aFile) {
  // This is the method accessible from service API. Make it off main thread.
  return SavePrefFileInternal(aFile, SaveMethod::Asynchronous);
}

/* static */
void Preferences::SetPreference(const dom::Pref& aDomPref) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  NS_ENSURE_TRUE(InitStaticMembers(), (void)0);

  const char* prefName = aDomPref.name().get();

  Pref* pref;
  auto p = gHashTable->lookupForAdd(prefName);
  if (!p) {
    pref = new Pref(prefName);
    if (!gHashTable->add(p, pref)) {
      delete pref;
      return;
    }
  } else {
    pref = p->get();
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
    // If the preference exists in the shared map, we need to keep the dynamic
    // entry around to mask it.
    if (gSharedMap->Has(pref->Name())) {
      pref->SetType(PrefType::None);
    } else {
      gHashTable->remove(prefName);
    }
    pref = nullptr;
  }

  // Note: we don't have to worry about HandleDirty() because we are setting
  // prefs in the content process that have come from the parent process.

  if (valueChanged) {
    if (pref) {
      NotifyCallbacks(prefName, PrefWrapper(pref));
    } else {
      NotifyCallbacks(prefName);
    }
  }
}

/* static */
void Preferences::GetPreference(dom::Pref* aDomPref) {
  MOZ_ASSERT(XRE_IsParentProcess());

  Pref* pref = pref_HashTableLookup(aDomPref->name().get());
  if (pref && pref->HasAdvisablySizedValues()) {
    pref->ToDomPref(aDomPref);
  }
}

#ifdef DEBUG
bool Preferences::ArePrefsInitedInContentProcess() {
  MOZ_ASSERT(!XRE_IsParentProcess());
  return gContentProcessPrefsAreInited;
}
#endif

NS_IMETHODIMP
Preferences::GetBranch(const char* aPrefRoot, nsIPrefBranch** aRetVal) {
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
Preferences::GetDefaultBranch(const char* aPrefRoot, nsIPrefBranch** aRetVal) {
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
Preferences::ReadStats(nsIPrefStatsCallback* aCallback) {
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
Preferences::ResetStats() {
#ifdef ACCESS_COUNTS
  gAccessCounts->Clear();
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
Preferences::GetDirty(bool* aRetVal) {
  *aRetVal = mDirty;
  return NS_OK;
}

nsresult Preferences::NotifyServiceObservers(const char* aTopic) {
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  auto subject = static_cast<nsIPrefService*>(this);
  observerService->NotifyObservers(subject, aTopic, nullptr);

  return NS_OK;
}

already_AddRefed<nsIFile> Preferences::ReadSavedPrefs() {
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

void Preferences::ReadUserOverridePrefs() {
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

nsresult Preferences::MakeBackupPrefFile(nsIFile* aFile) {
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

nsresult Preferences::SavePrefFileInternal(nsIFile* aFile,
                                           SaveMethod aSaveMethod) {
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

nsresult Preferences::WritePrefFile(nsIFile* aFile, SaveMethod aSaveMethod) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!gHashTable) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AUTO_PROFILER_LABEL("Preferences::WritePrefFile", OTHER);

  if (AllowOffMainThreadSave()) {
    nsresult rv = NS_OK;
    UniquePtr<PrefSaveData> prefs = MakeUnique<PrefSaveData>(pref_savePrefs());

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

static nsresult openPrefFile(nsIFile* aFile, PrefValueKind aKind) {
  MOZ_ASSERT(XRE_IsParentProcess());

  TimeStamp startTime = TimeStamp::Now();

  nsCString data;
  MOZ_TRY_VAR(data, URLPreloader::ReadFile(aFile));

  nsAutoString filenameUtf16;
  aFile->GetLeafName(filenameUtf16);
  NS_ConvertUTF16toUTF8 filename(filenameUtf16);

  nsAutoString path;
  aFile->GetPath(path);

  Parser parser;
  if (!parser.Parse(filename, aKind, NS_ConvertUTF16toUTF8(path).get(),
                    startTime, data)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

static nsresult parsePrefData(const nsCString& aData, PrefValueKind aKind) {
  TimeStamp startTime = TimeStamp::Now();
  const nsCString path = NS_LITERAL_CSTRING("$MOZ_DEFAULT_PREFS");

  Parser parser;
  if (!parser.Parse(path, aKind, path.get(), startTime, aData)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

static int pref_CompareFileNames(nsIFile* aFile1, nsIFile* aFile2,
                                 void* /* unused */) {
  nsAutoCString filename1, filename2;
  aFile1->GetNativeLeafName(filename1);
  aFile2->GetNativeLeafName(filename2);

  return Compare(filename2, filename1);
}

// Load default pref files from a directory. The files in the directory are
// sorted reverse-alphabetically; a set of "special file names" may be
// specified which are loaded after all the others.
static nsresult pref_LoadPrefsInDir(nsIFile* aDir,
                                    char const* const* aSpecialFiles,
                                    uint32_t aSpecialFilesCount) {
  MOZ_ASSERT(XRE_IsParentProcess());

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
    if (StringEndsWith(leafName, NS_LITERAL_CSTRING(".js"),
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

static nsresult pref_ReadPrefFromJar(nsZipArchive* aJarReader,
                                     const char* aName) {
  TimeStamp startTime = TimeStamp::Now();

  nsCString manifest;
  MOZ_TRY_VAR(manifest,
              URLPreloader::ReadZip(aJarReader, nsDependentCString(aName)));

  Parser parser;
  if (!parser.Parse(nsDependentCString(aName), PrefValueKind::Default, aName,
                    startTime, manifest)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

static nsresult pref_ReadDefaultPrefs(const RefPtr<nsZipArchive> jarReader,
                                      const char* path) {
  nsZipFind* findPtr;
  nsAutoPtr<nsZipFind> find;
  nsTArray<nsCString> prefEntries;
  const char* entryName;
  uint16_t entryNameLen;

  nsresult rv = jarReader->FindInit(path, &findPtr);
  NS_ENSURE_SUCCESS(rv, rv);

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

  return NS_OK;
}

// These preference getter wrappers allow us to look up the value for static
// preferences based on their native types, rather than manually mapping them to
// the appropriate Preferences::Get* functions.
// We define these methods in a struct which is made friend of Preferences in
// order to access private members.
struct Internals {
  template <typename T>
  static nsresult GetPrefValue(const char* aPrefName, T&& aResult,
                               PrefValueKind aKind) {
    NS_ENSURE_TRUE(Preferences::InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

    if (Maybe<PrefWrapper> pref = pref_Lookup(aPrefName)) {
      return pref->GetValue(aKind, std::forward<T>(aResult));
    }
    return NS_ERROR_UNEXPECTED;
  }

  template <typename T>
  static nsresult GetSharedPrefValue(const char* aName, T* aResult) {
    if (Maybe<PrefWrapper> pref = pref_SharedLookup(aName)) {
      return pref->GetValue(PrefValueKind::User, aResult);
    }
    return NS_ERROR_UNEXPECTED;
  }

  template <typename T>
  static T GetPref(const char* aPrefName, T aFallback,
                   PrefValueKind aKind = PrefValueKind::User) {
    T result = aFallback;
    GetPrefValue(aPrefName, &result, aKind);
    return result;
  }

  template <typename T>
  static void UpdateMirror(const char* aPref, void* aMirror) {
    StripAtomic<T> value;
    nsresult rv = GetPrefValue(aPref, &value, PrefValueKind::User);
    if (NS_SUCCEEDED(rv)) {
      *static_cast<T*>(aMirror) = value;
    } else {
      // GetPrefValue() can fail if the update is caused by the pref being
      // deleted. In that case the mirror variable will be untouched, thus
      // keeping the value it had prior to the deletion. (Note that this case
      // won't happen for a deletion via DeleteBranch() unless bug 343600 is
      // fixed, but it will happen for a deletion via ClearUserPref().)
      //
      // This is a case we want to avoid in general because it's a bit unclear
      // what value the mirror variable should take; hence the assertion
      // failure. Once all VarCache prefs are removed in favour of static prefs
      // (bug 1448219) the plan is to mark static prefs as undeletable and this
      // case will become impossible.
      NS_WARNING(nsPrintfCString("VarChanged failure: %s\n", aPref).get());
      MOZ_ASSERT(false);
    }
  }

  template <typename T>
  static nsresult RegisterCallback(void* aMirror, const nsACString& aPref) {
    return Preferences::RegisterCallback(UpdateMirror<T>, aPref, aMirror,
                                         Preferences::ExactMatch,
                                         /* isPriority */ true);
  }
};

// Initialize default preference JavaScript buffers from appropriate TEXT
// resources.
/* static */
nsresult Preferences::InitInitialObjects(bool aIsStartup) {
  MOZ_ASSERT(NS_IsMainThread());

  // Initialize static prefs before prefs from data files so that the latter
  // will override the former.
  StaticPrefs::InitAll(aIsStartup);

  if (!XRE_IsParentProcess()) {
    MOZ_DIAGNOSTIC_ASSERT(gSharedMap);
    return NS_OK;
  }

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

  nsresult rv = NS_ERROR_FAILURE;
  nsZipFind* findPtr;
  nsAutoPtr<nsZipFind> find;
  nsTArray<nsCString> prefEntries;
  const char* entryName;
  uint16_t entryNameLen;

  RefPtr<nsZipArchive> jarReader = Omnijar::GetReader(Omnijar::GRE);
  if (jarReader) {
#ifdef MOZ_WIDGET_ANDROID
    // Try to load an architecture-specific greprefs.js first. This will be
    // present in FAT AAR builds of GeckoView on Android.
    const char* abi = getenv("MOZ_ANDROID_CPU_ABI");
    if (abi) {
      nsAutoCString path;
      path.AppendPrintf("%s/greprefs.js", abi);
      rv = pref_ReadPrefFromJar(jarReader, path.get());
    }

    if (NS_FAILED(rv)) {
      // Fallback to toplevel greprefs.js if arch-specific load fails.
      rv = pref_ReadPrefFromJar(jarReader, "greprefs.js");
    }
#else
    // Load jar:$gre/omni.jar!/greprefs.js.
    rv = pref_ReadPrefFromJar(jarReader, "greprefs.js");
#endif
    NS_ENSURE_SUCCESS(rv, rv);

    // Load jar:$gre/omni.jar!/defaults/pref/*.js.
    rv = pref_ReadDefaultPrefs(jarReader, "defaults/pref/*.js$");
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_WIDGET_ANDROID
    // Load jar:$gre/omni.jar!/defaults/pref/$MOZ_ANDROID_CPU_ABI/*.js.
    nsAutoCString path;
    path.AppendPrintf("jar:$gre/omni.jar!/defaults/pref/%s/*.js$", abi);
    pref_ReadDefaultPrefs(jarReader, path.get());
    NS_ENSURE_SUCCESS(rv, rv);
#endif
  } else {
    // Load $gre/greprefs.js.
    nsCOMPtr<nsIFile> greprefsFile;
    rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(greprefsFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = greprefsFile->AppendNative(NS_LITERAL_CSTRING("greprefs.js"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = openPrefFile(greprefsFile, PrefValueKind::Default);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "Error parsing GRE default preferences. Is this an old-style "
          "embedding app?");
    }
  }

  // Load $gre/defaults/pref/*.js.
  nsCOMPtr<nsIFile> defaultPrefDir;
  rv = NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR,
                              getter_AddRefs(defaultPrefDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // These pref file names should not be used: we process them after all other
  // application pref files for backwards compatibility.
  static const char* specialFiles[] = {
#if defined(XP_MACOSX)
    "macprefs.js"
#elif defined(XP_WIN)
    "winpref.js"
#elif defined(XP_UNIX)
    "unix.js"
#  if defined(_AIX)
    ,
    "aix.js"
#  endif
#elif defined(XP_BEOS)
    "beos.js"
#endif
  };

  rv = pref_LoadPrefsInDir(defaultPrefDir, specialFiles,
                           ArrayLength(specialFiles));
  if (NS_FAILED(rv)) {
    NS_WARNING("Error parsing application default preferences.");
  }

  // Load jar:$app/omni.jar!/defaults/preferences/*.js
  // or jar:$gre/omni.jar!/defaults/preferences/*.js.
  RefPtr<nsZipArchive> appJarReader = Omnijar::GetReader(Omnijar::APP);

  // GetReader(Omnijar::APP) returns null when `$app == $gre`, in
  // which case we look for app-specific default preferences in $gre.
  if (!appJarReader) {
    appJarReader = Omnijar::GetReader(Omnijar::GRE);
  }

  if (appJarReader) {
    rv = appJarReader->FindInit("defaults/preferences/*.js$", &findPtr);
    NS_ENSURE_SUCCESS(rv, rv);
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
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> list;
  dirSvc->Get(NS_APP_PREFS_DEFAULTS_DIR_LIST, NS_GET_IID(nsISimpleEnumerator),
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

  NS_CreateServicesFromCategory(NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID, nullptr,
                                NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID);

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  NS_ENSURE_SUCCESS(rv, rv);

  observerService->NotifyObservers(nullptr, NS_PREFSERVICE_APPDEFAULTS_TOPIC_ID,
                                   nullptr);

  return NS_OK;
}

/* static */
nsresult Preferences::GetBool(const char* aPrefName, bool* aResult,
                              PrefValueKind aKind) {
  MOZ_ASSERT(aResult);
  return Internals::GetPrefValue(aPrefName, aResult, aKind);
}

/* static */
nsresult Preferences::GetInt(const char* aPrefName, int32_t* aResult,
                             PrefValueKind aKind) {
  MOZ_ASSERT(aResult);
  return Internals::GetPrefValue(aPrefName, aResult, aKind);
}

/* static */
nsresult Preferences::GetFloat(const char* aPrefName, float* aResult,
                               PrefValueKind aKind) {
  MOZ_ASSERT(aResult);
  return Internals::GetPrefValue(aPrefName, aResult, aKind);
}

/* static */
nsresult Preferences::GetCString(const char* aPrefName, nsACString& aResult,
                                 PrefValueKind aKind) {
  aResult.SetIsVoid(true);
  return Internals::GetPrefValue(aPrefName, aResult, aKind);
}

/* static */
nsresult Preferences::GetString(const char* aPrefName, nsAString& aResult,
                                PrefValueKind aKind) {
  nsAutoCString result;
  nsresult rv = Preferences::GetCString(aPrefName, result, aKind);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(result, aResult);
  }
  return rv;
}

/* static */
nsresult Preferences::GetLocalizedCString(const char* aPrefName,
                                          nsACString& aResult,
                                          PrefValueKind aKind) {
  nsAutoString result;
  nsresult rv = GetLocalizedString(aPrefName, result, aKind);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF16toUTF8(result, aResult);
  }
  return rv;
}

/* static */
nsresult Preferences::GetLocalizedString(const char* aPrefName,
                                         nsAString& aResult,
                                         PrefValueKind aKind) {
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
  nsresult rv = GetRootBranch(aKind)->GetComplexValue(
      aPrefName, NS_GET_IID(nsIPrefLocalizedString),
      getter_AddRefs(prefLocalString));
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(prefLocalString, "Succeeded but the result is NULL");
    prefLocalString->GetData(aResult);
  }
  return rv;
}

/* static */
nsresult Preferences::GetComplex(const char* aPrefName, const nsIID& aType,
                                 void** aResult, PrefValueKind aKind) {
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return GetRootBranch(aKind)->GetComplexValue(aPrefName, aType, aResult);
}

/* static */
bool Preferences::GetBool(const char* aPrefName, bool aFallback,
                          PrefValueKind aKind) {
  return Internals::GetPref(aPrefName, aFallback, aKind);
}

/* static */
int32_t Preferences::GetInt(const char* aPrefName, int32_t aFallback,
                            PrefValueKind aKind) {
  return Internals::GetPref(aPrefName, aFallback, aKind);
}

/* static */
uint32_t Preferences::GetUint(const char* aPrefName, uint32_t aFallback,
                              PrefValueKind aKind) {
  return Internals::GetPref(aPrefName, aFallback, aKind);
}

/* static */
float Preferences::GetFloat(const char* aPrefName, float aFallback,
                            PrefValueKind aKind) {
  return Internals::GetPref(aPrefName, aFallback, aKind);
}

/* static */
nsresult Preferences::SetCString(const char* aPrefName,
                                 const nsACString& aValue,
                                 PrefValueKind aKind) {
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
  return pref_SetPref(aPrefName, PrefType::String, aKind, prefValue,
                      /* isSticky */ false,
                      /* isLocked */ false,
                      /* fromInit */ false);
}

/* static */
nsresult Preferences::SetBool(const char* aPrefName, bool aValue,
                              PrefValueKind aKind) {
  ENSURE_PARENT_PROCESS("SetBool", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  PrefValue prefValue;
  prefValue.mBoolVal = aValue;
  return pref_SetPref(aPrefName, PrefType::Bool, aKind, prefValue,
                      /* isSticky */ false,
                      /* isLocked */ false,
                      /* fromInit */ false);
}

/* static */
nsresult Preferences::SetInt(const char* aPrefName, int32_t aValue,
                             PrefValueKind aKind) {
  ENSURE_PARENT_PROCESS("SetInt", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  PrefValue prefValue;
  prefValue.mIntVal = aValue;
  return pref_SetPref(aPrefName, PrefType::Int, aKind, prefValue,
                      /* isSticky */ false,
                      /* isLocked */ false,
                      /* fromInit */ false);
}

/* static */
nsresult Preferences::SetComplex(const char* aPrefName, const nsIID& aType,
                                 nsISupports* aValue, PrefValueKind aKind) {
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return GetRootBranch(aKind)->SetComplexValue(aPrefName, aType, aValue);
}

/* static */
nsresult Preferences::Lock(const char* aPrefName) {
  ENSURE_PARENT_PROCESS("Lock", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref;
  MOZ_TRY_VAR(pref,
              pref_LookupForModify(aPrefName, [](const PrefWrapper& aPref) {
                return !aPref.IsLocked();
              }));

  if (pref) {
    pref->SetIsLocked(true);
    NotifyCallbacks(aPrefName, PrefWrapper(pref));
  }

  return NS_OK;
}

/* static */
nsresult Preferences::Unlock(const char* aPrefName) {
  ENSURE_PARENT_PROCESS("Unlock", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  Pref* pref;
  MOZ_TRY_VAR(pref,
              pref_LookupForModify(aPrefName, [](const PrefWrapper& aPref) {
                return aPref.IsLocked();
              }));

  if (pref) {
    pref->SetIsLocked(false);
    NotifyCallbacks(aPrefName, PrefWrapper(pref));
  }

  return NS_OK;
}

/* static */
bool Preferences::IsLocked(const char* aPrefName) {
  NS_ENSURE_TRUE(InitStaticMembers(), false);

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  return pref.isSome() && pref->IsLocked();
}

/* static */
nsresult Preferences::ClearUser(const char* aPrefName) {
  ENSURE_PARENT_PROCESS("ClearUser", aPrefName);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);

  auto result = pref_LookupForModify(
      aPrefName, [](const PrefWrapper& aPref) { return aPref.HasUserValue(); });
  if (result.isErr()) {
    return NS_OK;
  }

  if (Pref* pref = result.unwrap()) {
    pref->ClearUserValue();

    if (!pref->HasDefaultValue()) {
      if (!gSharedMap || !gSharedMap->Has(pref->Name())) {
        gHashTable->remove(aPrefName);
      } else {
        pref->SetType(PrefType::None);
      }

      NotifyCallbacks(aPrefName);
    } else {
      NotifyCallbacks(aPrefName, PrefWrapper(pref));
    }

    Preferences::HandleDirty();
  }
  return NS_OK;
}

/* static */
bool Preferences::HasUserValue(const char* aPrefName) {
  NS_ENSURE_TRUE(InitStaticMembers(), false);

  Maybe<PrefWrapper> pref = pref_Lookup(aPrefName);
  return pref.isSome() && pref->HasUserValue();
}

/* static */
int32_t Preferences::GetType(const char* aPrefName) {
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

/* static */
nsresult Preferences::AddStrongObserver(nsIObserver* aObserver,
                                        const nsACString& aPref) {
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->AddObserver(aPref, aObserver, false);
}

/* static */
nsresult Preferences::AddWeakObserver(nsIObserver* aObserver,
                                      const nsACString& aPref) {
  MOZ_ASSERT(aObserver);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->AddObserver(aPref, aObserver, true);
}

/* static */
nsresult Preferences::RemoveObserver(nsIObserver* aObserver,
                                     const nsACString& aPref) {
  MOZ_ASSERT(aObserver);
  if (sShutdown) {
    MOZ_ASSERT(!sPreferences);
    return NS_OK;  // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);
  return sPreferences->mRootBranch->RemoveObserver(aPref, aObserver);
}

template <typename T>
static void AssertNotMallocAllocated(T* aPtr) {
#if defined(DEBUG) && defined(MOZ_MEMORY)
  jemalloc_ptr_info_t info;
  jemalloc_ptr_info((void*)aPtr, &info);
  MOZ_ASSERT(info.tag == TagUnknown);
#endif
}

/* static */
nsresult Preferences::AddStrongObservers(nsIObserver* aObserver,
                                         const char** aPrefs) {
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

/* static */
nsresult Preferences::AddWeakObservers(nsIObserver* aObserver,
                                       const char** aPrefs) {
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

/* static */
nsresult Preferences::RemoveObservers(nsIObserver* aObserver,
                                      const char** aPrefs) {
  MOZ_ASSERT(aObserver);
  if (sShutdown) {
    MOZ_ASSERT(!sPreferences);
    return NS_OK;  // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  for (uint32_t i = 0; aPrefs[i]; i++) {
    nsresult rv = RemoveObserver(aObserver, nsDependentCString(aPrefs[i]));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

template <typename T>
/* static */
nsresult Preferences::RegisterCallbackImpl(PrefChangedFunc aCallback,
                                           T& aPrefNode, void* aData,
                                           MatchKind aMatchKind,
                                           bool aIsPriority) {
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

/* static */
nsresult Preferences::RegisterCallback(PrefChangedFunc aCallback,
                                       const nsACString& aPrefNode, void* aData,
                                       MatchKind aMatchKind, bool aIsPriority) {
  return RegisterCallbackImpl(aCallback, aPrefNode, aData, aMatchKind,
                              aIsPriority);
}

/* static */
nsresult Preferences::RegisterCallbacks(PrefChangedFunc aCallback,
                                        const char** aPrefs, void* aData,
                                        MatchKind aMatchKind) {
  return RegisterCallbackImpl(aCallback, aPrefs, aData, aMatchKind);
}

/* static */
nsresult Preferences::RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                              const nsACString& aPref,
                                              void* aClosure,
                                              MatchKind aMatchKind) {
  MOZ_ASSERT(aCallback);
  nsresult rv = RegisterCallback(aCallback, aPref, aClosure, aMatchKind);
  if (NS_SUCCEEDED(rv)) {
    (*aCallback)(PromiseFlatCString(aPref).get(), aClosure);
  }
  return rv;
}

/* static */
nsresult Preferences::RegisterCallbacksAndCall(PrefChangedFunc aCallback,
                                               const char** aPrefs,
                                               void* aClosure) {
  MOZ_ASSERT(aCallback);

  nsresult rv =
      RegisterCallbacks(aCallback, aPrefs, aClosure, MatchKind::ExactMatch);
  if (NS_SUCCEEDED(rv)) {
    for (const char** ptr = aPrefs; *ptr; ptr++) {
      (*aCallback)(*ptr, aClosure);
    }
  }
  return rv;
}

template <typename T>
/* static */
nsresult Preferences::UnregisterCallbackImpl(PrefChangedFunc aCallback,
                                             T& aPrefNode, void* aData,
                                             MatchKind aMatchKind) {
  MOZ_ASSERT(aCallback);
  if (sShutdown) {
    MOZ_ASSERT(!sPreferences);
    return NS_OK;  // Observers have been released automatically.
  }
  NS_ENSURE_TRUE(sPreferences, NS_ERROR_NOT_AVAILABLE);

  nsresult rv = NS_ERROR_FAILURE;
  CallbackNode* node = gFirstCallback;
  CallbackNode* prev_node = nullptr;

  while (node) {
    if (node->Func() == aCallback && node->Data() == aData &&
        node->MatchKind() == aMatchKind && node->DomainIs(aPrefNode)) {
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

/* static */
nsresult Preferences::UnregisterCallback(PrefChangedFunc aCallback,
                                         const nsACString& aPrefNode,
                                         void* aData, MatchKind aMatchKind) {
  return UnregisterCallbackImpl<const nsACString&>(aCallback, aPrefNode, aData,
                                                   aMatchKind);
}

/* static */
nsresult Preferences::UnregisterCallbacks(PrefChangedFunc aCallback,
                                          const char** aPrefs, void* aData,
                                          MatchKind aMatchKind) {
  return UnregisterCallbackImpl(aCallback, aPrefs, aData, aMatchKind);
}

template <typename T>
static void AddMirrorCallback(T* aMirror, const nsACString& aPref) {
  MOZ_ASSERT(NS_IsMainThread());

  Internals::RegisterCallback<T>(aMirror, aPref);
}

template <typename T>
static void AddMirror(T* aMirror, const nsACString& aPref,
                      StripAtomic<T> aDefault) {
  *aMirror = Internals::GetPref(PromiseFlatCString(aPref).get(), aDefault);
  AddMirrorCallback(aMirror, aPref);
}

/* static */
void Preferences::AddBoolVarCache(bool* aCache, const nsACString& aPref,
                                  bool aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

template <MemoryOrdering Order>
/* static */
void Preferences::AddAtomicBoolVarCache(Atomic<bool, Order>* aCache,
                                        const nsACString& aPref,
                                        bool aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

/* static */
void Preferences::AddIntVarCache(int32_t* aCache, const nsACString& aPref,
                                 int32_t aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

template <MemoryOrdering Order>
/* static */
void Preferences::AddAtomicIntVarCache(Atomic<int32_t, Order>* aCache,
                                       const nsACString& aPref,
                                       int32_t aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

/* static */
void Preferences::AddUintVarCache(uint32_t* aCache, const nsACString& aPref,
                                  uint32_t aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

template <MemoryOrdering Order>
/* static */
void Preferences::AddAtomicUintVarCache(Atomic<uint32_t, Order>* aCache,
                                        const nsACString& aPref,
                                        uint32_t aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

// Since the definition of template functions is not in a header file, we
// need to explicitly specify the instantiations that are required. Currently
// limited orders are needed and therefore implemented.
template void Preferences::AddAtomicBoolVarCache(Atomic<bool, Relaxed>*,
                                                 const nsACString&, bool);

template void Preferences::AddAtomicBoolVarCache(Atomic<bool, ReleaseAcquire>*,
                                                 const nsACString&, bool);

template void Preferences::AddAtomicBoolVarCache(
    Atomic<bool, SequentiallyConsistent>*, const nsACString&, bool);

template void Preferences::AddAtomicIntVarCache(Atomic<int32_t, Relaxed>*,
                                                const nsACString&, int32_t);

template void Preferences::AddAtomicUintVarCache(Atomic<uint32_t, Relaxed>*,
                                                 const nsACString&, uint32_t);

template void Preferences::AddAtomicUintVarCache(
    Atomic<uint32_t, ReleaseAcquire>*, const nsACString&, uint32_t);

template void Preferences::AddAtomicUintVarCache(
    Atomic<uint32_t, SequentiallyConsistent>*, const nsACString&, uint32_t);

/* static */
void Preferences::AddFloatVarCache(float* aCache, const nsACString& aPref,
                                   float aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

/* static */
void Preferences::AddAtomicFloatVarCache(std::atomic<float>* aCache,
                                         const nsACString& aPref,
                                         float aDefault) {
  AddMirror(aCache, aPref, aDefault);
}

// The InitPref_*() functions below end in a `_<type>` suffix because they are
// used by the PREF macro definition in InitAll() below.

static void InitPref_bool(const char* aName, bool aDefaultValue) {
  MOZ_ASSERT(XRE_IsParentProcess());
  PrefValue value;
  value.mBoolVal = aDefaultValue;
  pref_SetPref(aName, PrefType::Bool, PrefValueKind::Default, value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

static void InitPref_int32_t(const char* aName, int32_t aDefaultValue) {
  MOZ_ASSERT(XRE_IsParentProcess());
  PrefValue value;
  value.mIntVal = aDefaultValue;
  pref_SetPref(aName, PrefType::Int, PrefValueKind::Default, value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

static void InitPref_uint32_t(const char* aName, uint32_t aDefaultValue) {
  InitPref_int32_t(aName, int32_t(aDefaultValue));
}

static void InitPref_float(const char* aName, float aDefaultValue) {
  MOZ_ASSERT(XRE_IsParentProcess());
  PrefValue value;
  // Convert the value in a locale-independent way.
  nsAutoCString defaultValue;
  defaultValue.AppendFloat(aDefaultValue);
  value.mStringVal = defaultValue.get();
  pref_SetPref(aName, PrefType::String, PrefValueKind::Default, value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

static void InitPref_String(const char* aName, const char* aDefaultValue) {
  MOZ_ASSERT(XRE_IsParentProcess());
  PrefValue value;
  value.mStringVal = aDefaultValue;
  pref_SetPref(aName, PrefType::String, PrefValueKind::Default, value,
               /* isSticky */ false,
               /* isLocked */ false,
               /* fromInit */ true);
}

static void InitPref(const char* aName, bool aDefaultValue) {
  InitPref_bool(aName, aDefaultValue);
}
static void InitPref(const char* aName, int32_t aDefaultValue) {
  InitPref_int32_t(aName, aDefaultValue);
}
static void InitPref(const char* aName, uint32_t aDefaultValue) {
  InitPref_uint32_t(aName, aDefaultValue);
}
static void InitPref(const char* aName, float aDefaultValue) {
  InitPref_float(aName, aDefaultValue);
}

template <typename T>
static void InitAlwaysPref(const nsACString& aName, T* aCache,
                           StripAtomic<T> aDefaultValue, bool aIsStartup,
                           bool aIsParent) {
  // In the parent process, set/reset the pref value and the `always` mirror (if
  // there is one) to the default value.
  // - `once` mirrors will be initialized lazily in InitOncePrefs().
  // - In child processes, the parent sends the correct initial values via
  //   shared memory, so we do not re-initialize them here.
  if (aIsParent) {
    InitPref(PromiseFlatCString(aName).get(), aDefaultValue);
    *aCache = aDefaultValue;
  }

  // At startup, setup the callback for the `always` mirror (if there is one).
  if (MOZ_LIKELY(aIsStartup)) {
    AddMirrorCallback(aCache, aName);
  }
}

static Atomic<bool> sOncePrefRead(false);
static StaticMutex sOncePrefMutex;

namespace StaticPrefs {

void MaybeInitOncePrefs() {
  if (MOZ_LIKELY(sOncePrefRead)) {
    // `once`-mirrored prefs have already been initialized to their default
    // value.
    return;
  }
  StaticMutexAutoLock lock(sOncePrefMutex);
  if (NS_IsMainThread()) {
    InitOncePrefs();
  } else {
    RefPtr<Runnable> runnable = NS_NewRunnableFunction(
        "Preferences::MaybeInitOncePrefs", [&]() { InitOncePrefs(); });
    // This logic needs to run on the main thread
    SyncRunnable::DispatchToThread(
        SystemGroup::EventTargetFor(TaskCategory::Other), runnable);
  }
  sOncePrefRead = true;
}

// For mirrored prefs we generate a variable definition.
#define NEVER_PREF(name, cpp_type, value)
#define ALWAYS_PREF(name, base_id, full_id, cpp_type, default_value) \
  cpp_type sMirror_##full_id(default_value);
#define ONCE_PREF(name, base_id, full_id, cpp_type, default_value) \
  cpp_type sMirror_##full_id(default_value);
#include "mozilla/StaticPrefListAll.h"
#undef NEVER_PREF
#undef ALWAYS_PREF
#undef ONCE_PREF

static void InitAll(bool aIsStartup) {
  MOZ_ASSERT(NS_IsMainThread());

  bool isParent = XRE_IsParentProcess();

  // For all prefs we generate some initialization code.
  //
  // The InitPref_*() functions have a type suffix to avoid ambiguity between
  // prefs having int32_t and float default values. That suffix is not needed
  // for the InitAlwaysPref() functions because they take a pointer parameter,
  // which prevents automatic int-to-float coercion.
  //
  // In content processes, we rely on the parent to send us the correct initial
  // values via shared memory, so we do not re-initialize them here.
  if (isParent) {
#define NEVER_PREF(name, cpp_type, value) InitPref_##cpp_type(name, value);
#define ALWAYS_PREF(name, base_id, full_id, cpp_type, value)
#define ONCE_PREF(name, base_id, full_id, cpp_type, value) \
  InitPref_##cpp_type(name, value);
#include "mozilla/StaticPrefListAll.h"
#undef NEVER_PREF
#undef ALWAYS_PREF
#undef ONCE_PREF
  }
#define NEVER_PREF(name, cpp_type, value)
#define ALWAYS_PREF(name, base_id, full_id, cpp_type, value)          \
  InitAlwaysPref(NS_LITERAL_CSTRING(name), &sMirror_##full_id, value, \
                 aIsStartup, isParent);
#define ONCE_PREF(name, base_id, full_id, cpp_type, value)
#include "mozilla/StaticPrefListAll.h"
#undef NEVER_PREF
#undef ALWAYS_PREF
#undef ONCE_PREF
}

static void InitOncePrefs() {
  // For `once`-mirrored prefs we generate some initialization code. This is
  // done in case the pref value was updated when reading pref data files. It's
  // necessary because we don't have callbacks registered for `once`-mirrored
  // prefs.
  //
  // In debug builds, we also install a mechanism that can check if the
  // preference value is modified after `once`-mirrored prefs are initialized.
  // In tests this would indicate a likely misuse of a `once`-mirrored pref and
  // suggest that it should instead be `always`-mirrored.
#define NEVER_PREF(name, cpp_type, value)
#define ALWAYS_PREF(name, base_id, full_id, cpp_type, value)
#ifdef DEBUG
#  define ONCE_PREF(name, base_id, full_id, cpp_type, value)                   \
    {                                                                          \
      MOZ_ASSERT(gOnceStaticPrefsAntiFootgun);                                 \
      sMirror_##full_id = Internals::GetPref(name, cpp_type(value));           \
      auto checkPref = [&]() {                                                 \
        MOZ_ASSERT(sOncePrefRead);                                             \
        cpp_type staticPrefValue = full_id();                                  \
        cpp_type preferenceValue =                                             \
            Internals::GetPref(GetPrefName_##base_id(), cpp_type(value));      \
        MOZ_ASSERT(staticPrefValue == preferenceValue,                         \
                   "Preference '" name                                         \
                   "' got modified since StaticPrefs::" #full_id               \
                   " was initialized. Consider using an `always` mirror kind " \
                   "instead");                                                 \
      };                                                                       \
      gOnceStaticPrefsAntiFootgun->insert(                                     \
          std::pair<const char*, AntiFootgunCallback>(GetPrefName_##base_id(), \
                                                      std::move(checkPref)));  \
    }
#else
#  define ONCE_PREF(name, base_id, full_id, cpp_type, value) \
    sMirror_##full_id = Internals::GetPref(name, cpp_type(value));
#endif

#include "mozilla/StaticPrefListAll.h"
#undef NEVER_PREF
#undef ALWAYS_PREF
#undef ONCE_PREF
}

}  // namespace StaticPrefs

static MOZ_MAYBE_UNUSED void SaveOncePrefToSharedMap(
    SharedPrefMapBuilder& aBuilder, const char* aName, bool aValue) {
  auto oncePref = MakeUnique<Pref>(aName);
  oncePref->SetType(PrefType::Bool);
  oncePref->SetIsSkippedByIteration(true);
  bool valueChanged = false;
  MOZ_ALWAYS_SUCCEEDS(
      oncePref->SetDefaultValue(PrefType::Bool, PrefValue(aValue),
                                /* isSticky */ true,
                                /* isLocked */ true, &valueChanged));
  oncePref->AddToMap(aBuilder);
}

static MOZ_MAYBE_UNUSED void SaveOncePrefToSharedMap(
    SharedPrefMapBuilder& aBuilder, const char* aName, int32_t aValue) {
  auto oncePref = MakeUnique<Pref>(aName);
  oncePref->SetType(PrefType::Int);
  oncePref->SetIsSkippedByIteration(true);
  bool valueChanged = false;
  MOZ_ALWAYS_SUCCEEDS(
      oncePref->SetDefaultValue(PrefType::Int, PrefValue(aValue),
                                /* isSticky */ true,
                                /* isLocked */ true, &valueChanged));
  oncePref->AddToMap(aBuilder);
}

static MOZ_MAYBE_UNUSED void SaveOncePrefToSharedMap(
    SharedPrefMapBuilder& aBuilder, const char* aName, uint32_t aValue) {
  SaveOncePrefToSharedMap(aBuilder, aName, int32_t(aValue));
}

static MOZ_MAYBE_UNUSED void SaveOncePrefToSharedMap(
    SharedPrefMapBuilder& aBuilder, const char* aName, float aValue) {
  auto oncePref = MakeUnique<Pref>(aName);
  oncePref->SetType(PrefType::String);
  oncePref->SetIsSkippedByIteration(true);
  nsAutoCString value;
  value.AppendFloat(aValue);
  bool valueChanged = false;
  // It's ok to stash a pointer to the temporary PromiseFlatCString's chars in
  // pref because pref_SetPref() duplicates those chars.
  const nsCString& flat = PromiseFlatCString(value);
  MOZ_ALWAYS_SUCCEEDS(
      oncePref->SetDefaultValue(PrefType::String, PrefValue(flat.get()),
                                /* isSticky */ true,
                                /* isLocked */ true, &valueChanged));
  oncePref->AddToMap(aBuilder);
}

#define ONCE_PREF_NAME(name) ("$$$" name "$$$")

namespace StaticPrefs {

static void RegisterOncePrefs(SharedPrefMapBuilder& aBuilder) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_DIAGNOSTIC_ASSERT(!gSharedMap,
                        "Must be called before gSharedMap has been created");
  MaybeInitOncePrefs();

  // For `once`-mirrored prefs we generate a save call, which saves the value
  // as it was at parent startup. It is stored in a special (hidden and locked)
  // entry in the global SharedPreferenceMap. In order for the entry to be
  // hidden and not appear in about:config nor ever be stored to disk, we set
  // its IsSkippedByIteration flag to true. We also distinguish it by adding a
  // "$$$" prefix and suffix to the preference name.
#define NEVER_PREF(name, cpp_type, value)
#define ALWAYS_PREF(name, base_id, full_id, cpp_type, value)
#define ONCE_PREF(name, base_id, full_id, cpp_type, value) \
  SaveOncePrefToSharedMap(aBuilder, ONCE_PREF_NAME(name),  \
                          cpp_type(sMirror_##full_id));
#include "mozilla/StaticPrefListAll.h"
#undef NEVER_PREF
#undef ALWAYS_PREF
#undef ONCE_PREF
}

static void InitStaticPrefsFromShared() {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_DIAGNOSTIC_ASSERT(gSharedMap,
                        "Must be called once gSharedMap has been created");

  // For mirrored prefs we generate some initialization code.
#define NEVER_PREF(name, cpp_type, value)
#define ALWAYS_PREF(name, base_id, full_id, cpp_type, value) \
  {                                                          \
    StripAtomic<cpp_type> val;                               \
    nsresult rv = Internals::GetSharedPrefValue(name, &val); \
    MOZ_DIAGNOSTIC_ALWAYS_TRUE(NS_SUCCEEDED(rv));            \
    StaticPrefs::sMirror_##full_id = val;                    \
  }
#define ONCE_PREF(name, base_id, full_id, cpp_type, value)                   \
  {                                                                          \
    cpp_type val;                                                            \
    nsresult rv = Internals::GetSharedPrefValue(ONCE_PREF_NAME(name), &val); \
    MOZ_DIAGNOSTIC_ALWAYS_TRUE(NS_SUCCEEDED(rv));                            \
    StaticPrefs::sMirror_##full_id = val;                                    \
  }
#include "mozilla/StaticPrefListAll.h"
#undef NEVER_PREF
#undef ALWAYS_PREF
#undef ONCE_PREF

  // `once`-mirrored prefs have been set to their value in the step above and
  // outside the parent process they are immutable. We set sOncePrefRead so
  // that we can directly skip any lazy initializations.
  sOncePrefRead = true;
}

}  // namespace StaticPrefs

}  // namespace mozilla

#undef ENSURE_PARENT_PROCESS

//===========================================================================
// Module and factory stuff
//===========================================================================

NS_IMPL_COMPONENT_FACTORY(nsPrefLocalizedString) {
  auto str = MakeRefPtr<nsPrefLocalizedString>();
  if (NS_SUCCEEDED(str->Init())) {
    return str.forget().downcast<nsISupports>();
  }
  return nullptr;
}

namespace mozilla {

void UnloadPrefsModule() { Preferences::Shutdown(); }

}  // namespace mozilla

// This file contains the C wrappers for the C++ static pref getters, as used
// by Rust code.
#include "init/StaticPrefsCGetters.cpp"
