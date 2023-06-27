/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessHost_h
#define mozilla_net_SocketProcessHost_h

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/ipc/TaskFactory.h"

namespace mozilla {

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
class SandboxBroker;
#endif

namespace net {

class SocketProcessParent;

// SocketProcessHost is the "parent process" container for a subprocess handle
// and IPC connection. It owns the parent process IPDL actor, which in this
// case, is a SocketProcessParent.
// SocketProcessHost is allocated and managed by nsIOService in parent process.
class SocketProcessHost final : public mozilla::ipc::GeckoChildProcessHost {
  friend class SocketProcessParent;

 public:
  class Listener {
   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Listener)

    // Called when the process of launching the process is complete.
    virtual void OnProcessLaunchComplete(SocketProcessHost* aHost,
                                         bool aSucceeded) = 0;

    // Called when the channel is closed but Shutdown() is not invoked.
    virtual void OnProcessUnexpectedShutdown(SocketProcessHost* aHost) = 0;

   protected:
    virtual ~Listener() = default;
  };

  explicit SocketProcessHost(Listener* listener);

  // Launch the socket process asynchronously.
  // The OnProcessLaunchComplete listener callback will be invoked
  // either when a connection has been established, or if a connection
  // could not be established due to an asynchronous error.
  bool Launch();

  // Inform the socket process that it should clean up its resources and shut
  // down. This initiates an asynchronous shutdown sequence. After this method
  // returns, it is safe for the caller to forget its pointer to the
  // SocketProcessHost.
  void Shutdown();

  // Return the actor for the top-level actor of the process. Return null if
  // the process is not connected.
  SocketProcessParent* GetActor() const {
    MOZ_ASSERT(NS_IsMainThread());

    return mSocketProcessParent.get();
  }

  bool IsConnected() const {
    MOZ_ASSERT(NS_IsMainThread());

    return !!mSocketProcessParent;
  }

  // Called on the IO thread.
  void OnChannelConnected(base::ProcessId peer_pid) override;

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // Return the sandbox type to be used with this process type.
  static MacSandboxType GetMacSandboxType();
#endif

 private:
  ~SocketProcessHost();

  // Called on the main thread.
  void OnChannelConnectedTask();

  // Called on the main thread after a connection has been established.
  void InitAfterConnect(bool aSucceeded);

  // Called on the main thread when the mSocketParent actor is shutting down.
  void OnChannelClosed();

  void DestroyProcess();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  static bool sLaunchWithMacSandbox;

  // Sandbox the Socket process at launch for all instances
  bool IsMacSandboxLaunchEnabled() override { return sLaunchWithMacSandbox; }

  // Override so we can turn on Socket process-specific sandbox logging
  bool FillMacSandboxInfo(MacSandboxInfo& aInfo) override;
#endif

  DISALLOW_COPY_AND_ASSIGN(SocketProcessHost);

  RefPtr<Listener> mListener;
  mozilla::Maybe<mozilla::ipc::TaskFactory<SocketProcessHost>> mTaskFactory;

  enum class LaunchPhase { Unlaunched, Waiting, Complete };
  LaunchPhase mLaunchPhase;

  RefPtr<SocketProcessParent> mSocketProcessParent;
  // mShutdownRequested is set to true only when Shutdown() is called.
  // If mShutdownRequested is false and the IPC channel is closed,
  // OnProcessUnexpectedShutdown will be invoked.
  bool mShutdownRequested;
  bool mChannelClosed;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  UniquePtr<SandboxBroker> mSandboxBroker;
#endif
};

class SocketProcessMemoryReporter : public MemoryReportingProcess {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SocketProcessMemoryReporter, override)

  SocketProcessMemoryReporter() = default;

  bool IsAlive() const override;

  bool SendRequestMemoryReport(
      const uint32_t& aGeneration, const bool& aAnonymize,
      const bool& aMinimizeMemoryUsage,
      const Maybe<mozilla::ipc::FileDescriptor>& aDMDFile) override;

  int32_t Pid() const override;

 protected:
  virtual ~SocketProcessMemoryReporter() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessHost_h
