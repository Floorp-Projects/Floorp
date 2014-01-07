/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include "prlog.h"
#include "prprf.h"
#include "prnetdb.h"
#include "nsCRTGlue.h"
#include "nsIPermissionManager.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsSiteSecurityService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsIScriptSecurityManager.h"
#include "nsISocketProvider.h"
#include "mozilla/Preferences.h"
#include "mozilla/LinkedList.h"
#include "nsSecurityHeaderParser.h"
#include "nsXULAppAPI.h"

// A note about the preload list:
// When a site specifically disables sts by sending a header with
// 'max-age: 0', we keep a "knockout" value that means "we have no information
// regarding the sts state of this host" (any ancestor of "this host" can still
// influence its sts status via include subdomains, however).
// This prevents the preload list from overriding the site's current
// desired sts status. Knockout values are indicated by permission values of
// STS_KNOCKOUT.
#include "nsSTSPreloadList.inc"

#define STS_SET (nsIPermissionManager::ALLOW_ACTION)
#define STS_UNSET (nsIPermissionManager::UNKNOWN_ACTION)
#define STS_KNOCKOUT (nsIPermissionManager::DENY_ACTION)

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

nsSSSHostEntry::nsSSSHostEntry(const char* aHost)
  : mHost(aHost)
  , mExpireTime(0)
  , mStsPermission(STS_UNSET)
  , mExpired(false)
  , mIncludeSubdomains(false)
{
}

nsSSSHostEntry::nsSSSHostEntry(const nsSSSHostEntry& toCopy)
  : mHost(toCopy.mHost)
  , mExpireTime(toCopy.mExpireTime)
  , mStsPermission(toCopy.mStsPermission)
  , mExpired(toCopy.mExpired)
  , mIncludeSubdomains(toCopy.mIncludeSubdomains)
{
}

////////////////////////////////////////////////////////////////////////////////


nsSiteSecurityService::nsSiteSecurityService()
  : mUsePreloadList(true)
{
}

nsSiteSecurityService::~nsSiteSecurityService()
{
}

NS_IMPL_ISUPPORTS2(nsSiteSecurityService,
                   nsIObserver,
                   nsISiteSecurityService)

nsresult
nsSiteSecurityService::Init()
{
   // Child processes are not allowed direct access to this.
   if (XRE_GetProcessType() != GeckoProcessType_Default) {
     MOZ_CRASH("Child process: no direct access to nsSiteSecurityService");
   }

   nsresult rv;

   mPermMgr = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
   NS_ENSURE_SUCCESS(rv, rv);

   mUsePreloadList = mozilla::Preferences::GetBool("network.stricttransportsecurity.preloadlist", true);
   mozilla::Preferences::AddStrongObserver(this, "network.stricttransportsecurity.preloadlist");
   mObserverService = mozilla::services::GetObserverService();
   if (mObserverService)
     mObserverService->AddObserver(this, "last-pb-context-exited", false);

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
nsSiteSecurityService::GetPrincipalForURI(nsIURI* aURI,
                                          nsIPrincipal** aPrincipal)
{
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> securityManager =
     do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // We have to normalize the scheme of the URIs we're using, so just use https.
  // HSTS information is shared across all ports for a given host.
  nsAutoCString host;
  rv = GetHost(aURI, host);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("https://") + host);
  NS_ENSURE_SUCCESS(rv, rv);

  // We want all apps to share HSTS state, so this is one of the few places
  // where we do not silo persistent state by extended origin.
  return securityManager->GetNoAppCodebasePrincipal(uri, aPrincipal);
}

