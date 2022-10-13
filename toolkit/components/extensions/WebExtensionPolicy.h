/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_WebExtensionPolicy_h
#define mozilla_extensions_WebExtensionPolicy_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/WebExtensionPolicyBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/extensions/MatchPattern.h"

#include "jspubtd.h"

#include "mozilla/Result.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {
class Promise;
}  // namespace dom

namespace extensions {

using dom::WebAccessibleResourceInit;
using dom::WebExtensionInit;
using dom::WebExtensionLocalizeCallback;

class DocInfo;
class WebExtensionContentScript;

class WebAccessibleResource final : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(WebAccessibleResource)

  WebAccessibleResource(dom::GlobalObject& aGlobal,
                        const WebAccessibleResourceInit& aInit,
                        ErrorResult& aRv);

  bool IsWebAccessiblePath(const nsAString& aPath) const {
    return mWebAccessiblePaths.Matches(aPath);
  }

  bool SourceMayAccessPath(const URLInfo& aURI, const nsAString& aPath) {
    return mWebAccessiblePaths.Matches(aPath) &&
           (IsHostMatch(aURI) || IsExtensionMatch(aURI));
  }

  bool IsHostMatch(const URLInfo& aURI) {
    return mMatches && mMatches->Matches(aURI);
  }

  bool IsExtensionMatch(const URLInfo& aURI);

 protected:
  virtual ~WebAccessibleResource() = default;

 private:
  MatchGlobSet mWebAccessiblePaths;
  RefPtr<MatchPatternSet> mMatches;
  RefPtr<AtomSet> mExtensionIDs;
};

class WebExtensionPolicy final : public nsISupports,
                                 public nsWrapperCache,
                                 public SupportsWeakPtr {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebExtensionPolicy)

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

  bool TemporarilyInstalled() { return mTemporarilyInstalled; }

  void GetURL(const nsAString& aPath, nsAString& aURL, ErrorResult& aRv) const;

  Result<nsString, nsresult> GetURL(const nsAString& aPath) const;

  void RegisterContentScript(WebExtensionContentScript& script,
                             ErrorResult& aRv);

  void UnregisterContentScript(const WebExtensionContentScript& script,
                               ErrorResult& aRv);

  void InjectContentScripts(ErrorResult& aRv);

  bool CanAccessURI(const URLInfo& aURI, bool aExplicit = false,
                    bool aCheckRestricted = true,
                    bool aAllowFilePermission = false) const;

  bool IsWebAccessiblePath(const nsAString& aPath) const {
    for (const auto& resource : mWebAccessibleResources) {
      if (resource->IsWebAccessiblePath(aPath)) {
        return true;
      }
    }
    return false;
  }

  bool SourceMayAccessPath(const URLInfo& aURI, const nsAString& aPath) const;

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

  nsAtom* Type() const { return mType; }
  void GetType(nsAString& aType) const {
    aType = nsDependentAtomString(mType);
  };

  uint32_t ManifestVersion() const { return mManifestVersion; }

  const nsString& ExtensionPageCSP() const { return mExtensionPageCSP; }
  void GetExtensionPageCSP(nsAString& aCSP) const { aCSP = mExtensionPageCSP; }

  const nsString& BaseCSP() const { return mBaseCSP; }
  void GetBaseCSP(nsAString& aCSP) const { aCSP = mBaseCSP; }

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

  bool PrivateBrowsingAllowed() const;

  bool CanAccessContext(nsILoadContext* aContext) const;

  bool CanAccessWindow(const dom::WindowProxyHolder& aWindow) const;

  void GetReadyPromise(JSContext* aCx,
                       JS::MutableHandle<JSObject*> aResult) const;
  dom::Promise* ReadyPromise() const { return mReadyPromise; }

  void GetBackgroundWorker(nsString& aScriptURL) const {
    aScriptURL.Assign(mBackgroundWorkerScript);
  }

  bool IsManifestBackgroundWorker(const nsAString& aWorkerScriptURL) const {
    return mBackgroundWorkerScript.Equals(aWorkerScriptURL);
  }

  uint64_t GetBrowsingContextGroupId() const;
  uint64_t GetBrowsingContextGroupId(ErrorResult& aRv);

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
  static bool BackgroundServiceWorkerEnabled(dom::GlobalObject& aGlobal);

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~WebExtensionPolicy() = default;

 private:
  WebExtensionPolicy(dom::GlobalObject& aGlobal, const WebExtensionInit& aInit,
                     ErrorResult& aRv);

  bool Enable();
  bool Disable();
  void InitializeBaseCSP();

  nsCOMPtr<nsISupports> mParent;

  RefPtr<nsAtom> mId;
  nsCString mHostname;
  nsCOMPtr<nsIURI> mBaseURI;

  nsString mName;
  RefPtr<nsAtom> mType;
  uint32_t mManifestVersion = 2;
  nsString mExtensionPageCSP;
  nsString mBaseCSP;

  dom::BrowsingContextGroup::KeepAlivePtr mBrowsingContextGroup;

  bool mActive = false;

  RefPtr<WebExtensionLocalizeCallback> mLocalizeCallback;

  bool mIsPrivileged;
  bool mTemporarilyInstalled;

  RefPtr<AtomSet> mPermissions;
  RefPtr<MatchPatternSet> mHostPermissions;

  dom::Nullable<nsTArray<nsString>> mBackgroundScripts;
  nsString mBackgroundWorkerScript;

  nsTArray<RefPtr<WebAccessibleResource>> mWebAccessibleResources;
  nsTArray<RefPtr<WebExtensionContentScript>> mContentScripts;

  RefPtr<dom::Promise> mReadyPromise;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_WebExtensionPolicy_h
