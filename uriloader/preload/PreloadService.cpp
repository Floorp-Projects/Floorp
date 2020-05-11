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

bool PreloadService::RegisterPreload(PreloadHashKey* aKey,
                                     PreloaderBase* aPreload) {
  if (PreloadExists(aKey)) {
    return false;
  }

  mPreloads.Put(aKey, RefPtr{aPreload});
  return true;
}

void PreloadService::DeregisterPreload(PreloadHashKey* aKey) {
  mPreloads.Remove(aKey);
}

void PreloadService::ClearAllPreloads() { mPreloads.Clear(); }

bool PreloadService::PreloadExists(PreloadHashKey* aKey) {
  bool found;
  mPreloads.GetWeak(aKey, &found);
  return found;
}

already_AddRefed<PreloaderBase> PreloadService::LookupPreload(
    PreloadHashKey* aKey) const {
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

  nsCOMPtr<nsIURI> uri = aLinkElement->GetURI();

  nsAutoString as, charset, crossOrigin, integrity, referrerPolicyAttr, srcset,
      sizes, type;
  aLinkElement->GetAs(as);
  aLinkElement->GetCharset(charset);
  aLinkElement->GetCrossOrigin(crossOrigin);
  aLinkElement->GetIntegrity(integrity);
  aLinkElement->GetReferrerPolicy(referrerPolicyAttr);
  auto referrerPolicy = PreloadReferrerPolicy(referrerPolicyAttr);

  bool isImgSet = false;
  PreloadHashKey preloadKey;

  if (as.LowerCaseEqualsASCII("script")) {
    dom::DOMString domType;
    aLinkElement->GetType(domType);
    domType.ToString(type);
    preloadKey =
        PreloadHashKey::CreateAsScript(uri, crossOrigin, type, referrerPolicy);
  } else if (as.LowerCaseEqualsASCII("style")) {
    nsCOMPtr<nsIReferrerInfo> referrerInfo =
        dom::ReferrerInfo::CreateFromDocumentAndPolicyOverride(mDocument,
                                                               referrerPolicy);
    preloadKey = PreloadHashKey::CreateAsStyle(
        uri, mDocument->NodePrincipal(), referrerInfo,
        dom::Element::StringToCORSMode(crossOrigin),
        css::eAuthorSheetFeatures /* see Loader::LoadSheet */);
  } else if (as.LowerCaseEqualsASCII("image")) {
    aLinkElement->GetImageSrcset(srcset);
    aLinkElement->GetImageSizes(sizes);
    nsAutoString url;
    aLinkElement->GetHref(url);
    uri = mDocument->ResolvePreloadImage(BaseURIForPreload(), url, srcset,
                                         sizes, &isImgSet);
    if (!uri) {
      NotifyNodeEvent(aLinkElement, false);
      return nullptr;
    }

    preloadKey = PreloadHashKey::CreateAsImage(
        uri, mDocument->NodePrincipal(),
        dom::Element::StringToCORSMode(crossOrigin), referrerPolicy);
  } else if (as.LowerCaseEqualsASCII("font")) {
    preloadKey = PreloadHashKey::CreateAsFont(
        uri, dom::Element::StringToCORSMode(crossOrigin), referrerPolicy);
  } else if (as.LowerCaseEqualsASCII("fetch")) {
    preloadKey = PreloadHashKey::CreateAsFetch(
        uri, dom::Element::StringToCORSMode(crossOrigin), referrerPolicy);
  } else {
    NotifyNodeEvent(aLinkElement, false);
    return nullptr;
  }

  RefPtr<PreloaderBase> preload = LookupPreload(&preloadKey);
  if (!preload) {
    if (as.LowerCaseEqualsASCII("script")) {
      PreloadScript(uri, type, charset, crossOrigin, referrerPolicyAttr,
                    integrity, true /* isInHead - TODO */);
    } else if (as.LowerCaseEqualsASCII("style")) {
      PreloadStyle(uri, charset, crossOrigin, referrerPolicyAttr, integrity);
    } else if (as.LowerCaseEqualsASCII("image")) {
      PreloadImage(uri, crossOrigin, referrerPolicyAttr, isImgSet);
    } else if (as.LowerCaseEqualsASCII("font")) {
      PreloadFont(uri, crossOrigin, referrerPolicyAttr);
    } else if (as.LowerCaseEqualsASCII("fetch")) {
      PreloadFetch(uri, crossOrigin, referrerPolicyAttr);
    }

    preload = LookupPreload(&preloadKey);
    if (!preload) {
      NotifyNodeEvent(aLinkElement, false);
      return nullptr;
    }
  }

  preload->AddLinkPreloadNode(aLinkElement);

  return preload.forget();
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

void PreloadService::PreloadStyle(nsIURI* aURI, const nsAString& aCharset,
                                  const nsAString& aCrossOrigin,
                                  const nsAString& aReferrerPolicy,
                                  const nsAString& aIntegrity) {
  mDocument->PreloadStyle(aURI, Encoding::ForLabel(aCharset), aCrossOrigin,
                          PreloadReferrerPolicy(aReferrerPolicy), aIntegrity,
                          true);
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
  dom::ReferrerPolicy referrerPolicy = PreloadReferrerPolicy(aReferrerPolicy);
  auto key = PreloadHashKey::CreateAsFont(aURI, cors, referrerPolicy);

  // * Bug 1618549: Depending on where we decide to do the deduplication, we may
  // want to check if the font is already being preloaded here.

  RefPtr<FontPreloader> preloader = new FontPreloader();
  preloader->OpenChannel(&key, aURI, cors, referrerPolicy, mDocument);
}

void PreloadService::PreloadFetch(nsIURI* aURI, const nsAString& aCrossOrigin,
                                  const nsAString& aReferrerPolicy) {
  CORSMode cors = dom::Element::StringToCORSMode(aCrossOrigin);
  dom::ReferrerPolicy referrerPolicy = PreloadReferrerPolicy(aReferrerPolicy);
  auto key = PreloadHashKey::CreateAsFetch(aURI, cors, referrerPolicy);

  // * Bug 1618549: Depending on where we decide to do the deduplication, we may
  // want to check if a fetch is already being preloaded here.

  RefPtr<FetchPreloader> preloader = new FetchPreloader();
  preloader->OpenChannel(&key, aURI, cors, referrerPolicy, mDocument);
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
      aNode, aSuccess ? NS_LITERAL_STRING("load") : NS_LITERAL_STRING("error"),
      CanBubble::eNo);

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
