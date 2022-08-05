/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCFuzzController.h"
#include "mozilla/Fuzzing.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/SyncRunnable.h"

#include "nsIThread.h"
#include "nsThreadUtils.h"

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/MessageLink.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/NodeChannel.h"
#include "mozilla/ipc/NodeController.h"

#include "mozilla/ipc/PIdleScheduler.h"
#include "mozilla/ipc/PBackground.h"
#include "mozilla/dom/PContent.h"

using namespace mojo::core::ports;
using namespace mozilla::ipc;

// Sync inject means that the actual fuzzing takes place on the I/O thread
// and hence it injects directly into the target NodeChannel. In async mode,
// we run the fuzzing on a separate thread and dispatch the runnable that
// injects the message back to the I/O thread. Both approaches seem to work
// and have advantages and disadvantages. Blocking the I/O thread means no
// IPC between other processes will interfere with our fuzzing in the meantime
// but blocking could also cause hangs when such IPC is required during the
// fuzzing runtime for some reason.
//#define MOZ_FUZZ_IPC_SYNC_INJECT 1

// For debugging purposes, it can be helpful to synchronize after each message
// rather than after each iteration, to see which messages are particularly
// slow or cause a hang. Without this, synchronization will occur at the end
// of each iteration as well as after each constructor message.
//#define MOZ_FUZZ_IPC_SYNC_AFTER_EACH_MSG

