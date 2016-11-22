/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "necko-config.h"
#include "nsHttp.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/WyciwygChannelParent.h"
#include "mozilla/net/FTPChannelParent.h"
#include "mozilla/net/WebSocketChannelParent.h"
#include "mozilla/net/WebSocketEventListenerParent.h"
#include "mozilla/net/DataChannelParent.h"
#include "mozilla/net/AltDataOutputStreamParent.h"
#include "mozilla/Unused.h"
#ifdef NECKO_PROTOCOL_rtsp
#include "mozilla/net/RtspControllerParent.h"
#include "mozilla/net/RtspChannelParent.h"
#endif
#include "mozilla/net/DNSRequestParent.h"
#include "mozilla/net/ChannelDiverterParent.h"
#include "mozilla/net/IPCTransportProvider.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/network/TCPSocketParent.h"
#include "mozilla/dom/network/TCPServerSocketParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"
#include "mozilla/LoadContext.h"
#include "nsPrintfCString.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsEscape.h"
#include "SerializedLoadContext.h"
#include "nsAuthInformationHolder.h"
#include "nsIAuthPromptCallback.h"
#include "nsPrincipal.h"
#include "nsINetworkPredictor.h"
#include "nsINetworkPredictorVerifier.h"
#include "nsISpeculativeConnect.h"

using mozilla::DocShellOriginAttributes;
using mozilla::NeckoOriginAttributes;
using mozilla::dom::ChromeUtils;
using mozilla::dom::ContentParent;
using mozilla::dom::TabContext;
using mozilla::dom::TabParent;
using mozilla::net::PTCPSocketParent;
using mozilla::dom::TCPSocketParent;
using mozilla::net::PTCPServerSocketParent;
using mozilla::dom::TCPServerSocketParent;
using mozilla::net::PUDPSocketParent;
using mozilla::dom::UDPSocketParent;
using mozilla::dom::workers::ServiceWorkerManager;
using mozilla::ipc::OptionalPrincipalInfo;
using mozilla::ipc::PrincipalInfo;
using IPC::SerializedLoadContext;

