/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/extensions/DocumentObserver.h"
#include "mozilla/extensions/WebExtensionContentScript.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

#include "mozilla/AddonManagerWebAPI.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/Try.h"
#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsGlobalWindowInner.h"
#include "nsIObserver.h"
#include "nsISubstitutingProtocolHandler.h"
#include "nsLiteralString.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace extensions {

using namespace dom;

static const char kProto[] = "moz-extension";

static const char kBackgroundScriptTypeDefault[] = "text/javascript";

static const char kBackgroundScriptTypeModule[] = "module";

static const char kBackgroundPageHTMLStart[] =
    "<!DOCTYPE html>\n\
<html>\n\
  <head><meta charset=\"utf-8\"></head>\n\
  <body>";

static const char kBackgroundPageHTMLScript[] =
    "\n\
    <script type=\"%s\" src=\"%s\"></script>";

static const char kBackgroundPageHTMLEnd[] =
    "\n\
  </body>\n\
</html>";

#define BASE_CSP_PREF_V2 "extensions.webextensions.base-content-security-policy"
#define DEFAULT_BASE_CSP_V2                                            \
  "script-src 'self' https://* http://localhost:* http://127.0.0.1:* " \
  "moz-extension: blob: filesystem: 'unsafe-eval' 'wasm-unsafe-eval' " \
  "'unsafe-inline';"

#define BASE_CSP_PREF_V3 \
  "extensions.webextensions.base-content-security-policy.v3"
#define DEFAULT_BASE_CSP_V3 "script-src 'self' 'wasm-unsafe-eval';"

static inline ExtensionPolicyService& EPS() {
  return ExtensionPolicyService::GetSingleton();
}

static nsISubstitutingProtocolHandler* Proto() {
  static nsCOMPtr<nsISubstitutingProtocolHandler> sHandler;

  if (MOZ_UNLIKELY(!sHandler)) {
    nsCOMPtr<nsIIOService> ios = do_GetIOService();
    MOZ_RELEASE_ASSERT(ios);

    nsCOMPtr<nsIProtocolHandler> handler;
    ios->GetProtocolHandler(kProto, getter_AddRefs(handler));

    sHandler = do_QueryInterface(handler);
    MOZ_RELEASE_ASSERT(sHandler);

    ClearOnShutdown(&sHandler);
  }

  return sHandler;
}

bool ParseGlobs(GlobalObject& aGlobal,
                Sequence<OwningMatchGlobOrUTF8String> aGlobs,
                nsTArray<RefPtr<MatchGlobCore>>& aResult, ErrorResult& aRv) {
  for (auto& elem : aGlobs) {
    if (elem.IsMatchGlob()) {
      aResult.AppendElement(elem.GetAsMatchGlob()->Core());
    } else {
      RefPtr<MatchGlobCore> glob =
          new MatchGlobCore(elem.GetAsUTF8String(), true, false, aRv);
      if (aRv.Failed()) {
        return false;
      }
      aResult.AppendElement(glob);
    }
  }
  return true;
}

enum class ErrorBehavior {
  CreateEmptyPattern,
  Fail,
};

