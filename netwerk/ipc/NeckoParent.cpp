/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/WyciwygChannelParent.h"
#include "mozilla/net/FTPChannelParent.h"
#include "mozilla/net/WebSocketChannelParent.h"
#include "mozilla/net/RemoteOpenFileParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/network/TCPSocketParent.h"
#include "mozilla/dom/network/TCPServerSocketParent.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/LoadContext.h"
#include "mozilla/AppProcessChecker.h"
#include "nsPrintfCString.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsIAppsService.h"
#include "nsEscape.h"
#include "RemoteOpenFileParent.h"

using mozilla::dom::ContentParent;
using mozilla::dom::TabParent;
using mozilla::net::PTCPSocketParent;
using mozilla::dom::TCPSocketParent;
using mozilla::net::PTCPServerSocketParent;
using mozilla::dom::TCPServerSocketParent;
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
}

NeckoParent::~NeckoParent()
{
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

  const InfallibleTArray<PBrowserParent*>& browsers = aContent->ManagedPBrowserParent();
  for (uint32_t i = 0; i < browsers.Length(); i++) {
    // GetValidatedAppInfo returning null means we passed security checks.
    if (!GetValidatedAppInfo(aSerialized, browsers[i], aAppId, aInBrowserElement)) {
      return nullptr;
    }
  }

  if (browsers.Length() == 0) {
    if (UsingNeckoIPCSecurity()) {
      return "ContentParent does not have any PBrowsers";
    }
    if (aSerialized.IsNotNull()) {
      *aAppId = aSerialized.mAppId;
      *aInBrowserElement = aSerialized.mIsInBrowserElement;
    } else {
      *aAppId = NECKO_NO_APP_ID;
    }
    return nullptr;
  }

  // If we reached this point, we failed the security check.
  // Try to return a reasonable error message.
  return GetValidatedAppInfo(aSerialized, browsers[0], aAppId, aInBrowserElement);
}

const char*
NeckoParent::GetValidatedAppInfo(const SerializedLoadContext& aSerialized,
                                 PBrowserParent* aBrowser,
                                 uint32_t* aAppId,
                                 bool* aInBrowserElement)
{
  if (UsingNeckoIPCSecurity()) {
    if (!aBrowser) {
      return "missing required PBrowser argument";
    }
    if (!aSerialized.IsNotNull()) {
      return "SerializedLoadContext from child is null";
    }
  }

  *aAppId = NECKO_UNKNOWN_APP_ID;
  *aInBrowserElement = false;

  if (aBrowser) {
    nsRefPtr<TabParent> tabParent = static_cast<TabParent*>(aBrowser);

    *aAppId = tabParent->OwnOrContainingAppId();
    *aInBrowserElement = aSerialized.IsNotNull() ? aSerialized.mIsInBrowserElement
                                                 : tabParent->IsBrowserElement();

    if (*aAppId == NECKO_UNKNOWN_APP_ID) {
      return "TabParent reports appId=NECKO_UNKNOWN_APP_ID!";
    }
    // We may get appID=NO_APP if child frame is neither a browser nor an app
    if (*aAppId == NECKO_NO_APP_ID) {
      if (tabParent->HasOwnApp()) {
        return "TabParent reports NECKO_NO_APP_ID but also is an app";
      }
      if (UsingNeckoIPCSecurity() && tabParent->IsBrowserElement()) {
        // <iframe mozbrowser> which doesn't have an <iframe mozapp> above it.
        // This is not supported now, and we'll need to do a code audit to make
        // sure we can handle it (i.e don't short-circuit using separate
        // namespace if just appID==0)
        return "TabParent reports appId=NECKO_NO_APP_ID but is a mozbrowser";
      }
    }
  } else {
    // Only trust appId/inBrowser from child-side loadcontext if we're in
    // testing mode: allows xpcshell tests to masquerade as apps
    MOZ_ASSERT(!UsingNeckoIPCSecurity());
    if (UsingNeckoIPCSecurity()) {
      return "internal error";
    }
    if (aSerialized.IsNotNull()) {
      *aAppId = aSerialized.mAppId;
      *aInBrowserElement = aSerialized.mIsInBrowserElement;
    } else {
      *aAppId = NECKO_NO_APP_ID;
    }
  }
  return nullptr;
}

