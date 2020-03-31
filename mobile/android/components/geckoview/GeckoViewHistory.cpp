/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewHistory.h"

#include "JavaBuiltins.h"
#include "jsapi.h"
#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject
#include "nsIURI.h"
#include "nsXULAppAPI.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/StaticPrefs_layout.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/BrowserChild.h"

#include "mozilla/ipc/URIUtils.h"

#include "mozilla/widget/EventDispatcher.h"
#include "mozilla/widget/nsWindow.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::widget;

static const char16_t kOnVisitedMessage[] = u"GeckoView:OnVisited";
static const char16_t kGetVisitedMessage[] = u"GeckoView:GetVisited";

// Keep in sync with `GeckoSession.HistoryDelegate.VisitFlags`.
enum class GeckoViewVisitFlags : int32_t {
  VISIT_TOP_LEVEL = 1 << 0,
  VISIT_REDIRECT_TEMPORARY = 1 << 1,
  VISIT_REDIRECT_PERMANENT = 1 << 2,
  VISIT_REDIRECT_SOURCE = 1 << 3,
  VISIT_REDIRECT_SOURCE_PERMANENT = 1 << 4,
  VISIT_UNRECOVERABLE_ERROR = 1 << 5,
};

GeckoViewHistory::GeckoViewHistory() {}

GeckoViewHistory::~GeckoViewHistory() {}

NS_IMPL_ISUPPORTS(GeckoViewHistory, IHistory)

StaticRefPtr<GeckoViewHistory> GeckoViewHistory::sHistory;

/* static */
already_AddRefed<GeckoViewHistory> GeckoViewHistory::GetSingleton() {
  if (!sHistory) {
    sHistory = new GeckoViewHistory();
    ClearOnShutdown(&sHistory);
  }
  RefPtr<GeckoViewHistory> history = sHistory;
  return history.forget();
}

// Handles a request to fetch visited statuses for new tracked URIs in the
// content process (e10s).
void GeckoViewHistory::QueryVisitedStateInContentProcess(
    const PendingVisitedQueries& aQueries) {
  // Holds an array of new tracked URIs for a tab in the content process.
  struct NewURIEntry {
    explicit NewURIEntry(BrowserChild* aBrowserChild, nsIURI* aURI)
        : mBrowserChild(aBrowserChild) {
      AddURI(aURI);
    }

    void AddURI(nsIURI* aURI) { mURIs.AppendElement(aURI); }

    BrowserChild* mBrowserChild;
    nsTArray<RefPtr<nsIURI>> mURIs;
  };

  MOZ_ASSERT(XRE_IsContentProcess());

  // First, serialize all the new URIs that we need to look up. Note that this
  // could be written as `nsDataHashtable<nsUint64HashKey, nsTArray<URIParams>`
  // instead, but, since we don't expect to have many tab children, we can avoid
  // the cost of hashing.
  AutoTArray<NewURIEntry, 8> newEntries;
  for (auto query = aQueries.ConstIter(); !query.Done(); query.Next()) {
    nsIURI* uri = query.Get()->GetKey();
    auto entry = mTrackedURIs.Lookup(uri);
    if (!entry) {
      continue;
    }
    ObservingLinks& links = entry.Data();
    nsTObserverArray<Link*>::BackwardIterator linksIter(links.mLinks);
    while (linksIter.HasMore()) {
      Link* link = linksIter.GetNext();

      nsIWidget* widget = nsContentUtils::WidgetForContent(link->GetElement());
      if (!widget) {
        continue;
      }
      BrowserChild* browserChild = widget->GetOwningBrowserChild();
      if (!browserChild) {
        continue;
      }
      // Add to the list of new URIs for this document, or make a new entry.
      bool hasEntry = false;
      for (NewURIEntry& entry : newEntries) {
        if (entry.mBrowserChild == browserChild) {
          entry.AddURI(uri);
          hasEntry = true;
          break;
        }
      }
      if (!hasEntry) {
        newEntries.AppendElement(NewURIEntry(browserChild, uri));
      }
    }
  }

  // Send the request to the parent process, one message per tab child.
  for (const NewURIEntry& entry : newEntries) {
    Unused << NS_WARN_IF(
        !entry.mBrowserChild->SendQueryVisitedState(entry.mURIs));
  }
}

