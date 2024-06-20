/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Documentation for libpref is in modules/libpref/docs/index.rst.

#ifndef mozilla_Preferences_h
#define mozilla_Preferences_h

#ifndef MOZILLA_INTERNAL_API
#  error "This header is only usable from within libxul (MOZILLA_INTERNAL_API)."
#endif

#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWeakReference.h"
#include "nsXULAppAPI.h"
#include <atomic>
#include <functional>

class nsIFile;

// The callback function will get passed the pref name which triggered the call
// and the void* data which was passed to the registered callback function.
typedef void (*PrefChangedFunc)(const char* aPref, void* aData);

class nsPrefBranch;

namespace mozilla {

struct RegisterCallbacksInternal;

void UnloadPrefsModule();

class PreferenceServiceReporter;

namespace dom {
class Pref;
class PrefValue;
}  // namespace dom

namespace ipc {
class FileDescriptor;
}  // namespace ipc

struct PrefsSizes;

// Xlib.h defines Bool as a macro constant. Don't try to define this enum if
// it's already been included.
#ifndef Bool

// Keep this in sync with PrefType in parser/src/lib.rs.
enum class PrefType : uint8_t {
  None = 0,  // only used when neither the default nor user value is set
  String = 1,
  Int = 2,
  Bool = 3,
};

#endif

#ifdef XP_UNIX
// We need to send two shared memory descriptors to every child process:
//
// 1) A read-only/write-protected snapshot of the initial state of the
//    preference database. This memory is shared between all processes, and
//    therefore cannot be modified once it has been created.
//
// 2) A set of changes on top of the snapshot, containing the current values of
//    all preferences which have changed since it was created.
//
// Since the second set will be different for every process, and the first set
// cannot be modified, it is unfortunately not possible to combine them into a
// single file descriptor.
//
// XXX: bug 1440207 is about improving how fixed fds such as this are used.
static const int kPrefsFileDescriptor = 8;
static const int kPrefMapFileDescriptor = 9;
#endif

// Keep this in sync with PrefType in parser/src/lib.rs.
enum class PrefValueKind : uint8_t { Default, User };

class Preferences final : public nsIPrefService,
                          public nsIObserver,
                          public nsIPrefBranch,
                          public nsSupportsWeakReference {
  friend class ::nsPrefBranch;

 public:
  using WritePrefFilePromise = MozPromise<bool, nsresult, false>;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPREFSERVICE
  NS_FORWARD_NSIPREFBRANCH(mRootBranch->)
  NS_DECL_NSIOBSERVER

  Preferences();

  // Returns true if the Preferences service is available, false otherwise.
  static bool IsServiceAvailable();

  // Initialize user prefs from prefs.js/user.js
  static void InitializeUserPrefs();
  static void FinishInitializingUserPrefs();

  // Returns the singleton instance which is addreffed.
  static already_AddRefed<Preferences> GetInstanceForService();

  // Finallizes global members.
  static void Shutdown();

  // Returns shared pref service instance NOTE: not addreffed.
  static nsIPrefService* GetService() {
    NS_ENSURE_TRUE(InitStaticMembers(), nullptr);
    return sPreferences;
  }

  // Returns shared pref branch instance. NOTE: not addreffed.
  static nsIPrefBranch* GetRootBranch(
      PrefValueKind aKind = PrefValueKind::User) {
    NS_ENSURE_TRUE(InitStaticMembers(), nullptr);
    return (aKind == PrefValueKind::Default) ? sPreferences->mDefaultRootBranch
                                             : sPreferences->mRootBranch;
  }

  // Gets the type of the pref.
  static int32_t GetType(const char* aPrefName);

  // Fallible value getters. When `aKind` is `User` they will get the user
  // value if possible, and fall back to the default value otherwise.
  static nsresult GetBool(const char* aPrefName, bool* aResult,
                          PrefValueKind aKind = PrefValueKind::User);
  static nsresult GetInt(const char* aPrefName, int32_t* aResult,
                         PrefValueKind aKind = PrefValueKind::User);
  static nsresult GetUint(const char* aPrefName, uint32_t* aResult,
                          PrefValueKind aKind = PrefValueKind::User) {
    return GetInt(aPrefName, reinterpret_cast<int32_t*>(aResult), aKind);
  }
  static nsresult GetFloat(const char* aPrefName, float* aResult,
                           PrefValueKind aKind = PrefValueKind::User);
  static nsresult GetCString(const char* aPrefName, nsACString& aResult,
                             PrefValueKind aKind = PrefValueKind::User);
  static nsresult GetString(const char* aPrefName, nsAString& aResult,
                            PrefValueKind aKind = PrefValueKind::User);
  static nsresult GetLocalizedCString(
      const char* aPrefName, nsACString& aResult,
      PrefValueKind aKind = PrefValueKind::User);
  static nsresult GetLocalizedString(const char* aPrefName, nsAString& aResult,
                                     PrefValueKind aKind = PrefValueKind::User);
  static nsresult GetComplex(const char* aPrefName, const nsIID& aType,
                             void** aResult,
                             PrefValueKind aKind = PrefValueKind::User);

  // Infallible getters of user or default values, with fallback results on
  // failure. When `aKind` is `User` they will get the user value if possible,
  // and fall back to the default value otherwise.
  static bool GetBool(const char* aPrefName, bool aFallback = false,
                      PrefValueKind aKind = PrefValueKind::User);
  static int32_t GetInt(const char* aPrefName, int32_t aFallback = 0,
                        PrefValueKind aKind = PrefValueKind::User);
  static uint32_t GetUint(const char* aPrefName, uint32_t aFallback = 0,
                          PrefValueKind aKind = PrefValueKind::User);
  static float GetFloat(const char* aPrefName, float aFallback = 0.0f,
                        PrefValueKind aKind = PrefValueKind::User);

  // Value setters. These fail if run outside the parent process.

  static nsresult SetBool(const char* aPrefName, bool aValue,
                          PrefValueKind aKind = PrefValueKind::User);
  static nsresult SetInt(const char* aPrefName, int32_t aValue,
                         PrefValueKind aKind = PrefValueKind::User);
  static nsresult SetCString(const char* aPrefName, const nsACString& aValue,
                             PrefValueKind aKind = PrefValueKind::User);

  static nsresult SetUint(const char* aPrefName, uint32_t aValue,
                          PrefValueKind aKind = PrefValueKind::User) {
    return SetInt(aPrefName, static_cast<int32_t>(aValue), aKind);
  }

  static nsresult SetFloat(const char* aPrefName, float aValue,
                           PrefValueKind aKind = PrefValueKind::User) {
    nsAutoCString value;
    value.AppendFloat(aValue);
    return SetCString(aPrefName, value, aKind);
  }

  static nsresult SetCString(const char* aPrefName, const char* aValue,
                             PrefValueKind aKind = PrefValueKind::User) {
    return Preferences::SetCString(aPrefName, nsDependentCString(aValue),
                                   aKind);
  }

  static nsresult SetString(const char* aPrefName, const char16ptr_t aValue,
                            PrefValueKind aKind = PrefValueKind::User) {
    return Preferences::SetCString(aPrefName, NS_ConvertUTF16toUTF8(aValue),
                                   aKind);
  }

  static nsresult SetString(const char* aPrefName, const nsAString& aValue,
                            PrefValueKind aKind = PrefValueKind::User) {
    return Preferences::SetCString(aPrefName, NS_ConvertUTF16toUTF8(aValue),
                                   aKind);
  }

  static nsresult SetComplex(const char* aPrefName, const nsIID& aType,
                             nsISupports* aValue,
                             PrefValueKind aKind = PrefValueKind::User);

  static nsresult Lock(const char* aPrefName);
  static nsresult Unlock(const char* aPrefName);
  static bool IsLocked(const char* aPrefName);
  static bool IsSanitized(const char* aPrefName);

  // Clears user set pref. Fails if run outside the parent process.
  static nsresult ClearUser(const char* aPrefName);

  // Whether the pref has a user value or not.
  static bool HasUserValue(const char* aPref);

  // Whether the pref has a user value or not.
  static bool HasDefaultValue(const char* aPref);

  // Adds/Removes the observer for the root pref branch. See nsIPrefBranch.idl
  // for details.
  static nsresult AddStrongObserver(nsIObserver* aObserver,
                                    const nsACString& aPref);
  static nsresult AddWeakObserver(nsIObserver* aObserver,
                                  const nsACString& aPref);
  static nsresult RemoveObserver(nsIObserver* aObserver,
                                 const nsACString& aPref);

  template <int N>
  static nsresult AddStrongObserver(nsIObserver* aObserver,
                                    const char (&aPref)[N]) {
    return AddStrongObserver(aObserver, nsLiteralCString(aPref));
  }
  template <int N>
  static nsresult AddWeakObserver(nsIObserver* aObserver,
                                  const char (&aPref)[N]) {
    return AddWeakObserver(aObserver, nsLiteralCString(aPref));
  }
  template <int N>
  static nsresult RemoveObserver(nsIObserver* aObserver,
                                 const char (&aPref)[N]) {
    return RemoveObserver(aObserver, nsLiteralCString(aPref));
  }

  // Adds/Removes two or more observers for the root pref branch. Pass to
  // aPrefs an array of const char* whose last item is nullptr.
  // Note: All preference strings *must* be statically-allocated string
  // literals.
  static nsresult AddStrongObservers(nsIObserver* aObserver,
                                     const char* const* aPrefs);
  static nsresult AddWeakObservers(nsIObserver* aObserver,
                                   const char* const* aPrefs);
  static nsresult RemoveObservers(nsIObserver* aObserver,
                                  const char* const* aPrefs);

  // Registers/Unregisters the callback function for the aPref.
  template <typename T = void>
  static nsresult RegisterCallback(PrefChangedFunc aCallback,
                                   const nsACString& aPref,
                                   T* aClosure = nullptr) {
    return RegisterCallback(aCallback, aPref, aClosure, ExactMatch);
  }

  template <typename T = void>
  static nsresult UnregisterCallback(PrefChangedFunc aCallback,
                                     const nsACString& aPref,
                                     T* aClosure = nullptr) {
    return UnregisterCallback(aCallback, aPref, aClosure, ExactMatch);
  }

  // Like RegisterCallback, but also calls the callback immediately for
  // initialization.
  template <typename T = void>
  static nsresult RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                          const nsACString& aPref,
                                          T* aClosure = nullptr) {
    return RegisterCallbackAndCall(aCallback, aPref, aClosure, ExactMatch);
  }

