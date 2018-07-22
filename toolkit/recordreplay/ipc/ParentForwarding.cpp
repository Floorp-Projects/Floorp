/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the middleman process uses to forward IPDL
// messages from the recording process to the UI process, and from the UI
// process to either itself, the recording process, or both.

#include "ParentInternal.h"

#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

// Known associations between managee and manager routing IDs.
static StaticInfallibleVector<std::pair<int32_t, int32_t>> gProtocolManagers;

// The routing IDs of actors in the parent process that have been destroyed.
static StaticInfallibleVector<int32_t> gDeadRoutingIds;

static void
NoteProtocolManager(int32_t aManagee, int32_t aManager)
{
  gProtocolManagers.emplaceBack(aManagee, aManager);
  for (auto id : gDeadRoutingIds) {
    if (id == aManager) {
      gDeadRoutingIds.emplaceBack(aManagee);
    }
  }
}

static void
DestroyRoutingId(int32_t aId)
{
  gDeadRoutingIds.emplaceBack(aId);
  for (auto manager : gProtocolManagers) {
    if (manager.second == aId) {
      DestroyRoutingId(manager.first);
    }
  }
}

// Return whether a message from the child process to the UI process is being
// sent to a target that is being destroyed, and should be suppressed.
static bool
MessageTargetIsDead(const IPC::Message& aMessage)
{
  // After the parent process destroys a browser, we handle the destroy in
  // both the middleman and child processes. Both processes will respond to
  // the destroy by sending additional messages to the UI process indicating
  // the browser has been destroyed, but we need to ignore such messages from
  // the child process (if it is still recording) to avoid confusing the UI
  // process.
  for (int32_t id : gDeadRoutingIds) {
    if (id == aMessage.routing_id()) {
      return true;
    }
  }
  return false;
}

static bool
HandleMessageInMiddleman(ipc::Side aSide, const IPC::Message& aMessage)
{
  IPC::Message::msgid_t type = aMessage.type();

  // Ignore messages sent from the child to dead UI process targets.
  if (aSide == ipc::ParentSide) {
    // When the browser is destroyed in the UI process all its children will
    // also be destroyed. Figure out the routing IDs of children which we need
    // to recognize as dead once the browser is destroyed. This is not a
    // complete list of all the browser's children, but only includes ones
    // where crashes have been seen as a result.
    if (type == dom::PBrowser::Msg_PDocAccessibleConstructor__ID) {
      PickleIterator iter(aMessage);
      ipc::ActorHandle handle;

      if (!IPC::ReadParam(&aMessage, &iter, &handle))
        MOZ_CRASH("IPC::ReadParam failed");

      NoteProtocolManager(handle.mId, aMessage.routing_id());
    }

    if (MessageTargetIsDead(aMessage)) {
      PrintSpew("Suppressing %s message to dead target\n", IPC::StringFromIPCMessageType(type));
      return true;
    }
    return false;
  }

  // Handle messages that should be sent to both the middleman and the
  // child process.
  if (// Initialization that must be performed in both processes.
      type == dom::PContent::Msg_PBrowserConstructor__ID ||
      type == dom::PContent::Msg_RegisterChrome__ID ||
      type == dom::PContent::Msg_SetXPCOMProcessAttributes__ID ||
      type == dom::PContent::Msg_SetProcessSandbox__ID ||
      // Graphics messages that affect both processes.
      type == dom::PBrowser::Msg_InitRendering__ID ||
      type == dom::PBrowser::Msg_SetDocShellIsActive__ID ||
      type == dom::PBrowser::Msg_PRenderFrameConstructor__ID ||
      type == dom::PBrowser::Msg_RenderLayers__ID ||
      // May be loading devtools code that runs in the middleman process.
      type == dom::PBrowser::Msg_LoadRemoteScript__ID ||
      // May be sending a message for receipt by devtools code.
      type == dom::PBrowser::Msg_AsyncMessage__ID ||
      // Teardown that must be performed in both processes.
      type == dom::PBrowser::Msg_Destroy__ID) {
    ipc::IProtocol::Result r =
      dom::ContentChild::GetSingleton()->PContentChild::OnMessageReceived(aMessage);
    MOZ_RELEASE_ASSERT(r == ipc::IProtocol::MsgProcessed);
    if (type == dom::PContent::Msg_SetXPCOMProcessAttributes__ID) {
      // Preferences are initialized via the SetXPCOMProcessAttributes message.
      PreferencesLoaded();
    }
    if (type == dom::PBrowser::Msg_Destroy__ID) {
      DestroyRoutingId(aMessage.routing_id());
    }
    if (type == dom::PBrowser::Msg_RenderLayers__ID) {
      // Graphics are being loaded or unloaded for a tab, so update what we are
      // showing to the UI process according to the last paint performed.
      UpdateGraphicsInUIProcess(nullptr);
    }
    return false;
  }

  // Handle messages that should only be sent to the middleman.
  if (// Initialization that should only happen in the middleman.
      type == dom::PContent::Msg_InitRendering__ID ||
      // Record/replay specific messages.
      type == dom::PContent::Msg_SaveRecording__ID ||
      // Teardown that should only happen in the middleman.
      type == dom::PContent::Msg_Shutdown__ID) {
    ipc::IProtocol::Result r = dom::ContentChild::GetSingleton()->PContentChild::OnMessageReceived(aMessage);
    MOZ_RELEASE_ASSERT(r == ipc::IProtocol::MsgProcessed);
    return true;
  }

  // The content process has its own compositor, so compositor messages from
  // the UI process should only be handled in the middleman.
  if (type >= layers::PCompositorBridge::PCompositorBridgeStart &&
      type <= layers::PCompositorBridge::PCompositorBridgeEnd) {
    layers::CompositorBridgeChild* compositorChild = layers::CompositorBridgeChild::Get();
    ipc::IProtocol::Result r = compositorChild->OnMessageReceived(aMessage);
    MOZ_RELEASE_ASSERT(r == ipc::IProtocol::MsgProcessed);
    return true;
  }

  return false;
}

