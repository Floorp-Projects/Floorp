/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include "prlog.h"
#include "prprf.h"
#include "nsCRTGlue.h"
#include "nsIPermissionManager.h"
#include "nsIPrivateBrowsingService.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsStrictTransportSecurityService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsStringGlue.h"
#include "nsIScriptSecurityManager.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gSTSLog = PR_NewLogModule("nsSTSService");
#endif

#define STSLOG(args) PR_LOG(gSTSLog, 4, args)

#define STS_PARSER_FAIL_IF(test,args) \
  if (test) { \
    STSLOG(args); \
    return NS_ERROR_FAILURE; \
  }

namespace {

/**
 * Returns a principal (aPrincipal) corresponding to aURI.
 * This is used to interact with the permission manager.
 */
nsresult
GetPrincipalForURI(nsIURI* aURI, nsIPrincipal** aPrincipal)
{
   // The permission manager wants a principal but don't actually check a
   // permission but a data we saved in the permission manager so we are good by
   // creating a no-app codebase principal and send it to the permission manager.
   nsresult rv;
   nsCOMPtr<nsIScriptSecurityManager> securityManager =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
   NS_ENSURE_SUCCESS(rv, rv);

   return securityManager->GetNoAppCodebasePrincipal(aURI, aPrincipal);
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

nsSTSHostEntry::nsSTSHostEntry(const char* aHost)
  : mHost(aHost)
  , mExpireTime(0)
  , mDeleted(false)
  , mIncludeSubdomains(false)
{
}

nsSTSHostEntry::nsSTSHostEntry(const nsSTSHostEntry& toCopy)
  : mHost(toCopy.mHost)
  , mExpireTime(toCopy.mExpireTime)
  , mDeleted(toCopy.mDeleted)
  , mIncludeSubdomains(toCopy.mIncludeSubdomains)
{
}

////////////////////////////////////////////////////////////////////////////////


nsStrictTransportSecurityService::nsStrictTransportSecurityService()
  : mInPrivateMode(false)
{
}

nsStrictTransportSecurityService::~nsStrictTransportSecurityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsStrictTransportSecurityService,
                              nsIObserver,
                              nsIStrictTransportSecurityService)

nsresult
nsStrictTransportSecurityService::Init()
{
   nsresult rv;

   mPermMgr = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
   NS_ENSURE_SUCCESS(rv, rv);

   // figure out if we're starting in private browsing mode
   nsCOMPtr<nsIPrivateBrowsingService> pbs =
     do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
   if (pbs)
     pbs->GetPrivateBrowsingEnabled(&mInPrivateMode);

   mObserverService = mozilla::services::GetObserverService();
   if (mObserverService)
     mObserverService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, false);

   if (mInPrivateMode)
     mPrivateModeHostTable.Init();

   return NS_OK;
}

nsresult
nsStrictTransportSecurityService::GetHost(nsIURI *aURI, nsACString &aResult)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  if (!innerURI) return NS_ERROR_FAILURE;

  nsresult rv = innerURI->GetAsciiHost(aResult);

  if (NS_FAILED(rv) || aResult.IsEmpty())
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

