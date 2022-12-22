/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessParent_h
#define mozilla_net_SocketProcessParent_h

#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/CrashReporterHelper.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/net/PSocketProcessParent.h"

namespace mozilla {

namespace dom {
class MemoryReport;
class MemoryReportRequestHost;
}  // namespace dom

namespace net {

class SocketProcessHost;

// IPC actor of socket process in parent process. This is allocated and managed
// by SocketProcessHost.
class SocketProcessParent final
    : public PSocketProcessParent,
      public ipc::CrashReporterHelper<GeckoProcessType_Socket> {
 public:
  friend class SocketProcessHost;

  explicit SocketProcessParent(SocketProcessHost* aHost);
  ~SocketProcessParent();

  static SocketProcessParent* GetSingleton();

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);
  mozilla::ipc::IPCResult RecvAccumulateChildHistograms(
      nsTArray<HistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistograms(
      nsTArray<KeyedHistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvUpdateChildScalars(
      nsTArray<ScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvUpdateChildKeyedScalars(
      nsTArray<KeyedScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvRecordChildEvents(
      nsTArray<ChildEventData>&& events);
  mozilla::ipc::IPCResult RecvRecordDiscardedData(
      const DiscardedData& aDiscardedData);

  PWebrtcTCPSocketParent* AllocPWebrtcTCPSocketParent(
      const Maybe<TabId>& aTabId);
  bool DeallocPWebrtcTCPSocketParent(PWebrtcTCPSocketParent* aActor);
  already_AddRefed<PDNSRequestParent> AllocPDNSRequestParent(
      const nsACString& aHost, const nsACString& aTrrServer,
      const int32_t& port, const uint16_t& aType,
      const OriginAttributes& aOriginAttributes, const uint32_t& aFlags);
  virtual mozilla::ipc::IPCResult RecvPDNSRequestConstructor(
      PDNSRequestParent* actor, const nsACString& aHost,
      const nsACString& trrServer, const int32_t& port, const uint16_t& type,
      const OriginAttributes& aOriginAttributes,
      const uint32_t& flags) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<ipc::FileDescriptor>& aDMDFile);

  mozilla::ipc::IPCResult RecvObserveHttpActivity(
      const HttpActivityArgs& aArgs, const uint32_t& aActivityType,
      const uint32_t& aActivitySubtype, const PRTime& aTimestamp,
      const uint64_t& aExtraSizeData, const nsACString& aExtraStringData);

  mozilla::ipc::IPCResult RecvInitBackground(
      Endpoint<PBackgroundStarterParent>&& aEndpoint);

  already_AddRefed<PAltServiceParent> AllocPAltServiceParent();

  mozilla::ipc::IPCResult RecvFindIPCClientCertObjects(
      nsTArray<IPCClientCertObject>* aObjects);
  mozilla::ipc::IPCResult RecvIPCClientCertSign(ByteArray aCert,
                                                ByteArray aData,
                                                ByteArray aParams,
                                                ByteArray* aSignature);

  already_AddRefed<PProxyConfigLookupParent> AllocPProxyConfigLookupParent(
      nsIURI* aURI, const uint32_t& aProxyResolveFlags);
  mozilla::ipc::IPCResult RecvPProxyConfigLookupConstructor(
      PProxyConfigLookupParent* aActor, nsIURI* aURI,
      const uint32_t& aProxyResolveFlags) override;

  mozilla::ipc::IPCResult RecvCachePushCheck(
      nsIURI* aPushedURL, OriginAttributes&& aOriginAttributes,
      nsCString&& aRequestString, CachePushCheckResolver&& aResolver);

  mozilla::ipc::IPCResult RecvODoHServiceActivated(const bool& aActivated);

  mozilla::ipc::IPCResult RecvExcludeHttp2OrHttp3(
      const HttpConnectionInfoCloneArgs& aArgs);
  mozilla::ipc::IPCResult RecvOnConsoleMessage(const nsString& aMessage);

  mozilla::ipc::IPCResult RecvFOGData(ByteBuf&& aBuf);

#if defined(XP_WIN)
  mozilla::ipc::IPCResult RecvGetModulesTrust(
      ModulePaths&& aModPaths, bool aRunAtNormalPriority,
      GetModulesTrustResolver&& aResolver);
#endif  // defined(XP_WIN)

 private:
  SocketProcessHost* mHost;
  UniquePtr<dom::MemoryReportRequestHost> mMemoryReportRequest;

  static void Destroy(UniquePtr<SocketProcessParent>&& aParent);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessParent_h
