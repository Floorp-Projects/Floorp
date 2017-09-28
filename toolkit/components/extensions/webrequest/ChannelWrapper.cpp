/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelWrapper.h"

#include "jsapi.h"
#include "xpcpublic.h"

#include "mozilla/BasePrincipal.h"
#include "SystemPrincipal.h"

#include "mozilla/AddonManagerWebAPI.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"
#include "nsIContentPolicy.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"
#include "nsIProxiedChannel.h"
#include "nsIProxyInfo.h"
#include "nsIWritablePropertyBag2.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

using namespace mozilla::dom;
using namespace JS;

namespace mozilla {
namespace extensions {

#define CHANNELWRAPPER_PROP_KEY NS_LITERAL_STRING("ChannelWrapper::CachedInstance")

/*****************************************************************************
 * Initialization
 *****************************************************************************/

/* static */

already_AddRefed<ChannelWrapper>
ChannelWrapper::Get(const GlobalObject& global, nsIChannel* channel)
{
  RefPtr<ChannelWrapper> wrapper;

  nsCOMPtr<nsIWritablePropertyBag2> props = do_QueryInterface(channel);
  if (props) {
    Unused << props->GetPropertyAsInterface(CHANNELWRAPPER_PROP_KEY,
                                            NS_GET_IID(ChannelWrapper),
                                            getter_AddRefs(wrapper));

    if (wrapper) {
      // Assume cached attributes may have changed at this point.
      wrapper->ClearCachedAttributes();
    }
  }

  if (!wrapper) {
    wrapper = new ChannelWrapper(global.GetAsSupports(), channel);
    if (props) {
      Unused << props->SetPropertyAsInterface(CHANNELWRAPPER_PROP_KEY,
                                              wrapper);
    }
  }

  return wrapper.forget();
}

void
ChannelWrapper::SetChannel(nsIChannel* aChannel)
{
  detail::ChannelHolder::SetChannel(aChannel);
  ClearCachedAttributes();
}

void
ChannelWrapper::ClearCachedAttributes()
{
  ChannelWrapperBinding::ClearCachedFinalURIValue(this);
  ChannelWrapperBinding::ClearCachedFinalURLValue(this);
  ChannelWrapperBinding::ClearCachedProxyInfoValue(this);
  ChannelWrapperBinding::ClearCachedRemoteAddressValue(this);
  ChannelWrapperBinding::ClearCachedStatusCodeValue(this);
  ChannelWrapperBinding::ClearCachedStatusLineValue(this);
}

/*****************************************************************************
 * ...
 *****************************************************************************/

void
ChannelWrapper::Cancel(uint32_t aResult, ErrorResult& aRv)
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
    rv = chan->Cancel(nsresult(aResult));
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
ChannelWrapper::RedirectTo(nsIURI* aURI, ErrorResult& aRv)
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    rv = chan->RedirectTo(aURI);
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
ChannelWrapper::SetSuspended(bool aSuspended, ErrorResult& aRv)
{
  if (aSuspended != mSuspended) {
    nsresult rv = NS_ERROR_UNEXPECTED;
    if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
      if (aSuspended) {
        rv = chan->Suspend();
      } else {
        rv = chan->Resume();
      }
    }
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
    } else {
      mSuspended = aSuspended;
    }
  }
}

void
ChannelWrapper::GetContentType(nsCString& aContentType) const
{
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->GetContentType(aContentType);
  }
}

void
ChannelWrapper::SetContentType(const nsACString& aContentType)
{
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->SetContentType(aContentType);
  }
}


/*****************************************************************************
 * Headers
 *****************************************************************************/

namespace {

class MOZ_STACK_CLASS HeaderVisitor final : public nsIHttpHeaderVisitor
{
public:
  NS_DECL_NSIHTTPHEADERVISITOR

  explicit HeaderVisitor(JSContext* aCx)
    : mCx(aCx)
    , mMap(aCx)
  {}

  JSObject* VisitRequestHeaders(nsIHttpChannel* aChannel, ErrorResult& aRv)
  {
    if (!Init()) {
      return nullptr;
    }
    if (!CheckResult(aChannel->VisitRequestHeaders(this), aRv)) {
      return nullptr;
    }
    return mMap;
  }

  JSObject* VisitResponseHeaders(nsIHttpChannel* aChannel, ErrorResult& aRv)
  {
    if (!Init()) {
      return nullptr;
    }
    if (!CheckResult(aChannel->VisitResponseHeaders(this), aRv)) {
      return nullptr;
    }
    return mMap;
  }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  // Stub AddRef/Release since this is a stack class.
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override
  {
    return ++mRefCnt;
  }

