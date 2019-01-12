/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSessionStoreUtils.h"

#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsGlobalWindowOuter.h"
#include "nsIScrollableFrame.h"
#include "nsPresContext.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsPrintfCString.h"

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
  ~DynamicFrameEventFilter() {}

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

NS_IMPL_ISUPPORTS(nsSessionStoreUtils, nsISessionStoreUtils)

NS_IMETHODIMP
nsSessionStoreUtils::ForEachNonDynamicChildFrame(
    mozIDOMWindowProxy* aWindow, nsISessionStoreUtilsFrameCallback* aCallback) {
  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsPIDOMWindowOuter> outer = nsPIDOMWindowOuter::From(aWindow);
  NS_ENSURE_TRUE(outer, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell = outer->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  int32_t length;
  nsresult rv = docShell->GetChildCount(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int32_t i = 0; i < length; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    docShell->GetChildAt(i, getter_AddRefs(item));
    NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShell> childDocShell(do_QueryInterface(item));
    NS_ENSURE_TRUE(childDocShell, NS_ERROR_FAILURE);

    bool isDynamic = false;
    nsresult rv = childDocShell->GetCreatedDynamically(&isDynamic);
    if (NS_SUCCEEDED(rv) && isDynamic) {
      continue;
    }

    int32_t childOffset = childDocShell->GetChildOffset();
    aCallback->HandleFrame(item->GetWindow(), childOffset);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::AddDynamicFrameFilteredListener(
    EventTarget* aTarget, const nsAString& aType,
    JS::Handle<JS::Value> aListener, bool aUseCapture, bool aMozSystemGroup,
    JSContext* aCx, nsISupports** aResult) {
  if (NS_WARN_IF(!aListener.isObject())) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_TRUE(aTarget, NS_ERROR_NO_INTERFACE);

  JS::Rooted<JSObject*> obj(aCx, &aListener.toObject());
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  RefPtr<EventListener> listener =
      new EventListener(aCx, obj, global, GetIncumbentGlobal());

  nsCOMPtr<nsIDOMEventListener> filter(new DynamicFrameEventFilter(listener));

  nsresult rv;
  if (aMozSystemGroup) {
    rv = aTarget->AddSystemEventListener(aType, filter, aUseCapture);
  } else {
    rv = aTarget->AddEventListener(aType, filter, aUseCapture);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  filter.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::RemoveDynamicFrameFilteredListener(EventTarget* aTarget,
                                                        const nsAString& aType,
                                                        nsISupports* aListener,
                                                        bool aUseCapture,
                                                        bool aMozSystemGroup) {
  NS_ENSURE_TRUE(aTarget, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIDOMEventListener> listener = do_QueryInterface(aListener);
  NS_ENSURE_TRUE(listener, NS_ERROR_NO_INTERFACE);

  if (aMozSystemGroup) {
    aTarget->RemoveSystemEventListener(aType, listener, aUseCapture);
  } else {
    aTarget->RemoveEventListener(aType, listener, aUseCapture);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::CollectDocShellCapabilities(
    nsIDocShell* aDocShell, nsACString& aDisallowCapabilities) {
  bool allow;

#define TRY_ALLOWPROP(y)                    \
  PR_BEGIN_MACRO                            \
  aDocShell->GetAllow##y(&allow);           \
  if (!allow) {                             \
    if (!aDisallowCapabilities.IsEmpty()) { \
      aDisallowCapabilities.Append(',');    \
    }                                       \
    aDisallowCapabilities.Append(#y);       \
  }                                         \
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

  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::RestoreDocShellCapabilities(
    nsIDocShell* aDocShell, const nsACString& aDisallowCapabilities) {
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

  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::CollectScrollPosition(Document* aDocument,
                                           nsACString& aRet) {
  aRet.Truncate();

  nsIPresShell* presShell = aDocument->GetShell();
  if (!presShell) {
    return NS_OK;
  }

  nsPoint scrollPos = presShell->GetVisualViewportOffset();
  int scrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  int scrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  if ((scrollX != 0) || (scrollY != 0)) {
    const nsPrintfCString position("%d,%d", scrollX, scrollY);
    aRet.Assign(position);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::RestoreScrollPosition(mozIDOMWindow* aWindow,
                                           const nsACString& aPos) {
  nsCCharSeparatedTokenizer tokenizer(aPos, ',');
  nsAutoCString token(tokenizer.nextToken());
  int pos_X = atoi(token.get());
  token = tokenizer.nextToken();
  int pos_Y = atoi(token.get());
  nsGlobalWindowInner::Cast(aWindow)->ScrollTo(pos_X, pos_Y);

  return NS_OK;
}
