// vim:ts=2:sw=2:et:
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsCookieService.h"
#include "nsIServiceManager.h"

#include "nsIIOService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefService.h"
#include "nsICookieConsent.h"
#include "nsICookiePermission.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h" // evil hack!
#include "nsIPrompt.h"
#include "nsITimer.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsILineInputStream.h"

#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsAutoPtr.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prtime.h"
#include "prprf.h"
#include "prnetdb.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsAppDirectoryServiceDefs.h"

/******************************************************************************
 * nsCookieService impl:
 * useful types & constants
 ******************************************************************************/

static const char kCookieFileName[] = "cookies.txt";

static const PRUint32 kLazyWriteTimeout = 5000; //msec

#undef  LIMIT
#define LIMIT(x, low, high, default) ((x) >= (low) && (x) <= (high) ? (x) : (default))

// default limits for the cookie list. these can be tuned by the
// network.cookie.maxNumber and network.cookie.maxPerHost prefs respectively.
static const PRUint32 kMaxNumberOfCookies = 1000;
static const PRUint32 kMaxCookiesPerHost  = 50;
static const PRUint32 kMaxBytesPerCookie  = 4096;

// this constant augments those defined on nsICookie, and indicates
// the cookie should be rejected because of an error (rather than
// something the user can control). this is used for notifying about
// rejected cookies, since we only want to notify of rejections where
// the user can do something about it (e.g. whitelist the site).
static const nsCookieStatus STATUS_REJECTED_WITH_ERROR = 5;

// XXX these casts and constructs are horrible, but our nsInt64/nsTime
// classes are lacking so we need them for now. see bug 198694.
#define USEC_PER_SEC   (nsInt64(1000000))
#define NOW_IN_SECONDS (nsInt64(PR_Now()) / USEC_PER_SEC)

// behavior pref constants 
static const PRUint32 BEHAVIOR_ACCEPT        = 0;
static const PRUint32 BEHAVIOR_REJECTFOREIGN = 1;
static const PRUint32 BEHAVIOR_REJECT        = 2;
static const PRUint32 BEHAVIOR_P3P           = 3;

// pref string constants
static const char kPrefCookiesPermissions[] = "network.cookie.cookieBehavior";
static const char kPrefMaxNumberOfCookies[] = "network.cookie.maxNumber";
static const char kPrefMaxCookiesPerHost[]  = "network.cookie.maxPerHost";

// struct for temporarily storing cookie attributes during header parsing
struct nsCookieAttributes
{
  nsCAutoString name;
  nsCAutoString value;
  nsCAutoString host;
  nsCAutoString path;
  nsCAutoString expires;
  nsCAutoString maxage;
  nsInt64 expiryTime;
  PRBool isSession;
  PRBool isSecure;
};

// stores linked list iteration state, and provides a rudimentary
// list traversal method
struct nsListIter
{
  nsListIter() {}

  nsListIter(nsCookieEntry *aEntry)
   : entry(aEntry)
   , prev(nsnull)
   , current(aEntry ? aEntry->Head() : nsnull) {}

  nsListIter(nsCookieEntry *aEntry,
             nsCookie      *aPrev,
             nsCookie      *aCurrent)
   : entry(aEntry)
   , prev(aPrev)
   , current(aCurrent) {}

  nsListIter& operator++() { prev = current; current = current->Next(); return *this; }

  nsCookieEntry *entry;
  nsCookie      *prev;
  nsCookie      *current;
};

// stores temporary data for enumerating over the hash entries,
// since enumeration is done using callback functions
struct nsEnumerationData
{
  nsEnumerationData(nsInt64 aCurrentTime,
                    PRInt64 aOldestTime)
   : currentTime(aCurrentTime)
   , oldestTime(aOldestTime)
   , iter(nsnull, nsnull, nsnull) {}

  // the current time
  nsInt64 currentTime;

  // oldest lastAccessed time in the cookie list. use aOldestTime = LL_MAXINT
  // to enable this search, LL_MININT to disable it.
  nsInt64 oldestTime;

  // an iterator object that points to the desired cookie
  nsListIter iter;
};

/******************************************************************************
 * Cookie logging handlers
 * used for logging in nsCookieService
 ******************************************************************************/

// logging handlers
#ifdef MOZ_LOGGING
// in order to do logging, the following environment variables need to be set:
//
//    set NSPR_LOG_MODULES=cookie:3 -- shows rejected cookies
//    set NSPR_LOG_MODULES=cookie:4 -- shows accepted and rejected cookies
//    set NSPR_LOG_FILE=cookie.log
//
// this next define has to appear before the include of prlog.h
#define FORCE_PR_LOG // Allow logging in the release build
#include "prlog.h"
#endif

// define logging macros for convenience
#define SET_COOKIE PR_TRUE
#define GET_COOKIE PR_FALSE

#ifdef PR_LOGGING
static PRLogModuleInfo *sCookieLog = PR_NewLogModule("cookie");

#define COOKIE_LOGFAILURE(a, b, c, d) LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d) LogSuccess(a, b, c, d)

static void
LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, const char *aReason)
{
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(sCookieLog, PR_LOG_WARNING)) {
    return;
  }

  nsCAutoString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  PR_LOG(sCookieLog, PR_LOG_WARNING,
    ("%s%s%s\n", "===== ", aSetCookie ? "COOKIE NOT ACCEPTED" : "COOKIE NOT SENT", " ====="));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("request URL: %s\n", spec.get()));
  if (aSetCookie) {
    PR_LOG(sCookieLog, PR_LOG_WARNING,("cookie string: %s\n", aCookieString));
  }

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(sCookieLog, PR_LOG_WARNING,("current time: %s", timeString));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("rejected because %s\n", aReason));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("\n"));
}

static void
LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, nsCookie *aCookie)
{
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(sCookieLog, PR_LOG_DEBUG)) {
    return;
  }

  nsCAutoString spec;
  aHostURI->GetAsciiSpec(spec);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,
    ("%s%s%s\n", "===== ", aSetCookie ? "COOKIE ACCEPTED" : "COOKIE SENT", " ====="));
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("request URL: %s\n", spec.get()));
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("cookie string: %s\n", aCookieString));

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,("current time: %s", timeString));

  if (aSetCookie) {
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("----------------\n"));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("name: %s\n", aCookie->Name().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("value: %s\n", aCookie->Value().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("%s: %s\n", aCookie->IsDomain() ? "domain" : "host", aCookie->Host().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("path: %s\n", aCookie->Path().get()));

    if (!aCookie->IsSession()) {
      PR_ExplodeTime(aCookie->Expiry() * USEC_PER_SEC, PR_GMTParameters, &explodedTime);
      PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    }

    PR_LOG(sCookieLog, PR_LOG_DEBUG,
      ("expires: %s", aCookie->IsSession() ? "at end of session" : timeString));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("is secure: %s\n", aCookie->IsSecure() ? "true" : "false"));
  }
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("\n"));
}

// inline wrappers to make passing in nsAFlatCStrings easier
static inline void
LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, const char *aReason)
{
  LogFailure(aSetCookie, aHostURI, aCookieString.get(), aReason);
}

static inline void
LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, nsCookie *aCookie)
{
  LogSuccess(aSetCookie, aHostURI, aCookieString.get(), aCookie);
}

#else
#define COOKIE_LOGFAILURE(a, b, c, d) /* nothing */
#define COOKIE_LOGSUCCESS(a, b, c, d) /* nothing */
#endif

/******************************************************************************
 * nsCookieService impl:
 * private list sorting callbacks
 *
 * these functions return:
 *   < 0 if the first element should come before the second element,
 *     0 if the first element may come before or after the second element,
 *   > 0 if the first element should come after the second element.
 ******************************************************************************/

// comparison function for sorting cookies before sending to a server.
PR_STATIC_CALLBACK(int)
compareCookiesForSending(const void *aElement1,
                         const void *aElement2,
                         void       *aData)
{
  const nsCookie *cookie1 = NS_STATIC_CAST(const nsCookie*, aElement1);
  const nsCookie *cookie2 = NS_STATIC_CAST(const nsCookie*, aElement2);

  // compare by cookie path length in accordance with RFC2109
  int rv = cookie2->Path().Length() - cookie1->Path().Length();
  if (rv == 0) {
    // when path lengths match, older cookies should be listed first.  this is
    // required for backwards compatibility since some websites erroneously
    // depend on receiving cookies in the order in which they were sent to the
    // browser!  see bug 236772.
    rv = cookie1->CreationTime() - cookie2->CreationTime();
  }
  return rv;
}