  NS_IMETHOD_(MozExternalRefCountType) Release(void) override
  {
    return --mRefCnt;
  }

  virtual ~HeaderVisitor()
  {
    MOZ_DIAGNOSTIC_ASSERT(mRefCnt == 0);
  }

private:
  bool Init()
  {
    mMap = NewMapObject(mCx);
    return mMap;
  }

  bool CheckResult(nsresult aNSRv, ErrorResult& aRv)
  {
    if (JS_IsExceptionPending(mCx)) {
      aRv.NoteJSContextException(mCx);
      return false;
    }
    if (NS_FAILED(aNSRv)) {
      aRv.Throw(aNSRv);
      return false;
    }
    return true;
  }

  JSContext* mCx;
  RootedObject mMap;

  nsrefcnt mRefCnt = 0;
};

NS_IMETHODIMP
HeaderVisitor::VisitHeader(const nsACString& aHeader, const nsACString& aValue)
{
  RootedValue header(mCx);
  RootedValue value(mCx);

  if (!xpc::NonVoidStringToJsval(mCx, NS_ConvertUTF8toUTF16(aHeader), &header) ||
      !xpc::NonVoidStringToJsval(mCx, NS_ConvertUTF8toUTF16(aValue), &value) ||
      !MapSet(mCx, mMap, header, value)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE(HeaderVisitor, nsIHttpHeaderVisitor)

} // anonymous namespace


void
ChannelWrapper::GetRequestHeaders(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal, ErrorResult& aRv) const
{
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    HeaderVisitor visitor(cx);
    aRetVal.set(visitor.VisitRequestHeaders(chan, aRv));
  } else {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

void
ChannelWrapper::GetResponseHeaders(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal, ErrorResult& aRv) const
{
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    HeaderVisitor visitor(cx);
    aRetVal.set(visitor.VisitResponseHeaders(chan, aRv));
  } else {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

void
ChannelWrapper::SetRequestHeader(const nsCString& aHeader, const nsCString& aValue, ErrorResult& aRv)
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    rv = chan->SetRequestHeader(aHeader, aValue, false);
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
ChannelWrapper::SetResponseHeader(const nsCString& aHeader, const nsCString& aValue, ErrorResult& aRv)
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    rv = chan->SetResponseHeader(aHeader, aValue, false);
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

/*****************************************************************************
 * LoadInfo
 *****************************************************************************/

already_AddRefed<nsILoadContext>
ChannelWrapper::GetLoadContext() const
{
  if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
    nsCOMPtr<nsILoadContext> ctxt;
    NS_QueryNotificationCallbacks(chan, ctxt);
    return ctxt.forget();
  }
  return nullptr;
}

already_AddRefed<nsIDOMElement>
ChannelWrapper::GetBrowserElement() const
{
  if (nsCOMPtr<nsILoadContext> ctxt = GetLoadContext()) {
    nsCOMPtr<nsIDOMElement> elem;
    if (NS_SUCCEEDED(ctxt->GetTopFrameElement(getter_AddRefs(elem)))) {
      return elem.forget();
    }
  }
  return nullptr;
}

static inline bool
IsSystemPrincipal(nsIPrincipal* aPrincipal)
{
  return BasePrincipal::Cast(aPrincipal)->Is<SystemPrincipal>();
}

bool
ChannelWrapper::IsSystemLoad() const
{
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (nsIPrincipal* prin = loadInfo->LoadingPrincipal()) {
      return IsSystemPrincipal(prin);
    }

    if (loadInfo->GetOuterWindowID() == loadInfo->GetTopOuterWindowID()) {
      return false;
    }

    if (nsIPrincipal* prin = loadInfo->PrincipalToInherit()) {
      return IsSystemPrincipal(prin);
    }
    if (nsIPrincipal* prin = loadInfo->TriggeringPrincipal()) {
      return IsSystemPrincipal(prin);
    }
  }
  return false;
}

bool
ChannelWrapper::GetCanModify(ErrorResult& aRv) const
{
  nsCOMPtr<nsIURI> uri = GetFinalURI(aRv);
  nsAutoCString spec;
  if (uri) {
    uri->GetSpec(spec);
  }
  if (!uri || AddonManagerWebAPI::IsValidSite(uri)) {
    return false;
  }

  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (nsIPrincipal* prin = loadInfo->LoadingPrincipal()) {
      if (IsSystemPrincipal(prin)) {
        return false;
      }

      if (prin->GetIsCodebasePrincipal() &&
          (NS_FAILED(prin->GetURI(getter_AddRefs(uri))) ||
           AddonManagerWebAPI::IsValidSite(uri))) {
          return false;
      }
    }
  }
  return true;
}

already_AddRefed<nsIURI>
ChannelWrapper::GetOriginURI() const
{
  nsCOMPtr<nsIURI> uri;
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (nsIPrincipal* prin = loadInfo->TriggeringPrincipal()) {
      if (prin->GetIsCodebasePrincipal()) {
        Unused << prin->GetURI(getter_AddRefs(uri));
      }
    }
  }
  return uri.forget();
}

