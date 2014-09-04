/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSiteSecurityService.h"

#include "mozilla/LinkedList.h"
#include "mozilla/Preferences.h"
#include "nsCRTGlue.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsISocketProvider.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsSecurityHeaderParser.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "plstr.h"
#include "prlog.h"
#include "prnetdb.h"
#include "prprf.h"
#include "nsXULAppAPI.h"

// A note about the preload list:
// When a site specifically disables HSTS by sending a header with
// 'max-age: 0', we keep a "knockout" value that means "we have no information
// regarding the HSTS state of this host" (any ancestor of "this host" can still
// influence its HSTS status via include subdomains, however).
// This prevents the preload list from overriding the site's current
// desired HSTS status.
#include "nsSTSPreloadList.inc"

#if defined(PR_LOGGING)
static PRLogModuleInfo *
GetSSSLog()
{
  static PRLogModuleInfo *gSSSLog;
  if (!gSSSLog)
    gSSSLog = PR_NewLogModule("nsSSService");
  return gSSSLog;
}
#endif

#define SSSLOG(args) PR_LOG(GetSSSLog(), 4, args)

////////////////////////////////////////////////////////////////////////////////

SiteSecurityState::SiteSecurityState(nsCString& aStateString)
  : mHSTSExpireTime(0)
  , mHSTSState(SecurityPropertyUnset)
  , mHSTSIncludeSubdomains(false)
{
  uint32_t hstsState = 0;
  uint32_t hstsIncludeSubdomains = 0; // PR_sscanf doesn't handle bools.
  int32_t matches = PR_sscanf(aStateString.get(), "%lld,%lu,%lu",
                              &mHSTSExpireTime, &hstsState,
                              &hstsIncludeSubdomains);
  bool valid = (matches == 3 &&
                (hstsIncludeSubdomains == 0 || hstsIncludeSubdomains == 1) &&
                ((SecurityPropertyState)hstsState == SecurityPropertyUnset ||
                 (SecurityPropertyState)hstsState == SecurityPropertySet ||
                 (SecurityPropertyState)hstsState == SecurityPropertyKnockout));
  if (valid) {
    mHSTSState = (SecurityPropertyState)hstsState;
    mHSTSIncludeSubdomains = (hstsIncludeSubdomains == 1);
  } else {
    SSSLOG(("%s is not a valid SiteSecurityState", aStateString.get()));
    mHSTSExpireTime = 0;
    mHSTSState = SecurityPropertyUnset;
    mHSTSIncludeSubdomains = false;
  }
}

SiteSecurityState::SiteSecurityState(PRTime aHSTSExpireTime,
                                     SecurityPropertyState aHSTSState,
                                     bool aHSTSIncludeSubdomains)

  : mHSTSExpireTime(aHSTSExpireTime)
  , mHSTSState(aHSTSState)
  , mHSTSIncludeSubdomains(aHSTSIncludeSubdomains)
{
}

void
SiteSecurityState::ToString(nsCString& aString)
{
  aString.Truncate();
  aString.AppendInt(mHSTSExpireTime);
  aString.Append(',');
  aString.AppendInt(mHSTSState);
  aString.Append(',');
  aString.AppendInt(static_cast<uint32_t>(mHSTSIncludeSubdomains));
}

////////////////////////////////////////////////////////////////////////////////

nsSiteSecurityService::nsSiteSecurityService()
  : mUsePreloadList(true)
  , mPreloadListTimeOffset(0)
{
}

nsSiteSecurityService::~nsSiteSecurityService()
{
}

NS_IMPL_ISUPPORTS(nsSiteSecurityService,
                  nsIObserver,
                  nsISiteSecurityService)