already_AddRefed<MatchPatternSet> ParseMatches(
    GlobalObject& aGlobal,
    const OwningMatchPatternSetOrStringSequence& aMatches,
    const MatchPatternOptions& aOptions, ErrorBehavior aErrorBehavior,
    ErrorResult& aRv) {
  if (aMatches.IsMatchPatternSet()) {
    return do_AddRef(aMatches.GetAsMatchPatternSet().get());
  }

  const auto& strings = aMatches.GetAsStringSequence();

  nsTArray<OwningStringOrMatchPattern> patterns;
  if (!patterns.SetCapacity(strings.Length(), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  for (auto& string : strings) {
    OwningStringOrMatchPattern elt;
    elt.SetAsString() = string;
    patterns.AppendElement(elt);
  }

  RefPtr<MatchPatternSet> result =
      MatchPatternSet::Constructor(aGlobal, patterns, aOptions, aRv);

  if (aRv.Failed() && aErrorBehavior == ErrorBehavior::CreateEmptyPattern) {
    aRv.SuppressException();
    result = MatchPatternSet::Constructor(aGlobal, {}, aOptions, aRv);
  }

  return result.forget();
}

WebAccessibleResource::WebAccessibleResource(
    GlobalObject& aGlobal, const WebAccessibleResourceInit& aInit,
    ErrorResult& aRv) {
  ParseGlobs(aGlobal, aInit.mResources, mWebAccessiblePaths, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (!aInit.mMatches.IsNull()) {
    MatchPatternOptions options;
    options.mRestrictSchemes = true;
    RefPtr<MatchPatternSet> matches =
        ParseMatches(aGlobal, aInit.mMatches.Value(), options,
                     ErrorBehavior::CreateEmptyPattern, aRv);
    MOZ_DIAGNOSTIC_ASSERT(!aRv.Failed());
    mMatches = matches->Core();
  }

  if (!aInit.mExtension_ids.IsNull()) {
    mExtensionIDs = new AtomSet(aInit.mExtension_ids.Value());
  }
}

bool WebAccessibleResource::IsExtensionMatch(const URLInfo& aURI) {
  if (!mExtensionIDs) {
    return false;
  }
  RefPtr<WebExtensionPolicyCore> policy =
      ExtensionPolicyService::GetCoreByHost(aURI.Host());
  return policy && (mExtensionIDs->Contains(nsGkAtoms::_asterisk) ||
                    mExtensionIDs->Contains(policy->Id()));
}

/*****************************************************************************
 * WebExtensionPolicyCore
 *****************************************************************************/

WebExtensionPolicyCore::WebExtensionPolicyCore(GlobalObject& aGlobal,
                                               WebExtensionPolicy* aPolicy,
                                               const WebExtensionInit& aInit,
                                               ErrorResult& aRv)
    : mPolicy(aPolicy),
      mId(NS_AtomizeMainThread(aInit.mId)),
      mName(aInit.mName),
      mType(NS_AtomizeMainThread(aInit.mType)),
      mManifestVersion(aInit.mManifestVersion),
      mExtensionPageCSP(aInit.mExtensionPageCSP),
      mIsPrivileged(aInit.mIsPrivileged),
      mTemporarilyInstalled(aInit.mTemporarilyInstalled),
      mBackgroundWorkerScript(aInit.mBackgroundWorkerScript),
      mIgnoreQuarantine(aInit.mIsPrivileged || aInit.mIgnoreQuarantine),
      mPermissions(new AtomSet(aInit.mPermissions)) {
  // In practice this is not necessary, but in tests where the uuid
  // passed in is not lowercased various tests can fail.
  ToLowerCase(aInit.mMozExtensionHostname, mHostname);

  // Initialize the base CSP and extension page CSP
  if (mManifestVersion < 3) {
    nsresult rv = Preferences::GetString(BASE_CSP_PREF_V2, mBaseCSP);
    if (NS_FAILED(rv)) {
      mBaseCSP = NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_BASE_CSP_V2);
    }
  } else {
    nsresult rv = Preferences::GetString(BASE_CSP_PREF_V3, mBaseCSP);
    if (NS_FAILED(rv)) {
      mBaseCSP = NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_BASE_CSP_V3);
    }
  }

  if (mExtensionPageCSP.IsVoid()) {
    if (mManifestVersion < 3) {
      EPS().GetDefaultCSP(mExtensionPageCSP);
    } else {
      EPS().GetDefaultCSPV3(mExtensionPageCSP);
    }
  }

  mWebAccessibleResources.SetCapacity(aInit.mWebAccessibleResources.Length());
  for (const auto& resourceInit : aInit.mWebAccessibleResources) {
    RefPtr<WebAccessibleResource> resource =
        new WebAccessibleResource(aGlobal, resourceInit, aRv);
    if (aRv.Failed()) {
      return;
    }
    mWebAccessibleResources.AppendElement(std::move(resource));
  }

  nsresult rv = NS_NewURI(getter_AddRefs(mBaseURI), aInit.mBaseURL);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

bool WebExtensionPolicyCore::SourceMayAccessPath(
    const URLInfo& aURI, const nsACString& aPath) const {
  if (aURI.Scheme() == nsGkAtoms::moz_extension &&
      MozExtensionHostname().Equals(aURI.Host())) {
    // An extension can always access it's own paths.
    return true;
  }
  // Bug 1786564 Static themes need to allow access to theme resources.
  if (Type() == nsGkAtoms::theme) {
    RefPtr<WebExtensionPolicyCore> policyCore =
        ExtensionPolicyService::GetCoreByHost(aURI.Host());
    return policyCore != nullptr;
  }

  if (ManifestVersion() < 3) {
    return IsWebAccessiblePath(aPath);
  }
  for (const auto& resource : mWebAccessibleResources) {
    if (resource->SourceMayAccessPath(aURI, aPath)) {
      return true;
    }
  }
  return false;
}

bool WebExtensionPolicyCore::CanAccessURI(const URLInfo& aURI, bool aExplicit,
                                          bool aCheckRestricted,
                                          bool aAllowFilePermission) const {
  if (aCheckRestricted && WebExtensionPolicy::IsRestrictedURI(aURI)) {
    return false;
  }
  if (aCheckRestricted && QuarantinedFromURI(aURI)) {
    return false;
  }
  if (!aAllowFilePermission && aURI.Scheme() == nsGkAtoms::file) {
    return false;
  }

  AutoReadLock lock(mLock);
  return mHostPermissions && mHostPermissions->Matches(aURI, aExplicit);
}

bool WebExtensionPolicyCore::QuarantinedFromDoc(const DocInfo& aDoc) const {
  return QuarantinedFromURI(aDoc.PrincipalURL());
}

bool WebExtensionPolicyCore::QuarantinedFromURI(const URLInfo& aURI) const {
  return !IgnoreQuarantine() && WebExtensionPolicy::IsQuarantinedURI(aURI);
}

/*****************************************************************************
 * WebExtensionPolicy
 *****************************************************************************/

WebExtensionPolicy::WebExtensionPolicy(GlobalObject& aGlobal,
                                       const WebExtensionInit& aInit,
                                       ErrorResult& aRv)
    : mCore(new WebExtensionPolicyCore(aGlobal, this, aInit, aRv)),
      mLocalizeCallback(aInit.mLocalizeCallback) {
  if (aRv.Failed()) {
    return;
  }

  MatchPatternOptions options;
  options.mRestrictSchemes = !HasPermission(nsGkAtoms::mozillaAddons);

  // Set host permissions with SetAllowedOrigins to make sure the copy in core
  // and WebExtensionPolicy stay in sync.
  RefPtr<MatchPatternSet> hostPermissions =
      ParseMatches(aGlobal, aInit.mAllowedOrigins, options,
                   ErrorBehavior::CreateEmptyPattern, aRv);
  if (aRv.Failed()) {
    return;
  }
  SetAllowedOrigins(*hostPermissions);

  if (!aInit.mBackgroundScripts.IsNull()) {
    mBackgroundScripts.SetValue().AppendElements(
        aInit.mBackgroundScripts.Value());
  }

  mBackgroundTypeModule = aInit.mBackgroundTypeModule;

  mContentScripts.SetCapacity(aInit.mContentScripts.Length());
  for (const auto& scriptInit : aInit.mContentScripts) {
    // The activeTab permission is only for dynamically injected scripts,
    // it cannot be used for declarative content scripts.
    if (scriptInit.mHasActiveTabPermission) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    RefPtr<WebExtensionContentScript> contentScript =
        new WebExtensionContentScript(aGlobal, *this, scriptInit, aRv);
    if (aRv.Failed()) {
      return;
    }
    mContentScripts.AppendElement(std::move(contentScript));
  }

  if (aInit.mReadyPromise.WasPassed()) {
    mReadyPromise = &aInit.mReadyPromise.Value();
  }
}

already_AddRefed<WebExtensionPolicy> WebExtensionPolicy::Constructor(
    GlobalObject& aGlobal, const WebExtensionInit& aInit, ErrorResult& aRv) {
  RefPtr<WebExtensionPolicy> policy =
      new WebExtensionPolicy(aGlobal, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return policy.forget();
}

/* static */
void WebExtensionPolicy::GetActiveExtensions(
    dom::GlobalObject& aGlobal,
    nsTArray<RefPtr<WebExtensionPolicy>>& aResults) {
  EPS().GetAll(aResults);
}

/* static */
already_AddRefed<WebExtensionPolicy> WebExtensionPolicy::GetByID(
    dom::GlobalObject& aGlobal, const nsAString& aID) {
  return do_AddRef(EPS().GetByID(aID));
}

/* static */
already_AddRefed<WebExtensionPolicy> WebExtensionPolicy::GetByHostname(
    dom::GlobalObject& aGlobal, const nsACString& aHostname) {
  return do_AddRef(EPS().GetByHost(aHostname));
}

/* static */
already_AddRefed<WebExtensionPolicy> WebExtensionPolicy::GetByURI(
    dom::GlobalObject& aGlobal, nsIURI* aURI) {
  return do_AddRef(EPS().GetByURL(aURI));
}

void WebExtensionPolicy::SetActive(bool aActive, ErrorResult& aRv) {
  if (aActive == mActive) {
    return;
  }

  bool ok = aActive ? Enable() : Disable();

  if (!ok) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

bool WebExtensionPolicy::Enable() {
  MOZ_ASSERT(!mActive);

  if (!EPS().RegisterExtension(*this)) {
    return false;
  }

  if (XRE_IsParentProcess()) {
    // Reserve a BrowsingContextGroup for use by this WebExtensionPolicy.
    RefPtr<BrowsingContextGroup> group = BrowsingContextGroup::Create();
    mBrowsingContextGroup = group->MakeKeepAlivePtr();
  }

  Unused << Proto()->SetSubstitution(MozExtensionHostname(), BaseURI());

  mActive = true;
  return true;
}

bool WebExtensionPolicy::Disable() {
  MOZ_ASSERT(mActive);
  MOZ_ASSERT(EPS().GetByID(Id()) == this);

  if (!EPS().UnregisterExtension(*this)) {
    return false;
  }

  if (XRE_IsParentProcess()) {
    // Clear our BrowsingContextGroup reference. A new instance will be created
    // when the extension is next activated.
    mBrowsingContextGroup = nullptr;
  }

  Unused << Proto()->SetSubstitution(MozExtensionHostname(), nullptr);

  mActive = false;
  return true;
}

void WebExtensionPolicy::GetURL(const nsAString& aPath, nsAString& aResult,
                                ErrorResult& aRv) const {
  auto result = GetURL(aPath);
  if (result.isOk()) {
    aResult = result.unwrap();
  } else {
    aRv.Throw(result.unwrapErr());
  }
}

Result<nsString, nsresult> WebExtensionPolicy::GetURL(
    const nsAString& aPath) const {
  nsPrintfCString spec("%s://%s/", kProto, MozExtensionHostname().get());

  nsCOMPtr<nsIURI> uri;
  MOZ_TRY(NS_NewURI(getter_AddRefs(uri), spec));

  MOZ_TRY(uri->Resolve(NS_ConvertUTF16toUTF8(aPath), spec));

  return NS_ConvertUTF8toUTF16(spec);
}

void WebExtensionPolicy::SetIgnoreQuarantine(bool aIgnore) {
  WebExtensionPolicy_Binding::ClearCachedIgnoreQuarantineValue(this);
  mCore->SetIgnoreQuarantine(aIgnore);
}

void WebExtensionPolicy::RegisterContentScript(
    WebExtensionContentScript& script, ErrorResult& aRv) {
  // Raise an "invalid argument" error if the script is not related to
  // the expected extension or if it is already registered.
  if (script.mExtension != this || mContentScripts.Contains(&script)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  RefPtr<WebExtensionContentScript> newScript = &script;

  if (!mContentScripts.AppendElement(std::move(newScript), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  WebExtensionPolicy_Binding::ClearCachedContentScriptsValue(this);
}

void WebExtensionPolicy::UnregisterContentScript(
    const WebExtensionContentScript& script, ErrorResult& aRv) {
  if (script.mExtension != this || !mContentScripts.RemoveElement(&script)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  WebExtensionPolicy_Binding::ClearCachedContentScriptsValue(this);
}

void WebExtensionPolicy::SetAllowedOrigins(MatchPatternSet& aAllowedOrigins) {
  // Make sure to keep the version in `WebExtensionPolicy` (which can be exposed
  // back to script using AllowedOrigins()), and the version in
  // `WebExtensionPolicyCore` (which is threadsafe) in sync.
  AutoWriteLock lock(mCore->mLock);
  mHostPermissions = &aAllowedOrigins;
  mCore->mHostPermissions = aAllowedOrigins.Core();
}

void WebExtensionPolicy::InjectContentScripts(ErrorResult& aRv) {
  nsresult rv = EPS().InjectContentScripts(this);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

/* static */
bool WebExtensionPolicy::UseRemoteWebExtensions(GlobalObject& aGlobal) {
  return EPS().UseRemoteExtensions();
}

/* static */
bool WebExtensionPolicy::IsExtensionProcess(GlobalObject& aGlobal) {
  return EPS().IsExtensionProcess();
}

/* static */
bool WebExtensionPolicy::BackgroundServiceWorkerEnabled(GlobalObject& aGlobal) {
  return StaticPrefs::extensions_backgroundServiceWorker_enabled_AtStartup();
}

/* static */
bool WebExtensionPolicy::QuarantinedDomainsEnabled(GlobalObject& aGlobal) {
  return EPS().GetQuarantinedDomainsEnabled();
}

/* static */
bool WebExtensionPolicy::IsRestrictedDoc(const DocInfo& aDoc) {
  // With the exception of top-level about:blank documents with null
  // principals, we never match documents that have non-content principals,
  // including those with null principals or system principals.
  if (aDoc.Principal() && !aDoc.Principal()->GetIsContentPrincipal()) {
    return true;
  }

  return IsRestrictedURI(aDoc.PrincipalURL());
}

/* static */
bool WebExtensionPolicy::IsRestrictedURI(const URLInfo& aURI) {
  RefPtr<AtomSet> restrictedDomains =
      ExtensionPolicyService::RestrictedDomains();

  if (restrictedDomains && restrictedDomains->Contains(aURI.HostAtom())) {
    return true;
  }

  if (AddonManagerWebAPI::IsValidSite(aURI.URI())) {
    return true;
  }

  return false;
}

/* static */
bool WebExtensionPolicy::IsQuarantinedDoc(const DocInfo& aDoc) {
  return IsQuarantinedURI(aDoc.PrincipalURL());
}

/* static */
bool WebExtensionPolicy::IsQuarantinedURI(const URLInfo& aURI) {
  // Ensure EPS is initialized before asking it about quarantined domains.
  Unused << EPS();

  RefPtr<AtomSet> quarantinedDomains =
      ExtensionPolicyService::QuarantinedDomains();

  return quarantinedDomains && quarantinedDomains->Contains(aURI.HostAtom());
}

nsCString WebExtensionPolicy::BackgroundPageHTML() const {
  nsCString result;

  if (mBackgroundScripts.IsNull()) {
    result.SetIsVoid(true);
    return result;
  }

  result.AppendLiteral(kBackgroundPageHTMLStart);

  const char* scriptType = mBackgroundTypeModule ? kBackgroundScriptTypeModule
                                                 : kBackgroundScriptTypeDefault;

  for (auto& script : mBackgroundScripts.Value()) {
    nsCString escaped;
    nsAppendEscapedHTML(NS_ConvertUTF16toUTF8(script), escaped);
    result.AppendPrintf(kBackgroundPageHTMLScript, scriptType, escaped.get());
  }

  result.AppendLiteral(kBackgroundPageHTMLEnd);
  return result;
}

void WebExtensionPolicy::Localize(const nsAString& aInput,
                                  nsString& aOutput) const {
  RefPtr<WebExtensionLocalizeCallback> callback(mLocalizeCallback);
  callback->Call(aInput, aOutput);
}

JSObject* WebExtensionPolicy::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return WebExtensionPolicy_Binding::Wrap(aCx, this, aGivenProto);
}

void WebExtensionPolicy::GetContentScripts(
    nsTArray<RefPtr<WebExtensionContentScript>>& aScripts) const {
  aScripts.AppendElements(mContentScripts);
}

bool WebExtensionPolicy::PrivateBrowsingAllowed() const {
  return HasPermission(nsGkAtoms::privateBrowsingAllowedPermission);
}

bool WebExtensionPolicy::CanAccessContext(nsILoadContext* aContext) const {
  MOZ_ASSERT(aContext);
  return PrivateBrowsingAllowed() || !aContext->UsePrivateBrowsing();
}

bool WebExtensionPolicy::CanAccessWindow(
    const dom::WindowProxyHolder& aWindow) const {
  if (PrivateBrowsingAllowed()) {
    return true;
  }
  // match browsing mode with policy
  nsIDocShell* docShell = aWindow.get()->GetDocShell();
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
  return !(loadContext && loadContext->UsePrivateBrowsing());
}

void WebExtensionPolicy::GetReadyPromise(
    JSContext* aCx, JS::MutableHandle<JSObject*> aResult) const {
  if (mReadyPromise) {
    aResult.set(mReadyPromise->PromiseObj());
  } else {
    aResult.set(nullptr);
  }
}

uint64_t WebExtensionPolicy::GetBrowsingContextGroupId() const {
  MOZ_ASSERT(XRE_IsParentProcess() && mActive);
  return mBrowsingContextGroup ? mBrowsingContextGroup->Id() : 0;
}

uint64_t WebExtensionPolicy::GetBrowsingContextGroupId(ErrorResult& aRv) {
  if (XRE_IsParentProcess() && mActive) {
    return GetBrowsingContextGroupId();
  }
  aRv.ThrowInvalidAccessError(
      "browsingContextGroupId only available for active policies in the "
      "parent process");
  return 0;
}

WebExtensionPolicy::~WebExtensionPolicy() { mCore->ClearPolicyWeakRef(); }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebExtensionPolicy)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebExtensionPolicy)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowsingContextGroup)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalizeCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHostPermissions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContentScripts)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  AssertIsOnMainThread();
  tmp->mCore->ClearPolicyWeakRef();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebExtensionPolicy)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContextGroup)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalizeCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHostPermissions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContentScripts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebExtensionPolicy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebExtensionPolicy)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebExtensionPolicy)

