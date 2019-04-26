/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessParent_h
#define mozilla_net_SocketProcessParent_h

#include "mozilla/UniquePtr.h"
#include "mozilla/net/PSocketProcessParent.h"

namespace mozilla {

namespace dom {
class MemoryReport;
class MemoryReportRequestHost;
}  // namespace dom

namespace ipc {
class CrashReporterHost;
}  // namespace ipc

namespace net {

class SocketProcessHost;

// IPC actor of socket process in parent process. This is allocated and managed
// by SocketProcessHost.
class SocketProcessParent final : public PSocketProcessParent {
 public:
  friend class SocketProcessHost;

  explicit SocketProcessParent(SocketProcessHost* aHost);
  ~SocketProcessParent();

  static SocketProcessParent* GetSingleton();

  mozilla::ipc::IPCResult RecvInitCrashReporter(
      Shmem&& aShmem, const NativeThreadId& aThreadId);
  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);
  mozilla::ipc::IPCResult RecvFinishMemoryReport(const uint32_t& aGeneration);
  mozilla::ipc::IPCResult RecvAccumulateChildHistograms(
      InfallibleTArray<HistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistograms(
      InfallibleTArray<KeyedHistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvUpdateChildScalars(
      InfallibleTArray<ScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvUpdateChildKeyedScalars(
      InfallibleTArray<KeyedScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvRecordChildEvents(
      nsTArray<ChildEventData>&& events);
  mozilla::ipc::IPCResult RecvRecordDiscardedData(
      const DiscardedData& aDiscardedData);

  PWebrtcProxyChannelParent* AllocPWebrtcProxyChannelParent(
      const TabId& aTabId);
  bool DeallocPWebrtcProxyChannelParent(PWebrtcProxyChannelParent* aActor);

  void ActorDestroy(ActorDestroyReason aWhy) override;
  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<ipc::FileDescriptor>& aDMDFile);

 private:
  SocketProcessHost* mHost;
  UniquePtr<ipc::CrashReporterHost> mCrashReporter;
  UniquePtr<dom::MemoryReportRequestHost> mMemoryReportRequest;

  static void Destroy(UniquePtr<SocketProcessParent>&& aParent);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessParent_h
