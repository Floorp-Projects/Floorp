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

void OpenChannel(base::ProcessId aProcessId, uint32_t aChannelId,
                 ipc::FileDescriptor* aConnection) {
  int connectionFd = socket(AF_UNIX, SOCK_STREAM, 0);
  MOZ_RELEASE_ASSERT(connectionFd > 0);

  struct sockaddr_un addr;
  GetSocketAddress(&addr, aProcessId, aChannelId);
  DirectDeleteFile(addr.sun_path);

  int rv = bind(connectionFd, (sockaddr*)&addr, SUN_LEN(&addr));
  if (rv < 0) {
    Print("Error: bind() failed [errno %d], crashing...\n", errno);
    MOZ_CRASH("OpenChannel");
  }

  *aConnection = ipc::FileDescriptor(connectionFd);
  close(connectionFd);
}

}  // namespace parent

struct HelloMessage {
  int32_t mMagic;
};

Channel::Channel(size_t aId, Kind aKind, const MessageHandler& aHandler,
                 base::ProcessId aParentPid)
    : mId(aId),
      mKind(aKind),
      mHandler(aHandler),
      mInitialized(false),
      mConnectionFd(0),
      mFd(0),
      mMessageBytes(0) {
  MOZ_RELEASE_ASSERT(!IsRecordingOrReplaying() || AreThreadEventsPassedThrough());

  if (IsParent()) {
    ipc::FileDescriptor connection;
    if (aKind == Kind::MiddlemanReplay) {
      // The middleman is sandboxed at this point and the parent must open
      // the channel on our behalf.
      dom::ContentChild::GetSingleton()->SendOpenRecordReplayChannel(
          aId, &connection);
      MOZ_RELEASE_ASSERT(connection.IsValid());
    } else {
      parent::OpenChannel(base::GetCurrentProcId(), aId, &connection);
    }

    mConnectionFd = connection.ClonePlatformHandle().release();
    int rv = listen(mConnectionFd, 1);
    MOZ_RELEASE_ASSERT(rv >= 0);
  } else {
    MOZ_RELEASE_ASSERT(aParentPid);
    mFd = socket(AF_UNIX, SOCK_STREAM, 0);
    MOZ_RELEASE_ASSERT(mFd > 0);

    struct sockaddr_un addr;
    GetSocketAddress(&addr, aParentPid, aId);

    int rv = HANDLE_EINTR(connect(mFd, (sockaddr*)&addr, SUN_LEN(&addr)));
    MOZ_RELEASE_ASSERT(rv >= 0);

    DirectDeleteFile(addr.sun_path);
  }

  Thread::SpawnNonRecordedThread(ThreadMain, this);
}

/* static */
void Channel::ThreadMain(void* aChannelArg) {
  Channel* channel = (Channel*)aChannelArg;

  static const int32_t MagicValue = 0x914522b9;

  if (channel->IsParent()) {
    channel->mFd = HANDLE_EINTR(accept(channel->mConnectionFd, nullptr, 0));
    MOZ_RELEASE_ASSERT(channel->mFd > 0);

    HelloMessage msg;
    msg.mMagic = MagicValue;

    int rv = HANDLE_EINTR(send(channel->mFd, &msg, sizeof(msg), 0));
    MOZ_RELEASE_ASSERT(rv == sizeof(msg));
  } else {
    HelloMessage msg;

    int rv = HANDLE_EINTR(recv(channel->mFd, &msg, sizeof(msg), MSG_WAITALL));
    MOZ_RELEASE_ASSERT(rv == sizeof(msg));
    MOZ_RELEASE_ASSERT(msg.mMagic == MagicValue);
  }

  {
    MonitorAutoLock lock(channel->mMonitor);
    channel->mInitialized = true;
    channel->mMonitor.Notify();

    auto& pending = channel->mPendingData;
    if (!pending.empty()) {
      channel->SendRaw(pending.begin(), pending.length());
      pending.clear();
    }
  }

  while (true) {
    Message::UniquePtr msg = channel->WaitForMessage();
    if (!msg) {
      break;
    }
    channel->mHandler(std::move(msg));
  }
}

