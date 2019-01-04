/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDocShell.h"
#include "nsGlobalWindowOuter.h"
#include "nsIScrollableFrame.h"
#include "nsPresContext.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsPrintfCString.h"
#include "mozilla/dom/SessionStoreUtils.h"

using namespace mozilla::dom;

namespace {

class DynamicFrameEventFilter final : public nsIDOMEventListener {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DynamicFrameEventFilter)

  explicit DynamicFrameEventFilter(EventListener* aListener)
      : mListener(aListener) {}

  NS_IMETHODIMP HandleEvent(Event* aEvent) override {
    if (mListener && TargetInNonDynamicDocShell(aEvent)) {
      mListener->HandleEvent(*aEvent);
    }

    return NS_OK;
  }

 private:
  ~DynamicFrameEventFilter() = default;

  bool TargetInNonDynamicDocShell(Event* aEvent) {
    EventTarget* target = aEvent->GetTarget();
    if (!target) {
      return false;
    }

    nsPIDOMWindowOuter* outer = target->GetOwnerGlobalForBindingsInternal();
    if (!outer) {
      return false;
    }

    nsIDocShell* docShell = outer->GetDocShell();
    if (!docShell) {
      return false;
    }

    bool isDynamic = false;
    nsresult rv = docShell->GetCreatedDynamically(&isDynamic);
    return NS_SUCCEEDED(rv) && !isDynamic;
  }

  RefPtr<EventListener> mListener;
};

NS_IMPL_CYCLE_COLLECTION(DynamicFrameEventFilter, mListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DynamicFrameEventFilter)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DynamicFrameEventFilter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DynamicFrameEventFilter)

}  // anonymous namespace

