/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PreloadService_h__
#define PreloadService_h__

#include "nsIContentPolicy.h"
#include "nsIURI.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/PreloadHashKey.h"

class nsINode;

namespace mozilla {

class PreloaderBase;

namespace dom {

class HTMLLinkElement;
class Document;
enum class ReferrerPolicy : uint8_t;

}  // namespace dom

/**
 * Intended to scope preloads
 * (https://developer.mozilla.org/en-US/docs/Web/HTML/Attributes/rel/preload)
 * and speculative loads initiated by the parser under one roof.  This class
 * is intended to be a member of dom::Document. Provides registration of
 * speculative loads via a `key` which is defined to consist of the URL,
 * resource type, and resource-specific attributes that are further
 * distinguishing loads with otherwise same type and url.
 */
class PreloadService {
 public:
  explicit PreloadService(dom::Document*);
  ~PreloadService();

  // Called by resource loaders to register a running resource load.  This is
  // called for a speculative load when it's started the first time, being it
  // either regular speculative load or a preload.
  //
  // Returns false and does nothing if a preload is already registered under
  // this key, true otherwise.
  bool RegisterPreload(const PreloadHashKey& aKey, PreloaderBase* aPreload);

  // Called when the load is about to be cancelled.  Exact behavior is to be
  // determined yet.
  void DeregisterPreload(const PreloadHashKey& aKey);

  // Called when the scope is to go away.
  void ClearAllPreloads();

  // True when there is a preload registered under the key.
  bool PreloadExists(const PreloadHashKey& aKey);

  // Returns an existing preload under the key or null, when there is none
  // registered under the key.
  already_AddRefed<PreloaderBase> LookupPreload(
      const PreloadHashKey& aKey) const;

  void SetSpeculationBase(nsIURI* aURI) { mSpeculationBaseURI = aURI; }
  already_AddRefed<nsIURI> GetPreloadURI(const nsAString& aURL);

  already_AddRefed<PreloaderBase> PreloadLinkElement(
      dom::HTMLLinkElement* aLink, nsContentPolicyType aPolicyType);

  // a non-zero aEarlyHintPreloaderId tells this service that a preload for this
  // link was started by the EarlyHintPreloader and the preloaders should
  // connect back by setting earlyHintPreloaderId in nsIChannelInternal before
  // AsyncOpen.
  void PreloadLinkHeader(nsIURI* aURI, const nsAString& aURL,
                         nsContentPolicyType aPolicyType, const nsAString& aAs,
                         const nsAString& aType, const nsAString& aNonce,
                         const nsAString& aIntegrity, const nsAString& aSrcset,
                         const nsAString& aSizes, const nsAString& aCORS,
                         const nsAString& aReferrerPolicy,
                         uint64_t aEarlyHintPreloaderId,
                         const nsAString& aFetchPriority);

  void PreloadScript(nsIURI* aURI, const nsAString& aType,
                     const nsAString& aCharset, const nsAString& aCrossOrigin,
                     const nsAString& aReferrerPolicy, const nsAString& aNonce,
                     const nsAString& aFetchPriority,
                     const nsAString& aIntegrity, bool aScriptFromHead,
                     uint64_t aEarlyHintPreloaderId);

  void PreloadImage(nsIURI* aURI, const nsAString& aCrossOrigin,
                    const nsAString& aImageReferrerPolicy, bool aIsImgSet,
                    uint64_t aEarlyHintPreloaderId);

  void PreloadFont(nsIURI* aURI, const nsAString& aCrossOrigin,
                   const nsAString& aReferrerPolicy,
                   uint64_t aEarlyHintPreloaderId,
                   const nsAString& aFetchPriority);

  void PreloadFetch(nsIURI* aURI, const nsAString& aCrossOrigin,
                    const nsAString& aReferrerPolicy,
                    uint64_t aEarlyHintPreloaderId,
                    const nsAString& aFetchPriority);

  static void NotifyNodeEvent(nsINode* aNode, bool aSuccess);

  void SetEarlyHintUsed() { mEarlyHintUsed = true; }
  bool GetEarlyHintUsed() const { return mEarlyHintUsed; }

 private:
  dom::ReferrerPolicy PreloadReferrerPolicy(const nsAString& aReferrerPolicy);
  nsIURI* BaseURIForPreload();

  struct PreloadOrCoalesceResult {
    RefPtr<PreloaderBase> mPreloader;
    bool mAlreadyComplete;
  };

  PreloadOrCoalesceResult PreloadOrCoalesce(
      nsIURI* aURI, const nsAString& aURL, nsContentPolicyType aPolicyType,
      const nsAString& aAs, const nsAString& aType, const nsAString& aCharset,
      const nsAString& aSrcset, const nsAString& aSizes,
      const nsAString& aNonce, const nsAString& aIntegrity,
      const nsAString& aCORS, const nsAString& aReferrerPolicy,
      const nsAString& aFetchPriority, bool aFromHeader,
      uint64_t aEarlyHintPreloaderId);

 private:
  nsRefPtrHashtable<PreloadHashKey, PreloaderBase> mPreloads;

  // Raw pointer only, we are intended to be a direct member of dom::Document
  dom::Document* mDocument;

  // Set by `nsHtml5TreeOpExecutor::SetSpeculationBase`.
  nsCOMPtr<nsIURI> mSpeculationBaseURI;

  bool mEarlyHintUsed = false;
};

}  // namespace mozilla

#endif
