/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "necko-config.h"
#include "nsHttp.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/ExtensionProtocolHandler.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/FTPChannelParent.h"
#include "mozilla/net/WebSocketChannelParent.h"
#include "mozilla/net/WebSocketEventListenerParent.h"
#include "mozilla/net/DataChannelParent.h"
#include "mozilla/net/SimpleChannelParent.h"
#include "mozilla/net/AltDataOutputStreamParent.h"
#include "mozilla/Unused.h"
#include "mozilla/net/FileChannelParent.h"
#include "mozilla/net/DNSRequestParent.h"
#include "mozilla/net/ChannelDiverterParent.h"
#include "mozilla/net/ClassifierDummyChannelParent.h"
#include "mozilla/net/IPCTransportProvider.h"
#include "mozilla/net/RequestContextService.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/net/PSocketProcessBridgeParent.h"
#ifdef MOZ_WEBRTC
#  include "mozilla/net/ProxyConfigLookupParent.h"
#  include "mozilla/net/StunAddrsRequestParent.h"
#  include "mozilla/net/WebrtcProxyChannelParent.h"
#endif
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/network/TCPSocketParent.h"
#include "mozilla/dom/network/TCPServerSocketParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/LoadContext.h"
#include "mozilla/MozPromise.h"
#include "nsPrintfCString.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsEscape.h"
#include "SerializedLoadContext.h"
#include "nsAuthInformationHolder.h"
#include "nsIAuthPromptCallback.h"
#include "nsINetworkPredictor.h"
#include "nsINetworkPredictorVerifier.h"
#include "nsISpeculativeConnect.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"

using IPC::SerializedLoadContext;
using mozilla::OriginAttributes;
using mozilla::dom::BrowserParent;
using mozilla::dom::ChromeUtils;
using mozilla::dom::ContentParent;
using mozilla::dom::ServiceWorkerManager;
using mozilla::dom::TabContext;
using mozilla::dom::TCPServerSocketParent;
using mozilla::dom::TCPSocketParent;
using mozilla::dom::UDPSocketParent;
using mozilla::ipc::LoadInfoArgsToLoadInfo;
using mozilla::ipc::PrincipalInfo;
using mozilla::net::PTCPServerSocketParent;
using mozilla::net::PTCPSocketParent;
using mozilla::net::PUDPSocketParent;