void Channel::SendMessage(Message&& aMsg) {
  PrintMessage("SendMsg", aMsg);
  SendMessageData((const char*)&aMsg, aMsg.mSize);
}

void Channel::SendMessageData(const char* aData, size_t aSize) {
  MonitorAutoLock lock(mMonitor);

  if (mInitialized) {
    SendRaw(aData, aSize);
  } else {
    mPendingData.append(aData, aSize);
  }
}

void Channel::SendRaw(const char* aData, size_t aSize) {
  while (aSize) {
    int rv = HANDLE_EINTR(send(mFd, aData, aSize, 0));
    if (rv < 0) {
      // If the other side of the channel has crashed, don't send the message.
      // Avoid crashing in this process too, so that we don't generate another
      // minidump that masks the original crash.
      MOZ_RELEASE_ASSERT(errno == EPIPE);
      return;
    }
    aData += rv;
    aSize -= rv;
  }
}

Message::UniquePtr Channel::WaitForMessage() {
  if (mMessageBuffer.empty()) {
    mMessageBuffer.appendN(0, PageSize);
  }

  size_t messageSize = 0;
  while (true) {
    if (mMessageBytes >= sizeof(Message)) {
      Message* msg = (Message*)mMessageBuffer.begin();
      messageSize = msg->mSize;
      MOZ_RELEASE_ASSERT(messageSize >= sizeof(Message));
      if (mMessageBytes >= messageSize) {
        break;
      }
    }

    // Make sure the buffer is large enough for the entire incoming message.
    if (messageSize > mMessageBuffer.length()) {
      mMessageBuffer.appendN(0, messageSize - mMessageBuffer.length());
    }

    ssize_t nbytes =
        HANDLE_EINTR(recv(mFd, &mMessageBuffer.begin()[mMessageBytes],
                          mMessageBuffer.length() - mMessageBytes, 0));
    if (nbytes < 0 && errno == EAGAIN) {
      continue;
    }

    if (nbytes == 0 || (nbytes < 0 && errno == ECONNRESET)) {
      // The other side of the channel has shut down.
      if (ExitProcessOnDisconnect()) {
        PrintSpew("Channel disconnected, exiting...\n");
        _exit(0);
      } else {
        // Returning null will shut down the channel.
        PrintSpew("Channel disconnected, shutting down thread.\n");
        return nullptr;
      }
    }

    MOZ_RELEASE_ASSERT(nbytes > 0);
    mMessageBytes += nbytes;
  }

  Message::UniquePtr res = ((Message*)mMessageBuffer.begin())->Clone();

  // Remove the message we just received from the incoming buffer.
  size_t remaining = mMessageBytes - messageSize;
  if (remaining) {
    memmove(mMessageBuffer.begin(), &mMessageBuffer[messageSize],
            remaining);
  }
  mMessageBytes = remaining;

  PrintMessage("RecvMsg", *res);
  return res;
}

void Channel::ExitIfNotInitializedBefore(const TimeStamp& aDeadline) {
  MOZ_RELEASE_ASSERT(IsParent());

  MonitorAutoLock lock(mMonitor);
  while (!mInitialized) {
    if (TimeStamp::Now() >= aDeadline) {
      PrintSpew("Timed out waiting for channel initialization, exiting...\n");
      _exit(0);
    }

    mMonitor.WaitUntil(aDeadline);
  }
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
    case MessageType::RecordingData: {
      const auto& nmsg = static_cast<const RecordingDataMessage&>(aMsg);
      data = nsPrintfCString("Start %llu Size %lu", nmsg.mTag,
                             nmsg.BinaryDataSize());
      break;
    }
    default:
      break;
  }
  const char* kind =
      IsMiddleman() ? "Middleman" : (IsRecording() ? "Recording" : "Replaying");
  PrintSpew("%s%s:%lu:%lu %s %s\n", kind, aPrefix, mId, aMsg.mForkId,
            aMsg.TypeString(), data.get());
}

}  // namespace recordreplay
}  // namespace mozilla
