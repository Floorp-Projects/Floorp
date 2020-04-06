/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookieService_h__
#define nsCookieService_h__

#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsICookiePermission.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "Cookie.h"
#include "nsCookieKey.h"
#include "nsString.h"
#include "nsHashKeys.h"
#include "nsIMemoryReporter.h"
#include "nsTHashtable.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageConnection.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageStatementCallback.h"
#include "nsIFile.h"
#include "mozilla/Atomics.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"

using mozilla::OriginAttributes;

class nsICookiePermission;
class nsICookieJarSettings;
class nsIEffectiveTLDService;
class nsIIDNService;
class nsIPrefBranch;
class nsIObserverService;
class nsIURI;
class nsIChannel;
class nsIArray;
class nsIThread;
class mozIStorageService;
class mozIThirdPartyUtil;
class ReadCookieDBListener;

struct nsListIter;

namespace mozilla {
namespace net {
class nsCookieKey;
class CookieServiceParent;
}  // namespace net
}  // namespace mozilla

using mozilla::net::nsCookieKey;
// Inherit from nsCookieKey so this can be stored in nsTHashTable
// TODO: why aren't we using nsClassHashTable<nsCookieKey, ArrayType>?
class nsCookieEntry : public nsCookieKey {
 public:
  // Hash methods
  typedef nsTArray<RefPtr<mozilla::net::Cookie>> ArrayType;
  typedef ArrayType::index_type IndexType;

  explicit nsCookieEntry(KeyTypePointer aKey) : nsCookieKey(aKey) {}

  nsCookieEntry(const nsCookieEntry& toCopy) {
    // if we end up here, things will break. nsTHashtable shouldn't
    // allow this, since we set ALLOW_MEMMOVE to true.
    MOZ_ASSERT_UNREACHABLE("nsCookieEntry copy constructor is forbidden!");
  }

  ~nsCookieEntry() = default;

  inline ArrayType& GetCookies() { return mCookies; }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  ArrayType mCookies;
};

// encapsulates a (key, Cookie) tuple for temporary storage purposes.
struct CookieDomainTuple {
  nsCookieKey key;
  OriginAttributes originAttributes;
  mozilla::UniquePtr<mozilla::net::CookieStruct> cookie;
};

// encapsulates in-memory and on-disk DB states, so we can
// conveniently switch state when entering or exiting private browsing.
struct DBState final {
  DBState()
      : cookieCount(0),
        cookieOldestTime(INT64_MAX),
        corruptFlag(OK),
        readListener(nullptr) {}

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~DBState() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING(DBState)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // State of the database connection.
  enum CorruptFlag {
    OK,                   // normal
    CLOSING_FOR_REBUILD,  // corruption detected, connection closing
    REBUILDING            // close complete, rebuilding database from memory
  };

  nsTHashtable<nsCookieEntry> hostTable;
  uint32_t cookieCount;
  int64_t cookieOldestTime;
  nsCOMPtr<nsIFile> cookieFile;
  nsCOMPtr<mozIStorageConnection> dbConn;
  nsCOMPtr<mozIStorageAsyncStatement> stmtInsert;
  nsCOMPtr<mozIStorageAsyncStatement> stmtDelete;
  nsCOMPtr<mozIStorageAsyncStatement> stmtUpdate;
  CorruptFlag corruptFlag;

  // Various parts representing asynchronous read state. These are useful
  // while the background read is taking place.
  nsCOMPtr<mozIStorageConnection> syncConn;
  nsCOMPtr<mozIStorageStatement> stmtReadDomain;
  // The asynchronous read listener. This is a weak ref (storage has ownership)
  // since it may need to outlive the DBState's database connection.
  ReadCookieDBListener* readListener;

  // DB completion handlers.
  nsCOMPtr<mozIStorageStatementCallback> insertListener;
  nsCOMPtr<mozIStorageStatementCallback> updateListener;
  nsCOMPtr<mozIStorageStatementCallback> removeListener;
  nsCOMPtr<mozIStorageCompletionCallback> closeListener;
};

// these constants represent an operation being performed on cookies
enum CookieOperation { OPERATION_READ, OPERATION_WRITE };

