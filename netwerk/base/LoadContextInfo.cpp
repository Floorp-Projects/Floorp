/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadContextInfo.h"

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsDocShell.h"
#include "nsIChannel.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"

using namespace mozilla::dom;
namespace mozilla {
namespace net {

// LoadContextInfo

NS_IMPL_ISUPPORTS(LoadContextInfo, nsILoadContextInfo)

LoadContextInfo::LoadContextInfo(bool aIsAnonymous,
                                 OriginAttributes aOriginAttributes)
    : mIsAnonymous(aIsAnonymous),
      mOriginAttributes(std::move(aOriginAttributes)) {}

NS_IMETHODIMP LoadContextInfo::GetIsPrivate(bool* aIsPrivate) {
  *aIsPrivate = mOriginAttributes.mPrivateBrowsingId > 0;
  return NS_OK;
}

NS_IMETHODIMP LoadContextInfo::GetIsAnonymous(bool* aIsAnonymous) {
  *aIsAnonymous = mIsAnonymous;
  return NS_OK;
}

OriginAttributes const* LoadContextInfo::OriginAttributesPtr() {
  return &mOriginAttributes;
}

NS_IMETHODIMP LoadContextInfo::GetOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aVal) {
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aVal))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// LoadContextInfoFactory

NS_IMPL_ISUPPORTS(LoadContextInfoFactory, nsILoadContextInfoFactory)

NS_IMETHODIMP LoadContextInfoFactory::GetDefault(
    nsILoadContextInfo** aDefault) {
  nsCOMPtr<nsILoadContextInfo> info =
      GetLoadContextInfo(false, OriginAttributes());
  info.forget(aDefault);
  return NS_OK;
}

NS_IMETHODIMP LoadContextInfoFactory::GetPrivate(
    nsILoadContextInfo** aPrivate) {
  OriginAttributes attrs;
  attrs.SyncAttributesWithPrivateBrowsing(true);
  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(false, attrs);
  info.forget(aPrivate);
  return NS_OK;
}

NS_IMETHODIMP LoadContextInfoFactory::GetAnonymous(
    nsILoadContextInfo** aAnonymous) {
  nsCOMPtr<nsILoadContextInfo> info =
      GetLoadContextInfo(true, OriginAttributes());
  info.forget(aAnonymous);
  return NS_OK;
}

NS_IMETHODIMP LoadContextInfoFactory::Custom(bool aAnonymous,
                                             JS::HandleValue aOriginAttributes,
                                             JSContext* cx,
                                             nsILoadContextInfo** _retval) {
  OriginAttributes attrs;
  bool status = attrs.Init(cx, aOriginAttributes);
  NS_ENSURE_TRUE(status, NS_ERROR_FAILURE);

  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(aAnonymous, attrs);
  info.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP LoadContextInfoFactory::FromLoadContext(
    nsILoadContext* aLoadContext, bool aAnonymous,
    nsILoadContextInfo** _retval) {
  nsCOMPtr<nsILoadContextInfo> info =
      GetLoadContextInfo(aLoadContext, aAnonymous);
  info.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP LoadContextInfoFactory::FromWindow(nsIDOMWindow* aWindow,
                                                 bool aAnonymous,
                                                 nsILoadContextInfo** _retval) {
  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(aWindow, aAnonymous);
  info.forget(_retval);
  return NS_OK;
}

// Helper functions

LoadContextInfo* GetLoadContextInfo(nsIChannel* aChannel) {
  nsresult rv;

  DebugOnly<bool> pb = NS_UsePrivateBrowsing(aChannel);

  bool anon = false;
  nsLoadFlags loadFlags;
  rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_SUCCEEDED(rv)) {
    anon = !!(loadFlags & nsIChannel::LOAD_ANONYMOUS);
  }

  OriginAttributes oa;
  StoragePrincipalHelper::GetOriginAttributesForNetworkState(aChannel, oa);
  MOZ_ASSERT(pb == (oa.mPrivateBrowsingId > 0));

  return new LoadContextInfo(anon, oa);
}

LoadContextInfo* GetLoadContextInfo(nsILoadContext* aLoadContext,
                                    bool aIsAnonymous) {
  if (!aLoadContext) {
    return new LoadContextInfo(aIsAnonymous, OriginAttributes());
  }

  OriginAttributes oa;
  aLoadContext->GetOriginAttributes(oa);

#ifdef DEBUG
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aLoadContext);
  if (!docShell ||
      nsDocShell::Cast(docShell)->GetBrowsingContext()->IsContent()) {
    MOZ_ASSERT(aLoadContext->UsePrivateBrowsing() ==
               (oa.mPrivateBrowsingId > 0));
  }
#endif

  return new LoadContextInfo(aIsAnonymous, oa);
}

LoadContextInfo* GetLoadContextInfo(nsIDOMWindow* aWindow, bool aIsAnonymous) {
  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);

  return GetLoadContextInfo(loadContext, aIsAnonymous);
}

LoadContextInfo* GetLoadContextInfo(nsILoadContextInfo* aInfo) {
  return new LoadContextInfo(aInfo->IsAnonymous(),
                             *aInfo->OriginAttributesPtr());
}

LoadContextInfo* GetLoadContextInfo(bool const aIsAnonymous,
                                    OriginAttributes const& aOriginAttributes) {
  return new LoadContextInfo(aIsAnonymous, aOriginAttributes);
}

}  // namespace net
}  // namespace mozilla
