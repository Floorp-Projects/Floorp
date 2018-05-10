/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/extensions/WebExtensionContentScript.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

#include "mozilla/AddonManagerWebAPI.h"
#include "mozilla/ResultExtensions.h"
#include "nsEscape.h"
#include "nsIDocShell.h"
#include "nsIObserver.h"
#include "nsISubstitutingProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace extensions {

using namespace dom;

static const char kProto[] = "moz-extension";

static const char kBackgroundPageHTMLStart[] = "<!DOCTYPE html>\n\
<html>\n\
  <head><meta charset=\"utf-8\"></head>\n\
  <body>";

static const char kBackgroundPageHTMLScript[] = "\n\
    <script type=\"text/javascript\" src=\"%s\"></script>";

static const char kBackgroundPageHTMLEnd[] = "\n\
  </body>\n\
</html>";

static const char kRestrictedDomainPref[] =
  "extensions.webextensions.restrictedDomains";

static inline ExtensionPolicyService&
EPS()
{
  return ExtensionPolicyService::GetSingleton();
}

static nsISubstitutingProtocolHandler*
Proto()
{
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


/*****************************************************************************
 * WebExtensionPolicy
 *****************************************************************************/

WebExtensionPolicy::WebExtensionPolicy(GlobalObject& aGlobal,
                                       const WebExtensionInit& aInit,
                                       ErrorResult& aRv)
  : mId(NS_AtomizeMainThread(aInit.mId))
  , mHostname(aInit.mMozExtensionHostname)
  , mName(aInit.mName)
  , mContentSecurityPolicy(aInit.mContentSecurityPolicy)
  , mLocalizeCallback(aInit.mLocalizeCallback)
  , mPermissions(new AtomSet(aInit.mPermissions))
  , mHostPermissions(aInit.mAllowedOrigins)
{
  mWebAccessiblePaths.AppendElements(aInit.mWebAccessibleResources);

  if (!aInit.mBackgroundScripts.IsNull()) {
    mBackgroundScripts.SetValue().AppendElements(aInit.mBackgroundScripts.Value());
  }

  if (mContentSecurityPolicy.IsVoid()) {
    EPS().DefaultCSP(mContentSecurityPolicy);
  }

  mContentScripts.SetCapacity(aInit.mContentScripts.Length());
  for (const auto& scriptInit : aInit.mContentScripts) {
    // The activeTab permission is only for dynamically injected scripts,
    // it cannot be used for declarative content scripts.
    if (scriptInit.mHasActiveTabPermission) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    RefPtr<WebExtensionContentScript> contentScript =
      new WebExtensionContentScript(*this, scriptInit, aRv);
    if (aRv.Failed()) {
      return;
    }
    mContentScripts.AppendElement(Move(contentScript));
  }

  nsresult rv = NS_NewURI(getter_AddRefs(mBaseURI), aInit.mBaseURL);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::Constructor(GlobalObject& aGlobal,
                                const WebExtensionInit& aInit,
                                ErrorResult& aRv)
{
  RefPtr<WebExtensionPolicy> policy = new WebExtensionPolicy(aGlobal, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return policy.forget();
}


/* static */ void
WebExtensionPolicy::GetActiveExtensions(dom::GlobalObject& aGlobal,
                                        nsTArray<RefPtr<WebExtensionPolicy>>& aResults)
{
  EPS().GetAll(aResults);
}

/* static */ already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::GetByID(dom::GlobalObject& aGlobal, const nsAString& aID)
{
  return do_AddRef(EPS().GetByID(aID));
}

/* static */ already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::GetByHostname(dom::GlobalObject& aGlobal, const nsACString& aHostname)
{
  return do_AddRef(EPS().GetByHost(aHostname));
}

/* static */ already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::GetByURI(dom::GlobalObject& aGlobal, nsIURI* aURI)
{
  return do_AddRef(EPS().GetByURL(aURI));
}


void
WebExtensionPolicy::SetActive(bool aActive, ErrorResult& aRv)
{
  if (aActive == mActive) {
    return;
  }

  bool ok = aActive ? Enable() : Disable();

  if (!ok) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

bool
WebExtensionPolicy::Enable()
{
  MOZ_ASSERT(!mActive);

  if (!EPS().RegisterExtension(*this)) {
    return false;
  }

  Unused << Proto()->SetSubstitution(MozExtensionHostname(), mBaseURI);

  mActive = true;
  return true;
}

bool
WebExtensionPolicy::Disable()
{
  MOZ_ASSERT(mActive);
  MOZ_ASSERT(EPS().GetByID(Id()) == this);

  if (!EPS().UnregisterExtension(*this)) {
    return false;
  }

  Unused << Proto()->SetSubstitution(MozExtensionHostname(), nullptr);

  mActive = false;
  return true;
}

void
WebExtensionPolicy::GetURL(const nsAString& aPath,
                           nsAString& aResult,
                           ErrorResult& aRv) const
{
  auto result = GetURL(aPath);
  if (result.isOk()) {
    aResult = result.unwrap();
  } else {
    aRv.Throw(result.unwrapErr());
  }
}

Result<nsString, nsresult>
WebExtensionPolicy::GetURL(const nsAString& aPath) const
{
  nsPrintfCString spec("%s://%s/", kProto, mHostname.get());

  nsCOMPtr<nsIURI> uri;
  MOZ_TRY(NS_NewURI(getter_AddRefs(uri), spec));

  MOZ_TRY(uri->Resolve(NS_ConvertUTF16toUTF8(aPath), spec));

  return NS_ConvertUTF8toUTF16(spec);
}

void
WebExtensionPolicy::RegisterContentScript(WebExtensionContentScript& script,
                                          ErrorResult& aRv)
{
  // Raise an "invalid argument" error if the script is not related to
  // the expected extension or if it is already registered.
  if (script.mExtension != this || mContentScripts.Contains(&script)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  RefPtr<WebExtensionContentScript> newScript = &script;

  if (!mContentScripts.AppendElement(Move(newScript), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  WebExtensionPolicyBinding::ClearCachedContentScriptsValue(this);
}

void
WebExtensionPolicy::UnregisterContentScript(const WebExtensionContentScript& script,
                                            ErrorResult& aRv)
{
  if (script.mExtension != this || !mContentScripts.RemoveElement(&script)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  WebExtensionPolicyBinding::ClearCachedContentScriptsValue(this);
}

/* static */ bool
WebExtensionPolicy::UseRemoteWebExtensions(GlobalObject& aGlobal)
{
  return EPS().UseRemoteExtensions();
}

/* static */ bool
WebExtensionPolicy::IsExtensionProcess(GlobalObject& aGlobal)
{
  return EPS().IsExtensionProcess();
}

namespace {
  /**
   * Maintains a dynamically updated AtomSet based on the comma-separated
   * values in the given string pref.
   */
  class AtomSetPref : public nsIObserver
                    , public nsSupportsWeakReference
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    static already_AddRefed<AtomSetPref>
    Create(const char* aPref)
    {
      RefPtr<AtomSetPref> self = new AtomSetPref(aPref);
      Preferences::AddWeakObserver(self, aPref);
      return self.forget();
    }

    const AtomSet& Get() const;

    bool Contains(const nsAtom* aAtom) const
    {
      return Get().Contains(aAtom);
    }

  protected:
    virtual ~AtomSetPref() = default;

    explicit AtomSetPref(const char* aPref) : mPref(aPref)
    {}

  private:
    mutable RefPtr<AtomSet> mAtomSet;
    const char* mPref;
  };

  const AtomSet&
  AtomSetPref::Get() const
  {
    if (!mAtomSet) {
      nsAutoCString eltsString;
      Unused << Preferences::GetCString(mPref, eltsString);

      AutoTArray<nsString, 32> elts;
      for (const nsACString& elt : eltsString.Split(',')) {
        elts.AppendElement(NS_ConvertUTF8toUTF16(elt));
        elts.LastElement().StripWhitespace();
      }
      mAtomSet = new AtomSet(elts);
    }

    return *mAtomSet;
  }

  NS_IMETHODIMP
  AtomSetPref::Observe(nsISupports *aSubject, const char *aTopic,
                       const char16_t *aData)
  {
    mAtomSet = nullptr;
    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(AtomSetPref, nsIObserver, nsISupportsWeakReference)
};

/* static */ bool
WebExtensionPolicy::IsRestrictedDoc(const DocInfo& aDoc)
{
  // With the exception of top-level about:blank documents with null
  // principals, we never match documents that have non-codebase principals,
  // including those with null principals or system principals.
  if (aDoc.Principal() && !aDoc.Principal()->GetIsCodebasePrincipal()) {
    return true;
  }

  return IsRestrictedURI(aDoc.PrincipalURL());
}

/* static */ bool
WebExtensionPolicy::IsRestrictedURI(const URLInfo &aURI)
{
  static RefPtr<AtomSetPref> domains;
  if (!domains) {
    domains = AtomSetPref::Create(kRestrictedDomainPref);
    ClearOnShutdown(&domains);
  }

  if (domains->Contains(aURI.HostAtom())) {
    return true;
  }

  if (AddonManagerWebAPI::IsValidSite(aURI.URI())) {
    return true;
  }

  return false;
}

nsCString
WebExtensionPolicy::BackgroundPageHTML() const
{
  nsAutoCString result;

  if (mBackgroundScripts.IsNull()) {
    result.SetIsVoid(true);
    return result;
  }

  result.AppendLiteral(kBackgroundPageHTMLStart);

  for (auto& script : mBackgroundScripts.Value()) {
    nsCString escaped;
    nsAppendEscapedHTML(NS_ConvertUTF16toUTF8(script), escaped);

    result.AppendPrintf(kBackgroundPageHTMLScript, escaped.get());
  }

  result.AppendLiteral(kBackgroundPageHTMLEnd);
  return result;
}

void
WebExtensionPolicy::Localize(const nsAString& aInput, nsString& aOutput) const
{
  mLocalizeCallback->Call(aInput, aOutput);
}


JSObject*
WebExtensionPolicy::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return WebExtensionPolicyBinding::Wrap(aCx, this, aGivenProto);
}

void
WebExtensionPolicy::GetContentScripts(nsTArray<RefPtr<WebExtensionContentScript>>& aScripts) const
{
  aScripts.AppendElements(mContentScripts);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebExtensionPolicy, mParent,
                                      mLocalizeCallback,
                                      mHostPermissions,
                                      mWebAccessiblePaths,
                                      mContentScripts)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebExtensionPolicy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebExtensionPolicy)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebExtensionPolicy)


/*****************************************************************************
 * WebExtensionContentScript
 *****************************************************************************/

/* static */ already_AddRefed<WebExtensionContentScript>
WebExtensionContentScript::Constructor(GlobalObject& aGlobal,
                                       WebExtensionPolicy& aExtension,
                                       const ContentScriptInit& aInit,
                                       ErrorResult& aRv)
{
  RefPtr<WebExtensionContentScript> script = new WebExtensionContentScript(aExtension, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return script.forget();
}

WebExtensionContentScript::WebExtensionContentScript(WebExtensionPolicy& aExtension,
                                                     const ContentScriptInit& aInit,
                                                     ErrorResult& aRv)
  : mExtension(&aExtension)
  , mHasActiveTabPermission(aInit.mHasActiveTabPermission)
  , mRestricted(!aExtension.HasPermission(nsGkAtoms::mozillaAddons))
  , mMatches(aInit.mMatches)
  , mExcludeMatches(aInit.mExcludeMatches)
  , mCssPaths(aInit.mCssPaths)
  , mJsPaths(aInit.mJsPaths)
  , mRunAt(aInit.mRunAt)
  , mAllFrames(aInit.mAllFrames)
  , mFrameID(aInit.mFrameID)
  , mMatchAboutBlank(aInit.mMatchAboutBlank)
{
  if (!aInit.mIncludeGlobs.IsNull()) {
    mIncludeGlobs.SetValue().AppendElements(aInit.mIncludeGlobs.Value());
  }

  if (!aInit.mExcludeGlobs.IsNull()) {
    mExcludeGlobs.SetValue().AppendElements(aInit.mExcludeGlobs.Value());
  }
}

bool
WebExtensionContentScript::Matches(const DocInfo& aDoc) const
{
  if (!mFrameID.IsNull()) {
    if (aDoc.FrameID() != mFrameID.Value()) {
      return false;
    }
  } else {
    if (!mAllFrames && !aDoc.IsTopLevel()) {
      return false;
    }
  }

  if (!mMatchAboutBlank && aDoc.URL().InheritsPrincipal()) {
    return false;
  }

  // Top-level about:blank is a special case. We treat it as a match if
  // matchAboutBlank is true and it has the null principal. In all other
  // cases, we test the URL of the principal that it inherits.
  if (mMatchAboutBlank && aDoc.IsTopLevel() &&
      aDoc.URL().Spec().EqualsLiteral("about:blank") &&
      aDoc.Principal() && aDoc.Principal()->GetIsNullPrincipal()) {
    return true;
  }

  if (mRestricted && mExtension->IsRestrictedDoc(aDoc)) {
    return false;
  }

  auto& urlinfo = aDoc.PrincipalURL();
  if (mHasActiveTabPermission && aDoc.ShouldMatchActiveTabPermission() &&
      MatchPattern::MatchesAllURLs(urlinfo)) {
    return true;
  }

  return MatchesURI(urlinfo);
}

bool
WebExtensionContentScript::MatchesURI(const URLInfo& aURL) const
{
  if (!mMatches->Matches(aURL)) {
    return false;
  }

  if (mExcludeMatches && mExcludeMatches->Matches(aURL)) {
    return false;
  }

  if (!mIncludeGlobs.IsNull() && !mIncludeGlobs.Value().Matches(aURL.Spec())) {
    return false;
  }

  if (!mExcludeGlobs.IsNull() && mExcludeGlobs.Value().Matches(aURL.Spec())) {
    return false;
  }

  if (mRestricted && mExtension->IsRestrictedURI(aURL)) {
    return false;
  }

  return true;
}


JSObject*
WebExtensionContentScript::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return WebExtensionContentScriptBinding::Wrap(aCx, this, aGivenProto);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebExtensionContentScript,
                                      mMatches, mExcludeMatches,
                                      mIncludeGlobs, mExcludeGlobs,
                                      mExtension)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebExtensionContentScript)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebExtensionContentScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebExtensionContentScript)


/*****************************************************************************
 * DocInfo
 *****************************************************************************/

DocInfo::DocInfo(const URLInfo& aURL, nsILoadInfo* aLoadInfo)
  : mURL(aURL)
  , mObj(AsVariant(aLoadInfo))
{}

DocInfo::DocInfo(nsPIDOMWindowOuter* aWindow)
  : mURL(aWindow->GetDocumentURI())
  , mObj(AsVariant(aWindow))
{}

bool
DocInfo::IsTopLevel() const
{
  if (mIsTopLevel.isNothing()) {
    struct Matcher
    {
      bool match(Window aWin) { return aWin->IsTopLevelWindow(); }
      bool match(LoadInfo aLoadInfo) { return aLoadInfo->GetIsTopLevelLoad(); }
    };
    mIsTopLevel.emplace(mObj.match(Matcher()));
  }
  return mIsTopLevel.ref();
}

bool
WindowShouldMatchActiveTab(nsPIDOMWindowOuter* aWin)
{
  if (aWin->IsTopLevelWindow()) {
    return true;
  }

  nsIDocShell* docshell = aWin->GetDocShell();
  if (!docshell || docshell->GetCreatedDynamically()) {
    return false;
  }

  nsIDocument* doc = aWin->GetExtantDoc();
  if (!doc) {
    return false;
  }

  nsIChannel* channel = doc->GetChannel();
  if (!channel) {
    return false;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();

  if (!loadInfo) {
    return false;
  }

  if (!loadInfo->GetOriginalFrameSrcLoad()) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parent = aWin->GetParent();
  MOZ_ASSERT(parent != nullptr);
  return WindowShouldMatchActiveTab(parent);
}

bool
DocInfo::ShouldMatchActiveTabPermission() const
{
  struct Matcher
  {
    bool match(Window aWin) { return WindowShouldMatchActiveTab(aWin); }
    bool match(LoadInfo aLoadInfo) { return false; }
  };
  return mObj.match(Matcher());
}

uint64_t
DocInfo::FrameID() const
{
  if (mFrameID.isNothing()) {
    if (IsTopLevel()) {
      mFrameID.emplace(0);
    } else {
      struct Matcher
      {
        uint64_t match(Window aWin) { return aWin->WindowID(); }
        uint64_t match(LoadInfo aLoadInfo) { return aLoadInfo->GetOuterWindowID(); }
      };
      mFrameID.emplace(mObj.match(Matcher()));
    }
  }
  return mFrameID.ref();
}

nsIPrincipal*
DocInfo::Principal() const
{
  if (mPrincipal.isNothing()) {
    struct Matcher
    {
      explicit Matcher(const DocInfo& aThis) : mThis(aThis) {}
      const DocInfo& mThis;

      nsIPrincipal* match(Window aWin)
      {
        nsCOMPtr<nsIDocument> doc = aWin->GetDoc();
        return doc->NodePrincipal();
      }
      nsIPrincipal* match(LoadInfo aLoadInfo)
      {
        if (!(mThis.URL().InheritsPrincipal() || aLoadInfo->GetForceInheritPrincipal())) {
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

const URLInfo&
DocInfo::PrincipalURL() const
{
  if (!(Principal() && Principal()->GetIsCodebasePrincipal())) {
    return URL();
  }

  if (mPrincipalURL.isNothing()) {
    nsIPrincipal* prin = Principal();
    nsCOMPtr<nsIURI> uri;
    if (NS_SUCCEEDED(prin->GetURI(getter_AddRefs(uri)))) {
      MOZ_DIAGNOSTIC_ASSERT(uri);
      mPrincipalURL.emplace(uri);
    } else {
      mPrincipalURL.emplace(URL());
    }
  }

  return mPrincipalURL.ref();
}

} // namespace extensions
} // namespace mozilla
