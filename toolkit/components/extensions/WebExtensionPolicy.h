/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_WebExtensionPolicy_h
#define mozilla_extensions_WebExtensionPolicy_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/WebExtensionPolicyBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/extensions/MatchPattern.h"

#include "jspubtd.h"

#include "mozilla/Result.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {
class Promise;
}  // namespace dom

namespace extensions {

using dom::WebExtensionInit;
using dom::WebExtensionLocalizeCallback;

class DocInfo;
class WebExtensionContentScript;

class WebExtensionPolicy final : public nsISupports,
                                 public nsWrapperCache,
                                 public SupportsWeakPtr<WebExtensionPolicy> {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebExtensionPolicy)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebExtensionPolicy)

  using ScriptArray = nsTArray<RefPtr<WebExtensionContentScript>>;

  static already_AddRefed<WebExtensionPolicy> Constructor(
      dom::GlobalObject& aGlobal, const WebExtensionInit& aInit,
      ErrorResult& aRv);

  nsAtom* Id() const { return mId; }
  void GetId(nsAString& aId) const { aId = nsDependentAtomString(mId); };

  const nsCString& MozExtensionHostname() const { return mHostname; }
  void GetMozExtensionHostname(nsACString& aHostname) const {
    aHostname = MozExtensionHostname();
  }

  void GetBaseURL(nsACString& aBaseURL) const {
    MOZ_ALWAYS_SUCCEEDS(mBaseURI->GetSpec(aBaseURL));
  }

  bool IsPrivileged() { return mIsPrivileged; }

  void GetURL(const nsAString& aPath, nsAString& aURL, ErrorResult& aRv) const;

  Result<nsString, nsresult> GetURL(const nsAString& aPath) const;

  void RegisterContentScript(WebExtensionContentScript& script,
                             ErrorResult& aRv);

  void UnregisterContentScript(const WebExtensionContentScript& script,
                               ErrorResult& aRv);

  void InjectContentScripts(ErrorResult& aRv);

  bool CanAccessURI(const URLInfo& aURI, bool aExplicit = false,
                    bool aCheckRestricted = true,
                    bool aAllowFilePermission = false) const {
    return (!aCheckRestricted || !IsRestrictedURI(aURI)) && mHostPermissions &&
           mHostPermissions->Matches(aURI, aExplicit) &&
           (aURI.Scheme() != nsGkAtoms::file || aAllowFilePermission);
  }

  bool IsPathWebAccessible(const nsAString& aPath) const {
    return mWebAccessiblePaths.Matches(aPath);
  }

  bool HasPermission(const nsAtom* aPermission) const {
    return mPermissions->Contains(aPermission);
  }
  bool HasPermission(const nsAString& aPermission) const {
    return mPermissions->Contains(aPermission);
  }

  static bool IsRestrictedDoc(const DocInfo& aDoc);
  static bool IsRestrictedURI(const URLInfo& aURI);

  nsCString BackgroundPageHTML() const;

  MOZ_CAN_RUN_SCRIPT
  void Localize(const nsAString& aInput, nsString& aResult) const;

  const nsString& Name() const { return mName; }
  void GetName(nsAString& aName) const { aName = mName; }

  const nsString& ExtensionPageCSP() const { return mExtensionPageCSP; }
  void GetExtensionPageCSP(nsAString& aCSP) const { aCSP = mExtensionPageCSP; }

  const nsString& ContentScriptCSP() const { return mContentScriptCSP; }
  void GetContentScriptCSP(nsAString& aCSP) const { aCSP = mContentScriptCSP; }

  already_AddRefed<MatchPatternSet> AllowedOrigins() {
    return do_AddRef(mHostPermissions);
  }
  void SetAllowedOrigins(MatchPatternSet& aAllowedOrigins) {
    mHostPermissions = &aAllowedOrigins;
  }

  void GetPermissions(nsTArray<nsString>& aResult) const {
    mPermissions->Get(aResult);
  }
  void SetPermissions(const nsTArray<nsString>& aPermissions) {
    mPermissions = new AtomSet(aPermissions);
  }

  void GetContentScripts(ScriptArray& aScripts) const;
  const ScriptArray& ContentScripts() const { return mContentScripts; }

  bool Active() const { return mActive; }
  void SetActive(bool aActive, ErrorResult& aRv);

  bool PrivateBrowsingAllowed() const {
    return mAllowPrivateBrowsingByDefault ||
           HasPermission(nsGkAtoms::privateBrowsingAllowedPermission);
  }

  bool CanAccessContext(nsILoadContext* aContext) const;

  bool CanAccessWindow(const dom::WindowProxyHolder& aWindow) const;

  void GetReadyPromise(JSContext* aCx, JS::MutableHandleObject aResult) const;
  dom::Promise* ReadyPromise() const { return mReadyPromise; }

  static void GetActiveExtensions(
      dom::GlobalObject& aGlobal,
      nsTArray<RefPtr<WebExtensionPolicy>>& aResults);

  static already_AddRefed<WebExtensionPolicy> GetByID(
      dom::GlobalObject& aGlobal, const nsAString& aID);

  static already_AddRefed<WebExtensionPolicy> GetByHostname(
      dom::GlobalObject& aGlobal, const nsACString& aHostname);

  static already_AddRefed<WebExtensionPolicy> GetByURI(
      dom::GlobalObject& aGlobal, nsIURI* aURI);

  static bool IsRestrictedURI(dom::GlobalObject& aGlobal, const URLInfo& aURI) {
    return IsRestrictedURI(aURI);
  }

  static bool UseRemoteWebExtensions(dom::GlobalObject& aGlobal);
  static bool IsExtensionProcess(dom::GlobalObject& aGlobal);

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::HandleObject aGivenProto) override;

 protected:
  virtual ~WebExtensionPolicy() = default;

 private:
  WebExtensionPolicy(dom::GlobalObject& aGlobal, const WebExtensionInit& aInit,
                     ErrorResult& aRv);

  bool Enable();
  bool Disable();

  nsCOMPtr<nsISupports> mParent;

  RefPtr<nsAtom> mId;
  nsCString mHostname;
  nsCOMPtr<nsIURI> mBaseURI;

  nsString mName;
  nsString mExtensionPageCSP;
  nsString mContentScriptCSP;

  bool mActive = false;
  bool mAllowPrivateBrowsingByDefault = true;

  RefPtr<WebExtensionLocalizeCallback> mLocalizeCallback;

  bool mIsPrivileged;
  RefPtr<AtomSet> mPermissions;
  RefPtr<MatchPatternSet> mHostPermissions;
  MatchGlobSet mWebAccessiblePaths;

  dom::Nullable<nsTArray<nsString>> mBackgroundScripts;

  nsTArray<RefPtr<WebExtensionContentScript>> mContentScripts;

  RefPtr<dom::Promise> mReadyPromise;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_WebExtensionPolicy_h
