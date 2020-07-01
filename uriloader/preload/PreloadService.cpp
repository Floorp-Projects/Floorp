/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreloadService.h"

#include "FetchPreloader.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/HTMLLinkElement.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/FontPreloader.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIReferrerInfo.h"
#include "nsNetUtil.h"

namespace mozilla {

bool PreloadService::RegisterPreload(const PreloadHashKey& aKey,
                                     PreloaderBase* aPreload) {
  if (PreloadExists(aKey)) {
    return false;
  }

  mPreloads.Put(aKey, RefPtr{aPreload});
  return true;
}

void PreloadService::DeregisterPreload(const PreloadHashKey& aKey) {
  mPreloads.Remove(aKey);
}

void PreloadService::ClearAllPreloads() { mPreloads.Clear(); }

bool PreloadService::PreloadExists(const PreloadHashKey& aKey) {
  bool found;
  mPreloads.GetWeak(aKey, &found);
  return found;
}

already_AddRefed<PreloaderBase> PreloadService::LookupPreload(
    const PreloadHashKey& aKey) const {
  return mPreloads.Get(aKey);
}

already_AddRefed<nsIURI> PreloadService::GetPreloadURI(const nsAString& aURL) {
  nsIURI* base = BaseURIForPreload();
  auto encoding = mDocument->GetDocumentCharacterSet();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, encoding, base);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return uri.forget();
}

already_AddRefed<PreloaderBase> PreloadService::PreloadLinkElement(
    dom::HTMLLinkElement* aLinkElement, nsContentPolicyType aPolicyType,
    nsIReferrerInfo* aReferrerInfo) {
  if (!StaticPrefs::network_preload()) {
    return nullptr;
  }

  if (!CheckReferrerURIScheme(aReferrerInfo)) {
    return nullptr;
  }

  if (aPolicyType == nsIContentPolicy::TYPE_INVALID) {
    NotifyNodeEvent(aLinkElement, false);
    return nullptr;
  }

  nsAutoString as, charset, crossOrigin, integrity, referrerPolicy, srcset,
      sizes, type, url;

  nsCOMPtr<nsIURI> uri = aLinkElement->GetURI();
  aLinkElement->GetAs(as);
  aLinkElement->GetCharset(charset);
  aLinkElement->GetImageSrcset(srcset);
  aLinkElement->GetImageSizes(sizes);
  aLinkElement->GetHref(url);
  aLinkElement->GetCrossOrigin(crossOrigin);
  aLinkElement->GetIntegrity(integrity);
  aLinkElement->GetReferrerPolicy(referrerPolicy);
  aLinkElement->GetType(type);

  auto result =
      PreloadOrCoalesce(uri, url, aPolicyType, as, type, charset, srcset, sizes,
                        integrity, crossOrigin, referrerPolicy);

  if (!result.mPreloader) {
    NotifyNodeEvent(aLinkElement, result.mAlreadyComplete);
    return nullptr;
  }

  result.mPreloader->AddLinkPreloadNode(aLinkElement);
  return result.mPreloader.forget();
}

void PreloadService::PreloadLinkHeader(
    nsIURI* aURI, const nsAString& aURL, nsContentPolicyType aPolicyType,
    const nsAString& aAs, const nsAString& aType, const nsAString& aIntegrity,
    const nsAString& aSrcset, const nsAString& aSizes, const nsAString& aCORS,
    const nsAString& aReferrerPolicy, nsIReferrerInfo* aReferrerInfo) {
  if (!StaticPrefs::network_preload()) {
    return;
  }

  if (!CheckReferrerURIScheme(aReferrerInfo)) {
    return;
  }

  if (aPolicyType == nsIContentPolicy::TYPE_INVALID) {
    return;
  }

  PreloadOrCoalesce(aURI, aURL, aPolicyType, aAs, aType, EmptyString(), aSrcset,
                    aSizes, aIntegrity, aCORS, aReferrerPolicy);
}

