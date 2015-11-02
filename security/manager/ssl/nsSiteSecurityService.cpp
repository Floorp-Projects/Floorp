/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSiteSecurityService.h"

#include "mozilla/LinkedList.h"
#include "mozilla/Preferences.h"
#include "mozilla/Base64.h"
#include "base64.h"
#include "CertVerifier.h"
#include "nsCRTGlue.h"
#include "nsISSLStatus.h"
#include "nsISocketProvider.h"
#include "nsIURI.h"
#include "nsIX509Cert.h"
#include "nsNetUtil.h"
#include "nsNSSComponent.h"
#include "nsSecurityHeaderParser.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "pkix/pkixtypes.h"
#include "plstr.h"
#include "mozilla/Logging.h"
#include "prnetdb.h"
#include "prprf.h"
#include "PublicKeyPinningService.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"

// A note about the preload list:
// When a site specifically disables HSTS by sending a header with
// 'max-age: 0', we keep a "knockout" value that means "we have no information
// regarding the HSTS state of this host" (any ancestor of "this host" can still
// influence its HSTS status via include subdomains, however).
// This prevents the preload list from overriding the site's current
// desired HSTS status.
#include "nsSTSPreloadList.inc"

using namespace mozilla;
using namespace mozilla::psm;

static PRLogModuleInfo *
GetSSSLog()
{
  static PRLogModuleInfo *gSSSLog;
  if (!gSSSLog)
    gSSSLog = PR_NewLogModule("nsSSService");
  return gSSSLog;
}

#define SSSLOG(args) MOZ_LOG(GetSSSLog(), mozilla::LogLevel::Debug, args)

////////////////////////////////////////////////////////////////////////////////

SiteHSTSState::SiteHSTSState(nsCString& aStateString)
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
    SSSLOG(("%s is not a valid SiteHSTSState", aStateString.get()));
    mHSTSExpireTime = 0;
    mHSTSState = SecurityPropertyUnset;
    mHSTSIncludeSubdomains = false;
  }
}

SiteHSTSState::SiteHSTSState(PRTime aHSTSExpireTime,
                             SecurityPropertyState aHSTSState,
                             bool aHSTSIncludeSubdomains)

  : mHSTSExpireTime(aHSTSExpireTime)
  , mHSTSState(aHSTSState)
  , mHSTSIncludeSubdomains(aHSTSIncludeSubdomains)
{
}

void
SiteHSTSState::ToString(nsCString& aString)
{
  aString.Truncate();
  aString.AppendInt(mHSTSExpireTime);
  aString.Append(',');
  aString.AppendInt(mHSTSState);
  aString.Append(',');
  aString.AppendInt(static_cast<uint32_t>(mHSTSIncludeSubdomains));
}

////////////////////////////////////////////////////////////////////////////////
static bool
stringIsBase64EncodingOf256bitValue(nsCString& encodedString) {
  nsAutoCString binaryValue;
  nsresult rv = mozilla::Base64Decode(encodedString, binaryValue);
  if (NS_FAILED(rv)) {
    return false;
  }
  if (binaryValue.Length() != SHA256_LENGTH) {
    return false;
  }
  return true;
}

SiteHPKPState::SiteHPKPState()
  : mExpireTime(0)
  , mState(SecurityPropertyUnset)
  , mIncludeSubdomains(false)
{
}

