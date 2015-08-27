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
#include "mozilla/net/DataChannelParent.h"
#ifdef NECKO_PROTOCOL_rtsp
#include "mozilla/net/RtspControllerParent.h"
#include "mozilla/net/RtspChannelParent.h"
#endif
#include "mozilla/net/DNSRequestParent.h"
#include "mozilla/net/RemoteOpenFileParent.h"
#include "mozilla/net/ChannelDiverterParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/network/TCPSocketParent.h"
#include "mozilla/dom/network/TCPServerSocketParent.h"
#include "mozilla/dom/network/UDPSocketParent.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/LoadContext.h"
#include "mozilla/AppProcessChecker.h"
#include "nsPrintfCString.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsIAppsService.h"
#include "nsEscape.h"
#include "RemoteOpenFileParent.h"
#include "SerializedLoadContext.h"
#include "nsAuthInformationHolder.h"
#include "nsIAuthPromptCallback.h"
#include "nsPrincipal.h"
#include "nsIOService.h"
#include "nsINetworkPredictor.h"
#include "mozilla/net/OfflineObserver.h"
#include "nsISpeculativeConnect.h"

using mozilla::dom::ContentParent;
using mozilla::dom::TabContext;
using mozilla::dom::TabParent;
using mozilla::net::PTCPSocketParent;
using mozilla::dom::TCPSocketParent;
using mozilla::net::PTCPServerSocketParent;
using mozilla::dom::TCPServerSocketParent;
using mozilla::net::PUDPSocketParent;
using mozilla::dom::UDPSocketParent;
using IPC::SerializedLoadContext;

