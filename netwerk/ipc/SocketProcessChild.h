/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessChild_h
#define mozilla_net_SocketProcessChild_h

#include "mozilla/net/PSocketProcessChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/Mutex.h"
#include "nsRefPtrHashtable.h"
#include "nsTHashMap.h"

namespace mozilla {
class ChildProfilerController;
}

namespace mozilla {
namespace net {

class SocketProcessBridgeParent;
class BackgroundDataBridgeParent;

// The IPC actor implements PSocketProcessChild in child process.
// This is allocated and kept alive by SocketProcessImpl.
class SocketProcessChild final
    : public PSocketProcessChild,
      public mozilla::ipc::ChildToParentStreamActorManager {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SocketProcessChild)

  SocketProcessChild();

  static SocketProcessChild* GetSingleton();

  bool Init(base::ProcessId aParentPid, const char* aParentBuildID,
            MessageLoop* aIOLoop, UniquePtr<IPC::Channel> aChannel);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvInit(
      const SocketPorcessInitAttributes& aAttributes);
  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& aPref);
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage,
      const Maybe<mozilla::ipc::FileDescriptor>& DMDFile,
      const RequestMemoryReportResolver& aResolver);
  mozilla::ipc::IPCResult RecvSetOffline(const bool& aOffline);
  mozilla::ipc::IPCResult RecvSetConnectivity(const bool& aConnectivity);
  mozilla::ipc::IPCResult RecvInitLinuxSandbox(
      const Maybe<ipc::FileDescriptor>& aBrokerFd);
  mozilla::ipc::IPCResult RecvInitSocketProcessBridgeParent(
      const ProcessId& aContentProcessId,
      Endpoint<mozilla::net::PSocketProcessBridgeParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvInitProfiler(
      Endpoint<mozilla::PProfilerChild>&& aEndpoint);
  mozilla::ipc::IPCResult RecvSocketProcessTelemetryPing();

  PWebrtcTCPSocketChild* AllocPWebrtcTCPSocketChild(const Maybe<TabId>& tabId);
  bool DeallocPWebrtcTCPSocketChild(PWebrtcTCPSocketChild* aActor);

  already_AddRefed<PHttpTransactionChild> AllocPHttpTransactionChild();

  PFileDescriptorSetChild* AllocPFileDescriptorSetChild(
      const FileDescriptor& fd);
  bool DeallocPFileDescriptorSetChild(PFileDescriptorSetChild* aActor);

  PChildToParentStreamChild* AllocPChildToParentStreamChild();
  bool DeallocPChildToParentStreamChild(PChildToParentStreamChild* aActor);
  PParentToChildStreamChild* AllocPParentToChildStreamChild();
  bool DeallocPParentToChildStreamChild(PParentToChildStreamChild* aActor);

  void CleanUp();
  void DestroySocketProcessBridgeParent(ProcessId aId);

  PChildToParentStreamChild* SendPChildToParentStreamConstructor(
      PChildToParentStreamChild* aActor) override;
  PFileDescriptorSetChild* SendPFileDescriptorSetConstructor(
      const FileDescriptor& aFD) override;
  already_AddRefed<PHttpConnectionMgrChild> AllocPHttpConnectionMgrChild(
      const HttpHandlerInitArgs& aArgs);
  mozilla::ipc::IPCResult RecvUpdateDeviceModelId(const nsCString& aModelId);
  mozilla::ipc::IPCResult RecvOnHttpActivityDistributorActivated(
      const bool& aIsActivated);

  already_AddRefed<PInputChannelThrottleQueueChild>
  AllocPInputChannelThrottleQueueChild(const uint32_t& aMeanBytesPerSecond,
                                       const uint32_t& aMaxBytesPerSecond);

  already_AddRefed<PAltSvcTransactionChild> AllocPAltSvcTransactionChild(
      const HttpConnectionInfoCloneArgs& aConnInfo, const uint32_t& aCaps);

  bool IsShuttingDown() { return mShuttingDown; }

  already_AddRefed<PDNSRequestChild> AllocPDNSRequestChild(
      const nsCString& aHost, const nsCString& aTrrServer,
      const uint16_t& aType, const OriginAttributes& aOriginAttributes,
      const uint32_t& aFlags);
  mozilla::ipc::IPCResult RecvPDNSRequestConstructor(
      PDNSRequestChild* aActor, const nsCString& aHost,
      const nsCString& aTrrServer, const uint16_t& aType,
      const OriginAttributes& aOriginAttributes,
      const uint32_t& aFlags) override;

  void AddDataBridgeToMap(uint64_t aChannelId,
                          BackgroundDataBridgeParent* aActor);
  void RemoveDataBridgeFromMap(uint64_t aChannelId);
  Maybe<RefPtr<BackgroundDataBridgeParent>> GetAndRemoveDataBridge(
      uint64_t aChannelId);

  mozilla::ipc::IPCResult RecvClearSessionCache();

  already_AddRefed<PTRRServiceChild> AllocPTRRServiceChild(
      const bool& aCaptiveIsPassed, const bool& aParentalControlEnabled,
      const nsTArray<nsCString>& aDNSSuffixList);
  mozilla::ipc::IPCResult RecvPTRRServiceConstructor(
      PTRRServiceChild* aActor, const bool& aCaptiveIsPassed,
      const bool& aParentalControlEnabled,
      nsTArray<nsCString>&& aDNSSuffixList) override;

  already_AddRefed<PNativeDNSResolverOverrideChild>
  AllocPNativeDNSResolverOverrideChild();
  mozilla::ipc::IPCResult RecvPNativeDNSResolverOverrideConstructor(
      PNativeDNSResolverOverrideChild* aActor) override;

  mozilla::ipc::IPCResult RecvNotifyObserver(const nsCString& aTopic,
                                             const nsString& aData);

  virtual already_AddRefed<PRemoteLazyInputStreamChild>
  AllocPRemoteLazyInputStreamChild(const nsID& aID, const uint64_t& aSize);

  mozilla::ipc::IPCResult RecvGetSocketData(GetSocketDataResolver&& aResolve);
  mozilla::ipc::IPCResult RecvGetDNSCacheEntries(
      GetDNSCacheEntriesResolver&& aResolve);
  mozilla::ipc::IPCResult RecvGetHttpConnectionData(
      GetHttpConnectionDataResolver&& aResolve);

 protected:
  friend class SocketProcessImpl;
  ~SocketProcessChild();

 private:
  // Mapping of content process id and the SocketProcessBridgeParent.
  // This table keeps SocketProcessBridgeParent alive in socket process.
  nsRefPtrHashtable<nsUint32HashKey, SocketProcessBridgeParent>
      mSocketProcessBridgeParentMap;

#ifdef MOZ_GECKO_PROFILER
  RefPtr<ChildProfilerController> mProfilerController;
#endif

  bool mShuttingDown;
  // Protect the table below.
  Mutex mMutex;
  nsTHashMap<uint64_t, RefPtr<BackgroundDataBridgeParent>>
      mBackgroundDataBridgeMap;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessChild_h