SiteHPKPState::SiteHPKPState(nsCString& aStateString)
  : mExpireTime(0)
  , mState(SecurityPropertyUnset)
  , mIncludeSubdomains(false)
{
  uint32_t hpkpState = 0;
  uint32_t hpkpIncludeSubdomains = 0; // PR_sscanf doesn't handle bools.
  const uint32_t MaxMergedHPKPPinSize = 1024;
  char mergedHPKPins[MaxMergedHPKPPinSize];
  memset(mergedHPKPins, 0, MaxMergedHPKPPinSize);

  if (aStateString.Length() >= MaxMergedHPKPPinSize) {
    SSSLOG(("SSS: Cannot parse PKPState string, too large\n"));
    return;
  }

  int32_t matches = PR_sscanf(aStateString.get(), "%lld,%lu,%lu,%s",
                              &mExpireTime, &hpkpState,
                              &hpkpIncludeSubdomains, mergedHPKPins);
  bool valid = (matches == 4 &&
                (hpkpIncludeSubdomains == 0 || hpkpIncludeSubdomains == 1) &&
                ((SecurityPropertyState)hpkpState == SecurityPropertyUnset ||
                 (SecurityPropertyState)hpkpState == SecurityPropertySet ||
                 (SecurityPropertyState)hpkpState == SecurityPropertyKnockout));

  SSSLOG(("SSS: loading SiteHPKPState matches=%d\n", matches));
  const uint32_t SHA256Base64Len = 44;

  if (valid && (SecurityPropertyState)hpkpState == SecurityPropertySet) {
    // try to expand the merged PKPins
    const char* cur = mergedHPKPins;
    nsAutoCString pin;
    uint32_t collectedLen = 0;
    mergedHPKPins[MaxMergedHPKPPinSize - 1] = 0;
    size_t totalLen = strlen(mergedHPKPins);
    while (collectedLen + SHA256Base64Len <= totalLen) {
      pin.Assign(cur, SHA256Base64Len);
      if (stringIsBase64EncodingOf256bitValue(pin)) {
        mSHA256keys.AppendElement(pin);
      }
      cur += SHA256Base64Len;
      collectedLen += SHA256Base64Len;
    }
    if (mSHA256keys.IsEmpty()) {
      valid = false;
    }
  }
  if (valid) {
    mState = (SecurityPropertyState)hpkpState;
    mIncludeSubdomains = (hpkpIncludeSubdomains == 1);
  } else {
    SSSLOG(("%s is not a valid SiteHPKPState", aStateString.get()));
    mExpireTime = 0;
    mState = SecurityPropertyUnset;
    mIncludeSubdomains = false;
    if (!mSHA256keys.IsEmpty()) {
      mSHA256keys.Clear();
    }
  }
}

SiteHPKPState::SiteHPKPState(PRTime aExpireTime,
                             SecurityPropertyState aState,
                             bool aIncludeSubdomains,
                             nsTArray<nsCString>& aSHA256keys)
  : mExpireTime(aExpireTime)
  , mState(aState)
  , mIncludeSubdomains(aIncludeSubdomains)
  , mSHA256keys(aSHA256keys)
{
}

