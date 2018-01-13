/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/extensions/WebExtensionContentScript.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozIExtensionProcessScript.h"
#include "nsEscape.h"
#include "nsGkAtoms.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsILoadInfo.h"
#include "nsIXULRuntime.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsXULAppAPI.h"

namespace mozilla {

using namespace extensions;

#define DEFAULT_BASE_CSP \
    "script-src 'self' https://* moz-extension: blob: filesystem: 'unsafe-eval' 'unsafe-inline'; " \
    "object-src 'self' https://* moz-extension: blob: filesystem:;"

#define DEFAULT_DEFAULT_CSP \
    "script-src 'self'; object-src 'self';"


#define OBS_TOPIC_PRELOAD_SCRIPT "web-extension-preload-content-script"
#define OBS_TOPIC_LOAD_SCRIPT "web-extension-load-content-script"


static mozIExtensionProcessScript&
ProcessScript()
{
  static nsCOMPtr<mozIExtensionProcessScript> sProcessScript;

  if (MOZ_UNLIKELY(!sProcessScript)) {
    sProcessScript = do_GetService("@mozilla.org/webextensions/extension-process-script;1");
    MOZ_RELEASE_ASSERT(sProcessScript);
    ClearOnShutdown(&sProcessScript);
  }
  return *sProcessScript;
}

/*****************************************************************************
 * ExtensionPolicyService
 *****************************************************************************/

/* static */ bool ExtensionPolicyService::sRemoteExtensions;

/* static */ ExtensionPolicyService&
ExtensionPolicyService::GetSingleton()
{
  static RefPtr<ExtensionPolicyService> sExtensionPolicyService;

  if (MOZ_UNLIKELY(!sExtensionPolicyService)) {
    sExtensionPolicyService = new ExtensionPolicyService();
    RegisterWeakMemoryReporter(sExtensionPolicyService);
    ClearOnShutdown(&sExtensionPolicyService);
  }
  return *sExtensionPolicyService.get();
}

ExtensionPolicyService::ExtensionPolicyService()
{
  mObs = services::GetObserverService();
  MOZ_RELEASE_ASSERT(mObs);

  Preferences::AddBoolVarCache(&sRemoteExtensions, "extensions.webextensions.remote", false);

  RegisterObservers();
}

ExtensionPolicyService::~ExtensionPolicyService()
{
  UnregisterWeakMemoryReporter(this);
}

bool
ExtensionPolicyService::UseRemoteExtensions() const
{
  return sRemoteExtensions && BrowserTabsRemoteAutostart();
}

bool
ExtensionPolicyService::IsExtensionProcess() const
{
  bool isRemote = UseRemoteExtensions();

  if (isRemote && XRE_IsContentProcess()) {
    auto& remoteType = dom::ContentChild::GetSingleton()->GetRemoteType();
    return remoteType.EqualsLiteral(EXTENSION_REMOTE_TYPE);
  }
  return !isRemote && XRE_IsParentProcess();
}


WebExtensionPolicy*
ExtensionPolicyService::GetByURL(const URLInfo& aURL)
{
  if (aURL.Scheme() == nsGkAtoms::moz_extension) {
    return GetByHost(aURL.Host());
  }
  return nullptr;
}

void
ExtensionPolicyService::GetAll(nsTArray<RefPtr<WebExtensionPolicy>>& aResult)
{
  for (auto iter = mExtensions.Iter(); !iter.Done(); iter.Next()) {
    aResult.AppendElement(iter.Data());
  }
}

bool
ExtensionPolicyService::RegisterExtension(WebExtensionPolicy& aPolicy)
{
  bool ok = (!GetByID(aPolicy.Id()) &&
             !GetByHost(aPolicy.MozExtensionHostname()));
  MOZ_ASSERT(ok);

  if (!ok) {
    return false;
  }

  mExtensions.Put(aPolicy.Id(), &aPolicy);
  mExtensionHosts.Put(aPolicy.MozExtensionHostname(), &aPolicy);
  return true;
}

bool
ExtensionPolicyService::UnregisterExtension(WebExtensionPolicy& aPolicy)
{
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


void
ExtensionPolicyService::BaseCSP(nsAString& aBaseCSP) const
{
  nsresult rv;

  rv = Preferences::GetString("extensions.webextensions.base-content-security-policy", aBaseCSP);
  if (NS_FAILED(rv)) {
    aBaseCSP.AssignLiteral(DEFAULT_BASE_CSP);
  }
}

void
ExtensionPolicyService::DefaultCSP(nsAString& aDefaultCSP) const
{
  nsresult rv;

  rv = Preferences::GetString("extensions.webextensions.default-content-security-policy", aDefaultCSP);
  if (NS_FAILED(rv)) {
    aDefaultCSP.AssignLiteral(DEFAULT_DEFAULT_CSP);
  }
}


/*****************************************************************************
 * nsIMemoryReporter
 *****************************************************************************/

NS_IMETHODIMP
ExtensionPolicyService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData, bool aAnonymize)
{
  for (auto iter = mExtensions.Iter(); !iter.Done(); iter.Next()) {
    auto& ext = iter.Data();

    nsAtomCString id(ext->Id());

    NS_ConvertUTF16toUTF8 name(ext->Name());
    name.ReplaceSubstring("\"", "");
    name.ReplaceSubstring("\\", "");

    nsString url;
    MOZ_TRY_VAR(url, ext->GetURL(NS_LITERAL_STRING("")));

    nsPrintfCString desc("Extension(id=%s, name=\"%s\", baseURL=%s)",
                         id.get(), name.get(),
                         NS_ConvertUTF16toUTF8(url).get());
    desc.ReplaceChar('/', '\\');

    nsCString path("extensions/");
    path.Append(desc);

    aHandleReport->Callback(
      EmptyCString(), path,
      KIND_NONHEAP, UNITS_COUNT, 1,
      NS_LITERAL_CSTRING("WebExtensions that are active in this session"),
      aData);
  }

  return NS_OK;
}


/*****************************************************************************
 * Content script management
 *****************************************************************************/

void
ExtensionPolicyService::RegisterObservers()
{
  mObs->AddObserver(this, "content-document-global-created", false);
  mObs->AddObserver(this, "document-element-inserted", false);
  if (XRE_IsContentProcess()) {
    mObs->AddObserver(this, "http-on-opening-request", false);
  }
}

void
ExtensionPolicyService::UnregisterObservers()
{
  mObs->RemoveObserver(this, "content-document-global-created");
  mObs->RemoveObserver(this, "document-element-inserted");
  if (XRE_IsContentProcess()) {
    mObs->RemoveObserver(this, "http-on-opening-request");
  }
}

nsresult
ExtensionPolicyService::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (!strcmp(aTopic, "content-document-global-created")) {
    nsCOMPtr<nsPIDOMWindowOuter> win = do_QueryInterface(aSubject);
    if (win) {
      CheckWindow(win);
    }
  } else if (!strcmp(aTopic, "document-element-inserted")) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aSubject);
    if (doc) {
      CheckDocument(doc);
    }
  } else if (!strcmp(aTopic, "http-on-opening-request")) {
    nsCOMPtr<nsIChannel> chan = do_QueryInterface(aSubject);
    if (chan) {
      CheckRequest(chan);
    }
  }
  return NS_OK;
}