namespace mozilla {
namespace net {

// C++ file contents
NeckoParent::NeckoParent() : mSocketProcessBridgeInited(false) {
  // Init HTTP protocol handler now since we need atomTable up and running very
  // early (IPDL argument handling for PHttpChannel constructor needs it) so
  // normal init (during 1st Http channel request) isn't early enough.
  nsCOMPtr<nsIProtocolHandler> proto =
      do_GetService("@mozilla.org/network/protocol;1?name=http");

  // only register once--we will have multiple NeckoParents if there are
  // multiple child processes.
  static bool registeredBool = false;
  if (!registeredBool) {
    Preferences::AddBoolVarCache(&NeckoCommonInternal::gSecurityDisabled,
                                 "network.disable.ipc.security");
    registeredBool = true;
  }
}

static PBOverrideStatus PBOverrideStatusFromLoadContext(
    const SerializedLoadContext& aSerialized) {
  if (!aSerialized.IsNotNull() && aSerialized.IsPrivateBitValid()) {
    return (aSerialized.mOriginAttributes.mPrivateBrowsingId > 0)
               ? kPBOverride_Private
               : kPBOverride_NotPrivate;
  }
  return kPBOverride_Unset;
}

static already_AddRefed<nsIPrincipal> GetRequestingPrincipal(
    const Maybe<LoadInfoArgs>& aOptionalLoadInfoArgs) {
  if (aOptionalLoadInfoArgs.isNothing()) {
    return nullptr;
  }

  const LoadInfoArgs& loadInfoArgs = aOptionalLoadInfoArgs.ref();
  const Maybe<PrincipalInfo>& optionalPrincipalInfo =
      loadInfoArgs.requestingPrincipalInfo();

  if (optionalPrincipalInfo.isNothing()) {
    return nullptr;
  }

  const PrincipalInfo& principalInfo = optionalPrincipalInfo.ref();

  return PrincipalInfoToPrincipal(principalInfo);
}

static already_AddRefed<nsIPrincipal> GetRequestingPrincipal(
    const HttpChannelCreationArgs& aArgs) {
  if (aArgs.type() != HttpChannelCreationArgs::THttpChannelOpenArgs) {
    return nullptr;
  }

  const HttpChannelOpenArgs& args = aArgs.get_HttpChannelOpenArgs();
  return GetRequestingPrincipal(args.loadInfo());
}

static already_AddRefed<nsIPrincipal> GetRequestingPrincipal(
    const FTPChannelCreationArgs& aArgs) {
  if (aArgs.type() != FTPChannelCreationArgs::TFTPChannelOpenArgs) {
    return nullptr;
  }

  const FTPChannelOpenArgs& args = aArgs.get_FTPChannelOpenArgs();
  return GetRequestingPrincipal(args.loadInfo());
}

// Bug 1289001 - If GetValidatedOriginAttributes returns an error string, that
// usually leads to a content crash with very little info about the cause.
// We prefer to crash on the parent, so we get the reason in the crash report.
static MOZ_COLD void CrashWithReason(const char* reason) {
#ifndef RELEASE_OR_BETA
  MOZ_CRASH_UNSAFE(reason);
#endif
}

const char* NeckoParent::GetValidatedOriginAttributes(
    const SerializedLoadContext& aSerialized, PContentParent* aContent,
    nsIPrincipal* aRequestingPrincipal, OriginAttributes& aAttrs) {
  if (!UsingNeckoIPCSecurity()) {
    if (!aSerialized.IsNotNull()) {
      // If serialized is null, we cannot validate anything. We have to assume
      // that this requests comes from a SystemPrincipal.
      aAttrs = OriginAttributes(false);
    } else {
      aAttrs = aSerialized.mOriginAttributes;
    }
    return nullptr;
  }

  if (!aSerialized.IsNotNull()) {
    CrashWithReason(
        "GetValidatedOriginAttributes | SerializedLoadContext from child is "
        "null");
    return "SerializedLoadContext from child is null";
  }

  nsAutoCString serializedSuffix;
  aSerialized.mOriginAttributes.CreateAnonymizedSuffix(serializedSuffix);

  nsAutoCString debugString;
  const auto& browsers = aContent->ManagedPBrowserParent();
  for (auto iter = browsers.ConstIter(); !iter.Done(); iter.Next()) {
    auto* browserParent = BrowserParent::GetFrom(iter.Get()->GetKey());

    if (!ChromeUtils::IsOriginAttributesEqual(
            aSerialized.mOriginAttributes,
            browserParent->OriginAttributesRef())) {
      debugString.AppendLiteral("(");
      debugString.Append(serializedSuffix);
      debugString.AppendLiteral(",");

      nsAutoCString tabSuffix;
      browserParent->OriginAttributesRef().CreateAnonymizedSuffix(tabSuffix);
      debugString.Append(tabSuffix);

      debugString.AppendLiteral(")");
      continue;
    }

    aAttrs = aSerialized.mOriginAttributes;
    return nullptr;
  }

  // This may be a ServiceWorker: when a push notification is received, FF wakes
  // up the corrisponding service worker so that it can manage the PushEvent. At
  // that time we probably don't have any valid tabcontext, but still, we want
  // to support http channel requests coming from that ServiceWorker.
  if (aRequestingPrincipal) {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm &&
        swm->MayHaveActiveServiceWorkerInstance(
            static_cast<ContentParent*>(aContent), aRequestingPrincipal)) {
      aAttrs = aSerialized.mOriginAttributes;
      return nullptr;
    }
  }

  nsAutoCString errorString;
  errorString.AppendLiteral(
      "GetValidatedOriginAttributes | App does not have permission -");
  errorString.Append(debugString);

  // Leak the buffer on the heap to make sure that it lives long enough, as
  // MOZ_CRASH_ANNOTATE expects the pointer passed to it to live to the end of
  // the program.
  char* error = strdup(errorString.BeginReading());
  CrashWithReason(error);
  return "App does not have permission";
}

const char* NeckoParent::CreateChannelLoadContext(
    const PBrowserOrId& aBrowser, PContentParent* aContent,
    const SerializedLoadContext& aSerialized,
    nsIPrincipal* aRequestingPrincipal, nsCOMPtr<nsILoadContext>& aResult) {
  OriginAttributes attrs;
  const char* error = GetValidatedOriginAttributes(aSerialized, aContent,
                                                   aRequestingPrincipal, attrs);
  if (error) {
    return error;
  }

  // if !UsingNeckoIPCSecurity(), we may not have a LoadContext to set. This is
  // the common case for most xpcshell tests.
  if (aSerialized.IsNotNull()) {
    attrs.SyncAttributesWithPrivateBrowsing(
        aSerialized.mOriginAttributes.mPrivateBrowsingId > 0);
    switch (aBrowser.type()) {
      case PBrowserOrId::TPBrowserParent: {
        RefPtr<BrowserParent> browserParent =
            BrowserParent::GetFrom(aBrowser.get_PBrowserParent());
        dom::Element* topFrameElement = nullptr;
        if (browserParent) {
          topFrameElement = browserParent->GetOwnerElement();
        }
        aResult = new LoadContext(aSerialized, topFrameElement, attrs);
        break;
      }
      case PBrowserOrId::TTabId: {
        aResult = new LoadContext(aSerialized, aBrowser.get_TabId(), attrs);
        break;
      }
      default:
        MOZ_CRASH();
    }
  }

  return nullptr;
}

void NeckoParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Nothing needed here. Called right before destructor since this is a
  // non-refcounted class.
}