// comparison function for sorting cookies by lastAccessed time, with most-
// recently-used cookies listed first.
PR_STATIC_CALLBACK(int)
compareCookiesForWriting(const void *aElement1,
                         const void *aElement2,
                         void       *aData)
{
  const nsCookie *cookie1 = NS_STATIC_CAST(const nsCookie*, aElement1);
  const nsCookie *cookie2 = NS_STATIC_CAST(const nsCookie*, aElement2);

  // we may have overflow problems returning the result directly, so we need branches
  nsInt64 difference = cookie2->LastAccessed() - cookie1->LastAccessed();
  return (difference > nsInt64(0)) ? 1 : (difference < nsInt64(0)) ? -1 : 0;
}

/******************************************************************************
 * nsCookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

nsCookieService *nsCookieService::gCookieService = nsnull;

nsCookieService*
nsCookieService::GetSingleton()
{
  if (gCookieService) {
    NS_ADDREF(gCookieService);
    return gCookieService;
  }

  // Create a new singleton nsCookieService.
  // We AddRef only once since XPCOM has rules about the ordering of module
  // teardowns - by the time our module destructor is called, it's too late to
  // Release our members (e.g. nsIObserverService and nsIPrefBranch), since GC
  // cycles have already been completed and would result in serious leaks.
  // See bug 209571.
  gCookieService = new nsCookieService();
  if (gCookieService) {
    NS_ADDREF(gCookieService);
    if (NS_FAILED(gCookieService->Init())) {
      NS_RELEASE(gCookieService);
    }
  }

  return gCookieService;
}

/******************************************************************************
 * nsCookieService impl:
 * public methods
 ******************************************************************************/

NS_IMPL_ISUPPORTS5(nsCookieService,
                   nsICookieService,
                   nsICookieManager,
                   nsICookieManager2,
                   nsIObserver,
                   nsISupportsWeakReference)

nsCookieService::nsCookieService()
 : mCookieCount(0)
 , mCookieChanged(PR_FALSE)
 , mCookieIconVisible(PR_FALSE)
 , mCookiesPermissions(BEHAVIOR_ACCEPT)
 , mMaxNumberOfCookies(kMaxNumberOfCookies)
 , mMaxCookiesPerHost(kMaxCookiesPerHost)
{
}

nsresult
nsCookieService::Init()
{
  if (!mHostTable.Init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // init our pref and observer
  nsCOMPtr<nsIPrefBranchInternal> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kPrefCookiesPermissions, this, PR_TRUE);
    prefBranch->AddObserver(kPrefMaxNumberOfCookies, this, PR_TRUE);
    prefBranch->AddObserver(kPrefMaxCookiesPerHost,  this, PR_TRUE);
    PrefChanged(prefBranch);
  }

  // cache mCookieFile
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mCookieFile));
  if (mCookieFile) {
    mCookieFile->AppendNative(NS_LITERAL_CSTRING(kCookieFileName));
  }

  Read();

  mObserverService = do_GetService("@mozilla.org/observer-service;1");
  if (mObserverService) {
    mObserverService->AddObserver(this, "profile-before-change", PR_TRUE);
    mObserverService->AddObserver(this, "profile-do-change", PR_TRUE);
    mObserverService->AddObserver(this, "cookieIcon", PR_TRUE);
  }

  mPermissionService = do_GetService(NS_COOKIEPERMISSION_CONTRACTID);

  return NS_OK;
}

nsCookieService::~nsCookieService()
{
  gCookieService = nsnull;

  if (mWriteTimer)
    mWriteTimer->Cancel();

  // clean up memory
  RemoveAllFromMemory();
}

NS_IMETHODIMP
nsCookieService::Observe(nsISupports     *aSubject,
                         const char      *aTopic,
                         const PRUnichar *aData)
{
  // check the topic
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.
    if (mWriteTimer) {
      mWriteTimer->Cancel();
      mWriteTimer = 0;
    }

    if (!nsCRT::strcmp(aData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      RemoveAllFromMemory();
      // delete the cookie file
      if (mCookieFile) {
        mCookieFile->Remove(PR_FALSE);
      }
    } else {
      Write();
      RemoveAllFromMemory();
    }

  } else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // The profile has already changed.    
    // Now just read them from the new profile location.
    // we also need to update the cached cookie file location
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mCookieFile));
    if (NS_SUCCEEDED(rv)) {
      mCookieFile->AppendNative(NS_LITERAL_CSTRING(kCookieFileName));
    }
    Read();

  } else if (!nsCRT::strcmp(aTopic, "cookieIcon")) {
    // this is an evil trick to avoid the blatant inefficiency of
    // (!nsCRT::strcmp(aData, NS_LITERAL_STRING("on").get()))
    mCookieIconVisible = (aData[0] == 'o' && aData[1] == 'n' && aData[2] == '\0');

  } else if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    if (prefBranch)
      PrefChanged(prefBranch);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI     *aHostURI,
                                 nsIChannel *aChannel,
                                 char       **aCookie)
{
  // try to determine first party URI
  nsCOMPtr<nsIURI> firstURI;
  if (aChannel) {
    nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(aChannel);
    if (httpInternal)
      httpInternal->GetDocumentURI(getter_AddRefs(firstURI));
  }

  return GetCookieStringFromHttp(aHostURI, firstURI, aChannel, aCookie);
}

// helper function for GetCookieStringFromHttp
static inline PRBool ispathdelimiter(char c) { return c == '/' || c == '?' || c == '#' || c == ';'; }

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIChannel *aChannel,
                                         char       **aCookie)
{
  *aCookie = nsnull;

  if (!aHostURI) {
    COOKIE_LOGFAILURE(GET_COOKIE, nsnull, nsnull, "host URI is null");
    return NS_OK;
  }

  // check default prefs
  nsCookiePolicy cookiePolicy; // we don't use this here... just a placeholder
  nsCookieStatus cookieStatus = CheckPrefs(aHostURI, aFirstURI, aChannel, nsnull, cookiePolicy);
  // for GetCookie(), we don't fire rejection notifications.
  switch (cookieStatus) {
  case nsICookie::STATUS_REJECTED:
  case STATUS_REJECTED_WITH_ERROR:
    return NS_OK;
  }

  // get host and path from the nsIURI
  // note: there was a "check if host has embedded whitespace" here.
  // it was removed since this check was added into the nsIURI impl (bug 146094).
  nsCAutoString hostFromURI, pathFromURI;
  if (NS_FAILED(aHostURI->GetAsciiHost(hostFromURI)) ||
      NS_FAILED(aHostURI->GetPath(pathFromURI))) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, nsnull, "couldn't get host/path from URI");
    return NS_OK;
  }
  // trim trailing dots
  hostFromURI.Trim(".");
  // insert a leading dot, so we begin the hash lookup with the
  // equivalent domain cookie host
  hostFromURI.Insert(NS_LITERAL_CSTRING("."), 0);
  ToLowerCase(hostFromURI);

  // check if aHostURI is using an https secure protocol.
  // if it isn't, then we can't send a secure cookie over the connection.
  // if SchemeIs fails, assume an insecure connection, to be on the safe side
  PRBool isSecure;
  if NS_FAILED(aHostURI->SchemeIs("https", &isSecure)) {
    isSecure = PR_FALSE;
  }

  nsCookie *cookie;
  nsAutoVoidArray foundCookieList;
  nsInt64 currentTime = NOW_IN_SECONDS;
  const char *currentDot = hostFromURI.get();
  const char *nextDot = currentDot + 1;

  // begin hash lookup, walking up the subdomain levels.
  // we use nextDot to force a lookup of the original host (without leading dot).
  do {
    nsCookieEntry *entry = mHostTable.GetEntry(currentDot);
    cookie = entry ? entry->Head() : nsnull;
    for (; cookie; cookie = cookie->Next()) {
      // if the cookie is secure and the host scheme isn't, we can't send it
      if (cookie->IsSecure() && !isSecure) {
        continue;
      }

      // calculate cookie path length, excluding trailing '/'
      PRUint32 cookiePathLen = cookie->Path().Length();
      if (cookiePathLen > 0 && cookie->Path().Last() == '/') {
        --cookiePathLen;
      }

      // if the nsIURI path is shorter than the cookie path, don't send it back
      if (!StringBeginsWith(pathFromURI, Substring(cookie->Path(), 0, cookiePathLen))) {
        continue;
      }

      if (pathFromURI.Length() > cookiePathLen &&
          !ispathdelimiter(pathFromURI.CharAt(cookiePathLen))) {
        /*
         * |ispathdelimiter| tests four cases: '/', '?', '#', and ';'.
         * '/' is the "standard" case; the '?' test allows a site at host/abc?def
         * to receive a cookie that has a path attribute of abc.  this seems
         * strange but at least one major site (citibank, bug 156725) depends
         * on it.  The test for # and ; are put in to proactively avoid problems
         * with other sites - these are the only other chars allowed in the path.
         */
        continue;
      }

      // check if the cookie has expired
      if (cookie->Expiry() <= currentTime) {
        continue;
      }

      // all checks passed - add to list and update lastAccessed stamp of cookie
      foundCookieList.AppendElement(cookie);
      cookie->SetLastAccessed(currentTime);
    }

    currentDot = nextDot;
    if (currentDot)
      nextDot = strchr(currentDot + 1, '.');

  } while (currentDot);

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  foundCookieList.Sort(compareCookiesForSending, nsnull);

  nsCAutoString cookieData;
  PRInt32 count = foundCookieList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie = NS_STATIC_CAST(nsCookie*, foundCookieList.ElementAt(i));

    // check if we have anything to write
    if (!cookie->Name().IsEmpty() || !cookie->Value().IsEmpty()) {
      // if we've already added a cookie to the return list, append a "; " so
      // that subsequent cookies are delimited in the final list.
      if (!cookieData.IsEmpty()) {
        cookieData.AppendLiteral("; ");
      }

      if (!cookie->Name().IsEmpty()) {
        // we have a name and value - write both
        cookieData += cookie->Name() + NS_LITERAL_CSTRING("=") + cookie->Value();
      } else {
        // just write value
        cookieData += cookie->Value();
      }
    }
  }

  // it's wasteful to alloc a new string; but we have no other choice, until we
  // fix the callers to use nsACStrings.
  if (!cookieData.IsEmpty()) {
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, cookieData, nsnull);
    *aCookie = ToNewCString(cookieData);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI     *aHostURI,
                                 nsIPrompt  *aPrompt,
                                 const char *aCookieHeader,
                                 nsIChannel *aChannel)
{
  // try to determine first party URI
  nsCOMPtr<nsIURI> firstURI;

  if (aChannel) {
    nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(aChannel);
    if (httpInternal)
      httpInternal->GetDocumentURI(getter_AddRefs(firstURI));
  }

  return SetCookieStringFromHttp(aHostURI, firstURI, aPrompt, aCookieHeader, nsnull, aChannel);
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIPrompt  *aPrompt,
                                         const char *aCookieHeader,
                                         const char *aServerTime,
                                         nsIChannel *aChannel) 
{
  if (!aHostURI) {
    COOKIE_LOGFAILURE(SET_COOKIE, nsnull, aCookieHeader, "host URI is null");
    return NS_OK;
  }

  // check default prefs
  nsCookiePolicy cookiePolicy = nsICookie::POLICY_UNKNOWN;
  nsCookieStatus cookieStatus = CheckPrefs(aHostURI, aFirstURI, aChannel, aCookieHeader, cookiePolicy);
  // fire a notification if cookie was rejected (but not if there was an error)
  switch (cookieStatus) {
  case nsICookie::STATUS_REJECTED:
    NotifyRejected(aHostURI);
  case STATUS_REJECTED_WITH_ERROR:
    return NS_OK;
  }

  // parse server local time. this is not just done here for efficiency
  // reasons - if there's an error parsing it, and we need to default it
  // to the current time, we must do it here since the current time in
  // SetCookieInternal() will change for each cookie processed (e.g. if the
  // user is prompted).
  nsInt64 serverTime;
  PRTime tempServerTime;
  if (aServerTime && PR_ParseTimeString(aServerTime, PR_TRUE, &tempServerTime) == PR_SUCCESS) {
    serverTime = nsInt64(tempServerTime) / USEC_PER_SEC;
  } else {
    serverTime = NOW_IN_SECONDS;
  }

  // switch to a nice string type now, and process each cookie in the header
  nsDependentCString cookieHeader(aCookieHeader);
  while (SetCookieInternal(aHostURI, aChannel,
                           cookieHeader, serverTime,
                           cookieStatus, cookiePolicy));

  // write out the cookie file
  LazyWrite();
  return NS_OK;
}

