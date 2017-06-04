/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "nsEscape.h"
#include "nsGkAtoms.h"

namespace mozilla {

using namespace extensions;

#define DEFAULT_BASE_CSP \
    "script-src 'self' https://* moz-extension: blob: filesystem: 'unsafe-eval' 'unsafe-inline'; " \
    "object-src 'self' https://* moz-extension: blob: filesystem:;"

#define DEFAULT_DEFAULT_CSP \
    "script-src 'self'; object-src 'self';"


/*****************************************************************************
 * ExtensionPolicyService
 *****************************************************************************/

/* static */ ExtensionPolicyService&
ExtensionPolicyService::GetSingleton()
{
  static RefPtr<ExtensionPolicyService> sExtensionPolicyService;

  if (MOZ_UNLIKELY(!sExtensionPolicyService)) {
    sExtensionPolicyService = new ExtensionPolicyService();
    ClearOnShutdown(&sExtensionPolicyService);
  }
  return *sExtensionPolicyService.get();
}


WebExtensionPolicy*
ExtensionPolicyService::GetByURL(const URLInfo& aURL)
{
  if (aURL.Scheme() == nsGkAtoms::moz_extension) {
    return GetByHost(aURL.Host());
  }
  return nullptr;
}

void
ExtensionPolicyService::GetAll(nsTArray<RefPtr<WebExtensionPolicy>>& aResult)
{
  for (auto iter = mExtensions.Iter(); !iter.Done(); iter.Next()) {
    aResult.AppendElement(iter.Data());
  }
}

bool
ExtensionPolicyService::RegisterExtension(WebExtensionPolicy& aPolicy)
{
  bool ok = (!GetByID(aPolicy.Id()) &&
             !GetByHost(aPolicy.MozExtensionHostname()));
  MOZ_ASSERT(ok);

  if (!ok) {
    return false;
  }

  mExtensions.Put(aPolicy.Id(), &aPolicy);
  mExtensionHosts.Put(aPolicy.MozExtensionHostname(), &aPolicy);
  return true;
}

bool
ExtensionPolicyService::UnregisterExtension(WebExtensionPolicy& aPolicy)
{
  bool ok = (GetByID(aPolicy.Id()) == &aPolicy &&
             GetByHost(aPolicy.MozExtensionHostname()) == &aPolicy);
  MOZ_ASSERT(ok);

  if (!ok) {
    return false;
  }

  mExtensions.Remove(aPolicy.Id());
  mExtensionHosts.Remove(aPolicy.MozExtensionHostname());
  return true;
}


void
ExtensionPolicyService::BaseCSP(nsAString& aBaseCSP) const
{
  nsresult rv;

  rv = Preferences::GetString("extensions.webextensions.base-content-security-policy", &aBaseCSP);
  if (NS_FAILED(rv)) {
    aBaseCSP.AssignLiteral(DEFAULT_BASE_CSP);
  }
}

void
ExtensionPolicyService::DefaultCSP(nsAString& aDefaultCSP) const
{
  nsresult rv;

  rv = Preferences::GetString("extensions.webextensions.default-content-security-policy", &aDefaultCSP);
  if (NS_FAILED(rv)) {
    aDefaultCSP.AssignLiteral(DEFAULT_DEFAULT_CSP);
  }
}


/*****************************************************************************
 * nsIAddonPolicyService
 *****************************************************************************/

nsresult
ExtensionPolicyService::GetBaseCSP(nsAString& aBaseCSP)
{
  BaseCSP(aBaseCSP);
  return NS_OK;
}

nsresult
ExtensionPolicyService::GetDefaultCSP(nsAString& aDefaultCSP)
{
  DefaultCSP(aDefaultCSP);
  return NS_OK;
}

nsresult
ExtensionPolicyService::GetAddonCSP(const nsAString& aAddonId,
                                    nsAString& aResult)
{
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    policy->GetContentSecurityPolicy(aResult);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::GetGeneratedBackgroundPageUrl(const nsACString& aHostname,
                                                      nsACString& aResult)
{
  if (WebExtensionPolicy* policy = GetByHost(aHostname)) {
    nsAutoCString url("data:text/html,");

    nsCString html = policy->BackgroundPageHTML();
    nsAutoCString escaped;

    url.Append(NS_EscapeURL(html, esc_Minimal, escaped));

    aResult = url;
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::AddonHasPermission(const nsAString& aAddonId,
                                           const nsAString& aPerm,
                                           bool* aResult)
{
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    *aResult = policy->HasPermission(aPerm);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::AddonMayLoadURI(const nsAString& aAddonId,
                                        nsIURI* aURI,
                                        bool aExplicit,
                                        bool* aResult)
{
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    *aResult = policy->CanAccessURI(aURI, aExplicit);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::ExtensionURILoadableByAnyone(nsIURI* aURI, bool* aResult)
{
  URLInfo url(aURI);
  if (WebExtensionPolicy* policy = GetByURL(url)) {
    *aResult = policy->IsPathWebAccessible(url.FilePath());
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::ExtensionURIToAddonId(nsIURI* aURI, nsAString& aResult)
{
  if (WebExtensionPolicy* policy = GetByURL(aURI)) {
    policy->GetId(aResult);
  } else {
    aResult.SetIsVoid(true);
  }
  return NS_OK;
}


NS_IMPL_CYCLE_COLLECTION(ExtensionPolicyService, mExtensions, mExtensionHosts)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionPolicyService)
  NS_INTERFACE_MAP_ENTRY(nsIAddonPolicyService)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionPolicyService)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionPolicyService)

} // namespace mozilla
