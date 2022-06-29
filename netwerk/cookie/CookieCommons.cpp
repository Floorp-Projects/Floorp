/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Cookie.h"
#include "CookieCommons.h"
#include "CookieLogging.h"
#include "CookieService.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsICookiePermission.h"
#include "nsICookieService.h"
#include "nsIEffectiveTLDService.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"
#include "ThirdPartyUtil.h"

namespace mozilla {

using dom::Document;

namespace net {

// static
bool CookieCommons::DomainMatches(Cookie* aCookie, const nsACString& aHost) {
  // first, check for an exact host or domain cookie match, e.g. "google.com"
  // or ".google.com"; second a subdomain match, e.g.
  // host = "mail.google.com", cookie domain = ".google.com".
  return aCookie->RawHost() == aHost ||
         (aCookie->IsDomain() && StringEndsWith(aHost, aCookie->Host()));
}

// static
bool CookieCommons::PathMatches(Cookie* aCookie, const nsACString& aPath) {
  nsCString cookiePath(aCookie->GetFilePath());

  // if our cookie path is empty we can't really perform our prefix check, and
  // also we can't check the last character of the cookie path, so we would
  // never return a successful match.
  if (cookiePath.IsEmpty()) {
    return false;
  }

  // if the cookie path and the request path are identical, they match.
  if (cookiePath.Equals(aPath)) {
    return true;
  }

  // if the cookie path is a prefix of the request path, and the last character
  // of the cookie path is %x2F ("/"), they match.
  bool isPrefix = StringBeginsWith(aPath, cookiePath);
  if (isPrefix && cookiePath.Last() == '/') {
    return true;
  }

  // if the cookie path is a prefix of the request path, and the first character
  // of the request path that is not included in the cookie path is a %x2F ("/")
  // character, they match.
  uint32_t cookiePathLen = cookiePath.Length();
  return isPrefix && aPath[cookiePathLen] == '/';
}

// Get the base domain for aHostURI; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". Only properly-formed URI's are tolerated, though a trailing
// dot may be present. If aHostURI is an IP address, an alias such as
// 'localhost', an eTLD such as 'co.uk', or the empty string, aBaseDomain will
// be the exact host, and aRequireHostMatch will be true to indicate that
// substring matches should not be performed.
nsresult CookieCommons::GetBaseDomain(nsIEffectiveTLDService* aTLDService,
                                      nsIURI* aHostURI, nsACString& aBaseDomain,
                                      bool& aRequireHostMatch) {
  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = aTLDService->GetBaseDomain(aHostURI, 0, aBaseDomain);
  aRequireHostMatch = rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
                      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS;
  if (aRequireHostMatch) {
    // aHostURI is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases.
    rv = nsContentUtils::GetHostOrIPv6WithBrackets(aHostURI, aBaseDomain);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // aHost (and thus aBaseDomain) may be the string '.'. If so, fail.
  if (aBaseDomain.Length() == 1 && aBaseDomain.Last() == '.') {
    return NS_ERROR_INVALID_ARG;
  }

  // block any URIs without a host that aren't file:// URIs.
  if (aBaseDomain.IsEmpty() && !aHostURI->SchemeIs("file")) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

nsresult CookieCommons::GetBaseDomain(nsIPrincipal* aPrincipal,
                                      nsACString& aBaseDomain) {
  MOZ_ASSERT(aPrincipal);

  // for historical reasons we use ascii host for file:// URLs.
  if (aPrincipal->SchemeIs("file")) {
    return nsContentUtils::GetHostOrIPv6WithBrackets(aPrincipal, aBaseDomain);
  }

  nsresult rv = aPrincipal->GetBaseDomain(aBaseDomain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsContentUtils::MaybeFixIPv6Host(aBaseDomain);
  return NS_OK;
}

// Get the base domain for aHost; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". This is done differently than GetBaseDomain(mTLDService, ): it
// is assumed that aHost is already normalized, and it may contain a leading dot
// (indicating that it represents a domain). A trailing dot may be present.
// If aHost is an IP address, an alias such as 'localhost', an eTLD such as
// 'co.uk', or the empty string, aBaseDomain will be the exact host, and a
// leading dot will be treated as an error.
nsresult CookieCommons::GetBaseDomainFromHost(
    nsIEffectiveTLDService* aTLDService, const nsACString& aHost,
    nsCString& aBaseDomain) {
  // aHost must not be the string '.'.
  if (aHost.Length() == 1 && aHost.Last() == '.') {
    return NS_ERROR_INVALID_ARG;
  }

  // aHost may contain a leading dot; if so, strip it now.
  bool domain = !aHost.IsEmpty() && aHost.First() == '.';

  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = aTLDService->GetBaseDomainFromHost(Substring(aHost, domain), 0,
                                                   aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aHost is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases; however, we reject any such hosts with a leading dot, since it
    // doesn't make sense for them to be domain cookies.
    if (domain) {
      return NS_ERROR_INVALID_ARG;
    }

    aBaseDomain = aHost;
    return NS_OK;
  }
  return rv;
}

namespace {

void NotifyRejectionToObservers(nsIURI* aHostURI, CookieOperation aOperation) {
  if (aOperation == OPERATION_WRITE) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(aHostURI, "cookie-rejected", nullptr);
    }
  } else {
    MOZ_ASSERT(aOperation == OPERATION_READ);
  }
}

}  // namespace

// Notify observers that a cookie was rejected due to the users' prefs.
void CookieCommons::NotifyRejected(nsIURI* aHostURI, nsIChannel* aChannel,
                                   uint32_t aRejectedReason,
                                   CookieOperation aOperation) {
  NotifyRejectionToObservers(aHostURI, aOperation);

  ContentBlockingNotifier::OnDecision(
      aChannel, ContentBlockingNotifier::BlockingDecision::eBlock,
      aRejectedReason);
}

bool CookieCommons::CheckPathSize(const CookieStruct& aCookieData) {
  return aCookieData.path().Length() <= kMaxBytesPerPath;
}

bool CookieCommons::CheckNameAndValueSize(const CookieStruct& aCookieData) {
  // reject cookie if it's over the size limit, per RFC2109
  return (aCookieData.name().Length() + aCookieData.value().Length()) <=
         kMaxBytesPerCookie;
}

bool CookieCommons::CheckName(const CookieStruct& aCookieData) {
  const char illegalNameCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
      0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x00};

  return aCookieData.name().FindCharInSet(illegalNameCharacters, 0) == -1;
}

bool CookieCommons::CheckHttpValue(const CookieStruct& aCookieData) {
  // reject cookie if value contains an RFC 6265 disallowed character - see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1191423
  // NOTE: this is not the full set of characters disallowed by 6265 - notably
  // 0x09, 0x20, 0x22, 0x2C, 0x5C, and 0x7F are missing from this list. This is
  // for parity with Chrome. This only applies to cookies set via the Set-Cookie
  // header, as document.cookie is defined to be UTF-8. Hooray for
  // symmetry!</sarcasm>
  const char illegalCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
      0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x3B, 0x00};
  return aCookieData.value().FindCharInSet(illegalCharacters, 0) == -1;
}

// static
bool CookieCommons::CheckCookiePermission(nsIChannel* aChannel,
                                          CookieStruct& aCookieData) {
  if (!aChannel) {
    // No channel, let's assume this is a system-principal request.
    return true;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  nsresult rv =
      loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  nsIScriptSecurityManager* ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  MOZ_ASSERT(ssm);

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  rv = ssm->GetChannelURIPrincipal(aChannel, getter_AddRefs(channelPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return CheckCookiePermission(channelPrincipal, cookieJarSettings,
                               aCookieData);
}

// static
bool CookieCommons::CheckCookiePermission(
    nsIPrincipal* aPrincipal, nsICookieJarSettings* aCookieJarSettings,
    CookieStruct& aCookieData) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCookieJarSettings);

  if (!aPrincipal->GetIsContentPrincipal()) {
    return true;
  }

  uint32_t cookiePermission = nsICookiePermission::ACCESS_DEFAULT;
  nsresult rv =
      aCookieJarSettings->CookiePermission(aPrincipal, &cookiePermission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  if (cookiePermission == nsICookiePermission::ACCESS_ALLOW) {
    return true;
  }

  if (cookiePermission == nsICookiePermission::ACCESS_SESSION) {
    aCookieData.isSession() = true;
    return true;
  }

  if (cookiePermission == nsICookiePermission::ACCESS_DENY) {
    return false;
  }

  return true;
}

namespace {

CookieStatus CookieStatusForWindow(nsPIDOMWindowInner* aWindow,
                                   nsIURI* aDocumentURI) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aDocumentURI);

  ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();
  if (thirdPartyUtil) {
    bool isThirdParty = true;

    nsresult rv = thirdPartyUtil->IsThirdPartyWindow(
        aWindow->GetOuterWindow(), aDocumentURI, &isThirdParty);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Third-party window check failed.");

    if (NS_SUCCEEDED(rv) && !isThirdParty) {
      return STATUS_ACCEPTED;
    }
  }

  if (StaticPrefs::network_cookie_thirdparty_sessionOnly()) {
    return STATUS_ACCEPT_SESSION;
  }

  if (StaticPrefs::network_cookie_thirdparty_nonsecureSessionOnly() &&
      !nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aDocumentURI)) {
    return STATUS_ACCEPT_SESSION;
  }

  return STATUS_ACCEPTED;
}

}  // namespace