void
nsCookieService::LazyWrite()
{
  if (mWriteTimer) {
    mWriteTimer->SetDelay(kLazyWriteTimeout);
  } else {
    mWriteTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mWriteTimer) {
      mWriteTimer->InitWithFuncCallback(DoLazyWrite, this, kLazyWriteTimeout,
                                        nsITimer::TYPE_ONE_SHOT);
    }
  }
}

void
nsCookieService::DoLazyWrite(nsITimer *aTimer,
                             void     *aClosure)
{
  nsCookieService *service = NS_REINTERPRET_CAST(nsCookieService*, aClosure);
  service->Write();
  service->mWriteTimer = 0;
}

// notify observers that a cookie was rejected due to the users' prefs.
void
nsCookieService::NotifyRejected(nsIURI *aHostURI)
{
  if (mObserverService) {
    mObserverService->NotifyObservers(aHostURI, "cookie-rejected", nsnull);
    // the cookieIcon notification is now deprecated, in favor of cookie-rejected.
    // we still need this until consumers can be switched over.
    mObserverService->NotifyObservers(nsnull, "cookieIcon", NS_LITERAL_STRING("on").get());
  }
  mCookieIconVisible = PR_TRUE;
}

// notify observers that the cookie list changed. there are four possible
// values for aData:
// "deleted" means a cookie was deleted. aCookie is the deleted cookie.
// "added"   means a cookie was added. aCookie is the added cookie.
// "changed" means a cookie was altered. aCookie is the new cookie.
// "cleared" means the entire cookie list was cleared. aCookie is null.
void
nsCookieService::NotifyChanged(nsICookie2      *aCookie,
                               const PRUnichar *aData)
{
  mCookieChanged = PR_TRUE;

  if (mObserverService) {
    mObserverService->NotifyObservers(aCookie, "cookie-changed", aData);
    // the cookieChanged notification is now deprecated. use cookie-changed instead.
    mObserverService->NotifyObservers(nsnull, "cookieChanged", NS_LITERAL_STRING("cookies").get());
  }

  // the cookieIcon notification is now deprecated, but we still need
  // this until consumers can be fixed. to see if cookies have been
  // downgraded or flagged, listen to cookie-changed directly.
  if (!nsCRT::strcmp(aData, NS_LITERAL_STRING("added").get()) ||
      !nsCRT::strcmp(aData, NS_LITERAL_STRING("changed").get())) {
    nsCookieStatus status;
    aCookie->GetStatus(&status);
    if (status == nsICookie::STATUS_DOWNGRADED ||
        status == nsICookie::STATUS_FLAGGED) {
      mCookieIconVisible = PR_TRUE;
      if (mObserverService)
        mObserverService->NotifyObservers(nsnull, "cookieIcon", NS_LITERAL_STRING("on").get());
    }
  }
}

// this method is deprecated. listen to the cookie-changed notification instead.
NS_IMETHODIMP
nsCookieService::GetCookieIconIsVisible(PRBool *aIsVisible)
{
  *aIsVisible = mCookieIconVisible;
  return NS_OK;
}

/******************************************************************************
 * nsCookieService:
 * pref observer impl
 ******************************************************************************/

void
nsCookieService::PrefChanged(nsIPrefBranch *aPrefBranch)
{
  PRInt32 val;
  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookiesPermissions, &val)))
    mCookiesPermissions = LIMIT(val, 0, 3, 0);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxNumberOfCookies, &val)))
    mMaxNumberOfCookies = LIMIT(val, 0, 0xFFFF, 0xFFFF);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxCookiesPerHost, &val)))
    mMaxCookiesPerHost = LIMIT(val, 0, 0xFFFF, 0xFFFF);
}

/******************************************************************************
 * nsICookieManager impl:
 * nsICookieManager
 ******************************************************************************/

