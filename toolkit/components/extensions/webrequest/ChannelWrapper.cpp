/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelWrapper.h"

#include "jsapi.h"
#include "xpcpublic.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/SystemPrincipal.h"

#include "NSSErrorsService.h"
#include "nsITransportSecurityInfo.h"

#include "mozilla/AddonManagerWebAPI.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/BrowserHost.h"
#include "nsIContentPolicy.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"
#include "nsIProxiedChannel.h"
#include "nsIProxyInfo.h"
#include "nsITraceableChannel.h"
#include "nsIWritablePropertyBag.h"
#include "nsIWritablePropertyBag2.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsPrintfCString.h"

using namespace mozilla::dom;
using namespace JS;

namespace mozilla {
namespace extensions {

#define CHANNELWRAPPER_PROP_KEY \
  NS_LITERAL_STRING("ChannelWrapper::CachedInstance")

using CF = nsIHttpChannel::ClassificationFlags;
using MUC = MozUrlClassificationFlags;

struct ClassificationStruct {
  uint32_t mFlag;
  MozUrlClassificationFlags mValue;
};
static const ClassificationStruct classificationArray[] = {
    {CF::CLASSIFIED_FINGERPRINTING, MUC::Fingerprinting},
    {CF::CLASSIFIED_FINGERPRINTING_CONTENT, MUC::Fingerprinting_content},
    {CF::CLASSIFIED_CRYPTOMINING, MUC::Cryptomining},
    {CF::CLASSIFIED_CRYPTOMINING_CONTENT, MUC::Cryptomining_content},
    {CF::CLASSIFIED_TRACKING, MUC::Tracking},
    {CF::CLASSIFIED_TRACKING_AD, MUC::Tracking_ad},
    {CF::CLASSIFIED_TRACKING_ANALYTICS, MUC::Tracking_analytics},
    {CF::CLASSIFIED_TRACKING_SOCIAL, MUC::Tracking_social},
    {CF::CLASSIFIED_TRACKING_CONTENT, MUC::Tracking_content},
    {CF::CLASSIFIED_SOCIALTRACKING, MUC::Socialtracking},
    {CF::CLASSIFIED_SOCIALTRACKING_FACEBOOK, MUC::Socialtracking_facebook},
    {CF::CLASSIFIED_SOCIALTRACKING_LINKEDIN, MUC::Socialtracking_linkedin},
    {CF::CLASSIFIED_SOCIALTRACKING_TWITTER, MUC::Socialtracking_twitter},
    {CF::CLASSIFIED_ANY_BASIC_TRACKING, MUC::Any_basic_tracking},
    {CF::CLASSIFIED_ANY_STRICT_TRACKING, MUC::Any_strict_tracking},
    {CF::CLASSIFIED_ANY_SOCIAL_TRACKING, MUC::Any_social_tracking}};

/*****************************************************************************
 * Lifetimes
 *****************************************************************************/

namespace {
class ChannelListHolder : public LinkedList<ChannelWrapper> {
 public:
  ChannelListHolder() : LinkedList<ChannelWrapper>() {}

  ~ChannelListHolder();
};

}  // anonymous namespace

ChannelListHolder::~ChannelListHolder() {
  while (ChannelWrapper* wrapper = popFirst()) {
    wrapper->Die();
  }
}

static LinkedList<ChannelWrapper>& ChannelList() {
  static UniquePtr<ChannelListHolder> sChannelList;
  if (!sChannelList) {
    sChannelList.reset(new ChannelListHolder());
    ClearOnShutdown(&sChannelList, ShutdownPhase::Shutdown);
  }
  return *sChannelList;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(ChannelWrapper::ChannelWrapperStub)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ChannelWrapper::ChannelWrapperStub)

NS_IMPL_CYCLE_COLLECTION(ChannelWrapper::ChannelWrapperStub, mChannelWrapper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChannelWrapper::ChannelWrapperStub)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(ChannelWrapper, mChannelWrapper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/*****************************************************************************
 * Initialization
 *****************************************************************************/

ChannelWrapper::ChannelWrapper(nsISupports* aParent, nsIChannel* aChannel)
    : ChannelHolder(aChannel), mParent(aParent) {
  mStub = new ChannelWrapperStub(this);

  ChannelList().insertBack(this);
}

ChannelWrapper::~ChannelWrapper() {
  if (LinkedListElement<ChannelWrapper>::isInList()) {
    LinkedListElement<ChannelWrapper>::remove();
  }
}

void ChannelWrapper::Die() {
  if (mStub) {
    mStub->mChannelWrapper = nullptr;
  }
}

/* static */
already_AddRefed<ChannelWrapper> ChannelWrapper::Get(const GlobalObject& global,
                                                     nsIChannel* channel) {
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
                                              wrapper->mStub);
    }
  }

