/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadContextInfo.h"

#include "nsIChannel.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"

namespace mozilla {
namespace net {

// LoadContextInfo

NS_IMPL_ISUPPORTS(LoadContextInfo, nsILoadContextInfo)

LoadContextInfo::LoadContextInfo(bool aIsPrivate, bool aIsAnonymous, OriginAttributes aOriginAttributes)
  : mIsPrivate(aIsPrivate)
  , mIsAnonymous(aIsAnonymous)
  , mOriginAttributes(aOriginAttributes)
{
}

LoadContextInfo::~LoadContextInfo()
{
}

NS_IMETHODIMP LoadContextInfo::GetIsPrivate(bool *aIsPrivate)
{
  *aIsPrivate = mIsPrivate;
  return NS_OK;
}

NS_IMETHODIMP LoadContextInfo::GetIsAnonymous(bool *aIsAnonymous)
{
  *aIsAnonymous = mIsAnonymous;
  return NS_OK;
}

OriginAttributes const* LoadContextInfo::OriginAttributesPtr()
{
  return &mOriginAttributes;
}

NS_IMETHODIMP LoadContextInfo::GetOriginAttributes(JSContext *aCx,
                                                   JS::MutableHandle<JS::Value> aVal)
{
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aVal))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// LoadContextInfoFactory

NS_IMPL_ISUPPORTS(LoadContextInfoFactory, nsILoadContextInfoFactory)

/* readonly attribute nsILoadContextInfo default; */
NS_IMETHODIMP LoadContextInfoFactory::GetDefault(nsILoadContextInfo * *aDefault)
{
  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(false, false, OriginAttributes());
  info.forget(aDefault);
  return NS_OK;
}

/* readonly attribute nsILoadContextInfo private; */
NS_IMETHODIMP LoadContextInfoFactory::GetPrivate(nsILoadContextInfo * *aPrivate)
{
  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(true, false, OriginAttributes());
  info.forget(aPrivate);
  return NS_OK;
}

/* readonly attribute nsILoadContextInfo anonymous; */
NS_IMETHODIMP LoadContextInfoFactory::GetAnonymous(nsILoadContextInfo * *aAnonymous)
{
  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(false, true, OriginAttributes());
  info.forget(aAnonymous);
  return NS_OK;
}

/* nsILoadContextInfo custom (in boolean aPrivate, in boolean aAnonymous, in jsval aOriginAttributes); */
NS_IMETHODIMP LoadContextInfoFactory::Custom(bool aPrivate, bool aAnonymous,
                                             JS::HandleValue aOriginAttributes, JSContext *cx,
                                             nsILoadContextInfo * *_retval)
{
  OriginAttributes attrs;
  bool status = attrs.Init(cx, aOriginAttributes);
  NS_ENSURE_TRUE(status, NS_ERROR_FAILURE);

  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(aPrivate, aAnonymous, attrs);
  info.forget(_retval);
  return NS_OK;
}

/* nsILoadContextInfo fromLoadContext (in nsILoadContext aLoadContext, in boolean aAnonymous); */
NS_IMETHODIMP LoadContextInfoFactory::FromLoadContext(nsILoadContext *aLoadContext, bool aAnonymous,
                                                      nsILoadContextInfo * *_retval)
{
  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(aLoadContext, aAnonymous);
  info.forget(_retval);
  return NS_OK;
}

/* nsILoadContextInfo fromWindow (in nsIDOMWindow aWindow, in boolean aAnonymous); */
NS_IMETHODIMP LoadContextInfoFactory::FromWindow(nsIDOMWindow *aWindow, bool aAnonymous,
                                                 nsILoadContextInfo * *_retval)
{
  nsCOMPtr<nsILoadContextInfo> info = GetLoadContextInfo(aWindow, aAnonymous);
  info.forget(_retval);
  return NS_OK;
}

// Helper functions

LoadContextInfo *
GetLoadContextInfo(nsIChannel * aChannel)
{
  nsresult rv;

  bool pb = NS_UsePrivateBrowsing(aChannel);

  bool anon = false;
  nsLoadFlags loadFlags;
  rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_SUCCEEDED(rv)) {
    anon = !!(loadFlags & nsIChannel::LOAD_ANONYMOUS);
  }

  OriginAttributes oa;
  NS_GetOriginAttributes(aChannel, oa);

  return new LoadContextInfo(pb, anon, oa);
}

LoadContextInfo *
GetLoadContextInfo(nsILoadContext *aLoadContext, bool aIsAnonymous)
{
  if (!aLoadContext) {
    return new LoadContextInfo(false, aIsAnonymous,
                               OriginAttributes(nsILoadContextInfo::NO_APP_ID, false));
  }

  bool pb = aLoadContext->UsePrivateBrowsing();
  OriginAttributes oa;
  aLoadContext->GetOriginAttributes(oa);

  return new LoadContextInfo(pb, aIsAnonymous, oa);
}

LoadContextInfo*
GetLoadContextInfo(nsIDOMWindow *aWindow,
                   bool aIsAnonymous)
{
  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);

  return GetLoadContextInfo(loadContext, aIsAnonymous);
}

LoadContextInfo *
GetLoadContextInfo(nsILoadContextInfo *aInfo)
{
  return new LoadContextInfo(aInfo->IsPrivate(),
                             aInfo->IsAnonymous(),
                             *aInfo->OriginAttributesPtr());
}

LoadContextInfo *
GetLoadContextInfo(bool const aIsPrivate,
                   bool const aIsAnonymous,
                   OriginAttributes const &aOriginAttributes)
{
  return new LoadContextInfo(aIsPrivate,
                             aIsAnonymous,
                             aOriginAttributes);
}

} // namespace net
} // namespace mozilla