NS_IMETHODIMP
nsCookieService::RemoveAll()
{
  RemoveAllFromMemory();
  NotifyChanged(nsnull, NS_LITERAL_STRING("cleared").get());
  Write();
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
COMArrayCallback(nsCookieEntry *aEntry,
                 void          *aArg)
{
  for (nsCookie *cookie = aEntry->Head(); cookie; cookie = cookie->Next()) {
    NS_STATIC_CAST(nsCOMArray<nsICookie>*, aArg)->AppendObject(cookie);
  }
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsCookieService::GetEnumerator(nsISimpleEnumerator **aEnumerator)
{
  RemoveExpiredCookies(NOW_IN_SECONDS);

  nsCOMArray<nsICookie> cookieList(mCookieCount);
  mHostTable.EnumerateEntries(COMArrayCallback, &cookieList);

  return NS_NewArrayEnumerator(aEnumerator, cookieList);
}

NS_IMETHODIMP
nsCookieService::Add(const nsACString &aDomain,
                     const nsACString &aPath,
                     const nsACString &aName,
                     const nsACString &aValue,
                     PRBool            aIsSecure,
                     PRBool            aIsSession,
                     PRInt64           aExpiry)
{
  nsInt64 currentTime = NOW_IN_SECONDS;

  nsRefPtr<nsCookie> cookie =
    nsCookie::Create(aName, aValue, aDomain, aPath,
                     nsInt64(aExpiry),
                     currentTime,
                     aIsSession,
                     aIsSecure,
                     nsICookie::STATUS_UNKNOWN,
                     nsICookie::POLICY_UNKNOWN);
  if (!cookie) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AddInternal(cookie, currentTime, nsnull, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Remove(const nsACString &aHost,
                        const nsACString &aName,
                        const nsACString &aPath,
                        PRBool           aBlocked)
{
  nsListIter matchIter;
  if (FindCookie(PromiseFlatCString(aHost),
                 PromiseFlatCString(aName),
                 PromiseFlatCString(aPath),
                 matchIter)) {
    nsRefPtr<nsCookie> cookie = matchIter.current;
    RemoveCookieFromList(matchIter);
    NotifyChanged(cookie, NS_LITERAL_STRING("deleted").get());

    // check if we need to add the host to the permissions blacklist.
    if (aBlocked && mPermissionService) {
      nsCAutoString host(NS_LITERAL_CSTRING("http://") + cookie->RawHost());
      nsCOMPtr<nsIURI> uri;
      NS_NewURI(getter_AddRefs(uri), host);

      if (uri)
        mPermissionService->SetAccess(uri, nsICookiePermission::ACCESS_DENY);
    }

    LazyWrite();
  }
  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private file I/O functions
 ******************************************************************************/

nsresult
nsCookieService::Read()
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), mCookieFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  static NS_NAMED_LITERAL_CSTRING(kTrue, "TRUE");

  nsCAutoString buffer;
  PRBool isMore = PR_TRUE;
  PRInt32 hostIndex = 0, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
  nsASingleFragmentCString::char_iterator iter;
  PRInt32 numInts;
  PRInt64 expires;
  PRBool isDomain;
  nsInt64 currentTime = NOW_IN_SECONDS;
  // we use lastAccessedCounter to keep cookies in recently-used order,
  // so we start by initializing to currentTime (somewhat arbitrary)
  nsInt64 lastAccessedCounter = currentTime;
  nsCookie *newCookie;

  /* file format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is "TRUE" or "FALSE" (default to "FALSE")
   * isSecure is "TRUE" or "FALSE" (default to "TRUE")
   * expires is a PRInt64 integer
   * note 1: cookie can contain tabs.
   * note 2: cookies are written in order of lastAccessed time:
   *         most-recently used come first; least-recently-used come last.
   */

  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    // this is a cheap, cheesy way of parsing a tab-delimited line into
    // string indexes, which can be lopped off into substrings. just for
    // purposes of obfuscation, it also checks that each token was found.
    // todo: use iterators?
    if ((isDomainIndex = buffer.FindChar('\t', hostIndex)     + 1) == 0 ||
        (pathIndex     = buffer.FindChar('\t', isDomainIndex) + 1) == 0 ||
        (secureIndex   = buffer.FindChar('\t', pathIndex)     + 1) == 0 ||
        (expiresIndex  = buffer.FindChar('\t', secureIndex)   + 1) == 0 ||
        (nameIndex     = buffer.FindChar('\t', expiresIndex)  + 1) == 0 ||
        (cookieIndex   = buffer.FindChar('\t', nameIndex)     + 1) == 0) {
      continue;
    }

    // check the expirytime first - if it's expired, ignore
    // nullstomp the trailing tab, to avoid copying the string
    buffer.BeginWriting(iter);
    *(iter += nameIndex - 1) = char(0);
    numInts = PR_sscanf(buffer.get() + expiresIndex, "%lld", &expires);
    if (numInts != 1 || nsInt64(expires) < currentTime) {
      continue;
    }

    isDomain = Substring(buffer, isDomainIndex, pathIndex - isDomainIndex - 1).Equals(kTrue);
    const nsASingleFragmentCString &host = Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    // check for bad legacy cookies (domain not starting with a dot, or containing a port),
    // and discard
    if (isDomain && !host.IsEmpty() && host.First() != '.' ||
        host.FindChar(':') != kNotFound) {
      continue;
    }

    // create a new nsCookie and assign the data.
    newCookie =
      nsCookie::Create(Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
                       Substring(buffer, cookieIndex, buffer.Length() - cookieIndex),
                       host,
                       Substring(buffer, pathIndex, secureIndex - pathIndex - 1),
                       nsInt64(expires),
                       lastAccessedCounter,
                       PR_FALSE,
                       Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).Equals(kTrue),
                       nsICookie::STATUS_UNKNOWN,
                       nsICookie::POLICY_UNKNOWN);
    if (!newCookie) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // trick: keep the cookies in most-recently-used order,
    // by successively decrementing the lastAccessed time
    lastAccessedCounter -= nsInt64(1);

    if (!AddCookieToList(newCookie)) {
      // It is purpose that created us; purpose that connects us;
      // purpose that pulls us; that guides us; that drives us.
      // It is purpose that defines us; purpose that binds us.
      // When a cookie no longer has purpose, it has a choice:
      // it can return to the source to be deleted, or it can go
      // into exile, and stay hidden inside the Matrix.
      // Let's choose deletion.
      delete newCookie;
    }
  }

  mCookieChanged = PR_FALSE;
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
cookieListCallback(nsCookieEntry *aEntry,
                   void          *aArg)
{
  for (nsCookie *cookie = aEntry->Head(); cookie; cookie = cookie->Next()) {
    NS_STATIC_CAST(nsVoidArray*, aArg)->AppendElement(cookie);
  }
  return PL_DHASH_NEXT;
}

nsresult
nsCookieService::Write()
{
  if (!mCookieChanged) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIOutputStream> fileOutputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), mCookieFile);
  if (NS_FAILED(rv)) {
    NS_ERROR("failed to open cookies.txt for writing");
    return rv;
  }

  // get a buffered output stream 4096 bytes big, to optimize writes
  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream), fileOutputStream, 4096);
  if (NS_FAILED(rv)) {
    return rv;
  }

  static const char kHeader[] =
      "# HTTP Cookie File\n"
      "# http://www.netscape.com/newsref/std/cookie_spec.html\n"
      "# This is a generated file!  Do not edit.\n"
      "# To delete cookies, use the Cookie Manager.\n\n";
  // note: kTrue and kFalse have leading/trailing tabs already added
  static const char kTrue[] = "\tTRUE\t";
  static const char kFalse[] = "\tFALSE\t";
  static const char kTab[] = "\t";
  static const char kNew[] = "\n";

  // create a new nsVoidArray to hold the cookie list, and sort it
  // such that least-recently-used cookies come last
  nsVoidArray sortedCookieList(mCookieCount);
  mHostTable.EnumerateEntries(cookieListCallback, &sortedCookieList);
  sortedCookieList.Sort(compareCookiesForWriting, nsnull);

  bufferedOutputStream->Write(kHeader, sizeof(kHeader) - 1, &rv);

  /* file format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * isDomain is "TRUE" or "FALSE"
   * isSecure is "TRUE" or "FALSE"
   * expires is a PRInt64 integer
   * note 1: cookie can contain tabs.
   * note 2: cookies are written in order of lastAccessed time:
   *         most-recently used come first; least-recently-used come last.
   */
  nsCookie *cookie;
  nsInt64 currentTime = NOW_IN_SECONDS;
  char dateString[22];
  PRUint32 dateLen;
  for (PRUint32 i = 0; i < mCookieCount; ++i) {
    cookie = NS_STATIC_CAST(nsCookie*, sortedCookieList.ElementAt(i));

    // don't write entry if cookie has expired, or is a session cookie
    if (cookie->IsSession() || cookie->Expiry() <= currentTime) {
      continue;
    }

    bufferedOutputStream->Write(cookie->Host().get(), cookie->Host().Length(), &rv);
    if (cookie->IsDomain()) {
      bufferedOutputStream->Write(kTrue, sizeof(kTrue) - 1, &rv);
    } else {
      bufferedOutputStream->Write(kFalse, sizeof(kFalse) - 1, &rv);
    }
    bufferedOutputStream->Write(cookie->Path().get(), cookie->Path().Length(), &rv);
    if (cookie->IsSecure()) {
      bufferedOutputStream->Write(kTrue, sizeof(kTrue) - 1, &rv);
    } else {
      bufferedOutputStream->Write(kFalse, sizeof(kFalse) - 1, &rv);
    }
    dateLen = PR_snprintf(dateString, sizeof(dateString), "%lld", PRInt64(cookie->Expiry()));
    bufferedOutputStream->Write(dateString, dateLen, &rv);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &rv);
    bufferedOutputStream->Write(cookie->Name().get(), cookie->Name().Length(), &rv);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &rv);
    bufferedOutputStream->Write(cookie->Value().get(), cookie->Value().Length(), &rv);
    bufferedOutputStream->Write(kNew, sizeof(kNew) - 1, &rv);
  }

  mCookieChanged = PR_FALSE;
  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private GetCookie/SetCookie helpers
 ******************************************************************************/