nsresult
nsStrictTransportSecurityService::SetStsState(nsIURI* aSourceURI,
                                              PRInt64 maxage,
                                              bool includeSubdomains)
{
  // If max-age is zero, that's an indication to immediately remove the
  // permissions, so here's a shortcut.
  if (!maxage)
    return RemoveStsState(aSourceURI);

  // Expire time is millis from now.  Since STS max-age is in seconds, and
  // PR_Now() is in micros, must equalize the units at milliseconds.
  PRInt64 expiretime = (PR_Now() / 1000) + (maxage * 1000);

  // record entry for this host with max-age in the permissions manager
  STSLOG(("STS: maxage permission SET, adding permission\n"));
  nsresult rv = AddPermission(aSourceURI,
                              STS_PERMISSION,
                              (PRUint32) nsIPermissionManager::ALLOW_ACTION,
                              (PRUint32) nsIPermissionManager::EXPIRE_TIME,
                              expiretime);
  NS_ENSURE_SUCCESS(rv, rv);

  if (includeSubdomains) {
    // record entry for this host with include subdomains in the permissions manager
    STSLOG(("STS: subdomains permission SET, adding permission\n"));
    rv = AddPermission(aSourceURI,
                       STS_SUBDOMAIN_PERMISSION,
                       (PRUint32) nsIPermissionManager::ALLOW_ACTION,
                       (PRUint32) nsIPermissionManager::EXPIRE_TIME,
                       expiretime);
    NS_ENSURE_SUCCESS(rv, rv);
  } else { // !includeSubdomains
    nsCAutoString hostname;
    rv = GetHost(aSourceURI, hostname);
    NS_ENSURE_SUCCESS(rv, rv);

    STSLOG(("STS: subdomains permission UNSET, removing any existing ones\n"));
    rv = RemovePermission(hostname, STS_SUBDOMAIN_PERMISSION);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsStrictTransportSecurityService::RemoveStsState(nsIURI* aURI)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);

  nsCAutoString hostname;
  nsresult rv = GetHost(aURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = RemovePermission(hostname, STS_PERMISSION);
  NS_ENSURE_SUCCESS(rv, rv);
  STSLOG(("STS: deleted maxage permission\n"));

  rv = RemovePermission(hostname, STS_SUBDOMAIN_PERMISSION);
  NS_ENSURE_SUCCESS(rv, rv);
  STSLOG(("STS: deleted subdomains permission\n"));

  return NS_OK;
}

NS_IMETHODIMP
nsStrictTransportSecurityService::ProcessStsHeader(nsIURI* aSourceURI,
                                                   const char* aHeader)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);

  char * header = NS_strdup(aHeader);
  if (!header) return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = ProcessStsHeaderMutating(aSourceURI, header);
  NS_Free(header);
  return rv;
}

nsresult
nsStrictTransportSecurityService::ProcessStsHeaderMutating(nsIURI* aSourceURI,
                                                           char* aHeader)
{
  STSLOG(("STS: ProcessStrictTransportHeader(%s)\n", aHeader));

  // "Strict-Transport-Security" ":" OWS
  //      STS-d  *( OWS ";" OWS STS-d  OWS)
  //
  //  ; STS directive
  //  STS-d      = maxAge / includeSubDomains
  //
  //  maxAge     = "max-age" "=" delta-seconds v-ext
  //
  //  includeSubDomains = [ "includeSubDomains" ]

  const char* directive;

  bool foundMaxAge = false;
  bool foundUnrecognizedTokens = false;
  bool includeSubdomains = false;
  PRInt64 maxAge = 0;

  NS_NAMED_LITERAL_CSTRING(max_age_var, "max-age");
  NS_NAMED_LITERAL_CSTRING(include_subd_var, "includesubdomains");

  while ((directive = NS_strtok(";", &aHeader))) {
    //skip leading whitespace
    directive = NS_strspnp(" \t", directive);
    STS_PARSER_FAIL_IF(!(*directive), ("error removing initial whitespace\n."));

    if (!PL_strncasecmp(directive, max_age_var.get(), max_age_var.Length())) {
      // skip directive name
      directive += max_age_var.Length();
      // skip leading whitespace
      directive = NS_strspnp(" \t", directive);
      STS_PARSER_FAIL_IF(*directive != '=',
                  ("No equal sign found in max-age directive\n"));

      // skip over the equal sign
      STS_PARSER_FAIL_IF(*(++directive) == '\0',
                  ("No delta-seconds present\n"));

      // obtain the delta-seconds value
      STS_PARSER_FAIL_IF(PR_sscanf(directive, "%lld", &maxAge) != 1,
                  ("Could not convert delta-seconds\n"));
      STSLOG(("STS: ProcessStrictTransportHeader() STS found maxage %lld\n", maxAge));
      foundMaxAge = true;

      // skip max-age value and trailing whitespace
      directive = NS_strspnp("0123456789 \t", directive);

      // log unknown tokens, but don't fail (for forwards compatibility)
      if (*directive != '\0') {
        foundUnrecognizedTokens = true;
        STSLOG(("Extra stuff in max-age after delta-seconds: %s \n", directive));
      }
    }
    else if (!PL_strncasecmp(directive, include_subd_var.get(), include_subd_var.Length())) {
      directive += include_subd_var.Length();

      // only record "includesubdomains" if it is a token by itself... for
      // example, don't set includeSubdomains = true if the directive is
      // "includesubdomainsFooBar".
      if (*directive == '\0' || *directive =='\t' || *directive == ' ') {
        includeSubdomains = true;
        STSLOG(("STS: ProcessStrictTransportHeader: obtained subdomains status\n"));

        // skip trailing whitespace
        directive = NS_strspnp(" \t", directive);

        if (*directive != '\0') {
          foundUnrecognizedTokens = true;
          STSLOG(("Extra stuff after includesubdomains: %s\n", directive));
        }
      } else {
        foundUnrecognizedTokens = true;
        STSLOG(("Unrecognized directive in header: %s\n", directive));
      }
    }
    else {
      // log unknown directives, but don't fail (for backwards compatibility)
      foundUnrecognizedTokens = true;
      STSLOG(("Unrecognized directive in header: %s\n", directive));
    }
  }

  // after processing all the directives, make sure we came across max-age
  // somewhere.
  STS_PARSER_FAIL_IF(!foundMaxAge,
              ("Parse ERROR: couldn't locate max-age token\n"));

  // record the successfully parsed header data.
  SetStsState(aSourceURI, maxAge, includeSubdomains);

  return foundUnrecognizedTokens ?
         NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA :
         NS_OK;
}

