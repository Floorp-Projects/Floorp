/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Channel.h"

#include "ChildIPC.h"
#include "ProcessRewind.h"
#include "Thread.h"

#include "MainThreadUtils.h"
#include "nsXULAppAPI.h"
#include "base/eintr_wrapper.h"
#include "base/process_util.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/FileDescriptor.h"

#include <sys/socket.h>
#include <sys/un.h>

namespace mozilla {
namespace recordreplay {

static void GetSocketAddress(struct sockaddr_un* addr,
                             base::ProcessId aMiddlemanPid, size_t aId) {
  addr->sun_family = AF_UNIX;
  int n = snprintf(addr->sun_path, sizeof(addr->sun_path),
                   "/tmp/WebReplay_%d_%d", aMiddlemanPid, (int)aId);
  MOZ_RELEASE_ASSERT(n >= 0 && n < (int)sizeof(addr->sun_path));
  addr->sun_len = SUN_LEN(addr);
}

namespace parent {

void OpenChannel(base::ProcessId aMiddlemanPid, uint32_t aChannelId,
                 ipc::FileDescriptor* aConnection) {
  MOZ_RELEASE_ASSERT(IsMiddleman() || XRE_IsParentProcess());

  int connectionFd = socket(AF_UNIX, SOCK_STREAM, 0);
  MOZ_RELEASE_ASSERT(connectionFd > 0);

  struct sockaddr_un addr;
  GetSocketAddress(&addr, aMiddlemanPid, aChannelId);

  int rv = bind(connectionFd, (sockaddr*)&addr, SUN_LEN(&addr));
  MOZ_RELEASE_ASSERT(rv >= 0);

  *aConnection = ipc::FileDescriptor(connectionFd);
  close(connectionFd);
}

}  // namespace parent

static void InitializeSimulatedDelayState();

struct HelloMessage {
  int32_t mMagic;
};

Channel::Channel(size_t aId, bool aMiddlemanRecording,
                 const MessageHandler& aHandler)
    : mId(aId),
      mHandler(aHandler),
      mInitialized(false),
      mConnectionFd(0),
      mFd(0),
      mMessageBuffer(nullptr),
      mMessageBytes(0),
      mSimulateDelays(false) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (IsRecordingOrReplaying()) {
    MOZ_RELEASE_ASSERT(AreThreadEventsPassedThrough());

    mFd = socket(AF_UNIX, SOCK_STREAM, 0);
    MOZ_RELEASE_ASSERT(mFd > 0);

    struct sockaddr_un addr;
    GetSocketAddress(&addr, child::MiddlemanProcessId(), mId);

    int rv = HANDLE_EINTR(connect(mFd, (sockaddr*)&addr, SUN_LEN(&addr)));
    MOZ_RELEASE_ASSERT(rv >= 0);

    DirectDeleteFile(addr.sun_path);
  } else {
    MOZ_RELEASE_ASSERT(IsMiddleman());

    ipc::FileDescriptor connection;
    if (aMiddlemanRecording) {
      // When starting the recording child process we have not done enough
      // initialization to ask for a channel from the parent, but have also not
      // started the sandbox so we can do it ourselves.
      parent::OpenChannel(base::GetCurrentProcId(), mId, &connection);
    } else {
      dom::ContentChild::GetSingleton()->SendOpenRecordReplayChannel(
          mId, &connection);
      MOZ_RELEASE_ASSERT(connection.IsValid());
    }

    mConnectionFd = connection.ClonePlatformHandle().release();
    int rv = listen(mConnectionFd, 1);
    MOZ_RELEASE_ASSERT(rv >= 0);
  }

  // Simulate message delays in channels used to communicate with a replaying
  // process.
  mSimulateDelays = IsMiddleman() ? !aMiddlemanRecording : IsReplaying();

  InitializeSimulatedDelayState();