// processes a single cookie, and returns PR_TRUE if there are more cookies
// to be processed
PRBool
nsCookieService::SetCookieInternal(nsIURI             *aHostURI,
                                   nsIChannel         *aChannel,
                                   nsDependentCString &aCookieHeader,
                                   nsInt64            aServerTime,
                                   nsCookieStatus     aStatus,
                                   nsCookiePolicy     aPolicy)
{
  // keep a |const char*| version of the unmodified aCookieHeader,
  // for logging purposes
  const char *cookieHeader = aCookieHeader.get();

  // create a stack-based nsCookieAttributes, to store all the
  // attributes parsed from the cookie
  nsCookieAttributes cookieAttributes;

  // init expiryTime such that session cookies won't prematurely expire
  cookieAttributes.expiryTime = LL_MAXINT;

  // newCookie says whether there are multiple cookies in the header; so we can handle them separately.
  // after this function, we don't need the cookieHeader string for processing this cookie anymore;
  // so this function uses it as an outparam to point to the next cookie, if there is one.
  const PRBool newCookie = ParseAttributes(aCookieHeader, cookieAttributes);

  // reject cookie if it's over the size limit, per RFC2109
  if ((cookieAttributes.name.Length() + cookieAttributes.value.Length()) > kMaxBytesPerCookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookie too big (> 4kb)");
    return newCookie;
  }

  // calculate expiry time of cookie. we need to pass in cookieStatus, since
  // the cookie may have been downgraded to a session cookie by p3p.
  const nsInt64 currentTime = NOW_IN_SECONDS;
  cookieAttributes.isSession = GetExpiry(cookieAttributes, aServerTime,
                                         currentTime, aStatus);

  // domain & path checks
  if (!CheckDomain(cookieAttributes, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "failed the domain tests");
    return newCookie;
  }
  if (!CheckPath(cookieAttributes, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "failed the path tests");
    return newCookie;
  }

  // create a new nsCookie and copy attributes
  nsRefPtr<nsCookie> cookie =
    nsCookie::Create(cookieAttributes.name,
                     cookieAttributes.value,
                     cookieAttributes.host,
                     cookieAttributes.path,
                     cookieAttributes.expiryTime,
                     currentTime,
                     cookieAttributes.isSession,
                     cookieAttributes.isSecure,
                     aStatus,
                     aPolicy);
  if (!cookie) {
    return newCookie;
  }

  // check permissions from site permission list, or ask the user,
  // to determine if we can set the cookie
  if (mPermissionService) {
    PRBool permission;
    // we need to think about prompters/parent windows here - TestPermission
    // needs one to prompt, so right now it has to fend for itself to get one
    mPermissionService->CanSetCookie(aHostURI,
                                     aChannel,
                                     NS_STATIC_CAST(nsICookie2*, NS_STATIC_CAST(nsCookie*, cookie)),
                                     &cookieAttributes.isSession,
                                     &cookieAttributes.expiryTime.mValue,
                                     &permission);
    if (!permission) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookie rejected by permission manager");
      NotifyRejected(aHostURI);
      return newCookie;
    }

    // update isSession and expiry attributes, in case they changed
    cookie->SetIsSession(cookieAttributes.isSession);
    cookie->SetExpiry(cookieAttributes.expiryTime);
  }

  // add the cookie to the list. AddInternal() takes care of logging.
  AddInternal(cookie, NOW_IN_SECONDS, aHostURI, cookieHeader);
  return newCookie;
}

// this is a backend function for adding a cookie to the list, via SetCookie.
// also used in the cookie manager, for profile migration from IE.
// it either replaces an existing cookie; or adds the cookie to the hashtable,
// and deletes a cookie (if maximum number of cookies has been
// reached). also performs list maintenance by removing expired cookies.
void
nsCookieService::AddInternal(nsCookie   *aCookie,
                             nsInt64    aCurrentTime,
                             nsIURI     *aHostURI,
                             const char *aCookieHeader)
{
  nsListIter matchIter;
  const PRBool foundCookie =
    FindCookie(aCookie->Host(), aCookie->Name(), aCookie->Path(), matchIter);

  nsRefPtr<nsCookie> oldCookie;
  if (foundCookie) {
    oldCookie = matchIter.current;
    RemoveCookieFromList(matchIter);

    // check if the cookie has expired
    if (aCookie->Expiry() <= aCurrentTime) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "previously stored cookie was deleted");
      NotifyChanged(oldCookie, NS_LITERAL_STRING("deleted").get());
      return;
    }

    // preserve creation time of cookie
    if (oldCookie) {
      aCookie->SetCreationTime(oldCookie->CreationTime());
    }

  } else {
    // check if cookie has already expired
    if (aCookie->Expiry() <= aCurrentTime) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "cookie has already expired");
      return;
    }

    // check if we have to delete an old cookie.
    nsEnumerationData data(aCurrentTime, LL_MAXINT);
    if (CountCookiesFromHost(aCookie, data) >= mMaxCookiesPerHost) {
      // remove the oldest cookie from host
      oldCookie = data.iter.current;
      RemoveCookieFromList(data.iter);

    } else if (mCookieCount >= mMaxNumberOfCookies) {
      // try to make room, by removing expired cookies
      RemoveExpiredCookies(aCurrentTime);

      // check if we still have to get rid of something
      if (mCookieCount >= mMaxNumberOfCookies) {
        // find the position of the oldest cookie, and remove it
        data.oldestTime = LL_MAXINT;
        FindOldestCookie(data);
        oldCookie = data.iter.current;
        RemoveCookieFromList(data.iter);
      }
    }

    // if we deleted an old cookie, notify consumers
    if (oldCookie)
      NotifyChanged(oldCookie, NS_LITERAL_STRING("deleted").get());
  }

  // add the cookie to head of list
  AddCookieToList(aCookie);
  NotifyChanged(aCookie, foundCookie ? NS_LITERAL_STRING("changed").get()
                                     : NS_LITERAL_STRING("added").get());

  COOKIE_LOGSUCCESS(SET_COOKIE, aHostURI, aCookieHeader, aCookie);
}

/******************************************************************************
 * nsCookieService impl:
 * private cookie header parsing functions
 ******************************************************************************/

// The following comment block elucidates the function of ParseAttributes.
/******************************************************************************
 ** Augmented BNF, modified from RFC2109 Section 4.2.2 and RFC2616 Section 2.1
 ** please note: this BNF deviates from both specifications, and reflects this
 ** implementation. <bnf> indicates a reference to the defined grammer "bnf".

 ** Differences from RFC2109/2616 and explanations:
    1. implied *LWS
         The grammar described by this specification is word-based. Except
         where noted otherwise, linear white space (<LWS>) can be included
         between any two adjacent words (token or quoted-string), and
         between adjacent words and separators, without changing the
         interpretation of a field.
       <LWS> according to spec is SP|HT|CR|LF, but here, we allow only SP | HT.

    2. We use CR | LF as cookie separators, not ',' per spec, since ',' is in
       common use inside values.

    3. tokens and values have looser restrictions on allowed characters than
       spec. This is also due to certain characters being in common use inside
       values. We allow only '=' to separate token/value pairs, and ';' to
       terminate tokens or values. <LWS> is allowed within tokens and values
       (see bug 206022).

    4. where appropriate, full <OCTET>s are allowed, where the spec dictates to
       reject control chars or non-ASCII chars. This is erring on the loose
       side, since there's probably no good reason to enforce this strictness.

    5. cookie <NAME> is optional, where spec requires it. This is a fairly
       trivial case, but allows the flexibility of setting only a cookie <VALUE>
       with a blank <NAME> and is required by some sites (see bug 169091).

 ** Begin BNF:
    token         = 1*<any allowed-chars except separators>
    value         = token-value | quoted-string
    token-value   = 1*<any allowed-chars except value-sep>
    quoted-string = ( <"> *( qdtext | quoted-pair ) <"> )
    qdtext        = <any allowed-chars except <">>             ; CR | LF removed by necko
    quoted-pair   = "\" <any OCTET except NUL or cookie-sep>   ; CR | LF removed by necko
    separators    = ";" | "="
    value-sep     = ";"
    cookie-sep    = CR | LF
    allowed-chars = <any OCTET except NUL or cookie-sep>
    OCTET         = <any 8-bit sequence of data>
    LWS           = SP | HT
    NUL           = <US-ASCII NUL, null control character (0)>
    CR            = <US-ASCII CR, carriage return (13)>
    LF            = <US-ASCII LF, linefeed (10)>
    SP            = <US-ASCII SP, space (32)>
    HT            = <US-ASCII HT, horizontal-tab (9)>

    set-cookie    = "Set-Cookie:" cookies
    cookies       = cookie *( cookie-sep cookie )
    cookie        = [NAME "="] VALUE *(";" cookie-av)    ; cookie NAME/VALUE must come first
    NAME          = token                                ; cookie name
    VALUE         = value                                ; cookie value
    cookie-av     = token ["=" value]

    valid values for cookie-av (checked post-parsing) are:
    cookie-av     = "Path"    "=" value
                  | "Domain"  "=" value
                  | "Expires" "=" value
                  | "Max-Age" "=" value
                  | "Comment" "=" value
                  | "Version" "=" value
                  | "Secure"

******************************************************************************/