NS_IMETHODIMP
nsStrictTransportSecurityService::IsStsHost(const char* aHost, bool* aResult)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIURI> uri;
  nsDependentCString hostString(aHost);
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                          NS_LITERAL_CSTRING("https://") + hostString);
  NS_ENSURE_SUCCESS(rv, rv);
  return IsStsURI(uri, aResult);
}

NS_IMETHODIMP
nsStrictTransportSecurityService::IsStsURI(nsIURI* aURI, bool* aResult)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);

  nsresult rv;
  PRUint32 permExact, permGeneral;
  // If this domain has the forcehttps permission, this is an STS host.
  rv = TestPermission(aURI, STS_PERMISSION, &permExact, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // If any super-domain has the includeSubdomains permission, this is an
  // STS host.
  rv = TestPermission(aURI, STS_SUBDOMAIN_PERMISSION, &permGeneral, false);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = ((permExact   == nsIPermissionManager::ALLOW_ACTION) ||
              (permGeneral == nsIPermissionManager::ALLOW_ACTION));
  return NS_OK;
}


// Verify the trustworthiness of the security info (are there any cert errors?)
NS_IMETHODIMP
nsStrictTransportSecurityService::ShouldIgnoreStsHeader(nsISupports* aSecurityInfo,
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
// nsStrictTransportSecurityService::nsIObserver
//------------------------------------------------------------

NS_IMETHODIMP
nsStrictTransportSecurityService::Observe(nsISupports *subject,
                                          const char *topic,
                                          const PRUnichar *data)
{
  if (strcmp(topic, NS_PRIVATE_BROWSING_SWITCH_TOPIC) == 0) {
    if(NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(data)) {
      // Indication to start recording stuff locally and not writing changes
      // out to the permission manager.

      if (!mPrivateModeHostTable.IsInitialized()) {
        mPrivateModeHostTable.Init();
      }
      mInPrivateMode = true;
    }
    else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(data)) {
      mPrivateModeHostTable.Clear();
      mInPrivateMode = false;
    }
  }

  return NS_OK;
}