// static
already_AddRefed<Cookie> CookieCommons::CreateCookieFromDocument(
    Document* aDocument, const nsACString& aCookieString,
    int64_t currentTimeInUsec, nsIEffectiveTLDService* aTLDService,
    mozIThirdPartyUtil* aThirdPartyUtil,
    std::function<bool(const nsACString&, const OriginAttributes&)>&&
        aHasExistingCookiesLambda,
    nsIURI** aDocumentURI, nsACString& aBaseDomain, OriginAttributes& aAttrs) {
  nsCOMPtr<nsIPrincipal> storagePrincipal =
      aDocument->EffectiveStoragePrincipal();
  MOZ_ASSERT(storagePrincipal);

  nsCOMPtr<nsIURI> principalURI;
  auto* basePrincipal = BasePrincipal::Cast(aDocument->NodePrincipal());
  basePrincipal->GetURI(getter_AddRefs(principalURI));
  if (NS_WARN_IF(!principalURI)) {
    // Document's principal is not a content or null (may be system), so
    // can't set cookies
    return nullptr;
  }

  if (!CookieCommons::IsSchemeSupported(principalURI)) {
    return nullptr;
  }

  nsAutoCString baseDomain;
  bool requireHostMatch = false;
  nsresult rv = CookieCommons::GetBaseDomain(aTLDService, principalURI,
                                             baseDomain, requireHostMatch);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsPIDOMWindowInner* innerWindow = aDocument->GetInnerWindow();
  if (NS_WARN_IF(!innerWindow)) {
    return nullptr;
  }

  // Check if limit-foreign is required.
  uint32_t dummyRejectedReason = 0;
  if (aDocument->CookieJarSettings()->GetLimitForeignContexts() &&
      !aHasExistingCookiesLambda(baseDomain,
                                 storagePrincipal->OriginAttributesRef()) &&
      !ShouldAllowAccessFor(innerWindow, principalURI, &dummyRejectedReason)) {
    return nullptr;
  }

  bool isForeignAndNotAddon = false;
  if (!BasePrincipal::Cast(aDocument->NodePrincipal())->AddonPolicy()) {
    rv = aThirdPartyUtil->IsThirdPartyWindow(
        innerWindow->GetOuterWindow(), principalURI, &isForeignAndNotAddon);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      isForeignAndNotAddon = true;
    }
  }

  // If we are here, we have been already accepted by the anti-tracking.
  // We just need to check if we have to be in session-only mode.
  CookieStatus cookieStatus = CookieStatusForWindow(innerWindow, principalURI);
  MOZ_ASSERT(cookieStatus == STATUS_ACCEPTED ||
             cookieStatus == STATUS_ACCEPT_SESSION);

  // Console report takes care of the correct reporting at the exit of this
  // method.
  RefPtr<ConsoleReportCollector> crc = new ConsoleReportCollector();
  auto scopeExit = MakeScopeExit([&] { crc->FlushConsoleReports(aDocument); });

  nsCString cookieString(aCookieString);

  CookieStruct cookieData;
  bool canSetCookie = false;
  CookieService::CanSetCookie(principalURI, baseDomain, cookieData,
                              requireHostMatch, cookieStatus, cookieString,
                              false, isForeignAndNotAddon, crc, canSetCookie);

  if (!canSetCookie) {
    return nullptr;
  }

  // check permissions from site permission list.
  if (!CookieCommons::CheckCookiePermission(aDocument->NodePrincipal(),
                                            aDocument->CookieJarSettings(),
                                            cookieData)) {
    NotifyRejectionToObservers(principalURI, OPERATION_WRITE);
    ContentBlockingNotifier::OnDecision(
        innerWindow, ContentBlockingNotifier::BlockingDecision::eBlock,
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION);
    return nullptr;
  }

  RefPtr<Cookie> cookie =
      Cookie::Create(cookieData, storagePrincipal->OriginAttributesRef());
  MOZ_ASSERT(cookie);

  cookie->SetLastAccessed(currentTimeInUsec);
  cookie->SetCreationTime(
      Cookie::GenerateUniqueCreationTime(currentTimeInUsec));

  aBaseDomain = baseDomain;
  aAttrs = storagePrincipal->OriginAttributesRef();
  principalURI.forget(aDocumentURI);

  return cookie.forget();
}