// Handles a request to fetch visited statuses for new tracked URIs in the
// parent process (non-e10s).
void GeckoViewHistory::QueryVisitedStateInParentProcess(
    const PendingVisitedQueries& aQueries) {
  // Holds an array of new URIs for a window in the parent process. Unlike
  // the content process case, we don't need to track tab children, since we
  // have the outer window and can send the request directly to Java.
  struct NewURIEntry {
    explicit NewURIEntry(nsIWidget* aWidget, nsIURI* aURI) : mWidget(aWidget) {
      AddURI(aURI);
    }

    void AddURI(nsIURI* aURI) { mURIs.AppendElement(aURI); }

    nsCOMPtr<nsIWidget> mWidget;
    nsTArray<RefPtr<nsIURI>> mURIs;
  };

  MOZ_ASSERT(XRE_IsParentProcess());

  nsTArray<NewURIEntry> newEntries;
  for (auto query = aQueries.ConstIter(); !query.Done(); query.Next()) {
    nsIURI* uri = query.Get()->GetKey();
    auto entry = mTrackedURIs.Lookup(uri);
    if (!entry) {
      continue;  // Nobody cares about this uri anymore.
    }

    ObservingLinks& links = entry.Data();
    nsTObserverArray<Link*>::BackwardIterator linksIter(links.mLinks);
    while (linksIter.HasMore()) {
      Link* link = linksIter.GetNext();

      nsIWidget* widget = nsContentUtils::WidgetForContent(link->GetElement());
      if (!widget) {
        continue;
      }

      bool hasEntry = false;
      for (NewURIEntry& entry : newEntries) {
        if (entry.mWidget != widget) {
          continue;
        }
        entry.AddURI(uri);
        hasEntry = true;
      }
      if (!hasEntry) {
        newEntries.AppendElement(NewURIEntry(widget, uri));
      }
    }
  }

  for (const NewURIEntry& entry : newEntries) {
    QueryVisitedState(entry.mWidget, std::move(entry.mURIs));
  }
}

void GeckoViewHistory::StartPendingVisitedQueries(
    const PendingVisitedQueries& aQueries) {
  if (XRE_IsContentProcess()) {
    QueryVisitedStateInContentProcess(aQueries);
  } else {
    QueryVisitedStateInParentProcess(aQueries);
  }
}

/**
 * Called from the session handler for the history delegate, after the new
 * visit is recorded.
 */
class OnVisitedCallback final : public nsIAndroidEventCallback {
 public:
  explicit OnVisitedCallback(GeckoViewHistory* aHistory,
                             nsIGlobalObject* aGlobalObject, nsIURI* aURI)
      : mHistory(aHistory), mGlobalObject(aGlobalObject), mURI(aURI) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnSuccess(JS::HandleValue aData, JSContext* aCx) override {
    Maybe<bool> visitedState = GetVisitedValue(aCx, aData);
    JS_ClearPendingException(aCx);
    if (visitedState) {
      AutoTArray<VisitedURI, 1> visitedURIs;
      visitedURIs.AppendElement(VisitedURI{mURI.get(), *visitedState});
      mHistory->HandleVisitedState(visitedURIs);
    }
    return NS_OK;
  }

  NS_IMETHOD
  OnError(JS::HandleValue aData, JSContext* aCx) override { return NS_OK; }

 private:
  virtual ~OnVisitedCallback() {}

