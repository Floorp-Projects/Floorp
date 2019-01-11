/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/shared_memory.h"

#include "SocketProcessHost.h"
#include "SocketProcessParent.h"
#include "nsAppRunner.h"

namespace mozilla {
namespace net {

SocketProcessHost::SocketProcessHost(Listener* aListener)
    : GeckoChildProcessHost(GeckoProcessType_Socket),
      mListener(aListener),
      mTaskFactory(this),
      mLaunchPhase(LaunchPhase::Unlaunched),
      mShutdownRequested(false),
      mChannelClosed(false) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(SocketProcessHost);
}

SocketProcessHost::~SocketProcessHost() { MOZ_COUNT_DTOR(SocketProcessHost); }

bool SocketProcessHost::Launch() {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Unlaunched);
  MOZ_ASSERT(!mSocketProcessParent);
  MOZ_ASSERT(NS_IsMainThread());

  std::vector<std::string> extraArgs;

  // TODO: reduce duplicate code about preference below. See bug 1484774.
  // Prefs information is passed via anonymous shared memory to avoid bloating
  // the command line.
  size_t prefMapSize;
  auto prefMapHandle =
      Preferences::EnsureSnapshot(&prefMapSize).ClonePlatformHandle();

  // Serialize the early prefs.
  nsAutoCStringN<1024> prefs;
  Preferences::SerializePreferences(prefs);

  // Set up the shared memory.
  base::SharedMemory shm;
  if (!shm.Create(prefs.Length())) {
    NS_ERROR("failed to create shared memory in the parent");
    return false;
  }
  if (!shm.Map(prefs.Length())) {
    NS_ERROR("failed to map shared memory in the parent");
    return false;
  }

  // Copy the serialized prefs into the shared memory.
  memcpy(static_cast<char*>(shm.memory()), prefs.get(), prefs.Length());

  // Formats a pointer or pointer-sized-integer as a string suitable for passing
  // in an arguments list.
  auto formatPtrArg = [](auto arg) {
    return nsPrintfCString("%zu", uintptr_t(arg));
  };

#if defined(XP_WIN)
  // Record the handle as to-be-shared, and pass it via a command flag. This
  // works because Windows handles are system-wide.
  HANDLE prefsHandle = shm.handle();
  AddHandleToShare(prefsHandle);
  AddHandleToShare(prefMapHandle.get());
  extraArgs.push_back("-prefsHandle");
  extraArgs.push_back(formatPtrArg(prefsHandle).get());
  extraArgs.push_back("-prefMapHandle");
  extraArgs.push_back(formatPtrArg(prefMapHandle.get()).get());
#else
  // In contrast, Unix fds are per-process. So remap the fd to a fixed one that
  // will be used in the child.
  // XXX: bug 1440207 is about improving how fixed fds are used.
  //
  // Note: on Android, AddFdToRemap() sets up the fd to be passed via a Parcel,
  // and the fixed fd isn't used. However, we still need to mark it for
  // remapping so it doesn't get closed in the child.
  AddFdToRemap(shm.handle().fd, kPrefsFileDescriptor);
  AddFdToRemap(prefMapHandle.get(), kPrefMapFileDescriptor);
#endif

  // Pass the lengths via command line flags.
  extraArgs.push_back("-prefsLen");
  extraArgs.push_back(formatPtrArg(prefs.Length()).get());

  extraArgs.push_back("-prefMapSize");
  extraArgs.push_back(formatPtrArg(prefMapSize).get());

  nsAutoCString parentBuildID(mozilla::PlatformBuildID());
  extraArgs.push_back("-parentBuildID");
  extraArgs.push_back(parentBuildID.get());

  mLaunchPhase = LaunchPhase::Waiting;
  if (!GeckoChildProcessHost::LaunchAndWaitForProcessHandle(extraArgs)) {
    mLaunchPhase = LaunchPhase::Complete;
    return false;
  }

  return true;
}

void SocketProcessHost::OnChannelConnected(int32_t peer_pid) {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelConnected(peer_pid);

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable = mTaskFactory.NewRunnableMethod(
        &SocketProcessHost::OnChannelConnectedTask);
  }
  NS_DispatchToMainThread(runnable);
}

void SocketProcessHost::OnChannelError() {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelError();

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable =
        mTaskFactory.NewRunnableMethod(&SocketProcessHost::OnChannelErrorTask);
  }
  NS_DispatchToMainThread(runnable);
}

void SocketProcessHost::OnChannelConnectedTask() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(true);
  }
}

void SocketProcessHost::OnChannelErrorTask() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(false);
  }
}

void SocketProcessHost::InitAfterConnect(bool aSucceeded) {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mSocketProcessParent);
  MOZ_ASSERT(NS_IsMainThread());

  mLaunchPhase = LaunchPhase::Complete;

  if (aSucceeded) {
    mSocketProcessParent = MakeUnique<SocketProcessParent>(this);
    DebugOnly<bool> rv = mSocketProcessParent->Open(
        GetChannel(), base::GetProcId(GetChildProcessHandle()));
    MOZ_ASSERT(rv);
  }

  if (mListener) {
    mListener->OnProcessLaunchComplete(this, aSucceeded);
  }
}

void SocketProcessHost::Shutdown() {
  MOZ_ASSERT(!mShutdownRequested);
  MOZ_ASSERT(NS_IsMainThread());

  mListener = nullptr;

  if (mSocketProcessParent) {
    // OnChannelClosed uses this to check if the shutdown was expected or
    // unexpected.
    mShutdownRequested = true;

    // The channel might already be closed if we got here unexpectedly.
    if (!mChannelClosed) {
      mSocketProcessParent->Close();
    }

    return;
  }

  DestroyProcess();
}

void SocketProcessHost::OnChannelClosed() {
  MOZ_ASSERT(NS_IsMainThread());

  mChannelClosed = true;

  if (!mShutdownRequested && mListener) {
    // This is an unclean shutdown. Notify our listener that we're going away.
    mListener->OnProcessUnexpectedShutdown(this);
  } else {
    DestroyProcess();
  }

  // Release the actor.
  SocketProcessParent::Destroy(std::move(mSocketProcessParent));
  MOZ_ASSERT(!mSocketProcessParent);
}

static void DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess) {
  XRE_GetIOMessageLoop()->PostTask(
      MakeAndAddRef<DeleteTask<GeckoChildProcessHost>>(aSubprocess));
}

void SocketProcessHost::DestroyProcess() {
  {
    MonitorAutoLock lock(mMonitor);
    mTaskFactory.RevokeAll();
  }

  MessageLoop::current()->PostTask(NewRunnableFunction(
      "DestroySocketProcessRunnable", DelayedDeleteSubprocess, this));
}

}  // namespace net
}  // namespace mozilla
