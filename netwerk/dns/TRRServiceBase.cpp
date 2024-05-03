/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TRRServiceBase.h"

#include "TRRService.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "nsHostResolver.h"
#include "nsNetUtil.h"
#include "nsIOService.h"
#include "nsIDNSService.h"
#include "nsIProxyInfo.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpHandler.h"
#include "mozilla/StaticPrefs_network.h"
#include "AlternateServices.h"
#include "ProxyConfigLookup.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"

#if defined(XP_WIN)
#  include <shlobj.h>  // for SHGetSpecialFolderPathA
#endif                 // XP_WIN

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(TRRServiceBase, nsIProxyConfigChangedCallback)

TRRServiceBase::TRRServiceBase()
    : mDefaultTRRConnectionInfo("DataMutex::mDefaultTRRConnectionInfo") {}

TRRServiceBase::~TRRServiceBase() {
  if (mTRRConnectionInfoInited) {
    UnregisterProxyChangeListener();
  }
}

void TRRServiceBase::ProcessURITemplate(nsACString& aURI) {
  // URI Template, RFC 6570.
  if (aURI.IsEmpty()) {
    return;
  }
  nsAutoCString scheme;
  nsCOMPtr<nsIIOService> ios(do_GetIOService());
  if (ios) {
    ios->ExtractScheme(aURI, scheme);
  }
  if (!scheme.Equals("https")) {
    LOG(("TRRService TRR URI %s is not https. Not used.\n",
         PromiseFlatCString(aURI).get()));
    aURI.Truncate();
    return;
  }

  // cut off everything from "{" to "}" sequences (potentially multiple),
  // as a crude conversion from template into URI.
  nsAutoCString uri(aURI);

  do {
    nsCCharSeparatedTokenizer openBrace(uri, '{');
    if (openBrace.hasMoreTokens()) {
      // the 'nextToken' is the left side of the open brace (or full uri)
      nsAutoCString prefix(openBrace.nextToken());

      // if there is an open brace, there's another token
      const nsACString& endBrace = openBrace.nextToken();
      nsCCharSeparatedTokenizer closeBrace(endBrace, '}');
      if (closeBrace.hasMoreTokens()) {
        // there is a close brace as well, make a URI out of the prefix
        // and the suffix
        closeBrace.nextToken();
        nsAutoCString suffix(closeBrace.nextToken());
        uri = prefix + suffix;
      } else {
        // no (more) close brace
        break;
      }
    } else {
      // no (more) open brace
      break;
    }
  } while (true);

  aURI = uri;
}

void TRRServiceBase::CheckURIPrefs() {
  mURISetByDetection = false;

  if (StaticPrefs::network_trr_use_ohttp() && !mOHTTPURIPref.IsEmpty()) {
    MaybeSetPrivateURI(mOHTTPURIPref);
    return;
  }

  // The user has set a custom URI so it takes precedence.
  if (!mURIPref.IsEmpty()) {
    MaybeSetPrivateURI(mURIPref);
    return;
  }

  // Check if the rollout addon has set a pref.
  if (!mRolloutURIPref.IsEmpty()) {
    MaybeSetPrivateURI(mRolloutURIPref);
    return;
  }

  // Otherwise just use the default value.
  MaybeSetPrivateURI(mDefaultURIPref);
}

// static
nsIDNSService::ResolverMode ModeFromPrefs(
    nsIDNSService::ResolverMode& aTRRModePrefValue) {
  // 0 - off, 1 - reserved, 2 - TRR first, 3 - TRR only, 4 - reserved,
  // 5 - explicit off

  auto processPrefValue = [](uint32_t value) -> nsIDNSService::ResolverMode {
    if (value == nsIDNSService::MODE_RESERVED1 ||
        value == nsIDNSService::MODE_RESERVED4 ||
        value > nsIDNSService::MODE_TRROFF) {
      return nsIDNSService::MODE_TRROFF;
    }
    return static_cast<nsIDNSService::ResolverMode>(value);
  };

  uint32_t tmp;
  if (NS_FAILED(Preferences::GetUint("network.trr.mode", &tmp))) {
    tmp = 0;
  }
  nsIDNSService::ResolverMode modeFromPref = processPrefValue(tmp);
  aTRRModePrefValue = modeFromPref;

  if (modeFromPref != nsIDNSService::MODE_NATIVEONLY) {
    return modeFromPref;
  }

  if (NS_FAILED(Preferences::GetUint(kRolloutModePref, &tmp))) {
    tmp = 0;
  }
  modeFromPref = processPrefValue(tmp);

  return modeFromPref;
}

