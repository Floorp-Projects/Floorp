/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowserSessionStoreBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/SessionStoreListener.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/SessionStoreUtilsBinding.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsGenericHTMLElement.h"
#include "nsDocShell.h"
#include "nsIAppWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsImportModule.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsITimer.h"
#include "nsIWebProgress.h"
#include "nsIXPConnect.h"
#include "nsIXULRuntime.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "SessionStoreFunctions.h"

using namespace mozilla;
using namespace mozilla::dom;

// This pref controls whether or not we send updates to the parent on a timeout
// or not, and should only be used for tests or debugging.
static const char kTimeOutDisable[] =
    "browser.sessionstore.debug.no_auto_updates";
// Timeout for waiting an idle period to send data.
static const char kPrefInterval[] = "browser.sessionstore.interval";

NS_IMPL_CYCLE_COLLECTION(ContentSessionStore, mDocShell)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ContentSessionStore, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ContentSessionStore, Release)

ContentSessionStore::ContentSessionStore(nsIDocShell* aDocShell)
    : mDocShell(aDocShell),
      mPrivateChanged(false),
      mIsPrivate(false),
      mDocCapChanged(false),
      mSHistoryChanged(false) {
  MOZ_ASSERT(mDocShell);
  // Check that value at startup as it might have
  // been set before the frame script was loaded.
  if (NS_SUCCEEDED(nsDocShell::Cast(mDocShell)->GetUsePrivateBrowsing(
          &mPrivateChanged)) &&
      mPrivateChanged) {
    mIsPrivate = true;
  }
}

