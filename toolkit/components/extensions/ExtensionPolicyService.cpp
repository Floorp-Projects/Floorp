/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/extensions/DocumentObserver.h"
#include "mozilla/extensions/WebExtensionContentScript.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/SimpleEnumerator.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozIExtensionProcessScript.h"
#include "nsEscape.h"
#include "nsGkAtoms.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "mozilla/dom/Document.h"
#include "nsGlobalWindowOuter.h"
#include "nsILoadInfo.h"
#include "nsIXULRuntime.h"
#include "nsImportModule.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsPIDOMWindow.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"

namespace mozilla {

using namespace extensions;

using dom::AutoJSAPI;
using dom::Document;
using dom::Promise;

#define DEFAULT_CSP_PREF \
  "extensions.webextensions.default-content-security-policy"
#define DEFAULT_DEFAULT_CSP "script-src 'self'; object-src 'self';"

#define OBS_TOPIC_PRELOAD_SCRIPT "web-extension-preload-content-script"
#define OBS_TOPIC_LOAD_SCRIPT "web-extension-load-content-script"

static const char kDocElementInserted[] = "initial-document-element-inserted";

/*****************************************************************************
 * ExtensionPolicyService
 *****************************************************************************/

/* static */
mozIExtensionProcessScript& ExtensionPolicyService::ProcessScript() {
  static nsCOMPtr<mozIExtensionProcessScript> sProcessScript;

  MOZ_ASSERT(NS_IsMainThread());

  if (MOZ_UNLIKELY(!sProcessScript)) {
    sProcessScript =
        do_ImportModule("resource://gre/modules/ExtensionProcessScript.jsm",
                        "ExtensionProcessScript");
    ClearOnShutdown(&sProcessScript);
  }
  return *sProcessScript;
}

/* static */ ExtensionPolicyService& ExtensionPolicyService::GetSingleton() {
  static RefPtr<ExtensionPolicyService> sExtensionPolicyService;

  if (MOZ_UNLIKELY(!sExtensionPolicyService)) {
    sExtensionPolicyService = new ExtensionPolicyService();
    RegisterWeakMemoryReporter(sExtensionPolicyService);
    ClearOnShutdown(&sExtensionPolicyService);
  }
  return *sExtensionPolicyService.get();
}

ExtensionPolicyService::ExtensionPolicyService() {
  mObs = services::GetObserverService();
  MOZ_RELEASE_ASSERT(mObs);

  mDefaultCSP.SetIsVoid(true);

  RegisterObservers();
}

ExtensionPolicyService::~ExtensionPolicyService() {
  UnregisterWeakMemoryReporter(this);
}

bool ExtensionPolicyService::UseRemoteExtensions() const {
  static Maybe<bool> sRemoteExtensions;
  if (MOZ_UNLIKELY(sRemoteExtensions.isNothing())) {
    sRemoteExtensions = Some(StaticPrefs::extensions_webextensions_remote());
  }
  return sRemoteExtensions.value() && BrowserTabsRemoteAutostart();
}

bool ExtensionPolicyService::IsExtensionProcess() const {
  bool isRemote = UseRemoteExtensions();

  if (isRemote && XRE_IsContentProcess()) {
    auto& remoteType = dom::ContentChild::GetSingleton()->GetRemoteType();
    return remoteType == EXTENSION_REMOTE_TYPE;
  }
  return !isRemote && XRE_IsParentProcess();
}

WebExtensionPolicy* ExtensionPolicyService::GetByURL(const URLInfo& aURL) {
  if (aURL.Scheme() == nsGkAtoms::moz_extension) {
    return GetByHost(aURL.Host());
  }
  return nullptr;
}

void ExtensionPolicyService::GetAll(
    nsTArray<RefPtr<WebExtensionPolicy>>& aResult) {
  AppendToArray(aResult, mExtensions.Values());
}

bool ExtensionPolicyService::RegisterExtension(WebExtensionPolicy& aPolicy) {
  bool ok =
      (!GetByID(aPolicy.Id()) && !GetByHost(aPolicy.MozExtensionHostname()));
  MOZ_ASSERT(ok);

  if (!ok) {
    return false;
  }

  mExtensions.InsertOrUpdate(aPolicy.Id(), RefPtr{&aPolicy});
  mExtensionHosts.InsertOrUpdate(aPolicy.MozExtensionHostname(),
                                 RefPtr{&aPolicy});
  return true;
}

bool ExtensionPolicyService::UnregisterExtension(WebExtensionPolicy& aPolicy) {
  bool ok = (GetByID(aPolicy.Id()) == &aPolicy &&
             GetByHost(aPolicy.MozExtensionHostname()) == &aPolicy);
  MOZ_ASSERT(ok);

  if (!ok) {
    return false;
  }

  mExtensions.Remove(aPolicy.Id());
  mExtensionHosts.Remove(aPolicy.MozExtensionHostname());
  return true;
}

bool ExtensionPolicyService::RegisterObserver(DocumentObserver& aObserver) {
  bool inserted = false;
  mObservers.LookupOrInsertWith(&aObserver, [&] {
    inserted = true;
    return RefPtr{&aObserver};
  });
  return inserted;
}

bool ExtensionPolicyService::UnregisterObserver(DocumentObserver& aObserver) {
  return mObservers.Remove(&aObserver);
}

/*****************************************************************************
 * nsIMemoryReporter
 *****************************************************************************/

NS_IMETHODIMP
ExtensionPolicyService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData, bool aAnonymize) {
  for (const auto& ext : mExtensions.Values()) {
    nsAtomCString id(ext->Id());

    NS_ConvertUTF16toUTF8 name(ext->Name());
    name.ReplaceSubstring("\"", "");
    name.ReplaceSubstring("\\", "");

    nsString url;
    MOZ_TRY_VAR(url, ext->GetURL(u""_ns));

    nsPrintfCString desc("Extension(id=%s, name=\"%s\", baseURL=%s)", id.get(),
                         name.get(), NS_ConvertUTF16toUTF8(url).get());
    desc.ReplaceChar('/', '\\');

    nsCString path("extensions/");
    path.Append(desc);

    aHandleReport->Callback(""_ns, path, KIND_NONHEAP, UNITS_COUNT, 1,
                            "WebExtensions that are active in this session"_ns,
                            aData);
  }

  return NS_OK;
}

