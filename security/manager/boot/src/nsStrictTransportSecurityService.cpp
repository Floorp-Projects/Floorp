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
 * The Original Code is Strict-Transport-Security.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Sid Stamm <sid@mozilla.com>
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

#include "plstr.h"
#include "prlog.h"
#include "prprf.h"
#include "nsCRTGlue.h"
#include "nsIPermissionManager.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsStrictTransportSecurityService.h"
#include "nsIURI.h"
#include "nsInt64.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsStringGlue.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gSTSLog = PR_NewLogModule("nsSTSService");
#endif

#define STSLOG(args) PR_LOG(gSTSLog, 4, args)

#define STS_PARSER_FAIL_IF(test,args) \
  if (test) { \
    STSLOG(args); \
    return NS_ERROR_FAILURE; \
  }

nsStrictTransportSecurityService::nsStrictTransportSecurityService()
{
}

nsStrictTransportSecurityService::~nsStrictTransportSecurityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStrictTransportSecurityService,
                              nsIStrictTransportSecurityService)

nsresult
nsStrictTransportSecurityService::Init()
{
   nsresult rv;

   mPermMgr = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
   NS_ENSURE_SUCCESS(rv, rv);

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
                                              PRBool includeSubdomains)
{
  // If max-age is zero, that's an indication to immediately remove the
  // permissions, so here's a shortcut.
  if (!maxage)
    return RemoveStsState(aSourceURI);

  // Expire time is millis from now.  Since STS max-age is in seconds, and
  // PR_Now() is in micros, must equalize the units at milliseconds.
  PRInt64 expiretime = (PR_Now() / 1000) + (maxage * 1000);

  // record entry for this host with max-age in the permissions manager
  mPermMgr->Add(aSourceURI, STS_PERMISSION,
                              (PRUint32) nsIPermissionManager::ALLOW_ACTION,
                              (PRUint32) nsIPermissionManager::EXPIRE_TIME,
                              expiretime);
  STSLOG(("STS: set maxage permission\n"));

  if (includeSubdomains) {
    // record entry for this host with include subdomains in the permissions manager
    mPermMgr->Add(aSourceURI, STS_SUBDOMAIN_PERMISSION,
                                (PRUint32) nsIPermissionManager::ALLOW_ACTION,
                                (PRUint32) nsIPermissionManager::EXPIRE_TIME,
                                expiretime);
    STSLOG(("STS: set subdomains permission\n"));
  } else { // !includeSubdomains
    nsCAutoString hostname;
    nsresult rv = GetHost(aSourceURI, hostname);
    NS_ENSURE_SUCCESS(rv, rv);

    mPermMgr->Remove(hostname, STS_SUBDOMAIN_PERMISSION);
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

  mPermMgr->Remove(hostname, STS_PERMISSION);
  STSLOG(("STS: deleted maxage permission\n"));

  mPermMgr->Remove(hostname, STS_SUBDOMAIN_PERMISSION);
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

  PRBool foundMaxAge = PR_FALSE;
  PRBool foundUnrecognizedTokens = PR_FALSE;
  PRBool includeSubdomains = PR_FALSE;
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
      foundMaxAge = PR_TRUE;

      // skip max-age value and trailing whitespace
      directive = NS_strspnp("0123456789 \t", directive);

      // log unknown tokens, but don't fail (for forwards compatibility)
      if (*directive != '\0') {
        foundUnrecognizedTokens = PR_TRUE;
        STSLOG(("Extra stuff in max-age after delta-seconds: %s \n", directive));
      }
    }
    else if (!PL_strncasecmp(directive, include_subd_var.get(), include_subd_var.Length())) {
      directive += include_subd_var.Length();

      // only record "includesubdomains" if it is a token by itself... for
      // example, don't set includeSubdomains = PR_TRUE if the directive is
      // "includesubdomainsFooBar".
      if (*directive == '\0' || *directive =='\t' || *directive == ' ') {
        includeSubdomains = PR_TRUE;
        STSLOG(("STS: ProcessStrictTransportHeader: obtained subdomains status\n"));

        // skip trailing whitespace
        directive = NS_strspnp(" \t", directive);

        if (*directive != '\0') {
          foundUnrecognizedTokens = PR_TRUE;
          STSLOG(("Extra stuff after includesubdomains: %s\n", directive));
        }
      } else {
        foundUnrecognizedTokens = PR_TRUE;
        STSLOG(("Unrecognized directive in header: %s\n", directive));
      }
    }
    else {
      // log unknown directives, but don't fail (for backwards compatibility)
      foundUnrecognizedTokens = PR_TRUE;
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
nsStrictTransportSecurityService::IsStsHost(const char* aHost, PRBool* aResult)
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
nsStrictTransportSecurityService::IsStsURI(nsIURI* aURI, PRBool* aResult)
{
  // Should be called on the main thread (or via proxy) since the permission
  // manager is used and it's not threadsafe.
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);

  nsresult rv;
  PRUint32 permExact, permGeneral;
  // If this domain has the forcehttps permission, this is an STS host.
  rv = mPermMgr->TestExactPermission(aURI, STS_PERMISSION, &permExact);
  NS_ENSURE_SUCCESS(rv, rv);
  // If any super-domain has the includeSubdomains permission, this is an
  // STS host.
  rv = mPermMgr->TestPermission(aURI, STS_SUBDOMAIN_PERMISSION, &permGeneral);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = ((permExact   == nsIPermissionManager::ALLOW_ACTION) ||
              (permGeneral == nsIPermissionManager::ALLOW_ACTION));
  return NS_OK;
}


// Verify the trustworthiness of the security info (are there any cert errors?)
NS_IMETHODIMP
nsStrictTransportSecurityService::ShouldIgnoreStsHeader(nsISupports* aSecurityInfo,
                                                        PRBool* aResult)
{
  nsresult rv;
  PRBool tlsIsBroken = PR_FALSE;
  nsCOMPtr<nsISSLStatusProvider> sslprov = do_QueryInterface(aSecurityInfo);
  NS_ENSURE_TRUE(sslprov, NS_ERROR_FAILURE);

  nsCOMPtr<nsISupports> isupstat;
  rv = sslprov->GetSSLStatus(getter_AddRefs(isupstat));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISSLStatus> sslstat = do_QueryInterface(isupstat);
  NS_ENSURE_TRUE(sslstat, NS_ERROR_FAILURE);

  PRBool trustcheck;
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