// helper functions for GetTokenValue
static inline PRBool iswhitespace     (char c) { return c == ' '  || c == '\t'; }
static inline PRBool isterminator     (char c) { return c == '\n' || c == '\r'; }
static inline PRBool isquoteterminator(char c) { return isterminator(c) || c == '"'; }
static inline PRBool isvalueseparator (char c) { return isterminator(c) || c == ';'; }
static inline PRBool istokenseparator (char c) { return isvalueseparator(c) || c == '='; }

// Parse a single token/value pair.
// Returns PR_TRUE if a cookie terminator is found, so caller can parse new cookie.
PRBool
nsCookieService::GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter,
                               nsASingleFragmentCString::const_char_iterator &aEndIter,
                               nsDependentCSubstring                         &aTokenString,
                               nsDependentCSubstring                         &aTokenValue,
                               PRBool                                        &aEqualsFound)
{
  nsASingleFragmentCString::const_char_iterator start, lastSpace;
  // initialize value string to clear garbage
  aTokenValue.Rebind(aIter, aIter);

  // find <token>, including any <LWS> between the end-of-token and the
  // token separator. we'll remove trailing <LWS> next
  while (aIter != aEndIter && iswhitespace(*aIter))
    ++aIter;
  start = aIter;
  while (aIter != aEndIter && !istokenseparator(*aIter))
    ++aIter;

  // remove trailing <LWS>; first check we're not at the beginning
  lastSpace = aIter;
  if (lastSpace != start) {
    while (--lastSpace != start && iswhitespace(*lastSpace));
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter));

    start = aIter;

    if (*aIter == '"') {
      // process <quoted-string>
      // (note: cookie terminators, CR | LF, can't happen:
      // they're removed by necko before the header gets here)
      // assume value mangled if no terminating '"', return
      while (++aIter != aEndIter && !isquoteterminator(*aIter)) {
        // if <qdtext> (backwhacked char), skip over it. this allows '\"' in <quoted-string>.
        // we increment once over the backwhack, nullcheck, then continue to the 'while',
        // which increments over the backwhacked char. one exception - we don't allow
        // CR | LF here either (see above about necko)
        if (*aIter == '\\' && (++aIter == aEndIter || isterminator(*aIter)))
          break;
      }

      if (aIter != aEndIter && !isterminator(*aIter)) {
        // include terminating quote in attribute string
        aTokenValue.Rebind(start, ++aIter);
        // skip to next ';'
        while (aIter != aEndIter && !isvalueseparator(*aIter))
          ++aIter;
      }
    } else {
      // process <token-value>
      // just look for ';' to terminate ('=' allowed)
      while (aIter != aEndIter && !isvalueseparator(*aIter))
        ++aIter;

      // remove trailing <LWS>; first check we're not at the beginning
      if (aIter != start) {
        lastSpace = aIter;
        while (--lastSpace != start && iswhitespace(*lastSpace));
        aTokenValue.Rebind(start, ++lastSpace);
      }
    }
  }

  // aIter is on ';', or terminator, or EOS
  if (aIter != aEndIter) {
    // if on terminator, increment past & return PR_TRUE to process new cookie
    if (isterminator(*aIter)) {
      ++aIter;
      return PR_TRUE;
    }
    // fall-through: aIter is on ';', increment and return PR_FALSE
    ++aIter;
  }
  return PR_FALSE;
}

