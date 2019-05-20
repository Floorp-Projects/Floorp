/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Channel_h
#define mozilla_recordreplay_Channel_h

#include "base/process.h"

#include "mozilla/gfx/Types.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

#include "File.h"
#include "JSControl.h"
#include "MiddlemanCall.h"
#include "Monitor.h"

namespace mozilla {
namespace recordreplay {

// This file has definitions for creating and communicating on a special
// bidirectional channel between a middleman process and a recording or
// replaying process. This communication is not included in the recording, and
// when replaying this is the only mechanism the child can use to communicate
// with the middleman process.
//
// Replaying processes can rewind themselves, restoring execution state and the
// contents of all heap memory to that at an earlier point. To keep the
// replaying process and middleman from getting out of sync with each other,
// there are tight constraints on when messages may be sent across the channel
// by one process or the other. At any given time the child process may be
// either paused or unpaused. If it is paused, it is not doing any execution
// and cannot rewind itself. If it is unpaused, it may execute content and may
// rewind itself.
//
// Messages can be sent from the child process to the middleman only when the
// child process is unpaused, and messages can only be sent from the middleman
// to the child process when the child process is paused. This prevents
// messages from being lost when they are sent from the middleman as the
// replaying process rewinds itself. A few exceptions to this rule are noted
// below.

#define ForEachMessageType(_Macro)                             \
  /* Messages sent from the middleman to the child process. */ \
                                                               \
  /* Sent at startup. */                                       \
  _Macro(Introduction)                                         \
                                                               \
  /* Sent to recording processes to indicate that the middleman will be running */ \
  /* developer tools server-side code instead of the recording process itself. */ \
  _Macro(SetDebuggerRunsInMiddleman)                           \
                                                               \
  /* Sent to recording processes when exiting, or to force a hanged replaying */ \
  /* process to crash. */                                      \
  _Macro(Terminate)                                            \
                                                               \
  /* Poke a child that is recording to create an artificial checkpoint, rather than */ \
  /* (potentially) idling indefinitely. This has no effect on a replaying process. */ \
  _Macro(CreateCheckpoint)                                     \
                                                               \
  /* Unpause the child and perform a debugger-defined operation. */ \
  _Macro(ManifestStart)                                             \
                                                               \
  /* Respond to a MiddlemanCallRequest message. */             \
  _Macro(MiddlemanCallResponse)                                \
                                                               \
  /* Messages sent from the child process to the middleman. */ \
                                                               \
  /* Pause after executing a manifest, specifying its response. */ \
  _Macro(ManifestFinished)                                     \
                                                               \
  /* A critical error occurred and execution cannot continue. The child will */ \
  /* stop executing after sending this message and will wait to be terminated. */ \
  /* A minidump for the child has been generated. */           \
  _Macro(FatalError)                                           \
                                                               \
  /* Sent when a fatal error has occurred, but before the minidump has been */ \
  /* generated. */                                             \
  _Macro(BeginFatalError)                                      \
                                                               \
  /* The child's graphics were repainted. */                   \
  _Macro(Paint)                                                \
                                                               \
  /* Call a system function from the middleman process which the child has */ \
  /* encountered after diverging from the recording. */        \
  _Macro(MiddlemanCallRequest)                                 \
                                                               \
  /* Reset all information generated by previous MiddlemanCallRequest messages. */ \
  _Macro(ResetMiddlemanCalls)

enum class MessageType {
#define DefineEnum(Kind) Kind,
  ForEachMessageType(DefineEnum)
#undef DefineEnum
};

struct Message {
  MessageType mType;

  // When simulating message delays, the time this message should be received,
  // relative to when the channel was opened.
  uint32_t mReceiveTime;

  // Total message size, including the header.
  uint32_t mSize;

 protected:
  Message(MessageType aType, uint32_t aSize)
    : mType(aType), mReceiveTime(0), mSize(aSize) {
    MOZ_RELEASE_ASSERT(mSize >= sizeof(*this));
  }

 public:
  struct FreePolicy {
    void operator()(Message* msg) { /*free(msg);*/
    }
  };
  typedef UniquePtr<Message, FreePolicy> UniquePtr;

  UniquePtr Clone() const {
    Message* res = static_cast<Message*>(malloc(mSize));
    memcpy(res, this, mSize);
    return UniquePtr(res);
  }

