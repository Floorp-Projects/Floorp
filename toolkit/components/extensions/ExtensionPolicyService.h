/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ExtensionPolicyService_h
#define mozilla_ExtensionPolicyService_h

#include "mozilla/extensions/WebExtensionPolicy.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsIAddonPolicyService.h"
#include "nsIAtom.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsPointerHashKeys.h"
#include "nsRefPtrHashtable.h"

class nsIChannel;
class nsIObserverService;
class nsIDocument;
class nsIPIDOMWindowOuter;

namespace mozilla {
namespace extensions {
  class DocInfo;
}

using extensions::DocInfo;
using extensions::WebExtensionPolicy;

class ExtensionPolicyService final : public nsIAddonPolicyService
                                   , public nsIObserver
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ExtensionPolicyService,
                                           nsIAddonPolicyService)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIADDONPOLICYSERVICE
  NS_DECL_NSIOBSERVER

  static ExtensionPolicyService& GetSingleton();

  static already_AddRefed<ExtensionPolicyService> GetInstance()
  {
    return do_AddRef(&GetSingleton());
  }

  WebExtensionPolicy*
  GetByID(const nsIAtom* aAddonId)
  {
    return mExtensions.GetWeak(aAddonId);
  }

  WebExtensionPolicy* GetByID(const nsAString& aAddonId)
  {
    nsCOMPtr<nsIAtom> atom = NS_AtomizeMainThread(aAddonId);
    return GetByID(atom);
  }

  WebExtensionPolicy* GetByURL(const extensions::URLInfo& aURL);

  WebExtensionPolicy* GetByHost(const nsACString& aHost) const
  {
    return mExtensionHosts.GetWeak(aHost);
  }

  void GetAll(nsTArray<RefPtr<WebExtensionPolicy>>& aResult);

  bool RegisterExtension(WebExtensionPolicy& aPolicy);
  bool UnregisterExtension(WebExtensionPolicy& aPolicy);

  void BaseCSP(nsAString& aDefaultCSP) const;
  void DefaultCSP(nsAString& aDefaultCSP) const;

  bool IsExtensionProcess() const;

protected:
  virtual ~ExtensionPolicyService() = default;

private:
  ExtensionPolicyService();

  void RegisterObservers();
  void UnregisterObservers();

  void CheckRequest(nsIChannel* aChannel);
  void CheckDocument(nsIDocument* aDocument);
  void CheckWindow(nsPIDOMWindowOuter* aWindow);

  void CheckContentScripts(const DocInfo& aDocInfo, bool aIsPreload);

  nsRefPtrHashtable<nsPtrHashKey<const nsIAtom>, WebExtensionPolicy> mExtensions;
  nsRefPtrHashtable<nsCStringHashKey, WebExtensionPolicy> mExtensionHosts;

  nsCOMPtr<nsIObserverService> mObs;

  static bool sRemoteExtensions;
};

} // namespace mozilla

#endif // mozilla_ExtensionPolicyService_h
