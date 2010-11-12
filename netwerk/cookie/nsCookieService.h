/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCookieService_h__
#define nsCookieService_h__

#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsICookieManager2.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "nsCookie.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStoragePendingStatement.h"
#include "mozIStorageConnection.h"
#include "mozIStorageRow.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageStatementCallback.h"

class nsICookiePermission;
class nsIEffectiveTLDService;
class nsIIDNService;
class nsIPrefBranch;
class nsIObserverService;
class nsIURI;
class nsIChannel;
class nsIArray;
class mozIStorageService;
class mozIThirdPartyUtil;
class ReadCookieDBListener;

struct nsCookieAttributes;
struct nsListIter;
struct nsEnumerationData;

namespace mozilla {
namespace net {
#ifdef MOZ_IPC
class CookieServiceParent;
#endif
}
}

// hash entry class
class nsCookieEntry : public PLDHashEntryHdr
{
  public:
    // Hash methods
    typedef const nsCString& KeyType;
    typedef const nsCString* KeyTypePointer;
    typedef nsTArray< nsRefPtr<nsCookie> > ArrayType;
    typedef ArrayType::index_type IndexType;

    explicit
    nsCookieEntry(KeyTypePointer aBaseDomain)
     : mBaseDomain(*aBaseDomain)
    {
    }

    nsCookieEntry(const nsCookieEntry& toCopy)
    {
      // if we end up here, things will break. nsTHashtable shouldn't
      // allow this, since we set ALLOW_MEMMOVE to true.
      NS_NOTREACHED("nsCookieEntry copy constructor is forbidden!");
    }

    ~nsCookieEntry()
    {
    }

    KeyType GetKey() const
    {
      return mBaseDomain;
    }

    PRBool KeyEquals(KeyTypePointer aKey) const
    {
      return mBaseDomain == *aKey;
    }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
      return &aKey;
    }

    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      return HashString(*aKey);
    }

    enum { ALLOW_MEMMOVE = PR_TRUE };

    inline ArrayType& GetCookies() { return mCookies; }

  private:
    nsCString mBaseDomain;
    ArrayType mCookies;
};

// encapsulates a (baseDomain, nsCookie) tuple for temporary storage purposes.
struct CookieDomainTuple
{
  nsCString baseDomain;
  nsRefPtr<nsCookie> cookie;
};

// encapsulates in-memory and on-disk DB states, so we can
// conveniently switch state when entering or exiting private browsing.
struct DBState
{
  DBState() : cookieCount(0), cookieOldestTime(LL_MAXINT)
  {
    hostTable.Init();
  }

  NS_INLINE_DECL_REFCOUNTING(DBState)

  nsTHashtable<nsCookieEntry>     hostTable;
  PRUint32                        cookieCount;
  PRInt64                         cookieOldestTime;
  nsCOMPtr<nsIFile>               cookieFile;
  nsCOMPtr<mozIStorageConnection> dbConn;
  nsCOMPtr<mozIStorageAsyncStatement> stmtInsert;
  nsCOMPtr<mozIStorageAsyncStatement> stmtDelete;
  nsCOMPtr<mozIStorageAsyncStatement> stmtUpdate;