  // Like RegisterCallback, but registers a callback for a prefix of multiple
  // pref names, not a single pref name.
  template <typename T = void>
  static nsresult RegisterPrefixCallback(PrefChangedFunc aCallback,
                                         const nsACString& aPref,
                                         T* aClosure = nullptr) {
    return RegisterCallback(aCallback, aPref, aClosure, PrefixMatch);
  }

  // Like RegisterPrefixCallback, but also calls the callback immediately for
  // initialization.
  template <typename T = void>
  static nsresult RegisterPrefixCallbackAndCall(PrefChangedFunc aCallback,
                                                const nsACString& aPref,
                                                T* aClosure = nullptr) {
    return RegisterCallbackAndCall(aCallback, aPref, aClosure, PrefixMatch);
  }

  // Unregister a callback registered with RegisterPrefixCallback or
  // RegisterPrefixCallbackAndCall.
  template <typename T = void>
  static nsresult UnregisterPrefixCallback(PrefChangedFunc aCallback,
                                           const nsACString& aPref,
                                           T* aClosure = nullptr) {
    return UnregisterCallback(aCallback, aPref, aClosure, PrefixMatch);
  }

  // Variants of the above which register a single callback to handle multiple
  // preferences.
  //
  // The array of preference names must be null terminated. It may be
  // dynamically allocated, but the caller is responsible for keeping it alive
  // until the callback is unregistered.
  //
  // Also note that the exact same aPrefs pointer must be passed to the
  // Unregister call as was passed to the Register call.
  template <typename T = void>
  static nsresult RegisterCallbacks(PrefChangedFunc aCallback,
                                    const char* const* aPrefs,
                                    T* aClosure = nullptr) {
    return RegisterCallbacks(aCallback, aPrefs, aClosure, ExactMatch);
  }
  static nsresult RegisterCallbacksAndCall(PrefChangedFunc aCallback,
                                           const char* const* aPrefs,
                                           void* aClosure = nullptr);
  template <typename T = void>
  static nsresult UnregisterCallbacks(PrefChangedFunc aCallback,
                                      const char* const* aPrefs,
                                      T* aClosure = nullptr) {
    return UnregisterCallbacks(aCallback, aPrefs, aClosure, ExactMatch);
  }
  template <typename T = void>
  static nsresult RegisterPrefixCallbacks(PrefChangedFunc aCallback,
                                          const char* const* aPrefs,
                                          T* aClosure = nullptr) {
    return RegisterCallbacks(aCallback, aPrefs, aClosure, PrefixMatch);
  }
  template <typename T = void>
  static nsresult UnregisterPrefixCallbacks(PrefChangedFunc aCallback,
                                            const char* const* aPrefs,
                                            T* aClosure = nullptr) {
    return UnregisterCallbacks(aCallback, aPrefs, aClosure, PrefixMatch);
  }