// Checks a request for matching content scripts, and begins pre-loading them
// if necessary.
void
ExtensionPolicyService::CheckRequest(nsIChannel* aChannel)
{
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    return;
  }

  auto loadType = loadInfo->GetExternalContentPolicyType();
  if (loadType != nsIContentPolicy::TYPE_DOCUMENT &&
      loadType != nsIContentPolicy::TYPE_SUBDOCUMENT) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(aChannel->GetURI(getter_AddRefs(uri)))) {
    return;
  }

  CheckContentScripts({uri.get(), loadInfo}, true);
}

// Checks a document, just after the document element has been inserted, for
// matching content scripts or extension principals, and loads them if
// necessary.
void
ExtensionPolicyService::CheckDocument(nsIDocument* aDocument)
{
  nsCOMPtr<nsPIDOMWindowOuter> win = aDocument->GetWindow();
  if (win) {
    if (win->GetDocumentURI()) {
      CheckContentScripts(win.get(), false);
    }

    nsIPrincipal* principal = aDocument->NodePrincipal();

    RefPtr<WebExtensionPolicy> policy = BasePrincipal::Cast(principal)->AddonPolicy();
    if (policy) {
      nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDocument);
      ProcessScript().InitExtensionDocument(policy, doc);
    }
  }
}

// Checks for loads of about:blank into new window globals, and loads any
// matching content scripts. about:blank loads do not trigger document element
// inserted events, so they're the only load type that are special cased this
// way.
void
ExtensionPolicyService::CheckWindow(nsPIDOMWindowOuter* aWindow)
{
  // We only care about non-initial document loads here. The initial
  // about:blank document will usually be re-used to load another document.
  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (!doc || doc->IsInitialDocument() ||
      doc->GetReadyStateEnum() == nsIDocument::READYSTATE_UNINITIALIZED) {
    return;
  }

  nsCOMPtr<nsIURI> docUri = doc->GetDocumentURI();
  nsCOMPtr<nsIURI> uri;
  if (!docUri || NS_FAILED(docUri->CloneIgnoringRef(getter_AddRefs(uri))) ||
      !NS_IsAboutBlank(uri)) {
    return;
  }

  CheckContentScripts(aWindow, false);
}