already_AddRefed<PHttpChannelParent> NeckoParent::AllocPHttpChannelParent(
    const PBrowserOrId& aBrowser, const SerializedLoadContext& aSerialized,
    const HttpChannelCreationArgs& aOpenArgs) {
  nsCOMPtr<nsIPrincipal> requestingPrincipal =
      GetRequestingPrincipal(aOpenArgs);

  nsCOMPtr<nsILoadContext> loadContext;
  const char* error = CreateChannelLoadContext(
      aBrowser, Manager(), aSerialized, requestingPrincipal, loadContext);
  if (error) {
    printf_stderr(
        "NeckoParent::AllocPHttpChannelParent: "
        "FATAL error: %s: KILLING CHILD PROCESS\n",
        error);
    return nullptr;
  }
  PBOverrideStatus overrideStatus =
      PBOverrideStatusFromLoadContext(aSerialized);
  RefPtr<HttpChannelParent> p =
      new HttpChannelParent(aBrowser, loadContext, overrideStatus);
  return p.forget();
}

mozilla::ipc::IPCResult NeckoParent::RecvPHttpChannelConstructor(
    PHttpChannelParent* aActor, const PBrowserOrId& aBrowser,
    const SerializedLoadContext& aSerialized,
    const HttpChannelCreationArgs& aOpenArgs) {
  HttpChannelParent* p = static_cast<HttpChannelParent*>(aActor);
  if (!p->Init(aOpenArgs)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PStunAddrsRequestParent* NeckoParent::AllocPStunAddrsRequestParent() {
#ifdef MOZ_WEBRTC
  StunAddrsRequestParent* p = new StunAddrsRequestParent();
  p->AddRef();
  return p;
#else
  return nullptr;
#endif
}

bool NeckoParent::DeallocPStunAddrsRequestParent(
    PStunAddrsRequestParent* aActor) {
#ifdef MOZ_WEBRTC
  StunAddrsRequestParent* p = static_cast<StunAddrsRequestParent*>(aActor);
  p->Release();
#endif
  return true;
}

PWebrtcProxyChannelParent* NeckoParent::AllocPWebrtcProxyChannelParent(
    const TabId& aTabId) {
#ifdef MOZ_WEBRTC
  WebrtcProxyChannelParent* parent = new WebrtcProxyChannelParent(aTabId);
  parent->AddRef();
  return parent;
#else
  return nullptr;
#endif
}

bool NeckoParent::DeallocPWebrtcProxyChannelParent(
    PWebrtcProxyChannelParent* aActor) {
#ifdef MOZ_WEBRTC
  WebrtcProxyChannelParent* parent =
      static_cast<WebrtcProxyChannelParent*>(aActor);
  parent->Release();
#endif
  return true;
}

PAltDataOutputStreamParent* NeckoParent::AllocPAltDataOutputStreamParent(
    const nsCString& type, const int64_t& predictedSize,
    PHttpChannelParent* channel) {
  HttpChannelParent* chan = static_cast<HttpChannelParent*>(channel);
  nsCOMPtr<nsIAsyncOutputStream> stream;
  nsresult rv = chan->OpenAlternativeOutputStream(type, predictedSize,
                                                  getter_AddRefs(stream));
  AltDataOutputStreamParent* parent = new AltDataOutputStreamParent(stream);
  parent->AddRef();
  // If the return value was not NS_OK, the error code will be sent
  // asynchronously to the child, after receiving the first message.
  parent->SetError(rv);
  return parent;
}

bool NeckoParent::DeallocPAltDataOutputStreamParent(
    PAltDataOutputStreamParent* aActor) {
  AltDataOutputStreamParent* parent =
      static_cast<AltDataOutputStreamParent*>(aActor);
  parent->Release();
  return true;
}

PFTPChannelParent* NeckoParent::AllocPFTPChannelParent(
    const PBrowserOrId& aBrowser, const SerializedLoadContext& aSerialized,
    const FTPChannelCreationArgs& aOpenArgs) {
  nsCOMPtr<nsIPrincipal> requestingPrincipal =
      GetRequestingPrincipal(aOpenArgs);

  nsCOMPtr<nsILoadContext> loadContext;
  const char* error = CreateChannelLoadContext(
      aBrowser, Manager(), aSerialized, requestingPrincipal, loadContext);
  if (error) {
    printf_stderr(
        "NeckoParent::AllocPFTPChannelParent: "
        "FATAL error: %s: KILLING CHILD PROCESS\n",
        error);
    return nullptr;
  }
  PBOverrideStatus overrideStatus =
      PBOverrideStatusFromLoadContext(aSerialized);
  FTPChannelParent* p =
      new FTPChannelParent(aBrowser, loadContext, overrideStatus);
  p->AddRef();
  return p;
}

bool NeckoParent::DeallocPFTPChannelParent(PFTPChannelParent* channel) {
  FTPChannelParent* p = static_cast<FTPChannelParent*>(channel);
  p->Release();
  return true;
}

mozilla::ipc::IPCResult NeckoParent::RecvPFTPChannelConstructor(
    PFTPChannelParent* aActor, const PBrowserOrId& aBrowser,
    const SerializedLoadContext& aSerialized,
    const FTPChannelCreationArgs& aOpenArgs) {
  FTPChannelParent* p = static_cast<FTPChannelParent*>(aActor);
  if (!p->Init(aOpenArgs)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PCookieServiceParent* NeckoParent::AllocPCookieServiceParent() {
  return new CookieServiceParent();
}

bool NeckoParent::DeallocPCookieServiceParent(PCookieServiceParent* cs) {
  delete cs;
  return true;
}

PWebSocketParent* NeckoParent::AllocPWebSocketParent(
    const PBrowserOrId& browser, const SerializedLoadContext& serialized,
    const uint32_t& aSerial) {
  nsCOMPtr<nsILoadContext> loadContext;
  const char* error = CreateChannelLoadContext(browser, Manager(), serialized,
                                               nullptr, loadContext);
  if (error) {
    printf_stderr(
        "NeckoParent::AllocPWebSocketParent: "
        "FATAL error: %s: KILLING CHILD PROCESS\n",
        error);
    return nullptr;
  }

  RefPtr<BrowserParent> browserParent =
      BrowserParent::GetFrom(browser.get_PBrowserParent());
  PBOverrideStatus overrideStatus = PBOverrideStatusFromLoadContext(serialized);
  WebSocketChannelParent* p = new WebSocketChannelParent(
      browserParent, loadContext, overrideStatus, aSerial);
  p->AddRef();
  return p;
}

bool NeckoParent::DeallocPWebSocketParent(PWebSocketParent* actor) {
  WebSocketChannelParent* p = static_cast<WebSocketChannelParent*>(actor);
  p->Release();
  return true;
}

PWebSocketEventListenerParent* NeckoParent::AllocPWebSocketEventListenerParent(
    const uint64_t& aInnerWindowID) {
  RefPtr<WebSocketEventListenerParent> c =
      new WebSocketEventListenerParent(aInnerWindowID);
  return c.forget().take();
}

bool NeckoParent::DeallocPWebSocketEventListenerParent(
    PWebSocketEventListenerParent* aActor) {
  RefPtr<WebSocketEventListenerParent> c =
      dont_AddRef(static_cast<WebSocketEventListenerParent*>(aActor));
  MOZ_ASSERT(c);
  return true;
}

already_AddRefed<PDataChannelParent> NeckoParent::AllocPDataChannelParent(
    const uint32_t& channelId) {
  RefPtr<DataChannelParent> p = new DataChannelParent();
  return p.forget();
}

mozilla::ipc::IPCResult NeckoParent::RecvPDataChannelConstructor(
    PDataChannelParent* actor, const uint32_t& channelId) {
  DataChannelParent* p = static_cast<DataChannelParent*>(actor);
  DebugOnly<bool> rv = p->Init(channelId);
  MOZ_ASSERT(rv);
  return IPC_OK();
}

PSimpleChannelParent* NeckoParent::AllocPSimpleChannelParent(
    const uint32_t& channelId) {
  RefPtr<SimpleChannelParent> p = new SimpleChannelParent();
  return p.forget().take();
}

bool NeckoParent::DeallocPSimpleChannelParent(PSimpleChannelParent* actor) {
  RefPtr<SimpleChannelParent> p =
      dont_AddRef(actor).downcast<SimpleChannelParent>();
  return true;
}

mozilla::ipc::IPCResult NeckoParent::RecvPSimpleChannelConstructor(
    PSimpleChannelParent* actor, const uint32_t& channelId) {
  SimpleChannelParent* p = static_cast<SimpleChannelParent*>(actor);
  MOZ_ALWAYS_TRUE(p->Init(channelId));
  return IPC_OK();
}

already_AddRefed<PFileChannelParent> NeckoParent::AllocPFileChannelParent(
    const uint32_t& channelId) {
  RefPtr<FileChannelParent> p = new FileChannelParent();
  return p.forget();
}

mozilla::ipc::IPCResult NeckoParent::RecvPFileChannelConstructor(
    PFileChannelParent* actor, const uint32_t& channelId) {
  FileChannelParent* p = static_cast<FileChannelParent*>(actor);
  DebugOnly<bool> rv = p->Init(channelId);
  MOZ_ASSERT(rv);
  return IPC_OK();
}

PTCPSocketParent* NeckoParent::AllocPTCPSocketParent(
    const nsString& /* host */, const uint16_t& /* port */) {
  // We actually don't need host/port to construct a TCPSocketParent since
  // TCPSocketParent will maintain an internal nsIDOMTCPSocket instance which
  // can be delegated to get the host/port.
  TCPSocketParent* p = new TCPSocketParent();
  p->AddIPDLReference();
  return p;
}

bool NeckoParent::DeallocPTCPSocketParent(PTCPSocketParent* actor) {
  TCPSocketParent* p = static_cast<TCPSocketParent*>(actor);
  p->ReleaseIPDLReference();
  return true;
}

PTCPServerSocketParent* NeckoParent::AllocPTCPServerSocketParent(
    const uint16_t& aLocalPort, const uint16_t& aBacklog,
    const bool& aUseArrayBuffers) {
  TCPServerSocketParent* p =
      new TCPServerSocketParent(this, aLocalPort, aBacklog, aUseArrayBuffers);
  p->AddIPDLReference();
  return p;
}

mozilla::ipc::IPCResult NeckoParent::RecvPTCPServerSocketConstructor(
    PTCPServerSocketParent* aActor, const uint16_t& aLocalPort,
    const uint16_t& aBacklog, const bool& aUseArrayBuffers) {
  static_cast<TCPServerSocketParent*>(aActor)->Init();
  return IPC_OK();
}

bool NeckoParent::DeallocPTCPServerSocketParent(PTCPServerSocketParent* actor) {
  TCPServerSocketParent* p = static_cast<TCPServerSocketParent*>(actor);
  p->ReleaseIPDLReference();
  return true;
}

PUDPSocketParent* NeckoParent::AllocPUDPSocketParent(
    nsIPrincipal* /* unused */, const nsCString& /* unused */) {
  RefPtr<UDPSocketParent> p = new UDPSocketParent(this);

  return p.forget().take();
}

mozilla::ipc::IPCResult NeckoParent::RecvPUDPSocketConstructor(
    PUDPSocketParent* aActor, nsIPrincipal* aPrincipal,
    const nsCString& aFilter) {
  if (!static_cast<UDPSocketParent*>(aActor)->Init(aPrincipal, aFilter)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool NeckoParent::DeallocPUDPSocketParent(PUDPSocketParent* actor) {
  UDPSocketParent* p = static_cast<UDPSocketParent*>(actor);
  p->Release();
  return true;
}

PDNSRequestParent* NeckoParent::AllocPDNSRequestParent(
    const nsCString& aHost, const OriginAttributes& aOriginAttributes,
    const uint32_t& aFlags) {
  DNSRequestParent* p = new DNSRequestParent();
  p->AddRef();
  return p;
}

mozilla::ipc::IPCResult NeckoParent::RecvPDNSRequestConstructor(
    PDNSRequestParent* aActor, const nsCString& aHost,
    const OriginAttributes& aOriginAttributes, const uint32_t& aFlags) {
  static_cast<DNSRequestParent*>(aActor)->DoAsyncResolve(
      aHost, aOriginAttributes, aFlags);
  return IPC_OK();
}

bool NeckoParent::DeallocPDNSRequestParent(PDNSRequestParent* aParent) {
  DNSRequestParent* p = static_cast<DNSRequestParent*>(aParent);
  p->Release();
  return true;
}

mozilla::ipc::IPCResult NeckoParent::RecvSpeculativeConnect(
    const URIParams& aURI, nsIPrincipal* aPrincipal, const bool& aAnonymous) {
  nsCOMPtr<nsISpeculativeConnect> speculator(gIOService);
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  if (uri && speculator) {
    if (aAnonymous) {
      speculator->SpeculativeAnonymousConnect(uri, principal, nullptr);
    } else {
      speculator->SpeculativeConnect(uri, principal, nullptr);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvHTMLDNSPrefetch(
    const nsString& hostname, const bool& isHttps,
    const OriginAttributes& aOriginAttributes, const uint16_t& flags) {
  nsHTMLDNSPrefetch::Prefetch(hostname, isHttps, aOriginAttributes, flags);
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvCancelHTMLDNSPrefetch(
    const nsString& hostname, const bool& isHttps,
    const OriginAttributes& aOriginAttributes, const uint16_t& flags,
    const nsresult& reason) {
  nsHTMLDNSPrefetch::CancelPrefetch(hostname, isHttps, aOriginAttributes, flags,
                                    reason);
  return IPC_OK();
}

PChannelDiverterParent* NeckoParent::AllocPChannelDiverterParent(
    const ChannelDiverterArgs& channel) {
  return new ChannelDiverterParent();
}

mozilla::ipc::IPCResult NeckoParent::RecvPChannelDiverterConstructor(
    PChannelDiverterParent* actor, const ChannelDiverterArgs& channel) {
  auto parent = static_cast<ChannelDiverterParent*>(actor);
  parent->Init(channel);
  return IPC_OK();
}

bool NeckoParent::DeallocPChannelDiverterParent(
    PChannelDiverterParent* parent) {
  delete static_cast<ChannelDiverterParent*>(parent);
  return true;
}

PTransportProviderParent* NeckoParent::AllocPTransportProviderParent() {
  RefPtr<TransportProviderParent> res = new TransportProviderParent();
  return res.forget().take();
}

bool NeckoParent::DeallocPTransportProviderParent(
    PTransportProviderParent* aActor) {
  RefPtr<TransportProviderParent> provider =
      dont_AddRef(static_cast<TransportProviderParent*>(aActor));
  return true;
}

namespace {
std::map<uint64_t, nsCOMPtr<nsIAuthPromptCallback> >& CallbackMap() {
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<uint64_t, nsCOMPtr<nsIAuthPromptCallback> > sCallbackMap;
  return sCallbackMap;
}
}  // namespace

NS_IMPL_ISUPPORTS(NeckoParent::NestedFrameAuthPrompt, nsIAuthPrompt2)

NeckoParent::NestedFrameAuthPrompt::NestedFrameAuthPrompt(PNeckoParent* aParent,
                                                          TabId aNestedFrameId)
    : mNeckoParent(aParent), mNestedFrameId(aNestedFrameId) {}

NS_IMETHODIMP
NeckoParent::NestedFrameAuthPrompt::AsyncPromptAuth(
    nsIChannel* aChannel, nsIAuthPromptCallback* callback, nsISupports*,
    uint32_t, nsIAuthInformation* aInfo, nsICancelable**) {
  static uint64_t callbackId = 0;
  MOZ_ASSERT(XRE_IsParentProcess());
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString spec;
  if (uri) {
    rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsString realm;
  rv = aInfo->GetRealm(realm);
  NS_ENSURE_SUCCESS(rv, rv);
  callbackId++;
  if (mNeckoParent->SendAsyncAuthPromptForNestedFrame(mNestedFrameId, spec,
                                                      realm, callbackId)) {
    CallbackMap()[callbackId] = callback;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

mozilla::ipc::IPCResult NeckoParent::RecvOnAuthAvailable(
    const uint64_t& aCallbackId, const nsString& aUser,
    const nsString& aPassword, const nsString& aDomain) {
  nsCOMPtr<nsIAuthPromptCallback> callback = CallbackMap()[aCallbackId];
  if (!callback) {
    return IPC_OK();
  }
  CallbackMap().erase(aCallbackId);

  RefPtr<nsAuthInformationHolder> holder =
      new nsAuthInformationHolder(0, EmptyString(), EmptyCString());
  holder->SetUsername(aUser);
  holder->SetPassword(aPassword);
  holder->SetDomain(aDomain);

  callback->OnAuthAvailable(nullptr, holder);
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvOnAuthCancelled(
    const uint64_t& aCallbackId, const bool& aUserCancel) {
  nsCOMPtr<nsIAuthPromptCallback> callback = CallbackMap()[aCallbackId];
  if (!callback) {
    return IPC_OK();
  }
  CallbackMap().erase(aCallbackId);
  callback->OnAuthCancelled(nullptr, aUserCancel);
  return IPC_OK();
}

/* Predictor Messages */
mozilla::ipc::IPCResult NeckoParent::RecvPredPredict(
    const Maybe<ipc::URIParams>& aTargetURI,
    const Maybe<ipc::URIParams>& aSourceURI, const uint32_t& aReason,
    const OriginAttributes& aOriginAttributes, const bool& hasVerifier) {
  nsCOMPtr<nsIURI> targetURI = DeserializeURI(aTargetURI);
  nsCOMPtr<nsIURI> sourceURI = DeserializeURI(aSourceURI);

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
      do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsINetworkPredictorVerifier> verifier;
  if (hasVerifier) {
    verifier = do_QueryInterface(predictor);
  }
  predictor->PredictNative(targetURI, sourceURI, aReason, aOriginAttributes,
                           verifier);
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvPredLearn(
    const ipc::URIParams& aTargetURI, const Maybe<ipc::URIParams>& aSourceURI,
    const uint32_t& aReason, const OriginAttributes& aOriginAttributes) {
  nsCOMPtr<nsIURI> targetURI = DeserializeURI(aTargetURI);
  nsCOMPtr<nsIURI> sourceURI = DeserializeURI(aSourceURI);

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
      do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->LearnNative(targetURI, sourceURI, aReason, aOriginAttributes);
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvPredReset() {
  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
      do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->Reset();
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvRequestContextLoadBegin(
    const uint64_t& rcid) {
  nsCOMPtr<nsIRequestContextService> rcsvc =
      RequestContextService::GetOrCreate();
  if (!rcsvc) {
    return IPC_OK();
  }
  nsCOMPtr<nsIRequestContext> rc;
  rcsvc->GetRequestContext(rcid, getter_AddRefs(rc));
  if (rc) {
    rc->BeginLoad();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvRequestContextAfterDOMContentLoaded(
    const uint64_t& rcid) {
  nsCOMPtr<nsIRequestContextService> rcsvc =
      RequestContextService::GetOrCreate();
  if (!rcsvc) {
    return IPC_OK();
  }
  nsCOMPtr<nsIRequestContext> rc;
  rcsvc->GetRequestContext(rcid, getter_AddRefs(rc));
  if (rc) {
    rc->DOMContentLoaded();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvRemoveRequestContext(
    const uint64_t& rcid) {
  nsCOMPtr<nsIRequestContextService> rcsvc =
      RequestContextService::GetOrCreate();
  if (!rcsvc) {
    return IPC_OK();
  }

  rcsvc->RemoveRequestContext(rcid);

  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvGetExtensionStream(
    const URIParams& aURI, GetExtensionStreamResolver&& aResolve) {
  nsCOMPtr<nsIURI> deserializedURI = DeserializeURI(aURI);
  if (!deserializedURI) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<ExtensionProtocolHandler> ph(ExtensionProtocolHandler::GetSingleton());
  MOZ_ASSERT(ph);

  // Ask the ExtensionProtocolHandler to give us a new input stream for
  // this URI. The request comes from an ExtensionProtocolHandler in the
  // child process, but is not guaranteed to be a valid moz-extension URI,
  // and not guaranteed to represent a resource that the child should be
  // allowed to access. The ExtensionProtocolHandler is responsible for
  // validating the request. Specifically, only URI's for local files that
  // an extension is allowed to access via moz-extension URI's should be
  // accepted.
  nsCOMPtr<nsIInputStream> inputStream;
  bool terminateSender = true;
  auto inputStreamOrReason = ph->NewStream(deserializedURI, &terminateSender);
  if (inputStreamOrReason.isOk()) {
    inputStream = inputStreamOrReason.unwrap();
  }

  // If NewStream failed, we send back an invalid stream to the child so
  // it can handle the error. MozPromise rejection is reserved for channel
  // errors/disconnects.
  aResolve(inputStream);

  if (terminateSender) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvGetExtensionFD(
    const URIParams& aURI, GetExtensionFDResolver&& aResolve) {
  nsCOMPtr<nsIURI> deserializedURI = DeserializeURI(aURI);
  if (!deserializedURI) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<ExtensionProtocolHandler> ph(ExtensionProtocolHandler::GetSingleton());
  MOZ_ASSERT(ph);

  // Ask the ExtensionProtocolHandler to give us a new input stream for
  // this URI. The request comes from an ExtensionProtocolHandler in the
  // child process, but is not guaranteed to be a valid moz-extension URI,
  // and not guaranteed to represent a resource that the child should be
  // allowed to access. The ExtensionProtocolHandler is responsible for
  // validating the request. Specifically, only URI's for local files that
  // an extension is allowed to access via moz-extension URI's should be
  // accepted.
  bool terminateSender = true;
  auto result = ph->NewFD(deserializedURI, &terminateSender, aResolve);

  if (result.isErr() && terminateSender) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (result.isErr()) {
    FileDescriptor invalidFD;
    aResolve(invalidFD);
  }

  return IPC_OK();
}

PClassifierDummyChannelParent* NeckoParent::AllocPClassifierDummyChannelParent(
    nsIURI* aURI, nsIURI* aTopWindowURI,
    nsIPrincipal* aContentBlockingAllowListPrincipal,
    const nsresult& aTopWindowURIResult, const Maybe<LoadInfoArgs>& aLoadInfo) {
  RefPtr<ClassifierDummyChannelParent> c = new ClassifierDummyChannelParent();
  return c.forget().take();
}

mozilla::ipc::IPCResult NeckoParent::RecvPClassifierDummyChannelConstructor(
    PClassifierDummyChannelParent* aActor, nsIURI* aURI, nsIURI* aTopWindowURI,
    nsIPrincipal* aContentBlockingAllowListPrincipal,
    const nsresult& aTopWindowURIResult, const Maybe<LoadInfoArgs>& aLoadInfo) {
  ClassifierDummyChannelParent* p =
      static_cast<ClassifierDummyChannelParent*>(aActor);

  if (NS_WARN_IF(!aURI)) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  nsresult rv = LoadInfoArgsToLoadInfo(aLoadInfo, getter_AddRefs(loadInfo));
  if (NS_WARN_IF(NS_FAILED(rv)) || !loadInfo) {
    return IPC_FAIL_NO_REASON(this);
  }

  p->Init(aURI, aTopWindowURI, aContentBlockingAllowListPrincipal,
          aTopWindowURIResult, loadInfo);
  return IPC_OK();
}

bool NeckoParent::DeallocPClassifierDummyChannelParent(
    PClassifierDummyChannelParent* aActor) {
  RefPtr<ClassifierDummyChannelParent> c =
      dont_AddRef(static_cast<ClassifierDummyChannelParent*>(aActor));
  MOZ_ASSERT(c);
  return true;
}

mozilla::ipc::IPCResult NeckoParent::RecvInitSocketProcessBridge(
    InitSocketProcessBridgeResolver&& aResolver) {
  MOZ_ASSERT(NS_IsMainThread());

  Endpoint<PSocketProcessBridgeChild> invalidEndpoint;
  if (NS_WARN_IF(mSocketProcessBridgeInited)) {
    aResolver(std::move(invalidEndpoint));
    return IPC_OK();
  }

  SocketProcessParent* parent = SocketProcessParent::GetSingleton();
  if (NS_WARN_IF(!parent)) {
    aResolver(std::move(invalidEndpoint));
    return IPC_OK();
  }

  Endpoint<PSocketProcessBridgeParent> parentEndpoint;
  Endpoint<PSocketProcessBridgeChild> childEndpoint;
  if (NS_WARN_IF(NS_FAILED(PSocketProcessBridge::CreateEndpoints(
          parent->OtherPid(), Manager()->OtherPid(), &parentEndpoint,
          &childEndpoint)))) {
    aResolver(std::move(invalidEndpoint));
    return IPC_OK();
  }

  if (NS_WARN_IF(!parent->SendInitSocketProcessBridgeParent(
          Manager()->OtherPid(), std::move(parentEndpoint)))) {
    aResolver(std::move(invalidEndpoint));
    return IPC_OK();
  }

  aResolver(std::move(childEndpoint));
  mSocketProcessBridgeInited = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoParent::RecvEnsureHSTSData(
    EnsureHSTSDataResolver&& aResolver) {
  auto callback = [aResolver{std::move(aResolver)}](bool aResult) {
    aResolver(aResult);
  };
  RefPtr<HSTSDataCallbackWrapper> wrapper =
      new HSTSDataCallbackWrapper(std::move(callback));
  gHttpHandler->EnsureHSTSDataReadyNative(wrapper);
  return IPC_OK();
}

PProxyConfigLookupParent* NeckoParent::AllocPProxyConfigLookupParent() {
#ifdef MOZ_WEBRTC
  RefPtr<ProxyConfigLookupParent> actor = new ProxyConfigLookupParent();
  return actor.forget().take();
#else
  return nullptr;
#endif
}

mozilla::ipc::IPCResult NeckoParent::RecvPProxyConfigLookupConstructor(
    PProxyConfigLookupParent* aActor) {
#ifdef MOZ_WEBRTC
  ProxyConfigLookupParent* actor =
      static_cast<ProxyConfigLookupParent*>(aActor);
  actor->DoProxyLookup();
#endif
  return IPC_OK();
}

bool NeckoParent::DeallocPProxyConfigLookupParent(
    PProxyConfigLookupParent* aActor) {
#ifdef MOZ_WEBRTC
  RefPtr<ProxyConfigLookupParent> actor =
      dont_AddRef(static_cast<ProxyConfigLookupParent*>(aActor));
  MOZ_ASSERT(actor);
#endif
  return true;
}

}  // namespace net
}  // namespace mozilla