already_AddRefed<nsIURI>
ChannelWrapper::GetDocumentURI() const
{
  nsCOMPtr<nsIURI> uri;
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (nsIPrincipal* prin = loadInfo->LoadingPrincipal()) {
      if (prin->GetIsCodebasePrincipal()) {
        Unused << prin->GetURI(getter_AddRefs(uri));
      }
    }
  }
  return uri.forget();
}


void
ChannelWrapper::GetOriginURL(nsCString& aRetVal) const
{
  if (nsCOMPtr<nsIURI> uri = GetOriginURI()) {
    Unused << uri->GetSpec(aRetVal);
  }
}

void
ChannelWrapper::GetDocumentURL(nsCString& aRetVal) const
{
  if (nsCOMPtr<nsIURI> uri = GetDocumentURI()) {
    Unused << uri->GetSpec(aRetVal);
  }
}

int64_t
NormalizeWindowID(nsILoadInfo* aLoadInfo, uint64_t windowID)
{
  if (windowID == aLoadInfo->GetTopOuterWindowID()) {
    return 0;
  }
  return windowID;
}

uint64_t
ChannelWrapper::WindowId(nsILoadInfo* aLoadInfo) const
{
  auto frameID = aLoadInfo->GetFrameOuterWindowID();
  if (!frameID) {
    frameID = aLoadInfo->GetOuterWindowID();
  }
  return frameID;
}

int64_t
ChannelWrapper::WindowId() const
{
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    return NormalizeWindowID(loadInfo, WindowId(loadInfo));
  }
  return 0;
}

int64_t
ChannelWrapper::ParentWindowId() const
{
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (WindowId(loadInfo) == loadInfo->GetTopOuterWindowID()) {
      return -1;
    }

    uint64_t parentID;
    if (loadInfo->GetFrameOuterWindowID()) {
      parentID = loadInfo->GetOuterWindowID();
    } else {
      parentID = loadInfo->GetParentOuterWindowID();
    }
    return NormalizeWindowID(loadInfo, parentID);
  }
  return -1;
}

/*****************************************************************************
 * ...
 *****************************************************************************/

MozContentPolicyType
GetContentPolicyType(uint32_t aType)
{
  // Note: Please keep this function in sync with the external types in
  // nsIContentPolicy.idl
  switch (aType) {
  case nsIContentPolicy::TYPE_DOCUMENT:
    return MozContentPolicyType::Main_frame;
  case nsIContentPolicy::TYPE_SUBDOCUMENT:
    return MozContentPolicyType::Sub_frame;
  case nsIContentPolicy::TYPE_STYLESHEET:
    return MozContentPolicyType::Stylesheet;
  case nsIContentPolicy::TYPE_SCRIPT:
    return MozContentPolicyType::Script;
  case nsIContentPolicy::TYPE_IMAGE:
    return MozContentPolicyType::Image;
  case nsIContentPolicy::TYPE_OBJECT:
    return MozContentPolicyType::Object;
  case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST:
    return MozContentPolicyType::Object_subrequest;
  case nsIContentPolicy::TYPE_XMLHTTPREQUEST:
    return MozContentPolicyType::Xmlhttprequest;
  // TYPE_FETCH returns xmlhttprequest for cross-browser compatibility.
  case nsIContentPolicy::TYPE_FETCH:
    return MozContentPolicyType::Xmlhttprequest;
  case nsIContentPolicy::TYPE_XBL:
    return MozContentPolicyType::Xbl;
  case nsIContentPolicy::TYPE_XSLT:
    return MozContentPolicyType::Xslt;
  case nsIContentPolicy::TYPE_PING:
    return MozContentPolicyType::Ping;
  case nsIContentPolicy::TYPE_BEACON:
    return MozContentPolicyType::Beacon;
  case nsIContentPolicy::TYPE_DTD:
    return MozContentPolicyType::Xml_dtd;
  case nsIContentPolicy::TYPE_FONT:
    return MozContentPolicyType::Font;
  case nsIContentPolicy::TYPE_MEDIA:
    return MozContentPolicyType::Media;
  case nsIContentPolicy::TYPE_WEBSOCKET:
    return MozContentPolicyType::Websocket;
  case nsIContentPolicy::TYPE_CSP_REPORT:
    return MozContentPolicyType::Csp_report;
  case nsIContentPolicy::TYPE_IMAGESET:
    return MozContentPolicyType::Imageset;
  case nsIContentPolicy::TYPE_WEB_MANIFEST:
    return MozContentPolicyType::Web_manifest;
  default:
    return MozContentPolicyType::Other;
  }
}

