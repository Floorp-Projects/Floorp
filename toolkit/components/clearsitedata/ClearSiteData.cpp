/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClearSiteData.h"

#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsASCIIMask.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsIClearDataService.h"
#include "nsIHttpChannel.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"

using namespace mozilla;

namespace {

StaticRefPtr<ClearSiteData> gClearSiteData;

}  // namespace

// This object is used to suspend/resume the channel.
class ClearSiteData::PendingCleanupHolder final : public nsIClearDataCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit PendingCleanupHolder(nsIHttpChannel* aChannel)
      : mChannel(aChannel), mPendingOp(false) {}

  nsresult Start() {
    MOZ_ASSERT(!mPendingOp);
    nsresult rv = mChannel->Suspend();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mPendingOp = true;
    return NS_OK;
  }

  // nsIClearDataCallback interface

  NS_IMETHOD
  OnDataDeleted(uint32_t aFailedFlags) override {
    MOZ_ASSERT(mPendingOp);
    mPendingOp = false;

    mChannel->Resume();
    mChannel = nullptr;

    return NS_OK;
  }

 private:
  ~PendingCleanupHolder() {
    if (mPendingOp) {
      mChannel->Resume();
    }
  }

  nsCOMPtr<nsIHttpChannel> mChannel;
  bool mPendingOp;
};

NS_INTERFACE_MAP_BEGIN(ClearSiteData::PendingCleanupHolder)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIClearDataCallback)
  NS_INTERFACE_MAP_ENTRY(nsIClearDataCallback)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(ClearSiteData::PendingCleanupHolder)
NS_IMPL_RELEASE(ClearSiteData::PendingCleanupHolder)

/* static */
void ClearSiteData::Initialize() {
  MOZ_ASSERT(!gClearSiteData);
  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return;
  }

  RefPtr<ClearSiteData> service = new ClearSiteData();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->AddObserver(service, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC, false);
  obs->AddObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  gClearSiteData = service;
}

/* static */
void ClearSiteData::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gClearSiteData) {
    return;
  }

  RefPtr<ClearSiteData> service = gClearSiteData;
  gClearSiteData = nullptr;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->RemoveObserver(service, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC);
  obs->RemoveObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
}

ClearSiteData::ClearSiteData() = default;
ClearSiteData::~ClearSiteData() = default;

NS_IMETHODIMP
ClearSiteData::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
    return NS_OK;
  }

  MOZ_ASSERT(!strcmp(aTopic, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC));

  nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aSubject);
  if (NS_WARN_IF(!channel)) {
    return NS_OK;
  }

  ClearDataFromChannel(channel);
  return NS_OK;
}

void ClearSiteData::ClearDataFromChannel(nsIHttpChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsresult rv;
  nsCOMPtr<nsIURI> uri;

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (NS_WARN_IF(!ssm)) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(principal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  bool secure = principal->GetIsOriginPotentiallyTrustworthy();
  if (NS_WARN_IF(NS_FAILED(rv)) || !secure) {
    return;
  }

  // We want to use the final URI to check if Clear-Site-Data should be allowed
  // or not.
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  uint32_t flags = ParseHeader(aChannel, uri);
  if (flags == 0) {
    // Nothing to do.
    return;
  }

  int32_t cleanFlags = 0;
  RefPtr<PendingCleanupHolder> holder = new PendingCleanupHolder(aChannel);

  if (flags & eCache) {
    LogOpToConsole(aChannel, uri, eCache);
    cleanFlags |= nsIClearDataService::CLEAR_ALL_CACHES;
  }

  if (flags & eCookies) {
    LogOpToConsole(aChannel, uri, eCookies);
    cleanFlags |= nsIClearDataService::CLEAR_COOKIES;
  }

  if (flags & eStorage) {
    LogOpToConsole(aChannel, uri, eStorage);
    cleanFlags |= nsIClearDataService::CLEAR_DOM_STORAGES;
  }

  if (cleanFlags) {
    nsCOMPtr<nsIClearDataService> csd =
        do_GetService("@mozilla.org/clear-data-service;1");
    MOZ_ASSERT(csd);

    rv = holder->Start();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    rv = csd->DeleteDataFromPrincipal(principal, false /* user request */,
                                      cleanFlags, holder);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }
}

uint32_t ClearSiteData::ParseHeader(nsIHttpChannel* aChannel,
                                    nsIURI* aURI) const {
  MOZ_ASSERT(aChannel);

  nsAutoCString headerValue;
  nsresult rv = aChannel->GetResponseHeader("Clear-Site-Data"_ns, headerValue);
  if (NS_FAILED(rv)) {
    return 0;
  }

  uint32_t flags = 0;

  nsCCharSeparatedTokenizer token(headerValue, ',');
  while (token.hasMoreTokens()) {
    auto value = token.nextToken();
    value.StripTaggedASCII(mozilla::ASCIIMask::MaskWhitespace());

    if (value.EqualsLiteral("\"cache\"")) {
      flags |= eCache;
      continue;
    }

    if (value.EqualsLiteral("\"cookies\"")) {
      flags |= eCookies;
      continue;
    }

    if (value.EqualsLiteral("\"storage\"")) {
      flags |= eStorage;
      continue;
    }

    if (value.EqualsLiteral("\"*\"")) {
      flags = eCache | eCookies | eStorage;
      break;
    }

    LogErrorToConsole(aChannel, aURI, value);
  }

  return flags;
}

void ClearSiteData::LogOpToConsole(nsIHttpChannel* aChannel, nsIURI* aURI,
                                   Type aType) const {
  nsAutoString type;
  TypeToString(aType, type);

  nsTArray<nsString> params;
  params.AppendElement(type);

  LogToConsoleInternal(aChannel, aURI, "RunningClearSiteDataValue", params);
}

void ClearSiteData::LogErrorToConsole(nsIHttpChannel* aChannel, nsIURI* aURI,
                                      const nsACString& aUnknownType) const {
  nsTArray<nsString> params;
  params.AppendElement(NS_ConvertUTF8toUTF16(aUnknownType));

  LogToConsoleInternal(aChannel, aURI, "UnknownClearSiteDataValue", params);
}

void ClearSiteData::LogToConsoleInternal(
    nsIHttpChannel* aChannel, nsIURI* aURI, const char* aMsg,
    const nsTArray<nsString>& aParams) const {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<net::HttpBaseChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return;
  }

  nsAutoCString uri;
  nsresult rv = aURI->GetSpec(uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  httpChannel->AddConsoleReport(nsIScriptError::infoFlag, "Clear-Site-Data"_ns,
                                nsContentUtils::eSECURITY_PROPERTIES, uri, 0, 0,
                                nsDependentCString(aMsg), aParams);
}

void ClearSiteData::TypeToString(Type aType, nsAString& aStr) const {
  switch (aType) {
    case eCache:
      aStr.AssignLiteral("cache");
      break;

    case eCookies:
      aStr.AssignLiteral("cookies");
      break;

    case eStorage:
      aStr.AssignLiteral("storage");
      break;

    default:
      MOZ_CRASH("Unknown type.");
  }
}

NS_INTERFACE_MAP_BEGIN(ClearSiteData)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(ClearSiteData)
NS_IMPL_RELEASE(ClearSiteData)