  template <int N, typename T = void>
  static nsresult RegisterCallback(PrefChangedFunc aCallback,
                                   const char (&aPref)[N],
                                   T* aClosure = nullptr) {
    return RegisterCallback(aCallback, nsLiteralCString(aPref), aClosure,
                            ExactMatch);
  }

  template <int N, typename T = void>
  static nsresult UnregisterCallback(PrefChangedFunc aCallback,
                                     const char (&aPref)[N],
                                     T* aClosure = nullptr) {
    return UnregisterCallback(aCallback, nsLiteralCString(aPref), aClosure,
                              ExactMatch);
  }

  template <int N, typename T = void>
  static nsresult RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                          const char (&aPref)[N],
                                          T* aClosure = nullptr) {
    return RegisterCallbackAndCall(aCallback, nsLiteralCString(aPref), aClosure,
                                   ExactMatch);
  }

  template <int N, typename T = void>
  static nsresult RegisterPrefixCallback(PrefChangedFunc aCallback,
                                         const char (&aPref)[N],
                                         T* aClosure = nullptr) {
    return RegisterCallback(aCallback, nsLiteralCString(aPref), aClosure,
                            PrefixMatch);
  }

  template <int N, typename T = void>
  static nsresult RegisterPrefixCallbackAndCall(PrefChangedFunc aCallback,
                                                const char (&aPref)[N],
                                                T* aClosure = nullptr) {
    return RegisterCallbackAndCall(aCallback, nsLiteralCString(aPref), aClosure,
                                   PrefixMatch);
  }

  template <int N, typename T = void>
  static nsresult UnregisterPrefixCallback(PrefChangedFunc aCallback,
                                           const char (&aPref)[N],
                                           T* aClosure = nullptr) {
    return UnregisterCallback(aCallback, nsLiteralCString(aPref), aClosure,
                              PrefixMatch);
  }

  // When a content process is created these methods are used to pass changed
  // prefs in bulk from the parent process, via shared memory.
  static void SerializePreferences(nsCString& aStr,
                                   bool aIsDestinationWebContentProcess);
  static void DeserializePreferences(char* aStr, size_t aPrefsLen);

  static mozilla::ipc::FileDescriptor EnsureSnapshot(size_t* aSize);
  static void InitSnapshot(const mozilla::ipc::FileDescriptor&, size_t aSize);

  // When a single pref is changed in the parent process, these methods are
  // used to pass the update to content processes.
  static void GetPreference(dom::Pref* aPref,
                            const GeckoProcessType aDestinationProcessType,
                            const nsACString& aDestinationRemoteType);
  static void SetPreference(const dom::Pref& aPref);

