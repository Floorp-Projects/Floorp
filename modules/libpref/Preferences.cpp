/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/HashFunctions.h"
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
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsClassHashtable.h"
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
#include "nsWeakReference.h"
#include "nsXPCOMCID.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "nsZipArchive.h"
#include "PLDHashTable.h"
#include "prefapi.h"
#include "prefapi_private_data.h"
#include "prefread.h"

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#endif

using namespace mozilla;

#ifdef DEBUG

#define ENSURE_MAIN_PROCESS(func, pref)                                        \
  do {                                                                         \
    if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                \
      nsPrintfCString msg(                                                     \
        "ENSURE_MAIN_PROCESS: called %s on %s in a non-main process",          \
        func, pref);                                                           \
      NS_ERROR(msg.get());                                                     \
      return NS_ERROR_NOT_AVAILABLE;                                           \
    }                                                                          \
  } while (0)

#define ENSURE_MAIN_PROCESS_WITH_WARNING(func, pref)                           \
  do {                                                                         \
    if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                \
      nsPrintfCString msg(                                                     \
        "ENSURE_MAIN_PROCESS: called %s on %s in a non-main process",          \
        func, pref);                                                           \
      NS_WARNING(msg.get());                                                   \
      return NS_ERROR_NOT_AVAILABLE;                                           \
    }                                                                          \
  } while (0)

class WatchinPrefRAII
{
public:
  WatchinPrefRAII() { pref_SetWatchingPref(true); }
  ~WatchinPrefRAII() { pref_SetWatchingPref(false); }
};

#define WATCHING_PREF_RAII() WatchinPrefRAII watchingPrefRAII

#else // DEBUG

#define ENSURE_MAIN_PROCESS(func, pref)                                        \
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                  \
    return NS_ERROR_NOT_AVAILABLE;                                             \
  }

#define ENSURE_MAIN_PROCESS_WITH_WARNING(func, pref)                           \
  if (MOZ_UNLIKELY(!XRE_IsParentProcess())) {                                  \
    return NS_ERROR_NOT_AVAILABLE;                                             \
  }

#define WATCHING_PREF_RAII()

#endif // DEBUG

//===========================================================================
// nsPrefBranch et al.
//===========================================================================

using mozilla::dom::ContentChild;

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

  nsresult RemoveObserverFromMap(const char* aDomain, nsISupports* aObserver);

  static void NotifyObserver(const char* aNewpref, void* aData);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  static void ReportToConsole(const nsAString& aMessage);

protected:
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
  nsresult SetCharPrefInternal(const char* aPrefName, const char* aValue);

  // Reject strings that are more than 1Mb, warn if strings are more than 16kb.
  nsresult CheckSanityOfStringLength(const char* aPrefName,
                                     const nsAString& aValue);
  nsresult CheckSanityOfStringLength(const char* aPrefName,
                                     const nsACString& aValue);
  nsresult CheckSanityOfStringLength(const char* aPrefName, const char* aValue);
  nsresult CheckSanityOfStringLength(const char* aPrefName,
                                     const uint32_t aLength);

  void RemoveExpiredCallback(PrefCallback* aCallback);

  PrefName GetPrefName(const char* aPrefName) const;

  void FreeObserverList(void);

private:
  const nsCString mPrefRoot;
  bool mIsDefault;

  bool mFreeingObserverList;
  nsClassHashtable<PrefCallback, PrefCallback> mObservers;
};

class nsPrefLocalizedString final
  : public nsIPrefLocalizedString
  , public nsISupportsString
{
public:
  nsPrefLocalizedString();

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSISUPPORTSSTRING(mUnicodeString->)
  NS_FORWARD_NSISUPPORTSPRIMITIVE(mUnicodeString->)

  nsresult Init();

private:
  virtual ~nsPrefLocalizedString();

  NS_IMETHOD GetData(char16_t**) override;
  NS_IMETHOD SetData(const char16_t* aData) override;
  NS_IMETHOD SetDataWithLength(uint32_t aLength,
                               const char16_t* aData) override;

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

static ContentChild*
GetContentChild()
{
  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    if (!cpc) {
      MOZ_CRASH("Content Protocol is NULL!  We're going to crash!");
    }
    return cpc;
  }
  return nullptr;
}

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
nsPrefBranch::GetRoot(char** aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);

  *aRoot = ToNewCString(mPrefRoot);
  return NS_OK;
}