  const char* TypeString() const {
    switch (mType) {
#define EnumToString(Kind) \
  case MessageType::Kind:  \
    return #Kind;
      ForEachMessageType(EnumToString)
#undef EnumToString
          default : return "Unknown";
    }
  }

  // Return whether this is a middleman->child message that can be sent while
  // the child is unpaused.
  bool CanBeSentWhileUnpaused() const {
    return mType == MessageType::CreateCheckpoint ||
           mType == MessageType::SetDebuggerRunsInMiddleman ||
           mType == MessageType::MiddlemanCallResponse ||
           mType == MessageType::Terminate ||
           mType == MessageType::Introduction;
  }

 protected:
  template <typename T, typename Elem>
  Elem* Data() {
    return (Elem*)(sizeof(T) + (char*)this);
  }

  template <typename T, typename Elem>
  const Elem* Data() const {
    return (const Elem*)(sizeof(T) + (const char*)this);
  }

  template <typename T, typename Elem>
  size_t DataSize() const {
    return (mSize - sizeof(T)) / sizeof(Elem);
  }

  template <typename T, typename Elem, typename... Args>
  static T* NewWithData(size_t aBufferSize, Args&&... aArgs) {
    size_t size = sizeof(T) + aBufferSize * sizeof(Elem);
    void* ptr = malloc(size);
    return new (ptr) T(size, std::forward<Args>(aArgs)...);
  }
};

struct IntroductionMessage : public Message {
  base::ProcessId mParentPid;
  uint32_t mArgc;

  IntroductionMessage(uint32_t aSize, base::ProcessId aParentPid,
                      uint32_t aArgc)
      : Message(MessageType::Introduction, aSize),
        mParentPid(aParentPid),
        mArgc(aArgc) {}

  char* ArgvString() { return Data<IntroductionMessage, char>(); }
  const char* ArgvString() const { return Data<IntroductionMessage, char>(); }

  static IntroductionMessage* New(base::ProcessId aParentPid, int aArgc,
                                  char* aArgv[]) {
    size_t argsLen = 0;
    for (int i = 0; i < aArgc; i++) {
      argsLen += strlen(aArgv[i]) + 1;
    }

    IntroductionMessage* res =
        NewWithData<IntroductionMessage, char>(argsLen, aParentPid, aArgc);

    size_t offset = 0;
    for (int i = 0; i < aArgc; i++) {
      memcpy(&res->ArgvString()[offset], aArgv[i], strlen(aArgv[i]) + 1);
      offset += strlen(aArgv[i]) + 1;
    }
    MOZ_RELEASE_ASSERT(offset == argsLen);

    return res;
  }

  static IntroductionMessage* RecordReplay(const IntroductionMessage& aMsg) {
    size_t introductionSize = RecordReplayValue(aMsg.mSize);
    IntroductionMessage* msg = (IntroductionMessage*)malloc(introductionSize);
    if (IsRecording()) {
      memcpy(msg, &aMsg, introductionSize);
    }
    RecordReplayBytes(msg, introductionSize);
    return msg;
  }
};

template <MessageType Type>
struct EmptyMessage : public Message {
  EmptyMessage() : Message(Type, sizeof(*this)) {}
};

typedef EmptyMessage<MessageType::SetDebuggerRunsInMiddleman>
    SetDebuggerRunsInMiddlemanMessage;
typedef EmptyMessage<MessageType::Terminate> TerminateMessage;
typedef EmptyMessage<MessageType::CreateCheckpoint> CreateCheckpointMessage;

template <MessageType Type>
struct JSONMessage : public Message {
  explicit JSONMessage(uint32_t aSize) : Message(Type, aSize) {}

  const char16_t* Buffer() const { return Data<JSONMessage<Type>, char16_t>(); }
  size_t BufferSize() const { return DataSize<JSONMessage<Type>, char16_t>(); }

  static JSONMessage<Type>* New(const char16_t* aBuffer, size_t aBufferSize) {
    JSONMessage<Type>* res =
        NewWithData<JSONMessage<Type>, char16_t>(aBufferSize);
    MOZ_RELEASE_ASSERT(res->BufferSize() == aBufferSize);
    PodCopy(res->Data<JSONMessage<Type>, char16_t>(), aBuffer, aBufferSize);
    return res;
  }
};

typedef JSONMessage<MessageType::ManifestStart> ManifestStartMessage;
typedef JSONMessage<MessageType::ManifestFinished> ManifestFinishedMessage;

struct FatalErrorMessage : public Message {
  explicit FatalErrorMessage(uint32_t aSize)
      : Message(MessageType::FatalError, aSize) {}