  // Various parts representing asynchronous read state. These are useful
  // while the background read is taking place.
  nsCOMPtr<mozIStorageConnection>       syncConn;
  nsCOMPtr<mozIStorageStatement>        stmtReadDomain;
  nsCOMPtr<mozIStoragePendingStatement> pendingRead;
  // The asynchronous read listener. This is a weak ref (storage has ownership)
  // since it may need to outlive the DBState's database connection.
  ReadCookieDBListener*                 readListener;
  // An array of (baseDomain, cookie) tuples representing data read in
  // asynchronously. This is merged into hostTable once read is complete.
  nsTArray<CookieDomainTuple>           hostArray;
  // A hashset of baseDomains read in synchronously, while the async read is
  // in flight. This is used to keep track of which data in hostArray is stale
  // when the time comes to merge.
  nsTHashtable<nsCStringHashKey>        readSet;

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

class nsCookieService : public nsICookieService
                      , public nsICookieManager2
                      , public nsIObserver
                      , public nsSupportsWeakReference
{
  public:
    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSICOOKIESERVICE
    NS_DECL_NSICOOKIEMANAGER
    NS_DECL_NSICOOKIEMANAGER2

    nsCookieService();
    virtual ~nsCookieService();
    static nsICookieService*      GetXPCOMSingleton();
    nsresult                      Init();

  protected:
    void                          PrefChanged(nsIPrefBranch *aPrefBranch);
    void                          InitDBStates();
    OpenDBResult                  TryInitDB(bool aDeleteExistingDB);
    nsresult                      CreateTable();
    void                          CloseDBStates();
    void                          CloseDefaultDBConnection();
    OpenDBResult                  Read();
    template<class T> nsCookie*   GetCookieFromRow(T &aRow);
    void                          AsyncReadComplete();
    void                          CancelAsyncRead(PRBool aPurgeReadSet);
    void                          EnsureReadDomain(const nsCString &aBaseDomain);
    void                          EnsureReadComplete();
    nsresult                      NormalizeHost(nsCString &aHost);
    nsresult                      GetBaseDomain(nsIURI *aHostURI, nsCString &aBaseDomain, PRBool &aRequireHostMatch);
    nsresult                      GetBaseDomainFromHost(const nsACString &aHost, nsCString &aBaseDomain);
    nsresult                      GetCookieStringCommon(nsIURI *aHostURI, nsIChannel *aChannel, bool aHttpBound, char** aCookie);
    void                          GetCookieStringInternal(nsIURI *aHostURI, bool aIsForeign, PRBool aHttpBound, nsCString &aCookie);
    nsresult                      SetCookieStringCommon(nsIURI *aHostURI, const char *aCookieHeader, const char *aServerTime, nsIChannel *aChannel, bool aFromHttp);
    void                          SetCookieStringInternal(nsIURI *aHostURI, bool aIsForeign, const nsCString &aCookieHeader, const nsCString &aServerTime, PRBool aFromHttp);
    PRBool                        SetCookieInternal(nsIURI *aHostURI, const nsCString& aBaseDomain, PRBool aRequireHostMatch, CookieStatus aStatus, nsDependentCString &aCookieHeader, PRInt64 aServerTime, PRBool aFromHttp);
    void                          AddInternal(const nsCString& aBaseDomain, nsCookie *aCookie, PRInt64 aCurrentTimeInUsec, nsIURI *aHostURI, const char *aCookieHeader, PRBool aFromHttp);
    void                          RemoveCookieFromList(const nsListIter &aIter, mozIStorageBindingParamsArray *aParamsArray = NULL);
    void                          AddCookieToList(const nsCString& aBaseDomain, nsCookie *aCookie, DBState *aDBState, mozIStorageBindingParamsArray *aParamsArray, PRBool aWriteToDB = PR_TRUE);
    void                          UpdateCookieInList(nsCookie *aCookie, PRInt64 aLastAccessed, mozIStorageBindingParamsArray *aParamsArray);
    static PRBool                 GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter, nsASingleFragmentCString::const_char_iterator &aEndIter, nsDependentCSubstring &aTokenString, nsDependentCSubstring &aTokenValue, PRBool &aEqualsFound);
    static PRBool                 ParseAttributes(nsDependentCString &aCookieHeader, nsCookieAttributes &aCookie);
    bool                          RequireThirdPartyCheck();
    CookieStatus                  CheckPrefs(nsIURI *aHostURI, bool aIsForeign, const nsCString &aBaseDomain, PRBool aRequireHostMatch, const char *aCookieHeader);
    PRBool                        CheckDomain(nsCookieAttributes &aCookie, nsIURI *aHostURI, const nsCString &aBaseDomain, PRBool aRequireHostMatch);
    static PRBool                 CheckPath(nsCookieAttributes &aCookie, nsIURI *aHostURI);
    static PRBool                 GetExpiry(nsCookieAttributes &aCookie, PRInt64 aServerTime, PRInt64 aCurrentTime);
    void                          RemoveAllFromMemory();
    already_AddRefed<nsIArray>    PurgeCookies(PRInt64 aCurrentTimeInUsec);
    PRBool                        FindCookie(const nsCString& aBaseDomain, const nsAFlatCString &aHost, const nsAFlatCString &aName, const nsAFlatCString &aPath, nsListIter &aIter);
    static void                   FindStaleCookie(nsCookieEntry *aEntry, PRInt64 aCurrentTime, nsListIter &aIter);
    void                          NotifyRejected(nsIURI *aHostURI);
    void                          NotifyChanged(nsISupports *aSubject, const PRUnichar *aData);
    void                          NotifyPurged(nsICookie2* aCookie);
    already_AddRefed<nsIArray>    CreatePurgeList(nsICookie2* aCookie);

  protected:
    // cached members.
    nsCOMPtr<nsIObserverService>     mObserverService;
    nsCOMPtr<nsICookiePermission>    mPermissionService;
    nsCOMPtr<mozIThirdPartyUtil>     mThirdPartyUtil;
    nsCOMPtr<nsIEffectiveTLDService> mTLDService;
    nsCOMPtr<nsIIDNService>          mIDNService;
    nsCOMPtr<mozIStorageService>     mStorageService;

    // we have two separate DB states: one for normal browsing and one for
    // private browsing, switching between them as appropriate. this state
    // encapsulates both the in-memory table and the on-disk DB.
    // note that the private states' dbConn should always be null - we never
    // want to be dealing with the on-disk DB when in private browsing.
    DBState                      *mDBState;
    nsRefPtr<DBState>             mDefaultDBState;
    nsRefPtr<DBState>             mPrivateDBState;

    // cached prefs
    PRUint8                       mCookieBehavior; // BEHAVIOR_{ACCEPT, REJECTFOREIGN, REJECT}
    PRBool                        mThirdPartySession;
    PRUint16                      mMaxNumberOfCookies;
    PRUint16                      mMaxCookiesPerHost;
    PRInt64                       mCookiePurgeAge;

    // friends!
    friend PLDHashOperator purgeCookiesCallback(nsCookieEntry *aEntry, void *aArg);
    friend class ReadCookieDBListener;

    static nsCookieService*       GetSingleton();
#ifdef MOZ_IPC
    friend class mozilla::net::CookieServiceParent;
#endif
};

#endif // nsCookieService_h__
