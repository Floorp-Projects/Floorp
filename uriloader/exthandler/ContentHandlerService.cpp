/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentHandlerService.h"
#include "HandlerServiceChild.h"
#include "ContentChild.h"
#include "nsIMutableArray.h"
#include "nsIMIMEInfo.h"
#include "nsIStringEnumerator.h"

using mozilla::dom::ContentChild;
using mozilla::dom::HandlerInfo;
using mozilla::dom::PHandlerServiceChild;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ContentHandlerService, nsIHandlerService)

ContentHandlerService::ContentHandlerService() {}

nsresult ContentHandlerService::Init() {
  if (!XRE_IsContentProcess()) {
    return NS_ERROR_FAILURE;
  }
  ContentChild* cpc = ContentChild::GetSingleton();

  mHandlerServiceChild = new HandlerServiceChild();
  if (!cpc->SendPHandlerServiceConstructor(mHandlerServiceChild)) {
    mHandlerServiceChild = nullptr;
  }
  return NS_OK;
}

void ContentHandlerService::nsIHandlerInfoToHandlerInfo(
    nsIHandlerInfo* aInfo, HandlerInfo* aHandlerInfo) {
  nsCString type;
  aInfo->GetType(type);
  nsCOMPtr<nsIMIMEInfo> mimeInfo = do_QueryInterface(aInfo);
  bool isMIMEInfo = !!mimeInfo;
  nsString description;
  aInfo->GetDescription(description);
  bool alwaysAskBeforeHandling;
  aInfo->GetAlwaysAskBeforeHandling(&alwaysAskBeforeHandling);
  nsCOMPtr<nsIHandlerApp> app;
  aInfo->GetPreferredApplicationHandler(getter_AddRefs(app));
  nsString name;
  nsString detailedDescription;
  if (app) {
    app->GetName(name);
    app->GetDetailedDescription(detailedDescription);
  }
  HandlerApp happ(name, detailedDescription);
  nsTArray<HandlerApp> happs;
  nsCOMPtr<nsIMutableArray> apps;
  aInfo->GetPossibleApplicationHandlers(getter_AddRefs(apps));
  if (apps) {
    unsigned int length;
    apps->GetLength(&length);
    for (unsigned int i = 0; i < length; i++) {
      apps->QueryElementAt(i, NS_GET_IID(nsIHandlerApp), getter_AddRefs(app));
      app->GetName(name);
      app->GetDetailedDescription(detailedDescription);
      happs.AppendElement(HandlerApp(name, detailedDescription));
    }
  }

  nsTArray<nsCString> extensions;

  if (isMIMEInfo) {
    nsCOMPtr<nsIUTF8StringEnumerator> extensionsIter;
    mimeInfo->GetFileExtensions(getter_AddRefs(extensionsIter));
    if (extensionsIter) {
      bool hasMore = false;
      while (NS_SUCCEEDED(extensionsIter->HasMore(&hasMore)) && hasMore) {
        nsAutoCString extension;
        if (NS_SUCCEEDED(extensionsIter->GetNext(extension))) {
          extensions.AppendElement(std::move(extension));
        }
      }
    }
  }

  nsHandlerInfoAction action;
  aInfo->GetPreferredAction(&action);
  HandlerInfo info(type, isMIMEInfo, description, alwaysAskBeforeHandling,
                   std::move(extensions), happ, happs, action);
  *aHandlerInfo = info;
}

NS_IMETHODIMP RemoteHandlerApp::GetName(nsAString& aName) {
  aName.Assign(mAppChild.name());
  return NS_OK;
}

NS_IMETHODIMP RemoteHandlerApp::SetName(const nsAString& aName) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteHandlerApp::GetDetailedDescription(
    nsAString& aDetailedDescription) {
  aDetailedDescription.Assign(mAppChild.detailedDescription());
  return NS_OK;
}