namespace mozilla {
namespace fuzzing {

IPCFuzzController::IPCFuzzController() : mMutex("IPCFuzzController") {
  InitializeIPCTypes();

  // We use 6 bits for port index selection without wrapping, so we just
  // create 64 empty rows in our port matrix. Not all of these rows will
  // be used though.
  portNames.resize(64);

  // This is our port / toplevel actor ordering. Add new toplevel actors
  // here to support them in the fuzzer. Do *NOT* change the order of
  // these, as it will invalidate our fuzzing corpus.
  portNameToIndex["PContent"] = 0;
  portNameToIndex["PBackground"] = 1;
  portNameToIndex["PBackgroundStarter"] = 2;
  portNameToIndex["PCompositorManager"] = 3;
  portNameToIndex["PImageBridge"] = 4;
  portNameToIndex["PProcessHangMonitor"] = 5;
  portNameToIndex["PProfiler"] = 6;
  portNameToIndex["PVRManager"] = 7;
  portNameToIndex["PCanvasManager"] = 8;
}

// static
IPCFuzzController& IPCFuzzController::instance() {
  static IPCFuzzController ifc;
  return ifc;
}

void IPCFuzzController::InitializeIPCTypes() {
  const char* cons = "Constructor";
  size_t cons_len = strlen(cons);

  for (uint32_t start = 0; start < LastMsgIndex; ++start) {
    uint32_t i;
    for (i = (start << 16) + 1; i < ((start + 1) << 16); ++i) {
      const char* name = IPC::StringFromIPCMessageType(i);

      if (name[0] == '<') break;

      size_t len = strlen(name);
      if (len > cons_len && !memcmp(cons, name + len - cons_len, cons_len)) {
        constructorTypes.insert(i);
      }
    }

    validMsgTypes[(ProtocolId)start] = i - ((start << 16) + 1);
  }
}

bool IPCFuzzController::GetRandomIPCMessageType(ProtocolId pId,
                                                uint16_t typeOffset,
                                                uint32_t* type) {
  auto pIdEntry = validMsgTypes.find(pId);
  if (pIdEntry == validMsgTypes.end()) {
    return false;
  }

  *type =
      ((uint32_t)pIdEntry->first << 16) + 1 + (typeOffset % pIdEntry->second);
  return true;
}

void IPCFuzzController::OnActorConnected(IProtocol* protocol) {
  if (!XRE_IsParentProcess()) {
    return;
  }

#ifdef FUZZ_DEBUG
  MOZ_FUZZING_NYX_PRINTF("INFO: [OnActorConnected] ActorID %d Protocol: %s\n",
                         protocol->Id(), protocol->GetProtocolName());
#endif

  MessageChannel* channel = protocol->ToplevelProtocol()->GetIPCChannel();

  Maybe<PortName> portName = channel->GetPortName();
  if (portName) {
    MOZ_FUZZING_NYX_DEBUG(
        "DEBUG: IPCFuzzController::OnActorConnected() Mutex try\n");
    // Called on background threads and modifies `actorIds`.
    MutexAutoLock lock(mMutex);
    MOZ_FUZZING_NYX_DEBUG(
        "DEBUG: IPCFuzzController::OnActorConnected() Mutex locked\n");
    actorIds[*portName].emplace_back(protocol->Id(), protocol->GetProtocolId());

    // Fix the port we will be using for at least the next 5 messages
    useLastPortName = true;
    lastActorPortName = *portName;

    // Use this actor for the next 5 messages
    useLastActor = 5;
  } else {
    MOZ_FUZZING_NYX_PRINT("WARNING: No port name on actor?!\n");
  }
}

void IPCFuzzController::OnActorDestroyed(IProtocol* protocol) {
  if (!XRE_IsParentProcess()) {
    return;
  }

  MOZ_FUZZING_NYX_PRINTF("INFO: [OnActorDestroyed] ActorID %d Protocol: %s\n",
                         protocol->Id(), protocol->GetProtocolName());

  MessageChannel* channel = protocol->ToplevelProtocol()->GetIPCChannel();

  Maybe<PortName> portName = channel->GetPortName();
  if (portName) {
    MOZ_FUZZING_NYX_DEBUG(
        "DEBUG: IPCFuzzController::OnActorDestroyed() Mutex try\n");
    // Called on background threads and modifies `actorIds`.
    MutexAutoLock lock(mMutex);
    MOZ_FUZZING_NYX_DEBUG(
        "DEBUG: IPCFuzzController::OnActorDestroyed() Mutex locked\n");

    for (auto iter = actorIds[*portName].begin();
         iter != actorIds[*portName].end();) {
      if (iter->first == protocol->Id() &&
          iter->second == protocol->GetProtocolId()) {
        iter = actorIds[*portName].erase(iter);
      } else {
        ++iter;
      }
    }
  } else {
    MOZ_FUZZING_NYX_PRINT("WARNING: No port name on destroyed actor?!\n");
  }
}

void IPCFuzzController::AddToplevelActor(PortName name, ProtocolId protocolId) {
  const char* protocolName = ProtocolIdToName(protocolId);
  auto result = portNameToIndex.find(protocolName);
  if (result == portNameToIndex.end()) {
    MOZ_FUZZING_NYX_PRINTF(
        "ERROR: [OnActorConnected] Unknown Top-Level Protocol: %s\n",
        protocolName);
    MOZ_REALLY_CRASH(__LINE__);
  }
  uint8_t portIndex = result->second;
  portNames[portIndex].push_back(name);
}

bool IPCFuzzController::ObserveIPCMessage(mozilla::ipc::NodeChannel* channel,
                                          IPC::Message& aMessage) {
  if (!mozilla::fuzzing::Nyx::instance().is_enabled("IPC_Generic")) {
    // Fuzzer is not enabled.
    return true;
  }

  if (!XRE_IsParentProcess()) {
    // For now we only care about things in the parent process.
    return true;
  }

  if (aMessage.IsFuzzMsg()) {
    // Don't observe our own messages. If this is the first fuzzing message,
    // we also block further non-fuzzing communication on that node.
    if (!channel->mBlockSendRecv) {
      MOZ_FUZZING_NYX_PRINTF(
          "INFO: [NodeChannel::OnMessageReceived] Blocking further "
          "communication on Port %lu %lu (seen fuzz msg)\n",
          channel->GetName().v1, channel->GetName().v2);
      channel->mBlockSendRecv = true;
    }
    return true;
  } else if (aMessage.type() == dom::PContent::Msg_SignalFuzzingReady__ID) {
    MOZ_FUZZING_NYX_PRINT("DEBUG: Ready message detected.\n");

    // TODO: This is specific to PContent fuzzing. If we later want to fuzz
    // a different process pair, we need additional signals here.
    OnChildReady();

    // The ready message indicates the right node name for us to work with
    // and we should only ever receive it once.
    if (haveTargetNodeName) {
      MOZ_FUZZING_NYX_PRINT("ERROR: Received ready signal twice?!\n");
      MOZ_REALLY_CRASH(__LINE__);
    }

    targetNodeName = channel->GetName();
    haveTargetNodeName = true;

    // We can also use this message as the base template for other messages
    if (!this->sampleHeader.initLengthUninitialized(
            sizeof(IPC::Message::Header))) {
      MOZ_REALLY_CRASH(__LINE__);
    }

    memcpy(sampleHeader.begin(), aMessage.header(),
           sizeof(IPC::Message::Header));
  } else if (haveTargetNodeName && targetNodeName != channel->GetName()) {
    // Not our node, no need to observe
    return true;
  } else if (Nyx::instance().started()) {
    // When fuzzing is already started, we shouldn't observe messages anymore.
    if (!channel->mBlockSendRecv) {
      MOZ_FUZZING_NYX_PRINTF(
          "INFO: [NodeChannel::OnMessageReceived] Blocking further "
          "communication on Port %lu %lu (fuzzing started)\n",
          channel->GetName().v1, channel->GetName().v2);
      channel->mBlockSendRecv = true;
    }
    return false;
  }

  Vector<char, 256, InfallibleAllocPolicy> footer;

  if (!footer.initLengthUninitialized(aMessage.event_footer_size())) {
    MOZ_REALLY_CRASH(__LINE__);
  }

  if (!aMessage.ReadFooter(footer.begin(), footer.length(), false)) {
    MOZ_FUZZING_NYX_PRINT("ERROR: ReadFooter() failed?!\n");
    MOZ_REALLY_CRASH(__LINE__);
  }

  UniquePtr<Event> event = Event::Deserialize(footer.begin(), footer.length());

  if (!event) {
    MOZ_FUZZING_NYX_PRINT("ERROR: Failed to deserialize observed message?!\n");
    MOZ_REALLY_CRASH(__LINE__);
  }

  if (event->type() == Event::kUserMessage) {
    if (haveTargetNodeName && !fuzzingStartPending) {
      bool missingActor = false;

      // Check if we have any entries in our port map that we haven't seen yet
      // though `OnActorConnected`. That method is called on a background
      // thread and this call will race with the I/O thread.
      {
        MOZ_FUZZING_NYX_DEBUG(
            "DEBUG: IPCFuzzController::ObserveIPCMessage() Mutex try\n");
        // Called on the I/O thread and reads `portSeqNos`.
        //
        // IMPORTANT: We must give up any locks before entering `StartFuzzing`,
        // as we will never return. This would cause a deadlock with new actors
        // being created and `OnActorConnected` being called.
        MutexAutoLock lock(mMutex);

        MOZ_FUZZING_NYX_DEBUG(
            "DEBUG: IPCFuzzController::ObserveIPCMessage() Mutex locked\n");

        for (auto iter = portSeqNos.begin(); iter != portSeqNos.end(); ++iter) {
          auto result = actorIds.find(iter->first);
          if (result == actorIds.end()) {
            // Make sure we only wait for actors that belong to us.
            auto result = portNodeName.find(iter->first);
            if (result->second == targetNodeName) {
              missingActor = true;
              break;
            }
          }
        }
      }

      if (missingActor) {
        MOZ_FUZZING_NYX_PRINT(
            "INFO: Delaying fuzzing start, missing actors...\n");
      } else if (!childReady) {
        MOZ_FUZZING_NYX_PRINT(
            "INFO: Delaying fuzzing start, waiting for child...\n");
      } else {
        fuzzingStartPending = true;
        StartFuzzing(channel, aMessage);

        // In the async case, we return and can already block the relevant
        // communication.
        if (targetNodeName == channel->GetName()) {
          if (!channel->mBlockSendRecv) {
            MOZ_FUZZING_NYX_PRINTF(
                "INFO: [NodeChannel::OnMessageReceived] Blocking further "
                "communication on Port %lu %lu (fuzzing start pending)\n",
                channel->GetName().v1, channel->GetName().v2);
            channel->mBlockSendRecv = true;
          }

          return false;
        }
        return true;
      }
    }

    // Add/update sequence numbers. We need to make sure to do this after our
    // call to `StartFuzzing` because once we start fuzzing, the message will
    // never actually be processed, so we run into a sequence number desync.
    {
      // Get the port name associated with this message
      UserMessageEvent* userMsgEv = static_cast<UserMessageEvent*>(event.get());
      PortName name = event->port_name();

      // Called on the I/O thread and modifies `portSeqNos`.
      MutexAutoLock lock(mMutex);
      portSeqNos.insert_or_assign(
          name, std::pair<int32_t, uint64_t>(aMessage.seqno(),
                                             userMsgEv->sequence_num()));

      portNodeName.insert_or_assign(name, channel->GetName());
    }
  }

  return true;
}

bool IPCFuzzController::MakeTargetDecision(
    uint8_t portIndex, uint8_t portInstanceIndex, uint8_t actorIndex,
    uint16_t typeOffset, PortName* name, int32_t* seqno, uint64_t* fseqno,
    int32_t* actorId, uint32_t* type, bool* is_cons, bool update) {
  // Every possible toplevel actor type has a fixed number that
  // we assign to it in the constructor of this class. Here, we
  // use the lower 6 bits to select this toplevel actor type.
  // This approach has the advantage that the tests will always
  // select the same toplevel actor type deterministically,
  // independent of the order they appeared and independent
  // of the type of fuzzing we are doing.
  auto portInstances = portNames[portIndex & 0x3f];
  if (!portInstances.size()) {
    return false;
  }

  if (useLastActor) {
    useLastActor--;
    *name = lastActorPortName;

    MOZ_FUZZING_NYX_PRINT("DEBUG: MakeTargetDecision: Pinned to last actor.\n");

    // Once we stop pinning to the last actor, we need to decide if we
    // want to keep the pinning on the port itself. We use one of the
    // unused upper bits of portIndex for this purpose.
    if (!useLastActor && (portIndex & (1 << 7))) {
      MOZ_FUZZING_NYX_PRINT(
          "DEBUG: MakeTargetDecision: Released pinning on last port.\n");
      useLastPortName = false;
    }
  } else if (useLastPortName) {
    *name = lastActorPortName;
    MOZ_FUZZING_NYX_PRINT("DEBUG: MakeTargetDecision: Pinned to last port.\n");
  } else {
    *name = portInstances[portInstanceIndex % portInstances.size()];
  }

  // We should always have at least one actor per port
  auto result = actorIds.find(*name);
  if (result == actorIds.end()) {
    MOZ_FUZZING_NYX_PRINT("ERROR: Couldn't find port in actors map?!\n");
    return false;
  }

  // Find a random actor on this port
  auto actors = result->second;
  if (actors.empty()) {
    MOZ_FUZZING_NYX_PRINT(
        "ERROR: Couldn't find an actor for selected port?!\n");
    return false;
  }

  auto seqNos = portSeqNos[*name];

  // Hand out the correct sequence numbers
  *seqno = seqNos.first - 1;
  *fseqno = seqNos.second + 1;

  if (update) {
    portSeqNos.insert_or_assign(*name,
                                std::pair<int32_t, uint64_t>(*seqno, *fseqno));
  }

  if (useLastActor) {
    actorIndex = actors.size() - 1;
  } else {
    actorIndex %= actors.size();
  }

  ActorIdPair ids = actors[actorIndex];
  *actorId = ids.first;

  // If the actor ID is 0, then we are talking to the toplevel actor
  // of this port. Hence we must set the ID to MSG_ROUTING_CONTROL.
  if (!*actorId) {
    *actorId = MSG_ROUTING_CONTROL;
  }

  if (!this->GetRandomIPCMessageType(ids.second, typeOffset, type)) {
    MOZ_FUZZING_NYX_PRINT("ERROR: GetRandomIPCMessageType failed?!\n");
    return false;
  }

  *is_cons = false;
  if (constructorTypes.find(*type) != constructorTypes.end()) {
    *is_cons = true;
  }

#ifdef FUZZ_DEBUG
  MOZ_FUZZING_NYX_PRINTF(
      "DEBUG: MakeTargetDecision: Protocol: %s msgType: %s\n",
      ProtocolIdToName(ids.second), IPC::StringFromIPCMessageType(*type));
#endif

  return true;
}

void IPCFuzzController::OnMessageTaskStart() { messageStartCount++; }

void IPCFuzzController::OnMessageTaskStop() { messageStopCount++; }

void IPCFuzzController::OnPreFuzzMessageTaskRun() { messageTaskCount++; }
void IPCFuzzController::OnPreFuzzMessageTaskStop() { messageTaskCount--; }

void IPCFuzzController::OnDropPeer(const char* reason = nullptr,
                                   const char* file = nullptr, int line = 0) {
  if (!XRE_IsParentProcess()) {
    return;
  }

  if (!Nyx::instance().started()) {
    // It's possible to close a connection to some peer before we have even
    // started fuzzing. We ignore these events until we are actually fuzzing.
    return;
  }

  MOZ_FUZZING_NYX_PRINT(
      "ERROR: ======== END OF ITERATION (DROP_PEER) ========\n");
#ifdef FUZZ_DEBUG
  MOZ_FUZZING_NYX_PRINTF("DEBUG: ======== %s:%d ========\n", file, line);
#endif
  Nyx::instance().handle_event("MOZ_IPC_DROP_PEER", file, line, reason);

  if (Nyx::instance().is_replay()) {
    // In replay mode, let's ignore drop peer to avoid races with it.
    return;
  }

  Nyx::instance().release(IPCFuzzController::instance().getMessageStopCount());
}

void IPCFuzzController::StartFuzzing(mozilla::ipc::NodeChannel* channel,
                                     IPC::Message& aMessage) {
  nodeChannel = channel;

  RefPtr<IPCFuzzLoop> runnable = new IPCFuzzLoop();

#if MOZ_FUZZ_IPC_SYNC_INJECT
  runnable->Run();
#else
  nsCOMPtr<nsIThread> newThread;
  nsresult rv =
      NS_NewNamedThread("IPCFuzzLoop", getter_AddRefs(newThread), runnable);

  if (NS_FAILED(rv)) {
    MOZ_FUZZING_NYX_PRINT("ERROR: [StartFuzzing] NS_NewNamedThread failed?!\n");
    MOZ_REALLY_CRASH(__LINE__);
  }
#endif
}

IPCFuzzController::IPCFuzzLoop::IPCFuzzLoop()
    : mozilla::Runnable("IPCFuzzLoop") {}

NS_IMETHODIMP IPCFuzzController::IPCFuzzLoop::Run() {
  MOZ_FUZZING_NYX_DEBUG("DEBUG: BEGIN IPCFuzzLoop::Run()\n");

  const size_t maxMsgSize = 2048;
  const size_t controlLen = 16;

  Vector<char, 256, InfallibleAllocPolicy> buffer;

  RefPtr<NodeController> controller = NodeController::GetSingleton();

  // TODO: The following code is full of data races. We need synchronization
  // on the `IPCFuzzController` instance, because the I/O thread can call into
  // this class via ObserveIPCMessages. The problem is that any such call
  // must either be observed to update the sequence numbers, or the packet
  // must be dropped already.
  if (!IPCFuzzController::instance().haveTargetNodeName) {
    MOZ_FUZZING_NYX_PRINT("ERROR: I don't have the target NodeName?!\n");
    MOZ_REALLY_CRASH(__LINE__);
  }

  {
    MOZ_FUZZING_NYX_DEBUG("DEBUG: IPCFuzzLoop::Run() Mutex try\n");
    // Called on the I/O thread and modifies `portSeqNos` and `actorIds`.
    MutexAutoLock lock(IPCFuzzController::instance().mMutex);
    MOZ_FUZZING_NYX_DEBUG("DEBUG: IPCFuzzLoop::Run() Mutex locked\n");

    // The wait/delay logic in ObserveIPCMessage should ensure that we haven't
    // seen any packets on ports for which we haven't received actor information
    // yet, if those ports belong to our channel. However, we might also have
    // seen ports not belonging to our channel, which we have to remove now.
    for (auto iter = IPCFuzzController::instance().portSeqNos.begin();
         iter != IPCFuzzController::instance().portSeqNos.end();) {
      auto result = IPCFuzzController::instance().actorIds.find(iter->first);
      if (result == IPCFuzzController::instance().actorIds.end()) {
        auto portNameResult =
            IPCFuzzController::instance().portNodeName.find(iter->first);
        if (portNameResult->second ==
            IPCFuzzController::instance().targetNodeName) {
          MOZ_FUZZING_NYX_PRINT(
              "ERROR: We should not have port map entries without a "
              "corresponding "
              "entry in our actors map\n");
          MOZ_REALLY_CRASH(__LINE__);
        } else {
          iter = IPCFuzzController::instance().portSeqNos.erase(iter);
        }
      } else {
        ++iter;
      }
    }

    // TODO: Technically, at this point we only know that PContent (or whatever
    // toplevel protocol we decided to synchronize on), is present. It might
    // be possible that others aren't created yet and we are racing on this.
    //
    // Note: The delay logic mentioned above makes this less likely. Only actors
    // which are created on-demand and which have not been referenced yet at all
    // would be affected by such a race.
    for (auto iter = IPCFuzzController::instance().actorIds.begin();
         iter != IPCFuzzController::instance().actorIds.end(); ++iter) {
      bool isValidTarget = false;
      Maybe<PortStatus> status;
      PortRef ref = controller->GetPort(iter->first);
      if (ref.is_valid()) {
        status = controller->GetStatus(ref);
        if (status) {
          isValidTarget = status->peer_node_name ==
                          IPCFuzzController::instance().targetNodeName;
        }
      }

      auto result = IPCFuzzController::instance().portSeqNos.find(iter->first);
      if (result == IPCFuzzController::instance().portSeqNos.end()) {
        if (isValidTarget) {
          MOZ_FUZZING_NYX_PRINTF(
              "INFO: Using Port %lu %lu for protocol %s (*)\n", iter->first.v1,
              iter->first.v2, ProtocolIdToName(iter->second[0].second));

          // Normally the start sequence numbers would be -1 and 1, but our map
          // does not record the next numbers, but the "last seen" state. So we
          // have to adjust these so the next calculated sequence number pair
          // matches the start sequence numbers.
          IPCFuzzController::instance().portSeqNos.insert_or_assign(
              iter->first, std::pair<int32_t, uint64_t>(0, 0));

          IPCFuzzController::instance().AddToplevelActor(
              iter->first, iter->second[0].second);

        } else {
          MOZ_FUZZING_NYX_PRINTF(
              "INFO: Removing Port %lu %lu for protocol %s (*)\n",
              iter->first.v1, iter->first.v2,
              ProtocolIdToName(iter->second[0].second));

          // This toplevel actor does not belong to us, but we haven't added
          // it to `portSeqNos`, so we don't have to remove it.
        }
      } else {
        if (isValidTarget) {
          MOZ_FUZZING_NYX_PRINTF("INFO: Using Port %lu %lu for protocol %s\n",
                                 iter->first.v1, iter->first.v2,
                                 ProtocolIdToName(iter->second[0].second));

          IPCFuzzController::instance().AddToplevelActor(
              iter->first, iter->second[0].second);
        } else {
          MOZ_FUZZING_NYX_PRINTF(
              "INFO: Removing Port %lu %lu for protocol %s\n", iter->first.v1,
              iter->first.v2, ProtocolIdToName(iter->second[0].second));

          // This toplevel actor does not belong to us, so remove it.
          IPCFuzzController::instance().portSeqNos.erase(result);
        }
      }
    }
  }

  IPCFuzzController::instance().runnableDone = false;

  SyncRunnable::DispatchToThread(
      GetMainThreadEventTarget(),
      new SyncRunnable(NS_NewRunnableFunction(
          "IPCFuzzController::StartFuzzing", [&]() -> void {
            MOZ_FUZZING_NYX_PRINT("INFO: Main thread runnable start.\n");
            NS_ProcessPendingEvents(NS_GetCurrentThread());
            MOZ_FUZZING_NYX_PRINT("INFO: Main thread runnable done.\n");
          })));

  MOZ_FUZZING_NYX_PRINT("INFO: Performing snapshot...\n");
  Nyx::instance().start();

  uint32_t expected_messages = 0;

  IPCFuzzController::instance().useLastActor = 0;
  IPCFuzzController::instance().useLastPortName = false;

  for (int i = 0; i < 16; ++i) {
    if (!buffer.initLengthUninitialized(maxMsgSize)) {
      MOZ_REALLY_CRASH(__LINE__);
    }

    // Grab enough data to potentially fill our everything except the footer.
    uint32_t bufsize =
        Nyx::instance().get_data((uint8_t*)buffer.begin(), buffer.length());

    if (bufsize == 0xFFFFFFFF) {
      // Done constructing
      MOZ_FUZZING_NYX_DEBUG("Iteration complete: Out of data.\n");
      break;
    }

    // Payload must be int aligned
    bufsize -= bufsize % 4;

    // Need at least a header and the control bytes.
    if (bufsize < sizeof(IPC::Message::Header) + controlLen) {
      MOZ_FUZZING_NYX_DEBUG("INFO: Not enough data to craft IPC message.\n");
      continue;
    }

    buffer.shrinkTo(bufsize);

    const uint8_t* controlData = (uint8_t*)buffer.begin();

    char* ipcMsgData = buffer.begin() + controlLen;
    size_t ipcMsgLen = buffer.length() - controlLen;

    // Copy the header of the original message
    memcpy(ipcMsgData, IPCFuzzController::instance().sampleHeader.begin(),
           sizeof(IPC::Message::Header));

    IPC::Message::Header* ipchdr = (IPC::Message::Header*)ipcMsgData;

    ipchdr->payload_size = ipcMsgLen - sizeof(IPC::Message::Header);

    PortName new_port_name;
    int32_t new_seqno;
    uint64_t new_fseqno;

    int32_t actorId;
    uint32_t msgType;
    bool isConstructor;
    // Control Data Layout (16 byte)
    // Byte  0 - Port Index (selects out of the valid ports seen)
    // Byte  1 - Actor Index (selects one of the actors for that port)
    // Byte  2 - Type Offset (select valid type for the specified actor)
    // Byte  3 -  ^- continued
    // Byte  4 - Sync Bit
    // Byte  5 - Optionally select a particular instance of the selected
    //           port type. Some toplevel protocols can have multiple
    //           instances running at the same time.

    uint8_t portIndex = controlData[0];
    uint8_t actorIndex = controlData[1];
    uint16_t typeOffset = *(uint16_t*)(&controlData[2]);
    bool isSync = controlData[4] > 127;
    uint8_t portInstanceIndex = controlData[5];

    if (!IPCFuzzController::instance().MakeTargetDecision(
            portIndex, portInstanceIndex, actorIndex, typeOffset,
            &new_port_name, &new_seqno, &new_fseqno, &actorId, &msgType,
            &isConstructor)) {
      MOZ_FUZZING_NYX_DEBUG("DEBUG: MakeTargetDecision returned false.\n");
      continue;
    }

    if (Nyx::instance().is_replay()) {
      MOZ_FUZZING_NYX_PRINT("INFO: Replaying IPC packet with payload:\n");
      for (uint32_t i = 0; i < ipcMsgLen - sizeof(IPC::Message::Header); ++i) {
        if (i % 16 == 0) {
          MOZ_FUZZING_NYX_PRINT("\n  ");
        }

        MOZ_FUZZING_NYX_PRINTF(
            "0x%02X ",
            (unsigned char)(ipcMsgData[sizeof(IPC::Message::Header) + i]));
      }
      MOZ_FUZZING_NYX_PRINT("\n");
    }

    UniquePtr<IPC::Message> msg(new IPC::Message(ipcMsgData, ipcMsgLen));

    if (isConstructor) {
      MOZ_FUZZING_NYX_DEBUG("DEBUG: Sending constructor message...\n");
      msg->header()->flags.SetConstructor();
    }

    if (!isConstructor && isSync) {
      MOZ_FUZZING_NYX_DEBUG("INFO: Sending sync message...\n");
      msg->header()->flags.SetSync();
    }

    msg->set_seqno(new_seqno);
    msg->set_routing_id(actorId);

    // TODO: There is no setter for this.
    msg->header()->type = msgType;

    // Create the footer
    auto messageEvent = MakeUnique<UserMessageEvent>(0);
    messageEvent->set_port_name(new_port_name);
    messageEvent->set_sequence_num(new_fseqno);

    Vector<char, 256, InfallibleAllocPolicy> footerBuffer;
    (void)footerBuffer.initLengthUninitialized(
        messageEvent->GetSerializedSize());
    messageEvent->Serialize(footerBuffer.begin());

    msg->WriteFooter(footerBuffer.begin(), footerBuffer.length());
    msg->set_event_footer_size(footerBuffer.length());

    // This marks the message as a fuzzing message. Without this, it will
    // be ignored by MessageTask and also not even scheduled by NodeChannel
    // in asynchronous mode. We use this to ignore any IPC activity that
    // happens just while we are fuzzing.
    msg->SetFuzzMsg();

#ifdef FUZZ_DEBUG
    MOZ_FUZZING_NYX_PRINTF(
        "DEBUG: OnEventMessage iteration %d, EVS: %u Payload: %u.\n", i,
        ipchdr->event_footer_size, ipchdr->payload_size);
#endif

#ifdef FUZZ_DEBUG
    MOZ_FUZZING_NYX_PRINTF("DEBUG: OnEventMessage: Port %lu %lu. Actor %d\n",
                           new_port_name.v1, new_port_name.v2, actorId);
    MOZ_FUZZING_NYX_PRINTF(
        "DEBUG: OnEventMessage: Flags: %u TxID: %d Handles: %u\n",
        msg->header()->flags, msg->header()->txid, msg->header()->num_handles);
#endif

    // The number of messages we expect to see stopped.
    expected_messages++;

#if MOZ_FUZZ_IPC_SYNC_INJECT
    // For synchronous injection, we just call OnMessageReceived directly.
    IPCFuzzController::instance().nodeChannel->OnMessageReceived(
        std::move(msg));
#else
    // For asynchronous injection, we have to post to the I/O thread instead.
    XRE_GetIOMessageLoop()->PostTask(NS_NewRunnableFunction(
        "NodeChannel::OnMessageReceived",
        [msg = std::move(msg),
         nodeChannel =
             RefPtr{IPCFuzzController::instance().nodeChannel}]() mutable {
          nodeChannel->OnMessageReceived(std::move(msg));
        }));
#endif

#ifdef MOZ_FUZZ_IPC_SYNC_AFTER_EACH_MSG
    MOZ_FUZZING_NYX_DEBUG("DEBUG: Synchronizing after message...\n");
    IPCFuzzController::instance().SynchronizeOnMessageExecution(
        expected_messages);
#else

    if (isConstructor) {
      MOZ_FUZZING_NYX_DEBUG(
          "DEBUG: Synchronizing due to constructor message...\n");
      IPCFuzzController::instance().SynchronizeOnMessageExecution(
          expected_messages);
    }
#endif
  }

  MOZ_FUZZING_NYX_DEBUG("DEBUG: Synchronizing due to end of iteration...\n");
  IPCFuzzController::instance().SynchronizeOnMessageExecution(
      expected_messages);

  MOZ_FUZZING_NYX_DEBUG(
      "DEBUG: ======== END OF ITERATION (RELEASE) ========\n");

  Nyx::instance().release(IPCFuzzController::instance().getMessageStopCount());

  // Never reached.
  return NS_OK;
}

void IPCFuzzController::SynchronizeOnMessageExecution(
    uint32_t expected_messages) {
  // This synchronization will work in both the sync and async case.
  // For the async case, it is important to wait for the exact stop count
  // because the message task is not even started potentially when we
  // read this loop.
  int hang_timeout = 10 * 1000;
  while (IPCFuzzController::instance().getMessageStopCount() !=
         expected_messages) {
#ifdef FUZZ_DEBUG
    uint32_t count_stopped =
        IPCFuzzController::instance().getMessageStopCount();
    uint32_t count_live = IPCFuzzController::instance().getMessageStartCount();
    MOZ_FUZZING_NYX_PRINTF(
        "DEBUG: Post Constructor: %d stopped messages (%d live, %d "
        "expected)!\n",
        count_stopped, count_live, expected_messages);
#endif
    PR_Sleep(PR_MillisecondsToInterval(50));
    hang_timeout -= 50;

    if (hang_timeout <= 0) {
      Nyx::instance().handle_event("MOZ_TIMEOUT", nullptr, 0, nullptr);
      MOZ_FUZZING_NYX_PRINT(
          "ERROR: ======== END OF ITERATION (TIMEOUT) ========\n");
      Nyx::instance().release(
          IPCFuzzController::instance().getMessageStopCount());
    }
  }
}

}  // namespace fuzzing
}  // namespace mozilla