PreloadService::PreloadOrCoalesceResult PreloadService::PreloadOrCoalesce(
    nsIURI* aURI, const nsAString& aURL, nsContentPolicyType aPolicyType,
    const nsAString& aAs, const nsAString& aType, const nsAString& aCharset,
    const nsAString& aSrcset, const nsAString& aSizes,
    const nsAString& aIntegrity, const nsAString& aCORS,
    const nsAString& aReferrerPolicy) {
  bool isImgSet = false;
  PreloadHashKey preloadKey;
  nsCOMPtr<nsIURI> uri = aURI;

  if (aAs.LowerCaseEqualsASCII("script")) {
    preloadKey = PreloadHashKey::CreateAsScript(uri, aCORS, aType);
  } else if (aAs.LowerCaseEqualsASCII("style")) {
    preloadKey = PreloadHashKey::CreateAsStyle(
        uri, mDocument->NodePrincipal(), dom::Element::StringToCORSMode(aCORS),
        css::eAuthorSheetFeatures /* see Loader::LoadSheet */);
  } else if (aAs.LowerCaseEqualsASCII("image")) {
    uri = mDocument->ResolvePreloadImage(BaseURIForPreload(), aURL, aSrcset,
                                         aSizes, &isImgSet);
    if (!uri) {
      return {nullptr, false};
    }

    preloadKey = PreloadHashKey::CreateAsImage(
        uri, mDocument->NodePrincipal(), dom::Element::StringToCORSMode(aCORS));
  } else if (aAs.LowerCaseEqualsASCII("font")) {
    preloadKey = PreloadHashKey::CreateAsFont(
        uri, dom::Element::StringToCORSMode(aCORS));
  } else if (aAs.LowerCaseEqualsASCII("fetch")) {
    preloadKey = PreloadHashKey::CreateAsFetch(
        uri, dom::Element::StringToCORSMode(aCORS));
  } else {
    return {nullptr, false};
  }

  if (RefPtr<PreloaderBase> preload = LookupPreload(preloadKey)) {
    return {std::move(preload), false};
  }

  if (aAs.LowerCaseEqualsASCII("script")) {
    PreloadScript(uri, aType, aCharset, aCORS, aReferrerPolicy, aIntegrity,
                  true /* isInHead - TODO */);
  } else if (aAs.LowerCaseEqualsASCII("style")) {
    switch (PreloadStyle(uri, aCharset, aCORS, aReferrerPolicy, aIntegrity)) {
      case dom::SheetPreloadStatus::AlreadyComplete:
        return {nullptr, /* already_complete = */ true};
      case dom::SheetPreloadStatus::Errored:
      case dom::SheetPreloadStatus::InProgress:
        break;
    }
  } else if (aAs.LowerCaseEqualsASCII("image")) {
    PreloadImage(uri, aCORS, aReferrerPolicy, isImgSet);
  } else if (aAs.LowerCaseEqualsASCII("font")) {
    PreloadFont(uri, aCORS, aReferrerPolicy);
  } else if (aAs.LowerCaseEqualsASCII("fetch")) {
    PreloadFetch(uri, aCORS, aReferrerPolicy);
  }

  return {LookupPreload(preloadKey), false};
}

void PreloadService::PreloadScript(nsIURI* aURI, const nsAString& aType,
                                   const nsAString& aCharset,
                                   const nsAString& aCrossOrigin,
                                   const nsAString& aReferrerPolicy,
                                   const nsAString& aIntegrity,
                                   bool aScriptFromHead) {
  mDocument->ScriptLoader()->PreloadURI(
      aURI, aCharset, aType, aCrossOrigin, aIntegrity, aScriptFromHead, false,
      false, false, true, PreloadReferrerPolicy(aReferrerPolicy));
}

dom::SheetPreloadStatus PreloadService::PreloadStyle(
    nsIURI* aURI, const nsAString& aCharset, const nsAString& aCrossOrigin,
    const nsAString& aReferrerPolicy, const nsAString& aIntegrity) {
  return mDocument->PreloadStyle(
      aURI, Encoding::ForLabel(aCharset), aCrossOrigin,
      PreloadReferrerPolicy(aReferrerPolicy), aIntegrity, true);
}