// static
already_AddRefed<nsICookieJarSettings> CookieCommons::GetCookieJarSettings(
    nsIChannel* aChannel) {
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  if (aChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    nsresult rv =
        loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      cookieJarSettings = CookieJarSettings::GetBlockingAll();
    }
  } else {
    cookieJarSettings = CookieJarSettings::Create(CookieJarSettings::eRegular);
  }

  MOZ_ASSERT(cookieJarSettings);
  return cookieJarSettings.forget();
}

// static
bool CookieCommons::ShouldIncludeCrossSiteCookieForDocument(Cookie* aCookie) {
  MOZ_ASSERT(aCookie);

  int32_t sameSiteAttr = 0;
  aCookie->GetSameSite(&sameSiteAttr);

  return sameSiteAttr == nsICookie::SAMESITE_NONE;
}

bool CookieCommons::IsSafeTopLevelNav(nsIChannel* aChannel) {
  if (!aChannel) {
    return false;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
          ExtContentPolicy::TYPE_DOCUMENT &&
      loadInfo->GetExternalContentPolicyType() !=
          ExtContentPolicy::TYPE_SAVEAS_DOWNLOAD) {
    return false;
  }
  return NS_IsSafeMethodNav(aChannel);
}

// This function determines if two schemes are equal in the context of
// "Schemeful SameSite cookies".
//
// Two schemes are considered equal:
//   - if the "network.cookie.sameSite.schemeful" pref is set to false.
// OR
//   - if one of the schemes is not http or https.
// OR
//   - if both schemes are equal AND both are either http or https.
bool IsSameSiteSchemeEqual(const nsACString& aFirstScheme,
                           const nsACString& aSecondScheme) {
  if (!StaticPrefs::network_cookie_sameSite_schemeful()) {
    return true;
  }

  auto isSchemeHttpOrHttps = [](const nsACString& scheme) -> bool {
    return scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https");
  };

  if (!isSchemeHttpOrHttps(aFirstScheme) ||
      !isSchemeHttpOrHttps(aSecondScheme)) {
    return true;
  }

  return aFirstScheme.Equals(aSecondScheme);
}