/* static */ void SessionStoreUtils::ForEachNonDynamicChildFrame(
    const GlobalObject& aGlobal, WindowProxyHolder& aWindow,
    SessionStoreUtilsFrameCallback& aCallback, ErrorResult& aRv) {
  if (!aWindow.get()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow.get()->GetDocShell();
  if (!docShell) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t length;
  aRv = docShell->GetChildCount(&length);
  if (aRv.Failed()) {
    return;
  }

  for (int32_t i = 0; i < length; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    docShell->GetChildAt(i, getter_AddRefs(item));
    if (!item) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsCOMPtr<nsIDocShell> childDocShell(do_QueryInterface(item));
    if (!childDocShell) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    bool isDynamic = false;
    nsresult rv = childDocShell->GetCreatedDynamically(&isDynamic);
    if (NS_SUCCEEDED(rv) && isDynamic) {
      continue;
    }

    int32_t childOffset = childDocShell->GetChildOffset();
    aCallback.Call(WindowProxyHolder(item->GetWindow()->GetBrowsingContext()),
                   childOffset);
  }
}

/* static */ already_AddRefed<nsISupports>
SessionStoreUtils::AddDynamicFrameFilteredListener(
    const GlobalObject& aGlobal, EventTarget& aTarget, const nsAString& aType,
    JS::Handle<JS::Value> aListener, bool aUseCapture, ErrorResult& aRv) {
  if (NS_WARN_IF(!aListener.isObject())) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> obj(cx, &aListener.toObject());
  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  RefPtr<EventListener> listener =
      new EventListener(cx, obj, global, GetIncumbentGlobal());
  nsCOMPtr<nsIDOMEventListener> filter(new DynamicFrameEventFilter(listener));

  aRv = aTarget.AddEventListener(aType, filter, aUseCapture);
  if (aRv.Failed()) {
    return nullptr;
  }

  return filter.forget();
}

/* static */ void SessionStoreUtils::RemoveDynamicFrameFilteredListener(
    const GlobalObject& global, EventTarget& aTarget, const nsAString& aType,
    nsISupports* aListener, bool aUseCapture, ErrorResult& aRv) {
  nsCOMPtr<nsIDOMEventListener> listener = do_QueryInterface(aListener);
  if (!listener) {
    aRv.Throw(NS_ERROR_NO_INTERFACE);
    return;
  }

  aTarget.RemoveEventListener(aType, listener, aUseCapture);
}

/* static */ void SessionStoreUtils::CollectDocShellCapabilities(
    const GlobalObject& aGlobal, nsIDocShell* aDocShell, nsCString& aRetVal) {
  bool allow;

#define TRY_ALLOWPROP(y)          \
  PR_BEGIN_MACRO                  \
  aDocShell->GetAllow##y(&allow); \
  if (!allow) {                   \
    if (!aRetVal.IsEmpty()) {     \
      aRetVal.Append(',');        \
    }                             \
    aRetVal.Append(#y);           \
  }                               \
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
}

/* static */ void SessionStoreUtils::RestoreDocShellCapabilities(
    const GlobalObject& aGlobal, nsIDocShell* aDocShell,
    const nsCString& aDisallowCapabilities) {
  aDocShell->SetAllowPlugins(true);
  aDocShell->SetAllowJavascript(true);
  aDocShell->SetAllowMetaRedirects(true);
  aDocShell->SetAllowSubframes(true);
  aDocShell->SetAllowImages(true);
  aDocShell->SetAllowMedia(true);
  aDocShell->SetAllowDNSPrefetch(true);
  aDocShell->SetAllowWindowControl(true);
  aDocShell->SetAllowContentRetargeting(true);
  aDocShell->SetAllowContentRetargetingOnChildren(true);

  nsCCharSeparatedTokenizer tokenizer(aDisallowCapabilities, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsACString& token = tokenizer.nextToken();
    if (token.EqualsLiteral("Plugins")) {
      aDocShell->SetAllowPlugins(false);
    } else if (token.EqualsLiteral("Javascript")) {
      aDocShell->SetAllowJavascript(false);
    } else if (token.EqualsLiteral("MetaRedirects")) {
      aDocShell->SetAllowMetaRedirects(false);
    } else if (token.EqualsLiteral("Subframes")) {
      aDocShell->SetAllowSubframes(false);
    } else if (token.EqualsLiteral("Images")) {
      aDocShell->SetAllowImages(false);
    } else if (token.EqualsLiteral("Media")) {
      aDocShell->SetAllowMedia(false);
    } else if (token.EqualsLiteral("DNSPrefetch")) {
      aDocShell->SetAllowDNSPrefetch(false);
    } else if (token.EqualsLiteral("WindowControl")) {
      aDocShell->SetAllowWindowControl(false);
    } else if (token.EqualsLiteral("ContentRetargeting")) {
      bool allow;
      aDocShell->GetAllowContentRetargetingOnChildren(&allow);
      aDocShell->SetAllowContentRetargeting(
          false);  // will also set AllowContentRetargetingOnChildren
      aDocShell->SetAllowContentRetargetingOnChildren(
          allow);  // restore the allowProp to original
    } else if (token.EqualsLiteral("ContentRetargetingOnChildren")) {
      aDocShell->SetAllowContentRetargetingOnChildren(false);
    }
  }
}

/* static */ void SessionStoreUtils::CollectScrollPosition(
    const GlobalObject& aGlobal, Document& aDocument,
    SSScrollPositionDict& aRetVal) {
  nsIPresShell* presShell = aDocument.GetShell();
  if (!presShell) {
    return;
  }

  nsIScrollableFrame* frame = presShell->GetRootScrollFrameAsScrollable();
  if (!frame) {
    return;
  }

  nsPoint scrollPos = frame->GetScrollPosition();
  int scrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  int scrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  if ((scrollX != 0) || (scrollY != 0)) {
    aRetVal.mScroll.Construct() = nsPrintfCString("%d,%d", scrollX, scrollY);
  }
}

/* static */ void SessionStoreUtils::RestoreScrollPosition(
    const GlobalObject& aGlobal, nsGlobalWindowInner& aWindow,
    const SSScrollPositionDict& aData) {
  if (!aData.mScroll.WasPassed()) {
    return;
  }

  nsCCharSeparatedTokenizer tokenizer(aData.mScroll.Value(), ',');
  nsAutoCString token(tokenizer.nextToken());
  int pos_X = atoi(token.get());
  token = tokenizer.nextToken();
  int pos_Y = atoi(token.get());
  aWindow.ScrollTo(pos_X, pos_Y);
}