MozContentPolicyType
ChannelWrapper::Type() const
{
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    return GetContentPolicyType(loadInfo->GetExternalContentPolicyType());
  }
  return MozContentPolicyType::Other;
}

void
ChannelWrapper::GetMethod(nsCString& aMethod) const
{
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->GetRequestMethod(aMethod);
  }
}


/*****************************************************************************
 * ...
 *****************************************************************************/

uint32_t
ChannelWrapper::StatusCode() const
{
  uint32_t result = 0;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->GetResponseStatus(&result);
  }
  return result;
}

void
ChannelWrapper::GetStatusLine(nsCString& aRetVal) const
{
  nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel();
  nsCOMPtr<nsIHttpChannelInternal> internal = do_QueryInterface(chan);

  if (internal) {
    nsAutoCString statusText;
    uint32_t major, minor, status;
    if (NS_FAILED(chan->GetResponseStatus(&status)) ||
        NS_FAILED(chan->GetResponseStatusText(statusText)) ||
        NS_FAILED(internal->GetResponseVersion(&major, &minor))) {
      return;
    }

    aRetVal = nsPrintfCString("HTTP/%u.%u %u %s",
                              major, minor, status, statusText.get());
  }
}

/*****************************************************************************
 * ...
 *****************************************************************************/

already_AddRefed<nsIURI>
ChannelWrapper::GetFinalURI(ErrorResult& aRv) const
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIURI> uri;
  if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
    rv = NS_GetFinalChannelURI(chan, getter_AddRefs(uri));
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return uri.forget();;
}

void
ChannelWrapper::GetFinalURL(nsCString& aRetVal, ErrorResult& aRv) const
{
  nsCOMPtr<nsIURI> uri = GetFinalURI(aRv);
  if (uri) {
    Unused << uri->GetSpec(aRetVal);
  }
}

/*****************************************************************************
 * ...
 *****************************************************************************/

nsresult
FillProxyInfo(MozProxyInfo &aDict, nsIProxyInfo* aProxyInfo)
{
  MOZ_TRY(aProxyInfo->GetHost(aDict.mHost));
  MOZ_TRY(aProxyInfo->GetPort(&aDict.mPort));
  MOZ_TRY(aProxyInfo->GetType(aDict.mType));
  MOZ_TRY(aProxyInfo->GetUsername(aDict.mUsername));
  MOZ_TRY(aProxyInfo->GetFailoverTimeout(&aDict.mFailoverTimeout.Construct()));

  uint32_t flags;
  MOZ_TRY(aProxyInfo->GetFlags(&flags));
  aDict.mProxyDNS = flags & nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;

  return NS_OK;
}

void
ChannelWrapper::GetProxyInfo(dom::Nullable<MozProxyInfo>& aRetVal, ErrorResult& aRv) const
{
  nsCOMPtr<nsIProxyInfo> proxyInfo;
  if (nsCOMPtr<nsIProxiedChannel> proxied = QueryChannel()) {
    Unused << proxied->GetProxyInfo(getter_AddRefs(proxyInfo));
  }
  if (proxyInfo) {
    MozProxyInfo result;

    nsresult rv = FillProxyInfo(result, proxyInfo);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
    } else {
      aRetVal.SetValue(Move(result));
    }
  }
}

void
ChannelWrapper::GetRemoteAddress(nsCString& aRetVal) const
{
  aRetVal.SetIsVoid(true);
  if (nsCOMPtr<nsIHttpChannelInternal> internal = QueryChannel()) {
    Unused << internal->GetRemoteAddress(aRetVal);
  }
}

/*****************************************************************************
 * Glue
 *****************************************************************************/

JSObject*
ChannelWrapper::WrapObject(JSContext* aCx, HandleObject aGivenProto)
{
  return ChannelWrapperBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ChannelWrapper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChannelWrapper)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(ChannelWrapper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ChannelWrapper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ChannelWrapper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(ChannelWrapper)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ChannelWrapper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ChannelWrapper)

} // namespace extensions
} // namespace mozilla