const char *
NeckoParent::CreateChannelLoadContext(PBrowserParent* aBrowser,
                                      const SerializedLoadContext& aSerialized,
                                      nsCOMPtr<nsILoadContext> &aResult)
{
  uint32_t appId = NECKO_UNKNOWN_APP_ID;
  bool inBrowser = false;
  dom::Element* topFrameElement = nullptr;
  const char* error = GetValidatedAppInfo(aSerialized, aBrowser, &appId, &inBrowser);
  if (error) {
    return error;
  }

  if (aBrowser) {
    nsRefPtr<TabParent> tabParent = static_cast<TabParent*>(aBrowser);
    topFrameElement = tabParent->GetOwnerElement();
  }

  // if !UsingNeckoIPCSecurity(), we may not have a LoadContext to set. This is
  // the common case for most xpcshell tests.
  if (aSerialized.IsNotNull()) {
    aResult = new LoadContext(aSerialized, topFrameElement, appId, inBrowser);
  }

  return nullptr;
}

PHttpChannelParent*
NeckoParent::AllocPHttpChannelParent(PBrowserParent* aBrowser,
                                     const SerializedLoadContext& aSerialized,
                                     const HttpChannelCreationArgs& aOpenArgs)
{
  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(aBrowser, aSerialized,
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

bool
NeckoParent::RecvPHttpChannelConstructor(
                      PHttpChannelParent* aActor,
                      PBrowserParent* aBrowser,
                      const SerializedLoadContext& aSerialized,
                      const HttpChannelCreationArgs& aOpenArgs)
{
  HttpChannelParent* p = static_cast<HttpChannelParent*>(aActor);
  return p->Init(aOpenArgs);
}

PFTPChannelParent*
NeckoParent::AllocPFTPChannelParent(PBrowserParent* aBrowser,
                                    const SerializedLoadContext& aSerialized,
                                    const FTPChannelCreationArgs& aOpenArgs)
{
  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(aBrowser, aSerialized,
                                               loadContext);
  if (error) {
    printf_stderr("NeckoParent::AllocPFTPChannelParent: "
                  "FATAL error: %s: KILLING CHILD PROCESS\n",
                  error);
    return nullptr;
  }
  PBOverrideStatus overrideStatus = PBOverrideStatusFromLoadContext(aSerialized);
  FTPChannelParent *p = new FTPChannelParent(loadContext, overrideStatus);
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
                      PBrowserParent* aBrowser,
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
NeckoParent::AllocPWebSocketParent(PBrowserParent* browser,
                                   const SerializedLoadContext& serialized)
{
  nsCOMPtr<nsILoadContext> loadContext;
  const char *error = CreateChannelLoadContext(browser, serialized,
                                               loadContext);
  if (error) {
    printf_stderr("NeckoParent::AllocPWebSocketParent: "
                  "FATAL error: %s: KILLING CHILD PROCESS\n",
                  error);
    return nullptr;
  }

  TabParent* tabParent = static_cast<TabParent*>(browser);
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

PTCPSocketParent*
NeckoParent::AllocPTCPSocketParent()
{
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

PRemoteOpenFileParent*
NeckoParent::AllocPRemoteOpenFileParent(const URIParams& aURI)
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
    for (uint32_t i = 0; i < Manager()->ManagedPBrowserParent().Length(); i++) {
      nsRefPtr<TabParent> tabParent =
        static_cast<TabParent*>(Manager()->ManagedPBrowserParent()[i]);
      uint32_t appId = tabParent->OwnOrContainingAppId();
      nsCOMPtr<mozIDOMApplication> domApp;
      nsresult rv = appsService->GetAppByLocalId(appId, getter_AddRefs(domApp));
      if (!domApp) {
        continue;
      }
      mozApp = do_QueryInterface(domApp);
      if (!mozApp) {
        continue;
      }
      hasManage = false;
      rv = mozApp->HasPermission("webapps-manage", &hasManage);
      if (NS_FAILED(rv)) {
        continue;
      }
      haveValidBrowser = true;
      break;
    }

    if (!haveValidBrowser) {
      return nullptr;
    }

    nsAutoCString requestedPath;
    fileURL->GetPath(requestedPath);
    NS_UnescapeURL(requestedPath);

    if (hasManage) {
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
                      "own application.zip: KILLING CHILD PROCESS\n",
                      requestedPath.get());
        return nullptr;
      }
    }
  }

  RemoteOpenFileParent* parent = new RemoteOpenFileParent(fileURL);
  return parent;
}

bool
NeckoParent::RecvPRemoteOpenFileConstructor(PRemoteOpenFileParent* aActor,
                                            const URIParams& aFileURI)
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

}} // mozilla::net