void
ExtensionPolicyService::CheckContentScripts(const DocInfo& aDocInfo, bool aIsPreload)
{
  for (auto iter = mExtensions.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<WebExtensionPolicy> policy = iter.Data();

    for (auto& script : policy->ContentScripts()) {
      if (script->Matches(aDocInfo)) {
        if (aIsPreload) {
          ProcessScript().PreloadContentScript(script);
        } else {
          ProcessScript().LoadContentScript(script, aDocInfo.GetWindow());
        }
      }
    }
  }
}


/*****************************************************************************
 * nsIAddonPolicyService
 *****************************************************************************/

nsresult
ExtensionPolicyService::GetBaseCSP(nsAString& aBaseCSP)
{
  BaseCSP(aBaseCSP);
  return NS_OK;
}

nsresult
ExtensionPolicyService::GetDefaultCSP(nsAString& aDefaultCSP)
{
  DefaultCSP(aDefaultCSP);
  return NS_OK;
}

nsresult
ExtensionPolicyService::GetAddonCSP(const nsAString& aAddonId,
                                    nsAString& aResult)
{
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    policy->GetContentSecurityPolicy(aResult);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::GetGeneratedBackgroundPageUrl(const nsACString& aHostname,
                                                      nsACString& aResult)
{
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

nsresult
ExtensionPolicyService::AddonHasPermission(const nsAString& aAddonId,
                                           const nsAString& aPerm,
                                           bool* aResult)
{
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    *aResult = policy->HasPermission(aPerm);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::AddonMayLoadURI(const nsAString& aAddonId,
                                        nsIURI* aURI,
                                        bool aExplicit,
                                        bool* aResult)
{
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    *aResult = policy->CanAccessURI(aURI, aExplicit);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::GetExtensionName(const nsAString& aAddonId,
                                         nsAString& aName)
{
  if (WebExtensionPolicy* policy = GetByID(aAddonId)) {
    aName.Assign(policy->Name());
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::ExtensionURILoadableByAnyone(nsIURI* aURI, bool* aResult)
{
  URLInfo url(aURI);
  if (WebExtensionPolicy* policy = GetByURL(url)) {
    *aResult = policy->IsPathWebAccessible(url.FilePath());
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
ExtensionPolicyService::ExtensionURIToAddonId(nsIURI* aURI, nsAString& aResult)
{
  if (WebExtensionPolicy* policy = GetByURL(aURI)) {
    policy->GetId(aResult);
  } else {
    aResult.SetIsVoid(true);
  }
  return NS_OK;
}


NS_IMPL_CYCLE_COLLECTION(ExtensionPolicyService, mExtensions, mExtensionHosts)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionPolicyService)
  NS_INTERFACE_MAP_ENTRY(nsIAddonPolicyService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMemoryReporter)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAddonPolicyService)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionPolicyService)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionPolicyService)

} // namespace mozilla
