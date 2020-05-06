/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSiteSecurityService.h"

#include "PublicKeyPinningService.h"
#include "ScopedNSSTypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Base64.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsIScriptSecurityManager.h"
#include "nsISocketProvider.h"
#include "nsITransportSecurityInfo.h"
#include "nsIURI.h"
#include "nsIX509Cert.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSComponent.h"
#include "nsNetUtil.h"
#include "nsPromiseFlatString.h"
#include "nsReadableUtils.h"
#include "nsSecurityHeaderParser.h"
#include "nsThreadUtils.h"
#include "nsVariant.h"
#include "nsXULAppAPI.h"
#include "prnetdb.h"

// A note about the preload list:
// When a site specifically disables HSTS by sending a header with
// 'max-age: 0', we keep a "knockout" value that means "we have no information
// regarding the HSTS state of this host" (any ancestor of "this host" can still
// influence its HSTS status via include subdomains, however).
// This prevents the preload list from overriding the site's current
// desired HSTS status.
#include "nsSTSPreloadList.h"

using namespace mozilla;
using namespace mozilla::psm;

static LazyLogModule gSSSLog("nsSSService");

#define SSSLOG(args) MOZ_LOG(gSSSLog, mozilla::LogLevel::Debug, args)

const char kHSTSKeySuffix[] = ":HSTS";

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(SiteHSTSState, nsISiteSecurityState, nsISiteHSTSState)

namespace {

class SSSTokenizer final : public Tokenizer {
 public:
  explicit SSSTokenizer(const nsACString& source) : Tokenizer(source) {}

  MOZ_MUST_USE bool ReadBool(/*out*/ bool& value) {
    uint8_t rawValue;
    if (!ReadInteger(&rawValue)) {
      return false;
    }

    if (rawValue != 0 && rawValue != 1) {
      return false;
    }

    value = (rawValue == 1);
    return true;
  }

  MOZ_MUST_USE bool ReadState(/*out*/ SecurityPropertyState& state) {
    uint32_t rawValue;
    if (!ReadInteger(&rawValue)) {
      return false;
    }

    state = static_cast<SecurityPropertyState>(rawValue);
    switch (state) {
      case SecurityPropertyKnockout:
      case SecurityPropertyNegative:
      case SecurityPropertySet:
      case SecurityPropertyUnset:
        break;
      default:
        return false;
    }

    return true;
  }

  MOZ_MUST_USE bool ReadSource(/*out*/ SecurityPropertySource& source) {
    uint32_t rawValue;
    if (!ReadInteger(&rawValue)) {
      return false;
    }

    source = static_cast<SecurityPropertySource>(rawValue);
    switch (source) {
      case SourceUnknown:
      case SourcePreload:
      case SourceOrganic:
        break;
      default:
        return false;
    }

    return true;
  }
};

// Parses a state string like "1500918564034,1,1" into its constituent parts.
bool ParseHSTSState(const nsCString& stateString,
                    /*out*/ PRTime& expireTime,
                    /*out*/ SecurityPropertyState& state,
                    /*out*/ bool& includeSubdomains,
                    /*out*/ SecurityPropertySource& source) {
  SSSTokenizer tokenizer(stateString);
  SSSLOG(("Parsing state from %s", stateString.get()));

  if (!tokenizer.ReadInteger(&expireTime)) {
    return false;
  }

  if (!tokenizer.CheckChar(',')) {
    return false;
  }

  if (!tokenizer.ReadState(state)) {
    return false;
  }

  if (!tokenizer.CheckChar(',')) {
    return false;
  }

  if (!tokenizer.ReadBool(includeSubdomains)) {
    return false;
  }

  source = SourceUnknown;
  if (tokenizer.CheckChar(',')) {
    if (!tokenizer.ReadSource(source)) {
      return false;
    }
  }

  return tokenizer.CheckEOF();
}

}  // namespace

SiteHSTSState::SiteHSTSState(const nsCString& aHost,
                             const OriginAttributes& aOriginAttributes,
                             const nsCString& aStateString)
    : mHostname(aHost),
      mOriginAttributes(aOriginAttributes),
      mHSTSExpireTime(0),
      mHSTSState(SecurityPropertyUnset),
      mHSTSIncludeSubdomains(false),
      mHSTSSource(SourceUnknown) {
  bool valid = ParseHSTSState(aStateString, mHSTSExpireTime, mHSTSState,
                              mHSTSIncludeSubdomains, mHSTSSource);
  if (!valid) {
    SSSLOG(("%s is not a valid SiteHSTSState", aStateString.get()));
    mHSTSExpireTime = 0;
    mHSTSState = SecurityPropertyUnset;
    mHSTSIncludeSubdomains = false;
    mHSTSSource = SourceUnknown;
  }
}

