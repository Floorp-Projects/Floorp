/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ExtensionPolicyService_h
#define mozilla_ExtensionPolicyService_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "mozIExtensionProcessScript.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsIAddonPolicyService.h"
#include "nsAtom.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsPointerHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsTHashSet.h"
#include "nsAtomHashKeys.h"

class nsIChannel;
class nsIObserverService;

class nsIPIDOMWindowInner;
class nsIPIDOMWindowOuter;

namespace mozilla {
namespace dom {
class Promise;
}  // namespace dom
namespace extensions {
class DocInfo;
class DocumentObserver;
class WebExtensionContentScript;
}  // namespace extensions

using extensions::DocInfo;
using extensions::WebExtensionPolicy;

class ExtensionPolicyService final : public nsIAddonPolicyService,
                                     public nsIObserver,
                                     public nsIMemoryReporter {
 public:
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ExtensionPolicyService,
                                           nsIAddonPolicyService)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIADDONPOLICYSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  static mozIExtensionProcessScript& ProcessScript();

  static ExtensionPolicyService& GetSingleton();

  // Helper for fetching an AtomSet of restricted domains as configured by the
  // extensions.webextensions.restrictedDomains pref. Safe to call from any
  // thread.
  static RefPtr<extensions::AtomSet> RestrictedDomains();

  // Thread-safe AtomSet from extensions.quarantinedDomains.list.
  static RefPtr<extensions::AtomSet> QuarantinedDomains();

  static already_AddRefed<ExtensionPolicyService> GetInstance() {
    return do_AddRef(&GetSingleton());
  }

  // Unlike the other methods on the ExtensionPolicyService, this method is
  // threadsafe, and can look up a WebExtensionPolicyCore by hostname on any
  // thread.
  static RefPtr<extensions::WebExtensionPolicyCore> GetCoreByHost(
      const nsACString& aHost);

  WebExtensionPolicy* GetByID(nsAtom* aAddonId) {
    return mExtensions.GetWeak(aAddonId);
  }

  WebExtensionPolicy* GetByID(const nsAString& aAddonId) {
    RefPtr<nsAtom> atom = NS_AtomizeMainThread(aAddonId);
    return GetByID(atom);
  }

  WebExtensionPolicy* GetByURL(const extensions::URLInfo& aURL);

  WebExtensionPolicy* GetByHost(const nsACString& aHost) const;

  void GetAll(nsTArray<RefPtr<WebExtensionPolicy>>& aResult);

  bool RegisterExtension(WebExtensionPolicy& aPolicy);
  bool UnregisterExtension(WebExtensionPolicy& aPolicy);

  bool RegisterObserver(extensions::DocumentObserver& aPolicy);
  bool UnregisterObserver(extensions::DocumentObserver& aPolicy);

  bool UseRemoteExtensions() const;
  bool IsExtensionProcess() const;
  bool GetQuarantinedDomainsEnabled() const;

  nsresult InjectContentScripts(WebExtensionPolicy* aExtension);

 protected:
  virtual ~ExtensionPolicyService();

 private:
  ExtensionPolicyService();

  void RegisterObservers();
  void UnregisterObservers();

  void CheckRequest(nsIChannel* aChannel);
  void CheckDocument(dom::Document* aDocument);

  void CheckContentScripts(const DocInfo& aDocInfo, bool aIsPreload);

  already_AddRefed<dom::Promise> ExecuteContentScript(
      nsPIDOMWindowInner* aWindow,
      extensions::WebExtensionContentScript& aScript);

  RefPtr<dom::Promise> ExecuteContentScripts(
      JSContext* aCx, nsPIDOMWindowInner* aWindow,
      const nsTArray<RefPtr<extensions::WebExtensionContentScript>>& aScripts);

  void UpdateRestrictedDomains();
  void UpdateQuarantinedDomains();

  // The WebExtensionPolicy object keeps the key alive.
  nsRefPtrHashtable<nsWeakAtomHashKey, WebExtensionPolicy> mExtensions;

  nsRefPtrHashtable<nsPtrHashKey<const extensions::DocumentObserver>,
                    extensions::DocumentObserver>
      mObservers;

  nsCOMPtr<nsIObserverService> mObs;

  nsString mDefaultCSP;
  nsString mDefaultCSPV3;
};

}  // namespace mozilla

#endif  // mozilla_ExtensionPolicyService_h