/*****************************************************************************
 * Content script management
 *****************************************************************************/

void ExtensionPolicyService::RegisterObservers() {
  mObs->AddObserver(this, kDocElementInserted, false);
  if (XRE_IsContentProcess()) {
    mObs->AddObserver(this, "http-on-opening-request", false);
    mObs->AddObserver(this, "document-on-opening-request", false);
  }

  Preferences::AddStrongObserver(this, DEFAULT_CSP_PREF);
}

void ExtensionPolicyService::UnregisterObservers() {
  mObs->RemoveObserver(this, kDocElementInserted);
  if (XRE_IsContentProcess()) {
    mObs->RemoveObserver(this, "http-on-opening-request");
    mObs->RemoveObserver(this, "document-on-opening-request");
  }

  Preferences::RemoveObserver(this, DEFAULT_CSP_PREF);
}

nsresult ExtensionPolicyService::Observe(nsISupports* aSubject,
                                         const char* aTopic,
                                         const char16_t* aData) {
  if (!strcmp(aTopic, kDocElementInserted)) {
    nsCOMPtr<Document> doc = do_QueryInterface(aSubject);
    if (doc) {
      CheckDocument(doc);
    }
  } else if (!strcmp(aTopic, "http-on-opening-request") ||
             !strcmp(aTopic, "document-on-opening-request")) {
    nsCOMPtr<nsIChannel> chan = do_QueryInterface(aSubject);
    if (chan) {
      CheckRequest(chan);
    }
  } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    const nsCString converted = NS_ConvertUTF16toUTF8(aData);
    const char* pref = converted.get();
    if (!strcmp(pref, DEFAULT_CSP_PREF)) {
      mDefaultCSP.SetIsVoid(true);
    }
  }
  return NS_OK;
}

