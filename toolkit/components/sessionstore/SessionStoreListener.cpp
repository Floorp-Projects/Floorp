/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/SessionStoreListener.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/SessionStoreUtilsBinding.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/StaticPrefs_fission.h"
#include "nsGenericHTMLElement.h"
#include "nsDocShell.h"
#include "nsIAppWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsImportModule.h"
#include "nsIPrefBranch.h"
#include "nsITimer.h"
#include "nsIWebProgress.h"
#include "nsIXPConnect.h"
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
      mScrollChanged(NO_CHANGE),
      mFormDataChanged(NO_CHANGE),
      mStorageStatus(NO_STORAGE),
      mDocCapChanged(false),
      mSHistoryInParent(StaticPrefs::fission_sessionHistoryInParent()),
      mSHistoryChanged(false),
      mSHistoryChangedFromParent(false) {
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

void ContentSessionStore::SetFullStorageNeeded() {
  // We need the entire session storage, reset the pending individual change
  ResetStorageChanges();
  mStorageStatus = FULLSTORAGE;
}

void ContentSessionStore::ResetStorageChanges() {
  mOrigins.Clear();
  mKeys.Clear();
  mValues.Clear();
}

void ContentSessionStore::OnDocumentStart() {
  mScrollChanged = PAGELOADEDSTART;
  mFormDataChanged = PAGELOADEDSTART;
  nsCString caps = CollectDocShellCapabilities();
  if (!mDocCaps.Equals(caps)) {
    mDocCaps = caps;
    mDocCapChanged = true;
  }

  SetFullStorageNeeded();

  if (mSHistoryInParent) {
    mSHistoryChanged = true;
  }
}

void ContentSessionStore::OnDocumentEnd() {
  mScrollChanged = WITH_CHANGE;
  SetFullStorageNeeded();

  if (mSHistoryInParent) {
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
      mStorageObserverRegistered(false),
      mStorageChangeListenerRegistered(false),
      mUpdatedTimer(nullptr),
      mTimeoutDisabled(false),
      mUpdateInterval(15000),
      mEpoch(0),
      mSHistoryInParent(StaticPrefs::fission_sessionHistoryInParent()) {
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

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  NS_WARNING_ASSERTION(obs, "no observer service");
  if (obs) {
    obs->AddObserver(this, "browser:purge-sessionStorage", true);
    mStorageObserverRegistered = true;
  }

  nsCOMPtr<EventTarget> eventTarget = GetEventTarget();
  if (!eventTarget) {
    return NS_OK;
  }
  eventTarget->AddSystemEventListener(u"mozvisualscroll"_ns, this, false);
  eventTarget->AddSystemEventListener(u"input"_ns, this, false);

  if (mSHistoryInParent) {
    eventTarget->AddSystemEventListener(u"DOMTitleChanged"_ns, this, false);
  }

  mEventListenerRegistered = true;
  eventTarget->AddSystemEventListener(u"MozSessionStorageChanged"_ns, this,
                                      false);
  mStorageChangeListenerRegistered = true;
  return NS_OK;
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
    ResetStorageChangeListener();
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
  if (!outer) {
    return NS_OK;
  }

  nsIDocShell* docShell = outer->GetDocShell();
  if (!docShell) {
    return NS_OK;
  }

  bool isDynamic = false;
  nsresult rv = docShell->GetCreatedDynamically(&isDynamic);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  if (isDynamic) {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("mozvisualscroll")) {
    mSessionStore->SetScrollPositionChanged();
    AddTimerForUpdate();
  } else if (eventType.EqualsLiteral("input")) {
    mSessionStore->SetFormDataChanged();
    AddTimerForUpdate();
  } else if (eventType.EqualsLiteral("MozSessionStorageChanged")) {
    auto event = static_cast<StorageEvent*>(aEvent);
    RefPtr<Storage> changingStorage = event->GetStorageArea();
    if (!changingStorage) {
      return NS_OK;
    }
    // How much data does DOMSessionStorage contain?
    int64_t storageUsage = changingStorage->GetOriginQuotaUsage();
    if (storageUsage > StaticPrefs::browser_sessionstore_dom_storage_limit()) {
      RemoveStorageChangeListener();
      mSessionStore->ResetStorageChanges();
      mSessionStore->ResetStorage();
      return NS_OK;
    }
    if (mSessionStore->AppendSessionStorageChange(event)) {
      AddTimerForUpdate();
    }
  } else if (eventType.EqualsLiteral("DOMTitleChanged")) {
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
  if (!strcmp(aTopic, "browser:purge-sessionStorage")) {
    mSessionStore->SetFullStorageNeeded();
    AddTimerForUpdate();
    return NS_OK;
  }

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

nsCString CollectPosition(Document& aDocument) {
  PresShell* presShell = aDocument.GetPresShell();
  if (!presShell) {
    return EmptyCString();
  }
  nsPoint scrollPos = presShell->GetVisualViewportOffset();
  int scrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  int scrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);
  if ((scrollX != 0) || (scrollY != 0)) {
    return nsPrintfCString("%d,%d", scrollX, scrollY);
  }

  return EmptyCString();
}

int CollectPositions(BrowsingContext* aBrowsingContext,
                     nsTArray<nsCString>& aPositions,
                     nsTArray<int32_t>& aPositionDescendants) {
  nsPIDOMWindowOuter* window = aBrowsingContext->GetDOMWindow();
  if (!window) {
    return 0;
  }

  nsIDocShell* docShell = window->GetDocShell();
  if (!docShell || docShell->GetCreatedDynamically()) {
    return 0;
  }

  Document* document = window->GetDoc();
  if (!document) {
    return 0;
  }

  /* Collect data from current frame */
  aPositions.AppendElement(CollectPosition(*document));
  aPositionDescendants.AppendElement(0);
  unsigned long currentIdx = aPositions.Length() - 1;

  /* Collect data from all child frame */
  // This is not going to work for fission. Bug 1572084 for tracking it.
  for (auto& child : aBrowsingContext->Children()) {
    aPositionDescendants[currentIdx] +=
        CollectPositions(child, aPositions, aPositionDescendants);
  }

  return aPositionDescendants[currentIdx] + 1;
}

void ContentSessionStore::GetScrollPositions(
    nsTArray<nsCString>& aPositions, nsTArray<int32_t>& aPositionDescendants) {
  if (mScrollChanged == PAGELOADEDSTART) {
    aPositionDescendants.AppendElement(0);
    aPositions.AppendElement(EmptyCString());
  } else {
    CollectPositions(mDocShell->GetBrowsingContext(), aPositions,
                     aPositionDescendants);
  }
  mScrollChanged = NO_CHANGE;
}

void CollectInput(Document& aDocument, InputFormData& aInput,
                  nsTArray<CollectedInputDataValue>& aIdVals,
                  nsTArray<CollectedInputDataValue>& aXPathVals) {
  PresShell* presShell = aDocument.GetPresShell();
  if (!presShell) {
    return;
  }

  uint16_t numXPath = 0;
  uint16_t numId = 0;

  // textarea element
  SessionStoreUtils::CollectFromTextAreaElement(aDocument, numXPath, numId,
                                                aXPathVals, aIdVals);
  // input element
  SessionStoreUtils::CollectFromInputElement(aDocument, numXPath, numId,
                                             aXPathVals, aIdVals);
  // select element
  SessionStoreUtils::CollectFromSelectElement(aDocument, numXPath, numId,
                                              aXPathVals, aIdVals);

  Element* bodyElement = aDocument.GetBody();
  if (aDocument.HasFlag(NODE_IS_EDITABLE) && bodyElement) {
    bodyElement->GetInnerHTML(aInput.innerHTML, IgnoreErrors());
  }
  if (aInput.innerHTML.IsEmpty() && numXPath == 0 && numId == 0) {
    return;
  }

  // Store the frame's current URL with its form data so that we can compare
  // it when restoring data to not inject form data into the wrong document.
  nsIURI* uri = aDocument.GetDocumentURI();
  if (uri) {
    uri->GetSpecIgnoringRef(aInput.url);
  }
  aInput.numId = numId;
  aInput.numXPath = numXPath;
}

int CollectInputs(BrowsingContext* aBrowsingContext,
                  nsTArray<InputFormData>& aInputs,
                  nsTArray<CollectedInputDataValue>& aIdVals,
                  nsTArray<CollectedInputDataValue>& aXPathVals) {
  nsPIDOMWindowOuter* window = aBrowsingContext->GetDOMWindow();
  if (!window) {
    return 0;
  }

  nsIDocShell* docShell = window->GetDocShell();
  if (!docShell || docShell->GetCreatedDynamically()) {
    return 0;
  }

  Document* document = window->GetDoc();
  if (!document) {
    return 0;
  }

  /* Collect data from current frame */
  InputFormData input;
  input.descendants = 0;
  input.numId = 0;
  input.numXPath = 0;
  CollectInput(*document, input, aIdVals, aXPathVals);
  aInputs.AppendElement(input);
  unsigned long currentIdx = aInputs.Length() - 1;

  /* Collect data from all child frame */
  // This is not going to work for fission. Bug 1572084 for tracking it.
  for (auto& child : aBrowsingContext->Children()) {
    aInputs[currentIdx].descendants +=
        CollectInputs(child, aInputs, aIdVals, aXPathVals);
  }

  return aInputs[currentIdx].descendants + 1;
}

nsTArray<InputFormData> ContentSessionStore::GetInputs(
    nsTArray<CollectedInputDataValue>& aIdVals,
    nsTArray<CollectedInputDataValue>& aXPathVals) {
  nsTArray<InputFormData> inputs;
  if (mFormDataChanged == PAGELOADEDSTART) {
    mFormDataChanged = NO_CHANGE;
    InputFormData input;
    input.descendants = 0;
    input.innerHTML = EmptyString();
    input.url = EmptyCString();
    input.numId = 0;
    input.numXPath = 0;
    inputs.AppendElement(input);
  } else {
    mFormDataChanged = NO_CHANGE;
    CollectInputs(nsDocShell::Cast(mDocShell)->GetBrowsingContext(), inputs,
                  aIdVals, aXPathVals);
  }
  return inputs;
}

bool ContentSessionStore::AppendSessionStorageChange(StorageEvent* aEvent) {
  // We will collect the full SessionStore if mStorageStatus is FULLSTORAGE.
  // These partial changes can be skipped in this case.
  if (mStorageStatus == FULLSTORAGE) {
    return false;
  }

  RefPtr<Storage> changingStorage = aEvent->GetStorageArea();
  if (!changingStorage) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> storagePrincipal = changingStorage->StoragePrincipal();
  if (!storagePrincipal) {
    return false;
  }

  nsAutoCString origin;
  nsresult rv = storagePrincipal->GetOrigin(origin);
  if (NS_FAILED(rv)) {
    return false;
  }

  mOrigins.AppendElement(origin);
  aEvent->GetKey(*mKeys.AppendElement());
  aEvent->GetNewValue(*mValues.AppendElement());
  mStorageStatus = STORAGECHANGE;
  return true;
}

bool ContentSessionStore::GetAndClearStorageChanges(
    nsTArray<nsCString>& aOrigins, nsTArray<nsString>& aKeys,
    nsTArray<nsString>& aValues) {
  MOZ_ASSERT(IsStorageUpdated());
  bool isFullStorage = false;

  if (mStorageStatus == RESET) {
    isFullStorage = true;
  } else if (mStorageStatus == FULLSTORAGE) {
    MOZ_ASSERT(mDocShell);
    SessionStoreUtils::CollectedSessionStorage(
        nsDocShell::Cast(mDocShell)->GetBrowsingContext(), aOrigins, aKeys,
        aValues);
    isFullStorage = true;
  } else if (mStorageStatus == STORAGECHANGE) {
    aOrigins.SwapElements(mOrigins);
    aKeys.SwapElements(mKeys);
    aValues.SwapElements(mValues);
  }

  ResetStorageChanges();
  mStorageStatus = NO_STORAGE;
  return isFullStorage;
}

bool TabListener::ForceFlushFromParent(uint32_t aFlushId, bool aIsFinal) {
  if (!XRE_IsParentProcess()) {
    return false;
  }
  if (!mSessionStore) {
    return false;
  }
  return UpdateSessionStore(aFlushId, aIsFinal);
}

void TabListener::UpdateSHistoryChanges(bool aImmediately) {
  mSessionStore->SetSHistoryFromParentChanged();
  if (aImmediately) {
    UpdateSessionStore();
  } else {
    AddTimerForUpdate();
  }
}

bool TabListener::UpdateSessionStore(uint32_t aFlushId, bool aIsFinal) {
  if (!aFlushId) {
    if (!mSessionStore || !mSessionStore->UpdateNeeded()) {
      return false;
    }
  }

  if (!XRE_IsParentProcess()) {
    BrowserChild* browserChild = BrowserChild::GetFrom(mDocShell);
    if (browserChild) {
      StopTimerForUpdate();
      return browserChild->UpdateSessionStore(aFlushId);
    }
    return false;
  }

  if (!mOwnerContent) {
    return false;
  }

  uint32_t chromeFlags = 0;
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner) {
    return false;
  }
  nsCOMPtr<nsIAppWindow> window(do_GetInterface(treeOwner));
  if (!window) {
    return false;
  }
  if (window && NS_FAILED(window->GetChromeFlags(&chromeFlags))) {
    return false;
  }

  UpdateSessionStoreData data;
  if (mSessionStore->IsDocCapChanged()) {
    data.mDocShellCaps.Construct() = mSessionStore->GetDocShellCaps();
  }
  if (mSessionStore->IsPrivateChanged()) {
    data.mIsPrivate.Construct() = mSessionStore->GetPrivateModeEnabled();
  }
  if (mSessionStore->IsScrollPositionChanged()) {
    nsTArray<nsCString> positions;
    nsTArray<int> descendants;
    mSessionStore->GetScrollPositions(positions, descendants);
    data.mPositions.Construct(std::move(positions));
    data.mPositionDescendants.Construct(std::move(descendants));
  }
  if (mSessionStore->IsFormDataChanged()) {
    nsTArray<CollectedInputDataValue> dataWithId, dataWithXpath;
    nsTArray<InputFormData> inputs =
        mSessionStore->GetInputs(dataWithId, dataWithXpath);
    nsTArray<int> descendants, numId, numXPath;
    nsTArray<nsString> innerHTML;
    nsTArray<nsCString> url;

    if (dataWithId.Length() != 0) {
      SessionStoreUtils::ComposeInputData(dataWithId, data.mId.Construct());
    }
    if (dataWithXpath.Length() != 0) {
      SessionStoreUtils::ComposeInputData(dataWithXpath,
                                          data.mXpath.Construct());
    }

    for (const InputFormData& input : inputs) {
      descendants.AppendElement(input.descendants);
      numId.AppendElement(input.numId);
      numXPath.AppendElement(input.numXPath);
      innerHTML.AppendElement(input.innerHTML);
      url.AppendElement(input.url);
    }
    if (descendants.Length() != 0) {
      data.mInputDescendants.Construct(std::move(descendants));
      data.mNumId.Construct(std::move(numId));
      data.mNumXPath.Construct(std::move(numXPath));
      data.mInnerHTML.Construct(std::move(innerHTML));
      data.mUrl.Construct(std::move(url));
    }
  }
  if (mSessionStore->IsStorageUpdated()) {
    nsTArray<nsCString> origins;
    nsTArray<nsString> keys, values;
    data.mIsFullStorage.Construct() =
        mSessionStore->GetAndClearStorageChanges(origins, keys, values);
    data.mStorageOrigins.Construct(std::move(origins));
    data.mStorageKeys.Construct(std::move(keys));
    data.mStorageValues.Construct(std::move(values));
  }

  nsCOMPtr<nsISessionStoreFunctions> funcs =
      do_ImportModule("resource://gre/modules/SessionStoreFunctions.jsm");
  NS_ENSURE_TRUE(funcs, false);
  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(funcs);
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(wrapped->GetJSObjectGlobal()));
  JS::Rooted<JS::Value> dataVal(jsapi.cx());
  bool ok = ToJSValue(jsapi.cx(), data, &dataVal);
  NS_ENSURE_TRUE(ok, false);

  nsresult rv = funcs->UpdateSessionStore(
      mOwnerContent, aFlushId, aIsFinal, mEpoch, dataVal,
      mSessionStore->GetAndClearSHistoryChanged());
  NS_ENSURE_SUCCESS(rv, false);
  StopTimerForUpdate();
  return true;
}

void TabListener::ResetStorageChangeListener() {
  if (mStorageChangeListenerRegistered) {
    return;
  }

  nsCOMPtr<EventTarget> eventTarget = GetEventTarget();
  if (!eventTarget) {
    return;
  }
  eventTarget->AddSystemEventListener(u"MozSessionStorageChanged"_ns, this,
                                      false);
  mStorageChangeListenerRegistered = true;
}

void TabListener::RemoveStorageChangeListener() {
  if (!mStorageChangeListenerRegistered) {
    return;
  }

  nsCOMPtr<EventTarget> eventTarget = GetEventTarget();
  if (eventTarget) {
    eventTarget->RemoveSystemEventListener(u"MozSessionStorageChanged"_ns, this,
                                           false);
    mStorageChangeListenerRegistered = false;
  }
}

void TabListener::RemoveListeners() {
  if (mProgressListenerRegistered) {
    nsCOMPtr<nsIWebProgress> webProgress = do_QueryInterface(mDocShell);
    if (webProgress) {
      webProgress->RemoveProgressListener(this);
      mProgressListenerRegistered = false;
    }
  }

  if (mEventListenerRegistered || mStorageChangeListenerRegistered) {
    nsCOMPtr<EventTarget> eventTarget = GetEventTarget();
    if (eventTarget) {
      if (mEventListenerRegistered) {
        eventTarget->RemoveSystemEventListener(u"mozvisualscroll"_ns, this,
                                               false);
        eventTarget->RemoveSystemEventListener(u"input"_ns, this, false);
        if (mSHistoryInParent) {
          eventTarget->RemoveSystemEventListener(u"DOMTitleChanged"_ns, this,
                                                 false);
        }
        mEventListenerRegistered = false;
      }
      if (mStorageChangeListenerRegistered) {
        eventTarget->RemoveSystemEventListener(u"MozSessionStorageChanged"_ns,
                                               this, false);
        mStorageChangeListenerRegistered = false;
      }
    }
  }

  if (mPrefObserverRegistered || mStorageObserverRegistered) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (!obs) {
      return;
    }
    if (mPrefObserverRegistered) {
      obs->RemoveObserver(this, kTimeOutDisable);
      obs->RemoveObserver(this, kPrefInterval);
      mPrefObserverRegistered = false;
    }
    if (mStorageObserverRegistered) {
      obs->RemoveObserver(this, "browser:purge-sessionStorage");
      mStorageObserverRegistered = false;
    }
  }
}

TabListener::~TabListener() { RemoveListeners(); }