//------------------------------------------------------------
// Functions to overlay the permission manager calls in case
// we're in private browsing mode.
//------------------------------------------------------------
nsresult
nsStrictTransportSecurityService::AddPermission(nsIURI     *aURI,
                                                const char *aType,
                                                PRUint32   aPermission,
                                                PRUint32   aExpireType,
                                                PRInt64    aExpireTime)
{
    // Private mode doesn't address user-set (EXPIRE_NEVER) permissions: let
    // those be stored persistently.
    if (!mInPrivateMode || aExpireType == nsIPermissionManager::EXPIRE_NEVER) {
      // Not in private mode, or manually-set permission
      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = GetPrincipalForURI(aURI, getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      return mPermMgr->AddFromPrincipal(principal, aType, aPermission,
                                        aExpireType, aExpireTime);
    }

    nsCAutoString host;
    nsresult rv = GetHost(aURI, host);
    NS_ENSURE_SUCCESS(rv, rv);
    STSLOG(("AddPermission for entry for for %s", host.get()));

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
    nsSTSHostEntry* entry = mPrivateModeHostTable.PutEntry(host.get());
    STSLOG(("Created private mode entry for for %s", host.get()));

    // AddPermission() will be called twice if the STS header encountered has
    // includeSubdomains (first for the main permission and second for the
    // subdomains permission). If AddPermission() gets called a second time
    // with the STS_SUBDOMAIN_PERMISSION, we just have to flip that bit in
    // the nsSTSHostEntry.
    if (strcmp(aType, STS_SUBDOMAIN_PERMISSION) == 0) {
      entry->mIncludeSubdomains = true;
    }
    // for the case where PutEntry() returned an existing host entry, make
    // sure it's not set as deleted (which might have happened in the past).
    entry->mDeleted = false;

    // Also refresh the expiration time.
    entry->mExpireTime = aExpireTime;
    return NS_OK;

}

nsresult
nsStrictTransportSecurityService::RemovePermission(const nsCString  &aHost,
                                                   const char       *aType)
{
    // Build up a principal for use with the permission manager.
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri),
                            NS_LITERAL_CSTRING("http://") + aHost);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal;
    rv = GetPrincipalForURI(uri, getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mInPrivateMode) {
      // Not in private mode: remove permissions persistently.
      return mPermMgr->RemoveFromPrincipal(principal, aType);
    }

    // Make changes in mPrivateModeHostTable only, so any changes will be
    // rolled back when exiting private mode.
    nsSTSHostEntry* entry = mPrivateModeHostTable.GetEntry(aHost.get());

    // Check to see if there's STS data stored for this host in the
    // permission manager (probably set outside private mode).
    PRUint32 permmgrValue;
    rv = mPermMgr->TestExactPermissionFromPrincipal(principal, aType,
                                                    &permmgrValue);
    NS_ENSURE_SUCCESS(rv, rv);

    // If there is STS data in the permission manager, store a "deleted" mask
    // for the permission in mPrivateModeHostTable (either update
    // mPrivateModeHostTable to have the deleted mask, or add one).
    // This is because we don't want removals that happen in private mode to
    // be reflected when private mode is exited -- but while in private mode
    // we still want the effect of the removal.
    if (permmgrValue != nsIPermissionManager::UNKNOWN_ACTION) {
      // if there's no entry in mPrivateModeHostTable, we have to make one.
      if (!entry) {
        entry = mPrivateModeHostTable.PutEntry(aHost.get());
        STSLOG(("Created private mode deleted mask for for %s", aHost.get()));
      }
      entry->mDeleted = true;
      entry->mIncludeSubdomains = false;
      return NS_OK;
    }

    // Otherwise, permission doesn't exist in the real permission manager, so
    // there's nothing to "pretend" to delete.  I'ts ok to delete any copy in
    // mPrivateModeHostTable.
    if (entry) mPrivateModeHostTable.RawRemoveEntry(entry);
    return NS_OK;
}