NS_IMETHODIMP RemoteHandlerApp::SetDetailedDescription(
    const nsAString& aDetailedDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteHandlerApp::Equals(nsIHandlerApp* aHandlerApp,
                                       bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteHandlerApp::LaunchWithURI(
    nsIURI* aURI, nsIInterfaceRequestor* aWindowContext) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS(RemoteHandlerApp, nsIHandlerApp)

static inline void CopyHanderInfoTonsIHandlerInfo(
    const HandlerInfo& info, nsIHandlerInfo* aHandlerInfo) {
  HandlerApp preferredApplicationHandler = info.preferredApplicationHandler();
  nsCOMPtr<nsIHandlerApp> preferredApp(
      new RemoteHandlerApp(preferredApplicationHandler));
  aHandlerInfo->SetPreferredApplicationHandler(preferredApp);
  nsCOMPtr<nsIMutableArray> possibleHandlers;
  aHandlerInfo->GetPossibleApplicationHandlers(
      getter_AddRefs(possibleHandlers));
  possibleHandlers->AppendElement(preferredApp);

  if (info.isMIMEInfo()) {
    const auto& fileExtensions = info.extensions();
    bool first = true;
    nsAutoCString extensionsStr;
    for (const auto& extension : fileExtensions) {
      if (!first) {
        extensionsStr.Append(',');
      }

      extensionsStr.Append(extension);
      first = false;
    }

    nsCOMPtr<nsIMIMEInfo> mimeInfo(do_QueryInterface(aHandlerInfo));
    MOZ_ASSERT(mimeInfo,
               "parent and child don't agree on whether this is a MIME info");
    mimeInfo->SetFileExtensions(extensionsStr);
  }
}

ContentHandlerService::~ContentHandlerService() {}

NS_IMETHODIMP ContentHandlerService::AsyncInit() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ContentHandlerService::Enumerate(nsISimpleEnumerator** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ContentHandlerService::FillHandlerInfo(
    nsIHandlerInfo* aHandlerInfo, const nsACString& aOverrideType) {
  HandlerInfo info, returnedInfo;
  nsIHandlerInfoToHandlerInfo(aHandlerInfo, &info);
  mHandlerServiceChild->SendFillHandlerInfo(info, nsCString(aOverrideType),
                                            &returnedInfo);
  CopyHanderInfoTonsIHandlerInfo(returnedInfo, aHandlerInfo);
  return NS_OK;
}

NS_IMETHODIMP ContentHandlerService::GetMIMEInfoFromOS(
    nsIHandlerInfo* aHandlerInfo, const nsACString& aMIMEType,
    const nsACString& aExtension, bool* aFound) {
  nsresult rv = NS_ERROR_FAILURE;
  HandlerInfo returnedInfo;
  if (!mHandlerServiceChild->SendGetMIMEInfoFromOS(nsCString(aMIMEType),
                                                   nsCString(aExtension), &rv,
                                                   &returnedInfo, aFound)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  CopyHanderInfoTonsIHandlerInfo(returnedInfo, aHandlerInfo);
  return NS_OK;
}

NS_IMETHODIMP ContentHandlerService::Store(nsIHandlerInfo* aHandlerInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ContentHandlerService::Exists(nsIHandlerInfo* aHandlerInfo,
                                            bool* _retval) {
  HandlerInfo info;
  nsIHandlerInfoToHandlerInfo(aHandlerInfo, &info);
  mHandlerServiceChild->SendExists(info, _retval);
  return NS_OK;
}

NS_IMETHODIMP ContentHandlerService::Remove(nsIHandlerInfo* aHandlerInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ContentHandlerService::ExistsForProtocolOS(const nsACString& aProtocolScheme,
                                           bool* aRetval) {
  if (!mHandlerServiceChild->SendExistsForProtocolOS(nsCString(aProtocolScheme),
                                                     aRetval)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
ContentHandlerService::ExistsForProtocol(const nsACString& aProtocolScheme,
                                         bool* aRetval) {
  if (!mHandlerServiceChild->SendExistsForProtocol(nsCString(aProtocolScheme),
                                                   aRetval)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP ContentHandlerService::GetTypeFromExtension(
    const nsACString& aFileExtension, nsACString& _retval) {
  nsCString* cachedType = nullptr;
  if (!!mExtToTypeMap.Get(aFileExtension, &cachedType) && !!cachedType) {
    _retval.Assign(*cachedType);
    return NS_OK;
  }
  nsCString type;
  mHandlerServiceChild->SendGetTypeFromExtension(nsCString(aFileExtension),
                                                 &type);
  _retval.Assign(type);
  mExtToTypeMap.Put(nsCString(aFileExtension), new nsCString(type));

  return NS_OK;
}

NS_IMETHODIMP ContentHandlerService::GetApplicationDescription(
    const nsACString& aProtocolScheme, nsAString& aRetVal) {
  nsresult rv = NS_ERROR_FAILURE;
  nsAutoCString scheme(aProtocolScheme);
  nsAutoString desc;
  mHandlerServiceChild->SendGetApplicationDescription(scheme, &rv, &desc);
  aRetVal.Assign(desc);
  return rv;
}

}  // namespace dom
}  // namespace mozilla
