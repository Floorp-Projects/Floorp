/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PreloadService_h__
#define PreloadService_h__

#include "nsIContentPolicy.h"
#include "nsIReferrerInfo.h"
#include "nsIURI.h"
#include "nsRefPtrHashtable.h"
#include "PreloaderBase.h"

namespace mozilla {

namespace dom {

class HTMLLinkElement;
class Document;
enum class ReferrerPolicy : uint8_t;

}  // namespace dom

/**
 * Intended to scope preloads and speculative loads under one roof.  This class
 * is intended to be a member of dom::Document. Provides registration of
 * speculative loads via a `key` which is defined to consist of the URL,
 * resource type, and resource-specific attributes that are further
 * distinguishing loads with otherwise same type and url.
 */
class PreloadService {
 public:
  explicit PreloadService(dom::Document* aDocument) : mDocument(aDocument) {}

  // Called by resource loaders to register a running resource load.  This is
  // called for a speculative load when it's started the first time, being it
  // either regular speculative load or a preload.
  //
  // Returns false and does nothing if a preload is already registered under
  // this key, true otherwise.
  bool RegisterPreload(PreloadHashKey* aKey, PreloaderBase* aPreload);

  // Called when the load is about to be cancelled.  Exact behavior is to be
  // determined yet.
  void DeregisterPreload(PreloadHashKey* aKey);

  // Called when the scope is to go away.
  void ClearAllPreloads();

  // True when there is a preload registered under the key.
  bool PreloadExists(PreloadHashKey* aKey);

  // Returns an existing preload under the key or null, when there is none
  // registered under the key.
  already_AddRefed<PreloaderBase> LookupPreload(PreloadHashKey* aKey) const;

  void SetSpeculationBase(nsIURI* aURI) { mSpeculationBaseURI = aURI; }
  already_AddRefed<nsIURI> GetPreloadURI(const nsAString& aURL);

  already_AddRefed<PreloaderBase> PreloadLinkElement(
      dom::HTMLLinkElement* aLinkElement, nsContentPolicyType aPolicyType,
      nsIReferrerInfo* aReferrerInfo);

  void PreloadScript(nsIURI* aURI, const nsAString& aType,
                     const nsAString& aCharset, const nsAString& aCrossOrigin,
                     const nsAString& aReferrerPolicy,
                     const nsAString& aIntegrity, bool aScriptFromHead);

  void PreloadStyle(nsIURI* aURI, const nsAString& aCharset,
                    const nsAString& aCrossOrigin,
                    const nsAString& aReferrerPolicy,
                    const nsAString& aIntegrity);

  void PreloadImage(nsIURI* aURI, const nsAString& aCrossOrigin,
                    const nsAString& aImageReferrerPolicy, bool aIsImgSet);

  void PreloadFont(nsIURI* aURI, const nsAString& aCrossOrigin,
                   const nsAString& aReferrerPolicy);

  void PreloadFetch(nsIURI* aURI, const nsAString& aCrossOrigin,
                    const nsAString& aReferrerPolicy);

  static void NotifyNodeEvent(nsINode* aNode, bool aSuccess);

 private:
  dom::ReferrerPolicy PreloadReferrerPolicy(const nsAString& aReferrerPolicy);
  bool CheckReferrerURIScheme(nsIReferrerInfo* aReferrerInfo);
  nsIURI* BaseURIForPreload();

 private:
  nsRefPtrHashtable<PreloadHashKey, PreloaderBase> mPreloads;

  // Raw pointer only, we are intended to be a direct member of dom::Document
  dom::Document* mDocument;

  // Set by `nsHtml5TreeOpExecutor::SetSpeculationBase`.
  nsCOMPtr<nsIURI> mSpeculationBaseURI;
};

}  // namespace mozilla

#endif