already_AddRefed<Promise> ExtensionPolicyService::ExecuteContentScript(
    nsPIDOMWindowInner* aWindow, WebExtensionContentScript& aScript) {
  if (!aWindow->IsCurrentInnerWindow()) {
    return nullptr;
  }

  RefPtr<Promise> promise;
  ProcessScript().LoadContentScript(&aScript, aWindow, getter_AddRefs(promise));
  return promise.forget();
}

RefPtr<Promise> ExtensionPolicyService::ExecuteContentScripts(
    JSContext* aCx, nsPIDOMWindowInner* aWindow,
    const nsTArray<RefPtr<WebExtensionContentScript>>& aScripts) {
  AutoTArray<RefPtr<Promise>, 8> promises;

  for (auto& script : aScripts) {
    if (RefPtr<Promise> promise = ExecuteContentScript(aWindow, *script)) {
      promises.AppendElement(std::move(promise));
    }
  }

  RefPtr<Promise> promise = Promise::All(aCx, promises, IgnoreErrors());
  MOZ_RELEASE_ASSERT(promise);
  return promise;
}

// Use browser's MessageManagerGroup to decide if we care about it, to inject
// extension APIs or content scripts.  Tabs use "browsers", and all custom
// extension browsers use "webext-browsers", including popups & sidebars,
// background & options pages, and xpcshell tests.
static bool IsTabOrExtensionBrowser(dom::BrowsingContext* aBC) {
  const auto& group = aBC->Top()->GetMessageManagerGroup();
  bool rv = group == u"browsers"_ns || group == u"webext-browsers"_ns;

#ifdef MOZ_THUNDERBIRD
  // ...unless it's Thunderbird, which has extra groups for unrelated reasons.
  rv = rv || group == u"single-site"_ns || group == u"single-page"_ns;
#endif

  return rv;
}

static nsTArray<RefPtr<dom::BrowsingContext>> GetAllInProcessContentBCs() {
  nsTArray<RefPtr<dom::BrowsingContext>> contentBCs;
  nsTArray<RefPtr<dom::BrowsingContextGroup>> groups;
  dom::BrowsingContextGroup::GetAllGroups(groups);
  for (const auto& group : groups) {
    for (const auto& toplevel : group->Toplevels()) {
      if (!toplevel->IsContent() || toplevel->IsDiscarded() ||
          !IsTabOrExtensionBrowser(toplevel)) {
        continue;
      }

      toplevel->PreOrderWalk([&](dom::BrowsingContext* aContext) {
        contentBCs.AppendElement(aContext);
      });
    }
  }
  return contentBCs;
}

nsresult ExtensionPolicyService::InjectContentScripts(
    WebExtensionPolicy* aExtension) {
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));

  auto contentBCs = GetAllInProcessContentBCs();
  for (dom::BrowsingContext* bc : contentBCs) {
    auto* win = bc->GetDOMWindow();

    if (bc->Top()->IsDiscarded() || !win || !win->GetDocumentURI()) {
      continue;
    }
    DocInfo docInfo(win);

    using RunAt = dom::ContentScriptRunAt;
    namespace RunAtValues = dom::ContentScriptRunAtValues;
    using Scripts = AutoTArray<RefPtr<WebExtensionContentScript>, 8>;

    Scripts scripts[RunAtValues::Count];

    auto GetScripts = [&](RunAt aRunAt) -> Scripts&& {
      static_assert(sizeof(aRunAt) == 1, "Our cast is wrong");
      return std::move(scripts[uint8_t(aRunAt)]);
    };

    for (const auto& script : aExtension->ContentScripts()) {
      if (script->Matches(docInfo)) {
        GetScripts(script->RunAt()).AppendElement(script);
      }
    }

    nsCOMPtr<nsPIDOMWindowInner> inner = win->GetCurrentInnerWindow();

    MOZ_TRY(ExecuteContentScripts(jsapi.cx(), inner,
                                  GetScripts(RunAt::Document_start))
                ->ThenWithCycleCollectedArgs(
                    [](JSContext* aCx, JS::HandleValue aValue,
                       ExtensionPolicyService* aSelf,
                       nsPIDOMWindowInner* aInner, Scripts&& aScripts) {
                      return aSelf->ExecuteContentScripts(aCx, aInner, aScripts)
                          .forget();
                    },
                    this, inner, GetScripts(RunAt::Document_end))
                .andThen([&](auto aPromise) {
                  return aPromise->ThenWithCycleCollectedArgs(
                      [](JSContext* aCx, JS::HandleValue aValue,
                         ExtensionPolicyService* aSelf,
                         nsPIDOMWindowInner* aInner, Scripts&& aScripts) {
                        return aSelf
                            ->ExecuteContentScripts(aCx, aInner, aScripts)
                            .forget();
                      },
                      this, inner, GetScripts(RunAt::Document_idle));
                }));
  }
  return NS_OK;
}