nsresult
nsSiteSecurityService::Init()
{
   // Child processes are not allowed direct access to this.
   if (XRE_GetProcessType() != GeckoProcessType_Default) {
     MOZ_CRASH("Child process: no direct access to nsSiteSecurityService");
   }

  // Don't access Preferences off the main thread.
  if (!NS_IsMainThread()) {
    NS_NOTREACHED("nsSiteSecurityService initialized off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  mUsePreloadList = mozilla::Preferences::GetBool(
    "network.stricttransportsecurity.preloadlist", true);
  mozilla::Preferences::AddStrongObserver(this,
    "network.stricttransportsecurity.preloadlist");
  mPreloadListTimeOffset = mozilla::Preferences::GetInt(
    "test.currentTimeOffsetSeconds", 0);
  mozilla::Preferences::AddStrongObserver(this,
    "test.currentTimeOffsetSeconds");
  mSiteStateStorage =
    new mozilla::DataStorage(NS_LITERAL_STRING("SiteSecurityServiceState.txt"));
  bool storageWillPersist = false;
  nsresult rv = mSiteStateStorage->Init(storageWillPersist);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // This is not fatal. There are some cases where there won't be a
  // profile directory (e.g. running xpcshell). There isn't the
  // expectation that site information will be presisted in those cases.
  if (!storageWillPersist) {
    NS_WARNING("site security information will not be persisted");
  }

  return NS_OK;
}

nsresult
nsSiteSecurityService::GetHost(nsIURI *aURI, nsACString &aResult)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  if (!innerURI) return NS_ERROR_FAILURE;

  nsresult rv = innerURI->GetAsciiHost(aResult);

  if (NS_FAILED(rv) || aResult.IsEmpty())
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

nsresult
nsSiteSecurityService::SetState(uint32_t aType,
                                nsIURI* aSourceURI,
                                int64_t maxage,
                                bool includeSubdomains,
                                uint32_t flags)
{
  // If max-age is zero, that's an indication to immediately remove the
  // security state, so here's a shortcut.
  if (!maxage) {
    return RemoveState(aType, aSourceURI, flags);
  }

  // Expire time is millis from now.  Since STS max-age is in seconds, and
  // PR_Now() is in micros, must equalize the units at milliseconds.
  int64_t expiretime = (PR_Now() / PR_USEC_PER_MSEC) +
                       (maxage * PR_MSEC_PER_SEC);

  SiteSecurityState siteState(expiretime, SecurityPropertySet,
                              includeSubdomains);
  nsAutoCString stateString;
  siteState.ToString(stateString);
  nsAutoCString hostname;
  nsresult rv = GetHost(aSourceURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);
  SSSLOG(("SSS: setting state for %s", hostname.get()));
  bool isPrivate = flags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                         ? mozilla::DataStorage_Private
                                         : mozilla::DataStorage_Persistent;
  rv = mSiteStateStorage->Put(hostname, stateString, storageType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::RemoveState(uint32_t aType, nsIURI* aURI,
                                   uint32_t aFlags)
{
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  nsAutoCString hostname;
  nsresult rv = GetHost(aURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                         ? mozilla::DataStorage_Private
                                         : mozilla::DataStorage_Persistent;
  // If this host is in the preload list, we have to store a knockout entry.
  if (GetPreloadListEntry(hostname.get())) {
    SSSLOG(("SSS: storing knockout entry for %s", hostname.get()));
    SiteSecurityState siteState(0, SecurityPropertyKnockout, false);
    nsAutoCString stateString;
    siteState.ToString(stateString);
    rv = mSiteStateStorage->Put(hostname, stateString, storageType);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    SSSLOG(("SSS: removing entry for %s", hostname.get()));
    mSiteStateStorage->Remove(hostname, storageType);
  }

  return NS_OK;
}

static bool
HostIsIPAddress(const char *hostname)
{
  PRNetAddr hostAddr;
  return (PR_StringToNetAddr(hostname, &hostAddr) == PR_SUCCESS);
}

NS_IMETHODIMP
nsSiteSecurityService::ProcessHeader(uint32_t aType,
                                     nsIURI* aSourceURI,
                                     const char* aHeader,
                                     uint32_t aFlags,
                                     uint64_t *aMaxAge,
                                     bool *aIncludeSubdomains)
{
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  if (aMaxAge != nullptr) {
    *aMaxAge = 0;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = false;
  }

  nsAutoCString host;
  nsresult rv = GetHost(aSourceURI, host);
  NS_ENSURE_SUCCESS(rv, rv);
  if (HostIsIPAddress(host.get())) {
    /* Don't process headers if a site is accessed by IP address. */
    return NS_OK;
  }

  char * header = NS_strdup(aHeader);
  if (!header) return NS_ERROR_OUT_OF_MEMORY;
  rv = ProcessHeaderMutating(aType, aSourceURI, header, aFlags,
                             aMaxAge, aIncludeSubdomains);
  NS_Free(header);
  return rv;
}

nsresult
nsSiteSecurityService::ProcessHeaderMutating(uint32_t aType,
                                             nsIURI* aSourceURI,
                                             char* aHeader,
                                             uint32_t aFlags,
                                             uint64_t *aMaxAge,
                                             bool *aIncludeSubdomains)
{
  SSSLOG(("SSS: processing header '%s'", aHeader));

  // "Strict-Transport-Security" ":" OWS
  //      STS-d  *( OWS ";" OWS STS-d  OWS)
  //
  //  ; STS directive
  //  STS-d      = maxAge / includeSubDomains
  //
  //  maxAge     = "max-age" "=" delta-seconds v-ext
  //
  //  includeSubDomains = [ "includeSubDomains" ]
  //
  //  The order of the directives is not significant.
  //  All directives must appear only once.
  //  Directive names are case-insensitive.
  //  The entire header is invalid if a directive not conforming to the
  //  syntax is encountered.
  //  Unrecognized directives (that are otherwise syntactically valid) are
  //  ignored, and the rest of the header is parsed as normal.

  bool foundMaxAge = false;
  bool foundIncludeSubdomains = false;
  bool foundUnrecognizedDirective = false;
  int64_t maxAge = 0;

  NS_NAMED_LITERAL_CSTRING(max_age_var, "max-age");
  NS_NAMED_LITERAL_CSTRING(include_subd_var, "includesubdomains");


  nsSecurityHeaderParser parser(aHeader);
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: could not parse header"));
    return rv;
  }
  mozilla::LinkedList<nsSecurityHeaderDirective> *directives = parser.GetDirectives();

  for (nsSecurityHeaderDirective *directive = directives->getFirst();
       directive != nullptr; directive = directive->getNext()) {
    if (directive->mName.Length() == max_age_var.Length() &&
        directive->mName.EqualsIgnoreCase(max_age_var.get(),
                                          max_age_var.Length())) {
      if (foundMaxAge) {
        SSSLOG(("SSS: found two max-age directives"));
        return NS_ERROR_FAILURE;
      }

      SSSLOG(("SSS: found max-age directive"));
      foundMaxAge = true;

      size_t len = directive->mValue.Length();
      for (size_t i = 0; i < len; i++) {
        char chr = directive->mValue.CharAt(i);
        if (chr < '0' || chr > '9') {
          SSSLOG(("SSS: invalid value for max-age directive"));
          return NS_ERROR_FAILURE;
        }
      }

      if (PR_sscanf(directive->mValue.get(), "%lld", &maxAge) != 1) {
        SSSLOG(("SSS: could not parse delta-seconds"));
        return NS_ERROR_FAILURE;
      }

      SSSLOG(("SSS: parsed delta-seconds: %lld", maxAge));
    } else if (directive->mName.Length() == include_subd_var.Length() &&
               directive->mName.EqualsIgnoreCase(include_subd_var.get(),
                                                 include_subd_var.Length())) {
      if (foundIncludeSubdomains) {
        SSSLOG(("SSS: found two includeSubdomains directives"));
        return NS_ERROR_FAILURE;
      }

      SSSLOG(("SSS: found includeSubdomains directive"));
      foundIncludeSubdomains = true;

      if (directive->mValue.Length() != 0) {
        SSSLOG(("SSS: includeSubdomains directive unexpectedly had value '%s'",
                directive->mValue.get()));
        return NS_ERROR_FAILURE;
      }
    } else {
      SSSLOG(("SSS: ignoring unrecognized directive '%s'",
              directive->mName.get()));
      foundUnrecognizedDirective = true;
    }
  }

  // after processing all the directives, make sure we came across max-age
  // somewhere.
  if (!foundMaxAge) {
    SSSLOG(("SSS: did not encounter required max-age directive"));
    return NS_ERROR_FAILURE;
  }

  // record the successfully parsed header data.
  SetState(aType, aSourceURI, maxAge, foundIncludeSubdomains, aFlags);

  if (aMaxAge != nullptr) {
    *aMaxAge = (uint64_t)maxAge;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = foundIncludeSubdomains;
  }

  return foundUnrecognizedDirective ? NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
                                    : NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureURI(uint32_t aType, nsIURI* aURI,
                                   uint32_t aFlags, bool* aResult)
{
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  nsAutoCString hostname;
  nsresult rv = GetHost(aURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);
  /* An IP address never qualifies as a secure URI. */
  if (HostIsIPAddress(hostname.get())) {
    *aResult = false;
    return NS_OK;
  }

  return IsSecureHost(aType, hostname.get(), aFlags, aResult);
}

int STSPreloadCompare(const void *key, const void *entry)
{
  const char *keyStr = (const char *)key;
  const nsSTSPreload *preloadEntry = (const nsSTSPreload *)entry;
  return strcmp(keyStr, preloadEntry->mHost);
}

// Returns the preload list entry for the given host, if it exists.
// Only does exact host matching - the user must decide how to use the returned
// data. May return null.
const nsSTSPreload *
nsSiteSecurityService::GetPreloadListEntry(const char *aHost)
{
  PRTime currentTime = PR_Now() + (mPreloadListTimeOffset * PR_USEC_PER_SEC);
  if (mUsePreloadList && currentTime < gPreloadListExpirationTime) {
    return (const nsSTSPreload *) bsearch(aHost,
                                          kSTSPreloadList,
                                          mozilla::ArrayLength(kSTSPreloadList),
                                          sizeof(nsSTSPreload),
                                          STSPreloadCompare);
  }

  return nullptr;
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureHost(uint32_t aType, const char* aHost,
                                    uint32_t aFlags, bool* aResult)
{
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  // set default in case if we can't find any STS information
  *aResult = false;

  /* An IP address never qualifies as a secure URI. */
  if (HostIsIPAddress(aHost)) {
    return NS_OK;
  }

  // Holepunch chart.apis.google.com and subdomains.
  nsAutoCString host(aHost);
  ToLowerCase(host);
  if (host.EqualsLiteral("chart.apis.google.com") ||
      StringEndsWith(host, NS_LITERAL_CSTRING(".chart.apis.google.com"))) {
    return NS_OK;
  }

  const nsSTSPreload *preload = nullptr;

  // First check the exact host. This involves first checking for an entry in
  // site security storage. If that entry exists, we don't want to check
  // in the preload list. We only want to use the stored value if it is not a
  // knockout entry, however.
  // Additionally, if it is a knockout entry, we want to stop looking for data
  // on the host, because the knockout entry indicates "we have no information
  // regarding the security status of this host".
  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                         ? mozilla::DataStorage_Private
                                         : mozilla::DataStorage_Persistent;
  nsCString value = mSiteStateStorage->Get(host, storageType);
  SiteSecurityState siteState(value);
  if (siteState.mHSTSState != SecurityPropertyUnset) {
    SSSLOG(("Found entry for %s", host.get()));
    bool expired = siteState.IsExpired(aType);
    if (!expired && siteState.mHSTSState == SecurityPropertySet) {
      *aResult = true;
      return NS_OK;
    }

    // If the entry is expired and not in the preload list, we can remove it.
    if (expired && !GetPreloadListEntry(host.get())) {
      mSiteStateStorage->Remove(host, storageType);
    }
  }
  // Finally look in the preloaded list. This is the exact host,
  // so if an entry exists at all, this host is HSTS.
  else if (GetPreloadListEntry(host.get())) {
    SSSLOG(("%s is a preloaded STS host", host.get()));
    *aResult = true;
    return NS_OK;
  }

  SSSLOG(("no HSTS data for %s found, walking up domain", host.get()));
  const char *subdomain;

  uint32_t offset = 0;
  for (offset = host.FindChar('.', offset) + 1;
       offset > 0;
       offset = host.FindChar('.', offset) + 1) {

    subdomain = host.get() + offset;

    // If we get an empty string, don't continue.
    if (strlen(subdomain) < 1) {
      break;
    }

    // Do the same thing as with the exact host, except now we're looking at
    // ancestor domains of the original host. So, we have to look at the
    // include subdomains flag (although we still have to check for a
    // SecurityPropertySet flag first to check that this is a secure host and
    // not a knockout entry - and again, if it is a knockout entry, we stop
    // looking for data on it and skip to the next higher up ancestor domain).
    nsCString subdomainString(subdomain);
    value = mSiteStateStorage->Get(subdomainString, storageType);
    SiteSecurityState siteState(value);
    if (siteState.mHSTSState != SecurityPropertyUnset) {
      SSSLOG(("Found entry for %s", subdomain));
      bool expired = siteState.IsExpired(aType);
      if (!expired && siteState.mHSTSState == SecurityPropertySet) {
        *aResult = siteState.mHSTSIncludeSubdomains;
        break;
      }

      // If the entry is expired and not in the preload list, we can remove it.
      if (expired && !GetPreloadListEntry(subdomain)) {
        mSiteStateStorage->Remove(subdomainString, storageType);
      }
    }
    // This is an ancestor, so if we get a match, we have to check if the
    // preloaded entry includes subdomains.
    else if ((preload = GetPreloadListEntry(subdomain)) != nullptr) {
      if (preload->mIncludeSubdomains) {
        SSSLOG(("%s is a preloaded STS host", subdomain));
        *aResult = true;
        break;
      }
    }

    SSSLOG(("no HSTS data for %s found, walking up domain", subdomain));
  }

  // Use whatever we ended up with, which defaults to false.
  return NS_OK;
}


// Verify the trustworthiness of the security info (are there any cert errors?)
NS_IMETHODIMP
nsSiteSecurityService::ShouldIgnoreHeaders(nsISupports* aSecurityInfo,
                                           bool* aResult)
{
  nsresult rv;
  bool tlsIsBroken = false;
  nsCOMPtr<nsISSLStatusProvider> sslprov = do_QueryInterface(aSecurityInfo);
  NS_ENSURE_TRUE(sslprov, NS_ERROR_FAILURE);

  nsCOMPtr<nsISSLStatus> sslstat;
  rv = sslprov->GetSSLStatus(getter_AddRefs(sslstat));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(sslstat, NS_ERROR_FAILURE);

  bool trustcheck;
  rv = sslstat->GetIsDomainMismatch(&trustcheck);
  NS_ENSURE_SUCCESS(rv, rv);
  tlsIsBroken = tlsIsBroken || trustcheck;

  rv = sslstat->GetIsNotValidAtThisTime(&trustcheck);
  NS_ENSURE_SUCCESS(rv, rv);
  tlsIsBroken = tlsIsBroken || trustcheck;

  rv = sslstat->GetIsUntrusted(&trustcheck);
  NS_ENSURE_SUCCESS(rv, rv);
  tlsIsBroken = tlsIsBroken || trustcheck;

  *aResult = tlsIsBroken;
  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::ClearAll()
{
  return mSiteStateStorage->Clear();
}

//------------------------------------------------------------
// nsSiteSecurityService::nsIObserver
//------------------------------------------------------------

NS_IMETHODIMP
nsSiteSecurityService::Observe(nsISupports *subject,
                               const char *topic,
                               const char16_t *data)
{
  // Don't access Preferences off the main thread.
  if (!NS_IsMainThread()) {
    NS_NOTREACHED("Preferences accessed off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    mUsePreloadList = mozilla::Preferences::GetBool(
      "network.stricttransportsecurity.preloadlist", true);
    mPreloadListTimeOffset =
      mozilla::Preferences::GetInt("test.currentTimeOffsetSeconds", 0);
  }

  return NS_OK;
}
