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
#include "nsIAtom.h"
#include "nsISupports.h"
#include "nsPointerHashKeys.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {

using extensions::WebExtensionPolicy;

class ExtensionPolicyService final : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(ExtensionPolicyService)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  static ExtensionPolicyService& GetSingleton();

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

protected:
  virtual ~ExtensionPolicyService() = default;

private:
  ExtensionPolicyService() = default;

  nsRefPtrHashtable<nsPtrHashKey<const nsIAtom>, WebExtensionPolicy> mExtensions;
  nsRefPtrHashtable<nsCStringHashKey, WebExtensionPolicy> mExtensionHosts;
};

} // namespace mozilla

#endif // mozilla_ExtensionPolicyService_h