  Thread::SpawnNonRecordedThread(ThreadMain, this);
}

/* static */
void Channel::ThreadMain(void* aChannelArg) {
  Channel* channel = (Channel*)aChannelArg;

  static const int32_t MagicValue = 0x914522b9;

  if (IsRecordingOrReplaying()) {
    HelloMessage msg;

    int rv = HANDLE_EINTR(recv(channel->mFd, &msg, sizeof(msg), MSG_WAITALL));
    MOZ_RELEASE_ASSERT(rv == sizeof(msg));
    MOZ_RELEASE_ASSERT(msg.mMagic == MagicValue);
  } else {
    MOZ_RELEASE_ASSERT(IsMiddleman());

    channel->mFd = HANDLE_EINTR(accept(channel->mConnectionFd, nullptr, 0));
    MOZ_RELEASE_ASSERT(channel->mFd > 0);

    HelloMessage msg;
    msg.mMagic = MagicValue;

    int rv = HANDLE_EINTR(send(channel->mFd, &msg, sizeof(msg), 0));
    MOZ_RELEASE_ASSERT(rv == sizeof(msg));
  }

  channel->mStartTime = channel->mAvailableTime = TimeStamp::Now();

  {
    MonitorAutoLock lock(channel->mMonitor);
    channel->mInitialized = true;
    channel->mMonitor.Notify();
  }

  while (true) {
    Message::UniquePtr msg = channel->WaitForMessage();
    if (!msg) {
      break;
    }
    channel->mHandler(std::move(msg));
  }
}

// Simulated one way latency between middleman and replaying children, in ms.
static size_t gSimulatedLatency;

// Simulated bandwidth for data transferred between middleman and replaying
// children, in bytes/ms.
static size_t gSimulatedBandwidth;

static size_t LoadEnvValue(const char* aEnv) {
  const char* value = getenv(aEnv);
  if (value && value[0]) {
    int n = atoi(value);
    return n >= 0 ? n : 0;
  }
  return 0;
}

static void InitializeSimulatedDelayState() {
  // In preparation for shifting computing resources into the cloud when
  // debugging a recorded execution (see bug 1547081), we need to be able to
  // test expected performance when there is a significant distance between the
  // user's machine (running the UI, middleman, and recording process) and
  // machines in the cloud (running replaying processes). To assess this
  // expected performance, the environment variables below can be used to
  // specify the one-way latency and bandwidth to simulate for connections
  // between the middleman and replaying processes.
  //
  // This simulation is approximate: the bandwidth tracked is per connection
  // instead of the total across all connections, and network restrictions are
  // not yet simulated when transferring graphics data.
  //
  // If there are multiple channels then we will do this initialization multiple
  // times, so this needs to be idempotent.
  gSimulatedLatency = LoadEnvValue("MOZ_RECORD_REPLAY_SIMULATED_LATENCY");
  gSimulatedBandwidth = LoadEnvValue("MOZ_RECORD_REPLAY_SIMULATED_BANDWIDTH");
}

static bool MessageSubjectToSimulatedDelay(MessageType aType) {
  switch (aType) {
    // Middleman call messages are not subject to delays. When replaying
    // children are in the cloud they will use a local process to perform
    // middleman calls.
    case MessageType::MiddlemanCallResponse:
    case MessageType::MiddlemanCallRequest:
    case MessageType::ResetMiddlemanCalls:
    // Don't call system functions when we're in the process of crashing.
    case MessageType::BeginFatalError:
    case MessageType::FatalError:
      return false;
    default:
      return true;
  }
}

void Channel::SendMessage(Message&& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread() ||
                     aMsg.mType == MessageType::BeginFatalError ||
                     aMsg.mType == MessageType::FatalError ||
                     aMsg.mType == MessageType::MiddlemanCallRequest);

  // Block until the channel is initialized.
  if (!mInitialized) {
    MonitorAutoLock lock(mMonitor);
    while (!mInitialized) {
      mMonitor.Wait();
    }
  }

  PrintMessage("SendMsg", aMsg);

  if (gSimulatedLatency &&
      gSimulatedBandwidth &&
      mSimulateDelays &&
      MessageSubjectToSimulatedDelay(aMsg.mType)) {
    AutoEnsurePassThroughThreadEvents pt;

    // Find the time this message will start sending.
    TimeStamp sendTime = TimeStamp::Now();
    if (sendTime < mAvailableTime) {
      sendTime = mAvailableTime;
    }

    // Find the time spent sending the message over the channel.
    size_t sendDurationMs = aMsg.mSize / gSimulatedBandwidth;
    mAvailableTime = sendTime + TimeDuration::FromMilliseconds(sendDurationMs);

    // The receive time of the message is the time the message finishes sending
    // plus the connection latency.
    TimeStamp receiveTime =
      mAvailableTime + TimeDuration::FromMilliseconds(gSimulatedLatency);

    aMsg.mReceiveTime = (receiveTime - mStartTime).ToMilliseconds();
  }