  const char* Error() const { return Data<FatalErrorMessage, const char>(); }
};

typedef EmptyMessage<MessageType::BeginFatalError> BeginFatalErrorMessage;

// The format for graphics data which will be sent to the middleman process.
// This needs to match the format expected for canvas image data, to avoid
// transforming the data before rendering it in the middleman process.
static const gfx::SurfaceFormat gSurfaceFormat = gfx::SurfaceFormat::R8G8B8X8;

struct PaintMessage : public Message {
  // Checkpoint whose state is being painted.
  uint32_t mCheckpointId;

  uint32_t mWidth;
  uint32_t mHeight;

  PaintMessage(uint32_t aCheckpointId, uint32_t aWidth, uint32_t aHeight)
      : Message(MessageType::Paint, sizeof(*this)),
        mCheckpointId(aCheckpointId),
        mWidth(aWidth),
        mHeight(aHeight) {}
};

template <MessageType Type>
struct BinaryMessage : public Message {
  explicit BinaryMessage(uint32_t aSize) : Message(Type, aSize) {}

  const char* BinaryData() const { return Data<BinaryMessage<Type>, char>(); }
  size_t BinaryDataSize() const {
    return DataSize<BinaryMessage<Type>, char>();
  }

  static BinaryMessage<Type>* New(const char* aData, size_t aDataSize) {
    BinaryMessage<Type>* res =
        NewWithData<BinaryMessage<Type>, char>(aDataSize);
    MOZ_RELEASE_ASSERT(res->BinaryDataSize() == aDataSize);
    PodCopy(res->Data<BinaryMessage<Type>, char>(), aData, aDataSize);
    return res;
  }
};

typedef BinaryMessage<MessageType::MiddlemanCallRequest>
    MiddlemanCallRequestMessage;
typedef BinaryMessage<MessageType::MiddlemanCallResponse>
    MiddlemanCallResponseMessage;
typedef EmptyMessage<MessageType::ResetMiddlemanCalls>
    ResetMiddlemanCallsMessage;

class Channel {
 public:
  // Note: the handler is responsible for freeing its input message. It will be
  // called on the channel's message thread.
  typedef std::function<void(Message::UniquePtr)> MessageHandler;

 private:
  // ID for this channel, unique for the middleman.
  size_t mId;

  // Callback to invoke off thread on incoming messages.
  MessageHandler mHandler;

  // Whether the channel is initialized and ready for outgoing messages.
  Atomic<bool, SequentiallyConsistent, Behavior::DontPreserve> mInitialized;

  // Descriptor used to accept connections on the parent side.
  int mConnectionFd;

  // Descriptor used to communicate with the other side.
  int mFd;

  // For synchronizing initialization of the channel.
  Monitor mMonitor;

  // Buffer for message data received from the other side of the channel.
  typedef InfallibleVector<char, 0, AllocPolicy<MemoryKind::Generic>>
      MessageBuffer;
  MessageBuffer* mMessageBuffer;

  // The number of bytes of data already in the message buffer.
  size_t mMessageBytes;

  // Whether this channel is subject to message delays during simulation.
  bool mSimulateDelays;

  // The time this channel was opened, for use in simulating message delays.
  TimeStamp mStartTime;

  // When simulating message delays, the time at which old messages will have
  // finished sending and new messages may be sent.
  TimeStamp mAvailableTime;

  // If spew is enabled, print a message and associated info to stderr.
  void PrintMessage(const char* aPrefix, const Message& aMsg);

  // Block until a complete message is received from the other side of the
  // channel.
  Message::UniquePtr WaitForMessage();

  // Main routine for the channel's thread.
  static void ThreadMain(void* aChannel);

 public:
  // Initialize this channel, connect to the other side, and spin up a thread
  // to process incoming messages by calling aHandler.
  Channel(size_t aId, bool aMiddlemanRecording, const MessageHandler& aHandler);

  size_t GetId() { return mId; }

  // Send a message to the other side of the channel. This must be called on
  // the main thread, except for fatal error messages.
  void SendMessage(Message&& aMsg);
};

// Command line option used to specify the middleman pid for a child process.
static const char* gMiddlemanPidOption = "-middlemanPid";

// Command line option used to specify the channel ID for a child process.
static const char* gChannelIDOption = "-recordReplayChannelID";

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_Channel_h