void PreloadService::PreloadImage(nsIURI* aURI, const nsAString& aCrossOrigin,
                                  const nsAString& aImageReferrerPolicy,
                                  bool aIsImgSet) {
  mDocument->PreLoadImage(aURI, aCrossOrigin,
                          PreloadReferrerPolicy(aImageReferrerPolicy),
                          aIsImgSet, true);
}

void PreloadService::PreloadFont(nsIURI* aURI, const nsAString& aCrossOrigin,
                                 const nsAString& aReferrerPolicy) {
  CORSMode cors = dom::Element::StringToCORSMode(aCrossOrigin);
  auto key = PreloadHashKey::CreateAsFont(aURI, cors);

  // * Bug 1618549: Depending on where we decide to do the deduplication, we may
  // want to check if the font is already being preloaded here.

  RefPtr<FontPreloader> preloader = new FontPreloader();
  dom::ReferrerPolicy referrerPolicy = PreloadReferrerPolicy(aReferrerPolicy);
  preloader->OpenChannel(key, aURI, cors, referrerPolicy, mDocument);
}

void PreloadService::PreloadFetch(nsIURI* aURI, const nsAString& aCrossOrigin,
                                  const nsAString& aReferrerPolicy) {
  CORSMode cors = dom::Element::StringToCORSMode(aCrossOrigin);
  auto key = PreloadHashKey::CreateAsFetch(aURI, cors);

  // * Bug 1618549: Depending on where we decide to do the deduplication, we may
  // want to check if a fetch is already being preloaded here.

  RefPtr<FetchPreloader> preloader = new FetchPreloader();
  dom::ReferrerPolicy referrerPolicy = PreloadReferrerPolicy(aReferrerPolicy);
  preloader->OpenChannel(key, aURI, cors, referrerPolicy, mDocument);
}

// static
void PreloadService::NotifyNodeEvent(nsINode* aNode, bool aSuccess) {
  if (!aNode->IsInComposedDoc()) {
    return;
  }

  // We don't dispatch synchronously since |node| might be in a DocGroup
  // that we're not allowed to touch. (Our network request happens in the
  // DocGroup of one of the mSources nodes--not necessarily this one).

  RefPtr<AsyncEventDispatcher> dispatcher = new AsyncEventDispatcher(
      aNode, aSuccess ? u"load"_ns : u"error"_ns, CanBubble::eNo);

  dispatcher->RequireNodeInDocument();
  dispatcher->PostDOMEvent();
}

dom::ReferrerPolicy PreloadService::PreloadReferrerPolicy(
    const nsAString& aReferrerPolicy) {
  dom::ReferrerPolicy referrerPolicy =
      dom::ReferrerInfo::ReferrerPolicyAttributeFromString(aReferrerPolicy);
  if (referrerPolicy == dom::ReferrerPolicy::_empty) {
    referrerPolicy = mDocument->GetPreloadReferrerInfo()->ReferrerPolicy();
  }

  return referrerPolicy;
}

bool PreloadService::CheckReferrerURIScheme(nsIReferrerInfo* aReferrerInfo) {
  if (!aReferrerInfo) {
    return false;
  }

  nsCOMPtr<nsIURI> referrer = aReferrerInfo->GetOriginalReferrer();
  if (!referrer) {
    return false;
  }
  if (!referrer->SchemeIs("http") && !referrer->SchemeIs("https")) {
    return false;
  }

  return true;
}

nsIURI* PreloadService::BaseURIForPreload() {
  nsIURI* documentURI = mDocument->GetDocumentURI();
  nsIURI* documentBaseURI = mDocument->GetDocBaseURI();
  return (documentURI == documentBaseURI)
             ? (mSpeculationBaseURI ? mSpeculationBaseURI.get() : documentURI)
             : documentBaseURI;
}

}  // namespace mozilla
