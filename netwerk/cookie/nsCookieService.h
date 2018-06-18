/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookieService_h__
#define nsCookieService_h__

#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "nsCookie.h"
#include "nsCookieKey.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsHashKeys.h"
#include "nsIMemoryReporter.h"
#include "nsTHashtable.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStoragePendingStatement.h"
#include "mozIStorageConnection.h"
#include "mozIStorageRow.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStorageFunction.h"
#include "nsIVariant.h"
#include "nsIFile.h"
#include "mozilla/Atomics.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"


using mozilla::OriginAttributes;

class nsICookiePermission;
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
} // namespace net
} // namespace mozilla

using mozilla::net::nsCookieKey;
// Inherit from nsCookieKey so this can be stored in nsTHashTable
// TODO: why aren't we using nsClassHashTable<nsCookieKey, ArrayType>?
class nsCookieEntry : public nsCookieKey
{
  public:
    // Hash methods
    typedef nsTArray< RefPtr<nsCookie> > ArrayType;
    typedef ArrayType::index_type IndexType;

    explicit nsCookieEntry(KeyTypePointer aKey)
     : nsCookieKey(aKey)
    {}

    nsCookieEntry(const nsCookieEntry& toCopy)
    {
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

// struct for a constant cookie for threadsafe
struct ConstCookie
{
  ConstCookie(const nsCString& aName,
              const nsCString& aValue,
              const nsCString& aHost,
              const nsCString& aPath,
              int64_t aExpiry,
              int64_t aLastAccessed,
              int64_t aCreationTime,
              bool aIsSecure,
              bool aIsHttpOnly,
              const OriginAttributes &aOriginAttributes,
              int32_t aSameSite)
    : name(aName)
    , value(aValue)
    , host(aHost)
    , path(aPath)
    , expiry(aExpiry)
    , lastAccessed(aLastAccessed)
    , creationTime(aCreationTime)
    , isSecure(aIsSecure)
    , isHttpOnly(aIsHttpOnly)
    , originAttributes(aOriginAttributes)
    , sameSite(aSameSite)
  {
  }

  const nsCString name;
  const nsCString value;
  const nsCString host;
  const nsCString path;
  const int64_t expiry;
  const int64_t lastAccessed;
  const int64_t creationTime;
  const bool isSecure;
  const bool isHttpOnly;
  const OriginAttributes originAttributes;
  const int32_t sameSite;
};

// encapsulates a (key, nsCookie) tuple for temporary storage purposes.
struct CookieDomainTuple
{
  nsCookieKey key;
  mozilla::UniquePtr<ConstCookie> cookie;
};

// encapsulates in-memory and on-disk DB states, so we can
// conveniently switch state when entering or exiting private browsing.
struct DBState final
{
  DBState()
    : cookieCount(0)
    , cookieOldestTime(INT64_MAX)
    , corruptFlag(OK)
    , readListener(nullptr)
  {
  }

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

  nsTHashtable<nsCookieEntry>     hostTable;
  uint32_t                        cookieCount;
  int64_t                         cookieOldestTime;
  nsCOMPtr<nsIFile>               cookieFile;
  nsCOMPtr<mozIStorageConnection> dbConn;
  nsCOMPtr<mozIStorageAsyncStatement> stmtInsert;
  nsCOMPtr<mozIStorageAsyncStatement> stmtDelete;
  nsCOMPtr<mozIStorageAsyncStatement> stmtUpdate;
  CorruptFlag                     corruptFlag;

  // Various parts representing asynchronous read state. These are useful
  // while the background read is taking place.
  nsCOMPtr<mozIStorageConnection>       syncConn;
  nsCOMPtr<mozIStorageStatement>        stmtReadDomain;
  // The asynchronous read listener. This is a weak ref (storage has ownership)
  // since it may need to outlive the DBState's database connection.
  ReadCookieDBListener*                 readListener;

  // DB completion handlers.
  nsCOMPtr<mozIStorageStatementCallback>  insertListener;
  nsCOMPtr<mozIStorageStatementCallback>  updateListener;
  nsCOMPtr<mozIStorageStatementCallback>  removeListener;
  nsCOMPtr<mozIStorageCompletionCallback> closeListener;
};

// these constants represent a decision about a cookie based on user prefs.
enum CookieStatus
{
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
enum OpenDBResult
{
  RESULT_OK,
  RESULT_RETRY,
  RESULT_FAILURE
};

/******************************************************************************
 * nsCookieService:
 * class declaration
 ******************************************************************************/

// struct for temporarily storing cookie attributes during header parsing
struct nsCookieAttributes
{
  nsAutoCString name;
  nsAutoCString value;
  nsAutoCString host;
  nsAutoCString path;
  nsAutoCString expires;
  nsAutoCString maxage;
  int64_t expiryTime;
  bool isSession;
  bool isSecure;
  bool isHttpOnly;
  int8_t sameSite;
};

class nsCookieService final : public nsICookieService
                            , public nsICookieManager
                            , public nsIObserver
                            , public nsSupportsWeakReference
                            , public nsIMemoryReporter
{
  private:
    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    static bool sSameSiteEnabled; // cached pref

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
  static void AppClearDataObserverInit();
  static nsCString GetPathFromURI(nsIURI *aHostURI);
  static nsresult GetBaseDomain(nsIEffectiveTLDService *aTLDService, nsIURI *aHostURI, nsCString &aBaseDomain, bool &aRequireHostMatch);
  static nsresult GetBaseDomainFromHost(nsIEffectiveTLDService *aTLDService, const nsACString &aHost, nsCString &aBaseDomain);
  static bool DomainMatches(nsCookie* aCookie, const nsACString& aHost);
  static bool IsSameSiteEnabled();
  static bool PathMatches(nsCookie* aCookie, const nsACString& aPath);
  static bool CanSetCookie(nsIURI *aHostURI, const nsCookieKey& aKey, nsCookieAttributes &aCookieAttributes, bool aRequireHostMatch, CookieStatus aStatus, nsDependentCString &aCookieHeader, int64_t aServerTime, bool aFromHttp, nsIChannel* aChannel, bool aLeaveSercureAlone, bool &aSetCookie, mozIThirdPartyUtil* aThirdPartyUtil);
  static CookieStatus CheckPrefs(nsICookiePermission *aPermissionServices, uint8_t aCookieBehavior, bool aThirdPartySession, bool aThirdPartyNonsecureSession, nsIURI *aHostURI, bool aIsForeign, bool aIsTrackingResource, const char *aCookieHeader, const int aNumOfCookies, const OriginAttributes& aOriginAttrs);
  static int64_t ParseServerTime(const nsCString &aServerTime);
  void GetCookiesForURI(nsIURI *aHostURI, bool aIsForeign, bool aIsTrackingResource, bool aIsSafeTopLevelNav, bool aIsTopLevelForeign, bool aHttpBound, const OriginAttributes& aOriginAttrs, nsTArray<nsCookie*>& aCookieList);

  protected:
    virtual ~nsCookieService();

    void                          PrefChanged(nsIPrefBranch *aPrefBranch);
    void                          InitDBStates();
    OpenDBResult                  TryInitDB(bool aDeleteExistingDB);
    void                          InitDBConn();
    nsresult                      InitDBConnInternal();
    nsresult                      CreateTableWorker(const char* aName);
    nsresult                      CreateIndex();
    nsresult                      CreateTable();
    nsresult                      CreateTableForSchemaVersion6();
    nsresult                      CreateTableForSchemaVersion5();
    void                          CloseDBStates();
    void                          CleanupCachedStatements();
    void                          CleanupDefaultDBConnection();
    void                          HandleDBClosed(DBState* aDBState);
    void                          HandleCorruptDB(DBState* aDBState);
    void                          RebuildCorruptDB(DBState* aDBState);
    OpenDBResult                  Read();
    mozilla::UniquePtr<ConstCookie> GetCookieFromRow(mozIStorageStatement *aRow, const OriginAttributes &aOriginAttributes);
    void                          EnsureReadComplete(bool aInitDBConn);
    nsresult                      NormalizeHost(nsCString &aHost);
    nsresult                      GetCookieStringCommon(nsIURI *aHostURI, nsIChannel *aChannel, bool aHttpBound, char** aCookie);
    void                          GetCookieStringInternal(nsIURI *aHostURI, bool aIsForeign, bool aIsTrackingResource, bool aIsSafeTopLevelNav, bool aIsTopLevelForeign, bool aHttpBound, const OriginAttributes& aOriginAttrs, nsCString &aCookie);
    nsresult                      SetCookieStringCommon(nsIURI *aHostURI, const char *aCookieHeader, const char *aServerTime, nsIChannel *aChannel, bool aFromHttp);
    void                          SetCookieStringInternal(nsIURI *aHostURI, bool aIsForeign, bool aIsTrackingResource, nsDependentCString &aCookieHeader, const nsCString &aServerTime, bool aFromHttp, const OriginAttributes &aOriginAttrs, nsIChannel* aChannel);
    bool                          SetCookieInternal(nsIURI *aHostURI, const nsCookieKey& aKey, bool aRequireHostMatch, CookieStatus aStatus, nsDependentCString &aCookieHeader, int64_t aServerTime, bool aFromHttp, nsIChannel* aChannel);
    void                          AddInternal(const nsCookieKey& aKey, nsCookie *aCookie, int64_t aCurrentTimeInUsec, nsIURI *aHostURI, const char *aCookieHeader, bool aFromHttp);
    void                          RemoveCookieFromList(const nsListIter &aIter, mozIStorageBindingParamsArray *aParamsArray = nullptr);
    void                          AddCookieToList(const nsCookieKey& aKey, nsCookie *aCookie, DBState *aDBState, mozIStorageBindingParamsArray *aParamsArray, bool aWriteToDB = true);
    void                          UpdateCookieInList(nsCookie *aCookie, int64_t aLastAccessed, mozIStorageBindingParamsArray *aParamsArray);
    static bool                   GetTokenValue(nsACString::const_char_iterator &aIter, nsACString::const_char_iterator &aEndIter, nsDependentCSubstring &aTokenString, nsDependentCSubstring &aTokenValue, bool &aEqualsFound);
    static bool                   ParseAttributes(nsDependentCString &aCookieHeader, nsCookieAttributes &aCookie);
    bool                          RequireThirdPartyCheck();
    static bool                   CheckDomain(nsCookieAttributes &aCookie, nsIURI *aHostURI, const nsCString &aBaseDomain, bool aRequireHostMatch);
    static bool                   CheckPath(nsCookieAttributes &aCookie, nsIURI *aHostURI);
    static bool                   CheckPrefixes(nsCookieAttributes &aCookie, bool aSecureRequest);
    static bool                   GetExpiry(nsCookieAttributes &aCookie, int64_t aServerTime, int64_t aCurrentTime);
    void                          RemoveAllFromMemory();
    already_AddRefed<nsIArray>    PurgeCookies(int64_t aCurrentTimeInUsec);
    bool                          FindCookie(const nsCookieKey& aKey, const nsCString& aHost, const nsCString& aName, const nsCString& aPath, nsListIter &aIter);
    bool                          FindSecureCookie(const nsCookieKey& aKey, nsCookie* aCookie);
    int64_t                       FindStaleCookie(nsCookieEntry *aEntry, int64_t aCurrentTime, nsIURI* aSource, const mozilla::Maybe<bool> &aIsSecure, nsListIter &aIter);
    void                          TelemetryForEvictingStaleCookie(nsCookie* aEvicted, int64_t oldestCookieTime);
    void                          NotifyRejected(nsIURI *aHostURI);
    void                          NotifyThirdParty(nsIURI *aHostURI, bool aAccepted, nsIChannel *aChannel);
    void                          NotifyChanged(nsISupports *aSubject, const char16_t *aData, bool aOldCookieIsSession = false, bool aFromHttp = false);
    void                          NotifyPurged(nsICookie2* aCookie);
    already_AddRefed<nsIArray>    CreatePurgeList(nsICookie2* aCookie);
    void                          UpdateCookieOldestTime(DBState* aDBState, nsCookie* aCookie);

    nsresult                      GetCookiesWithOriginAttributes(const mozilla::OriginAttributesPattern& aPattern, const nsCString& aBaseDomain, nsISimpleEnumerator **aEnumerator);
    nsresult                      RemoveCookiesWithOriginAttributes(const mozilla::OriginAttributesPattern& aPattern, const nsCString& aBaseDomain);

    /**
     * This method is a helper that allows calling nsICookieManager::Remove()
     * with OriginAttributes parameter.
     * NOTE: this could be added to a public interface if we happen to need it.
     */
    nsresult Remove(const nsACString& aHost, const OriginAttributes& aAttrs,
                    const nsACString& aName, const nsACString& aPath,
                    bool aBlocked);

  protected:
    // cached members.
    nsCOMPtr<nsICookiePermission>    mPermissionService;
    nsCOMPtr<mozIThirdPartyUtil>     mThirdPartyUtil;
    nsCOMPtr<nsIEffectiveTLDService> mTLDService;
    nsCOMPtr<nsIIDNService>          mIDNService;
    nsCOMPtr<mozIStorageService>     mStorageService;

    // we have two separate DB states: one for normal browsing and one for
    // private browsing, switching between them on a per-cookie-request basis.
    // this state encapsulates both the in-memory table and the on-disk DB.
    // note that the private states' dbConn should always be null - we never
    // want to be dealing with the on-disk DB when in private browsing.
    DBState                      *mDBState;
    RefPtr<DBState>             mDefaultDBState;
    RefPtr<DBState>             mPrivateDBState;

    // cached prefs
    uint8_t                       mCookieBehavior; // BEHAVIOR_{ACCEPT, REJECTFOREIGN, REJECT, LIMITFOREIGN}
    bool                          mThirdPartySession;
    bool                          mThirdPartyNonsecureSession;
    bool                          mLeaveSecureAlone;
    uint16_t                      mMaxNumberOfCookies;
    uint16_t                      mMaxCookiesPerHost;
    int64_t                       mCookiePurgeAge;

    // thread
    nsCOMPtr<nsIThread>           mThread;
    mozilla::Monitor              mMonitor;
    mozilla::Atomic<bool>         mInitializedDBStates;
    mozilla::Atomic<bool>         mInitializedDBConn;
    mozilla::TimeStamp            mEndInitDBConn;
    nsTArray<CookieDomainTuple>   mReadArray;

    // friends!
    friend class DBListenerErrorHandler;
    friend class ReadCookieDBListener;
    friend class CloseCookieDBListener;

    static already_AddRefed<nsCookieService> GetSingleton();
    friend class mozilla::net::CookieServiceParent;
};

#endif // nsCookieService_h__