// Parses attributes from cookie header. expires/max-age attributes aren't folded into the
// cookie struct here, because we don't know which one to use until we've parsed the header.
PRBool
nsCookieService::ParseAttributes(nsDependentCString &aCookieHeader,
                                 nsCookieAttributes &aCookieAttributes)
{
  static NS_NAMED_LITERAL_CSTRING(kPath,    "path"   );
  static NS_NAMED_LITERAL_CSTRING(kDomain,  "domain" );
  static NS_NAMED_LITERAL_CSTRING(kExpires, "expires");
  static NS_NAMED_LITERAL_CSTRING(kMaxage,  "max-age");
  static NS_NAMED_LITERAL_CSTRING(kSecure,  "secure" );

  nsASingleFragmentCString::const_char_iterator tempBegin, tempEnd;
  nsASingleFragmentCString::const_char_iterator cookieStart, cookieEnd;
  aCookieHeader.BeginReading(cookieStart);
  aCookieHeader.EndReading(cookieEnd);

  aCookieAttributes.isSecure = PR_FALSE;

  nsDependentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentCSubstring tokenValue (cookieStart, cookieStart);
  PRBool newCookie, equalsFound;

  // extract cookie <NAME> & <VALUE> (first attribute), and copy the strings.
  // if we find multiple cookies, return for processing
  // note: if there's no '=', we assume token is <VALUE>. this is required by
  //       some sites (see bug 169091).
  // XXX fix the parser to parse according to <VALUE> grammar for this case
  newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);
  if (equalsFound) {
    aCookieAttributes.name = tokenString;
    aCookieAttributes.value = tokenValue;
  } else {
    aCookieAttributes.value = tokenString;
  }

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);

    if (!tokenValue.IsEmpty() && *tokenValue.BeginReading(tempBegin) == '"'
                              && *tokenValue.EndReading(tempEnd) == '"') {
      // our parameter is a quoted-string; remove quotes for later parsing
      tokenValue.Rebind(++tempBegin, --tempEnd);
    }

    // decide which attribute we have, and copy the string
    if (tokenString.Equals(kPath, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.path = tokenValue;

    else if (tokenString.Equals(kDomain, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.host = tokenValue;

    else if (tokenString.Equals(kExpires, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.expires = tokenValue;

    else if (tokenString.Equals(kMaxage, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.maxage = tokenValue;

    // ignore any tokenValue for isSecure; just set the boolean
    else if (tokenString.Equals(kSecure, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.isSecure = PR_TRUE;
  }

  // rebind aCookieHeader, in case we need to process another cookie
  aCookieHeader.Rebind(cookieStart, cookieEnd);
  return newCookie;
}

/******************************************************************************
 * nsCookieService impl:
 * private domain & permission compliance enforcement functions
 ******************************************************************************/

// returns PR_TRUE if aHost is an IP address
PRBool
nsCookieService::IsIPAddress(const nsAFlatCString &aHost)
{
  PRNetAddr addr;
  return (PR_StringToNetAddr(aHost.get(), &addr) == PR_SUCCESS);
}

PRBool
nsCookieService::IsInDomain(const nsACString &aDomain,
                            const nsACString &aHost,
                            PRBool           aIsDomain)
{
  // if we have a non-domain cookie, require an exact match between domain and host.
  // RFC2109 specifies this behavior; it allows a site to prevent its subdomains
  // from accessing a cookie, for whatever reason.
  if (!aIsDomain) {
    return aDomain.Equals(aHost);
  }

  // we have a domain cookie; test the following two cases:
  /*
   * normal case for hostName = x<domainName>
   *    e.g., hostName = home.netscape.com
   *          domainName = .netscape.com
   *
   * special case for domainName = .hostName
   *    e.g., hostName = netscape.com
   *          domainName = .netscape.com
   */
  // the lengthDifference tests are for efficiency, so we do only one .Equals()
  PRUint32 domainLength = aDomain.Length();
  PRInt32 lengthDifference = aHost.Length() - domainLength;
  // case for host & domain equal
  // (e.g. .netscape.com & .netscape.com)
  // this gives us slightly more efficiency, since we don't have
  // to call up Substring().
  if (lengthDifference == 0) {
    return aDomain.Equals(aHost);
  }
  // normal case
  if (lengthDifference > 0) {
    return aDomain.Equals(Substring(aHost, lengthDifference, domainLength));
  }
  // special case
  if (lengthDifference == -1) {
    return Substring(aDomain, 1, domainLength - 1).Equals(aHost);
  }
  // no match
  return PR_FALSE;
}

PRBool
nsCookieService::IsForeign(nsIURI *aHostURI,
                           nsIURI *aFirstURI)
{
  // if aFirstURI is null, default to not foreign
  if (!aFirstURI) {
    return PR_FALSE;
  }

  // chrome URLs are never foreign (otherwise sidebar cookies won't work).
  // eventually we want to have a protocol whitelist here,
  // _or_ do something smart with nsIProtocolHandler::protocolFlags.
  PRBool isChrome = PR_FALSE;
  nsresult rv = aFirstURI->SchemeIs("chrome", &isChrome);
  if (NS_SUCCEEDED(rv) && isChrome) {
    return PR_FALSE;
  }

  // Get hosts
  nsCAutoString currentHost, firstHost;
  if (NS_FAILED(aHostURI->GetAsciiHost(currentHost)) ||
      NS_FAILED(aFirstURI->GetAsciiHost(firstHost))) {
    return PR_TRUE;
  }
  // trim trailing dots
  currentHost.Trim(".");
  firstHost.Trim(".");
  ToLowerCase(currentHost);
  ToLowerCase(firstHost);

  // determine if it's foreign. we have a new algorithm for doing this,
  // since the old behavior was broken:

  // first ensure we're not dealing with IP addresses; if we are, require an
  // exact match. we can't avoid this, otherwise the algo below will allow two
  // IP's such as 128.12.96.5 and 213.12.96.5 to match.
  if (IsIPAddress(firstHost)) {
    return !IsInDomain(firstHost, currentHost, PR_FALSE);
  }

  // next, allow a one-subdomain-level "fuzz" in the comparison. first, we need
  // to find how many subdomain levels each host has; we only do the looser
  // comparison if they have the same number of levels. e.g.
  //  firstHost = weather.yahoo.com, currentHost = cookies.yahoo.com -> match
  //  firstHost =     a.b.yahoo.com, currentHost =       b.yahoo.com -> no match
  //  firstHost =         yahoo.com, currentHost = weather.yahoo.com -> no match
  //  (since the normal test (next) will catch this case and give a match.)
  // also, we can only do this if they have >=2 subdomain levels, to avoid
  // matching yahoo.com with netscape.com (yes, this breaks for .co.nz etc...)
  PRUint32 dotsInFirstHost = firstHost.CountChar('.');
  if (dotsInFirstHost == currentHost.CountChar('.') &&
      dotsInFirstHost >= 2) {
    // we have enough dots - check IsInDomain(choppedFirstHost, currentHost)
    PRInt32 dot1 = firstHost.FindChar('.');
    return !IsInDomain(Substring(firstHost, dot1, firstHost.Length() - dot1), currentHost);
  }

  // don't have enough dots to chop firstHost, or the subdomain levels differ;
  // so we just do the plain old check, IsInDomain(firstHost, currentHost).
  return !IsInDomain(NS_LITERAL_CSTRING(".") + firstHost, currentHost);
}

nsCookieStatus
nsCookieService::CheckPrefs(nsIURI         *aHostURI,
                            nsIURI         *aFirstURI,
                            nsIChannel     *aChannel,
                            const char     *aCookieHeader,
                            nsCookiePolicy &aPolicy)
{
  // pref tree:
  // 0) get the scheme strings from the two URI's
  // 1) disallow ftp
  // 2) disallow mailnews, if pref set
  // 3) perform a permissionlist lookup to see if an entry exists for this host
  //    (a match here will override defaults in 4)
  // 4) go through enumerated permissions to see which one we have:
  // -> cookies disabled: return
  // -> dontacceptforeign: check if cookie is foreign
  // -> p3p: check p3p cookie data

  // we've extended the "nsCookieStatus" type to be used for all cases now
  // (used to be only for p3p), so beware that its interpretation is not p3p-
  // specific anymore.

  // first, get the URI scheme for further use
  // if GetScheme fails on aHostURI, reject; aFirstURI is optional, so failing is ok
  nsCAutoString currentURIScheme, firstURIScheme;
  nsresult rv, rv2 = NS_OK;
  rv = aHostURI->GetScheme(currentURIScheme);
  if (aFirstURI) {
    rv2 = aFirstURI->GetScheme(firstURIScheme);
  }
  if (NS_FAILED(rv) || NS_FAILED(rv2)) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "couldn't get scheme of host URI");
    return STATUS_REJECTED_WITH_ERROR;
  }

  // don't let ftp sites get/set cookies (could be a security issue)
  if (currentURIScheme.EqualsLiteral("ftp")) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "ftp sites cannot read cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  // check the permission list first; if we find an entry, it overrides
  // default prefs. see bug 184059.
  if (mPermissionService) {
    nsCookieAccess access;
    rv = mPermissionService->CanAccess(aHostURI, aFirstURI, aChannel, &access);

    // if we found an entry, use it
    if (NS_SUCCEEDED(rv)) {
      switch (access) {
      case nsICookiePermission::ACCESS_DENY:
        COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are blocked for this site");
        return nsICookie::STATUS_REJECTED;

      case nsICookiePermission::ACCESS_ALLOW:
        return nsICookie::STATUS_ACCEPTED;
      }
    }
  }

  // check default prefs - go thru enumerated permissions
  if (mCookiesPermissions == BEHAVIOR_REJECT) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are disabled");
    return nsICookie::STATUS_REJECTED;

  } else if (mCookiesPermissions == BEHAVIOR_REJECTFOREIGN) {
    // check if cookie is foreign.
    // if aFirstURI is null, allow by default

    // note: this can be circumvented if we have http redirects within html,
    // since the documentURI attribute isn't always correctly
    // passed to the redirected channels. (or isn't correctly set in the first place)
    if (IsForeign(aHostURI, aFirstURI)) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "originating server test failed");
      return nsICookie::STATUS_REJECTED;
    }

  } else if (mCookiesPermissions == BEHAVIOR_P3P) {
    // check to see if P3P conditions are satisfied. see nsICookie.idl for
    // P3P-related constants.

    nsCookieStatus p3pStatus = nsICookie::STATUS_UNKNOWN;

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);

    // lazily init the P3P service
    if (!mP3PService)
      mP3PService = do_GetService(NS_COOKIECONSENT_CONTRACTID);

    if (mP3PService) {
      // get the site policy and a status decision for the cookie
      PRBool isForeign = IsForeign(aHostURI, aFirstURI);
      mP3PService->GetConsent(aHostURI, httpChannel, isForeign, &aPolicy, &p3pStatus);
    }

    if (p3pStatus == nsICookie::STATUS_REJECTED) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "P3P test failed");
    }
    return p3pStatus;
  }

  // if nothing has complained, accept cookie
  return nsICookie::STATUS_ACCEPTED;
}

// processes domain attribute, and returns PR_TRUE if host has permission to set for this domain.
PRBool
nsCookieService::CheckDomain(nsCookieAttributes &aCookieAttributes,
                             nsIURI             *aHostURI)
{
  // get host from aHostURI
  nsCAutoString hostFromURI;
  if (NS_FAILED(aHostURI->GetAsciiHost(hostFromURI))) {
    return PR_FALSE;
  }
  // trim trailing dots
  hostFromURI.Trim(".");
  ToLowerCase(hostFromURI);

  // if a domain is given, check the host has permission
  if (!aCookieAttributes.host.IsEmpty()) {
    aCookieAttributes.host.Trim(".");
    // switch to lowercase now, to avoid case-insensitive compares everywhere
    ToLowerCase(aCookieAttributes.host);

    // check whether the host is an IP address, and override isDomain to
    // make the cookie a non-domain one. this will require an exact host
    // match for the cookie, so we eliminate any chance of IP address
    // funkiness (e.g. the alias 127.1 domain-matching 99.54.127.1).
    // bug 105917 originally noted the requirement to deal with IP addresses.
    if (IsIPAddress(aCookieAttributes.host)) {
      return IsInDomain(aCookieAttributes.host, hostFromURI, PR_FALSE);
    }

    /*
     * verify that this host has the authority to set for this domain.   We do
     * this by making sure that the host is in the domain.  We also require
     * that a domain have at least one embedded period to prevent domains of the form
     * ".com" and ".edu"
     */
    PRInt32 dot = aCookieAttributes.host.FindChar('.');
    if (dot == kNotFound) {
      // fail dot test
      return PR_FALSE;
    }

    // prepend a dot, and check if the host is in the domain
    aCookieAttributes.host.Insert(NS_LITERAL_CSTRING("."), 0);
    if (!IsInDomain(aCookieAttributes.host, hostFromURI)) {
      return PR_FALSE;
    }

    /*
     * note: RFC2109 section 4.3.2 requires that we check the following:
     * that the portion of host not in domain does not contain a dot.
     * this prevents hosts of the form x.y.co.nz from setting cookies in the
     * entire .co.nz domain. however, it's only a only a partial solution and
     * it breaks sites (IE doesn't enforce it), so we don't perform this check.
     */

  // no domain specified, use hostFromURI
  } else {
    aCookieAttributes.host = hostFromURI;
  }

  return PR_TRUE;
}