SiteHSTSState::SiteHSTSState(const nsCString& aHost,
                             const OriginAttributes& aOriginAttributes,
                             PRTime aHSTSExpireTime,
                             SecurityPropertyState aHSTSState,
                             bool aHSTSIncludeSubdomains,
                             SecurityPropertySource aSource)

    : mHostname(aHost),
      mOriginAttributes(aOriginAttributes),
      mHSTSExpireTime(aHSTSExpireTime),
      mHSTSState(aHSTSState),
      mHSTSIncludeSubdomains(aHSTSIncludeSubdomains),
      mHSTSSource(aSource) {}

void SiteHSTSState::ToString(nsCString& aString) {
  aString.Truncate();
  aString.AppendInt(mHSTSExpireTime);
  aString.Append(',');
  aString.AppendInt(mHSTSState);
  aString.Append(',');
  aString.AppendInt(static_cast<uint32_t>(mHSTSIncludeSubdomains));
  aString.Append(',');
  aString.AppendInt(mHSTSSource);
}

NS_IMETHODIMP
SiteHSTSState::GetHostname(nsACString& aHostname) {
  aHostname = mHostname;
  return NS_OK;
}

NS_IMETHODIMP
SiteHSTSState::GetExpireTime(int64_t* aExpireTime) {
  NS_ENSURE_ARG(aExpireTime);
  *aExpireTime = mHSTSExpireTime;
  return NS_OK;
}

NS_IMETHODIMP
SiteHSTSState::GetSecurityPropertyState(int16_t* aSecurityPropertyState) {
  NS_ENSURE_ARG(aSecurityPropertyState);
  *aSecurityPropertyState = mHSTSState;
  return NS_OK;
}

NS_IMETHODIMP
SiteHSTSState::GetIncludeSubdomains(bool* aIncludeSubdomains) {
  NS_ENSURE_ARG(aIncludeSubdomains);
  *aIncludeSubdomains = mHSTSIncludeSubdomains;
  return NS_OK;
}