bool CookieCommons::IsSameSiteForeign(nsIChannel* aChannel, nsIURI* aHostURI,
                                      bool* aHadCrossSiteRedirects) {
  *aHadCrossSiteRedirects = false;

  if (!aChannel) {
    return false;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  // Do not treat loads triggered by web extensions as foreign
  nsCOMPtr<nsIURI> channelURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  RefPtr<BasePrincipal> triggeringPrincipal =
      BasePrincipal::Cast(loadInfo->TriggeringPrincipal());
  if (triggeringPrincipal->AddonPolicy() &&
      triggeringPrincipal->AddonAllowsLoad(channelURI)) {
    return false;
  }

  nsAutoCString hostScheme, otherScheme;
  aHostURI->GetScheme(hostScheme);

  bool isForeign = true;
  nsresult rv;
  if (loadInfo->GetExternalContentPolicyType() ==
          ExtContentPolicy::TYPE_DOCUMENT ||
      loadInfo->GetExternalContentPolicyType() ==
          ExtContentPolicy::TYPE_SAVEAS_DOWNLOAD) {
    // for loads of TYPE_DOCUMENT we query the hostURI from the
    // triggeringPrincipal which returns the URI of the document that caused the
    // navigation.
    rv = triggeringPrincipal->IsThirdPartyChannel(aChannel, &isForeign);

    triggeringPrincipal->GetScheme(otherScheme);
  } else {
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID);
    if (!thirdPartyUtil) {
      return true;
    }
    rv = thirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

    channelURI->GetScheme(otherScheme);
  }
  // if we are dealing with a cross origin request, we can return here
  // because we already know the request is 'foreign'.
  if (NS_FAILED(rv) || isForeign) {
    return true;
  }

  if (!IsSameSiteSchemeEqual(otherScheme, hostScheme)) {
    // If the two schemes are not of the same http(s) scheme then we
    // consider the request as foreign.
    return true;
  }

  // for loads of TYPE_SUBDOCUMENT we have to perform an additional test,
  // because a cross-origin iframe might perform a navigation to a same-origin
  // iframe which would send same-site cookies. Hence, if the iframe navigation
  // was triggered by a cross-origin triggeringPrincipal, we treat the load as
  // foreign.
  if (loadInfo->GetExternalContentPolicyType() ==
      ExtContentPolicy::TYPE_SUBDOCUMENT) {
    rv = loadInfo->TriggeringPrincipal()->IsThirdPartyChannel(aChannel,
                                                              &isForeign);
    if (NS_FAILED(rv) || isForeign) {
      return true;
    }
  }

  // for the purpose of same-site cookies we have to treat any cross-origin
  // redirects as foreign. E.g. cross-site to same-site redirect is a problem
  // with regards to CSRF.

  nsCOMPtr<nsIPrincipal> redirectPrincipal;
  for (nsIRedirectHistoryEntry* entry : loadInfo->RedirectChain()) {
    entry->GetPrincipal(getter_AddRefs(redirectPrincipal));
    if (redirectPrincipal) {
      rv = redirectPrincipal->IsThirdPartyChannel(aChannel, &isForeign);
      // if at any point we encounter a cross-origin redirect we can return.
      if (NS_FAILED(rv) || isForeign) {
        *aHadCrossSiteRedirects = true;
        return true;
      }

      nsAutoCString redirectScheme;
      redirectPrincipal->GetScheme(redirectScheme);
      if (!IsSameSiteSchemeEqual(redirectScheme, hostScheme)) {
        // If the two schemes are not of the same http(s) scheme then we
        // consider the request as foreign.
        *aHadCrossSiteRedirects = true;
        return true;
      }
    }
  }
  return isForeign;
}