  Maybe<bool> GetVisitedValue(JSContext* aCx, JS::HandleValue aData) {
    if (NS_WARN_IF(!aData.isBoolean())) {
      return Nothing();
    }
    return Some(aData.toBoolean());
  }

  RefPtr<GeckoViewHistory> mHistory;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  nsCOMPtr<nsIURI> mURI;
};

NS_IMPL_ISUPPORTS(OnVisitedCallback, nsIAndroidEventCallback)

NS_IMETHODIMP
GeckoViewHistory::VisitURI(nsIWidget* aWidget, nsIURI* aURI,
                           nsIURI* aLastVisitedURI, uint32_t aFlags) {
  if (!aURI) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    // If we're in the content process, send the visit to the parent. The parent
    // will find the matching chrome window for the content process and tab,
    // then forward the visit to Java.
    if (NS_WARN_IF(!aWidget)) {
      return NS_OK;
    }
    BrowserChild* browserChild = aWidget->GetOwningBrowserChild();
    if (NS_WARN_IF(!browserChild)) {
      return NS_OK;
    }
    Unused << NS_WARN_IF(
        !browserChild->SendVisitURI(aURI, aLastVisitedURI, aFlags));
    return NS_OK;
  }

  // Otherwise, we're in the parent process. Wrap the URIs up in a bundle, and
  // send them to Java.
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<nsWindow> window = nsWindow::From(aWidget);
  if (NS_WARN_IF(!window)) {
    return NS_OK;
  }
  widget::EventDispatcher* dispatcher = window->GetEventDispatcher();
  if (NS_WARN_IF(!dispatcher)) {
    return NS_OK;
  }

  // If nobody is listening for this, we can stop now.
  if (!dispatcher->HasListener(kOnVisitedMessage)) {
    return NS_OK;
  }

  AutoTArray<jni::String::LocalRef, 3> keys;
  AutoTArray<jni::Object::LocalRef, 3> values;

  nsAutoCString uriSpec;
  if (NS_WARN_IF(NS_FAILED(aURI->GetSpec(uriSpec)))) {
    return NS_OK;
  }
  keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("url")));
  values.AppendElement(jni::StringParam(uriSpec));

  if (aLastVisitedURI) {
    nsAutoCString lastVisitedURISpec;
    if (NS_WARN_IF(NS_FAILED(aLastVisitedURI->GetSpec(lastVisitedURISpec)))) {
      return NS_OK;
    }
    keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("lastVisitedURL")));
    values.AppendElement(jni::StringParam(lastVisitedURISpec));
  }

  int32_t flags = 0;
  if (aFlags & TOP_LEVEL) {
    flags |= static_cast<int32_t>(GeckoViewVisitFlags::VISIT_TOP_LEVEL);
  }
  if (aFlags & REDIRECT_TEMPORARY) {
    flags |=
        static_cast<int32_t>(GeckoViewVisitFlags::VISIT_REDIRECT_TEMPORARY);
  }
  if (aFlags & REDIRECT_PERMANENT) {
    flags |=
        static_cast<int32_t>(GeckoViewVisitFlags::VISIT_REDIRECT_PERMANENT);
  }
  if (aFlags & REDIRECT_SOURCE) {
    flags |= static_cast<int32_t>(GeckoViewVisitFlags::VISIT_REDIRECT_SOURCE);
  }
  if (aFlags & REDIRECT_SOURCE_PERMANENT) {
    flags |= static_cast<int32_t>(
        GeckoViewVisitFlags::VISIT_REDIRECT_SOURCE_PERMANENT);
  }
  if (aFlags & UNRECOVERABLE_ERROR) {
    flags |=
        static_cast<int32_t>(GeckoViewVisitFlags::VISIT_UNRECOVERABLE_ERROR);
  }
  keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("flags")));
  values.AppendElement(java::sdk::Integer::ValueOf(flags));

  MOZ_ASSERT(keys.Length() == values.Length());

  auto bundleKeys = jni::ObjectArray::New<jni::String>(keys.Length());
  auto bundleValues = jni::ObjectArray::New<jni::Object>(values.Length());
  for (size_t i = 0; i < keys.Length(); ++i) {
    bundleKeys->SetElement(i, keys[i]);
    bundleValues->SetElement(i, values[i]);
  }
  auto bundle = java::GeckoBundle::New(bundleKeys, bundleValues);

  nsCOMPtr<nsIAndroidEventCallback> callback =
      new OnVisitedCallback(this, dispatcher->GetGlobalObject(), aURI);

  Unused << NS_WARN_IF(
      NS_FAILED(dispatcher->Dispatch(kOnVisitedMessage, bundle, callback)));

  return NS_OK;
}

