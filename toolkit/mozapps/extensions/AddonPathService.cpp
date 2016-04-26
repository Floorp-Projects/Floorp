/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonPathService.h"

#include "amIAddonManager.h"
#include "nsIURI.h"
#include "nsXULAppAPI.h"
#include "jsapi.h"
#include "nsServiceManagerUtils.h"
#include "nsLiteralString.h"
#include "nsThreadUtils.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIAddonPolicyService.h"
#include "nsIFileURL.h"
#include "nsIResProtocolHandler.h"
#include "nsIChromeRegistry.h"
#include "nsIJARURI.h"
#include "nsJSUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/AddonPathService.h"
#include "mozilla/Omnijar.h"

#include <algorithm>

namespace mozilla {

struct PathEntryComparator
{
  typedef AddonPathService::PathEntry PathEntry;

  bool Equals(const PathEntry& entry1, const PathEntry& entry2) const
  {
    return entry1.mPath == entry2.mPath;
  }

  bool LessThan(const PathEntry& entry1, const PathEntry& entry2) const
  {
    return entry1.mPath < entry2.mPath;
  }
};

AddonPathService::AddonPathService()
{
}

AddonPathService::~AddonPathService()
{
  sInstance = nullptr;
}

NS_IMPL_ISUPPORTS(AddonPathService, amIAddonPathService)

AddonPathService *AddonPathService::sInstance;

/* static */ AddonPathService*
AddonPathService::GetInstance()
{
  if (!sInstance) {
    sInstance = new AddonPathService();
  }
  NS_ADDREF(sInstance);
  return sInstance;
}

static JSAddonId*
ConvertAddonId(const nsAString& addonIdString)
{
  AutoSafeJSContext cx;
  JS::RootedValue strv(cx);
  if (!mozilla::dom::ToJSValue(cx, addonIdString, &strv)) {
    return nullptr;
  }
  JS::RootedString str(cx, strv.toString());
  return JS::NewAddonId(cx, str);
}

JSAddonId*
AddonPathService::Find(const nsAString& path)
{
  // Use binary search to find the nearest entry that is <= |path|.
  PathEntryComparator comparator;
  unsigned index = mPaths.IndexOfFirstElementGt(PathEntry(path, nullptr), comparator);
  if (index == 0) {
    return nullptr;
  }
  const PathEntry& entry = mPaths[index - 1];

  // Return the entry's addon if its path is a prefix of |path|.
  if (StringBeginsWith(path, entry.mPath)) {
    return entry.mAddonId;
  }
  return nullptr;
}

NS_IMETHODIMP
AddonPathService::FindAddonId(const nsAString& path, nsAString& addonIdString)
{
  if (JSAddonId* id = Find(path)) {
    JSFlatString* flat = JS_ASSERT_STRING_IS_FLAT(JS::StringOfAddonId(id));
    AssignJSFlatString(addonIdString, flat);
  }
  return NS_OK;
}

/* static */ JSAddonId*
AddonPathService::FindAddonId(const nsAString& path)
{
  // If no service has been created, then we're not going to find anything.
  if (!sInstance) {
    return nullptr;
  }

  return sInstance->Find(path);
}

NS_IMETHODIMP
AddonPathService::InsertPath(const nsAString& path, const nsAString& addonIdString)
{
  JSAddonId* addonId = ConvertAddonId(addonIdString);

  // Add the new path in sorted order.
  PathEntryComparator comparator;
  mPaths.InsertElementSorted(PathEntry(path, addonId), comparator);
  return NS_OK;
}

NS_IMETHODIMP
AddonPathService::MapURIToAddonId(nsIURI* aURI, nsAString& addonIdString)
{
  if (JSAddonId* id = MapURIToAddonID(aURI)) {
    JSFlatString* flat = JS_ASSERT_STRING_IS_FLAT(JS::StringOfAddonId(id));
    AssignJSFlatString(addonIdString, flat);
  }
  return NS_OK;
}

static nsresult
ResolveURI(nsIURI* aURI, nsAString& out)
{
  bool equals;
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  nsAutoCString spec;

  // Resolve resource:// URIs. At the end of this if/else block, we
  // have both spec and uri variables identifying the same URI.
  if (NS_SUCCEEDED(aURI->SchemeIs("resource", &equals)) && equals) {
    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    nsCOMPtr<nsIProtocolHandler> ph;
    rv = ioService->GetProtocolHandler("resource", getter_AddRefs(ph));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    nsCOMPtr<nsIResProtocolHandler> irph(do_QueryInterface(ph, &rv));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    rv = irph->ResolveURI(aURI, spec);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    rv = ioService->NewURI(spec, nullptr, nullptr, getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;
  } else if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &equals)) && equals) {
    nsCOMPtr<nsIChromeRegistry> chromeReg =
      mozilla::services::GetChromeRegistryService();
    if (NS_WARN_IF(!chromeReg))
      return NS_ERROR_UNEXPECTED;

    rv = chromeReg->ConvertChromeURL(aURI, getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;
  } else {
    uri = aURI;
  }

  if (NS_SUCCEEDED(uri->SchemeIs("jar", &equals)) && equals) {
    nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri, &rv);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    nsCOMPtr<nsIURI> jarFileURI;
    rv = jarURI->GetJARFile(getter_AddRefs(jarFileURI));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    return ResolveURI(jarFileURI, out);
  }

  if (NS_SUCCEEDED(uri->SchemeIs("file", &equals)) && equals) {
    nsCOMPtr<nsIFileURL> baseFileURL = do_QueryInterface(uri, &rv);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    nsCOMPtr<nsIFile> file;
    rv = baseFileURL->GetFile(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    return file->GetPath(out);
  }
  return NS_ERROR_FAILURE;
}

JSAddonId*
MapURIToAddonID(nsIURI* aURI)
{
  if (!NS_IsMainThread() || !XRE_IsParentProcess()) {
    return nullptr;
  }

  bool equals;
  nsresult rv;
  if (NS_SUCCEEDED(aURI->SchemeIs("moz-extension", &equals)) && equals) {
    nsCOMPtr<nsIAddonPolicyService> service = do_GetService("@mozilla.org/addons/policy-service;1");
    if (service) {
      nsString addonId;
      rv = service->ExtensionURIToAddonId(aURI, addonId);
      if (NS_FAILED(rv))
        return nullptr;

      return ConvertAddonId(addonId);
    }
  }

  nsAutoString filePath;
  rv = ResolveURI(aURI, filePath);
  if (NS_FAILED(rv))
    return nullptr;

  nsCOMPtr<nsIFile> greJar = Omnijar::GetPath(Omnijar::GRE);
  nsCOMPtr<nsIFile> appJar = Omnijar::GetPath(Omnijar::APP);
  if (greJar && appJar) {
    nsAutoString greJarString, appJarString;
    if (NS_FAILED(greJar->GetPath(greJarString)) || NS_FAILED(appJar->GetPath(appJarString)))
      return nullptr;

    // If |aURI| is part of either Omnijar, then it can't be part of an
    // add-on. This catches pretty much all URLs for Firefox content.
    if (filePath.Equals(greJarString) || filePath.Equals(appJarString))
      return nullptr;
  }

  // If it's not part of Firefox, we resort to binary searching through the
  // add-on paths.
  return AddonPathService::FindAddonId(filePath);
}

} // namespace mozilla