void TRRServiceBase::OnTRRModeChange() {
  uint32_t oldMode = mMode;
  // This is the value of the pref "network.trr.mode"
  nsIDNSService::ResolverMode trrModePrefValue;
  mMode = ModeFromPrefs(trrModePrefValue);
  if (mMode != oldMode) {
    LOG(("TRR Mode changed from %d to %d", oldMode, int(mMode)));
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, NS_NETWORK_TRR_MODE_CHANGED_TOPIC, nullptr);
    }

    TRRService::SetCurrentTRRMode(trrModePrefValue);
  }

  static bool readHosts = false;
  // When native HTTPS query is enabled, we need to read etc/hosts.
  if ((mMode == nsIDNSService::MODE_TRRFIRST ||
       mMode == nsIDNSService::MODE_TRRONLY || mNativeHTTPSQueryEnabled) &&
      !readHosts) {
    readHosts = true;
    ReadEtcHostsFile();
  }
}

void TRRServiceBase::OnTRRURIChange() {
  Preferences::GetCString("network.trr.uri", mURIPref);
  Preferences::GetCString(kRolloutURIPref, mRolloutURIPref);
  Preferences::GetCString("network.trr.default_provider_uri", mDefaultURIPref);
  Preferences::GetCString("network.trr.ohttp.uri", mOHTTPURIPref);

  CheckURIPrefs();
}

static already_AddRefed<nsHttpConnectionInfo> CreateConnInfoHelper(
    nsIURI* aURI, nsIProxyInfo* aProxyInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString host;
  nsAutoCString scheme;
  nsAutoCString username;
  int32_t port = -1;
  bool isHttps = aURI->SchemeIs("https");

  nsresult rv = aURI->GetScheme(scheme);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  rv = aURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  rv = aURI->GetPort(&port);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  // Just a warning here because some nsIURIs do not implement this method.
  if (NS_WARN_IF(NS_FAILED(aURI->GetUsername(username)))) {
    LOG(("Failed to get username for aURI(%s)",
         aURI->GetSpecOrDefault().get()));
  }

  gHttpHandler->MaybeAddAltSvcForTesting(aURI, username, false, nullptr,
                                         OriginAttributes());

  nsCOMPtr<nsProxyInfo> proxyInfo = do_QueryInterface(aProxyInfo);
  RefPtr<nsHttpConnectionInfo> connInfo = new nsHttpConnectionInfo(
      host, port, ""_ns, username, proxyInfo, OriginAttributes(), isHttps);
  bool http2Allowed = !gHttpHandler->IsHttp2Excluded(connInfo);
  bool http3Allowed = proxyInfo ? proxyInfo->IsDirect() : true;

  RefPtr<AltSvcMapping> mapping;
  if ((http2Allowed || http3Allowed) &&
      AltSvcMapping::AcceptableProxy(proxyInfo) &&
      (scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https")) &&
      (mapping = gHttpHandler->GetAltServiceMapping(
           scheme, host, port, false, OriginAttributes(), http2Allowed,
           http3Allowed))) {
    mapping->GetConnectionInfo(getter_AddRefs(connInfo), proxyInfo,
                               OriginAttributes());
  }

  return connInfo.forget();
}

void TRRServiceBase::InitTRRConnectionInfo() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  if (mTRRConnectionInfoInited) {
    return;
  }

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "TRRServiceBase::InitTRRConnectionInfo",
        [self = RefPtr{this}]() { self->InitTRRConnectionInfo(); }));
    return;
  }

  LOG(("TRRServiceBase::InitTRRConnectionInfo"));
  nsAutoCString uri;
  GetURI(uri);
  AsyncCreateTRRConnectionInfoInternal(uri);
}

