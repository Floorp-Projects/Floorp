/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessChild_h
#define mozilla_net_SocketProcessChild_h

#include "mozilla/net/PSocketProcessChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
class ChildProfilerController;
}

namespace mozilla {
namespace net {

class SocketProcessBridgeParent;

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

  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& aPref);
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage,
      const Maybe<mozilla::ipc::FileDescriptor>& DMDFile);
  mozilla::ipc::IPCResult RecvSetOffline(const bool& aOffline);
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
  already_AddRefed<PHttpConnectionMgrChild> AllocPHttpConnectionMgrChild();

  mozilla::ipc::IPCResult RecvOnHttpActivityDistributorActivated(
      const bool& aIsActivated);

  already_AddRefed<PInputChannelThrottleQueueChild>
  AllocPInputChannelThrottleQueueChild(const uint32_t& aMeanBytesPerSecond,
                                       const uint32_t& aMaxBytesPerSecond);

  already_AddRefed<PAltSvcTransactionChild> AllocPAltSvcTransactionChild(
      const HttpConnectionInfoCloneArgs& aConnInfo, const uint32_t& aCaps);

  bool IsShuttingDown() { return mShuttingDown; }

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
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessChild_h