PRBool
nsCookieService::CheckPath(nsCookieAttributes &aCookieAttributes,
                           nsIURI             *aHostURI)
{
  // if a path is given, check the host has permission
  if (aCookieAttributes.path.IsEmpty()) {
    // strip down everything after the last slash to get the path,
    // ignoring slashes in the query string part.
    // if we can QI to nsIURL, that'll take care of the query string portion.
    // otherwise, it's not an nsIURL and can't have a query string, so just find the last slash.
    nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
    if (hostURL) {
      hostURL->GetDirectory(aCookieAttributes.path);
    } else {
      aHostURI->GetPath(aCookieAttributes.path);
      PRInt32 slash = aCookieAttributes.path.RFindChar('/');
      if (slash != kNotFound) {
        aCookieAttributes.path.Truncate(slash + 1);
      }
    }

#if 0
  } else {
    /**
     * The following test is part of the RFC2109 spec.  Loosely speaking, it says that a site
     * cannot set a cookie for a path that it is not on.  See bug 155083.  However this patch
     * broke several sites -- nordea (bug 155768) and citibank (bug 156725).  So this test has
     * been disabled, unless we can evangelize these sites.
     */
    // get path from aHostURI
    nsCAutoString pathFromURI;
    if (NS_FAILED(aHostURI->GetPath(pathFromURI)) ||
        !StringBeginsWith(pathFromURI, aCookieAttributes.path)) {
      return PR_FALSE;
    }
#endif
  }

  return PR_TRUE;
}

PRBool
nsCookieService::GetExpiry(nsCookieAttributes &aCookieAttributes,
                           nsInt64            aServerTime,
                           nsInt64            aCurrentTime,
                           nsCookieStatus     aStatus)
{
  /* Determine when the cookie should expire. This is done by taking the difference between 
   * the server time and the time the server wants the cookie to expire, and adding that 
   * difference to the client time. This localizes the client time regardless of whether or
   * not the TZ environment variable was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  nsInt64 delta;

  // check for max-age attribute first; this overrides expires attribute
  if (!aCookieAttributes.maxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    PRInt64 maxage;
    PRInt32 numInts = PR_sscanf(aCookieAttributes.maxage.get(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return PR_TRUE;
    }

    delta = nsInt64(maxage);

  // check for expires attribute
  } else if (!aCookieAttributes.expires.IsEmpty()) {
    nsInt64 expires;
    PRTime tempExpires;

    // parse expiry time
    if (PR_ParseTimeString(aCookieAttributes.expires.get(), PR_TRUE, &tempExpires) == PR_SUCCESS) {
      expires = nsInt64(tempExpires) / USEC_PER_SEC;
    } else {
      return PR_TRUE;
    }

    delta = expires - aServerTime;

  // default to session cookie if no attributes found
  } else {
    return PR_TRUE;
  }

  // if this addition overflows, expiryTime will be less than currentTime
  // and the cookie will be expired - that's okay.
  aCookieAttributes.expiryTime = aCurrentTime + delta;

  // we need to return whether the cookie is a session cookie or not:
  // the cookie may have been previously downgraded by p3p prefs,
  // so we take that into account here. only applies to non-expired cookies.
  return aStatus == nsICookie::STATUS_DOWNGRADED &&
         aCookieAttributes.expiryTime > aCurrentTime;
}

/******************************************************************************
 * nsCookieService impl:
 * private cookielist management functions
 ******************************************************************************/

void
nsCookieService::RemoveAllFromMemory()
{
  // clearing the hashtable will call each nsCookieEntry's dtor,
  // which releases all their respective children.
  mHostTable.Clear();
  mCookieCount = 0;
  mCookieChanged = PR_TRUE;
}

PLDHashOperator PR_CALLBACK
removeExpiredCallback(nsCookieEntry *aEntry,
                      void          *aArg)
{
  const nsInt64 &currentTime = *NS_STATIC_CAST(nsInt64*, aArg);
  for (nsListIter iter(aEntry, nsnull, aEntry->Head()); iter.current; ) {
    if (iter.current->Expiry() <= currentTime)
      // remove from list. this takes care of updating the iterator for us
      nsCookieService::gCookieService->RemoveCookieFromList(iter);
    else
      ++iter;
  }
  return PL_DHASH_NEXT;
}

// removes any expired cookies from memory
void
nsCookieService::RemoveExpiredCookies(nsInt64 aCurrentTime)
{
  mHostTable.EnumerateEntries(removeExpiredCallback, &aCurrentTime);
}

// find whether a previous cookie has been set, and count the number of cookies from
// this host, for prompting purposes. this is provided by the nsICookieManager2
// interface.
NS_IMETHODIMP
nsCookieService::FindMatchingCookie(nsICookie2 *aCookie,
                                    PRUint32   *aCountFromHost,
                                    PRBool     *aFoundCookie)
{
  NS_ENSURE_ARG_POINTER(aCookie);

  // we don't care about finding the oldest cookie here, so disable the search
  nsEnumerationData data(NOW_IN_SECONDS, LL_MININT);
  nsCookie *cookie = NS_STATIC_CAST(nsCookie*, aCookie);

  *aCountFromHost = CountCookiesFromHost(cookie, data);
  *aFoundCookie = FindCookie(cookie->Host(), cookie->Name(), cookie->Path(), data.iter);
  return NS_OK;
}

// count the number of cookies from this host, and find the oldest cookie
// from this host.
PRUint32
nsCookieService::CountCookiesFromHost(nsCookie          *aCookie,
                                      nsEnumerationData &aData)
{
  PRUint32 countFromHost = 0;

  nsCAutoString hostWithDot(NS_LITERAL_CSTRING(".") + aCookie->RawHost());

  const char *currentDot = hostWithDot.get();
  const char *nextDot = currentDot + 1;
  do {
    nsCookieEntry *entry = mHostTable.GetEntry(currentDot);
    for (nsListIter iter(entry); iter.current; ++iter) {
      // only count non-expired cookies
      if (iter.current->Expiry() > aData.currentTime) {
        ++countFromHost;

        // check if we've found the oldest cookie so far
        if (aData.oldestTime > iter.current->LastAccessed()) {
          aData.oldestTime = iter.current->LastAccessed();
          aData.iter = iter;
        }
      }
    }

    currentDot = nextDot;
    if (currentDot)
      nextDot = strchr(currentDot + 1, '.');

  } while (currentDot);

  return countFromHost;
}

// find an exact previous match.
PRBool
nsCookieService::FindCookie(const nsAFlatCString &aHost,
                            const nsAFlatCString &aName,
                            const nsAFlatCString &aPath,
                            nsListIter           &aIter)
{
  nsCookieEntry *entry = mHostTable.GetEntry(aHost.get());
  for (aIter = nsListIter(entry); aIter.current; ++aIter) {
    if (aPath.Equals(aIter.current->Path()) &&
        aName.Equals(aIter.current->Name())) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

// removes a cookie from the hashtable, and update the iterator state.
void
nsCookieService::RemoveCookieFromList(nsListIter &aIter)
{
  if (!aIter.prev && !aIter.current->Next()) {
    // we're removing the last element in the list - so just remove the entry
    // from the hash. note that the entryclass' dtor will take care of
    // releasing this last element for us!
    mHostTable.RawRemoveEntry(aIter.entry);
    aIter.current = nsnull;

  } else {
    // just remove the element from the list, and increment the iterator
    nsCookie *next = aIter.current->Next();
    NS_RELEASE(aIter.current);
    if (aIter.prev) {
      // element to remove is not the head
      aIter.current = aIter.prev->Next() = next;
    } else {
      // element to remove is the head
      aIter.current = aIter.entry->Head() = next;
    }
  }

  --mCookieCount;
  mCookieChanged = PR_TRUE;
}

PRBool
nsCookieService::AddCookieToList(nsCookie *aCookie)
{
  nsCookieEntry *entry = mHostTable.PutEntry(aCookie->Host().get());

  if (!entry) {
    NS_ERROR("can't insert element into a null entry!");
    return PR_FALSE;
  }

  NS_ADDREF(aCookie);

  aCookie->Next() = entry->Head();
  entry->Head() = aCookie;
  ++mCookieCount;
  mCookieChanged = PR_TRUE;

  return PR_TRUE;
}

PR_STATIC_CALLBACK(PLDHashOperator)
findOldestCallback(nsCookieEntry *aEntry,
                   void          *aArg)
{
  nsEnumerationData *data = NS_STATIC_CAST(nsEnumerationData*, aArg);
  for (nsListIter iter(aEntry, nsnull, aEntry->Head()); iter.current; ++iter) {
    // check if we've found the oldest cookie so far
    if (data->oldestTime > iter.current->LastAccessed()) {
      data->oldestTime = iter.current->LastAccessed();
      data->iter = iter;
    }
  }
  return PL_DHASH_NEXT;
}

void
nsCookieService::FindOldestCookie(nsEnumerationData &aData)
{
  mHostTable.EnumerateEntries(findOldestCallback, &aData);
}