  const char* ptr = (const char*)&aMsg;
  size_t nbytes = aMsg.mSize;
  while (nbytes) {
    int rv = HANDLE_EINTR(send(mFd, ptr, nbytes, 0));
    if (rv < 0) {
      // If the other side of the channel has crashed, don't send the message.
      // Avoid crashing in this process too, so that we don't generate another
      // minidump that masks the original crash.
      MOZ_RELEASE_ASSERT(errno == EPIPE);
      return;
    }
    ptr += rv;
    nbytes -= rv;
  }
}

Message::UniquePtr Channel::WaitForMessage() {
  if (!mMessageBuffer) {
    mMessageBuffer = (MessageBuffer*)AllocateMemory(sizeof(MessageBuffer),
                                                    MemoryKind::Generic);
    mMessageBuffer->appendN(0, PageSize);
  }

  size_t messageSize = 0;
  while (true) {
    if (mMessageBytes >= sizeof(Message)) {
      Message* msg = (Message*)mMessageBuffer->begin();
      messageSize = msg->mSize;
      MOZ_RELEASE_ASSERT(messageSize >= sizeof(Message));
      if (mMessageBytes >= messageSize) {
        break;
      }
    }

    // Make sure the buffer is large enough for the entire incoming message.
    if (messageSize > mMessageBuffer->length()) {
      mMessageBuffer->appendN(0, messageSize - mMessageBuffer->length());
    }

    ssize_t nbytes =
        HANDLE_EINTR(recv(mFd, &mMessageBuffer->begin()[mMessageBytes],
                          mMessageBuffer->length() - mMessageBytes, 0));
    if (nbytes < 0) {
      MOZ_RELEASE_ASSERT(errno == EAGAIN);
      continue;
    } else if (nbytes == 0) {
      // The other side of the channel has shut down.
      if (IsMiddleman()) {
        return nullptr;
      }
      PrintSpew("Channel disconnected, exiting...\n");
      _exit(0);
    }

    mMessageBytes += nbytes;
  }

  Message::UniquePtr res = ((Message*)mMessageBuffer->begin())->Clone();

  // Remove the message we just received from the incoming buffer.
  size_t remaining = mMessageBytes - messageSize;
  if (remaining) {
    memmove(mMessageBuffer->begin(), &mMessageBuffer->begin()[messageSize],
            remaining);
  }
  mMessageBytes = remaining;

  // If there is a simulated delay on the message, wait until it completes.
  if (res->mReceiveTime) {
    TimeStamp receiveTime =
      mStartTime + TimeDuration::FromMilliseconds(res->mReceiveTime);
    while (receiveTime > TimeStamp::Now()) {
      MonitorAutoLock lock(mMonitor);
      mMonitor.WaitUntil(receiveTime);
    }
  }

  PrintMessage("RecvMsg", *res);
  return res;
}

void Channel::PrintMessage(const char* aPrefix, const Message& aMsg) {
  if (!SpewEnabled()) {
    return;
  }
  AutoEnsurePassThroughThreadEvents pt;
  nsCString data;
  switch (aMsg.mType) {
    case MessageType::ManifestStart: {
      const ManifestStartMessage& nmsg = (const ManifestStartMessage&)aMsg;
      data = NS_ConvertUTF16toUTF8(
          nsDependentSubstring(nmsg.Buffer(), nmsg.BufferSize()));
      break;
    }
    case MessageType::ManifestFinished: {
      const ManifestFinishedMessage& nmsg =
          (const ManifestFinishedMessage&)aMsg;
      data = NS_ConvertUTF16toUTF8(
          nsDependentSubstring(nmsg.Buffer(), nmsg.BufferSize()));
      break;
    }
    default:
      break;
  }
  const char* kind =
      IsMiddleman() ? "Middleman" : (IsRecording() ? "Recording" : "Replaying");
  PrintSpew("%s%s:%d %s %s\n", kind, aPrefix, (int)mId, aMsg.TypeString(),
            data.get());
}

}  // namespace recordreplay
}  // namespace mozilla