nsCString ContentSessionStore::CollectDocShellCapabilities() {
  bool allow;
  nsCString aRetVal;

#define TRY_ALLOWPROP(y)                        \
  PR_BEGIN_MACRO                                \
  nsresult rv = mDocShell->GetAllow##y(&allow); \
  if (NS_SUCCEEDED(rv) && !allow) {             \
    if (!aRetVal.IsEmpty()) {                   \
      aRetVal.Append(',');                      \
    }                                           \
    aRetVal.Append(#y);                         \
  }                                             \
  PR_END_MACRO

  TRY_ALLOWPROP(Plugins);
  // Bug 1328013 : Don't collect "AllowJavascript" property
  // TRY_ALLOWPROP(Javascript);
  TRY_ALLOWPROP(MetaRedirects);
  TRY_ALLOWPROP(Subframes);
  TRY_ALLOWPROP(Images);
  TRY_ALLOWPROP(Media);
  TRY_ALLOWPROP(DNSPrefetch);
  TRY_ALLOWPROP(WindowControl);
  TRY_ALLOWPROP(Auth);
  TRY_ALLOWPROP(ContentRetargeting);
  TRY_ALLOWPROP(ContentRetargetingOnChildren);
#undef TRY_ALLOWPROP
  return aRetVal;
}

void ContentSessionStore::OnPrivateModeChanged(bool aEnabled) {
  mPrivateChanged = true;
  mIsPrivate = aEnabled;
}

nsCString ContentSessionStore::GetDocShellCaps() {
  mDocCapChanged = false;
  return mDocCaps;
}

bool ContentSessionStore::GetPrivateModeEnabled() {
  mPrivateChanged = false;
  return mIsPrivate;
}

void ContentSessionStore::SetSHistoryChanged() {
  mSHistoryChanged = mozilla::SessionHistoryInParent();
}

void ContentSessionStore::OnDocumentStart() {
  nsCString caps = CollectDocShellCapabilities();
  if (!mDocCaps.Equals(caps)) {
    mDocCaps = caps;
    mDocCapChanged = true;
  }

  if (mozilla::SessionHistoryInParent()) {
    mSHistoryChanged = true;
  }
}

void ContentSessionStore::OnDocumentEnd() {
  if (mozilla::SessionHistoryInParent()) {
    mSHistoryChanged = true;
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TabListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIPrivacyTransitionObserver)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WEAK(TabListener, mDocShell, mSessionStore,
                              mOwnerContent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TabListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TabListener)

TabListener::TabListener(nsIDocShell* aDocShell, Element* aElement)
    : mDocShell(aDocShell),
      mSessionStore(new ContentSessionStore(aDocShell)),
      mOwnerContent(aElement),
      mProgressListenerRegistered(false),
      mEventListenerRegistered(false),
      mPrefObserverRegistered(false),
      mUpdatedTimer(nullptr),
      mTimeoutDisabled(false),
      mUpdateInterval(15000),
      mEpoch(0) {
  MOZ_ASSERT(mDocShell);
}

EventTarget* TabListener::GetEventTarget() {
  if (mOwnerContent) {
    return mOwnerContent;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(mDocShell);
  if (window) {
    return window->GetChromeEventHandler();
  }

  return nullptr;
}

nsresult TabListener::Init() {
  TabListener::UpdateSessionStore();
  nsresult rv = mDocShell->AddWeakPrivacyTransitionObserver(this);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWebProgress> webProgress = do_QueryInterface(mDocShell);
  rv = webProgress->AddProgressListener(this,
                                        nsIWebProgress::NOTIFY_STATE_DOCUMENT);
  NS_ENSURE_SUCCESS(rv, rv);
  mProgressListenerRegistered = true;

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(prefBranch, "no prefservice");
  if (prefBranch) {
    prefBranch->AddObserver(kTimeOutDisable, this, true);
    prefBranch->AddObserver(kPrefInterval, this, true);
    mPrefObserverRegistered = true;
  }

  AddEventListeners();
  return NS_OK;
}

void TabListener::AddEventListeners() {
  if (nsCOMPtr<EventTarget> eventTarget = GetEventTarget()) {
    if (mozilla::SessionHistoryInParent()) {
      eventTarget->AddSystemEventListener(u"DOMTitleChanged"_ns, this, false);
    }
    mEventListenerRegistered = true;
  }
}

void TabListener::RemoveEventListeners() {
  if (nsCOMPtr<EventTarget> eventTarget = GetEventTarget()) {
    if (mEventListenerRegistered) {
      if (mozilla::SessionHistoryInParent()) {
        eventTarget->RemoveSystemEventListener(u"DOMTitleChanged"_ns, this,
                                               false);
      }
      mEventListenerRegistered = false;
    }
  }
}

void TabListener::SetOwnerContent(Element* aElement) {
  MOZ_DIAGNOSTIC_ASSERT(aElement);
  RemoveEventListeners();
  mOwnerContent = aElement;
  AddEventListeners();
}

/* static */
void TabListener::TimerCallback(nsITimer* aTimer, void* aClosure) {
  auto listener = static_cast<TabListener*>(aClosure);
  listener->UpdateSessionStore();
  listener->StopTimerForUpdate();
}

void TabListener::StopTimerForUpdate() {
  if (mUpdatedTimer) {
    mUpdatedTimer->Cancel();
    mUpdatedTimer = nullptr;
  }
}

void TabListener::AddTimerForUpdate() {
  if (mUpdatedTimer) {
    return;
  }

  if (mTimeoutDisabled) {
    UpdateSessionStore();
    return;
  }

  NS_NewTimerWithFuncCallback(getter_AddRefs(mUpdatedTimer), TimerCallback,
                              this, mUpdateInterval, nsITimer::TYPE_ONE_SHOT,
                              "TabListener::TimerCallback");
}

NS_IMETHODIMP TabListener::PrivateModeChanged(bool aEnabled) {
  mSessionStore->OnPrivateModeChanged(aEnabled);
  AddTimerForUpdate();
  return NS_OK;
}

NS_IMETHODIMP TabListener::OnStateChange(nsIWebProgress* aWebProgress,
                                         nsIRequest* aRequest,
                                         uint32_t aStateFlags,
                                         nsresult aStatus) {
  if (!mSessionStore) {
    return NS_OK;
  }

  // Ignore state changes for subframes because we're only interested in the
  // top-document starting or stopping its load.
  bool isTopLevel = false;
  nsresult rv = aWebProgress->GetIsTopLevel(&isTopLevel);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isTopLevel) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebProgress> webProgress = do_QueryInterface(mDocShell);
  if (webProgress != aWebProgress) {
    return NS_OK;
  }

  // onStateChange will be fired when loading the initial about:blank URI for
  // a browser, which we don't actually care about. This is particularly for
  // the case of unrestored background tabs, where the content has not yet
  // been restored: we don't want to accidentally send any updates to the
  // parent when the about:blank placeholder page has loaded.
  if (!mDocShell->GetHasLoadedNonBlankURI()) {
    return NS_OK;
  }

  if (aStateFlags & (nsIWebProgressListener::STATE_START)) {
    mSessionStore->OnDocumentStart();
  } else if (aStateFlags & (nsIWebProgressListener::STATE_STOP)) {
    mSessionStore->OnDocumentEnd();
  }

  return NS_OK;
}

NS_IMETHODIMP
TabListener::HandleEvent(Event* aEvent) {
  EventTarget* target = aEvent->GetTarget();
  if (!target) {
    return NS_OK;
  }

  nsPIDOMWindowOuter* outer = target->GetOwnerGlobalForBindingsInternal();
  if (!outer || !outer->GetDocShell()) {
    return NS_OK;
  }

  RefPtr<BrowsingContext> context = outer->GetBrowsingContext();
  if (!context || context->CreatedDynamically()) {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("DOMTitleChanged")) {
    mSessionStore->SetSHistoryChanged();
    AddTimerForUpdate();
  }

  return NS_OK;
}

NS_IMETHODIMP TabListener::OnProgressChange(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            int32_t aCurSelfProgress,
                                            int32_t aMaxSelfProgress,
                                            int32_t aCurTotalProgress,
                                            int32_t aMaxTotalProgress) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP TabListener::OnLocationChange(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            nsIURI* aLocation,
                                            uint32_t aFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP TabListener::OnStatusChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          nsresult aStatus,
                                          const char16_t* aMessage) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP TabListener::OnSecurityChange(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            uint32_t aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP TabListener::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                                  nsIRequest* aRequest,
                                                  uint32_t aEvent) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult TabListener::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);

    bool timeoutDisabled;
    if (NS_SUCCEEDED(
            prefBranch->GetBoolPref(kTimeOutDisable, &timeoutDisabled))) {
      if (mTimeoutDisabled != timeoutDisabled) {
        mTimeoutDisabled = timeoutDisabled;
        if (mUpdatedTimer) {
          StopTimerForUpdate();
          AddTimerForUpdate();
        }
      }
    }

    int32_t interval = 0;
    if (NS_SUCCEEDED(prefBranch->GetIntPref(kPrefInterval, &interval))) {
      if (mUpdateInterval != interval) {
        mUpdateInterval = interval;
        if (mUpdatedTimer) {
          StopTimerForUpdate();
          AddTimerForUpdate();
        }
      }
    }
    return NS_OK;
  }

  NS_ERROR("Unexpected topic");
  return NS_ERROR_UNEXPECTED;
}