nsresult
nsStrictTransportSecurityService::TestPermission(nsIURI     *aURI,
                                                 const char *aType,
                                                 PRUint32   *aPermission,
                                                 bool       testExact)
{
    // set default for if we can't find any STS information
    *aPermission = nsIPermissionManager::UNKNOWN_ACTION;

    if (!mInPrivateMode) {
      // if not in private mode, just delegate to the permission manager.
      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = GetPrincipalForURI(aURI, getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      if (testExact)
        return mPermMgr->TestExactPermissionFromPrincipal(principal, aType, aPermission);
      else
        return mPermMgr->TestPermissionFromPrincipal(principal, aType, aPermission);
    }

    nsCAutoString host;
    nsresult rv = GetHost(aURI, host);
    if (NS_FAILED(rv)) return NS_OK;

    nsSTSHostEntry *entry;
    PRUint32 actualExactPermission;
    PRUint32 offset = 0;
    PRInt64 now = PR_Now() / 1000;

    // Used for testing permissions as we walk up the domain tree.
    nsCOMPtr<nsIURI> domainWalkURI;

    // In parallel, loop over private mode cache and also the real permission
    // manager--ignoring any masked as "deleted" in the local cache. We have
    // to do this here since the most specific permission in *either* the
    // permission manager or mPrivateModeHostTable should be used.
    do {
      entry = mPrivateModeHostTable.GetEntry(host.get() + offset);
      STSLOG(("Checking PM Table entry and permmgr for %s", host.get()+offset));

      // flag as deleted any entries encountered that have expired.  We only
      // flag the nsSTSHostEntry because there could be some data in the
      // permission manager that -- if not in private mode -- would have been
      // overwritten by newly encountered STS data.
      if (entry && (now > entry->mExpireTime)) {
        STSLOG(("Deleting expired PM Table entry for %s", host.get()+offset));
        entry->mDeleted = true;
        entry->mIncludeSubdomains = false;
      }

      rv = NS_NewURI(getter_AddRefs(domainWalkURI),
                      NS_LITERAL_CSTRING("http://") + Substring(host, offset));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = GetPrincipalForURI(domainWalkURI, getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mPermMgr->TestExactPermissionFromPrincipal(principal, aType,
                                                      &actualExactPermission);
      NS_ENSURE_SUCCESS(rv, rv);

      // There are three cases as we walk up the hostname testing
      // permissions:
      // 1. There's no entry in mPrivateModeHostTable for this host; rely
      // on data in the permission manager
      if (!entry) {
        if (actualExactPermission != nsIPermissionManager::UNKNOWN_ACTION) {
          // no cached data but a permission in the permission manager so use
          // it and stop looking.
          *aPermission = actualExactPermission;
          STSLOG(("no PM Table entry for %s, using permmgr", host.get()+offset));
          break;
        }
      }
      // 2. There's a "deleted" mask in mPrivateModeHostTable for this host
      // or we're looking for includeSubdomain information and it's not set:
      // any data in the permission manager must be ignored, since the
      // permission would have been deleted if not in private mode.
      else if (entry->mDeleted || (strcmp(aType, STS_SUBDOMAIN_PERMISSION) == 0
                                  && !entry->mIncludeSubdomains)) {
        STSLOG(("no entry at all for %s, walking up", host.get()+offset));
        // keep looking
      }
      // 3. There's a non-deleted entry in mPrivateModeHostTable for this
      // host, so it should be used.
      else {
        // All STS permissions' values are ALLOW_ACTION or they are not
        // known (as in, not set or turned off).
        *aPermission = nsIPermissionManager::ALLOW_ACTION;
        STSLOG(("PM Table entry for %s: forcing", host.get()+offset));
        break;
      }

      // Don't continue walking up the host segments if the test was for an
      // exact match only.
      if (testExact) break;

      STSLOG(("no PM Table entry or permmgr data for %s, walking up domain",
              host.get()+offset));
      // walk up the host segments
      offset = host.FindChar('.', offset) + 1;
    } while (offset > 0);

    // Use whatever we ended up with, which defaults to UNKNOWN_ACTION.
    return NS_OK;
}