// Checks a request for matching content scripts, and begins pre-loading them
// if necessary.
void ExtensionPolicyService::CheckRequest(nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  auto loadType = loadInfo->GetExternalContentPolicyType();
  if (loadType != ExtContentPolicy::TYPE_DOCUMENT &&
      loadType != ExtContentPolicy::TYPE_SUBDOCUMENT) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(aChannel->GetURI(getter_AddRefs(uri)))) {
    return;
  }

  CheckContentScripts({uri.get(), loadInfo}, true);
}

static bool CheckParentFrames(nsPIDOMWindowOuter* aWindow,
                              WebExtensionPolicy& aPolicy) {
  nsCOMPtr<nsIURI> aboutAddons;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(aboutAddons), "about:addons"))) {
    return false;
  }
  nsCOMPtr<nsIURI> htmlAboutAddons;
  if (NS_FAILED(
          NS_NewURI(getter_AddRefs(htmlAboutAddons),
                    "chrome://mozapps/content/extensions/aboutaddons.html"))) {
    return false;
  }

  dom::WindowContext* wc = aWindow->GetCurrentInnerWindow()->GetWindowContext();
  while ((wc = wc->GetParentWindowContext())) {
    if (!wc->IsInProcess()) {
      return false;
    }

    nsGlobalWindowInner* win = wc->GetInnerWindow();

    auto* principal = BasePrincipal::Cast(win->GetPrincipal());
    if (principal->IsSystemPrincipal()) {
      // The add-on manager is a special case, since it contains extension
      // options pages in same-type <browser> frames.
      nsIURI* uri = win->GetDocumentURI();
      bool equals;
      if ((NS_SUCCEEDED(uri->Equals(aboutAddons, &equals)) && equals) ||
          (NS_SUCCEEDED(uri->Equals(htmlAboutAddons, &equals)) && equals)) {
        return true;
      }
    }

    if (principal->AddonPolicy() != &aPolicy) {
      return false;
    }
  }

  return true;
}

// Checks a document, just after the document element has been inserted, for
// matching content scripts or extension principals, and loads them if
// necessary.
void ExtensionPolicyService::CheckDocument(Document* aDocument) {
  nsCOMPtr<nsPIDOMWindowOuter> win = aDocument->GetWindow();
  if (win) {
    if (!IsTabOrExtensionBrowser(win->GetBrowsingContext())) {
      return;
    }

    if (win->GetDocumentURI()) {
      CheckContentScripts(win.get(), false);
    }

    nsIPrincipal* principal = aDocument->NodePrincipal();

    RefPtr<WebExtensionPolicy> policy =
        BasePrincipal::Cast(principal)->AddonPolicy();
    if (policy) {
      bool privileged = IsExtensionProcess() && CheckParentFrames(win, *policy);

      ProcessScript().InitExtensionDocument(policy, aDocument, privileged);
    }
  }
}