NS_IMETHODIMP
GeckoViewHistory::SetURITitle(nsIURI* aURI, const nsAString& aTitle) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 * Called from the session handler for the history delegate, with visited
 * statuses for all requested URIs.
 */
class GetVisitedCallback final : public nsIAndroidEventCallback {
 public:
  explicit GetVisitedCallback(GeckoViewHistory* aHistory,
                              nsIGlobalObject* aGlobalObject,
                              const nsTArray<RefPtr<nsIURI>>& aURIs)
      : mHistory(aHistory), mGlobalObject(aGlobalObject), mURIs(aURIs) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnSuccess(JS::HandleValue aData, JSContext* aCx) override {
    nsTArray<VisitedURI> visitedURIs;
    if (!ExtractVisitedURIs(aCx, aData, visitedURIs)) {
      JS_ClearPendingException(aCx);
      return NS_ERROR_FAILURE;
    }
    mHistory->HandleVisitedState(visitedURIs);
    return NS_OK;
  }

  NS_IMETHOD
  OnError(JS::HandleValue aData, JSContext* aCx) override { return NS_OK; }

 private:
  virtual ~GetVisitedCallback() {}

  /**
   * Unpacks an array of Boolean visited statuses from the session handler into
   * an array of `VisitedURI` structs. Each element in the array corresponds to
   * a URI in `mURIs`.
   *
   * Returns `false` on error, `true` if the array is `null` or was successfully
   * unpacked.
   *
   * TODO (bug 1503482): Remove this unboxing.
   */
  bool ExtractVisitedURIs(JSContext* aCx, JS::HandleValue aData,
                          nsTArray<VisitedURI>& aVisitedURIs) {
    if (aData.isNull()) {
      return true;
    }
    bool isArray = false;
    if (NS_WARN_IF(!JS::IsArrayObject(aCx, aData, &isArray))) {
      return false;
    }
    if (NS_WARN_IF(!isArray)) {
      return false;
    }
    JS::Rooted<JSObject*> visited(aCx, &aData.toObject());
    uint32_t length = 0;
    if (NS_WARN_IF(!JS::GetArrayLength(aCx, visited, &length))) {
      return false;
    }
    if (NS_WARN_IF(length != mURIs.Length())) {
      return false;
    }
    if (!aVisitedURIs.SetCapacity(length, mozilla::fallible)) {
      return false;
    }
    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(aCx);
      if (NS_WARN_IF(!JS_GetElement(aCx, visited, i, &value))) {
        JS_ClearPendingException(aCx);
        aVisitedURIs.AppendElement(VisitedURI{mURIs[i].get(), false});
        continue;
      }
      if (NS_WARN_IF(!value.isBoolean())) {
        aVisitedURIs.AppendElement(VisitedURI{mURIs[i].get(), false});
        continue;
      }
      aVisitedURIs.AppendElement(VisitedURI{mURIs[i].get(), value.toBoolean()});
    }
    return true;
  }

  RefPtr<GeckoViewHistory> mHistory;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  nsTArray<RefPtr<nsIURI>> mURIs;
};

NS_IMPL_ISUPPORTS(GetVisitedCallback, nsIAndroidEventCallback)