void
SiteHPKPState::ToString(nsCString& aString)
{
  aString.Truncate();
  aString.AppendInt(mExpireTime);
  aString.Append(',');
  aString.AppendInt(mState);
  aString.Append(',');
  aString.AppendInt(static_cast<uint32_t>(mIncludeSubdomains));
  aString.Append(',');
  for (unsigned int i = 0; i < mSHA256keys.Length(); i++) {
    aString.Append(mSHA256keys[i]);
  }
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
   if (!XRE_IsParentProcess()) {
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
  mProcessPKPHeadersFromNonBuiltInRoots = mozilla::Preferences::GetBool(
    "security.cert_pinning.process_headers_from_non_builtin_roots", false);
  mozilla::Preferences::AddStrongObserver(this,
    "security.cert_pinning.process_headers_from_non_builtin_roots");
  mPreloadListTimeOffset = mozilla::Preferences::GetInt(
    "test.currentTimeOffsetSeconds", 0);
  mozilla::Preferences::AddStrongObserver(this,
    "test.currentTimeOffsetSeconds");
  mSiteStateStorage =
    mozilla::DataStorage::Get(NS_LITERAL_STRING("SiteSecurityServiceState.txt"));
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
nsSiteSecurityService::GetHost(nsIURI* aURI, nsACString& aResult)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  if (!innerURI) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString host;
  nsresult rv = innerURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aResult.Assign(PublicKeyPinningService::CanonicalizeHostname(host.get()));
  if (aResult.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

static void
SetStorageKey(nsAutoCString& storageKey, nsCString& hostname, uint32_t aType)
{
  storageKey = hostname;
  switch (aType) {
    case nsISiteSecurityService::HEADER_HSTS:
      storageKey.AppendLiteral(":HSTS");
      break;
    case nsISiteSecurityService::HEADER_HPKP:
      storageKey.AppendLiteral(":HPKP");
      break;
    default:
      NS_ASSERTION(false, "SSS:SetStorageKey got invalid type");
  }
}

// Expire times are in millis.  Since Headers max-age is in seconds, and
// PR_Now() is in micros, normalize the units at milliseconds.
static int64_t
ExpireTimeFromMaxAge(int64_t maxAge)
{
  return (PR_Now() / PR_USEC_PER_MSEC) + (maxAge * PR_MSEC_PER_SEC);
}

nsresult
nsSiteSecurityService::SetHSTSState(uint32_t aType,
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

  int64_t expiretime = ExpireTimeFromMaxAge(maxage);
  SiteHSTSState siteState(expiretime, SecurityPropertySet, includeSubdomains);
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
  nsAutoCString storageKey;
  SetStorageKey(storageKey, hostname, aType);
  rv = mSiteStateStorage->Put(storageKey, stateString, storageType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::RemoveState(uint32_t aType, nsIURI* aURI,
                                   uint32_t aFlags)
{
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                 aType == nsISiteSecurityService::HEADER_HPKP,
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
    SiteHSTSState siteState(0, SecurityPropertyKnockout, false);
    nsAutoCString stateString;
    siteState.ToString(stateString);
    nsAutoCString storageKey;
    SetStorageKey(storageKey, hostname, aType);
    rv = mSiteStateStorage->Put(storageKey, stateString, storageType);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    SSSLOG(("SSS: removing entry for %s", hostname.get()));
    nsAutoCString storageKey;
    SetStorageKey(storageKey, hostname, aType);
    mSiteStateStorage->Remove(storageKey, storageType);
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
                                     nsISSLStatus* aSSLStatus,
                                     uint32_t aFlags,
                                     uint64_t* aMaxAge,
                                     bool* aIncludeSubdomains,
                                     uint32_t* aFailureResult)
{
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                 aType == nsISiteSecurityService::HEADER_HPKP,
                 NS_ERROR_NOT_IMPLEMENTED);

  NS_ENSURE_ARG(aSSLStatus);
  return ProcessHeaderInternal(aType, aSourceURI, aHeader, aSSLStatus, aFlags,
                               aMaxAge, aIncludeSubdomains, aFailureResult);
}

NS_IMETHODIMP
nsSiteSecurityService::UnsafeProcessHeader(uint32_t aType,
                                           nsIURI* aSourceURI,
                                           const char* aHeader,
                                           uint32_t aFlags,
                                           uint64_t* aMaxAge,
                                           bool* aIncludeSubdomains,
                                           uint32_t* aFailureResult)
{
  return ProcessHeaderInternal(aType, aSourceURI, aHeader, nullptr, aFlags,
                               aMaxAge, aIncludeSubdomains, aFailureResult);
}

nsresult
nsSiteSecurityService::ProcessHeaderInternal(uint32_t aType,
                                             nsIURI* aSourceURI,
                                             const char* aHeader,
                                             nsISSLStatus* aSSLStatus,
                                             uint32_t aFlags,
                                             uint64_t* aMaxAge,
                                             bool* aIncludeSubdomains,
                                             uint32_t* aFailureResult)
{
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  // Only HSTS and HPKP are supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                 aType == nsISiteSecurityService::HEADER_HPKP,
                 NS_ERROR_NOT_IMPLEMENTED);

  if (aMaxAge != nullptr) {
    *aMaxAge = 0;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = false;
  }

  if (aSSLStatus) {
    bool tlsIsBroken = false;
    bool trustcheck;
    nsresult rv;
    rv = aSSLStatus->GetIsDomainMismatch(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;

    rv = aSSLStatus->GetIsNotValidAtThisTime(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;

    rv = aSSLStatus->GetIsUntrusted(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;
    if (tlsIsBroken) {
       SSSLOG(("SSS: discarding header from untrustworthy connection"));
       if (aFailureResult) {
         *aFailureResult = nsISiteSecurityService::ERROR_UNTRUSTWORTHY_CONNECTION;
       }
      return NS_ERROR_FAILURE;
    }
  }

  nsAutoCString host;
  nsresult rv = GetHost(aSourceURI, host);
  NS_ENSURE_SUCCESS(rv, rv);
  if (HostIsIPAddress(host.get())) {
    /* Don't process headers if a site is accessed by IP address. */
    return NS_OK;
  }

  switch (aType) {
    case nsISiteSecurityService::HEADER_HSTS:
      rv = ProcessSTSHeader(aSourceURI, aHeader, aFlags, aMaxAge,
                            aIncludeSubdomains, aFailureResult);
      break;
    case nsISiteSecurityService::HEADER_HPKP:
      rv = ProcessPKPHeader(aSourceURI, aHeader, aSSLStatus, aFlags, aMaxAge,
                            aIncludeSubdomains, aFailureResult);
      break;
    default:
      MOZ_CRASH("unexpected header type");
  }
  return rv;
}

static uint32_t
ParseSSSHeaders(uint32_t aType,
                const char* aHeader,
                bool& foundIncludeSubdomains,
                bool& foundMaxAge,
                bool& foundUnrecognizedDirective,
                int64_t& maxAge,
                nsTArray<nsCString>& sha256keys)
{
  // Strict transport security and Public Key Pinning have very similar
  // Header formats.

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

  // "Public-Key-Pins ":" OWS
  //      PKP-d  *( OWS ";" OWS PKP-d  OWS)
  //
  //  ; PKP directive
  //  PKP-d      = maxAge / includeSubDomains / reportUri / pin-directive
  //
  //  maxAge     = "max-age" "=" delta-seconds v-ext
  //
  //  includeSubDomains = [ "includeSubDomains" ]
  //
  //  reportURi  = "report-uri" "=" quoted-string
  //
  //  pin-directive = "pin-" token "=" quoted-string
  //
  //  the only valid token currently specified is sha256
  //  the quoted string for a pin directive is the base64 encoding
  //  of the hash of the public key of the fingerprint
  //

  //  The order of the directives is not significant.
  //  All directives must appear only once.
  //  Directive names are case-insensitive.
  //  The entire header is invalid if a directive not conforming to the
  //  syntax is encountered.
  //  Unrecognized directives (that are otherwise syntactically valid) are
  //  ignored, and the rest of the header is parsed as normal.

  bool foundReportURI = false;

  NS_NAMED_LITERAL_CSTRING(max_age_var, "max-age");
  NS_NAMED_LITERAL_CSTRING(include_subd_var, "includesubdomains");
  NS_NAMED_LITERAL_CSTRING(pin_sha256_var, "pin-sha256");
  NS_NAMED_LITERAL_CSTRING(report_uri_var, "report-uri");

  nsSecurityHeaderParser parser(aHeader);
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: could not parse header"));
    return nsISiteSecurityService::ERROR_COULD_NOT_PARSE_HEADER;
  }
  mozilla::LinkedList<nsSecurityHeaderDirective>* directives = parser.GetDirectives();

  for (nsSecurityHeaderDirective* directive = directives->getFirst();
       directive != nullptr; directive = directive->getNext()) {
    SSSLOG(("SSS: found directive %s\n", directive->mName.get()));
    if (directive->mName.Length() == max_age_var.Length() &&
        directive->mName.EqualsIgnoreCase(max_age_var.get(),
                                          max_age_var.Length())) {
      if (foundMaxAge) {
        SSSLOG(("SSS: found two max-age directives"));
        return nsISiteSecurityService::ERROR_MULTIPLE_MAX_AGES;
      }

      SSSLOG(("SSS: found max-age directive"));
      foundMaxAge = true;

      size_t len = directive->mValue.Length();
      for (size_t i = 0; i < len; i++) {
        char chr = directive->mValue.CharAt(i);
        if (chr < '0' || chr > '9') {
          SSSLOG(("SSS: invalid value for max-age directive"));
          return nsISiteSecurityService::ERROR_INVALID_MAX_AGE;
        }
      }

      if (PR_sscanf(directive->mValue.get(), "%lld", &maxAge) != 1) {
        SSSLOG(("SSS: could not parse delta-seconds"));
        return nsISiteSecurityService::ERROR_INVALID_MAX_AGE;
      }

      SSSLOG(("SSS: parsed delta-seconds: %lld", maxAge));
    } else if (directive->mName.Length() == include_subd_var.Length() &&
               directive->mName.EqualsIgnoreCase(include_subd_var.get(),
                                                 include_subd_var.Length())) {
      if (foundIncludeSubdomains) {
        SSSLOG(("SSS: found two includeSubdomains directives"));
        return nsISiteSecurityService::ERROR_MULTIPLE_INCLUDE_SUBDOMAINS;
      }

      SSSLOG(("SSS: found includeSubdomains directive"));
      foundIncludeSubdomains = true;

      if (directive->mValue.Length() != 0) {
        SSSLOG(("SSS: includeSubdomains directive unexpectedly had value '%s'",
                directive->mValue.get()));
        return nsISiteSecurityService::ERROR_INVALID_INCLUDE_SUBDOMAINS;
      }
    } else if (aType == nsISiteSecurityService::HEADER_HPKP &&
               directive->mName.Length() == pin_sha256_var.Length() &&
               directive->mName.EqualsIgnoreCase(pin_sha256_var.get(),
                                                 pin_sha256_var.Length())) {
       SSSLOG(("SSS: found pinning entry '%s' length=%d",
               directive->mValue.get(), directive->mValue.Length()));
       if (!stringIsBase64EncodingOf256bitValue(directive->mValue)) {
         return nsISiteSecurityService::ERROR_INVALID_PIN;
       }
       sha256keys.AppendElement(directive->mValue);
   } else if (aType == nsISiteSecurityService::HEADER_HPKP &&
              directive->mName.Length() == report_uri_var.Length() &&
              directive->mName.EqualsIgnoreCase(report_uri_var.get(),
                                                report_uri_var.Length())) {
       // We don't support the report-uri yet, but to avoid unrecognized
       // directive warnings, we still have to handle its presence
      if (foundReportURI) {
        SSSLOG(("SSS: found two report-uri directives"));
        return nsISiteSecurityService::ERROR_MULTIPLE_REPORT_URIS;
      }
      SSSLOG(("SSS: found report-uri directive"));
      foundReportURI = true;
   } else {
      SSSLOG(("SSS: ignoring unrecognized directive '%s'",
              directive->mName.get()));
      foundUnrecognizedDirective = true;
    }
  }
  return nsISiteSecurityService::Success;
}

nsresult
nsSiteSecurityService::ProcessPKPHeader(nsIURI* aSourceURI,
                                        const char* aHeader,
                                        nsISSLStatus* aSSLStatus,
                                        uint32_t aFlags,
                                        uint64_t* aMaxAge,
                                        bool* aIncludeSubdomains,
                                        uint32_t* aFailureResult)
{
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  SSSLOG(("SSS: processing HPKP header '%s'", aHeader));
  NS_ENSURE_ARG(aSSLStatus);

  const uint32_t aType = nsISiteSecurityService::HEADER_HPKP;
  bool foundMaxAge = false;
  bool foundIncludeSubdomains = false;
  bool foundUnrecognizedDirective = false;
  int64_t maxAge = 0;
  nsTArray<nsCString> sha256keys;
  uint32_t sssrv = ParseSSSHeaders(aType, aHeader, foundIncludeSubdomains,
                                   foundMaxAge, foundUnrecognizedDirective,
                                   maxAge, sha256keys);
  if (sssrv != nsISiteSecurityService::Success) {
    if (aFailureResult) {
      *aFailureResult = sssrv;
    }
    return NS_ERROR_FAILURE;
  }

  // after processing all the directives, make sure we came across max-age
  // somewhere.
  if (!foundMaxAge) {
    SSSLOG(("SSS: did not encounter required max-age directive"));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_NO_MAX_AGE;
    }
    return NS_ERROR_FAILURE;
  }

  // before we add the pin we need to ensure it will not break the site as
  // currently visited so:
  // 1. recompute a valid chain (no external ocsp)
  // 2. use this chain to check if things would have broken!
  nsAutoCString host;
  nsresult rv = GetHost(aSourceURI, host);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIX509Cert> cert;
  rv = aSSLStatus->GetServerCert(getter_AddRefs(cert));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(cert, NS_ERROR_FAILURE);
  ScopedCERTCertificate nssCert(cert->GetCert());
  NS_ENSURE_TRUE(nssCert, NS_ERROR_FAILURE);

  mozilla::pkix::Time now(mozilla::pkix::Now());
  ScopedCERTCertList certList;
  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_UNEXPECTED);
  if (certVerifier->VerifySSLServerCert(nssCert, nullptr, // stapled ocsp
                                        now, nullptr, // pinarg
                                        host.get(), // hostname
                                        false, // don't store intermediates
                                        CertVerifier::FLAG_LOCAL_ONLY,
                                        &certList) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  if (CERT_LIST_END(rootNode, certList)) {
    return NS_ERROR_FAILURE;
  }
  bool isBuiltIn = false;
  SECStatus srv = IsCertBuiltInRoot(rootNode->cert, isBuiltIn);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  if (!isBuiltIn && !mProcessPKPHeadersFromNonBuiltInRoots) {
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_ROOT_NOT_BUILT_IN;
    }
    return NS_ERROR_FAILURE;
  }

  // if maxAge == 0 we must delete all state, for now no hole-punching
  if (maxAge == 0) {
    return RemoveState(aType, aSourceURI, aFlags);
  }

  bool chainMatchesPinset;
  rv = PublicKeyPinningService::ChainMatchesPinset(certList, sha256keys,
                                                   chainMatchesPinset);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!chainMatchesPinset) {
    // is invalid
    SSSLOG(("SSS: Pins provided by %s are invalid no match with certList\n", host.get()));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_PINSET_DOES_NOT_MATCH_CHAIN;
    }
    return NS_ERROR_FAILURE;
  }

  // finally we need to ensure that there is a "backup pin" ie. There must be
  // at least one fingerprint hash that does NOT validate against the verified
  // chain (Section 2.5 of the spec)
  bool hasBackupPin = false;
  for (uint32_t i = 0; i < sha256keys.Length(); i++) {
    nsTArray<nsCString> singlePin;
    singlePin.AppendElement(sha256keys[i]);
    rv = PublicKeyPinningService::ChainMatchesPinset(certList, singlePin,
                                                     chainMatchesPinset);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!chainMatchesPinset) {
      hasBackupPin = true;
    }
  }
  if (!hasBackupPin) {
     // is invalid
    SSSLOG(("SSS: Pins provided by %s are invalid no backupPin\n", host.get()));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_NO_BACKUP_PIN;
    }
    return NS_ERROR_FAILURE;
  }

  int64_t expireTime = ExpireTimeFromMaxAge(maxAge);
  SiteHPKPState dynamicEntry(expireTime, SecurityPropertySet,
                             foundIncludeSubdomains, sha256keys);
  SSSLOG(("SSS: about to set pins for  %s, expires=%ld now=%ld maxAge=%ld\n",
           host.get(), expireTime, PR_Now() / PR_USEC_PER_MSEC, maxAge));

  rv = SetHPKPState(host.get(), dynamicEntry, aFlags);
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: failed to set pins for %s\n", host.get()));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_COULD_NOT_SAVE_STATE;
    }
    return rv;
  }

  if (aMaxAge != nullptr) {
    *aMaxAge = (uint64_t)maxAge;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = foundIncludeSubdomains;
  }

  return foundUnrecognizedDirective
           ? NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
           : NS_OK;
}