NS_IMETHODIMP
nsPrefBranch::GetPrefType(const char* aPrefName, int32_t* aRetVal)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = GetPrefName(aPrefName);
  switch (PREF_GetPrefType(pref.get())) {
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
  nsresult rv = GetCharPref(aPrefName, getter_Copies(stringVal));
  if (NS_SUCCEEDED(rv)) {
    *aRetVal = stringVal.ToFloat(&rv);
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetCharPrefWithDefault(const char* aPrefName,
                                     const char* aDefaultValue,
                                     uint8_t aArgc,
                                     char** aRetVal)
{
  nsresult rv = GetCharPref(aPrefName, aRetVal);

  if (NS_FAILED(rv) && aArgc == 1) {
    NS_ENSURE_ARG(aDefaultValue);
    *aRetVal = NS_strdup(aDefaultValue);
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsPrefBranch::GetCharPref(const char* aPrefName, char** aRetVal)
{
  NS_ENSURE_ARG(aPrefName);
  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_CopyCharPref(pref.get(), aRetVal, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::SetCharPref(const char* aPrefName, const char* aValue)
{
  nsresult rv = CheckSanityOfStringLength(aPrefName, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return SetCharPrefInternal(aPrefName, aValue);
}

nsresult
nsPrefBranch::SetCharPrefInternal(const char* aPrefName, const char* aValue)

{
  ENSURE_MAIN_PROCESS("SetCharPref", aPrefName);
  NS_ENSURE_ARG(aPrefName);
  NS_ENSURE_ARG(aValue);

  const PrefName& pref = GetPrefName(aPrefName);
  return PREF_SetCharPref(pref.get(), aValue, mIsDefault);
}

NS_IMETHODIMP
nsPrefBranch::GetStringPref(const char* aPrefName,
                            const nsACString& aDefaultValue,
                            uint8_t aArgc,
                            nsACString& aRetVal)
{
  nsCString utf8String;
  nsresult rv = GetCharPref(aPrefName, getter_Copies(utf8String));
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

  return SetCharPrefInternal(aPrefName, PromiseFlatCString(aValue).get());
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
  nsCString utf8String;

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
        theString->SetData(utf16String.get());
      }
    } else {
      rv = GetCharPref(aPrefName, getter_Copies(utf8String));
      if (NS_SUCCEEDED(rv)) {
        theString->SetData(NS_ConvertUTF8toUTF16(utf8String).get());
      }
    }

    if (NS_SUCCEEDED(rv)) {
      theString.forget(reinterpret_cast<nsIPrefLocalizedString**>(aRetVal));
    }

    return rv;
  }

  // if we can't get the pref, there's no point in being here
  rv = GetCharPref(aPrefName, getter_Copies(utf8String));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIFile))) {
    if (GetContentChild()) {
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
    if (GetContentChild()) {
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
                                        const char* aValue)
{
  if (!aValue) {
    return NS_OK;
  }
  return CheckSanityOfStringLength(aPrefName, strlen(aValue));
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
      rv = SetCharPrefInternal(aPrefName, descriptorString.get());
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
    return SetCharPrefInternal(aPrefName, descriptorString.get());
  }

  if (aType.Equals(NS_GET_IID(nsISupportsString))) {
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
        rv = SetCharPrefInternal(aPrefName,
                                 NS_ConvertUTF16toUTF8(wideString).get());
      }
    }
    return rv;
  }

  if (aType.Equals(NS_GET_IID(nsIPrefLocalizedString))) {
    nsCOMPtr<nsIPrefLocalizedString> theString = do_QueryInterface(aValue);

    if (theString) {
      nsString wideString;
      rv = theString->GetData(getter_Copies(wideString));
      if (NS_SUCCEEDED(rv)) {
        // Check sanity of string length before any lengthy conversion
        rv = CheckSanityOfStringLength(aPrefName, wideString);
        if (NS_FAILED(rv)) {
          return rv;
        }
        rv = SetCharPrefInternal(aPrefName,
                                 NS_ConvertUTF16toUTF8(wideString).get());
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

  const PrefName& pref = GetPrefName(aStartingAt);
  return PREF_DeleteBranch(pref.get());
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
    if (!outArray) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

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
  PREF_RegisterCallback(pref.get(), NotifyObserver, pCallback);
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

  nsCString propertyFileURL;
  nsresult rv =
    PREF_CopyCharPref(aPrefName, getter_Copies(propertyFileURL), true);
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

NS_IMETHODIMP
nsPrefLocalizedString::GetData(char16_t** aRetVal)
{
  nsAutoString data;

  nsresult rv = GetData(data);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aRetVal = ToNewUnicode(data);
  if (!*aRetVal) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrefLocalizedString::SetData(const char16_t* aData)
{
  if (!aData) {
    return SetData(EmptyString());
  }
  return SetData(nsDependentString(aData));
}

NS_IMETHODIMP
nsPrefLocalizedString::SetDataWithLength(uint32_t aLength,
                                         const char16_t* aData)
{
  if (!aData) {
    return SetData(EmptyString());
  }
  return SetData(Substring(aData, aLength));
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

class PrefCallback;

namespace mozilla {

#define INITIAL_PREF_FILES 10

static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

void
Preferences::DirtyCallback()
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

static nsresult
ReadExtensionPrefs(nsIFile* aFile);

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
      getter_AddRefs(outStream), outStreamSink, 4096);
    if (NS_FAILED(rv)) {
      return rv;
    }

    struct CharComparator
    {
      bool LessThan(const mozilla::UniqueFreePtr<char>& a,
                    const mozilla::UniqueFreePtr<char>& b) const
      {
        return strcmp(a.get(), b.get()) < 0;
      }

      bool Equals(const mozilla::UniqueFreePtr<char>& a,
                  const mozilla::UniqueFreePtr<char>& b) const
      {
        return strcmp(a.get(), b.get()) == 0;
      }
    };

    // Sort the preferences to make a readable file on disk.
    aPrefs.Sort(CharComparator());

    // Write out the file header.
    outStream->Write(
      kPrefFileHeader, sizeof(kPrefFileHeader) - 1, &writeAmount);

    for (auto& prefptr : aPrefs) {
      char* pref = prefptr.get();
      MOZ_ASSERT(pref);
      outStream->Write(pref, strlen(pref), &writeAmount);
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
                                   Preferences::DirtyCallback();
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

/* static */ Preferences*
Preferences::GetInstanceForService()
{
  if (sPreferences) {
    NS_ADDREF(sPreferences);
    return sPreferences;
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

  NS_ADDREF(sPreferences);
  return sPreferences;
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

InfallibleTArray<Preferences::PrefSetting>* gInitPrefs;

/* static */ void
Preferences::SetInitPreferences(nsTArray<PrefSetting>* aPrefs)
{
  gInitPrefs = new InfallibleTArray<PrefSetting>(mozilla::Move(*aPrefs));
}

Result<Ok, const char*>
Preferences::Init()
{
  PREF_SetDirtyCallback(&DirtyCallback);
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

  nsCString lockFileName;

  // The following is a small hack which will allow us to only load the library
  // which supports the netscape.cfg file if the preference is defined. We
  // test for the existence of the pref, set in the all.js (mozilla) or
  // all-ns.js (netscape 6), and if it exists we startup the pref config
  // category which will do the rest.

  nsresult rv = PREF_CopyCharPref(
    "general.config.filename", getter_Copies(lockFileName), false);
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
Preferences::SetInitPhase(pref_initPhase phase)
{
  pref_SetInitPhase(phase);
}

pref_initPhase
Preferences::InitPhase()
{
  return pref_GetInitPhase();
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
    // DirtyCallback can still be pending.
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
      MakeUnique<PrefSaveData>(pref_savePrefs(gHashTable));

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
  PrefSaveData prefsData = pref_savePrefs(gHashTable);
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
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), false);
  if (NS_SUCCEEDED(rv)) {
    *aResult = result.ToFloat(&rv);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetCString(const char* aPref, nsACString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  char* result;
  nsresult rv = PREF_CopyCharPref(aPref, &result, false);
  if (NS_SUCCEEDED(rv)) {
    aResult.Adopt(result);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetString(const char* aPref, nsAString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), false);
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
    prefLocalString->GetData(getter_Copies(aResult));
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
  return PREF_SetCharPref(aPref, aValue, false);
}

/* static */ nsresult
Preferences::SetCString(const char* aPref, const nsACString& aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetCString", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCharPref(aPref, PromiseFlatCString(aValue).get(), false);
}

/* static */ nsresult
Preferences::SetString(const char* aPref, const char16ptr_t aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetString", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCharPref(aPref, NS_ConvertUTF16toUTF8(aValue).get(), false);
}

/* static */ nsresult
Preferences::SetString(const char* aPref, const nsAString& aValue)
{
  ENSURE_MAIN_PROCESS_WITH_WARNING("SetString", aPref);
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  return PREF_SetCharPref(aPref, NS_ConvertUTF16toUTF8(aValue).get(), false);
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
  PREF_RegisterPriorityCallback(
    aPref, NotifyObserver, static_cast<nsIObserver*>(observer));
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
  char* result;
  nsresult rv = PREF_CopyCharPref(aPref, &result, true);
  if (NS_SUCCEEDED(rv)) {
    aResult.Adopt(result);
  }
  return rv;
}

/* static */ nsresult
Preferences::GetDefaultString(const char* aPref, nsAString& aResult)
{
  NS_ENSURE_TRUE(InitStaticMembers(), NS_ERROR_NOT_AVAILABLE);
  nsAutoCString result;
  nsresult rv = PREF_CopyCharPref(aPref, getter_Copies(result), true);
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
    prefLocalString->GetData(getter_Copies(aResult));
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