// static
nsICookie::schemeType CookieCommons::URIToSchemeType(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsICookie::SCHEME_UNSET;
  }

  return SchemeToSchemeType(scheme);
}

// static
nsICookie::schemeType CookieCommons::PrincipalToSchemeType(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  nsAutoCString scheme;
  nsresult rv = aPrincipal->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsICookie::SCHEME_UNSET;
  }

  return SchemeToSchemeType(scheme);
}

// static
nsICookie::schemeType CookieCommons::SchemeToSchemeType(
    const nsACString& aScheme) {
  MOZ_ASSERT(IsSchemeSupported(aScheme));

  if (aScheme.Equals("https")) {
    return nsICookie::SCHEME_HTTPS;
  }

  if (aScheme.Equals("http")) {
    return nsICookie::SCHEME_HTTP;
  }

  if (aScheme.Equals("file")) {
    return nsICookie::SCHEME_FILE;
  }

  MOZ_CRASH("Unsupported scheme type");
}

// static
bool CookieCommons::IsSchemeSupported(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  nsAutoCString scheme;
  nsresult rv = aPrincipal->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return IsSchemeSupported(scheme);
}

// static
bool CookieCommons::IsSchemeSupported(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return IsSchemeSupported(scheme);
}

// static
bool CookieCommons::IsSchemeSupported(const nsACString& aScheme) {
  return aScheme.Equals("https") || aScheme.Equals("http") ||
         aScheme.Equals("file");
}

}  // namespace net
}  // namespace mozilla