nsresult
nsSiteSecurityService::ProcessSTSHeader(nsIURI* aSourceURI,
                                        const char* aHeader,
                                        uint32_t aFlags,
                                        uint64_t* aMaxAge,
                                        bool* aIncludeSubdomains,
                                        uint32_t* aFailureResult)
{
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  SSSLOG(("SSS: processing HSTS header '%s'", aHeader));

  const uint32_t aType = nsISiteSecurityService::HEADER_HSTS;
  bool foundMaxAge = false;
  bool foundIncludeSubdomains = false;
  bool foundUnrecognizedDirective = false;
  int64_t maxAge = 0;
  nsTArray<nsCString> unusedSHA256keys; // Required for sane internal interface

  uint32_t sssrv = ParseSSSHeaders(aType, aHeader, foundIncludeSubdomains,
                                   foundMaxAge, foundUnrecognizedDirective,
                                   maxAge, unusedSHA256keys);
  if (sssrv != nsISiteSecurityService::Success) {
    if (aFailureResult) {
      *aFailureResult = sssrv;
    }
    return NS_ERROR_FAILURE;
  }

  // after processing all the directives, make sure we came across max-age
  // somewhere.
  if (!foundMaxAge) {
    SSSLOG(("SSS: did not encounter required max-age directive"));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_NO_MAX_AGE;
    }
    return NS_ERROR_FAILURE;
  }

  // record the successfully parsed header data.
  nsresult rv = SetHSTSState(aType, aSourceURI, maxAge, foundIncludeSubdomains,
                             aFlags);
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: failed to set STS state"));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_COULD_NOT_SAVE_STATE;
    }
    return rv;
  }

  if (aMaxAge != nullptr) {
    *aMaxAge = (uint64_t)maxAge;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = foundIncludeSubdomains;
  }

  return foundUnrecognizedDirective
           ? NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
           : NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureURI(uint32_t aType, nsIURI* aURI,
                                   uint32_t aFlags, bool* aResult)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aResult);

  // Only HSTS and HPKP are supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                 aType == nsISiteSecurityService::HEADER_HPKP,
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
  NS_ENSURE_ARG(aHost);
  NS_ENSURE_ARG(aResult);

  // Only HSTS and HPKP are supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                 aType == nsISiteSecurityService::HEADER_HPKP,
                 NS_ERROR_NOT_IMPLEMENTED);

  // set default in case if we can't find any STS information
  *aResult = false;

  /* An IP address never qualifies as a secure URI. */
  if (HostIsIPAddress(aHost)) {
    return NS_OK;
  }

  if (aType == nsISiteSecurityService::HEADER_HPKP) {
    RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
    if (!certVerifier) {
      return NS_ERROR_FAILURE;
    }
    if (certVerifier->mPinningMode ==
        CertVerifier::PinningMode::pinningDisabled) {
      return NS_OK;
    }
    bool enforceTestMode = certVerifier->mPinningMode ==
                           CertVerifier::PinningMode::pinningEnforceTestMode;
    return PublicKeyPinningService::HostHasPins(aHost, mozilla::pkix::Now(),
                                                enforceTestMode, *aResult);
  }

  // Holepunch chart.apis.google.com and subdomains.
  nsAutoCString host(PublicKeyPinningService::CanonicalizeHostname(aHost));
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
  nsAutoCString storageKey;
  SetStorageKey(storageKey, host, aType);
  nsCString value = mSiteStateStorage->Get(storageKey, storageType);
  SiteHSTSState siteState(value);
  if (siteState.mHSTSState != SecurityPropertyUnset) {
    SSSLOG(("Found entry for %s", host.get()));
    bool expired = siteState.IsExpired(aType);
    if (!expired && siteState.mHSTSState == SecurityPropertySet) {
      *aResult = true;
      return NS_OK;
    }

    // If the entry is expired and not in the preload list, we can remove it.
    if (expired && !GetPreloadListEntry(host.get())) {
      mSiteStateStorage->Remove(storageKey, storageType);
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
    nsAutoCString storageKey;
    SetStorageKey(storageKey, subdomainString, aType);
    value = mSiteStateStorage->Get(storageKey, storageType);
    SiteHSTSState siteState(value);
    if (siteState.mHSTSState != SecurityPropertyUnset) {
      SSSLOG(("Found entry for %s", subdomain));
      bool expired = siteState.IsExpired(aType);
      if (!expired && siteState.mHSTSState == SecurityPropertySet) {
        *aResult = siteState.mHSTSIncludeSubdomains;
        break;
      }

      // If the entry is expired and not in the preload list, we can remove it.
      if (expired && !GetPreloadListEntry(subdomain)) {
        mSiteStateStorage->Remove(storageKey, storageType);
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

NS_IMETHODIMP
nsSiteSecurityService::ClearAll()
{
  return mSiteStateStorage->Clear();
}

NS_IMETHODIMP
nsSiteSecurityService::GetKeyPinsForHostname(const char* aHostname,
                                             mozilla::pkix::Time& aEvalTime,
                                             /*out*/ nsTArray<nsCString>& pinArray,
                                             /*out*/ bool* aIncludeSubdomains,
                                             /*out*/ bool* afound) {
  NS_ENSURE_ARG(afound);
  NS_ENSURE_ARG(aHostname);

  SSSLOG(("Top of GetKeyPinsForHostname"));
  *afound = false;
  *aIncludeSubdomains = false;
  pinArray.Clear();

  nsAutoCString host(PublicKeyPinningService::CanonicalizeHostname(aHostname));
  nsAutoCString storageKey;
  SetStorageKey(storageKey, host, nsISiteSecurityService::HEADER_HPKP);

  SSSLOG(("storagekey '%s'\n", storageKey.get()));
  mozilla::DataStorageType storageType = mozilla::DataStorage_Persistent;
  nsCString value = mSiteStateStorage->Get(storageKey, storageType);
  // decode now
  SiteHPKPState foundEntry(value);
  if (foundEntry.mState != SecurityPropertySet ||
      foundEntry.IsExpired(aEvalTime) ||
      foundEntry.mSHA256keys.Length() < 1 ) {
    // not in permanent storage, try now private
    value = mSiteStateStorage->Get(storageKey, mozilla::DataStorage_Private);
    SiteHPKPState privateEntry(value);
    if (privateEntry.mState != SecurityPropertySet ||
        privateEntry.IsExpired(aEvalTime) ||
        privateEntry.mSHA256keys.Length() < 1 ) {
      return NS_OK;
    }
    foundEntry = privateEntry;
  }
  pinArray = foundEntry.mSHA256keys;
  *aIncludeSubdomains = foundEntry.mIncludeSubdomains;
  *afound = true;
  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::SetKeyPins(const char* aHost, bool aIncludeSubdomains,
                                  uint32_t aMaxAge, uint32_t aPinCount,
                                  const char** aSha256Pins,
                                  /*out*/ bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aHost);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aSha256Pins);

  SSSLOG(("Top of SetPins"));

  int64_t expireTime = ExpireTimeFromMaxAge(aMaxAge);
  nsTArray<nsCString> sha256keys;
  for (unsigned int i = 0; i < aPinCount; i++) {
    nsAutoCString pin(aSha256Pins[i]);
    SSSLOG(("SetPins pin=%s\n", pin.get()));
    if (!stringIsBase64EncodingOf256bitValue(pin)) {
      return NS_ERROR_INVALID_ARG;
    }
    sha256keys.AppendElement(pin);
  }
  SiteHPKPState dynamicEntry(expireTime, SecurityPropertySet,
                             aIncludeSubdomains, sha256keys);
  // we always store data in permanent storage (ie no flags)
  nsAutoCString host(PublicKeyPinningService::CanonicalizeHostname(aHost));
  return SetHPKPState(host.get(), dynamicEntry, 0);
}

nsresult
nsSiteSecurityService::SetHPKPState(const char* aHost, SiteHPKPState& entry,
                                    uint32_t aFlags)
{
  SSSLOG(("Top of SetPKPState"));
  nsAutoCString host(aHost);
  nsAutoCString storageKey;
  SetStorageKey(storageKey, host, nsISiteSecurityService::HEADER_HPKP);
  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                         ? mozilla::DataStorage_Private
                                         : mozilla::DataStorage_Persistent;
  nsAutoCString stateString;
  entry.ToString(stateString);
  nsresult rv = mSiteStateStorage->Put(storageKey, stateString, storageType);
  NS_ENSURE_SUCCESS(rv, rv);
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
    mProcessPKPHeadersFromNonBuiltInRoots = mozilla::Preferences::GetBool(
      "security.cert_pinning.process_headers_from_non_builtin_roots", false);
  }

  return NS_OK;
}