/*****************************************************************************
 * WebExtensionContentScript / MozDocumentMatcher
 *****************************************************************************/

/* static */
already_AddRefed<MozDocumentMatcher> MozDocumentMatcher::Constructor(
    GlobalObject& aGlobal, const dom::MozDocumentMatcherInit& aInit,
    ErrorResult& aRv) {
  RefPtr<MozDocumentMatcher> matcher =
      new MozDocumentMatcher(aGlobal, aInit, false, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return matcher.forget();
}

/* static */
already_AddRefed<WebExtensionContentScript>
WebExtensionContentScript::Constructor(GlobalObject& aGlobal,
                                       WebExtensionPolicy& aExtension,
                                       const ContentScriptInit& aInit,
                                       ErrorResult& aRv) {
  RefPtr<WebExtensionContentScript> script =
      new WebExtensionContentScript(aGlobal, aExtension, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return script.forget();
}

MozDocumentMatcher::MozDocumentMatcher(GlobalObject& aGlobal,
                                       const dom::MozDocumentMatcherInit& aInit,
                                       bool aRestricted, ErrorResult& aRv)
    : mHasActiveTabPermission(aInit.mHasActiveTabPermission),
      mRestricted(aRestricted),
      mAllFrames(aInit.mAllFrames),
      mCheckPermissions(aInit.mCheckPermissions),
      mFrameID(aInit.mFrameID),
      mMatchAboutBlank(aInit.mMatchAboutBlank) {
  MatchPatternOptions options;
  options.mRestrictSchemes = mRestricted;

  mMatches = ParseMatches(aGlobal, aInit.mMatches, options,
                          ErrorBehavior::CreateEmptyPattern, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (!aInit.mExcludeMatches.IsNull()) {
    mExcludeMatches =
        ParseMatches(aGlobal, aInit.mExcludeMatches.Value(), options,
                     ErrorBehavior::CreateEmptyPattern, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  if (!aInit.mIncludeGlobs.IsNull()) {
    if (!ParseGlobs(aGlobal, aInit.mIncludeGlobs.Value(),
                    mIncludeGlobs.SetValue(), aRv)) {
      return;
    }
  }

  if (!aInit.mExcludeGlobs.IsNull()) {
    if (!ParseGlobs(aGlobal, aInit.mExcludeGlobs.Value(),
                    mExcludeGlobs.SetValue(), aRv)) {
      return;
    }
  }

  if (!aInit.mOriginAttributesPatterns.IsNull()) {
    Sequence<OriginAttributesPattern>& arr =
        mOriginAttributesPatterns.SetValue();
    for (const auto& pattern : aInit.mOriginAttributesPatterns.Value()) {
      if (!arr.AppendElement(OriginAttributesPattern(pattern), fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }
  }
}

WebExtensionContentScript::WebExtensionContentScript(
    GlobalObject& aGlobal, WebExtensionPolicy& aExtension,
    const ContentScriptInit& aInit, ErrorResult& aRv)
    : MozDocumentMatcher(aGlobal, aInit,
                         !aExtension.HasPermission(nsGkAtoms::mozillaAddons),
                         aRv),
      mRunAt(aInit.mRunAt) {
  mCssPaths.Assign(aInit.mCssPaths);
  mJsPaths.Assign(aInit.mJsPaths);
  mExtension = &aExtension;

  // Origin permissions are optional in mv3, so always check them at runtime.
  if (mExtension->ManifestVersion() >= 3) {
    mCheckPermissions = true;
  }
}

bool MozDocumentMatcher::Matches(const DocInfo& aDoc,
                                 bool aIgnorePermissions) const {
  if (!mFrameID.IsNull()) {
    if (aDoc.FrameID() != mFrameID.Value()) {
      return false;
    }
  } else {
    if (!mAllFrames && !aDoc.IsTopLevel()) {
      return false;
    }
  }

  // match browsing mode with policy
  nsCOMPtr<nsILoadContext> loadContext = aDoc.GetLoadContext();
  if (loadContext && mExtension && !mExtension->CanAccessContext(loadContext)) {
    return false;
  }

  if (loadContext && !mOriginAttributesPatterns.IsNull()) {
    OriginAttributes docShellAttrs;
    loadContext->GetOriginAttributes(docShellAttrs);
    bool patternMatch = false;
    for (const auto& pattern : mOriginAttributesPatterns.Value()) {
      if (pattern.Matches(docShellAttrs)) {
        patternMatch = true;
        break;
      }
    }
    if (!patternMatch) {
      return false;
    }
  }

  // TODO bug 1411641: we should account for precursorPrincipal if
  // match_origin_as_fallback is specified (see also bug 1853411).
  if (!mMatchAboutBlank && aDoc.URL().InheritsPrincipal()) {
    return false;
  }

  // Top-level about:blank is a special case. Unlike about:blank frames/windows
  // opened by web pages, these do not have an origin that could be matched by
  // a match pattern (they have a null principal instead). To allow extensions
  // that intend to run scripts "everywhere", consider the document matched if
  // the match pattern describe a very broad pattern (such as "<all_urls>").
  if (mMatchAboutBlank && aDoc.IsTopLevel() &&
      (aDoc.URL().Spec().EqualsLiteral("about:blank") ||
       aDoc.URL().Scheme() == nsGkAtoms::data) &&
      aDoc.Principal() && aDoc.Principal()->GetIsNullPrincipal()) {
    if (StaticPrefs::extensions_script_about_blank_without_permission()) {
      return true;
    }
    if (mHasActiveTabPermission) {
      return true;
    }
    if (mMatches->MatchesAllWebUrls() && mIncludeGlobs.IsNull()) {
      // When mIncludeGlobs is present, mMatches does not necessarily match
      // everything (except possibly if include_globs is just ["*"]). So we
      // only match if mMatches is present without mIncludeGlobs.
      return true;
    }
    // Null principal is never going to match, so we may as well return now.
    return false;
  }

  if (mRestricted && WebExtensionPolicy::IsRestrictedDoc(aDoc)) {
    return false;
  }

  if (mRestricted && mExtension && mExtension->QuarantinedFromDoc(aDoc)) {
    return false;
  }

  auto& urlinfo = aDoc.PrincipalURL();
  if (mExtension && mExtension->ManifestVersion() >= 3) {
    // In MV3, activeTab only allows access to same-origin iframes.
    if (mHasActiveTabPermission && aDoc.IsSameOriginWithTop() &&
        MatchPattern::MatchesAllURLs(urlinfo)) {
      return true;
    }
  } else {
    if (mHasActiveTabPermission && aDoc.ShouldMatchActiveTabPermission() &&
        MatchPattern::MatchesAllURLs(urlinfo)) {
      return true;
    }
  }

  return MatchesURI(urlinfo, aIgnorePermissions);
}

bool MozDocumentMatcher::MatchesURI(const URLInfo& aURL,
                                    bool aIgnorePermissions) const {
  MOZ_ASSERT((!mRestricted && !mCheckPermissions) || mExtension);

  if (!mMatches->Matches(aURL)) {
    return false;
  }

  if (mExcludeMatches && mExcludeMatches->Matches(aURL)) {
    return false;
  }

  if (!mIncludeGlobs.IsNull() && !mIncludeGlobs.Value().Matches(aURL.CSpec())) {
    return false;
  }

  if (!mExcludeGlobs.IsNull() && mExcludeGlobs.Value().Matches(aURL.CSpec())) {
    return false;
  }

  if (mRestricted && WebExtensionPolicy::IsRestrictedURI(aURL)) {
    return false;
  }

  if (mRestricted && mExtension->QuarantinedFromURI(aURL)) {
    return false;
  }

  if (mCheckPermissions && !aIgnorePermissions &&
      !mExtension->CanAccessURI(aURL, false, false, true)) {
    return false;
  }

  return true;
}

bool MozDocumentMatcher::MatchesWindowGlobal(WindowGlobalChild& aWindow,
                                             bool aIgnorePermissions) const {
  if (aWindow.IsClosed() || !aWindow.IsCurrentGlobal()) {
    return false;
  }
  nsGlobalWindowInner* inner = aWindow.GetWindowGlobal();
  if (!inner || !inner->GetDocShell()) {
    return false;
  }
  return Matches(inner->GetOuterWindow(), aIgnorePermissions);
}

void MozDocumentMatcher::GetOriginAttributesPatterns(
    JSContext* aCx, JS::MutableHandle<JS::Value> aVal,
    ErrorResult& aError) const {
  if (!ToJSValue(aCx, mOriginAttributesPatterns, aVal)) {
    aError.NoteJSContextException(aCx);
  }
}

JSObject* MozDocumentMatcher::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return MozDocumentMatcher_Binding::Wrap(aCx, this, aGivenProto);
}

JSObject* WebExtensionContentScript::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return WebExtensionContentScript_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MozDocumentMatcher, mMatches,
                                      mExcludeMatches, mExtension)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MozDocumentMatcher)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MozDocumentMatcher)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MozDocumentMatcher)

/*****************************************************************************
 * MozDocumentObserver
 *****************************************************************************/

/* static */
already_AddRefed<DocumentObserver> DocumentObserver::Constructor(
    GlobalObject& aGlobal, dom::MozDocumentCallback& aCallbacks) {
  RefPtr<DocumentObserver> matcher =
      new DocumentObserver(aGlobal.GetAsSupports(), aCallbacks);
  return matcher.forget();
}

void DocumentObserver::Observe(
    const dom::Sequence<OwningNonNull<MozDocumentMatcher>>& matchers,
    ErrorResult& aRv) {
  if (!EPS().RegisterObserver(*this)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  mMatchers.Clear();
  for (auto& matcher : matchers) {
    if (!mMatchers.AppendElement(matcher, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }
}

void DocumentObserver::Disconnect() {
  Unused << EPS().UnregisterObserver(*this);
}

void DocumentObserver::NotifyMatch(MozDocumentMatcher& aMatcher,
                                   nsPIDOMWindowOuter* aWindow) {
  IgnoredErrorResult rv;
  mCallbacks->OnNewDocument(
      aMatcher, WindowProxyHolder(aWindow->GetBrowsingContext()), rv);
}

void DocumentObserver::NotifyMatch(MozDocumentMatcher& aMatcher,
                                   nsILoadInfo* aLoadInfo) {
  IgnoredErrorResult rv;
  mCallbacks->OnPreloadDocument(aMatcher, aLoadInfo, rv);
}

JSObject* DocumentObserver::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return MozDocumentObserver_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DocumentObserver, mCallbacks, mMatchers,
                                      mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DocumentObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DocumentObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DocumentObserver)

/*****************************************************************************
 * DocInfo
 *****************************************************************************/

DocInfo::DocInfo(const URLInfo& aURL, nsILoadInfo* aLoadInfo)
    : mURL(aURL), mObj(AsVariant(aLoadInfo)) {}

DocInfo::DocInfo(nsPIDOMWindowOuter* aWindow)
    : mURL(aWindow->GetDocumentURI()), mObj(AsVariant(aWindow)) {}

bool DocInfo::IsTopLevel() const {
  if (mIsTopLevel.isNothing()) {
    struct Matcher {
      bool operator()(Window aWin) {
        return aWin->GetBrowsingContext()->IsTop();
      }
      bool operator()(LoadInfo aLoadInfo) {
        return aLoadInfo->GetIsTopLevelLoad();
      }
    };
    mIsTopLevel.emplace(mObj.match(Matcher()));
  }
  return mIsTopLevel.ref();
}

bool WindowShouldMatchActiveTab(nsPIDOMWindowOuter* aWin) {
  WindowContext* wc = aWin->GetCurrentInnerWindow()->GetWindowContext();
  if (wc && wc->SameOriginWithTop()) {
    // If the frame is same-origin to top, accept the match regardless of
    // whether the frame was populated dynamically.
    return true;
  }
  for (; wc; wc = wc->GetParentWindowContext()) {
    BrowsingContext* bc = wc->GetBrowsingContext();
    if (bc->IsTopContent()) {
      return true;
    }

    if (bc->CreatedDynamically() || !wc->GetIsOriginalFrameSource()) {
      return false;
    }
  }
  MOZ_ASSERT_UNREACHABLE("Should reach top content before end of loop");
  return false;
}

bool DocInfo::ShouldMatchActiveTabPermission() const {
  struct Matcher {
    bool operator()(Window aWin) { return WindowShouldMatchActiveTab(aWin); }
    bool operator()(LoadInfo aLoadInfo) { return false; }
  };
  return mObj.match(Matcher());
}

bool DocInfo::IsSameOriginWithTop() const {
  struct Matcher {
    bool operator()(Window aWin) {
      WindowContext* wc = aWin->GetCurrentInnerWindow()->GetWindowContext();
      return wc && wc->SameOriginWithTop();
    }
    bool operator()(LoadInfo aLoadInfo) { return false; }
  };
  return mObj.match(Matcher());
}

uint64_t DocInfo::FrameID() const {
  if (mFrameID.isNothing()) {
    if (IsTopLevel()) {
      mFrameID.emplace(0);
    } else {
      struct Matcher {
        uint64_t operator()(Window aWin) {
          return aWin->GetBrowsingContext()->Id();
        }
        uint64_t operator()(LoadInfo aLoadInfo) {
          return aLoadInfo->GetBrowsingContextID();
        }
      };
      mFrameID.emplace(mObj.match(Matcher()));
    }
  }
  return mFrameID.ref();
}

nsIPrincipal* DocInfo::Principal() const {
  if (mPrincipal.isNothing()) {
    struct Matcher {
      explicit Matcher(const DocInfo& aThis) : mThis(aThis) {}
      const DocInfo& mThis;

      nsIPrincipal* operator()(Window aWin) {
        RefPtr<Document> doc = aWin->GetDoc();
        return doc->NodePrincipal();
      }
      nsIPrincipal* operator()(LoadInfo aLoadInfo) {
        if (!(mThis.URL().InheritsPrincipal() ||
              aLoadInfo->GetForceInheritPrincipal())) {
          return nullptr;
        }
        if (auto principal = aLoadInfo->PrincipalToInherit()) {
          return principal;
        }
        return aLoadInfo->TriggeringPrincipal();
      }
    };
    mPrincipal.emplace(mObj.match(Matcher(*this)));
  }
  return mPrincipal.ref();
}

const URLInfo& DocInfo::PrincipalURL() const {
  if (!(Principal() && Principal()->GetIsContentPrincipal())) {
    return URL();
  }

  if (mPrincipalURL.isNothing()) {
    nsIPrincipal* prin = Principal();
    auto* basePrin = BasePrincipal::Cast(prin);
    nsCOMPtr<nsIURI> uri;
    if (NS_SUCCEEDED(basePrin->GetURI(getter_AddRefs(uri)))) {
      MOZ_DIAGNOSTIC_ASSERT(uri);
      mPrincipalURL.emplace(uri);
    } else {
      mPrincipalURL.emplace(URL());
    }
  }

  return mPrincipalURL.ref();
}

}  // namespace extensions
}  // namespace mozilla