namespace mozilla {
namespace net {

// C++ file contents
NeckoParent::NeckoParent()
{
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

NeckoParent::~NeckoParent()
{
}

static PBOverrideStatus
PBOverrideStatusFromLoadContext(const SerializedLoadContext& aSerialized)
{
  if (!aSerialized.IsNotNull() && aSerialized.IsPrivateBitValid()) {
    return (aSerialized.mOriginAttributes.mPrivateBrowsingId > 0) ?
      kPBOverride_Private :
      kPBOverride_NotPrivate;
  }
  return kPBOverride_Unset;
}

static already_AddRefed<nsIPrincipal>
GetRequestingPrincipal(const OptionalLoadInfoArgs aOptionalLoadInfoArgs)
{
  if (aOptionalLoadInfoArgs.type() != OptionalLoadInfoArgs::TLoadInfoArgs) {
    return nullptr;
  }

  const LoadInfoArgs& loadInfoArgs = aOptionalLoadInfoArgs.get_LoadInfoArgs();
  const OptionalPrincipalInfo& optionalPrincipalInfo =
    loadInfoArgs.requestingPrincipalInfo();

  if (optionalPrincipalInfo.type() != OptionalPrincipalInfo::TPrincipalInfo) {
    return nullptr;
  }

  const PrincipalInfo& principalInfo =
    optionalPrincipalInfo.get_PrincipalInfo();

  return PrincipalInfoToPrincipal(principalInfo);
}

static already_AddRefed<nsIPrincipal>
GetRequestingPrincipal(const HttpChannelCreationArgs& aArgs)
{
  if (aArgs.type() != HttpChannelCreationArgs::THttpChannelOpenArgs) {
    return nullptr;
  }

  const HttpChannelOpenArgs& args = aArgs.get_HttpChannelOpenArgs();
  return GetRequestingPrincipal(args.loadInfo());
}

static already_AddRefed<nsIPrincipal>
GetRequestingPrincipal(const FTPChannelCreationArgs& aArgs)
{
  if (aArgs.type() != FTPChannelCreationArgs::TFTPChannelOpenArgs) {
    return nullptr;
  }

  const FTPChannelOpenArgs& args = aArgs.get_FTPChannelOpenArgs();
  return GetRequestingPrincipal(args.loadInfo());
}

// Bug 1289001 - If GetValidatedOriginAttributes returns an error string, that
// usually leads to a content crash with very little info about the cause.
// We prefer to crash on the parent, so we get the reason in the crash report.
static MOZ_COLD
void CrashWithReason(const char * reason)
{
#ifndef RELEASE_OR_BETA
  MOZ_CRASH_ANNOTATE(reason);
  MOZ_REALLY_CRASH();
#endif
}

const char*
NeckoParent::GetValidatedOriginAttributes(const SerializedLoadContext& aSerialized,
                                          PContentParent* aContent,
                                          nsIPrincipal* aRequestingPrincipal,
                                          DocShellOriginAttributes& aAttrs)
{
  if (!aSerialized.IsNotNull()) {
    if (UsingNeckoIPCSecurity()) {
      CrashWithReason("GetValidatedOriginAttributes | SerializedLoadContext from child is null");
      return "SerializedLoadContext from child is null";
    }

    // If serialized is null, we cannot validate anything. We have to assume
    // that this requests comes from a SystemPrincipal.
    aAttrs = DocShellOriginAttributes(NECKO_NO_APP_ID, false);
    return nullptr;
  }

  nsTArray<TabContext> contextArray =
    static_cast<ContentParent*>(aContent)->GetManagedTabContext();

  nsAutoCString serializedSuffix;
  aSerialized.mOriginAttributes.CreateAnonymizedSuffix(serializedSuffix);

  nsAutoCString debugString;
  for (uint32_t i = 0; i < contextArray.Length(); i++) {
    const TabContext& tabContext = contextArray[i];

    if (!ChromeUtils::IsOriginAttributesEqual(aSerialized.mOriginAttributes,
                                              tabContext.OriginAttributesRef())) {
      debugString.Append("(");
      debugString.Append(serializedSuffix);
      debugString.Append(",");

      nsAutoCString tabSuffix;
      tabContext.OriginAttributesRef().CreateAnonymizedSuffix(tabSuffix);
      debugString.Append(tabSuffix);

      debugString.Append(")");
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
        swm->MayHaveActiveServiceWorkerInstance(static_cast<ContentParent*>(aContent),
                                                aRequestingPrincipal)) {
      aAttrs = aSerialized.mOriginAttributes;
      return nullptr;
    }
  }

  if (!UsingNeckoIPCSecurity()) {
    // We are running some tests
    aAttrs = aSerialized.mOriginAttributes;
    return nullptr;
  }

  nsAutoCString errorString;
  errorString.Append("GetValidatedOriginAttributes | App does not have permission -");
  errorString.Append(debugString);

  // Leak the buffer on the heap to make sure that it lives long enough, as
  // MOZ_CRASH_ANNOTATE expects the pointer passed to it to live to the end of
  // the program.
  char * error = strdup(errorString.BeginReading());
  CrashWithReason(error);
  return "App does not have permission";
}

const char *
NeckoParent::CreateChannelLoadContext(const PBrowserOrId& aBrowser,
                                      PContentParent* aContent,
                                      const SerializedLoadContext& aSerialized,
                                      nsIPrincipal* aRequestingPrincipal,
                                      nsCOMPtr<nsILoadContext> &aResult)
{
  DocShellOriginAttributes attrs;
  const char* error = GetValidatedOriginAttributes(aSerialized, aContent,
                                                   aRequestingPrincipal, attrs);
  if (error) {
    return error;
  }

  // if !UsingNeckoIPCSecurity(), we may not have a LoadContext to set. This is
  // the common case for most xpcshell tests.
  if (aSerialized.IsNotNull()) {
    attrs.SyncAttributesWithPrivateBrowsing(aSerialized.mOriginAttributes.mPrivateBrowsingId > 0);
    switch (aBrowser.type()) {
      case PBrowserOrId::TPBrowserParent:
      {
        RefPtr<TabParent> tabParent =
          TabParent::GetFrom(aBrowser.get_PBrowserParent());
        dom::Element* topFrameElement = nullptr;
        if (tabParent) {
          topFrameElement = tabParent->GetOwnerElement();
        }
        aResult = new LoadContext(aSerialized, topFrameElement, attrs);
        break;
      }
      case PBrowserOrId::TTabId:
      {
        aResult = new LoadContext(aSerialized, aBrowser.get_TabId(), attrs);
        break;
      }
      default:
        MOZ_CRASH();
    }
  }

  return nullptr;
}

void
NeckoParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Nothing needed here. Called right before destructor since this is a
  // non-refcounted class.
}

PHttpChannelParent*
NeckoParent::AllocPHttpChannelParent(const PBrowserOrId& aBrowser,
                                     const SerializedLoadContext& aSerialized,
                                     const HttpChannelCreationArgs& aOpenArgs)
{
  nsCOMPtr<nsIPrincipal> requestingPrincipal =
    GetRequestingPrincipal(aOpenArgs);

  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(aBrowser, Manager(),
                                               aSerialized, requestingPrincipal,
                                               loadContext);
  if (error) {
    printf_stderr("NeckoParent::AllocPHttpChannelParent: "
                  "FATAL error: %s: KILLING CHILD PROCESS\n",
                  error);
    return nullptr;
  }
  PBOverrideStatus overrideStatus = PBOverrideStatusFromLoadContext(aSerialized);
  HttpChannelParent *p = new HttpChannelParent(aBrowser, loadContext, overrideStatus);
  p->AddRef();
  return p;
}

bool
NeckoParent::DeallocPHttpChannelParent(PHttpChannelParent* channel)
{
  HttpChannelParent *p = static_cast<HttpChannelParent *>(channel);
  p->Release();
  return true;
}

mozilla::ipc::IPCResult
NeckoParent::RecvPHttpChannelConstructor(
                      PHttpChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const HttpChannelCreationArgs& aOpenArgs)
{
  HttpChannelParent* p = static_cast<HttpChannelParent*>(aActor);
  if (!p->Init(aOpenArgs)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PAltDataOutputStreamParent*
NeckoParent::AllocPAltDataOutputStreamParent(
        const nsCString& type,
        PHttpChannelParent* channel)
{
  HttpChannelParent* chan = static_cast<HttpChannelParent*>(channel);
  nsCOMPtr<nsIOutputStream> stream;
  nsresult rv = chan->OpenAlternativeOutputStream(type, getter_AddRefs(stream));
  AltDataOutputStreamParent* parent = new AltDataOutputStreamParent(stream);
  parent->AddRef();
  // If the return value was not NS_OK, the error code will be sent
  // asynchronously to the child, after receiving the first message.
  parent->SetError(rv);
  return parent;
}

bool
NeckoParent::DeallocPAltDataOutputStreamParent(PAltDataOutputStreamParent* aActor)
{
  AltDataOutputStreamParent* parent = static_cast<AltDataOutputStreamParent*>(aActor);
  parent->Release();
  return true;
}

PFTPChannelParent*
NeckoParent::AllocPFTPChannelParent(const PBrowserOrId& aBrowser,
                                    const SerializedLoadContext& aSerialized,
                                    const FTPChannelCreationArgs& aOpenArgs)
{
  nsCOMPtr<nsIPrincipal> requestingPrincipal =
    GetRequestingPrincipal(aOpenArgs);

  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(aBrowser, Manager(),
                                               aSerialized, requestingPrincipal,
                                               loadContext);
  if (error) {
    printf_stderr("NeckoParent::AllocPFTPChannelParent: "
                  "FATAL error: %s: KILLING CHILD PROCESS\n",
                  error);
    return nullptr;
  }
  PBOverrideStatus overrideStatus = PBOverrideStatusFromLoadContext(aSerialized);
  FTPChannelParent *p = new FTPChannelParent(aBrowser, loadContext, overrideStatus);
  p->AddRef();
  return p;
}

bool
NeckoParent::DeallocPFTPChannelParent(PFTPChannelParent* channel)
{
  FTPChannelParent *p = static_cast<FTPChannelParent *>(channel);
  p->Release();
  return true;
}

mozilla::ipc::IPCResult
NeckoParent::RecvPFTPChannelConstructor(
                      PFTPChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const FTPChannelCreationArgs& aOpenArgs)
{
  FTPChannelParent* p = static_cast<FTPChannelParent*>(aActor);
  if (!p->Init(aOpenArgs)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PCookieServiceParent*
NeckoParent::AllocPCookieServiceParent()
{
  return new CookieServiceParent();
}

bool
NeckoParent::DeallocPCookieServiceParent(PCookieServiceParent* cs)
{
  delete cs;
  return true;
}

PWyciwygChannelParent*
NeckoParent::AllocPWyciwygChannelParent()
{
  WyciwygChannelParent *p = new WyciwygChannelParent();
  p->AddRef();
  return p;
}

bool
NeckoParent::DeallocPWyciwygChannelParent(PWyciwygChannelParent* channel)
{
  WyciwygChannelParent *p = static_cast<WyciwygChannelParent *>(channel);
  p->Release();
  return true;
}

PWebSocketParent*
NeckoParent::AllocPWebSocketParent(const PBrowserOrId& browser,
                                   const SerializedLoadContext& serialized,
                                   const uint32_t& aSerial)
{
  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(browser, Manager(),
                                               serialized,
                                               nullptr,
                                               loadContext);
  if (error) {
    printf_stderr("NeckoParent::AllocPWebSocketParent: "
                  "FATAL error: %s: KILLING CHILD PROCESS\n",
                  error);
    return nullptr;
  }

  RefPtr<TabParent> tabParent = TabParent::GetFrom(browser.get_PBrowserParent());
  PBOverrideStatus overrideStatus = PBOverrideStatusFromLoadContext(serialized);
  WebSocketChannelParent* p = new WebSocketChannelParent(tabParent, loadContext,
                                                         overrideStatus,
                                                         aSerial);
  p->AddRef();
  return p;
}

bool
NeckoParent::DeallocPWebSocketParent(PWebSocketParent* actor)
{
  WebSocketChannelParent* p = static_cast<WebSocketChannelParent*>(actor);
  p->Release();
  return true;
}

PWebSocketEventListenerParent*
NeckoParent::AllocPWebSocketEventListenerParent(const uint64_t& aInnerWindowID)
{
  RefPtr<WebSocketEventListenerParent> c =
    new WebSocketEventListenerParent(aInnerWindowID);
  return c.forget().take();
}

bool
NeckoParent::DeallocPWebSocketEventListenerParent(PWebSocketEventListenerParent* aActor)
{
  RefPtr<WebSocketEventListenerParent> c =
    dont_AddRef(static_cast<WebSocketEventListenerParent*>(aActor));
  MOZ_ASSERT(c);
  return true;
}

PDataChannelParent*
NeckoParent::AllocPDataChannelParent(const uint32_t &channelId)
{
  RefPtr<DataChannelParent> p = new DataChannelParent();
  return p.forget().take();
}

bool
NeckoParent::DeallocPDataChannelParent(PDataChannelParent* actor)
{
  RefPtr<DataChannelParent> p = dont_AddRef(static_cast<DataChannelParent*>(actor));
  return true;
}

mozilla::ipc::IPCResult
NeckoParent::RecvPDataChannelConstructor(PDataChannelParent* actor,
                                         const uint32_t& channelId)
{
  DataChannelParent* p = static_cast<DataChannelParent*>(actor);
  MOZ_DIAGNOSTIC_ASSERT(p->Init(channelId));
  return IPC_OK();
}

PRtspControllerParent*
NeckoParent::AllocPRtspControllerParent()
{
#ifdef NECKO_PROTOCOL_rtsp
  RtspControllerParent* p = new RtspControllerParent();
  p->AddRef();
  return p;
#else
  return nullptr;
#endif
}

bool
NeckoParent::DeallocPRtspControllerParent(PRtspControllerParent* actor)
{
#ifdef NECKO_PROTOCOL_rtsp
  RtspControllerParent* p = static_cast<RtspControllerParent*>(actor);
  p->Release();
#endif
  return true;
}

PRtspChannelParent*
NeckoParent::AllocPRtspChannelParent(const RtspChannelConnectArgs& aArgs)
{
#ifdef NECKO_PROTOCOL_rtsp
  nsCOMPtr<nsIURI> uri = DeserializeURI(aArgs.uri());
  RtspChannelParent *p = new RtspChannelParent(uri);
  p->AddRef();
  return p;
#else
  return nullptr;
#endif
}

mozilla::ipc::IPCResult
NeckoParent::RecvPRtspChannelConstructor(
                      PRtspChannelParent* aActor,
                      const RtspChannelConnectArgs& aConnectArgs)
{
#ifdef NECKO_PROTOCOL_rtsp
  RtspChannelParent* p = static_cast<RtspChannelParent*>(aActor);
  return p->Init(aConnectArgs);
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

bool
NeckoParent::DeallocPRtspChannelParent(PRtspChannelParent* actor)
{
#ifdef NECKO_PROTOCOL_rtsp
  RtspChannelParent* p = static_cast<RtspChannelParent*>(actor);
  p->Release();
#endif
  return true;
}

PTCPSocketParent*
NeckoParent::AllocPTCPSocketParent(const nsString& /* host */,
                                   const uint16_t& /* port */)
{
  // We actually don't need host/port to construct a TCPSocketParent since
  // TCPSocketParent will maintain an internal nsIDOMTCPSocket instance which
  // can be delegated to get the host/port.
  TCPSocketParent* p = new TCPSocketParent();
  p->AddIPDLReference();
  return p;
}

bool
NeckoParent::DeallocPTCPSocketParent(PTCPSocketParent* actor)
{
  TCPSocketParent* p = static_cast<TCPSocketParent*>(actor);
  p->ReleaseIPDLReference();
  return true;
}

PTCPServerSocketParent*
NeckoParent::AllocPTCPServerSocketParent(const uint16_t& aLocalPort,
                                         const uint16_t& aBacklog,
                                         const bool& aUseArrayBuffers)
{
  TCPServerSocketParent* p = new TCPServerSocketParent(this, aLocalPort, aBacklog, aUseArrayBuffers);
  p->AddIPDLReference();
  return p;
}

mozilla::ipc::IPCResult
NeckoParent::RecvPTCPServerSocketConstructor(PTCPServerSocketParent* aActor,
                                             const uint16_t& aLocalPort,
                                             const uint16_t& aBacklog,
                                             const bool& aUseArrayBuffers)
{
  static_cast<TCPServerSocketParent*>(aActor)->Init();
  return IPC_OK();
}

bool
NeckoParent::DeallocPTCPServerSocketParent(PTCPServerSocketParent* actor)
{
  TCPServerSocketParent* p = static_cast<TCPServerSocketParent*>(actor);
   p->ReleaseIPDLReference();
  return true;
}

PUDPSocketParent*
NeckoParent::AllocPUDPSocketParent(const Principal& /* unused */,
                                   const nsCString& /* unused */)
{
  RefPtr<UDPSocketParent> p = new UDPSocketParent(this);

  return p.forget().take();
}

mozilla::ipc::IPCResult
NeckoParent::RecvPUDPSocketConstructor(PUDPSocketParent* aActor,
                                       const Principal& aPrincipal,
                                       const nsCString& aFilter)
{
  if (!static_cast<UDPSocketParent*>(aActor)->Init(aPrincipal, aFilter)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool
NeckoParent::DeallocPUDPSocketParent(PUDPSocketParent* actor)
{
  UDPSocketParent* p = static_cast<UDPSocketParent*>(actor);
  p->Release();
  return true;
}

PDNSRequestParent*
NeckoParent::AllocPDNSRequestParent(const nsCString& aHost,
                                    const uint32_t& aFlags,
                                    const nsCString& aNetworkInterface)
{
  DNSRequestParent *p = new DNSRequestParent();
  p->AddRef();
  return p;
}

mozilla::ipc::IPCResult
NeckoParent::RecvPDNSRequestConstructor(PDNSRequestParent* aActor,
                                        const nsCString& aHost,
                                        const uint32_t& aFlags,
                                        const nsCString& aNetworkInterface)
{
  static_cast<DNSRequestParent*>(aActor)->DoAsyncResolve(aHost, aFlags,
                                                         aNetworkInterface);
  return IPC_OK();
}

bool
NeckoParent::DeallocPDNSRequestParent(PDNSRequestParent* aParent)
{
  DNSRequestParent *p = static_cast<DNSRequestParent*>(aParent);
  p->Release();
  return true;
}

mozilla::ipc::IPCResult
NeckoParent::RecvSpeculativeConnect(const URIParams& aURI,
                                    const Principal& aPrincipal,
                                    const bool& aAnonymous)
{
  nsCOMPtr<nsISpeculativeConnect> speculator(gIOService);
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  if (uri && speculator) {
    if (aAnonymous) {
      speculator->SpeculativeAnonymousConnect2(uri, principal, nullptr);
    } else {
      speculator->SpeculativeConnect2(uri, principal, nullptr);
    }

  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoParent::RecvHTMLDNSPrefetch(const nsString& hostname,
                                 const uint16_t& flags)
{
  nsHTMLDNSPrefetch::Prefetch(hostname, flags);
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoParent::RecvCancelHTMLDNSPrefetch(const nsString& hostname,
                                 const uint16_t& flags,
                                 const nsresult& reason)
{
  nsHTMLDNSPrefetch::CancelPrefetch(hostname, flags, reason);
  return IPC_OK();
}

PChannelDiverterParent*
NeckoParent::AllocPChannelDiverterParent(const ChannelDiverterArgs& channel)
{
  return new ChannelDiverterParent();
}

mozilla::ipc::IPCResult
NeckoParent::RecvPChannelDiverterConstructor(PChannelDiverterParent* actor,
                                             const ChannelDiverterArgs& channel)
{
  auto parent = static_cast<ChannelDiverterParent*>(actor);
  parent->Init(channel);
  return IPC_OK();
}

bool
NeckoParent::DeallocPChannelDiverterParent(PChannelDiverterParent* parent)
{
  delete static_cast<ChannelDiverterParent*>(parent);
  return true;
}

PTransportProviderParent*
NeckoParent::AllocPTransportProviderParent()
{
  RefPtr<TransportProviderParent> res = new TransportProviderParent();
  return res.forget().take();
}

bool
NeckoParent::DeallocPTransportProviderParent(PTransportProviderParent* aActor)
{
  RefPtr<TransportProviderParent> provider =
    dont_AddRef(static_cast<TransportProviderParent*>(aActor));
  return true;
}

namespace {
std::map<uint64_t, nsCOMPtr<nsIAuthPromptCallback> >&
CallbackMap()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<uint64_t, nsCOMPtr<nsIAuthPromptCallback> > sCallbackMap;
  return sCallbackMap;
}
} // namespace

NS_IMPL_ISUPPORTS(NeckoParent::NestedFrameAuthPrompt, nsIAuthPrompt2)

NeckoParent::NestedFrameAuthPrompt::NestedFrameAuthPrompt(PNeckoParent* aParent,
                                                          TabId aNestedFrameId)
  : mNeckoParent(aParent)
  , mNestedFrameId(aNestedFrameId)
{}

NS_IMETHODIMP
NeckoParent::NestedFrameAuthPrompt::AsyncPromptAuth(
  nsIChannel* aChannel, nsIAuthPromptCallback* callback,
  nsISupports*, uint32_t,
  nsIAuthInformation* aInfo, nsICancelable**)
{
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
  if (mNeckoParent->SendAsyncAuthPromptForNestedFrame(mNestedFrameId,
                                                      spec,
                                                      realm,
                                                      callbackId)) {
    CallbackMap()[callbackId] = callback;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

mozilla::ipc::IPCResult
NeckoParent::RecvOnAuthAvailable(const uint64_t& aCallbackId,
                                 const nsString& aUser,
                                 const nsString& aPassword,
                                 const nsString& aDomain)
{
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

mozilla::ipc::IPCResult
NeckoParent::RecvOnAuthCancelled(const uint64_t& aCallbackId,
                                 const bool& aUserCancel)
{
  nsCOMPtr<nsIAuthPromptCallback> callback = CallbackMap()[aCallbackId];
  if (!callback) {
    return IPC_OK();
  }
  CallbackMap().erase(aCallbackId);
  callback->OnAuthCancelled(nullptr, aUserCancel);
  return IPC_OK();
}

/* Predictor Messages */
mozilla::ipc::IPCResult
NeckoParent::RecvPredPredict(const ipc::OptionalURIParams& aTargetURI,
                             const ipc::OptionalURIParams& aSourceURI,
                             const uint32_t& aReason,
                             const SerializedLoadContext& aLoadContext,
                             const bool& hasVerifier)
{
  nsCOMPtr<nsIURI> targetURI = DeserializeURI(aTargetURI);
  nsCOMPtr<nsIURI> sourceURI = DeserializeURI(aSourceURI);

  // We only actually care about the loadContext.mPrivateBrowsing, so we'll just
  // pass dummy params for nestFrameId, and originAttributes.
  uint64_t nestedFrameId = 0;
  DocShellOriginAttributes attrs(NECKO_UNKNOWN_APP_ID, false);
  nsCOMPtr<nsILoadContext> loadContext;
  if (aLoadContext.IsNotNull()) {
    attrs.SyncAttributesWithPrivateBrowsing(aLoadContext.mOriginAttributes.mPrivateBrowsingId > 0);
    loadContext = new LoadContext(aLoadContext, nestedFrameId, attrs);
  }

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsINetworkPredictorVerifier> verifier;
  if (hasVerifier) {
    verifier = do_QueryInterface(predictor);
  }
  predictor->Predict(targetURI, sourceURI, aReason, loadContext, verifier);
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoParent::RecvPredLearn(const ipc::URIParams& aTargetURI,
                           const ipc::OptionalURIParams& aSourceURI,
                           const uint32_t& aReason,
                           const SerializedLoadContext& aLoadContext)
{
  nsCOMPtr<nsIURI> targetURI = DeserializeURI(aTargetURI);
  nsCOMPtr<nsIURI> sourceURI = DeserializeURI(aSourceURI);

  // We only actually care about the loadContext.mPrivateBrowsing, so we'll just
  // pass dummy params for nestFrameId, and originAttributes;
  uint64_t nestedFrameId = 0;
  DocShellOriginAttributes attrs(NECKO_UNKNOWN_APP_ID, false);
  nsCOMPtr<nsILoadContext> loadContext;
  if (aLoadContext.IsNotNull()) {
    attrs.SyncAttributesWithPrivateBrowsing(aLoadContext.mOriginAttributes.mPrivateBrowsingId > 0);
    loadContext = new LoadContext(aLoadContext, nestedFrameId, attrs);
  }

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->Learn(targetURI, sourceURI, aReason, loadContext);
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoParent::RecvPredReset()
{
  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->Reset();
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoParent::RecvRemoveRequestContext(const nsCString& rcid)
{
  nsCOMPtr<nsIRequestContextService> rcsvc =
    do_GetService("@mozilla.org/network/request-context-service;1");
  if (!rcsvc) {
    return IPC_OK();
  }

  nsID id;
  id.Parse(rcid.BeginReading());
  rcsvc->RemoveRequestContext(id);

  return IPC_OK();
}

} // namespace net
} // namespace mozilla
