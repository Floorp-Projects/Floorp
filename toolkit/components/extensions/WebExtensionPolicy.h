/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_WebExtensionPolicy_h
#define mozilla_extensions_WebExtensionPolicy_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/WebExtensionPolicyBinding.h"
#include "mozilla/extensions/MatchPattern.h"

#include "jspubtd.h"

#include "mozilla/Result.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace extensions {

using dom::WebExtensionInit;
using dom::WebExtensionLocalizeCallback;

class WebExtensionPolicy final : public nsISupports
                               , public nsWrapperCache
                               , public SupportsWeakPtr<WebExtensionPolicy>
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebExtensionPolicy)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebExtensionPolicy)

  static already_AddRefed<WebExtensionPolicy>
  Constructor(dom::GlobalObject& aGlobal, const WebExtensionInit& aInit, ErrorResult& aRv);

  nsIAtom* Id() const { return mId; }
  void GetId(nsAString& aId) const { aId = nsDependentAtomString(mId); };

  const nsCString& MozExtensionHostname() const { return mHostname; }
  void GetMozExtensionHostname(nsACString& aHostname) const
  {
    aHostname = MozExtensionHostname();
  }

  void GetBaseURL(nsACString& aBaseURL) const
  {
    MOZ_ALWAYS_SUCCEEDS(mBaseURI->GetSpec(aBaseURL));
  }

  void GetURL(const nsAString& aPath, nsAString& aURL, ErrorResult& aRv) const;

  Result<nsString, nsresult> GetURL(const nsAString& aPath) const;

  bool CanAccessURI(nsIURI* aURI, bool aExplicit = false) const
  {
    return mHostPermissions && mHostPermissions->Matches(aURI, aExplicit);
  }

  bool IsPathWebAccessible(const nsAString& aPath) const
  {
    return mWebAccessiblePaths.Matches(aPath);
  }

  bool HasPermission(const nsIAtom* aPermission) const
  {
    return mPermissions->Contains(aPermission);
  }
  bool HasPermission(const nsAString& aPermission) const
  {
    return mPermissions->Contains(aPermission);
  }

  nsCString BackgroundPageHTML() const;

  void Localize(const nsAString& aInput, nsString& aResult) const;

  const nsString& ContentSecurityPolicy() const
  {
    return mContentSecurityPolicy;
  }
  void GetContentSecurityPolicy(nsAString& aCSP) const
  {
    aCSP = mContentSecurityPolicy;
  }


  already_AddRefed<MatchPatternSet> AllowedOrigins()
  {
    return do_AddRef(mHostPermissions);
  }
  void SetAllowedOrigins(MatchPatternSet& aAllowedOrigins)
  {
    mHostPermissions = &aAllowedOrigins;
  }

  void GetPermissions(nsTArray<nsString>& aResult) const
  {
    mPermissions->Get(aResult);
  }
  void SetPermissions(const nsTArray<nsString>& aPermissions)
  {
    mPermissions = new AtomSet(aPermissions);
  }


  bool Active() const { return mActive; }
  void SetActive(bool aActive, ErrorResult& aRv);


  static void
  GetActiveExtensions(dom::GlobalObject& aGlobal, nsTArray<RefPtr<WebExtensionPolicy>>& aResults);

  static already_AddRefed<WebExtensionPolicy>
  GetByID(dom::GlobalObject& aGlobal, const nsAString& aID);

  static already_AddRefed<WebExtensionPolicy>
  GetByHostname(dom::GlobalObject& aGlobal, const nsACString& aHostname);

  static already_AddRefed<WebExtensionPolicy>
  GetByURI(dom::GlobalObject& aGlobal, nsIURI* aURI);


  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) override;

protected:
  virtual ~WebExtensionPolicy() = default;

private:
  WebExtensionPolicy(dom::GlobalObject& aGlobal, const WebExtensionInit& aInit, ErrorResult& aRv);

  bool Enable();
  bool Disable();

  nsCOMPtr<nsISupports> mParent;

  nsCOMPtr<nsIAtom> mId;
  nsCString mHostname;
  nsCOMPtr<nsIURI> mBaseURI;

  nsString mContentSecurityPolicy;

  bool mActive = false;

  RefPtr<WebExtensionLocalizeCallback> mLocalizeCallback;

  RefPtr<AtomSet> mPermissions;
  RefPtr<MatchPatternSet> mHostPermissions;
  MatchGlobSet mWebAccessiblePaths;

  Nullable<nsTArray<nsString>> mBackgroundScripts;
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_WebExtensionPolicy_h
