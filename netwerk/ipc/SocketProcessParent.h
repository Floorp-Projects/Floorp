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
      public ipc::CrashReporterHelper<GeckoProcessType_Socket>,
      public ipc::ParentToChildStreamActorManager {
 public:
  friend class SocketProcessHost;

  explicit SocketProcessParent(SocketProcessHost* aHost);
  ~SocketProcessParent();

  static SocketProcessParent* GetSingleton();

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);
  mozilla::ipc::IPCResult RecvFinishMemoryReport(const uint32_t& aGeneration);
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
      const nsCString& aHost, const nsCString& aTrrServer,
      const uint16_t& aType, const OriginAttributes& aOriginAttributes,
      const uint32_t& aFlags);
  virtual mozilla::ipc::IPCResult RecvPDNSRequestConstructor(
      PDNSRequestParent* actor, const nsCString& hostName,
      const nsCString& trrServer, const uint16_t& type,
      const OriginAttributes& aOriginAttributes,
      const uint32_t& flags) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<ipc::FileDescriptor>& aDMDFile);

  PFileDescriptorSetParent* AllocPFileDescriptorSetParent(
      const FileDescriptor& fd);
  bool DeallocPFileDescriptorSetParent(PFileDescriptorSetParent* aActor);

  PChildToParentStreamParent* AllocPChildToParentStreamParent();
  bool DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor);
  PParentToChildStreamParent* AllocPParentToChildStreamParent();
  bool DeallocPParentToChildStreamParent(PParentToChildStreamParent* aActor);

  PParentToChildStreamParent* SendPParentToChildStreamConstructor(
      PParentToChildStreamParent* aActor) override;
  PFileDescriptorSetParent* SendPFileDescriptorSetConstructor(
      const FileDescriptor& aFD) override;

  mozilla::ipc::IPCResult RecvObserveHttpActivity(
      const HttpActivityArgs& aArgs, const uint32_t& aActivityType,
      const uint32_t& aActivitySubtype, const PRTime& aTimestamp,
      const uint64_t& aExtraSizeData, const nsCString& aExtraStringData);

  mozilla::ipc::IPCResult RecvInitBackground(
      Endpoint<PBackgroundParent>&& aEndpoint);

  already_AddRefed<PAltServiceParent> AllocPAltServiceParent();

 private:
  SocketProcessHost* mHost;
  UniquePtr<dom::MemoryReportRequestHost> mMemoryReportRequest;

  static void Destroy(UniquePtr<SocketProcessParent>&& aParent);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessParent_h