void ExtensionPolicyService::CheckContentScripts(const DocInfo& aDocInfo,
                                                 bool aIsPreload) {
  nsCOMPtr<nsPIDOMWindowInner> win;
  if (!aIsPreload) {
    win = aDocInfo.GetWindow()->GetCurrentInnerWindow();
  }

  nsTArray<RefPtr<WebExtensionContentScript>> scriptsToLoad;

  for (RefPtr<WebExtensionPolicy> policy : mExtensions.Values()) {
    for (auto& script : policy->ContentScripts()) {
      if (script->Matches(aDocInfo)) {
        if (aIsPreload) {
          ProcessScript().PreloadContentScript(script);
        } else {
          // Collect the content scripts to load instead of loading them
          // right away (to prevent a loaded content script from being
          // able to invalidate the iterator by triggering a call to
          // policy->UnregisterContentScript while we are still iterating
          // over all its content scripts). See Bug 1593240.
          scriptsToLoad.AppendElement(script);
        }
      }
    }

    for (auto& script : scriptsToLoad) {
      if (!win->IsCurrentInnerWindow()) {
        break;
      }

      RefPtr<Promise> promise;
      ProcessScript().LoadContentScript(script, win, getter_AddRefs(promise));
    }

    scriptsToLoad.ClearAndRetainStorage();
  }

  for (RefPtr<DocumentObserver> observer : mObservers.Values()) {
    for (auto& matcher : observer->Matchers()) {
      if (matcher->Matches(aDocInfo)) {
        if (aIsPreload) {
          observer->NotifyMatch(*matcher, aDocInfo.GetLoadInfo());
        } else {
          observer->NotifyMatch(*matcher, aDocInfo.GetWindow());
        }
      }
    }
  }
}

/*****************************************************************************
 * nsIAddonPolicyService
 *****************************************************************************/

nsresult ExtensionPolicyService::GetDefaultCSP(nsAString& aDefaultCSP) {
  if (mDefaultCSP.IsVoid()) {
    nsresult rv = Preferences::GetString(DEFAULT_CSP_PREF, mDefaultCSP);
    if (NS_FAILED(rv)) {
      mDefaultCSP.AssignLiteral(DEFAULT_DEFAULT_CSP);
    }
    mDefaultCSP.SetIsVoid(false);
  }

  aDefaultCSP.Assign(mDefaultCSP);
  return NS_OK;
}

nsresult ExtensionPolicyService::GetBaseCSP(const nsAString& aAddonId,
                                            nsAString& aResult) {
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    policy->GetBaseCSP(aResult);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult ExtensionPolicyService::GetExtensionPageCSP(const nsAString& aAddonId,
                                                     nsAString& aResult) {
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    policy->GetExtensionPageCSP(aResult);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult ExtensionPolicyService::GetGeneratedBackgroundPageUrl(
    const nsACString& aHostname, nsACString& aResult) {
  if (WebExtensionPolicy* policy = GetByHost(aHostname)) {
    nsAutoCString url("data:text/html,");

    nsCString html = policy->BackgroundPageHTML();
    nsAutoCString escaped;

    url.Append(NS_EscapeURL(html, esc_Minimal, escaped));

    aResult = url;
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult ExtensionPolicyService::AddonHasPermission(const nsAString& aAddonId,
                                                    const nsAString& aPerm,
                                                    bool* aResult) {
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    *aResult = policy->HasPermission(aPerm);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult ExtensionPolicyService::AddonMayLoadURI(const nsAString& aAddonId,
                                                 nsIURI* aURI, bool aExplicit,
                                                 bool* aResult) {
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    *aResult = policy->CanAccessURI(aURI, aExplicit);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult ExtensionPolicyService::GetExtensionName(const nsAString& aAddonId,
                                                  nsAString& aName) {
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    aName.Assign(policy->Name());
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult ExtensionPolicyService::SourceMayLoadExtensionURI(
    nsIURI* aSourceURI, nsIURI* aExtensionURI, bool* aResult) {
  URLInfo source(aSourceURI);
  URLInfo url(aExtensionURI);
  if (WebExtensionPolicy* policy = GetByURL(url)) {
    *aResult = policy->SourceMayAccessPath(source, url.FilePath());
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult ExtensionPolicyService::ExtensionURIToAddonId(nsIURI* aURI,
                                                       nsAString& aResult) {
  if (WebExtensionPolicy* policy = GetByURL(aURI)) {
    policy->GetId(aResult);
  } else {
    aResult.SetIsVoid(true);
  }
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION(ExtensionPolicyService, mExtensions, mExtensionHosts,
                         mObservers)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionPolicyService)
  NS_INTERFACE_MAP_ENTRY(nsIAddonPolicyService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMemoryReporter)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAddonPolicyService)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionPolicyService)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionPolicyService)

}  // namespace mozilla