/**
 * Queries the history delegate to find which URIs have been visited. This
 * is always called in the parent process: from `GetVisited` in non-e10s, and
 * from `ContentParent::RecvGetVisited` in e10s.
 */
void GeckoViewHistory::QueryVisitedState(
    nsIWidget* aWidget, const nsTArray<RefPtr<nsIURI>>&& aURIs) {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<nsWindow> window = nsWindow::From(aWidget);
  if (NS_WARN_IF(!window)) {
    return;
  }
  widget::EventDispatcher* dispatcher = window->GetEventDispatcher();
  if (NS_WARN_IF(!dispatcher)) {
    return;
  }

  // If nobody is listening for this we can stop now
  if (!dispatcher->HasListener(kGetVisitedMessage)) {
    return;
  }

  // Assemble a bundle like `{ urls: ["http://example.com/1", ...] }`.
  auto uris = jni::ObjectArray::New<jni::String>(aURIs.Length());
  for (size_t i = 0; i < aURIs.Length(); ++i) {
    nsAutoCString uriSpec;
    if (NS_WARN_IF(NS_FAILED(aURIs[i]->GetSpec(uriSpec)))) {
      continue;
    }
    jni::String::LocalRef value{jni::StringParam(uriSpec)};
    uris->SetElement(i, value);
  }

  auto bundleKeys = jni::ObjectArray::New<jni::String>(1);
  jni::String::LocalRef key(jni::StringParam(NS_LITERAL_STRING("urls")));
  bundleKeys->SetElement(0, key);

  auto bundleValues = jni::ObjectArray::New<jni::Object>(1);
  jni::Object::LocalRef value(uris);
  bundleValues->SetElement(0, value);

  auto bundle = java::GeckoBundle::New(bundleKeys, bundleValues);

  nsCOMPtr<nsIAndroidEventCallback> callback =
      new GetVisitedCallback(this, dispatcher->GetGlobalObject(), aURIs);

  Unused << NS_WARN_IF(
      NS_FAILED(dispatcher->Dispatch(kGetVisitedMessage, bundle, callback)));
}

/**
 * Updates link states for all tracked links, forwarding the visited statuses to
 * the content process in e10s. This is always called in the parent process,
 * from `VisitedCallback::OnSuccess` and `GetVisitedCallback::OnSuccess`.
 */
void GeckoViewHistory::HandleVisitedState(
    const nsTArray<VisitedURI>& aVisitedURIs) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (aVisitedURIs.IsEmpty()) {
    return;
  }

  nsTArray<ContentParent*> cplist;
  ContentParent::GetAll(cplist);
  if (!cplist.IsEmpty()) {
    nsTArray<VisitedQueryResult> visitedURIs(aVisitedURIs.Length());
    for (const VisitedURI& visitedURI : aVisitedURIs) {
      if (!visitedURI.mVisited &&
          !StaticPrefs::layout_css_notify_of_unvisited()) {
        continue;
      }

      VisitedQueryResult& result = *visitedURIs.AppendElement();
      result.uri() = visitedURI.mURI;
      result.visited() = visitedURI.mVisited;
    }
    if (visitedURIs.IsEmpty()) {
      return;
    }
    for (ContentParent* cp : cplist) {
      Unused << NS_WARN_IF(!cp->SendNotifyVisited(visitedURIs));
    }
  }

  // We might still have child processes even if e10s is disabled, so always
  // check if we're tracking any links in the parent, and notify them if so.
  if (!mTrackedURIs.IsEmpty()) {
    for (const VisitedURI& visitedURI : aVisitedURIs) {
      if (!visitedURI.mVisited &&
          !StaticPrefs::layout_css_notify_of_unvisited()) {
        continue;
      }
      auto status = visitedURI.mVisited ? VisitedStatus::Visited
                                        : VisitedStatus::Unvisited;
      NotifyVisited(visitedURI.mURI, status);
    }
  }
}