  return wrapper.forget();
}

already_AddRefed<ChannelWrapper> ChannelWrapper::GetRegisteredChannel(
    const GlobalObject& global, uint64_t aChannelId,
    const WebExtensionPolicy& aAddon, nsIRemoteTab* aRemoteTab) {
  ContentParent* contentParent = nullptr;
  if (BrowserHost* host = BrowserHost::GetFrom(aRemoteTab)) {
    contentParent = host->GetActor()->Manager();
  }

  auto& webreq = WebRequestService::GetSingleton();

  nsCOMPtr<nsITraceableChannel> channel =
      webreq.GetTraceableChannel(aChannelId, aAddon.Id(), contentParent);
  if (!channel) {
    return nullptr;
  }
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(channel));
  return ChannelWrapper::Get(global, chan);
}

void ChannelWrapper::SetChannel(nsIChannel* aChannel) {
  detail::ChannelHolder::SetChannel(aChannel);
  ClearCachedAttributes();
  ChannelWrapper_Binding::ClearCachedFinalURIValue(this);
  ChannelWrapper_Binding::ClearCachedFinalURLValue(this);
  mFinalURLInfo.reset();
  ChannelWrapper_Binding::ClearCachedProxyInfoValue(this);
}

void ChannelWrapper::ClearCachedAttributes() {
  ChannelWrapper_Binding::ClearCachedRemoteAddressValue(this);
  ChannelWrapper_Binding::ClearCachedStatusCodeValue(this);
  ChannelWrapper_Binding::ClearCachedStatusLineValue(this);
  ChannelWrapper_Binding::ClearCachedUrlClassificationValue(this);
  if (!mFiredErrorEvent) {
    ChannelWrapper_Binding::ClearCachedErrorStringValue(this);
  }
}

/*****************************************************************************
 * ...
 *****************************************************************************/