NS_IMETHODIMP
SiteHSTSState::GetOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aOriginAttributes) {
  if (!ToJSValue(aCx, mOriginAttributes, aOriginAttributes)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

const uint64_t kSixtyDaysInSeconds = 60 * 24 * 60 * 60;

nsSiteSecurityService::nsSiteSecurityService()
    : mUsePreloadList(true), mPreloadListTimeOffset(0), mDafsa(kDafsa) {}

nsSiteSecurityService::~nsSiteSecurityService() = default;

NS_IMPL_ISUPPORTS(nsSiteSecurityService, nsIObserver, nsISiteSecurityService)

nsresult nsSiteSecurityService::Init() {
  // Don't access Preferences off the main thread.
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE("nsSiteSecurityService initialized off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  mUsePreloadList = mozilla::Preferences::GetBool(
      "network.stricttransportsecurity.preloadlist", true);
  mozilla::Preferences::AddStrongObserver(
      this, "network.stricttransportsecurity.preloadlist");
  mPreloadListTimeOffset =
      mozilla::Preferences::GetInt("test.currentTimeOffsetSeconds", 0);
  mozilla::Preferences::AddStrongObserver(this,
                                          "test.currentTimeOffsetSeconds");
  mSiteStateStorage =
      mozilla::DataStorage::Get(DataStorageClass::SiteSecurityServiceState);
  mPreloadStateStorage =
      mozilla::DataStorage::Get(DataStorageClass::SecurityPreloadState);
  nsresult rv = mSiteStateStorage->Init(nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = mPreloadStateStorage->Init(nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult nsSiteSecurityService::GetHost(nsIURI* aURI, nsACString& aResult) {
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

static void SetStorageKey(const nsACString& hostname, uint32_t aType,
                          const OriginAttributes& aOriginAttributes,
                          /*out*/ nsAutoCString& storageKey) {
  storageKey = hostname;

  // Don't isolate by userContextId.
  OriginAttributes originAttributesNoUserContext = aOriginAttributes;
  originAttributesNoUserContext.mUserContextId =
      nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID;
  nsAutoCString originAttributesSuffix;
  originAttributesNoUserContext.CreateSuffix(originAttributesSuffix);
  storageKey.Append(originAttributesSuffix);
  switch (aType) {
    case nsISiteSecurityService::HEADER_HSTS:
      storageKey.AppendASCII(kHSTSKeySuffix);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("SSS:SetStorageKey got invalid type");
  }
}

// Expire times are in millis.  Since Headers max-age is in seconds, and
// PR_Now() is in micros, normalize the units at milliseconds.
static int64_t ExpireTimeFromMaxAge(uint64_t maxAge) {
  return (PR_Now() / PR_USEC_PER_MSEC) + ((int64_t)maxAge * PR_MSEC_PER_SEC);
}

nsresult nsSiteSecurityService::SetHSTSState(
    uint32_t aType, const char* aHost, int64_t maxage, bool includeSubdomains,
    uint32_t flags, SecurityPropertyState aHSTSState,
    SecurityPropertySource aSource, const OriginAttributes& aOriginAttributes) {
  nsAutoCString hostname(aHost);
  bool isPreload = (aSource == SourcePreload);
  // If max-age is zero, the host is no longer considered HSTS. If the host was
  // preloaded, we store an entry indicating that this host is not HSTS, causing
  // the preloaded information to be ignored.
  if (maxage == 0) {
    return MarkHostAsNotHSTS(aType, hostname, flags, isPreload,
                             aOriginAttributes);
  }

  MOZ_ASSERT(
      (aHSTSState == SecurityPropertySet ||
       aHSTSState == SecurityPropertyNegative),
      "HSTS State must be SecurityPropertySet or SecurityPropertyNegative");
  if (isPreload && aOriginAttributes != OriginAttributes()) {
    return NS_ERROR_INVALID_ARG;
  }

  int64_t expiretime = ExpireTimeFromMaxAge(maxage);
  RefPtr<SiteHSTSState> siteState =
      new SiteHSTSState(hostname, aOriginAttributes, expiretime, aHSTSState,
                        includeSubdomains, aSource);
  nsAutoCString stateString;
  siteState->ToString(stateString);
  SSSLOG(("SSS: setting state for %s", hostname.get()));
  bool isPrivate = flags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                             ? mozilla::DataStorage_Private
                                             : mozilla::DataStorage_Persistent;
  nsAutoCString storageKey;
  SetStorageKey(hostname, aType, aOriginAttributes, storageKey);
  nsresult rv;
  if (isPreload) {
    SSSLOG(("SSS: storing entry for %s in dynamic preloads", hostname.get()));
    rv = mPreloadStateStorage->Put(storageKey, stateString,
                                   mozilla::DataStorage_Persistent);
  } else {
    SSSLOG(("SSS: storing HSTS site entry for %s", hostname.get()));
    nsCString value = mSiteStateStorage->Get(storageKey, storageType);
    RefPtr<SiteHSTSState> curSiteState =
        new SiteHSTSState(hostname, aOriginAttributes, value);
    if (curSiteState->mHSTSState != SecurityPropertyUnset &&
        curSiteState->mHSTSSource != SourceUnknown) {
      // don't override the source
      siteState->mHSTSSource = curSiteState->mHSTSSource;
      siteState->ToString(stateString);
    }
    rv = mSiteStateStorage->Put(storageKey, stateString, storageType);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Helper function to mark a host as not HSTS. In the general case, we can just
// remove the HSTS state. However, for preloaded entries, we have to store an
// entry that indicates this host is not HSTS to prevent the implementation
// using the preloaded information.
nsresult nsSiteSecurityService::MarkHostAsNotHSTS(
    uint32_t aType, const nsAutoCString& aHost, uint32_t aFlags,
    bool aIsPreload, const OriginAttributes& aOriginAttributes) {
  // This only applies to HSTS.
  if (aType != nsISiteSecurityService::HEADER_HSTS) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aIsPreload && aOriginAttributes != OriginAttributes()) {
    return NS_ERROR_INVALID_ARG;
  }

  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                             ? mozilla::DataStorage_Private
                                             : mozilla::DataStorage_Persistent;
  nsAutoCString storageKey;
  SetStorageKey(aHost, aType, aOriginAttributes, storageKey);

  nsCString value =
      mPreloadStateStorage->Get(storageKey, mozilla::DataStorage_Persistent);
  RefPtr<SiteHSTSState> dynamicState =
      new SiteHSTSState(aHost, aOriginAttributes, value);
  if (GetPreloadStatus(aHost) ||
      dynamicState->mHSTSState != SecurityPropertyUnset) {
    SSSLOG(("SSS: storing knockout entry for %s", aHost.get()));
    RefPtr<SiteHSTSState> siteState =
        new SiteHSTSState(aHost, aOriginAttributes, 0, SecurityPropertyKnockout,
                          false, SourceUnknown);
    nsAutoCString stateString;
    siteState->ToString(stateString);
    nsresult rv;
    if (aIsPreload) {
      rv = mPreloadStateStorage->Put(storageKey, stateString,
                                     mozilla::DataStorage_Persistent);
    } else {
      rv = mSiteStateStorage->Put(storageKey, stateString, storageType);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    SSSLOG(("SSS: removing entry for %s", aHost.get()));
    if (aIsPreload) {
      mPreloadStateStorage->Remove(storageKey, mozilla::DataStorage_Persistent);
    } else {
      mSiteStateStorage->Remove(storageKey, storageType);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::ResetState(uint32_t aType, nsIURI* aURI, uint32_t aFlags,
                                  JS::HandleValue aOriginAttributes,
                                  JSContext* aCx, uint8_t aArgc) {
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH(
        "Child process: no direct access to "
        "nsISiteSecurityService::ResetState");
  }
  if (!aURI) {
    return NS_ERROR_INVALID_ARG;
  }

  OriginAttributes originAttributes;
  if (aArgc > 0) {
    // OriginAttributes were passed in.
    if (!aOriginAttributes.isObject() ||
        !originAttributes.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return ResetStateInternal(aType, aURI, aFlags, originAttributes);
}

// Helper function to reset stored state of the given type for the host
// identified by the given URI. If there is preloaded information for the host,
// that information will be used for future queries. C.f. MarkHostAsNotHSTS,
// which will store a knockout entry for preloaded HSTS hosts that have sent a
// header with max-age=0 (meaning preloaded information will then not be used
// for that host).
nsresult nsSiteSecurityService::ResetStateInternal(
    uint32_t aType, nsIURI* aURI, uint32_t aFlags,
    const OriginAttributes& aOriginAttributes) {
  if (!aURI) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aType != nsISiteSecurityService::HEADER_HSTS) {
    return NS_ERROR_INVALID_ARG;
  }
  nsAutoCString hostname;
  nsresult rv = GetHost(aURI, hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString storageKey;
  SetStorageKey(hostname, aType, aOriginAttributes, storageKey);
  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                             ? mozilla::DataStorage_Private
                                             : mozilla::DataStorage_Persistent;
  mSiteStateStorage->Remove(storageKey, storageType);
  return NS_OK;
}

static bool HostIsIPAddress(const nsCString& hostname) {
  PRNetAddr hostAddr;
  PRErrorCode prv = PR_StringToNetAddr(hostname.get(), &hostAddr);
  return (prv == PR_SUCCESS);
}

NS_IMETHODIMP
nsSiteSecurityService::ProcessHeaderScriptable(
    uint32_t aType, nsIURI* aSourceURI, const nsACString& aHeader,
    nsITransportSecurityInfo* aSecInfo, uint32_t aFlags, uint32_t aSource,
    JS::HandleValue aOriginAttributes, uint64_t* aMaxAge,
    bool* aIncludeSubdomains, uint32_t* aFailureResult, JSContext* aCx,
    uint8_t aArgc) {
  OriginAttributes originAttributes;
  if (aArgc > 0) {
    if (!aOriginAttributes.isObject() ||
        !originAttributes.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }
  return ProcessHeader(aType, aSourceURI, aHeader, aSecInfo, aFlags, aSource,
                       originAttributes, aMaxAge, aIncludeSubdomains,
                       aFailureResult);
}

NS_IMETHODIMP
nsSiteSecurityService::ProcessHeader(
    uint32_t aType, nsIURI* aSourceURI, const nsACString& aHeader,
    nsITransportSecurityInfo* aSecInfo, uint32_t aFlags, uint32_t aHeaderSource,
    const OriginAttributes& aOriginAttributes, uint64_t* aMaxAge,
    bool* aIncludeSubdomains, uint32_t* aFailureResult) {
  // Child processes are not allowed direct access to this.
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH(
        "Child process: no direct access to "
        "nsISiteSecurityService::ProcessHeader");
  }

  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);
  SecurityPropertySource source =
      static_cast<SecurityPropertySource>(aHeaderSource);
  switch (source) {
    case SourceUnknown:
    case SourcePreload:
    case SourceOrganic:
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_ARG(aSecInfo);
  return ProcessHeaderInternal(aType, aSourceURI, PromiseFlatCString(aHeader),
                               aSecInfo, aFlags, source, aOriginAttributes,
                               aMaxAge, aIncludeSubdomains, aFailureResult);
}

nsresult nsSiteSecurityService::ProcessHeaderInternal(
    uint32_t aType, nsIURI* aSourceURI, const nsCString& aHeader,
    nsITransportSecurityInfo* aSecInfo, uint32_t aFlags,
    SecurityPropertySource aSource, const OriginAttributes& aOriginAttributes,
    uint64_t* aMaxAge, bool* aIncludeSubdomains, uint32_t* aFailureResult) {
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  if (aMaxAge != nullptr) {
    *aMaxAge = 0;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = false;
  }

  if (aSecInfo) {
    bool tlsIsBroken = false;
    bool trustcheck;
    nsresult rv;
    rv = aSecInfo->GetIsDomainMismatch(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;

    rv = aSecInfo->GetIsNotValidAtThisTime(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;

    rv = aSecInfo->GetIsUntrusted(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;
    if (tlsIsBroken) {
      SSSLOG(("SSS: discarding header from untrustworthy connection"));
      if (aFailureResult) {
        *aFailureResult =
            nsISiteSecurityService::ERROR_UNTRUSTWORTHY_CONNECTION;
      }
      return NS_ERROR_FAILURE;
    }
  }

  nsAutoCString host;
  nsresult rv = GetHost(aSourceURI, host);
  NS_ENSURE_SUCCESS(rv, rv);
  if (HostIsIPAddress(host)) {
    /* Don't process headers if a site is accessed by IP address. */
    return NS_OK;
  }

  switch (aType) {
    case nsISiteSecurityService::HEADER_HSTS:
      rv = ProcessSTSHeader(aSourceURI, aHeader, aFlags, aSource,
                            aOriginAttributes, aMaxAge, aIncludeSubdomains,
                            aFailureResult);
      break;
    default:
      MOZ_CRASH("unexpected header type");
  }
  return rv;
}

static uint32_t ParseSSSHeaders(uint32_t aType, const nsCString& aHeader,
                                bool& foundIncludeSubdomains, bool& foundMaxAge,
                                bool& foundUnrecognizedDirective,
                                uint64_t& maxAge) {
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

  NS_NAMED_LITERAL_CSTRING(max_age_var, "max-age");
  NS_NAMED_LITERAL_CSTRING(include_subd_var, "includesubdomains");

  nsSecurityHeaderParser parser(aHeader);
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: could not parse header"));
    return nsISiteSecurityService::ERROR_COULD_NOT_PARSE_HEADER;
  }
  mozilla::LinkedList<nsSecurityHeaderDirective>* directives =
      parser.GetDirectives();

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

      Tokenizer tokenizer(directive->mValue);
      if (!tokenizer.ReadInteger(&maxAge)) {
        SSSLOG(("SSS: could not parse delta-seconds"));
        return nsISiteSecurityService::ERROR_INVALID_MAX_AGE;
      }

      if (!tokenizer.CheckEOF()) {
        SSSLOG(("SSS: invalid value for max-age directive"));
        return nsISiteSecurityService::ERROR_INVALID_MAX_AGE;
      }

      SSSLOG(("SSS: parsed delta-seconds: %" PRIu64, maxAge));
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
    } else {
      SSSLOG(("SSS: ignoring unrecognized directive '%s'",
              directive->mName.get()));
      foundUnrecognizedDirective = true;
    }
  }
  return nsISiteSecurityService::Success;
}

nsresult nsSiteSecurityService::ProcessSTSHeader(
    nsIURI* aSourceURI, const nsCString& aHeader, uint32_t aFlags,
    SecurityPropertySource aSource, const OriginAttributes& aOriginAttributes,
    uint64_t* aMaxAge, bool* aIncludeSubdomains, uint32_t* aFailureResult) {
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  SSSLOG(("SSS: processing HSTS header '%s'", aHeader.get()));

  const uint32_t aType = nsISiteSecurityService::HEADER_HSTS;
  bool foundMaxAge = false;
  bool foundIncludeSubdomains = false;
  bool foundUnrecognizedDirective = false;
  uint64_t maxAge = 0;
  nsTArray<nsCString> unusedSHA256keys;  // Required for sane internal interface

  uint32_t sssrv =
      ParseSSSHeaders(aType, aHeader, foundIncludeSubdomains, foundMaxAge,
                      foundUnrecognizedDirective, maxAge);
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

  nsAutoCString hostname;
  nsresult rv = GetHost(aSourceURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);

  // record the successfully parsed header data.
  rv = SetHSTSState(aType, hostname.get(), maxAge, foundIncludeSubdomains,
                    aFlags, SecurityPropertySet, aSource, aOriginAttributes);
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: failed to set STS state"));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_COULD_NOT_SAVE_STATE;
    }
    return rv;
  }

  if (aMaxAge != nullptr) {
    *aMaxAge = maxAge;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = foundIncludeSubdomains;
  }

  return foundUnrecognizedDirective ? NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
                                    : NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureURIScriptable(uint32_t aType, nsIURI* aURI,
                                             uint32_t aFlags,
                                             JS::HandleValue aOriginAttributes,
                                             bool* aCached, uint32_t* aSource,
                                             JSContext* aCx, uint8_t aArgc,
                                             bool* aResult) {
  OriginAttributes originAttributes;
  if (aArgc > 0) {
    if (!aOriginAttributes.isObject() ||
        !originAttributes.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }
  return IsSecureURI(aType, aURI, aFlags, originAttributes, aCached, aSource,
                     aResult);
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureURI(uint32_t aType, nsIURI* aURI,
                                   uint32_t aFlags,
                                   const OriginAttributes& aOriginAttributes,
                                   bool* aCached, uint32_t* aSource,
                                   bool* aResult) {
  // Child processes are not allowed direct access to this.
  if (!XRE_IsParentProcess() && aType != nsISiteSecurityService::HEADER_HSTS) {
    MOZ_CRASH(
        "Child process: no direct access to "
        "nsISiteSecurityService::IsSecureURI for non-HSTS entries");
  }

  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aResult);

  // Only HSTS and static pinning are supported.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                     aType == nsISiteSecurityService::STATIC_PINNING,
                 NS_ERROR_NOT_IMPLEMENTED);

  nsAutoCString hostname;
  nsresult rv = GetHost(aURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);
  /* An IP address never qualifies as a secure URI. */
  if (HostIsIPAddress(hostname)) {
    *aResult = false;
    return NS_OK;
  }

  SecurityPropertySource* source =
      BitwiseCast<SecurityPropertySource*>(aSource);

  return IsSecureHost(aType, hostname, aFlags, aOriginAttributes, aCached,
                      source, aResult);
}

// Checks if the given host is in the preload list.
//
// @param aHost The host to match. Only does exact host matching.
// @param aIncludeSubdomains Out, optional. Indicates whether or not to include
//        subdomains. Only set if the host is matched and this function returns
//        true.
//
// @return True if the host is matched, false otherwise.
bool nsSiteSecurityService::GetPreloadStatus(const nsACString& aHost,
                                             bool* aIncludeSubdomains) const {
  const int kIncludeSubdomains = 1;
  bool found = false;

  PRTime currentTime = PR_Now() + (mPreloadListTimeOffset * PR_USEC_PER_SEC);
  if (mUsePreloadList && currentTime < gPreloadListExpirationTime) {
    int result = mDafsa.Lookup(aHost);
    found = (result != mozilla::Dafsa::kKeyNotFound);
    if (found && aIncludeSubdomains) {
      *aIncludeSubdomains = (result == kIncludeSubdomains);
    }
  }

  return found;
}

// Allows us to determine if we have an HSTS entry for a given host (and, if
// so, what that state is). The return value says whether or not we know
// anything about this host (true if the host has an HSTS entry). aHost is
// the host which we wish to deteming HSTS information on,
// aRequireIncludeSubdomains specifies whether we require includeSubdomains
// to be set on the entry (with the other parameters being as per IsSecureHost).
bool nsSiteSecurityService::HostHasHSTSEntry(
    const nsAutoCString& aHost, bool aRequireIncludeSubdomains, uint32_t aFlags,
    const OriginAttributes& aOriginAttributes, bool* aResult, bool* aCached,
    SecurityPropertySource* aSource) {
  if (aSource) {
    *aSource = SourceUnknown;
  }
  if (aCached) {
    *aCached = false;
  }
  // First we check for an entry in site security storage. If that entry exists,
  // we don't want to check in the preload lists. We only want to use the
  // stored value if it is not a knockout entry, however.
  // Additionally, if it is a knockout entry, we want to stop looking for data
  // on the host, because the knockout entry indicates "we have no information
  // regarding the security status of this host".
  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                             ? mozilla::DataStorage_Private
                                             : mozilla::DataStorage_Persistent;
  nsAutoCString storageKey;
  SSSLOG(("Seeking HSTS entry for %s", aHost.get()));
  SetStorageKey(aHost, nsISiteSecurityService::HEADER_HSTS, aOriginAttributes,
                storageKey);
  nsAutoCString preloadKey;
  SetStorageKey(aHost, nsISiteSecurityService::HEADER_HSTS, OriginAttributes(),
                preloadKey);
  nsCString value = mSiteStateStorage->Get(storageKey, storageType);
  RefPtr<SiteHSTSState> siteState =
      new SiteHSTSState(aHost, aOriginAttributes, value);
  if (siteState->mHSTSState != SecurityPropertyUnset) {
    SSSLOG(("Found HSTS entry for %s", aHost.get()));
    bool expired = siteState->IsExpired(nsISiteSecurityService::HEADER_HSTS);
    if (!expired) {
      SSSLOG(("Entry for %s is not expired", aHost.get()));
      if (siteState->mHSTSState == SecurityPropertySet) {
        *aResult = aRequireIncludeSubdomains ? siteState->mHSTSIncludeSubdomains
                                             : true;
        if (aCached) {
          // Only set cached if this includes subdomains
          *aCached = aRequireIncludeSubdomains
                         ? siteState->mHSTSIncludeSubdomains
                         : true;
        }
        if (aSource) {
          *aSource = siteState->mHSTSSource;
        }
        return true;
      } else if (siteState->mHSTSState == SecurityPropertyNegative) {
        *aResult = false;
        if (aCached) {
          // if it's negative, it is always cached
          SSSLOG(("Marking HSTS as as cached (SecurityPropertyNegative)"));
          *aCached = true;
        }
        if (aSource) {
          *aSource = siteState->mHSTSSource;
        }
        return true;
      }
    }

    if (expired) {
      SSSLOG(("Entry %s is expired - checking for preload state", aHost.get()));
      // If the entry is expired and is not in either the static or dynamic
      // preload lists, we can remove it.
      // First, check the dynamic preload list.
      value = mPreloadStateStorage->Get(preloadKey,
                                        mozilla::DataStorage_Persistent);
      RefPtr<SiteHSTSState> dynamicState =
          new SiteHSTSState(aHost, aOriginAttributes, value);
      if (dynamicState->mHSTSState == SecurityPropertyUnset) {
        SSSLOG(("No dynamic preload - checking for static preload"));
        // Now check the static preload list.
        if (!GetPreloadStatus(aHost)) {
          SSSLOG(("No static preload - removing expired entry"));
          mSiteStateStorage->Remove(storageKey, storageType);
        }
      }
    }
    return false;
  }

  // Next, look in the dynamic preload list.
  value =
      mPreloadStateStorage->Get(preloadKey, mozilla::DataStorage_Persistent);
  RefPtr<SiteHSTSState> dynamicState =
      new SiteHSTSState(aHost, aOriginAttributes, value);
  if (dynamicState->mHSTSState != SecurityPropertyUnset) {
    SSSLOG(("Found dynamic preload entry for %s", aHost.get()));
    bool expired = dynamicState->IsExpired(nsISiteSecurityService::HEADER_HSTS);
    if (!expired) {
      if (dynamicState->mHSTSState == SecurityPropertySet) {
        *aResult = aRequireIncludeSubdomains
                       ? dynamicState->mHSTSIncludeSubdomains
                       : true;
        if (aCached) {
          // Only set cached if this includes subdomains
          *aCached = aRequireIncludeSubdomains
                         ? dynamicState->mHSTSIncludeSubdomains
                         : true;
        }
        if (aSource) {
          *aSource = dynamicState->mHSTSSource;
        }
        return true;
      } else if (dynamicState->mHSTSState == SecurityPropertyNegative) {
        *aResult = false;
        if (aCached) {
          // if it's negative, it is always cached
          *aCached = true;
        }
        if (aSource) {
          *aSource = dynamicState->mHSTSSource;
        }
        return true;
      }
    } else {
      // if a dynamic preload has expired and is not in the static preload
      // list, we can remove it.
      if (!GetPreloadStatus(aHost)) {
        mPreloadStateStorage->Remove(preloadKey,
                                     mozilla::DataStorage_Persistent);
      }
    }
    return false;
  }

  bool includeSubdomains = false;

  // Finally look in the static preload list.
  if (siteState->mHSTSState == SecurityPropertyUnset &&
      dynamicState->mHSTSState == SecurityPropertyUnset &&
      GetPreloadStatus(aHost, &includeSubdomains)) {
    SSSLOG(("%s is a preloaded HSTS host", aHost.get()));
    *aResult = aRequireIncludeSubdomains ? includeSubdomains : true;
    if (aCached) {
      // Only set cached if this includes subdomains
      *aCached = aRequireIncludeSubdomains ? includeSubdomains : true;
    }
    if (aSource) {
      *aSource = SourcePreload;
    }
    return true;
  }

  return false;
}

nsresult nsSiteSecurityService::IsSecureHost(
    uint32_t aType, const nsACString& aHost, uint32_t aFlags,
    const OriginAttributes& aOriginAttributes, bool* aCached,
    SecurityPropertySource* aSource, bool* aResult) {
  // Child processes are not allowed direct access to this.
  if (!XRE_IsParentProcess() && aType != nsISiteSecurityService::HEADER_HSTS) {
    MOZ_CRASH(
        "Child process: no direct access to "
        "nsISiteSecurityService::IsSecureHost for non-HSTS entries");
  }

  NS_ENSURE_ARG(aResult);

  // Only HSTS and static pinning are supported.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                     aType == nsISiteSecurityService::STATIC_PINNING,
                 NS_ERROR_NOT_IMPLEMENTED);

  // set default in case if we can't find any STS information
  *aResult = false;

  /* An IP address never qualifies as a secure URI. */
  const nsCString& flatHost = PromiseFlatCString(aHost);
  if (HostIsIPAddress(flatHost)) {
    return NS_OK;
  }

  if (aType == nsISiteSecurityService::STATIC_PINNING) {
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
    return PublicKeyPinningService::HostHasPins(
        flatHost.get(), mozilla::pkix::Now(), enforceTestMode,
        aOriginAttributes, *aResult);
  }

  nsAutoCString host(
      PublicKeyPinningService::CanonicalizeHostname(flatHost.get()));

  // First check the exact host.
  if (HostHasHSTSEntry(host, false, aFlags, aOriginAttributes, aResult, aCached,
                       aSource)) {
    return NS_OK;
  }

  SSSLOG(("no HSTS data for %s found, walking up domain", host.get()));
  const char* subdomain;

  uint32_t offset = 0;
  for (offset = host.FindChar('.', offset) + 1; offset > 0;
       offset = host.FindChar('.', offset) + 1) {
    subdomain = host.get() + offset;

    // If we get an empty string, don't continue.
    if (strlen(subdomain) < 1) {
      break;
    }

    // Do the same thing as with the exact host except now we're looking at
    // ancestor domains of the original host and, therefore, we have to require
    // that the entry includes subdomains.
    nsAutoCString subdomainString(subdomain);

    if (HostHasHSTSEntry(subdomainString, true, aFlags, aOriginAttributes,
                         aResult, aCached, aSource)) {
      break;
    }

    SSSLOG(("no HSTS data for %s found, walking up domain", subdomain));
  }

  // Use whatever we ended up with, which defaults to false.
  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::ClearAll() {
  // Child processes are not allowed direct access to this.
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH(
        "Child process: no direct access to nsISiteSecurityService::ClearAll");
  }

  return mSiteStateStorage->Clear();
}

NS_IMETHODIMP
nsSiteSecurityService::ClearPreloads() {
  // Child processes are not allowed direct access to this.
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH(
        "Child process: no direct access to "
        "nsISiteSecurityService::ClearPreloads");
  }

  return mPreloadStateStorage->Clear();
}

NS_IMETHODIMP
nsSiteSecurityService::SetHSTSPreload(const nsACString& aHost,
                                      bool aIncludeSubdomains, int64_t aExpires,
                                      /*out*/ bool* aResult) {
  // Child processes are not allowed direct access to this.
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH(
        "Child process: no direct access to "
        "nsISiteSecurityService::SetHSTSPreload");
  }

  NS_ENSURE_ARG_POINTER(aResult);

  SSSLOG(("Top of SetHSTSPreload"));

  const nsCString& flatHost = PromiseFlatCString(aHost);
  nsAutoCString host(
      PublicKeyPinningService::CanonicalizeHostname(flatHost.get()));
  return SetHSTSState(nsISiteSecurityService::HEADER_HSTS, host.get(), aExpires,
                      aIncludeSubdomains, 0, SecurityPropertySet, SourcePreload,
                      OriginAttributes());
}

NS_IMETHODIMP
nsSiteSecurityService::Enumerate(uint32_t aType,
                                 nsISimpleEnumerator** aEnumerator) {
  NS_ENSURE_ARG(aEnumerator);

  nsAutoCString keySuffix;
  switch (aType) {
    case nsISiteSecurityService::HEADER_HSTS:
      keySuffix.AssignASCII(kHSTSKeySuffix);
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  nsTArray<mozilla::psm::DataStorageItem> items;
  mSiteStateStorage->GetAll(&items);

  nsCOMArray<nsISiteSecurityState> states;
  for (const mozilla::psm::DataStorageItem& item : items) {
    if (!StringEndsWith(item.key(), keySuffix)) {
      // The key does not end with correct suffix, so is not the type we want.
      continue;
    }

    nsCString origin(
        StringHead(item.key(), item.key().Length() - keySuffix.Length()));
    nsAutoCString hostname;
    OriginAttributes originAttributes;
    if (!originAttributes.PopulateFromOrigin(origin, hostname)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsISiteSecurityState> state;
    switch (aType) {
      case nsISiteSecurityService::HEADER_HSTS:
        state = new SiteHSTSState(hostname, originAttributes, item.value());
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("SSS:Enumerate got invalid type");
    }

    states.AppendObject(state);
  }

  NS_NewArrayEnumerator(aEnumerator, states, NS_GET_IID(nsISiteSecurityState));
  return NS_OK;
}

//------------------------------------------------------------
// nsSiteSecurityService::nsIObserver
//------------------------------------------------------------

NS_IMETHODIMP
nsSiteSecurityService::Observe(nsISupports* /*subject*/, const char* topic,
                               const char16_t* /*data*/) {
  // Don't access Preferences off the main thread.
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE("Preferences accessed off main thread");
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