#ifdef DEBUG
  static bool ArePrefsInitedInContentProcess();
#endif

  static void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                     PrefsSizes& aSizes);

  static void HandleDirty();

  // Explicitly choosing synchronous or asynchronous (if allowed) preferences
  // file write. Only for the default file.  The guarantee for the "blocking"
  // is that when it returns, the file on disk reflect the current state of
  // preferences.
  nsresult SavePrefFileBlocking();
  nsresult SavePrefFileAsynchronous();

  // If this is false, only blocking writes, on main thread are allowed.
  bool AllowOffMainThreadSave();

 private:
  virtual ~Preferences();

  nsresult NotifyServiceObservers(const char* aSubject);

  // Loads the prefs.js file from the profile, or creates a new one. Returns
  // the prefs file if successful, or nullptr on failure.
  already_AddRefed<nsIFile> ReadSavedPrefs();

  // Loads the user.js file from the profile if present.
  void ReadUserOverridePrefs();

  nsresult MakeBackupPrefFile(nsIFile* aFile);

  // Default pref file save can be blocking or not.
  enum class SaveMethod { Blocking, Asynchronous };

  // Off main thread is only respected for the default aFile value (nullptr).
  nsresult SavePrefFileInternal(nsIFile* aFile, SaveMethod aSaveMethod);
  nsresult WritePrefFile(
      nsIFile* aFile, SaveMethod aSaveMethod,
      UniquePtr<MozPromiseHolder<WritePrefFilePromise>> aPromise = nullptr);

  nsresult ResetUserPrefs();

  // Helpers for implementing
  // Register(Prefix)Callback/Unregister(Prefix)Callback.
 public:
  // Public so the ValueObserver classes can use it.
  enum MatchKind {
    PrefixMatch,
    ExactMatch,
  };

 private:
  static void SetupTelemetryPref();
  static nsresult InitInitialObjects(bool aIsStartup);

  friend struct Internals;

  static nsresult RegisterCallback(PrefChangedFunc aCallback,
                                   const nsACString& aPref, void* aClosure,
                                   MatchKind aMatchKind,
                                   bool aIsPriority = false);
  static nsresult UnregisterCallback(PrefChangedFunc aCallback,
                                     const nsACString& aPref, void* aClosure,
                                     MatchKind aMatchKind);
  static nsresult RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                          const nsACString& aPref,
                                          void* aClosure, MatchKind aMatchKind);

  static nsresult RegisterCallbacks(PrefChangedFunc aCallback,
                                    const char* const* aPrefs, void* aClosure,
                                    MatchKind aMatchKind);
  static nsresult UnregisterCallbacks(PrefChangedFunc aCallback,
                                      const char* const* aPrefs, void* aClosure,
                                      MatchKind aMatchKind);

  template <typename T>
  static nsresult RegisterCallbackImpl(PrefChangedFunc aCallback, T& aPref,
                                       void* aClosure, MatchKind aMatchKind,
                                       bool aIsPriority = false);
  template <typename T>
  static nsresult UnregisterCallbackImpl(PrefChangedFunc aCallback, T& aPref,
                                         void* aClosure, MatchKind aMatchKind);

  static nsresult RegisterCallback(PrefChangedFunc aCallback, const char* aPref,
                                   void* aClosure, MatchKind aMatchKind,
                                   bool aIsPriority = false) {
    return RegisterCallback(aCallback, nsDependentCString(aPref), aClosure,
                            aMatchKind, aIsPriority);
  }
  static nsresult UnregisterCallback(PrefChangedFunc aCallback,
                                     const char* aPref, void* aClosure,
                                     MatchKind aMatchKind) {
    return UnregisterCallback(aCallback, nsDependentCString(aPref), aClosure,
                              aMatchKind);
  }
  static nsresult RegisterCallbackAndCall(PrefChangedFunc aCallback,
                                          const char* aPref, void* aClosure,
                                          MatchKind aMatchKind) {
    return RegisterCallbackAndCall(aCallback, nsDependentCString(aPref),
                                   aClosure, aMatchKind);
  }

 private:
  nsCOMPtr<nsIFile> mCurrentFile;
  // Time since unix epoch in ms (JS Date compatible)
  PRTime mUserPrefsFileLastModifiedAtStartup = 0;
  bool mDirty = false;
  bool mProfileShutdown = false;
  // We wait a bit after prefs are dirty before writing them. In this period,
  // mDirty and mSavePending will both be true.
  bool mSavePending = false;

  nsCOMPtr<nsIPrefBranch> mRootBranch;
  nsCOMPtr<nsIPrefBranch> mDefaultRootBranch;

  static StaticRefPtr<Preferences> sPreferences;
  static bool sShutdown;

  // Init static members. Returns true on success.
  static bool InitStaticMembers();
};

extern Atomic<bool, Relaxed> sOmitBlocklistedPrefValues;
extern Atomic<bool, Relaxed> sCrashOnBlocklistedPref;

bool IsPreferenceSanitized(const char* aPref);

const char kFissionEnforceBlockList[] =
    "fission.enforceBlocklistedPrefsInSubprocesses";
const char kFissionOmitBlockListValues[] =
    "fission.omitBlocklistedPrefsInSubprocesses";

void OnFissionBlocklistPrefChange(const char* aPref, void* aData);

}  // namespace mozilla

#endif  // mozilla_Preferences_h