// these constants represent a decision about a cookie based on user prefs.
enum CookieStatus {
  STATUS_ACCEPTED,
  STATUS_ACCEPT_SESSION,
  STATUS_REJECTED,
  // STATUS_REJECTED_WITH_ERROR indicates the cookie should be rejected because
  // of an error (rather than something the user can control). this is used for
  // notification purposes, since we only want to notify of rejections where
  // the user can do something about it (e.g. whitelist the site).
  STATUS_REJECTED_WITH_ERROR
};

// Result codes for TryInitDB() and Read().
enum OpenDBResult { RESULT_OK, RESULT_RETRY, RESULT_FAILURE };

/******************************************************************************
 * nsCookieService:
 * class declaration
 ******************************************************************************/

class nsCookieService final : public nsICookieService,
                              public nsICookieManager,
                              public nsIObserver,
                              public nsSupportsWeakReference,
                              public nsIMemoryReporter {
 private:
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSICOOKIESERVICE
  NS_DECL_NSICOOKIEMANAGER
  NS_DECL_NSIMEMORYREPORTER

  nsCookieService();
  static already_AddRefed<nsICookieService> GetXPCOMSingleton();
  nsresult Init();

  /**
   * Start watching the observer service for messages indicating that an app has
   * been uninstalled.  When an app is uninstalled, we get the cookie service
   * (thus instantiating it, if necessary) and clear all the cookies for that
   * app.
   */
  static nsAutoCString GetPathFromURI(nsIURI* aHostURI);
  static nsresult GetBaseDomain(nsIEffectiveTLDService* aTLDService,
                                nsIURI* aHostURI, nsCString& aBaseDomain,
                                bool& aRequireHostMatch);
  static nsresult GetBaseDomainFromHost(nsIEffectiveTLDService* aTLDService,
                                        const nsACString& aHost,
                                        nsCString& aBaseDomain);
  static bool DomainMatches(mozilla::net::Cookie* aCookie,
                            const nsACString& aHost);
  static bool PathMatches(mozilla::net::Cookie* aCookie,
                          const nsACString& aPath);
  static bool CanSetCookie(nsIURI* aHostURI, const nsCookieKey& aKey,
                           mozilla::net::CookieStruct& aCookieData,
                           bool aRequireHostMatch, CookieStatus aStatus,
                           nsCString& aCookieHeader, int64_t aServerTime,
                           bool aFromHttp, nsIChannel* aChannel,
                           bool& aSetCookie,
                           mozIThirdPartyUtil* aThirdPartyUtil);
  static CookieStatus CheckPrefs(nsICookieJarSettings* aCookieJarSettings,
                                 nsIURI* aHostURI, bool aIsForeign,
                                 bool aIsThirdPartyTrackingResource,
                                 bool aIsThirdPartySocialTrackingResource,
                                 bool aFirstPartyStorageAccessGranted,
                                 const nsACString& aCookieHeader,
                                 const int aNumOfCookies,
                                 const OriginAttributes& aOriginAttrs,
                                 uint32_t* aRejectedReason);
  static int64_t ParseServerTime(const nsACString& aServerTime);

  static already_AddRefed<nsICookieJarSettings> GetCookieJarSettings(
      nsIChannel* aChannel);

  void GetCookiesForURI(nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
                        bool aIsThirdPartyTrackingResource,
                        bool aIsThirdPartySocialTrackingResource,
                        bool aFirstPartyStorageAccessGranted,
                        uint32_t aRejectedReason, bool aIsSafeTopLevelNav,
                        bool aIsSameSiteForeign, bool aHttpBound,
                        const OriginAttributes& aOriginAttrs,
                        nsTArray<mozilla::net::Cookie*>& aCookieList);

  /**
   * This method is a helper that allows calling nsICookieManager::Remove()
   * with OriginAttributes parameter.
   */
  nsresult Remove(const nsACString& aHost, const OriginAttributes& aAttrs,
                  const nsACString& aName, const nsACString& aPath);

 protected:
  virtual ~nsCookieService();

  void PrefChanged(nsIPrefBranch* aPrefBranch);
  void InitDBStates();
  OpenDBResult TryInitDB(bool aDeleteExistingDB);
  void InitDBConn();
  nsresult InitDBConnInternal();
  nsresult CreateTableWorker(const char* aName);
  nsresult CreateTable();
  nsresult CreateTableForSchemaVersion6();
  nsresult CreateTableForSchemaVersion5();
  void CloseDBStates();
  void CleanupCachedStatements();
  void CleanupDefaultDBConnection();
  void HandleDBClosed(DBState* aDBState);
  void HandleCorruptDB(DBState* aDBState);
  void RebuildCorruptDB(DBState* aDBState);
  OpenDBResult Read();
  mozilla::UniquePtr<mozilla::net::CookieStruct> GetCookieFromRow(
      mozIStorageStatement* aRow);
  void EnsureReadComplete(bool aInitDBConn);
  nsresult NormalizeHost(nsCString& aHost);
  nsresult GetCookieStringCommon(nsIURI* aHostURI, nsIChannel* aChannel,
                                 bool aHttpBound, nsACString& aCookie);
  void GetCookieStringInternal(
      nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
      bool aIsThirdPartyTrackingResource,
      bool aIsThirdPartySocialTrackingResource,
      bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
      bool aIsSafeTopLevelNav, bool aIsSameSiteForeign, bool aHttpBound,
      const OriginAttributes& aOriginAttrs, nsACString& aCookie);
  nsresult SetCookieStringCommon(nsIURI* aHostURI,
                                 const nsACString& aCookieHeader,
                                 const nsACString& aServerTime,
                                 nsIChannel* aChannel, bool aFromHttp);
  void SetCookieStringInternal(
      nsIURI* aHostURI, bool aIsForeign, bool aIsThirdPartyTrackingResource,
      bool aIsThirdPartySocialTrackingResource,
      bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
      nsCString& aCookieHeader, const nsACString& aServerTime, bool aFromHttp,
      const OriginAttributes& aOriginAttrs, nsIChannel* aChannel);
  bool SetCookieInternal(nsIURI* aHostURI, const nsCookieKey& aKey,
                         bool aRequireHostMatch, CookieStatus aStatus,
                         nsCString& aCookieHeader, int64_t aServerTime,
                         bool aFromHttp, nsIChannel* aChannel);
  void AddInternal(const nsCookieKey& aKey, mozilla::net::Cookie* aCookie,
                   int64_t aCurrentTimeInUsec, nsIURI* aHostURI,
                   const nsACString& aCookieHeader, bool aFromHttp);
  void RemoveCookieFromList(
      const nsListIter& aIter,
      mozIStorageBindingParamsArray* aParamsArray = nullptr);
  void AddCookieToList(const nsCookieKey& aKey, mozilla::net::Cookie* aCookie,
                       DBState* aDBState,
                       mozIStorageBindingParamsArray* aParamsArray,
                       bool aWriteToDB = true);
  void UpdateCookieInList(mozilla::net::Cookie* aCookie, int64_t aLastAccessed,
                          mozIStorageBindingParamsArray* aParamsArray);
  static bool GetTokenValue(nsACString::const_char_iterator& aIter,
                            nsACString::const_char_iterator& aEndIter,
                            nsDependentCSubstring& aTokenString,
                            nsDependentCSubstring& aTokenValue,
                            bool& aEqualsFound);
  static bool ParseAttributes(nsIChannel* aChannel, nsIURI* aHostURI,
                              nsCString& aCookieHeader,
                              mozilla::net::CookieStruct& aCookieData,
                              nsACString& aExpires, nsACString& aMaxage,
                              bool& aAcceptedByParser);
  bool RequireThirdPartyCheck();
  static bool CheckDomain(mozilla::net::CookieStruct& aCookieData,
                          nsIURI* aHostURI, const nsCString& aBaseDomain,
                          bool aRequireHostMatch);
  static bool CheckPath(mozilla::net::CookieStruct& aCookieData,
                        nsIChannel* aChannel, nsIURI* aHostURI);
  static bool CheckPrefixes(mozilla::net::CookieStruct& aCookieData,
                            bool aSecureRequest);
  static bool GetExpiry(mozilla::net::CookieStruct& aCookieData,
                        const nsACString& aExpires, const nsACString& aMaxage,
                        int64_t aServerTime, int64_t aCurrentTime,
                        bool aFromHttp);
  void RemoveAllFromMemory();
  already_AddRefed<nsIArray> PurgeCookies(int64_t aCurrentTimeInUsec);
  bool FindCookie(const nsCookieKey& aKey, const nsCString& aHost,
                  const nsCString& aName, const nsCString& aPath,
                  nsListIter& aIter);
  bool FindSecureCookie(const nsCookieKey& aKey, mozilla::net::Cookie* aCookie);
  void FindStaleCookies(nsCookieEntry* aEntry, int64_t aCurrentTime,
                        bool aIsSecure, nsTArray<nsListIter>& aOutput,
                        uint32_t aLimit);
  void NotifyAccepted(nsIChannel* aChannel);
  void NotifyRejected(nsIURI* aHostURI, nsIChannel* aChannel,
                      uint32_t aRejectedReason, CookieOperation aOperation);
  void NotifyChanged(nsISupports* aSubject, const char16_t* aData,
                     bool aOldCookieIsSession = false, bool aFromHttp = false);
  void NotifyPurged(nsICookie* aCookie);
  already_AddRefed<nsIArray> CreatePurgeList(nsICookie* aCookie);
  void CreateOrUpdatePurgeList(nsIArray** aPurgeList, nsICookie* aCookie);
  void UpdateCookieOldestTime(DBState* aDBState, mozilla::net::Cookie* aCookie);

  nsresult GetCookiesWithOriginAttributes(
      const mozilla::OriginAttributesPattern& aPattern,
      const nsCString& aBaseDomain, nsTArray<RefPtr<nsICookie>>& aResult);
  nsresult RemoveCookiesWithOriginAttributes(
      const mozilla::OriginAttributesPattern& aPattern,
      const nsCString& aBaseDomain);

  nsresult CountCookiesFromHostInternal(const nsACString& aHost,
                                        uint32_t aPrivateBrowsingId,
                                        uint32_t* aCountFromHost);

 protected:
  nsresult RemoveCookiesFromExactHost(
      const nsACString& aHost,
      const mozilla::OriginAttributesPattern& aPattern);

  static void LogMessageToConsole(nsIChannel* aChannel, nsIURI* aURI,
                                  uint32_t aErrorFlags,
                                  const nsACString& aCategory,
                                  const nsACString& aMsg,
                                  const nsTArray<nsString>& aParams);

  // cached members.
  nsCOMPtr<nsICookiePermission> mPermissionService;
  nsCOMPtr<mozIThirdPartyUtil> mThirdPartyUtil;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  nsCOMPtr<nsIIDNService> mIDNService;
  nsCOMPtr<mozIStorageService> mStorageService;

  // we have two separate DB states: one for normal browsing and one for
  // private browsing, switching between them on a per-cookie-request basis.
  // this state encapsulates both the in-memory table and the on-disk DB.
  // note that the private states' dbConn should always be null - we never
  // want to be dealing with the on-disk DB when in private browsing.
  DBState* mDBState;
  RefPtr<DBState> mDefaultDBState;
  RefPtr<DBState> mPrivateDBState;

  uint16_t mMaxNumberOfCookies;
  uint16_t mMaxCookiesPerHost;
  uint16_t mCookieQuotaPerHost;
  int64_t mCookiePurgeAge;

  // thread
  nsCOMPtr<nsIThread> mThread;
  mozilla::Monitor mMonitor;
  mozilla::Atomic<bool> mInitializedDBStates;
  mozilla::Atomic<bool> mInitializedDBConn;
  mozilla::TimeStamp mEndInitDBConn;
  nsTArray<CookieDomainTuple> mReadArray;

  // friends!
  friend class DBListenerErrorHandler;
  friend class ReadCookieDBListener;
  friend class CloseCookieDBListener;

  static already_AddRefed<nsCookieService> GetSingleton();
  friend class mozilla::net::CookieServiceParent;
};

#endif  // nsCookieService_h__