namespace mozilla {
namespace net {

PNeckoParent *gNeckoParent = nullptr;

// C++ file contents
NeckoParent::NeckoParent()
{
  // Init HTTP protocol handler now since we need atomTable up and running very
  // early (IPDL argument handling for PHttpChannel constructor needs it) so
  // normal init (during 1st Http channel request) isn't early enough.
  nsCOMPtr<nsIProtocolHandler> proto =
    do_GetService("@mozilla.org/network/protocol;1?name=http");

  if (UsingNeckoIPCSecurity()) {
    // cache values for core/packaged apps basepaths
    nsAutoString corePath, webPath;
    nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
    if (appsService) {
      appsService->GetCoreAppsBasePath(corePath);
      appsService->GetWebAppsBasePath(webPath);
    }
    // corePath may be empty: we don't use it for all build types
    MOZ_ASSERT(!webPath.IsEmpty());

    LossyCopyUTF16toASCII(corePath, mCoreAppsBasePath);
    LossyCopyUTF16toASCII(webPath, mWebAppsBasePath);
  }

  mObserver = new OfflineObserver(this);
  gNeckoParent = this;
}

NeckoParent::~NeckoParent()
{
  if (mObserver) {
    mObserver->RemoveObserver();
  }
}

static PBOverrideStatus
PBOverrideStatusFromLoadContext(const SerializedLoadContext& aSerialized)
{
  if (!aSerialized.IsNotNull() && aSerialized.IsPrivateBitValid()) {
    return aSerialized.mUsePrivateBrowsing ?
      kPBOverride_Private :
      kPBOverride_NotPrivate;
  }
  return kPBOverride_Unset;
}

const char*
NeckoParent::GetValidatedAppInfo(const SerializedLoadContext& aSerialized,
                                 PContentParent* aContent,
                                 uint32_t* aAppId,
                                 bool* aInBrowserElement)
{
  *aAppId = NECKO_UNKNOWN_APP_ID;
  *aInBrowserElement = false;

  if (UsingNeckoIPCSecurity()) {
    if (!aSerialized.IsNotNull()) {
      return "SerializedLoadContext from child is null";
    }
  }

  nsTArray<TabContext> contextArray =
    static_cast<ContentParent*>(aContent)->GetManagedTabContext();
  for (uint32_t i = 0; i < contextArray.Length(); i++) {
    TabContext tabContext = contextArray[i];
    uint32_t appId = tabContext.OwnOrContainingAppId();
    bool inBrowserElement = aSerialized.IsNotNull() ? aSerialized.mIsInBrowserElement
                                                    : tabContext.IsBrowserElement();

    if (appId == NECKO_UNKNOWN_APP_ID) {
      continue;
    }
    // We may get appID=NO_APP if child frame is neither a browser nor an app
    if (appId == NECKO_NO_APP_ID) {
      if (tabContext.HasOwnApp()) {
        continue;
      }
      if (UsingNeckoIPCSecurity() && tabContext.IsBrowserElement()) {
        // <iframe mozbrowser> which doesn't have an <iframe mozapp> above it.
        // This is not supported now, and we'll need to do a code audit to make
        // sure we can handle it (i.e don't short-circuit using separate
        // namespace if just appID==0)
        continue;
      }
    }
    *aAppId = appId;
    *aInBrowserElement = inBrowserElement;
    return nullptr;
  }

  if (contextArray.Length() != 0) {
    return "App does not have permission";
  }

  if (!UsingNeckoIPCSecurity()) {
    // We are running xpcshell tests
    if (aSerialized.IsNotNull()) {
      *aAppId = aSerialized.mAppId;
      *aInBrowserElement = aSerialized.mIsInBrowserElement;
    } else {
      *aAppId = NECKO_NO_APP_ID;
    }
    return nullptr;
  }

  return "ContentParent does not have any PBrowsers";
}

const char *
NeckoParent::CreateChannelLoadContext(const PBrowserOrId& aBrowser,
                                      PContentParent* aContent,
                                      const SerializedLoadContext& aSerialized,
                                      nsCOMPtr<nsILoadContext> &aResult)
{
  uint32_t appId = NECKO_UNKNOWN_APP_ID;
  bool inBrowser = false;
  const char* error = GetValidatedAppInfo(aSerialized, aContent, &appId, &inBrowser);
  if (error) {
    return error;
  }

  // if !UsingNeckoIPCSecurity(), we may not have a LoadContext to set. This is
  // the common case for most xpcshell tests.
  if (aSerialized.IsNotNull()) {
    switch (aBrowser.type()) {
      case PBrowserOrId::TPBrowserParent:
      {
        nsRefPtr<TabParent> tabParent =
          TabParent::GetFrom(aBrowser.get_PBrowserParent());
        dom::Element* topFrameElement = nullptr;
        if (tabParent) {
          topFrameElement = tabParent->GetOwnerElement();
        }
        aResult = new LoadContext(aSerialized, topFrameElement,
                                  appId, inBrowser);
        break;
      }
      case PBrowserOrId::TTabId:
      {
        aResult = new LoadContext(aSerialized, aBrowser.get_TabId(),
                                  appId, inBrowser);
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
  // Implement me! Bug 1005184
}

PHttpChannelParent*
NeckoParent::AllocPHttpChannelParent(const PBrowserOrId& aBrowser,
                                     const SerializedLoadContext& aSerialized,
                                     const HttpChannelCreationArgs& aOpenArgs)
{
  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(aBrowser, Manager(),
                                               aSerialized, loadContext);
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

bool
NeckoParent::RecvPHttpChannelConstructor(
                      PHttpChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const HttpChannelCreationArgs& aOpenArgs)
{
  HttpChannelParent* p = static_cast<HttpChannelParent*>(aActor);
  return p->Init(aOpenArgs);
}

PFTPChannelParent*
NeckoParent::AllocPFTPChannelParent(const PBrowserOrId& aBrowser,
                                    const SerializedLoadContext& aSerialized,
                                    const FTPChannelCreationArgs& aOpenArgs)
{
  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(aBrowser, Manager(),
                                               aSerialized, loadContext);
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

bool
NeckoParent::RecvPFTPChannelConstructor(
                      PFTPChannelParent* aActor,
                      const PBrowserOrId& aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const FTPChannelCreationArgs& aOpenArgs)
{
  FTPChannelParent* p = static_cast<FTPChannelParent*>(aActor);
  return p->Init(aOpenArgs);
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
                                   const SerializedLoadContext& serialized)
{
  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(browser, Manager(),
                                               serialized, loadContext);
  if (error) {
    printf_stderr("NeckoParent::AllocPWebSocketParent: "
                  "FATAL error: %s: KILLING CHILD PROCESS\n",
                  error);
    return nullptr;
  }

  nsRefPtr<TabParent> tabParent = TabParent::GetFrom(browser.get_PBrowserParent());
  PBOverrideStatus overrideStatus = PBOverrideStatusFromLoadContext(serialized);
  WebSocketChannelParent* p = new WebSocketChannelParent(tabParent, loadContext,
                                                         overrideStatus);
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

PDataChannelParent*
NeckoParent::AllocPDataChannelParent(const uint32_t &channelId)
{
  nsRefPtr<DataChannelParent> p = new DataChannelParent();
  return p.forget().take();
}

bool
NeckoParent::DeallocPDataChannelParent(PDataChannelParent* actor)
{
  nsRefPtr<DataChannelParent> p = dont_AddRef(static_cast<DataChannelParent*>(actor));
  return true;
}

bool
NeckoParent::RecvPDataChannelConstructor(PDataChannelParent* actor,
                                         const uint32_t& channelId)
{
  DataChannelParent* p = static_cast<DataChannelParent*>(actor);
  p->Init(channelId);
  return true;
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

bool
NeckoParent::RecvPRtspChannelConstructor(
                      PRtspChannelParent* aActor,
                      const RtspChannelConnectArgs& aConnectArgs)
{
#ifdef NECKO_PROTOCOL_rtsp
  RtspChannelParent* p = static_cast<RtspChannelParent*>(aActor);
  return p->Init(aConnectArgs);
#else
  return false;
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
                                   const nsString& aBinaryType)
{
  TCPServerSocketParent* p = new TCPServerSocketParent();
  p->AddIPDLReference();
  return p;
}

bool
NeckoParent::RecvPTCPServerSocketConstructor(PTCPServerSocketParent* aActor,
                                             const uint16_t& aLocalPort,
                                             const uint16_t& aBacklog,
                                             const nsString& aBinaryType)
{
  return static_cast<TCPServerSocketParent*>(aActor)->
      Init(this, aLocalPort, aBacklog, aBinaryType);
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
  nsRefPtr<UDPSocketParent> p = new UDPSocketParent(this);

  return p.forget().take();
}

bool
NeckoParent::RecvPUDPSocketConstructor(PUDPSocketParent* aActor,
                                       const Principal& aPrincipal,
                                       const nsCString& aFilter)
{
  return static_cast<UDPSocketParent*>(aActor)->Init(aPrincipal, aFilter);
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

bool
NeckoParent::RecvPDNSRequestConstructor(PDNSRequestParent* aActor,
                                        const nsCString& aHost,
                                        const uint32_t& aFlags,
                                        const nsCString& aNetworkInterface)
{
  static_cast<DNSRequestParent*>(aActor)->DoAsyncResolve(aHost, aFlags,
                                                         aNetworkInterface);
  return true;
}

bool
NeckoParent::DeallocPDNSRequestParent(PDNSRequestParent* aParent)
{
  DNSRequestParent *p = static_cast<DNSRequestParent*>(aParent);
  p->Release();
  return true;
}

PRemoteOpenFileParent*
NeckoParent::AllocPRemoteOpenFileParent(const SerializedLoadContext& aSerialized,
                                        const URIParams& aURI,
                                        const OptionalURIParams& aAppURI)
{
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(uri);
  if (!fileURL) {
    return nullptr;
  }

  // security checks
  if (UsingNeckoIPCSecurity()) {
    nsCOMPtr<nsIAppsService> appsService =
      do_GetService(APPS_SERVICE_CONTRACTID);
    if (!appsService) {
      return nullptr;
    }
    bool haveValidBrowser = false;
    bool hasManage = false;
    nsCOMPtr<mozIApplication> mozApp;
    nsTArray<TabContext> contextArray =
      static_cast<ContentParent*>(Manager())->GetManagedTabContext();
    for (uint32_t i = 0; i < contextArray.Length(); i++) {
      TabContext tabContext = contextArray[i];
      uint32_t appId = tabContext.OwnOrContainingAppId();
      // Note: this enforces that SerializedLoadContext.appID is one of the apps
      // in the child process, but there's currently no way to verify the
      // request is not from a different app in that process.
      if (appId == aSerialized.mAppId) {
        nsresult rv = appsService->GetAppByLocalId(appId, getter_AddRefs(mozApp));
        if (NS_FAILED(rv) || !mozApp) {
          break;
        }
        rv = mozApp->HasPermission("webapps-manage", &hasManage);
        if (NS_FAILED(rv)) {
          break;
        }
        haveValidBrowser = true;
        break;
      }
    }

    nsCOMPtr<nsIURI> appUri = DeserializeURI(aAppURI);

    if (!haveValidBrowser) {
      // Extension loads come from chrome and have no valid browser, so we check
      // for these early on.
      bool fromExtension = false;
      if (NS_SUCCEEDED(appsService->IsExtensionResource(appUri, &fromExtension)) &&
          fromExtension) {
        RemoteOpenFileParent* parent = new RemoteOpenFileParent(fileURL);
        return parent;
      }
      return nullptr;
    }

    nsAutoCString requestedPath;
    fileURL->GetPath(requestedPath);
    NS_UnescapeURL(requestedPath);

    // Check if we load the whitelisted app uri for the neterror page.
    bool netErrorWhiteList = false;

    if (appUri) {
      nsAdoptingString netErrorURI;
      netErrorURI = Preferences::GetString("b2g.neterror.url");
      if (netErrorURI) {
        nsAutoCString spec;
        appUri->GetSpec(spec);
        netErrorWhiteList = spec.Equals(NS_ConvertUTF16toUTF8(netErrorURI).get());
      }
    }

    // Check if we load a resource from the shared theme url space.
    // If we try to load the theme but have no permission, refuse to load.
    bool themeWhitelist = false;
    if (Preferences::GetBool("dom.mozApps.themable") && appUri) {
      nsAutoCString origin;
      nsPrincipal::GetOriginForURI(appUri, origin);
      nsAutoCString themeOrigin;
      themeOrigin = Preferences::GetCString("b2g.theme.origin");
      themeWhitelist = origin.Equals(themeOrigin);
      if (themeWhitelist) {
        bool hasThemePerm = false;
        mozApp->HasPermission("themeable", &hasThemePerm);
        if (!hasThemePerm) {
          return nullptr;
        }
      }
    }

    if (hasManage || netErrorWhiteList || themeWhitelist) {
      // webapps-manage permission means allow reading any application.zip file
      // in either the regular webapps directory, or the core apps directory (if
      // we're using one).
      NS_NAMED_LITERAL_CSTRING(appzip, "/application.zip");
      nsAutoCString pathEnd;
      requestedPath.Right(pathEnd, appzip.Length());
      if (!pathEnd.Equals(appzip)) {
        return nullptr;
      }
      nsAutoCString pathStart;
      requestedPath.Left(pathStart, mWebAppsBasePath.Length());
      if (!pathStart.Equals(mWebAppsBasePath)) {
        if (mCoreAppsBasePath.IsEmpty()) {
          return nullptr;
        }
        requestedPath.Left(pathStart, mCoreAppsBasePath.Length());
        if (!pathStart.Equals(mCoreAppsBasePath)) {
          return nullptr;
        }
      }
      // Finally: make sure there are no "../" in URI.
      // Note: not checking for symlinks (would cause I/O for each path
      // component).  So it's up to us to avoid creating symlinks that could
      // provide attack vectors.
      if (PL_strnstr(requestedPath.BeginReading(), "/../",
                     requestedPath.Length())) {
        printf_stderr("NeckoParent::AllocPRemoteOpenFile: "
                      "FATAL error: requested file URI '%s' contains '/../' "
                      "KILLING CHILD PROCESS\n", requestedPath.get());
        return nullptr;
      }
    } else {
      // regular packaged apps can only access their own application.zip file
      nsAutoString basePath;
      nsresult rv = mozApp->GetBasePath(basePath);
      if (NS_FAILED(rv)) {
        return nullptr;
      }
      nsAutoString uuid;
      rv = mozApp->GetId(uuid);
      if (NS_FAILED(rv)) {
        return nullptr;
      }
      nsPrintfCString mustMatch("%s/%s/application.zip",
                                NS_LossyConvertUTF16toASCII(basePath).get(),
                                NS_LossyConvertUTF16toASCII(uuid).get());
      if (!requestedPath.Equals(mustMatch)) {
        printf_stderr("NeckoParent::AllocPRemoteOpenFile: "
                      "FATAL error: app without webapps-manage permission is "
                      "requesting file '%s' but is only allowed to open its "
                      "own application.zip at %s: KILLING CHILD PROCESS\n",
                      requestedPath.get(), mustMatch.get());
        return nullptr;
      }
    }
  }

  RemoteOpenFileParent* parent = new RemoteOpenFileParent(fileURL);
  return parent;
}

bool
NeckoParent::RecvPRemoteOpenFileConstructor(
                PRemoteOpenFileParent* aActor,
                const SerializedLoadContext& aSerialized,
                const URIParams& aFileURI,
                const OptionalURIParams& aAppURI)
{
  return static_cast<RemoteOpenFileParent*>(aActor)->OpenSendCloseDelete();
}

bool
NeckoParent::DeallocPRemoteOpenFileParent(PRemoteOpenFileParent* actor)
{
  delete actor;
  return true;
}

bool
NeckoParent::RecvSpeculativeConnect(const URIParams& aURI, const bool& aAnonymous)
{
  nsCOMPtr<nsISpeculativeConnect> speculator(gIOService);
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (uri && speculator) {
    if (aAnonymous) {
      speculator->SpeculativeAnonymousConnect(uri, nullptr);
    } else {
      speculator->SpeculativeConnect(uri, nullptr);
    }

  }
  return true;
}

bool
NeckoParent::RecvHTMLDNSPrefetch(const nsString& hostname,
                                 const uint16_t& flags)
{
  nsHTMLDNSPrefetch::Prefetch(hostname, flags);
  return true;
}

bool
NeckoParent::RecvCancelHTMLDNSPrefetch(const nsString& hostname,
                                 const uint16_t& flags,
                                 const nsresult& reason)
{
  nsHTMLDNSPrefetch::CancelPrefetch(hostname, flags, reason);
  return true;
}

PChannelDiverterParent*
NeckoParent::AllocPChannelDiverterParent(const ChannelDiverterArgs& channel)
{
  return new ChannelDiverterParent();
}

bool
NeckoParent::RecvPChannelDiverterConstructor(PChannelDiverterParent* actor,
                                             const ChannelDiverterArgs& channel)
{
  auto parent = static_cast<ChannelDiverterParent*>(actor);
  parent->Init(channel);
  return true;
}

bool
NeckoParent::DeallocPChannelDiverterParent(PChannelDiverterParent* parent)
{
  delete static_cast<ChannelDiverterParent*>(parent);
  return true;
}

void
NeckoParent::CloneManagees(ProtocolBase* aSource,
                         mozilla::ipc::ProtocolCloneContext* aCtx)
{
  aCtx->SetNeckoParent(this); // For cloning protocols managed by this.
  PNeckoParent::CloneManagees(aSource, aCtx);
}

mozilla::ipc::IProtocol*
NeckoParent::CloneProtocol(Channel* aChannel,
                           mozilla::ipc::ProtocolCloneContext* aCtx)
{
  ContentParent* contentParent = aCtx->GetContentParent();
  nsAutoPtr<PNeckoParent> actor(contentParent->AllocPNeckoParent());
  if (!actor || !contentParent->RecvPNeckoConstructor(actor)) {
    return nullptr;
  }
  return actor.forget();
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

bool
NeckoParent::RecvOnAuthAvailable(const uint64_t& aCallbackId,
                                 const nsString& aUser,
                                 const nsString& aPassword,
                                 const nsString& aDomain)
{
  nsCOMPtr<nsIAuthPromptCallback> callback = CallbackMap()[aCallbackId];
  if (!callback) {
    return true;
  }
  CallbackMap().erase(aCallbackId);

  nsRefPtr<nsAuthInformationHolder> holder =
    new nsAuthInformationHolder(0, EmptyString(), EmptyCString());
  holder->SetUsername(aUser);
  holder->SetPassword(aPassword);
  holder->SetDomain(aDomain);

  callback->OnAuthAvailable(nullptr, holder);
  return true;
}

bool
NeckoParent::RecvOnAuthCancelled(const uint64_t& aCallbackId,
                                 const bool& aUserCancel)
{
  nsCOMPtr<nsIAuthPromptCallback> callback = CallbackMap()[aCallbackId];
  if (!callback) {
    return true;
  }
  CallbackMap().erase(aCallbackId);
  callback->OnAuthCancelled(nullptr, aUserCancel);
  return true;
}

/* Predictor Messages */
bool
NeckoParent::RecvPredPredict(const ipc::OptionalURIParams& aTargetURI,
                             const ipc::OptionalURIParams& aSourceURI,
                             const uint32_t& aReason,
                             const SerializedLoadContext& aLoadContext,
                             const bool& hasVerifier)
{
  nsCOMPtr<nsIURI> targetURI = DeserializeURI(aTargetURI);
  nsCOMPtr<nsIURI> sourceURI = DeserializeURI(aSourceURI);

  // We only actually care about the loadContext.mPrivateBrowsing, so we'll just
  // pass dummy params for nestFrameId, inBrowser and appId
  uint64_t nestedFrameId = 0;
  nsCOMPtr<nsILoadContext> loadContext;
  if (aLoadContext.IsNotNull()) {
    loadContext = new LoadContext(aLoadContext, nestedFrameId, NECKO_UNKNOWN_APP_ID, false);
  }

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsINetworkPredictorVerifier> verifier;
  if (hasVerifier) {
    verifier = do_QueryInterface(predictor);
  }
  predictor->Predict(targetURI, sourceURI, aReason, loadContext, verifier);
  return true;
}

bool
NeckoParent::RecvPredLearn(const ipc::URIParams& aTargetURI,
                           const ipc::OptionalURIParams& aSourceURI,
                           const uint32_t& aReason,
                           const SerializedLoadContext& aLoadContext)
{
  nsCOMPtr<nsIURI> targetURI = DeserializeURI(aTargetURI);
  nsCOMPtr<nsIURI> sourceURI = DeserializeURI(aSourceURI);

  // We only actually care about the loadContext.mPrivateBrowsing, so we'll just
  // pass dummy params for nestFrameId, inBrowser and appId
  uint64_t nestedFrameId = 0;
  nsCOMPtr<nsILoadContext> loadContext;
  if (aLoadContext.IsNotNull()) {
    loadContext = new LoadContext(aLoadContext, nestedFrameId, NECKO_UNKNOWN_APP_ID, false);
  }

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, false);

  predictor->Learn(targetURI, sourceURI, aReason, loadContext);
  return true;
}

bool
NeckoParent::RecvPredReset()
{
  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, false);

  predictor->Reset();
  return true;
}

nsresult
NeckoParent::OfflineNotification(nsISupports *aSubject)
{
  nsCOMPtr<nsIAppOfflineInfo> info(do_QueryInterface(aSubject));
  if (!info) {
    return NS_OK;
  }

  uint32_t targetAppId = NECKO_UNKNOWN_APP_ID;
  info->GetAppId(&targetAppId);

  nsTArray<TabContext> contextArray =
      static_cast<ContentParent*>(Manager())->GetManagedTabContext();
  for (uint32_t i = 0; i < contextArray.Length(); ++i) {
    TabContext tabContext = contextArray[i];
    uint32_t appId = tabContext.OwnOrContainingAppId();

    if (appId == targetAppId) {
      if (gIOService) {
        bool offline = false;
        nsresult rv = gIOService->IsAppOffline(appId, &offline);
        if (NS_FAILED(rv)) {
          printf_stderr("Unexpected - NeckoParent: "
                        "appId not found by isAppOffline(): %u\n", appId);
          break;
        }
        if (!SendAppOfflineStatus(appId, offline)) {
          printf_stderr("NeckoParent: "
                        "SendAppOfflineStatus failed for appId: %u\n", appId);
        }
        // Once we found the targetAppId, we don't need to continue
        break;
      }
    }

  }

  return NS_OK;
}

bool
NeckoParent::RecvRemoveSchedulingContext(const nsCString& scid)
{
  nsCOMPtr<nsISchedulingContextService> scsvc =
    do_GetService("@mozilla.org/network/scheduling-context-service;1");
  if (!scsvc) {
    return true;
  }

  nsID id;
  id.Parse(scid.BeginReading());
  scsvc->RemoveSchedulingContext(id);

  return true;
}

} // namespace net
} // namespace mozilla