static bool gMainThreadIsWaitingForIPDLReply = false;

bool
MainThreadIsWaitingForIPDLReply()
{
  return gMainThreadIsWaitingForIPDLReply;
}

// Helper for places where the main thread will block while waiting on a
// synchronous IPDL reply from a child process. Incoming messages from the
// child must be handled immediately.
struct MOZ_RAII AutoMarkMainThreadWaitingForIPDLReply
{
  AutoMarkMainThreadWaitingForIPDLReply() {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    MOZ_RELEASE_ASSERT(!gMainThreadIsWaitingForIPDLReply);
    gMainThreadIsWaitingForIPDLReply = true;
  }

  ~AutoMarkMainThreadWaitingForIPDLReply() {
    gMainThreadIsWaitingForIPDLReply = false;
  }
};

class MiddlemanProtocol : public ipc::IToplevelProtocol
{
public:
  ipc::Side mSide;
  MiddlemanProtocol* mOpposite;
  MessageLoop* mOppositeMessageLoop;

  explicit MiddlemanProtocol(ipc::Side aSide)
    : ipc::IToplevelProtocol("MiddlemanProtocol", PContentMsgStart, aSide)
    , mSide(aSide)
    , mOpposite(nullptr)
    , mOppositeMessageLoop(nullptr)
  {}

  virtual void RemoveManagee(int32_t, IProtocol*) override {
    MOZ_CRASH("MiddlemanProtocol::RemoveManagee");
  }

  static void ForwardMessageAsync(MiddlemanProtocol* aProtocol, Message* aMessage) {
    if (ActiveChildIsRecording()) {
      PrintSpew("ForwardAsyncMsg %s %s %d\n",
                (aProtocol->mSide == ipc::ChildSide) ? "Child" : "Parent",
                IPC::StringFromIPCMessageType(aMessage->type()),
                (int) aMessage->routing_id());
      if (!aProtocol->GetIPCChannel()->Send(aMessage)) {
        MOZ_CRASH("MiddlemanProtocol::ForwardMessageAsync");
      }
    } else {
      delete aMessage;
    }
  }

  virtual Result OnMessageReceived(const Message& aMessage) override {
    // If we do not have a recording process then just see if the message can
    // be handled in the middleman.
    if (!mOppositeMessageLoop) {
      MOZ_RELEASE_ASSERT(mSide == ipc::ChildSide);
      HandleMessageInMiddleman(mSide, aMessage);
      return MsgProcessed;
    }

    // Copy the message first, since HandleMessageInMiddleman may destructively
    // modify it through OnMessageReceived calls.
    Message* nMessage = new Message();
    nMessage->CopyFrom(aMessage);

    if (HandleMessageInMiddleman(mSide, aMessage)) {
      delete nMessage;
      return MsgProcessed;
    }

    mOppositeMessageLoop->PostTask(NewRunnableFunction("ForwardMessageAsync", ForwardMessageAsync,
                                                       mOpposite, nMessage));
    return MsgProcessed;
  }

  static void ForwardMessageSync(MiddlemanProtocol* aProtocol, Message* aMessage, Message** aReply) {
    PrintSpew("ForwardSyncMsg %s\n", IPC::StringFromIPCMessageType(aMessage->type()));

    MOZ_RELEASE_ASSERT(!*aReply);
    Message* nReply = new Message();
    if (!aProtocol->GetIPCChannel()->Send(aMessage, nReply)) {
      MOZ_CRASH("MiddlemanProtocol::ForwardMessageSync");
    }

    MonitorAutoLock lock(*gMonitor);
    *aReply = nReply;
    gMonitor->Notify();
  }