void TRRServiceBase::AsyncCreateTRRConnectionInfo(const nsACString& aURI) {
  LOG(
      ("TRRServiceBase::AsyncCreateTRRConnectionInfo "
       "mTRRConnectionInfoInited=%d",
       bool(mTRRConnectionInfoInited)));
  if (!mTRRConnectionInfoInited) {
    return;
  }

  AsyncCreateTRRConnectionInfoInternal(aURI);
}

void TRRServiceBase::AsyncCreateTRRConnectionInfoInternal(
    const nsACString& aURI) {
  if (!XRE_IsParentProcess()) {
    return;
  }

  SetDefaultTRRConnectionInfo(nullptr);

  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> dnsURI;
  nsresult rv = NS_NewURI(getter_AddRefs(dnsURI), aURI);
  if (NS_FAILED(rv)) {
    return;
  }

  rv = ProxyConfigLookup::Create(
      [self = RefPtr{this}, uri(dnsURI)](nsIProxyInfo* aProxyInfo,
                                         nsresult aStatus) mutable {
        if (NS_FAILED(aStatus)) {
          self->SetDefaultTRRConnectionInfo(nullptr);
          return;
        }

        RefPtr<nsHttpConnectionInfo> connInfo =
            CreateConnInfoHelper(uri, aProxyInfo);
        self->SetDefaultTRRConnectionInfo(connInfo);
        if (!self->mTRRConnectionInfoInited) {
          self->mTRRConnectionInfoInited = true;
          self->RegisterProxyChangeListener();
        }
      },
      dnsURI, 0, nullptr);

  // mDefaultTRRConnectionInfo is set to nullptr at the beginning of this
  // method, so we don't really care aobut the |rv| here. If it's failed,
  // mDefaultTRRConnectionInfo stays as nullptr and we'll create a new
  // connection info in TRRServiceChannel again.
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

already_AddRefed<nsHttpConnectionInfo> TRRServiceBase::TRRConnectionInfo() {
  RefPtr<nsHttpConnectionInfo> connInfo;
  {
    auto lock = mDefaultTRRConnectionInfo.Lock();
    connInfo = *lock;
  }
  return connInfo.forget();
}

void TRRServiceBase::SetDefaultTRRConnectionInfo(
    nsHttpConnectionInfo* aConnInfo) {
  LOG(("TRRService::SetDefaultTRRConnectionInfo aConnInfo=%s",
       aConnInfo ? aConnInfo->HashKey().get() : "none"));
  {
    auto lock = mDefaultTRRConnectionInfo.Lock();
    lock.ref() = aConnInfo;
  }
}

void TRRServiceBase::RegisterProxyChangeListener() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID);
  if (!pps) {
    return;
  }

  pps->AddProxyConfigCallback(this);
}

void TRRServiceBase::UnregisterProxyChangeListener() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID);
  if (!pps) {
    return;
  }

  pps->RemoveProxyConfigCallback(this);
}

void TRRServiceBase::DoReadEtcHostsFile(ParsingCallback aCallback) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!StaticPrefs::network_trr_exclude_etc_hosts()) {
    return;
  }

  auto readHostsTask = [aCallback]() {
    MOZ_ASSERT(!NS_IsMainThread(), "Must not run on the main thread");
#if defined(XP_WIN)
    // Inspired by libevent/evdns.c
    // Windows is a little coy about where it puts its configuration
    // files.  Sure, they're _usually_ in C:\windows\system32, but
    // there's no reason in principle they couldn't be in
    // W:\hoboken chicken emergency

    nsCString path;
    path.SetLength(MAX_PATH + 1);
    if (!SHGetSpecialFolderPathA(NULL, path.BeginWriting(), CSIDL_SYSTEM,
                                 false)) {
      LOG(("Calling SHGetSpecialFolderPathA failed"));
      return;
    }

    path.SetLength(strlen(path.get()));
    path.Append("\\drivers\\etc\\hosts");
#else
    nsAutoCString path("/etc/hosts"_ns);
#endif

    LOG(("Reading hosts file at %s", path.get()));
    rust_parse_etc_hosts(&path, aCallback);
  };

  Unused << NS_DispatchBackgroundTask(
      NS_NewRunnableFunction("Read /etc/hosts file", readHostsTask),
      NS_DISPATCH_EVENT_MAY_BLOCK);
}

}  // namespace net
}  // namespace mozilla