void TabListener::ForceFlushFromParent() {
  if (!XRE_IsParentProcess()) {
    return;
  }
  if (!mSessionStore) {
    return;
  }

  UpdateSessionStore(true);
}

void TabListener::UpdateSessionStore(bool aIsFlush) {
  if (!aIsFlush) {
    if (!mSessionStore || !mSessionStore->UpdateNeeded()) {
      return;
    }
  }

  if (!XRE_IsParentProcess()) {
    BrowserChild* browserChild = BrowserChild::GetFrom(mDocShell);
    if (browserChild) {
      StopTimerForUpdate();
      browserChild->UpdateSessionStore();
    }
    return;
  }

  BrowsingContext* context = mDocShell->GetBrowsingContext();
  if (!context) {
    return;
  }

  uint32_t chromeFlags = 0;
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner) {
    return;
  }
  nsCOMPtr<nsIAppWindow> window(do_GetInterface(treeOwner));
  if (!window) {
    return;
  }
  if (window && NS_FAILED(window->GetChromeFlags(&chromeFlags))) {
    return;
  }

  UpdateSessionStoreData data;
  if (mSessionStore->IsDocCapChanged()) {
    data.mDisallow.Construct() = mSessionStore->GetDocShellCaps();
  }
  if (mSessionStore->IsPrivateChanged()) {
    data.mIsPrivate.Construct() = mSessionStore->GetPrivateModeEnabled();
  }

  nsCOMPtr<nsISessionStoreFunctions> funcs = do_ImportModule(
      "resource://gre/modules/SessionStoreFunctions.jsm", fallible);
  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(funcs);
  if (!wrapped) {
    return;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(wrapped->GetJSObjectGlobal())) {
    return;
  }

  JS::Rooted<JS::Value> update(jsapi.cx());
  if (!ToJSValue(jsapi.cx(), data, &update)) {
    return;
  }

  JS::Rooted<JS::Value> key(jsapi.cx(),
                            context->Canonical()->Top()->PermanentKey());

  nsresult rv = funcs->UpdateSessionStore(
      mOwnerContent, context, key, mEpoch,
      mSessionStore->GetAndClearSHistoryChanged(), update);
  if (NS_FAILED(rv)) {
    return;
  }

  StopTimerForUpdate();
}

void TabListener::RemoveListeners() {
  if (mProgressListenerRegistered) {
    nsCOMPtr<nsIWebProgress> webProgress = do_QueryInterface(mDocShell);
    if (webProgress) {
      webProgress->RemoveProgressListener(this);
      mProgressListenerRegistered = false;
    }
  }

  RemoveEventListeners();

  if (mPrefObserverRegistered) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (!obs) {
      return;
    }
    if (mPrefObserverRegistered) {
      obs->RemoveObserver(this, kTimeOutDisable);
      obs->RemoveObserver(this, kPrefInterval);
      mPrefObserverRegistered = false;
    }
  }
}

TabListener::~TabListener() { RemoveListeners(); }
