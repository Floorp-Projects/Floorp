/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPCFuzzController_h
#define mozilla_ipc_IPCFuzzController_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/HashTable.h"
#include "mozilla/Mutex.h"
#include "mozilla/fuzzing/Nyx.h"

#include "nsIRunnable.h"
#include "nsThreadUtils.h"

#include "chrome/common/ipc_message.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/ports/event.h"

#include "IPCMessageStart.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#define MOZ_FUZZING_IPC_DROP_PEER(aReason)                    \
  mozilla::fuzzing::IPCFuzzController::instance().OnDropPeer( \
      aReason, __FILE__, __LINE__);

#define MOZ_FUZZING_IPC_MT_CTOR() \
  mozilla::fuzzing::IPCFuzzController::instance().OnMessageTaskStart();

#define MOZ_FUZZING_IPC_MT_STOP() \
  mozilla::fuzzing::IPCFuzzController::instance().OnMessageTaskStop();

#define MOZ_FUZZING_IPC_PRE_FUZZ_MT_RUN() \
  mozilla::fuzzing::IPCFuzzController::instance().OnPreFuzzMessageTaskRun();

#define MOZ_FUZZING_IPC_PRE_FUZZ_MT_STOP() \
  mozilla::fuzzing::IPCFuzzController::instance().OnPreFuzzMessageTaskStop();

namespace mozilla {

namespace ipc {
// We can't include ProtocolUtils.h here
class IProtocol;
typedef IPCMessageStart ProtocolId;

class NodeChannel;
}  // namespace ipc

namespace fuzzing {

class IPCFuzzController {
  typedef std::pair<int32_t, uint64_t> SeqNoPair;

  typedef std::pair<int32_t, mozilla::ipc::ProtocolId> ActorIdPair;

  class IPCFuzzLoop final : public Runnable {
    friend class IPCFuzzController;

   public:
    NS_DECL_NSIRUNNABLE

    IPCFuzzLoop();

   private:
    ~IPCFuzzLoop() = default;
  };

 public:
  static IPCFuzzController& instance();

  void InitializeIPCTypes();
  bool GetRandomIPCMessageType(mozilla::ipc::ProtocolId pId,
                               uint16_t typeOffset, uint32_t* type);

  bool ObserveIPCMessage(mozilla::ipc::NodeChannel* channel,
                         IPC::Message& aMessage);
  bool MakeTargetDecision(uint8_t portIndex, uint8_t portInstanceIndex,
                          uint8_t actorIndex, uint16_t typeOffset,
                          mojo::core::ports::PortName* name, int32_t* seqno,
                          uint64_t* fseqno, int32_t* actorId, uint32_t* type,
                          bool* is_cons, bool update = true);

  void OnActorConnected(mozilla::ipc::IProtocol* protocol);
  void OnActorDestroyed(mozilla::ipc::IProtocol* protocol);
  void OnDropPeer(const char* reason, const char* file, int line);
  void OnMessageTaskStart();
  void OnMessageTaskStop();
  void OnPreFuzzMessageTaskRun();
  void OnPreFuzzMessageTaskStop();
  void OnChildReady() { childReady = true; }
  void OnRunnableDone() { runnableDone = true; }

  uint32_t getPreFuzzMessageTaskCount() { return messageTaskCount; };
  uint32_t getMessageStartCount() { return messageStartCount; };
  uint32_t getMessageStopCount() { return messageStopCount; };

  void StartFuzzing(mozilla::ipc::NodeChannel* channel, IPC::Message& aMessage);

  void SynchronizeOnMessageExecution(uint32_t expected_messages);
  void AddToplevelActor(mojo::core::ports::PortName name,
                        mozilla::ipc::ProtocolId protocolId);

 private:
  // This is a mapping from port name to a pair of last seen sequence numbers.
  std::unordered_map<mojo::core::ports::PortName, SeqNoPair> portSeqNos;

  // This is a mapping from port name to node name.
  std::unordered_map<mojo::core::ports::PortName, mojo::core::ports::NodeName>
      portNodeName;

  // This maps each ProtocolId (IPCMessageStart) to the number of valid
  // messages for that particular type.
  std::unordered_map<mozilla::ipc::ProtocolId, uint32_t> validMsgTypes;

  // This is a mapping from port name to pairs of actor Id and ProtocolId.
  std::unordered_map<mojo::core::ports::PortName, std::vector<ActorIdPair>>
      actorIds;

  // If set, `lastActorPortName` is valid and fuzzing is pinned to this port.
  Atomic<bool> useLastPortName;

  // Last port where a new actor appeared. Only valid with `useLastPortName`.
  mojo::core::ports::PortName lastActorPortName;

  // Counter to indicate how long fuzzing should stay pinned to the last
  // actor that appeared on `lastActorPortName`.
  Atomic<uint32_t> useLastActor;

  // This is the deterministic ordering of toplevel actors for fuzzing.
  // In this matrix, each row (toplevel index) corresponds to one toplevel
  // actor *type* while each entry in that row is an instance of that type,
  // since some actors usually have multiple instances alive while others
  // don't. For the exact ordering, please check the constructor for this
  // class.
  std::vector<std::vector<mojo::core::ports::PortName>> portNames;
  std::unordered_map<std::string, uint8_t> portNameToIndex;

  // This is a set of all types that are constructors.
  std::unordered_set<uint32_t> constructorTypes;

  // This is the name of the target node. We select one Node based on a
  // particular toplevel actor and then use this to pull in additional
  // toplevel actors that are on the same node (i.e. belong to the same
  // process pair).
  mojo::core::ports::NodeName targetNodeName;
  bool haveTargetNodeName = false;

  // This indicates that we have started the fuzzing thread and fuzzing will
  // begin shortly.
  bool fuzzingStartPending = false;

  // This is used as a signal from other threads that runnables we dispatched
  // are completed. Right now we use this only when dispatching to the main
  // thread to await the completion of all pending events.
  Atomic<bool> runnableDone;

  // This is used to signal that the other process we are talking to is ready
  // to start fuzzing. In the case of Parent <-> Child, a special IPC message
  // is used to signal this. We might not be able to start fuzzing immediately
  // hough if not all toplevel actors have been created yet.
  Atomic<bool> childReady;

  // Current amount of pending message tasks.
  Atomic<uint32_t> messageStartCount;
  Atomic<uint32_t> messageStopCount;

  Atomic<uint32_t> messageTaskCount;

  Vector<char, 256, InfallibleAllocPolicy> sampleHeader;

  mozilla::ipc::NodeChannel* nodeChannel = nullptr;

  // This class is used both on the I/O and background threads as well as
  // its own fuzzing thread. Those methods that alter non-threadsafe data
  // structures need to aquire this mutex first.
  Mutex mMutex;  // MOZ_UNANNOTATED;

  IPCFuzzController();
  NYX_DISALLOW_COPY_AND_ASSIGN(IPCFuzzController);
};

}  // namespace fuzzing
}  // namespace mozilla

#endif