nsresult
nsSiteSecurityService::SetState(uint32_t aType,
                                nsIURI* aSourceURI,
                                int64_t maxage,
                                bool includeSubdomains,
                                uint32_t flags)
{
  // If max-age is zero, that's an indication to immediately remove the
  // permissions, so here's a shortcut.
  if (!maxage) {
    return RemoveState(aType, aSourceURI, flags);
  }

  // Expire time is millis from now.  Since STS max-age is in seconds, and
  // PR_Now() is in micros, must equalize the units at milliseconds.
  int64_t expiretime = (PR_Now() / PR_USEC_PER_MSEC) +
                       (maxage * PR_MSEC_PER_SEC);

  bool isPrivate = flags & nsISocketProvider::NO_PERMANENT_STORAGE;

  // record entry for this host with max-age in the permissions manager
  SSSLOG(("SSS: maxage permission SET, adding permission\n"));
  nsresult rv = AddPermission(aSourceURI,
                              STS_PERMISSION,
                              (uint32_t) STS_SET,
                              (uint32_t) nsIPermissionManager::EXPIRE_TIME,
                              expiretime,
                              isPrivate);
  NS_ENSURE_SUCCESS(rv, rv);

  if (includeSubdomains) {
    // record entry for this host with include subdomains in the permissions manager
    SSSLOG(("SSS: subdomains permission SET, adding permission\n"));
    rv = AddPermission(aSourceURI,
                       STS_SUBDOMAIN_PERMISSION,
                       (uint32_t) STS_SET,
                       (uint32_t) nsIPermissionManager::EXPIRE_TIME,
                       expiretime,
                       isPrivate);
    NS_ENSURE_SUCCESS(rv, rv);
  } else { // !includeSubdomains
    nsAutoCString hostname;
    rv = GetHost(aSourceURI, hostname);
    NS_ENSURE_SUCCESS(rv, rv);

    SSSLOG(("SSS: subdomains permission UNSET, removing any existing ones\n"));
    rv = RemovePermission(hostname, STS_SUBDOMAIN_PERMISSION, isPrivate);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::RemoveState(uint32_t aType, nsIURI* aURI, uint32_t aFlags)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  nsAutoCString hostname;
  nsresult rv = GetHost(aURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;

  rv = RemovePermission(hostname, STS_PERMISSION, isPrivate);
  NS_ENSURE_SUCCESS(rv, rv);
  SSSLOG(("SSS: deleted maxage permission\n"));

  rv = RemovePermission(hostname, STS_SUBDOMAIN_PERMISSION, isPrivate);
  NS_ENSURE_SUCCESS(rv, rv);
  SSSLOG(("SSS: deleted subdomains permission\n"));

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
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);
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
        SSSLOG(("SSS: includeSubdomains directive unexpectedly had value '%s'", directive->mValue.get()));
        return NS_ERROR_FAILURE;
      }
    } else {
      SSSLOG(("SSS: ignoring unrecognized directive '%s'", directive->mName.get()));
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

  return foundUnrecognizedDirective ?
         NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA :
         NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureHost(uint32_t aType, const char* aHost,
                                    uint32_t aFlags, bool* aResult)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  /* An IP address never qualifies as a secure URI. */
  if (HostIsIPAddress(aHost)) {
    *aResult = false;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsDependentCString hostString(aHost);
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                          NS_LITERAL_CSTRING("https://") + hostString);
  NS_ENSURE_SUCCESS(rv, rv);
  return IsSecureURI(aType, uri, aFlags, aResult);
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
  PRTime currentTime = PR_Now();
  int32_t timeOffset = 0;
  nsresult rv = mozilla::Preferences::GetInt("test.currentTimeOffsetSeconds",
                                             &timeOffset);
  if (NS_SUCCEEDED(rv)) {
    currentTime += (PRTime(timeOffset) * PR_USEC_PER_SEC);
  }

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
nsSiteSecurityService::IsSecureURI(uint32_t aType, nsIURI* aURI,
                                   uint32_t aFlags, bool* aResult)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  // set default in case if we can't find any STS information
  *aResult = false;

  nsAutoCString host;
  nsresult rv = GetHost(aURI, host);
  NS_ENSURE_SUCCESS(rv, rv);

  /* An IP address never qualifies as a secure URI. */
  if (HostIsIPAddress(host.BeginReading())) {
    return NS_OK;
  }

  const nsSTSPreload *preload = nullptr;
  nsSSSHostEntry *pbEntry = nullptr;

  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  if (isPrivate) {
    pbEntry = mPrivateModeHostTable.GetEntry(host.get());
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = GetPrincipalForURI(aURI, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t permMgrPermission;
  rv = mPermMgr->TestExactPermissionFromPrincipal(principal, STS_PERMISSION,
                                                  &permMgrPermission);
  NS_ENSURE_SUCCESS(rv, rv);

  // First check the exact host. This involves first checking for an entry in
  // the private browsing table. If that entry exists, we don't want to check
  // in either the permission manager or the preload list. We only want to use
  // the stored permission if it is not a knockout entry, however.
  // Additionally, if it is a knockout entry, we want to stop looking for data
  // on the host, because the knockout entry indicates "we have no information
  // regarding the sts status of this host".
  if (pbEntry && pbEntry->mStsPermission != STS_UNSET) {
    SSSLOG(("Found private browsing table entry for %s", host.get()));
    if (!pbEntry->IsExpired() && pbEntry->mStsPermission == STS_SET) {
      *aResult = true;
      return NS_OK;
    }
  }
  // Next we look in the permission manager. Same story here regarding
  // knockout entries.
  else if (permMgrPermission != STS_UNSET) {
    SSSLOG(("Found permission manager entry for %s", host.get()));
    if (permMgrPermission == STS_SET) {
      *aResult = true;
      return NS_OK;
    }
  }
  // Finally look in the preloaded list. This is the exact host,
  // so if an entry exists at all, this host is sts.
  else if (GetPreloadListEntry(host.get())) {
    SSSLOG(("%s is a preloaded STS host", host.get()));
    *aResult = true;
    return NS_OK;
  }

  // Used for testing permissions as we walk up the domain tree.
  nsCOMPtr<nsIURI> domainWalkURI;
  nsCOMPtr<nsIPrincipal> domainWalkPrincipal;
  const char *subdomain;

  SSSLOG(("no HSTS data for %s found, walking up domain", host.get()));
  uint32_t offset = 0;
  for (offset = host.FindChar('.', offset) + 1;
       offset > 0;
       offset = host.FindChar('.', offset) + 1) {

    subdomain = host.get() + offset;

    // If we get an empty string, don't continue.
    if (strlen(subdomain) < 1) {
      break;
    }

    if (isPrivate) {
      pbEntry = mPrivateModeHostTable.GetEntry(subdomain);
    }

    // normalize all URIs with https://
    rv = NS_NewURI(getter_AddRefs(domainWalkURI),
                   NS_LITERAL_CSTRING("https://") + Substring(host, offset));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetPrincipalForURI(domainWalkURI, getter_AddRefs(domainWalkPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mPermMgr->TestExactPermissionFromPrincipal(domainWalkPrincipal,
                                                    STS_PERMISSION,
                                                    &permMgrPermission);
    NS_ENSURE_SUCCESS(rv, rv);

    // Do the same thing as with the exact host, except now we're looking at
    // ancestor domains of the original host. So, we have to look at the
    // include subdomains permissions (although we still have to check for the
    // STS_PERMISSION first to check that this is an sts host and not a
    // knockout entry - and again, if it is a knockout entry, we stop looking
    // for data on it and skip to the next higher up ancestor domain).
    if (pbEntry && pbEntry->mStsPermission != STS_UNSET) {
      SSSLOG(("Found private browsing table entry for %s", subdomain));
      if (!pbEntry->IsExpired() && pbEntry->mStsPermission == STS_SET) {
        *aResult = pbEntry->mIncludeSubdomains;
        break;
      }
    }
    else if (permMgrPermission != STS_UNSET) {
      SSSLOG(("Found permission manager entry for %s", subdomain));
      if (permMgrPermission == STS_SET) {
        uint32_t subdomainPermission;
        rv = mPermMgr->TestExactPermissionFromPrincipal(domainWalkPrincipal,
                                                        STS_SUBDOMAIN_PERMISSION,
                                                        &subdomainPermission);
        NS_ENSURE_SUCCESS(rv, rv);
        *aResult = (subdomainPermission == STS_SET);
        break;
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

//------------------------------------------------------------
// nsSiteSecurityService::nsIObserver
//------------------------------------------------------------

NS_IMETHODIMP
nsSiteSecurityService::Observe(nsISupports *subject,
                               const char *topic,
                               const char16_t *data)
{
  if (strcmp(topic, "last-pb-context-exited") == 0) {
    mPrivateModeHostTable.Clear();
  }
  else if (strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    mUsePreloadList = mozilla::Preferences::GetBool("network.stricttransportsecurity.preloadlist", true);
  }

  return NS_OK;
}

//------------------------------------------------------------
// Functions to overlay the permission manager calls in case
// we're in private browsing mode.
//------------------------------------------------------------
nsresult
nsSiteSecurityService::AddPermission(nsIURI     *aURI,
                                     const char *aType,
                                     uint32_t   aPermission,
                                     uint32_t   aExpireType,
                                     int64_t    aExpireTime,
                                     bool       aIsPrivate)
{
    // Private mode doesn't address user-set (EXPIRE_NEVER) permissions: let
    // those be stored persistently.
    if (!aIsPrivate || aExpireType == nsIPermissionManager::EXPIRE_NEVER) {
      // Not in private mode, or manually-set permission
      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = GetPrincipalForURI(aURI, getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      return mPermMgr->AddFromPrincipal(principal, aType, aPermission,
                                        aExpireType, aExpireTime);
    }

    nsAutoCString host;
    nsresult rv = GetHost(aURI, host);
    NS_ENSURE_SUCCESS(rv, rv);
    SSSLOG(("AddPermission for entry for %s", host.get()));

    // Update in mPrivateModeHostTable only, so any changes will be rolled
    // back when exiting private mode.

    // Note: EXPIRE_NEVER permissions should trump anything that shows up in
    // the HTTP header, so if there's an EXPIRE_NEVER permission already
    // don't store anything new.
    // Currently there's no way to get the type of expiry out of the
    // permission manager, but that's okay since there's nothing that stores
    // EXPIRE_NEVER permissions.

    // PutEntry returns an existing entry if there already is one, or it
    // creates a new one if there isn't.
    nsSSSHostEntry* entry = mPrivateModeHostTable.PutEntry(host.get());
    if (!entry) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    SSSLOG(("Created private mode entry for %s", host.get()));

    // AddPermission() will be called twice if the STS header encountered has
    // includeSubdomains (first for the main permission and second for the
    // subdomains permission). If AddPermission() gets called a second time
    // with the STS_SUBDOMAIN_PERMISSION, we just have to flip that bit in
    // the nsSSSHostEntry.
    if (strcmp(aType, STS_SUBDOMAIN_PERMISSION) == 0) {
      entry->mIncludeSubdomains = true;
    }
    else if (strcmp(aType, STS_PERMISSION) == 0) {
      entry->mStsPermission = aPermission;
    }

    // Also refresh the expiration time.
    entry->SetExpireTime(aExpireTime);
    return NS_OK;
}

nsresult
nsSiteSecurityService::RemovePermission(const nsCString  &aHost,
                                        const char       *aType,
                                        bool aIsPrivate)
{
    // Build up a principal for use with the permission manager.
    // normalize all URIs with https://
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri),
                            NS_LITERAL_CSTRING("https://") + aHost);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal;
    rv = GetPrincipalForURI(uri, getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!aIsPrivate) {
      // Not in private mode: remove permissions persistently.
      // This means setting the permission to STS_KNOCKOUT in case
      // this host is on the preload list (so we can override it).
      return mPermMgr->AddFromPrincipal(principal, aType,
                                        STS_KNOCKOUT,
                                        nsIPermissionManager::EXPIRE_NEVER, 0);
    }

    // Make changes in mPrivateModeHostTable only, so any changes will be
    // rolled back when exiting private mode.
    nsSSSHostEntry* entry = mPrivateModeHostTable.GetEntry(aHost.get());

    if (!entry) {
      entry = mPrivateModeHostTable.PutEntry(aHost.get());
      if (!entry) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      SSSLOG(("Created private mode deleted mask for %s", aHost.get()));
    }

    if (strcmp(aType, STS_PERMISSION) == 0) {
      entry->mStsPermission = STS_KNOCKOUT;
    }
    else if (strcmp(aType, STS_SUBDOMAIN_PERMISSION) == 0) {
      entry->mIncludeSubdomains = false;
    }

    return NS_OK;
}