  virtual Result OnMessageReceived(const Message& aMessage, Message*& aReply) override {
    MOZ_RELEASE_ASSERT(mOppositeMessageLoop);
    MOZ_RELEASE_ASSERT(mSide == ipc::ChildSide || !MessageTargetIsDead(aMessage));

    Message* nMessage = new Message();
    nMessage->CopyFrom(aMessage);
    mOppositeMessageLoop->PostTask(NewRunnableFunction("ForwardMessageSync", ForwardMessageSync,
                                                       mOpposite, nMessage, &aReply));

    if (mSide == ipc::ChildSide) {
      AutoMarkMainThreadWaitingForIPDLReply blocked;
      ActiveRecordingChild()->WaitUntil([&]() { return !!aReply; });
    } else {
      MonitorAutoLock lock(*gMonitor);
      while (!aReply) {
        gMonitor->Wait();
      }
    }

    PrintSpew("SyncMsgDone\n");
    return MsgProcessed;
  }

  static void ForwardCallMessage(MiddlemanProtocol* aProtocol, Message* aMessage, Message** aReply) {
    PrintSpew("ForwardSyncCall %s\n", IPC::StringFromIPCMessageType(aMessage->type()));

    MOZ_RELEASE_ASSERT(!*aReply);
    Message* nReply = new Message();
    if (!aProtocol->GetIPCChannel()->Call(aMessage, nReply)) {
      MOZ_CRASH("MiddlemanProtocol::ForwardCallMessage");
    }

    MonitorAutoLock lock(*gMonitor);
    *aReply = nReply;
    gMonitor->Notify();
  }

  virtual Result OnCallReceived(const Message& aMessage, Message*& aReply) override {
    MOZ_RELEASE_ASSERT(mOppositeMessageLoop);
    MOZ_RELEASE_ASSERT(mSide == ipc::ChildSide || !MessageTargetIsDead(aMessage));

    Message* nMessage = new Message();
    nMessage->CopyFrom(aMessage);
    mOppositeMessageLoop->PostTask(NewRunnableFunction("ForwardCallMessage", ForwardCallMessage,
                                                       mOpposite, nMessage, &aReply));

    if (mSide == ipc::ChildSide) {
      AutoMarkMainThreadWaitingForIPDLReply blocked;
      ActiveRecordingChild()->WaitUntil([&]() { return !!aReply; });
    } else {
      MonitorAutoLock lock(*gMonitor);
      while (!aReply) {
        gMonitor->Wait();
      }
    }

    PrintSpew("SyncCallDone\n");
    return MsgProcessed;
  }

  virtual int32_t GetProtocolTypeId() override {
    MOZ_CRASH("MiddlemanProtocol::GetProtocolTypeId");
  }

  virtual void OnChannelClose() override {
    MOZ_RELEASE_ASSERT(mSide == ipc::ChildSide);
    MainThreadMessageLoop()->PostTask(NewRunnableFunction("Shutdown", Shutdown));
  }

  virtual void OnChannelError() override {
    MOZ_CRASH("MiddlemanProtocol::OnChannelError");
  }
};

static MiddlemanProtocol* gChildProtocol;
static MiddlemanProtocol* gParentProtocol;

ipc::MessageChannel*
ChannelToUIProcess()
{
  return gChildProtocol->GetIPCChannel();
}

// Message loop for forwarding messages between the parent process and a
// recording process.
static MessageLoop* gForwardingMessageLoop;

static bool gParentProtocolOpened = false;

// Main routine for the forwarding message loop thread.
static void
ForwardingMessageLoopMain(void*)
{
  MOZ_RELEASE_ASSERT(ActiveChildIsRecording());

  MessageLoop messageLoop;
  gForwardingMessageLoop = &messageLoop;

  gChildProtocol->mOppositeMessageLoop = gForwardingMessageLoop;

  gParentProtocol->Open(gRecordingProcess->GetChannel(),
                        base::GetProcId(gRecordingProcess->GetChildProcessHandle()));

  // Notify the main thread that we have finished initialization.
  {
    MonitorAutoLock lock(*gMonitor);
    gParentProtocolOpened = true;
    gMonitor->Notify();
  }

  messageLoop.Run();
}

void
InitializeForwarding()
{
  gChildProtocol = new MiddlemanProtocol(ipc::ChildSide);

  if (gProcessKind == ProcessKind::MiddlemanRecording) {
    gParentProtocol = new MiddlemanProtocol(ipc::ParentSide);
    gParentProtocol->mOpposite = gChildProtocol;
    gChildProtocol->mOpposite = gParentProtocol;

    gParentProtocol->mOppositeMessageLoop = MainThreadMessageLoop();

    if (!PR_CreateThread(PR_USER_THREAD, ForwardingMessageLoopMain, nullptr,
                         PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0)) {
      MOZ_CRASH("parent::Initialize");
    }

    // Wait for the forwarding message loop thread to finish initialization.
    MonitorAutoLock lock(*gMonitor);
    while (!gParentProtocolOpened) {
      gMonitor->Wait();
    }
  }
}

} // namespace parent
} // namespace recordreplay
} // namespace mozilla