void ChannelWrapper::Cancel(uint32_t aResult, ErrorResult& aRv) {
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
    rv = chan->Cancel(nsresult(aResult));
    ErrorCheck();
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void ChannelWrapper::RedirectTo(nsIURI* aURI, ErrorResult& aRv) {
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    rv = chan->RedirectTo(aURI);
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void ChannelWrapper::UpgradeToSecure(ErrorResult& aRv) {
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    rv = chan->UpgradeToSecure();
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void ChannelWrapper::SetSuspended(bool aSuspended, ErrorResult& aRv) {
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

void ChannelWrapper::GetContentType(nsCString& aContentType) const {
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->GetContentType(aContentType);
  }
}

void ChannelWrapper::SetContentType(const nsACString& aContentType) {
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->SetContentType(aContentType);
  }
}

/*****************************************************************************
 * Headers
 *****************************************************************************/

namespace {

class MOZ_STACK_CLASS HeaderVisitor final : public nsIHttpHeaderVisitor {
 public:
  NS_DECL_NSIHTTPHEADERVISITOR

  explicit HeaderVisitor(nsTArray<dom::MozHTTPHeader>& aHeaders)
      : mHeaders(aHeaders) {}

  HeaderVisitor(nsTArray<dom::MozHTTPHeader>& aHeaders,
                const nsCString& aContentTypeHdr)
      : mHeaders(aHeaders), mContentTypeHdr(aContentTypeHdr) {}

  void VisitRequestHeaders(nsIHttpChannel* aChannel, ErrorResult& aRv) {
    CheckResult(aChannel->VisitRequestHeaders(this), aRv);
  }

  void VisitResponseHeaders(nsIHttpChannel* aChannel, ErrorResult& aRv) {
    CheckResult(aChannel->VisitResponseHeaders(this), aRv);
  }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  // Stub AddRef/Release since this is a stack class.
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override {
    return ++mRefCnt;
  }

  NS_IMETHOD_(MozExternalRefCountType) Release(void) override {
    return --mRefCnt;
  }

  virtual ~HeaderVisitor() { MOZ_DIAGNOSTIC_ASSERT(mRefCnt == 0); }

 private:
  bool CheckResult(nsresult aNSRv, ErrorResult& aRv) {
    if (NS_FAILED(aNSRv)) {
      aRv.Throw(aNSRv);
      return false;
    }
    return true;
  }

  nsTArray<dom::MozHTTPHeader>& mHeaders;
  nsCString mContentTypeHdr = VoidCString();

  nsrefcnt mRefCnt = 0;
};

NS_IMETHODIMP
HeaderVisitor::VisitHeader(const nsACString& aHeader,
                           const nsACString& aValue) {
  auto dict = mHeaders.AppendElement(fallible);
  if (!dict) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  dict->mName = aHeader;

  if (!mContentTypeHdr.IsVoid() &&
      aHeader.LowerCaseEqualsLiteral("content-type")) {
    dict->mValue = mContentTypeHdr;
  } else {
    dict->mValue = aValue;
  }

  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE(HeaderVisitor, nsIHttpHeaderVisitor)

}  // anonymous namespace

void ChannelWrapper::GetRequestHeaders(nsTArray<dom::MozHTTPHeader>& aRetVal,
                                       ErrorResult& aRv) const {
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    HeaderVisitor visitor(aRetVal);
    visitor.VisitRequestHeaders(chan, aRv);
  } else {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

void ChannelWrapper::GetResponseHeaders(nsTArray<dom::MozHTTPHeader>& aRetVal,
                                        ErrorResult& aRv) const {
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    HeaderVisitor visitor(aRetVal, mContentTypeHdr);
    visitor.VisitResponseHeaders(chan, aRv);
  } else {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

void ChannelWrapper::SetRequestHeader(const nsCString& aHeader,
                                      const nsCString& aValue, bool aMerge,
                                      ErrorResult& aRv) {
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    rv = chan->SetRequestHeader(aHeader, aValue, aMerge);
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void ChannelWrapper::SetResponseHeader(const nsCString& aHeader,
                                       const nsCString& aValue, bool aMerge,
                                       ErrorResult& aRv) {
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    if (aHeader.LowerCaseEqualsLiteral("content-type")) {
      rv = chan->SetContentType(aValue);
      if (NS_SUCCEEDED(rv)) {
        mContentTypeHdr = aValue;
      }
    } else {
      rv = chan->SetResponseHeader(aHeader, aValue, aMerge);
    }
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

/*****************************************************************************
 * LoadInfo
 *****************************************************************************/

already_AddRefed<nsILoadContext> ChannelWrapper::GetLoadContext() const {
  if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
    nsCOMPtr<nsILoadContext> ctxt;
    NS_QueryNotificationCallbacks(chan, ctxt);
    return ctxt.forget();
  }
  return nullptr;
}

already_AddRefed<Element> ChannelWrapper::GetBrowserElement() const {
  if (nsCOMPtr<nsILoadContext> ctxt = GetLoadContext()) {
    RefPtr<Element> elem;
    if (NS_SUCCEEDED(ctxt->GetTopFrameElement(getter_AddRefs(elem)))) {
      return elem.forget();
    }
  }
  return nullptr;
}

static inline bool IsSystemPrincipal(nsIPrincipal* aPrincipal) {
  return BasePrincipal::Cast(aPrincipal)->Is<SystemPrincipal>();
}

bool ChannelWrapper::IsSystemLoad() const {
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

bool ChannelWrapper::CanModify() const {
  if (WebExtensionPolicy::IsRestrictedURI(FinalURLInfo())) {
    return false;
  }

  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (nsIPrincipal* prin = loadInfo->LoadingPrincipal()) {
      if (IsSystemPrincipal(prin)) {
        return false;
      }

      auto* docURI = DocumentURLInfo();
      if (docURI && WebExtensionPolicy::IsRestrictedURI(*docURI)) {
        return false;
      }
    }
  }
  return true;
}

already_AddRefed<nsIURI> ChannelWrapper::GetOriginURI() const {
  nsCOMPtr<nsIURI> uri;
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (nsIPrincipal* prin = loadInfo->TriggeringPrincipal()) {
      if (prin->GetIsContentPrincipal()) {
        Unused << prin->GetURI(getter_AddRefs(uri));
      }
    }
  }
  return uri.forget();
}

already_AddRefed<nsIURI> ChannelWrapper::GetDocumentURI() const {
  nsCOMPtr<nsIURI> uri;
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    if (nsIPrincipal* prin = loadInfo->LoadingPrincipal()) {
      if (prin->GetIsContentPrincipal()) {
        Unused << prin->GetURI(getter_AddRefs(uri));
      }
    }
  }
  return uri.forget();
}

void ChannelWrapper::GetOriginURL(nsCString& aRetVal) const {
  if (nsCOMPtr<nsIURI> uri = GetOriginURI()) {
    Unused << uri->GetSpec(aRetVal);
  }
}

void ChannelWrapper::GetDocumentURL(nsCString& aRetVal) const {
  if (nsCOMPtr<nsIURI> uri = GetDocumentURI()) {
    Unused << uri->GetSpec(aRetVal);
  }
}

const URLInfo& ChannelWrapper::FinalURLInfo() const {
  if (mFinalURLInfo.isNothing()) {
    ErrorResult rv;
    nsCOMPtr<nsIURI> uri = FinalURI();
    MOZ_ASSERT(uri);
    mFinalURLInfo.emplace(uri.get(), true);

    // If this is a WebSocket request, mangle the URL so that the scheme is
    // ws: or wss:, as appropriate.
    auto& url = mFinalURLInfo.ref();
    if (Type() == MozContentPolicyType::Websocket &&
        (url.Scheme() == nsGkAtoms::http || url.Scheme() == nsGkAtoms::https)) {
      nsAutoCString spec(url.CSpec());
      spec.Replace(0, 4, NS_LITERAL_CSTRING("ws"));

      Unused << NS_NewURI(getter_AddRefs(uri), spec);
      MOZ_RELEASE_ASSERT(uri);
      mFinalURLInfo.reset();
      mFinalURLInfo.emplace(uri.get(), true);
    }
  }
  return mFinalURLInfo.ref();
}

const URLInfo* ChannelWrapper::DocumentURLInfo() const {
  if (mDocumentURLInfo.isNothing()) {
    nsCOMPtr<nsIURI> uri = GetDocumentURI();
    if (!uri) {
      return nullptr;
    }
    mDocumentURLInfo.emplace(uri.get(), true);
  }
  return &mDocumentURLInfo.ref();
}

bool ChannelWrapper::Matches(
    const dom::MozRequestFilter& aFilter, const WebExtensionPolicy* aExtension,
    const dom::MozRequestMatchOptions& aOptions) const {
  if (!HaveChannel()) {
    return false;
  }

  if (!aFilter.mTypes.IsNull() && !aFilter.mTypes.Value().Contains(Type())) {
    return false;
  }

  auto& urlInfo = FinalURLInfo();
  if (aFilter.mUrls && !aFilter.mUrls->Matches(urlInfo)) {
    return false;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo();
  bool isPrivate =
      loadInfo && loadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
  if (!aFilter.mIncognito.IsNull() && aFilter.mIncognito.Value() != isPrivate) {
    return false;
  }

  if (aExtension) {
    // Verify extension access to private requests
    if (isPrivate && !aExtension->PrivateBrowsingAllowed()) {
      return false;
    }

    bool isProxy =
        aOptions.mIsProxy && aExtension->HasPermission(nsGkAtoms::proxy);
    // Proxies are allowed access to all urls, including restricted urls.
    if (!aExtension->CanAccessURI(urlInfo, false, !isProxy)) {
      return false;
    }

    // If this isn't the proxy phase of the request, check that the extension
    // has origin permissions for origin that originated the request.
    if (!isProxy) {
      if (IsSystemLoad()) {
        return false;
      }

      if (auto origin = DocumentURLInfo()) {
        nsAutoCString baseURL;
        aExtension->GetBaseURL(baseURL);

        if (!StringBeginsWith(origin->CSpec(), baseURL) &&
            !aExtension->CanAccessURI(*origin)) {
          return false;
        }
      }
    }
  }

  return true;
}

int64_t NormalizeWindowID(nsILoadInfo* aLoadInfo, uint64_t windowID) {
  if (windowID == aLoadInfo->GetTopOuterWindowID()) {
    return 0;
  }
  return windowID;
}

uint64_t ChannelWrapper::WindowId(nsILoadInfo* aLoadInfo) const {
  auto frameID = aLoadInfo->GetFrameOuterWindowID();
  if (!frameID) {
    frameID = aLoadInfo->GetOuterWindowID();
  }
  return frameID;
}

int64_t ChannelWrapper::WindowId() const {
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    return NormalizeWindowID(loadInfo, WindowId(loadInfo));
  }
  return 0;
}

int64_t ChannelWrapper::ParentWindowId() const {
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

void ChannelWrapper::GetFrameAncestors(
    dom::Nullable<nsTArray<dom::MozFrameAncestorInfo>>& aFrameAncestors,
    ErrorResult& aRv) const {
  nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo();
  if (!loadInfo || WindowId(loadInfo) == 0) {
    aFrameAncestors.SetNull();
    return;
  }

  nsresult rv = GetFrameAncestors(loadInfo, aFrameAncestors.SetValue());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

nsresult ChannelWrapper::GetFrameAncestors(
    nsILoadInfo* aLoadInfo,
    nsTArray<dom::MozFrameAncestorInfo>& aFrameAncestors) const {
  const nsTArray<nsCOMPtr<nsIPrincipal>>& ancestorPrincipals =
      aLoadInfo->AncestorPrincipals();
  const nsTArray<uint64_t>& ancestorOuterWindowIDs =
      aLoadInfo->AncestorOuterWindowIDs();
  uint32_t size = ancestorPrincipals.Length();
  MOZ_DIAGNOSTIC_ASSERT(size == ancestorOuterWindowIDs.Length());
  if (size != ancestorOuterWindowIDs.Length()) {
    return NS_ERROR_UNEXPECTED;
  }

  bool subFrame = aLoadInfo->GetExternalContentPolicyType() ==
                  nsIContentPolicy::TYPE_SUBDOCUMENT;
  if (!aFrameAncestors.SetCapacity(subFrame ? size : size + 1, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // The immediate parent is always the first element in the ancestor arrays,
  // however SUBDOCUMENTs do not have their immediate parent included, so we
  // inject it here. This will force wrapper.parentWindowId ==
  // wrapper.frameAncestors[0].frameId to always be true.  All ather requests
  // already match this way.
  if (subFrame) {
    auto ancestor = aFrameAncestors.AppendElement();
    GetDocumentURL(ancestor->mUrl);
    ancestor->mFrameId = ParentWindowId();
  }

  for (uint32_t i = 0; i < size; ++i) {
    auto ancestor = aFrameAncestors.AppendElement();
    nsCOMPtr<nsIURI> uri;
    MOZ_TRY(ancestorPrincipals[i]->GetURI(getter_AddRefs(uri)));
    if (!uri) {
      return NS_ERROR_UNEXPECTED;
    }
    MOZ_TRY(uri->GetSpec(ancestor->mUrl));
    ancestor->mFrameId =
        NormalizeWindowID(aLoadInfo, ancestorOuterWindowIDs[i]);
  }
  return NS_OK;
}

/*****************************************************************************
 * Response filtering
 *****************************************************************************/

void ChannelWrapper::RegisterTraceableChannel(const WebExtensionPolicy& aAddon,
                                              nsIRemoteTab* aBrowserParent) {
  // We can't attach new listeners after the response has started, so don't
  // bother registering anything.
  if (mResponseStarted || !CanModify()) {
    return;
  }

  mAddonEntries.Put(aAddon.Id(), aBrowserParent);
  if (!mChannelEntry) {
    mChannelEntry = WebRequestService::GetSingleton().RegisterChannel(this);
    CheckEventListeners();
  }
}

already_AddRefed<nsITraceableChannel> ChannelWrapper::GetTraceableChannel(
    nsAtom* aAddonId, dom::ContentParent* aContentParent) const {
  nsCOMPtr<nsIRemoteTab> remoteTab;
  if (mAddonEntries.Get(aAddonId, getter_AddRefs(remoteTab))) {
    ContentParent* contentParent = nullptr;
    if (remoteTab) {
      contentParent =
          BrowserHost::GetFrom(remoteTab.get())->GetActor()->Manager();
    }

    if (contentParent == aContentParent) {
      nsCOMPtr<nsITraceableChannel> chan = QueryChannel();
      return chan.forget();
    }
  }
  return nullptr;
}

/*****************************************************************************
 * ...
 *****************************************************************************/

MozContentPolicyType GetContentPolicyType(uint32_t aType) {
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
    case nsIContentPolicy::TYPE_SPECULATIVE:
      return MozContentPolicyType::Speculative;
    default:
      return MozContentPolicyType::Other;
  }
}

MozContentPolicyType ChannelWrapper::Type() const {
  if (nsCOMPtr<nsILoadInfo> loadInfo = GetLoadInfo()) {
    return GetContentPolicyType(loadInfo->GetExternalContentPolicyType());
  }
  return MozContentPolicyType::Other;
}

void ChannelWrapper::GetMethod(nsCString& aMethod) const {
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->GetRequestMethod(aMethod);
  }
}

/*****************************************************************************
 * ...
 *****************************************************************************/

uint32_t ChannelWrapper::StatusCode() const {
  uint32_t result = 0;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    Unused << chan->GetResponseStatus(&result);
  }
  return result;
}

void ChannelWrapper::GetStatusLine(nsCString& aRetVal) const {
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

    aRetVal = nsPrintfCString("HTTP/%u.%u %u %s", major, minor, status,
                              statusText.get());
  }
}

/*****************************************************************************
 * ...
 *****************************************************************************/

already_AddRefed<nsIURI> ChannelWrapper::FinalURI() const {
  nsCOMPtr<nsIURI> uri;
  if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
    NS_GetFinalChannelURI(chan, getter_AddRefs(uri));
  }
  return uri.forget();
}

void ChannelWrapper::GetFinalURL(nsString& aRetVal) const {
  if (HaveChannel()) {
    aRetVal = FinalURLInfo().Spec();
  }
}

/*****************************************************************************
 * ...
 *****************************************************************************/

nsresult FillProxyInfo(MozProxyInfo& aDict, nsIProxyInfo* aProxyInfo) {
  MOZ_TRY(aProxyInfo->GetHost(aDict.mHost));
  MOZ_TRY(aProxyInfo->GetPort(&aDict.mPort));
  MOZ_TRY(aProxyInfo->GetType(aDict.mType));
  MOZ_TRY(aProxyInfo->GetUsername(aDict.mUsername));
  MOZ_TRY(
      aProxyInfo->GetProxyAuthorizationHeader(aDict.mProxyAuthorizationHeader));
  MOZ_TRY(aProxyInfo->GetConnectionIsolationKey(aDict.mConnectionIsolationKey));
  MOZ_TRY(aProxyInfo->GetFailoverTimeout(&aDict.mFailoverTimeout.Construct()));

  uint32_t flags;
  MOZ_TRY(aProxyInfo->GetFlags(&flags));
  aDict.mProxyDNS = flags & nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;

  return NS_OK;
}

void ChannelWrapper::GetProxyInfo(dom::Nullable<MozProxyInfo>& aRetVal,
                                  ErrorResult& aRv) const {
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
      aRetVal.SetValue(std::move(result));
    }
  }
}

void ChannelWrapper::GetRemoteAddress(nsCString& aRetVal) const {
  aRetVal.SetIsVoid(true);
  if (nsCOMPtr<nsIHttpChannelInternal> internal = QueryChannel()) {
    Unused << internal->GetRemoteAddress(aRetVal);
  }
}

void FillClassification(
    Sequence<mozilla::dom::MozUrlClassificationFlags>& classifications,
    uint32_t classificationFlags, ErrorResult& aRv) {
  if (classificationFlags == 0) {
    return;
  }
  for (const auto& entry : classificationArray) {
    if (classificationFlags & entry.mFlag) {
      if (!classifications.AppendElement(entry.mValue, mozilla::fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }
  }
}

void ChannelWrapper::GetUrlClassification(
    dom::Nullable<dom::MozUrlClassification>& aRetVal, ErrorResult& aRv) const {
  MozUrlClassification classification;
  if (nsCOMPtr<nsIHttpChannel> chan = MaybeHttpChannel()) {
    uint32_t classificationFlags;
    chan->GetFirstPartyClassificationFlags(&classificationFlags);
    FillClassification(classification.mFirstParty, classificationFlags, aRv);
    if (aRv.Failed()) {
      return;
    }
    chan->GetThirdPartyClassificationFlags(&classificationFlags);
    FillClassification(classification.mThirdParty, classificationFlags, aRv);
  }
  aRetVal.SetValue(std::move(classification));
}

/*****************************************************************************
 * Error handling
 *****************************************************************************/

void ChannelWrapper::GetErrorString(nsString& aRetVal) const {
  if (nsCOMPtr<nsIChannel> chan = MaybeChannel()) {
    nsCOMPtr<nsISupports> securityInfo;
    Unused << chan->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (nsCOMPtr<nsITransportSecurityInfo> tsi =
            do_QueryInterface(securityInfo)) {
      int32_t errorCode = 0;
      tsi->GetErrorCode(&errorCode);
      if (psm::IsNSSErrorCode(errorCode)) {
        nsCOMPtr<nsINSSErrorsService> nsserr =
            do_GetService(NS_NSS_ERRORS_SERVICE_CONTRACTID);

        nsresult rv = psm::GetXPCOMFromNSSError(errorCode);
        if (nsserr && NS_SUCCEEDED(nsserr->GetErrorMessage(rv, aRetVal))) {
          return;
        }
      }
    }

    nsresult status;
    if (NS_SUCCEEDED(chan->GetStatus(&status)) && NS_FAILED(status)) {
      nsAutoCString name;
      GetErrorName(status, name);
      AppendUTF8toUTF16(name, aRetVal);
    } else {
      aRetVal.SetIsVoid(true);
    }
  } else {
    aRetVal.AssignLiteral("NS_ERROR_UNEXPECTED");
  }
}

void ChannelWrapper::ErrorCheck() {
  if (!mFiredErrorEvent) {
    nsAutoString error;
    GetErrorString(error);
    if (error.Length()) {
      mChannelEntry = nullptr;
      mFiredErrorEvent = true;
      ChannelWrapper_Binding::ClearCachedErrorStringValue(this);
      FireEvent(NS_LITERAL_STRING("error"));
    }
  }
}

/*****************************************************************************
 * nsIWebRequestListener
 *****************************************************************************/

NS_IMPL_ISUPPORTS(ChannelWrapper::RequestListener, nsIStreamListener,
                  nsIRequestObserver, nsIThreadRetargetableStreamListener)

ChannelWrapper::RequestListener::~RequestListener() {
  NS_ReleaseOnMainThreadSystemGroup("RequestListener::mChannelWrapper",
                                    mChannelWrapper.forget());
}

nsresult ChannelWrapper::RequestListener::Init() {
  if (nsCOMPtr<nsITraceableChannel> chan = mChannelWrapper->QueryChannel()) {
    return chan->SetNewListener(this, getter_AddRefs(mOrigStreamListener));
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
ChannelWrapper::RequestListener::OnStartRequest(nsIRequest* request) {
  MOZ_ASSERT(mOrigStreamListener, "Should have mOrigStreamListener");

  mChannelWrapper->mChannelEntry = nullptr;
  mChannelWrapper->mResponseStarted = true;
  mChannelWrapper->ErrorCheck();
  mChannelWrapper->FireEvent(NS_LITERAL_STRING("start"));

  return mOrigStreamListener->OnStartRequest(request);
}

NS_IMETHODIMP
ChannelWrapper::RequestListener::OnStopRequest(nsIRequest* request,
                                               nsresult aStatus) {
  MOZ_ASSERT(mOrigStreamListener, "Should have mOrigStreamListener");

  mChannelWrapper->mChannelEntry = nullptr;
  mChannelWrapper->ErrorCheck();
  mChannelWrapper->FireEvent(NS_LITERAL_STRING("stop"));

  return mOrigStreamListener->OnStopRequest(request, aStatus);
}

NS_IMETHODIMP
ChannelWrapper::RequestListener::OnDataAvailable(nsIRequest* request,
                                                 nsIInputStream* inStr,
                                                 uint64_t sourceOffset,
                                                 uint32_t count) {
  MOZ_ASSERT(mOrigStreamListener, "Should have mOrigStreamListener");
  return mOrigStreamListener->OnDataAvailable(request, inStr, sourceOffset,
                                              count);
}

NS_IMETHODIMP
ChannelWrapper::RequestListener::CheckListenerChain() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread!");
  nsresult rv;
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
      do_QueryInterface(mOrigStreamListener, &rv);
  if (retargetableListener) {
    return retargetableListener->CheckListenerChain();
  }
  return rv;
}

/*****************************************************************************
 * Event dispatching
 *****************************************************************************/

void ChannelWrapper::FireEvent(const nsAString& aType) {
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, aType, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

void ChannelWrapper::CheckEventListeners() {
  if (!mAddedStreamListener &&
      (HasListenersFor(nsGkAtoms::onerror) ||
       HasListenersFor(nsGkAtoms::onstart) ||
       HasListenersFor(nsGkAtoms::onstop) || mChannelEntry)) {
    auto listener = MakeRefPtr<RequestListener>(this);
    if (!NS_WARN_IF(NS_FAILED(listener->Init()))) {
      mAddedStreamListener = true;
    }
  }
}

void ChannelWrapper::EventListenerAdded(nsAtom* aType) {
  CheckEventListeners();
}

void ChannelWrapper::EventListenerRemoved(nsAtom* aType) {
  CheckEventListeners();
}

/*****************************************************************************
 * Glue
 *****************************************************************************/

JSObject* ChannelWrapper::WrapObject(JSContext* aCx, HandleObject aGivenProto) {
  return ChannelWrapper_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ChannelWrapper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChannelWrapper)
  NS_INTERFACE_MAP_ENTRY(ChannelWrapper)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ChannelWrapper,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStub)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ChannelWrapper,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStub)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ChannelWrapper,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(ChannelWrapper, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ChannelWrapper, DOMEventTargetHelper)

}  // namespace extensions
}  // namespace mozilla
